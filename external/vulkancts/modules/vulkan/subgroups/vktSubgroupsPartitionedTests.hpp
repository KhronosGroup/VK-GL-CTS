#ifndef _VKTSUBGROUPSPARTITIONEDTESTS_HPP
#define _VKTSUBGROUPSPARTITIONEDTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2018 NVIDIA Corporation
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

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace subgroups
{

tcu::TestCaseGroup *createSubgroupsPartitionedTests(tcu::TestContext &testCtx);

} // namespace subgroups
} // namespace vkt

#endif // _VKTSUBGROUPSPARTITIONEDTESTS_HPP
