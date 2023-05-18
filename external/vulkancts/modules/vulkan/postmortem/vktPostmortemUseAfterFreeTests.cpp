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
 * \brief Experimental crash postmortem use after free tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktPostmortemTests.hpp"
#include "vktPostmortemUseAfterFreeTests.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "vktPostmortemUtil.hpp"

#include <vector>
#include <memory>

using namespace vk;

namespace vkt
{
namespace postmortem
{
namespace
{

enum BufferType
{
	BUFFER_TYPE_UNIFORM = 0,
	BUFFER_TYPE_SSBO,
};

class Buffer
{
public:
						Buffer			(const vk::DeviceInterface&		vk,
										const vk::VkDevice				device,
										vk::Allocator&					allocator,
										const vk::VkBufferCreateInfo&	bufferCreateInfo,
										const vk::MemoryRequirement		memoryRequirement);

	const vk::VkBuffer&	get				(void) const { return *m_buffer; }
	const vk::VkBuffer&	operator*		(void) const { return get(); }
	vk::Allocation&		getAllocation	(void) const { return *m_allocation; }

	void				freeAllocation	(void) { delete m_allocation.release(); }

private:
	de::MovePtr<vk::Allocation>		m_allocation;
	vk::Move<vk::VkBuffer>			m_buffer;

	Buffer(const Buffer&);          // "deleted"
	Buffer&							operator=		(const Buffer&);
};

Buffer::Buffer(const DeviceInterface&		vk,
			   const VkDevice				device,
			   Allocator&					allocator,
			   const VkBufferCreateInfo&	bufferCreateInfo,
			   const MemoryRequirement		memoryRequirement)
{
	m_buffer = createBuffer(vk, device, &bufferCreateInfo);
	m_allocation = allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement);
	VK_CHECK(vk.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
}


class UseAfterFreeTestCase : public vkt::TestCase
{
public:
	void								initPrograms			(vk::SourceCollections&	sourceCollections) const;
	TestInstance*						createInstance			(Context&			context) const;

	static UseAfterFreeTestCase*		UBOToSSBOInvertCase		(tcu::TestContext&	testCtx,
																const std::string&  name,
																const std::string&  description,
																const deUint32		numValues,
																const tcu::IVec3&	localSize,
																const tcu::IVec3&	workSize);

	static UseAfterFreeTestCase*		CopyInvertSSBOCase		(tcu::TestContext&	testCtx,
																const std::string&	name,
																const std::string&	description,
																const deUint32		numValues,
																const tcu::IVec3&	localSize,
																const tcu::IVec3&	workSize);

private:
										UseAfterFreeTestCase	(tcu::TestContext&	testCtx,
																const std::string& name,
																const std::string& description,
																const deUint32		numValues,
																const tcu::IVec3&	localSize,
																const tcu::IVec3&	workSize,
																const BufferType	bufferType);

	const BufferType					m_bufferType;
	const deUint32                      m_numValues;
	const tcu::IVec3					m_localSize;
	const tcu::IVec3					m_workSize;
};

class UseAfterFreeTestInstance : public PostmortemTestInstance
{
public:
	UseAfterFreeTestInstance(Context&			context,
							 const deUint32		numValues,
							 const tcu::IVec3&	localSize,
							 const tcu::IVec3&	workSize,
							 const BufferType	bufferType);

