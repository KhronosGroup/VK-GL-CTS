#ifndef _GLCCOMPRESSEDFORMATTESTS_HPP
#define _GLCCOMPRESSEDFORMATTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2021 Google Inc.
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \file glcCompressedFormatTests.hpp
 * \brief Tests for OpenGL ES 3.1 and 3.2 compressed image formats
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"

namespace glcts
{

class CompressedFormatTests : public deqp::TestCaseGroup
{
public:
	CompressedFormatTests(deqp::Context& context);
	~CompressedFormatTests(void);

	virtual void init(void);

private:
	CompressedFormatTests(const CompressedFormatTests& other) = delete;
	CompressedFormatTests& operator=(const CompressedFormatTests& other) = delete;
};

} // namespace glcts

#endif // _GLCCOMPRESSEDFORMATTESTS_HPP
