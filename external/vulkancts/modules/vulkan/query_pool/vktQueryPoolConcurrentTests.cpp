/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation
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
 * \brief Vulkan Concurrent Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolConcurrentTests.hpp"

#include "vktTestCase.hpp"

#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

#include <memory>

namespace vkt
{

namespace QueryPool
{

using namespace Draw;

namespace
{

enum QueryType
{
	QUERY_TYPE_OCCLUSION = vk::VK_QUERY_TYPE_OCCLUSION,
	QUERY_TYPE_PIPELINE_STATISTICS = vk::VK_QUERY_TYPE_PIPELINE_STATISTICS,
	QUERY_TYPE_TIMESTAMP = vk::VK_QUERY_TYPE_TIMESTAMP,
	NUM_QUERY_POOLS = 3
};

struct StateObjects
{
			StateObjects	(const vk::DeviceInterface&vk, vkt::Context &context, const int numVertices, vk::VkPrimitiveTopology primitive);
	void	setVertices		(const vk::DeviceInterface&vk, std::vector<tcu::Vec4> vertices);

	enum
	{
		WIDTH	= 128,
		HEIGHT	= 128
	};

	vkt::Context &m_context;

	vk::Move<vk::VkPipeline>		m_pipeline;
	vk::Move<vk::VkPipelineLayout>	m_pipelineLayout;

	de::SharedPtr<Image>			m_colorAttachmentImage, m_DepthImage;
	vk::Move<vk::VkImageView>		m_attachmentView;
	vk::Move<vk::VkImageView>		m_depthiew;

	vk::Move<vk::VkRenderPass>		m_renderPass;
	vk::Move<vk::VkFramebuffer>		m_framebuffer;

	de::SharedPtr<Buffer>			m_vertexBuffer;

	vk::VkFormat					m_colorAttachmentFormat;
};

StateObjects::StateObjects (const vk::DeviceInterface&vk, vkt::Context &context, const int numVertices, vk::VkPrimitiveTopology primitive)
	: m_context(context)
	, m_colorAttachmentFormat(vk::VK_FORMAT_R8G8B8A8_UNORM)

{
	vk::VkFormat		depthFormat = vk::VK_FORMAT_D16_UNORM;
	const vk::VkDevice	device		= m_context.getDevice();

	//attachment images and views
	{
		vk::VkExtent3D imageExtent =
		{
			WIDTH,	// width;
			HEIGHT,	// height;
			1		// depth;
		};

		const ImageCreateInfo colorImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
												   vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		m_colorAttachmentImage	= Image::createAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		const ImageViewCreateInfo attachmentViewInfo(m_colorAttachmentImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
		m_attachmentView		= vk::createImageView(vk, device, &attachmentViewInfo);

		ImageCreateInfo depthImageCreateInfo(vk::VK_IMAGE_TYPE_2D, depthFormat, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
			vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

		m_DepthImage			= Image::createAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		// Construct a depth  view from depth image
		const ImageViewCreateInfo depthViewInfo(m_DepthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, depthFormat);
		m_depthiew				= vk::createImageView(vk, device, &depthViewInfo);
	}

	{
		// Renderpass and Framebuffer

		RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,									// format
																	vk::VK_SAMPLE_COUNT_1_BIT,								// samples
																	vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						// loadOp
																	vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,					// storeOp
																	vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// stencilLoadOp
																	vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,					// stencilLoadOp
																	vk::VK_IMAGE_LAYOUT_GENERAL,							// initialLauout
																	vk::VK_IMAGE_LAYOUT_GENERAL));							// finalLayout

		renderPassCreateInfo.addAttachment(AttachmentDescription(depthFormat,												// format
																 vk::VK_SAMPLE_COUNT_1_BIT,									// samples
																 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,							// loadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// storeOp
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// initialLauout
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));	// finalLayout

		const vk::VkAttachmentReference colorAttachmentReference =
		{
			0,															// attachment
			vk::VK_IMAGE_LAYOUT_GENERAL									// layout
		};

		const vk::VkAttachmentReference depthAttachmentReference =
		{
			1,															// attachment
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL		// layout
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS,					// pipelineBindPoint
														   0,													// flags
														   0,													// inputCount
														   DE_NULL,												// pInputAttachments
														   1,													// colorCount
														   &colorAttachmentReference,							// pColorAttachments
														   DE_NULL,												// pResolveAttachments
														   depthAttachmentReference,							// depthStencilAttachment
														   0,													// preserveCount
														   DE_NULL));											// preserveAttachments

		m_renderPass = vk::createRenderPass(vk, device, &renderPassCreateInfo);

		std::vector<vk::VkImageView> attachments(2);
		attachments[0] = *m_attachmentView;
		attachments[1] = *m_depthiew;

		FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
		m_framebuffer = vk::createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	{
		// Pipeline

		vk::Unique<vk::VkShaderModule> vs(vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
		vk::Unique<vk::VkShaderModule> fs(vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

		const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

		const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		m_pipelineLayout = vk::createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

		const vk::VkVertexInputBindingDescription vf_binding_desc		=
		{
			0,																// binding;
			4 * (deUint32)sizeof(float),									// stride;
			vk::VK_VERTEX_INPUT_RATE_VERTEX									// inputRate
		};

		const vk::VkVertexInputAttributeDescription vf_attribute_desc	=
		{
			0,																// location;
			0,																// binding;
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,								// format;
			0																// offset;
		};

		const vk::VkPipelineVertexInputStateCreateInfo vf_info			=
		{																	// sType;
			vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// pNext;
			NULL,															// flags;
			0u,																// vertexBindingDescriptionCount;
			1,																// pVertexBindingDescriptions;
			&vf_binding_desc,												// vertexAttributeDescriptionCount;
			1,																// pVertexAttributeDescriptions;
			&vf_attribute_desc
		};

		PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(primitive));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));
		const vk::VkViewport viewport	= vk::makeViewport(WIDTH, HEIGHT);
		const vk::VkRect2D scissor		= vk::makeRect2D(WIDTH, HEIGHT);
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport), std::vector<vk::VkRect2D>(1, scissor)));
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState(true, true, vk::VK_COMPARE_OP_GREATER_OR_EQUAL));
		pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
		pipelineCreateInfo.addState(vf_info);
		m_pipeline = vk::createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
	}

	{
		// Vertex buffer
		const size_t kBufferSize = numVertices * sizeof(tcu::Vec4);
		m_vertexBuffer = Buffer::createAndAlloc(vk, device, BufferCreateInfo(kBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);
	}
}

