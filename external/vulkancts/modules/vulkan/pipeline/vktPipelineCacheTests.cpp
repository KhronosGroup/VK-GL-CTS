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
 * \brief Pipeline Cache Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineCacheTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
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
#include "deMemory.h"
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
	};

	return desc.str();
}

// helper classes
class CacheTestParam
{
public:
								CacheTestParam			(const VkShaderStageFlagBits*	shaders,
														 deUint32						count,
														 bool							compileCacheMissShaders);
	virtual						~CacheTestParam			(void);
	virtual const std::string	generateTestName		(void)			const;
	virtual const std::string	generateTestDescription	(void)			const;
	VkShaderStageFlagBits		getShaderFlag			(deUint32 ndx)	const	{ return m_shaders[ndx]; }
	deUint32					getShaderCount			(void)			const	{ return (deUint32)m_shaderCount; }
	bool						getCompileMissShaders	(void)			const	{ return m_compileCacheMissShaders;	}
protected:
	VkShaderStageFlagBits		m_shaders[VK_MAX_SHADER_STAGES];
	size_t						m_shaderCount;
	bool						m_compileCacheMissShaders;
};

CacheTestParam::CacheTestParam (const VkShaderStageFlagBits* shaders, deUint32 count, bool compileCacheMissShaders)
	: m_compileCacheMissShaders	(compileCacheMissShaders)
{
	DE_ASSERT(count <= VK_MAX_SHADER_STAGES);

	for (deUint32 ndx = 0; ndx < count; ndx++)
		m_shaders[ndx] = shaders[ndx];

	m_shaderCount = count;
}

CacheTestParam::~CacheTestParam (void)
{
}

const std::string CacheTestParam::generateTestName (void) const
{
	std::string result(getShaderFlagStr(m_shaders[0], false));

	for(deUint32 ndx = 1; ndx < m_shaderCount; ndx++)
		result += '_' + getShaderFlagStr(m_shaders[ndx], false) ;

	return result;
}

const std::string CacheTestParam::generateTestDescription (void) const
{
	std::string result("Create pipeline cache with " + getShaderFlagStr(m_shaders[0], true));

	for(deUint32 ndx = 1; ndx < m_shaderCount; ndx++)
		result += ' ' + getShaderFlagStr(m_shaders[ndx], true);

	return result;
}

class SimpleGraphicsPipelineBuilder
{
public:
							SimpleGraphicsPipelineBuilder	(Context&				context);
							~SimpleGraphicsPipelineBuilder	(void) { }
	void					bindShaderStage					(VkShaderStageFlagBits	stage,
															 const char*			sourceName,
															 const char*			entryName);
	void					enableTessellationStage			(deUint32				patchControlPoints);
	Move<VkPipeline>		buildPipeline					(tcu::UVec2				renderSize,
															 VkRenderPass			renderPass,
															 VkPipelineCache		cache,
															 VkPipelineLayout		pipelineLayout);
protected:
	Context&							m_context;
	Move<VkShaderModule>				m_shaderModules[VK_MAX_SHADER_STAGES];
	deUint32							m_shaderStageCount;
	VkPipelineShaderStageCreateInfo		m_shaderStageInfo[VK_MAX_SHADER_STAGES];
	deUint32							m_patchControlPoints;
};

SimpleGraphicsPipelineBuilder::SimpleGraphicsPipelineBuilder (Context& context)
	: m_context(context)
{
	m_patchControlPoints = 0;
	m_shaderStageCount   = 0;
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
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,                // VkStructureType             sType;
		DE_NULL,                                                    // const void*                 pNext;
		0u,                                                         // VkShaderModuleCreateFlags   flags;
		codeSize,                                                   // deUintptr                   codeSize;
		code,                                                       // const deUint32*             pCode;
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

Move<VkPipeline> SimpleGraphicsPipelineBuilder::buildPipeline (tcu::UVec2 renderSize, VkRenderPass renderPass, VkPipelineCache cache, VkPipelineLayout pipelineLayout)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

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
			0u,								// deUint32 location;
			0u,								// deUint32 binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat format;
			0u								// deUint32 offsetInBytes;
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

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		0u,																// VkPipelineInputAssemblyStateCreateFlags  flags;
		(m_patchControlPoints == 0 ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
								   : VK_PRIMITIVE_TOPOLOGY_PATCH_LIST),	// VkPrimitiveTopology                      topology;
		VK_FALSE,														// VkBool32                                 primitiveRestartEnable;
	};

	const VkViewport	viewport	= makeViewport(renderSize);
	const VkRect2D		scissor		= makeRect2D(renderSize);

	const VkPipelineViewportStateCreateInfo viewportStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                      sType;
		DE_NULL,												// const void*                          pNext;
		0u,														// VkPipelineViewportStateCreateFlags   flags;
		1u,														// deUint32                             viewportCount;
		&viewport,												// const VkViewport*                    pViewports;
		1u,														// deUint32                             scissorCount;
		&scissor												// const VkRect2D*                      pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo rasterStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                          sType;
		DE_NULL,													// const void*                              pNext;
		0u,															// VkPipelineRasterizationStateCreateFlags  flags;
		VK_FALSE,													// VkBool32                                 depthClampEnable;
		VK_FALSE,													// VkBool32                                 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                            polygonMode;
		VK_CULL_MODE_NONE,											// VkCullModeFlags                          cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace                              frontFace;
		VK_FALSE,													// VkBool32                                 depthBiasEnable;
		0.0f,														// float                                    depthBiasConstantFactor;
		0.0f,														// float                                    depthBiasClamp;
		0.0f,														// float                                    depthBiasSlopeFactor;
		1.0f,														// float                                    lineWidth;
	};

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE,						// VkBool32                 blendEnable;
		VK_BLEND_FACTOR_ONE,			// VkBlendFactor            srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            dstColorBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp                colorBlendOp;
		VK_BLEND_FACTOR_ONE,			// VkBlendFactor            srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp                alphaBlendOp;
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT		// VkColorComponentFlags    colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                              sType;
		DE_NULL,													// const void*                                  pNext;
		0u,															// VkPipelineColorBlendStateCreateFlags         flags;
		VK_FALSE,													// VkBool32                                     logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp                                    logicOp;
		1u,															// deUint32                                     attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*   pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float                                        blendConst[4];
	};

	const VkPipelineMultisampleStateCreateInfo  multisampleStateParams  =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType                          sType;
		DE_NULL,													// const void*                              pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags    flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits                    rasterizationSamples;
		VK_FALSE,													// VkBool32                                 sampleShadingEnable;
		0.0f,														// float                                    minSampleShading;
		DE_NULL,													// const VkSampleMask*                      pSampleMask;
		VK_FALSE,													// VkBool32                                 alphaToCoverageEnable;
		VK_FALSE,													// VkBool32                                 alphaToOneEnable;
	};

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

	const VkPipelineTessellationStateCreateInfo tessStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	// VkStructureType                          sType;
		DE_NULL,													// const void*                              pNext;
		0u,															// VkPipelineTesselationStateCreateFlags    flags;
		m_patchControlPoints,										// deUint32                                 patchControlPoints;
	};
	const VkPipelineTessellationStateCreateInfo* pTessCreateInfo = (m_patchControlPoints > 0)
																  ? &tessStateCreateInfo
																  : DE_NULL;

	const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType                                  sType;
		DE_NULL,											// const void*                                      pNext;
		0u,													// VkPipelineCreateFlags                            flags;
		m_shaderStageCount,									// deUint32                                         stageCount;
		m_shaderStageInfo,									// const VkPipelineShaderStageCreateInfo*           pStages;
		&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
		&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
		pTessCreateInfo,									// const VkPipelineTessellationStateCreateInfo*     pTessellationState;
		&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*         pViewportState;
		&rasterStateParams,									// const VkPipelineRasterizationStateCreateInfo*    pRasterState;
		&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
		&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
		&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
		(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*          pDynamicState;
		pipelineLayout,										// VkPipelineLayout                                 layout;
		renderPass,											// VkRenderPass                                     renderPass;
		0u,													// deUint32                                         subpass;
		0u,													// VkPipeline                                       basePipelineHandle;
		0,													// deInt32                                          basePipelineIndex;
	};

	return createGraphicsPipeline(vk, vkDevice, cache, &graphicsPipelineParams);
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

