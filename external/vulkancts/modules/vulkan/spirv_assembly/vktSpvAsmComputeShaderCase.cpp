/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Test Case Skeleton Based on Compute Shaders
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmComputeShaderCase.hpp"

#include "deSharedPtr.hpp"

#include "vkBuilderUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

namespace
{

using namespace vk;
using std::vector;

typedef vkt::SpirVAssembly::AllocationMp			AllocationMp;
typedef vkt::SpirVAssembly::AllocationSp			AllocationSp;

typedef Unique<VkBuffer>							BufferHandleUp;
typedef de::SharedPtr<BufferHandleUp>				BufferHandleSp;

/*--------------------------------------------------------------------*//*!
 * \brief Create storage buffer, allocate and bind memory for the buffer
 *
 * The memory is created as host visible and passed back as a vk::Allocation
 * instance via outMemory.
 *//*--------------------------------------------------------------------*/
Move<VkBuffer> createBufferAndBindMemory (const DeviceInterface& vkdi, const VkDevice& device, Allocator& allocator, size_t numBytes, AllocationMp* outMemory)
{
	const VkBufferCreateInfo bufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		DE_NULL,								// pNext
		0u,										// flags
		numBytes,								// size
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		0u,										// queueFamilyCount
		DE_NULL,								// pQueueFamilyIndices
	};

	Move<VkBuffer>				buffer			(createBuffer(vkdi, device, &bufferCreateInfo));
	const VkMemoryRequirements	requirements	= getBufferMemoryRequirements(vkdi, device, *buffer);
	AllocationMp				bufferMemory	= allocator.allocate(requirements, MemoryRequirement::HostVisible);

	VK_CHECK(vkdi.bindBufferMemory(device, *buffer, bufferMemory->getMemory(), bufferMemory->getOffset()));
	*outMemory = bufferMemory;

	return buffer;
}

void setMemory (const DeviceInterface& vkdi, const VkDevice& device, Allocation* destAlloc, size_t numBytes, const void* data)
{
	void* const hostPtr = destAlloc->getHostPtr();

	deMemcpy((deUint8*)hostPtr, data, numBytes);
	flushMappedMemoryRange(vkdi, device, destAlloc->getMemory(), destAlloc->getOffset(), numBytes);
}

void fillMemoryWithValue (const DeviceInterface& vkdi, const VkDevice& device, Allocation* destAlloc, size_t numBytes, deUint8 value)
{
	void* const hostPtr = destAlloc->getHostPtr();

	deMemset((deUint8*)hostPtr, value, numBytes);
	flushMappedMemoryRange(vkdi, device, destAlloc->getMemory(), destAlloc->getOffset(), numBytes);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a descriptor set layout with numBindings descriptors
 *
 * All descriptors are created for shader storage buffer objects and
 * compute pipeline.
 *//*--------------------------------------------------------------------*/
Move<VkDescriptorSetLayout> createDescriptorSetLayout (const DeviceInterface& vkdi, const VkDevice& device, size_t numBindings)
{
	DescriptorSetLayoutBuilder builder;

	for (size_t bindingNdx = 0; bindingNdx < numBindings; ++bindingNdx)
		builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

	return builder.build(vkdi, device);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a pipeline layout with one descriptor set
 *//*--------------------------------------------------------------------*/
Move<VkPipelineLayout> createPipelineLayout (const DeviceInterface& vkdi, const VkDevice& device, VkDescriptorSetLayout descriptorSetLayout)
{
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// sType
		DE_NULL,										// pNext
		(VkPipelineLayoutCreateFlags)0,
		1u,												// descriptorSetCount
		&descriptorSetLayout,							// pSetLayouts
		0u,												// pushConstantRangeCount
		DE_NULL,										// pPushConstantRanges
	};

	return createPipelineLayout(vkdi, device, &pipelineLayoutCreateInfo);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a one-time descriptor pool for one descriptor set
 *
 * The pool supports numDescriptors storage buffer descriptors.
 *//*--------------------------------------------------------------------*/
inline Move<VkDescriptorPool> createDescriptorPool (const DeviceInterface& vkdi, const VkDevice& device, deUint32 numDescriptors)
{
	return DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numDescriptors)
		.build(vkdi, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, /* maxSets = */ 1);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a descriptor set
 *
 * The descriptor set's layout should contain numViews descriptors.
 * All the descriptors represent buffer views, and they are sequentially
 * binded to binding point starting from 0.
 *//*--------------------------------------------------------------------*/
Move<VkDescriptorSet> createDescriptorSet (const DeviceInterface& vkdi, const VkDevice& device, VkDescriptorPool pool, VkDescriptorSetLayout layout, size_t numViews, const vector<VkDescriptorBufferInfo>& descriptorInfos)
{
	const VkDescriptorSetAllocateInfo	allocInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		pool,
		1u,
		&layout
	};

	Move<VkDescriptorSet>				descriptorSet	= allocateDescriptorSet(vkdi, device, &allocInfo);
	DescriptorSetUpdateBuilder			builder;

	for (deUint32 descriptorNdx = 0; descriptorNdx < numViews; ++descriptorNdx)
		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descriptorNdx), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfos[descriptorNdx]);
	builder.update(vkdi, device);

	return descriptorSet;
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a compute pipeline based on the given shader
 *//*--------------------------------------------------------------------*/
