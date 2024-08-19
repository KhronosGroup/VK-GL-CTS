#ifndef _ES3CDRIVERERRORTESTS_HPP
#define _ES3CDRIVERERRORTESTS_HPP

/*-------------------------------------------------------------------------
 * OpenGL Driver Error Test Suite
 * -----------------------------
 *
 * Copyright (c) 2024 Google Inc.
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \file  es3cDriverErrorTests.hpp
 * \brief Tests for known driver errors in GLSL ES 3.0.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"

namespace es3cts
{

class DriverErrorTests : public deqp::TestCaseGroup
{
public:
    DriverErrorTests(deqp::Context &context);
    virtual ~DriverErrorTests(void);

    void init(void);

private:
    DriverErrorTests(const DriverErrorTests &other)            = delete;
    DriverErrorTests &operator=(const DriverErrorTests &other) = delete;
};

} // namespace es3cts
#endif // _ES3CDRIVERERRORTESTS_HPP
