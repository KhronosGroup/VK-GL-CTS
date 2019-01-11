/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Tests sparse render target.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassSubpassDependencyTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "rrRenderer.hpp"
#include "deRandom.hpp"
#include "deMath.h"

using namespace vk;

using tcu::UVec4;
using tcu::Vec2;
using tcu::UVec2;
using tcu::Vec4;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;

using tcu::TestLog;

using std::string;
using std::vector;
using de::SharedPtr;

typedef de::SharedPtr<Unique<VkImage> >					SharedPtrVkImage;
typedef de::SharedPtr<Unique<VkImageView> >				SharedPtrVkImageView;
typedef de::SharedPtr<Unique<VkPipeline> >				SharedPtrVkPipeline;
typedef de::SharedPtr<Unique<VkSampler> >				SharedPtrVkSampler;
typedef de::SharedPtr<Unique<VkRenderPass> >			SharedPtrVkRenderPass;
typedef de::SharedPtr<Unique<VkFramebuffer> >			SharedPtrVkFramebuffer;
typedef de::SharedPtr<Unique<VkDescriptorPool> >		SharedPtrVkDescriptorPool;
typedef de::SharedPtr<Unique<VkDescriptorSetLayout> >	SharedPtrVkDescriptorLayout;
typedef de::SharedPtr<Unique<VkDescriptorSet> >			SharedPtrVkDescriptorSet;
typedef de::SharedPtr<Unique<VkPipelineLayout> >		SharedPtrVkPipelineLayout;
typedef de::SharedPtr<Unique<VkPipeline> >				SharedPtrVkPipeline;

namespace vkt
{
namespace
{
using namespace renderpass;

template<typename T>
inline SharedPtr<Unique<T> > makeSharedPtr(Move<T> move)
{
	return SharedPtr<Unique<T> >(new Unique<T>(move));
}

// Reference renderer shaders
class DepthVertShader : public rr::VertexShader
{
public:
	DepthVertShader (void)
	: rr::VertexShader (1, 1)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			packets[packetNdx]->position	= rr::readVertexAttribFloat(inputs[0],
																		packets[packetNdx]->instanceNdx,
																		packets[packetNdx]->vertexNdx);

			packets[packetNdx]->outputs[0]	= rr::readVertexAttribFloat(inputs[0],
																		packets[packetNdx]->instanceNdx,
																		packets[packetNdx]->vertexNdx);
		}
	}
};

class DepthFragShader : public rr::FragmentShader
{
public:
	DepthFragShader (void)
	: rr::FragmentShader(1, 1)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments (rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			rr::FragmentPacket& packet = packets[packetNdx];
			for (deUint32 fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
			{
				const tcu::Vec4 vtxPosition = rr::readVarying<float>(packet, context, 0, fragNdx);

				rr::writeFragmentDepth(context, packetNdx, fragNdx, 0, vtxPosition.z());
			}
		}
	}
};

de::MovePtr<Allocation> createBufferMemory (const DeviceInterface&	vk,
											VkDevice				device,
											Allocator&				allocator,
											VkBuffer				buffer)
{
	de::MovePtr<Allocation> allocation (allocator.allocate(getBufferMemoryRequirements(vk, device, buffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(device, buffer, allocation->getMemory(), allocation->getOffset()));

	return allocation;
}

Move<VkImageView> createImageView (const DeviceInterface&	vk,
								   VkDevice					device,
								   VkImageViewCreateFlags	flags,
								   VkImage					image,
								   VkImageViewType			viewType,
								   VkFormat					format,
								   VkComponentMapping		components,
								   VkImageSubresourceRange	subresourceRange)
{
	const VkImageViewCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		flags,										// VkImageViewCreateFlags	flags
		image,										// VkImage					image
		viewType,									// VkImageViewType			viewType
		format,										// VkFormat					format
		components,									// VkComponentMapping		components
		subresourceRange,							// VkImageSubresourceRange	subresourceRange
	};

	return createImageView(vk, device, &pCreateInfo);
}

vector<SharedPtrVkImageView> createImageViews (const DeviceInterface&	vkd,
											   VkDevice					device,
											   vector<SharedPtrVkImage>	images,
											   VkFormat					format,
											   VkImageAspectFlags		aspect)
{
	vector<SharedPtrVkImageView> imageViews;

	for (size_t imageViewNdx = 0; imageViewNdx < images.size(); imageViewNdx++)
	{
		const VkImageSubresourceRange range =
		{
			aspect,	// VkImageAspectFlags	aspectMask
			0u,		// uint32_t				baseMipLevel
			1u,		// uint32_t				levelCount
			0u,		// uint32_t				baseArrayLayer
			1u		// uint32_t				layerCount
		};

		imageViews.push_back(makeSharedPtr(createImageView(vkd, device, 0u, **images[imageViewNdx], VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range)));
	}

	return imageViews;
}

Move<VkBuffer> createBuffer (const DeviceInterface&	vkd,
							 VkDevice				device,
							 VkFormat				format,
							 deUint32				width,
							 deUint32				height)
{
	const VkBufferUsageFlags	bufferUsage	(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkDeviceSize			pixelSize	= mapVkFormat(format).getPixelSize();
	const VkBufferCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,								// const void*			pNext
		0u,										// VkBufferCreateFlags	flags
		width * height * pixelSize,				// VkDeviceSize			size
		bufferUsage,							// VkBufferUsageFlags	usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0u,										// uint32_t				queueFamilyIndexCount
		DE_NULL									// const uint32_t*		pQueueFamilyIndices
	};

	return createBuffer(vkd, device, &createInfo);
}

vector<SharedPtrVkDescriptorLayout> createDescriptorSetLayouts (const DeviceInterface&		vkd,
																VkDevice					device,
																vector<SharedPtrVkSampler>&	samplers)
{
	vector<SharedPtrVkDescriptorLayout> layouts;

	for (size_t layoutNdx = 0; layoutNdx < samplers.size(); layoutNdx++)
	{
		const VkDescriptorSetLayoutBinding		bindings	=
		{
				0u,											// uint32_t				binding
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	// VkDescriptorType		descriptorType
				1u,											// uint32_t				descriptorCount
				VK_SHADER_STAGE_FRAGMENT_BIT,				// VkShaderStageFlags	stageFlags
				&**samplers[layoutNdx]						// const VkSampler*		pImmutableSamplers
		};

		const VkDescriptorSetLayoutCreateInfo	createInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkDescriptorSetLayoutCreateFlags		flags
			1u,														// uint32_t								bindingCount
			&bindings												// const VkDescriptorSetLayoutBinding*	pBindings
		};

		layouts.push_back(makeSharedPtr(createDescriptorSetLayout(vkd, device, &createInfo)));
	}

	return layouts;
}

vector<SharedPtrVkDescriptorPool> createDescriptorPools (const DeviceInterface&					vkd,
														 VkDevice								device,
														 vector<SharedPtrVkDescriptorLayout>&	layouts,
														 VkDescriptorType						type)
{
	vector<SharedPtrVkDescriptorPool> descriptorPools;

	for (size_t poolNdx = 0; poolNdx < layouts.size(); poolNdx++)
	{
		const VkDescriptorPoolSize			size		=
		{
			type,	// VkDescriptorType		type
			1u		// uint32_t				descriptorCount
		};

		const VkDescriptorPoolCreateInfo	createInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,		// VkStructureType				sType
			DE_NULL,											// const void*					pNext
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,	// VkDescriptorPoolCreateFlags	flags
			1u,													// uint32_t						maxSets
			1u,													// uint32_t						poolSizeCount
			&size												// const VkDescriptorPoolSize*	pPoolSizes
		};

		descriptorPools.push_back(makeSharedPtr(createDescriptorPool(vkd, device, &createInfo)));
	}

	return descriptorPools;
}

struct ExternalTestConfig
{
	ExternalTestConfig	(VkFormat			format_,
						 UVec2				imageSize_,
						 vector<RenderPass>	renderPasses_,
						 RenderPassType		renderPassType_,
						 deUint32			blurKernel_ = 4)
		: format			(format_)
		, imageSize			(imageSize_)
		, renderPasses		(renderPasses_)
		, renderPassType	(renderPassType_)
		, blurKernel		(blurKernel_)
	{
	}

	VkFormat			format;
	UVec2				imageSize;
	vector<RenderPass>	renderPasses;
	RenderPassType		renderPassType;
	deUint32			blurKernel;
};

class ExternalDependencyTestInstance : public TestInstance
{
public:
											ExternalDependencyTestInstance	(Context& context, ExternalTestConfig testConfig);
											~ExternalDependencyTestInstance	(void);

	vector<SharedPtrVkImage>				createAndAllocateImages			(const DeviceInterface&					vk,
																			 VkDevice								device,
																			 Allocator&								allocator,
																			 vector<de::SharedPtr<Allocation> >&	imageMemories,
																			 deUint32								universalQueueFamilyIndex,
																			 VkFormat								format,
																			 deUint32								width,
																			 deUint32								height,
																			 vector<RenderPass>						renderPasses);

	vector<SharedPtrVkSampler>				createSamplers					(const DeviceInterface&					vkd,
																			 const VkDevice							device,
																			 vector<RenderPass>&					renderPasses);

	vector<SharedPtrVkRenderPass>			createRenderPasses				(const DeviceInterface&					vkd,
																			 VkDevice								device,
																			 vector<RenderPass>						renderPassInfos,
																			 const RenderPassType					renderPassType);

	 vector<SharedPtrVkFramebuffer>			createFramebuffers				(const DeviceInterface&					vkd,
																			 VkDevice								device,
																			 vector<SharedPtrVkRenderPass>&			renderPasses,
																			 vector<SharedPtrVkImageView>&			dstImageViews,
																			 deUint32								width,
																			 deUint32								height);

	vector<SharedPtrVkPipelineLayout>		createRenderPipelineLayouts		(const DeviceInterface&					vkd,
																			 VkDevice								device,
																			 vector<SharedPtrVkRenderPass>&			renderPasses,
																			 vector<SharedPtrVkDescriptorLayout>&	descriptorSetLayouts);

	vector<SharedPtrVkPipeline>				createRenderPipelines			(const DeviceInterface&					vkd,
																			 VkDevice								device,
																			 vector<SharedPtrVkRenderPass>&			renderPasses,
																			 vector<SharedPtrVkPipelineLayout>&		pipelineLayouts,
																			 const BinaryCollection&				binaryCollection,
																			 deUint32								width,
																			 deUint32								height);

	vector<SharedPtrVkDescriptorSet>		createDescriptorSets			(const DeviceInterface&					vkd,
																			 VkDevice								device,
																			 vector<SharedPtrVkDescriptorPool>&		pools,
																			 vector<SharedPtrVkDescriptorLayout>&	layouts,
																			 vector<SharedPtrVkImageView>&			imageViews,
																			 vector<SharedPtrVkSampler>&			samplers);

