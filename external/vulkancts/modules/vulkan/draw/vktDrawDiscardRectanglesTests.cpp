/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief EXT_discard_rectangles tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawDiscardRectanglesTests.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"

#include "tcuTestCase.hpp"
#include "tcuVector.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

namespace vkt
{
namespace Draw
{

namespace
{
using namespace vk;
using de::UniquePtr;
using de::SharedPtr;
using de::MovePtr;
using tcu::Vec4;
using tcu::Vec2;
using tcu::UVec2;
using tcu::UVec4;

enum TestMode
{
	TEST_MODE_INCLUSIVE = 0,
	TEST_MODE_EXCLUSIVE,
	TEST_MODE_COUNT
};

enum TestScissorMode
{
	TEST_SCISSOR_MODE_NONE = 0,
	TEST_SCISSOR_MODE_STATIC,
	TEST_SCISSOR_MODE_DYNAMIC,
	TEST_SCISSOR_MODE_COUNT
};

#define NUM_RECT_TESTS 6
#define NUM_DYNAMIC_DISCARD_TYPE_TESTS 2

struct TestParams
{
	TestMode				testMode;
	deUint32				numRectangles;
	deBool					dynamicDiscardRectangles;
	TestScissorMode			scissorMode;
	const SharedGroupParams	groupParams;
};

template<typename T>
inline VkDeviceSize sizeInBytes(const std::vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
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

VkPipelineDiscardRectangleStateCreateInfoEXT makeDiscardRectangleStateCreateInfo (const deBool						dynamicDiscardRectangle,
																				  const VkDiscardRectangleModeEXT	discardRectangleMode,
																				  const deUint32					discardRectangleCount,
																				  const VkRect2D					*pDiscardRectangles)
{
	const VkPipelineDiscardRectangleStateCreateInfoEXT discardRectanglesCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT,		// VkStructureType									sType;
		DE_NULL,																// const void*										pNext;
		0u,																		// VkPipelineDiscardRectangleStateCreateFlagsEXT	flags;
		discardRectangleMode,													// VkDiscardRectangleModeEXT						discardRectangleMode;
		discardRectangleCount,													// deUint32											discardRectangleCount;
		dynamicDiscardRectangle ? DE_NULL : pDiscardRectangles					// const VkRect2D*									pDiscardRectangles;
	};
	return discardRectanglesCreateInfo;
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&			vk,
									   const VkDevice					device,
									   const VkPipelineLayout			pipelineLayout,
									   const VkRenderPass				renderPass,
									   const VkShaderModule				vertexModule,
									   const VkShaderModule				fragmentModule,
									   const UVec2						renderSize,
									   const deBool						dynamicDiscardRectangle,
									   const VkDiscardRectangleModeEXT	discardRectangleMode,
									   const deUint32					discardRectangleCount,
									   const VkRect2D*					pDiscardRectangles,
									   const TestScissorMode			scissorMode,
									   const VkRect2D					rectScissor)
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


	VkViewport		viewport				= makeViewport(0.0f, 0.0f, static_cast<float>(renderSize.x()), static_cast<float>(renderSize.y()), 0.0f, 1.0f);
	const VkRect2D	rectScissorRenderSize	= { { 0, 0 }, { renderSize.x(), renderSize.y() } };

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,							// VkStructureType								sType;
		DE_NULL,																		// const void*									pNext;
		(VkPipelineViewportStateCreateFlags)0,											// VkPipelineViewportStateCreateFlags			flags;
		1u,																				// uint32_t										viewportCount;
		&viewport,																		// const VkViewport*							pViewports;
		1u,																				// uint32_t										scissorCount;
		scissorMode != TEST_SCISSOR_MODE_NONE ? &rectScissor : &rectScissorRenderSize,	// const VkRect2D*								pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags		flags;
		VK_FALSE,														// VkBool32										depthClampEnable;
		VK_FALSE,														// VkBool32										rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode								polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags								cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace									frontFace;
		VK_FALSE,														// VkBool32										depthBiasEnable;
		0.0f,															// float										depthBiasConstantFactor;
		0.0f,															// float										depthBiasClamp;
		0.0f,															// float										depthBiasSlopeFactor;
		1.0f,															// float										lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineMultisampleStateCreateFlags)0,						// VkPipelineMultisampleStateCreateFlags		flags;
		VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits						rasterizationSamples;
		VK_FALSE,														// VkBool32										sampleShadingEnable;
		0.0f,															// float										minSampleShading;
		DE_NULL,														// const VkSampleMask*							pSampleMask;
		VK_FALSE,														// VkBool32										alphaToCoverageEnable;
		VK_FALSE														// VkBool32										alphaToOneEnable;
	};

