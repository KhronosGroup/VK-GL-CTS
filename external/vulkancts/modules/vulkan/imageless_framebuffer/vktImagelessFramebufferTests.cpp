/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Vulkan Imageless Framebuffer Tests
 *//*--------------------------------------------------------------------*/

#include "vktImagelessFramebufferTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"

#include <iostream>

namespace vkt
{
namespace imageless
{

namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using de::SharedPtr;

typedef SharedPtr<Unique<VkPipeline> >	SharedPtrVkPipeline;

enum TestType
{
	TEST_TYPE_COLOR = 0,
	TEST_TYPE_DEPTH_STENCIL,
	TEST_TYPE_COLOR_RESOLVE,
	TEST_TYPE_DEPTH_STENCIL_RESOLVE,
	TEST_TYPE_MULTISUBPASS,
	TEST_TYPE_DIFFERENT_ATTACHMENTS,
	TEST_TYPE_LAST
};

enum AspectFlagBits
{
	ASPECT_NONE				= 0,
	ASPECT_COLOR			= (1<<0),
	ASPECT_DEPTH			= (1<<1),
	ASPECT_STENCIL			= (1<<2),
	ASPECT_DEPTH_STENCIL	= ASPECT_DEPTH | ASPECT_STENCIL,
};
typedef deUint32 AspectFlags;

const deUint32	NO_SAMPLE	= static_cast<deUint32>(-1);
const deUint32	NO_SUBPASS	= static_cast<deUint32>(-1);

struct TestParameters
{
	TestType	testType;
	VkFormat	colorFormat;
	VkFormat	dsFormat;
};

template<typename T>
inline SharedPtr<Unique<T> > makeSharedPtr (Move<T> move)
{
	return SharedPtr<Unique<T> >(new Unique<T>(move));
}

VkSampleCountFlagBits sampleCountBitFromSampleCount (deUint32 count)
{
	switch (count)
	{
		case 1:  return VK_SAMPLE_COUNT_1_BIT;
		case 2:  return VK_SAMPLE_COUNT_2_BIT;
		case 4:  return VK_SAMPLE_COUNT_4_BIT;
		case 8:  return VK_SAMPLE_COUNT_8_BIT;
		case 16: return VK_SAMPLE_COUNT_16_BIT;
		case 32: return VK_SAMPLE_COUNT_32_BIT;
		case 64: return VK_SAMPLE_COUNT_64_BIT;

		default:
			DE_FATAL("Invalid sample count");
			return (VkSampleCountFlagBits)0x0;
	}
}

VkAttachmentReference2 convertAttachmentReference (const VkAttachmentReference& attachmentReference, const VkImageAspectFlags aspectMask)
{
	const VkAttachmentReference2	attachmentReference2	=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,		//  VkStructureType		sType;
		DE_NULL,										//  const void*			pNext;
		attachmentReference.attachment,					//  deUint32			attachment;
		attachmentReference.layout,						//  VkImageLayout		layout;
		aspectMask										//  VkImageAspectFlags	aspectMask;
	};

	return attachmentReference2;
}

std::vector<VkAttachmentDescription2> convertAttachmentDescriptions (const std::vector<VkAttachmentDescription>& attachmentDescriptions)
{
	std::vector<VkAttachmentDescription2>	attachmentDescriptions2;

	attachmentDescriptions2.reserve(attachmentDescriptions.size());

	for (size_t adNdx = 0; adNdx < attachmentDescriptions.size(); ++adNdx)
	{
		const VkAttachmentDescription&		attachmentDescription	= attachmentDescriptions[adNdx];
		const VkAttachmentDescription2		attachmentDescription2	=
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,		//  VkStructureType					sType;
			DE_NULL,										//  const void*						pNext;
			attachmentDescription.flags,					//  VkAttachmentDescriptionFlags	flags;
			attachmentDescription.format,					//  VkFormat						format;
			attachmentDescription.samples,					//  VkSampleCountFlagBits			samples;
			attachmentDescription.loadOp,					//  VkAttachmentLoadOp				loadOp;
			attachmentDescription.storeOp,					//  VkAttachmentStoreOp				storeOp;
			attachmentDescription.stencilLoadOp,			//  VkAttachmentLoadOp				stencilLoadOp;
			attachmentDescription.stencilStoreOp,			//  VkAttachmentStoreOp				stencilStoreOp;
			attachmentDescription.initialLayout,			//  VkImageLayout					initialLayout;
			attachmentDescription.finalLayout,				//  VkImageLayout					finalLayout;
		};

		attachmentDescriptions2.push_back(attachmentDescription2);
	}

	return attachmentDescriptions2;
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&		vk,
									   const VkDevice				device,
									   const VkPipelineLayout		pipelineLayout,
									   const VkRenderPass			renderPass,
									   const VkShaderModule			vertexModule,
									   const VkShaderModule			fragmendModule,
									   const VkExtent2D				renderSize,
									   const AspectFlags			depthStencilAspects	= ASPECT_NONE,
									   const VkSampleCountFlagBits	sampleCountBits		= VK_SAMPLE_COUNT_1_BIT,
									   const deUint32				subpass				= 0)
{
	const bool									useDepth						= (depthStencilAspects & ASPECT_DEPTH) != 0;
	const bool									useStencil						= (depthStencilAspects & ASPECT_STENCIL) != 0;
	const std::vector<VkViewport>				viewports						(1, makeViewport(renderSize));
	const std::vector<VkRect2D>					scissors						(1, makeRect2D(renderSize));
	const VkStencilOpState						stencilOpState					=
	{
		VK_STENCIL_OP_KEEP,					//  VkStencilOp	failOp;
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,	//  VkStencilOp	passOp;
		VK_STENCIL_OP_KEEP,					//  VkStencilOp	depthFailOp;
		VK_COMPARE_OP_ALWAYS,				//  VkCompareOp	compareOp;
		~0u,								//  deUint32	compareMask;
		~0u,								//  deUint32	writeMask;
		0u									//  deUint32	reference;
	};
	const VkPipelineDepthStencilStateCreateInfo	pipelineDepthStencilStateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	//  VkStructureType							sType;
		DE_NULL,													//  const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,					//  VkPipelineDepthStencilStateCreateFlags	flags;
		useDepth ? VK_TRUE : VK_FALSE,								//  VkBool32								depthTestEnable;
		useDepth ? VK_TRUE : VK_FALSE,								//  VkBool32								depthWriteEnable;
		VK_COMPARE_OP_LESS,											//  VkCompareOp								depthCompareOp;
		VK_FALSE,													//  VkBool32								depthBoundsTestEnable;
		useStencil ? VK_TRUE : VK_FALSE,							//  VkBool32								stencilTestEnable;
		stencilOpState,												//  VkStencilOpState						front;
		stencilOpState,												//  VkStencilOpState						back;
		0.0f,														//  float									minDepthBounds;
		1.0f														//  float									maxDepthBounds;
	};
	const VkPipelineMultisampleStateCreateInfo	multisampleState				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//  VkStructureType							sType;
		DE_NULL,													//  const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0u,					//  VkPipelineMultisampleStateCreateFlags	flags;
		sampleCountBits,											//  VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													//  VkBool32								sampleShadingEnable;
		0.0f,														//  float									minSampleShading;
		DE_NULL,													//  const VkSampleMask*						pSampleMask;
		VK_FALSE,													//  VkBool32								alphaToCoverageEnable;
		VK_FALSE,													//  VkBool32								alphaToOneEnable;
	};

	return makeGraphicsPipeline(vk,									//  const DeviceInterface&							vk
								device,								//  const VkDevice									device
								pipelineLayout,						//  const VkPipelineLayout							pipelineLayout
								vertexModule,						//  const VkShaderModule							vertexShaderModule
								DE_NULL,							//  const VkShaderModule							tessellationControlModule
								DE_NULL,							//  const VkShaderModule							tessellationEvalModule
								DE_NULL,							//  const VkShaderModule							geometryShaderModule
								fragmendModule,						//  const VkShaderModule							fragmentShaderModule
								renderPass,							//  const VkRenderPass								renderPass
								viewports,							//  const std::vector<VkViewport>&					viewports
								scissors,							//  const std::vector<VkRect2D>&					scissors
								VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,//  const VkPrimitiveTopology						topology
								subpass,							//  const deUint32									subpass
								0u,									//  const deUint32									patchControlPoints
								DE_NULL,							//  const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
								DE_NULL,							//  const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
								&multisampleState,					//  const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
								&pipelineDepthStencilStateInfo);	//  const VkPipelineDepthStencilStateCreateInfo*	depthStencilStateCreateInfo
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface&				vk,
								   const VkDevice						device,
								   const VkFormat						colorFormat,
								   const VkFormat						depthStencilFormat,
								   const VkSampleCountFlagBits			colorSamples,
								   const VkSampleCountFlagBits			depthStencilSamples			= VK_SAMPLE_COUNT_1_BIT,
								   const VkAttachmentLoadOp				loadOperation				= VK_ATTACHMENT_LOAD_OP_CLEAR,
								   const VkImageLayout					finalLayoutColor			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
								   const VkImageLayout					finalLayoutDepthStencil		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								   const VkImageLayout					subpassLayoutColor			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
								   const VkImageLayout					subpassLayoutDepthStencil	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								   const VkAllocationCallbacks* const	allocationCallbacks			= DE_NULL)
{
	const bool								hasColor									= colorFormat != VK_FORMAT_UNDEFINED;
	const bool								hasDepthStencil								= depthStencilFormat != VK_FORMAT_UNDEFINED;
	const bool								hasColorResolve								= hasColor && (colorSamples != VK_SAMPLE_COUNT_1_BIT);
	const bool								hasDepthStencilResolve						= hasDepthStencil && (depthStencilSamples != VK_SAMPLE_COUNT_1_BIT);
	const VkImageLayout						initialLayoutColor							= loadOperation == VK_ATTACHMENT_LOAD_OP_LOAD ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
	const VkImageLayout						initialLayoutDepthStencil					= loadOperation == VK_ATTACHMENT_LOAD_OP_LOAD ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
	const VkAttachmentDescription			colorAttachmentDescription					=
	{
		(VkAttachmentDescriptionFlags)0,		//  VkAttachmentDescriptionFlags	flags;
		colorFormat,							//  VkFormat						format;
		colorSamples,							//  VkSampleCountFlagBits			samples;
		loadOperation,							//  VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,			//  VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,		//  VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,		//  VkAttachmentStoreOp				stencilStoreOp;
		initialLayoutColor,						//  VkImageLayout					initialLayout;
		finalLayoutColor						//  VkImageLayout					finalLayout;
	};
	const VkAttachmentDescription			depthStencilAttachmentDescription			=
	{
		(VkAttachmentDescriptionFlags)0,		//  VkAttachmentDescriptionFlags	flags;
		depthStencilFormat,						//  VkFormat						format;
		depthStencilSamples,					//  VkSampleCountFlagBits			samples;
		loadOperation,							//  VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,			//  VkAttachmentStoreOp				storeOp;
		loadOperation,							//  VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_STORE,			//  VkAttachmentStoreOp				stencilStoreOp;
		initialLayoutDepthStencil,				//  VkImageLayout					initialLayout;
		finalLayoutDepthStencil					//  VkImageLayout					finalLayout;
	};
	const VkAttachmentDescription			colorResolveAttachmentDescription			=
	{
		(VkAttachmentDescriptionFlags)0,		//  VkAttachmentDescriptionFlags	flags;
		colorFormat,							//  VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,		//  VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,			//  VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,		//  VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,		//  VkAttachmentStoreOp				stencilStoreOp;
		initialLayoutColor,						//  VkImageLayout					initialLayout;
		finalLayoutColor						//  VkImageLayout					finalLayout;
	};
	const VkAttachmentDescription			depthStencilResolveAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,		//  VkAttachmentDescriptionFlags	flags;
		depthStencilFormat,						//  VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,		//  VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,			//  VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,		//  VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_STORE,			//  VkAttachmentStoreOp				stencilStoreOp;
		initialLayoutDepthStencil,				//  VkImageLayout					initialLayout;
		finalLayoutDepthStencil					//  VkImageLayout					finalLayout;
	};
	std::vector<VkAttachmentDescription>	attachmentDescriptions;

	if (hasColor)
		attachmentDescriptions.push_back(colorAttachmentDescription);
	if (hasDepthStencil)
		attachmentDescriptions.push_back(depthStencilAttachmentDescription);
	if (hasColorResolve)
		attachmentDescriptions.push_back(colorResolveAttachmentDescription);
	if (hasDepthStencilResolve)
		attachmentDescriptions.push_back(depthStencilResolveAttachmentDescription);

	deUint32								attachmentCounter								= 0;
	const VkAttachmentReference				colorAttachmentRef								=
	{
		hasColor ? attachmentCounter++ : 0u,				//  deUint32		attachment;
		subpassLayoutColor									//  VkImageLayout	layout;
	};
	const VkAttachmentReference				depthStencilAttachmentRef						=
	{
		hasDepthStencil ? attachmentCounter++ : 0u,			//  deUint32		attachment;
		subpassLayoutDepthStencil							//  VkImageLayout	layout;
	};
	const VkAttachmentReference				colorResolveAttachmentRef						=
	{
		hasColorResolve ? attachmentCounter++ : 0u,			//  deUint32		attachment;
		subpassLayoutColor									//  VkImageLayout	layout;
	};

	if (hasDepthStencilResolve)
	{
		const VkImageAspectFlags							colorAspectMask							= VK_IMAGE_ASPECT_COLOR_BIT;
		const VkImageAspectFlags							depthStencilAspectMask					= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		const VkAttachmentReference2						colorAttachmentRef2						= convertAttachmentReference(colorAttachmentRef, colorAspectMask);
		const VkAttachmentReference2						depthStencilAttachmentRef2				= convertAttachmentReference(depthStencilAttachmentRef, depthStencilAspectMask);
		const VkAttachmentReference2						colorResolveAttachmentRef2				= convertAttachmentReference(colorResolveAttachmentRef, colorAspectMask);
		const VkAttachmentReference2						depthStencilResolveAttachmentRef2		=
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,				//  VkStructureType		sType;
			DE_NULL,												//  const void*			pNext;
			hasDepthStencilResolve ? attachmentCounter++ : 0u,		//  deUint32			attachment;
			subpassLayoutDepthStencil,								//  VkImageLayout		layout;
			depthStencilAspectMask									//  VkImageAspectFlags	aspectMask;
		};
		const VkSubpassDescriptionDepthStencilResolve		subpassDescriptionDepthStencilResolve	=
		{
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,		//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,									//  VkResolveModeFlagBitsKHR			depthResolveMode;
			VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,									//  VkResolveModeFlagBitsKHR			stencilResolveMode;
			&depthStencilResolveAttachmentRef2									//  const VkAttachmentReference2KHR*	pDepthStencilResolveAttachment;
		};
		const VkSubpassDescription2							subpassDescription2						=
		{
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,					//  VkStructureType						sType;
			&subpassDescriptionDepthStencilResolve,						//  const void*							pNext;
			(VkSubpassDescriptionFlags)0,								//  VkSubpassDescriptionFlags			flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,							//  VkPipelineBindPoint					pipelineBindPoint;
			0u,															//  deUint32							viewMask;
			0u,															//  deUint32							inputAttachmentCount;
			DE_NULL,													//  const VkAttachmentReference2*		pInputAttachments;
			hasColor ? 1u : 0u,											//  deUint32							colorAttachmentCount;
			hasColor ? &colorAttachmentRef2 : DE_NULL,					//  const VkAttachmentReference2*		pColorAttachments;
			hasColorResolve ? &colorResolveAttachmentRef2 : DE_NULL,	//  const VkAttachmentReference2*		pResolveAttachments;
			hasDepthStencil ? &depthStencilAttachmentRef2 : DE_NULL,	//  const VkAttachmentReference2*		pDepthStencilAttachment;
			0u,															//  deUint32							preserveAttachmentCount;
			DE_NULL														//  const deUint32*						pPreserveAttachments;
		};
		const std::vector<VkAttachmentDescription2>			attachmentDescriptions2					= convertAttachmentDescriptions(attachmentDescriptions);
		const VkRenderPassCreateInfo2						renderPassInfo							=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,		//  VkStructureType						sType;
			DE_NULL,											//  const void*							pNext;
			(VkRenderPassCreateFlags)0,							//  VkRenderPassCreateFlags				flags;
			(deUint32)attachmentDescriptions2.size(),			//  deUint32							attachmentCount;
			&attachmentDescriptions2[0],						//  const VkAttachmentDescription2*		pAttachments;
			1u,													//  deUint32							subpassCount;
			&subpassDescription2,								//  const VkSubpassDescription2*		pSubpasses;
			0u,													//  deUint32							dependencyCount;
			DE_NULL,											//  const VkSubpassDependency2*			pDependencies;
			0u,													//  deUint32							correlatedViewMaskCount;
			DE_NULL												//  const deUint32*						pCorrelatedViewMasks;
		};

		return createRenderPass2(vk, device, &renderPassInfo, allocationCallbacks);
	}
	else
	{
		const VkSubpassDescription				subpassDescription							=
		{
			(VkSubpassDescriptionFlags)0,							//  VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,						//  VkPipelineBindPoint				pipelineBindPoint;
			0u,														//  deUint32						inputAttachmentCount;
			DE_NULL,												//  const VkAttachmentReference*	pInputAttachments;
			hasColor ? 1u : 0u,										//  deUint32						colorAttachmentCount;
			hasColor ? &colorAttachmentRef : DE_NULL,				//  const VkAttachmentReference*	pColorAttachments;
			hasColorResolve ? &colorResolveAttachmentRef : DE_NULL,	//  const VkAttachmentReference*	pResolveAttachments;
			hasDepthStencil ? &depthStencilAttachmentRef : DE_NULL,	//  const VkAttachmentReference*	pDepthStencilAttachment;
			0u,														//  deUint32						preserveAttachmentCount;
			DE_NULL													//  const deUint32*					pPreserveAttachments;
		};
		const VkRenderPassCreateInfo			renderPassInfo								=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	//  VkStructureType					sType;
			DE_NULL,									//  const void*						pNext;
			(VkRenderPassCreateFlags)0,					//  VkRenderPassCreateFlags			flags;
			(deUint32)attachmentDescriptions.size(),	//  deUint32						attachmentCount;
			&attachmentDescriptions[0],					//  const VkAttachmentDescription*	pAttachments;
			1u,											//  deUint32						subpassCount;
			&subpassDescription,						//  const VkSubpassDescription*		pSubpasses;
			0u,											//  deUint32						dependencyCount;
			DE_NULL										//  const VkSubpassDependency*		pDependencies;
		};

		return createRenderPass(vk, device, &renderPassInfo, allocationCallbacks);
	}
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface&				vk,
								   const VkDevice						device,
								   const VkFormat						colorFormat,
								   const VkAllocationCallbacks* const	allocationCallbacks)
{
	const VkAttachmentDescription	attachmentDescriptions[]	=
	{
		{
			(VkAttachmentDescriptionFlags)0,			//  VkAttachmentDescriptionFlags	flags;
			colorFormat,								//  VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,						//  VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,				//  VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,				//  VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//  VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			//  VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,					//  VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_GENERAL						//  VkImageLayout					finalLayout;
		},
		{
			(VkAttachmentDescriptionFlags)0,			//  VkAttachmentDescriptionFlags	flags;
			colorFormat,								//  VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,						//  VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//  VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,				//  VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//  VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			//  VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,					//  VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_GENERAL						//  VkImageLayout					finalLayout;
		},
	};
	const VkAttachmentReference		colorAttachmentRef0			=
	{
		0u,							//  deUint32		attachment;
		VK_IMAGE_LAYOUT_GENERAL		//  VkImageLayout	layout;
	};
	const deUint32					preserveAttachment			= 1u;
	const VkAttachmentReference		inputAttachmentRef1			=
	{
		0u,							//  deUint32		attachment;
		VK_IMAGE_LAYOUT_GENERAL		//  VkImageLayout	layout;
	};
	const VkAttachmentReference		colorAttachmentRef1			=
	{
		1u,							//  deUint32		attachment;
		VK_IMAGE_LAYOUT_GENERAL		//  VkImageLayout	layout;
	};
	const VkSubpassDescription		subpassDescriptions[]		=
	{
		{
			(VkSubpassDescriptionFlags)0,				//  VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,			//  VkPipelineBindPoint				pipelineBindPoint;
			0u,											//  deUint32						inputAttachmentCount;
			DE_NULL,									//  const VkAttachmentReference*	pInputAttachments;
			1u,											//  deUint32						colorAttachmentCount;
			&colorAttachmentRef0,						//  const VkAttachmentReference*	pColorAttachments;
			DE_NULL,									//  const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,									//  const VkAttachmentReference*	pDepthStencilAttachment;
			1u,											//  deUint32						preserveAttachmentCount;
			&preserveAttachment							//  const deUint32*					pPreserveAttachments;
		},
		{
			(VkSubpassDescriptionFlags)0,				//  VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,			//  VkPipelineBindPoint				pipelineBindPoint;
			1u,											//  deUint32						inputAttachmentCount;
			&inputAttachmentRef1,						//  const VkAttachmentReference*	pInputAttachments;
			1u,											//  deUint32						colorAttachmentCount;
			&colorAttachmentRef1,						//  const VkAttachmentReference*	pColorAttachments;
			DE_NULL,									//  const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,									//  const VkAttachmentReference*	pDepthStencilAttachment;
			0u,											//  deUint32						preserveAttachmentCount;
			DE_NULL										//  const deUint32*					pPreserveAttachments;
		},
	};
	const VkSubpassDependency		subpassDependency			=
	{
		0,												//  deUint32						srcSubpass;
		1u,												//  deUint32						dstSubpass;
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	//  VkPipelineStageFlags			srcStageMask;
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,			//  VkPipelineStageFlags			dstStageMask;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			//  VkAccessFlags					srcAccessMask;
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			//  VkAccessFlags					dstAccessMask;
		VK_DEPENDENCY_VIEW_LOCAL_BIT,					//  VkDependencyFlags				dependencyFlags;
	};
	const VkRenderPassCreateInfo	renderPassInfo				=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		//  VkStructureType					sType;
		DE_NULL,										//  const void*						pNext;
		(VkRenderPassCreateFlags)0,						//  VkRenderPassCreateFlags			flags;
		DE_LENGTH_OF_ARRAY(attachmentDescriptions),		//  deUint32						attachmentCount;
		&attachmentDescriptions[0],						//  const VkAttachmentDescription*	pAttachments;
		DE_LENGTH_OF_ARRAY(subpassDescriptions),		//  deUint32						subpassCount;
		&subpassDescriptions[0],						//  const VkSubpassDescription*		pSubpasses;
		1u,												//  deUint32						dependencyCount;
		&subpassDependency								//  const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo, allocationCallbacks);
}

