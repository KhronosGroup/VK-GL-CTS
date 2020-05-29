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
 * \file
 * \brief Image Tests Utility Classes
 *//*--------------------------------------------------------------------*/

#include "vktImageTestsUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTextureUtil.hpp"

using namespace vk;

namespace vkt
{
namespace image
{

Buffer::Buffer (const DeviceInterface&		vk,
				const VkDevice				device,
				Allocator&					allocator,
				const VkBufferCreateInfo&	bufferCreateInfo,
				const MemoryRequirement		memoryRequirement)
{
	m_buffer = createBuffer(vk, device, &bufferCreateInfo);
	m_allocation = allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement);
	VK_CHECK(vk.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
}

Image::Image (const DeviceInterface&	vk,
			  const VkDevice			device,
			  Allocator&				allocator,
			  const VkImageCreateInfo&	imageCreateInfo,
			  const MemoryRequirement	memoryRequirement)
{
	m_image = createImage(vk, device, &imageCreateInfo);
	m_allocation = allocator.allocate(getImageMemoryRequirements(vk, device, *m_image), memoryRequirement);
	VK_CHECK(vk.bindImageMemory(device, *m_image, m_allocation->getMemory(), m_allocation->getOffset()));
}

tcu::UVec3 getShaderGridSize (const ImageType imageType, const tcu::UVec3& imageSize)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return tcu::UVec3(imageSize.x(), 1u, 1u);

		case IMAGE_TYPE_1D_ARRAY:
			return tcu::UVec3(imageSize.x(), imageSize.z(), 1u);

		case IMAGE_TYPE_2D:
			return tcu::UVec3(imageSize.x(), imageSize.y(), 1u);

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_3D:
			return tcu::UVec3(imageSize.x(), imageSize.y(), imageSize.z());

		case IMAGE_TYPE_CUBE:
			return tcu::UVec3(imageSize.x(), imageSize.y(), 6u);

		case IMAGE_TYPE_CUBE_ARRAY:
			return tcu::UVec3(imageSize.x(), imageSize.y(), 6u * imageSize.z());

		default:
			DE_FATAL("Unknown image type");
			return tcu::UVec3(1u, 1u, 1u);
	}
}

tcu::UVec3 getLayerSize (const ImageType imageType, const tcu::UVec3& imageSize)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_BUFFER:
			return tcu::UVec3(imageSize.x(), 1u, 1u);

		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return tcu::UVec3(imageSize.x(), imageSize.y(), 1u);

		case IMAGE_TYPE_3D:
			return tcu::UVec3(imageSize.x(), imageSize.y(), imageSize.z());

		default:
			DE_FATAL("Unknown image type");
			return tcu::UVec3(1u, 1u, 1u);
	}
}

deUint32 getNumLayers (const ImageType imageType, const tcu::UVec3& imageSize)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_3D:
		case IMAGE_TYPE_BUFFER:
			return 1u;

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D_ARRAY:
			return imageSize.z();

		case IMAGE_TYPE_CUBE:
			return 6u;

		case IMAGE_TYPE_CUBE_ARRAY:
			return imageSize.z() * 6u;

		default:
			DE_FATAL("Unknown image type");
			return 0u;
	}
}

deUint32 getNumPixels (const ImageType imageType, const tcu::UVec3& imageSize)
{
	const tcu::UVec3 gridSize = getShaderGridSize(imageType, imageSize);

	return gridSize.x() * gridSize.y() * gridSize.z();
}

deUint32 getDimensions (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return 1u;

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D:
			return 2u;

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
		case IMAGE_TYPE_3D:
			return 3u;

		default:
			DE_FATAL("Unknown image type");
			return 0u;
	}
}

deUint32 getLayerDimensions (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
		case IMAGE_TYPE_1D_ARRAY:
			return 1u;

		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return 2u;

		case IMAGE_TYPE_3D:
			return 3u;

		default:
			DE_FATAL("Unknown image type");
			return 0u;
	}
}

