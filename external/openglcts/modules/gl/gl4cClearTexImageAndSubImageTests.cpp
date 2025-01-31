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
 * \file  gl4cClearTexImageAndSubImageTests.hpp
 * \brief Implements conformance tests for GL_ARB_clear_texture functionality
 */ /*-------------------------------------------------------------------*/

#include "gl4cClearTexImageAndSubImageTests.hpp"
#include "gluContextInfo.hpp"
#include "glwEnums.hpp"
#include "tcuRenderTarget.hpp"
#include "gluStrUtil.hpp"
#include "tcuTestLog.hpp"

#include <cmath>

namespace gl4cts
{

const int FILL_IMAGE_VALUE      = 15;
const int CLEAR_SUB_IMAGE_VALUE = 5;

ClearTexAndSubImageTest::ClearTexAndSubImageTest(deqp::Context &context, const char *test_name,
                                                 const char *test_description, glw::GLenum format,
                                                 glw::GLenum internalFormat, glw::GLenum type, size_t pixelSize,
                                                 glw::GLint texLevel, TestOptions testOptions)
    : TestCase(context, test_name, test_description)
    , m_texture(0)
    , m_width(context.getRenderTarget().getWidth())
    , m_height(context.getRenderTarget().getHeight())
    , m_format(format)
    , m_internalFormat(internalFormat)
    , m_pixelSize(pixelSize)
    , m_type(type)
    , m_texLevel(texLevel)
    , m_testOptions(testOptions)
{
    const auto &gl                 = m_context.getRenderContext().getFunctions();
    glw::GLint gl_max_texture_size = 0;
    gl.getIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);
    const glw::GLint max_level = glw::GLint(log2(double(gl_max_texture_size)) / log2(2.0));
    if (m_texLevel > max_level)
        m_texLevel = max_level;

    m_name.append("_format_" + std::string(glu::getTextureFormatName(m_format)) + "_internalFormat_" +
                  std::string(glu::getTextureFormatName(m_internalFormat)) + "_type_" +
                  std::string(glu::getTypeName(m_type)) + "_pixelSize_" + std::to_string(m_pixelSize) + "_texLevel_" +
                  std::to_string(m_texLevel));
}

static decltype(glw::Functions::clearTexImage) getClearTexImageFunction(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.clearTexImage != nullptr)
        return gl.clearTexImage;

    decltype(glw::Functions::clearTexImage) clearTexImageFunc;

    clearTexImageFunc = (decltype(clearTexImageFunc))context.getRenderContext().getProcAddress("glClearTexImage");

    DE_ASSERT(clearTexImageFunc);

    return clearTexImageFunc;
}

static decltype(glw::Functions::clearTexSubImage) getClearTexSubImageFunction(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.clearTexSubImage != nullptr)
        return gl.clearTexSubImage;

    decltype(glw::Functions::clearTexSubImage) clearTexSubImageFunc;

    clearTexSubImageFunc =
        (decltype(clearTexSubImageFunc))context.getRenderContext().getProcAddress("glClearTexSubImage");

    DE_ASSERT(clearTexSubImageFunc);

    return clearTexSubImageFunc;
}

tcu::TestNode::IterateResult ClearTexAndSubImageTest::iterate(void)
{
    bool is_at_least_gl_44 = (glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::core(4, 4)));
    bool is_arb_clear_texture = m_context.getContextInfo().isExtensionSupported("GL_ARB_clear_texture");

    if (!(is_at_least_gl_44 || is_arb_clear_texture))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }
    if (getClearTexImageFunction(m_context) == nullptr || getClearTexSubImageFunction(m_context) == nullptr)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    bool is_ok      = true;
    bool test_error = false;
    try
    {
        switch (m_type)
        {
        case GL_UNSIGNED_SHORT:
        {
            is_ok = test<unsigned short>();
            break;
        }
        case GL_FLOAT:
        {
            is_ok = test<float>();
            break;
        }
        }
    }
    catch (...)
    {
        is_ok      = false;
        test_error = true;
    }

    if (is_ok)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    else
    {
        if (test_error)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, "Error");
        }
        else
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        }
    }

    return STOP;
}

template <typename type>
bool ClearTexAndSubImageTest::test()
{
    bool is_ok = true;

    createTexture();
    fillTexture<type>();
    clearTexture<type>();
    is_ok = verifyResults<type>();
    deleteTexture();

    return is_ok;
}

void ClearTexAndSubImageTest::createTexture()
{
    const auto &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_texLevel);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
}

template <typename type>
void ClearTexAndSubImageTest::clearTexture()
{
    m_testOptions.clearSubTexImage ? clearSubImageTexture<type>() : clearImageTexture<type>();
}

