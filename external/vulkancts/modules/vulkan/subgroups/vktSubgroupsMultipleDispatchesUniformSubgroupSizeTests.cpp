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
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include "vktTestCaseUtil.hpp"

using namespace vk;

namespace vkt
{
namespace subgroups
{
namespace
{
using std::vector;
using de::MovePtr;

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
	const deUint32						queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const Move<VkCommandPool>			cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	Move<VkShaderModule>				computeShader			= createShaderModule (vk, device, m_context.getBinaryCollection().get("comp"), 0u);

	// The number of invocations in a workgroup.
	const deUint32						maxLocalSize			= m_context.getDeviceProperties().limits.maxComputeWorkGroupSize[0];

	// Create a storage buffer to hold the sizes of subgroups.
	const VkDeviceSize					bufferSize				= maxLocalSize * 2 * sizeof(deUint32);

	const VkBufferCreateInfo			resultBufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	Move<VkBuffer>						resultBuffer			= createBuffer(vk, device, &resultBufferCreateInfo);
	MovePtr<Allocation>					resultBufferMemory		= allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer), MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));

	// Build descriptors for the storage buffer
	const Unique<VkDescriptorPool>		descriptorPool			(DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
																						.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	const auto							descriptorSetLayout1	(DescriptorSetLayoutBuilder().addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_COMPUTE_BIT)
																							 .build(vk, device));
	const VkDescriptorBufferInfo		resultInfo				= makeDescriptorBufferInfo(*resultBuffer, 0u,
																						   (VkDeviceSize) bufferSize - maxLocalSize * sizeof(deUint32));

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

	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, &resultInfo);
	builder.update(vk, device);

	// Compute pipeline
	const Move<VkPipelineLayout>		computePipelineLayout	= makePipelineLayout (vk, device, *descriptorSetLayout1);

	for (deUint32 localSize1 = 8; localSize1 < maxLocalSize + 1; localSize1 *= 2)
	{
		for (deUint32 localSize2 = 8; localSize2 < maxLocalSize + 1; localSize2 *= 2)
		{
			// On each iteration, change the number of invocations which might affect
			// the subgroup size if the driver doesn't behave as expected.
			const VkSpecializationMapEntry			entries					=
			{
				0u,					// deUint32 constantID;
				0u,					// deUint32 offset;
				sizeof(localSize1)	// size_t size;
			};
			const VkSpecializationInfo				specInfo				=
			{
				1,					// mapEntryCount
				&entries,			// pMapEntries
				sizeof(localSize1),	// dataSize
				&localSize1			// pData
			};
			const VkSpecializationInfo				specInfo2				=
			{
				1,					// mapEntryCount
				&entries,			// pMapEntries
				sizeof(localSize2),	// dataSize
				&localSize2			// pData
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

			const VkPipelineShaderStageCreateInfo	shaderStageCreateInfo2	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,					// sType
				DE_NULL,																// pNext
				VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,	// flags
				VK_SHADER_STAGE_COMPUTE_BIT,											// stage
				*computeShader,															// module
				"main",																	// pName
				&specInfo2,																// pSpecializationInfo
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

			const VkComputePipelineCreateInfo		pipelineCreateInfo2		=
			{
				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// sType
				DE_NULL,										// pNext
				0u,												// flags
				shaderStageCreateInfo2,							// stage
				*computePipelineLayout,							// layout
				(VkPipeline) 0,									// basePipelineHandle
				0u,												// basePipelineIndex
			};

			Move<VkPipeline>						computePipeline			= createComputePipeline(vk, device, (VkPipelineCache) 0u, &pipelineCreateInfo);
			Move<VkPipeline>						computePipeline2		= createComputePipeline(vk, device, (VkPipelineCache) 0u, &pipelineCreateInfo2);

			beginCommandBuffer(vk, *cmdBuffer);

			// Clears the values written on the previous iteration.
			vk.cmdFillBuffer(*cmdBuffer, *resultBuffer, 0u, VK_WHOLE_SIZE, 0);

			const auto								fillBarrier				= makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, *resultBuffer, 0ull, bufferSize);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags) 0,
								  0, (const VkMemoryBarrier *) DE_NULL, 1, &fillBarrier, 0, (const VkImageMemoryBarrier *) DE_NULL);

			const deUint32							zero					= 0u;
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u, &descriptorSet.get(), 1, &zero);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
			vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

			const auto								barrier					= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, *resultBuffer, 0ull, bufferSize);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags) 0,
								  0, (const VkMemoryBarrier *) DE_NULL, 1, &barrier, 0, (const VkImageMemoryBarrier *) DE_NULL);

			const deUint32							offset					= static_cast<deUint32>(maxLocalSize * sizeof(deUint32));
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u, &descriptorSet.get(), 1u, &offset);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline2);
			vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

			endCommandBuffer(vk, *cmdBuffer);
			submitCommandsAndWait(vk, device, queue, *cmdBuffer);

			invalidateAlloc(vk, device, *resultBufferMemory);

			const deUint32							*res					= static_cast<const deUint32 *>(resultBufferMemory->getHostPtr());
			deUint32								size					= 0;

			// Search for the first nonzero size. Then go through the data of both pipelines and check that
			// the first nonzero size matches with other nonzero values.
			for (deUint32 i = 0; i < maxLocalSize; i++)
			{
				if (res[i] != 0)
				{
					size = res[i];
					break;
				}
			}

			// Subgroup size is guaranteed to be at least 1.
			DE_ASSERT(size > 0);

			for (deUint32 i = 0; i < maxLocalSize * 2; i++)
			{
				if (size != res[i] && res[i] != 0)
					return tcu::TestStatus::fail("Subgroup size not uniform in command scope. " + std::to_string(res[i]) + " != " + std::to_string(size));
			}
		}
	}

	return tcu::TestStatus::pass("pass");
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
	const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT&	subgroupSizeControlFeatures	= context.getSubgroupSizeControlFeatures();

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
