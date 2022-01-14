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
 * \brief Pipeline Cache Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineCreationFeedbackTests.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTestLog.hpp"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{
enum
{
	VK_MAX_SHADER_STAGES = 6,
};

enum
{
	PIPELINE_CACHE_NDX_NO_CACHE = 0,
	PIPELINE_CACHE_NDX_DERIVATIVE = 1,
	PIPELINE_CACHE_NDX_CACHED = 2,
	PIPELINE_CACHE_NDX_COUNT,
};

// helper functions

std::string getShaderFlagStr (const VkShaderStageFlagBits	shader,
							  bool							isDescription)
{
	std::ostringstream desc;
	switch(shader)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		{
			desc << ((isDescription) ? "vertex stage" : "vertex_stage");
			break;
		}
		case VK_SHADER_STAGE_FRAGMENT_BIT:
		{
			desc << ((isDescription) ? "fragment stage" : "fragment_stage");
			break;
		}
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		{
			desc << ((isDescription) ? "geometry stage" : "geometry_stage");
			break;
		}
		case VK_SHADER_STAGE_COMPUTE_BIT:
		{
			desc << ((isDescription) ? "compute stage" : "compute_stage");
			break;
		}
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		{
			desc << ((isDescription) ? "tessellation control stage" : "tessellation_control_stage");
			break;
		}
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		{
			desc << ((isDescription) ? "tessellation evaluation stage" : "tessellation_evaluation_stage");
			break;
		}
	  default:
		desc << "unknown shader stage!";
		DE_FATAL("Unknown shader Stage!");
		break;
	}

	return desc.str();
}

std::string getCaseStr (const deUint32 ndx)
{
	std::ostringstream desc;
	switch(ndx)
	{
		case PIPELINE_CACHE_NDX_NO_CACHE:
		{
			desc << "No cached pipeline";
			break;
		}
		case PIPELINE_CACHE_NDX_CACHED:
		{
			desc << "Cached pipeline";
			break;
		}
		case PIPELINE_CACHE_NDX_DERIVATIVE:
		{
			desc << "Pipeline derivative";
			break;
		}
	  default:
		desc << "Unknown case";
		DE_FATAL("Unknown case!");
		break;
	}

	return desc.str();
}

// helper classes
class CacheTestParam
{
public:
								CacheTestParam				(const VkShaderStageFlagBits*	shaders,
															 deUint32						count,
															 deBool							noCache,
															 deBool							delayedDestroy);
	virtual					~CacheTestParam					(void);
	virtual const std::string	generateTestName			(void)			const;
	virtual const std::string	generateTestDescription		(void)			const;
	VkShaderStageFlagBits		getShaderFlag				(deUint32 ndx)	const	{ return m_shaders[ndx]; }
	deUint32					getShaderCount				(void)			const	{ return (deUint32)m_shaderCount; }
	deBool						isCacheDisabled				(void)			const	{ return m_noCache; }
	deBool						isDelayedDestroy			(void)			const	{ return m_delayedDestroy; }

protected:
	VkShaderStageFlagBits		m_shaders[VK_MAX_SHADER_STAGES];
	size_t						m_shaderCount;
	bool						m_noCache;
	bool						m_delayedDestroy;
};

CacheTestParam::CacheTestParam (const VkShaderStageFlagBits* shaders, deUint32 count, deBool noCache, deBool delayedDestroy)
{
	DE_ASSERT(count <= VK_MAX_SHADER_STAGES);
	for (deUint32 ndx = 0; ndx < count; ndx++)
		m_shaders[ndx] = shaders[ndx];
	m_shaderCount		= count;
	m_noCache			= noCache;
	m_delayedDestroy	= delayedDestroy;
}

CacheTestParam::~CacheTestParam (void)
{
}

const std::string CacheTestParam::generateTestName (void) const
{
	std::string result(getShaderFlagStr(m_shaders[0], false));
	std::string cacheString [] = { "", "_no_cache" };
	std::string delayedDestroyString [] = { "", "_delayed_destroy" };

	for(deUint32 ndx = 1; ndx < m_shaderCount; ndx++)
		result += '_' + getShaderFlagStr(m_shaders[ndx], false) + cacheString[m_noCache ? 1 : 0] + delayedDestroyString[m_delayedDestroy ? 1 : 0];

	if (m_shaderCount == 1)
		result += cacheString[m_noCache ? 1 : 0] + delayedDestroyString[m_delayedDestroy ? 1 : 0];

	return result;
}

const std::string CacheTestParam::generateTestDescription (void) const
{
	std::string result("Get pipeline creation feedback with " + getShaderFlagStr(m_shaders[0], true));
	if (m_noCache)
		result += " with no cache";
	if (m_delayedDestroy)
		result += " with delayed destroy";

	for(deUint32 ndx = 1; ndx < m_shaderCount; ndx++)
		result += ' ' + getShaderFlagStr(m_shaders[ndx], true);

	return result;
}

class SimpleGraphicsPipelineBuilder
{
public:
							SimpleGraphicsPipelineBuilder	(Context&						context);
							~SimpleGraphicsPipelineBuilder	(void) { }
	void					bindShaderStage					(VkShaderStageFlagBits			stage,
															 const char*					sourceName,
															 const char*					entryName);
	void					enableTessellationStage			(deUint32						patchControlPoints);
	VkPipeline				buildPipeline					(tcu::UVec2						renderSize,
															 VkRenderPass					renderPass,
															 VkPipelineCache				cache,
															 VkPipelineLayout				pipelineLayout,
															 VkPipelineCreationFeedbackEXT	*pipelineCreationFeedback,
															 VkPipelineCreationFeedbackEXT	*pipelineStageCreationFeedbacks,
															 VkPipeline						basePipelineHandle);
	void					resetBuilder					(void);

protected:
	Context&							m_context;

