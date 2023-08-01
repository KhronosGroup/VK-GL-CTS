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

struct TestSpec2 : TestSpecBase
{
	const int32_t			vertexOffset;
	const vk::VkDeviceSize	bindIndexBufferOffset;
	const vk::VkDeviceSize	memoryBindOffset;
	bool	useMaintenance5Ext;

	TestSpec2 (const ShaderMap& shaders_,
			  vk::VkPrimitiveTopology topology_,
			  SharedGroupParams groupParams_,
			  int32_t vertexOffset_,
			  vk::VkDeviceSize bindIndexBufferOffset_,
			  vk::VkDeviceSize memoryBindOffset_,
			  bool useMaintenance5Ext_)
		: TestSpecBase			{shaders_, topology_, groupParams_}
		, vertexOffset			(vertexOffset_)
		, bindIndexBufferOffset	(bindIndexBufferOffset_)
		, memoryBindOffset		(memoryBindOffset_)
		, useMaintenance5Ext(useMaintenance5Ext_)
	{
	}
};

class DrawIndexed : public DrawTestsBaseClass
{
public:
	typedef		TestSpec2	TestSpec;

								DrawIndexed				(Context				&context,
														 TestSpec				testSpec);
	virtual		tcu::TestStatus iterate					(void);
protected:
				void			cmdBindIndexBufferImpl	(vk::VkCommandBuffer	commandBuffer,
														 vk::VkBuffer			indexBuffer,
														 vk::VkDeviceSize		offset,
														 vk::VkDeviceSize		size,
														 vk::VkIndexType		indexType);
	std::vector<deUint32>		m_indexes;
	de::SharedPtr<Buffer>		m_indexBuffer;
	const TestSpec				m_testSpec;
};

class DrawInstancedIndexed : public DrawIndexed
{
public:
								DrawInstancedIndexed	(Context &context, TestSpec testSpec);
	virtual		tcu::TestStatus	iterate					(void);
};

DrawIndexed::DrawIndexed (Context &context, TestSpec testSpec)
	: DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT], testSpec.groupParams, testSpec.topology)
	, m_testSpec(testSpec)
{
	// When using a positive vertex offset, the strategy is:
	// - Storing vertices with that offset in the vertex buffer.
	// - Using indices normally as if they were stored at the start of the buffer.
	//
	// When using a negative vertex offset, the strategy is:
	// - Store vertices at the start of the vertex buffer.
	// - Increase indices by abs(offset) so when substracting it, it results in the regular positions.

	const uint32_t indexOffset = (testSpec.vertexOffset < 0 ? static_cast<uint32_t>(-testSpec.vertexOffset) : 0u);
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
	for (int unusedIdx = 0; unusedIdx < testSpec.vertexOffset; unusedIdx++)
	{
		m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
	}

	int vertexIndex = (testSpec.vertexOffset >= 0 ? testSpec.vertexOffset : 0);

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

void DrawIndexed::cmdBindIndexBufferImpl(vk::VkCommandBuffer	commandBuffer,
										 vk::VkBuffer			indexBuffer,
										 vk::VkDeviceSize		offset,
										 vk::VkDeviceSize		dataSize,
										 vk::VkIndexType		indexType)
{
#ifndef CTS_USES_VULKANSC
	if (m_testSpec.useMaintenance5Ext)
		m_vk.cmdBindIndexBuffer2KHR(commandBuffer, indexBuffer, offset, dataSize, indexType);
	else
#endif
	{
		DE_UNREF(dataSize);
		m_vk.cmdBindIndexBuffer(commandBuffer, indexBuffer, offset, indexType);
	}
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
	const auto				dataSize	= static_cast<vk::VkDeviceSize>(de::dataSize(m_indexes));
	const auto				bufferSize	= dataSize + m_testSpec.bindIndexBufferOffset;
	vk::SimpleAllocator		allocator	(m_vk, device, memProps, vk::SimpleAllocator::OptionalOffsetParams({ atomSize, m_testSpec.memoryBindOffset }));

	m_indexBuffer = Buffer::createAndAlloc(	m_vk, device,
											BufferCreateInfo(bufferSize,
															 vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
											allocator,
											vk::MemoryRequirement::HostVisible);

	uint8_t* ptr = reinterpret_cast<uint8_t*>(m_indexBuffer->getBoundMemory().getHostPtr());

	deMemset(ptr, 0xFF, static_cast<size_t>(m_testSpec.bindIndexBufferOffset));
	deMemcpy(ptr + m_testSpec.bindIndexBufferOffset, de::dataOrNull(m_indexes), de::dataSize(m_indexes));
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
		cmdBindIndexBufferImpl(*m_secCmdBuffer, indexBuffer, m_testSpec.bindIndexBufferOffset, dataSize, vk::VK_INDEX_TYPE_UINT32);
		m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDrawIndexed(*m_secCmdBuffer, 6, 1, 2, m_testSpec.vertexOffset, 0);

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
		cmdBindIndexBufferImpl(*m_cmdBuffer, indexBuffer, m_testSpec.bindIndexBufferOffset, dataSize, vk::VK_INDEX_TYPE_UINT32);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 1, 2, m_testSpec.vertexOffset, 0);

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
		cmdBindIndexBufferImpl(*m_cmdBuffer, indexBuffer, m_testSpec.bindIndexBufferOffset, dataSize, vk::VK_INDEX_TYPE_UINT32);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 1, 2, m_testSpec.vertexOffset, 0);

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
	const auto				dataSize	= static_cast<vk::VkDeviceSize>(de::dataSize(m_indexes));
	const vk::VkDeviceSize	bufferSize	= dataSize + m_testSpec.bindIndexBufferOffset;
	const auto				atomSize	= m_context.getDeviceProperties().limits.nonCoherentAtomSize;
	vk::SimpleAllocator		allocator	(m_vk, device, memProps, vk::SimpleAllocator::OptionalOffsetParams({ atomSize, m_testSpec.memoryBindOffset }));

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

	deMemset(ptr, 0xFF, static_cast<size_t>(m_testSpec.bindIndexBufferOffset));
	deMemcpy(ptr + m_testSpec.bindIndexBufferOffset, de::dataOrNull(m_indexes), de::dataSize(m_indexes));
	vk::flushAlloc(m_vk, device, m_indexBuffer->getBoundMemory());

	const vk::VkDeviceSize	vertexBufferOffset	= 0;
	const vk::VkBuffer		vertexBuffer		= m_vertexBuffer->object();
	const vk::VkBuffer		indexBuffer			= m_indexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
	cmdBindIndexBufferImpl(*m_cmdBuffer, indexBuffer, m_testSpec.bindIndexBufferOffset, dataSize, vk::VK_INDEX_TYPE_UINT32);
	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 4, 2, m_testSpec.vertexOffset, 2);
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_vk.cmdDrawIndexed(*m_cmdBuffer, 4, 4, 2, m_testSpec.vertexOffset, 2);
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

