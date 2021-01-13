/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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

#include "vktRenderPassSparseRenderTargetTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

using namespace vk;

using tcu::UVec4;
using tcu::Vec4;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;

using tcu::TestLog;

using std::string;
using std::vector;

namespace vkt
{
namespace
{
using namespace renderpass;

de::MovePtr<Allocation> createBufferMemory (const DeviceInterface&	vk,
											VkDevice				device,
											Allocator&				allocator,
											VkBuffer				buffer)
{
	de::MovePtr<Allocation> allocation (allocator.allocate(getBufferMemoryRequirements(vk, device, buffer), MemoryRequirement::HostVisible));
	VK_CHECK(vk.bindBufferMemory(device, buffer, allocation->getMemory(), allocation->getOffset()));
	return allocation;
}

Move<VkImage> createSparseImageAndMemory (const DeviceInterface&				vk,
										  VkDevice								device,
										  const VkPhysicalDevice				physicalDevice,
										  const InstanceInterface&				instance,
										  Allocator&							allocator,
										  vector<de::SharedPtr<Allocation> >&	allocations,
										  deUint32								universalQueueFamilyIndex,
										  VkQueue								sparseQueue,
										  deUint32								sparseQueueFamilyIndex,
										  const VkSemaphore&					bindSemaphore,
										  VkFormat								format,
										  deUint32								width,
										  deUint32								height)
{
	deUint32				queueFamilyIndices[]	= {universalQueueFamilyIndex, sparseQueueFamilyIndex};
	const VkSharingMode		sharingMode             = universalQueueFamilyIndex != sparseQueueFamilyIndex ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;

	const VkExtent3D		imageExtent				=
	{
		width,
		height,
		1u
	};

	const VkImageCreateInfo	imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,
		VK_IMAGE_TYPE_2D,
		format,
		imageExtent,
		1u,
		1u,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		sharingMode,
		sharingMode == VK_SHARING_MODE_CONCURRENT ? 2u : 1u,
		queueFamilyIndices,
		VK_IMAGE_LAYOUT_UNDEFINED
	};

	if (!checkSparseImageFormatSupport(physicalDevice, instance, imageCreateInfo))
		TCU_THROW(NotSupportedError, "The image format does not support sparse operations");

	Move<VkImage> destImage = createImage(vk, device, &imageCreateInfo);
	allocateAndBindSparseImage(vk, device, physicalDevice, instance, imageCreateInfo, bindSemaphore, sparseQueue, allocator, allocations, mapVkFormat(format), *destImage);

	return destImage;
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
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		flags,
		image,
		viewType,
		format,
		components,
		subresourceRange,
	};

	return createImageView(vk, device, &pCreateInfo);
}

Move<VkImageView> createImageView (const DeviceInterface&	vkd,
								   VkDevice					device,
								   VkImage					image,
								   VkFormat					format,
								   VkImageAspectFlags		aspect)
{
	const VkImageSubresourceRange range =
	{
		aspect,
		0u,
		1u,
		0u,
		1u
	};

	return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
}

