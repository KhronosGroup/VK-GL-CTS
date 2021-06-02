/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 ARM Ltd.
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
 * \brief Timestamp Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineTimestampTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <sstream>
#include <vector>
#include <set>
#include <cctype>
#include <locale>
#include <limits>
#include <thread>
#include <chrono>
#include <time.h>
#include <algorithm>

#if (DE_OS == DE_OS_WIN32)
#	define VC_EXTRALEAN
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <windows.h>
#endif

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{
typedef std::vector<VkPipelineStageFlagBits> StageFlagVector;

// helper functions
#define GEN_DESC_STRING(name,postfix)                                      \
		do {                                                               \
		   for (std::string::size_type ndx = 0; ndx<strlen(#name); ++ndx)  \
			 if(isDescription && #name[ndx] == '_')                        \
			   desc << " ";                                                \
			 else                                                          \
			   desc << std::tolower(#name[ndx],loc);                       \
		   if (isDescription)                                              \
			 desc << " " << #postfix;                                      \
		   else                                                            \
			 desc << "_" << #postfix;                                      \
		} while (deGetFalse())

std::string getPipelineStageFlagStr (const VkPipelineStageFlagBits stage,
									 bool                          isDescription)
{
	std::ostringstream desc;
	std::locale loc;
	switch(stage)
	{
#define STAGE_CASE(p)                              \
		case VK_PIPELINE_STAGE_##p##_BIT:          \
		{                                          \
			GEN_DESC_STRING(p, stage);             \
			break;                                 \
		}
		STAGE_CASE(TOP_OF_PIPE);
		STAGE_CASE(DRAW_INDIRECT);
		STAGE_CASE(VERTEX_INPUT);
		STAGE_CASE(VERTEX_SHADER);
		STAGE_CASE(TESSELLATION_CONTROL_SHADER);
		STAGE_CASE(TESSELLATION_EVALUATION_SHADER);
		STAGE_CASE(GEOMETRY_SHADER);
		STAGE_CASE(FRAGMENT_SHADER);
		STAGE_CASE(EARLY_FRAGMENT_TESTS);
		STAGE_CASE(LATE_FRAGMENT_TESTS);
		STAGE_CASE(COLOR_ATTACHMENT_OUTPUT);
		STAGE_CASE(COMPUTE_SHADER);
		STAGE_CASE(TRANSFER);
		STAGE_CASE(HOST);
		STAGE_CASE(ALL_GRAPHICS);
		STAGE_CASE(ALL_COMMANDS);
#undef STAGE_CASE
	  default:
		desc << "unknown stage!";
		DE_FATAL("Unknown Stage!");
		break;
	};

	return desc.str();
}

enum TransferMethod
{
	TRANSFER_METHOD_COPY_BUFFER = 0,
	TRANSFER_METHOD_COPY_IMAGE,
	TRANSFER_METHOD_BLIT_IMAGE,
	TRANSFER_METHOD_COPY_BUFFER_TO_IMAGE,
	TRANSFER_METHOD_COPY_IMAGE_TO_BUFFER,
	TRANSFER_METHOD_UPDATE_BUFFER,
	TRANSFER_METHOD_FILL_BUFFER,
	TRANSFER_METHOD_CLEAR_COLOR_IMAGE,
	TRANSFER_METHOD_CLEAR_DEPTH_STENCIL_IMAGE,
	TRANSFER_METHOD_RESOLVE_IMAGE,
	TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS,
	TRANSFER_METHOD_LAST
};

std::string getTransferMethodStr (const TransferMethod	method,
								  bool					isDescription)
{
	std::ostringstream	desc;
	std::locale			loc;

	switch(method)
	{
#define METHOD_CASE(p)                             \
		case TRANSFER_METHOD_##p:                  \
		{                                          \
			GEN_DESC_STRING(p, method);            \
			break;                                 \
		}
	  METHOD_CASE(COPY_BUFFER)
	  METHOD_CASE(COPY_IMAGE)
	  METHOD_CASE(BLIT_IMAGE)
	  METHOD_CASE(COPY_BUFFER_TO_IMAGE)
	  METHOD_CASE(COPY_IMAGE_TO_BUFFER)
	  METHOD_CASE(UPDATE_BUFFER)
	  METHOD_CASE(FILL_BUFFER)
	  METHOD_CASE(CLEAR_COLOR_IMAGE)
	  METHOD_CASE(CLEAR_DEPTH_STENCIL_IMAGE)
	  METHOD_CASE(RESOLVE_IMAGE)
	  METHOD_CASE(COPY_QUERY_POOL_RESULTS)
#undef METHOD_CASE
	  default:
		desc << "unknown method!";
		DE_FATAL("Unknown method!");
		break;
	};

	return desc.str();
}

constexpr deUint32 MIN_TIMESTAMP_VALID_BITS = 36;
constexpr deUint32 MAX_TIMESTAMP_VALID_BITS = 64;

// Checks the number of valid bits for the given queue meets the spec requirements.
void checkValidBits (deUint32 validBits, deUint32 queueFamilyIndex)
{
	if (validBits < MIN_TIMESTAMP_VALID_BITS || validBits > MAX_TIMESTAMP_VALID_BITS)
	{
		std::ostringstream msg;
		msg << "Invalid value for timestampValidBits (" << validBits << ") in queue index " << queueFamilyIndex;
		TCU_FAIL(msg.str());
	}
}

// Returns the timestamp mask given the number of valid timestamp bits.
deUint64 timestampMaskFromValidBits (deUint32 validBits)
{
	return ((validBits == MAX_TIMESTAMP_VALID_BITS) ? std::numeric_limits<deUint64>::max() : ((1ULL << validBits) - 1));
}

// Checks support for timestamps and returns the timestamp mask.
deUint64 checkTimestampsSupported (Context& context)
{
	const InstanceInterface&					vki					= context.getInstanceInterface();
	const VkPhysicalDevice						physDevice			= context.getPhysicalDevice();
	const deUint32								queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const std::vector<VkQueueFamilyProperties>	queueProperties		= vk::getPhysicalDeviceQueueFamilyProperties(vki, physDevice);
	DE_ASSERT(queueFamilyIndex < queueProperties.size());
	const deUint32&								validBits			= queueProperties[queueFamilyIndex].timestampValidBits;

	if (validBits == 0)
		throw tcu::NotSupportedError("Universal queue does not support timestamps");

	checkValidBits(validBits, queueFamilyIndex);
	return timestampMaskFromValidBits(validBits);
}

void checkTimestampBits (deUint64 timestamp, deUint64 mask)
{
	// The spec says:
	// timestampValidBits is the unsigned integer count of meaningful bits in
	// the timestamps written via vkCmdWriteTimestamp. The valid range for the
	// count is 36..64 bits, or a value of 0, indicating no support for
	// timestamps. Bits outside the valid range are guaranteed to be zeros.
	if (timestamp > mask)
	{
		std::ostringstream msg;
		msg << std::hex << "Invalid device timestamp value 0x" << timestamp << " according to device timestamp mask 0x" << mask;
		TCU_FAIL(msg.str());
	}
}

// helper classes
class TimestampTestParam
{
public:
								TimestampTestParam		(const VkPipelineStageFlagBits*	stages,
														 const deUint32					stageCount,
														 const bool						inRenderPass,
														 const bool						hostQueryReset,
														 const VkQueryResultFlags		queryResultFlags);
								~TimestampTestParam		(void);
	virtual const std::string	generateTestName		(void) const;
	virtual const std::string	generateTestDescription	(void) const;
	StageFlagVector				getStageVector			(void) const	{ return m_stageVec; }
	bool						getInRenderPass			(void) const	{ return m_inRenderPass; }
	bool						getHostQueryReset		(void) const	{ return m_hostQueryReset; }
	VkQueryResultFlags			getQueryResultFlags		(void) const	{ return m_queryResultFlags; }
	void						toggleInRenderPass		(void)			{ m_inRenderPass = !m_inRenderPass; }
	void						toggleHostQueryReset	(void)			{ m_hostQueryReset = !m_hostQueryReset; }

	void						setQueryResultFlags		(VkQueryResultFlags flags)	{ m_queryResultFlags = flags; }

protected:
	StageFlagVector			m_stageVec;
	bool					m_inRenderPass;
	bool					m_hostQueryReset;
	VkQueryResultFlags	m_queryResultFlags;

};

TimestampTestParam::TimestampTestParam (const VkPipelineStageFlagBits*	stages,
									    const deUint32					stageCount,
									    const bool						inRenderPass,
									    const bool						hostQueryReset,
										const VkQueryResultFlags		queryResultFlags)
	: m_inRenderPass(inRenderPass)
	, m_hostQueryReset(hostQueryReset)
	, m_queryResultFlags(queryResultFlags)
{
	for (deUint32 ndx = 0; ndx < stageCount; ndx++)
	{
		m_stageVec.push_back(stages[ndx]);
	}
}

TimestampTestParam::~TimestampTestParam (void)
{
}

const std::string TimestampTestParam::generateTestName (void) const
{
	std::string result("");

	for (StageFlagVector::const_iterator it = m_stageVec.begin(); it != m_stageVec.end(); it++)
	{
		if(*it != VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
		{
			result += getPipelineStageFlagStr(*it, false) + '_';
		}
	}
	if(m_inRenderPass)
		result += "in_render_pass";
	else
		result += "out_of_render_pass";

	if(m_hostQueryReset)
		result += "_host_query_reset";

	if (m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
		result += "_with_availability_bit";

	return result;
}

const std::string TimestampTestParam::generateTestDescription (void) const
{
	std::string result("Record timestamp after ");

	for (StageFlagVector::const_iterator it = m_stageVec.begin(); it != m_stageVec.end(); it++)
	{
		if(*it != VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
		{
			result += getPipelineStageFlagStr(*it, true) + ' ';
		}
	}
	if(m_inRenderPass)
		result += " in the renderpass";
	else
		result += " out of the render pass";

	if(m_hostQueryReset)
		result += "and the host resets query pool";

	return result;
}

class TransferTimestampTestParam : public TimestampTestParam
{
public:
						TransferTimestampTestParam	(const VkPipelineStageFlagBits*	stages,
													 const deUint32					stageCount,
													 const bool						inRenderPass,
													 const bool						hostQueryReset,
													 const deUint32					methodNdx,
													 const VkQueryResultFlags		flags);
						~TransferTimestampTestParam	(void) { }
	const std::string	generateTestName			(void) const;
	const std::string	generateTestDescription		(void) const;
	TransferMethod		getMethod					(void) const { return m_method; }
protected:
	TransferMethod		m_method;
};

TransferTimestampTestParam::TransferTimestampTestParam (const VkPipelineStageFlagBits*	stages,
													    const deUint32					stageCount,
													    const bool						inRenderPass,
													    const bool						hostQueryReset,
													    const deUint32					methodNdx,
														const VkQueryResultFlags		flags)
	: TimestampTestParam(stages, stageCount, inRenderPass, hostQueryReset, flags)
{
	DE_ASSERT(methodNdx < (deUint32)TRANSFER_METHOD_LAST);

	m_method = (TransferMethod)methodNdx;
}

const std::string TransferTimestampTestParam::generateTestName (void) const
{
	std::string result("");

	for (StageFlagVector::const_iterator it = m_stageVec.begin(); it != m_stageVec.end(); it++)
	{
		if(*it != VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
		{
			result += getPipelineStageFlagStr(*it, false) + '_';
		}
	}

	result += "with_" + getTransferMethodStr(m_method, false);

	if(m_hostQueryReset)
		result += "_host_query_reset";

	if (m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
		result += "_with_availability_bit";

	return result;
}

const std::string TransferTimestampTestParam::generateTestDescription (void) const
{
	std::string result("");

	for (StageFlagVector::const_iterator it = m_stageVec.begin(); it != m_stageVec.end(); it++)
	{
		if(*it != VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
		{
			result += getPipelineStageFlagStr(*it, true) + ' ';
		}
	}

	result += "with " + getTransferMethodStr(m_method, true);

	if(m_hostQueryReset)
		result += "and the host resets query pool";

	return result;
}

class TwoCmdBuffersTestParam : public TimestampTestParam
{
public:
							TwoCmdBuffersTestParam	(const VkPipelineStageFlagBits*	stages,
													 const deUint32					stageCount,
													 const bool						inRenderPass,
													 const bool						hostQueryReset,
													 const VkCommandBufferLevel		cmdBufferLevel,
													 const VkQueryResultFlags		queryPoolResultFlags);
							~TwoCmdBuffersTestParam	(void) { }
	VkCommandBufferLevel	getCmdBufferLevel		(void) const { return m_cmdBufferLevel; }
protected:
	VkCommandBufferLevel	m_cmdBufferLevel;
};

TwoCmdBuffersTestParam::TwoCmdBuffersTestParam	(const VkPipelineStageFlagBits*	stages,
												 const deUint32					stageCount,
												 const bool						inRenderPass,
												 const bool						hostQueryReset,
												 const VkCommandBufferLevel		cmdBufferLevel,
												 const VkQueryResultFlags		queryPoolResultFlags)
: TimestampTestParam(stages, stageCount, inRenderPass, hostQueryReset, queryPoolResultFlags), m_cmdBufferLevel(cmdBufferLevel)
{
}

class SimpleGraphicsPipelineBuilder
{
public:
						 SimpleGraphicsPipelineBuilder  (Context& context);
						 ~SimpleGraphicsPipelineBuilder (void) { }

	void				bindShaderStage					(VkShaderStageFlagBits	stage,
														 const char*			source_name);

	void				enableTessellationStage			(deUint32              patchControlPoints);

	Move<VkPipeline>	buildPipeline					(tcu::UVec2            renderSize,
														 VkRenderPass          renderPass);
protected:
	enum
	{
		VK_MAX_SHADER_STAGES = 6,
	};

	Context&				m_context;

	Move<VkShaderModule>	m_shaderModules[VK_MAX_SHADER_STAGES];
	deUint32				m_shaderStageCount;
	VkShaderStageFlagBits	m_shaderStages[VK_MAX_SHADER_STAGES];

	deUint32				m_patchControlPoints;

	Move<VkPipelineLayout>	m_pipelineLayout;
	Move<VkPipeline>		m_graphicsPipelines;

};

SimpleGraphicsPipelineBuilder::SimpleGraphicsPipelineBuilder(Context& context)
	: m_context(context)
{
	m_patchControlPoints = 0;
	m_shaderStageCount   = 0;
}

void SimpleGraphicsPipelineBuilder::bindShaderStage (VkShaderStageFlagBits	stage,
													 const char*			source_name)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create shader module
	deUint32*				pCode		= (deUint32*)m_context.getBinaryCollection().get(source_name).getBinary();
	deUint32				codeSize	= (deUint32)m_context.getBinaryCollection().get(source_name).getSize();

	const VkShaderModuleCreateInfo moduleCreateInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,	// VkStructureType             sType;
		DE_NULL,										// const void*                 pNext;
		0u,												// VkShaderModuleCreateFlags   flags;
		codeSize,										// deUintptr                   codeSize;
		pCode,											// const deUint32*             pCode;
	};

	m_shaderModules[m_shaderStageCount] = createShaderModule(vk, vkDevice, &moduleCreateInfo);
	m_shaderStages[m_shaderStageCount] = stage;

	m_shaderStageCount++;
}

Move<VkPipeline> SimpleGraphicsPipelineBuilder::buildPipeline (tcu::UVec2 renderSize, VkRenderPass renderPass)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			vkDevice				= m_context.getDevice();
	VkShaderModule			vertShaderModule		= DE_NULL;
	VkShaderModule			tessControlShaderModule	= DE_NULL;
	VkShaderModule			tessEvalShaderModule	= DE_NULL;
	VkShaderModule			geomShaderModule		= DE_NULL;
	VkShaderModule			fragShaderModule		= DE_NULL;

	for (deUint32 i = 0; i < m_shaderStageCount; i++)
	{
		if (m_shaderStages[i] == VK_SHADER_STAGE_VERTEX_BIT)
			vertShaderModule = *m_shaderModules[i];
		else if (m_shaderStages[i] == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
			tessControlShaderModule = *m_shaderModules[i];
		else if (m_shaderStages[i] == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
			tessEvalShaderModule = *m_shaderModules[i];
		else if (m_shaderStages[i] == VK_SHADER_STAGE_GEOMETRY_BIT)
			geomShaderModule = *m_shaderModules[i];
		else if (m_shaderStages[i] == VK_SHADER_STAGE_FRAGMENT_BIT)
			fragShaderModule = *m_shaderModules[i];
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                  sType;
			DE_NULL,										// const void*                      pNext;
			0u,												// VkPipelineLayoutCreateFlags      flags;
			0u,												// deUint32                         setLayoutCount;
			DE_NULL,										// const VkDescriptorSetLayout*     pSetLayouts;
			0u,												// deUint32                         pushConstantRangeCount;
			DE_NULL											// const VkPushConstantRange*       pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create pipeline
	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,								// deUint32                 binding;
		sizeof(Vertex4RGBA),			// deUint32                 strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate        inputRate;
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
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                          sType;
		DE_NULL,													// const void*                              pNext;
		0u,															// VkPipelineVertexInputStateCreateFlags    flags;
		1u,															// deUint32                                 vertexBindingDescriptionCount;
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*   pVertexBindingDescriptions;
		2u,															// deUint32                                 vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,							// const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
	};

	VkPrimitiveTopology primitiveTopology = (m_patchControlPoints > 0) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	const std::vector<VkViewport>	viewports	(1, makeViewport(renderSize));
	const std::vector<VkRect2D>		scissors	(1, makeRect2D(renderSize));

	VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType;
		DE_NULL,													// const void*                              pNext;
		0u,															// VkPipelineDepthStencilStateCreateFlags   flags;
		VK_TRUE,													// VkBool32                                 depthTestEnable;
		VK_TRUE,													// VkBool32                                 depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp                              depthCompareOp;
		VK_FALSE,													// VkBool32                                 depthBoundsTestEnable;
		VK_FALSE,													// VkBool32                                 stencilTestEnable;
		// VkStencilOpState front;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp  failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp  passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp  depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp  compareOp;
			0u,						// deUint32     compareMask;
			0u,						// deUint32     writeMask;
			0u,						// deUint32     reference;
		},
		// VkStencilOpState back;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp  failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp  passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp  depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp  compareOp;
			0u,						// deUint32     compareMask;
			0u,						// deUint32     writeMask;
			0u,						// deUint32     reference;
		},
		0.0f,														// float                                    minDepthBounds;
		1.0f,														// float                                    maxDepthBounds;
	};

	return makeGraphicsPipeline(vk,							// const DeviceInterface&                        vk
								vkDevice,					// const VkDevice                                device
								*m_pipelineLayout,			// const VkPipelineLayout                        pipelineLayout
								vertShaderModule,			// const VkShaderModule                          vertexShaderModule
								tessControlShaderModule,	// const VkShaderModule                          tessellationControlModule
								tessEvalShaderModule,		// const VkShaderModule                          tessellationEvalModule
								geomShaderModule,			// const VkShaderModule                          geometryShaderModule
								fragShaderModule,			// const VkShaderModule                          fragmentShaderModule
								renderPass,					// const VkRenderPass                            renderPass
								viewports,					// const std::vector<VkViewport>&                viewports
								scissors,					// const std::vector<VkRect2D>&                  scissors
								primitiveTopology,			// const VkPrimitiveTopology                     topology
								0u,							// const deUint32                                subpass
								m_patchControlPoints,		// const deUint32                                patchControlPoints
								&vertexInputStateParams,	// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								DE_NULL,					// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								DE_NULL,					// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
								&depthStencilStateParams);	// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
}

void SimpleGraphicsPipelineBuilder::enableTessellationStage (deUint32 patchControlPoints)
{
	m_patchControlPoints = patchControlPoints;
}

template <class Test>
vkt::TestCase* newTestCase	(tcu::TestContext&		testContext,
							 TimestampTestParam*	testParam)
{
	return new Test(testContext,
					testParam->generateTestName().c_str(),
					testParam->generateTestDescription().c_str(),
					testParam);
}

// Test Classes
class TimestampTest : public vkt::TestCase
{
public:
	enum
	{
		ENTRY_COUNT = 8
	};

							TimestampTest	(tcu::TestContext&			testContext,
											 const std::string&			name,
											 const std::string&			description,
											 const TimestampTestParam*	param)
											 : vkt::TestCase		(testContext, name, description)
											 , m_stages				(param->getStageVector())
											 , m_inRenderPass		(param->getInRenderPass())
											 , m_hostQueryReset		(param->getHostQueryReset())
											 , m_queryResultFlags	(param->getQueryResultFlags())
											 { }
	virtual					~TimestampTest	(void) { }
	virtual void			initPrograms	(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance	(Context& context) const;
	virtual void			checkSupport	(Context& context) const;
protected:
	const StageFlagVector			m_stages;
	const bool						m_inRenderPass;
	const bool						m_hostQueryReset;
	const VkQueryResultFlags		m_queryResultFlags;
};

class TimestampTestInstance : public vkt::TestInstance
{
public:
							TimestampTestInstance		(Context&						context,
														 const StageFlagVector&			stages,
														 const bool						inRenderPass,
														 const bool						hostQueryReset,
														 const VkQueryResultFlags		queryResultFlags);

	virtual					~TimestampTestInstance		(void);
	virtual tcu::TestStatus	iterate						(void);

protected:
	virtual tcu::TestStatus	verifyTimestamp				(void);
	virtual void            configCommandBuffer			(void);

	Move<VkBuffer>			createBufferAndBindMemory	(VkDeviceSize				size,
														 VkBufferUsageFlags			usage,
														 de::MovePtr<Allocation>*	pAlloc);

	Move<VkImage>			createImage2DAndBindMemory	(VkFormat					format,
														 deUint32					width,
														 deUint32					height,
														 VkImageUsageFlags			usage,
														 VkSampleCountFlagBits		sampleCount,
														 de::MovePtr<Allocation>*	pAlloc);

protected:
	const StageFlagVector	m_stages;
	bool					m_inRenderPass;
	bool					m_hostQueryReset;
	VkQueryResultFlags		m_queryResultFlags;

	Move<VkCommandPool>		m_cmdPool;
	Move<VkCommandBuffer>	m_cmdBuffer;
	Move<VkQueryPool>		m_queryPool;
	deUint64*				m_timestampValues;
	deUint64*				m_timestampValuesHostQueryReset;
	deUint64				m_timestampMask;
};

void TimestampTest::initPrograms (SourceCollections& programCollection) const
{
	vkt::TestCase::initPrograms(programCollection);
}

void TimestampTest::checkSupport (Context& context) const
{
	// Check support for timestamp queries
	{
		const deUint32								queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
		const std::vector<VkQueueFamilyProperties>	queueProperties		= vk::getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

		DE_ASSERT(queueFamilyIndex < (deUint32)queueProperties.size());

		if (!queueProperties[queueFamilyIndex].timestampValidBits)
			throw tcu::NotSupportedError("Universal queue does not support timestamps");
	}

	if (m_hostQueryReset)
	{
		// Check VK_EXT_host_query_reset is supported
		context.requireDeviceFunctionality("VK_EXT_host_query_reset");

		if(context.getHostQueryResetFeatures().hostQueryReset == VK_FALSE)
			throw tcu::NotSupportedError("Implementation doesn't support resetting queries from the host");
	}
}

TestInstance* TimestampTest::createInstance (Context& context) const
{
	return new TimestampTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, m_queryResultFlags);
}

TimestampTestInstance::TimestampTestInstance (Context&						context,
											  const StageFlagVector&		stages,
											  const bool					inRenderPass,
											  const bool					hostQueryReset,
											  const VkQueryResultFlags		queryResultFlags)
	: TestInstance			(context)
	, m_stages				(stages)
	, m_inRenderPass		(inRenderPass)
	, m_hostQueryReset		(hostQueryReset)
	, m_queryResultFlags	(queryResultFlags)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	m_timestampMask = checkTimestampsSupported(context);

	// Create Query Pool
	{
		const VkQueryPoolCreateInfo queryPoolParams =
		{
		   VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType               sType;
		   DE_NULL,										// const void*                   pNext;
		   0u,											// VkQueryPoolCreateFlags        flags;
		   VK_QUERY_TYPE_TIMESTAMP,						// VkQueryType                   queryType;
		   TimestampTest::ENTRY_COUNT,					// deUint32                      entryCount;
		   0u,											// VkQueryPipelineStatisticFlags pipelineStatistics;
		};

		m_queryPool = createQueryPool(vk, vkDevice, &queryPoolParams);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// alloc timestamp values
	m_timestampValues = new deUint64[m_stages.size() * ((m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ? 2u : 1u)];

	if (m_hostQueryReset)
		m_timestampValuesHostQueryReset = new deUint64[m_stages.size() * 2];
	else
		m_timestampValuesHostQueryReset = DE_NULL;
}

TimestampTestInstance::~TimestampTestInstance (void)
{
	delete[] m_timestampValues;
	m_timestampValues = NULL;

	delete[] m_timestampValuesHostQueryReset;
	m_timestampValuesHostQueryReset = NULL;
}

void TimestampTestInstance::configCommandBuffer (void)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	if (!m_hostQueryReset)
		vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

	deUint32 timestampEntry = 0;
	for (const auto& stage : m_stages)
	{
		vk.cmdWriteTimestamp(*m_cmdBuffer, stage, *m_queryPool, timestampEntry++);
	}

	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus TimestampTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const bool				availabilityBit		= m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
	const deUint32			stageSize			= (deUint32)m_stages.size();
	const deUint32			queryDataSize		= sizeof(deUint64) * (availabilityBit ? 2u : 1u);

	configCommandBuffer();
	if (m_hostQueryReset)
	{
		vk.resetQueryPool(vkDevice, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);
	}
	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	// Get timestamp value from query pool
	vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, stageSize, queryDataSize * stageSize, (void*)m_timestampValues, queryDataSize, m_queryResultFlags);

	for (deUint32 ndx = 0; ndx < stageSize; ndx++)
	{
		m_timestampValues[ndx] &= m_timestampMask;
		if (availabilityBit)
			ndx++;
	}

	if(m_hostQueryReset)
	{
		// Initialize timestampValuesHostQueryReset values
		deMemset(m_timestampValuesHostQueryReset, 0, sizeof(deUint64) * stageSize * 2);

		for (deUint32 ndx = 0; ndx < stageSize; ndx++)
		{
			const deUint32 ndxTimestampValue = ndx * (availabilityBit ? 2u : 1u);
			m_timestampValuesHostQueryReset[2 * ndx] = m_timestampValues[ndxTimestampValue];
		}

		// Host resets the query pool
		vk.resetQueryPool(vkDevice, *m_queryPool, 0u, stageSize);
		// Get timestamp value from query pool
		vk::VkResult res = vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, stageSize, sizeof(deUint64) * stageSize * 2, (void*)m_timestampValuesHostQueryReset, sizeof(deUint64) * 2, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

		/* From Vulkan spec:
		 *
		 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
		 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
		 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
		 */
		if (res != vk::VK_NOT_READY)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");

		for (deUint32 ndx = 0; ndx < stageSize; ndx++)
		{
			const deUint32 ndxTimestampValue = ndx * (availabilityBit ? 2u : 1u);
			if ((m_timestampValuesHostQueryReset[2 * ndx] & m_timestampMask) != m_timestampValues[ndxTimestampValue])
				return tcu::TestStatus::fail("QueryPoolResults returned value was modified");
			if (m_timestampValuesHostQueryReset[2 * ndx + 1] != 0u)
				return tcu::TestStatus::fail("QueryPoolResults availability status is not zero");
		}
	}

	return verifyTimestamp();
}

tcu::TestStatus TimestampTestInstance::verifyTimestamp (void)
{
	bool availabilityBit = m_queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
	deUint32 increment = availabilityBit ? 2u : 1u;
	for (deUint32 first = 0u; first < m_stages.size(); first += increment)
	{
		for (deUint32 second = 0u; second < first; second += increment)
		{
			if (availabilityBit && (m_timestampValues[first + 1u] == 0u || m_timestampValues[second + 1u] == 0u))
			{
				return tcu::TestStatus::fail("Timestamp query not available");
			}

			if(m_timestampValues[first] < m_timestampValues[second])
			{
				return tcu::TestStatus::fail("Latter stage timestamp is smaller than the former stage timestamp.");
			}
		}
	}

	return tcu::TestStatus::pass("Timestamp increases steadily.");
}

Move<VkBuffer> TimestampTestInstance::createBufferAndBindMemory (VkDeviceSize size, VkBufferUsageFlags usage, de::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	const VkBufferCreateInfo	vertexBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType      sType;
		DE_NULL,								// const void*          pNext;
		0u,										// VkBufferCreateFlags  flags;
		size,									// VkDeviceSize         size;
		usage,									// VkBufferUsageFlags   usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode        sharingMode;
		1u,										// deUint32             queueFamilyCount;
		&queueFamilyIndex						// const deUint32*      pQueueFamilyIndices;
	};

	Move<VkBuffer> vertexBuffer = createBuffer(vk, vkDevice, &vertexBufferParams);
	de::MovePtr<Allocation> vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferAlloc->getMemory(), vertexBufferAlloc->getOffset()));

	DE_ASSERT(pAlloc);
	*pAlloc = vertexBufferAlloc;

	return vertexBuffer;
}

Move<VkImage> TimestampTestInstance::createImage2DAndBindMemory (VkFormat							format,
																 deUint32							width,
																 deUint32							height,
																 VkImageUsageFlags					usage,
																 VkSampleCountFlagBits				sampleCount,
																 de::details::MovePtr<Allocation>*	pAlloc)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator			memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	// Optimal tiling feature check
	VkFormatProperties		formatProperty;

	m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), format, &formatProperty);

	if((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) && !(formatProperty.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
	{
		// Remove color attachment usage if the optimal tiling feature does not support it
		usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if((usage & VK_IMAGE_USAGE_STORAGE_BIT) && !(formatProperty.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
	{
		// Remove storage usage if the optimal tiling feature does not support it
		usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
	}

	const VkImageCreateInfo colorImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType      sType;
		DE_NULL,								// const void*          pNext;
		0u,										// VkImageCreateFlags   flags;
		VK_IMAGE_TYPE_2D,						// VkImageType          imageType;
		format,									// VkFormat             format;
		{ width, height, 1u },					// VkExtent3D           extent;
		1u,										// deUint32             mipLevels;
		1u,										// deUint32             arraySize;
		sampleCount,							// deUint32             samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling        tiling;
		usage,									// VkImageUsageFlags    usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode        sharingMode;
		1u,										// deUint32             queueFamilyCount;
		&queueFamilyIndex,						// const deUint32*      pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout        initialLayout;
	};

	Move<VkImage> image = createImage(vk, vkDevice, &colorImageParams);

	// Allocate and bind image memory
	de::MovePtr<Allocation> colorImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *image, colorImageAlloc->getMemory(), colorImageAlloc->getOffset()));

	DE_ASSERT(pAlloc);
	*pAlloc = colorImageAlloc;

	return image;
}

template <class T>
class CalibratedTimestampTest : public vkt::TestCase
{
public:
								CalibratedTimestampTest		(tcu::TestContext&		testContext,
															 const std::string&		name,
															 const std::string&		description)
									: vkt::TestCase{testContext, name, description}
									{ }

	virtual						~CalibratedTimestampTest	(void) override { }
	virtual void				initPrograms				(SourceCollections&		programCollection) const override;
	virtual void				checkSupport				(Context&				context) const override;
	virtual vkt::TestInstance*	createInstance				(Context&				context) const override;
};

class CalibratedTimestampTestInstance : public vkt::TestInstance
{
public:
							CalibratedTimestampTestInstance		(Context& context);
	virtual                 ~CalibratedTimestampTestInstance	(void) override { }
	virtual tcu::TestStatus iterate								(void) override;
	virtual tcu::TestStatus runTest								(void) = 0;
protected:
	struct CalibratedTimestamp
	{
		CalibratedTimestamp(deUint64 timestamp_, deUint64 deviation_) : timestamp{timestamp_}, deviation(deviation_) { }
		CalibratedTimestamp() : timestamp{}, deviation{} { }
		deUint64 timestamp;
		deUint64 deviation;
	};

	std::vector<VkTimeDomainEXT>		getDomainSubset(const std::vector<VkTimeDomainEXT>& available, const std::vector<VkTimeDomainEXT>& interesting) const;
	std::string							domainName(VkTimeDomainEXT domain) const;
	deUint64							getHostNativeTimestamp(VkTimeDomainEXT hostDomain) const;
	deUint64							getHostNanoseconds(deUint64 hostTimestamp) const;
	deUint64							getDeviceNanoseconds(deUint64 devTicksDelta) const;
	std::vector<CalibratedTimestamp>	getCalibratedTimestamps(const std::vector<VkTimeDomainEXT>& domains);
	CalibratedTimestamp					getCalibratedTimestamp(VkTimeDomainEXT domain);
	void								appendQualityMessage(const std::string& message);

	void								verifyDevTimestampMask(deUint64 value) const;
	deUint64							absDiffWithOverflow(deUint64 a, deUint64 b, deUint64 mask = std::numeric_limits<deUint64>::max()) const;
	deUint64							positiveDiffWithOverflow(deUint64 before, deUint64 after, deUint64 mask = std::numeric_limits<deUint64>::max()) const;
	bool								outOfRange(deUint64 begin, deUint64 middle, deUint64 end) const;

	static constexpr deUint64		kBatchTimeLimitNanos		= 1000000000u;	// 1 sec.
	static constexpr deUint64		kDeviationErrorLimitNanos	=  100000000u;	// 100 ms.
	static constexpr deUint64		kDeviationWarningLimitNanos =   50000000u;	// 50 ms.
	static constexpr deUint64		kDefaultToleranceNanos		=  100000000u;	// 100 ms.

#if (DE_OS == DE_OS_WIN32)
    // Preprocessor used to avoid warning about unused variable.
	static constexpr deUint64		kNanosecondsPerSecond		= 1000000000u;
#endif
	static constexpr deUint64		kNanosecondsPerMillisecond	=    1000000u;

	std::string						m_qualityMessage;
	float							m_timestampPeriod;
	std::vector<VkTimeDomainEXT>	m_devDomains;
	std::vector<VkTimeDomainEXT>	m_hostDomains;
#if (DE_OS == DE_OS_WIN32)
	deUint64						m_frequency;
#endif

	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	Move<VkQueryPool>				m_queryPool;
	deUint64						m_devTimestampMask;
};

class CalibratedTimestampDevDomainTestInstance : public CalibratedTimestampTestInstance
{
public:
							CalibratedTimestampDevDomainTestInstance	(Context& context)
								: CalibratedTimestampTestInstance{context}
								{ }

	virtual                 ~CalibratedTimestampDevDomainTestInstance	(void) { }
	virtual tcu::TestStatus runTest										(void) override;
};

class CalibratedTimestampHostDomainTestInstance : public CalibratedTimestampTestInstance
{
public:
							CalibratedTimestampHostDomainTestInstance	(Context& context)
								: CalibratedTimestampTestInstance{context}
								{ }

	virtual                 ~CalibratedTimestampHostDomainTestInstance	(void) { }
	virtual tcu::TestStatus runTest										(void) override;
};

class CalibratedTimestampCalibrationTestInstance : public CalibratedTimestampTestInstance
{
public:
							CalibratedTimestampCalibrationTestInstance	(Context& context)
								: CalibratedTimestampTestInstance{context}
								{ }

	virtual                 ~CalibratedTimestampCalibrationTestInstance	(void) { }
	virtual tcu::TestStatus runTest										(void) override;
};

template <class T>
void CalibratedTimestampTest<T>::initPrograms (SourceCollections& programCollection) const
{
	vkt::TestCase::initPrograms(programCollection);
}

template <class T>
vkt::TestInstance* CalibratedTimestampTest<T>::createInstance (Context& context) const
{
	return new T{context};
}

template <class T>
void CalibratedTimestampTest<T>::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_calibrated_timestamps");
}

CalibratedTimestampTestInstance::CalibratedTimestampTestInstance (Context& context)
	: TestInstance{context}
{
#if (DE_OS == DE_OS_WIN32)
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq))
	{
		throw tcu::ResourceError("Unable to get clock frequency with QueryPerformanceFrequency");
	}
	if (freq.QuadPart <= 0)
	{
		throw tcu::ResourceError("QueryPerformanceFrequency did not return a positive number");
	}
	m_frequency = static_cast<deUint64>(freq.QuadPart);
