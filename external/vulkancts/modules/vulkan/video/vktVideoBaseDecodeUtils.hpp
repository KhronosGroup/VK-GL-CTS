#ifndef _VKTVIDEOBASEDECODEUTILS_HPP
#define _VKTVIDEOBASEDECODEUTILS_HPP
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
 */
/*!
 * \file
 * \brief Video Decoding Base Classe Functionality
 */
/*--------------------------------------------------------------------*/
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

#include "extNvidiaVideoParserIf.hpp"
#include "vktVideoTestUtils.hpp"
#include "vktVideoFrameBuffer.hpp"

#include "deMemory.h"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"

#include <array>
#include <bitset>
#include <list>
#include <queue>
#include <vector>

namespace vkt
{
namespace video
{

using namespace vk;
using namespace std;

#define MAKEFRAMERATE(num, den) (((num) << 14) | (den))
#define NV_FRAME_RATE_NUM(rate) ((rate) >> 14)
#define NV_FRAME_RATE_DEN(rate) ((rate)&0x3fff)

const uint64_t TIMEOUT_100ms = 100 * 1000 * 1000;

// Keeps track of data associated with active internal reference frames
class DpbSlot
{
public:
	bool isInUse()
	{
		return (m_reserved || m_inUse);
	}

	bool isAvailable()
	{
		return !isInUse();
	}

	bool Invalidate()
	{
		bool wasInUse = isInUse();
		if (m_picBuf)
		{
			m_picBuf->Release();
			m_picBuf = NULL;
		}

		m_reserved = m_inUse = false;

		return wasInUse;
	}

	vkPicBuffBase* getPictureResource()
	{
		return m_picBuf;
	}

	vkPicBuffBase* setPictureResource(vkPicBuffBase* picBuf, int32_t age = 0)
	{
		vkPicBuffBase* oldPic = m_picBuf;

		if (picBuf)
		{
			picBuf->AddRef();
		}
		m_picBuf = picBuf;

		if (oldPic)
		{
			oldPic->Release();
		}

		m_pictureId = age;
		return oldPic;
	}

	void Reserve()
	{
		m_reserved = true;
	}

	void MarkInUse(int32_t age = 0)
	{
		m_pictureId = age;
		m_inUse		= true;
	}

	int32_t getAge()
	{
		return m_pictureId;
	}

private:
	int32_t		   m_pictureId; // PictureID at map time (age)
	vkPicBuffBase* m_picBuf; // Associated resource

	deUint32	   m_reserved : 1;
	deUint32	   m_inUse : 1;
};

class DpbSlots
{
public:
	explicit DpbSlots(deUint8 dpbMaxSize)
		: m_dpbMaxSize(0)
		, m_slotInUseMask(0)
		, m_dpb(m_dpbMaxSize)
		, m_dpbSlotsAvailable()
	{
		Init(dpbMaxSize, false);
	}

	int32_t Init(deUint8 newDpbMaxSize, bool reconfigure)
	{
		DE_ASSERT(newDpbMaxSize <= VkParserPerFrameDecodeParameters::MAX_DPB_REF_AND_SETUP_SLOTS);

		if (!reconfigure)
		{
			Deinit();
		}

		if (reconfigure && (newDpbMaxSize < m_dpbMaxSize))
		{
			return m_dpbMaxSize;
		}

		deUint8 oldDpbMaxSize = reconfigure ? m_dpbMaxSize : 0;
		m_dpbMaxSize		  = newDpbMaxSize;

		m_dpb.resize(m_dpbMaxSize);

		for (deUint32 ndx = oldDpbMaxSize; ndx < m_dpbMaxSize; ndx++)
		{
			m_dpb[ndx].Invalidate();
		}

		for (deUint8 dpbIndx = oldDpbMaxSize; dpbIndx < m_dpbMaxSize; dpbIndx++)
		{
			m_dpbSlotsAvailable.push(dpbIndx);
		}

		return m_dpbMaxSize;
	}

	void Deinit()
	{
		for (deUint32 ndx = 0; ndx < m_dpbMaxSize; ndx++)
		{
			m_dpb[ndx].Invalidate();
		}

		while (!m_dpbSlotsAvailable.empty())
		{
			m_dpbSlotsAvailable.pop();
		}

		m_dpbMaxSize	= 0;
		m_slotInUseMask = 0;
	}

	~DpbSlots()
	{
		Deinit();
	}

	int8_t AllocateSlot()
	{
		if (m_dpbSlotsAvailable.empty())
		{
			DE_ASSERT(!"No more h.264/5 DPB slots are available");
			return -1;
		}
		int8_t slot = (int8_t)m_dpbSlotsAvailable.front();
		DE_ASSERT((slot >= 0) && ((deUint8)slot < m_dpbMaxSize));
		m_slotInUseMask |= (1 << slot);
		m_dpbSlotsAvailable.pop();
		m_dpb[slot].Reserve();
		return slot;
	}

