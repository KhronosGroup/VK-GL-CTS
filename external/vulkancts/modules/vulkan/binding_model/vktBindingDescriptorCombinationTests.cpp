/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \file vktBindingDescriptorCombinationTests.cpp
 * \brief Test using both descriptor buffers & legacy descriptors in
 *        the same command buffer.
 *//*--------------------------------------------------------------------*/

#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"
#include "vktBindingDescriptorCombinationTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkRefUtil.hpp"
#include <utility>
#include <array>

namespace vkt::BindingModel
{
namespace
{

using namespace vk;

enum class TestType
{
	DESCRIPTOR_BUFFER_AND_LEGACY_DESCRIPTOR_IN_COMMAND_BUFFER = 0,
};

struct TestParams
{
	TestType testType;
};

class DescriptorCombinationTestInstance : public TestInstance
{
public:
	DescriptorCombinationTestInstance	(Context&				context,
										 const TestParams&		params);

	tcu::TestStatus			iterate		() override;

protected:

	Move<VkPipeline>		createBasicPipeline(VkPipelineLayout layout, VkShaderModule shaderModule, VkPipelineCreateFlags flags = 0) const;

protected:
	TestParams				m_params;
};

DescriptorCombinationTestInstance::DescriptorCombinationTestInstance(Context& context, const TestParams& params)
	: TestInstance							(context)
	, m_params								(params)
{
}

tcu::TestStatus DescriptorCombinationTestInstance::iterate()
{
	// test using both descriptor buffers & legacy descriptors in the same command buffer

	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create three storage buffers, one for each way we setup descriptors
	using BufferWithMemorySp = de::SharedPtr<BufferWithMemory>;
	const VkDeviceSize			bufferSize			= static_cast<VkDeviceSize>(16 * sizeof(deUint32));
	const auto					bufferUsage			= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VkBufferCreateInfo			bufferCreateInfo	= makeBufferCreateInfo(bufferSize, bufferUsage);
	BufferWithMemorySp			bufferForLegacyDS	= BufferWithMemorySp(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
	BufferWithMemorySp			bufferForPushDesc	= BufferWithMemorySp(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
	bufferCreateInfo.usage							|= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	BufferWithMemorySp			bufferForDescBuffer	= BufferWithMemorySp(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));

	// Create descriptor pool - we need just one descriptor set
	const VkDescriptorType descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(descType, 1)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	// Create three descriptor layouts
	const Unique<VkDescriptorSetLayout> descriptorSetLayoutForPushDesc(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
	const Unique<VkDescriptorSetLayout> descriptorSetLayoutForLegacyDS(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));
	const Unique<VkDescriptorSetLayout> descriptorSetLayoutForDescBuffer(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

	// Create legacy descriptor set
	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayoutForLegacyDS));
	VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(**bufferForLegacyDS, 0ull, bufferSize);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &bufferDescriptorInfo)
		.update(vk, device);

	// Define WriteDescriptorSet for push descriptor
	bufferDescriptorInfo.buffer = **bufferForPushDesc;
	VkWriteDescriptorSet descriptorWrites = initVulkanStructure();
	descriptorWrites.descriptorCount = 1u;
	descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrites.pBufferInfo = &bufferDescriptorInfo;

	// Check how big descriptor buffer we need
	VkDeviceSize descriptorBufferSize;
	vk.getDescriptorSetLayoutSizeEXT(device, *descriptorSetLayoutForDescBuffer, &descriptorBufferSize);

	// Create descriptor buffer with needed size
	const auto					descriptorBufferUsage		= VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	const VkBufferCreateInfo	descriptorBufferCreateInfo	= makeBufferCreateInfo(descriptorBufferSize, descriptorBufferUsage);
	const MemoryRequirement		bufferMemoryRequirement		= MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress;
	BufferWithMemorySp			descriptorBuffer			= BufferWithMemorySp(new BufferWithMemory(vk, device, allocator, descriptorBufferCreateInfo, bufferMemoryRequirement));
	char*						descriptorBufferHostPtr		= static_cast<char*>(descriptorBuffer->getAllocation().getHostPtr());

	// Get adress of ssbo to which we will write in shader
	VkBufferDeviceAddressInfo bdaInfo = initVulkanStructure();
	bdaInfo.buffer = **bufferForDescBuffer;
	VkDescriptorAddressInfoEXT descriptorAddressInfo = initVulkanStructure();
	descriptorAddressInfo.address	= vk.getBufferDeviceAddress(device, &bdaInfo);
	descriptorAddressInfo.range		= bufferSize;

	VkDescriptorGetInfoEXT descriptorGetInfo = initVulkanStructure();
	descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorGetInfo.data.pStorageBuffer = &descriptorAddressInfo;

	VkDeviceSize	offset	= 0;
	std::size_t		size	= m_context.getDescriptorBufferPropertiesEXT().storageBufferDescriptorSize;
	vk.getDescriptorSetLayoutBindingOffsetEXT(device, *descriptorSetLayoutForDescBuffer, 0, &offset);
	vk.getDescriptorEXT(device, &descriptorGetInfo, size, descriptorBufferHostPtr + offset);
	flushAlloc(vk, device, descriptorBuffer->getAllocation());

