/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Image sampling case
 *//*--------------------------------------------------------------------*/

#include "vktPipelineImageSamplingInstance.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "tcuImageCompare.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;
using de::MovePtr;

namespace
{

static VkImageType getCompatibleImageType (VkImageViewType viewType)
{
	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:				return VK_IMAGE_TYPE_1D;
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:		return VK_IMAGE_TYPE_1D;
		case VK_IMAGE_VIEW_TYPE_2D:				return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:		return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_3D:				return VK_IMAGE_TYPE_3D;
		case VK_IMAGE_VIEW_TYPE_CUBE:			return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:		return VK_IMAGE_TYPE_2D;
		default:
			break;
	}

	DE_ASSERT(false);
	return VK_IMAGE_TYPE_1D;
}

template<typename TcuFormatType>
static MovePtr<TestTexture> createTestTexture (const TcuFormatType format, VkImageViewType viewType, const tcu::IVec3& size, int arraySize)
{
	MovePtr<TestTexture>	texture;
	const VkImageType		imageType = getCompatibleImageType(viewType);

	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:
			if (arraySize == 1)
				texture = MovePtr<TestTexture>(new TestTexture1D(format, size.x()));
			else
				texture = MovePtr<TestTexture>(new TestTexture1DArray(format, size.x(), arraySize));

			break;

		case VK_IMAGE_TYPE_2D:
			if (arraySize == 1)
			{
				texture = MovePtr<TestTexture>(new TestTexture2D(format, size.x(), size.y()));
			}
			else
			{
				if (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
				{
					if (arraySize == tcu::CUBEFACE_LAST)
					{
						texture = MovePtr<TestTexture>(new TestTextureCube(format, size.x()));
					}
					else
					{
						DE_ASSERT(arraySize % tcu::CUBEFACE_LAST == 0);

						texture = MovePtr<TestTexture>(new TestTextureCubeArray(format, size.x(), arraySize));
					}
				}
				else
				{
					texture = MovePtr<TestTexture>(new TestTexture2DArray(format, size.x(), size.y(), arraySize));
				}
			}

			break;

		case VK_IMAGE_TYPE_3D:
			texture = MovePtr<TestTexture>(new TestTexture3D(format, size.x(), size.y(), size.z()));
			break;

		default:
			DE_ASSERT(false);
	}

	return texture;
}

template<typename TcuTextureType>
static void copySubresourceRange (TcuTextureType& dest, const TcuTextureType& src, const VkImageSubresourceRange& subresourceRange)
{
	DE_ASSERT(subresourceRange.mipLevels <= (deUint32)dest.getNumLevels());
	DE_ASSERT(subresourceRange.baseMipLevel + subresourceRange.mipLevels <= (deUint32)src.getNumLevels());

	for (int levelNdx = 0; levelNdx < dest.getNumLevels(); levelNdx++)
	{
		const tcu::ConstPixelBufferAccess	srcLevel		(src.getLevel(subresourceRange.baseMipLevel + levelNdx));
		const deUint32						srcLayerOffset	= subresourceRange.baseArrayLayer * srcLevel.getWidth() * srcLevel.getHeight() * srcLevel.getFormat().getPixelSize();
		const tcu::ConstPixelBufferAccess	srcLevelLayers	(srcLevel.getFormat(), srcLevel.getWidth(), srcLevel.getHeight(), subresourceRange.arraySize, (deUint8*)srcLevel.getDataPtr() + srcLayerOffset);

		if (dest.isLevelEmpty(levelNdx))
			dest.allocLevel(levelNdx);

		tcu::copy(dest.getLevel(levelNdx), srcLevelLayers);
	}
}

template<>
void copySubresourceRange<tcu::Texture1DArray> (tcu::Texture1DArray& dest, const tcu::Texture1DArray& src, const VkImageSubresourceRange& subresourceRange)
{
	DE_ASSERT(subresourceRange.mipLevels <= (deUint32)dest.getNumLevels());
	DE_ASSERT(subresourceRange.baseMipLevel + subresourceRange.mipLevels <= (deUint32)src.getNumLevels());

	DE_ASSERT(subresourceRange.arraySize == (deUint32)dest.getNumLayers());
	DE_ASSERT(subresourceRange.baseArrayLayer + subresourceRange.arraySize <= (deUint32)src.getNumLayers());

	for (int levelNdx = 0; levelNdx < dest.getNumLevels(); levelNdx++)
	{
		const tcu::ConstPixelBufferAccess	srcLevel		(src.getLevel(subresourceRange.baseMipLevel + levelNdx));
		const deUint32						srcLayerOffset	= subresourceRange.baseArrayLayer * srcLevel.getWidth() * srcLevel.getFormat().getPixelSize();
		const tcu::ConstPixelBufferAccess	srcLevelLayers	(srcLevel.getFormat(), srcLevel.getWidth(), subresourceRange.arraySize, 1, (deUint8*)srcLevel.getDataPtr() + srcLayerOffset);

		if (dest.isLevelEmpty(levelNdx))
			dest.allocLevel(levelNdx);

		tcu::copy(dest.getLevel(levelNdx), srcLevelLayers);
	}
}

