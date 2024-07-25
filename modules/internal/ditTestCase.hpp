#ifndef _DITTESTCASE_HPP
#define _DITTESTCASE_HPP
/*-------------------------------------------------------------------------
 * drawElements Internal Test Module
 * ---------------------------------
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
 * \brief Test case classes for internal tests.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace dit
{

class SelfCheckCase : public tcu::TestCase
{
public:
    typedef void (*Function)(void);

    SelfCheckCase(tcu::TestContext &testCtx, const char *name, const char *desc, Function func)
        : tcu::TestCase(testCtx, name, desc)
        , m_function(func)
    {
    }

    IterateResult iterate(void)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        m_function();
        return STOP;
    }

private:
    Function m_function;
};

} // namespace dit

#endif // _DITTESTCASE_HPP
