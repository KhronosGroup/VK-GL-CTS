/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 The Android Open Source Project
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
 * \brief Use of gl_ViewportIndex in Vertex and Tessellation Shaders
 *        (part of VK_EXT_ShaderViewportIndexLayer)
 *//*--------------------------------------------------------------------*/

#include "vktDrawShaderViewportIndexTests.hpp"

#include "vktDrawBaseClass.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deMath.h"

#include <vector>
#include <memory>

namespace vkt
{
namespace Draw
{
using namespace vk;
using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;
using tcu::Vec4;
using tcu::Vec2;
using tcu::UVec2;
using tcu::UVec4;

namespace
{

enum Constants
{
	MIN_MAX_VIEWPORTS = 16,		//!< Minimum number of viewports for an implementation supporting multiViewport.
};

struct TestParams
{
	int		numViewports;
	bool	writeFromVertex;
	bool	useDynamicRendering;
	bool	useTessellationShader;
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

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&		vk,
									   const VkDevice				device,
									   const VkPipelineLayout		pipelineLayout,
									   const VkRenderPass			renderPass,
									   const VkShaderModule			vertexModule,
									   const VkShaderModule			tessellationControlModule,
									   const VkShaderModule			tessellationEvaluationModule,
									   const VkShaderModule			fragmentModule,
									   const UVec2					renderSize,
									   const int					numViewports,
									   const std::vector<UVec4>		cells)
{
	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,								// uint32_t				binding;
		sizeof(PositionColorVertex),	// uint32_t				stride;
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,									// uint32_t			location;
			0u,									// uint32_t			binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat			format;
			0u,									// uint32_t			offset;
		},
		{
			1u,									// uint32_t			location;
			0u,									// uint32_t			binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat			format;
			sizeof(Vec4),						// uint32_t			offset;
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags       flags;
		1u,																// uint32_t                                    vertexBindingDescriptionCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),			// uint32_t                                    vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,								// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const bool useTessellationShaders = (tessellationControlModule != DE_NULL) && (tessellationEvaluationModule != DE_NULL);

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,										// VkStructureType                             sType;
		DE_NULL,																							// const void*                                 pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,															// VkPipelineInputAssemblyStateCreateFlags     flags;
		useTessellationShaders ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// VkPrimitiveTopology                         topology;
		VK_FALSE,																							// VkBool32                                    primitiveRestartEnable;
	};

	DE_ASSERT(numViewports == static_cast<int>(cells.size()));

	std::vector<VkViewport> viewports;
	viewports.reserve(numViewports);

	std::vector<VkRect2D> rectScissors;
	rectScissors.reserve(numViewports);

