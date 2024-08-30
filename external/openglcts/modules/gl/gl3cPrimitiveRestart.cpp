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
 * \file  glcPrimitiveRestartTests.cpp
 * \brief Conformance tests for the primitive restart mode functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "gl3cPrimitiveRestart.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuMatrixUtil.hpp"

#include <cstdint>

using namespace glw;
using namespace glu;

namespace
{

void ReadScreen(const glw::Functions &gl, GLint x, GLint y, GLsizei w, GLsizei h, GLenum type, GLubyte *buf)
{
    long repeat = 1;

    switch (type)
    {
    case GL_ALPHA:
    case GL_LUMINANCE:
        repeat = 1;
        break;
    case GL_LUMINANCE_ALPHA:
        repeat = 2;
        break;
    case GL_RGB:
        repeat = 3;
        break;
    case GL_RGBA:
    case GL_BGRA_EXT:
        repeat = 4;
        break;
    }

    memset(buf, 0, sizeof(GLubyte) * w * h * repeat);

    gl.pixelStorei(GL_PACK_ALIGNMENT, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "pixelStorei");

    gl.readPixels(x, y, w, h, type, GL_UNSIGNED_BYTE, (void *)buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");
}

int map_coord(float c, std::uint32_t N)
{
    if ((c < 0.0f) || (c > 1.0f))
    {
        return -1;
    }
    return (int)(0.5f + (c * (N - 1)));
}

bool checkPixel(const GLint y, const GLint x, const GLuint buf_w, GLuint buf_h, const GLubyte *buf, const GLubyte *ref,
                const GLubyte tolerance)
{
    if (GLuint(y) > buf_h)
        return false;
    int index = (buf_w * y + x) * 4;
    if ((abs((int)buf[index] - (int)ref[0]) <= tolerance) && (abs((int)buf[index + 1] - (int)ref[1]) <= tolerance) &&
        (abs((int)buf[index + 2] - (int)ref[2]) <= tolerance))
        return true;
    return false;
};

bool testSpotLine(const GLint screen_y, const GLint screen_x, const GLuint buf_w, GLuint buf_h, const GLubyte *buf,
                  const GLubyte *ref)
{
    if (checkPixel(screen_y, screen_x, buf_w, buf_h, buf, ref, TEST_TOLERANCE))
        return true;

    /* OpenGL line rasterization rules state that results "may not deviate by
       more than one unit in either x or y window coordinates from a
       corresponding fragment produced by the diamond-exit rule", so we need
       to search for a hit compared to our expected result. */
    int points[4][2] = {
        {screen_x - 1, screen_y},
        {screen_x + 1, screen_y},
        {screen_x, screen_y - 1},
        {screen_x, screen_y + 1},
    };
    for (int pointIdx = 0; pointIdx < 4; ++pointIdx)
    {
        if (points[pointIdx][0] >= 0 && points[pointIdx][0] < (GLint)buf_w && points[pointIdx][1] >= 0 &&
            points[pointIdx][1] < (GLint)buf_h &&
            checkPixel(points[pointIdx][1], points[pointIdx][0], buf_w, buf_h, buf, ref, TEST_TOLERANCE))
        {
            return true;
        }
    }

    return false;
}

const GLfloat vertices[] = {
    /* quad vertices */
    1.0,
    -1.0,
    -2.0,
    1.0,
    1.0,
    -2.0,
    -1.0,
    -1.0,
    -2.0,
    -1.0,
    1.0,
    -2.0,

    /* mid-screen point for LINES primitives */
    0.0,
    0.0,
    -2.0,

    /* spot-check points for POINTS */
    0.5,
    -0.5,
    -2.0,
    -0.5,
    0.5,
    -2.0,

    /* extra vertices for triple-check */
    1.0,
    0.5,
    -2.0,
    0.5,
    1.0,
    -2.0,

    1.0,
    0.0,
    -2.0,
    -1.0,
    0.0,
    -2.0,
    0.0,
    1.0,
    -2.0,
    0.0,
    -1.0,
    -2.0,

    /* duplicated quad for *BaseVertex calls */
    1.0,
    -1.0,
    -2.0,
    1.0,
    1.0,
    -2.0,
    -1.0,
    -1.0,
    -2.0,
    -1.0,
    1.0,
    -2.0,
};

GLuint triangles[]             = {0, 1, 2, RESTART_INDEX, 2, 1, 3, TERMINATOR_INDEX};
GLuint const reset_triangles[] = {0, 1, 2, RESTART_INDEX, 2, 1, 3, TERMINATOR_INDEX};
const GLubyte gc_white[]       = {255, 255, 255};

} // anonymous namespace

namespace gl3cts
{

/** @brief Vertex shader source code to test primitive restart functionality. */
const glw::GLchar *gl3cts::PrimitiveRestartModeTestCase::m_vert_shader = R"(
    ${VERSION}
    uniform mat4 ModelViewProjectionMatrix;
    uniform vec4 testcolor;

