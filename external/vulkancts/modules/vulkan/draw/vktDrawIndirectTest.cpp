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
#include "vkQueryUtil.hpp"

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
	VERTEX_OFFSET = 13
};

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

enum DrawType
{
	DRAW_TYPE_SEQUENTIAL,
	DRAW_TYPE_INDEXED,

	DRAWTYPE_LAST
};

enum class IndirectCountType
{
	NONE,
	BUFFER_LIMIT,
	PARAM_LIMIT,

	LAST
};

struct DrawTypedTestSpec : public TestSpecBase
{
	DrawTypedTestSpec()
		: testFirstInstanceNdx(false)
		, testIndirectCountExt(IndirectCountType::NONE)
	{};

	DrawType			drawType;
	bool				testFirstInstanceNdx;
	IndirectCountType	testIndirectCountExt;
};

class IndirectDraw : public DrawTestsBaseClass
{
public:
	typedef DrawTypedTestSpec	TestSpec;

								IndirectDraw	(Context &context, TestSpec testSpec);
	virtual	tcu::TestStatus		iterate			(void);

	template<typename T> void	addCommand		(const T&);

protected:
	void						setVertexBuffer						(void);
	void						setFirstInstanceVertexBuffer		(void);

	std::vector<char>			m_indirectBufferContents;
	de::SharedPtr<Buffer>		m_indirectBuffer;
	vk::VkDeviceSize			m_offsetInBuffer;
	deUint32					m_strideInBuffer;

	const IndirectCountType		m_testIndirectCountExt;
	de::SharedPtr<Buffer>		m_indirectCountBuffer;
	vk::VkDeviceSize			m_offsetInCountBuffer;
	const deUint32				m_indirectCountExtDrawPadding;

	deUint32					m_drawCount;
	JunkData					m_junkData;

	const DrawType				m_drawType;
	const bool					m_testFirstInstanceNdx;
	deBool						m_isMultiDrawEnabled;
	deUint32					m_drawIndirectMaxCount;

	de::SharedPtr<Buffer>		m_indexBuffer;
};

struct FirstInstanceSupported
{
	static deUint32 getFirstInstance	(void)											{ return 2; }
	static bool		isTestSupported		(const vk::VkPhysicalDeviceFeatures& features)	{ return features.drawIndirectFirstInstance == VK_TRUE; }
};

struct FirstInstanceNotSupported
{
	static deUint32 getFirstInstance	(void)											{ return 0; }
	static bool		isTestSupported		(const vk::VkPhysicalDeviceFeatures&)			{ return true; }
};

template<class FirstInstanceSupport>
class IndirectDrawInstanced : public IndirectDraw
{
public:
								IndirectDrawInstanced	(Context &context, TestSpec testSpec);
	virtual tcu::TestStatus		iterate					(void);
};

void IndirectDraw::setVertexBuffer (void)
{
	int refVertexIndex = 2;

	if (m_drawType == DRAW_TYPE_INDEXED)
	{
		for (int unusedIdx = 0; unusedIdx < VERTEX_OFFSET; unusedIdx++)
		{
			m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
		}
		refVertexIndex += VERTEX_OFFSET;
	}

	m_data.push_back(VertexElementData(tcu::Vec4( 1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
	m_data.push_back(VertexElementData(tcu::Vec4(-1.0f,  1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.0f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.0f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	-0.3f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.3f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.0f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refVertexIndex++));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.0f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refVertexIndex++));
			break;
		default:
			DE_FATAL("Unknown topology");
			break;
	}

	m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
}

void IndirectDraw::setFirstInstanceVertexBuffer (void)
{
	if (m_context.getDeviceFeatures().drawIndirectFirstInstance != VK_TRUE)
	{
		TCU_THROW(NotSupportedError, "Required 'drawIndirectFirstInstance' feature is not supported");
	}

	if (m_drawType == DRAW_TYPE_INDEXED)
	{
		for (int unusedIdx = 0; unusedIdx < VERTEX_OFFSET; unusedIdx++)
		{
			m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
		}
	}

	m_data.push_back(VertexElementData(tcu::Vec4( 1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
	m_data.push_back(VertexElementData(tcu::Vec4(-1.0f,  1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));

	switch (m_topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			int refInstanceIndex = 1;
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refInstanceIndex));

			refInstanceIndex = 0;
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refInstanceIndex));
			break;
		}
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			int refInstanceIndex = 1;
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.0f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.0f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	-0.3f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	-0.3f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refInstanceIndex));

			refInstanceIndex = 0;
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.3f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.3f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4(-0.3f,	 0.0f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refInstanceIndex));
			m_data.push_back(VertexElementData(tcu::Vec4( 0.3f,	 0.0f, 1.0f, 1.0f),	 tcu::RGBA::blue().toVec(), refInstanceIndex));
			break;
		}
		default:
			DE_FATAL("Unknown topology");
			break;
	}

	m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
}