	const VkStencilOpState stencilOpState = makeStencilOpState(
		VK_STENCIL_OP_KEEP,				// stencil fail
		VK_STENCIL_OP_KEEP,				// depth & stencil pass
		VK_STENCIL_OP_KEEP,				// depth only fail
		VK_COMPARE_OP_ALWAYS,			// compare op
		0u,								// compare mask
		0u,								// write mask
		0u);							// reference

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,						// VkPipelineDepthStencilStateCreateFlags		flags;
		VK_FALSE,														// VkBool32										depthTestEnable;
		VK_FALSE,														// VkBool32										depthWriteEnable;
		VK_COMPARE_OP_LESS,												// VkCompareOp									depthCompareOp;
		VK_FALSE,														// VkBool32										depthBoundsTestEnable;
		VK_FALSE,														// VkBool32										stencilTestEnable;
		stencilOpState,													// VkStencilOpState								front;
		stencilOpState,													// VkStencilOpState								back;
		0.0f,															// float										minDepthBounds;
		1.0f,															// float										maxDepthBounds;
	};

	const VkColorComponentFlags					colorComponentsAll					= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	const VkPipelineColorBlendAttachmentState	pipelineColorBlendAttachmentState	=
	{
		VK_FALSE,						// VkBool32					blendEnable;
		VK_BLEND_FACTOR_ONE,			// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,			// VkBlendFactor			srcAlphaBlendFactor;
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
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags				flags;
			VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBits						stage;
			vertexModule,												// VkShaderModule								module;
			"main",														// const char*									pName;
			DE_NULL,													// const VkSpecializationInfo*					pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags				flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBits						stage;
			fragmentModule,												// VkShaderModule								module;
			"main",														// const char*									pName;
			DE_NULL,													// const VkSpecializationInfo*					pSpecializationInfo;
		},
	};

	const VkPipelineDiscardRectangleStateCreateInfoEXT discardRectangleStateCreateInfo = makeDiscardRectangleStateCreateInfo(dynamicDiscardRectangle, discardRectangleMode, discardRectangleCount, pDiscardRectangles);

	const VkDynamicState dynamicStateDiscardRectangles	= VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT;
	const VkDynamicState dynamicStateScissor			= VK_DYNAMIC_STATE_SCISSOR;
	std::vector<VkDynamicState> dynamicStates;

	if (dynamicDiscardRectangle)
		dynamicStates.push_back(dynamicStateDiscardRectangles);
	if (scissorMode == TEST_SCISSOR_MODE_DYNAMIC)
		dynamicStates.push_back(dynamicStateScissor);

	const VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,			// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		0u,																// VkPipelineDynamicStateCreateFlags			flags;
		(deUint32)dynamicStates.size(),									// deUint32										dynamicStateCount;
		dynamicStates.data()											// const VkDynamicState*						pDynamicStates;
	};

	VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,				// VkStructureType									sType;
		&discardRectangleStateCreateInfo,								// const void*										pNext;
		(VkPipelineCreateFlags)0,										// VkPipelineCreateFlags							flags;
		2u,																// deUint32											stageCount;
		pShaderStages,													// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,											// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,								// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,														// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&pipelineViewportStateInfo,										// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,								// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&pipelineMultisampleStateInfo,									// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&pipelineDepthStencilStateInfo,									// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&pipelineColorBlendStateInfo,									// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		&pipelineDynamicStateCreateInfo,								// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,													// VkPipelineLayout									layout;
		renderPass,														// VkRenderPass										renderPass;
		0u,																// deUint32											subpass;
		DE_NULL,														// VkPipeline										basePipelineHandle;
		0,																// deInt32											basePipelineIndex;
	};

