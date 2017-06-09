/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \file  vktImageCompressionTranscodingSupport.cpp
 * \brief Compression transcoding support
 *//*--------------------------------------------------------------------*/

#include "vktImageCompressionTranscodingSupport.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"

#include "vktTestCaseUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuVectorType.hpp"
#include "tcuResource.hpp"
#include "tcuImageIO.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"

#include <vector>
#include <iomanip>

using namespace vk;
namespace vkt
{
namespace image
{
namespace
{
using std::string;
using std::vector;
using tcu::TestContext;
using tcu::TestStatus;
using tcu::UVec3;
using tcu::CompressedTexFormat;
using tcu::CompressedTexture;
using tcu::Resource;
using tcu::Archive;
using de::MovePtr;
using de::SharedPtr;
using de::Random;

typedef SharedPtr<MovePtr<Image> >		ImageSp;
typedef SharedPtr<Move<VkImageView> >	ImageViewSp;

enum ShaderType
{
	SHADER_TYPE_COMPUTE,
	SHADER_TYPE_FRAGMENT,
	SHADER_TYPE_LAST
};

enum Operation
{
	OPERATION_IMAGE_LOAD,
	OPERATION_TEXEL_FETCH,
	OPERATION_TEXTURE,
	OPERATION_IMAGE_STORE,
	OPERATION_ATTACHMENT_READ,
	OPERATION_ATTACHMENT_WRITE,
	OPERATION_TEXTURE_READ,
	OPERATION_TEXTURE_WRITE,
	OPERATION_LAST
};

struct TestParameters
{
	Operation			operation;
	ShaderType			shader;
	UVec3				size;
	ImageType			imageType;
	VkFormat			formatCompressed;
	VkFormat			formatUncompressed;
	deUint32			imagesCount;
	VkImageUsageFlags	compressedImageUsage;
	VkImageUsageFlags	uncompressedImageUsage;
	bool				useMipmaps;
	VkFormat			formatForVerify;
};

template<typename T>
inline SharedPtr<Move<T> > makeVkSharedPtr (Move<T> move)
{
	return SharedPtr<Move<T> >(new vk::Move<T>(move));
}

template<typename T>
inline SharedPtr<MovePtr<T> > makeVkSharedPtr (MovePtr<T> movePtr)
{
	return SharedPtr<MovePtr<T> >(new MovePtr<T>(movePtr));
}

class BasicTranscodingTestInstance : public TestInstance
{
public:
							BasicTranscodingTestInstance	(Context&				contex,
															 const TestParameters&	parameters);
	virtual TestStatus		iterate							(void) = 0;
protected:
	void					generateData					(deUint8*				toFill,
															 size_t					size,
															 const VkFormat			format = VK_FORMAT_UNDEFINED);
	const TestParameters	m_parameters;
};

BasicTranscodingTestInstance::BasicTranscodingTestInstance (Context& context, const TestParameters& parameters)
	: TestInstance	(context)
	, m_parameters	(parameters)
{
}

void BasicTranscodingTestInstance::generateData (deUint8* toFill, size_t size, const VkFormat format)
{
	const deUint8 pattern[] =
	{
		// 64-bit values
		0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
		0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// Positive infinity
		0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// Negative infinity
		0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,		// Start of a signalling NaN (NANS)
		0x7F, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		// End of a signalling NaN (NANS)
		0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,		// Start of a signalling NaN (NANS)
		0xFF, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		// End of a signalling NaN (NANS)
		0x7F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// Start of a quiet NaN (NANQ)
		0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		// End of of a quiet NaN (NANQ)
		0xFF, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// Start of a quiet NaN (NANQ)
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		// End of a quiet NaN (NANQ)
		// 32-bit values
		0x7F, 0x80, 0x00, 0x00,								// Positive infinity
		0xFF, 0x80, 0x00, 0x00,								// Negative infinity
		0x7F, 0x80, 0x00, 0x01,								// Start of a signalling NaN (NANS)
		0x7F, 0xBF, 0xFF, 0xFF,								// End of a signalling NaN (NANS)
		0xFF, 0x80, 0x00, 0x01,								// Start of a signalling NaN (NANS)
		0xFF, 0xBF, 0xFF, 0xFF,								// End of a signalling NaN (NANS)
		0x7F, 0xC0, 0x00, 0x00,								// Start of a quiet NaN (NANQ)
		0x7F, 0xFF, 0xFF, 0xFF,								// End of of a quiet NaN (NANQ)
		0xFF, 0xC0, 0x00, 0x00,								// Start of a quiet NaN (NANQ)
		0xFF, 0xFF, 0xFF, 0xFF,								// End of a quiet NaN (NANQ)
		0xAA, 0xAA, 0xAA, 0xAA,
		0x55, 0x55, 0x55, 0x55,
	};

	deUint8*	start		= toFill;
	size_t		sizeToRnd	= size;

	// Pattern part
	if (size >= 2 * sizeof(pattern))
	{
		// Rotated pattern
		for (size_t i = 0; i < sizeof(pattern); i++)
			start[sizeof(pattern) - i - 1] = pattern[i];

		start		+= sizeof(pattern);
		sizeToRnd	-= sizeof(pattern);

		// Direct pattern
		deMemcpy(start, pattern, sizeof(pattern));

		start		+= sizeof(pattern);
		sizeToRnd	-= sizeof(pattern);
	}

	// Random part
	{
		DE_ASSERT(sizeToRnd % sizeof(deUint32) == 0);

		deUint32*	start32		= reinterpret_cast<deUint32*>(start);
		size_t		sizeToRnd32	= sizeToRnd / sizeof(deUint32);
		Random		rnd			(static_cast<deUint32>(format));

		for (size_t i = 0; i < sizeToRnd32; i++)
			start32[i] = rnd.getUint32();
	}

	{
		// Remove certain values that may not be preserved based on the uncompressed view format
		if (isSnormFormat(m_parameters.formatUncompressed))
		{
			for (size_t i = 0; i < size; i += 2)
			{
				// SNORM fix: due to write operation in SNORM format
				// replaces 0x00 0x80 to 0x01 0x80
				if (toFill[i] == 0x00 && toFill[i+1] == 0x80)
					toFill[i+1] = 0x81;
			}
		}
		else if (isFloatFormat(m_parameters.formatUncompressed))
		{
			tcu::TextureFormat textureFormat = mapVkFormat(m_parameters.formatUncompressed);

			if (textureFormat.type == tcu::TextureFormat::HALF_FLOAT)
			{
				for (size_t i = 0; i < size; i += 2)
				{
					// HALF_FLOAT fix: remove INF and NaN
					if ((toFill[i+1] & 0x7C) == 0x7C)
						toFill[i+1] = 0x00;
				}
			}
			else if (textureFormat.type == tcu::TextureFormat::FLOAT)
			{
				for (size_t i = 0; i < size; i += 4)
				{
					// HALF_FLOAT fix: remove INF and NaN
					if ((toFill[i+1] & 0x7C) == 0x7C)
						toFill[i+1] = 0x00;
				}

				for (size_t i = 0; i < size; i += 4)
				{
					// FLOAT fix: remove INF, NaN, and denorm
					// Little endian fix
					if (((toFill[i+3] & 0x7F) == 0x7F && (toFill[i+2] & 0x80) == 0x80) || ((toFill[i+3] & 0x7F) == 0x00 && (toFill[i+2] & 0x80) == 0x00))
						toFill[i+3] = 0x01;
					// Big endian fix
					if (((toFill[i+0] & 0x7F) == 0x7F && (toFill[i+1] & 0x80) == 0x80) || ((toFill[i+0] & 0x7F) == 0x00 && (toFill[i+1] & 0x80) == 0x00))
						toFill[i+0] = 0x01;
				}
			}
		}
	}
}

class BasicComputeTestInstance : public BasicTranscodingTestInstance
{
public:
					BasicComputeTestInstance	(Context&							contex,
												const TestParameters&				parameters);
	TestStatus		iterate						(void);
protected:
	void			copyDataToImage				(const VkCommandBuffer&				cmdBuffer,
												 const VkImage&						compressed,
												 const VkImageCreateInfo&			imageInfo);
	virtual void	executeShader				(const VkCommandBuffer&				cmdBuffer,
												 const VkDescriptorSetLayout&		descriptorSetLayout,
												 const VkDescriptorPool&			descriptorPool,
												 const vector<ImageSp>&				images,
												 const vector<ImageViewSp>&			imageViews);
	bool			copyResultAndCompare		(const VkCommandBuffer&				cmdBuffer,
												 const VkImage&						uncompressed);
	void			descriptorSetUpdate			(VkDescriptorSet					descriptorSet,
												 const VkDescriptorImageInfo*		descriptorImageInfos);
	void			createImageInfos			(VkImageCreateInfo*					imageInfos);
	bool			decompressImage				(const VkCommandBuffer&				cmdBuffer,
												 const VkImage&						uncompressed,
												 const VkImage&						compressed,
												 const VkExtent3D&					extentunCompressed);
	vector<deUint8>	m_data;
};

BasicComputeTestInstance::BasicComputeTestInstance (Context& context, const TestParameters& parameters)
	:BasicTranscodingTestInstance	(context, parameters)
	,m_data							(static_cast<size_t>(getCompressedImageSizeInBytes(parameters.formatCompressed, parameters.size)))
{
	generateData (&m_data[0], m_data.size(), m_parameters.formatCompressed);
}

TestStatus BasicComputeTestInstance::iterate (void)
{
	const DeviceInterface&					vk					= m_context.getDeviceInterface();
	const VkDevice							device				= m_context.getDevice();
	const deUint32							queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&								allocator			= m_context.getDefaultAllocator();

	Move<VkDescriptorSetLayout>				descriptorSetLayout;
	Move<VkDescriptorPool>					descriptorPool;

	vector<ImageSp>							images;
	vector<VkImageCreateInfo>				imagesInfo			(m_parameters.imagesCount);
	createImageInfos(&imagesInfo[0]);
	vector<ImageViewSp>						imageViews			(m_parameters.imagesCount);
	images.resize(m_parameters.imagesCount);
	const deUint32							compressedNdx		= 0u;
	const deUint32							uncompressedNdx		= m_parameters.imagesCount - 1u;

	{
		DescriptorSetLayoutBuilder	descriptorSetLayoutBuilder;
		DescriptorPoolBuilder		descriptorPoolBuilder;

		for (deUint32 imageNdx = 0; imageNdx < m_parameters.imagesCount; ++imageNdx)
		{
			images[imageNdx] = makeVkSharedPtr(MovePtr<Image>(new Image(vk, device, allocator, imagesInfo[imageNdx], MemoryRequirement::Any)));
			if (compressedNdx == imageNdx)
			{
				const VkImageViewUsageCreateInfoKHR	imageViewUsageCreateInfoKHR	=
				{
					VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO_KHR,	//VkStructureType		sType;
					DE_NULL,											//const void*			pNext;
					m_parameters.compressedImageUsage,					//VkImageUsageFlags		usage;
				};
				imageViews[imageNdx] = makeVkSharedPtr(makeImageView(vk, device, **images[imageNdx]->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatUncompressed,
						makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imagesInfo[imageNdx].extent.depth, 0u, imagesInfo[imageNdx].arrayLayers), &imageViewUsageCreateInfoKHR));
			}
			else
			{
				imageViews[imageNdx] = makeVkSharedPtr(makeImageView(vk, device, **images[imageNdx]->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatUncompressed,
										makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imagesInfo[imageNdx].extent.depth, 0u, imagesInfo[imageNdx].arrayLayers)));
			}
			switch(m_parameters.operation)
			{
				case OPERATION_IMAGE_LOAD:
				case OPERATION_IMAGE_STORE:
					descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
					descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imagesInfo[imageNdx].arrayLayers);
					break;
				case OPERATION_TEXEL_FETCH:
				case OPERATION_TEXTURE:
					descriptorSetLayoutBuilder.addSingleBinding((compressedNdx == imageNdx) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
					descriptorPoolBuilder.addType((compressedNdx == imageNdx) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imagesInfo[imageNdx].arrayLayers);
					break;
				default:
					DE_ASSERT(false);
					break;
			}
		}
		descriptorSetLayout	= descriptorSetLayoutBuilder.build(vk, device);
		descriptorPool		= descriptorPoolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, imagesInfo[0].arrayLayers);
	}