	// Get adress of descriptor buffer
	bdaInfo.buffer = **descriptorBuffer;
	VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo = initVulkanStructure();
	descriptorBufferBindingInfo.address = vk.getBufferDeviceAddress(device, &bdaInfo);
	descriptorBufferBindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

	VkMemoryBarrier memoryBarrier	= initVulkanStructure();
	memoryBarrier.srcAccessMask		= VK_ACCESS_SHADER_WRITE_BIT;
	memoryBarrier.dstAccessMask		= VK_ACCESS_SHADER_READ_BIT;

	const VkPushConstantRange pushConstantRange
	{
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,					// offset
		sizeof(int)			// size
	};

	const deUint32		descriptorBufferIndices = 0;
	const VkDeviceSize	descriptorBufferoffsets = 0;
	deUint32			pushConstValue			= 3;
	BinaryCollection&	binaryCollection		= m_context.getBinaryCollection();

	const auto	shaderModuleInit			= createShaderModule(vk, device, binaryCollection.get("comp_init"));
	const auto	shaderModuleAdd				= createShaderModule(vk, device, binaryCollection.get("comp_add"));
	const auto	pipelineLayoutForLegacyDS	= makePipelineLayout(vk, device, *descriptorSetLayoutForLegacyDS, &pushConstantRange);
	const auto	pipelineLayoutForPushDesc	= makePipelineLayout(vk, device, *descriptorSetLayoutForPushDesc, &pushConstantRange);
	const auto	pipelineLayoutForDescBuffer	= makePipelineLayout(vk, device, *descriptorSetLayoutForDescBuffer, &pushConstantRange);
	const auto	pipelineInitForPushDesc		= createBasicPipeline(*pipelineLayoutForPushDesc,   *shaderModuleInit);
	const auto	pipelineInitForLegacyDS		= createBasicPipeline(*pipelineLayoutForLegacyDS,   *shaderModuleInit);
	const auto	pipelineInitForDescBuffer	= createBasicPipeline(*pipelineLayoutForDescBuffer, *shaderModuleInit, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
	const auto	pipelineAddForPushDesc		= createBasicPipeline(*pipelineLayoutForPushDesc,   *shaderModuleAdd);
	const auto	pipelineAddForLegacyDS		= createBasicPipeline(*pipelineLayoutForLegacyDS,   *shaderModuleAdd);
	const auto	pipelineAddForDescBuffer	= createBasicPipeline(*pipelineLayoutForDescBuffer, *shaderModuleAdd, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
	auto		cmdPool						= makeCommandPool(vk, device, m_context.getUniversalQueueFamilyIndex());
	auto		cmdBuffer					= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer);

	// use push descriptor
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineInitForPushDesc);
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForPushDesc, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &pushConstValue);
	vk.cmdPushDescriptorSetKHR(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForPushDesc, 0, 1, &descriptorWrites);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	// use legacy descriptor
	pushConstValue = 5;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineInitForLegacyDS);
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForLegacyDS, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &pushConstValue);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForLegacyDS, 0u, 1u, &*descriptorSet, 0u, 0);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	// use descriptor buffer
	pushConstValue = 6;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineInitForDescBuffer);
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForDescBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &pushConstValue);
	vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1, &descriptorBufferBindingInfo);
	vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForDescBuffer, 0, 1, &descriptorBufferIndices, &descriptorBufferoffsets);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, 0, 0, 0);

	// use push descriptor
	pushConstValue = 2;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForPushDesc);
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForPushDesc, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &pushConstValue);
	vk.cmdPushDescriptorSetKHR(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForPushDesc, 0, 1, &descriptorWrites);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	// use legacy descriptor
	pushConstValue = 1;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForLegacyDS);
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForLegacyDS, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &pushConstValue);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForLegacyDS, 0u, 1u, &*descriptorSet, 0u, 0);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, 0, 0, 0);

	// use push descriptor
	pushConstValue = 2;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForPushDesc);
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForPushDesc, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &pushConstValue);
	vk.cmdPushDescriptorSetKHR(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForPushDesc, 0, 1, &descriptorWrites);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	// use descriptor buffer
	pushConstValue = 3;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForDescBuffer);
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForDescBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &pushConstValue);
	vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1, &descriptorBufferBindingInfo);
	vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForDescBuffer, 0, 1, &descriptorBufferIndices, &descriptorBufferoffsets);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, 0, 0, 0);

	// use push descriptor
	pushConstValue = 2;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForPushDesc);
	vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForPushDesc, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &pushConstValue);
	vk.cmdPushDescriptorSetKHR(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForPushDesc, 0, 1, &descriptorWrites);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

	std::pair<BufferWithMemorySp, std::array<int, 16> > expectedResults[]
	{
		{ bufferForLegacyDS,	{ 1, 6, 11, 16,		21, 26, 31, 36,		41, 46, 51, 56,		61, 66, 71, 76 }},
		{ bufferForPushDesc,	{ 6, 9, 12, 15,		18, 21, 24, 27,		30, 33, 36, 39,		42, 45, 48, 51 }},
		{ bufferForDescBuffer,	{ 3, 9, 15, 21,		27, 33, 39, 45,		51, 57, 63, 69,		75, 81, 87, 93 }},
	};

	// Verify all three result buffers
	for (const auto& [bufferWithemory, expectedValues] : expectedResults)
	{
		auto& allocation = bufferWithemory->getAllocation();
		invalidateAlloc(vk, device, allocation);

		int* bufferPtr = static_cast<int*>(allocation.getHostPtr());
		if (deMemCmp(bufferPtr, expectedValues.data(), bufferSize) != 0)
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

Move<VkPipeline> DescriptorCombinationTestInstance::createBasicPipeline(VkPipelineLayout layout, VkShaderModule shaderModule, VkPipelineCreateFlags flags) const
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	const VkComputePipelineCreateInfo pipelineCreateInfo
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,				// VkStructureType						sType
		nullptr,													// const void*							pNext
		flags,														// VkPipelineCreateFlags				flags
		{															// VkPipelineShaderStageCreateInfo		stage
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//		VkStructureType						sType
			nullptr,												//		const void*							pNext
			0u,														//		VkPipelineShaderStageCreateFlags	flags
			VK_SHADER_STAGE_COMPUTE_BIT,							//		VkShaderStageFlagBits				stage
			shaderModule,											//		VkShaderModule						module
			"main",													//		const char*							pName
			nullptr,												//		const VkSpecializationInfo*			pSpecializationInfo
		},
		layout,														// VkPipelineLayout						layout
		DE_NULL,													// VkPipeline							basePipelineHandle
		0,															// deInt32								basePipelineIndex
	};

	return createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

