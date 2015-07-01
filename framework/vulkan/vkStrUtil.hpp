#ifndef _VKSTRUTIL_HPP
#define _VKSTRUTIL_HPP
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