#endif

	const InstanceInterface&	vki					= context.getInstanceInterface();
	const VkPhysicalDevice		physDevice			= context.getPhysicalDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	// Get timestamp mask.
	m_devTimestampMask = checkTimestampsSupported(context);

	// Get calibreatable time domains.
	m_timestampPeriod = getPhysicalDeviceProperties(vki, physDevice).limits.timestampPeriod;

	deUint32 domainCount;
	VK_CHECK(vki.getPhysicalDeviceCalibrateableTimeDomainsEXT(physDevice, &domainCount, DE_NULL));
	if (domainCount == 0)
	{
		throw tcu::NotSupportedError("No calibrateable time domains found");
	}

	std::vector<VkTimeDomainEXT> domains;
	domains.resize(domainCount);
	VK_CHECK(vki.getPhysicalDeviceCalibrateableTimeDomainsEXT(physDevice, &domainCount, domains.data()));

	// Find the dev domain.
	std::vector<VkTimeDomainEXT> preferredDevDomains;
	preferredDevDomains.push_back(VK_TIME_DOMAIN_DEVICE_EXT);
	m_devDomains = getDomainSubset(domains, preferredDevDomains);

	// Find the host domain.
	std::vector<VkTimeDomainEXT> preferredHostDomains;
#if (DE_OS == DE_OS_WIN32)
	preferredHostDomains.push_back(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT);
