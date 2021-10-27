/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Vulkan Dynamic Rendering Tests
 *//*--------------------------------------------------------------------*/

#include "deRandom.hpp"
#include "deUniquePtr.hpp"

#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDynamicRenderingTests.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"

#include <iostream>

namespace vkt
{
namespace renderpass
{
namespace
{

using namespace vk;
using namespace Draw;

using de::MovePtr;
using de::UniquePtr;
using de::SharedPtr;

using tcu::UVec2;
using tcu::Vec4;
using tcu::IVec4;
using tcu::UVec4;

constexpr auto		VK_NULL_HANDLE				= DE_NULL;
 // maxColorAttachments is guaranteed to be at least 4.
constexpr deUint32	COLOR_ATTACHMENTS_NUMBER	= 4;

constexpr deUint32	TEST_ATTACHMENT_LOAD_OP_LAST = 3;

enum TestType
{
	// Draw two triangles in a single primary command buffer, beginning and ending the render pass instance.
	TEST_TYPE_SINGLE_CMDBUF = 0,
	// Draw two triangles in a single primary command buffer, but across two render pass instances, with the second RESUMING the first.
	TEST_TYPE_SINGLE_CMDBUF_RESUMING,
	// Draw two triangles in two primary command buffers, across two render pass instances, with the second RESUMING the first.
	TEST_TYPE_TWO_CMDBUF_RESUMING,
	// Draw two triangles in two secondary command buffers, across two render pass instances,
	//with the second RESUMING the first, both recorded to the same primary command buffer.
	TEST_TYPE_SECONDARY_CMDBUF_RESUMING,
	// Draw two triangles in two secondary command buffers, across two render pass instances,
	// with the second RESUMING the first, executed in the two primary command buffers.
	TEST_TYPE_SECONDARY_CMDBUF_TWO_PRIMARY_RESUMING,
	// Using CONTENTS_SECONDARY_COMMAND_BUFFER_BIT_KHR, draw two triangles in one secondary command buffer,
	// and execute it inside a single render pass instance in one primary command buffer.
	TEST_TYPE_CONTENTS_SECONDARY_COMMAND_BUFFER,
	// Using CONTENTS_SECONDARY_COMMAND_BUFFER_BIT_KHR, draw two triangles in two secondary command buffers,
	// and execute them inside a single render pass instance in one primary command buffer.
	TEST_TYPE_CONTENTS_2_SECONDARY_COMMAND_BUFFER,
	// Using CONTENTS_SECONDARY_COMMAND_BUFFER_BIT_KHR, draw two triangles in two secondary command buffers,
	// and execute them inside two render pass instances, with the second RESUMING the first, both recorded in the same primary command buffer.
	TEST_TYPE_CONTENTS_2_SECONDARY_COMMAND_BUFFER_RESUMING,
	// Using CONTENTS_SECONDARY_COMMAND_BUFFER_BIT_KHR, draw two triangles in two secondary command buffers,
	// and execute them inside two render pass instances, with the second RESUMING the first, recorded into two primary command buffers.
	TEST_TYPE_CONTENTS_2_SECONDARY_2_PRIMARY_COMDBUF_RESUMING,
	// In one primary command buffer, record two render pass instances, with the second resuming the first.In the first,
	// draw one triangle directly in the primary command buffer.For the second, use CONTENTS_SECONDARY_COMMAND_BUFFER_BIT_KHR,
	// draw the second triangle in a secondary command buffer, and execute it in that second render pass instance.
	TEST_TYPE_CONTENTS_PRIMARY_SECONDARY_COMDBUF_RESUMING,
	// In one primary command buffer, record two render pass instances, with the second resuming the first.In the first,
	// use CONTENTS_SECONDARY_COMMAND_BUFFER_BIT_KHR, draw the first triangle in a secondary command buffer,
	// and execute it in that first render pass instance.In the second, draw one triangle directly in the primary command buffer.
	TEST_TYPE_CONTENTS_SECONDARY_PRIMARY_COMDBUF_RESUMING,
	// In two primary command buffers, record two render pass instances(one in each), with the second resuming the first.In the first,
	// draw one triangle directly in the primary command buffer.For the second, use CONTENTS_SECONDARY_COMMAND_BUFFER_BIT_KHR,
	// draw the second triangle in a secondary command buffer, and execute it in that second render pass instance.
	TEST_TYPE_CONTENTS_2_PRIMARY_SECONDARY_COMDBUF_RESUMING,
	// In two primary command buffers, record two render pass instances(one in each), with the second resuming the first.In the first,
	// use CONTENTS_SECONDARY_COMMAND_BUFFER_BIT_KHR, draw the first triangle in a secondary command buffer, and execute it in that first
	// render pass instance.In the second, draw one triangle directly in the primary command buffer.
	TEST_TYPE_CONTENTS_SECONDARY_2_PRIMARY_COMDBUF_RESUMING,
	TEST_TYPE_LAST
};

enum TestAttachmentType
{
	TEST_ATTACHMENT_SINGLE_COLOR = 0,
	TEST_ATTACHMENT_DEPTH_ATTACHMENT,
	TEST_ATTACHMENT_STENCIL_ATTACHMENT,
	TEST_ATTACHMENT_MULTIPLE_COLOR,
	TEST_ATTACHMENT_ALL,
	TEST_ATTACHMENT_LAST
};

enum TestAttachmentStoreOp
{
	TEST_ATTACHMENT_STORE_OP_STORE     = VK_ATTACHMENT_STORE_OP_STORE,
	TEST_ATTACHMENT_STORE_OP_DONT_CARE =VK_ATTACHMENT_STORE_OP_DONT_CARE,
	TEST_ATTACHMENT_STORE_OP_LAST
};

struct TestParameters
{
	TestType		testType;
	const Vec4		clearColor;
	const VkFormat	imageFormat;
	const UVec2		renderSize;
};

struct ImagesLayout
{
	VkImageLayout	oldColors[COLOR_ATTACHMENTS_NUMBER];
	VkImageLayout	oldStencil;
	VkImageLayout	oldDepth;
};

struct ImagesFormat
{
	VkFormat	colors[COLOR_ATTACHMENTS_NUMBER];
	VkFormat	depth;
	VkFormat	stencil;
};

struct ClearAttachmentData
{
	std::vector<VkClearAttachment>	colorDepthClear1;
	std::vector<VkClearAttachment>	colorDepthClear2;
	VkClearAttachment				stencilClear1;
	VkClearAttachment				stencilClear2;
	VkClearRect						rectColorDepth1;
	VkClearRect						rectColorDepth2;
	VkClearRect						rectStencil1;
	VkClearRect						rectStencil2;

	ClearAttachmentData	(const deUint32	colorAtchCount,
						const VkFormat	depth,
						const VkFormat	stencil)
	{
		if (colorAtchCount != 0)
		{
			for (deUint32 atchNdx = 0; atchNdx < colorAtchCount; ++atchNdx)
			{
				const VkClearAttachment green =
				{
					VK_IMAGE_ASPECT_COLOR_BIT,
					atchNdx,
					makeClearValueColorF32(0.0f, 1.0f, static_cast<float>(atchNdx) * 0.15f, 1.0f)
				};
				colorDepthClear1.push_back(green);

				const VkClearAttachment yellow =
				{
					VK_IMAGE_ASPECT_COLOR_BIT,
					atchNdx,
					makeClearValueColorF32(1.0f, 1.0f, static_cast<float>(atchNdx) * 0.15f, 1.0f)
				};
				colorDepthClear2.push_back(yellow);
			}
		}

		if (depth != VK_FORMAT_UNDEFINED)
		{
			const VkClearAttachment zero =
			{
				VK_IMAGE_ASPECT_DEPTH_BIT,
				0,
				makeClearValueDepthStencil(0.0f, 0)
			};
			colorDepthClear1.push_back(zero);

			const VkClearAttachment one =
			{
				VK_IMAGE_ASPECT_DEPTH_BIT,
				0,
				makeClearValueDepthStencil(0.2f, 0)
			};
			colorDepthClear2.push_back(one);
		}

		if (stencil != VK_FORMAT_UNDEFINED)
		{
			stencilClear1 =
			{
				VK_IMAGE_ASPECT_STENCIL_BIT,
				0,
				makeClearValueDepthStencil(0.0f, 1)
			};

			stencilClear2 =
			{
				VK_IMAGE_ASPECT_STENCIL_BIT,
				0,
				makeClearValueDepthStencil(0.0f, 2)
			};

			rectStencil1 =
			{
				makeRect2D(0, 0, 32, 16),
				0u,
				1u,
			};

			rectStencil2 =
			{
				makeRect2D(0, 16, 32, 16),
				0u,
				1u,
			};
		}

		rectColorDepth1 =
		{
			makeRect2D(0, 0, 16, 32),
			0u,
			1u,
		};

		rectColorDepth2 =
		{
			makeRect2D(16, 0, 16, 32),
			0u,
			1u,
		};
	}
};

template<typename T>
inline VkDeviceSize sizeInBytes (const std::vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

VkImageCreateInfo makeImageCreateInfo (const VkFormat		format,
									   const UVec2&			size,
									   VkImageUsageFlags	usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),	// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};
	return imageParams;
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&	vk,
									   const VkDevice			device,
									   const VkPipelineLayout	pipelineLayout,
									   const VkShaderModule		vertexModule,
									   const VkShaderModule		fragmentModule,
									   const UVec2				renderSize,
									   const deUint32			colorAttachmentCount,
									   const VkFormat*			pColorAttachmentFormats,
									   const VkFormat			depthStencilAttachmentFormat)
{
	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,								// uint32_t				binding;
		sizeof(Vec4),					// uint32_t				stride;
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,								// uint32_t			location;
			0u,								// uint32_t			binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat			format;
			0u,								// uint32_t			offset;
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags		flags;
		1u,																// uint32_t										vertexBindingDescriptionCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),			// uint32_t										vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,								// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags		flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology							topology;
		VK_FALSE,														// VkBool32										primitiveRestartEnable;
	};

