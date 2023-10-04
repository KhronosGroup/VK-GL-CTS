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
 * \brief Tests for subpass dependency
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
#include "vkBuilderUtil.hpp"

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

tcu::TextureLevel getRepresentableDepthChannel (const ConstPixelBufferAccess& access)
{
	tcu::TextureLevel depthChannel (mapVkFormat(VK_FORMAT_R8G8B8_UNORM), access.getWidth(), access.getHeight());

	for (int y = 0; y < access.getHeight(); y++)
	for (int x = 0; x < access.getWidth(); x++)
		depthChannel.getAccess().setPixel(tcu::Vec4(access.getPixDepth(x, y)), x, y);

	return depthChannel;
}

bool verifyDepth (Context&						context,
				  const ConstPixelBufferAccess&	reference,
				  const ConstPixelBufferAccess&	result,
				  const float					threshold)
{
	tcu::TestLog& log (context.getTestContext().getLog());

	return tcu::floatThresholdCompare(log,										// log
									  "Depth channel",							// imageSetName
									  "Depth compare",							// imageSetDesc
									  getRepresentableDepthChannel(reference),	// reference
									  getRepresentableDepthChannel(result),		// result
									  Vec4(threshold),							// threshold
									  tcu::COMPARE_LOG_RESULT);					// logMode
}

bool verifyStencil (Context&						context,
					const ConstPixelBufferAccess&	reference,
					const ConstPixelBufferAccess&	result)
{
	tcu::TextureLevel	stencilErrorImage	(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), result.getWidth(), result.getHeight());
	tcu::TestLog&		log					(context.getTestContext().getLog());
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

class SelfDependencyBackwardsVertShader : public rr::VertexShader
{
public:
	SelfDependencyBackwardsVertShader (void)
	: rr::VertexShader (1, 0)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			packets[packetNdx]->position	= rr::readVertexAttribFloat(inputs[0],
																		packets[packetNdx]->instanceNdx,
																		packets[packetNdx]->vertexNdx);
		}
	}
};

class SelfDependencyBackwardsFragShader : public rr::FragmentShader
{
public:
	SelfDependencyBackwardsFragShader (void)
	: rr::FragmentShader(0, 1)
	{
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments (rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
	{
		DE_UNREF(packets);

		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
			for (deUint32 fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
				rr::writeFragmentOutput<tcu::Vec4>(context, packetNdx, fragNdx, 0, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
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
	ExternalTestConfig	(VkFormat				format_,
						 UVec2					imageSize_,
						 vector<RenderPass>		renderPasses_,
						 RenderingType			renderingType_,
						 SynchronizationType	synchronizationType_,
						 deUint32				blurKernel_ = 4)
		: format				(format_)
		, imageSize				(imageSize_)
		, renderPasses			(renderPasses_)
		, renderingType			(renderingType_)
		, synchronizationType	(synchronizationType_)
		, blurKernel			(blurKernel_)
	{
	}

	VkFormat			format;
	UVec2				imageSize;
	vector<RenderPass>	renderPasses;
	RenderingType		renderingType;
	SynchronizationType	synchronizationType;
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
																			 const RenderingType					renderingType,
																			 const SynchronizationType				synchronizationType);

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
	const bool								m_renderPass2Supported;
	const bool								m_synchronization2Supported;
	const RenderingType						m_renderingType;

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
	, m_renderPass2Supported		((testConfig.renderingType == RENDERING_TYPE_RENDERPASS2) && context.requireDeviceFunctionality("VK_KHR_create_renderpass2"))
	, m_synchronization2Supported	((testConfig.synchronizationType == SYNCHRONIZATION_TYPE_SYNCHRONIZATION2) && context.requireDeviceFunctionality("VK_KHR_synchronization2"))
	, m_renderingType				(testConfig.renderingType)
	, m_width						(testConfig.imageSize.x())
	, m_height						(testConfig.imageSize.y())
	, m_blurKernel					(testConfig.blurKernel)
	, m_format						(testConfig.format)
	, m_images						(createAndAllocateImages(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), m_imageMemories, context.getUniversalQueueFamilyIndex(), m_format, m_width, m_height, testConfig.renderPasses))
	, m_imageViews					(createImageViews(context.getDeviceInterface(), context.getDevice(), m_images, m_format, VK_IMAGE_ASPECT_COLOR_BIT))
	, m_samplers					(createSamplers(context.getDeviceInterface(), context.getDevice(), testConfig.renderPasses))
	, m_dstBuffer					(createBuffer(context.getDeviceInterface(), context.getDevice(), m_format, m_width, m_height))
	, m_dstBufferMemory				(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_dstBuffer))
	, m_renderPasses				(createRenderPasses(context.getDeviceInterface(), context.getDevice(), testConfig.renderPasses, testConfig.renderingType, testConfig.synchronizationType))
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
																				  const RenderingType		renderingType,
																				  const SynchronizationType	synchronizationType)
{
	vector<SharedPtrVkRenderPass> renderPasses;
	renderPasses.reserve(renderPassInfos.size());

	for (const auto& renderPassInfo : renderPassInfos)
		renderPasses.push_back(makeSharedPtr(createRenderPass(vkd, device, renderPassInfo, renderingType, synchronizationType)));

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
	switch (m_renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERING_TYPE_RENDERPASS2:
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
			0,											// VkAccessFlags			srcAccessMask
			VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout			oldLayout
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t					srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t					dstQueueFamilyIndex
			**m_images[m_renderPasses.size() - 1],		// VkImage					image
			imageSubresourceRange						// VkImageSubresourceRange	subresourceRange
		};
		// Since the implicit 'end' subpass dependency has VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT in its dstStageMask,
		// we can't form an execution dependency chain with a specific pipeline stage. The cases that provide an explict
		// 'end' subpass dependency could use a specific pipline stage, but there isn't a way to distinguish between the
		// implicit and explicit cases here.
		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
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
							 RenderingType	renderingType_)
		: format			(format_)
		, imageSize			(imageSize_)
		, renderPass		(renderPass_)
		, renderingType		(renderingType_)
	{
	}

	VkFormat			format;
	UVec2				imageSize;
	RenderPass			renderPass;
	RenderingType		renderingType;
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

	tcu::TestStatus						iterate							(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus						iterateInternal					(void);

private:
	const bool							m_extensionSupported;
	const RenderPass					m_renderPassInfo;
	const RenderingType					m_renderingType;

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
	, m_extensionSupported			((testConfig.renderingType == RENDERING_TYPE_RENDERPASS2) && context.requireDeviceFunctionality("VK_KHR_create_renderpass2"))
	, m_renderPassInfo				(testConfig.renderPass)
	, m_renderingType				(testConfig.renderingType)
	, m_width						(testConfig.imageSize.x())
	, m_height						(testConfig.imageSize.y())
	, m_format						(testConfig.format)
	, m_images						(createAndAllocateImages(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), m_imageMemories, context.getUniversalQueueFamilyIndex(), m_renderPassInfo, m_format, m_width, m_height))
	, m_imageViews					(createImageViews(context.getDeviceInterface(), context.getDevice(), m_images, m_format, isDepthStencilFormat(m_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT))
	, m_primaryBuffer				(createBuffer(context.getDeviceInterface(), context.getDevice(), m_format, m_width, m_height))
	, m_secondaryBuffer				(createBuffer(context.getDeviceInterface(), context.getDevice(), m_format, m_width, m_height))
	, m_primaryBufferMemory			(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_primaryBuffer))
	, m_secondaryBufferMemory		(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_secondaryBuffer))
	, m_renderPass					(createRenderPass(context.getDeviceInterface(), context.getDevice(), m_renderPassInfo, testConfig.renderingType))
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
		const VkFormatFeatureFlags	flags		= ( isDepthStencilFormat(m_format) ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT ) | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
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

tcu::TestStatus SubpassDependencyTestInstance::iterate (void)
{
	switch (m_renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERING_TYPE_RENDERPASS2:
			return iterateInternal<RenderpassSubpass2>();
		default:
			TCU_THROW(InternalError, "Impossible");
	}
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

	// Transition stencil aspects to the final layout directly.
	if (isDepthStencilFormat(m_format))
	{
		const VkImageSubresourceRange imageSubresourceRange =
		{
			VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask
			0u,								// uint32_t				baseMipLevel
			1u,								// uint32_t				levelCount
			0u,								// uint32_t				baseArrayLayer
			1u								// uint32_t				layerCount
		};

		VkImageMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType
			DE_NULL,											// const void*				pNext
			0u,													// VkAccessFlags			srcAccessMask
			VK_ACCESS_TRANSFER_READ_BIT,						// VkAccessFlags			dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,	// VkImageLayout			newLayout
			VK_QUEUE_FAMILY_IGNORED,							// uint32_t					srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,							// uint32_t					dstQueueFamilyIndex
			DE_NULL,											// VkImage					image
			imageSubresourceRange								// VkImageSubresourceRange	subresourceRange
		};

		for (deUint32 attachmentNdx = 0; attachmentNdx < attachmentCount; ++attachmentNdx)
		{
			barrier.image = **m_images[attachmentNdx];
			vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
		}
	}

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
			float x = 2.0f * rand.getFloat() - 1.0f;
			float y = 2.0f * rand.getFloat() - 1.0f;

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
				rr::RenderState							renderState			((rr::ViewportState(depthBuffer)), m_context.getDeviceProperties().limits.subPixelPrecisionBits);

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

			if (!verifyDepth(m_context, reference.getAccess(), resultDepthAccess, depthThreshold))
				m_resultCollector.fail("Depth compare failed.");

			if (!verifyStencil(m_context, referenceStencilAccess, resultStencilAccess))
				m_resultCollector.fail("Stencil compare failed.");
		}
		else
			DE_FATAL("Not implemented");
	}

	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

struct SubpassSelfDependencyBackwardsTestConfig
{
		SubpassSelfDependencyBackwardsTestConfig	(VkFormat		format_,
													 UVec2			imageSize_,
													 RenderingType	renderingType_)
		: format			(format_)
		, imageSize			(imageSize_)
		, renderingType		(renderingType_)
	{
	}

	VkFormat		format;
	UVec2			imageSize;
	RenderingType	renderingType;
};

class SubpassSelfDependencyBackwardsTestInstance : public TestInstance
{
public:
							SubpassSelfDependencyBackwardsTestInstance	(Context&									context,
																		 SubpassSelfDependencyBackwardsTestConfig	testConfig);

							~SubpassSelfDependencyBackwardsTestInstance	(void);

	tcu::TestStatus			iterate										(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus			iterateInternal								(void);

private:
	const bool				m_extensionSupported;
	const bool				m_featuresSupported;
	const RenderingType		m_renderingType;

	const deUint32			m_width;
	const deUint32			m_height;
	const VkFormat			m_format;
	tcu::ResultCollector	m_resultCollector;
};

SubpassSelfDependencyBackwardsTestInstance::SubpassSelfDependencyBackwardsTestInstance (Context& context, SubpassSelfDependencyBackwardsTestConfig testConfig)
	: TestInstance			(context)
	, m_extensionSupported	((testConfig.renderingType == RENDERING_TYPE_RENDERPASS2) && context.requireDeviceFunctionality("VK_KHR_create_renderpass2"))
	, m_featuresSupported	(context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER))
	, m_renderingType		(testConfig.renderingType)
	, m_width				(testConfig.imageSize.x())
	, m_height				(testConfig.imageSize.y())
	, m_format				(testConfig.format)
{
}

