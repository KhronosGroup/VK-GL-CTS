#ifndef _TEGLCREATESURFACETESTS_HPP
#define _TEGLCREATESURFACETESTS_HPP
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
 * \brief Simple surface construction test.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "teglTestCase.hpp"

namespace deqp
{
namespace egl
{

class CreateSurfaceTests : public TestCaseGroup
{
public:
    CreateSurfaceTests(EglTestContext &eglTestCtx);
    virtual ~CreateSurfaceTests(void);

    void init(void);
};

} // namespace egl
} // namespace deqp

#endif // _TEGLCREATESURFACETESTS_HPP