	tcu::TestStatus					iterate						(void);

private:
	const BufferType				m_bufferType;
	const deUint32					m_numValues;
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
};

template<typename T, int size>
T multiplyComponents(const tcu::Vector<T, size>& v)
{
	T accum = 1;
	for (int i = 0; i < size; ++i)
		accum *= v[i];
	return accum;
}

UseAfterFreeTestCase::UseAfterFreeTestCase (tcu::TestContext&	testCtx,
											const std::string&	name,
											const std::string&	description,
											const deUint32		numValues,
											const tcu::IVec3&	localSize,
											const tcu::IVec3&	workSize,
											const BufferType		bufferType)
	: TestCase		(testCtx, name, description)
	, m_bufferType	(bufferType)
	, m_numValues	(numValues)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
	DE_ASSERT(m_numValues % (multiplyComponents(m_workSize) * multiplyComponents(m_localSize)) == 0);
	DE_ASSERT(m_bufferType == BUFFER_TYPE_UNIFORM || m_bufferType == BUFFER_TYPE_SSBO);
}

UseAfterFreeTestCase* UseAfterFreeTestCase::UBOToSSBOInvertCase (tcu::TestContext&	testCtx,
																const std::string&	name,
																const std::string&	description,
																const deUint32		numValues,
																const tcu::IVec3&	localSize,
																const tcu::IVec3&	workSize)
{
	return new UseAfterFreeTestCase(testCtx, name, description, numValues, localSize, workSize, BUFFER_TYPE_UNIFORM);
}

UseAfterFreeTestCase* UseAfterFreeTestCase::CopyInvertSSBOCase (tcu::TestContext&	testCtx,
																const std::string&	name,
																const std::string&	description,
																const deUint32		numValues,
																const tcu::IVec3&	localSize,
																const tcu::IVec3&	workSize)
{
	return new UseAfterFreeTestCase(testCtx, name, description, numValues, localSize, workSize, BUFFER_TYPE_SSBO);
}

void UseAfterFreeTestCase::initPrograms	(SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	if (m_bufferType == BUFFER_TYPE_UNIFORM)
	{
		src << "#version 310 es\n"
			<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
			<< "layout(binding = 0) readonly uniform Input {\n"
			<< "    uint values[" << m_numValues << "];\n"
			<< "} ub_in;\n"
			<< "layout(binding = 1, std140) writeonly buffer Output {\n"
			<< "    uint values[" << m_numValues << "];\n"
			<< "} sb_out;\n"
			<< "void main (void) {\n"
			<< "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "    uint numValuesPerInv = uint(ub_in.values.length()) / (size.x*size.y*size.z);\n"
			<< "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
			<< "    uint offset          = numValuesPerInv*groupNdx;\n"
			<< "\n"
			<< "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
			<< "        sb_out.values[offset + ndx] = ~ub_in.values[offset + ndx];\n"
			<< "}\n";
	}
	else if (m_bufferType == BUFFER_TYPE_SSBO)
	{
		src << "#version 310 es\n"
			<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
			<< "layout(binding = 0, std140) readonly buffer Input {\n"
			<< "    uint values[" << m_numValues << "];\n"
			<< "} sb_in;\n"
			<< "layout (binding = 1, std140) writeonly buffer Output {\n"
			<< "    uint values[" << m_numValues << "];\n"
			<< "} sb_out;\n"
			<< "void main (void) {\n"
			<< "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "    uint numValuesPerInv = uint(sb_in.values.length()) / (size.x*size.y*size.z);\n"
			<< "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
			<< "    uint offset          = numValuesPerInv*groupNdx;\n"
			<< "\n"
			<< "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
			<< "        sb_out.values[offset + ndx] = ~sb_in.values[offset + ndx];\n"
			<< "}\n";
	}

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* UseAfterFreeTestCase::createInstance(Context& context) const
{
	return new UseAfterFreeTestInstance(context, m_numValues, m_localSize, m_workSize, m_bufferType);
}

UseAfterFreeTestInstance::UseAfterFreeTestInstance (Context&			context,
													const deUint32		numValues,
													const tcu::IVec3&	localSize,
													const tcu::IVec3&	workSize,
													const BufferType	bufferType)
													: PostmortemTestInstance(context)
													, m_bufferType(bufferType)
													, m_numValues(numValues)
													, m_localSize(localSize)
													, m_workSize(workSize)
{

}

tcu::TestStatus UseAfterFreeTestInstance::iterate(void)
{
	const VkDevice			device				= *m_logicalDevice;
	const DeviceInterface&	vk					= m_deviceDriver;
	const VkQueue			queue				= m_queue;
	const deUint32			queueFamilyIndex	= m_queueFamilyIndex;
	Allocator&				allocator			= m_allocator;

	// Customize the test based on buffer type

	const VkBufferUsageFlags inputBufferUsageFlags = (m_bufferType == BUFFER_TYPE_UNIFORM ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	const VkDescriptorType inputBufferDescriptorType = (m_bufferType == BUFFER_TYPE_UNIFORM ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const deUint32 randomSeed = (m_bufferType == BUFFER_TYPE_UNIFORM ? 0x111223f : 0x124fef);

	// Create an input buffer

	const VkDeviceSize bufferSizeBytes = sizeof(tcu::UVec4) * m_numValues;
	Buffer inputBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, inputBufferUsageFlags), MemoryRequirement::HostVisible);

	// Fill the input buffer with data
	{
		de::Random rnd(randomSeed);
		const Allocation& inputBufferAllocation = inputBuffer.getAllocation();
		tcu::UVec4* bufferPtr = static_cast<tcu::UVec4*>(inputBufferAllocation.getHostPtr());
		for (deUint32 i = 0; i < m_numValues; ++i)
			bufferPtr[i].x() = rnd.getUint32();

		flushAlloc(vk, device, inputBufferAllocation);
	}

	// Create an output buffer

	Buffer outputBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(inputBufferDescriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(inputBufferDescriptorType)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo inputBufferDescriptorInfo = makeDescriptorBufferInfo(*inputBuffer, 0ull, bufferSizeBytes);
	const VkDescriptorBufferInfo outputBufferDescriptorInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), inputBufferDescriptorType, &inputBufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier hostWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull, bufferSizeBytes);

	const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, bufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Free the memory backing the buffer
	inputBuffer.freeAllocation();
	outputBuffer.freeAllocation();

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Pointers are invalid, so nothing to verify
	return tcu::TestStatus::pass("Test succeeded without device loss");
}
}

tcu::TestCaseGroup* createUseAfterFreeTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> useAfterFreeGroup(new tcu::TestCaseGroup(testCtx, "use_after_free", "Use buffer after free."));

	useAfterFreeGroup->addChild(UseAfterFreeTestCase::UBOToSSBOInvertCase(testCtx, "ubo_to_ssbo_single_invocation", "Copy from UBO to SSBO, inverting bits", 256, tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1)));
	useAfterFreeGroup->addChild(UseAfterFreeTestCase::CopyInvertSSBOCase (testCtx, "ssbo_to_ssbo_single_invocation", "Copy from SSBO to SSBO, inverting bits", 256, tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1)));

	return useAfterFreeGroup.release();
}

} // postmortem
} // vkt