	const Unique<VkCommandPool>				cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>			cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	switch(m_parameters.operation)
	{
		case OPERATION_IMAGE_LOAD:
		case OPERATION_TEXEL_FETCH:
		case OPERATION_TEXTURE:
			copyDataToImage(*cmdBuffer, **images[compressedNdx]->get(), imagesInfo[compressedNdx]);
			break;
		case OPERATION_IMAGE_STORE:
			copyDataToImage(*cmdBuffer, **images[1]->get(), imagesInfo[1]);
			break;
		default:
			DE_ASSERT(false);
			break;
	}
	executeShader(*cmdBuffer, *descriptorSetLayout, *descriptorPool, images, imageViews);

	if (copyResultAndCompare(*cmdBuffer, **images[uncompressedNdx]->get()) &&
		decompressImage(*cmdBuffer, **images[uncompressedNdx]->get(), **images[compressedNdx]->get(), imagesInfo[uncompressedNdx].extent))
	{
		return TestStatus::pass("Pass");
	}
	return TestStatus::fail("Fail");
}

void BasicComputeTestInstance::copyDataToImage (const VkCommandBuffer& cmdBuffer, const VkImage& compressed, const VkImageCreateInfo& imageInfo)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				device		= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();
	Allocator&					allocator	= m_context.getDefaultAllocator();

	Buffer						imageBuffer	(vk, device, allocator,
												makeBufferCreateInfo(m_data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
												MemoryRequirement::HostVisible);
	{
		const Allocation& alloc = imageBuffer.getAllocation();
		deMemcpy(alloc.getHostPtr(), &m_data[0], m_data.size());
		flushMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), m_data.size());
	}

	beginCommandBuffer(vk, cmdBuffer);
	{
		const VkImageSubresourceRange	subresourceRange		=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,											//VkImageAspectFlags	aspectMask
			0u,																	//deUint32				baseMipLevel
			1u,																	//deUint32				levelCount
			0u,																	//deUint32				baseArrayLayer
			1																	//deUint32				layerCount
		};

		const VkImageMemoryBarrier		preCopyImageBarrier		= makeImageMemoryBarrier(
																	0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																	VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																	compressed, subresourceRange);

		const VkBufferMemoryBarrier		FlushHostCopyBarrier	= makeBufferMemoryBarrier(
																	VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																	imageBuffer.get(), 0ull, m_data.size());

		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &FlushHostCopyBarrier, 1u, &preCopyImageBarrier);

		const VkBufferImageCopy			copyRegion				=
		{
			0ull,																//	VkDeviceSize				bufferOffset;
			0u,																	//	deUint32					bufferRowLength;
			0u,																	//	deUint32					bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	//	VkImageSubresourceLayers	imageSubresource;
			makeOffset3D(0, 0, 0),												//	VkOffset3D					imageOffset;
			imageInfo.extent,													//	VkExtent3D					imageExtent;
		};

		vk.cmdCopyBufferToImage(cmdBuffer, imageBuffer.get(), compressed, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	}
	endCommandBuffer(vk, cmdBuffer);
	submitCommandsAndWait(vk, device, queue, cmdBuffer);
}

void BasicComputeTestInstance::executeShader (const VkCommandBuffer&		cmdBuffer,
											  const VkDescriptorSetLayout&	descriptorSetLayout,
											  const VkDescriptorPool&		descriptorPool,
											  const vector<ImageSp>&		images,
											  const vector<ImageViewSp>&	imageViews)
{
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const Unique<VkShaderModule>	shaderModule			(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0));
	Move<VkDescriptorSet>			descriptorSet			= makeDescriptorSet(vk, device, descriptorPool, descriptorSetLayout);
	const Unique<VkPipelineLayout>	pipelineLayout			(makePipelineLayout(vk, device, descriptorSetLayout));
	const Unique<VkPipeline>		pipeline				(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));
	const UVec3						extentUncompressed		= getCompressedImageResolutionInBlocks(m_parameters.formatCompressed, m_parameters.size);
	Move<VkSampler>					sampler;
	{
		const vk::VkSamplerCreateInfo createInfo =
		{
			vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,	//VkStructureType		sType;
			DE_NULL,									//const void*			pNext;
			0u,											//VkSamplerCreateFlags	flags;
			VK_FILTER_NEAREST,							//VkFilter				magFilter;
			VK_FILTER_NEAREST,							//VkFilter				minFilter;
			VK_SAMPLER_MIPMAP_MODE_NEAREST,				//VkSamplerMipmapMode	mipmapMode;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//VkSamplerAddressMode	addressModeU;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//VkSamplerAddressMode	addressModeV;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//VkSamplerAddressMode	addressModeW;
			0.0f,										//float					mipLodBias;
			VK_FALSE,									//VkBool32				anisotropyEnable;
			1.0f,										//float					maxAnisotropy;
			VK_TRUE,									//VkBool32				compareEnable;
			VK_COMPARE_OP_EQUAL,						//VkCompareOp			compareOp;
			0.0f,										//float					minLod;
			0.0f,										//float					maxLod;
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	//VkBorderColor			borderColor;
			VK_TRUE,									//VkBool32				unnormalizedCoordinates;
		};
		sampler = vk::createSampler(vk, device, &createInfo);
	}

	vector<VkDescriptorImageInfo>	descriptorImageInfos	(m_parameters.imagesCount);
	for (deUint32 bindingNdx = 0; bindingNdx < m_parameters.imagesCount; ++bindingNdx)
		descriptorImageInfos[bindingNdx] = makeDescriptorImageInfo(*sampler, **imageViews[bindingNdx], VK_IMAGE_LAYOUT_GENERAL);

	beginCommandBuffer(vk, cmdBuffer);
	{
		const VkImageSubresourceRange	subresourceRange			=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,					//VkImageAspectFlags	aspectMask
			0u,											//deUint32				baseMipLevel
			1u,											//deUint32				levelCount
			0u,											//deUint32				baseArrayLayer
			1u											//deUint32				layerCount
		};

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

		const VkImageMemoryBarrier		preShaderImageBarriers[]	=
		{
			makeImageMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
				**images[0]->get(), subresourceRange),

			makeImageMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				**images[1]->get(), subresourceRange)
		};

		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL,
			DE_LENGTH_OF_ARRAY(preShaderImageBarriers), preShaderImageBarriers);

		descriptorSetUpdate (*descriptorSet, &descriptorImageInfos[0]);

		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdDispatch(cmdBuffer, extentUncompressed.x(), extentUncompressed.y(), extentUncompressed.z());
	}
	endCommandBuffer(vk, cmdBuffer);
	submitCommandsAndWait(vk, device, queue, cmdBuffer);
}

bool BasicComputeTestInstance::copyResultAndCompare (const VkCommandBuffer& cmdBuffer, const VkImage& uncompressed)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const VkDevice			device				= m_context.getDevice();
	Allocator&				allocator			= m_context.getDefaultAllocator();
	const UVec3				extentUncompressed	= getCompressedImageResolutionInBlocks(m_parameters.formatCompressed, m_parameters.size);

	vk::VkDeviceSize		imageResultSize		= getImageSizeBytes (tcu::IVec3(extentUncompressed.x(), extentUncompressed.y(), extentUncompressed.z()), m_parameters.formatUncompressed);
	Buffer					imageBufferResult	(vk, device, allocator,
													makeBufferCreateInfo(imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
													MemoryRequirement::HostVisible);

	beginCommandBuffer(vk, cmdBuffer);
	{
		const VkImageSubresourceRange	subresourceRange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,											//VkImageAspectFlags	aspectMask
			0u,																	//deUint32				baseMipLevel
			1u,																	//deUint32				levelCount
			0u,																	//deUint32				baseArrayLayer
			1u																	//deUint32				layerCount
		};

		const VkBufferImageCopy			copyRegion			=
		{
			0ull,																//	VkDeviceSize				bufferOffset;
			0u,																	//	deUint32					bufferRowLength;
			0u,																	//	deUint32					bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	//	VkImageSubresourceLayers	imageSubresource;
			makeOffset3D(0, 0, 0),												//	VkOffset3D					imageOffset;
			makeExtent3D(extentUncompressed),									//	VkExtent3D					imageExtent;
		};

		const VkImageMemoryBarrier prepareForTransferBarrier = makeImageMemoryBarrier(
																VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																uncompressed, subresourceRange);

		const VkBufferMemoryBarrier copyBarrier = makeBufferMemoryBarrier(
													VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
													imageBufferResult.get(), 0ull, imageResultSize);

		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &prepareForTransferBarrier);
		vk.cmdCopyImageToBuffer(cmdBuffer, uncompressed, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageBufferResult.get(), 1u, &copyRegion);
		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &copyBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	}
	endCommandBuffer(vk, cmdBuffer);
	submitCommandsAndWait(vk, device, queue, cmdBuffer);

	const Allocation& allocResult = imageBufferResult.getAllocation();
	invalidateMappedMemoryRange(vk, device, allocResult.getMemory(), allocResult.getOffset(), imageResultSize);

	if (deMemCmp(allocResult.getHostPtr(), &m_data[0], (size_t)imageResultSize) == 0)
		return true;
	return false;
}

void BasicComputeTestInstance::descriptorSetUpdate (VkDescriptorSet descriptorSet, const VkDescriptorImageInfo* descriptorImageInfos)
{
	const DeviceInterface&		vk		= m_context.getDeviceInterface();
	const VkDevice				device	= m_context.getDevice();
	DescriptorSetUpdateBuilder	descriptorSetUpdateBuilder;

	switch(m_parameters.operation)
	{
		case OPERATION_IMAGE_LOAD:
		case OPERATION_IMAGE_STORE:
		{
			for (deUint32 bindingNdx = 0; bindingNdx < m_parameters.imagesCount; ++bindingNdx)
				descriptorSetUpdateBuilder.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bindingNdx), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfos[bindingNdx]);

			break;
		}

		case OPERATION_TEXEL_FETCH:
		case OPERATION_TEXTURE:
		{
			for (deUint32 bindingNdx = 0; bindingNdx < m_parameters.imagesCount; ++bindingNdx)
			{
				descriptorSetUpdateBuilder.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bindingNdx),
					bindingNdx == 0 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfos[bindingNdx]);
			}
			break;
		}

		default:
			DE_ASSERT(false);
	}
	descriptorSetUpdateBuilder.update(vk, device);
}

void BasicComputeTestInstance::createImageInfos (VkImageCreateInfo* imageInfos)
{
	const VkExtent3D			extentUncompressed	= makeExtent3D(getCompressedImageResolutionInBlocks(m_parameters.formatCompressed, m_parameters.size));
	const VkExtent3D			extentCompressed	= makeExtent3D(getLayerSize(m_parameters.imageType, m_parameters.size));
	const deUint32				arrayLayers			= getNumLayers(m_parameters.imageType, m_parameters.size);
	const VkImageType			imageType			= mapImageType(m_parameters.imageType);

	const VkImageCreateInfo compressedInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
		DE_NULL,												// const void*				pNext;
		VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT |
		VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT_KHR |
		VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR,					// VkImageCreateFlags		flags;
		imageType,												// VkImageType				imageType;
		m_parameters.formatCompressed,							// VkFormat					format;
		extentCompressed,										// VkExtent3D				extent;
		1u,														// deUint32					mipLevels;
		arrayLayers,											// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT,						// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
		0u,														// deUint32					queueFamilyIndexCount;
		DE_NULL,												// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			initialLayout;
	};
	imageInfos[0] = compressedInfo;

	for (size_t ndx = 1; ndx < m_parameters.imagesCount; ++ndx)
	{
		const VkImageCreateInfo uncompressedInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,				// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageCreateFlags		flags;
			imageType,											// VkImageType				imageType;
			m_parameters.formatUncompressed,					// VkFormat					format;
			extentUncompressed,									// VkExtent3D				extent;
			1u,													// deUint32					mipLevels;
			arrayLayers,										// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,							// VkImageTiling			tiling;
			m_parameters.uncompressedImageUsage |
			VK_IMAGE_USAGE_SAMPLED_BIT,							// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,							// VkSharingMode			sharingMode;
			0u,													// deUint32					queueFamilyIndexCount;
			DE_NULL,											// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			initialLayout;
		};
		imageInfos[ndx] = uncompressedInfo;
	}
}

