#ifndef _VKTSYNCHRONIZATIONTESTS_HPP
#define _VKTSYNCHRONIZATIONTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Synchronization tests
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vktSynchronizationDefs.hpp"

namespace vkt
{

tcu::TestCaseGroup* createSynchronizationTests (tcu::TestContext& testCtx, const std::string& name);
tcu::TestCaseGroup* createSynchronization2Tests (tcu::TestContext& testCtx, const std::string& name);
tcu::TestCaseGroup* createSynchronizationTests (tcu::TestContext& testCtx, const std::string& name, synchronization::VideoCodecOperationFlags videoCodecOperation);
tcu::TestCaseGroup* createSynchronization2Tests (tcu::TestContext& testCtx, const std::string& name, synchronization::VideoCodecOperationFlags videoCodecOperation);

} // vkt

#endif // _VKTSYNCHRONIZATIONTESTS_HPP
