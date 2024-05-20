/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief VK_EXT_debug_utils utilities
 *//*--------------------------------------------------------------------*/

#include "vkDebugReportUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "deArrayUtil.hpp"
#include "tcuDefs.hpp"

namespace vk
{

#ifndef CTS_USES_VULKANSC

namespace
{

const char* getSeverityStr	(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
	switch (severity)
	{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:	return "VERBOSE";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:		return "INFO";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:	return "WARNING";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:		return "ERROR";
		default:												return "UNKNOWN";
	}
}

const char* getMessageTypeStr	(VkDebugUtilsMessageTypeFlagsEXT type)
{
	switch (type)
	{
		case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:					return "GENERAL";
		case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:				return "VALIDATION";
		case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:				return "PERFORMANCE";
		case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:	return "DEVICE_ADDRESS_BINDING";
		default:															return "UNKNOWN";
	}
}

constexpr const char* kIgnoredMessages[] =
{
	// TODO: fill with currently known errors.
	"VUID-example-12345",
};

// Skip logging messages for known issues.
bool ignoreDebugMessage(const DebugUtilsMessage& message)
{
	for (size_t i = 0; i < std::size(kIgnoredMessages); ++i)
	{
		if (message.vuid == kIgnoredMessages[i])
		{
			return true;
		}
	}
	return false;
}
} // anonymous

std::ostream& operator<< (std::ostream& str, const DebugUtilsMessage& message)
{
	str << getSeverityStr(message.severity) << "|" << getMessageTypeStr(message.type) << ": ["
		<< message.vuid << "] " << message.message;

	return str;
}

namespace
{

VKAPI_ATTR VkBool32	VKAPI_CALL debugUtilsCallback (VkDebugUtilsMessageSeverityFlagBitsEXT		severity,
													VkDebugUtilsMessageTypeFlagsEXT				type,
													const VkDebugUtilsMessengerCallbackDataEXT*	callbackData,
													void*										userData)
{
	auto						recorder	= reinterpret_cast<DebugReportRecorder*>(userData);
	auto&						messageList	= recorder->getMessages();
	const DebugUtilsMessage	message		(severity, type, callbackData->pMessageIdName, callbackData->pMessage);

	// Skip logging messages for known issues.
	if (ignoreDebugMessage(message))
	{
		return VK_FALSE;
	}

	messageList.append(message);

	if (recorder->errorPrinting() && message.isError())
		tcu::printError("%s\n", callbackData->pMessage);

	// Return false to indicate that the call should not return error and should
	// continue execution normally.
	return VK_FALSE;
}

} // anonymous

DebugReportRecorder::DebugReportRecorder (bool printValidationErrors)
	: m_messages		(1024)
	, m_print_errors	(printValidationErrors)
{
}

DebugReportRecorder::~DebugReportRecorder (void)
{
}

VkDebugUtilsMessengerCreateInfoEXT DebugReportRecorder::makeCreateInfo (void)
{
	const VkDebugUtilsMessageSeverityFlagsEXT	severity	= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
															| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	const VkDebugUtilsMessageTypeFlagsEXT		types		= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
															| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
															| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	const VkDebugUtilsMessengerCreateInfoEXT	createInfo	=
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		nullptr,
		0,
		severity,
		types,
		debugUtilsCallback,
		this,
	};

	return createInfo;
}

Move<VkDebugUtilsMessengerEXT> DebugReportRecorder::createCallback (const InstanceInterface& vki, VkInstance instance)
{
	const auto createInfo = makeCreateInfo();
	return createDebugUtilsMessengerEXT(vki, instance, &createInfo);
}

#endif // CTS_USES_VULKANSC

bool isDebugUtilsSupported (const PlatformInterface& vkp)
{
#ifndef CTS_USES_VULKANSC
	return isExtensionStructSupported(enumerateInstanceExtensionProperties(vkp, DE_NULL),
								RequiredExtension("VK_EXT_debug_utils"));
#else // CTS_USES_VULKANSC
	DE_UNREF(vkp);
	return false;
#endif // CTS_USES_VULKANSC
}

} // vk
