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

#include "vktVideoFrameBuffer.hpp"

#include <queue>

namespace vkt
{
namespace video
{
static VkSharedBaseObj<VkImageResourceView> emptyImageView;

class NvPerFrameDecodeResources : public vkPicBuffBase {
public:
	NvPerFrameDecodeResources()
		: m_picDispInfo()
		, m_frameCompleteFence(VK_NULL_HANDLE)
		, m_frameCompleteSemaphore(VK_NULL_HANDLE)
		, m_frameConsumerDoneFence(VK_NULL_HANDLE)
		, m_frameConsumerDoneSemaphore(VK_NULL_HANDLE)
		, m_hasFrameCompleteSignalFence(false)
		, m_hasFrameCompleteSignalSemaphore(false)
		, m_hasConsummerSignalFence(false)
		, m_hasConsummerSignalSemaphore(false)
		, m_recreateImage(false)
		, m_currentDpbImageLayerLayout(VK_IMAGE_LAYOUT_UNDEFINED)
		, m_currentOutputImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
		, m_vkDevCtx(nullptr)
		, m_frameDpbImageView(VK_NULL_HANDLE)
		, m_outImageView(VK_NULL_HANDLE)
	{
	}

	VkResult CreateImage( DeviceContext& vkDevCtx,
						 const VkImageCreateInfo* pDpbImageCreateInfo,
						 const VkImageCreateInfo* pOutImageCreateInfo,
						 deUint32 imageIndex,
						 VkSharedBaseObj<VkImageResource>&  imageArrayParent,
						 VkSharedBaseObj<VkImageResourceView>& imageViewArrayParent,
						 bool useSeparateOutputImage = false,
						 bool useLinearOutput = false);

	VkResult init(DeviceContext& vkDevCtx);

	void Deinit();

	NvPerFrameDecodeResources (const NvPerFrameDecodeResources &srcObj) = delete;
	NvPerFrameDecodeResources (NvPerFrameDecodeResources &&srcObj) = delete;

	~NvPerFrameDecodeResources() override
	{
		Deinit();
	}

	VkSharedBaseObj<VkImageResourceView>& GetFrameImageView() {
		if (ImageExist()) {
			return m_frameDpbImageView;
		} else {
			return emptyImageView;
		}
	}

	VkSharedBaseObj<VkImageResourceView>& GetDisplayImageView() {
		if (ImageExist()) {
			return m_outImageView;
		} else {
			return emptyImageView;
		}
	}

	bool ImageExist() {

		return (!!m_frameDpbImageView && (m_frameDpbImageView->GetImageView() != VK_NULL_HANDLE));
	}

	bool GetImageSetNewLayout(VkImageLayout newDpbImageLayout,
							  VkVideoPictureResourceInfoKHR* pDpbPictureResource,
							  VulkanVideoFrameBuffer::PictureResourceInfo* pDpbPictureResourceInfo,
							  VkImageLayout newOutputImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM,
							  VkVideoPictureResourceInfoKHR* pOutputPictureResource = nullptr,
							  VulkanVideoFrameBuffer::PictureResourceInfo* pOutputPictureResourceInfo = nullptr) {


		if (m_recreateImage || !ImageExist()) {
			return false;
		}

		if (pDpbPictureResourceInfo) {
			pDpbPictureResourceInfo->image = m_frameDpbImageView->GetImageResource()->GetImage();
			pDpbPictureResourceInfo->imageFormat = m_frameDpbImageView->GetImageResource()->GetImageCreateInfo().format;
			pDpbPictureResourceInfo->currentImageLayout = m_currentDpbImageLayerLayout;
		}

		if (VK_IMAGE_LAYOUT_MAX_ENUM != newDpbImageLayout) {
			m_currentDpbImageLayerLayout = newDpbImageLayout;
		}

		if (pDpbPictureResource) {
			pDpbPictureResource->imageViewBinding = m_frameDpbImageView->GetImageView();
		}

		if (pOutputPictureResourceInfo) {
			pOutputPictureResourceInfo->image = m_outImageView->GetImageResource()->GetImage();
			pOutputPictureResourceInfo->imageFormat = m_outImageView->GetImageResource()->GetImageCreateInfo().format;
			pOutputPictureResourceInfo->currentImageLayout = m_currentOutputImageLayout;
		}

		if (VK_IMAGE_LAYOUT_MAX_ENUM != newOutputImageLayout) {
			m_currentOutputImageLayout = newOutputImageLayout;
		}

		if (pOutputPictureResource) {
			pOutputPictureResource->imageViewBinding = m_outImageView->GetImageView();
		}

		return true;
	}

	VkParserDecodePictureInfo m_picDispInfo;
	VkFence m_frameCompleteFence;
	VkSemaphore m_frameCompleteSemaphore;
	VkFence m_frameConsumerDoneFence;
	VkSemaphore m_frameConsumerDoneSemaphore;
	deUint32 m_hasFrameCompleteSignalFence : 1;
	deUint32 m_hasFrameCompleteSignalSemaphore : 1;
	deUint32 m_hasConsummerSignalFence : 1;
	deUint32 m_hasConsummerSignalSemaphore : 1;
	deUint32 m_recreateImage : 1;
	// VPS
	VkSharedBaseObj<VkVideoRefCountBase>  stdVps;
	// SPS
	VkSharedBaseObj<VkVideoRefCountBase>  stdSps;
	// PPS
	VkSharedBaseObj<VkVideoRefCountBase>  stdPps;
	// The bitstream Buffer
	VkSharedBaseObj<VkVideoRefCountBase>  bitstreamData;

private:
	VkImageLayout                        m_currentDpbImageLayerLayout;
	VkImageLayout                        m_currentOutputImageLayout;
	DeviceContext*						 m_vkDevCtx;
	VkSharedBaseObj<VkImageResourceView> m_frameDpbImageView;
	VkSharedBaseObj<VkImageResourceView> m_outImageView;
};

class NvPerFrameDecodeImageSet {
public:

