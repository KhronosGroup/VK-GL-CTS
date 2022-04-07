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

de::SharedPtr<Draw::Buffer>	createConditionalRenderingBuffer (vkt::Context& context, const ConditionalData& data, vk::VkCommandPool cmdPool)
{
	// When padding the condition value, it will be surounded by two additional values with nonzero bytes in them.
	const auto					bufferSize	= static_cast<vk::VkDeviceSize>(sizeof(data.conditionValue)) * (data.padConditionValue ? 3ull : 1ull);
	const auto					dataOffset	= static_cast<vk::VkDeviceSize>(data.padConditionValue ? sizeof(data.conditionValue) : 0);
	const auto					usage		= data.memoryType ? vk::VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT : vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	const vk::DeviceInterface&	vk			= context.getDeviceInterface();
	de::SharedPtr<Draw::Buffer>	buffer		= Draw::Buffer::createAndAlloc(vk, context.getDevice(),
												Draw::BufferCreateInfo(bufferSize, usage),
												context.getDefaultAllocator(),
												vk::MemoryRequirement::HostVisible);

	deUint8* conditionBufferPtr = reinterpret_cast<deUint8*>(buffer->getBoundMemory().getHostPtr()) + buffer->getBoundMemory().getOffset();
	deMemset(conditionBufferPtr, 1, static_cast<size_t>(bufferSize));
	deMemcpy(conditionBufferPtr + dataOffset, &data.conditionValue, sizeof(data.conditionValue));

	vk::flushMappedMemoryRange(	vk,
								context.getDevice(),
								buffer->getBoundMemory().getMemory(),
								buffer->getBoundMemory().getOffset(),
								VK_WHOLE_SIZE);

	if (data.memoryType == ConditionalBufferMemory::LOCAL)
	{
		de::SharedPtr<Draw::Buffer>	conditionalBuffer = Draw::Buffer::createAndAlloc(vk, context.getDevice(),
												Draw::BufferCreateInfo(bufferSize,
																 vk::VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT
																|vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
												context.getDefaultAllocator(),
												vk::MemoryRequirement::Local);

		auto cmdBuffer = vk::allocateCommandBuffer
		(
			vk, context.getDevice(),
			cmdPool,
			vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY
		);

		const vk::VkCommandBufferBeginInfo commandBufferBeginInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			DE_NULL,
			vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			nullptr
		};

		vk.beginCommandBuffer(*cmdBuffer, &commandBufferBeginInfo);

		vk::VkBufferCopy copyInfo
		{
			buffer->getBoundMemory().getOffset(),
			conditionalBuffer->getBoundMemory().getOffset(),
			static_cast<size_t>(bufferSize)
		};
		vk.cmdCopyBuffer(*cmdBuffer, buffer->object(), conditionalBuffer->object(), 1, &copyInfo);
		vk.endCommandBuffer(*cmdBuffer);

		vk::VkSubmitInfo submitInfo{};
		submitInfo.sType = vk::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &(*cmdBuffer);

		auto queue = context.getUniversalQueue();

		vk.queueSubmit(queue, 1, &submitInfo, 0);

		vk.queueWaitIdle(queue);

		return conditionalBuffer;
	}

	return buffer;
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

	return str;
}

}   // conditional
}	// vkt
