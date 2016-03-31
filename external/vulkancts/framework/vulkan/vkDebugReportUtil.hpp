#ifndef _VKDEBUGREPORTUTIL_HPP
#define _VKDEBUGREPORTUTIL_HPP
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

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "deAppendList.hpp"

#include <ostream>

namespace vk
{

struct DebugReportMessage
{
	VkDebugReportFlagsEXT		flags;
	VkDebugReportObjectTypeEXT	objectType;
	deUint64					object;
	size_t						location;
	deInt32						messageCode;
	std::string					layerPrefix;
	std::string					message;

	DebugReportMessage (void)
		: flags			(0)
		, objectType	((VkDebugReportObjectTypeEXT)0)
		, object		(0)
		, location		(0)
		, messageCode	(0)
	{}

	DebugReportMessage (VkDebugReportFlagsEXT		flags_,
						VkDebugReportObjectTypeEXT	objectType_,
						deUint64					object_,
						size_t						location_,
						deInt32						messageCode_,
						const std::string&			layerPrefix_,
						const std::string&			message_)
		: flags			(flags_)
		, objectType	(objectType_)
		, object		(object_)
		, location		(location_)
		, messageCode	(messageCode_)
		, layerPrefix	(layerPrefix_)
		, message		(message_)
	{}
};

std::ostream&	operator<<	(std::ostream& str, const DebugReportMessage& message);

class DebugReportRecorder
{
public:
	typedef de::AppendList<DebugReportMessage>	MessageList;

											DebugReportRecorder		(const InstanceInterface& vki, VkInstance instance);
											~DebugReportRecorder	(void);

	const MessageList&						getMessages				(void) const { return m_messages; }
	void									clearMessages			(void) { m_messages.clear(); }

private:
	MessageList								m_messages;
	const Unique<VkDebugReportCallbackEXT>	m_callback;
};

} // vk

#endif // _VKDEBUGREPORTUTIL_HPP
