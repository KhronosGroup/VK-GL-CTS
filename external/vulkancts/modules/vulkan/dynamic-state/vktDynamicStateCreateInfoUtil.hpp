#ifndef _VKT_DYNAMIC_STATE_CREATEINFO_UTIL_HPP
#define _VKT_DYNAMIC_STATE_CREATEINFO_UTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief CreateInfo utilities
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuVector.hpp"

#include "deSharedPtr.hpp"

#include <vector>

namespace vkt
{
namespace DynamicState
{

class ImageSubresourceRange : public vk::VkImageSubresourceRange
{
public:
	ImageSubresourceRange (	vk::VkImageAspectFlags	aspectMask,
							deUint32				baseMipLevel	= 0,
							deUint32				mipLevels		= 1,
							deUint32				baseArrayLayer	= 0,
							deUint32				arraySize		= 1);
};

class ChannelMapping : public vk::VkChannelMapping
{
public:
	ChannelMapping (	vk::VkChannelSwizzle r = vk::VK_CHANNEL_SWIZZLE_R,
						vk::VkChannelSwizzle g = vk::VK_CHANNEL_SWIZZLE_G,
						vk::VkChannelSwizzle b = vk::VK_CHANNEL_SWIZZLE_B,
						vk::VkChannelSwizzle a = vk::VK_CHANNEL_SWIZZLE_A);
};

class ImageViewCreateInfo : public vk::VkImageViewCreateInfo
{
public:
	ImageViewCreateInfo (		vk::VkImage						image,
								vk::VkImageViewType				viewType,
								vk::VkFormat					format,
						const	vk::VkImageSubresourceRange&	subresourceRange,
						const	vk::VkChannelMapping&			channels			= ChannelMapping(),
								vk::VkImageViewCreateFlags		flags				= 0);

	ImageViewCreateInfo (		vk::VkImage						image,
								vk::VkImageViewType				viewType,
								vk::VkFormat					format,
						const	vk::VkChannelMapping&			channels			= ChannelMapping(),
								vk::VkImageViewCreateFlags		flags				= 0);
};

class BufferViewCreateInfo : public vk::VkBufferViewCreateInfo
{
public:
	BufferViewCreateInfo (	vk::VkBuffer		buffer,
							vk::VkFormat		format,
							vk::VkDeviceSize	offset,
							vk::VkDeviceSize	range);
};

class BufferCreateInfo : public vk::VkBufferCreateInfo
{
public:
	BufferCreateInfo (		vk::VkDeviceSize			size,
							vk::VkBufferCreateFlags		usage,
							vk::VkSharingMode			sharingMode			= vk::VK_SHARING_MODE_EXCLUSIVE,
							deUint32					queueFamilyCount	= 0,
					 const	deUint32*					pQueueFamilyIndices = DE_NULL, 
							vk::VkBufferCreateFlags		flags				= 0);

	BufferCreateInfo			(const BufferCreateInfo &other);	
	BufferCreateInfo &operator=	(const BufferCreateInfo &other);

private:
	std::vector<deUint32> m_queueFamilyIndices;
};


class ImageCreateInfo : public vk::VkImageCreateInfo
{
public:
	ImageCreateInfo (		vk::VkImageType			imageType,
							vk::VkFormat			format,
							vk::VkExtent3D			extent,
							deUint32				mipLevels,
							deUint32				arraySize,
							deUint32				samples,
							vk::VkImageTiling		tiling,
							vk::VkImageUsageFlags	usage,
							vk::VkSharingMode		sharingMode			= vk::VK_SHARING_MODE_EXCLUSIVE,
							deUint32				queueFamilyCount	= 0,
					const	deUint32*				pQueueFamilyIndices = DE_NULL,
							vk::VkImageCreateFlags	flags				= 0, 
							vk::VkImageLayout		initialLayout		= vk::VK_IMAGE_LAYOUT_UNDEFINED);

private:
	ImageCreateInfo				(const ImageCreateInfo &other);
	ImageCreateInfo &operator=	(const ImageCreateInfo &other);

	std::vector<deUint32> m_queueFamilyIndices;
};

class FramebufferCreateInfo : public vk::VkFramebufferCreateInfo
{
public:
	FramebufferCreateInfo (			vk::VkRenderPass				renderPass,
							const	std::vector<vk::VkImageView>&	attachments,
									deUint32						width,
									deUint32						height,
									deUint32						layers);
};

class AttachmentDescription : public vk::VkAttachmentDescription
{
public:
	AttachmentDescription (	vk::VkFormat				format,
							deUint32					samples,
							vk::VkAttachmentLoadOp	loadOp,
							vk::VkAttachmentStoreOp	storeOp,
							vk::VkAttachmentLoadOp	stencilLoadOp,
							vk::VkAttachmentStoreOp	stencilStoreOp,
							vk::VkImageLayout			initialLayout,
							vk::VkImageLayout			finalLayout);

