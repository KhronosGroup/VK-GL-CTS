/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Indirect Compute Dispatch tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeIndirectComputeDispatchTests.hpp"
#include "vktComputeTestsUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include <string>
#include <map>
#include <vector>

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"

#include "gluShaderUtil.hpp"
#include "tcuCommandLine.hpp"

#include <set>

namespace vkt
{
namespace compute
{
namespace
{
std::vector<std::string> removeCoreExtensions (const std::vector<std::string>& supportedExtensions, const std::vector<const char*>& coreExtensions)
{
	std::vector<std::string>	nonCoreExtensions;
	std::set<std::string>		excludedExtensions	(coreExtensions.begin(), coreExtensions.end());

	for (const auto & supportedExtension : supportedExtensions)
	{
		if (!de::contains(excludedExtensions, supportedExtension))
			nonCoreExtensions.push_back(supportedExtension);
	}

	return nonCoreExtensions;
}

// Creates a device that has a queue for compute capabilities without graphics.
vk::Move<vk::VkDevice> createCustomDevice (Context& context,
#ifdef CTS_USES_VULKANSC
										  const vkt::CustomInstance& customInstance,
#endif // CTS_USES_VULKANSC
										  uint32_t& queueFamilyIndex)
{
#ifdef CTS_USES_VULKANSC
	const vk::InstanceInterface&	instanceDriver		= customInstance.getDriver();
	const vk::VkPhysicalDevice		physicalDevice		= chooseDevice(instanceDriver, customInstance, context.getTestContext().getCommandLine());
#else
	const vk::InstanceInterface&	instanceDriver		= context.getInstanceInterface();
	const vk::VkPhysicalDevice		physicalDevice		= context.getPhysicalDevice();
#endif // CTS_USES_VULKANSC

	const std::vector<vk::VkQueueFamilyProperties>	queueFamilies = getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

	queueFamilyIndex = 0;
	for (const auto &queueFamily: queueFamilies)
	{
		if (queueFamily.queueFlags & vk::VK_QUEUE_COMPUTE_BIT && !(queueFamily.queueFlags & vk::VK_QUEUE_GRAPHICS_BIT))
			break;
		else
			queueFamilyIndex++;
	}

	// One queue family without a graphics bit should be found, since this is checked in checkSupport.
	DE_ASSERT(queueFamilyIndex < queueFamilies.size());

	const float										queuePriority				= 1.0f;
	const vk::VkDeviceQueueCreateInfo				deviceQueueCreateInfos[]	= {
		{
			vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			(vk::VkDeviceQueueCreateFlags)0u,				// VkDeviceQueueCreateFlags		flags;
			context.getUniversalQueueFamilyIndex(),			// uint32_t						queueFamilyIndex;
			1u,												// uint32_t						queueCount;
			&queuePriority,									// const float*					pQueuePriorities;
		},
		{
			vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			(vk::VkDeviceQueueCreateFlags)0u,				// VkDeviceQueueCreateFlags		flags;
			queueFamilyIndex,								// uint32_t						queueFamilyIndex;
			1u,												// uint32_t						queueCount;
			&queuePriority,									// const float*					pQueuePriorities;
		}
	};

	// context.getDeviceExtensions() returns supported device extension including extensions that have been promoted to
	// Vulkan core. The core extensions must be removed from the list.
	std::vector<const char*>						coreExtensions;
	vk::getCoreDeviceExtensions(context.getUsedApiVersion(), coreExtensions);
	std::vector<std::string> nonCoreExtensions(removeCoreExtensions(context.getDeviceExtensions(), coreExtensions));

	std::vector<const char*>						extensionNames;
	extensionNames.reserve(nonCoreExtensions.size());
	for (const std::string& extension : nonCoreExtensions)
		extensionNames.push_back(extension.c_str());

	const auto&										deviceFeatures2				= context.getDeviceFeatures2();

	const void *pNext = &deviceFeatures2;
#ifdef CTS_USES_VULKANSC
	VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() : resetDeviceObjectReservationCreateInfo();
	memReservationInfo.pNext = pNext;
	pNext = &memReservationInfo;

	VkPipelineCacheCreateInfo			pcCI;
	std::vector<VkPipelinePoolSize>		poolSizes;
	if (context.getTestContext().getCommandLine().isSubProcess())
	{
		if (context.getResourceInterface()->getCacheDataSize() > 0)
		{
			pcCI =
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,			// VkStructureType				sType;
				DE_NULL,												// const void*					pNext;
				VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
					VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
				context.getResourceInterface()->getCacheDataSize(),	// deUintptr					initialDataSize;
				context.getResourceInterface()->getCacheData()		// const void*					pInitialData;
			};
			memReservationInfo.pipelineCacheCreateInfoCount		= 1;
			memReservationInfo.pPipelineCacheCreateInfos		= &pcCI;
		}
		poolSizes							= context.getResourceInterface()->getPipelinePoolSizes();
		if (!poolSizes.empty())
		{
			memReservationInfo.pipelinePoolSizeCount		= deUint32(poolSizes.size());
			memReservationInfo.pPipelinePoolSizes			= poolSizes.data();
		}
	}
#endif // CTS_USES_VULKANSC

