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

#include "vktVideoTestUtils.hpp"
#include "vktVideoSessionNvUtils.hpp"
#include "extNvidiaVideoParserIf.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"

#include <vector>
#include <list>
#include <bitset>
#include <queue>

namespace vkt
{
namespace video
{

using namespace vk;
using namespace std;



class ImageObject
{
public:
									ImageObject				();
									~ImageObject			();
	void							DestroyImage			(void);

	VkResult						CreateImage				(const DeviceInterface&		vkd,
															 VkDevice					device,
															 int32_t					queueFamilyIndex,
															 Allocator&					allocator,
															 const VkImageCreateInfo*	pImageCreateInfo,
															 const MemoryRequirement	memoryRequirement);
	VkResult						StageImage				(const DeviceInterface&		vkd,
															 VkDevice					device,
															 VkImageUsageFlags			usage,
															 const MemoryRequirement	memoryRequirement,
															 uint32_t					queueFamilyIndex);
	VkFormat						getFormat				(void) const;
	VkExtent2D						getExtent				(void) const;
	VkImage							getImage				(void) const;
	VkImageView						getView					(void) const;
	bool							isArray					(void) const;
	bool							isImageExist			(void) const;

protected:
	VkFormat						m_imageFormat;
	VkExtent2D						m_imageExtent;
	de::MovePtr<ImageWithMemory>	m_image;
	Move<VkImageView>				m_imageView;
	uint32_t						m_imageArrayLayers;
};

struct DecodedFrame
{
	int32_t				pictureIndex;
	const ImageObject*	pDecodedImage;
	VkImageLayout		decodedImageLayout;
	VkFence				frameCompleteFence;
	VkFence				frameConsumerDoneFence;
	VkSemaphore			frameCompleteSemaphore;
	VkSemaphore			frameConsumerDoneSemaphore;
	VkQueryPool			queryPool;
	int32_t				startQueryId;
	uint32_t			numQueries;
	uint64_t			timestamp;
	uint32_t			hasConsummerSignalFence : 1;
	uint32_t			hasConsummerSignalSemaphore : 1;
	// For debugging
	int32_t				decodeOrder;
	int32_t				displayOrder;
};

struct DecodedFrameRelease
{
	int32_t		pictureIndex;
	int64_t		timestamp;
	uint32_t	hasConsummerSignalFence : 1;
	uint32_t	hasConsummerSignalSemaphore : 1;
	// For debugging
	int32_t		decodeOrder;
	int32_t		displayOrder;
};

struct DisplayPictureInfo
{
	int64_t timestamp; // Presentation time stamp
};


class NvidiaPerFrameDecodeImage : public NvidiaVulkanPictureBase
{
public:
														NvidiaPerFrameDecodeImage	();
														NvidiaPerFrameDecodeImage	(const NvidiaPerFrameDecodeImage&	srcObj) = delete;
														NvidiaPerFrameDecodeImage	(NvidiaPerFrameDecodeImage&&		srcObj) = delete;
														~NvidiaPerFrameDecodeImage	();

	void												deinit						(void);
	void												init						(const DeviceInterface&					vkd,
																					 VkDevice								device);

	VkResult											CreateImage					(const DeviceInterface&					vkd,
																					 VkDevice								device,
																					 int32_t								queueFamilyIndex,
																					 Allocator&								allocator,
																					 const VkImageCreateInfo*				pImageCreateInfo,
																					 const MemoryRequirement				memoryRequirement);

	const ImageObject*									GetImageObject				(void);
	bool												isImageExist				(void);
	const ImageObject*									GetDPBImageObject			(void);