VkBufferImageCopy makeBufferImageCopy (const VkExtent3D extent,
									   const deUint32	arraySize)
{
	const VkBufferImageCopy copyParams =
	{
		0ull,																		//	VkDeviceSize				bufferOffset;
		0u,																			//	deUint32					bufferRowLength;
		0u,																			//	deUint32					bufferImageHeight;
		makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, arraySize),	//	VkImageSubresourceLayers	imageSubresource;
		makeOffset3D(0, 0, 0),														//	VkOffset3D					imageOffset;
		extent,																		//	VkExtent3D					imageExtent;
	};
	return copyParams;
}

Move<VkPipeline> makeComputePipeline (const DeviceInterface&	vk,
									  const VkDevice			device,
									  const VkPipelineLayout	pipelineLayout,
									  const VkShaderModule		shaderModule)
{
	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		shaderModule,											// VkShaderModule						module;
		"main",													// const char*							pName;
		DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};
	return createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo);
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&	vk,
									   const VkDevice			device,
									   const VkPipelineLayout	pipelineLayout,
									   const VkRenderPass		renderPass,
									   const VkShaderModule		vertexModule,
									   const VkShaderModule		fragmentModule,
									   const VkExtent2D			renderSize,
									   const deUint32			colorAttachmentCount,
									   const bool				dynamicSize)
{
	std::vector<VkViewport>								viewports;
	std::vector<VkRect2D>								scissors;

	const VkViewport									viewport						= makeViewport(renderSize);
	const VkRect2D										scissor							= makeRect2D(renderSize);

	const VkFormat										vertexFormatPosition			= VK_FORMAT_R32G32B32A32_SFLOAT;
	const deUint32										vertexSizePosition				= tcu::getPixelSize(mapVkFormat(vertexFormatPosition));
	const deUint32										vertexBufferOffsetPosition		= 0u;
	const deUint32										vertexDataStride				= vertexSizePosition;

	if (!dynamicSize)
	{
		viewports.push_back(viewport);
		scissors.push_back(scissor);
	}

	const VkVertexInputBindingDescription				vertexInputBindingDescription	=
	{
		0u,							// deUint32             binding;
		vertexDataStride,			// deUint32             stride;
		VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputRate    inputRate;
	};

	const VkVertexInputAttributeDescription				vertexInputAttributeDescription	=
	{
		0u,							// deUint32    location;
		0u,							// deUint32    binding;
		vertexFormatPosition,		// VkFormat    format;
		vertexBufferOffsetPosition,	// deUint32    offset;
	};

	const VkPipelineVertexInputStateCreateInfo			vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags;
		1u,															// deUint32                                    vertexBindingDescriptionCount;
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		1u,															// deUint32                                    vertexAttributeDescriptionCount;
		&vertexInputAttributeDescription							// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkColorComponentFlags							colorComponentsAll				= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	const VkPipelineColorBlendAttachmentState			colorBlendAttachmentState		=
	{
		VK_FALSE,				// VkBool32                 blendEnable;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor            srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor            dstColorBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp                colorBlendOp;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor            srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor            dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp                alphaBlendOp;
		colorComponentsAll		// VkColorComponentFlags    colorWriteMask;
	};

	std::vector<VkPipelineColorBlendAttachmentState>	colorAttachments				(colorAttachmentCount, colorBlendAttachmentState);

	const VkPipelineColorBlendStateCreateInfo			pipelineColorBlendStateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType                              sType;
		DE_NULL,														// const void*                                  pNext;
		(VkPipelineColorBlendStateCreateFlags)0,						// VkPipelineColorBlendStateCreateFlags         flags;
		VK_FALSE,														// VkBool32                                     logicOpEnable;
		VK_LOGIC_OP_COPY,												// VkLogicOp                                    logicOp;
		(deUint32)colorAttachments.size(),								// deUint32                                     attachmentCount;
		colorAttachments.size() != 0 ? &colorAttachments[0] : DE_NULL,	// const VkPipelineColorBlendAttachmentState*   pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f }										// float                                        blendConstants[4];
	};

	return vk::makeGraphicsPipeline(vk,										// const DeviceInterface&                        vk
									device,									// const VkDevice                                device
									pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
									vertexModule,							// const VkShaderModule                          vertexShaderModule
									DE_NULL,								// const VkShaderModule                          tessellationControlModule
									DE_NULL,								// const VkShaderModule                          tessellationEvalModule
									DE_NULL,								// const VkShaderModule                          geometryShaderModule
									fragmentModule,							// const VkShaderModule                          fragmentShaderModule
									renderPass,								// const VkRenderPass                            renderPass
									viewports,								// const std::vector<VkViewport>&                viewports
									scissors,								// const std::vector<VkRect2D>&                  scissors
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
									0u,										// const deUint32                                subpass
									0u,										// const deUint32                                patchControlPoints
									&vertexInputStateCreateInfo,			// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
									DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
									DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
									DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
									&pipelineColorBlendStateInfo);			// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
}