Move<VkBuffer> createBufferAndBindMemory (Context& context, VkDeviceSize size, VkBufferUsageFlags usage, de::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,       // VkStructureType      sType;
		DE_NULL,                                    // const void*          pNext;
		0u,                                         // VkBufferCreateFlags  flags;
		size,                                       // VkDeviceSize         size;
		usage,                                      // VkBufferUsageFlags   usage;
		VK_SHARING_MODE_EXCLUSIVE,                  // VkSharingMode        sharingMode;
		1u,                                         // deUint32             queueFamilyCount;
		&queueFamilyIndex                           // const deUint32*      pQueueFamilyIndices;
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
										  de::details::MovePtr<Allocation>*	pAlloc)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

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

	*pAlloc = context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *image, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	return image;
}

// Test Classes
class CacheTest : public vkt::TestCase
{
public:
							CacheTest	(tcu::TestContext&		testContext,
										 const std::string&		name,
										 const std::string&		description,
										 const CacheTestParam*	param)
							  : vkt::TestCase (testContext, name, description)
							  , m_param (*param)
							  { }
	virtual					~CacheTest (void) { }
protected:
	const CacheTestParam	m_param;
};

class CacheTestInstance : public vkt::TestInstance
{
public:
	enum
	{
		PIPELINE_CACHE_NDX_NO_CACHE,
		PIPELINE_CACHE_NDX_CACHED,
		PIPELINE_CACHE_NDX_COUNT,
	};
							CacheTestInstance		(Context&				context,
													 const CacheTestParam*	param);
	virtual					~CacheTestInstance		(void);
	virtual tcu::TestStatus	iterate					(void);
protected:
	virtual tcu::TestStatus	verifyTestResult		(void) = 0;
	virtual void			prepareCommandBuffer	(void) = 0;
protected:
	const CacheTestParam*   m_param;
	Move<VkCommandPool>		m_cmdPool;
	Move<VkCommandBuffer>	m_cmdBuffer;
	Move<VkPipelineCache>	m_cache;
};

CacheTestInstance::CacheTestInstance (Context&				context,
									  const CacheTestParam*	param)
	: TestInstance	(context)
	, m_param		(param)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Create the Pipeline Cache
	{
		const VkPipelineCacheCreateInfo pipelineCacheCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
			DE_NULL,										// const void*                 pNext;
			0u,												// VkPipelineCacheCreateFlags  flags;
			0u,												// deUintptr                   initialDataSize;
			DE_NULL,										// const void*                 pInitialData;
		};

		m_cache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
	}
}

CacheTestInstance::~CacheTestInstance (void)
{
}

tcu::TestStatus CacheTestInstance::iterate (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();
	const VkQueue			queue		= m_context.getUniversalQueue();

	prepareCommandBuffer();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

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
	virtual                 ~GraphicsCacheTest	(void) { }
	virtual void            initPrograms		(SourceCollections&		programCollection) const;
	virtual void			checkSupport		(Context&				context) const;
	virtual TestInstance*   createInstance		(Context&				context) const;
};

class GraphicsCacheTestInstance : public CacheTestInstance
{
public:
							GraphicsCacheTestInstance   (Context&				context,
														 const CacheTestParam*	param);
	virtual					~GraphicsCacheTestInstance	(void);
protected:
			void			prepareRenderPass			(VkFramebuffer framebuffer, VkPipeline pipeline);
	virtual void			prepareCommandBuffer		(void);
	virtual tcu::TestStatus	verifyTestResult			(void);

protected:
	const tcu::UVec2				m_renderSize;
	const VkFormat					m_colorFormat;
	const VkFormat					m_depthFormat;
	Move<VkPipelineLayout>			m_pipelineLayout;

	Move<VkImage>					m_depthImage;
	de::MovePtr<Allocation>			m_depthImageAlloc;
	de::MovePtr<Allocation>			m_colorImageAlloc[PIPELINE_CACHE_NDX_COUNT];
	Move<VkImageView>				m_depthAttachmentView;
	VkImageMemoryBarrier			m_imageLayoutBarriers[3];

	Move<VkBuffer>					m_vertexBuffer;
	de::MovePtr<Allocation>			m_vertexBufferMemory;
	std::vector<Vertex4RGBA>		m_vertices;

	SimpleGraphicsPipelineBuilder	m_pipelineBuilder;
	SimpleGraphicsPipelineBuilder	m_missPipelineBuilder;
	Move<VkRenderPass>				m_renderPass;

	Move<VkImage>					m_colorImage[PIPELINE_CACHE_NDX_COUNT];
	Move<VkImageView>				m_colorAttachmentView[PIPELINE_CACHE_NDX_COUNT];
	Move<VkFramebuffer>				m_framebuffer[PIPELINE_CACHE_NDX_COUNT];
	Move<VkPipeline>				m_pipeline[PIPELINE_CACHE_NDX_COUNT];
};

