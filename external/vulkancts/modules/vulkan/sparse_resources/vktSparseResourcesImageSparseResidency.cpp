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
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

const std::string getCoordStr  (const ImageType		imageType,
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

deUint32 getNumUsedChannels (const tcu::TextureFormat& format)
{
	switch (format.order)
	{
		case tcu::TextureFormat::R:			return 1;
		case tcu::TextureFormat::A:			return 1;
		case tcu::TextureFormat::I:			return 1;
		case tcu::TextureFormat::L:			return 1;
		case tcu::TextureFormat::LA:		return 2;
		case tcu::TextureFormat::RG:		return 2;
		case tcu::TextureFormat::RA:		return 2;
		case tcu::TextureFormat::RGB:		return 3;
		case tcu::TextureFormat::RGBA:		return 4;
		case tcu::TextureFormat::ARGB:		return 4;
		case tcu::TextureFormat::BGR:		return 3;
		case tcu::TextureFormat::BGRA:		return 4;
		case tcu::TextureFormat::sR:		return 1;
		case tcu::TextureFormat::sRG:		return 2;
		case tcu::TextureFormat::sRGB:		return 3;
		case tcu::TextureFormat::sRGBA:		return 4;
		case tcu::TextureFormat::sBGR:		return 3;
		case tcu::TextureFormat::sBGRA:		return 4;
		case tcu::TextureFormat::D:			return 1;
		case tcu::TextureFormat::S:			return 1;
		case tcu::TextureFormat::DS:		return 2;
		default:
			DE_ASSERT(DE_FALSE);
			return 0;
	}
}

tcu::UVec3 alignedDivide (const VkExtent3D& extent, const VkExtent3D& divisor)
{
	tcu::UVec3 result;

	result.x() = extent.width  / divisor.width  + ((extent.width  % divisor.width)  ? 1u : 0u);
	result.y() = extent.height / divisor.height + ((extent.height % divisor.height) ? 1u : 0u);
	result.z() = extent.depth  / divisor.depth  + ((extent.depth  % divisor.depth)  ? 1u : 0u);

	return result;
}

tcu::UVec3 computeWorkGroupSize (const tcu::UVec3& gridSize)
{
	const deUint32		maxComputeWorkGroupInvocations	= 128u;
	const tcu::UVec3	maxComputeWorkGroupSize			= tcu::UVec3(128u, 128u, 64u);

	const deUint32 xWorkGroupSize = std::min(std::min(gridSize.x(), maxComputeWorkGroupSize.x()), maxComputeWorkGroupInvocations);
	const deUint32 yWorkGroupSize = std::min(std::min(gridSize.y(), maxComputeWorkGroupSize.y()), maxComputeWorkGroupInvocations /  xWorkGroupSize);
	const deUint32 zWorkGroupSize = std::min(std::min(gridSize.z(), maxComputeWorkGroupSize.z()), maxComputeWorkGroupInvocations / (xWorkGroupSize*yWorkGroupSize));

	return tcu::UVec3(xWorkGroupSize, yWorkGroupSize, zWorkGroupSize);
}

class ImageSparseResidencyCase : public TestCase
{
public:
					ImageSparseResidencyCase	(tcu::TestContext&			testCtx,
												 const std::string&			name,
												 const std::string&			description,
												 const ImageType			imageType,
												 const tcu::UVec3&			imageSize,
												 const tcu::TextureFormat&	format,
												 const glu::GLSLVersion		glslVersion);

	void			initPrograms				(SourceCollections&			sourceCollections) const;
	TestInstance*	createInstance				(Context&					context) const;

private:
	const ImageType				m_imageType;
	const tcu::UVec3			m_imageSize;
	const tcu::TextureFormat	m_format;
	const glu::GLSLVersion		m_glslVersion;
};

ImageSparseResidencyCase::ImageSparseResidencyCase (tcu::TestContext&			testCtx,
													const std::string&			name,
													const std::string&			description,
													const ImageType				imageType,
													const tcu::UVec3&			imageSize,
													const tcu::TextureFormat&	format,
													const glu::GLSLVersion		glslVersion)
	: TestCase				(testCtx, name, description)
	, m_imageType			(imageType)
	, m_imageSize			(imageSize)
	, m_format				(format)
	, m_glslVersion			(glslVersion)
{
}


void ImageSparseResidencyCase::initPrograms (SourceCollections&	sourceCollections) const
{
	// Create compute program
	const char* const versionDecl			= glu::getGLSLVersionDeclaration(m_glslVersion);
	const std::string imageTypeStr			= getShaderImageType(m_format, m_imageType);
	const std::string formatQualifierStr	= getShaderImageFormatQualifier(m_format);
	const std::string formatDataStr			= getShaderImageDataType(m_format);
	const tcu::UVec3  gridSize				= getShaderGridSize(m_imageType, m_imageSize);
	const tcu::UVec3  workGroupSize			= computeWorkGroupSize(gridSize);

	std::ostringstream src;
	src << versionDecl << "\n"
		<< "layout (local_size_x = " << workGroupSize.x() << ", local_size_y = " << workGroupSize.y() << ", local_size_z = " << workGroupSize.z() << ") in; \n"
		<< "layout (binding = 0, " << formatQualifierStr << ") writeonly uniform highp " << imageTypeStr << " u_image;\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	if( gl_GlobalInvocationID.x < " << gridSize.x() << " ) \n"
		<< "	if( gl_GlobalInvocationID.y < " << gridSize.y() << " ) \n"
		<< "	if( gl_GlobalInvocationID.z < " << gridSize.z() << " ) \n"
		<< "	{\n"
		<< "		imageStore(u_image, " << getCoordStr(m_imageType, "gl_GlobalInvocationID.x", "gl_GlobalInvocationID.y", "gl_GlobalInvocationID.z") << ","
		<< formatDataStr << "( int(gl_GlobalInvocationID.x) % 127, int(gl_GlobalInvocationID.y) % 127, int(gl_GlobalInvocationID.z) % 127, 1));\n"
		<< "	}\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

class ImageSparseResidencyInstance : public SparseResourcesBaseInstance
{
public:
					ImageSparseResidencyInstance(Context&									 context,
												 const ImageType							 imageType,
												 const tcu::UVec3&							 imageSize,
												 const tcu::TextureFormat&					 format);

	tcu::TestStatus	iterate						(void);

private:
	const ImageType				m_imageType;
	const tcu::UVec3			m_imageSize;
	const tcu::TextureFormat	m_format;
};

ImageSparseResidencyInstance::ImageSparseResidencyInstance (Context&					context,
															const ImageType				imageType,
															const tcu::UVec3&			imageSize,
															const tcu::TextureFormat&	format)
	: SparseResourcesBaseInstance	(context)
	, m_imageType					(imageType)
	, m_imageSize					(imageSize)
	, m_format						(format)
{
}

tcu::TestStatus ImageSparseResidencyInstance::iterate (void)
{
	const InstanceInterface&			 instance		 = m_context.getInstanceInterface();
	const DeviceInterface&				 deviceInterface = m_context.getDeviceInterface();
	const VkPhysicalDevice				 physicalDevice	 = m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures		 deviceFeatures	 = getPhysicalDeviceFeatures(instance, physicalDevice);
	
	switch (mapImageType(m_imageType))
	{
		case VK_IMAGE_TYPE_2D:
		{
			if (deviceFeatures.sparseResidencyImage2D == false)
				return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Sparse residency for 2D Image not supported");
		}
		break;
		case VK_IMAGE_TYPE_3D:
		{
			if (deviceFeatures.sparseResidencyImage3D == false)
				return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Sparse residency for 3D Image not supported");

		}
		break;
		default:
			return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported image type");
	};

	// Check if the image format supports sparse operations
	const std::vector<VkSparseImageFormatProperties> sparseImageFormatPropVec =
		getPhysicalDeviceSparseImageFormatProperties(instance, physicalDevice, mapTextureFormat(m_format), mapImageType(m_imageType),
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_TILING_OPTIMAL);

	if (sparseImageFormatPropVec.size() == 0)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "The image format does not support sparse operations");
	}

	const VkPhysicalDeviceProperties deviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);

	if (isImageSizeSupported(m_imageType, m_imageSize, deviceProperties.limits) == false)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Image size not supported for device");
	}

	QueueRequirementsVec queueRequirements;
	queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
	queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

	// Create logical device supporting both sparse and compute queues
	if (!createDeviceSupportingQueues(queueRequirements))
	{
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Could not create device supporting sparse and compute queue");
	}

	const VkPhysicalDeviceMemoryProperties deviceMemoryProperties = getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	// Create memory allocator for logical device
	const de::UniquePtr<Allocator> allocator(new SimpleAllocator(deviceInterface, *m_logicalDevice, deviceMemoryProperties));

	// Create queue supporting sparse binding operations
	const Queue& sparseQueue = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);

	// Create queue supporting compute and transfer operations
	const Queue& computeQueue = getQueue(VK_QUEUE_COMPUTE_BIT, 0);

	VkImageCreateInfo imageCreateInfo;

	imageCreateInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;					//VkStructureType		sType;
	imageCreateInfo.pNext					= DE_NULL;												//const void*			pNext;
	imageCreateInfo.flags					= VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;					//VkImageCreateFlags	flags;
	imageCreateInfo.imageType				= mapImageType(m_imageType);							//VkImageType			imageType;
	imageCreateInfo.format					= mapTextureFormat(m_format);							//VkFormat				format;
	imageCreateInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageSize));	//VkExtent3D			extent;
	imageCreateInfo.mipLevels				= 1u;													//deUint32				mipLevels;
	imageCreateInfo.arrayLayers				= getNumLayers(m_imageType, m_imageSize);				//deUint32				arrayLayers;
	imageCreateInfo.samples					= VK_SAMPLE_COUNT_1_BIT;								//VkSampleCountFlagBits	samples;
	imageCreateInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;								//VkImageTiling			tiling;
	imageCreateInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;							//VkImageLayout			initialLayout;
	imageCreateInfo.usage					= VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
											  VK_IMAGE_USAGE_STORAGE_BIT;							//VkImageUsageFlags		usage;
	imageCreateInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;							//VkSharingMode			sharingMode;
	imageCreateInfo.queueFamilyIndexCount	= 0u;													//deUint32				queueFamilyIndexCount;
	imageCreateInfo.pQueueFamilyIndices		= DE_NULL;												//const deUint32*		pQueueFamilyIndices;

	if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
	{
		imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	const deUint32 queueFamilyIndices[] = { sparseQueue.queueFamilyIndex, computeQueue.queueFamilyIndex };

	if (sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex)
	{
		imageCreateInfo.sharingMode				= VK_SHARING_MODE_CONCURRENT;	//VkSharingMode			sharingMode;
		imageCreateInfo.queueFamilyIndexCount	= 2u;							//deUint32				queueFamilyIndexCount;
		imageCreateInfo.pQueueFamilyIndices		= queueFamilyIndices;			//const deUint32*		pQueueFamilyIndices;
	}

	// Create sparse image
	const Unique<VkImage> sparseImage(createImage(deviceInterface, *m_logicalDevice, &imageCreateInfo));

	// Get image general memory requirements
	const VkMemoryRequirements imageMemoryRequirements = getImageMemoryRequirements(deviceInterface, *m_logicalDevice, *sparseImage);

	if (imageMemoryRequirements.size > deviceProperties.limits.sparseAddressSpaceSize)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Required memory size for sparse resource exceeds device limits");
	}
	
	DE_ASSERT((imageMemoryRequirements.size % imageMemoryRequirements.alignment) == 0);

	// Get image sparse memory requirements
	deUint32 sparseMemoryReqCount = 0;

	deviceInterface.getImageSparseMemoryRequirements(*m_logicalDevice, *sparseImage, &sparseMemoryReqCount, DE_NULL);

	DE_ASSERT(sparseMemoryReqCount != 0);

	std::vector<VkSparseImageMemoryRequirements> sparseImageMemoryRequirements;
	sparseImageMemoryRequirements.resize(sparseMemoryReqCount);

	deviceInterface.getImageSparseMemoryRequirements(*m_logicalDevice, *sparseImage, &sparseMemoryReqCount, &sparseImageMemoryRequirements[0]);

	// Make sure the image type includes color aspect
	deUint32 colorAspectIndex = NO_MATCH_FOUND;

	for (deUint32 memoryReqNdx = 0; memoryReqNdx < sparseMemoryReqCount; ++memoryReqNdx)
	{
		if (sparseImageMemoryRequirements[memoryReqNdx].formatProperties.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
		{
			colorAspectIndex = memoryReqNdx;
			break;
		}
	}

	if (colorAspectIndex == NO_MATCH_FOUND)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported image aspect - the test supports currently only VK_IMAGE_ASPECT_COLOR_BIT");
	}

	const VkSparseImageMemoryRequirements aspectRequirements	= sparseImageMemoryRequirements[colorAspectIndex];
	const VkImageAspectFlags			  aspectMask			= aspectRequirements.formatProperties.aspectMask;
	const VkExtent3D					  imageGranularity		= aspectRequirements.formatProperties.imageGranularity;

	DE_ASSERT((aspectRequirements.imageMipTailSize % imageMemoryRequirements.alignment) == 0);

	typedef de::SharedPtr< Unique<VkDeviceMemory> > DeviceMemoryUniquePtr;

	std::vector<VkSparseImageMemoryBind> imageResidencyMemoryBinds;
	std::vector<VkSparseMemoryBind>		 imageMipTailMemoryBinds;
	std::vector<DeviceMemoryUniquePtr>	 deviceMemUniquePtrVec;
	const deUint32						 memoryType = findMatchingMemoryType(deviceMemoryProperties, imageMemoryRequirements, MemoryRequirement::Any);

	if (memoryType == NO_MATCH_FOUND)
	{
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "No matching memory type found");
	}

	// Bind device memory for each aspect
	for (deUint32 layerNdx = 0; layerNdx < imageCreateInfo.arrayLayers; ++layerNdx)
	{
		for (deUint32 mipLevelNdx = 0; mipLevelNdx < aspectRequirements.imageMipTailFirstLod; ++mipLevelNdx)
		{
			const VkExtent3D		 mipExtent			= mipLevelExtents(imageCreateInfo.extent, mipLevelNdx);
			const tcu::UVec3		 numSparseBinds		= alignedDivide(mipExtent, imageGranularity);
			const tcu::UVec3		 lastBlockExtent	= tcu::UVec3(mipExtent.width  % imageGranularity.width  ? mipExtent.width  % imageGranularity.width  : imageGranularity.width,
																	 mipExtent.height % imageGranularity.height ? mipExtent.height % imageGranularity.height : imageGranularity.height,
																	 mipExtent.depth  % imageGranularity.depth  ? mipExtent.depth  % imageGranularity.depth  : imageGranularity.depth );

			for (deUint32 z = 0; z < numSparseBinds.z(); ++z)
			for (deUint32 y = 0; y < numSparseBinds.y(); ++y)
			for (deUint32 x = 0; x < numSparseBinds.x(); ++x)
			{
				const deUint32 linearIndex = x + y*numSparseBinds.x() + z*numSparseBinds.x()*numSparseBinds.y() + layerNdx*numSparseBinds.x()*numSparseBinds.y()*numSparseBinds.z();

				if (linearIndex % 2 == 1)
				{
					continue;
				}

				const VkMemoryAllocateInfo allocInfo =
				{
					VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType			sType;
					DE_NULL,								//	const void*				pNext;
					imageMemoryRequirements.alignment,		//	VkDeviceSize			allocationSize;
					memoryType,								//	deUint32				memoryTypeIndex;
				};

				VkDeviceMemory deviceMemory = 0;
				VK_CHECK(deviceInterface.allocateMemory(*m_logicalDevice, &allocInfo, DE_NULL, &deviceMemory));

				deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(deviceMemory), Deleter<VkDeviceMemory>(deviceInterface, *m_logicalDevice, DE_NULL))));

				VkOffset3D offset;
				offset.x = x*imageGranularity.width;
				offset.y = y*imageGranularity.height;
				offset.z = z*imageGranularity.depth;

				VkExtent3D extent;
				extent.width =  (x == numSparseBinds.x() - 1) ? lastBlockExtent.x() : imageGranularity.width;
				extent.height = (y == numSparseBinds.y() - 1) ? lastBlockExtent.y() : imageGranularity.height;
				extent.depth =  (z == numSparseBinds.z() - 1) ? lastBlockExtent.z() : imageGranularity.depth;

				VkSparseImageMemoryBind imageMemoryBind;
				imageMemoryBind.subresource.aspectMask	= aspectMask;
				imageMemoryBind.subresource.mipLevel	= mipLevelNdx;
				imageMemoryBind.subresource.arrayLayer	= layerNdx;
				imageMemoryBind.memory					= deviceMemory;
				imageMemoryBind.memoryOffset			= 0u;
				imageMemoryBind.flags					= 0u;
				imageMemoryBind.offset					= offset;
				imageMemoryBind.extent					= extent;

				imageResidencyMemoryBinds.push_back(imageMemoryBind);
			}
		}

		if (!(aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) && aspectRequirements.imageMipTailFirstLod < imageCreateInfo.mipLevels)
		{
			const VkMemoryAllocateInfo allocInfo =
			{
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType	sType;
				DE_NULL,								//	const void*		pNext;
				aspectRequirements.imageMipTailSize,	//	VkDeviceSize	allocationSize;
				memoryType,								//	deUint32		memoryTypeIndex;
			};

			VkDeviceMemory deviceMemory = 0;
			VK_CHECK(deviceInterface.allocateMemory(*m_logicalDevice, &allocInfo, DE_NULL, &deviceMemory));

			deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(deviceMemory), Deleter<VkDeviceMemory>(deviceInterface, *m_logicalDevice, DE_NULL))));

			VkSparseMemoryBind imageMipTailMemoryBind;

			imageMipTailMemoryBind.resourceOffset	= aspectRequirements.imageMipTailOffset + layerNdx * aspectRequirements.imageMipTailStride;
			imageMipTailMemoryBind.size				= aspectRequirements.imageMipTailSize;
			imageMipTailMemoryBind.memory			= deviceMemory;
			imageMipTailMemoryBind.memoryOffset		= 0u;
			imageMipTailMemoryBind.flags			= 0u;

			imageMipTailMemoryBinds.push_back(imageMipTailMemoryBind);
		}
	}

	if ((aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) && aspectRequirements.imageMipTailFirstLod < imageCreateInfo.mipLevels)
	{
		const VkMemoryAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType	sType;
			DE_NULL,								//	const void*		pNext;
			aspectRequirements.imageMipTailSize,	//	VkDeviceSize	allocationSize;
			memoryType,								//	deUint32		memoryTypeIndex;
		};

		VkDeviceMemory deviceMemory = 0;
		VK_CHECK(deviceInterface.allocateMemory(*m_logicalDevice, &allocInfo, DE_NULL, &deviceMemory));

		deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(deviceMemory), Deleter<VkDeviceMemory>(deviceInterface, *m_logicalDevice, DE_NULL))));

		VkSparseMemoryBind imageMipTailMemoryBind;

		imageMipTailMemoryBind.resourceOffset	= aspectRequirements.imageMipTailOffset;
		imageMipTailMemoryBind.size				= aspectRequirements.imageMipTailSize;
		imageMipTailMemoryBind.memory			= deviceMemory;
		imageMipTailMemoryBind.memoryOffset		= 0u;
		imageMipTailMemoryBind.flags			= 0u;

		imageMipTailMemoryBinds.push_back(imageMipTailMemoryBind);
	}

	const Unique<VkSemaphore> imageMemoryBindSemaphore(makeSemaphore(deviceInterface, *m_logicalDevice));

	VkBindSparseInfo bindSparseInfo =
	{
		VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,			//VkStructureType							sType;
		DE_NULL,									//const void*								pNext;
		0u,											//deUint32									waitSemaphoreCount;
		DE_NULL,									//const VkSemaphore*						pWaitSemaphores;
		0u,											//deUint32									bufferBindCount;
		DE_NULL,									//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
		0u,											//deUint32									imageOpaqueBindCount;
		DE_NULL,									//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
		0u,											//deUint32									imageBindCount;
		DE_NULL,									//const VkSparseImageMemoryBindInfo*		pImageBinds;
		1u,											//deUint32									signalSemaphoreCount;
		&imageMemoryBindSemaphore.get()				//const VkSemaphore*						pSignalSemaphores;
	};

	VkSparseImageMemoryBindInfo		  imageResidencyBindInfo;
	VkSparseImageOpaqueMemoryBindInfo imageMipTailBindInfo;

	if (imageResidencyMemoryBinds.size() > 0)
	{
		imageResidencyBindInfo.image	 = *sparseImage;
		imageResidencyBindInfo.bindCount = static_cast<deUint32>(imageResidencyMemoryBinds.size());
		imageResidencyBindInfo.pBinds    = &imageResidencyMemoryBinds[0];

		bindSparseInfo.imageBindCount	 = 1u;
		bindSparseInfo.pImageBinds		 = &imageResidencyBindInfo;
	}

	if (imageMipTailMemoryBinds.size() > 0)
	{
		imageMipTailBindInfo.image			= *sparseImage;
		imageMipTailBindInfo.bindCount		= static_cast<deUint32>(imageMipTailMemoryBinds.size());
		imageMipTailBindInfo.pBinds			= &imageMipTailMemoryBinds[0];

		bindSparseInfo.imageOpaqueBindCount = 1u;
		bindSparseInfo.pImageOpaqueBinds	= &imageMipTailBindInfo;
	}

	// Submit sparse bind commands for execution
	VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));

	// Create command buffer for compute and transfer oparations
	const Unique<VkCommandPool>	  commandPool(makeCommandPool(deviceInterface, *m_logicalDevice, computeQueue.queueFamilyIndex));
	const Unique<VkCommandBuffer> commandBuffer(makeCommandBuffer(deviceInterface, *m_logicalDevice, *commandPool));

	// Start recording commands
	beginCommandBuffer(deviceInterface, *commandBuffer);

	// Create descriptor set layout
	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(deviceInterface, *m_logicalDevice));

	// Create and bind compute pipeline
	const Unique<VkShaderModule>	shaderModule(createShaderModule(deviceInterface, *m_logicalDevice, m_context.getBinaryCollection().get("comp"), DE_NULL));
	const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(deviceInterface, *m_logicalDevice, *descriptorSetLayout));
	const Unique<VkPipeline>		computePipeline(makeComputePipeline(deviceInterface, *m_logicalDevice, *pipelineLayout, *shaderModule));

	deviceInterface.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

	// Create and bind descriptor set
	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
		.build(deviceInterface, *m_logicalDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet>	descriptorSet(makeDescriptorSet(deviceInterface, *m_logicalDevice, *descriptorPool, *descriptorSetLayout));

	const VkImageSubresourceRange	subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, getNumLayers(m_imageType, m_imageSize));
	const Unique<VkImageView>		imageView(makeImageView(deviceInterface, *m_logicalDevice, *sparseImage, mapImageViewType(m_imageType), mapTextureFormat(m_format), subresourceRange));
	const VkDescriptorImageInfo		sparseImageInfo  = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &sparseImageInfo)
		.update(deviceInterface, *m_logicalDevice);

	deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	const VkImageMemoryBarrier sparseImageLayoutChangeBarrier
		= makeImageMemoryBarrier(
		0u, VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		*sparseImage, subresourceRange);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &sparseImageLayoutChangeBarrier);

	const tcu::UVec3  gridSize		= getShaderGridSize(m_imageType, m_imageSize);
	const tcu::UVec3  workGroupSize = computeWorkGroupSize(gridSize);

	const deUint32 xWorkGroupCount = gridSize.x() / workGroupSize.x() + (gridSize.x() % workGroupSize.x() ? 1u : 0u);
	const deUint32 yWorkGroupCount = gridSize.y() / workGroupSize.y() + (gridSize.y() % workGroupSize.y() ? 1u : 0u);
	const deUint32 zWorkGroupCount = gridSize.z() / workGroupSize.z() + (gridSize.z() % workGroupSize.z() ? 1u : 0u);

	const tcu::UVec3 maxComputeWorkGroupCount = tcu::UVec3(65535u, 65535u, 65535u);

	if (maxComputeWorkGroupCount.x() < xWorkGroupCount ||
		maxComputeWorkGroupCount.y() < yWorkGroupCount ||
		maxComputeWorkGroupCount.z() < zWorkGroupCount)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Image size is not supported");
	}
	
	deviceInterface.cmdDispatch(*commandBuffer, xWorkGroupCount, yWorkGroupCount, zWorkGroupCount);

	const VkImageMemoryBarrier sparseImageTrasferBarrier
		= makeImageMemoryBarrier(
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		*sparseImage, subresourceRange);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &sparseImageTrasferBarrier);

	const deUint32			 imageSizeInBytes		= getNumPixels(m_imageType, m_imageSize) * tcu::getPixelSize(m_format);
	const VkBufferCreateInfo outputBufferCreateInfo = makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	const de::UniquePtr<Buffer>	outputBuffer(new Buffer(deviceInterface, *m_logicalDevice, *allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkBufferImageCopy bufferImageCopy = makeBufferImageCopy(imageCreateInfo.extent, imageCreateInfo.arrayLayers);

	deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *sparseImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outputBuffer->get(), 1u, &bufferImageCopy);

	const VkBufferMemoryBarrier outputBufferHostReadBarrier 
		= makeBufferMemoryBarrier(
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
		outputBuffer->get(), 0u, imageSizeInBytes);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferHostReadBarrier, 0u, DE_NULL);

	// End recording commands
	endCommandBuffer(deviceInterface, *commandBuffer);

	// The stage at which execution is going to wait for finish of sparse binding operations
	const VkPipelineStageFlags stageBits[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

	// Submit commands for execution and wait for completion
	submitCommandsAndWait(deviceInterface, *m_logicalDevice, computeQueue.queueHandle, *commandBuffer, 1u, &imageMemoryBindSemaphore.get(), stageBits);

	// Retrieve data from buffer to host memory
	const Allocation& allocation = outputBuffer->getAllocation();

	invalidateMappedMemoryRange(deviceInterface, *m_logicalDevice, allocation.getMemory(), allocation.getOffset(), imageSizeInBytes);

	const deUint8*	outputData = static_cast<const deUint8*>(allocation.getHostPtr());
	tcu::TestStatus testStatus = tcu::TestStatus::pass("Passed");

	const tcu::ConstPixelBufferAccess pixelBuffer = tcu::ConstPixelBufferAccess(m_format, gridSize.x(), gridSize.y(), gridSize.z(), outputData);

	// Validate results
	if( aspectRequirements.imageMipTailFirstLod > 0u )
	{
		const VkExtent3D		 mipExtent		 = mipLevelExtents(imageCreateInfo.extent, 0u);
		const tcu::UVec3		 numSparseBinds  = alignedDivide(mipExtent, imageGranularity);
		const tcu::UVec3		 lastBlockExtent = tcu::UVec3(	mipExtent.width  % imageGranularity.width  ? mipExtent.width  % imageGranularity.width  : imageGranularity.width,
																mipExtent.height % imageGranularity.height ? mipExtent.height % imageGranularity.height : imageGranularity.height,
																mipExtent.depth  % imageGranularity.depth  ? mipExtent.depth  % imageGranularity.depth  : imageGranularity.depth);

		for (deUint32 layerNdx = 0; layerNdx < imageCreateInfo.arrayLayers; ++layerNdx)
		{
			for (deUint32 z = 0; z < numSparseBinds.z(); ++z)
			for (deUint32 y = 0; y < numSparseBinds.y(); ++y)
			for (deUint32 x = 0; x < numSparseBinds.x(); ++x)
			{
				VkExtent3D offset;
				offset.width  = x*imageGranularity.width;
				offset.height = y*imageGranularity.height;
				offset.depth  = z*imageGranularity.depth + layerNdx*numSparseBinds.z()*imageGranularity.depth;

				VkExtent3D extent;
				extent.width  = (x == numSparseBinds.x() - 1) ? lastBlockExtent.x() : imageGranularity.width;
				extent.height = (y == numSparseBinds.y() - 1) ? lastBlockExtent.y() : imageGranularity.height;
				extent.depth  = (z == numSparseBinds.z() - 1) ? lastBlockExtent.z() : imageGranularity.depth;

				const deUint32 linearIndex = x + y*numSparseBinds.x() + z*numSparseBinds.x()*numSparseBinds.y() + layerNdx*numSparseBinds.x()*numSparseBinds.y()*numSparseBinds.z();

				if (linearIndex % 2 == 0)
				{
					for (deUint32 offsetZ = offset.depth;  offsetZ < offset.depth  + extent.depth;  ++offsetZ)
					for (deUint32 offsetY = offset.height; offsetY < offset.height + extent.height; ++offsetY)
					for (deUint32 offsetX = offset.width;  offsetX < offset.width  + extent.width;  ++offsetX)
					{
						const tcu::UVec4 referenceValue = tcu::UVec4(offsetX % 127u, offsetY % 127u, offsetZ % 127u, 1u);
						const tcu::UVec4 outputValue	= pixelBuffer.getPixelUint(offsetX, offsetY, offsetZ);

						if (memcmp(&outputValue, &referenceValue, sizeof(deUint32) * getNumUsedChannels(m_format)))
						{
							testStatus = tcu::TestStatus::fail("Failed");
							goto verificationFinished;
						}
					}
				}
				else
				{
					if (deviceProperties.sparseProperties.residencyNonResidentStrict)
					{
						for (deUint32 offsetZ = offset.depth;  offsetZ < offset.depth  + extent.depth;  ++offsetZ)
						for (deUint32 offsetY = offset.height; offsetY < offset.height + extent.height; ++offsetY)
						for (deUint32 offsetX = offset.width;  offsetX < offset.width  + extent.width;  ++offsetX)
						{
							const tcu::UVec4 referenceValue = tcu::UVec4(0u, 0u, 0u, 0u);
							const tcu::UVec4 outputValue = pixelBuffer.getPixelUint(offsetX, offsetY, offsetZ);

							if (memcmp(&outputValue, &referenceValue, sizeof(deUint32) * getNumUsedChannels(m_format)))
							{
								testStatus = tcu::TestStatus::fail("Failed");
								goto verificationFinished;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		const VkExtent3D mipExtent = mipLevelExtents(imageCreateInfo.extent, 0u);

		for (deUint32 offsetZ = 0u; offsetZ < mipExtent.depth * imageCreateInfo.arrayLayers; ++offsetZ)
		for (deUint32 offsetY = 0u; offsetY < mipExtent.height; ++offsetY)
		for (deUint32 offsetX = 0u; offsetX < mipExtent.width;  ++offsetX)
		{
			const tcu::UVec4 referenceValue = tcu::UVec4(offsetX % 127u, offsetY % 127u, offsetZ % 127u, 1u);
			const tcu::UVec4 outputValue	= pixelBuffer.getPixelUint(offsetX, offsetY, offsetZ);

			if (memcmp(&outputValue, &referenceValue, sizeof(deUint32) * getNumUsedChannels(m_format)))
			{
				testStatus = tcu::TestStatus::fail("Failed");
				goto verificationFinished;
			}
		}
	}

	verificationFinished:

	// Wait for sparse queue to become idle
	deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

	return testStatus;
}

TestInstance* ImageSparseResidencyCase::createInstance (Context& context) const
{
	return new ImageSparseResidencyInstance(context, m_imageType, m_imageSize, m_format);
}

} // anonymous ns

tcu::TestCaseGroup* createImageSparseResidencyTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_sparse_residency", "Buffer Sparse Residency"));

	static const deUint32 sizeCountPerImageType = 3u;

	struct ImageParameters
	{
		ImageType	imageType;
		tcu::UVec3	imageSizes[sizeCountPerImageType];
	};

	static const ImageParameters imageParametersArray[] =
	{
		{ IMAGE_TYPE_2D,		 { tcu::UVec3(512u, 256u, 1u),  tcu::UVec3(1024u, 128u, 1u), tcu::UVec3(11u, 137u, 1u) } },
		{ IMAGE_TYPE_2D_ARRAY,	 { tcu::UVec3(512u, 256u, 6u),	tcu::UVec3(1024u, 128u, 8u), tcu::UVec3(11u, 137u, 3u) } },
		{ IMAGE_TYPE_CUBE,		 { tcu::UVec3(512u, 256u, 1u),	tcu::UVec3(1024u, 128u, 1u), tcu::UVec3(11u, 137u, 1u) } },
		{ IMAGE_TYPE_CUBE_ARRAY, { tcu::UVec3(512u, 256u, 6u),	tcu::UVec3(1024u, 128u, 8u), tcu::UVec3(11u, 137u, 3u) } },
		{ IMAGE_TYPE_3D,		 { tcu::UVec3(512u, 256u, 16u), tcu::UVec3(1024u, 128u, 8u), tcu::UVec3(11u, 137u, 3u) } }
	};

	static const tcu::TextureFormat formats[] =
	{
		tcu::TextureFormat(tcu::TextureFormat::R,	 tcu::TextureFormat::SIGNED_INT32),
		tcu::TextureFormat(tcu::TextureFormat::R,	 tcu::TextureFormat::SIGNED_INT16),
		tcu::TextureFormat(tcu::TextureFormat::R,	 tcu::TextureFormat::SIGNED_INT8),
		tcu::TextureFormat(tcu::TextureFormat::RG,	 tcu::TextureFormat::SIGNED_INT32),
		tcu::TextureFormat(tcu::TextureFormat::RG,   tcu::TextureFormat::SIGNED_INT16),
		tcu::TextureFormat(tcu::TextureFormat::RG,   tcu::TextureFormat::SIGNED_INT8),
		tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT32),
		tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT16),
		tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT8)
	};

	for (deInt32 imageTypeNdx = 0; imageTypeNdx < DE_LENGTH_OF_ARRAY(imageParametersArray); ++imageTypeNdx)
	{
		const ImageType					imageType = imageParametersArray[imageTypeNdx].imageType;
		de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str(), ""));

		for (deInt32 formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); ++formatNdx)
		{
			const tcu::TextureFormat&		format = formats[formatNdx];
			de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, getShaderImageFormatQualifier(format).c_str(), ""));

			for (deInt32 imageSizeNdx = 0; imageSizeNdx < DE_LENGTH_OF_ARRAY(imageParametersArray[imageTypeNdx].imageSizes); ++imageSizeNdx)
			{
				const tcu::UVec3 imageSize = imageParametersArray[imageTypeNdx].imageSizes[imageSizeNdx];

				std::ostringstream stream;
				stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

				formatGroup->addChild(new ImageSparseResidencyCase(testCtx, stream.str(), "", imageType, imageSize, format, glu::GLSL_VERSION_440));
			}
			imageTypeGroup->addChild(formatGroup.release());
		}
		testGroup->addChild(imageTypeGroup.release());
	}

	return testGroup.release();
}

} // sparse
} // vkt
