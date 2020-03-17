/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Vulkan Statistics Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolStatisticsTests.hpp"
#include "vktTestCase.hpp"

#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deMath.h"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuMaybe.hpp"

#include <vector>
#include <utility>
#include <algorithm>

using std::vector;
using std::pair;

namespace vkt
{
namespace QueryPool
{
namespace
{

using namespace vk;
using namespace Draw;

//Test parameters
enum
{
	WIDTH	= 64,
	HEIGHT	= 64
};

enum ResetType
{
	RESET_TYPE_NORMAL = 0,
	RESET_TYPE_HOST,
	RESET_TYPE_BEFORE_COPY,
	RESET_TYPE_LAST
};

std::string inputTypeToGLString (const VkPrimitiveTopology& inputType)
{
	switch (inputType)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			return "points";
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			return "lines";
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			return "lines_adjacency";
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			return "triangles";
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			return "triangles_adjacency";
		default:
			DE_ASSERT(DE_FALSE);
			return "error";
	}
}

std::string outputTypeToGLString (const VkPrimitiveTopology& outputType)
{
	switch (outputType)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			return "points";
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
				return "line_strip";
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			return "triangle_strip";
		default:
			DE_ASSERT(DE_FALSE);
			return "error";
	}
}

using Pair32						= pair<deUint32, deUint32>;
using Pair64						= pair<deUint64, deUint64>;
using ResultsVector					= vector<deUint64>;
using ResultsVectorWithAvailability	= vector<Pair64>;

// Get query pool results as a vector. Note results are always converted to
// deUint64, but the actual vkGetQueryPoolResults call will use the 64-bits flag
// or not depending on your preferences.
vk::VkResult GetQueryPoolResultsVector(
	ResultsVector& output, const DeviceInterface& vk, vk::VkDevice device, vk::VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount, VkQueryResultFlags flags)
{
	if (flags & vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
		TCU_THROW(InternalError, "Availability flag passed when expecting results as ResultsVector");

	vk::VkResult result;
	output.resize(queryCount);

	if (flags & vk::VK_QUERY_RESULT_64_BIT)
	{
		constexpr size_t	stride		= sizeof(ResultsVector::value_type);
		const size_t		totalSize	= stride * output.size();
		result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, output.data(), stride, flags);
	}
	else
	{
		using IntermediateVector = vector<deUint32>;

		IntermediateVector	intermediate(queryCount);

		// Try to preserve existing data if possible.
		std::transform(begin(output), end(output), begin(intermediate), [](deUint64 v) { return static_cast<deUint32>(v); });

		constexpr size_t	stride		= sizeof(decltype(intermediate)::value_type);
		const size_t		totalSize	= stride * intermediate.size();

		// Get and copy results.
		result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, intermediate.data(), stride, flags);
		std::copy(begin(intermediate), end(intermediate), begin(output));
	}

	return result;
}

// Same as the normal GetQueryPoolResultsVector but returning the availability
// bit associated to each query in addition to the query value.
vk::VkResult GetQueryPoolResultsVector(
	ResultsVectorWithAvailability& output, const DeviceInterface& vk, vk::VkDevice device, vk::VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount, VkQueryResultFlags flags)
{
	flags |= vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;

	vk::VkResult result;
	output.resize(queryCount);

	if (flags & vk::VK_QUERY_RESULT_64_BIT)
	{
		constexpr size_t	stride		= sizeof(ResultsVectorWithAvailability::value_type);
		const size_t		totalSize	= stride * output.size();
		result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, output.data(), stride, flags);
	}
	else
	{
		using IntermediateVector = vector<Pair32>;

		IntermediateVector	intermediate(queryCount);

		// Try to preserve existing output data if possible.
		std::transform(begin(output), end(output), begin(intermediate), [](const Pair64& p) { return Pair32{static_cast<deUint32>(p.first), static_cast<deUint32>(p.second)}; });

		constexpr size_t	stride		= sizeof(decltype(intermediate)::value_type);
		const size_t		totalSize	= stride * intermediate.size();

		// Get and copy.
		result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, intermediate.data(), stride, flags);
		std::transform(begin(intermediate), end(intermediate), begin(output), [](const Pair32& p) { return Pair64{p.first, p.second}; });
	}

	return result;
}

// Generic parameters structure.
struct GenericParameters
{
	ResetType	resetType;
	deBool		query64Bits;

	GenericParameters (ResetType resetType_, deBool query64Bits_)
		: resetType{resetType_}, query64Bits{query64Bits_}
		{}

	VkQueryResultFlags querySizeFlags () const
	{
		return (query64Bits ? static_cast<VkQueryResultFlags>(vk::VK_QUERY_RESULT_64_BIT) : 0u);
	}
};

void beginSecondaryCommandBuffer (const DeviceInterface&				vk,
								  const VkCommandBuffer					commandBuffer,
								  const VkQueryPipelineStatisticFlags	queryFlags,
								  const VkRenderPass					renderPass = (VkRenderPass)0u,
								  const VkFramebuffer					framebuffer = (VkFramebuffer)0u,
								  const VkCommandBufferUsageFlags		bufferUsageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
{
	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		renderPass,					// renderPass
		0u,							// subpass
		framebuffer,				// framebuffer
		VK_FALSE,					// occlusionQueryEnable
		(VkQueryControlFlags)0u,	// queryFlags
		queryFlags,					// pipelineStatistics
	};

	const VkCommandBufferBeginInfo			info					=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType							sType;
		DE_NULL,										// const void*								pNext;
		bufferUsageFlags,								// VkCommandBufferUsageFlags				flags;
		&secCmdBufInheritInfo,							// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &info));
}

Move<VkQueryPool> makeQueryPool (const DeviceInterface& vk, const VkDevice device, deUint32 queryCount, VkQueryPipelineStatisticFlags statisticFlags )
{
	const VkQueryPoolCreateInfo queryPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType					sType
		DE_NULL,									// const void*						pNext
		(VkQueryPoolCreateFlags)0,					// VkQueryPoolCreateFlags			flags
		VK_QUERY_TYPE_PIPELINE_STATISTICS ,			// VkQueryType						queryType
		queryCount,									// deUint32							entryCount
		statisticFlags,								// VkQueryPipelineStatisticFlags	pipelineStatistics
	};
	return createQueryPool(vk, device, &queryPoolCreateInfo);
}

double calculatePearsonCorrelation(const std::vector<deUint64>& x, const ResultsVector& y)
{
	// This function calculates Pearson correlation coefficient ( https://en.wikipedia.org/wiki/Pearson_correlation_coefficient )
	// Two statistical variables are linear ( == fully corellated ) when fabs( Pearson corelation coefficient ) == 1
	// Two statistical variables are independent when pearson corelation coefficient == 0
	// If fabs( Pearson coefficient ) is > 0.8 then these two variables are almost linear

	DE_ASSERT(x.size() == y.size());
	DE_ASSERT(x.size() > 1);

	// calculate mean values
	double xMean = 0.0, yMean = 0.0;
	for (deUint32 i = 0; i < x.size(); ++i)
	{
		xMean += static_cast<double>(x[i]);
		yMean += static_cast<double>(y[i]);
	}
	xMean /= static_cast<double>(x.size());
	yMean /= static_cast<double>(x.size());

	// calculate standard deviations
	double xS = 0.0, yS = 0.0;
	for (deUint32 i = 0; i < x.size(); ++i)
	{
		double xv = static_cast<double>(x[i]) - xMean;
		double yv = static_cast<double>(y[i]) - yMean;

		xS += xv * xv;
		yS += yv * yv;
	}
	xS = sqrt( xS / static_cast<double>(x.size() - 1) );
	yS = sqrt( yS / static_cast<double>(x.size() - 1) );

	// calculate Pearson coefficient
	double pearson = 0.0;
	for (deUint32 i = 0; i < x.size(); ++i)
	{
		double xv = (static_cast<double>(x[i]) - xMean ) / xS;
		double yv = (static_cast<double>(y[i]) - yMean ) / yS;
		pearson   += xv * yv;
	}

	return pearson / static_cast<double>(x.size() - 1);
}

double calculatePearsonCorrelation(const std::vector<deUint64>& x, const ResultsVectorWithAvailability& ya)
{
	ResultsVector y;
	for (const auto& elt : ya)
		y.push_back(elt.first);
	return calculatePearsonCorrelation(x, y);
}

void clearBuffer (const DeviceInterface& vk, const VkDevice device, const de::SharedPtr<Buffer> buffer, const VkDeviceSize bufferSizeBytes)
{
	const std::vector<deUint8>	data			((size_t)bufferSizeBytes, 0u);
	const Allocation&			allocation		= buffer->getBoundMemory();
	void*						allocationData	= allocation.getHostPtr();
	invalidateAlloc(vk, device, allocation);
	deMemcpy(allocationData, &data[0], (size_t)bufferSizeBytes);
}

class StatisticQueryTestInstance : public TestInstance
{
public:
					StatisticQueryTestInstance	(Context& context, deUint32 queryCount);
protected:
	struct ValueAndAvailability
	{
		deUint64 value;
		deUint64 availability;
	};

	de::SharedPtr<Buffer> m_resetBuffer;

	virtual void			checkExtensions		(deBool hostResetQueryEnabled);
	tcu::TestStatus			verifyUnavailable	();
};

StatisticQueryTestInstance::StatisticQueryTestInstance (Context& context, deUint32 queryCount)
	: TestInstance	(context)
	, m_resetBuffer (Buffer::createAndAlloc(context.getDeviceInterface(),
											context.getDevice(),
											BufferCreateInfo(queryCount * sizeof(ValueAndAvailability), VK_BUFFER_USAGE_TRANSFER_DST_BIT),
											context.getDefaultAllocator(),
											vk::MemoryRequirement::HostVisible))
{
}