	void FreeSlot(int8_t slot)
	{
		DE_ASSERT((deUint8)slot < m_dpbMaxSize);
		DE_ASSERT(m_dpb[slot].isInUse());
		DE_ASSERT(m_slotInUseMask & (1 << slot));

		m_dpb[slot].Invalidate();
		m_dpbSlotsAvailable.push(slot);
		m_slotInUseMask &= ~(1 << slot);
	}

	DpbSlot& operator[](deUint32 slot)
	{
		DE_ASSERT(slot < m_dpbMaxSize);
		return m_dpb[slot];
	}

	// Return the remapped index given an external decode render target index
	int8_t GetSlotOfPictureResource(vkPicBuffBase* pPic)
	{
		for (int8_t i = 0; i < (int8_t)m_dpbMaxSize; i++)
		{
			if ((m_slotInUseMask & (1 << i)) && m_dpb[i].isInUse() && (pPic == m_dpb[i].getPictureResource()))
			{
				return i;
			}
		}
		return -1; // not found
	}

	void MapPictureResource(vkPicBuffBase* pPic, deUint8 dpbSlot, int32_t age = 0)
	{
		for (deUint8 slot = 0; slot < m_dpbMaxSize; slot++)
		{
			if (slot == dpbSlot)
			{
				m_dpb[slot].setPictureResource(pPic, age);
			}
			else if (pPic)
			{
				if (m_dpb[slot].getPictureResource() == pPic)
				{
					FreeSlot(slot);
				}
			}
		}
	}

	deUint32 getSlotInUseMask()
	{
		return m_slotInUseMask;
	}

	deUint32 getMaxSize()
	{
		return m_dpbMaxSize;
	}

private:
	deUint8				 m_dpbMaxSize;
	deUint32			 m_slotInUseMask;
	std::vector<DpbSlot> m_dpb;
	std::queue<deUint8>	 m_dpbSlotsAvailable;
};

class VulkanVideoSession : public VkVideoRefCountBase
{
	enum
	{
		MAX_BOUND_MEMORY = 9
	};

public:
	static VkResult Create(DeviceContext&						devCtx,
						   deUint32								videoQueueFamily,
						   VkVideoCoreProfile*					pVideoProfile,
						   VkFormat								pictureFormat,
						   const VkExtent2D&					maxCodedExtent,
						   VkFormat								referencePicturesFormat,
						   deUint32								maxDpbSlots,
						   deUint32								maxActiveReferencePictures,
						   VkSharedBaseObj<VulkanVideoSession>& videoSession);

	bool			IsCompatible(VkDevice			 device,
								 deUint32			 videoQueueFamily,
								 VkVideoCoreProfile* pVideoProfile,
								 VkFormat			 pictureFormat,
								 const VkExtent2D&	 maxCodedExtent,
								 VkFormat			 referencePicturesFormat,
								 deUint32			 maxDpbSlots,
								 deUint32			 maxActiveReferencePictures)
	{
		if (*pVideoProfile != m_profile)
		{
			return false;
		}

		if (maxCodedExtent.width > m_createInfo.maxCodedExtent.width)
		{
			return false;
		}

		if (maxCodedExtent.height > m_createInfo.maxCodedExtent.height)
		{
			return false;
		}

		if (maxDpbSlots > m_createInfo.maxDpbSlots)
		{
			return false;
		}

		if (maxActiveReferencePictures > m_createInfo.maxActiveReferencePictures)
		{
			return false;
		}

		if (m_createInfo.referencePictureFormat != referencePicturesFormat)
		{
			return false;
		}

		if (m_createInfo.pictureFormat != pictureFormat)
		{
			return false;
		}

		if (m_devCtx.device != device)
		{
			return false;
		}

		if (m_createInfo.queueFamilyIndex != videoQueueFamily)
		{
			return false;
		}

		return true;
	}

	int32_t AddRef() override
	{
		return ++m_refCount;
	}

	int32_t Release() override
	{
		deUint32 ret = --m_refCount;
		// Destroy the device if refcount reaches zero
		if (ret == 0)
		{
			delete this;
		}
		return ret;
	}