bool BasicComputeTestInstance::decompressImage (const VkCommandBuffer& cmdBuffer, const VkImage& uncompressed, const VkImage& compressed, const VkExtent3D& extentunCompressed)
{
	const DeviceInterface&				vk							= m_context.getDeviceInterface();
	const VkDevice						device						= m_context.getDevice();
	const VkQueue						queue						= m_context.getUniversalQueue();
	Allocator&							allocator					= m_context.getDefaultAllocator();
	const Unique<VkShaderModule>		shaderModule				(createShaderModule(vk, device, m_context.getBinaryCollection().get("decompress"), 0));
	const VkImageCreateInfo				decompressedImageInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,								// VkStructureType			sType;
		DE_NULL,															// const void*				pNext;
		0u,																	// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,													// VkImageType				imageType;
		VK_FORMAT_R8G8B8A8_UNORM,											// VkFormat					format;
		makeExtent3D(m_parameters.size),									// VkExtent3D				extent;
		1u,																	// deUint32					mipLevels;
		1u,																	// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,											// VkImageTiling			tiling;
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,											// VkSharingMode			sharingMode;
		0u,																	// deUint32					queueFamilyIndexCount;
		DE_NULL,															// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout			initialLayout;
	};
	const VkImageCreateInfo				compressedImageInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,								// VkStructureType			sType;
		DE_NULL,															// const void*				pNext;
		0u,																	// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,													// VkImageType				imageType;
		m_parameters.formatCompressed,										// VkFormat					format;
		makeExtent3D(m_parameters.size),									// VkExtent3D				extent;
		1u,																	// deUint32					mipLevels;
		1u,																	// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,											// VkImageTiling			tiling;
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,											// VkSharingMode			sharingMode;
		0u,																	// deUint32					queueFamilyIndexCount;
		DE_NULL,															// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout			initialLayout;
	};
	const VkImageUsageFlags				compressedViewUsageFlags	= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageViewUsageCreateInfoKHR	compressedViewUsageCI		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO_KHR,					//VkStructureType		sType;
		DE_NULL,															//const void*			pNext;
		compressedViewUsageFlags,											//VkImageUsageFlags		usage;
	};
	Image								resultImage					(vk, device, allocator, decompressedImageInfo, MemoryRequirement::Any);
	Image								referenceImage				(vk, device, allocator, decompressedImageInfo, MemoryRequirement::Any);
	Image								uncompressedImage			(vk, device, allocator, compressedImageInfo, MemoryRequirement::Any);
	Move<VkImageView>					resultView					= makeImageView(vk, device, resultImage.get(), mapImageViewType(m_parameters.imageType), decompressedImageInfo.format,
																		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, decompressedImageInfo.extent.depth, 0u, decompressedImageInfo.arrayLayers));
	Move<VkImageView>					referenceView				= makeImageView(vk, device, referenceImage.get(), mapImageViewType(m_parameters.imageType), decompressedImageInfo.format,
																		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, decompressedImageInfo.extent.depth, 0u, decompressedImageInfo.arrayLayers));
	Move<VkImageView>					uncompressedView			= makeImageView(vk, device, uncompressedImage.get(), mapImageViewType(m_parameters.imageType), m_parameters.formatCompressed,
																		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, compressedImageInfo.extent.depth, 0u, compressedImageInfo.arrayLayers));
	Move<VkImageView>					compressedView				= makeImageView(vk, device, compressed, mapImageViewType(m_parameters.imageType), m_parameters.formatCompressed,
																		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, compressedImageInfo.extent.depth, 0u, compressedImageInfo.arrayLayers), &compressedViewUsageCI);
	Move<VkDescriptorSetLayout>			descriptorSetLayout			= DescriptorSetLayoutBuilder()
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
																		.build(vk, device);
	Move<VkDescriptorPool>				descriptorPool				= DescriptorPoolBuilder()
																		.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, decompressedImageInfo.arrayLayers)
																		.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, decompressedImageInfo.arrayLayers)
																		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, decompressedImageInfo.arrayLayers)
																		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, decompressedImageInfo.arrayLayers)
																		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, decompressedImageInfo.arrayLayers);

	Move<VkDescriptorSet>				descriptorSet				= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	const Unique<VkPipelineLayout>		pipelineLayout				(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>			pipeline					(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));
	const VkDeviceSize					bufferSize					= getImageSizeBytes(tcu::IVec3((int)m_parameters.size.x(), (int)m_parameters.size.y(), (int)m_parameters.size.z()), VK_FORMAT_R8G8B8A8_UNORM);
	Buffer								resultBuffer				(vk, device, allocator,
																		makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);
	Buffer								referenceBuffer				(vk, device, allocator,
																		makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);
	Buffer								transferBuffer				(vk, device, allocator,
																		makeBufferCreateInfo(m_data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);
	Move<VkSampler>						sampler;
	{
		const vk::VkSamplerCreateInfo createInfo =
		{
			vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,						//VkStructureType		sType;
			DE_NULL,														//const void*			pNext;
			0u,																//VkSamplerCreateFlags	flags;
			VK_FILTER_NEAREST,												//VkFilter				magFilter;
			VK_FILTER_NEAREST,												//VkFilter				minFilter;
			VK_SAMPLER_MIPMAP_MODE_NEAREST,									//VkSamplerMipmapMode	mipmapMode;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							//VkSamplerAddressMode	addressModeU;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							//VkSamplerAddressMode	addressModeV;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,							//VkSamplerAddressMode	addressModeW;
			0.0f,															//float					mipLodBias;
			VK_FALSE,														//VkBool32				anisotropyEnable;
			1.0f,															//float					maxAnisotropy;
			VK_TRUE,														//VkBool32				compareEnable;
			VK_COMPARE_OP_EQUAL,											//VkCompareOp			compareOp;
			0.0f,															//float					minLod;
			0.0f,															//float					maxLod;
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,						//VkBorderColor			borderColor;
			VK_TRUE,														//VkBool32				unnormalizedCoordinates;
		};
		sampler = vk::createSampler(vk, device, &createInfo);
	}

	VkDescriptorImageInfo			descriptorImageInfos[]	=
	{
		makeDescriptorImageInfo(*sampler,	*uncompressedView,	VK_IMAGE_LAYOUT_GENERAL),
		makeDescriptorImageInfo(*sampler,	*compressedView,	VK_IMAGE_LAYOUT_GENERAL),
		makeDescriptorImageInfo(DE_NULL,	*resultView,		VK_IMAGE_LAYOUT_GENERAL),
		makeDescriptorImageInfo(DE_NULL,	*referenceView,		VK_IMAGE_LAYOUT_GENERAL)
	};
	DescriptorSetUpdateBuilder()
		.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfos[0])
		.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfos[1])
		.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfos[2])
		.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(3), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfos[3])
		.update(vk, device);


	beginCommandBuffer(vk, cmdBuffer);
	{
		const VkImageSubresourceRange	subresourceRange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,											//VkImageAspectFlags			aspectMask
			0u,																	//deUint32						baseMipLevel
			1u,																	//deUint32						levelCount
			0u,																	//deUint32						baseArrayLayer
			1																	//deUint32						layerCount
		};

		const VkBufferImageCopy			copyRegion			=
		{
			0ull,																//	VkDeviceSize				bufferOffset;
			0u,																	//	deUint32					bufferRowLength;
			0u,																	//	deUint32					bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	//	VkImageSubresourceLayers	imageSubresource;
			makeOffset3D(0, 0, 0),												//	VkOffset3D					imageOffset;
			decompressedImageInfo.extent,										//	VkExtent3D					imageExtent;
		};

		const VkBufferImageCopy			compressedCopyRegion			=
		{
			0ull,																//	VkDeviceSize				bufferOffset;
			0u,																	//	deUint32					bufferRowLength;
			0u,																	//	deUint32					bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	//	VkImageSubresourceLayers	imageSubresource;
			makeOffset3D(0, 0, 0),												//	VkOffset3D					imageOffset;
			extentunCompressed,													//	VkExtent3D					imageExtent;
		};

		{
			const VkBufferMemoryBarrier		preCopyBufferBarriers	= makeBufferMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																		transferBuffer.get(), 0ull, m_data.size());

			vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &preCopyBufferBarriers, 0u, (const VkImageMemoryBarrier*)DE_NULL);
		}

		vk.cmdCopyImageToBuffer(cmdBuffer, uncompressed, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transferBuffer.get(), 1u, &compressedCopyRegion);

		{
			const VkBufferMemoryBarrier		postCopyBufferBarriers	= makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																		transferBuffer.get(), 0ull, m_data.size());

			const VkImageMemoryBarrier		preCopyImageBarriers	= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uncompressedImage.get(), subresourceRange);

			vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &postCopyBufferBarriers, 1u, &preCopyImageBarriers);
		}

		vk.cmdCopyBufferToImage(cmdBuffer, transferBuffer.get(), uncompressedImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		{
			const VkImageMemoryBarrier		preShaderImageBarriers[]	=
			{
				makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
					uncompressedImage.get(), subresourceRange),

				makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
					resultImage.get(), subresourceRange),

				makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
					referenceImage.get(), subresourceRange)
			};

			vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL,
				DE_LENGTH_OF_ARRAY(preShaderImageBarriers), preShaderImageBarriers);
		}

		vk.cmdDispatch(cmdBuffer, m_parameters.size.x(), m_parameters.size.y(), m_parameters.size.z());

		{
			const VkImageMemoryBarrier		postShaderImageBarriers[]	=
			{
				makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				resultImage.get(), subresourceRange),

				makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					referenceImage.get(), subresourceRange)
			};

			 const VkBufferMemoryBarrier		preCopyBufferBarrier[]		=
			{
				makeBufferMemoryBarrier( 0, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					resultBuffer.get(), 0ull, bufferSize),

				makeBufferMemoryBarrier( 0, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					referenceBuffer.get(), 0ull, bufferSize),
			};

			vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(preCopyBufferBarrier), preCopyBufferBarrier,
				DE_LENGTH_OF_ARRAY(postShaderImageBarriers), postShaderImageBarriers);
		}
		vk.cmdCopyImageToBuffer(cmdBuffer, resultImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resultBuffer.get(), 1u, &copyRegion);
		vk.cmdCopyImageToBuffer(cmdBuffer, referenceImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, referenceBuffer.get(), 1u, &copyRegion);
	}
	endCommandBuffer(vk, cmdBuffer);
	submitCommandsAndWait(vk, device, queue, cmdBuffer);

	const Allocation&		resultAlloc		= resultBuffer.getAllocation();
	const Allocation&		referenceAlloc	= referenceBuffer.getAllocation();
	invalidateMappedMemoryRange(vk, device, resultAlloc.getMemory(), resultAlloc.getOffset(), bufferSize);
	invalidateMappedMemoryRange(vk, device, referenceAlloc.getMemory(), referenceAlloc.getOffset(), bufferSize);

	tcu::ConstPixelBufferAccess	resultPixels		(mapVkFormat(decompressedImageInfo.format), decompressedImageInfo.extent.width, decompressedImageInfo.extent.height, decompressedImageInfo.extent.depth, resultAlloc.getHostPtr());
	tcu::ConstPixelBufferAccess	referencePixels		(mapVkFormat(decompressedImageInfo.format), decompressedImageInfo.extent.width, decompressedImageInfo.extent.height, decompressedImageInfo.extent.depth, referenceAlloc.getHostPtr());

	return tcu::fuzzyCompare(m_context.getTestContext().getLog(), "ImageComparison", "Image Comparison", resultPixels, referencePixels, 0.001f, tcu::COMPARE_LOG_EVERYTHING);
}


class ImageStoreComputeTestInstance : public BasicComputeTestInstance
{
public:
					ImageStoreComputeTestInstance	(Context& contex, const TestParameters& parameters);
protected:
	virtual void	executeShader		(const VkCommandBuffer&			cmdBuffer,
										 const VkDescriptorSetLayout&	descriptorSetLayout,
										 const VkDescriptorPool&		descriptorPool,
										 const vector<ImageSp>&			images,
										 const vector<ImageViewSp>&		imageViews);
private:
};

