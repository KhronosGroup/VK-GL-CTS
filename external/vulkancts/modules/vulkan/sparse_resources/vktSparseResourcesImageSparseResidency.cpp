/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesImageSparseResidency.cpp
 * \brief Sparse partially resident images tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferSparseBinding.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTestLog.hpp"

#include "deMath.h"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuTexVerifierUtil.hpp"

#include <string>
#include <vector>
#include <sstream>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

std::string getFormatValueString	(const std::vector<std::pair<deUint32, deUint32>>& channelsOnPlane,
									 const std::vector<std::string>& formatValueStrings)
{
	std::string result = "( ";
	deUint32 i;
	for (i=0; i<channelsOnPlane.size(); ++i)
	{
		result += formatValueStrings[channelsOnPlane[i].first];
		if (i < 3)
			result += ", ";
	}
	for (; i < 4; ++i)
	{
		result += "0";
		if (i < 3)
			result += ", ";
	}
	result += " )";
	return result;
}

const std::string getCoordStr	(const ImageType	imageType,
								 const std::string&	x,
								 const std::string&	y,
								 const std::string&	z)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return x;

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D:
			return "ivec2(" + x + "," + y + ")";

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_3D:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return "ivec3(" + x + "," + y + "," + z + ")";

		default:
			DE_ASSERT(false);
			return "";
	}
}

tcu::UVec3 computeWorkGroupSize (const VkExtent3D& planeExtent)
{
	const deUint32		maxComputeWorkGroupInvocations	= 128u;
	const tcu::UVec3	maxComputeWorkGroupSize			= tcu::UVec3(128u, 128u, 64u);

	const deUint32 xWorkGroupSize = std::min(std::min(planeExtent.width,	maxComputeWorkGroupSize.x()), maxComputeWorkGroupInvocations);
	const deUint32 yWorkGroupSize = std::min(std::min(planeExtent.height,	maxComputeWorkGroupSize.y()), maxComputeWorkGroupInvocations /  xWorkGroupSize);
	const deUint32 zWorkGroupSize = std::min(std::min(planeExtent.depth,	maxComputeWorkGroupSize.z()), maxComputeWorkGroupInvocations / (xWorkGroupSize*yWorkGroupSize));

	return tcu::UVec3(xWorkGroupSize, yWorkGroupSize, zWorkGroupSize);
}

class ImageSparseResidencyCase : public TestCase
{
public:
	ImageSparseResidencyCase		(tcu::TestContext&		testCtx,
									 const std::string&		name,
									 const std::string&		description,
									 const ImageType		imageType,
									 const tcu::UVec3&		imageSize,
									 const VkFormat			format,
									 const glu::GLSLVersion	glslVersion,
									 const bool				useDeviceGroups);

	void			initPrograms	(SourceCollections&		sourceCollections) const;
	virtual void	checkSupport	(Context&				context) const;
	TestInstance*	createInstance	(Context&				context) const;

private:
	const bool				m_useDeviceGroups;
	const ImageType			m_imageType;
	const tcu::UVec3		m_imageSize;
	const VkFormat			m_format;
	const glu::GLSLVersion	m_glslVersion;
};

ImageSparseResidencyCase::ImageSparseResidencyCase	(tcu::TestContext&		testCtx,
													 const std::string&		name,
													 const std::string&		description,
													 const ImageType		imageType,
													 const tcu::UVec3&		imageSize,
													 const VkFormat			format,
													 const glu::GLSLVersion	glslVersion,
													 const bool				useDeviceGroups)
	: TestCase			(testCtx, name, description)
	, m_useDeviceGroups	(useDeviceGroups)
	, m_imageType		(imageType)
	, m_imageSize		(imageSize)
	, m_format			(format)
	, m_glslVersion		(glslVersion)
{
}