	VulkanParserDecodePictureInfo						m_picDispInfo;
	ImageObject											m_frameImage;
	VkImageLayout										m_frameImageCurrentLayout;
	Move<VkFence>										m_frameCompleteFence;
	Move<VkSemaphore>									m_frameCompleteSemaphore;
	Move<VkFence>										m_frameConsumerDoneFence;
	Move<VkSemaphore>									m_frameConsumerDoneSemaphore;
	uint32_t											m_hasFrameCompleteSignalFence : 1;
	uint32_t											m_hasFrameCompleteSignalSemaphore : 1;
	uint32_t											m_hasConsummerSignalFence : 1;
	uint32_t											m_hasConsummerSignalSemaphore : 1;
	uint32_t											m_inDecodeQueue : 1;
	uint32_t											m_inDisplayQueue : 1;
	uint32_t											m_ownedByDisplay : 1;
	NvidiaSharedBaseObj<NvidiaParserVideoRefCountBase>	currentVkPictureParameters;
	de::SharedPtr<ImageObject>							m_dpbImage;
	VkImageLayout										m_dpbImageCurrentLayout;
};

class NvidiaPerFrameDecodeImageSet
{
public:
	enum
	{
		MAX_FRAMEBUFFER_IMAGES = 32
	};
								NvidiaPerFrameDecodeImageSet	();
								~NvidiaPerFrameDecodeImageSet	();

	int32_t						init							(const DeviceInterface&		vkd,
																 VkDevice					device,
																 int32_t					queueFamilyIndex,
																 Allocator&					allocator,
																 uint32_t					numImages,
																 const VkImageCreateInfo*	pOutImageCreateInfo,
																 const VkImageCreateInfo*	pDpbImageCreateInfo,
																 MemoryRequirement			memoryRequirement = MemoryRequirement::Any);

	void						deinit							(void);

	NvidiaPerFrameDecodeImage&	operator[]						(size_t						index);
	size_t						size							(void);

private:
	size_t						m_size;
	NvidiaPerFrameDecodeImage	m_frameDecodeImages[MAX_FRAMEBUFFER_IMAGES];
};

class VideoFrameBuffer
{
public:
	// Synchronization
	struct FrameSynchronizationInfo
	{
		VkFence		frameCompleteFence;
		VkSemaphore	frameCompleteSemaphore;
		VkFence		frameConsumerDoneFence;
		VkSemaphore	frameConsumerDoneSemaphore;
		VkQueryPool	queryPool;
		int32_t		startQueryId;
		uint32_t	numQueries;
		uint32_t	hasFrameCompleteSignalFence : 1;
		uint32_t	hasFrameCompleteSignalSemaphore : 1;
	};

	struct PictureResourceInfo
	{
		VkImage			image;
		VkImageLayout	currentImageLayout;
	};

									VideoFrameBuffer				();
	virtual							~VideoFrameBuffer				();