#ifndef CTS_USES_VULKANSC
	VkFormat colorAttachmentFormat = VK_FORMAT_R8G8B8A8_UNORM;
	vk::VkPipelineRenderingCreateInfoKHR renderingCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		&discardRectangleStateCreateInfo,
		0u,
		1u,
		&colorAttachmentFormat,
		VK_FORMAT_UNDEFINED,
		VK_FORMAT_UNDEFINED
	};

	// when pipeline is created without render pass we are using dynamic rendering
	if (renderPass == DE_NULL)
		graphicsPipelineInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

void generateDiscardRectangles(const UVec2& renderSize, deUint32 numRect, std::vector<VkRect2D>& rectangles)
{
	deUint32 cellHight = renderSize.y() - 10;
	deUint32 cellWidth = (renderSize.x() - 10) / (2 * numRect - 1);

	DE_ASSERT(rectangles.size() == 0);

	for (deUint32 i = 0; i < numRect; i++)
	{
		VkRect2D rect;
		rect.extent.height = cellHight;
		rect.extent.width = cellWidth;
		rect.offset.x = 5u + i * 2 * cellWidth;
		rect.offset.y = 5u;
		rectangles.push_back(rect);
	}
}

//! Renders a colorful grid of rectangles.
tcu::TextureLevel generateReferenceImage (const tcu::TextureFormat		format,
										  const UVec2&					renderSize,
										  const TestMode				testMode,
										  const Vec4&					color,
										  const deUint32				numRectangles,
										  const std::vector<VkRect2D>	rectangles,
										  const deBool					enableScissor,
										  const VkRect2D				scissor)
{
	tcu::TextureLevel	image(format, renderSize.x(), renderSize.y());
	const Vec4			rectColor	= testMode == TEST_MODE_INCLUSIVE ? Vec4(0.0f, 1.0f, 0.0f, 1.0f)	: color;
	const Vec4			clearColor	= testMode == TEST_MODE_INCLUSIVE ? color							: Vec4(0.0f, 1.0f, 0.0f, 1.0f);

	if (!enableScissor)
	{
		// Clear the image with clearColor
		tcu::clear(image.getAccess(), clearColor);

		// Now draw the discard rectangles taking into account the selected mode.
		for (deUint32 i = 0; i < numRectangles; i++)
		{
			tcu::clear(tcu::getSubregion(image.getAccess(), rectangles[i].offset.x, rectangles[i].offset.y,
										 rectangles[i].extent.width, rectangles[i].extent.height),
					   rectColor);
		}
	}
	else
	{
		// Clear the image with the original clear color
		tcu::clear(image.getAccess(), color);
		// Clear the scissor are with the clearColor which depends on the selected mode
		tcu::clear(tcu::getSubregion(image.getAccess(), scissor.offset.x, scissor.offset.y,
									 scissor.extent.width, scissor.extent.height),
				   clearColor);

		// Now draw the discard rectangles taking into account both the scissor area and
		// the selected mode.
		for (deUint32 rect = 0; rect < numRectangles; rect++)
		{
			for (deUint32 x = rectangles[rect].offset.x; x < (rectangles[rect].offset.x + rectangles[rect].extent.width); x++)
			{
				for(deUint32 y = rectangles[rect].offset.y; y < (rectangles[rect].offset.y + rectangles[rect].extent.height); y++)
				{
					if ((x >= (deUint32)scissor.offset.x) && (x < (scissor.offset.x + scissor.extent.width)) &&
						(y >= (deUint32)scissor.offset.y) && (y < (scissor.offset.y + scissor.extent.height)))
					{
						image.getAccess().setPixel(rectColor, x, y);
					}
				}
			}
		}
	}
	return image;
}