	AttachmentDescription (const vk::VkAttachmentDescription &);
};

class AttachmentReference : public vk::VkAttachmentReference
{
public:
	AttachmentReference (deUint32 attachment, vk::VkImageLayout layout);
	AttachmentReference (void);
};

class SubpassDescription : public vk::VkSubpassDescription
{
public:
	SubpassDescription (		vk::VkPipelineBindPoint			pipelineBindPoint,
								vk::VkSubpassDescriptionFlags	flags,
								deUint32						inputCount,
						const	vk::VkAttachmentReference*		inputAttachments,
								deUint32						colorCount,
					   const	vk::VkAttachmentReference*		colorAttachments,
					   const	vk::VkAttachmentReference*		resolveAttachments,
								vk::VkAttachmentReference		depthStencilAttachment,
								deUint32						preserveCount,
					   const	vk::VkAttachmentReference*		preserveAttachments);

	SubpassDescription				(const vk::VkSubpassDescription &other);
	SubpassDescription				(const SubpassDescription &other);
	SubpassDescription &operator=	(const SubpassDescription &other);

private:
	std::vector<vk::VkAttachmentReference> m_InputAttachments;
	std::vector<vk::VkAttachmentReference> m_ColorAttachments;
	std::vector<vk::VkAttachmentReference> m_ResolveAttachments;
	std::vector<vk::VkAttachmentReference> m_PreserveAttachments;
};

class SubpassDependency : public vk::VkSubpassDependency
{
public:
	SubpassDependency (	deUint32					srcSubpass,
						deUint32					destSubpass,
						vk::VkPipelineStageFlags	srcStageMask,
						vk::VkPipelineStageFlags	destStageMask,
						vk::VkMemoryOutputFlags		outputMask,
						vk::VkMemoryInputFlags		inputMask,
						vk::VkBool32				byRegion);

	SubpassDependency (const vk::VkSubpassDependency& other);
};

class RenderPassCreateInfo : public vk::VkRenderPassCreateInfo
{
public:
	RenderPassCreateInfo (	const std::vector<vk::VkAttachmentDescription>&	attachments,
							const std::vector<vk::VkSubpassDescription>&	subpasses,
							const std::vector<vk::VkSubpassDependency>&		dependiences		= std::vector<vk::VkSubpassDependency>());

	RenderPassCreateInfo (			deUint32 attachmentCount						= 0,
							const	vk::VkAttachmentDescription*	pAttachments	= DE_NULL,
									deUint32						subpassCount	= 0,
							const	vk::VkSubpassDescription*		pSubpasses		= DE_NULL,
									deUint32						dependencyCount	= 0,
							const	vk::VkSubpassDependency*		pDependiences	= DE_NULL);

	void addAttachment	(vk::VkAttachmentDescription attachment);
	void addSubpass		(vk::VkSubpassDescription subpass);
	void addDependency	(vk::VkSubpassDependency dependency);

private:
	std::vector<AttachmentDescription>	 m_attachments;
	std::vector<SubpassDescription>		 m_subpasses;
	std::vector<SubpassDependency>		 m_dependiences;

	std::vector<vk::VkAttachmentDescription> m_attachmentsStructs;
	std::vector<vk::VkSubpassDescription>	 m_subpassesStructs;
	std::vector<vk::VkSubpassDependency>	 m_dependiencesStructs;

	RenderPassCreateInfo			(const RenderPassCreateInfo &other); //Not allowed!
	RenderPassCreateInfo &operator= (const RenderPassCreateInfo &other); //Not allowed!
};

class RenderPassBeginInfo : public vk::VkRenderPassBeginInfo
{
public:
	RenderPassBeginInfo (			vk::VkRenderPass				renderPass,
									vk::VkFramebuffer				framebuffer,
									vk::VkRect2D					renderArea,
							const	std::vector<vk::VkClearValue>&	clearValues = std::vector<vk::VkClearValue>());

private:
	std::vector<vk::VkClearValue> m_clearValues;