	const vk::VkDeviceCreateInfo					deviceCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,		// VkStructureType					sType;
		pNext,											// const void*						pNext;
		(vk::VkDeviceCreateFlags)0u,					// VkDeviceCreateFlags				flags;
		DE_LENGTH_OF_ARRAY(deviceQueueCreateInfos),		// uint32_t							queueCreateInfoCount;
		deviceQueueCreateInfos,							// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,												// uint32_t							enabledLayerCount;
		DE_NULL,										// const char* const*				ppEnabledLayerNames;
		static_cast<uint32_t>(extensionNames.size()),	// uint32_t							enabledExtensionCount;
		extensionNames.data(),							// const char* const*				ppEnabledExtensionNames;
		DE_NULL,										// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	return vkt::createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
								   context.getPlatformInterface(),
#ifdef CTS_USES_VULKANSC
								   customInstance,
#else
								   context.getInstance(),
#endif
								   instanceDriver, physicalDevice, &deviceCreateInfo);
}

enum
{
	RESULT_BLOCK_BASE_SIZE			= 4 * (int)sizeof(deUint32), // uvec3 + uint
	RESULT_BLOCK_NUM_PASSED_OFFSET	= 3 * (int)sizeof(deUint32),
	INDIRECT_COMMAND_OFFSET			= 3 * (int)sizeof(deUint32),
};

vk::VkDeviceSize getResultBlockAlignedSize (const vk::InstanceInterface&	instance_interface,
											const vk::VkPhysicalDevice		physicalDevice,
											const vk::VkDeviceSize			baseSize)
{
	// TODO getPhysicalDeviceProperties() was added to vkQueryUtil in 41-image-load-store-tests. Use it once it's merged.
	vk::VkPhysicalDeviceProperties deviceProperties;
	instance_interface.getPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vk::VkDeviceSize alignment = deviceProperties.limits.minStorageBufferOffsetAlignment;

	if (alignment == 0 || (baseSize % alignment == 0))
		return baseSize;
	else
		return (baseSize / alignment + 1)*alignment;
}

struct DispatchCommand
{
				DispatchCommand (const deIntptr		offset,
								 const tcu::UVec3&	numWorkGroups)
					: m_offset			(offset)
					, m_numWorkGroups	(numWorkGroups) {}

	deIntptr	m_offset;
	tcu::UVec3	m_numWorkGroups;
};

typedef std::vector<DispatchCommand> DispatchCommandsVec;

struct DispatchCaseDesc
{
								DispatchCaseDesc (const char*					name,
												  const char*					description,
												  const deUintptr				bufferSize,
												  const tcu::UVec3				workGroupSize,
												  const DispatchCommandsVec&	dispatchCommands,
												  const bool					computeQueueOnly)
									: m_name				(name)
									, m_description			(description)
									, m_bufferSize			(bufferSize)
									, m_workGroupSize		(workGroupSize)
									, m_dispatchCommands	(dispatchCommands)
									, m_computeOnlyQueue	(computeQueueOnly) {}

	const char*					m_name;
	const char*					m_description;
	const deUintptr				m_bufferSize;
	const tcu::UVec3			m_workGroupSize;
	const DispatchCommandsVec	m_dispatchCommands;
	const bool					m_computeOnlyQueue;
};

class IndirectDispatchInstanceBufferUpload : public vkt::TestInstance
{
public:
									IndirectDispatchInstanceBufferUpload	(Context&					context,
																			 const std::string&			name,
																			 const deUintptr			bufferSize,
																			 const tcu::UVec3&			workGroupSize,
																			 const DispatchCommandsVec& dispatchCommands,
																			 const bool					computeQueueOnly,
																			 const vk::ComputePipelineConstructionType computePipelineConstructionType);

	virtual							~IndirectDispatchInstanceBufferUpload	(void) {}

	virtual tcu::TestStatus			iterate									(void);

protected:
	virtual void					fillIndirectBufferData					(const vk::VkCommandBuffer		commandBuffer,
																			 const vk::DeviceInterface&     vkdi,
																			 const vk::BufferWithMemory&	indirectBuffer);

	deBool							verifyResultBuffer						(const vk::BufferWithMemory&	resultBuffer,
																			 const vk::DeviceInterface&     vkdi,
																			 const vk::VkDeviceSize			resultBlockSize) const;

	Context&							m_context;
	const std::string					m_name;

	vk::VkDevice						m_device;
#ifdef CTS_USES_VULKANSC
	const CustomInstance				m_customInstance;
#endif // CTS_USES_VULKANSC
	vk::Move<vk::VkDevice>				m_customDevice;
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>		m_deviceDriver;
#else
	de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>	m_deviceDriver;
#endif // CTS_USES_VULKANSC

	vk::VkQueue							m_queue;
	deUint32							m_queueFamilyIndex;

	const deUintptr						m_bufferSize;
	const tcu::UVec3					m_workGroupSize;
	const DispatchCommandsVec			m_dispatchCommands;

	de::MovePtr<vk::Allocator>			m_allocator;