	virtual int32_t					InitImagePool					(const DeviceInterface&			vkd,
																	 VkDevice						device,
																	 int32_t						queueFamilyIndex,
																	 Allocator&						allocator,
																	 uint32_t						numImages,
																	 uint32_t						maxNumImages,
																	 const VkImageCreateInfo*		pOutImageCreateInfo,
																	 const VkImageCreateInfo*		pDpbImageCreateInfo,
																	 const VkVideoProfileInfoKHR*	pDecodeProfile = DE_NULL);
	virtual int32_t					QueuePictureForDecode			(int8_t							picId,
																	 VulkanParserDecodePictureInfo*	pDecodePictureInfo,
																	 NvidiaParserVideoRefCountBase*	pCurrentVkPictureParameters,
																	 FrameSynchronizationInfo*		pFrameSynchronizationInfo);
	virtual int32_t					DequeueDecodedPicture			(DecodedFrame*					pDecodedFrame);
	virtual int32_t					GetDisplayFramesCount			(void);
	virtual int32_t					ReleaseDisplayedPicture			(DecodedFrameRelease**			pDecodedFramesRelease,
																	 uint32_t						numFramesToRelease);
	virtual void					GetImageResourcesByIndex		(int32_t						numResources,
																	 const int8_t*					referenceSlotIndexes,
																	 VkVideoPictureResourceInfoKHR*	videoPictureResources,
																	 PictureResourceInfo*			pictureResourcesInfo,
																	 VkImageLayout					newImageLayout);
	virtual void					GetImageResourcesByIndex		(const int8_t					referenceSlotIndex,
																	 VkVideoPictureResourceInfoKHR*	videoPictureResources,
																	 PictureResourceInfo*			pictureResourcesInfo,
																	 VkImageLayout					newImageLayout);
	virtual int32_t					SetPicNumInDecodeOrder			(int32_t						picId,
																	 int32_t						picNumInDecodeOrder);
	virtual int32_t					SetPicNumInDisplayOrder			(int32_t						picId,
																	 int32_t						picNumInDisplayOrder);
	virtual size_t					GetSize							(void);
	Move<VkQueryPool>				CreateVideoQueries				(const DeviceInterface&			vkd,
																	 VkDevice						device,
																	 uint32_t						numSlots,
																	 const VkVideoProfileInfoKHR*	pDecodeProfile);
	int32_t							QueueDecodedPictureForDisplay	(int8_t							picId,
																	 DisplayPictureInfo*			pDispInfo);
	NvidiaVulkanPictureBase*		ReservePictureBuffer			(void);

protected:
	NvidiaPerFrameDecodeImageSet	m_perFrameDecodeImageSet;
	queue<uint8_t>					m_displayFrames;
	Move<VkQueryPool>				m_queryPool;
	uint32_t						m_ownedByDisplayMask;
	int32_t							m_frameNumInDecodeOrder;
	int32_t							m_frameNumInDisplayOrder;
	VkExtent2D						m_extent;
};

struct NvidiaVideoDecodeH264DpbSlotInfo
{
											NvidiaVideoDecodeH264DpbSlotInfo	();
	const VkVideoDecodeH264DpbSlotInfoKHR*	Init								(int32_t slotIndex);
	bool									IsReference							() const;
	operator								bool								() const;
	void									Invalidate							();

	VkVideoDecodeH264DpbSlotInfoKHR			dpbSlotInfo;
	StdVideoDecodeH264ReferenceInfo			stdReferenceInfo;
};

struct NvidiaVideoDecodeH265DpbSlotInfo
{
											NvidiaVideoDecodeH265DpbSlotInfo	();
	const VkVideoDecodeH265DpbSlotInfoKHR*	Init								(int32_t slotIndex);
	bool									IsReference							() const;
	operator								bool								() const;
	void									Invalidate							();

	VkVideoDecodeH265DpbSlotInfoKHR			dpbSlotInfo;
	StdVideoDecodeH265ReferenceInfo			stdReferenceInfo;
};

class DpbSlot
{
public:
	bool						isInUse				();
	bool						isAvailable			();
	bool						Invalidate			();
	NvidiaVulkanPictureBase*	getPictureResource	();
	NvidiaVulkanPictureBase*	setPictureResource	(NvidiaVulkanPictureBase* picBuf);
	void						Reserve				();
	void						MarkInUse			();

private:
	NvidiaVulkanPictureBase*	m_picBuf;		// Associated resource
	bool						m_reserved;
	bool						m_inUse;
};

class DpbSlots
{
public:
					DpbSlots					(uint32_t					dpbMaxSize);
					~DpbSlots					();

	int32_t			Init						(uint32_t					newDpbMaxSize,
												 bool						reconfigure);
	void			Deinit						(void);
	int8_t			AllocateSlot				(void);
	void			FreeSlot					(int8_t						slot);
	DpbSlot&		operator[]					(uint32_t					slot);
	void			MapPictureResource			(NvidiaVulkanPictureBase*	pPic,
												 int32_t					dpbSlot);
	uint32_t		getMaxSize					(void);
	uint32_t		getSlotInUseMask			(void);

private:
	uint32_t		m_dpbMaxSize;
	uint32_t		m_slotInUseMask;
	vector<DpbSlot>	m_dpb;
	queue<uint8_t>	m_dpbSlotsAvailable;
};

class NvidiaParserVideoPictureParameters : public NvidiaParserVideoRefCountBase
{
public:
	static const uint32_t MAX_VPS_IDS = 16;
	static const uint32_t MAX_SPS_IDS = 32;
	static const uint32_t MAX_PPS_IDS = 256;