Move<VkRenderPass> makeSingleAttachmentRenderPass (const DeviceInterface&				vk,
												   const VkDevice						device,
												   const VkFormat						colorFormat,
												   const VkAllocationCallbacks* const	allocationCallbacks)
{
	const VkAttachmentDescription	attachmentDescriptions[]	=
	{
		{
			(VkAttachmentDescriptionFlags)0,			//  VkAttachmentDescriptionFlags	flags;
			colorFormat,								//  VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,						//  VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,				//  VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,				//  VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//  VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			//  VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,					//  VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_GENERAL						//  VkImageLayout					finalLayout;
		},
	};
	const VkAttachmentReference		colorAttachmentRef0			=
	{
		0u,							//  deUint32		attachment;
		VK_IMAGE_LAYOUT_GENERAL		//  VkImageLayout	layout;
	};
	const VkSubpassDescription		subpassDescriptions[]		=
	{
		{
			(VkSubpassDescriptionFlags)0,				//  VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,			//  VkPipelineBindPoint				pipelineBindPoint;
			0u,											//  deUint32						inputAttachmentCount;
			DE_NULL,									//  const VkAttachmentReference*	pInputAttachments;
			1u,											//  deUint32						colorAttachmentCount;
			&colorAttachmentRef0,						//  const VkAttachmentReference*	pColorAttachments;
			DE_NULL,									//  const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,									//  const VkAttachmentReference*	pDepthStencilAttachment;
			0u,											//  deUint32						preserveAttachmentCount;
			DE_NULL										//  const deUint32*					pPreserveAttachments;
		}
	};
	const VkRenderPassCreateInfo	renderPassInfo				=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		//  VkStructureType					sType;
		DE_NULL,										//  const void*						pNext;
		(VkRenderPassCreateFlags)0,						//  VkRenderPassCreateFlags			flags;
		DE_LENGTH_OF_ARRAY(attachmentDescriptions),		//  deUint32						attachmentCount;
		&attachmentDescriptions[0],						//  const VkAttachmentDescription*	pAttachments;
		DE_LENGTH_OF_ARRAY(subpassDescriptions),		//  deUint32						subpassCount;
		&subpassDescriptions[0],						//  const VkSubpassDescription*		pSubpasses;
		0u,												//  deUint32						dependencyCount;
		DE_NULL											//  const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo, allocationCallbacks);
}

VkImageCreateInfo makeImageCreateInfo (const VkFormat format, const VkExtent2D size, const VkImageUsageFlags usage, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT)
{
	const VkExtent3D		extent		= { size.width, size.height, 1u };
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		format,											// VkFormat					format;
		extent,											// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		samples,										// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return imageParams;
}

std::vector<VkFramebufferAttachmentImageInfo> makeFramebufferAttachmentImageInfos (const VkExtent2D&			renderSize,
																				   const VkFormat*				colorFormat,
																				   const VkImageUsageFlags		colorUsage,
																				   const VkFormat*				dsFormat,
																				   const VkImageUsageFlags		dsUsage,
																				   const AspectFlags			resolveAspects,
																				   const deUint32				inputAttachmentCount)
{
	const bool										colorResolve					= (resolveAspects & ASPECT_COLOR) != 0;
	const bool										depthStencilResolve				= (resolveAspects & ASPECT_DEPTH_STENCIL) != 0;
	std::vector<VkFramebufferAttachmentImageInfo>	framebufferAttachmentImageInfos;

	DE_ASSERT(colorFormat != DE_NULL);
	DE_ASSERT(dsFormat != DE_NULL);

	if (*colorFormat != VK_FORMAT_UNDEFINED)
	{
		const VkFramebufferAttachmentImageInfo framebufferAttachmentImageInfo		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			(VkImageCreateFlags)0u,										//  VkImageCreateFlags	flags;
			colorUsage,													//  VkImageUsageFlags	usage;
			renderSize.width,											//  deUint32			width;
			renderSize.height,											//  deUint32			height;
			1u,															//  deUint32			layerCount;
			1u,															//  deUint32			viewFormatCount;
			colorFormat													//  const VkFormat*		pViewFormats;
		};

		framebufferAttachmentImageInfos.push_back(framebufferAttachmentImageInfo);
	}

	if (*dsFormat != VK_FORMAT_UNDEFINED)
	{
		const VkFramebufferAttachmentImageInfo framebufferAttachmentImageInfo		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			(VkImageCreateFlags)0u,										//  VkImageCreateFlags	flags;
			dsUsage,													//  VkImageUsageFlags	usage;
			renderSize.width,											//  deUint32			width;
			renderSize.height,											//  deUint32			height;
			1u,															//  deUint32			layerCount;
			1u,															//  deUint32			viewFormatCount;
			dsFormat													//  const VkFormat*		pViewFormats;
		};

		framebufferAttachmentImageInfos.push_back(framebufferAttachmentImageInfo);
	}

	if (colorResolve)
	{
		const VkFramebufferAttachmentImageInfo framebufferAttachmentImageInfo		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			(VkImageCreateFlags)0u,										//  VkImageCreateFlags	flags;
			colorUsage,													//  VkImageUsageFlags	usage;
			renderSize.width,											//  deUint32			width;
			renderSize.height,											//  deUint32			height;
			1u,															//  deUint32			layerCount;
			1u,															//  deUint32			viewFormatCount;
			colorFormat													//  const VkFormat*		pViewFormats;
		};

		DE_ASSERT(*colorFormat != VK_FORMAT_UNDEFINED);

		framebufferAttachmentImageInfos.push_back(framebufferAttachmentImageInfo);
	}

	if (depthStencilResolve)
	{
		const VkFramebufferAttachmentImageInfo framebufferAttachmentImageInfo		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			(VkImageCreateFlags)0u,										//  VkImageCreateFlags	flags;
			dsUsage,													//  VkImageUsageFlags	usage;
			renderSize.width,											//  deUint32			width;
			renderSize.height,											//  deUint32			height;
			1u,															//  deUint32			layerCount;
			1u,															//  deUint32			viewFormatCount;
			dsFormat													//  const VkFormat*		pViewFormats;
		};

		DE_ASSERT(*dsFormat != VK_FORMAT_UNDEFINED);

		framebufferAttachmentImageInfos.push_back(framebufferAttachmentImageInfo);
	}

	for (deUint32 inputAttachmentNdx = 0; inputAttachmentNdx < inputAttachmentCount; ++inputAttachmentNdx)
	{
		const VkFramebufferAttachmentImageInfo framebufferAttachmentImageInfo		=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			(VkImageCreateFlags)0u,										//  VkImageCreateFlags	flags;
			colorUsage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,			//  VkImageUsageFlags	usage;
			renderSize.width,											//  deUint32			width;
			renderSize.height,											//  deUint32			height;
			1u,															//  deUint32			layerCount;
			1u,															//  deUint32			viewFormatCount;
			colorFormat													//  const VkFormat*		pViewFormats;
		};

		framebufferAttachmentImageInfos.push_back(framebufferAttachmentImageInfo);
	}

	return framebufferAttachmentImageInfos;
}

