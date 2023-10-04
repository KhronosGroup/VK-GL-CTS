/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief SPIR-V Assembly Tests for indexing with access chain operations.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmFromHlslTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPrograms.hpp"
#include "vkObjUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

using namespace vk;

enum TestType
{
	TT_CBUFFER_PACKING		= 0,
};

struct TestConfig
{
	TestType	type;
};

struct Programs
{
	void init (vk::SourceCollections& dst, TestConfig config) const
	{
		if (config.type == TT_CBUFFER_PACKING)
		{
			// HLSL shaders has a packing corner case that GLSL shaders cannot exhibit.
			// Below shader, foo has an ArrayStride of 16, which leaves bar effectively
			// 'within' the end of the foo array. This is entirely valid for HLSL and
			// with the VK_EXT_scalar_block_layout extension.
			std::string source(
				"cbuffer cbIn\n"
				"{\n"
				"  int foo[2] : packoffset(c0);\n"
				"  int bar    : packoffset(c1.y);\n"
				"};\n"
				"RWStructuredBuffer<int> result : register(u1);\n"
				"[numthreads(1, 1, 1)]\n"
				"void main(uint3 dispatchThreadID : SV_DispatchThreadID)\n"
				"{\n"
				"  result[0] = bar;\n"
				"}\n");

			dst.hlslSources.add("comp") << glu::ComputeSource(source)
				<< vk::ShaderBuildOptions(dst.usedVulkanVersion, vk::SPIRV_VERSION_1_0, vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);
		}
	}
};

class HlslTest : public TestInstance
{
public:
						HlslTest	(Context& context, TestConfig config);
	virtual				~HlslTest	(void) = default;

	tcu::TestStatus		iterate (void);
};


HlslTest::HlslTest(Context& context, TestConfig config)
	: TestInstance(context)
{
	DE_UNREF(config);
}

tcu::TestStatus HlslTest::iterate(void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();
	const int				testValue			= 5;

	// Create an input buffer
	const VkBufferUsageFlags	inBufferUsageFlags		= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	const VkDeviceSize			inBufferSizeBytes		= 32; // 2 element array with 16B stride
	VkBufferCreateInfo			inBufferCreateInfo		= makeBufferCreateInfo(inBufferSizeBytes, inBufferUsageFlags);
	vk::Move<vk::VkBuffer>		inBuffer				= createBuffer(vk, device, &inBufferCreateInfo);
	de::MovePtr<vk::Allocation>	inAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *inBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(device, *inBuffer, inAllocation->getMemory(), inAllocation->getOffset()));

	// Fill the input structure with data - first attribute is array that has 16B stride,
	// this means that second attribute has to start at offset 20B (4B + 16B)
	{
		int* bufferPtr = static_cast<int*>(inAllocation->getHostPtr());
		memset(bufferPtr, 0, inBufferSizeBytes);
		bufferPtr[5] = testValue;
		flushAlloc(vk, device, *inAllocation);
	}

	// Create an output buffer
	const VkBufferUsageFlags	outBufferUsageFlags	= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const VkDeviceSize			outBufferSizeBytes	= sizeof(int);
	VkBufferCreateInfo			outBufferCreateInfo	= makeBufferCreateInfo(outBufferSizeBytes, outBufferUsageFlags);
	vk::Move<vk::VkBuffer>		outBuffer			= createBuffer(vk, device, &outBufferCreateInfo);
	de::MovePtr<vk::Allocation>	outAllocation		= allocator.allocate(getBufferMemoryRequirements(vk, device, *outBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(device, *outBuffer, outAllocation->getMemory(), outAllocation->getOffset()));

	// Create descriptor set
	const VkDescriptorType uniBufDesc	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	const VkDescriptorType storBufDesc	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(uniBufDesc, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(storBufDesc, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(uniBufDesc)
		.addType(storBufDesc)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo inputBufferDescriptorInfo = makeDescriptorBufferInfo(*inBuffer, 0ull, inBufferSizeBytes);
	const VkDescriptorBufferInfo outputBufferDescriptorInfo = makeDescriptorBufferInfo(*outBuffer, 0ull, outBufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), uniBufDesc, &inputBufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), storBufDesc, &outputBufferDescriptorInfo)
		.update(vk, device);

	// Perform the computation
	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		static_cast<VkPipelineShaderStageCreateFlags>(0u),
		VK_SHADER_STAGE_COMPUTE_BIT,
		*shaderModule,
		"main",
		DE_NULL,
	};
	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		static_cast<VkPipelineCreateFlags>(0u),
		pipelineShaderStageParams,
		*pipelineLayout,
		DE_NULL,
		0,
	};
	Unique<VkPipeline> pipeline(createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo));
	const VkBufferMemoryBarrier hostWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inBuffer, 0ull, inBufferSizeBytes);
	const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outBuffer, 0ull, outBufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands
	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results
	invalidateAlloc(vk, device, *outAllocation);
	const int* bufferPtr = static_cast<int*>(outAllocation->getHostPtr());
	if (*bufferPtr != testValue)
		return tcu::TestStatus::fail("Fail");
	return tcu::TestStatus::pass("Pass");
}

void checkSupport(Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_scalar_block_layout");
}

} // anonymous

tcu::TestCaseGroup* createHlslComputeGroup (tcu::TestContext& testCtx)
{
	typedef InstanceFactory1WithSupport<HlslTest, TestConfig, FunctionSupport0, Programs> HlslTestInstance;
	de::MovePtr<tcu::TestCaseGroup> hlslCasesGroup(new tcu::TestCaseGroup(testCtx, "hlsl_cases", ""));

	TestConfig testConfig = { TT_CBUFFER_PACKING };
	hlslCasesGroup->addChild(new HlslTestInstance(testCtx, "cbuffer_packing", "", testConfig, checkSupport));

	return hlslCasesGroup.release();
}

} // SpirVAssembly
} // vkt
