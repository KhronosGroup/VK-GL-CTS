/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Vulkan Occlusion Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolOcclusionTests.hpp"

#include "vktTestCase.hpp"


#include "vktQueryPoolImageObjectUtil.hpp"
#include "vktQueryPoolBufferObjectUtil.hpp"
#include "vktQueryPoolCreateInfoUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"



using namespace vkt::QueryPool;

namespace
{
vk::Move<vk::VkShader> createShader(const vk::DeviceInterface &vk, const vk::VkDevice device, 
									const vk::VkShaderModule module, const char* name, const vk::VkShaderStage stage)
{
	vk::VkShaderCreateInfo createInfo;
	createInfo.sType	= vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO;
	createInfo.pNext	= DE_NULL;
	createInfo.module	= module;
	createInfo.pName	= name;
	createInfo.flags	= 0;
	createInfo.stage	= stage;
	return vk::createShader(vk, device, &createInfo);
}

struct StateObjects {
			StateObjects	(const vk::DeviceInterface&vk, vkt::Context &context, const int numVertices, vk::VkPrimitiveTopology primitive);
	void	setVertices		(const vk::DeviceInterface&vk, std::vector<tcu::Vec4> vertices);
	
	enum
	{
		WIDTH	= 128,
		HEIGHT	= 128
	};

	vkt::Context &m_context;

	vk::Move<vk::VkPipeline>		m_Pipeline;
	vk::Move<vk::VkPipelineLayout>	m_PipelineLayout;

	de::SharedPtr<Image>			m_ColorAttachmentImage, m_DepthImage;
	vk::Move<vk::VkImageView>		m_AttachmentView;
	vk::Move<vk::VkImageView>		m_DepthView;

	vk::Move<vk::VkRenderPass>		m_RenderPass;
	vk::Move<vk::VkFramebuffer>		m_Framebuffer;

	de::SharedPtr<Buffer>			m_VertexBuffer;

	vk::VkFormat					m_ColorAttachmentFormat;
};


StateObjects::StateObjects (const vk::DeviceInterface&vk, vkt::Context &context, const int numVertices, vk::VkPrimitiveTopology primitive)
	: m_context(context) 
	, m_ColorAttachmentFormat(vk::VK_FORMAT_R8G8B8A8_UNORM)

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

		ImageCreateInfo colorImageCreateInfo(
			vk::VK_IMAGE_TYPE_2D, m_ColorAttachmentFormat, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
			vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT);

		m_ColorAttachmentImage = Image::CreateAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator());

		ImageViewCreateInfo attachmentViewInfo(m_ColorAttachmentImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_ColorAttachmentFormat);
		m_AttachmentView = vk::createImageView(vk, device, &attachmentViewInfo);

		ImageCreateInfo depthImageCreateInfo(vk::VK_IMAGE_TYPE_2D, depthFormat, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
			vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

		m_DepthImage = Image::CreateAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator());

		// Construct a depth  view from depth image
		ImageViewCreateInfo depthViewInfo(m_DepthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, depthFormat);
		m_DepthView = vk::createImageView(vk, device, &depthViewInfo);
	}

	{
		// Renderpass and Framebuffer

		RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(
				m_ColorAttachmentFormat,								// format
				vk::VK_SAMPLE_COUNT_1_BIT,								// samples
				vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						// loadOp
				vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,					// storeOp
				vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// stencilLoadOp
				vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,					// stencilLoadOp
				vk::VK_IMAGE_LAYOUT_GENERAL,							// initialLauout
				vk::VK_IMAGE_LAYOUT_GENERAL								// finalLayout
			));

		renderPassCreateInfo.addAttachment(AttachmentDescription(
				depthFormat,											// format
				vk::VK_SAMPLE_COUNT_1_BIT,								// samples
				vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						// loadOp
				vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,					// storeOp
				vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// stencilLoadOp
				vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,					// stencilLoadOp
				vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// initialLauout
				vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// finalLayout
			));

		vk::VkAttachmentReference colorAttachmentReference =
		{
			0,															// attachment
			vk::VK_IMAGE_LAYOUT_GENERAL									// layout
		};


		vk::VkAttachmentReference depthAttachmentReference =
		{
			1,															// attachment
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL		// layout
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(
				vk::VK_PIPELINE_BIND_POINT_GRAPHICS,					// pipelineBindPoint
				0,														// flags
				0,														// inputCount
				DE_NULL,												// pInputAttachments
				1,														// colorCount
				&colorAttachmentReference,								// pColorAttachments
				DE_NULL,												// pResolveAttachments
				depthAttachmentReference,								// depthStencilAttachment
				0,														// preserveCount
				DE_NULL													// preserveAttachments
			));                                      

		m_RenderPass = vk::createRenderPass(vk, device, &renderPassCreateInfo);

		std::vector<vk::VkImageView> attachments(2);
		attachments[0] = *m_AttachmentView;
		attachments[1] = *m_DepthView;

		FramebufferCreateInfo framebufferCreateInfo(*m_RenderPass, attachments, WIDTH, HEIGHT, 0);
		m_Framebuffer = vk::createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	{
		// Pipeline

		const vk::Unique<vk::VkShader> vs(createShader(vk, device,
			*createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0),
			"main", vk::VK_SHADER_STAGE_VERTEX));

		const vk::Unique<vk::VkShader> fs(createShader(vk, device,
			*createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0),
			"main", vk::VK_SHADER_STAGE_FRAGMENT));

		PipelineCreateInfo::ColorBlendState::Attachment attachmentState;


		PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		m_PipelineLayout = vk::createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

		vk::VkVertexInputBindingDescription vf_binding_desc =		{
																		0,																// binding;
																		4 * sizeof(float),												// stride;
																		vk::VK_VERTEX_INPUT_STEP_RATE_VERTEX							// inputRate
																	};

		vk::VkVertexInputAttributeDescription vf_attribute_desc =	{
																		0,																// location;
																		0,																// binding;
																		vk::VK_FORMAT_R32G32B32A32_SFLOAT,								// format;
																		0																// offset;
																	};

		vk::VkPipelineVertexInputStateCreateInfo vf_info = 			{																	
																		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// sType;
																		NULL,															// pNext;
																		1,																// vertexBindingDescriptionCount;
																		&vf_binding_desc,												// pVertexBindingDescriptions;
																		1,																// vertexAttributeDescriptionCount;
																		&vf_attribute_desc												// pVertexAttributeDescriptions;
																	};

		PipelineCreateInfo pipelineCreateInfo(*m_PipelineLayout, *m_RenderPass, 0, 0);
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, vk::VK_SHADER_STAGE_VERTEX));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, vk::VK_SHADER_STAGE_FRAGMENT));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(primitive));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));
		vk::VkViewport viewport	=	{
										0,		// float x;
										0,		// float y;
										WIDTH,	// float width;
										HEIGHT,	// float height;
										0.0f,	// float minDepth;
										1.0f	// float maxDepth;
									};
		
		vk::VkRect2D scissor	=	{
										{
											0,		// deInt32 x
											0,		// deInt32 y
										},		// VkOffset2D	offset;
										{
											WIDTH,	// deInt32 width;
											HEIGHT,	// deInt32 height
										},		// VkExtent2D	extent;
									};
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport), std::vector<vk::VkRect2D>(1, scissor)));
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState(true, true, vk::VK_COMPARE_OP_GREATER_EQUAL));
		pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
		pipelineCreateInfo.addState(vf_info);
		m_Pipeline = vk::createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
	}

	{
		// Vertex buffer
		const size_t kBufferSize = numVertices * sizeof(tcu::Vec4);
		m_VertexBuffer = Buffer::CreateAndAlloc(vk, device, BufferCreateInfo(kBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);
	}
}