Move<VkFramebuffer> makeFramebuffer (const DeviceInterface&			vk,
									 const VkDevice					device,
									 const VkRenderPass				renderPass,
									 const VkExtent2D&				renderSize,
									 const VkFormat*				colorFormat,
									 const VkImageUsageFlags		colorUsage,
									 const VkFormat*				dsFormat,
									 const VkImageUsageFlags		dsUsage					= static_cast<VkImageUsageFlags>(0),
									 const AspectFlags				resolveAspects			= ASPECT_NONE,
									 const deUint32					inputAttachmentCount	= 0)
{
	const std::vector<VkFramebufferAttachmentImageInfo>		framebufferAttachmentImageInfos		= makeFramebufferAttachmentImageInfos(renderSize, colorFormat, colorUsage, dsFormat, dsUsage, resolveAspects, inputAttachmentCount);
	const deUint32											attachmentCount						= static_cast<deUint32>(framebufferAttachmentImageInfos.size());
	const VkFramebufferAttachmentsCreateInfo				framebufferAttachmentsCreateInfo	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,		//  VkStructureType								sType;
		DE_NULL,													//  const void*									pNext;
		attachmentCount,											//  deUint32									attachmentImageInfoCount;
		&framebufferAttachmentImageInfos[0]							//  const VkFramebufferAttachmentImageInfo*		pAttachmentImageInfos;
	};
	const VkFramebufferCreateInfo							framebufferInfo	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,					//  VkStructureType				sType;
		&framebufferAttachmentsCreateInfo,							//  const void*					pNext;
		VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,						//  VkFramebufferCreateFlags	flags;
		renderPass,													//  VkRenderPass				renderPass;
		attachmentCount,											//  deUint32					attachmentCount;
		DE_NULL,													//  const VkImageView*			pAttachments;
		renderSize.width,											//  deUint32					width;
		renderSize.height,											//  deUint32					height;
		1u,															//  deUint32					layers;
	};

	return createFramebuffer(vk, device, &framebufferInfo);
}

Move<VkFramebuffer> makeVerifyFramebuffer (const DeviceInterface&	vk,
										   const VkDevice			device,
										   const VkRenderPass		renderPass,
										   const VkImageView		colorAttachment,
										   const VkExtent2D&		renderSize,
										   const deUint32			layers = 1u)
{
	const VkFramebufferCreateInfo framebufferInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		//  VkStructureType				sType;
		DE_NULL,										//  const void*					pNext;
		(VkFramebufferCreateFlags)0,					//  VkFramebufferCreateFlags	flags;
		renderPass,										//  VkRenderPass				renderPass;
		1u,												//  deUint32					attachmentCount;
		&colorAttachment,								//  const VkImageView*			pAttachments;
		renderSize.width,								//  deUint32					width;
		renderSize.height,								//  deUint32					height;
		layers,											//  deUint32					layers;
	};

	return createFramebuffer(vk, device, &framebufferInfo);
}

Move<VkPipelineLayout> makeVerifyPipelineLayout (const DeviceInterface&			vk,
												 const VkDevice					device,
												 const VkDescriptorSetLayout	descriptorSetLayout)
{
	const VkPushConstantRange			pushConstantRanges			=
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,					//  VkShaderStageFlags				stageFlags;
		0u,												//  deUint32						offset;
		sizeof(deUint32)								//  deUint32						size;
	};
	const VkPipelineLayoutCreateInfo	pipelineLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//  VkStructureType					sType;
		DE_NULL,										//  const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,					//  VkPipelineLayoutCreateFlags		flags;
		1u,												//  deUint32						setLayoutCount;
		&descriptorSetLayout,							//  const VkDescriptorSetLayout*	pSetLayouts;
		1u,												//  deUint32						pushConstantRangeCount;
		&pushConstantRanges,							//  const VkPushConstantRange*		pPushConstantRanges;
	};
	return createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
}

Move<VkRenderPass> makeVerifyRenderPass (const DeviceInterface&	vk,
										 const VkDevice			device,
										 const VkFormat			colorFormat)
{
	return makeRenderPass(vk, device, colorFormat);
}

VkImageMemoryBarrier makeImageMemoryBarrier	(const VkAccessFlags			srcAccessMask,
											 const VkAccessFlags			dstAccessMask,
											 const VkImageLayout			oldLayout,
											 const VkImageLayout			newLayout,
											 const VkImage					image,
											 const VkImageSubresourceRange	subresourceRange)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		srcAccessMask,									// VkAccessFlags			outputMask;
		dstAccessMask,									// VkAccessFlags			inputMask;
		oldLayout,										// VkImageLayout			oldLayout;
		newLayout,										// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return barrier;
}

VkBufferMemoryBarrier makeBufferMemoryBarrier (const VkAccessFlags	srcAccessMask,
											   const VkAccessFlags	dstAccessMask,
											   const VkBuffer		buffer,
											   const VkDeviceSize	offset,
											   const VkDeviceSize	bufferSizeBytes)
{
	const VkBufferMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
		DE_NULL,									//  const void*		pNext;
		srcAccessMask,								//  VkAccessFlags	srcAccessMask;
		dstAccessMask,								//  VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
		buffer,										//  VkBuffer		buffer;
		offset,										//  VkDeviceSize	offset;
		bufferSizeBytes,							//  VkDeviceSize	size;
	};
	return barrier;
}

Move<VkSampler> makeSampler (const DeviceInterface& vk, const VkDevice& device)
{
	const VkSamplerCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		//  VkStructureType			sType;
		DE_NULL,									//  const void*				pNext;
		0u,											//  VkSamplerCreateFlags	flags;
		VK_FILTER_NEAREST,							//  VkFilter				magFilter;
		VK_FILTER_NEAREST,							//  VkFilter				minFilter;
		VK_SAMPLER_MIPMAP_MODE_LINEAR,				//  VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//  VkSamplerAddressMode	addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//  VkSamplerAddressMode	addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		//  VkSamplerAddressMode	addressModeW;
		0.0f,										//  float					mipLodBias;
		VK_FALSE,									//  VkBool32				anisotropyEnable;
		1.0f,										//  float					maxAnisotropy;
		VK_FALSE,									//  VkBool32				compareEnable;
		VK_COMPARE_OP_ALWAYS,						//  VkCompareOp				compareOp;
		0.0f,										//  float					minLod;
		0.0f,										//  float					maxLod;
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	//  VkBorderColor			borderColor;
		VK_FALSE									//  VkBool32				unnormalizedCoordinates;
	};

	return createSampler(vk, device, &createInfo);
}

void fillBuffer (const DeviceInterface& vk, const VkDevice device, Allocation& bufferAlloc, const void* data, const VkDeviceSize dataSize)
{
	const VkMappedMemoryRange	memRange		=
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//  VkStructureType	sType;
		DE_NULL,								//  const void*		pNext;
		bufferAlloc.getMemory(),				//  VkDeviceMemory	memory;
		bufferAlloc.getOffset(),				//  VkDeviceSize	offset;
		VK_WHOLE_SIZE							//  VkDeviceSize	size;
	};
	const deUint32				dataSize32		= static_cast<deUint32>(dataSize);

	deMemcpy(bufferAlloc.getHostPtr(), data, dataSize32);
	VK_CHECK(vk.flushMappedMemoryRanges(device, 1u, &memRange));
}

std::vector<float> getFullQuadVertices (void)
{
	const float					verticesData[]	=
	{
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f, -1.0f, 0.0f, 1.0f,
		+1.0f, +1.0f, 0.0f, 1.0f,
	};
	const std::vector<float>	vertices		(verticesData, verticesData + DE_LENGTH_OF_ARRAY(verticesData));

	return vertices;
}

void checkImageFormatProperties (const InstanceInterface&	vki,
								 const VkPhysicalDevice&	physDevice,
								 const VkFormat				format,
								 const VkImageUsageFlags	imageUsageFlags,
								 const VkExtent2D&			requiredSize2D)
{
	const VkImageType			imageType			= VK_IMAGE_TYPE_2D;
	const VkImageTiling			imageTiling			= VK_IMAGE_TILING_OPTIMAL;
	const VkImageCreateFlags	imageCreateFlags	= static_cast<VkImageCreateFlags>(0u);
	const deUint32				requiredLayers		= 1u;
	const VkExtent3D			requiredSize		= makeExtent3D(requiredSize2D.height, requiredSize2D.width, 1u);

	VkImageFormatProperties	imageFormatProperties;
	VkResult				result;

	deMemset(&imageFormatProperties, 0, sizeof(imageFormatProperties));

	result = vki.getPhysicalDeviceImageFormatProperties(physDevice, format, imageType, imageTiling, imageUsageFlags, imageCreateFlags, &imageFormatProperties);

	if (result									!= VK_SUCCESS			||
		imageFormatProperties.maxArrayLayers	<  requiredLayers		||
		imageFormatProperties.maxExtent.height	<  requiredSize.height	||
		imageFormatProperties.maxExtent.width	<  requiredSize.width	||
		imageFormatProperties.maxExtent.depth	<  requiredSize.depth)
	{
		TCU_THROW(NotSupportedError, "Depth/stencil format is not supported");
	}
}

VkFormat getStencilBufferFormat(VkFormat depthStencilImageFormat)
{
	const tcu::TextureFormat	tcuFormat	= mapVkFormat(depthStencilImageFormat);
	const VkFormat				result		= (tcuFormat.order == tcu::TextureFormat::S || tcuFormat.order == tcu::TextureFormat::DS) ? VK_FORMAT_S8_UINT : VK_FORMAT_UNDEFINED;

	DE_ASSERT(result != VK_FORMAT_UNDEFINED);

	return result;
}

static MovePtr<tcu::TextureLevel> convertDepthToColor (const tcu::TextureFormat& dataFormat, const int width, const int height, const void* data, const tcu::TextureFormat& targetFormat)
{
	const tcu::ConstPixelBufferAccess	srcImage	(dataFormat, width, height, 1u, data);
	MovePtr<tcu::TextureLevel>			dstImage	(new tcu::TextureLevel(targetFormat, width, height, 1u));
	tcu::PixelBufferAccess				dstAccess	(dstImage->getAccess());

	for (int y = 0; y < height; y++)
	for (int x = 0; x < width; x++)
	{
		const float		depth	= srcImage.getPixDepth(x, y);
		const tcu::Vec4	color	= tcu::Vec4(depth, depth, depth, 1.0f);

		dstAccess.setPixel(color, x, y);
	}

	return dstImage;
}

static MovePtr<tcu::TextureLevel> convertStencilToColor (const tcu::TextureFormat& dataFormat, const int width, const int height, const void* data, const tcu::TextureFormat& targetFormat)
{
	const int							maxValue	(4);
	const tcu::ConstPixelBufferAccess	srcImage	(dataFormat, width, height, 1u, data);
	MovePtr<tcu::TextureLevel>			dstImage	(new tcu::TextureLevel(targetFormat, width, height, 1u));
	tcu::PixelBufferAccess				dstAccess	(dstImage->getAccess());

	for (int y = 0; y < height; y++)
	for (int x = 0; x < width; x++)
	{
		const int		stencilInt	= srcImage.getPixStencil(x, y);
		const float		stencil		= (stencilInt < maxValue) ? float(stencilInt) / float(maxValue) : 1.0f;
		const tcu::Vec4	color		= tcu::Vec4(stencil, stencil, stencil, 1.0f);

		dstAccess.setPixel(color, x, y);
	}

	return dstImage;
}

class ColorImagelessTestInstance : public TestInstance
{
public:
										ColorImagelessTestInstance			(Context& context, const TestParameters& parameters);
protected:
	virtual tcu::TestStatus				iterate								(void);

	virtual std::vector<float>			getVertices							(void);
	void								readOneSampleFromMultisampleImage	(const VkFormat					srcFormat,
																			 const Unique<VkImage>&			srcImage,
																			 const deUint32					sampleID,
																			 const VkFormat					dstFormat,
																			 const Unique<VkImage>&			dstImage,
																			 const Unique<VkBuffer>&		dstBuffer,
																			 const AspectFlags				aspect);
	virtual MovePtr<tcu::TextureLevel>	generateReferenceImage				(const tcu::TextureFormat&		textureFormat,
																			 const AspectFlags				aspectFlags,
																			 const deUint32					sample,
																			 const deUint32					subpass);
	virtual bool						verifyBuffer						(const UniquePtr<Allocation>&	bufAlloc,
																			 const VkFormat					bufferFormat,
																			 const std::string&				name,
																			 const AspectFlags				aspectFlags,
																			 const deUint32					sample		= NO_SAMPLE,
																			 const deUint32					subpass		= NO_SUBPASS);
	virtual bool						verifyBufferInternal				(const void*					resultData,
																			 const tcu::TextureFormat&		textureFormat,
																			 const tcu::TextureLevel&		referenceImage,
																			 const std::string&				name);

	const bool							m_extensions;
	const VkExtent2D					m_imageExtent2D;
	const TestParameters				m_parameters;
	VkImageUsageFlags					m_colorImageUsage;
};