//! A single-subpass render pass.
Move<VkRenderPass> makeRenderPass (const DeviceInterface&	vk,
								   const VkDevice			device,
								   const VkFormat			inputFormat,
								   const VkFormat			colorFormat)
{
	const VkAttachmentReference		inputAttachmentRef			=
	{
		0u,															// deUint32			attachment;
		VK_IMAGE_LAYOUT_GENERAL										// VkImageLayout	layout;
	};

	const VkAttachmentReference		colorAttachmentRef			=
	{
		1u,															// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL					// VkImageLayout	layout;
	};

	const VkSubpassDescription		subpassDescription			=
	{
		(VkSubpassDescriptionFlags)0,								// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,							// VkPipelineBindPoint				pipelineBindPoint;
		1u,															// deUint32							inputAttachmentCount;
		&inputAttachmentRef,										// const VkAttachmentReference*		pInputAttachments;
		1u,															// deUint32							colorAttachmentCount;
		&colorAttachmentRef,										// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,													// const VkAttachmentReference*		pResolveAttachments;
		DE_NULL,													// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,															// deUint32							preserveAttachmentCount;
		DE_NULL														// const deUint32*					pPreserveAttachments;
	};

	const VkAttachmentDescription	attachmentsDescriptions[]	=
	{
		//inputAttachmentDescription,
		{
			(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags		flags;
			inputFormat,											// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_LOAD,								// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_GENERAL,								// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_GENERAL,								// VkImageLayout					finalLayout;
		},
		//colorAttachmentDescription
		{
			(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags		flags;
			colorFormat,											// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,							// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_GENERAL,								// VkImageLayout					finalLayout;
		}
	};

	const VkRenderPassCreateInfo	renderPassInfo				=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,					// VkStructureType					sType;
		DE_NULL,													// const void*						pNext;
		(VkRenderPassCreateFlags)0,									// VkRenderPassCreateFlags			flags;
		DE_LENGTH_OF_ARRAY(attachmentsDescriptions),				// deUint32							attachmentCount;
		attachmentsDescriptions,									// const VkAttachmentDescription*	pAttachments;
		1u,															// deUint32							subpassCount;
		&subpassDescription,										// const VkSubpassDescription*		pSubpasses;
		0u,															// deUint32							dependencyCount;
		DE_NULL														// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

VkImageViewUsageCreateInfo makeImageViewUsageCreateInfo (const VkImageUsageFlags imageUsageFlags)
{
	VkImageViewUsageCreateInfo imageViewUsageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO_KHR,	//VkStructureType		sType;
		DE_NULL,											//const void*			pNext;
		imageUsageFlags,									//VkImageUsageFlags		usage;
	};

	return imageViewUsageCreateInfo;
}

VkSamplerCreateInfo makeSamplerCreateInfo ()
{
	const VkSamplerCreateInfo defaultSamplerParams =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkSamplerCreateFlags		flags;
		VK_FILTER_NEAREST,							// VkFilter					magFilter;
		VK_FILTER_NEAREST,							// VkFilter					minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeW;
		0.0f,										// float					mipLodBias;
		VK_FALSE,									// VkBool32					anisotropyEnable;
		1.0f,										// float					maxAnisotropy;
		VK_FALSE,									// VkBool32					compareEnable;
		VK_COMPARE_OP_NEVER,						// VkCompareOp				compareOp;
		0.0f,										// float					minLod;
		0.25f,										// float					maxLod;
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// VkBorderColor			borderColor;
		VK_FALSE									// VkBool32					unnormalizedCoordinates;
	};

	return defaultSamplerParams;
}

tcu::UVec3 getCompressedImageResolutionInBlocks (const vk::VkFormat format, const tcu::UVec3& size)
{
	deUint32	blockWidth	= getBlockWidth(format);
	deUint32	blockHeight	= getBlockHeight(format);

	DE_ASSERT(size[2] == 1);
	DE_ASSERT(blockWidth != 0 && blockHeight != 0);

	deUint32	widthInBlocks	= (size[0] + blockWidth - 1) / blockWidth;
	deUint32	heightInBlocks	= (size[1] + blockHeight - 1) / blockHeight;

	return tcu::UVec3(widthInBlocks, heightInBlocks, 1);
}

tcu::UVec3 getCompressedImageResolutionBlockCeil (const vk::VkFormat format, const tcu::UVec3& size)
{
	deUint32	blockWidth	= getBlockWidth(format);
	deUint32	blockHeight	= getBlockHeight(format);

	DE_ASSERT(size[2] == 1);
	DE_ASSERT(blockWidth != 0 && blockHeight != 0);

	deUint32	widthInBlocks	= (size[0] + blockWidth - 1) / blockWidth;
	deUint32	heightInBlocks	= (size[1] + blockHeight - 1) / blockHeight;

	return tcu::UVec3(blockWidth * widthInBlocks, blockHeight * heightInBlocks, 1);
}

VkDeviceSize getCompressedImageSizeInBytes (const vk::VkFormat format, const tcu::UVec3& size)
{
	tcu::UVec3		sizeInBlocks	= getCompressedImageResolutionInBlocks(format, size);
	deUint32		blockBytes		= getBlockSizeInBytes(format);
	VkDeviceSize	sizeBytes		= sizeInBlocks[0] * sizeInBlocks[1] * sizeInBlocks[2] * blockBytes;

	return sizeBytes;
}

VkDeviceSize getUncompressedImageSizeInBytes (const vk::VkFormat format, const tcu::UVec3& size)
{
	const tcu::IVec3	sizeAsIVec3	= tcu::IVec3((int)size.x(), (int)size.y(), (int)size.z());
	const VkDeviceSize	sizeBytes	= getImageSizeBytes(sizeAsIVec3, format);

	return sizeBytes;
}

VkImageType	mapImageType (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_BUFFER:
			return VK_IMAGE_TYPE_1D;

		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return VK_IMAGE_TYPE_2D;

		case IMAGE_TYPE_3D:
			return VK_IMAGE_TYPE_3D;

		default:
			DE_ASSERT(false);
			return VK_IMAGE_TYPE_LAST;
	}
}