void StateObjects::setVertices (const vk::DeviceInterface&vk, std::vector<tcu::Vec4> vertices)
{
	const vk::VkDevice device			= m_context.getDevice();

	tcu::Vec4 *ptr = reinterpret_cast<tcu::Vec4*>(m_vertexBuffer->getBoundMemory().getHostPtr());
	std::copy(vertices.begin(), vertices.end(), ptr);

	vk::flushAlloc(vk, device,	m_vertexBuffer->getBoundMemory());
}

class PrimaryCommandBufferConcurrentTestInstance : public vkt::TestInstance
{
public:
	PrimaryCommandBufferConcurrentTestInstance		(vkt::Context &context);
	~PrimaryCommandBufferConcurrentTestInstance		(void);
private:
	tcu::TestStatus					iterate							(void);

	enum
	{
		NUM_QUERIES_IN_POOL				= 2,
		QUERY_INDEX_CAPTURE_EMPTY		= 0,
		QUERY_INDEX_CAPTURE_DRAWCALL	= 1,
		NUM_VERTICES_IN_DRAWCALL		= 3
	};

	std::unique_ptr<StateObjects>		m_stateObjects;
	vk::Move<vk::VkQueryPool>			m_queryPools[NUM_QUERY_POOLS];
	deBool								m_supportedQueryType[NUM_QUERY_POOLS];
};

PrimaryCommandBufferConcurrentTestInstance::PrimaryCommandBufferConcurrentTestInstance (vkt::Context &context)
	: TestInstance		(context)
{
	// Check support for multiple query types
	{
		for(deUint32 poolNdx = 0; poolNdx < NUM_QUERY_POOLS; poolNdx++)
			m_supportedQueryType[poolNdx] = DE_FALSE;

		deUint32 numSupportedQueryTypes = 0;
		m_supportedQueryType[QUERY_TYPE_OCCLUSION] = DE_TRUE;
		numSupportedQueryTypes++;

		if (context.getDeviceFeatures().pipelineStatisticsQuery)
		{
			m_supportedQueryType[QUERY_TYPE_PIPELINE_STATISTICS] = DE_TRUE;
			numSupportedQueryTypes++;
		}

		// Check support for timestamp queries
		{
			const deUint32									queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
			const std::vector<vk::VkQueueFamilyProperties>	queueProperties		= vk::getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

			DE_ASSERT(queueFamilyIndex < (deUint32)queueProperties.size());

			if (queueProperties[queueFamilyIndex].timestampValidBits)
			 {
				 m_supportedQueryType[QUERY_TYPE_TIMESTAMP] = DE_TRUE;
				 numSupportedQueryTypes++;
			 }
		}
		if (numSupportedQueryTypes < 2)
			throw tcu::NotSupportedError("Device does not support multiple query types");
	}

	m_stateObjects = std::unique_ptr<StateObjects>(new StateObjects(m_context.getDeviceInterface(), m_context, NUM_VERTICES_IN_DRAWCALL, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));

	const vk::VkDevice			device	= m_context.getDevice();
	const vk::DeviceInterface&	vk		= m_context.getDeviceInterface();

	for(deUint32 poolNdx = 0; poolNdx < NUM_QUERY_POOLS; poolNdx++)
	{
		if (!m_supportedQueryType[poolNdx])
			continue;

		vk::VkQueryPoolCreateInfo queryPoolCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			DE_NULL,
			0u,
			static_cast<vk::VkQueryType>(poolNdx),
			NUM_QUERIES_IN_POOL,
			0u,
		};
		if (poolNdx == QUERY_TYPE_PIPELINE_STATISTICS)
			queryPoolCreateInfo.pipelineStatistics = vk::VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;

		m_queryPools[poolNdx] = createQueryPool(vk, device, &queryPoolCreateInfo, /*pAllocator*/ DE_NULL);
	}

	std::vector<tcu::Vec4> vertices(NUM_VERTICES_IN_DRAWCALL);
	vertices[0] = tcu::Vec4(0.5, 0.5, 0.0, 1.0);
	vertices[1] = tcu::Vec4(0.5, 0.0, 0.0, 1.0);
	vertices[2] = tcu::Vec4(0.0, 0.5, 0.0, 1.0);
	m_stateObjects->setVertices(vk, vertices);
}

