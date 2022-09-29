/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief VK_EXT_depth_range_unrestricted Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDepthRangeUnrestrictedTests.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

enum testDynamicStaticMode
{
	TEST_MODE_VIEWPORT_DEPTH_BOUNDS_STATIC	= 0,
	TEST_MODE_VIEWPORT_DYNAMIC				= 1,
	TEST_MODE_DEPTH_BOUNDS_DYNAMIC			= 2,
	TEST_MODE_VIEWPORT_DEPTH_BOUNDS_DYNAMIC	= 3,
};

struct DepthRangeUnrestrictedParam
{
	VkFormat		depthFormat;
	VkBool32		testClearValueOnly;
	VkClearValue	depthBufferClearValue;
	VkBool32		depthClampEnable;
	float			wc;							// Component W of the vertices
	deUint32		viewportDepthBoundsMode;
	float			viewportMinDepth;
	float			viewportMaxDepth;
	VkBool32		depthBoundsTestEnable;
	float			minDepthBounds;
	float			maxDepthBounds;
	VkCompareOp		depthCompareOp;
};

// helper functions
std::string getFormatCaseName (vk::VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

std::string getCompareOpStringName (VkCompareOp compare)
{
	return de::toLower(de::toString(getCompareOpStr(compare)).substr(3));
}

const std::string generateTestName (struct DepthRangeUnrestrictedParam param)
{
	std::ostringstream result;

	result << getFormatCaseName(param.depthFormat).c_str();
	result << "_" << getCompareOpStringName(param.depthCompareOp).c_str();
	result << "_clear_value_" << (int) param.depthBufferClearValue.depthStencil.depth;

	if (param.depthClampEnable == VK_FALSE)
		result << "_wc_" << (int) param.wc;

	if (param.viewportDepthBoundsMode & TEST_MODE_VIEWPORT_DYNAMIC)
		result << "_dynamic";
	result << "_viewport_min_" << (int)param.viewportMinDepth << "_max_" << (int)param.viewportMaxDepth;

	if (param.depthBoundsTestEnable)
	{
		if (param.viewportDepthBoundsMode & TEST_MODE_DEPTH_BOUNDS_DYNAMIC)
			result << "_dynamic";
		result << "_boundstest_min" << (int)param.minDepthBounds << "_max_" << (int)param.maxDepthBounds;
	}

	return result.str();
}

const std::string generateTestDescription (struct DepthRangeUnrestrictedParam param)
{
	std::string result("Test unrestricted depth ranges on viewport");
	if (param.depthBoundsTestEnable)
		result += " , depth bounds test";
	return result;
}

deBool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, VkPhysicalDevice device, VkFormat format)
{
	VkFormatProperties formatProps;

	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u;
}

deBool isFloatingPointDepthFormat (VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return DE_TRUE;
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D16_UNORM:
		return DE_FALSE;
	default:
		DE_FATAL("No depth format");
	}
	return DE_FALSE;
}

deBool depthFormatHasStencilComponent (VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D16_UNORM_S8_UINT:
		return DE_TRUE;
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_D16_UNORM:
		return DE_FALSE;
	default:
		DE_FATAL("No depth format");
	}
	return DE_FALSE;
}

deBool compareDepthResult (VkCompareOp compare, float depth, float clearValue)
{
	deBool result = DE_FALSE;

	DE_ASSERT(compare <= VK_COMPARE_OP_ALWAYS && compare >= VK_COMPARE_OP_NEVER);

	switch (compare)
	{
	case VK_COMPARE_OP_ALWAYS:
		result = DE_TRUE;
		break;
	case VK_COMPARE_OP_NEVER:
		result = DE_FALSE;
		break;
	case VK_COMPARE_OP_EQUAL:
		result = depth == clearValue;
		break;
	case VK_COMPARE_OP_NOT_EQUAL:
		result = depth != clearValue;
		break;
	case VK_COMPARE_OP_GREATER:
		result = depth > clearValue;
		break;
	case VK_COMPARE_OP_GREATER_OR_EQUAL:
		result = depth >= clearValue;
		break;
	case VK_COMPARE_OP_LESS:
		result = depth < clearValue;
		break;
	case VK_COMPARE_OP_LESS_OR_EQUAL:
		result = depth <= clearValue;
		break;
	default:
		result = false;
		break;
	}
	return result;
}

static inline std::vector<Vertex4RGBA> createPoints (float wc)
{
	using tcu::Vec2;
	using tcu::Vec4;

	std::vector<Vertex4RGBA> vertices;

	// Vertices are in the following positions of the image:
	//
	// ----------------------------------
	// |                                |
	// |                                |
	// |      5                  6      |
	// |                                |
	// |          1         2           |
	// |                                |
	// |                                |
	// |          3         0           |
	// |                                |
	// |      7                  4      |
	// |                                |
	// |                                |
	// ----------------------------------
	//
	// Vertex    Depth    Color
	//   0        0.0     white
	//   1        0.25    magenta
	//   2       -2.0     yellow
	//   3        2.0     red
	//   4       -5.0     black
	//   5        5.0     cyan
	//   6       10.0     blue
	//   7      -10.0     green
	// Depth values are constant, they don't depend on wc.
	const Vertex4RGBA vertex0 =
	{
		Vec4(0.25f * wc, 0.25f * wc, 0.0f, wc),
		Vec4(1.0f, 1.0f, 1.0f, 1.0)
	};
	const Vertex4RGBA vertex1 =
	{
		Vec4(-0.25f * wc, -0.25f * wc, 0.25f, wc),
		Vec4(1.0f, 0.0f, 1.0f, 1.0)
	};
	const Vertex4RGBA vertex2 =
	{
		Vec4(0.25f * wc, -0.25f * wc, -2.0f, wc),
		Vec4(1.0f, 1.0f, 0.0f, 1.0)
	};
	const Vertex4RGBA vertex3 =
	{
		Vec4(-0.25f * wc, 0.25f * wc, 2.0f, wc),
		Vec4(1.0f, 0.0f, 0.0f, 1.0)
	};
	const Vertex4RGBA vertex4 =
	{
		Vec4(0.5f * wc, 0.5f * wc, -5.0f, wc),
		Vec4(0.0f, 0.0f, 0.0f, 1.0)
	};
	const Vertex4RGBA vertex5 =
	{
		Vec4(-0.5f * wc, -0.5f * wc, 5.0f, wc),
		Vec4(0.0f, 1.0f, 1.0f, 1.0)
	};
	const Vertex4RGBA vertex6 =
	{
		Vec4(0.5f * wc, -0.5f * wc, 10.0f, wc),
		Vec4(0.0f, 0.0f, 1.0f, 1.0)
	};

	const Vertex4RGBA vertex7 =
	{
		Vec4(-0.5f * wc, 0.5f * wc, -10.0f, wc),
		Vec4(0.0f, 1.0f, 0.0f, 1.0)
	};

	vertices.push_back(vertex0);
	vertices.push_back(vertex1);
	vertices.push_back(vertex2);
	vertices.push_back(vertex3);
	vertices.push_back(vertex4);
	vertices.push_back(vertex5);
	vertices.push_back(vertex6);
	vertices.push_back(vertex7);

	return vertices;
}

