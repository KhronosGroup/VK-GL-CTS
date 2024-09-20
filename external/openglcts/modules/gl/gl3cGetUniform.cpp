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
 * \file  gl3cGetUniform.cpp
 * \brief Conformance tests for the uniform getter functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "gl3cGetUniform.hpp"
#include "gluDefs.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace glu;

namespace
{

/** @brief Vertex shader source code to test vectors GetUniform functionality. */
const glw::GLchar *vec_vert_shader =
    R"(${VERSION}
    in vec4 vertex;
    uniform mat4 ModelViewProjectionMatrix;
    uniform float vuni1;
    uniform vec2 vuni2;
    uniform vec3 vuni3;
    uniform vec4 vuni4;
    out vec4 color;

    void main (void)
    {
        color = vec4(vuni1, vuni2[0] + vuni2[1], vuni3[0] + vuni3[1] + vuni3[2], vuni4[0] + vuni4[1] + vuni4[2] + vuni4[3]);
        gl_Position = ModelViewProjectionMatrix * vertex;
    }
    )";

/** @brief Fragment shader source code to test vectors GetUniform functionality. */
const glw::GLchar *vec_frag_shader =
    R"(${VERSION}
    uniform float funi1;
    uniform vec2 funi2;
    uniform vec3 funi3;
    uniform vec4 funi4;
    in vec4 color;
    out vec4 fragColor;

    void main (void)
    {
        vec4 temp = vec4(funi1, funi2[0] + funi2[1], funi3[0] + funi3[1] + funi3[2], funi4[0] + funi4[1] + funi4[2] + funi4[3]);
        fragColor = temp + color;
    }
    )";

/** @brief Vertex shader source code to test integer vector GetUniform functionality. */
const glw::GLchar *ivec_vert_shader =
    R"(${VERSION}
    in vec4 vertex;
    uniform mat4 ModelViewProjectionMatrix;
    uniform int vuni1;
    uniform ivec2 vuni2;
    uniform ivec3 vuni3;
    uniform ivec4 vuni4;
    out vec4 color;

    void main (void)
    {
        color = vec4(float(vuni1), float(vuni2[0] + vuni2[1]), float(vuni3[0] + vuni3[1] + vuni3[2]), float(vuni4[0] + vuni4[1] + vuni4[2] + vuni4[3]) );
        gl_Position = ModelViewProjectionMatrix * vertex;
    }
    )";

/** @brief Fragment shader source code to test integer vector GetUniform functionality. */
const glw::GLchar *ivec_frag_shader =
    R"(${VERSION}
    uniform int funi1;
    uniform ivec2 funi2;
    uniform ivec3 funi3;
    uniform ivec4 funi4;
    in vec4 color;
    out vec4 fragColor;

    void main (void)
    {
        vec4 temp = vec4(float(funi1), float(funi2[0] + funi2[1]), float(funi3[0] + funi3[1] + funi3[2]), float(funi4[0] + funi4[1] + funi4[2] + funi4[3]));
        fragColor = temp + color;
    }
    )";

/** @brief Vertex shader source code to test boolean vector GetUniform functionality. */
const glw::GLchar *bvec_vert_shader =
    R"(${VERSION}
    in vec4 vertex;
    uniform mat4 ModelViewProjectionMatrix;
    uniform bool vuni1;
    uniform bvec2 vuni2;
    uniform bvec3 vuni3;
    uniform bvec4 vuni4;
    out vec4 color;

    void main (void)
    {
        color = vec4(0.0, 0.0, 0.0, 0.0);
        if(vuni1 || vuni2[0] && vuni2[1] && vuni3[0] && vuni3[1] && vuni3[2] || vuni4[0] && vuni4[1] && vuni4[2] && vuni4[3])
        color = vec4(1.0, 0.0, 0.5, 1.0);
        gl_Position = ModelViewProjectionMatrix * vertex;
    }
    )";

