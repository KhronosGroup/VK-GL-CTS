#ifndef _VKTCONDITIONALDISPATCHTESTS_HPP
#define _VKTCONDITIONALDISPATCHTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Test for conditional rendering of vkCmdDispatch* functions
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

namespace vkt
{
namespace conditional
{

class ConditionalDispatchTests : public tcu::TestCaseGroup
{
public:
    ConditionalDispatchTests(tcu::TestContext &testCtx);
    ~ConditionalDispatchTests(void);
    void init(void);

private:
    ConditionalDispatchTests(const ConditionalDispatchTests &other);
    ConditionalDispatchTests &operator=(const ConditionalDispatchTests &other);
};

} // namespace conditional
} // namespace vkt

#endif // _VKTCONDITIONALDISPATCHTESTS_HPP