IndirectDraw::IndirectDraw (Context &context, TestSpec testSpec)
	: DrawTestsBaseClass				(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT], testSpec.topology)
	, m_testIndirectCountExt			(testSpec.testIndirectCountExt)
	, m_indirectCountExtDrawPadding		(1u)
	, m_drawType						(testSpec.drawType)
	, m_testFirstInstanceNdx			(testSpec.testFirstInstanceNdx)
{
	if (m_testFirstInstanceNdx)
		setFirstInstanceVertexBuffer();
	else
		setVertexBuffer();

	initialize();

	if (testSpec.drawType == DRAW_TYPE_INDEXED)
	{
		const size_t indexBufferLength = m_data.size() - VERTEX_OFFSET;

		m_indexBuffer = Buffer::createAndAlloc(m_vk, m_context.getDevice(), BufferCreateInfo(sizeof(deUint32) * indexBufferLength, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT), m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);
		deUint32* indices = reinterpret_cast<deUint32*>(m_indexBuffer->getBoundMemory().getHostPtr());
		for (size_t i = 0; i < indexBufferLength; i++)
		{
			indices[i] = static_cast<deUint32>(i);
		}
		vk::flushAlloc(m_vk, m_context.getDevice(), m_indexBuffer->getBoundMemory());
	}

	// Check device for multidraw support:
	if (!m_context.getDeviceFeatures().multiDrawIndirect || m_testFirstInstanceNdx)
		m_isMultiDrawEnabled = false;
	else
		m_isMultiDrawEnabled = true;

	m_drawIndirectMaxCount = m_context.getDeviceProperties().limits.maxDrawIndirectCount;
}

template<>
void IndirectDraw::addCommand<vk::VkDrawIndirectCommand> (const vk::VkDrawIndirectCommand& command)
{
	DE_ASSERT(m_drawType == DRAW_TYPE_SEQUENTIAL);

	const size_t currentSize = m_indirectBufferContents.size();

	m_indirectBufferContents.resize(currentSize + sizeof(command));

	deMemcpy(&m_indirectBufferContents[currentSize], &command, sizeof(command));
}

template<>
void IndirectDraw::addCommand<vk::VkDrawIndexedIndirectCommand> (const vk::VkDrawIndexedIndirectCommand& command)
{
	DE_ASSERT(m_drawType == DRAW_TYPE_INDEXED);

	const size_t currentSize = m_indirectBufferContents.size();

	m_indirectBufferContents.resize(currentSize + sizeof(command));

	deMemcpy(&m_indirectBufferContents[currentSize], &command, sizeof(command));
}