	VkVideoSessionKHR GetVideoSession() const
	{
		return m_videoSession;
	}

private:
	VulkanVideoSession(DeviceContext&	   devCtx,
					   VkVideoCoreProfile* pVideoProfile)
		: m_refCount(0), m_profile(*pVideoProfile), m_devCtx(devCtx), m_videoSession(VkVideoSessionKHR(0))
	{
		deMemset(&m_createInfo, 0, sizeof(VkVideoSessionCreateInfoKHR));
		m_createInfo.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;

		for (auto & binding : m_memoryBound)
		{
			binding = VK_NULL_HANDLE;
		}
	}

	~VulkanVideoSession() override
	{
		auto& vk = m_devCtx.getDeviceDriver();
		if (!!m_videoSession)
		{
			vk.destroyVideoSessionKHR(m_devCtx.device, m_videoSession, NULL);
			m_videoSession = VK_NULL_HANDLE;
		}

		for (deUint32 memIdx = 0; memIdx < MAX_BOUND_MEMORY; memIdx++)
		{
			if (m_memoryBound[memIdx] != VK_NULL_HANDLE)
			{
				vk.freeMemory(m_devCtx.device, m_memoryBound[memIdx], 0);
				m_memoryBound[memIdx] = VK_NULL_HANDLE;
			}
		}
	}

private:
	std::atomic<int32_t>		m_refCount;
	VkVideoCoreProfile			m_profile;
	DeviceContext&				m_devCtx;
	VkVideoSessionCreateInfoKHR m_createInfo;
	VkVideoSessionKHR			m_videoSession;
	VkDeviceMemory				m_memoryBound[MAX_BOUND_MEMORY];
};

class VkParserVideoPictureParameters : public VkVideoRefCountBase
{
public:
	static const deUint32				   MAX_VPS_IDS = 16;
	static const deUint32				   MAX_SPS_IDS = 32;
	static const deUint32				   MAX_PPS_IDS = 256;

	//! Increment the reference count by 1.
	virtual int32_t						   AddRef();

	//! Decrement the reference count by 1. When the reference count
	//! goes to 0 the object is automatically destroyed.
	virtual int32_t						   Release();

	static VkParserVideoPictureParameters* VideoPictureParametersFromBase(VkVideoRefCountBase* pBase)
	{
		if (!pBase)
		{
			return NULL;
		}
		VkParserVideoPictureParameters* pPictureParameters = static_cast<VkParserVideoPictureParameters*>(pBase);
		if (m_refClassId == pPictureParameters->m_classId)
		{
			return pPictureParameters;
		}
		DE_ASSERT(false && "Invalid VkParserVideoPictureParameters from base");
		return nullptr;
	}

	static VkResult AddPictureParameters(DeviceContext&									  deviceContext,
										 VkSharedBaseObj<VulkanVideoSession>&			  videoSession,
										 VkSharedBaseObj<StdVideoPictureParametersSet>&	  stdPictureParametersSet,
										 VkSharedBaseObj<VkParserVideoPictureParameters>& currentVideoPictureParameters);

	static bool		CheckStdObjectBeforeUpdate(VkSharedBaseObj<StdVideoPictureParametersSet>&	pictureParametersSet,
											   VkSharedBaseObj<VkParserVideoPictureParameters>& currentVideoPictureParameters);

	static VkResult Create(DeviceContext&									deviceContext,
						   VkSharedBaseObj<VkParserVideoPictureParameters>& templatePictureParameters,
						   VkSharedBaseObj<VkParserVideoPictureParameters>& videoPictureParameters);

	static int32_t	PopulateH264UpdateFields(const StdVideoPictureParametersSet*		   pStdPictureParametersSet,
											 VkVideoDecodeH264SessionParametersAddInfoKHR& h264SessionParametersAddInfo);

	static int32_t	PopulateH265UpdateFields(const StdVideoPictureParametersSet*		   pStdPictureParametersSet,
											 VkVideoDecodeH265SessionParametersAddInfoKHR& h265SessionParametersAddInfo);

	VkResult		CreateParametersObject(VkSharedBaseObj<VulkanVideoSession>& videoSession,
										   const StdVideoPictureParametersSet*	pStdVideoPictureParametersSet,
										   VkParserVideoPictureParameters*		pTemplatePictureParameters);

	VkResult		UpdateParametersObject(StdVideoPictureParametersSet* pStdVideoPictureParametersSet);

	VkResult		HandleNewPictureParametersSet(VkSharedBaseObj<VulkanVideoSession>& videoSession,
												  StdVideoPictureParametersSet*		   pStdVideoPictureParametersSet);

	operator VkVideoSessionParametersKHR() const
	{
		DE_ASSERT(m_sessionParameters != VK_NULL_HANDLE);
		return m_sessionParameters;
	}

	VkVideoSessionParametersKHR GetVideoSessionParametersKHR() const
	{
		DE_ASSERT(m_sessionParameters != VK_NULL_HANDLE);
		return m_sessionParameters;
	}

