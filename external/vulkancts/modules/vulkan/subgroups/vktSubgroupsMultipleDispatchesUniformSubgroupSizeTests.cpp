/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC.
 *
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
 * \brief Tests that compute shaders have a subgroup size that is uniform in
 * command scope.
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"

#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include "vktTestCaseUtil.hpp"

#include "tcuTestLog.hpp"

#include <sstream>

using namespace vk;

namespace vkt
{
namespace subgroups
{
namespace
{

class MultipleDispatchesUniformSubgroupSizeInstance : public TestInstance
{
public:
					MultipleDispatchesUniformSubgroupSizeInstance	(Context&	context);
	tcu::TestStatus	iterate											(void);
};

MultipleDispatchesUniformSubgroupSizeInstance::MultipleDispatchesUniformSubgroupSizeInstance	(Context&	context)
	:TestInstance																				(context)
{
}

tcu::TestStatus MultipleDispatchesUniformSubgroupSizeInstance::iterate (void)
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						device					= m_context.getDevice();
	Allocator&							allocator				= m_context.getDefaultAllocator();
	const VkQueue						queue					= m_context.getUniversalQueue();
	const uint32_t						queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const Move<VkCommandPool>			cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	Move<VkShaderModule>				computeShader			= createShaderModule (vk, device, m_context.getBinaryCollection().get("comp"), 0u);

	// The maximum number of invocations in a workgroup.
	const uint32_t						maxLocalSize			= m_context.getDeviceProperties().limits.maxComputeWorkGroupSize[0];
	const uint32_t						minSubgroupSize			= m_context.getSubgroupSizeControlProperties().minSubgroupSize;

	// Create a storage buffer to hold the sizes of subgroups.
	const VkDeviceSize					bufferSize				= (maxLocalSize / minSubgroupSize + 1u) * sizeof(uint32_t);

	const VkBufferCreateInfo			resultBufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory					resultBuffer			(vk, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&								resultBufferAlloc		= resultBuffer.getAllocation();

	// Build descriptors for the storage buffer
	const Unique<VkDescriptorPool>		descriptorPool			(DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																						.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	const auto							descriptorSetLayout1	(DescriptorSetLayoutBuilder().addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
																							 .build(vk, device));
	const VkDescriptorBufferInfo		resultInfo				= makeDescriptorBufferInfo(*resultBuffer, 0u, bufferSize);

	const VkDescriptorSetAllocateInfo	allocInfo				=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// sType
		DE_NULL,										// pNext
		*descriptorPool,								// descriptorPool
		1u,												// descriptorSetCount
		&(*descriptorSetLayout1)						// pSetLayouts
	};

	Move<VkDescriptorSet>				descriptorSet			= allocateDescriptorSet(vk, device, &allocInfo);
	DescriptorSetUpdateBuilder			builder;

	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo);
	builder.update(vk, device);

	// Compute pipeline
	const Move<VkPipelineLayout>		computePipelineLayout	= makePipelineLayout (vk, device, *descriptorSetLayout1);

	for (uint32_t localSize = 1u; localSize <= maxLocalSize; localSize *= 2u)
	{
		// On each iteration, change the number of invocations which might affect
		// the subgroup size.
		const VkSpecializationMapEntry			entries					=
		{
			0u,					// uint32_t constantID;
			0u,					// uint32_t offset;
			sizeof(localSize)	// size_t size;
		};

		const VkSpecializationInfo				specInfo				=
		{
			1,					// mapEntryCount
			&entries,			// pMapEntries
			sizeof(localSize),	// dataSize
			&localSize			// pData
		};

		const VkPipelineShaderStageCreateInfo	shaderStageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,					// sType
			DE_NULL,																// pNext
			VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,	// flags
			VK_SHADER_STAGE_COMPUTE_BIT,											// stage
			*computeShader,															// module
			"main",																	// pName
			&specInfo,																// pSpecializationInfo
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo		=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// sType
			DE_NULL,										// pNext
			0u,												// flags
			shaderStageCreateInfo,							// stage
			*computePipelineLayout,							// layout
			(VkPipeline) 0,									// basePipelineHandle
			0u,												// basePipelineIndex
		};

		Move<VkPipeline>						computePipeline			= createComputePipeline(vk, device, (VkPipelineCache) 0u, &pipelineCreateInfo);

		beginCommandBuffer(vk, *cmdBuffer);