#ifndef CTS_USES_VULKANSC
	if (testSpec.useMaintenance5Ext)
		context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
#endif
}

}	// anonymous

DrawIndexedTests::DrawIndexedTests (tcu::TestContext &testCtx, const SharedGroupParams groupParams)
	: TestCaseGroup		(testCtx, "indexed_draw", "drawing indexed geometry")
	, m_groupParams		(groupParams)
{
	/* Left blank on purpose */
}

DrawIndexedTests::~DrawIndexedTests (void) {}


void DrawIndexedTests::init	(void)
{
	init(false);
#ifndef CTS_USES_VULKANSC
	init(true);
#endif
}

void DrawIndexedTests::init (bool useMaintenance5Ext)
{
	std::string	maintenance5ExtNameSuffix;
	std::string	maintenance5ExtDescSuffix;

	if (useMaintenance5Ext)
	{
		maintenance5ExtNameSuffix = "_maintenance_5";
		maintenance5ExtDescSuffix = " using vkCmdBindIndexBuffer2KHR() introduced in VK_KHR_maintenance5";
	}

	const struct {
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
							useMaintenance5Ext
						);

						const auto testName = std::string("draw_indexed_") + topologyCase.nameSuffix + offsetCase.nameSuffix + indexBindOffsetCase.nameSuffix + memoryBindOffsetCase.nameSuffix + maintenance5ExtNameSuffix;
						const auto testDesc = std::string("Draws indexed ") + topologyCase.descSuffix + offsetCase.descSuffix + indexBindOffsetCase.descSuffix + memoryBindOffsetCase.descSuffix + maintenance5ExtDescSuffix;

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
							useMaintenance5Ext
						);

						const auto testName = std::string("draw_instanced_indexed_") + topologyCase.nameSuffix + offsetCase.nameSuffix + indexBindOffsetCase.nameSuffix + memoryBindOffsetCase.nameSuffix + maintenance5ExtNameSuffix;
						const auto testDesc = std::string("Draws instanced indexed ") + topologyCase.descSuffix + offsetCase.descSuffix + indexBindOffsetCase.descSuffix + memoryBindOffsetCase.descSuffix + maintenance5ExtDescSuffix;

						addChild(new InstanceFactory<DrawInstancedIndexed, FunctionSupport1<DrawInstancedIndexed::TestSpec> >
							(m_testCtx, testName, testDesc, testSpec, FunctionSupport1<DrawInstancedIndexed::TestSpec>::Args(checkSupport, testSpec)));
					}
				}
			}
		}
	}
}

}	// DrawTests
}	// vkt