	int32_t GetId() const
	{
		return m_Id;
	}

	bool HasVpsId(deUint32 vpsId) const
	{
		DE_ASSERT(vpsId < MAX_VPS_IDS);
		return m_vpsIdsUsed[vpsId];
	}

	bool HasSpsId(deUint32 spsId) const
	{
		DE_ASSERT(spsId < MAX_SPS_IDS);
		return m_spsIdsUsed[spsId];
	}

	bool HasPpsId(deUint32 ppsId) const
	{
		DE_ASSERT(ppsId < MAX_PPS_IDS);
		return m_ppsIdsUsed[ppsId];
	}

	bool	 UpdatePictureParametersHierarchy(VkSharedBaseObj<StdVideoPictureParametersSet>& pictureParametersObject);

	VkResult AddPictureParametersToQueue(VkSharedBaseObj<StdVideoPictureParametersSet>& pictureParametersSet);
	int32_t	 FlushPictureParametersQueue(VkSharedBaseObj<VulkanVideoSession>& videoSession);

protected:
	VkParserVideoPictureParameters(DeviceContext&									deviceContext,
								   VkSharedBaseObj<VkParserVideoPictureParameters>& templatePictureParameters)
		: m_classId(m_refClassId), m_Id(-1), m_refCount(0), m_deviceContext(deviceContext), m_videoSession(), m_sessionParameters(VK_NULL_HANDLE), m_templatePictureParameters(templatePictureParameters)
	{
	}

	virtual ~VkParserVideoPictureParameters();

private:
	static const char*										  m_refClassId;
	static int32_t											  m_currentId;
	const char*												  m_classId;
	int32_t													  m_Id;
	std::atomic<int32_t>									  m_refCount;
	DeviceContext&											  m_deviceContext;
	VkSharedBaseObj<VulkanVideoSession>						  m_videoSession;
	VkVideoSessionParametersKHR								  m_sessionParameters;
	std::bitset<MAX_VPS_IDS>								  m_vpsIdsUsed;
	std::bitset<MAX_SPS_IDS>								  m_spsIdsUsed;
	std::bitset<MAX_PPS_IDS>								  m_ppsIdsUsed;
	int														  m_updateCount{0};
	VkSharedBaseObj<VkParserVideoPictureParameters>			  m_templatePictureParameters; // needed only for the create

	std::queue<VkSharedBaseObj<StdVideoPictureParametersSet>> m_pictureParametersQueue;
	VkSharedBaseObj<StdVideoPictureParametersSet>			  m_lastPictParamsQueue[StdVideoPictureParametersSet::NUM_OF_TYPES];
};

struct nvVideoDecodeH264DpbSlotInfo
{
	VkVideoDecodeH264DpbSlotInfoKHR dpbSlotInfo;
	StdVideoDecodeH264ReferenceInfo stdReferenceInfo;

	nvVideoDecodeH264DpbSlotInfo()
		: dpbSlotInfo()
		, stdReferenceInfo()
	{
	}

	const VkVideoDecodeH264DpbSlotInfoKHR* Init(int8_t slotIndex)
	{
		DE_ASSERT((slotIndex >= 0) && (slotIndex < (int8_t)VkParserPerFrameDecodeParameters::MAX_DPB_REF_AND_SETUP_SLOTS));
		DE_UNREF(slotIndex);
		dpbSlotInfo.sType			  = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
		dpbSlotInfo.pNext			  = NULL;
		dpbSlotInfo.pStdReferenceInfo = &stdReferenceInfo;
		return &dpbSlotInfo;
	}

	bool IsReference() const
	{
		return (dpbSlotInfo.pStdReferenceInfo == &stdReferenceInfo);
	}

	operator bool() const
	{
		return IsReference();
	}
	void Invalidate()
	{
		memset(this, 0x00, sizeof(*this));
	}
};

struct nvVideoDecodeH265DpbSlotInfo
{
	VkVideoDecodeH265DpbSlotInfoKHR dpbSlotInfo;
	StdVideoDecodeH265ReferenceInfo stdReferenceInfo;

	nvVideoDecodeH265DpbSlotInfo()
		: dpbSlotInfo()
		, stdReferenceInfo()
	{
	}

	const VkVideoDecodeH265DpbSlotInfoKHR* Init(int8_t slotIndex)
	{
		DE_ASSERT((slotIndex >= 0) && (slotIndex < (int8_t)VkParserPerFrameDecodeParameters::MAX_DPB_REF_SLOTS));
		DE_UNREF(slotIndex);
		dpbSlotInfo.sType			  = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR;
		dpbSlotInfo.pNext			  = NULL;
		dpbSlotInfo.pStdReferenceInfo = &stdReferenceInfo;
		return &dpbSlotInfo;
	}

