#ifndef _VKTDRAWSIMPLETEST_HPP
#define _VKTDRAWSIMPLETEST_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Draw Simple Test
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktDrawGroupParams.hpp"

namespace vkt
{
namespace Draw
{
class SimpleDrawTests : public tcu::TestCaseGroup
{
public:
    SimpleDrawTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams);
    ~SimpleDrawTests(void);
    void init(void);

private:
    SimpleDrawTests(const SimpleDrawTests &other);
    SimpleDrawTests &operator=(const SimpleDrawTests &other);

    const SharedGroupParams m_groupParams;
};
} // namespace Draw
} // namespace vkt

#endif // _VKTDRAWSIMPLETEST_HPP
