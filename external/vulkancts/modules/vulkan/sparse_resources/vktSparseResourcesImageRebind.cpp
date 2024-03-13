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
 * \file  vktSparseResourcesRebind.cpp
 * \brief Sparse image rebind tests
 *
 * Summary of the test:
 *
 * Creates a sparse buffer and two backing device memory objects.
 * 1) Binds the first memory fully to the image and fill it with data.
 * 2) Binds the second memory fully (this unbinds the first memory) to the image and fill it with different data.
 * 3) Rebinds one block from the first memory back into one layer and at non 0, 0 offset.
 * 4) Copies data out of sparse image into host accessible buffer.
 * 5) Verifies if the data in the host accesible buffer is correct.
 *
 * For example, 2D image with VK_FORMAT_R16G16B16A16_UNORM, 2 layers, dimensions 512x256, and the block size of 256x128, the final layout will be:
 *
 *  Layer 1, 512x256
 * +-----------------------+
 * | memory 2   256        |-+
 * |           +-----------+ |
 * |       128 | memory 1  | |
 * +-----------+-----------+ |
 *   | memory 2              |
 *   +-----------------------+
 *    Layer 0
 *
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesImageRebind.hpp"
#include "deDefs.h"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"


#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTexVerifierUtil.hpp"

#include <cassert>
#include <deMath.h>
#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

constexpr deInt32 kMemoryObjectCount = 2;

class ImageSparseRebindCase : public TestCase
{
public:
	ImageSparseRebindCase	(tcu::TestContext&		testCtx,
							 const std::string&		name,
							 const ImageType		imageType,
							 const tcu::UVec3&		imageSize,
							 const VkFormat			format,
							 const bool				useDeviceGroups);

	TestInstance*	createInstance	(Context&				context) const;
	virtual void	checkSupport	(Context&				context) const;


private:
	const bool				m_useDeviceGroups;
	const ImageType			m_imageType;
	const tcu::UVec3		m_imageSize;
	const VkFormat			m_format;
};

ImageSparseRebindCase::ImageSparseRebindCase	(tcu::TestContext&		testCtx,
												 const std::string&		name,
												 const ImageType		imageType,
												 const tcu::UVec3&		imageSize,
												 const VkFormat			format,
												 const bool				useDeviceGroups)
	: TestCase			(testCtx, name)
	, m_useDeviceGroups	(useDeviceGroups)
	, m_imageType		(imageType)
	, m_imageSize		(imageSize)
	, m_format			(format)
{
}

void ImageSparseRebindCase::checkSupport (Context& context) const
{
	const InstanceInterface&	instance		= context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED);

	// Check if image size does not exceed device limits
	if (!isImageSizeSupported(instance, physicalDevice, m_imageType, m_imageSize))
		TCU_THROW(NotSupportedError, "Image size not supported for device");

	// Check if device supports sparse operations for image type
	if (!checkSparseSupportForImageType(instance, physicalDevice, m_imageType))
		TCU_THROW(NotSupportedError, "Sparse residency for image type is not supported");
}

class ImageSparseRebindInstance : public SparseResourcesBaseInstance
{
public:
	ImageSparseRebindInstance	(Context&			context,
								 const ImageType	imageType,
								 const tcu::UVec3&	imageSize,
								 const VkFormat		format,
								 const bool			useDeviceGroups);

	tcu::TestStatus	iterate		(void);

private:
	const bool			m_useDeviceGroups;
	const ImageType		m_imageType;
	const tcu::UVec3	m_imageSize;
	const VkFormat		m_format;

	VkClearColorValue	getColorClearValue(deUint32 memoryIdx);
};

ImageSparseRebindInstance::ImageSparseRebindInstance	(Context&			context,
														 const ImageType	imageType,
														 const tcu::UVec3&	imageSize,
														 const VkFormat		format,
														 const bool			useDeviceGroups)
	: SparseResourcesBaseInstance	(context, useDeviceGroups)
	, m_useDeviceGroups				(useDeviceGroups)
	, m_imageType					(imageType)
	, m_imageSize					(imageSize)
	, m_format						(format)
{
}

