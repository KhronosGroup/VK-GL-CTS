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
#include "../compute/vktComputeTestsUtil.hpp"

#include "vktDrawBaseClass.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "vkQueryUtil.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include <numeric>
#include <vector>

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

enum
{
	//INDEX_BUFFER_ALLOCATION_OFFSET = static_cast<int>(sizeof(tcu::Vec4)),
	INDEX_BUFFER_ALLOCATION_OFFSET = 0,
};

struct JunkData
{
	JunkData()
	{
		for (auto& val : junk)
			val = 0xEFBEADDEu;
	}

	uint32_t junk[1024];
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
	DrawTypedTestSpec(const SharedGroupParams groupParams_)
		: TestSpecBase{ {}, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, groupParams_ }
		, drawType(DRAWTYPE_LAST)
		, testFirstInstanceNdx(false)
		, testIndirectCountExt(IndirectCountType::NONE)
		, dataFromCompute(false)
		, useMemoryAccess(false)
		, layerCount(1u)
		, bindIndexBufferOffset(0ull)
		, indexBufferAllocOffset(0ull)
	{}

	DrawType			drawType;
	bool				testFirstInstanceNdx;
	IndirectCountType	testIndirectCountExt;
	bool				dataFromCompute;
	bool				useMemoryAccess;
	uint32_t			layerCount;
	vk::VkDeviceSize	bindIndexBufferOffset;
	vk::VkDeviceSize	indexBufferAllocOffset;
};

class IndirectDraw : public DrawTestsBaseClass
{
public:
	typedef DrawTypedTestSpec			TestSpec;

										IndirectDraw					(Context &context, TestSpec testSpec);
	virtual	tcu::TestStatus				iterate							(void);

	void								draw							(vk::VkCommandBuffer cmdBuffer);
	template<typename T> void			addCommand						(const T&);

protected:
	void								setVertexBuffer					(void);
	void								setFirstInstanceVertexBuffer	(void);
	void								negateDataUsingCompute			(vk::VkDeviceSize indirectBufferSize, vk::VkDeviceSize countBufferSize);
	void								countBufferBarrier				(vk::VkBuffer indirectCountBuffer, vk::VkDeviceSize indirectCountBufferSize) const;

	std::vector<char>					m_indirectBufferContents;
	de::SharedPtr<Buffer>				m_indirectBuffer;
	vk::VkDeviceSize					m_offsetInBuffer;
	deUint32							m_strideInBuffer;

	const IndirectCountType				m_testIndirectCountExt;
	de::SharedPtr<Buffer>				m_indirectCountBuffer;
	vk::VkDeviceSize					m_offsetInCountBuffer;
	const deUint32						m_indirectCountExtDrawPadding;

	deUint32							m_drawCount;
	JunkData							m_junkData;

	const DrawType						m_drawType;
	const bool							m_testFirstInstanceNdx;
	deBool								m_isMultiDrawEnabled;
	deUint32							m_drawIndirectMaxCount;
	deBool								m_dataFromComputeShader;
	deBool								m_useMemoryAccess;

	de::SharedPtr<Buffer>				m_indexBuffer;
	const vk::VkDeviceSize				m_bindIndexBufferOffset;
	const vk::VkDeviceSize				m_indexBufferAllocOffset;

