#ifndef _GLCBINDIMAGETEXTURETESTS_HPP
#define _GLCBINDIMAGETEXTURETESTS_HPP
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
 * \file  glcBindImageTextureTests.hpp
 * \brief Conformance tests for binding texture to an image unit operations.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include <map>

namespace glu
{
class ShaderProgram;
}

namespace glcts
{

/** class to test non-layered bindings of shader images */
class BindImageTextureSingleLayerTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    BindImageTextureSingleLayerTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

private:
    /* Private methods */
    void setBuffers(const glu::ShaderProgram &program);
    void releaseBuffers();

    bool drawAndVerify(const glw::GLenum tex_target);

    /* Private members */
    static const glw::GLchar *m_shader_vert;
    static const glw::GLchar *m_shader_frag;
    static const glw::GLchar *m_shader_1d_frag;

    std::map<std::string, std::string> specializationMap;

    glw::GLuint m_vao;
    glw::GLuint m_vbo;

    glw::GLuint m_target;
    glw::GLuint m_fbo;

    bool m_isContextES;
};

/** Test group which encapsulates all conformance tests */
class BindImageTextureTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    BindImageTextureTests(deqp::Context &context);

    void init();

private:
    BindImageTextureTests(const BindImageTextureTests &other);
    BindImageTextureTests &operator=(const BindImageTextureTests &other);
};

} // namespace glcts

#endif // _GLCBINDIMAGETEXTURETESTS_HPP