void GraphicsCacheTest::initPrograms (SourceCollections& programCollection) const
{
	enum ShaderCacheOpType
	{
		SHADERS_CACHE_OP_HIT = 0,
		SHADERS_CACHE_OP_MISS,

		SHADERS_CACHE_OP_LAST
	};

	for (deUint32 shaderOpNdx = 0u; shaderOpNdx < SHADERS_CACHE_OP_LAST; shaderOpNdx++)
	{
		const ShaderCacheOpType shaderOp = (ShaderCacheOpType)shaderOpNdx;

		if (shaderOp == SHADERS_CACHE_OP_MISS && !m_param.getCompileMissShaders())
			continue;

		const std::string missHitDiff = (shaderOp == SHADERS_CACHE_OP_HIT ? "" : " + 0.1");
		const std::string missSuffix = (shaderOp == SHADERS_CACHE_OP_HIT ? "" : "_miss");

		for (deUint32 shaderNdx = 0; shaderNdx < m_param.getShaderCount(); shaderNdx++)
		{
			switch(m_param.getShaderFlag(shaderNdx))
			{
				case VK_SHADER_STAGE_VERTEX_BIT:
					programCollection.glslSources.add("color_vert" + missSuffix) << glu::VertexSource(
						"#version 310 es\n"
						"layout(location = 0) in vec4 position;\n"
						"layout(location = 1) in vec4 color;\n"
						"layout(location = 0) out highp vec4 vtxColor;\n"
						"void main (void)\n"
						"{\n"
						"  gl_Position = position;\n"
						"  vtxColor = color" + missHitDiff + ";\n"
						"}\n");
					break;

				case VK_SHADER_STAGE_FRAGMENT_BIT:
					programCollection.glslSources.add("color_frag" + missSuffix) << glu::FragmentSource(
						"#version 310 es\n"
						"layout(location = 0) in highp vec4 vtxColor;\n"
						"layout(location = 0) out highp vec4 fragColor;\n"
						"void main (void)\n"
						"{\n"
						"  fragColor = vtxColor" + missHitDiff + ";\n"
						"}\n");
					break;

				case VK_SHADER_STAGE_GEOMETRY_BIT:
					programCollection.glslSources.add("unused_geo" + missSuffix) << glu::GeometrySource(
						"#version 450 \n"
						"layout(triangles) in;\n"
						"layout(triangle_strip, max_vertices = 3) out;\n"
						"layout(location = 0) in highp vec4 in_vtxColor[];\n"
						"layout(location = 0) out highp vec4 vtxColor;\n"
						"out gl_PerVertex { vec4 gl_Position; };\n"
						"in gl_PerVertex { vec4 gl_Position; } gl_in[];\n"
						"void main (void)\n"
						"{\n"
						"  for(int ndx=0; ndx<3; ndx++)\n"
						"  {\n"
						"    gl_Position = gl_in[ndx].gl_Position;\n"
						"    vtxColor    = in_vtxColor[ndx]" + missHitDiff + ";\n"
						"    EmitVertex();\n"
						"  }\n"
						"  EndPrimitive();\n"
						"}\n");
					break;

				case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
					programCollection.glslSources.add("basic_tcs" + missSuffix) << glu::TessellationControlSource(
						"#version 450 \n"
						"layout(vertices = 3) out;\n"
						"layout(location = 0) in highp vec4 color[];\n"
						"layout(location = 0) out highp vec4 vtxColor[];\n"
						"out gl_PerVertex { vec4 gl_Position; } gl_out[3];\n"
						"in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
						"void main()\n"
						"{\n"
						"  gl_TessLevelOuter[0] = 4.0;\n"
						"  gl_TessLevelOuter[1] = 4.0;\n"
						"  gl_TessLevelOuter[2] = 4.0;\n"
						"  gl_TessLevelInner[0] = 4.0;\n"
						"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
						"  vtxColor[gl_InvocationID] = color[gl_InvocationID]" + missHitDiff + ";\n"
						"}\n");
					break;

				case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
					programCollection.glslSources.add("basic_tes" + missSuffix) << glu::TessellationEvaluationSource(
						"#version 450 \n"
						"layout(triangles, fractional_even_spacing, ccw) in;\n"
						"layout(location = 0) in highp vec4 colors[];\n"
						"layout(location = 0) out highp vec4 vtxColor;\n"
						"out gl_PerVertex { vec4 gl_Position; };\n"
						"in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
						"void main() \n"
						"{\n"
						"  float u = gl_TessCoord.x;\n"
						"  float v = gl_TessCoord.y;\n"
						"  float w = gl_TessCoord.z;\n"
						"  vec4 pos = vec4(0);\n"
						"  vec4 color = vec4(0)" + missHitDiff + ";\n"
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
					break;

				default:
					DE_FATAL("Unknown Shader Stage!");
					break;
			};
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
		};
	}
}

TestInstance* GraphicsCacheTest::createInstance (Context& context) const
{
	return new GraphicsCacheTestInstance(context, &m_param);
}

GraphicsCacheTestInstance::GraphicsCacheTestInstance (Context&				context,
													  const CacheTestParam*	param)
	: CacheTestInstance		(context,param)
	, m_renderSize			(32u, 32u)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthFormat			(VK_FORMAT_D16_UNORM)
	, m_pipelineBuilder		(context)
	, m_missPipelineBuilder	(context)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create vertex buffer
	{
		m_vertexBuffer	= createBufferAndBindMemory(m_context, 1024u, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &m_vertexBufferMemory);

		m_vertices		= createOverlappingQuads();
		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferMemory->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(vk, vkDevice, *m_vertexBufferMemory);
	}

	// Create render pass
	m_renderPass = makeRenderPass(vk, vkDevice, m_colorFormat, m_depthFormat);

	const VkComponentMapping ComponentMappingRGBA = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
	// Create color image
	{
		m_colorImage[PIPELINE_CACHE_NDX_NO_CACHE]	= createImage2DAndBindMemory(m_context,
																				 m_colorFormat,
																				 m_renderSize.x(),
																				 m_renderSize.y(),
																				 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																				 VK_SAMPLE_COUNT_1_BIT,
																				 &m_colorImageAlloc[PIPELINE_CACHE_NDX_NO_CACHE]);
		m_colorImage[PIPELINE_CACHE_NDX_CACHED]		= createImage2DAndBindMemory(m_context,
																				 m_colorFormat,
																				 m_renderSize.x(),
																				 m_renderSize.y(),
																				 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																				 VK_SAMPLE_COUNT_1_BIT,
																				 &m_colorImageAlloc[PIPELINE_CACHE_NDX_CACHED]);
	}

	// Create depth image
	{
		m_depthImage = createImage2DAndBindMemory(m_context,
												  m_depthFormat,
												  m_renderSize.x(),
												  m_renderSize.y(),
												  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
												  VK_SAMPLE_COUNT_1_BIT,
												  &m_depthImageAlloc);
	}

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
			*m_colorImage[PIPELINE_CACHE_NDX_NO_CACHE],			// VkImage					image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },		// VkImageSubresourceRange	subresourceRange;
		};

		m_imageLayoutBarriers[0] = colorImageBarrier;

		colorImageBarrier.image = *m_colorImage[PIPELINE_CACHE_NDX_CACHED];
		m_imageLayoutBarriers[1] = colorImageBarrier;

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

		m_imageLayoutBarriers[2] = depthImageBarrier;
	}
	// Create color attachment view
	{
		VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType          sType;
			DE_NULL,										// const void*              pNext;
			0u,												// VkImageViewCreateFlags   flags;
			*m_colorImage[PIPELINE_CACHE_NDX_NO_CACHE],		// VkImage                  image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType          viewType;
			m_colorFormat,									// VkFormat                 format;
			ComponentMappingRGBA,							// VkComponentMapping       components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange  subresourceRange;
		};

		m_colorAttachmentView[PIPELINE_CACHE_NDX_NO_CACHE] = createImageView(vk, vkDevice, &colorAttachmentViewParams);

		colorAttachmentViewParams.image = *m_colorImage[PIPELINE_CACHE_NDX_CACHED];
		m_colorAttachmentView[PIPELINE_CACHE_NDX_CACHED] = createImageView(vk, vkDevice, &colorAttachmentViewParams);
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
			m_depthFormat,									// VkFormat                 format;
			ComponentMappingRGBA,							// VkComponentMapping       components;
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange  subresourceRange;
		};

		m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
	}

	// Create framebuffer
	{
		VkImageView attachmentBindInfos[2] =
		{
			*m_colorAttachmentView[PIPELINE_CACHE_NDX_NO_CACHE],
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
			(deUint32)m_renderSize.x(),					// deUint32                     width;
			(deUint32)m_renderSize.y(),					// deUint32                     height;
			1u,											// deUint32                     layers;
		};

		m_framebuffer[PIPELINE_CACHE_NDX_NO_CACHE] = createFramebuffer(vk, vkDevice, &framebufferParams);

		attachmentBindInfos[0] = *m_colorAttachmentView[PIPELINE_CACHE_NDX_CACHED];
		m_framebuffer[PIPELINE_CACHE_NDX_CACHED] = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Bind shader stages
	for (deUint32 shaderNdx = 0; shaderNdx < m_param->getShaderCount(); shaderNdx++)
	{
		switch(m_param->getShaderFlag(shaderNdx))
		{
			case VK_SHADER_STAGE_VERTEX_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "color_vert", "main");
				if (m_param->getCompileMissShaders())
				{
					m_missPipelineBuilder.bindShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "color_vert_miss", "main");
				}
				break;
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "color_frag", "main");
				if (m_param->getCompileMissShaders())
				{
					m_missPipelineBuilder.bindShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "color_frag_miss", "main");
				}
				break;
			case VK_SHADER_STAGE_GEOMETRY_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, "unused_geo", "main");
				if (m_param->getCompileMissShaders())
				{
					m_missPipelineBuilder.bindShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, "unused_geo_miss", "main");
				}
				break;
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "basic_tcs", "main");
				m_pipelineBuilder.enableTessellationStage(3);
				if (m_param->getCompileMissShaders())
				{
					m_missPipelineBuilder.bindShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "basic_tcs_miss", "main");
					m_missPipelineBuilder.enableTessellationStage(3);
				}
				break;
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				m_pipelineBuilder.bindShaderStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "basic_tes", "main");
				m_pipelineBuilder.enableTessellationStage(3);
				if (m_param->getCompileMissShaders())
				{
					m_missPipelineBuilder.bindShaderStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "basic_tes_miss", "main");
					m_missPipelineBuilder.enableTessellationStage(3);
				}
				break;
			default:
				DE_FATAL("Unknown Shader Stage!");
				break;
		};
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkPipelineLayoutCreateFlags		flags;
			0u,												// deUint32							setLayoutCount;
			DE_NULL,										// const VkDescriptorSetLayout*		pSetLayouts;
			0u,												// deUint32							pushConstantRangeCount;
			DE_NULL											// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE]	= m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *m_cache, *m_pipelineLayout);
	m_pipeline[PIPELINE_CACHE_NDX_CACHED]	= m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *m_cache, *m_pipelineLayout);
}

