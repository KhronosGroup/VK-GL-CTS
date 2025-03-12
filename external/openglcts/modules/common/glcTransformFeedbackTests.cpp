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
 * \file  glcTransformFeedbackTests.cpp
 * \brief Conformance tests for the transform_feedback2 functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "glcTransformFeedbackTests.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace glu;

namespace
{
// clang-format off
GLfloat vertices[24] = {
    -0.8f, -0.8f, 0.0f, 1.0f,
    0.8f,  -0.8f, 0.0f, 1.0f,
    -0.8f, 0.8f,  0.0f, 1.0f,

    0.8f,  0.8f,  0.0f, 1.0f,
    -0.8f, 0.8f,  0.0f, 1.0f,
    0.8f,  -0.8f, 0.0f, 1.0f,
};
// clang-format on

} // namespace

namespace glcts
{

// clang-format off
/** @brief Vertex shader source code to test transform feedback states conformance. */
const glw::GLchar* glcts::TransformFeedbackStatesTestCase::m_shader_vert =
    R"(${VERSION}
    in vec4 in_vertex;

    void main (void)
    {
        vec4 temp = in_vertex;

        temp.xyz *= 0.5;

        gl_Position = temp;
    }
)";

/** @brief Fragment shader source code to test transform feedback states conformance. */
const glw::GLchar* glcts::TransformFeedbackStatesTestCase::m_shader_frag =
    R"(${VERSION}
    ${PRECISION}
    out vec4 frag;
    void main (void)
    {
        frag = vec4(0.0);
    }
)";
// clang-format on

/** Constructor.
 *
 *  @param context     Rendering context
 */
TransformFeedbackStatesTestCase::TransformFeedbackStatesTestCase(deqp::Context &context)
    : TestCase(context, "transform_feedback2_states", "Verifies transform feedback objects with different states")
    , m_program(0)
    , m_vao(0)
    , m_buffers{0, 0}
    , m_tf_id(0)
    , m_queries{0, 0}
    , m_isContextES(false)
    , m_testSupported(false)
{
}

