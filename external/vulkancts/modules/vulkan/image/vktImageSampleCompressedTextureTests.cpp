/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC.
 *
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
 * \brief Tests that images using a block-compressed format are sampled
 * correctly
 *
 * These tests create a storage image using a 128-bit or a 64-bit
 * block-compressed image format and an ImageView using an uncompressed
 * format. Each test case then fills the storage image with compressed
 * color values in a compute shader and samples the storage image. If the
 * sampled values are pure blue, the test passes.
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include "tcuVectorType.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vktImageTestsUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "tcuTestLog.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{
using tcu::IVec3;
using tcu::Vec2;
using tcu::Vec4;
using std::vector;
using de::MovePtr;
using tcu::TextureLevel;
using tcu::PixelBufferAccess;
using tcu::ConstPixelBufferAccess;

const VkDeviceSize	BUFFERSIZE	= 100u * 1024;
const int			WIDTH		= 80;
const int			HEIGHT		= 80;
const int			FACES		= 6;

inline VkImageCreateInfo makeImageCreateInfo (const IVec3& size, const VkFormat& format, bool storageImage, bool cubemap)
{
	VkImageUsageFlags	usageFlags	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
									  | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkImageCreateFlags	createFlags	= cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : DE_NULL;
	const deUint32		layerCount	= cubemap ? 6u : 1u;

	if (storageImage)
	{
		usageFlags	= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
					  | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		createFlags	|= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT
					  | VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT;
	}

	const VkImageCreateInfo	imageParams	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType         sType;
		DE_NULL,								//  const void*             pNext;
		createFlags,							//  VkImageCreateFlags      flags;
		VK_IMAGE_TYPE_2D,						//  VkImageType             imageType;
		format,									//  VkFormat                format;
		makeExtent3D(size.x(), size.y(), 1u),	//  VkExtent3D              extent;
		1u,										//  deUint32                mipLevels;
		layerCount,								//  deUint32                arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits   samples;
		VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling           tiling;
		usageFlags,								//  VkImageUsageFlags       usage;
		VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode           sharingMode;
		0u,										//  deUint32                queueFamilyIndexCount;
		DE_NULL,								//  const deUint32*         pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//  VkImageLayout           initialLayout;
	};

	return imageParams;
}

Move<VkBuffer> makeVertexBuffer (const DeviceInterface& vk, const VkDevice device, const deUint32 queueFamilyIndex)
{
	const VkBufferCreateInfo	vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType      sType;
		DE_NULL,								// const void*          pNext;
		0u,										// VkBufferCreateFlags  flags;
		BUFFERSIZE,								// VkDeviceSize         size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// VkBufferUsageFlags   usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode        sharingMode;
		1u,										// deUint32             queueFamilyIndexCount;
		&queueFamilyIndex						// const deUint32*      pQueueFamilyIndices;
	};

	Move<VkBuffer>				vertexBuffer		= createBuffer(vk, device, &vertexBufferParams);
	return vertexBuffer;
}

class SampleDrawnTextureTestInstance : public TestInstance
{
public:
	SampleDrawnTextureTestInstance (Context&			context,
									const VkFormat	imageFormat,
									const VkFormat	imageViewFormat,
									const bool		twoSamplers,
									const bool		cubemap);

	tcu::TestStatus iterate			(void);

private:
	const VkFormat	m_imageFormat;
	const VkFormat	m_imageViewFormat;
	const bool		m_twoSamplers;
	const bool		m_cubemap;
};

SampleDrawnTextureTestInstance::SampleDrawnTextureTestInstance (Context& context, const VkFormat imageFormat, const VkFormat imageViewFormat,
																const bool twoSamplers, const bool cubemap)
	: TestInstance											   (context)
	, m_imageFormat		(imageFormat)
	, m_imageViewFormat	(imageViewFormat)
	, m_twoSamplers		(twoSamplers)
	, m_cubemap			(cubemap)
{
}