	static constexpr size_t maxImages = 32;

	NvPerFrameDecodeImageSet()
		: m_queueFamilyIndex((deUint32)-1)
		, m_dpbImageCreateInfo()
		, m_outImageCreateInfo()
		, m_numImages(0)
		, m_usesImageArray(false)
		, m_usesImageViewArray(false)
		, m_usesSeparateOutputImage(false)
		, m_usesLinearOutput(false)
		, m_perFrameDecodeResources(maxImages)
		, m_imageArray()
		, m_imageViewArray()
	{
	}

	int32_t init(DeviceContext& vkDevCtx,
				 const VkVideoProfileInfoKHR* pDecodeProfile,
				 deUint32              numImages,
				 VkFormat              dpbImageFormat,
				 VkFormat              outImageFormat,
				 const VkExtent2D&     maxImageExtent,
				 VkImageUsageFlags     dpbImageUsage,
				 VkImageUsageFlags     outImageUsage,
				 deUint32              queueFamilyIndex,
				 bool useImageArray = false,
				 bool useImageViewArray = false,
				 bool useSeparateOutputImages = false,
				 bool useLinearOutput = false);

	~NvPerFrameDecodeImageSet()
	{
		m_numImages = 0;
	}

	NvPerFrameDecodeResources& operator[](unsigned int index)
	{
		DE_ASSERT(index < m_perFrameDecodeResources.size());
		return m_perFrameDecodeResources[index];
	}

	size_t size() const
	{
		return m_numImages;
	}

	VkResult GetImageSetNewLayout(DeviceContext& vkDevCtx,
								  deUint32 imageIndex,
								  VkImageLayout newDpbImageLayout,
								  VkVideoPictureResourceInfoKHR* pDpbPictureResource = nullptr,
								  VulkanVideoFrameBuffer::PictureResourceInfo* pDpbPictureResourceInfo = nullptr,
								  VkImageLayout newOutputImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM,
								  VkVideoPictureResourceInfoKHR* pOutputPictureResource = nullptr,
								  VulkanVideoFrameBuffer::PictureResourceInfo* pOutputPictureResourceInfo = nullptr) {

		VkResult result = VK_SUCCESS;
		if (pDpbPictureResource) {
			if (m_imageViewArray) {
				// We have an image view that has the same number of layers as the image.
				// In that scenario, while specifying the resource, the API must specifically choose the image layer.
				pDpbPictureResource->baseArrayLayer = imageIndex;
			} else {
				// Let the image view sub-resource specify the image layer.
				pDpbPictureResource->baseArrayLayer = 0;
			}
		}

		if(pOutputPictureResource) {
			// Output pictures currently are only allocated as discrete
			// Let the image view sub-resource specify the image layer.
			pOutputPictureResource->baseArrayLayer = 0;
		}

		bool validImage = m_perFrameDecodeResources[imageIndex].GetImageSetNewLayout(
			newDpbImageLayout,
			pDpbPictureResource,
			pDpbPictureResourceInfo,
			newOutputImageLayout,
			pOutputPictureResource,
			pOutputPictureResourceInfo);

		if (!validImage) {
			result = m_perFrameDecodeResources[imageIndex].CreateImage(
				vkDevCtx,
				&m_dpbImageCreateInfo,
				&m_outImageCreateInfo,
				imageIndex,
				m_imageArray,
				m_imageViewArray,
				m_usesSeparateOutputImage,
				m_usesLinearOutput);

			if (result == VK_SUCCESS) {
				validImage = m_perFrameDecodeResources[imageIndex].GetImageSetNewLayout(
					newDpbImageLayout,
					pDpbPictureResource,
					pDpbPictureResourceInfo,
					newOutputImageLayout,
					pOutputPictureResource,
					pOutputPictureResourceInfo);

				DE_ASSERT(validImage);
			}
		}

		return result;
	}

private:
	deUint32							   m_queueFamilyIndex;
	VkVideoCoreProfile					   m_videoProfile;
	VkImageCreateInfo					   m_dpbImageCreateInfo;
	VkImageCreateInfo					   m_outImageCreateInfo;
	deUint32							   m_numImages;
	// TODO: This code copied from the NVIDIA sample app has never been tested. Need to make sure the IHVs have a Radeon 5000 series GPU that uses this feature.
	deUint32							   m_usesImageArray : 1;
	deUint32							   m_usesImageViewArray : 1;
	deUint32							   m_usesSeparateOutputImage : 1;
	deUint32							   m_usesLinearOutput : 1;
	std::vector<NvPerFrameDecodeResources> m_perFrameDecodeResources;
	VkSharedBaseObj<VkImageResource>	   m_imageArray; // must be valid if m_usesImageArray is true
	VkSharedBaseObj<VkImageResourceView>   m_imageViewArray; // must be valid if m_usesImageViewArray is true
};

class VkVideoFrameBuffer : public VulkanVideoFrameBuffer {
public:
	static constexpr size_t maxFramebufferImages = 32;

	VkVideoFrameBuffer(DeviceContext& vkDevCtx, bool supportsQueries)
		: m_vkDevCtx(vkDevCtx),
		m_refCount(0),
		m_displayQueueMutex(),
		m_perFrameDecodeImageSet(),
		m_displayFrames(),
		m_supportsQueries(supportsQueries),
		m_queryPool(VK_NULL_HANDLE),
		m_ownedByDisplayMask(0),
		m_frameNumInDecodeOrder(0),
		m_frameNumInDisplayOrder(0),
		m_codedExtent{0, 0},
		m_numberParameterUpdates(0)
	{ }

	int32_t AddRef() override;
	int32_t Release() override;