GraphicsCacheTestInstance::~GraphicsCacheTestInstance (void)
{
}

void GraphicsCacheTestInstance::prepareRenderPass (VkFramebuffer framebuffer, VkPipeline pipeline)
{
	const DeviceInterface&	vk							= m_context.getDeviceInterface();

	const VkClearValue		attachmentClearValues[2]	=
	{
		defaultClearValue(m_colorFormat),
		defaultClearValue(m_depthFormat),
	};

	beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u, attachmentClearValues);

	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkDeviceSize offsets = 0u;
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);
	vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1u, 0u, 0u);

	endRenderPass(vk, *m_cmdBuffer);
}

void GraphicsCacheTestInstance::prepareCommandBuffer (void)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
		0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers), m_imageLayoutBarriers);

	prepareRenderPass(*m_framebuffer[PIPELINE_CACHE_NDX_NO_CACHE], *m_pipeline[PIPELINE_CACHE_NDX_NO_CACHE]);

	// After the first render pass, the images are in correct layouts

	prepareRenderPass(*m_framebuffer[PIPELINE_CACHE_NDX_CACHED], *m_pipeline[PIPELINE_CACHE_NDX_CACHED]);

	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus GraphicsCacheTestInstance::verifyTestResult (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					vkDevice			= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const VkQueue					queue				= m_context.getUniversalQueue();
	de::MovePtr<tcu::TextureLevel>	resultNoCache		= readColorAttachment(vk,
																			  vkDevice,
																			  queue,
																			  queueFamilyIndex,
																			  m_context.getDefaultAllocator(),
																			  *m_colorImage[PIPELINE_CACHE_NDX_NO_CACHE],
																			  m_colorFormat,
																			  m_renderSize);
	de::MovePtr<tcu::TextureLevel>	resultCache			= readColorAttachment(vk,
																			  vkDevice,
																			  queue,
																			  queueFamilyIndex,
																			  m_context.getDefaultAllocator(),
																			  *m_colorImage[PIPELINE_CACHE_NDX_CACHED],
																			  m_colorFormat,
																			  m_renderSize);

	bool compareOk = tcu::intThresholdCompare(m_context.getTestContext().getLog(),
											  "IntImageCompare",
											  "Image comparison",
											  resultNoCache->getAccess(),
											  resultCache->getAccess(),
											  tcu::UVec4(1, 1, 1, 1),
											  tcu::COMPARE_LOG_RESULT);

	if (compareOk)
		return tcu::TestStatus::pass("Render images w/o cached pipeline match.");
	else
		return tcu::TestStatus::fail("Render Images mismatch.");
}

class ComputeCacheTest : public CacheTest
{
public:
							ComputeCacheTest	(tcu::TestContext&		testContext,
												 const std::string&		name,
												 const std::string&		description,
												 const CacheTestParam*	param)
								: CacheTest (testContext, name, description, param)
								{ }
	virtual					~ComputeCacheTest	(void) { }
	virtual void			initPrograms		(SourceCollections&		programCollection) const;
	virtual TestInstance*	createInstance		(Context&				context) const;
};

class ComputeCacheTestInstance : public CacheTestInstance
{
public:
							ComputeCacheTestInstance	(Context&				context,
														 const CacheTestParam*	param);
	virtual					~ComputeCacheTestInstance	(void);
	virtual void			prepareCommandBuffer		(void);
protected:
	virtual tcu::TestStatus	verifyTestResult			(void);
			void			buildBuffers				(void);
			void			buildDescriptorSets			(deUint32 ndx);
			void			buildShader					(void);
			void			buildPipeline				(deUint32 ndx);
protected:
	Move<VkBuffer>				m_inputBuf;
	de::MovePtr<Allocation>		m_inputBufferAlloc;
	Move<VkShaderModule>		m_computeShaderModule;

	Move<VkBuffer>				m_outputBuf[PIPELINE_CACHE_NDX_COUNT];
	de::MovePtr<Allocation>		m_outputBufferAlloc[PIPELINE_CACHE_NDX_COUNT];

	Move<VkDescriptorPool>		m_descriptorPool[PIPELINE_CACHE_NDX_COUNT];
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout[PIPELINE_CACHE_NDX_COUNT];
	Move<VkDescriptorSet>		m_descriptorSet[PIPELINE_CACHE_NDX_COUNT];

	Move<VkPipelineLayout>		m_pipelineLayout[PIPELINE_CACHE_NDX_COUNT];
	Move<VkPipeline>			m_pipeline[PIPELINE_CACHE_NDX_COUNT];
};

void ComputeCacheTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("basic_compute") << glu::ComputeSource(
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
}

TestInstance* ComputeCacheTest::createInstance (Context& context) const
{
	return new ComputeCacheTestInstance(context, &m_param);
}

void ComputeCacheTestInstance::buildBuffers (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create buffer object, allocate storage, and generate input data
	const VkDeviceSize		size		= sizeof(tcu::Vec4) * 128u;
	m_inputBuf = createBufferAndBindMemory(m_context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_inputBufferAlloc);

	// Initialize input buffer
	tcu::Vec4* pVec = reinterpret_cast<tcu::Vec4*>(m_inputBufferAlloc->getHostPtr());
	for (deUint32 ndx = 0u; ndx < 128u; ndx++)
	{
		for (deUint32 component = 0u; component < 4u; component++)
			pVec[ndx][component]= (float)(ndx * (component + 1u));
	}
	flushAlloc(vk, vkDevice, *m_inputBufferAlloc);

	// Clear the output buffer
	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		m_outputBuf[ndx] = createBufferAndBindMemory(m_context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_outputBufferAlloc[ndx]);

		pVec = reinterpret_cast<tcu::Vec4*>(m_outputBufferAlloc[ndx]->getHostPtr());

		for (deUint32 i = 0; i < (size / sizeof(tcu::Vec4)); i++)
			pVec[i] = tcu::Vec4(0.0f);

		flushAlloc(vk, vkDevice, *m_outputBufferAlloc[ndx]);
	}
}

