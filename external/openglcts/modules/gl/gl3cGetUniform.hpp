#ifndef _GL3CGETUNIFORM_HPP
#define _GL3CGETUNIFORM_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
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
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/**
 */ /*!
 * \file  gl3cGetUniform.hpp
 * \brief Conformance tests for the uniform getter functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "gluShaderProgram.hpp"
#include "glwDefs.hpp"

#include <map>
#include <memory>

namespace gl3cts
{

template <typename T>
using TestParams = std::tuple<T *, T *, glw::GLint, T>;

class GetUniformTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    GetUniformTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

private:
    template <typename T>
    bool test_buffer(const T *, const T *, glw::GLint, glw::GLfloat);
    template <typename T>
    bool verify_get_uniform_ops(const char *, const char *, const TestParams<T> &);

private:
    /* Private members */
    std::vector<std::unique_ptr<glu::ShaderProgram>> m_programs;

    glw::GLint m_active_program_id;

    std::map<std::string, std::string> specializationMap;
};

/** Test group which encapsulates all conformance tests */
class GetUniformTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    GetUniformTests(deqp::Context &context);

    void init();

private:
    GetUniformTests(const GetUniformTests &other);
    GetUniformTests &operator=(const GetUniformTests &other);
};

} // namespace gl3cts

#endif // _GL3CGETUNIFORM_HPP
