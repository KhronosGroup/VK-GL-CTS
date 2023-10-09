/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Device loss tests.
 *//*--------------------------------------------------------------------*/

#include "vktPostmortemDeviceLossTests.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vktTestCase.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include <functional>
#include <vector>

namespace vkt
{
namespace postmortem
{
namespace
{
using namespace vk;
using namespace tcu;

Move<VkDevice> createPostmortemDevice(Context& context)
{
	const float queuePriority = 1.0f;

	// Create a universal queue that supports graphics and compute
	const VkDeviceQueueCreateInfo queueParams
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		0u,											// VkDeviceQueueCreateFlags		flags;
		context.getUniversalQueueFamilyIndex(),		// deUint32						queueFamilyIndex;
		1u,											// deUint32						queueCount;
		&queuePriority								// const float*					pQueuePriorities;
	};

	std::vector<const char*>					extensionPtrs				= { "VK_KHR_maintenance5" };
	VkPhysicalDeviceTimelineSemaphoreFeatures	timelineSemaphoreFeatures	= initVulkanStructure();
	VkPhysicalDeviceFeatures2					features2					= initVulkanStructure();
	const auto									addFeatures					= makeStructChainAdder(&features2);

	deMemset(&features2.features, 0, sizeof(VkPhysicalDeviceFeatures));
	if (context.getDeviceFeatures().pipelineStatisticsQuery)
		features2.features.pipelineStatisticsQuery = 1;

	if (context.isDeviceFunctionalitySupported("VK_KHR_timeline_semaphore"))
	{
		extensionPtrs.push_back("VK_KHR_timeline_semaphore");
		timelineSemaphoreFeatures.timelineSemaphore = 1;
		addFeatures(&timelineSemaphoreFeatures);
	}

	const VkDeviceCreateInfo deviceParams
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	// VkStructureType					sType;
		&features2,								// const void*						pNext;
		0u,										// VkDeviceCreateFlags				flags;
		1u,										// deUint32							queueCreateInfoCount;
		&queueParams,							// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,										// deUint32							enabledLayerCount;
		DE_NULL,								// const char* const*				ppEnabledLayerNames;
		(deUint32)extensionPtrs.size(),			// deUint32							enabledExtensionCount;
		extensionPtrs.data(),					// const char* const*				ppEnabledExtensionNames;
		DE_NULL									// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(),
							  context.getInstance(), context.getInstanceInterface(), context.getPhysicalDevice(), &deviceParams);
}

class DeviceLossInstance : public TestInstance
{
public:
							DeviceLossInstance	(Context& context)
								: TestInstance	(context) {}
	virtual					~DeviceLossInstance	() =  default;

