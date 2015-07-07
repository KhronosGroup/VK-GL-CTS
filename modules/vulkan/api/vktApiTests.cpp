/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Module
 * --------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief API Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiTests.hpp"

#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPrograms.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

namespace vkt
{
namespace api
{

using namespace vk;
using std::vector;
using tcu::TestLog;
using de::UniquePtr;
using de::MovePtr;

// \todo [pyry] Want probably clean this up..

typedef std::vector<de::SharedPtr<Allocation> >	AllocationList;

AllocationList allocate (Allocator& allocator, size_t numAllocations, const VkMemoryRequirements* allocRequirements, VkMemoryPropertyFlags memProps = 0u)
{
	AllocationList	allocs;

	for (size_t ndx = 0; ndx < numAllocations; ndx++)
		allocs.push_back(de::SharedPtr<Allocation>(allocate(allocator, allocRequirements[ndx], memProps).release()));

	return allocs;
}

AllocationList allocate (Allocator& allocator, const vector<VkMemoryRequirements>& allocRequirements, VkMemoryPropertyFlags memProps = 0u)
{
	if (!allocRequirements.empty())
		return allocate(allocator, allocRequirements.size(), &allocRequirements[0], memProps);
	else
		return AllocationList();
}

tcu::TestStatus createSampler (Context& context)
{
	const VkDevice			vkDevice	= context.getDevice();
	const DeviceInterface&	vk			= context.getDeviceInterface();

	{
		const struct VkSamplerCreateInfo		samplerInfo	=
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,	//	VkStructureType	sType;
			DE_NULL,								//	const void*		pNext;
			VK_TEX_FILTER_NEAREST,					//	VkTexFilter		magFilter;
			VK_TEX_FILTER_NEAREST,					//	VkTexFilter		minFilter;
			VK_TEX_MIPMAP_MODE_BASE,				//	VkTexMipmapMode	mipMode;
			VK_TEX_ADDRESS_CLAMP,					//	VkTexAddress	addressU;
			VK_TEX_ADDRESS_CLAMP,					//	VkTexAddress	addressV;
			VK_TEX_ADDRESS_CLAMP,					//	VkTexAddress	addressW;
			0.0f,									//	float			mipLodBias;
			0u,										//	deUint32		maxAnisotropy;
			VK_COMPARE_OP_ALWAYS,					//	VkCompareOp		compareOp;
			0.0f,									//	float			minLod;
			0.0f,									//	float			maxLod;
			VK_BORDER_COLOR_TRANSPARENT_BLACK,		//	VkBorderColor	borderColor;
		};

		Move<VkSamplerT>	tmpSampler	= createSampler(vk, vkDevice, &samplerInfo);
		Move<VkSamplerT>	tmp2Sampler	(vk);

		tmp2Sampler = tmpSampler;

		Unique<VkSamplerT>	sampler		(tmp2Sampler);
	}