	for (std::vector<UVec4>::const_iterator it = cells.begin(); it != cells.end(); ++it) {
		const VkViewport viewport = makeViewport(float(it->x()), float(it->y()), float(it->z()), float(it->w()), 0.0f, 1.0f);
		viewports.push_back(viewport);
		const VkRect2D rect = makeRect2D(renderSize);
		rectScissors.push_back(rect);
	}

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags          flags;
		static_cast<deUint32>(numViewports),							// uint32_t                                    viewportCount;
		&viewports[0],													// const VkViewport*                           pViewports;
		static_cast<deUint32>(numViewports),							// uint32_t                                    scissorCount;
		&rectScissors[0],												// const VkRect2D*                             pScissors;
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
		VK_STENCIL_OP_KEEP,				// stencil fail
		VK_STENCIL_OP_KEEP,				// depth & stencil pass
		VK_STENCIL_OP_KEEP,				// depth only fail
		VK_COMPARE_OP_ALWAYS,			// compare op
		0u,								// compare mask
		0u,								// write mask
		0u);							// reference

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,						// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_FALSE,														// VkBool32									depthTestEnable;
		VK_FALSE,														// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS,												// VkCompareOp								depthCompareOp;
		VK_FALSE,														// VkBool32									depthBoundsTestEnable;
		VK_FALSE,														// VkBool32									stencilTestEnable;
		stencilOpState,													// VkStencilOpState							front;
		stencilOpState,													// VkStencilOpState							back;
		0.0f,															// float									minDepthBounds;
		1.0f,															// float									maxDepthBounds;
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
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,					// VkShaderStageFlagBits				stage;
			tessellationControlModule,									// VkShaderModule						module;
			"main",														// const char*							pName;
			DE_NULL,													// const VkSpecializationInfo*			pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,				// VkShaderStageFlagBits				stage;
			tessellationEvaluationModule,								// VkShaderModule						module;
			"main",														// const char*							pName;
			DE_NULL,													// const VkSpecializationInfo*			pSpecializationInfo;
		},
	};

	const VkPipelineTessellationStateCreateInfo pipelineTessellationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineTessellationStateCreateFlags)0,						// VkPipelineTessellationStateCreateFlags	flags;
		3,																// uint32_t									patchControlPoints;
	};

	VkGraphicsPipelineCreateInfo graphicsPipelineInfo
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,					// VkStructureType									sType;
		DE_NULL,															// const void*										pNext;
		(VkPipelineCreateFlags)0,											// VkPipelineCreateFlags							flags;
		useTessellationShaders ? deUint32(4) : deUint32(2),					// deUint32											stageCount;
		pShaderStages,														// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,												// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,									// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		useTessellationShaders ? &pipelineTessellationStateInfo : DE_NULL,	// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&pipelineViewportStateInfo,											// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,									// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&pipelineMultisampleStateInfo,										// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&pipelineDepthStencilStateInfo,										// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&pipelineColorBlendStateInfo,										// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,															// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,														// VkPipelineLayout									layout;
		renderPass,															// VkRenderPass										renderPass;
		0u,																	// deUint32											subpass;
		DE_NULL,															// VkPipeline										basePipelineHandle;
		0,																	// deInt32											basePipelineIndex;
	};

	VkFormat colorAttachmentFormat = VK_FORMAT_R8G8B8A8_UNORM;
	VkPipelineRenderingCreateInfoKHR renderingCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		DE_NULL,
		0u,
		1u,
		&colorAttachmentFormat,
		VK_FORMAT_UNDEFINED,
		VK_FORMAT_UNDEFINED
	};

	// when pipeline is created without render pass we are using dynamic rendering
	if (renderPass == DE_NULL)
		graphicsPipelineInfo.pNext = &renderingCreateInfo;

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

std::vector<UVec4> generateGrid (const int numCells, const UVec2& renderSize)
{
	const int numCols		= deCeilFloatToInt32(deFloatSqrt(static_cast<float>(numCells)));
	const int numRows		= deCeilFloatToInt32(static_cast<float>(numCells) / static_cast<float>(numCols));
	const int rectWidth		= renderSize.x() / numCols;
	const int rectHeight	= renderSize.y() / numRows;

	std::vector<UVec4> cells;
	cells.reserve(numCells);

	int x = 0;
	int y = 0;

	for (int cellNdx = 0; cellNdx < numCells; ++cellNdx)
	{
		const bool nextRow = (cellNdx != 0) && (cellNdx % numCols == 0);
		if (nextRow)
		{
			x  = 0;
			y += rectHeight;
		}

		cells.push_back(UVec4(x, y, rectWidth, rectHeight));

		x += rectWidth;
	}

	return cells;
}