SubpassSelfDependencyBackwardsTestInstance::~SubpassSelfDependencyBackwardsTestInstance (void)
{
}

tcu::TestStatus SubpassSelfDependencyBackwardsTestInstance::iterate (void)
{
	switch (m_renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERING_TYPE_RENDERPASS2:
			return iterateInternal<RenderpassSubpass2>();
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
tcu::TestStatus SubpassSelfDependencyBackwardsTestInstance::iterateInternal (void)
{
	de::Random											rand					(5);
	const DeviceInterface&								vkd						(m_context.getDeviceInterface());
	const VkDevice										device					= m_context.getDevice();
	const deUint32										queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const Unique<VkCommandPool>							commandPool				(createCommandPool(vkd, m_context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>						commandBuffer			(allocateCommandBuffer(vkd, m_context.getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo		(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo			(DE_NULL);
	vector<Vec4>										vertexData;
	Move<VkImage>										outputImage;
	de::MovePtr<Allocation>								outputImageAllocation;
	Move<VkImageView>									outputImageView;
	Move<VkPipelineLayout>								pipelineLayout;
	Move<VkPipeline>									renderPipeline;
	Move<VkFramebuffer>									framebuffer;
	Move<VkRenderPass>									renderPass;
	Move<VkBuffer>										indirectBuffer;
	de::MovePtr<Allocation>								indirectBufferMemory;
	Move<VkBuffer>										resultBuffer;
	de::MovePtr<Allocation>								resultBufferMemory;
	const VkDeviceSize									indirectBufferSize		= 4 * sizeof(deUint32);
	Move<VkBuffer>										vertexBuffer;
	de::MovePtr<Allocation>								vertexBufferMemory;

	// Create output image.
	{
		const VkExtent3D		imageExtent		=
		{
			m_width,	// uint32_t	width
			m_height,	// uint32_t	height
			1u			// uint32_t	depth
		};

		VkImageUsageFlags		usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,								// const void*				pNext
			0u,										// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			m_format,								// VkFormat					format
			imageExtent,							// VkExtent3D				extent
			1u,										// uint32_t					mipLevels
			1u,										// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
			usage,									// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			1u,										// uint32_t					queueFamilyIndexCount
			&queueFamilyIndex,						// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		outputImage = createImage(vkd, device, &imageCreateInfo, DE_NULL);
		outputImageAllocation = m_context.getDefaultAllocator().allocate(getImageMemoryRequirements(vkd, device, *outputImage), MemoryRequirement::Any);
		VK_CHECK(vkd.bindImageMemory(device, *outputImage, outputImageAllocation->getMemory(), outputImageAllocation->getOffset()));
	}

	// Create indirect buffer and initialize.
	{
		const VkBufferUsageFlags	bufferUsage	(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		const VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
			DE_NULL,								// const void*			pNext
			0u,										// VkBufferCreateFlags	flags
			indirectBufferSize,						// VkDeviceSize			size
			bufferUsage,							// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
			0u,										// uint32_t				queueFamilyIndexCount
			DE_NULL									// const uint32_t*		pQueueFamilyIndices
		};

		indirectBuffer			= createBuffer(vkd, device, &bufferCreateInfo);
		indirectBufferMemory	= createBufferMemory(vkd, device, m_context.getDefaultAllocator(), *indirectBuffer);

		VkDrawIndirectCommand	drawIndirectCommand	=
		{
			64u,	// deUint32	vertexCount
			1u,		// deUint32	instanceCount
			0u,		// deUint32	firstVertex
			0u,		// deUint32	firstInstance
		};

		deMemcpy(indirectBufferMemory->getHostPtr(), (void*)&drawIndirectCommand, sizeof(VkDrawIndirectCommand));
		flushAlloc(vkd, device, *indirectBufferMemory);
	}

	// Create result buffer.
	{
		resultBuffer		= createBuffer(vkd, device, m_format, m_width, m_height);
		resultBufferMemory	= createBufferMemory(vkd, device, m_context.getDefaultAllocator(), *resultBuffer);
	}

	// Create descriptor set layout.
	Unique<VkDescriptorSetLayout>	descriptorSetLayout	(DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT)
			.build(vkd, device));
	// Create descriptor pool.
	Unique<VkDescriptorPool>		descriptorPool		(DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
			.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	// Create descriptor set.
	Unique<VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout));

	// Update descriptor set information.
	{
		VkDescriptorBufferInfo descIndirectBuffer = makeDescriptorBufferInfo(*indirectBuffer, 0, indirectBufferSize);

		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descIndirectBuffer)
			.update(vkd, device);
	}

	// Create render pipeline layout.
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
			DE_NULL,										// const void*					pNext
			(vk::VkPipelineLayoutCreateFlags)0,				// VkPipelineLayoutCreateFlags	flags
			1u,												// deUint32						setLayoutCount
			&*descriptorSetLayout,							// const VkDescriptorSetLayout*	pSetLayouts
			0u,												// deUint32						pushConstantRangeCount
			DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
		};

		pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);
	}

	// Create render pass.
	{
		vector<Attachment>			attachments;
		vector<AttachmentReference>	colorAttachmentReferences;

		attachments.push_back(Attachment(m_format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
		colorAttachmentReferences.push_back(AttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

		const vector<Subpass>		subpasses	(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, vector<AttachmentReference>(), colorAttachmentReferences, vector<AttachmentReference>(), AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL), vector<deUint32>()));
		vector<SubpassDependency>	deps;

		deps.push_back(SubpassDependency(0u,									// deUint32				srcPass
										 0u,									// deUint32				dstPass
										 VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,	// VkPipelineStageFlags	srcStageMask
										 VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,	// VkPipelineStageFlags	dstStageMask
										 VK_ACCESS_SHADER_WRITE_BIT,			// VkAccessFlags		srcAccessMask
										 VK_ACCESS_INDIRECT_COMMAND_READ_BIT,	// VkAccessFlags		dstAccessMask
										 0));									// VkDependencyFlags	flags

		renderPass = createRenderPass(vkd, device, RenderPass(attachments, subpasses, deps), m_renderingType);
	}

	// Create render pipeline.
	{
		const Unique<VkShaderModule>				vertexShaderModule			(createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule>				geometryShaderModule		(createShaderModule(vkd, device, m_context.getBinaryCollection().get("geom"), 0u));
		const Unique<VkShaderModule>				fragmentShaderModule		(createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u));

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

		const std::vector<VkViewport>				viewports					(1, makeViewport(tcu::UVec2(m_width, m_height)));
		const std::vector<VkRect2D>					scissors					(1, makeRect2D(tcu::UVec2(m_width, m_height)));

		renderPipeline = makeGraphicsPipeline(vkd,	// const DeviceInterface&						vk
				device,								// const VkDevice								device
				*pipelineLayout,					// const VkPipelineLayout						pipelineLayout
				*vertexShaderModule,				// const VkShaderModule							vertexShaderModule
				DE_NULL,							// const VkShaderModule							tessellationControlShaderModule
				DE_NULL,							// const VkShaderModule							tessellationEvalShaderModule
				*geometryShaderModule,				// const VkShaderModule							geometryShaderModule
				*fragmentShaderModule,				// const VkShaderModule							fragmentShaderModule
				*renderPass,						// const VkRenderPass							renderPass
				viewports,							// const std::vector<VkViewport>&				viewports
				scissors,							// const std::vector<VkRect2D>&					scissors
				VK_PRIMITIVE_TOPOLOGY_POINT_LIST,	// const VkPrimitiveTopology					topology
				0u,									// const deUint32								subpass
				0u,									// const deUint32								patchControlPoints
				&vertexInputState);					// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo
	}

	// Create framebuffer.
	{
		const VkImageViewCreateInfo imageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageViewCreateFlags	flags
			*outputImage,								// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType
			m_format,									// VkFormat					format
			makeComponentMappingRGBA(),					// VkComponentMapping		components
			{											// VkImageSubresourceRange	subresourceRange
				VK_IMAGE_ASPECT_COLOR_BIT,
				0u,
				1u,
				0u,
				1u
			}
		};
		outputImageView	= createImageView(vkd, device, &imageViewCreateInfo);

		const VkFramebufferCreateInfo framebufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkFramebufferCreateFlags	flags
			*renderPass,								// VkRenderPass				renderPass
			1u,											// uint32_t					attachmentCount
			&*outputImageView,							// const VkImageView*		pAttachments
			m_width,									// uint32_t					width
			m_height,									// uint32_t					height
			1u											// uint32_t					layers
		};

		framebuffer = vk::createFramebuffer(vkd, device, &framebufferCreateInfo);
	}

	// Generate random point locations (pixel centered to make reference comparison easier).
	for (int primitiveNdx = 0; primitiveNdx < 128; primitiveNdx++)
	{
		vertexData.push_back(Vec4((float)((rand.getUint32() % m_width) * 2) / (float)m_width - 1.0f,
				(float)((rand.getUint32() % m_height) * 2) / (float)m_height - 1.0f,
				1.0f, 1.0f));
	}

	// Upload vertex data.
	{
		const size_t				vertexDataSize		= vertexData.size() * sizeof(Vec4);

		const VkBufferCreateInfo	vertexBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	//	VkStructureType		sType;
			DE_NULL,								//	const void*			pNext;
			0u,										//	VkBufferCreateFlags	flags;
			(VkDeviceSize)vertexDataSize,			//	VkDeviceSize		size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		//	VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode		sharingMode;
			1u,										//	deUint32			queueFamilyCount;
			&queueFamilyIndex,						//	const deUint32*		pQueueFamilyIndices;
		};

		vertexBuffer		= createBuffer(vkd, m_context.getDevice(), &vertexBufferParams);
		vertexBufferMemory	= createBufferMemory(vkd, device, m_context.getDefaultAllocator(), *vertexBuffer);

		deMemcpy(vertexBufferMemory->getHostPtr(), vertexData.data(), vertexDataSize);
		flushAlloc(vkd, device, *vertexBufferMemory);
	}

	beginCommandBuffer(vkd, *commandBuffer);
	vkd.cmdBindPipeline(*commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *renderPipeline);
	vkd.cmdBindDescriptorSets(*commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

	// Begin render pass.
	{
		VkRect2D					renderArea	=
		{
			{ 0u, 0u },				// VkOffset2D	offset
			{ m_width, m_height }	// VkExtent2D	extent
		};

		VkClearValue				clearValue	= makeClearValueColor(Vec4(0.0f, 1.0f, 0.0f, 1.0f));

		const VkRenderPassBeginInfo	beginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType
			DE_NULL,									// const void*			pNext
			*renderPass,								// VkRenderPass			renderPass
			*framebuffer,								// VkFramebuffer		framebuffer
			renderArea,									// VkRect2D				renderArea
			1u,											// uint32_t				clearValueCount
			&clearValue									// const VkClearValue*	pClearValues
		};

		RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
	}

	const VkDeviceSize bindingOffset = 0;
	vkd.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer.get(), &bindingOffset);

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *renderPipeline);

	// The first indirect draw: Draw the first 64 items.
	vkd.cmdDrawIndirect(*commandBuffer, *indirectBuffer, 0u, 1u, 0u);

	// Barrier for indirect buffer.
	{
		const VkMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// VkStructureType	sType
			DE_NULL,							// const void*		pNext
			VK_ACCESS_SHADER_WRITE_BIT,			// VkAccessFlags	srcAccessMask
			VK_ACCESS_INDIRECT_COMMAND_READ_BIT	// VkAccessFlags	dstAccessMask
		};

		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 1u, &barrier, 0u, DE_NULL, 0u, DE_NULL);
	}

	// The second indirect draw: Draw the last 64 items.
	vkd.cmdDrawIndirect(*commandBuffer, *indirectBuffer, 0u, 1u, 0u);

	RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	// Copy results to a buffer.
	copyImageToBuffer(vkd, *commandBuffer, *outputImage, *resultBuffer, tcu::IVec2(m_width, m_height));

	endCommandBuffer(vkd, *commandBuffer);
	submitCommandsAndWait(vkd, m_context.getDevice(), m_context.getUniversalQueue(), *commandBuffer);
	invalidateMappedMemoryRange(vkd, m_context.getDevice(), resultBufferMemory->getMemory(), resultBufferMemory->getOffset(), VK_WHOLE_SIZE);

	// Verify result.
	{
		const tcu::TextureFormat format (mapVkFormat(m_format));

		const void* const					ptrResult		(resultBufferMemory->getHostPtr());
		tcu::TextureLevel					reference		(format, m_width, m_height);
		const tcu::ConstPixelBufferAccess	resultAccess	(format, m_width, m_height, 1, ptrResult);
		const PixelBufferAccess				referenceAccess	(reference.getAccess());


		// Setup and run reference renderer.
		{
			vector<Vec4>							triangles;
			const float								offset			= 0.03f;

			// Convert points into triangles to have quads similar to what GPU is producing from geometry shader.
			for (size_t vtxIdx = 0; vtxIdx < vertexData.size(); vtxIdx++)
			{
				triangles.push_back(vertexData[vtxIdx] + tcu::Vec4(-offset, offset, 0.0f, 0.0f));
				triangles.push_back(vertexData[vtxIdx] + tcu::Vec4(-offset, -offset, 0.0f, 0.0f));
				triangles.push_back(vertexData[vtxIdx] + tcu::Vec4(offset, offset, 0.0f, 0.0f));

				triangles.push_back(vertexData[vtxIdx] + tcu::Vec4(-offset, -offset, 0.0f, 0.0f));
				triangles.push_back(vertexData[vtxIdx] + tcu::Vec4(offset, offset, 0.0f, 0.0f));
				triangles.push_back(vertexData[vtxIdx] + tcu::Vec4(offset, -offset, 0.0f, 0.0f));
			}

			const SelfDependencyBackwardsVertShader	vertShader;
			const SelfDependencyBackwardsFragShader	fragShader;
			const rr::Renderer						renderer;
			const rr::Program						program			(&vertShader, &fragShader);
			const rr::MultisamplePixelBufferAccess	msAccess		(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(referenceAccess));
			const rr::RenderTarget					renderTarget	(msAccess);
			const rr::PrimitiveType					primitiveType	(rr::PRIMITIVETYPE_TRIANGLES);
			const rr::PrimitiveList					primitiveList	(rr::PrimitiveList(primitiveType, (deUint32)triangles.size(), 0));
			const rr::ViewportState					viewportState	(msAccess);
			const rr::RenderState					renderState		(viewportState, m_context.getDeviceProperties().limits.subPixelPrecisionBits);
			const rr::VertexAttrib					vertices		= rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &triangles[0]);

			tcu::clear(referenceAccess, tcu::UVec4(0, 255, 0, 255));
			renderer.draw(rr::DrawCommand(renderState, renderTarget, program, 1u, &vertices, primitiveList));
		}

		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(),	// log
										"Color buffer",							// imageSetName
										"",										// imageSetDesc
										referenceAccess,						// reference
										resultAccess,							// result
										Vec4(0.01f),							// threshold
										tcu::COMPARE_LOG_RESULT))				// logMode
		{
			m_resultCollector.fail("Image compare failed.");
		}
	}

	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

