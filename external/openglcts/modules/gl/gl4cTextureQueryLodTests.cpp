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
 * \file  gl4cTextureQueryLodTests.cpp
 * \brief Implements conformance texture query lod tests
 */ /*-------------------------------------------------------------------*/

#include "gl4cTextureQueryLodTests.hpp"
#include "tcuStringTemplate.hpp"
#include "gluContextInfo.hpp"
#include "glwEnums.hpp"
#include <cmath>
#include <algorithm>

namespace gl4cts
{

TextureQueryLodBaseTest::TextureQueryLodBaseTest(deqp::Context &context, const std::string &test_name,
                                                 const std::string &test_description)
    : TestCase(context, test_name.c_str(), test_description.c_str())
    , m_vertex_shader_txt("")
    , m_fragment_shader_txt("")
    , m_vbo(0)
    , m_vao(0)
    , m_ebo(0)
    , m_texture(0)
    , m_width(256)
    , m_height(256)
    , m_viewPortWidth(512)
    , m_viewPortHeight(512)
    , m_scaleLoc(0)
{
    m_vertex_shader_txt   = R"(
    #version 400 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in ${texCoordType} aTexCoord;
    uniform float scale;
    out ${texCoordType} texCoord;
    void main()
    {
        gl_Position = vec4(aPos * scale, 1.0);
        texCoord = aTexCoord;
    }
    )";
    m_fragment_shader_txt = R"(
    #version 400 core
    #extension GL_ARB_texture_query_lod: require
    out vec4 fragColor;
    in ${texCoordType} texCoord;
    uniform ${sampler} texture;
    void main()
    {
        float lod = textureQueryLOD(texture, texCoord).x;
        vec4 sampledColor = textureLod(texture, texCoord, lod);
        fragColor = sampledColor;
    }
    )";
}

