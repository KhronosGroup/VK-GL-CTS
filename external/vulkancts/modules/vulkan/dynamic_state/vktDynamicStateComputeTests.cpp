/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Dynamic State tests mixing it with compute and transfer.
 *//*--------------------------------------------------------------------*/
#include "vktDynamicStateComputeTests.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuVector.hpp"

#include <vector>
#include <string>
#include <functional>
#include <map>
#include <sstream>
#include <cstring>
#include <iterator>
#include <numeric>
#include <memory>

namespace vkt
{
namespace DynamicState
{

namespace
{

using namespace vk;

// Additional objects needed to set a given dynamic state that need to exist beyond the state-setting call. Empty by default.
struct DynamicStateData
{
	virtual ~DynamicStateData() {}
};

// A vertex buffer and graphics pipeline are needed for vkCmdBindVertexBuffers2EXT().
struct BindVertexBuffersData : public DynamicStateData
{
private:
	using BufferPtr			= de::MovePtr<BufferWithMemory>;
	using RenderPassPtr		= RenderPassWrapper;
	using LayoutPtr			= Move<VkPipelineLayout>;
	using ModulePtr			= Move<VkShaderModule>;
	using PipelinePtr		= Move<VkPipeline>;

	static constexpr deUint32 kWidth	= 16u;
	static constexpr deUint32 kHeight	= 16u;

	VkExtent3D getExtent (void)
	{
		return makeExtent3D(kWidth, kHeight, 1u);
	}

public:
	BindVertexBuffersData(Context& ctx, VkDevice device, PipelineConstructionType pipelineConstructionType)
		: m_vertexBuffer		()
		, m_dataSize			(0u)
		, m_vertexBufferSize	(0ull)
		, m_renderPass			()
		, m_pipelineLayout		()
		, m_vertexShader		()
		, m_graphicsPipeline	()
	{
		const auto&	vki			= ctx.getInstanceInterface();
		const auto	phyDev		= ctx.getPhysicalDevice();
		const auto&	vkd			= ctx.getDeviceInterface();
		auto&		alloc		= ctx.getDefaultAllocator();

		// Vertex buffer.
		tcu::Vec4	vertex		(0.f, 0.f, 0.f, 1.f);
		m_dataSize				= sizeof(vertex);
		m_vertexBufferSize		= de::roundUp(static_cast<VkDeviceSize>(m_dataSize), getPhysicalDeviceProperties(vki, phyDev).limits.nonCoherentAtomSize);
		const auto	bufferInfo	= makeBufferCreateInfo(m_vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		m_vertexBuffer			= BufferPtr(new BufferWithMemory(vkd, device, alloc, bufferInfo, MemoryRequirement::HostVisible));
		auto&	bufferAlloc		= m_vertexBuffer->getAllocation();

		deMemcpy(bufferAlloc.getHostPtr(), &vertex, m_dataSize);
		flushAlloc(vkd, device, bufferAlloc);

		// Empty render pass.
		m_renderPass = RenderPassWrapper(pipelineConstructionType, vkd, device);

		// Empty pipeline layout.
		m_pipelineLayout = makePipelineLayout(vkd, device);

		// Passthrough vertex shader.
		m_vertexShader = createShaderModule(vkd, device, ctx.getBinaryCollection().get("vert"), 0u);

		const auto						extent		= getExtent();
		const std::vector<VkViewport>	viewports	(1, makeViewport(extent));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(extent));
		const VkDynamicState			state		= VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT;

		const VkPipelineDynamicStateCreateInfo dynamicStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
			nullptr,												//	const void*							pNext;
			0u,														//	VkPipelineDynamicStateCreateFlags	flags;
			1u,														//	deUint32							dynamicStateCount;
			&state,													//	const VkDynamicState*				pDynamicStates;
		};

		// Graphics pipeline.
		m_graphicsPipeline = makeGraphicsPipeline(vkd, device, m_pipelineLayout.get(),
			m_vertexShader.get(), DE_NULL, DE_NULL, DE_NULL, DE_NULL,
			m_renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
			nullptr, nullptr, nullptr, nullptr, nullptr, &dynamicStateInfo);
	}

	const BufferWithMemory*	getVertexBuffer () const
	{
		return m_vertexBuffer.get();
	}

	size_t getDataSize () const
	{
		return m_dataSize;
	}

	VkPipeline getPipeline () const
	{
		return m_graphicsPipeline.get();
	}

