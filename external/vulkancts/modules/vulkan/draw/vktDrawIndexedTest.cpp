/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Draw Indexed Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawIndexedTest.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"

#include "vktDrawBaseClass.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{

enum
{
	VERTEX_OFFSET_DEFAULT	= 13,
	VERTEX_OFFSET_MINUS_ONE	= -1,
	VERTEX_OFFSET_NEGATIVE	= -13,
};

class DrawIndexed : public DrawTestsBaseClass
{
public:
	struct TestSpec : public TestSpecBase
	{
		const int32_t vertexOffset;

		TestSpec (const ShaderMap& shaders_, vk::VkPrimitiveTopology topology_, SharedGroupParams groupParams_, int32_t vertexOffset_)
			: TestSpecBase{shaders_, topology_, groupParams_}
			, vertexOffset(vertexOffset_)
		{
		}
	};

								DrawIndexed				(Context &context, TestSpec testSpec);
	virtual		tcu::TestStatus iterate					(void);
protected:
	std::vector<deUint32>		m_indexes;
	de::SharedPtr<Buffer>		m_indexBuffer;
	const int32_t				m_vertexOffset;
};

class DrawInstancedIndexed : public DrawIndexed
{
public:
								DrawInstancedIndexed	(Context &context, TestSpec testSpec);
	virtual		tcu::TestStatus	iterate					(void);
};