template<typename T>
inline size_t sizeInBytes (const vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

Move<VkSampler> makeSampler (const DeviceInterface& vk, const VkDevice& device)
{
	const VkSamplerCreateInfo samplerParams =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType          sType;
		DE_NULL,									// const void*              pNext;
		(VkSamplerCreateFlags)0,					// VkSamplerCreateFlags     flags;
		VK_FILTER_NEAREST,							// VkFilter                 magFilter;
		VK_FILTER_NEAREST,							// VkFilter                 minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode      mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode     addressModeU;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode     addressModeV;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode     addressModeW;
		0.0f,										// float                    mipLodBias;
		VK_FALSE,									// VkBool32                 anisotropyEnable;
		1.0f,										// float                    maxAnisotropy;
		VK_FALSE,									// VkBool32                 compareEnable;
		VK_COMPARE_OP_ALWAYS,						// VkCompareOp              compareOp;
		0.0f,										// float                    minLod;
		0.0f,										// float                    maxLod;
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// VkBorderColor            borderColor;
		VK_FALSE,									// VkBool32                 unnormalizedCoordinates;
	};

	return createSampler(vk, device, &samplerParams);
}

struct Vertex
{
	Vertex(Vec4 position_, Vec2 uv_) : position(position_), uv(uv_) {}
	Vec4 position;
	Vec2 uv;

	static VkVertexInputBindingDescription				getBindingDescription		(void);
	static vector<VkVertexInputAttributeDescription>	getAttributeDescriptions	(void);
};

VkVertexInputBindingDescription Vertex::getBindingDescription (void)
{
	static const VkVertexInputBindingDescription desc =
	{
		0u,										// deUint32             binding;
		static_cast<deUint32>(sizeof(Vertex)),	// deUint32             stride;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate    inputRate;
	};

	return desc;
}

vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions (void)
{
	static const vector<VkVertexInputAttributeDescription> desc =
	{
		{
			0u,													// deUint32    location;
			0u,													// deUint32    binding;
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,					// VkFormat    format;
			static_cast<deUint32>(offsetof(Vertex, position)),	// deUint32    offset;
		},
		{
			1u,													// deUint32    location;
			0u,													// deUint32    binding;
			vk::VK_FORMAT_R32G32_SFLOAT,						// VkFormat    format;
			static_cast<deUint32>(offsetof(Vertex, uv)),		// deUint32    offset;
		},
	};

	return desc;
}

// Generates the vertices of a full quad and texture coordinates of each vertex.
vector<Vertex> generateVertices (void)
{
	vector<Vertex> vertices;
	vertices.push_back(Vertex(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec2(0.0f, 0.0f)));
	vertices.push_back(Vertex(Vec4( 1.0f, -1.0f, 0.0f, 1.0f), Vec2(1.0f, 0.0f)));
	vertices.push_back(Vertex(Vec4(-1.0f,  1.0f, 0.0f, 1.0f), Vec2(0.0f, 1.0f)));
	vertices.push_back(Vertex(Vec4( 1.0f, -1.0f, 0.0f, 1.0f), Vec2(1.0f, 0.0f)));
	vertices.push_back(Vertex(Vec4( 1.0f,  1.0f, 0.0f, 1.0f), Vec2(1.0f, 1.0f)));
	vertices.push_back(Vertex(Vec4(-1.0f,  1.0f, 0.0f, 1.0f), Vec2(0.0f, 1.0f)));

	return vertices;
}

// Generates a reference image filled with pure blue.
TextureLevel makeReferenceImage (const VkFormat format, int width, int height)
{
	TextureLevel referenceImage(mapVkFormat(format), width, height, 1);
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
			referenceImage.getAccess().setPixel(tcu::IVec4(0, 0, 255, 255), x, y, 0);

	return referenceImage;
}