#else
	preferredHostDomains.push_back(VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT);
	preferredHostDomains.push_back(VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT);
#endif
	m_hostDomains = getDomainSubset(domains, preferredHostDomains);

	// Initialize command buffers and queries.
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const VkDevice				vkDevice		= context.getDevice();

	const VkQueryPoolCreateInfo	queryPoolParams	=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType               sType;
		DE_NULL,									// const void*                   pNext;
		0u,											// VkQueryPoolCreateFlags        flags;
		VK_QUERY_TYPE_TIMESTAMP,					// VkQueryType                   queryType;
		1u,											// deUint32                      entryCount;
		0u,											// VkQueryPipelineStatisticFlags pipelineStatistics;
	};

	m_queryPool	= createQueryPool(vk, vkDevice, &queryPoolParams);
	m_cmdPool	= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);
	vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
	vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);
	endCommandBuffer(vk, *m_cmdBuffer);
}

std::vector<VkTimeDomainEXT> CalibratedTimestampTestInstance::getDomainSubset (const std::vector<VkTimeDomainEXT>& available, const std::vector<VkTimeDomainEXT>& interesting) const
{
	const std::set<VkTimeDomainEXT> availableSet	(begin(available),		end(available));
	const std::set<VkTimeDomainEXT> interestingSet	(begin(interesting),	end(interesting));

	std::vector<VkTimeDomainEXT> subset;
	std::set_intersection(begin(availableSet), end(availableSet), begin(interestingSet), end(interestingSet), std::back_inserter(subset));
	return subset;
}

std::string CalibratedTimestampTestInstance::domainName(VkTimeDomainEXT domain) const
{
	switch (domain)
	{
	case VK_TIME_DOMAIN_DEVICE_EXT:						return "Device Domain";
	case VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT:			return "Monotonic Clock";
	case VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT:		return "Raw Monotonic Clock";
	case VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT:	return "Query Performance Counter";
	default:											DE_ASSERT(DE_FALSE); return "Unknown Time Domain";
	}
}

deUint64 CalibratedTimestampTestInstance::getHostNativeTimestamp (VkTimeDomainEXT hostDomain) const
{
#if (DE_OS == DE_OS_WIN32)
	DE_ASSERT(hostDomain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT);
	LARGE_INTEGER result;
	if (!QueryPerformanceCounter(&result))
	{
		throw tcu::ResourceError("Unable to obtain host native timestamp for Win32");
	}
	if (result.QuadPart < 0)
	{
		throw tcu::ResourceError("Host-native timestamp for Win32 less than zero");
	}
	return static_cast<deUint64>(result.QuadPart);
#else
	DE_ASSERT(hostDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT ||
			  hostDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT);

#if defined(CLOCK_MONOTONIC_RAW)
	clockid_t id = ((hostDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT) ? CLOCK_MONOTONIC : CLOCK_MONOTONIC_RAW);
#else
	clockid_t id = CLOCK_MONOTONIC;
#endif
	struct timespec ts;
	if (clock_gettime(id, &ts) != 0)
	{
		throw tcu::ResourceError("Unable to obtain host native timestamp for POSIX");
	}
	return (static_cast<deUint64>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec);
#endif
}

deUint64 CalibratedTimestampTestInstance::getHostNanoseconds (deUint64 hostTimestamp) const
{
#if (DE_OS == DE_OS_WIN32)
	deUint64	secs	= hostTimestamp / m_frequency;
	deUint64	nanos	= ((hostTimestamp % m_frequency) * kNanosecondsPerSecond) / m_frequency;

	return ((secs * kNanosecondsPerSecond) + nanos);
#else
	return hostTimestamp;
#endif
}

// This method will be used when devTicksDelta is (supposedly) a small amount of ticks between two events. We will check
// devTicksDelta is reasonably small for the calculation below to succeed without losing precision.
deUint64 CalibratedTimestampTestInstance::getDeviceNanoseconds (deUint64 devTicksDelta) const
{
	if (devTicksDelta > static_cast<deUint64>(std::numeric_limits<deUint32>::max()))
	{
		std::ostringstream msg;
		msg << "Number of device ticks too big for conversion to nanoseconds: " << devTicksDelta;
		throw tcu::InternalError(msg.str());
	}
	return static_cast<deUint64>(static_cast<double>(devTicksDelta) * m_timestampPeriod);
}

tcu::TestStatus CalibratedTimestampTestInstance::iterate (void)
{
	// Notes:
	//	1) Clocks may overflow.
	//	2) Because m_timestampPeriod is a floating point value, there may be less than one nano per tick.

	const tcu::TestStatus result = runTest();
	if (result.getCode() != QP_TEST_RESULT_PASS)
		return result;

	if (!m_qualityMessage.empty())
	{
		const std::string msg = "Warnings found: " + m_qualityMessage;
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, msg);
	}
	return tcu::TestStatus::pass("Pass");
}

// Verify all invalid timestamp bits are zero.
void CalibratedTimestampTestInstance::verifyDevTimestampMask (deUint64 value) const
{
	checkTimestampBits(value, m_devTimestampMask);
}