static MovePtr<Program> createRefProgram (const tcu::TextureFormat&			renderTargetFormat,
										  const tcu::Sampler&				sampler,
										  float								samplerLod,
										  const tcu::UVec4&					channelMapping,
										  const TestTexture&				testTexture,
										  VkImageViewType					viewType,
										  int								arraySize,
										  const VkImageSubresourceRange&	subresource)
{
	MovePtr<Program>	program;
	const VkImageType	imageType	= getCompatibleImageType(viewType);

	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:
			if (arraySize == 1)
			{
				const tcu::Texture1D& texture = dynamic_cast<const TestTexture1D&>(testTexture).getTexture();
				program = MovePtr<Program>(new SamplerProgram<tcu::Texture1D>(renderTargetFormat, texture, sampler, samplerLod, channelMapping));
			}
			else
			{
				const tcu::Texture1DArray& texture = dynamic_cast<const TestTexture1DArray&>(testTexture).getTexture();

				if (subresource.baseMipLevel > 0 || subresource.arraySize < (deUint32)texture.getNumLayers())
				{
					// Not all texture levels and layers are needed. Create new sub-texture.
					const tcu::ConstPixelBufferAccess	baseLevel	= texture.getLevel(subresource.baseMipLevel);
					tcu::Texture1DArray					textureView	(texture.getFormat(), baseLevel.getWidth(), subresource.arraySize);

					copySubresourceRange(textureView, texture, subresource);

					program = MovePtr<Program>(new SamplerProgram<tcu::Texture1DArray>(renderTargetFormat, textureView, sampler, samplerLod, channelMapping));
				}
				else
				{
					program = MovePtr<Program>(new SamplerProgram<tcu::Texture1DArray>(renderTargetFormat, texture, sampler, samplerLod, channelMapping));
				}
			}
			break;

		case VK_IMAGE_TYPE_2D:
			if (arraySize == 1)
			{
				const tcu::Texture2D& texture = dynamic_cast<const TestTexture2D&>(testTexture).getTexture();
				program = MovePtr<Program>(new SamplerProgram<tcu::Texture2D>(renderTargetFormat, texture, sampler, samplerLod, channelMapping));
			}
			else
			{
				if (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
				{
					if (arraySize == tcu::CUBEFACE_LAST)
					{
						const tcu::TextureCube& texture = dynamic_cast<const TestTextureCube&>(testTexture).getTexture();
						program = MovePtr<Program>(new SamplerProgram<tcu::TextureCube>(renderTargetFormat, texture, sampler, samplerLod, channelMapping));
					}
					else
					{
						DE_ASSERT(arraySize % tcu::CUBEFACE_LAST == 0);

						const tcu::TextureCubeArray& texture = dynamic_cast<const TestTextureCubeArray&>(testTexture).getTexture();

						if (subresource.baseMipLevel > 0 || subresource.arraySize < (deUint32)texture.getDepth())
						{
							DE_ASSERT(subresource.baseArrayLayer + subresource.arraySize <= (deUint32)texture.getDepth());

							// Not all texture levels and layers are needed. Create new sub-texture.
							const tcu::ConstPixelBufferAccess	baseLevel		= texture.getLevel(subresource.baseMipLevel);
							tcu::TextureCubeArray				textureView		(texture.getFormat(), baseLevel.getWidth(), subresource.arraySize);

							copySubresourceRange(textureView, texture, subresource);

							program = MovePtr<Program>(new SamplerProgram<tcu::TextureCubeArray>(renderTargetFormat, textureView, sampler, samplerLod, channelMapping));
						}
						else
						{
							// Use all array layers
							program = MovePtr<Program>(new SamplerProgram<tcu::TextureCubeArray>(renderTargetFormat, texture, sampler, samplerLod, channelMapping));
						}
					}
				}
				else
				{
					const tcu::Texture2DArray& texture = dynamic_cast<const TestTexture2DArray&>(testTexture).getTexture();

					if (subresource.baseMipLevel > 0 || subresource.arraySize < (deUint32)texture.getNumLayers())
					{
						DE_ASSERT(subresource.baseArrayLayer + subresource.arraySize <= (deUint32)texture.getNumLayers());

						// Not all texture levels and layers are needed. Create new sub-texture.
						const tcu::ConstPixelBufferAccess	baseLevel	= texture.getLevel(subresource.baseMipLevel);
						tcu::Texture2DArray					textureView	(texture.getFormat(), baseLevel.getWidth(), baseLevel.getHeight(), subresource.arraySize);

						copySubresourceRange(textureView, texture, subresource);

						program = MovePtr<Program>(new SamplerProgram<tcu::Texture2DArray>(renderTargetFormat, textureView, sampler, samplerLod, channelMapping));
					}
					else
					{
						// Use all array layers
						program = MovePtr<Program>(new SamplerProgram<tcu::Texture2DArray>(renderTargetFormat, texture, sampler, samplerLod, channelMapping));
					}
				}
			}
			break;

		case VK_IMAGE_TYPE_3D:
			{
				const tcu::Texture3D& texture = dynamic_cast<const TestTexture3D&>(testTexture).getTexture();
				program = MovePtr<Program>(new SamplerProgram<tcu::Texture3D>(renderTargetFormat, texture, sampler, samplerLod, channelMapping));
			}
			break;

		default:
			DE_ASSERT(false);
	}

	return program;
}

} // anonymous

