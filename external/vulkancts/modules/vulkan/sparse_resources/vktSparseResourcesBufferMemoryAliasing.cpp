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
 * \file  vktSparseResourcesBufferMemoryAliasing.cpp
 * \brief Sparse buffer memory aliasing tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferMemoryAliasing.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

enum ShaderParameters
{
	SIZE_OF_UINT_IN_SHADER	= 4u,
	MODULO_DIVISOR			= 1024u
};

tcu::UVec3 computeWorkGroupSize (const deUint32 numInvocations)
{
	const deUint32		maxComputeWorkGroupInvocations	= 128u;
	const tcu::UVec3	maxComputeWorkGroupSize			= tcu::UVec3(128u, 128u, 64u);
	deUint32			numInvocationsLeft				= numInvocations;

	const deUint32 xWorkGroupSize = std::min(std::min(numInvocationsLeft, maxComputeWorkGroupSize.x()), maxComputeWorkGroupInvocations);
	numInvocationsLeft = numInvocationsLeft / xWorkGroupSize + ((numInvocationsLeft % xWorkGroupSize) ? 1u : 0u);

	const deUint32 yWorkGroupSize = std::min(std::min(numInvocationsLeft, maxComputeWorkGroupSize.y()), maxComputeWorkGroupInvocations / xWorkGroupSize);
	numInvocationsLeft = numInvocationsLeft / yWorkGroupSize + ((numInvocationsLeft % yWorkGroupSize) ? 1u : 0u);

	const deUint32 zWorkGroupSize = std::min(std::min(numInvocationsLeft, maxComputeWorkGroupSize.z()), maxComputeWorkGroupInvocations / (xWorkGroupSize*yWorkGroupSize));
	numInvocationsLeft = numInvocationsLeft / zWorkGroupSize + ((numInvocationsLeft % zWorkGroupSize) ? 1u : 0u);

	return tcu::UVec3(xWorkGroupSize, yWorkGroupSize, zWorkGroupSize);
}

class BufferSparseMemoryAliasingCase : public TestCase
{
public:
					BufferSparseMemoryAliasingCase	(tcu::TestContext&		testCtx,
													 const std::string&		name,
													 const std::string&		description,
													 const deUint32			bufferSize,
													 const glu::GLSLVersion	glslVersion);

	void			initPrograms					(SourceCollections&		sourceCollections) const;
	TestInstance*	createInstance					(Context&				context) const;

private:
	const	deUint32			m_bufferSizeInBytes;
	const	glu::GLSLVersion	m_glslVersion;
};

BufferSparseMemoryAliasingCase::BufferSparseMemoryAliasingCase (tcu::TestContext&		testCtx,
																const std::string&		name,
																const std::string&		description,
																const deUint32			bufferSize,
																const glu::GLSLVersion	glslVersion)
	: TestCase				(testCtx, name, description)
	, m_bufferSizeInBytes	(bufferSize)
	, m_glslVersion			(glslVersion)
{
}

