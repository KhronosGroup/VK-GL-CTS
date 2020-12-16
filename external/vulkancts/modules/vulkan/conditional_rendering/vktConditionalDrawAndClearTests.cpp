/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2018 The Khronos Group Inc.
* Copyright (c) 2018 Intel Corporation
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
* \brief VK_EXT_conditional_rendering extension tests.
*//*--------------------------------------------------------------------*/

#include "vktConditionalDrawAndClearTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDrawBaseClass.hpp"
#include "vktDrawTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include <bitset>

namespace vkt
{
namespace conditional
{
namespace
{

using namespace vk;
using namespace Draw;

struct ClearTestParams
{
	bool	m_discard;
	bool	m_invert;
	bool	m_testDepth;
	bool	m_partialClear;
	bool	m_useOffset;
	bool	m_clearAttachmentTwice;
};

const ClearTestParams clearColorTestGrid[] =
{
	{ false,	false,		false,		false,		false,		false },
	{ true,		false,		false,		false,		false,		false },
	{ false,	true,		false,		false,		false,		false },
	{ true,		true,		false,		false,		false,		false },
	{ false,	false,		false,		true,		false,		false },
	{ true,		false,		false,		true,		false,		false },
	{ false,	true,		false,		true,		false,		false },
	{ true,		true,		false,		true,		false,		false },
	{ false,	false,		false,		true,		true,		false },
	{ true,		false,		false,		true,		true,		false },
	{ false,	true,		false,		true,		true,		false },
	{ true,		true,		false,		true,		true,		false },
	{ true,		true,		false,		false,		true,		false },
};

const ClearTestParams clearDepthTestGrid[] =
{
	{ false,	false,		true,		false,		false,		false },
	{ true,		false,		true,		false,		false,		false },
	{ false,	 true,		true,		false,		false,		false },
	{ true,		true,		true,		false,		false,		false },
	{ false,	false,		true,		true,		false,		false },
	{ true,		false,		true,		true,		false,		false },
	{ false,	true,		true,		true,		false,		false },
	{ true,		true,		true,		true,		false,		false },
	{ false,	false,		true,		true,		true,		false },
	{ true,		false,		true,		true,		true,		false },
	{ false,	true,		true,		true,		true,		false },
	{ true,		true,		true,		true,		true,		false },
};

const ClearTestParams clearColorTwiceGrid[] =
{
	{ false,	false,		false,		false,		false,		true },
	{ true,		false,		false,		false,		false,		true },
	{ false,	true,		false,		false,		false,		true },
	{ true,		true,		false,		false,		false,		true },
	{ false,	true,		false,		true,		true,		true },
	{ true,		true,		false,		true,		true,		true }
};

const ClearTestParams clearDepthTwiceGrid[] =
{
	{ false,	false,		true,		false,		false,		true },
	{ true,		false,		true,		false,		false,		true },
	{ false,	true,		true,		false,		false,		true },
	{ true,		true,		true,		false,		false,		true },
	{ false,	true,		true,		true,		true,		true },
	{ true,		true,		true,		true,		true,		true }
};

enum TogglePredicateMode { FILL, COPY, NONE };

struct DrawTestParams
{
	bool						m_discard; //controls the setting of the predicate for conditional rendering.Initial state, may be toggled later depending on the m_togglePredicate setting.
	bool						m_invert;
	bool						m_useOffset;
	deUint32					m_beginSequenceBits; //bits 0..3 control BEFORE which of the 4 draw calls the vkCmdBeginConditionalRenderingEXT call is executed. Least significant bit corresponds to the first draw call.
	deUint32					m_endSequenceBits; //bits 0..3 control AFTER which of the 4 draw calls the vkCmdEndConditionalRenderingEXT call is executed. Least significant bit corresponds to the first draw call.
	deUint32					m_resultBits; //used for reference image preparation.
	bool						m_togglePredicate; //if true, toggle the predicate setting before rendering.
	TogglePredicateMode			m_toggleMode; //method of the predicate toggling
};

enum
{
	b0000 = 0x0,
	b0001 = 0x1,
	b0010 = 0x2,
	b0011 = 0x3,
	b0100 = 0x4,
	b0101 = 0x5,
	b0110 = 0x6,
	b0111 = 0x7,
	b1000 = 0x8,
	b1001 = 0x9,
	b1010 = 0xA,
	b1011 = 0xB,
	b1100 = 0xC,
	b1101 = 0xD,
	b1110 = 0xE,
	b1111 = 0xF,
};

const DrawTestParams drawTestGrid[] =
{
	{ false,	false,	false,	b0001, b1000, b1111, false,	NONE },
	{ true,		false,	false,	b0001, b1000, b0000, false,	NONE },
	{ true,		false,	false,	b0001, b0001, b1110, false,	NONE },
	{ true,		false,	false,	b1111, b1111, b0000, false,	NONE },
	{ true,		false,	false,	b0010, b0010, b1101, false,	NONE },
	{ true,		true,	false,	b1010, b1010, b0101, false,	NONE },
	{ false,	true,	true,	b1010, b1010, b1111, false,	NONE },
	{ true,		true,	true,	b0010, b1000, b0001, false,	NONE },
	{ true,		true,	true,	b1001, b1001, b0110, false,	NONE },
	{ true,		true,	true,	b0010, b1000, b1111, true,	FILL },
	{ true,		true,	true,	b1001, b1001, b1111, true,	FILL },
	{ false,	true,	true,	b1001, b1001, b0110, true,	FILL },
	{ true,		true,	true,	b0010, b1000, b1111, true,	COPY },
	{ true,		true,	true,	b1001, b1001, b1111, true,	COPY },
	{ false,	true,	true,	b1001, b1001, b0110, true,	COPY },
};

std::string generateClearTestName(const ClearTestParams& clearTestParams)
{
	std::string		name			=	(clearTestParams.m_discard		? "discard_"	:	"no_discard_");
	name			+=	(clearTestParams.m_invert		? "invert_"		:	"no_invert_");
	name			+=	(clearTestParams.m_partialClear	? "partial_"	:	"full_");
	name			+=	(clearTestParams.m_useOffset	? "offset"		:	"no_offset");
	return name;
}

inline deUint32 getBit(deUint32 src, int ndx)
{
	return (src >> ndx) & 1;
}

inline bool isBitSet(deUint32 src, int ndx)
{
	return getBit(src, ndx) != 0;
}

class ConditionalRenderingBaseTestInstance : public TestInstance
{
public:
									ConditionalRenderingBaseTestInstance					(Context& context);
protected:
	virtual tcu::TestStatus			iterate													(void) = 0;
	void							createInitBufferWithPredicate							(bool discard, bool invert, deUint32 offsetMultiplier, VkBufferUsageFlagBits extraUsage);
	void							createTargetColorImageAndImageView						(void);
	void							createTargetDepthImageAndImageView						(void);
	void							createRenderPass										(VkFormat format, VkImageLayout layout);
	void							createFramebuffer										(VkImageView imageView);
	void							clearWithClearColorImage								(const VkClearColorValue& color);
	void							clearWithClearDepthStencilImage							(const VkClearDepthStencilValue& value);
	void							clearColorWithClearAttachments							(const VkClearColorValue& color, bool partial);
	void							clearDepthWithClearAttachments							(const VkClearDepthStencilValue& depthStencil, bool partial);
	void							createResultBuffer										(VkFormat format);
	void							createVertexBuffer										(void);
	void							createPipelineLayout									(void);
	void							createAndUpdateDescriptorSet							(void);
	void							createPipeline											(void);
	void							copyResultImageToBuffer									(VkImageAspectFlags imageAspectFlags, VkImage image);
	void							draw													(void);
	void							imageMemoryBarrier										(VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout,
																							VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageAspectFlags imageAspectFlags);
	void							bufferMemoryBarrier										(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
																							VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);
	void							prepareReferenceImageOneColor							(tcu::PixelBufferAccess& reference, const VkClearColorValue& clearColor);
	void							prepareReferenceImageOneColor							(tcu::PixelBufferAccess& reference, const tcu::Vec4& color);
	void							prepareReferenceImageOneDepth							(tcu::PixelBufferAccess& reference, const VkClearDepthStencilValue& clearValue);
	void							prepareReferenceImageDepthClearPartial					(tcu::PixelBufferAccess& reference, const VkClearDepthStencilValue& clearValueInitial, const VkClearDepthStencilValue& clearValueFinal);
	void							prepareReferenceImageColorClearPartial					(tcu::PixelBufferAccess& reference, const VkClearColorValue& clearColorInitial, const VkClearColorValue& clearColorFinal);