	VkResult CreateVideoQueries(deUint32 numSlots, DeviceContext& vkDevCtx,
									   const VkVideoProfileInfoKHR* pDecodeProfile) {
		DE_ASSERT(numSlots <= maxFramebufferImages);

		auto& vk = vkDevCtx.context->getDeviceInterface();

		if (m_queryPool == VK_NULL_HANDLE) {
			// It would be difficult to resize a query pool, so allocate the maximum possible slot.
			numSlots = maxFramebufferImages;
			VkQueryPoolCreateInfo queryPoolCreateInfo{};
			queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			queryPoolCreateInfo.pNext = pDecodeProfile;
			queryPoolCreateInfo.queryType = VK_QUERY_TYPE_RESULT_STATUS_ONLY_KHR;
			queryPoolCreateInfo.queryCount = numSlots;  // m_numDecodeSurfaces frames worth

			return vk.createQueryPool(vkDevCtx.device, &queryPoolCreateInfo, nullptr, &m_queryPool);
		}

		return VK_SUCCESS;
	}

	void DestroyVideoQueries() {
		if (m_queryPool != VK_NULL_HANDLE) {
			m_vkDevCtx.getDeviceDriver().destroyQueryPool(m_vkDevCtx.device, m_queryPool, nullptr);
			m_queryPool = VK_NULL_HANDLE;
		}
	}

	deUint32 FlushDisplayQueue() {
		std::lock_guard<std::mutex> lock(m_displayQueueMutex);

		deUint32 flushedImages = 0;
		while (!m_displayFrames.empty()) {
			deUint8 pictureIndex = m_displayFrames.front();
			DE_ASSERT((pictureIndex >= 0) && (pictureIndex < m_perFrameDecodeImageSet.size()));
			m_displayFrames.pop();
			if (m_perFrameDecodeImageSet[(deUint32)pictureIndex].IsAvailable()) {
				// The frame is not released yet - force release it.
				m_perFrameDecodeImageSet[(deUint32)pictureIndex].Release();
			}
			flushedImages++;
		}

		return flushedImages;
	}

	int32_t InitImagePool(const VkVideoProfileInfoKHR* pDecodeProfile, deUint32 numImages, VkFormat dpbImageFormat,
								  VkFormat outImageFormat, const VkExtent2D& codedExtent, const VkExtent2D& maxImageExtent,
								  VkImageUsageFlags dpbImageUsage, VkImageUsageFlags outImageUsage, deUint32 queueFamilyIndex,
								  bool useImageArray, bool useImageViewArray,
								  bool useSeparateOutputImage, bool useLinearOutput) override;

	void Deinitialize() {
		FlushDisplayQueue();

		if (m_supportsQueries)
			DestroyVideoQueries();

		m_ownedByDisplayMask = 0;
		m_frameNumInDecodeOrder = 0;
		m_frameNumInDisplayOrder = 0;

		if (m_queryPool != VK_NULL_HANDLE) {
			m_vkDevCtx.getDeviceDriver().destroyQueryPool(m_vkDevCtx.device, m_queryPool, nullptr);
			m_queryPool = VK_NULL_HANDLE;
		}
	};

	int32_t QueueDecodedPictureForDisplay(int8_t picId, VulkanVideoDisplayPictureInfo* pDispInfo) override {
		DE_ASSERT((deUint32)picId < m_perFrameDecodeImageSet.size());

		std::lock_guard<std::mutex> lock(m_displayQueueMutex);
		m_perFrameDecodeImageSet[picId].m_displayOrder = m_frameNumInDisplayOrder++;
		m_perFrameDecodeImageSet[picId].m_timestamp = pDispInfo->timestamp;
		m_perFrameDecodeImageSet[picId].AddRef();

		m_displayFrames.push((deUint8)picId);

		if (videoLoggingEnabled()) {
			std::cout << "==> Queue Display Picture picIdx: " << (deUint32)picId
					  << "\t\tdisplayOrder: " << m_perFrameDecodeImageSet[picId].m_displayOrder
					  << "\tdecodeOrder: " << m_perFrameDecodeImageSet[picId].m_decodeOrder << "\ttimestamp "
					  << m_perFrameDecodeImageSet[picId].m_timestamp << std::endl;
		}
		return picId;
	}

	int32_t QueuePictureForDecode(int8_t picId, VkParserDecodePictureInfo* pDecodePictureInfo,
										  ReferencedObjectsInfo* pReferencedObjectsInfo,
										  FrameSynchronizationInfo* pFrameSynchronizationInfo) override;

	size_t GetDisplayedFrameCount() const override { return m_displayFrames.size(); }

	// dequeue
	int32_t DequeueDecodedPicture(DecodedFrame* pDecodedFrame) override;

	int32_t ReleaseDisplayedPicture(DecodedFrameRelease** pDecodedFramesRelease, deUint32 numFramesToRelease) override;

	int32_t GetDpbImageResourcesByIndex(deUint32 numResources, const int8_t* referenceSlotIndexes,
												VkVideoPictureResourceInfoKHR* dpbPictureResources,
												PictureResourceInfo* dpbPictureResourcesInfo,
												VkImageLayout newDpbImageLayerLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR) override;

	int32_t GetCurrentImageResourceByIndex(int8_t referenceSlotIndex, VkVideoPictureResourceInfoKHR* dpbPictureResource,
												   PictureResourceInfo* dpbPictureResourceInfo,
												   VkImageLayout newDpbImageLayerLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
												   VkVideoPictureResourceInfoKHR* outputPictureResource = nullptr,
												   PictureResourceInfo* outputPictureResourceInfo = nullptr,
												   VkImageLayout newOutputImageLayerLayout = VK_IMAGE_LAYOUT_MAX_ENUM) override;