	VkViewport		viewport = makeViewport(0.0f, 0.0f, static_cast<float>(renderSize.x()), static_cast<float>(renderSize.y()), 0.0f, 1.0f);
	const VkRect2D	rectScissorRenderSize = { { 0, 0 }, { renderSize.x(), renderSize.y() } };

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,												// const void*									pNext;
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags			flags;
		1u,														// uint32_t										viewportCount;
		&viewport,												// const VkViewport*							pViewports;
		1u,														// uint32_t										scissorCount;
		&rectScissorRenderSize,									// const VkRect2D*								pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		(VkPipelineRasterizationStateCreateFlags)0,					// VkPipelineRasterizationStateCreateFlags		flags;
		VK_FALSE,													// VkBool32										depthClampEnable;
		VK_FALSE,													// VkBool32										rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode								polygonMode;
		VK_CULL_MODE_NONE,											// VkCullModeFlags								cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace									frontFace;
		VK_FALSE,													// VkBool32										depthBiasEnable;
		0.0f,														// float										depthBiasConstantFactor;
		0.0f,														// float										depthBiasClamp;
		0.0f,														// float										depthBiasSlopeFactor;
		1.0f,														// float										lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags		flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits						rasterizationSamples;
		VK_FALSE,													// VkBool32										sampleShadingEnable;
		0.0f,														// float										minSampleShading;
		DE_NULL,													// const VkSampleMask*							pSampleMask;
		VK_FALSE,													// VkBool32										alphaToCoverageEnable;
		VK_FALSE													// VkBool32										alphaToOneEnable;
	};

	const VkStencilOpState stencilOp = makeStencilOpState(
		VK_STENCIL_OP_ZERO,					// stencil fail
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,	// depth & stencil pass
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,	// depth only fail
		VK_COMPARE_OP_NOT_EQUAL,			// compare op
		240u,								// compare mask
		255u,								// write mask
		255u);								// reference

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,						// VkPipelineDepthStencilStateCreateFlags		flags;
		VK_TRUE,														// VkBool32										depthTestEnable;
		VK_TRUE,														// VkBool32										depthWriteEnable;
		VK_COMPARE_OP_ALWAYS,											// VkCompareOp									depthCompareOp;
		VK_FALSE,														// VkBool32										depthBoundsTestEnable;
		VK_TRUE,														// VkBool32										stencilTestEnable;
		stencilOp,														// VkStencilOpState								front;
		stencilOp,														// VkStencilOpState								back;
		0.0f,															// float										minDepthBounds;
		1.0f,															// float										maxDepthBounds;
	};

	const VkColorComponentFlags					colorComponentsAll = VK_COLOR_COMPONENT_R_BIT |
																	 VK_COLOR_COMPONENT_G_BIT |
																	 VK_COLOR_COMPONENT_B_BIT |
																	 VK_COLOR_COMPONENT_A_BIT;

	std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentState;

	for (deUint32 ndx = 0 ; ndx < colorAttachmentCount; ++ndx)
	{
		const VkPipelineColorBlendAttachmentState	pipelineColorBlendAttachmentState =
		{
			VK_FALSE,				// VkBool32					blendEnable;
			VK_BLEND_FACTOR_ONE,	// VkBlendFactor			srcColorBlendFactor;
			VK_BLEND_FACTOR_ZERO,	// VkBlendFactor			dstColorBlendFactor;
			VK_BLEND_OP_ADD,		// VkBlendOp				colorBlendOp;
			VK_BLEND_FACTOR_ONE,	// VkBlendFactor			srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ZERO,	// VkBlendFactor			dstAlphaBlendFactor;
			VK_BLEND_OP_ADD,		// VkBlendOp				alphaBlendOp;
			colorComponentsAll,		// VkColorComponentFlags	colorWriteMask;
		};

		colorBlendAttachmentState.push_back(pipelineColorBlendAttachmentState);
	}

	const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		colorAttachmentCount,										// deUint32										attachmentCount;
		colorBlendAttachmentState.data(),							// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConstants[4];
	};

	const VkPipelineShaderStageCreateInfo pShaderStages[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,												// const void*									pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags				flags;
			VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits						stage;
			vertexModule,											// VkShaderModule								module;
			"main",													// const char*									pName;
			DE_NULL,												// const VkSpecializationInfo*					pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,												// const void*									pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags				flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits						stage;
			fragmentModule,											// VkShaderModule								module;
			"main",													// const char*									pName;
			DE_NULL,												// const VkSpecializationInfo*					pSpecializationInfo;
		},
	};

	const VkPipelineRenderingCreateInfoKHR renderingCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,			// VkStructureType	sType;
		DE_NULL,														// const void*		pNext;
		0u,																// deUint32			viewMask;
		colorAttachmentCount,											// deUint32			colorAttachmentCount;
		pColorAttachmentFormats,										// const VkFormat*	pColorAttachmentFormats;
		depthStencilAttachmentFormat,									// VkFormat			depthAttachmentFormat;
		depthStencilAttachmentFormat,									// VkFormat			stencilAttachmentFormat;
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
		&renderingCreateInfo,								// const void*										pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
		2u,													// deUint32											stageCount;
		pShaderStages,										// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,								// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,					// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&pipelineViewportStateInfo,							// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,					// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&pipelineMultisampleStateInfo,						// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&pipelineDepthStencilStateInfo,						// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&pipelineColorBlendStateInfo,						// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,										// VkPipelineLayout									layout;
		VK_NULL_HANDLE,										// VkRenderPass										renderPass;
		0u,													// deUint32											subpass;
		DE_NULL,											// VkPipeline										basePipelineHandle;
		0,													// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

VkFormat getSupportedStencilFormat (const InstanceInterface&	vki,
									VkPhysicalDevice			physDev)
{
	const VkFormat				formatList[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
	const VkFormatFeatureFlags	requirements = (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
												VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(formatList); ++i)
	{
		const auto properties = getPhysicalDeviceFormatProperties(vki, physDev, formatList[i]);
		if ((properties.optimalTilingFeatures & requirements) == requirements)
			return formatList[i];
	}

	return VK_FORMAT_UNDEFINED;
}

tcu::TextureFormat getDepthTextureFormat (const VkFormat	depthStencilFormat)
{
	return	((depthStencilFormat == VK_FORMAT_D24_UNORM_S8_UINT) ?
			tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNSIGNED_INT_24_8_REV) :
			tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT));
}

tcu::TextureLevel generateColroImage (const tcu::TextureFormat	format,
									  const UVec2&				renderSize,
									  const int					attachmentNdx)
{
	tcu::TextureLevel	image		(format, renderSize.x(), renderSize.y());
	const float			atchNdx		= static_cast<float>(attachmentNdx);
	const Vec4			greenColor	= Vec4(0.0f, 1.0f, atchNdx * 0.15f, 1.0f);
	const Vec4			yellowColor	= Vec4(1.0f, 1.0f, atchNdx * 0.15f, 1.0f);

	for (deUint32 y = 0; y < renderSize.y(); ++y)
	{
		for (deUint32 x = 0; x < renderSize.x() / 2u; ++x)
		{
			image.getAccess().setPixel(greenColor, x, y);
		}
		for (deUint32 x = renderSize.x() / 2u; x < renderSize.x(); ++x)
		{
			image.getAccess().setPixel(yellowColor, x, y);
		}
	}

	return image;
}

tcu::TextureLevel generateDepthImage (const tcu::TextureFormat	format,
									  const UVec2&				renderSize)
{
	tcu::TextureLevel	image	(format, renderSize.x(), renderSize.y());
	const float			value1	= 0.0f;
	const float			value2	= 0.2f;

	for (deUint32 y = 0; y < renderSize.y(); ++y)
	{
		for (deUint32 x = 0; x < renderSize.x() / 2u; ++x)
		{
			image.getAccess().setPixDepth(value1, x, y);
		}
		for (deUint32 x = renderSize.x() / 2u; x < renderSize.x(); ++x)
		{
			image.getAccess().setPixDepth(value2, x, y);
		}
	}

	return image;
}

tcu::TextureLevel generateStencilImage (const tcu::TextureFormat	format,
										const UVec2&				renderSize)
{
	tcu::TextureLevel	image	(format, renderSize.x(), renderSize.y());
	const IVec4			value1	= IVec4(1,0,0,0);
	const IVec4			value2	= IVec4(2,0,0,0);

	for (deUint32 x = 0; x < renderSize.x(); ++x)
	{
		for (deUint32 y = 0; y < renderSize.y() / 2u; ++y)
		{
			image.getAccess().setPixel(value1, x, y);
		}
		for (deUint32 y = renderSize.y() / 2u; y < renderSize.y(); ++y)
		{
			image.getAccess().setPixel(value2, x, y);
		}
	}

	return image;
}

void submitCommandsAndWait (const DeviceInterface&	vk,
							const VkDevice			device,
							const VkQueue			queue,
							const VkCommandBuffer	commandBuffer,
							const VkCommandBuffer	commandBuffer2)
{
	const Unique<VkFence>	fence(createFence(vk, device));
	const VkCommandBuffer	cmdBuffers[2] = { commandBuffer, commandBuffer2 };

	const VkSubmitInfo		submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		DE_NULL,						// const void*					pNext;
		0u,								// deUint32						waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*			pWaitSemaphores;
		DE_NULL,						// const VkPipelineStageFlags*	pWaitDstStageMask;
		2u,								// deUint32						commandBufferCount;
		cmdBuffers,						// const VkCommandBuffer*		pCommandBuffers;
		0u,								// deUint32						signalSemaphoreCount;
		nullptr,						// const VkSemaphore*			pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
}

void beginSecondaryCmdBuffer (const DeviceInterface&	vk,
							  const VkCommandBuffer		commandBuffer,
							  VkRenderingFlagsKHR		renderingFlags,
							  const deUint32			colorAttachmentCount,
							  const ImagesFormat&		imagesFormat)
{
	const VkFormat depthStencilFormat = (imagesFormat.depth != VK_FORMAT_UNDEFINED) ?
		imagesFormat.depth : imagesFormat.stencil;
	const VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,	// VkStructureType			sType;
		DE_NULL,															// const void*				pNext;
		renderingFlags,														// VkRenderingFlagsKHR		flags;
		0u,																	// uint32_t					viewMask;
		colorAttachmentCount,												// uint32_t					colorAttachmentCount;
		(colorAttachmentCount > 0) ? imagesFormat.colors : DE_NULL,			// const VkFormat*			pColorAttachmentFormats;
		depthStencilFormat,													// VkFormat					depthAttachmentFormat;
		depthStencilFormat,													// VkFormat					stencilAttachmentFormat;
		VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits	rasterizationSamples;
	};

	const VkCommandBufferInheritanceInfo	bufferInheritanceInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	// VkStructureType					sType;
		&inheritanceRenderingInfo,								// const void*						pNext;
		VK_NULL_HANDLE,											// VkRenderPass						renderPass;
		0u,														// deUint32							subpass;
		VK_NULL_HANDLE,											// VkFramebuffer					framebuffer;
		VK_FALSE,												// VkBool32							occlusionQueryEnable;
		(vk::VkQueryControlFlags)0u,							// VkQueryControlFlags				queryFlags;
		(vk::VkQueryPipelineStatisticFlags)0u					// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,		// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,	// VkCommandBufferUsageFlags	flags;
		&bufferInheritanceInfo
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &commandBufBeginParams));
}