    in vec4 vertex;
    out vec4 color;

    void main (void)
    {
            color = testcolor;
            gl_Position = ModelViewProjectionMatrix * vertex;
            gl_PointSize = 4.0;
    }
    )";

/** @brief Fragment shader source code to test primitive restart functionality. */
const glw::GLchar *gl3cts::PrimitiveRestartModeTestCase::m_frag_shader = R"(
    ${VERSION}
    in vec4 color;
    out vec4 frag_color;

    void main()
    {
        frag_color = color;
    }
    )";

/** @brief Vertex shader source code to test primitive restart functionality. */
const glw::GLchar *gl3cts::PrimitiveRestartModeTestCase::m_tess_vert_shader = R"(
    ${VERSION}
    uniform mat4 ModelViewProjectionMatrix;
    uniform vec4 testcolor;

    in vec4 vertex;
    out vec4 color;

    void main (void)
    {
            color = testcolor;
            gl_Position = ModelViewProjectionMatrix * vertex;
            gl_PointSize = 1.0;
    }
    )";

/** @brief Control shader source code to test primitive restart functionality. */
const glw::GLchar *gl3cts::PrimitiveRestartModeTestCase::m_tess_ctrl_shader = R"(
    ${VERSION}
    #extension GL_ARB_tessellation_shader : require
    #define ID gl_InvocationID

    in vec4 color[];

    layout(vertices = 3) out;

    out vec4 frontColor[3];

    void main()
    {
            gl_out[ID].gl_Position = gl_in[ID].gl_Position;
            frontColor[ID] = color[ID];

            gl_TessLevelInner[0] = 0;
            gl_TessLevelOuter[0] = 1;
            gl_TessLevelOuter[1] = 1;
            gl_TessLevelOuter[2] = 1;
    }
    )";

/** @brief Evaluation shader source code to test primitive restart functionality. */
const glw::GLchar *gl3cts::PrimitiveRestartModeTestCase::m_tess_eval_shader = R"(
    ${VERSION}
    #extension GL_ARB_tessellation_shader : require

    uniform mat4 ModelViewMatrix;

    in vec4 frontColor[gl_MaxPatchVertices];

    out vec4 color;

    layout(triangles, equal_spacing) in;

    void main()
    {
            color = vec4(
                gl_TessCoord.x * frontColor[0].xyz +
                gl_TessCoord.y * frontColor[1].xyz +
                gl_TessCoord.z * frontColor[2].xyz,
                1.0);
            gl_Position = ModelViewMatrix * vec4(
                gl_TessCoord.x * gl_in[0].gl_Position.xyz +
                gl_TessCoord.y * gl_in[1].gl_Position.xyz +
                gl_TessCoord.z * gl_in[2].gl_Position.xyz,
                1.0);
    }
    )";

/** Constructor.
 *
 *  @param context     Rendering context
 */