Move<VkBuffer> createBuffer (const DeviceInterface&		vkd,
							 VkDevice					device,
							 VkFormat					format,
							 deUint32					width,
							 deUint32					height)
{
	const VkBufferUsageFlags	bufferUsage			(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkDeviceSize			pixelSize			= mapVkFormat(format).getPixelSize();
	const VkBufferCreateInfo	createInfo			=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		0u,

		width * height * pixelSize,
		bufferUsage,

		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		DE_NULL
	};

	return createBuffer(vkd, device, &createInfo);
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass (const DeviceInterface&	vkd,
									 VkDevice				device,
									 VkFormat				dstFormat)
{
	const AttachmentRef		dstAttachmentRef		//  VkAttachmentReference										||  VkAttachmentReference2KHR
	(
													//																||  VkStructureType						sType;
		DE_NULL,									//																||  const void*							pNext;
		0u,											//  deUint32						attachment;					||  deUint32							attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//  VkImageLayout					layout;						||  VkImageLayout						layout;
		0u											//																||  VkImageAspectFlags					aspectMask;
	);
	const AttachmentDesc	dstAttachment			//  VkAttachmentDescription										||  VkAttachmentDescription2KHR
	(
													//																||  VkStructureType						sType;
		DE_NULL,									//																||  const void*							pNext;
		0u,											//  VkAttachmentDescriptionFlags	flags;						||  VkAttachmentDescriptionFlags		flags;
		dstFormat,									//  VkFormat						format;						||  VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,						//  VkSampleCountFlagBits			samples;					||  VkSampleCountFlagBits				samples;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//  VkAttachmentLoadOp				loadOp;						||  VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				//  VkAttachmentStoreOp				storeOp;					||  VkAttachmentStoreOp					storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//  VkAttachmentLoadOp				stencilLoadOp;				||  VkAttachmentLoadOp					stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			//  VkAttachmentStoreOp				stencilStoreOp;				||  VkAttachmentStoreOp					stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,					//  VkImageLayout					initialLayout;				||  VkImageLayout						initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	//  VkImageLayout					finalLayout;				||  VkImageLayout						finalLayout;
	);
	const SubpassDesc			subpass				//  VkSubpassDescription										||  VkSubpassDescription2KHR
	(
													//																||  VkStructureType						sType;
		DE_NULL,									//																||  const void*							pNext;
		(VkSubpassDescriptionFlags)0,				//  VkSubpassDescriptionFlags		flags;						||  VkSubpassDescriptionFlags			flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,			//  VkPipelineBindPoint				pipelineBindPoint;			||  VkPipelineBindPoint					pipelineBindPoint;
		0u,											//																||  deUint32							viewMask;
		0u,											//  deUint32						inputAttachmentCount;		||  deUint32							inputAttachmentCount;
		DE_NULL,									//  const VkAttachmentReference*	pInputAttachments;			||  const VkAttachmentReference2KHR*	pInputAttachments;
		1u,											//  deUint32						colorAttachmentCount;		||  deUint32							colorAttachmentCount;
		&dstAttachmentRef,							//  const VkAttachmentReference*	pColorAttachments;			||  const VkAttachmentReference2KHR*	pColorAttachments;
		DE_NULL,									//  const VkAttachmentReference*	pResolveAttachments;		||  const VkAttachmentReference2KHR*	pResolveAttachments;
		DE_NULL,									//  const VkAttachmentReference*	pDepthStencilAttachment;	||  const VkAttachmentReference2KHR*	pDepthStencilAttachment;
		0u,											//  deUint32						preserveAttachmentCount;	||  deUint32							preserveAttachmentCount;
		DE_NULL										//  const deUint32*					pPreserveAttachments;		||  const deUint32*						pPreserveAttachments;
	);
	const RenderPassCreateInfo	renderPassCreator	//  VkRenderPassCreateInfo										||  VkRenderPassCreateInfo2KHR
	(
													//  VkStructureType					sType;						||  VkStructureType						sType;
		DE_NULL,									//  const void*						pNext;						||  const void*							pNext;
		(VkRenderPassCreateFlags)0u,				//  VkRenderPassCreateFlags			flags;						||  VkRenderPassCreateFlags				flags;
		1u,											//  deUint32						attachmentCount;			||  deUint32							attachmentCount;
		&dstAttachment,								//  const VkAttachmentDescription*	pAttachments;				||  const VkAttachmentDescription2KHR*	pAttachments;
		1u,											//  deUint32						subpassCount;				||  deUint32							subpassCount;
		&subpass,									//  const VkSubpassDescription*		pSubpasses;					||  const VkSubpassDescription2KHR*		pSubpasses;
		0u,											//  deUint32						dependencyCount;			||  deUint32							dependencyCount;
		DE_NULL,									//  const VkSubpassDependency*		pDependencies;				||  const VkSubpassDependency2KHR*		pDependencies;
		0u,											//																||  deUint32							correlatedViewMaskCount;
		DE_NULL										//																||  const deUint32*						pCorrelatedViewMasks;
	);

	return renderPassCreator.createRenderPass(vkd, device);
}

Move<VkRenderPass> createRenderPass (const DeviceInterface&	vkd,
									 VkDevice				device,
									 VkFormat				dstFormat,
									 const RenderPassType	renderPassType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vkd, device, dstFormat);
		case RENDERPASS_TYPE_RENDERPASS2:
			return createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vkd, device, dstFormat);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

Move<VkFramebuffer> createFramebuffer (const DeviceInterface&	vkd,
									   VkDevice					device,
									   VkRenderPass				renderPass,
									   VkImageView				dstImageView,
									   deUint32					width,
									   deUint32					height)
{
	const VkFramebufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		0u,

		renderPass,
		1u,
		&dstImageView,

		width,
		height,
		1u
	};

	return createFramebuffer(vkd, device, &createInfo);
}

