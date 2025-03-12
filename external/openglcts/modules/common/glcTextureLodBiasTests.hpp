#ifndef _GLCTEXTURELODBIASTESTS_HPP
#define _GLCTEXTURELODBIASTESTS_HPP
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
 * \file  glcTextureLodBiasTests.hpp
 * \brief Conformance tests for texture lod bias feature functionality.
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

class TextureLodBiasAllTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    TextureLodBiasAllTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

private:
    /* Private methods */
    void createRenderingResources();
    bool drawQuad(glw::GLuint, bool, float, float, float, float, float);
    void releaseRenderingResources();
    void setBuffers(const glu::ShaderProgram &program);
    void releaseBuffers();

    /* Private members */
    static const glw::GLchar *m_vert_shader_sampler_vert;
    static const glw::GLchar *m_frag_shader_sampler_vert;

    static const glw::GLchar *m_vert_shader_sampler_frag;
    static const glw::GLchar *m_frag_shader_sampler_frag;

    std::map<std::string, std::string> specializationMap;

    glw::GLuint m_texture;
    glw::GLuint m_target;
    glw::GLuint m_fbo;

    glw::GLuint m_vao;
    glw::GLuint m_vbo;

    bool m_isContextES;
    bool m_testSupported;
    bool m_vertexLookupSupported;

    int m_maxErrorTolerance;
};

/** Test group which encapsulates all conformance tests */
class TextureLodBiasTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    TextureLodBiasTests(deqp::Context &context);

    void init();

private:
    TextureLodBiasTests(const TextureLodBiasTests &other);
    TextureLodBiasTests &operator=(const TextureLodBiasTests &other);
};

} // namespace glcts

#endif // _GLCTEXTURELODBIASTESTS_HPP