	return tcu::TestStatus::pass("Creating sampler succeeded");
}

void createShaderProgs (SourceCollection& dst)
{
	dst.add("test") << glu::VertexSource(
		"#version 300 es\n"
		"in highp vec4 a_position;\n"
		"void main (void) { gl_Position = a_position; }\n");
}

tcu::TestStatus createShader (Context& context)
{
	const VkDevice			vkDevice	= context.getDevice();
	const DeviceInterface&	vk			= context.getDeviceInterface();
	const Unique<VkShaderT>	shader		(createShader(vk, vkDevice, context.getBinaryCollection().get("test"), 0));

	return tcu::TestStatus::pass("Creating shader succeeded");
}

void createTriangleProgs (SourceCollection& dst)
{
	dst.add("vert") << glu::VertexSource(
		"#version 300 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void) { gl_Position = a_position; }\n");
	dst.add("frag") << glu::FragmentSource(
		"#version 300 es\n"
		"layout(location = 0) out lowp vec4 o_color;\n"
		"void main (void) { o_color = vec4(1.0, 0.0, 1.0, 1.0); }\n");
}

tcu::TestStatus renderTriangle (Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	SimpleAllocator							memAlloc				(vk, vkDevice);
	const tcu::IVec2						renderSize				(256, 256);

	const tcu::Vec4							vertices[]				=
	{
		tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
		tcu::Vec4(+0.5f, -0.5f, 0.0f, 1.0f),
		tcu::Vec4( 0.0f, +0.5f, 0.0f, 1.0f)
	};

	const VkBufferCreateInfo				vertexBufferParams		=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,								//	const void*			pNext;
		(VkDeviceSize)sizeof(vertices),			//	VkDeviceSize		size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		//	VkBufferUsageFlags	usage;
		0u,										//	VkBufferCreateFlags	flags;
	};
	const Unique<VkBufferT>					vertexBuffer			(createBuffer(vk, vkDevice, &vertexBufferParams));
	const AllocationList					vertexBufferAllocs		= allocate(memAlloc, getObjectInfo<VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS>(vk, vkDevice, vertexBuffer), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	const VkDeviceSize						imageSizeBytes			= (VkDeviceSize)(sizeof(deUint32)*renderSize.x()*renderSize.y());
	const VkBufferCreateInfo				readImageBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		//	VkStructureType		sType;
		DE_NULL,									//	const void*			pNext;
		imageSizeBytes,								//	VkDeviceSize		size;
		VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT,	//	VkBufferUsageFlags	usage;
		0u,											//	VkBufferCreateFlags	flags;
	};
	const Unique<VkBufferT>					readImageBuffer			(createBuffer(vk, vkDevice, &readImageBufferParams));
	const AllocationList					readImageBufferAllocs	= allocate(memAlloc, getObjectInfo<VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS>(vk, vkDevice, vertexBuffer), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_UNCACHED_BIT);

