#ifndef _VKDEBUGREPORTUTIL_HPP
#define _VKDEBUGREPORTUTIL_HPP
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

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "deAppendList.hpp"

#include <ostream>

namespace vk
{

#ifndef CTS_USES_VULKANSC

struct DebugUtilsMessage
{
	VkDebugUtilsMessageSeverityFlagBitsEXT		severity;
	VkDebugUtilsMessageTypeFlagsEXT				type;
	std::string									vuid;
	std::string									message;

	DebugUtilsMessage (void)
		: severity	{}
		, type		{}
	{}

	DebugUtilsMessage (VkDebugUtilsMessageSeverityFlagBitsEXT		severity_,
						VkDebugUtilsMessageTypeFlagsEXT				type_,
						std::string									vuid_,
						std::string									message_)
		: severity	(severity_)
		, type		(type_)
		, vuid		(vuid_)
		, message	(message_)
	{}

	bool isError	() const
	{
		static const vk::VkDebugUtilsMessageSeverityFlagsEXT errorFlags = vk::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		return (severity & errorFlags) != 0u;
	}

	bool shouldBeLogged	() const
	{
		// \note We are not logging INFORMATION and DEBUG messages. They are diabled when creating
		// the debug utils messenger.
		return true;
	}
};

std::ostream& operator<< (std::ostream& str, const DebugUtilsMessage& message);

class DebugReportRecorder
{
public:
	using MessageList = de::AppendList<DebugUtilsMessage>;

											DebugReportRecorder		(bool printValidationErrors);
											~DebugReportRecorder	(void);

	MessageList&							getMessages				(void) { return m_messages; }
	void									clearMessages			(void) { m_messages.clear(); }
	bool									errorPrinting			(void) const { return m_print_errors; }

	VkDebugUtilsMessengerCreateInfoEXT		makeCreateInfo			(void);
	Move<VkDebugUtilsMessengerEXT>			createCallback			(const InstanceInterface& vki, VkInstance instance);

private:
	MessageList								m_messages;
	const bool								m_print_errors;
};

#endif // CTS_USES_VULKANSC

bool isDebugReportSupported (const PlatformInterface& vkp);

} // vk

#endif // _VKDEBUGREPORTUTIL_HPP