template <typename type>
void ClearTexAndSubImageTest::clearImageTexture()
{
    const auto &gl         = m_context.getRenderContext().getFunctions();
    auto clearTexImageFunc = getClearTexImageFunction(m_context);

    //    Disabled color write masks must not affect texture clears.
    gl.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    switch (m_pixelSize)
    {
    case 1:
    {
        type clearData = (m_format == GL_DEPTH_COMPONENT ? 1 : 5);
        if (m_testOptions.clearWithRed)
            clearTexImageFunc(m_texture, m_texLevel, GL_RED, m_type, &clearData);
        else if (m_testOptions.clearWithRg)
            clearTexImageFunc(m_texture, m_texLevel, GL_RG, m_type, &clearData);
        else if (m_testOptions.clearWithRgb)
            clearTexImageFunc(m_texture, m_texLevel, GL_RGB, m_type, &clearData);
        else if (m_testOptions.clearWithRgba)
            clearTexImageFunc(m_texture, m_texLevel, GL_RGBA, m_type, &clearData);
        else
            clearTexImageFunc(m_texture, m_texLevel, m_format, m_type, &clearData);
        break;
    }
    case 2:
    {
        type clearData[2] = {5, 4};
        if (m_testOptions.clearWithRed)
            clearTexImageFunc(m_texture, m_texLevel, GL_RED, m_type, clearData);
        else if (m_testOptions.clearWithRg)
            clearTexImageFunc(m_texture, m_texLevel, GL_RG, m_type, &clearData);
        else if (m_testOptions.clearWithRgb)
            clearTexImageFunc(m_texture, m_texLevel, GL_RGB, m_type, &clearData);
        else if (m_testOptions.clearWithRgba)
            clearTexImageFunc(m_texture, m_texLevel, GL_RGBA, m_type, clearData);
        else
            clearTexImageFunc(m_texture, m_texLevel, m_format, m_type, clearData);
        break;
    }
    case 3:
    {
        type clearData[3] = {5, 4, 3};
        if (m_testOptions.clearWithRed)
            clearTexImageFunc(m_texture, m_texLevel, GL_RED, m_type, clearData);
        else if (m_testOptions.clearWithRg)
            clearTexImageFunc(m_texture, m_texLevel, GL_RG, m_type, &clearData);
        else if (m_testOptions.clearWithRgb)
            clearTexImageFunc(m_texture, m_texLevel, GL_RGB, m_type, &clearData);
        else if (m_testOptions.clearWithRgba)
            clearTexImageFunc(m_texture, m_texLevel, GL_RGBA, m_type, clearData);
        else
            clearTexImageFunc(m_texture, m_texLevel, m_format, m_type, clearData);
        break;
    }
    case 4:
    {
        type clearData[4] = {5, 4, 3, 2};
        if (m_testOptions.clearWithRed)
            clearTexImageFunc(m_texture, m_texLevel, GL_RED, m_type, clearData);
        else if (m_testOptions.clearWithRg)
            clearTexImageFunc(m_texture, m_texLevel, GL_RG, m_type, &clearData);
        else if (m_testOptions.clearWithRgb)
            clearTexImageFunc(m_texture, m_texLevel, GL_RGB, m_type, &clearData);
        else if (m_testOptions.clearWithRgba)
            clearTexImageFunc(m_texture, m_texLevel, GL_RGBA, m_type, clearData);
        else
            clearTexImageFunc(m_texture, m_texLevel, m_format, m_type, clearData);
        break;
    }
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "clearTexImage");
}

template <typename type>
void ClearTexAndSubImageTest::clearSubImageTexture()
{
    const auto &gl            = m_context.getRenderContext().getFunctions();
    auto clearTexSubImageFunc = getClearTexSubImageFunction(m_context);

    //    Disabled color write masks must not affect texture clears.
    gl.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glw::GLint xoffset  = 0;
    glw::GLint yoffset  = 0;
    glw::GLint zoffset  = 0;
    glw::GLsizei width  = m_width / 2;
    glw::GLsizei height = m_height / 2;
    glw::GLsizei depth  = m_testOptions.dimensionZero ? 0 : 1;

    switch (m_pixelSize)
    {
    case 1:
    {
        type clearData = m_format == GL_DEPTH_COMPONENT ? 1 : CLEAR_SUB_IMAGE_VALUE;
        if (m_testOptions.clearWithRed)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RED, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRg)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RG, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRgb)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RGB, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRgba)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RGBA,
                                 m_type, &clearData);
        else
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, m_format,
                                 m_type, &clearData);
        break;
    }
    case 2:
    {
        type clearData[2] = {CLEAR_SUB_IMAGE_VALUE, CLEAR_SUB_IMAGE_VALUE};
        if (m_testOptions.clearWithRed)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RED, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRg)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RG, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRgb)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RGB, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRgba)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RGBA,
                                 m_type, &clearData);
        else
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, m_format,
                                 m_type, &clearData);
        break;
    }
    case 3:
    {
        type clearData[3] = {CLEAR_SUB_IMAGE_VALUE, CLEAR_SUB_IMAGE_VALUE, CLEAR_SUB_IMAGE_VALUE};
        if (m_testOptions.clearWithRed)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RED, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRg)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RG, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRgb)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RGB, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRgba)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RGBA,
                                 m_type, &clearData);
        else
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, m_format,
                                 m_type, &clearData);
        break;
    }
    case 4:
    {
        type clearData[4] = {CLEAR_SUB_IMAGE_VALUE, CLEAR_SUB_IMAGE_VALUE, CLEAR_SUB_IMAGE_VALUE,
                             CLEAR_SUB_IMAGE_VALUE};
        if (m_testOptions.clearWithRed)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RED, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRg)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RG, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRgb)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RGB, m_type,
                                 &clearData);
        else if (m_testOptions.clearWithRgba)
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, GL_RGBA,
                                 m_type, &clearData);
        else
            clearTexSubImageFunc(m_texture, m_texLevel, xoffset, yoffset, zoffset, width, height, depth, m_format,
                                 m_type, &clearData);
        break;
    }
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "clearTexImage");
}

