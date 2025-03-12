#ifndef _TEGLSYNCTESTS_HPP
#define _TEGLSYNCTESTS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief EGL EGL_KHR_fence_sync and EGL_KHR_reusable_sync tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "teglTestCase.hpp"

namespace deqp
{
namespace egl
{

class FenceSyncTests : public TestCaseGroup
{
public:
    FenceSyncTests(EglTestContext &eglTestCtx);
    void init(void);
};

class ReusableSyncTests : public TestCaseGroup
{
public:
    ReusableSyncTests(EglTestContext &eglTestCtx);
    void init(void);
};

} // namespace egl
} // namespace deqp

#endif // _TEGLSYNCTESTS_HPP