Move<VkPipeline> createComputePipeline (const DeviceInterface& vkdi, const VkDevice& device, VkPipelineLayout pipelineLayout, VkShaderModule shader, const char* entryPoint, const vector<deUint32>& specConstants)
{
	const deUint32							numSpecConstants				= (deUint32)specConstants.size();
	vector<VkSpecializationMapEntry>		entries;
	VkSpecializationInfo					specInfo;

	if (numSpecConstants != 0)
	{
		entries.resize(numSpecConstants);

		for (deUint32 ndx = 0; ndx < numSpecConstants; ++ndx)
		{
			entries[ndx].constantID	= ndx;
			entries[ndx].offset		= ndx * (deUint32)sizeof(deUint32);
			entries[ndx].size		= sizeof(deUint32);
		}

		specInfo.mapEntryCount		= numSpecConstants;
		specInfo.pMapEntries		= &entries[0];
		specInfo.dataSize			= numSpecConstants * sizeof(deUint32);
		specInfo.pData				= specConstants.data();
	}

	const VkPipelineShaderStageCreateInfo	pipelineShaderStageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// sType
		DE_NULL,												// pNext
		(VkPipelineShaderStageCreateFlags)0,					// flags
		VK_SHADER_STAGE_COMPUTE_BIT,							// stage
		shader,													// module
		entryPoint,												// pName
		(numSpecConstants == 0) ? DE_NULL : &specInfo,			// pSpecializationInfo
	};
	const VkComputePipelineCreateInfo		pipelineCreateInfo				=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		(VkPipelineCreateFlags)0,
		pipelineShaderStageCreateInfo,							// cs
		pipelineLayout,											// layout
		(VkPipeline)0,											// basePipelineHandle
		0u,														// basePipelineIndex
	};

	return createComputePipeline(vkdi, device, (VkPipelineCache)0u, &pipelineCreateInfo);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a command pool
 *
 * The created command pool is designated for use on the queue type
 * represented by the given queueFamilyIndex.
 *//*--------------------------------------------------------------------*/
Move<VkCommandPool> createCommandPool (const DeviceInterface& vkdi, VkDevice device, deUint32 queueFamilyIndex)
{
	const VkCommandPoolCreateInfo cmdPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// sType
		DE_NULL,									// pNext
		0u,											// flags
		queueFamilyIndex,							// queueFamilyIndex
	};

	return createCommandPool(vkdi, device, &cmdPoolCreateInfo);
}

} // anonymous