	vk::Move<vk::VkDescriptorSetLayout>	m_descriptorSetLayout;
	vk::Move<vk::VkDescriptorPool>		m_descriptorPool;
	vk::Move<vk::VkDescriptorSet>		m_descriptorSetIndirectBuffer;
	vk::Move<vk::VkDescriptorSet>		m_descriptorSetCountBuffer;
	vk::Move<vk::VkShaderModule>		m_computeShaderModule;
	vk::Move<vk::VkPipelineLayout>		m_pipelineLayout;
	vk::Move<vk::VkPipeline>			m_computePipeline;
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

// When testing indirect data generation from a compute shader, the original data is negated bitwise
// and compute shader restores it before being used in an indirect draw.
void IndirectDraw::negateDataUsingCompute(vk::VkDeviceSize indirectBufferSize, vk::VkDeviceSize countBufferSize)
{
	m_descriptorSetLayout	= vk::DescriptorSetLayoutBuilder().addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT).build(m_vk, m_context.getDevice());
	m_descriptorPool		= vk::DescriptorPoolBuilder().addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u).build(m_vk, m_context.getDevice(), vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

	// Indirect buffer
	{
		m_descriptorSetIndirectBuffer = vk::makeDescriptorSet(m_vk, m_context.getDevice(), *m_descriptorPool, *m_descriptorSetLayout);
		const vk::VkDescriptorBufferInfo bufferDescriptorInfo = vk::makeDescriptorBufferInfo(m_indirectBuffer->object(), 0ull, indirectBufferSize);
		vk::DescriptorSetUpdateBuilder()
				.writeSingle(*m_descriptorSetIndirectBuffer, vk::DescriptorSetUpdateBuilder::Location::binding(0u),
							 vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
				.update(m_vk, m_context.getDevice());
	}

	// Indirect count buffer
	if (m_testIndirectCountExt != IndirectCountType::NONE)
	{
		m_descriptorSetCountBuffer = vk::makeDescriptorSet(m_vk, m_context.getDevice(), *m_descriptorPool, *m_descriptorSetLayout);
		const vk::VkDescriptorBufferInfo bufferDescriptorInfo = vk::makeDescriptorBufferInfo(m_indirectCountBuffer->object(), 0ull, countBufferSize);
		vk::DescriptorSetUpdateBuilder()
				.writeSingle(*m_descriptorSetCountBuffer, vk::DescriptorSetUpdateBuilder::Location::binding(0u),
							 vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
				.update(m_vk, m_context.getDevice());
	}

	m_computeShaderModule	= vk::createShaderModule(m_vk, m_context.getDevice(), m_context.getBinaryCollection().get("vulkan/draw/NegateData.comp"), 0u);
	m_pipelineLayout		= vk::makePipelineLayout(m_vk, m_context.getDevice(), *m_descriptorSetLayout);
	m_computePipeline		= makeComputePipeline(m_vk, m_context.getDevice(), *m_pipelineLayout, *m_computeShaderModule);

	const vk::VkBufferMemoryBarrier		hostWriteBarrier	= vk::makeBufferMemoryBarrier(
																m_useMemoryAccess ? vk::VK_ACCESS_MEMORY_WRITE_BIT : vk::VK_ACCESS_HOST_WRITE_BIT,
																m_useMemoryAccess ? vk::VK_ACCESS_MEMORY_READ_BIT : vk::VK_ACCESS_SHADER_READ_BIT,
																m_indirectBuffer->object(), 0ull, indirectBufferSize);
	const vk::VkBufferMemoryBarrier		indirectDrawBarrier	= vk::makeBufferMemoryBarrier(
																m_useMemoryAccess ? vk::VK_ACCESS_MEMORY_WRITE_BIT : vk::VK_ACCESS_SHADER_WRITE_BIT,
																m_useMemoryAccess ? vk::VK_ACCESS_MEMORY_READ_BIT : vk::VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
																m_indirectBuffer->object(), 0ull, indirectBufferSize);

	m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);
	m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &m_descriptorSetIndirectBuffer.get(), 0u, DE_NULL);
	m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (vk::VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier, 0, (const vk::VkImageMemoryBarrier*)DE_NULL);
	m_vk.cmdDispatch(*m_cmdBuffer, 1, 1, 1);
	m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, (vk::VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL, 1, &indirectDrawBarrier, 0, (const vk::VkImageMemoryBarrier*)DE_NULL);

	if (m_testIndirectCountExt != IndirectCountType::NONE)
	{
		m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &m_descriptorSetCountBuffer.get(), 0u, DE_NULL);
		m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (vk::VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier, 0, (const vk::VkImageMemoryBarrier*)DE_NULL);
		m_vk.cmdDispatch(*m_cmdBuffer, 1, 1, 1);
		m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, (vk::VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL, 1, &indirectDrawBarrier, 0, (const vk::VkImageMemoryBarrier*)DE_NULL);
	}
}

