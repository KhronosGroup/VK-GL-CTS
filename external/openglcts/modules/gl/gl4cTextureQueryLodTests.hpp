#ifndef _GL4CTEXTUREQUERYLODTESTS_HPP
#define _GL4CTEXTUREQUERYLODTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \file  gl4cTextureQueryLodTests.hpp
 * \brief Texture query lod tests Suite Interface
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "tcuVectorType.hpp"
#include "gluShaderProgram.hpp"

namespace gl4cts
{

class TextureQueryLodBaseTest : public deqp::TestCase
{
public:
    TextureQueryLodBaseTest(deqp::Context &context, const std::string &test_name, const std::string &test_description);
    virtual ~TextureQueryLodBaseTest() = default;
    tcu::TestNode::IterateResult iterate(void) override;
    virtual bool test();
    virtual void createBuffers();
    virtual void createTexture();
    virtual void clean();
    virtual bool verify();
    tcu::Vec3 calculateExpectedColor(float scale, int textureSize);

protected:
    std::string m_vertex_shader_txt;
    std::string m_fragment_shader_txt;
    glw::GLuint m_vbo, m_vao, m_ebo;
    glw::GLuint m_texture;
    glw::GLuint m_width;
    glw::GLuint m_height;
    glw::GLuint m_viewPortWidth;
    glw::GLuint m_viewPortHeight;
    glw::GLenum m_textureType;
    std::vector<unsigned int> m_indices;
    unsigned int m_scaleLoc;
};

class TextureQueryLodSampler1DTest : public TextureQueryLodBaseTest
{
public:
    TextureQueryLodSampler1DTest(deqp::Context &context, const std::string &test_name,
                                 const std::string &test_description);
    virtual ~TextureQueryLodSampler1DTest() = default;
    void createBuffers() override;
    void createTexture() override;
    bool verify() override;
};

class TextureQueryLodSampler2DTest : public TextureQueryLodBaseTest
{
public:
    TextureQueryLodSampler2DTest(deqp::Context &context, const std::string &test_name,
                                 const std::string &test_description);
    virtual ~TextureQueryLodSampler2DTest() = default;
    void createBuffers() override;
    void createTexture() override;
    bool verify() override;
};

class TextureQueryLodSampler3DTest : public TextureQueryLodBaseTest
{
public:
    TextureQueryLodSampler3DTest(deqp::Context &context, const std::string &test_name,
                                 const std::string &test_description);
    virtual ~TextureQueryLodSampler3DTest() = default;
    void createBuffers() override;
    void createTexture() override;
    bool verify() override;
};

class TextureQueryLodTests : public deqp::TestCaseGroup
{
public:
    TextureQueryLodTests(deqp::Context &context);
    ~TextureQueryLodTests() = default;
    void init(void);

private:
    TextureQueryLodTests(const TextureQueryLodTests &other);
    TextureQueryLodTests &operator=(const TextureQueryLodTests &other);
};
} // namespace gl4cts
#endif // _GL4CTEXTUREQUERYLODTESTS_HPP