VkImageViewType	mapImageViewType (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:			return VK_IMAGE_VIEW_TYPE_1D;
		case IMAGE_TYPE_1D_ARRAY:	return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		case IMAGE_TYPE_2D:			return VK_IMAGE_VIEW_TYPE_2D;
		case IMAGE_TYPE_2D_ARRAY:	return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		case IMAGE_TYPE_3D:			return VK_IMAGE_VIEW_TYPE_3D;
		case IMAGE_TYPE_CUBE:		return VK_IMAGE_VIEW_TYPE_CUBE;
		case IMAGE_TYPE_CUBE_ARRAY:	return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

		default:
			DE_ASSERT(false);
			return VK_IMAGE_VIEW_TYPE_LAST;
	}
}

std::string getImageTypeName (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:			return "1d";
		case IMAGE_TYPE_1D_ARRAY:	return "1d_array";
		case IMAGE_TYPE_2D:			return "2d";
		case IMAGE_TYPE_2D_ARRAY:	return "2d_array";
		case IMAGE_TYPE_3D:			return "3d";
		case IMAGE_TYPE_CUBE:		return "cube";
		case IMAGE_TYPE_CUBE_ARRAY:	return "cube_array";
		case IMAGE_TYPE_BUFFER:		return "buffer";

		default:
			DE_ASSERT(false);
			return "";
	}
}