ImageSamplingInstance::ImageSamplingInstance (Context&							context,
											  const tcu::IVec2&					renderSize,
											  VkImageViewType					imageViewType,
											  VkFormat							imageFormat,
											  const tcu::IVec3&					imageSize,
											  int								arraySize,
											  const VkChannelMapping&			channelMapping,
											  const VkImageSubresourceRange&	subresourceRange,
											  const VkSamplerCreateInfo&		samplerParams,
											  float								samplerLod,
											  const std::vector<Vertex4Tex4>&	vertices)
	: vkt::TestInstance		(context)
	, m_imageViewType		(imageViewType)
	, m_imageSize			(imageSize)
	, m_arraySize			(arraySize)
	, m_channelMapping		(channelMapping)
	, m_subresourceRange	(subresourceRange)
	, m_samplerParams		(samplerParams)
	, m_samplerLod			(samplerLod)
	, m_renderSize			(renderSize)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_vertices			(vertices)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const VkQueue				queue				= context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkChannelMapping		channelMappingRGBA	= { VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A };

	if (!isSupportedSamplableFormat(context.getInstanceInterface(), context.getPhysicalDevice(), imageFormat))
		throw tcu::NotSupportedError(std::string("Unsupported format for sampling: ") + getFormatName(imageFormat));

	// Create texture image, view and sampler
	{
		VkImageCreateFlags			imageFlags			= 0u;

		if (m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE || m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
			imageFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		// Initialize texture data
		if (isCompressedFormat(imageFormat))
			m_texture = createTestTexture(mapVkCompressedFormat(imageFormat), imageViewType, imageSize, arraySize);
		else
			m_texture = createTestTexture(mapVkFormat(imageFormat), imageViewType, imageSize, arraySize);

		const VkImageCreateInfo	imageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType		sType;
			DE_NULL,																// const void*			pNext;
			getCompatibleImageType(m_imageViewType),								// VkImageType			imageViewType;
			imageFormat,															// VkFormat				format;
			{																		// VkExtent3D			extent;
				m_imageSize.x(),
				m_imageSize.y(),
				m_imageSize.z()
			},
			(deUint32)m_texture->getNumLevels(),									// deUint32				mipLevels;
			(deUint32)m_arraySize,													// deUint32				arraySize;
			1u,																		// deUint32				samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling		tiling;
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,	// VkImageUsageFlags	usage;
			imageFlags,																// VkImageCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode		sharingMode;
			1u,																		// deUint32				queueFamilyCount;
			&queueFamilyIndex,														// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout		initialLayout;
		};

		m_image			= createImage(vk, vkDevice, &imageParams);
		m_imageAlloc	= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_image), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_image, m_imageAlloc->getMemory(), m_imageAlloc->getOffset()));

		// Upload texture data
		uploadTestTexture(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_texture, *m_image);

		// Create image view and sampler
		const VkImageViewCreateInfo imageViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			*m_image,									// VkImage					image;
			m_imageViewType,							// VkImageViewType			viewType;
			imageFormat,								// VkFormat					format;
			m_channelMapping,							// VkChannelMapping			channels;
			m_subresourceRange,							// VkImageSubresourceRange	subresourceRange;
			0u											// VkImageViewCreateFlags	flags;
		};

		m_imageView	= createImageView(vk, vkDevice, &imageViewParams);
		m_sampler	= createSampler(vk, vkDevice, &m_samplerParams);
	}

	// Create descriptor set for combined image and sampler
	{
		DescriptorPoolBuilder descriptorPoolBuilder;
		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u);
		m_descriptorPool = descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT, 1u);

		DescriptorSetLayoutBuilder setLayoutBuilder;
		setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_descriptorSetLayout = setLayoutBuilder.build(vk, vkDevice);

		m_descriptorSet = allocDescriptorSet(vk, vkDevice, *m_descriptorPool, VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, *m_descriptorSetLayout);

		const VkDescriptorInfo descriptorInfo =
		{
			(VkBufferView)DE_NULL,						// VkBufferView				bufferView;
			*m_sampler,									// VkSampler				sampler;
			*m_imageView,								// VkImageView				imageView;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout			imageLayout;
			{											// VkDescriptorBufferInfo	bufferInfo;
				(VkBuffer)0,	// VkBuffer		buffer;
				0,				// VkDeviceSize	offset;
				0				// VkDeviceSize	range;
			}
		};

		DescriptorSetUpdateBuilder setUpdateBuilder;
		setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorInfo);
		setUpdateBuilder.update(vk, vkDevice);
	}

	// Create color image and view
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType		sType;
			DE_NULL,																	// const void*			pNext;
			VK_IMAGE_TYPE_2D,															// VkImageType			imageViewType;
			m_colorFormat,																// VkFormat				format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D			extent;
			1u,																			// deUint32				mipLevels;
			1u,																			// deUint32				arraySize;
			1u,																			// deUint32				samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling		tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT,	// VkImageUsageFlags	usage;
			0u,																			// VkImageCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode		sharingMode;
			1u,																			// deUint32				queueFamilyCount;
			&queueFamilyIndex,															// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout		initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));

		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*m_colorImage,										// VkImage						image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType				viewType;
			m_colorFormat,										// VkFormat						format;
			channelMappingRGBA,									// VkChannelMapping				channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },		// VkImageSubresourceRange		subresourceRange;
			0u													// VkImageViewCreateFlags		flags;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create render pass
	{
		const VkAttachmentDescription colorAttachmentDescription =
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			m_colorFormat,										// VkFormat						format;
			1u,													// deUint32						samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout				finalLayout;
			0u													// VkAttachmentDescriptionFlags	flags;
		};

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkSubpassDescription subpassDescription =
		{
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION,				// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// VkSubpassDescriptionFlags	flags;
			0u,													// deUint32						inputCount;
			DE_NULL,											// const VkAttachmentReference*	pInputAttachments;
			1u,													// deUint32						colorCount;
			&colorAttachmentReference,							// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*	pResolveAttachments;
			{ VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED},	// VkAttachmentReference		depthStencilAttachment;
			0u,													// deUint32						preserveCount;
			DE_NULL												// const VkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			1u,													// deUint32							attachmentCount;
			&colorAttachmentDescription,						// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*m_renderPass,										// VkRenderPass					renderPass;
			1u,													// deUint32						attachmentCount;
			&m_colorAttachmentView.get(),						// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			1u,													// deUint32						descriptorSetCount;
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tex_vert"), 0);
		m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tex_frag"), 0);

		const VkShaderCreateInfo vertexShaderParams =
		{
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*m_vertexShaderModule,							// VkShaderModule		module;
			"main",											// const char*			pName;
			0u,												// VkShaderCreateFlags	flags;
			VK_SHADER_STAGE_VERTEX							// VkShaderStage		stage;
		};

		const VkShaderCreateInfo fragmentShaderParams =
		{
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*m_fragmentShaderModule,						// VkShaderModule		module;
			"main",											// const char*			pName;
			0u,												// VkShaderCreateFlags	flags;
			VK_SHADER_STAGE_FRAGMENT						// VkShaderStage		stage;
		};

		m_vertexShader		= createShader(vk, vkDevice, &vertexShaderParams);
		m_fragmentShader	= createShader(vk, vkDevice, &fragmentShaderParams);
	}

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo shaderStageParams[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,													// const void*					pNext;
				VK_SHADER_STAGE_VERTEX,										// VkShaderStage				stage;
				*m_vertexShader,											// VkShader						shader;
				DE_NULL														// const VkSpecializationInfo*	pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,													// const void*					pNext;
				VK_SHADER_STAGE_FRAGMENT,									// VkShaderStage				stage;
				*m_fragmentShader,											// VkShader						shader;
				DE_NULL														// const VkSpecializationInfo*	pSpecializationInfo;
			}
		};

		const VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0u,											// deUint32					binding;
			sizeof(Vertex4Tex4),						// deUint32					strideInBytes;
			VK_VERTEX_INPUT_STEP_RATE_VERTEX			// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				0u										// deUint32	offsetInBytes;
			},
			{
				1u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				DE_OFFSET_OF(Vertex4Tex4, texCoord),	// deUint32	offsetInBytes;
			}
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			1u,																// deUint32									bindingCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									attributeCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,														// const void*			pNext;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology	topology;
			false															// VkBool32				primitiveRestartEnable;
		};

		const VkViewport viewport =
		{
			0.0f,						// float	originX;
			0.0f,						// float	originY;
			(float)m_renderSize.x(),	// float	width;
			(float)m_renderSize.y(),	// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};

		const VkRect2D scissor = { { 0, 0 }, { m_renderSize.x(), m_renderSize.y() } };

		const VkPipelineViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,														// const void*			pNext;
			1u,																// deUint32				viewportCount;
			&viewport,														// const VkViewport*	pViewports;
			1u,																// deUint32				scissorCount;
			&scissor														// const VkRect2D*		pScissors;
		};

		const VkPipelineRasterStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO,			// VkStructureType	sType;
			DE_NULL,														// const void*		pNext;
			false,															// VkBool32			depthClipEnable;
			false,															// VkBool32			rasterizerDiscardEnable;
			VK_FILL_MODE_SOLID,												// VkFillMode		fillMode;
			VK_CULL_MODE_NONE,												// VkCullMode		cullMode;
			VK_FRONT_FACE_CCW,												// VkFrontFace		frontFace;
			false,															// VkBool32			depthBiasEnable;
			0.0f,															// float			depthBias;
			0.0f,															// float			depthBiasClamp;
			0.0f,															// float			slopeScaledDepthBias;
			1.0f															// float			lineWidth;
		};

		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			false,																		// VkBool32			blendEnable;
			VK_BLEND_ONE,																// VkBlend			srcBlendColor;
			VK_BLEND_ZERO,																// VkBlend			destBlendColor;
			VK_BLEND_OP_ADD,															// VkBlendOp		blendOpColor;
			VK_BLEND_ONE,																// VkBlend			srcBlendAlpha;
			VK_BLEND_ZERO,																// VkBlend			destBlendAlpha;
			VK_BLEND_OP_ADD,															// VkBlendOp		blendOpAlpha;
			VK_CHANNEL_R_BIT | VK_CHANNEL_G_BIT | VK_CHANNEL_B_BIT | VK_CHANNEL_A_BIT	// VkChannelFlags	channelWriteMask;
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			false,														// VkBool32										alphaToCoverageEnable;
			false,														// VkBool32										alphaToOneEnable;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConst[4];
		};

		const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			1u,															// deUint32					rasterSamples;
			false,														// VkBool32					sampleShadingEnable;
			0.0f,														// float					minSampleShading;
			DE_NULL														// const VkSampleMask*		pSampleMask;
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType	sType;
			DE_NULL,													// const void*		pNext;
			false,														// VkBool32			depthTestEnable;
			false,														// VkBool32			depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp		depthCompareOp;
			false,														// VkBool32			depthBoundsTestEnable;
			false,														// VkBool32			stencilTestEnable;
			{															// VkStencilOpState	front;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	stencilFailOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	stencilPassOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	stencilDepthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	stencilCompareOp;
				0u,						// deUint32		stencilCompareMask;
				0u,						// deUint32		stencilWriteMask;
				0u						// deUint32		stencilReference;
			},
			{															// VkStencilOpState	back;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	stencilFailOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	stencilPassOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	stencilDepthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	stencilCompareOp;
				0u,						// deUint32		stencilCompareMask;
				0u,						// deUint32		stencilWriteMask;
				0u						// deUint32		stencilReference;
			},
			-1.0f,														// float			minDepthBounds;
			+1.0f														// float			maxDepthBounds;
		};

		const VkPipelineDynamicStateCreateInfo dynamicStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			0u,															// deUint32					dynamicStateCount;
			DE_NULL														// const VkDynamicState*	pDynamicStates;
		};

		const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			2u,													// deUint32											stageCount;
			shaderStageParams,									// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterStateParams,									// const VkPipelineRasterStateCreateInfo*			pRasterState;
			&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			&dynamicStateParams,								// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			0u,													// VkPipelineCreateFlags							flags;
			*m_pipelineLayout,									// VkPipelineLayout									layout;
			*m_renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		m_graphicsPipeline	= createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			1024u,										// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			0u,											// VkBufferCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4Tex4));
		flushMappedMemoryRange(vk, vkDevice, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset(), vertexBufferParams.size);
	}

	// Create command pool
	{
		const VkCmdPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			queueFamilyIndex,							// deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_TRANSIENT_BIT			// VkCmdPoolCreateFlags	flags;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCmdBufferCreateInfo cmdBufferParams =
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			*m_cmdPool,									// VkCmdPool				cmdPool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			0u											// VkCmdBufferCreateFlags	flags;
		};

		const VkCmdBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkCmdBufferOptimizeFlags	flags;
			DE_NULL,									// VkRenderPass				renderPass;
			0u,											// deUint32					subpass;
			DE_NULL										// VkFramebuffer			framebuffer;
		};

		const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*m_renderPass,											// VkRenderPass			renderPass;
			*m_framebuffer,											// VkFramebuffer		framebuffer;
			{ { 0, 0 }, { m_renderSize.x(), m_renderSize.y() } },	// VkRect2D				renderArea;
			1,														// deUint32				clearValueCount;
			&attachmentClearValue									// const VkClearValue*	pClearValues;
		};

		m_cmdBuffer = createCommandBuffer(vk, vkDevice, &cmdBufferParams);

		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_RENDER_PASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);

		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &m_descriptorSet.get(), 0, DE_NULL);

		const VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);

		vk.cmdEndRenderPass(*m_cmdBuffer);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		m_fence = createFence(vk, vkDevice, &fenceParams);
	}
}