	RenderPassBeginInfo				(const RenderPassBeginInfo &other); //Not allowed!
	RenderPassBeginInfo &operator=	(const RenderPassBeginInfo &other); //Not allowed!
};

class CmdPoolCreateInfo : public vk::VkCmdPoolCreateInfo
{
public:
	CmdPoolCreateInfo (deUint32 queueFamilyIndex,
		vk::VkCmdPoolCreateFlags flags = vk::VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
};

class CmdBufferCreateInfo : public vk::VkCmdBufferCreateInfo
{
public:
	CmdBufferCreateInfo (vk::VkCmdPool pool, vk::VkCmdBufferLevel level, vk::VkCmdBufferCreateFlags flags);
};

class CmdBufferBeginInfo : public vk::VkCmdBufferBeginInfo
{
public:
	CmdBufferBeginInfo (vk::VkCmdBufferOptimizeFlags	flags	= 0);
	CmdBufferBeginInfo (vk::VkRenderPass				renderPass,
						deUint32						subpass,
						vk::VkFramebuffer				framebuffer,
						vk::VkCmdBufferOptimizeFlags	flags	= 0);
};


class DescriptorTypeCount : public vk::VkDescriptorTypeCount
{
public:
	DescriptorTypeCount (vk::VkDescriptorType _type, deUint32 _count)
	{
		type = _type;
		count = _count;
	}
};

class DescriptorPoolCreateInfo : public vk::VkDescriptorPoolCreateInfo
{
public:
	DescriptorPoolCreateInfo (	const	std::vector<vk::VkDescriptorTypeCount>&	typeCounts,
										vk::VkDescriptorPoolUsage poolUsage, 
										deUint32 maxSets);

	DescriptorPoolCreateInfo& addDescriptors (vk::VkDescriptorType type, deUint32 count);

private:
	std::vector<vk::VkDescriptorTypeCount> m_typeCounts;
};


class DescriptorSetLayoutCreateInfo : public vk::VkDescriptorSetLayoutCreateInfo
{
public:
	DescriptorSetLayoutCreateInfo (deUint32 count, const vk::VkDescriptorSetLayoutBinding* pBinding);
};


class PipelineLayoutCreateInfo : public vk::VkPipelineLayoutCreateInfo
{
public:
	PipelineLayoutCreateInfo (			deUint32 descriptorSetCount,
								const	vk::VkDescriptorSetLayout*				pSetLayouts,
										deUint32								pushConstantRangeCount	= 0,
										const vk::VkPushConstantRange*			pPushConstantRanges		= DE_NULL);

	PipelineLayoutCreateInfo (	const	std::vector<vk::VkDescriptorSetLayout>&	setLayouts				= std::vector<vk::VkDescriptorSetLayout>(),
										deUint32								pushConstantRangeCount	= 0,
								const	vk::VkPushConstantRange*				pPushConstantRanges		= DE_NULL);

private:
	std::vector<vk::VkDescriptorSetLayout>	m_setLayouts;
	std::vector<vk::VkPushConstantRange>	m_pushConstantRanges;
};

class PipelineCreateInfo : public vk::VkGraphicsPipelineCreateInfo
{
public:
	class VertexInputState : public vk::VkPipelineVertexInputStateCreateInfo
	{
	public:
		VertexInputState (			deUint32								bindingCount					= 0,
							const	vk::VkVertexInputBindingDescription*	pVertexBindingDescriptions		= NULL,
									deUint32								attributeCount					= 0,
							const	vk::VkVertexInputAttributeDescription*	pVertexAttributeDescriptions	= NULL);
	};

	class InputAssemblerState : public vk::VkPipelineInputAssemblyStateCreateInfo
	{
	public:
		InputAssemblerState (vk::VkPrimitiveTopology topology, vk::VkBool32 primitiveRestartEnable = false);
	};

	class TesselationState : public vk::VkPipelineTessellationStateCreateInfo
	{
	public:
		TesselationState (deUint32 patchControlPoints = 0);
	};

	class ViewportState : public vk::VkPipelineViewportStateCreateInfo
	{
	public:
		ViewportState (	deUint32					viewportCount,
						std::vector<vk::VkViewport> viewports		= std::vector<vk::VkViewport>(0),
						std::vector<vk::VkRect2D>	scissors		= std::vector<vk::VkRect2D>(0));

		ViewportState				(const ViewportState &other);
		ViewportState &operator= 	(const ViewportState &other);

		std::vector<vk::VkViewport> m_viewports;
		std::vector<vk::VkRect2D>	m_scissors;
	};