std::string getFormatPrefix (const tcu::TextureFormat& format)
{
	const std::string image64 = ((mapTextureFormat(format) == VK_FORMAT_R64_UINT || mapTextureFormat(format) == VK_FORMAT_R64_SINT) ? "64" : "");
	return tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ? "u" + image64 :
		   tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER   ? "i" + image64 : "";
}

std::string getShaderImageType (const tcu::TextureFormat& format, const ImageType imageType, const bool multisample)
{
	std::string formatPart = getFormatPrefix(format);

	std::string imageTypePart;
	if (multisample)
	{
		switch (imageType)
		{
			case IMAGE_TYPE_2D:			imageTypePart = "2DMS";			break;
			case IMAGE_TYPE_2D_ARRAY:	imageTypePart = "2DMSArray";	break;

			default:
				DE_ASSERT(false);
		}
	}
	else
	{
		switch (imageType)
		{
			case IMAGE_TYPE_1D:			imageTypePart = "1D";			break;
			case IMAGE_TYPE_1D_ARRAY:	imageTypePart = "1DArray";		break;
			case IMAGE_TYPE_2D:			imageTypePart = "2D";			break;
			case IMAGE_TYPE_2D_ARRAY:	imageTypePart = "2DArray";		break;
			case IMAGE_TYPE_3D:			imageTypePart = "3D";			break;
			case IMAGE_TYPE_CUBE:		imageTypePart = "Cube";			break;
			case IMAGE_TYPE_CUBE_ARRAY:	imageTypePart = "CubeArray";	break;
			case IMAGE_TYPE_BUFFER:		imageTypePart = "Buffer";		break;

			default:
				DE_ASSERT(false);
		}
	}

	return formatPart + "image" + imageTypePart;
}

std::string getShaderImageFormatQualifier (const tcu::TextureFormat& format)
{
	if (!isPackedType(mapTextureFormat(format)))
	{
		const char* orderPart;
		const char* typePart;

		switch (format.order)
		{
			case tcu::TextureFormat::R:		orderPart = "r";	break;
			case tcu::TextureFormat::RG:	orderPart = "rg";	break;
			case tcu::TextureFormat::RGB:	orderPart = "rgb";	break;
			case tcu::TextureFormat::RGBA:	orderPart = "rgba";	break;
			case tcu::TextureFormat::sRGBA:	orderPart = "rgba";	break;

			default:
				DE_FATAL("Order not found");
				orderPart = DE_NULL;
		}

		switch (format.type)
		{
			case tcu::TextureFormat::FLOAT:				typePart = "32f";		break;
			case tcu::TextureFormat::HALF_FLOAT:		typePart = "16f";		break;

			case tcu::TextureFormat::UNSIGNED_INT64:	typePart = "64ui";		break;
			case tcu::TextureFormat::UNSIGNED_INT32:	typePart = "32ui";		break;
			case tcu::TextureFormat::USCALED_INT16:
			case tcu::TextureFormat::UNSIGNED_INT16:	typePart = "16ui";		break;
			case tcu::TextureFormat::USCALED_INT8:
			case tcu::TextureFormat::UNSIGNED_INT8:		typePart = "8ui";		break;

			case tcu::TextureFormat::SIGNED_INT64:		typePart = "64i";		break;
			case tcu::TextureFormat::SIGNED_INT32:		typePart = "32i";		break;
			case tcu::TextureFormat::SSCALED_INT16:
			case tcu::TextureFormat::SIGNED_INT16:		typePart = "16i";		break;
			case tcu::TextureFormat::SSCALED_INT8:
			case tcu::TextureFormat::SIGNED_INT8:		typePart = "8i";		break;

			case tcu::TextureFormat::UNORM_INT16:		typePart = "16";		break;
			case tcu::TextureFormat::UNORM_INT8:		typePart = "8";			break;

			case tcu::TextureFormat::SNORM_INT16:		typePart = "16_snorm";	break;
			case tcu::TextureFormat::SNORM_INT8:		typePart = "8_snorm";	break;

			default:
				DE_FATAL("Type not found");
				typePart = DE_NULL;
		}

		return std::string() + orderPart + typePart;
	}
	else
	{
		switch (mapTextureFormat(format))
		{
			case VK_FORMAT_B10G11R11_UFLOAT_PACK32:		return "r11f_g11f_b10f";
			case VK_FORMAT_A2B10G10R10_UNORM_PACK32:	return "rgb10_a2";
			case VK_FORMAT_A2B10G10R10_UINT_PACK32:		return "rgb10_a2ui";

			default:
				DE_FATAL("Qualifier not found");
				return "";
		}
	}
}