tcu::TestStatus IndirectDraw::iterate (void)
{
	tcu::TestLog&		log		= m_context.getTestContext().getLog();
	const vk::VkQueue	queue	= m_context.getUniversalQueue();
	const vk::VkDevice	device	= m_context.getDevice();

					m_drawCount			= 2;
					m_offsetInBuffer	= sizeof(m_junkData);
	const deUint32	m_bufferDrawCount	= 2u * m_drawCount;

	if (m_drawType == DRAW_TYPE_SEQUENTIAL)
	{
		switch (m_topology)
		{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			vk::VkDrawIndirectCommand drawCommands[] =
			{
				{
					3u,									//vertexCount
					1u,									//instanceCount
					2u,									//firstVertex
					(m_testFirstInstanceNdx ? 1u : 0u)	//firstInstance
				},
				{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deUint32)-9 }, // junk (stride)
				{
					3u,									//vertexCount
					1u,									//instanceCount
					5u,									//firstVertex
					0u									//firstInstance
				}
			};
			addCommand(drawCommands[0]);
			addCommand(drawCommands[1]);
			addCommand(drawCommands[2]);
			addCommand(drawCommands[1]);
			if (m_testIndirectCountExt != IndirectCountType::NONE)
			{
				// Add padding data to the buffer to make sure it's large enough.
				for (deUint32 i = 0; i < m_bufferDrawCount; ++i)
				{
					addCommand(drawCommands[1]);
					addCommand(drawCommands[1]);
				}
			}
			break;
		}
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			vk::VkDrawIndirectCommand drawCommands[] =
			{
				{
					4u,									//vertexCount
					1u,									//instanceCount
					2u,									//firstVertex
					(m_testFirstInstanceNdx ? 1u : 0u)	//firstInstance
				},
				{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deUint32)-9 }, // junk (stride)
				{
					4u,									//vertexCount
					1u,									//instanceCount
					6u,									//firstVertex
					0u									//firstInstance
				}
			};
			addCommand(drawCommands[0]);
			addCommand(drawCommands[1]);
			addCommand(drawCommands[2]);
			addCommand(drawCommands[1]);
			if (m_testIndirectCountExt != IndirectCountType::NONE)
			{
				// Add padding data to the buffer to make sure it's large enough.
				for (deUint32 i = 0; i < m_bufferDrawCount; ++i)
				{
					addCommand(drawCommands[1]);
					addCommand(drawCommands[1]);
				}
			}
			break;
		}
		default:
			TCU_FAIL("impossible");
		}

		m_strideInBuffer = 2 * (deUint32)sizeof(vk::VkDrawIndirectCommand);
	}
	else if (m_drawType == DRAW_TYPE_INDEXED)
	{
		switch (m_topology)
		{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			vk::VkDrawIndexedIndirectCommand drawCommands[] =
			{
				{
					3u,									// indexCount
					1u,									// instanceCount
					2u,									// firstIndex
					VERTEX_OFFSET,						// vertexOffset
					(m_testFirstInstanceNdx ? 1u : 0u),	// firstInstance
				},
				{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deInt32)9, (deUint32)-7 }, // junk (stride)
				{
					3u,									// indexCount
					1u,									// instanceCount
					5u,									// firstIndex
					VERTEX_OFFSET,						// vertexOffset
					0u									// firstInstance
				}
			};
			addCommand(drawCommands[0]);
			addCommand(drawCommands[1]);
			addCommand(drawCommands[2]);
			addCommand(drawCommands[1]);
			if (m_testIndirectCountExt != IndirectCountType::NONE)
			{
				// Add padding data to the buffer to make sure it's large enough.
				for (deUint32 i = 0; i < m_bufferDrawCount; ++i)
				{
					addCommand(drawCommands[1]);
					addCommand(drawCommands[1]);
				}
			}
			break;
		}
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			vk::VkDrawIndexedIndirectCommand drawCommands[] =
			{
				{
					4u,									// indexCount
					1u,									// instanceCount
					2u,									// firstIndex
					VERTEX_OFFSET,						// vertexOffset
					(m_testFirstInstanceNdx ? 1u : 0u),	// firstInstance
				},
				{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deInt32)9, (deUint32)-7 }, // junk (stride)
				{
					4u,									// indexCount
					1u,									// instanceCount
					6u,									// firstIndex
					VERTEX_OFFSET,						// vertexOffset
					0u									// firstInstance
				}
			};
			addCommand(drawCommands[0]);
			addCommand(drawCommands[1]);
			addCommand(drawCommands[2]);
			addCommand(drawCommands[1]);
			if (m_testIndirectCountExt != IndirectCountType::NONE)
			{
				// Add padding data to the buffer to make sure it's large enough.
				for (deUint32 i = 0; i < m_bufferDrawCount; ++i)
				{
					addCommand(drawCommands[1]);
					addCommand(drawCommands[1]);
				}
			}
			break;
		}
		default:
			TCU_FAIL("impossible");
		}

		m_strideInBuffer = 2 * (deUint32)sizeof(vk::VkDrawIndexedIndirectCommand);
	}

	beginRenderPass();

	const vk::VkDeviceSize vertexBufferOffset	= 0;
	const vk::VkBuffer vertexBuffer				= m_vertexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	const vk::VkDeviceSize dataSize = m_indirectBufferContents.size();

	m_indirectBuffer = Buffer::createAndAlloc(	m_vk,
												m_context.getDevice(),
												BufferCreateInfo(dataSize + m_offsetInBuffer,
																 vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
												m_context.getDefaultAllocator(),
												vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(m_indirectBuffer->getBoundMemory().getHostPtr());

	deMemcpy(ptr, &m_junkData, static_cast<size_t>(m_offsetInBuffer));
	deMemcpy(ptr + m_offsetInBuffer, &m_indirectBufferContents[0], static_cast<size_t>(dataSize));

	vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory());

	if (m_testIndirectCountExt != IndirectCountType::NONE)
	{
		m_offsetInCountBuffer = sizeof(tcu::Vec3);
		m_indirectCountBuffer = Buffer::createAndAlloc(m_vk,
													   m_context.getDevice(),
													   BufferCreateInfo(m_offsetInCountBuffer + sizeof(m_drawCount),
																		vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
													   m_context.getDefaultAllocator(),
													   vk::MemoryRequirement::HostVisible);

		deUint8* countBufferPtr = reinterpret_cast<deUint8*>(m_indirectCountBuffer->getBoundMemory().getHostPtr());

		// For IndirectCountType::PARAM_LIMIT, the real limit will be set using the call parameter.
		if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
			*(deUint32*)(countBufferPtr + m_offsetInCountBuffer) = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? 0u : m_indirectCountExtDrawPadding);
		else
			*(deUint32*)(countBufferPtr + m_offsetInCountBuffer) = (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? 1u : m_drawCount + m_indirectCountExtDrawPadding);

		vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectCountBuffer->getBoundMemory());
	}

	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	if (m_drawType == DRAW_TYPE_INDEXED)
	{
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, m_indexBuffer->object(), DE_NULL, vk::VK_INDEX_TYPE_UINT32);
	}

	if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
	{
		switch (m_drawType)
		{
			case DRAW_TYPE_SEQUENTIAL:
			{
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					const deUint32 maxDrawCount = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_indirectCountExtDrawPadding : 0u);
					m_vk.cmdDrawIndirectCount(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer,
											  m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
											  m_strideInBuffer);
				}
				else
					m_vk.cmdDrawIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer, m_drawCount, m_strideInBuffer);
				break;
			}
			case DRAW_TYPE_INDEXED:
			{
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					const deUint32 maxDrawCount = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_indirectCountExtDrawPadding : 0u);
					m_vk.cmdDrawIndexedIndirectCount(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer,
													 m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
													 m_strideInBuffer);
				}
				else
					m_vk.cmdDrawIndexedIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer, m_drawCount, m_strideInBuffer);
				break;
			}
			default:
				TCU_FAIL("impossible");
		}
	}
	else
	{
		for (deUint32 drawNdx = 0; drawNdx < m_drawCount; drawNdx++)
		{
			switch (m_drawType)
			{
				case DRAW_TYPE_SEQUENTIAL:
				{
					if (m_testIndirectCountExt != IndirectCountType::NONE)
					{
						const deUint32 maxDrawCount = (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_drawCount + m_indirectCountExtDrawPadding : 1u);
						m_vk.cmdDrawIndirectCount(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer,
												  m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
												  m_strideInBuffer);
					}
					else
						m_vk.cmdDrawIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer, 1u, 0u);
					break;
				}
				case DRAW_TYPE_INDEXED:
				{
					if (m_testIndirectCountExt != IndirectCountType::NONE)
					{
						const deUint32 maxDrawCount = (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_drawCount + m_indirectCountExtDrawPadding : 1u);
						m_vk.cmdDrawIndexedIndirectCount(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer,
														 m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
														 m_strideInBuffer);
					}
					else
						m_vk.cmdDrawIndexedIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer, 1u, 0u);
					break;
				}
				default:
					TCU_FAIL("impossible");
			}
		}
	}
	endRenderPass(m_vk, *m_cmdBuffer);
	endCommandBuffer(m_vk, *m_cmdBuffer);

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
		referenceFrame.getLevel(0), renderedFrame, 0.05f, tcu::COMPARE_LOG_RESULT))
	{
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

template<class FirstInstanceSupport>
IndirectDrawInstanced<FirstInstanceSupport>::IndirectDrawInstanced (Context &context, TestSpec testSpec)
	: IndirectDraw(context, testSpec)
{
	if (!FirstInstanceSupport::isTestSupported(m_context.getDeviceFeatures()))
	{
		throw tcu::NotSupportedError("Required 'drawIndirectFirstInstance' feature is not supported");
	}
}

template<class FirstInstanceSupport>
tcu::TestStatus IndirectDrawInstanced<FirstInstanceSupport>::iterate (void)
{
	tcu::TestLog&		log		= m_context.getTestContext().getLog();
	const vk::VkQueue	queue	= m_context.getUniversalQueue();
	const vk::VkDevice	device	= m_context.getDevice();

					m_drawCount			= 2;
					m_offsetInBuffer	= sizeof(m_junkData);
	const deUint32	m_bufferDrawCount	= 2u * m_drawCount;

	if (m_drawType == DRAW_TYPE_SEQUENTIAL)
	{
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
				addCommand(drawCmd[0]);
				addCommand(drawCmd[1]);
				addCommand(drawCmd[2]);
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					// Add padding data to the buffer to make sure it's large enough.
					for (deUint32 i = 0; i < m_bufferDrawCount; ++i)
					{
						addCommand(drawCmd[1]);
						addCommand(drawCmd[1]);
					}
				}
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
				addCommand(drawCmd[0]);
				addCommand(drawCmd[1]);
				addCommand(drawCmd[2]);
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					// Add padding data to the buffer to make sure it's large enough.
					for (deUint32 i = 0; i < m_bufferDrawCount; ++i)
					{
						addCommand(drawCmd[1]);
						addCommand(drawCmd[1]);
					}
				}
				break;
			}
			default:
				TCU_FAIL("impossible");
				break;
		}

		m_strideInBuffer = 2 * (deUint32)sizeof(vk::VkDrawIndirectCommand);
	}
	else if (m_drawType == DRAW_TYPE_INDEXED)
	{
		switch (m_topology)
		{
			case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			{
				vk::VkDrawIndexedIndirectCommand drawCmd[] =
				{
					{
						3,											// indexCount
						4,											// instanceCount
						2,											// firstIndex
						VERTEX_OFFSET,								// vertexOffset
						FirstInstanceSupport::getFirstInstance()	// firstInstance
					},
					{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deInt32)9, (deUint32)-7 },	// junk (stride)
					{
						3,											// indexCount
						4,											// instanceCount
						5,											// firstIndex
						VERTEX_OFFSET,								// vertexOffset
						FirstInstanceSupport::getFirstInstance()	// firstInstance
					}
				};
				addCommand(drawCmd[0]);
				addCommand(drawCmd[1]);
				addCommand(drawCmd[2]);
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					// Add padding data to the buffer to make sure it's large enough.
					for (deUint32 i = 0; i < m_bufferDrawCount; ++i)
					{
						addCommand(drawCmd[1]);
						addCommand(drawCmd[1]);
					}
				}
				break;
			}
			case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			{
				vk::VkDrawIndexedIndirectCommand drawCmd[] =
				{
					{
						4,											// indexCount
						4,											// instanceCount
						2,											// firstIndex
						VERTEX_OFFSET,								// vertexOffset
						FirstInstanceSupport::getFirstInstance()	// firstInstance
					},
					{ (deUint32)-4, (deUint32)-2, (deUint32)-11, (deInt32)9, (deUint32)-7 },	// junk (stride)
					{
						4,											// indexCount
						4,											// instanceCount
						6,											// firstIndex
						VERTEX_OFFSET,								// vertexOffset
						FirstInstanceSupport::getFirstInstance()	// firstInstance
					}
				};
				addCommand(drawCmd[0]);
				addCommand(drawCmd[1]);
				addCommand(drawCmd[2]);
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					// Add padding data to the buffer to make sure it's large enough.
					for (deUint32 i = 0; i < m_bufferDrawCount; ++i)
					{
						addCommand(drawCmd[1]);
						addCommand(drawCmd[1]);
					}
				}
				break;
			}
			default:
				TCU_FAIL("impossible");
				break;
		}

		m_strideInBuffer = 2 * (deUint32)sizeof(vk::VkDrawIndexedIndirectCommand);
	}

	beginRenderPass();

	const vk::VkDeviceSize vertexBufferOffset = 0;
	const vk::VkBuffer vertexBuffer = m_vertexBuffer->object();

	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	const vk::VkDeviceSize dataSize = m_indirectBufferContents.size();

	m_indirectBuffer = Buffer::createAndAlloc(	m_vk,
												m_context.getDevice(),
												BufferCreateInfo(dataSize + m_offsetInBuffer,
																 vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
												m_context.getDefaultAllocator(),
												vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(m_indirectBuffer->getBoundMemory().getHostPtr());

	deMemcpy(ptr, &m_junkData, static_cast<size_t>(m_offsetInBuffer));
	deMemcpy((ptr + m_offsetInBuffer), &m_indirectBufferContents[0], static_cast<size_t>(dataSize));

	vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory());

	if (m_testIndirectCountExt != IndirectCountType::NONE)
	{
		m_offsetInCountBuffer = sizeof(tcu::Vec3);
		m_indirectCountBuffer = Buffer::createAndAlloc(m_vk,
													   m_context.getDevice(),
													   BufferCreateInfo(m_offsetInCountBuffer + sizeof(m_drawCount),
																		vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
													   m_context.getDefaultAllocator(),
													   vk::MemoryRequirement::HostVisible);

		deUint8* countBufferPtr = reinterpret_cast<deUint8*>(m_indirectCountBuffer->getBoundMemory().getHostPtr());

		// For IndirectCountType::PARAM_LIMIT, the real limit will be set using the call parameter.
		if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
			*(deUint32*)(countBufferPtr + m_offsetInCountBuffer) = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? 0u : m_indirectCountExtDrawPadding);
		else
			*(deUint32*)(countBufferPtr + m_offsetInCountBuffer) = 1u;

		vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectCountBuffer->getBoundMemory());
	}

	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	if (m_drawType == DRAW_TYPE_INDEXED)
	{
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, m_indexBuffer->object(), DE_NULL, vk::VK_INDEX_TYPE_UINT32);
	}

	if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
	{
		switch (m_drawType)
		{
			case DRAW_TYPE_SEQUENTIAL:
			{
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					const deUint32 maxDrawCount = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_indirectCountExtDrawPadding : 0u);
					m_vk.cmdDrawIndirectCount(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer,
											  m_indirectCountBuffer->object(), m_offsetInCountBuffer,
											  maxDrawCount, m_strideInBuffer);
				}
				else
					m_vk.cmdDrawIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer, m_drawCount, m_strideInBuffer);
				break;
			}
			case DRAW_TYPE_INDEXED:
			{
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					const deUint32 maxDrawCount = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_indirectCountExtDrawPadding : 0u);
					m_vk.cmdDrawIndexedIndirectCount(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer,
													 m_indirectCountBuffer->object(), m_offsetInCountBuffer,
													 maxDrawCount, m_strideInBuffer);
				}
				else
					m_vk.cmdDrawIndexedIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer, m_drawCount, m_strideInBuffer);
				break;
			}
			default:
				TCU_FAIL("impossible");
		}
	}
	else
	{
		for (deUint32 drawNdx = 0; drawNdx < m_drawCount; drawNdx++)
		{
			switch (m_drawType)
			{
				case DRAW_TYPE_SEQUENTIAL:
				{
					if (m_testIndirectCountExt != IndirectCountType::NONE)
					{
						const deUint32 maxDrawCount = (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_drawCount + m_indirectCountExtDrawPadding : 1u);
						m_vk.cmdDrawIndirectCount(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer,
												  m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
												  m_strideInBuffer);
					}
					else
						m_vk.cmdDrawIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer, 1u, 0u);
					break;
				}
				case DRAW_TYPE_INDEXED:
				{
					if (m_testIndirectCountExt != IndirectCountType::NONE)
					{
						const deUint32 maxDrawCount = (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_drawCount + m_indirectCountExtDrawPadding : 1u);
						m_vk.cmdDrawIndexedIndirectCount(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer,
														 m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
														 m_strideInBuffer);
					}
					else
						m_vk.cmdDrawIndexedIndirect(*m_cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer, 1u, 0u);
					break;
				}
				default:
					TCU_FAIL("impossible");
			}
		}
	}
	endRenderPass(m_vk, *m_cmdBuffer);
	endCommandBuffer(m_vk, *m_cmdBuffer);

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	// Validation
	VK_CHECK(m_vk.queueWaitIdle(queue));

	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5 + static_cast<float>(HEIGHT)));

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
		referenceFrame.getLevel(0), renderedFrame, 0.05f, tcu::COMPARE_LOG_RESULT))
	{
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void checkIndirectCountExt (Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
}

}	// anonymous