// Absolute difference between two timestamps A and B taking overflow into account. Pick the smallest difference between the two
// possibilities. We don't know beforehand if B > A or vice versa. Take the valid bit mask into account.
deUint64 CalibratedTimestampTestInstance::absDiffWithOverflow (deUint64 a, deUint64 b, deUint64 mask) const
{
	//	<---------+ range +-------->
	//
	//	+--------------------------+
	//	|         deUint64         |
	//	+------^-----------^-------+
	//	       +           +
	//	       a           b
	//	       +----------->
	//	       ccccccccccccc
	//	------>             +-------
	//	ddddddd             dddddddd

	DE_ASSERT(a <= mask);
	DE_ASSERT(b <= mask);

	const deUint64 c = ((a >= b) ? (a - b) : (b - a));

	if (c == 0u)
		return c;

	const deUint64 d = (mask - c) + 1;

	return ((c < d) ? c : d);
}

// Positive difference between both marks, advancing from before to after, taking overflow and the valid bit mask into account.
deUint64 CalibratedTimestampTestInstance::positiveDiffWithOverflow (deUint64 before, deUint64 after, deUint64 mask) const
{
	DE_ASSERT(before <= mask);
	DE_ASSERT(after  <= mask);

	return ((before <= after) ? (after - before) : ((mask - (before - after)) + 1));
}

// Return true if middle is not between begin and end, taking overflow into account.
bool CalibratedTimestampTestInstance::outOfRange (deUint64 begin, deUint64 middle, deUint64 end) const
{
	return (((begin <= end) && (middle < begin || middle > end	)) ||
			((begin >  end) && (middle > end   && middle < begin)));
}

std::vector<CalibratedTimestampTestInstance::CalibratedTimestamp> CalibratedTimestampTestInstance::getCalibratedTimestamps (const std::vector<VkTimeDomainEXT>& domains)
{
	std::vector<VkCalibratedTimestampInfoEXT> infos;

	for (auto domain : domains)
	{
		VkCalibratedTimestampInfoEXT info;
		info.sType = getStructureType<VkCalibratedTimestampInfoEXT>();
		info.pNext = DE_NULL;
		info.timeDomain = domain;
		infos.push_back(info);
	}

	std::vector<deUint64> timestamps(domains.size());
	deUint64			  deviation;

	const DeviceInterface&      vk          = m_context.getDeviceInterface();
	const VkDevice              vkDevice    = m_context.getDevice();

	VK_CHECK(vk.getCalibratedTimestampsEXT(vkDevice, static_cast<deUint32>(domains.size()), infos.data(), timestamps.data(), &deviation));

	if (deviation > kDeviationErrorLimitNanos)
	{
		throw tcu::InternalError("Calibrated maximum deviation too big");
	}
	else if (deviation > kDeviationWarningLimitNanos)
	{
		appendQualityMessage("Calibrated maximum deviation beyond desirable limits");
	}
	else if (deviation == 0 && domains.size() > 1)
	{
		appendQualityMessage("Calibrated maximum deviation reported as zero");
	}

	// Pack results.
	std::vector<CalibratedTimestamp> results;

	for (size_t i = 0; i < domains.size(); ++i)
	{
		if (domains[i] == VK_TIME_DOMAIN_DEVICE_EXT)
			verifyDevTimestampMask(timestamps[i]);
		results.emplace_back(timestamps[i], deviation);
	}

	return results;
}

CalibratedTimestampTestInstance::CalibratedTimestamp CalibratedTimestampTestInstance::getCalibratedTimestamp (VkTimeDomainEXT domain)
{
	// Single domain, single result.
	return getCalibratedTimestamps(std::vector<VkTimeDomainEXT>(1, domain))[0];
}

void CalibratedTimestampTestInstance::appendQualityMessage (const std::string& message)
{
	if (!m_qualityMessage.empty())
		m_qualityMessage += "; ";

	m_qualityMessage += message;
}

// Test device domain makes sense and is consistent with vkCmdWriteTimestamp().
tcu::TestStatus CalibratedTimestampDevDomainTestInstance::runTest (void)
{
	if (m_devDomains.empty())
		throw tcu::NotSupportedError("No suitable device time domains found");

	const DeviceInterface&      vk          = m_context.getDeviceInterface();
	const VkDevice              vkDevice    = m_context.getDevice();
	const VkQueue               queue       = m_context.getUniversalQueue();

	for (const auto devDomain : m_devDomains)
	{
		const CalibratedTimestamp	before		= getCalibratedTimestamp(devDomain);
		submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
		const CalibratedTimestamp	after		= getCalibratedTimestamp(devDomain);
		const deUint64				diffNanos	= getDeviceNanoseconds(positiveDiffWithOverflow(before.timestamp, after.timestamp, m_devTimestampMask));
		deUint64					written;
		VK_CHECK(vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, 1u, sizeof(written), &written, sizeof(written), (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)));
		verifyDevTimestampMask(written);

		if (diffNanos > kBatchTimeLimitNanos)
		{
			return tcu::TestStatus::fail(domainName(devDomain) + ": Batch of work took too long to execute");
		}

		if (outOfRange(before.timestamp, written, after.timestamp))
		{
			return tcu::TestStatus::fail(domainName(devDomain) + ": vkCmdWriteTimestamp() inconsistent with vkGetCalibratedTimestampsEXT()");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

// Test host domain makes sense and is consistent with native host values.
tcu::TestStatus CalibratedTimestampHostDomainTestInstance::runTest (void)
{
	if (m_hostDomains.empty())
		throw tcu::NotSupportedError("No suitable host time domains found");

	for (const auto hostDomain : m_hostDomains)
	{
		const deUint64				before		= getHostNativeTimestamp(hostDomain);
		const CalibratedTimestamp	vkTS		= getCalibratedTimestamp(hostDomain);
		const deUint64				after		= getHostNativeTimestamp(hostDomain);
		const deUint64				diffNanos	= getHostNanoseconds(positiveDiffWithOverflow(before, after));

		if (diffNanos > kBatchTimeLimitNanos)
		{
			return tcu::TestStatus::fail(domainName(hostDomain) + ": Querying host domain took too long to execute");
		}

		if (outOfRange(before, vkTS.timestamp, after))
		{
			return tcu::TestStatus::fail(domainName(hostDomain) + ": vkGetCalibratedTimestampsEXT() inconsistent with native host API");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

// Verify predictable timestamps and calibration possible.
tcu::TestStatus CalibratedTimestampCalibrationTestInstance::runTest (void)
{
	if (m_devDomains.empty())
		throw tcu::NotSupportedError("No suitable device time domains found");
	if (m_hostDomains.empty())
		throw tcu::NotSupportedError("No suitable host time domains found");

	// Sleep time.
	constexpr deUint32	kSleepMilliseconds	= 200;
	constexpr deUint32	kSleepNanoseconds	= kSleepMilliseconds * kNanosecondsPerMillisecond;

	for (const auto devDomain	: m_devDomains)
	for (const auto hostDomain	: m_hostDomains)
	{
		std::vector<VkTimeDomainEXT>	domains;
		domains.push_back(devDomain);	// Device results at index 0.
		domains.push_back(hostDomain);	// Host results at index 1.

		// Measure time.
		const std::vector<CalibratedTimestamp> before	= getCalibratedTimestamps(domains);
		std::this_thread::sleep_for(std::chrono::nanoseconds(kSleepNanoseconds));
		const std::vector<CalibratedTimestamp> after	= getCalibratedTimestamps(domains);

		// Check device timestamp is as expected.
		const deUint64 devBeforeTicks	= before[0].timestamp;
		const deUint64 devAfterTicks	= after[0].timestamp;
		const deUint64 devExpectedTicks	= ((devBeforeTicks + static_cast<deUint64>(static_cast<double>(kSleepNanoseconds) / m_timestampPeriod)) & m_devTimestampMask);
		const deUint64 devDiffNanos		= getDeviceNanoseconds(absDiffWithOverflow(devAfterTicks, devExpectedTicks, m_devTimestampMask));
		const deUint64 maxDevDiffNanos	= std::max({ kDefaultToleranceNanos, before[0].deviation + after[0].deviation });

		if (devDiffNanos > maxDevDiffNanos)
		{
			std::ostringstream msg;
			msg << "[" << domainName(devDomain) << "] Device expected timestamp differs " << devDiffNanos << " nanoseconds (expect value <= " << maxDevDiffNanos << ")";
			return tcu::TestStatus::fail(msg.str());
		}

		// Check host timestamp is as expected.
		const deUint64 hostBefore		= getHostNanoseconds(before[1].timestamp);
		const deUint64 hostAfter		= getHostNanoseconds(after[1].timestamp);
		const deUint64 hostExpected		= hostBefore + kSleepNanoseconds;
		const deUint64 hostDiff			= absDiffWithOverflow(hostAfter, hostExpected);
		const deUint64 maxHostDiff		= std::max({ kDefaultToleranceNanos, before[1].deviation + after[1].deviation });

		if (hostDiff > maxHostDiff)
		{
			std::ostringstream msg;
			msg << "[" << domainName(hostDomain) << "] Host expected timestamp differs " << hostDiff << " nanoseconds (expected value <= " << maxHostDiff << ")";
			return tcu::TestStatus::fail(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class BasicGraphicsTest : public TimestampTest
{
public:
							BasicGraphicsTest	(tcu::TestContext&			testContext,
												 const std::string&			name,
												 const std::string&			description,
												 const TimestampTestParam*	param)
							: TimestampTest (testContext, name, description, param)
{ }
	virtual					~BasicGraphicsTest	(void) { }
	virtual void			initPrograms		(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance		(Context& context) const;
};

class BasicGraphicsTestInstance : public TimestampTestInstance
{
public:
	enum
	{
		VK_MAX_SHADER_STAGES = 6,
	};
					BasicGraphicsTestInstance	(Context&					context,
												 const StageFlagVector		stages,
												 const bool					inRenderPass,
												 const bool					hostQueryReset,
												 const VkQueryResultFlags	queryResultFlags);

	virtual			~BasicGraphicsTestInstance	(void);
protected:
	virtual void	configCommandBuffer			(void);
	virtual void	buildVertexBuffer			(void);
	virtual void	buildRenderPass				(VkFormat	colorFormat,
												 VkFormat	depthFormat);

	virtual void	buildFrameBuffer			(tcu::UVec2	renderSize,
												 VkFormat	colorFormat,
												 VkFormat	depthFormat);

protected:
	const tcu::UVec2				m_renderSize;
	const VkFormat					m_colorFormat;
	const VkFormat					m_depthFormat;

	Move<VkImage>					m_colorImage;
	de::MovePtr<Allocation>			m_colorImageAlloc;
	Move<VkImage>					m_depthImage;
	de::MovePtr<Allocation>			m_depthImageAlloc;
	Move<VkImageView>				m_colorAttachmentView;
	Move<VkImageView>				m_depthAttachmentView;
	Move<VkRenderPass>				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	VkImageMemoryBarrier			m_imageLayoutBarriers[2];

	de::MovePtr<Allocation>			m_vertexBufferAlloc;
	Move<VkBuffer>					m_vertexBuffer;
	std::vector<Vertex4RGBA>		m_vertices;

	SimpleGraphicsPipelineBuilder	m_pipelineBuilder;
	Move<VkPipeline>				m_graphicsPipelines;
};

void BasicGraphicsTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("color_vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"  gl_Position = position;\n"
		"  vtxColor = color;\n"
		"}\n");

	programCollection.glslSources.add("color_frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"  fragColor = vtxColor;\n"
		"}\n");
}

TestInstance* BasicGraphicsTest::createInstance (Context& context) const
{
	return new BasicGraphicsTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, m_queryResultFlags);
}

void BasicGraphicsTestInstance::buildVertexBuffer (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create vertex buffer
	{
		m_vertexBuffer	= createBufferAndBindMemory(1024u, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &m_vertexBufferAlloc);
		m_vertices		= createOverlappingQuads();

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}
}

void BasicGraphicsTestInstance::buildRenderPass (VkFormat colorFormat, VkFormat depthFormat)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create render pass
	m_renderPass = makeRenderPass(vk, vkDevice, colorFormat, depthFormat);
}

void BasicGraphicsTestInstance::buildFrameBuffer (tcu::UVec2 renderSize, VkFormat colorFormat, VkFormat depthFormat)
{
	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice				vkDevice				= m_context.getDevice();
	const VkComponentMapping	ComponentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

	// Create color image
	{
		m_colorImage = createImage2DAndBindMemory(colorFormat,
												  renderSize.x(),
												  renderSize.y(),
												  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
												  VK_SAMPLE_COUNT_1_BIT,
												  &m_colorImageAlloc);
	}

	// Create depth image
	{
		m_depthImage = createImage2DAndBindMemory(depthFormat,
												  renderSize.x(),
												  renderSize.y(),
												  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
												  VK_SAMPLE_COUNT_1_BIT,
												  &m_depthImageAlloc);
	}

	// Set up image layout transition barriers
	{
		const VkImageMemoryBarrier colorImageBarrier =
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
		const VkImageMemoryBarrier depthImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkAccessFlags			srcAccessMask;
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					dstQueueFamilyIndex;
			*m_depthImage,										// VkImage					image;
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u },		// VkImageSubresourceRange	subresourceRange;
		};

		m_imageLayoutBarriers[0] = colorImageBarrier;
		m_imageLayoutBarriers[1] = depthImageBarrier;
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType          sType;
			DE_NULL,										// const void*              pNext;
			0u,												// VkImageViewCreateFlags   flags;
			*m_colorImage,									// VkImage                  image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType          viewType;
			colorFormat,									// VkFormat                 format;
			ComponentMappingRGBA,							// VkComponentMapping       components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange  subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create depth attachment view
	{
		const VkImageViewCreateInfo depthAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType          sType;
			DE_NULL,										// const void*              pNext;
			0u,												// VkImageViewCreateFlags   flags;
			*m_depthImage,									// VkImage                  image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType          viewType;
			depthFormat,									// VkFormat                 format;
			ComponentMappingRGBA,							// VkComponentMapping       components;
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange  subresourceRange;
		};

		m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
	}

	// Create framebuffer
	{
		const VkImageView attachmentBindInfos[2] =
		{
			*m_colorAttachmentView,
			*m_depthAttachmentView,
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType              sType;
			DE_NULL,									// const void*                  pNext;
			0u,											// VkFramebufferCreateFlags     flags;
			*m_renderPass,								// VkRenderPass                 renderPass;
			2u,											// deUint32                     attachmentCount;
			attachmentBindInfos,						// const VkImageView*           pAttachments;
			(deUint32)renderSize.x(),					// deUint32                     width;
			(deUint32)renderSize.y(),					// deUint32                     height;
			1u,											// deUint32                     layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

}

BasicGraphicsTestInstance::BasicGraphicsTestInstance (Context&					context,
													  const StageFlagVector		stages,
													  const bool				inRenderPass,
													  const bool				hostQueryReset,
													  const VkQueryResultFlags	queryResultFlags)
													  : TimestampTestInstance (context,stages,inRenderPass, hostQueryReset, queryResultFlags)
													  , m_renderSize		(32, 32)
													  , m_colorFormat		(VK_FORMAT_R8G8B8A8_UNORM)
													  , m_depthFormat		(VK_FORMAT_D16_UNORM)
													  , m_pipelineBuilder	(context)
{
	buildVertexBuffer();

	buildRenderPass(m_colorFormat, m_depthFormat);

	buildFrameBuffer(m_renderSize, m_colorFormat, m_depthFormat);

	m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "color_vert");
	m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "color_frag");

	m_graphicsPipelines = m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass);

}

BasicGraphicsTestInstance::~BasicGraphicsTestInstance (void)
{
}

void BasicGraphicsTestInstance::configCommandBuffer (void)
{
	const DeviceInterface&		vk							= m_context.getDeviceInterface();

	const VkClearValue			attachmentClearValues[2]	=
	{
		defaultClearValue(m_colorFormat),
		defaultClearValue(m_depthFormat),
	};

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
		0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers), m_imageLayoutBarriers);

	if (!m_hostQueryReset)
		vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

	beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u, attachmentClearValues);

	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelines);
	VkDeviceSize offsets = 0u;
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);
	vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1u, 0u, 0u);

	if(m_inRenderPass)
	{
	  deUint32 timestampEntry = 0u;

	  for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
	  {
		  vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
	  }
	}

	endRenderPass(vk, *m_cmdBuffer);

	if(!m_inRenderPass)
	{
	  deUint32 timestampEntry = 0u;

	  for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
	  {
		  vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
	  }
	}

	endCommandBuffer(vk, *m_cmdBuffer);
}