void beginSecondaryCmdBuffer(const DeviceInterface&	vk,
							const VkCommandBuffer	commandBuffer)
{
	const VkCommandBufferInheritanceInfo	bufferInheritanceInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_NULL_HANDLE,											// VkRenderPass						renderPass;
		0u,														// deUint32							subpass;
		VK_NULL_HANDLE,											// VkFramebuffer					framebuffer;
		VK_FALSE,												// VkBool32							occlusionQueryEnable;
		(vk::VkQueryControlFlags)0u,							// VkQueryControlFlags				queryFlags;
		(vk::VkQueryPipelineStatisticFlags)0u					// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,		// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,		// VkCommandBufferUsageFlags	flags;
		&bufferInheritanceInfo
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &commandBufBeginParams));
}

class DynamicRenderingTestInstance : public TestInstance
{
public:
									DynamicRenderingTestInstance	(Context&							context,
																	 const TestParameters&				parameters);
protected:
	virtual tcu::TestStatus			iterate							(void);
	void							initialize						(void);
	void							createCmdBuffer					(void);
	virtual void					rendering						(const VkPipeline					pipeline,
																	 const std::vector<VkImageView>&	attachmentBindInfos,
																	 const deUint32						colorAtchCount,
																	 ImagesLayout&						imagesLayout,
																	 const ImagesFormat&				imagesFormat);
	void							preBarier						(const deUint32						colorAtchCount,
																	 ImagesLayout&						imagesLayout,
																	 const ImagesFormat&				imagesFormat);
	void							beginRendering					(VkCommandBuffer					cmdBuffer,
																	 const std::vector<VkImageView>&	attachmentBindInfos,
																	 const VkRenderingFlagsKHR			flags,
																	 const deUint32						colorAtchCount,
																	 const ImagesFormat&				imagesFormat,
																	 const VkAttachmentLoadOp			loadOp,
																	 const VkAttachmentStoreOp			storeOp);
	void							copyImgToBuff					(VkCommandBuffer					commandBuffer,
																	const deUint32						colorAtchCount,
																	 ImagesLayout&						imagesLayout,
																	 const ImagesFormat&				imagesFormat);
	void							verifyResults					(const deUint32						colorAtchCount,
																	 const ImagesFormat&				imagesFormat);

	const TestParameters			m_parameters;
	VkFormat						m_formatStencilDepthImage;
	Move<VkImage>					m_imageColor[COLOR_ATTACHMENTS_NUMBER];
	Move<VkImage>					m_imageStencilDepth;
	Move<VkImageView>				m_colorAttachmentView[COLOR_ATTACHMENTS_NUMBER];
	Move<VkImageView>				m_stencilDepthAttachmentView;
	MovePtr<Allocation>				m_imageColorAlloc[COLOR_ATTACHMENTS_NUMBER];
	MovePtr<Allocation>				m_imageStencilDepthAlloc;
	SharedPtr<Buffer>				m_imageBuffer[COLOR_ATTACHMENTS_NUMBER];
	SharedPtr<Buffer>				m_imageDepthBuffer;
	SharedPtr<Buffer>				m_imageStencilBuffer;

	Move<VkShaderModule>			m_vertexModule;
	Move<VkShaderModule>			m_fragmentModule;
	SharedPtr<Buffer>				m_vertexBuffer;
	Move<VkPipelineLayout>			m_pipelineLayout;

	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;

	std::vector<tcu::TextureLevel>	m_referenceImages;
};

DynamicRenderingTestInstance::DynamicRenderingTestInstance (Context&				context,
															const TestParameters&	parameters)
	: TestInstance	(context)
	, m_parameters	(parameters)
{
	const VkPhysicalDeviceDynamicRenderingFeaturesKHR&		dynamicRenderingFeatures	(context.getDynamicRenderingFeatures());

	if (dynamicRenderingFeatures.dynamicRendering == DE_FALSE)
		TCU_FAIL("dynamicRendering is not supported");

	initialize();
	createCmdBuffer();
}

tcu::TestStatus DynamicRenderingTestInstance::iterate (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	ImagesLayout imagesLayout
	{
		{
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_UNDEFINED
		},								// oldColors
		VK_IMAGE_LAYOUT_UNDEFINED,		// oldLStencil
		VK_IMAGE_LAYOUT_UNDEFINED,		// oldDepth
	};

	for (int attachmentTest = 0; attachmentTest < TEST_ATTACHMENT_LAST; ++attachmentTest)
	{
		std::vector<VkImageView>	attachmentBindInfos;

		ImagesFormat imagesFormat
		{
			{
				m_parameters.imageFormat,
				m_parameters.imageFormat,
				m_parameters.imageFormat,
				m_parameters.imageFormat,
			},								// colors
			m_formatStencilDepthImage,		// depth
			m_formatStencilDepthImage,		// stencil
		};

		deUint32	colorAtchCount		= 0u;

		switch(attachmentTest)
		{
			case TEST_ATTACHMENT_SINGLE_COLOR:
			{
				attachmentBindInfos.push_back(*m_colorAttachmentView[0]);
				imagesFormat.depth		= VK_FORMAT_UNDEFINED;
				imagesFormat.stencil	= VK_FORMAT_UNDEFINED;
				colorAtchCount			= 1u;
				break;
			}
			case TEST_ATTACHMENT_DEPTH_ATTACHMENT:
			{
				attachmentBindInfos.push_back(*m_stencilDepthAttachmentView);
				imagesFormat.colors[0]	= VK_FORMAT_UNDEFINED;
				imagesFormat.stencil	= VK_FORMAT_UNDEFINED;
				break;
			}
			case TEST_ATTACHMENT_STENCIL_ATTACHMENT:
			{
				attachmentBindInfos.push_back(*m_stencilDepthAttachmentView);
				imagesFormat.colors[0]	= VK_FORMAT_UNDEFINED;
				imagesFormat.depth		= VK_FORMAT_UNDEFINED;
				break;
			}
			case TEST_ATTACHMENT_MULTIPLE_COLOR:
			{
				for(deUint32 ndx = 0; ndx < COLOR_ATTACHMENTS_NUMBER; ndx++)
					attachmentBindInfos.push_back(*m_colorAttachmentView[ndx]);

				colorAtchCount			= COLOR_ATTACHMENTS_NUMBER;
				imagesFormat.depth		= VK_FORMAT_UNDEFINED;
				imagesFormat.stencil	= VK_FORMAT_UNDEFINED;
				break;
			}
			case TEST_ATTACHMENT_ALL:
			{
				for (deUint32 ndx = 0; ndx < COLOR_ATTACHMENTS_NUMBER; ndx++)
					attachmentBindInfos.push_back(*m_colorAttachmentView[ndx]);

				attachmentBindInfos.push_back(*m_stencilDepthAttachmentView);
				attachmentBindInfos.push_back(*m_stencilDepthAttachmentView);

				colorAtchCount = COLOR_ATTACHMENTS_NUMBER;
				break;
			}
			default:
				DE_FATAL("Impossible");
		};
		Move<VkPipeline>	pipeline	= makeGraphicsPipeline(vk, device, *m_pipelineLayout,
															  *m_vertexModule, *m_fragmentModule,
															   m_parameters.renderSize, colorAtchCount, imagesFormat.colors,
															   ((imagesFormat.depth == VK_FORMAT_UNDEFINED) ? imagesFormat.stencil : imagesFormat.depth));

		rendering(*pipeline, attachmentBindInfos, colorAtchCount, imagesLayout, imagesFormat);
	}
	return tcu::TestStatus::pass("Pass");
}