std::vector<Vec4> generateColors (const int numColors)
{
	const Vec4 colors[] =
	{
		Vec4(0.18f, 0.42f, 0.17f, 1.0f),
		Vec4(0.29f, 0.62f, 0.28f, 1.0f),
		Vec4(0.59f, 0.84f, 0.44f, 1.0f),
		Vec4(0.96f, 0.95f, 0.72f, 1.0f),
		Vec4(0.94f, 0.55f, 0.39f, 1.0f),
		Vec4(0.82f, 0.19f, 0.12f, 1.0f),
		Vec4(0.46f, 0.15f, 0.26f, 1.0f),
		Vec4(0.24f, 0.14f, 0.24f, 1.0f),
		Vec4(0.49f, 0.31f, 0.26f, 1.0f),
		Vec4(0.78f, 0.52f, 0.33f, 1.0f),
		Vec4(0.94f, 0.82f, 0.31f, 1.0f),
		Vec4(0.98f, 0.65f, 0.30f, 1.0f),
		Vec4(0.22f, 0.65f, 0.53f, 1.0f),
		Vec4(0.67f, 0.81f, 0.91f, 1.0f),
		Vec4(0.43f, 0.44f, 0.75f, 1.0f),
		Vec4(0.26f, 0.24f, 0.48f, 1.0f),
	};

	DE_ASSERT(numColors <= DE_LENGTH_OF_ARRAY(colors));

	return std::vector<Vec4>(colors, colors + numColors);
}

//! Renders a colorful grid of rectangles.
tcu::TextureLevel generateReferenceImage (const tcu::TextureFormat	format,
										  const UVec2&				renderSize,
										  const Vec4&				clearColor,
										  const std::vector<UVec4>&	cells,
										  const std::vector<Vec4>&	cellColors)
{
	DE_ASSERT(cells.size() == cellColors.size());

	tcu::TextureLevel image(format, renderSize.x(), renderSize.y());
	tcu::clear(image.getAccess(), clearColor);

	for (std::size_t i = 0; i < cells.size(); ++i)
	{
		const UVec4& cell = cells[i];
		tcu::clear(
			tcu::getSubregion(image.getAccess(), cell.x(), cell.y(), cell.z(), cell.w()),
			cellColors[i]);
	}

	return image;
}