	bool IsReference() const
	{
		return (dpbSlotInfo.pStdReferenceInfo == &stdReferenceInfo);
	}

	operator bool() const
	{
		return IsReference();
	}

	void Invalidate()
	{
		memset(this, 0x00, sizeof(*this));
	}
};

// TODO: These optimizations from the NVIDIA sample code are not worth it for CTS.
using VulkanBitstreamBufferPool = VulkanVideoRefCountedPool<BitstreamBufferImpl, 64>;

// A pool of bitstream buffers and a collection of command buffers for all frames in the decode sequence.
class NvVkDecodeFrameData
{
public:
	NvVkDecodeFrameData(const DeviceInterface& vkd, VkDevice device, deUint32 decodeQueueIdx)
		: m_deviceInterface(vkd), m_device(device), m_decodeQueueIdx(decodeQueueIdx), m_videoCommandPool(VK_NULL_HANDLE), m_bitstreamBuffersQueue()
	{
	}

	void deinit()
	{
		if (m_videoCommandPool != VK_NULL_HANDLE)
		{
			m_deviceInterface.freeCommandBuffers(m_device, m_videoCommandPool, (deUint32)m_commandBuffers.size(), &m_commandBuffers[0]);
			m_deviceInterface.destroyCommandPool(m_device, m_videoCommandPool, NULL);
			m_videoCommandPool = VK_NULL_HANDLE;
		}
	}

	~NvVkDecodeFrameData()
	{
		deinit();
	}

	size_t resize(size_t maxDecodeFramesCount)
	{
		size_t allocatedCommandBuffers = 0;
		if (m_videoCommandPool == VK_NULL_HANDLE)
		{
			VkCommandPoolCreateInfo cmdPoolInfo = {};
			cmdPoolInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			cmdPoolInfo.flags					= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			cmdPoolInfo.queueFamilyIndex		= m_decodeQueueIdx;
			VK_CHECK(m_deviceInterface.createCommandPool(m_device, &cmdPoolInfo, nullptr, &m_videoCommandPool));

			VkCommandBufferAllocateInfo cmdInfo = {};
			cmdInfo.sType						= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			cmdInfo.commandBufferCount			= (deUint32)maxDecodeFramesCount;
			cmdInfo.level						= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmdInfo.commandPool					= m_videoCommandPool;

			m_commandBuffers.resize(maxDecodeFramesCount);
			VK_CHECK(m_deviceInterface.allocateCommandBuffers(m_device, &cmdInfo, &m_commandBuffers[0]));
			allocatedCommandBuffers = maxDecodeFramesCount;
		}
		else
		{
			allocatedCommandBuffers = m_commandBuffers.size();
			DE_ASSERT(maxDecodeFramesCount <= allocatedCommandBuffers);
		}

		return allocatedCommandBuffers;
	}

	VkCommandBuffer GetCommandBuffer(deUint32 slot)
	{
		DE_ASSERT(slot < m_commandBuffers.size());
		return m_commandBuffers[slot];
	}

	size_t size()
	{
		return m_commandBuffers.size();
	}

	VulkanBitstreamBufferPool& GetBitstreamBuffersQueue()
	{
		return m_bitstreamBuffersQueue;
	}

private:
	const DeviceInterface&		 m_deviceInterface;
	VkDevice					 m_device;
	deUint32					 m_decodeQueueIdx;
	VkCommandPool				 m_videoCommandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;
	VulkanBitstreamBufferPool	 m_bitstreamBuffersQueue;
};

struct nvVideoH264PicParameters
{
	enum
	{
		MAX_REF_PICTURES_LIST_ENTRIES = 16
	};

	StdVideoDecodeH264PictureInfo				 stdPictureInfo;
	VkVideoDecodeH264PictureInfoKHR				 pictureInfo;
	VkVideoDecodeH264SessionParametersAddInfoKHR pictureParameters;
	nvVideoDecodeH264DpbSlotInfo				 currentDpbSlotInfo;
	nvVideoDecodeH264DpbSlotInfo				 dpbRefList[MAX_REF_PICTURES_LIST_ENTRIES];
};

struct nvVideoH265PicParameters
{
	enum
	{
		MAX_REF_PICTURES_LIST_ENTRIES = 16
	};