std::string getGlslSamplerType (const tcu::TextureFormat& format, VkImageViewType type)
{
	const char* typePart	= DE_NULL;
	const char* formatPart	= tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ? "u" :
							  tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER   ? "i" : "";

	switch (type)
	{
		case VK_IMAGE_VIEW_TYPE_1D:			typePart = "sampler1D";			break;
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:	typePart = "sampler1DArray";	break;
		case VK_IMAGE_VIEW_TYPE_2D:			typePart = "sampler2D";			break;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:	typePart = "sampler2DArray";	break;
		case VK_IMAGE_VIEW_TYPE_3D:			typePart = "sampler3D";			break;
		case VK_IMAGE_VIEW_TYPE_CUBE:		typePart = "samplerCube";		break;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:	typePart = "samplerCubeArray";	break;

		default:
			DE_FATAL("Unknown image view type");
			break;
	}

	return std::string(formatPart) + typePart;
}


const char* getGlslInputFormatType (const vk::VkFormat format)
{
	switch (format)
	{
		// 64-bit
		case VK_FORMAT_R16G16B16A16_UNORM:		return "subpassInput";
		case VK_FORMAT_R16G16B16A16_SNORM:		return "subpassInput";
		case VK_FORMAT_R16G16B16A16_USCALED:	return "subpassInput";
		case VK_FORMAT_R16G16B16A16_SSCALED:	return "subpassInput";
		case VK_FORMAT_R16G16B16A16_UINT:		return "usubpassInput";
		case VK_FORMAT_R16G16B16A16_SINT:		return "isubpassInput";
		case VK_FORMAT_R16G16B16A16_SFLOAT:		return "subpassInput";
		case VK_FORMAT_R32G32_UINT:				return "usubpassInput";
		case VK_FORMAT_R32G32_SINT:				return "isubpassInput";
		case VK_FORMAT_R32G32_SFLOAT:			return "subpassInput";
		// TODO: case VK_FORMAT_R64_UINT:		return "usubpassInput";
		// TODO: case VK_FORMAT_R64_SINT:		return "isubpassInput";
		// TODO: case VK_FORMAT_R64_SFLOAT:		return "subpassInput";

		// 128-bit
		case VK_FORMAT_R32G32B32A32_UINT:		return "usubpassInput";
		case VK_FORMAT_R32G32B32A32_SINT:		return "isubpassInput";
		case VK_FORMAT_R32G32B32A32_SFLOAT:		return "subpassInput";
		// TODO: case VK_FORMAT_R64G64_UINT:	return "usubpassInput";
		// TODO: case VK_FORMAT_R64G64_SINT:	return "isubpassInput";
		// TODO: case VK_FORMAT_R64G64_SFLOAT:	return "subpassInput";

		default:	TCU_THROW(InternalError, "Unknown format");
	}
}