ImageStoreComputeTestInstance::ImageStoreComputeTestInstance (Context& contex, const TestParameters& parameters)
	:BasicComputeTestInstance	(contex, parameters)
{
}

void ImageStoreComputeTestInstance::executeShader (const VkCommandBuffer&		cmdBuffer,
												   const VkDescriptorSetLayout&	descriptorSetLayout,
												   const VkDescriptorPool&		descriptorPool,
												   const vector<ImageSp>&		images,
												   const vector<ImageViewSp>&	imageViews)
{
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const Unique<VkShaderModule>	shaderModule			(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0));
	Move<VkDescriptorSet>			descriptorSet			= makeDescriptorSet(vk, device, descriptorPool, descriptorSetLayout);
	const Unique<VkPipelineLayout>	pipelineLayout			(makePipelineLayout(vk, device, descriptorSetLayout));
	const Unique<VkPipeline>		pipeline				(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));
	const UVec3						extentUncompressed		= getCompressedImageResolutionInBlocks(m_parameters.formatCompressed, m_parameters.size);

	vector<VkDescriptorImageInfo>	descriptorImageInfos	(m_parameters.imagesCount);
	for (deUint32 bindingNdx = 0; bindingNdx < m_parameters.imagesCount; ++bindingNdx)
		descriptorImageInfos[bindingNdx] = makeDescriptorImageInfo(DE_NULL, **imageViews[bindingNdx], VK_IMAGE_LAYOUT_GENERAL);

	beginCommandBuffer(vk, cmdBuffer);
	{
		const VkImageSubresourceRange	subresourceRange		=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,		//VkImageAspectFlags	aspectMask
			0u,								//deUint32				baseMipLevel
			1u,								//deUint32				levelCount
			0u,								//deUint32				baseArrayLayer
			1u								//deUint32				layerCount
		};

		const VkImageMemoryBarrier		preShaderImageBarriers[]	=
		{
			makeImageMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			**images[0]->get(), subresourceRange),

			makeImageMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			**images[1]->get(), subresourceRange),

			makeImageMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			**images[2]->get(), subresourceRange)
		};

		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL,
			DE_LENGTH_OF_ARRAY(preShaderImageBarriers), preShaderImageBarriers);

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		descriptorSetUpdate (*descriptorSet, &descriptorImageInfos[0]);

		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		vk.cmdDispatch(cmdBuffer, extentUncompressed.x(), extentUncompressed.y(), extentUncompressed.z());
	}
	endCommandBuffer(vk, cmdBuffer);
	submitCommandsAndWait(vk, device, queue, cmdBuffer);
}

class GraphicsAttachmentsTestInstance : public BasicTranscodingTestInstance
{
public:
									GraphicsAttachmentsTestInstance	(Context& context, const TestParameters& parameters);
	virtual TestStatus				iterate							(void);

protected:
	virtual bool					isWriteToCompressedOperation	();
	VkImageCreateInfo				makeCreateImageInfo				(VkFormat format, ImageType type, const UVec3& size, VkImageUsageFlags usageFlags, const VkImageCreateFlags* createFlags);
	VkImageViewUsageCreateInfoKHR	makeImageViewUsageCreateInfo	(VkImageUsageFlags imageUsageFlags);
	VkDeviceSize					getCompressedImageData			(const VkFormat format, const UVec3& size, std::vector<deUint8>& data);
	VkDeviceSize					getUncompressedImageData		(const VkFormat format, const UVec3& size, std::vector<deUint8>& data);
	virtual void					transcode						(std::vector<deUint8>& srcData, std::vector<deUint8>& dstData, de::MovePtr<Image>& outputImage);
	bool							compareAndLog					(const void* reference, const void* result, size_t size);
	bool							verifyDecompression				(const std::vector<deUint8>& refCompressedData, const de::MovePtr<Image>& resCompressedImage);

	deUint32						m_arrayLayers;
	UVec3							m_layerSize;
};

GraphicsAttachmentsTestInstance::GraphicsAttachmentsTestInstance (Context& context, const TestParameters& parameters)
	: BasicTranscodingTestInstance(context, parameters)
	, m_arrayLayers(getNumLayers(m_parameters.imageType, m_parameters.size))
	, m_layerSize(getLayerSize(m_parameters.imageType, m_parameters.size))
{
}

TestStatus GraphicsAttachmentsTestInstance::iterate (void)
{
	std::vector<deUint8>	srcData;
	std::vector<deUint8>	dstData;
	de::MovePtr<Image>		outputImage;

	transcode(srcData, dstData, outputImage);

	DE_ASSERT(srcData.size() > 0 && srcData.size() == dstData.size());

	if (!compareAndLog(&srcData[0], &dstData[0], srcData.size()))
		return TestStatus::fail("Output differs from input");

	// Verify by sampling
	if (isWriteToCompressedOperation())
		if (!verifyDecompression(srcData, outputImage))
			return TestStatus::fail("Decompressed images difference detected");

	return TestStatus::pass("Pass");
}

void GraphicsAttachmentsTestInstance::transcode (std::vector<deUint8>& srcData, std::vector<deUint8>& dstData, de::MovePtr<Image>& outputImage)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue							queue					= m_context.getUniversalQueue();
	Allocator&								allocator				= m_context.getDefaultAllocator();

	const deUint32							levelCount				= m_layerSize[2];
	const VkImageSubresourceRange			subresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, levelCount, 0u, m_arrayLayers);
	const VkImageCreateFlags*				imgCreateFlagsOverride	= DE_NULL;
	const VkImageViewUsageCreateInfoKHR		imageViewUsageKHR		= makeImageViewUsageCreateInfo(m_parameters.compressedImageUsage);
	const VkImageViewUsageCreateInfoKHR*	imageViewUsageKHRNull	= (VkImageViewUsageCreateInfoKHR*)DE_NULL;

	const UVec3								compressedImageRes		= m_parameters.size;
	const UVec3								uncompressedImageRes	= getCompressedImageResolutionInBlocks(m_parameters.formatCompressed, m_parameters.size);

	const VkFormat							srcFormat				= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? m_parameters.formatCompressed :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? m_parameters.formatUncompressed :
																	  VK_FORMAT_UNDEFINED;
	const UVec3								srcImageResolution		= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? compressedImageRes :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? uncompressedImageRes :
																	  UVec3(0, 0, 0);
	const VkDeviceSize						srcImageSizeInBytes		= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? getCompressedImageData(srcFormat, srcImageResolution, srcData) :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? getUncompressedImageData(srcFormat, srcImageResolution, srcData) :
																	  0;
	const VkImageUsageFlags					srcImageUsageFlags		= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? m_parameters.compressedImageUsage :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? m_parameters.uncompressedImageUsage :
																	  0;
	const VkImageViewUsageCreateInfoKHR*	srcImageViewUsageKHR	= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? &imageViewUsageKHR :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? imageViewUsageKHRNull :
																	  imageViewUsageKHRNull;

	const VkFormat							dstFormat				= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? m_parameters.formatUncompressed :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? m_parameters.formatCompressed :
																	  VK_FORMAT_UNDEFINED;
	const UVec3								dstImageResolution		= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? uncompressedImageRes :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? compressedImageRes :
																	  UVec3(0, 0, 0);
	const VkDeviceSize						dstImageSizeInBytes		= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? getUncompressedImageSizeInBytes(dstFormat, dstImageResolution) :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? getCompressedImageSizeInBytes(dstFormat, dstImageResolution) :
																	  0;
	const VkImageUsageFlags					dstImageUsageFlags		= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? m_parameters.uncompressedImageUsage :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? m_parameters.compressedImageUsage :
																	  0;
	const VkImageViewUsageCreateInfoKHR*	dstImageViewUsageKHR	= (m_parameters.operation == OPERATION_ATTACHMENT_READ)  ? imageViewUsageKHRNull :
																	  (m_parameters.operation == OPERATION_ATTACHMENT_WRITE) ? &imageViewUsageKHR :
																	  imageViewUsageKHRNull;

	const std::vector<tcu::Vec4>			vertexArray				= createFullscreenQuad();
	const deUint32							vertexCount				= static_cast<deUint32>(vertexArray.size());
	const size_t							vertexBufferSizeInBytes	= vertexCount * sizeof(vertexArray[0]);
	const MovePtr<Buffer>					vertexBuffer			= MovePtr<Buffer>(new Buffer(vk, device, allocator, makeBufferCreateInfo(vertexBufferSizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));
	const Allocation&						vertexBufferAlloc		= vertexBuffer->getAllocation();
	const VkDeviceSize						vertexBufferOffset[]	= { 0 };

	const VkBufferCreateInfo				srcImageBufferInfo		(makeBufferCreateInfo(srcImageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
	const MovePtr<Buffer>					srcImageBuffer			= MovePtr<Buffer>(new Buffer(vk, device, allocator, srcImageBufferInfo, MemoryRequirement::HostVisible));

	const VkImageCreateInfo					srcImageCreateInfo		= makeCreateImageInfo(srcFormat, m_parameters.imageType, srcImageResolution, srcImageUsageFlags, imgCreateFlagsOverride);
	const MovePtr<Image>					srcImage				(new Image(vk, device, allocator, srcImageCreateInfo, MemoryRequirement::Any));
	Move<VkImageView>						srcImageView			(makeImageView(vk, device, srcImage->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatUncompressed, subresourceRange, srcImageViewUsageKHR));

	const VkImageCreateInfo					dstImageCreateInfo		= makeCreateImageInfo(dstFormat, m_parameters.imageType, dstImageResolution, dstImageUsageFlags, imgCreateFlagsOverride);
	de::MovePtr<Image>						dstImage				(new Image(vk, device, allocator, dstImageCreateInfo, MemoryRequirement::Any));
	Move<VkImageView>						dstImageView			(makeImageView(vk, device, dstImage->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatUncompressed, subresourceRange, dstImageViewUsageKHR));

	const VkBufferCreateInfo				dstImageBufferInfo		(makeBufferCreateInfo(dstImageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	MovePtr<Buffer>							dstImageBuffer			= MovePtr<Buffer>(new Buffer(vk, device, allocator, dstImageBufferInfo, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>			vertShaderModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>			fragShaderModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

	const Unique<VkRenderPass>				renderPass				(makeRenderPass(vk, device, m_parameters.formatUncompressed, m_parameters.formatUncompressed));

	const Move<VkDescriptorSetLayout>		descriptorSetLayout		(DescriptorSetLayoutBuilder()
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT)
																		.build(vk, device));
	const Move<VkDescriptorPool>			descriptorPool			(DescriptorPoolBuilder()
																		.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, m_arrayLayers)
																		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, m_arrayLayers));
	const Move<VkDescriptorSet>				descriptorSet			(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorImageInfo				descriptorSrcImageInfo	(makeDescriptorImageInfo(DE_NULL, *srcImageView, VK_IMAGE_LAYOUT_GENERAL));

	const VkExtent2D						renderSize				(makeExtent2D(uncompressedImageRes[0], uncompressedImageRes[1]));
	const Unique<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertShaderModule, *fragShaderModule, renderSize, 1u));
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferImageCopy					srcCopyRegion			= makeBufferImageCopy(srcImageResolution[0], srcImageResolution[1]);
	const VkBufferMemoryBarrier				srcCopyBufferBarrierPre	= makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, srcImageBuffer->get(), 0ull, srcImageSizeInBytes);
	const VkImageMemoryBarrier				srcCopyImageBarrierPre	= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, srcImage->get(), subresourceRange);
	const VkImageMemoryBarrier				srcCopyImageBarrierPost	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, srcImage->get(), subresourceRange);
	const VkBufferImageCopy					dstCopyRegion			= makeBufferImageCopy(dstImageResolution[0], dstImageResolution[1]);

	const VkImageView						attachmentBindInfos[]	= { *srcImageView, *dstImageView };
	const Move<VkFramebuffer>				framebuffer				(makeFramebuffer(vk, device, *renderPass, DE_LENGTH_OF_ARRAY(attachmentBindInfos), attachmentBindInfos, renderSize, m_arrayLayers));

	DE_ASSERT(srcImageSizeInBytes == dstImageSizeInBytes);

	// Upload vertex data
	deMemcpy(vertexBufferAlloc.getHostPtr(), &vertexArray[0], vertexBufferSizeInBytes);
	flushMappedMemoryRange(vk, device, vertexBufferAlloc.getMemory(), vertexBufferAlloc.getOffset(), vertexBufferSizeInBytes);

	// Upload source image data
	const Allocation& alloc = srcImageBuffer->getAllocation();
	deMemcpy(alloc.getHostPtr(), &srcData[0], (size_t)srcImageSizeInBytes);
	flushMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), srcImageSizeInBytes);

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

	//Copy buffer to image
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &srcCopyBufferBarrierPre, 1u, &srcCopyImageBarrierPre);
	vk.cmdCopyBufferToImage(*cmdBuffer, srcImageBuffer->get(), srcImage->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &srcCopyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &srcCopyImageBarrierPost);

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderSize);

	for (deUint32 layerNdx = 0; layerNdx < m_arrayLayers; ++layerNdx)
	{
		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descriptorSrcImageInfo)
			.update(vk, device);

		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer->get(), vertexBufferOffset);
		vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);
	}

	vk.cmdEndRenderPass(*cmdBuffer);

	const VkImageMemoryBarrier prepareForTransferBarrier = makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		dstImage->get(), subresourceRange);

	const VkBufferMemoryBarrier copyBarrier = makeBufferMemoryBarrier(
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
		dstImageBuffer->get(), 0ull, dstImageSizeInBytes);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &prepareForTransferBarrier);
	vk.cmdCopyImageToBuffer(*cmdBuffer, dstImage->get(), VK_IMAGE_LAYOUT_GENERAL, dstImageBuffer->get(), 1u, &dstCopyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &copyBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	const Allocation& dstImageBufferAlloc = dstImageBuffer->getAllocation();
	invalidateMappedMemoryRange(vk, device, dstImageBufferAlloc.getMemory(), dstImageBufferAlloc.getOffset(), dstImageSizeInBytes);
	dstData.resize((size_t)dstImageSizeInBytes);
	deMemcpy(&dstData[0], dstImageBufferAlloc.getHostPtr(), (size_t)dstImageSizeInBytes);

	outputImage = dstImage;
}