void StatisticQueryTestInstance::checkExtensions (deBool hostResetQueryEnabled)
{
	if (!m_context.getDeviceFeatures().pipelineStatisticsQuery)
		throw tcu::NotSupportedError("Pipeline statistics queries are not supported");

	if (hostResetQueryEnabled == DE_TRUE)
	{
		// Check VK_EXT_host_query_reset is supported
		m_context.requireDeviceFunctionality("VK_EXT_host_query_reset");
		if(m_context.getHostQueryResetFeatures().hostQueryReset == VK_FALSE)
			throw tcu::NotSupportedError(std::string("Implementation doesn't support resetting queries from the host").c_str());
	}
}

tcu::TestStatus	StatisticQueryTestInstance::verifyUnavailable ()
{
	struct ValueAndAvailability va;
	const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
	const void* allocationData = allocation.getHostPtr();

	vk::invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), allocation);
	deMemcpy(&va, allocationData, sizeof(va));
	return ((va.availability != 0) ? tcu::TestStatus::fail("Availability bit nonzero after resetting query") : tcu::TestStatus::pass("Pass"));
}

class ComputeInvocationsTestInstance : public StatisticQueryTestInstance
{
public:
	struct ParametersCompute : public GenericParameters
	{
		ParametersCompute (const tcu::UVec3& localSize_, const tcu::UVec3& groupSize_, const std::string& shaderName_, ResetType resetType_, deBool query64Bits_)
			: GenericParameters{resetType_, query64Bits_}
			, localSize(localSize_)
			, groupSize(groupSize_)
			, shaderName(shaderName_)
			{}

		tcu::UVec3	localSize;
		tcu::UVec3	groupSize;
		std::string	shaderName;
	};
							ComputeInvocationsTestInstance		(Context& context, const std::vector<ParametersCompute>& parameters);
	tcu::TestStatus			iterate								(void);
protected:
	virtual tcu::TestStatus	executeTest							(const VkCommandPool&			cmdPool,
																 const VkPipelineLayout			pipelineLayout,
																 const VkDescriptorSet&			descriptorSet,
																 const de::SharedPtr<Buffer>	buffer,
																 const VkDeviceSize				bufferSizeBytes);
	deUint32				getComputeExecution					(const ParametersCompute& parm) const
		{
			return parm.localSize.x() * parm.localSize.y() *parm.localSize.z() * parm.groupSize.x() * parm.groupSize.y() * parm.groupSize.z();
		}
	const std::vector<ParametersCompute>&	m_parameters;
};

ComputeInvocationsTestInstance::ComputeInvocationsTestInstance (Context& context, const std::vector<ParametersCompute>& parameters)
	: StatisticQueryTestInstance	(context, 1u)
	, m_parameters					(parameters)
{
}

tcu::TestStatus	ComputeInvocationsTestInstance::iterate (void)
{
	checkExtensions((m_parameters[0].resetType == RESET_TYPE_HOST)? DE_TRUE : DE_FALSE);
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						device					= m_context.getDevice();
	deUint32							maxSize					= 0u;

	for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
		maxSize = deMaxu32(maxSize, getComputeExecution(m_parameters[parametersNdx]));

	const VkDeviceSize					bufferSizeBytes			= static_cast<VkDeviceSize>(deAlignSize(static_cast<size_t>(sizeof(deUint32) * maxSize),
																								static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));
	de::SharedPtr<Buffer>				buffer					= Buffer::createAndAlloc(vk, device, BufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
																							 m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

	const Unique<VkDescriptorSetLayout>	descriptorSetLayout		(DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(vk, device));

	const Unique<VkPipelineLayout>		pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));

	const Unique<VkDescriptorPool>		descriptorPool			(DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
			.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const VkDescriptorSetAllocateInfo allocateParams		=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		*descriptorPool,								// VkDescriptorPool				descriptorPool;
		1u,												// deUint32						setLayoutCount;
		&(*descriptorSetLayout),						// const VkDescriptorSetLayout*	pSetLayouts;
	};

	const Unique<VkDescriptorSet>		descriptorSet		(allocateDescriptorSet(vk, device, &allocateParams));
	const VkDescriptorBufferInfo		descriptorInfo		=
	{
		buffer->object(),	//VkBuffer		buffer;
		0ull,				//VkDeviceSize	offset;
		bufferSizeBytes,	//VkDeviceSize	range;
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	const CmdPoolCreateInfo			cmdPoolCreateInfo	(m_context.getUniversalQueueFamilyIndex());
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, &cmdPoolCreateInfo));

	return executeTest (*cmdPool, *pipelineLayout, *descriptorSet, buffer, bufferSizeBytes);
}

