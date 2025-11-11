#ifndef _GL4CTEXTUREQUERYLEVELSTESTS_HPP
#define _GL4CTEXTUREQUERYLEVELSTESTS_HPP

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
 * \file  gl4cTextureQueryLevelsTests.hpp
 * \brief Texture query levels tests Suite Interface
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"

namespace gl4cts
{

class TextureQueryLevelsBaseTest : public deqp::TestCase
{
public:
    TextureQueryLevelsBaseTest(deqp::Context &context, const std::string &test_name,
                               const std::string &test_description);
    virtual ~TextureQueryLevelsBaseTest() = default;

    tcu::TestNode::IterateResult iterate(void) override;
    virtual bool test();
    virtual void createBuffers();
    virtual void createTexture();
    virtual void clean();
    virtual bool verify();

protected:
    std::string m_vertex_shader_txt;
    std::string m_fragment_shader_txt;
    glw::GLuint m_vbo, m_vao;
    glw::GLuint m_texture;
    glw::GLuint m_width;
    glw::GLuint m_height;
    glw::GLint m_initialLevels;
    glw::GLenum m_textureType;
};

class TextureQueryLevelsSampler1DTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler1DTest(deqp::Context &context, const std::string &test_name,
                                    const std::string &test_description);
    virtual ~TextureQueryLevelsSampler1DTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSampler2DTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler2DTest(deqp::Context &context, const std::string &test_name,
                                    const std::string &test_description);
    virtual ~TextureQueryLevelsSampler2DTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSampler3DTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler3DTest(deqp::Context &context, const std::string &test_name,
                                    const std::string &test_description);
    virtual ~TextureQueryLevelsSampler3DTest() = default;

    void createTexture() override;

private:
    glw::GLuint m_depth;
};

class TextureQueryLevelsSamplerCubeTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSamplerCubeTest(deqp::Context &context, const std::string &test_name,
                                      const std::string &test_description);
    virtual ~TextureQueryLevelsSamplerCubeTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSampler1DArrayTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler1DArrayTest(deqp::Context &context, const std::string &test_name,
                                         const std::string &test_description);
    virtual ~TextureQueryLevelsSampler1DArrayTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSampler2DArrayTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler2DArrayTest(deqp::Context &context, const std::string &test_name,
                                         const std::string &test_description);
    virtual ~TextureQueryLevelsSampler2DArrayTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSamplerCubeArrayTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSamplerCubeArrayTest(deqp::Context &context, const std::string &test_name,
                                           const std::string &test_description);
    virtual ~TextureQueryLevelsSamplerCubeArrayTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSampler1DShadowTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler1DShadowTest(deqp::Context &context, const std::string &test_name,
                                          const std::string &test_description);
    virtual ~TextureQueryLevelsSampler1DShadowTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSampler2DShadowTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler2DShadowTest(deqp::Context &context, const std::string &test_name,
                                          const std::string &test_description);
    virtual ~TextureQueryLevelsSampler2DShadowTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSamplerCubeShadowTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSamplerCubeShadowTest(deqp::Context &context, const std::string &test_name,
                                            const std::string &test_description);
    virtual ~TextureQueryLevelsSamplerCubeShadowTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSampler1DArrayShadowTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler1DArrayShadowTest(deqp::Context &context, const std::string &test_name,
                                               const std::string &test_description);
    virtual ~TextureQueryLevelsSampler1DArrayShadowTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSampler2DArrayShadowTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSampler2DArrayShadowTest(deqp::Context &context, const std::string &test_name,
                                               const std::string &test_description);
    virtual ~TextureQueryLevelsSampler2DArrayShadowTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsSamplerCubeArrayShadowTest : public TextureQueryLevelsBaseTest
{
public:
    TextureQueryLevelsSamplerCubeArrayShadowTest(deqp::Context &context, const std::string &test_name,
                                                 const std::string &test_description);
    virtual ~TextureQueryLevelsSamplerCubeArrayShadowTest() = default;

    void createTexture() override;
};

class TextureQueryLevelsTests : public deqp::TestCaseGroup
{
public:
    TextureQueryLevelsTests(deqp::Context &context);
    ~TextureQueryLevelsTests() = default;

    void init(void);

private:
    TextureQueryLevelsTests(const TextureQueryLevelsTests &other);
    TextureQueryLevelsTests &operator=(const TextureQueryLevelsTests &other);
};
} // namespace gl4cts
#endif // _GL4CTEXTUREQUERYLEVELSTESTS_HPP