void DynamicRenderingTestInstance::initialize (void)
{
	const InstanceInterface&	vki			= m_context.getInstanceInterface();
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkPhysicalDevice		physDevice	= m_context.getPhysicalDevice();
	const VkDevice				device		= m_context.getDevice();
	Allocator&					allocator	= m_context.getDefaultAllocator();

	// Vertices.
	{
		std::vector<Vec4>	vertices;

		// Draw a quad covering the whole renderarea
		vertices.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
		vertices.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
		vertices.push_back(Vec4( 0.0f,  1.0f, 0.0f, 1.0f));
		vertices.push_back(Vec4( 0.0f, -1.0f, 0.0f, 1.0f));

		vertices.push_back(Vec4(1.0f, -1.0f, 0.2f, 1.0f));
		vertices.push_back(Vec4(0.0f, -1.0f, 0.2f, 1.0f));
		vertices.push_back(Vec4(1.0f,  1.0f, 0.2f, 1.0f));
		vertices.push_back(Vec4(0.0f,  1.0f, 0.2f, 1.0f));

		vertices.push_back(Vec4(-1.0f, 1.0f, 0.0f, 1.0f));
		vertices.push_back(Vec4(-1.0f, 0.0f, 0.0f, 1.0f));
		vertices.push_back(Vec4( 1.0f, 1.0f, 0.0f, 1.0f));
		vertices.push_back(Vec4( 1.0f, 0.0f, 0.0f, 1.0f));

		const VkDeviceSize			bufferSize	= sizeInBytes(vertices);
		const VkBufferCreateInfo	bufferInfo	= makeBufferCreateInfo(bufferSize,
																	   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		m_vertexBuffer	= Buffer::createAndAlloc(vk, device,
												 bufferInfo,
												 allocator,
												 MemoryRequirement::HostVisible);
		deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), vertices.data(), static_cast<std::size_t>(bufferSize));
		flushAlloc(vk, device, m_vertexBuffer->getBoundMemory());
	}

	// Images color attachment.
	{
		const VkImageUsageFlags			imageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
															  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		const VkDeviceSize				imageBufferSize		= m_parameters.renderSize.x() *
															  m_parameters.renderSize.y() *
															  tcu::getPixelSize(mapVkFormat(m_parameters.imageFormat));
		const VkImageSubresourceRange	imageSubresource	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const VkImageCreateInfo			imageInfo			= makeImageCreateInfo(m_parameters.imageFormat,
																				  m_parameters.renderSize, imageUsage);
		const VkBufferCreateInfo		bufferInfo			= makeBufferCreateInfo(imageBufferSize,
																				   VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		for(deUint32 ndx = 0; ndx < COLOR_ATTACHMENTS_NUMBER; ++ndx)
		{
			m_imageColor[ndx]			= makeImage(vk, device, imageInfo);
			m_imageColorAlloc[ndx]		= bindImage(vk, device, allocator, *m_imageColor[ndx], MemoryRequirement::Any);
			m_imageBuffer[ndx]			= Buffer::createAndAlloc(vk, device, bufferInfo,
																 allocator, MemoryRequirement::HostVisible);
			m_colorAttachmentView[ndx]	= makeImageView(vk, device, *m_imageColor[ndx],
														VK_IMAGE_VIEW_TYPE_2D, m_parameters.imageFormat, imageSubresource);

			const Allocation alloc = m_imageBuffer[ndx]->getBoundMemory();
			deMemset(alloc.getHostPtr(), 0, static_cast<std::size_t>(imageBufferSize));
			flushAlloc(vk, device, alloc);
		}
	}

	//Image stencil and depth attachment.
	{
		m_formatStencilDepthImage = getSupportedStencilFormat(vki, physDevice);

		const VkImageAspectFlags		imageDepthStencilAspec	= VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
		const VkImageSubresourceRange	imageStencilSubresource	= makeImageSubresourceRange(imageDepthStencilAspec, 0u, 1u, 0u, 1u);
		const VkDeviceSize				imageBufferStencilSize	= m_parameters.renderSize.x() *
																  m_parameters.renderSize.y() *
																  tcu::getPixelSize(mapVkFormat(VK_FORMAT_S8_UINT));
		const VkDeviceSize				imageBufferDepthlSize	= m_parameters.renderSize.x() *
																  m_parameters.renderSize.y() *
																  tcu::getPixelSize(getDepthTextureFormat(m_formatStencilDepthImage));

		const VkImageUsageFlags			imageStenciDepthlUsage	= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
																  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		const VkImageCreateInfo			imageInfo				= makeImageCreateInfo(m_formatStencilDepthImage,
																					  m_parameters.renderSize, imageStenciDepthlUsage);
		const VkBufferCreateInfo		bufferStencilInfo		= makeBufferCreateInfo(imageBufferStencilSize,
																					   VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const VkBufferCreateInfo		bufferDepthlInfo		= makeBufferCreateInfo(imageBufferDepthlSize,
																					   VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		m_imageStencilDepth				= makeImage(vk, device, imageInfo);
		m_imageStencilDepthAlloc		= bindImage(vk, device, allocator, *m_imageStencilDepth, MemoryRequirement::Any);

		m_imageStencilBuffer			= Buffer::createAndAlloc(vk, device, bufferStencilInfo,
																 allocator, MemoryRequirement::HostVisible);
		m_imageDepthBuffer				= Buffer::createAndAlloc(vk, device, bufferDepthlInfo,
																 allocator, MemoryRequirement::HostVisible);
		m_stencilDepthAttachmentView	= makeImageView(vk, device, *m_imageStencilDepth,
														VK_IMAGE_VIEW_TYPE_2D, m_formatStencilDepthImage, imageStencilSubresource);

		const Allocation alloc = m_imageStencilBuffer->getBoundMemory();
		deMemset(alloc.getHostPtr(), 0, static_cast<std::size_t>(imageBufferStencilSize));
		flushAlloc(vk, device, alloc);

		const Allocation allocDepth = m_imageDepthBuffer->getBoundMemory();
		deMemset(allocDepth.getHostPtr(), 0, static_cast<std::size_t>(imageBufferDepthlSize));
		flushAlloc(vk, device, allocDepth);
	}

	m_pipelineLayout		= makePipelineLayout(vk, device);
	m_vertexModule			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	m_fragmentModule		= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);

	for (deUint32 ndx = 0; ndx < COLOR_ATTACHMENTS_NUMBER; ++ndx)
	{
		m_referenceImages.push_back(generateColroImage(mapVkFormat(m_parameters.imageFormat),
			m_parameters.renderSize, ndx));
	}

	m_referenceImages.push_back(generateDepthImage(getDepthTextureFormat(m_formatStencilDepthImage),
		m_parameters.renderSize));

	m_referenceImages.push_back(generateStencilImage(mapVkFormat(VK_FORMAT_S8_UINT),
		m_parameters.renderSize));
}

void DynamicRenderingTestInstance::createCmdBuffer (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_cmdPool	= createCommandPool(vk, device,
									VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
									m_context.getUniversalQueueFamilyIndex());
	m_cmdBuffer	= allocateCommandBuffer(vk, device, *m_cmdPool,
										VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

void DynamicRenderingTestInstance::rendering (const VkPipeline					pipeline,
											  const std::vector<VkImageView>&	attachmentBindInfos,
											  const deUint32					colorAtchCount,
											  ImagesLayout&						imagesLayout,
											  const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{

		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   0,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		{
			const VkBuffer vertexBuffer = m_vertexBuffer->object();
			const VkDeviceSize vertexBufferOffset = 0ull;
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 8u, 0u);
		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 0u);
		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 4u, 0u);

		vk.cmdEndRendering(*m_cmdBuffer);

		copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   0,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear1.size()),
										clearData.colorDepthClear1.data(),
										1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer, 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer, 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(*m_cmdBuffer);

			copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

void DynamicRenderingTestInstance::preBarier (const deUint32		colorAtchCount,
											  ImagesLayout&			imagesLayout,
											  const ImagesFormat&	imagesFormat)
{
	const DeviceInterface&	vk = m_context.getDeviceInterface();

	std::vector<VkImageMemoryBarrier> barriers;

	for (deUint32 ndx = 0; ndx < colorAtchCount; ++ndx)
	{
		const VkImageSubresourceRange	subresource = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const VkImageMemoryBarrier		barrier = makeImageMemoryBarrier(VK_ACCESS_NONE_KHR,
																		 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																		 imagesLayout.oldColors[ndx],
																		 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																		 *m_imageColor[ndx],
																		 subresource);
		barriers.push_back(barrier);
		imagesLayout.oldColors[ndx] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	if (imagesFormat.depth != VK_FORMAT_UNDEFINED)
	{
		const VkImageSubresourceRange	subresource = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
		const VkImageMemoryBarrier		barrier = makeImageMemoryBarrier(VK_ACCESS_NONE_KHR,
																		 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																		 imagesLayout.oldDepth,
																		 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																		 *m_imageStencilDepth,
																		 subresource);
		imagesLayout.oldDepth = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		barriers.push_back(barrier);
	}

	if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
	{
		const VkImageSubresourceRange	subresource = makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);
		const VkImageMemoryBarrier		barrier = makeImageMemoryBarrier(VK_ACCESS_NONE_KHR,
																		 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																		 imagesLayout.oldStencil,
																		 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																		 *m_imageStencilDepth,
																		 subresource);
		imagesLayout.oldStencil = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		barriers.push_back(barrier);
	}

	cmdPipelineImageMemoryBarrier(vk,
								  *m_cmdBuffer,
								  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
								  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								  barriers.data(),
								  barriers.size());
}

void DynamicRenderingTestInstance::beginRendering (VkCommandBuffer					cmdBuffer,
												   const std::vector<VkImageView>&	attachmentBindInfos,
												   const VkRenderingFlagsKHR			flags,
												   const deUint32					colorAtchCount,
												   const ImagesFormat&				imagesFormat,
												   const VkAttachmentLoadOp			loadOp,
												   const VkAttachmentStoreOp			storeOp)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkClearValue		clearValue	= makeClearValueColor(m_parameters.clearColor);
	const VkRect2D			renderArea	=
	{
		makeOffset2D(0, 0),
		makeExtent2D(m_parameters.renderSize.x(), m_parameters.renderSize.y()),
	};

	std::vector<VkRenderingAttachmentInfoKHR>	attachments;

	for (deUint32 ndx = 0; ndx < colorAtchCount; ++ndx)
	{
		const VkRenderingAttachmentInfoKHR renderingAtachInfo =
		{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			attachmentBindInfos[ndx],							// VkImageView				imageView;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout			imageLayout;
			VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits	resolveMode;
			VK_NULL_HANDLE,										// VkImageView				resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			resolveImageLayout;
			loadOp,												// VkAttachmentLoadOp		loadOp;
			storeOp,											// VkAttachmentStoreOp		storeOp;
			clearValue,											// VkClearValue				clearValue;
		};

		attachments.push_back(renderingAtachInfo);
	}

	if (imagesFormat.depth != VK_FORMAT_UNDEFINED)
	{
		const VkRenderingAttachmentInfoKHR renderingAtachInfo =
		{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			attachmentBindInfos[colorAtchCount],				// VkImageView				imageView;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			imageLayout;
			VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits	resolveMode;
			VK_NULL_HANDLE,										// VkImageView				resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			resolveImageLayout;
			loadOp,												// VkAttachmentLoadOp		loadOp;
			storeOp,											// VkAttachmentStoreOp		storeOp;
			clearValue,											// VkClearValue				clearValue;
		};

		attachments.push_back(renderingAtachInfo);
	}

	const deUint32 stencilNdx = colorAtchCount + ((imagesFormat.depth != VK_FORMAT_UNDEFINED) ? 1 : 0);

	if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
	{
		const VkRenderingAttachmentInfoKHR renderingAtachInfo =
		{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			attachmentBindInfos[stencilNdx],					// VkImageView				imageView;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			imageLayout;
			VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits	resolveMode;
			VK_NULL_HANDLE,										// VkImageView				resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			resolveImageLayout;
			loadOp,												// VkAttachmentLoadOp		loadOp;
			storeOp,											// VkAttachmentStoreOp		storeOp;
			clearValue,											// VkClearValue				clearValue;
		};

		attachments.push_back(renderingAtachInfo);
	}

	const VkRenderingInfoKHR renderingInfo =
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,					// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		flags,													// VkRenderingFlagsKHR					flags;
		renderArea,												// VkRect2D								renderArea;
		1u,														// deUint32								layerCount;
		0u,														// deUint32								viewMask;
		colorAtchCount,											// deUint32								colorAttachmentCount;
		((colorAtchCount != 0) ? attachments.data() : DE_NULL),	// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		((imagesFormat.depth != VK_FORMAT_UNDEFINED) ?
			&attachments[colorAtchCount] :
			DE_NULL),											// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		((imagesFormat.stencil != VK_FORMAT_UNDEFINED) ?
			&attachments[stencilNdx] :
			DE_NULL),											// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	vk.cmdBeginRendering(cmdBuffer, &renderingInfo);
}