namespace vkt
{
namespace SpirVAssembly
{

/*--------------------------------------------------------------------*//*!
 * \brief Test instance for compute pipeline
 *
 * The compute shader is specified in the format of SPIR-V assembly, which
 * is allowed to access MAX_NUM_INPUT_BUFFERS input storage buffers and
 * MAX_NUM_OUTPUT_BUFFERS output storage buffers maximally. The shader
 * source and input/output data are given in a ComputeShaderSpec object.
 *
 * This instance runs the given compute shader by feeding the data from input
 * buffers and compares the data in the output buffers with the expected.
 *//*--------------------------------------------------------------------*/
class SpvAsmComputeShaderInstance : public TestInstance
{
public:
								SpvAsmComputeShaderInstance	(Context& ctx, const ComputeShaderSpec& spec);
	tcu::TestStatus				iterate						(void);

private:
	const ComputeShaderSpec&	m_shaderSpec;
};

// ComputeShaderTestCase implementations

SpvAsmComputeShaderCase::SpvAsmComputeShaderCase (tcu::TestContext& testCtx, const char* name, const char* description, const ComputeShaderSpec& spec)
	: TestCase		(testCtx, name, description)
	, m_shaderSpec	(spec)
{
}

void SpvAsmComputeShaderCase::initPrograms (SourceCollections& programCollection) const
{
	programCollection.spirvAsmSources.add("compute") << m_shaderSpec.assembly.c_str();
}

TestInstance* SpvAsmComputeShaderCase::createInstance (Context& ctx) const
{
	return new SpvAsmComputeShaderInstance(ctx, m_shaderSpec);
}

// ComputeShaderTestInstance implementations

SpvAsmComputeShaderInstance::SpvAsmComputeShaderInstance (Context& ctx, const ComputeShaderSpec& spec)
	: TestInstance	(ctx)
	, m_shaderSpec	(spec)
{
}

tcu::TestStatus SpvAsmComputeShaderInstance::iterate (void)
{
	const DeviceInterface&				vkdi				= m_context.getDeviceInterface();
	const VkDevice&						device				= m_context.getDevice();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	vector<AllocationSp>				inputAllocs;
	vector<AllocationSp>				outputAllocs;
	vector<BufferHandleSp>				inputBuffers;
	vector<BufferHandleSp>				outputBuffers;
	vector<VkDescriptorBufferInfo>		descriptorInfos;

	DE_ASSERT(!m_shaderSpec.outputs.empty());
	const size_t						numBuffers			= m_shaderSpec.inputs.size() + m_shaderSpec.outputs.size();

	// Create buffer object, allocate storage, and create view for all input/output buffers.

	for (size_t inputNdx = 0; inputNdx < m_shaderSpec.inputs.size(); ++inputNdx)
	{
		AllocationMp		alloc;
		const BufferSp&		input		= m_shaderSpec.inputs[inputNdx];
		const size_t		numBytes	= input->getNumBytes();
		BufferHandleUp*		buffer		= new BufferHandleUp(createBufferAndBindMemory(vkdi, device, allocator, numBytes, &alloc));

		setMemory(vkdi, device, &*alloc, numBytes, input->data());
		descriptorInfos.push_back(vk::makeDescriptorBufferInfo(**buffer, 0u, numBytes));
		inputBuffers.push_back(BufferHandleSp(buffer));
		inputAllocs.push_back(de::SharedPtr<Allocation>(alloc.release()));
	}

	for (size_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
	{
		AllocationMp		alloc;
		const BufferSp&		output		= m_shaderSpec.outputs[outputNdx];
		const size_t		numBytes	= output->getNumBytes();
		BufferHandleUp*		buffer		= new BufferHandleUp(createBufferAndBindMemory(vkdi, device, allocator, numBytes, &alloc));

		fillMemoryWithValue(vkdi, device, &*alloc, numBytes, 0xff);
		descriptorInfos.push_back(vk::makeDescriptorBufferInfo(**buffer, 0u, numBytes));
		outputBuffers.push_back(BufferHandleSp(buffer));
		outputAllocs.push_back(de::SharedPtr<Allocation>(alloc.release()));
	}

	// Create layouts and descriptor set.

	Unique<VkDescriptorSetLayout>		descriptorSetLayout	(createDescriptorSetLayout(vkdi, device, numBuffers));
	Unique<VkPipelineLayout>			pipelineLayout		(createPipelineLayout(vkdi, device, *descriptorSetLayout));
	Unique<VkDescriptorPool>			descriptorPool		(createDescriptorPool(vkdi, device, (deUint32)numBuffers));
	Unique<VkDescriptorSet>				descriptorSet		(createDescriptorSet(vkdi, device, *descriptorPool, *descriptorSetLayout, numBuffers, descriptorInfos));

	// Create compute shader and pipeline.

	const ProgramBinary&				binary				= m_context.getBinaryCollection().get("compute");
	Unique<VkShaderModule>				module				(createShaderModule(vkdi, device, binary, (VkShaderModuleCreateFlags)0u));

	Unique<VkPipeline>					computePipeline		(createComputePipeline(vkdi, device, *pipelineLayout, *module, m_shaderSpec.entryPoint.c_str(), m_shaderSpec.specConstants));

	// Create command buffer and record commands

	const Unique<VkCommandPool>			cmdPool				(createCommandPool(vkdi, device, m_context.getUniversalQueueFamilyIndex()));
	const VkCommandBufferAllocateInfo	cmdBufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// sType
		NULL,											// pNext
		*cmdPool,										// cmdPool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// level
		1u												// count
	};

	Unique<VkCommandBuffer>				cmdBuffer			(allocateCommandBuffer(vkdi, device, &cmdBufferCreateInfo));

	const VkCommandBufferBeginInfo		cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// sType
		DE_NULL,										// pNext
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const tcu::IVec3&				numWorkGroups		= m_shaderSpec.numWorkGroups;

	VK_CHECK(vkdi.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vkdi.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
	vkdi.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);
	vkdi.cmdDispatch(*cmdBuffer, numWorkGroups.x(), numWorkGroups.y(), numWorkGroups.z());
	VK_CHECK(vkdi.endCommandBuffer(*cmdBuffer));

	// Create fence and run.

	const VkFenceCreateInfo			fenceCreateInfo		=
	{
		 VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,		// sType
		 NULL,										// pNext
		 0											// flags
    };
	const Unique<VkFence>			cmdCompleteFence	(createFence(vkdi, device, &fenceCreateInfo));
	const deUint64					infiniteTimeout		= ~(deUint64)0u;
	const VkSubmitInfo				submitInfo			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		DE_NULL,
		0u,
		(const VkSemaphore*)DE_NULL,
		(const VkPipelineStageFlags*)DE_NULL,
		1u,
		&cmdBuffer.get(),
		0u,
		(const VkSemaphore*)DE_NULL,
	};

	VK_CHECK(vkdi.queueSubmit(m_context.getUniversalQueue(), 1, &submitInfo, *cmdCompleteFence));
	VK_CHECK(vkdi.waitForFences(device, 1, &cmdCompleteFence.get(), 0u, infiniteTimeout)); // \note: timeout is failure

	// Check output.
	if (m_shaderSpec.verifyIO)
	{
		if (!(*m_shaderSpec.verifyIO)(m_shaderSpec.inputs, outputAllocs, m_shaderSpec.outputs))
			return tcu::TestStatus::fail("Output doesn't match with expected");
	}
	else
	{
		for (size_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
		{
			const BufferSp& expectedOutput = m_shaderSpec.outputs[outputNdx];
			if (deMemCmp(expectedOutput->data(), outputAllocs[outputNdx]->getHostPtr(), expectedOutput->getNumBytes()))
				return tcu::TestStatus::fail("Output doesn't match with expected");
		}
	}

	return tcu::TestStatus::pass("Ouput match with expected");
}

} // SpirVAssembly
} // vkt
