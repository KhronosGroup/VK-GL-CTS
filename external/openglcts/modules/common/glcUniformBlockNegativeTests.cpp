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
 * \file  glcUniformBlockNegativeTests.cpp
 * \brief Conformance tests uniform block negative functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcUniformBlockNegativeTests.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderProgram.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace glu;

namespace deqp
{

// clang-format off
/** @brief Vertex shader source code to test uniform buffer objects. */
const glw::GLchar* deqp::UniformBlockNegativeTestBase::m_shader_vert =
    R"(${VERSION}
    ${EXTENSION}
    ${VERT_DEFINITION}
    in vec2 Pos;
    out float Status;
    const float OK = 0.9;
    const float FAILED = 0.1;
    bool TestFunction()
    {
        ${VERT_CONDITION}
            return false;
        else
            return true;
    }
    void main()
    {
        Status = TestFunction() ? OK : FAILED;
        gl_Position = vec4(Pos, 0, 1);
    }
)";

/** @brief Fragment shader source code to test uniform buffer objects. */
const glw::GLchar* deqp::UniformBlockNegativeTestBase::m_shader_frag =
    R"(${VERSION}
    ${PRECISION}
    ${FRAG_DEFINITION}
    in float Status;
    out vec4 Color_out;
    const vec3 OK = vec3(0.1, 0.9, 0.1);
    const vec3 FAILED = vec3(0.9, 0.1, 0.1);
    bool TestFunction()
    {
        ${FRAG_CONDITION}
            return false;
        else
            return true;
    }
    void main()
    {
        Color_out = vec4(TestFunction() && Status>0.5 ? OK : FAILED, 1);
    }
    )";
// clang-format on

UniformBlockNegativeTestBase::UniformBlockNegativeTestBase(deqp::Context &context, glu::GLSLVersion glslVersion,
                                                           const char *name, const char *desc)
    : TestCase(context, name, desc)
    , m_isContextES(false)
    , m_isTestSupported(false)
    , m_glslVersion(glslVersion)
{
}

/** Stub deinit method. */
void UniformBlockNegativeTestBase::deinit()
{
}

/** Stub init method */
void UniformBlockNegativeTestBase::init()
{
    const glu::RenderContext &renderContext = m_context.getRenderContext();
    m_isContextES                           = glu::isContextTypeES(renderContext.getType());

    specializationMap["VERSION"]   = glu::getGLSLVersionDeclaration(m_glslVersion);
    specializationMap["PRECISION"] = "";
    specializationMap["EXTENSION"] = "";

    if (m_isContextES)
    {
        specializationMap["PRECISION"] = "precision highp float;";

        if (m_glslVersion >= glu::GLSL_VERSION_300_ES)
            m_isTestSupported = true;
    }
    else
    {
        if (m_context.getContextInfo().isExtensionSupported("GL_ARB_uniform_buffer_object"))
        {
            specializationMap["EXTENSION"] = "#extension GL_ARB_uniform_buffer_object: require  \n";
        }

        if (m_glslVersion >= glu::GLSL_VERSION_150)
            m_isTestSupported = true;
    }
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult UniformBlockNegativeTestBase::iterate()
{
    if (!m_isTestSupported)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
        /* This test should only be executed if we're running a GL2.0 context */
        throw tcu::NotSupportedError("GL_ARB_uniform_buffer_object is not supported");
    }

    return run_test();
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
UniformBlockStructDeclarationNegativeTestBase::UniformBlockStructDeclarationNegativeTestBase(
    deqp::Context &context, glu::GLSLVersion glslVersion)
    : UniformBlockNegativeTestBase(context, glslVersion, "structure_declaration",
                                   "Verify that structure can't be declared inside an uniform block")
{
}

/** Stub deinit method. */
void UniformBlockStructDeclarationNegativeTestBase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void UniformBlockStructDeclarationNegativeTestBase::init()
{
    UniformBlockNegativeTestBase::init();

    specializationMap["VERT_DEFINITION"] = "uniform UB0 { struct S { vec4 elem0; }; S ub_elem0; };";
    specializationMap["VERT_CONDITION"]  = "if (ub_elem0.elem0 != vec4(0.0,1.0,2.0,3.0))";

    specializationMap["FRAG_DEFINITION"] = "uniform UB0 { struct S { vec4 elem0; }; S ub_elem0; };";
    specializationMap["FRAG_CONDITION"]  = "if (ub_elem0.elem0 != vec4(0.0,1.0,2.0,3.0))";
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult UniformBlockStructDeclarationNegativeTestBase::run_test()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    m_testCtx.getLog() << tcu::TestLog::Message
                       << "-------------------------------- BEGIN ---------------------------------\n"
                       << tcu::TestLog::EndMessage;

    std::string vshader = tcu::StringTemplate(m_shader_vert).specialize(specializationMap);
    std::string fshader = tcu::StringTemplate(m_shader_frag).specialize(specializationMap);

    ProgramSources sources = makeVtxFragSources(vshader, fshader);

    ShaderProgram program(gl, sources);

    if (program.isOk())
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Shader build succeeded unexpectedly.\n"
                           << tcu::TestLog::EndMessage;
        TCU_FAIL("Compile succeeded unexpectedly");
    }
    else
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed as expected.\n"
                           << "Vertex: " << program.getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                           << program.getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
                           << "Fragment: " << program.getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                           << program.getShader(SHADERTYPE_FRAGMENT)->getSource() << "\n"
                           << "Program: " << program.getProgramInfo().infoLog << tcu::TestLog::EndMessage;
    }

    m_testCtx.getLog() << tcu::TestLog::Message
                       << "--------------------------------- END ----------------------------------\n"
                       << tcu::TestLog::EndMessage;

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
UniformBlockNegativeTests::UniformBlockNegativeTests(deqp::Context &context, glu::GLSLVersion glslVersion)
    : TestCaseGroup(context, "uniform_block_negative", "Verify uniform block negative functionality")
    , m_glslVersion(glslVersion)
{
}

/** Initializes the test group contents. */
void UniformBlockNegativeTests::init()
{
    addChild(new UniformBlockStructDeclarationNegativeTestBase(m_context, m_glslVersion));
}

} // namespace deqp