PrimaryCommandBufferConcurrentTestInstance::~PrimaryCommandBufferConcurrentTestInstance (void)
{
}

tcu::TestStatus	PrimaryCommandBufferConcurrentTestInstance::iterate (void)
{
	tcu::TestLog &log				= m_context.getTestContext().getLog();
	const vk::VkDevice device		= m_context.getDevice();
	const vk::VkQueue queue			= m_context.getUniversalQueue();
	const vk::DeviceInterface& vk	= m_context.getDeviceInterface();

	const CmdPoolCreateInfo			cmdPoolCreateInfo	(m_context.getUniversalQueueFamilyIndex());
	vk::Move<vk::VkCommandPool>		cmdPool				= vk::createCommandPool(vk, device, &cmdPoolCreateInfo);

	vk::Unique<vk::VkCommandBuffer> cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);

	initialTransitionColor2DImage(vk, *cmdBuffer, m_stateObjects->m_colorAttachmentImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
								  vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	initialTransitionDepth2DImage(vk, *cmdBuffer, m_stateObjects->m_DepthImage->object(), vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								  vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

	std::vector<vk::VkClearValue> renderPassClearValues(2);
	deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(vk::VkClearValue));

	for (deUint32 poolNdx = 0u; poolNdx < NUM_QUERY_POOLS; poolNdx++)
	{
		if (m_supportedQueryType[poolNdx])
			vk.cmdResetQueryPool(*cmdBuffer, *m_queryPools[poolNdx], 0u, NUM_QUERIES_IN_POOL);
	}

	beginRenderPass(vk, *cmdBuffer, *m_stateObjects->m_renderPass, *m_stateObjects->m_framebuffer, vk::makeRect2D(0, 0, StateObjects::WIDTH, StateObjects::HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0]);

	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_stateObjects->m_pipeline);

	vk::VkBuffer vertexBuffer = m_stateObjects->m_vertexBuffer->object();
	const vk::VkDeviceSize vertexBufferOffset = 0;
	vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	// Begin all queries
	for (deUint32 poolNdx = 0u; poolNdx < QUERY_TYPE_TIMESTAMP; poolNdx++)
	{
		if (m_supportedQueryType[poolNdx])
			vk.cmdBeginQuery(*cmdBuffer, *m_queryPools[poolNdx], QUERY_INDEX_CAPTURE_EMPTY, 0u);
	}

	// End first capture (should not have any result). Start the second one.
	for (deUint32 poolNdx = 0u; poolNdx < QUERY_TYPE_TIMESTAMP; poolNdx++)
	{
		if (m_supportedQueryType[poolNdx])
		{
			vk.cmdEndQuery(*cmdBuffer, *m_queryPools[poolNdx],	QUERY_INDEX_CAPTURE_EMPTY);
			vk.cmdBeginQuery(*cmdBuffer, *m_queryPools[poolNdx], QUERY_INDEX_CAPTURE_DRAWCALL, 0u);
		}
	}

	vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_DRAWCALL, 1, 0, 0);

	if (m_supportedQueryType[QUERY_TYPE_TIMESTAMP])
		vk.cmdWriteTimestamp(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, *m_queryPools[QUERY_TYPE_TIMESTAMP], QUERY_INDEX_CAPTURE_DRAWCALL);

	for (deUint32 poolNdx = 0u; poolNdx < QUERY_TYPE_TIMESTAMP; poolNdx++)
	{
		if (m_supportedQueryType[poolNdx])
			vk.cmdEndQuery(*cmdBuffer, *m_queryPools[poolNdx],	QUERY_INDEX_CAPTURE_DRAWCALL);
	}

	endRenderPass(vk, *cmdBuffer);

	transition2DImage(vk, *cmdBuffer, m_stateObjects->m_colorAttachmentImage->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT,
					  vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					  vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	deUint64 queryResults[NUM_QUERIES_IN_POOL] = { 0 };
	size_t queryResultsSize		= sizeof(queryResults);
	bool passed = true;

	// Occlusion and pipeline statistics queries verification
	for (deUint32 poolNdx = 0; poolNdx < QUERY_TYPE_TIMESTAMP; poolNdx++)
	{
		if (m_supportedQueryType[poolNdx] == DE_FALSE)
			continue;
		vk::VkResult queryResult	= vk.getQueryPoolResults(device, *m_queryPools[poolNdx], 0, NUM_QUERIES_IN_POOL, queryResultsSize, queryResults, sizeof(queryResults[0]), vk::VK_QUERY_RESULT_64_BIT);

		if (queryResult == vk::VK_NOT_READY)
		{
			TCU_FAIL("Query result not available, but vkWaitIdle() was called.");
		}

		VK_CHECK(queryResult);
		std::string name = (poolNdx == QUERY_TYPE_OCCLUSION) ? "OcclusionQueryResults" : "PipelineStatisticsQueryResults";
		std::string desc = (poolNdx == QUERY_TYPE_OCCLUSION) ? "Occlusion query results" : "PipelineStatistics query results";
		log << tcu::TestLog::Section(name, desc);
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(queryResults); ++ndx)
		{
			log << tcu::TestLog::Message << "query[slot == " << ndx
				<< "] result == " << queryResults[ndx] << tcu::TestLog::EndMessage;
		}


		for (deUint32 queryNdx = 0; queryNdx < DE_LENGTH_OF_ARRAY(queryResults); ++queryNdx)
		{
			if (queryNdx == QUERY_INDEX_CAPTURE_EMPTY && queryResults[queryNdx] != 0u)
			{
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"wrong value of query for index "
					<< queryNdx << ", expected any zero value, got "
					<< queryResults[0] << "." << tcu::TestLog::EndMessage;
				passed = false;
			}

			if (queryNdx != QUERY_INDEX_CAPTURE_EMPTY && queryResults[queryNdx] == 0)
			{
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"wrong value of query for index "
					<< queryNdx << ", expected any non-zero value, got "
					<< queryResults[0] << "." << tcu::TestLog::EndMessage;
				passed = false;
			}
		}
		log << tcu::TestLog::EndSection;
	}

	// Timestamp query verification
	if (m_supportedQueryType[QUERY_TYPE_TIMESTAMP])
	{
		std::pair<deUint64, deUint64>	queryResultsWithAvailabilityBit[NUM_QUERIES_IN_POOL];
		size_t queryResultsWithAvailabilityBitSize		= sizeof(queryResultsWithAvailabilityBit);
		vk::VkResult queryResult	= vk.getQueryPoolResults(device, *m_queryPools[QUERY_TYPE_TIMESTAMP], 0, NUM_QUERIES_IN_POOL, queryResultsWithAvailabilityBitSize, &queryResultsWithAvailabilityBit[0], sizeof(queryResultsWithAvailabilityBit[0]), vk::VK_QUERY_RESULT_64_BIT | vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

		if (queryResult != vk::VK_NOT_READY)
		{
			TCU_FAIL("We don't have available one query, it should return VK_NOT_READY");
		}

		log << tcu::TestLog::Section("TimestampQueryResults",
									 "Timestamp query results");
		for (int ndx = 0; ndx < NUM_QUERIES_IN_POOL; ++ndx)
		{
			log << tcu::TestLog::Message << "query[slot == " << ndx
				<< "] result == " << queryResultsWithAvailabilityBit[ndx].first << tcu::TestLog::EndMessage;
		}


		for (deUint32 queryNdx = 0; queryNdx < NUM_QUERIES_IN_POOL; ++queryNdx)
		{
			if (queryNdx == QUERY_INDEX_CAPTURE_EMPTY && (queryResultsWithAvailabilityBit[queryNdx].first != 0u || queryResultsWithAvailabilityBit[queryNdx].second != 0u))
			{
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"either wrong value of query for index "
					<< queryNdx << " (expected any zero value, got "
					<< queryResultsWithAvailabilityBit[queryNdx].first << ") or the result is available (" << queryResultsWithAvailabilityBit[queryNdx].second << ")"
					<< tcu::TestLog::EndMessage;
				passed = false;
			}

			if (queryNdx != QUERY_INDEX_CAPTURE_EMPTY && (queryResultsWithAvailabilityBit[queryNdx].first == 0u || queryResultsWithAvailabilityBit[queryNdx].second == 0u))
			{
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"either wrong value of query for index "
					<< queryNdx << " (expected any non-zero value, got "
					<< queryResults[0] << ") or result is unavailable." << tcu::TestLog::EndMessage;
				passed = false;
			}
		}
		log << tcu::TestLog::EndSection;
	}

	if (passed)
	{
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Query result verification passed");
	}
	return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Query result verification failed");
}

