#ifndef _VKTDRAWCONCURRENTTESTS_HPP
#define _VKTDRAWCONCURRENTTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
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
 * \brief Concurrent draw tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktDrawGroupParams.hpp"

namespace vkt
{
namespace Draw
{
class ConcurrentDrawTests : public tcu::TestCaseGroup
{
public:
    ConcurrentDrawTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams);
    ~ConcurrentDrawTests(void) = default;
    void init(void);

private:
    ConcurrentDrawTests(const ConcurrentDrawTests &other);
    ConcurrentDrawTests &operator=(const ConcurrentDrawTests &other);

private:
    const SharedGroupParams m_groupParams;
};
} // namespace Draw
} // namespace vkt

#endif // _VKTDRAWCONCURRENTTESTS_HPP