	virtual ~BindVertexBuffersData() {}

private:
	BufferPtr		m_vertexBuffer;
	size_t			m_dataSize;
	VkDeviceSize	m_vertexBufferSize;
	RenderPassPtr	m_renderPass;
	LayoutPtr		m_pipelineLayout;
	ModulePtr		m_vertexShader;
	PipelinePtr		m_graphicsPipeline;
};

// Function that records a state-setting command in the given command buffer.
using RecordStateFunction = std::function<void(const DeviceInterface*, VkCommandBuffer, const DynamicStateData*)>;

// State-setting functions
void setViewport (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkViewport viewport =
	{
		0.0f,	//	float	x;
		0.0f,	//	float	y;
		1.0f,	//	float	width;
		1.0f,	//	float	height;
		0.0f,	//	float	minDepth;
		1.0f,	//	float	maxDepth;
	};
	vkd->cmdSetViewport(cmdBuffer, 0u, 1u, &viewport);
}

void setScissor (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkRect2D scissor =
	{
		{ 0, 0 },	//	VkOffset2D	offset;
		{ 1u, 1u },	//	VkExtent2D	extent;
	};
	vkd->cmdSetScissor(cmdBuffer, 0u, 1u, &scissor);
}

void setLineWidth (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	vkd->cmdSetLineWidth(cmdBuffer, 1.0f);
}

void setDepthBias (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	vkd->cmdSetDepthBias(cmdBuffer, 0.0f, 0.0f, 0.0f);
}

void setBlendConstants (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const float blendConstants[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	vkd->cmdSetBlendConstants(cmdBuffer, blendConstants);
}

void setDepthBounds (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	vkd->cmdSetDepthBounds(cmdBuffer, 0.0f, 1.0f);
}

void setStencilCompareMask (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	vkd->cmdSetStencilCompareMask(cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFu);
}

void setStencilWriteMask (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	vkd->cmdSetStencilWriteMask(cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFu);
}

void setStencilReference (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	vkd->cmdSetStencilReference(cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFu);
}

void setDiscardRectangle (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkRect2D rectangle =
	{
		{ 0, 0 },	//	VkOffset2D	offset;
		{ 1u, 1u },	//	VkExtent2D	extent;
	};
	vkd->cmdSetDiscardRectangleEXT(cmdBuffer, 0u, 1u, &rectangle);
}

void setSampleLocations (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkSampleLocationEXT locations[] =
	{
		{ 0.5f, 0.5f },
		{ 0.5f, 1.5f },
		{ 1.5f, 0.5f },
		{ 1.5f, 1.5f },
	};
	const VkSampleLocationsInfoEXT info =
	{
		VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT,	//	VkStructureType				sType;
		nullptr,										//	const void*					pNext;
		VK_SAMPLE_COUNT_4_BIT,							//	VkSampleCountFlagBits		sampleLocationsPerPixel;
		{ 1u, 1u },										//	VkExtent2D					sampleLocationGridSize;
		4u,												//	deUint32					sampleLocationsCount;
		locations,										//	const VkSampleLocationEXT*	pSampleLocations;
	};
	vkd->cmdSetSampleLocationsEXT(cmdBuffer, &info);
}

#ifndef CTS_USES_VULKANSC
void setRTPipelineStatckSize (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	vkd->cmdSetRayTracingPipelineStackSizeKHR(cmdBuffer, 4096u);
}
#endif // CTS_USES_VULKANSC

void setFragmentShadingRage (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkExtent2D							fragmentSize	= { 1u, 1u };
	const VkFragmentShadingRateCombinerOpKHR	combinerOps[2]	=
	{
		VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
		VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
	};
	vkd->cmdSetFragmentShadingRateKHR(cmdBuffer, &fragmentSize, combinerOps);
}

void setLineStipple (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	vkd->cmdSetLineStippleEXT(cmdBuffer, 1u, 1u);
}

void setCullMode (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetCullMode(cmdBuffer, VK_CULL_MODE_FRONT_AND_BACK);
#else
	vkd->cmdSetCullModeEXT(cmdBuffer, VK_CULL_MODE_FRONT_AND_BACK);
#endif // CTS_USES_VULKANSC
}

void setFrontFace (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetFrontFace(cmdBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
#else
	vkd->cmdSetFrontFaceEXT(cmdBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
#endif // CTS_USES_VULKANSC
}

void setPrimitiveTopology (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetPrimitiveTopology(cmdBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
#else
	vkd->cmdSetPrimitiveTopologyEXT(cmdBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
#endif // CTS_USES_VULKANSC
}

void setViewportWithCount (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkViewport viewport =
	{
		0.0f,	//	float	x;
		0.0f,	//	float	y;
		1.0f,	//	float	width;
		1.0f,	//	float	height;
		0.0f,	//	float	minDepth;
		1.0f,	//	float	maxDepth;
	};
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetViewportWithCount(cmdBuffer, 1u, &viewport);
#else
	vkd->cmdSetViewportWithCountEXT(cmdBuffer, 1u, &viewport);
#endif // CTS_USES_VULKANSC
}

void setScissorWithCount (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkRect2D scissor =
	{
		{ 0, 0 },	//	VkOffset2D	offset;
		{ 1u, 1u },	//	VkExtent2D	extent;
	};
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetScissorWithCount(cmdBuffer, 1u, &scissor);
#else
	vkd->cmdSetScissorWithCountEXT(cmdBuffer, 1u, &scissor);
#endif // CTS_USES_VULKANSC
}

void bindVertexBuffers (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData* data)
{
	const auto bindData			= dynamic_cast<const BindVertexBuffersData*>(data);
	DE_ASSERT(bindData != nullptr);
	const auto vertexBuffer		= bindData->getVertexBuffer();
	const auto dataSize			= static_cast<VkDeviceSize>(bindData->getDataSize());
	const auto bufferOffset		= vertexBuffer->getAllocation().getOffset();
	const auto stride			= static_cast<VkDeviceSize>(0);
	const auto pipeline			= bindData->getPipeline();

	vkd->cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
#ifndef CTS_USES_VULKANSC
	vkd->cmdBindVertexBuffers2(cmdBuffer, 0u, 1u, &vertexBuffer->get(), &bufferOffset, &dataSize, &stride);
#else
	vkd->cmdBindVertexBuffers2EXT(cmdBuffer, 0u, 1u, &vertexBuffer->get(), &bufferOffset, &dataSize, &stride);
#endif // CTS_USES_VULKANSC
}

void setDepthTestEnable (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetDepthTestEnable(cmdBuffer, VK_TRUE);
#else
	vkd->cmdSetDepthTestEnableEXT(cmdBuffer, VK_TRUE);
#endif // CTS_USES_VULKANSC
}

void setDepthWriteEnable (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetDepthWriteEnable(cmdBuffer, VK_TRUE);
#else
	vkd->cmdSetDepthWriteEnableEXT(cmdBuffer, VK_TRUE);
#endif // CTS_USES_VULKANSC
}

void setDepthCompareOp (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetDepthCompareOp(cmdBuffer, VK_COMPARE_OP_LESS);
#else
	vkd->cmdSetDepthCompareOpEXT(cmdBuffer, VK_COMPARE_OP_LESS);
#endif // CTS_USES_VULKANSC
}

void setDepthBoundsTestEnable (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetDepthBoundsTestEnable(cmdBuffer, VK_TRUE);
#else
	vkd->cmdSetDepthBoundsTestEnableEXT(cmdBuffer, VK_TRUE);
#endif // CTS_USES_VULKANSC
}

void setStencilTestEnable (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetStencilTestEnable(cmdBuffer, VK_TRUE);
#else
	vkd->cmdSetStencilTestEnableEXT(cmdBuffer, VK_TRUE);
#endif // CTS_USES_VULKANSC
}

void setStencilOp (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
#ifndef CTS_USES_VULKANSC
	vkd->cmdSetStencilOp(cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
#else
	vkd->cmdSetStencilOpEXT(cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
#endif // CTS_USES_VULKANSC
}

#ifndef CTS_USES_VULKANSC

void setViewportWScaling (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkViewportWScalingNV viewport =
	{
		1.0f,	//	float	xcoeff;
		1.0f,	//	float	ycoeff;
	};
	vkd->cmdSetViewportWScalingNV(cmdBuffer, 0u, 1u, &viewport);
}

void setViewportShadingRatePalette (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkShadingRatePaletteEntryNV	entry	= VK_SHADING_RATE_PALETTE_ENTRY_NO_INVOCATIONS_NV;
	const VkShadingRatePaletteNV		palette	=
	{
		1u,		//	deUint32							shadingRatePaletteEntryCount;
		&entry,	//	const VkShadingRatePaletteEntryNV*	pShadingRatePaletteEntries;
	};
	vkd->cmdSetViewportShadingRatePaletteNV(cmdBuffer, 0u, 1u, &palette);
}

void setCoarseSamplingOrder (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkCoarseSampleLocationNV locations[2] =
	{
		{
			0u,	//	deUint32	pixelX;
			0u,	//	deUint32	pixelY;
			0u,	//	deUint32	sample;
		},
		{
			0u,	//	deUint32	pixelX;
			1u,	//	deUint32	pixelY;
			0u,	//	deUint32	sample;
		},
	};
	const VkCoarseSampleOrderCustomNV order =
	{
		VK_SHADING_RATE_PALETTE_ENTRY_1_INVOCATION_PER_1X2_PIXELS_NV,	//	VkShadingRatePaletteEntryNV		shadingRate;
		1u,																//	deUint32						sampleCount;
		2u,																//	deUint32						sampleLocationCount;
		locations														//	const VkCoarseSampleLocationNV*	pSampleLocations;
	};
	vkd->cmdSetCoarseSampleOrderNV(cmdBuffer, VK_COARSE_SAMPLE_ORDER_TYPE_CUSTOM_NV, 1u, &order);
}

void setExclusiveScissor (const DeviceInterface* vkd, VkCommandBuffer cmdBuffer, const DynamicStateData*)
{
	const VkRect2D scissor =
	{
		{ 0, 0 },	//	VkOffset2D	offset;
		{ 1u, 1u },	//	VkExtent2D	extent;
	};
	vkd->cmdSetExclusiveScissorNV(cmdBuffer, 0u, 1u, &scissor);
}

#endif // CTS_USES_VULKANSC

const VkDynamicState dynamicStateList[] =
{
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR,
	VK_DYNAMIC_STATE_LINE_WIDTH,
	VK_DYNAMIC_STATE_DEPTH_BIAS,
	VK_DYNAMIC_STATE_BLEND_CONSTANTS,
	VK_DYNAMIC_STATE_DEPTH_BOUNDS,
	VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
	VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
	VK_DYNAMIC_STATE_STENCIL_REFERENCE,
	VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT,
	VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,
#ifndef CTS_USES_VULKANSC
	VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR,
#endif // CTS_USES_VULKANSC
	VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
	VK_DYNAMIC_STATE_LINE_STIPPLE_EXT,
	VK_DYNAMIC_STATE_CULL_MODE_EXT,
	VK_DYNAMIC_STATE_FRONT_FACE_EXT,
	VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
	VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT,
	VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT,
	VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,
	VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
	VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
	VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
	VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
	VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
	VK_DYNAMIC_STATE_STENCIL_OP_EXT,
#ifndef CTS_USES_VULKANSC
	VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV,
	VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV,
	VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV,
	VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV,
#endif // CTS_USES_VULKANSC
};

// Information about a dynamic state.
struct StateInfo
{
	std::vector<std::string>	requirements;	// List of required functionalities.
	RecordStateFunction			recorder;		// Function that records the state to the command buffer being used.
};

// Returns the state info for a given dynamic state.
const StateInfo& getDynamicStateInfo (VkDynamicState state)
{
	// Maps a given state to its state info structure.
	using StateInfoMap = std::map<VkDynamicState, StateInfo>;

	static const StateInfoMap result =
	{
		{	VK_DYNAMIC_STATE_VIEWPORT,								{	{},										setViewport						}	},
		{	VK_DYNAMIC_STATE_SCISSOR,								{	{},										setScissor						}	},
		{	VK_DYNAMIC_STATE_LINE_WIDTH,							{	{},										setLineWidth					}	},
		{	VK_DYNAMIC_STATE_DEPTH_BIAS,							{	{},										setDepthBias					}	},
		{	VK_DYNAMIC_STATE_BLEND_CONSTANTS,						{	{},										setBlendConstants				}	},
		{	VK_DYNAMIC_STATE_DEPTH_BOUNDS,							{	{},										setDepthBounds					}	},
		{	VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,					{	{},										setStencilCompareMask			}	},
		{	VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,					{	{},										setStencilWriteMask				}	},
		{	VK_DYNAMIC_STATE_STENCIL_REFERENCE,						{	{},										setStencilReference				}	},
		{	VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT,					{	{ "VK_EXT_discard_rectangles" },		setDiscardRectangle				}	},
		{	VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,					{	{ "VK_EXT_sample_locations" },			setSampleLocations				}	},
#ifndef CTS_USES_VULKANSC
		{	VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR,	{	{ "VK_KHR_ray_tracing_pipeline" },		setRTPipelineStatckSize			}	},
#endif // CTS_USES_VULKANSC
		{	VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,				{	{ "VK_KHR_fragment_shading_rate" },		setFragmentShadingRage			}	},
		{	VK_DYNAMIC_STATE_LINE_STIPPLE_EXT,						{	{ "VK_EXT_line_rasterization" },		setLineStipple					}	},
		{	VK_DYNAMIC_STATE_CULL_MODE_EXT,							{	{ "VK_EXT_extended_dynamic_state" },	setCullMode						}	},
		{	VK_DYNAMIC_STATE_FRONT_FACE_EXT,						{	{ "VK_EXT_extended_dynamic_state" },	setFrontFace					}	},
		{	VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,				{	{ "VK_EXT_extended_dynamic_state" },	setPrimitiveTopology			}	},
		{	VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT,				{	{ "VK_EXT_extended_dynamic_state" },	setViewportWithCount			}	},
		{	VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT,				{	{ "VK_EXT_extended_dynamic_state" },	setScissorWithCount				}	},
		{	VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,		{	{ "VK_EXT_extended_dynamic_state" },	bindVertexBuffers				}	},
		{	VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,					{	{ "VK_EXT_extended_dynamic_state" },	setDepthTestEnable				}	},
		{	VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,				{	{ "VK_EXT_extended_dynamic_state" },	setDepthWriteEnable				}	},
		{	VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,					{	{ "VK_EXT_extended_dynamic_state" },	setDepthCompareOp				}	},
		{	VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,			{	{ "VK_EXT_extended_dynamic_state" },	setDepthBoundsTestEnable		}	},
		{	VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,				{	{ "VK_EXT_extended_dynamic_state" },	setStencilTestEnable			}	},
		{	VK_DYNAMIC_STATE_STENCIL_OP_EXT,						{	{ "VK_EXT_extended_dynamic_state" },	setStencilOp					}	},
#ifndef CTS_USES_VULKANSC
		{	VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV,					{	{ "VK_NV_clip_space_w_scaling" },		setViewportWScaling				}	},
		{	VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV,		{	{ "VK_NV_shading_rate_image"},			setViewportShadingRatePalette	}	},
		{	VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV,		{	{ "VK_NV_shading_rate_image"},			setCoarseSamplingOrder			}	},
		{	VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV,					{	{ "VK_NV_scissor_exclusive"},			setExclusiveScissor				}	},
#endif // CTS_USES_VULKANSC
	};

	const auto itr = result.find(state);
	DE_ASSERT(itr != result.end());

	return itr->second;
}

// Device helper: this is needed in some tests when we create custom devices.
class DeviceHelper
{
public:
	virtual ~DeviceHelper () {}
	virtual const DeviceInterface&			getDeviceInterface	(void) const = 0;
	virtual VkDevice						getDevice			(void) const = 0;
	virtual uint32_t						getQueueFamilyIndex	(void) const = 0;
	virtual VkQueue							getQueue			(void) const = 0;
	virtual Allocator&						getAllocator		(void) const = 0;
	virtual const std::vector<std::string>&	getDeviceExtensions	(void) const = 0;
};

// This one just reuses the default device from the context.
class ContextDeviceHelper : public DeviceHelper
{
public:
	ContextDeviceHelper (Context& context)
		: m_deviceInterface		(context.getDeviceInterface())
		, m_device				(context.getDevice())
		, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
		, m_queue				(context.getUniversalQueue())
		, m_allocator			(context.getDefaultAllocator())
		, m_extensions			(context.getDeviceExtensions())
		{}

	virtual ~ContextDeviceHelper () {}

	const DeviceInterface&			getDeviceInterface	(void) const override	{ return m_deviceInterface;		}
	VkDevice						getDevice			(void) const override	{ return m_device;				}
	uint32_t						getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	VkQueue							getQueue			(void) const override	{ return m_queue;				}
	Allocator&						getAllocator		(void) const override	{ return m_allocator;			}
	const std::vector<std::string>&	getDeviceExtensions	(void) const override	{ return m_extensions;			}

protected:
	const DeviceInterface&		m_deviceInterface;
	const VkDevice				m_device;
	const uint32_t				m_queueFamilyIndex;
	const VkQueue				m_queue;
	Allocator&					m_allocator;
	std::vector<std::string>	m_extensions;
};

// This one creates a new device with VK_NV_shading_rate_image.
class ShadingRateImageDeviceHelper : public DeviceHelper
{
public:
	ShadingRateImageDeviceHelper (Context& context)
	{
		const auto&	vkp				= context.getPlatformInterface();
		const auto&	vki				= context.getInstanceInterface();
		const auto	instance		= context.getInstance();
		const auto	physicalDevice	= context.getPhysicalDevice();
		const auto	queuePriority	= 1.0f;

		// Queue index first.
		m_queueFamilyIndex = context.getUniversalQueueFamilyIndex();

		// Create a universal queue that supports graphics and compute.
		const VkDeviceQueueCreateInfo queueParams =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkDeviceQueueCreateFlags		flags;
			m_queueFamilyIndex,							// deUint32						queueFamilyIndex;
			1u,											// deUint32						queueCount;
			&queuePriority								// const float*					pQueuePriorities;
		};

		const char* extensions[] =
		{
			"VK_NV_shading_rate_image",
		};
		m_extensions.push_back("VK_NV_shading_rate_image");

#ifndef CTS_USES_VULKANSC
		VkPhysicalDeviceShadingRateImageFeaturesNV	shadingRateImageFeatures	= initVulkanStructure();
		VkPhysicalDeviceFeatures2					features2					= initVulkanStructure(&shadingRateImageFeatures);

		vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
#endif // CTS_USES_VULKANSC

		const VkDeviceCreateInfo deviceCreateInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,					//sType;
#ifndef CTS_USES_VULKANSC
			&features2,												//pNext;
#else
			DE_NULL,
#endif // CTS_USES_VULKANSC
			0u,														//flags
			1u,														//queueRecordCount;
			&queueParams,											//pRequestedQueues;
			0u,														//layerCount;
			nullptr,												//ppEnabledLayerNames;
			static_cast<uint32_t>(de::arrayLength(extensions)),		// deUint32							enabledExtensionCount;
			extensions,												// const char* const*				ppEnabledExtensionNames;
			nullptr,												//pEnabledFeatures;
		};

		m_device	= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &deviceCreateInfo);
		m_vkd		.reset(new DeviceDriver(vkp, instance, m_device.get(), context.getUsedApiVersion()));
		m_queue		= getDeviceQueue(*m_vkd, *m_device, m_queueFamilyIndex, 0u);
		m_allocator	.reset(new SimpleAllocator(*m_vkd, m_device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}

	virtual ~ShadingRateImageDeviceHelper () {}

	const DeviceInterface&			getDeviceInterface	(void) const override	{ return *m_vkd;				}
	VkDevice						getDevice			(void) const override	{ return m_device.get();		}
	uint32_t						getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	VkQueue							getQueue			(void) const override	{ return m_queue;				}
	Allocator&						getAllocator		(void) const override	{ return *m_allocator;			}
	const std::vector<std::string>&	getDeviceExtensions	(void) const override	{ return m_extensions;			}

protected:
	Move<VkDevice>						m_device;
	std::unique_ptr<DeviceDriver>		m_vkd;
	deUint32							m_queueFamilyIndex;
	VkQueue								m_queue;
	std::unique_ptr<SimpleAllocator>	m_allocator;
	std::vector<std::string>			m_extensions;
};

std::unique_ptr<DeviceHelper> g_shadingRateDeviceHelper;
std::unique_ptr<DeviceHelper> g_contextDeviceHelper;

DeviceHelper& getDeviceHelper(Context& context, VkDynamicState dynamicState)
{
	const auto& stateInfo = getDynamicStateInfo(dynamicState);

	if (de::contains(stateInfo.requirements.begin(), stateInfo.requirements.end(), "VK_NV_shading_rate_image"))
	{
		if (!g_shadingRateDeviceHelper)
			g_shadingRateDeviceHelper.reset(new ShadingRateImageDeviceHelper(context));
		return *g_shadingRateDeviceHelper;
	}

	if (!g_contextDeviceHelper)
		g_contextDeviceHelper.reset(new ContextDeviceHelper(context));
	return *g_contextDeviceHelper;
}

// Returns the set of auxiliary data needed to set a given state.
de::MovePtr<DynamicStateData> getDynamicStateData (Context& ctx, VkDevice device, VkDynamicState state, PipelineConstructionType pipelineConstructionType)
{
	// Create vertex buffer for VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT.
	if (state == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT)
		return de::MovePtr<DynamicStateData>(new BindVertexBuffersData(ctx, device, pipelineConstructionType));

	// null pointer normally.
	return de::MovePtr<DynamicStateData>();
}

enum class OperType		{ COMPUTE = 0,	TRANSFER	};
enum class WhenToSet	{ BEFORE = 0,	AFTER		};

// Set dynamic state before or after attempting to run a compute or transfer operation.
struct TestParams
{
	OperType					operationType;
	WhenToSet					whenToSet;
	std::vector<VkDynamicState>	states;
};

class DynamicStateComputeCase : public vkt::TestCase
{
public:

							DynamicStateComputeCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params, PipelineConstructionType pipelineConstructionType);
	virtual					~DynamicStateComputeCase	(void) {}

	virtual void			checkSupport				(Context& context) const;
	virtual void			initPrograms				(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;

protected:
	TestParams					m_params;
	PipelineConstructionType	m_pipelineConstructionType;
};

class DynamicStateComputeInstance : public vkt::TestInstance
{
public:
								DynamicStateComputeInstance		(Context& context, const TestParams& params, PipelineConstructionType pipelineConstructionType);
	virtual						~DynamicStateComputeInstance	(void) {}

	virtual tcu::TestStatus		iterate							(void);

protected:
	tcu::TestStatus				iterateTransfer					(void);
	tcu::TestStatus				iterateCompute					(void);

	TestParams					m_params;
	PipelineConstructionType	m_pipelineConstructionType;
};

DynamicStateComputeCase::DynamicStateComputeCase(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params, PipelineConstructionType pipelineConstructionType)
	: vkt::TestCase					(testCtx, name, description)
	, m_params						(params)
	, m_pipelineConstructionType	(pipelineConstructionType)
{}

DynamicStateComputeInstance::DynamicStateComputeInstance (Context& context, const TestParams& params, PipelineConstructionType pipelineConstructionType)
	: vkt::TestInstance				(context)
	, m_params						(params)
	, m_pipelineConstructionType	(pipelineConstructionType)
{}

void DynamicStateComputeCase::checkSupport (Context& context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);

	// Check required functionalities.
	for (const auto& state : m_params.states)
	{
		const auto stateInfo = getDynamicStateInfo(state);
		for (const auto& functionality : stateInfo.requirements)
			context.requireDeviceFunctionality(functionality);
	}
}

void DynamicStateComputeCase::initPrograms (vk::SourceCollections& programCollection) const
{
	if (m_params.operationType == OperType::COMPUTE)
	{
		std::ostringstream comp;
		comp
			<< "#version 450\n"
			<< "\n"
			<< "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			<< "\n"
			<< "layout (push_constant, std430) uniform PushConstants {\n"
			<< "	uint valueIndex;\n"
			<< "} pc;\n"
			<< "\n"
			<< "layout (set=0, binding=0, std430) buffer OutputBlock {\n"
			<< "	uint value[];\n"
			<< "} ob;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "	ob.value[pc.valueIndex] = 1u;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
	}

	if (de::contains(begin(m_params.states), end(m_params.states), VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT))
	{
		// Passthrough vertex shader for stand-in graphics pipeline.
		std::ostringstream vert;
		vert
			<< "#version 450\n"
			<< "layout (location=0) in vec4 inVertex;\n"
			<< "void main() {\n"
			<< "    gl_Position = inVertex;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	}
}

vkt::TestInstance* DynamicStateComputeCase::createInstance (Context& context) const
{
	return new DynamicStateComputeInstance(context, m_params, m_pipelineConstructionType);
}

tcu::TestStatus DynamicStateComputeInstance::iterate (void)
{
	if (m_params.operationType == OperType::COMPUTE)
		return iterateCompute();
	else
		return iterateTransfer();
}

void fillBuffer(const DeviceInterface& vkd, VkDevice device, BufferWithMemory& buffer, const std::vector<deUint32> &values)
{
	auto& alloc = buffer.getAllocation();

	deMemcpy(alloc.getHostPtr(), values.data(), de::dataSize(values));
	flushAlloc(vkd, device, alloc);
}

tcu::TestStatus DynamicStateComputeInstance::iterateTransfer (void)
{
	const auto&	vki			= m_context.getInstanceInterface();
	const auto	phyDev		= m_context.getPhysicalDevice();
	auto&		devHelper	= getDeviceHelper(m_context, m_params.states.at(0));
	const auto&	vkd			= devHelper.getDeviceInterface();
	const auto	device		= devHelper.getDevice();
	const auto	qIndex		= devHelper.getQueueFamilyIndex();
	const auto	queue		= devHelper.getQueue();
	auto&		alloc		= devHelper.getAllocator();

	const auto	cmdPool			= makeCommandPool(vkd, device, qIndex);
	const auto	cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto	cmdBuffer		= cmdBufferPtr.get();

	// Prepare two host-visible buffers for a transfer operation, with one element per dynamic state.
	const deUint32		seqStart	= 1611747605u;

	DE_ASSERT(!m_params.states.empty());
	std::vector<deUint32>		srcValues(m_params.states.size());
	const decltype(srcValues)	dstValues(srcValues.size(), 0u);
	std::iota(begin(srcValues), end(srcValues), seqStart);

	const auto			elemSize	= static_cast<VkDeviceSize>(sizeof(decltype(srcValues)::value_type));
	const auto			dataSize	= static_cast<VkDeviceSize>(de::dataSize(srcValues));
	const auto			bufferSize	= de::roundUp(dataSize, getPhysicalDeviceProperties(vki, phyDev).limits.nonCoherentAtomSize);
	const auto			srcInfo		= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	const auto			dstInfo		= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory	srcBuffer	(vkd, device, alloc, srcInfo, MemoryRequirement::HostVisible);
	BufferWithMemory	dstBuffer	(vkd, device, alloc, dstInfo, MemoryRequirement::HostVisible);

	// Fill source and destination buffer.
	fillBuffer(vkd, device, srcBuffer, srcValues);
	fillBuffer(vkd, device, dstBuffer, dstValues);

	beginCommandBuffer(vkd, cmdBuffer);

	// We need to preserve dynamic state data until the command buffer has run.
	std::vector<de::MovePtr<DynamicStateData>> statesData;

	for (size_t stateIdx = 0; stateIdx < m_params.states.size(); ++stateIdx)
	{
		// Get extra data needed for using the dynamic state.
		const auto	offset		= elemSize * stateIdx;
		const auto&	state		= m_params.states[stateIdx];
		const auto	stateInfo	= getDynamicStateInfo(state);
		statesData.push_back(getDynamicStateData(m_context, device, state, m_pipelineConstructionType));

		// Record command if before.
		if (m_params.whenToSet == WhenToSet::BEFORE)
			stateInfo.recorder(&vkd, cmdBuffer, statesData.back().get());

		// Transfer op (copy one buffer element per dynamic state).
		const VkBufferCopy region = { offset, offset, elemSize };
		vkd.cmdCopyBuffer(cmdBuffer, srcBuffer.get(), dstBuffer.get(), 1u, &region);

		// Record command if after.
		if (m_params.whenToSet == WhenToSet::AFTER)
			stateInfo.recorder(&vkd, cmdBuffer, statesData.back().get());
	}

	const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &barrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Invalidate alloc and check destination buffer.
	auto& dstBufferAlloc = dstBuffer.getAllocation();
	invalidateAlloc(vkd, device, dstBufferAlloc);

	decltype(srcValues) results (srcValues.size());
	deMemcpy(results.data(), dstBufferAlloc.getHostPtr(), de::dataSize(srcValues));

	for (size_t valueIdx = 0; valueIdx < srcValues.size(); ++valueIdx)
	{
		const auto& orig	= srcValues[valueIdx];
		const auto& res		= results[valueIdx];

		if (orig != res)
		{
			std::ostringstream msg;
			msg << "Unexpected value found in destination buffer at position " << valueIdx << " (found=" << res << " expected=" << orig << ")";
			TCU_FAIL(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus DynamicStateComputeInstance::iterateCompute (void)
{
	const auto&	vki			= m_context.getInstanceInterface();
	const auto	phyDev		= m_context.getPhysicalDevice();
	auto&		devHelper	= getDeviceHelper(m_context, m_params.states.at(0));
	const auto&	vkd			= devHelper.getDeviceInterface();
	const auto	device		= devHelper.getDevice();
	const auto	qIndex		= devHelper.getQueueFamilyIndex();
	const auto	queue		= devHelper.getQueue();
	auto&		alloc		= devHelper.getAllocator();

	const auto	cmdPool			= makeCommandPool(vkd, device, qIndex);
	const auto	cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto	cmdBuffer		= cmdBufferPtr.get();

	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	const auto	setLayout		= setLayoutBuilder.build(vkd, device);

	// Push constants.
	const deUint32	pcSize		= static_cast<deUint32>(sizeof(deUint32));
	const auto		pcRange		= makePushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0u, pcSize);

	// Pipeline.
	const VkPipelineLayoutCreateInfo layoutInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkPipelineLayoutCreateFlags		flags;
		1u,												//	deUint32						setLayoutCount;
		&setLayout.get(),								//	const VkDescriptorSetLayout*	pSetLayouts;
		1u,												//	deUint32						pushConstantRangeCount;
		&pcRange,										//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const auto pipelineLayout = createPipelineLayout(vkd, device, &layoutInfo);

	const auto shaderModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);

	const VkPipelineShaderStageCreateInfo shaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							//	VkShaderStageFlagBits				stage;
		shaderModule.get(),										//	VkShaderModule						module;
		"main",													//	const char*							pName;
		nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo pipelineInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkPipelineCreateFlags			flags;
		shaderStageInfo,								//	VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout.get(),							//	VkPipelineLayout				layout;
		DE_NULL,										//	VkPipeline						basePipelineHandle;
		0,												//	deInt32							basePipelineIndex;
	};
	const auto pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineInfo);

	DE_ASSERT(!m_params.states.empty());

	// Output buffer with one value per state.
	std::vector<deUint32>	bufferData			(m_params.states.size(), 0u);
	const auto				dataSize			(de::dataSize(bufferData));
	const auto				outputBufferSize	= de::roundUp(static_cast<VkDeviceSize>(dataSize), getPhysicalDeviceProperties(vki, phyDev).limits.nonCoherentAtomSize);
	const auto				bufferCreateInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	BufferWithMemory		outputBuffer		(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible);
	auto&					outputBufferAlloc	= outputBuffer.getAllocation();
	auto					outputBufferPtr		= outputBufferAlloc.getHostPtr();

	deMemcpy(outputBufferPtr, bufferData.data(), dataSize);
	flushAlloc(vkd, device, outputBufferAlloc);

	// Descriptor set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	const auto bufferInfo = makeDescriptorBufferInfo(outputBuffer.get(), 0ull, outputBufferSize);
	DescriptorSetUpdateBuilder updateBuilder;
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
	updateBuilder.update(vkd, device);

	// Record and submit.
	beginCommandBuffer(vkd, cmdBuffer);

	// We need to preserve dynamic state data until the command buffer has run.
	std::vector<de::MovePtr<DynamicStateData>> statesData;

	for (size_t stateIdx = 0; stateIdx < m_params.states.size(); ++stateIdx)
	{
		// Objects needed to set the dynamic state.
		auto		state		= m_params.states[stateIdx];
		if (vk::isConstructionTypeShaderObject(m_pipelineConstructionType))
		{
			if (state == vk::VK_DYNAMIC_STATE_VIEWPORT)
				state = vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT;
			if (state == vk::VK_DYNAMIC_STATE_SCISSOR)
				state = vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT;
		}

		const auto	stateInfo	= getDynamicStateInfo(state);
		statesData.push_back(getDynamicStateData(m_context, device, state, m_pipelineConstructionType));

		if (m_params.whenToSet == WhenToSet::BEFORE)
			stateInfo.recorder(&vkd, cmdBuffer, statesData.back().get());

		vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
		vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		{
			// Each state will write to a different buffer position.
			const deUint32 pcData = static_cast<deUint32>(stateIdx);
			vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0u, pcSize, &pcData);
		}
		vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

		if (m_params.whenToSet == WhenToSet::AFTER)
			stateInfo.recorder(&vkd, cmdBuffer, statesData.back().get());
	}

	// Barrier to read buffer contents.
	const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &barrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read and verify buffer contents.
	invalidateAlloc(vkd, device, outputBufferAlloc);
	deMemcpy(bufferData.data(), outputBufferPtr, dataSize);

	for (size_t idx = 0u; idx < bufferData.size(); ++idx)
	{
		if (bufferData[idx] != 1u)
		{
			std::ostringstream msg;
			msg << "Unexpected value found at buffer position " << idx << ": " << bufferData[idx];
			TCU_FAIL(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

std::string getDynamicStateBriefName (VkDynamicState state)
{
	const auto fullName		= de::toString(state);
	const auto prefixLen	= strlen("VK_DYNAMIC_STATE_");

	return de::toLower(fullName.substr(prefixLen));
}

} // anonymous

tcu::TestCaseGroup* createDynamicStateComputeTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "compute_transfer", "Dynamic state mixed with compute and transfer operations"));

	const struct
	{
		OperType	operationType;
		const char*	name;
	} operations[] =
	{
		{	OperType::COMPUTE,	"compute"	},
		{	OperType::TRANSFER,	"transfer"	},
	};

	const struct
	{
		WhenToSet	when;
		const char*	name;
	} moments[] =
	{
		{	WhenToSet::BEFORE,	"before"	},
		{	WhenToSet::AFTER,	"after"		},
	};

	// Tests with a single dynamic state.
	{
		GroupPtr singleStateGroup(new tcu::TestCaseGroup(testCtx, "single", "Tests using a single dynamic state"));

		for (int operIdx = 0; operIdx < DE_LENGTH_OF_ARRAY(operations); ++operIdx)
		{
			GroupPtr operationGroup(new tcu::TestCaseGroup(testCtx, operations[operIdx].name, ""));

			for (int stateIdx = 0; stateIdx < DE_LENGTH_OF_ARRAY(dynamicStateList); ++stateIdx)
			{
				const auto	state		= dynamicStateList[stateIdx];
				const auto	stateName	= getDynamicStateBriefName(state);

				GroupPtr stateGroup(new tcu::TestCaseGroup(testCtx, stateName.c_str(), ""));

				for (int momentIdx = 0; momentIdx < DE_LENGTH_OF_ARRAY(moments); ++momentIdx)
				{
					const TestParams testParams =
					{
						operations[operIdx].operationType,		//	OperType					operationType;
						moments[momentIdx].when,				//	WhenToSet					whenToSet;
						std::vector<VkDynamicState>(1, state),	//	std::vector<VkDynamicState>	state;
					};

					stateGroup->addChild(new DynamicStateComputeCase(testCtx, moments[momentIdx].name, "", testParams, pipelineConstructionType));
				}

				operationGroup->addChild(stateGroup.release());
			}

			singleStateGroup->addChild(operationGroup.release());
		}

		mainGroup->addChild(singleStateGroup.release());
	}

	// A few tests with several dynamic states.
	{
		GroupPtr multiStateGroup(new tcu::TestCaseGroup(testCtx, "multi", "Tests using multiple dynamic states"));

		for (int operIdx = 0; operIdx < DE_LENGTH_OF_ARRAY(operations); ++operIdx)
		{
			GroupPtr operationGroup(new tcu::TestCaseGroup(testCtx, operations[operIdx].name, ""));

			for (int momentIdx = 0; momentIdx < DE_LENGTH_OF_ARRAY(moments); ++momentIdx)
			{
				TestParams testParams =
				{
					operations[operIdx].operationType,	//	OperType					operationType;
					moments[momentIdx].when,			//	WhenToSet					whenToSet;
					std::vector<VkDynamicState>(),		//	std::vector<VkDynamicState>	states;
				};

				// Use the basic states so as not to introduce extra requirements.
				for (int stateIdx = 0; stateIdx < DE_LENGTH_OF_ARRAY(dynamicStateList); ++stateIdx)
				{
					testParams.states.push_back(dynamicStateList[stateIdx]);
					if (dynamicStateList[stateIdx] == VK_DYNAMIC_STATE_STENCIL_REFERENCE)
						break;
				}

				operationGroup->addChild(new DynamicStateComputeCase(testCtx, moments[momentIdx].name, "", testParams, pipelineConstructionType));
			}

			multiStateGroup->addChild(operationGroup.release());
		}

		mainGroup->addChild(multiStateGroup.release());
	}

	return mainGroup.release();
}

void cleanupDevice()
{
	g_shadingRateDeviceHelper.reset(nullptr);
	g_contextDeviceHelper.reset(nullptr);
}

} // DynamicState
} // vkt
