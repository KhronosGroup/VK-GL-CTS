#ifndef _GL3CDRAWBUFFERS_HPP
#define _GL3CDRAWBUFFERS_HPP
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
 * \file  gl3cDrawBuffers.hpp
 * \brief Conformance tests for the DrawBuffers functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "gluShaderProgram.hpp"
#include "glwDefs.hpp"

#include <map>
#include <memory>

namespace gl3cts
{
#define MAX_ATTACHMENTS 16

struct GL3FBOAttachment
{
    glw::GLenum attachment = 0; /* GL_COLOR_ATTACHMENT0, etc. */
    glw::GLenum target     = 0; /* GL_RENDERBUFFER for renderbuffers, GL_TEXTURE_2D or similar for textures etc. */
    glw::GLuint object     = 0;
    glw::GLint level       = 0; /* For texture 2D */
    glw::GLint zoffset     = 0; /* For texture 3D */
};

struct GL3FBO
{
    GL3FBOAttachment attachments[MAX_ATTACHMENTS];

    glw::GLenum target = 0;
    glw::GLuint object;
};

struct GL3RBO
{
    glw::GLenum target         = 0;
    glw::GLenum internalformat = 0;
    glw::GLsizei width         = 0;
    glw::GLsizei height        = 0;
    glw::GLuint object         = 0;
};

/*
    Test 1: FragColor writes to all DrawBuffers except for NONE.
*/

class DrawBuffersTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    DrawBuffersTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

private:
    bool setupProgram(const glw::GLint numAttachments);
    bool setupBuffers(const glw::GLint maxColAtt, GL3RBO *rbo, GL3FBO &fbo);
    bool applyFBO(GL3FBO &fbo);
    bool checkResults(const int DB, const bool bCheckNone);
    void deleteBuffers(const int CA, GL3RBO *rbo, GL3FBO &fbo);

private:
    /* Private members */
    std::unique_ptr<glu::ShaderProgram> m_program;

    std::map<std::string, std::string> specializationMap;

    glw::GLuint m_vao;
    glw::GLuint m_vbo;
};

/** Test group which encapsulates all conformance tests */
class DrawBuffersTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    DrawBuffersTests(deqp::Context &context);

    void init();

private:
    DrawBuffersTests(const DrawBuffersTests &other);
    DrawBuffersTests &operator=(const DrawBuffersTests &other);
};

} // namespace gl3cts

#endif // _GL3CDRAWBUFFERS_HPP
