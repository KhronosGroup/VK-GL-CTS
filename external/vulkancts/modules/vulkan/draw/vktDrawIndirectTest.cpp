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
 * \brief Draw Indirect Test
 *//*--------------------------------------------------------------------*/

#include "vktDrawIndirectTest.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"

#include "vktDrawBaseClass.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"

namespace vkt
{
namespace Draw
{
namespace
{
struct JunkData
{
	JunkData()
		: varA	(0xcd)
		, varB	(0xcd)
	{
	}
	const deUint16	varA;
	const deUint32	varB;
};

class IndirectDraw : public DrawTestsBaseClass
{
public:
								IndirectDraw	(Context &context, ShaderMap shaders, vk::VkPrimitiveTopology topology);
	virtual	tcu::TestStatus		iterate			(void);
private:
	de::SharedPtr<Buffer>					m_indirectBuffer;
	std::vector<vk::VkDrawIndirectCommand>	m_indirectDrawCmd;
	vk::VkDeviceSize						m_offsetInBuffer;
	deUint32								m_strideInBuffer;
	deUint32								m_drawCount;
	JunkData								m_junkData;
protected:
	deBool									m_isMultiDrawEnabled;
	deUint32								m_drawIndirectMaxCount;
};

struct FirtsInstanceSupported
{
	static deUint32 getFirstInstance	(void)											{ return 2; }
	static bool		isTestSupported		(const vk::VkPhysicalDeviceFeatures& features)	{ return features.drawIndirectFirstInstance == vk::VK_TRUE; }
};

struct FirtsInstanceNotSupported
{
	static deUint32 getFirstInstance	(void)											{ return 0; }
	static bool		isTestSupported		(const vk::VkPhysicalDeviceFeatures&)			{ return true; }
};

template<class FirstInstanceSupport>
class IndirectDrawInstanced : public IndirectDraw
{
public:
								IndirectDrawInstanced	(Context &context, ShaderMap shaders, vk::VkPrimitiveTopology topology);
	virtual tcu::TestStatus		iterate					(void);
private:
	de::SharedPtr<Buffer>					m_indirectBuffer;
	std::vector<vk::VkDrawIndirectCommand>	m_indirectDrawCmd;
	vk::VkDeviceSize						m_offsetInBuffer;
	deUint32								m_strideInBuffer;
	deUint32								m_drawCount;
	JunkData								m_junkData;
};

IndirectDraw::IndirectDraw (Context &context, ShaderMap shaders, vk::VkPrimitiveTopology topology)
		: DrawTestsBaseClass(context, shaders[glu::SHADERTYPE_VERTEX], shaders[glu::SHADERTYPE_FRAGMENT])
{
	m_topology = topology;

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 1.0f,	-1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-1.0f,	 1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 0.3f,	-0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	 0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-0.3f,	 0.3f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(	-1.0f,	 1.0f,	1.0f,	1.0f), tcu::RGBA::blue().toVec()));
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_data.push_back(PositionColorVertex(tcu::Vec4( 1.0f,	-1.0f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f,	 1.0f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(-0.3f,	 0.0f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4( 0.3f,	 0.0f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(-0.3f,	-0.3f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4( 0.3f,	-0.3f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(-0.3f,	 0.3f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4( 0.3f,	 0.3f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(-0.3f,	 0.0f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4( 0.3f,	 0.0f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
			m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f,	 1.0f,	 1.0f,	 1.0f),	 tcu::RGBA::blue().toVec()));
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
	initialize();

	// Check device for multidraw support:
	if (m_context.getDeviceFeatures().multiDrawIndirect)
		m_isMultiDrawEnabled = true;	
	else
		m_isMultiDrawEnabled = false;
	
	m_drawIndirectMaxCount = m_context.getDeviceProperties().limits.maxDrawIndirectCount;

}

tcu::TestStatus IndirectDraw::iterate (void)
{
	tcu::TestLog &log = m_context.getTestContext().getLog();
	const vk::VkQueue queue = m_context.getUniversalQueue();

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			vk::VkDrawIndirectCommand drawCommands[] =
			{
				{
					3,		//vertexCount
					1,		//instanceCount
					2,		//firstVertex
					0		//firstInstance
				},
				{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deUint32)-9 }, // junk (stride)
				{
					3,		//vertexCount
					1,		//instanceCount
					5,		//firstVertex
					0		//firstInstance
				}
			};
			m_indirectDrawCmd.push_back(drawCommands[0]);
			m_indirectDrawCmd.push_back(drawCommands[1]);
			m_indirectDrawCmd.push_back(drawCommands[2]);
			break;
		}
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			vk::VkDrawIndirectCommand drawCommands[] =
			{
				{
					4,		//vertexCount
					1,		//instanceCount
					2,		//firstVertex
					0		//firstInstance
				},
				{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deUint32)-9 }, // junk (stride)
				{
					4,		//vertexCount
					1,		//instanceCount
					6,		//firstVertex
					0		//firstInstance
				}
			};
			m_indirectDrawCmd.push_back(drawCommands[0]);
			m_indirectDrawCmd.push_back(drawCommands[1]);
			m_indirectDrawCmd.push_back(drawCommands[2]);
			break;
		}
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

	m_strideInBuffer	= 2 * (deUint32)sizeof(m_indirectDrawCmd[0]);
	m_drawCount			= 2;
	m_offsetInBuffer	= sizeof(m_junkData);

	beginRenderPass();

	const vk::VkDeviceSize vertexBufferOffset	= 0;
	const vk::VkBuffer vertexBuffer				= m_vertexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	const vk::VkDeviceSize dataSize = m_indirectDrawCmd.size()*sizeof(m_indirectDrawCmd[0]);

	m_indirectBuffer = Buffer::createAndAlloc(	m_vk,
												m_context.getDevice(),
												BufferCreateInfo(dataSize,
																 vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
												m_context.getDefaultAllocator(),
												vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(m_indirectBuffer->getBoundMemory().getHostPtr());

	deMemcpy(ptr, &m_junkData, static_cast<size_t>(m_offsetInBuffer));
	deMemcpy((ptr+m_offsetInBuffer), &m_indirectDrawCmd[0], static_cast<size_t>(dataSize));

	vk::flushMappedMemoryRange(m_vk,
							   m_context.getDevice(),
							   m_indirectBuffer->getBoundMemory().getMemory(),
							   m_indirectBuffer->getBoundMemory().getOffset(),
							   dataSize);

	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
		m_vk.cmdDrawIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer, m_drawCount, m_strideInBuffer);
	else
	{
		for(deUint32 drawNdx = 0; drawNdx < m_drawCount; drawNdx++){
			m_vk.cmdDrawIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer, 1, m_strideInBuffer);
		}
	}
	m_vk.cmdEndRenderPass(*m_cmdBuffer);
	m_vk.endCommandBuffer(*m_cmdBuffer);

	vk::VkSubmitInfo submitInfo =
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		0,										// deUint32					waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*		pWaitSemaphores;
		(const vk::VkPipelineStageFlags*)DE_NULL,
		1,										// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),					// const VkCommandBuffer*	pCommandBuffers;
		0,										// deUint32					signalSemaphoreCount;
		DE_NULL								// const VkSemaphore*		pSignalSemaphores;
	};
	VK_CHECK(m_vk.queueSubmit(queue, 1, &submitInfo, DE_NULL));

	VK_CHECK(m_vk.queueWaitIdle(queue));

	// Validation
	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH), (int)(0.5 + HEIGHT));
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

			if ((yCoord >= refCoords.bottom	&&
				 yCoord <= refCoords.top	&&
				 xCoord >= refCoords.left	&&
				 xCoord <= refCoords.right))
				referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
		}
	}

