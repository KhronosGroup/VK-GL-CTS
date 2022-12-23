/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
#include "vkQueryUtil.hpp"
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
	VK_MAX_PIPELINE_PARTS = 5,			// 4 parts + 1 final
};

enum
{
	PIPELINE_CACHE_NDX_NO_CACHE = 0,
	PIPELINE_CACHE_NDX_DERIVATIVE = 1,
	PIPELINE_CACHE_NDX_CACHED = 2,
	PIPELINE_CACHE_NDX_COUNT,
};

// helper functions

std::string getShaderFlagStr (const VkShaderStageFlags	shader,
							  bool						isDescription)
{
	std::ostringstream desc;
	if (shader & VK_SHADER_STAGE_COMPUTE_BIT)
	{
		desc << ((isDescription) ? "compute stage" : "compute_stage");
	}
	else
	{
		desc << ((isDescription) ? "vertex stage" : "vertex_stage");
		if (shader & VK_SHADER_STAGE_GEOMETRY_BIT)
			desc << ((isDescription) ? " geometry stage" : "_geometry_stage");
		if (shader & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
			desc << ((isDescription) ? " tessellation control stage" : "_tessellation_control_stage");
		if (shader & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
			desc << ((isDescription) ? " tessellation evaluation stage" : "_tessellation_evaluation_stage");
		desc << ((isDescription) ? " fragment stage" : "_fragment_stage");
	}

	return desc.str();
}

std::string getCaseStr (const deUint32 ndx)
{
	switch(ndx)
	{
		case PIPELINE_CACHE_NDX_NO_CACHE:
			return "No cached pipeline";
		case PIPELINE_CACHE_NDX_CACHED:
			return "Cached pipeline";
		case PIPELINE_CACHE_NDX_DERIVATIVE:
			return "Pipeline derivative";
		default:
			DE_FATAL("Unknown case!");
	}

	return "Unknown case";
}

// helper classes
class CacheTestParam
{
public:
								CacheTestParam				(const PipelineConstructionType	pipelineConstructionType,
															 const VkShaderStageFlags		shaders,
															 deBool							noCache,
															 deBool							delayedDestroy,
															 deBool							zeroOutFeedbackCount = VK_FALSE);
	virtual						~CacheTestParam				(void) = default;
	virtual const std::string	generateTestName			(void)	const;
	virtual const std::string	generateTestDescription		(void)	const;
	PipelineConstructionType	getPipelineConstructionType	(void)	const	{ return m_pipelineConstructionType; }
	VkShaderStageFlags			getShaderFlags				(void)	const	{ return m_shaders; }
	deBool						isCacheDisabled				(void)	const	{ return m_noCache; }
	deBool						isDelayedDestroy			(void)	const	{ return m_delayedDestroy; }
	deBool						isZeroOutFeedbackCount		(void)	const	{ return m_zeroOutFeedbackCount; }

protected:
	PipelineConstructionType	m_pipelineConstructionType;
	VkShaderStageFlags			m_shaders;
	bool						m_noCache;
	bool						m_delayedDestroy;
	bool						m_zeroOutFeedbackCount;
};

CacheTestParam::CacheTestParam (const PipelineConstructionType pipelineConstructionType, const VkShaderStageFlags shaders, deBool noCache, deBool delayedDestroy, deBool zeroOutFeedbackCount)
	: m_pipelineConstructionType	(pipelineConstructionType)
	, m_shaders						(shaders)
	, m_noCache						(noCache)
	, m_delayedDestroy				(delayedDestroy)
	, m_zeroOutFeedbackCount		(zeroOutFeedbackCount)
{
}

const std::string CacheTestParam::generateTestName (void) const
{
	std::string cacheString [] = { "", "_no_cache" };
	std::string delayedDestroyString [] = { "", "_delayed_destroy" };
	std::string zeroOutFeedbackCoutString [] = { "", "_zero_out_feedback_cout" };

	return getShaderFlagStr(m_shaders, false) + cacheString[m_noCache ? 1 : 0] + delayedDestroyString[m_delayedDestroy ? 1 : 0] + zeroOutFeedbackCoutString[m_zeroOutFeedbackCount ? 1 : 0];
}

const std::string CacheTestParam::generateTestDescription (void) const
{
	std::string result("Get pipeline creation feedback with " + getShaderFlagStr(m_shaders, true));
	if (m_noCache)
		result += " with no cache";
	if (m_delayedDestroy)
		result += " with delayed destroy";

	return result;
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
	void					preparePipelineWrapper		(GraphicsPipelineWrapper&		gpw,
														 ShaderWrapper					vertShaderModule,
														 ShaderWrapper					tescShaderModule,
														 ShaderWrapper					teseShaderModule,
														 ShaderWrapper					geomShaderModule,
														 ShaderWrapper					fragShaderModule,
														 VkPipelineCreationFeedbackEXT*	pipelineCreationFeedback,
														 bool*							pipelineCreationIsHeavy,
														 VkPipelineCreationFeedbackEXT*	pipelineStageCreationFeedbacks,
														 VkPipeline						basePipelineHandle,
														 VkBool32						zeroOutFeedbackCount);
	virtual tcu::TestStatus verifyTestResult			(void);
	void					clearFeedbacks				(void);

protected:
	const tcu::UVec2					m_renderSize;
	const VkFormat						m_colorFormat;
	const VkFormat						m_depthFormat;
	PipelineLayoutWrapper				m_pipelineLayout;

	RenderPassWrapper					m_renderPass;

	GraphicsPipelineWrapper				m_pipeline[PIPELINE_CACHE_NDX_COUNT];
	VkPipelineCreationFeedbackEXT		m_pipelineCreationFeedback[VK_MAX_PIPELINE_PARTS * PIPELINE_CACHE_NDX_COUNT];
	bool								m_pipelineCreationIsHeavy[VK_MAX_PIPELINE_PARTS * PIPELINE_CACHE_NDX_COUNT];
	VkPipelineCreationFeedbackEXT		m_pipelineStageCreationFeedbacks[PIPELINE_CACHE_NDX_COUNT * VK_MAX_SHADER_STAGES];
};

void GraphicsCacheTest::initPrograms (SourceCollections& programCollection) const
{
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
	programCollection.glslSources.add("color_frag") << glu::FragmentSource(
				"#version 310 es\n"
				"layout(location = 0) in highp vec4 vtxColor;\n"
				"layout(location = 0) out highp vec4 fragColor;\n"
				"void main (void)\n"
				"{\n"
				"  fragColor = vtxColor;\n"
				"}\n");

	VkShaderStageFlags shaderFlag = m_param.getShaderFlags();
	if (shaderFlag & VK_SHADER_STAGE_GEOMETRY_BIT)
	{
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
	}

	if (shaderFlag & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
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
	}

	if (shaderFlag & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
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
	}
}

void GraphicsCacheTest::checkSupport (Context& context) const
{
	if (m_param.getShaderFlags() & VK_SHADER_STAGE_GEOMETRY_BIT)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	if ((m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
		(m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_param.getPipelineConstructionType());
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
	, m_pipeline
	{
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), param->getPipelineConstructionType(), VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), param->getPipelineConstructionType(), VK_PIPELINE_CREATE_DERIVATIVE_BIT },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), param->getPipelineConstructionType(), VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT },
	}
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

		m_pipelineLayout = PipelineLayoutWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, &pipelineLayoutParams);
	}

	// Create render pass
	m_renderPass = RenderPassWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, m_colorFormat, m_depthFormat);

	// Create shader modules
	ShaderWrapper vertShaderModule1	= ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("color_vert_1"), 0);
	ShaderWrapper vertShaderModule2	= ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("color_vert_2"), 0);
	ShaderWrapper fragShaderModule	= ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("color_frag"), 0);
	ShaderWrapper tescShaderModule;
	ShaderWrapper teseShaderModule;
	ShaderWrapper geomShaderModule;

	VkShaderStageFlags shaderFlags = m_param->getShaderFlags();
	if (shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
		geomShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("unused_geo"), 0);
	if (shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		tescShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("basic_tcs"), 0);
	if (shaderFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		teseShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("basic_tes"), 0);

	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		ShaderWrapper vertShaderModule = (ndx == PIPELINE_CACHE_NDX_DERIVATIVE) ? vertShaderModule2 : vertShaderModule1;

		if (ndx == PIPELINE_CACHE_NDX_CACHED && !param->isDelayedDestroy())
		{
			// Destroy the NO_CACHE pipeline to check that the cached one really hits cache,
			// except for the case where we're testing cache hit of a pipeline still active.
			if (m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE].wasBuild())
				m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE].destroyPipeline();
		}

		clearFeedbacks();

		VkPipeline basePipeline = (ndx == PIPELINE_CACHE_NDX_DERIVATIVE && m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE].wasBuild()) ? m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE].getPipeline() : DE_NULL;

		preparePipelineWrapper(m_pipeline[ndx], vertShaderModule, tescShaderModule, teseShaderModule, geomShaderModule, fragShaderModule,
							   &m_pipelineCreationFeedback[VK_MAX_PIPELINE_PARTS * ndx],
							   &m_pipelineCreationIsHeavy[VK_MAX_PIPELINE_PARTS * ndx],
							   &m_pipelineStageCreationFeedbacks[VK_MAX_SHADER_STAGES * ndx],
							   basePipeline,
							   param->isZeroOutFeedbackCount());

		if (ndx != PIPELINE_CACHE_NDX_NO_CACHE)
		{
			// Destroy the pipeline as soon as it is created, except the NO_CACHE because
			// it is needed as a base pipeline for the derivative case.
			if (m_pipeline[ndx].wasBuild())
				m_pipeline[ndx].destroyPipeline();

			if (ndx == PIPELINE_CACHE_NDX_CACHED && param->isDelayedDestroy())
			{
				// Destroy the pipeline we didn't destroy earlier for the isDelayedDestroy case.
				if (m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE].wasBuild())
					m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE].destroyPipeline();
			}
		}
	}
}