ColorImagelessTestInstance::ColorImagelessTestInstance (Context& context, const TestParameters& parameters)
	: TestInstance				(context)
	, m_extensions				(context.requireDeviceFunctionality("VK_KHR_imageless_framebuffer"))
	, m_imageExtent2D			(makeExtent2D(32u, 32u))
	, m_parameters				(parameters)
	, m_colorImageUsage			(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
{
	const InstanceInterface&								vki								= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice						= m_context.getPhysicalDevice();
	const VkPhysicalDeviceImagelessFramebufferFeatures&		imagelessFramebufferFeatures	(context.getImagelessFramebufferFeatures());

	if (imagelessFramebufferFeatures.imagelessFramebuffer == DE_FALSE)
		TCU_THROW(NotSupportedError, "Imageless framebuffer is not supported");

	checkImageFormatProperties(vki, physDevice, m_parameters.colorFormat, m_colorImageUsage, m_imageExtent2D);
}

void ColorImagelessTestInstance::readOneSampleFromMultisampleImage (const VkFormat			srcFormat,
																	const Unique<VkImage>&	srcImage,
																	const deUint32			sampleID,
																	const VkFormat			dstFormat,
																	const Unique<VkImage>&	dstImage,
																	const Unique<VkBuffer>&	dstBuffer,
																	const AspectFlags		aspect)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= m_context.getUniversalQueue();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	const tcu::Vec4						clearColor			= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const bool							color				= ((aspect & ASPECT_COLOR) != 0);
	const bool							depth				= ((aspect & ASPECT_DEPTH) != 0);
	const bool							stencil				= ((aspect & ASPECT_STENCIL) != 0);
	const VkImageAspectFlags			srcAspect			= color   ? VK_IMAGE_ASPECT_COLOR_BIT
															: depth   ? VK_IMAGE_ASPECT_DEPTH_BIT
															: VK_IMAGE_ASPECT_STENCIL_BIT;
	const VkImageSubresourceRange		srcSubresRange		= makeImageSubresourceRange(srcAspect, 0u, 1u, 0u, 1u);
	const Unique<VkImageView>			srcImageView		(makeImageView			(vk, device, *srcImage, VK_IMAGE_VIEW_TYPE_2D, srcFormat, srcSubresRange));

	const VkImageAspectFlags			dstAspect			= VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageSubresourceRange		dstSubresRange		= makeImageSubresourceRange(dstAspect, 0u, 1u, 0u, 1u);
	const Unique<VkImageView>			dstAttachment		(makeImageView			(vk, device, *dstImage, VK_IMAGE_VIEW_TYPE_2D, dstFormat, dstSubresRange));

	const std::string					fragModuleInfix		= color   ? "-color"
															: depth   ? "-depth"
															: stencil ? "-stencil"
															: "";
	const Unique<VkShaderModule>		vertModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("demultisample-vert"), 0u));
	const Unique<VkShaderModule>		fragModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("demultisample" + fragModuleInfix + "-frag"), 0u));
	const Unique<VkRenderPass>			renderPass			(makeVerifyRenderPass	(vk, device, dstFormat));
	const Unique<VkFramebuffer>			framebuffer			(makeVerifyFramebuffer	(vk, device, *renderPass, *dstAttachment, m_imageExtent2D));

	const VkDescriptorType				samplerDescType		(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	const Unique<VkSampler>				sampler				(makeSampler(vk, device));
	const Unique<VkDescriptorSetLayout>	descriptorSetLayout	(DescriptorSetLayoutBuilder()
		.addSingleSamplerBinding(samplerDescType, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler.get())
		.build(vk, device));
	const Unique<VkDescriptorPool>		descriptorPool		(DescriptorPoolBuilder()
		.addType(samplerDescType)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	const Unique<VkDescriptorSet>		descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorImageInfo			imageDescriptorInfo	(makeDescriptorImageInfo(DE_NULL, *srcImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), samplerDescType, &imageDescriptorInfo)
		.update(vk, device);

	const Unique<VkPipelineLayout>		pipelineLayout		(makeVerifyPipelineLayout	(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>			pipeline			(makeGraphicsPipeline		(vk, device, *pipelineLayout, *renderPass, *vertModule, *fragModule, m_imageExtent2D));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool			(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer		(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const std::vector<float>			vertexArray			(getFullQuadVertices());
	const deUint32						vertexCount			(static_cast<deUint32>(vertexArray.size() / 4u));
	const VkDeviceSize					vertexArraySize		(vertexArray.size() * sizeof(vertexArray[0]));
	const Unique<VkBuffer>				vertexBuffer		(makeBuffer				(vk, device, vertexArraySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>			vertexBufferAlloc	(bindBuffer				(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));
	const VkDeviceSize					vertexBufferOffset	(0u);

	fillBuffer(vk, device, *vertexBufferAlloc, &vertexArray[0], vertexArraySize);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		if (sampleID == 0)
		{
			if (color)
			{
				const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
																						 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
																						 *srcImage, srcSubresRange);

				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			}
			else if (depth)
			{
				const VkImageSubresourceRange	preCopySubresRange	= makeImageSubresourceRange	(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);
				const VkImageMemoryBarrier		preCopyBarrier		= makeImageMemoryBarrier	(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
																								 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
																								 *srcImage, preCopySubresRange);

				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			}
		}

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(sampleID), &sampleID);

			vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		// Image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *dstImage, dstSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *dstBuffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *dstImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstBuffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
}

bool ColorImagelessTestInstance::verifyBufferInternal (const void* resultData, const tcu::TextureFormat& textureFormat, const tcu::TextureLevel& referenceImage, const std::string& name)
{
	const int							dataSize		(m_imageExtent2D.width * m_imageExtent2D.height * textureFormat.getPixelSize());
	const tcu::ConstPixelBufferAccess	referenceAccess	(referenceImage.getAccess());

	if (deMemCmp(resultData, referenceAccess.getDataPtr(), dataSize) != 0)
	{
		const tcu::ConstPixelBufferAccess	resultImage	(textureFormat, m_imageExtent2D.width, m_imageExtent2D.height, 1u, resultData);
		bool								ok;

		ok = tcu::intThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), "", referenceAccess, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT);

		return ok;
	}

	return true;
}

bool ColorImagelessTestInstance::verifyBuffer (const UniquePtr<Allocation>& bufAlloc, const VkFormat bufferFormat, const std::string& name, const AspectFlags aspectFlags, const deUint32 sample, const deUint32 subpass)
{
	invalidateMappedMemoryRange(m_context.getDeviceInterface(), m_context.getDevice(), bufAlloc->getMemory(), bufAlloc->getOffset(), VK_WHOLE_SIZE);

	const tcu::TextureFormat			bufferTextureFormat		(mapVkFormat(bufferFormat));
	const bool							multisampled			(sample != NO_SAMPLE);
	const bool							depth					((aspectFlags & ASPECT_DEPTH) != 0);
	const bool							stencil					((aspectFlags & ASPECT_STENCIL) != 0);
	const bool							convertRequired			((depth || stencil) && !multisampled);
	const tcu::TextureFormat			convertTextureFormat	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::UNORM_INT8));
	const tcu::TextureFormat			referenceTextureFormat	(convertRequired ? convertTextureFormat : bufferTextureFormat);
	const MovePtr<tcu::TextureLevel>	referenceImage			(generateReferenceImage(referenceTextureFormat, aspectFlags, sample, subpass));

	if (!multisampled && depth)
	{
		MovePtr<tcu::TextureLevel>	convertedImage	(convertDepthToColor(bufferTextureFormat, m_imageExtent2D.width, m_imageExtent2D.height, bufAlloc->getHostPtr(), convertTextureFormat));
		tcu::ConstPixelBufferAccess	convertedAccess	(convertedImage->getAccess());

		return verifyBufferInternal(convertedAccess.getDataPtr(), convertTextureFormat, *referenceImage, name);
	}
	else if (!multisampled && stencil)
	{
		MovePtr<tcu::TextureLevel>	convertedImage	(convertStencilToColor(bufferTextureFormat, m_imageExtent2D.width, m_imageExtent2D.height, bufAlloc->getHostPtr(), convertTextureFormat));
		tcu::ConstPixelBufferAccess	convertedAccess	(convertedImage->getAccess());

		return verifyBufferInternal(convertedAccess.getDataPtr(), convertTextureFormat, *referenceImage, name);
	}
	else
	{
		const void*	resultData	(bufAlloc->getHostPtr());

		return verifyBufferInternal(resultData, bufferTextureFormat, *referenceImage, name);
	}
}

MovePtr<tcu::TextureLevel> ColorImagelessTestInstance::generateReferenceImage (const tcu::TextureFormat&	textureFormat,
																			   const AspectFlags			aspectFlags,
																			   const deUint32				sample,
																			   const deUint32				subpass)
{
	const int					width			= m_imageExtent2D.width;
	const int					height			= m_imageExtent2D.height;
	const int					componentValue	(static_cast<int>(0.75f * 0x100));
	const tcu::RGBA				colorDrawRGBA	(tcu::RGBA(componentValue, componentValue, componentValue, 0xFF));
	const tcu::Vec4				colorDraw		(colorDrawRGBA.toVec());
	const tcu::Vec4				colorFill		(tcu::RGBA::black().toVec());
	MovePtr<tcu::TextureLevel>	image			(new tcu::TextureLevel(textureFormat, width, height));
	tcu::PixelBufferAccess		access			(image->getAccess());

	DE_UNREF(aspectFlags);
	DE_ASSERT(aspectFlags == ASPECT_COLOR);
	DE_UNREF(sample);
	DE_ASSERT(sample == NO_SAMPLE);
	DE_UNREF(subpass);
	DE_ASSERT(subpass == NO_SUBPASS);

	for (int y = 0; y < height; ++y)
	{
		const tcu::Vec4&	validColor	= (y < height / 2) ? colorFill : colorDraw;

		for (int x = 0; x < width; ++x)
			access.setPixel(validColor, x, y);
	}

	return image;
}

std::vector<float> ColorImagelessTestInstance::getVertices (void)
{
	const float					verticesData[]	=
	{
		-1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f,  0.0f, 0.0f, 1.0f,
		+1.0f, +1.0f, 0.0f, 1.0f,
	};
	const std::vector<float>	vertices		(verticesData, verticesData + DE_LENGTH_OF_ARRAY(verticesData));

	return vertices;
}

tcu::TestStatus ColorImagelessTestInstance::iterate (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue				= m_context.getUniversalQueue();
	Allocator&						allocator			= m_context.getDefaultAllocator();

	const tcu::Vec4					clearColor			= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const VkFormat					colorFormat			= m_parameters.colorFormat;
	const VkDeviceSize				colorBufferSize		= m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkImageSubresourceRange	colorSubresRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const Unique<VkImage>			colorImage			(makeImage				(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage)));
	const UniquePtr<Allocation>		colorImageAlloc		(bindImage				(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorAttachment		(makeImageView			(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>			colorBuffer			(makeBuffer				(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc	(bindBuffer				(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>	vertModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass			(makeRenderPass			(vk, device, colorFormat, m_parameters.dsFormat));
	const Unique<VkFramebuffer>		framebuffer			(makeFramebuffer		(vk, device, *renderPass, m_imageExtent2D, &colorFormat, m_colorImageUsage, &m_parameters.dsFormat));
	const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout		(vk, device));
	const Unique<VkPipeline>		pipeline			(makeGraphicsPipeline	(vk, device, *pipelineLayout, *renderPass, *vertModule, *fragModule, m_imageExtent2D));
	const Unique<VkCommandPool>		cmdPool				(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const std::vector<float>		vertexArray			(getVertices());
	const deUint32					vertexCount			(static_cast<deUint32>(vertexArray.size() / 4u));
	const VkDeviceSize				vertexArraySize		(vertexArray.size() * sizeof(vertexArray[0]));
	const Unique<VkBuffer>			vertexBuffer		(makeBuffer				(vk, device, vertexArraySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc	(bindBuffer				(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));
	const VkDeviceSize				vertexBufferOffset	(0u);

	fillBuffer(vk, device, *vertexBufferAlloc, &vertexArray[0], vertexArraySize);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const VkRenderPassAttachmentBeginInfo		renderPassAttachmentBeginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			1u,															//  deUint32			attachmentCount;
			&*colorAttachment											//  const VkImageView*	pAttachments;
		};

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor, &renderPassAttachmentBeginInfo);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		// Color image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *colorImage, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	if (verifyBuffer(colorBufferAlloc, colorFormat, "Color", ASPECT_COLOR))
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

class DepthImagelessTestInstance : public ColorImagelessTestInstance
{
public:
										DepthImagelessTestInstance	(Context& context, const TestParameters& parameters);

protected:
	virtual tcu::TestStatus				iterate						(void);

	virtual std::vector<float>			getVertices					(void);

	virtual MovePtr<tcu::TextureLevel>	generateReferenceImage		(const tcu::TextureFormat&	textureFormat,
																	 const AspectFlags			aspectFlags,
																	 const deUint32				sample,
																	 const deUint32				subpass);

	VkImageUsageFlags					m_dsImageUsage;
};

DepthImagelessTestInstance::DepthImagelessTestInstance (Context& context, const TestParameters& parameters)
	: ColorImagelessTestInstance	(context, parameters)
	, m_dsImageUsage				(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
{
	const InstanceInterface&	vki			= m_context.getInstanceInterface();
	const VkPhysicalDevice		physDevice	= m_context.getPhysicalDevice();

	checkImageFormatProperties(vki, physDevice, m_parameters.dsFormat, m_dsImageUsage, m_imageExtent2D);
}

MovePtr<tcu::TextureLevel> DepthImagelessTestInstance::generateReferenceImage (const tcu::TextureFormat&	textureFormat,
																			   const AspectFlags			aspectFlags,
																			   const deUint32				sample,
																			   const deUint32				subpass)
{
	const bool					color	= ((aspectFlags & ASPECT_COLOR) != 0);
	const bool					depth	= ((aspectFlags & ASPECT_DEPTH) != 0);
	const bool					stencil	= ((aspectFlags & ASPECT_STENCIL) != 0);
	const int					width	= m_imageExtent2D.width;
	const int					height	= m_imageExtent2D.height;
	MovePtr<tcu::TextureLevel>	image	(new tcu::TextureLevel(textureFormat, width, height));
	tcu::PixelBufferAccess		access	(image->getAccess());

	DE_ASSERT(dePop32(aspectFlags) == 1);
	DE_UNREF(sample);
	DE_ASSERT(sample == NO_SAMPLE);
	DE_UNREF(subpass);
	DE_ASSERT(subpass == NO_SUBPASS);

	if (color)
	{
		const int		componentValue	(static_cast<int>(0.75f * 0x100));
		const tcu::RGBA	colorDrawRGBA	(tcu::RGBA(componentValue, componentValue, componentValue, 0xFF));
		const tcu::Vec4	colorDraw		(colorDrawRGBA.toVec());
		const tcu::Vec4	colorDrawTop	(tcu::RGBA::white().toVec());
		const tcu::Vec4	colorFill		(tcu::RGBA::black().toVec());

		for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			const tcu::Vec4&	validColor	= (y < height / 2) ? colorFill
											: (x < width  / 2) ? colorDraw
											: colorDrawTop;

			access.setPixel(validColor, x, y);
		}
	}

	if (depth)
	{
		const int			colorFillValue	(static_cast<int>(1.00f * 0x100));
		const int			colorDrawValue	(static_cast<int>(0.50f * 0x100));
		const int			colorTopValue	(static_cast<int>(0.25f * 0x100));
		const tcu::IVec4	colorFill		(colorFillValue, 0, 0, 0xFF);
		const tcu::IVec4	colorDraw		(colorDrawValue, 0, 0, 0xFF);
		const tcu::IVec4	colorTop		(colorTopValue,  0, 0, 0xFF);

		for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			const tcu::IVec4&	validColor	= (y < height / 2) ? colorFill
											: (x < width  / 2) ? colorDraw
											: colorTop;

			access.setPixel(validColor, x, y);
		}
	}

	if (stencil)
	{
		const int			colorFillValue	(static_cast<int>(0.00f * 0x100));
		const int			colorDrawValue	(static_cast<int>(0.25f * 0x100));
		const int			colorTopValue	(static_cast<int>(0.50f * 0x100));
		const tcu::IVec4	colorFill		(colorFillValue, 0, 0, 0xFF);
		const tcu::IVec4	colorDraw		(colorDrawValue, 0, 0, 0xFF);
		const tcu::IVec4	colorTop		(colorTopValue,  0, 0, 0xFF);

		for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			const tcu::IVec4&	validColor	= (y < height / 2) ? colorFill
											: (x < width  / 2) ? colorDraw
											: colorTop;

			access.setPixel(validColor, x, y);
		}
	}

	return image;
}

std::vector<float> DepthImagelessTestInstance::getVertices (void)
{
	const float					verticesData[]	=
	{
		-1.0f,  0.0f, 0.50f, 1.0f,
		-1.0f, +1.0f, 0.50f, 1.0f,
		+1.0f,  0.0f, 0.50f, 1.0f,
		-1.0f, +1.0f, 0.50f, 1.0f,
		+1.0f,  0.0f, 0.50f, 1.0f,
		+1.0f, +1.0f, 0.50f, 1.0f,

		 0.0f,  0.0f, 0.25f, 1.0f,
		 0.0f, +1.0f, 0.25f, 1.0f,
		+1.0f,  0.0f, 0.25f, 1.0f,
		 0.0f, +1.0f, 0.25f, 1.0f,
		+1.0f,  0.0f, 0.25f, 1.0f,
		+1.0f, +1.0f, 0.25f, 1.0f,
	};
	const std::vector<float>	vertices		(verticesData, verticesData + DE_LENGTH_OF_ARRAY(verticesData));

	return vertices;
}

tcu::TestStatus DepthImagelessTestInstance::iterate (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue				= m_context.getUniversalQueue();
	Allocator&						allocator			= m_context.getDefaultAllocator();

	const deUint32					sampleCount			= 1u;
	const VkSampleCountFlagBits		sampleCountFlag		= sampleCountBitFromSampleCount(sampleCount);
	const tcu::Vec4					clearColor			= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const VkFormat					colorFormat			= m_parameters.colorFormat;
	const VkDeviceSize				colorBufferSize		= m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkImageSubresourceRange	colorSubresRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const Unique<VkImage>			colorImage			(makeImage				(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage)));
	const UniquePtr<Allocation>		colorImageAlloc		(bindImage				(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorAttachment		(makeImageView			(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>			colorBuffer			(makeBuffer				(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc	(bindBuffer				(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const float						clearDepth			= 1.0f;
	const deUint32					clearStencil		= 0u;
	const VkFormat					dsFormat			= m_parameters.dsFormat;
	const deUint32					dsImagePixelSize	= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(dsFormat)));
	const VkImageAspectFlags		dsAspectFlags		= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	const VkImageSubresourceRange	dsSubresRange		= makeImageSubresourceRange(dsAspectFlags, 0u, 1u, 0u, 1u);

	const VkDeviceSize				depthBufferSize		= m_imageExtent2D.width * m_imageExtent2D.height * dsImagePixelSize;
	const VkFormat					stencilBufferFormat	= getStencilBufferFormat(dsFormat);
	const deUint32					stencilPixelSize	= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(stencilBufferFormat)));
	const VkDeviceSize				stencilBufferSize	= m_imageExtent2D.width * m_imageExtent2D.height * stencilPixelSize;

	const Unique<VkImage>			dsImage				(makeImage				(vk, device, makeImageCreateInfo(dsFormat, m_imageExtent2D, m_dsImageUsage)));
	const UniquePtr<Allocation>		dsImageAlloc		(bindImage				(vk, device, allocator, *dsImage, MemoryRequirement::Any));
	const Unique<VkImageView>		dsAttachment		(makeImageView			(vk, device, *dsImage, VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSubresRange));
	const Unique<VkBuffer>			depthBuffer			(makeBuffer				(vk, device, depthBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		depthBufferAlloc	(bindBuffer				(vk, device, allocator, *depthBuffer, MemoryRequirement::HostVisible));
	const Unique<VkBuffer>			stencilBuffer		(makeBuffer				(vk, device, stencilBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		stencilBufferAlloc	(bindBuffer				(vk, device, allocator, *stencilBuffer, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>	vertModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass			(makeRenderPass			(vk, device, colorFormat, dsFormat, sampleCountFlag));
	const Unique<VkFramebuffer>		framebuffer			(makeFramebuffer		(vk, device, *renderPass, m_imageExtent2D, &colorFormat, m_colorImageUsage, &dsFormat, m_dsImageUsage));
	const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout		(vk, device));
	const Unique<VkPipeline>		pipeline			(makeGraphicsPipeline	(vk, device, *pipelineLayout, *renderPass, *vertModule, *fragModule, m_imageExtent2D, ASPECT_DEPTH_STENCIL));
	const Unique<VkCommandPool>		cmdPool				(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const std::vector<float>		vertexArray			(getVertices());
	const deUint32					vertexCount			(static_cast<deUint32>(vertexArray.size() / 4u));
	const VkDeviceSize				vertexArraySize		(vertexArray.size() * sizeof(vertexArray[0]));
	const Unique<VkBuffer>			vertexBuffer		(makeBuffer				(vk, device, vertexArraySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc	(bindBuffer				(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));
	const VkDeviceSize				vertexBufferOffset	(0u);

	fillBuffer(vk, device, *vertexBufferAlloc, &vertexArray[0], vertexArraySize);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const VkImageView							attachments[]					= { *colorAttachment, *dsAttachment };
		const VkRenderPassAttachmentBeginInfo		renderPassAttachmentBeginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			DE_LENGTH_OF_ARRAY(attachments),							//  deUint32			attachmentCount;
			attachments													//  const VkImageView*	pAttachments;
		};

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor, clearDepth, clearStencil, &renderPassAttachmentBeginInfo);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		// Color image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *colorImage, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}

		// Depth/Stencil image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier		= makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dsImage, dsSubresRange);
			const VkBufferImageCopy		depthCopyRegion		= makeBufferImageCopy(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																				  makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u));
			const VkBufferImageCopy		stencilCopyRegion	= makeBufferImageCopy(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																				  makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarriers[]	=
			{
				makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *depthBuffer, 0ull, VK_WHOLE_SIZE),
				makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *stencilBuffer, 0ull, VK_WHOLE_SIZE),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *dsImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *depthBuffer, 1u, &depthCopyRegion);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *dsImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *stencilBuffer, 1u, &stencilCopyRegion);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(postCopyBarriers), postCopyBarriers, DE_NULL, 0u);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	{
		std::string result;

		if (!verifyBuffer(colorBufferAlloc, colorFormat, "Color", ASPECT_COLOR))
			result += " Color";

		if (!verifyBuffer(depthBufferAlloc, dsFormat, "Depth", ASPECT_DEPTH))
			result += " Depth";

		if (!verifyBuffer(stencilBufferAlloc, stencilBufferFormat, "Stencil", ASPECT_STENCIL))
			result += " Stencil";

		if (result.empty())
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Following parts of image are incorrect:" + result);
	}
}

class ColorResolveImagelessTestInstance : public ColorImagelessTestInstance
{
public:
										ColorResolveImagelessTestInstance	(Context& context, const TestParameters& parameters);
protected:
	virtual tcu::TestStatus				iterate								(void);

	virtual MovePtr<tcu::TextureLevel>	generateReferenceImage				(const tcu::TextureFormat&	textureFormat,
																			 const AspectFlags			aspectFlags,
																			 const deUint32				sample,
																			 const deUint32				subpass);

	virtual std::vector<float>			getVertices							(void);
};

ColorResolveImagelessTestInstance::ColorResolveImagelessTestInstance (Context& context, const TestParameters& parameters)
	: ColorImagelessTestInstance	(context, parameters)
{
	const InstanceInterface&	vki			= m_context.getInstanceInterface();
	const VkPhysicalDevice		physDevice	= m_context.getPhysicalDevice();

	// To validate per-sample image image must also be sampled
	m_colorImageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	checkImageFormatProperties(vki, physDevice, m_parameters.colorFormat, m_colorImageUsage, m_imageExtent2D);
}

MovePtr<tcu::TextureLevel> ColorResolveImagelessTestInstance::generateReferenceImage (const tcu::TextureFormat&	textureFormat,
																					  const AspectFlags			aspectFlags,
																					  const deUint32			sample,
																					  const deUint32			subpass)
{
	const int					width			= m_imageExtent2D.width;
	const int					height			= m_imageExtent2D.height;
	MovePtr<tcu::TextureLevel>	image			(new tcu::TextureLevel(textureFormat, width, height));
	tcu::PixelBufferAccess		access			(image->getAccess());
	const int					componentValue	(static_cast<int>(0.75f * 0x100));
	const tcu::RGBA				colorDrawRGBA	(tcu::RGBA(componentValue, componentValue, componentValue, 0xFF));
	const tcu::Vec4				colorDraw		(colorDrawRGBA.toVec());
	const tcu::Vec4				colorFill		(tcu::RGBA::black().toVec());
	const tcu::Vec4				colorEdge0		(colorDraw);
	const tcu::Vec4				colorEdge1		(colorFill);
	const tcu::Vec4				colorEdge2		(colorDraw);
	const tcu::Vec4				colorEdge3		(colorFill);
	const tcu::Vec4				colorEdgeR		((colorDraw.x() + colorFill.x()) / 2, (colorDraw.y() + colorFill.y()) / 2, (colorDraw.z() + colorFill.z()) / 2, colorDraw.w()); // AVERAGE
	const tcu::Vec4&			colorEdge		= sample == 0 ? colorEdge0
												: sample == 1 ? colorEdge1
												: sample == 2 ? colorEdge2
												: sample == 3 ? colorEdge3
												: colorEdgeR;

	DE_UNREF(aspectFlags);
	DE_ASSERT(dePop32(aspectFlags) == 1);
	DE_ASSERT(aspectFlags == ASPECT_COLOR);
	DE_UNREF(subpass);
	DE_ASSERT(subpass == NO_SUBPASS);

	for (int y = 0; y < height; ++y)
	for (int x = 0; x < width; ++x)
	{
		const int			mx			= width - 1 - x;
		const tcu::Vec4&	validColor	= (y == mx) ? colorEdge
										: (y >  mx) ? colorFill
										: colorDraw;

		access.setPixel(validColor, x, y);
	}

	return image;
}

std::vector<float> ColorResolveImagelessTestInstance::getVertices (void)
{
	const float					verticesData[]	=
	{
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f, -1.0f, 0.0f, 1.0f,
	};
	const std::vector<float>	vertices		(verticesData, verticesData + DE_LENGTH_OF_ARRAY(verticesData));

	return vertices;
}

tcu::TestStatus ColorResolveImagelessTestInstance::iterate (void)
{
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue					= m_context.getUniversalQueue();
	Allocator&						allocator				= m_context.getDefaultAllocator();

	const VkSampleCountFlagBits		sampleCount				= VK_SAMPLE_COUNT_4_BIT;
	const tcu::Vec4					clearColor				= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const VkFormat					colorFormat				= m_parameters.colorFormat;
	const VkDeviceSize				colorBufferSize			= m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkImageSubresourceRange	colorSubresRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const Unique<VkImage>			colorImage				(makeImage				(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage, sampleCount)));
	const UniquePtr<Allocation>		colorImageAlloc			(bindImage				(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorAttachment			(makeImageView			(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));

	const Unique<VkImage>			colorResolveImage		(makeImage				(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage)));
	const UniquePtr<Allocation>		colorResolveImageAlloc	(bindImage				(vk, device, allocator, *colorResolveImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorResolveAttachment	(makeImageView			(vk, device, *colorResolveImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>			colorResolveBuffer		(makeBuffer				(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorResolveBufferAlloc	(bindBuffer				(vk, device, allocator, *colorResolveBuffer, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>	vertModule				(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragModule				(createShaderModule		(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass				(makeRenderPass			(vk, device, colorFormat, m_parameters.dsFormat, sampleCount));
	const Unique<VkFramebuffer>		framebuffer				(makeFramebuffer		(vk, device, *renderPass, m_imageExtent2D, &colorFormat, m_colorImageUsage, &m_parameters.dsFormat, 0u, ASPECT_COLOR));
	const Unique<VkPipelineLayout>	pipelineLayout			(makePipelineLayout		(vk, device));
	const Unique<VkPipeline>		pipeline				(makeGraphicsPipeline	(vk, device, *pipelineLayout, *renderPass, *vertModule, *fragModule, m_imageExtent2D, ASPECT_NONE, sampleCount));
	const Unique<VkCommandPool>		cmdPool					(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer				(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const std::vector<float>		vertexArray				(getVertices());
	const deUint32					vertexCount				(static_cast<deUint32>(vertexArray.size() / 4u));
	const VkDeviceSize				vertexArraySize			(vertexArray.size() * sizeof(vertexArray[0]));
	const Unique<VkBuffer>			vertexBuffer			(makeBuffer				(vk, device, vertexArraySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc		(bindBuffer				(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));
	const VkDeviceSize				vertexBufferOffset		(0u);

	fillBuffer(vk, device, *vertexBufferAlloc, &vertexArray[0], vertexArraySize);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const VkImageView							attachments[]					= { *colorAttachment, *colorResolveAttachment };
		const VkRenderPassAttachmentBeginInfo		renderPassAttachmentBeginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			DE_LENGTH_OF_ARRAY(attachments),							//  deUint32			attachmentCount;
			attachments													//  const VkImageView*	pAttachments;
		};

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor, &renderPassAttachmentBeginInfo);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		// Color image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *colorResolveImage, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorResolveBuffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *colorResolveImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorResolveBuffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	{
		std::string result;

		if (!verifyBuffer(colorResolveBufferAlloc, colorFormat, "ResolveColor", ASPECT_COLOR))
			result += " ResolveColor";

		// Parse color aspect of separate samples of multisample image
		for (deUint32 sampleNdx = 0; sampleNdx < sampleCount; ++sampleNdx)
		{
			const std::string				name				("Color" + de::toString(sampleNdx));
			const Unique<VkImage>			imageSample			(makeImage	(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage)));
			const UniquePtr<Allocation>		imageSampleAlloc	(bindImage	(vk, device, allocator, *imageSample, MemoryRequirement::Any));
			const Unique<VkBuffer>			imageBuffer			(makeBuffer	(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
			const UniquePtr<Allocation>		imageBufferAlloc	(bindBuffer	(vk, device, allocator, *imageBuffer, MemoryRequirement::HostVisible));

			readOneSampleFromMultisampleImage(colorFormat, colorImage, sampleNdx, colorFormat, imageSample, imageBuffer, ASPECT_COLOR);

			if (!verifyBuffer(imageBufferAlloc, colorFormat, name, ASPECT_COLOR, sampleNdx))
				result += " " + name;
		}


		if (result.empty())
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Fail");
	}
}

class DepthResolveImagelessTestInstance : public DepthImagelessTestInstance
{
public:
										DepthResolveImagelessTestInstance	(Context& context, const TestParameters& parameters);

protected:
	virtual tcu::TestStatus				iterate								(void);

	virtual MovePtr<tcu::TextureLevel>	generateReferenceImage				(const tcu::TextureFormat&	textureFormat,
																			 const AspectFlags			aspectFlags,
																			 const deUint32				sample,
																			 const deUint32				subpass);

	virtual std::vector<float>			getVertices							(void);
};

DepthResolveImagelessTestInstance::DepthResolveImagelessTestInstance (Context& context, const TestParameters& parameters)
	: DepthImagelessTestInstance	(context, parameters)
{
	context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");

	const InstanceInterface&							vki					= m_context.getInstanceInterface();
	const VkPhysicalDevice								physDevice			= m_context.getPhysicalDevice();
	VkPhysicalDeviceProperties2							deviceProperties;
	VkPhysicalDeviceDepthStencilResolveProperties		dsResolveProperties;

	deMemset(&deviceProperties, 0, sizeof(deviceProperties));
	deMemset(&dsResolveProperties, 0, sizeof(dsResolveProperties));

	deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties.pNext = &dsResolveProperties;

	dsResolveProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
	dsResolveProperties.pNext = DE_NULL;

	vki.getPhysicalDeviceProperties2(physDevice, &deviceProperties);

	m_colorImageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	checkImageFormatProperties(vki, physDevice, m_parameters.colorFormat, m_colorImageUsage, m_imageExtent2D);

	m_dsImageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	checkImageFormatProperties(vki, physDevice, m_parameters.dsFormat, m_dsImageUsage, m_imageExtent2D);
}

MovePtr<tcu::TextureLevel> DepthResolveImagelessTestInstance::generateReferenceImage (const tcu::TextureFormat&	textureFormat,
																					  const AspectFlags			aspectFlags,
																					  const deUint32			sample,
																					  const deUint32			subpass)
{
	const bool					color	= ((aspectFlags & ASPECT_COLOR) != 0);
	const bool					depth	= ((aspectFlags & ASPECT_DEPTH) != 0);
	const bool					stencil	= ((aspectFlags & ASPECT_STENCIL) != 0);
	const int					width	= m_imageExtent2D.width;
	const int					height	= m_imageExtent2D.height;
	MovePtr<tcu::TextureLevel>	image	(new tcu::TextureLevel(textureFormat, width, height));
	tcu::PixelBufferAccess		access	(image->getAccess());

	DE_ASSERT(dePop32(aspectFlags) == 1);
	DE_UNREF(subpass);

	if (color)
	{
		const tcu::Vec4		colorDraw	(tcu::RGBA::blue().toVec());
		const tcu::Vec4		colorFill	(tcu::RGBA::black().toVec());
		const tcu::Vec4		colorEdge0	(colorDraw);
		const tcu::Vec4		colorEdge1	(colorFill);
		const tcu::Vec4		colorEdge2	(colorDraw);
		const tcu::Vec4		colorEdge3	(colorFill);
		const tcu::Vec4		colorEdgeR	((colorDraw.x() + colorFill.x()) / 2, (colorDraw.y() + colorFill.y()) / 2, (colorDraw.z() + colorFill.z()) / 2, colorDraw.w()); // AVERAGE
		const tcu::Vec4&	colorEdge	= sample == 0 ? colorEdge0
										: sample == 1 ? colorEdge1
										: sample == 2 ? colorEdge2
										: sample == 3 ? colorEdge3
										: colorEdgeR;

		for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			const int			mx			= width - 1 - x;
			const tcu::Vec4&	validColor	= (y == mx) ? colorEdge
											: (y >  mx) ? colorFill
											: colorDraw;

			access.setPixel(validColor, x, y);
		}
	}

	if (depth)
	{
		const int			colorFillValue	(static_cast<int>(1.00f * 0x100));
		const int			colorDrawValue	(static_cast<int>(0.00f * 0x100));
		const tcu::IVec4	colorFill		(colorFillValue, colorFillValue, colorFillValue, 0xFF);
		const tcu::IVec4	colorDraw		(colorDrawValue, colorDrawValue, colorDrawValue, 0xFF);
		const tcu::IVec4	colorEdge0		(colorDraw);
		const tcu::IVec4	colorEdge1		(colorFill);
		const tcu::IVec4	colorEdge2		(colorDraw);
		const tcu::IVec4	colorEdge3		(colorFill);
		const tcu::IVec4	colorEdgeR		(colorEdge0); // SAMPLE_ZERO
		const tcu::IVec4&	colorEdge		= sample == 0 ? colorEdge0
											: sample == 1 ? colorEdge1
											: sample == 2 ? colorEdge2
											: sample == 3 ? colorEdge3
											: colorEdgeR;

		for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			const int			mx			= width - 1 - x;
			const tcu::IVec4&	validColor	= (y == mx) ? colorEdge
											: (y >  mx) ? colorFill
											: colorDraw;

			access.setPixel(validColor, x, y);
		}
	}

	if (stencil)
	{
		const int			colorFillValue	((0 * 0x100) / 4);
		const int			colorDrawValue	((1 * 0x100) / 4);
		const tcu::IVec4	colorFill		(colorFillValue, colorFillValue, colorFillValue, 0xFF);
		const tcu::IVec4	colorDraw		(colorDrawValue, colorDrawValue, colorDrawValue, 0xFF);
		const tcu::IVec4	colorEdge0		(colorDraw);
		const tcu::IVec4	colorEdge1		(colorFill);
		const tcu::IVec4	colorEdge2		(colorDraw);
		const tcu::IVec4	colorEdge3		(colorFill);
		const tcu::IVec4	colorEdgeR		(colorEdge0); // SAMPLE_ZERO
		const tcu::IVec4&	colorEdge		= sample == 0 ? colorEdge0
											: sample == 1 ? colorEdge1
											: sample == 2 ? colorEdge2
											: sample == 3 ? colorEdge3
											: colorEdgeR;

		for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			const int			mx			= width - 1 - x;
			const tcu::IVec4&	validColor	= (y == mx) ? colorEdge
											: (y >  mx) ? colorFill
											: colorDraw;

			access.setPixel(validColor, x, y);
		}
	}

	return image;
}

std::vector<float> DepthResolveImagelessTestInstance::getVertices (void)
{
	const float					verticesData[]	=
	{
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.5f, 1.0f,
		-1.0f, +1.0f, 0.5f, 1.0f,
		+1.0f, -1.0f, 0.5f, 1.0f,
	};
	const std::vector<float>	vertices		(verticesData, verticesData + DE_LENGTH_OF_ARRAY(verticesData));

	return vertices;
}

tcu::TestStatus DepthResolveImagelessTestInstance::iterate (void)
{
	const DeviceInterface&			vk							= m_context.getDeviceInterface();
	const VkDevice					device						= m_context.getDevice();
	const deUint32					queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue						= m_context.getUniversalQueue();
	Allocator&						allocator					= m_context.getDefaultAllocator();

	const deUint32					sampleCount					= 4u;
	const VkSampleCountFlagBits		sampleCountFlag				= sampleCountBitFromSampleCount(sampleCount);
	const tcu::Vec4					clearColor					= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const VkFormat					colorFormat					= m_parameters.colorFormat;
	const VkDeviceSize				colorBufferSize				= m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkImageSubresourceRange	colorSubresRange			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const Unique<VkImage>			colorImage					(makeImage				(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage, sampleCountFlag)));
	const UniquePtr<Allocation>		colorImageAlloc				(bindImage				(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorAttachment				(makeImageView			(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));

	const Unique<VkImage>			colorResolveImage			(makeImage				(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage)));
	const UniquePtr<Allocation>		colorResolveImageAlloc		(bindImage				(vk, device, allocator, *colorResolveImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorResolveAttachment		(makeImageView			(vk, device, *colorResolveImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>			colorResolveBuffer			(makeBuffer				(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorResolveBufferAlloc		(bindBuffer				(vk, device, allocator, *colorResolveBuffer, MemoryRequirement::HostVisible));

	const float						clearDepth					= 1.0f;
	const deUint32					clearStencil				= 0u;
	const VkFormat					dsFormat					= m_parameters.dsFormat;
	const deUint32					dsImagePixelSize			= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(dsFormat)));
	const VkImageAspectFlags		dsAspectFlags				= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	const VkImageSubresourceRange	dsSubresRange				= makeImageSubresourceRange(dsAspectFlags, 0u, 1u, 0u, 1u);

	const VkDeviceSize				depthBufferSize				= m_imageExtent2D.width * m_imageExtent2D.height * dsImagePixelSize;
	const VkFormat					stencilBufferFormat			= getStencilBufferFormat(dsFormat);
	const deUint32					stencilPixelSize			= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(stencilBufferFormat)));
	const VkDeviceSize				stencilBufferSize			= m_imageExtent2D.width * m_imageExtent2D.height * stencilPixelSize;

	const Unique<VkImage>			dsImage						(makeImage				(vk, device, makeImageCreateInfo(dsFormat, m_imageExtent2D, m_dsImageUsage, sampleCountFlag)));
	const UniquePtr<Allocation>		dsImageAlloc				(bindImage				(vk, device, allocator, *dsImage, MemoryRequirement::Any));
	const Unique<VkImageView>		dsAttachment				(makeImageView			(vk, device, *dsImage, VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSubresRange));

	const Unique<VkImage>			dsResolveImage				(makeImage				(vk, device, makeImageCreateInfo(dsFormat, m_imageExtent2D, m_dsImageUsage)));
	const UniquePtr<Allocation>		dsResolveImageAlloc			(bindImage				(vk, device, allocator, *dsResolveImage, MemoryRequirement::Any));
	const Unique<VkImageView>		dsResolveAttachment			(makeImageView			(vk, device, *dsResolveImage, VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSubresRange));
	const Unique<VkBuffer>			depthResolveBuffer			(makeBuffer				(vk, device, depthBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		depthResolveBufferAlloc		(bindBuffer				(vk, device, allocator, *depthResolveBuffer, MemoryRequirement::HostVisible));
	const Unique<VkBuffer>			stencilResolveBuffer		(makeBuffer				(vk, device, stencilBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		stencilResolveBufferAlloc	(bindBuffer				(vk, device, allocator, *stencilResolveBuffer, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>	vertModule					(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragModule					(createShaderModule		(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass					(makeRenderPass			(vk, device, colorFormat, m_parameters.dsFormat, sampleCountFlag, sampleCountFlag));
	const Unique<VkFramebuffer>		framebuffer					(makeFramebuffer		(vk, device, *renderPass, m_imageExtent2D, &colorFormat, m_colorImageUsage, &m_parameters.dsFormat, m_dsImageUsage, ASPECT_COLOR|ASPECT_DEPTH_STENCIL));
	const Unique<VkPipelineLayout>	pipelineLayout				(makePipelineLayout		(vk, device));
	const Unique<VkPipeline>		pipeline					(makeGraphicsPipeline	(vk, device, *pipelineLayout, *renderPass, *vertModule, *fragModule, m_imageExtent2D, ASPECT_DEPTH_STENCIL, sampleCountFlag));
	const Unique<VkCommandPool>		cmdPool						(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer					(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const std::vector<float>		vertexArray					(getVertices());
	const deUint32					vertexCount					(static_cast<deUint32>(vertexArray.size() / 4u));
	const VkDeviceSize				vertexArraySize				(vertexArray.size() * sizeof(vertexArray[0]));
	const Unique<VkBuffer>			vertexBuffer				(makeBuffer				(vk, device, vertexArraySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc			(bindBuffer				(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));
	const VkDeviceSize				vertexBufferOffset			(0u);

	fillBuffer(vk, device, *vertexBufferAlloc, &vertexArray[0], vertexArraySize);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const VkImageView							attachments[]					= { *colorAttachment, *dsAttachment, *colorResolveAttachment, *dsResolveAttachment };
		const VkRenderPassAttachmentBeginInfo		renderPassAttachmentBeginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			DE_LENGTH_OF_ARRAY(attachments),							//  deUint32			attachmentCount;
			attachments													//  const VkImageView*	pAttachments;
		};

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor, clearDepth, clearStencil, &renderPassAttachmentBeginInfo);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		// Color resolve image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *colorResolveImage, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorResolveBuffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *colorResolveImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorResolveBuffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}

		// Depth/Stencil resolve image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *dsResolveImage, dsSubresRange);
			const VkBufferImageCopy		depthCopyRegion		= makeBufferImageCopy(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																				  makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u));
			const VkBufferImageCopy		stencilCopyRegion	= makeBufferImageCopy(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																				  makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarriers[]	=
			{
				makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *depthResolveBuffer, 0ull, VK_WHOLE_SIZE),
				makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *stencilResolveBuffer, 0ull, VK_WHOLE_SIZE),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *dsResolveImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *depthResolveBuffer, 1u, &depthCopyRegion);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *dsResolveImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *stencilResolveBuffer, 1u, &stencilCopyRegion);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(postCopyBarriers), postCopyBarriers, DE_NULL, 0u);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	{
		std::string result;

		if (!verifyBuffer(colorResolveBufferAlloc, colorFormat, "ResolveColor", ASPECT_COLOR))
			result += " ResolveColor";

		if (!verifyBuffer(depthResolveBufferAlloc, dsFormat, "ResolveDepth", ASPECT_DEPTH))
			result += " ResolveDepth";

		if (!verifyBuffer(stencilResolveBufferAlloc, stencilBufferFormat, "ResolveStencil", ASPECT_STENCIL))
			result += " ResolveStencil";

		// Parse color aspect of separate samples of multisample image
		for (deUint32 sampleNdx = 0; sampleNdx < sampleCount; ++sampleNdx)
		{
			const std::string				name				("Color" + de::toString(sampleNdx));
			const Unique<VkImage>			imageSample			(makeImage	(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage)));
			const UniquePtr<Allocation>		imageSampleAlloc	(bindImage	(vk, device, allocator, *imageSample, MemoryRequirement::Any));
			const Unique<VkBuffer>			imageBuffer			(makeBuffer	(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
			const UniquePtr<Allocation>		imageBufferAlloc	(bindBuffer	(vk, device, allocator, *imageBuffer, MemoryRequirement::HostVisible));

			readOneSampleFromMultisampleImage(colorFormat, colorImage, sampleNdx, colorFormat, imageSample, imageBuffer, ASPECT_COLOR);

			if (!verifyBuffer(imageBufferAlloc, colorFormat, name, ASPECT_COLOR, sampleNdx))
				result += " " + name;
		}

		// Parse depth aspect of separate samples of multisample image
		for (deUint32 sampleNdx = 0; sampleNdx < sampleCount; ++sampleNdx)
		{
			const std::string				name				("Depth" + de::toString(sampleNdx));
			const Unique<VkImage>			imageSample			(makeImage	(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage)));
			const UniquePtr<Allocation>		imageSampleAlloc	(bindImage	(vk, device, allocator, *imageSample, MemoryRequirement::Any));
			const Unique<VkBuffer>			imageBuffer			(makeBuffer	(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
			const UniquePtr<Allocation>		imageBufferAlloc	(bindBuffer	(vk, device, allocator, *imageBuffer, MemoryRequirement::HostVisible));

			readOneSampleFromMultisampleImage(dsFormat, dsImage, sampleNdx, colorFormat, imageSample, imageBuffer, ASPECT_DEPTH);

			if (!verifyBuffer(imageBufferAlloc, colorFormat, name, ASPECT_DEPTH, sampleNdx))
				result += " " + name;
		}

		// Parse stencil aspect of separate samples of multisample image
		for (deUint32 sampleNdx = 0; sampleNdx < sampleCount; ++sampleNdx)
		{
			const std::string				name				("Stencil" + de::toString(sampleNdx));
			const Unique<VkImage>			imageSample			(makeImage	(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage)));
			const UniquePtr<Allocation>		imageSampleAlloc	(bindImage	(vk, device, allocator, *imageSample, MemoryRequirement::Any));
			const Unique<VkBuffer>			imageBuffer			(makeBuffer	(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
			const UniquePtr<Allocation>		imageBufferAlloc	(bindBuffer	(vk, device, allocator, *imageBuffer, MemoryRequirement::HostVisible));

			readOneSampleFromMultisampleImage(dsFormat, dsImage, sampleNdx, colorFormat, imageSample, imageBuffer, ASPECT_STENCIL);

			if (!verifyBuffer(imageBufferAlloc, colorFormat, name, ASPECT_STENCIL, sampleNdx))
				result += " " + name;
		}

		if (result.empty())
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Following parts of image are incorrect:" + result);
	}
}

class MultisubpassTestInstance : public ColorImagelessTestInstance
{
public:
										MultisubpassTestInstance	(Context& context, const TestParameters& parameters);

protected:
	virtual tcu::TestStatus				iterate						(void);

	virtual std::vector<float>			getVertices					(void);

	virtual MovePtr<tcu::TextureLevel>	generateReferenceImage		(const tcu::TextureFormat&	textureFormat,
																	 const AspectFlags			aspectFlags,
																	 const deUint32				sample,
																	 const deUint32				subpass);
};

MultisubpassTestInstance::MultisubpassTestInstance (Context& context, const TestParameters& parameters)
	: ColorImagelessTestInstance	(context, parameters)
{
}

std::vector<float> MultisubpassTestInstance::getVertices (void)
{
	const float					verticesData[]	=
	{
		-1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f,  0.0f, 0.0f, 1.0f,
		+1.0f, +1.0f, 0.0f, 1.0f,
	};
	const std::vector<float>	vertices		(verticesData, verticesData + DE_LENGTH_OF_ARRAY(verticesData));

	return vertices;
}

MovePtr<tcu::TextureLevel> MultisubpassTestInstance::generateReferenceImage (const tcu::TextureFormat&	textureFormat,
																			 const AspectFlags			aspectFlags,
																			 const deUint32				sample,
																			 const deUint32				subpass)
{
	const int					width			= m_imageExtent2D.width;
	const int					height			= m_imageExtent2D.height;
	const tcu::Vec4				colorDraw0		(0.0f, 0.0f, 1.0f, 1.0f);
	const tcu::Vec4				colorFill0		(tcu::RGBA::black().toVec());
	const tcu::Vec4				colorDraw1		(colorDraw0.x(), 1.0f, colorDraw0.z(), 1.0f);
	const tcu::Vec4				colorFill1		(colorFill0.x(), 1.0f, colorFill0.z(), 1.0f);
	const tcu::Vec4&			colorDraw		((subpass == 0) ? colorDraw0 : colorDraw1);
	const tcu::Vec4&			colorFill		((subpass == 0) ? colorFill0 : colorFill1);
	MovePtr<tcu::TextureLevel>	image			(new tcu::TextureLevel(textureFormat, width, height));
	tcu::PixelBufferAccess		access			(image->getAccess());

	DE_UNREF(aspectFlags);
	DE_ASSERT(aspectFlags == ASPECT_COLOR);
	DE_UNREF(sample);
	DE_ASSERT(sample == NO_SAMPLE);
	DE_ASSERT(subpass != NO_SUBPASS);

	for (int y = 0; y < height; ++y)
	{
		const tcu::Vec4&	validColor	= (y < height / 2) ? colorFill : colorDraw;

		for (int x = 0; x < width; ++x)
			access.setPixel(validColor, x, y);
	}

	return image;
}

tcu::TestStatus MultisubpassTestInstance::iterate (void)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= m_context.getUniversalQueue();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	const tcu::Vec4						clearColor			= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const VkFormat						colorFormat			= m_parameters.colorFormat;
	const VkDeviceSize					colorBufferSize		= m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkImageSubresourceRange		colorSubresRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const Unique<VkImage>				color0Image			(makeImage				(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)));
	const UniquePtr<Allocation>			color0ImageAlloc	(bindImage				(vk, device, allocator, *color0Image, MemoryRequirement::Any));
	const Unique<VkImageView>			color0Attachment	(makeImageView			(vk, device, *color0Image, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>				color0Buffer		(makeBuffer				(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			color0BufferAlloc	(bindBuffer				(vk, device, allocator, *color0Buffer, MemoryRequirement::HostVisible));

	const Unique<VkImage>				color1Image			(makeImage				(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)));
	const UniquePtr<Allocation>			color1ImageAlloc	(bindImage				(vk, device, allocator, *color1Image, MemoryRequirement::Any));
	const Unique<VkImageView>			color1Attachment	(makeImageView			(vk, device, *color1Image, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>				color1Buffer		(makeBuffer				(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			color1BufferAlloc	(bindBuffer				(vk, device, allocator, *color1Buffer, MemoryRequirement::HostVisible));

	const VkDescriptorType				descriptorType		(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
	const Unique<VkDescriptorSetLayout>	descriptorSetLayout	(DescriptorSetLayoutBuilder()
		.addSingleBinding(descriptorType, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));
	const Unique<VkDescriptorPool>		descriptorPool		(DescriptorPoolBuilder()
		.addType(descriptorType)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	const Unique<VkDescriptorSet>		descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorImageInfo			imageDescriptorInfo	(makeDescriptorImageInfo(DE_NULL, *color0Attachment, VK_IMAGE_LAYOUT_GENERAL));

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &imageDescriptorInfo)
		.update(vk, device);

	const Unique<VkRenderPass>			renderPass			(makeRenderPass			(vk, device, colorFormat, DE_NULL));
	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer		(vk, device, *renderPass, m_imageExtent2D, &colorFormat, m_colorImageUsage, &m_parameters.dsFormat, 0u, ASPECT_NONE, 1u));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const Unique<VkShaderModule>		vertModule0			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		fragModule0			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkPipelineLayout>		pipelineLayout0		(makePipelineLayout		(vk, device));
	const Unique<VkPipeline>			pipeline0			(makeGraphicsPipeline	(vk, device, *pipelineLayout0, *renderPass, *vertModule0, *fragModule0, m_imageExtent2D));

	const Unique<VkShaderModule>		vertModule1			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert1"), 0u));
	const Unique<VkShaderModule>		fragModule1			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("frag1"), 0u));
	const Unique<VkPipelineLayout>		pipelineLayout1		(makePipelineLayout		(vk, device, 1u, &*descriptorSetLayout));
	const Unique<VkPipeline>			pipeline1			(makeGraphicsPipeline	(vk, device, *pipelineLayout1, *renderPass, *vertModule1, *fragModule1, m_imageExtent2D, 0u, VK_SAMPLE_COUNT_1_BIT, 1u));

	const std::vector<float>			vertex0Array		(getVertices());
	const deUint32						vertex0Count		(static_cast<deUint32>(vertex0Array.size() / 4u));
	const VkDeviceSize					vertex0ArraySize	(vertex0Array.size() * sizeof(vertex0Array[0]));
	const Unique<VkBuffer>				vertex0Buffer		(makeBuffer				(vk, device, vertex0ArraySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>			vertex0BufferAlloc	(bindBuffer				(vk, device, allocator, *vertex0Buffer, MemoryRequirement::HostVisible));
	const VkDeviceSize					vertex0BufferOffset	(0u);

	const std::vector<float>			vertex1Array		(getFullQuadVertices());
	const deUint32						vertex1Count		(static_cast<deUint32>(vertex1Array.size() / 4u));
	const VkDeviceSize					vertex1ArraySize	(vertex1Array.size() * sizeof(vertex1Array[0]));
	const Unique<VkBuffer>				vertex1Buffer		(makeBuffer				(vk, device, vertex1ArraySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>			vertex1BufferAlloc	(bindBuffer				(vk, device, allocator, *vertex1Buffer, MemoryRequirement::HostVisible));
	const VkDeviceSize					vertex1BufferOffset	(0u);

	fillBuffer(vk, device, *vertex0BufferAlloc, &vertex0Array[0], vertex0ArraySize);
	fillBuffer(vk, device, *vertex1BufferAlloc, &vertex1Array[0], vertex1ArraySize);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const VkImageView							attachments[]					= { *color0Attachment, *color1Attachment };
		const VkRenderPassAttachmentBeginInfo		renderPassAttachmentBeginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			DE_LENGTH_OF_ARRAY(attachments),							//  deUint32			attachmentCount;
			&attachments[0]												//  const VkImageView*	pAttachments;
		};

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor, &renderPassAttachmentBeginInfo);
		{
			{
				vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline0);

				vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertex0Buffer, &vertex0BufferOffset);

				vk.cmdDraw(*cmdBuffer, vertex0Count, 1u, 0u, 0u);
			}

			vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

			{
				vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline1);

				vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertex1Buffer, &vertex1BufferOffset);

				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout1, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

				vk.cmdDraw(*cmdBuffer, vertex1Count, 1u, 0u, 0u);
			}
		}
		endRenderPass(vk, *cmdBuffer);

		// Subpass0 color image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *color0Image, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *color0Buffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *color0Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *color0Buffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}

		// Subpass1 color image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *color1Image, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *color1Buffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *color1Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *color1Buffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	{
		std::string result;

		if (!verifyBuffer(color0BufferAlloc, colorFormat, "ColorSubpass0", ASPECT_COLOR, NO_SAMPLE, 0u))
			result += " ColorSubpass0";

		if (!verifyBuffer(color1BufferAlloc, colorFormat, "ColorSubpass1", ASPECT_COLOR, NO_SAMPLE, 1u))
			result += " ColorSubpass1";

		if (result.empty())
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Following parts of image are incorrect:" + result);
	}
}


class DifferentAttachmentsTestInstance : public ColorImagelessTestInstance
{
public:
	DifferentAttachmentsTestInstance(Context& context, const TestParameters& parameters);

protected:
	virtual tcu::TestStatus				iterate(void);

	virtual std::vector<float>			getVertices(void);

	virtual MovePtr<tcu::TextureLevel>	generateReferenceImage(const tcu::TextureFormat& textureFormat,
															   const AspectFlags		 aspectFlags,
															   const deUint32			 sample,
															   const deUint32			 subpass);
};

DifferentAttachmentsTestInstance::DifferentAttachmentsTestInstance(Context& context, const TestParameters& parameters)
	: ColorImagelessTestInstance(context, parameters)
{
}

std::vector<float> DifferentAttachmentsTestInstance::getVertices(void)
{
	const float					verticesData[] =
	{
		-1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 1.0f,
		+1.0f,  0.0f, 0.0f, 1.0f,
		+1.0f, +1.0f, 0.0f, 1.0f,
	};
	const std::vector<float>	vertices(verticesData, verticesData + DE_LENGTH_OF_ARRAY(verticesData));

	return vertices;
}

MovePtr<tcu::TextureLevel> DifferentAttachmentsTestInstance::generateReferenceImage(const tcu::TextureFormat& textureFormat,
																					const AspectFlags			aspectFlags,
																					const deUint32				sample,
																					const deUint32)
{
	const int					width			= m_imageExtent2D.width;
	const int					height			= m_imageExtent2D.height;
	const tcu::Vec4				colorDraw		(0.0f, 0.0f, 1.0f, 1.0f);
	const tcu::Vec4				colorFill		(tcu::RGBA::black().toVec());
	MovePtr<tcu::TextureLevel>	image			(new tcu::TextureLevel(textureFormat, width, height));
	tcu::PixelBufferAccess		access			(image->getAccess());

	DE_UNREF(aspectFlags);
	DE_ASSERT(aspectFlags == ASPECT_COLOR);
	DE_UNREF(sample);
	DE_ASSERT(sample == NO_SAMPLE);

	for (int y = 0; y < height; ++y)
	{
		const tcu::Vec4&	validColor	= (y < height / 2) ? colorFill : colorDraw;

		for (int x = 0; x < width; ++x)
			access.setPixel(validColor, x, y);
	}

	return image;
}

tcu::TestStatus DifferentAttachmentsTestInstance::iterate(void)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= m_context.getUniversalQueue();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	const tcu::Vec4						clearColor			= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const VkFormat						colorFormat			= m_parameters.colorFormat;
	const VkDeviceSize					colorBufferSize		= m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkImageSubresourceRange		colorSubresRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const Unique<VkImage>				color0Image			(makeImage						(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)));
	const UniquePtr<Allocation>			color0ImageAlloc	(bindImage						(vk, device, allocator, *color0Image, MemoryRequirement::Any));
	const Unique<VkImageView>			color0Attachment	(makeImageView					(vk, device, *color0Image, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>				color0Buffer		(makeBuffer						(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			color0BufferAlloc	(bindBuffer						(vk, device, allocator, *color0Buffer, MemoryRequirement::HostVisible));

	const Unique<VkImage>				color1Image			(makeImage						(vk, device, makeImageCreateInfo(colorFormat, m_imageExtent2D, m_colorImageUsage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)));
	const UniquePtr<Allocation>			color1ImageAlloc	(bindImage						(vk, device, allocator, *color1Image, MemoryRequirement::Any));
	const Unique<VkImageView>			color1Attachment	(makeImageView					(vk, device, *color1Image, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>				color1Buffer		(makeBuffer						(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			color1BufferAlloc	(bindBuffer						(vk, device, allocator, *color1Buffer, MemoryRequirement::HostVisible));

	const Unique<VkRenderPass>			renderPass			(makeSingleAttachmentRenderPass	(vk, device, colorFormat, DE_NULL));
	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer				(vk, device, *renderPass, m_imageExtent2D, &colorFormat, m_colorImageUsage, &m_parameters.dsFormat));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool				(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer			(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const Unique<VkShaderModule>		vertModule			(createShaderModule				(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		fragModule			(createShaderModule				(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkPipelineLayout>		pipelineLayout		(makePipelineLayout				(vk, device));
	const Unique<VkPipeline>			pipeline			(makeGraphicsPipeline			(vk, device, *pipelineLayout, *renderPass, *vertModule, *fragModule, m_imageExtent2D));

	const std::vector<float>			vertexArray			(getVertices());
	const deUint32						vertexCount			(static_cast<deUint32>(vertexArray.size() / 4u));
	const VkDeviceSize					vertexArraySize		(vertexArray.size() * sizeof(vertexArray[0]));
	const Unique<VkBuffer>				vertexBuffer		(makeBuffer				(vk, device, vertexArraySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>			vertexBufferAlloc	(bindBuffer				(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));
	const VkDeviceSize					vertexBufferOffset	(0u);

	fillBuffer(vk, device, *vertexBufferAlloc, &vertexArray[0], vertexArraySize);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const VkRenderPassAttachmentBeginInfo		renderPassAttachmentBeginInfo0	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			1u,															//  deUint32			attachmentCount;
			&*color0Attachment											//  const VkImageView*	pAttachments;
		};

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor, &renderPassAttachmentBeginInfo0);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		const VkRenderPassAttachmentBeginInfo		renderPassAttachmentBeginInfo1	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		//  VkStructureType		sType;
			DE_NULL,													//  const void*			pNext;
			1u,															//  deUint32			attachmentCount;
			&*color1Attachment											//  const VkImageView*	pAttachments;
		};

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor, &renderPassAttachmentBeginInfo1);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		// Subpass0 color image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *color0Image, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *color0Buffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *color0Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *color0Buffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}

		// Subpass1 color image copy
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *color1Image, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy		(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																					 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier	(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *color1Buffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *color1Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *color1Buffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	{
		std::string result;

		if (!verifyBuffer(color0BufferAlloc, colorFormat, "ColorSubpass0", ASPECT_COLOR, NO_SAMPLE, 0u))
			result += " ColorSubpass0";

		if (!verifyBuffer(color1BufferAlloc, colorFormat, "ColorSubpass1", ASPECT_COLOR, NO_SAMPLE, 1u))
			result += " ColorSubpass1";

		if (result.empty())
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Following parts of image are incorrect:" + result);
	}
}

class BaseTestCase : public TestCase
{
public:
							BaseTestCase	(tcu::TestContext& context, const std::string& name, const std::string& description, const TestParameters& parameters);
	virtual					~BaseTestCase	(void);

protected:
	virtual void			checkSupport	(Context& context) const;
	virtual void			initPrograms	(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance	(Context& context) const;

	const TestParameters	m_parameters;
};

BaseTestCase::BaseTestCase (tcu::TestContext& context, const std::string& name, const std::string& description, const TestParameters& parameters)
	: TestCase		(context, name, description)
	, m_parameters	(parameters)
{
}

BaseTestCase::~BaseTestCase ()
{
}

void BaseTestCase::checkSupport (Context& context) const
{
	if (m_parameters.testType == TEST_TYPE_COLOR_RESOLVE || m_parameters.testType == TEST_TYPE_DEPTH_STENCIL_RESOLVE)
	{
		if (!context.getDeviceProperties().limits.standardSampleLocations)
			TCU_THROW(NotSupportedError, "Non-standard sample locations are not supported");
	}
}

void BaseTestCase::initPrograms (SourceCollections& programCollection) const
{
	// Vertex shader
	{
		std::ostringstream src;

		if (m_parameters.testType == TEST_TYPE_COLOR || m_parameters.testType == TEST_TYPE_COLOR_RESOLVE || m_parameters.testType == TEST_TYPE_DEPTH_STENCIL)
		{
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) in highp vec4 a_position;\n"
				<< "layout(location = 0) out highp vec4 a_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_Position = a_position;\n"
				<< "    if (gl_VertexIndex < 6)\n"
				<< "        a_color = vec4(0.75f, 0.75f, 0.75f, 1.0f);\n"
				<< "    else\n"
				<< "        a_color = vec4(1.00f, 1.00f, 1.00f, 1.0f);\n"
				<< "}\n";
		}

		if (m_parameters.testType == TEST_TYPE_DEPTH_STENCIL_RESOLVE)
		{
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) in highp vec4 a_position;\n"
				<< "layout(location = 0) out highp vec4 a_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_Position = a_position;\n"
				<< "    if (gl_VertexIndex < 3)\n"
				<< "        a_color = vec4(0.00f, 0.00f, 1.00f, 1.0f);\n"
				<< "    else\n"
				<< "        a_color = vec4(0.00f, 1.00f, 0.00f, 1.0f);\n"
				<< "}\n";
		}

		if (m_parameters.testType == TEST_TYPE_MULTISUBPASS || m_parameters.testType == TEST_TYPE_DIFFERENT_ATTACHMENTS)
		{
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) in highp vec4 a_position;\n"
				<< "layout(location = 0) out highp vec4 a_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_Position = a_position;\n"
				<< "    a_color = vec4(0.0f, 0.0f, 1.0f, 1.0f);\n"
				<< "}\n";
		}

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 a_color;\n"
			<< "layout(location = 0) out highp vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = a_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}

	// Additional shaders
	if (m_parameters.testType == TEST_TYPE_COLOR_RESOLVE || m_parameters.testType == TEST_TYPE_DEPTH_STENCIL_RESOLVE)
	{
		// Vertex shader
		{
			std::ostringstream src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) in highp vec4 a_position;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_Position = a_position;\n"
				<< "}\n";

			programCollection.glslSources.add("demultisample-vert") << glu::VertexSource(src.str());
		}

		// Fragment shader
		{
			// Color
			{
				std::ostringstream src;

				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform sampler2DMS u_ms_image_sampler;\n"
					<< "layout(push_constant) uniform PushConstantsBlock {\n"
					<< "    highp int sampleID;\n"
					<< "} pushConstants;\n"
					<< "layout(location = 0) out highp vec4 o_color;\n"
					<< "\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "    o_color = texelFetch(u_ms_image_sampler, ivec2(gl_FragCoord.xy), pushConstants.sampleID);\n"
					<< "}\n";

				programCollection.glslSources.add("demultisample-color-frag") << glu::FragmentSource(src.str());
			}

			// Depth
			{
				std::ostringstream src;

				// Depth-component textures are treated as one-component floating-point textures.
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
					<< "\n"
					<< "layout(binding = 0) uniform sampler2DMS u_ms_image_sampler;\n"
					<< "layout(push_constant) uniform PushConstantsBlock {\n"
					<< "    highp int sampleID;\n"
					<< "} pushConstants;\n"
					<< "layout(location = 0) out highp vec4 o_color;\n"
					<< "\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "    vec4 val = texelFetch(u_ms_image_sampler, ivec2(gl_FragCoord.xy), pushConstants.sampleID);\n"
					<< "    o_color = vec4(val.x, val.x, val.x, 1.0);\n"
					<< "}\n";

				programCollection.glslSources.add("demultisample-depth-frag") << glu::FragmentSource(src.str());
			}

			// Stencil
			{
				std::ostringstream src;

				// Stencil-component textures are treated as one-component unsigned integer textures.
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
					<< "\n"
					<< "layout(binding = 0) uniform usampler2DMS u_ms_image_sampler;\n"
					<< "layout(push_constant) uniform PushConstantsBlock {\n"
					<< "    highp int sampleID;\n"
					<< "} pushConstants;\n"
					<< "layout(location = 0) out highp vec4 o_color;\n"
					<< "\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "    uvec4 uVal = texelFetch(u_ms_image_sampler, ivec2(gl_FragCoord.xy), pushConstants.sampleID);\n"
					<< "    float val = float(uVal.x) / 4.0f;\n"
					<< "    o_color = vec4(val, val, val, 1.0);\n"
					<< "}\n";

				programCollection.glslSources.add("demultisample-stencil-frag") << glu::FragmentSource(src.str());
			}
		}
	}

	if (m_parameters.testType == TEST_TYPE_MULTISUBPASS)
	{
		// Vertex shader
		{
			std::ostringstream src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) in highp vec4 a_position;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_Position = a_position;\n"
				<< "}\n";

			programCollection.glslSources.add("vert1") << glu::VertexSource(src.str());
		}

		// Fragment shader
		{
			std::ostringstream src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput u_colors;\n"
				<< "layout(location = 0) out highp vec4 o_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    o_color = subpassLoad(u_colors);\n"
				<< "    o_color.g = 1.0f;\n"
				<< "    o_color.a = 1.0f;\n"
				<< "}\n";

			programCollection.glslSources.add("frag1") << glu::FragmentSource(src.str());
		}
	}


	return;
}

TestInstance*	BaseTestCase::createInstance (Context& context) const
{
	if (m_parameters.testType == TEST_TYPE_COLOR)
		return new ColorImagelessTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_DEPTH_STENCIL)
		return new DepthImagelessTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_COLOR_RESOLVE)
		return new ColorResolveImagelessTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_DEPTH_STENCIL_RESOLVE)
		return new DepthResolveImagelessTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_MULTISUBPASS)
		return new MultisubpassTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_DIFFERENT_ATTACHMENTS)
		return new DifferentAttachmentsTestInstance(context, m_parameters);


	TCU_THROW(InternalError, "Unknown test type specified");
}

tcu::TestNode*	imagelessColorTests (tcu::TestContext& testCtx)
{
	TestParameters	parameters	=
	{
		TEST_TYPE_COLOR,					//  TestType	testType;
		VK_FORMAT_R8G8B8A8_UNORM,			//  VkFormat	colorFormat;
		VK_FORMAT_UNDEFINED,				//  VkFormat	dsFormat;
	};

	return new BaseTestCase(testCtx, "color", "Imageless color attachment test", parameters);
}

tcu::TestNode*	imagelessDepthStencilTests (tcu::TestContext& testCtx)
{
	TestParameters	parameters	=
	{
		TEST_TYPE_DEPTH_STENCIL,			//  TestType	testType;
		VK_FORMAT_R8G8B8A8_UNORM,			//  VkFormat	colorFormat;
		VK_FORMAT_D24_UNORM_S8_UINT,		//  VkFormat	dsFormat;
	};

	return new BaseTestCase(testCtx, "depth_stencil", "Imageless depth/stencil attachment test", parameters);
}

tcu::TestNode*	imagelessColorResolveTests (tcu::TestContext& testCtx)
{
	TestParameters	parameters	=
	{
		TEST_TYPE_COLOR_RESOLVE,			//  TestType	testType;
		VK_FORMAT_R8G8B8A8_UNORM,			//  VkFormat	colorFormat;
		VK_FORMAT_UNDEFINED,				//  VkFormat	dsFormat;
	};

	return new BaseTestCase(testCtx, "color_resolve", "Imageless color attachment resolve test", parameters);
}

tcu::TestNode*	imagelessDepthStencilResolveTests (tcu::TestContext& testCtx)
{
	TestParameters	parameters	=
	{
		TEST_TYPE_DEPTH_STENCIL_RESOLVE,	//  TestType	testType;
		VK_FORMAT_R8G8B8A8_UNORM,			//  VkFormat	colorFormat;
		VK_FORMAT_D24_UNORM_S8_UINT,		//  VkFormat	dsFormat;
	};

	return new BaseTestCase(testCtx, "depth_stencil_resolve", "Imageless color and depth/stencil attachment resolve test", parameters);
}

tcu::TestNode* imagelessMultisubpass(tcu::TestContext& testCtx)
{
	TestParameters	parameters =
	{
		TEST_TYPE_MULTISUBPASS,			//  TestType	testType;
		VK_FORMAT_R8G8B8A8_UNORM,		//  VkFormat	colorFormat;
		VK_FORMAT_UNDEFINED,			//  VkFormat	dsFormat;
	};

	return new BaseTestCase(testCtx, "multisubpass", "Multi-subpass test", parameters);
}

tcu::TestNode* imagelessDifferentAttachments(tcu::TestContext& testCtx)
{
	TestParameters	parameters =
	{
		TEST_TYPE_DIFFERENT_ATTACHMENTS,	//  TestType	testType;
		VK_FORMAT_R8G8B8A8_UNORM,			//  VkFormat	colorFormat;
		VK_FORMAT_UNDEFINED,				//  VkFormat	dsFormat;
	};

	return new BaseTestCase(testCtx, "different_attachments", "Different attachments in multiple render passes", parameters);
}

}	// anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup> imagelessFramebufferGroup (new tcu::TestCaseGroup(testCtx, name.c_str(), "Imageless Framebuffer tests"));

	imagelessFramebufferGroup->addChild(imagelessColorTests(testCtx));						// Color only test
	imagelessFramebufferGroup->addChild(imagelessDepthStencilTests(testCtx));				// Color and depth/stencil test
	imagelessFramebufferGroup->addChild(imagelessColorResolveTests(testCtx));				// Color and color resolve test
	imagelessFramebufferGroup->addChild(imagelessDepthStencilResolveTests(testCtx));		// Color, depth and depth resolve test (interaction with VK_KHR_depth_stencil_resolve)
	imagelessFramebufferGroup->addChild(imagelessMultisubpass(testCtx));					// Multi-subpass test
	imagelessFramebufferGroup->addChild(imagelessDifferentAttachments(testCtx));			// Different attachments in multiple render passes

	return imagelessFramebufferGroup.release();
}

} // imageless
} // vkt