tcu::TestStatus ComputeInvocationsTestInstance::executeTest (const VkCommandPool&			cmdPool,
															 const VkPipelineLayout			pipelineLayout,
															 const VkDescriptorSet&			descriptorSet,
															 const de::SharedPtr<Buffer>	buffer,
															 const VkDeviceSize				bufferSizeBytes)
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						device					= m_context.getDevice();
	const VkQueue						queue					= m_context.getUniversalQueue();
	const VkBufferMemoryBarrier			computeFinishBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,									// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	for(size_t parametersNdx = 0u; parametersNdx < m_parameters.size(); ++parametersNdx)
	{
		clearBuffer(vk, device, buffer, bufferSizeBytes);
		const Unique<VkShaderModule>			shaderModule				(createShaderModule(vk, device,
																			m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName), (VkShaderModuleCreateFlags)0u));

		const VkPipelineShaderStageCreateInfo	pipelineShaderStageParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0u,					// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			*shaderModule,											// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo			=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			(VkPipelineCreateFlags)0u,						// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,						// VkPipelineShaderStageCreateInfo	stage;
			pipelineLayout,									// VkPipelineLayout					layout;
			DE_NULL,										// VkPipeline						basePipelineHandle;
			0,												// deInt32							basePipelineIndex;
		};
		const Unique<VkPipeline> pipeline(createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo));

		const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		const Unique<VkQueryPool>		queryPool			(makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

		beginCommandBuffer(vk, *cmdBuffer);
			if (m_parameters[0].resetType != RESET_TYPE_HOST)
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

			vk.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
			vk.cmdDispatch(*cmdBuffer, m_parameters[parametersNdx].groupSize.x(), m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());
			vk.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

			if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);
				vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
			}

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
				(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeFinishBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);

		endCommandBuffer(vk, *cmdBuffer);

		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Compute shader invocations: " << getComputeExecution(m_parameters[parametersNdx]) << tcu::TestLog::EndMessage;

		if (m_parameters[0].resetType == RESET_TYPE_HOST)
			vk.resetQueryPool(device, *queryPool, 0u, 1u);

		// Wait for completion
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		// Validate the results
		const Allocation& bufferAllocation = buffer->getBoundMemory();
		invalidateAlloc(vk, device, bufferAllocation);

		if (m_parameters[0].resetType == RESET_TYPE_NORMAL)
		{
			ResultsVector data;
			VK_CHECK(GetQueryPoolResultsVector(data, vk, device, *queryPool, 0u, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags())));
			if (getComputeExecution(m_parameters[parametersNdx]) != data[0])
				return tcu::TestStatus::fail("QueryPoolResults incorrect");
		}
		else if (m_parameters[0].resetType == RESET_TYPE_HOST)
		{
			ResultsVectorWithAvailability data;
			VK_CHECK(GetQueryPoolResultsVector(data, vk, device, *queryPool, 0u, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
			if (getComputeExecution(m_parameters[parametersNdx]) != data[0].first || data[0].second == 0)
				return tcu::TestStatus::fail("QueryPoolResults incorrect");

			deUint64 temp = data[0].first;

			vk.resetQueryPool(device, *queryPool, 0, 1u);
			vk::VkResult res = GetQueryPoolResultsVector(data, vk, device, *queryPool, 0u, 1u, (m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
			/* From Vulkan spec:
			 *
			 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
			 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
			 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
			 */
			if (res != vk::VK_NOT_READY || data[0].first != temp || data[0].second != 0u)
				return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
		}
		else
		{
			// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
			return verifyUnavailable();
		}

		const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
		for (deUint32 ndx = 0u; ndx < getComputeExecution(m_parameters[parametersNdx]); ++ndx)
		{
			if (bufferPtr[ndx] != ndx)
				return tcu::TestStatus::fail("Compute shader didn't write data to the buffer");
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class ComputeInvocationsSecondaryTestInstance : public ComputeInvocationsTestInstance
{
public:
							ComputeInvocationsSecondaryTestInstance	(Context& context, const std::vector<ParametersCompute>& parameters);
protected:
	tcu::TestStatus			executeTest								(const VkCommandPool&			cmdPool,
																	 const VkPipelineLayout			pipelineLayout,
																	 const VkDescriptorSet&			descriptorSet,
																	 const de::SharedPtr<Buffer>	buffer,
																	 const VkDeviceSize				bufferSizeBytes);
	virtual tcu::TestStatus	checkResult								(const de::SharedPtr<Buffer>	buffer,
																	 const VkQueryPool				queryPool);
};

ComputeInvocationsSecondaryTestInstance::ComputeInvocationsSecondaryTestInstance	(Context& context, const std::vector<ParametersCompute>& parameters)
	: ComputeInvocationsTestInstance	(context, parameters)
{
}

tcu::TestStatus ComputeInvocationsSecondaryTestInstance::executeTest (const VkCommandPool&			cmdPool,
																	  const VkPipelineLayout		pipelineLayout,
																	  const VkDescriptorSet&		descriptorSet,
																	  const de::SharedPtr<Buffer>	buffer,
																	  const VkDeviceSize			bufferSizeBytes)
{
	typedef de::SharedPtr<Unique<VkShaderModule> >	VkShaderModuleSp;
	typedef de::SharedPtr<Unique<VkPipeline> >		VkPipelineSp;

	const DeviceInterface&					vk							= m_context.getDeviceInterface();
	const VkDevice							device						= m_context.getDevice();
	const VkQueue							queue						= m_context.getUniversalQueue();

	const VkBufferMemoryBarrier				computeShaderWriteBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	const VkBufferMemoryBarrier				computeFinishBarrier		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,									// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	std::vector<VkShaderModuleSp>			shaderModule;
	std::vector<VkPipelineSp>				pipeline;
	for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
	{
		shaderModule.push_back(VkShaderModuleSp(new Unique<VkShaderModule>(createShaderModule(vk, device, m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName), (VkShaderModuleCreateFlags)0u))));
		const VkPipelineShaderStageCreateInfo	pipelineShaderStageParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			0u,														// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			shaderModule.back().get()->get(),						// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo			=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,						// VkPipelineShaderStageCreateInfo	stage;
			pipelineLayout,									// VkPipelineLayout					layout;
			DE_NULL,										// VkPipeline						basePipelineHandle;
			0,												// deInt32							basePipelineIndex;
		};
		pipeline.push_back(VkPipelineSp(new Unique<VkPipeline>(createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo))));
	}

	const Unique<VkCommandBuffer>				primaryCmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkCommandBuffer>				secondaryCmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

	const Unique<VkQueryPool>					queryPool					(makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

	clearBuffer(vk, device, buffer, bufferSizeBytes);
	beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT);
		vk.cmdBindDescriptorSets(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		if (m_parameters[0].resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*secondaryCmdBuffer, *queryPool, 0u, 1u);
		vk.cmdBeginQuery(*secondaryCmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
		for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
		{
				vk.cmdBindPipeline(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[parametersNdx].get()->get());
				vk.cmdDispatch(*secondaryCmdBuffer, m_parameters[parametersNdx].groupSize.x(), m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());

				vk.cmdPipelineBarrier(*secondaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeShaderWriteBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
		}
		vk.cmdEndQuery(*secondaryCmdBuffer, *queryPool, 0u);

		if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*secondaryCmdBuffer, *queryPool, 0u, 1u);
			vk.cmdCopyQueryPoolResults(*secondaryCmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

	endCommandBuffer(vk, *secondaryCmdBuffer);

	beginCommandBuffer(vk, *primaryCmdBuffer);
		vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &secondaryCmdBuffer.get());

		vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
			(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeFinishBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *primaryCmdBuffer);

	// Secondary buffer is emitted only once, so it is safe to reset the query pool here.
	if (m_parameters[0].resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, 1u);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult(buffer, *queryPool);
}

tcu::TestStatus ComputeInvocationsSecondaryTestInstance::checkResult (const de::SharedPtr<Buffer> buffer, const VkQueryPool queryPool)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	{
		deUint64 expected	= 0u;
		for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
			expected += getComputeExecution(m_parameters[parametersNdx]);

		if (m_parameters[0].resetType == RESET_TYPE_NORMAL)
		{
			ResultsVector results;
			VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags())));
			if (expected != results[0])
				return tcu::TestStatus::fail("QueryPoolResults incorrect");
		}
		else if (m_parameters[0].resetType == RESET_TYPE_HOST)
		{
			ResultsVectorWithAvailability results;
			VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
			if (expected != results[0].first || results[0].second == 0u)
				return tcu::TestStatus::fail("QueryPoolResults incorrect");

			deUint64 temp = results[0].first;

			vk.resetQueryPool(device, queryPool, 0u, 1u);
			vk::VkResult res = GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u, (m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
			/* From Vulkan spec:
			 *
			 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
			 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
			 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
			 */
			if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
				return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
		}
		else
		{
			// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
			return verifyUnavailable();
		}

	}

	{
		// Validate the results
		const Allocation&	bufferAllocation	= buffer->getBoundMemory();
		invalidateAlloc(vk, device, bufferAllocation);
		const deUint32*		bufferPtr			= static_cast<deUint32*>(bufferAllocation.getHostPtr());
		deUint32			minSize				= ~0u;
		for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
			minSize = deMinu32(minSize, getComputeExecution(m_parameters[parametersNdx]));
		for (deUint32 ndx = 0u; ndx < minSize; ++ndx)
		{
			if (bufferPtr[ndx] != ndx * m_parameters.size())
				return tcu::TestStatus::fail("Compute shader didn't write data to the buffer");
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class ComputeInvocationsSecondaryInheritedTestInstance : public ComputeInvocationsSecondaryTestInstance
{
public:
					ComputeInvocationsSecondaryInheritedTestInstance	(Context& context, const std::vector<ParametersCompute>& parameters);
protected:
	virtual void	checkExtensions							(deBool hostResetQueryEnabled);

	tcu::TestStatus	executeTest								(const VkCommandPool&			cmdPool,
															 const VkPipelineLayout			pipelineLayout,
															 const VkDescriptorSet&			descriptorSet,
															 const de::SharedPtr<Buffer>	buffer,
															 const VkDeviceSize				bufferSizeBytes);
};

ComputeInvocationsSecondaryInheritedTestInstance::ComputeInvocationsSecondaryInheritedTestInstance	(Context& context, const std::vector<ParametersCompute>& parameters)
	: ComputeInvocationsSecondaryTestInstance	(context, parameters)
{
}

void ComputeInvocationsSecondaryInheritedTestInstance::checkExtensions (deBool hostResetQueryEnabled)
{
	StatisticQueryTestInstance::checkExtensions(hostResetQueryEnabled);
	if (!m_context.getDeviceFeatures().inheritedQueries)
		throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus ComputeInvocationsSecondaryInheritedTestInstance::executeTest (const VkCommandPool&			cmdPool,
																			  const VkPipelineLayout		pipelineLayout,
																			  const VkDescriptorSet&		descriptorSet,
																			  const de::SharedPtr<Buffer>	buffer,
																			  const VkDeviceSize			bufferSizeBytes)
{
	typedef de::SharedPtr<Unique<VkShaderModule> >	VkShaderModuleSp;
	typedef de::SharedPtr<Unique<VkPipeline> >		VkPipelineSp;

	const DeviceInterface&						vk								= m_context.getDeviceInterface();
	const VkDevice								device							= m_context.getDevice();
	const VkQueue								queue							= m_context.getUniversalQueue();

	const VkBufferMemoryBarrier					computeShaderWriteBarrier		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	const VkBufferMemoryBarrier					computeFinishBarrier			=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,									// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	std::vector<VkShaderModuleSp>				shaderModule;
	std::vector<VkPipelineSp>					pipeline;
	for(size_t parametersNdx = 0u; parametersNdx < m_parameters.size(); ++parametersNdx)
	{
		shaderModule.push_back(VkShaderModuleSp(new Unique<VkShaderModule>(createShaderModule(vk, device, m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName), (VkShaderModuleCreateFlags)0u))));
		const VkPipelineShaderStageCreateInfo	pipelineShaderStageParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
			shaderModule.back().get()->get(),						// VkShaderModule					module;
			"main",													// const char*						pName;
			DE_NULL,												// const VkSpecializationInfo*		pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo				=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,						// VkPipelineShaderStageCreateInfo	stage;
			pipelineLayout,									// VkPipelineLayout					layout;
			DE_NULL,										// VkPipeline						basePipelineHandle;
			0,												// deInt32							basePipelineIndex;
		};
		pipeline.push_back(VkPipelineSp(new Unique<VkPipeline>(createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo))));
	}

	const Unique<VkCommandBuffer>				primaryCmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkCommandBuffer>				secondaryCmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

	const Unique<VkQueryPool>					queryPool					(makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

	clearBuffer(vk, device, buffer, bufferSizeBytes);
	beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT);
		vk.cmdBindDescriptorSets(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		for(size_t parametersNdx = 1; parametersNdx < m_parameters.size(); ++parametersNdx)
		{
				vk.cmdBindPipeline(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[parametersNdx].get()->get());
				vk.cmdDispatch(*secondaryCmdBuffer, m_parameters[parametersNdx].groupSize.x(), m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());

				vk.cmdPipelineBarrier(*secondaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeShaderWriteBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
		}
	endCommandBuffer(vk, *secondaryCmdBuffer);

	beginCommandBuffer(vk, *primaryCmdBuffer);
		if (m_parameters[0].resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, 1u);
		vk.cmdBindDescriptorSets(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		vk.cmdBindPipeline(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[0].get()->get());

		vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
		vk.cmdDispatch(*primaryCmdBuffer, m_parameters[0].groupSize.x(), m_parameters[0].groupSize.y(), m_parameters[0].groupSize.z());

		vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeShaderWriteBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);

		vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &secondaryCmdBuffer.get());

		vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
			(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeFinishBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);

		vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, 0u);

		if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, 1u);
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parameters[0].resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, 1u);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult(buffer, *queryPool);
}

class GraphicBasicTestInstance : public StatisticQueryTestInstance
{
public:
	struct VertexData
	{
		VertexData (const tcu::Vec4 position_, const tcu::Vec4 color_)
			: position	(position_)
			, color		(color_)
		{}
		tcu::Vec4	position;
		tcu::Vec4	color;
	};

	struct ParametersGraphic : public GenericParameters
	{
		ParametersGraphic (const VkQueryPipelineStatisticFlags queryStatisticFlags_, const VkPrimitiveTopology primitiveTopology_, const ResetType resetType_, const deBool query64Bits_, const deBool vertexOnlyPipe_ = DE_FALSE)
			: GenericParameters		{resetType_, query64Bits_}
			, queryStatisticFlags	(queryStatisticFlags_)
			, primitiveTopology		(primitiveTopology_)
			, vertexOnlyPipe		(vertexOnlyPipe_)
			{}

		VkQueryPipelineStatisticFlags	queryStatisticFlags;
		VkPrimitiveTopology				primitiveTopology;
		deBool							vertexOnlyPipe;
	};
											GraphicBasicTestInstance			(vkt::Context&					context,
																				 const std::vector<VertexData>&	data,
																				 const ParametersGraphic&		parametersGraphic,
																				 const std::vector<deUint64>&	drawRepeats );
	tcu::TestStatus							iterate								(void);
protected:
	de::SharedPtr<Buffer>					creatAndFillVertexBuffer			(void);
	virtual void							createPipeline						(void) = 0;
	void									creatColorAttachmentAndRenderPass	(void);
	bool									checkImage							(void);
	virtual tcu::TestStatus					executeTest							(void) = 0;
	virtual tcu::TestStatus					checkResult							(VkQueryPool queryPool) = 0;
	virtual void							draw								(VkCommandBuffer cmdBuffer) = 0;

