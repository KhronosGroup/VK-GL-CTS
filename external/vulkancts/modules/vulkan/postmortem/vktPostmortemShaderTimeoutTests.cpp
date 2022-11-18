/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Experimental crash postmortem shader timeout tests
 *//*--------------------------------------------------------------------*/

#include "vktPostmortemTests.hpp"
#include "vktPostmortemShaderTimeoutTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuCommandLine.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktPostmortemUtil.hpp"

using namespace vk;

namespace vkt
{
namespace postmortem
{
namespace
{

class ShaderTimeoutCase : public vkt::TestCase
{
public:
	ShaderTimeoutCase(tcu::TestContext& testCtx, const std::string& name, deUint32 iterations) : TestCase(testCtx, name, "Long-running compute shader"), m_iterations(iterations) {}

	TestInstance* createInstance(Context& context) const override;
	void initPrograms(vk::SourceCollections& programCollection) const override;

private:
	deUint32 m_iterations;
};

class ShaderTimeoutInstance : public PostmortemTestInstance
{
public:
	ShaderTimeoutInstance(Context& context, deUint32 iterations);

	tcu::TestStatus		iterate(void) override;

private:
	deUint32			m_iterations;
};

ShaderTimeoutInstance::ShaderTimeoutInstance(Context& context, deUint32 iterations)
	: PostmortemTestInstance(context), m_iterations(iterations)
{

}

TestInstance* ShaderTimeoutCase::createInstance(Context& context) const
{
	return new ShaderTimeoutInstance(context, m_iterations);
}

void ShaderTimeoutCase::initPrograms(vk::SourceCollections& programCollection) const
{
	std::ostringstream src;
	src << "#version 320 es\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1)\n"
		<< "layout(binding = 0) uniform Params {\n"
		<< "  int x;\n"
		<< "  int y;\n"
		<< "} bounds;\n"
		<< "layout(std430, binding = 1) buffer  Output {\n"
		<< "  uint values[];\n"
		<< "} sb_out;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  uint localSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n"
		<< "  uint globalNdx = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z + gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		<< "  uint globalOffs = localSize * globalNdx;\n"
		<< "  uint localOffs = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_LocalInvocationID.z + gl_WorkGroupSize.x * gl_LocalInvocationID.y + gl_LocalInvocationID.x;\n"
		<< "  uint sum = uint(0);\n"
		<< "  for (int y = 0; y < bounds.y; ++y) {\n"
		<< "    for (int x = 0; x < bounds.x; ++x) {\n"
		<< "	  sb_out.values[globalOffs + localOffs] = sb_out.values[globalOffs + localOffs] + uint(1);\n"
		<< "      memoryBarrierBuffer();\n"
		<< "      barrier();\n"
		<< "    }\n"
		<< "  }\n"
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
}

tcu::TestStatus	ShaderTimeoutInstance::iterate(void)
{
	const VkDevice			device				= *m_logicalDevice;
	const DeviceInterface&	vk					= m_deviceDriver;
	const VkQueue			queue				= m_queue;
	const deUint32			queueFamilyIndex	= m_queueFamilyIndex;
	Allocator&				allocator			= m_allocator;

	const int workSize = 1024;
	const VkDeviceSize storageSizeInBytes = sizeof(deUint32) * workSize;
	const VkDeviceSize uniformSizeInBytes = sizeof(deUint32) * 2;

	// Create storage and uniform buffers
	BufferWithMemory storageBuffer(vk, device, allocator,
		makeBufferCreateInfo(storageSizeInBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
		MemoryRequirement::HostVisible);
	BufferWithMemory uniformBuffer(vk, device, allocator,
		makeBufferCreateInfo(uniformSizeInBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
		MemoryRequirement::HostVisible);

	// Fill storage buffer with sequentially increasing values
	{
		const Allocation& storageBufferAllocation = storageBuffer.getAllocation();
		deUint32* storageBufferPtr = static_cast<deUint32*>(storageBufferAllocation.getHostPtr());
		for (int i = 0; i < workSize; ++i)
			storageBufferPtr[i] = i;

		flushAlloc(vk, device, storageBufferAllocation);
	}

	// Set uniforms for shader loop bounds to m_iterations
	{
		const Allocation& uniformBufferAllocation = uniformBuffer.getAllocation();
		deUint32* uniformBufferPtr = static_cast<deUint32*>(uniformBufferAllocation.getHostPtr());
		uniformBufferPtr[0] = m_iterations;
		uniformBufferPtr[1] = m_iterations;

		flushAlloc(vk, device, uniformBufferAllocation);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo uniformDescriptorInfo = makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformSizeInBytes);
	const VkDescriptorBufferInfo storageDescriptorInfo = makeDescriptorBufferInfo(*storageBuffer, 0ull, storageSizeInBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageDescriptorInfo)
		.update(vk, device);

	// Create pipelines
	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier hostWriteBarriers[2] =
	{
		makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *storageBuffer, 0ull, storageSizeInBytes),
		makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformSizeInBytes)
	};
	const VkBufferMemoryBarrier computeFinishBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *storageBuffer, 0ull, storageSizeInBytes);

	// Create command buffer and launch dispatch,
	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 2u, hostWriteBarriers, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdDispatch(*cmdBuffer, workSize, 1, 1);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*) DE_NULL, 1u, &computeFinishBarrier, 0, (const VkImageMemoryBarrier*) DE_NULL);
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify output
	const Allocation& storageAllocation = storageBuffer.getAllocation();
	invalidateAlloc(vk, device, storageAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(storageAllocation.getHostPtr());
	for (int i = 0; i < workSize; ++i)
	{
		const deUint32	res = bufferPtr[i];
		const deUint32	ref = i + m_iterations * m_iterations;
		if (res != ref)
		{
			std::ostringstream msg;
			msg << "Comparison failed for sb_out.values[" << i << "] ref:" << ref << " res:" << res;
			return tcu::TestStatus::fail(msg.str());
		}
	}

	return tcu::TestStatus::pass("Test succeeded without device loss");
}

}

tcu::TestCaseGroup* createShaderTimeoutTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> timeoutGroup(new tcu::TestCaseGroup(testCtx, "shader_timeout", "Shader timeout tests."));
	for (int i = 0; i < 16; ++i)
	{
		deUint32 iterations = 0x1u << i;
		std::stringstream name;
		name << "compute_" << iterations << "x" << iterations;
		timeoutGroup->addChild(new ShaderTimeoutCase(testCtx, name.str(), iterations));
	}

	return timeoutGroup.release();
}

} // postmortem
} // vkt
