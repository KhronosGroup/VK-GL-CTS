/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Mesh Shader Query Tests for VK_EXT_mesh_shader
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderQueryTestsEXT.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deRandom.hpp"
#include "deUniquePtr.hpp"

#include <vector>
#include <algorithm>
#include <sstream>
#include <string>
#include <numeric>
#include <array>
#include <limits>

namespace vkt
{
namespace MeshShader
{

namespace
{

using namespace vk;

using BufferWithMemoryPtr = de::MovePtr<BufferWithMemory>;

constexpr uint32_t kImageWidth				= 32u;
constexpr uint32_t kMeshWorkGroupsPerCall	= 4u;
constexpr uint32_t kTaskWorkGroupsPerCall	= 2u;
constexpr uint32_t kMeshWorkGroupsPerTask	= kMeshWorkGroupsPerCall / kTaskWorkGroupsPerCall;

constexpr uint32_t kMeshLocalInvocationsX	= 10u;
constexpr uint32_t kMeshLocalInvocationsY	= 4u;
constexpr uint32_t kMeshLocalInvocationsZ	= 1u;
constexpr uint32_t kMeshLocalInvocations	= kMeshLocalInvocationsX * kMeshLocalInvocationsY * kMeshLocalInvocationsZ;

constexpr uint32_t kTaskLocalInvocationsX	= 1u;
constexpr uint32_t kTaskLocalInvocationsY	= 4u;
constexpr uint32_t kTaskLocalInvocationsZ	= 6u;
constexpr uint32_t kTaskLocalInvocations	= kTaskLocalInvocationsX * kTaskLocalInvocationsY * kTaskLocalInvocationsZ;

constexpr VkDeviceSize k64sz				= static_cast<VkDeviceSize>(sizeof(uint64_t));
constexpr VkDeviceSize k32sz				= static_cast<VkDeviceSize>(sizeof(uint32_t));

enum class QueryType
{
	PRIMITIVES = 0,
	TASK_INVOCATIONS,
	MESH_INVOCATIONS,
};

enum class DrawCallType
{
	DIRECT = 0,
	INDIRECT,
	INDIRECT_WITH_COUNT,
};

enum class GeometryType
{
	POINTS = 0,
	LINES,
	TRIANGLES,
};

std::string toString (GeometryType geometryType)
{
	std::string result;
	switch (geometryType)
	{
	case GeometryType::POINTS:		result = "points";		break;
	case GeometryType::LINES:		result = "lines";		break;
	case GeometryType::TRIANGLES:	result = "triangles";	break;
	default:
		DE_ASSERT(false);
		break;
	}
	return result;
}

uint32_t vertsPerPrimitive (GeometryType geometryType)
{
	uint32_t vertices = 0u;
	switch (geometryType)
	{
	case GeometryType::POINTS:		vertices = 1u;	break;
	case GeometryType::LINES:		vertices = 2u;	break;
	case GeometryType::TRIANGLES:	vertices = 3u;	break;
	default:
		DE_ASSERT(false);
		break;
	}
	return vertices;
}

enum class ResetCase
{
	NONE = 0,
	NONE_WITH_HOST, // After checking results normally, reset query from the host and verify availability.
	BEFORE_ACCESS,
	AFTER_ACCESS,
};

enum class AccessMethod
{
	COPY = 0,
	GET,
};

void checkGetQueryRes(VkResult result, bool allowNotReady)
{
	if (result == VK_SUCCESS || (result == VK_NOT_READY && allowNotReady))
		return;

	const auto msg = getResultStr(result);
	TCU_FAIL(msg.toString());
}

// The pseudrandom number generator will be used in the test case and test instance, so we use two seeds per case.
uint32_t getNewSeed (void)
{
	static uint32_t seed = 1656078156u;
	uint32_t returnedSeed = seed;
	seed += 2u;
	return returnedSeed;
}

struct TestParams
{
	uint32_t				randomSeed;
	std::vector<QueryType>	queryTypes;
	std::vector<uint32_t>	drawBlocks;
	DrawCallType			drawCall;
	GeometryType			geometry;
	ResetCase				resetType;
	AccessMethod			access;
	bool					use64Bits;
	bool					availabilityBit;
	bool					waitBit;
	bool					useTaskShader;
	bool					insideRenderPass;
	bool					useSecondary;
	bool					multiView;

	void swap (TestParams& other)
	{
		std::swap(randomSeed, other.randomSeed);
		queryTypes.swap(other.queryTypes);
		drawBlocks.swap(other.drawBlocks);
		std::swap(drawCall, other.drawCall);
		std::swap(geometry, other.geometry);
		std::swap(resetType, other.resetType);
		std::swap(access, other.access);
		std::swap(use64Bits, other.use64Bits);
		std::swap(availabilityBit, other.availabilityBit);
		std::swap(waitBit, other.waitBit);
		std::swap(useTaskShader, other.useTaskShader);
		std::swap(insideRenderPass, other.insideRenderPass);
		std::swap(useSecondary, other.useSecondary);
		std::swap(multiView, other.multiView);
	}

	TestParams ()
		: randomSeed		(getNewSeed())
		, queryTypes		()
		, drawBlocks		()
		, drawCall			(DrawCallType::DIRECT)
		, geometry			(GeometryType::POINTS)
		, resetType			(ResetCase::NONE)
		, access			(AccessMethod::COPY)
		, use64Bits			(false)
		, availabilityBit	(false)
		, waitBit			(false)
		, useTaskShader		(false)
		, insideRenderPass	(false)
		, useSecondary		(false)
		, multiView			(false)
	{}

	TestParams (const TestParams& other)
		: randomSeed		(other.randomSeed)
		, queryTypes		(other.queryTypes)
		, drawBlocks		(other.drawBlocks)
		, drawCall			(other.drawCall)
		, geometry			(other.geometry)
		, resetType			(other.resetType)
		, access			(other.access)
		, use64Bits			(other.use64Bits)
		, availabilityBit	(other.availabilityBit)
		, waitBit			(other.waitBit)
		, useTaskShader		(other.useTaskShader)
		, insideRenderPass	(other.insideRenderPass)
		, useSecondary		(other.useSecondary)
		, multiView			(other.multiView)
	{}

	TestParams (TestParams&& other)
		: TestParams()
	{
		this->swap(other);
	}

	uint32_t getTotalDrawCount (void) const
	{
		const uint32_t callCount = std::accumulate(drawBlocks.begin(), drawBlocks.end(), 0u);
		return callCount;
	}

	uint32_t getImageHeight (void) const
	{
		return getTotalDrawCount() * kMeshWorkGroupsPerCall;
	}

	// The goal is dispatching 4 mesh work groups per draw call in total. When not using task shaders, we dispatch that number
	// directly. When using task shaders, we dispatch 2 task work groups that will dispatch 2 mesh work groups each. The axis will
	// be pseudorandomly chosen in each case.
	uint32_t getDrawGroupCount (void) const
	{
		return (useTaskShader ? kTaskWorkGroupsPerCall : kMeshWorkGroupsPerCall);
	}

	// Gets the right query result flags for the current parameters.
	VkQueryResultFlags getQueryResultFlags (void) const
	{
		const VkQueryResultFlags queryResultFlags =	( (use64Bits		? VK_QUERY_RESULT_64_BIT				: 0)
													| (availabilityBit	? VK_QUERY_RESULT_WITH_AVAILABILITY_BIT	: 0)
													| (waitBit			? VK_QUERY_RESULT_WAIT_BIT				: VK_QUERY_RESULT_PARTIAL_BIT) );
		return queryResultFlags;
	}

