/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \brief Vulkan utilites.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkStrUtil.hpp"

#include <sstream>

DE_STATIC_ASSERT(sizeof(vk::VkImageType)	== sizeof(deUint32));
DE_STATIC_ASSERT(sizeof(vk::VkResult)		== sizeof(deUint32));
DE_STATIC_ASSERT(sizeof(vk::VkDevice)		== sizeof(void*));
DE_STATIC_ASSERT(sizeof(vk::VkBuffer)		== sizeof(deUint64));

namespace vk
{

static bool isOutOfMemoryError (VkResult result)
{
	return result == VK_ERROR_OUT_OF_DEVICE_MEMORY	||
		   result == VK_ERROR_OUT_OF_HOST_MEMORY;
}

Error::Error (VkResult error, const char* message, const char* expr, const char* file, int line)
	: tcu::TestError	(message, expr, file, line)
	, m_error			(error)
{
}

Error::Error (VkResult error, const std::string& message)
	: tcu::TestError	(message)
	, m_error			(error)
{
}

Error::~Error (void) throw()
{
}

OutOfMemoryError::OutOfMemoryError (VkResult error, const char* message, const char* expr, const char* file, int line)
	: tcu::ResourceError(message, expr, file, line)
	, m_error			(error)
{
	DE_ASSERT(isOutOfMemoryError(error));
}

OutOfMemoryError::OutOfMemoryError (VkResult error, const std::string& message)
	: tcu::ResourceError(message)
	, m_error			(error)
{
	DE_ASSERT(isOutOfMemoryError(error));
}

OutOfMemoryError::~OutOfMemoryError (void) throw()
{
}

void checkResult (VkResult result, const char* msg, const char* file, int line)
{
	if (result != VK_SUCCESS)
	{
		std::ostringstream msgStr;
		if (msg)
			msgStr << msg << ": ";

		msgStr << getResultStr(result);

		if (isOutOfMemoryError(result))
			throw OutOfMemoryError(result, msgStr.str().c_str(), DE_NULL, file, line);
		else if (result == VK_UNSUPPORTED)
			throw tcu::NotSupportedError(msgStr.str().c_str(), DE_NULL, file, line);
		else
			throw Error(result, msgStr.str().c_str(), DE_NULL, file, line);
	}
}

VkClearValue clearValueColorF32 (float r, float g, float b, float a)
{
	VkClearValue v;
	v.color.f32[0] = r;
	v.color.f32[1] = g;
	v.color.f32[2] = b;
	v.color.f32[3] = a;
	return v;
}

} // vk