	virtual int32_t								AddRef();
	virtual int32_t								Release();
	static NvidiaParserVideoPictureParameters*	VideoPictureParametersFromBase		(NvidiaParserVideoRefCountBase*					pBase);
	static NvidiaParserVideoPictureParameters*	Create								(const DeviceInterface&							vkd,
																					 VkDevice										device,
																					 VkVideoSessionKHR								videoSession,
																					 const StdVideoPictureParametersSet*			pVpsStdPictureParametersSet,
																					 const StdVideoPictureParametersSet*			pSpsStdPictureParametersSet,
																					 const StdVideoPictureParametersSet*			pPpsStdPictureParametersSet,
																					 NvidiaParserVideoPictureParameters*			pTemplate);
	static int32_t								PopulateH264UpdateFields			(const StdVideoPictureParametersSet*			pStdPictureParametersSet,
																					 VkVideoDecodeH264SessionParametersAddInfoKHR&	h264SessionParametersAddInfo);
	static int32_t								PopulateH265UpdateFields			(const StdVideoPictureParametersSet*			pStdPictureParametersSet,
																					 VkVideoDecodeH265SessionParametersAddInfoKHR&	h265SessionParametersAddInfo);
	VkResult									Update								(const DeviceInterface&							vkd,
																					 const StdVideoPictureParametersSet*			pVpsStdPictureParametersSet,
																					 const StdVideoPictureParametersSet*			pSpsStdPictureParametersSet,
																					 const StdVideoPictureParametersSet*			pPpsStdPictureParametersSet);
	VkVideoSessionParametersKHR					GetVideoSessionParametersKHR		() const;
	int32_t										GetId								() const;
	bool										HasVpsId							(uint32_t										vpsId) const;
	bool										HasSpsId							(uint32_t										spsId) const;
	bool										HasPpsId							(uint32_t										ppsId) const;

protected:
												NvidiaParserVideoPictureParameters	(VkDevice										device);
	virtual										~NvidiaParserVideoPictureParameters	();

private:
	static int32_t								m_currentId;
	int32_t										m_Id;
	atomic<int32_t>								m_refCount;
	VkDevice									m_device;
	Move<VkVideoSessionParametersKHR>			m_sessionParameters;
	bitset<MAX_VPS_IDS>							m_vpsIdsUsed;
	bitset<MAX_SPS_IDS>							m_spsIdsUsed;
	bitset<MAX_PPS_IDS>							m_ppsIdsUsed;
};

class VulkanVideoBitstreamBuffer
{
public:
									VulkanVideoBitstreamBuffer	();
									~VulkanVideoBitstreamBuffer	();
	VkResult						CreateVideoBitstreamBuffer	(const DeviceInterface&	vkd,
																 VkDevice				device,
																 Allocator&				allocator,
																 VkDeviceSize			bufferSize,
																 VkDeviceSize			bufferOffsetAlignment,
																 VkDeviceSize			bufferSizeAlignment,
																 void*					pNext,
																 const unsigned char*	pBitstreamData = DE_NULL,
																 VkDeviceSize			bitstreamDataSize = 0);
	VkResult						CopyVideoBitstreamToBuffer	(const DeviceInterface&	vkd,
																 VkDevice				device,
																 const unsigned char*	pBitstreamData,
																 VkDeviceSize			bitstreamDataSize);
	void							DestroyVideoBitstreamBuffer	();
	VkDeviceSize					GetBufferSize				();
	VkDeviceSize					GetBufferOffsetAlignment	();
	const VkBuffer&					get							();

protected:
	VkDeviceSize					m_bufferSize;
	VkDeviceSize					m_bufferOffsetAlignment;
	VkDeviceSize					m_bufferSizeAlignment;
	de::MovePtr<BufferWithMemory>	m_buffer;
};

class NvVkDecodeFrameData
{
public:
	VulkanVideoBitstreamBuffer	bitstreamBuffer;
	Move<VkCommandBuffer>		commandBuffer;
};

using PictureParameters = std::array<NvidiaSharedBaseObj<StdVideoPictureParametersSet>, 3>;
typedef queue<NvidiaSharedBaseObj<StdVideoPictureParametersSet>>	PictureParametersQueue;
typedef NvidiaSharedBaseObj<StdVideoPictureParametersSet>			LastVpsPictureParametersQueue;
typedef NvidiaSharedBaseObj<StdVideoPictureParametersSet>			LastSpsPictureParametersQueue;
typedef NvidiaSharedBaseObj<StdVideoPictureParametersSet>			LastPpsPictureParametersQueue;
typedef NvidiaSharedBaseObj<NvidiaParserVideoPictureParameters>		CurrentPictureParameters;

typedef vector<de::MovePtr<vector<deUint8>>> HeapType;

#define ALLOC_HEAP_OBJECT_ARRAY(H,T,N) ((T*)heap.emplace(H.end(), de::MovePtr<vector<deUint8>>(new vector<deUint8>(sizeof(T)*N)))->get()->data())
#define ALLOC_HEAP_OBJECT(H,T) ALLOC_HEAP_OBJECT_ARRAY(H,T,1)
void* copyToHeap (HeapType& heap, const void* p, size_t size);
void appendHeap (HeapType& heapTo, HeapType& heapFrom);

class VideoBaseDecoder : public NvidiaVulkanParserVideoDecodeClient
{
public:
	enum
	{
		MAX_FRM_CNT	=	32,
	};