void initVertexTestPrograms (SourceCollections& programCollection, const TestParams)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_ARB_shader_viewport_layer_array : require\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "layout(location = 1) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_ViewportIndex = gl_VertexIndex / 6;\n"
			<< "    gl_Position = in_position;\n"
			<< "    out_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    out_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void initFragmentTestPrograms (SourceCollections& programCollection, const TestParams testParams)
{
	// Vertex shader.
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_ARB_shader_viewport_layer_array : require\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "layout(location = 1) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< (testParams.writeFromVertex ? "    gl_ViewportIndex = gl_VertexIndex / 6;\n" : "")
			<< "    gl_Position = in_position;\n"
			<< "    out_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		// Ignore input color and choose one using the viewport index.
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "layout(set=0, binding=0) uniform Colors {\n"
			<< "    vec4 color[" << testParams.numViewports << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    out_color = color[gl_ViewportIndex];\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void initTessellationTestPrograms (SourceCollections& programCollection, const TestParams)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "layout(location = 1) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = in_position;\n"
			<< "    out_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation control shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(vertices = 3) out;\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color[];\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    if (gl_InvocationID == 0) {\n"
			<< "        gl_TessLevelInner[0] = 1.0;\n"
			<< "        gl_TessLevelInner[1] = 1.0;\n"
			<< "        gl_TessLevelOuter[0] = 1.0;\n"
			<< "        gl_TessLevelOuter[1] = 1.0;\n"
			<< "        gl_TessLevelOuter[2] = 1.0;\n"
			<< "        gl_TessLevelOuter[3] = 1.0;\n"
			<< "    }\n"
			<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "    out_color[gl_InvocationID] = in_color[gl_InvocationID];\n"
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	// Tessellation evaluation shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_ARB_shader_viewport_layer_array : require\n"
			<< "\n"
			<< "layout(triangles, equal_spacing, cw) in;\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_ViewportIndex = gl_PrimitiveID / 2;\n"
			<< "    gl_Position = gl_in[0].gl_Position * gl_TessCoord.x +\n"
			<< "                  gl_in[1].gl_Position * gl_TessCoord.y +\n"
			<< "                  gl_in[2].gl_Position * gl_TessCoord.z;\n"
			<< "\n"
			<< "    out_color = in_color[0] * gl_TessCoord.x +\n"
			<< "                in_color[1] * gl_TessCoord.y +\n"
			<< "                in_color[2] * gl_TessCoord.z;\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    out_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

std::vector<PositionColorVertex> generateVertices (const std::vector<Vec4>& colors)
{
	// Two triangles for each color (viewport).
	std::size_t total = colors.size() * 6;

	std::vector<PositionColorVertex> result;
	result.reserve(total);

	for (std::size_t i = 0; i < total; ++i)
	{
		Vec4 pos;
		pos.z() = 0.0;
		pos.w() = 1.0;

		switch (i % 6) {
		case 0: pos.xy() = Vec2(-1.0,  1.0); break;
		case 1: pos.xy() = Vec2( 1.0,  1.0); break;
		case 2: pos.xy() = Vec2(-1.0, -1.0); break;
		case 3: pos.xy() = Vec2( 1.0, -1.0); break;
		case 4: pos.xy() = Vec2( 1.0,  1.0); break;
		case 5: pos.xy() = Vec2(-1.0, -1.0); break;
		}

		result.push_back(PositionColorVertex(pos, colors[i/6]));
	}

	return result;
}

// Renderer generates two triangles per viewport, each pair using a different color. The
// numViewports are positioned to form a grid.
class Renderer
{
public:
	enum Shader {
		VERTEX,
		TESSELLATION,
		FRAGMENT,
	};

	Renderer (Context&						context,
			  const UVec2&					renderSize,
			  const TestParams&				testParams,
			  const std::vector<UVec4>&		cells,
			  const VkFormat				colorFormat,
			  const Vec4&					clearColor,
			  const std::vector<Vec4>&		colors,
			  const Shader					shader)
		: m_useDynamicRendering		(testParams.useDynamicRendering)
		, m_renderSize				(renderSize)
		, m_colorFormat				(colorFormat)
		, m_colorSubresourceRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u))
		, m_clearValue				(makeClearValueColor(clearColor))
		, m_numViewports			(testParams.numViewports)
		, m_colors					(colors)
		, m_vertices				(generateVertices(colors))
		, m_shader					(shader)
	{
		const DeviceInterface&		vk					= context.getDeviceInterface();
		const VkDevice				device				= context.getDevice();
		const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
		Allocator&					allocator			= context.getDefaultAllocator();
		const VkDeviceSize			vertexBufferSize    = sizeInBytes(m_vertices);

		m_colorImage		= makeImage					(vk, device, makeImageCreateInfo(m_colorFormat, m_renderSize, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
		m_colorImageAlloc	= bindImage					(vk, device, allocator, *m_colorImage, MemoryRequirement::Any);
		m_colorAttachment	= makeImageView				(vk, device, *m_colorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, m_colorSubresourceRange);

		m_vertexBuffer		= Buffer::createAndAlloc	(vk, device, makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), allocator, MemoryRequirement::HostVisible);

		{
			deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), &m_vertices[0], static_cast<std::size_t>(vertexBufferSize));
			flushAlloc(vk, device, m_vertexBuffer->getBoundMemory());
		}

		if (shader == TESSELLATION)
		{
			m_tessellationControlModule		= createShaderModule	(vk, device, context.getBinaryCollection().get("tesc"), 0u);
			m_tessellationEvaluationModule	= createShaderModule	(vk, device, context.getBinaryCollection().get("tese"), 0u);
		}

		m_vertexModule		= createShaderModule	(vk, device, context.getBinaryCollection().get("vert"), 0u);
		m_fragmentModule	= createShaderModule	(vk, device, context.getBinaryCollection().get("frag"), 0u);

		if (!m_useDynamicRendering)
		{
			m_renderPass	= makeRenderPass		(vk, device, m_colorFormat);

			m_framebuffer	= makeFramebuffer		(vk, device, *m_renderPass, m_colorAttachment.get(),
													 static_cast<deUint32>(m_renderSize.x()),  static_cast<deUint32>(m_renderSize.y()));
		}

		if (shader == FRAGMENT)
		{
			vk::DescriptorSetLayoutBuilder builder;
			builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
			m_descriptorSetLayout = builder.build(vk, device);
		}

		m_pipelineLayout	= makePipelineLayout	(vk, device, (shader == FRAGMENT ? m_descriptorSetLayout.get() : DE_NULL));
		m_pipeline			= makeGraphicsPipeline	(vk, device, *m_pipelineLayout, *m_renderPass, *m_vertexModule, *m_tessellationControlModule,
													 *m_tessellationEvaluationModule, *m_fragmentModule, m_renderSize, m_numViewports, cells);
		m_cmdPool			= createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
		m_cmdBuffer			= allocateCommandBuffer	(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	}

	void draw (Context& context, const VkBuffer colorBuffer) const
	{
		const DeviceInterface&		vk			= context.getDeviceInterface();
		const VkDevice				device		= context.getDevice();
		const VkQueue				queue		= context.getUniversalQueue();
		Allocator&					allocator	= context.getDefaultAllocator();

		beginCommandBuffer(vk, *m_cmdBuffer);

		const VkRect2D renderArea = makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y());
		if (m_useDynamicRendering)
		{
			initialTransitionColor2DImage(vk, *m_cmdBuffer, *m_colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			beginRendering(vk, *m_cmdBuffer, *m_colorAttachment, renderArea, m_clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR);
		}
		else
			beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, renderArea, m_clearValue);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		{
			const VkBuffer vertexBuffer = m_vertexBuffer->object();
			const VkDeviceSize vertexBufferOffset = 0ull;
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		// Prepare colors buffer if needed.
		std::unique_ptr<vk::BufferWithMemory>	colorsBuffer;
		vk::Move<vk::VkDescriptorPool>			descriptorPool;
		vk::Move<vk::VkDescriptorSet>			descriptorSet;

		if (m_shader == FRAGMENT)
		{
			// Create buffer.
			const auto	colorsBufferSize		= m_colors.size() * sizeof(decltype(m_colors)::value_type);
			const auto	colorsBufferCreateInfo	= vk::makeBufferCreateInfo(static_cast<VkDeviceSize>(colorsBufferSize), vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
			colorsBuffer.reset(new vk::BufferWithMemory{vk, device, allocator, colorsBufferCreateInfo, MemoryRequirement::HostVisible});

			// Copy colors and flush allocation.
			auto& colorsBufferAlloc = colorsBuffer->getAllocation();
			deMemcpy(colorsBufferAlloc.getHostPtr(), m_colors.data(), colorsBufferSize);
			vk::flushAlloc(vk, device, colorsBufferAlloc);

			// Descriptor pool.
			vk::DescriptorPoolBuilder poolBuilder;
			poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u);
			descriptorPool = poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			// Descriptor set.
			descriptorSet = vk::makeDescriptorSet(vk, device, descriptorPool.get(), m_descriptorSetLayout.get());

			// Update and bind descriptor set.
			const auto						colorsBufferDescriptorInfo = vk::makeDescriptorBufferInfo(colorsBuffer->get(), 0ull, VK_WHOLE_SIZE);
			vk::DescriptorSetUpdateBuilder	updateBuilder;
			updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &colorsBufferDescriptorInfo);
			updateBuilder.update(vk, device);

			vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		}

		vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_numViewports * 6), 1u, 0u, 0u);	// two triangles per viewport

		if (m_useDynamicRendering)
			endRendering(vk, *m_cmdBuffer);
		else
			endRenderPass(vk, *m_cmdBuffer);

		copyImageToBuffer(vk, *m_cmdBuffer, *m_colorImage, colorBuffer, tcu::IVec2(m_renderSize.x(), m_renderSize.y()));

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);
	}