void ComputeCacheTestInstance::buildDescriptorSets (deUint32 ndx)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create descriptor set layout
	DescriptorSetLayoutBuilder descLayoutBuilder;

	for (deUint32 bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
		descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

	m_descriptorSetLayout[ndx] = descLayoutBuilder.build(vk, vkDevice);

	std::vector<VkDescriptorBufferInfo> descriptorInfos;
	descriptorInfos.push_back(makeDescriptorBufferInfo(*m_inputBuf, 0u, sizeof(tcu::Vec4) * 128u));
	descriptorInfos.push_back(makeDescriptorBufferInfo(*m_outputBuf[ndx], 0u, sizeof(tcu::Vec4) * 128u));

	// Create descriptor pool
	m_descriptorPool[ndx] = DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u).build(vk,
																										 vkDevice,
																										 VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
																										 1u);

	// Create descriptor set
	const VkDescriptorSetAllocateInfo descriptorSetAllocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		*m_descriptorPool[ndx],							// VkDescriptorPool                descriptorPool;
		1u,												// deUint32                        setLayoutCount;
		&m_descriptorSetLayout[ndx].get(),				// const VkDescriptorSetLayout*    pSetLayouts;
	};
	m_descriptorSet[ndx] = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocInfo);

	DescriptorSetUpdateBuilder  builder;
	for (deUint32 descriptorNdx = 0u; descriptorNdx < 2u; descriptorNdx++)
	{
		builder.writeSingle(*m_descriptorSet[ndx],
							DescriptorSetUpdateBuilder::Location::binding(descriptorNdx),
							VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							&descriptorInfos[descriptorNdx]);
	}
	builder.update(vk, vkDevice);
}

void ComputeCacheTestInstance::buildShader (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

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
}

void ComputeCacheTestInstance::buildPipeline (deUint32 ndx)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create compute pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		0u,												// VkPipelineLayoutCreateFlags     flags;
		1u,												// deUint32                        setLayoutCount;
		&m_descriptorSetLayout[ndx].get(),				// const VkDescriptorSetLayout*    pSetLayouts;
		0u,												// deUint32                        pushConstantRangeCount;
		DE_NULL,										// const VkPushConstantRange*      pPushConstantRanges;
	};

	m_pipelineLayout[ndx] = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

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
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType                 sType;
		DE_NULL,											// const void*                     pNext;
		0u,													// VkPipelineCreateFlags           flags;
		stageCreateInfo,									// VkPipelineShaderStageCreateInfo stage;
		*m_pipelineLayout[ndx],								// VkPipelineLayout                layout;
		(VkPipeline)0,										// VkPipeline                      basePipelineHandle;
		0u,													// deInt32                         basePipelineIndex;
	};

	m_pipeline[ndx] = createComputePipeline(vk, vkDevice, *m_cache, &pipelineCreateInfo);
}

ComputeCacheTestInstance::ComputeCacheTestInstance (Context&				context,
													const CacheTestParam*	param)
	: CacheTestInstance (context, param)
{
	buildBuffers();

	buildDescriptorSets(PIPELINE_CACHE_NDX_NO_CACHE);

	buildDescriptorSets(PIPELINE_CACHE_NDX_CACHED);

	buildShader();

	buildPipeline(PIPELINE_CACHE_NDX_NO_CACHE);

	buildPipeline(PIPELINE_CACHE_NDX_CACHED);
}

ComputeCacheTestInstance::~ComputeCacheTestInstance (void)
{
}

void ComputeCacheTestInstance::prepareCommandBuffer (void)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline[ndx]);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout[ndx], 0u, 1u, &m_descriptorSet[ndx].get(), 0u, DE_NULL);
		vk.cmdDispatch(*m_cmdBuffer, 128u, 1u, 1u);
	}

	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus ComputeCacheTestInstance::verifyTestResult (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Read the content of output buffers
	invalidateAlloc(vk, vkDevice, *m_outputBufferAlloc[PIPELINE_CACHE_NDX_NO_CACHE]);

	invalidateAlloc(vk, vkDevice, *m_outputBufferAlloc[PIPELINE_CACHE_NDX_CACHED]);
	// Compare the content
	deUint8* bufNoCache = reinterpret_cast<deUint8*>(m_outputBufferAlloc[PIPELINE_CACHE_NDX_NO_CACHE]->getHostPtr());
	deUint8* bufCached  = reinterpret_cast<deUint8*>(m_outputBufferAlloc[PIPELINE_CACHE_NDX_CACHED]->getHostPtr());
	for (deUint32 ndx = 0u; ndx < sizeof(tcu::Vec4) * 128u; ndx++)
	{
		if (bufNoCache[ndx] != bufCached[ndx])
		{
			return tcu::TestStatus::fail("Output buffers w/o cached pipeline mismatch.");
		}
	}

	return tcu::TestStatus::pass("Output buffers w/o cached pipeline match.");
}

class PipelineFromCacheTest : public GraphicsCacheTest
{
public:
							PipelineFromCacheTest		(tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param);
	virtual					~PipelineFromCacheTest		(void) { }
	virtual TestInstance*	createInstance				(Context& context) const;
};

PipelineFromCacheTest::PipelineFromCacheTest (tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param)
	: GraphicsCacheTest(testContext, name, description, param)
{
}

class PipelineFromCacheTestInstance : public GraphicsCacheTestInstance
{
public:
							PipelineFromCacheTestInstance	(Context& context, const CacheTestParam* param);
	virtual					~PipelineFromCacheTestInstance	(void);
protected:
	Move<VkPipelineCache>	m_newCache;
	deUint8*				m_data;
};

TestInstance* PipelineFromCacheTest::createInstance (Context& context) const
{
	return new PipelineFromCacheTestInstance(context, &m_param);
}

PipelineFromCacheTestInstance::PipelineFromCacheTestInstance (Context& context, const CacheTestParam* param)
	: GraphicsCacheTestInstance	(context, param)
	, m_data					(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	{
		size_t dataSize	= 0u;

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		m_data = new deUint8[dataSize];
		DE_ASSERT(m_data);
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data));

		const VkPipelineCacheCreateInfo pipelineCacheCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
			DE_NULL,										// const void*                 pNext;
			0u,												// VkPipelineCacheCreateFlags  flags;
			dataSize,										// deUintptr                   initialDataSize;
			m_data,											// const void*                 pInitialData;
		};
		m_newCache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
	}
	m_pipeline[PIPELINE_CACHE_NDX_CACHED] = m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *m_newCache, *m_pipelineLayout);
}

PipelineFromCacheTestInstance::~PipelineFromCacheTestInstance (void)
{
	delete[] m_data;
}

class PipelineFromIncompleteCacheTest : public GraphicsCacheTest
{
public:
							PipelineFromIncompleteCacheTest		(tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param);
	virtual					~PipelineFromIncompleteCacheTest	(void) {}
	virtual TestInstance*	createInstance						(Context& context) const;
};

PipelineFromIncompleteCacheTest::PipelineFromIncompleteCacheTest (tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param)
	: GraphicsCacheTest(testContext, name, description, param)
{
}

class PipelineFromIncompleteCacheTestInstance : public GraphicsCacheTestInstance
{
public:
							PipelineFromIncompleteCacheTestInstance(Context& context, const CacheTestParam* param);
	virtual					~PipelineFromIncompleteCacheTestInstance(void);
protected:
protected:
	Move<VkPipelineCache>	m_newCache;
	deUint8*				m_data;
};

TestInstance* PipelineFromIncompleteCacheTest::createInstance (Context& context) const
{
	return new PipelineFromIncompleteCacheTestInstance(context, &m_param);
}

PipelineFromIncompleteCacheTestInstance::PipelineFromIncompleteCacheTestInstance (Context& context, const CacheTestParam* param)
	: GraphicsCacheTestInstance	(context, param)
	, m_data					(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	{
		size_t dataSize = 0u;
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		if (dataSize == 0)
			TCU_THROW(NotSupportedError, "Empty pipeline cache - unable to test");

		dataSize--;

		m_data = new deUint8[dataSize];
		DE_ASSERT(m_data);
		if (vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data) != VK_INCOMPLETE)
			TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

		const VkPipelineCacheCreateInfo pipelineCacheCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
			DE_NULL,										// const void*                 pNext;
			0u,												// VkPipelineCacheCreateFlags  flags;
			dataSize,										// deUintptr                   initialDataSize;
			m_data,											// const void*                 pInitialData;
		};
		m_newCache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
	}
	m_pipeline[PIPELINE_CACHE_NDX_CACHED] = m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *m_newCache, *m_pipelineLayout);
}

