#ifndef _ES3CNUMBERPARSINGTESTS_HPP
#define _ES3CNUMBERPARSINGTESTS_HPP

/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2020 Google Inc.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \file  es3cNumberParsingTests.hpp
 * \brief Tests for numeric value parsing in GLSL ES 3.0
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"

namespace es3cts
{

class NumberParsingTests : public deqp::TestCaseGroup
{
public:
	NumberParsingTests(deqp::Context& context);
	virtual ~NumberParsingTests(void);

	void init(void);

private:
	NumberParsingTests(const NumberParsingTests& other) = delete;
	NumberParsingTests& operator=(const NumberParsingTests& other) = delete;
};

}
#endif // _ES3CNUMBERPARSINGTESTS_HPP
