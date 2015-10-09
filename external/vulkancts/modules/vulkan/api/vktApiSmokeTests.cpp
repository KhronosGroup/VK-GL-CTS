/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \brief Simple Smoke Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiTests.hpp"

#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace api
{

namespace
{

using namespace vk;
using std::vector;
using tcu::TestLog;
using de::UniquePtr;

tcu::TestStatus createSamplerTest (Context& context)
{
	const VkDevice			vkDevice	= context.getDevice();
	const DeviceInterface&	vk			= context.getDeviceInterface();

	{
		const struct VkSamplerCreateInfo		samplerInfo	=
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		//	VkStructureType		sType;
			DE_NULL,									//	const void*			pNext;
			VK_TEX_FILTER_NEAREST,						//	VkTexFilter			magFilter;
			VK_TEX_FILTER_NEAREST,						//	VkTexFilter			minFilter;
			VK_TEX_MIPMAP_MODE_BASE,					//	VkTexMipmapMode		mipMode;
			VK_TEX_ADDRESS_MODE_CLAMP,					//	VkTexAddressMode	addressU;
			VK_TEX_ADDRESS_MODE_CLAMP,					//	VkTexAddressMode	addressV;
			VK_TEX_ADDRESS_MODE_CLAMP,					//	VkTexAddressMode	addressW;
			0.0f,										//	float				mipLodBias;
			0.0f,										//	float				maxAnisotropy;
			DE_FALSE,									//	VkBool32			compareEnable;
			VK_COMPARE_OP_ALWAYS,						//	VkCompareOp			compareOp;
			0.0f,										//	float				minLod;
			0.0f,										//	float				maxLod;
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	//	VkBorderColor		borderColor;
			VK_FALSE,									//	VKBool32			unnormalizedCoords;
		};

		Move<VkSampler>			tmpSampler	= createSampler(vk, vkDevice, &samplerInfo);
		Move<VkSampler>			tmp2Sampler;

		tmp2Sampler = tmpSampler;

		const Unique<VkSampler>	sampler		(tmp2Sampler);
	}

	return tcu::TestStatus::pass("Creating sampler succeeded");
}

void createShaderProgs (SourceCollections& dst)
{
	dst.glslSources.add("test") << glu::VertexSource(
		"#version 300 es\n"
		"in highp vec4 a_position;\n"
		"void main (void) { gl_Position = a_position; }\n");
}

tcu::TestStatus createShaderModuleTest (Context& context)
{
	const VkDevice					vkDevice	= context.getDevice();
	const DeviceInterface&			vk			= context.getDeviceInterface();
	const Unique<VkShaderModule>	shader		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("test"), 0));

	return tcu::TestStatus::pass("Creating shader module succeeded");
}