class DiscardRectanglesTestInstance : public TestInstance
{
public:
							DiscardRectanglesTestInstance		(Context& context,
																 TestParams params);
	virtual					~DiscardRectanglesTestInstance		(void) {}
	virtual tcu::TestStatus	iterate								(void);

protected:

	void					preRenderCommands					(VkCommandBuffer cmdBuffer) const;
	void					drawCommands						(VkCommandBuffer cmdBuffer, const VkRect2D& rectScissor) const;

#ifndef CTS_USES_VULKANSC
	void					beginSecondaryCmdBuffer				(VkCommandBuffer cmdBuffer, VkFormat colorFormat, VkRenderingFlagsKHR renderingFlags = 0u) const;
#endif // CTS_USES_VULKANSC

private:
	const TestParams			m_params;
	const Vec4					m_clearColor;
	const UVec2					m_renderSize;
	std::vector<Vec4>			m_vertices;
	std::vector<VkRect2D>		m_rectangles;

	Move<VkImage>				m_colorImage;
	MovePtr<Allocation>			m_colorImageAlloc;
	Move<VkImageView>			m_colorAttachment;
	SharedPtr<Buffer>			m_colorBuffer;
	SharedPtr<Buffer>			m_vertexBuffer;
	Move<VkShaderModule>		m_vertexModule;
	Move<VkShaderModule>		m_fragmentModule;
	Move<VkRenderPass>			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_pipeline;
	Move<VkCommandPool>			m_cmdPool;
	Move<VkCommandBuffer>		m_cmdBuffer;
	Move<VkCommandBuffer>		m_secCmdBuffer;
};

DiscardRectanglesTestInstance::DiscardRectanglesTestInstance (Context& context,
															  TestParams params)
	: TestInstance	(context)
	, m_params		(params)
	, m_clearColor	(Vec4(1.0f, 0.0f, 0.0f, 1.0f))
	, m_renderSize	(UVec2(340, 100))
{
}