Move<VkPipelineLayout> createRenderPipelineLayout (const DeviceInterface&	vkd,
												   VkDevice					device)
{
	const VkPipelineLayoutCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,

		0u,
		DE_NULL,

		0u,
		DE_NULL
	};

	return createPipelineLayout(vkd, device, &createInfo);
}

Move<VkPipeline> createRenderPipeline (const DeviceInterface&							vkd,
									   VkDevice											device,
									   VkRenderPass										renderPass,
									   VkPipelineLayout									pipelineLayout,
									   const BinaryCollection&							binaryCollection,
									   deUint32											width,
									   deUint32											height)
{
	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u));
	const Unique<VkShaderModule>					fragmentShaderModule			(createShaderModule(vkd, device, binaryCollection.get("quad-frag"), 0u));

	const VkPipelineVertexInputStateCreateInfo		vertexInputState				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineVertexInputStateCreateFlags)0u,

		0u,
		DE_NULL,

		0u,
		DE_NULL
	};

	const std::vector<VkViewport>					viewports						(1, makeViewport(tcu::UVec2(width, height)));
	const std::vector<VkRect2D>						scissors						(1, makeRect2D(tcu::UVec2(width, height)));

	return makeGraphicsPipeline(vkd,									// const DeviceInterface&                        vk
								device,									// const VkDevice                                device
								pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
								*vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,								// const VkShaderModule                          geometryShaderModule
								*fragmentShaderModule,					// const VkShaderModule                          fragmentShaderModule
								renderPass,								// const VkRenderPass                            renderPass
								viewports,								// const std::vector<VkViewport>&                viewports
								scissors,								// const std::vector<VkRect2D>&                  scissors
								VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
								0u,										// const deUint32                                subpass
								0u,										// const deUint32                                patchControlPoints
								&vertexInputState);						// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
}

struct TestConfig
{
				TestConfig		(VkFormat		format_,
								 RenderPassType	renderPassType_)
		: format			(format_)
		, renderPassType	(renderPassType_)
	{
	}

	VkFormat		format;
	RenderPassType	renderPassType;
};

class SparseRenderTargetTestInstance : public TestInstance
{
public:
											SparseRenderTargetTestInstance	(Context& context, TestConfig testConfig);
											~SparseRenderTargetTestInstance	(void);

