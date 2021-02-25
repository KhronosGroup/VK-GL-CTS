#ifndef _VKTTYPECOMPARISONUTIL_HPP
#define _VKTTYPECOMPARISONUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google LLC.
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
 * \brief Utility functions for generating comparison code for values with different types.
 *//*--------------------------------------------------------------------*/

#include <gluShaderUtil.hpp>
#include "gluVarTypeUtil.hpp"
#include <set>

namespace vkt
{
namespace typecomputil
{
const	char*			getCompareFuncForType (glu::DataType type);
		void			getCompareDependencies (std::set<glu::DataType> &compareFuncs, glu::DataType basicType);
		void			collectUniqueBasicTypes (std::set<glu::DataType> &basicTypes, const glu::VarType &type);
		glu::DataType	getPromoteType (glu::DataType type);
} // typecomputil
} // vkt

#endif // _VKTTYPECOMPARISONUTIL_HPP