	Move<VkShaderModule>				m_shaderModules[VK_MAX_SHADER_STAGES];
	deUint32							m_shaderStageCount;
	VkPipelineShaderStageCreateInfo	m_shaderStageInfo[VK_MAX_SHADER_STAGES];

	deUint32							m_patchControlPoints;
};

SimpleGraphicsPipelineBuilder::SimpleGraphicsPipelineBuilder (Context& context)
	: m_context(context)
{
	m_patchControlPoints = 0;
	m_shaderStageCount   = 0;
}

void SimpleGraphicsPipelineBuilder::resetBuilder (void)
{
	m_shaderStageCount = 0;
}

void SimpleGraphicsPipelineBuilder::bindShaderStage (VkShaderStageFlagBits	stage,
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
	m_shaderStageInfo[m_shaderStageCount].sType				= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	m_shaderStageInfo[m_shaderStageCount].pNext				= DE_NULL;
	m_shaderStageInfo[m_shaderStageCount].flags				= 0u;
	m_shaderStageInfo[m_shaderStageCount].stage				= stage;
	m_shaderStageInfo[m_shaderStageCount].module				= *m_shaderModules[m_shaderStageCount];
	m_shaderStageInfo[m_shaderStageCount].pName				= entryName;
	m_shaderStageInfo[m_shaderStageCount].pSpecializationInfo	= DE_NULL;

	m_shaderStageCount++;
}

VkPipeline SimpleGraphicsPipelineBuilder::buildPipeline (tcu::UVec2 renderSize, VkRenderPass renderPass, VkPipelineCache cache,
														 VkPipelineLayout pipelineLayout, VkPipelineCreationFeedbackEXT *pipelineCreationFeedback,
														 VkPipelineCreationFeedbackEXT *pipelineStageCreationFeedbacks, VkPipeline basePipelineHandle)
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
		(m_patchControlPoints == 0 ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
								   : VK_PRIMITIVE_TOPOLOGY_PATCH_LIST), // VkPrimitiveTopology						topology;
		VK_FALSE,														// VkBool32									primitiveRestartEnable;
	};

	const VkViewport	viewport	= makeViewport(renderSize);
	const VkRect2D		scissor	= makeRect2D(renderSize);

	const VkPipelineViewportStateCreateInfo viewportStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType						sType;
		DE_NULL,														// const void*							pNext;
		0u,																// VkPipelineViewportStateCreateFlags	flags;
		1u,																// deUint32								viewportCount;
		&viewport,														// const VkViewport*					pViewports;
		1u,																// deUint32								scissorCount;
		&scissor														// const VkRect2D*						pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo rasterStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														// VkBool32									depthClampEnable;
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
		VK_FALSE,														// VkBool32		blendEnable;
		VK_BLEND_FACTOR_ONE,											// VkBlendFactor	srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor	dstColorBlendFactor;
		VK_BLEND_OP_ADD,												// VkBlendOp		colorBlendOp;
		VK_BLEND_FACTOR_ONE,											// VkBlendFactor	srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor	dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,												// VkBlendOp		alphaBlendOp;
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT										// VkColorComponentFlags    colorWriteMask;
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
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE,													// VkBool32									alphaToOneEnable;
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,													// VkBool32									depthTestEnable;
		VK_TRUE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		VK_FALSE,													// VkBool32									stencilTestEnable;
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
		0.0f,														// float									minDepthBounds;
		1.0f,														// float									maxDepthBounds;
	};

	const VkPipelineTessellationStateCreateInfo tessStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineTesselationStateCreateFlags	flags;
		m_patchControlPoints,										// deUint32									patchControlPoints;
	};
	const VkPipelineTessellationStateCreateInfo* pTessCreateInfo = (m_patchControlPoints > 0)
																  ? &tessStateCreateInfo
																  : DE_NULL;

	const VkPipelineCreationFeedbackCreateInfoEXT pipelineCreationFeedbackCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT,	// VkStructureType						sType;
		DE_NULL,														// const void*							pNext;
		pipelineCreationFeedback,										// VkPipelineCreationFeedbackEXT*		pPipelineCreationFeedback;
		m_shaderStageCount,												// deUint32								pipelineStageCreationFeedbackCount;
		pipelineStageCreationFeedbacks									// VkPipelineCreationFeedbackEXT*		pPipelineStageCreationFeedbacks;
	};

	deUint32 flagsCreateInfo = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	if (basePipelineHandle != DE_NULL)
	{
		flagsCreateInfo = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	}

	const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
		&pipelineCreationFeedbackCreateInfo,				// const void*										pNext;
		flagsCreateInfo,									// VkPipelineCreateFlags							flags;
		m_shaderStageCount,									// deUint32											stageCount;
		m_shaderStageInfo,									// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		pTessCreateInfo,									// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&rasterStateParams,									// const VkPipelineRasterizationStateCreateInfo*	pRasterState;
		&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,										// VkPipelineLayout									layout;
		renderPass,											// VkRenderPass										renderPass;
		0u,													// deUint32											subpass;
		basePipelineHandle,									// VkPipeline										basePipelineHandle;
		basePipelineHandle != DE_NULL ? -1 : 0,				// deInt32											basePipelineIndex;
	};
	VkPipeline pipeline;
	vk.createGraphicsPipelines(vkDevice, cache, 1u, &graphicsPipelineParams, DE_NULL, &pipeline);
	return pipeline;
}