	const InstanceInterface&		m_vki;
	const DeviceInterface&			m_vkd;
	const VkDevice					m_device;
	const VkPhysicalDevice			m_physicalDevice;
	const VkQueue					m_queue;
	de::SharedPtr<Buffer>			m_conditionalRenderingBuffer;
	de::SharedPtr<Buffer>			m_resultBuffer;
	de::SharedPtr<Buffer>			m_vertexBuffer;
	de::SharedPtr<Image>			m_colorTargetImage;
	de::SharedPtr<Image>			m_depthTargetImage;
	Move<VkImageView>				m_colorTargetView;
	Move<VkImageView>				m_depthTargetView;
	Move<VkRenderPass>				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBufferPrimary;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkDescriptorSet>			m_descriptorSet;
	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkShaderModule>			m_vertexShaderModule;
	Move<VkShaderModule>			m_fragmentShaderModule;
	Move<VkPipeline>				m_pipeline;
	VkDeviceSize					m_conditionalRenderingBufferOffset;

	enum
	{
		WIDTH = 256,
		HEIGHT = 256
	};
};

class ConditionalRenderingClearAttachmentsTestInstance : public ConditionalRenderingBaseTestInstance
{
public:
									ConditionalRenderingClearAttachmentsTestInstance		(Context& context, const ClearTestParams& testParams);
protected:
	virtual tcu::TestStatus			iterate													(void);
	ClearTestParams					m_testParams;
};

class ConditionalRenderingDrawTestInstance : public ConditionalRenderingBaseTestInstance
{
public:
									ConditionalRenderingDrawTestInstance					(Context& context, const DrawTestParams& testParams);
protected:
	//Execute 4 draw calls, each can be drawn with or without conditional rendering. Each draw call renders to the different part of an image - this is achieved by
	//using push constant and 'discard' in the fragment shader. This way it is possible to tell which of the rendering command were discarded by the conditional rendering mechanism.
	virtual tcu::TestStatus			iterate													(void);
	void							createPipelineLayout									(void);
	void							prepareReferenceImage									(tcu::PixelBufferAccess& reference, const VkClearColorValue& clearColor, deUint32 resultBits);

	DrawTestParams					m_testParams;
	de::SharedPtr<Buffer>			m_conditionalRenderingBufferForCopy;
};

class ConditionalRenderingUpdateBufferWithDrawTestInstance : public ConditionalRenderingBaseTestInstance
{
public:
									ConditionalRenderingUpdateBufferWithDrawTestInstance	(Context& context, bool testParams);
protected:
	virtual tcu::TestStatus			iterate													(void);
	void							createAndUpdateDescriptorSets							(void);
	void							createPipelines											(void);
	void							createRenderPass										(VkFormat format, VkImageLayout layout);
	Move<VkDescriptorSet>			m_descriptorSetUpdate;
	Move<VkShaderModule>			m_vertexShaderModuleDraw;
	Move<VkShaderModule>			m_fragmentShaderModuleDraw;
	Move<VkShaderModule>			m_vertexShaderModuleUpdate;
	Move<VkShaderModule>			m_fragmentShaderModuleDiscard;
	Move<VkPipeline>				m_pipelineDraw;
	Move<VkPipeline>				m_pipelineUpdate;
	bool							m_testParams;
};

ConditionalRenderingBaseTestInstance::ConditionalRenderingBaseTestInstance (Context& context)
	: TestInstance					(context)
	, m_vki							(m_context.getInstanceInterface())
	, m_vkd							(m_context.getDeviceInterface())
	, m_device						(m_context.getDevice())
	, m_physicalDevice				(m_context.getPhysicalDevice())
	, m_queue						(m_context.getUniversalQueue())
{
}

void ConditionalRenderingBaseTestInstance::createInitBufferWithPredicate (bool discard, bool invert, deUint32 offsetMultiplier = 0, VkBufferUsageFlagBits extraUsage = (VkBufferUsageFlagBits)0)
{
	m_conditionalRenderingBufferOffset		= sizeof(deUint32) * offsetMultiplier;

	const VkDeviceSize						dataSize										= sizeof(deUint32) + m_conditionalRenderingBufferOffset;
	deUint32								predicate										= discard ? invert : !invert;

	m_conditionalRenderingBuffer			= Buffer::createAndAlloc(m_vkd, m_device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT | extraUsage), m_context.getDefaultAllocator(),
																									   MemoryRequirement::HostVisible);

	void *									conditionalRenderingBufferDataPointer			= static_cast<char*>(m_conditionalRenderingBuffer->getBoundMemory().getHostPtr()) + m_conditionalRenderingBufferOffset;