bool GraphicsAttachmentsTestInstance::isWriteToCompressedOperation ()
{
	return (m_parameters.operation == OPERATION_ATTACHMENT_WRITE);
}

VkImageCreateInfo GraphicsAttachmentsTestInstance::makeCreateImageInfo (VkFormat format, ImageType type, const UVec3& size, VkImageUsageFlags usageFlags, const VkImageCreateFlags* createFlags)
{
	const VkImageType			imageType				= mapImageType(type);
	const VkImageCreateFlags	imageCreateFlagsBase	= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	const VkImageCreateFlags	imageCreateFlagsAddOn	= isCompressedFormat(format) ? VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT_KHR | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR : 0;
	const VkImageCreateFlags	imageCreateFlags		= (createFlags != DE_NULL) ? *createFlags : (imageCreateFlagsBase | imageCreateFlagsAddOn);

	const VkImageCreateInfo createImageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		imageCreateFlags,								// VkImageCreateFlags		flags;
		imageType,										// VkImageType				imageType;
		format,											// VkFormat					format;
		makeExtent3D(getLayerSize(type, size)),			// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usageFlags,										// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};

	return createImageInfo;
}

VkImageViewUsageCreateInfoKHR GraphicsAttachmentsTestInstance::makeImageViewUsageCreateInfo (VkImageUsageFlags imageUsageFlags)
{
	VkImageViewUsageCreateInfoKHR imageViewUsageCreateInfoKHR =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO_KHR,	//VkStructureType		sType;
		DE_NULL,											//const void*			pNext;
		imageUsageFlags,									//VkImageUsageFlags		usage;
	};

	return imageViewUsageCreateInfoKHR;
}

VkDeviceSize GraphicsAttachmentsTestInstance::getCompressedImageData (const VkFormat format, const UVec3& size, std::vector<deUint8>& data)
{
	VkDeviceSize	sizeBytes	= getCompressedImageSizeInBytes(format, size);

	data.resize((size_t)sizeBytes);
	generateData(&data[0], data.size(), format);

	return sizeBytes;
}

VkDeviceSize GraphicsAttachmentsTestInstance::getUncompressedImageData (const VkFormat format, const UVec3& size, std::vector<deUint8>& data)
{
	tcu::IVec3				sizeAsIVec3	= tcu::IVec3(static_cast<int>(size[0]), static_cast<int>(size[1]), static_cast<int>(size[2]));
	VkDeviceSize			sizeBytes	= getImageSizeBytes(sizeAsIVec3, format);

	data.resize((size_t)sizeBytes);
	generateData(&data[0], data.size(), format);

	return sizeBytes;
}

bool GraphicsAttachmentsTestInstance::compareAndLog (const void* reference, const void* result, size_t size)
{
	tcu::TestLog&	log			= m_context.getTestContext().getLog();

	const deUint64*	ref64	= reinterpret_cast<const deUint64*>(reference);
	const deUint64*	res64	= reinterpret_cast<const deUint64*>(result);
	const size_t	sizew	= size / sizeof(deUint64);
	bool			equal	= true;

	DE_ASSERT(size % sizeof(deUint64) == 0);

	for (deUint32 ndx = 0u; ndx < static_cast<deUint32>(sizew); ndx++)
	{
		if (ref64[ndx] != res64[ndx])
		{
			std::stringstream str;

			str	<< "Difference begins near byte " << ndx * sizeof(deUint64) << "."
				<< " reference value: 0x" << std::hex << std::setw(2ull * sizeof(deUint64)) << std::setfill('0') << ref64[ndx]
				<< " result value: 0x" << std::hex << std::setw(2ull * sizeof(deUint64)) << std::setfill('0') << res64[ndx];

			log.writeMessage(str.str().c_str());

			equal = false;

			break;
		}
	}

	return equal;
}

bool GraphicsAttachmentsTestInstance::verifyDecompression (const std::vector<deUint8>& refCompressedData, const de::MovePtr<Image>& resCompressedImage)
{
	const DeviceInterface&				vk							= m_context.getDeviceInterface();
	const VkDevice						device						= m_context.getDevice();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue						= m_context.getUniversalQueue();
	Allocator&							allocator					= m_context.getDefaultAllocator();

	const deUint32						levelCount					= m_layerSize[2];
	const VkImageSubresourceRange		subresourceRange			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, levelCount, 0u, m_arrayLayers);

	const VkDeviceSize					dstBufferSize				= getUncompressedImageSizeInBytes(m_parameters.formatForVerify, m_parameters.size);
	const VkImageUsageFlags				refSrcImageUsageFlags		= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	const std::vector<tcu::Vec4>		vertexArray					= createFullscreenQuad();
	const deUint32						vertexCount					= static_cast<deUint32>(vertexArray.size());
	const size_t						vertexBufferSizeInBytes		= vertexCount * sizeof(vertexArray[0]);
	const MovePtr<Buffer>				vertexBuffer				(new Buffer(vk, device, allocator, makeBufferCreateInfo(vertexBufferSizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));
	const Allocation&					vertexBufferAlloc			= vertexBuffer->getAllocation();
	const VkDeviceSize					vertexBufferOffset[]		= { 0 };

	const VkBufferCreateInfo			refSrcImageBufferInfo		(makeBufferCreateInfo(refCompressedData.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
	const MovePtr<Buffer>				refSrcImageBuffer			= MovePtr<Buffer>(new Buffer(vk, device, allocator, refSrcImageBufferInfo, MemoryRequirement::HostVisible));

	const VkImageCreateFlags			refSrcImageCreateFlags		= 0;
	const VkImageCreateInfo				refSrcImageCreateInfo		= makeCreateImageInfo(m_parameters.formatCompressed, m_parameters.imageType, m_parameters.size, refSrcImageUsageFlags, &refSrcImageCreateFlags);
	const MovePtr<Image>				refSrcImage					(new Image(vk, device, allocator, refSrcImageCreateInfo, MemoryRequirement::Any));
	Move<VkImageView>					refSrcImageView				(makeImageView(vk, device, refSrcImage->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatCompressed, subresourceRange));

	const VkImageUsageFlags				resSrcImageUsageFlags		= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageViewUsageCreateInfoKHR	resSrcImageViewUsageKHR		= makeImageViewUsageCreateInfo(resSrcImageUsageFlags);
	Move<VkImageView>					resSrcImageView				(makeImageView(vk, device, resCompressedImage->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatCompressed, subresourceRange, &resSrcImageViewUsageKHR));

	const VkImageCreateFlags			refDstImageCreateFlags		= 0;
	const VkImageUsageFlags				refDstImageUsageFlags		= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const VkImageCreateInfo				refDstImageCreateInfo		= makeCreateImageInfo(m_parameters.formatForVerify, m_parameters.imageType, m_parameters.size, refDstImageUsageFlags, &refDstImageCreateFlags);
	const MovePtr<Image>				refDstImage					(new Image(vk, device, allocator, refDstImageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				refDstImageView				(makeImageView(vk, device, refDstImage->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatForVerify, subresourceRange));
	const VkImageMemoryBarrier			refDstCopyImageBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, refDstImage->get(), subresourceRange);
	const VkBufferCreateInfo			refDstBufferInfo			(makeBufferCreateInfo(dstBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const MovePtr<Buffer>				refDstBuffer				= MovePtr<Buffer>(new Buffer(vk, device, allocator, refDstBufferInfo, MemoryRequirement::HostVisible));

	const VkImageCreateFlags			resDstImageCreateFlags		= 0;
	const VkImageUsageFlags				resDstImageUsageFlags		= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const VkImageCreateInfo				resDstImageCreateInfo		= makeCreateImageInfo(m_parameters.formatForVerify, m_parameters.imageType, m_parameters.size, resDstImageUsageFlags, &resDstImageCreateFlags);
	const MovePtr<Image>				resDstImage					(new Image(vk, device, allocator, resDstImageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				resDstImageView				(makeImageView(vk, device, resDstImage->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatForVerify, subresourceRange));
	const VkImageMemoryBarrier			resDstCopyImageBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, resDstImage->get(), subresourceRange);
	const VkBufferCreateInfo			resDstBufferInfo			(makeBufferCreateInfo(dstBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const MovePtr<Buffer>				resDstBuffer				= MovePtr<Buffer>(new Buffer(vk, device, allocator, resDstBufferInfo, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>		vertShaderModule			(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>		fragShaderModule			(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag_verify"), 0));

	const Unique<VkRenderPass>			renderPass					(makeRenderPass(vk, device));

	const Move<VkDescriptorSetLayout>	descriptorSetLayout			(DescriptorSetLayoutBuilder()
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
																		.build(vk, device));
	const Move<VkDescriptorPool>		descriptorPool				(DescriptorPoolBuilder()
																		.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_arrayLayers)
																		.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_arrayLayers)
																		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_arrayLayers)
																		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_arrayLayers)
																		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, m_arrayLayers));
	const Move<VkDescriptorSet>			descriptorSet				(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkSamplerCreateInfo			refSrcSamplerInfo			(makeSamplerCreateInfo());
	const Move<VkSampler>				refSrcSampler				= vk::createSampler(vk, device, &refSrcSamplerInfo);
	const VkSamplerCreateInfo			resSrcSamplerInfo			(makeSamplerCreateInfo());
	const Move<VkSampler>				resSrcSampler				= vk::createSampler(vk, device, &resSrcSamplerInfo);
	const VkDescriptorImageInfo			descriptorRefSrcImage		(makeDescriptorImageInfo(*refSrcSampler, *refSrcImageView, VK_IMAGE_LAYOUT_GENERAL));
	const VkDescriptorImageInfo			descriptorResSrcImage		(makeDescriptorImageInfo(*resSrcSampler, *resSrcImageView, VK_IMAGE_LAYOUT_GENERAL));
	const VkDescriptorImageInfo			descriptorRefDstImage		(makeDescriptorImageInfo(DE_NULL, *refDstImageView, VK_IMAGE_LAYOUT_GENERAL));
	const VkDescriptorImageInfo			descriptorResDstImage		(makeDescriptorImageInfo(DE_NULL, *resDstImageView, VK_IMAGE_LAYOUT_GENERAL));

	const VkExtent2D					renderSize					(makeExtent2D(m_parameters.size.x(), m_parameters.size.y()));
	const Unique<VkPipelineLayout>		pipelineLayout				(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>			pipeline					(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertShaderModule, *fragShaderModule, renderSize, 0u));
	const Unique<VkCommandPool>			cmdPool						(createCommandPool(vk, device, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer					(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferImageCopy				copyRegion					= makeBufferImageCopy(m_parameters.size.x(), m_parameters.size.y());
	const VkBufferMemoryBarrier			refSrcCopyBufferBarrier		= makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, refSrcImageBuffer->get(), 0ull, refCompressedData.size());
	const VkImageMemoryBarrier			refSrcCopyImageBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, refSrcImage->get(), subresourceRange);
	const VkImageMemoryBarrier			refSrcCopyImageBarrierPost	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, refSrcImage->get(), subresourceRange);

	const Move<VkFramebuffer>			framebuffer					(makeFramebuffer(vk, device, *renderPass, 0, DE_NULL, renderSize, m_arrayLayers));

	// Upload vertex data
	deMemcpy(vertexBufferAlloc.getHostPtr(), &vertexArray[0], vertexBufferSizeInBytes);
	flushMappedMemoryRange(vk, device, vertexBufferAlloc.getMemory(), vertexBufferAlloc.getOffset(), vertexBufferSizeInBytes);

	// Upload source image data
	{
		const Allocation& refSrcImageBufferAlloc = refSrcImageBuffer->getAllocation();
		deMemcpy(refSrcImageBufferAlloc.getHostPtr(), &refCompressedData[0], refCompressedData.size());
		flushMappedMemoryRange(vk, device, refSrcImageBufferAlloc.getMemory(), refSrcImageBufferAlloc.getOffset(), refCompressedData.size());
	}

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

	//Copy buffer to image
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &refSrcCopyBufferBarrier, 1u, &refSrcCopyImageBarrier);
	vk.cmdCopyBufferToImage(*cmdBuffer, refSrcImageBuffer->get(), refSrcImage->get(), VK_IMAGE_LAYOUT_GENERAL, 1u, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, DE_NULL, 1u, &refSrcCopyImageBarrierPost);

	// Make reference and result images readable
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0u, DE_NULL, 1u, &refDstCopyImageBarrier);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0u, DE_NULL, 1u, &resDstCopyImageBarrier);

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderSize);
	for (deUint32 layerNdx = 0; layerNdx < m_arrayLayers; ++layerNdx)
	{
		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorRefSrcImage)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorResSrcImage)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorRefDstImage)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(3u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorResDstImage)
			.update(vk, device);

		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer->get(), vertexBufferOffset);
		vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);
	}
	vk.cmdEndRenderPass(*cmdBuffer);

	// Decompress reference image
	{
		const VkImageMemoryBarrier refDstImageBarrier = makeImageMemoryBarrier(
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			refDstImage->get(), subresourceRange);

		const VkBufferMemoryBarrier refDstBufferBarrier = makeBufferMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
			refDstBuffer->get(), 0ull, dstBufferSize);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &refDstImageBarrier);
		vk.cmdCopyImageToBuffer(*cmdBuffer, refDstImage->get(), VK_IMAGE_LAYOUT_GENERAL, refDstBuffer->get(), 1u, &copyRegion);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &refDstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	}

	// Decompress result image
	{
		const VkImageMemoryBarrier resDstImageBarrier = makeImageMemoryBarrier(
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			resDstImage->get(), subresourceRange);

		const VkBufferMemoryBarrier resDstBufferBarrier = makeBufferMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
			resDstBuffer->get(), 0ull, dstBufferSize);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &resDstImageBarrier);
		vk.cmdCopyImageToBuffer(*cmdBuffer, resDstImage->get(), VK_IMAGE_LAYOUT_GENERAL, resDstBuffer->get(), 1u, &copyRegion);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &resDstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	}

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Compare decompressed pixel data in reference and result images
	{
		const Allocation&	refDstBufferAlloc	= refDstBuffer->getAllocation();
		invalidateMappedMemoryRange(vk, device, refDstBufferAlloc.getMemory(), refDstBufferAlloc.getOffset(), dstBufferSize);

		const Allocation&	resDstBufferAlloc	= resDstBuffer->getAllocation();
		invalidateMappedMemoryRange(vk, device, resDstBufferAlloc.getMemory(), resDstBufferAlloc.getOffset(), dstBufferSize);

		return compareAndLog(refDstBufferAlloc.getHostPtr(), resDstBufferAlloc.getHostPtr(), (size_t)dstBufferSize);
	}
}