	tcu::TestStatus							iterate							(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus							iterateInternal					(void);

private:
	const bool								m_extensionSupported;
	const RenderPassType					m_renderPassType;

	const deUint32							m_width;
	const deUint32							m_height;
	const deUint32							m_blurKernel;
	const VkFormat							m_format;

	vector<de::SharedPtr<Allocation> >		m_imageMemories;
	vector<SharedPtrVkImage>				m_images;
	vector<SharedPtrVkImageView>			m_imageViews;
	vector<SharedPtrVkSampler>				m_samplers;

	const Unique<VkBuffer>					m_dstBuffer;
	const de::UniquePtr<Allocation>			m_dstBufferMemory;

	vector<SharedPtrVkRenderPass>			m_renderPasses;
	vector<SharedPtrVkFramebuffer>			m_framebuffers;

	vector<SharedPtrVkDescriptorLayout>		m_subpassDescriptorSetLayouts;
	vector<SharedPtrVkDescriptorPool>		m_subpassDescriptorPools;
	vector<SharedPtrVkDescriptorSet>		m_subpassDescriptorSets;

	vector<SharedPtrVkPipelineLayout>		m_renderPipelineLayouts;
	vector<SharedPtrVkPipeline>				m_renderPipelines;

	const Unique<VkCommandPool>				m_commandPool;
	tcu::ResultCollector					m_resultCollector;
};

ExternalDependencyTestInstance::ExternalDependencyTestInstance (Context& context, ExternalTestConfig testConfig)
	: TestInstance					(context)
	, m_extensionSupported			((testConfig.renderPassType == RENDERPASS_TYPE_RENDERPASS2) && context.requireDeviceExtension("VK_KHR_create_renderpass2"))
	, m_renderPassType				(testConfig.renderPassType)
	, m_width						(testConfig.imageSize.x())
	, m_height						(testConfig.imageSize.y())
	, m_blurKernel					(testConfig.blurKernel)
	, m_format						(testConfig.format)
	, m_images						(createAndAllocateImages(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), m_imageMemories, context.getUniversalQueueFamilyIndex(), m_format, m_width, m_height, testConfig.renderPasses))
	, m_imageViews					(createImageViews(context.getDeviceInterface(), context.getDevice(), m_images, m_format, VK_IMAGE_ASPECT_COLOR_BIT))
	, m_samplers					(createSamplers(context.getDeviceInterface(), context.getDevice(), testConfig.renderPasses))
	, m_dstBuffer					(createBuffer(context.getDeviceInterface(), context.getDevice(), m_format, m_width, m_height))
	, m_dstBufferMemory				(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_dstBuffer))
	, m_renderPasses				(createRenderPasses(context.getDeviceInterface(), context.getDevice(), testConfig.renderPasses, testConfig.renderPassType))
	, m_framebuffers				(createFramebuffers(context.getDeviceInterface(), context.getDevice(), m_renderPasses, m_imageViews, m_width, m_height))
	, m_subpassDescriptorSetLayouts	(createDescriptorSetLayouts(context.getDeviceInterface(), context.getDevice(), m_samplers))
	, m_subpassDescriptorPools		(createDescriptorPools(context.getDeviceInterface(), context.getDevice(), m_subpassDescriptorSetLayouts, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
	, m_subpassDescriptorSets		(createDescriptorSets(context.getDeviceInterface(), context.getDevice(), m_subpassDescriptorPools, m_subpassDescriptorSetLayouts, m_imageViews, m_samplers))
	, m_renderPipelineLayouts		(createRenderPipelineLayouts(context.getDeviceInterface(), context.getDevice(), m_renderPasses, m_subpassDescriptorSetLayouts))
	, m_renderPipelines				(createRenderPipelines(context.getDeviceInterface(), context.getDevice(), m_renderPasses, m_renderPipelineLayouts, context.getBinaryCollection(), m_width, m_height))
	, m_commandPool					(createCommandPool(context.getDeviceInterface(), context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
}

ExternalDependencyTestInstance::~ExternalDependencyTestInstance (void)
{
}

vector<SharedPtrVkImage> ExternalDependencyTestInstance::createAndAllocateImages (const DeviceInterface&				vk,
																				  VkDevice								device,
																				  Allocator&							allocator,
																				  vector<de::SharedPtr<Allocation> >&	imageMemories,
																				  deUint32								universalQueueFamilyIndex,
																				  VkFormat								format,
																				  deUint32								width,
																				  deUint32								height,
																				  vector<RenderPass>					renderPasses)
{
	vector<SharedPtrVkImage> images;

	for (size_t imageNdx = 0; imageNdx < renderPasses.size(); imageNdx++)
	{
		const VkExtent3D		imageExtent		=
		{
			width,		// uint32_t		width
			height,		// uint32_t		height
			1u			// uint32_t		depth
		};

		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,								// const void*				pNext
			0u,										// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			format,									// VkFormat					format
			imageExtent,							// VkExtent3D				extent
			1u,										// uint32_t					mipLevels
			1u,										// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT
				| VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			1u,										// uint32_t					queueFamilyIndexCount
			&universalQueueFamilyIndex,				// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		images.push_back(makeSharedPtr(vk::createImage(vk, device, &imageCreateInfo, DE_NULL)));
		imageMemories.push_back((de::SharedPtr<Allocation>)allocator.allocate(getImageMemoryRequirements(vk, device, **images[imageNdx]), MemoryRequirement::Any).release());
		VK_CHECK(vk.bindImageMemory(device, **images[imageNdx], imageMemories[imageNdx]->getMemory(), imageMemories[imageNdx]->getOffset()));
	}

	return images;
}

vector<SharedPtrVkSampler> ExternalDependencyTestInstance::createSamplers (const DeviceInterface&	vkd,
																		   const VkDevice			device,
																		   vector<RenderPass>&		renderPasses)
{
	vector<SharedPtrVkSampler> samplers;

	for (size_t samplerNdx = 0; samplerNdx < renderPasses.size() - 1; samplerNdx++)
	{
		const VkSamplerCreateInfo samplerInfo =
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkSamplerCreateFlags		flags
			VK_FILTER_NEAREST,							// VkFilter					magFilter
			VK_FILTER_NEAREST,							// VkFilter					minFilter
			VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode		mipmapMode
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeU
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeV
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// VkSamplerAddressMode		addressModeW
			0.0f,										// float					mipLodBias
			VK_FALSE,									// VkBool32					anisotropyEnable
			1.0f,										// float					maxAnisotropy
			VK_FALSE,									// VkBool32					compareEnable
			VK_COMPARE_OP_ALWAYS,						// VkCompareOp				compareOp
			0.0f,										// float					minLod
			0.0f,										// float					maxLod
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// VkBorderColor			borderColor
			VK_FALSE,									// VkBool32					unnormalizedCoordinates
		};

		samplers.push_back(makeSharedPtr(createSampler(vkd, device, &samplerInfo)));
	}

	return samplers;
}

vector<SharedPtrVkRenderPass> ExternalDependencyTestInstance::createRenderPasses (const DeviceInterface&	vkd,
																				  VkDevice					device,
																				  vector<RenderPass>		renderPassInfos,
																				  const RenderPassType		renderPassType)
{
	vector<SharedPtrVkRenderPass> renderPasses;

	for (size_t renderPassNdx = 0; renderPassNdx < renderPassInfos.size(); renderPassNdx++)
	{
		renderPasses.push_back(makeSharedPtr(createRenderPass(vkd, device, renderPassInfos[renderPassNdx], renderPassType)));
	}

	return renderPasses;
}

vector<SharedPtrVkFramebuffer> ExternalDependencyTestInstance::createFramebuffers (const DeviceInterface&			vkd,
																				   VkDevice							device,
																				   vector<SharedPtrVkRenderPass>&	renderPasses,
																				   vector<SharedPtrVkImageView>&	dstImageViews,
																				   deUint32							width,
																				   deUint32							height)
{
	vector<SharedPtrVkFramebuffer> framebuffers;

	for (size_t renderPassNdx = 0; renderPassNdx < renderPasses.size(); renderPassNdx++)
	{
		VkRenderPass renderPass (**renderPasses[renderPassNdx]);

		const VkFramebufferCreateInfo createInfo =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkFramebufferCreateFlags	flags
			renderPass,									// VkRenderPass				renderPass
			1u,											// uint32_t					attachmentCount
			&**dstImageViews[renderPassNdx],			// const VkImageView*		pAttachments
			width,										// uint32_t					width
			height,										// uint32_t					height
			1u											// uint32_t					layers
		};

		framebuffers.push_back(makeSharedPtr(createFramebuffer(vkd, device, &createInfo)));
	}

	return framebuffers;
}

vector<SharedPtrVkDescriptorSet> ExternalDependencyTestInstance::createDescriptorSets (const DeviceInterface&				vkd,
																					   VkDevice								device,
																					   vector<SharedPtrVkDescriptorPool>&	pools,
																					   vector<SharedPtrVkDescriptorLayout>&	layouts,
																					   vector<SharedPtrVkImageView>&		imageViews,
																					   vector<SharedPtrVkSampler>&			samplers)
{
	vector<SharedPtrVkDescriptorSet> descriptorSets;

	for (size_t setNdx = 0; setNdx < layouts.size(); setNdx++)
	{
		const VkDescriptorSetAllocateInfo allocateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType					sType
			DE_NULL,										// const void*						pNext
			**pools[setNdx],								// VkDescriptorPool					descriptorPool
			1u,												// uint32_t							descriptorSetCount
			&**layouts[setNdx]								// const VkDescriptorSetLayout*		pSetLayouts
		};

		descriptorSets.push_back(makeSharedPtr(allocateDescriptorSet(vkd, device, &allocateInfo)));

		{
			const VkDescriptorImageInfo	imageInfo	=
			{
				**samplers[setNdx],							// VkSampler		sampler
				**imageViews[setNdx],						// VkImageView		imageView
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout
			};

			const VkWriteDescriptorSet	write		=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,		// VkStructureType					sType
				DE_NULL,									// const void*						pNext
				**descriptorSets[setNdx],					// VkDescriptorSet					dstSet
				0u,											// uint32_t							dstBinding
				0u,											// uint32_t							dstArrayElement
				1u,											// uint32_t							descriptorCount
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	// VkDescriptorType					descriptorType
				&imageInfo,									// const VkDescriptorImageInfo*		pImageInfo
				DE_NULL,									// const VkDescriptorBufferInfo*	pBufferInfo
				DE_NULL										// const VkBufferView*				pTexelBufferView
			};

			vkd.updateDescriptorSets(device, 1u, &write, 0u, DE_NULL);
		}
	}

	return descriptorSets;
}

vector<SharedPtrVkPipelineLayout> ExternalDependencyTestInstance::createRenderPipelineLayouts (const DeviceInterface&				vkd,
																							   VkDevice								device,
																							   vector<SharedPtrVkRenderPass>&		renderPasses,
																							   vector<SharedPtrVkDescriptorLayout>&	descriptorSetLayouts)
{
	vector<SharedPtrVkPipelineLayout> pipelineLayouts;

	for (size_t renderPassNdx = 0; renderPassNdx < renderPasses.size(); renderPassNdx++)
	{
		const VkPipelineLayoutCreateInfo createInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,								// VkStructureType				sType
			DE_NULL,																	// const void*					pNext
			(vk::VkPipelineLayoutCreateFlags)0,											// VkPipelineLayoutCreateFlags	flags
			renderPassNdx > 0 ? 1u : 0u,												// deUint32						setLayoutCount
			renderPassNdx > 0 ? &**descriptorSetLayouts[renderPassNdx - 1] : DE_NULL,	// const VkDescriptorSetLayout*	pSetLayouts
			0u,																			// deUint32						pushConstantRangeCount
			DE_NULL																		// const VkPushConstantRange*	pPushConstantRanges
		};

		pipelineLayouts.push_back(makeSharedPtr(createPipelineLayout(vkd, device, &createInfo)));
	}

	return pipelineLayouts;
}