tcu::TestStatus SampleDrawnTextureTestInstance::iterate (void)
{
	DE_ASSERT(m_imageFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK || m_imageFormat == VK_FORMAT_BC3_UNORM_BLOCK);

	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	Allocator&						allocator				= m_context.getDefaultAllocator();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const IVec3						imageSize				= {static_cast<int>(WIDTH), HEIGHT, 1};
	const VkExtent2D				renderSize				= {deUint32(WIDTH), deUint32(HEIGHT)};
	const VkRect2D					renderArea				= makeRect2D(makeExtent3D(WIDTH, HEIGHT, 1u));
	const vector<VkRect2D>			scissors				(1u, renderArea);
	const vector<VkViewport>		viewports				(1u, makeViewport(makeExtent3D(WIDTH, HEIGHT, 1u)));

	const Move<VkCommandPool>		cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>		cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const Unique<VkDescriptorPool>	descriptorPool			(DescriptorPoolBuilder()
															 .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 6)
															 .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 12)
															 .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 21u));

	const VkFormat					renderedImageFormat		= VK_FORMAT_R8G8B8A8_UNORM;

	// Create a storage image. The first pipeline fills it and the second pipeline
	// uses it as a sampling source.
	const VkImageCreateInfo			imageCreateInfo			= makeImageCreateInfo(imageSize, m_imageFormat, true, m_cubemap);
	const VkImageSubresourceRange	imageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, 1);
	const ImageWithMemory			storageImage			(vk, device, m_context.getDefaultAllocator(), imageCreateInfo, MemoryRequirement::Any);

	// Create image views and descriptor sets for the first pipeline
	Move<VkDescriptorSetLayout>		descriptorSetLayout		= DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(vk, device);

	Move<VkImageView>				storageImageImageView;
	VkDescriptorImageInfo			storageImageDscrInfo;
	Move<VkDescriptorSet>			storageImageDescriptorSet;

	// Cubemap tests use separate image views for each side of a cubemap.
	vector<VkImageSubresourceRange>	cubeSubresourceRanges;
	vector<Move<VkImageView>>		cubeStorageImageViews;
	vector<VkDescriptorImageInfo>	cubeStorageDscrImageInfos;
	vector<Move<VkDescriptorSet>>	cubeStorageDscrSets;

	if (m_cubemap)
	{
		DescriptorSetUpdateBuilder updateBuilder;
		for (int i = 0; i < FACES; i++)
		{
			cubeSubresourceRanges.emplace_back(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, i, 1));
			cubeStorageImageViews.emplace_back(makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageViewFormat, cubeSubresourceRanges[i]));
			cubeStorageDscrImageInfos.emplace_back(makeDescriptorImageInfo(DE_NULL, *cubeStorageImageViews[i], VK_IMAGE_LAYOUT_GENERAL));
			cubeStorageDscrSets.emplace_back(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
			updateBuilder.writeSingle(*cubeStorageDscrSets[i], DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &cubeStorageDscrImageInfos[i]);
		}
		updateBuilder.update(vk, device);
	}
	else
	{
		storageImageImageView		= makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageViewFormat, imageSubresourceRange);
		storageImageDscrInfo		= makeDescriptorImageInfo(DE_NULL, *storageImageImageView, VK_IMAGE_LAYOUT_GENERAL);
		storageImageDescriptorSet	= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

		DescriptorSetUpdateBuilder()
			.writeSingle(*storageImageDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageImageDscrInfo)
			.update(vk, device);
	}

	// Create a compute pipeline.
	Move<VkShaderModule>			computeShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u);
	const VkPushConstantRange		pushConstantRange		=
	{
		VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags    stageFlags;
		0u,								// uint32_t              offset;
		(deUint32)sizeof(deUint32),		// uint32_t              size;
	};

	const Move<VkPipelineLayout>	computePipelineLayout	= makePipelineLayout(vk, device, 1, &(*descriptorSetLayout), 1, &pushConstantRange);
	Move<VkPipeline>				computePipeline			= makeComputePipeline(vk, device, *computePipelineLayout, *computeShader);


	// Create a graphics pipeline and all the necessary components for sampling the storage image

	// The first sampler uses an uncompressed format.
	const Unique<VkSampler>			sampler					(makeSampler(vk, device));

	// The second sampler uses the same format as the image.
	const Unique<VkSampler>			sampler2				(makeSampler(vk, device));

	// Image views implicitly derive the usage flags from the image. Drop the storage image flag since it's incompatible
	// with the compressed format and unnecessary in sampling.
	VkImageUsageFlags				usageFlags				= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageViewUsageCreateInfo		imageViewUsageInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO_KHR,	//VkStructureType      sType;
		DE_NULL,											//const void*          pNext;
		usageFlags,											//VkImageUsageFlags    usage;
	};

	Move<VkImageView>				sampledImageView;
	Move<VkImageView>				sampledImageView2;
	VkDescriptorImageInfo			samplerDscrImageInfo;
	VkDescriptorImageInfo			samplerDscrImageInfo2;
	Move<VkDescriptorSet>			graphicsDescriptorSet;

	// Cubemap tests use separate image views for each side of a cubemap.
	vector<Move<VkImageView>>		cubeSamplerImageViews;
	vector<Move<VkImageView>>		cubeSampler2ImageViews;
	vector<VkDescriptorImageInfo>	cubeSamplerDscrImageInfos;
	vector<VkDescriptorImageInfo>	cubeSampler2DscrImageInfos;
	vector<Move<VkDescriptorSet>>	cubeSamplerDescriptorSets;

	const auto						graphicsDscrSetLayout	(DescriptorSetLayoutBuilder()
											.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler2.get())
											.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler.get())
											.build(vk, device));

	if (m_cubemap)
	{
		DescriptorSetUpdateBuilder updateBuilder;
		for (int i = 0; i < FACES; i++)
		{
			cubeSamplerImageViews.emplace_back(makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageFormat, cubeSubresourceRanges[i], &imageViewUsageInfo));
			cubeSamplerDscrImageInfos.emplace_back(makeDescriptorImageInfo(sampler2.get(), *cubeSamplerImageViews[i], VK_IMAGE_LAYOUT_GENERAL));
			cubeSamplerDescriptorSets.emplace_back(makeDescriptorSet(vk, device, *descriptorPool, *graphicsDscrSetLayout));
			updateBuilder.writeSingle(*cubeSamplerDescriptorSets[i], DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &cubeSamplerDscrImageInfos[i]);
		}

		if (m_twoSamplers)
		{
			for (int i = 0; i < FACES; i++)
			{
				cubeSampler2ImageViews.emplace_back(makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageViewFormat, cubeSubresourceRanges[i]));
				cubeSampler2DscrImageInfos.emplace_back(makeDescriptorImageInfo(sampler.get(), *cubeSampler2ImageViews[i], VK_IMAGE_LAYOUT_GENERAL));
				updateBuilder.writeSingle(*cubeSamplerDescriptorSets[i], DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &cubeSampler2DscrImageInfos[i]);
			}
		}
		updateBuilder.update(vk, device);
	}
	else
	{
		const VkImageSubresourceRange	subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, 1);
		DescriptorSetUpdateBuilder		updateBuilder;

		sampledImageView2 = makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageFormat, subresourceRange, &imageViewUsageInfo);
		samplerDscrImageInfo2 = makeDescriptorImageInfo(sampler2.get(), *sampledImageView2, VK_IMAGE_LAYOUT_GENERAL);
		graphicsDescriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *graphicsDscrSetLayout);

		if (m_twoSamplers)
		{
			sampledImageView = makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageViewFormat, subresourceRange);
			samplerDscrImageInfo = makeDescriptorImageInfo(sampler.get(), *sampledImageView, VK_IMAGE_LAYOUT_GENERAL);
		}

		updateBuilder.writeSingle(*graphicsDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &samplerDscrImageInfo2);
		if (m_twoSamplers)
			updateBuilder.writeSingle(*graphicsDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &samplerDscrImageInfo);

		updateBuilder.update(vk, device);
	}

	// Sampled values will be rendered on this image.
	const VkImageSubresourceRange	targetSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, 1);
	const VkImageCreateInfo			targetImageCreateInfo	= makeImageCreateInfo(imageSize, renderedImageFormat, false, false);

	const ImageWithMemory			targetImage				(vk, device, m_context.getDefaultAllocator(), targetImageCreateInfo, MemoryRequirement::Any);
	Move<VkImageView>				targetImageView			= makeImageView(vk, device, *targetImage, VK_IMAGE_VIEW_TYPE_2D, renderedImageFormat, targetSubresourceRange);

	// Clear the render target image as black and do a layout transition.
	clearColorImage(vk, device, m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(), targetImage.get(),
					Vec4(0, 0, 0, 0), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	const VkPushConstantRange		pushConstantRange2		=
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,	// VkShaderStageFlags    stageFlags;
		0u,								// uint32_t              offset;
		(deUint32)sizeof(deUint32),		// uint32_t              size;
	};

	const Move<VkPipelineLayout>	graphicsPipelineLayout	= makePipelineLayout(vk, device, 1, &(*graphicsDscrSetLayout), 1, &pushConstantRange2);

	// Vertices for a full quad and texture coordinates for each vertex.
	const vector<Vertex>			vertices				= generateVertices();
	Move<VkBuffer>					vertexBuffer			= makeVertexBuffer(vk, device, queueFamilyIndex);
	de::MovePtr<Allocation>			vertexBufferAlloc		= bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);
	const VkDeviceSize				vertexBufferOffset		= 0ull;
	deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], sizeInBytes(vertices));
	flushAlloc(vk, device, *vertexBufferAlloc);

	const auto						vtxBindingDescription	= Vertex::getBindingDescription();
	const auto						vtxAttrDescriptions		= Vertex::getAttributeDescriptions();

	const VkPipelineVertexInputStateCreateInfo vtxInputInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType
		nullptr,													// const void*                                 pNext
		0u,															// VkPipelineVertexInputStateCreateFlags       flags
		1u,															// deUint32                                    vertexBindingDescriptionCount
		&vtxBindingDescription,										// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		static_cast<deUint32>(vtxAttrDescriptions.size()),			// deUint32                                    vertexAttributeDescriptionCount
		vtxAttrDescriptions.data(),									// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	Move<VkShaderModule>			vertexShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	Move<VkShaderModule>			fragmentShader			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);

	// Create a render pass, a framebuffer, and the second pipeline.
	Move<VkRenderPass>				renderPass				= makeRenderPass(vk, device, renderedImageFormat, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD,
																			 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	Move<VkFramebuffer>				framebuffer				= makeFramebuffer(vk, device, *renderPass, targetImageView.get(), renderSize.width, renderSize.height);
	const Move<VkPipeline>			graphicsPipeline		= makeGraphicsPipeline(vk, device, graphicsPipelineLayout.get(), vertexShader.get(), DE_NULL,
																				   DE_NULL, DE_NULL, fragmentShader.get(), renderPass.get(), viewports, scissors,
																				   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vtxInputInfo);

	// Create a result buffer.
	const VkBufferCreateInfo		resultBufferCreateInfo	= makeBufferCreateInfo(BUFFERSIZE, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	Move<VkBuffer>					resultBuffer			= createBuffer(vk, device, &resultBufferCreateInfo);
	MovePtr<Allocation>				resultBufferMemory		= allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer), MemoryRequirement::HostVisible);
	TextureLevel					resultImage				(mapVkFormat(renderedImageFormat), renderSize.width, renderSize.height, 1);
	VK_CHECK(vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));

	// Generate a reference image.
	TextureLevel					expectedImage			= makeReferenceImage(renderedImageFormat, WIDTH, HEIGHT);

	beginCommandBuffer(vk, *cmdBuffer);

	// Do a layout transition for the storage image.
	const VkImageSubresourceRange	imageSubresourceRange2	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, m_cubemap ? 6 : 1);
	const auto						barrier1				= makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
																					 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
																					 storageImage.get(), imageSubresourceRange2);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier1);

	// Bind the vertices and the descriptors used in the graphics pipeline.
	vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

	// Fill the storage image and sample it twice.
	for (int pass = 0; pass < 2; pass++)
	{
		// If both samplers are enabled, it's not necessary to run the compute shader twice since it already writes
		// the expected values on the first pass. The first sampler uses an uncompressed image format so the result
		// image will contain garbage if the second sampler doesn't work properly.
		if (!m_twoSamplers || pass == 0)
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
			vk.cmdPushConstants(*cmdBuffer, *computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(deInt32), &pass);

			// If cubemaps are enabled, loop over six times and bind the next face of the cubemap image on each iteration.
			if (m_cubemap)
			{
				for (int face = 0; face < FACES; face++)
				{
					vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u, &(cubeStorageDscrSets[face].get()), 0u, DE_NULL);
					vk.cmdDispatch(*cmdBuffer, WIDTH, HEIGHT, 1u);
				}
			}
			else
			{
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u, &storageImageDescriptorSet.get(), 0u, DE_NULL);
				vk.cmdDispatch(*cmdBuffer, WIDTH, HEIGHT, 1u);
			}

			const auto barrier2 = makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
														 VK_IMAGE_LAYOUT_GENERAL, storageImage.get(), imageSubresourceRange);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier2);
		}

		vk.cmdPushConstants(*cmdBuffer, *graphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(deInt32), &pass);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

		// If cubemaps are enabled, loop over six times and bind the next face of the cubemap image on each iteration.
		if (m_cubemap)
		{
			for (int face = 0; face < FACES; face++)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelineLayout, 0u, 1u, &(cubeSamplerDescriptorSets[face].get()), 0u, DE_NULL);

				beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, imageSize.x(), imageSize.y()),0u, DE_NULL);
				vk.cmdDraw(*cmdBuffer, 6u, 1u, 0u, 0u);
				endRenderPass(vk, *cmdBuffer);

				if (face < FACES-1)
				{
					const auto barrier4 = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																 targetImage.get(), targetSubresourceRange);
					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
										  0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier4);
				}
			}
		}
		else
		{
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelineLayout, 0u, 1u, &(graphicsDescriptorSet.get()), 0u, DE_NULL);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, imageSize.x(), imageSize.y()),0u, DE_NULL);
			vk.cmdDraw(*cmdBuffer, 6u, 1u, 0u, 0u);
			endRenderPass(vk, *cmdBuffer);
		}

		if (pass == 0)
		{
			const auto barrier3 = makeImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
														 VK_IMAGE_LAYOUT_GENERAL, storageImage.get(), imageSubresourceRange);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier3);

			const auto barrier4 = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
														 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
														 targetImage.get(), targetSubresourceRange);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier4);
		}
	}

	// Copy the sampled values from the target image into the result image.
	copyImageToBuffer(vk, *cmdBuffer, *targetImage, *resultBuffer, tcu::IVec2(WIDTH, HEIGHT), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateAlloc(vk, device, *resultBufferMemory);

	clear(resultImage.getAccess(), tcu::IVec4(0));
	copy(resultImage.getAccess(), ConstPixelBufferAccess(resultImage.getFormat(), resultImage.getSize(), resultBufferMemory->getHostPtr()));

	bool							result					= true;

	if (m_cubemap)
	{
		// The first pass draws pure red on the faces and the second pass redraws them with pure blue.
		// Sampling anywhere should produce colors with a 0.0 red component and > 0.0 blue and alpha components.
		for (deUint32 y = 0; y < renderSize.height; y++)
		{
			for (deUint32 x = 0; x < renderSize.width; x++)
			{
				const deUint8* ptr = static_cast<const deUint8 *>(resultImage.getAccess().getPixelPtr(x, y, 0));
				const tcu::IVec4 val = tcu::IVec4(ptr[0], ptr[1], ptr[2], ptr[3]);
				if (!(val[0] == 0 && val[2] > 0 && val[3] > 0))
					result = false;
			}
		}

		// Log attachment contents.
		m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Attachment ", "")
											<< tcu::TestLog::Image("Rendered image", "Rendered image", resultImage.getAccess())
											<< tcu::TestLog::EndImageSet;
	}
	else
	{
		// Each test case should render pure blue as the result.
		result = tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Image Comparison", "",
											expectedImage.getAccess(), resultImage.getAccess(),
											tcu::Vec4(0.01f), tcu::COMPARE_LOG_RESULT);
	}

	if (result)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