		// Clears the values in the buffer.
		vk.cmdFillBuffer(*cmdBuffer, *resultBuffer, 0u, VK_WHOLE_SIZE, 0);

		const auto fillBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, *resultBuffer, 0ull, bufferSize);
		cmdPipelineBufferMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &fillBarrier);

		// Runs pipeline.
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
		vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

		const auto computeToHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &computeToHostBarrier);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		invalidateAlloc(vk, device, resultBufferAlloc);

		// Validate results: all non-zero subgroup sizes must be the same.
		const uint32_t							*res					= static_cast<const uint32_t *>(resultBufferAlloc.getHostPtr());
		const uint32_t							maxIters				= static_cast<uint32_t>(bufferSize / sizeof(uint32_t));
		uint32_t								size					= 0u;
		uint32_t								subgroupCount			= 0u;
		auto&									log						= m_context.getTestContext().getLog();

		for (uint32_t sizeIdx = 0u; sizeIdx < maxIters; ++sizeIdx)
		{
			if (res[sizeIdx] != 0u)
			{
				if (size == 0u)
				{
					size = res[sizeIdx];
				}
				else if (res[sizeIdx] != size)
				{
					std::ostringstream msg;
					msg << "Subgroup size not uniform in command scope: " << res[sizeIdx] << " != " << size << " at position " << sizeIdx;
					TCU_FAIL(msg.str());
				}
				++subgroupCount;
			}
		}

		// Subgroup size is guaranteed to be at least 1.
		if (size == 0u)
			TCU_FAIL("Subgroup size must be at least 1");

		// The number of reported sizes must match.
		const auto expectedSubgroupCount = (localSize / size + ((localSize % size != 0u) ? 1u : 0u));
		if (subgroupCount != expectedSubgroupCount)
		{
			std::ostringstream msg;
			msg << "Local size " << localSize << " with subgroup size " << size << " resulted in subgroup count " << subgroupCount << " != " << expectedSubgroupCount;
			TCU_FAIL(msg.str());
		}

		{
			std::ostringstream msg;
			msg << "Subgroup size " << size << " with local size " << localSize;
			log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class MultipleDispatchesUniformSubgroupSize : public TestCase
{
public:
						MultipleDispatchesUniformSubgroupSize (tcu::TestContext&	testCtx,
															   const std::string&	name,
															   const std::string&	description);

	void				initPrograms						  (SourceCollections&	programCollection) const;
	TestInstance*		createInstance						  (Context&				context) const;
	virtual void		checkSupport						  (Context&				context) const;

};

MultipleDispatchesUniformSubgroupSize::MultipleDispatchesUniformSubgroupSize (tcu::TestContext&	testCtx,
																			  const std::string&	name,
																			  const std::string&	description)
	: TestCase	(testCtx, name, description)
{
}

void MultipleDispatchesUniformSubgroupSize::checkSupport (Context& context) const
{
	const auto& subgroupSizeControlFeatures = context.getSubgroupSizeControlFeatures();

	if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
		TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes");
}

void MultipleDispatchesUniformSubgroupSize::initPrograms (SourceCollections& programCollection) const
{
	std::ostringstream computeSrc;
	computeSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "#extension GL_KHR_shader_subgroup_basic : enable\n"
		<< "#extension GL_KHR_shader_subgroup_vote : enable\n"
		<< "#extension GL_KHR_shader_subgroup_ballot : enable\n"
		<< "layout(std430, binding = 0) buffer Outputs { uint sizes[]; };\n"

		<< "layout(local_size_x_id = 0) in;\n"

		<< "void main()\n"
		<< "{\n"
		<< "    if (subgroupElect())\n"
		<< "    {\n"
		<< "        sizes[gl_WorkGroupID.x * gl_NumSubgroups + gl_SubgroupID] = gl_SubgroupSize;\n"
		<< "    }\n"
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(computeSrc.str())
	<< ShaderBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
}

TestInstance* MultipleDispatchesUniformSubgroupSize::createInstance (Context& context) const
{
	return new MultipleDispatchesUniformSubgroupSizeInstance(context);
}

} // anonymous ns

tcu::TestCaseGroup* createMultipleDispatchesUniformSubgroupSizeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "multiple_dispatches", "Multiple dispatches uniform subgroup size tests"));

	testGroup->addChild(new MultipleDispatchesUniformSubgroupSize(testCtx, "uniform_subgroup_size", ""));
	return testGroup.release();
}

} // compute
} // vkt
