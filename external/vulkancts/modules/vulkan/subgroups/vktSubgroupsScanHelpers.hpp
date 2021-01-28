#ifndef _VKTSUBGROUPSSCANHELPERS_HPP
#define _VKTSUBGROUPSSCANHELPERS_HPP

/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
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
 */ /*!
 * \file
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsTestsUtils.hpp"

#include <string>

enum Operator
{
	OPERATOR_ADD,
	OPERATOR_MUL,
	OPERATOR_MIN,
	OPERATOR_MAX,
	OPERATOR_AND,
	OPERATOR_OR,
	OPERATOR_XOR,
};

enum ScanType
{
	SCAN_REDUCE,
	SCAN_INCLUSIVE,
	SCAN_EXCLUSIVE
};

std::string getScanOpName (std::string prefix, std::string suffix, Operator op, ScanType scanType);
std::string getOpOperation (Operator op, vk::VkFormat format, std::string lhs, std::string rhs);
std::string getIdentity (Operator op, vk::VkFormat format);
std::string getCompare (Operator op, vk::VkFormat format, std::string lhs, std::string rhs);

#endif // _VKTSUBGROUPSSCANHELPERS_HPP