class AdvGraphicsTest : public BasicGraphicsTest
{
public:
						  AdvGraphicsTest	(tcu::TestContext&			testContext,
											 const std::string&			name,
											 const std::string&			description,
											 const TimestampTestParam*	param)
							  : BasicGraphicsTest(testContext, name, description, param)
							  { }

	virtual               ~AdvGraphicsTest (void) { }
	virtual void          initPrograms     (SourceCollections& programCollection) const;
	virtual TestInstance* createInstance   (Context& context) const;
};

class AdvGraphicsTestInstance : public BasicGraphicsTestInstance
{
public:
				 AdvGraphicsTestInstance	(Context&					context,
											 const StageFlagVector		stages,
											 const bool					inRenderPass,
											 const bool					hostQueryReset,
											 const VkQueryResultFlags	queryResultFlags);

	virtual      ~AdvGraphicsTestInstance	(void);
	virtual void configCommandBuffer		(void);

protected:
	virtual void featureSupportCheck		(void);

protected:
	VkPhysicalDeviceFeatures	m_features;
	deUint32					m_draw_count;
	de::MovePtr<Allocation>		m_indirectBufferAlloc;
	Move<VkBuffer>				m_indirectBuffer;
};

void AdvGraphicsTest::initPrograms (SourceCollections& programCollection) const
{
	BasicGraphicsTest::initPrograms(programCollection);

	programCollection.glslSources.add("dummy_geo") << glu::GeometrySource(
		"#version 310 es\n"
		"#extension GL_EXT_geometry_shader : enable\n"
		"layout(triangles) in;\n"
		"layout(triangle_strip, max_vertices = 3) out;\n"
		"layout(location = 0) in highp vec4 in_vtxColor[];\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"  for(int ndx=0; ndx<3; ndx++)\n"
		"  {\n"
		"    gl_Position = gl_in[ndx].gl_Position;\n"
		"    vtxColor    = in_vtxColor[ndx];\n"
		"    EmitVertex();\n"
		"  }\n"
		"  EndPrimitive();\n"
		"}\n");

	programCollection.glslSources.add("basic_tcs") << glu::TessellationControlSource(
		"#version 310 es\n"
		"#extension GL_EXT_tessellation_shader : enable\n"
		"layout(vertices = 3) out;\n"
		"layout(location = 0) in highp vec4 color[];\n"
		"layout(location = 0) out highp vec4 vtxColor[];\n"
		"void main()\n"
		"{\n"
		"  gl_TessLevelOuter[0] = 4.0;\n"
		"  gl_TessLevelOuter[1] = 4.0;\n"
		"  gl_TessLevelOuter[2] = 4.0;\n"
		"  gl_TessLevelInner[0] = 4.0;\n"
		"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		"  vtxColor[gl_InvocationID] = color[gl_InvocationID];\n"
		"}\n");

	programCollection.glslSources.add("basic_tes") << glu::TessellationEvaluationSource(
		"#version 310 es\n"
		"#extension GL_EXT_tessellation_shader : enable\n"
		"layout(triangles, fractional_even_spacing, ccw) in;\n"
		"layout(location = 0) in highp vec4 colors[];\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
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
		"  vtxColor = color;\n"
		"}\n");
}

TestInstance* AdvGraphicsTest::createInstance (Context& context) const
{
	return new AdvGraphicsTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, m_queryResultFlags);
}

void AdvGraphicsTestInstance::featureSupportCheck (void)
{
	for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
	{
		switch(*it)
		{
			case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT:
				if (m_features.geometryShader == VK_FALSE)
				{
					TCU_THROW(NotSupportedError, "Geometry Shader Not Supported");
				}
				break;
			case VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT:
			case VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT:
				if (m_features.tessellationShader == VK_FALSE)
				{
					TCU_THROW(NotSupportedError, "Tessellation Not Supported");
				}
				break;
			case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
			default:
				break;
		};
	}
}

AdvGraphicsTestInstance::AdvGraphicsTestInstance (Context&					context,
												  const StageFlagVector		stages,
												  const bool				inRenderPass,
												  const bool				hostQueryReset,
												  const VkQueryResultFlags	queryResultFlags)
	: BasicGraphicsTestInstance(context, stages, inRenderPass, hostQueryReset, queryResultFlags)
{
	m_features = m_context.getDeviceFeatures();

	// If necessary feature is not supported, throw error and fail current test
	featureSupportCheck();

	if(m_features.geometryShader == VK_TRUE)
	{
		m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, "dummy_geo");
	}

	if(m_features.tessellationShader == VK_TRUE)
	{
		m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "basic_tcs");
		m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "basic_tes");
		m_pipelineBuilder.enableTessellationStage(3);
	}

	m_graphicsPipelines = m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass);

	// Prepare the indirect draw buffer
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	if(m_features.multiDrawIndirect == VK_TRUE)
	{
		m_draw_count = 2;
	}
	else
	{
		m_draw_count = 1;
	}

	m_indirectBuffer = createBufferAndBindMemory(32u, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, &m_indirectBufferAlloc);

	const VkDrawIndirectCommand indirectCmds[] =
	{
		{
			12u,	// deUint32    vertexCount;
			1u,		// deUint32    instanceCount;
			0u,		// deUint32    firstVertex;
			0u,		// deUint32    firstInstance;
		},
		{
			12u,	// deUint32    vertexCount;
			1u,		// deUint32    instanceCount;
			11u,	// deUint32    firstVertex;
			0u,		// deUint32    firstInstance;
		},
	};

	// Load data into indirect draw buffer
	deMemcpy(m_indirectBufferAlloc->getHostPtr(), indirectCmds, m_draw_count * sizeof(VkDrawIndirectCommand));
	flushAlloc(vk, vkDevice, *m_indirectBufferAlloc);

}

AdvGraphicsTestInstance::~AdvGraphicsTestInstance (void)
{
}

void AdvGraphicsTestInstance::configCommandBuffer (void)
{
	const DeviceInterface&		vk							= m_context.getDeviceInterface();

	const VkClearValue			attachmentClearValues[2]	=
	{
		defaultClearValue(m_colorFormat),
		defaultClearValue(m_depthFormat),
	};

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
		0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers), m_imageLayoutBarriers);

	if (!m_hostQueryReset)
		vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

	beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u, attachmentClearValues);

	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelines);

	VkDeviceSize offsets = 0u;
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);

	vk.cmdDrawIndirect(*m_cmdBuffer, *m_indirectBuffer, 0u, m_draw_count, sizeof(VkDrawIndirectCommand));

	if(m_inRenderPass)
	{
	  deUint32 timestampEntry = 0u;
	  for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
	  {
		  vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
	  }
	}

	endRenderPass(vk, *m_cmdBuffer);

	if(!m_inRenderPass)
	{
	  deUint32 timestampEntry = 0u;
	  for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
	  {
		  vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
	  }
	}

	endCommandBuffer(vk, *m_cmdBuffer);
}

class BasicComputeTest : public TimestampTest
{
public:
							BasicComputeTest	(tcu::TestContext&			testContext,
												 const std::string&			name,
												 const std::string&			description,
												 const TimestampTestParam*	param)
							  : TimestampTest(testContext, name, description, param)
							  { }

	virtual					~BasicComputeTest	(void) { }
	virtual void			initPrograms		(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance		(Context& context) const;
};

class BasicComputeTestInstance : public TimestampTestInstance
{
public:
					BasicComputeTestInstance	(Context&				context,
												 const StageFlagVector	stages,
												 const bool				inRenderPass,
												 const bool				hostQueryReset,
												 VkQueryResultFlags		VkQueryResultFlags);

	virtual			~BasicComputeTestInstance	(void);
	virtual void	configCommandBuffer			(void);
protected:
	de::MovePtr<Allocation>     m_inputBufAlloc;
	Move<VkBuffer>              m_inputBuf;
	de::MovePtr<Allocation>     m_outputBufAlloc;
	Move<VkBuffer>              m_outputBuf;

	Move<VkDescriptorPool>      m_descriptorPool;
	Move<VkDescriptorSet>       m_descriptorSet;
	Move<VkDescriptorSetLayout> m_descriptorSetLayout;

	Move<VkPipelineLayout>      m_pipelineLayout;
	Move<VkShaderModule>        m_computeShaderModule;
	Move<VkPipeline>            m_computePipelines;
};

void BasicComputeTest::initPrograms (SourceCollections& programCollection) const
{
	TimestampTest::initPrograms(programCollection);

	programCollection.glslSources.add("basic_compute") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(local_size_x = 128) in;\n"
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
}

TestInstance* BasicComputeTest::createInstance (Context& context) const
{
	return new BasicComputeTestInstance(context,m_stages,m_inRenderPass, m_hostQueryReset, m_queryResultFlags);
}

BasicComputeTestInstance::BasicComputeTestInstance (Context&				context,
												    const StageFlagVector	stages,
												    const bool				inRenderPass,
												    const bool				hostQueryReset,
													VkQueryResultFlags		VkQueryResultFlags)
	: TimestampTestInstance(context, stages, inRenderPass, hostQueryReset, VkQueryResultFlags)
{
	const DeviceInterface&	vk			= context.getDeviceInterface();
	const VkDevice			vkDevice	= context.getDevice();

	// Create buffer object, allocate storage, and generate input data
	const VkDeviceSize		size		= sizeof(tcu::Vec4) * 128u * 128u;

	m_inputBuf = createBufferAndBindMemory(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_inputBufAlloc);

	// Load vertices into buffer
	tcu::Vec4*				pVec		= reinterpret_cast<tcu::Vec4*>(m_inputBufAlloc->getHostPtr());

	for (deUint32 ndx = 0u; ndx < (128u * 128u); ndx++)
	{
		for (deUint32 component = 0u; component < 4u; component++)
		{
			pVec[ndx][component]= (float)(ndx * (component + 1u));
		}
	}

	flushAlloc(vk, vkDevice, *m_inputBufAlloc);

	m_outputBuf = createBufferAndBindMemory(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_outputBufAlloc);

	std::vector<VkDescriptorBufferInfo> descriptorInfos;

	descriptorInfos.push_back(makeDescriptorBufferInfo(*m_inputBuf, 0u, size));
	descriptorInfos.push_back(makeDescriptorBufferInfo(*m_outputBuf, 0u, size));

	// Create descriptor set layout
	DescriptorSetLayoutBuilder descLayoutBuilder;

	for (deUint32 bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
	{
		descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	m_descriptorSetLayout = descLayoutBuilder.build(vk, vkDevice);

	// Create descriptor pool
	m_descriptorPool = DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2).build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Create descriptor set
	const VkDescriptorSetAllocateInfo descriptorSetAllocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		*m_descriptorPool,								// VkDescriptorPool                descriptorPool;
		1u,												// deUint32                        setLayoutCount;
		&m_descriptorSetLayout.get(),					// const VkDescriptorSetLayout*    pSetLayouts;
	};
	m_descriptorSet   = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocInfo);

	DescriptorSetUpdateBuilder  builder;
	for (deUint32 descriptorNdx = 0u; descriptorNdx < 2u; descriptorNdx++)
	{
		builder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descriptorNdx), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfos[descriptorNdx]);
	}
	builder.update(vk, vkDevice);

	// Create compute pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		0u,												// VkPipelineLayoutCreateFlags     flags;
		1u,												// deUint32                        setLayoutCount;
		&m_descriptorSetLayout.get(),					// const VkDescriptorSetLayout*    pSetLayouts;
		0u,												// deUint32                        pushConstantRangeCount;
		DE_NULL,										// const VkPushConstantRange*      pPushConstantRanges;
	};

	m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

	// Create compute shader
	VkShaderModuleCreateInfo shaderModuleCreateInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,									// VkStructureType             sType;
		DE_NULL,																		// const void*                 pNext;
		0u,																				// VkShaderModuleCreateFlags   flags;
		m_context.getBinaryCollection().get("basic_compute").getSize(),					// deUintptr                   codeSize;
		(deUint32*)m_context.getBinaryCollection().get("basic_compute").getBinary(),	// const deUint32*             pCode;

	};

	m_computeShaderModule = createShaderModule(vk, vkDevice, &shaderModuleCreateInfo);

	// Create compute pipeline
	const VkPipelineShaderStageCreateInfo stageCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
		DE_NULL,												// const void*                         pNext;
		0u,														// VkPipelineShaderStageCreateFlags    flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits               stage;
		*m_computeShaderModule,									// VkShaderModule                      module;
		"main",													// const char*                         pName;
		DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		0u,												// VkPipelineCreateFlags           flags;
		stageCreateInfo,								// VkPipelineShaderStageCreateInfo stage;
		*m_pipelineLayout,								// VkPipelineLayout                layout;
		(VkPipeline)0,									// VkPipeline                      basePipelineHandle;
		0u,												// deInt32                         basePipelineIndex;
	};

	m_computePipelines = createComputePipeline(vk, vkDevice, (VkPipelineCache)0u, &pipelineCreateInfo);
}