struct SeparateChannelsTestConfig
{
		SeparateChannelsTestConfig	(VkFormat		format_,
									 RenderingType	renderingType_)
		: format			(format_)
		, renderingType		(renderingType_)
	{
	}

	VkFormat		format;
	RenderingType	renderingType;
};

class SeparateChannelsTestInstance : public TestInstance
{
public:
							SeparateChannelsTestInstance	(Context&					context,
															 SeparateChannelsTestConfig	testConfig);

							~SeparateChannelsTestInstance	(void);

	tcu::TestStatus			iterate							(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus			iterateInternal					(void);

private:
	const bool				m_extensionSupported;
	const RenderingType		m_renderingType;

	const deUint32			m_width;
	const deUint32			m_height;
	const VkFormat			m_format;
	tcu::ResultCollector	m_resultCollector;
};

SeparateChannelsTestInstance::SeparateChannelsTestInstance (Context& context, SeparateChannelsTestConfig testConfig)
	: TestInstance			(context)
	, m_extensionSupported	((testConfig.renderingType == RENDERING_TYPE_RENDERPASS2) && context.requireDeviceFunctionality("VK_KHR_create_renderpass2"))
	, m_renderingType		(testConfig.renderingType)
	, m_width				(256u)
	, m_height				(256u)
	, m_format				(testConfig.format)
{
}

SeparateChannelsTestInstance::~SeparateChannelsTestInstance (void)
{
}

tcu::TestStatus SeparateChannelsTestInstance::iterate (void)
{
	switch (m_renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERING_TYPE_RENDERPASS2:
			return iterateInternal<RenderpassSubpass2>();
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
tcu::TestStatus SeparateChannelsTestInstance::iterateInternal (void)
{
	const DeviceInterface&								vkd						(m_context.getDeviceInterface());
	const VkDevice										device					= m_context.getDevice();
	const VkQueue										queue					= m_context.getUniversalQueue();
	const deUint32										queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const Unique<VkCommandPool>							commandPool				(createCommandPool(vkd, m_context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>						commandBuffer			(allocateCommandBuffer(vkd, m_context.getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo		(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo			(DE_NULL);
	const bool											isDSFormat				= isDepthStencilFormat(m_format);
	const VkFormat										colorFormat				= isDSFormat ? VK_FORMAT_R8G8B8A8_UNORM : m_format;
	const tcu::Vec4										colorInitValues[2]		= { tcu::Vec4(0.2f, 0.4f, 0.1f, 1.0f), tcu::Vec4(0.5f, 0.4f, 0.7f, 1.0f) };
	const float											depthInitValues[2]		= { 0.3f, 0.7f };
	const deUint32										stencilInitValues[2]	= { 2u, 100u };
	const deUint32										stencilRefValue			= 200u;
	const deUint32										tileSize				= 32u;
	vector<Vec4>										vertexData;
	Move<VkImage>										colorImage;
	de::MovePtr<Allocation>								colorImageAllocation;
	// When testing color formats the same attachment is used as input and output. This requires general layout to be used.
	const VkImageLayout									colorImageLayout		= isDSFormat ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
	Move<VkImage>										dsImage;
	de::MovePtr<Allocation>								dsImageAllocation;
	Move<VkImageView>									imageView;
	Move<VkImageView>									dsImageView;
	Move<VkPipelineLayout>								pipelineLayout;
	Move<VkPipeline>									renderPipeline;
	Move<VkFramebuffer>									framebuffer;
	Move<VkRenderPass>									renderPass;
	Move<VkBuffer>										resultBuffer0;
	de::MovePtr<Allocation>								resultBuffer0Memory;
	Move<VkBuffer>										resultBuffer1;
	de::MovePtr<Allocation>								resultBuffer1Memory;
	Move<VkBuffer>										vertexBuffer;
	de::MovePtr<Allocation>								vertexBufferMemory;

	const VkExtent3D		imageExtent		=
	{
		m_width,	// deUint32	width
		m_height,	// deUint32	height
		1u			// deUint32	depth
	};

	// Create image used for both input and output in case of color test, and as a color output in depth/stencil test.
	{
		VkImageUsageFlags		usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,								// const void*				pNext
			0u,										// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			colorFormat,							// VkFormat					format
			imageExtent,							// VkExtent3D				extent
			1u,										// uint32_t					mipLevels
			1u,										// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
			usage,									// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			1u,										// uint32_t					queueFamilyIndexCount
			&queueFamilyIndex,						// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		checkImageSupport(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), imageCreateInfo);

		colorImage = createImage(vkd, device, &imageCreateInfo, DE_NULL);
		colorImageAllocation = m_context.getDefaultAllocator().allocate(getImageMemoryRequirements(vkd, device, *colorImage), MemoryRequirement::Any);
		VK_CHECK(vkd.bindImageMemory(device, *colorImage, colorImageAllocation->getMemory(), colorImageAllocation->getOffset()));
	}

	// Create depth/stencil image
	if (isDSFormat)
	{
		VkImageUsageFlags		usage			= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,								// const void*				pNext
			0u,										// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			m_format,								// VkFormat					format
			imageExtent,							// VkExtent3D				extent
			1u,										// uint32_t					mipLevels
			1u,										// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
			usage,									// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			1u,										// uint32_t					queueFamilyIndexCount
			&queueFamilyIndex,						// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		checkImageSupport(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), imageCreateInfo);

		dsImage = createImage(vkd, device, &imageCreateInfo, DE_NULL);
		dsImageAllocation = m_context.getDefaultAllocator().allocate(getImageMemoryRequirements(vkd, device, *dsImage), MemoryRequirement::Any);
		VK_CHECK(vkd.bindImageMemory(device, *dsImage, dsImageAllocation->getMemory(), dsImageAllocation->getOffset()));

		// Initialize depth / stencil image
		initDepthStencilImageChessboardPattern(vkd, device, queue, queueFamilyIndex, m_context.getDefaultAllocator(), *dsImage, m_format, depthInitValues[0], depthInitValues[1], stencilInitValues[0], stencilInitValues[1], m_width, m_height, tileSize, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

	// Initialize color image
	initColorImageChessboardPattern(vkd, device, queue, queueFamilyIndex, m_context.getDefaultAllocator(), *colorImage, colorFormat, colorInitValues[0], colorInitValues[1], m_width, m_height, tileSize, VK_IMAGE_LAYOUT_UNDEFINED, colorImageLayout, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// Create color image views
	{
		const VkImageViewCreateInfo imageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageViewCreateFlags	flags
			*colorImage,								// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType
			colorFormat,								// VkFormat					format
			makeComponentMappingRGBA(),					// VkComponentMapping		components
			{											// VkImageSubresourceRange	subresourceRange
				VK_IMAGE_ASPECT_COLOR_BIT,
				0u,
				1u,
				0u,
				1u
			}
		};

		imageView = createImageView(vkd, device, &imageViewCreateInfo);
	}

	// Create depth/stencil image view
	if (isDSFormat)
	{
		const VkImageViewCreateInfo imageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageViewCreateFlags	flags
			*dsImage,									// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType
			m_format,									// VkFormat					format
			makeComponentMappingRGBA(),					// VkComponentMapping		components
			{											// VkImageSubresourceRange	subresourceRange
				VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
				0u,
				1u,
				0u,
				1u
			}
		};

		dsImageView	= createImageView(vkd, device, &imageViewCreateInfo);
	}

	// Create result buffers.
	{
		resultBuffer0		= createBuffer(vkd, device, m_format, m_width, m_height);
		resultBuffer0Memory	= createBufferMemory(vkd, device, m_context.getDefaultAllocator(), *resultBuffer0);
		resultBuffer1		= createBuffer(vkd, device, m_format, m_width, m_height);
		resultBuffer1Memory	= createBufferMemory(vkd, device, m_context.getDefaultAllocator(), *resultBuffer1);
	}

	// Create descriptor set layout.
	Unique<VkDescriptorSetLayout>	descriptorSetLayout	(DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(vkd, device));
	// Create descriptor pool.
	Unique<VkDescriptorPool>		descriptorPool		(DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
			.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	// Create descriptor set.
	Unique<VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout));

	// Update descriptor set information.
	if (!isDSFormat)
	{
		VkDescriptorImageInfo descInputAttachment = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descInputAttachment)
			.update(vkd, device);
	}

	// Create render pipeline layout.
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
			DE_NULL,										// const void*					pNext
			(vk::VkPipelineLayoutCreateFlags)0,				// VkPipelineLayoutCreateFlags	flags
			1u,												// deUint32						setLayoutCount
			&*descriptorSetLayout,							// const VkDescriptorSetLayout*	pSetLayouts
			0u,												// deUint32						pushConstantRangeCount
			DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
		};

		pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);
	}

	// Create render pass.
	{
		vector<Attachment>			attachments;
		vector<AttachmentReference>	colorAttachmentReferences;
		vector<AttachmentReference>	inputAttachmentReferences;
		AttachmentReference			dsAttachmentReference		(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

		const VkImageAspectFlags	inputAttachmentAspectMask	((m_renderingType == RENDERING_TYPE_RENDERPASS2)
																? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT)
																: static_cast<VkImageAspectFlags>(0));

		attachments.push_back(Attachment(colorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, colorImageLayout, colorImageLayout));
		colorAttachmentReferences.push_back(AttachmentReference(0u, colorImageLayout));

		if (isDSFormat)
		{
			attachments.push_back(Attachment(m_format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
		}
		else
		{
			inputAttachmentReferences.push_back(AttachmentReference(0u, VK_IMAGE_LAYOUT_GENERAL, inputAttachmentAspectMask));
		}

		const vector<Subpass>		subpasses	(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, inputAttachmentReferences, colorAttachmentReferences, vector<AttachmentReference>(), isDSFormat ? dsAttachmentReference : AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL), vector<deUint32>()));
		vector<SubpassDependency> subpassDependency;
		if(!isDSFormat)
		{
			/* Self supass-dependency */
			subpassDependency.push_back(SubpassDependency(0u, 0u, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));
		}
		renderPass = createRenderPass(vkd, device, RenderPass(attachments, subpasses, subpassDependency), m_renderingType);

	}

	// Create render pipeline.
	{
		const Unique<VkShaderModule>				vertexShaderModule			(createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule>				fragmentShaderModule		(createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u));

		const VkVertexInputBindingDescription		vertexBinding0				=
		{
			0u,							// deUint32					binding
			sizeof(Vec4),				// deUint32					strideInBytes
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	stepRate
		};

		const VkVertexInputAttributeDescription		attr0						=
		{
			0u,								// deUint32	location
			0u,								// deUint32	binding
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format
			0u								// deUint32	offsetInBytes
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputState			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			(VkPipelineVertexInputStateCreateFlags)0u,					// VkPipelineVertexInputStateCreateFlags	flags
			1u,															// deUint32									vertexBindingDescriptionCount
			&vertexBinding0,											// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
			1u,															// deUint32									vertexAttributeDescriptionCount
			&attr0														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
		};

		// Use write mask to enable only B and A channels to prevent self dependency (reads are done for channels R and G).
		const VkPipelineColorBlendAttachmentState	colorBlendAttachmentState	=
		{
			VK_FALSE,					// VkBool32					blendEnable
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			srcColorBlendFactor
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstColorBlendFactor
			VK_BLEND_OP_ADD,			// VkBlendOp				colorBlendOp
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			srcAlphaBlendFactor
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstAlphaBlendFactor
			VK_BLEND_OP_ADD,			// VkBlendOp				alphaBlendOp
			VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT	// VkColorComponentFlags	colorWriteMask
		};

		const VkPipelineColorBlendStateCreateInfo	colorBlendState				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType
			DE_NULL,													// const void*									pNext
			0u,															// VkPipelineColorBlendStateCreateFlags			flags
			VK_FALSE,													// VkBool32										logicOpEnable
			VK_LOGIC_OP_CLEAR,											// VkLogicOp									logicOp
			1u,															// deUint32										attachmentCount
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4]
		};

		const VkStencilOpState						stencilOpState				=
		{
			VK_STENCIL_OP_REPLACE,	// VkStencilOp	failOp
			VK_STENCIL_OP_REPLACE,	// VkStencilOp	passOp
			VK_STENCIL_OP_ZERO,		// VkStencilOp	depthFailOp
			VK_COMPARE_OP_ALWAYS,	// VkCompareOp	compareOp
			0xff,					// deUint32		compareMask
			0xff,					// deUint32		writeMask
			stencilRefValue			// deUint32		reference
		};

		const VkPipelineDepthStencilStateCreateInfo	depthStencilState			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags
			VK_TRUE,													// VkBool32									depthTestEnable
			VK_FALSE,													// VkBool32									depthWriteEnable
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp
			VK_FALSE,													// VkBool32									depthBoundsTestEnable
			VK_TRUE,													// VkBool32									stencilTestEnable
			stencilOpState,												// VkStencilOpState							front
			stencilOpState,												// VkStencilOpState							back
			0.0f,														// float									minDepthBounds
			1.0f														// float									maxDepthBounds
		};

		const std::vector<VkViewport>				viewports					(1, makeViewport(tcu::UVec2(m_width, m_height)));
		const std::vector<VkRect2D>					scissors					(1, makeRect2D(tcu::UVec2(m_width, m_height)));

		renderPipeline = makeGraphicsPipeline(vkd,			// const DeviceInterface&							vk
				device,										// const VkDevice									device
				*pipelineLayout,							// const VkPipelineLayout							pipelineLayout
				*vertexShaderModule,						// const VkShaderModule								vertexShaderModule
				DE_NULL,									// const VkShaderModule								tessellationControlShaderModule
				DE_NULL,									// const VkShaderModule								tessellationEvalShaderModule
				DE_NULL,									// const VkShaderModule								geometryShaderModule
				*fragmentShaderModule,						// const VkShaderModule								fragmentShaderModule
				*renderPass,								// const VkRenderPass								renderPass
				viewports,									// const std::vector<VkViewport>&					viewports
				scissors,									// const std::vector<VkRect2D>&						scissors
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,		// const VkPrimitiveTopology						topology
				0u,											// const deUint32									subpass
				0u,											// const deUint32									patchControlPoints
				&vertexInputState,							// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
				DE_NULL,									// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
				DE_NULL,									// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
				isDSFormat ? &depthStencilState : DE_NULL,	// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
				isDSFormat ? DE_NULL : &colorBlendState);	// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
	}

	// Create framebuffer.
	{
		const VkImageView				dsAttachments[]			=
		{
			*imageView,
			*dsImageView
		};

		const VkFramebufferCreateInfo	framebufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkFramebufferCreateFlags	flags
			*renderPass,								// VkRenderPass				renderPass
			isDSFormat ? 2u : 1u,						// uint32_t					attachmentCount
			isDSFormat ? dsAttachments : &*imageView,	// const VkImageView*		pAttachments
			m_width,									// uint32_t					width
			m_height,									// uint32_t					height
			1u											// uint32_t					layers
		};

		framebuffer = vk::createFramebuffer(vkd, device, &framebufferCreateInfo);
	}