void SimpleGraphicsPipelineBuilder::enableTessellationStage (deUint32 patchControlPoints)
{
	m_patchControlPoints = patchControlPoints;
}

template <class Test>
vkt::TestCase* newTestCase (tcu::TestContext&		testContext,
							const CacheTestParam*	testParam)
{
	return new Test(testContext,
					testParam->generateTestName().c_str(),
					testParam->generateTestDescription().c_str(),
					testParam);
}

// Test Classes
class CacheTest : public vkt::TestCase
{
public:
							CacheTest(tcu::TestContext&		testContext,
									  const std::string&		name,
									  const std::string&		description,
									  const CacheTestParam*	param)
								: vkt::TestCase (testContext, name, description)
								, m_param (*param)
								{ }
	virtual				~CacheTest (void) { }
protected:
	const CacheTestParam	m_param;
};

class CacheTestInstance : public vkt::TestInstance
{
public:
							CacheTestInstance			(Context&				context,
														 const CacheTestParam*	param);
	virtual					~CacheTestInstance			(void);
	virtual tcu::TestStatus iterate						(void);
protected:
	virtual tcu::TestStatus verifyTestResult			(void) = 0;
protected:
	const CacheTestParam*	m_param;

	Move<VkPipelineCache>	m_cache;
	deBool					m_extensions;
};

CacheTestInstance::CacheTestInstance (Context&					context,
									  const CacheTestParam*	param)
	: TestInstance		(context)
	, m_param			(param)
	, m_extensions		(m_context.requireDeviceFunctionality("VK_EXT_pipeline_creation_feedback"))
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const VkDevice			vkDevice		= m_context.getDevice();

	if (m_param->isCacheDisabled() == DE_FALSE)
	{
		const VkPipelineCacheCreateInfo pipelineCacheCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,												// const void*					pNext;
			0u,														// VkPipelineCacheCreateFlags	flags;
			0u,														// deUintptr					initialDataSize;
			DE_NULL,												// const void*					pInitialData;
		};

		m_cache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
	}
}

CacheTestInstance::~CacheTestInstance (void)
{
}

tcu::TestStatus CacheTestInstance::iterate (void)
{
	return verifyTestResult();
}

class GraphicsCacheTest : public CacheTest
{
public:
							GraphicsCacheTest	(tcu::TestContext&		testContext,
												 const std::string&		name,
												 const std::string&		description,
												 const CacheTestParam*	param)
								: CacheTest (testContext, name, description, param)
								{ }
	virtual					~GraphicsCacheTest	(void) { }
	virtual void			initPrograms		(SourceCollections&	programCollection) const;
	virtual void			checkSupport		(Context&			context) const;
	virtual TestInstance*	createInstance		(Context&			context) const;
};

class GraphicsCacheTestInstance : public CacheTestInstance
{
public:
							GraphicsCacheTestInstance	(Context&				context,
														 const CacheTestParam*	param);
	virtual					~GraphicsCacheTestInstance	(void);
protected:
	virtual tcu::TestStatus verifyTestResult			(void);

protected:
	const tcu::UVec2					m_renderSize;
	const VkFormat						m_colorFormat;
	const VkFormat						m_depthFormat;
	Move<VkPipelineLayout>				m_pipelineLayout;

	SimpleGraphicsPipelineBuilder		m_pipelineBuilder;
	SimpleGraphicsPipelineBuilder		m_missPipelineBuilder;
	Move<VkRenderPass>					m_renderPass;

	VkPipeline							m_pipeline[PIPELINE_CACHE_NDX_COUNT];
	VkPipelineCreationFeedbackEXT		m_pipelineCreationFeedback[PIPELINE_CACHE_NDX_COUNT];
	VkPipelineCreationFeedbackEXT		m_pipelineStageCreationFeedbacks[PIPELINE_CACHE_NDX_COUNT * VK_MAX_SHADER_STAGES];
};