	StdVideoDecodeH265PictureInfo				 stdPictureInfo;
	VkVideoDecodeH265PictureInfoKHR				 pictureInfo;
	VkVideoDecodeH265SessionParametersAddInfoKHR pictureParameters;
	nvVideoDecodeH265DpbSlotInfo				 dpbRefList[MAX_REF_PICTURES_LIST_ENTRIES];
};

struct NvVkDecodeFrameDataSlot
{
	deUint32		slot;
	VkCommandBuffer commandBuffer;
};

class VideoBaseDecoder final : public VkParserVideoDecodeClient
{
	enum
	{
		MAX_FRM_CNT = 32
	};

public:
	struct CachedDecodeParameters
	{
		VkParserPictureData								 pd;
		VkParserDecodePictureInfo						 decodedPictureInfo;
		VkParserPerFrameDecodeParameters				 pictureParams;
		// NVIDIA API conflates picture parameters with picture parameter objects.
		VkSharedBaseObj<VkParserVideoPictureParameters>  currentPictureParameterObject;
		VkVideoReferenceSlotInfoKHR						 referenceSlots[VkParserPerFrameDecodeParameters::MAX_DPB_REF_AND_SETUP_SLOTS];
		VkVideoReferenceSlotInfoKHR						 setupReferenceSlot;

		VkVideoDecodeH264DpbSlotInfoKHR					 h264SlotInfo{};
		StdVideoDecodeH264ReferenceInfo					 h264RefInfo{};
		VkVideoDecodeH265DpbSlotInfoKHR					 h265SlotInfo{};
		StdVideoDecodeH265ReferenceInfo					 h265RefInfo{};

		nvVideoH264PicParameters						 h264PicParams;
		nvVideoH265PicParameters						 h265PicParams;
		NvVkDecodeFrameDataSlot							 frameDataSlot;
		VkVideoBeginCodingInfoKHR						 decodeBeginInfo{};
		VkBufferMemoryBarrier2KHR						 bitstreamBufferMemoryBarrier;
		std::vector<VkImageMemoryBarrier2KHR>			 imageBarriers;
		VulkanVideoFrameBuffer::PictureResourceInfo		 currentDpbPictureResourceInfo;
		VulkanVideoFrameBuffer::PictureResourceInfo		 currentOutputPictureResourceInfo;
		VkVideoPictureResourceInfoKHR					 currentOutputPictureResource;
		VkVideoPictureResourceInfoKHR*					 pOutputPictureResource{};
		VulkanVideoFrameBuffer::PictureResourceInfo*	 pOutputPictureResourceInfo{};
		VulkanVideoFrameBuffer::PictureResourceInfo		 pictureResourcesInfo[VkParserPerFrameDecodeParameters::MAX_DPB_REF_AND_SETUP_SLOTS];

		std::vector<VkVideoReferenceSlotInfoKHR>		 fullReferenceSlots;
		int32_t											 picNumInDecodeOrder;
		VulkanVideoFrameBuffer::FrameSynchronizationInfo frameSynchronizationInfo;

		// When set, command buffer recording for this cached frame will reset the codec.
		bool											 performCodecReset{false};

		~CachedDecodeParameters()
		{
			if (pd.sideDataLen > 0)
			{
				DE_ASSERT(pd.pSideData);
				delete[] pd.pSideData;
			}
		}
	};

	struct Parameters
	{
		DeviceContext*							context{};
		const VkVideoCoreProfile*				profile{};
		size_t									framesToCheck{};
		bool									queryDecodeStatus{};
		bool									outOfOrderDecoding{};
		bool									alwaysRecreateDPB{};
		size_t									pictureParameterUpdateTriggerHack{0};
		VkSharedBaseObj<VulkanVideoFrameBuffer> framebuffer;
	};
	explicit VideoBaseDecoder(Parameters&& params);
	~VideoBaseDecoder() override
	{
		Deinitialize();
	}

	int32_t					ReleaseDisplayedFrame(DecodedFrame* pDisplayedFrame);
	VulkanVideoFrameBuffer* GetVideoFrameBuffer()
	{
		return m_videoFrameBuffer.Get();
	}
	const VkVideoCapabilitiesKHR* getVideoCaps() const
	{
		return &m_videoCaps;
	}

	// VkParserVideoDecodeClient callbacks
	// Returns max number of reference frames (always at least 2 for MPEG-2)
	int32_t			BeginSequence(const VkParserSequenceInfo* pnvsi) override;
	// Returns a new INvidiaVulkanPicture interface
	bool			AllocPictureBuffer(VkPicIf** ppNvidiaVulkanPicture) override;
	// Called when a picture is ready to be decoded
	bool			DecodePicture(VkParserPictureData* pNvidiaVulkanParserPictureData) override;
	// Called when the stream parameters have changed
	bool			UpdatePictureParameters(VkSharedBaseObj<StdVideoPictureParametersSet>& pictureParametersObject, /* in */
											VkSharedBaseObj<VkVideoRefCountBase>&		   client /* out */) override;
	// Called when a picture is ready to be displayed
	bool			DisplayPicture(VkPicIf* pNvidiaVulkanPicture, int64_t llPTS) override;
	// Called for custom NAL parsing (not required)
	void			UnhandledNALU(const deUint8* pbData, size_t cbData) override;