void createTriangleAsmProgs (SourceCollections& dst)
{
	dst.spirvAsmSources.add("vert") <<
		"		 OpSource ESSL 300\n"
		"		 OpCapability Shader\n"
		"%1 =	 OpExtInstImport \"GLSL.std.450\"\n"
		"		 OpMemoryModel Logical GLSL450\n"
		"		 OpEntryPoint Vertex %4 \"main\" %10 %12 %16 %17\n"
		"		 OpName %4 \"main\"\n"
		"		 OpName %10 \"gl_Position\"\n"
		"		 OpName %12 \"a_position\"\n"
		"		 OpName %16 \"gl_VertexID\"\n"
		"		 OpName %17 \"gl_InstanceID\"\n"
		"		 OpDecorate %10 BuiltIn Position\n"
		"		 OpDecorate %12 Location 0\n"
		"		 OpDecorate %16 BuiltIn VertexId\n"
		"		 OpDecorate %17 BuiltIn InstanceId\n"
		"%2 =	 OpTypeVoid\n"
		"%3 =	 OpTypeFunction %2\n"
		"%7 =	 OpTypeFloat 32\n"
		"%8 =	 OpTypeVector %7 4\n"
		"%9 =	 OpTypePointer Output %8\n"
		"%10 =	 OpVariable %9 Output\n"
		"%11 =	 OpTypePointer Input %8\n"
		"%12 =	 OpVariable %11 Input\n"
		"%14 =	 OpTypeInt 32 1\n"
		"%15 =	 OpTypePointer Input %14\n"
		"%16 =	 OpVariable %15 Input\n"
		"%17 =	 OpVariable %15 Input\n"
		"%4 =	 OpFunction %2 None %3\n"
		"%5 =	 OpLabel\n"
		"%13 =	 OpLoad %8 %12\n"
		"		 OpStore %10 %13\n"
		"		 OpBranch %6\n"
		"%6 =	 OpLabel\n"
		"		 OpReturn\n"
		"		 OpFunctionEnd\n";
	dst.spirvAsmSources.add("frag") <<
		"		OpSource ESSL 300\n"
		"		OpCapability Shader\n"
		"%1 =	OpExtInstImport \"GLSL.std.450\"\n"
		"		OpMemoryModel Logical GLSL450\n"
		"		OpEntryPoint Fragment %4 \"main\" %10\n"
		"		OpExecutionMode %4 OriginLowerLeft\n"
		"		OpName %4 \"main\"\n"
		"		OpName %10 \"o_color\"\n"
		"		OpDecorate %10 RelaxedPrecision\n"
		"		OpDecorate %10 Location 0\n"
		"%2 =	OpTypeVoid\n"
		"%3 =	OpTypeFunction %2\n"
		"%7 =	OpTypeFloat 32\n"
		"%8 =	OpTypeVector %7 4\n"
		"%9 =	OpTypePointer Output %8\n"
		"%10 =	OpVariable %9 Output\n"
		"%11 =	OpConstant %7 1065353216\n"
		"%12 =	OpConstant %7 0\n"
		"%13 =	OpConstantComposite %8 %11 %12 %11 %11\n"
		"%4 =	OpFunction %2 None %3\n"
		"%5 =	OpLabel\n"
		"		OpStore %10 %13\n"
		"		OpBranch %6\n"
		"%6 =	OpLabel\n"
		"		OpReturn\n"
		"		OpFunctionEnd\n";
}

void createTriangleProgs (SourceCollections& dst)
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 300 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void) { gl_Position = a_position; }\n");
	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 300 es\n"
		"layout(location = 0) out lowp vec4 o_color;\n"
		"void main (void) { o_color = vec4(1.0, 0.0, 1.0, 1.0); }\n");
}