	// Generate quad vertices
	{
		const tcu::Vec4	lowerLeftVertex		(-1.0f, -1.0f, 0.5f, 1.0f);
		const tcu::Vec4	lowerRightVertex	(1.0f, -1.0f, 0.5f, 1.0f);
		const tcu::Vec4	upperLeftVertex		(-1.0f, 1.0f, 0.5f, 1.0f);
		const tcu::Vec4	upperRightVertex	(1.0f, 1.0f, 0.5f, 1.0f);

		vertexData.push_back(lowerLeftVertex);
		vertexData.push_back(upperLeftVertex);
		vertexData.push_back(lowerRightVertex);
		vertexData.push_back(upperRightVertex);
	}

	// Upload vertex data.
	{
		const size_t				vertexDataSize		= vertexData.size() * sizeof(Vec4);

		const VkBufferCreateInfo	vertexBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	//	VkStructureType		sType
			DE_NULL,								//	const void*			pNext
			0u,										//	VkBufferCreateFlags	flags
			(VkDeviceSize)vertexDataSize,			//	VkDeviceSize		size
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		//	VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode		sharingMode
			1u,										//	deUint32			queueFamilyCount
			&queueFamilyIndex,						//	const deUint32*		pQueueFamilyIndices
		};

		vertexBuffer		= createBuffer(vkd, m_context.getDevice(), &vertexBufferParams);
		vertexBufferMemory	= createBufferMemory(vkd, device, m_context.getDefaultAllocator(), *vertexBuffer);