	tcu::TestStatus							iterate					(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus							iterateInternal			(void);

private:
	const bool								m_extensionSupported;
	const RenderPassType					m_renderPassType;

	const deUint32							m_width;
	const deUint32							m_height;
	const VkFormat							m_format;

	vector<de::SharedPtr<Allocation> >		m_allocations;

	const Unique<VkSemaphore>				m_bindSemaphore;

	const Unique<VkImage>					m_dstImage;
	const Unique<VkImageView>				m_dstImageView;

	const Unique<VkBuffer>					m_dstBuffer;
	const de::UniquePtr<Allocation>			m_dstBufferMemory;

	const Unique<VkRenderPass>				m_renderPass;
	const Unique<VkFramebuffer>				m_framebuffer;

	const Unique<VkPipelineLayout>			m_renderPipelineLayout;
	const Unique<VkPipeline>				m_renderPipeline;

	const Unique<VkCommandPool>				m_commandPool;
	tcu::ResultCollector					m_resultCollector;
};

SparseRenderTargetTestInstance::SparseRenderTargetTestInstance (Context& context, TestConfig testConfig)
	: TestInstance				(context)
	, m_extensionSupported		((testConfig.renderPassType == RENDERPASS_TYPE_RENDERPASS2) && context.requireDeviceFunctionality("VK_KHR_create_renderpass2"))
	, m_renderPassType			(testConfig.renderPassType)
	, m_width					(32u)
	, m_height					(32u)
	, m_format					(testConfig.format)
	, m_bindSemaphore			(createSemaphore(context.getDeviceInterface(), context.getDevice()))
	, m_dstImage				(createSparseImageAndMemory(context.getDeviceInterface(), context.getDevice(), context.getPhysicalDevice(), context.getInstanceInterface(), context.getDefaultAllocator(), m_allocations, context.getUniversalQueueFamilyIndex(), context.getSparseQueue(), context.getSparseQueueFamilyIndex(), *m_bindSemaphore, m_format, m_width, m_height))
	, m_dstImageView			(createImageView(context.getDeviceInterface(), context.getDevice(), *m_dstImage, m_format, VK_IMAGE_ASPECT_COLOR_BIT))
	, m_dstBuffer				(createBuffer(context.getDeviceInterface(), context.getDevice(), m_format, m_width, m_height))
	, m_dstBufferMemory			(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_dstBuffer))
	, m_renderPass				(createRenderPass(context.getDeviceInterface(), context.getDevice(), m_format, testConfig.renderPassType))
	, m_framebuffer				(createFramebuffer(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_dstImageView, m_width, m_height))
	, m_renderPipelineLayout	(createRenderPipelineLayout(context.getDeviceInterface(), context.getDevice()))
	, m_renderPipeline			(createRenderPipeline(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_renderPipelineLayout, context.getBinaryCollection(), m_width, m_height))
	, m_commandPool				(createCommandPool(context.getDeviceInterface(), context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
}

SparseRenderTargetTestInstance::~SparseRenderTargetTestInstance (void)
{
}

tcu::TestStatus SparseRenderTargetTestInstance::iterate (void)
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
tcu::TestStatus SparseRenderTargetTestInstance::iterateInternal (void)

{
	const DeviceInterface&								vkd					(m_context.getDeviceInterface());
	const Unique<VkCommandBuffer>						commandBuffer		(allocateCommandBuffer(vkd, m_context.getDevice(), *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo		(DE_NULL);

	beginCommandBuffer(vkd, *commandBuffer);

	{
		const VkRenderPassBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			DE_NULL,

			*m_renderPass,
			*m_framebuffer,

			{
				{ 0u, 0u },
				{ m_width, m_height }
			},

			0u,
			DE_NULL
		};
		RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
	}

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
	vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
	RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	copyImageToBuffer(vkd, *commandBuffer, *m_dstImage, *m_dstBuffer, tcu::IVec2(m_width, m_height));

	endCommandBuffer(vkd, *commandBuffer);

	submitCommandsAndWait(vkd, m_context.getDevice(), m_context.getUniversalQueue(), *commandBuffer);

	{
		const tcu::TextureFormat			format			(mapVkFormat(m_format));
		const void* const					ptr				(m_dstBufferMemory->getHostPtr());
		const tcu::ConstPixelBufferAccess	access			(format, m_width, m_height, 1, ptr);
		tcu::TextureLevel					reference		(format, m_width, m_height);
		const tcu::TextureChannelClass		channelClass	(tcu::getTextureChannelClass(format.type));

		switch (channelClass)
		{
			case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			{
				const UVec4	bits	(tcu::getTextureFormatBitDepth(format).cast<deUint32>());
				const UVec4	color	(1u << (bits.x()-1), 1u << (bits.y()-2), 1u << (bits.z()-3), 0xffffffff);

				for (deUint32 y = 0; y < m_height; y++)
				for (deUint32 x = 0; x < m_width; x++)
				{
					reference.getAccess().setPixel(color, x, y);
				}

				if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "", "", reference.getAccess(), access, UVec4(0u), tcu::COMPARE_LOG_ON_ERROR))
					m_resultCollector.fail("Compare failed.");
			}
			break;

			case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			{
				const UVec4	bits	(tcu::getTextureFormatBitDepth(format).cast<deUint32>());
				const UVec4	color	(1u << (bits.x()-2), 1u << (bits.y()-3), 1u << (bits.z()-4), 0xffffffff);

				for (deUint32 y = 0; y < m_height; y++)
				for (deUint32 x = 0; x < m_width; x++)
				{
					reference.getAccess().setPixel(color, x, y);
				}

				if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "", "", reference.getAccess(), access, UVec4(0u), tcu::COMPARE_LOG_ON_ERROR))
					m_resultCollector.fail("Compare failed.");
			}
			break;

			case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
			case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
			{
				const tcu::TextureFormatInfo	info		(tcu::getTextureFormatInfo(format));
				const Vec4						maxValue	(info.valueMax);
				const Vec4						color		(maxValue.x() / 2.0f, maxValue.y() / 4.0f, maxValue.z() / 8.0f, maxValue.w());

				for (deUint32 y = 0; y < m_height; y++)
				for (deUint32 x = 0; x < m_width; x++)
				{
					if (tcu::isSRGB(format))
						reference.getAccess().setPixel(tcu::linearToSRGB(color), x, y);
					else
						reference.getAccess().setPixel(color, x, y);
				}

				{
					// Allow error of 4 times the minimum presentable difference
					const Vec4 threshold (4.0f * 1.0f / ((UVec4(1u) << tcu::getTextureFormatMantissaBitDepth(format).cast<deUint32>()) - 1u).cast<float>());

					if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "", "", reference.getAccess(), access, threshold, tcu::COMPARE_LOG_ON_ERROR))
						m_resultCollector.fail("Compare failed.");
				}
			}
			break;

			case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			{
				const Vec4 color(0.5f, 0.25f, 0.125f, 1.0f);

				for (deUint32 y = 0; y < m_height; y++)
				for (deUint32 x = 0; x < m_width; x++)
				{
					if (tcu::isSRGB(format))
						reference.getAccess().setPixel(tcu::linearToSRGB(color), x, y);
					else
						reference.getAccess().setPixel(color, x, y);
				}

				{
					// Convert target format ulps to float ulps and allow 64ulp differences
					const UVec4 threshold (64u * (UVec4(1u) << (UVec4(23) - tcu::getTextureFormatMantissaBitDepth(format).cast<deUint32>())));

					if (!tcu::floatUlpThresholdCompare(m_context.getTestContext().getLog(), "", "", reference.getAccess(), access, threshold, tcu::COMPARE_LOG_ON_ERROR))
						m_resultCollector.fail("Compare failed.");
				}
			}
			break;

			default:
				DE_FATAL("Unknown channel class");
		}
	}

	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