void StateObjects::setVertices (const vk::DeviceInterface&vk, std::vector<tcu::Vec4> vertices)
{
	const vk::VkDevice device			= m_context.getDevice();

	tcu::Vec4 *ptr = reinterpret_cast<tcu::Vec4*>(m_VertexBuffer->getBoundMemory().getHostPtr());
	std::copy(vertices.begin(), vertices.end(), ptr);

	vk::flushMappedMemoryRange(vk, device,	m_VertexBuffer->getBoundMemory().getMemory(), m_VertexBuffer->getBoundMemory().getOffset(),	vertices.size() * sizeof(vertices[0]));
}

enum OcclusionQueryResultSize
{
	RESULT_SIZE_64_BIT,
	RESULT_SIZE_32_BIT,
};

enum OcclusionQueryWait
{
	WAIT_QUEUE,
	WAIT_QUERY,
	WAIT_NONE
};

enum OcclusionQueryResultsMode	
{
	RESULTS_MODE_GET,
	RESULTS_MODE_COPY
};

struct OcclusionQueryTestVector
{
	vk::VkQueryControlFlags		queryControlFlags;
	OcclusionQueryResultSize	queryResultSize;
	OcclusionQueryWait			queryWait;
	OcclusionQueryResultsMode	queryResultsMode;
	vk::VkDeviceSize			queryResultsStride;
	bool						queryResultsAvailability;
	vk::VkPrimitiveTopology		primitiveRopology;
};


class BasicOcclusionQueryTestInstance : public vkt::TestInstance
{
public:
					BasicOcclusionQueryTestInstance		(vkt::Context &context, const OcclusionQueryTestVector&  testVector);
					~BasicOcclusionQueryTestInstance	(void);
private:
	tcu::TestStatus	iterate								(void);

	static const int			kNumQueriesInPool			= 2;
	static const int			kQueryIndexCaptureEmpty		= 0;
	static const int			kQueryIndexCaptureDrawcall	= 1;
	static const int			kNumVerticesInDrawCall		= 3;

	OcclusionQueryTestVector	m_testVector;
	StateObjects*				m_stateObjects;
	vk::VkQueryPool				m_queryPool;
};

BasicOcclusionQueryTestInstance::BasicOcclusionQueryTestInstance (vkt::Context &context, const OcclusionQueryTestVector&  testVector)
	: TestInstance		(context)
	, m_testVector		(testVector)
	, m_stateObjects	(new StateObjects(m_context.getDeviceInterface(), m_context, kNumVerticesInDrawCall, m_testVector.primitiveRopology))
{
	DE_ASSERT(testVector.queryResultSize			== RESULT_SIZE_64_BIT
			&& testVector.queryWait					== WAIT_QUEUE
			&& testVector.queryResultsMode			== RESULTS_MODE_GET
			&& testVector.queryResultsStride		== sizeof(deUint64)
			&& testVector.queryResultsAvailability	== false
			&& testVector.primitiveRopology			== vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

	tcu::TestLog&				log		= m_context.getTestContext().getLog();
	const vk::VkDevice			device	= m_context.getDevice();
	const vk::DeviceInterface&	vk		= m_context.getDeviceInterface();

	vk::VkQueryPoolCreateInfo queryPoolCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
		DE_NULL,
		vk::VK_QUERY_TYPE_OCCLUSION,
		kNumQueriesInPool,
		0
	};
	VK_CHECK(vk.createQueryPool(device, &queryPoolCreateInfo, &m_queryPool));

	std::vector<tcu::Vec4> vertices(kNumVerticesInDrawCall);
	vertices[0] = tcu::Vec4(0.5, 0.5, 0.0, 1.0);
	vertices[1] = tcu::Vec4(0.5, 0.0, 0.0, 1.0);
	vertices[2] = tcu::Vec4(0.0, 0.5, 0.0, 1.0);
	m_stateObjects->setVertices(vk, vertices);
}

BasicOcclusionQueryTestInstance::~BasicOcclusionQueryTestInstance (void)
{
	if (m_stateObjects)
		delete m_stateObjects;

	if (m_queryPool != DE_NULL)
	{
		const vk::VkDevice device		= m_context.getDevice();
		const vk::DeviceInterface& vk	= m_context.getDeviceInterface();

		vk.destroyQueryPool(device, m_queryPool);
	}
}

