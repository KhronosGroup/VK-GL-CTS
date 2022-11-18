/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Conditional Rendering Test Utils
 *//*--------------------------------------------------------------------*/

#include "vktConditionalRenderingTestUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace conditional
{

void checkConditionalRenderingCapabilities (vkt::Context& context, const ConditionalData& data)
{
	context.requireDeviceFunctionality("VK_EXT_conditional_rendering");

	const auto& conditionalRenderingFeatures = context.getConditionalRenderingFeaturesEXT();

	if (conditionalRenderingFeatures.conditionalRendering == VK_FALSE)
		TCU_FAIL("conditionalRendering feature not supported but VK_EXT_conditional_rendering present");

	if (data.conditionInherited && !conditionalRenderingFeatures.inheritedConditionalRendering)
		TCU_THROW(NotSupportedError, "Device does not support inherited conditional rendering");
}

de::SharedPtr<Draw::Buffer>	createConditionalRenderingBuffer (vkt::Context& context, const ConditionalData& data)
{
	const auto&	vk			= context.getDeviceInterface();
	const auto	device		= context.getDevice();
	const auto	queueIndex	= context.getUniversalQueueFamilyIndex();
	const auto	queue		= context.getUniversalQueue();
	auto&		alloc		= context.getDefaultAllocator();

	// When padding the condition value, it will be surounded by two additional values with nonzero bytes in them.
	// When choosing to apply an offset to the allocation, the offset will be four times the size of the condition variable.
	const auto					bufferSize	= static_cast<vk::VkDeviceSize>(sizeof(data.conditionValue)) * (data.padConditionValue ? 3ull : 1ull);
	const auto					dataOffset	= static_cast<vk::VkDeviceSize>(data.padConditionValue ? sizeof(data.conditionValue) : 0);
	const auto					allocOffset	= static_cast<vk::VkDeviceSize>(sizeof(data.conditionValue) * (data.allocationOffset ? 4u : 0u));

	// Create host-visible buffer. This may be the final buffer or only a staging buffer.
	const auto					hostUsage	= ((data.memoryType == HOST) ? vk::VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT : vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	de::SharedPtr<Draw::Buffer>	hostBuffer	= Draw::Buffer::createAndAlloc(vk, device,
												Draw::BufferCreateInfo(bufferSize, hostUsage),
												alloc,
												vk::MemoryRequirement::HostVisible,
												allocOffset);

	// Copy data to host buffer.
	deUint8* conditionBufferPtr = reinterpret_cast<deUint8*>(hostBuffer->getHostPtr());
	deMemset(conditionBufferPtr, 1, static_cast<size_t>(bufferSize));
	deMemcpy(conditionBufferPtr + dataOffset, &data.conditionValue, sizeof(data.conditionValue));
	vk::flushAlloc(vk, context.getDevice(), hostBuffer->getBoundMemory());

	// Return host buffer if appropriate.
	if (data.memoryType == HOST)
		return hostBuffer;

	// Create and return device-local buffer otherwise, after copying host-visible buffer contents to it.
	const auto					deviceLocalUsage	= (vk::VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::SharedPtr<Draw::Buffer>	deviceLocalBuffer	= Draw::Buffer::createAndAlloc(vk, device,
														Draw::BufferCreateInfo(bufferSize, deviceLocalUsage),
														alloc,
														vk::MemoryRequirement::Local,
														allocOffset);

	const auto cmdPool		= vk::makeCommandPool(vk, device, queueIndex);
	const auto cmdBuffer	= vk::allocateCommandBuffer (vk, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto copyInfo		= vk::makeBufferCopy(0ull, 0ull, bufferSize);

	vk::beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdCopyBuffer(*cmdBuffer, hostBuffer->object(), deviceLocalBuffer->object(), 1, &copyInfo);
	vk::endCommandBuffer(vk, *cmdBuffer);
	vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	return deviceLocalBuffer;
}

void beginConditionalRendering (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, Draw::Buffer& buffer, const ConditionalData& data)
{
	vk::VkConditionalRenderingBeginInfoEXT conditionalRenderingBeginInfo;
	conditionalRenderingBeginInfo.sType		= vk::VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
	conditionalRenderingBeginInfo.pNext		= nullptr;
	conditionalRenderingBeginInfo.buffer	= buffer.object();
	conditionalRenderingBeginInfo.offset	= static_cast<vk::VkDeviceSize>(data.padConditionValue ? sizeof(data.conditionValue) : 0u);
	conditionalRenderingBeginInfo.flags		= data.conditionInverted ? vk::VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0;

	vk.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBeginInfo);
}

std::ostream& operator<< (std::ostream& str, ConditionalData const& c)
{
	const bool conditionEnabled = c.conditionInPrimaryCommandBuffer || c.conditionInSecondaryCommandBuffer;
	str << (conditionEnabled ? "condition" : "no_condition");
	str << (c.memoryType ? "_host_memory" : "_local_memory");


	if (c.conditionInSecondaryCommandBuffer || !conditionEnabled)
	{
		str << "_secondary_buffer";
	}

	if (c.conditionInherited)
	{
		str << "_inherited";
	}

	str << "_" << (c.expectCommandExecution ? "expect_execution" : "expect_noop");

	if (c.conditionInverted)
	{
		str << "_inverted";
	}

	if (c.padConditionValue)
	{
		str << "_padded";
	}

	if (c.clearInRenderPass)
	{
		str << "_rp_clear";
	}

	return str;
}

}   // conditional
}	// vkt