DrawIndexed::DrawIndexed (Context &context, TestSpec testSpec)
	: DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT], testSpec.groupParams, testSpec.topology)
	, m_vertexOffset	(testSpec.vertexOffset)
{
	// When using a positive vertex offset, the strategy is:
	// - Storing vertices with that offset in the vertex buffer.
	// - Using indices normally as if they were stored at the start of the buffer.
	//
	// When using a negative vertex offset, the strategy is:
	// - Store vertices at the start of the vertex buffer.
	// - Increase indices by abs(offset) so when substracting it, it results in the regular positions.

	const uint32_t indexOffset = (m_vertexOffset < 0 ? static_cast<uint32_t>(-m_vertexOffset) : 0u);

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_indexes.push_back(0 + indexOffset);
			m_indexes.push_back(0 + indexOffset);
			m_indexes.push_back(2 + indexOffset);
			m_indexes.push_back(0 + indexOffset);
			m_indexes.push_back(6 + indexOffset);
			m_indexes.push_back(6 + indexOffset);
			m_indexes.push_back(0 + indexOffset);
			m_indexes.push_back(7 + indexOffset);
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_indexes.push_back(0 + indexOffset);
			m_indexes.push_back(0 + indexOffset);
			m_indexes.push_back(2 + indexOffset);
			m_indexes.push_back(0 + indexOffset);
			m_indexes.push_back(6 + indexOffset);
			m_indexes.push_back(5 + indexOffset);
			m_indexes.push_back(0 + indexOffset);
			m_indexes.push_back(7 + indexOffset);
			break;

		case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LAST:
			DE_FATAL("Topology not implemented");
			break;
		default:
			DE_FATAL("Unknown topology");
			break;
	}

	// This works for both positive and negative vertex offsets.
	for (int unusedIdx = 0; unusedIdx < m_vertexOffset; unusedIdx++)
	{
		m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
	}

	int vertexIndex = (m_vertexOffset >= 0 ? m_vertexOffset : 0);

	m_data.push_back(VertexElementData(tcu::Vec4(	-0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
	m_data.push_back(VertexElementData(tcu::Vec4(	-1.0f,	 1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
	m_data.push_back(VertexElementData(tcu::Vec4(	-0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
	m_data.push_back(VertexElementData(tcu::Vec4(	 1.0f,	-1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
	m_data.push_back(VertexElementData(tcu::Vec4(	-0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
	m_data.push_back(VertexElementData(tcu::Vec4(	 0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
	m_data.push_back(VertexElementData(tcu::Vec4(	 0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
	m_data.push_back(VertexElementData(tcu::Vec4(	 0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));

	m_data.push_back(VertexElementData(tcu::Vec4(	-1.0f,	 1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec(), -1));

	initialize();
}

tcu::TestStatus DrawIndexed::iterate (void)
{
	tcu::TestLog&		log		= m_context.getTestContext().getLog();
	const vk::VkQueue	queue	= m_context.getUniversalQueue();
	const vk::VkDevice	device	= m_context.getDevice();

	const vk::VkDeviceSize dataSize = m_indexes.size() * sizeof(deUint32);
	m_indexBuffer = Buffer::createAndAlloc(	m_vk, m_context.getDevice(),
											BufferCreateInfo(dataSize,
															 vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
											m_context.getDefaultAllocator(),
											vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(m_indexBuffer->getBoundMemory().getHostPtr());
	deMemcpy(ptr, &m_indexes[0], static_cast<size_t>(dataSize));
	vk::flushAlloc(m_vk, m_context.getDevice(), m_indexBuffer->getBoundMemory());

	const vk::VkDeviceSize	vertexBufferOffset	= 0;
	const vk::VkBuffer		vertexBuffer		= m_vertexBuffer->object();
	const vk::VkBuffer		indexBuffer			= m_indexBuffer->object();

#ifndef CTS_USES_VULKANSC
	if (m_groupParams->useSecondaryCmdBuffer)
	{
		// record secondary command buffer
		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*m_secCmdBuffer);
		}
		else
			beginSecondaryCmdBuffer(m_vk);

		m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_secCmdBuffer, indexBuffer, 0, vk::VK_INDEX_TYPE_UINT32);
		m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDrawIndexed(*m_secCmdBuffer, 6, 1, 2, m_vertexOffset, 0);

		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if (m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginDynamicRender(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, indexBuffer, 0, vk::VK_INDEX_TYPE_UINT32);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 1, 2, m_vertexOffset, 0);

		endDynamicRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginLegacyRender(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, indexBuffer, 0, vk::VK_INDEX_TYPE_UINT32);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 1, 2, m_vertexOffset, 0);

		endLegacyRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	// Validation
	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
								  referenceFrame.allocLevel(0);

	const deInt32 frameWidth	= referenceFrame.getWidth();
	const deInt32 frameHeight	= referenceFrame.getHeight();

	tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	ReferenceImageCoordinates refCoords;

	for (int y = 0; y < frameHeight; y++)
	{
		const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

		for (int x = 0; x < frameWidth; x++)
		{
			const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

			if ((yCoord >= refCoords.bottom &&
				 yCoord <= refCoords.top	&&
				 xCoord >= refCoords.left	&&
				 xCoord <= refCoords.right))
				referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
		}
	}

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
		referenceFrame.getLevel(0), renderedFrame, 0.05f,
		tcu::COMPARE_LOG_RESULT)) {
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

DrawInstancedIndexed::DrawInstancedIndexed (Context &context, TestSpec testSpec)
	: DrawIndexed	(context, testSpec)
{
}

tcu::TestStatus DrawInstancedIndexed::iterate (void)
{
	tcu::TestLog&		log		= m_context.getTestContext().getLog();
	const vk::VkQueue	queue	= m_context.getUniversalQueue();
	const vk::VkDevice	device	= m_context.getDevice();

	beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
	preRenderBarriers();

#ifndef CTS_USES_VULKANSC
	if (m_groupParams->useDynamicRendering)
		beginDynamicRender(*m_cmdBuffer);
	else
		beginLegacyRender(*m_cmdBuffer);
#else
	beginLegacyRender(*m_cmdBuffer);
#endif // CTS_USES_VULKANSC

	const vk::VkDeviceSize dataSize = m_indexes.size() * sizeof(deUint32);
	m_indexBuffer = Buffer::createAndAlloc(	m_vk, m_context.getDevice(),
											BufferCreateInfo(dataSize,
															 vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
											m_context.getDefaultAllocator(),
											vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(m_indexBuffer->getBoundMemory().getHostPtr());

	deMemcpy(ptr, &m_indexes[0], static_cast<size_t>(dataSize));
	vk::flushAlloc(m_vk, m_context.getDevice(), m_indexBuffer->getBoundMemory());

	const vk::VkDeviceSize vertexBufferOffset = 0;
	const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();
	const vk::VkBuffer indexBuffer = m_indexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
	m_vk.cmdBindIndexBuffer(*m_cmdBuffer, indexBuffer, 0, vk::VK_INDEX_TYPE_UINT32);
	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 4, 2, m_vertexOffset, 2);
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_vk.cmdDrawIndexed(*m_cmdBuffer, 4, 4, 2, m_vertexOffset, 2);
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		case vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		case vk::VK_PRIMITIVE_TOPOLOGY_LAST:
			DE_FATAL("Topology not implemented");
			break;
		default:
			DE_FATAL("Unknown topology");
			break;
	}

#ifndef CTS_USES_VULKANSC
	if (m_groupParams->useDynamicRendering)
		endDynamicRender(*m_cmdBuffer);
	else
		endLegacyRender(*m_cmdBuffer);
#else
	endLegacyRender(*m_cmdBuffer);
#endif // CTS_USES_VULKANSC

	endCommandBuffer(m_vk, *m_cmdBuffer);

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	// Validation
	VK_CHECK(m_vk.queueWaitIdle(queue));

	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
	referenceFrame.allocLevel(0);

	const deInt32 frameWidth = referenceFrame.getWidth();
	const deInt32 frameHeight = referenceFrame.getHeight();

	tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	ReferenceImageInstancedCoordinates refInstancedCoords;

	for (int y = 0; y < frameHeight; y++)
	{
		const float yCoord = (float)(y / (0.5*frameHeight)) - 1.0f;

		for (int x = 0; x < frameWidth; x++)
		{
			const float xCoord = (float)(x / (0.5*frameWidth)) - 1.0f;

			if ((yCoord >= refInstancedCoords.bottom	&&
				 yCoord <= refInstancedCoords.top		&&
				 xCoord >= refInstancedCoords.left		&&
				 xCoord <= refInstancedCoords.right))
				referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
		}
	}

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
		referenceFrame.getLevel(0), renderedFrame, 0.05f,
		tcu::COMPARE_LOG_RESULT)) {
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void checkSupport(Context& context, DrawIndexed::TestSpec testSpec)
{
	if (testSpec.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

}	// anonymous

DrawIndexedTests::DrawIndexedTests (tcu::TestContext &testCtx, const SharedGroupParams groupParams)
	: TestCaseGroup		(testCtx, "indexed_draw", "drawing indexed geometry")
	, m_groupParams		(groupParams)
{
	/* Left blank on purpose */
}

DrawIndexedTests::~DrawIndexedTests (void) {}

void DrawIndexedTests::init (void)
{
	const struct
	{
		const vk::VkPrimitiveTopology		topology;
		const char*							nameSuffix;
		const char*							descSuffix;
	} TopologyCases[] =
	{
		{ vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	"triangle_list",	"triangle list" },
		{ vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	"triangle_strip",	"triangle strip" },
	};

	const struct
	{
		const int		offset;
		const char*		nameSuffix;
		const char*		descSuffix;
	} OffsetCases[] =
	{
		{ VERTEX_OFFSET_DEFAULT,	"",							""														},
		{ VERTEX_OFFSET_MINUS_ONE,	"_offset_minus_one",		" using -1 as the vertex offset"						},
		{ VERTEX_OFFSET_NEGATIVE,	"_offset_negative_large",	" using a large negative number as the vertex offset"	},
	};

	for (const auto& offsetCase : OffsetCases)
	{
		for (const auto& topologyCase : TopologyCases)
		{
			{
				DrawIndexed::TestSpec testSpec
				(
					{
						{ glu::SHADERTYPE_VERTEX, "vulkan/draw/VertexFetch.vert" },
						{ glu::SHADERTYPE_FRAGMENT, "vulkan/draw/VertexFetch.frag" }
					},
					topologyCase.topology,
					m_groupParams,
					offsetCase.offset
				);

				const auto testName = std::string("draw_indexed_") + topologyCase.nameSuffix + offsetCase.nameSuffix;
				const auto testDesc = std::string("Draws indexed ") + topologyCase.descSuffix + offsetCase.descSuffix;

				addChild(new InstanceFactory<DrawIndexed, FunctionSupport1<DrawIndexed::TestSpec> >
					(m_testCtx, testName, testDesc, testSpec, FunctionSupport1<DrawIndexed::TestSpec>::Args(checkSupport, testSpec)));
			}
			{
				DrawInstancedIndexed::TestSpec testSpec
				(
					{
						{ glu::SHADERTYPE_VERTEX, "vulkan/draw/VertexFetchInstancedFirstInstance.vert" },
						{ glu::SHADERTYPE_FRAGMENT, "vulkan/draw/VertexFetch.frag" }
					},
					topologyCase.topology,
					m_groupParams,
					VERTEX_OFFSET_NEGATIVE
				);

				const auto testName = std::string("draw_instanced_indexed_") + topologyCase.nameSuffix + offsetCase.nameSuffix;
				const auto testDesc = std::string("Draws instanced indexed ") + topologyCase.descSuffix + offsetCase.descSuffix;

				addChild(new InstanceFactory<DrawInstancedIndexed, FunctionSupport1<DrawInstancedIndexed::TestSpec> >
					(m_testCtx, testName, testDesc, testSpec, FunctionSupport1<DrawInstancedIndexed::TestSpec>::Args(checkSupport, testSpec)));
			}
		}
	}
}

}	// DrawTests
}	// vkt