	deMemcpy(conditionalRenderingBufferDataPointer, &predicate, static_cast<size_t>(sizeof(deUint32)));
	flushMappedMemoryRange(m_vkd, m_device, m_conditionalRenderingBuffer->getBoundMemory().getMemory(), m_conditionalRenderingBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
}

void ConditionalRenderingBaseTestInstance::createTargetColorImageAndImageView (void)
{
	const VkExtent3D			targetImageExtent		= { WIDTH, HEIGHT, 1 };

	const ImageCreateInfo		targetImageCreateInfo(	VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, targetImageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
														VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	m_colorTargetImage			= Image::createAndAlloc(m_vkd, m_device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	const ImageViewCreateInfo	colorTargetViewInfo(m_colorTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM);

	m_colorTargetView			= createImageView(m_vkd, m_device, &colorTargetViewInfo);
}

void ConditionalRenderingBaseTestInstance::createTargetDepthImageAndImageView (void)
{
	const VkExtent3D			targetImageExtent		= { WIDTH, HEIGHT, 1 };

	const ImageCreateInfo		targetImageCreateInfo(VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT, targetImageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
													  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	m_depthTargetImage			= Image::createAndAlloc(m_vkd, m_device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	const ImageViewCreateInfo	depthTargetViewInfo(m_depthTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT);

	m_depthTargetView			= createImageView(m_vkd, m_device, &depthTargetViewInfo);
}

void ConditionalRenderingBaseTestInstance::createRenderPass (VkFormat format, VkImageLayout layout)
{
	RenderPassCreateInfo			renderPassCreateInfo;

	renderPassCreateInfo.addAttachment(AttachmentDescription(format,
															 VK_SAMPLE_COUNT_1_BIT,
															 VK_ATTACHMENT_LOAD_OP_LOAD,
															 VK_ATTACHMENT_STORE_OP_STORE,
															 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
															 VK_ATTACHMENT_STORE_OP_STORE,
															 isDepthStencilFormat(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
															 isDepthStencilFormat(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

	const VkAttachmentReference		attachmentReference =
	{
		0u,														// deUint32				attachment
		layout													// VkImageLayout		layout
	};

	renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
													   0,
													   0,
													   DE_NULL,
													   isDepthStencilFormat(format) ? 0 : 1,
													   isDepthStencilFormat(format) ? DE_NULL : &attachmentReference,
													   DE_NULL,
													   isDepthStencilFormat(format) ? attachmentReference : AttachmentReference(),
													   0,
													   DE_NULL));

	m_renderPass					= vk::createRenderPass(m_vkd, m_device, &renderPassCreateInfo);
}

void ConditionalRenderingBaseTestInstance::createFramebuffer (VkImageView imageView)
{
	const VkFramebufferCreateInfo	framebufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType
		DE_NULL,											// const void*					pNext
		(VkFramebufferCreateFlags)0,						// VkFramebufferCreateFlags		flags;
		*m_renderPass,										// VkRenderPass					renderPass
		1,													// deUint32						attachmentCount
		&imageView,											// const VkImageView*			pAttachments
		WIDTH,												// deUint32						width
		HEIGHT,												// deUint32						height
		1													// deUint32						layers
	};
	m_framebuffer					= vk::createFramebuffer(m_vkd, m_device, &framebufferCreateInfo);
}

void ConditionalRenderingBaseTestInstance::imageMemoryBarrier (	VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout,
																VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageAspectFlags imageAspectFlags)
{
	const struct VkImageSubresourceRange	subRangeColor =
	{
		imageAspectFlags,								// VkImageAspectFlags		aspectMask
		0u,												// deUint32					baseMipLevel
		1u,												// deUint32					mipLevels
		0u,												// deUint32					baseArrayLayer
		1u,												// deUint32					arraySize
	};
	const VkImageMemoryBarrier				imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType
		DE_NULL,										// const void*				pNext
		srcAccessMask,									// VkAccessFlags			srcAccessMask
		dstAccessMask,									// VkAccessFlags			dstAccessMask
		oldLayout,										// VkImageLayout			oldLayout
		newLayout,										// VkImageLayout			newLayout
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex
		image,											// VkImage					image
		subRangeColor									// VkImageSubresourceRange	subresourceRange
	};

	m_vkd.cmdPipelineBarrier(*m_cmdBufferPrimary, srcStageMask, dstStageMask, DE_FALSE, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);
}

void ConditionalRenderingBaseTestInstance::bufferMemoryBarrier (VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
																VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	const VkBufferMemoryBarrier bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		//VkStructureType			sType;
		DE_NULL,										//const void*				pNext;
		srcAccessMask,									//VkAccessFlags				srcAccessMask;
		dstAccessMask,									//VkAccessFlags				dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						//uint32_t					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						//uint32_t					dstQueueFamilyIndex;
		buffer,											//VkBuffer					buffer;
		offset,											//VkDeviceSize				offset;
		size											//VkDeviceSize				size;
	};

	m_vkd.cmdPipelineBarrier(*m_cmdBufferPrimary, srcStageMask, dstStageMask, DE_FALSE, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
}

void ConditionalRenderingBaseTestInstance::prepareReferenceImageOneColor (tcu::PixelBufferAccess& reference, const VkClearColorValue& clearColor)
{
	for (int w = 0; w < WIDTH; ++w)
		for (int h = 0; h < HEIGHT; ++h)
			reference.setPixel(tcu::Vec4(clearColor.float32[0], clearColor.float32[1], clearColor.float32[2], clearColor.float32[3]), w, h);
}

void ConditionalRenderingBaseTestInstance::prepareReferenceImageOneColor (tcu::PixelBufferAccess& reference, const tcu::Vec4& color)
{
	for (int w = 0; w < WIDTH; ++w)
		for (int h = 0; h < HEIGHT; ++h)
			reference.setPixel(tcu::Vec4(color), w, h);
}

void ConditionalRenderingBaseTestInstance::prepareReferenceImageOneDepth (tcu::PixelBufferAccess& reference, const VkClearDepthStencilValue& clearValue)
{
	for (int w = 0; w < WIDTH; ++w)
		for (int h = 0; h < HEIGHT; ++h)
			reference.setPixDepth(clearValue.depth, w, h);
}

void ConditionalRenderingBaseTestInstance::prepareReferenceImageDepthClearPartial (tcu::PixelBufferAccess& reference, const VkClearDepthStencilValue& clearValueInitial, const VkClearDepthStencilValue& clearValueFinal)
{
	for (int w = 0; w < WIDTH; ++w)
		for (int h = 0; h < HEIGHT; ++h)
		{
			if
				(w >= (WIDTH / 2) && h >= (HEIGHT / 2)) reference.setPixDepth(clearValueFinal.depth, w, h);
			else
				reference.setPixDepth(clearValueInitial.depth, w, h);
		}
}

void ConditionalRenderingBaseTestInstance::prepareReferenceImageColorClearPartial (tcu::PixelBufferAccess& reference, const VkClearColorValue& clearColorInitial, const VkClearColorValue& clearColorFinal)
{
	for (int w = 0; w < WIDTH; ++w)
		for (int h = 0; h < HEIGHT; ++h)
		{
			if
				(w >= (WIDTH / 2) && h >= (HEIGHT / 2)) reference.setPixel(tcu::Vec4(clearColorFinal.float32[0], clearColorFinal.float32[1], clearColorFinal.float32[2], clearColorFinal.float32[3]), w, h);
			else
				reference.setPixel(tcu::Vec4(clearColorInitial.float32[0], clearColorInitial.float32[1], clearColorInitial.float32[2], clearColorInitial.float32[3]), w, h);
		}
}

void ConditionalRenderingBaseTestInstance::clearWithClearColorImage (const VkClearColorValue& color)
{
	const struct VkImageSubresourceRange	subRangeColor =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags		aspectMask
		0u,							// deUint32					baseMipLevel
		1u,							// deUint32					mipLevels
		0u,							// deUint32					baseArrayLayer
		1u,							// deUint32					arraySize
	};
	m_vkd.cmdClearColorImage(*m_cmdBufferPrimary, m_colorTargetImage->object(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &subRangeColor);
}

void ConditionalRenderingBaseTestInstance::clearWithClearDepthStencilImage (const VkClearDepthStencilValue& value)
{
	const struct VkImageSubresourceRange	subRangeColor =
	{
		VK_IMAGE_ASPECT_DEPTH_BIT,	// VkImageAspectFlags	aspectMask
		0u,							// deUint32				baseMipLevel
		1u,							// deUint32				mipLevels
		0u,							// deUint32				baseArrayLayer
		1u,							// deUint32				arraySize
	};
	m_vkd.cmdClearDepthStencilImage(*m_cmdBufferPrimary, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &subRangeColor);
}

void ConditionalRenderingBaseTestInstance::clearColorWithClearAttachments (const VkClearColorValue& color, bool partial)
{
	const VkClearAttachment		clearAttachment =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags		aspectMask;
		0u,							// deUint32					colorAttachment;
		{ color }					// VkClearValue				clearValue;
	};
	VkRect2D					renderArea			= { { 0, 0 },{ WIDTH, HEIGHT } };

	if (partial)
	{
		renderArea.offset.x = WIDTH / 2;
		renderArea.offset.y = HEIGHT / 2;
		renderArea.extent.width = WIDTH / 2;
		renderArea.extent.height = HEIGHT / 2;
	}

	const VkClearRect			clearRect =
	{
		renderArea,					// VkRect2D					rect;
		0u,							// deUint32					baseArrayLayer;
		1u							// deUint32					layerCount;
	};

	m_vkd.cmdClearAttachments(*m_cmdBufferPrimary, 1, &clearAttachment, 1, &clearRect);
}

void ConditionalRenderingBaseTestInstance::clearDepthWithClearAttachments (const VkClearDepthStencilValue& depthStencil, bool partial)
{
	const VkClearAttachment clearAttachment =
	{
		VK_IMAGE_ASPECT_DEPTH_BIT,												// VkImageAspectFlags		aspectMask;
		0u,																		// deUint32					colorAttachment;
		makeClearValueDepthStencil(depthStencil.depth, depthStencil.stencil)	// VkClearValue				clearValue;
	};
	VkRect2D				renderArea			= { { 0, 0 },{ WIDTH, HEIGHT } };

	if (partial)
	{
		renderArea.offset.x = WIDTH / 2;
		renderArea.offset.y = HEIGHT / 2;
		renderArea.extent.width = WIDTH / 2;
		renderArea.extent.height = HEIGHT / 2;
	}

	const VkClearRect		clearRect =
	{
		renderArea,																// VkRect2D					rect;
		0u,																		// deUint32					baseArrayLayer;
		1u																		// deUint32					layerCount;
	};
	m_vkd.cmdClearAttachments(*m_cmdBufferPrimary, 1, &clearAttachment, 1, &clearRect);
}

void ConditionalRenderingBaseTestInstance::createResultBuffer (VkFormat format)
{
	VkDeviceSize		size	= WIDTH * HEIGHT * mapVkFormat(format).getPixelSize();
	m_resultBuffer				= Buffer::createAndAlloc(m_vkd, m_device, BufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
}

void ConditionalRenderingBaseTestInstance::createVertexBuffer (void)
{
	float triangleData[]							= {	-1.0f,		-1.0f,		0.0f,	1.0f,
														-1.0f,		1.0f,		0.0f,	1.0f,
														1.0f,		1.0f,		0.0f,	1.0f,
														1.0f,		-1.0f,		0.0f,	1.0f };

	m_vertexBuffer									= Buffer::createAndAlloc(m_vkd, m_device, BufferCreateInfo(sizeof(triangleData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

	void * vertexBufferDataPointer					= m_vertexBuffer->getBoundMemory().getHostPtr();

	deMemcpy(vertexBufferDataPointer, triangleData, sizeof(triangleData));
	flushMappedMemoryRange(m_vkd, m_device, m_vertexBuffer->getBoundMemory().getMemory(), m_vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
}

void ConditionalRenderingBaseTestInstance::createPipelineLayout (void)
{
	const VkPipelineLayoutCreateInfo	pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
		DE_NULL,										// const void*					pNext
		(VkPipelineLayoutCreateFlags)0,					// VkPipelineLayoutCreateFlags	flags
		1u,												// deUint32						descriptorSetCount
		&(m_descriptorSetLayout.get()),					// const VkDescriptorSetLayout*	pSetLayouts
		0u,												// deUint32						pushConstantRangeCount
		DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
	};

	m_pipelineLayout					= vk::createPipelineLayout(m_vkd, m_device, &pipelineLayoutParams);
}

void ConditionalRenderingBaseTestInstance::createAndUpdateDescriptorSet (void)
{
	const VkDescriptorSetAllocateInfo	allocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,			// VkStructureType								sType
		DE_NULL,												// const void*									pNext
		*m_descriptorPool,										// VkDescriptorPool								descriptorPool
		1u,														// deUint32										setLayoutCount
		&(m_descriptorSetLayout.get())							// const VkDescriptorSetLayout*					pSetLayouts
	};

	m_descriptorSet						= allocateDescriptorSet(m_vkd, m_device, &allocInfo);
	VkDescriptorBufferInfo				descriptorInfo			= makeDescriptorBufferInfo(m_vertexBuffer->object(), (VkDeviceSize)0u, sizeof(float) * 16);

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(m_vkd, m_device);
}

void ConditionalRenderingBaseTestInstance::createPipeline (void)
{
	const std::vector<VkViewport>					viewports(1, makeViewport(tcu::UVec2(WIDTH, HEIGHT)));
	const std::vector<VkRect2D>						scissors(1, makeRect2D(tcu::UVec2(WIDTH, HEIGHT)));
	const VkPrimitiveTopology						topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,										// VkStructureType								sType
		DE_NULL,																						// const void*									pNext
		0u,																								// vkPipelineVertexInputStateCreateFlags		flags
		0u,																								// deUint32										bindingCount
		DE_NULL,																						// const VkVertexInputBindingDescription*		pVertexBindingDescriptions
		0u,																								// deUint32										attributeCount
		DE_NULL,																						// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
	};

	m_pipeline										= makeGraphicsPipeline(m_vkd,						// const DeviceInterface&						vk
																		   m_device,					// const VkDevice								device
																		   *m_pipelineLayout,			// const VkPipelineLayout						pipelineLayout
																		   *m_vertexShaderModule,		// const VkShaderModule							vertexShaderModule
																		   DE_NULL,						// const VkShaderModule							tessellationControlShaderModule
																		   DE_NULL,						// const VkShaderModule							tessellationEvalShaderModule
																		   DE_NULL,						// const VkShaderModule							geometryShaderModule
																		   *m_fragmentShaderModule,		// const VkShaderModule							fragmentShaderModule
																		   *m_renderPass,				// const VkRenderPass							renderPass
																		   viewports,					// const std::vector<VkViewport>&				viewports
																		   scissors,					// const std::vector<VkRect2D>&					scissors
																		   topology,					// const VkPrimitiveTopology					topology
																		   0u,							// const deUint32								subpass
																		   0u,							// const deUint32								patchControlPoints
																		   &vertexInputStateParams);	// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo
}

void ConditionalRenderingBaseTestInstance::copyResultImageToBuffer (VkImageAspectFlags imageAspectFlags, VkImage image)
{
	const VkBufferImageCopy region_all =
	{
		0,												// VkDeviceSize					bufferOffset
		0,												// deUint32						bufferRowLength
		0,												// deUint32						bufferImageHeight
		{ imageAspectFlags, 0, 0, 1 },					// VkImageSubresourceLayers		imageSubresource
		{ 0, 0, 0 },									// VkOffset3D					imageOffset
		{ WIDTH, HEIGHT, 1 }							// VkExtent3D					imageExtent
	};

	m_vkd.cmdCopyImageToBuffer(*m_cmdBufferPrimary, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_resultBuffer->object(), 1, &region_all);
}

void ConditionalRenderingBaseTestInstance::draw (void)
{
	m_vkd.cmdDraw(*m_cmdBufferPrimary, 4, 1, 0, 0);
}

ConditionalRenderingClearAttachmentsTestInstance::ConditionalRenderingClearAttachmentsTestInstance (Context& context, const ClearTestParams& testParams)
	: ConditionalRenderingBaseTestInstance	(context)
	, m_testParams							(testParams)
{}

tcu::TestStatus ConditionalRenderingClearAttachmentsTestInstance::iterate (void)
{
	const deUint32								queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	deUint32									offsetMultiplier		= 0;
	VkClearColorValue							clearColorInitial		= { { 0.0f, 0.0f, 1.0f, 1.0f } };
	VkClearColorValue							clearColorMiddle		= { { 1.0f, 0.0f, 0.0f, 1.0f } };
	VkClearColorValue							clearColorFinal			= { { 0.0f, 1.0f, 0.0f, 1.0f } };
	VkClearDepthStencilValue					clearDepthValueInitial	= { 0.4f, 0 };
	VkClearDepthStencilValue					clearDepthValueMiddle	= { 0.6f, 0 };
	VkClearDepthStencilValue					clearDepthValueFinal	= { 0.9f, 0 };

	if (m_testParams.m_useOffset) offsetMultiplier = 3;

	createInitBufferWithPredicate(m_testParams.m_discard, m_testParams.m_invert, offsetMultiplier);
	m_testParams.m_testDepth ? createTargetDepthImageAndImageView() : createTargetColorImageAndImageView();
	createResultBuffer(m_testParams.m_testDepth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM);

	m_cmdPool									= createCommandPool(m_vkd, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBufferPrimary							= allocateCommandBuffer(m_vkd, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	createRenderPass(m_testParams.m_testDepth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM, m_testParams.m_testDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	createFramebuffer(m_testParams.m_testDepth ? m_depthTargetView.get() : m_colorTargetView.get());

	const VkConditionalRenderingBeginInfoEXT	conditionalRenderingBeginInfo =
	{
		VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,																						//VkStructureType					sType;
		DE_NULL,																																	//const void*						pNext;
		m_conditionalRenderingBuffer->object(),																										//VkBuffer							buffer;
		sizeof(deUint32) * offsetMultiplier,																										//VkDeviceSize						offset;
		(m_testParams.m_invert ? (VkConditionalRenderingFlagsEXT) VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : (VkConditionalRenderingFlagsEXT) 0)	//VkConditionalRenderingFlagsEXT	flags;
	};

	beginCommandBuffer(m_vkd, *m_cmdBufferPrimary);

	imageMemoryBarrier(m_testParams.m_testDepth ? m_depthTargetImage->object() : m_colorTargetImage->object(),										//VkImage							 image
					   0u,																															//VkAccessFlags						srcAccessMask
					   VK_ACCESS_TRANSFER_WRITE_BIT,																								//VkAccessFlags						dstAccessMask
					   VK_IMAGE_LAYOUT_UNDEFINED,																									//VkImageLayout						oldLayout
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,																						//VkImageLayout						newLayout
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																								//VkPipelineStageFlags				srcStageMask
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																								//VkPipelineStageFlags				dstStageMask
					   m_testParams.m_testDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);											//VkImageAspectFlags				flags

	m_testParams.m_testDepth					?	clearWithClearDepthStencilImage(clearDepthValueInitial)
												:	clearWithClearColorImage(clearColorInitial);

	imageMemoryBarrier(m_testParams.m_testDepth ? m_depthTargetImage->object() : m_colorTargetImage->object(),										//VkImage							image
					   VK_ACCESS_TRANSFER_WRITE_BIT,																								//VkAccessFlags						srcAccessMask
					   m_testParams.m_testDepth ? (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
												: (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),						//VkAccessFlags						dstAccessMask
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,																						//VkImageLayout						oldLayout
					   m_testParams.m_testDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//VkImageLayout						newLayout
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																								//VkPipelineStageFlags				srcStageMask
					   m_testParams.m_testDepth ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,				//VkPipelineStageFlags				dstStageMask
					   m_testParams.m_testDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);											//VkImageAspectFlags				flags

	if (m_testParams.m_clearAttachmentTwice)
	{
		beginRenderPass(m_vkd, *m_cmdBufferPrimary, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT));

		m_testParams.m_testDepth	? clearDepthWithClearAttachments(clearDepthValueMiddle, m_testParams.m_partialClear)
									: clearColorWithClearAttachments(clearColorMiddle, m_testParams.m_partialClear);

		m_vkd.cmdBeginConditionalRenderingEXT(*m_cmdBufferPrimary, &conditionalRenderingBeginInfo);

		m_testParams.m_testDepth	? clearDepthWithClearAttachments(clearDepthValueFinal, m_testParams.m_partialClear)
									: clearColorWithClearAttachments(clearColorFinal, m_testParams.m_partialClear);

		m_vkd.cmdEndConditionalRenderingEXT(*m_cmdBufferPrimary);

		endRenderPass(m_vkd, *m_cmdBufferPrimary);
	}
	else
	{
		m_vkd.cmdBeginConditionalRenderingEXT(*m_cmdBufferPrimary, &conditionalRenderingBeginInfo);

		beginRenderPass(m_vkd, *m_cmdBufferPrimary, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT));

		m_testParams.m_testDepth	?	clearDepthWithClearAttachments(clearDepthValueFinal, m_testParams.m_partialClear)
									:	clearColorWithClearAttachments(clearColorFinal, m_testParams.m_partialClear);

		endRenderPass(m_vkd, *m_cmdBufferPrimary);
		m_vkd.cmdEndConditionalRenderingEXT(*m_cmdBufferPrimary);
	}

	imageMemoryBarrier(m_testParams.m_testDepth ? m_depthTargetImage->object() : m_colorTargetImage->object(),										//VkImage							image
					   m_testParams.m_testDepth ? (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
												: (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),						//VkAccessFlags						dstAccessMask
					   VK_ACCESS_TRANSFER_READ_BIT,																									//VkAccessFlags						dstAccessMask
					   m_testParams.m_testDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//VkImageLayout						oldLayout
					   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,																						//VkImageLayout						newLayout
					   m_testParams.m_testDepth ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,				//VkPipelineStageFlags				srcStageMask
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																								//VkPipelineStageFlags				dstStageMask
					   m_testParams.m_testDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);											//VkImageAspectFlags				flags

	copyResultImageToBuffer(m_testParams.m_testDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT, m_testParams.m_testDepth ? m_depthTargetImage->object() : m_colorTargetImage->object());

	const vk::VkBufferMemoryBarrier bufferMemoryBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,
		vk::VK_ACCESS_HOST_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		m_resultBuffer->object(),
		0u,
		VK_WHOLE_SIZE
	};

	m_vkd.cmdPipelineBarrier(*m_cmdBufferPrimary, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferMemoryBarrier, 0u, DE_NULL);

	endCommandBuffer(m_vkd, *m_cmdBufferPrimary);

	submitCommandsAndWait(m_vkd, m_device, m_queue, *m_cmdBufferPrimary);

	invalidateMappedMemoryRange(m_vkd, m_device, m_resultBuffer->getBoundMemory().getMemory(), m_resultBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);

	tcu::ConstPixelBufferAccess					result(mapVkFormat(m_testParams.m_testDepth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM), tcu::IVec3(WIDTH, HEIGHT, 1), m_resultBuffer->getBoundMemory().getHostPtr());

	std::vector<float>							referenceData((m_testParams.m_testDepth ? 1 : 4) * WIDTH * HEIGHT, 0);
	tcu::PixelBufferAccess						reference(mapVkFormat(m_testParams.m_testDepth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM), tcu::IVec3(WIDTH, HEIGHT, 1), referenceData.data());

	if (!m_testParams.m_partialClear)
	{
		m_testParams.m_testDepth	? prepareReferenceImageOneDepth(reference, m_testParams.m_discard ? (m_testParams.m_clearAttachmentTwice ? clearDepthValueMiddle : clearDepthValueInitial) : clearDepthValueFinal)
									: prepareReferenceImageOneColor(reference, m_testParams.m_discard ? (m_testParams.m_clearAttachmentTwice ? clearColorMiddle : clearColorInitial) : clearColorFinal);
	}
	else
	{
		m_testParams.m_testDepth	? prepareReferenceImageDepthClearPartial(reference, clearDepthValueInitial, m_testParams.m_discard ? (m_testParams.m_clearAttachmentTwice ? clearDepthValueMiddle : clearDepthValueInitial) : clearDepthValueFinal)
									: prepareReferenceImageColorClearPartial(reference, clearColorInitial, m_testParams.m_discard ? (m_testParams.m_clearAttachmentTwice ? clearColorMiddle : clearColorInitial) : clearColorFinal);
	}

	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Comparison", "Comparison", reference, result, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

ConditionalRenderingDrawTestInstance::ConditionalRenderingDrawTestInstance (Context& context, const DrawTestParams& testParams)
	: ConditionalRenderingBaseTestInstance	(context)
	, m_testParams							(testParams)
{}

tcu::TestStatus ConditionalRenderingDrawTestInstance::iterate (void)
{
	const deUint32						queueFamilyIndex				= m_context.getUniversalQueueFamilyIndex();
	VkClearColorValue					clearColorInitial				= { { 0.0f, 0.0f, 1.0f, 1.0f } };
	deUint32							offsetMultiplier				= 0;

	if (m_testParams.m_useOffset) offsetMultiplier = 3;

	VkBufferUsageFlagBits				bufferUsageExtraFlags			= (VkBufferUsageFlagBits)0;
	if (m_testParams.m_togglePredicate)
		bufferUsageExtraFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	createInitBufferWithPredicate(m_testParams.m_discard, m_testParams.m_invert, offsetMultiplier, bufferUsageExtraFlags);

	if (m_testParams.m_toggleMode == COPY)
	{
		//we need another buffer to copy from, with toggled predicate value
		m_conditionalRenderingBufferForCopy.swap(m_conditionalRenderingBuffer);
		createInitBufferWithPredicate(!m_testParams.m_discard, m_testParams.m_invert, offsetMultiplier, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		m_conditionalRenderingBufferForCopy.swap(m_conditionalRenderingBuffer);
	}
	createTargetColorImageAndImageView();
	createResultBuffer(VK_FORMAT_R8G8B8A8_UNORM);
	createVertexBuffer();

	m_cmdPool							= createCommandPool(m_vkd, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBufferPrimary					= allocateCommandBuffer(m_vkd, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	createRenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	createFramebuffer(m_colorTargetView.get());

	DescriptorSetLayoutBuilder			builder;

	builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);

	m_descriptorSetLayout				= builder.build(m_vkd, m_device, (VkDescriptorSetLayoutCreateFlags)0);

	m_descriptorPool					= DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																 .build(m_vkd, m_device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	createPipelineLayout();
	createAndUpdateDescriptorSet();

	m_vertexShaderModule				= createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("position_only.vert"), 0);
	m_fragmentShaderModule				= createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("only_color_out.frag"), 0);

	createPipeline();

	VkConditionalRenderingBeginInfoEXT	conditionalRenderingBeginInfo =
	{
		VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,																		//VkStructureType					sType;
		DE_NULL,																													//const void*						pNext;
		m_conditionalRenderingBuffer->object(),																						//VkBuffer							buffer;
		sizeof(deUint32) * offsetMultiplier,																						//VkDeviceSize						offset;
		(m_testParams.m_invert	? (VkConditionalRenderingFlagsEXT)VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT
								: (VkConditionalRenderingFlagsEXT)0)																//VkConditionalRenderingFlagsEXT	flags;
	};

	beginCommandBuffer(m_vkd, *m_cmdBufferPrimary);

	imageMemoryBarrier(m_colorTargetImage->object(),																				//VkImage							image
					   0u,																											//VkAccessFlags						srcAccessMask
					   VK_ACCESS_TRANSFER_WRITE_BIT,																				//VkAccessFlags						dstAccessMask
					   VK_IMAGE_LAYOUT_UNDEFINED,																					//VkImageLayout						oldLayout
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,																		//VkImageLayout						newLayout
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																				//VkPipelineStageFlags				srcStageMask
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																				//VkPipelineStageFlags				dstStageMask
					   VK_IMAGE_ASPECT_COLOR_BIT);																					//VkImageAspectFlags				flags

	clearWithClearColorImage(clearColorInitial);

	imageMemoryBarrier(m_colorTargetImage->object(),																				//VkImage							image
					   VK_ACCESS_TRANSFER_WRITE_BIT,																				//VkAccessFlags						srcAccessMask
					   VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,									//VkAccessFlags						dstAccessMask
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,																		//VkImageLayout						oldLayout
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,																	//VkImageLayout						newLayout
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																				//VkPipelineStageFlags				srcStageMask
					   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,																//VkPipelineStageFlags				dstStageMask
					   VK_IMAGE_ASPECT_COLOR_BIT);																					//VkImageAspectFlags				flags

	m_vkd.cmdBindPipeline(*m_cmdBufferPrimary, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	m_vkd.cmdBindDescriptorSets(*m_cmdBufferPrimary, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &(*m_descriptorSet), 0, DE_NULL);

	if (m_testParams.m_togglePredicate)
	{
		if (m_testParams.m_toggleMode == FILL)
		{
			m_testParams.m_discard		= !m_testParams.m_discard;
			deUint32 predicate			= m_testParams.m_discard ? m_testParams.m_invert : !m_testParams.m_invert;
			m_vkd.cmdFillBuffer(*m_cmdBufferPrimary, m_conditionalRenderingBuffer->object(), m_conditionalRenderingBufferOffset, sizeof(predicate), predicate);
			bufferMemoryBarrier(m_conditionalRenderingBuffer->object(), m_conditionalRenderingBufferOffset, sizeof(predicate), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
								VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT);
		}
		if (m_testParams.m_toggleMode == COPY)
		{
			VkBufferCopy region =
			{
				m_conditionalRenderingBufferOffset,																					//VkDeviceSize						srcOffset;
				m_conditionalRenderingBufferOffset,																					//VkDeviceSize						dstOffset;
				sizeof(deUint32)																									//VkDeviceSize						size;
			};
			m_vkd.cmdCopyBuffer(*m_cmdBufferPrimary, m_conditionalRenderingBufferForCopy->object(), m_conditionalRenderingBuffer->object(), 1, &region);
			bufferMemoryBarrier(m_conditionalRenderingBuffer->object(), m_conditionalRenderingBufferOffset, sizeof(deUint32), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
								VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT);
		}
	}

	beginRenderPass(m_vkd, *m_cmdBufferPrimary, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT));

	deInt32								data[4]							= { -1, -1, -1, -1 };
	void*								dataPtr							= data;

	for (int drawNdx = 0; drawNdx < 4; drawNdx++)
	{
		data[0] = drawNdx;
		m_vkd.cmdPushConstants(*m_cmdBufferPrimary, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, dataPtr);

		if (isBitSet(m_testParams.m_beginSequenceBits, drawNdx))
			m_vkd.cmdBeginConditionalRenderingEXT(*m_cmdBufferPrimary, &conditionalRenderingBeginInfo);

		draw();

		if (isBitSet(m_testParams.m_endSequenceBits, drawNdx))
			m_vkd.cmdEndConditionalRenderingEXT(*m_cmdBufferPrimary);
	}

	endRenderPass(m_vkd, *m_cmdBufferPrimary);

	imageMemoryBarrier(m_colorTargetImage->object(),																				//VkImage							image
					   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,																		//VkAccessFlags						srcAccessMask
					   VK_ACCESS_TRANSFER_READ_BIT,																					//VkAccessFlags						dstAccessMask
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,																	//VkImageLayout						oldLayout
					   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,																		//VkImageLayout						newLayout
					   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,																//VkPipelineStageFlags				srcStageMask
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																				//VkPipelineStageFlags				dstStageMask
					   VK_IMAGE_ASPECT_COLOR_BIT);																					//VkImageAspectFlags				flags

	copyResultImageToBuffer(VK_IMAGE_ASPECT_COLOR_BIT, m_colorTargetImage->object());

	const vk::VkBufferMemoryBarrier bufferMemoryBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,
		vk::VK_ACCESS_HOST_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		m_resultBuffer->object(),
		0u,
		VK_WHOLE_SIZE
	};

	m_vkd.cmdPipelineBarrier(*m_cmdBufferPrimary, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferMemoryBarrier, 0u, DE_NULL);

	endCommandBuffer(m_vkd, *m_cmdBufferPrimary);

	submitCommandsAndWait(m_vkd, m_device, m_queue, *m_cmdBufferPrimary);

	invalidateMappedMemoryRange(m_vkd, m_device, m_resultBuffer->getBoundMemory().getMemory(), m_resultBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);

	tcu::ConstPixelBufferAccess			result(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), tcu::IVec3(WIDTH, HEIGHT, 1), m_resultBuffer->getBoundMemory().getHostPtr());

	std::vector<float>					referenceData(4 * WIDTH * HEIGHT, 0.5f);
	tcu::PixelBufferAccess				reference(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), tcu::IVec3(WIDTH, HEIGHT, 1), referenceData.data());

	prepareReferenceImage(reference, clearColorInitial, m_testParams.m_resultBits);

	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Comparison", "Comparison", reference, result, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

void ConditionalRenderingDrawTestInstance::createPipelineLayout (void)
{
	const VkPushConstantRange			pushConstantRange =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,					//VkShaderStageFlags			stageFlags;
		0,												//deUint32						offset;
		16												//deUint32						size;
	};

	const VkPipelineLayoutCreateInfo	pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//VkStructureType				sType
		DE_NULL,										//const void*					pNext
		(VkPipelineLayoutCreateFlags)0,					//VkPipelineLayoutCreateFlags	flags
		1u,												//deUint32						descriptorSetCount
		&(m_descriptorSetLayout.get()),					//const VkDescriptorSetLayout*	pSetLayouts
		1u,												//deUint32						pushConstantRangeCount
		&pushConstantRange								//const VkPushConstantRange*	pPushConstantRanges
	};

	m_pipelineLayout					= vk::createPipelineLayout(m_vkd, m_device, &pipelineLayoutParams);
}

void ConditionalRenderingDrawTestInstance::prepareReferenceImage (tcu::PixelBufferAccess& reference, const VkClearColorValue& clearColor, deUint32 resultBits)
{
	for (int w = 0; w < WIDTH; w++)
		for (int h = 0; h < HEIGHT; h++)
			reference.setPixel(tcu::Vec4(clearColor.float32), w, h);

	int step = (HEIGHT / 4);
	for (int w = 0; w < WIDTH; w++)
		for (int h = 0; h < HEIGHT; h++)
		{
			if (h < step && isBitSet(resultBits, 0)) reference.setPixel(tcu::Vec4(0, 1, 0, 1), w, h);
			if (h >= step && h < (step * 2) && isBitSet(resultBits, 1)) reference.setPixel(tcu::Vec4(0, 1, 0, 1), w, h);
			if (h >= (step * 2) && h < (step * 3) && isBitSet(resultBits, 2)) reference.setPixel(tcu::Vec4(0, 1, 0, 1), w, h);
			if (h >= (step * 3) && isBitSet(resultBits, 3)) reference.setPixel(tcu::Vec4(0, 1, 0, 1), w, h);
		}
}

ConditionalRenderingUpdateBufferWithDrawTestInstance::ConditionalRenderingUpdateBufferWithDrawTestInstance (Context& context, bool testParams)
	: ConditionalRenderingBaseTestInstance	(context)
	, m_testParams							(testParams)
{}

void ConditionalRenderingUpdateBufferWithDrawTestInstance::createAndUpdateDescriptorSets (void)
{
	//the same descriptor set layout can be used for the creation of both descriptor sets
	const VkDescriptorSetAllocateInfo	allocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,			//VkStructureType						sType
		DE_NULL,												//const void*							pNext
		*m_descriptorPool,										//VkDescriptorPool						descriptorPool
		1u,														//deUint32								setLayoutCount
		&(m_descriptorSetLayout.get())							//const VkDescriptorSetLayout*			pSetLayouts
	};

	m_descriptorSet						= allocateDescriptorSet(m_vkd, m_device, &allocInfo);
	VkDescriptorBufferInfo				descriptorInfo			= makeDescriptorBufferInfo(m_vertexBuffer->object(), (VkDeviceSize)0u, sizeof(float) * 16);

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(m_vkd, m_device);

	m_descriptorSetUpdate				= allocateDescriptorSet(m_vkd, m_device, &allocInfo);
	VkDescriptorBufferInfo				descriptorInfoUpdate	= makeDescriptorBufferInfo(m_conditionalRenderingBuffer->object(), (VkDeviceSize)0u, sizeof(deUint32));

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSetUpdate, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfoUpdate)
		.update(m_vkd, m_device);
}

void ConditionalRenderingUpdateBufferWithDrawTestInstance::createPipelines (void)
{
	const std::vector<VkViewport>					viewports(1, makeViewport(tcu::UVec2(WIDTH, HEIGHT)));
	const std::vector<VkRect2D>						scissors(1, makeRect2D(tcu::UVec2(WIDTH, HEIGHT)));
	const VkPrimitiveTopology						topology														= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,													//VkStructureType								sType
		DE_NULL,																									//const void*									pNext
		0u,																											//vkPipelineVertexInputStateCreateFlags			flags
		0u,																											//deUint32										bindingCount
		DE_NULL,																									//const VkVertexInputBindingDescription*		pVertexBindingDescriptions
		0u,																											//deUint32										attributeCount
		DE_NULL,																									//const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
	};

	m_pipelineDraw									= makeGraphicsPipeline(m_vkd,									//const DeviceInterface&						vk
																		   m_device,								//const VkDevice								device
																		   *m_pipelineLayout,						//const VkPipelineLayout						pipelineLayout
																		   *m_vertexShaderModuleDraw,				//const VkShaderModule							vertexShaderModule
																		   DE_NULL,									//const VkShaderModule							tessellationControlShaderModule
																		   DE_NULL,									//const VkShaderModule							tessellationEvalShaderModule
																		   DE_NULL,									//const VkShaderModule							geometryShaderModule
																		   *m_fragmentShaderModuleDraw,				//const VkShaderModule							fragmentShaderModule
																		   *m_renderPass,							//const VkRenderPass							renderPass
																		   viewports,								//const std::vector<VkViewport>&				viewports
																		   scissors,								//const std::vector<VkRect2D>&					scissors
																		   topology,								//const VkPrimitiveTopology						topology
																		   0u,										//const deUint32								subpass
																		   0u,										//const deUint32								patchControlPoints
																		   &vertexInputStateParams);				//const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo

	m_pipelineUpdate								= makeGraphicsPipeline(m_vkd,									//const DeviceInterface&						vk
																		   m_device,								//const VkDevice								device
																		   *m_pipelineLayout,						//const VkPipelineLayout						pipelineLayout
																		   *m_vertexShaderModuleUpdate,				//const VkShaderModule							vertexShaderModule
																		   DE_NULL,									//const VkShaderModule							tessellationControlShaderModule
																		   DE_NULL,									//const VkShaderModule							tessellationEvalShaderModule
																		   DE_NULL,									//const VkShaderModule							geometryShaderModule
																		   *m_fragmentShaderModuleDiscard,			//const VkShaderModule							fragmentShaderModule
																		   *m_renderPass,							//const VkRenderPass							renderPass
																		   viewports,								//const std::vector<VkViewport>&				viewports
																		   scissors,								//const std::vector<VkRect2D>&					scissors
																		   topology,								//const VkPrimitiveTopology						topology
																		   0u,										//const deUint32								subpass
																		   0u,										//const deUint32								patchControlPoints
																		   &vertexInputStateParams);				//const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo
}

void ConditionalRenderingUpdateBufferWithDrawTestInstance::createRenderPass (VkFormat format, VkImageLayout layout)
{
	RenderPassCreateInfo			renderPassCreateInfo;

	renderPassCreateInfo.addAttachment(AttachmentDescription(format,
															 VK_SAMPLE_COUNT_1_BIT,
															 VK_ATTACHMENT_LOAD_OP_LOAD,
															 VK_ATTACHMENT_STORE_OP_STORE,
															 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
															 VK_ATTACHMENT_STORE_OP_STORE,
															 isDepthStencilFormat(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
															 isDepthStencilFormat(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

	const VkAttachmentReference		attachmentReference =
	{
		0u,														// deUint32				attachment
		layout													// VkImageLayout		layout
	};

	renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
													   0,
													   0,
													   DE_NULL,
													   isDepthStencilFormat(format) ? 0 : 1,
													   isDepthStencilFormat(format) ? DE_NULL : &attachmentReference,
													   DE_NULL,
													   isDepthStencilFormat(format) ? attachmentReference : AttachmentReference(),
													   0,
													   DE_NULL));

	VkSubpassDependency				dependency =
	{
		0,														//deUint32				srcSubpass;
		0,														//deUint32				dstSubpass;
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,					//VkPipelineStageFlags	srcStageMask;
		VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT,		//VkPipelineStageFlags	dstStageMask;
		VK_ACCESS_SHADER_WRITE_BIT,								//VkAccessFlags			srcAccessMask;
		VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,			//VkAccessFlags			dstAccessMask;
		(VkDependencyFlags)0									//VkDependencyFlags		dependencyFlags;
	};

	renderPassCreateInfo.addDependency(dependency);

	m_renderPass					= vk::createRenderPass(m_vkd, m_device, &renderPassCreateInfo);
}

tcu::TestStatus ConditionalRenderingUpdateBufferWithDrawTestInstance::iterate (void)
{
	const deUint32							queueFamilyIndex						= m_context.getUniversalQueueFamilyIndex();
	VkClearColorValue						clearColorInitial						= { { 0.0f, 0.0f, 1.0f, 1.0f } };

	createInitBufferWithPredicate(m_testParams, true, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	createTargetColorImageAndImageView();
	createResultBuffer(VK_FORMAT_R8G8B8A8_UNORM);
	createVertexBuffer();

	m_cmdPool								= createCommandPool(m_vkd, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBufferPrimary						= allocateCommandBuffer(m_vkd, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	createRenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	createFramebuffer(m_colorTargetView.get());

	DescriptorSetLayoutBuilder				builder;
	builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
	m_descriptorSetLayout					= builder.build(m_vkd, m_device, (VkDescriptorSetLayoutCreateFlags)0);

	m_descriptorPool						= DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2)
																	 .build(m_vkd, m_device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

	createPipelineLayout();
	createAndUpdateDescriptorSets();

	m_vertexShaderModuleDraw				= createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("position_only.vert"), 0);
	m_fragmentShaderModuleDraw				= createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("only_color_out.frag"), 0);
	m_vertexShaderModuleUpdate				= createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("update.vert"), 0);
	m_fragmentShaderModuleDiscard			= createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("discard.frag"), 0);

	createPipelines();

	VkConditionalRenderingBeginInfoEXT		conditionalRenderingBeginInfo =
	{
		VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,																		//VkStructureType					sType;
		DE_NULL,																													//const void*						pNext;
		m_conditionalRenderingBuffer->object(),																						//VkBuffer							buffer;
		0,																															//VkDeviceSize						offset;
		VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT																					//VkConditionalRenderingFlagsEXT	flags;
	};

	beginCommandBuffer(m_vkd, *m_cmdBufferPrimary);

	imageMemoryBarrier(m_colorTargetImage->object(),																				//VkImage							image
					   0u,																											//VkAccessFlags						srcAccessMask
					   VK_ACCESS_TRANSFER_WRITE_BIT,																				//VkAccessFlags						dstAccessMask
					   VK_IMAGE_LAYOUT_UNDEFINED,																					//VkImageLayout						oldLayout
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,																		//VkImageLayout						newLayout
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																				//VkPipelineStageFlags				srcStageMask
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																				//VkPipelineStageFlags				dstStageMask
					   VK_IMAGE_ASPECT_COLOR_BIT);																					//VkImageAspectFlags				flags

	clearWithClearColorImage(clearColorInitial);

	imageMemoryBarrier(m_colorTargetImage->object(),																				//VkImage							image
					   VK_ACCESS_TRANSFER_WRITE_BIT,																				//VkAccessFlags						srcAccessMask
					   VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,									//VkAccessFlags						dstAccessMask
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,																		//VkImageLayout						oldLayout
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,																	//VkImageLayout						newLayout
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																				//VkPipelineStageFlags				srcStageMask
					   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,																//VkPipelineStageFlags				dstStageMask
					   VK_IMAGE_ASPECT_COLOR_BIT);																					//VkImageAspectFlags				flags

	beginRenderPass(m_vkd, *m_cmdBufferPrimary, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT));

	m_vkd.cmdBindPipeline(*m_cmdBufferPrimary, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineUpdate);
	m_vkd.cmdBindDescriptorSets(*m_cmdBufferPrimary, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &(*m_descriptorSetUpdate), 0, DE_NULL);

	draw();

	endRenderPass(m_vkd, *m_cmdBufferPrimary);

	bufferMemoryBarrier(m_conditionalRenderingBuffer->object(), m_conditionalRenderingBufferOffset, sizeof(deUint32), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
						VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT);

	beginRenderPass(m_vkd, *m_cmdBufferPrimary, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT));

	m_vkd.cmdBindPipeline(*m_cmdBufferPrimary, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineDraw);
	m_vkd.cmdBindDescriptorSets(*m_cmdBufferPrimary, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &(*m_descriptorSet), 0, DE_NULL);

	m_vkd.cmdBeginConditionalRenderingEXT(*m_cmdBufferPrimary, &conditionalRenderingBeginInfo);
	draw();
	m_vkd.cmdEndConditionalRenderingEXT(*m_cmdBufferPrimary);

	endRenderPass(m_vkd, *m_cmdBufferPrimary);

	imageMemoryBarrier(m_colorTargetImage->object(),																				//VkImage							image
					   VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,																		//VkAccessFlags						srcAccessMask
					   VK_ACCESS_TRANSFER_READ_BIT,																					//VkAccessFlags						dstAccessMask
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,																	//VkImageLayout						oldLayout
					   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,																		//VkImageLayout						newLayout
					   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,																//VkPipelineStageFlags				srcStageMask
					   VK_PIPELINE_STAGE_TRANSFER_BIT,																				//VkPipelineStageFlags				dstStageMask
					   VK_IMAGE_ASPECT_COLOR_BIT);																					//VkImageAspectFlags				flags

	copyResultImageToBuffer(VK_IMAGE_ASPECT_COLOR_BIT, m_colorTargetImage->object());

	const vk::VkBufferMemoryBarrier bufferMemoryBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,
		vk::VK_ACCESS_HOST_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		m_resultBuffer->object(),
		0u,
		VK_WHOLE_SIZE
	};

	m_vkd.cmdPipelineBarrier(*m_cmdBufferPrimary, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferMemoryBarrier, 0u, DE_NULL);

	endCommandBuffer(m_vkd, *m_cmdBufferPrimary);

	submitCommandsAndWait(m_vkd, m_device, m_queue, *m_cmdBufferPrimary);

	invalidateMappedMemoryRange(m_vkd, m_device, m_resultBuffer->getBoundMemory().getMemory(), m_resultBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);

	tcu::ConstPixelBufferAccess			result(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), tcu::IVec3(WIDTH, HEIGHT, 1), m_resultBuffer->getBoundMemory().getHostPtr());