template <class Test>
vkt::TestCase* newTestCase (tcu::TestContext&					testContext,
							const DepthRangeUnrestrictedParam	testParam)
{
	return new Test(testContext,
					generateTestName(testParam).c_str(),
					generateTestDescription(testParam).c_str(),
					testParam);
}

Move<VkBuffer> createBufferAndBindMemory (Context& context, VkDeviceSize size, VkBufferUsageFlags usage, de::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&	vk				 = context.getDeviceInterface();
	const VkDevice			vkDevice		 = context.getDevice();
	const deUint32			queueFamilyIndex = context.getUniversalQueueFamilyIndex();

	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		size,										// VkDeviceSize			size;
		usage,										// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer> vertexBuffer = createBuffer(vk, vkDevice, &vertexBufferParams);

	*pAlloc = context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	return vertexBuffer;
}

Move<VkImage> createImage2DAndBindMemory (Context&							context,
										  VkFormat							format,
										  deUint32							width,
										  deUint32							height,
										  VkImageUsageFlags					usage,
										  VkSampleCountFlagBits				sampleCount,
										  de::details::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&	vk				 = context.getDeviceInterface();
	const VkDevice			vkDevice		 = context.getDevice();
	const deUint32			queueFamilyIndex = context.getUniversalQueueFamilyIndex();

	const VkImageCreateInfo colorImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType		sType;
		DE_NULL,																	// const void*			pNext;
		0u,																			// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,															// VkImageType			imageType;
		format,																		// VkFormat				format;
		{ width, height, 1u },														// VkExtent3D			extent;
		1u,																			// deUint32				mipLevels;
		1u,																			// deUint32				arraySize;
		sampleCount,																// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling		tiling;
		usage,																		// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode		sharingMode;
		1u,																			// deUint32				queueFamilyCount;
		&queueFamilyIndex,															// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout		initialLayout;
	};

	Move<VkImage> image = createImage(vk, vkDevice, &colorImageParams);

	*pAlloc = context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *image, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	return image;
}
Move<VkRenderPass> makeRenderPass (const DeviceInterface&				vk,
								   const VkDevice						device,
								   const VkFormat						colorFormat,
								   const VkFormat						depthStencilFormat,
								   const VkAttachmentLoadOp				loadOperationColor,
								   const VkAttachmentLoadOp				loadOperationDepthStencil)
{
	const bool								hasColor							= colorFormat != VK_FORMAT_UNDEFINED;
	const bool								hasDepthStencil						= depthStencilFormat != VK_FORMAT_UNDEFINED;
	const VkImageLayout						initialLayoutColor					= loadOperationColor == VK_ATTACHMENT_LOAD_OP_LOAD ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
	const VkImageLayout						initialLayoutDepthStencil			= loadOperationDepthStencil == VK_ATTACHMENT_LOAD_OP_LOAD ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;

	const VkAttachmentDescription			colorAttachmentDescription			=
	{
		(VkAttachmentDescriptionFlags)0,				// VkAttachmentDescriptionFlags    flags
		colorFormat,									// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits           samples
		loadOperationColor,								// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,					// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,				// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,				// VkAttachmentStoreOp             stencilStoreOp
		initialLayoutColor,								// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL		// VkImageLayout                   finalLayout
	};

	const VkAttachmentDescription			depthStencilAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags    flags
		depthStencilFormat,									// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits           samples
		loadOperationDepthStencil,							// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp             storeOp
		loadOperationDepthStencil,							// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp             stencilStoreOp
		initialLayoutDepthStencil,							// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout
	};

	std::vector<VkAttachmentDescription>	attachmentDescriptions;

	if (hasColor)
		attachmentDescriptions.push_back(colorAttachmentDescription);
	if (hasDepthStencil)
		attachmentDescriptions.push_back(depthStencilAttachmentDescription);

	const VkAttachmentReference				colorAttachmentRef					=
	{
		0u,											// deUint32         attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout    layout
	};

	const VkAttachmentReference				depthStencilAttachmentRef			=
	{
		hasColor ? 1u : 0u,									// deUint32         attachment
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout    layout
	};

	const VkSubpassDescription				subpassDescription					=
	{
		(VkSubpassDescriptionFlags)0,							// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,						// VkPipelineBindPoint             pipelineBindPoint
		0u,														// deUint32                        inputAttachmentCount
		DE_NULL,												// const VkAttachmentReference*    pInputAttachments
		hasColor ? 1u : 0u,										// deUint32                        colorAttachmentCount
		hasColor ? &colorAttachmentRef : DE_NULL,				// const VkAttachmentReference*    pColorAttachments
		DE_NULL,												// const VkAttachmentReference*    pResolveAttachments
		hasDepthStencil ? &depthStencilAttachmentRef : DE_NULL,	// const VkAttachmentReference*    pDepthStencilAttachment
		0u,														// deUint32                        preserveAttachmentCount
		DE_NULL													// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo			renderPassInfo						=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,									// VkStructureType                   sType
		DE_NULL,																	// const void*                       pNext
		(VkRenderPassCreateFlags)0,													// VkRenderPassCreateFlags           flags
		(deUint32)attachmentDescriptions.size(),									// deUint32                          attachmentCount
		attachmentDescriptions.size() > 0 ? &attachmentDescriptions[0] : DE_NULL,	// const VkAttachmentDescription*    pAttachments
		1u,																			// deUint32                          subpassCount
		&subpassDescription,														// const VkSubpassDescription*       pSubpasses
		0u,																			// deUint32                          dependencyCount
		DE_NULL																		// const VkSubpassDependency*        pDependencies
	};

	return createRenderPass(vk, device, &renderPassInfo, DE_NULL);
}

// Test Classes
class DepthRangeUnrestrictedTestInstance : public vkt::TestInstance
{
public:
								DepthRangeUnrestrictedTestInstance		(Context&				context,
																		 const DepthRangeUnrestrictedParam	param);
	virtual						~DepthRangeUnrestrictedTestInstance		(void);
	virtual tcu::TestStatus		iterate									(void);
protected:
			void				prepareRenderPass						(VkRenderPass renderPass, VkFramebuffer framebuffer, VkPipeline pipeline);
			void				prepareCommandBuffer					(void);
			Move<VkPipeline>	buildPipeline							(VkRenderPass renderpass);
			void				bindShaderStage							(VkShaderStageFlagBits					stage,
																		 const char*							sourceName,
																		 const char*							entryName);
			tcu::TestStatus		verifyTestResult						(void);
protected:
	const DepthRangeUnrestrictedParam	m_param;
	deBool								m_extensions;
	const tcu::UVec2					m_renderSize;
	const VkFormat						m_colorFormat;
	Move<VkPipelineLayout>				m_pipelineLayout;

	Move<VkImage>						m_depthImage;
	de::MovePtr<Allocation>				m_depthImageAlloc;
	de::MovePtr<Allocation>				m_colorImageAlloc;
	Move<VkImageView>					m_depthAttachmentView;
	VkImageMemoryBarrier				m_imageLayoutBarriers[2];