	int32_t ReleaseImageResources(deUint32 numResources, const deUint32* indexes) override {
		std::lock_guard<std::mutex> lock(m_displayQueueMutex);
		for (unsigned int resId = 0; resId < numResources; resId++) {
			if ((deUint32)indexes[resId] < m_perFrameDecodeImageSet.size()) {
				m_perFrameDecodeImageSet[indexes[resId]].Deinit();
			}
		}
		return (int32_t)m_perFrameDecodeImageSet.size();
	}

	int32_t SetPicNumInDecodeOrder(int32_t picId, int32_t picNumInDecodeOrder) override {
		std::lock_guard<std::mutex> lock(m_displayQueueMutex);
		if ((deUint32)picId < m_perFrameDecodeImageSet.size()) {
			int32_t oldPicNumInDecodeOrder = m_perFrameDecodeImageSet[picId].m_decodeOrder;
			m_perFrameDecodeImageSet[picId].m_decodeOrder = picNumInDecodeOrder;
			return oldPicNumInDecodeOrder;
		}
		DE_ASSERT(false);
		return -1;
	}

	int32_t SetPicNumInDisplayOrder(int32_t picId, int32_t picNumInDisplayOrder) override {
		std::lock_guard<std::mutex> lock(m_displayQueueMutex);
		if ((deUint32)picId < m_perFrameDecodeImageSet.size()) {
			int32_t oldPicNumInDisplayOrder = m_perFrameDecodeImageSet[picId].m_displayOrder;
			m_perFrameDecodeImageSet[picId].m_displayOrder = picNumInDisplayOrder;
			return oldPicNumInDisplayOrder;
		}
		DE_ASSERT(false);
		return -1;
	}

	virtual const VkSharedBaseObj<VkImageResourceView>& GetImageResourceByIndex(int8_t picId) {
		std::lock_guard<std::mutex> lock(m_displayQueueMutex);
		if ((deUint32)picId < m_perFrameDecodeImageSet.size()) {
			return m_perFrameDecodeImageSet[picId].GetFrameImageView();
		}
		DE_ASSERT(false);
		return emptyImageView;
	}

	vkPicBuffBase* ReservePictureBuffer() override {
		std::lock_guard<std::mutex> lock(m_displayQueueMutex);
		int foundPicId = -1;
		for (int picId = 0; picId < m_perFrameDecodeImageSet.size(); picId++) {
			if (m_perFrameDecodeImageSet[picId].IsAvailable()) {
				foundPicId = picId;
				break;
			}
		}

		if (foundPicId >= 0) {
			m_perFrameDecodeImageSet[foundPicId].Reset();
			m_perFrameDecodeImageSet[foundPicId].AddRef();
			m_perFrameDecodeImageSet[foundPicId].m_picIdx = foundPicId;
			return &m_perFrameDecodeImageSet[foundPicId];
		}

		DE_ASSERT(foundPicId >= 0);
		return nullptr;
	}

	size_t GetSize() override {
		std::lock_guard<std::mutex> lock(m_displayQueueMutex);
		return m_perFrameDecodeImageSet.size();
	}

