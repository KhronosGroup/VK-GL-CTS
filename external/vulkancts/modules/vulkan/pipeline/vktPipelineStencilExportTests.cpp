/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \brief VK_EXT_shader_stencil_export tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineStencilExportTests.hpp"

#include "vktPipelineMakeUtil.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktPipelineUniqueRandomIterator.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

#include "deMemory.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using tcu::Vec4;
using tcu::Vec2;
using tcu::UVec2;
using tcu::UVec4;
using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;

namespace
{

void initPrograms (SourceCollections& programCollection, vk::VkFormat)
{
	// Vertex shader.
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "vec2 positions[6] = vec2[](\n"
			<< "	vec2(-1.0, -1.0),\n"
			<< "	vec2(-1.0, +1.0),\n"
			<< "	vec2(+1.0, -1.0),\n"
			<< "	vec2(+1.0, +1.0),\n"
			<< "	vec2(+1.0, -1.0),\n"
			<< "	vec2(-1.0, +1.0)\n"
			<< "\n"
			<< ");\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
			<< "}\n";
		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader that writes to Stencil buffer.
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_ARB_shader_stencil_export: enable\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    int refX = (int(gl_FragCoord.x) >> 4) % 2;\n"
			<< "    int refY = (int(gl_FragCoord.y) >> 4) % 2;\n"
			<< "    gl_FragStencilRefARB = (refX + refY) % 2;\n"
			<< "}\n";
		programCollection.glslSources.add("frag-stencil") << glu::FragmentSource(src.str());
	}

	// Fragment shader that writes to Color buffer.
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "layout(location = 0) out highp vec4 fragColor;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    fragColor = vec4(0, 0, 1, 1);\n"
			<< "}\n";
		programCollection.glslSources.add("frag-color") << glu::FragmentSource(src.str());
	}
}