	Move<VkBuffer>						m_vertexBuffer;
	de::MovePtr<Allocation>				m_vertexBufferMemory;
	std::vector<Vertex4RGBA>			m_vertices;

	Move<VkRenderPass>					m_renderPass;
	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
	Move<VkImage>						m_colorImage;
	Move<VkImageView>					m_colorAttachmentView;
	Move<VkFramebuffer>					m_framebuffer;
	Move<VkPipeline>					m_pipeline;

	Move<VkShaderModule>				m_shaderModules[2];
	deUint32							m_shaderStageCount;
	VkPipelineShaderStageCreateInfo		m_shaderStageInfo[2];
};

void DepthRangeUnrestrictedTestInstance::bindShaderStage (VkShaderStageFlagBits	stage,
														  const char*			sourceName,
														  const char*			entryName)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create shader module
	deUint32*				code		= (deUint32*)m_context.getBinaryCollection().get(sourceName).getBinary();
	deUint32				codeSize	= (deUint32)m_context.getBinaryCollection().get(sourceName).getSize();

	const VkShaderModuleCreateInfo moduleCreateInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,				// VkStructureType				sType;
		DE_NULL,													// const void*					pNext;
		0u,															// VkShaderModuleCreateFlags	flags;
		codeSize,													// deUintptr					codeSize;
		code,														// const deUint32*				pCode;
	};

	m_shaderModules[m_shaderStageCount] = createShaderModule(vk, vkDevice, &moduleCreateInfo);

	// Prepare shader stage info
	m_shaderStageInfo[m_shaderStageCount].sType					= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	m_shaderStageInfo[m_shaderStageCount].pNext					= DE_NULL;
	m_shaderStageInfo[m_shaderStageCount].flags					= 0u;
	m_shaderStageInfo[m_shaderStageCount].stage					= stage;
	m_shaderStageInfo[m_shaderStageCount].module				= *m_shaderModules[m_shaderStageCount];
	m_shaderStageInfo[m_shaderStageCount].pName					= entryName;
	m_shaderStageInfo[m_shaderStageCount].pSpecializationInfo	= DE_NULL;

	m_shaderStageCount++;
}

Move<VkPipeline> DepthRangeUnrestrictedTestInstance::buildPipeline (VkRenderPass renderPass)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();

	// Create pipeline
	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,									// deUint32				binding;
		sizeof(Vertex4RGBA),				// deUint32				strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX,		// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
	{
		{
			0u,									// deUint32 location;
			0u,									// deUint32 binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat format;
			0u									// deUint32 offsetInBytes;
		},
		{
			1u,									// deUint32 location;
			0u,									// deUint32 binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat format;
			DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32 offsetInBytes;
		}
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		1u,																// deUint32									vertexBindingDescriptionCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		2u,																// deUint32									vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,								// const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,								// VkPrimitiveTopology						topology;
		VK_FALSE,														// VkBool32									primitiveRestartEnable;
	};

	const VkRect2D		scissor		= makeRect2D(m_renderSize);
	VkViewport			viewport	= makeViewport(m_renderSize);

	if (!(m_param.viewportDepthBoundsMode & TEST_MODE_VIEWPORT_DYNAMIC))
	{
		viewport.minDepth				= m_param.viewportMinDepth;
		viewport.maxDepth				= m_param.viewportMaxDepth;
	}

	const VkPipelineViewportStateCreateInfo viewportStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineViewportStateCreateFlags		flags;
		1u,																// deUint32									viewportCount;
		&viewport,														// const VkViewport*						pViewports;
		1u,																// deUint32									scissorCount;
		&scissor														// const VkRect2D*							pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo rasterStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineRasterizationStateCreateFlags	flags;
		m_param.depthClampEnable,										// VkBool32									depthClampEnable;
		VK_FALSE,														// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
	};

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE,														// VkBool32									blendEnable;
		VK_BLEND_FACTOR_ONE,											// VkBlendFactor							srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor							dstColorBlendFactor;
		VK_BLEND_OP_ADD,												// VkBlendOp								colorBlendOp;
		VK_BLEND_FACTOR_ONE,											// VkBlendFactor							srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor							dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,												// VkBlendOp								alphaBlendOp;
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT										// VkColorComponentFlags					colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// deUint32										attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
	};

	const VkPipelineMultisampleStateCreateInfo  multisampleStateParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags		flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits						rasterizationSamples;
		VK_FALSE,													// VkBool32										sampleShadingEnable;
		0.0f,														// float										minSampleShading;
		DE_NULL,													// const VkSampleMask*							pSampleMask;
		VK_FALSE,													// VkBool32										alphaToCoverageEnable;
		VK_FALSE,													// VkBool32										alphaToOneEnable;
	};

	float minDepthBounds = m_param.minDepthBounds;
	float maxDepthBounds = m_param.maxDepthBounds;

	if (m_param.viewportDepthBoundsMode & TEST_MODE_DEPTH_BOUNDS_DYNAMIC)
	{
		minDepthBounds = 0.0f;
		maxDepthBounds = 1.0f;
	}

	VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineDepthStencilStateCreateFlags		flags;
		VK_TRUE,													// VkBool32										depthTestEnable;
		VK_TRUE,													// VkBool32										depthWriteEnable;
		m_param.depthCompareOp,										// VkCompareOp									depthCompareOp;
		m_param.depthBoundsTestEnable,								// VkBool32										depthBoundsTestEnable;
		VK_FALSE,													// VkBool32										stencilTestEnable;
		// VkStencilOpState front;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		// VkStencilOpState back;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		minDepthBounds,												// float										minDepthBounds;
		maxDepthBounds,												// float										maxDepthBounds;
	};

	std::vector<VkDynamicState> dynamicStates;
	if (m_param.viewportDepthBoundsMode & TEST_MODE_VIEWPORT_DYNAMIC)
		dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	if (m_param.viewportDepthBoundsMode & TEST_MODE_DEPTH_BOUNDS_DYNAMIC)
		dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);

	const VkPipelineDynamicStateCreateInfo			dynamicStateParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType                      sType;
		DE_NULL,												// const void*                          pNext;
		(VkPipelineDynamicStateCreateFlags)0u,					// VkPipelineDynamicStateCreateFlags    flags;
		(deUint32)dynamicStates.size(),							// deUint32                             dynamicStateCount;
		(const VkDynamicState*)dynamicStates.data()				// const VkDynamicState*                pDynamicStates;
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType										sType;
		DE_NULL,											// const void*											pNext;
		0u,													// VkPipelineCreateFlags								flags;
		m_shaderStageCount,									// deUint32												stageCount;
		m_shaderStageInfo,									// const VkPipelineShaderStageCreateInfo*				pStages;
		&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*			pVertexInputState;
		&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*		pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*			pTessellationState;
		&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*				pViewportState;
		&rasterStateParams,									// const VkPipelineRasterizationStateCreateInfo*		pRasterState;
		&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*			pMultisampleState;
		&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*			pDepthStencilState;
		&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*			pColorBlendState;
		&dynamicStateParams,								// const VkPipelineDynamicStateCreateInfo*				pDynamicState;
		*m_pipelineLayout,									// VkPipelineLayout										layout;
		renderPass,											// VkRenderPass											renderPass;
		0u,													// deUint32												subpass;
		DE_NULL,											// VkPipeline											basePipelineHandle;
		0u,													// deInt32												basePipelineIndex;
	};

	return createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
}