void ClearTexAndSubImageTest::deleteTexture()
{
    const auto &gl = m_context.getRenderContext().getFunctions();

    gl.deleteTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
}

template <typename type>
void ClearTexAndSubImageTest::fillTexture()
{
    const auto &gl = m_context.getRenderContext().getFunctions();

    auto color = (m_format == GL_DEPTH_COMPONENT ? 1 : FILL_IMAGE_VALUE);
    std::vector<type> texData(m_width * m_height * m_pixelSize, color);

    if (m_pixelSize < 4)
        gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);

    gl.texImage2D(GL_TEXTURE_2D, m_texLevel, m_internalFormat, m_width, m_height, 0, m_format, m_type, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");
}

template <typename type>
bool ClearTexAndSubImageTest::verifyResults()
{
    return m_testOptions.clearSubTexImage ? verifyClearSubImageResults<type>() : verifyClearImageResults<type>();
}

template <typename type>
bool ClearTexAndSubImageTest::verifyClearImageResults()
{
    const auto &gl = m_context.getRenderContext().getFunctions();

    auto size = m_width * m_height * m_pixelSize;
    std::vector<type> texReadData(size, 0);

    gl.getTexImage(GL_TEXTURE_2D, m_texLevel, m_format, m_type, texReadData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "getTexImage");

    switch (m_pixelSize)
    {
    case 1:
    {
        type clearData = (m_format == GL_DEPTH_COMPONENT ? 1 : 5);
        for (size_t x = 0; x < size; x += m_pixelSize)
        {
            if (texReadData[x] != clearData)
            {
                return false;
            }
        }

        break;
    }
    case 2:
    {
        type clearData[2] = {5, 4};
        if (m_testOptions.clearWithRed)
        {
            clearData[1] = 0;
        }
        for (size_t x = 0; x < size; x += m_pixelSize)
        {
            if (texReadData[x] != clearData[0] || texReadData[x + 1] != clearData[1])
            {
                return false;
            }
        }

        break;
    }
    case 3:
    {
        type clearData[3] = {5, 4, 3};
        if (m_testOptions.clearWithRed)
        {
            clearData[1] = 0;
            clearData[2] = 0;
        }
        else if (m_testOptions.clearWithRg)
        {
            clearData[2] = 0;
        }
        for (size_t x = 0; x < size; x += m_pixelSize)
        {
            if (texReadData[x] != clearData[0] || texReadData[x + 1] != clearData[1] ||
                texReadData[x + 2] != clearData[2])
            {
                return false;
            }
        }

        break;
    }
    case 4:
    {
        type clearData[4] = {5, 4, 3, 2};
        if (m_testOptions.clearWithRed)
        {
            auto ALPHA = static_cast<unsigned short>(-1);
            if (m_type == GL_FLOAT)
            {
                ALPHA = 1;
            }
            clearData[1] = 0;
            clearData[2] = 0;
            clearData[3] = ALPHA;
        }
        else if (m_testOptions.clearWithRg)
        {
            auto ALPHA = static_cast<unsigned short>(-1);
            if (m_type == GL_FLOAT)
            {
                ALPHA = 1;
            }
            clearData[2] = 0;
            clearData[3] = ALPHA;
        }
        else if (m_testOptions.clearWithRgb)
        {
            auto ALPHA = static_cast<unsigned short>(-1);
            if (m_type == GL_FLOAT)
            {
                ALPHA = 1;
            }
            clearData[3] = ALPHA;
        }
        for (size_t x = 0; x < size; x += m_pixelSize)
        {
            if (texReadData[x] != clearData[0] || texReadData[x + 1] != clearData[1] ||
                texReadData[x + 2] != clearData[2] || texReadData[x + 3] != clearData[3])
            {
                return false;
            }
        }

        break;
    }
    default:
        return false;
    }

    return true;
}