	virtual int32_t StartVideoSequence(const VkParserDetectedVideoFormat* pVideoFormat);
	virtual int32_t DecodePictureWithParameters(de::MovePtr<CachedDecodeParameters>& params);
	VkDeviceSize	GetBitstreamBuffer(VkDeviceSize							   size,
									   VkDeviceSize							   minBitstreamBufferOffsetAlignment,
									   VkDeviceSize							   minBitstreamBufferSizeAlignment,
									   const deUint8*						   pInitializeBufferMemory,
									   VkDeviceSize							   initializeBufferMemorySize,
									   VkSharedBaseObj<VulkanBitstreamBuffer>& bitstreamBuffer) override;


	// Parser methods
	bool			DecodePicture(VkParserPictureData* pParserPictureData, vkPicBuffBase* pVkPicBuff, VkParserDecodePictureInfo*);
	deUint32		FillDpbH264State(const VkParserPictureData*			pd,
									 const VkParserH264DpbEntry*		dpbIn,
									 deUint32							maxDpbInSlotsInUse,
									 nvVideoDecodeH264DpbSlotInfo*		pDpbRefList,
									 deUint32							maxRefPictures,
									 VkVideoReferenceSlotInfoKHR*		pReferenceSlots,
									 int8_t*							pGopReferenceImagesIndexes,
									 StdVideoDecodeH264PictureInfoFlags currPicFlags,
									 int32_t*							pCurrAllocatedSlotIndex);
	deUint32		FillDpbH265State(const VkParserPictureData*		pd,
									 const VkParserHevcPictureData* pin,
									 nvVideoDecodeH265DpbSlotInfo*	pDpbSlotInfo,
									 StdVideoDecodeH265PictureInfo* pStdPictureInfo,
									 deUint32						maxRefPictures,
									 VkVideoReferenceSlotInfoKHR*	pReferenceSlots,
									 int8_t*						pGopReferenceImagesIndexes,
									 int32_t*						pCurrAllocatedSlotIndex);

	int8_t			AllocateDpbSlotForCurrentH264(vkPicBuffBase*					 pPic,
												  StdVideoDecodeH264PictureInfoFlags currPicFlags,
												  int8_t							 presetDpbSlot);
	int8_t			AllocateDpbSlotForCurrentH265(vkPicBuffBase* pPic, bool isReference, int8_t presetDpbSlot);
	int8_t			GetPicIdx(vkPicBuffBase* pNvidiaVulkanPictureBase);
	int8_t			GetPicIdx(VkPicIf* pNvidiaVulkanPicture);
	int8_t			GetPicDpbSlot(int8_t picIndex);
	int8_t			SetPicDpbSlot(int8_t picIndex, int8_t dpbSlot);
	deUint32		ResetPicDpbSlots(deUint32 picIndexSlotValidMask);
	bool			GetFieldPicFlag(int8_t picIndex);
	bool			SetFieldPicFlag(int8_t picIndex, bool fieldPicFlag);

	void			Deinitialize();
	int32_t			GetCurrentFrameData(deUint32 slotId, NvVkDecodeFrameDataSlot& frameDataSlot)
	{
		if (slotId < m_decodeFramesData.size())
		{
			frameDataSlot.commandBuffer = m_decodeFramesData.GetCommandBuffer(slotId);
			frameDataSlot.slot			= slotId;
			return slotId;
		}
		return -1;
	}

	void			ApplyPictureParameters(de::MovePtr<CachedDecodeParameters>& cachedParameters);
	void			WaitForFrameFences(de::MovePtr<CachedDecodeParameters>& cachedParameters);
	void			RecordCommandBuffer(de::MovePtr<CachedDecodeParameters>& cachedParameters);
	void			SubmitQueue(de::MovePtr<CachedDecodeParameters>& cachedParameters);
	void			QueryDecodeResults(de::MovePtr<CachedDecodeParameters>& cachedParameters);
	void			decodeFramesOutOfOrder();