class GraphicsTextureTestInstance : public GraphicsAttachmentsTestInstance
{
public:
						GraphicsTextureTestInstance		(Context& context, const TestParameters& parameters);

protected:
	virtual bool		isWriteToCompressedOperation	();
	void				transcode						(std::vector<deUint8>& srcData, std::vector<deUint8>& dstData, de::MovePtr<Image>& outputImage);
};

GraphicsTextureTestInstance::GraphicsTextureTestInstance (Context& context, const TestParameters& parameters)
	: GraphicsAttachmentsTestInstance(context, parameters)
{
}

bool GraphicsTextureTestInstance::isWriteToCompressedOperation ()
{
	return (m_parameters.operation == OPERATION_TEXTURE_WRITE);
}

void GraphicsTextureTestInstance::transcode (std::vector<deUint8>& srcData, std::vector<deUint8>& dstData, de::MovePtr<Image>& outputImage)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue							queue					= m_context.getUniversalQueue();
	Allocator&								allocator				= m_context.getDefaultAllocator();

	const deUint32							levelCount				= m_layerSize[2];
	const VkImageSubresourceRange			subresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, levelCount, 0u, m_arrayLayers);
	const UVec3								compressedImageRes		= m_parameters.size;
	const UVec3								uncompressedImageRes	= getCompressedImageResolutionInBlocks(m_parameters.formatCompressed, m_parameters.size);
	const VkImageCreateFlags*				imgCreateFlagsOverride	= DE_NULL;
	const VkImageViewUsageCreateInfoKHR		imageViewUsageKHR		= makeImageViewUsageCreateInfo(m_parameters.compressedImageUsage);
	const VkImageViewUsageCreateInfoKHR*	imageViewUsageKHRNull	= (VkImageViewUsageCreateInfoKHR*)DE_NULL;

	const VkFormat							srcFormat				= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? m_parameters.formatCompressed :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? m_parameters.formatUncompressed :
																	  VK_FORMAT_UNDEFINED;
	const UVec3								srcImageResolution		= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? compressedImageRes :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? uncompressedImageRes :
																	  UVec3(0, 0, 0);
	const VkDeviceSize						srcImageSizeInBytes		= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? getCompressedImageData(srcFormat, srcImageResolution, srcData) :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? getUncompressedImageData(srcFormat, srcImageResolution, srcData) :
																	  0;
	const VkImageUsageFlags					srcImageUsageFlags		= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? m_parameters.compressedImageUsage :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? m_parameters.uncompressedImageUsage :
																	  0;
	const VkImageViewUsageCreateInfoKHR*	srcImageViewUsageKHR	= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? &imageViewUsageKHR :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? imageViewUsageKHRNull :
																	  imageViewUsageKHRNull;

	const VkFormat							dstFormat				= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? m_parameters.formatUncompressed :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? m_parameters.formatCompressed :
																	  VK_FORMAT_UNDEFINED;
	const UVec3								dstImageResolution		= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? uncompressedImageRes :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? compressedImageRes :
																	  UVec3(0, 0, 0);
	const VkDeviceSize						dstImageSizeInBytes		= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? getUncompressedImageSizeInBytes(dstFormat, dstImageResolution) :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? getCompressedImageSizeInBytes(dstFormat, dstImageResolution) :
																	  0;
	const VkImageUsageFlags					dstImageUsageFlags		= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? m_parameters.uncompressedImageUsage :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? m_parameters.compressedImageUsage :
																	  0;
	const VkImageViewUsageCreateInfoKHR*	dstImageViewUsageKHR	= (m_parameters.operation == OPERATION_TEXTURE_READ)  ? imageViewUsageKHRNull :
																	  (m_parameters.operation == OPERATION_TEXTURE_WRITE) ? &imageViewUsageKHR :
																	  imageViewUsageKHRNull;

	const std::vector<tcu::Vec4>			vertexArray				= createFullscreenQuad();
	const deUint32							vertexCount				= static_cast<deUint32>(vertexArray.size());
	const size_t							vertexBufferSizeInBytes	= vertexCount * sizeof(vertexArray[0]);
	const MovePtr<Buffer>					vertexBuffer			= MovePtr<Buffer>(new Buffer(vk, device, allocator, makeBufferCreateInfo(vertexBufferSizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));
	const Allocation&						vertexBufferAlloc		= vertexBuffer->getAllocation();
	const VkDeviceSize						vertexBufferOffset[]	= { 0 };

	const VkBufferCreateInfo				srcImageBufferInfo		(makeBufferCreateInfo(srcImageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
	const MovePtr<Buffer>					srcImageBuffer			= MovePtr<Buffer>(new Buffer(vk, device, allocator, srcImageBufferInfo, MemoryRequirement::HostVisible));

	const VkImageCreateInfo					srcImageCreateInfo		= makeCreateImageInfo(srcFormat, m_parameters.imageType, srcImageResolution, srcImageUsageFlags, imgCreateFlagsOverride);
	const MovePtr<Image>					srcImage				(new Image(vk, device, allocator, srcImageCreateInfo, MemoryRequirement::Any));
	Move<VkImageView>						srcImageView			(makeImageView(vk, device, srcImage->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatUncompressed, subresourceRange, srcImageViewUsageKHR));

	const VkImageCreateInfo					dstImageCreateInfo		= makeCreateImageInfo(dstFormat, m_parameters.imageType, dstImageResolution, dstImageUsageFlags, imgCreateFlagsOverride);
	de::MovePtr<Image>						dstImage				(new Image(vk, device, allocator, dstImageCreateInfo, MemoryRequirement::Any));
	Move<VkImageView>						dstImageView			(makeImageView(vk, device, dstImage->get(), mapImageViewType(m_parameters.imageType), m_parameters.formatUncompressed, subresourceRange, dstImageViewUsageKHR));
	const VkImageMemoryBarrier				dstCopyImageBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, dstImage->get(), subresourceRange);

	const VkBufferCreateInfo				dstImageBufferInfo		(makeBufferCreateInfo(dstImageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	MovePtr<Buffer>							dstImageBuffer			= MovePtr<Buffer>(new Buffer(vk, device, allocator, dstImageBufferInfo, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>			vertShaderModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>			fragShaderModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

	const Unique<VkRenderPass>				renderPass				(makeRenderPass(vk, device));

	const Move<VkDescriptorSetLayout>		descriptorSetLayout		(DescriptorSetLayoutBuilder()
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
																		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
																		.build(vk, device));
	const Move<VkDescriptorPool>			descriptorPool			(DescriptorPoolBuilder()
																		.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_arrayLayers)
																		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_arrayLayers)
																		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, m_arrayLayers));
	const Move<VkDescriptorSet>				descriptorSet			(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkSamplerCreateInfo				srcSamplerInfo			(makeSamplerCreateInfo());
	const Move<VkSampler>					srcSampler				= vk::createSampler(vk, device, &srcSamplerInfo);
	const VkDescriptorImageInfo				descriptorSrcImage		(makeDescriptorImageInfo(*srcSampler, *srcImageView, VK_IMAGE_LAYOUT_GENERAL));
	const VkDescriptorImageInfo				descriptorDstImage		(makeDescriptorImageInfo(DE_NULL, *dstImageView, VK_IMAGE_LAYOUT_GENERAL));

	const VkExtent2D						renderSize				(makeExtent2D(uncompressedImageRes[0], uncompressedImageRes[1]));
	const Unique<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertShaderModule, *fragShaderModule, renderSize, 0u));
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferImageCopy					srcCopyRegion			= makeBufferImageCopy(srcImageResolution[0], srcImageResolution[1]);
	const VkBufferMemoryBarrier				srcCopyBufferBarrier	= makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, srcImageBuffer->get(), 0ull, srcImageSizeInBytes);
	const VkImageMemoryBarrier				srcCopyImageBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, srcImage->get(), subresourceRange);
	const VkImageMemoryBarrier				srcCopyImageBarrierPost	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, srcImage->get(), subresourceRange);

	const VkBufferImageCopy					dstCopyRegion			= makeBufferImageCopy(dstImageResolution[0], dstImageResolution[1]);

	const VkExtent2D						framebufferSize			(makeExtent2D(dstImageResolution[0], dstImageResolution[1]));
	const Move<VkFramebuffer>				framebuffer				(makeFramebuffer(vk, device, *renderPass, 0, DE_NULL, framebufferSize, m_arrayLayers));

	DE_ASSERT(srcImageSizeInBytes == dstImageSizeInBytes);

	// Upload vertex data
	deMemcpy(vertexBufferAlloc.getHostPtr(), &vertexArray[0], vertexBufferSizeInBytes);
	flushMappedMemoryRange(vk, device, vertexBufferAlloc.getMemory(), vertexBufferAlloc.getOffset(), vertexBufferSizeInBytes);

	// Upload source image data
	const Allocation& alloc = srcImageBuffer->getAllocation();
	deMemcpy(alloc.getHostPtr(), &srcData[0], (size_t)srcImageSizeInBytes);
	flushMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), srcImageSizeInBytes);

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

	//Copy buffer to image
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &srcCopyBufferBarrier, 1u, &srcCopyImageBarrier);
	vk.cmdCopyBufferToImage(*cmdBuffer, srcImageBuffer->get(), srcImage->get(), VK_IMAGE_LAYOUT_GENERAL, 1u, &srcCopyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &srcCopyImageBarrierPost);

	// Make source image readable
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0u, DE_NULL, 1u, &dstCopyImageBarrier);

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderSize);

	for (deUint32 layerNdx = 0; layerNdx < m_arrayLayers; ++layerNdx)
	{
		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorSrcImage)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorDstImage)
			.update(vk, device);

		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer->get(), vertexBufferOffset);
		vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);
	}

	vk.cmdEndRenderPass(*cmdBuffer);

	const VkImageMemoryBarrier prepareForTransferBarrier = makeImageMemoryBarrier(
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
		dstImage->get(), subresourceRange);

	const VkBufferMemoryBarrier copyBarrier = makeBufferMemoryBarrier(
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
		dstImageBuffer->get(), 0ull, dstImageSizeInBytes);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &prepareForTransferBarrier);
	vk.cmdCopyImageToBuffer(*cmdBuffer, dstImage->get(), VK_IMAGE_LAYOUT_GENERAL, dstImageBuffer->get(), 1u, &dstCopyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &copyBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	const Allocation& dstImageBufferAlloc = dstImageBuffer->getAllocation();
	invalidateMappedMemoryRange(vk, device, dstImageBufferAlloc.getMemory(), dstImageBufferAlloc.getOffset(), dstImageSizeInBytes);
	dstData.resize((size_t)dstImageSizeInBytes);
	deMemcpy(&dstData[0], dstImageBufferAlloc.getHostPtr(), (size_t)dstImageSizeInBytes);

	outputImage = dstImage;
}