const char* getGlslFormatType (const vk::VkFormat format)
{
	switch (format)
	{
		// 64-bit
		case VK_FORMAT_R16G16B16A16_UNORM:		return "vec4";
		case VK_FORMAT_R16G16B16A16_SNORM:		return "vec4";
		case VK_FORMAT_R16G16B16A16_USCALED:	return "vec4";
		case VK_FORMAT_R16G16B16A16_SSCALED:	return "vec4";
		case VK_FORMAT_R16G16B16A16_UINT:		return "uvec4";
		case VK_FORMAT_R16G16B16A16_SINT:		return "ivec4";
		case VK_FORMAT_R16G16B16A16_SFLOAT:		return "vec4";
		case VK_FORMAT_R32G32_UINT:				return "uvec2";
		case VK_FORMAT_R32G32_SINT:				return "ivec2";
		case VK_FORMAT_R32G32_SFLOAT:			return "vec2";
		// TODO: case VK_FORMAT_R64_UINT:		return "uint64";
		// TODO: case VK_FORMAT_R64_SINT:		return "int64";
		// TODO: case VK_FORMAT_R64_SFLOAT:		return "double";

		// 128-bit
		case VK_FORMAT_R32G32B32A32_UINT:		return "uvec4";
		case VK_FORMAT_R32G32B32A32_SINT:		return "ivec4";
		case VK_FORMAT_R32G32B32A32_SFLOAT:		return "vec4";
		// TODO: case VK_FORMAT_R64G64_UINT:	return "ulvec2";
		// TODO: case VK_FORMAT_R64G64_SINT:	return "ilvec2";
		// TODO: case VK_FORMAT_R64G64_SFLOAT:	return "dvec2";

		default:	TCU_THROW(InternalError, "Unknown format");
	}
}

const char* getGlslAttachmentType (const vk::VkFormat format)
{
	const tcu::TextureFormat		textureFormat	= mapVkFormat(format);
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(textureFormat.type);

	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "ivec4";

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "uvec4";

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "vec4";

		default:
			DE_FATAL("Unknown channel class");
			return "";
	}
}

const char* getGlslInputAttachmentType (const vk::VkFormat format)
{
	const tcu::TextureFormat		textureFormat	= mapVkFormat(format);
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(textureFormat.type);

	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "isubpassInput";

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "usubpassInput";

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "subpassInput";

		default:
			DE_FATAL("Unknown channel class");
			return "";
	}
}

bool isPackedType (const vk::VkFormat format)
{
	const tcu::TextureFormat	textureFormat	= mapVkFormat(format);

	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELTYPE_LAST == 48);

	switch (textureFormat.type)
	{
		case tcu::TextureFormat::UNORM_BYTE_44:
		case tcu::TextureFormat::UNORM_SHORT_565:
		case tcu::TextureFormat::UNORM_SHORT_555:
		case tcu::TextureFormat::UNORM_SHORT_4444:
		case tcu::TextureFormat::UNORM_SHORT_5551:
		case tcu::TextureFormat::UNORM_SHORT_1555:
		case tcu::TextureFormat::UNORM_INT_101010:
		case tcu::TextureFormat::SNORM_INT_1010102_REV:
		case tcu::TextureFormat::UNORM_INT_1010102_REV:
		case tcu::TextureFormat::UNSIGNED_BYTE_44:
		case tcu::TextureFormat::UNSIGNED_SHORT_565:
		case tcu::TextureFormat::UNSIGNED_SHORT_4444:
		case tcu::TextureFormat::UNSIGNED_SHORT_5551:
		case tcu::TextureFormat::SIGNED_INT_1010102_REV:
		case tcu::TextureFormat::UNSIGNED_INT_1010102_REV:
		case tcu::TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
		case tcu::TextureFormat::UNSIGNED_INT_999_E5_REV:
		case tcu::TextureFormat::UNSIGNED_INT_16_8_8:
		case tcu::TextureFormat::UNSIGNED_INT_24_8:
		case tcu::TextureFormat::UNSIGNED_INT_24_8_REV:
		case tcu::TextureFormat::SSCALED_INT_1010102_REV:
		case tcu::TextureFormat::USCALED_INT_1010102_REV:
			return true;

		default:
			return false;
	}
}