ImageSamplingInstance::~ImageSamplingInstance (void)
{
}

tcu::TestStatus ImageSamplingInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &m_cmdBuffer.get(), *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	return verifyImage();
}

tcu::TestStatus ImageSamplingInstance::verifyImage (void)
{
	const tcu::TextureFormat		colorFormat				= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat		depthStencilFormat		= tcu::TextureFormat(); // Undefined depth/stencil format.
	const tcu::Sampler				sampler					= mapVkSampler(m_samplerParams);
	const tcu::UVec4				channelMapping			= mapVkChannelMapping(m_channelMapping);
	float							samplerLod;
	bool							compareOk;
	MovePtr<Program>				program;
	MovePtr<ReferenceRenderer>		refRenderer;

	// Set up LOD of reference sampler
	if (m_samplerParams.mipMode == VK_TEX_MIPMAP_MODE_BASE)
		samplerLod = 0.0f;
	else
		samplerLod = de::max(m_samplerParams.minLod, de::min(m_samplerParams.maxLod, m_samplerParams.mipLodBias + m_samplerLod));

	// Create reference program that uses image subresource range
	program = createRefProgram(colorFormat, sampler, samplerLod, channelMapping, *m_texture, m_imageViewType, m_arraySize, m_subresourceRange);
	const rr::Program referenceProgram = program->getReferenceProgram();

	// Render reference image
	refRenderer = MovePtr<ReferenceRenderer>(new ReferenceRenderer(m_renderSize.x(), m_renderSize.y(), 1, colorFormat, depthStencilFormat, &referenceProgram));
	const rr::RenderState renderState(refRenderer->getViewportState());
	refRenderer->draw(renderState, rr::PRIMITIVETYPE_TRIANGLES, m_vertices);

	// Compare result with reference image
	{
		const DeviceInterface&		vk							= m_context.getDeviceInterface();
		const VkDevice				vkDevice					= m_context.getDevice();
		const VkQueue				queue						= m_context.getUniversalQueue();
		const deUint32				queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator				memAlloc					(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		MovePtr<tcu::TextureLevel>	result						= readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer->getAccess(),
															  result->getAccess(),
															  tcu::UVec4(4, 4, 4, 4),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

} // pipeline
} // vkt