	const VkFormat						m_colorAttachmentFormat;
	de::SharedPtr<Image>				m_colorAttachmentImage;
	de::SharedPtr<Image>				m_depthImage;
	Move<VkImageView>					m_attachmentView;
	Move<VkImageView>					m_depthiew;
	Move<VkRenderPass>					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;
	Move<VkPipeline>					m_pipeline;
	Move<VkPipelineLayout>				m_pipelineLayout;
	const std::vector<VertexData>&		m_data;
	const ParametersGraphic&			m_parametersGraphic;
	std::vector<deUint64>				m_drawRepeats;
};

GraphicBasicTestInstance::GraphicBasicTestInstance (vkt::Context&					context,
													const std::vector<VertexData>&	data,
													const ParametersGraphic&		parametersGraphic,
													const std::vector<deUint64>&	drawRepeats )
	: StatisticQueryTestInstance	(context, static_cast<deUint32>(drawRepeats.size()))
	, m_colorAttachmentFormat		(VK_FORMAT_R8G8B8A8_UNORM)
	, m_data						(data)
	, m_parametersGraphic			(parametersGraphic)
	, m_drawRepeats					(drawRepeats)
{
}

tcu::TestStatus GraphicBasicTestInstance::iterate (void)
{
	checkExtensions((m_parametersGraphic.resetType == RESET_TYPE_HOST)? DE_TRUE : DE_FALSE);
	creatColorAttachmentAndRenderPass();
	createPipeline();
	return executeTest();
}

de::SharedPtr<Buffer> GraphicBasicTestInstance::creatAndFillVertexBuffer (void)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	const VkDevice				device			= m_context.getDevice();

	const VkDeviceSize			dataSize		= static_cast<VkDeviceSize>(deAlignSize(static_cast<size_t>( m_data.size() * sizeof(VertexData)),
		static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));

	de::SharedPtr<Buffer>		vertexBuffer	= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

	deUint8*					ptr				= reinterpret_cast<deUint8*>(vertexBuffer->getBoundMemory().getHostPtr());
	deMemcpy(ptr, &m_data[0], static_cast<size_t>( m_data.size() * sizeof(VertexData)));

	flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(), vertexBuffer->getBoundMemory().getOffset(), dataSize);
	return vertexBuffer;
}

void GraphicBasicTestInstance::creatColorAttachmentAndRenderPass (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	{
		VkExtent3D					imageExtent				=
		{
			WIDTH,	// width;
			HEIGHT,	// height;
			1u		// depth;
		};

		const ImageCreateInfo		colorImageCreateInfo	(VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
															VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		m_colorAttachmentImage	= Image::createAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		const ImageViewCreateInfo	attachmentViewInfo		(m_colorAttachmentImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
		m_attachmentView			= createImageView(vk, device, &attachmentViewInfo);

		ImageCreateInfo				depthImageCreateInfo	(vk::VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
															 vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

		m_depthImage				= Image::createAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		// Construct a depth  view from depth image
		const ImageViewCreateInfo	depthViewInfo			(m_depthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D16_UNORM);
		m_depthiew				= vk::createImageView(vk, device, &depthViewInfo);
	}

	{
		// Renderpass and Framebuffer
		RenderPassCreateInfo		renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,						// format
																	VK_SAMPLE_COUNT_1_BIT,						// samples
																	VK_ATTACHMENT_LOAD_OP_CLEAR,				// loadOp
																	VK_ATTACHMENT_STORE_OP_STORE ,				// storeOp
																	VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// stencilLoadOp
																	VK_ATTACHMENT_STORE_OP_STORE ,				// stencilLoadOp
																	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// initialLauout
																	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));	// finalLayout

		renderPassCreateInfo.addAttachment(AttachmentDescription(VK_FORMAT_D16_UNORM,										// format
																 vk::VK_SAMPLE_COUNT_1_BIT,									// samples
																 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,							// loadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// storeOp
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// initialLauout
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));	// finalLayout

		const VkAttachmentReference	colorAttachmentReference =
		{
			0u,											// attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// layout
		};

		const VkAttachmentReference depthAttachmentReference =
		{
			1u,															// attachment
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL		// layout
		};

		const VkSubpassDescription	subpass =
		{
			(VkSubpassDescriptionFlags) 0,		//VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	//VkPipelineBindPoint			pipelineBindPoint;
			0u,									//deUint32						inputAttachmentCount;
			DE_NULL,							//const VkAttachmentReference*	pInputAttachments;
			1u,									//deUint32						colorAttachmentCount;
			&colorAttachmentReference,			//const VkAttachmentReference*	pColorAttachments;
			DE_NULL,							//const VkAttachmentReference*	pResolveAttachments;
			&depthAttachmentReference,			//const VkAttachmentReference*	pDepthStencilAttachment;
			0u,									//deUint32						preserveAttachmentCount;
			DE_NULL,							//const deUint32*				pPreserveAttachments;
		};

		renderPassCreateInfo.addSubpass(subpass);
		m_renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

		std::vector<vk::VkImageView> attachments(2);
		attachments[0] = *m_attachmentView;
		attachments[1] = *m_depthiew;

		FramebufferCreateInfo		framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
		m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}
}

bool GraphicBasicTestInstance::checkImage (void)
{
	if (m_parametersGraphic.vertexOnlyPipe)
		return true;

	const VkQueue						queue			= m_context.getUniversalQueue();
	const VkOffset3D					zeroOffset		= { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess	renderedFrame	= m_colorAttachmentImage->readSurface(queue, m_context.getDefaultAllocator(),
															VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, VK_IMAGE_ASPECT_COLOR_BIT);
	int									colorNdx		= 0;
	tcu::Texture2D						referenceFrame	(mapVkFormat(m_colorAttachmentFormat), WIDTH, HEIGHT);
	referenceFrame.allocLevel(0);

	for (int y = 0; y < HEIGHT/2; ++y)
	for (int x = 0; x < WIDTH/2; ++x)
			referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

	colorNdx += 4;
	for (int y =  HEIGHT/2; y < HEIGHT; ++y)
	for (int x = 0; x < WIDTH/2; ++x)
			referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

	colorNdx += 4;
	for (int y = 0; y < HEIGHT/2; ++y)
	for (int x =  WIDTH/2; x < WIDTH; ++x)
			referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

	colorNdx += 4;
	for (int y =  HEIGHT/2; y < HEIGHT; ++y)
	for (int x =  WIDTH/2; x < WIDTH; ++x)
			referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

	return tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR);
}

class VertexShaderTestInstance : public GraphicBasicTestInstance
{
public:
							VertexShaderTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			createPipeline				(void);
	virtual tcu::TestStatus	executeTest					(void);
	virtual tcu::TestStatus	checkResult					(VkQueryPool queryPool);
	void					draw						(VkCommandBuffer cmdBuffer);
};

VertexShaderTestInstance::VertexShaderTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GraphicBasicTestInstance	(context, data, parametersGraphic, drawRepeats )
{
}

void VertexShaderTestInstance::createPipeline (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	switch (m_parametersGraphic.primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			if (!m_context.getDeviceFeatures().geometryShader)
				throw tcu::NotSupportedError("Geometry shader are not supported");
			break;
		default:
			break;
	}

	// Pipeline
	Unique<VkShaderModule>	vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), 0));
	Move<VkShaderModule>	fs;

	if (!m_parametersGraphic.vertexOnlyPipe)
		fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), 0);

	const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkVertexInputBindingDescription vertexInputBindingDescription		=
	{
		0,											// binding;
		static_cast<deUint32>(sizeof(VertexData)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX				// inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo vf_info			=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1u,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2u,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
	if (!m_parametersGraphic.vertexOnlyPipe)
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_parametersGraphic.primitiveTopology));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

	const VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
	const VkRect2D		scissor		= makeRect2D(WIDTH, HEIGHT);
	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1u, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
	pipelineCreateInfo.addState(vf_info);
	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::TestStatus VertexShaderTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0]);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

			for(deUint64 j=0; j<m_drawRepeats[i]; ++j)
				draw(*cmdBuffer);

			vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
		}

		endRenderPass(vk, *cmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *cmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	return checkResult (*queryPool);
}

tcu::TestStatus VertexShaderTestInstance::checkResult (VkQueryPool queryPool)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	deUint64				expectedMin	= 0u;

	switch(m_parametersGraphic.queryStatisticFlags)
	{
		case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT:
			expectedMin = 16u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					? 15u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			?  8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		? 14u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		?  6u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	?  8u :
							16u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST						? 16u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST						?  8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						? 15u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					?  5u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					?  8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						? 14u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			?  4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		? 13u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		?  2u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	?  6u :
							0u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST						?     9u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST						?   192u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						?   448u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					?  2016u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					?  4096u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						? 10208u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			?   128u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		?   416u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		?   992u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	?  3072u :
							0u;
			break;
		default:
			DE_FATAL("Unexpected type of statistics query");
			break;
	}

	const deUint32 queryCount = static_cast<deUint32>(m_drawRepeats.size());

	if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL)
	{
		ResultsVector results(queryCount, 0u);
		VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
		if (results[0] < expectedMin)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");
		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}
	}
	else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
	{
		ResultsVectorWithAvailability results(queryCount, pair<deUint64, deUint64>(0u,0u));
		VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
		if (results[0].first < expectedMin || results[0].second == 0)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");

		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}

		deUint64 temp = results[0].first;

		vk.resetQueryPool(device, queryPool, 0, queryCount);
		vk::VkResult res = GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
		/* From Vulkan spec:
		 *
		 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
		 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
		 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
		 */
		if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
	}
	else
	{
		// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
		return verifyUnavailable();
	}

	if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP && !checkImage())
		return tcu::TestStatus::fail("Result image doesn't match expected image.");

	return tcu::TestStatus::pass("Pass");
}