void BufferSparseMemoryAliasingCase::initPrograms (SourceCollections& sourceCollections) const
{
	// Create compute program
	const char* const versionDecl		= glu::getGLSLVersionDeclaration(m_glslVersion);
	const deUint32	  numInvocations	= m_bufferSizeInBytes / SIZE_OF_UINT_IN_SHADER;
	const tcu::UVec3  workGroupSize		= computeWorkGroupSize(numInvocations);

	std::ostringstream src;
	src << versionDecl << "\n"
		<< "layout (local_size_x = " << workGroupSize.x() << ", local_size_y = " << workGroupSize.y() << ", local_size_z = " << workGroupSize.z() << ") in;\n"
		<< "layout(set = 0, binding = 0, std430) writeonly buffer Output\n"
		<< "{\n"
		<< "	uint result[];\n"
		<< "} sb_out;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	uint index = gl_GlobalInvocationID.x + (gl_GlobalInvocationID.y + gl_GlobalInvocationID.z*gl_NumWorkGroups.y*gl_WorkGroupSize.y)*gl_NumWorkGroups.x*gl_WorkGroupSize.x;\n"
		<< "	if ( index < " << m_bufferSizeInBytes / SIZE_OF_UINT_IN_SHADER << "u )\n"
		<< "	{\n"
		<< "		sb_out.result[index] = index % " << MODULO_DIVISOR << "u;\n"
		<< "	}\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

class BufferSparseMemoryAliasingInstance : public SparseResourcesBaseInstance
{
public:
					BufferSparseMemoryAliasingInstance	(Context&					context,
														 const deUint32				bufferSize);

	tcu::TestStatus	iterate								(void);

private:
	const deUint32			m_bufferSizeInBytes;
};

BufferSparseMemoryAliasingInstance::BufferSparseMemoryAliasingInstance (Context&					context,
																		const deUint32			bufferSize)
	: SparseResourcesBaseInstance	(context)
	, m_bufferSizeInBytes			(bufferSize)
{
}

tcu::TestStatus BufferSparseMemoryAliasingInstance::iterate (void)
{
	const InstanceInterface&		instance		= m_context.getInstanceInterface();
	const DeviceInterface&			deviceInterface	= m_context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice	= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures	deviceFeatures	= getPhysicalDeviceFeatures(instance, physicalDevice);

	if (deviceFeatures.sparseBinding == false)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Sparse binding not supported");
	}

	if (deviceFeatures.sparseResidencyAliased == false)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Sparse memory aliasing not supported");
	}

	QueueRequirementsVec queueRequirements;
	queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
	queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

	// Create logical device supporting both sparse and compute oprations
	if (!createDeviceSupportingQueues(queueRequirements))
	{
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Could not create device supporting sparse and compute queue");
	}

	const VkPhysicalDeviceMemoryProperties deviceMemoryProperties = getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	// Create memory allocator for device
	const de::UniquePtr<Allocator> allocator(new SimpleAllocator(deviceInterface, *m_logicalDevice, deviceMemoryProperties));

	// Create queue supporting sparse binding operations
	const Queue& sparseQueue = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);

	// Create queue supporting compute and transfer operations
	const Queue& computeQueue = getQueue(VK_QUEUE_COMPUTE_BIT, 0);

	VkBufferCreateInfo bufferCreateInfo = 
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		VK_BUFFER_CREATE_SPARSE_BINDING_BIT | 
		VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,	// VkBufferCreateFlags	flags;
		m_bufferSizeInBytes,					// VkDeviceSize			size;
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,		// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		0u,										// deUint32				queueFamilyIndexCount;
		DE_NULL									// const deUint32*		pQueueFamilyIndices;
	};

	const deUint32 queueFamilyIndices[] = { sparseQueue.queueFamilyIndex, computeQueue.queueFamilyIndex };

	if (sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex)
	{
		bufferCreateInfo.sharingMode			= VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount	= 2u;
		bufferCreateInfo.pQueueFamilyIndices	= queueFamilyIndices;
	}

	// Create sparse buffers
	const Unique<VkBuffer> sparseBufferWrite(createBuffer(deviceInterface, *m_logicalDevice, &bufferCreateInfo));
	const Unique<VkBuffer> sparseBufferRead	(createBuffer(deviceInterface, *m_logicalDevice, &bufferCreateInfo));

	const VkMemoryRequirements		 bufferMemRequirements = getBufferMemoryRequirements(deviceInterface, *m_logicalDevice, *sparseBufferWrite);
	const VkPhysicalDeviceProperties deviceProperties	   = getPhysicalDeviceProperties(instance, physicalDevice);

	if (bufferMemRequirements.size > deviceProperties.limits.sparseAddressSpaceSize)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Required memory size for sparse resources exceeds device limits");
	}

	DE_ASSERT((bufferMemRequirements.size % bufferMemRequirements.alignment) == 0);
	
	const deUint32 memoryType = findMatchingMemoryType(deviceMemoryProperties, bufferMemRequirements, MemoryRequirement::Any);

	if (memoryType == NO_MATCH_FOUND)
	{
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "No matching memory type found");
	}

	const VkMemoryAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType			sType;
		DE_NULL,								//	const void*				pNext;
		bufferMemRequirements.size,				//	VkDeviceSize			allocationSize;
		memoryType,								//	deUint32				memoryTypeIndex;
	};

	VkDeviceMemory deviceMemory;
	VK_CHECK(deviceInterface.allocateMemory(*m_logicalDevice, &allocInfo, DE_NULL, &deviceMemory));
	
	Move<VkDeviceMemory> deviceMemoryPtr(check<VkDeviceMemory>(deviceMemory), Deleter<VkDeviceMemory>(deviceInterface, *m_logicalDevice, DE_NULL));

	const VkSparseMemoryBind sparseMemoryBind = makeSparseMemoryBind
	(
		0u,							//VkDeviceSize				resourceOffset
		bufferMemRequirements.size,	//VkDeviceSize				size
		deviceMemory,				//VkDeviceMemory			memory
		0u,							//VkDeviceSize				memoryOffset
		0u							//VkSparseMemoryBindFlags	flags
	);

	const VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo[2] = 
	{
		makeSparseBufferMemoryBindInfo
		(*sparseBufferWrite,	//VkBuffer					buffer;
		1u,						//deUint32					bindCount;
		&sparseMemoryBind		//const VkSparseMemoryBind*	Binds;
		),

		makeSparseBufferMemoryBindInfo
		(*sparseBufferRead,		//VkBuffer					buffer;
		1u,						//deUint32					bindCount;
		&sparseMemoryBind		//const VkSparseMemoryBind*	Binds;
		)
	};

	const Unique<VkSemaphore> bufferMemoryBindSemaphore(makeSemaphore(deviceInterface, *m_logicalDevice));

	const VkBindSparseInfo bindSparseInfo =
	{
		VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,			//VkStructureType							sType;
		DE_NULL,									//const void*								pNext;
		0u,											//deUint32									waitSemaphoreCount;
		DE_NULL,									//const VkSemaphore*						pWaitSemaphores;
		2u,											//deUint32									bufferBindCount;
		sparseBufferMemoryBindInfo,					//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
		0u,											//deUint32									imageOpaqueBindCount;
		DE_NULL,									//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
		0u,											//deUint32									imageBindCount;
		DE_NULL,									//const VkSparseImageMemoryBindInfo*		pImageBinds;
		1u,											//deUint32									signalSemaphoreCount;
		&bufferMemoryBindSemaphore.get()			//const VkSemaphore*						pSignalSemaphores;
	};

	// Submit sparse bind commands for execution
	VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));

	// Create output buffer
	const VkBufferCreateInfo outputBufferCreateInfo = makeBufferCreateInfo(m_bufferSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::UniquePtr<Buffer>	 outputBuffer(new Buffer(deviceInterface, *m_logicalDevice, *allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible));

	// Create command buffer for compute and data transfer oparations
	const Unique<VkCommandPool>	  commandPool(makeCommandPool(deviceInterface, *m_logicalDevice, computeQueue.queueFamilyIndex));
	const Unique<VkCommandBuffer> commandBuffer(makeCommandBuffer(deviceInterface, *m_logicalDevice, *commandPool));

	// Start recording commands
	beginCommandBuffer(deviceInterface, *commandBuffer);

	// Create descriptor set
	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(deviceInterface, *m_logicalDevice));

	// Create compute pipeline
	const Unique<VkShaderModule>	shaderModule(createShaderModule(deviceInterface, *m_logicalDevice, m_context.getBinaryCollection().get("comp"), DE_NULL));
	const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(deviceInterface, *m_logicalDevice, *descriptorSetLayout));
	const Unique<VkPipeline>		computePipeline(makeComputePipeline(deviceInterface, *m_logicalDevice, *pipelineLayout, *shaderModule));

	deviceInterface.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

	// Create descriptor set
	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(deviceInterface, *m_logicalDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(deviceInterface, *m_logicalDevice, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo sparseBufferInfo = makeDescriptorBufferInfo(*sparseBufferWrite, 0u, m_bufferSizeInBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &sparseBufferInfo)
		.update(deviceInterface, *m_logicalDevice);

	deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	deUint32		 numInvocationsLeft			= m_bufferSizeInBytes / SIZE_OF_UINT_IN_SHADER;
	const tcu::UVec3 workGroupSize				= computeWorkGroupSize(numInvocationsLeft);
	const tcu::UVec3 maxComputeWorkGroupCount	= tcu::UVec3(65535u, 65535u, 65535u);

	numInvocationsLeft -= workGroupSize.x()*workGroupSize.y()*workGroupSize.z();

	const deUint32	xWorkGroupCount = std::min(numInvocationsLeft, maxComputeWorkGroupCount.x());
	numInvocationsLeft = numInvocationsLeft / xWorkGroupCount + ((numInvocationsLeft % xWorkGroupCount) ? 1u : 0u);
	const deUint32	yWorkGroupCount = std::min(numInvocationsLeft, maxComputeWorkGroupCount.y());
	numInvocationsLeft = numInvocationsLeft / yWorkGroupCount + ((numInvocationsLeft % yWorkGroupCount) ? 1u : 0u);
	const deUint32	zWorkGroupCount = std::min(numInvocationsLeft, maxComputeWorkGroupCount.z());
	numInvocationsLeft = numInvocationsLeft / zWorkGroupCount + ((numInvocationsLeft % zWorkGroupCount) ? 1u : 0u);

	if (numInvocationsLeft != 1u)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Buffer size is not supported");
	}

	deviceInterface.cmdDispatch(*commandBuffer, xWorkGroupCount, yWorkGroupCount, zWorkGroupCount);

	const VkBufferMemoryBarrier sparseBufferWriteBarrier
		= makeBufferMemoryBarrier(	VK_ACCESS_SHADER_WRITE_BIT,
									VK_ACCESS_TRANSFER_READ_BIT,
									*sparseBufferWrite,
									0ull,
									m_bufferSizeInBytes);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 1u, &sparseBufferWriteBarrier, 0u, DE_NULL);

	const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_bufferSizeInBytes);

	deviceInterface.cmdCopyBuffer(*commandBuffer, *sparseBufferRead, outputBuffer->get(), 1u, &bufferCopy);

	const VkBufferMemoryBarrier outputBufferHostBarrier 
		= makeBufferMemoryBarrier(	VK_ACCESS_TRANSFER_WRITE_BIT,
									VK_ACCESS_HOST_READ_BIT,
									outputBuffer->get(),
									0ull,
									m_bufferSizeInBytes);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferHostBarrier, 0u, DE_NULL);

	// End recording commands
	endCommandBuffer(deviceInterface, *commandBuffer);

	// The stage at which execution is going to wait for finish of sparse binding operations
	const VkPipelineStageFlags waitStageBits[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

	// Submit commands for execution and wait for completion
	submitCommandsAndWait(deviceInterface, *m_logicalDevice, computeQueue.queueHandle, *commandBuffer, 1u, &bufferMemoryBindSemaphore.get(), waitStageBits);

	// Retrieve data from output buffer to host memory
	const Allocation& allocation = outputBuffer->getAllocation();
	
	invalidateMappedMemoryRange(deviceInterface, *m_logicalDevice, allocation.getMemory(), allocation.getOffset(), m_bufferSizeInBytes);

	const deUint8*	outputData = static_cast<const deUint8*>(allocation.getHostPtr());

	// Prepare reference data
	std::vector<deUint8> referenceData;
	referenceData.resize(m_bufferSizeInBytes);

	std::vector<deUint32> referenceDataBlock;
	referenceDataBlock.resize(MODULO_DIVISOR);

	for (deUint32 valueNdx = 0; valueNdx < MODULO_DIVISOR; ++valueNdx)
	{
		referenceDataBlock[valueNdx] = valueNdx % MODULO_DIVISOR;
	}

	const deUint32 fullBlockSizeInBytes = MODULO_DIVISOR * SIZE_OF_UINT_IN_SHADER;
	const deUint32 lastBlockSizeInBytes = m_bufferSizeInBytes % fullBlockSizeInBytes;
	const deUint32 numberOfBlocks		= m_bufferSizeInBytes / fullBlockSizeInBytes + (lastBlockSizeInBytes ? 1u : 0u);

	for (deUint32 blockNdx = 0; blockNdx < numberOfBlocks; ++blockNdx)
	{
		const deUint32 offset = blockNdx * fullBlockSizeInBytes;
		deMemcpy(&referenceData[0] + offset, &referenceDataBlock[0], ((offset + fullBlockSizeInBytes) <= m_bufferSizeInBytes) ? fullBlockSizeInBytes : lastBlockSizeInBytes);
	}

	tcu::TestStatus testStatus = tcu::TestStatus::pass("Passed");

	// Compare reference data with output data
	if (deMemCmp(&referenceData[0], outputData, m_bufferSizeInBytes) != 0)
	{
		testStatus = tcu::TestStatus::fail("Failed");
	}

	// Wait for sparse queue to become idle
	deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

	return testStatus;
}