	// Queries will be inherited if they are started outside of a render pass and using secondary command buffers.
	// - If secondary command buffers are not used, nothing will be inherited.
	// - If secondary command buffers are used but queries start inside of a render pass, queries will run entirely inside the secondary command buffer.
	bool areQueriesInherited (void) const
	{
		return (useSecondary && !insideRenderPass);
	}

protected:
	bool hasQueryType (QueryType queryType) const
	{
		return de::contains(queryTypes.begin(), queryTypes.end(), queryType);
	}

public:
	bool hasPrimitivesQuery (void) const
	{
		return hasQueryType(QueryType::PRIMITIVES);
	}

	bool hasMeshInvStat (void) const
	{
		return hasQueryType(QueryType::MESH_INVOCATIONS);
	}

	bool hasTaskInvStat (void) const
	{
		return hasQueryType(QueryType::TASK_INVOCATIONS);
	}

	struct QuerySizesAndOffsets
	{
		VkDeviceSize queryItemSize;
		VkDeviceSize primitivesQuerySize;
		VkDeviceSize statsQuerySize;
		VkDeviceSize statsQueryOffset;
	};

	uint32_t getViewCount (void) const
	{
		return (multiView ? 2u : 1u);
	}

	QuerySizesAndOffsets getQuerySizesAndOffsets (void) const
	{
		QuerySizesAndOffsets	sizesAndOffsets;
		const VkDeviceSize		extraQueryItems		= (availabilityBit ? 1ull : 0ull);
		const VkDeviceSize		viewMultiplier		= getViewCount();

		sizesAndOffsets.queryItemSize		= (use64Bits ? k64sz : k32sz);
		sizesAndOffsets.primitivesQuerySize	= (extraQueryItems + 1ull) * sizesAndOffsets.queryItemSize;
		sizesAndOffsets.statsQuerySize		= (extraQueryItems + (hasTaskInvStat() ? 1ull : 0ull) + (hasMeshInvStat() ? 1ull : 0ull)) * sizesAndOffsets.queryItemSize;
		sizesAndOffsets.statsQueryOffset	= (hasPrimitivesQuery() ? (sizesAndOffsets.primitivesQuerySize * viewMultiplier) : 0ull);

		return sizesAndOffsets;
	}
};

class MeshQueryCase : public vkt::TestCase
{
public:
					MeshQueryCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, TestParams&& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(std::move(params))
						{}
	virtual			~MeshQueryCase	(void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

protected:
	TestParams m_params;
};

class MeshQueryInstance : public vkt::TestInstance
{
public:
						MeshQueryInstance		(Context& context, const TestParams& params)
							: vkt::TestInstance		(context)
							, m_params				(&params)
							, m_rnd					(params.randomSeed + 1u) // Add 1 to make the instance seed different.
							, m_indirectBuffer		()
							, m_indirectCountBuffer	()
							, m_fence				(createFence(context.getDeviceInterface(), context.getDevice()))
							{}
	virtual				~MeshQueryInstance		(void) {}

	Move<VkRenderPass>	makeCustomRenderPass	(const DeviceInterface& vkd, VkDevice device, uint32_t layerCount, VkFormat format);
	tcu::TestStatus		iterate					(void) override;

protected:
	VkDrawMeshTasksIndirectCommandEXT	getRandomShuffle	(uint32_t groupCount);
	void								recordDraws			(const VkCommandBuffer cmdBuffer, const VkPipeline pipeline, const VkPipelineLayout layout);
	void								beginFirstQueries	(const VkCommandBuffer cmdBuffer, const std::vector<VkQueryPool>& queryPools) const;
	void								endFirstQueries		(const VkCommandBuffer cmdBuffer, const std::vector<VkQueryPool>& queryPools) const;
	void								resetFirstQueries	(const VkCommandBuffer cmdBuffer, const std::vector<VkQueryPool>& queryPools, const uint32_t queryCount) const;
	void								submitCommands		(const VkCommandBuffer cmdBuffer) const;
	void								waitForFence		() const;

	const TestParams*					m_params;
	de::Random							m_rnd;
	BufferWithMemoryPtr					m_indirectBuffer;
	BufferWithMemoryPtr					m_indirectCountBuffer;
	Move<VkFence>						m_fence;
};

void MeshQueryCase::initPrograms (vk::SourceCollections &programCollection) const
{
	const auto meshBuildOpts	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto imageHeight		= m_params.getImageHeight();

	const std::string taskDataDecl =
		"struct TaskData {\n"
		"    uint branch[" + std::to_string(kTaskLocalInvocations) + "];\n"
		"    uint drawIndex;\n"
		"};\n"
		"taskPayloadSharedEXT TaskData td;\n"
		;

	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< (m_params.multiView ? "#extension GL_EXT_multiview : enable\n" : "")
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main (void) { outColor = vec4(0.0, " << (m_params.multiView ? "float(gl_ViewIndex)" : "0.0") << ", 1.0, 1.0); }\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

	std::ostringstream mesh;
	mesh
		<< "#version 460\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kMeshLocalInvocationsX << ", local_size_y=" << kMeshLocalInvocationsY << ", local_size_z=" << kMeshLocalInvocationsZ << ") in;\n"
		<< "layout (" << toString(m_params.geometry) << ") out;\n"
		<< "layout (max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (push_constant, std430) uniform PushConstants {\n"
		<< "	uint prevDrawCalls;\n"
		<< "} pc;\n"
		<< "\n"
		;

	if (m_params.useTaskShader)
		mesh << taskDataDecl << "\n";

	mesh
		<< "\n"
		<< "shared uint currentCol;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    atomicExchange(currentCol, 0u);\n"
		<< "    barrier();\n"
		<< "\n"
		<< "    const uint colCount = uint(" << kImageWidth << ");\n"
		<< "    const uint rowCount = uint(" << imageHeight << ");\n"
		<< "    const uint rowsPerDraw = uint(" << kMeshWorkGroupsPerCall << ");\n"
		<< "\n"
		<< "    const float pixWidth = 2.0 / float(colCount);\n"
		<< "    const float pixHeight = 2.0 / float(rowCount);\n"
		<< "    const float horDelta = pixWidth / 4.0;\n"
		<< "    const float verDelta = pixHeight / 4.0;\n"
		<< "\n"
		<< "    const uint DrawIndex = " << (m_params.useTaskShader ? "td.drawIndex" : "uint(gl_DrawID)") << ";\n"
		<< "    const uint currentWGIndex = (" << (m_params.useTaskShader ? "2u * td.branch[min(gl_LocalInvocationIndex, " + std::to_string(kTaskLocalInvocations - 1u) + ")] + " : "") << "gl_WorkGroupID.x + gl_WorkGroupID.y + gl_WorkGroupID.z);\n"
		<< "    const uint row = (pc.prevDrawCalls + DrawIndex) * rowsPerDraw + currentWGIndex;\n"
		<< "    const uint vertsPerPrimitive = " << vertsPerPrimitive(m_params.geometry) << ";\n"
		<< "\n"
		<< "    SetMeshOutputsEXT(colCount * vertsPerPrimitive, colCount);\n"
		<< "\n"
		<< "    const uint col = atomicAdd(currentCol, 1);\n"
		<< "    if (col < colCount)\n"
		<< "    {\n"
		<< "        const float xCenter = (float(col) + 0.5) / colCount * 2.0 - 1.0;\n"
		<< "        const float yCenter = (float(row) + 0.5) / rowCount * 2.0 - 1.0;\n"
		<< "\n"
		<< "        const uint firstVert = col * vertsPerPrimitive;\n"
		<< "\n"
		;