class SecondaryCommandBufferConcurrentTestInstance : public vkt::TestInstance
{
public:
	SecondaryCommandBufferConcurrentTestInstance		(vkt::Context &context);
	~SecondaryCommandBufferConcurrentTestInstance		(void);
private:
	tcu::TestStatus					iterate							(void);

	enum
	{
		NUM_QUERIES_IN_POOL				= 2,
		QUERY_INDEX_CAPTURE_EMPTY		= 0,
		QUERY_INDEX_CAPTURE_DRAWCALL	= 1,
		NUM_VERTICES_IN_DRAWCALL		= 3
	};

	std::unique_ptr<StateObjects>			m_stateObjects;
	vk::Move<vk::VkQueryPool>				m_queryPools[NUM_QUERY_POOLS];
	deBool									m_supportedQueryType[NUM_QUERY_POOLS];

};

SecondaryCommandBufferConcurrentTestInstance::SecondaryCommandBufferConcurrentTestInstance (vkt::Context &context)
	: TestInstance		(context)
{
	// Check support for multiple query types
	{
		for(deUint32 poolNdx = 0; poolNdx < NUM_QUERY_POOLS; poolNdx++)
			m_supportedQueryType[poolNdx] = DE_FALSE;

		deUint32 numSupportedQueryTypes = 0;
		m_supportedQueryType[QUERY_TYPE_OCCLUSION] = DE_TRUE;
		numSupportedQueryTypes++;

		if (context.getDeviceFeatures().pipelineStatisticsQuery)
		{
			m_supportedQueryType[QUERY_TYPE_PIPELINE_STATISTICS] = DE_TRUE;
			numSupportedQueryTypes++;
		}

		// Check support for timestamp queries
		{
			const deUint32									queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
			const std::vector<vk::VkQueueFamilyProperties>	queueProperties		= vk::getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

			DE_ASSERT(queueFamilyIndex < (deUint32)queueProperties.size());

			if (queueProperties[queueFamilyIndex].timestampValidBits)
			 {
				 m_supportedQueryType[QUERY_TYPE_TIMESTAMP] = DE_TRUE;
				 numSupportedQueryTypes++;
			 }
		}
		if (numSupportedQueryTypes < 2)
			throw tcu::NotSupportedError("Device does not support multiple query types");
	}

	m_stateObjects = std::unique_ptr<StateObjects>(new StateObjects(m_context.getDeviceInterface(), m_context, NUM_VERTICES_IN_DRAWCALL, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));

	const vk::VkDevice			device	= m_context.getDevice();
	const vk::DeviceInterface&	vk		= m_context.getDeviceInterface();

	for(deUint32 poolNdx = 0; poolNdx < NUM_QUERY_POOLS; poolNdx++)
	{
		if (!m_supportedQueryType[poolNdx])
			continue;

		vk::VkQueryPoolCreateInfo queryPoolCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			DE_NULL,
			0u,
			static_cast<vk::VkQueryType>(poolNdx),
			NUM_QUERIES_IN_POOL,
			0u,
		};
		if (poolNdx == QUERY_TYPE_PIPELINE_STATISTICS)
			queryPoolCreateInfo.pipelineStatistics = vk::VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;

		m_queryPools[poolNdx] = createQueryPool(vk, device, &queryPoolCreateInfo, /*pAllocator*/ DE_NULL);
	}

	std::vector<tcu::Vec4> vertices(NUM_VERTICES_IN_DRAWCALL);
	vertices[0] = tcu::Vec4(0.5, 0.5, 0.0, 1.0);
	vertices[1] = tcu::Vec4(0.5, 0.0, 0.0, 1.0);
	vertices[2] = tcu::Vec4(0.0, 0.5, 0.0, 1.0);
	m_stateObjects->setVertices(vk, vertices);
}