void GraphicsCacheTest::initPrograms (SourceCollections& programCollection) const
{
	for (deUint32 shaderNdx = 0; shaderNdx < m_param.getShaderCount(); shaderNdx++)
	{
		switch(m_param.getShaderFlag(shaderNdx))
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			programCollection.glslSources.add("color_vert_1") << glu::VertexSource(
						"#version 310 es\n"
						"layout(location = 0) in vec4 position;\n"
						"layout(location = 1) in vec4 color;\n"
						"layout(location = 0) out highp vec4 vtxColor;\n"
						"void main (void)\n"
						"{\n"
						"  gl_Position = position;\n"
						"  vtxColor = color;\n"
						"}\n");
			programCollection.glslSources.add("color_vert_2") << glu::VertexSource(
						"#version 310 es\n"
						"layout(location = 0) in vec4 position;\n"
						"layout(location = 1) in vec4 color;\n"
						"layout(location = 0) out highp vec4 vtxColor;\n"
						"void main (void)\n"
						"{\n"
						"  gl_Position = position;\n"
						"  gl_PointSize = 1.0f;\n"
						"  vtxColor = color + vec4(0.1, 0.2, 0.3, 0.0);\n"
						"}\n");
					break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			programCollection.glslSources.add("color_frag") << glu::FragmentSource(
						"#version 310 es\n"
						"layout(location = 0) in highp vec4 vtxColor;\n"
						"layout(location = 0) out highp vec4 fragColor;\n"
						"void main (void)\n"
						"{\n"
						"  fragColor = vtxColor;\n"
						"}\n");
			break;

		case VK_SHADER_STAGE_GEOMETRY_BIT:
			programCollection.glslSources.add("unused_geo") << glu::GeometrySource(
						"#version 450 \n"
						"layout(triangles) in;\n"
						"layout(triangle_strip, max_vertices = 3) out;\n"
						"layout(location = 0) in highp vec4 in_vtxColor[];\n"
						"layout(location = 0) out highp vec4 vtxColor;\n"
						"out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
						"in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[];\n"
						"void main (void)\n"
						"{\n"
						"  for(int ndx=0; ndx<3; ndx++)\n"
						"  {\n"
						"    gl_Position = gl_in[ndx].gl_Position;\n"
						"    gl_PointSize = gl_in[ndx].gl_PointSize;\n"
						"    vtxColor    = in_vtxColor[ndx];\n"
						"    EmitVertex();\n"
						"  }\n"
						"  EndPrimitive();\n"
						"}\n");
			break;

		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			programCollection.glslSources.add("basic_tcs") << glu::TessellationControlSource(
						"#version 450 \n"
						"layout(vertices = 3) out;\n"
						"layout(location = 0) in highp vec4 color[];\n"
						"layout(location = 0) out highp vec4 vtxColor[];\n"
						"out gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_out[3];\n"
						"in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[gl_MaxPatchVertices];\n"
						"void main()\n"
						"{\n"
						"  gl_TessLevelOuter[0] = 4.0;\n"
						"  gl_TessLevelOuter[1] = 4.0;\n"
						"  gl_TessLevelOuter[2] = 4.0;\n"
						"  gl_TessLevelInner[0] = 4.0;\n"
						"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
						"  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
						"  vtxColor[gl_InvocationID] = color[gl_InvocationID];\n"
						"}\n");
					break;

				case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
					programCollection.glslSources.add("basic_tes") << glu::TessellationEvaluationSource(
						"#version 450 \n"
						"layout(triangles, fractional_even_spacing, ccw) in;\n"
						"layout(location = 0) in highp vec4 colors[];\n"
						"layout(location = 0) out highp vec4 vtxColor;\n"
						"out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
						"in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[gl_MaxPatchVertices];\n"
						"void main() \n"
						"{\n"
						"  float u = gl_TessCoord.x;\n"
						"  float v = gl_TessCoord.y;\n"
						"  float w = gl_TessCoord.z;\n"
						"  vec4 pos = vec4(0);\n"
						"  vec4 color = vec4(0);\n"
						"  pos.xyz += u * gl_in[0].gl_Position.xyz;\n"
						"  color.xyz += u * colors[0].xyz;\n"
						"  pos.xyz += v * gl_in[1].gl_Position.xyz;\n"
						"  color.xyz += v * colors[1].xyz;\n"
						"  pos.xyz += w * gl_in[2].gl_Position.xyz;\n"
						"  color.xyz += w * colors[2].xyz;\n"
						"  pos.w = 1.0;\n"
						"  color.w = 1.0;\n"
						"  gl_Position = pos;\n"
						"  gl_PointSize = gl_in[0].gl_PointSize;"
						"  vtxColor = color;\n"
						"}\n");
					break;

				default:
					DE_FATAL("Unknown Shader Stage!");
					break;
		}
	}
}

void GraphicsCacheTest::checkSupport (Context& context) const
{
	for (deUint32 shaderNdx = 0; shaderNdx < m_param.getShaderCount(); shaderNdx++)
	{
		switch(m_param.getShaderFlag(shaderNdx))
		{
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
			break;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
			break;
		default:
			break;
		}
	}
}

TestInstance* GraphicsCacheTest::createInstance (Context& context) const
{
	return new GraphicsCacheTestInstance(context, &m_param);
}

GraphicsCacheTestInstance::GraphicsCacheTestInstance (Context&				context,
													  const CacheTestParam*	param)
	: CacheTestInstance		(context, param)
	, m_renderSize			(32u, 32u)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthFormat			(VK_FORMAT_D16_UNORM)
	, m_pipelineBuilder		(context)
	, m_missPipelineBuilder	(context)
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const VkDevice			vkDevice		= m_context.getDevice();

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

	// Create render pass
	m_renderPass = makeRenderPass(vk, vkDevice, m_colorFormat, m_depthFormat);

	// Bind shader stages
	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		for (deUint32 shaderNdx = 0; shaderNdx < m_param->getShaderCount(); shaderNdx++)
		{
			switch(m_param->getShaderFlag(shaderNdx))
			{
			case VK_SHADER_STAGE_VERTEX_BIT:
				{
					std::string	shader_name("color_vert_");
					shader_name += (ndx == PIPELINE_CACHE_NDX_DERIVATIVE) ? "2" : "1";
					m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_VERTEX_BIT, shader_name.c_str(), "main");
				}
				break;
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "color_frag", "main");
				break;
			case VK_SHADER_STAGE_GEOMETRY_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, "unused_geo", "main");
				break;
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "basic_tcs", "main");
				m_pipelineBuilder.enableTessellationStage(3);
				break;
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "basic_tes", "main");
				m_pipelineBuilder.enableTessellationStage(3);
				break;
			default:
				DE_FATAL("Unknown Shader Stage!");
				break;
			}
		}
		if (ndx == PIPELINE_CACHE_NDX_CACHED && !param->isDelayedDestroy())
		{
			// Destroy the NO_CACHE pipeline to check that the cached one really hits cache,
			// except for the case where we're testing cache hit of a pipeline still active.
			vk.destroyPipeline(vkDevice, m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE], DE_NULL);
		}

		m_pipeline[ndx] = m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *m_cache, *m_pipelineLayout,
														  &m_pipelineCreationFeedback[ndx],
														  &m_pipelineStageCreationFeedbacks[VK_MAX_SHADER_STAGES * ndx],
														  ndx == PIPELINE_CACHE_NDX_DERIVATIVE ? m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE] : DE_NULL);
		m_pipelineBuilder.resetBuilder();

		if (ndx != PIPELINE_CACHE_NDX_NO_CACHE)
		{
			// Destroy the pipeline as soon as it is created, except the NO_CACHE because
			// it is needed as a base pipeline for the derivative case.
			vk.destroyPipeline(vkDevice, m_pipeline[ndx], DE_NULL);

			if (ndx == PIPELINE_CACHE_NDX_CACHED && param->isDelayedDestroy())
			{
				// Destroy the pipeline we didn't destroy earlier for the isDelayedDestroy case.
				vk.destroyPipeline(vkDevice, m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE], DE_NULL);
			}
	}
	}
}