	switch (m_params.geometry)
	{
	case GeometryType::POINTS:
		mesh
			<< "        gl_MeshVerticesEXT[firstVert].gl_Position = vec4(xCenter, yCenter, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesEXT[firstVert].gl_PointSize = 1.0;\n"
			<< "        gl_PrimitivePointIndicesEXT[col] = firstVert;\n"
			;
		break;
	case GeometryType::LINES:
		mesh
			<< "        gl_MeshVerticesEXT[firstVert + 0].gl_Position = vec4(xCenter - horDelta, yCenter, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesEXT[firstVert + 1].gl_Position = vec4(xCenter + horDelta, yCenter, 0.0, 1.0);\n"
			<< "        gl_PrimitiveLineIndicesEXT[col] = uvec2(firstVert, firstVert + 1);\n"
			;
		break;
	case GeometryType::TRIANGLES:
		mesh
			<< "        gl_MeshVerticesEXT[firstVert + 0].gl_Position = vec4(xCenter           , yCenter - verDelta, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesEXT[firstVert + 1].gl_Position = vec4(xCenter - horDelta, yCenter + verDelta, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesEXT[firstVert + 2].gl_Position = vec4(xCenter + horDelta, yCenter + verDelta, 0.0, 1.0);\n"
			<< "        gl_PrimitiveTriangleIndicesEXT[col] = uvec3(firstVert, firstVert + 1, firstVert + 2);\n"
			;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	mesh
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << meshBuildOpts;

	if (m_params.useTaskShader)
	{
		// See TestParams::getDrawGroupCount().
		de::Random				rnd				(m_params.randomSeed);
		std::vector<uint32_t>	meshTaskCount	{kMeshWorkGroupsPerTask, 1u, 1u};

		rnd.shuffle(meshTaskCount.begin(), meshTaskCount.end());

		std::ostringstream task;
		task
			<< "#version 460\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << kTaskLocalInvocationsX << ", local_size_y=" << kTaskLocalInvocationsY << ", local_size_z=" << kTaskLocalInvocationsZ << ") in;\n"
			<< "\n"
			<< taskDataDecl
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "   td.branch[gl_LocalInvocationIndex] = gl_WorkGroupID.x + gl_WorkGroupID.y + gl_WorkGroupID.z;\n"
			<< "   td.drawIndex = uint(gl_DrawID);\n"
			<< "   EmitMeshTasksEXT(" << meshTaskCount.at(0) << ", " << meshTaskCount.at(1) << ", " << meshTaskCount.at(2) << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << meshBuildOpts;
	}
}

TestInstance* MeshQueryCase::createInstance (Context& context) const
{
	return new MeshQueryInstance(context, m_params);
}

void MeshQueryCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportEXT(context, m_params.useTaskShader/*requireTask*/, true/*requireMesh*/);

	const auto& meshFeatures = context.getMeshShaderFeaturesEXT();
	if (!meshFeatures.meshShaderQueries)
		TCU_THROW(NotSupportedError, "meshShaderQueries not supported");

	if (m_params.areQueriesInherited())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_INHERITED_QUERIES);

	if (m_params.resetType == ResetCase::NONE_WITH_HOST)
		context.requireDeviceFunctionality("VK_EXT_host_query_reset");

	if (m_params.multiView)
	{
		if (!meshFeatures.multiviewMeshShader)
			TCU_THROW(NotSupportedError, "multiviewMeshShader not supported");

		const auto& meshProperties = context.getMeshShaderPropertiesEXT();
		if (meshProperties.maxMeshMultiviewViewCount < m_params.getViewCount())
			TCU_THROW(NotSupportedError, "maxMeshMultiviewViewCount too low");
	}
}

VkDrawMeshTasksIndirectCommandEXT MeshQueryInstance::getRandomShuffle (uint32_t groupCount)
{
	std::array<uint32_t, 3> counts { groupCount, 1u, 1u };
	m_rnd.shuffle(counts.begin(), counts.end());

	const VkDrawMeshTasksIndirectCommandEXT result { counts[0], counts[1], counts[2] };
	return result;
}

void MeshQueryInstance::recordDraws (const VkCommandBuffer cmdBuffer, const VkPipeline pipeline, const VkPipelineLayout layout)
{
	const auto&	vkd				= m_context.getDeviceInterface();
	const auto	device			= m_context.getDevice();
	auto&		alloc			= m_context.getDefaultAllocator();
	const auto	drawGroupCount	= m_params->getDrawGroupCount();
	const auto	pcSize			= static_cast<uint32_t>(sizeof(uint32_t));

	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	if (m_params->drawCall == DrawCallType::DIRECT)
	{
		uint32_t totalDrawCalls = 0u;
		for (const auto& blockSize : m_params->drawBlocks)
		{
			for (uint32_t drawIdx = 0u; drawIdx < blockSize; ++drawIdx)
			{
				const auto counts = getRandomShuffle(drawGroupCount);
				vkd.cmdPushConstants(cmdBuffer, layout, VK_SHADER_STAGE_MESH_BIT_EXT, 0u, pcSize, &totalDrawCalls);
				vkd.cmdDrawMeshTasksEXT(cmdBuffer, counts.groupCountX, counts.groupCountY, counts.groupCountZ);
				++totalDrawCalls;
			}
		}
	}
	else if (m_params->drawCall == DrawCallType::INDIRECT || m_params->drawCall == DrawCallType::INDIRECT_WITH_COUNT)
	{
		if (m_params->drawBlocks.empty())
			return;

		const auto totalDrawCount	= m_params->getTotalDrawCount();
		const auto cmdSize			= static_cast<uint32_t>(sizeof(VkDrawMeshTasksIndirectCommandEXT));

		std::vector<VkDrawMeshTasksIndirectCommandEXT> indirectCommands;
		indirectCommands.reserve(totalDrawCount);

		for (uint32_t i = 0u; i < totalDrawCount; ++i)
			indirectCommands.emplace_back(getRandomShuffle(drawGroupCount));

		// Copy the array to a host-visible buffer.
		// Note: We make sure all indirect buffers are allocated with a non-zero size by adding cmdSize to the expected size.
		// Size of buffer must be greater than stride * (maxDrawCount - 1) + offset + sizeof(VkDrawMeshTasksIndirectCommandEXT) so we multiply by 2
		const auto indirectBufferSize		= de::dataSize(indirectCommands);
		const auto indirectBufferCreateInfo	= makeBufferCreateInfo(static_cast<VkDeviceSize>((indirectBufferSize + cmdSize) * 2), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

		m_indirectBuffer			= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, indirectBufferCreateInfo, MemoryRequirement::HostVisible));
		auto& indirectBufferAlloc	= m_indirectBuffer->getAllocation();
		void* indirectBufferData	= indirectBufferAlloc.getHostPtr();

		deMemcpy(indirectBufferData, indirectCommands.data(), indirectBufferSize);
		flushAlloc(vkd, device, indirectBufferAlloc);

		if (m_params->drawCall == DrawCallType::INDIRECT)
		{
			uint32_t accumulatedCount = 0u;

			for (const auto& blockSize : m_params->drawBlocks)
			{
				const auto offset = static_cast<VkDeviceSize>(cmdSize * accumulatedCount);
				vkd.cmdPushConstants(cmdBuffer, layout, VK_SHADER_STAGE_MESH_BIT_EXT, 0u, pcSize, &accumulatedCount);
				vkd.cmdDrawMeshTasksIndirectEXT(cmdBuffer, m_indirectBuffer->get(), offset, blockSize, cmdSize);
				accumulatedCount += blockSize;
			}
		}
		else
		{
			// Copy the "block sizes" to a host-visible buffer.
			const auto indirectCountBufferSize			= de::dataSize(m_params->drawBlocks);
			const auto indirectCountBufferCreateInfo	= makeBufferCreateInfo(static_cast<VkDeviceSize>(indirectCountBufferSize + cmdSize), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

			m_indirectCountBuffer = BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, indirectCountBufferCreateInfo, MemoryRequirement::HostVisible));
			auto& indirectCountBufferAlloc = m_indirectCountBuffer->getAllocation();
			void* indirectCountBufferData = indirectCountBufferAlloc.getHostPtr();

			deMemcpy(indirectCountBufferData, m_params->drawBlocks.data(), indirectCountBufferSize);
			flushAlloc(vkd, device, indirectCountBufferAlloc);

			// Record indirect draws with count.
			uint32_t accumulatedCount = 0u;

			for (uint32_t countIdx = 0u; countIdx < m_params->drawBlocks.size(); ++countIdx)
			{
				const auto&	blockSize	= m_params->drawBlocks.at(countIdx);
				const auto	offset		= static_cast<VkDeviceSize>(cmdSize * accumulatedCount);
				const auto	countOffset	= static_cast<VkDeviceSize>(sizeof(uint32_t) * countIdx);

				vkd.cmdPushConstants(cmdBuffer, layout, VK_SHADER_STAGE_MESH_BIT_EXT, 0u, pcSize, &accumulatedCount);
				vkd.cmdDrawMeshTasksIndirectCountEXT(cmdBuffer, m_indirectBuffer->get(), offset, m_indirectCountBuffer->get(), countOffset, blockSize * 2u, cmdSize);
				accumulatedCount += blockSize;
			}
		}
	}
	else
	{
		DE_ASSERT(false);
	}
}