PipelineFromIncompleteCacheTestInstance::~PipelineFromIncompleteCacheTestInstance (void)
{
	delete[] m_data;
}

enum MergeCacheType
{
	MERGE_CACHE_EMPTY = 0,
	MERGE_CACHE_FROM_DATA,
	MERGE_CACHE_HIT,
	MERGE_CACHE_MISS,
	MERGE_CACHE_MISS_AND_HIT,
	MERGE_CACHE_MERGED,

	MERGE_CACHE_TYPE_LAST = MERGE_CACHE_MERGED
};

std::string getMergeCacheTypeStr (MergeCacheType type)
{
	switch (type)
	{
		case MERGE_CACHE_EMPTY:
			return "empty";
		case MERGE_CACHE_FROM_DATA:
			return "from_data";
		case MERGE_CACHE_HIT:
			return "hit";
		case MERGE_CACHE_MISS_AND_HIT:
			return "misshit";
		case MERGE_CACHE_MISS:
			return "miss";
		case MERGE_CACHE_MERGED:
			return "merged";
	}
	TCU_FAIL("unhandled merge cache type");
}

std::string getMergeCacheTypesStr (const std::vector<MergeCacheType>& types)
{
	std::string ret;
	for (size_t idx = 0; idx < types.size(); ++idx)
	{
		if (ret.size())
			ret += '_';
		ret += getMergeCacheTypeStr(types[idx]);
	}
	return ret;
}


class MergeCacheTestParam
{
public:
	MergeCacheType				destCacheType;
	std::vector<MergeCacheType> srcCacheTypes;
};

class MergeCacheTest : public GraphicsCacheTest
{
public:
								MergeCacheTest	(tcu::TestContext&			testContext,
												 const std::string&			name,
												 const std::string&			description,
												 const CacheTestParam*		param,
												 const MergeCacheTestParam* mergeCacheParam)
									: GraphicsCacheTest (testContext, name, description, param)
									, m_mergeCacheParam	(*mergeCacheParam)
									{ }
	virtual						~MergeCacheTest	(void) { }
	virtual TestInstance*		createInstance	(Context& context) const;
private:
	const MergeCacheTestParam	m_mergeCacheParam;
};

class MergeCacheTestInstance : public GraphicsCacheTestInstance
{
public:
							MergeCacheTestInstance	(Context&					context,
													 const CacheTestParam*		param,
													 const MergeCacheTestParam* mergeCacheParam);
private:
	Move<VkPipelineCache>	createPipelineCache		(const DeviceInterface& vk, VkDevice device, MergeCacheType type);

protected:
	Move<VkPipelineCache>	m_cacheMerged;
};

TestInstance* MergeCacheTest::createInstance (Context& context) const
{
	return new MergeCacheTestInstance(context, &m_param, &m_mergeCacheParam);
}

MergeCacheTestInstance::MergeCacheTestInstance (Context& context, const CacheTestParam* param, const MergeCacheTestParam* mergeCacheParam)
	: GraphicsCacheTestInstance (context, param)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create a merge destination cache
	m_cacheMerged = createPipelineCache(vk, vkDevice, mergeCacheParam->destCacheType);

	// Create more pipeline caches
	std::vector<VkPipelineCache>	sourceCaches	(mergeCacheParam->srcCacheTypes.size());
	typedef de::SharedPtr<Move<VkPipelineCache> > PipelineCachePtr;
	std::vector<PipelineCachePtr>	sourceCachePtrs	(sourceCaches.size());
	{
		for (size_t sourceIdx = 0; sourceIdx < mergeCacheParam->srcCacheTypes.size(); sourceIdx++)
		{
			// vk::Move is not copyable, so create it on heap and wrap into de::SharedPtr
			PipelineCachePtr	pipelineCachePtr	(new Move<VkPipelineCache>());
			*pipelineCachePtr = createPipelineCache(vk, vkDevice, mergeCacheParam->srcCacheTypes[sourceIdx]);

			sourceCachePtrs[sourceIdx]	= pipelineCachePtr;
			sourceCaches[sourceIdx]		= **pipelineCachePtr;
		}
	}

	// Merge the caches
	VK_CHECK(vk.mergePipelineCaches(vkDevice, *m_cacheMerged, static_cast<deUint32>(sourceCaches.size()), &sourceCaches[0]));

	// Create pipeline from merged cache
	m_pipeline[PIPELINE_CACHE_NDX_CACHED] = m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *m_cacheMerged, *m_pipelineLayout);
}

Move<VkPipelineCache> MergeCacheTestInstance::createPipelineCache (const DeviceInterface& vk, VkDevice device, MergeCacheType type)
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
		DE_NULL,										// const void*                 pNext;
		0u,												// VkPipelineCacheCreateFlags  flags;
		0u,												// deUintptr                   initialDataSize;
		DE_NULL,										// const void*                 pInitialData;
	};

	switch (type)
	{
		case MERGE_CACHE_EMPTY:
		{
			return vk::createPipelineCache(vk, device, &pipelineCacheCreateInfo);
		}
		case MERGE_CACHE_FROM_DATA:
		{
			// Create a cache with init data from m_cache
			size_t  dataSize = 0u;
			VK_CHECK(vk.getPipelineCacheData(device, *m_cache, (deUintptr*)&dataSize, DE_NULL));

			std::vector<deUint8> data(dataSize);
			VK_CHECK(vk.getPipelineCacheData(device, *m_cache, (deUintptr*)&dataSize, &data[0]));

			pipelineCacheCreateInfo.initialDataSize = data.size();
			pipelineCacheCreateInfo.pInitialData = &data[0];
			return vk::createPipelineCache(vk, device, &pipelineCacheCreateInfo);
		}
		case MERGE_CACHE_HIT:
		{
			Move<VkPipelineCache> ret = createPipelineCache(vk, device, MERGE_CACHE_EMPTY);

			m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *ret, *m_pipelineLayout);

			return ret;
		}
		case MERGE_CACHE_MISS:
		{
			Move<VkPipelineCache> ret = createPipelineCache(vk, device, MERGE_CACHE_EMPTY);

			m_missPipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *ret, *m_pipelineLayout);

			return ret;
		}
		case MERGE_CACHE_MISS_AND_HIT:
		{
			Move<VkPipelineCache> ret = createPipelineCache(vk, device, MERGE_CACHE_EMPTY);

			m_pipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *ret, *m_pipelineLayout);
			m_missPipelineBuilder.buildPipeline(m_renderSize, *m_renderPass, *ret, *m_pipelineLayout);

			return ret;
		}
		case MERGE_CACHE_MERGED:
		{
			Move<VkPipelineCache>	cache1			= createPipelineCache(vk, device, MERGE_CACHE_FROM_DATA);
			Move<VkPipelineCache>	cache2			= createPipelineCache(vk, device, MERGE_CACHE_HIT);
			Move<VkPipelineCache>	cache3			= createPipelineCache(vk, device, MERGE_CACHE_MISS);

			const VkPipelineCache	sourceCaches[]	=
			{
				*cache1,
				*cache2,
				*cache3
			};

			Move<VkPipelineCache>	ret				= createPipelineCache(vk, device, MERGE_CACHE_EMPTY);

			// Merge the caches
			VK_CHECK(vk.mergePipelineCaches(device, *ret, DE_LENGTH_OF_ARRAY(sourceCaches), sourceCaches));

			return ret;
		}
	}
	TCU_FAIL("unhandled merge cache type");
}

class CacheHeaderTest : public GraphicsCacheTest
{
public:
			CacheHeaderTest		(tcu::TestContext&		testContext,
								 const std::string&		name,
								 const std::string&		description,
								 const CacheTestParam*	param)
								: GraphicsCacheTest(testContext, name, description, param)
	{ }
	virtual	~CacheHeaderTest	(void) { }
	virtual	TestInstance*		createInstance(Context& context) const;
};