tcu::TestStatus	BasicOcclusionQueryTestInstance::iterate (void)
{
	tcu::TestLog &log				= m_context.getTestContext().getLog();
	const vk::VkDevice device		= m_context.getDevice();
	const vk::VkQueue queue			= m_context.getUniversalQueue();
	const vk::DeviceInterface& vk	= m_context.getDeviceInterface();

	const CmdPoolCreateInfo			cmdPoolCreateInfo	(m_context.getUniversalQueueFamilyIndex());
	vk::Unique<vk::VkCmdPool>		cmdPool				= vk::createCommandPool(vk, device, &cmdPoolCreateInfo);
	const CmdBufferCreateInfo		cmdBufCreateInfo	(*cmdPool, vk::VK_CMD_BUFFER_LEVEL_PRIMARY, 0);
	vk::Unique<vk::VkCmdBuffer>		cmdBuffer			(vk::createCommandBuffer(vk, device, &cmdBufCreateInfo));
	const CmdBufferBeginInfo		beginInfo;			//[scygan]: remember about query control flags here when porting to newer API

	vk.beginCommandBuffer(*cmdBuffer, &beginInfo);

	transition2DImage(vk, *cmdBuffer, m_stateObjects->m_ColorAttachmentImage->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	transition2DImage(vk, *cmdBuffer, m_stateObjects->m_DepthImage->object(), vk::VK_IMAGE_ASPECT_DEPTH_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	std::vector<vk::VkClearValue> renderPassClearValues(2);
	deMemset(&renderPassClearValues[0], static_cast<int>(renderPassClearValues.size()) * sizeof(vk::VkClearValue), 0);

	vk::VkRect2D renderArea = {
		{ 0, 0 },
		{ StateObjects::WIDTH, StateObjects::HEIGHT }
	};
	RenderPassBeginInfo renderPassBegin(*m_stateObjects->m_RenderPass, *m_stateObjects->m_Framebuffer, renderArea, renderPassClearValues);

	vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBegin, vk::VK_RENDER_PASS_CONTENTS_INLINE);

	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_stateObjects->m_Pipeline);

	vk::VkBuffer vertexBuffer = m_stateObjects->m_VertexBuffer->object();
	const vk::VkDeviceSize vertexBufferOffset = 0;
	vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	vk.cmdResetQueryPool(*cmdBuffer, m_queryPool, 0, kNumQueriesInPool);

	vk.cmdBeginQuery(*cmdBuffer, m_queryPool, kQueryIndexCaptureEmpty, m_testVector.queryControlFlags);
	vk.cmdEndQuery(*cmdBuffer, m_queryPool,	kQueryIndexCaptureEmpty);

	vk.cmdBeginQuery(*cmdBuffer, m_queryPool, kQueryIndexCaptureDrawcall, m_testVector.queryControlFlags);
	vk.cmdDraw(*cmdBuffer, kNumVerticesInDrawCall, 1, 0, 0);
	vk.cmdEndQuery(*cmdBuffer, m_queryPool,	kQueryIndexCaptureDrawcall);

	vk.cmdEndRenderPass(*cmdBuffer);

	transition2DImage(vk, *cmdBuffer, m_stateObjects->m_ColorAttachmentImage->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, vk::VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL);

	vk.endCommandBuffer(*cmdBuffer);

	// Submit command buffer
	vk::VkCmdBuffer buffer = *cmdBuffer;
	VK_CHECK(vk.queueSubmit(queue, 1, &buffer, DE_NULL));

	VK_CHECK(vk.queueWaitIdle(queue));

	deUint64 queryResults[kNumQueriesInPool] = { 0 };
	size_t queryResultsSize = sizeof(queryResults);

	vk::VkResult queryResult = vk.getQueryPoolResults(device, m_queryPool, 0, kNumQueriesInPool, &queryResultsSize, queryResults, vk::VK_QUERY_RESULT_64_BIT);

	if (queryResult == vk::VK_NOT_READY) {
		TCU_FAIL("Query result not avaliable, but vkWaitIdle() was called.");
	}

	VK_CHECK(queryResult);

	log << tcu::TestLog::Section("OcclusionQueryResults",
		"Occlusion query results");
	for (int i = 0; i < DE_LENGTH_OF_ARRAY(queryResults); ++i) {
		log << tcu::TestLog::Message << "query[ slot == " << i
			<< "] result == " << queryResults[i] << tcu::TestLog::EndMessage;
	}

	qpTestResult result = QP_TEST_RESULT_PASS;

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(queryResults); ++i) {
		
		deUint64 expectedValue;

		switch (i) {
			case kQueryIndexCaptureEmpty:
				expectedValue = 0;
				break;
			case kQueryIndexCaptureDrawcall:
				expectedValue = kNumVerticesInDrawCall;
				break;
		}

		if ((!m_testVector.queryControlFlags & vk::VK_QUERY_CONTROL_CONSERVATIVE_BIT) || expectedValue == 0) {
			// require precise value
			if (queryResults[i] != expectedValue) {
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"wrong value of query for index "
					<< i << ", expected " << expectedValue << ", got "
					<< queryResults[0] << "." << tcu::TestLog::EndMessage;
				result = QP_TEST_RESULT_FAIL;
			}
		}
		else {
			// require imprecize value > 0
			if (queryResults[i] == 0) {
				log << tcu::TestLog::Message << "vkGetQueryPoolResults returned "
					"wrong value of query for index "
					<< i << ", expected any non-zero value, got "
					<< queryResults[0] << "." << tcu::TestLog::EndMessage;
				result = QP_TEST_RESULT_FAIL;
			}
		}
	}
	log << tcu::TestLog::EndSection;

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };


	tcu::ConstPixelBufferAccess resultImageAccess = m_stateObjects->m_ColorAttachmentImage->readSurface(
				queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL,
				zeroOffset,  StateObjects::HEIGHT, StateObjects::WIDTH, vk::VK_IMAGE_ASPECT_COLOR);

	log << tcu::TestLog::Image("Result", "Result", resultImageAccess);

	return tcu::TestStatus(result, qpGetTestResultName(result));
}