tcu::TestNode::IterateResult TextureQueryLodBaseTest::iterate(void)
{
    bool texture_query_lod_supported = m_context.getContextInfo().isExtensionSupported("GL_ARB_texture_query_lod");
    if (!texture_query_lod_supported)
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

tcu::Vec3 TextureQueryLodBaseTest::calculateExpectedColor(float scale, int textureSize)
{
    float projectedSize = scale;
    float lod           = std::log2(static_cast<float>(textureSize) * projectedSize);
    lod                 = std::clamp(lod, 0.0f, static_cast<float>(std::log2(textureSize)));
    float colorFactor   = lod / static_cast<float>(std::log2(textureSize));
    return {1.0f - colorFactor, colorFactor, 0.0f};
}

bool TextureQueryLodBaseTest::test()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool result              = true;
    gl.clearColor(0.2f, 0.3f, 0.3f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glClear failed");
    gl.viewport(0, 0, m_viewPortWidth, m_viewPortHeight);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport failed");
    glu::ShaderProgram shaderProgram(m_context.getRenderContext(),
                                     glu::makeVtxFragSources(m_vertex_shader_txt, m_fragment_shader_txt));
    gl.useProgram(shaderProgram.getProgram());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram failed");
    gl.uniform1i(gl.getUniformLocation(shaderProgram.getProgram(), "texture"), 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");
    m_scaleLoc = gl.getUniformLocation(shaderProgram.getProgram(), "scale");
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation failed");
    gl.bindVertexArray(m_vao);
    gl.bindTexture(m_textureType, m_texture);
    result &= verify();
    return result;
}

bool TextureQueryLodBaseTest::verify()
{
    return false;
}

void TextureQueryLodBaseTest::createTexture()
{
}

void TextureQueryLodBaseTest::createBuffers()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.genBuffers(1, &m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers failed");
    gl.genBuffers(1, &m_ebo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers failed");
    gl.genVertexArrays(1, &m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenVertexArrays failed");
}

void TextureQueryLodBaseTest::clean()
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
    if (m_ebo != 0)
    {
        gl.deleteBuffers(1, &m_ebo);
    }
    if (m_texture != 0)
    {
        gl.deleteTextures(1, &m_texture);
    }
}

TextureQueryLodSampler1DTest::TextureQueryLodSampler1DTest(deqp::Context &context, const std::string &test_name,
                                                           const std::string &test_description)
    : TextureQueryLodBaseTest(context, test_name.c_str(), test_description.c_str())
{
    tcu::StringTemplate vertShaderTemplate{m_vertex_shader_txt};
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;
    replacements["texCoordType"] = "float";
    m_vertex_shader_txt          = vertShaderTemplate.specialize(replacements);
    replacements["sampler"]      = "sampler1D";
    m_fragment_shader_txt        = fragShaderTemplate.specialize(replacements);
    m_textureType                = GL_TEXTURE_1D;
}

void TextureQueryLodSampler1DTest::createBuffers()
{
    const glw::Functions &gl    = m_context.getRenderContext().getFunctions();
    std::vector<float> vertices = {-0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.5f, 0.0f, 1.0f, -0.5f, 0.0f, 0.0f};
    m_indices                   = std::vector<unsigned int>{0, 1, 2, 2, 3, 0};
    TextureQueryLodBaseTest::createBuffers();
    gl.bindVertexArray(m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray failed");
    gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer failed");
    gl.bufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData failed");
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer failed");
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData failed");
    gl.vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer failed");
    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray failed");
    gl.vertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)(3 * sizeof(float)));
    GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer failed");
    gl.enableVertexAttribArray(1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray failed");
}

void TextureQueryLodSampler1DTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");
    for (int level = 0; level <= static_cast<int>(std::log2(m_width)); ++level)
    {
        int mipmapSize = m_width >> level;
        if (mipmapSize < 1)
            mipmapSize = 1;
        std::vector<unsigned char> mipmapData(mipmapSize * 4);
        float colorFactor = static_cast<float>(level) / static_cast<float>(std::log2(m_width));
        unsigned char r   = static_cast<unsigned char>(255 * colorFactor);
        unsigned char g   = static_cast<unsigned char>(255 * (1.0f - colorFactor));
        unsigned char b   = 0;
        unsigned char a   = 255;
        for (int i = 0; i < mipmapSize; ++i)
        {
            mipmapData[i * 4 + 0] = r;
            mipmapData[i * 4 + 1] = g;
            mipmapData[i * 4 + 2] = b;
            mipmapData[i * 4 + 3] = a;
        }
        gl.texImage1D(GL_TEXTURE_1D, level, GL_RGBA, mipmapSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, mipmapData.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage1D failed");
    }
    gl.texParameteri(m_textureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
}

bool TextureQueryLodSampler1DTest::verify()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool result              = true;
    for (int i = 0; i < 5; ++i)
    {
        float scaleValue = 0.05f + 0.2f * i;
        gl.uniform1f(m_scaleLoc, scaleValue);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f failed");
        gl.drawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawElements failed");
        std::vector<unsigned char> pixelData(4);
        gl.readPixels(m_viewPortWidth / 2, m_viewPortHeight / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels failed");
        tcu::Vec3 expectedColor = calculateExpectedColor(scaleValue, m_width);
        unsigned char expectedR = static_cast<unsigned char>(std::clamp(expectedColor.x(), 0.0f, 1.0f) * 255.0f);
        unsigned char expectedG = static_cast<unsigned char>(std::clamp(expectedColor.y(), 0.0f, 1.0f) * 255.0f);
        unsigned char expectedB = static_cast<unsigned char>(std::clamp(expectedColor.z(), 0.0f, 1.0f) * 255.0f);
        int tolerance           = 15;
        if (std::abs(pixelData[0] - expectedR) < tolerance && std::abs(pixelData[1] - expectedG) < tolerance &&
            std::abs(pixelData[2] - expectedB) < tolerance)
        {
            result &= true;
        }
        else
        {
            result &= false;
        }
    }
    return result;
}

TextureQueryLodSampler2DTest::TextureQueryLodSampler2DTest(deqp::Context &context, const std::string &test_name,
                                                           const std::string &test_description)
    : TextureQueryLodBaseTest(context, test_name.c_str(), test_description.c_str())
{
    tcu::StringTemplate vertShaderTemplate{m_vertex_shader_txt};
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;
    replacements["texCoordType"] = "vec2";
    m_vertex_shader_txt          = vertShaderTemplate.specialize(replacements);
    replacements["sampler"]      = "sampler2D";
    m_fragment_shader_txt        = fragShaderTemplate.specialize(replacements);
    m_textureType                = GL_TEXTURE_2D;
}

void TextureQueryLodSampler2DTest::createBuffers()
{
    const glw::Functions &gl    = m_context.getRenderContext().getFunctions();
    std::vector<float> vertices = {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,
                                   0.5f,  0.5f,  0.0f, 1.0f, 1.0f, -0.5f, 0.5f,  0.0f, 0.0f, 1.0f};
    m_indices                   = std::vector<unsigned int>{0, 1, 2, 2, 3, 0};
    TextureQueryLodBaseTest::createBuffers();
    gl.bindVertexArray(m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray failed");
    gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer failed");
    gl.bufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData failed");
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer failed");
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData failed");
    gl.vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer failed");
    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray failed");
    gl.vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer failed");
    gl.enableVertexAttribArray(1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray failed");
}

void TextureQueryLodSampler2DTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");
    for (int level = 0; level <= static_cast<int>(std::log2(m_width)); ++level)
    {
        int mipmapSize = m_width >> level;
        if (mipmapSize < 1)
            mipmapSize = 1;
        std::vector<unsigned char> mipmapData(mipmapSize * mipmapSize * 4);
        float colorFactor = static_cast<float>(level) / static_cast<float>(std::log2(m_width));
        unsigned char r   = static_cast<unsigned char>(255 * colorFactor);
        unsigned char g   = static_cast<unsigned char>(255 * (1.0f - colorFactor));
        unsigned char b   = 0;
        unsigned char a   = 255;
        for (size_t i = 0; i < mipmapData.size(); i += 4)
        {
            mipmapData[i]     = r;
            mipmapData[i + 1] = g;
            mipmapData[i + 2] = b;
            mipmapData[i + 3] = a;
        }
        gl.texImage2D(m_textureType, level, GL_RGBA, mipmapSize, mipmapSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      mipmapData.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D failed");
    }
    gl.texParameteri(m_textureType, GL_TEXTURE_WRAP_S, GL_REPEAT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
}

bool TextureQueryLodSampler2DTest::verify()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool result              = true;
    for (int i = 0; i < 5; ++i)
    {
        float scaleValue = 0.05f + 0.2f * i;
        gl.uniform1f(m_scaleLoc, scaleValue);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f failed");
        gl.drawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawElements failed");
        std::vector<unsigned char> pixelData(4);
        gl.readPixels(m_viewPortWidth / 2, m_viewPortHeight / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels failed");
        tcu::Vec3 expectedColor = calculateExpectedColor(scaleValue, m_width);
        unsigned char expectedR = static_cast<unsigned char>(std::clamp(expectedColor.x(), 0.0f, 1.0f) * 255.0f);
        unsigned char expectedG = static_cast<unsigned char>(std::clamp(expectedColor.y(), 0.0f, 1.0f) * 255.0f);
        unsigned char expectedB = static_cast<unsigned char>(std::clamp(expectedColor.z(), 0.0f, 1.0f) * 255.0f);
        int tolerance           = 15;
        if (std::abs(pixelData[0] - expectedR) < tolerance && std::abs(pixelData[1] - expectedG) < tolerance &&
            std::abs(pixelData[2] - expectedB) < tolerance)
        {
            result &= true;
        }
        else
        {
            result &= false;
        }
    }
    return result;
}

TextureQueryLodSampler3DTest::TextureQueryLodSampler3DTest(deqp::Context &context, const std::string &test_name,
                                                           const std::string &test_description)
    : TextureQueryLodBaseTest(context, test_name.c_str(), test_description.c_str())
{
    tcu::StringTemplate vertShaderTemplate{m_vertex_shader_txt};
    tcu::StringTemplate fragShaderTemplate{m_fragment_shader_txt};
    std::map<std::string, std::string> replacements;
    replacements["texCoordType"] = "vec3";
    m_vertex_shader_txt          = vertShaderTemplate.specialize(replacements);
    replacements["sampler"]      = "sampler3D";
    m_fragment_shader_txt        = fragShaderTemplate.specialize(replacements);
    m_textureType                = GL_TEXTURE_3D;
}

void TextureQueryLodSampler3DTest::createBuffers()
{
    const glw::Functions &gl    = m_context.getRenderContext().getFunctions();
    std::vector<float> vertices = {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.5f,  -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
                                   0.5f,  0.5f,  -0.5f, 1.0f, 1.0f, 0.0f, -0.5f, 0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,
                                   -0.5f, -0.5f, 0.5f,  0.0f, 0.0f, 1.0f, 0.5f,  -0.5f, 0.5f,  1.0f, 0.0f, 1.0f,
                                   0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f, -0.5f, 0.5f,  0.5f,  0.0f, 1.0f, 1.0f};
    m_indices                   = std::vector<unsigned int>{0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 3, 2, 6, 6, 7, 3,
                                                            4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 4, 7, 6, 6, 5, 4};
    TextureQueryLodBaseTest::createBuffers();
    gl.bindVertexArray(m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray failed");
    gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer failed");
    gl.bufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData failed");
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer failed");
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData failed");
    gl.vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer failed");
    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray failed");
    gl.vertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer failed");
    gl.enableVertexAttribArray(1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray failed");
}

void TextureQueryLodSampler3DTest::createTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures failed");
    gl.bindTexture(m_textureType, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture failed");
    for (int level = 0; level <= static_cast<int>(std::log2(m_width)); ++level)
    {
        int mipmapSize = m_width >> level;
        if (mipmapSize < 1)
            mipmapSize = 1;
        std::vector<unsigned char> mipmapData(mipmapSize * mipmapSize * mipmapSize * 4);
        float colorFactor = static_cast<float>(level) / static_cast<float>(std::log2(m_width));
        unsigned char r   = static_cast<unsigned char>(255 * colorFactor);
        unsigned char g   = static_cast<unsigned char>(255 * (1.0f - colorFactor));
        unsigned char b   = 0;
        unsigned char a   = 255;
        for (size_t i = 0; i < mipmapData.size(); i += 4)
        {
            mipmapData[i + 0] = r;
            mipmapData[i + 1] = g;
            mipmapData[i + 2] = b;
            mipmapData[i + 3] = a;
        }
        gl.texImage3D(m_textureType, level, GL_RGBA, mipmapSize, mipmapSize, mipmapSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      mipmapData.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage3D failed");
    }
    gl.texParameteri(m_textureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
    gl.texParameteri(m_textureType, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri failed");
}

bool TextureQueryLodSampler3DTest::verify()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool result              = true;
    for (int i = 0; i < 5; ++i)
    {
        float scaleValue = 0.05f + 0.2f * i;
        gl.uniform1f(m_scaleLoc, scaleValue);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1f failed");
        gl.drawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawElements failed");
        std::vector<unsigned char> pixelData(4);
        gl.readPixels(m_viewPortWidth / 2, m_viewPortHeight / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels failed");
        tcu::Vec3 expectedColor = calculateExpectedColor(scaleValue, m_width);
        unsigned char expectedR = static_cast<unsigned char>(std::clamp(expectedColor.x(), 0.0f, 1.0f) * 255.0f);
        unsigned char expectedG = static_cast<unsigned char>(std::clamp(expectedColor.y(), 0.0f, 1.0f) * 255.0f);
        unsigned char expectedB = static_cast<unsigned char>(std::clamp(expectedColor.z(), 0.0f, 1.0f) * 255.0f);
        int tolerance           = 15;
        if (std::abs(pixelData[0] - expectedR) < tolerance && std::abs(pixelData[1] - expectedG) < tolerance &&
            std::abs(pixelData[2] - expectedB) < tolerance)
        {
            result &= true;
        }
        else
        {
            result &= false;
        }
    }
    return result;
}

TextureQueryLodTests::TextureQueryLodTests(deqp::Context &context)
    : TestCaseGroup(context, "texture_query_lod", "Testes textureQueryLod()")
{
}

void TextureQueryLodTests::init(void)
{
    addChild(new TextureQueryLodSampler1DTest(m_context, "sampler1D_test", "Tests textureQueryLod with sampler1D"));
    addChild(new TextureQueryLodSampler2DTest(m_context, "sampler2D_test", "Tests textureQueryLod with sampler2D"));
    addChild(new TextureQueryLodSampler3DTest(m_context, "sampler3D_test", "Tests textureQueryLod with sampler3D"));
}
} /* namespace gl4cts */