void VertexShaderTestInstance::draw (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();
	switch(m_parametersGraphic.primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			vk.cmdDraw(cmdBuffer, 16u, 1u, 0u, 0u);
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			vk.cmdDraw(cmdBuffer, 4u, 1u, 0u,  0u);
			vk.cmdDraw(cmdBuffer, 4u, 1u, 4u,  1u);
			vk.cmdDraw(cmdBuffer, 4u, 1u, 8u,  2u);
			vk.cmdDraw(cmdBuffer, 4u, 1u, 12u, 3u);
			break;
		default:
			DE_ASSERT(0);
			break;
	}
}

class VertexShaderSecondaryTestInstance : public VertexShaderTestInstance
{
public:
							VertexShaderSecondaryTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual tcu::TestStatus	executeTest							(void);
};

VertexShaderSecondaryTestInstance::VertexShaderSecondaryTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: VertexShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

typedef de::SharedPtr<vk::Unique<VkCommandBuffer>> VkCommandBufferSp;

tcu::TestStatus VertexShaderSecondaryTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for(deUint32 j=0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		for (deUint32 i = 0; i < queryCount; ++i)
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
		endRenderPass(vk, *primaryCmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult (*queryPool);
}

class VertexShaderSecondaryInheritedTestInstance : public VertexShaderTestInstance
{
public:
							VertexShaderSecondaryInheritedTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			checkExtensions						(deBool hostQueryResetEnabled);
	virtual tcu::TestStatus	executeTest							(void);
};

VertexShaderSecondaryInheritedTestInstance::VertexShaderSecondaryInheritedTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: VertexShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

void VertexShaderSecondaryInheritedTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().inheritedQueries)
		throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus VertexShaderSecondaryInheritedTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
			endRenderPass(vk, *primaryCmdBuffer);
			vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
		}

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult (*queryPool);
}

class GeometryShaderTestInstance : public GraphicBasicTestInstance
{
public:
							GeometryShaderTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			checkExtensions				(deBool hostQueryResetEnabled);
	virtual void			createPipeline				(void);
	virtual tcu::TestStatus	executeTest					(void);
	tcu::TestStatus			checkResult					(VkQueryPool queryPool);
	void					draw						(VkCommandBuffer cmdBuffer);
};

GeometryShaderTestInstance::GeometryShaderTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GraphicBasicTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void GeometryShaderTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().geometryShader)
		throw tcu::NotSupportedError("Geometry shader are not supported");
}

void GeometryShaderTestInstance::createPipeline (void)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	const VkBool32			useGeomPointSize		= m_context.getDeviceFeatures().shaderTessellationAndGeometryPointSize && (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

	// Pipeline
	Unique<VkShaderModule> vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> gs(createShaderModule(vk, device, m_context.getBinaryCollection().get(useGeomPointSize ? "geometry_point_size" : "geometry"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> fs(createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), (VkShaderModuleCreateFlags)0));

	const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkVertexInputBindingDescription vertexInputBindingDescription		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(VertexData)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX					// inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo vf_info			=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*gs, "main", VK_SHADER_STAGE_GEOMETRY_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_parametersGraphic.primitiveTopology));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

	const VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
	const VkRect2D		scissor		= makeRect2D(WIDTH, HEIGHT);

	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));

	if (m_context.getDeviceFeatures().depthBounds)
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL, true));
	else
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());

	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState(false));
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
	pipelineCreateInfo.addState(vf_info);
	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::TestStatus GeometryShaderTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0]);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

			for (deUint64 j = 0; j<m_drawRepeats[i]; ++j)
				draw(*cmdBuffer);

			vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
		}

		endRenderPass(vk, *cmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *cmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	return checkResult(*queryPool);
}

tcu::TestStatus GeometryShaderTestInstance::checkResult (VkQueryPool queryPool)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	deUint64				expectedMin	= 0u;

	switch(m_parametersGraphic.queryStatisticFlags)
	{
		case VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST						? 16u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST						? 8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						? 15u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					? 4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					? 4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						? 14u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			? 4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		? 13u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		? 2u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	? 6u :
							0u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT:
		case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT:
		case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST						? 112u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST						? 32u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						? 60u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					? 8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					? 8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						? 28u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			? 16u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		? 52u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		? 4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	? 12u :
							0u;
			break;
		default:
			DE_FATAL("Unexpected type of statistics query");
			break;
	}

	const deUint32 queryCount = static_cast<deUint32>(m_drawRepeats.size());

	if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL)
	{
		ResultsVector results(queryCount, 0u);
		VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
		if (results[0] < expectedMin)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");
		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}
	}
	else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
	{
		ResultsVectorWithAvailability results(queryCount, pair<deUint64, deUint64>(0u, 0u));
		VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
		if (results[0].first < expectedMin || results[0].second == 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");

		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}

		deUint64 temp = results[0].first;

		vk.resetQueryPool(device, queryPool, 0, queryCount);
		vk::VkResult res = GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
		/* From Vulkan spec:
		 *
		 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
		 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
		 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
		 */
		if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
	}
	else
	{
		// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
		return verifyUnavailable();
	}

	if ( (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ) && !checkImage())
		return tcu::TestStatus::fail("Result image doesn't match expected image.");

	return tcu::TestStatus::pass("Pass");
}

void GeometryShaderTestInstance::draw (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
		m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	{
		vk.cmdDraw(cmdBuffer, 3u, 1u,  0u,  1u);
		vk.cmdDraw(cmdBuffer, 3u, 1u,  4u,  1u);
		vk.cmdDraw(cmdBuffer, 3u, 1u,  8u,  2u);
		vk.cmdDraw(cmdBuffer, 3u, 1u, 12u,  3u);
	}
	else
	{
		vk.cmdDraw(cmdBuffer, 16u, 1u, 0u,  0u);
	}
}

class GeometryShaderSecondaryTestInstance : public GeometryShaderTestInstance
{
public:
							GeometryShaderSecondaryTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual tcu::TestStatus	executeTest							(void);
};

GeometryShaderSecondaryTestInstance::GeometryShaderSecondaryTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GeometryShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

tcu::TestStatus GeometryShaderSecondaryTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
		beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		for (deUint32 i = 0; i < queryCount; ++i)
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
		endRenderPass(vk, *primaryCmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult(*queryPool);
}

class GeometryShaderSecondaryInheritedTestInstance : public GeometryShaderTestInstance
{
public:
							GeometryShaderSecondaryInheritedTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			checkExtensions						(deBool hostQueryResetEnabled);
	virtual tcu::TestStatus	executeTest							(void);
};

GeometryShaderSecondaryInheritedTestInstance::GeometryShaderSecondaryInheritedTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GeometryShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

void GeometryShaderSecondaryInheritedTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	GeometryShaderTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().inheritedQueries)
		throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus GeometryShaderSecondaryInheritedTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
			endRenderPass(vk, *primaryCmdBuffer);
			vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
		}

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult(*queryPool);
}

class TessellationShaderTestInstance : public GraphicBasicTestInstance
{
public:
							TessellationShaderTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual	void			checkExtensions				(deBool hostQueryResetEnabled);
	virtual void			createPipeline				(void);
	virtual tcu::TestStatus	executeTest					(void);
	virtual tcu::TestStatus	checkResult					(VkQueryPool queryPool);
	void					draw						(VkCommandBuffer cmdBuffer);
};

TessellationShaderTestInstance::TessellationShaderTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GraphicBasicTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

void TessellationShaderTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().tessellationShader)
		throw tcu::NotSupportedError("Tessellation shader are not supported");
}


void TessellationShaderTestInstance::createPipeline (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	// Pipeline
	Unique<VkShaderModule> vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> tc(createShaderModule(vk, device, m_context.getBinaryCollection().get("tessellation_control"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> te(createShaderModule(vk, device, m_context.getBinaryCollection().get("tessellation_evaluation"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> fs(createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), (VkShaderModuleCreateFlags)0));

	const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkVertexInputBindingDescription vertexInputBindingDescription		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(VertexData)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX					// inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo vf_info			=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1u,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2u,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*tc, "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*te, "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState	(PipelineCreateInfo::TessellationState(4));
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

	const VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
	const VkRect2D		scissor		= makeRect2D(WIDTH, HEIGHT);

	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
	pipelineCreateInfo.addState(vf_info);
	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::TestStatus	TessellationShaderTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0]);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

			for (deUint64 j = 0; j<m_drawRepeats[i]; ++j)
				draw(*cmdBuffer);

			vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
		}

		endRenderPass(vk, *cmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *cmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	return checkResult (*queryPool);
}

tcu::TestStatus TessellationShaderTestInstance::checkResult (VkQueryPool queryPool)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	deUint64				expectedMin	= 0u;

	switch(m_parametersGraphic.queryStatisticFlags)
	{
		case VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT:
			expectedMin = 4u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT:
			expectedMin = 100u;
			break;
		default:
			DE_FATAL("Unexpected type of statistics query");
			break;
	}

	const deUint32 queryCount = static_cast<deUint32>(m_drawRepeats.size());

	if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL)
	{
		ResultsVector results(queryCount, 0u);
		VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
		if (results[0] < expectedMin)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");
		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}

		if (!checkImage())
			return tcu::TestStatus::fail("Result image doesn't match expected image.");
	}
	else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
	{
		ResultsVectorWithAvailability results(queryCount, pair<deUint64,deUint64>(0u,0u));
		VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
		if (results[0].first < expectedMin || results[0].second == 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");

		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}

		deUint64 temp = results[0].first;

		vk.resetQueryPool(device, queryPool, 0, queryCount);
		vk::VkResult res = GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
		/* From Vulkan spec:
		 *
		 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
		 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
		 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
		 */
		if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
	}
	else
	{
		// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
		return verifyUnavailable();
	}
	return tcu::TestStatus::pass("Pass");
}

void TessellationShaderTestInstance::draw (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();
	vk.cmdDraw(cmdBuffer, static_cast<deUint32>(m_data.size()), 1u, 0u, 0u);
}