class OcclusionQueryTestInstance : public vkt::TestInstance
{
public:
	OcclusionQueryTestInstance		(vkt::Context &context, const OcclusionQueryTestVector& testVector);
	~OcclusionQueryTestInstance		(void);
private:
	tcu::TestStatus					iterate							(void);

	vk::Move<vk::VkCmdBuffer>		recordRender					(vk::VkCmdPool commandPool);
	vk::Move<vk::VkCmdBuffer>		recordCopyResults				(vk::VkCmdPool commandPool);

	void							captureResults					(deUint64*			retResults,	deUint64*		retAvailability,	bool	allowNotReady);
	void							logResults						(const deUint64*	results,	const deUint64* availability);
	qpTestResult					validateResults					(const deUint64*	results,	const deUint64* availability,		bool	allowUnavailable,	vk::VkPrimitiveTopology primitiveTopology);
	void							logRenderTarget					(void);

	static const int				kNumQueriesInPool						= 3;
	static const int				kQueryIndexCaptureAll					= 0;
	static const int				kQueryIndexCapturePartiallyOccluded		= 1;
	static const int				kQueryIndexCaptureOccluded				= 2;

	static const int				kNumVerticesInDrawCall					= 3;
	static const int				kNumVerticesInPartialOccludeDrawCall	= 3;
	static const int				kNumVerticesInOccludeDrawCall			= 3;
	static const int				kNumVertices							= kNumVerticesInDrawCall + kNumVerticesInPartialOccludeDrawCall + kNumVerticesInOccludeDrawCall;
	static const int				kStartVertex							= 0;
	static const int				kPartialOccludeCallStartVertex			= kStartVertex + kNumVerticesInDrawCall;
	static const int				kOccludeStartVertex						= kPartialOccludeCallStartVertex + kNumVerticesInPartialOccludeDrawCall;

	OcclusionQueryTestVector		m_testVector;

	const vk::VkQueryResultFlags	m_queryResultFlags;

	StateObjects*					m_stateObjects;
	vk::VkQueryPool					m_queryPool;
	de::SharedPtr<Buffer>			m_queryPoolResultsBuffer;

	vk::Move<vk::VkCmdPool>			m_commandPool;
	vk::Move<vk::VkCmdBuffer>		m_renderCommandBuffer;
	vk::Move<vk::VkCmdBuffer>		m_copyResultsCommandBuffer;
};

OcclusionQueryTestInstance::OcclusionQueryTestInstance (vkt::Context &context, const OcclusionQueryTestVector& testVector)
	: vkt::TestInstance		(context)
	, m_testVector			(testVector)
	, m_stateObjects		(new StateObjects(m_context.getDeviceInterface(), m_context, kNumVerticesInDrawCall + kNumVerticesInPartialOccludeDrawCall + kNumVerticesInOccludeDrawCall, m_testVector.primitiveRopology))
	, m_queryResultFlags	((m_testVector.queryWait == WAIT_QUERY					? vk::VK_QUERY_RESULT_WAIT_BIT				: 0)
							| (m_testVector.queryResultSize == RESULT_SIZE_64_BIT	? vk::VK_QUERY_RESULT_64_BIT				: 0)
							| (m_testVector.queryResultsAvailability				? vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT	: 0))
{
	const vk::VkDevice			device				= m_context.getDevice();
	const vk::DeviceInterface&	vk					= m_context.getDeviceInterface();

	vk::VkQueryPoolCreateInfo queryPoolCreateInfo	=	{
															vk::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
															DE_NULL,
															vk::VK_QUERY_TYPE_OCCLUSION,
															kNumQueriesInPool,
															0
														};

	VK_CHECK(vk.createQueryPool(device, &queryPoolCreateInfo, &m_queryPool));
		
	if (m_testVector.queryResultsMode == RESULTS_MODE_COPY)
	{
		const vk::VkDeviceSize	resultsBufferSize			= m_testVector.queryResultsStride * kNumQueriesInPool;
								m_queryPoolResultsBuffer	= Buffer::CreateAndAlloc(vk, device, BufferCreateInfo(resultsBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT), m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);
	}
	
	const CmdPoolCreateInfo	cmdPoolCreateInfo		(m_context.getUniversalQueueFamilyIndex());
							m_commandPool			= vk::createCommandPool(vk, device, &cmdPoolCreateInfo);
							m_renderCommandBuffer	= recordRender(*m_commandPool);

	if (m_testVector.queryWait == WAIT_QUEUE && m_testVector.queryResultsMode == RESULTS_MODE_COPY)
	{
		m_copyResultsCommandBuffer = recordCopyResults(*m_commandPool);
	}
}

OcclusionQueryTestInstance::~OcclusionQueryTestInstance (void)
{
	const vk::VkDevice device = m_context.getDevice();

	if (m_stateObjects)
		delete m_stateObjects;

	if (m_queryPool != DE_NULL)
	{	
		const vk::DeviceInterface& vk = m_context.getDeviceInterface();
		vk.destroyQueryPool(device, m_queryPool);
	}
}

