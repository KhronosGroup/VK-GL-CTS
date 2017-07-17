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
#include "deSTLUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
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
Move<VkBuffer> createBufferAndBindMemory (const DeviceInterface& vkdi, const VkDevice& device, VkDescriptorType dtype, Allocator& allocator, size_t numBytes, AllocationMp* outMemory)
{
	VkBufferUsageFlags			usageBit		= (VkBufferUsageFlags)0;

	switch (dtype)
	{
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:	usageBit = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:	usageBit = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
		default:								DE_ASSERT(false);
	}

	const VkBufferCreateInfo bufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		DE_NULL,								// pNext
		0u,										// flags
		numBytes,								// size
		usageBit,								// usage
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
 * \brief Create a descriptor set layout with the given descriptor types
 *
 * All descriptors are created for compute pipeline.
 *//*--------------------------------------------------------------------*/
Move<VkDescriptorSetLayout> createDescriptorSetLayout (const DeviceInterface& vkdi, const VkDevice& device, const vector<VkDescriptorType>& dtypes)
{
	DescriptorSetLayoutBuilder builder;

	for (size_t bindingNdx = 0; bindingNdx < dtypes.size(); ++bindingNdx)
		builder.addSingleBinding(dtypes[bindingNdx], VK_SHADER_STAGE_COMPUTE_BIT);

	return builder.build(vkdi, device);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a pipeline layout with one descriptor set
 *//*--------------------------------------------------------------------*/
Move<VkPipelineLayout> createPipelineLayout (const DeviceInterface& vkdi, const VkDevice& device, VkDescriptorSetLayout descriptorSetLayout, const vkt::SpirVAssembly::BufferSp& pushConstants)
{
	VkPipelineLayoutCreateInfo		createInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// sType
		DE_NULL,										// pNext
		(VkPipelineLayoutCreateFlags)0,
		1u,												// descriptorSetCount
		&descriptorSetLayout,							// pSetLayouts
		0u,												// pushConstantRangeCount
		DE_NULL,										// pPushConstantRanges
	};

	VkPushConstantRange				range		=
	{
		VK_SHADER_STAGE_COMPUTE_BIT,					// stageFlags
		0,												// offset
		0,												// size
	};

	if (pushConstants != DE_NULL)
	{
		range.size							= static_cast<deUint32>(pushConstants->getNumBytes());
		createInfo.pushConstantRangeCount	= 1;
		createInfo.pPushConstantRanges		= &range;
	}

	return createPipelineLayout(vkdi, device, &createInfo);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a one-time descriptor pool for one descriptor set that
 * support the given descriptor types.
 *//*--------------------------------------------------------------------*/
inline Move<VkDescriptorPool> createDescriptorPool (const DeviceInterface& vkdi, const VkDevice& device, const vector<VkDescriptorType>& dtypes)
{
	DescriptorPoolBuilder builder;

	for (size_t typeNdx = 0; typeNdx < dtypes.size(); ++typeNdx)
		builder.addType(dtypes[typeNdx], 1);

	return builder.build(vkdi, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, /* maxSets = */ 1);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a descriptor set
 *
 * The descriptor set's layout contains the given descriptor types,
 * sequentially binded to binding points starting from 0.
 *//*--------------------------------------------------------------------*/
Move<VkDescriptorSet> createDescriptorSet (const DeviceInterface& vkdi, const VkDevice& device, VkDescriptorPool pool, VkDescriptorSetLayout layout, const vector<VkDescriptorType>& dtypes, const vector<VkDescriptorBufferInfo>& descriptorInfos)
{
	DE_ASSERT(dtypes.size() == descriptorInfos.size());

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

	for (deUint32 descriptorNdx = 0; descriptorNdx < dtypes.size(); ++descriptorNdx)
		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descriptorNdx), dtypes[descriptorNdx], &descriptorInfos[descriptorNdx]);
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
	return createCommandPool(vkdi, device, 0u, queueFamilyIndex);
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
										SpvAsmComputeShaderInstance	(Context& ctx, const ComputeShaderSpec& spec, const ComputeTestFeatures features);
	tcu::TestStatus						iterate						(void);

private:
	const ComputeShaderSpec&			m_shaderSpec;
	const ComputeTestFeatures			m_features;
};

// ComputeShaderTestCase implementations

SpvAsmComputeShaderCase::SpvAsmComputeShaderCase (tcu::TestContext& testCtx, const char* name, const char* description, const ComputeShaderSpec& spec, const ComputeTestFeatures features)
	: TestCase		(testCtx, name, description)
	, m_shaderSpec	(spec)
	, m_features	(features)
{
}

void SpvAsmComputeShaderCase::initPrograms (SourceCollections& programCollection) const
{
	programCollection.spirvAsmSources.add("compute") << m_shaderSpec.assembly.c_str();
}

TestInstance* SpvAsmComputeShaderCase::createInstance (Context& ctx) const
{
	return new SpvAsmComputeShaderInstance(ctx, m_shaderSpec, m_features);
}

// ComputeShaderTestInstance implementations

SpvAsmComputeShaderInstance::SpvAsmComputeShaderInstance (Context& ctx, const ComputeShaderSpec& spec, const ComputeTestFeatures features)
	: TestInstance		(ctx)
	, m_shaderSpec		(spec)
	, m_features		(features)
{
}

tcu::TestStatus SpvAsmComputeShaderInstance::iterate (void)
{
	const VkPhysicalDeviceFeatures&		features			= m_context.getDeviceFeatures();

	if ((m_features == COMPUTE_TEST_USES_INT16 || m_features == COMPUTE_TEST_USES_INT16_INT64) && !features.shaderInt16)
	{
		TCU_THROW(NotSupportedError, "shaderInt16 feature is not supported");
	}

	if ((m_features == COMPUTE_TEST_USES_INT64 || m_features == COMPUTE_TEST_USES_INT16_INT64) && !features.shaderInt64)
	{
		TCU_THROW(NotSupportedError, "shaderInt64 feature is not supported");
	}

	{
		const InstanceInterface&			vki					= m_context.getInstanceInterface();
		const VkPhysicalDevice				physicalDevice		= m_context.getPhysicalDevice();

		// 16bit storage features
		{
			if (!is16BitStorageFeaturesSupported(vki, physicalDevice, m_context.getInstanceExtensions(), m_shaderSpec.requestedVulkanFeatures.ext16BitStorage))
				TCU_THROW(NotSupportedError, "Requested 16bit storage features not supported");
		}

		// VariablePointers features
		{
			if (!isVariablePointersFeaturesSupported(vki, physicalDevice, m_context.getInstanceExtensions(), m_shaderSpec.requestedVulkanFeatures.extVariablePointers))
				TCU_THROW(NotSupportedError, "Request Variable Pointer feature not supported");
		}
	}

	// defer device and resource creation until after feature checks
	const Unique<VkDevice>				vkDevice			(createDeviceWithExtensions(m_context, m_context.getUniversalQueueFamilyIndex(), m_context.getDeviceExtensions(), m_shaderSpec.extensions));
	const VkDevice&						device				= *vkDevice;
	const DeviceDriver					vkDeviceInterface	(m_context.getInstanceInterface(), device);
	const DeviceInterface&				vkdi				= vkDeviceInterface;
	const de::UniquePtr<vk::Allocator>	vkAllocator			(createAllocator(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), vkDeviceInterface, device));
	Allocator&							allocator			= *vkAllocator;
	const VkQueue						queue				(getDeviceQueue(vkDeviceInterface, device, m_context.getUniversalQueueFamilyIndex(), 0));

	vector<AllocationSp>				inputAllocs;
	vector<AllocationSp>				outputAllocs;
	vector<BufferHandleSp>				inputBuffers;
	vector<BufferHandleSp>				outputBuffers;
	vector<VkDescriptorBufferInfo>		descriptorInfos;
	vector<VkDescriptorType>			descriptorTypes;

	DE_ASSERT(!m_shaderSpec.outputs.empty());

	// Create buffer object, allocate storage, and create view for all input/output buffers.

	for (deUint32 inputNdx = 0; inputNdx < m_shaderSpec.inputs.size(); ++inputNdx)
	{
		if (m_shaderSpec.inputTypes.count(inputNdx) != 0)
			descriptorTypes.push_back(m_shaderSpec.inputTypes.at(inputNdx));
		else
			descriptorTypes.push_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		AllocationMp		alloc;
		const BufferSp&		input		= m_shaderSpec.inputs[inputNdx];
		const size_t		numBytes	= input->getNumBytes();
		BufferHandleUp*		buffer		= new BufferHandleUp(createBufferAndBindMemory(vkdi, device, descriptorTypes.back(), allocator, numBytes, &alloc));

		setMemory(vkdi, device, &*alloc, numBytes, input->data());
		descriptorInfos.push_back(vk::makeDescriptorBufferInfo(**buffer, 0u, numBytes));
		inputBuffers.push_back(BufferHandleSp(buffer));
		inputAllocs.push_back(de::SharedPtr<Allocation>(alloc.release()));
	}

	for (deUint32 outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
	{
		descriptorTypes.push_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		AllocationMp		alloc;
		const BufferSp&		output		= m_shaderSpec.outputs[outputNdx];
		const size_t		numBytes	= output->getNumBytes();
		BufferHandleUp*		buffer		= new BufferHandleUp(createBufferAndBindMemory(vkdi, device, descriptorTypes.back(), allocator, numBytes, &alloc));

		fillMemoryWithValue(vkdi, device, &*alloc, numBytes, 0xff);
		descriptorInfos.push_back(vk::makeDescriptorBufferInfo(**buffer, 0u, numBytes));
		outputBuffers.push_back(BufferHandleSp(buffer));
		outputAllocs.push_back(de::SharedPtr<Allocation>(alloc.release()));
	}

	// Create layouts and descriptor set.

	Unique<VkDescriptorSetLayout>		descriptorSetLayout	(createDescriptorSetLayout(vkdi, device, descriptorTypes));
	Unique<VkPipelineLayout>			pipelineLayout		(createPipelineLayout(vkdi, device, *descriptorSetLayout, m_shaderSpec.pushConstants));
	Unique<VkDescriptorPool>			descriptorPool		(createDescriptorPool(vkdi, device, descriptorTypes));
	Unique<VkDescriptorSet>				descriptorSet		(createDescriptorSet(vkdi, device, *descriptorPool, *descriptorSetLayout, descriptorTypes, descriptorInfos));

	// Create compute shader and pipeline.

	const ProgramBinary&				binary				= m_context.getBinaryCollection().get("compute");
	Unique<VkShaderModule>				module				(createShaderModule(vkdi, device, binary, (VkShaderModuleCreateFlags)0u));

	Unique<VkPipeline>					computePipeline		(createComputePipeline(vkdi, device, *pipelineLayout, *module, m_shaderSpec.entryPoint.c_str(), m_shaderSpec.specConstants));

	// Create command buffer and record commands

	const Unique<VkCommandPool>			cmdPool				(createCommandPool(vkdi, device, m_context.getUniversalQueueFamilyIndex()));
	Unique<VkCommandBuffer>				cmdBuffer			(allocateCommandBuffer(vkdi, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

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
	if (m_shaderSpec.pushConstants != DE_NULL)
	{
		const deUint32	size	= static_cast<deUint32>(m_shaderSpec.pushConstants->getNumBytes());
		const void*		data	= m_shaderSpec.pushConstants->data();

		vkdi.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, /* offset = */ 0, /* size = */ size, data);
	}
	vkdi.cmdDispatch(*cmdBuffer, numWorkGroups.x(), numWorkGroups.y(), numWorkGroups.z());
	VK_CHECK(vkdi.endCommandBuffer(*cmdBuffer));

	// Create fence and run.

	const Unique<VkFence>			cmdCompleteFence	(createFence(vkdi, device));
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

	VK_CHECK(vkdi.queueSubmit(queue, 1, &submitInfo, *cmdCompleteFence));
	VK_CHECK(vkdi.waitForFences(device, 1, &cmdCompleteFence.get(), 0u, infiniteTimeout)); // \note: timeout is failure

	// Check output.
	if (m_shaderSpec.verifyIO)
	{
		if (!(*m_shaderSpec.verifyIO)(m_shaderSpec.inputs, outputAllocs, m_shaderSpec.outputs, m_context.getTestContext().getLog()))
			return tcu::TestStatus(m_shaderSpec.failResult, m_shaderSpec.failMessage);
	}
	else
	{
		for (size_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
		{
			const BufferSp& expectedOutput = m_shaderSpec.outputs[outputNdx];
			if (deMemCmp(expectedOutput->data(), outputAllocs[outputNdx]->getHostPtr(), expectedOutput->getNumBytes()))
				return tcu::TestStatus(m_shaderSpec.failResult, m_shaderSpec.failMessage);
		}
	}

	return tcu::TestStatus::pass("Output match with expected");
}

} // SpirVAssembly
} // vkt