		deMemcpy(vertexBufferMemory->getHostPtr(), vertexData.data(), vertexDataSize);
		flushAlloc(vkd, device, *vertexBufferMemory);
	}

	beginCommandBuffer(vkd, *commandBuffer);
	vkd.cmdBindPipeline(*commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *renderPipeline);

	if (!isDSFormat)
		vkd.cmdBindDescriptorSets(*commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

	// Begin render pass.
	{
		VkRect2D					renderArea	=
		{
			{ 0u, 0u },				// VkOffset2D	offset
			{ m_width, m_height }	// VkExtent2D	extent
		};

		const VkRenderPassBeginInfo	beginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType
			DE_NULL,									// const void*			pNext
			*renderPass,								// VkRenderPass			renderPass
			*framebuffer,								// VkFramebuffer		framebuffer
			renderArea,									// VkRect2D				renderArea
			0u,											// uint32_t				clearValueCount
			DE_NULL										// const VkClearValue*	pClearValues
		};

		RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
	}

	const VkDeviceSize bindingOffset = 0;

	vkd.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer.get(), &bindingOffset);
	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *renderPipeline);

	if(!isDSFormat)
	{
		const VkImageMemoryBarrier	imageBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask;
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
			*colorImage,									// VkImage					image;
			makeImageSubresourceRange(1u, 0u, 1u, 0, 1u)	// VkImageSubresourceRange	subresourceRange;
		};
		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						VK_DEPENDENCY_BY_REGION_BIT, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);
	}

	vkd.cmdDraw(*commandBuffer, 4u, 1u, 0u, 0u);
	RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	// Copy results to a buffer.
	if (isDSFormat)
	{
		copyDepthStencilImageToBuffers(vkd, *commandBuffer, *dsImage, *resultBuffer0, *resultBuffer1, tcu::IVec2(m_width, m_height), VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}
	else
	{
		copyImageToBuffer(vkd, *commandBuffer, *colorImage, *resultBuffer0, tcu::IVec2(m_width, m_height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
	}

	endCommandBuffer(vkd, *commandBuffer);
	submitCommandsAndWait(vkd, m_context.getDevice(), m_context.getUniversalQueue(), *commandBuffer);
	invalidateMappedMemoryRange(vkd, m_context.getDevice(), resultBuffer0Memory->getMemory(), resultBuffer0Memory->getOffset(), VK_WHOLE_SIZE);
	invalidateMappedMemoryRange(vkd, m_context.getDevice(), resultBuffer1Memory->getMemory(), resultBuffer1Memory->getOffset(), VK_WHOLE_SIZE);

	// Verify result.
	{
		const tcu::TextureFormat	format		(mapVkFormat(m_format));
		tcu::TextureLevel			reference	(format, m_width, m_height);

		if (isDSFormat)
		{
			const void* const					ptrDepth				(resultBuffer0Memory->getHostPtr());
			const void* const					ptrStencil				(resultBuffer1Memory->getHostPtr());
			const tcu::ConstPixelBufferAccess	resultDepthAccess		(getDepthCopyFormat(m_format), m_width, m_height, 1, ptrDepth);
			const tcu::ConstPixelBufferAccess	resultStencilAccess		(getStencilCopyFormat(m_format), m_width, m_height, 1, ptrStencil);
			const PixelBufferAccess				referenceDepthAccess	(tcu::getEffectiveDepthStencilAccess(reference.getAccess(), tcu::Sampler::MODE_DEPTH));
			const PixelBufferAccess				referenceStencilAccess	(tcu::getEffectiveDepthStencilAccess(reference.getAccess(), tcu::Sampler::MODE_STENCIL));
			const float							depthThreshold			(1.0f / ((UVec4(1u) << tcu::getTextureFormatMantissaBitDepth(resultDepthAccess.getFormat()).cast<deUint32>()) - 1u).cast<float>().x());

			for (deUint32 x = 0; x < m_width; x++)
				for (deUint32 y = 0; y < m_height; y++)
				{
					float depthValue = ((x / tileSize) % 2 != (y / tileSize) % 2) ? depthInitValues[0] : depthInitValues[1];
					referenceDepthAccess.setPixDepth(depthValue, x, y, 0);
					referenceStencilAccess.setPixel(tcu::IVec4(0.5f < depthValue ? stencilRefValue : 0), x, y, 0);
				}

			if (!verifyDepth(m_context, reference.getAccess(), resultDepthAccess, depthThreshold))
				m_resultCollector.fail("Depth compare failed.");

			if (!verifyStencil(m_context, referenceStencilAccess, resultStencilAccess))
				m_resultCollector.fail("Stencil compare failed.");
		}
		else
		{
			const void* const					ptrResult		(resultBuffer0Memory->getHostPtr());
			const tcu::ConstPixelBufferAccess	resultAccess	(format, m_width, m_height, 1, ptrResult);
			const PixelBufferAccess				referenceAccess	(reference.getAccess());

			for (deUint32 x = 0; x < m_width; x++)
				for (deUint32 y = 0; y < m_height; y++)
				{
					const tcu::Vec4	initValue	= ((x / tileSize) % 2 != (y / tileSize) % 2) ? colorInitValues[0] : colorInitValues[1];
					const tcu::Vec4	refValue	= tcu::Vec4(initValue.x(), initValue.y(), initValue.x() + initValue.y(), 1.0f);

					referenceAccess.setPixel(refValue, x, y, 0);
				}

			if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(),	// log
											"Rendered result",						// imageSetName
											"",										// imageSetDesc
											referenceAccess,						// reference
											resultAccess,							// result
											Vec4(0.01f),							// threshold
											tcu::COMPARE_LOG_RESULT))				// logMode
			{
				m_resultCollector.fail("Image compare failed.");
			}
		}

	}

	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

struct SingleAttachmentTestConfig
{
		SingleAttachmentTestConfig	(VkFormat		format_,
									 RenderingType	renderingType_)
		: format			(format_)
		, renderingType		(renderingType_)
	{
	}

	VkFormat		format;
	RenderingType	renderingType;
};

class SingleAttachmentTestInstance : public TestInstance
{
public:
							SingleAttachmentTestInstance	(Context&					context,
															 SingleAttachmentTestConfig	testConfig);

							~SingleAttachmentTestInstance	(void);

	tcu::TestStatus			iterate							(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus			iterateInternal					(void);

private:
	const bool				m_extensionSupported;
	const RenderingType		m_renderingType;

	const deUint32			m_width;
	const deUint32			m_height;
	const VkFormat			m_format;
	tcu::ResultCollector	m_resultCollector;
};

SingleAttachmentTestInstance::SingleAttachmentTestInstance (Context& context, SingleAttachmentTestConfig testConfig)
	: TestInstance			(context)
	, m_extensionSupported	((testConfig.renderingType == RENDERING_TYPE_RENDERPASS2) && context.requireDeviceFunctionality("VK_KHR_create_renderpass2"))
	, m_renderingType		(testConfig.renderingType)
	, m_width				(256u)
	, m_height				(256u)
	, m_format				(testConfig.format)
{
}

SingleAttachmentTestInstance::~SingleAttachmentTestInstance (void)
{
}

tcu::TestStatus SingleAttachmentTestInstance::iterate (void)
{
	switch (m_renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERING_TYPE_RENDERPASS2:
			return iterateInternal<RenderpassSubpass2>();
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
tcu::TestStatus SingleAttachmentTestInstance::iterateInternal (void)
{
	const DeviceInterface&								vkd					(m_context.getDeviceInterface());
	const VkDevice										device				= m_context.getDevice();
	const VkQueue										queue				= m_context.getUniversalQueue();
	const deUint32										queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const Unique<VkCommandPool>							commandPool			(createCommandPool(vkd, m_context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>						commandBuffer		(allocateCommandBuffer(vkd, m_context.getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo		(DE_NULL);
	const tcu::Vec4										colorInitValues[2]	= { tcu::Vec4(0.2f, 0.4f, 0.1f, 1.0f), tcu::Vec4(0.5f, 0.4f, 0.7f, 1.0f) };
	const VkExtent3D									imageExtent			= { m_width, m_height, 1u };
	vector<Vec4>										vertexData;
	Move<VkImage>										colorImage;
	Move<VkImage>										resultImage;
	de::MovePtr<Allocation>								colorImageAllocation;
	de::MovePtr<Allocation>								resultImageAllocation;
	Move<VkImageView>									imageViewInput;
	Move<VkImageView>									imageViewResult;
	Move<VkPipelineLayout>								pipelineLayoutInput;
	Move<VkPipelineLayout>								pipelineLayoutImageSampler;
	Move<VkPipeline>									pipelineSolidColor;
	Move<VkPipeline>									pipelineInputAtt;
	Move<VkPipeline>									pipelineImageSampler;
	Move<VkFramebuffer>									framebuffer1;
	Move<VkFramebuffer>									framebuffer0;
	Move<VkRenderPass>									renderPass0;
	Move<VkRenderPass>									renderPass1;
	Move<VkBuffer>										resultBuffer;
	de::MovePtr<Allocation>								resultBufferMemory;
	Move<VkBuffer>										vertexBuffer;
	de::MovePtr<Allocation>								vertexBufferMemory;
	Move<VkSampler>										sampler;

	// Create image used for both input and output.
	{
		VkImageUsageFlags		usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
												  | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
												  | VK_IMAGE_USAGE_SAMPLED_BIT;

		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,								// const void*				pNext
			0u,										// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			m_format,								// VkFormat					format
			imageExtent,							// VkExtent3D				extent
			1u,										// uint32_t					mipLevels
			1u,										// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
			usage,									// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			1u,										// uint32_t					queueFamilyIndexCount
			&queueFamilyIndex,						// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		checkImageSupport(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), imageCreateInfo);

		colorImage = createImage(vkd, device, &imageCreateInfo, DE_NULL);
		colorImageAllocation = m_context.getDefaultAllocator().allocate(getImageMemoryRequirements(vkd, device, *colorImage), MemoryRequirement::Any);
		VK_CHECK(vkd.bindImageMemory(device, *colorImage, colorImageAllocation->getMemory(), colorImageAllocation->getOffset()));
	}

	// Create image used for final result.
	{
		VkImageUsageFlags		usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		const VkImageCreateInfo	imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,								// const void*				pNext
			0u,										// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			m_format,								// VkFormat					format
			imageExtent,							// VkExtent3D				extent
			1u,										// uint32_t					mipLevels
			1u,										// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
			usage,									// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			1u,										// uint32_t					queueFamilyIndexCount
			&queueFamilyIndex,						// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		checkImageSupport(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), imageCreateInfo);

		resultImage = createImage(vkd, device, &imageCreateInfo, DE_NULL);
		resultImageAllocation = m_context.getDefaultAllocator().allocate(getImageMemoryRequirements(vkd, device, *resultImage), MemoryRequirement::Any);
		VK_CHECK(vkd.bindImageMemory(device, *resultImage, resultImageAllocation->getMemory(), resultImageAllocation->getOffset()));
	}

	// Initialize color image. This is expected to be cleared later.
	initColorImageChessboardPattern(vkd, device, queue, queueFamilyIndex, m_context.getDefaultAllocator(), *colorImage, m_format, colorInitValues[0], colorInitValues[1], m_width, m_height, 32u, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	// Initialize result image. This will be overwritten later.
	initColorImageChessboardPattern(vkd, device, queue, queueFamilyIndex, m_context.getDefaultAllocator(), *resultImage, m_format, colorInitValues[0], colorInitValues[1], m_width, m_height, 32u, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// Create image views.
	{
		const VkImageViewCreateInfo imageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageViewCreateFlags	flags
			*colorImage,								// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType
			m_format,									// VkFormat					format
			makeComponentMappingRGBA(),					// VkComponentMapping		components
			{											// VkImageSubresourceRange	subresourceRange
				VK_IMAGE_ASPECT_COLOR_BIT,
				0u,
				1u,
				0u,
				1u
			}
		};

		imageViewInput = createImageView(vkd, device, &imageViewCreateInfo);
	}

	{
		const VkImageViewCreateInfo imageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageViewCreateFlags	flags
			*resultImage,								// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType
			m_format,									// VkFormat					format
			makeComponentMappingRGBA(),					// VkComponentMapping		components
			{											// VkImageSubresourceRange	subresourceRange
				VK_IMAGE_ASPECT_COLOR_BIT,
				0u,
				1u,
				0u,
				1u
			}
		};

		imageViewResult = createImageView(vkd, device, &imageViewCreateInfo);
	}

	// Create result buffer.
	{
		resultBuffer		= createBuffer(vkd, device, m_format, m_width, m_height);
		resultBufferMemory	= createBufferMemory(vkd, device, m_context.getDefaultAllocator(), *resultBuffer);
	}

	// Create sampler.
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

		sampler = createSampler(vkd, device, &samplerInfo);
	}

	// Create descriptor set layouts.
	Unique<VkDescriptorSetLayout>	descriptorSetLayoutInput	(DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(vkd, device));

	Unique<VkDescriptorSetLayout>	descriptorSetLayoutImageSampler	(DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(vkd, device));

	// Create descriptor pool.
	Unique<VkDescriptorPool>		descriptorPool		(DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
			.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u)
			.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u));

	// Create desriptor sets.
	Unique<VkDescriptorSet>			descriptorSetInput	(makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayoutInput));
	Unique<VkDescriptorSet>			descriptorSetImageSampler	(makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayoutImageSampler));

	// Update descriptor set information.
	VkDescriptorImageInfo			descIOAttachment	= makeDescriptorImageInfo(DE_NULL, *imageViewInput, VK_IMAGE_LAYOUT_GENERAL);
	VkDescriptorImageInfo			descImageSampler	= makeDescriptorImageInfo(*sampler, *imageViewInput, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSetInput, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descIOAttachment)
		.update(vkd, device);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSetImageSampler, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descImageSampler)
		.update(vkd, device);

	// Create pipeline layouts.
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
			DE_NULL,										// const void*					pNext
			(vk::VkPipelineLayoutCreateFlags)0,				// VkPipelineLayoutCreateFlags	flags
			1u,												// deUint32						setLayoutCount
			&descriptorSetLayoutInput.get(),				// const VkDescriptorSetLayout*	pSetLayouts
			0u,												// deUint32						pushConstantRangeCount
			DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
		};

		pipelineLayoutInput = createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);
	}
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
			DE_NULL,										// const void*					pNext
			(vk::VkPipelineLayoutCreateFlags)0,				// VkPipelineLayoutCreateFlags	flags
			1u,												// deUint32						setLayoutCount
			&descriptorSetLayoutImageSampler.get(),			// const VkDescriptorSetLayout*	pSetLayouts
			0u,												// deUint32						pushConstantRangeCount
			DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
		};

		pipelineLayoutImageSampler = createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);
	}

	// Create render passes.
	{
		vector<Attachment>			attachments;
		vector<AttachmentReference>	colorAttachmentReferences;

		attachments.push_back(Attachment(m_format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
										 VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
										 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

		colorAttachmentReferences.push_back(AttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

		const vector<Subpass>			subpasses	(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, vector<AttachmentReference>(),
													 colorAttachmentReferences, vector<AttachmentReference>(),
													 AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL), vector<deUint32>()));

		renderPass1 = createRenderPass(vkd, device, RenderPass(attachments, subpasses, vector<SubpassDependency>()), m_renderingType);
	}
	{
		vector<Attachment>			attachments;
		vector<AttachmentReference>	colorAttachmentReferences;
		vector<AttachmentReference>	inputAttachmentReferences;

		const VkImageAspectFlags	inputAttachmentAspectMask	((m_renderingType == RENDERING_TYPE_RENDERPASS2)
																? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT)
																: static_cast<VkImageAspectFlags>(0));

		attachments.push_back(Attachment(m_format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
										 VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));

		colorAttachmentReferences.push_back(AttachmentReference(0u, VK_IMAGE_LAYOUT_GENERAL));
		inputAttachmentReferences.push_back(AttachmentReference(0u, VK_IMAGE_LAYOUT_GENERAL, inputAttachmentAspectMask));

		const vector<Subpass>			subpasses		(1, Subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, inputAttachmentReferences,
														 colorAttachmentReferences, vector<AttachmentReference>(),
														 AttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL), vector<deUint32>()));

		const vector<SubpassDependency>	dependencies	(1, SubpassDependency(0u, 0u, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
														 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
														 VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

		renderPass0 = createRenderPass(vkd, device, RenderPass(attachments, subpasses, dependencies), m_renderingType);
	}

	// Create pipelines.
	{
		const Unique<VkShaderModule>				vertexShaderModule				(createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule>				fragmentShaderModuleInputAtt	(createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag_input_attachment"), 0u));
		const Unique<VkShaderModule>				fragmentShaderModuleSolidColor	(createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag_solid_color"), 0u));
		const Unique<VkShaderModule>				fragmentShaderModuleSampler		(createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag_combined_image_sampler"), 0u));

		const VkVertexInputBindingDescription		vertexBinding0					=
		{
			0u,							// deUint32					binding
			sizeof(Vec4),				// deUint32					strideInBytes
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	stepRate
		};

		const VkVertexInputAttributeDescription		attr0							=
		{
			0u,								// deUint32	location
			0u,								// deUint32	binding
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format
			0u								// deUint32	offsetInBytes
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputState				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			(VkPipelineVertexInputStateCreateFlags)0u,					// VkPipelineVertexInputStateCreateFlags	flags
			1u,															// deUint32									vertexBindingDescriptionCount
			&vertexBinding0,											// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
			1u,															// deUint32									vertexAttributeDescriptionCount
			&attr0														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
		};

		const std::vector<VkViewport>				viewports						(1, makeViewport(tcu::UVec2(m_width, m_height)));
		const std::vector<VkRect2D>					scissors						(1, makeRect2D(tcu::UVec2(m_width, m_height)));

		const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState	=
		{
			VK_TRUE,								// VkBool32					blendEnable
			VK_BLEND_FACTOR_ONE,					// VkBlendFactor			srcColorBlendFactor
			VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	// VkBlendFactor			dstColorBlendFactor
			VK_BLEND_OP_ADD,						// VkBlendOp				colorBlendOp
			VK_BLEND_FACTOR_ONE,					// VkBlendFactor			srcAlphaBlendFactor
			VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	// VkBlendFactor			dstAlphaBlendFactor
			VK_BLEND_OP_ADD,						// VkBlendOp				alphaBlendOp
			VK_COLOR_COMPONENT_R_BIT				// VkColorComponentFlags	colorWriteMask
				| VK_COLOR_COMPONENT_G_BIT
				| VK_COLOR_COMPONENT_B_BIT
				| VK_COLOR_COMPONENT_A_BIT
		};

		const VkPipelineColorBlendStateCreateInfo		colorBlendState				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType
			DE_NULL,													// const void*									pNext
			0u,															// VkPipelineColorBlendStateCreateFlags			flags
			VK_FALSE,													// VkBool32										logicOpEnable
			VK_LOGIC_OP_CLEAR,											// VkLogicOp									logicOp
			1u,															// deUint32										attachmentCount
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4]
		};

		pipelineSolidColor = makeGraphicsPipeline(vkd,		// const DeviceInterface&							vk
				device,										// const VkDevice									device
				*pipelineLayoutInput,						// const VkPipelineLayout							pipelineLayout
				*vertexShaderModule,						// const VkShaderModule								vertexShaderModule
				DE_NULL,									// const VkShaderModule								tessellationControlShaderModule
				DE_NULL,									// const VkShaderModule								tessellationEvalShaderModule
				DE_NULL,									// const VkShaderModule								geometryShaderModule
				*fragmentShaderModuleSolidColor,			// const VkShaderModule								fragmentShaderModule
				*renderPass0,								// const VkRenderPass								renderPass
				viewports,									// const std::vector<VkViewport>&					viewports
				scissors,									// const std::vector<VkRect2D>&						scissors
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,		// const VkPrimitiveTopology						topology
				0u,											// const deUint32									subpass
				0u,											// const deUint32									patchControlPoints
				&vertexInputState,							// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
				DE_NULL,									// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
				DE_NULL,									// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
				DE_NULL,									// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
				&colorBlendState);							// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo

		pipelineInputAtt = makeGraphicsPipeline(vkd,		// const DeviceInterface&							vk
				device,										// const VkDevice									device
				*pipelineLayoutInput,						// const VkPipelineLayout							pipelineLayout
				*vertexShaderModule,						// const VkShaderModule								vertexShaderModule
				DE_NULL,									// const VkShaderModule								tessellationControlShaderModule
				DE_NULL,									// const VkShaderModule								tessellationEvalShaderModule
				DE_NULL,									// const VkShaderModule								geometryShaderModule
				*fragmentShaderModuleInputAtt,				// const VkShaderModule								fragmentShaderModule
				*renderPass0,								// const VkRenderPass								renderPass
				viewports,									// const std::vector<VkViewport>&					viewports
				scissors,									// const std::vector<VkRect2D>&						scissors
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,		// const VkPrimitiveTopology						topology
				0u,											// const deUint32									subpass
				0u,											// const deUint32									patchControlPoints
				&vertexInputState,							// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
				DE_NULL,									// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
				DE_NULL,									// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
				DE_NULL,									// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
				&colorBlendState);							// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo

		pipelineImageSampler = makeGraphicsPipeline(vkd,	// const DeviceInterface&							vk
				device,										// const VkDevice									device
				*pipelineLayoutImageSampler,				// const VkPipelineLayout							pipelineLayout
				*vertexShaderModule,						// const VkShaderModule								vertexShaderModule
				DE_NULL,									// const VkShaderModule								tessellationControlShaderModule
				DE_NULL,									// const VkShaderModule								tessellationEvalShaderModule
				DE_NULL,									// const VkShaderModule								geometryShaderModule
				*fragmentShaderModuleSampler,				// const VkShaderModule								fragmentShaderModule
				*renderPass1,								// const VkRenderPass								renderPass
				viewports,									// const std::vector<VkViewport>&					viewports
				scissors,									// const std::vector<VkRect2D>&						scissors
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,		// const VkPrimitiveTopology						topology
				0u,											// const deUint32									subpass
				0u,											// const deUint32									patchControlPoints
				&vertexInputState,							// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
				DE_NULL,									// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
				DE_NULL,									// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
				DE_NULL,									// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
				&colorBlendState);							// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
	}

	// Create framebuffers.
	{
		const VkFramebufferCreateInfo	framebufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkFramebufferCreateFlags	flags
			*renderPass0,								// VkRenderPass				renderPass
			1u,											// uint32_t					attachmentCount
			&imageViewInput.get(),						// const VkImageView*		pAttachments
			256u,										// uint32_t					width
			256u,										// uint32_t					height
			1u											// uint32_t					layers
		};

		framebuffer0 = vk::createFramebuffer(vkd, device, &framebufferCreateInfo);
	}
	{
		const VkFramebufferCreateInfo	framebufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkFramebufferCreateFlags	flags
			*renderPass1,								// VkRenderPass				renderPass
			1u,											// uint32_t					attachmentCount
			&imageViewResult.get(),						// const VkImageView*		pAttachments
			m_width,									// uint32_t					width
			m_height,									// uint32_t					height
			1u											// uint32_t					layers
		};

		framebuffer1 = vk::createFramebuffer(vkd, device, &framebufferCreateInfo);
	}

	// Generate quad vertices.
	{
		const tcu::Vec4	lowerLeftVertex		(-1.0f, -1.0f, 0.5f, 1.0f);
		const tcu::Vec4	lowerRightVertex	(1.0f, -1.0f, 0.5f, 1.0f);
		const tcu::Vec4	upperLeftVertex		(-1.0f, 1.0f, 0.5f, 1.0f);
		const tcu::Vec4	upperRightVertex	(1.0f, 1.0f, 0.5f, 1.0f);

		vertexData.push_back(lowerLeftVertex);
		vertexData.push_back(upperLeftVertex);
		vertexData.push_back(lowerRightVertex);
		vertexData.push_back(upperRightVertex);
	}

	// Upload vertex data.
	{
		const size_t				vertexDataSize		= vertexData.size() * sizeof(Vec4);

		const VkBufferCreateInfo	vertexBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	//	VkStructureType		sType
			DE_NULL,								//	const void*			pNext
			0u,										//	VkBufferCreateFlags	flags
			(VkDeviceSize)vertexDataSize,			//	VkDeviceSize		size
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		//	VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode		sharingMode
			1u,										//	deUint32			queueFamilyCount
			&queueFamilyIndex,						//	const deUint32*		pQueueFamilyIndices
		};

		vertexBuffer		= createBuffer(vkd, m_context.getDevice(), &vertexBufferParams);
		vertexBufferMemory	= createBufferMemory(vkd, device, m_context.getDefaultAllocator(), *vertexBuffer);

		deMemcpy(vertexBufferMemory->getHostPtr(), vertexData.data(), vertexDataSize);
		flushAlloc(vkd, device, *vertexBufferMemory);
	}

	beginCommandBuffer(vkd, *commandBuffer);

	// Begin render pass.
	{
		const VkRect2D				renderArea	=
		{
			{ 0u, 0u },				// VkOffset2D	offset
			{ m_width, m_height }	// VkExtent2D	extent
		};

		const VkClearValue			clearValue	= makeClearValueColor(Vec4(0.0f, 0.0f, 0.0f, 0.0f));

		const VkRenderPassBeginInfo	beginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType
			DE_NULL,									// const void*			pNext
			*renderPass0,								// VkRenderPass			renderPass
			*framebuffer0,								// VkFramebuffer		framebuffer
			renderArea,									// VkRect2D				renderArea
			1u,											// uint32_t				clearValueCount
			&clearValue									// const VkClearValue*	pClearValues
		};

		RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
	}

	// Bind pipeline.
	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineSolidColor);

	// Bind vertex buffer.
	const VkDeviceSize bindingOffset = 0;
	vkd.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer.get(), &bindingOffset);

	// Bind descriptor set.
	vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayoutInput, 0u, 1u, &*descriptorSetInput, 0u, DE_NULL);

	// Draw solid color.
	vkd.cmdDraw(*commandBuffer, 4u, 1u, 0u, 0u);

	// Bind pipeline.
	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineInputAtt);

	// Bind descriptor set.
	vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayoutInput, 0u, 1u, &*descriptorSetInput, 0u, DE_NULL);

	// Pipeline barrier to handle self dependency.
	{
		const VkImageMemoryBarrier imageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			// VkAccessFlags			dstAccessMask
			VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			oldLayout
			VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			newLayout
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t					srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t					dstQueueFamilyIndex
			*colorImage,									// VkImage					image
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange
		};

		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);
	}

	// Draw. Adds (0.1, 0.2, 0.0, 0.0) to the previous result.
	vkd.cmdDraw(*commandBuffer, 4u, 1u, 0u, 0u);

	// End render pass.
	RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	// Pipeline barrier.
	{
		const VkImageMemoryBarrier imageBarriers[] =
		{
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType
				DE_NULL,										// const void*				pNext
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
				| VK_ACCESS_TRANSFER_WRITE_BIT
				| VK_ACCESS_HOST_WRITE_BIT,						// VkAccessFlags			srcAccessMask
				VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags			dstAccessMask
				VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			oldLayout
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,		// VkImageLayout			newLayout
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t					srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t					dstQueueFamilyIndex
				*colorImage,									// VkImage					image
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType
				DE_NULL,										// const void*				pNext
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
				| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			oldLayout
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			newLayout
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t					srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t					dstQueueFamilyIndex
				*resultImage,									// VkImage					image
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange
			}
		};

		vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 2u, imageBarriers);
	}

	// Begin render pass.
	{
		const VkRect2D				renderArea	=
		{
			{ 0, 0 },	            // VkOffset2D	offset
			{ m_width, m_height }	// VkExtent2D	extent
		};

		const VkRenderPassBeginInfo	beginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType
			DE_NULL,									// const void*			pNext
			*renderPass1,								// VkRenderPass			renderPass
			*framebuffer1,								// VkFramebuffer		framebuffer
			renderArea,									// VkRect2D				renderArea
			0u,											// uint32_t				clearValueCount
			DE_NULL										// const VkClearValue*	pClearValues
		};

		RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
	}

	// Bind pipeline.
	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineImageSampler);

	// Bind descriptor set.
	vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayoutImageSampler, 0u, 1u, &*descriptorSetImageSampler, 0u, DE_NULL);

	// Draw. Samples the previous results and adds (0.1, 0.2, 0.0, 0.0).
	vkd.cmdDraw(*commandBuffer, 4u, 1u, 0u, 0u);

	// End render pass.
	RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	// Copy results to a buffer.
	copyImageToBuffer(vkd, *commandBuffer, *resultImage, *resultBuffer, tcu::IVec2(m_width, m_height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	endCommandBuffer(vkd, *commandBuffer);
	submitCommandsAndWait(vkd, m_context.getDevice(), m_context.getUniversalQueue(), *commandBuffer);
	invalidateMappedMemoryRange(vkd, m_context.getDevice(), resultBufferMemory->getMemory(), resultBufferMemory->getOffset(), VK_WHOLE_SIZE);

	// Verify results.
	{
		const tcu::TextureFormat			format			(mapVkFormat(m_format));
		tcu::TextureLevel					reference		(format, m_width, m_height);
		const void* const					ptrResult		(resultBufferMemory->getHostPtr());
		const tcu::ConstPixelBufferAccess	resultAccess	(format, m_width, m_height, 1, ptrResult);
		const PixelBufferAccess				referenceAccess	(reference.getAccess());

		for (deUint32 x = 0; x < m_width; x++)
			for (deUint32 y = 0; y < m_height; y++)
			{
				referenceAccess.setPixel(tcu::Vec4(0.3f, 0.6f, 0.0f, 1.0f), x, y, 0);
			}

		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(),	// log
										"Rendered result",						// imageSetName
										"",										// imageSetDesc
										referenceAccess,						// reference
										resultAccess,							// result
										Vec4(0.05f),							// threshold
										tcu::COMPARE_LOG_RESULT))				// logMode
		{
			m_resultCollector.fail("Image compare failed.");
		}
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

// Shader programs for testing backwards subpass self dependency from geometry stage to indirect draw
struct SubpassSelfDependencyBackwardsPrograms
{
	void init (vk::SourceCollections& dst, SubpassSelfDependencyBackwardsTestConfig testConfig) const
	{
		DE_UNREF(testConfig);

		dst.glslSources.add("vert") << glu::VertexSource(
				"#version 450\n"
				"layout(location = 0) in highp vec4 position;\n"
				"out gl_PerVertex {\n"
				"    vec4 gl_Position;\n"
				"};\n"
				"void main (void)\n"
				"{\n"
				"    gl_Position = position;\n"
				"}\n");

		dst.glslSources.add("geom") << glu::GeometrySource(
				"#version 450\n"
				"layout(points) in;\n"
				"layout(triangle_strip, max_vertices = 4) out;\n"
				"\n"
				"in gl_PerVertex {\n"
				"    vec4 gl_Position;\n"
				"} gl_in[];\n"
				"\n"
				"out gl_PerVertex {\n"
				"    vec4 gl_Position;\n"
				"};\n"
				"layout (binding = 0) buffer IndirectBuffer\n"
				"{\n"
				"    uint vertexCount;\n"
				"    uint instanceCount;\n"
				"    uint firstVertex;\n"
				"    uint firstInstance;\n"
				"} indirectBuffer;\n"
				"\n"
				"void main (void) {\n"
				"    vec4 p = gl_in[0].gl_Position;\n"
				"    float offset = 0.03f;\n"
				"    gl_Position = p + vec4(-offset, offset, 0, 0);\n"
				"    EmitVertex();\n"
				"    gl_Position = p + vec4(-offset, -offset, 0, 0);\n"
				"    EmitVertex();\n"
				"    gl_Position = p + vec4(offset, offset, 0, 0);\n"
				"    EmitVertex();\n"
				"    gl_Position = p + vec4(offset, -offset, 0, 0);\n"
				"    EmitVertex();\n"
				"    EndPrimitive();\n"
				"    indirectBuffer.vertexCount = 64;\n"
				"    indirectBuffer.instanceCount = 1;\n"
				"    indirectBuffer.firstVertex = 64;\n"
				"    indirectBuffer.firstInstance = 0;\n"
				"}\n");

		dst.glslSources.add("frag") << glu::FragmentSource(
				"#version 450\n"
				"layout(location = 0) out highp vec4 fragColor;\n"
				"void main (void)\n"
				"{\n"
				"    fragColor = vec4(1, 0, 0, 1);\n"
				"}\n");
	}
};

struct SeparateChannelsPrograms
{
	void init (vk::SourceCollections& dst, SeparateChannelsTestConfig testConfig) const
	{
		dst.glslSources.add("vert") << glu::VertexSource(
				"#version 450\n"
				"layout(location = 0) in highp vec4 position;\n"
				"void main (void)\n"
				"{\n"
				"    gl_Position = position;\n"
				"}\n");

		if (isDepthStencilFormat(testConfig.format))
		{
			dst.glslSources.add("frag") << glu::FragmentSource(
					"#version 450\n"
					"layout(location = 0) out highp vec4 fragColor;\n"
					"void main (void)\n"
					"{\n"
					"    fragColor = vec4(1);\n"
					"}\n");
		}
		else
		{
			dst.glslSources.add("frag") << glu::FragmentSource(
					"#version 450\n"
					"layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput inputAtt;\n"
					"layout(location = 0) out highp vec4 fragColor;\n"
					"void main (void)\n"
					"{\n"
					"    vec4 inputColor = subpassLoad(inputAtt);\n"
					"    fragColor = vec4(1, 1, inputColor.r + inputColor.g, 1);\n"
					"}\n");
		}
	}
};

struct SingleAttachmentPrograms
{
	void init (vk::SourceCollections& dst, SingleAttachmentTestConfig testConfig) const
	{
		DE_UNREF(testConfig);

		dst.glslSources.add("vert") << glu::VertexSource(
				"#version 450\n"
				"layout(location = 0) in highp vec4 position;\n"
				"void main (void)\n"
				"{\n"
				"    gl_Position = position;\n"
				"}\n");

		dst.glslSources.add("frag_solid_color") << glu::FragmentSource(
				"#version 450\n"
				"layout(location = 0) out highp vec4 fragColor;\n"
				"void main (void)\n"
				"{\n"
				"    fragColor = vec4(0.1, 0.2, 0.0, 1.0);\n"
				"}\n");

		dst.glslSources.add("frag_input_attachment") << glu::FragmentSource(
				"#version 450\n"
				"layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput inputAtt;\n"
				"layout(location = 0) out highp vec4 fragColor;\n"
				"void main (void)\n"
				"{\n"
				"    vec4 inputColor = subpassLoad(inputAtt);\n"
				"    fragColor = inputColor + vec4(0.1, 0.2, 0.0, 0.0);\n"
				"}\n");

		dst.glslSources.add("frag_combined_image_sampler") << glu::FragmentSource(
				"#version 450\n"
				"layout(set = 0, binding = 0) uniform highp sampler2D tex;\n"
				"layout(location = 0) out highp vec4 fragColor;\n"
				"void main (void)\n"
				"{\n"
				"    vec2 uv = vec2(gl_FragCoord) / 255.0;\n"
				"    vec4 inputColor = texture(tex, uv);\n"
				"    fragColor = inputColor + vec4(0.1, 0.2, 0.0, 0.0);\n"
				"}\n");
	}
};

std::string formatToName (VkFormat format)
{
	const std::string	formatStr	= de::toString(format);
	const std::string	prefix		= "VK_FORMAT_";

	DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

	return de::toLower(formatStr.substr(prefix.length()));
}

void initTests (tcu::TestCaseGroup* group, const RenderingType renderingType)
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
			string groupName ("render_size_" + de::toString(renderSizes[renderSizeNdx].x()) + "_" + de::toString(renderSizes[renderSizeNdx].y()));
			de::MovePtr<tcu::TestCaseGroup> renderSizeGroup	(new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupName.c_str()));

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
													 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,						// VkPipelineStageFlags	dstStageMask
													 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,						// VkAccessFlags		srcAccessMask
													 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,	// VkAccessFlags		dstAccessMask
													 0));														// VkDependencyFlags	flags

					deps.push_back(SubpassDependency(0,															// deUint32				srcPass
													 VK_SUBPASS_EXTERNAL,										// deUint32				dstPass
													 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,						// VkPipelineStageFlags	srcStageMask
													 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,				// VkPipelineStageFlags	dstStageMask
													 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,	// VkAccessFlags		srcAccessMask
													 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,						// VkAccessFlags		dstAccessMask
													 0));														// VkDependencyFlags	flags

					RenderPass					renderPass			(attachments, subpasses, deps);

					renderPasses.push_back(renderPass);
				}

				const deUint32		blurKernel	(12u);
				string				testName	("render_passes_" + de::toString(renderPassCounts[renderPassCountNdx]));
				ExternalTestConfig	testConfig
				{
					VK_FORMAT_R8G8B8A8_UNORM,
					renderSizes[renderSizeNdx],
					renderPasses,
					renderingType,
					SYNCHRONIZATION_TYPE_LEGACY,
					blurKernel
				};

				renderSizeGroup->addChild(new InstanceFactory1<ExternalDependencyTestInstance, ExternalTestConfig, ExternalPrograms>(testCtx, testName.c_str(), testName.c_str(), testConfig));
				if (renderingType == RENDERING_TYPE_RENDERPASS2)
				{
					testName += "_sync_2";
					testConfig.synchronizationType = SYNCHRONIZATION_TYPE_SYNCHRONIZATION2;
					renderSizeGroup->addChild(new InstanceFactory1<ExternalDependencyTestInstance, ExternalTestConfig, ExternalPrograms>(testCtx, testName.c_str(), testName.c_str(), testConfig));
				}
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
													 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,				// VkPipelineStageFlags	srcStageMask
													 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,						// VkPipelineStageFlags	dstStageMask
													 0,						// VkAccessFlags		srcAccessMask
													 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,	// VkAccessFlags		dstAccessMask
													 0));														// VkDependencyFlags	flags
				}

				RenderPass					renderPass				(attachments, subpasses, deps);

				renderPasses.push_back(renderPass);
			}

			const deUint32				blurKernel	(12u);
			const ExternalTestConfig	testConfig	(VK_FORMAT_R8G8B8A8_UNORM, UVec2(128, 128), renderPasses, renderingType, SYNCHRONIZATION_TYPE_LEGACY, blurKernel);
			const string				testName	("render_passes_" + de::toString(renderPassCounts[renderPassCountNdx]));

			implicitGroup->addChild(new InstanceFactory1<ExternalDependencyTestInstance, ExternalTestConfig, ExternalPrograms>(testCtx, testName.c_str(), testName.c_str(), testConfig));
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
						const VkAttachmentStoreOp	storeOp						((attachmentNdx == attachmentCount - 1)
																					? VK_ATTACHMENT_STORE_OP_STORE
																					: VK_ATTACHMENT_STORE_OP_DONT_CARE);
						const VkAttachmentLoadOp	stencilLoadOp				(VK_ATTACHMENT_LOAD_OP_CLEAR);
						const VkAttachmentStoreOp	stencilStoreOp				((attachmentNdx == attachmentCount - 1)
																					? VK_ATTACHMENT_STORE_OP_STORE
																					: VK_ATTACHMENT_STORE_OP_DONT_CARE);
						const VkImageLayout			initialLayout				(VK_IMAGE_LAYOUT_UNDEFINED);
						const VkImageLayout			finalLayout					(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

						attachments.push_back(Attachment(format, sampleCount, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalLayout));
					}

					// Subpasses
					for (size_t subpassNdx = 0; subpassNdx < subpassCount; subpassNdx++)
					{
						vector<AttachmentReference>	inputAttachmentReferences;
						const VkImageAspectFlags	inputAttachmentAspectMask	((renderingType == RENDERING_TYPE_RENDERPASS2)
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
					deps.push_back(SubpassDependency((deUint32)subpassCount - 1,								// deUint32				srcPass
													 VK_SUBPASS_EXTERNAL,										// deUint32				dstPass
													 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,					// VkPipelineStageFlags	srcStageMask
													 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,					// VkPipelineStageFlags	dstStageMask
													 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,				// VkAccessFlags		srcAccessMask
													 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // VkAccessFlags		dstAccessMask
													 VK_DEPENDENCY_BY_REGION_BIT));								// VkDependencyFlags	flags

					const RenderPass		renderPass	(attachments, subpasses, deps);
					const SubpassTestConfig	testConfig	(formats[formatNdx], renderSizes[renderSizeNdx], renderPass, renderingType);
					const string			format		(formatToName(formats[formatNdx]).c_str());

					subpassCountGroup->addChild(new InstanceFactory1<SubpassDependencyTestInstance, SubpassTestConfig, SubpassPrograms>(testCtx, format, format, testConfig));
				}

				renderSizeGroup->addChild(subpassCountGroup.release());
			}

			lateFragmentTestsGroup->addChild(renderSizeGroup.release());
		}

		group->addChild(lateFragmentTestsGroup.release());
	}

	// Test subpass self dependency
	{
		const UVec2		renderSizes[]		=
		{
			UVec2(64, 64),
			UVec2(128, 128),
			UVec2(512, 512)
		};

		de::MovePtr<tcu::TestCaseGroup>	selfDependencyGroup	(new tcu::TestCaseGroup(testCtx, "self_dependency", "self_dependency"));

		for (size_t renderSizeNdx = 0; renderSizeNdx < DE_LENGTH_OF_ARRAY(renderSizes); renderSizeNdx++)
		{
			string groupName	("render_size_" + de::toString(renderSizes[renderSizeNdx].x()) + "_" + de::toString(renderSizes[renderSizeNdx].y()));
			de::MovePtr<tcu::TestCaseGroup>	renderSizeGroup	(new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupName.c_str()));

			const SubpassSelfDependencyBackwardsTestConfig	testConfig	(VK_FORMAT_R8G8B8A8_UNORM, renderSizes[renderSizeNdx], renderingType);
			renderSizeGroup->addChild(new InstanceFactory1<SubpassSelfDependencyBackwardsTestInstance, SubpassSelfDependencyBackwardsTestConfig, SubpassSelfDependencyBackwardsPrograms>(testCtx, "geometry_to_indirectdraw", "", testConfig));

			selfDependencyGroup->addChild(renderSizeGroup.release());
		}

		group->addChild(selfDependencyGroup.release());
	}

	// Test using a single attachment with reads and writes using separate channels. This should work without subpass self-dependency.
	{
		de::MovePtr<tcu::TestCaseGroup>	separateChannelsGroup	(new tcu::TestCaseGroup(testCtx, "separate_channels", "separate_channels"));

		struct TestConfig
		{
			string		name;
			VkFormat	format;
		} configs[] =
		{
			{	"r8g8b8a8_unorm",		VK_FORMAT_R8G8B8A8_UNORM		},
			{	"r16g16b16a16_sfloat",	VK_FORMAT_R16G16B16A16_SFLOAT	},
			{	"d24_unorm_s8_uint",	VK_FORMAT_D24_UNORM_S8_UINT		},
			{	"d32_sfloat_s8_uint",	VK_FORMAT_D32_SFLOAT_S8_UINT	}
		};

		for (deUint32 configIdx = 0; configIdx < DE_LENGTH_OF_ARRAY(configs); configIdx++)
		{
			const SeparateChannelsTestConfig testConfig(configs[configIdx].format, renderingType);

			separateChannelsGroup->addChild(new InstanceFactory1<SeparateChannelsTestInstance, SeparateChannelsTestConfig, SeparateChannelsPrograms>(testCtx, configs[configIdx].name, "", testConfig));
		}

		group->addChild(separateChannelsGroup.release());
	}

	// Test using a single attachment for input and output.
	{
		de::MovePtr<tcu::TestCaseGroup>	singleAttachmentGroup	(new tcu::TestCaseGroup(testCtx, "single_attachment", "single_attachment"));

		struct TestConfig
		{
			string		name;
			VkFormat	format;
		} configs[] =
		{
			{	"r8g8b8a8_unorm",			VK_FORMAT_R8G8B8A8_UNORM		},
			{	"b8g8r8a8_unorm",			VK_FORMAT_B8G8R8A8_UNORM		},
			{	"r16g16b16a16_sfloat",		VK_FORMAT_R16G16B16A16_SFLOAT	},
			{	"r5g6b5_unorm_pack16",		VK_FORMAT_R5G6B5_UNORM_PACK16	},
			{	"a1r5g5b5_unorm_pack16",	VK_FORMAT_A1R5G5B5_UNORM_PACK16	}
		};

		for (deUint32 configIdx = 0; configIdx < DE_LENGTH_OF_ARRAY(configs); configIdx++)
		{
			const SingleAttachmentTestConfig testConfig(configs[configIdx].format, renderingType);

			singleAttachmentGroup->addChild(new InstanceFactory1<SingleAttachmentTestInstance, SingleAttachmentTestConfig, SingleAttachmentPrograms>(testCtx, configs[configIdx].name, "", testConfig));
		}

		group->addChild(singleAttachmentGroup.release());
	}
}
} // anonymous

tcu::TestCaseGroup* createRenderPassSubpassDependencyTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "subpass_dependencies", "Subpass dependency tests", initTests, RENDERING_TYPE_RENDERPASS_LEGACY);
}

tcu::TestCaseGroup* createRenderPass2SubpassDependencyTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "subpass_dependencies", "Subpass dependency tests", initTests, RENDERING_TYPE_RENDERPASS2);
}
} // vkt