	class RasterizerState : public vk::VkPipelineRasterStateCreateInfo
	{
	public:
		RasterizerState (	vk::VkBool32	depthClipEnable			= false,
							vk::VkBool32	rasterizerDiscardEnable = false,
							vk::VkFillMode	fillMode				= vk::VK_FILL_MODE_SOLID,
							vk::VkCullMode	cullMode				= vk::VK_CULL_MODE_NONE,
							vk::VkFrontFace frontFace				= vk::VK_FRONT_FACE_CW,
							vk::VkBool32	depthBiasEnable			= true,
							float			depthBias				= 0.0f,
							float			depthBiasClamp			= 0.0f,
							float			slopeScaledDepthBias	= 0.0f,
							float			lineWidth				= 1.0f);
	};

	class MultiSampleState : public vk::VkPipelineMultisampleStateCreateInfo
	{
	public:
		MultiSampleState (			deUint32		rasterSamples				= 1,
									vk::VkBool32	sampleShadingEnable			= false,
									float			minSampleShading			= 0.0f,
							const	std::vector<vk::VkSampleMask>& sampleMask	= std::vector<vk::VkSampleMask>(1, 0xffffffff));

		MultiSampleState			(const MultiSampleState &other);
		MultiSampleState &operator= (const MultiSampleState &other);

	private:
		std::vector<vk::VkSampleMask> m_sampleMask;
	};

	class ColorBlendState : public vk::VkPipelineColorBlendStateCreateInfo
	{
	public:
		class Attachment : public vk::VkPipelineColorBlendAttachmentState
		{
		public:
			Attachment (vk::VkBool32	blendEnable			= false,
						vk::VkBlend		srcBlendColor		= vk::VK_BLEND_SRC_COLOR,
						vk::VkBlend		destBlendColor		= vk::VK_BLEND_DEST_COLOR,
						vk::VkBlendOp	blendOpColor		= vk::VK_BLEND_OP_ADD,
						vk::VkBlend		srcBlendAlpha		= vk::VK_BLEND_SRC_COLOR,
						vk::VkBlend		destBlendAlpha		= vk::VK_BLEND_DEST_COLOR,
						vk::VkBlendOp	blendOpAlpha		= vk::VK_BLEND_OP_ADD,
						deUint8			channelWriteMask	= 0xff);
		};

		ColorBlendState (	const	std::vector<vk::VkPipelineColorBlendAttachmentState>&	attachments,
									vk::VkBool32											alphaToCoverageEnable	= false,
									vk::VkBool32											logicOpEnable			= false,
									vk::VkLogicOp											logicOp					= vk::VK_LOGIC_OP_COPY,
									vk::VkBool32											alphaToOneEnable		= false);

		ColorBlendState (			deUint32												attachmentCount,
							const	vk::VkPipelineColorBlendAttachmentState*				attachments,
									vk::VkBool32											alphaToCoverageEnable	= false,
									vk::VkBool32											logicOpEnable			= false,
									vk::VkLogicOp											logicOp					= vk::VK_LOGIC_OP_COPY,
									vk::VkBool32											alphaToOneEnable		= false);

		ColorBlendState (const vk::VkPipelineColorBlendStateCreateInfo &createInfo);
		ColorBlendState (const ColorBlendState &createInfo, std::vector<float> blendConst = std::vector<float>(4));

	private:
		std::vector<vk::VkPipelineColorBlendAttachmentState> m_attachments;
	};

	class DepthStencilState : public vk::VkPipelineDepthStencilStateCreateInfo
	{
	public:
		class StencilOpState : public vk::VkStencilOpState
		{
		public:
			StencilOpState (vk::VkStencilOp stencilFailOp		= vk::VK_STENCIL_OP_REPLACE,
							vk::VkStencilOp stencilPassOp		= vk::VK_STENCIL_OP_REPLACE,
							vk::VkStencilOp stencilDepthFailOp	= vk::VK_STENCIL_OP_REPLACE,
							vk::VkCompareOp stencilCompareOp	= vk::VK_COMPARE_OP_ALWAYS,
							deUint32		stencilCompareMask	= 0xffffffffu,
							deUint32		stencilWriteMask	= 0xffffffffu,
							deUint32		stencilReference	= 0);
		};

		DepthStencilState (	vk::VkBool32	depthTestEnable			= false,
							vk::VkBool32	depthWriteEnable		= false,
							vk::VkCompareOp depthCompareOp			= vk::VK_COMPARE_OP_ALWAYS,
							vk::VkBool32	depthBoundsTestEnable	= false,
							vk::VkBool32	stencilTestEnable		= false,
							StencilOpState	front					= StencilOpState(),
							StencilOpState	back					= StencilOpState(),
							float			minDepthBounds			= -1.0f,
							float			maxDepthBounds			= 1.0f);
	};

