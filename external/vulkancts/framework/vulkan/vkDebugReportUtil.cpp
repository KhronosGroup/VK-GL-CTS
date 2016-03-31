/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief VK_EXT_debug_report utilities
 *//*--------------------------------------------------------------------*/

#include "vkDebugReportUtil.hpp"
#include "vkRefUtil.hpp"
#include "deArrayUtil.hpp"

namespace vk
{

namespace
{

tcu::Format::Bitfield<32> shortDebugFlagsStr (VkDebugReportFlagsEXT flags)
{
	static const tcu::Format::BitDesc	s_bits[] =
	{
		tcu::Format::BitDesc(VK_DEBUG_REPORT_INFORMATION_BIT_EXT,			"INFO"),
		tcu::Format::BitDesc(VK_DEBUG_REPORT_WARNING_BIT_EXT,				"WARNING"),
		tcu::Format::BitDesc(VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,	"PERFORMANCE"),
		tcu::Format::BitDesc(VK_DEBUG_REPORT_ERROR_BIT_EXT,					"ERROR"),
		tcu::Format::BitDesc(VK_DEBUG_REPORT_DEBUG_BIT_EXT,					"DEBUG"),
	};

	return tcu::Format::Bitfield<32>(flags, DE_ARRAY_BEGIN(s_bits), DE_ARRAY_END(s_bits));
}

const char* getShortObjectTypeName (VkDebugReportObjectTypeEXT objectType)
{
	static const char* const s_names[] =
	{
		"Unknown",
		"Instance",
		"PhysicalDevice",
		"Device",
		"Queue",
		"Semaphore",
		"CommandBuffer",
		"Fence",
		"DeviceMemory",
		"Buffer",
		"Image",
		"Event",
		"QueryPool",
		"BufferView",
		"ImageView",
		"ShaderModule",
		"PipelineCache",
		"PipelineLayout",
		"RenderPass",
		"Pipeline",
		"DescriptorSetLayout",
		"Sampler",
		"DescriptorPool",
		"DescriptorSet",
		"Framebuffer",
		"CommandPool",
		"SurfaceKHR",
		"SwapchainKHR",
		"DebugReportCallbackEXT",
	};
	return de::getSizedArrayElement<VK_DEBUG_REPORT_OBJECT_TYPE_EXT_LAST>(s_names, objectType);
}

tcu::Format::Enum<VkDebugReportObjectTypeEXT> shortObjectTypeStr (VkDebugReportObjectTypeEXT objectType)
{
	return tcu::Format::Enum<VkDebugReportObjectTypeEXT>(getShortObjectTypeName, objectType);
}

} // anonymous

std::ostream& operator<< (std::ostream& str, const DebugReportMessage& message)
{
	str << shortDebugFlagsStr(message.flags) << ": "
		<< message.message
		<< " (code " << tcu::toHex(message.messageCode);

	if (message.layerPrefix.empty())
		str << " from " << message.layerPrefix;

	str << " at " << shortObjectTypeStr(message.objectType) << ":" << message.location << ")";

	return str;
}

namespace
{

VKAPI_ATTR VkBool32	VKAPI_CALL debugReportCallback (VkDebugReportFlagsEXT		flags,
													VkDebugReportObjectTypeEXT	objectType,
													deUint64					object,
													size_t						location,
													deInt32						messageCode,
													const char*					pLayerPrefix,
													const char*					pMessage,
													void*						pUserData)
{
	DebugReportRecorder::MessageList* const	messageList	= reinterpret_cast<DebugReportRecorder::MessageList*>(pUserData);

	messageList->append(DebugReportMessage(flags, objectType, object, location, messageCode, pLayerPrefix, pMessage));

	return VK_TRUE;
}

Move<VkDebugReportCallbackEXT> createCallback (const InstanceInterface&				vki,
											   VkInstance							instance,
											   DebugReportRecorder::MessageList*	messageList)
{
	const VkDebugReportFlagsEXT					allFlags	= VK_DEBUG_REPORT_INFORMATION_BIT_EXT
															| VK_DEBUG_REPORT_WARNING_BIT_EXT
															| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
															| VK_DEBUG_REPORT_ERROR_BIT_EXT
															| VK_DEBUG_REPORT_DEBUG_BIT_EXT;

	const VkDebugReportCallbackCreateInfoEXT	createInfo	=
	{
		VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		DE_NULL,
		allFlags,
		debugReportCallback,
		messageList
	};

	return createDebugReportCallbackEXT(vki, instance, &createInfo);
}

} // anonymous

DebugReportRecorder::DebugReportRecorder (const InstanceInterface& vki, VkInstance instance)
	: m_messages	(1024)
	, m_callback	(createCallback(vki, instance, &m_messages))
{
}

DebugReportRecorder::~DebugReportRecorder (void)
{
}

} // vk
