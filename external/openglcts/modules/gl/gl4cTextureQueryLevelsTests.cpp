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
 * \file  gl4cTextureQueryLevelsTests.cpp
 * \brief Implements conformance texture query levels tests
 */ /*-------------------------------------------------------------------*/

#include "gl4cTextureQueryLevelsTests.hpp"

#include "tcuStringTemplate.hpp"
#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"
#include "glwEnums.hpp"

#include <cmath>

namespace gl4cts
{

TextureQueryLevelsBaseTest::TextureQueryLevelsBaseTest(deqp::Context &context, const std::string &test_name,
                                                       const std::string &test_description)
    : TestCase(context, test_name.c_str(), test_description.c_str())
    , m_vertex_shader_txt("")
    , m_fragment_shader_txt("")
    , m_vbo(0)
    , m_vao(0)
    , m_texture(0)
    , m_width(32)
    , m_height(32)
    , m_initialLevels(0)
{
    m_vertex_shader_txt = R"(
    #version 400 core
    layout (location = 0) in vec3 aPos;
    void main()
    {
        gl_PointSize = 10.0f;
        gl_Position = vec4(aPos, 1.0);
    }
    )";

    m_fragment_shader_txt = R"(
    #version 400 core
    #extension GL_ARB_texture_query_levels: require
    out vec4 FragColor;
    uniform ${sampler} texture;
    uniform int expectedValue;

    void main()
    {
        int levels = textureQueryLevels(texture);
        if(levels == expectedValue)
        {
            FragColor = vec4(0.0, 1.0, 0.0, 1.0);//green
        }
        else
        {
            FragColor = vec4(1.0, 0.0, 0.0, 1.0);//red
        }
    }
    )";
}

tcu::TestNode::IterateResult TextureQueryLevelsBaseTest::iterate(void)
{
    bool texture_query_levels_supported =
        m_context.getContextInfo().isExtensionSupported("GL_ARB_texture_query_levels");

    if (!texture_query_levels_supported)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
        return STOP;
    }

    bool is_ok = false;

    createBuffers();
    createTexture();
    is_ok = test();
    clean();

    if (is_ok)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }

    return STOP;
}