vector<SharedPtrVkPipeline> ExternalDependencyTestInstance::createRenderPipelines (const DeviceInterface&				vkd,
																				   VkDevice								device,
																				   vector<SharedPtrVkRenderPass>&		renderPasses,
																				   vector<SharedPtrVkPipelineLayout>&	pipelineLayouts,
																				   const BinaryCollection&				binaryCollection,
																				   deUint32								width,
																				   deUint32								height)
{
	vector<SharedPtrVkPipeline> pipelines;

	for (size_t renderPassNdx = 0; renderPassNdx < renderPasses.size(); renderPassNdx++)
	{
		const Unique<VkShaderModule>					vertexShaderModule		(createShaderModule(vkd, device, binaryCollection.get("quad-vert-" + de::toString(renderPassNdx)), 0u));
		const Unique<VkShaderModule>					fragmentShaderModule	(createShaderModule(vkd, device, binaryCollection.get("quad-frag-" + de::toString(renderPassNdx)), 0u));

		const VkPipelineVertexInputStateCreateInfo		vertexInputState		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			(VkPipelineVertexInputStateCreateFlags)0u,					// VkPipelineVertexInputStateCreateFlags	flags
			0u,															// uint32_t									vertexBindingDescriptionCount
			DE_NULL,													// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
			0u,															// uint32_t									vertexAttributeDescriptionCount
			DE_NULL														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
		};

		const std::vector<VkViewport>					viewports				(1, makeViewport(tcu::UVec2(width, height)));
		const std::vector<VkRect2D>						scissors				(1, makeRect2D(tcu::UVec2(width, height)));
		const VkRenderPass								renderPass				(**renderPasses[renderPassNdx]);
		const VkPipelineLayout							layout					(**pipelineLayouts[renderPassNdx]);

		pipelines.push_back(makeSharedPtr(makeGraphicsPipeline(vkd,									// const DeviceInterface&						vk
															   device,								// const VkDevice								device
															   layout,								// const VkPipelineLayout						pipelineLayout
															   *vertexShaderModule,					// const VkShaderModule							vertexShaderModule
															   DE_NULL,								// const VkShaderModule							tessellationControlShaderModule
															   DE_NULL,								// const VkShaderModule							tessellationEvalShaderModule
															   DE_NULL,								// const VkShaderModule							geometryShaderModule
															   *fragmentShaderModule,				// const VkShaderModule							fragmentShaderModule
															   renderPass,							// const VkRenderPass							renderPass
															   viewports,							// const std::vector<VkViewport>&				viewports
															   scissors,							// const std::vector<VkRect2D>&					scissors
															   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology					topology
															   0u,									// const deUint32								subpass
															   0u,									// const deUint32								patchControlPoints
															   &vertexInputState)));				// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo
	}

	return pipelines;
}

tcu::TestStatus ExternalDependencyTestInstance::iterate (void)
{
	switch (m_renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERPASS_TYPE_RENDERPASS2:
			return iterateInternal<RenderpassSubpass2>();
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
tcu::TestStatus ExternalDependencyTestInstance::iterateInternal (void)
{
	const DeviceInterface&								vkd					(m_context.getDeviceInterface());
	const Unique<VkCommandBuffer>						commandBuffer		(allocateCommandBuffer(vkd, m_context.getDevice(), *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo		(DE_NULL);

	beginCommandBuffer(vkd, *commandBuffer);

	for (size_t renderPassNdx = 0; renderPassNdx < m_renderPasses.size(); renderPassNdx++)
	{
		// Begin render pass
		{
			VkRect2D renderArea =
			{
				{ 0u, 0u },				// VkOffset2D	offset
				{ m_width, m_height }	// VkExtent2D	extent
			};

			const VkRenderPassBeginInfo beginInfo =
			{
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType
				DE_NULL,									// const void*			pNext
				**m_renderPasses[renderPassNdx],			// VkRenderPass			renderPass
				**m_framebuffers[renderPassNdx],			// VkFramebuffer		framebuffer
				renderArea,									// VkRect2D				renderArea
				0u,											// uint32_t				clearValueCount
				DE_NULL										// const VkClearValue*	pClearValues
			};

			RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
		}

		vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **m_renderPipelines[renderPassNdx]);

		// Use results from the previous pass as input texture
		if (renderPassNdx > 0)
			vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **m_renderPipelineLayouts[renderPassNdx], 0, 1, &**m_subpassDescriptorSets[renderPassNdx - 1], 0, DE_NULL);

		vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);

		RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);
	}

	// Memory barrier between rendering and copy
	{
		VkImageSubresourceRange		imageSubresourceRange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
			0u,							// uint32_t				baseMipLevel
			1u,							// uint32_t				levelCount
			0u,							// uint32_t				baseArrayLayer
			1u							// uint32_t				layerCount
		};

		const VkImageMemoryBarrier	barrier					=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask
			VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout			oldLayout
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t					srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t					dstQueueFamilyIndex
			**m_images[m_renderPasses.size() - 1],		// VkImage					image
			imageSubresourceRange						// VkImageSubresourceRange	subresourceRange
		};

		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
	}

	// Copy image memory to buffer
	{
		const VkImageSubresourceLayers imageSubresourceLayers =
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
			0u,							// deUint32				mipLevel
			0u,							// deUint32				baseArrayLayer
			1u							// deUint32				layerCount
		};

		const VkBufferImageCopy region =
		{
			0u,							// VkDeviceSize				bufferOffset
			0u,							// uint32_t					bufferRowLength
			0u,							// uint32_t					bufferImageHeight
			imageSubresourceLayers,		// VkImageSubresourceLayers	imageSubresource
			{ 0u, 0u, 0u },				// VkOffset3D				imageOffset
			{ m_width, m_height, 1u }	// VkExtent3D				imageExtent
		};

		vkd.cmdCopyImageToBuffer(*commandBuffer, **m_images[m_renderPasses.size() - 1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *m_dstBuffer, 1u, &region);
	}

	// Memory barrier between copy and host access
	{
		const VkBufferMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType
			DE_NULL,									// const void*		pNext
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask
			VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t			srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t			dstQueueFamilyIndex
			*m_dstBuffer,								// VkBuffer			buffer
			0u,											// VkDeviceSize		offset
			VK_WHOLE_SIZE								// VkDeviceSize		size
		};

		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &barrier, 0u, DE_NULL);
	}

	endCommandBuffer(vkd, *commandBuffer);
	submitCommandsAndWait(vkd, m_context.getDevice(), m_context.getUniversalQueue(), *commandBuffer);
	invalidateMappedMemoryRange(vkd, m_context.getDevice(), m_dstBufferMemory->getMemory(), m_dstBufferMemory->getOffset(), VK_WHOLE_SIZE);

	{
		const tcu::TextureFormat			format		(mapVkFormat(m_format));
		const void* const					ptr			(m_dstBufferMemory->getHostPtr());
		const tcu::ConstPixelBufferAccess	access		(format, m_width, m_height, 1, ptr);
		tcu::TextureLevel					reference	(format, m_width, m_height);
		tcu::TextureLevel					textureA	(format, m_width, m_height);
		tcu::TextureLevel					textureB	(format, m_width, m_height);

		for (deUint32 renderPassNdx = 0; renderPassNdx < m_renderPasses.size(); renderPassNdx++)
		{
			// First pass renders four quads of different color, which will be blurred in the following passes
			if (renderPassNdx == 0)
			{
				for (deUint32 y = 0; y < m_height; y++)
				for (deUint32 x = 0; x < m_width; x++)
				{
					if (x <= (m_width - 1) / 2 && y <= (m_height - 1) / 2)
						textureA.getAccess().setPixel(Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y);
					else if (x > (m_width - 1) / 2 && y <= (m_height - 1) / 2)
						textureA.getAccess().setPixel(Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
					else if (x <= (m_width - 1) / 2 && y > (m_height - 1) / 2)
						textureA.getAccess().setPixel(Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
					else
						textureA.getAccess().setPixel(Vec4(0.0f, 0.0f, 0.0f, 1.0f), x, y);
				}
			}
			// Blur previous pass
			else
			{
				for (deUint32 y = 0; y < m_height; y++)
				for (deUint32 x = 0; x < m_width; x++)
				{
					Vec4 blurColor (Vec4(0.0));

					for (deUint32 sampleNdx = 0; sampleNdx < (m_blurKernel + 1); sampleNdx++)
					{
						if (renderPassNdx % 2 == 0)
						{
							// Do a horizontal blur
							blurColor += 0.12f * textureB.getAccess().getPixel(deClamp32((deInt32)x - (m_blurKernel / 2) + sampleNdx, 0u, m_width - 1u), y);
						}
						else
						{
							// Do a vertical blur
							blurColor += 0.12f * textureA.getAccess().getPixel(x, deClamp32((deInt32)y - (m_blurKernel / 2) + sampleNdx, 0u, m_height - 1u));
						}
					}

					renderPassNdx % 2 == 0 ? textureA.getAccess().setPixel(blurColor, x, y) : textureB.getAccess().setPixel(blurColor, x, y);
				}
			}
		}

		reference = m_renderPasses.size() % 2 == 0 ? textureB : textureA;

		{
			// Allow error of 4 times the minimum presentable difference
			const Vec4 threshold (4.0f * 1.0f / ((UVec4(1u) << tcu::getTextureFormatMantissaBitDepth(format).cast<deUint32>()) - 1u).cast<float>());

			if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "", "", reference.getAccess(), access, threshold, tcu::COMPARE_LOG_ON_ERROR))
				m_resultCollector.fail("Compare failed.");
		}
	}

	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

struct SubpassTestConfig
{
		SubpassTestConfig	(VkFormat		format_,
							 UVec2			imageSize_,
							 RenderPass		renderPass_,
							 RenderPassType	renderPassType_)
		: format			(format_)
		, imageSize			(imageSize_)
		, renderPass		(renderPass_)
		, renderPassType	(renderPassType_)
	{
	}

	VkFormat			format;
	UVec2				imageSize;
	RenderPass			renderPass;
	RenderPassType		renderPassType;
};

class SubpassDependencyTestInstance : public TestInstance
{
public:
										SubpassDependencyTestInstance	(Context&								context,
																		 SubpassTestConfig						testConfig);

										~SubpassDependencyTestInstance	(void);

	vector<SharedPtrVkImage>			createAndAllocateImages			(const DeviceInterface&					vk,
																		 VkDevice								device,
																		 Allocator&								allocator,
																		 vector<de::SharedPtr<Allocation> >&	imageMemories,
																		 deUint32								universalQueueFamilyIndex,
																		 RenderPass								renderPassInfo,
																		 VkFormat								format,
																		 deUint32								width,
																		 deUint32								height);

