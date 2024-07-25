#ifndef _GLCGLSLVECTORCONSTRUCTORTESTS_HPP
#define _GLCGLSLVECTORCONSTRUCTORTESTS_HPP

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
* \file  glcGLSLVectorConstructorTests.hpp
* \brief Tests for GLSL vector type constructors
*/ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"

namespace deqp
{

class GLSLVectorConstructorTests : public TestCaseGroup
{
public:
    GLSLVectorConstructorTests(Context &context, glu::GLSLVersion glslVersion);
    ~GLSLVectorConstructorTests();

    void init();

private:
    GLSLVectorConstructorTests(const GLSLVectorConstructorTests &other)            = delete;
    GLSLVectorConstructorTests &operator=(const GLSLVectorConstructorTests &other) = delete;

    glu::GLSLVersion m_glslVersion;
};

} // namespace deqp

#endif // _GLCGLSLVECTORCONSTRUCTORTESTS_HPP