	virtual ~VkVideoFrameBuffer() { Deinitialize(); }

private:
	DeviceContext& m_vkDevCtx;
	std::atomic<int32_t> m_refCount;
	std::mutex m_displayQueueMutex;
	NvPerFrameDecodeImageSet m_perFrameDecodeImageSet;
	std::queue<deUint8> m_displayFrames;
	bool m_supportsQueries;
	VkQueryPool m_queryPool;
	deUint32 m_ownedByDisplayMask;
	int32_t m_frameNumInDecodeOrder;
	int32_t m_frameNumInDisplayOrder;
	VkExtent2D m_codedExtent;  // for the codedExtent, not the max image resolution
	deUint32 m_numberParameterUpdates;
};

VkResult VulkanVideoFrameBuffer::Create(DeviceContext* vkDevCtx,
										bool supportsQueries,
										VkSharedBaseObj<VulkanVideoFrameBuffer>& vkVideoFrameBuffer)
{
	VkSharedBaseObj<VkVideoFrameBuffer> videoFrameBuffer(new VkVideoFrameBuffer(*vkDevCtx, supportsQueries));
	if (videoFrameBuffer) {
		vkVideoFrameBuffer = videoFrameBuffer;
		return VK_SUCCESS;
	}
	return VK_ERROR_OUT_OF_HOST_MEMORY;
}

int32_t VkVideoFrameBuffer::AddRef()
{
	return ++m_refCount;
}

int32_t VkVideoFrameBuffer::Release()
{
	deUint32 ret;
	ret = --m_refCount;
	// Destroy the device if refcount reaches zero
	if (ret == 0) {
		delete this;
	}
	return ret;
}

int32_t VkVideoFrameBuffer::QueuePictureForDecode(int8_t picId, VkParserDecodePictureInfo* pDecodePictureInfo, VulkanVideoFrameBuffer::ReferencedObjectsInfo* pReferencedObjectsInfo, VulkanVideoFrameBuffer::FrameSynchronizationInfo* pFrameSynchronizationInfo)
{
	DE_ASSERT((deUint32)picId < m_perFrameDecodeImageSet.size());

	std::lock_guard<std::mutex> lock(m_displayQueueMutex);
	m_perFrameDecodeImageSet[picId].m_picDispInfo = *pDecodePictureInfo;
	m_perFrameDecodeImageSet[picId].m_decodeOrder = m_frameNumInDecodeOrder++;
	m_perFrameDecodeImageSet[picId].stdPps = const_cast<VkVideoRefCountBase*>(pReferencedObjectsInfo->pStdPps);
	m_perFrameDecodeImageSet[picId].stdSps = const_cast<VkVideoRefCountBase*>(pReferencedObjectsInfo->pStdSps);
	m_perFrameDecodeImageSet[picId].stdVps = const_cast<VkVideoRefCountBase*>(pReferencedObjectsInfo->pStdVps);
	m_perFrameDecodeImageSet[picId].bitstreamData = const_cast<VkVideoRefCountBase*>(pReferencedObjectsInfo->pBitstreamData);

	if (videoLoggingEnabled()) {
		std::cout << std::dec << "==> Queue Decode Picture picIdx: " << (deUint32)picId
				  << "\t\tdisplayOrder: " << m_perFrameDecodeImageSet[picId].m_displayOrder
				  << "\tdecodeOrder: " << m_perFrameDecodeImageSet[picId].m_decodeOrder << "\tFrameType "
				  << m_perFrameDecodeImageSet[picId].m_picDispInfo.videoFrameType << std::endl;
	}

	if (pFrameSynchronizationInfo->hasFrameCompleteSignalFence) {
		pFrameSynchronizationInfo->frameCompleteFence = m_perFrameDecodeImageSet[picId].m_frameCompleteFence;
		if (!!pFrameSynchronizationInfo->frameCompleteFence) {
			m_perFrameDecodeImageSet[picId].m_hasFrameCompleteSignalFence = true;
		}
	}

	if (m_perFrameDecodeImageSet[picId].m_hasConsummerSignalFence) {
		pFrameSynchronizationInfo->frameConsumerDoneFence = m_perFrameDecodeImageSet[picId].m_frameConsumerDoneFence;
		m_perFrameDecodeImageSet[picId].m_hasConsummerSignalFence = false;
	}

	if (pFrameSynchronizationInfo->hasFrameCompleteSignalSemaphore) {
		pFrameSynchronizationInfo->frameCompleteSemaphore = m_perFrameDecodeImageSet[picId].m_frameCompleteSemaphore;
		if (!!pFrameSynchronizationInfo->frameCompleteSemaphore) {
			m_perFrameDecodeImageSet[picId].m_hasFrameCompleteSignalSemaphore = true;
		}
	}

	if (m_perFrameDecodeImageSet[picId].m_hasConsummerSignalSemaphore) {
		pFrameSynchronizationInfo->frameConsumerDoneSemaphore = m_perFrameDecodeImageSet[picId].m_frameConsumerDoneSemaphore;
		m_perFrameDecodeImageSet[picId].m_hasConsummerSignalSemaphore = false;
	}

	pFrameSynchronizationInfo->queryPool = m_queryPool;
	pFrameSynchronizationInfo->startQueryId = picId;
	pFrameSynchronizationInfo->numQueries = 1;

	return picId;
}

int32_t VkVideoFrameBuffer::DequeueDecodedPicture(DecodedFrame* pDecodedFrame)
{
	int numberofPendingFrames = 0;
	int pictureIndex = -1;
	std::lock_guard<std::mutex> lock(m_displayQueueMutex);
	if (!m_displayFrames.empty()) {
		numberofPendingFrames = (int)m_displayFrames.size();
		pictureIndex = m_displayFrames.front();
		DE_ASSERT((pictureIndex >= 0) && ((deUint32)pictureIndex < m_perFrameDecodeImageSet.size()));
		DE_ASSERT(!(m_ownedByDisplayMask & (1 << pictureIndex)));
		m_ownedByDisplayMask |= (1 << pictureIndex);
		m_displayFrames.pop();
	}

	if ((deUint32)pictureIndex < m_perFrameDecodeImageSet.size()) {
		pDecodedFrame->pictureIndex = pictureIndex;

		pDecodedFrame->decodedImageView = m_perFrameDecodeImageSet[pictureIndex].GetFrameImageView();
		pDecodedFrame->outputImageView = m_perFrameDecodeImageSet[pictureIndex].GetDisplayImageView();

		pDecodedFrame->displayWidth = m_perFrameDecodeImageSet[pictureIndex].m_picDispInfo.displayWidth;
		pDecodedFrame->displayHeight = m_perFrameDecodeImageSet[pictureIndex].m_picDispInfo.displayHeight;

		if (m_perFrameDecodeImageSet[pictureIndex].m_hasFrameCompleteSignalFence) {
			pDecodedFrame->frameCompleteFence = m_perFrameDecodeImageSet[pictureIndex].m_frameCompleteFence;
			m_perFrameDecodeImageSet[pictureIndex].m_hasFrameCompleteSignalFence = false;
		} else {
			pDecodedFrame->frameCompleteFence = VK_NULL_HANDLE;
		}

		if (m_perFrameDecodeImageSet[pictureIndex].m_hasFrameCompleteSignalSemaphore) {
			pDecodedFrame->frameCompleteSemaphore = m_perFrameDecodeImageSet[pictureIndex].m_frameCompleteSemaphore;
			m_perFrameDecodeImageSet[pictureIndex].m_hasFrameCompleteSignalSemaphore = false;
		} else {
			pDecodedFrame->frameCompleteSemaphore = VK_NULL_HANDLE;
		}

		pDecodedFrame->frameConsumerDoneFence = m_perFrameDecodeImageSet[pictureIndex].m_frameConsumerDoneFence;
		pDecodedFrame->frameConsumerDoneSemaphore = m_perFrameDecodeImageSet[pictureIndex].m_frameConsumerDoneSemaphore;

		pDecodedFrame->timestamp = m_perFrameDecodeImageSet[pictureIndex].m_timestamp;
		pDecodedFrame->decodeOrder = m_perFrameDecodeImageSet[pictureIndex].m_decodeOrder;
		pDecodedFrame->displayOrder = m_perFrameDecodeImageSet[pictureIndex].m_displayOrder;

		pDecodedFrame->queryPool = m_queryPool;
		pDecodedFrame->startQueryId = pictureIndex;
		pDecodedFrame->numQueries = 1;
	}

	if (videoLoggingEnabled()) {
		std::cout << "<<<<<<<<<<< Dequeue from Display: " << pictureIndex << " out of " << numberofPendingFrames
				  << " ===========" << std::endl;
	}
	return numberofPendingFrames;
}

int32_t VkVideoFrameBuffer::ReleaseDisplayedPicture(DecodedFrameRelease** pDecodedFramesRelease, deUint32 numFramesToRelease)
{
	std::lock_guard<std::mutex> lock(m_displayQueueMutex);
	for (deUint32 i = 0; i < numFramesToRelease; i++) {
		const DecodedFrameRelease* pDecodedFrameRelease = pDecodedFramesRelease[i];
		int picId = pDecodedFrameRelease->pictureIndex;
		DE_ASSERT((picId >= 0) && ((deUint32)picId < m_perFrameDecodeImageSet.size()));

		DE_ASSERT(m_perFrameDecodeImageSet[picId].m_decodeOrder == pDecodedFrameRelease->decodeOrder);
		DE_ASSERT(m_perFrameDecodeImageSet[picId].m_displayOrder == pDecodedFrameRelease->displayOrder);

		DE_ASSERT(m_ownedByDisplayMask & (1 << picId));
		m_ownedByDisplayMask &= ~(1 << picId);
		m_perFrameDecodeImageSet[picId].bitstreamData = nullptr;
		m_perFrameDecodeImageSet[picId].stdPps = nullptr;
		m_perFrameDecodeImageSet[picId].stdSps = nullptr;
		m_perFrameDecodeImageSet[picId].stdVps = nullptr;
		m_perFrameDecodeImageSet[picId].Release();

		m_perFrameDecodeImageSet[picId].m_hasConsummerSignalFence = pDecodedFrameRelease->hasConsummerSignalFence;
		m_perFrameDecodeImageSet[picId].m_hasConsummerSignalSemaphore = pDecodedFrameRelease->hasConsummerSignalSemaphore;
	}
	return 0;
}

int32_t VkVideoFrameBuffer::GetDpbImageResourcesByIndex(deUint32 numResources, const int8_t* referenceSlotIndexes, VkVideoPictureResourceInfoKHR* dpbPictureResources, VulkanVideoFrameBuffer::PictureResourceInfo* dpbPictureResourcesInfo, VkImageLayout newDpbImageLayerLayout)
{
	DE_ASSERT(dpbPictureResources);
	std::lock_guard<std::mutex> lock(m_displayQueueMutex);
	for (unsigned int resId = 0; resId < numResources; resId++) {
		if ((deUint32)referenceSlotIndexes[resId] < m_perFrameDecodeImageSet.size()) {
			VkResult result =
				m_perFrameDecodeImageSet.GetImageSetNewLayout(m_vkDevCtx, referenceSlotIndexes[resId], newDpbImageLayerLayout,
															  &dpbPictureResources[resId], &dpbPictureResourcesInfo[resId]);

			DE_ASSERT(result == VK_SUCCESS);
			if (result != VK_SUCCESS) {
				return -1;
			}

			DE_ASSERT(dpbPictureResources[resId].sType == VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR);
			dpbPictureResources[resId].codedOffset = {
				0, 0};  // FIXME: This parameter must to be adjusted based on the interlaced mode.
			dpbPictureResources[resId].codedExtent = m_codedExtent;
		}
	}
	return numResources;
}

int32_t VkVideoFrameBuffer::GetCurrentImageResourceByIndex(int8_t referenceSlotIndex, VkVideoPictureResourceInfoKHR* dpbPictureResource, VulkanVideoFrameBuffer::PictureResourceInfo* dpbPictureResourceInfo, VkImageLayout newDpbImageLayerLayout, VkVideoPictureResourceInfoKHR* outputPictureResource, VulkanVideoFrameBuffer::PictureResourceInfo* outputPictureResourceInfo, VkImageLayout newOutputImageLayerLayout)
{
	DE_ASSERT(dpbPictureResource);
	std::lock_guard<std::mutex> lock(m_displayQueueMutex);
	if ((deUint32)referenceSlotIndex < m_perFrameDecodeImageSet.size()) {
		VkResult result = m_perFrameDecodeImageSet.GetImageSetNewLayout(
			m_vkDevCtx, referenceSlotIndex, newDpbImageLayerLayout, dpbPictureResource, dpbPictureResourceInfo,
			newOutputImageLayerLayout, outputPictureResource, outputPictureResourceInfo);
		DE_ASSERT(result == VK_SUCCESS);
		if (result != VK_SUCCESS) {
			return -1;
		}

		DE_ASSERT(dpbPictureResource->sType == VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR);
		dpbPictureResource->codedOffset = {0, 0};  // FIXME: This parameter must to be adjusted based on the interlaced mode.
		dpbPictureResource->codedExtent = m_codedExtent;

		if (outputPictureResource) {
			DE_ASSERT(outputPictureResource->sType == VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR);
			outputPictureResource->codedOffset = {
				0, 0};  // FIXME: This parameter must to be adjusted based on the interlaced mode.
			outputPictureResource->codedExtent = m_codedExtent;
		}
	}
	return referenceSlotIndex;
}

int32_t VkVideoFrameBuffer::InitImagePool(const VkVideoProfileInfoKHR* pDecodeProfile, deUint32 numImages, VkFormat dpbImageFormat, VkFormat outImageFormat, const VkExtent2D& codedExtent, const VkExtent2D& maxImageExtent, VkImageUsageFlags dpbImageUsage, VkImageUsageFlags outImageUsage, deUint32 queueFamilyIndex, bool useImageArray, bool useImageViewArray, bool useSeparateOutputImage, bool useLinearOutput)
{
	std::lock_guard<std::mutex> lock(m_displayQueueMutex);

	DE_ASSERT(numImages && (numImages <= maxFramebufferImages) && pDecodeProfile);

	if (m_supportsQueries)
		VK_CHECK(CreateVideoQueries(numImages, m_vkDevCtx, pDecodeProfile));

	// m_extent is for the codedExtent, not the max image resolution
	m_codedExtent = codedExtent;

	int32_t imageSetCreateResult = m_perFrameDecodeImageSet.init(
		m_vkDevCtx, pDecodeProfile, numImages, dpbImageFormat, outImageFormat, maxImageExtent, dpbImageUsage, outImageUsage,
		queueFamilyIndex,
		useImageArray, useImageViewArray, useSeparateOutputImage, useLinearOutput);
	m_numberParameterUpdates++;

	return imageSetCreateResult;
}

VkResult NvPerFrameDecodeResources::CreateImage( DeviceContext& vkDevCtx,
												const VkImageCreateInfo* pDpbImageCreateInfo,
												const VkImageCreateInfo* pOutImageCreateInfo,
												deUint32 imageIndex,
												VkSharedBaseObj<VkImageResource>& imageArrayParent,
												VkSharedBaseObj<VkImageResourceView>& imageViewArrayParent,
												bool useSeparateOutputImage,
												bool useLinearOutput)
{
	VkResult result = VK_SUCCESS;

	if (!ImageExist() || m_recreateImage) {

		DE_ASSERT(m_vkDevCtx != nullptr);

		m_currentDpbImageLayerLayout = pDpbImageCreateInfo->initialLayout;
		m_currentOutputImageLayout   = pOutImageCreateInfo->initialLayout;

		VkSharedBaseObj<VkImageResource> imageResource;
		if (!imageArrayParent) {
			result = VkImageResource::Create(vkDevCtx,
											 pDpbImageCreateInfo,
											 imageResource);
			if (result != VK_SUCCESS) {
				return result;
			}
		} else {
			// We are using a parent array image
			imageResource = imageArrayParent;
		}

		if (!imageViewArrayParent) {

			deUint32 baseArrayLayer = imageArrayParent ? imageIndex : 0;
			VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, baseArrayLayer, 1 };
			result = VkImageResourceView::Create(vkDevCtx, imageResource,
												   subresourceRange,
												   m_frameDpbImageView);

			if (result != VK_SUCCESS) {
				return result;
			}

			if (!(useSeparateOutputImage || useLinearOutput)) {
				m_outImageView = m_frameDpbImageView;
			}

		} else {

			m_frameDpbImageView = imageViewArrayParent;

			if (!(useSeparateOutputImage || useLinearOutput)) {
				VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, imageIndex, 1 };
				result = VkImageResourceView::Create(vkDevCtx, imageResource,
													   subresourceRange,
													   m_outImageView);
				if (result != VK_SUCCESS) {
					return result;
				}
			}
		}