class TessellationShaderSecondrayTestInstance : public TessellationShaderTestInstance
{
public:
							TessellationShaderSecondrayTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual tcu::TestStatus	executeTest								(void);
};

TessellationShaderSecondrayTestInstance::TessellationShaderSecondrayTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: TessellationShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

tcu::TestStatus	TessellationShaderSecondrayTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		vk.cmdBindVertexBuffers(*primaryCmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		vk.cmdBindPipeline(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		for (deUint32 i = 0; i < queryCount; ++i)
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
		endRenderPass(vk, *primaryCmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult (*queryPool);
}

class TessellationShaderSecondrayInheritedTestInstance : public TessellationShaderTestInstance
{
public:
							TessellationShaderSecondrayInheritedTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			checkExtensions							(deBool hostQueryResetEnabled);
	virtual tcu::TestStatus	executeTest								(void);
};

TessellationShaderSecondrayInheritedTestInstance::TessellationShaderSecondrayInheritedTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: TessellationShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

void TessellationShaderSecondrayInheritedTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	TessellationShaderTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().inheritedQueries)
		throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus	TessellationShaderSecondrayInheritedTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const de::SharedPtr<Buffer>				vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
			endRenderPass(vk, *primaryCmdBuffer);
			vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
		}

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
		{
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), 0u, sizeof(ValueAndAvailability), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}

		transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult (*queryPool);
}

template<class Instance>
class QueryPoolStatisticsTest : public TestCase
{
public:
	QueryPoolStatisticsTest (tcu::TestContext &context, const std::string& name, const std::string& description, const ResetType resetType, deBool query64Bits)
		: TestCase			(context, name.c_str(), description.c_str())
	{
		const tcu::UVec3	localSize[]		=
		{
			tcu::UVec3	(2u,			2u,	2u),
			tcu::UVec3	(1u,			1u,	1u),
			tcu::UVec3	(WIDTH/(7u*3u),	7u,	3u),
		};

		const tcu::UVec3	groupSize[]		=
		{
			tcu::UVec3	(2u,			2u,	2u),
			tcu::UVec3	(WIDTH/(7u*3u),	7u,	3u),
			tcu::UVec3	(1u,			1u,	1u),
		};

		DE_ASSERT(DE_LENGTH_OF_ARRAY(localSize) == DE_LENGTH_OF_ARRAY(groupSize));

		for(int shaderNdx = 0; shaderNdx < DE_LENGTH_OF_ARRAY(localSize); ++shaderNdx)
		{
			std::ostringstream	shaderName;
			shaderName<< "compute_" << shaderNdx;
			const ComputeInvocationsTestInstance::ParametersCompute	parameters(
				localSize[shaderNdx],
				groupSize[shaderNdx],
				shaderName.str(),
				resetType,
				query64Bits
			);
			m_parameters.push_back(parameters);
		}
	}

	vkt::TestInstance* createInstance (vkt::Context& context) const
	{
		return new Instance(context, m_parameters);
	}

	void initPrograms(SourceCollections& sourceCollections) const
	{
		std::ostringstream	source;
		source	<< "layout(binding = 0) writeonly buffer Output {\n"
				<< "	uint values[];\n"
				<< "} sb_out;\n\n"
				<< "void main (void) {\n"
				<< "	uvec3 indexUvec3 = uvec3 (gl_GlobalInvocationID.x,\n"
				<< "	                          gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x,\n"
				<< "	                          gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupSize.x * gl_WorkGroupSize.y);\n"
				<< "	uint index = indexUvec3.x + indexUvec3.y + indexUvec3.z;\n"
				<< "	sb_out.values[index] += index;\n"
				<< "}\n";

		for(size_t shaderNdx = 0; shaderNdx < m_parameters.size(); ++shaderNdx)
		{
			std::ostringstream	src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "layout (local_size_x = " << m_parameters[shaderNdx].localSize.x() << ", local_size_y = " << m_parameters[shaderNdx].localSize.y() << ", local_size_z = " << m_parameters[shaderNdx].localSize.z() << ") in;\n"
				<< source.str();
			sourceCollections.glslSources.add(m_parameters[shaderNdx].shaderName) << glu::ComputeSource(src.str());
		}
	}
private:
	std::vector<ComputeInvocationsTestInstance::ParametersCompute>	m_parameters;
};

template<class Instance>
class QueryPoolGraphicStatisticsTest : public TestCase
{
public:
	QueryPoolGraphicStatisticsTest (tcu::TestContext &context, const std::string& name, const std::string& description, const GraphicBasicTestInstance::ParametersGraphic parametersGraphic, const std::vector<deUint64>& drawRepeats)
		: TestCase				(context, name.c_str(), description.c_str())
		, m_parametersGraphic	(parametersGraphic)
		, m_drawRepeats			( drawRepeats )
	{
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));

		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 1.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
	}

	vkt::TestInstance* createInstance (vkt::Context& context) const
	{
		return new Instance(context, m_data, m_parametersGraphic, m_drawRepeats);
	}

	void initPrograms(SourceCollections& sourceCollections) const
	{
		{ // Vertex Shader
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "layout(location = 1) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	gl_PointSize = 1.0;\n"
					<< "	gl_Position = in_position;\n"
					<< "	out_color = in_color;\n"
					<< "}\n";
			sourceCollections.glslSources.add("vertex") << glu::VertexSource(source.str());
		}

		if (m_parametersGraphic.queryStatisticFlags & (VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT|
									VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT))
		{// Tessellation control & evaluation
			std::ostringstream source_tc;
			source_tc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
						<< "#extension GL_EXT_tessellation_shader : require\n"
						<< "layout(vertices = 4) out;\n"
						<< "layout(location = 0) in vec4 in_color[];\n"
						<< "layout(location = 0) out vec4 out_color[];\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "	if( gl_InvocationID == 0 )\n"
						<< "	{\n"
						<< "		gl_TessLevelInner[0] = 4.0f;\n"
						<< "		gl_TessLevelInner[1] = 4.0f;\n"
						<< "		gl_TessLevelOuter[0] = 4.0f;\n"
						<< "		gl_TessLevelOuter[1] = 4.0f;\n"
						<< "		gl_TessLevelOuter[2] = 4.0f;\n"
						<< "		gl_TessLevelOuter[3] = 4.0f;\n"
						<< "	}\n"
						<< "	out_color[gl_InvocationID] = in_color[gl_InvocationID];\n"
						<< "	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
						<< "}\n";
			sourceCollections.glslSources.add("tessellation_control") << glu::TessellationControlSource(source_tc.str());

			std::ostringstream source_te;
			source_te	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
						<< "#extension GL_EXT_tessellation_shader : require\n"
						<< "layout( quads, equal_spacing, ccw ) in;\n"
						<< "layout(location = 0) in vec4 in_color[];\n"
						<< "layout(location = 0) out vec4 out_color;\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "	const float u = gl_TessCoord.x;\n"
						<< "	const float v = gl_TessCoord.y;\n"
						<< "	const float w = gl_TessCoord.z;\n"
						<< "	gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position +(1 - u) * v * gl_in[1].gl_Position + u * (1 - v) * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position;\n"
						<< "	out_color = in_color[0];\n"
						<< "}\n";
			sourceCollections.glslSources.add("tessellation_evaluation") << glu::TessellationEvaluationSource(source_te.str());
		}

		if(m_parametersGraphic.queryStatisticFlags & (VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
									VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT))
		{ // Geometry Shader
			const bool isTopologyPointSize = m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout("<<inputTypeToGLString(m_parametersGraphic.primitiveTopology)<<") in;\n"
					<< "layout("<<outputTypeToGLString (m_parametersGraphic.primitiveTopology)<<", max_vertices = 16) out;\n"
					<< "layout(location = 0) in vec4 in_color[];\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	out_color = in_color[0];\n"
					<< (isTopologyPointSize ? "${pointSize}" : "" )
					<< "	gl_Position = gl_in[0].gl_Position;\n"
					<< "	EmitVertex();\n"
					<< "	EndPrimitive();\n"
					<< "\n"
					<< "	out_color = in_color[0];\n"
					<< (isTopologyPointSize ? "${pointSize}" : "")
					<< "	gl_Position = vec4(1.0, 1.0, 1.0, 1.0);\n"
					<< "	EmitVertex();\n"
					<< "	out_color = in_color[0];\n"
					<< (isTopologyPointSize ? "${pointSize}" : "")
					<< "	gl_Position = vec4(-1.0, -1.0, 1.0, 1.0);\n"
					<< "	EmitVertex();\n"
					<< "	EndPrimitive();\n"
					<< "\n";
			if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
				m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			{
				source	<< "\n"
						<< "	out_color = in_color[0];\n"
						<< "	gl_Position = gl_in[0].gl_Position;\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< "	gl_Position = gl_in[1].gl_Position;\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< "	gl_Position = gl_in[2].gl_Position;\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< "	gl_Position = vec4(gl_in[2].gl_Position.x, gl_in[1].gl_Position.y, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	EndPrimitive();\n";
			}
			else
			{
				source	<< "	out_color = in_color[0];\n"
						<< (isTopologyPointSize ? "${pointSize}" : "")
						<< "	gl_Position = vec4(1.0, 1.0, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< (isTopologyPointSize ? "${pointSize}" : "")
						<< "	gl_Position = vec4(1.0, -1.0, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< (isTopologyPointSize ? "${pointSize}" : "")
						<< "	gl_Position = vec4(-1.0, 1.0, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< (isTopologyPointSize ? "${pointSize}" : "")
						<< "	gl_Position = vec4(-1.0, -1.0, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	EndPrimitive();\n";
			}
			source	<< "}\n";

			if (isTopologyPointSize)
			{
				// Add geometry shader codes with and without gl_PointSize if the primitive topology is VK_PRIMITIVE_TOPOLOGY_POINT_LIST

				tcu::StringTemplate sourceTemplate(source.str());

				std::map<std::string, std::string> pointSize;
				std::map<std::string, std::string> noPointSize;

				pointSize["pointSize"]		= "	gl_PointSize = gl_in[0].gl_PointSize;\n";
				noPointSize["pointSize"]	= "";

				sourceCollections.glslSources.add("geometry") << glu::GeometrySource(sourceTemplate.specialize(noPointSize));
				sourceCollections.glslSources.add("geometry_point_size") << glu::GeometrySource(sourceTemplate.specialize(pointSize));
			}
			else
			{
				sourceCollections.glslSources.add("geometry") << glu::GeometrySource(source.str());
			}
		}

		if (!m_parametersGraphic.vertexOnlyPipe)
		{ // Fragment Shader
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout(location = 0) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main()\n"
					<<"{\n"
					<< "	out_color = in_color;\n"
					<< "}\n";
			sourceCollections.glslSources.add("fragment") << glu::FragmentSource(source.str());
		}
	}
private:
	std::vector<GraphicBasicTestInstance::VertexData>	m_data;
	const GraphicBasicTestInstance::ParametersGraphic	m_parametersGraphic;
	std::vector<deUint64>								m_drawRepeats;
};
} //anonymous