															VideoBaseDecoder				(Context&												context);
															~VideoBaseDecoder				(void);

	void													initialize						(const VkVideoCodecOperationFlagBitsKHR					videoCodecOperation,
																							 const DeviceInterface&									vkd,
																							 const VkDevice											device,
																							 const deUint32											queueFamilyIndexTransfer,
																							 const deUint32											queueFamilyIndexDecode,
																							 Allocator&												allocator);

	VkDevice												getDevice						(void);
	const DeviceInterface&									getDeviceDriver					(void);
	deUint32												getQueueFamilyIndexTransfer		(void);
	deUint32												getQueueFamilyIndexDecode		(void);
	VkQueue													getQueueTransfer				(void);
	VkQueue													getQueueDecode					(void);
	Allocator&												getAllocator					(void);

	void													setDecodeParameters				(bool													randomOrSwapped,
																							 bool													queryResultWithStatus,
																							 uint32_t												frameCountTrigger,
																							 bool													submitAfter = true,
																							 uint32_t												gopSize = 0,
																							 uint32_t												dpbCount = 1);
	int32_t													DecodeCachedPictures			(VideoBaseDecoder*										friendDecoder = DE_NULL,
																							 bool													waitSubmitted = true);

protected:
	// NvidiaVulkanParserVideoDecodeClient callbacks
	virtual int32_t											BeginSequence					(const NvidiaVulkanParserSequenceInfo*					pnvsi)							override;	// Returns max number of reference frames (always at least 2 for MPEG-2)
	virtual bool											AllocPictureBuffer				(INvidiaVulkanPicture**									ppNvidiaVulkanPicture)			override;	// Returns a new INvidiaVulkanPicture interface
	virtual bool											DecodePicture					(NvidiaVulkanParserPictureData*							pNvidiaVulkanParserPictureData)	override;	// Called when a picture is ready to be decoded
	virtual bool											UpdatePictureParameters			(NvidiaVulkanPictureParameters*							pNvidiaVulkanPictureParameters,
																							 NvidiaSharedBaseObj<NvidiaParserVideoRefCountBase>&	pictureParametersObject,
																							 uint64_t												updateSequenceCount)			override;	// Called when a picture is ready to be decoded
	virtual bool											DisplayPicture					(INvidiaVulkanPicture*									pNvidiaVulkanPicture,
																							 int64_t												llPTS)							override;	// Called when a picture is ready to be displayed
	virtual void											UnhandledNALU					(const uint8_t* pbData,
																							 int32_t												cbData)							override;	// Called for custom NAL parsing (not required)