IndirectDrawTests::IndirectDrawTests (tcu::TestContext& testCtx)
	: TestCaseGroup(testCtx, "indirect_draw", "indirect drawing simple geometry")
{
	/* Left blank on purpose */
}

IndirectDrawTests::~IndirectDrawTests (void) {}


void IndirectDrawTests::init (void)
{
	for (int drawTypeIdx = 0; drawTypeIdx < DRAWTYPE_LAST; drawTypeIdx++)
	{
		std::string drawTypeStr;
		switch (drawTypeIdx)
		{
			case DRAW_TYPE_SEQUENTIAL:
				drawTypeStr = "sequential";
				break;
			case DRAW_TYPE_INDEXED:
				drawTypeStr = "indexed";
				break;
			default:
				TCU_FAIL("impossible");
		}

		tcu::TestCaseGroup* drawTypeGroup = new tcu::TestCaseGroup(m_testCtx, drawTypeStr.c_str(), ("Draws geometry using " + drawTypeStr + "draw call").c_str());
		{
			tcu::TestCaseGroup* indirectDrawGroup			= new tcu::TestCaseGroup(m_testCtx, "indirect_draw", "Draws geometry");
			tcu::TestCaseGroup* indirectDrawCountGroup		= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_count", "Draws geometry with VK_KHR_draw_indirect_count extension");
			tcu::TestCaseGroup* indirectDrawParamCountGroup	= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_param_count", "Draws geometry with VK_KHR_draw_indirect_count extension and limit draws count with call parameter");
			{
				IndirectDraw::TestSpec testSpec;
				testSpec.drawType = static_cast<DrawType>(drawTypeIdx);
				testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetch.vert";
				testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				indirectDrawGroup->addChild(new InstanceFactory<IndirectDraw>(m_testCtx, "triangle_list", "Draws triangle list", testSpec));
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				indirectDrawGroup->addChild(new InstanceFactory<IndirectDraw>(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec));

				testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				indirectDrawCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport0>(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport0(checkIndirectCountExt)));
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				indirectDrawCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport0>(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport0(checkIndirectCountExt)));

				testSpec.testIndirectCountExt = IndirectCountType::PARAM_LIMIT;
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				indirectDrawParamCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport0>(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport0(checkIndirectCountExt)));
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				indirectDrawParamCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport0>(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport0(checkIndirectCountExt)));
			}
			drawTypeGroup->addChild(indirectDrawGroup);
			drawTypeGroup->addChild(indirectDrawCountGroup);
			drawTypeGroup->addChild(indirectDrawParamCountGroup);

			{
				tcu::TestCaseGroup* indirectDrawFirstInstanceGroup				= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_first_instance", "Draws geometry with different first instance in one commandbuffer");
				tcu::TestCaseGroup* indirectDrawCountFirstInstanceGroup			= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_count_first_instance", "Draws geometry with VK_KHR_draw_indirect_count extension with different first instance in one commandbuffer");
				tcu::TestCaseGroup* indirectDrawParamCountFirstInstanceGroup	= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_param_count_first_instance", "Draws geometry with VK_KHR_draw_indirect_count extension with different first instance in one commandbuffer and limit draws count with call parameter");
				{
					IndirectDraw::TestSpec testSpec;
					testSpec.testFirstInstanceNdx = true;
					testSpec.drawType = static_cast<DrawType>(drawTypeIdx);
					testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetchInstanceIndex.vert";
					testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw>(m_testCtx, "triangle_list", "Draws triangle list", testSpec));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw>(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec));

					testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport0>(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport0(checkIndirectCountExt)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport0>(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport0(checkIndirectCountExt)));

					testSpec.testIndirectCountExt = IndirectCountType::PARAM_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawParamCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport0>(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport0(checkIndirectCountExt)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawParamCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport0>(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport0(checkIndirectCountExt)));
				}
				drawTypeGroup->addChild(indirectDrawFirstInstanceGroup);
				drawTypeGroup->addChild(indirectDrawCountFirstInstanceGroup);
				drawTypeGroup->addChild(indirectDrawParamCountFirstInstanceGroup);
			}

			tcu::TestCaseGroup* indirectDrawInstancedGroup				= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_instanced", "Draws an instanced geometry");
			tcu::TestCaseGroup* indirectDrawCountInstancedGroup			= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_count_instanced", "Draws an instanced geometry with VK_KHR_draw_indirect_count extension");
			tcu::TestCaseGroup* indirectDrawParamCountInstancedGroup	= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_param_count_instanced", "Draws an instanced geometry with VK_KHR_draw_indirect_count extension and limit draws count with call parameter");
			{
				tcu::TestCaseGroup*	indirectDrawNoFirstInstanceGroup			= new tcu::TestCaseGroup(m_testCtx, "no_first_instance", "Use 0 as firstInstance");
				tcu::TestCaseGroup*	indirectDrawCountNoFirstInstanceGroup		= new tcu::TestCaseGroup(m_testCtx, "no_first_instance", "Use 0 as firstInstance");
				tcu::TestCaseGroup*	indirectDrawParamCountNoFirstInstanceGroup	= new tcu::TestCaseGroup(m_testCtx, "no_first_instance", "Use 0 as firstInstance");
				{
					IndirectDrawInstanced<FirstInstanceNotSupported>::TestSpec testSpec;
					testSpec.drawType = static_cast<DrawType>(drawTypeIdx);

					testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetchInstanced.vert";
					testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";

					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawNoFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceNotSupported> >(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawNoFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceNotSupported> >(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec));

					testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawCountNoFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceNotSupported>, FunctionSupport0>(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport0(checkIndirectCountExt)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawCountNoFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceNotSupported>, FunctionSupport0>(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport0(checkIndirectCountExt)));

					testSpec.testIndirectCountExt = IndirectCountType::PARAM_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawParamCountNoFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceNotSupported>, FunctionSupport0>(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport0(checkIndirectCountExt)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawParamCountNoFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceNotSupported>, FunctionSupport0>(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport0(checkIndirectCountExt)));
				}
				indirectDrawInstancedGroup->addChild(indirectDrawNoFirstInstanceGroup);
				indirectDrawCountInstancedGroup->addChild(indirectDrawCountNoFirstInstanceGroup);
				indirectDrawParamCountInstancedGroup->addChild(indirectDrawParamCountNoFirstInstanceGroup);

				tcu::TestCaseGroup*	indirectDrawFirstInstanceGroup				= new tcu::TestCaseGroup(m_testCtx, "first_instance", "Use drawIndirectFirstInstance optional feature");
				tcu::TestCaseGroup*	indirectDrawCountFirstInstanceGroup			= new tcu::TestCaseGroup(m_testCtx, "first_instance", "Use drawIndirectFirstInstance optional feature");
				tcu::TestCaseGroup*	indirectDrawParamCountFirstInstanceGroup	= new tcu::TestCaseGroup(m_testCtx, "first_instance", "Use drawIndirectFirstInstance optional feature");
				{
					IndirectDrawInstanced<FirstInstanceSupported>::TestSpec testSpec;
					testSpec.drawType = static_cast<DrawType>(drawTypeIdx);

					testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetchInstancedFirstInstance.vert";
					testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";

					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceSupported> >(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceSupported> >(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec));

					testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceSupported>, FunctionSupport0>(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport0(checkIndirectCountExt)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceSupported>, FunctionSupport0>(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport0(checkIndirectCountExt)));

					testSpec.testIndirectCountExt = IndirectCountType::PARAM_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawParamCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceSupported>, FunctionSupport0>(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport0(checkIndirectCountExt)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawParamCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDrawInstanced<FirstInstanceSupported>, FunctionSupport0>(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport0(checkIndirectCountExt)));
				}
				indirectDrawInstancedGroup->addChild(indirectDrawFirstInstanceGroup);
				indirectDrawCountInstancedGroup->addChild(indirectDrawCountFirstInstanceGroup);
				indirectDrawParamCountInstancedGroup->addChild(indirectDrawParamCountFirstInstanceGroup);
			}
			drawTypeGroup->addChild(indirectDrawInstancedGroup);
			drawTypeGroup->addChild(indirectDrawCountInstancedGroup);
			drawTypeGroup->addChild(indirectDrawParamCountInstancedGroup);
		}

		addChild(drawTypeGroup);
	}
}

}	// DrawTests
}	// vkt