tcu::TestStatus renderTriangleTest (Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator							memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
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
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode		sharingMode;
		1u,										//	deUint32			queueFamilyCount;
		&queueFamilyIndex,						//	const deUint32*		pQueueFamilyIndices;
	};
	const Unique<VkBuffer>					vertexBuffer			(createBuffer(vk, vkDevice, &vertexBufferParams));
	const UniquePtr<Allocation>				vertexBufferMemory		(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

	const VkDeviceSize						imageSizeBytes			= (VkDeviceSize)(sizeof(deUint32)*renderSize.x()*renderSize.y());
	const VkBufferCreateInfo				readImageBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		//	VkStructureType		sType;
		DE_NULL,									//	const void*			pNext;
		imageSizeBytes,								//	VkDeviceSize		size;
		VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT,	//	VkBufferUsageFlags	usage;
		0u,											//	VkBufferCreateFlags	flags;
		VK_SHARING_MODE_EXCLUSIVE,					//	VkSharingMode		sharingMode;
		1u,											//	deUint32			queueFamilyCount;
		&queueFamilyIndex,							//	const deUint32*		pQueueFamilyIndices;
	};
	const Unique<VkBuffer>					readImageBuffer			(createBuffer(vk, vkDevice, &readImageBufferParams));
	const UniquePtr<Allocation>				readImageBufferMemory	(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

	const VkImageCreateInfo					imageParams				=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									//	VkStructureType		sType;
		DE_NULL,																//	const void*			pNext;
		VK_IMAGE_TYPE_2D,														//	VkImageType			imageType;
		VK_FORMAT_R8G8B8A8_UNORM,												//	VkFormat			format;
		{ renderSize.x(), renderSize.y(), 1 },									//	VkExtent3D			extent;
		1u,																		//	deUint32			mipLevels;
		1u,																		//	deUint32			arraySize;
		1u,																		//	deUint32			samples;
		VK_IMAGE_TILING_OPTIMAL,												//	VkImageTiling		tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT,	//	VkImageUsageFlags	usage;
		0u,																		//	VkImageCreateFlags	flags;
		VK_SHARING_MODE_EXCLUSIVE,												//	VkSharingMode		sharingMode;
		1u,																		//	deUint32			queueFamilyCount;
		&queueFamilyIndex,														//	const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,												//	VkImageLayout		initialLayout;
	};

	const Unique<VkImage>					image					(createImage(vk, vkDevice, &imageParams));
	const UniquePtr<Allocation>				imageMemory				(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any));

	VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageMemory->getMemory(), imageMemory->getOffset()));

	const VkAttachmentDescription			colorAttDesc			=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION,		//	VkStructureType					sType;
		DE_NULL,										//	const void*						pNext;
		VK_FORMAT_R8G8B8A8_UNORM,						//	VkFormat						format;
		1u,												//	deUint32						samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,					//	VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,					//	VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,				//	VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,				//	VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//	VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//	VkImageLayout					finalLayout;
		0u,												//	VkAttachmentDescriptionFlags	flags;
	};
	const VkAttachmentReference				colorAttRef				=
	{
		0u,												//	deUint32		attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//	VkImageLayout	layout;
	};
	const VkSubpassDescription				subpassDesc				=
	{
		VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION,			//	VkStructureType					sType;
		DE_NULL,										//	const void*						pNext;
		VK_PIPELINE_BIND_POINT_GRAPHICS,				//	VkPipelineBindPoint				pipelineBindPoint;
		0u,												//	VkSubpassDescriptionFlags		flags;
		0u,												//	deUint32						inputCount;
		DE_NULL,										//	const VkAttachmentReference*	pInputAttachments;
		1u,												//	deUint32						colorCount;
		&colorAttRef,									//	const VkAttachmentReference*	pColorAttachments;
		DE_NULL,										//	const VkAttachmentReference*	pResolveAttachments;
		{ VK_NO_ATTACHMENT, VK_IMAGE_LAYOUT_GENERAL },	//	VkAttachmentReference			depthStencilAttachment;
		0u,												//	deUint32						preserveCount;
		DE_NULL,										//	const VkAttachmentReference*	pPreserveAttachments;

	};
	const VkRenderPassCreateInfo			renderPassParams		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		//	VkStructureType					sType;
		DE_NULL,										//	const void*						pNext;
		1u,												//	deUint32						attachmentCount;
		&colorAttDesc,									//	const VkAttachmentDescription*	pAttachments;
		1u,												//	deUint32						subpassCount;
		&subpassDesc,									//	const VkSubpassDescription*		pSubpasses;
		0u,												//	deUint32						dependencyCount;
		DE_NULL,										//	const VkSubpassDependency*		pDependencies;
	};
	const Unique<VkRenderPass>				renderPass				(createRenderPass(vk, vkDevice, &renderPassParams));

	const VkImageViewCreateInfo				colorAttViewParams		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		//	VkStructureType				sType;
		DE_NULL,										//	const void*					pNext;
		*image,											//	VkImage						image;
		VK_IMAGE_VIEW_TYPE_2D,							//	VkImageViewType				viewType;
		VK_FORMAT_R8G8B8A8_UNORM,						//	VkFormat					format;
		{
			VK_CHANNEL_SWIZZLE_R,
			VK_CHANNEL_SWIZZLE_G,
			VK_CHANNEL_SWIZZLE_B,
			VK_CHANNEL_SWIZZLE_A
		},												//	VkChannelMapping			channels;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,						//	VkImageAspectFlags	aspectMask;
			0u,												//	deUint32			baseMipLevel;
			1u,												//	deUint32			mipLevels;
			0u,												//	deUint32			baseArrayLayer;
			1u,												//	deUint32			arraySize;
		},												//	VkImageSubresourceRange		subresourceRange;
		0u,												//	VkImageViewCreateFlags		flags;
	};
	const Unique<VkImageView>				colorAttView			(createImageView(vk, vkDevice, &colorAttViewParams));

	const Unique<VkShaderModule>			vertShaderModule		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("vert"), 0));
	const VkShaderCreateInfo				vertShaderParams		=
	{
		VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,			//	VkStructureType		sType;
		DE_NULL,										//	const void*			pNext;
		*vertShaderModule,								//	VkShaderModule		module;
		"main",											//	const char*			pName;
		0u,												//	VkShaderCreateFlags	flags;
		VK_SHADER_STAGE_VERTEX,							//	VkShaderStage		stage;
	};
	const Unique<VkShader>					vertShader				(createShader(vk, vkDevice, &vertShaderParams));
	const Unique<VkShaderModule>			fragShaderModule		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("frag"), 0));
	const VkShaderCreateInfo				fragShaderParams		=
	{
		VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,			//	VkStructureType		sType;
		DE_NULL,										//	const void*			pNext;
		*fragShaderModule,								//	VkShaderModule		module;
		"main",											//	const char*			pName;
		0u,												//	VkShaderCreateFlags	flags;
		VK_SHADER_STAGE_FRAGMENT,						//	VkShaderStage		stage;
	};
	const Unique<VkShader>					fragShader				(createShader(vk, vkDevice, &fragShaderParams));

	// Pipeline layout
	const VkPipelineLayoutCreateInfo		pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			//	VkStructureType					sType;
		DE_NULL,												//	const void*						pNext;
		0u,														//	deUint32						descriptorSetCount;
		DE_NULL,												//	const VkDescriptorSetLayout*	pSetLayouts;
		0u,														//	deUint32						pushConstantRangeCount;
		DE_NULL,												//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const Unique<VkPipelineLayout>			pipelineLayout			(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

	// Pipeline
	const VkSpecializationInfo				emptyShaderSpecParams	=
	{
		0u,														//	deUint32						mapEntryCount;
		DE_NULL,												//	const VkSpecializationMapEntry*	pMap;
		0,														//	const deUintptr					dataSize;
		DE_NULL,												//	const void*						pData;
	};
	const VkPipelineShaderStageCreateInfo	shaderStageParams[]	=
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType				sType;
			DE_NULL,												//	const void*					pNext;
			VK_SHADER_STAGE_VERTEX,									//	VkShaderStage				stage;
			*vertShader,											//	VkShader					shader;
			&emptyShaderSpecParams,									//	const VkSpecializationInfo*	pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType				sType;
			DE_NULL,												//	const void*					pNext;
			VK_SHADER_STAGE_FRAGMENT,								//	VkShaderStage				stage;
			*fragShader,											//	VkShader					shader;
			&emptyShaderSpecParams,									//	const VkSpecializationInfo*	pSpecializationInfo;
		}
	};
	const VkPipelineDepthStencilStateCreateInfo	depthStencilParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,													//	const void*			pNext;
		DE_FALSE,													//	deUint32			depthTestEnable;
		DE_FALSE,													//	deUint32			depthWriteEnable;
		VK_COMPARE_OP_ALWAYS,										//	VkCompareOp			depthCompareOp;
		DE_FALSE,													//	deUint32			depthBoundsTestEnable;
		DE_FALSE,													//	deUint32			stencilTestEnable;
		{
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilFailOp;
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilPassOp;
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilDepthFailOp;
			VK_COMPARE_OP_ALWAYS,										//	VkCompareOp	stencilCompareOp;
			0u,															//	deUint32	stencilCompareMask;
			0u,															//	deUint32	stencilWriteMask;
			0u,															//	deUint32	stencilReference;
		},															//	VkStencilOpState	front;
		{
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilFailOp;
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilPassOp;
			VK_STENCIL_OP_KEEP,											//	VkStencilOp	stencilDepthFailOp;
			VK_COMPARE_OP_ALWAYS,										//	VkCompareOp	stencilCompareOp;
			0u,															//	deUint32	stencilCompareMask;
			0u,															//	deUint32	stencilWriteMask;
			0u,															//	deUint32	stencilReference;
		},															//	VkStencilOpState	back;
		-1.0f,														//	float				minDepthBounds;
		+1.0f,														//	float				maxDepthBounds;
	};
	const VkViewport						viewport0				=
	{
		0.0f,														//	float	originX;
		0.0f,														//	float	originY;
		(float)renderSize.x(),										//	float	width;
		(float)renderSize.y(),										//	float	height;
		0.0f,														//	float	minDepth;
		1.0f,														//	float	maxDepth;
	};
	const VkRect2D							scissor0				=
	{
		{
			0u,															//	deInt32	x;
			0u,															//	deInt32	y;
		},															//	VkOffset2D	offset;
		{
			renderSize.x(),												//	deInt32	width;
			renderSize.y(),												//	deInt32	height;
		},															//	VkExtent2D	extent;
	};
	const VkPipelineViewportStateCreateInfo		viewportParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,		//	VkStructureType		sType;
		DE_NULL,													//	const void*			pNext;
		1u,															//	deUint32			viewportCount;
		&viewport0,
		1u,
		&scissor0
	};
	const VkSampleMask							sampleMask				= ~0u;
	const VkPipelineMultisampleStateCreateInfo	multisampleParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType	sType;
		DE_NULL,													//	const void*		pNext;
		1u,															//	deUint32		rasterSamples;
		DE_FALSE,													//	deUint32		sampleShadingEnable;
		0.0f,														//	float			minSampleShading;
		&sampleMask,												//	VkSampleMask	sampleMask;
	};
	const VkPipelineRasterStateCreateInfo		rasterParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO,	//	VkStructureType	sType;
		DE_NULL,												//	const void*		pNext;
		DE_TRUE,												//	deUint32		depthClipEnable;
		DE_FALSE,												//	deUint32		rasterizerDiscardEnable;
		VK_FILL_MODE_SOLID,										//	VkFillMode		fillMode;
		VK_CULL_MODE_NONE,										//	VkCullMode		cullMode;
		VK_FRONT_FACE_CCW,										//	VkFrontFace		frontFace;
		VK_FALSE,												//	VkBool32		depthBiasEnable;
		0.0f,													//	float			depthBias;
		0.0f,													//	float			depthBiasClamp;
		0.0f,													//	float			slopeScaledDepthBias;
		1.0f,													//	float			lineWidth;
	};
	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType		sType;
		DE_NULL,														//	const void*			pNext;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							//	VkPrimitiveTopology	topology;
		DE_FALSE,														//	deUint32			primitiveRestartEnable;
	};
	const VkVertexInputBindingDescription		vertexBinding0			=
	{
		0u,														//	deUint32				binding;
		(deUint32)sizeof(tcu::Vec4),							//	deUint32				strideInBytes;
		VK_VERTEX_INPUT_STEP_RATE_VERTEX,						//	VkVertexInputStepRate	stepRate;
	};
	const VkVertexInputAttributeDescription		vertexAttrib0			=
	{
		0u,														//	deUint32	location;
		0u,														//	deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,							//	VkFormat	format;
		0u,														//	deUint32	offsetInBytes;
	};
	const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		DE_NULL,													//	const void*									pNext;
		1u,															//	deUint32									bindingCount;
		&vertexBinding0,											//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		1u,															//	deUint32									attributeCount;
		&vertexAttrib0,												//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};
	const VkPipelineColorBlendAttachmentState	attBlendParams			=
	{
		DE_FALSE,																//	deUint32		blendEnable;
		VK_BLEND_ONE,															//	VkBlend			srcBlendColor;
		VK_BLEND_ZERO,															//	VkBlend			destBlendColor;
		VK_BLEND_OP_ADD,														//	VkBlendOp		blendOpColor;
		VK_BLEND_ONE,															//	VkBlend			srcBlendAlpha;
		VK_BLEND_ZERO,															//	VkBlend			destBlendAlpha;
		VK_BLEND_OP_ADD,														//	VkBlendOp		blendOpAlpha;
		VK_CHANNEL_R_BIT|VK_CHANNEL_G_BIT|VK_CHANNEL_B_BIT|VK_CHANNEL_A_BIT,	//	VkChannelFlags	channelWriteMask;
	};
	const VkPipelineColorBlendStateCreateInfo	blendParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		DE_NULL,													//	const void*									pNext;
		DE_FALSE,													//	VkBool32									alphaToCoverageEnable;
		DE_FALSE,													//	VkBool32									alphaToOneEnable;
		DE_FALSE,													//	VkBool32									logicOpEnable;
		VK_LOGIC_OP_COPY,											//	VkLogicOp									logicOp;
		1u,															//	deUint32									attachmentCount;
		&attBlendParams,											//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									//	float										blendConst[4];
	};
	const VkPipelineDynamicStateCreateInfo	dynamicStateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType			sType;
		DE_NULL,												//	const void*				pNext;
		0u,														//	deUint32				dynamicStateCount;
		DE_NULL													//	const VkDynamicState*	pDynamicStates;
	};
	const VkGraphicsPipelineCreateInfo		pipelineParams			=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,		//	VkStructureType									sType;
		DE_NULL,												//	const void*										pNext;
		(deUint32)DE_LENGTH_OF_ARRAY(shaderStageParams),		//	deUint32										stageCount;
		shaderStageParams,										//	const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateParams,								//	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyParams,									//	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,												//	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
		&viewportParams,										//	const VkPipelineViewportStateCreateInfo*		pViewportState;
		&rasterParams,											//	const VkPipelineRasterStateCreateInfo*			pRasterState;
		&multisampleParams,										//	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&depthStencilParams,									//	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
		&blendParams,											//	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		&dynamicStateInfo,										//	const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		0u,														//	VkPipelineCreateFlags							flags;
		*pipelineLayout,										//	VkPipelineLayout								layout;
		*renderPass,											//	VkRenderPass									renderPass;
		0u,														//	deUint32										subpass;
		DE_NULL,												//	VkPipeline										basePipelineHandle;
		0u,														//	deInt32											basePipelineIndex;
	};

	const Unique<VkPipeline>				pipeline				(createGraphicsPipeline(vk, vkDevice, DE_NULL, &pipelineParams));

	// Framebuffer
	const VkFramebufferCreateInfo			framebufferParams		=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				//	VkStructureType		sType;
		DE_NULL,												//	const void*			pNext;
		*renderPass,											//	VkRenderPass		renderPass;
		1u,														//	deUint32			attachmentCount;
		&*colorAttView,											//	const VkImageView*	pAttachments;
		(deUint32)renderSize.x(),								//	deUint32			width;
		(deUint32)renderSize.y(),								//	deUint32			height;
		1u,														//	deUint32			layers;
	};
	const Unique<VkFramebuffer>				framebuffer				(createFramebuffer(vk, vkDevice, &framebufferParams));

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,				//	VkStructureType			sType;
		DE_NULL,												//	const void*				pNext;
		*cmdPool,												//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,							//	VkCmdBufferLevel		level;
		0u,														//	VkCmdBufferCreateFlags	flags;
	};
	const Unique<VkCmdBuffer>				cmdBuf					(createCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCmdBufferBeginInfo				cmdBufBeginParams		=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,				//	VkStructureType				sType;
		DE_NULL,												//	const void*					pNext;
		0u,														//	VkCmdBufferOptimizeFlags	flags;
		DE_NULL,												//	VkRenderPass				renderPass;
		0u,														//	deUint32					subpass;
		DE_NULL,												//	VkFramebuffer				framebuffer;
	};

	// Record commands
	VK_CHECK(vk.beginCommandBuffer(*cmdBuf, &cmdBufBeginParams));

	{
		const VkMemoryBarrier		vertFlushBarrier	=
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,			//	VkStructureType		sType;
			DE_NULL,									//	const void*			pNext;
			VK_MEMORY_OUTPUT_HOST_WRITE_BIT,			//	VkMemoryOutputFlags	outputMask;
			VK_MEMORY_INPUT_VERTEX_ATTRIBUTE_FETCH_BIT,	//	VkMemoryInputFlags	inputMask;
		};
		const VkImageMemoryBarrier	colorAttBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		//	VkStructureType			sType;
			DE_NULL,									//	const void*				pNext;
			0u,											//	VkMemoryOutputFlags		outputMask;
			VK_MEMORY_INPUT_COLOR_ATTACHMENT_BIT,		//	VkMemoryInputFlags		inputMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout			newLayout;
			queueFamilyIndex,							//	deUint32				srcQueueFamilyIndex;
			queueFamilyIndex,							//	deUint32				destQueueFamilyIndex;
			*image,										//	VkImage					image;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					//	VkImageAspect	aspect;
				0u,											//	deUint32		baseMipLevel;
				1u,											//	deUint32		mipLevels;
				0u,											//	deUint32		baseArraySlice;
				1u,											//	deUint32		arraySize;
			}											//	VkImageSubresourceRange	subresourceRange;
		};
		const void*				barriers[]				= { &vertFlushBarrier, &colorAttBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_GPU_COMMANDS, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	{
		const VkClearValue			clearValue		= makeClearValueColorF32(0.125f, 0.25f, 0.75f, 1.0f);
		const VkRenderPassBeginInfo	passBeginParams	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,			//	VkStructureType		sType;
			DE_NULL,											//	const void*			pNext;
			*renderPass,										//	VkRenderPass		renderPass;
			*framebuffer,										//	VkFramebuffer		framebuffer;
			{ { 0, 0 }, { renderSize.x(), renderSize.y() } },	//	VkRect2D			renderArea;
			1u,													//	deUint32			clearValueCount;
			&clearValue,										//	const VkClearValue*	pClearValues;
		};
		vk.cmdBeginRenderPass(*cmdBuf, &passBeginParams, VK_RENDER_PASS_CONTENTS_INLINE);
	}

	vk.cmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	{
		const VkDeviceSize bindingOffset = 0;
		vk.cmdBindVertexBuffers(*cmdBuf, 0u, 1u, &vertexBuffer.get(), &bindingOffset);
	}
	vk.cmdDraw(*cmdBuf, 3u, 1u, 0u, 0u);
	vk.cmdEndRenderPass(*cmdBuf);

	{
		const VkImageMemoryBarrier	renderFinishBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		//	VkStructureType			sType;
			DE_NULL,									//	const void*				pNext;
			VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT,		//	VkMemoryOutputFlags		outputMask;
			VK_MEMORY_INPUT_TRANSFER_BIT,				//	VkMemoryInputFlags		inputMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,	//	VkImageLayout			newLayout;
			queueFamilyIndex,							//	deUint32				srcQueueFamilyIndex;
			queueFamilyIndex,							//	deUint32				destQueueFamilyIndex;
			*image,										//	VkImage					image;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					//	VkImageAspectFlags	aspectMask;
				0u,											//	deUint32			baseMipLevel;
				1u,											//	deUint32			mipLevels;
				0u,											//	deUint32			baseArraySlice;
				1u,											//	deUint32			arraySize;
			}											//	VkImageSubresourceRange	subresourceRange;
		};
		const void*				barriers[]				= { &renderFinishBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_ALL_GRAPHICS, VK_PIPELINE_STAGE_TRANSFER_BIT, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	{
		const VkBufferImageCopy	copyParams	=
		{
			(VkDeviceSize)0u,						//	VkDeviceSize			bufferOffset;
			(deUint32)renderSize.x(),				//	deUint32				bufferRowLength;
			(deUint32)renderSize.y(),				//	deUint32				bufferImageHeight;
			{
				VK_IMAGE_ASPECT_COLOR,					//	VkImageAspect		aspect;
				0u,										//	deUint32			mipLevel;
				0u,										//	deUint32			arrayLayer;
				1u,										//	deUint32			arraySize;
			},										//	VkImageSubresourceCopy	imageSubresource;
			{ 0u, 0u, 0u },							//	VkOffset3D				imageOffset;
			{ renderSize.x(), renderSize.y(), 1u }	//	VkExtent3D				imageExtent;
		};
		vk.cmdCopyImageToBuffer(*cmdBuf, *image, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, *readImageBuffer, 1u, &copyParams);
	}

	{
		const VkBufferMemoryBarrier	copyFinishBarrier	=
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//	VkStructureType		sType;
			DE_NULL,									//	const void*			pNext;
			VK_MEMORY_OUTPUT_TRANSFER_BIT,				//	VkMemoryOutputFlags	outputMask;
			VK_MEMORY_INPUT_HOST_READ_BIT,				//	VkMemoryInputFlags	inputMask;
			queueFamilyIndex,							//	deUint32			srcQueueFamilyIndex;
			queueFamilyIndex,							//	deUint32			destQueueFamilyIndex;
			*readImageBuffer,							//	VkBuffer			buffer;
			0u,											//	VkDeviceSize		offset;
			imageSizeBytes								//	VkDeviceSize		size;
		};
		const void*				barriers[]				= { &copyFinishBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	VK_CHECK(vk.endCommandBuffer(*cmdBuf));

	// Upload vertex data
	{
		const VkMappedMemoryRange	range			=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//	VkStructureType	sType;
			DE_NULL,								//	const void*		pNext;
			vertexBufferMemory->getMemory(),		//	VkDeviceMemory	mem;
			0,										//	VkDeviceSize	offset;
			(VkDeviceSize)sizeof(vertices),			//	VkDeviceSize	size;
		};
		void*						vertexBufPtr	= vertexBufferMemory->getHostPtr();

		deMemcpy(vertexBufPtr, &vertices[0], sizeof(vertices));
		VK_CHECK(vk.flushMappedMemoryRanges(vkDevice, 1u, &range));
	}

	// Submit & wait for completion
	{
		const VkFenceCreateInfo	fenceParams	=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	//	VkStructureType		sType;
			DE_NULL,								//	const void*			pNext;
			0u,										//	VkFenceCreateFlags	flags;
		};
		const Unique<VkFence>	fence		(createFence(vk, vkDevice, &fenceParams));

		VK_CHECK(vk.queueSubmit(queue, 1u, &cmdBuf.get(), *fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	// Log image
	{
		const VkMappedMemoryRange	range		=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//	VkStructureType	sType;
			DE_NULL,								//	const void*		pNext;
			readImageBufferMemory->getMemory(),		//	VkDeviceMemory	mem;
			0,										//	VkDeviceSize	offset;
			imageSizeBytes,							//	VkDeviceSize	size;
		};
		void*						imagePtr	= readImageBufferMemory->getHostPtr();

		VK_CHECK(vk.invalidateMappedMemoryRanges(vkDevice, 1u, &range));
		context.getTestContext().getLog() << TestLog::Image("Result", "Result", tcu::ConstPixelBufferAccess(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), renderSize.x(), renderSize.y(), 1, imagePtr));
	}

	return tcu::TestStatus::pass("Rendering succeeded");
}

} // anonymous

tcu::TestCaseGroup* createSmokeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	smokeTests	(new tcu::TestCaseGroup(testCtx, "smoke", "Smoke Tests"));

	addFunctionCase				(smokeTests.get(), "create_sampler",	"",	createSamplerTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "create_shader",		"", createShaderProgs,		createShaderModuleTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "triangle",			"", createTriangleProgs,	renderTriangleTest);
	addFunctionCaseWithPrograms	(smokeTests.get(), "asm_triangle",		"", createTriangleAsmProgs,	renderTriangleTest);

	return smokeTests.release();
}

} // api
} // vkt