GraphicsCacheTestInstance::~GraphicsCacheTestInstance (void)
{
}

void GraphicsCacheTestInstance::preparePipelineWrapper (GraphicsPipelineWrapper&		gpw,
														ShaderWrapper					vertShaderModule,
														ShaderWrapper					tescShaderModule,
														ShaderWrapper					teseShaderModule,
														ShaderWrapper					geomShaderModule,
														ShaderWrapper					fragShaderModule,
														VkPipelineCreationFeedbackEXT*	pipelineCreationFeedback,
														bool*							pipelineCreationIsHeavy,
														VkPipelineCreationFeedbackEXT*	pipelineStageCreationFeedbacks,
														VkPipeline						basePipelineHandle,
														VkBool32						zeroOutFeedbackCount)
{
	const VkVertexInputBindingDescription vertexInputBindingDescription
	{
		0u,										// deUint32				binding;
		sizeof(Vertex4RGBA),					// deUint32				strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2]
	{
		{
			0u,									// deUint32				location;
			0u,									// deUint32				binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat				format;
			0u									// deUint32				offsetInBytes;
		},
		{
			1u,									// deUint32				location;
			0u,									// deUint32				binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat				format;
			DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32				offsetInBytes;
		}
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateParams
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		1u,																// deUint32									vertexBindingDescriptionCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		2u,																// deUint32									vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,								// const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
	};

	const std::vector<VkViewport>	viewport	{ makeViewport(m_renderSize) };
	const std::vector<VkRect2D>		scissor		{ makeRect2D(m_renderSize) };

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState
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

	const VkPipelineColorBlendStateCreateInfo colorBlendStateParams
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

	VkPipelineDepthStencilStateCreateInfo depthStencilStateParams
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

	VkPipelineCreationFeedbackCreateInfoEXT		pipelineCreationFeedbackCreateInfo[VK_MAX_PIPELINE_PARTS];
	PipelineCreationFeedbackCreateInfoWrapper	pipelineCreationFeedbackWrapper[VK_MAX_PIPELINE_PARTS];
	for (deUint32 i = 0u ; i < VK_MAX_PIPELINE_PARTS ; ++i)
	{
		pipelineCreationFeedbackCreateInfo[i] = initVulkanStructure();
		pipelineCreationFeedbackCreateInfo[i].pPipelineCreationFeedback = &pipelineCreationFeedback[i];
		pipelineCreationFeedbackWrapper[i].ptr = &pipelineCreationFeedbackCreateInfo[i];

		pipelineCreationIsHeavy[i] = false;
	}

	deUint32 geometryStages = 1u + (geomShaderModule.isSet()) + (tescShaderModule.isSet()) + (teseShaderModule.isSet());
	if (m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		pipelineCreationFeedbackCreateInfo[4].pipelineStageCreationFeedbackCount	= zeroOutFeedbackCount ? 0u : (1u + geometryStages);
		pipelineCreationFeedbackCreateInfo[4].pPipelineStageCreationFeedbacks		= pipelineStageCreationFeedbacks;

		pipelineCreationIsHeavy[4] = true;
	}
	else
	{
		// setup proper stages count for CreationFeedback structures
		// that will be passed to pre-rasterization and fragment shader states
		pipelineCreationFeedbackCreateInfo[1].pipelineStageCreationFeedbackCount	= zeroOutFeedbackCount ? 0u : geometryStages;
		pipelineCreationFeedbackCreateInfo[1].pPipelineStageCreationFeedbacks		= pipelineStageCreationFeedbacks;
		pipelineCreationFeedbackCreateInfo[2].pipelineStageCreationFeedbackCount	= zeroOutFeedbackCount ? 0u : 1u;
		pipelineCreationFeedbackCreateInfo[2].pPipelineStageCreationFeedbacks		= pipelineStageCreationFeedbacks + geometryStages;

		pipelineCreationIsHeavy[1] = true;
		pipelineCreationIsHeavy[2] = true;

		if (m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
		{
			pipelineCreationIsHeavy[4] = true;
		}
	}

	// pipelineCreationIsHeavy element 0 and 3 intentionally left false,
	// because these relate to vertex input and fragment output stages, which may be
	// created in nearly zero time.

	gpw.setDefaultTopology((!tescShaderModule.isSet()) ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
	   .setDefaultRasterizationState()
	   .setDefaultMultisampleState()
	   .setupVertexInputState(&vertexInputStateParams, DE_NULL, *m_cache, pipelineCreationFeedbackWrapper[0])
	   .setupPreRasterizationShaderState(
			viewport,
			scissor,
			m_pipelineLayout,
			*m_renderPass,
			0u,
			vertShaderModule,
			DE_NULL,
			tescShaderModule,
			teseShaderModule,
			geomShaderModule,
			DE_NULL,
			nullptr,
			PipelineRenderingCreateInfoWrapper(),
			*m_cache,
			pipelineCreationFeedbackWrapper[1])
	   .setupFragmentShaderState(
			m_pipelineLayout,
			*m_renderPass,
			0u,
			fragShaderModule,
			&depthStencilStateParams,
			DE_NULL,
			DE_NULL,
			*m_cache,
			pipelineCreationFeedbackWrapper[2])
	   .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams, DE_NULL, *m_cache, pipelineCreationFeedbackWrapper[3])
	   .setMonolithicPipelineLayout(m_pipelineLayout)
	   .buildPipeline(*m_cache, basePipelineHandle, basePipelineHandle != DE_NULL ? -1 : 0, pipelineCreationFeedbackWrapper[4]);
}

tcu::TestStatus GraphicsCacheTestInstance::verifyTestResult (void)
{
	tcu::TestLog&	log						= m_context.getTestContext().getLog();
	bool			durationZeroWarning		= DE_FALSE;
	bool			cachedPipelineWarning	= DE_FALSE;
	bool			isMonolithic			= m_param->getPipelineConstructionType() == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC;
	bool			isZeroOutFeedbackCout	= m_param->isZeroOutFeedbackCount();
	deUint32		finalPipelineIndex		= deUint32(VK_MAX_PIPELINE_PARTS) - 1u;
	deUint32		start					= isMonolithic ? finalPipelineIndex : 0u;
	deUint32		step					= start + 1u;

	// Iterate ofer creation feedback for all pipeline parts - if monolithic pipeline is tested then skip (step over) feedback for parts
	for (deUint32 creationFeedbackNdx = start; creationFeedbackNdx < VK_MAX_PIPELINE_PARTS * PIPELINE_CACHE_NDX_COUNT; creationFeedbackNdx += step)
	{
		deUint32		pipelineCacheNdx		= creationFeedbackNdx / deUint32(VK_MAX_PIPELINE_PARTS);
		auto			creationFeedbackFlags	= m_pipelineCreationFeedback[creationFeedbackNdx].flags;
		std::string		caseString				= getCaseStr(pipelineCacheNdx);
		deUint32		pipelinePartIndex	= creationFeedbackNdx % deUint32(VK_MAX_PIPELINE_PARTS);

		std::ostringstream message;
		message << caseString;
		// Check first that the no cached pipeline was missed in the pipeline cache

		// According to the spec:
		// "An implementation should write pipeline creation feedback to pPipelineCreationFeedback and
		//	may write pipeline stage creation feedback to pPipelineStageCreationFeedbacks."
		if (!(creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
		{
			// According to the spec:
			// "If the VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT is not set in flags, an implementation
			//	must not set any other bits in flags, and all other VkPipelineCreationFeedbackEXT data members are undefined."
			if (m_pipelineCreationFeedback[creationFeedbackNdx].flags)
			{
				std::ostringstream			errorMsg;
				errorMsg << ": Creation feedback is not valid but there are other flags written";
				return tcu::TestStatus::fail(errorMsg.str());
			}
			message << "\t\t Pipeline Creation Feedback data is not valid\n";
		}
		else
		{
			if (m_param->isCacheDisabled() && creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
			{
				message << ": feedback indicates pipeline hit cache when it shouldn't";
				return tcu::TestStatus::fail(message.str());
			}

			if (pipelineCacheNdx == PIPELINE_CACHE_NDX_NO_CACHE && creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
			{
				message << ": hit the cache when it shouldn't";
				return tcu::TestStatus::fail(message.str());
			}

			if (pipelineCacheNdx != PIPELINE_CACHE_NDX_DERIVATIVE && creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT)
			{
				message << ": feedback indicates base pipeline acceleration when it shouldn't";
				return tcu::TestStatus::fail(message.str());
			}

			if (pipelineCacheNdx == PIPELINE_CACHE_NDX_CACHED && !m_param->isCacheDisabled() && (creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
			{
				// For graphics pipeline library cache is only hit for the pre_rasterization and fragment_shader stages
				if (isMonolithic || (pipelinePartIndex == 1u) || (pipelinePartIndex == 2u))
				{
					message << "\nWarning: Cached pipeline case did not hit the cache";
					cachedPipelineWarning = DE_TRUE;
				}
			}

			if (m_pipelineCreationFeedback[creationFeedbackNdx].duration == 0)
			{
				if (m_pipelineCreationIsHeavy[creationFeedbackNdx])
				{
					// Emit warnings only for pipelines, that are expected to have large creation times.
					// Pipelines containing only vertex input or fragment output stages may be created in
					// time duration less than the timer precision available on given platform.

					message << "\nWarning: Pipeline creation feedback reports duration spent creating a pipeline was zero nanoseconds\n";
					durationZeroWarning = DE_TRUE;
				}
			}

			message << "\n";
			message << "\t\t Hit cache ? \t\t\t"				<< (creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT ? "yes" : "no")	<< "\n";
			message << "\t\t Base Pipeline Acceleration ? \t"	<< (creationFeedbackFlags & VK_PIPELINE_CREATION_FEEDBACK_BASE_PIPELINE_ACCELERATION_BIT_EXT ?	 "yes" : "no")		<< "\n";
			message << "\t\t Duration (ns): \t\t"				<< m_pipelineCreationFeedback[creationFeedbackNdx].duration																							<< "\n";
		}

		// dont repeat checking shader feedback for pipeline parts - just check all shaders when checkin final pipelines
		if (pipelinePartIndex == finalPipelineIndex)
		{
			VkShaderStageFlags	testedShaderFlags	= m_param->getShaderFlags();
			deUint32			shaderCount			= 2u + ((testedShaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT) != 0) +
														   ((testedShaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != 0) +
														   ((testedShaderFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != 0);
			for(deUint32 shader = 0; shader < shaderCount; shader++)
			{
				const deUint32 index = VK_MAX_SHADER_STAGES * pipelineCacheNdx + shader;
				message << "\t" <<(shader + 1) << " shader stage\n";

				// According to the spec:
				// "An implementation should write pipeline creation feedback to pPipelineCreationFeedback and
				//      may write pipeline stage creation feedback to pPipelineStageCreationFeedbacks."
				if (m_pipelineStageCreationFeedbacks[index].flags & isZeroOutFeedbackCout)
				{
					std::ostringstream			errorMsg;
					errorMsg << caseString << ": feedback indicates pipeline " << (shader + 1) << " shader stage feedback was generated despite setting feedback count to zero";
					return tcu::TestStatus::fail(errorMsg.str());
				}

				if (!(m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
				{
					// According to the spec:
					// "If the VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT is not set in flags, an implementation
					//      must not set any other bits in flags, and all other VkPipelineCreationFeedbackEXT data members are undefined."
					if (m_pipelineStageCreationFeedbacks[index].flags)
					{
						std::ostringstream			errorMsg;
						errorMsg << caseString << ": Creation feedback is not valid for " << (shader + 1) << " shader stage but there are other flags written";
						return tcu::TestStatus::fail(errorMsg.str());
					}
					message << "\t\t Pipeline Creation Feedback data is not valid\n";
					continue;
				}
				if (m_param->isCacheDisabled() && m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT)
				{
					std::ostringstream			errorMsg;
					errorMsg << caseString << ": feedback indicates pipeline " << (shader + 1) << " shader stage hit cache when it shouldn't";
					return tcu::TestStatus::fail(errorMsg.str());
				}

				if (pipelineCacheNdx == PIPELINE_CACHE_NDX_CACHED && !m_param->isCacheDisabled() && (m_pipelineStageCreationFeedbacks[index].flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT) == 0)
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

void GraphicsCacheTestInstance::clearFeedbacks(void)
{
	deMemset(m_pipelineCreationFeedback, 0, sizeof(VkPipelineCreationFeedbackEXT) * VK_MAX_PIPELINE_PARTS * PIPELINE_CACHE_NDX_COUNT);
	deMemset(m_pipelineStageCreationFeedbacks, 0, sizeof(VkPipelineCreationFeedbackEXT) * PIPELINE_CACHE_NDX_COUNT * VK_MAX_SHADER_STAGES);
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
	de::MovePtr<Allocation>			m_inputBufferAlloc;
	Move<VkShaderModule>			m_computeShaderModule[PIPELINE_CACHE_NDX_COUNT];

	Move<VkBuffer>					m_outputBuf[PIPELINE_CACHE_NDX_COUNT];
	de::MovePtr<Allocation>			m_outputBufferAlloc[PIPELINE_CACHE_NDX_COUNT];

	Move<VkDescriptorPool>			m_descriptorPool[PIPELINE_CACHE_NDX_COUNT];
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout[PIPELINE_CACHE_NDX_COUNT];
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
		0u,																			// VkShaderModuleCreateFlags	flags;
		m_context.getBinaryCollection().get(shader_name).getSize(),					// deUintptr					codeSize;
		(deUint32*)m_context.getBinaryCollection().get(shader_name).getBinary(),	// const deUint32*				pCode;
	};
	m_computeShaderModule[ndx] = createShaderModule(vk, vkDevice, &shaderModuleCreateInfo);
}

void ComputeCacheTestInstance::buildPipeline (const CacheTestParam*	param, deUint32 ndx)
{
	const DeviceInterface&	vk					 = m_context.getDeviceInterface();
	const VkDevice			vkDevice			 = m_context.getDevice();
	const VkBool32			zeroOutFeedbackCount = param->isZeroOutFeedbackCount();

	deMemset(&m_pipelineCreationFeedback[ndx], 0, sizeof(VkPipelineCreationFeedbackEXT));
	deMemset(&m_pipelineStageCreationFeedback[ndx], 0, sizeof(VkPipelineCreationFeedbackEXT));

	const VkPipelineCreationFeedbackCreateInfoEXT pipelineCreationFeedbackCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT,	// VkStructureType					sType;
		DE_NULL,														// const void *						pNext;
		&m_pipelineCreationFeedback[ndx],								// VkPipelineCreationFeedbackEXT*	pPipelineCreationFeedback;
		zeroOutFeedbackCount ? 0u : 1u,									// deUint32							pipelineStageCreationFeedbackCount;
		&m_pipelineStageCreationFeedback[ndx]							// VkPipelineCreationFeedbackEXT*	pPipelineStageCreationFeedbacks;
	};

	// Create compute pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,					// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		0u,																// VkPipelineLayoutCreateFlags		flags;
		1u,																// deUint32							setLayoutCount;
		&m_descriptorSetLayout[ndx].get(),								// const VkDescriptorSetLayout*		pSetLayouts;
		0u,																// deUint32							pushConstantRangeCount;
		DE_NULL,														// const VkPushConstantRange*		pPushConstantRanges;
	};

	m_pipelineLayout[ndx] = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

	const VkPipelineShaderStageCreateInfo stageCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		0u,																// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,									// VkShaderStageFlagBits			stage;
		*m_computeShaderModule[ndx],									// VkShaderModule					module;
		"main",															// const char*						pName;
		DE_NULL,														// const VkSpecializationInfo*		pSpecializationInfo;
	};

	VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,					// VkStructureType					sType;
		&pipelineCreationFeedbackCreateInfo,							// const void*						pNext;
		0u,																// VkPipelineCreateFlags			flags;
		stageCreateInfo,												// VkPipelineShaderStageCreateInfo	stage;
		*m_pipelineLayout[ndx],											// VkPipelineLayout					layout;
		(VkPipeline)0,													// VkPipeline						basePipelineHandle;
		0u,																// deInt32							basePipelineIndex;
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

		// According to the spec:
		// "An implementation should write pipeline creation feedback to pPipelineCreationFeedback and
		//	may write pipeline stage creation feedback to pPipelineStageCreationFeedbacks."
		if (!(m_pipelineCreationFeedback[ndx].flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT))
		{
			// According to the spec:
			// "If the VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT is not set in flags, an implementation
			//	must not set any other bits in flags, and all other VkPipelineCreationFeedbackEXT data members are undefined."
			if (m_pipelineCreationFeedback[ndx].flags)
			{
				std::ostringstream			errorMsg;
				errorMsg << ": Creation feedback is not valid but there are other flags written";
				return tcu::TestStatus::fail(errorMsg.str());
			}
			message << "\t\t Pipeline Creation Feedback data is not valid\n";
		}
		else
		{
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
		}

		// According to the spec:
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

tcu::TestCaseGroup* createCreationFeedbackTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> cacheTests (new tcu::TestCaseGroup(testCtx, "creation_feedback", "pipeline creation feedback tests"));

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests (new tcu::TestCaseGroup(testCtx, "graphics_tests", "Test pipeline creation feedback with graphics pipeline."));

		const VkShaderStageFlags vertFragStages		= VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		const VkShaderStageFlags vertGeomFragStages	= vertFragStages | VK_SHADER_STAGE_GEOMETRY_BIT;
		const VkShaderStageFlags vertTessFragStages	= vertFragStages | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

		const std::vector<CacheTestParam> testParams
		{
			{ pipelineConstructionType, vertFragStages,		DE_FALSE, DE_FALSE },
			{ pipelineConstructionType, vertGeomFragStages,	DE_FALSE, DE_FALSE },
			{ pipelineConstructionType, vertTessFragStages,	DE_FALSE, DE_FALSE },
			{ pipelineConstructionType, vertFragStages,		DE_TRUE,  DE_FALSE },
			{ pipelineConstructionType, vertFragStages,		DE_TRUE,  DE_FALSE, DE_TRUE },
			{ pipelineConstructionType, vertGeomFragStages,	DE_TRUE,  DE_FALSE },
			{ pipelineConstructionType, vertTessFragStages,	DE_TRUE,  DE_FALSE },
			{ pipelineConstructionType, vertFragStages,		DE_FALSE, DE_TRUE },
			{ pipelineConstructionType, vertGeomFragStages,	DE_FALSE, DE_TRUE },
			{ pipelineConstructionType, vertTessFragStages,	DE_FALSE, DE_TRUE },
		};

		for (auto& param : testParams)
			graphicsTests->addChild(newTestCase<GraphicsCacheTest>(testCtx, &param));

		cacheTests->addChild(graphicsTests.release());
	}

	// Compute Pipeline Tests - don't repeat those tests for graphics pipeline library
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		de::MovePtr<tcu::TestCaseGroup> computeTests (new tcu::TestCaseGroup(testCtx, "compute_tests", "Test pipeline creation feedback with compute pipeline."));

		const std::vector<CacheTestParam> testParams
		{
			{ pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, DE_FALSE },
			{ pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, DE_FALSE },
			{ pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, DE_TRUE },
		};

		for (auto& param : testParams)
			computeTests->addChild(newTestCase<ComputeCacheTest>(testCtx, &param));

		cacheTests->addChild(computeTests.release());
	}

	return cacheTests.release();
}

} // pipeline

} // vkt