	// Parser methods
	bool													DecodePicture					(NvidiaVulkanParserPictureData*							pNvidiaVulkanParserPictureData,
																							 VulkanParserDecodePictureInfo*							pDecodePictureInfo);
	uint32_t												FillDpbH264State				(const NvidiaVulkanParserPictureData*					pNvidiaVulkanParserPictureData,
																							 const NvidiaVulkanParserH264DpbEntry*					dpbIn,
																							 uint32_t												maxDpbInSlotsInUse,
																							 NvidiaVideoDecodeH264DpbSlotInfo*						pDpbRefList,
																							 VkVideoReferenceSlotInfoKHR*							pReferenceSlots,
																							 int8_t*												pGopReferenceImagesIndexes,
																							 StdVideoDecodeH264PictureInfoFlags						currPicFlags,
																							 int32_t*												pCurrAllocatedSlotIndex);
	uint32_t												FillDpbH265State				(const NvidiaVulkanParserPictureData*					pNvidiaVulkanParserPictureData,
																							 const NvidiaVulkanParserH265PictureData*				pin,
																							 NvidiaVideoDecodeH265DpbSlotInfo*						pDpbSlotInfo,
																							 StdVideoDecodeH265PictureInfo*							pStdPictureInfo,
																							 VkVideoReferenceSlotInfoKHR*							pReferenceSlots,
																							 int8_t*												pGopReferenceImagesIndexes);
	int8_t													AllocateDpbSlotForCurrentH264	(NvidiaVulkanPictureBase*								pNvidiaVulkanPictureBase,
																							 StdVideoDecodeH264PictureInfoFlags						currPicFlags);
	int8_t													AllocateDpbSlotForCurrentH265	(NvidiaVulkanPictureBase*								pNvidiaVulkanPictureBase,
																							 bool													isReference);
	int8_t													GetPicIdx						(NvidiaVulkanPictureBase*								pNvidiaVulkanPictureBase);
	int8_t													GetPicIdx						(INvidiaVulkanPicture*									pNvidiaVulkanPicture);
	int8_t													GetPicDpbSlot					(NvidiaVulkanPictureBase*								pNvidiaVulkanPictureBase);
	int8_t													GetPicDpbSlot					(int8_t													picIndex);
	int8_t													SetPicDpbSlot					(NvidiaVulkanPictureBase*								pNvidiaVulkanPictureBase,
																							 int8_t													dpbSlot);
	int8_t													SetPicDpbSlot					(int8_t													picIndex,
																							 int8_t													dpbSlot);
	uint32_t												ResetPicDpbSlots				(uint32_t												picIndexSlotValidMask);
	bool													GetFieldPicFlag					(int8_t													picIndex);
	bool													SetFieldPicFlag					(int8_t													picIndex,
																							 bool													fieldPicFlag);

	void													ReinitCaches					(void);
	void													SubmitQueue						(VkCommandBufferSubmitInfoKHR*							commandBufferSubmitInfo,
																							 VkSubmitInfo2KHR*										submitInfo,
																							 VideoFrameBuffer::FrameSynchronizationInfo*			frameSynchronizationInfo,
																							 VkSemaphoreSubmitInfoKHR*								frameConsumerDoneSemaphore,
																							 VkSemaphoreSubmitInfoKHR*								frameCompleteSemaphore);

