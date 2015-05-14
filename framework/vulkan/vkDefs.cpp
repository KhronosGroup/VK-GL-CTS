/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Utilities
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Vulkan utilites.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkStrUtil.hpp"

#include <sstream>

DE_STATIC_ASSERT(sizeof(vk::VkImageType)	== sizeof(deUint32));
DE_STATIC_ASSERT(sizeof(vk::VkResult)		== sizeof(deUint32));

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

ApiVersion unpackVersion (deUint32 version)
{
	return ApiVersion((version & 0xFFC00000) >> 22,
					  (version & 0x003FF000) >> 12,
					   version & 0x00000FFF);
}

deUint32 pack (const ApiVersion& version)
{
	DE_ASSERT((version.major & ~0x3FF) == 0);
	DE_ASSERT((version.minor & ~0x3FF) == 0);
	DE_ASSERT((version.patch & ~0xFFF) == 0);

	return (version.major << 22) | (version.minor << 12) | version.patch;
}

#include "vkGetObjectTypeImpl.inl"

} // vk