	const bool							m_computeQueueOnly;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
private:
	IndirectDispatchInstanceBufferUpload (const vkt::TestInstance&);
	IndirectDispatchInstanceBufferUpload& operator= (const vkt::TestInstance&);
};

IndirectDispatchInstanceBufferUpload::IndirectDispatchInstanceBufferUpload (Context&									context,
																			const std::string&							name,
																			const deUintptr								bufferSize,
																			const tcu::UVec3&							workGroupSize,
																			const DispatchCommandsVec&					dispatchCommands,
																			const bool									computeQueueOnly,
																			const vk::ComputePipelineConstructionType	computePipelineConstructionType)
	: vkt::TestInstance					(context)
	, m_context							(context)
	, m_name							(name)
	, m_device							(context.getDevice())
#ifdef CTS_USES_VULKANSC
	, m_customInstance					(createCustomInstanceFromContext(context))
#endif // CTS_USES_VULKANSC
	, m_queue							(context.getUniversalQueue())
	, m_queueFamilyIndex				(context.getUniversalQueueFamilyIndex())
	, m_bufferSize						(bufferSize)
	, m_workGroupSize					(workGroupSize)
	, m_dispatchCommands				(dispatchCommands)
	, m_computeQueueOnly				(computeQueueOnly)
	, m_computePipelineConstructionType	(computePipelineConstructionType)
{
}

void IndirectDispatchInstanceBufferUpload::fillIndirectBufferData (const vk::VkCommandBuffer commandBuffer, const vk::DeviceInterface& vkdi, const vk::BufferWithMemory& indirectBuffer)
{
	DE_UNREF(commandBuffer);

	const vk::Allocation& alloc = indirectBuffer.getAllocation();
	deUint8* indirectDataPtr = reinterpret_cast<deUint8*>(alloc.getHostPtr());

	for (DispatchCommandsVec::const_iterator cmdIter = m_dispatchCommands.begin(); cmdIter != m_dispatchCommands.end(); ++cmdIter)
	{
		DE_ASSERT(cmdIter->m_offset >= 0);
		DE_ASSERT(cmdIter->m_offset % sizeof(deUint32) == 0);
		DE_ASSERT(cmdIter->m_offset + INDIRECT_COMMAND_OFFSET <= (deIntptr)m_bufferSize);

		deUint32* const dstPtr = (deUint32*)&indirectDataPtr[cmdIter->m_offset];

		dstPtr[0] = cmdIter->m_numWorkGroups[0];
		dstPtr[1] = cmdIter->m_numWorkGroups[1];
		dstPtr[2] = cmdIter->m_numWorkGroups[2];
	}

	vk::flushAlloc(vkdi, m_device, alloc);
}

tcu::TestStatus IndirectDispatchInstanceBufferUpload::iterate (void)
{
#ifdef CTS_USES_VULKANSC
	const vk::InstanceInterface&	vki						= m_customInstance.getDriver();
#else
	const vk::InstanceInterface&	vki						= m_context.getInstanceInterface();
#endif // CTS_USES_VULKANSC
	tcu::TestContext& testCtx = m_context.getTestContext();

	testCtx.getLog() << tcu::TestLog::Message << "GL_DISPATCH_INDIRECT_BUFFER size = " << m_bufferSize << tcu::TestLog::EndMessage;
	{
		tcu::ScopedLogSection section(testCtx.getLog(), "Commands", "Indirect Dispatch Commands (" + de::toString(m_dispatchCommands.size()) + " in total)");

		for (deUint32 cmdNdx = 0; cmdNdx < m_dispatchCommands.size(); ++cmdNdx)
		{
			testCtx.getLog()
				<< tcu::TestLog::Message
				<< cmdNdx << ": " << "offset = " << m_dispatchCommands[cmdNdx].m_offset << ", numWorkGroups = " << m_dispatchCommands[cmdNdx].m_numWorkGroups
				<< tcu::TestLog::EndMessage;
		}
	}

	if (m_computeQueueOnly)
	{
		// m_queueFamilyIndex will be updated in createCustomDevice() to match the requested queue type.
		m_customDevice = createCustomDevice(m_context,
#ifdef CTS_USES_VULKANSC
											m_customInstance,
#endif
											m_queueFamilyIndex);
		m_device = m_customDevice.get();
#ifndef CTS_USES_VULKANSC
		m_deviceDriver = de::MovePtr<vk::DeviceDriver>(new vk::DeviceDriver(m_context.getPlatformInterface(), m_context.getInstance(), m_device, m_context.getUsedApiVersion()));
#else
		m_deviceDriver = de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>(new vk::DeviceDriverSC(m_context.getPlatformInterface(), m_customInstance, m_device, m_context.getTestContext().getCommandLine(), m_context.getResourceInterface(), m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(), m_context.getUsedApiVersion()), vk::DeinitDeviceDeleter(m_context.getResourceInterface().get(), m_device));
#endif // CTS_USES_VULKANSC
	}
#ifndef CTS_USES_VULKANSC
	const vk::DeviceInterface& vkdi = m_context.getDeviceInterface();
#else
	const vk::DeviceInterface& vkdi = (m_computeQueueOnly && (DE_NULL != m_deviceDriver)) ? *m_deviceDriver : m_context.getDeviceInterface();
#endif // CTS_USES_VULKANSC
	if (m_computeQueueOnly)
	{
		m_queue = getDeviceQueue(vkdi, m_device, m_queueFamilyIndex, 0u);
		m_allocator		= de::MovePtr<vk::Allocator>(new vk::SimpleAllocator(vkdi, m_device, vk::getPhysicalDeviceMemoryProperties(vki, m_context.getPhysicalDevice())));
	}
	vk::Allocator&			allocator			= m_allocator.get() ? *m_allocator : m_context.getDefaultAllocator();

	// Create result buffer
	const vk::VkDeviceSize resultBlockSize = getResultBlockAlignedSize(vki, m_context.getPhysicalDevice(), RESULT_BLOCK_BASE_SIZE);
	const vk::VkDeviceSize resultBufferSize = resultBlockSize * (deUint32)m_dispatchCommands.size();

	vk::BufferWithMemory resultBuffer(
		vkdi, m_device, allocator,
		vk::makeBufferCreateInfo(resultBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
		vk::MemoryRequirement::HostVisible);

	{
		const vk::Allocation& alloc = resultBuffer.getAllocation();
		deUint8* resultDataPtr = reinterpret_cast<deUint8*>(alloc.getHostPtr());

		for (deUint32 cmdNdx = 0; cmdNdx < m_dispatchCommands.size(); ++cmdNdx)
		{
			deUint8* const	dstPtr = &resultDataPtr[resultBlockSize*cmdNdx];

			*(deUint32*)(dstPtr + 0 * sizeof(deUint32)) = m_dispatchCommands[cmdNdx].m_numWorkGroups[0];
			*(deUint32*)(dstPtr + 1 * sizeof(deUint32)) = m_dispatchCommands[cmdNdx].m_numWorkGroups[1];
			*(deUint32*)(dstPtr + 2 * sizeof(deUint32)) = m_dispatchCommands[cmdNdx].m_numWorkGroups[2];
			*(deUint32*)(dstPtr + RESULT_BLOCK_NUM_PASSED_OFFSET) = 0;
		}

		vk::flushAlloc(vkdi, m_device, alloc);
	}

	// Create descriptorSetLayout
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vkdi, m_device));