class CacheHeaderTestInstance : public GraphicsCacheTestInstance
{
public:
				CacheHeaderTestInstance		(Context& context, const CacheTestParam*  param);
	virtual		~CacheHeaderTestInstance	(void);
protected:
	deUint8*	m_data;

	struct CacheHeader
	{
		deUint32	HeaderLength;
		deUint32	HeaderVersion;
		deUint32	VendorID;
		deUint32	DeviceID;
		deUint8		PipelineCacheUUID[VK_UUID_SIZE];
	} m_header;
};

TestInstance* CacheHeaderTest::createInstance (Context& context) const
{
	return new CacheHeaderTestInstance(context, &m_param);
}

CacheHeaderTestInstance::CacheHeaderTestInstance (Context& context, const CacheTestParam* param)
	: GraphicsCacheTestInstance	(context, param)
	, m_data					(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	{
		// Create a cache with init data from m_cache
		size_t dataSize = 0u;
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		if (dataSize < sizeof(m_header))
			TCU_THROW(TestError, "Pipeline cache size is smaller than header size");

		m_data = new deUint8[dataSize];
		DE_ASSERT(m_data);
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data));

		deMemcpy(&m_header, m_data, sizeof(m_header));

		if (m_header.HeaderLength - VK_UUID_SIZE != 16)
			TCU_THROW(TestError, "Invalid header size!");

		if (m_header.HeaderVersion != 1)
			TCU_THROW(TestError, "Invalid header version!");

		if (m_header.VendorID != m_context.getDeviceProperties().vendorID)
			TCU_THROW(TestError, "Invalid header vendor ID!");

		if (m_header.DeviceID != m_context.getDeviceProperties().deviceID)
			TCU_THROW(TestError, "Invalid header device ID!");

		if (deMemCmp(&m_header.PipelineCacheUUID, &m_context.getDeviceProperties().pipelineCacheUUID, VK_UUID_SIZE) != 0)
			TCU_THROW(TestError, "Invalid header pipeline cache UUID!");
	}
}

CacheHeaderTestInstance::~CacheHeaderTestInstance (void)
{
	delete[] m_data;
}

class InvalidSizeTest : public GraphicsCacheTest
{
public:
							InvalidSizeTest		(tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param);
	virtual					~InvalidSizeTest	(void) {}
	virtual TestInstance*	createInstance		(Context& context) const;
};

InvalidSizeTest::InvalidSizeTest (tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param)
	: GraphicsCacheTest(testContext, name, description, param)
{
}

class InvalidSizeTestInstance : public GraphicsCacheTestInstance
{
public:
							InvalidSizeTestInstance		(Context& context, const CacheTestParam*  param);
	virtual					~InvalidSizeTestInstance	(void);
protected:
	deUint8*				m_data;
	deUint8*				m_zeroBlock;
};

TestInstance* InvalidSizeTest::createInstance (Context& context) const
{
	return new InvalidSizeTestInstance(context, &m_param);
}

InvalidSizeTestInstance::InvalidSizeTestInstance (Context& context, const CacheTestParam* param)
	: GraphicsCacheTestInstance	(context, param)
	, m_data					(DE_NULL)
	, m_zeroBlock				(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	try
	{
		// Create a cache with init data from m_cache
		size_t dataSize			= 0u;
		size_t savedDataSize	= 0u;
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));
		savedDataSize = dataSize;

		// If the value of dataSize is less than the maximum size that can be retrieved by the pipeline cache,
		// at most pDataSize bytes will be written to pData, and vkGetPipelineCacheData will return VK_INCOMPLETE.
		dataSize--;

		m_data = new deUint8[savedDataSize];
		deMemset(m_data, 0, savedDataSize);
		DE_ASSERT(m_data);
		if (vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data) != VK_INCOMPLETE)
			TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

		delete[] m_data;
		m_data = DE_NULL;

		// If the value of dataSize is less than what is necessary to store the header,
		// nothing will be written to pData and zero will be written to dataSize.
		dataSize = 16 + VK_UUID_SIZE - 1;

		m_data = new deUint8[savedDataSize];
		deMemset(m_data, 0, savedDataSize);
		DE_ASSERT(m_data);
		if (vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data) != VK_INCOMPLETE)
			TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

		m_zeroBlock = new deUint8[savedDataSize];
		deMemset(m_zeroBlock, 0, savedDataSize);
		if (deMemCmp(m_data, m_zeroBlock, savedDataSize) != 0 || dataSize != 0)
			TCU_THROW(TestError, "Data needs to be empty and data size should be 0 when invalid size is passed to GetPipelineCacheData!");
	}
	catch (...)
	{
		delete[] m_data;
		delete[] m_zeroBlock;
		throw;
	}
}

InvalidSizeTestInstance::~InvalidSizeTestInstance (void)
{
	delete[] m_data;
	delete[] m_zeroBlock;
}

class ZeroSizeTest : public GraphicsCacheTest
{
public:
							ZeroSizeTest	(tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param);
	virtual					~ZeroSizeTest	(void) {}
	virtual TestInstance*	createInstance	(Context& context) const;
};

ZeroSizeTest::ZeroSizeTest (tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param)
	: GraphicsCacheTest(testContext, name, description, param)
{
}

class ZeroSizeTestInstance : public GraphicsCacheTestInstance
{
public:
							ZeroSizeTestInstance	(Context& context, const CacheTestParam* param);
	virtual					~ZeroSizeTestInstance	(void);
protected:
	deUint8*				m_data;
	deUint8*				m_zeroBlock;
};

TestInstance* ZeroSizeTest::createInstance (Context& context) const
{
	return new ZeroSizeTestInstance(context, &m_param);
}

ZeroSizeTestInstance::ZeroSizeTestInstance (Context& context, const CacheTestParam* param)
	: GraphicsCacheTestInstance	(context, param)
	, m_data					(DE_NULL)
	, m_zeroBlock				(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	try
	{
		// Create a cache with init data from m_cache
		size_t dataSize = 0u;

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		m_data = new deUint8[dataSize];
		deMemset(m_data, 0, dataSize);
		DE_ASSERT(m_data);

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data));

		{
			// Create a cache with initialDataSize = 0 & pInitialData != NULL
			const VkPipelineCacheCreateInfo	pipelineCacheCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
				DE_NULL,										// const void*                 pNext;
				0u,												// VkPipelineCacheCreateFlags  flags;
				0u,												// deUintptr                   initialDataSize;
				m_data,											// const void*                 pInitialData;
			};

			const Unique<VkPipelineCache>	pipelineCache			(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));
		}
	}
	catch (...)
	{
		delete[] m_data;
		delete[] m_zeroBlock;
		throw;
	}
}

ZeroSizeTestInstance::~ZeroSizeTestInstance (void)
{
	delete[] m_data;
	delete[] m_zeroBlock;
}

class InvalidBlobTest : public GraphicsCacheTest
{
public:
							InvalidBlobTest		(tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param);
	virtual					~InvalidBlobTest	(void) {}
	virtual TestInstance*	createInstance		(Context& context) const;
};

InvalidBlobTest::InvalidBlobTest (tcu::TestContext& testContext, const std::string& name, const std::string& description, const CacheTestParam* param)
	: GraphicsCacheTest(testContext, name, description, param)
{
}

class InvalidBlobTestInstance : public GraphicsCacheTestInstance
{
public:
							InvalidBlobTestInstance		(Context& context, const CacheTestParam* param);
	virtual					~InvalidBlobTestInstance	(void);
protected:
	deUint8*				m_data;
	deUint8*				m_zeroBlock;
};

TestInstance* InvalidBlobTest::createInstance (Context& context) const
{
	return new InvalidBlobTestInstance(context, &m_param);
}