template <typename type>
bool ClearTexAndSubImageTest::verifyClearSubImageResults()
{
    const auto &gl = m_context.getRenderContext().getFunctions();

    auto size = m_width * m_height * m_pixelSize;
    std::vector<type> texReadData(size, 0);

    gl.getTexImage(GL_TEXTURE_2D, m_texLevel, m_format, m_type, texReadData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "getTexImage");

    if (m_testOptions.clearWithRed)
    {
        switch (m_pixelSize)
        {
        case 1:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE))
                        return false;
                }
            }
            break;
        }
        case 2:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) || (texReadData[pixel + 1] != 0))
                        return false;
                }
            }
            break;
        }
        case 3:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) || (texReadData[pixel + 1] != 0) ||
                        (texReadData[pixel + 2] != 0))
                        return false;
                }
            }
            break;
        }
        case 4:
        {
            auto ALPHA = static_cast<unsigned short>(-1);
            if (m_type == GL_FLOAT)
            {
                ALPHA = 1;
            }
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) || (texReadData[pixel + 1] != 0) ||
                        (texReadData[pixel + 2] != 0) || (texReadData[pixel + 3] != ALPHA))
                        return false;
                }
            }
            break;
        }
        default:
            return false;
        }
    }
    else if (m_testOptions.clearWithRg)
    {
        switch (m_pixelSize)
        {
        case 1:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE))
                        return false;
                }
            }
            break;
        }
        case 2:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) ||
                        (texReadData[pixel + 1] != CLEAR_SUB_IMAGE_VALUE))
                        return false;
                }
            }
            break;
        }
        case 3:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) ||
                        (texReadData[pixel + 1] != CLEAR_SUB_IMAGE_VALUE) || (texReadData[pixel + 2] != 0))
                        return false;
                }
            }
            break;
        }
        case 4:
        {
            auto ALPHA = static_cast<unsigned short>(-1);
            if (m_type == GL_FLOAT)
            {
                ALPHA = 1;
            }
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) ||
                        (texReadData[pixel + 1] != CLEAR_SUB_IMAGE_VALUE) || (texReadData[pixel + 2] != 0) ||
                        (texReadData[pixel + 3] != ALPHA))
                        return false;
                }
            }
            break;
        }
        default:
            return false;
        }
    }
    else if (m_testOptions.clearWithRgb)
    {
        switch (m_pixelSize)
        {
        case 1:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE))
                        return false;
                }
            }
            break;
        }
        case 2:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) ||
                        (texReadData[pixel + 1] != CLEAR_SUB_IMAGE_VALUE))
                        return false;
                }
            }
            break;
        }
        case 3:
        {
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) ||
                        (texReadData[pixel + 1] != CLEAR_SUB_IMAGE_VALUE) ||
                        (texReadData[pixel + 2] != CLEAR_SUB_IMAGE_VALUE))
                        return false;
                }
            }
            break;
        }
        case 4:
        {
            auto ALPHA = static_cast<unsigned short>(-1);
            if (m_type == GL_FLOAT)
            {
                ALPHA = 1;
            }
            for (size_t h = 0; h < (m_height) / 2; ++h)
            {
                for (size_t w = 0; w < (m_width * m_pixelSize) / 2; w += m_pixelSize)
                {
                    auto pixel = h * m_width * m_pixelSize + w;
                    if ((texReadData[pixel] != CLEAR_SUB_IMAGE_VALUE) ||
                        (texReadData[pixel + 1] != CLEAR_SUB_IMAGE_VALUE) ||
                        (texReadData[pixel + 2] != CLEAR_SUB_IMAGE_VALUE) || (texReadData[pixel + 3] != ALPHA))
                        return false;
                }
            }
            break;
        }
        default:
            return false;
        }
    }
    else
    {
        auto clear = m_testOptions.dimensionZero ? FILL_IMAGE_VALUE : CLEAR_SUB_IMAGE_VALUE;
        if (m_format == GL_DEPTH_COMPONENT)
            clear = 1;

        for (size_t h = 0; h < (m_height) / 2; ++h)
        {
            for (size_t w = 0; w < (m_width * m_pixelSize) / 2; ++w)
            {
                if (texReadData[h * m_width * m_pixelSize + w] != clear)
                    return false;
            }
        }
    }

    return true;
}

ClearTexAndSubImageNegativeTest::ClearTexAndSubImageNegativeTest(deqp::Context &context, const char *test_name,
                                                                 const char *test_description,
                                                                 std::function<bool(deqp::Context &)> &testFunc)
    : TestCase(context, test_name, test_description)
    , m_testFunc(testFunc)
{
}

tcu::TestNode::IterateResult ClearTexAndSubImageNegativeTest::iterate(void)
{
    bool is_at_least_gl_44 = (glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::core(4, 4)));
    bool is_arb_clear_texture = m_context.getContextInfo().isExtensionSupported("GL_ARB_clear_texture");

    if (!(is_at_least_gl_44 || is_arb_clear_texture))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }
    if (getClearTexImageFunction(m_context) == nullptr || getClearTexSubImageFunction(m_context) == nullptr)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    bool is_ok      = true;
    bool test_error = false;
    try
    {
        is_ok = m_testFunc(m_context);
    }
    catch (...)
    {
        is_ok      = false;
        test_error = true;
    }

    if (is_ok)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    else
    {
        if (test_error)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, "Error");
        }
        else
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        }
    }

    return STOP;
}

bool checkError(deqp::Context &context, glw::GLuint expected_error, const glw::GLchar *function_name)
{
    const glw::Functions &gl = context.getRenderContext().getFunctions();

    glw::GLenum error = GL_NO_ERROR;

    if (expected_error != (error = gl.getError()))
    {
        context.getTestContext().getLog()
            << tcu::TestLog::Message << function_name << " generated error " << glu::getErrorStr(error) << " but, "
            << glu::getErrorStr(expected_error) << " was expected" << tcu::TestLog::EndMessage;

        return false;
    }

    return true;
}