	virtual TestStatus		iterate				(void) override;
};

TestStatus DeviceLossInstance::iterate (void)
{
	vk::Unique<vk::VkDevice>	logicalDevice		(createPostmortemDevice(m_context));
	vk::DeviceDriver			deviceDriver		(m_context.getPlatformInterface(), m_context.getInstance(), *logicalDevice, m_context.getUsedApiVersion());
	deUint32					queueFamilyIndex	(0);
	vk::VkQueue					queue				(getDeviceQueue(deviceDriver, *logicalDevice, queueFamilyIndex, 0));
	vk::SimpleAllocator			allocator			(deviceDriver, *logicalDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	// create query pool
	const VkQueryPoolCreateInfo queryPoolInfo
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,						// VkStructureType					sType
		DE_NULL,														// const void*						pNext
		(VkQueryPoolCreateFlags)0,										// VkQueryPoolCreateFlags			flags
		VK_QUERY_TYPE_PIPELINE_STATISTICS ,								// VkQueryType						queryType
		1u,																// deUint32							entryCount
		VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT,		// VkQueryPipelineStatisticFlags	pipelineStatistics
	};
	Move<VkQueryPool> queryPool;
	const bool usePipelineStatisticsQuery = m_context.getDeviceFeatures().pipelineStatisticsQuery;
	if (usePipelineStatisticsQuery)
		queryPool = createQueryPool(deviceDriver, *logicalDevice, &queryPoolInfo);

	// create output buffer
	const auto outBufferInfo = makeBufferCreateInfo(sizeof(deUint32), (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	de::MovePtr<BufferWithMemory> outBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(deviceDriver, *logicalDevice, allocator, outBufferInfo, MemoryRequirement::HostVisible));

	// create descriptor set layout
	auto descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(deviceDriver, *logicalDevice);

	// create descriptor pool
	auto descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(deviceDriver, *logicalDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// create and update descriptor set
	const VkDescriptorSetAllocateInfo allocInfo
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		*descriptorPool,
		1u,
		&(*descriptorSetLayout)
	};
	auto descriptorSet = allocateDescriptorSet(deviceDriver, *logicalDevice, &allocInfo);
	const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(**outBuffer, (VkDeviceSize)0u, sizeof(deUint32));
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(deviceDriver, *logicalDevice);

	// create compute pipeline
	const Unique<VkShaderModule>	shaderModule		(createShaderModule(deviceDriver, *logicalDevice, m_context.getBinaryCollection().get("comp"), 0u));
	const VkPushConstantRange		pushConstantRange	{ VK_SHADER_STAGE_COMPUTE_BIT, 0u, 2 * sizeof(deUint32) };
	const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout(deviceDriver, *logicalDevice, 1u, &(*descriptorSetLayout), 1, &pushConstantRange));
	const Unique<VkPipeline>		pipeline			(makeComputePipeline(deviceDriver, *logicalDevice, *pipelineLayout, *shaderModule));

	// create command buffer
	const Unique<VkCommandPool>		cmdPool			(makeCommandPool(deviceDriver, *logicalDevice, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer		(allocateCommandBuffer(deviceDriver, *logicalDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const deUint32					pushConstant[]	{ 4u, 0u };

	beginCommandBuffer(deviceDriver, *cmdBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
	if (usePipelineStatisticsQuery)
		deviceDriver.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);
	deviceDriver.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	if (usePipelineStatisticsQuery)
		deviceDriver.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
	deviceDriver.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, 0);
	deviceDriver.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstant), &pushConstant);
	deviceDriver.cmdDispatch(*cmdBuffer, 1, 1, 1);
	if (usePipelineStatisticsQuery)
		deviceDriver.cmdEndQuery(*cmdBuffer, *queryPool, 0u);
	endCommandBuffer(deviceDriver, *cmdBuffer);

	const deUint64		waitValue	(0);
	deUint64			waitTimeout	(5000000000ull);
	deUint64			queryResult	(0);
	const Move<VkFence> fence[2]
	{
		createFence(deviceDriver, *logicalDevice),
		createFence(deviceDriver, *logicalDevice)
	};
	const Move<VkEvent> event[2]
	{
		createEvent(deviceDriver, *logicalDevice),
		createEvent(deviceDriver, *logicalDevice)
	};

	Move<VkSemaphore> semaphore[2];
	if (m_context.isDeviceFunctionalitySupported("VK_KHR_timeline_semaphore"))
	{
		semaphore[0] = createSemaphoreType(deviceDriver, *logicalDevice, VK_SEMAPHORE_TYPE_TIMELINE);
		semaphore[1] = createSemaphoreType(deviceDriver, *logicalDevice, VK_SEMAPHORE_TYPE_TIMELINE);
	}

	VkSemaphoreWaitInfo waitInfo
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,	// VkStructureType				sType
		DE_NULL,								// const void*					pNext
		VK_SEMAPHORE_WAIT_ANY_BIT,				// VkSemaphoreWaitFlags			flags;
		1u,										// deUint32						semaphoreCount;
		&*semaphore[0],							// const VkSemaphore*			pSemaphores;
		&waitValue								// const deUint64*				pValues;
	};

	const VkSubmitInfo submitInfo
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,			// VkStructureType				sType;
		DE_NULL,								// const void*					pNext;
		0u,										// deUint32						waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*			pWaitSemaphores;
		DE_NULL,								// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,										// deUint32						commandBufferCount;
		&*cmdBuffer,							// const VkCommandBuffer*		pCommandBuffers;
		0u,										// deUint32						signalSemaphoreCount;
		DE_NULL,								// const VkSemaphore*			pSignalSemaphores;
	};