void DepthRangeUnrestrictedTestInstance::prepareRenderPass (VkRenderPass renderPass, VkFramebuffer framebuffer, VkPipeline pipeline)
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();

	const VkClearValue attachmentClearValues[2] =
	{
		defaultClearValue(m_colorFormat),
		m_param.depthBufferClearValue,
	};

	beginRenderPass(vk, *m_cmdBuffer, renderPass, framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u, attachmentClearValues);

	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkDeviceSize offsets = 0u;
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);

	if (m_param.viewportDepthBoundsMode & TEST_MODE_VIEWPORT_DYNAMIC)
	{
		VkViewport	viewport	= makeViewport(m_renderSize);
		viewport.minDepth		= m_param.viewportMinDepth;
		viewport.maxDepth		= m_param.viewportMaxDepth;
		vk.cmdSetViewport(*m_cmdBuffer, 0u, 1u, &viewport);
	}

	if (m_param.viewportDepthBoundsMode & TEST_MODE_DEPTH_BOUNDS_DYNAMIC)
		vk.cmdSetDepthBounds(*m_cmdBuffer, m_param.minDepthBounds, m_param.maxDepthBounds);

	if (!m_vertices.empty() && !m_param.testClearValueOnly)
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1u, 0u, 0u);

	endRenderPass(vk, *m_cmdBuffer);
}

void DepthRangeUnrestrictedTestInstance::prepareCommandBuffer (void)
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
		0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers), m_imageLayoutBarriers);

	prepareRenderPass(*m_renderPass, *m_framebuffer, *m_pipeline);

	endCommandBuffer(vk, *m_cmdBuffer);
}

DepthRangeUnrestrictedTestInstance::DepthRangeUnrestrictedTestInstance	(Context&							context,
																		 const DepthRangeUnrestrictedParam	param)
	: TestInstance			(context)
	, m_param				(param)
	, m_extensions			(m_context.requireDeviceFunctionality("VK_EXT_depth_range_unrestricted"))
	, m_renderSize			(tcu::UVec2(32,32))
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_shaderStageCount	(0)
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();
	const VkDevice			vkDevice		 = m_context.getDevice();
	const deUint32			queueFamilyIndex = context.getUniversalQueueFamilyIndex();

	if (!isSupportedDepthStencilFormat(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), param.depthFormat))
	{
		throw tcu::NotSupportedError("Unsupported depth format");
	}

	VkPhysicalDeviceFeatures  features = m_context.getDeviceFeatures();
	if (param.depthClampEnable && features.depthClamp == DE_FALSE)
	{
		throw tcu::NotSupportedError("Unsupported feature: depthClamp");
	}

	if (param.depthBoundsTestEnable && features.depthBounds == DE_FALSE)
	{
		throw tcu::NotSupportedError("Unsupported feature: depthBounds");
	}

	// Create vertex buffer
	{
		m_vertexBuffer	= createBufferAndBindMemory(m_context, 1024u, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &m_vertexBufferMemory);
		m_vertices		= createPoints(m_param.wc);
		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferMemory->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(vk, vkDevice, *m_vertexBufferMemory);
	}

	// Create render pass
	m_renderPass = makeRenderPass(vk, vkDevice, m_colorFormat, m_param.depthFormat, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);

	const VkComponentMapping	ComponentMappingRGBA = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
	// Create color image
	{
		m_colorImage = createImage2DAndBindMemory(m_context,
												  m_colorFormat,
												  m_renderSize.x(),
												  m_renderSize.y(),
												  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
												  VK_SAMPLE_COUNT_1_BIT,
												  &m_colorImageAlloc);
	}

	// Create depth image
	{
		m_depthImage = createImage2DAndBindMemory(m_context,
												  m_param.depthFormat,
												  m_renderSize.x(),
												  m_renderSize.y(),
												  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
												  VK_SAMPLE_COUNT_1_BIT,
												  &m_depthImageAlloc);
	}

	deUint32 depthAspectBits = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (depthFormatHasStencilComponent(param.depthFormat))
		depthAspectBits |= VK_IMAGE_ASPECT_STENCIL_BIT;

	// Set up image layout transition barriers
	{
		VkImageMemoryBarrier colorImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkAccessFlags			srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					dstQueueFamilyIndex;
			*m_colorImage,										// VkImage					image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },		// VkImageSubresourceRange	subresourceRange;
		};

		m_imageLayoutBarriers[0] = colorImageBarrier;

		VkImageMemoryBarrier depthImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkAccessFlags			srcAccessMask;
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					dstQueueFamilyIndex;
			*m_depthImage,										// VkImage					image;
			{ depthAspectBits, 0u, 1u, 0u, 1u },				// VkImageSubresourceRange	subresourceRange;
		};

		m_imageLayoutBarriers[1] = depthImageBarrier;
	}
	// Create color attachment view
	{
		VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			ComponentMappingRGBA,							// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create depth attachment view
	{
		const VkImageViewCreateInfo depthAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_depthImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_param.depthFormat,							// VkFormat					format;
			ComponentMappingRGBA,							// VkComponentMapping		components;
			{ depthAspectBits, 0u, 1u, 0u, 1u },			// VkImageSubresourceRange	subresourceRange;
		};

		m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
	}

	// Create framebuffer
	{
		VkImageView attachmentBindInfos[2] =
		{
			*m_colorAttachmentView,
			*m_depthAttachmentView,
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			*m_renderPass,										// VkRenderPass					renderPass;
			2u,													// deUint32						attachmentCount;
			attachmentBindInfos,								// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u,													// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Bind shader stages
	{
		bindShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "vert", "main");
		bindShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "frag", "main");
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create pipeline
	m_pipeline = buildPipeline(*m_renderPass);

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

DepthRangeUnrestrictedTestInstance::~DepthRangeUnrestrictedTestInstance (void)
{
}

tcu::TestStatus DepthRangeUnrestrictedTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();

	prepareCommandBuffer();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
	return verifyTestResult();
}