class DescriptorCombinationTestCase : public TestCase
{
public:
	DescriptorCombinationTestCase	(tcu::TestContext&			testCtx,
									 const std::string&			name,
									 const TestParams&			params);

	void			checkSupport	(Context&					context) const override;
	void			initPrograms	(vk::SourceCollections&		programCollection) const override;
	TestInstance*	createInstance	(Context&					context) const override;

private:
	const TestParams m_params;
};

DescriptorCombinationTestCase::DescriptorCombinationTestCase(tcu::TestContext&	testCtx,
															 const std::string&	name,
															 const TestParams&	params)
	: TestCase(testCtx, name)
	, m_params(params)
{
}

void DescriptorCombinationTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_descriptor_buffer");
	context.requireDeviceFunctionality("VK_KHR_push_descriptor");
}

void DescriptorCombinationTestCase::initPrograms (vk::SourceCollections& programs) const
{
	const char* compInitSrc
	{
		"#version 460\n"
		"layout(local_size_x = 4, local_size_y = 4) in;\n"
		"layout(push_constant) uniform Params { int mulVal; } params;\n"
		"layout(binding = 0, std430) buffer OutBuf { uint v[]; } outBuf;\n"
		"void main()\n"
		"{\n"
		"  outBuf.v[gl_LocalInvocationIndex] = gl_LocalInvocationIndex * params.mulVal;\n"
		"}\n"
	};

	const char* compAddSrc
	{
		"#version 460\n"
		"layout(local_size_x = 4, local_size_y = 4) in;\n"
		"layout(push_constant) uniform Params { int addVal; } params;\n"
		"layout(binding = 0, std430) buffer InOutBuf { uint v[]; } inOutBuf;\n"
		"void main()\n"
		"{\n"
		"  uint value = inOutBuf.v[gl_LocalInvocationIndex];"
		"  inOutBuf.v[gl_LocalInvocationIndex] = value + params.addVal;\n"
		"}\n"
	};

	programs.glslSources.add("comp_init") << glu::ComputeSource(compInitSrc);
	programs.glslSources.add("comp_add") << glu::ComputeSource(compAddSrc);
}

TestInstance* DescriptorCombinationTestCase::createInstance (Context& context) const
{
	return new DescriptorCombinationTestInstance(context, m_params);
}

void populateDescriptorCombinationTests(tcu::TestCaseGroup* topGroup)
{
	tcu::TestContext& testCtx = topGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup> basicGroup (new tcu::TestCaseGroup(testCtx, "basic"));
	basicGroup->addChild(new DescriptorCombinationTestCase(testCtx, "descriptor_buffer_and_legacy_descriptor_in_command_buffer", { TestType::DESCRIPTOR_BUFFER_AND_LEGACY_DESCRIPTOR_IN_COMMAND_BUFFER }));

	topGroup->addChild(basicGroup.release());
}

} // anonymous

tcu::TestCaseGroup* createDescriptorCombinationTests(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "descriptor_combination", populateDescriptorCombinationTests);
}

} // vkt::BindingModel
