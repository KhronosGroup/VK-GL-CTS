#ifndef _GLCTEXTURECOMPATIBILITYTESTS_HPP
#define _GLCTEXTURECOMPATIBILITYTESTS_HPP

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
 * \file glcTexSubImageTests.hpp
 * \brief Add tests to exercise glTexSubImage with different
 * \      (but compatible) client format than was passed to glTexImage.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"

namespace glcts
{

class TextureCompatibilityTests : public deqp::TestCaseGroup
{
public:
	TextureCompatibilityTests			(deqp::Context& context);

	virtual ~TextureCompatibilityTests	(void);

	virtual void init					(void);
};

} // glcts

#endif // _GLCTEXTURECOMPATIBILITYTESTS_HPP