VkClearColorValue ImageSparseRebindInstance::getColorClearValue(deUint32 memoryIdx)
{
	deUint32 startI[kMemoryObjectCount] = { 7, 13 };
	deUint32 startU[kMemoryObjectCount] = { 53u, 61u };
	float startF[kMemoryObjectCount] = { 1.0f, 0.5f };
	VkClearColorValue result = {};

	if (isIntFormat(m_format))
	{
		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			result.int32[channelNdx] = startI[memoryIdx] * static_cast<deInt32>(channelNdx + 1) * (-1 * channelNdx % 2);
		}
	}
	else if (isUintFormat(m_format))
	{
		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			result.uint32[channelNdx] = startU[memoryIdx] * (channelNdx + 1);
		}
	} else {
		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			result.float32[channelNdx] = startF[memoryIdx] - (0.1f * static_cast<float>(channelNdx));
		}
	}


	return result;
}

tcu::TestStatus ImageSparseRebindInstance::iterate (void)
{
	const float					epsilon					= 1e-5f;
	const InstanceInterface&	instance				= m_context.getInstanceInterface();

	{
		// Create logical device supporting both sparse and transfer queues
		QueueRequirementsVec queueRequirements;
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_TRANSFER_BIT, 1u));

		createDeviceSupportingQueues(queueRequirements);
	}

	const VkPhysicalDevice		physicalDevice			= getPhysicalDevice();
	VkImageCreateInfo			imageSparseInfo;
	std::vector<DeviceMemorySp>	deviceMemUniquePtrVec;

	//vsk getting queues should be outside the loop
	//see these in all image files

	const DeviceInterface&			deviceInterface		= getDeviceInterface();
	const Queue&					sparseQueue			= getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
	const Queue&					transferQueue		= getQueue(VK_QUEUE_TRANSFER_BIT, 0);
	const PlanarFormatDescription	formatDescription	= getPlanarFormatDescription(m_format);

	// Go through all physical devices
	for (deUint32 physDevID = 0; physDevID < m_numPhysicalDevices; physDevID++)
	{
		const deUint32	firstDeviceID			= physDevID;
		const deUint32	secondDeviceID			= (firstDeviceID + 1) % m_numPhysicalDevices;

		imageSparseInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageSparseInfo.pNext					= DE_NULL;
		imageSparseInfo.flags					= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
		imageSparseInfo.imageType				= mapImageType(m_imageType);
		imageSparseInfo.format					= m_format;
		imageSparseInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageSize));
		imageSparseInfo.mipLevels				= 1;
		imageSparseInfo.arrayLayers				= getNumLayers(m_imageType, m_imageSize);
		imageSparseInfo.samples					= VK_SAMPLE_COUNT_1_BIT;
		imageSparseInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
		imageSparseInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
		imageSparseInfo.usage					= VK_IMAGE_USAGE_TRANSFER_DST_BIT |
												  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageSparseInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
		imageSparseInfo.queueFamilyIndexCount	= 0u;
		imageSparseInfo.pQueueFamilyIndices		= DE_NULL;

		if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
			imageSparseInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		// Check if device supports sparse operations for image format
		if (!checkSparseSupportForImageFormat(instance, physicalDevice, imageSparseInfo))
			TCU_THROW(NotSupportedError, "The image format does not support sparse operations");

		{
			VkImageFormatProperties imageFormatProperties;
			if (instance.getPhysicalDeviceImageFormatProperties(physicalDevice,
				imageSparseInfo.format,
				imageSparseInfo.imageType,
				imageSparseInfo.tiling,
				imageSparseInfo.usage,
				imageSparseInfo.flags,
				&imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
			{
				TCU_THROW(NotSupportedError, "Image format does not support sparse operations");
			}
		}

		// Create sparse image
		const Unique<VkImage> image(createImage(deviceInterface, getDevice(), &imageSparseInfo));

		// Create semaphores to synchronize sparse binding operations with transfer operations on the sparse images
		const Unique<VkSemaphore> bindSemaphore(createSemaphore(deviceInterface, getDevice()));
		const Unique<VkSemaphore> transferSemaphore(createSemaphore(deviceInterface, getDevice()));

		std::vector<VkSparseImageMemoryRequirements> sparseMemoryRequirements;

		// Get sparse image general memory requirements
		const VkMemoryRequirements imageMemoryRequirements = getImageMemoryRequirements(deviceInterface, getDevice(), *image);

		// Check if required image memory size does not exceed device limits
		if (imageMemoryRequirements.size > getPhysicalDeviceProperties(instance, getPhysicalDevice(secondDeviceID)).limits.sparseAddressSpaceSize)
			TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

		DE_ASSERT((imageMemoryRequirements.size % imageMemoryRequirements.alignment) == 0);

		const deUint32 memoryType = findMatchingMemoryType(instance, getPhysicalDevice(secondDeviceID), imageMemoryRequirements, MemoryRequirement::Any);

		if (memoryType == NO_MATCH_FOUND)
			return tcu::TestStatus::fail("No matching memory type found");

		if (firstDeviceID != secondDeviceID)
		{
			VkPeerMemoryFeatureFlags	peerMemoryFeatureFlags	= (VkPeerMemoryFeatureFlags)0;
			const deUint32				heapIndex				= getHeapIndexForMemoryType(instance, getPhysicalDevice(secondDeviceID), memoryType);
			deviceInterface.getDeviceGroupPeerMemoryFeatures(getDevice(), heapIndex, firstDeviceID, secondDeviceID, &peerMemoryFeatureFlags);

			if (((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT) == 0) ||
				((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT) == 0) ||
				((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT) == 0))
			{
				TCU_THROW(NotSupportedError, "Peer memory does not support COPY_SRC, COPY_DST, and GENERIC_DST");
			}
		}

		// Get sparse image sparse memory requirements
		sparseMemoryRequirements = getImageSparseMemoryRequirements(deviceInterface, getDevice(), *image);
		DE_ASSERT(sparseMemoryRequirements.size() != 0);

		// Select only one layer to partial bind
		const deUint32 partiallyBoundLayer = imageSparseInfo.arrayLayers - 1;

		// Prepare the binding structures and calculate the memory size
		VkDeviceSize allocationSize = 0;

		std::vector<VkSparseImageMemoryBind>	imageFullBinds[kMemoryObjectCount];
		VkSparseImageMemoryBind					imagePartialBind;

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags		aspect				= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
			const deUint32					aspectIndex			= getSparseAspectRequirementsIndex(sparseMemoryRequirements, aspect);

			if (aspectIndex == NO_MATCH_FOUND)
				TCU_THROW(NotSupportedError, "Not supported image aspect");

			VkSparseImageMemoryRequirements	aspectRequirements	= sparseMemoryRequirements[aspectIndex];

			VkExtent3D						imageGranularity	= aspectRequirements.formatProperties.imageGranularity;
			const VkExtent3D				planeExtent			= getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, 0);
			const tcu::UVec3				sparseBlocks		= alignedDivide(planeExtent, imageGranularity);
			const deUint32					numSparseBlocks		= sparseBlocks.x() * sparseBlocks.y() * sparseBlocks.z();

			if (numSparseBlocks < 2)
				TCU_THROW(NotSupportedError, "Image size is too small for partial binding");

			if (aspectRequirements.imageMipTailFirstLod == 0)
				TCU_THROW(NotSupportedError, "Image needs mip tail for mip level 0, partial binding is not possible");

			for (deUint32 layerNdx = 0; layerNdx < imageSparseInfo.arrayLayers; ++layerNdx)
			{
				const VkImageSubresource		subresource		= { aspect, 0, layerNdx };

				const VkSparseImageMemoryBind	imageFullBind	=
				{
					subresource,							// VkImageSubresource		subresource;
					makeOffset3D(0u, 0u, 0u),				// VkOffset3D				offset;
					planeExtent,							// VkExtent3D				extent;
					VK_NULL_HANDLE,							// VkDeviceMemory			memory; // will be patched in later
					allocationSize,							// VkDeviceSize				memoryOffset;
					(VkSparseMemoryBindFlags)0u,			// VkSparseMemoryBindFlags	flags;
				};

				for (deUint32 memoryIdx = 0; memoryIdx < kMemoryObjectCount; memoryIdx++)
					imageFullBinds[memoryIdx].push_back(imageFullBind);

				// Partially bind only one layer
				if (layerNdx == partiallyBoundLayer)
				{
					// Offset by one block in every direction if possible
					VkOffset3D partialOffset = makeOffset3D(0, 0, 0);
					if (sparseBlocks.x() > 1) partialOffset.x = imageGranularity.width;
					if (sparseBlocks.y() > 1) partialOffset.y = imageGranularity.height;
					if (sparseBlocks.z() > 1) partialOffset.z = imageGranularity.depth;

					// Map only one block and clamp it to the image dimensions
					VkExtent3D partialExtent = makeExtent3D(
						de::min(imageGranularity.width, planeExtent.width - partialOffset.x),
						de::min(imageGranularity.height, planeExtent.height - partialOffset.y),
						de::min(imageGranularity.depth, planeExtent.depth - partialOffset.z)
					);

					imagePartialBind =
					{
						subresource,							// VkImageSubresource		subresource;
						partialOffset,							// VkOffset3D				offset;
						partialExtent,							// VkExtent3D				extent;
						VK_NULL_HANDLE,							// VkDeviceMemory			memory; // will be patched in later
						allocationSize,							// VkDeviceSize				memoryOffset;
						(VkSparseMemoryBindFlags)0u,			// VkSparseMemoryBindFlags	flags;
					};
				}

				allocationSize += imageMemoryRequirements.alignment * numSparseBlocks;
			}
		}

		// Alocate device memory
		const VkMemoryAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	// VkStructureType	sType;
			DE_NULL,								// const void*		pNext;
			allocationSize,							// VkDeviceSize		allocationSize;
			memoryType,								// deUint32			memoryTypeIndex;
		};

		std::vector<Move<VkDeviceMemory>> deviceMemories;
		for (deUint32 memoryIdx = 0; memoryIdx < kMemoryObjectCount; memoryIdx++)
		{
			VkDeviceMemory deviceMemory = 0;
			VK_CHECK(deviceInterface.allocateMemory(getDevice(), &allocInfo, DE_NULL, &deviceMemory));
			deviceMemories.push_back(Move<VkDeviceMemory> (check<VkDeviceMemory>(deviceMemory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL)));
		}

		// Patch-in the newly generate memory objects into pre-created binding structures
		for (deUint32 i = 0; i < imageFullBinds[0].size(); i++)
		{
			for (deUint32 memoryIdx = 0; memoryIdx < kMemoryObjectCount; memoryIdx++)
			{
				DE_ASSERT(imageFullBinds[0].size() == imageFullBinds[memoryIdx].size());
				imageFullBinds[memoryIdx][i].memory = *deviceMemories[memoryIdx];
			}
		}

		imagePartialBind.memory = *deviceMemories[0];

		const Unique<VkCommandPool> commandPool(makeCommandPool(deviceInterface, getDevice(), transferQueue.queueFamilyIndex));

		const VkPipelineStageFlags waitStageBits[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };

		// Fully bind the memory and fill it with a value
		for (deUint32 memoryIdx = 0; memoryIdx < kMemoryObjectCount; memoryIdx++)
		{
			const VkDeviceGroupBindSparseInfo	devGroupBindSparseInfo =
			{
				VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO,		// VkStructureType							sType;
				DE_NULL,												// const void*								pNext;
				firstDeviceID,											// deUint32									resourceDeviceIndex;
				secondDeviceID,											// deUint32									memoryDeviceIndex;
			};

			VkBindSparseInfo					bindSparseInfo =
			{
				VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,						// VkStructureType							sType;
				m_useDeviceGroups ? &devGroupBindSparseInfo : DE_NULL,	// const void*								pNext;
				memoryIdx == 0 ? 0u : 1u,								// deUint32									waitSemaphoreCount;
				&transferSemaphore.get(),								// const VkSemaphore*						pWaitSemaphores;
				0u,														// deUint32									bufferBindCount;
				DE_NULL,												// const VkSparseBufferMemoryBindInfo*		pBufferBinds;
				0u,														// deUint32									imageOpaqueBindCount;
				DE_NULL,												// const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
				0u,														// deUint32									imageBindCount;
				DE_NULL,												// const VkSparseImageMemoryBindInfo*		pImageBinds;
				1u,														// deUint32									signalSemaphoreCount;
				&bindSemaphore.get()									// const VkSemaphore*						pSignalSemaphores;
			};

			VkSparseImageMemoryBindInfo			imageBindInfo;

			if (imageFullBinds[memoryIdx].size() > 0)
			{
				imageBindInfo.image				= *image;
				imageBindInfo.bindCount			= static_cast<deUint32>(imageFullBinds[memoryIdx].size());
				imageBindInfo.pBinds			= imageFullBinds[memoryIdx].data();

				bindSparseInfo.imageBindCount	= 1u;
				bindSparseInfo.pImageBinds		= &imageBindInfo;
			}

			// Submit sparse bind commands
			VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));

			const Unique<VkCommandBuffer> commandBuffer(allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

			beginCommandBuffer(deviceInterface, *commandBuffer);

			// Clear everything
			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags		aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

				const VkImageSubresourceRange	range =
				{
					aspect,									// VkImageAspectFlags				aspectMask;
					0,										// uint32_t							baseMipLevel;
					VK_REMAINING_MIP_LEVELS,				// uint32_t							levelCount;
					0,										// uint32_t							baseArrayLayer;
					VK_REMAINING_ARRAY_LAYERS				// uint32_t							layerCount;
				};

				const VkImageMemoryBarrier		imageMemoryBarrierBefore =
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType					sType;
					DE_NULL,								// const void*						pNext;
					0u,										// VkAccessFlags					srcAccessMask;
					VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags					dstAccessMask;
					VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout					oldLayout;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout					newLayout;
					VK_QUEUE_FAMILY_IGNORED,				// uint32_t							srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,				// uint32_t							dstQueueFamilyIndex;
					*image,									// VkImage							image;
					range									// VkImageSubresourceRange			subresourceRange;
				};

				deviceInterface.cmdPipelineBarrier(
					*commandBuffer,							// VkCommandBuffer					commandBuffer,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,		// VkPipelineStageFlags				srcStageMask,
					VK_PIPELINE_STAGE_TRANSFER_BIT,			// VkPipelineStageFlags				dstStageMask,
					(VkDependencyFlagBits)0u,				// VkDependencyFlags				dependencyFlags,
					0,										// uint32_t							memoryBarrierCount,
					nullptr,								// const VkMemoryBarrier *			pMemoryBarriers,
					0,										// uint32_t							bufferMemoryBarrierCount,
					nullptr,								// const VkBufferMemoryBarrier *	pBufferMemoryBarriers,
					1,										// uint32_t							imageMemoryBarrierCount,
					&imageMemoryBarrierBefore				// const VkImageMemoryBarrier *		pImageMemoryBarriers
				);
				VkClearColorValue				clearValue = getColorClearValue(memoryIdx);
				deviceInterface.cmdClearColorImage
				(
					*commandBuffer,							// VkCommandBuffer					commandBuffer,
					*image,									// VkImage							image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout					imageLayout,
					&clearValue,							// VkClearColorValue *				pColor,
					1u,										// uint32_t							rangeCount,
					&range									// VkImageSubresourceRange *		pRanges
				);

				const VkImageMemoryBarrier		imageMemoryBarrierAfter =
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType					sType;
					DE_NULL,								// const void*						pNext;
					VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags					srcAccessMask;
					VK_ACCESS_TRANSFER_READ_BIT,			// VkAccessFlags					dstAccessMask;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout					oldLayout;
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,	// VkImageLayout					newLayout;
					VK_QUEUE_FAMILY_IGNORED,				// uint32_t							srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,				// uint32_t							dstQueueFamilyIndex;
					*image,									// VkImage							image;
					range									// VkImageSubresourceRange			subresourceRange;
				};

				deviceInterface.cmdPipelineBarrier(
					*commandBuffer,							// VkCommandBuffer					commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT,			// VkPipelineStageFlags				srcStageMask,
					VK_PIPELINE_STAGE_TRANSFER_BIT,			// VkPipelineStageFlags				dstStageMask,
					(VkDependencyFlagBits)0u,				// VkDependencyFlags				dependencyFlags,
					0,										// uint32_t							memoryBarrierCount,
					nullptr,								// const VkMemoryBarrier *			pMemoryBarriers,
					0,										// uint32_t							bufferMemoryBarrierCount,
					nullptr,								// const VkBufferMemoryBarrier *	pBufferMemoryBarriers,
					1,										// uint32_t							imageMemoryBarrierCount,
					&imageMemoryBarrierAfter				// const VkImageMemoryBarrier *		pImageMemoryBarriers
				);
			}

			endCommandBuffer(deviceInterface, *commandBuffer);

			// Wait for the sparse bind operation semaphore, submit and wait on host for the transfer stage.
			// In case of device groups, submit on the physical device with the resource.
			submitCommandsAndWait(
				deviceInterface,							// DeviceInterface&					vk,
				getDevice(),								// VkDevice							device,
				transferQueue.queueHandle,					// VkQueue							queue,
				*commandBuffer,								// VkCommandBuffer					commandBuffer,
				1u,											// deUint32							waitSemaphoreCount,
				&bindSemaphore.get(),						// VkSemaphore*						pWaitSemaphores,
				waitStageBits,								// VkPipelineStageFlags*			pWaitDstStageMask,
				1u,											// deUint32							signalSemaphoreCount,
				&transferSemaphore.get(),					// VkSemaphore*						pSignalSemaphores)
				m_useDeviceGroups,							// bool								useDeviceGroups,
				firstDeviceID								// deUint32							physicalDeviceID

			);
		}

		// Partially bind memory 1 back to the image
		{
			const VkDeviceGroupBindSparseInfo	devGroupBindSparseInfo =
			{
				VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO,		// VkStructureType							sType;
				DE_NULL,												// const void*								pNext;
				firstDeviceID,											// deUint32									resourceDeviceIndex;
				secondDeviceID,											// deUint32									memoryDeviceIndex;
			};

			VkSparseImageMemoryBindInfo			imageBindInfo =
			{
				*image,													// VkImage							image;
				1,														// uint32_t							bindCount;
				&imagePartialBind										// const VkSparseImageMemoryBind*	pBinds;
			};

			VkBindSparseInfo					bindSparseInfo =
			{
				VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,						// VkStructureType							sType;
				m_useDeviceGroups ? &devGroupBindSparseInfo : DE_NULL,	// const void*								pNext;
				1u,														// deUint32									waitSemaphoreCount;
				&transferSemaphore.get(),								// const VkSemaphore*						pWaitSemaphores;
				0u,														// deUint32									bufferBindCount;
				DE_NULL,												// const VkSparseBufferMemoryBindInfo*		pBufferBinds;
				0u,														// deUint32									imageOpaqueBindCount;
				DE_NULL,												// const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
				1u,														// deUint32									imageBindCount;
				&imageBindInfo,											// const VkSparseImageMemoryBindInfo*		pImageBinds;
				1u,														// deUint32									signalSemaphoreCount;
				&bindSemaphore.get()									// const VkSemaphore*						pSignalSemaphores;
			};

			// Submit sparse bind commands for execution
			VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));
		}

		// Verify the results
		// Create a big buffer ...
		deUint32	bufferSize = 0;
		deUint32	bufferOffsets[PlanarFormatDescription::MAX_PLANES];
		deUint32	bufferRowPitches[PlanarFormatDescription::MAX_PLANES];

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkExtent3D planeExtent	= getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, 0);
			bufferOffsets[planeNdx]			= bufferSize;
			bufferRowPitches[planeNdx]		= formatDescription.planes[planeNdx].elementSizeBytes * planeExtent.width;
			bufferSize						+= getImageMipLevelSizeInBytes(imageSparseInfo.extent, 1, formatDescription, planeNdx, 0, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
		}

		const VkBufferCreateInfo		outputBufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const Unique<VkBuffer>			outputBuffer(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
		const de::UniquePtr<Allocation>	outputBufferAlloc(bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));

		std::vector<VkBufferImageCopy> bufferImageCopy(formatDescription.numPlanes);
		{
			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

				bufferImageCopy[planeNdx] =
				{
					bufferOffsets[planeNdx],													//	VkDeviceSize				bufferOffset;
					0u,																			//	deUint32					bufferRowLength;
					0u,																			//	deUint32					bufferImageHeight;
					makeImageSubresourceLayers(aspect, 0u, partiallyBoundLayer, 1u),			//	VkImageSubresourceLayers	imageSubresource;
					makeOffset3D(0, 0, 0),														//	VkOffset3D					imageOffset;
					vk::getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, 0u)	//	VkExtent3D					imageExtent;
				};
			}
		}

		//... and copy selected layer into it
		{
			const Unique<VkCommandBuffer> commandBuffer(allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

			beginCommandBuffer(deviceInterface, *commandBuffer);

			deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outputBuffer, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());

			// Make the changes visible to the host
			{
				const VkBufferMemoryBarrier outputBufferHostBarrier
					= makeBufferMemoryBarrier(
						VK_ACCESS_TRANSFER_WRITE_BIT,		// VkAccessFlags			srcAccessMask,
						VK_ACCESS_HOST_READ_BIT,			// VkAccessFlags			dstAccessMask,
						*outputBuffer,						// VkBuffer					buffer,
						0ull,								// VkDeviceSize				offset,
						bufferSize);						// VkDeviceSize				bufferSizeBytes,

				deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferHostBarrier, 0u, DE_NULL);
			}

			endCommandBuffer(deviceInterface, *commandBuffer);

			// Wait for the sparse bind operations, submit and wait on host for the transfer stage.
			// In case of device groups, submit on the physical device with the resource.
			submitCommandsAndWait(
				deviceInterface,							// DeviceInterface&			vk,
				getDevice(),								// VkDevice					device,
				transferQueue.queueHandle,					// VkQueue					queue,
				*commandBuffer,								// VkCommandBuffer			commandBuffer,
				1u,											// deUint32					waitSemaphoreCount,
				&bindSemaphore.get(),						// VkSemaphore*				pWaitSemaphores,
				waitStageBits,								// VkPipelineStageFlags*	pWaitDstStageMask,
				0,											// deUint32					signalSemaphoreCount,
				DE_NULL,									// VkSemaphore*				pSignalSemaphores,
				m_useDeviceGroups,							// bool						useDeviceGroups,
				firstDeviceID								// deUint32					physicalDeviceID
			);
		}

		// Retrieve data from output buffer to host memory
		invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

		deUint8*	outputData	= static_cast<deUint8*>(outputBufferAlloc->getHostPtr());

		void*		bufferPointers[PlanarFormatDescription::MAX_PLANES];

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			bufferPointers[planeNdx] = outputData + static_cast<size_t>(bufferOffsets[planeNdx]);


		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			if (!formatDescription.hasChannelNdx(channelNdx))
				continue;

			deUint32							planeNdx					= formatDescription.channels[channelNdx].planeNdx;
			vk::VkFormat						planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
			vk::PlanarFormatDescription			compatibleFormatDescription	= (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;

			const tcu::UVec3					size (imageSparseInfo.extent.width, imageSparseInfo.extent.height, imageSparseInfo.extent.depth);
			const tcu::ConstPixelBufferAccess	pixelBuffer					= vk::getChannelAccess(compatibleFormatDescription, size, bufferRowPitches, (const void* const*)bufferPointers, channelNdx);
			tcu::IVec3							pixelDivider				= pixelBuffer.getDivider();

			std::ostringstream str;
			str << "image" << channelNdx;
			m_context.getTestContext().getLog() << tcu::LogImage(str.str(), str.str(), pixelBuffer);

			const VkExtent3D					extent						= getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, 0);
			const VkOffset3D					partialBindOffset			= imagePartialBind.offset;
			const VkExtent3D					partialBindExtent			= imagePartialBind.extent;

			for (deUint32 offsetZ = 0u; offsetZ < extent.depth;  ++offsetZ)
			for (deUint32 offsetY = 0u; offsetY < extent.height; ++offsetY)
			for (deUint32 offsetX = 0u; offsetX < extent.width;  ++offsetX)
			{
				float			fReferenceValue		= 0.0f;
				deUint32		uReferenceValue		= 0;
				deInt32			iReferenceValue		= 0;
				float			acceptableError		= epsilon;

				if (
					(offsetX >= static_cast<deUint32>(partialBindOffset.x) && offsetX < partialBindOffset.x + partialBindExtent.width) &&
					(offsetY >= static_cast<deUint32>(partialBindOffset.y) && offsetY < partialBindOffset.y + partialBindExtent.height) &&
					(offsetZ >= static_cast<deUint32>(partialBindOffset.z) && offsetZ < partialBindOffset.z + partialBindExtent.depth)
				) {
					fReferenceValue = getColorClearValue(0).float32[channelNdx];
					uReferenceValue = getColorClearValue(0).uint32[channelNdx];
					iReferenceValue = getColorClearValue(0).int32[channelNdx];
				} else {
					fReferenceValue = getColorClearValue(kMemoryObjectCount - 1).float32[channelNdx];
					uReferenceValue = getColorClearValue(kMemoryObjectCount - 1).uint32[channelNdx];
					iReferenceValue = getColorClearValue(kMemoryObjectCount - 1).uint32[channelNdx];
				}

				switch (formatDescription.channels[channelNdx].type)
				{
					case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
					{
						const tcu::IVec4 outputValue = pixelBuffer.getPixelInt(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

						if (outputValue.x() != iReferenceValue)
							return tcu::TestStatus::fail("Failed");

						break;
					}
					case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
					{
						const tcu::UVec4 outputValue = pixelBuffer.getPixelUint(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

						if (outputValue.x() != uReferenceValue)
							return tcu::TestStatus::fail("Failed");

						break;
					}
					case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
					case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
					{
						int numAccurateBits = formatDescription.channels[channelNdx].sizeBits;
						if (formatDescription.channels[channelNdx].type == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT)
							numAccurateBits -= 1;
						float fixedPointError = tcu::TexVerifierUtil::computeFixedPointError(numAccurateBits);
						acceptableError += fixedPointError;
						const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

						if (deAbs(outputValue.x() - fReferenceValue) > acceptableError)
							return tcu::TestStatus::fail("Failed");

						break;
					}
					case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
					{
						const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

						if (deAbs(outputValue.x() - fReferenceValue) > acceptableError)
							return tcu::TestStatus::fail("Failed");

						break;
					}
					default:	DE_FATAL("Unexpected channel type");	break;
				}
			}
		}
	}

	return tcu::TestStatus::pass("Passed");
}