BasicComputeTestInstance::~BasicComputeTestInstance (void)
{
}

void BasicComputeTestInstance::configCommandBuffer (void)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	if (!m_hostQueryReset)
		vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipelines);
	vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &m_descriptorSet.get(), 0u, DE_NULL);
	vk.cmdDispatch(*m_cmdBuffer, 128u, 1u, 1u);

	deUint32 timestampEntry = 0u;
	for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
	{
		vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
	}

	endCommandBuffer(vk, *m_cmdBuffer);
}

class TransferTest : public TimestampTest
{
public:
							TransferTest	(tcu::TestContext&			testContext,
											 const std::string&			name,
											 const std::string&			description,
											 const TimestampTestParam*	param);

	virtual					~TransferTest	(void) { }
	virtual void			initPrograms	(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance	(Context& context) const;

protected:
	TransferMethod        m_method;
};

class TransferTestInstance : public TimestampTestInstance
{
public:
					TransferTestInstance	(Context&					context,
											 const StageFlagVector		stages,
											 const bool					inRenderPass,
											 const bool					hostQueryReset,
											 const TransferMethod		method,
											 const VkQueryResultFlags	queryResultFlags);

	virtual         ~TransferTestInstance	(void);
	virtual void    configCommandBuffer		(void);
	virtual void	initialImageTransition	(VkCommandBuffer			cmdBuffer,
											 VkImage					image,
											 VkImageSubresourceRange	subRange,
											 VkImageLayout				layout);
protected:
	TransferMethod			m_method;

	VkDeviceSize			m_bufSize;
	Move<VkBuffer>			m_srcBuffer;
	Move<VkBuffer>			m_dstBuffer;
	de::MovePtr<Allocation> m_srcBufferAlloc;
	de::MovePtr<Allocation> m_dstBufferAlloc;

	VkFormat				m_imageFormat;
	deInt32					m_imageWidth;
	deInt32					m_imageHeight;
	VkDeviceSize			m_imageSize;
	Move<VkImage>			m_srcImage;
	Move<VkImage>			m_dstImage;
	Move<VkImage>			m_depthImage;
	Move<VkImage>			m_msImage;
	de::MovePtr<Allocation>	m_srcImageAlloc;
	de::MovePtr<Allocation>	m_dstImageAlloc;
	de::MovePtr<Allocation>	m_depthImageAlloc;
	de::MovePtr<Allocation>	m_msImageAlloc;
};

TransferTest::TransferTest (tcu::TestContext&			testContext,
						    const std::string&			name,
						    const std::string&			description,
						    const TimestampTestParam*	param)
	: TimestampTest(testContext, name, description, param)
{
	const TransferTimestampTestParam* transferParam = dynamic_cast<const TransferTimestampTestParam*>(param);
	m_method = transferParam->getMethod();
}

void TransferTest::initPrograms (SourceCollections& programCollection) const
{
	TimestampTest::initPrograms(programCollection);
}

TestInstance* TransferTest::createInstance (Context& context) const
{
  return new TransferTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, m_method, m_queryResultFlags);
}

TransferTestInstance::TransferTestInstance (Context&					context,
										    const StageFlagVector		stages,
										    const bool					inRenderPass,
										    const bool					hostQueryReset,
										    const TransferMethod		method,
											const VkQueryResultFlags	queryResultFlags)
	: TimestampTestInstance(context, stages, inRenderPass, hostQueryReset, queryResultFlags)
	, m_method(method)
	, m_bufSize((queryResultFlags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ? 512u : 256u)
	, m_imageFormat(VK_FORMAT_R8G8B8A8_UNORM)
	, m_imageWidth(4u)
	, m_imageHeight(4u)
	, m_imageSize(256u)
{
	const DeviceInterface&	vk			= context.getDeviceInterface();
	const VkDevice			vkDevice	= context.getDevice();

	// Create src buffer
	m_srcBuffer = createBufferAndBindMemory(m_bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &m_srcBufferAlloc);

	// Init the source buffer memory
	char* pBuf = reinterpret_cast<char*>(m_srcBufferAlloc->getHostPtr());
	deMemset(pBuf, 0xFF, sizeof(char)*(size_t)m_bufSize);
	flushAlloc(vk, vkDevice, *m_srcBufferAlloc);

	// Create dst buffer
	m_dstBuffer	= createBufferAndBindMemory(m_bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &m_dstBufferAlloc);

	// Create src/dst/depth image
	m_srcImage		= createImage2DAndBindMemory(m_imageFormat, m_imageWidth, m_imageHeight,
												 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
												 VK_SAMPLE_COUNT_1_BIT,
												 &m_srcImageAlloc);
	m_dstImage		= createImage2DAndBindMemory(m_imageFormat, m_imageWidth, m_imageHeight,
												 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
												 VK_SAMPLE_COUNT_1_BIT,
												 &m_dstImageAlloc);
	m_depthImage	= createImage2DAndBindMemory(VK_FORMAT_D16_UNORM, m_imageWidth, m_imageHeight,
												 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
												 VK_SAMPLE_COUNT_1_BIT,
												 &m_depthImageAlloc);
	m_msImage		= createImage2DAndBindMemory(m_imageFormat, m_imageWidth, m_imageHeight,
												 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
												 VK_SAMPLE_COUNT_4_BIT,
												 &m_msImageAlloc);
}

TransferTestInstance::~TransferTestInstance (void)
{
}

void TransferTestInstance::configCommandBuffer (void)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	// Initialize buffer/image
	vk.cmdFillBuffer(*m_cmdBuffer, *m_dstBuffer, 0u, m_bufSize, 0x0);

	const VkClearColorValue srcClearValue =
	{
		{1.0f, 1.0f, 1.0f, 1.0f}
	};
	const VkClearColorValue dstClearValue =
	{
		{0.0f, 0.0f, 0.0f, 0.0f}
	};
	const struct VkImageSubresourceRange subRangeColor =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags  aspectMask;
		0u,							// deUint32            baseMipLevel;
		1u,							// deUint32            mipLevels;
		0u,							// deUint32            baseArrayLayer;
		1u,							// deUint32            arraySize;
	};
	const struct VkImageSubresourceRange subRangeDepth =
	{
		VK_IMAGE_ASPECT_DEPTH_BIT,	// VkImageAspectFlags  aspectMask;
		0u,							// deUint32            baseMipLevel;
		1u,							// deUint32            mipLevels;
		0u,							// deUint32            baseArrayLayer;
		1u,							// deUint32            arraySize;
	};

	initialImageTransition(*m_cmdBuffer, *m_srcImage, subRangeColor, VK_IMAGE_LAYOUT_GENERAL);
	initialImageTransition(*m_cmdBuffer, *m_dstImage, subRangeColor, VK_IMAGE_LAYOUT_GENERAL);

	vk.cmdClearColorImage(*m_cmdBuffer, *m_srcImage, VK_IMAGE_LAYOUT_GENERAL, &srcClearValue, 1u, &subRangeColor);
	vk.cmdClearColorImage(*m_cmdBuffer, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, &dstClearValue, 1u, &subRangeColor);

	// synchronize the Clear commands before starting any copy
	const vk::VkMemoryBarrier barrier =
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER,							// VkStructureType	sType;
		DE_NULL,														// const void*		pNext;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,								// VkAccessFlags	srcAccessMask;
		vk::VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags	dstAccessMask;
	};
	vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &barrier, 0u, DE_NULL, 0u, DE_NULL);

	if (!m_hostQueryReset)
		vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);

	// Copy Operations
	const VkImageSubresourceLayers imgSubResCopy =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags  aspectMask;
		0u,							// deUint32            mipLevel;
		0u,							// deUint32            baseArrayLayer;
		1u,							// deUint32            layerCount;
	};

	const VkOffset3D nullOffset  = {0u, 0u, 0u};
	const VkExtent3D imageExtent = {(deUint32)m_imageWidth, (deUint32)m_imageHeight, 1u};
	const VkOffset3D imageOffset = {(int)m_imageWidth, (int)m_imageHeight, 1};

	switch(m_method)
	{
		case TRANSFER_METHOD_COPY_BUFFER:
			{
				const VkBufferCopy  copyBufRegion =
					{
						0u,			// VkDeviceSize    srcOffset;
						0u,			// VkDeviceSize    destOffset;
						m_bufSize,	// VkDeviceSize    copySize;
					};

				vk.cmdCopyBuffer(*m_cmdBuffer, *m_srcBuffer, *m_dstBuffer, 1u, &copyBufRegion);
				break;
			}
		case TRANSFER_METHOD_COPY_IMAGE:
			{
				const VkImageCopy copyImageRegion =
				{
					imgSubResCopy,	// VkImageSubresourceCopy  srcSubresource;
					nullOffset,		// VkOffset3D              srcOffset;
					imgSubResCopy,	// VkImageSubresourceCopy  destSubresource;
					nullOffset,		// VkOffset3D              destOffset;
					imageExtent,	// VkExtent3D              extent;

				};

				vk.cmdCopyImage(*m_cmdBuffer, *m_srcImage, VK_IMAGE_LAYOUT_GENERAL, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u, &copyImageRegion);
				break;
			}
		case TRANSFER_METHOD_COPY_BUFFER_TO_IMAGE:
			{
				const VkBufferImageCopy bufImageCopy =
				{
					0u,							// VkDeviceSize            bufferOffset;
					(deUint32)m_imageWidth,		// deUint32                bufferRowLength;
					(deUint32)m_imageHeight,	// deUint32                bufferImageHeight;
					imgSubResCopy,				// VkImageSubresourceCopy  imageSubresource;
					nullOffset,					// VkOffset3D              imageOffset;
					imageExtent,				// VkExtent3D              imageExtent;
				};

				vk.cmdCopyBufferToImage(*m_cmdBuffer, *m_srcBuffer, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u, &bufImageCopy);
				break;
			}
		case TRANSFER_METHOD_COPY_IMAGE_TO_BUFFER:
			{
				const VkBufferImageCopy imgBufferCopy =
				{
					0u,							// VkDeviceSize            bufferOffset;
					(deUint32)m_imageWidth,		// deUint32                bufferRowLength;
					(deUint32)m_imageHeight,	// deUint32                bufferImageHeight;
					imgSubResCopy,				// VkImageSubresourceCopy  imageSubresource;
					nullOffset,					// VkOffset3D              imageOffset;
					imageExtent,				// VkExtent3D              imageExtent;
				};

				vk.cmdCopyImageToBuffer(*m_cmdBuffer, *m_srcImage, VK_IMAGE_LAYOUT_GENERAL, *m_dstBuffer, 1u, &imgBufferCopy);
				break;
			}
		case TRANSFER_METHOD_BLIT_IMAGE:
			{
				const VkImageBlit imageBlt =
				{
					imgSubResCopy,	// VkImageSubresourceCopy  srcSubresource;
					{
						nullOffset,
						imageOffset,
					},
					imgSubResCopy,	// VkImageSubresourceCopy  destSubresource;
					{
						nullOffset,
						imageOffset,
					}
				};

				vk.cmdBlitImage(*m_cmdBuffer, *m_srcImage, VK_IMAGE_LAYOUT_GENERAL, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u, &imageBlt, VK_FILTER_NEAREST);
				break;
			}
		case TRANSFER_METHOD_CLEAR_COLOR_IMAGE:
			{
				vk.cmdClearColorImage(*m_cmdBuffer, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, &srcClearValue, 1u, &subRangeColor);
				break;
			}
		case TRANSFER_METHOD_CLEAR_DEPTH_STENCIL_IMAGE:
			{
				initialImageTransition(*m_cmdBuffer, *m_depthImage, subRangeDepth, VK_IMAGE_LAYOUT_GENERAL);

				const VkClearDepthStencilValue clearDSValue =
				{
					1.0f,	// float       depth;
					0u,		// deUint32    stencil;
				};

				vk.cmdClearDepthStencilImage(*m_cmdBuffer, *m_depthImage, VK_IMAGE_LAYOUT_GENERAL, &clearDSValue, 1u, &subRangeDepth);
				break;
			}
		case TRANSFER_METHOD_FILL_BUFFER:
			{
				vk.cmdFillBuffer(*m_cmdBuffer, *m_dstBuffer, 0u, m_bufSize, 0x0);
				break;
			}
		case TRANSFER_METHOD_UPDATE_BUFFER:
			{
				const deUint32 data[] =
				{
					0xdeadbeef, 0xabcdef00, 0x12345678
				};

				vk.cmdUpdateBuffer(*m_cmdBuffer, *m_dstBuffer, 0x10, sizeof(data), data);
				break;
			}
		case TRANSFER_METHOD_COPY_QUERY_POOL_RESULTS:
			{
				vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);
				vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_dstBuffer, 0u, 8u, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

				const vk::VkBufferMemoryBarrier bufferBarrier =
				{
					vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
					DE_NULL,										// const void*		pNext;
					vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
					vk::VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
					VK_QUEUE_FAMILY_IGNORED,						// deUint32			srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,						// deUint32			dstQueueFamilyIndex;
					*m_dstBuffer,									// VkBuffer			buffer;
					0ull,											// VkDeviceSize		offset;
					VK_WHOLE_SIZE									// VkDeviceSize		size;
				};

				vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u,
									  0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);

				vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
				break;
			}
		case TRANSFER_METHOD_RESOLVE_IMAGE:
			{
				const VkImageResolve imageResolve =
				{
					imgSubResCopy,                              // VkImageSubresourceLayers  srcSubresource;
					nullOffset,                                 // VkOffset3D                srcOffset;
					imgSubResCopy,                              // VkImageSubresourceLayers  destSubresource;
					nullOffset,                                 // VkOffset3D                destOffset;
					imageExtent,                                // VkExtent3D                extent;
				};

				initialImageTransition(*m_cmdBuffer, *m_msImage, subRangeColor, VK_IMAGE_LAYOUT_GENERAL);
				vk.cmdClearColorImage(*m_cmdBuffer, *m_msImage, VK_IMAGE_LAYOUT_GENERAL, &srcClearValue, 1u, &subRangeColor);
				vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &barrier, 0u, DE_NULL, 0u, DE_NULL);
				vk.cmdResolveImage(*m_cmdBuffer, *m_msImage, VK_IMAGE_LAYOUT_GENERAL, *m_dstImage, VK_IMAGE_LAYOUT_GENERAL, 1u, &imageResolve);
				break;
			}
		default:
			DE_FATAL("Unknown Transfer Method!");
			break;
	};

	deUint32 timestampEntry = 0u;

	for (StageFlagVector::const_iterator it = m_stages.begin(); it != m_stages.end(); it++)
	{
		vk.cmdWriteTimestamp(*m_cmdBuffer, *it, *m_queryPool, timestampEntry++);
	}

	endCommandBuffer(vk, *m_cmdBuffer);
}

