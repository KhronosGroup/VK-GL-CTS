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

#include "tcuTestCase.hpp"
#include "tcuVectorUtil.hpp"
#include "rrRenderer.hpp"

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

enum class IndexBindOffset
{
	DEFAULT		= 0,
	POSITIVE	= 16,	// Must be aligned to the index data type size.
};

enum class MemoryBindOffset
{
	DEFAULT		= 0,
	POSITIVE	= 16,	// Will be rounded up to the alignment requirement.
};

enum TestType
{
	TEST_TYPE_NON_MAINTENANCE_6 = 0,
	TEST_TYPE_MAINTENANCE6_INDEXED,
	TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT,
//	TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT_KHR,
	TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT,
#ifndef CTS_USES_VULKANSC
	TEST_TYPE_MAINTENANCE6_MULTI_INDEXED_EXT,
#endif
	TEST_TYPE_LAST
};

class DrawIndexed : public DrawTestsBaseClass
{
public:
	struct TestSpec : public TestSpecBase
	{
		const int32_t			vertexOffset;
		const vk::VkDeviceSize	bindIndexBufferOffset;
		const vk::VkDeviceSize	memoryBindOffset;

		const TestType			testType;
		const bool				nullDescriptor;
		const bool				bindIndexBuffer2;

		TestSpec (const ShaderMap& shaders_,
				  vk::VkPrimitiveTopology topology_,
				  SharedGroupParams groupParams_,
				  int32_t vertexOffset_,
				  vk::VkDeviceSize bindIndexBufferOffset_,
				  vk::VkDeviceSize memoryBindOffset_,
				  TestType testType_,
				  bool nullDescriptor_,
				  bool bindIndexBuffer2_
			)
			: TestSpecBase			{shaders_, topology_, groupParams_}
			, vertexOffset			(vertexOffset_)
			, bindIndexBufferOffset	(bindIndexBufferOffset_)
			, memoryBindOffset		(memoryBindOffset_)
			, testType				(testType_)
			, nullDescriptor		(nullDescriptor_)
			, bindIndexBuffer2		(bindIndexBuffer2_)
		{
		}
	};

								DrawIndexed				(Context &context, TestSpec testSpec);
	virtual		tcu::TestStatus iterate					(void);
protected:
	std::vector<deUint32>		m_indexes;
	de::SharedPtr<Buffer>		m_indexBuffer;
	const int32_t				m_vertexOffset;
	const vk::VkDeviceSize		m_bindIndexBufferOffset;
	const vk::VkDeviceSize		m_memoryBindOffset;

	const TestType				m_testType;
	const bool					m_nullDescriptor;
	const bool					m_bindIndexBuffer2;
};

class DrawInstancedIndexed : public DrawIndexed
{
public:
								DrawInstancedIndexed	(Context &context, TestSpec testSpec);
	virtual		tcu::TestStatus	iterate					(void);
};

