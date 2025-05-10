#ifndef _GLCTEXTURESTENCIL8TESTS_HPP
#define _GLCTEXTURESTENCIL8TESTS_HPP
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
 * \file  glcTextureStencil8Tests.hpp
 * \brief Conformance tests for basic texture lod operations.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include <map>
#include <memory>

namespace glu
{
class ShaderProgram;
}

namespace glcts
{

/** class to handle rendering to multisampled texture stencil test, former texture_stencil8_gl44 */
class TextureMultisampledStencilTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    TextureMultisampledStencilTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

private:
    /* Private methods */

    bool testRenderToMultisampledStencilTexture();
    glw::GLuint createForTargetTexImage(glw::GLenum target);
    glw::GLuint createForTargetTexImageWithType(glw::GLenum target, glw::GLenum type);
    glw::GLuint createForTargetTexStorageMultisample(glw::GLenum target, glw::GLint numSamples);

    bool testCreateTexturesMultisample();
    bool createTexturesTexStorageMultisample(glw::GLuint *textures, int numSamples);
    bool checkMultisampledPattern(glw::GLenum texTarget, glw::GLuint texture, glw::GLuint stencilSampleToColorProg);
    bool drawScreenQuad(glw::GLuint program);

    void attachStencilTexture(glw::GLenum fboTarget, glw::GLenum texTarget, glw::GLuint texture);
    void fillStencilSamplePattern(glw::GLuint program);

    /* Private members */
    static const glw::GLchar *m_shader_vert;
    static const glw::GLchar *m_shader_frag;

    std::map<std::string, std::string> specializationMap;

    glw::GLuint m_vao;
    glw::GLuint m_vbo;

    bool m_isContextES;
    bool m_testSupported;

    std::unique_ptr<glu::ShaderProgram> m_stencilToColorProgram;
    std::unique_ptr<glu::ShaderProgram> m_simpleColorProgram;
    std::unique_ptr<glu::ShaderProgram> m_checkStencilSampleProgram;
};

/** Test group which encapsulates all conformance tests */
class TextureStencil8Tests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    TextureStencil8Tests(deqp::Context &context);

    void init();

private:
    TextureStencil8Tests(const TextureStencil8Tests &other);
    TextureStencil8Tests &operator=(const TextureStencil8Tests &other);
};

} // namespace glcts

#endif // _GLCTEXTURESTENCIL8TESTS_HPP
