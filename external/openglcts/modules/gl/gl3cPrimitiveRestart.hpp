#ifndef _GL3CPRIMITIVERESTART_HPP
#define _GL3CPRIMITIVERESTART_HPP
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
 * \file  glcPrimitiveRestartTests.hpp
 * \brief Conformance tests for primitive restart feature functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "gluShaderProgram.hpp"
#include "glwDefs.hpp"

#include <map>
#include <memory>

#define RESTART_INDEX 0xFFFFFFFF
/* WARNING: Do not set TERMINATOR_INDEX = RESTART_INDEX */
#define TERMINATOR_INDEX 0x77777777
#define POINT_SIZE 4.0f
#define TEST_TOLERANCE 24

namespace gl3cts
{

enum BufferObjectEnum
{
    BUFFER_ARRAY    = 0,
    BUFFER_ELEMENT  = 1,
    BUFFER_INDIRECT = 2,
    BUFFER_QUANTITY = 3
};

struct Spot
{
    glw::GLfloat u, v;
    const glw::GLubyte *rgb;
};

/*
SPECIFICATION:
2.2. Verify each primitive type. Perform default test method for all
applicable primitive types.
For OpenGL 2.0 + NV check only a core profile subset of available types.
Insert a restart index between primitives: point, line, triangle.
Insert a restart index in the middle of the primitive: line strip, line loop,
triangle fan, triangle strip to render 2 primitives.

IMPLEMENTATION NOTES:
- See testDrawAllPrimitives()
*/

class PrimitiveRestartModeTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    PrimitiveRestartModeTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

private:
    /* Private methods */
    bool testDrawElements(const glw::GLenum, const glw::GLuint *, const char *);
    glw::GLuint getIndicesLength(const glw::GLuint *indices);
    void setRestartIndex(glw::GLuint newRestartIndex);
    bool GL3AssertError(glw::GLenum expectedError);
    bool testSpots(const glw::GLubyte *buf, const glw::GLuint buf_w, const glw::GLuint buf_h);

    void initDraw(glw::GLuint, const glw::GLuint *, const glw::GLuint);
    void uninitDraw();
    bool testApply();

private:
    /* Private members */
    static const glw::GLchar *m_vert_shader;
    static const glw::GLchar *m_frag_shader;

    static const glw::GLchar *m_tess_vert_shader;
    static const glw::GLchar *m_tess_ctrl_shader;
    static const glw::GLchar *m_tess_eval_shader;

    glw::GLuint m_vao;

    std::map<std::string, std::string> specializationMap;

    std::unique_ptr<glu::ShaderProgram> m_program;
    std::unique_ptr<glu::ShaderProgram> m_tess_program;

    const Spot *spots;
    glw::GLuint numSpots;
    glw::GLenum expectedError;
    bool isLineTest;
    Spot defaultSpots[3];
    glw::GLuint active_program;
    glw::GLuint restartIndex;
    glw::GLuint bufferObjects[BUFFER_QUANTITY];
    glw::GLint locPositions;
    glw::GLuint verticesSize;
};

/** Test group which encapsulates all conformance tests */
class PrimitiveRestartTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    PrimitiveRestartTests(deqp::Context &context);

    void init();

private:
    PrimitiveRestartTests(const PrimitiveRestartTests &other);
    PrimitiveRestartTests &operator=(const PrimitiveRestartTests &other);
};

} // namespace gl3cts

#endif // _GL3CPRIMITIVERESTART_HPP
