/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Pretty-printing and logging utilities.
 *//*--------------------------------------------------------------------*/

#include "vkStrUtil.hpp"

#if (DE_OS == DE_OS_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#endif

namespace vk
{

struct CharPtr
{
	const char*	ptr;

	CharPtr (const char* ptr_) : ptr(ptr_) {}
};

std::ostream& operator<< (std::ostream& str, const CharPtr& ptr)
{
	if (!ptr.ptr)
		return str << "(null)";
	else
		return str << '"' << ptr.ptr << '"';
}

inline CharPtr getCharPtrStr (const char* ptr)
{
	return CharPtr(ptr);
}


#if (DE_OS == DE_OS_WIN32)

struct WStr
{
	LPCWSTR wstr;

	WStr (LPCWSTR wstr_) : wstr(wstr_) {}
};

std::ostream& operator<< (std::ostream& str, const WStr& wstr)
{
	int len = WideCharToMultiByte(CP_UTF8, 0, wstr.wstr, -1, NULL, 0, 0, 0);
	if (len < 1)
		return str << "(null)";

	std::string result;
	result.resize(len + 1);
	WideCharToMultiByte(CP_UTF8, 0, wstr.wstr, -1, &result[0], len, 0, 0);

	return str << '"' << result << '"';
}

inline WStr getWStr (pt::Win32LPCWSTR pt_wstr)
{
	return WStr(static_cast<LPCWSTR>(pt_wstr.internal));
}

#else

inline CharPtr getWStr (pt::Win32LPCWSTR pt_wstr)
{
	return CharPtr(static_cast<const char*>(pt_wstr.internal));
}

#endif


#include "vkStrUtilImpl.inl"

} // vk