struct Programs
{
	void init (vk::SourceCollections& dst, TestConfig testConfig) const
	{
		std::ostringstream				fragmentShader;
		const VkFormat					format			(testConfig.format);
		const tcu::TextureFormat		texFormat		(mapVkFormat(format));
		const UVec4						bits			(tcu::getTextureFormatBitDepth(texFormat).cast<deUint32>());
		const tcu::TextureChannelClass	channelClass	(tcu::getTextureChannelClass(texFormat.type));

		dst.glslSources.add("quad-vert") << glu::VertexSource(
			"#version 450\n"
			"out gl_PerVertex {\n"
			"\tvec4 gl_Position;\n"
			"};\n"
			"highp float;\n"
			"void main (void)\n"
			"{\n"
			"    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
			"                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
			"}\n");

		switch (channelClass)
		{
			case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			{
				fragmentShader <<
					"#version 450\n"
					"layout(location = 0) out highp uvec4 o_color;\n"
					"void main (void)\n"
					"{\n"
					"    o_color = uvec4(" << de::toString(1u << (bits.x()-1)) << ", " << de::toString(1u << (bits.y()-2)) << ", " << de::toString(1u << (bits.z()-3)) << ", 0xffffffff);"
					"}\n";
			}
			break;

			case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			{
				fragmentShader <<
					"#version 450\n"
					"layout(location = 0) out highp ivec4 o_color;\n"
					"void main (void)\n"
					"{\n"
					"    o_color = ivec4(" << de::toString(1u << (bits.x()-2)) << ", " << de::toString(1u << (bits.y()-3)) << ", " << de::toString(1u << (bits.z()-4)) << ", 0xffffffff);"
					"}\n";
			}
			break;

			default:
			{
				fragmentShader <<
					"#version 450\n"
					"layout(location = 0) out highp vec4 o_color;\n"
					"void main (void)\n"
					"{\n"
					"    o_color = vec4(0.5, 0.25, 0.125, 1.0);\n"
					"}\n";
			}
			break;
		};

		dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
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
	static const VkFormat	formats[]	=
	{
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT
	};

	tcu::TestContext&		testCtx		(group->getTestContext());

	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
	{
		const VkFormat		format		(formats[formatNdx]);
		const TestConfig	testConfig	(format, renderPassType);
		string				testName	(formatToName(format));

		group->addChild(new InstanceFactory1<SparseRenderTargetTestInstance, TestConfig, Programs>(testCtx, tcu::NODETYPE_SELF_VALIDATE, testName.c_str(), testName.c_str(), testConfig));
	}
}

} // anonymous

tcu::TestCaseGroup* createRenderPassSparseRenderTargetTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "sparserendertarget", "Sparse render target tests", initTests, RENDERPASS_TYPE_LEGACY);
}

tcu::TestCaseGroup* createRenderPass2SparseRenderTargetTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "sparserendertarget", "Sparse render target tests", initTests, RENDERPASS_TYPE_RENDERPASS2);
}
} // vkt