PrimitiveRestartModeTestCase::PrimitiveRestartModeTestCase(deqp::Context &context)
    : TestCase(context, "restart_mode", "Verifies primitive restart mode functionality")
    , m_vao(0)
    , numSpots(3)
    , expectedError(GL_NO_ERROR)
    , isLineTest(false)
    , active_program(0)
    , restartIndex(RESTART_INDEX)
    , locPositions(0)
    , verticesSize(0)
{
    for (GLuint i = 0; i < BUFFER_QUANTITY; i++)
        bufferObjects[i] = 0;

    defaultSpots[0].u = defaultSpots[0].v = 2.0f / 4.0f;
    defaultSpots[1].u = defaultSpots[2].v = 1.0f / 4.0f;
    defaultSpots[2].u = defaultSpots[1].v = 3.0f / 4.0f;
    defaultSpots[0].rgb = defaultSpots[1].rgb = defaultSpots[2].rgb = gc_white;
    spots                                                           = defaultSpots;
    numSpots                                                        = 3;
    expectedError                                                   = GL_NO_ERROR;
    isLineTest                                                      = false;
    verticesSize                                                    = sizeof(vertices);
}

/** Stub deinit method. */
void PrimitiveRestartModeTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void PrimitiveRestartModeTestCase::init()
{
    glu::GLSLVersion glslVersion = glu::getContextTypeGLSLVersion(m_context.getRenderContext().getType());

    specializationMap["VERSION"] = glu::getGLSLVersionDeclaration(glslVersion);

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    auto make_program = [&](const char *vs, const char *fs, const char *cs, const char *es)
    {
        /* Building basic program. */
        std::string vert_shader = tcu::StringTemplate(vs).specialize(specializationMap);
        std::string frag_shader = tcu::StringTemplate(fs).specialize(specializationMap);

        ProgramSources sources;
        sources.sources[SHADERTYPE_VERTEX].push_back(vert_shader);
        sources.sources[SHADERTYPE_FRAGMENT].push_back(frag_shader);

        if (cs)
        {
            sources.sources[SHADERTYPE_TESSELLATION_CONTROL].push_back(
                tcu::StringTemplate(cs).specialize(specializationMap));
        }

        if (es)
        {
            sources.sources[SHADERTYPE_TESSELLATION_EVALUATION].push_back(
                tcu::StringTemplate(es).specialize(specializationMap));
        }

        auto program = new ShaderProgram(gl, sources);

        if (!program->isOk())
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                               << "Vertex: " << program->getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                               << vert_shader << "\n"
                               << tcu::TestLog::EndMessage;
            if (cs)
                m_testCtx.getLog() << tcu::TestLog::Message
                                   << "Control: " << program->getShaderInfo(SHADERTYPE_TESSELLATION_CONTROL).infoLog
                                   << "\n"
                                   << sources.sources[SHADERTYPE_TESSELLATION_CONTROL].back() << "\n"
                                   << tcu::TestLog::EndMessage;
            if (es)
                m_testCtx.getLog() << tcu::TestLog::Message << "Evaluation: "
                                   << program->getShaderInfo(SHADERTYPE_TESSELLATION_EVALUATION).infoLog << "\n"
                                   << sources.sources[SHADERTYPE_TESSELLATION_EVALUATION].back() << "\n"
                                   << tcu::TestLog::EndMessage;
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "Fragment: " << program->getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                               << frag_shader << "\n"
                               << "Program: " << program->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
            delete program;
            TCU_FAIL("Invalid program");
        }
        return program;
    };

    m_program.reset(make_program(m_vert_shader, m_frag_shader, nullptr, nullptr));
    auto contextType = m_context.getRenderContext().getType();
    if (glu::contextSupports(contextType, glu::ApiType::core(4, 4)))
    {
        m_tess_program.reset(make_program(m_tess_vert_shader, m_frag_shader, m_tess_ctrl_shader, m_tess_eval_shader));
    }
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult PrimitiveRestartModeTestCase::iterate()
{
    bool result = true;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    const GLuint indicesPoints[]             = {5, RESTART_INDEX, 6, RESTART_INDEX, 4, TERMINATOR_INDEX};
    const GLuint indicesLines[]              = {4, 3, RESTART_INDEX, 4, 0, TERMINATOR_INDEX};
    const GLuint indicesLinesAdjacency[]     = {4, 4, 3, 3, RESTART_INDEX, 4, 4, 0, 0, TERMINATOR_INDEX};
    const GLuint indicesTrianglesAdjacency[] = {0, 0, 1, 1, 2, 2, RESTART_INDEX, 2, 2, 1, 1, 3, 3, TERMINATOR_INDEX};

    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    active_program = m_program->getProgram();

    result &= testDrawElements(GL_POINTS, indicesPoints, "POINTS");
    result &= testDrawElements(GL_LINES, indicesLines, "LINES");
    result &= testDrawElements(GL_LINE_STRIP, indicesLines, "LINE_STRIP");
    result &= testDrawElements(GL_LINE_LOOP, indicesLines, "LINE_LOOP");
    result &= testDrawElements(GL_TRIANGLE_STRIP, triangles, "TRIANGLE_STRIP");
    result &= testDrawElements(GL_TRIANGLE_FAN, triangles, "TRIANGLE_FAN");
    result &= testDrawElements(GL_TRIANGLES, triangles, "TRIANGLES");

    auto contextType = m_context.getRenderContext().getType();
    if (glu::contextSupports(contextType, glu::ApiType::core(3, 2))) /* core geometry shader support */
    {
        result &= testDrawElements(GL_LINES_ADJACENCY, indicesLinesAdjacency, "LINES_ADJACENCY");
        result &= testDrawElements(GL_LINE_STRIP_ADJACENCY, indicesLinesAdjacency, "LINE_STRIP_ADJACENCY");
        result &= testDrawElements(GL_TRIANGLES_ADJACENCY, indicesTrianglesAdjacency, "TRIANGLES_ADJACENCY");
        result &= testDrawElements(GL_TRIANGLE_STRIP_ADJACENCY, indicesTrianglesAdjacency, "TRIANGLE_STRIP_ADJACENCY");
    }

    if (glu::contextSupports(contextType, glu::ApiType::core(4, 4)) && m_tess_program.get() != nullptr)
    {
        GLboolean primitiveRestartForPatchesSupported = GL_FALSE;
        gl.getBooleanv(GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED, &primitiveRestartForPatchesSupported);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");

        if (primitiveRestartForPatchesSupported)
        {
            active_program = m_tess_program->getProgram();

            result &= testDrawElements(GL_PATCHES, triangles, "PATCHES");
        }
    }

    if (result)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

bool PrimitiveRestartModeTestCase::testDrawElements(const GLenum mode, const GLuint *indices, const char *errorMessage)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int numIndices     = getIndicesLength(indices);

    bool result = true;

    isLineTest = mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP || mode == GL_LINES_ADJACENCY ||
                 mode == GL_LINE_STRIP_ADJACENCY;

    initDraw(RESTART_INDEX, indices, numIndices * sizeof(GLuint));

    gl.drawElements(mode, numIndices, GL_UNSIGNED_INT, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawElements");

    if (testApply() == false)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Test fail: \n"
                           << errorMessage << "\n"
                           << tcu::TestLog::EndMessage;

        result = false;
    }

    uninitDraw();

    return result;
}