class TexelViewCompatibleCase : public TestCase
{
public:
							TexelViewCompatibleCase		(TestContext&				testCtx,
														 const std::string&			name,
														 const std::string&			desc,
														 const TestParameters&		parameters);
	void					initPrograms				(SourceCollections&			programCollection) const;
	TestInstance*			createInstance				(Context&					context) const;
protected:
	const TestParameters	m_parameters;
};

TexelViewCompatibleCase::TexelViewCompatibleCase (TestContext& testCtx, const std::string& name, const std::string& desc, const TestParameters& parameters)
	: TestCase				(testCtx, name, desc)
	, m_parameters			(parameters)
{
}

void TexelViewCompatibleCase::initPrograms (vk::SourceCollections&	programCollection) const
{
	switch (m_parameters.shader)
	{
		case SHADER_TYPE_COMPUTE:
		{
			const std::string	imageTypeStr		= getShaderImageType(mapVkFormat(m_parameters.formatUncompressed), m_parameters.imageType);
			const std::string	formatQualifierStr	= getShaderImageFormatQualifier(mapVkFormat(m_parameters.formatUncompressed));
			std::ostringstream	src;
			std::ostringstream	src_decompress;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n\n";
			src_decompress << src.str();

			switch(m_parameters.operation)
			{
				case OPERATION_IMAGE_LOAD:
				{
					src << "layout (binding = 0, "<<formatQualifierStr<<") readonly uniform "<<imageTypeStr<<" u_image0;\n"
						<< "layout (binding = 1, "<<formatQualifierStr<<") writeonly uniform "<<imageTypeStr<<" u_image1;\n\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
						<< "    imageStore(u_image1, pos, imageLoad(u_image0, pos));\n"
						<< "}\n";

					break;
				}

				case OPERATION_TEXEL_FETCH:
				{
					src << "layout (binding = 0) uniform "<<getGlslSamplerType(mapVkFormat(m_parameters.formatUncompressed), mapImageViewType(m_parameters.imageType))<<" u_image0;\n"
						<< "layout (binding = 1, "<<formatQualifierStr<<") writeonly uniform "<<imageTypeStr<<" u_image1;\n\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "    ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);\n"
						<< "    imageStore(u_image1, pos.xy, texelFetch(u_image0, pos.xy, pos.z));\n"
						<< "}\n";

					break;
				}

				case OPERATION_TEXTURE:
				{
					src << "layout (binding = 0) uniform "<<getGlslSamplerType(mapVkFormat(m_parameters.formatUncompressed), mapImageViewType(m_parameters.imageType))<<" u_image0;\n"
						<< "layout (binding = 1, "<<formatQualifierStr<<") writeonly uniform "<<imageTypeStr<<" u_image1;\n\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
						<< "    imageStore(u_image1, pos, texture(u_image0, pos));\n"
						<< "}\n";

					break;
				}

				case OPERATION_IMAGE_STORE:
				{
					src << "layout (binding = 0, "<<formatQualifierStr<<") uniform "<<imageTypeStr<<"           u_image0;\n"
						<< "layout (binding = 1, "<<formatQualifierStr<<") readonly uniform "<<imageTypeStr<<"  u_image1;\n"
						<< "layout (binding = 2, "<<formatQualifierStr<<") writeonly uniform "<<imageTypeStr<<" u_image2;\n\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
						<< "    imageStore(u_image0, pos, imageLoad(u_image1, pos));\n"
						<< "    imageStore(u_image2, pos, imageLoad(u_image0, pos));\n"
						<< "}\n";

					break;
				}

				default:
					DE_ASSERT(false);
			}

			src_decompress	<< "layout (binding = 0) uniform "<<getGlslSamplerType(mapVkFormat(m_parameters.formatUncompressed), mapImageViewType(m_parameters.imageType))<<" compressed_result;\n"
							<< "layout (binding = 1) uniform "<<getGlslSamplerType(mapVkFormat(m_parameters.formatUncompressed), mapImageViewType(m_parameters.imageType))<<" compressed_reference;\n"
							<< "layout (binding = 2, "<<formatQualifierStr<<") writeonly uniform "<<imageTypeStr<<" decompressed_result;\n"
							<< "layout (binding = 3, "<<formatQualifierStr<<") writeonly uniform "<<imageTypeStr<<" decompressed_reference;\n\n"
							<< "void main (void)\n"
							<< "{\n"
							<< "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
							<< "    imageStore(decompressed_result, pos, texture(compressed_result, pos));\n"
							<< "    imageStore(decompressed_reference, pos, texture(compressed_reference, pos));\n"
							<< "}\n";
			programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
			programCollection.glslSources.add("decompress") << glu::ComputeSource(src_decompress.str());

			break;
		}

		case SHADER_TYPE_FRAGMENT:
		{
			DE_ASSERT(m_parameters.size[0] > 0);
			DE_ASSERT(m_parameters.size[1] > 0);

			// Vertex shader
			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n\n"
					<< "layout(location = 0) in vec4 v_in_position;\n"
					<< "\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "    gl_Position = v_in_position;\n"
					<< "}\n";

				programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
			}

			// Fragment shader
			{
				switch(m_parameters.operation)
				{
					case OPERATION_ATTACHMENT_READ:
					case OPERATION_ATTACHMENT_WRITE:
					{
						std::ostringstream	src;

						const std::string	dstTypeStr	= getGlslFormatType(m_parameters.formatUncompressed);
						const std::string	srcTypeStr	= getGlslInputFormatType(m_parameters.formatUncompressed);

						src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n\n"
							<< "precision highp int;\n"
							<< "precision highp float;\n"
							<< "\n"
							<< "layout (location = 0) out highp " << dstTypeStr << " o_color;\n"
							<< "layout (input_attachment_index = 0, set = 0, binding = 0) uniform highp " << srcTypeStr << " inputImage1;\n"
							<< "\n"
							<< "void main (void)\n"
							<< "{\n"
							<< "    o_color = " << dstTypeStr << "(subpassLoad(inputImage1));\n"
							<< "}\n";

						programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());

						break;
					}

					case OPERATION_TEXTURE_READ:
					case OPERATION_TEXTURE_WRITE:
					{
						std::ostringstream	src;

						const std::string	srcSamplerTypeStr		= getGlslSamplerType(mapVkFormat(m_parameters.formatUncompressed), mapImageViewType(m_parameters.imageType));
						const std::string	dstImageTypeStr			= getShaderImageType(mapVkFormat(m_parameters.formatUncompressed), m_parameters.imageType);
						const std::string	dstFormatQualifierStr	= getShaderImageFormatQualifier(mapVkFormat(m_parameters.formatUncompressed));
						const UVec3			uncompressedImageRes	= getCompressedImageResolutionInBlocks(m_parameters.formatCompressed, m_parameters.size);

						src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n\n"
							<< "layout (binding = 0) uniform " << srcSamplerTypeStr << " u_imageIn;\n"
							<< "layout (binding = 1, " << dstFormatQualifierStr << ") writeonly uniform " << dstImageTypeStr << " u_imageOut;\n"
							<< "\n"
							<< "void main (void)\n"
							<< "{\n"
							<< "    const ivec2 out_pos = ivec2(gl_FragCoord.xy);\n"
							<< "    const ivec2 pixels_resolution = ivec2(" << uncompressedImageRes[0] - 1 << ", " << uncompressedImageRes[1] - 1 << ");\n"
							<< "    const vec2 in_pos = vec2(out_pos) / vec2(pixels_resolution);\n"
							<< "    imageStore(u_imageOut, out_pos, texture(u_imageIn, in_pos));\n"
							<< "}\n";

						programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());

						break;
					}

					default:
						DE_ASSERT(false);
				}
			}

			// Verification fragment shader
			{
				switch(m_parameters.operation)
				{
					case OPERATION_ATTACHMENT_WRITE:
					case OPERATION_TEXTURE_WRITE:
					{
						std::ostringstream	src;

						const std::string	samplerType			= getGlslSamplerType(mapVkFormat(m_parameters.formatForVerify), mapImageViewType(m_parameters.imageType));
						const std::string	imageTypeStr		= getShaderImageType(mapVkFormat(m_parameters.formatForVerify), m_parameters.imageType);
						const std::string	formatQualifierStr	= getShaderImageFormatQualifier(mapVkFormat(m_parameters.formatForVerify));

						src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n\n"
							<< "layout (binding = 0) uniform " << samplerType << " u_imageIn0;\n"
							<< "layout (binding = 1) uniform " << samplerType << " u_imageIn1;\n"
							<< "layout (binding = 2, " << formatQualifierStr << ") writeonly uniform " << imageTypeStr << " u_imageOut0;\n"
							<< "layout (binding = 3, " << formatQualifierStr << ") writeonly uniform " << imageTypeStr << " u_imageOut1;\n"
							<< "\n"
							<< "void main (void)\n"
							<< "{\n"
							<< "    const ivec2 out_pos = ivec2(gl_FragCoord.xy);\n"
							<< "    const ivec2 pixels_resolution = ivec2(" << m_parameters.size[0] - 1 << ", " << m_parameters.size[1] - 1 << ");\n"
							<< "    const vec2 in_pos = vec2(out_pos) / vec2(pixels_resolution);\n"
							<< "    imageStore(u_imageOut0, out_pos, texture(u_imageIn0, in_pos));\n"
							<< "    imageStore(u_imageOut1, out_pos, texture(u_imageIn1, in_pos));\n"
							<< "}\n";

						programCollection.glslSources.add("frag_verify") << glu::FragmentSource(src.str());

						break;
					}

					case OPERATION_ATTACHMENT_READ:
					case OPERATION_TEXTURE_READ:
						// Read operations do not have sampling verification
						break;

					default:
						DE_ASSERT(false);
				}
			}

			break;
		}

		default:
			DE_ASSERT(false);
	}
}