QueryPoolStatisticsTests::QueryPoolStatisticsTests (tcu::TestContext &testCtx)
	: TestCaseGroup(testCtx, "statistics_query", "Tests for statistics queries")
{
}

inline std::string bitPrefix(deBool query64bits)
{
	return (query64bits ? "64bits_" : "32bits_");
}

void QueryPoolStatisticsTests::init (void)
{
	std::string topology_name [VK_PRIMITIVE_TOPOLOGY_LAST] =
	{
		"point_list",
		"line_list",
		"line_strip",
		"triangle_list",
		"triangle_strip",
		"triangle_fan",
		"line_list_with_adjacency",
		"line_strip_with_adjacency",
		"triangle_list_with_adjacency",
		"triangle_strip_with_adjacency",
		"patch_list"
	};

	std::vector<deUint64> sixRepeats								= { 1, 3, 5, 8, 15, 24 };

	de::MovePtr<TestCaseGroup>	computeShaderInvocationsGroup		(new TestCaseGroup(m_testCtx, "compute_shader_invocations",			"Query pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVertices				(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitives				(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocations				(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));
	de::MovePtr<TestCaseGroup>	fragmentShaderInvocations			(new TestCaseGroup(m_testCtx, "fragment_shader_invocations",		"Query pipeline statistic fragment shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderInvocations			(new TestCaseGroup(m_testCtx, "geometry_shader_invocations",		"Query pipeline statistic geometry shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderPrimitives			(new TestCaseGroup(m_testCtx, "geometry_shader_primitives",			"Query pipeline statistic geometry shader primitives"));
	de::MovePtr<TestCaseGroup>	clippingInvocations					(new TestCaseGroup(m_testCtx, "clipping_invocations",				"Query pipeline statistic clipping invocations"));
	de::MovePtr<TestCaseGroup>	clippingPrimitives					(new TestCaseGroup(m_testCtx, "clipping_primitives",				"Query pipeline statistic clipping primitives"));
	de::MovePtr<TestCaseGroup>	tesControlPatches					(new TestCaseGroup(m_testCtx, "tes_control_patches",				"Query pipeline statistic tessellation control shader patches"));
	de::MovePtr<TestCaseGroup>	tesEvaluationShaderInvocations		(new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations",	"Query pipeline statistic tessellation evaluation shader invocations"));

	de::MovePtr<TestCaseGroup>	vertexOnlyGroup									(new TestCaseGroup(m_testCtx, "vertex_only",						"Use only vertex shader in a graphics pipeline"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVerticesVertexOnly					(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitivesVertexOnly				(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocationsVertexOnly				(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));

	de::MovePtr<TestCaseGroup>	hostQueryResetGroup								(new TestCaseGroup(m_testCtx, "host_query_reset",					"Check host query reset pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	computeShaderInvocationsGroupHostQueryReset		(new TestCaseGroup(m_testCtx, "compute_shader_invocations",			"Query pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVerticesHostQueryReset				(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitivesHostQueryReset			(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocationsHostQueryReset			(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));
	de::MovePtr<TestCaseGroup>	fragmentShaderInvocationsHostQueryReset			(new TestCaseGroup(m_testCtx, "fragment_shader_invocations",		"Query pipeline statistic fragment shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderInvocationsHostQueryReset			(new TestCaseGroup(m_testCtx, "geometry_shader_invocations",		"Query pipeline statistic geometry shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderPrimitivesHostQueryReset			(new TestCaseGroup(m_testCtx, "geometry_shader_primitives",			"Query pipeline statistic geometry shader primitives"));
	de::MovePtr<TestCaseGroup>	clippingInvocationsHostQueryReset				(new TestCaseGroup(m_testCtx, "clipping_invocations",				"Query pipeline statistic clipping invocations"));
	de::MovePtr<TestCaseGroup>	clippingPrimitivesHostQueryReset				(new TestCaseGroup(m_testCtx, "clipping_primitives",				"Query pipeline statistic clipping primitives"));
	de::MovePtr<TestCaseGroup>	tesControlPatchesHostQueryReset					(new TestCaseGroup(m_testCtx, "tes_control_patches",				"Query pipeline statistic tessellation control shader patches"));
	de::MovePtr<TestCaseGroup>	tesEvaluationShaderInvocationsHostQueryReset	(new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations",	"Query pipeline statistic tessellation evaluation shader invocations"));

	de::MovePtr<TestCaseGroup>	resetBeforeCopyGroup							(new TestCaseGroup(m_testCtx, "reset_before_copy",					"Check pipeline statistic unavailability when resetting before copying"));
	de::MovePtr<TestCaseGroup>	computeShaderInvocationsGroupResetBeforeCopy	(new TestCaseGroup(m_testCtx, "compute_shader_invocations",			"Query pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVerticesResetBeforeCopy			(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitivesResetBeforeCopy			(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocationsResetBeforeCopy			(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));
	de::MovePtr<TestCaseGroup>	fragmentShaderInvocationsResetBeforeCopy		(new TestCaseGroup(m_testCtx, "fragment_shader_invocations",		"Query pipeline statistic fragment shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderInvocationsResetBeforeCopy		(new TestCaseGroup(m_testCtx, "geometry_shader_invocations",		"Query pipeline statistic geometry shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderPrimitivesResetBeforeCopy			(new TestCaseGroup(m_testCtx, "geometry_shader_primitives",			"Query pipeline statistic geometry shader primitives"));
	de::MovePtr<TestCaseGroup>	clippingInvocationsResetBeforeCopy				(new TestCaseGroup(m_testCtx, "clipping_invocations",				"Query pipeline statistic clipping invocations"));
	de::MovePtr<TestCaseGroup>	clippingPrimitivesResetBeforeCopy				(new TestCaseGroup(m_testCtx, "clipping_primitives",				"Query pipeline statistic clipping primitives"));
	de::MovePtr<TestCaseGroup>	tesControlPatchesResetBeforeCopy				(new TestCaseGroup(m_testCtx, "tes_control_patches",				"Query pipeline statistic tessellation control shader patches"));
	de::MovePtr<TestCaseGroup>	tesEvaluationShaderInvocationsResetBeforeCopy	(new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations",	"Query pipeline statistic tessellation evaluation shader invocations"));

	for (deUint32 i = 0; i < 2; ++i)
	{
		deBool query64Bits = (i == 1);
		std::string prefix = bitPrefix(query64Bits);

		computeShaderInvocationsGroup->addChild(new QueryPoolStatisticsTest<ComputeInvocationsTestInstance>						(m_testCtx,	prefix + "primary",				"", RESET_TYPE_NORMAL, query64Bits));
		computeShaderInvocationsGroup->addChild(new QueryPoolStatisticsTest<ComputeInvocationsSecondaryTestInstance>			(m_testCtx,	prefix + "secondary",			"", RESET_TYPE_NORMAL, query64Bits));
		computeShaderInvocationsGroup->addChild(new QueryPoolStatisticsTest<ComputeInvocationsSecondaryInheritedTestInstance>	(m_testCtx,	prefix + "secondary_inherited",	"", RESET_TYPE_NORMAL, query64Bits));

		computeShaderInvocationsGroupHostQueryReset->addChild(new QueryPoolStatisticsTest<ComputeInvocationsTestInstance>					(m_testCtx,	prefix + "primary",				"", RESET_TYPE_HOST, query64Bits));
		computeShaderInvocationsGroupHostQueryReset->addChild(new QueryPoolStatisticsTest<ComputeInvocationsSecondaryTestInstance>			(m_testCtx,	prefix + "secondary",			"", RESET_TYPE_HOST, query64Bits));
		computeShaderInvocationsGroupHostQueryReset->addChild(new QueryPoolStatisticsTest<ComputeInvocationsSecondaryInheritedTestInstance>	(m_testCtx,	prefix + "secondary_inherited",	"", RESET_TYPE_HOST, query64Bits));

		computeShaderInvocationsGroupResetBeforeCopy->addChild(new QueryPoolStatisticsTest<ComputeInvocationsTestInstance>					(m_testCtx,	prefix + "primary",				"", RESET_TYPE_BEFORE_COPY, query64Bits));
		computeShaderInvocationsGroupResetBeforeCopy->addChild(new QueryPoolStatisticsTest<ComputeInvocationsSecondaryTestInstance>			(m_testCtx,	prefix + "secondary",			"", RESET_TYPE_BEFORE_COPY, query64Bits));
		computeShaderInvocationsGroupResetBeforeCopy->addChild(new QueryPoolStatisticsTest<ComputeInvocationsSecondaryInheritedTestInstance>(m_testCtx,	prefix + "secondary_inherited",	"", RESET_TYPE_BEFORE_COPY, query64Bits));

		//VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
		inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + "primary",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
		inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + "secondary",			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
		inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

		inputAssemblyVerticesVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	prefix + "primary",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));
		inputAssemblyVerticesVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>				(m_testCtx,	prefix + "secondary",			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));
		inputAssemblyVerticesVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));

		inputAssemblyVerticesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + "primary",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));
		inputAssemblyVerticesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + "secondary",			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));
		inputAssemblyVerticesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(m_testCtx,	prefix + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));

		inputAssemblyVerticesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	prefix + "primary",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
		inputAssemblyVerticesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + "secondary",			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
		inputAssemblyVerticesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
	}

	//VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryVertexOnly				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryVertexOnly				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedVertexOnly	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				deBool query64Bits = (i == 1);
				std::string prefix = bitPrefix(query64Bits);

				primary->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondary->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondaryInherited->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

				primaryHostQueryReset->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryInheritedHostQueryReset->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));

				primaryVertexOnly->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));
				secondaryVertexOnly->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));
				secondaryInheritedVertexOnly->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));

				primaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryInheritedResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
			}
		}

		inputAssemblyPrimitives->addChild(primary.release());
		inputAssemblyPrimitives->addChild(secondary.release());
		inputAssemblyPrimitives->addChild(secondaryInherited.release());

		inputAssemblyPrimitivesVertexOnly->addChild(primaryVertexOnly.release());
		inputAssemblyPrimitivesVertexOnly->addChild(secondaryVertexOnly.release());
		inputAssemblyPrimitivesVertexOnly->addChild(secondaryInheritedVertexOnly.release());

		inputAssemblyPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
		inputAssemblyPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
		inputAssemblyPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		inputAssemblyPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		inputAssemblyPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		inputAssemblyPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryVertexOnly				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryVertexOnly				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedVertexOnly	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				deBool query64Bits = (i == 1);
				std::string prefix = bitPrefix(query64Bits);

				primary->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondary->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondaryInherited->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

				primaryVertexOnly->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));
				secondaryVertexOnly->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));
				secondaryInheritedVertexOnly->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits, DE_TRUE), sixRepeats));

				primaryHostQueryReset->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryInheritedHostQueryReset->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));

				primaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryInheritedResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
			}
		}

		vertexShaderInvocations->addChild(primary.release());
		vertexShaderInvocations->addChild(secondary.release());
		vertexShaderInvocations->addChild(secondaryInherited.release());

		vertexShaderInvocationsVertexOnly->addChild(primaryVertexOnly.release());
		vertexShaderInvocationsVertexOnly->addChild(secondaryVertexOnly.release());
		vertexShaderInvocationsVertexOnly->addChild(secondaryInheritedVertexOnly.release());

		vertexShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
		vertexShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
		vertexShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		vertexShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		vertexShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		vertexShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				deBool query64Bits = (i == 1);
				std::string prefix = bitPrefix(query64Bits);

				primary->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondary->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondaryInherited->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

				primaryHostQueryReset->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryInheritedHostQueryReset->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));

				primaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryInheritedResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
			}
		}

		fragmentShaderInvocations->addChild(primary.release());
		fragmentShaderInvocations->addChild(secondary.release());
		fragmentShaderInvocations->addChild(secondaryInherited.release());

		fragmentShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
		fragmentShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
		fragmentShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		fragmentShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		fragmentShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		fragmentShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				deBool query64Bits = (i == 1);
				std::string prefix = bitPrefix(query64Bits);

				primary->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondary->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondaryInherited->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

				primaryHostQueryReset->addChild				(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryInheritedHostQueryReset->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));

				primaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryInheritedResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
			}
		}

		geometryShaderInvocations->addChild(primary.release());
		geometryShaderInvocations->addChild(secondary.release());
		geometryShaderInvocations->addChild(secondaryInherited.release());

		geometryShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
		geometryShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
		geometryShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		geometryShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		geometryShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		geometryShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				deBool query64Bits = (i == 1);
				std::string prefix = bitPrefix(query64Bits);

				primary->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondary->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondaryInherited->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

				primaryHostQueryReset->addChild				(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryInheritedHostQueryReset->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));

				primaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryInheritedResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
			}
		}

		geometryShaderPrimitives->addChild(primary.release());
		geometryShaderPrimitives->addChild(secondary.release());
		geometryShaderPrimitives->addChild(secondaryInherited.release());

		geometryShaderPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
		geometryShaderPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
		geometryShaderPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		geometryShaderPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		geometryShaderPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		geometryShaderPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				deBool query64Bits = (i == 1);
				std::string prefix = bitPrefix(query64Bits);

				primary->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondary->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondaryInherited->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

				primaryHostQueryReset->addChild				(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryInheritedHostQueryReset->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));

				primaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryInheritedResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
			}
		}

		clippingInvocations->addChild(primary.release());
		clippingInvocations->addChild(secondary.release());
		clippingInvocations->addChild(secondaryInherited.release());

		clippingInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
		clippingInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
		clippingInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		clippingInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		clippingInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		clippingInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				deBool query64Bits = (i == 1);
				std::string prefix = bitPrefix(query64Bits);

				primary->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondary->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
				secondaryInherited->addChild(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

				primaryHostQueryReset->addChild				(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));
				secondaryInheritedHostQueryReset->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, query64Bits), sixRepeats));

				primaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
				secondaryInheritedResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
			}
		}

		clippingPrimitives->addChild(primary.release());
		clippingPrimitives->addChild(secondary.release());
		clippingPrimitives->addChild(secondaryInherited.release());

		clippingPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
		clippingPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
		clippingPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		clippingPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		clippingPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		clippingPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT
	for (deUint32 i = 0; i < 2; ++i)
	{
		deBool query64Bits = (i == 1);
		std::string prefix = bitPrefix(query64Bits);

		tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + "tes_control_patches",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
		tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>			(m_testCtx,	prefix + "tes_control_patches_secondary",			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
		tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(m_testCtx,	prefix + "tes_control_patches_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

		tesControlPatchesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + "tes_control_patches",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));
		tesControlPatchesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>			(m_testCtx,	prefix + "tes_control_patches_secondary",			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));
		tesControlPatchesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + "tes_control_patches_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));

		tesControlPatchesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + "tes_control_patches",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
		tesControlPatchesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>			(m_testCtx,	prefix + "tes_control_patches_secondary",			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
		tesControlPatchesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + "tes_control_patches_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));

		//VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT
		tesEvaluationShaderInvocations->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					 (m_testCtx,	prefix + "tes_evaluation_shader_invocations",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
		tesEvaluationShaderInvocations->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>		 (m_testCtx,	prefix + "tes_evaluation_shader_invocations_secondary",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));
		tesEvaluationShaderInvocations->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(m_testCtx,	prefix + "tes_evaluation_shader_invocations_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, query64Bits), sixRepeats));

		tesEvaluationShaderInvocationsHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + "tes_evaluation_shader_invocations",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));
		tesEvaluationShaderInvocationsHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>			(m_testCtx,	prefix + "tes_evaluation_shader_invocations_secondary",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));
		tesEvaluationShaderInvocationsHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + "tes_evaluation_shader_invocations_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, query64Bits), sixRepeats));

		tesEvaluationShaderInvocationsResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + "tes_evaluation_shader_invocations",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
		tesEvaluationShaderInvocationsResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>			(m_testCtx,	prefix + "tes_evaluation_shader_invocations_secondary",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
		tesEvaluationShaderInvocationsResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(m_testCtx,	prefix + "tes_evaluation_shader_invocations_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, query64Bits), sixRepeats));
	}

	addChild(computeShaderInvocationsGroup.release());
	addChild(inputAssemblyVertices.release());
	addChild(inputAssemblyPrimitives.release());
	addChild(vertexShaderInvocations.release());
	addChild(fragmentShaderInvocations.release());
	addChild(geometryShaderInvocations.release());
	addChild(geometryShaderPrimitives.release());
	addChild(clippingInvocations.release());
	addChild(clippingPrimitives.release());
	addChild(tesControlPatches.release());
	addChild(tesEvaluationShaderInvocations.release());

	vertexOnlyGroup->addChild(inputAssemblyVerticesVertexOnly.release());
	vertexOnlyGroup->addChild(inputAssemblyPrimitivesVertexOnly.release());
	vertexOnlyGroup->addChild(vertexShaderInvocationsVertexOnly.release());
	addChild(vertexOnlyGroup.release());

	hostQueryResetGroup->addChild(computeShaderInvocationsGroupHostQueryReset.release());
	hostQueryResetGroup->addChild(inputAssemblyVerticesHostQueryReset.release());
	hostQueryResetGroup->addChild(inputAssemblyPrimitivesHostQueryReset.release());
	hostQueryResetGroup->addChild(vertexShaderInvocationsHostQueryReset.release());
	hostQueryResetGroup->addChild(fragmentShaderInvocationsHostQueryReset.release());
	hostQueryResetGroup->addChild(geometryShaderInvocationsHostQueryReset.release());
	hostQueryResetGroup->addChild(geometryShaderPrimitivesHostQueryReset.release());
	hostQueryResetGroup->addChild(clippingInvocationsHostQueryReset.release());
	hostQueryResetGroup->addChild(clippingPrimitivesHostQueryReset.release());
	hostQueryResetGroup->addChild(tesControlPatchesHostQueryReset.release());
	hostQueryResetGroup->addChild(tesEvaluationShaderInvocationsHostQueryReset.release());
	addChild(hostQueryResetGroup.release());

	resetBeforeCopyGroup->addChild(computeShaderInvocationsGroupResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(inputAssemblyVerticesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(inputAssemblyPrimitivesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(vertexShaderInvocationsResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(fragmentShaderInvocationsResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(geometryShaderInvocationsResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(geometryShaderPrimitivesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(clippingInvocationsResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(clippingPrimitivesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(tesControlPatchesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(tesEvaluationShaderInvocationsResetBeforeCopy.release());
	addChild(resetBeforeCopyGroup.release());
}

} //QueryPool
} //vkt