GraphicsCacheTestInstance::~GraphicsCacheTestInstance (void)
{
}

tcu::TestStatus GraphicsCacheTestInstance::verifyTestResult (void)
{
	tcu::TestLog &log			= m_context.getTestContext().getLog();
	bool durationZeroWarning	= DE_FALSE;
	bool cachedPipelineWarning	= DE_FALSE;

	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		std::ostringstream	message;
		message << getCaseStr(ndx);

		// No need to check per stage status as it is compute pipeline (only one stage) and Vulkan spec mentions that:
		// "One common scenario for an implementation to skip per-stage feedback is when
		// VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT is set in pPipelineCreationFeedback."
		//
		// Check first that the no cached pipeline was missed in the pipeline cache
		if (!(m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
		{
			message << ": invalid data";
			return tcu::TestStatus::fail(message.str());
		}

		if (m_param->isCacheDisabled() && m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
		{
			message << ": feedback indicates pipeline hit cache when it shouldn't";
			return tcu::TestStatus::fail(message.str());
		}

		if (ndx == PIPELINE_CACHE_NDX_NO_CACHE && m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
		{
			message << ": hit the cache when it shouldn't";
			return tcu::TestStatus::fail(message.str());
		}

		if (ndx != PIPELINE_CACHE_NDX_DERIVATIVE && m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT)
		{
			message << ": feedback indicates base pipeline acceleration when it shouldn't";
			return tcu::TestStatus::fail(message.str());
		}

		if (ndx == PIPELINE_CACHE_NDX_CACHED && !m_param->isCacheDisabled() && (m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
		{
			message << "\nWarning: Cached pipeline case did not hit the cache";
			cachedPipelineWarning = DE_TRUE;
		}

		if (m_pipelineCreationFeedback[ndx].duration == 0)
		{
			message << "\nWarning: Pipeline creation feedback reports duration spent creating a pipeline was zero nanoseconds\n";
			durationZeroWarning = DE_TRUE;
		}

		message << "\n";
		message << "\t\t Hit cache ? \t\t\t"				<< (m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ? "yes" : "no")	<< "\n";
		message << "\t\t Base Pipeline Acceleration ? \t"	<< (m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ?	 "yes" : "no")		<< "\n";
		message << "\t\t Duration (ns): \t\t"				<< m_pipelineCreationFeedback[ndx].duration																							<< "\n";

		for(deUint32 shader = 0; shader < m_param->getShaderCount(); shader++)
		{
			const deUint32 index = VK_MAX_SHADER_STAGES * ndx + shader;
			message << "\t" << getShaderFlagStr(m_param->getShaderFlag(shader), true) << "\n";

			if (!(m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
			{
				if (m_pipelineStageCreationFeedbacks[index].flags)
				{
					std::ostringstream			errorMsg;
					errorMsg << getCaseStr(ndx) << ": Creation feedback is not valid for " + getShaderFlagStr(m_param->getShaderFlag(shader), true) + " but there are other flags written";
					return tcu::TestStatus::fail(errorMsg.str());
				}
				message << "\t\t Pipeline Creation Feedback data is not valid\n";
				continue;
			}
			if (m_param->isCacheDisabled() && m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
			{
				std::ostringstream			errorMsg;
				errorMsg << getCaseStr(ndx) << ": feedback indicates pipeline " + getShaderFlagStr(m_param->getShaderFlag(shader), true) + " stage hit cache when it shouldn't";
				return tcu::TestStatus::fail(errorMsg.str());
			}

			if (ndx == PIPELINE_CACHE_NDX_CACHED && !m_param->isCacheDisabled() && (m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
			{
				message << "Warning: pipeline stage did not hit the cache\n";
				cachedPipelineWarning = DE_TRUE;
			}
			if (cachedPipelineWarning && m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
			{
				// We only set the warning when the pipeline nor the pipeline stages hit the cache. If any of them did, them disable the warning.
				cachedPipelineWarning = DE_FALSE;
			}

			message << "\t\t Hit cache ? \t\t\t"				<< (m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ? "yes" : "no")	<< "\n";
			message << "\t\t Base Pipeline Acceleration ? \t"	<< (m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ?	 "yes" : "no")		<< "\n";
			message << "\t\t Duration (ns): \t\t"				<< m_pipelineStageCreationFeedbacks[index].duration																							<< "\n";
		}

		log << tcu::TestLog::Message << message.str() << tcu::TestLog::EndMessage;
	}

	if (cachedPipelineWarning)
	{
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Cached pipeline or stage did not hit the cache");
	}
	if (durationZeroWarning)
	{
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Pipeline creation feedback reports duration spent creating a pipeline was zero nanoseconds");
	}
	return tcu::TestStatus::pass("Pass");
}

class ComputeCacheTest : public CacheTest
{
public:
							ComputeCacheTest		(tcu::TestContext&		testContext,
													 const std::string&		name,
													 const std::string&		description,
													 const CacheTestParam*	param)
								: CacheTest		(testContext, name, description, param)
								{ }
	virtual					~ComputeCacheTest	(void) { }
	virtual void			initPrograms			(SourceCollections&	programCollection) const;
	virtual TestInstance*	createInstance			(Context&				context) const;
};

class ComputeCacheTestInstance : public CacheTestInstance
{
public:
							ComputeCacheTestInstance		(Context&				context,
															 const CacheTestParam*	param);
	virtual					~ComputeCacheTestInstance	(void);
protected:
	virtual tcu::TestStatus verifyTestResult				(void);
			void			buildDescriptorSets				(deUint32 ndx);
			void			buildShader						(deUint32 ndx);
			void			buildPipeline					(const CacheTestParam*	param, deUint32 ndx);
protected:
	Move<VkBuffer>					m_inputBuf;
	de::MovePtr<Allocation>		m_inputBufferAlloc;
	Move<VkShaderModule>			m_computeShaderModule[PIPELINE_CACHE_NDX_COUNT];

	Move<VkBuffer>					m_outputBuf[PIPELINE_CACHE_NDX_COUNT];
	de::MovePtr<Allocation>		m_outputBufferAlloc[PIPELINE_CACHE_NDX_COUNT];

	Move<VkDescriptorPool>			m_descriptorPool[PIPELINE_CACHE_NDX_COUNT];
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout[PIPELINE_CACHE_NDX_COUNT];
	Move<VkDescriptorSet>			m_descriptorSet[PIPELINE_CACHE_NDX_COUNT];

	Move<VkPipelineLayout>			m_pipelineLayout[PIPELINE_CACHE_NDX_COUNT];
	VkPipeline						m_pipeline[PIPELINE_CACHE_NDX_COUNT];
	VkPipelineCreationFeedbackEXT	m_pipelineCreationFeedback[PIPELINE_CACHE_NDX_COUNT];
	VkPipelineCreationFeedbackEXT	m_pipelineStageCreationFeedback[PIPELINE_CACHE_NDX_COUNT];
};

void ComputeCacheTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("basic_compute_1") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(local_size_x = 1) in;\n"
		"layout(std430) buffer;\n"
		"layout(binding = 0) readonly buffer Input0\n"
		"{\n"
		"  vec4 elements[];\n"
		"} input_data0;\n"
		"layout(binding = 1) writeonly buffer Output\n"
		"{\n"
		"  vec4 elements[];\n"
		"} output_data;\n"
		"void main()\n"
		"{\n"
		"  uint ident = gl_GlobalInvocationID.x;\n"
		"  output_data.elements[ident] = input_data0.elements[ident] * input_data0.elements[ident];\n"
		"}");
	programCollection.glslSources.add("basic_compute_2") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(local_size_x = 1) in;\n"
		"layout(std430) buffer;\n"
		"layout(binding = 0) readonly buffer Input0\n"
		"{\n"
		"  vec4 elements[];\n"
		"} input_data0;\n"
		"layout(binding = 1) writeonly buffer Output\n"
		"{\n"
		"  vec4 elements[];\n"
		"} output_data;\n"
		"void main()\n"
		"{\n"
		"  uint ident = gl_GlobalInvocationID.x;\n"
		"  output_data.elements[ident] = input_data0.elements[ident];\n"
		"}");
}

TestInstance* ComputeCacheTest::createInstance (Context& context) const
{
	return new ComputeCacheTestInstance(context, &m_param);
}

void ComputeCacheTestInstance::buildDescriptorSets (deUint32 ndx)
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const VkDevice			vkDevice		= m_context.getDevice();

	// Create descriptor set layout
	DescriptorSetLayoutBuilder descLayoutBuilder;
	for (deUint32 bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
		descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	m_descriptorSetLayout[ndx] = descLayoutBuilder.build(vk, vkDevice);
}

void ComputeCacheTestInstance::buildShader (deUint32 ndx)
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const VkDevice			vkDevice		= m_context.getDevice();

	std::string shader_name("basic_compute_");

	shader_name += (ndx == PIPELINE_CACHE_NDX_DERIVATIVE) ? "2" : "1";

	// Create compute shader
	VkShaderModuleCreateInfo shaderModuleCreateInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,								// VkStructureType				sType;
		DE_NULL,																	// const void*					pNext;
		0u,																		// VkShaderModuleCreateFlags	flags;
		m_context.getBinaryCollection().get(shader_name).getSize(),				// deUintptr					codeSize;
		(deUint32*)m_context.getBinaryCollection().get(shader_name).getBinary(),	// const deUint32*				pCode;
	};
	m_computeShaderModule[ndx] = createShaderModule(vk, vkDevice, &shaderModuleCreateInfo);
}

void ComputeCacheTestInstance::buildPipeline (const CacheTestParam*	param, deUint32 ndx)
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();
	const VkDevice			vkDevice		 = m_context.getDevice();

	deMemset(&m_pipelineCreationFeedback[ndx], 0, sizeof(VkPipelineCreationFeedbackEXT));
	deMemset(&m_pipelineStageCreationFeedback[ndx], 0, sizeof(VkPipelineCreationFeedbackEXT));

	const VkPipelineCreationFeedbackCreateInfoEXT pipelineCreationFeedbackCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT,	// VkStructureType					sType;
		DE_NULL,														// const void *						pNext;
		&m_pipelineCreationFeedback[ndx],								// VkPipelineCreationFeedbackEXT*	pPipelineCreationFeedback;
		1u,															// deUint32						pipelineStageCreationFeedbackCount;
		&m_pipelineStageCreationFeedback[ndx]							// VkPipelineCreationFeedbackEXT*	pPipelineStageCreationFeedbacks;
	};

	// Create compute pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,					// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		0u,															// VkPipelineLayoutCreateFlags		flags;
		1u,															// deUint32						setLayoutCount;
		&m_descriptorSetLayout[ndx].get(),								// const VkDescriptorSetLayout*	pSetLayouts;
		0u,															// deUint32						pushConstantRangeCount;
		DE_NULL,														// const VkPushConstantRange*		pPushConstantRanges;
	};

	m_pipelineLayout[ndx] = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

	const VkPipelineShaderStageCreateInfo stageCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		0u,															// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,									// VkShaderStageFlagBits				stage;
		*m_computeShaderModule[ndx],									// VkShaderModule						module;
		"main",														// const char*						pName;
		DE_NULL,														// const VkSpecializationInfo*		pSpecializationInfo;
	};

	VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,				// VkStructureType					sType;
		&pipelineCreationFeedbackCreateInfo,							// const void*						pNext;
		0u,															// VkPipelineCreateFlags			flags;
		stageCreateInfo,												// VkPipelineShaderStageCreateInfo	stage;
		*m_pipelineLayout[ndx],										// VkPipelineLayout				layout;
		(VkPipeline)0,													// VkPipeline						basePipelineHandle;
		0u,															// deInt32							basePipelineIndex;
	};

	if (ndx != PIPELINE_CACHE_NDX_DERIVATIVE)
	{
		pipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	}

	if (ndx == PIPELINE_CACHE_NDX_DERIVATIVE)
	{
		pipelineCreateInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		pipelineCreateInfo.basePipelineHandle = m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE];
		pipelineCreateInfo.basePipelineIndex = -1;
	}

	if (ndx == PIPELINE_CACHE_NDX_CACHED && !param->isDelayedDestroy())
	{
		// Destroy the NO_CACHE pipeline to check that the cached one really hits cache,
		// except for the case where we're testing cache hit of a pipeline still active.
		vk.destroyPipeline(vkDevice, m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE], DE_NULL);
	}

	vk.createComputePipelines(vkDevice, *m_cache, 1u, &pipelineCreateInfo, DE_NULL, &m_pipeline[ndx]);

	if (ndx != PIPELINE_CACHE_NDX_NO_CACHE)
	{
		// Destroy the pipeline as soon as it is created, except the NO_CACHE because
		// it is needed as a base pipeline for the derivative case.
		vk.destroyPipeline(vkDevice, m_pipeline[ndx], DE_NULL);

		if (ndx == PIPELINE_CACHE_NDX_CACHED && param->isDelayedDestroy())
		{
			// Destroy the pipeline we didn't destroy earlier for the isDelayedDestroy case.
			vk.destroyPipeline(vkDevice, m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE], DE_NULL);
		}
	}
}