		if (useSeparateOutputImage || useLinearOutput) {

			VkSharedBaseObj<VkImageResource> displayImageResource;
			result = VkImageResource::Create(vkDevCtx,
											 pOutImageCreateInfo,
											 displayImageResource);
			if (result != VK_SUCCESS) {
				return result;
			}

			VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			result = VkImageResourceView::Create(vkDevCtx, displayImageResource,
												   subresourceRange,
												   m_outImageView);
			if (result != VK_SUCCESS) {
				return result;
			}
		}
	}

	m_currentDpbImageLayerLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_currentOutputImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_recreateImage = false;

	return result;
}

VkResult NvPerFrameDecodeResources::init(DeviceContext& vkDevCtx)
{
	m_vkDevCtx = &vkDevCtx;
	auto& vk = vkDevCtx.getDeviceDriver();
	auto device = vkDevCtx.device;

	// The fence waited on for the first frame should be signaled.
	const VkFenceCreateInfo fenceFrameCompleteInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
													  VK_FENCE_CREATE_SIGNALED_BIT };
	VkResult result = vk.createFence(device, &fenceFrameCompleteInfo, nullptr, &m_frameCompleteFence);

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	result = vk.createFence(device, &fenceInfo, nullptr, &m_frameConsumerDoneFence);
	DE_ASSERT(result == VK_SUCCESS);

	VkSemaphoreCreateInfo semInfo{};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	result = vk.createSemaphore(device, &semInfo, nullptr, &m_frameCompleteSemaphore);
	DE_ASSERT(result == VK_SUCCESS);
	result = vk.createSemaphore(device, &semInfo, nullptr, &m_frameConsumerDoneSemaphore);
	DE_ASSERT(result == VK_SUCCESS);

	Reset();

	return result;
}