bool TextureQueryLevelsBaseTest::test()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    bool result       = false;
    int expectedValue = m_initialLevels;

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glClear failed");
    gl.viewport(0, 0, m_width, m_height);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport failed");

    glu::ShaderProgram shaderProgram(m_context.getRenderContext(),
                                     glu::makeVtxFragSources(m_vertex_shader_txt, m_fragment_shader_txt));

    gl.useProgram(shaderProgram.getProgram());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram failed");
    gl.uniform1i(gl.getUniformLocation(shaderProgram.getProgram(), "texture"), 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");
    gl.uniform1i(gl.getUniformLocation(shaderProgram.getProgram(), "expectedValue"), expectedValue);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");

    gl.enable(GL_PROGRAM_POINT_SIZE);
    gl.drawArrays(GL_POINTS, 0, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays failed");
    result = verify();

    gl.texParameteri(m_textureType, GL_TEXTURE_BASE_LEVEL, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_MAX_LEVEL, m_initialLevels - 2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    expectedValue = m_initialLevels - 1;
    gl.uniform1i(gl.getUniformLocation(shaderProgram.getProgram(), "expectedValue"), expectedValue);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");

    gl.drawArrays(GL_POINTS, 0, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays failed");
    result &= verify();

    gl.texParameteri(m_textureType, GL_TEXTURE_BASE_LEVEL, 2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_MAX_LEVEL, 1000);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    expectedValue = m_initialLevels - 2;
    gl.uniform1i(gl.getUniformLocation(shaderProgram.getProgram(), "expectedValue"), expectedValue);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");

    gl.drawArrays(GL_POINTS, 0, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays failed");
    result &= verify();

    return result;
}

bool TextureQueryLevelsBaseTest::verify()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool result              = false;

    std::vector<unsigned char> pixel(4);

    gl.readPixels((m_width / 2), (m_height / 2), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels failed");

    if (pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255)
    {
        result = true;
    }
    else if (pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255)
    {
        result = false;
    }
    return result;
}

void TextureQueryLevelsBaseTest::createTexture()
{
}

void TextureQueryLevelsBaseTest::createBuffers()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    std::vector<float> vertices = {0.0f, 0.0f, 0.0f};

    gl.genBuffers(1, &m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers failed");
    gl.genVertexArrays(1, &m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenVertexArrays failed");

    gl.bindVertexArray(m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray failed");
    gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer failed");
    gl.bufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData failed");

    gl.vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer failed");
    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray failed");
}

void TextureQueryLevelsBaseTest::clean()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (m_vao != 0)
    {
        gl.deleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0)
    {
        gl.deleteBuffers(1, &m_vbo);
    }
    if (m_texture != 0)
    {
        gl.deleteTextures(1, &m_texture);
    }
}

TextureQueryLevelsSampler1DTest::TextureQueryLevelsSampler1DTest(deqp::Context &context, const std::string &test_name,
                                                                 const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler1D";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_height        = 1;
    m_initialLevels = 1 + static_cast<int>(floor(std::log2(m_width)));
    m_textureType   = GL_TEXTURE_1D;
}

void TextureQueryLevelsSampler1DTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    std::vector<unsigned char> textureData(m_width * 4, 255);
    gl.texImage1D(m_textureType, 0, GL_RGB, m_width, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSampler2DTest::TextureQueryLevelsSampler2DTest(deqp::Context &context, const std::string &test_name,
                                                                 const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())
{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler2D";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(std::max(m_width, m_height))));
    m_textureType   = GL_TEXTURE_2D;
}

void TextureQueryLevelsSampler2DTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    std::vector<unsigned char> textureData(m_width * m_height * 3, 255);
    gl.texImage2D(m_textureType, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSampler3DTest::TextureQueryLevelsSampler3DTest(deqp::Context &context, const std::string &test_name,
                                                                 const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())
    , m_depth(32)
{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler3D";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(std::max(m_width, std::max(m_height, m_depth)))));
    m_textureType   = GL_TEXTURE_3D;
}

void TextureQueryLevelsSampler3DTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    std::vector<unsigned char> textureData(m_width * m_height * m_depth * 3, 255);
    gl.texImage3D(m_textureType, 0, GL_RGB, m_width, m_height, m_depth, 0, GL_RGB, GL_UNSIGNED_BYTE,
                  textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSamplerCubeTest::TextureQueryLevelsSamplerCubeTest(deqp::Context &context,
                                                                     const std::string &test_name,
                                                                     const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())
{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "samplerCube";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(m_width)));
    m_textureType   = GL_TEXTURE_CUBE_MAP;
}

void TextureQueryLevelsSamplerCubeTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    std::vector<unsigned char> textureData(m_width * m_height * 3, 255);
    for (int i = 0; i < 6; ++i)
    {
        gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                      textureData.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D failed");
    }
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSampler1DArrayTest::TextureQueryLevelsSampler1DArrayTest(deqp::Context &context,
                                                                           const std::string &test_name,
                                                                           const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler1DArray";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_height        = 1;
    m_initialLevels = 1 + static_cast<int>(floor(std::log2(m_width)));
    m_textureType   = GL_TEXTURE_1D_ARRAY;
}

void TextureQueryLevelsSampler1DArrayTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    int layers = 4;
    std::vector<unsigned char> textureData(m_width * layers * 3, 255);
    gl.texImage2D(m_textureType, 0, GL_RGB, m_width, layers, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSampler2DArrayTest::TextureQueryLevelsSampler2DArrayTest(deqp::Context &context,
                                                                           const std::string &test_name,
                                                                           const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler2DArray";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(std::max(m_width, m_height))));
    m_textureType   = GL_TEXTURE_2D_ARRAY;
}

void TextureQueryLevelsSampler2DArrayTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    int layers = 4;
    std::vector<unsigned char> textureData(m_width * m_height * layers * 3, 255);
    gl.texImage3D(m_textureType, 0, GL_RGB, m_width, m_height, layers, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSamplerCubeArrayTest::TextureQueryLevelsSamplerCubeArrayTest(deqp::Context &context,
                                                                               const std::string &test_name,
                                                                               const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "samplerCubeArray";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(m_width)));
    m_textureType   = GL_TEXTURE_CUBE_MAP_ARRAY;
}

void TextureQueryLevelsSamplerCubeArrayTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    int layers = 2;
    std::vector<unsigned char> textureData(m_width * m_height * layers * 6 * 3, 255);
    gl.texImage3D(m_textureType, 0, GL_RGB, m_width, m_height, layers * 6, 0, GL_RGB, GL_UNSIGNED_BYTE,
                  textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSampler1DShadowTest::TextureQueryLevelsSampler1DShadowTest(deqp::Context &context,
                                                                             const std::string &test_name,
                                                                             const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler1DShadow";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_height        = 1;
    m_initialLevels = 1 + static_cast<int>(floor(std::log2(m_width)));
    m_textureType   = GL_TEXTURE_1D;
}

void TextureQueryLevelsSampler1DShadowTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    std::vector<float> textureData(m_width /** m_height*/, 1.0f);
    gl.texImage1D(m_textureType, 0, GL_DEPTH_COMPONENT16, m_width, 0, GL_DEPTH_COMPONENT, GL_FLOAT, textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSampler2DShadowTest::TextureQueryLevelsSampler2DShadowTest(deqp::Context &context,
                                                                             const std::string &test_name,
                                                                             const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler2DShadow";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(std::max(m_width, m_height))));
    m_textureType   = GL_TEXTURE_2D;
}

void TextureQueryLevelsSampler2DShadowTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    std::vector<float> textureData(m_width * m_height, 1.0f);
    gl.texImage2D(m_textureType, 0, GL_DEPTH_COMPONENT16, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                  textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSamplerCubeShadowTest::TextureQueryLevelsSamplerCubeShadowTest(deqp::Context &context,
                                                                                 const std::string &test_name,
                                                                                 const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())
{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "samplerCubeShadow";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(m_width)));
    m_textureType   = GL_TEXTURE_CUBE_MAP;
}

void TextureQueryLevelsSamplerCubeShadowTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    std::vector<float> textureData(m_width * m_height, 1.0f);
    for (int i = 0; i < 6; ++i)
    {
        gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT16, m_width, m_height, 0,
                      GL_DEPTH_COMPONENT, GL_FLOAT, textureData.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D failed");
    }
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSampler1DArrayShadowTest::TextureQueryLevelsSampler1DArrayShadowTest(
    deqp::Context &context, const std::string &test_name, const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler1DArrayShadow";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_height        = 1;
    m_initialLevels = 1 + static_cast<int>(floor(std::log2(m_width)));
    m_textureType   = GL_TEXTURE_1D_ARRAY;
}

void TextureQueryLevelsSampler1DArrayShadowTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    int layers = 4;
    std::vector<float> textureData(m_width * layers, 1.0f);
    gl.texImage2D(m_textureType, 0, GL_DEPTH_COMPONENT16, m_width, layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                  textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSampler2DArrayShadowTest::TextureQueryLevelsSampler2DArrayShadowTest(
    deqp::Context &context, const std::string &test_name, const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "sampler2DArrayShadow";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(std::max(m_width, m_height))));
    m_textureType   = GL_TEXTURE_2D_ARRAY;
}

void TextureQueryLevelsSampler2DArrayShadowTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    int layers = 4;
    std::vector<float> textureData(m_width * m_height * layers, 1.0f);
    gl.texImage3D(m_textureType, 0, GL_DEPTH_COMPONENT16, m_width, m_height, layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                  textureData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsSamplerCubeArrayShadowTest::TextureQueryLevelsSamplerCubeArrayShadowTest(
    deqp::Context &context, const std::string &test_name, const std::string &test_description)
    : TextureQueryLevelsBaseTest(context, test_name.c_str(), test_description.c_str())

{
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;

    replacements["sampler"] = "samplerCubeArrayShadow";
    m_fragment_shader_txt   = fragShaderTemplate.specialize(replacements);

    m_initialLevels = 1 + static_cast<int>(floor(std::log2(std::max(m_width, m_height))));
    m_textureType   = GL_TEXTURE_CUBE_MAP_ARRAY;
}

void TextureQueryLevelsSamplerCubeArrayShadowTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");

    int layers = 4;
    std::vector<float> textureData(m_width * m_height * layers * 6, 1.0f);
    gl.texImage3D(m_textureType, 0, GL_DEPTH_COMPONENT16, m_width, m_height, layers * 6, 0, GL_DEPTH_COMPONENT,
                  GL_FLOAT, textureData.data());

    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureParameteri failed");
    gl.generateMipmap(m_textureType);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenerateMipmap failed");
}

TextureQueryLevelsTests::TextureQueryLevelsTests(deqp::Context &context)
    : TestCaseGroup(context, "texture_query_levels", "Testes textureQueryLevels()")
{
}

void TextureQueryLevelsTests::init(void)
{
    addChild(
        new TextureQueryLevelsSampler1DTest(m_context, "sampler1D_test", "Tests textureQueryLevels with sampler1D"));
    addChild(
        new TextureQueryLevelsSampler2DTest(m_context, "sampler2D_test", "Tests textureQueryLevels with sampler2D"));
    addChild(
        new TextureQueryLevelsSampler3DTest(m_context, "sampler3D_test", "Tests textureQueryLevels with sampler3D"));
    addChild(new TextureQueryLevelsSamplerCubeTest(m_context, "samplerCube_test",
                                                   "Tests textureQueryLevels with samplerCube"));
    addChild(new TextureQueryLevelsSampler1DArrayTest(m_context, "sampler1DArray_test",
                                                      "Tests textureQueryLevels with sampler1DArray"));
    addChild(new TextureQueryLevelsSampler2DArrayTest(m_context, "sampler2DArray_test",
                                                      "Tests textureQueryLevels with sampler2DArray"));
    addChild(new TextureQueryLevelsSamplerCubeArrayTest(m_context, "samplerCubeArray_test",
                                                        "Tests textureQueryLevels with samplerCubeArray"));
    addChild(new TextureQueryLevelsSampler1DShadowTest(m_context, "sampler1DShadow_test",
                                                       "Tests textureQueryLevels with sampler1DShadow"));
    addChild(new TextureQueryLevelsSampler2DShadowTest(m_context, "sampler2DShadow_test",
                                                       "Tests textureQueryLevels with sampler2DShadow"));
    addChild(new TextureQueryLevelsSamplerCubeShadowTest(m_context, "samplerCubeShadow_test",
                                                         "Tests textureQueryLevels with samplerCubeShadow"));
    addChild((new TextureQueryLevelsSampler1DArrayShadowTest(m_context, "sampler1DArrayShadow_test",
                                                             "Tests textureQueryLevels with sampler1DArrayShadow")));
    addChild((new TextureQueryLevelsSampler2DArrayShadowTest(m_context, "sampler2DArrayShadow_test",
                                                             "Tests textureQueryLevels with sampler2DArrayShadow")));
    addChild((new TextureQueryLevelsSamplerCubeArrayShadowTest(
        m_context, "samplerCubeArrayShadow_test", "Tests textureQueryLevels with samplerCubeArrayShadow")));
}

} /* namespace gl4cts */