std::function<bool(deqp::Context &)> textureEqualZeroTestCase = [](deqp::Context &context)
{
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    clearTexImageFunc(0, 0, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(0, 0, 0, 0, 0, 10, 10, 1, GL_RGBA, GL_FLOAT, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    return result;
};

std::function<bool(deqp::Context &)> bufferTextureTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    std::vector<float> tbo_data(10 * 10 * 4, 1.0f);
    glw::GLuint tbo;
    glw::GLuint texture;
    gl.genBuffers(1, &tbo);
    gl.bindBuffer(GL_TEXTURE_BUFFER, tbo);
    gl.bufferData(GL_TEXTURE_BUFFER, tbo_data.size() * sizeof(tbo_data[0]), tbo_data.data(), GL_STATIC_DRAW);
    gl.bindBuffer(GL_TEXTURE_BUFFER, 0);

    gl.genTextures(1, &texture);
    gl.bindTexture(GL_TEXTURE_BUFFER, texture);
    gl.textureBuffer(texture, GL_RGBA32F, tbo);

    clearTexImageFunc(texture, 0, GL_RGBA, GL_FLOAT, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_RGBA, GL_FLOAT, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    gl.deleteBuffers(1, &tbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "gldeleteBuffers");

    return result;
};

std::function<bool(deqp::Context &)> compressedTextureTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned short> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA, 10, 10, 0, GL_RGBA, GL_UNSIGNED_SHORT, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    clearTexImageFunc(texture, 0, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_RGBA, GL_FLOAT, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> negativeDimensionTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned short> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 10, 10, 0, GL_RGBA, GL_UNSIGNED_SHORT, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, -1, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result = checkError(context, GL_INVALID_VALUE, "glClearTexSubImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, -10, 10, 1, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result &= checkError(context, GL_INVALID_VALUE, "glClearTexSubImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, -10, 1, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result &= checkError(context, GL_INVALID_VALUE, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> depthComponentIsInternalFormatButFormatNotTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned short> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 10, 10, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
                  texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    clearTexImageFunc(texture, 0, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> depthStencilIsInternalFormatButFormatNotTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned char> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, 10, 10, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                  texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    clearTexImageFunc(texture, 0, GL_RGBA, GL_UNSIGNED_INT_24_8, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_RGBA, GL_UNSIGNED_INT_24_8, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> stencilIndexIsInternalFormatButFormatNotTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned short> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_STENCIL_INDEX, 10, 10, 0, GL_STENCIL_INDEX, GL_UNSIGNED_SHORT, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    clearTexImageFunc(texture, 0, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_RGBA, GL_UNSIGNED_SHORT, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> rgbaIsInternalFormatButFormatDepthComponentTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned short> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 10, 10, 0, GL_RGBA, GL_UNSIGNED_SHORT, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    clearTexImageFunc(texture, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> rgbaIsInternalFormatButFormatDepthStencilTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned short> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 10, 10, 0, GL_RGBA, GL_UNSIGNED_SHORT, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    clearTexImageFunc(texture, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_SHORT, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_SHORT, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> rgbaIsInternalFormatButFormatStencilIndexTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned short> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 10, 10, 0, GL_RGBA, GL_UNSIGNED_SHORT, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    clearTexImageFunc(texture, 0, GL_STENCIL_INDEX, GL_UNSIGNED_SHORT, 0);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_STENCIL_INDEX, GL_UNSIGNED_SHORT, 0);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> integerIsInternalFormatButFormatNotIntegerTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<unsigned short> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, 10, 10, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    unsigned short clearData[4] = {5, 4, 3, 2};
    clearTexImageFunc(texture, 0, GL_RGBA, GL_UNSIGNED_SHORT, clearData);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_RGBA, GL_UNSIGNED_SHORT, clearData);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

std::function<bool(deqp::Context &)> integerIsNotInternalFormatButFormatIsIntegerTestCase = [](deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunction(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunction(context);
    bool result               = false;

    glw::GLuint texture;
    gl.genTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
    gl.bindTexture(GL_TEXTURE_2D, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

    std::vector<float> texData(10 * 10 * 4, 5);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, 10, 10, 0, GL_RGBA, GL_UNSIGNED_SHORT, texData.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");

    unsigned short clearData[4] = {5, 4, 3, 2};
    clearTexImageFunc(texture, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, clearData);
    result = checkError(context, GL_INVALID_OPERATION, "glClearTexImage");

    clearTexSubImageFunc(texture, 0, 0, 0, 0, 10, 10, 1, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, clearData);
    result &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage");

    gl.deleteTextures(1, &texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return result;
};

ClearTextureImageTestCases::ClearTextureImageTestCases(deqp::Context &context)
    : TestCaseGroup(context, "clear_tex_image", "GL_ARB_clear_texture extension test cases")
{
}

void ClearTextureImageTestCases::init(void)
{
    std::vector<glw::GLenum> formats{GL_RGBA, GL_RGBA, GL_RGB, GL_RGB, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT};
    std::vector<glw::GLenum> internalFormats{GL_RGBA16, GL_RGBA32F,           GL_RGB16,
                                             GL_RGB32F, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT32F};
    std::vector<glw::GLenum> types{GL_UNSIGNED_SHORT, GL_FLOAT,          GL_UNSIGNED_SHORT,
                                   GL_FLOAT,          GL_UNSIGNED_SHORT, GL_FLOAT};
    std::vector<size_t> pixelSizes{4, 4, 3, 3, 1, 1};

    ClearTexAndSubImageTest::TestOptions clearImgOpts{true, false, false, false, false, false, false};
    ClearTexAndSubImageTest::TestOptions clearSubImgOpts{false, true, false, false, false, false, false};
    ClearTexAndSubImageTest::TestOptions clearSubImgDimZeroOpts{false, true, true, false, false, false, false};

    for (size_t i = 0; i < formats.size(); ++i)
    {
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image", "tests glClearTexImage function",
                                             formats[i], internalFormats[i], types[i], pixelSizes[i], i, clearImgOpts));
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_sub_image", "tests glClearTexSubImage function",
                                             formats[i], internalFormats[i], types[i], pixelSizes[i], i,
                                             clearSubImgOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_dimension_equal_zero",
            "tests glClearTexSubImage function with one dimension equal to zero, nothing should be done", formats[i],
            internalFormats[i], types[i], pixelSizes[i], i, clearSubImgDimZeroOpts));
    }
    {
        // Tests glClearTexImage with GL_RGBA, GL_RGB, GL_RG, GL_RED image, type GL_FLOAT, GL_UNSIGNED_SHORT and clears with GL_RED, GL_RG, GL_RGB type GL_FLOAT
        ClearTexAndSubImageTest::TestOptions clearImgClearWithRedOpts{true, false, false, true, false, false, false};
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_red_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RED component", GL_RGBA, GL_RGBA32F,
            GL_FLOAT, 4, 0, clearImgClearWithRedOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_red_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RED component", GL_RGBA, GL_RGBA16,
            GL_UNSIGNED_SHORT, 4, 0, clearImgClearWithRedOpts));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_red_component_rgb_image",
            "tests glClearTexImage function with GL_RGB image and clear only GL_RED component", GL_RGB, GL_RGB32F,
            GL_FLOAT, 3, 0, clearImgClearWithRedOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_red_component_rgb_image",
            "tests glClearTexImage function with GL_RGB image and clear only GL_RED component", GL_RGB, GL_RGB16,
            GL_UNSIGNED_SHORT, 3, 0, clearImgClearWithRedOpts));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_red_component_rg_image",
            "tests glClearTexImage function with GL_RG image and clear only GL_RED component", GL_RG, GL_RG32F,
            GL_FLOAT, 2, 0, clearImgClearWithRedOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_red_component_rg_image",
            "tests glClearTexImage function with GL_RG image and clear only GL_RED component", GL_RG, GL_RG16,
            GL_UNSIGNED_SHORT, 2, 0, clearImgClearWithRedOpts));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_red_component_red_image",
            "tests glClearTexImage function with GL_RED image and clear only GL_RED component", GL_RED, GL_R32F,
            GL_FLOAT, 1, 0, clearImgClearWithRedOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_red_component_red_image",
            "tests glClearTexImage function with GL_RED image and clear only GL_RED component", GL_RED, GL_R32F,
            GL_UNSIGNED_SHORT, 1, 0, clearImgClearWithRedOpts));

        ClearTexAndSubImageTest::TestOptions clearImgClearWithRgOpts{true, false, false, false, true, false, false};
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_rg_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RG component", GL_RGBA, GL_RGBA32F,
            GL_FLOAT, 4, 0, clearImgClearWithRgOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_rg_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RG component", GL_RGBA, GL_RGBA16,
            GL_UNSIGNED_SHORT, 4, 0, clearImgClearWithRgOpts));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_rg_component_rgb_image",
            "tests glClearTexImage function with GL_RGB image and clear only GL_RG component", GL_RGB, GL_RGB32F,
            GL_FLOAT, 3, 0, clearImgClearWithRgOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_rg_component_rgb_image",
            "tests glClearTexImage function with GL_RGB image and clear only GL_RG component", GL_RGB, GL_RGB16,
            GL_UNSIGNED_SHORT, 3, 0, clearImgClearWithRgOpts));

        ClearTexAndSubImageTest::TestOptions clearImgClearWithRgbOpts{true, false, false, false, false, false, true};
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_rga_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RGB component", GL_RGBA, GL_RGBA32F,
            GL_FLOAT, 4, 0, clearImgClearWithRgbOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_image_clear_rga_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RGB component", GL_RGBA, GL_RGBA16,
            GL_UNSIGNED_SHORT, 4, 0, clearImgClearWithRgbOpts));
    }
    {
        // Tests glClearTexSubImage with GL_RGBA, GL_RGB, GL_RG, GL_RED image, type GL_FLOAT, GL_UNSIGNED_SHORT and clears with GL_RED, GL_RG, GL_RGB type GL_FLOAT
        ClearTexAndSubImageTest::TestOptions clearSubImgClearWithRedOpts{false, true, false, true, false, false, false};
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_red_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RED component", GL_RGBA, GL_RGBA32F,
            GL_FLOAT, 4, 0, clearSubImgClearWithRedOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_red_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RED component", GL_RGBA, GL_RGBA16,
            GL_UNSIGNED_SHORT, 4, 0, clearSubImgClearWithRedOpts));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_red_component_rgb_image",
            "tests glClearTexImage function with GL_RGB image and clear only GL_RED component", GL_RGB, GL_RGB32F,
            GL_FLOAT, 3, 0, clearSubImgClearWithRedOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_red_component_rgb_image",
            "tests glClearTexImage function with GL_RGB image and clear only GL_RED component", GL_RGB, GL_RGB16,
            GL_UNSIGNED_SHORT, 3, 0, clearSubImgClearWithRedOpts));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_red_component_rg_image",
            "tests glClearTexImage function with GL_RG image and clear only GL_RED component", GL_RG, GL_RG32F,
            GL_FLOAT, 2, 0, clearSubImgClearWithRedOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_red_component_rg_image",
            "tests glClearTexImage function with GL_RG image and clear only GL_RED component", GL_RG, GL_RG16,
            GL_UNSIGNED_SHORT, 2, 0, clearSubImgClearWithRedOpts));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_red_component_rg_image",
            "tests glClearTexImage function with GL_RED image and clear only GL_RED component", GL_RED, GL_R32F,
            GL_FLOAT, 1, 0, clearSubImgClearWithRedOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_red_component_rg_image",
            "tests glClearTexImage function with GL_RED image and clear only GL_RED component", GL_RED, GL_R16,
            GL_UNSIGNED_SHORT, 1, 0, clearSubImgClearWithRedOpts));

        ClearTexAndSubImageTest::TestOptions clearSubImgClearWithRgOpts{false, true, false, false, true, false, false};
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rg_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RG component", GL_RGBA, GL_RGBA32F,
            GL_FLOAT, 4, 0, clearSubImgClearWithRgOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rg_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RG component", GL_RGBA, GL_RGBA16,
            GL_UNSIGNED_SHORT, 4, 0, clearSubImgClearWithRgOpts));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rg_component_rgb_image",
            "tests glClearTexImage function with GL_RGB image and clear only GL_RG component", GL_RGB, GL_RGB32F,
            GL_FLOAT, 3, 0, clearSubImgClearWithRgOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rg_component_rgb_image",
            "tests glClearTexImage function with GL_RGB image and clear only GL_RG component", GL_RGB, GL_RGB16,
            GL_UNSIGNED_SHORT, 3, 0, clearSubImgClearWithRgOpts));

        ClearTexAndSubImageTest::TestOptions clearSubImgClearWithRgbOpts{false, true, false, false, false, false, true};
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rgb_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RGB component", GL_RGBA, GL_RGBA32F,
            GL_FLOAT, 4, 0, clearSubImgClearWithRgbOpts));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rgb_component_rgba_image",
            "tests glClearTexImage function with GL_RGBA image and clear only GL_RGB component", GL_RGBA, GL_RGBA16,
            GL_UNSIGNED_SHORT, 4, 0, clearSubImgClearWithRgbOpts));
    }
    {
        //Tests glClearTexImage and glClearTexSubImage with GL_RG, GL_RED image, type GL_FLOAT, GL_UNSIGNED_SHORT and clears with GL_RGBA, GL_RGB
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rgba_component_red_image",
                                             "tests glClearTexImage function with GL_RED image and clear with GL_RGBA",
                                             GL_RED, GL_R32F, GL_FLOAT, 1, 0,
                                             {true, false, false, false, false, true, false}));
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rgba_component_red_image",
                                             "tests glClearTexImage function with GL_RED image and clear with GL_RGBA",
                                             GL_RED, GL_R16, GL_UNSIGNED_SHORT, 1, 0,
                                             {true, false, false, false, false, true, false}));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rgba_component_red_image",
            "tests glClearTexSubImage function with GL_RED image and clear with GL_RGBA", GL_RED, GL_R32F, GL_FLOAT, 1,
            0, {false, true, false, false, false, true, false}));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rgba_component_red_image",
            "tests glClearTexSubImage function with GL_RED image and clear with GL_RGBA", GL_RED, GL_R16,
            GL_UNSIGNED_SHORT, 1, 0, {false, true, false, false, false, true, false}));

        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rgba_component_rg_image",
                                             "tests glClearTexImage function with GL_RG image and clear with GL_RGBA",
                                             GL_RG, GL_RG32F, GL_FLOAT, 2, 0,
                                             {true, false, false, false, false, true, false}));
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rgba_component_rg_image",
                                             "tests glClearTexImage function with GL_RG image and clear with GL_RGBA",
                                             GL_RG, GL_RG16, GL_UNSIGNED_SHORT, 2, 0,
                                             {true, false, false, false, false, true, false}));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rgba_component_rg_image",
            "tests glClearTexSubImage function with GL_RG image and clear with GL_RGBA", GL_RG, GL_RG32F, GL_FLOAT, 2,
            0, {false, true, false, false, false, true, false}));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rgba_component_rg_image",
            "tests glClearTexSubImage function with GL_RG image and clear with GL_RGBA", GL_RG, GL_RG16,
            GL_UNSIGNED_SHORT, 2, 0, {false, true, false, false, false, true, false}));

        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rgb_component_red_image",
                                             "tests glClearTexImage function with GL_RED image and clear with GL_RGB",
                                             GL_RED, GL_R32F, GL_FLOAT, 1, 0,
                                             {true, false, false, false, false, false, true}));
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rgb_component_red_image",
                                             "tests glClearTexImage function with GL_RED image and clear with GL_RGB",
                                             GL_RED, GL_R16, GL_UNSIGNED_SHORT, 1, 0,
                                             {true, false, false, false, false, false, true}));

        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rgb_component_red_image",
            "tests glClearTexSubImage function with GL_RED image and clear with GL_RGB", GL_RED, GL_R32F, GL_FLOAT, 1,
            0, {false, true, false, false, false, false, true}));
        addChild(new ClearTexAndSubImageTest(
            m_context, "gl_clear_tex_sub_image_clear_rgb_component_red_image",
            "tests glClearTexSubImage function with GL_RED image and clear with GL_RGB", GL_RED, GL_R16,
            GL_UNSIGNED_SHORT, 1, 0, {false, true, false, false, false, false, true}));

        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rgb_component_rg_image",
                                             "tests glClearTexImage function with GL_RG image and clear with GL_RGB",
                                             GL_RG, GL_RG32F, GL_FLOAT, 2, 0,
                                             {true, false, false, false, false, false, true}));
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rgb_component_rg_image",
                                             "tests glClearTexImage function with GL_RG image and clear with GL_RGB",
                                             GL_RG, GL_RG16, GL_UNSIGNED_SHORT, 2, 0,
                                             {true, false, false, false, false, false, true}));

        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_sub_image_clear_rgb_component_rg_image",
                                             "tests glClearTexSubImage function with GL_RG image and clear with GL_RGB",
                                             GL_RG, GL_RG32F, GL_FLOAT, 2, 0,
                                             {false, true, false, false, false, false, true}));
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_sub_image_clear_rgb_component_rg_image",
                                             "tests glClearTexSubImage function with GL_RG image and clear with GL_RGB",
                                             GL_RG, GL_RG16, GL_UNSIGNED_SHORT, 2, 0,
                                             {false, true, false, false, false, false, true}));

        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rg_component_red_image",
                                             "tests glClearTexImage function with GL_RED image and clear with GL_RG",
                                             GL_RED, GL_R32F, GL_FLOAT, 1, 0,
                                             {true, false, false, false, true, false, false}));
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_image_clear_rg_component_red_image",
                                             "tests glClearTexImage function with GL_RED image and clear with GL_RG",
                                             GL_RED, GL_R16, GL_UNSIGNED_SHORT, 1, 0,
                                             {true, false, false, false, true, false, false}));

        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_sub_image_clear_rg_component_red_image",
                                             "tests glClearTexSubImage function with GL_RED image and clear with GL_RG",
                                             GL_RED, GL_R32F, GL_FLOAT, 1, 0,
                                             {false, true, false, false, true, false, false}));
        addChild(new ClearTexAndSubImageTest(m_context, "gl_clear_tex_sub_image_clear_rg_component_red_image",
                                             "tests glClearTexSubImage function with GL_RED image and clear with GL_RG",
                                             GL_RED, GL_R16, GL_UNSIGNED_SHORT, 1, 0,
                                             {false, true, false, false, true, false, false}));
    }

    {
        // Negative tests for glClearTexImage and glClearTexSubImage
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_image_texture_zero",
            "tests glClearTexImage and glClearTexSubImage with texture equal to zero", textureEqualZeroTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(m_context, "negative_gl_clear_tex_and_sub_image_buffer_texture",
                                                     "tests glClearTexImage and glClearTexSubImage with buffer texture",
                                                     bufferTextureTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_image_compressed_texture",
            "tests glClearTexImage and glClearTexSubImage with compressed texture", compressedTextureTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(m_context, "negative_gl_clear_tex_sub_image_negative_dimension",
                                                     "tests glClearTexSubImage with negative_dimension",
                                                     negativeDimensionTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_image_depth_component_is_internal_format_but_format_not",
            "tests glClearTexImage and glClearTexSubImage using texture with DEPTH_COMPONENT as internalFormat but not "
            "format",
            depthComponentIsInternalFormatButFormatNotTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_image_depth_stencil_is_internal_format_but_format_not",
            "tests glClearTexImage and glClearTexSubImage using texture with DEPTH_STENCIL as internalFormat but not "
            "format",
            depthStencilIsInternalFormatButFormatNotTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_image_stencil_index_is_internal_format_but_format_not",
            "tests glClearTexImage and glClearTexSubImage using texture with STENCIL_INDEX as internalFormat but not "
            "format",
            stencilIndexIsInternalFormatButFormatNotTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_rgba_is_internal_format_but_depth_component_format",
            "tests glClearTexImage and glClearTexSubImage using texture with RGBA as internalFormat but format "
            "GL_DEPTH_COMPONENT",
            rgbaIsInternalFormatButFormatDepthComponentTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_rgba_is_internal_format_but_depth_stencil_format",
            "tests glClearTexImage and glClearTexSubImage using texture with RGBA as internalFormat but format "
            "GL_DEPTH_STENCIL",
            rgbaIsInternalFormatButFormatDepthStencilTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_rgba_is_internal_format_but_stencil_index_format",
            "tests glClearTexImage and glClearTexSubImage using texture with RGBA as internalFormat but format "
            "GL_STENCIL_INDEX",
            rgbaIsInternalFormatButFormatStencilIndexTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_integer_is_internal_format_but_format_not_integer",
            "tests glClearTexImage and glClearTexSubImage using texture with RGBA16 as internalFormat but format does "
            "not "
            "specify internal data",
            integerIsInternalFormatButFormatNotIntegerTestCase));
        addChild(new ClearTexAndSubImageNegativeTest(
            m_context, "negative_gl_clear_tex_and_sub_not_integer_internal_format_but_format_integer",
            "tests glClearTexImage and glClearTexSubImage using texture with DEPTH_COMPONENT as internalFormat but "
            "format "
            "specify internal data",
            integerIsNotInternalFormatButFormatIsIntegerTestCase));
    }
}

} // namespace gl4cts