private:
	const bool								m_useDynamicRendering;
	const UVec2								m_renderSize;
	const VkFormat							m_colorFormat;
	const VkImageSubresourceRange			m_colorSubresourceRange;
	const VkClearValue						m_clearValue;
	const int								m_numViewports;
	const std::vector<Vec4>					m_colors;
	const std::vector<PositionColorVertex>	m_vertices;
	const Shader							m_shader;

	Move<VkImage>							m_colorImage;
	MovePtr<Allocation>						m_colorImageAlloc;
	Move<VkImageView>						m_colorAttachment;
	SharedPtr<Buffer>						m_vertexBuffer;
	Move<VkShaderModule>					m_vertexModule;
	Move<VkShaderModule>					m_tessellationControlModule;
	Move<VkShaderModule>					m_tessellationEvaluationModule;
	Move<VkShaderModule>					m_fragmentModule;
	Move<VkRenderPass>						m_renderPass;
	Move<VkFramebuffer>						m_framebuffer;
	Move<VkDescriptorSetLayout>				m_descriptorSetLayout;
	Move<VkPipelineLayout>					m_pipelineLayout;
	Move<VkPipeline>						m_pipeline;
	Move<VkCommandPool>						m_cmdPool;
	Move<VkCommandBuffer>					m_cmdBuffer;

	// "deleted"
				Renderer	(const Renderer&);
	Renderer&	operator=	(const Renderer&);
};