	const vk::VkOffset3D zeroOffset					= { 0, 0, 0 };
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

template<class FirstInstanceSupport>
IndirectDrawInstanced<FirstInstanceSupport>::IndirectDrawInstanced (Context &context, ShaderMap shaders, vk::VkPrimitiveTopology topology)
	: IndirectDraw	(context, shaders, topology)
{
	if (!FirstInstanceSupport::isTestSupported(m_context.getDeviceFeatures()))
	{
		throw tcu::NotSupportedError("Required 'drawIndirectFirstInstance' feature is not supported");
	}
}

template<class FirstInstanceSupport>
tcu::TestStatus IndirectDrawInstanced<FirstInstanceSupport>::iterate (void)
{
	tcu::TestLog &log = m_context.getTestContext().getLog();
	const vk::VkQueue queue = m_context.getUniversalQueue();

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			vk::VkDrawIndirectCommand drawCmd[] =
			{
				{
					3,											//vertexCount
					4,											//instanceCount
					2,											//firstVertex
					FirstInstanceSupport::getFirstInstance()	//firstInstance
				},
				{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deUint32)-9 }, // junk (stride)
				{
					3,											//vertexCount
					4,											//instanceCount
					5,											//firstVertex
					FirstInstanceSupport::getFirstInstance()	//firstInstance
				}
			};
			m_indirectDrawCmd.push_back(drawCmd[0]);
			m_indirectDrawCmd.push_back(drawCmd[1]);
			m_indirectDrawCmd.push_back(drawCmd[2]);
			break;
		}
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			vk::VkDrawIndirectCommand drawCmd[] =
			{
				{
					4,											//vertexCount
					4,											//instanceCount
					2,											//firstVertex
					FirstInstanceSupport::getFirstInstance()	//firstInstance
				},
				{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deUint32)-9 },
				{
					4,											//vertexCount
					4,											//instanceCount
					6,											//firstVertex
					FirstInstanceSupport::getFirstInstance()	//firstInstance
				}
			};
			m_indirectDrawCmd.push_back(drawCmd[0]);
			m_indirectDrawCmd.push_back(drawCmd[1]);
			m_indirectDrawCmd.push_back(drawCmd[2]);
			break;
		}
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

	m_strideInBuffer	= 2 * (deUint32)sizeof(m_indirectDrawCmd[0]);
	m_drawCount			= 2;
	m_offsetInBuffer	= sizeof(m_junkData);

	beginRenderPass();

	const vk::VkDeviceSize vertexBufferOffset = 0;
	const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	const vk::VkDeviceSize dataSize = m_indirectDrawCmd.size()*sizeof(m_indirectDrawCmd[0]);

	m_indirectBuffer = Buffer::createAndAlloc(	m_vk,
												m_context.getDevice(),
												BufferCreateInfo(dataSize,
																 vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
												m_context.getDefaultAllocator(),
												vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(m_indirectBuffer->getBoundMemory().getHostPtr());

	deMemcpy(ptr, &m_junkData, static_cast<size_t>(m_offsetInBuffer));
	deMemcpy((ptr + m_offsetInBuffer), &m_indirectDrawCmd[0], static_cast<size_t>(dataSize));

	vk::flushMappedMemoryRange(m_vk,
							   m_context.getDevice(),
							   m_indirectBuffer->getBoundMemory().getMemory(),
							   m_indirectBuffer->getBoundMemory().getOffset(),
							   dataSize);

	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
		m_vk.cmdDrawIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer, m_drawCount, m_strideInBuffer);
	else
	{
		for (deUint32 drawNdx = 0; drawNdx < m_drawCount; drawNdx++){
			m_vk.cmdDrawIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer, 1, m_strideInBuffer);
		}
	}
	m_vk.cmdEndRenderPass(*m_cmdBuffer);
	m_vk.endCommandBuffer(*m_cmdBuffer);

	vk::VkSubmitInfo submitInfo =
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		0,										// deUint32					waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*		pWaitSemaphores;
		(const vk::VkPipelineStageFlags*)DE_NULL,
		1,										// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),					// const VkCommandBuffer*	pCommandBuffers;
		0,										// deUint32					signalSemaphoreCount;
		DE_NULL								// const VkSemaphore*		pSignalSemaphores;
	};
	VK_CHECK(m_vk.queueSubmit(queue, 1, &submitInfo, DE_NULL));

	VK_CHECK(m_vk.queueWaitIdle(queue));

	// Validation
	VK_CHECK(m_vk.queueWaitIdle(queue));

	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH), (int)(0.5 + HEIGHT));

	referenceFrame.allocLevel(0);

	const deInt32 frameWidth	= referenceFrame.getWidth();
	const deInt32 frameHeight	= referenceFrame.getHeight();

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

}	// anonymous