	// create vector containing lambdas with all functions that we need to check;
	// this will simplify testing code by allowing us to check those functions within a loop;
	// note that order of functions is important; we cant break any VUIDs
	deUint32 handleIndex = 0;
	std::vector< std::pair<std::string, std::function<VkResult () > > > functionsToCheck
	{
		{
			"queueSubmit",
			[&]() {
				return deviceDriver.queueSubmit(queue, 1u, &submitInfo, *fence[handleIndex]);
			}
		},
		{
			"waitSemaphores",
			[&]() {
				if (*semaphore[handleIndex] == 0)
					return VK_RESULT_MAX_ENUM;
				waitInfo.pSemaphores = &*semaphore[handleIndex];
				return deviceDriver.waitSemaphores(*logicalDevice, &waitInfo, waitTimeout);
			}
		},
		{
			"getEventStatus",
			[&]() {
				return deviceDriver.getEventStatus(*logicalDevice, *event[handleIndex]);
			}
		},
		{
			"waitForFences",
			[&]() {
				return deviceDriver.waitForFences(*logicalDevice, 1u, &(*fence[handleIndex]), DE_TRUE, waitTimeout);
			}
		},
		{
			"getFenceStatus",
			[&]() {
				return deviceDriver.getFenceStatus(*logicalDevice, *fence[handleIndex]);
			}
		},
		{
			"deviceWaitIdle",
			[&]() {
				return deviceDriver.deviceWaitIdle(*logicalDevice);
			}
		},
		{
			"getQueryPoolResults",
			[&]() {
				if (usePipelineStatisticsQuery)
					return deviceDriver.getQueryPoolResults(*logicalDevice, *queryPool, 0u, 1u, sizeof(queryResult), &queryResult, 0u, 0u);
				return VK_RESULT_MAX_ENUM;
			}
		}
	};

	// call all functions untill one returns VK_ERROR_DEVICE_LOST
	bool deviceWasLost = 0;
	for (const auto& funPair : functionsToCheck)
	{
		VkResult result = funPair.second();

		deviceWasLost = (result == VK_ERROR_DEVICE_LOST);
		if (deviceWasLost)
			break;

		if (result == VK_TIMEOUT)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Timeout exceeded");
	}

	// never returning DEVICE_LOST is fine
	if (!deviceWasLost)
		return TestStatus::pass("DEVICE_LOST was never returned");

	// call all functions once egain and expect all to return VK_ERROR_DEVICE_LOST
	handleIndex = 1;
	for (const auto& funPair : functionsToCheck)
	{
		VkResult result = funPair.second();
		if (result == VK_ERROR_DEVICE_LOST)
			continue;

		// skip waitSemaphores / getQueryPoolResults
		if (result == VK_RESULT_MAX_ENUM)
			continue;

		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, std::string("Wrong VkResult for ") + funPair.first);
	}

	return TestStatus::pass("DEVICE_LOST returned by all functions");
}

class DeviceLossCase : public TestCase
{
public:
							DeviceLossCase		(TestContext&			testCtx,
												 const std::string&		name);
	virtual					~DeviceLossCase		() = default;
	virtual void			checkSupport		(Context&				context) const override;
	virtual void			initPrograms		(SourceCollections&		programCollection) const override;
	virtual TestInstance*	createInstance		(Context&				context) const override;
};

DeviceLossCase::DeviceLossCase (TestContext& testCtx, const std::string& name)
	: TestCase(testCtx, name, "")
{
}

void DeviceLossCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

void DeviceLossCase::initPrograms(vk::SourceCollections& programCollection) const
{
	// create shader with infinite loop to trigger DEVICE_LOST
	programCollection.glslSources.add("comp") << glu::ComputeSource(
		"#version 320 es\n"
		"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1)\n"
		"layout(push_constant) uniform Constants { uvec2 inp; } pc; \n"
		"layout(std430, set = 0, binding = 0) writeonly buffer Data { uint outp[]; } data;\n"
		"void main()\n"
		"{\n"
		"  uint i = pc.inp.x;\n"
		"  while (i > pc.inp.y)\n"
		"  {\n"
		"    i = i + uint(1);\n"
		"    if (i == uint(0))\n"
		"      i = pc.inp.x;\n"
		"  }\n"
		"  data.outp[0] = i;\n"
		"}\n");
}

TestInstance* DeviceLossCase::createInstance(Context& context) const
{
	return new DeviceLossInstance(context);
}

} // unnamed

tcu::TestCaseGroup* createDeviceLossTests(tcu::TestContext& testCtx)
{
	auto rootGroup = new TestCaseGroup(testCtx, "device_loss", "");

	rootGroup->addChild(new DeviceLossCase(testCtx, "maintenance5"));

	return rootGroup;
}

} // postmortem
} // vkt