	// Create compute pipeline
	vk::ComputePipelineWrapper			computePipeline(vkdi, m_device, m_computePipelineConstructionType, m_context.getBinaryCollection().get("indirect_dispatch_" + m_name + "_verify"));
	computePipeline.setDescriptorSetLayout(descriptorSetLayout.get());
	computePipeline.buildPipeline();

	// Create descriptor pool
	const vk::Unique<vk::VkDescriptorPool> descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (deUint32)m_dispatchCommands.size())
		.build(vkdi, m_device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, static_cast<deUint32>(m_dispatchCommands.size())));

	const vk::VkBufferMemoryBarrier ssboPostBarrier = makeBufferMemoryBarrier(
		vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0ull, resultBufferSize);

	// Create command buffer
	const vk::Unique<vk::VkCommandPool> cmdPool(makeCommandPool(vkdi, m_device, m_queueFamilyIndex));
	const vk::Unique<vk::VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vkdi, m_device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Begin recording commands
	beginCommandBuffer(vkdi, *cmdBuffer);

	// Create indirect buffer
	vk::BufferWithMemory indirectBuffer(
		vkdi, m_device, allocator,
		vk::makeBufferCreateInfo(m_bufferSize, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
		vk::MemoryRequirement::HostVisible);
	fillIndirectBufferData(*cmdBuffer, vkdi, indirectBuffer);

	// Bind compute pipeline
	computePipeline.bind(*cmdBuffer);

	// Allocate descriptor sets
	typedef de::SharedPtr<vk::Unique<vk::VkDescriptorSet> > SharedVkDescriptorSet;
	std::vector<SharedVkDescriptorSet> descriptorSets(m_dispatchCommands.size());

	vk::VkDeviceSize curOffset = 0;

	// Create descriptor sets
	for (deUint32 cmdNdx = 0; cmdNdx < m_dispatchCommands.size(); ++cmdNdx)
	{
		descriptorSets[cmdNdx] = SharedVkDescriptorSet(new vk::Unique<vk::VkDescriptorSet>(
									makeDescriptorSet(vkdi, m_device, *descriptorPool, *descriptorSetLayout)));

		const vk::VkDescriptorBufferInfo resultDescriptorInfo = makeDescriptorBufferInfo(*resultBuffer, curOffset, resultBlockSize);

		vk::DescriptorSetUpdateBuilder descriptorSetBuilder;
		descriptorSetBuilder.writeSingle(**descriptorSets[cmdNdx], vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo);
		descriptorSetBuilder.update(vkdi, m_device);

		// Bind descriptor set
		vkdi.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.getPipelineLayout(), 0u, 1u, &(**descriptorSets[cmdNdx]), 0u, DE_NULL);

		// Dispatch indirect compute command
		vkdi.cmdDispatchIndirect(*cmdBuffer, *indirectBuffer, m_dispatchCommands[cmdNdx].m_offset);

		curOffset += resultBlockSize;
	}

	// Insert memory barrier
	vkdi.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, (vk::VkDependencyFlags)0,
										  0, (const vk::VkMemoryBarrier*)DE_NULL,
										  1, &ssboPostBarrier,
										  0, (const vk::VkImageMemoryBarrier*)DE_NULL);

	// End recording commands
	endCommandBuffer(vkdi, *cmdBuffer);

	// Wait for command buffer execution finish
	submitCommandsAndWait(vkdi, m_device, m_queue, *cmdBuffer);

	// Check if result buffer contains valid values
	if (verifyResultBuffer(resultBuffer, vkdi, resultBlockSize))
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Pass");
	else
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Invalid values in result buffer");
}