TestInstance* ImageSparseRebindCase::createInstance (Context& context) const
{
	return new ImageSparseRebindInstance(context, m_imageType, m_imageSize, m_format, m_useDeviceGroups);
}

} // anonymous ns

tcu::TestCaseGroup* createImageSparseRebindTestsCommon(tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup> testGroup, const bool useDeviceGroup = false)
{
	const std::vector<TestImageParameters> imageParameters
	{
		{ IMAGE_TYPE_2D,		{ tcu::UVec3(512u, 256u, 1u),	tcu::UVec3(128u, 128u, 1u),	tcu::UVec3(503u, 137u, 1u) },	getTestFormats(IMAGE_TYPE_2D) },
		{ IMAGE_TYPE_2D_ARRAY,	{ tcu::UVec3(512u, 256u, 6u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(503u, 137u, 3u) },	getTestFormats(IMAGE_TYPE_2D_ARRAY) },
		{ IMAGE_TYPE_CUBE,		{ tcu::UVec3(256u, 256u, 1u),	tcu::UVec3(128u, 128u, 1u),	tcu::UVec3(137u, 137u, 1u) },	getTestFormats(IMAGE_TYPE_CUBE) },
		{ IMAGE_TYPE_CUBE_ARRAY,{ tcu::UVec3(256u, 256u, 6u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(137u, 137u, 3u) },	getTestFormats(IMAGE_TYPE_CUBE_ARRAY) },
		{ IMAGE_TYPE_3D,		{ tcu::UVec3(256u, 256u, 16u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(503u, 137u, 3u) },	getTestFormats(IMAGE_TYPE_3D) }
	};

	for (size_t imageTypeNdx = 0; imageTypeNdx < imageParameters.size(); ++imageTypeNdx)
	{
		const ImageType					imageType = imageParameters[imageTypeNdx].imageType;
		de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str()));

		for (size_t formatNdx = 0; formatNdx < imageParameters[imageTypeNdx].formats.size(); ++formatNdx)
		{
			VkFormat						format				= imageParameters[imageTypeNdx].formats[formatNdx].format;

			// skip YCbCr formats for simplicity
			if (isYCbCrFormat(format))
				continue;

			de::MovePtr<tcu::TestCaseGroup> formatGroup			(new tcu::TestCaseGroup(testCtx, getImageFormatID(format).c_str()));

			for (size_t imageSizeNdx = 0; imageSizeNdx < imageParameters[imageTypeNdx].imageSizes.size(); ++imageSizeNdx)
			{
				const tcu::UVec3 imageSize = imageParameters[imageTypeNdx].imageSizes[imageSizeNdx];

				std::ostringstream stream;
				stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

				formatGroup->addChild(new ImageSparseRebindCase(testCtx, stream.str(), imageType, imageSize, format, useDeviceGroup));
			}
			imageTypeGroup->addChild(formatGroup.release());
		}
		testGroup->addChild(imageTypeGroup.release());
	}

	return testGroup.release();
}

tcu::TestCaseGroup* createImageSparseRebindTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_rebind"));
	return createImageSparseRebindTestsCommon(testCtx, testGroup);
}

} // sparse
} // vkt
