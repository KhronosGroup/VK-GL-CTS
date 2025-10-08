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
 * \file  gl4cTextureBufferTests.cpp
 * \brief Implements conformance tests for texture buffer functionality
 */ /*-------------------------------------------------------------------*/

#include "gl4cTextureBufferTests.hpp"
#include "gluContextInfo.hpp"
#include "glwEnums.hpp"

namespace gl4cts
{

static decltype(glw::Functions::textureBuffer) getTextureBufferFunction(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.textureBuffer != nullptr)
        return gl.textureBuffer;

    decltype(glw::Functions::textureBuffer) func =
        (decltype(func))context.getRenderContext().getProcAddress("glTextureBuffer");

    return func;
}

static decltype(glw::Functions::texBuffer) getTexBufferFunction(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.texBuffer != nullptr)
        return gl.texBuffer;

    decltype(glw::Functions::texBuffer) func = (decltype(func))context.getRenderContext().getProcAddress("glTexBuffer");

    return func;
}

static decltype(glw::Functions::textureBufferRange) getTextureBufferRangeFunction(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.textureBufferRange != nullptr)
        return gl.textureBufferRange;

    decltype(glw::Functions::textureBufferRange) func =
        (decltype(func))context.getRenderContext().getProcAddress("glTextureBufferRange");

    return func;
}

static decltype(glw::Functions::texBufferRange) getTexBufferRangeFunction(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.texBufferRange != nullptr)
        return gl.texBufferRange;

    decltype(glw::Functions::texBufferRange) func =
        (decltype(func))context.getRenderContext().getProcAddress("glTexBufferRange");

    return func;
}

TextureBufferTestBase::TextureBufferTestBase(deqp::Context &context, const char *test_name,
                                             const char *test_description)
    : TestCase(context, test_name, test_description)
    , m_tbo(0)
    , m_tboTexture(0)
    , m_bufferRange(0)
{
}

TextureBufferTestBase::~TextureBufferTestBase()
{
}

tcu::TestNode::IterateResult TextureBufferTestBase::iterate(void)
{
    bool is_at_least_gl_45 = (glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::core(4, 5)));
    bool is_arb_direct_state_access = m_context.getContextInfo().isExtensionSupported("GL_ARB_direct_state_access");

    if (!(is_at_least_gl_45 || is_arb_direct_state_access))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
                                "GL version < 4.5 or GL_ARB_direct_state_access not supported");
        return STOP;
    }
    if (getTextureBufferFunction(m_context) == nullptr || getTexBufferFunction(m_context) == nullptr ||
        getTextureBufferRangeFunction(m_context) == nullptr || getTexBufferRangeFunction(m_context) == nullptr)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "glTex*, glTexture* function pointers are null");
        return STOP;
    }

    bool is_ok         = true;
    bool out_of_memory = false;
    try
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const size_t pixelSize   = 1;
        const size_t exceed      = 10;

        glw::GLint maxTexBufferSize;
        gl.getIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTexBufferSize);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv GL_MAX_TEXTURE_BUFFER_SIZE");

        size_t dataSize = pixelSize * maxTexBufferSize + exceed;
        std::vector<glw::GLubyte> data;
        if (data.max_size() < dataSize)
        {
            throw glu::OutOfMemoryError("data.max_size() < dataSize");
        }
        data.resize(dataSize);

        m_bufferRange = sizeof(data[0]) * data.size();

        gl.genBuffers(1, &m_tbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers");
        gl.bindBuffer(GL_TEXTURE_BUFFER, m_tbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");
        gl.bufferData(GL_TEXTURE_BUFFER, m_bufferRange, data.data(), GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

        gl.genTextures(1, &m_tboTexture);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
        gl.bindTexture(GL_TEXTURE_BUFFER, m_tboTexture);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

        test();

        glw::GLint textureWidth;
        gl.getTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &textureWidth);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexLevelParameteriv GL_TEXTURE_WIDTH");

        glw::GLint textureSize = 0;
        gl.getTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_SIZE, &textureSize);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexLevelParameteriv GL_TEXTURE_BUFFER_SIZE");

        is_ok = (textureWidth == maxTexBufferSize) && ((static_cast<size_t>(textureSize)) == dataSize);
    }
    catch (const glu::OutOfMemoryError &error)
    {
        is_ok         = false;
        out_of_memory = true;
    }
    catch (const std::bad_alloc &error)
    {
        is_ok         = false;
        out_of_memory = true;
    }

    catch (...)
    {
        is_ok         = false;
        out_of_memory = false;
    }

    if (is_ok)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    else if (out_of_memory)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Out of memory error");
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }

    clean();

    return STOP;
}

void TextureBufferTestBase::clean()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    if (0 != m_tbo)
    {
        gl.deleteBuffers(1, &m_tbo);
        m_tbo = 0;
    }

    if (0 != m_tboTexture)
    {
        gl.deleteTextures(1, &m_tboTexture);
        m_tboTexture = 0;
    }
}

void TextureBufferTestBase::test()
{
}

TexBufferTest::TexBufferTest(deqp::Context &context, const char *test_name, const char *test_description)
    : TextureBufferTestBase(context, test_name, test_description)
{
}

void TexBufferTest::test()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    getTexBufferFunction(m_context)(GL_TEXTURE_BUFFER, GL_R8I, m_tbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexBuffer");
}

TextureBufferTest::TextureBufferTest(deqp::Context &context, const char *test_name, const char *test_description)
    : TextureBufferTestBase(context, test_name, test_description)
{
}

void TextureBufferTest::test()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    getTextureBufferFunction(m_context)(m_tboTexture, GL_R8I, m_tbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexBuffer");
}

TexBufferRangeTest::TexBufferRangeTest(deqp::Context &context, const char *test_name, const char *test_description)
    : TextureBufferTestBase(context, test_name, test_description)
{
}

void TexBufferRangeTest::test()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    getTexBufferRangeFunction(m_context)(GL_TEXTURE_BUFFER, GL_R8I, m_tbo, 0, m_bufferRange);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexBufferRange");
}

TextureBufferRangeTest::TextureBufferRangeTest(deqp::Context &context, const char *test_name,
                                               const char *test_description)
    : TextureBufferTestBase(context, test_name, test_description)
{
}

void TextureBufferRangeTest::test()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    getTextureBufferRangeFunction(m_context)(m_tboTexture, GL_R8I, m_tbo, 0, m_bufferRange);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexBufferRange");
}

TextureBufferTests::TextureBufferTests(deqp::Context &context)
    : TestCaseGroup(context, "texture_buffer_size_clamping", "Texture buffer size clamping test cases")
{
}

void TextureBufferTests::init(void)
{
    addChild(new TexBufferTest(m_context, "tex_buffer", "tests glTexBuffer()"));
    addChild(new TextureBufferTest(m_context, "texture_buffer", "tests glTextureBuffer()"));
    addChild(new TexBufferRangeTest(m_context, "tex_buffer_range", "tests glTexBufferRange()"));
    addChild(new TextureBufferRangeTest(m_context, "texture_buffer_range", "tests glTextureBufferRange()"));
}

} // namespace gl4cts