	vector<SharedPtrVkPipelineLayout>	createRenderPipelineLayouts		(const DeviceInterface&					vkd,
																		 VkDevice								device,
																		 RenderPass								renderPassInfo,
																		 vector<SharedPtrVkDescriptorLayout>	descriptorSetLayouts);

	vector<SharedPtrVkPipeline>			createRenderPipelines			(const DeviceInterface&					vkd,
																		 VkDevice								device,
																		 RenderPass								renderPassInfo,
																		 VkRenderPass							renderPass,
																		 vector<SharedPtrVkPipelineLayout>&		pipelineLayouts,
																		 const BinaryCollection&				binaryCollection,
																		 VkFormat								format,
																		 deUint32								width,
																		 deUint32								height);

	Move<VkFramebuffer>					createFramebuffer				(const DeviceInterface&					vkd,
																		 VkDevice								device,
																		 RenderPass								renderPassInfo,
																		 VkRenderPass							renderPass,
																		 vector<SharedPtrVkImageView>&			dstImageViews,
																		 deUint32								width,
																		 deUint32								height);

	vector<SharedPtrVkDescriptorLayout>	createDescriptorSetLayouts		(const DeviceInterface&					vkd,
																		 VkDevice								device,
																		 RenderPass								renderPassInfo);

	vector<SharedPtrVkDescriptorSet>	createDescriptorSets			(const DeviceInterface&					vkd,
																		 VkDevice								device,
																		 VkFormat								format,
																		 vector<SharedPtrVkDescriptorPool>&		pools,
																		 vector<SharedPtrVkDescriptorLayout>&	layouts,
																		 vector<SharedPtrVkImageView>&			imageViews);

	tcu::TextureLevel					getRepresentableDepthChannel	(const ConstPixelBufferAccess&			access);

	bool								verifyDepth						(const ConstPixelBufferAccess&			reference,
																		 const ConstPixelBufferAccess&			result,
																		 const float							threshold);

	bool								verifyStencil					(const ConstPixelBufferAccess&			reference,
																		 const ConstPixelBufferAccess&			result);

	tcu::TestStatus						iterate							(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus						iterateInternal					(void);

private:
	const bool							m_extensionSupported;
	const RenderPass					m_renderPassInfo;
	const RenderPassType				m_renderPassType;

	const deUint32						m_width;
	const deUint32						m_height;
	const VkFormat						m_format;

	vector<de::SharedPtr<Allocation> >	m_imageMemories;
	vector<SharedPtrVkImage>			m_images;
	vector<SharedPtrVkImageView>		m_imageViews;

	const Unique<VkBuffer>				m_primaryBuffer;
	const Unique<VkBuffer>				m_secondaryBuffer;
	const de::UniquePtr<Allocation>		m_primaryBufferMemory;
	const de::UniquePtr<Allocation>		m_secondaryBufferMemory;

	const Unique<VkRenderPass>			m_renderPass;
	const Unique<VkFramebuffer>			m_framebuffer;

	vector<SharedPtrVkDescriptorLayout>	m_subpassDescriptorSetLayouts;
	vector<SharedPtrVkDescriptorPool>	m_subpassDescriptorPools;
	vector<SharedPtrVkDescriptorSet>	m_subpassDescriptorSets;

	vector<SharedPtrVkPipelineLayout>	m_renderPipelineLayouts;
	vector<SharedPtrVkPipeline>			m_renderPipelines;

	const Unique<VkCommandPool>			m_commandPool;
	tcu::ResultCollector				m_resultCollector;
};

SubpassDependencyTestInstance::SubpassDependencyTestInstance (Context& context, SubpassTestConfig testConfig)
	: TestInstance					(context)
	, m_extensionSupported			((testConfig.renderPassType == RENDERPASS_TYPE_RENDERPASS2) && context.requireDeviceExtension("VK_KHR_create_renderpass2"))
	, m_renderPassInfo				(testConfig.renderPass)
	, m_renderPassType				(testConfig.renderPassType)
	, m_width						(testConfig.imageSize.x())
	, m_height						(testConfig.imageSize.y())
	, m_format						(testConfig.format)
	, m_images						(createAndAllocateImages(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), m_imageMemories, context.getUniversalQueueFamilyIndex(), m_renderPassInfo, m_format, m_width, m_height))
	, m_imageViews					(createImageViews(context.getDeviceInterface(), context.getDevice(), m_images, m_format, isDepthStencilFormat(m_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT))
	, m_primaryBuffer				(createBuffer(context.getDeviceInterface(), context.getDevice(), m_format, m_width, m_height))
	, m_secondaryBuffer				(createBuffer(context.getDeviceInterface(), context.getDevice(), m_format, m_width, m_height))
	, m_primaryBufferMemory			(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_primaryBuffer))
	, m_secondaryBufferMemory		(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_secondaryBuffer))
	, m_renderPass					(createRenderPass(context.getDeviceInterface(), context.getDevice(), m_renderPassInfo, testConfig.renderPassType))
	, m_framebuffer					(createFramebuffer(context.getDeviceInterface(), context.getDevice(), m_renderPassInfo, *m_renderPass, m_imageViews, m_width, m_height))
	, m_subpassDescriptorSetLayouts	(createDescriptorSetLayouts(context.getDeviceInterface(), context.getDevice(), m_renderPassInfo))
	, m_subpassDescriptorPools		(createDescriptorPools(context.getDeviceInterface(), context.getDevice(), m_subpassDescriptorSetLayouts, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT))
	, m_subpassDescriptorSets		(createDescriptorSets(context.getDeviceInterface(), context.getDevice(), m_format, m_subpassDescriptorPools, m_subpassDescriptorSetLayouts, m_imageViews))
	, m_renderPipelineLayouts		(createRenderPipelineLayouts(context.getDeviceInterface(), context.getDevice(), m_renderPassInfo, m_subpassDescriptorSetLayouts))
	, m_renderPipelines				(createRenderPipelines(context.getDeviceInterface(), context.getDevice(), m_renderPassInfo, *m_renderPass, m_renderPipelineLayouts, context.getBinaryCollection(), m_format, m_width, m_height))
	, m_commandPool					(createCommandPool(context.getDeviceInterface(), context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
}

SubpassDependencyTestInstance::~SubpassDependencyTestInstance (void)
{
}

vector<SharedPtrVkImage> SubpassDependencyTestInstance::createAndAllocateImages (const DeviceInterface&					vk,
																				 VkDevice								device,
																				 Allocator&								allocator,
																				 vector<de::SharedPtr<Allocation> >&	imageMemories,
																				 deUint32								universalQueueFamilyIndex,
																				 RenderPass								renderPassInfo,
																				 VkFormat								format,
																				 deUint32								width,
																				 deUint32								height)
{
	// Verify format support
	{
		const VkFormatFeatureFlags	flags		= isDepthStencilFormat(m_format) ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
		const VkFormatProperties	properties	= vk::getPhysicalDeviceFormatProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), format);

		if ((properties.optimalTilingFeatures & flags) != flags)
			TCU_THROW(NotSupportedError, "Format not supported");
	}

	vector<SharedPtrVkImage> images;

	for (size_t imageNdx = 0; imageNdx < renderPassInfo.getAttachments().size(); imageNdx++)
	{
		const VkExtent3D		imageExtent		=
		{
			width,		// uint32_t	width
			height,		// uint32_t	height
			1u			// uint32_t	depth
		};

		VkImageUsageFlags		usage			= ((isDepthStencilFormat(format)
													? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
													: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
													| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
													| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,								// const void*				pNext
			0u,										// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			format,									// VkFormat					format
			imageExtent,							// VkExtent3D				extent
			1u,										// uint32_t					mipLevels
			1u,										// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
			usage,									// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			1u,										// uint32_t					queueFamilyIndexCount
			&universalQueueFamilyIndex,				// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		images.push_back(makeSharedPtr(vk::createImage(vk, device, &imageCreateInfo, DE_NULL)));
		imageMemories.push_back((de::SharedPtr<Allocation>)allocator.allocate(getImageMemoryRequirements(vk, device, **images[imageNdx]), MemoryRequirement::Any).release());
		VK_CHECK(vk.bindImageMemory(device, **images[imageNdx], imageMemories[imageNdx]->getMemory(), imageMemories[imageNdx]->getOffset()));
	}

	return images;
}

vector<SharedPtrVkPipelineLayout> SubpassDependencyTestInstance::createRenderPipelineLayouts (const DeviceInterface&				vkd,
																							  VkDevice								device,
																							  RenderPass							renderPassInfo,
																							  vector<SharedPtrVkDescriptorLayout>	descriptorSetLayouts)
{
	vector<SharedPtrVkPipelineLayout>	pipelineLayouts;
	vector<VkDescriptorSetLayout>		descriptorSetLayoutHandles;
	const size_t						descriptorSetLayoutCount	= descriptorSetLayouts.size();

	for (size_t descriptorSetLayoutNdx = 0; descriptorSetLayoutNdx < descriptorSetLayoutCount; descriptorSetLayoutNdx++)
		descriptorSetLayoutHandles.push_back(**descriptorSetLayouts.at(descriptorSetLayoutNdx));

	for (size_t subpassNdx = 0; subpassNdx < renderPassInfo.getSubpasses().size(); subpassNdx++)
	{
		const VkPipelineLayoutCreateInfo createInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
			DE_NULL,										// const void*					pNext
			(vk::VkPipelineLayoutCreateFlags)0,				// VkPipelineLayoutCreateFlags	flags
			(deUint32)descriptorSetLayoutCount,				// deUint32						setLayoutCount
			descriptorSetLayoutHandles.data(),				// const VkDescriptorSetLayout*	pSetLayouts
			0u,												// deUint32						pushConstantRangeCount
			DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
		};

		pipelineLayouts.push_back(makeSharedPtr(createPipelineLayout(vkd, device, &createInfo)));
	}

	return pipelineLayouts;
}