tcu::TestStatus OcclusionQueryTestInstance::iterate (void)
{
	const vk::VkDevice			device		= m_context.getDevice();
	const vk::VkQueue			queue		= m_context.getUniversalQueue();
	const vk::DeviceInterface&	vk			= m_context.getDeviceInterface();
	tcu::TestLog&				log			= m_context.getTestContext().getLog();
	std::vector<tcu::Vec4>		vertices	(kNumVertices);

	// 1st triangle
	vertices[kStartVertex + 0] = tcu::Vec4( 0.5,  0.5, 0.5, 1.0);
	vertices[kStartVertex + 1] = tcu::Vec4( 0.5, -0.5, 0.5, 1.0);
	vertices[kStartVertex + 2] = tcu::Vec4(-0.5,  0.5, 0.5, 1.0);
	// 2nd triangle - partially occluding the scene
	vertices[kPartialOccludeCallStartVertex + 0] = tcu::Vec4(-0.5, -0.5, 1.0, 1.0);
	vertices[kPartialOccludeCallStartVertex + 1] = tcu::Vec4( 0.5, -0.5, 1.0, 1.0);
	vertices[kPartialOccludeCallStartVertex + 2] = tcu::Vec4(-0.5,  0.5, 1.0, 1.0);
	// 3nd triangle - fully occluding the scene
	vertices[kOccludeStartVertex + 0] = tcu::Vec4( 0.5,  0.5, 1.0, 1.0);
	vertices[kOccludeStartVertex + 1] = tcu::Vec4( 0.5, -0.5, 1.0, 1.0);
	vertices[kOccludeStartVertex + 2] = tcu::Vec4(-0.5,  0.5, 1.0, 1.0);

	m_stateObjects->setVertices(vk, vertices);

	vk::VkCmdBuffer buffer = *m_renderCommandBuffer;
	VK_CHECK(vk.queueSubmit(queue, 1, &buffer, DE_NULL));

	if (m_testVector.queryWait == WAIT_QUEUE)
	{
		VK_CHECK(vk.queueWaitIdle(queue));

		if (m_testVector.queryResultsMode == RESULTS_MODE_COPY)
		{
			vk::VkCmdBuffer buffer = *m_copyResultsCommandBuffer;
			VK_CHECK(vk.queueSubmit(queue, 1, &buffer, DE_NULL));

			VK_CHECK(vk.queueWaitIdle(queue));
		}
	}

	deUint64 queryResults		[kNumQueriesInPool]; 
	deUint64 queryAvailability	[kNumQueriesInPool]; 
	captureResults(queryResults, queryAvailability, m_testVector.queryWait == WAIT_NONE);

	log << tcu::TestLog::Section("OcclusionQueryResults", "Occlusion query results");

	logResults(queryResults, queryAvailability);
	qpTestResult result = validateResults(queryResults, queryAvailability, m_testVector.queryWait == WAIT_NONE, m_testVector.primitiveRopology);

	log << tcu::TestLog::EndSection;

	logRenderTarget();

	return tcu::TestStatus(result, qpGetTestResultName(result));
}

vk::Move<vk::VkCmdBuffer> OcclusionQueryTestInstance::recordRender (vk::VkCmdPool cmdPool)
{
	const vk::VkDevice				device = m_context.getDevice();
	const vk::VkQueue				queue = m_context.getUniversalQueue();
	const vk::DeviceInterface&		vk = m_context.getDeviceInterface();
	const CmdBufferCreateInfo		cmdBufCreateInfo	(cmdPool, vk::VK_CMD_BUFFER_LEVEL_PRIMARY, 0);
	vk::Move<vk::VkCmdBuffer>		cmdBuffer			(vk::createCommandBuffer(vk, device, &cmdBufCreateInfo));
	const CmdBufferBeginInfo		beginInfo;			//[scygan]: remember about query control flags here when porting to newer API

	vk.beginCommandBuffer(*cmdBuffer, &beginInfo);

	transition2DImage(vk, *cmdBuffer, m_stateObjects->m_ColorAttachmentImage->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	transition2DImage(vk, *cmdBuffer, m_stateObjects->m_DepthImage->object(), vk::VK_IMAGE_ASPECT_DEPTH_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	std::vector<vk::VkClearValue>	renderPassClearValues(2);
	deMemset(&renderPassClearValues[0], static_cast<int>(renderPassClearValues.size()) * sizeof(vk::VkClearValue), 0);
	vk::VkRect2D renderArea =	{
									{ 0, 0 },
									{ StateObjects::WIDTH,  StateObjects::HEIGHT }
								};

	RenderPassBeginInfo renderPassBegin(*m_stateObjects->m_RenderPass, *m_stateObjects->m_Framebuffer, renderArea, renderPassClearValues);

	vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBegin, vk::VK_RENDER_PASS_CONTENTS_INLINE);

	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS,	*m_stateObjects->m_Pipeline);

	vk::VkBuffer vertexBuffer = m_stateObjects->m_VertexBuffer->object();
	const vk::VkDeviceSize vertexBufferOffset = 0;
	vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	vk.cmdResetQueryPool(*cmdBuffer, m_queryPool, 0, kNumQueriesInPool);

	// Draw un-occluded geometry
	vk.cmdBeginQuery(*cmdBuffer, m_queryPool, kQueryIndexCaptureAll, m_testVector.queryControlFlags);
	vk.cmdDraw(*cmdBuffer, kNumVerticesInDrawCall, 1, kStartVertex, 0);
	vk.cmdEndQuery(*cmdBuffer, m_queryPool,	kQueryIndexCaptureAll);

	// Partially occlude geometry
	vk.cmdDraw(*cmdBuffer, kNumVerticesInPartialOccludeDrawCall, 1, kPartialOccludeCallStartVertex, 0);

	// Draw partially-occluded geometry
	vk.cmdBeginQuery(*cmdBuffer, m_queryPool, kQueryIndexCapturePartiallyOccluded, m_testVector.queryControlFlags);
	vk.cmdDraw(*cmdBuffer, kNumVerticesInDrawCall, 1, kStartVertex, 0);
	vk.cmdEndQuery(*cmdBuffer, m_queryPool, kQueryIndexCapturePartiallyOccluded);

	// Occlude geometry
	vk.cmdDraw(*cmdBuffer, kNumVerticesInOccludeDrawCall, 1, kOccludeStartVertex, 0);

	// Draw occluded geometry
	vk.cmdBeginQuery(*cmdBuffer, m_queryPool, kQueryIndexCaptureOccluded, m_testVector.queryControlFlags);
	vk.cmdDraw(*cmdBuffer, kNumVerticesInDrawCall, 1, kStartVertex, 0);
	vk.cmdEndQuery(*cmdBuffer, m_queryPool,	kQueryIndexCaptureOccluded);

	if (m_testVector.queryWait != WAIT_QUEUE )
	{
		//For WAIT_QUEUE another cmdBuffer is issued with cmdCopyQueryPoolResults
		if (m_testVector.queryResultsMode == RESULTS_MODE_COPY)
		{
			vk.cmdCopyQueryPoolResults(*cmdBuffer, m_queryPool, 0, kNumQueriesInPool, m_queryPoolResultsBuffer->object(), /*dstOffset*/ 0, m_testVector.queryResultsStride, m_queryResultFlags);
		}
	}

	vk.cmdEndRenderPass(*cmdBuffer);

	transition2DImage(vk, *cmdBuffer, m_stateObjects->m_ColorAttachmentImage->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, vk::VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL);

	vk.endCommandBuffer(*cmdBuffer);

	return cmdBuffer;
}