IndirectDraw::IndirectDraw (Context &context, TestSpec testSpec)
	: DrawTestsBaseClass				(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT], testSpec.groupParams, testSpec.topology, testSpec.layerCount)
	, m_testIndirectCountExt			(testSpec.testIndirectCountExt)
	, m_indirectCountExtDrawPadding		(1u)
	, m_drawType						(testSpec.drawType)
	, m_testFirstInstanceNdx			(testSpec.testFirstInstanceNdx)
	, m_dataFromComputeShader			(testSpec.dataFromCompute)
	, m_useMemoryAccess					(testSpec.useMemoryAccess)
	, m_bindIndexBufferOffset			(testSpec.bindIndexBufferOffset)
	, m_indexBufferAllocOffset			(testSpec.indexBufferAllocOffset)
{
	const auto& vki		= m_context.getInstanceInterface();
	const auto	physDev	= m_context.getPhysicalDevice();
	const auto	device	= m_context.getDevice();
	const auto&	devProp	= m_context.getDeviceProperties();

	if (m_testFirstInstanceNdx)
		setFirstInstanceVertexBuffer();
	else
		setVertexBuffer();

	initialize();

	if (testSpec.drawType == DRAW_TYPE_INDEXED)
	{
		const auto indexCount = m_data.size() - VERTEX_OFFSET;

		std::vector<uint32_t> indexVec (indexCount);
		std::iota(indexVec.begin(), indexVec.end(), 0u);

		const auto bufferSize = de::dataSize(indexVec) + m_bindIndexBufferOffset;

		const vk::SimpleAllocator::OptionalOffsetParams	offsetParams	({ devProp.limits.nonCoherentAtomSize, m_indexBufferAllocOffset });
		vk::SimpleAllocator								allocator		(m_vk, device, vk::getPhysicalDeviceMemoryProperties(vki, physDev), offsetParams);

		m_indexBuffer = Buffer::createAndAlloc(m_vk,
											   device,
											   BufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
											   allocator,
											   vk::MemoryRequirement::HostVisible);

		const auto bufferStart = reinterpret_cast<char*>(m_indexBuffer->getBoundMemory().getHostPtr());
		deMemset(bufferStart, 0xFF, static_cast<size_t>(m_bindIndexBufferOffset));
		deMemcpy(bufferStart + m_bindIndexBufferOffset, de::dataOrNull(indexVec), de::dataSize(indexVec));

		vk::flushAlloc(m_vk, device, m_indexBuffer->getBoundMemory());
	}

	// Check device for multidraw support:
	if (!m_context.getDeviceFeatures().multiDrawIndirect || m_testFirstInstanceNdx)
		m_isMultiDrawEnabled = false;
	else
		m_isMultiDrawEnabled = true;

	m_drawIndirectMaxCount = devProp.limits.maxDrawIndirectCount;
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

void IndirectDraw::draw (vk::VkCommandBuffer cmdBuffer)
{
	const vk::VkDeviceSize	vertexBufferOffset	= 0;
	const vk::VkBuffer		vertexBuffer		= m_vertexBuffer->object();

	m_vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
	m_vk.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	if (m_drawType == DRAW_TYPE_INDEXED)
		m_vk.cmdBindIndexBuffer(cmdBuffer, m_indexBuffer->object(), m_bindIndexBufferOffset, vk::VK_INDEX_TYPE_UINT32);

	if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
	{
		switch (m_drawType)
		{
			case DRAW_TYPE_SEQUENTIAL:
			{
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					const deUint32 maxDrawCount = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_indirectCountExtDrawPadding : 0u);
					m_vk.cmdDrawIndirectCount(cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer,
											  m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
											  m_strideInBuffer);
				}
				else
					m_vk.cmdDrawIndirect(cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer, m_drawCount, m_strideInBuffer);
				break;
			}
			case DRAW_TYPE_INDEXED:
			{
				if (m_testIndirectCountExt != IndirectCountType::NONE)
				{
					const deUint32 maxDrawCount = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_indirectCountExtDrawPadding : 0u);
					m_vk.cmdDrawIndexedIndirectCount(cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer,
													 m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
													 m_strideInBuffer);
				}
				else
					m_vk.cmdDrawIndexedIndirect(cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer, m_drawCount, m_strideInBuffer);
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
						m_vk.cmdDrawIndirectCount(cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer,
												  m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
												  m_strideInBuffer);
					}
					else
						m_vk.cmdDrawIndirect(cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer, 1u, 0u);
					break;
				}
				case DRAW_TYPE_INDEXED:
				{
					if (m_testIndirectCountExt != IndirectCountType::NONE)
					{
						const deUint32 maxDrawCount = (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? m_drawCount + m_indirectCountExtDrawPadding : 1u);
						m_vk.cmdDrawIndexedIndirectCount(cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer,
														 m_indirectCountBuffer->object(), m_offsetInCountBuffer, maxDrawCount,
														 m_strideInBuffer);
					}
					else
						m_vk.cmdDrawIndexedIndirect(cmdBuffer, m_indirectBuffer->object(), m_offsetInBuffer + drawNdx*m_strideInBuffer, 1u, 0u);
					break;
				}
				default:
					TCU_FAIL("impossible");
			}
		}
	}
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

	const vk::VkDeviceSize	dataSize			= m_indirectBufferContents.size();
	const vk::VkDeviceSize	indirectBufferSize	= dataSize + m_offsetInBuffer;
	vk::VkBufferUsageFlags	usageFlags			= vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	if (m_dataFromComputeShader)
		usageFlags |= vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	m_indirectBuffer = Buffer::createAndAlloc(	m_vk,
												m_context.getDevice(),
												BufferCreateInfo(indirectBufferSize, usageFlags),
												m_context.getDefaultAllocator(),
												vk::MemoryRequirement::HostVisible,
												static_cast<vk::VkDeviceSize>(INDEX_BUFFER_ALLOCATION_OFFSET));

	deUint8* ptr = reinterpret_cast<deUint8*>(m_indirectBuffer->getBoundMemory().getHostPtr()) + INDEX_BUFFER_ALLOCATION_OFFSET;

	deMemcpy(ptr, &m_junkData, static_cast<size_t>(m_offsetInBuffer));
	deMemcpy(ptr + m_offsetInBuffer, &m_indirectBufferContents[0], static_cast<size_t>(dataSize));

	if (m_dataFromComputeShader)
	{
		// Negate all the buffer data and let a compute shader restore it to original.
		for (int i = 0; i < static_cast<int>(indirectBufferSize); i++)
		{
			ptr[i] = static_cast<deUint8>(~ptr[i]);
		}
	}

	vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory());

	m_offsetInCountBuffer = sizeof(tcu::Vec3);
	const vk::VkDeviceSize countBufferSize	= m_offsetInCountBuffer + sizeof(m_drawCount);

	if (m_testIndirectCountExt != IndirectCountType::NONE)
	{
		m_indirectCountBuffer = Buffer::createAndAlloc(m_vk,
													   m_context.getDevice(),
													   BufferCreateInfo(countBufferSize, usageFlags),
													   m_context.getDefaultAllocator(),
													   vk::MemoryRequirement::HostVisible);

		deUint8* countBufferPtr = reinterpret_cast<deUint8*>(m_indirectCountBuffer->getBoundMemory().getHostPtr());

		// For IndirectCountType::PARAM_LIMIT, the real limit will be set using the call parameter.
		if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
			*(deUint32*)(countBufferPtr + m_offsetInCountBuffer) = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? 0u : m_indirectCountExtDrawPadding);
		else
			*(deUint32*)(countBufferPtr + m_offsetInCountBuffer) = (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? 1u : m_drawCount + m_indirectCountExtDrawPadding);

		if (m_dataFromComputeShader)
		{
			// Negate all the buffer data and let a compute shader restore it to original.
			for (int i = 0; i < static_cast<int>(countBufferSize); i++)
			{
				countBufferPtr[i] = static_cast<deUint8>(~countBufferPtr[i]);
			}
		}

		vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectCountBuffer->getBoundMemory());
	}

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

		draw(*m_secCmdBuffer);

		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		if (m_dataFromComputeShader)
			negateDataUsingCompute(indirectBufferSize, countBufferSize);

		preRenderBarriers();
		if (m_testIndirectCountExt != IndirectCountType::NONE)
			countBufferBarrier(m_indirectCountBuffer->object(), countBufferSize);

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

		if (m_dataFromComputeShader)
			negateDataUsingCompute(indirectBufferSize, countBufferSize);

		preRenderBarriers();
		if (m_testIndirectCountExt != IndirectCountType::NONE)
			countBufferBarrier(m_indirectCountBuffer->object(), countBufferSize);
		beginDynamicRender(*m_cmdBuffer);

		draw(*m_cmdBuffer);

		endDynamicRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		if (m_dataFromComputeShader)
			negateDataUsingCompute(indirectBufferSize, countBufferSize);

		preRenderBarriers();
		if (m_testIndirectCountExt != IndirectCountType::NONE)
			countBufferBarrier(m_indirectCountBuffer->object(), countBufferSize);
		beginLegacyRender(*m_cmdBuffer);

		draw(*m_cmdBuffer);

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