	void													SubmitQueue						(vector<VkCommandBufferSubmitInfoKHR>&					commandBufferSubmitInfos,
																							 VkSubmitInfo2KHR*										submitInfo,
																							 const VkFence											frameCompleteFence,
																							 const vector<VkFence>&									frameConsumerDoneFence,
																							 const vector<VkSemaphoreSubmitInfoKHR>&				frameCompleteSemaphores,
																							 const vector<VkSemaphoreSubmitInfoKHR>&				frameConsumerDoneSemaphores);

	// Client callbacks
	virtual int32_t											StartVideoSequence				(const VulkanParserDetectedVideoFormat*					pVideoFormat);
	virtual bool											UpdatePictureParametersHandler	(NvidiaVulkanPictureParameters*							pNvidiaVulkanPictureParameters,
																							 NvidiaSharedBaseObj<NvidiaParserVideoRefCountBase>&	pictureParametersObject,
																							 uint64_t												updateSequenceCount);
	virtual int32_t											DecodePictureWithParameters		(PerFrameDecodeParameters*								pPicParams,
																							 VulkanParserDecodePictureInfo*							pDecodePictureInfo,
																							 HeapType&												heap);

	// Client methods
	virtual void											Deinitialize					(void);
	uint32_t												GetNumDecodeSurfaces			(VkVideoCodecOperationFlagBitsKHR						codec,
																							 uint32_t												minNumDecodeSurfaces,
																							 uint32_t												width,
																							 uint32_t												height);
	bool													AddPictureParametersToQueue		(NvidiaSharedBaseObj<StdVideoPictureParametersSet>&		pictureParametersSet);
	void													FlushPictureParametersQueue		();
	bool													CheckStdObjectBeforeUpdate		(NvidiaSharedBaseObj<StdVideoPictureParametersSet>&		stdPictureParametersSet);
	NvidiaParserVideoPictureParameters*						CheckStdObjectAfterUpdate		(NvidiaSharedBaseObj<StdVideoPictureParametersSet>&		stdPictureParametersSet,
																							 NvidiaParserVideoPictureParameters*					pNewPictureParametersObject);
	NvidiaParserVideoPictureParameters*						AddPictureParameters			(NvidiaSharedBaseObj<StdVideoPictureParametersSet>&		vpsStdPictureParametersSet,
																							 NvidiaSharedBaseObj<StdVideoPictureParametersSet>&		spsStdPictureParametersSet,
																							 NvidiaSharedBaseObj<StdVideoPictureParametersSet>&		ppsStdPictureParametersSet);
	NvVkDecodeFrameData*									GetCurrentFrameData				(uint32_t												currentSlotId);

public:
	NvVkDecodeFrameData*									TriggerQueue					(uint32_t												currentSlotId);
	int32_t													ReleaseDisplayedFrame			(DecodedFrame*											pDisplayedFrame);
	VideoFrameBuffer*										GetVideoFrameBuffer				(void);
	IfcNvFunctions*											GetNvFuncs						(void);

protected:
	Context&												m_context;
	de::MovePtr<IfcNvFunctions>								m_nvFuncs;
	VkVideoCodecOperationFlagBitsKHR						m_videoCodecOperation;
	const DeviceInterface*									m_vkd;
	VkDevice												m_device;
	deUint32												m_queueFamilyIndexTransfer;
	deUint32												m_queueFamilyIndexDecode;
	VkQueue													m_queueTransfer;
	VkQueue													m_queueDecode;
	Allocator*												m_allocator;