tcu::TestStatus DepthRangeUnrestrictedTestInstance::verifyTestResult (void)
{
	deBool					compareOk			= DE_TRUE;
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	tcu::TestLog&			log					= m_context.getTestContext().getLog();
	Allocator&				allocator			= m_context.getDefaultAllocator();
	tcu::TextureLevel		refImage			(vk::mapVkFormat(m_colorFormat), 32, 32);
	float					clearValue			= m_param.depthBufferClearValue.depthStencil.depth;
	double					epsilon				= 1e-5;

	// For non-float depth formats, the value in the depth buffer is already clampled to the range [0, 1], which
	// includes the clear depth value.
	if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
		clearValue = de::min(de::max(clearValue, 0.0f), 1.0f);

	// Generate reference image
	{
		VkClearValue			clearColor		= defaultClearValue(m_colorFormat);
		tcu::Vec4				clearColorVec4  (clearColor.color.float32[0], clearColor.color.float32[1],
												 clearColor.color.float32[2], clearColor.color.float32[3]);

		tcu::clear(refImage.getAccess(), clearColorVec4);
		for (std::vector<Vertex4RGBA>::const_iterator vertex = m_vertices.begin(); vertex != m_vertices.end(); ++vertex)
		{
			if (m_param.depthClampEnable == VK_FALSE && (vertex->position.z() < 0.0f || vertex->position.z() > vertex->position.w()))
				continue;

			if (m_param.testClearValueOnly)
				continue;

			// Depth Clamp is enabled, then we clamp point depth to viewport's maxDepth and minDepth values, or [0.0f, 1.0f] is depth format is fixed-point.
			float scaling = ((vertex->position.z() / vertex->position.w()) * (m_param.viewportMaxDepth - m_param.viewportMinDepth)) + m_param.viewportMinDepth;
			float depth = de::min(de::max(scaling, m_param.viewportMinDepth), m_param.viewportMaxDepth);

			// For non-float depth formats, depth value is clampled to the range [0, 1].
			if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
				depth = de::min(de::max(depth, 0.0f), 1.0f);

			if (compareDepthResult(m_param.depthCompareOp, depth, clearValue))
			{
				deInt32 x = static_cast<deInt32>((((vertex->position.x() / vertex->position.w()) + 1.0f) / 2.0f) * static_cast<float>(m_renderSize.x() - 1));
				deInt32 y = static_cast<deInt32>((((vertex->position.y() / vertex->position.w()) + 1.0f) / 2.0f) * static_cast<float>(m_renderSize.y() - 1));
				refImage.getAccess().setPixel(vertex->color, x, y);
			}
		}
	}

	// Check the rendered image
	{
		de::MovePtr<tcu::TextureLevel> result = vkt::pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refImage.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
		if (!compareOk)
			return tcu::TestStatus::fail("Image mismatch");
	}

	// Check depth buffer contents
	{
		de::MovePtr<tcu::TextureLevel>	depthResult		= readDepthAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_depthImage, m_param.depthFormat, m_renderSize);

		if (m_param.testClearValueOnly) {
			compareOk = tcu::floatThresholdCompare(m_context.getTestContext().getLog(),
												   "DepthImagecompare",
												   "Depth image comparison",
												   tcu::Vec4(clearValue, 0.0f, 0.0f, 1.0f),
												   depthResult->getAccess(),
												   tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
												   tcu::COMPARE_LOG_RESULT);
			if (!compareOk)
				return tcu::TestStatus::fail("Depth buffer mismatch");
			else
				return tcu::TestStatus::pass("Result images matches references");
		}

		log << tcu::TestLog::Message;
		for (std::vector<Vertex4RGBA>::const_iterator vertex = m_vertices.begin(); vertex != m_vertices.end(); ++vertex)
		{
			deInt32 x = static_cast<deInt32>((((vertex->position.x() / vertex->position.w()) + 1.0f) / 2.0f) * static_cast<float>(m_renderSize.x() - 1));
			deInt32 y = static_cast<deInt32>((((vertex->position.y() / vertex->position.w()) + 1.0f) / 2.0f) * static_cast<float>(m_renderSize.y() - 1));
			tcu::Vec4 depth	= depthResult->getAccess().getPixel(x, y);

			// Check depth values are valid
			if (depth.y() != 0.0f || depth.z() != 0.0f || depth.w() != 1.0f)
			{
				log << tcu::TestLog::Message << "Invalid depth buffer values for pixel (" << x << ", " << y << ") = ("
					<< depth.x() << ", " << depth.y() << ", " << depth.z() << ", " << depth.w() << "." << tcu::TestLog::EndMessage;
				compareOk = DE_FALSE;
			}

			// Check the case where depth clamping is disabled.
			if (m_param.depthClampEnable == VK_FALSE)
			{
				if ((vertex->position.z() < 0.0f || vertex->position.z() > vertex->position.w()) &&
					fabs(clearValue - depth.x()) > epsilon)
				{
					log << tcu::TestLog::Message << "Error pixel (" << x << ", " << y << "). Depth value = " << depth
						<< ", expected " << clearValue << "." << tcu::TestLog::EndMessage;
					compareOk = DE_FALSE;
				}

				float expectedDepth = clearValue;

				if (vertex->position.z() <= vertex->position.w() && vertex->position.z() >= 0.0f)
				{
					// Assert we have a symmetric range around zero.
					DE_ASSERT(m_param.viewportMinDepth == (-m_param.viewportMaxDepth));

					// Calculate the expected depth value: first translate the value to from [0.0f, 1.0f] to [-1.0f, 1.0f].
					expectedDepth = 2 * (vertex->position.z() / vertex->position.w()) - 1.0f;
					// Now multiply by m_param.viewportMaxDepth to get the expected value.
					expectedDepth *= m_param.viewportMaxDepth;
				}

				// For non-float depth formats, depth value is clampled to the range [0, 1].
				if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
					expectedDepth = de::min(de::max(expectedDepth, 0.0f), 1.0f);

				expectedDepth = compareDepthResult(m_param.depthCompareOp, expectedDepth, clearValue) ? expectedDepth : clearValue;

				if (fabs(expectedDepth - depth.x()) > epsilon)
				{
					log << tcu::TestLog::Message << "Error pixel (" << x << ", " << y
						<< "). Depth value " << depth.x() << ", expected " << expectedDepth << ", error " << fabs(expectedDepth - depth.x()) << tcu::TestLog::EndMessage;
					compareOk = DE_FALSE;
				}

				continue;
			}

			// Depth Clamp is enabled, then we clamp point depth to viewport's maxDepth and minDepth values, or 0.0f and 1.0f is format is not float.
			float scaling = (vertex->position.z() / vertex->position.w()) * (m_param.viewportMaxDepth - m_param.viewportMinDepth) + m_param.viewportMinDepth;
			float expectedDepth = de::min(de::max(scaling, m_param.viewportMinDepth), m_param.viewportMaxDepth);

			// For non-float depth formats, depth value is clampled to the range [0, 1].
			if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
				expectedDepth = de::min(de::max(expectedDepth, 0.0f), 1.0f);

			expectedDepth = compareDepthResult(m_param.depthCompareOp, expectedDepth, clearValue) ? expectedDepth : clearValue;

			if (fabs(expectedDepth - depth.x()) > epsilon)
			{
				log << tcu::TestLog::Message << "Error pixel (" << x << ", " << y
					<< "). Depth value " << depth.x() << ", expected " << expectedDepth << ", error " << fabs(expectedDepth - depth.x()) << tcu::TestLog::EndMessage;
				compareOk = DE_FALSE;
			}
		}

		if (!compareOk)
			return tcu::TestStatus::fail("Depth buffer mismatch");
	}

	return tcu::TestStatus::pass("Result images matches references");
}