class SampleDrawnTextureTest : public TestCase
{
public:
						SampleDrawnTextureTest	(tcu::TestContext&	testCtx,
												 const std::string&	name,
												 const std::string&	description,
												 const VkFormat		imageFormat,
												 const VkFormat		imageViewFormat,
												 const bool			twoSamplers,
												 const bool			cubemap);

	void				initPrograms			(SourceCollections& programCollection) const;
	TestInstance*		createInstance			(Context&			context) const;
	virtual void		checkSupport			(Context& context) const;

private:
	const VkFormat		m_imageFormat;
	const VkFormat		m_imageViewFormat;
	const bool			m_twoSamplers;
	const bool			m_cubemap;
};

SampleDrawnTextureTest::SampleDrawnTextureTest (tcu::TestContext&	testCtx,
												const std::string&	name,
												const std::string&	description,
												const VkFormat		imageFormat,
												const VkFormat		imageViewFormat,
												const bool			twoSamplers,
												const bool			cubemap)
	: TestCase	(testCtx, name, description)
	, m_imageFormat	(imageFormat)
	, m_imageViewFormat	(imageViewFormat)
	, m_twoSamplers (twoSamplers)
	, m_cubemap (cubemap)
{
}

void SampleDrawnTextureTest::checkSupport(Context& context) const
{
	const auto&				vki					= context.getInstanceInterface();
	const auto				physicalDevice		= context.getPhysicalDevice();
	const auto				usageFlags			= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
												  | VK_IMAGE_USAGE_SAMPLED_BIT;
	const bool				haveMaintenance2	= context.isDeviceFunctionalitySupported("VK_KHR_maintenance2");

	// Check that:
	// - An image can be created with usage flags that are not supported by the image format
	//   but are supported by an image view created for the image.
	// - VkImageViewUsageCreateInfo can be used to override implicit usage flags derived from the image.
	if (!haveMaintenance2)
		TCU_THROW(NotSupportedError, "Device does not support extended image usage flags nor overriding implicit usage flags");

	VkImageFormatProperties	imageFormatProperties;

	if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_IMAGE_TYPE_2D,
												   VK_IMAGE_TILING_OPTIMAL, usageFlags, (VkImageCreateFlags)0,
												   &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "BC1 compressed texture formats not supported.");

	if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, VK_FORMAT_BC3_UNORM_BLOCK, VK_IMAGE_TYPE_2D,
												   VK_IMAGE_TILING_OPTIMAL, usageFlags, (VkImageCreateFlags)0,
												   &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "BC3 compressed texture formats not supported.");

	if (m_cubemap && vki.getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(), m_imageFormat,
																VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
																usageFlags, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
																&imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Compressed images cannot be created with the VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag");
}