void DynamicRenderingTestInstance::copyImgToBuff (VkCommandBuffer		commandBuffer,
												  const deUint32		colorAtchCount,
												  ImagesLayout&			imagesLayout,
												  const ImagesFormat&	imagesFormat)
{
	const DeviceInterface&	vk = m_context.getDeviceInterface();

	if (imagesFormat.colors[0] != VK_FORMAT_UNDEFINED)
	{
		for (deUint32 ndx = 0; ndx < colorAtchCount; ndx++)
		{
			vk::copyImageToBuffer(vk, commandBuffer, *m_imageColor[ndx], m_imageBuffer[ndx]->object(),
								  tcu::IVec2(m_parameters.renderSize.x(), m_parameters.renderSize.y()),
								  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, imagesLayout.oldColors[ndx], 1u,
								  VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			imagesLayout.oldColors[ndx] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}
	}
	if (imagesFormat.depth != VK_FORMAT_UNDEFINED)
	{
		copyImageToBuffer(vk, commandBuffer, *m_imageStencilDepth, m_imageDepthBuffer->object(),
						  tcu::IVec2(m_parameters.renderSize.x(), m_parameters.renderSize.y()),
						  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, imagesLayout.oldDepth,
						  1u, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
		imagesLayout.oldDepth = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
	{
		copyImageToBuffer(vk, commandBuffer, *m_imageStencilDepth, m_imageStencilBuffer->object(),
						  tcu::IVec2(m_parameters.renderSize.x(), m_parameters.renderSize.y()),
						  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, imagesLayout.oldStencil,
						  1u, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_ASPECT_STENCIL_BIT);
		imagesLayout.oldStencil = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
}

void	DynamicRenderingTestInstance::verifyResults (const deUint32			colorAtchCount,
													 const ImagesFormat&	imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	tcu::TestLog&			log		= m_context.getTestContext().getLog();

	if (imagesFormat.colors[0] != VK_FORMAT_UNDEFINED)
	{
		for (deUint32 ndx = 0; ndx < colorAtchCount; ndx++)
		{
			const Allocation allocColor = m_imageBuffer[ndx]->getBoundMemory();
			invalidateAlloc(vk, device, allocColor);
			const tcu::ConstPixelBufferAccess	resultColorImage(mapVkFormat(m_parameters.imageFormat), m_parameters.renderSize.x(), m_parameters.renderSize.y(), 1u, allocColor.getHostPtr());

			if (!tcu::floatThresholdCompare(log, "Compare Color Image", "Result comparison",
				m_referenceImages[ndx].getAccess(), resultColorImage,
				Vec4(0.02f), tcu::COMPARE_LOG_ON_ERROR))
			{
				TCU_FAIL("Rendered color image is not correct");
			}
		}
	}

	if (imagesFormat.depth != VK_FORMAT_UNDEFINED)
	{
		const Allocation			allocDepth = m_imageDepthBuffer->getBoundMemory();
		invalidateAlloc(vk, device, allocDepth);

		const tcu::ConstPixelBufferAccess	resultDepthImage(getDepthTextureFormat(m_formatStencilDepthImage),
															 m_parameters.renderSize.x(),
															 m_parameters.renderSize.y(),
															 1u, allocDepth.getHostPtr());
		if (m_formatStencilDepthImage == VK_FORMAT_D24_UNORM_S8_UINT)
		{
			de::MovePtr<tcu::TextureLevel>	result(new tcu::TextureLevel(mapVkFormat(m_formatStencilDepthImage),
														m_parameters.renderSize.x(), m_parameters.renderSize.y(), 1u));
			tcu::copy(tcu::getEffectiveDepthStencilAccess(result->getAccess(), tcu::Sampler::MODE_DEPTH), resultDepthImage);

			const tcu::ConstPixelBufferAccess	depthResult		= tcu::getEffectiveDepthStencilAccess(result->getAccess(), tcu::Sampler::MODE_DEPTH);
			const tcu::ConstPixelBufferAccess	expectedResult	= tcu::getEffectiveDepthStencilAccess(m_referenceImages[COLOR_ATTACHMENTS_NUMBER].getAccess(), tcu::Sampler::MODE_DEPTH);

			if (!tcu::intThresholdCompare(log, "Compare Depth Image", "Result comparison",
				expectedResult, depthResult,UVec4(0, 0, 0, 0), tcu::COMPARE_LOG_ON_ERROR))
			{
				TCU_FAIL("Rendered depth image is not correct");
			}
		}
		else
		{
			if (!tcu::floatThresholdCompare(log, "Compare Depth Image", "Result comparison",
				m_referenceImages[COLOR_ATTACHMENTS_NUMBER].getAccess(), resultDepthImage,
				Vec4(0.02f), tcu::COMPARE_LOG_ON_ERROR))
			{
				TCU_FAIL("Rendered depth image is not correct");
			}
		}
	}

	if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
	{
		const Allocation allocStencil = m_imageStencilBuffer->getBoundMemory();
		invalidateAlloc(vk, device, allocStencil);
		const tcu::ConstPixelBufferAccess	resultStencilImage(mapVkFormat(VK_FORMAT_S8_UINT),
															   m_parameters.renderSize.x(),
															   m_parameters.renderSize.y(),
															   1u, allocStencil.getHostPtr());

		if (!tcu::intThresholdCompare(log, "Compare Stencil Image", "Result comparison",
			m_referenceImages[COLOR_ATTACHMENTS_NUMBER + 1].getAccess(), resultStencilImage,
			UVec4(0, 0, 0, 0), tcu::COMPARE_LOG_ON_ERROR))
		{
			TCU_FAIL("Rendered stencil image is not correct");
		}
	}
}

class SingleCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			SingleCmdBufferResuming	(Context&							context,
									 const TestParameters&				parameters);
protected:
	void	rendering				(const VkPipeline					pipeline,
									 const std::vector<VkImageView>&	attachmentBindInfos,
									 const deUint32						colorAtchCount,
									 ImagesLayout&						imagesLayout,
									 const ImagesFormat&				imagesFormat) override;
};

SingleCmdBufferResuming::SingleCmdBufferResuming (Context&				context,
													const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
}

void	SingleCmdBufferResuming::rendering (const VkPipeline				pipeline,
											const std::vector<VkImageView>&	attachmentBindInfos,
											const deUint32					colorAtchCount,
											ImagesLayout&					imagesLayout,
											const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		{
			const VkBuffer vertexBuffer = m_vertexBuffer->object();
			const VkDeviceSize vertexBufferOffset = 0ull;
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 8u, 0u);
		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 0u);
		vk.cmdEndRendering(*m_cmdBuffer);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 4u, 0u);

		vk.cmdEndRendering(*m_cmdBuffer);

		copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear1.size()),
										clearData.colorDepthClear1.data(),
										1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer, 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			vk.cmdEndRendering(*m_cmdBuffer);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer, 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(*m_cmdBuffer);

			copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class TwoPrimaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			TwoPrimaryCmdBufferResuming	(Context&							context,
										 const TestParameters&				parameters);
protected:
	void	rendering					(const VkPipeline					pipeline,
										 const std::vector<VkImageView>&	attachmentBindInfos,
										 const deUint32						colorAtchCount,
										 ImagesLayout&						imagesLayout,
										 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_cmdBuffer2;
};

TwoPrimaryCmdBufferResuming::TwoPrimaryCmdBufferResuming (Context&				context,
														  const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_cmdBuffer2 = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

void	TwoPrimaryCmdBufferResuming::rendering (const VkPipeline				pipeline,
												const std::vector<VkImageView>&	attachmentBindInfos,
												const deUint32					colorAtchCount,
												ImagesLayout&					imagesLayout,
												const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		// First Primary CommandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		{
			const VkBuffer vertexBuffer = m_vertexBuffer->object();
			const VkDeviceSize vertexBufferOffset = 0ull;
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 8u, 0u);
		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 0u);

		vk.cmdEndRendering(*m_cmdBuffer);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		// Second Primary CommandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer2);

		beginRendering(*m_cmdBuffer2,
					   attachmentBindInfos,
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(*m_cmdBuffer2, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		{
			const VkBuffer vertexBuffer = m_vertexBuffer->object();
			const VkDeviceSize vertexBufferOffset = 0ull;
			vk.cmdBindVertexBuffers(*m_cmdBuffer2, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		vk.cmdDraw(*m_cmdBuffer2, 4u, 1u, 4u, 0u);

		vk.cmdEndRendering(*m_cmdBuffer2);

		copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// First Primary CommandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear1.size()),
										clearData.colorDepthClear1.data(),
										1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer, 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			vk.cmdEndRendering(*m_cmdBuffer);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			// Second Primary CommandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer2);

			beginRendering(*m_cmdBuffer2,
						   attachmentBindInfos,
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer2,
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer2, 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(*m_cmdBuffer2);

			copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class TwoSecondaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			TwoSecondaryCmdBufferResuming	(Context&							context,
											 const TestParameters&				parameters);
protected:
	void	rendering						(const VkPipeline					pipeline,
											 const std::vector<VkImageView>&	attachmentBindInfos,
											 const deUint32						colorAtchCount,
											 ImagesLayout&						imagesLayout,
											 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_secCmdBuffers[2];
};

TwoSecondaryCmdBufferResuming::TwoSecondaryCmdBufferResuming (Context&				context,
															  const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_secCmdBuffers[0] = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	m_secCmdBuffers[1] = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	TwoSecondaryCmdBufferResuming::rendering (const VkPipeline					pipeline,
												  const std::vector<VkImageView>&	attachmentBindInfos,
												  const deUint32					colorAtchCount,
												  ImagesLayout&						imagesLayout,
												  const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		VkCommandBuffer		secCmdBuffers[2]	= {
													*(m_secCmdBuffers[0]),
													*(m_secCmdBuffers[1])
												  };
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffersFirst
		beginSecondaryCmdBuffer(vk, secCmdBuffers[0]);

		beginRendering(secCmdBuffers[0],
					   attachmentBindInfos,
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(secCmdBuffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[0], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 8u, 0u);
		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 0u, 0u);

		vk.cmdEndRendering(secCmdBuffers[0]);
		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

		// secCmdBuffersSecond
		beginSecondaryCmdBuffer(vk, secCmdBuffers[1]);

		beginRendering(secCmdBuffers[1],
					   attachmentBindInfos,
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(secCmdBuffers[1], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[1], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[1], 4u, 1u, 4u, 0u);

		vk.cmdEndRendering(secCmdBuffers[1]);
		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		vk.cmdExecuteCommands(*m_cmdBuffer, 2u, secCmdBuffers);

		copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

		// secCmdBuffersFirst
			beginSecondaryCmdBuffer(vk, secCmdBuffers[0]);

			beginRendering(secCmdBuffers[0],
						   attachmentBindInfos,
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[0],
									   static_cast<deUint32>(clearData.colorDepthClear1.size()),
									   clearData.colorDepthClear1.data(),
									   1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[0], 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			vk.cmdEndRendering(secCmdBuffers[0]);
			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

			// secCmdBuffersSecond
			beginSecondaryCmdBuffer(vk, secCmdBuffers[1]);

			beginRendering(secCmdBuffers[1],
						   attachmentBindInfos,
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[1],
									   static_cast<deUint32>(clearData.colorDepthClear2.size()),
									   clearData.colorDepthClear2.data(),
									   1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[1], 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(secCmdBuffers[1]);
			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			vk.cmdExecuteCommands(*m_cmdBuffer, 2u, secCmdBuffers);

			copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);
			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class TwoSecondaryTwoPrimaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			TwoSecondaryTwoPrimaryCmdBufferResuming	(Context&							context,
													 const TestParameters&				parameters);
protected:
	void	rendering								(const VkPipeline					pipeline,
													 const std::vector<VkImageView>&	attachmentBindInfos,
													 const deUint32						colorAtchCount,
													 ImagesLayout&						imagesLayout,
													 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_cmdBuffer2;
	Move<VkCommandBuffer>	m_secCmdBuffers[2];
};

TwoSecondaryTwoPrimaryCmdBufferResuming::TwoSecondaryTwoPrimaryCmdBufferResuming (Context&				context,
																				  const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_cmdBuffer2		= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	m_secCmdBuffers[0]	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	m_secCmdBuffers[1]	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	TwoSecondaryTwoPrimaryCmdBufferResuming::rendering (const VkPipeline				pipeline,
															const std::vector<VkImageView>&	attachmentBindInfos,
															const deUint32					colorAtchCount,
															ImagesLayout&					imagesLayout,
															const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		VkCommandBuffer		secCmdBuffers[2]	= {
													*(m_secCmdBuffers[0]),
													*(m_secCmdBuffers[1])
												  };
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffersFirst
		beginSecondaryCmdBuffer(vk, secCmdBuffers[0]);

		beginRendering(secCmdBuffers[0],
					   attachmentBindInfos,
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(secCmdBuffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[0], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 8u, 0u);
		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 0u, 0u);

		vk.cmdEndRendering(secCmdBuffers[0]);
		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));


		// secCmdBuffersSecond
		beginSecondaryCmdBuffer(vk, secCmdBuffers[1]);

		beginRendering(secCmdBuffers[1],
					   attachmentBindInfos,
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(secCmdBuffers[1], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[1], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[1], 4u, 1u, 4u, 0u);

		vk.cmdEndRendering(secCmdBuffers[1]);
		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffers[0]);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		// Primary commandBuffer2
		beginCommandBuffer(vk, *m_cmdBuffer2);

		vk.cmdExecuteCommands(*m_cmdBuffer2, 1u, &secCmdBuffers[1]);

		copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffersFirst
			beginSecondaryCmdBuffer(vk, secCmdBuffers[0]);

			beginRendering(secCmdBuffers[0],
						   attachmentBindInfos,
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[0],
									   static_cast<deUint32>(clearData.colorDepthClear1.size()),
									   clearData.colorDepthClear1.data(),
									   1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[0], 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			vk.cmdEndRendering(secCmdBuffers[0]);
			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

			// secCmdBuffersSecond
			beginSecondaryCmdBuffer(vk, secCmdBuffers[1]);

			beginRendering(secCmdBuffers[1],
						   attachmentBindInfos,
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[1],
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[1], 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(secCmdBuffers[1]);
			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffers[0]);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			// Primary commandBuffer2
			beginCommandBuffer(vk, *m_cmdBuffer2);

			vk.cmdExecuteCommands(*m_cmdBuffer2, 1u, &secCmdBuffers[1]);

			copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);
			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class ContentsSecondaryCmdBuffer : public DynamicRenderingTestInstance
{
public:
			ContentsSecondaryCmdBuffer	(Context&							context,
										 const TestParameters&				parameters);
protected:
	void	rendering					(const VkPipeline					pipeline,
										 const std::vector<VkImageView>&	attachmentBindInfos,
										 const deUint32						colorAtchCount,
										 ImagesLayout&						imagesLayout,
										 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_secCmdBuffers;
};

ContentsSecondaryCmdBuffer::ContentsSecondaryCmdBuffer (Context&				context,
														const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_secCmdBuffers	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	ContentsSecondaryCmdBuffer::rendering (const VkPipeline					pipeline,
											   const std::vector<VkImageView>&	attachmentBindInfos,
											   const deUint32					colorAtchCount,
											   ImagesLayout&					imagesLayout,
											   const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffers
		beginSecondaryCmdBuffer(vk, *m_secCmdBuffers, (VkRenderingFlagsKHR)0u, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(*m_secCmdBuffers, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_secCmdBuffers, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_secCmdBuffers, 4u, 1u, 8u, 0u);
		vk.cmdDraw(*m_secCmdBuffers, 4u, 1u, 0u, 0u);
		vk.cmdDraw(*m_secCmdBuffers, 4u, 1u, 4u, 0u);

		VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffers));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &(*m_secCmdBuffers));

		vk.cmdEndRendering(*m_cmdBuffer);

		copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffers
			beginSecondaryCmdBuffer(vk, *m_secCmdBuffers, (VkRenderingFlagsKHR)0u, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(*m_secCmdBuffers,
									   static_cast<deUint32>(clearData.colorDepthClear1.size()),
									   clearData.colorDepthClear1.data(),
									   1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_secCmdBuffers, 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(*m_secCmdBuffers,
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_secCmdBuffers, 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffers));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &(*m_secCmdBuffers));

			vk.cmdEndRendering(*m_cmdBuffer);

			copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);
			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class ContentsTwoSecondaryCmdBuffer : public DynamicRenderingTestInstance
{
public:
			ContentsTwoSecondaryCmdBuffer		(Context&							context,
												 const TestParameters&				parameters);
protected:
	void	rendering							(const VkPipeline					pipeline,
												 const std::vector<VkImageView>&	attachmentBindInfos,
												 const deUint32						colorAtchCount,
												 ImagesLayout&						imagesLayout,
												 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_secCmdBuffers[2];
};

ContentsTwoSecondaryCmdBuffer::ContentsTwoSecondaryCmdBuffer (Context&				context,
															  const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_secCmdBuffers[0] = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	m_secCmdBuffers[1] = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	ContentsTwoSecondaryCmdBuffer::rendering (const VkPipeline					pipeline,
												  const std::vector<VkImageView>&	attachmentBindInfos,
												  const deUint32					colorAtchCount,
												  ImagesLayout&						imagesLayout,
												  const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	VkCommandBuffer		secCmdBuffers[2] = {
												*(m_secCmdBuffers[0]),
												*(m_secCmdBuffers[1])
											};

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffers
		beginSecondaryCmdBuffer(vk, secCmdBuffers[0], (VkRenderingFlagsKHR)0u, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(secCmdBuffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[0], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 8u, 0u);
		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 0u, 0u);

		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

		// secCmdBuffers2
		beginSecondaryCmdBuffer(vk, secCmdBuffers[1], (VkRenderingFlagsKHR)0u, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(secCmdBuffers[1], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[1], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[1], 4u, 1u, 4u, 0u);

		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer, 2u, secCmdBuffers);

		vk.cmdEndRendering(*m_cmdBuffer);

		copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffers
			beginSecondaryCmdBuffer(vk, secCmdBuffers[0], (VkRenderingFlagsKHR)0u, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[0],
									   static_cast<deUint32>(clearData.colorDepthClear1.size()),
									   clearData.colorDepthClear1.data(),
									   1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[0], 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

			// secCmdBuffers2
			beginSecondaryCmdBuffer(vk, secCmdBuffers[1], (VkRenderingFlagsKHR)0u, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[1],
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[1], 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer, 2u, secCmdBuffers);

			vk.cmdEndRendering(*m_cmdBuffer);

			copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);
			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class ContentsTwoSecondaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			ContentsTwoSecondaryCmdBufferResuming	(Context&							context,
													 const TestParameters&				parameters);
protected:
	void	rendering								(const VkPipeline					pipeline,
													 const std::vector<VkImageView>&	attachmentBindInfos,
													 const deUint32						colorAtchCount,
													 ImagesLayout&						imagesLayout,
													 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_secCmdBuffers[2];
};

ContentsTwoSecondaryCmdBufferResuming::ContentsTwoSecondaryCmdBufferResuming (Context&				context,
																			  const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_secCmdBuffers[0] = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	m_secCmdBuffers[1] = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	ContentsTwoSecondaryCmdBufferResuming::rendering (const VkPipeline					pipeline,
														  const std::vector<VkImageView>&	attachmentBindInfos,
														  const deUint32					colorAtchCount,
														  ImagesLayout&						imagesLayout,
														  const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	VkCommandBuffer		secCmdBuffers[2] = {
												*(m_secCmdBuffers[0]),
												*(m_secCmdBuffers[1])
											};

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffers
		beginSecondaryCmdBuffer(vk, secCmdBuffers[0], VK_RENDERING_SUSPENDING_BIT_KHR, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(secCmdBuffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[0], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 8u, 0u);
		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 0u, 0u);

		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

		// secCmdBuffers2
		beginSecondaryCmdBuffer(vk, secCmdBuffers[1], VK_RENDERING_RESUMING_BIT_KHR, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(secCmdBuffers[1], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[1], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[1], 4u, 1u, 4u, 0u);

		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffers[0]);

		vk.cmdEndRendering(*m_cmdBuffer);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffers[1]);

		vk.cmdEndRendering(*m_cmdBuffer);

		copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffers
			beginSecondaryCmdBuffer(vk, secCmdBuffers[0], VK_RENDERING_SUSPENDING_BIT_KHR, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[0],
									   static_cast<deUint32>(clearData.colorDepthClear1.size()),
									   clearData.colorDepthClear1.data(),
									   1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[0], 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

			// secCmdBuffers2
			beginSecondaryCmdBuffer(vk, secCmdBuffers[1], VK_RENDERING_RESUMING_BIT_KHR, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[1],
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[1], 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffers[0]);

			vk.cmdEndRendering(*m_cmdBuffer);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffers[1]);

			vk.cmdEndRendering(*m_cmdBuffer);

			copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);
			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class ContentsTwoSecondaryTwoPrimaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			ContentsTwoSecondaryTwoPrimaryCmdBufferResuming	(Context&							context,
															 const TestParameters&				parameters);
protected:
	void	rendering										(const VkPipeline					pipeline,
															 const std::vector<VkImageView>&	attachmentBindInfos,
															 const deUint32						colorAtchCount,
															 ImagesLayout&						imagesLayout,
															 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_cmdBuffer2;
	Move<VkCommandBuffer>	m_secCmdBuffers[2];
};

ContentsTwoSecondaryTwoPrimaryCmdBufferResuming::ContentsTwoSecondaryTwoPrimaryCmdBufferResuming	(Context&				context,
																									 const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_cmdBuffer2		= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	m_secCmdBuffers[0]	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	m_secCmdBuffers[1]	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	ContentsTwoSecondaryTwoPrimaryCmdBufferResuming::rendering (const VkPipeline				pipeline,
																	const std::vector<VkImageView>&	attachmentBindInfos,
																	const deUint32					colorAtchCount,
																	ImagesLayout&					imagesLayout,
																	const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	VkCommandBuffer		secCmdBuffers[2] = {
												*(m_secCmdBuffers[0]),
												*(m_secCmdBuffers[1])
											};

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffers
		beginSecondaryCmdBuffer(vk, secCmdBuffers[0], VK_RENDERING_SUSPENDING_BIT_KHR, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(secCmdBuffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[0], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 8u, 0u);
		vk.cmdDraw(secCmdBuffers[0], 4u, 1u, 0u, 0u);

		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

		// secCmdBuffers2
		beginSecondaryCmdBuffer(vk, secCmdBuffers[1], VK_RENDERING_RESUMING_BIT_KHR, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(secCmdBuffers[1], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(secCmdBuffers[1], 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(secCmdBuffers[1], 4u, 1u, 4u, 0u);

		VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffers[0]);

		vk.cmdEndRendering(*m_cmdBuffer);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		// Primary commandBuffer2
		beginCommandBuffer(vk, *m_cmdBuffer2);

		beginRendering(*m_cmdBuffer2,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer2, 1u, &secCmdBuffers[1]);

		vk.cmdEndRendering(*m_cmdBuffer2);

		copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffers
			beginSecondaryCmdBuffer(vk, secCmdBuffers[0], VK_RENDERING_SUSPENDING_BIT_KHR, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[0],
										static_cast<deUint32>(clearData.colorDepthClear1.size()),
										clearData.colorDepthClear1.data(),
										1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[0], 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[0]));

			// secCmdBuffers2
			beginSecondaryCmdBuffer(vk, secCmdBuffers[1], VK_RENDERING_RESUMING_BIT_KHR, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(secCmdBuffers[1],
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(secCmdBuffers[1], 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			VK_CHECK(vk.endCommandBuffer(secCmdBuffers[1]));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffers[0]);

			vk.cmdEndRendering(*m_cmdBuffer);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			// Primary commandBuffer2
			beginCommandBuffer(vk, *m_cmdBuffer2);

			beginRendering(*m_cmdBuffer2,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer2, 1u, &secCmdBuffers[1]);

			vk.cmdEndRendering(*m_cmdBuffer2);

			copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class ContentsPrimarySecondaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			ContentsPrimarySecondaryCmdBufferResuming	(Context&							context,
														 const TestParameters&				parameters);
protected:
	void	rendering									(const VkPipeline					pipeline,
														 const std::vector<VkImageView>&	attachmentBindInfos,
														 const deUint32						colorAtchCount,
														 ImagesLayout&						imagesLayout,
														 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_secCmdBuffer;
};

ContentsPrimarySecondaryCmdBufferResuming::ContentsPrimarySecondaryCmdBufferResuming (Context&				context,
																					  const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_secCmdBuffer	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	ContentsPrimarySecondaryCmdBufferResuming::rendering (const VkPipeline					pipeline,
															  const std::vector<VkImageView>&	attachmentBindInfos,
															  const deUint32					colorAtchCount,
															  ImagesLayout&						imagesLayout,
															  const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffer
		beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, VK_RENDERING_RESUMING_BIT_KHR, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(*m_secCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_secCmdBuffer, 4u, 1u, 4u, 0u);

		VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffer));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 8u, 0u);
		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 0u);

		vk.cmdEndRendering(*m_cmdBuffer);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &(*m_secCmdBuffer));

		vk.cmdEndRendering(*m_cmdBuffer);

		copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffer
			beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, VK_RENDERING_RESUMING_BIT_KHR, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(*m_secCmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear1.size()),
										clearData.colorDepthClear1.data(),
										1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_secCmdBuffer, 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffer));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer, 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(*m_cmdBuffer);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &(*m_secCmdBuffer));

			vk.cmdEndRendering(*m_cmdBuffer);

			copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class ContentsSecondaryPrimaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			ContentsSecondaryPrimaryCmdBufferResuming	(Context&							context,
														 const TestParameters&				parameters);
protected:
	void	rendering									(const VkPipeline					pipeline,
														 const std::vector<VkImageView>&	attachmentBindInfos,
														 const deUint32						colorAtchCount,
														 ImagesLayout&						imagesLayout,
														 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_secCmdBuffer;
};

ContentsSecondaryPrimaryCmdBufferResuming::ContentsSecondaryPrimaryCmdBufferResuming (Context&				context,
																					  const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_secCmdBuffer	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	ContentsSecondaryPrimaryCmdBufferResuming::rendering (const VkPipeline					pipeline,
															  const std::vector<VkImageView>&	attachmentBindInfos,
															  const deUint32					colorAtchCount,
															  ImagesLayout&						imagesLayout,
															  const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffer
		beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, VK_RENDERING_SUSPENDING_BIT_KHR, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(*m_secCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_secCmdBuffer, 4u, 1u, 8u, 0u);
		vk.cmdDraw(*m_secCmdBuffer, 4u, 1u, 0u, 0u);

		VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffer));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &(*m_secCmdBuffer));

		vk.cmdEndRendering(*m_cmdBuffer);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 4u, 0u);

		vk.cmdEndRendering(*m_cmdBuffer);

		copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffer
			beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, VK_RENDERING_SUSPENDING_BIT_KHR, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(*m_secCmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear1.size()),
										clearData.colorDepthClear1.data(),
										1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_secCmdBuffer, 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffer));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &(*m_secCmdBuffer));

			vk.cmdEndRendering(*m_cmdBuffer);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer, 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(*m_cmdBuffer);

			copyImgToBuff(*m_cmdBuffer, colorAtchCount, imagesLayout, imagesFormat);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class ContentsTwoPrimarySecondaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			ContentsTwoPrimarySecondaryCmdBufferResuming	(Context&							context,
															 const TestParameters&				parameters);
protected:
	void	rendering										(const VkPipeline					pipeline,
															 const std::vector<VkImageView>&	attachmentBindInfos,
															 const deUint32						colorAtchCount,
															 ImagesLayout&						imagesLayout,
															 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_cmdBuffer2;
	Move<VkCommandBuffer>	m_secCmdBuffer;
};

ContentsTwoPrimarySecondaryCmdBufferResuming::ContentsTwoPrimarySecondaryCmdBufferResuming (Context&				context,
																							const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_cmdBuffer2	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	m_secCmdBuffer	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	ContentsTwoPrimarySecondaryCmdBufferResuming::rendering (const VkPipeline					pipeline,
																 const std::vector<VkImageView>&	attachmentBindInfos,
																 const deUint32						colorAtchCount,
																 ImagesLayout&						imagesLayout,
																 const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffer
		beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, VK_RENDERING_RESUMING_BIT_KHR, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(*m_secCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_secCmdBuffer, 4u, 1u, 4u, 0u);

		VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffer));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 8u, 0u);
		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 0u);

		vk.cmdEndRendering(*m_cmdBuffer);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		// Primary commandBuffer2
		beginCommandBuffer(vk, *m_cmdBuffer2);

		beginRendering(*m_cmdBuffer2,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer2, 1u, &(*m_secCmdBuffer));

		vk.cmdEndRendering(*m_cmdBuffer2);

		copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffer
			beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, VK_RENDERING_RESUMING_BIT_KHR, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(*m_secCmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear1.size()),
										clearData.colorDepthClear1.data(),
										1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_secCmdBuffer, 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffer));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer, 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(*m_cmdBuffer);
			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			// Primary commandBuffer2
			beginCommandBuffer(vk, *m_cmdBuffer2);

			beginRendering(*m_cmdBuffer2,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer2, 1u, &(*m_secCmdBuffer));

			vk.cmdEndRendering(*m_cmdBuffer2);

			copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class ContentsSecondaryTwoPrimaryCmdBufferResuming : public DynamicRenderingTestInstance
{
public:
			ContentsSecondaryTwoPrimaryCmdBufferResuming	(Context&							context,
															 const TestParameters&				parameters);
protected:
	void	rendering										(const VkPipeline					pipeline,
															 const std::vector<VkImageView>&	attachmentBindInfos,
															 const deUint32						colorAtchCount,
															 ImagesLayout&						imagesLayout,
															 const ImagesFormat&				imagesFormat) override;

	Move<VkCommandBuffer>	m_cmdBuffer2;
	Move<VkCommandBuffer>	m_secCmdBuffer;
};

ContentsSecondaryTwoPrimaryCmdBufferResuming::ContentsSecondaryTwoPrimaryCmdBufferResuming (Context&				context,
																							const TestParameters&	parameters)
	: DynamicRenderingTestInstance(context, parameters)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_cmdBuffer2	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	m_secCmdBuffer	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void	ContentsSecondaryTwoPrimaryCmdBufferResuming::rendering (const VkPipeline					pipeline,
																 const std::vector<VkImageView>&	attachmentBindInfos,
																 const deUint32						colorAtchCount,
																 ImagesLayout&						imagesLayout,
																 const ImagesFormat&				imagesFormat)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	const VkQueue			queue	= m_context.getUniversalQueue();

	for (deUint32 attachmentLoadOp  = 0; attachmentLoadOp  < TEST_ATTACHMENT_LOAD_OP_LAST;  ++attachmentLoadOp)
	for (deUint32 attachmentStoreOp = 0; attachmentStoreOp < TEST_ATTACHMENT_STORE_OP_LAST; ++attachmentStoreOp)
	{
		const VkBuffer		vertexBuffer		= m_vertexBuffer->object();
		const VkDeviceSize	vertexBufferOffset	= 0ull;

		// secCmdBuffer
		beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, VK_RENDERING_SUSPENDING_BIT_KHR, colorAtchCount, imagesFormat);

		vk.cmdBindPipeline(*m_secCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_secCmdBuffer, 4u, 1u, 8u, 0u);
		vk.cmdDraw(*m_secCmdBuffer, 4u, 1u, 0u, 0u);

		VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffer));

		// Primary commandBuffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preBarier(colorAtchCount, imagesLayout, imagesFormat);

		beginRendering(*m_cmdBuffer,
					   attachmentBindInfos,
					   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
					   VK_RENDERING_SUSPENDING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &(*m_secCmdBuffer));

		vk.cmdEndRendering(*m_cmdBuffer);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

		// Primary commandBuffer2
		beginCommandBuffer(vk, *m_cmdBuffer2);

		beginRendering(*m_cmdBuffer2,
					   attachmentBindInfos,
					   VK_RENDERING_RESUMING_BIT_KHR,
					   colorAtchCount,
					   imagesFormat,
					   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
					   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

		vk.cmdBindPipeline(*m_cmdBuffer2, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmdBindVertexBuffers(*m_cmdBuffer2, 0u, 1u, &vertexBuffer, &vertexBufferOffset);

		vk.cmdDraw(*m_cmdBuffer2, 4u, 1u, 4u, 0u);

		vk.cmdEndRendering(*m_cmdBuffer2);

		copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

		if ((static_cast<VkAttachmentLoadOp>(attachmentLoadOp)   == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
			(static_cast<VkAttachmentStoreOp>(attachmentStoreOp) == VK_ATTACHMENT_STORE_OP_STORE))
		{
			verifyResults(colorAtchCount, imagesFormat);

			const ClearAttachmentData clearData(colorAtchCount, imagesFormat.depth, imagesFormat.stencil);

			// secCmdBuffer
			beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, VK_RENDERING_SUSPENDING_BIT_KHR, colorAtchCount, imagesFormat);

			if (clearData.colorDepthClear1.size() != 0)
			{
				vk.cmdClearAttachments(*m_secCmdBuffer,
										static_cast<deUint32>(clearData.colorDepthClear1.size()),
										clearData.colorDepthClear1.data(),
										1, &clearData.rectColorDepth1);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_secCmdBuffer, 1u, &clearData.stencilClear1, 1, &clearData.rectStencil1);

			VK_CHECK(vk.endCommandBuffer(*m_secCmdBuffer));

			// Primary commandBuffer
			beginCommandBuffer(vk, *m_cmdBuffer);
			preBarier(colorAtchCount, imagesLayout, imagesFormat);

			beginRendering(*m_cmdBuffer,
						   attachmentBindInfos,
						   VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR |
						   VK_RENDERING_SUSPENDING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &(*m_secCmdBuffer));

			vk.cmdEndRendering(*m_cmdBuffer);
			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

			// Primary commandBuffer2
			beginCommandBuffer(vk, *m_cmdBuffer2);

			beginRendering(*m_cmdBuffer2,
						   attachmentBindInfos,
						   VK_RENDERING_RESUMING_BIT_KHR,
						   colorAtchCount,
						   imagesFormat,
						   static_cast<VkAttachmentLoadOp>(attachmentLoadOp),
						   static_cast<VkAttachmentStoreOp>(attachmentStoreOp));

			if (clearData.colorDepthClear2.size() != 0)
			{
				vk.cmdClearAttachments(*m_cmdBuffer2,
										static_cast<deUint32>(clearData.colorDepthClear2.size()),
										clearData.colorDepthClear2.data(),
										1, &clearData.rectColorDepth2);
			}

			if (imagesFormat.stencil != VK_FORMAT_UNDEFINED)
				vk.cmdClearAttachments(*m_cmdBuffer2, 1u, &clearData.stencilClear2, 1, &clearData.rectStencil2);

			vk.cmdEndRendering(*m_cmdBuffer2);

			copyImgToBuff(*m_cmdBuffer2, colorAtchCount, imagesLayout, imagesFormat);

			VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer2));

			submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, *m_cmdBuffer2);

			verifyResults(colorAtchCount, imagesFormat);
		}
	}
}