deBool IndirectDispatchInstanceBufferUpload::verifyResultBuffer (const vk::BufferWithMemory&	resultBuffer,
																 const vk::DeviceInterface&     vkdi,
																 const vk::VkDeviceSize			resultBlockSize) const
{
	deBool allOk = true;
	const vk::Allocation& alloc = resultBuffer.getAllocation();
	vk::invalidateAlloc(vkdi, m_device, alloc);

	const deUint8* const resultDataPtr = reinterpret_cast<deUint8*>(alloc.getHostPtr());

	for (deUint32 cmdNdx = 0; cmdNdx < m_dispatchCommands.size(); cmdNdx++)
	{
		const DispatchCommand&	cmd = m_dispatchCommands[cmdNdx];
		const deUint8* const	srcPtr = (const deUint8*)resultDataPtr + cmdNdx*resultBlockSize;
		const deUint32			numPassed = *(const deUint32*)(srcPtr + RESULT_BLOCK_NUM_PASSED_OFFSET);
		const deUint32			numInvocationsPerGroup = m_workGroupSize[0] * m_workGroupSize[1] * m_workGroupSize[2];
		const deUint32			numGroups = cmd.m_numWorkGroups[0] * cmd.m_numWorkGroups[1] * cmd.m_numWorkGroups[2];
		const deUint32			expectedCount = numInvocationsPerGroup * numGroups;

		if (numPassed != expectedCount)
		{
			tcu::TestContext& testCtx = m_context.getTestContext();

			testCtx.getLog()
				<< tcu::TestLog::Message
				<< "ERROR: got invalid result for invocation " << cmdNdx
				<< ": got numPassed = " << numPassed << ", expected " << expectedCount
				<< tcu::TestLog::EndMessage;

			allOk = false;
		}
	}

	return allOk;
}

class IndirectDispatchCaseBufferUpload : public vkt::TestCase
{
public:
								IndirectDispatchCaseBufferUpload	(tcu::TestContext&			testCtx,
																	 const DispatchCaseDesc&	caseDesc,
																	 const glu::GLSLVersion		glslVersion,
																	 const vk::ComputePipelineConstructionType computePipelineConstructionType);

	virtual						~IndirectDispatchCaseBufferUpload	(void) {}

	virtual void				initPrograms						(vk::SourceCollections&		programCollection) const;
	virtual TestInstance*		createInstance						(Context&					context) const;
	virtual void				checkSupport						(Context& context) const;

protected:
	const deUintptr						m_bufferSize;
	const tcu::UVec3					m_workGroupSize;
	const DispatchCommandsVec			m_dispatchCommands;
	const glu::GLSLVersion				m_glslVersion;
	const bool							m_computeOnlyQueue;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;

private:
	IndirectDispatchCaseBufferUpload (const vkt::TestCase&);
	IndirectDispatchCaseBufferUpload& operator= (const vkt::TestCase&);
};

IndirectDispatchCaseBufferUpload::IndirectDispatchCaseBufferUpload (tcu::TestContext&		testCtx,
																	const DispatchCaseDesc& caseDesc,
																	const glu::GLSLVersion	glslVersion,
																	const vk::ComputePipelineConstructionType computePipelineConstructionType)
	: vkt::TestCase						(testCtx, caseDesc.m_name, caseDesc.m_description)
	, m_bufferSize						(caseDesc.m_bufferSize)
	, m_workGroupSize					(caseDesc.m_workGroupSize)
	, m_dispatchCommands				(caseDesc.m_dispatchCommands)
	, m_glslVersion						(glslVersion)
	, m_computeOnlyQueue				(caseDesc.m_computeOnlyQueue)
	, m_computePipelineConstructionType	(computePipelineConstructionType)
{
}

void IndirectDispatchCaseBufferUpload::initPrograms (vk::SourceCollections& programCollection) const
{
	const char* const	versionDecl = glu::getGLSLVersionDeclaration(m_glslVersion);

	std::ostringstream	verifyBuffer;

	verifyBuffer
		<< versionDecl << "\n"
		<< "layout(local_size_x = ${LOCAL_SIZE_X}, local_size_y = ${LOCAL_SIZE_Y}, local_size_z = ${LOCAL_SIZE_Z}) in;\n"
		<< "layout(set = 0, binding = 0, std430) buffer Result\n"
		<< "{\n"
		<< "    uvec3           expectedGroupCount;\n"
		<< "    coherent uint   numPassed;\n"
		<< "} result;\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    if (all(equal(result.expectedGroupCount, gl_NumWorkGroups)))\n"
		<< "        atomicAdd(result.numPassed, 1u);\n"
		<< "}\n";

	std::map<std::string, std::string> args;

	args["LOCAL_SIZE_X"] = de::toString(m_workGroupSize.x());
	args["LOCAL_SIZE_Y"] = de::toString(m_workGroupSize.y());
	args["LOCAL_SIZE_Z"] = de::toString(m_workGroupSize.z());

	std::string verifyProgramString = tcu::StringTemplate(verifyBuffer.str()).specialize(args);

	programCollection.glslSources.add("indirect_dispatch_" + m_name + "_verify") << glu::ComputeSource(verifyProgramString);
}