GLuint PrimitiveRestartModeTestCase::getIndicesLength(const GLuint *indices)
{
    int indexCount = 0;
    while (indices[indexCount] != TERMINATOR_INDEX)
        indexCount++;
    return indexCount;
}

void PrimitiveRestartModeTestCase::initDraw(GLuint restart_ind, const GLuint *indices, const GLuint indices_size)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    /* NOTE: Same data is used for both DrawArraysIndirect() and DrawElementsIndirect() */
    const GLuint defaultIndirectCommand[] = {
        getIndicesLength(triangles), /* GLuint count */
        1,                           /* GLuint primCount; */
        0,                           /* GLuint firstIndex; */
        0,                           /* GLint  baseVertex; */
        0                            /* GLuint reservedMustBeZero; */
    };

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    gl.genVertexArrays(1, &m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");

    gl.bindVertexArray(m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    /* Setup primitive restart */
    setRestartIndex(restart_ind);

    gl.enable(GL_PRIMITIVE_RESTART);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    gl.primitiveRestartIndex(restart_ind);
    GLU_EXPECT_NO_ERROR(gl.getError(), "primitiveRestartIndex");

    /* Setup POINTS: point size, no smooth */
    gl.pointSize(POINT_SIZE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "pointSize");

    gl.disable(GL_LINE_SMOOTH);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    /* Use appropriate shader program */
    gl.useProgram(active_program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

    const GLint locModelViewProj = gl.getUniformLocation(active_program, "ModelViewProjectionMatrix");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
    if (locModelViewProj != -1)
    {
        auto mat = tcu::ortho2DMatrix<GLfloat, 4, 4>(-1.0, 1.0f, -1.0f, 1.0f, -1.0f, -30.0f);
        gl.uniformMatrix4fv(locModelViewProj, 1, 0, mat.getRowMajorData().getPtr());
        GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix4fv");
    }

    const GLint locModelView = gl.getUniformLocation(active_program, "ModelViewMatrix");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
    if (locModelView != -1)
    {
        tcu::Matrix4f mat(tcu::Vec4(1.f, 1.f, 1.f, 1.f));
        gl.uniformMatrix4fv(locModelView, 1, 0, mat.getRowMajorData().getPtr());
        GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix4fv");
    }

    const GLint locTestColor = gl.getUniformLocation(active_program, "testcolor");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
    if (locTestColor != -1)
    {
        tcu::Vec4 color = tcu::Vec4(1.f, 1.f, 1.f, 1.f);
        gl.uniform4fv(locTestColor, 1, color.getPtr());
        GLU_EXPECT_NO_ERROR(gl.getError(), "uniform4fv");
    }

    /* Setup vertex BO */
    gl.genBuffers(BUFFER_QUANTITY, bufferObjects);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

    gl.bindBuffer(GL_ARRAY_BUFFER, bufferObjects[BUFFER_ARRAY]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    gl.bufferData(GL_ARRAY_BUFFER, verticesSize, vertices, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    /* Setup vertex attribs */
    locPositions = gl.getAttribLocation(active_program, "vertex");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");
    if (locPositions != -1)
    {
        gl.enableVertexAttribArray(locPositions);
        GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
        gl.vertexAttribPointer(locPositions, 3, GL_FLOAT, GL_FALSE, 0, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    }

    /* Setup element BO */
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[BUFFER_ELEMENT]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    /* Setup indirect BO */
    if (m_context.getContextInfo().isExtensionSupported("GL_ARB_draw_indirect"))
    {
        gl.bindBuffer(GL_DRAW_INDIRECT_BUFFER, bufferObjects[BUFFER_INDIRECT]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
        gl.bufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(defaultIndirectCommand), defaultIndirectCommand, GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");
    }
}

void PrimitiveRestartModeTestCase::uninitDraw()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    /* Default setup for VBO */
    gl.bindBuffer(GL_ARRAY_BUFFER, bufferObjects[BUFFER_ARRAY]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    gl.disableVertexAttribArray(locPositions);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    if (m_context.getContextInfo().isExtensionSupported("GL_ARB_draw_indirect"))
    {
        gl.bindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
    }

    gl.deleteBuffers(BUFFER_QUANTITY, bufferObjects);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

    /* Default setup of POINTS: point size=1, no smooth */
    gl.pointSize(1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "pointSize");

    gl.disable(GL_LINE_SMOOTH);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

    /* Default setup for primitive restart */
    setRestartIndex(RESTART_INDEX);

    gl.primitiveRestartIndex(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "primitiveRestartIndex");

    gl.disable(GL_PRIMITIVE_RESTART);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    for (size_t i = 0; i < sizeof(triangles) / sizeof(triangles[0]); ++i)
    {
        triangles[i] = reset_triangles[i];
    }

    if (m_vao)
    {
        gl.deleteVertexArrays(1, &m_vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");
        m_vao = 0;
    }
}

void PrimitiveRestartModeTestCase::setRestartIndex(GLuint newRestartIndex)
{
    const int length = getIndicesLength(triangles);
    int i;
    if (newRestartIndex == TERMINATOR_INDEX)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "setRestartIndex(): invalid newRestartIndex..\n"
                           << tcu::TestLog::EndMessage;

        return;
    }
    for (i = 0; i < length; i++)
    {
        if (triangles[i] == restartIndex)
        {
            triangles[i] = newRestartIndex;
        }
    }
    restartIndex = newRestartIndex;
}

bool PrimitiveRestartModeTestCase::GL3AssertError(const GLenum exp_error)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    const GLenum error = gl.getError();
    if (error == exp_error)
        return true;

    m_testCtx.getLog() << tcu::TestLog::Message << "GL3AssertError:expected " << glu::getErrorName(exp_error)
                       << " but got " << glu::getErrorName(error) << tcu::TestLog::EndMessage;

    return false;
}

bool PrimitiveRestartModeTestCase::testSpots(const GLubyte *buf, const GLuint buf_w, const GLuint buf_h)
{
    bool result = true;
    int screenX, screenY;
    const Spot *spot = nullptr;

    for (GLuint spotIdx = 0; spotIdx < numSpots; spotIdx++)
    {
        spot = &spots[spotIdx];
        if (spot->rgb != NULL)
        {
            screenX = map_coord(spot->u, buf_w);
            screenY = map_coord(spot->v, buf_h);
            if (screenX == -1)
            {
                m_testCtx.getLog() << tcu::TestLog::Message << "GL3SpotTestApply:Out of range [0,1] spot u coordinate."
                                   << tcu::TestLog::EndMessage;
                return false;
            }
            if (screenY == -1)
            {
                m_testCtx.getLog() << tcu::TestLog::Message << "GL3SpotTestApply:Out of range [0,1] spot v coordinate."
                                   << tcu::TestLog::EndMessage;
                return false;
            }

            if (isLineTest)
                result &= testSpotLine(screenY, screenX, buf_w, buf_h, buf, spot->rgb);
            else
                result &= checkPixel(screenY, screenX, buf_w, buf_h, buf, spot->rgb, TEST_TOLERANCE);
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "GL3SpotTestApply:Spot " << spotIdx
                               << " skipped. RGB was NULL." << tcu::TestLog::EndMessage;
        }
    }

    return result;
}

bool PrimitiveRestartModeTestCase::testApply()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    bool result = GL3AssertError(expectedError);

    if (true == result)
    {
        if (0 == numSpots)
        {
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "GL3SpotTestApply:Number of spots is 0. No spot testing done (result=PASS)."
                               << tcu::TestLog::EndMessage;
        }
        else
        {
            GLuint screen_w = m_context.getRenderTarget().getWidth();
            GLuint screen_h = m_context.getRenderTarget().getHeight();

            std::vector<GLubyte> buf(screen_w * screen_h * 4, '\0');
            ReadScreen(gl, 0, 0, screen_w, screen_h, GL_RGBA, buf.data());
            result = testSpots(buf.data(), screen_w, screen_h);
        }
    }

    m_context.getRenderContext().postIterate();

    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    return result;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
PrimitiveRestartTests::PrimitiveRestartTests(deqp::Context &context)
    : TestCaseGroup(context, "primitive_restart", "Verify conformance of primitive restart implementation")
{
}

/** Initializes the test group contents. */
void PrimitiveRestartTests::init()
{
    addChild(new PrimitiveRestartModeTestCase(m_context));
}

} // namespace gl3cts