// Test Classes
class DepthBoundsRangeUnrestrictedTestInstance : public DepthRangeUnrestrictedTestInstance
{
public:
								DepthBoundsRangeUnrestrictedTestInstance		(Context&				context,
																				 const DepthRangeUnrestrictedParam	param);
	virtual						~DepthBoundsRangeUnrestrictedTestInstance		(void);
	virtual tcu::TestStatus		iterate											(void);

protected:
			tcu::TestStatus		verifyTestResult								(bool firstDraw);
			void				prepareCommandBuffer							(bool firstDraw);

protected:
			Move<VkRenderPass>					m_renderPassSecondDraw;
			Move<VkFramebuffer>					m_framebufferSecondDraw;
			Move<VkPipeline>					m_pipelineSecondDraw;
			std::vector<bool>					m_vertexWasRendered;

};

DepthBoundsRangeUnrestrictedTestInstance::DepthBoundsRangeUnrestrictedTestInstance	(Context&							context,
																					 const DepthRangeUnrestrictedParam	param)
	: DepthRangeUnrestrictedTestInstance(context, param)
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();
	const VkDevice			vkDevice		 = m_context.getDevice();

	// Create render pass for second draw, we keep the first draw's contents of the depth buffer.
	m_renderPassSecondDraw = makeRenderPass(vk, vkDevice, m_colorFormat, m_param.depthFormat, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_LOAD);

	// Create framebuffer for second draw.
	{
		VkImageView attachmentBindInfos[2] =
		{
			*m_colorAttachmentView,
			*m_depthAttachmentView,
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			*m_renderPassSecondDraw,							// VkRenderPass					renderPass;
			2u,													// deUint32						attachmentCount;
			attachmentBindInfos,								// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u,													// deUint32						layers;
		};

		m_framebufferSecondDraw = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

		// Create pipeline
	m_pipelineSecondDraw = buildPipeline(*m_renderPassSecondDraw);
}

DepthBoundsRangeUnrestrictedTestInstance::~DepthBoundsRangeUnrestrictedTestInstance (void)
{
}

tcu::TestStatus DepthBoundsRangeUnrestrictedTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();

	// This test will draw the same scene two times.
	// First one will render the points depending on if the pass the depth test and if clear depth value passes the
	// depthBounds test.
	//
	// The second one, will render the same scene but the the point positions will have depth buffer values from
	// the first draw. If they pass the depth test, the depthBounds test will check the content of the depth buffer,
	// which is most cases, will make that the second result differs from the first one, hence the need to split
	// the verification in two steps.
	prepareCommandBuffer(true);
	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
	tcu::TestStatus status = verifyTestResult(true);
	if (status.getCode() != QP_TEST_RESULT_PASS)
		return status;

	prepareCommandBuffer(false);
	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
	return verifyTestResult(false);
}

void DepthBoundsRangeUnrestrictedTestInstance::prepareCommandBuffer (bool firstDraw)
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();

	if (!firstDraw)
	{
		vk.resetCommandBuffer(*m_cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		// Color image layout changed after verifying the first draw call, restore it.
		m_imageLayoutBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		m_imageLayoutBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		// Depth image layout changed after verifying the first draw call, restore it.
		m_imageLayoutBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		m_imageLayoutBarriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*m_cmdBuffer, (firstDraw ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
		0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers), m_imageLayoutBarriers);

	prepareRenderPass((firstDraw ? *m_renderPass : *m_renderPassSecondDraw),
					  (firstDraw ? *m_framebuffer : *m_framebufferSecondDraw),
					  (firstDraw ? *m_pipeline : *m_pipelineSecondDraw));

	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus DepthBoundsRangeUnrestrictedTestInstance::verifyTestResult (bool firstDraw)
{
	deBool					compareOk			= DE_TRUE;
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	tcu::TestLog&			log					= m_context.getTestContext().getLog();
	Allocator&				allocator			= m_context.getDefaultAllocator();
	tcu::TextureLevel		refImage			(vk::mapVkFormat(m_colorFormat), 32, 32);
	float					clearValue			= m_param.depthBufferClearValue.depthStencil.depth;
	double					epsilon				= 1e-5;

	// For non-float depth formats, depth value is clampled to the range [0, 1].
	if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
		clearValue = de::min(de::max(clearValue, 0.0f), 1.0f);

	// Generate reference image
	{
		VkClearValue			clearColor		= defaultClearValue(m_colorFormat);
		tcu::Vec4				clearColorVec4  (clearColor.color.float32[0], clearColor.color.float32[1],
												 clearColor.color.float32[2], clearColor.color.float32[3]);
		tcu::clear(refImage.getAccess(), clearColorVec4);
		for (std::vector<Vertex4RGBA>::const_iterator vertex = m_vertices.begin(); vertex != m_vertices.end(); ++vertex)
		{
			// Depth Clamp is enabled, then we clamp point depth to viewport's maxDepth and minDepth values and later check if it is inside depthBounds volume.
			float scaling = ((vertex->position.z() / vertex->position.w()) * (m_param.viewportMaxDepth - m_param.viewportMinDepth)) + m_param.viewportMinDepth;
			float depth = de::min(de::max(scaling, m_param.viewportMinDepth), m_param.viewportMaxDepth);
			if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
				depth = de::min(de::max(depth, 0.0f), 1.0f);

			auto i = vertex - m_vertices.begin();

			// Depending if the first draw call succeed, we need to know if the second draw call will render the points because the depth buffer content
			// will determine if it passes the depth test and the depth bounds test.
			bool firstDrawHasPassedDepthBoundsTest = !firstDraw && m_vertexWasRendered[i];
			float depthBufferValue = firstDrawHasPassedDepthBoundsTest ? depth : clearValue;

			// For non-float depth formats, depth value is clampled to the range [0, 1].
			if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
				depthBufferValue = de::min(de::max(depthBufferValue, 0.0f), 1.0f);

			// Check that the point passes the depth test and the depth bounds test.
			if (compareDepthResult(m_param.depthCompareOp, depth, depthBufferValue) &&
				depthBufferValue >= m_param.minDepthBounds && depthBufferValue <= m_param.maxDepthBounds)
			{
				deInt32 x = static_cast<deInt32>((((vertex->position.x() / vertex->position.w()) + 1.0f) / 2.0f) * static_cast<float>(m_renderSize.x() - 1));
				deInt32 y = static_cast<deInt32>((((vertex->position.y() / vertex->position.w()) + 1.0f) / 2.0f) * static_cast<float>(m_renderSize.y() - 1));
				refImage.getAccess().setPixel(vertex->color, x, y);
				if (firstDraw)
					m_vertexWasRendered.push_back(true);
				continue;
			}

			if (firstDraw)
				m_vertexWasRendered.push_back(false);
		}
	}

	// Check the rendered image
	{
		de::MovePtr<tcu::TextureLevel> result = vkt::pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize);
		std::string description = "Image comparison draw ";
		description += (firstDraw ? "1" : "2");

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  description.c_str(),
															  refImage.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
		if (!compareOk)
			return tcu::TestStatus::fail("Image mismatch");
	}

	// Check depth buffer contents
	{
		de::MovePtr<tcu::TextureLevel>	depthResult		= readDepthAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_depthImage, m_param.depthFormat, m_renderSize);

		log << tcu::TestLog::Message;
		for (std::vector<Vertex4RGBA>::const_iterator vertex = m_vertices.begin(); vertex != m_vertices.end(); ++vertex)
		{
			deInt32 x = static_cast<deInt32>((((vertex->position.x() / vertex->position.w()) + 1.0f) / 2.0f) * static_cast<float>(m_renderSize.x() - 1));
			deInt32 y = static_cast<deInt32>((((vertex->position.y() / vertex->position.w()) + 1.0f) / 2.0f) * static_cast<float>(m_renderSize.y() - 1));
			tcu::Vec4 depth	= depthResult->getAccess().getPixel(x, y);

			// Check depth values are valid
			if (depth.y() != 0.0f || depth.z() != 0.0f || depth.w() != 1.0f)
			{
				log << tcu::TestLog::Message << "Draw " << (firstDraw ? "1" : "2") << ": Invalid depth buffer values for pixel (" << x << ", " << y << ") = ("
					<< depth.x() << ", " << depth.y() << ", " << depth.z() << ", " << depth.w() << "." << tcu::TestLog::EndMessage;
				compareOk = DE_FALSE;
			}

			// Depth Clamp is enabled, so we clamp point depth to viewport's maxDepth and minDepth values, or 0.0f and 1.0f is format is not float.
			float scaling = (vertex->position.z() / vertex->position.w()) * (m_param.viewportMaxDepth - m_param.viewportMinDepth) + m_param.viewportMinDepth;
			float expectedDepth = de::min(de::max(scaling, m_param.viewportMinDepth), m_param.viewportMaxDepth);

			auto i = vertex - m_vertices.begin();

			// Depending if the first draw call succeed, we need to know if the second draw call will render the points because the depth buffer content
			// will determine if it passes the depth test and the depth bounds test.
			bool firstDrawHasPassedDepthBoundsTest = !firstDraw && m_vertexWasRendered[i];

			// If we are in the first draw call, the depth buffer content is clearValue. If we are in the second draw call, it is going to be depth.x() if the first
			// succeeded.
			float depthBufferValue = firstDrawHasPassedDepthBoundsTest ? depth.x() : clearValue;

			// For non-float depth formats, depth value is clampled to the range [0, 1].
			if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
				depthBufferValue = de::min(de::max(depthBufferValue, 0.0f), 1.0f);

			// Calculate the expectd depth depending on the depth test and the depth bounds test results.
			expectedDepth =
				(compareDepthResult(m_param.depthCompareOp, expectedDepth, depthBufferValue) && depthBufferValue <= m_param.maxDepthBounds && depthBufferValue >= m_param.minDepthBounds)
				? expectedDepth : depthBufferValue;

			// For non-float depth formats, depth value is clampled to the range [0, 1].
			if (isFloatingPointDepthFormat(m_param.depthFormat) == VK_FALSE)
				expectedDepth = de::min(de::max(expectedDepth, 0.0f), 1.0f);

			if (fabs(expectedDepth - depth.x()) > epsilon)
			{
				log << tcu::TestLog::Message << "Draw " << (firstDraw ? "1" : "2") << ": Error pixel (" << x << ", " << y
					<< "). Depth value " << depth.x() << ", expected " << expectedDepth << ", error " << fabs(expectedDepth - depth.x()) << tcu::TestLog::EndMessage;
				compareOk = DE_FALSE;
			}
		}

		if (!compareOk)
			return tcu::TestStatus::fail("Depth buffer mismatch");
	}

	return tcu::TestStatus::pass("Result images matches references");
}