void MeshQueryInstance::beginFirstQueries (const VkCommandBuffer cmdBuffer, const std::vector<VkQueryPool>& queryPools) const
{
	const auto& vkd = m_context.getDeviceInterface();
	for (const auto& pool : queryPools)
		vkd.cmdBeginQuery(cmdBuffer, pool, 0u, 0u);
}

void MeshQueryInstance::endFirstQueries (const VkCommandBuffer cmdBuffer, const std::vector<VkQueryPool>& queryPools) const
{
	const auto& vkd = m_context.getDeviceInterface();
	for (const auto& pool : queryPools)
		vkd.cmdEndQuery(cmdBuffer, pool, 0u);
}

void MeshQueryInstance::resetFirstQueries (const VkCommandBuffer cmdBuffer, const std::vector<VkQueryPool>& queryPools, const uint32_t queryCount) const
{
	const auto& vkd = m_context.getDeviceInterface();
	for (const auto& pool : queryPools)
		vkd.cmdResetQueryPool(cmdBuffer, pool, 0u, queryCount);
}

void MeshQueryInstance::submitCommands (const VkCommandBuffer cmdBuffer) const
{
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	queue	= m_context.getUniversalQueue();

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		nullptr,						// const void*					pNext;
		0u,								// deUint32						waitSemaphoreCount;
		nullptr,						// const VkSemaphore*			pWaitSemaphores;
		nullptr,						// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,								// deUint32						commandBufferCount;
		&cmdBuffer,						// const VkCommandBuffer*		pCommandBuffers;
		0u,								// deUint32						signalSemaphoreCount;
		nullptr,						// const VkSemaphore*			pSignalSemaphores;
	};

	VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, m_fence.get()));
}

void MeshQueryInstance::waitForFence (void) const
{
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();

	VK_CHECK(vkd.waitForFences(device, 1u, &m_fence.get(), VK_TRUE, ~0ull));
}

// Read query item from memory. Always returns uint64_t for convenience. Advances pointer to the next item.
uint64_t readFromPtrAndAdvance (uint8_t** const ptr, VkDeviceSize itemSize)
{
	const auto	itemSizeSz	= static_cast<size_t>(itemSize);
	uint64_t	result		= std::numeric_limits<uint64_t>::max();

	if (itemSize == k64sz)
	{
		deMemcpy(&result, *ptr, itemSizeSz);
	}
	else if (itemSize == k32sz)
	{
		uint32_t aux = std::numeric_limits<uint32_t>::max();
		deMemcpy(&aux, *ptr, itemSizeSz);
		result = static_cast<uint64_t>(aux);
	}
	else
		DE_ASSERT(false);

	*ptr += itemSizeSz;
	return result;
}

// General procedure to verify correctness of the availability bit, which does not depend on the exact query.
void readAndVerifyAvailabilityBit (uint8_t** const resultsPtr, VkDeviceSize itemSize, const TestParams& params, const std::string& queryName)
{
	const uint64_t availabilityBitVal = readFromPtrAndAdvance(resultsPtr, itemSize);

	if (params.resetType == ResetCase::BEFORE_ACCESS)
	{
		if (availabilityBitVal)
		{
			std::ostringstream msg;
			msg << queryName << " availability bit expected to be zero due to reset before access, but found " << availabilityBitVal;
			TCU_FAIL(msg.str());
		}
	}
	else if (params.waitBit)
	{
		if (!availabilityBitVal)
		{
			std::ostringstream msg;
			msg << queryName << " availability expected to be true due to wait bit and not previous reset, but found " << availabilityBitVal;
			TCU_FAIL(msg.str());
		}
	}
}

// Verifies a query counter has the right value given the test parameters.
// - readVal is the reported counter value.
// - expectedMinVal and expectedMaxVal are the known right counts under "normal" circumstances.
// - The actual range of valid values will be adjusted depending on the test parameters (wait bit, reset, etc).
void verifyQueryCounter (uint64_t readVal, uint64_t expectedMinVal, uint64_t expectedMaxVal, const TestParams& params, const std::string& queryName)
{
	uint64_t minVal = expectedMinVal;
	uint64_t maxVal = expectedMaxVal;

	// Resetting a query via vkCmdResetQueryPool or vkResetQueryPool sets the status to unavailable and makes the numerical results undefined.
	const bool wasReset = (params.resetType == ResetCase::BEFORE_ACCESS);

	if (!wasReset)
	{
		if (!params.waitBit)
			minVal = 0ull;

		if (!de::inRange(readVal, minVal, maxVal))
		{
			std::ostringstream msg;
			msg << queryName << " not in expected range: " << readVal << " out of [" << minVal << ", " << maxVal << "]";
			TCU_FAIL(msg.str());
		}
	}
}

Move<VkRenderPass> MeshQueryInstance::makeCustomRenderPass (const DeviceInterface& vkd, VkDevice device, uint32_t layerCount, VkFormat format)
{
	DE_ASSERT(layerCount > 0u);

	const VkAttachmentDescription colorAttachmentDescription =
	{
		0u,											// VkAttachmentDescriptionFlags    flags
		format,										// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout                   finalLayout
	};

	const VkAttachmentReference colorAttachmentRef = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	const VkSubpassDescription subpassDescription =
	{
		0u,									// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		nullptr,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&colorAttachmentRef,				// const VkAttachmentReference*    pColorAttachments
		nullptr,							// const VkAttachmentReference*    pResolveAttachments
		nullptr,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		nullptr								// const deUint32*                 pPreserveAttachments
	};

	const uint32_t viewMask = ((1u << layerCount) - 1u);
	const VkRenderPassMultiviewCreateInfo multiviewCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,	//	VkStructureType	sType;
		nullptr,												//	const void*		pNext;
		1u,														//	uint32_t		subpassCount;
		&viewMask,												//	const uint32_t*	pViewMasks;
		0u,														//	uint32_t		dependencyCount;
		nullptr,												//	const int32_t*	pViewOffsets;
		1u,														//	uint32_t		correlationMaskCount;
		&viewMask,												//	const uint32_t*	pCorrelationMasks;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureType                   sType
		&multiviewCreateInfo,									// const void*                       pNext
		0u,														// VkRenderPassCreateFlags           flags
		1u,														// deUint32                          attachmentCount
		&colorAttachmentDescription,							// const VkAttachmentDescription*    pAttachments
		1u,														// deUint32                          subpassCount
		&subpassDescription,									// const VkSubpassDescription*       pSubpasses
		0u,														// deUint32                          dependencyCount
		nullptr,												// const VkSubpassDependency*        pDependencies
	};

	return createRenderPass(vkd, device, &renderPassInfo);
}