/** @brief Fragment shader source code to test boolean vector GetUniform functionality. */
const glw::GLchar *bvec_frag_shader =
    R"(${VERSION}
    uniform bool funi1;
    uniform bvec2 funi2;
    uniform bvec3 funi3;
    uniform bvec4 funi4;
    in vec4 color;
    out vec4 fragColor;

    void main (void)
    {
        vec4 temp = vec4(0.0, 0.0, 0.0, 0.0);
        if(funi1 || funi2[0] && funi2[1] && funi3[0] && funi3[1] && funi3[2] || funi4[0] && funi4[1] && funi4[2] && funi4[3])
                temp = vec4(1.0, 0.0, 0.5, 1.0);
        fragColor = temp + color;
    }
    )";

/** @brief Vertex shader source code to test matrix GetUniform functionality. */
const glw::GLchar *mat_vert_shader =
    R"(${VERSION}
    in vec4 vertex;
    uniform mat4 ModelViewProjectionMatrix;
    uniform mat2 vuni2;
    uniform mat3 vuni3;
    uniform mat4 vuni4;
    out vec4 color;

    void main (void)
    {
        color = vec4( vuni2[0][0] + vuni2[0][1] + vuni2[1][0] + vuni2[1][1],
                      vuni3[0][0] + vuni3[0][1] + vuni3[0][2] + vuni3[1][0] + vuni3[1][1] + vuni3[1][2] + vuni3[2][0] + vuni3[2][1] + vuni3[2][2],
                      vuni4[0][0] + vuni4[0][1] + vuni4[0][2] + vuni4[0][3] + vuni4[1][0] + vuni4[1][1] + vuni4[1][2] + vuni4[1][3] + vuni4[2][0] + vuni4[2][1] + vuni4[2][2] + vuni4[2][3] + vuni4[3][0] + vuni4[3][1] + vuni4[3][2] + vuni4[3][3], 1.0 );

        gl_Position = ModelViewProjectionMatrix * vertex;
    }
    )";

/** @brief Fragment shader source code to test matrix GetUniform functionality. */
const glw::GLchar *mat_frag_shader =
    R"(${VERSION}
    uniform mat2 funi2;
    uniform mat3 funi3;
    uniform mat4 funi4;
    in vec4 color;
    out vec4 fragColor;

    void main (void)
    {
        vec4 temp = vec4( funi2[0][0] + funi2[0][1] + funi2[1][0] + funi2[1][1],
                      funi3[0][0] + funi3[0][1] + funi3[0][2] + funi3[1][0] + funi3[1][1] + funi3[1][2] + funi3[2][0] + funi3[2][1] + funi3[2][2],
                      funi4[0][0] + funi4[0][1] + funi4[0][2] + funi4[0][3] + funi4[1][0] + funi4[1][1] + funi4[1][2] + funi4[1][3] + funi4[2][0] + funi4[2][1] + funi4[2][2] + funi4[2][3] + funi4[3][0] + funi4[3][1] + funi4[3][2] + funi4[3][3], 1.0 );
        fragColor = temp + color;
    }
    )";

} // anonymous namespace

namespace gl3cts
{

/** Constructor.
 *
 *  @param context     Rendering context
 */
GetUniformTestCase::GetUniformTestCase(deqp::Context &context)
    : TestCase(context, "get_uniform", "Verifies uniform getter functionality")
    , m_active_program_id(0)
{
}

/** Stub deinit method. */
void GetUniformTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void GetUniformTestCase::init()
{
    glu::GLSLVersion glslVersion = glu::getContextTypeGLSLVersion(m_context.getRenderContext().getType());

    specializationMap["VERSION"] = glu::getGLSLVersionDeclaration(glslVersion);

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    auto make_program = [&](const char *vs, const char *fs)
    {
        /* Building basic program. */
        std::string vert_shader = tcu::StringTemplate(vs).specialize(specializationMap);
        std::string frag_shader = tcu::StringTemplate(fs).specialize(specializationMap);

        ProgramSources sources = makeVtxFragSources(vert_shader, frag_shader);

        auto program = new ShaderProgram(gl, sources);

        if (!program->isOk())
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                               << "Vertex: " << program->getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                               << vert_shader << "\n"
                               << "Fragment: " << program->getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                               << frag_shader << "\n"
                               << "Program: " << program->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
            delete program;
            TCU_FAIL("Invalid program");
        }
        return program;
    };