vector<SharedPtrVkPipeline> SubpassDependencyTestInstance::createRenderPipelines (const DeviceInterface&				vkd,
																				  VkDevice								device,
																				  RenderPass							renderPassInfo,
																				  VkRenderPass							renderPass,
																				  vector<SharedPtrVkPipelineLayout>&	pipelineLayouts,
																				  const BinaryCollection&				binaryCollection,
																				  VkFormat								format,
																				  deUint32								width,
																				  deUint32								height)
{
	vector<SharedPtrVkPipeline> pipelines;

	for (size_t subpassNdx = 0; subpassNdx < renderPassInfo.getSubpasses().size(); subpassNdx++)
	{
		const Unique<VkShaderModule>				vertexShaderModule			(createShaderModule(vkd, device, binaryCollection.get("subpass-vert-" + de::toString(subpassNdx)), 0u));
		const Unique<VkShaderModule>				fragmentShaderModule		(createShaderModule(vkd, device, binaryCollection.get("subpass-frag-" + de::toString(subpassNdx)), 0u));

		const VkVertexInputBindingDescription		vertexBinding0				=
		{
			0u,							// deUint32					binding;
			sizeof(Vec4),				// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	stepRate;
		};

		VkVertexInputAttributeDescription			attr0						=
		{
			0u,								// deUint32	location;
			0u,								// deUint32	binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format;
			0u								// deUint32	offsetInBytes;
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputState			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			(VkPipelineVertexInputStateCreateFlags)0u,					// VkPipelineVertexInputStateCreateFlags	flags
			1u,															// uint32_t									vertexBindingDescriptionCount
			&vertexBinding0,											// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
			1u,															// uint32_t									vertexAttributeDescriptionCount
			&attr0														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
		};

		const VkStencilOpState						stencilOpState				=
		{
			VK_STENCIL_OP_KEEP,		// stencilFailOp
			VK_STENCIL_OP_KEEP,		// stencilPassOp
			VK_STENCIL_OP_KEEP,		// stencilDepthFailOp
			VK_COMPARE_OP_ALWAYS,	// stencilCompareOp
			0x0u,					// stencilCompareMask
			0x0u,					// stencilWriteMask
			0u						// stencilReference
		};

		const VkPipelineDepthStencilStateCreateInfo	depthStencilStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags
			VK_TRUE,													// VkBool32									depthTestEnable
			VK_TRUE,													// VkBool32									depthWriteEnable
			VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp								depthCompareOp
			VK_FALSE,													// VkBool32									depthBoundsTestEnable
			VK_TRUE,													// VkBool32									stencilTestEnable
			stencilOpState,												// VkStencilOpState							front
			stencilOpState,												// VkStencilOpState							back
			0.0f,														// float									minDepthBounds
			1.0f,														// float									maxDepthBounds
		};

		const std::vector<VkViewport>				viewports					(1, makeViewport(tcu::UVec2(width, height)));
		const std::vector<VkRect2D>					scissors					(1, makeRect2D(tcu::UVec2(width, height)));
		const VkPipelineLayout						layout						(**pipelineLayouts[subpassNdx]);
		const VkPipelineDepthStencilStateCreateInfo	depthStencilCreateInfo		(isDepthStencilFormat(format)
																					? depthStencilStateCreateInfo
																					: VkPipelineDepthStencilStateCreateInfo());

		pipelines.push_back(makeSharedPtr(makeGraphicsPipeline(vkd,									// const DeviceInterface&							vk
															   device,								// const VkDevice									device
															   layout,								// const VkPipelineLayout							pipelineLayout
															   *vertexShaderModule,					// const VkShaderModule								vertexShaderModule
															   DE_NULL,								// const VkShaderModule								tessellationControlShaderModule
															   DE_NULL,								// const VkShaderModule								tessellationEvalShaderModule
															   DE_NULL,								// const VkShaderModule								geometryShaderModule
															   *fragmentShaderModule,				// const VkShaderModule								fragmentShaderModule
															   renderPass,							// const VkRenderPass								renderPass
															   viewports,							// const std::vector<VkViewport>&					viewports
															   scissors,							// const std::vector<VkRect2D>&						scissors
															   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology						topology
															   (deUint32)subpassNdx,				// const deUint32									subpass
															   0u,									// const deUint32									patchControlPoints
															   &vertexInputState,					// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
															   DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
															   DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
															   &depthStencilCreateInfo,				// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
															   DE_NULL)));							// const VkPipelineDynamicStateCreateInfo*			pDynamicState
	}

	return pipelines;
}

Move<VkFramebuffer> SubpassDependencyTestInstance::createFramebuffer (const DeviceInterface&		vkd,
																	  VkDevice						device,
																	  RenderPass					renderPassInfo,
																	  VkRenderPass					renderPass,
																	  vector<SharedPtrVkImageView>&	dstImageViews,
																	  deUint32						width,
																	  deUint32						height)
{
	const size_t		attachmentCount		(renderPassInfo.getAttachments().size());
	vector<VkImageView>	attachmentHandles;

	for (deUint32 attachmentNdx = 0; attachmentNdx < attachmentCount; attachmentNdx++)
		attachmentHandles.push_back(**dstImageViews.at(attachmentNdx));

	const VkFramebufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkFramebufferCreateFlags	flags
		renderPass,									// VkRenderPass				renderPass
		(deUint32)attachmentCount,					// uint32_t					attachmentCount
		attachmentHandles.data(),					// const VkImageView*		pAttachments
		width,										// uint32_t					width
		height,										// uint32_t					height
		1u											// uint32_t					layers
	};

	return vk::createFramebuffer(vkd, device, &createInfo);
}

vector<SharedPtrVkDescriptorLayout> SubpassDependencyTestInstance::createDescriptorSetLayouts (const DeviceInterface&	vkd,
																							   VkDevice					device,
																							   RenderPass				renderPassInfo)
{
	vector<SharedPtrVkDescriptorLayout> layouts;

	size_t attachmentCount = renderPassInfo.getAttachments().size();

	for (size_t layoutNdx = 0; layoutNdx < attachmentCount - 1; layoutNdx++)
	{
		const VkDescriptorSetLayoutBinding		bindings	=
		{
				0u,										// uint32_t				binding
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	// VkDescriptorType		descriptorType
				1u,										// uint32_t				descriptorCount
				VK_SHADER_STAGE_FRAGMENT_BIT,			// VkShaderStageFlags	stageFlags
				DE_NULL									// const VkSampler*		pImmutableSamplers
		};

		const VkDescriptorSetLayoutCreateInfo	createInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkDescriptorSetLayoutCreateFlags		flags
			1u,														// uint32_t								bindingCount
			&bindings												// const VkDescriptorSetLayoutBinding*	pBindings
		};

		layouts.push_back(makeSharedPtr(createDescriptorSetLayout(vkd, device, &createInfo)));
	}

	return layouts;
}

vector<SharedPtrVkDescriptorSet> SubpassDependencyTestInstance::createDescriptorSets (const DeviceInterface&				vkd,
																					  VkDevice								device,
																					  VkFormat								format,
																					  vector<SharedPtrVkDescriptorPool>&	pools,
																					  vector<SharedPtrVkDescriptorLayout>&	layouts,
																					  vector<SharedPtrVkImageView>&			imageViews)
{
	vector<SharedPtrVkDescriptorSet> descriptorSets;

	for (size_t setNdx = 0; setNdx < layouts.size(); setNdx++)
	{
		const VkDescriptorSetAllocateInfo allocateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType
			DE_NULL,										// const void*					pNext
			**pools[setNdx],								// VkDescriptorPool				descriptorPool
			1u,												// uint32_t						descriptorSetCount
			&**layouts[setNdx]								// const VkDescriptorSetLayout*	pSetLayouts
		};

		descriptorSets.push_back(makeSharedPtr(allocateDescriptorSet(vkd, device, &allocateInfo)));

		{
			VkImageLayout imageLayout				= isDepthStencilFormat(format)
														? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
														: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			const VkDescriptorImageInfo	imageInfo	=
			{
				DE_NULL,				// VkSampler		sampler
				**imageViews[setNdx],	// VkImageView		imageView
				imageLayout				// VkImageLayout	imageLayout
			};

			const VkWriteDescriptorSet	write		=
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType
				DE_NULL,								// const void*						pNext
				**descriptorSets[setNdx],				// VkDescriptorSet					dstSet
				0u,										// uint32_t							dstBinding
				0u,										// uint32_t							dstArrayElement
				1u,										// uint32_t							descriptorCount
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	// VkDescriptorType					descriptorType
				&imageInfo,								// const VkDescriptorImageInfo*		pImageInfo
				DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo
				DE_NULL									// const VkBufferView*				pTexelBufferView
			};

			vkd.updateDescriptorSets(device, 1u, &write, 0u, DE_NULL);
		}
	}

	return descriptorSets;
}

tcu::TextureLevel SubpassDependencyTestInstance::getRepresentableDepthChannel (const ConstPixelBufferAccess& access)
{
	tcu::TextureLevel depthChannel (mapVkFormat(VK_FORMAT_R8G8B8_UNORM), access.getWidth(), access.getHeight());

	for (int y = 0; y < access.getHeight(); y++)
	for (int x = 0; x < access.getWidth(); x++)
		depthChannel.getAccess().setPixel(tcu::Vec4(access.getPixDepth(x, y)), x, y);

	return depthChannel;
}

bool SubpassDependencyTestInstance::verifyDepth (const ConstPixelBufferAccess&	reference,
												 const ConstPixelBufferAccess&	result,
												 const float					threshold)
{
	tcu::TestLog& log (m_context.getTestContext().getLog());

	return tcu::floatThresholdCompare(log,										// log
									  "Depth channel",							// imageSetName
									  "Depth compare",							// imageSetDesc
									  getRepresentableDepthChannel(reference),	// reference
									  getRepresentableDepthChannel(result),		// result
									  Vec4(threshold),							// threshold
									  tcu::COMPARE_LOG_RESULT);					// logMode
}

bool SubpassDependencyTestInstance::verifyStencil (const ConstPixelBufferAccess&	reference,
												   const ConstPixelBufferAccess&	result)
{
	tcu::TextureLevel	stencilErrorImage	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), result.getWidth(), result.getHeight());
	tcu::TestLog&		log					(m_context.getTestContext().getLog());
	bool				stencilOk			(DE_TRUE);

	for (int y = 0; y < result.getHeight(); y++)
	for (int x = 0; x < result.getWidth(); x++)
	{
		if (result.getPixStencil(x, y) != reference.getPixStencil(x, y))
		{
			stencilErrorImage.getAccess().setPixel(Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y);
			stencilOk = DE_FALSE;
		}
		else
			stencilErrorImage.getAccess().setPixel(Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
	}

	log << tcu::TestLog::ImageSet("Stencil compare", "Stencil compare")
		<< tcu::TestLog::Image("Result stencil channel", "Result stencil channel", result)
		<< tcu::TestLog::Image("Reference stencil channel", "Reference stencil channel", reference);

	if (!stencilOk)
		log << tcu::TestLog::Image("Stencil error mask", "Stencil error mask", stencilErrorImage);

	log << tcu::TestLog::EndImageSet;

	return stencilOk;
}