	class PipelineShaderStage : public vk::VkPipelineShaderStageCreateInfo
	{
	public:
		PipelineShaderStage (vk::VkShader shader, vk::VkShaderStage stage);
	};

	class DynamicState : public vk::VkPipelineDynamicStateCreateInfo
	{
	public:
		DynamicState (const std::vector<vk::VkDynamicState>& dynamicStates = std::vector<vk::VkDynamicState>(0));

		DynamicState			(const DynamicState &other);
		DynamicState &operator= (const DynamicState &other);

		std::vector<vk::VkDynamicState> m_dynamicStates;
	};

	PipelineCreateInfo(vk::VkPipelineLayout layout, vk::VkRenderPass renderPass, int subpass, vk::VkPipelineCreateFlags flags);

	PipelineCreateInfo &addShader (const vk::VkPipelineShaderStageCreateInfo&		shader);

	PipelineCreateInfo &addState (const vk::VkPipelineVertexInputStateCreateInfo&	state);
	PipelineCreateInfo &addState (const vk::VkPipelineInputAssemblyStateCreateInfo&	state);
	PipelineCreateInfo &addState (const vk::VkPipelineColorBlendStateCreateInfo&	state);
	PipelineCreateInfo &addState (const vk::VkPipelineViewportStateCreateInfo&		state);
	PipelineCreateInfo &addState (const vk::VkPipelineDepthStencilStateCreateInfo&	state);
	PipelineCreateInfo &addState (const vk::VkPipelineTessellationStateCreateInfo&	state);
	PipelineCreateInfo &addState (const vk::VkPipelineRasterStateCreateInfo&		state);
	PipelineCreateInfo &addState (const vk::VkPipelineMultisampleStateCreateInfo&	state);
	PipelineCreateInfo &addState (const vk::VkPipelineDynamicStateCreateInfo&		state);

private:
	std::vector<vk::VkPipelineShaderStageCreateInfo>			m_shaders;

	de::SharedPtr<vk::VkPipelineVertexInputStateCreateInfo>		m_vertexInputState;
	de::SharedPtr<vk::VkPipelineInputAssemblyStateCreateInfo>	m_iputAssemblyState;
	std::vector<vk::VkPipelineColorBlendAttachmentState>		m_colorBlendStateAttachments;
	de::SharedPtr<vk::VkPipelineColorBlendStateCreateInfo>		m_colorBlendState;
	de::SharedPtr<vk::VkPipelineViewportStateCreateInfo>		m_viewportState;
	de::SharedPtr<vk::VkPipelineDepthStencilStateCreateInfo>	m_dynamicDepthStencilState;
	de::SharedPtr<vk::VkPipelineTessellationStateCreateInfo>	m_tessState;
	de::SharedPtr<vk::VkPipelineRasterStateCreateInfo>			m_rasterState;
	de::SharedPtr<vk::VkPipelineMultisampleStateCreateInfo>		m_multisampleState;
	de::SharedPtr<vk::VkPipelineDynamicStateCreateInfo>			m_dynamicState;

	std::vector<vk::VkDynamicState> m_dynamicStates;

	std::vector<vk::VkViewport> m_viewports;
	std::vector<vk::VkRect2D>	m_scissors;

	std::vector<vk::VkSampleMask>	m_multisampleStateSampleMask;
};

class SamplerCreateInfo : public vk::VkSamplerCreateInfo
{
public:
	SamplerCreateInfo (	vk::VkTexFilter			magFilter				= vk::VK_TEX_FILTER_NEAREST,
						vk::VkTexFilter			minFilter				= vk::VK_TEX_FILTER_NEAREST,
						vk::VkTexMipmapMode		mipMode					= vk::VK_TEX_MIPMAP_MODE_NEAREST,
						vk::VkTexAddressMode	addressU				= vk::VK_TEX_ADDRESS_MODE_MIRROR,
						vk::VkTexAddressMode	addressV				= vk::VK_TEX_ADDRESS_MODE_MIRROR,
						vk::VkTexAddressMode	addressW				= vk::VK_TEX_ADDRESS_MODE_MIRROR,
						float					mipLodBias				= 0.0f,
						float					maxAnisotropy			= 1.0f,
						vk::VkBool32			compareEnable			= false,
						vk::VkCompareOp			compareOp				= vk::VK_COMPARE_OP_ALWAYS,
						float					minLod					= 0.0f,
						float					maxLod					= 16.0f,
						vk::VkBorderColor		borderColor				= vk::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, 
						vk::VkBool32			unnormalizedCoordinates	= false);
};

} // DynamicState
} // vkt

#endif // _VKT_DYNAMIC_STATE_CREATEINFO_UTIL_HPP