ComputeCacheTestInstance::ComputeCacheTestInstance (Context&				context,
													const CacheTestParam*	param)
	: CacheTestInstance (context, param)
{
	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		buildDescriptorSets(ndx);
		buildShader(ndx);
		buildPipeline(param, ndx);
	}
}

ComputeCacheTestInstance::~ComputeCacheTestInstance (void)
{
}

tcu::TestStatus ComputeCacheTestInstance::verifyTestResult (void)
{
	tcu::TestLog &log				= m_context.getTestContext().getLog();
	deBool durationZeroWarning		= DE_FALSE;
	deBool cachedPipelineWarning	= DE_FALSE;

	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		std::ostringstream message;
		message << getCaseStr(ndx);

		// No need to check per stage status as it is compute pipeline (only one stage) and Vulkan spec mentions that:
		// "One common scenario for an implementation to skip per-stage feedback is when
		// VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT is set in pPipelineCreationFeedback."
		//
		// Check first that the no cached pipeline was missed in the pipeline cache
		if (!(m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
		{
			message << ": invalid data";
			return tcu::TestStatus::fail(message.str());
		}

		if (m_param->isCacheDisabled() && m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
		{
			message << ": feedback indicates pipeline hit cache when it shouldn't";
			return tcu::TestStatus::fail(message.str());
		}

		if (ndx == PIPELINE_CACHE_NDX_NO_CACHE && m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
		{
			message << ": hit the cache when it shouldn't";
			return tcu::TestStatus::fail(message.str());
		}

		if (!(ndx == PIPELINE_CACHE_NDX_DERIVATIVE && !m_param->isCacheDisabled()) && m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT)
		{
			message << ": feedback indicates base pipeline acceleration when it shouldn't";
			return tcu::TestStatus::fail(message.str());
		}

		if (ndx == PIPELINE_CACHE_NDX_CACHED && !m_param->isCacheDisabled() && (m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
		{
			message << "\nWarning: Cached pipeline case did not hit the cache";
			cachedPipelineWarning = DE_TRUE;
		}

		if (m_pipelineCreationFeedback[ndx].duration == 0)
		{
			message << "\nWarning: Pipeline creation feedback reports duration spent creating a pipeline was zero nanoseconds\n";
			durationZeroWarning = DE_TRUE;
		}

		message << "\n";

		message << "\t\t Hit cache ? \t\t\t"				<< (m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ? "yes" : "no")	<< "\n";
		message << "\t\t Base Pipeline Acceleration ? \t"	<< (m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ? "yes" : "no")		<< "\n";
		message << "\t\t Duration (ns): \t\t"				<< m_pipelineCreationFeedback[ndx].duration																						<< "\n";

		message << "\t Compute Stage\n";

		// According to the spec:
		//
		// "An implementation should write pipeline creation feedback to pPipelineCreationFeedback and
		//	may write pipeline stage creation feedback to pPipelineStageCreationFeedbacks."
		if (!(m_pipelineStageCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
		{
			// According to the spec:
			// "If the VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT is not set in flags, an implementation
			//	must not set any other bits in flags, and all other VkPipelineCreationFeedbackEXT data members are undefined."
			if (m_pipelineStageCreationFeedback[ndx].flags)
			{
				std::ostringstream			errorMsg;
				errorMsg << getCaseStr(ndx) << ": Creation feedback is not valid for compute stage but there are other flags written";
				return tcu::TestStatus::fail(errorMsg.str());
			}
			message << "\t\t Pipeline Creation Feedback data is not valid\n";
		}
		else
		{
			if (m_param->isCacheDisabled() && m_pipelineStageCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
			{
				std::ostringstream			errorMsg;
				errorMsg << getCaseStr(ndx) << ": feedback indicates pipeline compute stage hit cache when it shouldn't";
				return tcu::TestStatus::fail(errorMsg.str());
			}

			if (ndx == PIPELINE_CACHE_NDX_CACHED && !m_param->isCacheDisabled() && (m_pipelineStageCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
			{
				message << "Warning: pipeline stage did not hit the cache\n";
				cachedPipelineWarning = DE_TRUE;
			}
			if (cachedPipelineWarning && m_pipelineStageCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
			{
				// We only set the warning when the pipeline nor the pipeline stages hit the cache. If any of them did, them disable the warning.
				cachedPipelineWarning = DE_FALSE;
			}

			message << "\t\t Hit cache ? \t\t\t"				<< (m_pipelineStageCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ? "yes" : "no")	<< "\n";
			message << "\t\t Base Pipeline Acceleration ? \t"	<< (m_pipelineStageCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ? "yes" : "no")		<< "\n";
			message << "\t\t Duration (ns): \t\t"				<< m_pipelineStageCreationFeedback[ndx].duration																						<< "\n";
		}

		log << tcu::TestLog::Message << message.str() << tcu::TestLog::EndMessage;
	}

	if (cachedPipelineWarning)
	{
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Cached pipeline or stage did not hit the cache");
	}
	if (durationZeroWarning)
	{
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Pipeline creation feedback reports duration spent creating a pipeline was zero nanoseconds");
	}
	return tcu::TestStatus::pass("Pass");
}
} // anonymous

tcu::TestCaseGroup* createCreationFeedbackTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> cacheTests (new tcu::TestCaseGroup(testCtx, "creation_feedback", "pipeline creation feedback tests"));

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests (new tcu::TestCaseGroup(testCtx, "graphics_tests", "Test pipeline creation feedback with graphics pipeline."));

		const VkShaderStageFlagBits testParamShaders0[] =
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_FRAGMENT_BIT,
		};
		const VkShaderStageFlagBits testParamShaders1[] =
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_GEOMETRY_BIT,
			VK_SHADER_STAGE_FRAGMENT_BIT,
		};
		const VkShaderStageFlagBits testParamShaders2[] =
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
			VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
			VK_SHADER_STAGE_FRAGMENT_BIT,
		};
		const CacheTestParam testParams[] =
		{
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), DE_FALSE, DE_FALSE),
			CacheTestParam(testParamShaders1, DE_LENGTH_OF_ARRAY(testParamShaders1), DE_FALSE, DE_FALSE),
			CacheTestParam(testParamShaders2, DE_LENGTH_OF_ARRAY(testParamShaders2), DE_FALSE, DE_FALSE),
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), DE_TRUE, DE_FALSE),
			CacheTestParam(testParamShaders1, DE_LENGTH_OF_ARRAY(testParamShaders1), DE_TRUE, DE_FALSE),
			CacheTestParam(testParamShaders2, DE_LENGTH_OF_ARRAY(testParamShaders2), DE_TRUE, DE_FALSE),
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), DE_FALSE, DE_TRUE),
			CacheTestParam(testParamShaders1, DE_LENGTH_OF_ARRAY(testParamShaders1), DE_FALSE, DE_TRUE),
			CacheTestParam(testParamShaders2, DE_LENGTH_OF_ARRAY(testParamShaders2), DE_FALSE, DE_TRUE),
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			graphicsTests->addChild(newTestCase<GraphicsCacheTest>(testCtx, &testParams[i]));

		cacheTests->addChild(graphicsTests.release());
	}

	// Compute Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> computeTests (new tcu::TestCaseGroup(testCtx, "compute_tests", "Test pipeline creation feedback with compute pipeline."));

		const VkShaderStageFlagBits testParamShaders0[] =
		{
			VK_SHADER_STAGE_COMPUTE_BIT,
		};
		const CacheTestParam testParams[] =
		{
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), DE_FALSE, DE_FALSE),
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), DE_TRUE, DE_FALSE),
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), DE_FALSE, DE_TRUE),
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			computeTests->addChild(newTestCase<ComputeCacheTest>(testCtx, &testParams[i]));

		cacheTests->addChild(computeTests.release());
	}

	return cacheTests.release();
}

} // pipeline

} // vkt