	// Parser fields
	int32_t													m_nCurrentPictureID;
	uint32_t												m_dpbSlotsMask;
	uint32_t												m_fieldPicFlagMask;
	DpbSlots												m_dpb;
	int8_t													m_pictureToDpbSlotMap[MAX_FRM_CNT];
	uint32_t												m_maxNumDecodeSurfaces;
	uint32_t												m_maxNumDpbSurfaces;
	uint64_t												m_clockRate;
	VkDeviceSize											m_minBitstreamBufferSizeAlignment;
	VkDeviceSize											m_minBitstreamBufferOffsetAlignment;

	// Client fields
	Move<VkVideoSessionKHR>									m_videoDecodeSession;
	vector<AllocationPtr>									m_videoDecodeSessionAllocs;
	uint32_t												m_numDecodeSurfaces;
	Move<VkCommandPool>										m_videoCommandPool;
	de::MovePtr<VideoFrameBuffer>							m_videoFrameBuffer;
	NvVkDecodeFrameData*									m_decodeFramesData;
	uint32_t												m_maxDecodeFramesCount;
	uint32_t												m_maxDecodeFramesAllocated;
	// dimension of the output
	uint32_t												m_width;
	uint32_t												m_height;
	uint32_t												m_codedWidth;
	uint32_t												m_codedHeight;
	// height of the mapped surface
	VkVideoChromaSubsamplingFlagBitsKHR						m_chromaFormat;
	uint8_t													m_bitLumaDepthMinus8;
	uint8_t													m_bitChromaDepthMinus8;
	int32_t													m_decodePicCount;
	VulkanParserDetectedVideoFormat							m_videoFormat;
	int32_t													m_lastSpsIdInQueue;

	PictureParametersQueue									m_pictureParametersQueue;
	LastVpsPictureParametersQueue							m_lastVpsPictureParametersQueue;
	LastSpsPictureParametersQueue							m_lastSpsPictureParametersQueue;
	LastPpsPictureParametersQueue							m_lastPpsPictureParametersQueue;
	CurrentPictureParameters								m_currentPictureParameters;

	bool													m_randomOrSwapped;
	bool													m_queryResultWithStatus;
	int32_t													m_frameCountTrigger;
	bool													m_submitAfter;
	uint32_t												m_gopSize;
	uint32_t												m_dpbCount;

	vector<HeapType>										m_heaps;

	vector<PerFrameDecodeParameters*>						m_pPerFrameDecodeParameters;
	vector<VulkanParserDecodePictureInfo*>					m_pVulkanParserDecodePictureInfo;
	vector<NvVkDecodeFrameData*>							m_pFrameDatas;
	vector<VkBufferMemoryBarrier2KHR>						m_bitstreamBufferMemoryBarriers;
	vector<vector<VkImageMemoryBarrier2KHR>>				m_imageBarriersVec;
	vector<VideoFrameBuffer::FrameSynchronizationInfo>		m_frameSynchronizationInfos;
	vector<VkCommandBufferSubmitInfoKHR>					m_commandBufferSubmitInfos;
	vector<VkVideoBeginCodingInfoKHR>						m_decodeBeginInfos;
	vector<vector<VideoFrameBuffer::PictureResourceInfo>>	m_pictureResourcesInfos;
	vector<VkDependencyInfoKHR>								m_dependencyInfos;
	vector<VkVideoEndCodingInfoKHR>							m_decodeEndInfos;
	vector<VkSubmitInfo2KHR>								m_submitInfos;
	vector<VkFence>											m_frameCompleteFences;
	vector<VkFence>											m_frameConsumerDoneFences;
	vector<VkSemaphoreSubmitInfoKHR>						m_frameCompleteSemaphoreSubmitInfos;
	vector<VkSemaphoreSubmitInfoKHR>						m_frameConsumerDoneSemaphoreSubmitInfos;

	NvidiaVulkanParserSequenceInfo							m_nvidiaVulkanParserSequenceInfo;
	bool													m_distinctDstDpbImages;
};

} // video
} // vkt

#endif // _VKTVIDEOBASEDECODEUTILS_HPP