tcu::TestStatus SubpassDependencyTestInstance::iterate (void)
{
	switch (m_renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERPASS_TYPE_RENDERPASS2:
			return iterateInternal<RenderpassSubpass2>();
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

static float truncateCoord(int dim, int subPixelBits, float f)
{
	const float scale = (float)(dim << subPixelBits);
	const float coord = deFloatFloor(f * scale) / scale;

	return coord;
}

template<typename RenderpassSubpass>
tcu::TestStatus SubpassDependencyTestInstance::iterateInternal (void)
{
	de::Random											rand					(5);
	const DeviceInterface&								vkd						(m_context.getDeviceInterface());
	const Unique<VkCommandBuffer>						commandBuffer			(allocateCommandBuffer(vkd, m_context.getDevice(), *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo		(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo			(DE_NULL);
	const size_t										attachmentCount			(m_renderPassInfo.getAttachments().size());
	const size_t										subpassCount			(m_renderPassInfo.getSubpasses().size());
	vector<VkClearValue>								clearValues;
	vector<Vec4>										vertexData;

	beginCommandBuffer(vkd, *commandBuffer);

	// Begin render pass
	{
		VkRect2D					renderArea			=
		{
			{ 0u, 0u },				// VkOffset2D	offset
			{ m_width, m_height }	// VkExtent2D	extent
		};

		for (size_t attachmentNdx = 0; attachmentNdx < attachmentCount; attachmentNdx++)
			clearValues.push_back(isDepthStencilFormat(m_format) ? makeClearValueDepthStencil(1.0f, 255u) : makeClearValueColor(Vec4(1.0f, 0.0f, 0.0f, 1.0f)));

		const VkRenderPassBeginInfo	beginInfo			=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType
			DE_NULL,									// const void*			pNext
			*m_renderPass,								// VkRenderPass			renderPass
			*m_framebuffer,								// VkFramebuffer		framebuffer
			renderArea,									// VkRect2D				renderArea
			(deUint32)attachmentCount,					// uint32_t				clearValueCount
			clearValues.data()							// const VkClearValue*	pClearValues
		};

		RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
	}

	// Generate vertices for 128 triangles with pseudorandom positions and depths values
	for (int primitiveNdx = 0; primitiveNdx < 128; primitiveNdx++)
	{
		float primitiveDepth = rand.getFloat();

		for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
		{
			const int subPixelBits = m_context.getDeviceProperties().limits.subPixelPrecisionBits;

			float x = 2.0f * truncateCoord(m_width,  subPixelBits, rand.getFloat()) - 1.0f;
			float y = 2.0f * truncateCoord(m_height, subPixelBits, rand.getFloat()) - 1.0f;

			vertexData.push_back(Vec4(x, y, primitiveDepth, 1.0f));
		}
	}

	const size_t										singleVertexDataSize	= sizeof(Vec4);
	const size_t										vertexCount				= vertexData.size();
	const size_t										vertexDataSize			= vertexCount * singleVertexDataSize;
	const deUint32										queueFamilyIndices		= m_context.getUniversalQueueFamilyIndex();

	const VkBufferCreateInfo							vertexBufferParams		=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,								//	const void*			pNext;
		0u,										//	VkBufferCreateFlags	flags;
		(VkDeviceSize)vertexDataSize,			//	VkDeviceSize		size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		//	VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode		sharingMode;
		1u,										//	deUint32			queueFamilyCount;
		&queueFamilyIndices,					//	const deUint32*		pQueueFamilyIndices;
	};

	const Unique<VkBuffer>								vertexBuffer			(createBuffer(vkd, m_context.getDevice(), &vertexBufferParams));
	const de::UniquePtr<Allocation>						vertexBufferMemory		(m_context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vkd, m_context.getDevice(), *vertexBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vkd.bindBufferMemory(m_context.getDevice(), *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

	const VkDeviceSize bindingOffset = 0;
	vkd.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer.get(), &bindingOffset);

	for (size_t subpassNdx = 0; subpassNdx < subpassCount; subpassNdx++)
	{
		if (subpassNdx > 0)
		{
			RenderpassSubpass::cmdNextSubpass(vkd, *commandBuffer, &subpassBeginInfo, &subpassEndInfo);
			vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **m_renderPipelineLayouts[subpassNdx], 0, 1, &**m_subpassDescriptorSets[subpassNdx - 1], 0, DE_NULL);
		}

		vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **m_renderPipelines[subpassNdx]);

		if (subpassNdx == 0)
		{
			// Upload vertex data
			{
				void* vertexBufPtr = vertexBufferMemory->getHostPtr();
				deMemcpy(vertexBufPtr, vertexData.data(), vertexDataSize);
				flushAlloc(vkd, m_context.getDevice(), *vertexBufferMemory);
			}

			vkd.cmdDraw(*commandBuffer, (deUint32)vertexData.size(), 1u, 0u, 0u);
		}
		else
			vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
	}

	RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	// Memory barrier between rendering and copy
	{
		const VkImageAspectFlags	imageAspectFlags		= isDepthStencilFormat(m_format)
																? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		const VkAccessFlags			srcAccessMask			= isDepthStencilFormat(m_format)
																? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		const VkImageLayout			oldLayout				= isDepthStencilFormat(m_format)
																? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		const VkPipelineStageFlags	srcStageMask			= isDepthStencilFormat(m_format)
																? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkImageSubresourceRange		imageSubresourceRange	=
		{
			imageAspectFlags,	// VkImageAspectFlags	aspectMask
			0u,					// uint32_t				baseMipLevel
			1u,					// uint32_t				levelCount
			0u,					// uint32_t				baseArrayLayer
			1u					// uint32_t				layerCount
		};

		const VkImageMemoryBarrier	barrier					=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType
			DE_NULL,								// const void*				pNext
			srcAccessMask,							// VkAccessFlags			srcAccessMask
			VK_ACCESS_TRANSFER_READ_BIT,			// VkAccessFlags			dstAccessMask
			oldLayout,								// VkImageLayout			oldLayout
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,	// VkImageLayout			newLayout
			VK_QUEUE_FAMILY_IGNORED,				// uint32_t					srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,				// uint32_t					dstQueueFamilyIndex
			**m_images[attachmentCount - 1],		// VkImage					image
			imageSubresourceRange					// VkImageSubresourceRange	subresourceRange
		};

		vkd.cmdPipelineBarrier(*commandBuffer, srcStageMask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
	}

	// Copy image memory to buffer
	{
		if (isDepthStencilFormat(m_format))
		{
			// Copy depth
			const VkImageSubresourceLayers subresourceLayersDepth	=
			{
				VK_IMAGE_ASPECT_DEPTH_BIT,	// VkImageAspectFlags	aspectMask
				0u,							// deUint32				mipLevel
				0u,							// deUint32				baseArrayLayer
				1u							// deUint32				layerCount
			};

			const VkBufferImageCopy			regionDepth				=
			{
				0u,							// VkDeviceSize				bufferOffset
				0u,							// uint32_t					bufferRowLength
				0u,							// uint32_t					bufferImageHeight
				subresourceLayersDepth,		// VkImageSubresourceLayers	imageSubresource
				{ 0u, 0u, 0u },				// VkOffset3D				imageOffset
				{ m_width, m_height, 1u }	// VkExtent3D				imageExtent
			};

			vkd.cmdCopyImageToBuffer(*commandBuffer, **m_images[attachmentCount - 1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *m_primaryBuffer, 1u, &regionDepth);

			// Copy stencil
			const VkImageSubresourceLayers subresourceLayersStencil	=
			{
				VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask
				0u,								// deUint32				mipLevel
				0u,								// deUint32				baseArrayLayer
				1u								// deUint32				layerCount
			};

			const VkBufferImageCopy			regionStencil			=
			{
				0u,							// VkDeviceSize				bufferOffset
				0u,							// uint32_t					bufferRowLength
				0u,							// uint32_t					bufferImageHeight
				subresourceLayersStencil,	// VkImageSubresourceLayers	imageSubresource
				{ 0u, 0u, 0u },				// VkOffset3D				imageOffset
				{ m_width, m_height, 1u }	// VkExtent3D				imageExtent
			};

			vkd.cmdCopyImageToBuffer(*commandBuffer, **m_images[attachmentCount - 1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *m_secondaryBuffer, 1u, &regionStencil);
		}
		else
		{
			// Copy color
			const VkImageSubresourceLayers imageSubresourceLayers	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
				0u,							// deUint32				mipLevel
				0u,							// deUint32				baseArrayLayer
				1u							// deUint32				layerCount
			};

			const VkBufferImageCopy			region					=
			{
				0u,							// VkDeviceSize				bufferOffset
				0u,							// uint32_t					bufferRowLength
				0u,							// uint32_t					bufferImageHeight
				imageSubresourceLayers,		// VkImageSubresourceLayers	imageSubresource
				{ 0u, 0u, 0u },				// VkOffset3D				imageOffset
				{ m_width, m_height, 1u }	// VkExtent3D				imageExtent
			};

			vkd.cmdCopyImageToBuffer(*commandBuffer, **m_images[attachmentCount - 1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *m_primaryBuffer, 1u, &region);
		}
	}

	// Memory barrier between copy and host access
	{
		const VkBufferMemoryBarrier barrier		=
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType
			DE_NULL,									// const void*		pNext
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask
			VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t			srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t			dstQueueFamilyIndex
			*m_primaryBuffer,							// VkBuffer			buffer
			0u,											// VkDeviceSize		offset
			VK_WHOLE_SIZE								// VkDeviceSize		size
		};

		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &barrier, 0u, DE_NULL);

		if (isDepthStencilFormat(m_format))
		{
			const VkBufferMemoryBarrier stencilBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType
				DE_NULL,									// const void*		pNext
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask
				VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask
				VK_QUEUE_FAMILY_IGNORED,					// uint32_t			srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,					// uint32_t			dstQueueFamilyIndex
				*m_secondaryBuffer,							// VkBuffer			buffer
				0u,											// VkDeviceSize		offset
				VK_WHOLE_SIZE								// VkDeviceSize		size
			};

			vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &stencilBarrier, 0u, DE_NULL);
		}
	}

	endCommandBuffer(vkd, *commandBuffer);
	submitCommandsAndWait(vkd, m_context.getDevice(), m_context.getUniversalQueue(), *commandBuffer);
	invalidateMappedMemoryRange(vkd, m_context.getDevice(), m_primaryBufferMemory->getMemory(), m_primaryBufferMemory->getOffset(), VK_WHOLE_SIZE);
	invalidateMappedMemoryRange(vkd, m_context.getDevice(), m_secondaryBufferMemory->getMemory(), m_secondaryBufferMemory->getOffset(), VK_WHOLE_SIZE);

	// Verify result
	{
		const tcu::TextureFormat format (mapVkFormat(m_format));

		if (isDepthStencilFormat(m_format))
		{
			const void* const					ptrDepth				(m_primaryBufferMemory->getHostPtr());
			const void* const					ptrStencil				(m_secondaryBufferMemory->getHostPtr());
			tcu::TextureLevel					reference				(format, m_width, m_height);
			tcu::TextureLevel					colorBuffer				(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), m_width, m_height);
			const tcu::ConstPixelBufferAccess	resultDepthAccess		(getDepthCopyFormat(m_format), m_width, m_height, 1, ptrDepth);
			const tcu::ConstPixelBufferAccess	resultStencilAccess		(getStencilCopyFormat(m_format), m_width, m_height, 1, ptrStencil);
			const PixelBufferAccess				referenceDepthAccess	(tcu::getEffectiveDepthStencilAccess(reference.getAccess(), tcu::Sampler::MODE_DEPTH));
			const PixelBufferAccess				referenceStencilAccess	(tcu::getEffectiveDepthStencilAccess(reference.getAccess(), tcu::Sampler::MODE_STENCIL));

			tcu::clearDepth(referenceDepthAccess, 1.0f);
			tcu::clearStencil(referenceStencilAccess, 255);

			// Setup and run reference renderer
			{
				const DepthVertShader					vertShader;
				const DepthFragShader					fragShader;
				const rr::Renderer						renderer;
				const rr::Program						program				(&vertShader, &fragShader);
				const rr::MultisamplePixelBufferAccess	depthBuffer			(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(referenceDepthAccess));
				const rr::MultisamplePixelBufferAccess	colorBufferAccess	(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(colorBuffer.getAccess()));
				const rr::RenderTarget					renderTarget		(rr::MultisamplePixelBufferAccess(colorBufferAccess), depthBuffer, rr::MultisamplePixelBufferAccess());
				const rr::PrimitiveType					primitiveType		(rr::PRIMITIVETYPE_TRIANGLES);
				const rr::PrimitiveList					primitiveList		(rr::PrimitiveList(primitiveType, (deUint32)vertexData.size(), 0));
				rr::RenderState							renderState			((rr::ViewportState(depthBuffer)));

				const rr::VertexAttrib vertices			= rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &vertexData[0]);

				renderState.fragOps.depthTestEnabled	= DE_TRUE;
				renderState.fragOps.depthFunc			= rr::TESTFUNC_LEQUAL;

				renderer.draw(rr::DrawCommand(renderState,
											  renderTarget,
											  program,
											  1u,
											  &vertices,
											  primitiveList));
			}

			for (size_t subpassNdx = 0; subpassNdx < subpassCount - 1; subpassNdx++)
			{
				for (int y = 0; y < reference.getHeight(); y++)
				for (int x = 0; x < reference.getWidth(); x++)
					reference.getAccess().setPixDepth(reference.getAccess().getPixDepth(x, y) - 0.02f, x, y);
			}

			// Threshold size of subpass count multiplied by the minimum representable difference is allowed for depth compare
			const float							depthThreshold			((float)subpassCount * (1.0f / ((UVec4(1u) << tcu::getTextureFormatMantissaBitDepth(
																			resultDepthAccess.getFormat()).cast<deUint32>()) - 1u).cast<float>().x()));

			if (!verifyDepth(reference.getAccess(), resultDepthAccess, depthThreshold))
				m_resultCollector.fail("Depth compare failed.");

			if (!verifyStencil(referenceStencilAccess, resultStencilAccess))
				m_resultCollector.fail("Stencil compare failed.");
		}
		else
			DE_FATAL("Not implemented");
	}

	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

