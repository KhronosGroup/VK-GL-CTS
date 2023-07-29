#ifndef _GLCUNIFORMBLOCKNEGATIVETESTS_HPP
#define _GLCUNIFORMBLOCKNEGATIVETESTS_HPP
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
 * \file  glcUniformBlockNegativeTests.hpp
 * \brief Conformance tests uniform block negative functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "gluShaderUtil.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include <map>
#include <memory>

namespace glu
{
class ShaderProgram;
}

namespace deqp
{

/** base class to handle negative uniform block tests functionality */
class UniformBlockNegativeTestBase : public deqp::TestCase
{
public:
    /* Public methods */
    UniformBlockNegativeTestBase(deqp::Context &context, glu::GLSLVersion glslVersion, const char *name,
                                 const char *desc);

    void deinit() override;
    void init() override;

    virtual tcu::TestNode::IterateResult iterate() override;

    virtual tcu::TestNode::IterateResult run_test() = 0;

protected:
    /* Private members */
    static const glw::GLchar *m_shader_vert;
    static const glw::GLchar *m_shader_frag;

    std::map<std::string, std::string> specializationMap;

    bool m_isContextES;
    bool m_isTestSupported;

    glu::GLSLVersion m_glslVersion;
};

/*
 * 4.2      Structure declaration
 * Purpose: Verify that structure can't be declared inside an uniform block.
 * Method:  Modify default negative test method replacing UB0 declaration with:
 *              uniform UB0 { struct S { vec4 elem0 }; S ub_elem0; };
 * NOTE: fixed as:
 *              uniform UB0 { struct S { vec4 elem0; }; S ub_elem0; };
 */
class UniformBlockStructDeclarationNegativeTestBase : public UniformBlockNegativeTestBase
{
public:
    UniformBlockStructDeclarationNegativeTestBase(deqp::Context &context, glu::GLSLVersion glslVersion);

    virtual void deinit() override;
    virtual void init() override;

    tcu::TestNode::IterateResult run_test() override;
};

/** Test group which encapsulates all conformance tests */
class UniformBlockNegativeTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    UniformBlockNegativeTests(deqp::Context &context, glu::GLSLVersion glslVersion);

    void init();

private:
    UniformBlockNegativeTests(const UniformBlockNegativeTests &other);
    UniformBlockNegativeTests &operator=(const UniformBlockNegativeTests &other);

    glu::GLSLVersion m_glslVersion;
};

} // namespace deqp

#endif // _GLCUNIFORMBLOCKNEGATIVETESTS_HPP
