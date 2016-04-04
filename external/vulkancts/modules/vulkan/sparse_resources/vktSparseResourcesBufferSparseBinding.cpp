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
 * \file  vktSparseResourcesBufferSparseBinding.cpp
 * \brief Buffer Sparse Binding tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferSparseBinding.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

class BufferSparseBindingCase : public TestCase
{
public:
					BufferSparseBindingCase	(tcu::TestContext&	testCtx,
											 const std::string&	name,
											 const std::string&	description,
											 const deUint32		bufferSize);

	TestInstance*	createInstance			(Context&			context) const;

private:
	const deUint32	m_bufferSize;
};

BufferSparseBindingCase::BufferSparseBindingCase (tcu::TestContext&		testCtx,
												  const std::string&	name,
												  const std::string&	description,
												  const deUint32		bufferSize)
	: TestCase			(testCtx, name, description)
	, m_bufferSize		(bufferSize)
{
}

class BufferSparseBindingInstance : public SparseResourcesBaseInstance
{
public:
					BufferSparseBindingInstance (Context&		context,
												 const deUint32	bufferSize);

	tcu::TestStatus	iterate						(void);

private:
	const deUint32	m_bufferSize;
};

BufferSparseBindingInstance::BufferSparseBindingInstance (Context&			context,
														  const deUint32	bufferSize)

	: SparseResourcesBaseInstance	(context)
	, m_bufferSize					(bufferSize)
{
}