DrawIndexed::DrawIndexed(Context& context, TestSpec testSpec)
	: DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT], testSpec.groupParams, testSpec.topology)
	, m_vertexOffset(testSpec.vertexOffset)
	, m_bindIndexBufferOffset(testSpec.bindIndexBufferOffset)
	, m_memoryBindOffset(testSpec.memoryBindOffset)
	, m_testType(testSpec.testType)
	, m_nullDescriptor(testSpec.nullDescriptor)
	, m_bindIndexBuffer2(testSpec.bindIndexBuffer2)
{
	if (testSpec.testType == TEST_TYPE_NON_MAINTENANCE_6)
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
	tcu::TestLog&			log			= m_context.getTestContext().getLog();
	const auto&				vki			= m_context.getInstanceInterface();
	const auto				physDev		= m_context.getPhysicalDevice();
	const vk::VkQueue		queue		= m_context.getUniversalQueue();
	const vk::VkDevice		device		= m_context.getDevice();
	const auto				memProps	= vk::getPhysicalDeviceMemoryProperties(vki, physDev);
	const auto				atomSize	= m_context.getDeviceProperties().limits.nonCoherentAtomSize;
	const vk::VkDeviceSize	bufferSize	= de::dataSize(m_indexes) + m_bindIndexBufferOffset;
	vk::SimpleAllocator		allocator	(m_vk, device, memProps, vk::SimpleAllocator::OptionalOffsetParams({ atomSize, m_memoryBindOffset }));

	m_indexBuffer = Buffer::createAndAlloc(	m_vk, device,
											BufferCreateInfo(bufferSize,
															 vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
											allocator,
											vk::MemoryRequirement::HostVisible);

	uint8_t* ptr = reinterpret_cast<uint8_t*>(m_indexBuffer->getBoundMemory().getHostPtr());

	deMemset(ptr, 0xFF, static_cast<size_t>(m_bindIndexBufferOffset));
	deMemcpy(ptr + m_bindIndexBufferOffset, de::dataOrNull(m_indexes), de::dataSize(m_indexes));
	vk::flushAlloc(m_vk, device, m_indexBuffer->getBoundMemory());

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
		m_vk.cmdBindIndexBuffer(*m_secCmdBuffer, indexBuffer, m_bindIndexBufferOffset, vk::VK_INDEX_TYPE_UINT32);
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
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, indexBuffer, m_bindIndexBufferOffset, vk::VK_INDEX_TYPE_UINT32);
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
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, indexBuffer, m_bindIndexBufferOffset, vk::VK_INDEX_TYPE_UINT32);
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
	tcu::TestLog&			log			= m_context.getTestContext().getLog();
	const auto&				vki			= m_context.getInstanceInterface();
	const auto				physDev		= m_context.getPhysicalDevice();
	const vk::VkQueue		queue		= m_context.getUniversalQueue();
	const vk::VkDevice		device		= m_context.getDevice();
	const auto				memProps	= vk::getPhysicalDeviceMemoryProperties(vki, physDev);
	const vk::VkDeviceSize	bufferSize	= de::dataSize(m_indexes) + m_bindIndexBufferOffset;
	const auto				atomSize	= m_context.getDeviceProperties().limits.nonCoherentAtomSize;
	vk::SimpleAllocator		allocator	(m_vk, device, memProps, vk::SimpleAllocator::OptionalOffsetParams({ atomSize, m_memoryBindOffset }));

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

	m_indexBuffer = Buffer::createAndAlloc(	m_vk, device,
											BufferCreateInfo(bufferSize,
															 vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
											allocator,
											vk::MemoryRequirement::HostVisible);

	uint8_t* ptr = reinterpret_cast<uint8_t*>(m_indexBuffer->getBoundMemory().getHostPtr());

	deMemset(ptr, 0xFF, static_cast<size_t>(m_bindIndexBufferOffset));
	deMemcpy(ptr + m_bindIndexBufferOffset, de::dataOrNull(m_indexes), de::dataSize(m_indexes));
	vk::flushAlloc(m_vk, device, m_indexBuffer->getBoundMemory());

	const vk::VkDeviceSize	vertexBufferOffset	= 0;
	const vk::VkBuffer		vertexBuffer		= m_vertexBuffer->object();
	const vk::VkBuffer		indexBuffer			= m_indexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
	m_vk.cmdBindIndexBuffer(*m_cmdBuffer, indexBuffer, m_bindIndexBufferOffset, vk::VK_INDEX_TYPE_UINT32);
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

class DrawIndexedMaintenance6 : public DrawIndexed
{
public:
	DrawIndexedMaintenance6(Context& context, TestSpec testSpec);
	virtual		tcu::TestStatus	iterate(void);
};

DrawIndexedMaintenance6::DrawIndexedMaintenance6(Context& context, TestSpec testSpec)
	: DrawIndexed(context, testSpec)
{
}


// Reference renderer shaders
class PassthruVertShader : public rr::VertexShader
{
public:
	PassthruVertShader(void)
		: rr::VertexShader(2, 1)
	{
		m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
		m_inputs[1].type = rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices(const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			packets[packetNdx]->position = rr::readVertexAttribFloat(inputs[0],
				packets[packetNdx]->instanceNdx,
				packets[packetNdx]->vertexNdx);

			tcu::Vec4 color = rr::readVertexAttribFloat(inputs[1],
				packets[packetNdx]->instanceNdx,
				packets[packetNdx]->vertexNdx);

			packets[packetNdx]->outputs[0] = color;
		}
	}
};

class PassthruFragShader : public rr::FragmentShader
{
public:
	PassthruFragShader(void)
		: rr::FragmentShader(1, 1)
	{
		m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments(rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			rr::FragmentPacket& packet = packets[packetNdx];
			for (deUint32 fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
			{
				tcu::Vec4 color = rr::readVarying<float>(packet, context, 0, fragNdx);
				rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
			}
		}
	}
};

tcu::TestStatus DrawIndexedMaintenance6::iterate(void)
{
	tcu::TestLog& log = m_context.getTestContext().getLog();
	const auto& vki = m_context.getInstanceInterface();
	const auto				physDev = m_context.getPhysicalDevice();
	const vk::VkQueue		queue = m_context.getUniversalQueue();
	const vk::VkDevice		device = m_context.getDevice();
	const auto				memProps = vk::getPhysicalDeviceMemoryProperties(vki, physDev);
	const auto				atomSize = m_context.getDeviceProperties().limits.nonCoherentAtomSize;
	vk::SimpleAllocator		allocator(m_vk, device, memProps, vk::SimpleAllocator::OptionalOffsetParams({ atomSize, m_memoryBindOffset }));

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

	const uint32_t indexCount = m_nullDescriptor ? 3 : 0;

	const vk::VkDrawIndexedIndirectCommand drawParams =	{indexCount, 1, 0, 0, 0};

	const auto drawParamsBuffer = Buffer::createAndAlloc(m_vk, device,
		BufferCreateInfo(sizeof(vk::VkDrawIndexedIndirectCommand),
			vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
		allocator,
		vk::MemoryRequirement::HostVisible);

	uint8_t* ptr = reinterpret_cast<uint8_t*>(drawParamsBuffer->getBoundMemory().getHostPtr());

	deMemcpy(ptr, &drawParams, sizeof(vk::VkDrawIndexedIndirectCommand));
	vk::flushAlloc(m_vk, device, drawParamsBuffer->getBoundMemory());

	const auto countBuffer = Buffer::createAndAlloc(m_vk, device,
		BufferCreateInfo(sizeof(uint32_t),
			vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
		allocator,
		vk::MemoryRequirement::HostVisible);

	ptr = reinterpret_cast<uint8_t*>(countBuffer->getBoundMemory().getHostPtr());

	deMemset(ptr, 1, 1);
	vk::flushAlloc(m_vk, device, countBuffer->getBoundMemory());


	const vk::VkBuffer		vertexBuffer = m_vertexBuffer->object();
	const vk::VkDeviceSize	vertexBufferOffset = 0;

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

#ifndef CTS_USES_VULKANSC
	if (m_bindIndexBuffer2)
	{
		m_vk.cmdBindIndexBuffer2KHR(*m_cmdBuffer, VK_NULL_HANDLE, 0, 0, vk::VK_INDEX_TYPE_UINT32);
	}
	else
#endif
	{
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, VK_NULL_HANDLE, 0, vk::VK_INDEX_TYPE_UINT32);
	}

	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	switch (m_testType)
	{
		case TEST_TYPE_MAINTENANCE6_INDEXED:
		{
			m_vk.cmdDrawIndexed(*m_cmdBuffer, indexCount, 1, 0, 0, 0);

			break;
		}
		case TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT:
		{
			m_vk.cmdDrawIndexedIndirect(*m_cmdBuffer, drawParamsBuffer->object(), 0, 1, sizeof(vk::VkDrawIndexedIndirectCommand));

			break;
		}
		case TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT:
		{
			m_vk.cmdDrawIndexedIndirectCount(*m_cmdBuffer, drawParamsBuffer->object(), 0, countBuffer->object(), 0, 1, sizeof(vk::VkDrawIndexedIndirectCommand));

			break;
		}
#ifndef CTS_USES_VULKANSC
		case TEST_TYPE_MAINTENANCE6_MULTI_INDEXED_EXT:
		{
			const vk::VkMultiDrawIndexedInfoEXT indexInfo = { 0, indexCount, 0 };
			const int32_t vertexOffset = 0;

			m_vk.cmdDrawMultiIndexedEXT(*m_cmdBuffer, 1, &indexInfo, 1, 0, sizeof(vk::VkMultiDrawIndexedInfoEXT), &vertexOffset);

			break;
		}
#endif
		default:
		{
			DE_FATAL("Unknown test type");
			break;
		}
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

	tcu::TextureLevel refImage(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
	tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	if (m_nullDescriptor)
	{
		std::vector<tcu::Vec4>	vertices;
		std::vector<tcu::Vec4>	colors;

		// Draw just the first point
		vertices.push_back(m_data[0].position);
		colors.push_back(m_data[0].color);

		{
			const PassthruVertShader				vertShader;
			const PassthruFragShader				fragShader;
			const rr::Program						program(&vertShader, &fragShader);
			const rr::MultisamplePixelBufferAccess	colorBuffer = rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(refImage.getAccess());
			const rr::RenderTarget					renderTarget(colorBuffer);
			const rr::RenderState					renderState((rr::ViewportState(colorBuffer)), m_context.getDeviceProperties().limits.subPixelPrecisionBits);
			const rr::Renderer						renderer;

			const rr::VertexAttrib	vertexAttribs[] =
			{
				rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &vertices[0]),
				rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &colors[0])
			};
			renderer.draw(rr::DrawCommand(renderState,
				renderTarget,
				program,
				DE_LENGTH_OF_ARRAY(vertexAttribs),
				&vertexAttribs[0],
				rr::PrimitiveList(rr::PRIMITIVETYPE_POINTS, (deUint32)vertices.size(), 0)));
		}
	}

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!tcu::intThresholdPositionDeviationCompare(
		log, "Result", "Image comparison result", refImage.getAccess(), renderedFrame,
		tcu::UVec4(4u),					// color threshold
		tcu::IVec3(1, 1, 0),			// position deviation tolerance
		true,							// don't check the pixels at the boundary
		tcu::COMPARE_LOG_RESULT))
	{
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void checkSupport(Context& context, DrawIndexed::TestSpec testSpec)
{
	if (testSpec.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (testSpec.testType != TEST_TYPE_NON_MAINTENANCE_6)
	{
		context.requireDeviceFunctionality("VK_KHR_maintenance6");

		if (testSpec.nullDescriptor)
		{
			vk::VkPhysicalDeviceFeatures2				features2 = vk::initVulkanStructure();
			vk::VkPhysicalDeviceRobustness2FeaturesEXT	robustness2Features = vk::initVulkanStructure();

			features2.pNext = &robustness2Features;

			context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

			if (robustness2Features.nullDescriptor == VK_FALSE)
			{
				TCU_THROW(NotSupportedError, "robustness2 nullDescriptor is not supported");
			}

			DE_ASSERT(features2.features.robustBufferAccess);
		}

		if (testSpec.bindIndexBuffer2)
		{
			context.requireDeviceFunctionality("VK_KHR_maintenance5");
		}

#ifndef CTS_USES_VULKANSC
		if (testSpec.testType == TEST_TYPE_MAINTENANCE6_MULTI_INDEXED_EXT)
		{
			context.requireDeviceFunctionality("VK_EXT_multi_draw");
		}
#endif

		if (testSpec.testType == TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT)
		{
			context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
		}
	}
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

	const struct
	{
		IndexBindOffset	bindOffset;
		const char*		nameSuffix;
		const char*		descSuffix;
	} IndexBindOffsetCases[] =
	{
		{ IndexBindOffset::DEFAULT,		"",						""											},
		{ IndexBindOffset::POSITIVE,	"_with_bind_offset",	" and applying an index buffer bind offset"	},
	};

	const struct
	{
		MemoryBindOffset	memoryBindOffset;
		const char*			nameSuffix;
		const char*			descSuffix;
	} MemoryBindOffsetCases[] =
	{
		{ MemoryBindOffset::DEFAULT,	"",						""													},
		{ MemoryBindOffset::POSITIVE,	"_with_alloc_offset",	" and applying an extra memory allocation offset"	},
	};

	for (const auto& offsetCase : OffsetCases)
	{
		for (const auto& indexBindOffsetCase : IndexBindOffsetCases)
		{
			const auto indexBindOffset = static_cast<vk::VkDeviceSize>(indexBindOffsetCase.bindOffset);

			for (const auto& memoryBindOffsetCase : MemoryBindOffsetCases)
			{
				const auto memoryBindOffset = static_cast<vk::VkDeviceSize>(memoryBindOffsetCase.memoryBindOffset);

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
							offsetCase.offset,
							indexBindOffset,
							memoryBindOffset,
							TEST_TYPE_NON_MAINTENANCE_6,
							false,
							false
						);

						const auto testName = std::string("draw_indexed_") + topologyCase.nameSuffix + offsetCase.nameSuffix + indexBindOffsetCase.nameSuffix + memoryBindOffsetCase.nameSuffix;
						const auto testDesc = std::string("Draws indexed ") + topologyCase.descSuffix + offsetCase.descSuffix + indexBindOffsetCase.descSuffix + memoryBindOffsetCase.descSuffix;

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
							offsetCase.offset,
							indexBindOffset,
							memoryBindOffset,
							TEST_TYPE_NON_MAINTENANCE_6,
							false,
							false
						);

						const auto testName = std::string("draw_instanced_indexed_") + topologyCase.nameSuffix + offsetCase.nameSuffix + indexBindOffsetCase.nameSuffix + memoryBindOffsetCase.nameSuffix;
						const auto testDesc = std::string("Draws instanced indexed ") + topologyCase.descSuffix + offsetCase.descSuffix + indexBindOffsetCase.descSuffix + memoryBindOffsetCase.descSuffix;

						addChild(new InstanceFactory<DrawInstancedIndexed, FunctionSupport1<DrawInstancedIndexed::TestSpec> >
							(m_testCtx, testName, testDesc, testSpec, FunctionSupport1<DrawInstancedIndexed::TestSpec>::Args(checkSupport, testSpec)));
					}
				}
			}
		}
	}

	const struct
	{
		TestType	testType;
		const char* nameSuffix;
		const char* descSuffix;
	} Maintenance6Cases[] =
	{
		{ TEST_TYPE_MAINTENANCE6_INDEXED,	"",								""},
		{ TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT,	"indirect_",			""},
		{ TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT, "indirect_count_",	""},
#ifndef CTS_USES_VULKANSC
		{ TEST_TYPE_MAINTENANCE6_MULTI_INDEXED_EXT,	"multi_",				""},
#endif
	};

	for (const auto& maintenance6Case : Maintenance6Cases)
	{
		for (int m5 = 0; m5 < 2; m5++)
		{
			for (int null = 0; null < 2; null++)
			{
				DrawIndexedMaintenance6::TestSpec testSpec
				(
					{
						{ glu::SHADERTYPE_VERTEX, "vulkan/draw/VertexFetch.vert" },
						{ glu::SHADERTYPE_FRAGMENT, "vulkan/draw/VertexFetch.frag" }
					},
					vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
					m_groupParams,
					0,
					0,
					0,
					maintenance6Case.testType,
					null == 1,
					m5 == 1
					);

				const char* m5Suffix = m5 == 0 ? "" : "_bindindexbuffer2";
				const char* nullSuffix = null == 0 ? "" : "_nulldescriptor";

				const auto testName = std::string("draw_indexed_") + maintenance6Case.nameSuffix + std::string("maintenance6") + m5Suffix + nullSuffix;
				const auto testDesc = std::string("Draws indexed ") + maintenance6Case.descSuffix;

				addChild(new InstanceFactory<DrawIndexedMaintenance6, FunctionSupport1<DrawIndexed::TestSpec> >
					(m_testCtx, testName, testDesc, testSpec, FunctionSupport1<DrawIndexedMaintenance6::TestSpec>::Args(checkSupport, testSpec)));
			}
		}
	}
}

}	// DrawTests
}	// vkt