TestInstance* TexelViewCompatibleCase::createInstance (Context& context) const
{
	const VkPhysicalDevice			physicalDevice			= context.getPhysicalDevice();
	const InstanceInterface&		vk						= context.getInstanceInterface();

	DE_ASSERT(getNumLayers(m_parameters.imageType, m_parameters.size)     == 1u);
	DE_ASSERT(getLayerSize(m_parameters.imageType, m_parameters.size).z() == 1u);
	DE_ASSERT(getLayerSize(m_parameters.imageType, m_parameters.size).x() >  0u);
	DE_ASSERT(getLayerSize(m_parameters.imageType, m_parameters.size).y() >  0u);

	if (std::find(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_KHR_maintenance2") == context.getDeviceExtensions().end())
		TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance2 not supported");

	{
		VkImageFormatProperties imageFormatProperties;

		if (VK_ERROR_FORMAT_NOT_SUPPORTED == vk.getPhysicalDeviceImageFormatProperties(physicalDevice, m_parameters.formatUncompressed,
												mapImageType(m_parameters.imageType), VK_IMAGE_TILING_OPTIMAL,
												m_parameters.uncompressedImageUsage, 0u, &imageFormatProperties))
			TCU_THROW(NotSupportedError, "Operation not supported with this image format");

		if (VK_ERROR_FORMAT_NOT_SUPPORTED == vk.getPhysicalDeviceImageFormatProperties(physicalDevice, m_parameters.formatCompressed,
												mapImageType(m_parameters.imageType), VK_IMAGE_TILING_OPTIMAL,
												VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
												VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT_KHR | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR,
												&imageFormatProperties))
			TCU_THROW(NotSupportedError, "Operation not supported with this image format");
	}

	{
		const VkPhysicalDeviceFeatures	physicalDeviceFeatures	= getPhysicalDeviceFeatures (vk, physicalDevice);

		if (deInRange32(m_parameters.formatCompressed, VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK) &&
			!physicalDeviceFeatures.textureCompressionBC)
			TCU_THROW(NotSupportedError, "textureCompressionBC not supported");

		if (deInRange32(m_parameters.formatCompressed, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, VK_FORMAT_EAC_R11G11_SNORM_BLOCK) &&
			!physicalDeviceFeatures.textureCompressionETC2)
			TCU_THROW(NotSupportedError, "textureCompressionETC2 not supported");

		if (deInRange32(m_parameters.formatCompressed, VK_FORMAT_ASTC_4x4_UNORM_BLOCK, VK_FORMAT_ASTC_12x12_SRGB_BLOCK) &&
			!physicalDeviceFeatures.textureCompressionASTC_LDR)
			TCU_THROW(NotSupportedError, "textureCompressionASTC_LDR not supported");
	}

	switch (m_parameters.shader)
	{
		case SHADER_TYPE_COMPUTE:
		{
			switch (m_parameters.operation)
			{
				case OPERATION_IMAGE_LOAD:
				case OPERATION_TEXEL_FETCH:
				case OPERATION_TEXTURE:
					return new BasicComputeTestInstance(context, m_parameters);
				case OPERATION_IMAGE_STORE:
					return new ImageStoreComputeTestInstance(context, m_parameters);
				default:
					TCU_THROW(InternalError, "Impossible");
			}
		}

		case SHADER_TYPE_FRAGMENT:
		{
			switch (m_parameters.operation)
			{
				case OPERATION_ATTACHMENT_READ:
				case OPERATION_ATTACHMENT_WRITE:
					return new GraphicsAttachmentsTestInstance(context, m_parameters);

				case OPERATION_TEXTURE_READ:
				case OPERATION_TEXTURE_WRITE:
					return new GraphicsTextureTestInstance(context, m_parameters);

				default:
					TCU_THROW(InternalError, "Impossible");
			}
		}

		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

} // anonymous ns


tcu::TestCaseGroup* createImageCompressionTranscodingTests (tcu::TestContext& testCtx)
{
	MovePtr<tcu::TestCaseGroup>	texelViewCompatibleTests						(new tcu::TestCaseGroup(testCtx, "texel_view_compatible", "Texel view compatible cases"));

	struct FormatsArray
	{
		const VkFormat*	formats;
		deUint32		count;
	};

	const std::string			pipelineName[SHADER_TYPE_LAST]					=
	{
		"compute",
		"graphic",
	};

	const std::string			operationName[OPERATION_LAST]					=
	{
		"image_load",
		"texel_fetch",
		"texture",
		"image_store",
		"attachment_read",
		"attachment_write",
		"texture_read",
		"texture_write",
	};

	const VkImageUsageFlags		baseImageUsageFlagSet							= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageUsageFlags		compressedImageUsageFlags[OPERATION_LAST]		=
	{
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_STORAGE_BIT),											// "image_load"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT),				// "texel_fetch"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT),				// "texture"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT),				// "image_store"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),	// "attachment_read"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),	// "attachment_write"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT),											// "texture_read"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT),				// "texture_write"
	};

	const VkImageUsageFlags		uncompressedImageUsageFlags[OPERATION_LAST]		=
	{
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_STORAGE_BIT),											//"image_load"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT),				//"texel_fetch"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT),				//"texture"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT),				//"image_store"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),	//"attachment_read"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT),									//"attachment_write"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),				//"texture_read"
		baseImageUsageFlagSet | static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT),											//"texture_write"
	};

	const VkFormat				compressedFormats64bit[]						=
	{
		VK_FORMAT_BC1_RGB_UNORM_BLOCK,
		VK_FORMAT_BC1_RGB_SRGB_BLOCK,
		VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
		VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
		VK_FORMAT_BC4_UNORM_BLOCK,
		VK_FORMAT_BC4_SNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		VK_FORMAT_EAC_R11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11_SNORM_BLOCK,
	};

	const VkFormat				compressedFormats128bit[]						=
	{
		VK_FORMAT_BC2_UNORM_BLOCK,
		VK_FORMAT_BC2_SRGB_BLOCK,
		VK_FORMAT_BC3_UNORM_BLOCK,
		VK_FORMAT_BC3_SRGB_BLOCK,
		VK_FORMAT_BC5_UNORM_BLOCK,
		VK_FORMAT_BC5_SNORM_BLOCK,
		VK_FORMAT_BC6H_UFLOAT_BLOCK,
		VK_FORMAT_BC6H_SFLOAT_BLOCK,
		VK_FORMAT_BC7_UNORM_BLOCK,
		VK_FORMAT_BC7_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
	};

	const VkFormat				uncompressedFormats64bit[]						=
	{
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		//VK_FORMAT_R64_UINT, remove from the test it couln'd not be use
		//VK_FORMAT_R64_SINT, remove from the test it couln'd not be use
		//VK_FORMAT_R64_SFLOAT, remove from the test it couln'd not be use
	};

	const VkFormat				uncompressedFormats128bit[]						=
	{
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		//VK_FORMAT_R64G64_UINT, remove from the test it couln'd not be use
		//VK_FORMAT_R64G64_SINT, remove from the test it couln'd not be use
		//VK_FORMAT_R64G64_SFLOAT, remove from the test it couln'd not be use
	};

	const FormatsArray			formatsCompressed[]								=
	{
		{
			compressedFormats64bit,
			DE_LENGTH_OF_ARRAY(compressedFormats64bit)
		},
		{
			compressedFormats128bit,
			DE_LENGTH_OF_ARRAY(compressedFormats128bit)
		},
	};

	const FormatsArray			formatsUncompressed[]							=
	{
		{
			uncompressedFormats64bit,
			DE_LENGTH_OF_ARRAY(uncompressedFormats64bit)
		},
		{
			uncompressedFormats128bit,
			DE_LENGTH_OF_ARRAY(uncompressedFormats128bit)
		},
	};

	const bool					mipmapTest										= false; // TODO
	const deUint32				unniceMipmapTextureSize[]						= { 1, 1, 1, 2, 6, 8, 21, 51, 92, 209, 295, 512, 1134 };

	DE_ASSERT(DE_LENGTH_OF_ARRAY(formatsCompressed) == DE_LENGTH_OF_ARRAY(formatsUncompressed));

	for (int shaderType = SHADER_TYPE_COMPUTE; shaderType < SHADER_TYPE_LAST; ++shaderType)
	{
		MovePtr<tcu::TestCaseGroup>	pipelineTypeGroup	(new tcu::TestCaseGroup(testCtx, pipelineName[shaderType].c_str(), ""));

		for (int operationNdx = OPERATION_IMAGE_LOAD; operationNdx < OPERATION_LAST; ++operationNdx)
		{
			if (shaderType != SHADER_TYPE_FRAGMENT && deInRange32(operationNdx, OPERATION_ATTACHMENT_READ, OPERATION_TEXTURE_WRITE))
				continue;

			if (shaderType != SHADER_TYPE_COMPUTE && deInRange32(operationNdx, OPERATION_IMAGE_LOAD, OPERATION_IMAGE_STORE))
				continue;

			MovePtr<tcu::TestCaseGroup>	imageOperationGroup	(new tcu::TestCaseGroup(testCtx, operationName[operationNdx].c_str(), ""));

			// Iterate through bitness groups (64 bit, 128 bit, etc)
			for (deUint32 formatBitnessGroup = 0; formatBitnessGroup < DE_LENGTH_OF_ARRAY(formatsCompressed); ++formatBitnessGroup)
			{
				for (deUint32 formatCompressedNdx = 0; formatCompressedNdx < formatsCompressed[formatBitnessGroup].count; ++formatCompressedNdx)
				{
					const VkFormat				formatCompressed			= formatsCompressed[formatBitnessGroup].formats[formatCompressedNdx];
					const std::string			compressedFormatGroupName	= getFormatShortString(formatCompressed);
					MovePtr<tcu::TestCaseGroup>	compressedFormatGroup		(new tcu::TestCaseGroup(testCtx, compressedFormatGroupName.c_str(), ""));

					for (deUint32 formatUncompressedNdx = 0; formatUncompressedNdx < formatsUncompressed[formatBitnessGroup].count; ++formatUncompressedNdx)
					{
						const VkFormat			formatUncompressed			= formatsUncompressed[formatBitnessGroup].formats[formatUncompressedNdx];
						const std::string		uncompressedFormatGroupName	= getFormatShortString(formatUncompressed);
						const deUint32			testTextureWidth			= mipmapTest
																			? unniceMipmapTextureSize[getBlockWidth(formatCompressed)]
																			: 64u;
						const deUint32			testTextureHeight			= mipmapTest
																			? unniceMipmapTextureSize[getBlockWidth(formatCompressed)]
																			: 64u;
						const TestParameters	parameters					=
						{
							static_cast<Operation>(operationNdx),
							static_cast<ShaderType>(shaderType),
							UVec3(testTextureWidth, testTextureHeight, 1u),
							IMAGE_TYPE_2D,
							formatCompressed,
							formatUncompressed,
							(operationNdx == OPERATION_IMAGE_STORE) ? 3u : 2u,
							compressedImageUsageFlags[operationNdx],
							uncompressedImageUsageFlags[operationNdx],
							false,
							VK_FORMAT_R8G8B8A8_UNORM
						};

						compressedFormatGroup->addChild(new TexelViewCompatibleCase(testCtx, uncompressedFormatGroupName, "", parameters));
					}

					imageOperationGroup->addChild(compressedFormatGroup.release());
				}
			}

			pipelineTypeGroup->addChild(imageOperationGroup.release());
		}

		texelViewCompatibleTests->addChild(pipelineTypeGroup.release());
	}

	return texelViewCompatibleTests.release();
}

} // image
} // vkt