bool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, VkPhysicalDevice device, VkFormat format)
{
	VkFormatProperties formatProps;

	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkImageCreateInfo makeImageCreateInfo (const VkFormat format, const UVec2& size, VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		(VkImageCreateFlags)0,							// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		format,											// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),			// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return imageParams;
}

Move<VkRenderPass> makeTestRenderPass (const DeviceInterface& vk,
									   const VkDevice		  device,
									   const VkFormat		  colorFormat,
									   const VkFormat		  stencilFormat)
{
	VkAttachmentDescription attachmentDescriptions[] =
	{
		{
			(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags	flags;
			colorFormat,											// VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits		samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,							// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout				finalLayout;
		},
		{
			(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags	flags;
			stencilFormat,											// VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits		samples;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_CLEAR,							// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// VkImageLayout				finalLayout;
		},
	};

	VkAttachmentReference colorAttachmentReference =
	{
		0,															// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout	layout;
	};

	VkAttachmentReference stencilAttachmentReference =
	{
		1,															// deUint32			attachment;
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,			// VkImageLayout	layout;
	};

	VkSubpassDescription subpasses[] =
	{
		{
			(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint;
			0u,									// deUint32							inputAttachmentCount;
			DE_NULL,							// const VkAttachmentReference*		pInputAttachments;
			0u,									// deUint32							colorAttachmentCount;
		    DE_NULL,							// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,							// const VkAttachmentReference*		pResolveAttachments;
			&stencilAttachmentReference,		// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,									// deUint32							preserveAttachmentCount;
			DE_NULL								// const deUint32*					pPreserveAttachments;
		},
		{
			(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint;
			0u,									// deUint32							inputAttachmentCount;
			DE_NULL,							// const VkAttachmentReference*		pInputAttachments;
			1u,									// deUint32							colorAttachmentCount;
			&colorAttachmentReference,			// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,							// const VkAttachmentReference*		pResolveAttachments;
			&stencilAttachmentReference,		// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,									// deUint32							preserveAttachmentCount;
			DE_NULL								// const deUint32*					pPreserveAttachments;
		},
	};

	VkSubpassDependency dependency =
	{
		0u,												// uint32_t                srcSubpass;
		1u,												// uint32_t                dstSubpass;
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,		// VkPipelineStageFlags    srcStageMask;
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,		// VkPipelineStageFlags    dstStageMask;
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,	// VkAccessFlags           srcAccessMask;
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,	// VkAccessFlags           dstAccessMask;
		0u,												// VkDependencyFlags       dependencyFlags;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags;
		2u,											// deUint32							attachmentCount;
		&attachmentDescriptions[0],					// const VkAttachmentDescription*	pAttachments;
		2u,											// deUint32							subpassCount;
		&subpasses[0],								// const VkSubpassDescription*		pSubpasses;
		1u,											// deUint32							dependencyCount;
		&dependency,								// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&		vk,
									   const VkDevice				device,
									   const VkPipelineLayout		pipelineLayout,
									   const VkRenderPass			renderPass,
									   const deUint32				subpass,
									   const VkShaderModule			vertexModule,
									   const VkShaderModule			fragmentModule,
									   const UVec2					renderSize,
									   const bool					useColor)
{
	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags       flags;
		0u,																// uint32_t                                    vertexBindingDescriptionCount;
		DE_NULL,														// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		0u,																// uint32_t                                    vertexAttributeDescriptionCount;
		DE_NULL,														// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags     flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology                         topology;
		VK_FALSE,														// VkBool32                                    primitiveRestartEnable;
	};

	const VkViewport						viewport					= makeViewport(renderSize);
	const VkRect2D							scissor						= makeRect2D(renderSize);
	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags          flags;
		1u,																// uint32_t                                    viewportCount;
		&viewport,														// const VkViewport*                           pViewports;
		1u,																// uint32_t                                    scissorCount;
		&scissor,														// const VkRect2D*                             pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags  flags;
		VK_FALSE,														// VkBool32                                 depthClampEnable;
		VK_FALSE,														// VkBool32                                 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0,						// VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,														// VkBool32									sampleShadingEnable;
		0.0f,															// float									minSampleShading;
		DE_NULL,														// const VkSampleMask*						pSampleMask;
		VK_FALSE,														// VkBool32									alphaToCoverageEnable;
		VK_FALSE														// VkBool32									alphaToOneEnable;
	};

	const VkStencilOpState stencilOpState = makeStencilOpState(
		useColor ? VK_STENCIL_OP_KEEP : VK_STENCIL_OP_REPLACE,			// stencil fail
		useColor ? VK_STENCIL_OP_KEEP : VK_STENCIL_OP_REPLACE,			// depth & stencil pass
		useColor ? VK_STENCIL_OP_KEEP : VK_STENCIL_OP_REPLACE,			// depth only fail
		useColor ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_NEVER,			// compare op
		useColor ? 0xffu : 0u,											// compare mask
		useColor ? 0u : 0xffu,											// write mask
		0u);															// reference

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,						// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_FALSE,														// VkBool32									depthTestEnable;
		VK_FALSE,														// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_NEVER,											// VkCompareOp								depthCompareOp;
		VK_FALSE,														// VkBool32									depthBoundsTestEnable;
		VK_TRUE,														// VkBool32									stencilTestEnable;
		stencilOpState,													// VkStencilOpState							front;
		stencilOpState,													// VkStencilOpState							back;
		0.0f,															// float									minDepthBounds;
		1.0f,															// float									maxDepthBounds;
	};

	const VkColorComponentFlags					colorComponentsAll					= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	const VkPipelineColorBlendAttachmentState	pipelineColorBlendAttachmentState	=
	{
		VK_FALSE,						// VkBool32					blendEnable;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp				alphaBlendOp;
		colorComponentsAll,				// VkColorComponentFlags	colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0,						// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,														// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,												// VkLogicOp									logicOp;
		1u,																// deUint32										attachmentCount;
		&pipelineColorBlendAttachmentState,								// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },										// float										blendConstants[4];
	};

	const VkPipelineShaderStageCreateInfo pShaderStages[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBits				stage;
			vertexModule,												// VkShaderModule						module;
			"main",														// const char*							pName;
			DE_NULL,													// const VkSpecializationInfo*			pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBits				stage;
			fragmentModule,												// VkShaderModule						module;
			"main",														// const char*							pName;
			DE_NULL,													// const VkSpecializationInfo*			pSpecializationInfo;
		},
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,				// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		(VkPipelineCreateFlags)0,										// VkPipelineCreateFlags							flags;
	    2,																// deUint32											stageCount;
		pShaderStages,													// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,											// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,								// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,														// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&pipelineViewportStateInfo,										// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,								// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&pipelineMultisampleStateInfo,									// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&pipelineDepthStencilStateInfo,									// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&pipelineColorBlendStateInfo,									// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,														// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,													// VkPipelineLayout									layout;
		renderPass,														// VkRenderPass										renderPass;
		subpass,														// deUint32											subpass;
		DE_NULL,														// VkPipeline										basePipelineHandle;
		0,																// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

tcu::TextureLevel generateReferenceImage (const tcu::TextureFormat	format,
										  const UVec2&				renderSize,
										  const deUint32			patternSize,
										  const Vec4&				clearColor,
										  const Vec4&				color)
{
	tcu::TextureLevel image(format, renderSize.x(), renderSize.y());
	tcu::clear(image.getAccess(), clearColor);

	deUint32 rows = renderSize.y() / patternSize;
	deUint32 cols = renderSize.x() / patternSize;

	for (deUint32 i = 0; i < rows; i++)
	{
		for (deUint32 j = 0; j < cols; j++)
		{
			if ((i + j) % 2 == 0)
				tcu::clear(tcu::getSubregion(image.getAccess(), i * patternSize, j * patternSize, patternSize, patternSize), color);
		}
	}

	return image;
}

tcu::TestStatus testStencilExportReplace (Context& context, vk::VkFormat stencilFormat)
{
	auto& log = context.getTestContext().getLog();
	log << tcu::TestLog::Message << "Drawing to stencil using shader then using it for another draw." << tcu::TestLog::EndMessage;

	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	Allocator&						allocator			= context.getDefaultAllocator();

	const UVec2						renderSize			(128, 128);
	const VkFormat					colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const Vec4						clearColor			(0.5f, 0.5f, 0.5f, 1.0f);
	const VkDeviceSize				colorBufferSize		= renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));

	const Unique<VkBuffer>			colorBuffer			(makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc	(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	// Zero color buffer.
	deMemset(colorBufferAlloc->getHostPtr(), 0, static_cast<std::size_t>(colorBufferSize));
	flushAlloc(vk, device, *colorBufferAlloc);

	// Draw two subpasses: first write the stencil data, then use that data when writing color.
	//
	// The first pass will produce a checkerboard stencil by having the shader filling gl_FragStencilRefARB with 0 or 1,
	// and using OP_REPLACE to write those values to the stencil buffer.
	//
	// The second pass will use the stencil with a compare operation EQUAL with reference value 0.
	{
		const VkImageSubresourceRange	stencilSubresourceRange	= makeImageSubresourceRange	(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);
		Move<VkImage>					stencilImage			= makeImage					(vk, device, makeImageCreateInfo(stencilFormat, renderSize, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
		MovePtr<Allocation>				stencilImageAlloc		= bindImage					(vk, device, allocator, *stencilImage, MemoryRequirement::Any);
		Move<VkImageView>				stencilAttachment		= makeImageView				(vk, device, *stencilImage, VK_IMAGE_VIEW_TYPE_2D, stencilFormat, stencilSubresourceRange);

		const VkImageSubresourceRange	colorSubresourceRange	= makeImageSubresourceRange	(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		Move<VkImage>					colorImage				= makeImage					(vk, device, makeImageCreateInfo(colorFormat, renderSize, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
		MovePtr<Allocation>				colorImageAlloc			= bindImage					(vk, device, allocator, *colorImage, MemoryRequirement::Any);
		Move<VkImageView>				colorAttachment			= makeImageView				(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange);

		Move<VkShaderModule>			vertexModule			= createShaderModule		(vk, device, context.getBinaryCollection().get("vert"), 0);
		Move<VkShaderModule>			fragmentStencilModule	= createShaderModule		(vk, device, context.getBinaryCollection().get("frag-stencil"), 0);
		Move<VkShaderModule>			fragmentColorModule		= createShaderModule		(vk, device, context.getBinaryCollection().get("frag-color"), 0);

		Move<VkRenderPass>				renderPass				= makeTestRenderPass		(vk, device, colorFormat, stencilFormat);
		Move<VkPipelineLayout>			pipelineLayout			= makePipelineLayout		(vk, device);
		Move<VkPipeline>				stencilPipeline			= makeGraphicsPipeline		(vk, device, *pipelineLayout, *renderPass, 0, *vertexModule, *fragmentStencilModule, renderSize, false);
		Move<VkPipeline>				colorPipeline			= makeGraphicsPipeline		(vk, device, *pipelineLayout, *renderPass, 1, *vertexModule, *fragmentColorModule, renderSize, true);

		const VkImageView attachments[] =
		{
			*colorAttachment,
			*stencilAttachment,
		};
		Move<VkFramebuffer>				framebuffer				= makeFramebuffer			(vk, device, *renderPass, 2u, &attachments[0], renderSize.x(), renderSize.y());

		Move<VkCommandPool>				cmdPool					= createCommandPool			(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex());
		Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer		(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		const VkQueue					queue					= context.getUniversalQueue();

		beginCommandBuffer(vk, *cmdBuffer);
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()), clearColor, 0.0, 0u);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *stencilPipeline);
		vk.cmdDraw(*cmdBuffer, 6u, 1u, 0u, 0u);

		vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *colorPipeline);
		vk.cmdDraw(*cmdBuffer, 6u, 1u, 0u, 0u);

		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, tcu::IVec2(renderSize.x(), renderSize.y()));

		VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Compare the resulting color buffer.
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);
		const tcu::ConstPixelBufferAccess	resultImage		(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1u, colorBufferAlloc->getHostPtr());

		tcu::TextureLevel					referenceImage	= generateReferenceImage(mapVkFormat(colorFormat), renderSize, 1 << 4, clearColor, Vec4(0, 0, 1, 1));

		if (!tcu::floatThresholdCompare(log, "color", "Image compare", referenceImage.getAccess(), resultImage, Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			TCU_FAIL("Rendered image is not correct");
	}

	return tcu::TestStatus::pass("OK");
}

void checkSupport (Context& context, vk::VkFormat stencilFormat)
{
	context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");

	if (!isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), stencilFormat))
		TCU_THROW(NotSupportedError, "Image format not supported");
}

} // anonymous

tcu::TestCaseGroup* createStencilExportTests (tcu::TestContext& testCtx)
{
	struct
	{
		const vk::VkFormat	format;
		const std::string	name;
	} kFormats[] =
	{
		{ vk::VK_FORMAT_S8_UINT,			"s8_uint"				},
		{ vk::VK_FORMAT_D24_UNORM_S8_UINT,	"d24_unorm_s8_uint"		},
		{ vk::VK_FORMAT_D32_SFLOAT_S8_UINT,	"d32_sfloat_s8_uint"	},
	};

	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "shader_stencil_export", ""));
	for (int fmtIdx = 0; fmtIdx < DE_LENGTH_OF_ARRAY(kFormats); ++fmtIdx)
	{
		de::MovePtr<tcu::TestCaseGroup> formatGroup (new tcu::TestCaseGroup(testCtx, kFormats[fmtIdx].name.c_str(), ""));
		addFunctionCaseWithPrograms<vk::VkFormat>(formatGroup.get(), "op_replace", "", checkSupport, initPrograms, testStencilExportReplace, kFormats[fmtIdx].format);
		group->addChild(formatGroup.release());
	}
	return group.release();
}

} // pipeline
} // vkt