TestInstance* IndirectDispatchCaseBufferUpload::createInstance (Context& context) const
{
	return new IndirectDispatchInstanceBufferUpload(context, m_name, m_bufferSize, m_workGroupSize, m_dispatchCommands, m_computeOnlyQueue, m_computePipelineConstructionType);
}

void IndirectDispatchCaseBufferUpload::checkSupport (Context& context) const
{
	// Find at least one queue family that supports compute queue but does NOT support graphics queue.
	if (m_computeOnlyQueue)
	{
		bool foundQueue = false;
		const std::vector<vk::VkQueueFamilyProperties> queueFamilies = getPhysicalDeviceQueueFamilyProperties(
				context.getInstanceInterface(), context.getPhysicalDevice());

		for (const auto &queueFamily: queueFamilies)
		{
			if (queueFamily.queueFlags & vk::VK_QUEUE_COMPUTE_BIT &&
				!(queueFamily.queueFlags & vk::VK_QUEUE_GRAPHICS_BIT))
			{
				foundQueue = true;
				break;
			}
		}
		if (!foundQueue)
			TCU_THROW(NotSupportedError, "No queue family found that only supports compute queue.");
	}

	checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_computePipelineConstructionType);
}

	class IndirectDispatchInstanceBufferGenerate : public IndirectDispatchInstanceBufferUpload
{
public:
									IndirectDispatchInstanceBufferGenerate	(Context&					context,
																			 const std::string&			name,
																			 const deUintptr			bufferSize,
																			 const tcu::UVec3&			workGroupSize,
																			 const DispatchCommandsVec&	dispatchCommands,
																			 const bool					computeOnlyQueue,
																			 const vk::ComputePipelineConstructionType computePipelineConstructionType)

										: IndirectDispatchInstanceBufferUpload(context, name, bufferSize, workGroupSize, dispatchCommands, computeOnlyQueue, computePipelineConstructionType) {}

	virtual							~IndirectDispatchInstanceBufferGenerate	(void) {}

protected:
	virtual void					fillIndirectBufferData					(const vk::VkCommandBuffer		commandBuffer,
																			 const vk::DeviceInterface&     vkdi,
																			 const vk::BufferWithMemory&	indirectBuffer);

	vk::Move<vk::VkDescriptorSetLayout>	m_descriptorSetLayout;
	vk::Move<vk::VkDescriptorPool>		m_descriptorPool;
	vk::Move<vk::VkDescriptorSet>		m_descriptorSet;
	vk::Move<vk::VkPipelineLayout>		m_pipelineLayout;
	vk::Move<vk::VkPipeline>			m_computePipeline;

private:
	IndirectDispatchInstanceBufferGenerate (const vkt::TestInstance&);
	IndirectDispatchInstanceBufferGenerate& operator= (const vkt::TestInstance&);
};

void IndirectDispatchInstanceBufferGenerate::fillIndirectBufferData (const vk::VkCommandBuffer commandBuffer, const vk::DeviceInterface& vkdi, const vk::BufferWithMemory& indirectBuffer)
{
	// Create compute shader that generates data for indirect buffer
	const vk::Unique<vk::VkShaderModule> genIndirectBufferDataShader(createShaderModule(
		vkdi, m_device, m_context.getBinaryCollection().get("indirect_dispatch_" + m_name + "_generate"), 0u));

	// Create descriptorSetLayout
	m_descriptorSetLayout = vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vkdi, m_device);

	// Create compute pipeline
	m_pipelineLayout = makePipelineLayout(vkdi, m_device, *m_descriptorSetLayout);
	m_computePipeline = makeComputePipeline(vkdi, m_device, *m_pipelineLayout, *genIndirectBufferDataShader);

	// Create descriptor pool
	m_descriptorPool = vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vkdi, m_device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Create descriptor set
	m_descriptorSet = makeDescriptorSet(vkdi, m_device, *m_descriptorPool, *m_descriptorSetLayout);

	const vk::VkDescriptorBufferInfo indirectDescriptorInfo = makeDescriptorBufferInfo(*indirectBuffer, 0ull, m_bufferSize);

	vk::DescriptorSetUpdateBuilder	descriptorSetBuilder;
	descriptorSetBuilder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indirectDescriptorInfo);
	descriptorSetBuilder.update(vkdi, m_device);

	const vk::VkBufferMemoryBarrier bufferBarrier = makeBufferMemoryBarrier(
		vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_INDIRECT_COMMAND_READ_BIT, *indirectBuffer, 0ull, m_bufferSize);

	// Bind compute pipeline
	vkdi.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);

	// Bind descriptor set
	vkdi.cmdBindDescriptorSets(commandBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &m_descriptorSet.get(), 0u, DE_NULL);

	// Dispatch compute command
	vkdi.cmdDispatch(commandBuffer, 1u, 1u, 1u);

	// Insert memory barrier
	vkdi.cmdPipelineBarrier(commandBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, (vk::VkDependencyFlags)0,
										  0, (const vk::VkMemoryBarrier*)DE_NULL,
										  1, &bufferBarrier,
										  0, (const vk::VkImageMemoryBarrier*)DE_NULL);
}

