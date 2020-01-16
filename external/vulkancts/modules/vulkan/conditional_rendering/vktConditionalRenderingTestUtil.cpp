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
	if (!context.isDeviceFunctionalitySupported("VK_EXT_conditional_rendering"))
		TCU_THROW(NotSupportedError, "Missing extension: VK_EXT_conditional_rendering");

	if (data.conditionInherited)
	{
		const vk::VkPhysicalDeviceConditionalRenderingFeaturesEXT& conditionalRenderingFeatures = context.getConditionalRenderingFeaturesEXT();
		if (!conditionalRenderingFeatures.inheritedConditionalRendering)
		{
			TCU_THROW(NotSupportedError, "Device does not support inherited conditional rendering");
		}
	}
}

de::SharedPtr<Draw::Buffer>	createConditionalRenderingBuffer (vkt::Context& context, const ConditionalData& data)
{
	const vk::DeviceInterface& vk = context.getDeviceInterface();
	de::SharedPtr<Draw::Buffer> buffer = Draw::Buffer::createAndAlloc(vk, context.getDevice(),
											Draw::BufferCreateInfo(sizeof(deUint32),
															 vk::VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT),
											context.getDefaultAllocator(),
											vk::MemoryRequirement::HostVisible);

	deUint8* conditionBufferPtr = reinterpret_cast<deUint8*>(buffer->getBoundMemory().getHostPtr());
	*(deUint32*)(conditionBufferPtr) = data.conditionValue;

	vk::flushMappedMemoryRange(	vk,
								context.getDevice(),
								buffer->getBoundMemory().getMemory(),
								buffer->getBoundMemory().getOffset(),
								VK_WHOLE_SIZE);
	return buffer;
}

void beginConditionalRendering (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, Draw::Buffer& buffer, const ConditionalData& data)
{
	vk::VkConditionalRenderingBeginInfoEXT conditionalRenderingBeginInfo;
	conditionalRenderingBeginInfo.sType = vk::VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
	conditionalRenderingBeginInfo.pNext = DE_NULL;
	conditionalRenderingBeginInfo.buffer = buffer.object();
	conditionalRenderingBeginInfo.offset = 0;
	conditionalRenderingBeginInfo.flags = data.conditionInverted ? vk::VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0;

	vk.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBeginInfo);
}

std::ostream& operator<< (std::ostream& str, ConditionalData const& c)
{
	const bool conditionEnabled = c.conditionInPrimaryCommandBuffer || c.conditionInSecondaryCommandBuffer;
	str << (conditionEnabled ? "condition" : "no_condition");

	if (c.conditionInSecondaryCommandBuffer || !conditionEnabled)
	{
		str << "_secondary_buffer";
	}
	else if (c.conditionInherited)
	{
		str << "_inherited";
	}

	str << "_" << (c.expectCommandExecution ? "expect_execution" : "expect_noop");

	if (c.conditionInverted)
	{
		str << "_inverted";
	}

	return str;
}

}   // conditional
}	// vkt