bool isComponentSwizzled (const vk::VkFormat format)
{
	const tcu::TextureFormat	textureFormat	= mapVkFormat(format);

	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELORDER_LAST == 21);

	switch (textureFormat.order)
	{
		case tcu::TextureFormat::ARGB:
		case tcu::TextureFormat::BGR:
		case tcu::TextureFormat::BGRA:
		case tcu::TextureFormat::sBGR:
		case tcu::TextureFormat::sBGRA:
			return true;

		default:
			return false;
	}
}

int getNumUsedChannels (const vk::VkFormat format)
{
	// make sure this function will be checked if type table is updated
	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELORDER_LAST == 21);

	const tcu::TextureFormat	textureFormat	= mapVkFormat(format);

	return getNumUsedChannels(textureFormat.order);
}

bool isFormatImageLoadStoreCapable (const vk::VkFormat format)
{
	// These come from https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#spirvenv-image-formats
	switch (format)
	{
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R8_UINT:
			return true;

		default:
			return false;
	}
}

std::string getFormatShortString (const VkFormat format)
{
	const std::string fullName = getFormatName(format);

	DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

	return de::toLower(fullName.substr(10));
}

std::vector<tcu::Vec4> createFullscreenQuad (void)
{
	const tcu::Vec4 lowerLeftVertex		(-1.0f,	-1.0f,	0.0f,	1.0f);
	const tcu::Vec4 upperLeftVertex		(-1.0f,	1.0f,	0.0f,	1.0f);
	const tcu::Vec4 lowerRightVertex	(1.0f,	-1.0f,	0.0f,	1.0f);
	const tcu::Vec4 upperRightVertex	(1.0f,	1.0f,	0.0f,	1.0f);

	const tcu::Vec4 vertices[6] =
	{
		lowerLeftVertex,
		lowerRightVertex,
		upperLeftVertex,

		upperLeftVertex,
		lowerRightVertex,
		upperRightVertex
	};

	return std::vector<tcu::Vec4>(vertices, vertices + DE_LENGTH_OF_ARRAY(vertices));
}

vk::VkBufferImageCopy makeBufferImageCopy (const deUint32 imageWidth, const deUint32 imageHeight, const deUint32 mipLevel, const deUint32 layer)
{
	const VkBufferImageCopy	copyParams	=
	{
		(VkDeviceSize)0u,						// bufferOffset
		imageWidth,								// bufferRowLength
		imageHeight,							// bufferImageHeight
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// aspectMask
			mipLevel,								// mipLevel
			layer,									// baseArrayLayer
			1u,										// layerCount
		},										// imageSubresource
		{ 0u, 0u, 0u },							// imageOffset
		{
			imageWidth,
			imageHeight,
			1u
		}										// imageExtent
	};

	return copyParams;
}

vk::VkBufferImageCopy makeBufferImageCopy (const deUint32 imageWidth, const deUint32 imageHeight, const deUint32 mipLevel, const deUint32 layer, const deUint32 bufferRowLength, const deUint32 bufferImageHeight)
{
	const VkBufferImageCopy	copyParams	=
	{
		(VkDeviceSize)0u,						// bufferOffset
		bufferRowLength,						// bufferRowLength
		bufferImageHeight,						// bufferImageHeight
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// aspectMask
			mipLevel,								// mipLevel
			layer,									// baseArrayLayer
			1u,										// layerCount
		},										// imageSubresource
		{ 0u, 0u, 0u },							// imageOffset
		{
			imageWidth,
			imageHeight,
			1u
		}										// imageExtent
	};

	return copyParams;
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkExtent2D&			renderSize)
{
	const VkRect2D renderArea =
	{
		{0, 0},			// VkOffset2D				offset;
		renderSize,		// VkExtent2D				extent;
	};

	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, tcu::Vec4(0.0f), 0.0f, 0u);
}

} // image
} // vkt