vk::Move<vk::VkCmdBuffer> OcclusionQueryTestInstance::recordCopyResults (vk::VkCmdPool cmdPool)
{
	const vk::VkDevice				device				= m_context.getDevice();
	const vk::VkQueue				queue				= m_context.getUniversalQueue();
	const vk::DeviceInterface&		vk					= m_context.getDeviceInterface();
	const CmdBufferCreateInfo		cmdBufCreateInfo	(cmdPool, vk::VK_CMD_BUFFER_LEVEL_PRIMARY, 0);
	vk::Move<vk::VkCmdBuffer>		cmdBuffer			(vk::createCommandBuffer(vk, device, &cmdBufCreateInfo));
	const CmdBufferBeginInfo		beginInfo;			//[scygan]: remember about query control flags here when porting to newer API

	vk.beginCommandBuffer(*cmdBuffer, &beginInfo);
	vk.cmdCopyQueryPoolResults(*cmdBuffer, m_queryPool, 0, kNumQueriesInPool, m_queryPoolResultsBuffer->object(), /*dstOffset*/ 0, m_testVector.queryResultsStride, m_queryResultFlags);
	vk.endCommandBuffer(*cmdBuffer);

	return cmdBuffer;
}


void OcclusionQueryTestInstance::captureResults (deUint64* retResults, deUint64* retAvailAbility, bool allowNotReady)
{

	const vk::VkDevice			device			= m_context.getDevice();
	const vk::DeviceInterface&	vk				= m_context.getDeviceInterface();
	std::vector<deUint8>		resultsBuffer	(m_testVector.queryResultsStride * kNumQueriesInPool);

	if (m_testVector.queryResultsMode == RESULTS_MODE_GET)
	{
		size_t resultsSize = resultsBuffer.size();
		const vk::VkResult queryResult = vk.getQueryPoolResults(device, m_queryPool, 0, kNumQueriesInPool, &resultsSize, &resultsBuffer[0], m_queryResultFlags);
		if (queryResult == vk::VK_NOT_READY && !allowNotReady)
		{
			TCU_FAIL("getQueryPoolResults returned VK_NOT_READY, but results should be already available.");
		}
		else
		{
			VK_CHECK(queryResult);
		}
	}
	else if (m_testVector.queryResultsMode == RESULTS_MODE_COPY)
	{
		const vk::Allocation& allocation = m_queryPoolResultsBuffer->getBoundMemory();
		const void* allocationData = allocation.getHostPtr();

		vk::invalidateMappedMemoryRange(vk, device, allocation.getMemory(), allocation.getOffset(), resultsBuffer.size());

		deMemcpy(&resultsBuffer[0], allocationData, resultsBuffer.size());
	}


	for (int i = 0; i < kNumQueriesInPool; i++)
	{
		const void* srcPtr = &resultsBuffer[i * m_testVector.queryResultsStride];
		if (m_testVector.queryResultSize == RESULT_SIZE_32_BIT)
		{
			const deUint32* srcPtrTyped = static_cast<const deUint32*>(srcPtr);
			retResults[i] = *srcPtrTyped;
			if (m_testVector.queryResultsAvailability)
			{
				retAvailAbility[i] = *(srcPtrTyped + 1);
			}
		}
		else if (m_testVector.queryResultSize == RESULT_SIZE_64_BIT)
		{
			const deUint64* srcPtrTyped = static_cast<const deUint64*>(srcPtr);
			retResults[i] = *srcPtrTyped;

			if (m_testVector.queryResultsAvailability)
			{
				if (m_testVector.queryResultsAvailability)
				{
					retAvailAbility[i] = *(srcPtrTyped + 1);
				}
			}
		}
		else
		{
			TCU_FAIL("Wrong m_testVector.queryResultSize");
		}
	}
}

void OcclusionQueryTestInstance::logResults (const deUint64* results, const deUint64* availability)
{
	tcu::TestLog& log = m_context.getTestContext().getLog();

	for (int i = 0; i < kNumQueriesInPool; ++i)
	{
		if (!m_testVector.queryResultsAvailability)
		{
			log << tcu::TestLog::Message << "query[ slot == " << i	<< "] result == " << results[i] << tcu::TestLog::EndMessage;
		}
		else
		{
			log << tcu::TestLog::Message << "query[ slot == " << i	<< "] result == " << results[i] << ", availability	== " << availability[i] << tcu::TestLog::EndMessage;
		}
	}
}