// Shader programs for testing dependencies between render pass instances
struct ExternalPrograms
{
	void init (vk::SourceCollections& dst, ExternalTestConfig testConfig) const
	{
		for (size_t renderPassNdx = 0; renderPassNdx < testConfig.renderPasses.size(); renderPassNdx++)
		{
			dst.glslSources.add("quad-vert-" + de::toString(renderPassNdx)) << glu::VertexSource(
				"#version 450\n"
				"layout(location = 0) out highp vec2 vtxTexCoords;\n"
				"highp float;\n"
				"void main (void)\n"
				"{\n"
				"    vec4 position;"
				"    position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
				"                    ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
				"    gl_Position = position;\n"
				"	vtxTexCoords = position.xy / 2.0 + vec2(0.5);"
				"}\n");

			// First pass renders four quads of different color
			if (renderPassNdx == 0)
			{
				dst.glslSources.add("quad-frag-" + de::toString(renderPassNdx)) << glu::FragmentSource(
					"#version 450\n"
					"layout(location = 0) in highp vec2 vtxTexCoords;\n"
					"layout(location = 0) out highp vec4 o_color;\n"
					"void main (void)\n"
					"{\n"
					"    if (gl_FragCoord.x <= " + de::toString(testConfig.imageSize.x() / 2) + " && gl_FragCoord.y <= " + de::toString(testConfig.imageSize.y() / 2) + ")\n"
					"        o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
					"    else if (gl_FragCoord.x > " + de::toString(testConfig.imageSize.x() / 2) + " && gl_FragCoord.y <= " + de::toString(testConfig.imageSize.y() / 2) + ")\n"
					"        o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
					"    else if (gl_FragCoord.x <= " + de::toString(testConfig.imageSize.x() / 2) + " && gl_FragCoord.y > " + de::toString(testConfig.imageSize.y() / 2) + ")\n"
					"        o_color = vec4(0.0, 0.0, 1.0, 1.0);\n"
					"    else\n"
					"        o_color = vec4(0.0, 0.0, 0.0, 1.0);\n"
					""
					"}\n");
			}
			else
			{
				if (renderPassNdx % 2 == 0)
				{
					// Blur previous pass horizontally
					dst.glslSources.add("quad-frag-" + de::toString(renderPassNdx)) << glu::FragmentSource(
						"#version 450\n"
						"layout(binding = 0) uniform sampler2D previousPass;\n"
						"layout(location = 0) in highp vec2 vtxTexCoords;\n"
						"layout(location = 0) out highp vec4 o_color;\n"
						"void main (void)\n"
						"{\n"
						"    vec2 step = vec2(1.0 / " + de::toString(testConfig.imageSize.x()) + ", 1.0 / " + de::toString(testConfig.imageSize.y()) + ");\n"
						"    vec2 minCoord = vec2(0.0, 0.0);\n"
						"    vec2 maxCoord = vec2(1.0, 1.0);\n"
						"    vec4 blurColor = vec4(0.0);\n"
						"    for(int sampleNdx = 0; sampleNdx < " + de::toString(testConfig.blurKernel + 1) + "; sampleNdx++)\n"
						"    {\n"
						"        vec2 sampleCoord = vec2((vtxTexCoords.x - " + de::toString(testConfig.blurKernel / 2) + " * step.x) + step.x * sampleNdx, vtxTexCoords.y);\n"
						"        blurColor += 0.12 * texture(previousPass, clamp(sampleCoord, minCoord, maxCoord));\n"
						"    }\n"
						"    o_color = blurColor;\n"
						"}\n");
				}
				else
				{
					// Blur previous pass vertically
					dst.glslSources.add("quad-frag-" + de::toString(renderPassNdx)) << glu::FragmentSource(
						"#version 450\n"
						"layout(binding = 0) uniform highp sampler2D previousPass;\n"
						"layout(location = 0) in highp vec2 vtxTexCoords;\n"
						"layout(location = 0) out highp vec4 o_color;\n"
						"void main (void)\n"
						"{\n"
						"    vec2 step = vec2(1.0 / " + de::toString(testConfig.imageSize.x()) + ", 1.0 / " + de::toString(testConfig.imageSize.y()) + ");\n"
						"    vec2 minCoord = vec2(0.0, 0.0);\n"
						"    vec2 maxCoord = vec2(1.0, 1.0);\n"
						"    vec4 blurColor = vec4(0.0);\n"
						"    for(int sampleNdx = 0; sampleNdx < " + de::toString(testConfig.blurKernel + 1) + "; sampleNdx++)\n"
						"    {\n"
						"        vec2 sampleCoord = vec2(vtxTexCoords.x, (vtxTexCoords.y - " + de::toString(testConfig.blurKernel / 2) + " * step.y) + step.y * sampleNdx);\n"
						"        blurColor += 0.12 * texture(previousPass, clamp(sampleCoord, minCoord, maxCoord));\n"
						"    }\n"
						"    o_color = blurColor;\n"
						"}\n");
				}
			}
		}
	}
};

// Shader programs for testing dependencies between subpasses
struct SubpassPrograms
{
	void init (vk::SourceCollections& dst, SubpassTestConfig testConfig) const
	{
		size_t subpassCount = testConfig.renderPass.getSubpasses().size();

		for (size_t subpassNdx = 0; subpassNdx < subpassCount; subpassNdx++)
		{
			if (subpassNdx == 0)
			{
				dst.glslSources.add("subpass-vert-" + de::toString(subpassNdx)) << glu::VertexSource(
				"#version 450\n"
				"highp float;\n"
				"layout(location = 0) in highp vec4 position;\n"
				"void main (void)\n"
				"{\n"
				"    gl_Position = position;\n"
				"}\n");
			}
			else
			{
				dst.glslSources.add("subpass-vert-" + de::toString(subpassNdx)) << glu::VertexSource(
					"#version 450\n"
					"highp float;\n"
					"void main (void)\n"
					"{\n"
					"    vec4 position;"
					"    position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
					"                    ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
					"    gl_Position = position;\n"
					"}\n");
			}

			if (isDepthStencilFormat(testConfig.format))
			{
				if (subpassNdx == 0)
				{
					// Empty fragment shader: Fragment depth unmodified.
					dst.glslSources.add("subpass-frag-" + de::toString(subpassNdx)) << glu::FragmentSource(
						"#version 450\n"
						"void main (void)\n"
						"{\n"
						"}\n");
				}
				else
				{
					// Use fragment depth from previous depth rendering result.
					dst.glslSources.add("subpass-frag-" + de::toString(subpassNdx)) << glu::FragmentSource(
						"#version 450\n"
						"layout (input_attachment_index = 0, binding = 0) uniform subpassInput depthStencil;\n"
						"void main (void)\n"
						"{\n"
						"    float inputDepth = subpassLoad(depthStencil).x;\n"
						"    gl_FragDepth = inputDepth - 0.02;\n"
						"}\n");
				}
			}
			else
				DE_FATAL("Unimplemented");
		}
	}
};

std::string formatToName (VkFormat format)
{
	const std::string	formatStr	= de::toString(format);
	const std::string	prefix		= "VK_FORMAT_";

	DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

	return de::toLower(formatStr.substr(prefix.length()));
}