    auto sources_list = {
        std::make_pair(vec_vert_shader, vec_frag_shader), std::make_pair(ivec_vert_shader, ivec_frag_shader),
        std::make_pair(bvec_vert_shader, bvec_frag_shader), std::make_pair(mat_vert_shader, mat_frag_shader)};

    for (auto src : sources_list)
        m_programs.emplace_back(make_program(src.first, src.second));
}

template <typename T>
bool GetUniformTestCase::test_buffer(const T *resultBuf, const T *expectedBuf, GLint size, GLfloat tolerance)
{
    for (GLint i = 0; i < size; i++)
    {
        if (fabs(resultBuf[i] - expectedBuf[i]) > tolerance)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "TestBufferf: "
                               << "Found : " << resultBuf[i] << ", Expected : " << expectedBuf[i]
                               << tcu::TestLog::EndMessage;

            return false;
        }
    }

    return true;
}

template <typename T>
bool GetUniformTestCase::verify_get_uniform_ops(const char *name, const char *error_message, const TestParams<T> &p)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint loc                = gl.getUniformLocation(m_active_program_id, name);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

    if (loc != -1)
    {
        if (std::is_same<T, GLfloat>::value)
        {
            switch (std::get<2>(p))
            {
            default:
            case 1:
                gl.uniform1f(loc, std::get<0>(p)[0]);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");
                break;
            case 2:
                gl.uniform2f(loc, std::get<0>(p)[0], std::get<0>(p)[1]);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform2f");
                break;
            case 3:
                gl.uniform3f(loc, std::get<0>(p)[0], std::get<0>(p)[1], std::get<0>(p)[2]);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform3f");
                break;
            case 4:
                gl.uniform4f(loc, std::get<0>(p)[0], std::get<0>(p)[1], std::get<0>(p)[2], std::get<0>(p)[3]);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform4f");
                break;
            }

            gl.getUniformfv(m_active_program_id, loc, (GLfloat *)std::get<1>(p));
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformfv");
        }
        else if (std::is_same<T, GLint>::value)
        {
            switch (std::get<2>(p))
            {
            default:
            case 1:
                gl.uniform1i(loc, std::get<0>(p)[0]);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");
                break;
            case 2:
                gl.uniform2i(loc, std::get<0>(p)[0], std::get<0>(p)[1]);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform2i");
                break;
            case 3:
                gl.uniform3i(loc, std::get<0>(p)[0], std::get<0>(p)[1], std::get<0>(p)[2]);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform3i");
                break;
            case 4:
                gl.uniform4i(loc, std::get<0>(p)[0], std::get<0>(p)[1], std::get<0>(p)[2], std::get<0>(p)[3]);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform4i");
                break;
            }

            gl.getUniformiv(m_active_program_id, loc, (GLint *)std::get<1>(p));
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformiv");
        }

        if (!test_buffer(std::get<1>(p), std::get<0>(p), std::get<2>(p), std::get<3>(p)))
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: " << error_message << "\n"
                               << tcu::TestLog::EndMessage;
            return false;
        }
    }
    return true;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult GetUniformTestCase::iterate()
{
    bool result              = true;
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLenum error             = 0;

    // clang-format off
    GLfloat matProjection[] = {1,0,0,0,
                               0,1,0,0,
                               0,0,1,0,
                               0,0,0,1};

    GLfloat data_boolf[1] = { 1.0f };
    GLint data_booli[1] = { 1 };

    GLfloat data_float[1] = { -0.3f };
    GLint data_int[1] = { -1 };

    GLfloat data_bvec2f[2] = { 0.0f, 1.0f };
    GLfloat data_bvec3f[3] = { 1.0f, 0.0f, 0.0f };
    GLfloat data_bvec4f[4] = { 1.0f, 1.0f, 0.0f, 1.0f };

    GLint data_bvec2i[2] = { 0, 1 };
    GLint data_bvec3i[3] = { 1, 0, 0 };
    GLint data_bvec4i[4] = { 1, 1, 0, 1 };

    GLint data_vec2i[2] = { 1, 0 };
    GLint data_vec3i[3] = { 1, 0, 9 };
    GLint data_vec4i[4] = { 8, 12, 6, 3 };

    GLfloat data_vec2f[2] = { 0.1f, 0.1f };
    GLfloat data_vec3f[3] = { 1.0f, 0.0f, 0.9f };
    GLfloat data_vec4f[4] = { 0.8f, 12.0f, 6.7f, 3.8f };

    GLfloat data_mat2[4] = { 1.0f, 2.3f,
                             4.2f, 7.8f };
    GLfloat data_mat3[9] = { 1.0f, 2.3f, 5.67f,
                             4.2f, 7.8f, 8.33f,
                             0.2f, 1.23f, 4.57f };
    GLfloat data_mat4[16] = { 1.0f, 2.3f, 5.67f, 6.87f,
                              4.2f, 7.8f, 8.33f, 9.21f,
                              0.2f, 1.23f, 4.57f, 8.68f,
                              11.93f, 19.1f, 22.2f, 23.1f };
    // clang-format on

    GLfloat floatBuf[16];
    GLint intBuf[4];

    gl.disable(GL_DITHER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable(GL_DITHER)");

    gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

    // Test 4 : Program not linked successfully
    {
        GLuint program = gl.createProgram();
        GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

        gl.getUniformfv(program, 1, floatBuf);
        if ((error = gl.getError()) != GL_INVALID_OPERATION)
        {
            result = false;
            m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: "
                               << "glGetUniformfv : GL_INVALID_OPERATION not returned when program handle not linked.\n"
                               << tcu::TestLog::EndMessage;
        }

        gl.getUniformiv(program, 1, intBuf);
        if ((error = gl.getError()) != GL_INVALID_OPERATION)
        {
            result = false;
            m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: "
                               << "glGetUniformiv : GL_INVALID_OPERATION not returned when program handle not linked.\n"
                               << tcu::TestLog::EndMessage;
        }

        gl.useProgram(0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

        gl.deleteProgram(program);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");
    }

    for (size_t s = 0; s < m_programs.size(); s++)
    {
        m_active_program_id = m_programs[s]->getProgram();

        /* Use appropriate shader program */
        gl.useProgram(m_active_program_id);
        GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

        GLint locMatProjection = gl.getUniformLocation(m_active_program_id, "ModelViewProjectionMatrix");
        GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

        if (locMatProjection != -1)
        {
            gl.uniformMatrix4fv(locMatProjection, 1, 0, matProjection);
            GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix4fv");
        }

        GLint locVertices = gl.getAttribLocation(m_active_program_id, "vertex");
        GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

        if (locVertices != -1)
        {
            gl.enableVertexAttribArray(locVertices);
            GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
        }

        // Render Code
        //--------------------------------------------------------

        // vec_tests.vert
        if (s == 0)
        {
            auto check_getUniform_error = [&](const char *msg, const GLenum expected)
            {
                if ((error = gl.getError()) != expected)
                {
                    result = false;
                    m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: " << msg
                                       << tcu::TestLog::EndMessage;
                }
            };

            // Test 1 : Fragment shader for program handle
            gl.getUniformfv(m_programs[s]->getShader(SHADERTYPE_FRAGMENT)->getShader(), 1, floatBuf);
            check_getUniform_error(
                "glGetUniformfv : GL_INVALID_OPERATION not returned when passing fragment shader as handle.\n",
                GL_INVALID_OPERATION);

            gl.getUniformiv(m_programs[s]->getShader(SHADERTYPE_FRAGMENT)->getShader(), 1, intBuf);
            check_getUniform_error(
                "glGetUniformiv : GL_INVALID_OPERATION not returned when passing fragment shader as handle.\n",
                GL_INVALID_OPERATION);

            // Test 1B : Vertex shader for program handle
            gl.getUniformfv(m_programs[s]->getShader(glu::SHADERTYPE_VERTEX)->getShader(), 1, floatBuf);
            check_getUniform_error(
                "glGetUniformfv : GL_INVALID_OPERATION not returned when passing vertex shader as handle.\n",
                GL_INVALID_OPERATION);

            gl.getUniformiv(m_programs[s]->getShader(glu::SHADERTYPE_VERTEX)->getShader(), 1, intBuf);
            check_getUniform_error(
                "glGetUniformiv : GL_INVALID_OPERATION not returned when passing vertex shader as handle.\n",
                GL_INVALID_OPERATION);

            // Test 2 : Invalid location passed
            gl.getUniformfv(m_active_program_id, -1, floatBuf);
            check_getUniform_error(
                "glGetUniformfv : GL_INVALID_OPERATION not returned when passing invalid location.\n",
                GL_INVALID_OPERATION);

            gl.getUniformiv(m_active_program_id, -1, intBuf);
            check_getUniform_error(
                "glGetUniformiv : GL_INVALID_OPERATION not returned when passing invalid location.\n",
                GL_INVALID_OPERATION);

            // Test 3 : Invalid program handle
            gl.getUniformfv(0, 1, floatBuf);
            check_getUniform_error(
                "glGetUniformfv : GL_INVALID_VALUE not returned when passing invalid(0) program object.\n",
                GL_INVALID_VALUE);

            gl.getUniformiv(0, 1, intBuf);
            check_getUniform_error(
                "glGetUniformiv : GL_INVALID_VALUE not returned when passing invalid(0) program object.\n",
                GL_INVALID_VALUE);
        }

        // Other tests
        if (s == 0)
        {
            // Bool fragment
            result &= verify_get_uniform_ops("funi1", "Error while retrieving data from bool uniform.",
                                             std::make_tuple(data_boolf, floatBuf, 1, 0.0008f));

            // Bool vertex
            result &= verify_get_uniform_ops("vuni1", "Error while retrieving data from bool uniform.",
                                             std::make_tuple(data_boolf, floatBuf, 1, 0.0008f));

            // Float fragment
            result &= verify_get_uniform_ops("funi1", "Error while retrieving data from float uniform.",
                                             std::make_tuple(data_float, floatBuf, 1, 0.0008f));

            // Float vertex
            result &= verify_get_uniform_ops("vuni1", "Error while retrieving data from float uniform.",
                                             std::make_tuple(data_float, floatBuf, 1, 0.0008f));

            // vec2 frag - float
            result &= verify_get_uniform_ops("funi2", "Error while retrieving data from vec2 uniform.",
                                             std::make_tuple(data_vec2f, floatBuf, 2, 0.0008f));

            // vec2 vertex - float
            result &= verify_get_uniform_ops("vuni2", "Error while retrieving data from vec2 uniform.",
                                             std::make_tuple(data_vec2f, floatBuf, 2, 0.0008f));

            // vec3 frag - float
            result &= verify_get_uniform_ops("funi3", "Error while retrieving data from vec3 uniform.",
                                             std::make_tuple(data_vec3f, floatBuf, 3, 0.0008f));

            // vec3 vertex - float
            result &= verify_get_uniform_ops("vuni3", "Error while retrieving data from vec3 uniform.",
                                             std::make_tuple(data_vec3f, floatBuf, 3, 0.0008f));

            // vec4 frag - float
            result &= verify_get_uniform_ops("funi4", "Error while retrieving data from vec4 uniform.",
                                             std::make_tuple(data_vec4f, floatBuf, 4, 0.0008f));

            // vec4 vertex - float
            result &= verify_get_uniform_ops("vuni4", "Error while retrieving data from vec4 uniform.",
                                             std::make_tuple(data_vec4f, floatBuf, 4, 0.0008f));
        }
        else if (s == 1)
        {
            // Bool fragment
            result &= verify_get_uniform_ops("funi1", "Error while retrieving data from bool uniform.",
                                             std::make_tuple(data_booli, intBuf, 1, 0));

            // Bool vertex
            result &= verify_get_uniform_ops("vuni1", "Error while retrieving data from bool uniform.",
                                             std::make_tuple(data_booli, intBuf, 1, 0));

            // Int fragment
            result &= verify_get_uniform_ops("funi1", "Error while retrieving data from int uniform.",
                                             std::make_tuple(data_int, intBuf, 1, 0));

            // Int vertex
            result &= verify_get_uniform_ops("vuni1", "Error while retrieving data from int uniform.",
                                             std::make_tuple(data_int, intBuf, 1, 0));

            // vec2 frag - int
            result &= verify_get_uniform_ops("funi2", "Error while retrieving data from vec2 uniform.",
                                             std::make_tuple(data_vec2i, intBuf, 2, 0));

            // vec2 vertex - int
            result &= verify_get_uniform_ops("vuni2", "Error while retrieving data from vec2 uniform.",
                                             std::make_tuple(data_vec2i, intBuf, 2, 0));

            // vec3 frag - int
            result &= verify_get_uniform_ops("funi3", "Error while retrieving data from vec3 uniform.",
                                             std::make_tuple(data_vec3i, intBuf, 3, 0));

            // vec3 vertex - float
            result &= verify_get_uniform_ops("vuni3", "Error while retrieving data from vec3 uniform.",
                                             std::make_tuple(data_vec3i, intBuf, 3, 0));

            // vec4 frag - float
            result &= verify_get_uniform_ops("funi4", "Error while retrieving data from vec4 uniform.",
                                             std::make_tuple(data_vec4i, intBuf, 4, 0));

            // vec4 vertex - float
            result &= verify_get_uniform_ops("vuni4", "Error while retrieving data from vec4 uniform.",
                                             std::make_tuple(data_vec4i, intBuf, 4, 0));
        }
        else if (s == 2)
        {
            // bvec2 frag - float
            result &= verify_get_uniform_ops("funi2", "Error while retrieving data from bvec2 uniform.",
                                             std::make_tuple(data_bvec2f, floatBuf, 2, 0.0008f));

            // bvec2 vertex - float
            result &= verify_get_uniform_ops("vuni2", "Error while retrieving data from bvec2 uniform.",
                                             std::make_tuple(data_bvec2f, floatBuf, 2, 0.0008f));

            // bvec3 frag - float
            result &= verify_get_uniform_ops("funi3", "Error while retrieving data from bvec3 uniform.",
                                             std::make_tuple(data_bvec3f, floatBuf, 3, 0.0008f));

            // bvec3 vertex - float
            result &= verify_get_uniform_ops("vuni3", "Error while retrieving data from bvec3 uniform.",
                                             std::make_tuple(data_bvec3f, floatBuf, 3, 0.0008f));

            // bvec4 frag - float
            result &= verify_get_uniform_ops("funi4", "Error while retrieving data from bvec4 uniform.",
                                             std::make_tuple(data_bvec4f, floatBuf, 4, 0.0008f));

            // bvec4 vertex - float
            result &= verify_get_uniform_ops("vuni4", "Error while retrieving data from bvec4 uniform.",
                                             std::make_tuple(data_bvec4f, floatBuf, 4, 0.0008f));

            // bvec2 frag - int
            result &= verify_get_uniform_ops("funi2", "Error while retrieving data from bvec2 uniform.",
                                             std::make_tuple(data_bvec2i, intBuf, 2, 0));

            // bvec2 vertex - int
            result &= verify_get_uniform_ops("vuni2", "Error while retrieving data from bvec2 uniform.",
                                             std::make_tuple(data_bvec2i, intBuf, 2, 0));

            // bvec3 frag - int
            result &= verify_get_uniform_ops("funi3", "Error while retrieving data from bvec3 uniform.",
                                             std::make_tuple(data_bvec3i, intBuf, 3, 0));

            // bvec3 vertex - int
            result &= verify_get_uniform_ops("vuni3", "Error while retrieving data from bvec3 uniform.",
                                             std::make_tuple(data_bvec3i, intBuf, 3, 0));

            // bvec4 frag - int
            result &= verify_get_uniform_ops("funi4", "Error while retrieving data from bvec4 uniform.",
                                             std::make_tuple(data_bvec4i, intBuf, 4, 0));

            // bvec4 vertex - int
            result &= verify_get_uniform_ops("vuni4", "Error while retrieving data from bvec4 uniform.",
                                             std::make_tuple(data_bvec4i, intBuf, 4, 0));
        }
        else if (s == 3)
        {
            // mat2 frag - float
            GLint loc = gl.getUniformLocation(m_active_program_id, "funi2");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
            if (loc != -1)
            {
                gl.uniformMatrix2fv(loc, 1, GL_FALSE, data_mat2);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix2fv");

                gl.getUniformfv(m_active_program_id, loc, floatBuf);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformfv");

                if (!test_buffer(floatBuf, data_mat2, 4, 0.0008f))
                {
                    result = false;
                    m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: "
                                       << "Error while retrieving data from mat2 uniform\n"
                                       << tcu::TestLog::EndMessage;
                }
            }

            // mat2 vertex - float
            loc = gl.getUniformLocation(m_active_program_id, "vuni2");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
            if (loc != -1)
            {
                gl.uniformMatrix2fv(loc, 1, GL_FALSE, data_mat2);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix2fv");

                gl.getUniformfv(m_active_program_id, loc, floatBuf);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformfv");

                if (!test_buffer(floatBuf, data_mat2, 4, 0.0008f))
                {
                    result = false;
                    m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: "
                                       << "Error while retrieving data from mat2 uniform\n"
                                       << tcu::TestLog::EndMessage;
                }
            }

            // mat3 frag - float
            loc = gl.getUniformLocation(m_active_program_id, "funi3");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
            if (loc != -1)
            {
                gl.uniformMatrix3fv(loc, 1, GL_FALSE, data_mat3);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix3fv");

                gl.getUniformfv(m_active_program_id, loc, floatBuf);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformfv");

                if (!test_buffer(floatBuf, data_mat3, 9, 0.0008f))
                {
                    result = false;
                    m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: "
                                       << "Error while retrieving data from mat3 uniform\n"
                                       << tcu::TestLog::EndMessage;
                }
            }

            // mat3 vertex - float
            loc = gl.getUniformLocation(m_active_program_id, "vuni3");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
            if (loc != -1)
            {
                gl.uniformMatrix3fv(loc, 1, GL_FALSE, data_mat3);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix3fv");

                gl.getUniformfv(m_active_program_id, loc, floatBuf);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformfv");

                if (!test_buffer(floatBuf, data_mat3, 9, 0.0008f))
                {
                    result = false;
                    m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: "
                                       << "Error while retrieving data from mat3 uniform\n"
                                       << tcu::TestLog::EndMessage;
                }
            }

            // mat4 frag - float
            loc = gl.getUniformLocation(m_active_program_id, "funi4");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
            if (loc != -1)
            {
                gl.uniformMatrix4fv(loc, 1, GL_FALSE, data_mat4);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix4fv");

                gl.getUniformfv(m_active_program_id, loc, floatBuf);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformfv");

                if (!test_buffer(floatBuf, data_mat4, 16, 0.0008f))
                {
                    result = false;
                    m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: "
                                       << "Error while retrieving data from mat4 uniform\n"
                                       << tcu::TestLog::EndMessage;
                }
            }

            // mat4 vertex - float
            loc = gl.getUniformLocation(m_active_program_id, "vuni4");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
            if (loc != -1)
            {
                gl.uniformMatrix4fv(loc, 1, GL_FALSE, data_mat4);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix4fv");

                gl.getUniformfv(m_active_program_id, loc, floatBuf);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformfv");

                if (!test_buffer(floatBuf, data_mat4, 16, 0.0008f))
                {
                    result = false;
                    m_testCtx.getLog() << tcu::TestLog::Message << "GetUniformTestCase::iterate: "
                                       << "Error while retrieving data from mat4 uniform\n"
                                       << tcu::TestLog::EndMessage;
                }
            }
        }
    }

    gl.clearColor(0.1f, 0.2f, 0.3f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

    if (result)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
GetUniformTests::GetUniformTests(deqp::Context &context)
    : TestCaseGroup(context, "get_uniform_tests", "Verify conformance of uniform getters implementation")
{
}

/** Initializes the test group contents. */
void GetUniformTests::init()
{
    addChild(new GetUniformTestCase(m_context));
}

} // namespace gl3cts
