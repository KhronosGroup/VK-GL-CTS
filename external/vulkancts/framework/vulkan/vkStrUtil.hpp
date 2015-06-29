#ifndef _VKSTRUTIL_HPP
#define _VKSTRUTIL_HPP
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
 * \brief Pretty-printing and logging utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuFormatUtil.hpp"

namespace vk
{

#include "vkStrUtil.inl"

template<typename T>
const char*	getTypeName	(void);

inline std::ostream& operator<< (std::ostream& s, const ApiVersion& version)
{
	return s << version.major << "." << version.minor << "." << version.patch;
}

inline std::ostream& operator<< (std::ostream& s, const VkClearColorValue& value)
{
	return s << "{ floatColor = " << tcu::formatArray(DE_ARRAY_BEGIN(value.floatColor), DE_ARRAY_END(value.floatColor))
			 << ", rawColor = " << tcu::formatArray(tcu::Format::HexIterator<deUint32>(DE_ARRAY_BEGIN(value.rawColor)), tcu::Format::HexIterator<deUint32>(DE_ARRAY_END(value.rawColor))) << " }";
}

} // vk

#endif // _VKSTRUTIL_HPP