void ImageSparseResidencyCase::initPrograms (SourceCollections&	sourceCollections) const
{
	// Create compute program
	const char* const				versionDecl			= glu::getGLSLVersionDeclaration(m_glslVersion);
	const PlanarFormatDescription	formatDescription	= getPlanarFormatDescription(m_format);
	const std::string				imageTypeStr		= getShaderImageType(formatDescription, m_imageType);
	const std::string				formatDataStr		= getShaderImageDataType(formatDescription);
	const tcu::UVec3				shaderGridSize		= getShaderGridSize(m_imageType, m_imageSize);

	std::vector<std::string>		formatValueStrings;
	switch (formatDescription.channels[0].type)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			formatValueStrings = {
				"int(gl_GlobalInvocationID.x) % 127",
				"int(gl_GlobalInvocationID.y) % 127",
				"int(gl_GlobalInvocationID.z) % 127",
				"1"
			};
			break;
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			formatValueStrings = {
				"float(int(gl_GlobalInvocationID.x) % 127) / 127.0" ,
				"float(int(gl_GlobalInvocationID.y) % 127) / 127.0",
				"float(int(gl_GlobalInvocationID.z) % 127) / 127.0",
				"1.0"
			};
			break;
		default:	DE_ASSERT(false);	break;
	}

	for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
	{
		VkFormat						planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
		vk::PlanarFormatDescription		compatibleFormatDescription	= (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;
		VkExtent3D						compatibleShaderGridSize	{ shaderGridSize.x() / formatDescription.blockWidth, shaderGridSize.y() / formatDescription.blockHeight, shaderGridSize.z() / 1u };

		std::vector<std::pair<deUint32, deUint32>> channelsOnPlane;
		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			if (!formatDescription.hasChannelNdx(channelNdx))
				continue;
			if (formatDescription.channels[channelNdx].planeNdx != planeNdx)
				continue;
			channelsOnPlane.push_back({ channelNdx,formatDescription.channels[channelNdx].offsetBits });
		}
		// reorder channels for multi-planar images
		if(formatDescription.numPlanes>1)
			std::sort(begin(channelsOnPlane), end(channelsOnPlane), [](const std::pair<deUint32, deUint32>& lhs, const std::pair<deUint32, deUint32>& rhs) { return lhs.second < rhs.second; });
		std::string			formatValueStr		= getFormatValueString(channelsOnPlane, formatValueStrings);
		VkExtent3D			shaderExtent		= getPlaneExtent(compatibleFormatDescription, compatibleShaderGridSize, planeNdx, 0);
		const std::string	formatQualifierStr	= getShaderImageFormatQualifier(planeCompatibleFormat);
		const tcu::UVec3	workGroupSize		= computeWorkGroupSize(shaderExtent);

		std::ostringstream src;
		src << versionDecl << "\n"
			<< "layout (local_size_x = " << workGroupSize.x() << ", local_size_y = " << workGroupSize.y() << ", local_size_z = " << workGroupSize.z() << ") in; \n"
			<< "layout (binding = 0, " << formatQualifierStr << ") writeonly uniform highp " << imageTypeStr << " u_image;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	if( gl_GlobalInvocationID.x < " << shaderExtent.width << " ) \n"
			<< "	if( gl_GlobalInvocationID.y < " << shaderExtent.height << " ) \n"
			<< "	if( gl_GlobalInvocationID.z < " << shaderExtent.depth << " ) \n"
			<< "	{\n"
			<< "		imageStore(u_image, " << getCoordStr(m_imageType, "gl_GlobalInvocationID.x", "gl_GlobalInvocationID.y", "gl_GlobalInvocationID.z") << ","
			<< formatDataStr << formatValueStr << ");\n"
			<< "	}\n"
			<< "}\n";
		std::ostringstream shaderName;
		shaderName << "comp" << planeNdx;
		sourceCollections.glslSources.add(shaderName.str()) << glu::ComputeSource(src.str());
	}
}

void ImageSparseResidencyCase::checkSupport(Context& context) const
{
	const InstanceInterface&	instance = context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice = context.getPhysicalDevice();

	// Check if image size does not exceed device limits
	if (!isImageSizeSupported(instance, physicalDevice, m_imageType, m_imageSize))
		TCU_THROW(NotSupportedError, "Image size not supported for device");

	// Check if device supports sparse operations for image type
	if (!checkSparseSupportForImageType(instance, physicalDevice, m_imageType))
		TCU_THROW(NotSupportedError, "Sparse residency for image type is not supported");

	 //Check if image format supports storage images
	const VkFormatProperties	formatProperties = getPhysicalDeviceFormatProperties(instance, physicalDevice, m_format);
	if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		TCU_THROW(NotSupportedError, "Storage images are not supported for this format");
}

class ImageSparseResidencyInstance : public SparseResourcesBaseInstance
{
public:
	ImageSparseResidencyInstance	(Context&			context,
									 const ImageType	imageType,
									 const tcu::UVec3&	imageSize,
									 const VkFormat		format,
									 const bool			useDeviceGroups);


	tcu::TestStatus	iterate			(void);

private:
	const bool			m_useDeviceGroups;
	const ImageType		m_imageType;
	const tcu::UVec3	m_imageSize;
	const VkFormat		m_format;
};