void SampleDrawnTextureTest::initPrograms (SourceCollections& programCollection) const
{
	// Pure red, green, and blue compressed with the BC1 and BC3 algorithms.
	std::string			bc1_red		= "uvec4(4160813056u, 0u, 4160813056u, 0u);\n";
	std::string			bc1_blue	= "uvec4(2031647, 0u, 2031647, 0u);\n";
	std::string			bc3_red		= "uvec4(4294967295u, 4294967295u, 4160813056u, 0u);\n";
	std::string			bc3_blue	= "uvec4(4294967295u, 4294967295u, 2031647, 0u);\n";

	std::string			red			= (m_imageFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK) ? bc1_red : bc3_red;
	std::string			blue		= (m_imageFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK) ? bc1_blue : bc3_blue;

	std::ostringstream	computeSrc;

	// Generate the compute shader.
	computeSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
	computeSrc << "layout(set = 0, binding = 0, rgba32ui) uniform highp uimage2D img;\n";
	computeSrc << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";

	if (!m_twoSamplers)
	{
		computeSrc
			<< "layout(push_constant) uniform constants {\n"
			<< "    int pass;\n"
			<< "} pc;\n";
	}

	computeSrc << "void main() {\n";

	if (m_twoSamplers)
		computeSrc << "    uvec4 color = " << blue;
	else
	{
		computeSrc << "    uvec4 color = " << red;
		computeSrc << "    if (pc.pass == 1)\n";
		computeSrc << "        color = " << blue;
	}

	computeSrc
	<< "    for (int x = 0; x < " << WIDTH << "; x++)\n"
	<< "        for (int y = 0; y < " << HEIGHT << "; y++)\n"
	<< "            imageStore(img, ivec2(x, y), color);\n"
	<< "}\n";

	// Generate the vertex shader.
	std::ostringstream vertexSrc;
	vertexSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) in highp vec4 a_position;\n"
		<< "layout(location = 1) in vec2 inTexCoord;\n"
		<< "layout(location = 1) out vec2 fragTexCoord;\n"
		<< "void main (void) {\n"
		<< "    gl_Position = a_position;\n"
		<< "    fragTexCoord = inTexCoord;\n"
		<< "}\n";

	// Generate the fragment shader.
	std::ostringstream fragmentSrc;
	fragmentSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) out vec4 outColor;\n"
		<< "layout(location = 1) in vec2 fragTexCoord;\n";

	fragmentSrc << "layout(binding = 0) uniform sampler2D compTexSampler;\n";

	if (m_twoSamplers)
	{
		fragmentSrc
			<< "layout(binding = 1) uniform usampler2D texSampler;\n"
			<< "layout(push_constant) uniform constants {\n"
			<< "    int pass;\n"
			<< "} pc;\n"
			<< "void main() {\n"
			<< "    if (pc.pass == 1)\n"
			<< "        outColor = texture(compTexSampler, fragTexCoord);\n"
			<< "    else"
			<< "        outColor = texture(texSampler, fragTexCoord);\n";
	}
	else
	{
		fragmentSrc
			<< "void main() {\n"
			<< "    outColor = texture(compTexSampler, fragTexCoord);\n";
	}

	fragmentSrc << "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(computeSrc.str());
	programCollection.glslSources.add("vert") << glu::VertexSource(vertexSrc.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragmentSrc.str());
}