void NvPerFrameDecodeResources::Deinit()
{
	bitstreamData = nullptr;
	stdPps = nullptr;
	stdSps = nullptr;
	stdVps = nullptr;

	if (m_vkDevCtx == nullptr) {
		assert ((m_frameCompleteFence == VK_NULL_HANDLE) &&
			   (m_frameConsumerDoneFence == VK_NULL_HANDLE) &&
			   (m_frameCompleteSemaphore == VK_NULL_HANDLE) &&
			   (m_frameConsumerDoneSemaphore == VK_NULL_HANDLE) &&
			   !m_frameDpbImageView &&
			   !m_outImageView);
		return;
	}

	DE_ASSERT(m_vkDevCtx);
	auto& vk = m_vkDevCtx->getDeviceDriver();
	auto device = m_vkDevCtx->device;

	if (m_frameCompleteFence != VK_NULL_HANDLE) {
		vk.destroyFence(device, m_frameCompleteFence, nullptr);
		m_frameCompleteFence = VK_NULL_HANDLE;
	}

	if (m_frameConsumerDoneFence != VK_NULL_HANDLE) {
		vk.destroyFence(device, m_frameConsumerDoneFence, nullptr);
		m_frameConsumerDoneFence = VK_NULL_HANDLE;
	}

	if (m_frameCompleteSemaphore != VK_NULL_HANDLE) {
		vk.destroySemaphore(device, m_frameCompleteSemaphore, nullptr);
		m_frameCompleteSemaphore = VK_NULL_HANDLE;
	}

	if (m_frameConsumerDoneSemaphore != VK_NULL_HANDLE) {
		vk.destroySemaphore(device, m_frameConsumerDoneSemaphore, nullptr);
		m_frameConsumerDoneSemaphore = VK_NULL_HANDLE;
	}

	m_frameDpbImageView = nullptr;
	m_outImageView = nullptr;

	m_vkDevCtx = nullptr;

	Reset();
}