class DepthRangeUnrestrictedTest : public vkt::TestCase
{
public:
							DepthRangeUnrestrictedTest			(tcu::TestContext&					testContext,
																 const std::string&					name,
																 const std::string&					description,
																 const DepthRangeUnrestrictedParam	param)
								: vkt::TestCase (testContext, name, description)
								, m_param		(param)
								{ }
	virtual					~DepthRangeUnrestrictedTest	(void) { }
	virtual void			initPrograms		(SourceCollections&	programCollection) const;
	virtual TestInstance*	createInstance		(Context&				context) const;

protected:
		const DepthRangeUnrestrictedParam       m_param;
};

void DepthRangeUnrestrictedTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(
				"#version 310 es\n"
				"layout(location = 0) in vec4 position;\n"
				"layout(location = 1) in vec4 color;\n"
				"layout(location = 0) out highp vec4 vtxColor;\n"
				"void main (void)\n"
				"{\n"
				"  gl_Position = position;\n"
				"  gl_PointSize = 1.0f;\n"
				"  vtxColor = color;\n"

				"}\n");


	programCollection.glslSources.add("frag") << glu::FragmentSource(
				"#version 310 es\n"
				"layout(location = 0) in highp vec4 vtxColor;\n"
				"layout(location = 0) out highp vec4 fragColor;\n"
				"void main (void)\n"
				"{\n"
				"  fragColor = vtxColor;\n"
				"}\n");

}

TestInstance* DepthRangeUnrestrictedTest::createInstance (Context& context) const
{
	if (m_param.depthBoundsTestEnable)
		return new DepthBoundsRangeUnrestrictedTestInstance(context, m_param);
	return new DepthRangeUnrestrictedTestInstance(context, m_param);
}
} // anonymous