SecondaryCommandBufferConcurrentTestInstance::~SecondaryCommandBufferConcurrentTestInstance (void)
{
}

void beginSecondaryCommandBuffer (const vk::DeviceInterface&				vk,
								  const vk::VkCommandBuffer					secondaryCmdBuffer,
								  const vk::VkCommandBufferInheritanceInfo	bufferInheritanceInfo)
{
	const vk::VkCommandBufferUsageFlags	flags		= bufferInheritanceInfo.renderPass != DE_NULL
													  ? (vk::VkCommandBufferUsageFlags)vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
													  : (vk::VkCommandBufferUsageFlags)0u;
	const vk::VkCommandBufferBeginInfo	beginInfo	=
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// sType
		DE_NULL,													// pNext
		flags,														// flags
		&bufferInheritanceInfo,										// pInheritanceInfo
	};
	VK_CHECK(vk.beginCommandBuffer(secondaryCmdBuffer, &beginInfo));
}

tcu::TestStatus	SecondaryCommandBufferConcurrentTestInstance::iterate (void)
{
	tcu::TestLog &log				= m_context.getTestContext().getLog();
	const vk::VkDevice device		= m_context.getDevice();
	const vk::VkQueue queue			= m_context.getUniversalQueue();
	const vk::DeviceInterface& vk	= m_context.getDeviceInterface();
	const deBool inheritedQueries	= m_context.getDeviceFeatures().inheritedQueries;

	const CmdPoolCreateInfo			cmdPoolCreateInfo	(m_context.getUniversalQueueFamilyIndex());
	vk::Move<vk::VkCommandPool>		cmdPool				= vk::createCommandPool(vk, device, &cmdPoolCreateInfo);

	vk::Unique<vk::VkCommandBuffer> cmdBufferPrimary			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	vk::Unique<vk::VkCommandBuffer> cmdBufferSecondary			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));

	// Secondary command buffer recording.
	{
		// Begin secondary command buffer
		const vk::VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			DE_NULL,
			*m_stateObjects->m_renderPass,								// renderPass
			0u,															// subpass
			*m_stateObjects->m_framebuffer,								// framebuffer
			inheritedQueries ? VK_TRUE : VK_FALSE,						// occlusionQueryEnable
			(vk::VkQueryControlFlags)0u,								// queryFlags
			(vk::VkQueryPipelineStatisticFlags)0u,						// pipelineStatistics
		};
		beginSecondaryCommandBuffer(vk, *cmdBufferSecondary, secCmdBufInheritInfo);

		vk.cmdBindPipeline(*cmdBufferSecondary, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_stateObjects->m_pipeline);
		vk::VkBuffer vertexBuffer = m_stateObjects->m_vertexBuffer->object();
		const vk::VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*cmdBufferSecondary, 0, 1, &vertexBuffer, &vertexBufferOffset);

		if (!inheritedQueries && m_supportedQueryType[QUERY_TYPE_OCCLUSION])
			vk.cmdBeginQuery(*cmdBufferSecondary, *m_queryPools[QUERY_TYPE_OCCLUSION], QUERY_INDEX_CAPTURE_DRAWCALL, 0u);

		// Run pipeline statistics queries capture in the second command buffer
		if (m_supportedQueryType[QUERY_TYPE_PIPELINE_STATISTICS])
			vk.cmdBeginQuery(*cmdBufferSecondary, *m_queryPools[QUERY_TYPE_PIPELINE_STATISTICS], QUERY_INDEX_CAPTURE_DRAWCALL, 0u);

		// Timestamp query happening in the secondary command buffer
		if (m_supportedQueryType[QUERY_TYPE_TIMESTAMP])
			vk.cmdWriteTimestamp(*cmdBufferSecondary, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, *m_queryPools[QUERY_TYPE_TIMESTAMP], QUERY_INDEX_CAPTURE_DRAWCALL);

		vk.cmdDraw(*cmdBufferSecondary, NUM_VERTICES_IN_DRAWCALL, 1, 0, 0);

		if (m_supportedQueryType[QUERY_TYPE_PIPELINE_STATISTICS])
			vk.cmdEndQuery(*cmdBufferSecondary, *m_queryPools[QUERY_TYPE_PIPELINE_STATISTICS], QUERY_INDEX_CAPTURE_DRAWCALL);

		if (!inheritedQueries && m_supportedQueryType[QUERY_TYPE_OCCLUSION])
			vk.cmdEndQuery(*cmdBufferSecondary, *m_queryPools[QUERY_TYPE_OCCLUSION], QUERY_INDEX_CAPTURE_DRAWCALL);

		endCommandBuffer(vk, *cmdBufferSecondary);
	}

	// Primary command buffer recording
	{
		beginCommandBuffer(vk, *cmdBufferPrimary);

		initialTransitionColor2DImage(vk, *cmdBufferPrimary, m_stateObjects->m_colorAttachmentImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
									  vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *cmdBufferPrimary, m_stateObjects->m_DepthImage->object(), vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		std::vector<vk::VkClearValue> renderPassClearValues(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(vk::VkClearValue));

		for (deUint32 poolNdx = 0u; poolNdx < NUM_QUERY_POOLS; poolNdx++)
		{
			if (m_supportedQueryType[poolNdx])
				vk.cmdResetQueryPool(*cmdBufferPrimary, *m_queryPools[poolNdx], 0u, NUM_QUERIES_IN_POOL);
		}

		for (deUint32 poolNdx = 0u; poolNdx < QUERY_TYPE_TIMESTAMP; poolNdx++)
		{
			if (m_supportedQueryType[poolNdx])
				vk.cmdBeginQuery(*cmdBufferPrimary, *m_queryPools[poolNdx], QUERY_INDEX_CAPTURE_EMPTY, 0u);
		}

		for (deUint32 poolNdx = 0u; poolNdx < QUERY_TYPE_TIMESTAMP; poolNdx++)
		{
			if (m_supportedQueryType[poolNdx])
				vk.cmdEndQuery(*cmdBufferPrimary, *m_queryPools[poolNdx], QUERY_INDEX_CAPTURE_EMPTY);
		}

		// Run oclussion queries capture in the primary command buffer, inherit the counters for the secondary command buffer
		if (inheritedQueries && m_supportedQueryType[QUERY_TYPE_OCCLUSION])
			vk.cmdBeginQuery(*cmdBufferPrimary, *m_queryPools[QUERY_TYPE_OCCLUSION], QUERY_INDEX_CAPTURE_DRAWCALL, 0u);

		beginRenderPass(vk, *cmdBufferPrimary, *m_stateObjects->m_renderPass, *m_stateObjects->m_framebuffer, vk::makeRect2D(0, 0, StateObjects::WIDTH, StateObjects::HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		vk.cmdExecuteCommands(*cmdBufferPrimary, 1u, &cmdBufferSecondary.get());

		endRenderPass(vk, *cmdBufferPrimary);

		if (inheritedQueries && m_supportedQueryType[QUERY_TYPE_OCCLUSION])
			vk.cmdEndQuery(*cmdBufferPrimary, *m_queryPools[QUERY_TYPE_OCCLUSION], QUERY_INDEX_CAPTURE_DRAWCALL);

		transition2DImage(vk, *cmdBufferPrimary,
						  m_stateObjects->m_colorAttachmentImage->object(),
						  vk::VK_IMAGE_ASPECT_COLOR_BIT,
						  vk::VK_IMAGE_LAYOUT_GENERAL,
						  vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						  vk::VK_ACCESS_TRANSFER_READ_BIT,
						  vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						  vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

		endCommandBuffer(vk, *cmdBufferPrimary);
	}

	submitCommandsAndWait(vk, device, queue, cmdBufferPrimary.get());

	deUint64 queryResults[NUM_QUERIES_IN_POOL] = { 0 };
	size_t queryResultsSize		= sizeof(queryResults);
	bool passed = true;

	// Occlusion and pipeline statistics queries verification
	for (deUint32 poolNdx = 0; poolNdx < QUERY_TYPE_TIMESTAMP; poolNdx++)
	{
		if (!m_supportedQueryType[poolNdx])
			continue;
		vk::VkResult queryResult	= vk.getQueryPoolResults(device, *m_queryPools[poolNdx], 0, NUM_QUERIES_IN_POOL, queryResultsSize, queryResults, sizeof(queryResults[0]), vk::VK_QUERY_RESULT_64_BIT);

		if (queryResult == vk::VK_NOT_READY)
		{
			TCU_FAIL("Query result not available, but vkWaitIdle() was called.");
		}

		VK_CHECK(queryResult);
		std::string name = (poolNdx == QUERY_TYPE_OCCLUSION) ? "OcclusionQueryResults" : "PipelineStatisticsQueryResults";
		std::string desc = (poolNdx == QUERY_TYPE_OCCLUSION) ? "Occlusion query results" : "PipelineStatistics query results";
		log << tcu::TestLog::Section(name, desc);
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(queryResults); ++ndx)
		{
			log << tcu::TestLog::Message << "query[slot == " << ndx
				<< "] result == " << queryResults[ndx] << tcu::TestLog::EndMessage;
		}

		for (deUint32 queryNdx = 0; queryNdx < DE_LENGTH_OF_ARRAY(queryResults); ++queryNdx)
		{
			if (queryNdx == QUERY_INDEX_CAPTURE_EMPTY && queryResults[queryNdx] != 0u)
			{
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"wrong value of query for index "
					<< queryNdx << ", expected any zero value, got "
					<< queryResults[0] << "." << tcu::TestLog::EndMessage;
				passed = false;
			}

			if (queryNdx != QUERY_INDEX_CAPTURE_EMPTY && queryResults[queryNdx] == 0)
			{
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"wrong value of query for index "
					<< queryNdx << ", expected any non-zero value, got "
					<< queryResults[0] << "." << tcu::TestLog::EndMessage;
				passed = false;
			}
		}
		log << tcu::TestLog::EndSection;
	}

	// Timestamp query verification
	if (m_supportedQueryType[QUERY_TYPE_TIMESTAMP])
	{
		std::pair<deUint64, deUint64>	queryResultsWithAvailabilityBit[NUM_QUERIES_IN_POOL];
		size_t queryResultsWithAvailabilityBitSize		= sizeof(queryResultsWithAvailabilityBit);
		vk::VkResult queryResult	= vk.getQueryPoolResults(device, *m_queryPools[QUERY_TYPE_TIMESTAMP], 0, NUM_QUERIES_IN_POOL, queryResultsWithAvailabilityBitSize, &queryResultsWithAvailabilityBit[0], sizeof(queryResultsWithAvailabilityBit[0]), vk::VK_QUERY_RESULT_64_BIT | vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

		if (queryResult != vk::VK_NOT_READY)
		{
			TCU_FAIL("We don't have available one query, it should return VK_NOT_READY");
		}

		log << tcu::TestLog::Section("TimestampQueryResults",
									 "Timestamp query results");
		for (int ndx = 0; ndx < NUM_QUERIES_IN_POOL; ++ndx)
		{
			log << tcu::TestLog::Message << "query[slot == " << ndx
				<< "] result == " << queryResultsWithAvailabilityBit[ndx].first << tcu::TestLog::EndMessage;
		}


		for (deUint32 queryNdx = 0; queryNdx < NUM_QUERIES_IN_POOL; ++queryNdx)
		{
			if (queryNdx == QUERY_INDEX_CAPTURE_EMPTY && (queryResultsWithAvailabilityBit[queryNdx].first != 0u || queryResultsWithAvailabilityBit[queryNdx].second != 0u))
			{
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"either wrong value of query for index "
					<< queryNdx << " (expected any zero value, got "
					<< queryResultsWithAvailabilityBit[queryNdx].first << ") or the result is available (" << queryResultsWithAvailabilityBit[queryNdx].second << ")"
					<< tcu::TestLog::EndMessage;
				passed = false;
			}

			if (queryNdx != QUERY_INDEX_CAPTURE_EMPTY && (queryResultsWithAvailabilityBit[queryNdx].first == 0u || queryResultsWithAvailabilityBit[queryNdx].second == 0u))
			{
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"either wrong value of query for index "
					<< queryNdx << " (expected any non-zero value, got "
					<< queryResults[0] << ") or result is unavailable." << tcu::TestLog::EndMessage;
				passed = false;
			}
		}
		log << tcu::TestLog::EndSection;
	}

	if (passed)
	{
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Query result verification passed");
	}
	return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Query result verification failed");
}

template<class Instance>
class QueryPoolConcurrentTest : public vkt::TestCase
{
public:
	QueryPoolConcurrentTest (tcu::TestContext &context, const char *name, const char *description)
		: TestCase			(context, name, description)
	{
	}
private:
	vkt::TestInstance* createInstance (vkt::Context& context) const
	{
		return new Instance(context);
	}

	void initPrograms(vk::SourceCollections& programCollection) const
	{
		const std::string fragSrc = std::string(
			"#version 400\n"
			"layout(location = 0) out vec4 out_FragColor;\n"
			"void main()\n"
			"{\n"
			"	out_FragColor = vec4(0.07, 0.48, 0.75, 1.0);\n"
			"	if ((int(gl_FragCoord.x) % 2) == (int(gl_FragCoord.y) % 2))\n"
			"		discard;\n"
			"}");

		programCollection.glslSources.add("frag") << glu::FragmentSource(fragSrc.c_str());

		programCollection.glslSources.add("vert") << glu::VertexSource("#version 430\n"
																		 "layout(location = 0) in vec4 in_Position;\n"
																		 "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
																		 "void main() {\n"
																		 "	gl_Position  = in_Position;\n"
																		 "	gl_PointSize = 1.0;\n"
																		 "}\n");
	}
};

} //anonymous

QueryPoolConcurrentTests::QueryPoolConcurrentTests (tcu::TestContext &testCtx)
	: TestCaseGroup(testCtx, "concurrent_queries", "Tests for concurrent queries")
{
	/* Left blank on purpose */
}

QueryPoolConcurrentTests::~QueryPoolConcurrentTests (void)
{
	/* Left blank on purpose */
}

void QueryPoolConcurrentTests::init (void)
{
	addChild(new QueryPoolConcurrentTest<PrimaryCommandBufferConcurrentTestInstance>(m_testCtx, "primary_command_buffer", ""));
	addChild(new QueryPoolConcurrentTest<SecondaryCommandBufferConcurrentTestInstance>(m_testCtx, "secondary_command_buffer", ""));
}

} //QueryPool
} //vkt