	DeviceContext*					m_deviceContext{};
	VkVideoCoreProfile				m_profile{};
	deUint32						m_framesToCheck{};
	// Parser fields
	int32_t							m_nCurrentPictureID{};
	deUint32						m_dpbSlotsMask{};
	deUint32						m_fieldPicFlagMask{};
	DpbSlots						m_dpb;
	std::array<int8_t, MAX_FRM_CNT> m_pictureToDpbSlotMap;
	VkFormat						m_dpbImageFormat{VK_FORMAT_UNDEFINED};
	VkFormat						m_outImageFormat{VK_FORMAT_UNDEFINED};
	deUint32						m_maxNumDecodeSurfaces{1};
	deUint32						m_maxNumDpbSlots{1};
	vector<AllocationPtr>			m_videoDecodeSessionAllocs;
	deUint32						m_numDecodeSurfaces{};
	Move<VkCommandPool>				m_videoCommandPool{};
	VkVideoCapabilitiesKHR			m_videoCaps{};
	VkVideoDecodeCapabilitiesKHR	m_decodeCaps{};
	VkVideoCodecOperationFlagsKHR	m_supportedVideoCodecs{};
	inline bool						dpbAndOutputCoincide() const
	{
		return m_decodeCaps.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR;
	}

	VkSharedBaseObj<VulkanVideoSession>							m_videoSession{};
	VkSharedBaseObj<VulkanVideoFrameBuffer>						m_videoFrameBuffer{};
	NvVkDecodeFrameData											m_decodeFramesData;

	// This is only used by the frame buffer, to set picture number in decode order.
	// The framebuffer should manage this state ideally.
	int32_t														m_decodePicCount{};

	VkParserDetectedVideoFormat									m_videoFormat{};

	VkSharedBaseObj<VkParserVideoPictureParameters>				m_currentPictureParameters{};
	int															m_pictureParameterUpdateCount{0};
	// Due to the design of the NVIDIA decoder client library, there is not a clean way to reset parameter objects
	// in between GOPs. This becomes a problem when the session object needs to change, and then the parameter
	// objects get stored in the wrong session. This field contains a nonnegative integer, such that when it
	// becomes equal to m_pictureParameterUpdateCount, it will forcibly reset the current picture parameters.
	// This could be more general by taking a modulo formula, or a list of trigger numbers. But it is currently
	// only required for the h264_resolution_change_dpb test plan, so no need for complication.
	int															m_resetPictureParametersFrameTriggerHack{};
	void triggerPictureParameterSequenceCount()
	{
		++m_pictureParameterUpdateCount;
		if (m_resetPictureParametersFrameTriggerHack > 0 && m_pictureParameterUpdateCount == m_resetPictureParametersFrameTriggerHack)
		{
			m_currentPictureParameters = nullptr;
		}
	}

	bool														m_queryResultWithStatus{false};
	bool														m_outOfOrderDecoding{false};
	bool														m_alwaysRecreateDPB{false};
	vector<VkParserPerFrameDecodeParameters*>					m_pPerFrameDecodeParameters;
	vector<VkParserDecodePictureInfo*>							m_pVulkanParserDecodePictureInfo;
	vector<NvVkDecodeFrameData*>								m_pFrameDatas;
	vector<VkBufferMemoryBarrier2KHR>							m_bitstreamBufferMemoryBarriers;
	vector<vector<VkImageMemoryBarrier2KHR>>					m_imageBarriersVec;
	vector<VulkanVideoFrameBuffer::FrameSynchronizationInfo>	m_frameSynchronizationInfos;
	vector<VkCommandBufferSubmitInfoKHR>						m_commandBufferSubmitInfos;
	vector<VkVideoBeginCodingInfoKHR>							m_decodeBeginInfos;
	vector<vector<VulkanVideoFrameBuffer::PictureResourceInfo>> m_pictureResourcesInfos;
	vector<VkDependencyInfoKHR>									m_dependencyInfos;
	vector<VkVideoEndCodingInfoKHR>								m_decodeEndInfos;
	vector<VkSubmitInfo2KHR>									m_submitInfos;
	vector<VkFence>												m_frameCompleteFences;
	vector<VkFence>												m_frameConsumerDoneFences;
	vector<VkSemaphoreSubmitInfoKHR>							m_frameCompleteSemaphoreSubmitInfos;
	vector<VkSemaphoreSubmitInfoKHR>							m_frameConsumerDoneSemaphoreSubmitInfos;

	std::vector<de::MovePtr<CachedDecodeParameters>>			m_cachedDecodeParams;

	VkParserSequenceInfo										m_nvsi{};
	deUint32													m_maxStreamBufferSize{};
	deUint32													m_numBitstreamBuffersToPreallocate{8}; // TODO: Review
	bool														m_useImageArray{false};
	bool														m_useImageViewArray{false};
	bool														m_useSeparateOutputImages{false};
	bool														m_resetDecoder{false};
};

} // namespace video
} // namespace vkt

#endif // _VKTVIDEOBASEDECODEUTILS_HPP