IndirectDrawTests::IndirectDrawTests (tcu::TestContext &testCtx)
	: TestCaseGroup(testCtx, "indirect_draw", "indirect drawing simple geometry")
{
	/* Left blank on purpose */
}

IndirectDrawTests::~IndirectDrawTests (void) {}


void IndirectDrawTests::init (void)
{
	ShaderMap shaderPaths;
	shaderPaths[glu::SHADERTYPE_VERTEX]		= "vulkan/draw/VertexFetch.vert";
	shaderPaths[glu::SHADERTYPE_FRAGMENT]	= "vulkan/draw/VertexFetch.frag";

	tcu::TestCaseGroup* indirectDrawGroup	= new tcu::TestCaseGroup(m_testCtx, "indirect_draw", "Draws geometry");
	{
		indirectDrawGroup->addChild(new InstanceFactory<IndirectDraw>(m_testCtx, "triangle_list", "Draws triangle list", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
		indirectDrawGroup->addChild(new InstanceFactory<IndirectDraw>(m_testCtx, "triangle_strip", "Draws triangle strip", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP));
	}
	addChild(indirectDrawGroup);


	tcu::TestCaseGroup* indirectDrawInstancedGroup	= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_instanced", "Draws an instanced geometry");
	{
		tcu::TestCaseGroup*	noFirstInstanceGroup	= new tcu::TestCaseGroup(m_testCtx, "no_first_instance", "Use 0 as firstInstance");
		{
			shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetchInstanced.vert";

			noFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirtsInstanceNotSupported> >(m_testCtx, "triangle_list", "Draws an instanced triangle list", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
			noFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirtsInstanceNotSupported> >(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP));
		}		
		indirectDrawInstancedGroup->addChild(noFirstInstanceGroup);

		tcu::TestCaseGroup*	firstInstanceGroup		= new tcu::TestCaseGroup(m_testCtx, "first_instance", "Use drawIndirectFirstInstance optional feature");
		{
			shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetchInstancedFirstInstance.vert";

			firstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirtsInstanceSupported> >(m_testCtx, "triangle_list", "Draws an instanced triangle list", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
			firstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirtsInstanceSupported> >(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", shaderPaths, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP));
		}
		indirectDrawInstancedGroup->addChild(firstInstanceGroup);
	}
	addChild(indirectDrawInstancedGroup);
}

}	// DrawTests
}	// vkt