class BaseTestCase : public TestCase
{
public:
							BaseTestCase	(tcu::TestContext&		context,
											 const std::string&		name,
											 const std::string&		description,
											 const TestParameters&	parameters);
	virtual					~BaseTestCase	(void);

protected:
	virtual void			checkSupport	(Context&				context) const;
	virtual void			initPrograms	(SourceCollections&		programCollection) const;
	virtual TestInstance*	createInstance	(Context&				context) const;

	const TestParameters	m_parameters;
};

BaseTestCase::BaseTestCase (tcu::TestContext&		context,
							const std::string&		name,
							const std::string&		description,
							const TestParameters&	parameters)
	: TestCase		(context, name, description)
	, m_parameters	(parameters)
{
}

BaseTestCase::~BaseTestCase ()
{
}

void BaseTestCase::checkSupport (Context& context) const
{
	if(!context.requireDeviceFunctionality("VK_KHR_dynamic_rendering"))
		TCU_THROW(NotSupportedError, "VK_KHR_dynamic_rendering not supported");
}

void BaseTestCase::initPrograms (SourceCollections& programCollection) const
{
	// Vertex
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "layout(location = 0) out highp vec4 vsColor;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "   vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "    vsColor     = vec4(gl_Position.z * 5.0f, 1.0f, 0.0f, 1.0f);\n"
			<< "}\n";
		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment multi color attachment
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 vsColor;\n";

			for (deUint32 ndx = 0; ndx < COLOR_ATTACHMENTS_NUMBER; ++ndx)
				src << "layout(location = " << ndx << ") out highp vec4 fsColor" << ndx << ";\n";

		src << "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    vec4 color   = vsColor;\n";
		for (deUint32 ndx = 0; ndx < COLOR_ATTACHMENTS_NUMBER; ++ndx)
		{
			src << "    color.z      = 0.15f * " << ndx << ".0f;\n"
				<< "    fsColor" << ndx << "     = color;\n";
		}
		src << "}\n";
		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

TestInstance*	BaseTestCase::createInstance (Context& context) const
{
	switch(m_parameters.testType)
	{
		case TEST_TYPE_SINGLE_CMDBUF:
		{
			return new DynamicRenderingTestInstance(context, m_parameters);
		}
		case TEST_TYPE_SINGLE_CMDBUF_RESUMING:
		{
			return new SingleCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_TWO_CMDBUF_RESUMING:
		{
			return new TwoPrimaryCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_SECONDARY_CMDBUF_RESUMING:
		{
			return new TwoSecondaryCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_SECONDARY_CMDBUF_TWO_PRIMARY_RESUMING:
		{
			return new TwoSecondaryTwoPrimaryCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_CONTENTS_SECONDARY_COMMAND_BUFFER:
		{
			return new ContentsSecondaryCmdBuffer(context, m_parameters);
		}
		case TEST_TYPE_CONTENTS_2_SECONDARY_COMMAND_BUFFER:
		{
			return new ContentsTwoSecondaryCmdBuffer(context, m_parameters);
		}
		case TEST_TYPE_CONTENTS_2_SECONDARY_COMMAND_BUFFER_RESUMING:
		{
			return new ContentsTwoSecondaryCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_CONTENTS_2_SECONDARY_2_PRIMARY_COMDBUF_RESUMING:
		{
			return new ContentsTwoSecondaryTwoPrimaryCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_CONTENTS_PRIMARY_SECONDARY_COMDBUF_RESUMING:
		{
			return new ContentsPrimarySecondaryCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_CONTENTS_SECONDARY_PRIMARY_COMDBUF_RESUMING:
		{
			return new ContentsSecondaryPrimaryCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_CONTENTS_2_PRIMARY_SECONDARY_COMDBUF_RESUMING:
		{
			return new ContentsTwoPrimarySecondaryCmdBufferResuming(context, m_parameters);
		}
		case TEST_TYPE_CONTENTS_SECONDARY_2_PRIMARY_COMDBUF_RESUMING:
		{
			return new ContentsSecondaryTwoPrimaryCmdBufferResuming(context, m_parameters);
		}
		default:
			DE_FATAL("Impossible");
	}
	return nullptr;
}

tcu::TestNode* dynamicRenderingTests (tcu::TestContext& testCtx, const TestParameters& parameters)
{
	const std::string testName[TEST_TYPE_LAST] =
	{
		"single_cmdbuffer",
		"single_cmdbuffer_resuming",
		"2_cmdbuffers_resuming",
		"2_secondary_cmdbuffers_resuming",
		"2_secondary_2_primary_cmdbuffers_resuming",
		"contents_secondary_cmdbuffers",
		"contents_2_secondary_cmdbuffers",
		"contents_2_secondary_cmdbuffers_resuming",
		"contents_2_secondary_2_primary_cmdbuffers_resuming",
		"contents_primary_secondary_cmdbuffers_resuming",
		"contents_secondary_primary_cmdbuffers_resuming",
		"contents_2_primary_secondary_cmdbuffers_resuming",
		"contents_secondary_2_primary_cmdbuffers_resuming",
	};

	return new BaseTestCase(testCtx, testName[parameters.testType], "Dynamic Rendering tests", parameters);
}

}	// anonymous

tcu::TestCaseGroup* createDynamicRenderingBasicTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> dynamicRenderingGroup (new tcu::TestCaseGroup(testCtx, "basic", "Basic dynamic rendering tests"));

	for (int testType = 0; testType < TEST_TYPE_LAST; ++testType)
	{
		const TestParameters	parameters =
		{
			static_cast<TestType>(testType),	// TestType			testType;
			Vec4(0.0f, 0.0f, 0.0f, 1.0f),		// const Vec4		clearColor;
			VK_FORMAT_R8G8B8A8_UNORM,			// const VkFormat	imageFormat;
			(UVec2(32, 32))						// const UVec2		renderSize;
		};

		dynamicRenderingGroup->addChild(dynamicRenderingTests(testCtx, parameters));
	}

	return dynamicRenderingGroup.release();
}

} // renderpass
} // vkt