void initTests (tcu::TestCaseGroup* group, const RenderPassType renderPassType)
{
	tcu::TestContext& testCtx(group->getTestContext());

	// Test external subpass dependencies
	{
		const deUint32	renderPassCounts[]	= { 2u, 3u, 5u};

		const UVec2		renderSizes[]		=
		{
			UVec2(64, 64),
			UVec2(128, 128),
			UVec2(512, 512)
		};

		de::MovePtr<tcu::TestCaseGroup>	externalGroup	(new tcu::TestCaseGroup(testCtx, "external_subpass", "external_subpass"));

		for (size_t renderSizeNdx = 0; renderSizeNdx < DE_LENGTH_OF_ARRAY(renderSizes); renderSizeNdx++)
		{
			string groupName	("render_size_" + de::toString(renderSizes[renderSizeNdx].x()) + "_" + de::toString(renderSizes[renderSizeNdx].y()));
			de::MovePtr<tcu::TestCaseGroup>	renderSizeGroup	(new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupName.c_str()));

			for (size_t renderPassCountNdx = 0; renderPassCountNdx < DE_LENGTH_OF_ARRAY(renderPassCounts); renderPassCountNdx++)
			{
				vector<RenderPass>	renderPasses;

				for (size_t renderPassNdx = 0; renderPassNdx < renderPassCounts[renderPassCountNdx]; renderPassNdx++)
				{
					vector<Attachment>			attachments;
					vector<AttachmentReference>	colorAttachmentReferences;

					const VkFormat				format				(VK_FORMAT_R8G8B8A8_UNORM);
					const VkSampleCountFlagBits	sampleCount			(VK_SAMPLE_COUNT_1_BIT);
					const VkAttachmentLoadOp	loadOp				(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
					const VkAttachmentStoreOp	storeOp				(VK_ATTACHMENT_STORE_OP_STORE);
					const VkAttachmentLoadOp	stencilLoadOp		(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
					const VkAttachmentStoreOp	stencilStoreOp		(VK_ATTACHMENT_STORE_OP_DONT_CARE);
					const VkImageLayout			initialLayout		(VK_IMAGE_LAYOUT_UNDEFINED);
					const VkImageLayout			finalLayout			(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
					const VkImageLayout			subpassLayout		(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

					attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalLayout));
					colorAttachmentReferences.push_back(AttachmentReference((deUint32)0, subpassLayout));

					const VkImageLayout			depthStencilLayout	(VK_IMAGE_LAYOUT_GENERAL);
					const vector<Subpass>		subpasses			(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, vector<AttachmentReference>(), colorAttachmentReferences, vector<AttachmentReference>(),
																		AttachmentReference(VK_ATTACHMENT_UNUSED, depthStencilLayout), vector<deUint32>()));
					vector<SubpassDependency>	deps;

					deps.push_back(SubpassDependency(VK_SUBPASS_EXTERNAL,										// deUint32				srcPass
													 0,															// deUint32				dstPass
													 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,				// VkPipelineStageFlags	srcStageMask
													 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,						// VkPipelineStageFlags	dstStageMask
													 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,						// VkAccessFlags		srcAccessMask
													 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,	// VkAccessFlags		dstAccessMask
													 0));														// VkDependencyFlags	flags

					deps.push_back(SubpassDependency(0,															// deUint32				srcPass
													 VK_SUBPASS_EXTERNAL,										// deUint32				dstPass
													 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,						// VkPipelineStageFlags	srcStageMask
													 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,				// VkPipelineStageFlags	dstStageMask
													 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,	// VkAccessFlags		srcAccessMask
													 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,						// VkAccessFlags		dstAccessMask
													 0));														// VkDependencyFlags	flags

					RenderPass					renderPass			(attachments, subpasses, deps);

					renderPasses.push_back(renderPass);
				}

				const deUint32				blurKernel	(12u);
				const ExternalTestConfig	testConfig	(VK_FORMAT_R8G8B8A8_UNORM, renderSizes[renderSizeNdx], renderPasses, renderPassType, blurKernel);
				const string				testName	("render_passes_" + de::toString(renderPassCounts[renderPassCountNdx]));

				renderSizeGroup->addChild(new InstanceFactory1<ExternalDependencyTestInstance, ExternalTestConfig, ExternalPrograms>(testCtx, tcu::NODETYPE_SELF_VALIDATE, testName.c_str(), testName.c_str(), testConfig));
			}

			externalGroup->addChild(renderSizeGroup.release());
		}

		group->addChild(externalGroup.release());
	}

	// Test implicit subpass dependencies
	{
		const deUint32					renderPassCounts[]		= { 2u, 3u, 5u };

		de::MovePtr<tcu::TestCaseGroup>	implicitGroup			(new tcu::TestCaseGroup(testCtx, "implicit_dependencies", "implicit_dependencies"));

		for (size_t renderPassCountNdx = 0; renderPassCountNdx < DE_LENGTH_OF_ARRAY(renderPassCounts); renderPassCountNdx++)
		{
			vector<RenderPass> renderPasses;

			for (size_t renderPassNdx = 0; renderPassNdx < renderPassCounts[renderPassCountNdx]; renderPassNdx++)
			{
				vector<Attachment>			attachments;
				vector<AttachmentReference>	colorAttachmentReferences;

				const VkFormat				format					(VK_FORMAT_R8G8B8A8_UNORM);
				const VkSampleCountFlagBits	sampleCount				(VK_SAMPLE_COUNT_1_BIT);
				const VkAttachmentLoadOp	loadOp					(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
				const VkAttachmentStoreOp	storeOp					(VK_ATTACHMENT_STORE_OP_STORE);
				const VkAttachmentLoadOp	stencilLoadOp			(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
				const VkAttachmentStoreOp	stencilStoreOp			(VK_ATTACHMENT_STORE_OP_DONT_CARE);
				const VkImageLayout			initialLayout			(VK_IMAGE_LAYOUT_UNDEFINED);
				const VkImageLayout			finalLayout				(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				const VkImageLayout			subpassLayout			(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

				attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalLayout));
				colorAttachmentReferences.push_back(AttachmentReference((deUint32)0, subpassLayout));

				const VkImageLayout			depthStencilLayout		(VK_IMAGE_LAYOUT_GENERAL);
				const vector<Subpass>		subpasses				(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, vector<AttachmentReference>(), colorAttachmentReferences, vector<AttachmentReference>(), AttachmentReference(VK_ATTACHMENT_UNUSED, depthStencilLayout), vector<deUint32>()));
				vector<SubpassDependency>	deps;

				// The first render pass lets the implementation add all subpass dependencies implicitly.
				// On the following passes only the dependency from external to first subpass is defined as
				// we need to make sure we have the image ready from previous render pass. In this case
				// the dependency from subpass 0 to external is added implicitly by the implementation.
				if (renderPassNdx > 0)
				{
					deps.push_back(SubpassDependency(VK_SUBPASS_EXTERNAL,										// deUint32				srcPass
													 0,															// deUint32				dstPass
													 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,				// VkPipelineStageFlags	srcStageMask
													 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,						// VkPipelineStageFlags	dstStageMask
													 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,						// VkAccessFlags		srcAccessMask
													 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,	// VkAccessFlags		dstAccessMask
													 0));														// VkDependencyFlags	flags
				}

				RenderPass					renderPass				(attachments, subpasses, deps);

				renderPasses.push_back(renderPass);
			}

			const deUint32				blurKernel	(12u);
			const ExternalTestConfig	testConfig	(VK_FORMAT_R8G8B8A8_UNORM, UVec2(128, 128), renderPasses, renderPassType, blurKernel);
			const string				testName	("render_passes_" + de::toString(renderPassCounts[renderPassCountNdx]));

			implicitGroup->addChild(new InstanceFactory1<ExternalDependencyTestInstance, ExternalTestConfig, ExternalPrograms>(testCtx, tcu::NODETYPE_SELF_VALIDATE, testName.c_str(), testName.c_str(), testConfig));
		}

		group->addChild(implicitGroup.release());
	}

	// Test late fragment operations using depth_stencil attachments in multipass rendering
	{
		const UVec2		renderSizes[]		=
		{
			UVec2(32, 32),
			UVec2(64, 64),
			UVec2(128, 128)
		};

		const deUint32	subpassCounts[]		= { 2u, 3u, 5u };

		// Implementations must support at least one of the following formats
		// for depth_stencil attachments
		const VkFormat formats[]			=
		{
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT
		};

		de::MovePtr<tcu::TestCaseGroup>	lateFragmentTestsGroup (new tcu::TestCaseGroup(testCtx, "late_fragment_tests", "wait for late fragment tests"));

		for (size_t renderSizeNdx = 0; renderSizeNdx < DE_LENGTH_OF_ARRAY(renderSizes); renderSizeNdx++)
		{
			string							renderSizeGroupName	("render_size_" + de::toString(renderSizes[renderSizeNdx].x()) + "_" + de::toString(renderSizes[renderSizeNdx].y()));
			de::MovePtr<tcu::TestCaseGroup>	renderSizeGroup		(new tcu::TestCaseGroup(testCtx, renderSizeGroupName.c_str(), renderSizeGroupName.c_str()));

			for (size_t subpassCountNdx = 0; subpassCountNdx < DE_LENGTH_OF_ARRAY(subpassCounts); subpassCountNdx++)
			{
				string							subpassGroupName	("subpass_count_" + de::toString(subpassCounts[subpassCountNdx]));
				de::MovePtr<tcu::TestCaseGroup>	subpassCountGroup	(new tcu::TestCaseGroup(testCtx, subpassGroupName.c_str(), subpassGroupName.c_str()));

				for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
				{
					const deUint32				subpassCount	(subpassCounts[subpassCountNdx]);
					const deUint32				attachmentCount	(subpassCount);
					vector<Subpass>				subpasses;
					vector<Attachment>			attachments;
					vector<SubpassDependency>	deps;

					// Attachments
					for (size_t attachmentNdx = 0; attachmentNdx < attachmentCount; attachmentNdx++)
					{
						const VkFormat				format						(formats[formatNdx]);
						const VkSampleCountFlagBits	sampleCount					(VK_SAMPLE_COUNT_1_BIT);
						const VkAttachmentLoadOp	loadOp						(VK_ATTACHMENT_LOAD_OP_CLEAR);
						const VkAttachmentStoreOp	storeOp						(VK_ATTACHMENT_STORE_OP_DONT_CARE);
						const VkAttachmentLoadOp	stencilLoadOp				(VK_ATTACHMENT_LOAD_OP_CLEAR);
						const VkAttachmentStoreOp	stencilStoreOp				(VK_ATTACHMENT_STORE_OP_STORE);
						const VkImageLayout			initialLayout				(VK_IMAGE_LAYOUT_UNDEFINED);
						const VkImageLayout			finalLayout					(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

						attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalLayout));
					}

					// Subpasses
					for (size_t subpassNdx = 0; subpassNdx < subpassCount; subpassNdx++)
					{
						vector<AttachmentReference>	inputAttachmentReferences;
						const VkImageAspectFlags	inputAttachmentAspectMask	((renderPassType == RENDERPASS_TYPE_RENDERPASS2)
																					? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT)
																					: static_cast<VkImageAspectFlags>(0));

						// Input attachment references
						if (subpassNdx > 0)
							inputAttachmentReferences.push_back(AttachmentReference((deUint32)subpassNdx - 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, inputAttachmentAspectMask));

						subpasses.push_back(Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, inputAttachmentReferences, vector<AttachmentReference>(), vector<AttachmentReference>(), AttachmentReference((deUint32)subpassNdx, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL), vector<deUint32>()));

						// Subpass dependencies from current subpass to previous subpass.
						// Subpasses will wait for the late fragment operations before reading the contents
						// of previous subpass.
						if (subpassNdx > 0)
						{
							deps.push_back(SubpassDependency((deUint32)subpassNdx - 1,							// deUint32				srcPass
															 (deUint32)subpassNdx,								// deUint32				dstPass
															 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,			// VkPipelineStageFlags	srcStageMask
															 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
																| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,	// VkPipelineStageFlags	dstStageMask
															 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags		srcAccessMask
															 VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,				// VkAccessFlags		dstAccessMask
															 VK_DEPENDENCY_BY_REGION_BIT));						// VkDependencyFlags	flags
						}
					}

					const RenderPass		renderPass	(attachments, subpasses, deps);
					const SubpassTestConfig	testConfig	(formats[formatNdx], renderSizes[renderSizeNdx], renderPass, renderPassType);
					const string			format		(formatToName(formats[formatNdx]).c_str());

					subpassCountGroup->addChild(new InstanceFactory1<SubpassDependencyTestInstance, SubpassTestConfig, SubpassPrograms>(testCtx, tcu::NODETYPE_SELF_VALIDATE, format, format, testConfig));
				}

				renderSizeGroup->addChild(subpassCountGroup.release());
			}

			lateFragmentTestsGroup->addChild(renderSizeGroup.release());
		}

		group->addChild(lateFragmentTestsGroup.release());
	}
}
} // anonymous

tcu::TestCaseGroup* createRenderPassSubpassDependencyTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "subpass_dependencies", "Subpass dependency tests", initTests, RENDERPASS_TYPE_LEGACY);
}

tcu::TestCaseGroup* createRenderPass2SubpassDependencyTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "subpass_dependencies", "Subpass dependency tests", initTests, RENDERPASS_TYPE_RENDERPASS2);
}
} // vkt