class IndirectDispatchCaseBufferGenerate : public IndirectDispatchCaseBufferUpload
{
public:
							IndirectDispatchCaseBufferGenerate	(tcu::TestContext&			testCtx,
																 const DispatchCaseDesc&	caseDesc,
																 const glu::GLSLVersion		glslVersion,
																 const vk::ComputePipelineConstructionType computePipelineConstructionType)
								: IndirectDispatchCaseBufferUpload(testCtx, caseDesc, glslVersion, computePipelineConstructionType) {}

	virtual					~IndirectDispatchCaseBufferGenerate	(void) {}

	virtual void			initPrograms						(vk::SourceCollections&		programCollection) const;
	virtual TestInstance*	createInstance						(Context&					context) const;

private:
	IndirectDispatchCaseBufferGenerate (const vkt::TestCase&);
	IndirectDispatchCaseBufferGenerate& operator= (const vkt::TestCase&);
};

void IndirectDispatchCaseBufferGenerate::initPrograms (vk::SourceCollections& programCollection) const
{
	IndirectDispatchCaseBufferUpload::initPrograms(programCollection);

	const char* const	versionDecl = glu::getGLSLVersionDeclaration(m_glslVersion);

	std::ostringstream computeBuffer;

	// Header
	computeBuffer
		<< versionDecl << "\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout(set = 0, binding = 0, std430) buffer Out\n"
		<< "{\n"
		<< "	highp uint data[];\n"
		<< "};\n"
		<< "void writeCmd (uint offset, uvec3 numWorkGroups)\n"
		<< "{\n"
		<< "	data[offset+0u] = numWorkGroups.x;\n"
		<< "	data[offset+1u] = numWorkGroups.y;\n"
		<< "	data[offset+2u] = numWorkGroups.z;\n"
		<< "}\n"
		<< "void main (void)\n"
		<< "{\n";

	// Dispatch commands
	for (DispatchCommandsVec::const_iterator cmdIter = m_dispatchCommands.begin(); cmdIter != m_dispatchCommands.end(); ++cmdIter)
	{
		const deUint32 offs = (deUint32)(cmdIter->m_offset / sizeof(deUint32));
		DE_ASSERT((size_t)offs * sizeof(deUint32) == (size_t)cmdIter->m_offset);

		computeBuffer
			<< "\twriteCmd(" << offs << "u, uvec3("
			<< cmdIter->m_numWorkGroups.x() << "u, "
			<< cmdIter->m_numWorkGroups.y() << "u, "
			<< cmdIter->m_numWorkGroups.z() << "u));\n";
	}

	// Ending
	computeBuffer << "}\n";

	std::string computeString = computeBuffer.str();

	programCollection.glslSources.add("indirect_dispatch_" + m_name + "_generate") << glu::ComputeSource(computeString);
}

TestInstance* IndirectDispatchCaseBufferGenerate::createInstance (Context& context) const
{
	return new IndirectDispatchInstanceBufferGenerate(context, m_name, m_bufferSize, m_workGroupSize, m_dispatchCommands, m_computeOnlyQueue, m_computePipelineConstructionType);
}

DispatchCommandsVec commandsVec (const DispatchCommand& cmd)
{
	DispatchCommandsVec vec;
	vec.push_back(cmd);
	return vec;
}

DispatchCommandsVec commandsVec (const DispatchCommand& cmd0,
								 const DispatchCommand& cmd1,
								 const DispatchCommand& cmd2,
								 const DispatchCommand& cmd3,
								 const DispatchCommand& cmd4)
{
	DispatchCommandsVec vec;
	vec.push_back(cmd0);
	vec.push_back(cmd1);
	vec.push_back(cmd2);
	vec.push_back(cmd3);
	vec.push_back(cmd4);
	return vec;
}

DispatchCommandsVec commandsVec (const DispatchCommand& cmd0,
								 const DispatchCommand& cmd1,
								 const DispatchCommand& cmd2,
								 const DispatchCommand& cmd3,
								 const DispatchCommand& cmd4,
								 const DispatchCommand& cmd5,
								 const DispatchCommand& cmd6)
{
	DispatchCommandsVec vec;
	vec.push_back(cmd0);
	vec.push_back(cmd1);
	vec.push_back(cmd2);
	vec.push_back(cmd3);
	vec.push_back(cmd4);
	vec.push_back(cmd5);
	vec.push_back(cmd6);
	return vec;
}

} // anonymous ns