	std::vector<float>					referenceData(4 * WIDTH * HEIGHT, 0.0f);
	tcu::PixelBufferAccess				reference(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), tcu::IVec3(WIDTH, HEIGHT, 1), referenceData.data());

	m_testParams ? prepareReferenceImageOneColor(reference, tcu::Vec4(0,1,0,1)) : prepareReferenceImageOneColor(reference, clearColorInitial);

	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Comparison", "Comparison", reference, result, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

struct AddProgramsDraw
{
	void init (SourceCollections& sources, DrawTestParams testParams) const
	{
		DE_UNREF(testParams);

		const char* const		vertexShader =
			"#version 430\n"

			"layout(std430, binding = 0) buffer BufferPos {\n"
			"vec4 p[100];\n"
			"} pos;\n"

			"out gl_PerVertex{\n"
			"vec4 gl_Position;\n"
			"};\n"

			"void main() {\n"
			"gl_Position = pos.p[gl_VertexIndex];\n"
			"}\n";

		sources.glslSources.add("position_only.vert") << glu::VertexSource(vertexShader);

		const char* const		fragmentShader =
			"#version 430\n"

			"layout(location = 0) out vec4 my_FragColor;\n"

			"layout (push_constant) uniform AreaSelect {\n"
			"	ivec4 number;\n"
			"} Area;\n"

			"void main() {\n"
			"	if((gl_FragCoord.y < 64) && (Area.number.x != 0)) discard;\n"
			"	if((gl_FragCoord.y >= 64) && (gl_FragCoord.y < 128) && (Area.number.x != 1)) discard;\n"
			"	if((gl_FragCoord.y >= 128) && (gl_FragCoord.y < 192) && (Area.number.x != 2)) discard;\n"
			"	if((gl_FragCoord.y >= 192) && (Area.number.x != 3)) discard;\n"
			"	my_FragColor = vec4(0,1,0,1);\n"
			"}\n";

		sources.glslSources.add("only_color_out.frag") << glu::FragmentSource(fragmentShader);
	}
};

struct AddProgramsUpdateBufferUsingRendering
{
	void init (SourceCollections& sources, bool testParams) const
	{
		std::string				atomicOperation			= (testParams ? "atomicMin(predicate.p, 0);" : "atomicMax(predicate.p, 1);");

		std::string				vertexShaderUpdate =
			"#version 430\n"

			"layout(std430, binding = 0) buffer Predicate {\n"
			"uint p;\n"
			"} predicate;\n"

			"out gl_PerVertex{\n"
			"vec4 gl_Position;\n"
			"};\n"

			"void main() {\n" +
			atomicOperation +
			"gl_Position = vec4(1.0);\n"
			"}\n";

		sources.glslSources.add("update.vert") << glu::VertexSource(vertexShaderUpdate);

		const char* const		vertexShaderDraw =
			"#version 430\n"

			"layout(std430, binding = 0) buffer BufferPos {\n"
			"vec4 p[100];\n"
			"} pos;\n"

			"out gl_PerVertex{\n"
			"vec4 gl_Position;\n"
			"};\n"

			"void main() {\n"
			"gl_Position = pos.p[gl_VertexIndex];\n"
			"}\n";

		sources.glslSources.add("position_only.vert") << glu::VertexSource(vertexShaderDraw);

		const char* const		fragmentShaderDiscard =
			"#version 430\n"

			"layout(location = 0) out vec4 my_FragColor;\n"

			"void main() {\n"
			"	discard;\n"
			"}\n";

		sources.glslSources.add("discard.frag")
			<< glu::FragmentSource(fragmentShaderDiscard);

		const char* const		fragmentShaderDraw =
			"#version 430\n"

			"layout(location = 0) out vec4 my_FragColor;\n"

			"void main() {\n"
			"	my_FragColor = vec4(0,1,0,1);\n"
			"}\n";

		sources.glslSources.add("only_color_out.frag") << glu::FragmentSource(fragmentShaderDraw);
	}
};

void checkSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
}

void checkFan (Context& context)
{
	checkSupport(context);

	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().triangleFans)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
	}
}