qpTestResult OcclusionQueryTestInstance::validateResults (const deUint64* results , const deUint64* availability, bool allowUnavailable, vk::VkPrimitiveTopology primitiveTopology)
{
	qpTestResult result = QP_TEST_RESULT_PASS;
	tcu::TestLog& log	= m_context.getTestContext().getLog();

	for (int i = 0; i < kNumQueriesInPool; ++i)
	{
		deUint64 expectedValueMin = 0;
		deUint64 expectedValueMax = 0;

		if (m_testVector.queryResultsAvailability && availability[i] == 0)
		{
			// query result was not available
			if (!allowUnavailable)
			{
				log << tcu::TestLog::Message << "query results availability was 0 for index "
					<< i << ", expected any value greater than 0." << tcu::TestLog::EndMessage;
				result = QP_TEST_RESULT_FAIL;
				continue;
			}
		}
		else
		{
			// query is available, so expect proper result values
			if (primitiveTopology == vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
			{
				switch (i) {
					case kQueryIndexCaptureOccluded:
						expectedValueMin = 0;
						expectedValueMax = 0;
						break;
					case kQueryIndexCapturePartiallyOccluded:
						expectedValueMin = 1;
						expectedValueMax = 1;
						break;
					case kQueryIndexCaptureAll:
						expectedValueMin = kNumVerticesInDrawCall;
						expectedValueMax = kNumVerticesInDrawCall;
						break;
				}
			}
			else if (primitiveTopology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			{
				switch (i) {
					case kQueryIndexCaptureOccluded:
						expectedValueMin = 0;
						expectedValueMax = 0;
						break;
					case kQueryIndexCapturePartiallyOccluded:
					case kQueryIndexCaptureAll:
						{
							const int primWidth		= StateObjects::WIDTH  / 2;
							const int primHeight	= StateObjects::HEIGHT / 2;
							const int primArea		= primWidth * primHeight / 2;
							expectedValueMin		= (int)(0.97f * primArea);
							expectedValueMax		= (int)(1.03f * primArea);
						}
				}
			}
			else
			{
				TCU_FAIL("Unsupported primitive topology");
			}
		}

		if ((!m_testVector.queryControlFlags & vk::VK_QUERY_CONTROL_CONSERVATIVE_BIT) || (expectedValueMin == 0 && expectedValueMax == 0))
		{
			// require precise value
			if (results[i] < expectedValueMin || results[i] > expectedValueMax) {
				log << tcu::TestLog::Message << "wrong value of query for index "
					<< i << ", expected the value minimum of " << expectedValueMin << ", maximum of " << expectedValueMax << " got "
					<< results[i] << "." << tcu::TestLog::EndMessage;
				result = QP_TEST_RESULT_FAIL;
			}
		}
		else
		{
			// require imprecise value greater than 0
			if (results[i] == 0)
			{
				log << tcu::TestLog::Message << "wrong value of query for index "
					<< i << ", expected any non-zero value, got "
					<< results[i] << "." << tcu::TestLog::EndMessage;
				result = QP_TEST_RESULT_FAIL;
			}
		}
	}
	return result;
}

void OcclusionQueryTestInstance::logRenderTarget (void)
{
	tcu::TestLog&			log						= m_context.getTestContext().getLog();
	const vk::VkQueue		queue					= m_context.getUniversalQueue();
	const vk::VkOffset3D	zeroOffset				= { 0, 0, 0 };
	tcu::ConstPixelBufferAccess resultImageAccess	= m_stateObjects->m_ColorAttachmentImage->readSurface(
		queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
		zeroOffset, StateObjects::HEIGHT, StateObjects::WIDTH, vk::VK_IMAGE_ASPECT_COLOR);

	log << tcu::TestLog::Image("Result", "Result", resultImageAccess);
}

template<class Instance>
class QueryPoolOcclusionTest : public vkt::TestCase
{
public:
	QueryPoolOcclusionTest (tcu::TestContext &context, const char *name, const char *description, const OcclusionQueryTestVector& testVector)
		: TestCase			(context, name, description)
		, m_testVector		(testVector)
	{
	}
private:
	vkt::TestInstance* createInstance (vkt::Context& context) const
	{
		return new Instance(context, m_testVector);
	}

	void initPrograms(vk::SourceCollections& programCollection) const
	{
		programCollection.glslSources.add("frag") << glu::FragmentSource("#version 400\n"
																	   "layout(location = 0) out vec4 out_FragColor;\n"
																	   "void main()\n"
																	   "{\n"
																	   "	out_FragColor = vec4(0.07, 0.48, 0.75, 1.0);\n"
																	   "}\n");

		programCollection.glslSources.add("vert") << glu::VertexSource("#version 430\n"
																		 "layout(location = 0) in vec4 in_Postion;\n"
																		 "void main() {\n"
																		 "	gl_Position  = in_Postion;\n"
																		 "	gl_PointSize = 1.0;\n"
																		 "}\n");	
	}


	OcclusionQueryTestVector m_testVector;
};

} //anonymous


namespace vkt {

namespace QueryPool
{

QueryPoolOcclusionTests::QueryPoolOcclusionTests (tcu::TestContext &testCtx)
	: TestCaseGroup(testCtx, "occlusion_query", "Tests for occlusion queries")
{
	/* Left blank on purpose */
}

QueryPoolOcclusionTests::~QueryPoolOcclusionTests (void)
{
	/* Left blank on purpose */
}

void QueryPoolOcclusionTests::init (void)
{
	OcclusionQueryTestVector baseTestVector; 
	baseTestVector.queryControlFlags		= 0; 
	baseTestVector.queryResultSize			= RESULT_SIZE_64_BIT;
	baseTestVector.queryWait				= WAIT_QUEUE;
	baseTestVector.queryResultsMode			= RESULTS_MODE_GET;
	baseTestVector.queryResultsStride		= sizeof(deUint64);
	baseTestVector.queryResultsAvailability = false;
	baseTestVector.primitiveRopology		= vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

	//Basic tests
	{
		OcclusionQueryTestVector testVector = baseTestVector;
		testVector.queryControlFlags = 0;
		addChild(new QueryPoolOcclusionTest<BasicOcclusionQueryTestInstance>(m_testCtx,	"basic_conservative",	"draw with conservative occlusion query",	testVector));
		testVector.queryControlFlags = 0;
		addChild(new QueryPoolOcclusionTest<BasicOcclusionQueryTestInstance>(m_testCtx,	"basic_precise",		"draw with precise occlusion query",		testVector));
	}

	// Functional test
	{
		vk::VkQueryControlFlags	controlFlags[]		= { vk::VK_QUERY_CONTROL_CONSERVATIVE_BIT,	0			};
		const char*				controlFlagsStr[]	= { "conservative",							"precise"	};
		
		for (int controlFlagIdx = 0; controlFlagIdx < DE_LENGTH_OF_ARRAY(controlFlags); ++controlFlagIdx)
		{

			vk::VkPrimitiveTopology	primitiveTopology[]		= { vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
			const char*				primitiveTopologyStr[]	= { "points", "triangles" };
			for (int primitiveTopologyIdx = 0; primitiveTopologyIdx < DE_LENGTH_OF_ARRAY(primitiveTopology); ++primitiveTopologyIdx)
			{

				OcclusionQueryResultSize	resultSize[]	= { RESULT_SIZE_32_BIT, RESULT_SIZE_64_BIT };
				const char*					resultSizeStr[] = { "32",				"64" };

				for (int resultSizeIdx = 0; resultSizeIdx < DE_LENGTH_OF_ARRAY(resultSize); ++resultSizeIdx)
				{
	
					OcclusionQueryWait	wait[]		= { WAIT_QUEUE, WAIT_QUERY };
					const char*			waitStr[]	= { "queue",	"query" };

					for (int waitIdx = 0; waitIdx < DE_LENGTH_OF_ARRAY(wait); ++waitIdx)
					{
						OcclusionQueryResultsMode	resultsMode[]		= { RESULTS_MODE_GET,	RESULTS_MODE_COPY };
						const char*					resultsModeStr[]	= { "get",				"copy" };

						for (int resultsModeIdx = 0; resultsModeIdx < DE_LENGTH_OF_ARRAY(resultsMode); ++resultsModeIdx)
						{

							bool testAvailability[]				= { false, true };
							const char* testAvailabilityStr[]	= { "without", "with"};

							for (int testAvailabilityIdx = 0; testAvailabilityIdx < DE_LENGTH_OF_ARRAY(testAvailability); ++testAvailabilityIdx)
							{
								OcclusionQueryTestVector testVector			= baseTestVector;
								testVector.queryControlFlags				= controlFlags[controlFlagIdx];
								testVector.queryResultSize					= resultSize[resultSizeIdx];
								testVector.queryWait						= wait[waitIdx];
								testVector.queryResultsMode					= resultsMode[resultsModeIdx];
								testVector.queryResultsStride				= (testVector.queryResultSize == RESULT_SIZE_32_BIT ? sizeof(deUint32) : sizeof(deUint64));
								testVector.queryResultsAvailability			= testAvailability[testAvailabilityIdx];
								testVector.primitiveRopology				= primitiveTopology[primitiveTopologyIdx];

								if (testVector.queryResultsAvailability)
								{
									testVector.queryResultsStride *= 2;
								}

								std::ostringstream testName;
								std::ostringstream testDescr;

								testName << resultsModeStr[resultsModeIdx] << "_results"
										 << "_" << controlFlagsStr[controlFlagIdx]
										 << "_size_" << resultSizeStr[resultSizeIdx] 
										 << "_wait_" << waitStr[waitIdx] 
										 << "_" << testAvailabilityStr[testAvailabilityIdx] << "_availability"
										 << "_draw_" <<  primitiveTopologyStr[primitiveTopologyIdx];

								testDescr << "draw occluded " << primitiveTopologyStr[primitiveTopologyIdx] 
										  << "with " << controlFlagsStr[controlFlagIdx] << ", "
									      << resultsModeStr[resultsModeIdx] << " results "
									      << testAvailabilityStr[testAvailabilityIdx] << " availability bit as " 
										  << resultSizeStr[resultSizeIdx] << "bit variables,"
									      << "wait for results on" << waitStr[waitIdx];

								addChild(new QueryPoolOcclusionTest<OcclusionQueryTestInstance>(m_testCtx, testName.str().c_str(), testDescr.str().c_str(), testVector));
							}
						}
					}
				}
			}
		}
	}
#if 0
//[scygan]: these test should be enabled on newer API, where <stride> parameter is accepted by Copy and Get results function
	// Test different strides
	{
		OcclusionQueryResultsMode	resultsMode[]		= { RESULTS_MODE_GET,	RESULTS_MODE_COPY	};
		const char*					resultsModeStr[]	= { "get",				"copy"				};

		for (int resultsModeIdx = 0; resultsModeIdx < DE_LENGTH_OF_ARRAY(resultsMode); ++resultsModeIdx)
		{
			OcclusionQueryResultSize	resultSize[]	= { RESULT_SIZE_32_BIT, RESULT_SIZE_64_BIT };
			const char*					resultSizeStr[] = { "32", "64" };

			bool testAvailability[]				= { false,		true	};
			const char* testAvailabilityStr[]	= { "without",	"with"	};

			for (int testAvailabilityIdx = 0; testAvailabilityIdx < DE_LENGTH_OF_ARRAY(testAvailability); ++testAvailabilityIdx)
			{
				for (int resultSizeIdx = 0; resultSizeIdx < DE_LENGTH_OF_ARRAY(resultSize); ++resultSizeIdx)
				{
					const vk::VkDeviceSize		strides[] = { 4, 5, 8, 9, 13, 1024 };

					for (int strideIdx = 0; strideIdx < DE_LENGTH_OF_ARRAY(strides); strideIdx++)
					{
						OcclusionQueryTestVector testVector		= baseTestVector;
						testVector.queryResultsMode				= resultsMode[resultsModeIdx];
						testVector.queryResultSize				= resultSize[resultSizeIdx];
						testVector.queryResultsAvailability		= testAvailability[testAvailabilityIdx];
						testVector.queryResultsStride			= strides[strideIdx];

						vk::VkDeviceSize elementSize		= (testVector.queryResultSize == RESULT_SIZE_32_BIT ? sizeof(deUint32) : sizeof(deUint64));
						if (testVector.queryResultsAvailability)
						{
							elementSize *= 2;
						}

						if (elementSize > testVector.queryResultsStride)
						{
							continue;
						}

						std::ostringstream testName;
						std::ostringstream testDescr;

						testName << resultsModeStr[resultsModeIdx] 
								 << "_results_size_" << resultSizeStr[resultSizeIdx] 
								 << "_stride_" << strides[strideIdx] 
								 << "_" << testAvailabilityStr[testAvailabilityIdx] << "_availability";

						testDescr << resultsModeStr[resultsModeIdx] << " results "
								  << testAvailabilityStr[testAvailabilityIdx] << " availability bit as " 
								  << resultSizeStr[resultSizeIdx] << "bit variables, with stride" << strides[strideIdx];

						addChild(new QueryPoolOcclusionTest<OcclusionQueryTestInstance>(m_testCtx, testName.str().c_str(), testDescr.str().c_str(), testVector));
					}
				}
			}
		}
	
	}
#endif //0
}

} //QueryPool
} //vkt