tcu::TestCaseGroup* createDepthRangeUnrestrictedTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> depthTests (new tcu::TestCaseGroup(testCtx, "depth_range_unrestricted", "VK_EXT_depth_range_unrestricted tests"));
	const VkFormat depthFormats[]	=
	{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM,
	};

	const VkCompareOp compareOps[]	=
	{
		VK_COMPARE_OP_GREATER,
		VK_COMPARE_OP_GREATER_OR_EQUAL,
		VK_COMPARE_OP_LESS,
		VK_COMPARE_OP_LESS_OR_EQUAL,
	};

	float viewportValues[]			= {2.0f, 6.0f, 12.0f};
	float depthBoundsValues[]		= {2.0f, 4.0f, 8.0f};
	float wcValues[]				= {2.0f, 6.0f, 12.0f};
	float clearValues[]				= {2.0f, -3.0f, 6.0f, -7.0f};

	// Depth clear values outside range [0.0f, 1.0f].
	{
		de::MovePtr<tcu::TestCaseGroup> depthClearValueTests (new tcu::TestCaseGroup(testCtx, "clear_value", "Depth Clear value unrestricted"));
		DepthRangeUnrestrictedParam testParams;
		testParams.testClearValueOnly			= VK_TRUE;
		testParams.depthClampEnable				= VK_FALSE;
		testParams.wc							= 1.0f;
		testParams.viewportMinDepth				= 0.0f;
		testParams.viewportMaxDepth				= 1.0f;
		testParams.minDepthBounds				= 0.0f;
		testParams.maxDepthBounds				= 1.0f;
		testParams.depthBoundsTestEnable		= VK_FALSE;
		testParams.depthCompareOp				= VK_COMPARE_OP_LESS_OR_EQUAL;
		testParams.viewportDepthBoundsMode		= TEST_MODE_VIEWPORT_DEPTH_BOUNDS_STATIC;

		for (int format = 0; format < DE_LENGTH_OF_ARRAY(depthFormats); ++format)
		{
			testParams.depthFormat				= depthFormats[format];
			testParams.depthBufferClearValue	= defaultClearValue(depthFormats[format]);
			for (int val = 0; val < DE_LENGTH_OF_ARRAY(clearValues); val++)
			{
				testParams.depthBufferClearValue.depthStencil.depth	= clearValues[val];
				depthClearValueTests->addChild(newTestCase<DepthRangeUnrestrictedTest>(testCtx, testParams));
			}
		}
		depthTests->addChild(depthClearValueTests.release());
	}

	// Viewport's depth unrestricted range
	{
		de::MovePtr<tcu::TestCaseGroup> viewportTests (new tcu::TestCaseGroup(testCtx, "viewport", "Viewport depth unrestricted range"));
		DepthRangeUnrestrictedParam testParams;
		testParams.testClearValueOnly		= VK_FALSE;
		testParams.wc						= 1.0f;
		testParams.depthClampEnable			= VK_TRUE;
		testParams.minDepthBounds			= 0.0f;
		testParams.maxDepthBounds			= 1.0f;
		testParams.depthBoundsTestEnable	= VK_FALSE;

		for (int format = 0; format < DE_LENGTH_OF_ARRAY(depthFormats); ++format)
		{
			testParams.depthFormat				= depthFormats[format];
			testParams.depthBufferClearValue	= defaultClearValue(testParams.depthFormat);
			for (int compareOp = 0; compareOp < DE_LENGTH_OF_ARRAY(compareOps); compareOp++)
			{
				testParams.depthCompareOp		= compareOps[compareOp];
				for (int clearValue = 0; clearValue < DE_LENGTH_OF_ARRAY(clearValues); clearValue++)
				{
					testParams.depthBufferClearValue.depthStencil.depth		= clearValues[clearValue];
					for (int viewportValue = 0; viewportValue < DE_LENGTH_OF_ARRAY(viewportValues); viewportValue++)
					{
						testParams.viewportMinDepth			= -viewportValues[viewportValue];
						testParams.viewportMaxDepth			= viewportValues[viewportValue];
						testParams.viewportDepthBoundsMode	= TEST_MODE_VIEWPORT_DEPTH_BOUNDS_STATIC;
						viewportTests->addChild(newTestCase<DepthRangeUnrestrictedTest>(testCtx, testParams));
						testParams.viewportDepthBoundsMode	= TEST_MODE_VIEWPORT_DYNAMIC;
						viewportTests->addChild(newTestCase<DepthRangeUnrestrictedTest>(testCtx, testParams));
					}
				}
			}
		}

		depthTests->addChild(viewportTests.release());
	}

	// DepthBounds's depth unrestricted range
	{
		de::MovePtr<tcu::TestCaseGroup> depthBoundsTests (new tcu::TestCaseGroup(testCtx, "depthbounds", "Depthbounds unrestricted range"));
		DepthRangeUnrestrictedParam testParams;
		testParams.testClearValueOnly							= VK_FALSE;
		testParams.wc											= 1.0f;
		testParams.depthClampEnable								= VK_TRUE;
		testParams.depthBoundsTestEnable						= VK_TRUE;

		for (int format = 0; format < DE_LENGTH_OF_ARRAY(depthFormats); ++format)
		{
			testParams.depthFormat				= depthFormats[format];
			testParams.depthBufferClearValue	= defaultClearValue(testParams.depthFormat);
			for (int compareOp = 0; compareOp < DE_LENGTH_OF_ARRAY(compareOps); compareOp++)
			{
				testParams.depthCompareOp		= compareOps[compareOp];
				for (int clearValue = 0; clearValue < DE_LENGTH_OF_ARRAY(clearValues); clearValue++)
				{
					testParams.depthBufferClearValue.depthStencil.depth		= clearValues[clearValue];
					for (int viewportValue = 0; viewportValue < DE_LENGTH_OF_ARRAY(viewportValues); viewportValue++)
					{
						testParams.viewportMinDepth				= -viewportValues[viewportValue];
						testParams.viewportMaxDepth				= viewportValues[viewportValue];
						for (int depthValue = 0; depthValue < DE_LENGTH_OF_ARRAY(depthBoundsValues); depthValue++)
						{
							testParams.minDepthBounds			= -depthBoundsValues[depthValue];
							testParams.maxDepthBounds			= depthBoundsValues[depthValue];

							testParams.viewportDepthBoundsMode	= TEST_MODE_VIEWPORT_DEPTH_BOUNDS_STATIC;
							depthBoundsTests->addChild(newTestCase<DepthRangeUnrestrictedTest>(testCtx, testParams));
							testParams.viewportDepthBoundsMode	= TEST_MODE_DEPTH_BOUNDS_DYNAMIC;
							depthBoundsTests->addChild(newTestCase<DepthRangeUnrestrictedTest>(testCtx, testParams));
							testParams.viewportDepthBoundsMode  = TEST_MODE_VIEWPORT_DEPTH_BOUNDS_DYNAMIC;
							depthBoundsTests->addChild(newTestCase<DepthRangeUnrestrictedTest>(testCtx, testParams));
						}
					}
				}
			}
		}

		depthTests->addChild(depthBoundsTests.release());
	}

	// Depth clamping disabled
	{
		de::MovePtr<tcu::TestCaseGroup> noDepthClampingTests (new tcu::TestCaseGroup(testCtx, "depthclampingdisabled", "Depth clamping disabled tests"));
		DepthRangeUnrestrictedParam testParams;
		testParams.testClearValueOnly			= VK_FALSE;
		testParams.depthClampEnable				= VK_FALSE;
		testParams.minDepthBounds				= 0.0f;
		testParams.maxDepthBounds				= 1.0f;
		testParams.depthBoundsTestEnable		= VK_FALSE;
		testParams.viewportDepthBoundsMode		= TEST_MODE_VIEWPORT_DEPTH_BOUNDS_STATIC;

		for (int format = 0; format < DE_LENGTH_OF_ARRAY(depthFormats); ++format)
		{
			testParams.depthFormat					= depthFormats[format];
			testParams.depthBufferClearValue		= defaultClearValue(testParams.depthFormat);
			for (int compareOp = 0; compareOp < DE_LENGTH_OF_ARRAY(compareOps); compareOp++)
			{
				testParams.depthCompareOp			= compareOps[compareOp];
				for (int clearValue = 0; clearValue < DE_LENGTH_OF_ARRAY(clearValues); clearValue++)
				{
					testParams.depthBufferClearValue.depthStencil.depth	= clearValues[clearValue];
					for (int viewportValue = 0; viewportValue < DE_LENGTH_OF_ARRAY(viewportValues); viewportValue++)
					{
						testParams.viewportMinDepth	= -viewportValues[viewportValue];
						testParams.viewportMaxDepth	= viewportValues[viewportValue];
						for (int wc = 0; wc < DE_LENGTH_OF_ARRAY(wcValues); wc++)
						{
							testParams.wc	= wcValues[wc];
							noDepthClampingTests->addChild(newTestCase<DepthRangeUnrestrictedTest>(testCtx, testParams));
						}
					}
				}
			}
		}

		depthTests->addChild(noDepthClampingTests.release());
	}

	return depthTests.release();
}

} // pipeline

} // vkt