ImageSparseResidencyInstance::ImageSparseResidencyInstance	(Context&			context,
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

tcu::TestStatus ImageSparseResidencyInstance::iterate (void)
{
	const float					epsilon				= 1e-5f;
	const InstanceInterface&	instance			= m_context.getInstanceInterface();

	{
		// Create logical device supporting both sparse and compute queues
		QueueRequirementsVec queueRequirements;
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

		createDeviceSupportingQueues(queueRequirements);
	}

	VkImageCreateInfo			imageCreateInfo;
	std::vector<DeviceMemorySp>	deviceMemUniquePtrVec;

	const DeviceInterface&			deviceInterface		= getDeviceInterface();
	const Queue&					sparseQueue			= getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
	const Queue&					computeQueue		= getQueue(VK_QUEUE_COMPUTE_BIT, 0);
	const PlanarFormatDescription	formatDescription	= getPlanarFormatDescription(m_format);

	// Go through all physical devices
	for (deUint32 physDevID = 0; physDevID < m_numPhysicalDevices; physDevID++)
	{
		const deUint32						firstDeviceID				= physDevID;
		const deUint32						secondDeviceID				= (firstDeviceID + 1) % m_numPhysicalDevices;

		const VkPhysicalDevice				physicalDevice				= getPhysicalDevice(firstDeviceID);
		const VkPhysicalDeviceProperties	physicalDeviceProperties	= getPhysicalDeviceProperties(instance, physicalDevice);

		imageCreateInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext					= DE_NULL;
		imageCreateInfo.flags					= VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
		imageCreateInfo.imageType				= mapImageType(m_imageType);
		imageCreateInfo.format					= m_format;
		imageCreateInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageSize));
		imageCreateInfo.mipLevels				= 1u;
		imageCreateInfo.arrayLayers				= getNumLayers(m_imageType, m_imageSize);
		imageCreateInfo.samples					= VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage					= VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
												  VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount	= 0u;
		imageCreateInfo.pQueueFamilyIndices		= DE_NULL;

		if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
		{
			imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		// check if we need to create VkImageView with different VkFormat than VkImage format
		VkFormat planeCompatibleFormat0 = getPlaneCompatibleFormatForWriting(formatDescription, 0);
		if (planeCompatibleFormat0 != getPlaneCompatibleFormat(formatDescription, 0))
		{
			imageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		}

		// Check if device supports sparse operations for image format
		if (!checkSparseSupportForImageFormat(instance, physicalDevice, imageCreateInfo))
			TCU_THROW(NotSupportedError, "The image format does not support sparse operations");

		// Create sparse image
		const Unique<VkImage> imageSparse(createImage(deviceInterface, getDevice(), &imageCreateInfo));

		// Create sparse image memory bind semaphore
		const Unique<VkSemaphore> imageMemoryBindSemaphore(createSemaphore(deviceInterface, getDevice()));

		std::vector<VkSparseImageMemoryRequirements> sparseMemoryRequirements;

		{
			// Get image general memory requirements
			const VkMemoryRequirements imageMemoryRequirements = getImageMemoryRequirements(deviceInterface, getDevice(), *imageSparse);

			if (imageMemoryRequirements.size > physicalDeviceProperties.limits.sparseAddressSpaceSize)
				TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

			DE_ASSERT((imageMemoryRequirements.size % imageMemoryRequirements.alignment) == 0);

			const deUint32						 memoryType = findMatchingMemoryType(instance, getPhysicalDevice(secondDeviceID), imageMemoryRequirements, MemoryRequirement::Any);

			if (memoryType == NO_MATCH_FOUND)
				return tcu::TestStatus::fail("No matching memory type found");

			if (firstDeviceID != secondDeviceID)
			{
				VkPeerMemoryFeatureFlags	peerMemoryFeatureFlags = (VkPeerMemoryFeatureFlags)0;
				const deUint32				heapIndex = getHeapIndexForMemoryType(instance, getPhysicalDevice(secondDeviceID), memoryType);
				deviceInterface.getDeviceGroupPeerMemoryFeatures(getDevice(), heapIndex, firstDeviceID, secondDeviceID, &peerMemoryFeatureFlags);

				if (((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT)    == 0) ||
					((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT) == 0))
				{
					TCU_THROW(NotSupportedError, "Peer memory does not support COPY_SRC and GENERIC_DST");
				}
			}

			// Get sparse image sparse memory requirements
			sparseMemoryRequirements = getImageSparseMemoryRequirements(deviceInterface, getDevice(), *imageSparse);
			DE_ASSERT(sparseMemoryRequirements.size() != 0);

			const deUint32 metadataAspectIndex = getSparseAspectRequirementsIndex(sparseMemoryRequirements, VK_IMAGE_ASPECT_METADATA_BIT);

			std::vector<VkSparseImageMemoryBind>	imageResidencyMemoryBinds;
			std::vector<VkSparseMemoryBind>			imageMipTailMemoryBinds;

			// Bind device memory for each aspect
			for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			{
				const VkImageAspectFlags		aspect				= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
				const deUint32					aspectIndex			= getSparseAspectRequirementsIndex(sparseMemoryRequirements, aspect);

				if (aspectIndex == NO_MATCH_FOUND)
					TCU_THROW(NotSupportedError, "Not supported image aspect");

				VkSparseImageMemoryRequirements	aspectRequirements	= sparseMemoryRequirements[aspectIndex];
				VkExtent3D						imageGranularity	= aspectRequirements.formatProperties.imageGranularity;

				for (deUint32 layerNdx = 0; layerNdx < imageCreateInfo.arrayLayers; ++layerNdx)
				{
					for (deUint32 mipLevelNdx = 0; mipLevelNdx < aspectRequirements.imageMipTailFirstLod; ++mipLevelNdx)
					{
						const VkImageSubresource subresource		= { aspect, mipLevelNdx, layerNdx };
						const VkExtent3D		 planeExtent		= getPlaneExtent(formatDescription, imageCreateInfo.extent, planeNdx, mipLevelNdx);
						const tcu::UVec3		 numSparseBinds		= alignedDivide(planeExtent, imageGranularity);
						const tcu::UVec3		 lastBlockExtent	= tcu::UVec3(planeExtent.width  % imageGranularity.width  ? planeExtent.width  % imageGranularity.width  : imageGranularity.width,
																				 planeExtent.height % imageGranularity.height ? planeExtent.height % imageGranularity.height : imageGranularity.height,
																				 planeExtent.depth  % imageGranularity.depth  ? planeExtent.depth  % imageGranularity.depth  : imageGranularity.depth);

						for (deUint32 z = 0; z < numSparseBinds.z(); ++z)
						for (deUint32 y = 0; y < numSparseBinds.y(); ++y)
						for (deUint32 x = 0; x < numSparseBinds.x(); ++x)
						{
							const deUint32 linearIndex = x + y * numSparseBinds.x() + z * numSparseBinds.x() * numSparseBinds.y() + layerNdx * numSparseBinds.x() * numSparseBinds.y() * numSparseBinds.z();

							if (linearIndex % 2u == 0u)
							{
								VkOffset3D offset;
								offset.x		= x * imageGranularity.width;
								offset.y		= y * imageGranularity.height;
								offset.z		= z * imageGranularity.depth;

								VkExtent3D extent;
								extent.width	= (x == numSparseBinds.x() - 1) ? lastBlockExtent.x() : imageGranularity.width;
								extent.height	= (y == numSparseBinds.y() - 1) ? lastBlockExtent.y() : imageGranularity.height;
								extent.depth	= (z == numSparseBinds.z() - 1) ? lastBlockExtent.z() : imageGranularity.depth;

								const VkSparseImageMemoryBind imageMemoryBind = makeSparseImageMemoryBind(deviceInterface, getDevice(),
									imageMemoryRequirements.alignment, memoryType, subresource, offset, extent);

								deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

								imageResidencyMemoryBinds.push_back(imageMemoryBind);
							}
						}
					}

					if (!(aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) && aspectRequirements.imageMipTailFirstLod < imageCreateInfo.mipLevels)
					{
						const VkSparseMemoryBind imageMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
							aspectRequirements.imageMipTailSize, memoryType, aspectRequirements.imageMipTailOffset + layerNdx * aspectRequirements.imageMipTailStride);

						deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

						imageMipTailMemoryBinds.push_back(imageMipTailMemoryBind);
					}

					// Metadata
					if (metadataAspectIndex != NO_MATCH_FOUND)
					{
						const VkSparseImageMemoryRequirements metadataAspectRequirements = sparseMemoryRequirements[metadataAspectIndex];

						if (!(metadataAspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT))
						{
							const VkSparseMemoryBind imageMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
								metadataAspectRequirements.imageMipTailSize, memoryType,
								metadataAspectRequirements.imageMipTailOffset + layerNdx * metadataAspectRequirements.imageMipTailStride,
								VK_SPARSE_MEMORY_BIND_METADATA_BIT);

							deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

							imageMipTailMemoryBinds.push_back(imageMipTailMemoryBind);
						}
					}
				}

				if ((aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) && aspectRequirements.imageMipTailFirstLod < imageCreateInfo.mipLevels)
				{
					const VkSparseMemoryBind imageMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
						aspectRequirements.imageMipTailSize, memoryType, aspectRequirements.imageMipTailOffset);

					deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

					imageMipTailMemoryBinds.push_back(imageMipTailMemoryBind);
				}
			}

			// Metadata
			if (metadataAspectIndex != NO_MATCH_FOUND)
			{
				const VkSparseImageMemoryRequirements metadataAspectRequirements = sparseMemoryRequirements[metadataAspectIndex];

				if ((metadataAspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT))
				{
					const VkSparseMemoryBind imageMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
						metadataAspectRequirements.imageMipTailSize, memoryType, metadataAspectRequirements.imageMipTailOffset,
						VK_SPARSE_MEMORY_BIND_METADATA_BIT);

					deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

					imageMipTailMemoryBinds.push_back(imageMipTailMemoryBind);
				}
			}

			const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo =
			{
				VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO_KHR,	//VkStructureType							sType;
				DE_NULL,												//const void*								pNext;
				firstDeviceID,											//deUint32									resourceDeviceIndex;
				secondDeviceID,											//deUint32									memoryDeviceIndex;
			};

			VkBindSparseInfo bindSparseInfo =
			{
				VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,						//VkStructureType							sType;
				m_useDeviceGroups ? &devGroupBindSparseInfo : DE_NULL,	//const void*								pNext;
				0u,														//deUint32									waitSemaphoreCount;
				DE_NULL,												//const VkSemaphore*						pWaitSemaphores;
				0u,														//deUint32									bufferBindCount;
				DE_NULL,												//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
				0u,														//deUint32									imageOpaqueBindCount;
				DE_NULL,												//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
				0u,														//deUint32									imageBindCount;
				DE_NULL,												//const VkSparseImageMemoryBindInfo*		pImageBinds;
				1u,														//deUint32									signalSemaphoreCount;
				&imageMemoryBindSemaphore.get()							//const VkSemaphore*						pSignalSemaphores;
			};

			VkSparseImageMemoryBindInfo			imageResidencyBindInfo;
			VkSparseImageOpaqueMemoryBindInfo	imageMipTailBindInfo;

			if (imageResidencyMemoryBinds.size() > 0)
			{
				imageResidencyBindInfo.image		= *imageSparse;
				imageResidencyBindInfo.bindCount	= static_cast<deUint32>(imageResidencyMemoryBinds.size());
				imageResidencyBindInfo.pBinds		= imageResidencyMemoryBinds.data();

				bindSparseInfo.imageBindCount		= 1u;
				bindSparseInfo.pImageBinds			= &imageResidencyBindInfo;
			}

			if (imageMipTailMemoryBinds.size() > 0)
			{
				imageMipTailBindInfo.image			= *imageSparse;
				imageMipTailBindInfo.bindCount		= static_cast<deUint32>(imageMipTailMemoryBinds.size());
				imageMipTailBindInfo.pBinds			= imageMipTailMemoryBinds.data();

				bindSparseInfo.imageOpaqueBindCount = 1u;
				bindSparseInfo.pImageOpaqueBinds	= &imageMipTailBindInfo;
			}

			// Submit sparse bind commands for execution
			VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));
		}

		// Create command buffer for compute and transfer operations
		const Unique<VkCommandPool>		commandPool(makeCommandPool(deviceInterface, getDevice(), computeQueue.queueFamilyIndex));
		const Unique<VkCommandBuffer>	commandBuffer(allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		// Start recording commands
		beginCommandBuffer(deviceInterface, *commandBuffer);

		// Create descriptor set layout
		const Unique<VkDescriptorSetLayout> descriptorSetLayout(
			DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(deviceInterface, getDevice()));

		// Create and bind descriptor set
		const Unique<VkDescriptorPool> descriptorPool(
			DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
			.build(deviceInterface, getDevice(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, vk::PlanarFormatDescription::MAX_PLANES));

		const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(deviceInterface, getDevice(), *descriptorSetLayout));
		std::vector<de::SharedPtr<vk::Unique<vk::VkShaderModule>>>	shaderModules;
		std::vector<de::SharedPtr<vk::Unique<vk::VkPipeline>>>		computePipelines;
		std::vector<de::SharedPtr<vk::Unique<vk::VkDescriptorSet>>>	descriptorSets;
		std::vector<de::SharedPtr<vk::Unique<vk::VkImageView>>>		imageViews;

		const tcu::UVec3 shaderGridSize = getShaderGridSize(m_imageType, m_imageSize);

		// Run compute shader for each image plane
		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags		aspect						= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
			const VkImageSubresourceRange	subresourceRange			= makeImageSubresourceRange(aspect, 0u, 1u, 0u, getNumLayers(m_imageType, m_imageSize));
			VkFormat						planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
			vk::PlanarFormatDescription		compatibleFormatDescription	= (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;
			const tcu::UVec3				compatibleShaderGridSize	( shaderGridSize.x() / formatDescription.blockWidth, shaderGridSize.y() / formatDescription.blockHeight, shaderGridSize.z() / 1u);
			VkExtent3D						shaderExtent				= getPlaneExtent(compatibleFormatDescription, VkExtent3D{ compatibleShaderGridSize.x(), compatibleShaderGridSize.y(), compatibleShaderGridSize.z() }, planeNdx, 0u);

			// Create and bind compute pipeline
			std::ostringstream shaderName;
			shaderName << "comp" << planeNdx;
			auto shaderModule		= makeVkSharedPtr(createShaderModule(deviceInterface, getDevice(), m_context.getBinaryCollection().get(shaderName.str()), DE_NULL));
			shaderModules.push_back(shaderModule);
			auto computePipeline	= makeVkSharedPtr(makeComputePipeline(deviceInterface, getDevice(), *pipelineLayout, shaderModule->get()));
			computePipelines.push_back(computePipeline);
			deviceInterface.cmdBindPipeline	(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->get());

			auto descriptorSet		= makeVkSharedPtr(makeDescriptorSet(deviceInterface, getDevice(), *descriptorPool, *descriptorSetLayout));
			descriptorSets.push_back(descriptorSet);

			auto imageView			= makeVkSharedPtr(makeImageView(deviceInterface, getDevice(), *imageSparse, mapImageViewType(m_imageType), planeCompatibleFormat, subresourceRange));
			imageViews.push_back(imageView);
			const VkDescriptorImageInfo		imageSparseInfo			= makeDescriptorImageInfo(DE_NULL, imageView->get(), VK_IMAGE_LAYOUT_GENERAL);

			DescriptorSetUpdateBuilder()
				.writeSingle(descriptorSet->get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageSparseInfo)
				.update(deviceInterface, getDevice());

			deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet->get(), 0u, DE_NULL);

			{
				const VkImageMemoryBarrier imageSparseLayoutChangeBarrier = makeImageMemoryBarrier
				(
					0u,
					VK_ACCESS_SHADER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_GENERAL,
					*imageSparse,
					subresourceRange,
					sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex ? sparseQueue.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED,
					sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex ? computeQueue.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED
					);

				deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageSparseLayoutChangeBarrier);
			}

			{
				const tcu::UVec3 workGroupSize = computeWorkGroupSize(shaderExtent);

				const deUint32 xWorkGroupCount = shaderExtent.width  / workGroupSize.x() + (shaderExtent.width  % workGroupSize.x() ? 1u : 0u);
				const deUint32 yWorkGroupCount = shaderExtent.height / workGroupSize.y() + (shaderExtent.height % workGroupSize.y() ? 1u : 0u);
				const deUint32 zWorkGroupCount = shaderExtent.depth  / workGroupSize.z() + (shaderExtent.depth  % workGroupSize.z() ? 1u : 0u);

				const tcu::UVec3 maxComputeWorkGroupCount = tcu::UVec3(65535u, 65535u, 65535u);

				if (maxComputeWorkGroupCount.x() < xWorkGroupCount ||
					maxComputeWorkGroupCount.y() < yWorkGroupCount ||
					maxComputeWorkGroupCount.z() < zWorkGroupCount)
				{
					TCU_THROW(NotSupportedError, "Image size is not supported");
				}

				deviceInterface.cmdDispatch(*commandBuffer, xWorkGroupCount, yWorkGroupCount, zWorkGroupCount);
			}

			{
				const VkImageMemoryBarrier imageSparseTransferBarrier = makeImageMemoryBarrier
				(
					VK_ACCESS_SHADER_WRITE_BIT,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					*imageSparse,
					subresourceRange
				);

				deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageSparseTransferBarrier);
			}
		}

		deUint32	imageSizeInBytes = 0;
		deUint32	planeOffsets[PlanarFormatDescription::MAX_PLANES];
		deUint32	planeRowPitches[PlanarFormatDescription::MAX_PLANES];

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			planeOffsets[planeNdx]		= imageSizeInBytes;
			const deUint32	planeW		= imageCreateInfo.extent.width / (formatDescription.blockWidth * formatDescription.planes[planeNdx].widthDivisor);
			planeRowPitches[planeNdx]	= formatDescription.planes[planeNdx].elementSizeBytes * planeW;
			imageSizeInBytes			+= getImageMipLevelSizeInBytes(imageCreateInfo.extent, imageCreateInfo.arrayLayers, formatDescription, planeNdx, 0, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
		}

		const VkBufferCreateInfo		outputBufferCreateInfo	= makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const Unique<VkBuffer>			outputBuffer			(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
		const de::UniquePtr<Allocation>	outputBufferAlloc		(bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));
		std::vector<VkBufferImageCopy>	bufferImageCopy			(formatDescription.numPlanes);

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

			bufferImageCopy[planeNdx] =
			{
				planeOffsets[planeNdx],														//	VkDeviceSize				bufferOffset;
				0u,																			//	deUint32					bufferRowLength;
				0u,																			//	deUint32					bufferImageHeight;
				makeImageSubresourceLayers(aspect, 0u, 0u, imageCreateInfo.arrayLayers),	//	VkImageSubresourceLayers	imageSubresource;
				makeOffset3D(0, 0, 0),														//	VkOffset3D					imageOffset;
				vk::getPlaneExtent(formatDescription, imageCreateInfo.extent, planeNdx, 0)	//	VkExtent3D					imageExtent;
			};
		}
		deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *imageSparse, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outputBuffer, static_cast<deUint32>(bufferImageCopy.size()), bufferImageCopy.data());

		{
			const VkBufferMemoryBarrier outputBufferHostReadBarrier = makeBufferMemoryBarrier
			(
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_HOST_READ_BIT,
				*outputBuffer,
				0u,
				imageSizeInBytes
			);

			deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferHostReadBarrier, 0u, DE_NULL);
		}

		// End recording commands
		endCommandBuffer(deviceInterface, *commandBuffer);

		// The stage at which execution is going to wait for finish of sparse binding operations
		const VkPipelineStageFlags stageBits[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

		// Submit commands for execution and wait for completion
		submitCommandsAndWait(deviceInterface, getDevice(), computeQueue.queueHandle, *commandBuffer, 1u, &imageMemoryBindSemaphore.get(), stageBits,
			0, DE_NULL, m_useDeviceGroups, firstDeviceID);

		// Retrieve data from buffer to host memory
		invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);
		deUint8*	outputData	= static_cast<deUint8*>(outputBufferAlloc->getHostPtr());
		void*		planePointers[PlanarFormatDescription::MAX_PLANES];

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
			planePointers[planeNdx] = outputData + static_cast<size_t>(planeOffsets[planeNdx]);

		// Wait for sparse queue to become idle
		//vsk fails:
		deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

		// write result images to log file
		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			if (!formatDescription.hasChannelNdx(channelNdx))
				continue;
			deUint32					planeNdx					= formatDescription.channels[channelNdx].planeNdx;
			vk::VkFormat				planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
			vk::PlanarFormatDescription	compatibleFormatDescription	= (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;
			const tcu::UVec3			compatibleShaderGridSize	(shaderGridSize.x() / formatDescription.blockWidth, shaderGridSize.y() / formatDescription.blockHeight, shaderGridSize.z() / 1u);
			tcu::ConstPixelBufferAccess	pixelBuffer					= vk::getChannelAccess(compatibleFormatDescription, compatibleShaderGridSize, planeRowPitches, (const void* const*)planePointers, channelNdx);
			std::ostringstream str;
			str << "image" << channelNdx;
			m_context.getTestContext().getLog() << tcu::LogImage(str.str(), str.str(), pixelBuffer);;
		}

		// Validate results
		for (deUint32 channelNdx = 0; channelNdx < 4; ++channelNdx)
		{
			if (!formatDescription.hasChannelNdx(channelNdx))
				continue;

			deUint32						planeNdx					= formatDescription.channels[channelNdx].planeNdx;
			const VkImageAspectFlags		aspect						= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
			const deUint32					aspectIndex					= getSparseAspectRequirementsIndex(sparseMemoryRequirements, aspect);

			if (aspectIndex == NO_MATCH_FOUND)
				TCU_THROW(NotSupportedError, "Not supported image aspect");

			VkSparseImageMemoryRequirements	aspectRequirements			= sparseMemoryRequirements[aspectIndex];

			vk::VkFormat					planeCompatibleFormat		= getPlaneCompatibleFormatForWriting(formatDescription, planeNdx);
			vk::PlanarFormatDescription		compatibleFormatDescription	= (planeCompatibleFormat != getPlaneCompatibleFormat(formatDescription, planeNdx)) ? getPlanarFormatDescription(planeCompatibleFormat) : formatDescription;
			const tcu::UVec3				compatibleShaderGridSize	( shaderGridSize.x() / formatDescription.blockWidth, shaderGridSize.y() / formatDescription.blockHeight, shaderGridSize.z() / 1u );
			VkExtent3D						compatibleImageSize			{ imageCreateInfo.extent.width / formatDescription.blockWidth, imageCreateInfo.extent.height / formatDescription.blockHeight, imageCreateInfo.extent.depth / 1u };
			VkExtent3D						compatibleImageGranularity	{ aspectRequirements.formatProperties.imageGranularity.width / formatDescription.blockWidth,
																		  aspectRequirements.formatProperties.imageGranularity.height / formatDescription.blockHeight,
																		  aspectRequirements.formatProperties.imageGranularity.depth / 1u };
			tcu::ConstPixelBufferAccess		pixelBuffer					= vk::getChannelAccess(compatibleFormatDescription, compatibleShaderGridSize, planeRowPitches, (const void* const*)planePointers, channelNdx);
			VkExtent3D						planeExtent					= getPlaneExtent(compatibleFormatDescription, compatibleImageSize, planeNdx, 0u);
			tcu::IVec3						pixelDivider				= pixelBuffer.getDivider();
			float							fixedPointError				= tcu::TexVerifierUtil::computeFixedPointError(formatDescription.channels[channelNdx].sizeBits);

			if( aspectRequirements.imageMipTailFirstLod > 0u )
			{
				const tcu::UVec3					numSparseBinds	= alignedDivide(planeExtent, compatibleImageGranularity);
				const tcu::UVec3					lastBlockExtent	= tcu::UVec3(planeExtent.width  % compatibleImageGranularity.width  ? planeExtent.width  % compatibleImageGranularity.width  : compatibleImageGranularity.width,
																				 planeExtent.height % compatibleImageGranularity.height ? planeExtent.height % compatibleImageGranularity.height : compatibleImageGranularity.height,
																				 planeExtent.depth  % compatibleImageGranularity.depth  ? planeExtent.depth  % compatibleImageGranularity.depth  : compatibleImageGranularity.depth);

				for (deUint32 layerNdx = 0; layerNdx < imageCreateInfo.arrayLayers; ++layerNdx)
				{
					for (deUint32 z = 0; z < numSparseBinds.z(); ++z)
					for (deUint32 y = 0; y < numSparseBinds.y(); ++y)
					for (deUint32 x = 0; x < numSparseBinds.x(); ++x)
					{
						VkExtent3D offset;
						offset.width	= x * compatibleImageGranularity.width;
						offset.height	= y * compatibleImageGranularity.height;
						offset.depth	= z * compatibleImageGranularity.depth + layerNdx * numSparseBinds.z()*compatibleImageGranularity.depth;

						VkExtent3D extent;
						extent.width	= (x == numSparseBinds.x() - 1) ? lastBlockExtent.x() : compatibleImageGranularity.width;
						extent.height	= (y == numSparseBinds.y() - 1) ? lastBlockExtent.y() : compatibleImageGranularity.height;
						extent.depth	= (z == numSparseBinds.z() - 1) ? lastBlockExtent.z() : compatibleImageGranularity.depth;

						const deUint32 linearIndex = x + y * numSparseBinds.x() + z * numSparseBinds.x() * numSparseBinds.y() + layerNdx * numSparseBinds.x() * numSparseBinds.y() * numSparseBinds.z();

						if (linearIndex % 2u == 0u)
						{
							for (deUint32 offsetZ = offset.depth; offsetZ < offset.depth + extent.depth; ++offsetZ)
							for (deUint32 offsetY = offset.height; offsetY < offset.height + extent.height; ++offsetY)
							for (deUint32 offsetX = offset.width; offsetX < offset.width + extent.width; ++offsetX)
							{
								deUint32	iReferenceValue;
								float		fReferenceValue;

								switch (channelNdx)
								{
									case 0:
										iReferenceValue = offsetX % 127u;
										fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
										break;
									case 1:
										iReferenceValue = offsetY % 127u;
										fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
										break;
									case 2:
										iReferenceValue = offsetZ % 127u;
										fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
										break;
									case 3:
										iReferenceValue = 1u;
										fReferenceValue = 1.f;
										break;
									default:	DE_FATAL("Unexpected channel index");	break;
								}

								float acceptableError = epsilon;

								switch (formatDescription.channels[channelNdx].type)
								{
									case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
									case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
									{
										const tcu::UVec4 outputValue = pixelBuffer.getPixelUint(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

										if (outputValue.x() != iReferenceValue)
											return tcu::TestStatus::fail("Failed");

										break;
									}
									case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
									case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
									{
										acceptableError += fixedPointError;
										const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

										if (deAbs(outputValue.x() - fReferenceValue) > acceptableError)
											return tcu::TestStatus::fail("Failed");

										break;
									}
									case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
									{
										const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

										if (deAbs( outputValue.x() - fReferenceValue) > acceptableError)
											return tcu::TestStatus::fail("Failed");

										break;
									}
									default:	DE_FATAL("Unexpected channel type");	break;
								}
							}
						}
						else if (physicalDeviceProperties.sparseProperties.residencyNonResidentStrict)
						{
							for (deUint32 offsetZ = offset.depth; offsetZ < offset.depth + extent.depth; ++offsetZ)
							for (deUint32 offsetY = offset.height; offsetY < offset.height + extent.height; ++offsetY)
							for (deUint32 offsetX = offset.width; offsetX < offset.width + extent.width; ++offsetX)
							{
								float acceptableError = epsilon;

								switch (formatDescription.channels[channelNdx].type)
								{
									case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
									case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
									{
										const tcu::UVec4 outputValue = pixelBuffer.getPixelUint(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

										if (outputValue.x() != 0u)
											return tcu::TestStatus::fail("Failed");

										break;
									}
									case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
									case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
									{
										acceptableError += fixedPointError;
										const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

										if (deAbs(outputValue.x()) > acceptableError)
											return tcu::TestStatus::fail("Failed");

										break;
									}
									case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
									{
										const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

										if (deAbs(outputValue.x()) > acceptableError)
											return tcu::TestStatus::fail("Failed");

										break;
									}
									default:	DE_FATAL("Unexpected channel type");	break;
								}
							}
						}
					}
				}
			}
			else
			{
				for (deUint32 offsetZ = 0u; offsetZ < planeExtent.depth * imageCreateInfo.arrayLayers; ++offsetZ)
				for (deUint32 offsetY = 0u; offsetY < planeExtent.height; ++offsetY)
				for (deUint32 offsetX = 0u; offsetX < planeExtent.width; ++offsetX)
				{
					deUint32	iReferenceValue;
					float		fReferenceValue;
					switch (channelNdx)
					{
						case 0:
							iReferenceValue = offsetX % 127u;
							fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
							break;
						case 1:
							iReferenceValue = offsetY % 127u;
							fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
							break;
						case 2:
							iReferenceValue = offsetZ % 127u;
							fReferenceValue = static_cast<float>(iReferenceValue) / 127.f;
							break;
						case 3:
							iReferenceValue = 1u;
							fReferenceValue = 1.f;
							break;
						default:	DE_FATAL("Unexpected channel index");	break;
					}
					float acceptableError = epsilon;

					switch (formatDescription.channels[channelNdx].type)
					{
						case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
						case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
						{
							const tcu::UVec4 outputValue = pixelBuffer.getPixelUint(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

							if (outputValue.x() != iReferenceValue)
								return tcu::TestStatus::fail("Failed");

							break;
						}
						case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
						case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
						{
							acceptableError += fixedPointError;
							const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

							if (deAbs(outputValue.x() - fReferenceValue) > acceptableError)
								return tcu::TestStatus::fail("Failed");

							break;
						}
						case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
						{
							const tcu::Vec4 outputValue = pixelBuffer.getPixel(offsetX * pixelDivider.x(), offsetY * pixelDivider.y(), offsetZ * pixelDivider.z());

							if (deAbs( outputValue.x() - fReferenceValue) > acceptableError)
								return tcu::TestStatus::fail("Failed");

							break;
						}
						default:	DE_FATAL("Unexpected channel type");	break;
					}
				}
			}
		}
	}

	return tcu::TestStatus::pass("Passed");
}

TestInstance* ImageSparseResidencyCase::createInstance (Context& context) const
{
	return new ImageSparseResidencyInstance(context, m_imageType, m_imageSize, m_format, m_useDeviceGroups);
}

} // anonymous ns

