/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
static MovePtr<TestTexture> createTestTexture (const TcuFormatType format, VkImageViewType viewType, const tcu::IVec3& size, int layerCount)
{
	MovePtr<TestTexture>	texture;
	const VkImageType		imageType = getCompatibleImageType(viewType);

	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:
			if (layerCount == 1)
				texture = MovePtr<TestTexture>(new TestTexture1D(format, size.x()));
			else
				texture = MovePtr<TestTexture>(new TestTexture1DArray(format, size.x(), layerCount));

			break;

		case VK_IMAGE_TYPE_2D:
			if (layerCount == 1)
			{
				texture = MovePtr<TestTexture>(new TestTexture2D(format, size.x(), size.y()));
			}
			else
			{
				if (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
				{
					if (layerCount == tcu::CUBEFACE_LAST)
					{
						texture = MovePtr<TestTexture>(new TestTextureCube(format, size.x()));
					}
					else
					{
						DE_ASSERT(layerCount % tcu::CUBEFACE_LAST == 0);

						texture = MovePtr<TestTexture>(new TestTextureCubeArray(format, size.x(), layerCount));
					}
				}
				else
				{
					texture = MovePtr<TestTexture>(new TestTexture2DArray(format, size.x(), size.y(), layerCount));
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
	DE_ASSERT(subresourceRange.levelCount <= (deUint32)dest.getNumLevels());
	DE_ASSERT(subresourceRange.baseMipLevel + subresourceRange.levelCount <= (deUint32)src.getNumLevels());

	for (int levelNdx = 0; levelNdx < dest.getNumLevels(); levelNdx++)
	{
		const tcu::ConstPixelBufferAccess	srcLevel		(src.getLevel(subresourceRange.baseMipLevel + levelNdx));
		const deUint32						srcLayerOffset	= subresourceRange.baseArrayLayer * srcLevel.getWidth() * srcLevel.getHeight() * srcLevel.getFormat().getPixelSize();
		const tcu::ConstPixelBufferAccess	srcLevelLayers	(srcLevel.getFormat(), srcLevel.getWidth(), srcLevel.getHeight(), subresourceRange.layerCount, (deUint8*)srcLevel.getDataPtr() + srcLayerOffset);

		if (dest.isLevelEmpty(levelNdx))
			dest.allocLevel(levelNdx);

		tcu::copy(dest.getLevel(levelNdx), srcLevelLayers);
	}
}

template<>
void copySubresourceRange<tcu::Texture1DArray> (tcu::Texture1DArray& dest, const tcu::Texture1DArray& src, const VkImageSubresourceRange& subresourceRange)
{
	DE_ASSERT(subresourceRange.levelCount <= (deUint32)dest.getNumLevels());
	DE_ASSERT(subresourceRange.baseMipLevel + subresourceRange.levelCount <= (deUint32)src.getNumLevels());

	DE_ASSERT(subresourceRange.layerCount == (deUint32)dest.getNumLayers());
	DE_ASSERT(subresourceRange.baseArrayLayer + subresourceRange.layerCount <= (deUint32)src.getNumLayers());

	for (int levelNdx = 0; levelNdx < dest.getNumLevels(); levelNdx++)
	{
		const tcu::ConstPixelBufferAccess	srcLevel		(src.getLevel(subresourceRange.baseMipLevel + levelNdx));
		const deUint32						srcLayerOffset	= subresourceRange.baseArrayLayer * srcLevel.getWidth() * srcLevel.getFormat().getPixelSize();
		const tcu::ConstPixelBufferAccess	srcLevelLayers	(srcLevel.getFormat(), srcLevel.getWidth(), subresourceRange.layerCount, 1, (deUint8*)srcLevel.getDataPtr() + srcLayerOffset);

		if (dest.isLevelEmpty(levelNdx))
			dest.allocLevel(levelNdx);

		tcu::copy(dest.getLevel(levelNdx), srcLevelLayers);
	}
}

template<>
void copySubresourceRange<tcu::Texture3D>(tcu::Texture3D& dest, const tcu::Texture3D& src, const VkImageSubresourceRange& subresourceRange)
{
	DE_ASSERT(subresourceRange.levelCount <= (deUint32)dest.getNumLevels());
	DE_ASSERT(subresourceRange.baseMipLevel + subresourceRange.levelCount <= (deUint32)src.getNumLevels());

	for (int levelNdx = 0; levelNdx < dest.getNumLevels(); levelNdx++)
	{
		const tcu::ConstPixelBufferAccess	srcLevel(src.getLevel(subresourceRange.baseMipLevel + levelNdx));
		const tcu::ConstPixelBufferAccess	srcLevelLayers(srcLevel.getFormat(), srcLevel.getWidth(), srcLevel.getHeight(), srcLevel.getDepth(), (deUint8*)srcLevel.getDataPtr());

		if (dest.isLevelEmpty(levelNdx))
			dest.allocLevel(levelNdx);

		tcu::copy(dest.getLevel(levelNdx), srcLevelLayers);
	}
}

static MovePtr<Program> createRefProgram(const tcu::TextureFormat&			renderTargetFormat,
										  const tcu::Sampler&				sampler,
										  float								samplerLod,
										  const tcu::UVec4&					componentMapping,
										  const TestTexture&				testTexture,
										  VkImageViewType					viewType,
										  int								layerCount,
										  const VkImageSubresourceRange&	subresource)
{
	MovePtr<Program>	program;
	const VkImageType	imageType		= getCompatibleImageType(viewType);
	tcu::Vec4			lookupScale		(1.0f);
	tcu::Vec4			lookupBias		(0.0f);

	if (!testTexture.isCompressed())
	{
		const tcu::TextureFormatInfo	fmtInfo	= tcu::getTextureFormatInfo(testTexture.getLevel(0, 0).getFormat());

		// Needed to normalize various formats to 0..1 range for writing into RT
		lookupScale	= fmtInfo.lookupScale;
		lookupBias	= fmtInfo.lookupBias;
	}
	// else: All supported compressed formats are fine with no normalization.
	//		 ASTC LDR blocks decompress to f16 so querying normalization parameters
	//		 based on uncompressed formats would actually lead to massive precision loss
	//		 and complete lack of coverage in case of R8G8B8A8_UNORM RT.

	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:
			if (layerCount == 1)
			{
				const tcu::Texture1D& texture = dynamic_cast<const TestTexture1D&>(testTexture).getTexture();
				program = MovePtr<Program>(new SamplerProgram<tcu::Texture1D>(renderTargetFormat, texture, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
			}
			else
			{
				const tcu::Texture1DArray& texture = dynamic_cast<const TestTexture1DArray&>(testTexture).getTexture();

				if (subresource.baseMipLevel > 0 || subresource.layerCount < (deUint32)texture.getNumLayers())
				{
					// Not all texture levels and layers are needed. Create new sub-texture.
					const tcu::ConstPixelBufferAccess	baseLevel	= texture.getLevel(subresource.baseMipLevel);
					tcu::Texture1DArray					textureView	(texture.getFormat(), baseLevel.getWidth(), subresource.layerCount);

					copySubresourceRange(textureView, texture, subresource);

					program = MovePtr<Program>(new SamplerProgram<tcu::Texture1DArray>(renderTargetFormat, textureView, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
				}
				else
				{
					program = MovePtr<Program>(new SamplerProgram<tcu::Texture1DArray>(renderTargetFormat, texture, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
				}
			}
			break;

		case VK_IMAGE_TYPE_2D:
			if (layerCount == 1)
			{
				const tcu::Texture2D& texture = dynamic_cast<const TestTexture2D&>(testTexture).getTexture();
				program = MovePtr<Program>(new SamplerProgram<tcu::Texture2D>(renderTargetFormat, texture, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
			}
			else
			{
				if (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
				{
					if (layerCount == tcu::CUBEFACE_LAST)
					{
						const tcu::TextureCube& texture = dynamic_cast<const TestTextureCube&>(testTexture).getTexture();
						program = MovePtr<Program>(new SamplerProgram<tcu::TextureCube>(renderTargetFormat, texture, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
					}
					else
					{
						DE_ASSERT(layerCount % tcu::CUBEFACE_LAST == 0);

						const tcu::TextureCubeArray& texture = dynamic_cast<const TestTextureCubeArray&>(testTexture).getTexture();

						if (subresource.baseMipLevel > 0 || subresource.layerCount < (deUint32)texture.getDepth())
						{
							DE_ASSERT(subresource.baseArrayLayer + subresource.layerCount <= (deUint32)texture.getDepth());

							// Not all texture levels and layers are needed. Create new sub-texture.
							const tcu::ConstPixelBufferAccess	baseLevel		= texture.getLevel(subresource.baseMipLevel);
							tcu::TextureCubeArray				textureView		(texture.getFormat(), baseLevel.getWidth(), subresource.layerCount);

							copySubresourceRange(textureView, texture, subresource);

							program = MovePtr<Program>(new SamplerProgram<tcu::TextureCubeArray>(renderTargetFormat, textureView, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
						}
						else
						{
							// Use all array layers
							program = MovePtr<Program>(new SamplerProgram<tcu::TextureCubeArray>(renderTargetFormat, texture, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
						}
					}
				}
				else
				{
					const tcu::Texture2DArray& texture = dynamic_cast<const TestTexture2DArray&>(testTexture).getTexture();

					if (subresource.baseMipLevel > 0 || subresource.layerCount < (deUint32)texture.getNumLayers())
					{
						DE_ASSERT(subresource.baseArrayLayer + subresource.layerCount <= (deUint32)texture.getNumLayers());

						// Not all texture levels and layers are needed. Create new sub-texture.
						const tcu::ConstPixelBufferAccess	baseLevel	= texture.getLevel(subresource.baseMipLevel);
						tcu::Texture2DArray					textureView	(texture.getFormat(), baseLevel.getWidth(), baseLevel.getHeight(), subresource.layerCount);

						copySubresourceRange(textureView, texture, subresource);

						program = MovePtr<Program>(new SamplerProgram<tcu::Texture2DArray>(renderTargetFormat, textureView, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
					}
					else
					{
						// Use all array layers
						program = MovePtr<Program>(new SamplerProgram<tcu::Texture2DArray>(renderTargetFormat, texture, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
					}
				}
			}
			break;

		case VK_IMAGE_TYPE_3D:
			{
				const tcu::Texture3D& texture = dynamic_cast<const TestTexture3D&>(testTexture).getTexture();

				if (subresource.baseMipLevel > 0)
				{
					// Not all texture levels are needed. Create new sub-texture.
					const tcu::ConstPixelBufferAccess	baseLevel = texture.getLevel(subresource.baseMipLevel);
					tcu::Texture3D						textureView(texture.getFormat(), baseLevel.getWidth(), baseLevel.getHeight(), baseLevel.getDepth());

					copySubresourceRange(textureView, texture, subresource);

					program = MovePtr<Program>(new SamplerProgram<tcu::Texture3D>(renderTargetFormat, textureView, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
				}
				else
				{
					program = MovePtr<Program>(new SamplerProgram<tcu::Texture3D>(renderTargetFormat, texture, sampler, samplerLod, lookupScale, lookupBias, componentMapping));
				}
			}
			break;

		default:
			DE_ASSERT(false);
	}

	return program;
}

} // anonymous

ImageSamplingInstance::ImageSamplingInstance (Context&							context,
											  const tcu::UVec2&					renderSize,
											  VkImageViewType					imageViewType,
											  VkFormat							imageFormat,
											  const tcu::IVec3&					imageSize,
											  int								layerCount,
											  const VkComponentMapping&			componentMapping,
											  const VkImageSubresourceRange&	subresourceRange,
											  const VkSamplerCreateInfo&		samplerParams,
											  float								samplerLod,
											  const std::vector<Vertex4Tex4>&	vertices)
	: vkt::TestInstance		(context)
	, m_imageViewType		(imageViewType)
	, m_imageFormat			(imageFormat)
	, m_imageSize			(imageSize)
	, m_layerCount			(layerCount)
	, m_componentMapping	(componentMapping)
	, m_subresourceRange	(subresourceRange)
	, m_samplerParams		(samplerParams)
	, m_samplerLod			(samplerLod)
	, m_renderSize			(renderSize)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_vertices			(vertices)
{
	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkDevice				vkDevice				= context.getDevice();
	const VkQueue				queue					= context.getUniversalQueue();
	const deUint32				queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	if (!isSupportedSamplableFormat(context.getInstanceInterface(), context.getPhysicalDevice(), imageFormat))
		throw tcu::NotSupportedError(std::string("Unsupported format for sampling: ") + getFormatName(imageFormat));

	if ((samplerParams.minFilter == VK_FILTER_LINEAR ||
		 samplerParams.magFilter == VK_FILTER_LINEAR ||
		 samplerParams.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR) &&
		!isLinearFilteringSupported(context.getInstanceInterface(), context.getPhysicalDevice(), imageFormat, VK_IMAGE_TILING_OPTIMAL))
		throw tcu::NotSupportedError(std::string("Unsupported format for linear filtering: ") + getFormatName(imageFormat));

	if (isCompressedFormat(imageFormat) && imageViewType == VK_IMAGE_VIEW_TYPE_3D)
	{
		// \todo [2016-01-22 pyry] Mandate VK_ERROR_FORMAT_NOT_SUPPORTED
		try
		{
			const VkImageFormatProperties	formatProperties	= getPhysicalDeviceImageFormatProperties(context.getInstanceInterface(),
																										 context.getPhysicalDevice(),
																										 imageFormat,
																										 VK_IMAGE_TYPE_3D,
																										 VK_IMAGE_TILING_OPTIMAL,
																										 VK_IMAGE_USAGE_SAMPLED_BIT,
																										 (VkImageCreateFlags)0);

			if (formatProperties.maxExtent.width == 0 &&
				formatProperties.maxExtent.height == 0 &&
				formatProperties.maxExtent.depth == 0)
				TCU_THROW(NotSupportedError, "3D compressed format not supported");
		}
		catch (const Error&)
		{
			TCU_THROW(NotSupportedError, "3D compressed format not supported");
		}
	}

	// Create texture image, view and sampler
	{
		VkImageCreateFlags			imageFlags			= 0u;

		if (m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE || m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
			imageFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		// Initialize texture data
		if (isCompressedFormat(imageFormat))
			m_texture = createTestTexture(mapVkCompressedFormat(imageFormat), imageViewType, imageSize, layerCount);
		else
			m_texture = createTestTexture(mapVkFormat(imageFormat), imageViewType, imageSize, layerCount);

		const VkImageCreateInfo	imageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,							// VkStructureType			sType;
			DE_NULL,														// const void*				pNext;
			imageFlags,														// VkImageCreateFlags		flags;
			getCompatibleImageType(m_imageViewType),						// VkImageType				imageType;
			imageFormat,													// VkFormat					format;
			{																// VkExtent3D				extent;
				(deUint32)m_imageSize.x(),
				(deUint32)m_imageSize.y(),
				(deUint32)m_imageSize.z()
			},
			(deUint32)m_texture->getNumLevels(),							// deUint32					mipLevels;
			(deUint32)m_layerCount,											// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,										// VkImageTiling			tiling;
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharingMode;
			1u,																// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,												// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout;
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
			0u,											// VkImageViewCreateFlags	flags;
			*m_image,									// VkImage					image;
			m_imageViewType,							// VkImageViewType			viewType;
			imageFormat,								// VkFormat					format;
			m_componentMapping,							// VkComponentMapping		components;
			m_subresourceRange,							// VkImageSubresourceRange	subresourceRange;
		};

		m_imageView	= createImageView(vk, vkDevice, &imageViewParams);
		m_sampler	= createSampler(vk, vkDevice, &m_samplerParams);
	}

	// Create descriptor set for combined image and sampler
	{
		DescriptorPoolBuilder descriptorPoolBuilder;
		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u);
		m_descriptorPool = descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		DescriptorSetLayoutBuilder setLayoutBuilder;
		setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_descriptorSetLayout = setLayoutBuilder.build(vk, vkDevice);

		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*m_descriptorPool,									// VkDescriptorPool				descriptorPool;
			1u,													// deUint32						setLayoutCount;
			&m_descriptorSetLayout.get()						// const VkDescriptorSetLayout*	pSetLayouts;
		};

		m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

		const VkDescriptorImageInfo descriptorImageInfo =
		{
			*m_sampler,									// VkSampler		sampler;
			*m_imageView,								// VkImageView		imageView;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout;
		};

		DescriptorSetUpdateBuilder setUpdateBuilder;
		setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfo);
		setUpdateBuilder.update(vk, vkDevice);
	}

	// Create color image and view
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y(), 1u },				// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));

		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageViewCreateFlags	flags;
			*m_colorImage,										// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
			m_colorFormat,										// VkFormat					format;
			componentMappingRGBA,								// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create render pass
	{
		const VkAttachmentDescription colorAttachmentDescription =
		{
			0u,													// VkAttachmentDescriptionFlags		flags;
			m_colorFormat,										// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout					finalLayout;
		};

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkSubpassDescription subpassDescription =
		{
			0u,													// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// deUint32						inputAttachmentCount;
			DE_NULL,											// const VkAttachmentReference*	pInputAttachments;
			1u,													// deUint32						colorAttachmentCount;
			&colorAttachmentReference,							// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,											// const VkAttachmentReference*	pDepthStencilAttachment;
			0u,													// deUint32						preserveAttachmentCount;
			DE_NULL												// const VkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
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
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			1u,													// deUint32					attachmentCount;
			&m_colorAttachmentView.get(),						// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32					width;
			(deUint32)m_renderSize.y(),							// deUint32					height;
			1u													// deUint32					layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						setLayoutCount;
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tex_vert"), 0);
	m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tex_frag"), 0);

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo shaderStages[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0u,															// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBits				stage;
				*m_vertexShaderModule,										// VkShaderModule						module;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0u,															// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBits				stage;
				*m_fragmentShaderModule,									// VkShaderModule						module;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			}
		};

		const VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0u,									// deUint32					binding;
			sizeof(Vertex4Tex4),				// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX			// VkVertexInputStepRate	inputRate;
		};

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				0u										// deUint32	offset;
			},
			{
				1u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				DE_OFFSET_OF(Vertex4Tex4, texCoord),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology;
			false															// VkBool32									primitiveRestartEnable;
		};

		const VkViewport viewport =
		{
			0.0f,						// float	x;
			0.0f,						// float	y;
			(float)m_renderSize.x(),	// float	width;
			(float)m_renderSize.y(),	// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};

		const VkRect2D scissor = { { 0, 0 }, { (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y() } };

		const VkPipelineViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType						sType;
			DE_NULL,														// const void*							pNext;
			0u,																// VkPipelineViewportStateCreateFlags	flags;
			1u,																// deUint32								viewportCount;
			&viewport,														// const VkViewport*					pViewports;
			1u,																// deUint32								scissorCount;
			&scissor														// const VkRect2D*						pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineRasterizationStateCreateFlags	flags;
			false,															// VkBool32									depthClampEnable;
			false,															// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
			false,															// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			1.0f															// float									lineWidth;
		};

		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			false,														// VkBool32					blendEnable;
			VK_BLEND_FACTOR_ONE,										// VkBlendFactor			srcColorBlendFactor;
			VK_BLEND_FACTOR_ZERO,										// VkBlendFactor			dstColorBlendFactor;
			VK_BLEND_OP_ADD,											// VkBlendOp				colorBlendOp;
			VK_BLEND_FACTOR_ONE,										// VkBlendFactor			srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ZERO,										// VkBlendFactor			dstAlphaBlendFactor;
			VK_BLEND_OP_ADD,											// VkBlendOp				alphaBlendOp;
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |		// VkColorComponentFlags	colorWriteMask;
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			0u,															// VkPipelineColorBlendStateCreateFlags			flags;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
		};

		const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineMultisampleStateCreateFlags	flags;
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
			false,														// VkBool32									sampleShadingEnable;
			0.0f,														// float									minSampleShading;
			DE_NULL,													// const VkSampleMask*						pSampleMask;
			false,														// VkBool32									alphaToCoverageEnable;
			false														// VkBool32									alphaToOneEnable;
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
			false,														// VkBool32									depthTestEnable;
			false,														// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
			false,														// VkBool32									depthBoundsTestEnable;
			false,														// VkBool32									stencilTestEnable;
			{															// VkStencilOpState							front;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	failOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	passOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u						// deUint32		reference;
			},
			{															// VkStencilOpState	back;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	failOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	passOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u						// deUint32		reference;
			},
			0.0f,														// float			minDepthBounds;
			1.0f														// float			maxDepthBounds;
		};

		const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			0u,													// VkPipelineCreateFlags							flags;
			2u,													// deUint32											stageCount;
			shaderStages,										// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterStateParams,									// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
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
		const VkDeviceSize			vertexBufferSize	= (VkDeviceSize)(m_vertices.size() * sizeof(Vertex4Tex4));
		const VkBufferCreateInfo	vertexBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			vertexBufferSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		DE_ASSERT(vertexBufferSize > 0);

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), &m_vertices[0], (size_t)vertexBufferSize);
		flushMappedMemoryRange(vk, vkDevice, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset(), vertexBufferParams.size);
	}

	// Create command pool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCommandPoolCreateFlags	flags;
			queueFamilyIndex								// deUint32					queueFamilyIndex;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u,												// deUint32					bufferCount;
		};

		const VkCommandBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkCommandBufferUsageFlags		flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL,
		};

		const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*m_renderPass,											// VkRenderPass			renderPass;
			*m_framebuffer,											// VkFramebuffer		framebuffer;
			{
				{ 0, 0 },
				{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y() }
			},														// VkRect2D				renderArea;
			1,														// deUint32				clearValueCount;
			&attachmentClearValue									// const VkClearValue*	pClearValues;
		};

		const VkImageMemoryBarrier preAttachmentBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkAccessFlags			srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
			*m_colorImage,									// VkImage					image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferAllocateInfo);

		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, 1u, &preAttachmentBarrier);

		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

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
	const VkSubmitInfo			submitInfo	=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	return verifyImage();
}

tcu::TestStatus ImageSamplingInstance::verifyImage (void)
{
	const tcu::TextureFormat		colorFormat				= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat		depthStencilFormat		= tcu::TextureFormat(); // Undefined depth/stencil format.
	const tcu::Sampler				sampler					= mapVkSampler(m_samplerParams);
	const tcu::UVec4				componentMapping		= mapVkComponentMapping(m_componentMapping);
	float							samplerLod;
	bool							compareOk;
	MovePtr<Program>				program;
	MovePtr<ReferenceRenderer>		refRenderer;

	// Set up LOD of reference sampler
	samplerLod = de::max(m_samplerParams.minLod, de::min(m_samplerParams.maxLod, m_samplerParams.mipLodBias + m_samplerLod));

	// Create reference program that uses image subresource range
	program = createRefProgram(colorFormat, sampler, samplerLod, componentMapping, *m_texture, m_imageViewType, m_layerCount, m_subresourceRange);
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
		tcu::UVec4					threshold					= tcu::UVec4(4, 4, 4, 4);

		if ((m_imageFormat == vk::VK_FORMAT_EAC_R11G11_SNORM_BLOCK) || (m_imageFormat == vk::VK_FORMAT_EAC_R11_SNORM_BLOCK))
			threshold = tcu::UVec4(8, 8, 8, 8);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer->getAccess(),
															  result->getAccess(),
															  threshold,
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