int32_t NvPerFrameDecodeImageSet::init(DeviceContext& vkDevCtx,
									   const VkVideoProfileInfoKHR* pDecodeProfile,
									   deUint32                 numImages,
									   VkFormat                 dpbImageFormat,
									   VkFormat                 outImageFormat,
									   const VkExtent2D&        maxImageExtent,
									   VkImageUsageFlags        dpbImageUsage,
									   VkImageUsageFlags        outImageUsage,
									   deUint32                 queueFamilyIndex,
									   bool                     useImageArray,
									   bool                     useImageViewArray,
									   bool                     useSeparateOutputImage,
									   bool                     useLinearOutput)
{
	if (numImages > m_perFrameDecodeResources.size()) {
		DE_ASSERT(!"Number of requested images exceeds the max size of the image array");
		return -1;
	}

	const bool reconfigureImages = (m_numImages &&
									(m_dpbImageCreateInfo.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO)) &&
								   ((m_dpbImageCreateInfo.format != dpbImageFormat) ||
									(m_dpbImageCreateInfo.extent.width < maxImageExtent.width) ||
									(m_dpbImageCreateInfo.extent.height < maxImageExtent.height));

	for (deUint32 imageIndex = m_numImages; imageIndex < numImages; imageIndex++) {
		VkResult result = m_perFrameDecodeResources[imageIndex].init(vkDevCtx);
		DE_ASSERT(result == VK_SUCCESS);
		if (result != VK_SUCCESS) {
			return -1;
		}
	}

	if (useImageViewArray) {
		useImageArray = true;
	}

	m_videoProfile.InitFromProfile(pDecodeProfile);

	m_queueFamilyIndex = queueFamilyIndex;

	// Image create info for the DPBs
	m_dpbImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	// m_imageCreateInfo.pNext = m_videoProfile.GetProfile();
	m_dpbImageCreateInfo.pNext = m_videoProfile.GetProfileListInfo();
	m_dpbImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	m_dpbImageCreateInfo.format = dpbImageFormat;
	m_dpbImageCreateInfo.extent = { maxImageExtent.width, maxImageExtent.height, 1 };
	m_dpbImageCreateInfo.mipLevels = 1;
	m_dpbImageCreateInfo.arrayLayers = useImageArray ? numImages : 1;
	m_dpbImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	m_dpbImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	m_dpbImageCreateInfo.usage = dpbImageUsage;
	m_dpbImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	m_dpbImageCreateInfo.queueFamilyIndexCount = 1;
	m_dpbImageCreateInfo.pQueueFamilyIndices = &m_queueFamilyIndex;
	m_dpbImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_dpbImageCreateInfo.flags = 0;

	// Image create info for the output
	if (useSeparateOutputImage || useLinearOutput) {
		m_outImageCreateInfo = m_dpbImageCreateInfo;
		m_outImageCreateInfo.format = outImageFormat;
		m_outImageCreateInfo.arrayLayers = 1;
		m_outImageCreateInfo.tiling = useLinearOutput ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
		m_outImageCreateInfo.usage = outImageUsage;

		if ((outImageUsage & VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR) == 0) {
			// A simple output image not directly used by the decoder
			m_outImageCreateInfo.pNext = nullptr;
		}
	}

	if (useImageArray) {
		// Create an image that has the same number of layers as the DPB images required.
		VkResult result = VkImageResource::Create(vkDevCtx,
												  &m_dpbImageCreateInfo,
												  m_imageArray);
		if (result != VK_SUCCESS) {
			return -1;
		}
	} else {
		m_imageArray = nullptr;
	}

	if (useImageViewArray) {
		DE_ASSERT(m_imageArray);
		// Create an image view that has the same number of layers as the image.
		// In that scenario, while specifying the resource, the API must specifically choose the image layer.
		VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, numImages };
		VkResult result = VkImageResourceView::Create(vkDevCtx, m_imageArray,
													   subresourceRange,
													   m_imageViewArray);

		if (result != VK_SUCCESS) {
			return -1;
		}
	}

	deUint32 firstIndex = reconfigureImages ? 0 : m_numImages;
	deUint32 maxNumImages = std::max(m_numImages, numImages);
	for (deUint32 imageIndex = firstIndex; imageIndex < maxNumImages; imageIndex++) {

		if (m_perFrameDecodeResources[imageIndex].ImageExist() && reconfigureImages) {

			m_perFrameDecodeResources[imageIndex].m_recreateImage = true;

		} else if (!m_perFrameDecodeResources[imageIndex].ImageExist()) {

			VkResult result =
				m_perFrameDecodeResources[imageIndex].CreateImage(vkDevCtx,
																  &m_dpbImageCreateInfo,
																  &m_outImageCreateInfo,
																  imageIndex,
																  m_imageArray,
																  m_imageViewArray,
																  useSeparateOutputImage,
																  useLinearOutput);

			DE_ASSERT(result == VK_SUCCESS);
			if (result != VK_SUCCESS) {
				return -1;
			}
		}
	}

	m_numImages               = numImages;
	m_usesImageArray          = useImageArray;
	m_usesImageViewArray      = useImageViewArray;
	m_usesSeparateOutputImage = useSeparateOutputImage;
	m_usesLinearOutput        = useLinearOutput;

	return (int32_t)numImages;
}

} // namespace video
} // namespace vkt