InvalidBlobTestInstance::InvalidBlobTestInstance (Context& context, const CacheTestParam* param)
	: GraphicsCacheTestInstance	(context, param)
	, m_data					(DE_NULL)
	, m_zeroBlock				(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	try
	{
		// Create a cache with init data from m_cache
		size_t dataSize = 0u;

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		m_data = new deUint8[dataSize];
		deMemset(m_data, 0, dataSize);
		DE_ASSERT(m_data);

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data));

		const struct
		{
			deUint32	offset;
			std::string	name;
		} headerLayout[] =
		{
			{ 4u,	"pipeline cache header version"	},
			{ 8u,	"vendor ID"						},
			{ 12u,	"device ID"						},
			{ 16u,	"pipeline cache ID"				}
		};

		for (deUint32 i = 0u; i < DE_LENGTH_OF_ARRAY(headerLayout); i++)
		{
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "Creating pipeline cache using previously retrieved data with invalid " << headerLayout[i].name << tcu::TestLog::EndMessage;

			m_data[headerLayout[i].offset] = (deUint8)(m_data[headerLayout[i].offset] + 13u);	// Add arbitrary number to create an invalid value

			const VkPipelineCacheCreateInfo	pipelineCacheCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
				DE_NULL,										// const void*                 pNext;
				0u,												// VkPipelineCacheCreateFlags  flags;
				dataSize,										// deUintptr                   initialDataSize;
				m_data,											// const void*                 pInitialData;
			};

			const Unique<VkPipelineCache>	pipelineCache			(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));

			m_data[headerLayout[i].offset] = (deUint8)(m_data[headerLayout[i].offset] - 13u);	// Return to original value
		}
	}
	catch (...)
	{
		delete[] m_data;
		delete[] m_zeroBlock;
		throw;
	}
}

InvalidBlobTestInstance::~InvalidBlobTestInstance (void)
{
	delete[] m_data;
	delete[] m_zeroBlock;
}
} // anonymous

tcu::TestCaseGroup* createCacheTests (tcu::TestContext& testCtx)
{

	de::MovePtr<tcu::TestCaseGroup> cacheTests (new tcu::TestCaseGroup(testCtx, "cache", "pipeline cache tests"));

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests (new tcu::TestCaseGroup(testCtx, "graphics_tests", "Test pipeline cache with graphics pipeline."));

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
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), false),
			CacheTestParam(testParamShaders1, DE_LENGTH_OF_ARRAY(testParamShaders1), false),
			CacheTestParam(testParamShaders2, DE_LENGTH_OF_ARRAY(testParamShaders2), false),
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			graphicsTests->addChild(newTestCase<GraphicsCacheTest>(testCtx, &testParams[i]));

		cacheTests->addChild(graphicsTests.release());
	}

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "pipeline_from_get_data", "Test pipeline cache with graphics pipeline."));

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
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), false),
			CacheTestParam(testParamShaders1, DE_LENGTH_OF_ARRAY(testParamShaders1), false),
			CacheTestParam(testParamShaders2, DE_LENGTH_OF_ARRAY(testParamShaders2), false),
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			graphicsTests->addChild(newTestCase<PipelineFromCacheTest>(testCtx, &testParams[i]));

		cacheTests->addChild(graphicsTests.release());
	}

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "pipeline_from_incomplete_get_data", "Test pipeline cache with graphics pipeline."));

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
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), false),
			CacheTestParam(testParamShaders1, DE_LENGTH_OF_ARRAY(testParamShaders1), false),
			CacheTestParam(testParamShaders2, DE_LENGTH_OF_ARRAY(testParamShaders2), false),
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			graphicsTests->addChild(newTestCase<PipelineFromIncompleteCacheTest>(testCtx, &testParams[i]));

		cacheTests->addChild(graphicsTests.release());
	}

	// Compute Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> computeTests (new tcu::TestCaseGroup(testCtx, "compute_tests", "Test pipeline cache with compute pipeline."));

		const VkShaderStageFlagBits testParamShaders0[] =
		{
			VK_SHADER_STAGE_COMPUTE_BIT,
		};
		const CacheTestParam testParams[] =
		{
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), false),
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			computeTests->addChild(newTestCase<ComputeCacheTest>(testCtx, &testParams[i]));

		cacheTests->addChild(computeTests.release());
	}

	// Merge cache Tests
	{
		de::MovePtr<tcu::TestCaseGroup> mergeTests (new tcu::TestCaseGroup(testCtx, "merge", "Cache merging tests"));

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
			CacheTestParam(testParamShaders0, DE_LENGTH_OF_ARRAY(testParamShaders0), true),
			CacheTestParam(testParamShaders1, DE_LENGTH_OF_ARRAY(testParamShaders1), true),
			CacheTestParam(testParamShaders2, DE_LENGTH_OF_ARRAY(testParamShaders2), true),
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
		{

			de::MovePtr<tcu::TestCaseGroup> mergeStagesTests(new tcu::TestCaseGroup(testCtx, testParams[i].generateTestName().c_str(), testParams[i].generateTestDescription().c_str()));

			for (deUint32 destTypeIdx = 0u; destTypeIdx <= MERGE_CACHE_TYPE_LAST; destTypeIdx++)
			for (deUint32 srcType1Idx = 0u; srcType1Idx <= MERGE_CACHE_TYPE_LAST; srcType1Idx++)
			{

				MergeCacheTestParam cacheTestParam;
				cacheTestParam.destCacheType = MergeCacheType(destTypeIdx);
				cacheTestParam.srcCacheTypes.push_back(MergeCacheType(srcType1Idx));

				// merge with one cache
				{
					std::string testName = "src_" + getMergeCacheTypesStr(cacheTestParam.srcCacheTypes) + "_dst_" + getMergeCacheTypeStr(cacheTestParam.destCacheType);
					mergeStagesTests->addChild(new MergeCacheTest(testCtx,
															testName.c_str(),
															"Merge the caches test.",
															&testParams[i],
															&cacheTestParam));
				}

				// merge with two caches
				for (deUint32 srcType2Idx = 0u; srcType2Idx <= MERGE_CACHE_TYPE_LAST; srcType2Idx++)
				{
					MergeCacheTestParam cacheTestParamTwoCaches = cacheTestParam;

					cacheTestParamTwoCaches.srcCacheTypes.push_back(MergeCacheType(srcType2Idx));

					std::string testName = "src_" + getMergeCacheTypesStr(cacheTestParamTwoCaches.srcCacheTypes) + "_dst_" + getMergeCacheTypeStr(cacheTestParamTwoCaches.destCacheType);
					mergeStagesTests->addChild(new MergeCacheTest(testCtx,
														   testName.c_str(),
														   "Merge the caches test.",
														   &testParams[i],
														   &cacheTestParamTwoCaches));
				}
			}
			mergeTests->addChild(mergeStagesTests.release());
		}
		cacheTests->addChild(mergeTests.release());
	}

	// Misc Tests
	{
		de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "misc_tests", "Misc tests that can not be categorized to other group."));

		const VkShaderStageFlagBits testParamShaders[] =
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		const CacheTestParam testParam(testParamShaders, DE_LENGTH_OF_ARRAY(testParamShaders), false);


		miscTests->addChild(new CacheHeaderTest(testCtx,
											   "cache_header_test",
											   "Cache header test.",
											   &testParam));

		miscTests->addChild(new InvalidSizeTest(testCtx,
												"invalid_size_test",
												"Invalid size test.",
												&testParam));

		miscTests->addChild(new ZeroSizeTest(testCtx,
											 "zero_size_test",
											 "Zero size test.",
											 &testParam));

		miscTests->addChild(new InvalidBlobTest(testCtx,
												"invalid_blob_test",
												"Invalid cache blob test.",
												&testParam));

		cacheTests->addChild(miscTests.release());
	}

	return cacheTests.release();
}

} // pipeline

} // vkt