void TransferTestInstance::initialImageTransition (VkCommandBuffer cmdBuffer, VkImage image, VkImageSubresourceRange subRange, VkImageLayout layout)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();

	const VkImageMemoryBarrier	imageMemBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType          sType;
		DE_NULL,								// const void*              pNext;
		0u,										// VkAccessFlags            srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags            dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout            oldLayout;
		layout,									// VkImageLayout            newLayout;
		VK_QUEUE_FAMILY_IGNORED,				// uint32_t                 srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,				// uint32_t                 dstQueueFamilyIndex;
		image,									// VkImage                  image;
		subRange								// VkImageSubresourceRange  subresourceRange;
	};

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1, &imageMemBarrier);
}

class ResetTimestampQueryBeforeCopyTest : public vkt::TestCase
{
public:
	ResetTimestampQueryBeforeCopyTest							(tcu::TestContext&	testContext,
																 const std::string&	name,
																 const std::string&	description)
		: vkt::TestCase(testContext, name, description)
		{ }
	virtual               ~ResetTimestampQueryBeforeCopyTest	(void) { }
	virtual void          initPrograms							(SourceCollections&	programCollection) const;
	virtual TestInstance* createInstance						(Context&			context) const;
};

class ResetTimestampQueryBeforeCopyTestInstance : public vkt::TestInstance
{
public:
							ResetTimestampQueryBeforeCopyTestInstance	(Context& context);
	virtual					~ResetTimestampQueryBeforeCopyTestInstance	(void) { }
	virtual tcu::TestStatus	iterate										(void);
protected:
	struct TimestampWithAvailability
	{
		deUint64 timestamp;
		deUint64 availability;
	};

	Move<VkCommandPool>		m_cmdPool;
	Move<VkCommandBuffer>	m_cmdBuffer;
	Move<VkQueryPool>		m_queryPool;

	Move<VkBuffer>			m_resultBuffer;
	de::MovePtr<Allocation>	m_resultBufferMemory;
};

void ResetTimestampQueryBeforeCopyTest::initPrograms (SourceCollections& programCollection) const
{
	vkt::TestCase::initPrograms(programCollection);
}

TestInstance* ResetTimestampQueryBeforeCopyTest::createInstance (Context& context) const
{
	return new ResetTimestampQueryBeforeCopyTestInstance(context);
}

ResetTimestampQueryBeforeCopyTestInstance::ResetTimestampQueryBeforeCopyTestInstance (Context& context)
	: vkt::TestInstance(context)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Check support for timestamp queries
	checkTimestampsSupported(context);

	const VkQueryPoolCreateInfo queryPoolParams =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType               sType;
		DE_NULL,									// const void*                   pNext;
		0u,											// VkQueryPoolCreateFlags        flags;
		VK_QUERY_TYPE_TIMESTAMP,					// VkQueryType                   queryType;
		1u,											// deUint32                      entryCount;
		0u,											// VkQueryPipelineStatisticFlags pipelineStatistics;
	};

	m_queryPool		= createQueryPool(vk, vkDevice, &queryPoolParams);
	m_cmdPool		= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer		= allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Create results buffer.
	const VkBufferCreateInfo bufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		sizeof(TimestampWithAvailability),			// VkDeviceSize			size;
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyIndexCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	m_resultBuffer			= createBuffer(vk, vkDevice, &bufferCreateInfo);
	m_resultBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_resultBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *m_resultBuffer, m_resultBufferMemory->getMemory(), m_resultBufferMemory->getOffset()));

	const vk::VkBufferMemoryBarrier bufferBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,										// const void*		pNext;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		vk::VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			dstQueueFamilyIndex;
		*m_resultBuffer,								// VkBuffer			buffer;
		0ull,											// VkDeviceSize		offset;
		VK_WHOLE_SIZE									// VkDeviceSize		size;
	};

	// Prepare command buffer.
	beginCommandBuffer(vk, *m_cmdBuffer, 0u);
	vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
	vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);
	vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
	vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_resultBuffer, 0u, sizeof(TimestampWithAvailability), (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
	vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus ResetTimestampQueryBeforeCopyTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();
	TimestampWithAvailability	ta;

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
	invalidateAlloc(vk, vkDevice, *m_resultBufferMemory);
	deMemcpy(&ta, m_resultBufferMemory->getHostPtr(), sizeof(ta));
	return ((ta.availability != 0)? tcu::TestStatus::fail("Availability bit nonzero after resetting query") : tcu::TestStatus::pass("Pass"));
}


class TwoCmdBuffersTest : public TimestampTest
{
public:
							TwoCmdBuffersTest	(tcu::TestContext&				testContext,
												 const std::string&				name,
												 const std::string&				description,
												 const TwoCmdBuffersTestParam*	param)
: TimestampTest (testContext, name, description, param), m_cmdBufferLevel(param->getCmdBufferLevel()) { }
	virtual					~TwoCmdBuffersTest	(void) { }
	virtual TestInstance*	createInstance		(Context&						context) const;

protected:
	VkCommandBufferLevel	m_cmdBufferLevel;
};

class TwoCmdBuffersTestInstance : public TimestampTestInstance
{
public:
							TwoCmdBuffersTestInstance	(Context&				context,
														 const StageFlagVector	stages,
														 const bool				inRenderPass,
														 const bool				hostQueryReset,
														 VkCommandBufferLevel	cmdBufferLevel,
														 VkQueryResultFlags		queryResultFlags);
	virtual					~TwoCmdBuffersTestInstance	(void);
	virtual tcu::TestStatus	iterate						(void);
protected:
	virtual void			configCommandBuffer			(void);

protected:
	Move<VkCommandBuffer>	m_secondCmdBuffer;
	Move<VkBuffer>			m_dstBuffer;
	de::MovePtr<Allocation> m_dstBufferAlloc;
	VkCommandBufferLevel	m_cmdBufferLevel;
};

TestInstance* TwoCmdBuffersTest::createInstance (Context& context) const
{
	return new TwoCmdBuffersTestInstance(context, m_stages, m_inRenderPass, m_hostQueryReset, m_cmdBufferLevel, m_queryResultFlags);
}

TwoCmdBuffersTestInstance::TwoCmdBuffersTestInstance (Context&					context,
													  const StageFlagVector		stages,
													  const bool				inRenderPass,
													  const bool				hostQueryReset,
													  VkCommandBufferLevel		cmdBufferLevel,
													  VkQueryResultFlags		queryResultFlags)
: TimestampTestInstance (context, stages, inRenderPass, hostQueryReset, queryResultFlags), m_cmdBufferLevel(cmdBufferLevel)
{
	const DeviceInterface&	vk			= context.getDeviceInterface();
	const VkDevice			vkDevice	= context.getDevice();

	m_secondCmdBuffer	= allocateCommandBuffer(vk, vkDevice, *m_cmdPool, cmdBufferLevel);
	m_dstBuffer			= createBufferAndBindMemory(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &m_dstBufferAlloc);
}

TwoCmdBuffersTestInstance::~TwoCmdBuffersTestInstance (void)
{
}

void TwoCmdBuffersTestInstance::configCommandBuffer (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();

	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
		DE_NULL,										// const void*                              pNext;
		0u,												// VkCommandBufferUsageFlags                flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL	// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	const vk::VkBufferMemoryBarrier bufferBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,										// const void*		pNext;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		vk::VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			dstQueueFamilyIndex;
		*m_dstBuffer,									// VkBuffer			buffer;
		0ull,											// VkDeviceSize		offset;
		VK_WHOLE_SIZE									// VkDeviceSize		size;
	};

	if (m_cmdBufferLevel == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
	{
		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
		if (!m_hostQueryReset)
			vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);
		vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, *m_queryPool, 0);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
		VK_CHECK(vk.beginCommandBuffer(*m_secondCmdBuffer, &cmdBufferBeginInfo));
		vk.cmdCopyQueryPoolResults(*m_secondCmdBuffer, *m_queryPool, 0u, 1u, *m_dstBuffer, 0u, 0u, m_queryResultFlags);
		vk.cmdPipelineBarrier(*m_secondCmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u,
							  0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
		VK_CHECK(vk.endCommandBuffer(*m_secondCmdBuffer));
	}
	else
	{
		const VkCommandBufferInheritanceInfo inheritanceInfo		=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	// VkStructureType                  sType;
			DE_NULL,											// const void*                      pNext;
			DE_NULL,											// VkRenderPass                     renderPass;
			0u,													// deUint32                         subpass;
			DE_NULL,											// VkFramebuffer                    framebuffer;
			VK_FALSE,											// VkBool32                         occlusionQueryEnable;
			0u,													// VkQueryControlFlags              queryFlags;
			0u													// VkQueryPipelineStatisticFlags    pipelineStatistics;
		};

		const VkCommandBufferBeginInfo cmdBufferBeginInfoSecondary	=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
			DE_NULL,										// const void*                              pNext;
			0u,												// VkCommandBufferUsageFlags                flags;
			&inheritanceInfo								// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
		};

		VK_CHECK(vk.beginCommandBuffer(*m_secondCmdBuffer, &cmdBufferBeginInfoSecondary));
		vk.cmdResetQueryPool(*m_secondCmdBuffer, *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);
		vk.cmdWriteTimestamp(*m_secondCmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, *m_queryPool, 0);
		VK_CHECK(vk.endCommandBuffer(*m_secondCmdBuffer));
		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
		vk.cmdExecuteCommands(m_cmdBuffer.get(), 1u, &m_secondCmdBuffer.get());
		vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_dstBuffer, 0u, 0u, m_queryResultFlags);
		vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u,
							  0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
	}
}

tcu::TestStatus TwoCmdBuffersTestInstance::iterate (void)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	const VkQueue				queue			= m_context.getUniversalQueue();

	configCommandBuffer();

	const VkCommandBuffer		cmdBuffers[]	= { m_cmdBuffer.get(), m_secondCmdBuffer.get() };

	const VkSubmitInfo			submitInfo		=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,									// VkStructureType                sType;
		DE_NULL,														// const void*                    pNext;
		0u,																// deUint32                       waitSemaphoreCount;
		DE_NULL,														// const VkSemaphore*             pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,							// const VkPipelineStageFlags*    pWaitDstStageMask;
		m_cmdBufferLevel == VK_COMMAND_BUFFER_LEVEL_PRIMARY ? 2u : 1u,	// deUint32                       commandBufferCount;
		cmdBuffers,														// const VkCommandBuffer*         pCommandBuffers;
		0u,																// deUint32                       signalSemaphoreCount;
		DE_NULL,														// const VkSemaphore*             pSignalSemaphores;
	};

	if (m_hostQueryReset)
	{
		// Only reset the pool for the primary command buffer, the secondary command buffer will reset the pool by itself.
		vk.resetQueryPool(m_context.getDevice(), *m_queryPool, 0u, TimestampTest::ENTRY_COUNT);
	}

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, DE_NULL));
	VK_CHECK(vk.queueWaitIdle(queue));

	// Always pass in case no crash occurred.
	return tcu::TestStatus::pass("Pass");
}

class ConsistentQueryResultsTest : public vkt::TestCase
{
public:
	ConsistentQueryResultsTest							(tcu::TestContext&	testContext,
														 const std::string&	name,
														 const std::string&	description)
		: vkt::TestCase(testContext, name, description)
		{ }
	virtual               ~ConsistentQueryResultsTest	(void) { }
	virtual void          initPrograms					(SourceCollections&	programCollection) const;
	virtual TestInstance* createInstance				(Context&			context) const;
};

class ConsistentQueryResultsTestInstance : public vkt::TestInstance
{
public:
							ConsistentQueryResultsTestInstance	(Context& context);
	virtual					~ConsistentQueryResultsTestInstance	(void) { }
	virtual tcu::TestStatus	iterate								(void);
protected:
	Move<VkCommandPool>		m_cmdPool;
	Move<VkCommandBuffer>	m_cmdBuffer;
	Move<VkQueryPool>		m_queryPool;

	deUint64				m_timestampMask;
	Move<VkBuffer>			m_resultBuffer32Bits;
	Move<VkBuffer>			m_resultBuffer64Bits;
	de::MovePtr<Allocation>	m_resultBufferMemory32Bits;
	de::MovePtr<Allocation>	m_resultBufferMemory64Bits;
};

void ConsistentQueryResultsTest::initPrograms(SourceCollections& programCollection) const
{
	vkt::TestCase::initPrograms(programCollection);
}

TestInstance* ConsistentQueryResultsTest::createInstance(Context& context) const
{
	return new ConsistentQueryResultsTestInstance(context);
}