tcu::TestCaseGroup* createIndirectComputeDispatchTests (tcu::TestContext& testCtx, vk::ComputePipelineConstructionType computePipelineConstructionType)
{

	static const DispatchCaseDesc s_dispatchCases[] =
	{
		DispatchCaseDesc("single_invocation", "Single invocation only from offset 0", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
			commandsVec(DispatchCommand(0, tcu::UVec3(1, 1, 1))), false
		),
		DispatchCaseDesc("multiple_groups", "Multiple groups dispatched from offset 0", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
			commandsVec(DispatchCommand(0, tcu::UVec3(2, 3, 5))), false
		),
		DispatchCaseDesc("multiple_groups_multiple_invocations", "Multiple groups of size 2x3x1 from offset 0", INDIRECT_COMMAND_OFFSET, tcu::UVec3(2, 3, 1),
			commandsVec(DispatchCommand(0, tcu::UVec3(1, 2, 3))), false
		),
		DispatchCaseDesc("small_offset", "Small offset", 16 + INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
			commandsVec(DispatchCommand(16, tcu::UVec3(1, 1, 1))), false
		),
		DispatchCaseDesc("large_offset", "Large offset", (2 << 20), tcu::UVec3(1, 1, 1),
			commandsVec(DispatchCommand((1 << 20) + 12, tcu::UVec3(1, 1, 1))), false
		),
		DispatchCaseDesc("large_offset_multiple_invocations", "Large offset, multiple invocations", (2 << 20), tcu::UVec3(2, 3, 1),
			commandsVec(DispatchCommand((1 << 20) + 12, tcu::UVec3(1, 2, 3))), false
		),
		DispatchCaseDesc("empty_command", "Empty command", INDIRECT_COMMAND_OFFSET, tcu::UVec3(1, 1, 1),
			commandsVec(DispatchCommand(0, tcu::UVec3(0, 0, 0))), false
		),
		DispatchCaseDesc("multi_dispatch", "Dispatch multiple compute commands from single buffer", 1 << 10, tcu::UVec3(3, 1, 2),
			commandsVec(DispatchCommand(0, tcu::UVec3(1, 1, 1)),
						DispatchCommand(INDIRECT_COMMAND_OFFSET, tcu::UVec3(2, 1, 1)),
						DispatchCommand(104, tcu::UVec3(1, 3, 1)),
						DispatchCommand(40, tcu::UVec3(1, 1, 7)),
						DispatchCommand(52, tcu::UVec3(1, 1, 4))), false
		),
		DispatchCaseDesc("multi_dispatch_reuse_command", "Dispatch multiple compute commands from single buffer", 1 << 10, tcu::UVec3(3, 1, 2),
			commandsVec(DispatchCommand(0, tcu::UVec3(1, 1, 1)),
						DispatchCommand(0, tcu::UVec3(1, 1, 1)),
						DispatchCommand(0, tcu::UVec3(1, 1, 1)),
						DispatchCommand(104, tcu::UVec3(1, 3, 1)),
						DispatchCommand(104, tcu::UVec3(1, 3, 1)),
						DispatchCommand(52, tcu::UVec3(1, 1, 4)),
						DispatchCommand(52, tcu::UVec3(1, 1, 4))), false
		),
	};

	de::MovePtr<tcu::TestCaseGroup> indirectComputeDispatchTests(new tcu::TestCaseGroup(testCtx, "indirect_dispatch", "Indirect dispatch tests"));

	tcu::TestCaseGroup* const	groupBufferUpload = new tcu::TestCaseGroup(testCtx, "upload_buffer", "");
	indirectComputeDispatchTests->addChild(groupBufferUpload);

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_dispatchCases); ndx++)
	{
		DispatchCaseDesc desc = s_dispatchCases[ndx];
		std::string computeName = std::string(desc.m_name) + std::string("_compute_only_queue");
		DispatchCaseDesc computeOnlyDesc = DispatchCaseDesc(computeName.c_str(), desc.m_description, desc.m_bufferSize, desc.m_workGroupSize,
															desc.m_dispatchCommands, true);
		groupBufferUpload->addChild(new IndirectDispatchCaseBufferUpload(testCtx, desc, glu::GLSL_VERSION_310_ES, computePipelineConstructionType));
		groupBufferUpload->addChild(new IndirectDispatchCaseBufferUpload(testCtx, computeOnlyDesc, glu::GLSL_VERSION_310_ES, computePipelineConstructionType));
	}

	tcu::TestCaseGroup* const	groupBufferGenerate = new tcu::TestCaseGroup(testCtx, "gen_in_compute", "");
	indirectComputeDispatchTests->addChild(groupBufferGenerate);

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_dispatchCases); ndx++)
	{
		DispatchCaseDesc desc = s_dispatchCases[ndx];
		std::string computeName = std::string(desc.m_name) + std::string("_compute_only_queue");
		DispatchCaseDesc computeOnlyDesc = DispatchCaseDesc(computeName.c_str(), desc.m_description, desc.m_bufferSize, desc.m_workGroupSize,
															desc.m_dispatchCommands, true);
		groupBufferGenerate->addChild(new IndirectDispatchCaseBufferGenerate(testCtx, desc, glu::GLSL_VERSION_310_ES, computePipelineConstructionType));
		groupBufferGenerate->addChild(new IndirectDispatchCaseBufferGenerate(testCtx, computeOnlyDesc, glu::GLSL_VERSION_310_ES, computePipelineConstructionType));
	}

	return indirectComputeDispatchTests.release();
}

} // compute
} // vkt