tcu::TestStatus MeshQueryInstance::iterate (void)
{
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queue			= m_context.getUniversalQueue();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();

	const auto			colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			colorTcuFormat	= mapVkFormat(colorFormat);
	const auto			imageHeight		= m_params->getImageHeight();
	const auto			colorExtent		= makeExtent3D(kImageWidth, std::max(imageHeight, 1u), 1u);
	const auto			viewCount		= m_params->getViewCount();
	const tcu::IVec3	colorTcuExtent	(static_cast<int>(colorExtent.width), static_cast<int>(colorExtent.height), static_cast<int>(viewCount));
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const tcu::Vec4		clearColor		(0.0f, 0.0f, 0.0f, 1.0f);
	const auto			expectedPrims	= (imageHeight * kImageWidth);
	const auto			expectedTaskInv	= (m_params->useTaskShader ? (imageHeight * kTaskLocalInvocations / 2u) : 0u);
	const auto			expectedMeshInv	= imageHeight * kMeshLocalInvocations;
	const auto			imageViewType	= ((viewCount > 1u) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);

	// Color buffer.
	const VkImageCreateInfo colorBufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		colorExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		viewCount,								//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	const ImageWithMemory	colorBuffer	(vkd, device, alloc, colorBufferCreateInfo, MemoryRequirement::Any);
	const auto				colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, viewCount);
	const auto				colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, viewCount);
	const auto				colorView	= makeImageView(vkd, device, colorBuffer.get(), imageViewType, colorFormat, colorSRR);

	// Verification buffer.
	DE_ASSERT(colorExtent.depth == 1u);
	const VkDeviceSize		verifBufferSize			= colorExtent.width * colorExtent.height * viewCount * static_cast<VkDeviceSize>(tcu::getPixelSize(colorTcuFormat));
	const auto				verifBufferCreateInfo	= makeBufferCreateInfo(verifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const BufferWithMemory	verifBuffer				(vkd, device, alloc, verifBufferCreateInfo, MemoryRequirement::HostVisible);

	// Shader modules.
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	taskModule	= (binaries.contains("task")
							? createShaderModule(vkd, device, binaries.get("task"))
							: Move<VkShaderModule>());
	const auto	meshModule	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragModule	= createShaderModule(vkd, device, binaries.get("frag"));

	// Pipeline layout.
	const auto pcSize			= static_cast<uint32_t>(sizeof(uint32_t));
	const auto pcRange			= makePushConstantRange(VK_SHADER_STAGE_MESH_BIT_EXT, 0u, pcSize);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, DE_NULL, &pcRange);

	// Render pass, framebuffer, viewports, scissors.
	const auto renderPass	= makeCustomRenderPass(vkd, device, viewCount, colorFormat);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), colorExtent.width, colorExtent.height);

	const std::vector<VkViewport>	viewports	(1u, makeViewport(colorExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(colorExtent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskModule.get(), meshModule.get(), fragModule.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffers.
	const auto cmdPool			= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr		= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto resetCmdBuffer	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer		= cmdBufferPtr.get();
	const auto rawPipeline		= pipeline.get();
	const auto rawPipeLayout	= pipelineLayout.get();

	Move<VkCommandBuffer>	secCmdBufferPtr;
	VkCommandBuffer			secCmdBuffer = DE_NULL;

	if (m_params->useSecondary)
	{
		secCmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		secCmdBuffer	= secCmdBufferPtr.get();
	}

	// Create the query pools that we need.
	Move<VkQueryPool> primitivesQueryPool;
	Move<VkQueryPool> statsQueryPool;

	const bool hasPrimitivesQuery	= m_params->hasPrimitivesQuery();
	const bool hasMeshInvStat		= m_params->hasMeshInvStat();
	const bool hasTaskInvStat		= m_params->hasTaskInvStat();
	const bool hasStatsQuery		= (hasMeshInvStat || hasTaskInvStat);

	std::vector<VkQueryPool> allQueryPools;

	if (hasPrimitivesQuery)
	{
		const VkQueryPoolCreateInfo queryPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,			//	VkStructureType					sType;
			nullptr,											//	const void*						pNext;
			0u,													//	VkQueryPoolCreateFlags			flags;
			VK_QUERY_TYPE_MESH_PRIMITIVES_GENERATED_EXT,		//	VkQueryType						queryType;
			viewCount,											//	uint32_t						queryCount;
			0u,													//	VkQueryPipelineStatisticFlags	pipelineStatistics;
		};
		primitivesQueryPool = createQueryPool(vkd, device, &queryPoolCreateInfo);
		allQueryPools.push_back(primitivesQueryPool.get());
	}

	const VkQueryPipelineStatisticFlags statQueryFlags =
		( (hasMeshInvStat ? VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT : 0)
		| (hasTaskInvStat ? VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT : 0) );

	if (hasStatsQuery)
	{
		const VkQueryPoolCreateInfo queryPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	//	VkStructureType					sType;
			nullptr,									//	const void*						pNext;
			0u,											//	VkQueryPoolCreateFlags			flags;
			VK_QUERY_TYPE_PIPELINE_STATISTICS,			//	VkQueryType						queryType;
			viewCount,									//	uint32_t						queryCount;
			statQueryFlags,								//	VkQueryPipelineStatisticFlags	pipelineStatistics;
		};
		statsQueryPool = createQueryPool(vkd, device, &queryPoolCreateInfo);
		allQueryPools.push_back(statsQueryPool.get());
	}

	// Some query result parameters.
	const auto		querySizesAndOffsets	= m_params->getQuerySizesAndOffsets();
	const size_t	maxResultSize			= k64sz * 10ull; // 10 items at most: (prim+avail+task+mesh+avail)*2.
	const auto		statsQueryOffsetSz		= static_cast<size_t>(querySizesAndOffsets.statsQueryOffset);

	// Create output buffer for the queries.
	BufferWithMemoryPtr queryResultsBuffer;
	if (m_params->access == AccessMethod::COPY)
	{
		const auto queryResultsBufferInfo = makeBufferCreateInfo(static_cast<VkDeviceSize>(maxResultSize), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		queryResultsBuffer = BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, queryResultsBufferInfo, MemoryRequirement::HostVisible));
	}
	std::vector<uint8_t> queryResultsHostVec(maxResultSize, 0);

	const auto statsDataHostVecPtr	= queryResultsHostVec.data() + statsQueryOffsetSz;
	const auto statsRemainingSize	= maxResultSize - statsQueryOffsetSz;

	// Result flags when obtaining query results.
	const auto queryResultFlags = m_params->getQueryResultFlags();

	// Reset queries before use.
	// Queries will be reset in a separate command buffer to make sure they are always properly reset before use.
	// We could do this with VK_EXT_host_query_reset too.
	{
		beginCommandBuffer(vkd, resetCmdBuffer.get());
		resetFirstQueries(resetCmdBuffer.get(), allQueryPools, viewCount);
		endCommandBuffer(vkd, resetCmdBuffer.get());
		submitCommandsAndWait(vkd, device, queue, resetCmdBuffer.get());
	}

	// Command recording.
	beginCommandBuffer(vkd, cmdBuffer);

	if (m_params->useSecondary)
	{
		const VkCommandBufferInheritanceInfo inheritanceInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,			//	VkStructureType					sType;
			nullptr,													//	const void*						pNext;
			renderPass.get(),											//	VkRenderPass					renderPass;
			0u,															//	uint32_t						subpass;
			framebuffer.get(),											//	VkFramebuffer					framebuffer;
			VK_FALSE,													//	VkBool32						occlusionQueryEnable;
			0u,															//	VkQueryControlFlags				queryFlags;
			(m_params->areQueriesInherited() ? statQueryFlags : 0u),	//	VkQueryPipelineStatisticFlags	pipelineStatistics;
		};

		const auto secCmdBufferFlags = (VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		const VkCommandBufferBeginInfo secBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	//	VkStructureType							sType;
			nullptr,										//	const void*								pNext;
			secCmdBufferFlags,								//	VkCommandBufferUsageFlags				flags;
			&inheritanceInfo,								//	const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
		};

		VK_CHECK(vkd.beginCommandBuffer(secCmdBuffer, &secBeginInfo));
	}

	const auto subpassContents	= (m_params->useSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);

	// 4 cases:
	//
	// * Only primary, inside render pass
	// * Only primary, outside render pass
	// * Primary and secondary, inside render pass (query in secondary)
	// * Primary and secondary, outside render pass (query inheritance)

	if (!m_params->useSecondary)
	{
		if (m_params->insideRenderPass)
		{
			beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor, subpassContents);
				beginFirstQueries(cmdBuffer, allQueryPools);
				recordDraws(cmdBuffer, rawPipeline, rawPipeLayout);
				endFirstQueries(cmdBuffer, allQueryPools);
			endRenderPass(vkd, cmdBuffer);
		}
		else
		{
			DE_ASSERT(!m_params->multiView);
			beginFirstQueries(cmdBuffer, allQueryPools);
			beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor, subpassContents);
				recordDraws(cmdBuffer, rawPipeline, rawPipeLayout);
			endRenderPass(vkd, cmdBuffer);
			endFirstQueries(cmdBuffer, allQueryPools);
		}
	}
	else
	{
		if (m_params->insideRenderPass) // Queries in secondary command buffer.
		{
			beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor, subpassContents);
				beginFirstQueries(secCmdBuffer, allQueryPools);
				recordDraws(secCmdBuffer, rawPipeline, rawPipeLayout);
				endFirstQueries(secCmdBuffer, allQueryPools);
				endCommandBuffer(vkd, secCmdBuffer);
				vkd.cmdExecuteCommands(cmdBuffer, 1u, &secCmdBuffer);
			endRenderPass(vkd, cmdBuffer);
		}
		else // Inherited queries case.
		{
			DE_ASSERT(!m_params->multiView);
			beginFirstQueries(cmdBuffer, allQueryPools);
			beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor, subpassContents);
				recordDraws(secCmdBuffer, rawPipeline, rawPipeLayout);
				endCommandBuffer(vkd, secCmdBuffer);
				vkd.cmdExecuteCommands(cmdBuffer, 1u, &secCmdBuffer);
			endRenderPass(vkd, cmdBuffer);
			endFirstQueries(cmdBuffer, allQueryPools);
		}
	}

	// Render to copy barrier.
	{
		const auto preCopyImgBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.get(), colorSRR);
		cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyImgBarrier);
	}

	if (m_params->resetType == ResetCase::BEFORE_ACCESS)
		resetFirstQueries(cmdBuffer, allQueryPools, viewCount);

	if (m_params->access == AccessMethod::COPY)
	{
		if (hasPrimitivesQuery)
			vkd.cmdCopyQueryPoolResults(cmdBuffer, primitivesQueryPool.get(), 0u, viewCount, queryResultsBuffer->get(), 0ull, querySizesAndOffsets.primitivesQuerySize, queryResultFlags);

		if (hasStatsQuery)
			vkd.cmdCopyQueryPoolResults(cmdBuffer, statsQueryPool.get(), 0u, viewCount, queryResultsBuffer->get(), querySizesAndOffsets.statsQueryOffset, querySizesAndOffsets.statsQuerySize, queryResultFlags);
	}

	if (m_params->resetType == ResetCase::AFTER_ACCESS)
		resetFirstQueries(cmdBuffer, allQueryPools, viewCount);

	// Copy color attachment to verification buffer.
	{
		const auto copyRegion = makeBufferImageCopy(colorExtent, colorSRL);
		vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer.get(), 1u, &copyRegion);
	}

	// This barrier applies to both the color verification buffer and the queries if they were copied.
	const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommands(cmdBuffer);

	// When using GET, obtain results before actually waiting for the fence if possible. This way it's more interesting for cases
	// that do not use the wait bit.
	if (m_params->access == AccessMethod::GET)
	{
		// When resetting queries before access, we need to make sure the reset operation has really taken place.
		if (m_params->resetType == ResetCase::BEFORE_ACCESS)
			waitForFence();

		const bool allowNotReady = !m_params->waitBit;

		if (hasPrimitivesQuery)
		{
			const auto res = vkd.getQueryPoolResults(device, primitivesQueryPool.get(), 0u, viewCount, de::dataSize(queryResultsHostVec), queryResultsHostVec.data(), querySizesAndOffsets.primitivesQuerySize, queryResultFlags);
			checkGetQueryRes(res, allowNotReady);
		}

		if (hasStatsQuery)
		{
			const auto res = vkd.getQueryPoolResults(device, statsQueryPool.get(), 0u, viewCount, statsRemainingSize, statsDataHostVecPtr, querySizesAndOffsets.statsQuerySize, queryResultFlags);
			checkGetQueryRes(res, allowNotReady);
		}
	}

	waitForFence();

	// Verify color buffer.
	{
		auto& log				= m_context.getTestContext().getLog();
		auto& verifBufferAlloc	= verifBuffer.getAllocation();
		void* verifBufferData	= verifBufferAlloc.getHostPtr();

		invalidateAlloc(vkd, device, verifBufferAlloc);

		tcu::ConstPixelBufferAccess	verifAccess		(colorTcuFormat, colorTcuExtent, verifBufferData);
		const tcu::Vec4				threshold		(0.0f, 0.0f, 0.0f, 0.0f); // Results should be exact.

		for (int layer = 0; layer < colorTcuExtent.z(); ++layer)
		{
			// This should match the fragment shader.
			const auto green			= ((layer > 0) ? 1.0f : 0.0f);
			const auto referenceColor	= ((m_params->getTotalDrawCount() > 0u) ? tcu::Vec4(0.0f, green, 1.0f, 1.0f) : clearColor);
			const auto layerAccess		= tcu::getSubregion(verifAccess, 0, 0, layer, colorTcuExtent.x(), colorTcuExtent.y(), 1);

			if (!tcu::floatThresholdCompare(log, "Color Result", "", referenceColor, layerAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
			{
				std::ostringstream msg;
				msg << "Color target mismatch at layer " << layer << "; check log for details";
				TCU_FAIL(msg.str());
			}
		}
	}

	// Verify query results.
	{
		const auto	itemSize	= querySizesAndOffsets.queryItemSize;
		uint8_t*	resultsPtr	= nullptr;

		if (m_params->access == AccessMethod::COPY)
		{
			auto& queryResultsBufferAlloc	= queryResultsBuffer->getAllocation();
			void* queryResultsBufferData	= queryResultsBufferAlloc.getHostPtr();
			invalidateAlloc(vkd, device, queryResultsBufferAlloc);

			resultsPtr = reinterpret_cast<uint8_t*>(queryResultsBufferData);
		}
		else if (m_params->access == AccessMethod::GET)
		{
			resultsPtr = queryResultsHostVec.data();
		}


		if (hasPrimitivesQuery)
		{
			const std::string	queryGroupName		= "Primitive count";
			uint64_t			totalPrimitiveCount	= 0ull;

			for (uint32_t viewIndex = 0u; viewIndex < viewCount; ++viewIndex)
			{
				const std::string	queryName		= queryGroupName + " for view " + std::to_string(viewIndex);
				const uint64_t		primitiveCount	= readFromPtrAndAdvance(&resultsPtr, itemSize);

				totalPrimitiveCount += primitiveCount;

				if (m_params->availabilityBit)
					readAndVerifyAvailabilityBit(&resultsPtr, itemSize, *m_params, queryName);
			}

			verifyQueryCounter(totalPrimitiveCount, expectedPrims, expectedPrims * viewCount, *m_params, queryGroupName);
		}

		if (hasStatsQuery)
		{
			const std::string	queryGroupName	= "Stats query";
			uint64_t			totalTaskInvs	= 0ull;
			uint64_t			totalMeshInvs	= 0ull;

			for (uint32_t viewIndex = 0u; viewIndex < viewCount; ++viewIndex)
			{
				if (hasTaskInvStat)
				{
					const uint64_t taskInvs = readFromPtrAndAdvance(&resultsPtr, itemSize);
					totalTaskInvs += taskInvs;
				}

				if (hasMeshInvStat)
				{
					const uint64_t meshInvs = readFromPtrAndAdvance(&resultsPtr, itemSize);
					totalMeshInvs += meshInvs;
				}

				if (m_params->availabilityBit)
				{
					const std::string queryName = queryGroupName + " for view " + std::to_string(viewIndex);
					readAndVerifyAvailabilityBit(&resultsPtr, itemSize, *m_params, queryGroupName);
				}
			}

			if (hasTaskInvStat)
				verifyQueryCounter(totalTaskInvs, expectedTaskInv, expectedTaskInv * viewCount, *m_params, "Task invocations");

			if (hasMeshInvStat)
				verifyQueryCounter(totalMeshInvs, expectedMeshInv, expectedMeshInv * viewCount, *m_params, "Mesh invocations");
		}
	}

	if (m_params->resetType == ResetCase::NONE_WITH_HOST)
	{
		// We'll reset the different queries that we used before and we'll retrieve results again with GET, forcing availability bit
		// and no wait bit. We'll verify availability bits are zero.
		uint8_t* resultsPtr = queryResultsHostVec.data();

		// New parameters, based on the existing ones, that match the behavior we expect below.
		TestParams postResetParams		= *m_params;
		postResetParams.availabilityBit	= true;
		postResetParams.waitBit			= false;
		postResetParams.resetType		= ResetCase::BEFORE_ACCESS;

		const auto postResetFlags			= postResetParams.getQueryResultFlags();
		const auto newSizesAndOffsets		= postResetParams.getQuerySizesAndOffsets();
		const auto newStatsQueryOffsetSz	= static_cast<size_t>(newSizesAndOffsets.statsQueryOffset);
		const auto newStatsDataHostVecPtr	= queryResultsHostVec.data() + newStatsQueryOffsetSz;
		const auto newStatsRemainingSize	= maxResultSize - newStatsQueryOffsetSz;
		const auto itemSize					= newSizesAndOffsets.queryItemSize;

		if (hasPrimitivesQuery)
		{
			vkd.resetQueryPool(device, primitivesQueryPool.get(), 0u, viewCount);
			const auto res = vkd.getQueryPoolResults(device, primitivesQueryPool.get(), 0u, viewCount, de::dataSize(queryResultsHostVec), queryResultsHostVec.data(), newSizesAndOffsets.primitivesQuerySize, postResetFlags);
			checkGetQueryRes(res, true/*allowNotReady*/);
		}

		if (hasStatsQuery)
		{
			vkd.resetQueryPool(device, statsQueryPool.get(), 0u, viewCount);
			const auto res = vkd.getQueryPoolResults(device, statsQueryPool.get(), 0u, viewCount, newStatsRemainingSize, newStatsDataHostVecPtr, newSizesAndOffsets.statsQuerySize, postResetFlags);
			checkGetQueryRes(res, true/*allowNotReady*/);
		}

		if (hasPrimitivesQuery)
		{
			for (uint32_t viewIndex = 0u; viewIndex < viewCount; ++viewIndex)
			{
				const std::string	queryName		= "Post-reset primitive count for view " + std::to_string(viewIndex);
				const uint64_t		primitiveCount	= readFromPtrAndAdvance(&resultsPtr, itemSize);

				// Resetting a query without beginning it again makes numerical results undefined.
				//verifyQueryCounter(primitiveCount, 0ull, postResetParams, queryName);
				DE_UNREF(primitiveCount);
				readAndVerifyAvailabilityBit(&resultsPtr, itemSize, postResetParams, queryName);
			}
		}

		if (hasStatsQuery)
		{
			for (uint32_t viewIndex = 0u; viewIndex < viewCount; ++viewIndex)
			{
				if (hasTaskInvStat)
				{
					const uint64_t taskInvs = readFromPtrAndAdvance(&resultsPtr, itemSize);
					// Resetting a query without beginning it again makes numerical results undefined.
					//verifyQueryCounter(taskInvs, 0ull, postResetParams, "Post-reset task invocations");
					DE_UNREF(taskInvs);
				}

				if (hasMeshInvStat)
				{
					const uint64_t meshInvs = readFromPtrAndAdvance(&resultsPtr, itemSize);
					// Resetting a query without beginning it again makes numerical results undefined.
					//verifyQueryCounter(meshInvs, 0ull, postResetParams, "Post-reset mesh invocations");
					DE_UNREF(meshInvs);
				}

				const std::string queryName = "Post-reset stats query for view " + std::to_string(viewIndex);
				readAndVerifyAvailabilityBit(&resultsPtr, itemSize, postResetParams, queryName);
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // anonymous

tcu::TestCaseGroup* createMeshShaderQueryTestsEXT (tcu::TestContext& testCtx)
{
	GroupPtr queryGroup (new tcu::TestCaseGroup(testCtx, "query", "Mesh Shader Query Tests"));

	const struct
	{
		std::vector<QueryType>	queryTypes;
		const char*				name;
	} queryCombinations[] =
	{
		{ { QueryType::PRIMITIVES },															"prim_query"		},
		{ { QueryType::TASK_INVOCATIONS },														"task_invs_query"	},
		{ { QueryType::MESH_INVOCATIONS },														"mesh_invs_query"	},
		{ { QueryType::TASK_INVOCATIONS, QueryType::MESH_INVOCATIONS },							"all_stats_query"	},
		{ { QueryType::PRIMITIVES, QueryType::TASK_INVOCATIONS, QueryType::MESH_INVOCATIONS },	"all_queries"		},
	};

	const struct
	{
		DrawCallType	drawCallType;
		const char*		name;
	} drawCalls[] =
	{
		{ DrawCallType::DIRECT,					"draw"						},
		{ DrawCallType::INDIRECT,				"indirect_draw"				},
		{ DrawCallType::INDIRECT_WITH_COUNT,	"indirect_with_count_draw"	},
	};

	const struct
	{
		std::vector<uint32_t>	drawBlocks;
		const char*				name;
	} blockCases[] =
	{
		{ {},				"no_blocks"				},
		{ {10u},			"single_block"			},
		{ {10u, 20u, 30u},	"multiple_blocks"		},
	};

	const struct
	{
		ResetCase		resetCase;
		const char*		name;
	} resetTypes[] =
	{
		{ ResetCase::NONE,				"no_reset"		},
		{ ResetCase::NONE_WITH_HOST,	"host_reset"	},
		{ ResetCase::BEFORE_ACCESS,		"reset_before"	},
		{ ResetCase::AFTER_ACCESS,		"reset_after"	},
	};

	const struct
	{
		AccessMethod	accessMethod;
		const char*		name;
	} accessMethods[] =
	{
		{ AccessMethod::COPY,	"copy"	},
		{ AccessMethod::GET,	"get"	},
	};

	const struct
	{
		GeometryType	geometry;
		const char*		name;
	} geometryCases[] =
	{
		{ GeometryType::POINTS,		"points"	},
		{ GeometryType::LINES,		"lines"		},
		{ GeometryType::TRIANGLES,	"triangles"	},
	};

	const struct
	{
		bool			use64Bits;
		const char*		name;
	} resultSizes[] =
	{
		{ false,		"32bit"	},
		{ true,			"64bit"	},
	};

	const struct
	{
		bool			availabilityFlag;
		const char*		name;
	} availabilityCases[] =
	{
		{ false,		"no_availability"	},
		{ true,			"with_availability"	},
	};

	const struct
	{
		bool			waitFlag;
		const char*		name;
	} waitCases[] =
	{
		{ false,		"no_wait"	},
		{ true,			"wait"		},
	};

	const struct
	{
		bool			taskShader;
		const char*		name;
	} taskShaderCases[] =
	{
		{ false,		"mesh_only"	},
		{ true,			"task_mesh"	},
	};

	const struct
	{
		bool			insideRenderPass;
		const char*		name;
	} orderingCases[] =
	{
		{ false,		"include_rp"	},
		{ true,			"inside_rp"		},
	};

	const struct
	{
		bool			multiView;
		const char*		name;
	} multiViewCases[] =
	{
		{ false,		"single_view"	},
		{ true,			"multi_view"	},
	};

	const struct
	{
		bool			useSecondary;
		const char*		name;
	} cmdBufferTypes[] =
	{
		{ false,		"only_primary"		},
		{ true,			"with_secondary"	},
	};

	for (const auto& queryCombination : queryCombinations)
	{
		const bool hasPrimitivesQuery = de::contains(queryCombination.queryTypes.begin(), queryCombination.queryTypes.end(), QueryType::PRIMITIVES);

		GroupPtr queryCombinationGroup (new tcu::TestCaseGroup(testCtx, queryCombination.name, ""));

		for (const auto& geometryCase : geometryCases)
		{
			const bool nonTriangles = (geometryCase.geometry != GeometryType::TRIANGLES);

			// For cases without primitive queries, skip non-triangle geometries.
			if (!hasPrimitivesQuery && nonTriangles)
				continue;

			GroupPtr geometryCaseGroup (new tcu::TestCaseGroup(testCtx, geometryCase.name, ""));

			for (const auto& resetType : resetTypes)
			{
				GroupPtr resetTypeGroup (new tcu::TestCaseGroup(testCtx, resetType.name, ""));

				for (const auto& accessMethod : accessMethods)
				{
					// Get + reset after access is not a valid combination (queries will be accessed after submission).
					if (accessMethod.accessMethod == AccessMethod::GET && resetType.resetCase == ResetCase::AFTER_ACCESS)
						continue;

					GroupPtr accessMethodGroup (new tcu::TestCaseGroup(testCtx, accessMethod.name, ""));

					for (const auto& waitCase : waitCases)
					{
						// Wait and reset before access is not valid (the query would never finish).
						if (resetType.resetCase == ResetCase::BEFORE_ACCESS && waitCase.waitFlag)
							continue;

						GroupPtr waitCaseGroup (new tcu::TestCaseGroup(testCtx, waitCase.name, ""));

						for (const auto& drawCall : drawCalls)
						{
							// Explicitly remove some combinations with non-triangles, just to reduce the number of tests.
							if (drawCall.drawCallType != DrawCallType::DIRECT && nonTriangles)
								continue;

							GroupPtr drawCallGroup (new tcu::TestCaseGroup(testCtx, drawCall.name, ""));

							for (const auto& resultSize : resultSizes)
							{
								// Explicitly remove some combinations with non-triangles, just to reduce the number of tests.
								if (resultSize.use64Bits && nonTriangles)
									continue;

								GroupPtr resultSizeGroup (new tcu::TestCaseGroup(testCtx, resultSize.name, ""));

								for (const auto& availabilityCase : availabilityCases)
								{
									// Explicitly remove some combinations with non-triangles, just to reduce the number of tests.
									if (availabilityCase.availabilityFlag && nonTriangles)
										continue;

									GroupPtr availabilityCaseGroup (new tcu::TestCaseGroup(testCtx, availabilityCase.name, ""));

									for (const auto& blockCase : blockCases)
									{
										// Explicitly remove some combinations with non-triangles, just to reduce the number of tests.
										if (blockCase.drawBlocks.size() <= 1 && nonTriangles)
											continue;

										GroupPtr blockCaseGroup (new tcu::TestCaseGroup(testCtx, blockCase.name, ""));

										for (const auto& taskShaderCase : taskShaderCases)
										{
											GroupPtr taskShaderCaseGroup (new tcu::TestCaseGroup(testCtx, taskShaderCase.name, ""));

											for (const auto& orderingCase : orderingCases)
											{
												GroupPtr orderingCaseGroup (new tcu::TestCaseGroup(testCtx, orderingCase.name, ""));

												for (const auto& multiViewCase : multiViewCases)
												{
													if (multiViewCase.multiView && !orderingCase.insideRenderPass)
														continue;

													GroupPtr multiViewGroup (new tcu::TestCaseGroup(testCtx, multiViewCase.name, ""));

													for (const auto& cmdBufferType : cmdBufferTypes)
													{
														TestParams params;
														params.queryTypes		= queryCombination.queryTypes;
														params.drawBlocks		= blockCase.drawBlocks;
														params.drawCall			= drawCall.drawCallType;
														params.geometry			= geometryCase.geometry;
														params.resetType		= resetType.resetCase;
														params.access			= accessMethod.accessMethod;
														params.use64Bits		= resultSize.use64Bits;
														params.availabilityBit	= availabilityCase.availabilityFlag;
														params.waitBit			= waitCase.waitFlag;
														params.useTaskShader	= taskShaderCase.taskShader;
														params.insideRenderPass	= orderingCase.insideRenderPass;
														params.useSecondary		= cmdBufferType.useSecondary;
														params.multiView		= multiViewCase.multiView;

														// VUID-vkCmdExecuteCommands-commandBuffer-07594
														if (params.areQueriesInherited() && params.hasPrimitivesQuery())
															continue;

														multiViewGroup->addChild(new MeshQueryCase(testCtx, cmdBufferType.name, "", std::move(params)));
													}

													orderingCaseGroup->addChild(multiViewGroup.release());
												}

												taskShaderCaseGroup->addChild(orderingCaseGroup.release());
											}

											blockCaseGroup->addChild(taskShaderCaseGroup.release());
										}

										availabilityCaseGroup->addChild(blockCaseGroup.release());
									}

									resultSizeGroup->addChild(availabilityCaseGroup.release());
								}

								drawCallGroup->addChild(resultSizeGroup.release());
							}

							waitCaseGroup->addChild(drawCallGroup.release());
						}

						accessMethodGroup->addChild(waitCaseGroup.release());
					}

					resetTypeGroup->addChild(accessMethodGroup.release());
				}

				geometryCaseGroup->addChild(resetTypeGroup.release());
			}

			queryCombinationGroup->addChild(geometryCaseGroup.release());
		}

		queryGroup->addChild(queryCombinationGroup.release());
	}

	return queryGroup.release();
}

} // MeshShader
} // vkt
