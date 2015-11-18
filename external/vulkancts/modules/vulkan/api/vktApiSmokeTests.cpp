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
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// sType
			DE_NULL,									// pNext
			0u,											// flags
			VK_FILTER_NEAREST,							// magFilter
			VK_FILTER_NEAREST,							// minFilter
			VK_SAMPLER_MIPMAP_MODE_BASE,				// mipmapMode
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeU
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeV
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeW
			0.0f,										// mipLodBias
			0.0f,										// maxAnisotropy
			DE_FALSE,									// compareEnable
			VK_COMPARE_OP_ALWAYS,						// compareOp
			0.0f,										// minLod
			0.0f,										// maxLod
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// borderColor
			VK_FALSE,									// unnormalizedCoords
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
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		DE_NULL,								// pNext
		0u,										// flags
		(VkDeviceSize)sizeof(vertices),			// size
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		1u,										// queueFamilyIndexCount
		&queueFamilyIndex,						// pQueueFamilyIndices
	};
	const Unique<VkBuffer>					vertexBuffer			(createBuffer(vk, vkDevice, &vertexBufferParams));
	const UniquePtr<Allocation>				vertexBufferMemory		(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));

	const VkDeviceSize						imageSizeBytes			= (VkDeviceSize)(sizeof(deUint32)*renderSize.x()*renderSize.y());
	const VkBufferCreateInfo				readImageBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// sType
		DE_NULL,									// pNext
		(VkBufferCreateFlags)0u,					// flags
		imageSizeBytes,								// size
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// usage
		VK_SHARING_MODE_EXCLUSIVE,					// sharingMode
		1u,											// queueFamilyIndexCount
		&queueFamilyIndex,							// pQueueFamilyIndices
	};
	const Unique<VkBuffer>					readImageBuffer			(createBuffer(vk, vkDevice, &readImageBufferParams));
	const UniquePtr<Allocation>				readImageBufferMemory	(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

	const VkImageCreateInfo					imageParams				=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// sType
		DE_NULL,																// pNext
		0u,																		// flags
		VK_IMAGE_TYPE_2D,														// imageType
		VK_FORMAT_R8G8B8A8_UNORM,												// format
		{ renderSize.x(), renderSize.y(), 1 },									// extent
		1u,																		// mipLevels
		1u,																		// arraySize
		VK_SAMPLE_COUNT_1_BIT,													// samples
		VK_IMAGE_TILING_OPTIMAL,												// tiling
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// usage
		VK_SHARING_MODE_EXCLUSIVE,												// sharingMode
		1u,																		// queueFamilyIndexCount
		&queueFamilyIndex,														// pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,												// initialLayout
	};

	const Unique<VkImage>					image					(createImage(vk, vkDevice, &imageParams));
	const UniquePtr<Allocation>				imageMemory				(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any));

	VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageMemory->getMemory(), imageMemory->getOffset()));

	const VkAttachmentDescription			colorAttDesc			=
	{
		0u,												// flags
		VK_FORMAT_R8G8B8A8_UNORM,						// format
		VK_SAMPLE_COUNT_1_BIT,							// samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,					// loadOp
		VK_ATTACHMENT_STORE_OP_STORE,					// storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,				// stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,				// stencilStoreOp
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// finalLayout
	};
	const VkAttachmentReference				colorAttRef				=
	{
		0u,												// attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// layout
	};
	const VkSubpassDescription				subpassDesc				=
	{
		(VkSubpassDescriptionFlags)0u,					// flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,				// pipelineBindPoint
		0u,												// inputAttachmentCount
		DE_NULL,										// pInputAttachments
		1u,												// colorAttachmentCount
		&colorAttRef,									// pColorAttachments
		DE_NULL,										// pResolveAttachments
		DE_NULL,										// depthStencilAttachment
		0u,												// preserveAttachmentCount
		DE_NULL,										// pPreserveAttachments
	};
	const VkRenderPassCreateInfo			renderPassParams		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		// sType
		DE_NULL,										// pNext
		0u,												// flags
		1u,												// attachmentCount
		&colorAttDesc,									// pAttachments
		1u,												// subpassCount
		&subpassDesc,									// pSubpasses
		0u,												// dependencyCount
		DE_NULL,										// pDependencies
	};
	const Unique<VkRenderPass>				renderPass				(createRenderPass(vk, vkDevice, &renderPassParams));

	const VkImageViewCreateInfo				colorAttViewParams		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// sType
		DE_NULL,										// pNext
		0u,												// flags
		*image,											// image
		VK_IMAGE_VIEW_TYPE_2D,							// viewType
		VK_FORMAT_R8G8B8A8_UNORM,						// format
		{
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},												// components
		{
			VK_IMAGE_ASPECT_COLOR_BIT,						// aspectMask
			0u,												// baseMipLevel
			1u,												// levelCount
			0u,												// baseArrayLayer
			1u,												// layerCount
		},												// subresourceRange
	};
	const Unique<VkImageView>				colorAttView			(createImageView(vk, vkDevice, &colorAttViewParams));

	// Pipeline layout
	const VkPipelineLayoutCreateInfo		pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		(vk::VkPipelineLayoutCreateFlags)0,
		0u,														// setLayoutCount
		DE_NULL,												// pSetLayouts
		0u,														// pushConstantRangeCount
		DE_NULL,												// pPushConstantRanges
	};
	const Unique<VkPipelineLayout>			pipelineLayout			(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

	// Shaders
	const Unique<VkShaderModule>			vertShaderModule		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>			fragShaderModule		(createShaderModule(vk, vkDevice, context.getBinaryCollection().get("frag"), 0));

	// Pipeline
	const VkSpecializationInfo				emptyShaderSpecParams	=
	{
		0u,														// mapEntryCount
		DE_NULL,												// pMap
		0,														// dataSize
		DE_NULL,												// pData
	};
	const VkPipelineShaderStageCreateInfo	shaderStageParams[]	=
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// sType
			DE_NULL,												// pNext
			0u,														// flags
			VK_SHADER_STAGE_VERTEX_BIT,								// stage
			*vertShaderModule,										// module
			"main",													// pName
			&emptyShaderSpecParams,									// pSpecializationInfo
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// sType
			DE_NULL,												// pNext
			0u,														// flags
			VK_SHADER_STAGE_FRAGMENT_BIT,							// stage
			*fragShaderModule,										// module
			"main",													// pName
			&emptyShaderSpecParams,									// pSpecializationInfo
		}
	};
	const VkPipelineDepthStencilStateCreateInfo	depthStencilParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// sType
		DE_NULL,													// pNext
		0u,															// flags
		DE_FALSE,													// depthTestEnable
		DE_FALSE,													// depthWriteEnable
		VK_COMPARE_OP_ALWAYS,										// depthCompareOp
		DE_FALSE,													// depthBoundsTestEnable
		DE_FALSE,													// stencilTestEnable
		{
			VK_STENCIL_OP_KEEP,											// failOp
			VK_STENCIL_OP_KEEP,											// passOp
			VK_STENCIL_OP_KEEP,											// depthFailOp
			VK_COMPARE_OP_ALWAYS,										// compareOp
			0u,															// compareMask
			0u,															// writeMask
			0u,															// reference
		},															// front
		{
			VK_STENCIL_OP_KEEP,											// failOp
			VK_STENCIL_OP_KEEP,											// passOp
			VK_STENCIL_OP_KEEP,											// depthFailOp
			VK_COMPARE_OP_ALWAYS,										// compareOp
			0u,															// compareMask
			0u,															// writeMask
			0u,															// reference
		},															// back;
		-1.0f,														//	float				minDepthBounds;
		+1.0f,														//	float				maxDepthBounds;
	};
	const VkViewport						viewport0				=
	{
		0.0f,														// x
		0.0f,														// y
		(float)renderSize.x(),										// width
		(float)renderSize.y(),										// height
		0.0f,														// minDepth
		1.0f,														// maxDepth
	};
	const VkRect2D							scissor0				=
	{
		{
			0u,															// x
			0u,															// y
		},															// offset
		{
			renderSize.x(),												// width
			renderSize.y(),												// height
		},															// extent;
	};
	const VkPipelineViewportStateCreateInfo		viewportParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,		// sType
		DE_NULL,													// pNext
		0u,															// flags
		1u,															// viewportCount
		&viewport0,													// pViewports
		1u,															// scissorCount
		&scissor0													// pScissors
	};
	const VkSampleMask							sampleMask				= ~0u;
	const VkPipelineMultisampleStateCreateInfo	multisampleParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// sType
		DE_NULL,													// pNext
		0u,															// flags
		VK_SAMPLE_COUNT_1_BIT,										// rasterizationSamples
		VK_FALSE,													// sampleShadingEnable
		0.0f,														// minSampleShading
		&sampleMask,												// sampleMask
		VK_FALSE,													// alphaToCoverageEnable
		VK_FALSE,													// alphaToOneEnable
	};
	const VkPipelineRasterizationStateCreateInfo	rasterParams		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// sType
		DE_NULL,													// pNext
		0u,															// flags
		VK_TRUE,													// depthClampEnable
		VK_FALSE,													// rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										// polygonMode
		VK_CULL_MODE_NONE,											// cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// frontFace
		VK_FALSE,													// depthBiasEnable
		0.0f,														// depthBiasConstantFactor
		0.0f,														// depthBiasClamp
		0.0f,														// depthBiasSlopeFactor
		1.0f,														// lineWidth
	};
	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// sType
		DE_NULL,														// pNext
		0u,																// flags
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// topology
		DE_FALSE,														// primitiveRestartEnable
	};
	const VkVertexInputBindingDescription		vertexBinding0			=
	{
		0u,														// binding
		(deUint32)sizeof(tcu::Vec4),							// stride
		VK_VERTEX_INPUT_RATE_VERTEX,							// inputRate
	};
	const VkVertexInputAttributeDescription		vertexAttrib0			=
	{
		0u,														// location
		0u,														// binding
		VK_FORMAT_R32G32B32A32_SFLOAT,							// format
		0u,														// offset
	};
	const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// sType
		DE_NULL,													// pNext
		0u,															// flags
		1u,															// vertexBindingDescriptionCount
		&vertexBinding0,											// pVertexBindingDescriptions
		1u,															// vertexAttributeDescriptionCount
		&vertexAttrib0,												// pVertexAttributeDescriptions
	};
	const VkPipelineColorBlendAttachmentState	attBlendParams			=
	{
		VK_FALSE,													// blendEnable
		VK_BLEND_FACTOR_ONE,										// srcColorBlendFactor
		VK_BLEND_FACTOR_ZERO,										// dstColorBlendFactor
		VK_BLEND_OP_ADD,											// colorBlendOp
		VK_BLEND_FACTOR_ONE,										// srcAlphaBlendFactor
		VK_BLEND_FACTOR_ZERO,										// dstAlphaBlendFactor
		VK_BLEND_OP_ADD,											// alphaBlendOp
		(VK_COLOR_COMPONENT_R_BIT|
		 VK_COLOR_COMPONENT_G_BIT|
		 VK_COLOR_COMPONENT_B_BIT|
		 VK_COLOR_COMPONENT_A_BIT),									// colorWriteMask
	};
	const VkPipelineColorBlendStateCreateInfo	blendParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// sType
		DE_NULL,													// pNext
		0u,															// flags
		DE_FALSE,													// logicOpEnable
		VK_LOGIC_OP_COPY,											// logicOp
		1u,															// attachmentCount
		&attBlendParams,											// pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// blendConstants[4]
	};
	const VkPipelineDynamicStateCreateInfo	dynamicStateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// sType
		DE_NULL,												// pNext
		0u,														// flags
		0u,														// dynamicStateCount
		DE_NULL													// pDynamicStates
	};
	const VkGraphicsPipelineCreateInfo		pipelineParams			=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,		// sType
		DE_NULL,												// pNext
		0u,														// flags
		(deUint32)DE_LENGTH_OF_ARRAY(shaderStageParams),		// stageCount
		shaderStageParams,										// pStages
		&vertexInputStateParams,								// pVertexInputState
		&inputAssemblyParams,									// pInputAssemblyState
		DE_NULL,												// pTessellationState
		&viewportParams,										// pViewportState
		&rasterParams,											// pRasterizationState
		&multisampleParams,										// pMultisampleState
		&depthStencilParams,									// pDepthStencilState
		&blendParams,											// pColorBlendState
		&dynamicStateInfo,										// pDynamicState
		*pipelineLayout,										// layout
		*renderPass,											// renderPass
		0u,														// subpass
		DE_NULL,												// basePipelineHandle
		0u,														// basePipelineIndex
	};

	const Unique<VkPipeline>				pipeline				(createGraphicsPipeline(vk, vkDevice, DE_NULL, &pipelineParams));

	// Framebuffer
	const VkFramebufferCreateInfo			framebufferParams		=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				// sType
		DE_NULL,												// pNext
		0u,														// flags
		*renderPass,											// renderPass
		1u,														// attachmentCount
		&*colorAttView,											// pAttachments
		(deUint32)renderSize.x(),								// width
		(deUint32)renderSize.y(),								// height
		1u,														// layers
	};
	const Unique<VkFramebuffer>				framebuffer				(createFramebuffer(vk, vkDevice, &framebufferParams));

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// sType
		DE_NULL,													// pNext
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,			// flags
		queueFamilyIndex,											// queueFamilyIndex
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCommandBufferAllocateInfo		cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,			// sType
		DE_NULL,												// pNext
		*cmdPool,												// pool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,						// level
		1u,														// bufferCount
	};
	const Unique<VkCommandBuffer>			cmdBuf					(allocateCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCommandBufferBeginInfo			cmdBufBeginParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// sType
		DE_NULL,												// pNext
		0u,														// flags
		DE_NULL,												// renderPass
		0u,														// subpass
		DE_NULL,												// framebuffer
		VK_FALSE,												// occlusionQueryEnable
		(VkQueryControlFlags)0u,								// queryFlags
		(VkQueryPipelineStatisticFlags)0u,						// pipelineStatistics
	};

	// Record commands
	VK_CHECK(vk.beginCommandBuffer(*cmdBuf, &cmdBufBeginParams));

	{
		const VkMemoryBarrier		vertFlushBarrier	=
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,			// sType
			DE_NULL,									// pNext
			VK_ACCESS_HOST_WRITE_BIT,					// srcAccessMask
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,		// dstAccessMask
		};
		const VkImageMemoryBarrier	colorAttBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// sType
			DE_NULL,									// pNext
			0u,											// srcAccessMask
			(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),		// dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,					// oldLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// newLayout
			queueFamilyIndex,							// srcQueueFamilyIndex
			queueFamilyIndex,							// dstQueueFamilyIndex
			*image,										// image
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,											// baseMipLevel
				1u,											// levelCount
				0u,											// baseArrayLayer
				1u,											// layerCount
			}											// subresourceRange
		};
		const void*				barriers[]				= { &vertFlushBarrier, &colorAttBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	{
		const VkClearValue			clearValue		= makeClearValueColorF32(0.125f, 0.25f, 0.75f, 1.0f);
		const VkRenderPassBeginInfo	passBeginParams	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,			// sType
			DE_NULL,											// pNext
			*renderPass,										// renderPass
			*framebuffer,										// framebuffer
			{ { 0, 0 }, { renderSize.x(), renderSize.y() } },	// renderArea
			1u,													// clearValueCount
			&clearValue,										// pClearValues
		};
		vk.cmdBeginRenderPass(*cmdBuf, &passBeginParams, VK_SUBPASS_CONTENTS_INLINE);
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
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// sType
			DE_NULL,									// pNext
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// outputMask
			VK_ACCESS_TRANSFER_READ_BIT,				// inputMask
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// oldLayout
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// newLayout
			queueFamilyIndex,							// srcQueueFamilyIndex
			queueFamilyIndex,							// dstQueueFamilyIndex
			*image,										// image
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,											// baseMipLevel
				1u,											// mipLevels
				0u,											// baseArraySlice
				1u,											// arraySize
			}											// subresourceRange
		};
		const void*				barriers[]				= { &renderFinishBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	{
		const VkBufferImageCopy	copyParams	=
		{
			(VkDeviceSize)0u,						// bufferOffset
			(deUint32)renderSize.x(),				// bufferRowLength
			(deUint32)renderSize.y(),				// bufferImageHeight
			{
				VK_IMAGE_ASPECT_COLOR_BIT,				// aspectMask
				0u,										// mipLevel
				0u,										// baseArrayLayer
				1u,										// layerCount
			},										// imageSubresource
			{ 0u, 0u, 0u },							// imageOffset
			{ renderSize.x(), renderSize.y(), 1u }	// imageExtent
		};
		vk.cmdCopyImageToBuffer(*cmdBuf, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *readImageBuffer, 1u, &copyParams);
	}

	{
		const VkBufferMemoryBarrier	copyFinishBarrier	=
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// sType
			DE_NULL,									// pNext
			VK_ACCESS_TRANSFER_WRITE_BIT,				// srcAccessMask
			VK_ACCESS_HOST_READ_BIT,					// dstAccessMask
			queueFamilyIndex,							// srcQueueFamilyIndex
			queueFamilyIndex,							// dstQueueFamilyIndex
			*readImageBuffer,							// buffer
			0u,											// offset
			imageSizeBytes								// size
		};
		const void*				barriers[]				= { &copyFinishBarrier };
		vk.cmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, DE_FALSE, (deUint32)DE_LENGTH_OF_ARRAY(barriers), barriers);
	}

	VK_CHECK(vk.endCommandBuffer(*cmdBuf));

	// Upload vertex data
	{
		const VkMappedMemoryRange	range			=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	// sType
			DE_NULL,								// pNext
			vertexBufferMemory->getMemory(),		// memory
			0,										// offset
			(VkDeviceSize)sizeof(vertices),			// size
		};
		void*						vertexBufPtr	= vertexBufferMemory->getHostPtr();

		deMemcpy(vertexBufPtr, &vertices[0], sizeof(vertices));
		VK_CHECK(vk.flushMappedMemoryRanges(vkDevice, 1u, &range));
	}

	// Submit & wait for completion
	{
		const VkFenceCreateInfo	fenceParams	=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// sType
			DE_NULL,								// pNext
			0u,										// flags
		};
		const VkSubmitInfo		submitInfo	=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,			// sType
			DE_NULL,								// pNext
			0u,										// waitSemaphoreCount
			DE_NULL,								// pWaitSemaphores
			1u,										// commandBufferCount
			&cmdBuf.get(),							// pCommandBuffers
			0u,										// signalSemaphoreCount
			DE_NULL,								// pSignalSemaphores
		};
		const Unique<VkFence>	fence		(createFence(vk, vkDevice, &fenceParams));

		VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	// Log image
	{
		const VkMappedMemoryRange	range		=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	// sType
			DE_NULL,								// pNext
			readImageBufferMemory->getMemory(),		// memory
			0,										// offset
			imageSizeBytes,							// size
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