tcu::TestCaseGroup* createImageSparseResidencyTestsCommon (tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup> testGroup, const bool useDeviceGroup = false)
{
	const std::vector<TestImageParameters> imageParameters =
	{
		{ IMAGE_TYPE_2D,			{ tcu::UVec3(512u, 256u,  1u),	tcu::UVec3(1024u, 128u, 1u),	tcu::UVec3(11u,  137u, 1u) },	getTestFormats(IMAGE_TYPE_2D) },
		{ IMAGE_TYPE_2D_ARRAY,		{ tcu::UVec3(512u, 256u,  6u),	tcu::UVec3(1024u, 128u, 8u),	tcu::UVec3(11u,  137u, 3u) },	getTestFormats(IMAGE_TYPE_2D_ARRAY) },
		{ IMAGE_TYPE_CUBE,			{ tcu::UVec3(256u, 256u,  1u),	tcu::UVec3(128u,  128u, 1u),	tcu::UVec3(137u, 137u, 1u) },	getTestFormats(IMAGE_TYPE_CUBE) },
		{ IMAGE_TYPE_CUBE_ARRAY,	{ tcu::UVec3(256u, 256u,  6u),	tcu::UVec3(128u,  128u, 8u),	tcu::UVec3(137u, 137u, 3u) },	getTestFormats(IMAGE_TYPE_CUBE_ARRAY) },
		{ IMAGE_TYPE_3D,			{ tcu::UVec3(512u, 256u, 16u),	tcu::UVec3(1024u, 128u, 8u),	tcu::UVec3(11u,  137u, 3u) },	getTestFormats(IMAGE_TYPE_3D) }
	};

	for (size_t imageTypeNdx = 0; imageTypeNdx < imageParameters.size(); ++imageTypeNdx)
	{
		const ImageType					imageType = imageParameters[imageTypeNdx].imageType;
		de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str(), ""));

		for (size_t formatNdx = 0; formatNdx < imageParameters[imageTypeNdx].formats.size(); ++formatNdx)
		{
			const VkFormat					format				= imageParameters[imageTypeNdx].formats[formatNdx].format;
			tcu::UVec3						imageSizeAlignment	= getImageSizeAlignment(format);
			de::MovePtr<tcu::TestCaseGroup> formatGroup			(new tcu::TestCaseGroup(testCtx, getImageFormatID(format).c_str(), ""));

			for (size_t imageSizeNdx = 0; imageSizeNdx < imageParameters[imageTypeNdx].imageSizes.size(); ++imageSizeNdx)
			{
				const tcu::UVec3 imageSize = imageParameters[imageTypeNdx].imageSizes[imageSizeNdx];

				// skip test for images with odd sizes for some YCbCr formats
				if ((imageSize.x() % imageSizeAlignment.x()) != 0)
					continue;
				if ((imageSize.y() % imageSizeAlignment.y()) != 0)
					continue;

				std::ostringstream stream;
				stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

				formatGroup->addChild(new ImageSparseResidencyCase(testCtx, stream.str(), "", imageType, imageSize, format, glu::GLSL_VERSION_440, useDeviceGroup));
			}
			imageTypeGroup->addChild(formatGroup.release());
		}
		testGroup->addChild(imageTypeGroup.release());
	}

	return testGroup.release();
}

tcu::TestCaseGroup* createImageSparseResidencyTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_sparse_residency", "Image Sparse Residency"));
	return createImageSparseResidencyTestsCommon(testCtx, testGroup);
}

tcu::TestCaseGroup* createDeviceGroupImageSparseResidencyTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "device_group_image_sparse_residency", "Image Sparse Residency"));
	return createImageSparseResidencyTestsCommon(testCtx, testGroup, true);
}

} // sparse
} // vkt