TestInstance* BufferSparseMemoryAliasingCase::createInstance (Context& context) const
{
	return new BufferSparseMemoryAliasingInstance(context, m_bufferSizeInBytes);
}

} // anonymous ns

tcu::TestCaseGroup* createBufferSparseMemoryAliasingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "buffer_sparse_memory_aliasing", "Sparse Buffer Memory Aliasing"));

	testGroup->addChild(new BufferSparseMemoryAliasingCase(testCtx, "buffer_size_2_10", "", 1 << 10, glu::GLSL_VERSION_440));
	testGroup->addChild(new BufferSparseMemoryAliasingCase(testCtx, "buffer_size_2_12", "", 1 << 12, glu::GLSL_VERSION_440));
	testGroup->addChild(new BufferSparseMemoryAliasingCase(testCtx, "buffer_size_2_16", "", 1 << 16, glu::GLSL_VERSION_440));
	testGroup->addChild(new BufferSparseMemoryAliasingCase(testCtx, "buffer_size_2_17", "", 1 << 17, glu::GLSL_VERSION_440));
	testGroup->addChild(new BufferSparseMemoryAliasingCase(testCtx, "buffer_size_2_20", "", 1 << 20, glu::GLSL_VERSION_440));
	testGroup->addChild(new BufferSparseMemoryAliasingCase(testCtx, "buffer_size_2_24", "", 1 << 24, glu::GLSL_VERSION_440));

	return testGroup.release();
}

} // sparse
} // vkt
