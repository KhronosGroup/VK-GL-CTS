#ifndef _GLCLAYOUTLOCATIONTESTS_HPP
#define _GLCLAYOUTLOCATIONTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2018 The Khronos Group Inc.
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
 * \file glcLayoutLocationTests.hpp
 * \brief
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"
#include "tes31TestCase.hpp"

namespace glcts
{

class LayoutLocationTests : public TestCaseGroup
{
public:
	LayoutLocationTests(glcts::Context& context);
	~LayoutLocationTests(void);

	void init(void);

private:
	LayoutLocationTests(const LayoutLocationTests& other);
	LayoutLocationTests& operator=(const LayoutLocationTests& other);
};

} // glcts

#endif // _GLCLAYOUTLOCATIONTESTS_HPP