void IndirectDraw::countBufferBarrier(vk::VkBuffer indirectCountBuffer, vk::VkDeviceSize indirectCountBufferSize) const
{
	const vk::VkBufferMemoryBarrier countBufferBarrier = vk::makeBufferMemoryBarrier(
		m_useMemoryAccess ? vk::VK_ACCESS_MEMORY_WRITE_BIT : vk::VK_ACCESS_SHADER_WRITE_BIT,
		m_useMemoryAccess ? vk::VK_ACCESS_MEMORY_READ_BIT : vk::VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
		indirectCountBuffer, 0ull, indirectCountBufferSize);

	m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, DE_NULL, 1, &countBufferBarrier, 0, DE_NULL);
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

	const vk::VkDeviceSize	dataSize			= m_indirectBufferContents.size();
	const vk::VkDeviceSize	indirectBufferSize	= dataSize + m_offsetInBuffer;
	vk::VkBufferUsageFlags	usageFlags			= vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	if (m_dataFromComputeShader)
		usageFlags |= vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	m_indirectBuffer = Buffer::createAndAlloc(	m_vk,
												m_context.getDevice(),
												BufferCreateInfo(indirectBufferSize, usageFlags),
												m_context.getDefaultAllocator(),
												vk::MemoryRequirement::HostVisible,
												static_cast<vk::VkDeviceSize>(INDEX_BUFFER_ALLOCATION_OFFSET));

	deUint8* ptr = reinterpret_cast<deUint8*>(m_indirectBuffer->getBoundMemory().getHostPtr()) + INDEX_BUFFER_ALLOCATION_OFFSET;

	deMemcpy(ptr, &m_junkData, static_cast<size_t>(m_offsetInBuffer));
	deMemcpy((ptr + m_offsetInBuffer), &m_indirectBufferContents[0], static_cast<size_t>(dataSize));

	if (m_dataFromComputeShader)
	{
		// Negate all the buffer data and let a compute shader restore it to original.
		for (int i = 0; i < static_cast<int>(indirectBufferSize); i++)
		{
			ptr[i] = static_cast<deUint8>(~ptr[i]);
		}
	}

	vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory());

	m_offsetInCountBuffer = sizeof(tcu::Vec3);
	const vk::VkDeviceSize	countBufferSize	= m_offsetInCountBuffer + sizeof(m_drawCount);

	if (m_testIndirectCountExt != IndirectCountType::NONE)
	{
		m_indirectCountBuffer = Buffer::createAndAlloc(m_vk,
													   m_context.getDevice(),
													   BufferCreateInfo(countBufferSize, usageFlags),
													   m_context.getDefaultAllocator(),
													   vk::MemoryRequirement::HostVisible);

		deUint8* countBufferPtr = reinterpret_cast<deUint8*>(m_indirectCountBuffer->getBoundMemory().getHostPtr());

		// For IndirectCountType::PARAM_LIMIT, the real limit will be set using the call parameter.
		if (m_isMultiDrawEnabled && m_drawCount <= m_drawIndirectMaxCount)
			*(deUint32*)(countBufferPtr + m_offsetInCountBuffer) = m_drawCount + (m_testIndirectCountExt == IndirectCountType::BUFFER_LIMIT ? 0u : m_indirectCountExtDrawPadding);
		else
			*(deUint32*)(countBufferPtr + m_offsetInCountBuffer) = 1u;

		if (m_dataFromComputeShader)
		{
			// Negate all the buffer data and let a compute shader restore it to original.
			for (int i = 0; i < static_cast<int>(countBufferSize); i++)
			{
				countBufferPtr[i] = static_cast<deUint8>(~countBufferPtr[i]);
			}
		}

		vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectCountBuffer->getBoundMemory());
	}

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

		draw(*m_secCmdBuffer);

		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		if (m_dataFromComputeShader)
			negateDataUsingCompute(indirectBufferSize, countBufferSize);

		preRenderBarriers();
		if (m_testIndirectCountExt != IndirectCountType::NONE)
			countBufferBarrier(m_indirectCountBuffer->object(), countBufferSize);

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if(m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		if (m_dataFromComputeShader)
			negateDataUsingCompute(indirectBufferSize, countBufferSize);

		preRenderBarriers();
		if (m_testIndirectCountExt != IndirectCountType::NONE)
			countBufferBarrier(m_indirectCountBuffer->object(), countBufferSize);

		beginDynamicRender(*m_cmdBuffer);
		draw(*m_cmdBuffer);
		endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		if (m_dataFromComputeShader)
			negateDataUsingCompute(indirectBufferSize, countBufferSize);

		preRenderBarriers();
		if (m_testIndirectCountExt != IndirectCountType::NONE)
			countBufferBarrier(m_indirectCountBuffer->object(), countBufferSize);

		beginLegacyRender(*m_cmdBuffer);
		draw(*m_cmdBuffer);
		endLegacyRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

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

void checkSupport(Context& context, IndirectDraw::TestSpec testSpec)
{
	if (testSpec.testIndirectCountExt != IndirectCountType::NONE)
		context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");

	if (testSpec.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (testSpec.layerCount > 1u)
	{
		const auto& features = context.getMultiviewFeatures();
		if (!features.multiview)
			TCU_THROW(NotSupportedError, "multiview not supported");
	}
}

}	// anonymous

IndirectDrawTests::IndirectDrawTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
	: TestCaseGroup		(testCtx, "indirect_draw", "indirect drawing simple geometry")
	, m_groupParams		(groupParams)
{
	/* Left blank on purpose */
}

IndirectDrawTests::~IndirectDrawTests (void) {}


void IndirectDrawTests::init (void)
{
	for (auto dataFromCompute : { false, true })
	for (int drawTypeIdx = 0; drawTypeIdx < DRAWTYPE_LAST; drawTypeIdx++)
	for (const auto bindIndexBufferOffset : { vk::VkDeviceSize{0}, static_cast<vk::VkDeviceSize>(sizeof(uint32_t) * 4u) })
	for (const auto indexBufferAllocOffset : { vk::VkDeviceSize{0}, static_cast<vk::VkDeviceSize>(sizeof(tcu::Vec4)) })
	{
		const bool nonZeroBindIndexBufferOffset = (bindIndexBufferOffset > 0);
		const bool nonZeroAllocOffset = (indexBufferAllocOffset > 0);

		if (nonZeroBindIndexBufferOffset && drawTypeIdx != DRAW_TYPE_INDEXED)
			continue;

		if (nonZeroAllocOffset && drawTypeIdx != DRAW_TYPE_INDEXED)
			continue;

		// reduce number of tests for dynamic rendering cases where secondary command buffer is used
		if (m_groupParams->useSecondaryCmdBuffer && (dataFromCompute == m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass))
			continue;

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

		if (dataFromCompute)
			drawTypeStr += "_data_from_compute";

		if (nonZeroBindIndexBufferOffset)
			drawTypeStr += "_bind_offset_" + std::to_string(bindIndexBufferOffset);

		if (nonZeroAllocOffset)
			drawTypeStr += "_alloc_offset_" + std::to_string(indexBufferAllocOffset);

		tcu::TestCaseGroup* drawTypeGroup = new tcu::TestCaseGroup(m_testCtx, drawTypeStr.c_str(), ("Draws geometry using " + drawTypeStr + "draw call").c_str());
		{
			tcu::TestCaseGroup* indirectDrawGroup			= new tcu::TestCaseGroup(m_testCtx, "indirect_draw", "Draws geometry");
			tcu::TestCaseGroup* indirectDrawCountGroup		= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_count", "Draws geometry with VK_KHR_draw_indirect_count extension");
			tcu::TestCaseGroup* indirectDrawParamCountGroup	= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_param_count", "Draws geometry with VK_KHR_draw_indirect_count extension and limit draws count with call parameter");
			tcu::TestCaseGroup* indirectDrawMultiviewGroup	= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_multiview", "Draws geometry with indirect draws and multiview");
			{
				IndirectDraw::TestSpec testSpec(m_groupParams);
				testSpec.drawType = static_cast<DrawType>(drawTypeIdx);
				testSpec.dataFromCompute = dataFromCompute;
				testSpec.bindIndexBufferOffset = bindIndexBufferOffset;
				testSpec.indexBufferAllocOffset = indexBufferAllocOffset;
				testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetch.vert";
				testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
				if (dataFromCompute)
					testSpec.shaders[glu::SHADERTYPE_COMPUTE] = "vulkan/draw/NegateData.comp";

				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				indirectDrawGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
					(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				indirectDrawGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
					(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));

				// test using V_ACCESS_MEMORY_WRITE/READ_BIT - there is no need to repeat this case for different drawing options
				if (dataFromCompute && drawTypeIdx && !m_groupParams->useDynamicRendering && !m_groupParams->useSecondaryCmdBuffer)
				{
					testSpec.useMemoryAccess = true;
					indirectDrawGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
						(m_testCtx, "triangle_strip_memory_access", "Draws triangle strip", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.useMemoryAccess = false;
				}

				testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				indirectDrawCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
					(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				indirectDrawCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
					(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));

				// test using V_ACCESS_MEMORY_WRITE/READ_BIT - there is no need to repeat this case for different drawing options
				if (dataFromCompute && drawTypeIdx && !m_groupParams->useDynamicRendering && !m_groupParams->useSecondaryCmdBuffer)
				{
					testSpec.useMemoryAccess = true;
					indirectDrawCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
						(m_testCtx, "triangle_strip_memory_access", "Draws triangle strip", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.useMemoryAccess = false;
				}

				testSpec.testIndirectCountExt = IndirectCountType::PARAM_LIMIT;
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				indirectDrawParamCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
					(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				indirectDrawParamCountGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
					(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));

				testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
				testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				testSpec.layerCount = 2u;
				indirectDrawMultiviewGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
					(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
			}
			drawTypeGroup->addChild(indirectDrawGroup);
			drawTypeGroup->addChild(indirectDrawCountGroup);
			drawTypeGroup->addChild(indirectDrawParamCountGroup);
			drawTypeGroup->addChild(indirectDrawMultiviewGroup);

			{
				tcu::TestCaseGroup* indirectDrawFirstInstanceGroup				= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_first_instance", "Draws geometry with different first instance in one commandbuffer");
				tcu::TestCaseGroup* indirectDrawCountFirstInstanceGroup			= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_count_first_instance", "Draws geometry with VK_KHR_draw_indirect_count extension with different first instance in one commandbuffer");
				tcu::TestCaseGroup* indirectDrawParamCountFirstInstanceGroup	= new tcu::TestCaseGroup(m_testCtx, "indirect_draw_param_count_first_instance", "Draws geometry with VK_KHR_draw_indirect_count extension with different first instance in one commandbuffer and limit draws count with call parameter");
				{
					IndirectDraw::TestSpec testSpec(m_groupParams);
					testSpec.testFirstInstanceNdx = true;
					testSpec.drawType = static_cast<DrawType>(drawTypeIdx);
					testSpec.dataFromCompute = dataFromCompute;
					testSpec.bindIndexBufferOffset = bindIndexBufferOffset;
					testSpec.indexBufferAllocOffset = indexBufferAllocOffset;
					testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetchInstanceIndex.vert";
					testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
					if (dataFromCompute)
						testSpec.shaders[glu::SHADERTYPE_COMPUTE] = "vulkan/draw/NegateData.comp";

					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
						(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));

					testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
						(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));

					testSpec.testIndirectCountExt = IndirectCountType::PARAM_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawParamCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
						(m_testCtx, "triangle_list", "Draws triangle list", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawParamCountFirstInstanceGroup->addChild(new InstanceFactory<IndirectDraw, FunctionSupport1<IndirectDraw::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws triangle strip", testSpec, FunctionSupport1<IndirectDraw::TestSpec>::Args(checkSupport, testSpec)));
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
					typedef IndirectDrawInstanced<FirstInstanceNotSupported> IDFirstInstanceNotSupported;

					IDFirstInstanceNotSupported::TestSpec testSpec(m_groupParams);
					testSpec.drawType = static_cast<DrawType>(drawTypeIdx);
					testSpec.dataFromCompute = dataFromCompute;
					testSpec.bindIndexBufferOffset = bindIndexBufferOffset;
					testSpec.indexBufferAllocOffset = indexBufferAllocOffset;

					testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetchInstanced.vert";
					testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
					if (dataFromCompute)
						testSpec.shaders[glu::SHADERTYPE_COMPUTE] = "vulkan/draw/NegateData.comp";

					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawNoFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceNotSupported, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec> >
						(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawNoFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceNotSupported, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec>::Args(checkSupport, testSpec)));

					testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawCountNoFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceNotSupported, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec> >
						(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawCountNoFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceNotSupported, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec>::Args(checkSupport, testSpec)));

					testSpec.testIndirectCountExt = IndirectCountType::PARAM_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawParamCountNoFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceNotSupported, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec> >
						(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawParamCountNoFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceNotSupported, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport1<IDFirstInstanceNotSupported::TestSpec>::Args(checkSupport, testSpec)));
				}
				indirectDrawInstancedGroup->addChild(indirectDrawNoFirstInstanceGroup);
				indirectDrawCountInstancedGroup->addChild(indirectDrawCountNoFirstInstanceGroup);
				indirectDrawParamCountInstancedGroup->addChild(indirectDrawParamCountNoFirstInstanceGroup);

				tcu::TestCaseGroup*	indirectDrawFirstInstanceGroup				= new tcu::TestCaseGroup(m_testCtx, "first_instance", "Use drawIndirectFirstInstance optional feature");
				tcu::TestCaseGroup*	indirectDrawCountFirstInstanceGroup			= new tcu::TestCaseGroup(m_testCtx, "first_instance", "Use drawIndirectFirstInstance optional feature");
				tcu::TestCaseGroup*	indirectDrawParamCountFirstInstanceGroup	= new tcu::TestCaseGroup(m_testCtx, "first_instance", "Use drawIndirectFirstInstance optional feature");
				{
					typedef IndirectDrawInstanced<FirstInstanceSupported> IDFirstInstanceSupported;

					IDFirstInstanceSupported::TestSpec testSpec(m_groupParams);
					testSpec.drawType = static_cast<DrawType>(drawTypeIdx);
					testSpec.dataFromCompute = dataFromCompute;
					testSpec.bindIndexBufferOffset = bindIndexBufferOffset;
					testSpec.indexBufferAllocOffset = indexBufferAllocOffset;

					testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/draw/VertexFetchInstancedFirstInstance.vert";
					testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
					if (dataFromCompute)
						testSpec.shaders[glu::SHADERTYPE_COMPUTE] = "vulkan/draw/NegateData.comp";

					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceSupported, FunctionSupport1<IDFirstInstanceSupported::TestSpec> >
						(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport1<IDFirstInstanceSupported::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceSupported, FunctionSupport1<IDFirstInstanceSupported::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport1<IDFirstInstanceSupported::TestSpec>::Args(checkSupport, testSpec)));

					testSpec.testIndirectCountExt = IndirectCountType::BUFFER_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawCountFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceSupported, FunctionSupport1<IDFirstInstanceSupported::TestSpec> >
						(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport1<IDFirstInstanceSupported::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawCountFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceSupported, FunctionSupport1<IDFirstInstanceSupported::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport1<IDFirstInstanceSupported::TestSpec>::Args(checkSupport, testSpec)));

					testSpec.testIndirectCountExt = IndirectCountType::PARAM_LIMIT;
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					indirectDrawParamCountFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceSupported, FunctionSupport1<IDFirstInstanceSupported::TestSpec> >
						(m_testCtx, "triangle_list", "Draws an instanced triangle list", testSpec, FunctionSupport1<IDFirstInstanceSupported::TestSpec>::Args(checkSupport, testSpec)));
					testSpec.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					indirectDrawParamCountFirstInstanceGroup->addChild(new InstanceFactory<IDFirstInstanceSupported, FunctionSupport1<IDFirstInstanceSupported::TestSpec> >
						(m_testCtx, "triangle_strip", "Draws an instanced triangle strip", testSpec, FunctionSupport1<IDFirstInstanceSupported::TestSpec>::Args(checkSupport, testSpec)));
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