tcu::TestStatus DiscardRectanglesTestInstance::iterate	(void)
{
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const InstanceInterface&		vki						= m_context.getInstanceInterface();
	const VkPhysicalDevice			physicalDevice			= m_context.getPhysicalDevice();
	const VkDevice					device					= m_context.getDevice();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	Allocator&						allocator				= m_context.getDefaultAllocator();
	const VkDiscardRectangleModeEXT	discardRectangleMode	= m_params.testMode == TEST_MODE_EXCLUSIVE ? VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT : VK_DISCARD_RECTANGLE_MODE_INCLUSIVE_EXT;
	const VkRect2D					rectScissor				= { { 90, 25 }, { 160, 50} };
	const VkImageSubresourceRange	colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkFormat					colorFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const VkDeviceSize				colorBufferSize			= m_renderSize.x() * m_renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));

	// Check for VK_EXT_discard_rectangles support and maximum number of active discard rectangles
	{
		VkPhysicalDeviceDiscardRectanglePropertiesEXT discardRectangleProperties;
		deMemset(&discardRectangleProperties, 0, sizeof(VkPhysicalDeviceDiscardRectanglePropertiesEXT));
		discardRectangleProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT;

		VkPhysicalDeviceProperties2 physicalDeviceProperties;
		physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		physicalDeviceProperties.pNext = &discardRectangleProperties;

		vki.getPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties);

		if (discardRectangleProperties.maxDiscardRectangles == 0)
		{
			throw tcu::NotSupportedError("Implementation doesn't support discard rectangles");
		}

		if (discardRectangleProperties.maxDiscardRectangles < 4)
		{
			std::ostringstream message;
			message << "Implementation doesn't support the minimum value for maxDiscardRectangles: " << discardRectangleProperties.maxDiscardRectangles << " < 4";
			return tcu::TestStatus::fail(message.str());
		}

		if (discardRectangleProperties.maxDiscardRectangles < m_params.numRectangles)
		{
			std::ostringstream message;
			message << "Implementation doesn't support the required number of discard rectangles: " << discardRectangleProperties.maxDiscardRectangles << " < " << m_params.numRectangles;
			throw tcu::NotSupportedError(message.str());
		}
	}

	// Color attachment
	{
		m_colorImage		= makeImage(vk, device, makeImageCreateInfo(colorFormat, m_renderSize, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
		m_colorImageAlloc	= bindImage(vk, device, allocator, *m_colorImage, MemoryRequirement::Any);
		m_colorBuffer		= Buffer::createAndAlloc(vk, device, makeBufferCreateInfo(colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), allocator, MemoryRequirement::HostVisible);
		m_colorAttachment	= makeImageView(vk, device, *m_colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange);

		// Zero colorBuffer.
		const Allocation alloc = m_colorBuffer->getBoundMemory();
		deMemset(alloc.getHostPtr(), 0, static_cast<std::size_t>(colorBufferSize));
		flushAlloc(vk, device, alloc);
	}

	// Initialize the pipeline and other variables
	{
		// Draw a quad covering the whole framebuffer
		m_vertices.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
		m_vertices.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
		m_vertices.push_back(Vec4( 1.0f,  1.0f, 0.0f, 1.0f));
		m_vertices.push_back(Vec4( 1.0f, -1.0f, 0.0f, 1.0f));
		VkDeviceSize vertexBufferSize	= sizeInBytes(m_vertices);
		m_vertexBuffer					= Buffer::createAndAlloc	(vk, device, makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), allocator, MemoryRequirement::HostVisible);


		deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), m_vertices.data(), static_cast<std::size_t>(vertexBufferSize));
		flushAlloc(vk, device, m_vertexBuffer->getBoundMemory());

		m_vertexModule				= createShaderModule	(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
		m_fragmentModule			= createShaderModule	(vk, device, m_context.getBinaryCollection().get("frag"), 0u);

		if (!m_params.groupParams->useDynamicRendering)
		{
			m_renderPass			= makeRenderPass		(vk, device, colorFormat);
			m_framebuffer			= makeFramebuffer		(vk, device, *m_renderPass, m_colorAttachment.get(),
															 static_cast<deUint32>(m_renderSize.x()),
															 static_cast<deUint32>(m_renderSize.y()));
		}

		m_pipelineLayout			= makePipelineLayout	(vk, device);

		generateDiscardRectangles(m_renderSize, m_params.numRectangles, m_rectangles);
		m_pipeline					= makeGraphicsPipeline	(vk, device, *m_pipelineLayout, *m_renderPass, *m_vertexModule, *m_fragmentModule, m_renderSize,
															 m_params.dynamicDiscardRectangles, discardRectangleMode, m_params.numRectangles,
															 m_rectangles.data(), m_params.scissorMode, rectScissor);
		m_cmdPool					= createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
		m_cmdBuffer					= allocateCommandBuffer	(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	}

	const VkClearValue			clearValue = makeClearValueColor(m_clearColor);
	const VkRect2D				renderArea
	{
		makeOffset2D(0, 0),
		makeExtent2D(m_renderSize.x(), m_renderSize.y()),
	};

	// Write command buffers and submit it

#ifndef CTS_USES_VULKANSC
	if (m_params.groupParams->useSecondaryCmdBuffer)
	{
		m_secCmdBuffer = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

		// record secondary command buffer
		if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(*m_secCmdBuffer, colorFormat, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginRendering(vk, *m_secCmdBuffer, *m_colorAttachment, renderArea, clearValue,
						   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR);
		}
		else
			beginSecondaryCmdBuffer(*m_secCmdBuffer, colorFormat);

		drawCommands(*m_secCmdBuffer, rectScissor);

		if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			vk.cmdEndRendering(*m_secCmdBuffer);

		endCommandBuffer(vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(vk, *m_cmdBuffer, 0u);
		preRenderCommands(*m_cmdBuffer);

		if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginRendering(vk, *m_cmdBuffer, *m_colorAttachment, renderArea, clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						   VK_ATTACHMENT_LOAD_OP_CLEAR, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			vk.cmdEndRendering(*m_cmdBuffer);
	}
	else if (m_params.groupParams->useDynamicRendering)
	{
		beginCommandBuffer(vk, *m_cmdBuffer);

		preRenderCommands(*m_cmdBuffer);
		beginRendering(vk, *m_cmdBuffer, *m_colorAttachment, renderArea, clearValue,
					   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR);
		drawCommands(*m_cmdBuffer, rectScissor);
		vk.cmdEndRendering(*m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_params.groupParams->useDynamicRendering)
	{
		const VkRenderPassBeginInfo renderPassBeginInfo
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_renderPass,									// VkRenderPass				renderPass;
			*m_framebuffer,									// VkFramebuffer			framebuffer;
			renderArea,										// VkRect2D					renderArea;
			1u,												// uint32_t					clearValueCount;
			&clearValue,									// const VkClearValue*		pClearValues;
		};

		beginCommandBuffer(vk, *m_cmdBuffer);

		preRenderCommands(*m_cmdBuffer);
		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		drawCommands(*m_cmdBuffer, rectScissor);
		vk.cmdEndRenderPass(*m_cmdBuffer);
	}

	copyImageToBuffer(vk, *m_cmdBuffer, *m_colorImage, m_colorBuffer->object(), tcu::IVec2(m_renderSize.x(), m_renderSize.y()), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorSubresourceRange.layerCount);
	VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

	// Verify results
	{
		const Allocation alloc = m_colorBuffer->getBoundMemory();
		invalidateAlloc(vk, device, alloc);

		const tcu::ConstPixelBufferAccess	resultImage		(mapVkFormat(colorFormat), m_renderSize.x(), m_renderSize.y(), 1u, alloc.getHostPtr());
		const tcu::TextureLevel				referenceImage	= generateReferenceImage(mapVkFormat(colorFormat), m_renderSize, m_params.testMode, m_clearColor,
																					 m_params.numRectangles, m_rectangles, m_params.scissorMode != TEST_SCISSOR_MODE_NONE, rectScissor);
		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceImage.getAccess(), resultImage, Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			TCU_FAIL("Rendered image is not correct");
	}
	return tcu::TestStatus::pass("OK");
}

void DiscardRectanglesTestInstance::preRenderCommands(vk::VkCommandBuffer cmdBuffer) const
{
	if (!m_params.groupParams->useDynamicRendering)
		return;

	const DeviceInterface& vk = m_context.getDeviceInterface();
	initialTransitionColor2DImage(vk, cmdBuffer, *m_colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
								  VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

void DiscardRectanglesTestInstance::drawCommands(vk::VkCommandBuffer cmdBuffer, const VkRect2D& rectScissor) const
{
	const DeviceInterface& vk = m_context.getDeviceInterface();
	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	{
		const VkBuffer vertexBuffer = m_vertexBuffer->object();
		const VkDeviceSize vertexBufferOffset = 0ull;
		vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
	}
	if (m_params.dynamicDiscardRectangles)
	{
		vk.cmdSetDiscardRectangleEXT(cmdBuffer, 0u, m_params.numRectangles, m_rectangles.data());
	}
	if (m_params.scissorMode == TEST_SCISSOR_MODE_DYNAMIC)
	{
		vk.cmdSetScissor(cmdBuffer, 0u, 1u, &rectScissor);
	}
	vk.cmdDraw(cmdBuffer, static_cast<deUint32>(m_vertices.size()), 1u, 0u, 0u);	// two triangles
}

#ifndef CTS_USES_VULKANSC
void DiscardRectanglesTestInstance::beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkFormat colorFormat, VkRenderingFlagsKHR renderingFlags) const
{
	VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,		// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		renderingFlags,															// VkRenderingFlagsKHR				flags;
		0u,																		// uint32_t							viewMask;
		1u,																		// uint32_t							colorAttachmentCount;
		&colorFormat,															// const VkFormat*					pColorAttachmentFormats;
		VK_FORMAT_UNDEFINED,													// VkFormat							depthAttachmentFormat;
		VK_FORMAT_UNDEFINED,													// VkFormat							stencilAttachmentFormat;
		VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits			rasterizationSamples;
	};
	const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);

	VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	const VkCommandBufferBeginInfo commandBufBeginParams
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,							// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		usageFlags,																// VkCommandBufferUsageFlags		flags;
		&bufferInheritanceInfo
	};

	const DeviceInterface& vk = m_context.getDeviceInterface();
	VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

class DiscardRectanglesTestCase : public TestCase
{
public:
								DiscardRectanglesTestCase	(tcu::TestContext &context,
															 const char *name,
															 const char *description,
															 TestParams params);
	virtual						~DiscardRectanglesTestCase	(void) {}

	virtual TestInstance*		createInstance				(Context& context)	const;
	virtual void				initPrograms				(SourceCollections& programCollection) const;
	virtual void				checkSupport				(Context& context) const;

private:
	const TestParams			m_params;
};

DiscardRectanglesTestCase::DiscardRectanglesTestCase	(tcu::TestContext &context,
														 const char *name,
														 const char *description,
														 TestParams params)
	: TestCase		(context, name, description)
	, m_params		(params)
{
}

void DiscardRectanglesTestCase::initPrograms(SourceCollections& programCollection) const
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
			<< "    vsColor     = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n"
			<< "}\n";
		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 vsColor;\n"
			<< "layout(location = 0) out highp vec4 fsColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    fsColor     = vsColor;\n"
			<< "}\n";
		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void DiscardRectanglesTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_discard_rectangles");
	if (m_params.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

TestInstance* DiscardRectanglesTestCase::createInstance (Context& context) const
{
	return new DiscardRectanglesTestInstance(context, m_params);
}

void createTests (tcu::TestCaseGroup* testGroup, const SharedGroupParams groupParams)
{
	tcu::TestContext&	testCtx											= testGroup->getTestContext();
	deUint32			numRect [NUM_RECT_TESTS]						= { 1, 2, 3, 4,  8, 16};
	std::string			modeName [TEST_MODE_COUNT]						= { "inclusive_", "exclusive_" };
	std::string			scissorName [TEST_SCISSOR_MODE_COUNT]			= { "", "scissor_", "dynamic_scissor_" };
	std::string			dynamicName [NUM_DYNAMIC_DISCARD_TYPE_TESTS]	= { "", "dynamic_discard_" };

	for (deUint32 dynamic = 0 ; dynamic < NUM_DYNAMIC_DISCARD_TYPE_TESTS; dynamic++)
	{
		for (deUint32 scissor = 0 ; scissor < TEST_SCISSOR_MODE_COUNT; scissor++)
		{
			for (deUint32 mode = 0; mode < TEST_MODE_COUNT; mode++)
			{
				for (deUint32 rect = 0; rect < NUM_RECT_TESTS; rect++)
				{
					std::ostringstream	name;
					TestParams			params
					{
						(TestMode)mode,						// TestMode					testMode;
						numRect[rect],						// deUint32					numRectangles;
						dynamic ? DE_TRUE : DE_FALSE,		// deBool					dynamicDiscardRectangles;
						(TestScissorMode)scissor,			// TestScissorMode			scissorMode;
						groupParams,						// const SharedGroupParams	groupParams;
					};

					name << dynamicName[dynamic] << scissorName[scissor] << modeName[mode] << "rect_" << numRect[rect];

					testGroup->addChild(new DiscardRectanglesTestCase(testCtx, name.str().c_str(), "", params));
				}
			}
		}
	}
}
}	// Anonymous

tcu::TestCaseGroup* createDiscardRectanglesTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	return createTestGroup(testCtx, "discard_rectangles", "Discard Rectangles tests", createTests, groupParams);
}

}	// Draw
}	//vkt