tcu::TestStatus BufferSparseBindingInstance::iterate (void)
{
	const InstanceInterface&	instance		= m_context.getInstanceInterface();
	const DeviceInterface&		deviceInterface	= m_context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= m_context.getPhysicalDevice();

	VkPhysicalDeviceFeatures deviceFeatures;
	instance.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	if (deviceFeatures.sparseBinding == false)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Sparse binding not supported");
	}

	VkPhysicalDeviceProperties deviceProperties;
	instance.getPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	QueueRequirementsVec queueRequirements;
	queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
	queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

	// Create logical device supporting both sparse and transfer queues
	if (!createDeviceSupportingQueues(queueRequirements))
	{
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Could not create device supporting sparse and compute queue");
	}

	const VkPhysicalDeviceMemoryProperties deviceMemoryProperties = getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	// Create memory allocator for logical device
	const de::UniquePtr<Allocator> allocator(new SimpleAllocator(deviceInterface, *m_logicalDevice, deviceMemoryProperties));

	// Create queue supporting sparse binding operations
	const Queue& sparseQueue = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);

	// Create queue supporting compute and transfer operations
	const Queue& computeQueue = getQueue(VK_QUEUE_COMPUTE_BIT, 0);

	VkBufferCreateInfo bufferCreateInfo;

	bufferCreateInfo.sType					= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;	// VkStructureType		sType;
	bufferCreateInfo.pNext					= DE_NULL;								// const void*			pNext;
	bufferCreateInfo.flags					= VK_BUFFER_CREATE_SPARSE_BINDING_BIT;	// VkBufferCreateFlags	flags;
	bufferCreateInfo.size					= m_bufferSize;							// VkDeviceSize			size;
	bufferCreateInfo.usage					= VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
											  VK_BUFFER_USAGE_TRANSFER_DST_BIT;		// VkBufferUsageFlags	usage;
	bufferCreateInfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;			// VkSharingMode		sharingMode;
	bufferCreateInfo.queueFamilyIndexCount	= 0u;									// deUint32				queueFamilyIndexCount;
	bufferCreateInfo.pQueueFamilyIndices	= DE_NULL;								// const deUint32*		pQueueFamilyIndices;

	const deUint32 queueFamilyIndices[] = { sparseQueue.queueFamilyIndex, computeQueue.queueFamilyIndex };

	if (sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex)
	{
		bufferCreateInfo.sharingMode			= VK_SHARING_MODE_CONCURRENT;	// VkSharingMode		sharingMode;
		bufferCreateInfo.queueFamilyIndexCount	= 2u;							// deUint32				queueFamilyIndexCount;
		bufferCreateInfo.pQueueFamilyIndices	= queueFamilyIndices;			// const deUint32*		pQueueFamilyIndices;
	}

	// Create sparse buffer
	const Unique<VkBuffer> sparseBuffer(createBuffer(deviceInterface, *m_logicalDevice, &bufferCreateInfo));

	const VkMemoryRequirements bufferMemRequirement = getBufferMemoryRequirements(deviceInterface, *m_logicalDevice, *sparseBuffer);

	if (bufferMemRequirement.size > deviceProperties.limits.sparseAddressSpaceSize)
	{
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Required memory size for sparse resources exceeds device limits");
	}

	DE_ASSERT((bufferMemRequirement.size % bufferMemRequirement.alignment) == 0);

	const deUint32 numSparseBinds = static_cast<deUint32>(bufferMemRequirement.size / bufferMemRequirement.alignment);

	typedef de::SharedPtr< Unique<VkDeviceMemory> > DeviceMemoryUniquePtr;

	std::vector<VkSparseMemoryBind>		sparseMemoryBinds;
	std::vector<DeviceMemoryUniquePtr>	deviceMemUniquePtrVec;
	const deUint32						memoryType = findMatchingMemoryType(deviceMemoryProperties, bufferMemRequirement, MemoryRequirement::Any);

	if (memoryType == NO_MATCH_FOUND)
	{
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "No matching memory type found");
	}

	for (deUint32 sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
	{
		const VkMemoryAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType			sType;
			DE_NULL,								//	const void*				pNext;
			bufferMemRequirement.alignment,			//	VkDeviceSize			allocationSize;
			memoryType,								//	deUint32				memoryTypeIndex;
		};

		VkDeviceMemory deviceMemory = 0;
		VK_CHECK(deviceInterface.allocateMemory(*m_logicalDevice, &allocInfo, DE_NULL, &deviceMemory));

		deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(deviceMemory), Deleter<VkDeviceMemory>(deviceInterface, *m_logicalDevice, DE_NULL))));

		const VkSparseMemoryBind sparseMemoryBind = makeSparseMemoryBind
		(
			bufferMemRequirement.alignment * sparseBindNdx,	//VkDeviceSize				resourceOffset
			bufferMemRequirement.alignment,					//VkDeviceSize				size
			deviceMemory,									//VkDeviceMemory			memory
			0u,												//VkDeviceSize				memoryOffset
			0u												//VkSparseMemoryBindFlags	flags
		);

		sparseMemoryBinds.push_back(sparseMemoryBind);
	}

	const VkSparseBufferMemoryBindInfo sparseBufferBindInfo = makeSparseBufferMemoryBindInfo
	(
		*sparseBuffer,			//VkBuffer					buffer;
		numSparseBinds,			//deUint32					bindCount;
		&sparseMemoryBinds[0]	//const VkSparseMemoryBind*	Binds;
	);

	const Unique<VkSemaphore> bufferMemoryBindSemaphore(makeSemaphore(deviceInterface, *m_logicalDevice));

	const VkBindSparseInfo bindSparseInfo =
	{
		VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,			//VkStructureType							sType;
		DE_NULL,									//const void*								pNext;
		0u,											//deUint32									waitSemaphoreCount;
		DE_NULL,									//const VkSemaphore*						pWaitSemaphores;
		1u,											//deUint32									bufferBindCount;
		&sparseBufferBindInfo,						//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
		0u,											//deUint32									imageOpaqueBindCount;
		DE_NULL,									//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
		0u,											//deUint32									imageBindCount;
		DE_NULL,									//const VkSparseImageMemoryBindInfo*		pImageBinds;
		1u,											//deUint32									signalSemaphoreCount;
		&bufferMemoryBindSemaphore.get()			//const VkSemaphore*						pSignalSemaphores;
	};

	// Submit sparse bind commands for execution
	VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));

	// Create command buffer for transfer oparations
	const Unique<VkCommandPool>		commandPool(makeCommandPool(deviceInterface, *m_logicalDevice, computeQueue.queueFamilyIndex));
	const Unique<VkCommandBuffer>	commandBuffer(makeCommandBuffer(deviceInterface, *m_logicalDevice, *commandPool));

	// Start recording transfer commands
	beginCommandBuffer(deviceInterface, *commandBuffer);

	const VkBufferCreateInfo	inputBufferCreateInfo = makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	const de::UniquePtr<Buffer>	inputBuffer(new Buffer(deviceInterface, *m_logicalDevice, *allocator, inputBufferCreateInfo, MemoryRequirement::HostVisible));

	std::vector<deUint8> referenceData;
	referenceData.resize(m_bufferSize);

	for (deUint32 valueNdx = 0; valueNdx < m_bufferSize; ++valueNdx)
	{	
		referenceData[valueNdx] = static_cast<deUint8>((valueNdx % bufferMemRequirement.alignment) + 1u);
	}

	deMemcpy(inputBuffer->getAllocation().getHostPtr(), &referenceData[0], m_bufferSize);

	flushMappedMemoryRange(deviceInterface, *m_logicalDevice, inputBuffer->getAllocation().getMemory(), inputBuffer->getAllocation().getOffset(), m_bufferSize);

	const VkBufferMemoryBarrier inputBufferBarrier 
		= makeBufferMemoryBarrier(	VK_ACCESS_HOST_WRITE_BIT,
									VK_ACCESS_TRANSFER_READ_BIT,
									inputBuffer->get(),
									0u,
									m_bufferSize);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 1u, &inputBufferBarrier, 0u, DE_NULL);

	const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_bufferSize);

	deviceInterface.cmdCopyBuffer(*commandBuffer, inputBuffer->get(), *sparseBuffer, 1u, &bufferCopy);

	const VkBufferMemoryBarrier sparseBufferBarrier 
		= makeBufferMemoryBarrier(	VK_ACCESS_TRANSFER_WRITE_BIT,
									VK_ACCESS_TRANSFER_READ_BIT,
									*sparseBuffer,
									0u,
									m_bufferSize);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 1u, &sparseBufferBarrier, 0u, DE_NULL);

	const VkBufferCreateInfo	outputBufferCreateInfo = makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const de::UniquePtr<Buffer>	outputBuffer(new Buffer(deviceInterface, *m_logicalDevice, *allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible));

	deviceInterface.cmdCopyBuffer(*commandBuffer, *sparseBuffer, outputBuffer->get(), 1u, &bufferCopy);

	const VkBufferMemoryBarrier outputBufferBarrier 
		= makeBufferMemoryBarrier(	VK_ACCESS_TRANSFER_WRITE_BIT,
									VK_ACCESS_HOST_READ_BIT,
									outputBuffer->get(),
									0u,
									m_bufferSize);

	deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferBarrier, 0u, DE_NULL);

	// End recording transfer commands
	endCommandBuffer(deviceInterface, *commandBuffer);

	const VkPipelineStageFlags waitStageBits[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };

	// Submit transfer commands for execution and wait for completion
	submitCommandsAndWait(deviceInterface, *m_logicalDevice, computeQueue.queueHandle, *commandBuffer, 1u, &bufferMemoryBindSemaphore.get(), waitStageBits);

	// Retrieve data from output buffer to host memory
	const Allocation& allocation = outputBuffer->getAllocation();

	invalidateMappedMemoryRange(deviceInterface, *m_logicalDevice, allocation.getMemory(), allocation.getOffset(), m_bufferSize);

	const deUint8*	outputData = static_cast<const deUint8*>(allocation.getHostPtr());
	tcu::TestStatus testStatus = tcu::TestStatus::incomplete();

	// Compare output data with reference data
	if (deMemCmp(&referenceData[0], outputData, m_bufferSize) == 0)
		testStatus = tcu::TestStatus::pass("Passed");
	else
		testStatus = tcu::TestStatus::fail("Failed");

	// Wait for sparse queue to become idle
	deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

	return testStatus;
}

TestInstance* BufferSparseBindingCase::createInstance (Context& context) const
{
	return new BufferSparseBindingInstance(context, m_bufferSize);
}

} // anonymous ns

tcu::TestCaseGroup* createBufferSparseBindingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "buffer_sparse_binding", "Buffer Sparse Binding"));

	testGroup->addChild(new BufferSparseBindingCase(testCtx, "buffer_size_2_10", "", 1 << 10));
	testGroup->addChild(new BufferSparseBindingCase(testCtx, "buffer_size_2_12", "", 1 << 12));
	testGroup->addChild(new BufferSparseBindingCase(testCtx, "buffer_size_2_16", "", 1 << 16));
	testGroup->addChild(new BufferSparseBindingCase(testCtx, "buffer_size_2_17", "", 1 << 17));
	testGroup->addChild(new BufferSparseBindingCase(testCtx, "buffer_size_2_20", "", 1 << 20));
	testGroup->addChild(new BufferSparseBindingCase(testCtx, "buffer_size_2_24", "", 1 << 24));

	return testGroup.release();
}

} // sparse
} // vkt