void checkFanAndVertexStores (Context& context)
{
	checkFan(context);

	const auto& features = context.getDeviceFeatures();
	if (!features.vertexPipelineStoresAndAtomics)
		TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");
}

} // unnamed namespace

ConditionalRenderingDrawAndClearTests::ConditionalRenderingDrawAndClearTests (tcu::TestContext &testCtx)
	: TestCaseGroup (testCtx, "draw_clear", "VK_EXT_conditional_rendering extension tests")
{
	/* Left blank on purpose */
}

void ConditionalRenderingDrawAndClearTests::init (void)
{
	tcu::TestCaseGroup*	clear	= new tcu::TestCaseGroup(m_testCtx, "clear", "Tests using vkCmdClearAttachments.");
	tcu::TestCaseGroup*	color	= new tcu::TestCaseGroup(m_testCtx, "color", "Test color clear.");
	tcu::TestCaseGroup*	depth	= new tcu::TestCaseGroup(m_testCtx, "depth", "Test depth clear.");
	tcu::TestCaseGroup*	draw	= new tcu::TestCaseGroup(m_testCtx, "draw", "Test drawing.");

	for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(clearColorTestGrid); testNdx++)
		color->addChild(new InstanceFactory1WithSupport<ConditionalRenderingClearAttachmentsTestInstance, ClearTestParams, FunctionSupport0>(m_testCtx, tcu::NODETYPE_SELF_VALIDATE, generateClearTestName(clearColorTestGrid[testNdx]), "Color clear test.", clearColorTestGrid[testNdx], checkSupport));

	for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(clearDepthTestGrid); testNdx++)
		depth->addChild(new InstanceFactory1WithSupport<ConditionalRenderingClearAttachmentsTestInstance, ClearTestParams, FunctionSupport0>(m_testCtx, tcu::NODETYPE_SELF_VALIDATE, generateClearTestName(clearDepthTestGrid[testNdx]), "Depth clear test.", clearDepthTestGrid[testNdx], checkSupport));

	for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(clearColorTwiceGrid); testNdx++)
		color->addChild(new InstanceFactory1WithSupport<ConditionalRenderingClearAttachmentsTestInstance, ClearTestParams, FunctionSupport0>(m_testCtx, tcu::NODETYPE_SELF_VALIDATE, "clear_attachment_twice_" + generateClearTestName(clearColorTwiceGrid[testNdx]), "Color clear test.", clearColorTwiceGrid[testNdx], checkSupport));

	for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(clearDepthTwiceGrid); testNdx++)
		depth->addChild(new InstanceFactory1WithSupport<ConditionalRenderingClearAttachmentsTestInstance, ClearTestParams, FunctionSupport0>(m_testCtx, tcu::NODETYPE_SELF_VALIDATE, "clear_attachment_twice_" + generateClearTestName(clearDepthTwiceGrid[testNdx]), "Depth clear test.", clearDepthTwiceGrid[testNdx], checkSupport));

	for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(drawTestGrid); testNdx++)
		draw->addChild(new InstanceFactory1WithSupport<ConditionalRenderingDrawTestInstance, DrawTestParams, FunctionSupport0, AddProgramsDraw>(m_testCtx, tcu::NODETYPE_SELF_VALIDATE, "case_" + de::toString(testNdx), "Draw test.", AddProgramsDraw(), drawTestGrid[testNdx], checkFan));

	draw->addChild(new InstanceFactory1WithSupport<ConditionalRenderingUpdateBufferWithDrawTestInstance, bool, FunctionSupport0, AddProgramsUpdateBufferUsingRendering>(m_testCtx, tcu::NODETYPE_SELF_VALIDATE, "update_with_rendering_no_discard", "Draw test.", AddProgramsUpdateBufferUsingRendering(), true, checkFanAndVertexStores));
	draw->addChild(new InstanceFactory1WithSupport<ConditionalRenderingUpdateBufferWithDrawTestInstance, bool, FunctionSupport0, AddProgramsUpdateBufferUsingRendering>(m_testCtx, tcu::NODETYPE_SELF_VALIDATE, "update_with_rendering_discard", "Draw test.", AddProgramsUpdateBufferUsingRendering(), false, checkFanAndVertexStores));

	clear->addChild(color);
	clear->addChild(depth);
	addChild(clear);
	addChild(draw);
}

} // Draw
} // vkt
