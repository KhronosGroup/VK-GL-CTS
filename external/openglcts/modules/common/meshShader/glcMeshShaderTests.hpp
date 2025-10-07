#ifndef _GLCMESHSHADERTESTS_HPP
#define _GLCMESHSHADERTESTS_HPP

/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 AMD Corporation.
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
 * \file
 * \brief MeshShader tests
 */ /*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "glcTestCase.hpp"

namespace glc
{
namespace meshShader
{

class MeshShaderTests : public deqp::TestCaseGroup
{
public:
    MeshShaderTests(deqp::Context &context);
    ~MeshShaderTests()
    {
    }

    void init();
};

} // namespace meshShader
} // namespace glc

#endif // _GLCMESHSHADERTESTS_HPP