tcu::TestStatus testVertexFragmentShader (Context& context, const TestParams& testParams, Renderer::Shader shader)
{
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	Allocator&						allocator			= context.getDefaultAllocator();

	const UVec2						renderSize			(128, 128);
	const VkFormat					colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const Vec4						clearColor			(0.5f, 0.5f, 0.5f, 1.0f);
	const std::vector<Vec4>			colors				= generateColors(testParams.numViewports);
	const std::vector<UVec4>		cells				= generateGrid(testParams.numViewports, renderSize);

	const VkDeviceSize				colorBufferSize		= renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));

	const SharedPtr<Buffer>			colorBuffer			= Buffer::createAndAlloc(vk, device, makeBufferCreateInfo(colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), allocator, MemoryRequirement::HostVisible);

	// Zero buffer.
	{
		const Allocation alloc = colorBuffer->getBoundMemory();
		deMemset(alloc.getHostPtr(), 0, static_cast<std::size_t>(colorBufferSize));
		flushAlloc(vk, device, alloc);
	}

	{
		context.getTestContext().getLog()
			<< tcu::TestLog::Message << "Rendering a colorful grid of " << testParams.numViewports << " rectangle(s)." << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Not covered area will be filled with a gray color." << tcu::TestLog::EndMessage;
	}

	// Draw
	{
		const Renderer renderer (context, renderSize, testParams, cells, colorFormat, clearColor, colors, shader);
		renderer.draw(context, colorBuffer->object());
	}

	// Log image
	{
		const Allocation alloc = colorBuffer->getBoundMemory();
		invalidateAlloc(vk, device, alloc);

		const tcu::ConstPixelBufferAccess	resultImage		(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1u, alloc.getHostPtr());
		const tcu::TextureLevel				referenceImage	= generateReferenceImage(mapVkFormat(colorFormat), renderSize, clearColor, cells, colors);

		// Images should now match.
		if (!tcu::floatThresholdCompare(context.getTestContext().getLog(), "color", "Image compare", referenceImage.getAccess(), resultImage, Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			TCU_FAIL("Rendered image is not correct");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus testVertexShader (Context& context, const TestParams testParams)
{
	return testVertexFragmentShader(context, testParams, Renderer::VERTEX);
}

tcu::TestStatus testFragmentShader (Context& context, const TestParams testParams)
{
	return testVertexFragmentShader(context, testParams, Renderer::FRAGMENT);
}

tcu::TestStatus testTessellationShader (Context& context, const TestParams testParams)
{
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	Allocator&						allocator			= context.getDefaultAllocator();

	const UVec2						renderSize			(128, 128);
	const VkFormat					colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const Vec4						clearColor			(0.5f, 0.5f, 0.5f, 1.0f);
	const std::vector<Vec4>			colors				= generateColors(testParams.numViewports);
	const std::vector<UVec4>		cells				= generateGrid(testParams.numViewports, renderSize);

	const VkDeviceSize				colorBufferSize		= renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));

	const SharedPtr<Buffer>			colorBuffer			= Buffer::createAndAlloc(vk, device, makeBufferCreateInfo(colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), allocator, MemoryRequirement::HostVisible);

	// Zero buffer.
	{
		const Allocation alloc = colorBuffer->getBoundMemory();
		deMemset(alloc.getHostPtr(), 0, static_cast<std::size_t>(colorBufferSize));
		flushAlloc(vk, device, alloc);
	}

	{
		context.getTestContext().getLog()
			<< tcu::TestLog::Message << "Rendering a colorful grid of " << testParams.numViewports << " rectangle(s)." << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Not covered area will be filled with a gray color." << tcu::TestLog::EndMessage;
	}

	// Draw
	{
		const Renderer renderer (context, renderSize, testParams, cells, colorFormat, clearColor, colors, Renderer::TESSELLATION);
		renderer.draw(context, colorBuffer->object());
	}

	// Log image
	{
		const Allocation alloc = colorBuffer->getBoundMemory();
		invalidateAlloc(vk, device, alloc);

		const tcu::ConstPixelBufferAccess	resultImage		(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1u, alloc.getHostPtr());
		const tcu::TextureLevel				referenceImage	= generateReferenceImage(mapVkFormat(colorFormat), renderSize, clearColor, cells, colors);

		// Images should now match.
		if (!tcu::floatThresholdCompare(context.getTestContext().getLog(), "color", "Image compare", referenceImage.getAccess(), resultImage, Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			TCU_FAIL("Rendered image is not correct");
	}

	return tcu::TestStatus::pass("OK");
}

void checkSupport (Context& context, TestParams params)
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);
	context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");

	if (context.getDeviceProperties().limits.maxViewports < MIN_MAX_VIEWPORTS)
		TCU_FAIL("multiViewport supported but maxViewports is less than the minimum required");

	if (params.useTessellationShader)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (params.useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

} // anonymous

tcu::TestCaseGroup* createShaderViewportIndexTests	(tcu::TestContext& testCtx, bool useDynamicRendering)
{
	MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "shader_viewport_index", ""));

	TestParams testParams
	{
		1,						// int		numViewports;
		false,					// bool		writeFromVertex;
		useDynamicRendering,	// bool		useDynamicRendering;
		false					// bool		useTessellationShader;
	};

	for (testParams.numViewports = 1; testParams.numViewports <= MIN_MAX_VIEWPORTS; ++testParams.numViewports)
		addFunctionCaseWithPrograms(group.get(), "vertex_shader_" + de::toString(testParams.numViewports), "", checkSupport, initVertexTestPrograms, testVertexShader, testParams);

	testParams.numViewports = 1;
	addFunctionCaseWithPrograms(group.get(), "fragment_shader_implicit", "", checkSupport, initFragmentTestPrograms, testFragmentShader, testParams);
	testParams.writeFromVertex = true;
	for (testParams.numViewports = 1; testParams.numViewports <= MIN_MAX_VIEWPORTS; ++testParams.numViewports)
		addFunctionCaseWithPrograms(group.get(), "fragment_shader_" + de::toString(testParams.numViewports), "", checkSupport, initFragmentTestPrograms, testFragmentShader, testParams);
	testParams.writeFromVertex = false;

	testParams.useTessellationShader = true;
	for (testParams.numViewports = 1; testParams.numViewports <= MIN_MAX_VIEWPORTS; ++testParams.numViewports)
		addFunctionCaseWithPrograms(group.get(), "tessellation_shader_" + de::toString(testParams.numViewports), "", checkSupport, initTessellationTestPrograms, testTessellationShader, testParams);

	return group.release();
}

} // Draw
} // vkt