ConsistentQueryResultsTestInstance::ConsistentQueryResultsTestInstance(Context& context)
	: vkt::TestInstance(context)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Check support for timestamp queries
	m_timestampMask = checkTimestampsSupported(context);

	const VkQueryPoolCreateInfo queryPoolParams =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType               sType;
		DE_NULL,									// const void*                   pNext;
		0u,											// VkQueryPoolCreateFlags        flags;
		VK_QUERY_TYPE_TIMESTAMP,					// VkQueryType                   queryType;
		1u,											// deUint32                      entryCount;
		0u,											// VkQueryPipelineStatisticFlags pipelineStatistics;
	};

	m_queryPool		= createQueryPool(vk, vkDevice, &queryPoolParams);
	m_cmdPool		= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer		= allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Create results buffer.
	VkBufferCreateInfo bufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		0u,											// VkDeviceSize			size;
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyIndexCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	// 32 bits.
	bufferCreateInfo.size		= sizeof(deUint32);
	m_resultBuffer32Bits		= createBuffer(vk, vkDevice, &bufferCreateInfo);
	m_resultBufferMemory32Bits	= allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_resultBuffer32Bits), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *m_resultBuffer32Bits, m_resultBufferMemory32Bits->getMemory(), m_resultBufferMemory32Bits->getOffset()));

	// 64 bits.
	bufferCreateInfo.size		= sizeof(deUint64);
	m_resultBuffer64Bits		= createBuffer(vk, vkDevice, &bufferCreateInfo);
	m_resultBufferMemory64Bits	= allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_resultBuffer64Bits), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *m_resultBuffer64Bits, m_resultBufferMemory64Bits->getMemory(), m_resultBufferMemory64Bits->getOffset()));

	vk::VkBufferMemoryBarrier bufferBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,										// const void*		pNext;
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		vk::VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			dstQueueFamilyIndex;
		DE_NULL,										// VkBuffer			buffer;
		0ull,											// VkDeviceSize		offset;
		VK_WHOLE_SIZE									// VkDeviceSize		size;
	};

	// Prepare command buffer.
	beginCommandBuffer(vk, *m_cmdBuffer, 0u);
	vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
	vk.cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);

	// 32 bits.
	bufferBarrier.buffer = *m_resultBuffer32Bits;
	vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_resultBuffer32Bits, 0u, sizeof(deUint32), VK_QUERY_RESULT_WAIT_BIT);
	vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);

	// 64 bits.
	bufferBarrier.buffer = *m_resultBuffer64Bits;
	vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *m_queryPool, 0u, 1u, *m_resultBuffer64Bits, 0u, sizeof(deUint64), (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
	vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);

	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus ConsistentQueryResultsTestInstance::iterate(void)
{
	const DeviceInterface&      vk          = m_context.getDeviceInterface();
	const VkDevice              vkDevice    = m_context.getDevice();
	const VkQueue               queue       = m_context.getUniversalQueue();

	deUint32					tsBuffer32Bits;
	deUint64					tsBuffer64Bits;
	deUint32					tsGet32Bits;
	deUint64					tsGet64Bits;

	constexpr deUint32			maxDeUint32Value = std::numeric_limits<deUint32>::max();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	// Get results from buffers.
	invalidateAlloc(vk, vkDevice, *m_resultBufferMemory32Bits);
	invalidateAlloc(vk, vkDevice, *m_resultBufferMemory64Bits);
	deMemcpy(&tsBuffer32Bits, m_resultBufferMemory32Bits->getHostPtr(), sizeof(tsBuffer32Bits));
	deMemcpy(&tsBuffer64Bits, m_resultBufferMemory64Bits->getHostPtr(), sizeof(tsBuffer64Bits));

	// Get results with vkGetQueryPoolResults().
	VK_CHECK(vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, 1u, sizeof(tsGet32Bits), &tsGet32Bits, sizeof(tsGet32Bits), VK_QUERY_RESULT_WAIT_BIT));
	VK_CHECK(vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, 1u, sizeof(tsGet64Bits), &tsGet64Bits, sizeof(tsGet64Bits), (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)));

	// Check timestamp mask for both 64-bit results.
	checkTimestampBits(tsBuffer64Bits, m_timestampMask);
	checkTimestampBits(tsGet64Bits, m_timestampMask);

	// Check results are consistent.
	if (tsBuffer32Bits == tsGet32Bits &&
		tsBuffer64Bits == tsGet64Bits &&
		(((tsGet64Bits & maxDeUint32Value) == tsGet32Bits) ||
		((tsGet64Bits > maxDeUint32Value) && (maxDeUint32Value == tsGet32Bits))))
	{
		return tcu::TestStatus::pass("Pass");
	}

	std::ostringstream msg;
	msg << std::hex << "Results are inconsistent:"
		<< " B32=0x" << tsBuffer32Bits
		<< " B64=0x" << tsBuffer64Bits
		<< " G32=0x" << tsGet32Bits
		<< " G64=0x" << tsGet64Bits;
	return tcu::TestStatus::fail(msg.str());
}

} // anonymous

tcu::TestCaseGroup* createTimestampTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> timestampTests (new tcu::TestCaseGroup(testCtx, "timestamp", "timestamp tests"));
	const VkQueryResultFlags queryResultFlagsTimestampTest[] =
	{
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT,
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT,
	};

	// Basic Graphics Tests
	{
		de::MovePtr<tcu::TestCaseGroup> basicGraphicsTests (new tcu::TestCaseGroup(testCtx, "basic_graphics_tests", "Record timestamp in different pipeline stages of basic graphics tests"));

		const VkPipelineStageFlagBits basicGraphicsStages0[][2] =
		{
		  {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   VK_PIPELINE_STAGE_VERTEX_INPUT_BIT},
		  {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT},
		  {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
		  {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT},
		  {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT},
		  {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
		  {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT},
		  {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT},
		};
		for (deUint32 stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(basicGraphicsStages0); stageNdx++)
		{
			for (deUint32 flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
			{
				TimestampTestParam param(basicGraphicsStages0[stageNdx], 2u, true, false, queryResultFlagsTimestampTest[flagsIdx]);
				basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
				param.toggleInRenderPass();
				basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
				// Host Query reset tests
				param.toggleHostQueryReset();
				basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
				param.toggleInRenderPass();
				basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
			}
		}

		const VkPipelineStageFlagBits basicGraphicsStages1[][3] =
		{
		  {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT},
		  {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
		};
		for (deUint32 stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(basicGraphicsStages1); stageNdx++)
		{
			for (deUint32 flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
			{
				TimestampTestParam param(basicGraphicsStages1[stageNdx], 3u, true, false, queryResultFlagsTimestampTest[flagsIdx]);
				basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
				param.toggleInRenderPass();
				basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
				// Host Query reset tests
				param.toggleHostQueryReset();
				basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
				param.toggleInRenderPass();
				basicGraphicsTests->addChild(newTestCase<BasicGraphicsTest>(testCtx, &param));
			}
		}

		timestampTests->addChild(basicGraphicsTests.release());
	}

	// Advanced Graphics Tests
	{
		de::MovePtr<tcu::TestCaseGroup> advGraphicsTests (new tcu::TestCaseGroup(testCtx, "advanced_graphics_tests", "Record timestamp in different pipeline stages of advanced graphics tests"));

		const VkPipelineStageFlagBits advGraphicsStages[][2] =
		{
			{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT},
			{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT},
			{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT},
			{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT},
		};
		for (deUint32 stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(advGraphicsStages); stageNdx++)
		{
			for (deUint32 flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
			{
				TimestampTestParam param(advGraphicsStages[stageNdx], 2u, true, false, queryResultFlagsTimestampTest[flagsIdx]);
				advGraphicsTests->addChild(newTestCase<AdvGraphicsTest>(testCtx, &param));
				param.toggleInRenderPass();
				advGraphicsTests->addChild(newTestCase<AdvGraphicsTest>(testCtx, &param));
				// Host Query reset tests
				param.toggleHostQueryReset();
				advGraphicsTests->addChild(newTestCase<AdvGraphicsTest>(testCtx, &param));
				param.toggleInRenderPass();
				advGraphicsTests->addChild(newTestCase<AdvGraphicsTest>(testCtx, &param));
			}
		}

		timestampTests->addChild(advGraphicsTests.release());
	}

	// Basic Compute Tests
	{
		de::MovePtr<tcu::TestCaseGroup> basicComputeTests (new tcu::TestCaseGroup(testCtx, "basic_compute_tests", "Record timestamp for compute stages"));

		const VkPipelineStageFlagBits basicComputeStages[][2] =
		{
			{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
			{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT},
		};
		for (deUint32 stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(basicComputeStages); stageNdx++)
		{
			for (deUint32 flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
			{
				TimestampTestParam param(basicComputeStages[stageNdx], 2u, false, false, queryResultFlagsTimestampTest[flagsIdx]);
				basicComputeTests->addChild(newTestCase<BasicComputeTest>(testCtx, &param));
				// Host Query reset test
				param.toggleHostQueryReset();
				basicComputeTests->addChild(newTestCase<BasicComputeTest>(testCtx, &param));
			}
		}

		timestampTests->addChild(basicComputeTests.release());
	}

	// Transfer Tests
	{
		de::MovePtr<tcu::TestCaseGroup> transferTests (new tcu::TestCaseGroup(testCtx, "transfer_tests", "Record timestamp for transfer stages"));

		const VkPipelineStageFlagBits transferStages[][2] =
		{
			{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT},
			{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_HOST_BIT},
		};

		for (deUint32 stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(transferStages); stageNdx++)
		{
			for (deUint32 method = 0u; method < TRANSFER_METHOD_LAST; method++)
			{
				for (deUint32 flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsTimestampTest); flagsIdx++)
				{
					TransferTimestampTestParam param(transferStages[stageNdx], 2u, false, false, method, queryResultFlagsTimestampTest[flagsIdx]);
					transferTests->addChild(newTestCase<TransferTest>(testCtx, &param));
					// Host Query reset test
					param.toggleHostQueryReset();
					transferTests->addChild(newTestCase<TransferTest>(testCtx, &param));
				}
			}
		}

		timestampTests->addChild(transferTests.release());
	}

	// Calibrated Timestamp Tests.
	{
		de::MovePtr<tcu::TestCaseGroup> calibratedTimestampTests (new tcu::TestCaseGroup(testCtx, "calibrated", "VK_EXT_calibrated_timestamps tests"));

		calibratedTimestampTests->addChild(new CalibratedTimestampTest<CalibratedTimestampDevDomainTestInstance>	(testCtx, "dev_domain_test",	"Test device domain"));
		calibratedTimestampTests->addChild(new CalibratedTimestampTest<CalibratedTimestampHostDomainTestInstance>	(testCtx, "host_domain_test",	"Test host domain"));
		calibratedTimestampTests->addChild(new CalibratedTimestampTest<CalibratedTimestampCalibrationTestInstance>	(testCtx, "calibration_test",	"Test calibration using device and host domains"));

		timestampTests->addChild(calibratedTimestampTests.release());
	}

	// Misc Tests
	{
		const VkQueryResultFlags queryResultFlagsMiscTests[] =
		{
			VK_QUERY_RESULT_WAIT_BIT,
			VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT,
		};

		const std::string queryResultsFlagsMiscTestsStr[] = {"", "_with_availability_bit"};

		de::MovePtr<tcu::TestCaseGroup> miscTests (new tcu::TestCaseGroup(testCtx, "misc_tests", "Misc tests that can not be categorized to other group."));

		for (deUint32 flagsIdx = 0u; flagsIdx < DE_LENGTH_OF_ARRAY(queryResultFlagsMiscTests); flagsIdx++)
		{


			const VkPipelineStageFlagBits miscStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
			TimestampTestParam param(miscStages, 1u, false, false, queryResultFlagsTimestampTest[flagsIdx]);
			miscTests->addChild(new TimestampTest(testCtx,
												"timestamp_only" + queryResultsFlagsMiscTestsStr[flagsIdx],
												"Only write timestamp command in the commmand buffer",
												&param));

			TwoCmdBuffersTestParam twoCmdBuffersParamPrimary(miscStages, 1u, false, false, VK_COMMAND_BUFFER_LEVEL_PRIMARY, queryResultFlagsMiscTests[flagsIdx]);
			miscTests->addChild(new TwoCmdBuffersTest(testCtx,
													"two_cmd_buffers_primary" + queryResultsFlagsMiscTestsStr[flagsIdx],
													"Issue query in a command buffer and copy it on another primary command buffer",
													&twoCmdBuffersParamPrimary));

			TwoCmdBuffersTestParam twoCmdBuffersParamSecondary(miscStages, 1u, false, false, VK_COMMAND_BUFFER_LEVEL_SECONDARY, queryResultFlagsMiscTests[flagsIdx]);
			miscTests->addChild(new TwoCmdBuffersTest(testCtx,
													"two_cmd_buffers_secondary" + queryResultsFlagsMiscTestsStr[flagsIdx],
													"Issue query in a secondary command buffer and copy it on a primary command buffer",
													&twoCmdBuffersParamSecondary));
			// Misc: Host Query Reset tests
			param.toggleHostQueryReset();
			miscTests->addChild(new TimestampTest(testCtx,
												"timestamp_only_host_query_reset" + queryResultsFlagsMiscTestsStr[flagsIdx],
												"Only write timestamp command in the commmand buffer",
												&param));
			TwoCmdBuffersTestParam twoCmdBuffersParamPrimaryHostQueryReset(miscStages, 1u, false, true, VK_COMMAND_BUFFER_LEVEL_PRIMARY, queryResultFlagsMiscTests[flagsIdx]);
			miscTests->addChild(new TwoCmdBuffersTest(testCtx,
													"two_cmd_buffers_primary_host_query_reset" + queryResultsFlagsMiscTestsStr[flagsIdx],
													"Issue query in a command buffer and copy it on another primary command buffer",
													&twoCmdBuffersParamPrimaryHostQueryReset));

			TwoCmdBuffersTestParam twoCmdBuffersParamSecondaryHostQueryReset(miscStages, 1u, false, true, VK_COMMAND_BUFFER_LEVEL_SECONDARY, queryResultFlagsMiscTests[flagsIdx]);
			miscTests->addChild(new TwoCmdBuffersTest(testCtx,
													"two_cmd_buffers_secondary_host_query_reset" + queryResultsFlagsMiscTestsStr[flagsIdx],
													"Issue query in a secondary command buffer and copy it on a primary command buffer",
													&twoCmdBuffersParamSecondaryHostQueryReset));
		}
		// Reset timestamp query before copying results.
		miscTests->addChild(new ResetTimestampQueryBeforeCopyTest(testCtx,
																"reset_query_before_copy",
																"Issue a timestamp query and reset it before copying results"));

		// Check consistency between 32 and 64 bits.
		miscTests->addChild(new ConsistentQueryResultsTest(testCtx,
														"consistent_results",
														"Check consistency between 32-bit and 64-bit timestamp"));

		timestampTests->addChild(miscTests.release());
	}

	return timestampTests.release();
}

} // pipeline

} // vkt