TestInstance* SampleDrawnTextureTest::createInstance (Context& context) const
{
	return new SampleDrawnTextureTestInstance(context, m_imageFormat, m_imageViewFormat, m_twoSamplers, m_cubemap);
}

} // anonymous ns

tcu::TestCaseGroup* createImageSampleDrawnTextureTests	(tcu::TestContext& testCtx)
{
	/* If both samplers are enabled, the test works as follows:
	 *
	 * Pass 0:
	 * - Compute shader fills a storage image with values that are pure blue compressed with
	 *   either the BC1 or BC3 algorithm.
	 * - Fragment shader samples the image and draws the values on a target image.
	 * - As the sampled values are accessed through an image view using an uncompressed
	 *   format, they remain compressed and the drawn image ends up being garbage.
	 * Pass 1:
	 * - Fragment shader samples the image. On this pass, the image view uses
	 *   a block-compressed format and correctly interprets the sampled values.
	 * - As the values are uncompressed now, the target image is filled
	 *   with pure blue and the test passes.

	 * Only one sampler enabled:
	 * Pass 0:
	 * - Compute shader fills a storage image with values that are pure red compressed
	 *   with either the BC1 or BC3 algorithm.
	 * - Fragment shader samples the image through an image view which interprets the values
	 *   correctly. The values are drawn on a target image. The test doesn't pass yet
	 *   since the image is red.
	 * Pass 1:
	 * - Compute shader fills the storage image with values that are pure blue compressed
	 *   with the same algorithm as on the previous pass.
	 * - Fragment shader samples the image through an image view which interprets the values
	 *   correctly. The values are drawn on the target image and the test passes.
	 *
	 * If cubemaps are enabled:
	 * Pass 0:
	 * - If both samplers are enabled, draw compressed pure blue on the faces. Otherwise pure red.
	 * - Sample the image through an image view with or without compressed format as in the cases
	 *   without cubemaps.
	 * Pass 1:
	 * - If only one sampler is enabled, redraw the faces with pure blue
	 * - Sample the image. Sampling should produce colors with a 0.0 red component and with > 0.0
	 *   blue and alpha components.
	 */

	const bool twoSamplers	= true;
	const bool cubemap		= true;

	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "sample_texture", "Sample texture that has been rendered to tests"));

	testGroup->addChild(new SampleDrawnTextureTest(testCtx, "128_bit_compressed_format_cubemap", "", VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_R32G32B32A32_UINT, !twoSamplers, cubemap));
	testGroup->addChild(new SampleDrawnTextureTest(testCtx, "64_bit_compressed_format_cubemap", "", VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_R32G32_UINT, !twoSamplers, cubemap));
	testGroup->addChild(new SampleDrawnTextureTest(testCtx, "64_bit_compressed_format_two_samplers_cubemap", "", VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_R32G32_UINT, twoSamplers, cubemap));
	testGroup->addChild(new SampleDrawnTextureTest(testCtx, "128_bit_compressed_format_two_samplers_cubemap", "", VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_R32G32B32A32_UINT, twoSamplers, cubemap));

	testGroup->addChild(new SampleDrawnTextureTest(testCtx, "64_bit_compressed_format", "", VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_R32G32_UINT, !twoSamplers, false));
	testGroup->addChild(new SampleDrawnTextureTest(testCtx, "64_bit_compressed_format_two_samplers", "", VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_R32G32_UINT, twoSamplers, false));
	testGroup->addChild(new SampleDrawnTextureTest(testCtx, "128_bit_compressed_format", "", VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_R32G32B32A32_UINT, !twoSamplers, false));
	testGroup->addChild(new SampleDrawnTextureTest(testCtx, "128_bit_compressed_format_two_samplers", "", VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_R32G32B32A32_UINT, twoSamplers, false));

	return testGroup.release();
}

} // image
} // vkt