	const VkImageCreateInfo					imageParams				=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,								//	const void*			pNext;
		VK_IMAGE_TYPE_2D,						//	VkImageType			imageType;
		VK_FORMAT_R8G8B8A8_UNORM,				//	VkFormat			format;
		{ renderSize.x(), renderSize.y(), 1 },	//	VkExtent3D			extent;
		1u,										//	deUint32			mipLevels;
		1u,										//	deUint32			arraySize;
		1u,										//	deUint32			samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling		tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,	//	VkImageUsageFlags	usage;
		0u										//	VkImageCreateFlags	flags;
	};

	const Unique<VkImageT>					image					(createImage(vk, vkDevice, &imageParams));
	const AllocationList					imageAllocs				= allocate(memAlloc, getObjectInfo<VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS>(vk, vkDevice, image));

	const VkColorAttachmentViewCreateInfo	colorAttViewParams		=
	{
		VK_STRUCTURE_TYPE_COLOR_ATTACHMENT_VIEW_CREATE_INFO,	//	VkStructureType			sType;
		DE_NULL,												//	const void*				pNext;
		*image,													//	VkImage					image;
		VK_FORMAT_R8G8B8A8_UNORM,								//	VkFormat				format;
		0u,														//	deUint32				mipLevel;
		0u,														//	deUint32				baseArraySlice;
		1u,														//	deUint32				arraySize;
		DE_NULL,												//	VkImage					msaaResolveImage;
		{
			VK_IMAGE_ASPECT_COLOR,									//	VkImageAspect	aspect;
			0u,														//	deUint32		baseMipLevel;
			1u,														//	deUint32		mipLevels;
			0u,														//	deUint32		baseArraySlice;
			1u,														//	deUint32		arraySize;
		},														//	VkImageSubresourceRange	msaaResolveSubResource;
	};
	const Unique<VkColorAttachmentViewT>	colorAttView			(createColorAttachmentView(vk, vkDevice, &colorAttViewParams));

	const Unique<VkShaderT>					vertShader				(createShader(vk, vkDevice, context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderT>					fragShader				(createShader(vk, vkDevice, context.getBinaryCollection().get("frag"), 0));

	// Pipeline layout
	const VkPipelineLayoutCreateInfo		pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			//	VkStructureType					sType;
		DE_NULL,												//	const void*						pNext;
		0u,														//	deUint32						descriptorSetCount;
		DE_NULL,												//	const VkDescriptorSetLayout*	pSetLayouts;
	};
	const Unique<VkPipelineLayoutT>			pipelineLayout			(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

	// Pipeline
	const VkSpecializationInfo				emptyShaderSpecParams	=
	{
		0u,														//	deUint32						mapEntryCount;
		DE_NULL,												//	const VkSpecializationMapEntry*	pMap;
		DE_NULL,												//	const void*						pData;
	};
	const VkPipelineShaderStageCreateInfo	vertShaderParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,												//	const void*			pNext;
		{
			VK_SHADER_STAGE_VERTEX,									//	VkShaderStage				stage;
			*vertShader,											//	VkShader					shader;
			0u,														//	deUint32					linkConstBufferCount;
			DE_NULL,												//	const VkLinkConstBuffer*	pLinkConstBufferInfo;
			&emptyShaderSpecParams,									//	const VkSpecializationInfo*	pSpecializationInfo;
		}														//	VkPipelineShader	shader;
	};
	const VkPipelineShaderStageCreateInfo	fragShaderParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType		sType;
		&vertShaderParams,										//	const void*			pNext;
		{
			VK_SHADER_STAGE_FRAGMENT,								//	VkShaderStage				stage;
			*fragShader,											//	VkShader					shader;
			0u,														//	deUint32					linkConstBufferCount;
			DE_NULL,												//	const VkLinkConstBuffer*	pLinkConstBufferInfo;
			&emptyShaderSpecParams,									//	const VkSpecializationInfo*	pSpecializationInfo;
		}														//	VkPipelineShader	shader;
	};
	const VkPipelineDsStateCreateInfo		depthStencilParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DS_STATE_CREATE_INFO,		//	VkStructureType		sType;
		&fragShaderParams,										//	const void*			pNext;
		VK_FORMAT_UNDEFINED,									//	VkFormat			format;
		DE_FALSE,												//	deUint32			depthTestEnable;
		DE_FALSE,												//	deUint32			depthWriteEnable;
		VK_COMPARE_OP_ALWAYS,									//	VkCompareOp			depthCompareOp;
		DE_FALSE,												//	deUint32			depthBoundsEnable;
		DE_FALSE,												//	deUint32			stencilTestEnable;
		{
			VK_STENCIL_OP_KEEP,										//	VkStencilOp	stencilFailOp;
			VK_STENCIL_OP_KEEP,										//	VkStencilOp	stencilPassOp;
			VK_STENCIL_OP_KEEP,										//	VkStencilOp	stencilDepthFailOp;
			VK_COMPARE_OP_ALWAYS,									//	VkCompareOp	stencilCompareOp;
		},														//	VkStencilOpState	front;
		{
			VK_STENCIL_OP_KEEP,										//	VkStencilOp	stencilFailOp;
			VK_STENCIL_OP_KEEP,										//	VkStencilOp	stencilPassOp;
			VK_STENCIL_OP_KEEP,										//	VkStencilOp	stencilDepthFailOp;
			VK_COMPARE_OP_ALWAYS,									//	VkCompareOp	stencilCompareOp;
		}														//	VkStencilOpState	back;
	};
	const VkPipelineVpStateCreateInfo		viewportParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VP_STATE_CREATE_INFO,		//	VkStructureType		sType;
		&depthStencilParams,									//	const void*			pNext;
		1u,														//	deUint32			viewportCount;
		VK_COORDINATE_ORIGIN_LOWER_LEFT,						//	VkCoordinateOrigin	clipOrigin;
		VK_DEPTH_MODE_ZERO_TO_ONE,								//	VkDepthMode			depthMode;
	};
	const VkPipelineMsStateCreateInfo		multisampleParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MS_STATE_CREATE_INFO,		//	VkStructureType	sType;
		&viewportParams,										//	const void*		pNext;
		1u,														//	deUint32		samples;
		DE_FALSE,												//	deUint32		multisampleEnable;
		DE_FALSE,												//	deUint32		sampleShadingEnable;
		0.0f,													//	float			minSampleShading;
		~0u,													//	VkSampleMask	sampleMask;
	};
	const VkPipelineCbAttachmentState		colorAttachmentParams	=
	{
		DE_FALSE,																//	deUint32		blendEnable;
		VK_FORMAT_R8G8B8A8_UNORM,												//	VkFormat		format;
		VK_BLEND_ONE,															//	VkBlend			srcBlendColor;
		VK_BLEND_ZERO,															//	VkBlend			destBlendColor;
		VK_BLEND_OP_ADD,														//	VkBlendOp		blendOpColor;
		VK_BLEND_ONE,															//	VkBlend			srcBlendAlpha;
		VK_BLEND_ZERO,															//	VkBlend			destBlendAlpha;
		VK_BLEND_OP_ADD,														//	VkBlendOp		blendOpAlpha;
		VK_CHANNEL_R_BIT|VK_CHANNEL_G_BIT|VK_CHANNEL_B_BIT|VK_CHANNEL_A_BIT,	//	VkChannelFlags	channelWriteMask;
	};
	const VkPipelineCbStateCreateInfo		colorBufferParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_CB_STATE_CREATE_INFO,		//	VkStructureType						sType;
		&multisampleParams,										//	const void*							pNext;
		DE_FALSE,												//	deUint32							alphaToCoverageEnable;
		DE_FALSE,												//	deUint32							logicOpEnable;
		VK_LOGIC_OP_COPY,										//	VkLogicOp							logicOp;
		1u,														//	deUint32							attachmentCount;
		&colorAttachmentParams,									//	const VkPipelineCbAttachmentState*	pAttachments;
	};
	const VkPipelineRsStateCreateInfo		rasterParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RS_STATE_CREATE_INFO,		//	VkStructureType		sType;
		&colorBufferParams,										//	const void*			pNext;
		DE_TRUE,												//	deUint32			depthClipEnable;
		DE_FALSE,												//	deUint32			rasterizerDiscardEnable;
		DE_FALSE,												//	deUint32			programPointSize;
		VK_COORDINATE_ORIGIN_LOWER_LEFT,						//	VkCoordinateOrigin	pointOrigin;
		VK_PROVOKING_VERTEX_FIRST,								//	VkProvokingVertex	provokingVertex;
		VK_FILL_MODE_SOLID,										//	VkFillMode			fillMode;
		VK_CULL_MODE_NONE,										//	VkCullMode			cullMode;
		VK_FRONT_FACE_CCW,										//	VkFrontFace			frontFace;
	};
	const VkPipelineIaStateCreateInfo		inputAssemblerParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_IA_STATE_CREATE_INFO,		//	VkStructureType		sType;
		&rasterParams,											//	const void*			pNext;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,					//	VkPrimitiveTopology	topology;
		DE_FALSE,												//	deUint32			disableVertexReuse;
		DE_FALSE,												//	deUint32			primitiveRestartEnable;
		0u,														//	deUint32			primitiveRestartIndex;
	};
	const VkVertexInputBindingDescription	vertexBinding0			=
	{
		0u,														//	deUint32				binding;
		(deUint32)sizeof(tcu::Vec4),							//	deUint32				strideInBytes;
		VK_VERTEX_INPUT_STEP_RATE_VERTEX,						//	VkVertexInputStepRate	stepRate;
	};
	const VkVertexInputAttributeDescription	vertexAttrib0			=
	{
		0u,														//	deUint32	location;
		0u,														//	deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,							//	VkFormat	format;
		0u,														//	deUint32	offsetInBytes;
	};
	const VkPipelineVertexInputCreateInfo	vertexInputInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_CREATE_INFO,	//	VkStructureType								sType;
		&inputAssemblerParams,									//	const void*									pNext;
		1u,														//	deUint32									bindingCount;
		&vertexBinding0,										//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		1u,														//	deUint32									attributeCount;
		&vertexAttrib0,											//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};
	const VkGraphicsPipelineCreateInfo		pipelineParams			=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,		//	VkStructureType			sType;
		&vertexInputInfo,										//	const void*				pNext;
		0u,														//	VkPipelineCreateFlags	flags;
		*pipelineLayout											//	VkPipelineLayout		layout;
	};

	const Unique<VkPipelineT>				pipeline				(createGraphicsPipeline(vk, vkDevice, &pipelineParams));

	// Framebuffer
	const VkColorAttachmentBindInfo			colorBinding0			=
	{
		*colorAttView,											//	VkColorAttachmentView	view;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				//	VkImageLayout			layout;
	};
	const VkFramebufferCreateInfo			framebufferParams		=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				//	VkStructureType						sType;
		DE_NULL,												//	const void*							pNext;
		1u,														//	deUint32							colorAttachmentCount;
		&colorBinding0,											//	const VkColorAttachmentBindInfo*	pColorAttachments;
		DE_NULL,												//	const VkDepthStencilBindInfo*		pDepthStencilAttachment;
		1u,														//	deUint32							sampleCount;
		(deUint32)renderSize.x(),								//	deUint32							width;
		(deUint32)renderSize.y(),								//	deUint32							height;
		1u,														//	deUint32							layers;
	};
	const Unique<VkFramebufferT>			framebuffer				(createFramebuffer(vk, vkDevice, &framebufferParams));

	// Viewport state
	const VkViewport						viewport0				=
	{
		0.0f,													//	float	originX;
		0.0f,													//	float	originY;
		(float)renderSize.x(),									//	float	width;
		(float)renderSize.y(),									//	float	height;
		0.0f,													//	float	minDepth;
		1.0f,													//	float	maxDepth;
	};
	const VkRect							scissor0				=
	{
		{
			0u,														//	deInt32	x;
			0u,														//	deInt32	y;
		},														//	VkOffset2D	offset;
		{
			renderSize.x(),											//	deInt32	width;
			renderSize.y(),											//	deInt32	height;
		},														//	VkExtent2D	extent;
	};
	const VkDynamicVpStateCreateInfo		viewportStateParams		=
	{
		VK_STRUCTURE_TYPE_DYNAMIC_VP_STATE_CREATE_INFO,			//	VkStructureType		sType;
		DE_NULL,												//	const void*			pNext;
		1u,														//	deUint32			viewportAndScissorCount;
		&viewport0,												//	const VkViewport*	pViewports;
		&scissor0,												//	const VkRect*		pScissors;
	};
	const Unique<VkDynamicVpStateT>			viewportState			(createDynamicViewportState(vk, vkDevice, &viewportStateParams));

	// Render pass
	struct
	{
		VkFormat			format;
		VkImageLayout		layout;
		VkAttachmentLoadOp	loadOp;
		VkAttachmentStoreOp	storeOp;
		VkClearColor		clearColor;
	}										passAttParams			=
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		{
			VkClearColorValue(0.125f, 0.25f, 0.75f, 1.0f),
			DE_FALSE
		},
	};
	const VkRenderPassCreateInfo			renderPassParams		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				//	VkStructureType				sType;
		DE_NULL,												//	const void*					pNext;
		{ { 0u, 0u, }, { renderSize.x(), renderSize.y() } },	//	VkRect						renderArea;
		1u,														//	deUint32					colorAttachmentCount;
		{ renderSize.x(), renderSize.y() },						//	VkExtent2D					extent;
		1u,														//	deUint32					sampleCount;
		1u,														//	deUint32					layers;
		&passAttParams.format,									//	const VkFormat*				pColorFormats;
		&passAttParams.layout,									//	const VkImageLayout*		pColorLayouts;
		&passAttParams.loadOp,									//	const VkAttachmentLoadOp*	pColorLoadOps;
		&passAttParams.storeOp,									//	const VkAttachmentStoreOp*	pColorStoreOps;
		&passAttParams.clearColor,								//	const VkClearColor*			pColorLoadClearValues;
		VK_FORMAT_UNDEFINED,									//	VkFormat					depthStencilFormat;
		VK_IMAGE_LAYOUT_UNDEFINED,								//	VkImageLayout				depthStencilLayout;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,						//	VkAttachmentLoadOp			depthLoadOp;
		0.0f,													//	float						depthLoadClearValue;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,						//	VkAttachmentStoreOp			depthStoreOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,						//	VkAttachmentLoadOp			stencilLoadOp;
		0u,														//	deUint32					stencilLoadClearValue;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,						//	VkAttachmentStoreOp			stencilStoreOp;
	};
	const Unique<VkRenderPassT>				renderPass				(createRenderPass(vk, vkDevice, &renderPassParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,				//	VkStructureType			sType;
		DE_NULL,												//	const void*				pNext;
		context.getUniversalQueueIndex(),						//	deUint32				queueNodeIndex;
		0u,														//	VkCmdBufferCreateFlags	flags;
	};
	const Unique<VkCmdBufferT>				cmdBuf					(createCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCmdBufferBeginInfo				cmdBufBeginParams		=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,				//	VkStructureType				sType;
		DE_NULL,												//	const void*					pNext;
		0u,														//	VkCmdBufferOptimizeFlags	flags;
	};

	// Attach memory
	// \note [pyry] Should be able to do this after creating CmdBuffer but one driver crashes at vkCopyImageToBuffer if memory is not attached at that point
	for (size_t allocNdx = 0; allocNdx < vertexBufferAllocs.size(); allocNdx++)
	{
		const VkDeviceMemory	memory	= vertexBufferAllocs[allocNdx]->getMemory();
		VK_CHECK(vk.queueBindObjectMemory(queue, VK_OBJECT_TYPE_BUFFER, *vertexBuffer, (deUint32)allocNdx, memory, vertexBufferAllocs[allocNdx]->getOffset()));
		VK_CHECK(vk.queueAddMemReferences(queue, 1u, &memory));
	}

	for (size_t allocNdx = 0; allocNdx < readImageBufferAllocs.size(); allocNdx++)
	{
		const VkDeviceMemory	memory	= readImageBufferAllocs[allocNdx]->getMemory();
		VK_CHECK(vk.queueBindObjectMemory(queue, VK_OBJECT_TYPE_BUFFER, *readImageBuffer, (deUint32)allocNdx, memory, readImageBufferAllocs[allocNdx]->getOffset()));
		VK_CHECK(vk.queueAddMemReferences(queue, 1u, &memory));
	}

	for (size_t allocNdx = 0; allocNdx < imageAllocs.size(); allocNdx++)
	{
		const VkDeviceMemory	memory	= imageAllocs[allocNdx]->getMemory();
		VK_CHECK(vk.queueBindObjectMemory(queue, VK_OBJECT_TYPE_IMAGE, *image, (deUint32)allocNdx, memory, imageAllocs[allocNdx]->getOffset()));
		VK_CHECK(vk.queueAddMemReferences(queue, 1u, &memory));
	}

	// \note Only buffers and images are expected to require device memory. Later API revisions make this explicit.
	TCU_CHECK(getObjectInfo<VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS>(vk, vkDevice, pipeline).empty());
	TCU_CHECK(getObjectInfo<VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS>(vk, vkDevice, framebuffer).empty());
	TCU_CHECK(getObjectInfo<VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS>(vk, vkDevice, viewportState).empty());
	TCU_CHECK(getObjectInfo<VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS>(vk, vkDevice, renderPass).empty());
	TCU_CHECK(getObjectInfo<VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS>(vk, vkDevice, cmdBuf).empty());

	// Record commands
	VK_CHECK(vk.beginCommandBuffer(*cmdBuf, &cmdBufBeginParams));

	{
		const VkPipeEvent			pipeEvent			= VK_PIPE_EVENT_TOP_OF_PIPE;
		const VkMemoryBarrier		vertFlushBarrier	=
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,			//	VkStructureType		sType;
			DE_NULL,									//	const void*			pNext;
			VK_MEMORY_OUTPUT_CPU_WRITE_BIT,				//	VkMemoryOutputFlags	outputMask;
			VK_MEMORY_INPUT_VERTEX_ATTRIBUTE_FETCH_BIT,	//	VkMemoryInputFlags	inputMask;
		};
		const VkImageMemoryBarrier	colorAttBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		//	VkStructureType			sType;
			DE_NULL,									//	const void*				pNext;
			0u,											//	VkMemoryOutputFlags		outputMask;
			0u,											//	VkMemoryInputFlags		inputMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout			newLayout;
			*image,										//	VkImage					image;
			{
				VK_IMAGE_ASPECT_COLOR,						//	VkImageAspect	aspect;
				0u,											//	deUint32		baseMipLevel;
				1u,											//	deUint32		mipLevels;
				0u,											//	deUint32		baseArraySlice;
				1u,											//	deUint32		arraySize;
			}											//	VkImageSubresourceRange	subresourceRange;
		};
		const void*				barriers[]				= { &vertFlushBarrier, &colorAttBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_WAIT_EVENT_TOP_OF_PIPE, 1u, &pipeEvent, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	{
		const VkRenderPassBegin	passBeginParams	=
		{
			*renderPass,	//	VkRenderPass	renderPass;
			*framebuffer,	//	VkFramebuffer	framebuffer;
		};
		vk.cmdBeginRenderPass(*cmdBuf, &passBeginParams);
	}

	vk.cmdBindDynamicStateObject(*cmdBuf, VK_STATE_BIND_POINT_VIEWPORT, *viewportState);
	vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	{
		const VkDeviceSize bindingOffset = 0;
		vk.cmdBindVertexBuffers(*cmdBuf, 0u, 1u, &vertexBuffer.get(), &bindingOffset);
	}
	vk.cmdDraw(*cmdBuf, 0u, 3u, 0u, 1u);
	vk.cmdEndRenderPass(*cmdBuf, *renderPass);

	{
		const VkPipeEvent			pipeEvent			= VK_PIPE_EVENT_GRAPHICS_PIPELINE_COMPLETE;
		const VkImageMemoryBarrier	renderFinishBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		//	VkStructureType			sType;
			DE_NULL,									//	const void*				pNext;
			VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT,		//	VkMemoryOutputFlags		outputMask;
			VK_MEMORY_INPUT_TRANSFER_BIT,				//	VkMemoryInputFlags		inputMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,	//	VkImageLayout			newLayout;
			*image,										//	VkImage					image;
			{
				VK_IMAGE_ASPECT_COLOR,						//	VkImageAspect	aspect;
				0u,											//	deUint32		baseMipLevel;
				1u,											//	deUint32		mipLevels;
				0u,											//	deUint32		baseArraySlice;
				1u,											//	deUint32		arraySize;
			}											//	VkImageSubresourceRange	subresourceRange;
		};
		const void*				barriers[]				= { &renderFinishBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_WAIT_EVENT_TOP_OF_PIPE, 1u, &pipeEvent, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	{
		const VkBufferImageCopy	copyParams	=
		{
			(VkDeviceSize)0u,						//	VkDeviceSize		bufferOffset;
			{
				VK_IMAGE_ASPECT_COLOR,					//	VkImageAspect	aspect;
				0u,										//	deUint32		mipLevel;
				0u,										//	deUint32		arraySlice;
			},										//	VkImageSubresource	imageSubresource;
			{ 0u, 0u, 0u },							//	VkOffset3D			imageOffset;
			{ renderSize.x(), renderSize.y(), 1u }	//	VkExtent3D			imageExtent;
		};
		vk.cmdCopyImageToBuffer(*cmdBuf, *image, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, *readImageBuffer, 1u, &copyParams);
	}

	{
		const VkPipeEvent			pipeEvent			= VK_PIPE_EVENT_TRANSFER_COMPLETE;
		const VkBufferMemoryBarrier	copyFinishBarrier	=
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//	VkStructureType		sType;
			DE_NULL,									//	const void*			pNext;
			VK_MEMORY_OUTPUT_TRANSFER_BIT,				//	VkMemoryOutputFlags	outputMask;
			VK_MEMORY_INPUT_CPU_READ_BIT,				//	VkMemoryInputFlags	inputMask;
			*readImageBuffer,							//	VkBuffer			buffer;
			0u,											//	VkDeviceSize		offset;
			imageSizeBytes								//	VkDeviceSize		size;
		};
		const void*				barriers[]				= { &copyFinishBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_WAIT_EVENT_TOP_OF_PIPE, 1u, &pipeEvent, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	VK_CHECK(vk.endCommandBuffer(*cmdBuf));

	// Upload vertex data
	{
		void*	vertexBufPtr	= DE_NULL;

		VK_CHECK(vk.mapMemory(vkDevice, vertexBufferAllocs[0]->getMemory(), vertexBufferAllocs[0]->getOffset(), (VkDeviceSize)sizeof(vertices), 0u, &vertexBufPtr));
		deMemcpy(vertexBufPtr, &vertices[0], sizeof(vertices));
		VK_CHECK(vk.flushMappedMemory(vkDevice, vertexBufferAllocs[0]->getMemory(), vertexBufferAllocs[0]->getOffset(), (VkDeviceSize)sizeof(vertices)));
		VK_CHECK(vk.unmapMemory(vkDevice, vertexBufferAllocs[0]->getMemory()));
	}

	// Submit & wait for completion
	{
		const VkFenceCreateInfo	fenceParams	=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	//	VkStructureType		sType;
			DE_NULL,								//	const void*			pNext;
			0u,										//	VkFenceCreateFlags	flags;
		};
		const Unique<VkFenceT>	fence		(createFence(vk, vkDevice, &fenceParams));

		VK_CHECK(vk.queueSubmit(queue, 1u, &cmdBuf.get(), *fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	// Map & log image
	{
		void*	imagePtr	= DE_NULL;

		VK_CHECK(vk.mapMemory(vkDevice, readImageBufferAllocs[0]->getMemory(), readImageBufferAllocs[0]->getOffset(), imageSizeBytes, 0u, &imagePtr));
		context.getTestContext().getLog() << TestLog::Image("Result", "Result", tcu::ConstPixelBufferAccess(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), renderSize.x(), renderSize.y(), 1, imagePtr));
		VK_CHECK(vk.unmapMemory(vkDevice, readImageBufferAllocs[0]->getMemory()));
	}

	return tcu::TestStatus::pass("Rendering succeeded");
}

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	apiTests	(new tcu::TestCaseGroup(testCtx, "api", "API Tests"));

	addFunctionCase				(apiTests.get(), "create_sampler",	"",	createSampler);
	addFunctionCaseWithPrograms	(apiTests.get(), "create_shader",	"", createShaderProgs,		createShader);
	addFunctionCaseWithPrograms	(apiTests.get(), "triangle",		"", createTriangleProgs,	renderTriangle);

	return apiTests.release();
}

} // api
} // vkt