TransformFeedbackStatesTestCase::~TransformFeedbackStatesTestCase()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.deleteQueries(2, m_queries);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");

    gl.deleteBuffers(2, m_buffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

    if (m_tf_id != 0)
    {
        gl.deleteTransformFeedbacks(1, &m_tf_id);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }

    if (m_vao != 0)
    {
        gl.deleteVertexArrays(1, &m_vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");
    }

    if (m_program != 0)
    {
        gl.deleteProgram(m_program);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");
    }
}

/** Stub deinit method. */
void TransformFeedbackStatesTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void TransformFeedbackStatesTestCase::init()
{
    const glu::RenderContext &renderContext = m_context.getRenderContext();
    glu::GLSLVersion glslVersion            = glu::getContextTypeGLSLVersion(renderContext.getType());
    m_isContextES                           = glu::isContextTypeES(renderContext.getType());

    specializationMap["VERSION"]   = glu::getGLSLVersionDeclaration(glslVersion);
    specializationMap["PRECISION"] = "";

    if (m_isContextES)
    {
        specializationMap["PRECISION"] = "precision highp float;";
        m_testSupported                = true;
    }
    else
    {
        auto contextType = m_context.getRenderContext().getType();
        m_testSupported  = (glu::contextSupports(contextType, glu::ApiType::core(1, 4)) &&
                           m_context.getContextInfo().isExtensionSupported("GL_ARB_transform_feedback2")) ||
                          glu::contextSupports(contextType, glu::ApiType::core(4, 0));
    }
}

/* Compiles and links transform feedback program. */
void TransformFeedbackStatesTestCase::buildTransformFeedbackProgram(const char *vsSource, const char *fsSource)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    GLint status = 0;
    m_program    = gl.createProgram();
    if (!m_program)
    {
        TCU_FAIL("Program object not valid");
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    // vertex shader
    {
        GLuint vShader = gl.createShader(GL_VERTEX_SHADER);
        if (!vShader)
        {
            TCU_FAIL("Shader object not valid");
        }
        GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

        gl.shaderSource(vShader, 1, (const char **)&vsSource, NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "shaderSource");

        gl.compileShader(vShader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");

        gl.getShaderiv(vShader, GL_COMPILE_STATUS, &status);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");

        if (status == GL_FALSE)
        {
            GLint infoLogLength = 0;
            gl.getShaderiv(vShader, GL_INFO_LOG_LENGTH, &infoLogLength);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");

            std::vector<char> infoLogBuf(infoLogLength + 1);
            gl.getShaderInfoLog(vShader, (GLsizei)infoLogBuf.size(), NULL, infoLogBuf.data());
            GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderInfoLog");

            m_testCtx.getLog() << tcu::TestLog::Message << "Vertex shader build failed.\n"
                               << "Vertex: " << infoLogBuf.data() << "\n"
                               << vsSource << "\n"
                               << tcu::TestLog::EndMessage;

            gl.deleteShader(vShader);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");

            TCU_FAIL("Failed to compile transform feedback vertex shader");
        }
        gl.attachShader(m_program, vShader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "attachShader");

        gl.deleteShader(vShader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    if (fsSource)
    {
        GLuint fShader = gl.createShader(GL_FRAGMENT_SHADER);
        gl.shaderSource(fShader, 1, (const char **)&fsSource, NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "shaderSource");

        gl.compileShader(fShader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");

        gl.getShaderiv(fShader, GL_COMPILE_STATUS, &status);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");

        if (status == GL_FALSE)
        {
            GLint infoLogLength = 0;
            gl.getShaderiv(fShader, GL_INFO_LOG_LENGTH, &infoLogLength);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");

            std::vector<char> infoLogBuf(infoLogLength + 1);
            gl.getShaderInfoLog(fShader, (GLsizei)infoLogBuf.size(), NULL, &infoLogBuf[0]);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderInfoLog");

            m_testCtx.getLog() << tcu::TestLog::Message << "Fragment shader build failed.\n"
                               << "Fragment: " << infoLogBuf.data() << "\n"
                               << fsSource << "\n"
                               << tcu::TestLog::EndMessage;

            gl.deleteShader(fShader);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");

            TCU_FAIL("Failed to compile transform feedback fragment shader");
        }
        gl.attachShader(m_program, fShader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "attachShader");

        gl.deleteShader(fShader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    const char *outputVaryings[] = {"gl_Position"};
    int varyingcount             = 1;

    gl.transformFeedbackVaryings(m_program, varyingcount, &outputVaryings[0], GL_SEPARATE_ATTRIBS);
    GLU_EXPECT_NO_ERROR(gl.getError(), "transformFeedbackVaryings");

    gl.linkProgram(m_program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "linkProgram");

    gl.getProgramiv(m_program, GL_LINK_STATUS, &status);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getProgramiv");

    if (status == GL_FALSE)
    {
        GLint infoLogLength = 0;
        gl.getProgramiv(m_program, GL_INFO_LOG_LENGTH, &infoLogLength);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getProgramiv");

        std::vector<char> infoLogBuf(infoLogLength + 1);
        gl.getProgramInfoLog(m_program, (GLsizei)infoLogBuf.size(), NULL, infoLogBuf.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "getProgramInfoLog");

        m_testCtx.getLog() << tcu::TestLog::Message << "Fragment shader build failed.\n"
                           << "link log: " << infoLogBuf.data() << "\n"
                           << tcu::TestLog::EndMessage;

        TCU_FAIL("Failed to link transform feedback program");
    }

    gl.useProgram(m_program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TransformFeedbackStatesTestCase::iterate()
{
    if (!m_testSupported)
    {
        throw tcu::NotSupportedError("transform_feedback2 is not supported");
        return STOP;
    }

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool ret                 = true;

    /* setup shader program */
    std::string vshader = tcu::StringTemplate(m_shader_vert).specialize(specializationMap);
    std::string fshader = tcu::StringTemplate(m_shader_frag).specialize(specializationMap);

    {
        ProgramSources sources;
        sources.sources[SHADERTYPE_VERTEX].push_back(vshader);
        sources.sources[SHADERTYPE_FRAGMENT].push_back(fshader);

        ShaderProgram checker_program(gl, sources);

        if (!checker_program.isOk())
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                               << "Vertex: " << checker_program.getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                               << checker_program.getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
                               << "Fragment: " << checker_program.getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                               << checker_program.getShader(SHADERTYPE_FRAGMENT)->getSource() << "\n"
                               << "Program: " << checker_program.getProgramInfo().infoLog << tcu::TestLog::EndMessage;
            TCU_FAIL("Compile failed");
        }
    }

    // fragment shader needed for GLES program linking
    buildTransformFeedbackProgram(vshader.c_str(), m_isContextES ? fshader.c_str() : nullptr);

    GLuint queryresults[2] = {0, 0};
    GLint bbinding         = 0;
    GLint64 bsize = 0, bstart = 0;

    /* Create and bind a user transform feedback object with
        GenTransformFeedbacks and BindTransformFeedback and ensure the test
        runs correctly. Delete the user transform buffer object.

      * Create multiple user transform feedback objects and configure different
        state in each object. The state tested should be the following:

        TRANSFORM_FEEDBACK_BUFFER_BINDING
        TRANSFORM_FEEDBACK_BUFFER_START
        TRANSFORM_FEEDBACK_BUFFER_SIZE
    */
    gl.genTransformFeedbacks(1, &m_tf_id);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");

    gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, m_tf_id);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");

    gl.genBuffers(2, m_buffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

    gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    gl.bufferData(GL_ARRAY_BUFFER, sizeof(vertices), NULL, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, m_buffers[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");

    gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[1]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    gl.bufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    /*Test*/
    gl.getIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &bbinding);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    if (m_isContextES)
    {
        gl.getInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_START, 0, &bstart);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getInteger64i_v");

        gl.getInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 0, &bsize);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getInteger64i_v");
    }
    else
    {
        gl.getTransformFeedbacki64_v(m_tf_id, GL_TRANSFORM_FEEDBACK_BUFFER_START, 0, &bstart);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getTransformFeedbacki64_v");

        gl.getTransformFeedbacki64_v(m_tf_id, GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 0, &bsize);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getTransformFeedbacki64_v");
    }

    if (bbinding >= 0 && ((GLuint)bbinding != m_tf_id) && (bstart != 0) && (bsize != sizeof(vertices)))
    {
        TCU_FAIL("Unexpected state of transform feedback buffer");
    }

    /*
    * Create two query objects and call BeginQuery(PRIMITIVES_GENERATED) and
        BeginQuery(TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN), which can be used
        to determine when feedback is complete.
    */
    gl.genQueries(2, m_queries);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");

    if (!m_isContextES)
    {
        gl.beginQuery(GL_PRIMITIVES_GENERATED, m_queries[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "beginQuery");
    }

    gl.beginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, m_queries[1]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginQuery");

    ret = draw_simple2(m_program, GL_TRIANGLES, 6, true);
    if (!ret)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    if (!m_isContextES)
    {
        gl.endQuery(GL_PRIMITIVES_GENERATED);
        GLU_EXPECT_NO_ERROR(gl.getError(), "endQuery");
    }

    gl.endQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    GLU_EXPECT_NO_ERROR(gl.getError(), "endQuery");

    if (!m_isContextES)
    {
        gl.getQueryObjectuiv(m_queries[0], GL_QUERY_RESULT, &queryresults[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getQueryObjectuiv");
    }

    gl.getQueryObjectuiv(m_queries[1], GL_QUERY_RESULT, &queryresults[1]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getQueryObjectuiv");

    if ((!m_isContextES && queryresults[0] != 5) || queryresults[1] != 2)
    {
        ret = false;

        if (!m_isContextES)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Query result error: " << queryresults[0] << " != 5, "
                               << queryresults[1] << " != 2" << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Query result error: " << queryresults[1] << " != 2"
                               << tcu::TestLog::EndMessage;
        }
    }

    if (ret)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

/* basic drawing function
 MH-2024.05.10: stripped while porting from kc (GTFTransformFeedback2.c; drawsimple2) to vk-gl due to limited use in one test */
bool TransformFeedbackStatesTestCase::draw_simple2(GLuint program, GLenum primitivetype, GLint vertexcount,
                                                   bool pauseresume)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    GLint locVertices = gl.getAttribLocation(program, "in_vertex");
    if (locVertices < 0)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return false;
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

    bool result = true;

    if (!m_isContextES)
    {
        gl.genVertexArrays(1, &m_vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
        gl.bindVertexArray(m_vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");
    }

    gl.clearColor(0.1f, 0.0f, 0.0f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    gl.vertexAttribPointer(locVertices, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    gl.enableVertexAttribArray(locVertices);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

    gl.beginTransformFeedback(primitivetype);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginTransformFeedback");

    if (pauseresume)
    {
        /* Query the transform feedback state for TRANSFORM_FEEDBACK_BUFFER_PAUSED
            and TRANSFORM_FEEDBACK_BUFFER_ACTIVE to verify the state is reflected
            correctly.
        */
        GLboolean paused, active;

        gl.pauseTransformFeedback();
        GLU_EXPECT_NO_ERROR(gl.getError(), "pauseTransformFeedback");

        /* While the transform feedback is paused, verify that drawing with
           incompatible primitives does not produce an error like it would when
           transform feedback is not paused.
        */
        gl.drawArrays(GL_LINES, 0, vertexcount);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

        if (gl.getError() != GL_NO_ERROR)
        {
            result = false;
        }

        gl.getBooleanv(GL_TRANSFORM_FEEDBACK_PAUSED, &paused);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");

        gl.getBooleanv(GL_TRANSFORM_FEEDBACK_ACTIVE, &active);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");

        if (!paused || !active)
        {
            result = false;
        }

        gl.resumeTransformFeedback();
        GLU_EXPECT_NO_ERROR(gl.getError(), "resumeTransformFeedback");

        gl.getBooleanv(GL_TRANSFORM_FEEDBACK_PAUSED, &paused);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");

        gl.getBooleanv(GL_TRANSFORM_FEEDBACK_ACTIVE, &active);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");

        if (paused || !active)
        {
            result = false;
        }
    }

    {
        /* Draw primitives. For Halti only DrawArrays can be used with transform
        feedback; it does not support DrawElements with transform feedback. In
        addition Halti only supports independent primitives (POINTS, LINES and
        TRIANGLES), no primitive restart interaction and no writing of
        gl_Position.
        */
        gl.drawArrays(primitivetype, 0, vertexcount);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

        if (!m_isContextES)
        {
            /* For Halti an overflow while writing out to transform feedback buffers
            generates and GL_INVALID_OPERATION error. Clear out the error
            in case of an overflow.
            */
            gl.getError();
        }
    }

    gl.endTransformFeedback();
    GLU_EXPECT_NO_ERROR(gl.getError(), "endTransformFeedback");

    gl.disableVertexAttribArray(locVertices);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

    if (!m_isContextES)
    {
        gl.bindVertexArray(0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");
    }
    return result;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
TransformFeedbackTests::TransformFeedbackTests(deqp::Context &context)
    : TestCaseGroup(context, "transform_feedback2", "Verify conformance of transform_feedback2 functionality")
{
}

/** Initializes the test group contents. */
void TransformFeedbackTests::init()
{
    addChild(new TransformFeedbackStatesTestCase(m_context));
}

} // namespace glcts
