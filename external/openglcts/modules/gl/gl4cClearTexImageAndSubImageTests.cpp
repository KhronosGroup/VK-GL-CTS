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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

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
    , m_width(0)
    , m_height(0)
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
    DE_ASSERT(m_texLevel >= 0);

    glw::GLint max_width  = gl_max_texture_size >> m_texLevel;
    glw::GLint max_height = gl_max_texture_size >> m_texLevel;

    m_width  = std::min(max_width, context.getRenderTarget().getWidth());
    m_height = std::min(max_height, context.getRenderTarget().getHeight());

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

    if (m_pixelSize < 4)
        gl.pixelStorei(GL_PACK_ALIGNMENT, 1);

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

    if (m_pixelSize < 4)
        gl.pixelStorei(GL_PACK_ALIGNMENT, 1);

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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
                for (size_t w = 0; w < ((m_width / 2) * m_pixelSize); w += m_pixelSize)
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
            for (size_t w = 0; w < (m_width / 2 * m_pixelSize); ++w)
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

namespace
{

struct ByteBlock16
{
    uint8_t bytes[16];
};

ByteBlock16 makeZeroBytes(void)
{
    ByteBlock16 b{};
    return b;
}

ByteBlock16 fromU8(std::initializer_list<uint8_t> vals)
{
    ByteBlock16 b{};
    size_t idx = 0;
    for (uint8_t v : vals)
    {
        if (idx < 16)
            b.bytes[idx++] = v;
    }
    return b;
}

ByteBlock16 fromI8(std::initializer_list<int8_t> vals)
{
    ByteBlock16 b{};
    size_t idx = 0;
    for (int8_t v : vals)
    {
        if (idx < 16)
            std::memcpy(b.bytes + (idx++), &v, sizeof(int8_t));
    }
    return b;
}

ByteBlock16 fromU16(std::initializer_list<uint16_t> vals)
{
    ByteBlock16 b{};
    size_t offset = 0;
    for (uint16_t v : vals)
    {
        if (offset + sizeof(uint16_t) <= 16)
        {
            std::memcpy(b.bytes + offset, &v, sizeof(uint16_t));
            offset += sizeof(uint16_t);
        }
    }
    return b;
}

ByteBlock16 fromI16(std::initializer_list<int16_t> vals)
{
    ByteBlock16 b{};
    size_t offset = 0;
    for (int16_t v : vals)
    {
        if (offset + sizeof(int16_t) <= 16)
        {
            std::memcpy(b.bytes + offset, &v, sizeof(int16_t));
            offset += sizeof(int16_t);
        }
    }
    return b;
}

ByteBlock16 fromU32(std::initializer_list<uint32_t> vals)
{
    ByteBlock16 b{};
    size_t offset = 0;
    for (uint32_t v : vals)
    {
        if (offset + sizeof(uint32_t) <= 16)
        {
            std::memcpy(b.bytes + offset, &v, sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }
    }
    return b;
}

ByteBlock16 fromI32(std::initializer_list<int32_t> vals)
{
    ByteBlock16 b{};
    size_t offset = 0;
    for (int32_t v : vals)
    {
        if (offset + sizeof(int32_t) <= 16)
        {
            std::memcpy(b.bytes + offset, &v, sizeof(int32_t));
            offset += sizeof(int32_t);
        }
    }
    return b;
}

ByteBlock16 fromF32(std::initializer_list<float> vals)
{
    ByteBlock16 b{};
    size_t offset = 0;
    for (float v : vals)
    {
        if (offset + sizeof(float) <= 16)
        {
            std::memcpy(b.bytes + offset, &v, sizeof(float));
            offset += sizeof(float);
        }
    }
    return b;
}

ClearTexImageViewTest::ViewClearSpec makeFullViewSpec(
    const char *name, const char *description, glw::GLenum parentTarget, glw::GLenum parentInternalFormat,
    glw::GLsizei parentWidth, glw::GLsizei parentHeight, glw::GLsizei parentDepth, glw::GLsizei parentLevels,
    glw::GLenum viewTarget, glw::GLenum viewInternalFormat, glw::GLuint viewMinLevel, glw::GLuint viewNumLevels,
    glw::GLuint viewMinLayer, glw::GLuint viewNumLayers, glw::GLenum clearFormat, glw::GLenum clearType,
    bool nullClearData, const ByteBlock16 &clearData, glw::GLsizei texelSizeBytes, const ByteBlock16 &expectedTexel)
{
    ClearTexImageViewTest::ViewClearSpec spec{};
    spec.name                 = name;
    spec.description          = description;
    spec.parentTarget         = parentTarget;
    spec.parentInternalFormat = parentInternalFormat;
    spec.parentWidth          = parentWidth;
    spec.parentHeight         = parentHeight;
    spec.parentDepth          = parentDepth;
    spec.parentLevels         = parentLevels;
    spec.useMidView           = false;
    spec.midTarget            = 0;
    spec.midInternalFormat    = 0;
    spec.midMinLevel          = 0;
    spec.midNumLevels         = 0;
    spec.midMinLayer          = 0;
    spec.midNumLayers         = 0;
    spec.viewTarget           = viewTarget;
    spec.viewInternalFormat   = viewInternalFormat;
    spec.viewMinLevel         = viewMinLevel;
    spec.viewNumLevels        = viewNumLevels;
    spec.viewMinLayer         = viewMinLayer;
    spec.viewNumLayers        = viewNumLayers;
    spec.useSubImage          = false;
    spec.clearLevel           = 0;
    spec.xoffset              = 0;
    spec.yoffset              = 0;
    spec.zoffset              = 0;
    spec.clearWidth           = 0;
    spec.clearHeight          = 0;
    spec.clearDepth           = 0;
    spec.clearFormat          = clearFormat;
    spec.clearType            = clearType;
    spec.nullClearData        = nullClearData;
    std::memcpy(spec.clearDataBytes, clearData.bytes, 16);
    spec.texelSizeBytes = texelSizeBytes;
    std::memcpy(spec.expectedTexelBytes, expectedTexel.bytes, 16);
    return spec;
}

ClearTexImageViewTest::ViewClearSpec makeFullViewLevelSpec(
    const char *name, const char *description, glw::GLenum parentTarget, glw::GLenum parentInternalFormat,
    glw::GLsizei parentWidth, glw::GLsizei parentHeight, glw::GLsizei parentDepth, glw::GLsizei parentLevels,
    glw::GLenum viewTarget, glw::GLenum viewInternalFormat, glw::GLuint viewMinLevel, glw::GLuint viewNumLevels,
    glw::GLuint viewMinLayer, glw::GLuint viewNumLayers, glw::GLint clearLevel, glw::GLenum clearFormat,
    glw::GLenum clearType, const ByteBlock16 &clearData, glw::GLsizei texelSizeBytes, const ByteBlock16 &expectedTexel)
{
    ClearTexImageViewTest::ViewClearSpec spec =
        makeFullViewSpec(name, description, parentTarget, parentInternalFormat, parentWidth, parentHeight, parentDepth,
                         parentLevels, viewTarget, viewInternalFormat, viewMinLevel, viewNumLevels, viewMinLayer,
                         viewNumLayers, clearFormat, clearType, false, clearData, texelSizeBytes, expectedTexel);
    spec.clearLevel = clearLevel;
    return spec;
}

ClearTexImageViewTest::ViewClearSpec makeSubViewSpec(
    const char *name, const char *description, glw::GLenum parentTarget, glw::GLenum parentInternalFormat,
    glw::GLsizei parentWidth, glw::GLsizei parentHeight, glw::GLsizei parentDepth, glw::GLsizei parentLevels,
    glw::GLenum viewTarget, glw::GLenum viewInternalFormat, glw::GLuint viewMinLevel, glw::GLuint viewNumLevels,
    glw::GLuint viewMinLayer, glw::GLuint viewNumLayers, glw::GLint clearLevel, glw::GLint xoffset, glw::GLint yoffset,
    glw::GLint zoffset, glw::GLsizei clearWidth, glw::GLsizei clearHeight, glw::GLsizei clearDepth,
    glw::GLenum clearFormat, glw::GLenum clearType, const ByteBlock16 &clearData, glw::GLsizei texelSizeBytes,
    const ByteBlock16 &expectedTexel)
{
    ClearTexImageViewTest::ViewClearSpec spec =
        makeFullViewSpec(name, description, parentTarget, parentInternalFormat, parentWidth, parentHeight, parentDepth,
                         parentLevels, viewTarget, viewInternalFormat, viewMinLevel, viewNumLevels, viewMinLayer,
                         viewNumLayers, clearFormat, clearType, false, clearData, texelSizeBytes, expectedTexel);
    spec.useSubImage = true;
    spec.clearLevel  = clearLevel;
    spec.xoffset     = xoffset;
    spec.yoffset     = yoffset;
    spec.zoffset     = zoffset;
    spec.clearWidth  = clearWidth;
    spec.clearHeight = clearHeight;
    spec.clearDepth  = clearDepth;
    return spec;
}

ClearTexImageViewTest::ViewClearSpec makeChainedViewSpec(
    const char *name, const char *description, glw::GLenum parentTarget, glw::GLenum parentInternalFormat,
    glw::GLsizei parentWidth, glw::GLsizei parentHeight, glw::GLsizei parentDepth, glw::GLsizei parentLevels,
    glw::GLenum midTarget, glw::GLenum midInternalFormat, glw::GLuint midMinLevel, glw::GLuint midNumLevels,
    glw::GLuint midMinLayer, glw::GLuint midNumLayers, glw::GLenum viewTarget, glw::GLenum viewInternalFormat,
    glw::GLuint viewMinLevel, glw::GLuint viewNumLevels, glw::GLuint viewMinLayer, glw::GLuint viewNumLayers,
    bool useSubImage, glw::GLint clearLevel, glw::GLint xoffset, glw::GLint yoffset, glw::GLint zoffset,
    glw::GLsizei clearWidth, glw::GLsizei clearHeight, glw::GLsizei clearDepth, glw::GLenum clearFormat,
    glw::GLenum clearType, const ByteBlock16 &clearData, glw::GLsizei texelSizeBytes, const ByteBlock16 &expectedTexel)
{
    ClearTexImageViewTest::ViewClearSpec spec =
        makeFullViewSpec(name, description, parentTarget, parentInternalFormat, parentWidth, parentHeight, parentDepth,
                         parentLevels, viewTarget, viewInternalFormat, viewMinLevel, viewNumLevels, viewMinLayer,
                         viewNumLayers, clearFormat, clearType, false, clearData, texelSizeBytes, expectedTexel);
    spec.useMidView        = true;
    spec.midTarget         = midTarget;
    spec.midInternalFormat = midInternalFormat;
    spec.midMinLevel       = midMinLevel;
    spec.midNumLevels      = midNumLevels;
    spec.midMinLayer       = midMinLayer;
    spec.midNumLayers      = midNumLayers;
    spec.useSubImage       = useSubImage;
    spec.clearLevel        = clearLevel;
    spec.xoffset           = xoffset;
    spec.yoffset           = yoffset;
    spec.zoffset           = zoffset;
    spec.clearWidth        = clearWidth;
    spec.clearHeight       = clearHeight;
    spec.clearDepth        = clearDepth;
    return spec;
}

decltype(glw::Functions::clearTexImage) getClearTexImageFunctionOptional(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.clearTexImage != nullptr)
        return gl.clearTexImage;
    return (decltype(glw::Functions::clearTexImage))context.getRenderContext().getProcAddress("glClearTexImage");
}

decltype(glw::Functions::clearTexSubImage) getClearTexSubImageFunctionOptional(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.clearTexSubImage != nullptr)
        return gl.clearTexSubImage;
    return (decltype(glw::Functions::clearTexSubImage))context.getRenderContext().getProcAddress("glClearTexSubImage");
}

decltype(glw::Functions::textureView) getTextureViewFunctionOptional(deqp::Context &context)
{
    const auto &gl = context.getRenderContext().getFunctions();
    if (gl.textureView != nullptr)
        return gl.textureView;
    return (decltype(glw::Functions::textureView))context.getRenderContext().getProcAddress("glTextureView");
}

bool bytesEqual(const uint8_t *a, const uint8_t *b, glw::GLsizei count)
{
    for (glw::GLsizei i = 0; i < count; ++i)
    {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

std::string formatBytesHex(const uint8_t *bytes, glw::GLsizei count)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (glw::GLsizei i = 0; i < count; ++i)
    {
        if (i > 0)
            oss << ' ';
        oss << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    return oss.str();
}

void getHelperIntegerFormat(glw::GLsizei texelSizeBytes, glw::GLenum &internalFormat, glw::GLenum &format,
                            glw::GLenum &type)
{
    switch (texelSizeBytes)
    {
    case 1:
        internalFormat = GL_R8UI;
        format         = GL_RED_INTEGER;
        type           = GL_UNSIGNED_BYTE;
        break;
    case 2:
        internalFormat = GL_R16UI;
        format         = GL_RED_INTEGER;
        type           = GL_UNSIGNED_SHORT;
        break;
    case 3:
        internalFormat = GL_RGB8UI;
        format         = GL_RGB_INTEGER;
        type           = GL_UNSIGNED_BYTE;
        break;
    case 4:
        internalFormat = GL_R32UI;
        format         = GL_RED_INTEGER;
        type           = GL_UNSIGNED_INT;
        break;
    case 6:
        internalFormat = GL_RGB16UI;
        format         = GL_RGB_INTEGER;
        type           = GL_UNSIGNED_SHORT;
        break;
    case 8:
        internalFormat = GL_RG32UI;
        format         = GL_RG_INTEGER;
        type           = GL_UNSIGNED_INT;
        break;
    case 12:
        internalFormat = GL_RGB32UI;
        format         = GL_RGB_INTEGER;
        type           = GL_UNSIGNED_INT;
        break;
    case 16:
        internalFormat = GL_RGBA32UI;
        format         = GL_RGBA_INTEGER;
        type           = GL_UNSIGNED_INT;
        break;
    default:
        TCU_THROW(InternalError, "Unsupported texel byte size");
    }
}

std::vector<ClearTexImageViewTest::ViewClearSpec> getClearTexImageViewSpecs(void)
{
    return {
        // 8-bit view class (VIEW_CLASS_8_BITS)
        makeFullViewSpec("texture_view_r8ui_on_r8_snorm_neg_max",
                         "glClearTexImage through GL_R8UI view of GL_R8_SNORM preserves 0x80 bit pattern",
                         GL_TEXTURE_2D, GL_R8_SNORM, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R8UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_BYTE, false, fromU8({0x80}), 1, fromU8({0x80})),
        makeFullViewSpec("texture_view_r8i_on_r8_snorm_neg_max",
                         "glClearTexImage through signed GL_R8I view of GL_R8_SNORM preserves -128 (0x80) bit pattern",
                         GL_TEXTURE_2D, GL_R8_SNORM, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R8I, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_BYTE, false, fromI8({-128}), 1, fromU8({0x80})),
        makeFullViewSpec("texture_view_r8_on_r8ui_reverse",
                         "glClearTexImage through normalized GL_R8 view of integer GL_R8UI writes encoded UNORM byte",
                         GL_TEXTURE_2D, GL_R8UI, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R8, 0, 1, 0, 1, GL_RED, GL_UNSIGNED_BYTE,
                         false, fromU8({0x55}), 1, fromU8({0x55})),

        // 16-bit view class (VIEW_CLASS_16_BITS)
        makeFullViewSpec("texture_view_r16ui_on_r16_snorm_neg_max",
                         "glClearTexImage through GL_R16UI view of GL_R16_SNORM preserves 0x8000 bit pattern",
                         GL_TEXTURE_2D, GL_R16_SNORM, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R16UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_SHORT, false, fromU16({0x8000u}), 2, fromU16({0x8000u})),
        makeFullViewSpec("texture_view_r16ui_on_r16f_snan",
                         "glClearTexImage through GL_R16UI view of GL_R16F preserves float16 signaling NaN bits",
                         GL_TEXTURE_2D, GL_R16F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R16UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_SHORT, false, fromU16({0x7c01u}), 2, fromU16({0x7c01u})),
        makeFullViewSpec("texture_view_rg8_on_r16f_subnormal",
                         "glClearTexImage through GL_RG8 view of GL_R16F preserves float16 subnormal bits",
                         GL_TEXTURE_2D, GL_R16F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG8, 0, 1, 0, 1, GL_RG, GL_UNSIGNED_BYTE,
                         false, fromU8({0x01, 0x00}), 2, fromU16({0x0001u})),
        makeFullViewSpec("texture_view_r16_on_r16f_subnormal",
                         "glClearTexImage through normalized GL_R16 view of GL_R16F preserves float16 subnormal bits",
                         GL_TEXTURE_2D, GL_R16F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R16, 0, 1, 0, 1, GL_RED,
                         GL_UNSIGNED_SHORT, false, fromU16({0x0001u}), 2, fromU16({0x0001u})),
        makeFullViewSpec("texture_view_rg8ui_on_r16f_neg_subnormal",
                         "glClearTexImage through GL_RG8UI view of GL_R16F preserves negative float16 subnormal bits",
                         GL_TEXTURE_2D, GL_R16F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG8UI, 0, 1, 0, 1, GL_RG_INTEGER,
                         GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x80}), 2, fromU16({0x8001u})),
        makeFullViewSpec("texture_view_r16i_on_r16f_subnormal",
                         "glClearTexImage through signed GL_R16I view of GL_R16F preserves float16 subnormal bits",
                         GL_TEXTURE_2D, GL_R16F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R16I, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_SHORT, false, fromI16({1}), 2, fromU16({0x0001u})),
        makeFullViewSpec("texture_view_r16f_on_r16ui_reverse",
                         "glClearTexImage through float GL_R16F view of integer GL_R16UI writes IEEE binary16 bits",
                         GL_TEXTURE_2D, GL_R16UI, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R16F, 0, 1, 0, 1, GL_RED, GL_FLOAT,
                         false, fromF32({1.0f}), 2, fromU16({0x3c00u})),

        // 24-bit view class (VIEW_CLASS_24_BITS)
        makeFullViewSpec("texture_view_rgb8_on_srgb8",
                         "glClearTexImage through linear GL_RGB8 view of GL_SRGB8 does not apply sRGB encoding",
                         GL_TEXTURE_2D, GL_SRGB8, 7, 5, 1, 1, GL_TEXTURE_2D, GL_RGB8, 0, 1, 0, 1, GL_RGB,
                         GL_UNSIGNED_BYTE, false, fromU8({0x40, 0x80, 0xc0}), 3, fromU8({0x40, 0x80, 0xc0})),
        makeFullViewSpec("texture_view_rgb8ui_on_rgb8_snorm_neg_max",
                         "glClearTexImage through GL_RGB8UI view of GL_RGB8_SNORM preserves 0x80 bit pattern",
                         GL_TEXTURE_2D, GL_RGB8_SNORM, 7, 5, 1, 1, GL_TEXTURE_2D, GL_RGB8UI, 0, 1, 0, 1, GL_RGB_INTEGER,
                         GL_UNSIGNED_BYTE, false, fromU8({0x80, 0x7f, 0x01}), 3, fromU8({0x80, 0x7f, 0x01})),
        makeFullViewSpec("texture_view_rgb8i_on_srgb8",
                         "glClearTexImage through signed GL_RGB8I view of GL_SRGB8 preserves raw 24-bit bytes",
                         GL_TEXTURE_2D, GL_SRGB8, 7, 5, 1, 1, GL_TEXTURE_2D, GL_RGB8I, 0, 1, 0, 1, GL_RGB_INTEGER,
                         GL_BYTE, false, fromI8({-128, -1, 1}), 3, fromU8({0x80, 0xff, 0x01})),
        makeSubViewSpec(
            "texture_view_sub_image_rgb8_on_rgb8_snorm_neg_max",
            "glClearTexSubImage through GL_RGB8 view of GL_RGB8_SNORM preserves 0x80 and outer 24-bit texels",
            GL_TEXTURE_2D, GL_RGB8_SNORM, 7, 5, 1, 1, GL_TEXTURE_2D, GL_RGB8, 0, 1, 0, 1, 0, 1, 1, 0, 4, 3, 1, GL_RGB,
            GL_UNSIGNED_BYTE, fromU8({0x80, 0x80, 0x80}), 3, fromU8({0x80, 0x80, 0x80})),

        // 32-bit view class (VIEW_CLASS_32_BITS)
        makeFullViewSpec("texture_view_rgba8_on_r32f_denorm",
                         "glClearTexImage through GL_RGBA8 view of GL_R32F preserves positive float32 denormal bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_rgba8_on_r32f_neg_denorm",
                         "glClearTexImage through GL_RGBA8 view of GL_R32F preserves negative float32 denormal bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, false, fromU8({0xff, 0xff, 0x7f, 0x80}), 4, fromU32({0x807fffffu})),
        makeFullViewSpec("texture_view_r32ui_on_r32f_denorm",
                         "glClearTexImage through GL_R32UI view of GL_R32F preserves float32 denormal bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0x00000001u}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_r32ui_on_r32f_qnan_payload",
                         "glClearTexImage through GL_R32UI view of GL_R32F preserves float32 quiet NaN payload bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0x7fc12345u}), 4, fromU32({0x7fc12345u})),
        makeFullViewSpec("texture_view_r32ui_on_r32f_snan",
                         "glClearTexImage through GL_R32UI view of GL_R32F preserves float32 signaling NaN bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0x7f800001u}), 4, fromU32({0x7f800001u})),
        makeFullViewSpec("texture_view_rg16f_on_r32f_denorm",
                         "glClearTexImage through GL_RG16F view of GL_R32F preserves float32 denormal 0x00003c00 bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG16F, 0, 1, 0, 1, GL_RG, GL_FLOAT,
                         false, fromF32({1.0f, 0.0f}), 4, fromU32({0x00003c00u})),
        makeFullViewSpec(
            "texture_view_rgba8_on_rg16f_subnormals",
            "glClearTexImage through GL_RGBA8 view of GL_RG16F preserves float16 subnormals in both channels",
            GL_TEXTURE_2D, GL_RG16F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_RGBA, GL_UNSIGNED_BYTE, false,
            fromU8({0x01, 0x00, 0x02, 0x80}), 4, fromU32({0x80020001u})),
        makeFullViewSpec("texture_view_rgb10_a2ui_on_r32f_denorm",
                         "glClearTexImage through GL_RGB10_A2UI view of GL_R32F preserves float32 denormal bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGB10_A2UI, 0, 1, 0, 1, GL_RGBA_INTEGER,
                         GL_UNSIGNED_INT_2_10_10_10_REV, false, fromU32({0x00000001u}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_r32ui_on_rgb10_a2ui",
                         "glClearTexImage through GL_R32UI view of GL_RGB10_A2UI preserves packed 10:10:10:2 bits",
                         GL_TEXTURE_2D, GL_RGB10_A2UI, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0xc0100401u}), 4, fromU32({0xc0100401u})),
        makeFullViewSpec("texture_view_r32i_on_r32f_denorm",
                         "glClearTexImage through signed GL_R32I view of GL_R32F preserves float32 denormal bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32I, 0, 1, 0, 1, GL_RED_INTEGER, GL_INT,
                         false, fromI32({1}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_rgba8i_on_r32f_denorm",
                         "glClearTexImage through signed GL_RGBA8I view of GL_R32F preserves float32 denormal bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8I, 0, 1, 0, 1, GL_RGBA_INTEGER,
                         GL_BYTE, false, fromI8({1, 0, 0, 0}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_rg16_on_r32f_denorm",
                         "glClearTexImage through normalized GL_RG16 view of GL_R32F preserves float32 denormal bits",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG16, 0, 1, 0, 1, GL_RG,
                         GL_UNSIGNED_SHORT, false, fromU16({0x0001u, 0x0000u}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_rgba8_on_rgba8_snorm_neg_max",
                         "glClearTexImage through GL_RGBA8 view of GL_RGBA8_SNORM preserves 0x80808080 bits",
                         GL_TEXTURE_2D, GL_RGBA8_SNORM, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, false, fromU8({0x80, 0x80, 0x80, 0x80}), 4, fromU32({0x80808080u})),
        makeFullViewSpec("texture_view_rgba8_on_srgb8_alpha8",
                         "glClearTexImage through linear GL_RGBA8 view of GL_SRGB8_ALPHA8 does not apply sRGB encode",
                         GL_TEXTURE_2D, GL_SRGB8_ALPHA8, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, false, fromU8({0x40, 0x80, 0xc0, 0xff}), 4, fromU32({0xffc08040u})),
        makeFullViewSpec(
            "texture_view_srgb8_alpha8_on_r32f_denorm",
            "glClearTexImage through GL_SRGB8_ALPHA8 view of GL_R32F stores raw ubyte bits without sRGB encode",
            GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_SRGB8_ALPHA8, 0, 1, 0, 1, GL_RGBA, GL_UNSIGNED_BYTE,
            false, fromU8({0x40, 0x40, 0x40, 0x00}), 4, fromU32({0x00404040u})),
        makeFullViewSpec("texture_view_r32ui_on_r11f_g11f_b10f",
                         "glClearTexImage through GL_R32UI view of GL_R11F_G11F_B10F preserves arbitrary raw bits",
                         GL_TEXTURE_2D, GL_R11F_G11F_B10F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 0, 1,
                         GL_RED_INTEGER, GL_UNSIGNED_INT, false, fromU32({0xffe00001u}), 4, fromU32({0xffe00001u})),
        makeFullViewSpec("texture_view_r32ui_on_r11f_g11f_b10f_subnormal",
                         "glClearTexImage through GL_R32UI view of GL_R11F_G11F_B10F preserves 11f/10f subnormal bits",
                         GL_TEXTURE_2D, GL_R11F_G11F_B10F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 0, 1,
                         GL_RED_INTEGER, GL_UNSIGNED_INT, false, fromU32({0x00400801u}), 4, fromU32({0x00400801u})),
        makeFullViewSpec("texture_view_r32ui_on_rgb9_e5",
                         "glClearTexImage through GL_R32UI view of non-renderable GL_RGB9_E5 preserves raw bits",
                         GL_TEXTURE_2D, GL_RGB9_E5, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0xdeadbeefu}), 4, fromU32({0xdeadbeefu})),
        makeFullViewSpec("texture_view_r32ui_on_rgb10_a2",
                         "glClearTexImage through GL_R32UI view of GL_RGB10_A2 preserves raw bits", GL_TEXTURE_2D,
                         GL_RGB10_A2, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 0, 1, GL_RED_INTEGER, GL_UNSIGNED_INT,
                         false, fromU32({0x92345678u}), 4, fromU32({0x92345678u})),
        makeFullViewSpec("texture_view_rgba8_on_r32ui_reverse",
                         "glClearTexImage through normalized GL_RGBA8 view of integer GL_R32UI writes 32-bit word",
                         GL_TEXTURE_2D, GL_R32UI, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x02, 0x03, 0x04}), 4, fromU32({0x04030201u})),
        makeFullViewSpec("texture_view_rg16f_on_r32ui_reverse",
                         "glClearTexImage through float GL_RG16F view of integer GL_R32UI writes packed binary16 bits",
                         GL_TEXTURE_2D, GL_R32UI, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG16F, 0, 1, 0, 1, GL_RG, GL_FLOAT,
                         false, fromF32({1.0f, -2.0f}), 4, fromU32({0xc0003c00u})),

        // 48-bit view class (VIEW_CLASS_48_BITS)
        makeFullViewSpec("texture_view_rgb16ui_on_rgb16f_subnormals",
                         "glClearTexImage through GL_RGB16UI view of GL_RGB16F preserves subnormal and sNaN bits",
                         GL_TEXTURE_2D, GL_RGB16F, 7, 5, 1, 1, GL_TEXTURE_2D, GL_RGB16UI, 0, 1, 0, 1, GL_RGB_INTEGER,
                         GL_UNSIGNED_SHORT, false, fromU16({0x0001u, 0x8001u, 0x7c01u}), 6,
                         fromU16({0x0001u, 0x8001u, 0x7c01u})),
        makeFullViewSpec("texture_view_rgb16i_on_rgb16f_subnormals",
                         "glClearTexImage through signed GL_RGB16I view of GL_RGB16F preserves float16 subnormal bits",
                         GL_TEXTURE_2D, GL_RGB16F, 7, 5, 1, 1, GL_TEXTURE_2D, GL_RGB16I, 0, 1, 0, 1, GL_RGB_INTEGER,
                         GL_SHORT, false, fromI16({1, 2, -32767}), 6, fromU16({0x0001u, 0x0002u, 0x8001u})),
        makeFullViewSpec("texture_view_rgb16_on_rgb16f_subnormals",
                         "glClearTexImage through normalized GL_RGB16 view of GL_RGB16F preserves float16 subnormals",
                         GL_TEXTURE_2D, GL_RGB16F, 7, 5, 1, 1, GL_TEXTURE_2D, GL_RGB16, 0, 1, 0, 1, GL_RGB,
                         GL_UNSIGNED_SHORT, false, fromU16({0x0001u, 0x0002u, 0x0003u}), 6,
                         fromU16({0x0001u, 0x0002u, 0x0003u})),
        makeSubViewSpec(
            "texture_view_sub_image_rgb16ui_on_rgb16_snorm_neg_max",
            "glClearTexSubImage through GL_RGB16UI view of GL_RGB16_SNORM preserves 0x8000 and outer texels",
            GL_TEXTURE_2D, GL_RGB16_SNORM, 7, 5, 1, 1, GL_TEXTURE_2D, GL_RGB16UI, 0, 1, 0, 1, 0, 2, 1, 0, 3, 3, 1,
            GL_RGB_INTEGER, GL_UNSIGNED_SHORT, fromU16({0x8000u, 0x8000u, 0x8000u}), 6,
            fromU16({0x8000u, 0x8000u, 0x8000u})),

        // 64-bit view class (VIEW_CLASS_64_BITS)
        makeFullViewSpec("texture_view_rgba16f_on_rg32f_denorm",
                         "glClearTexImage through GL_RGBA16F view of GL_RG32F preserves float32 denormal bits",
                         GL_TEXTURE_2D, GL_RG32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA16F, 0, 1, 0, 1, GL_RGBA, GL_FLOAT,
                         false, fromF32({1.0f, 0.0f, 0.0f, 1.0f}), 8, fromU32({0x00003c00u, 0x3c000000u})),
        makeFullViewSpec("texture_view_rg32ui_on_rg32f_denorm_snan",
                         "glClearTexImage through GL_RG32UI view of GL_RG32F preserves denormal and sNaN bits",
                         GL_TEXTURE_2D, GL_RG32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG32UI, 0, 1, 0, 1, GL_RG_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0x00000001u, 0x7f800001u}), 8,
                         fromU32({0x00000001u, 0x7f800001u})),
        makeFullViewSpec("texture_view_rgba16ui_on_rgba16f_snan_subnorm",
                         "glClearTexImage through GL_RGBA16UI view of GL_RGBA16F preserves sNaN and subnormal bits",
                         GL_TEXTURE_2D, GL_RGBA16F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA16UI, 0, 1, 0, 1, GL_RGBA_INTEGER,
                         GL_UNSIGNED_SHORT, false, fromU16({0x7c01u, 0x0001u, 0xfc01u, 0x8001u}), 8,
                         fromU32({0x00017c01u, 0x8001fc01u})),
        makeFullViewSpec("texture_view_rg32ui_on_rgba16_snorm_neg_max",
                         "glClearTexImage through GL_RG32UI view of GL_RGBA16_SNORM preserves 0x8000 bits",
                         GL_TEXTURE_2D, GL_RGBA16_SNORM, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG32UI, 0, 1, 0, 1,
                         GL_RG_INTEGER, GL_UNSIGNED_INT, false, fromU32({0x80008000u, 0x80008000u}), 8,
                         fromU32({0x80008000u, 0x80008000u})),
        makeFullViewSpec("texture_view_rgba16_on_rg32f_denorms",
                         "glClearTexImage through normalized GL_RGBA16 view of GL_RG32F preserves float32 denormals",
                         GL_TEXTURE_2D, GL_RG32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA16, 0, 1, 0, 1, GL_RGBA,
                         GL_UNSIGNED_SHORT, false, fromU16({0x0001u, 0x0000u, 0x0002u, 0x0000u}), 8,
                         fromU32({0x00000001u, 0x00000002u})),
        makeFullViewSpec("texture_view_rg32i_on_rg32f_denorms",
                         "glClearTexImage through signed GL_RG32I view of GL_RG32F preserves pos and neg denormals",
                         GL_TEXTURE_2D, GL_RG32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG32I, 0, 1, 0, 1, GL_RG_INTEGER,
                         GL_INT, false, fromI32({1, -2139095041}), 8, fromU32({0x00000001u, 0x807fffffu})),
        makeFullViewSpec("texture_view_rgba16i_on_rg32f_denorms",
                         "glClearTexImage through signed GL_RGBA16I view of GL_RG32F preserves float32 denormals",
                         GL_TEXTURE_2D, GL_RG32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA16I, 0, 1, 0, 1, GL_RGBA_INTEGER,
                         GL_SHORT, false, fromI16({1, 0, 1, -32768}), 8, fromU32({0x00000001u, 0x80000001u})),
        makeFullViewSpec("texture_view_rgba16f_on_rg32ui_reverse",
                         "glClearTexImage through float GL_RGBA16F view of integer GL_RG32UI writes binary16 bits",
                         GL_TEXTURE_2D, GL_RG32UI, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA16F, 0, 1, 0, 1, GL_RGBA, GL_FLOAT,
                         false, fromF32({1.0f, 0.0f, 0.0f, 1.0f}), 8, fromU32({0x00003c00u, 0x3c000000u})),

        // 96-bit view class (VIEW_CLASS_96_BITS)
        makeFullViewSpec("texture_view_rgb32ui_on_rgb32f_denorms_snan",
                         "glClearTexImage through GL_RGB32UI view of 96-bit GL_RGB32F preserves denormal and sNaN bits",
                         GL_TEXTURE_2D, GL_RGB32F, 7, 6, 1, 1, GL_TEXTURE_2D, GL_RGB32UI, 0, 1, 0, 1, GL_RGB_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0x00000001u, 0x807fffffu, 0x7f800001u}), 12,
                         fromU32({0x00000001u, 0x807fffffu, 0x7f800001u})),
        makeFullViewSpec("texture_view_rgb32i_on_rgb32f_denorms",
                         "glClearTexImage through signed GL_RGB32I view of 96-bit GL_RGB32F preserves denormal bits",
                         GL_TEXTURE_2D, GL_RGB32F, 7, 6, 1, 1, GL_TEXTURE_2D, GL_RGB32I, 0, 1, 0, 1, GL_RGB_INTEGER,
                         GL_INT, false, fromI32({1, 2, 3}), 12, fromU32({0x00000001u, 0x00000002u, 0x00000003u})),
        makeSubViewSpec(
            "texture_view_sub_image_rgb32ui_on_rgb32f_denorms_snan",
            "glClearTexSubImage through GL_RGB32UI view of 96-bit GL_RGB32F preserves bits and outer texels",
            GL_TEXTURE_2D, GL_RGB32F, 7, 6, 1, 1, GL_TEXTURE_2D, GL_RGB32UI, 0, 1, 0, 1, 0, 1, 2, 0, 4, 3, 1,
            GL_RGB_INTEGER, GL_UNSIGNED_INT, fromU32({0x00000001u, 0x807fffffu, 0x7f800001u}), 12,
            fromU32({0x00000001u, 0x807fffffu, 0x7f800001u})),

        // 128-bit view class (VIEW_CLASS_128_BITS)
        makeFullViewSpec("texture_view_rgba32ui_on_rgba32f_denorms_snan",
                         "glClearTexImage through GL_RGBA32UI view of GL_RGBA32F preserves denormal and sNaN bits",
                         GL_TEXTURE_2D, GL_RGBA32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA32UI, 0, 1, 0, 1, GL_RGBA_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0x00000001u, 0x807fffffu, 0x7f800001u, 0xff800001u}), 16,
                         fromU32({0x00000001u, 0x807fffffu, 0x7f800001u, 0xff800001u})),
        makeFullViewSpec("texture_view_rgba32i_on_rgba32f_denorms",
                         "glClearTexImage through signed GL_RGBA32I view of GL_RGBA32F preserves float32 denormals",
                         GL_TEXTURE_2D, GL_RGBA32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA32I, 0, 1, 0, 1, GL_RGBA_INTEGER,
                         GL_INT, false, fromI32({1, 2, -2139095041, 3}), 16,
                         fromU32({0x00000001u, 0x00000002u, 0x807fffffu, 0x00000003u})),
        makeFullViewSpec(
            "texture_view_rgba32f_on_rgba32ui_reverse",
            "glClearTexImage through float GL_RGBA32F view of integer GL_RGBA32UI writes IEEE binary32 bits",
            GL_TEXTURE_2D, GL_RGBA32UI, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA32F, 0, 1, 0, 1, GL_RGBA, GL_FLOAT, false,
            fromF32({1.0f, -2.0f, 0.5f, 0.0f}), 16, fromU32({0x3f800000u, 0xc0000000u, 0x3f000000u, 0x00000000u})),

        // Sub-image clears, non-zero minlevel, non-zero clearLevel, view-of-view chaining, and NULL data zero-clear
        makeSubViewSpec("texture_view_sub_image_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through GL_RGBA8 view of GL_R32F preserves denormal bits and outer texels",
                        GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, 0, 2, 1, 0, 3, 4, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_sub_image_rgba32ui_on_rgba32f_denorms_snan",
                        "glClearTexSubImage through GL_RGBA32UI view of GL_RGBA32F preserves bits and outer texels",
                        GL_TEXTURE_2D, GL_RGBA32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA32UI, 0, 1, 0, 1, 0, 1, 2, 0, 4, 3,
                        1, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                        fromU32({0x00000001u, 0x807fffffu, 0x7f800001u, 0xff800001u}), 16,
                        fromU32({0x00000001u, 0x807fffffu, 0x7f800001u, 0xff800001u})),
        makeFullViewSpec("texture_view_nonzero_minlevel_rgba8_on_r32f_denorm",
                         "glClearTexImage on GL_RGBA8 view with non-zero minlevel clears only selected mip of GL_R32F",
                         GL_TEXTURE_2D, GL_R32F, 16, 16, 1, 4, GL_TEXTURE_2D, GL_RGBA8, 2, 1, 0, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeFullViewLevelSpec("texture_view_nonzero_minlevel_and_clearlevel_full_rgba8_on_r32f",
                              "glClearTexImage with minlevel=1 and level=1 clears only parent mip 2 of GL_R32F",
                              GL_TEXTURE_2D, GL_R32F, 16, 16, 1, 4, GL_TEXTURE_2D, GL_RGBA8, 1, 3, 0, 1, 1, GL_RGBA,
                              GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_nonzero_minlevel_and_clearlevel_sub_rgba8_on_r32f",
                        "glClearTexSubImage with minlevel=1, minlayer=1, level=1 clears only parent mip 2 sub-box",
                        GL_TEXTURE_2D_ARRAY, GL_R32F, 16, 16, 4, 4, GL_TEXTURE_2D_ARRAY, GL_RGBA8, 1, 3, 1, 2, 1, 1, 1,
                        1, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeChainedViewSpec("texture_view_chained_2d_on_2d_array_on_2d_array_r32f_denorm",
                            "glClearTexImage on view-of-view accumulates minlevel and minlayer offsets on GL_R32F",
                            GL_TEXTURE_2D_ARRAY, GL_R32F, 16, 16, 8, 4, GL_TEXTURE_2D_ARRAY, GL_R32UI, 1, 3, 2, 4,
                            GL_TEXTURE_2D, GL_RGBA8, 1, 1, 1, 1, false, 0, 0, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                            fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeChainedViewSpec("texture_view_chained_2d_on_cube_map_on_cube_map_array_r32f_denorm",
                            "glClearTexSubImage on 2D view of CUBE_MAP view of CUBE_MAP_ARRAY clears single face box",
                            GL_TEXTURE_CUBE_MAP_ARRAY, GL_R32F, 8, 8, 12, 2, GL_TEXTURE_CUBE_MAP, GL_RG16F, 0, 2, 6, 6,
                            GL_TEXTURE_2D, GL_RGBA8, 1, 1, 2, 1, true, 0, 1, 1, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                            fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_null_data_rgba8_on_r32f",
                         "glClearTexImage with NULL data on GL_RGBA8 view of GL_R32F clears all bits to zero",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, true, makeZeroBytes(), 4, fromU32({0x00000000u})),

        // Upload format/type variants, component-subset expansion, and zero-dimension no-op clears on views
        makeFullViewSpec("texture_view_bgra_ubyte_on_rgba8_view_of_r32f_denorm",
                         "glClearTexImage with GL_BGRA swizzles R=1 into GL_RGBA8 view of GL_R32F as float32 denormal",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_BGRA,
                         GL_UNSIGNED_BYTE, false, fromU8({0x00, 0x00, 0x01, 0x00}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_half_float_on_rg16f_view_of_r32f_denorm",
                         "glClearTexImage with GL_HALF_FLOAT on GL_RG16F view of GL_R32F preserves float32 denormal",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG16F, 0, 1, 0, 1, GL_RG, GL_HALF_FLOAT,
                         false, fromU16({0x3c00u, 0x0000u}), 4, fromU32({0x00003c00u})),
        makeFullViewSpec("texture_view_packed_10f_11f_11f_on_r11f_g11f_b10f_view_of_r32f_denorm",
                         "glClearTexImage with GL_UNSIGNED_INT_10F_11F_11F_REV on GL_R11F_G11F_B10F view of GL_R32F",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_R11F_G11F_B10F, 0, 1, 0, 1, GL_RGB,
                         GL_UNSIGNED_INT_10F_11F_11F_REV, false, fromU32({0x00000001u}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_packed_5_9_9_9_on_rgb9_e5_view_of_r32f_denorm",
                         "glClearTexImage with GL_UNSIGNED_INT_5_9_9_9_REV on GL_RGB9_E5 view of GL_R32F",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGB9_E5, 0, 1, 0, 1, GL_RGB,
                         GL_UNSIGNED_INT_5_9_9_9_REV, false, fromU32({0x00000001u}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_red_subset_on_rgba8_view_of_r32f",
                         "glClearTexImage with GL_RED on GL_RGBA8 view of GL_R32F expands missing G=0, B=0, A=1.0",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, GL_RED,
                         GL_UNSIGNED_BYTE, false, fromU8({0x01}), 4, fromU32({0xff000001u})),
        makeFullViewSpec("texture_view_red_integer_subset_on_rgba8ui_view_of_r32f",
                         "glClearTexImage with GL_RED_INTEGER on GL_RGBA8UI view of GL_R32F expands G=0, B=0, A=1",
                         GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8UI, 0, 1, 0, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_BYTE, false, fromU8({0x01}), 4, fromU32({0x01000001u})),
        makeFullViewSpec(
            "texture_view_red_subset_on_rg16_view_of_r32f_denorm",
            "glClearTexImage with GL_RED on GL_RG16 view of GL_R32F expands G=0 producing float32 denormal",
            GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RG16, 0, 1, 0, 1, GL_RED, GL_UNSIGNED_SHORT, false,
            fromU16({0x0001u}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_sub_image_zero_width_noop",
                        "glClearTexSubImage with width=0 on GL_RGBA8 view of GL_R32F is a no-op", GL_TEXTURE_2D,
                        GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, 0, 2, 2, 0, 0, 4, 1, GL_RGBA,
                        GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_sub_image_zero_height_noop",
                        "glClearTexSubImage with height=0 on GL_RGBA8 view of GL_R32F is a no-op", GL_TEXTURE_2D,
                        GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 0, 1, 0, 2, 2, 0, 4, 0, 1, GL_RGBA,
                        GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_sub_image_zero_depth_noop",
                        "glClearTexSubImage with depth=0 on 2D_ARRAY GL_RGBA8 view of GL_R32F is a no-op",
                        GL_TEXTURE_2D_ARRAY, GL_R32F, 8, 8, 4, 1, GL_TEXTURE_2D_ARRAY, GL_RGBA8, 0, 1, 1, 2, 0, 2, 2, 0,
                        4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),

        // Complete Table 8.20 (origTarget -> viewTarget) matrix: 1D, 1D_ARRAY, 2D, RECTANGLE, 3D, 2D_ARRAY, CUBE, CUBE_ARRAY, MSAA
        makeSubViewSpec("texture_view_1d_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 1D GL_RGBA8 view of GL_R32F preserves denormal bits", GL_TEXTURE_1D,
                        GL_R32F, 16, 1, 1, 2, GL_TEXTURE_1D, GL_RGBA8, 0, 2, 0, 1, 0, 3, 0, 0, 8, 1, 1, GL_RGBA,
                        GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_1d_to_1d_array_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 1D_ARRAY view of 1D GL_R32F preserves denormal bits", GL_TEXTURE_1D,
                        GL_R32F, 16, 1, 1, 2, GL_TEXTURE_1D_ARRAY, GL_RGBA8, 0, 2, 0, 1, 1, 2, 0, 0, 4, 1, 1, GL_RGBA,
                        GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_1d_array_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 1D_ARRAY GL_RGBA8 view of GL_R32F with minlayer preserves bits",
                        GL_TEXTURE_1D_ARRAY, GL_R32F, 16, 4, 1, 2, GL_TEXTURE_1D_ARRAY, GL_RGBA8, 0, 2, 1, 2, 0, 2, 0,
                        0, 8, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_1d_array_r32ui_on_rgb9_e5",
                         "glClearTexImage through 1D_ARRAY GL_R32UI view of GL_RGB9_E5 with minlayer preserves bits",
                         GL_TEXTURE_1D_ARRAY, GL_RGB9_E5, 16, 4, 1, 1, GL_TEXTURE_1D_ARRAY, GL_R32UI, 0, 1, 1, 2,
                         GL_RED_INTEGER, GL_UNSIGNED_INT, false, fromU32({0xa5a5f00du}), 4, fromU32({0xa5a5f00du})),
        makeSubViewSpec(
            "texture_view_1d_array_to_1d_rgba8_on_r32f_denorm",
            "glClearTexSubImage through single-layer 1D view of 1D_ARRAY GL_R32F clears only selected layer",
            GL_TEXTURE_1D_ARRAY, GL_R32F, 16, 4, 1, 2, GL_TEXTURE_1D, GL_RGBA8, 0, 2, 2, 1, 0, 3, 0, 0, 6, 1, 1,
            GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_2d_to_2d_array_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 1-layer 2D_ARRAY view of 2D GL_R32F preserves denormal bits",
                        GL_TEXTURE_2D, GL_R32F, 8, 8, 1, 2, GL_TEXTURE_2D_ARRAY, GL_RGBA8, 0, 2, 0, 1, 0, 2, 1, 0, 4, 5,
                        1, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_rectangle_rgba8_on_r32f_denorm",
                         "glClearTexImage through GL_TEXTURE_RECTANGLE GL_RGBA8 view of GL_R32F preserves denormals",
                         GL_TEXTURE_RECTANGLE, GL_R32F, 10, 6, 1, 1, GL_TEXTURE_RECTANGLE, GL_RGBA8, 0, 1, 0, 1,
                         GL_RGBA, GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_rectangle_sub_image_r32ui_on_r32f_snan",
                        "glClearTexSubImage through GL_TEXTURE_RECTANGLE GL_R32UI view of GL_R32F preserves sNaN bits",
                        GL_TEXTURE_RECTANGLE, GL_R32F, 10, 6, 1, 1, GL_TEXTURE_RECTANGLE, GL_R32UI, 0, 1, 0, 1, 0, 2, 1,
                        0, 5, 3, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, fromU32({0x7f800001u}), 4, fromU32({0x7f800001u})),
        makeSubViewSpec("texture_view_2d_array_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 2D_ARRAY GL_RGBA8 view with minlevel and minlayer preserves bits",
                        GL_TEXTURE_2D_ARRAY, GL_R32F, 8, 8, 4, 2, GL_TEXTURE_2D_ARRAY, GL_RGBA8, 1, 1, 1, 2, 0, 1, 1, 0,
                        2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_2d_single_layer_on_2d_array_r32f_denorm",
                         "glClearTexImage through single-layer 2D GL_R32UI view of 2D_ARRAY GL_R32F clears one layer",
                         GL_TEXTURE_2D_ARRAY, GL_R32F, 8, 8, 4, 1, GL_TEXTURE_2D, GL_R32UI, 0, 1, 2, 1, GL_RED_INTEGER,
                         GL_UNSIGNED_INT, false, fromU32({0x00000001u}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_2d_array_to_cube_map_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through CUBE_MAP view of 2D_ARRAY GL_R32F clears selected cube faces",
                        GL_TEXTURE_2D_ARRAY, GL_R32F, 8, 8, 8, 1, GL_TEXTURE_CUBE_MAP, GL_RGBA8, 0, 1, 1, 6, 0, 1, 1, 1,
                        4, 4, 3, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeSubViewSpec(
            "texture_view_2d_array_to_cube_map_array_rgba8_on_r32f_denorm",
            "glClearTexSubImage through CUBE_MAP_ARRAY view of 2D_ARRAY GL_R32F clears selected layer-faces",
            GL_TEXTURE_2D_ARRAY, GL_R32F, 8, 8, 14, 1, GL_TEXTURE_CUBE_MAP_ARRAY, GL_RGBA8, 0, 1, 1, 12, 0, 1, 2, 5, 4,
            3, 4, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_cube_map_rgba16f_on_rg32f_denorm",
                        "glClearTexSubImage through CUBE_MAP GL_RGBA16F view of GL_RG32F clears selected cube faces",
                        GL_TEXTURE_CUBE_MAP, GL_RG32F, 8, 8, 1, 1, GL_TEXTURE_CUBE_MAP, GL_RGBA16F, 0, 1, 0, 6, 0, 0, 0,
                        1, 8, 8, 3, GL_RGBA, GL_FLOAT, fromF32({1.0f, 0.0f, 0.0f, 1.0f}), 8,
                        fromU32({0x00003c00u, 0x3c000000u})),
        makeFullViewSpec("texture_view_cube_map_to_2d_rgba8_on_r32f_denorm",
                         "glClearTexImage through single-face 2D view of CUBE_MAP GL_R32F clears only selected face",
                         GL_TEXTURE_CUBE_MAP, GL_R32F, 8, 8, 1, 2, GL_TEXTURE_2D, GL_RGBA8, 0, 2, 3, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_cube_map_to_2d_array_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 2D_ARRAY view of CUBE_MAP GL_R32F clears selected face subset",
                        GL_TEXTURE_CUBE_MAP, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D_ARRAY, GL_RGBA8, 0, 1, 1, 3, 0, 2, 2, 1,
                        4, 4, 2, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_cube_map_to_cube_map_array_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through CUBE_MAP_ARRAY view of CUBE_MAP GL_R32F clears selected faces",
                        GL_TEXTURE_CUBE_MAP, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_CUBE_MAP_ARRAY, GL_RGBA8, 0, 1, 0, 6, 0, 0,
                        0, 2, 8, 8, 2, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_cube_map_array_rgba8_on_r32f_denorm",
                         "glClearTexImage through CUBE_MAP_ARRAY GL_RGBA8 view with minlayer clears second cube",
                         GL_TEXTURE_CUBE_MAP_ARRAY, GL_R32F, 8, 8, 12, 1, GL_TEXTURE_CUBE_MAP_ARRAY, GL_RGBA8, 0, 1, 6,
                         6, GL_RGBA, GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                         fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_cube_map_array_to_cube_map_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through CUBE_MAP view of CUBE_MAP_ARRAY GL_R32F clears second cube faces",
                        GL_TEXTURE_CUBE_MAP_ARRAY, GL_R32F, 8, 8, 12, 1, GL_TEXTURE_CUBE_MAP, GL_RGBA8, 0, 1, 6, 6, 0,
                        1, 1, 1, 5, 5, 3, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_cube_map_array_to_2d_rgba8_on_r32f_denorm",
                         "glClearTexImage through single-layer 2D view of CUBE_MAP_ARRAY GL_R32F clears one face",
                         GL_TEXTURE_CUBE_MAP_ARRAY, GL_R32F, 8, 8, 12, 1, GL_TEXTURE_2D, GL_RGBA8, 0, 1, 9, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_cube_map_array_to_2d_array_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 2D_ARRAY view spanning CUBE_MAP_ARRAY boundary clears sub-box",
                        GL_TEXTURE_CUBE_MAP_ARRAY, GL_R32F, 8, 8, 12, 1, GL_TEXTURE_2D_ARRAY, GL_RGBA8, 0, 1, 2, 5, 0,
                        2, 1, 1, 4, 5, 3, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_3d_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 3D GL_RGBA8 view of GL_R32F clears 3D sub-box with denormal bits",
                        GL_TEXTURE_3D, GL_R32F, 8, 8, 6, 2, GL_TEXTURE_3D, GL_RGBA8, 0, 2, 0, 1, 0, 1, 1, 2, 4, 4, 3,
                        GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4, fromU32({0x00000001u})),
        makeSubViewSpec("texture_view_3d_rgba32ui_on_rgba32f_denorms_snan",
                        "glClearTexSubImage through 3D GL_RGBA32UI view of GL_RGBA32F clears 3D slices", GL_TEXTURE_3D,
                        GL_RGBA32F, 4, 4, 4, 1, GL_TEXTURE_3D, GL_RGBA32UI, 0, 1, 0, 1, 0, 0, 0, 1, 4, 4, 2,
                        GL_RGBA_INTEGER, GL_UNSIGNED_INT, fromU32({0x00000001u, 0x807fffffu, 0x7f800001u, 0xff800001u}),
                        16, fromU32({0x00000001u, 0x807fffffu, 0x7f800001u, 0xff800001u})),

        // Multisample 2D and 2D_ARRAY views (all 4 legal Table 8.20 MSAA pairs + sub-image) verified across all samples
        makeSubViewSpec("texture_view_2d_multisample_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 2D_MULTISAMPLE GL_RGBA8 view of GL_R32F preserves denormal bits",
                        GL_TEXTURE_2D_MULTISAMPLE, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D_MULTISAMPLE, GL_RGBA8, 0, 1, 0, 1,
                        0, 2, 2, 0, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_2d_multisample_rgba32ui_on_rgba32f_denorms_snan",
                         "glClearTexImage through 2D_MULTISAMPLE GL_RGBA32UI view of GL_RGBA32F preserves all samples",
                         GL_TEXTURE_2D_MULTISAMPLE, GL_RGBA32F, 8, 8, 1, 1, GL_TEXTURE_2D_MULTISAMPLE, GL_RGBA32UI, 0,
                         1, 0, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, false,
                         fromU32({0x00000001u, 0x807fffffu, 0x7f800001u, 0xff800001u}), 16,
                         fromU32({0x00000001u, 0x807fffffu, 0x7f800001u, 0xff800001u})),
        makeSubViewSpec("texture_view_2d_multisample_to_2d_multisample_array_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 2D_MULTISAMPLE_ARRAY view of 2D_MULTISAMPLE GL_R32F clears samples",
                        GL_TEXTURE_2D_MULTISAMPLE, GL_R32F, 8, 8, 1, 1, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_RGBA8, 0, 1,
                        0, 1, 0, 1, 2, 0, 4, 3, 1, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_2d_multisample_array_r32ui_on_r32f_snan",
                         "glClearTexImage through 2D_MULTISAMPLE_ARRAY GL_R32UI view with minlayer preserves sNaN bits",
                         GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_R32F, 8, 8, 4, 1, GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                         GL_R32UI, 0, 1, 1, 2, GL_RED_INTEGER, GL_UNSIGNED_INT, false, fromU32({0x7f800001u}), 4,
                         fromU32({0x7f800001u})),
        makeSubViewSpec("texture_view_2d_multisample_array_sub_image_rgba8_on_r32f_denorm",
                        "glClearTexSubImage through 2D_MULTISAMPLE_ARRAY GL_RGBA8 view clears selected layer sub-box",
                        GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_R32F, 8, 8, 4, 1, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_RGBA8,
                        0, 1, 1, 2, 0, 2, 2, 1, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                        fromU32({0x00000001u})),
        makeFullViewSpec("texture_view_2d_multisample_array_to_2d_multisample_rgba8_on_r32f_denorm",
                         "glClearTexImage through single-layer 2D_MULTISAMPLE view of 2D_MULTISAMPLE_ARRAY GL_R32F",
                         GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_R32F, 8, 8, 4, 1, GL_TEXTURE_2D_MULTISAMPLE, GL_RGBA8, 0,
                         1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, false, fromU8({0x01, 0x00, 0x00, 0x00}), 4,
                         fromU32({0x00000001u})),
    };
}

bool negativeViewIntegerFormatOnNormalizedView(deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunctionOptional(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunctionOptional(context);
    auto textureViewFunc      = getTextureViewFunctionOptional(context);

    glw::GLuint parentTex = 0;
    glw::GLuint viewTex   = 0;
    gl.genTextures(1, &parentTex);
    gl.bindTexture(GL_TEXTURE_2D, parentTex);
    gl.texStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 8, 8);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2D(GL_R32UI)");

    gl.genTextures(1, &viewTex);
    textureViewFunc(viewTex, GL_TEXTURE_2D, parentTex, GL_RGBA8, 0, 1, 0, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureView(GL_RGBA8 on GL_R32UI)");

    const uint8_t clearData[4] = {1, 2, 3, 4};
    clearTexImageFunc(viewTex, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, clearData);
    bool ok = checkError(context, GL_INVALID_OPERATION, "glClearTexImage(integer format on GL_RGBA8 view)");

    clearTexSubImageFunc(viewTex, 0, 0, 0, 0, 4, 4, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, clearData);
    ok &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage(integer format on GL_RGBA8 view)");

    gl.deleteTextures(1, &viewTex);
    gl.deleteTextures(1, &parentTex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return ok;
}

bool negativeViewNonIntegerFormatOnIntegerView(deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunctionOptional(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunctionOptional(context);
    auto textureViewFunc      = getTextureViewFunctionOptional(context);

    glw::GLuint parentTex = 0;
    glw::GLuint viewTex   = 0;
    gl.genTextures(1, &parentTex);
    gl.bindTexture(GL_TEXTURE_2D, parentTex);
    gl.texStorage2D(GL_TEXTURE_2D, 1, GL_R32F, 8, 8);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2D(GL_R32F)");

    gl.genTextures(1, &viewTex);
    textureViewFunc(viewTex, GL_TEXTURE_2D, parentTex, GL_R32UI, 0, 1, 0, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureView(GL_R32UI on GL_R32F)");

    const float clearData = 1.0f;
    clearTexImageFunc(viewTex, 0, GL_RED, GL_FLOAT, &clearData);
    bool ok = checkError(context, GL_INVALID_OPERATION, "glClearTexImage(float format on GL_R32UI view)");

    clearTexSubImageFunc(viewTex, 0, 0, 0, 0, 4, 4, 1, GL_RED, GL_FLOAT, &clearData);
    ok &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage(float format on GL_R32UI view)");

    gl.deleteTextures(1, &viewTex);
    gl.deleteTextures(1, &parentTex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return ok;
}

bool negativeViewLevelOutOfBounds(deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunctionOptional(context);
    auto clearTexSubImageFunc = getClearTexSubImageFunctionOptional(context);
    auto textureViewFunc      = getTextureViewFunctionOptional(context);

    glw::GLuint parentTex = 0;
    glw::GLuint viewTex   = 0;
    gl.genTextures(1, &parentTex);
    gl.bindTexture(GL_TEXTURE_2D, parentTex);
    gl.texStorage2D(GL_TEXTURE_2D, 4, GL_R32F, 16, 16);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2D(4 levels)");

    // View exposes only 1 mip level (parent level 1), even though parent has 4 levels.
    gl.genTextures(1, &viewTex);
    textureViewFunc(viewTex, GL_TEXTURE_2D, parentTex, GL_RGBA8, 1, 1, 0, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureView(minlevel=1, numlevels=1)");

    const uint8_t clearData[4] = {1, 0, 0, 0};
    clearTexImageFunc(viewTex, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearData);
    bool ok = checkError(context, GL_INVALID_OPERATION, "glClearTexImage(level=1 >= VIEW_NUM_LEVELS)");

    clearTexSubImageFunc(viewTex, 1, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearData);
    ok &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage(level=1 >= VIEW_NUM_LEVELS)");

    clearTexImageFunc(viewTex, -1, GL_RGBA, GL_UNSIGNED_BYTE, clearData);
    ok &= checkError(context, GL_INVALID_OPERATION, "glClearTexImage(level=-1)");

    gl.deleteTextures(1, &viewTex);
    gl.deleteTextures(1, &parentTex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return ok;
}

bool negativeViewLayerOutOfBounds(deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexSubImageFunc = getClearTexSubImageFunctionOptional(context);
    auto textureViewFunc      = getTextureViewFunctionOptional(context);

    glw::GLuint parentTex = 0;
    glw::GLuint viewTex   = 0;
    gl.genTextures(1, &parentTex);
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, parentTex);
    gl.texStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R32F, 8, 8, 6);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage3D(6 layers)");

    // View exposes only 2 layers (parent layers 1..2), even though parent has 6 layers.
    gl.genTextures(1, &viewTex);
    textureViewFunc(viewTex, GL_TEXTURE_2D_ARRAY, parentTex, GL_RGBA8, 0, 1, 1, 2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureView(minlayer=1, numlayers=2)");

    const uint8_t clearData[4] = {1, 0, 0, 0};
    clearTexSubImageFunc(viewTex, 0, 0, 0, 2, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearData);
    bool ok = checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage(zoffset=2 >= VIEW_NUM_LAYERS)");

    clearTexSubImageFunc(viewTex, 0, 0, 0, 1, 4, 4, 2, GL_RGBA, GL_UNSIGNED_BYTE, clearData);
    ok &= checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage(zoffset+depth=3 > VIEW_NUM_LAYERS)");

    gl.deleteTextures(1, &viewTex);
    gl.deleteTextures(1, &parentTex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return ok;
}

bool negativeView2DOn2DArrayZOffsetOutOfBounds(deqp::Context &context)
{
    const auto &gl            = context.getRenderContext().getFunctions();
    auto clearTexSubImageFunc = getClearTexSubImageFunctionOptional(context);
    auto textureViewFunc      = getTextureViewFunctionOptional(context);

    glw::GLuint parentTex = 0;
    glw::GLuint viewTex   = 0;
    gl.genTextures(1, &parentTex);
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, parentTex);
    gl.texStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R32F, 8, 8, 4);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage3D(4 layers)");

    // Single-layer GL_TEXTURE_2D view of 4-layer GL_TEXTURE_2D_ARRAY has effective depth 1.
    gl.genTextures(1, &viewTex);
    textureViewFunc(viewTex, GL_TEXTURE_2D, parentTex, GL_RGBA8, 0, 1, 1, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureView(GL_TEXTURE_2D on GL_TEXTURE_2D_ARRAY)");

    const uint8_t clearData[4] = {1, 0, 0, 0};
    clearTexSubImageFunc(viewTex, 0, 0, 0, 1, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearData);
    bool ok = checkError(context, GL_INVALID_OPERATION, "glClearTexSubImage(zoffset=1 on GL_TEXTURE_2D view)");

    gl.deleteTextures(1, &viewTex);
    gl.deleteTextures(1, &parentTex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
    return ok;
}

} // namespace

ClearTexImageViewTest::ClearTexImageViewTest(deqp::Context &context, const ViewClearSpec &spec)
    : TestCase(context, spec.name, spec.description)
    , m_spec(spec)
{
}

tcu::TestNode::IterateResult ClearTexImageViewTest::iterate(void)
{
    const auto apiType        = m_context.getRenderContext().getType();
    const auto &ctxInfo       = m_context.getContextInfo();
    const bool isAtLeastGl44  = glu::contextSupports(apiType, glu::ApiType::core(4, 4));
    const bool isAtLeastGl43  = glu::contextSupports(apiType, glu::ApiType::core(4, 3));
    const bool hasClearTex    = isAtLeastGl44 || ctxInfo.isExtensionSupported("GL_ARB_clear_texture");
    const bool hasTextureView = isAtLeastGl43 || ctxInfo.isExtensionSupported("GL_ARB_texture_view");

    if (!hasClearTex || !hasTextureView)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    if (m_spec.parentTarget == GL_TEXTURE_CUBE_MAP_ARRAY || m_spec.viewTarget == GL_TEXTURE_CUBE_MAP_ARRAY ||
        (m_spec.useMidView && m_spec.midTarget == GL_TEXTURE_CUBE_MAP_ARRAY))
    {
        const bool isAtLeastGl40   = glu::contextSupports(apiType, glu::ApiType::core(4, 0));
        const bool hasCubeMapArray = isAtLeastGl40 || ctxInfo.isExtensionSupported("GL_ARB_texture_cube_map_array");
        if (!hasCubeMapArray)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "GL_ARB_texture_cube_map_array is not supported");
            return STOP;
        }
    }

    const bool isMultisample =
        (m_spec.parentTarget == GL_TEXTURE_2D_MULTISAMPLE || m_spec.parentTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
    if (isMultisample)
    {
        const bool hasTexStorageMs =
            isAtLeastGl43 || ctxInfo.isExtensionSupported("GL_ARB_texture_storage_multisample");
        const bool hasCompute = isAtLeastGl43 || ctxInfo.isExtensionSupported("GL_ARB_compute_shader");
        const bool hasSsbo    = isAtLeastGl43 || ctxInfo.isExtensionSupported("GL_ARB_shader_storage_buffer_object");
        if (!(hasTexStorageMs && hasCompute && hasSsbo))
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Multisample compute readback is not supported");
            return STOP;
        }
    }

    if (getClearTexImageFunctionOptional(m_context) == nullptr ||
        getClearTexSubImageFunctionOptional(m_context) == nullptr ||
        getTextureViewFunctionOptional(m_context) == nullptr)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Required entry points are not available");
        return STOP;
    }

    try
    {
        if (runTest())
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }
    catch (const tcu::NotSupportedError &e)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, e.what());
    }
    catch (const tcu::TestError &e)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Test error: " << e.what() << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }
    catch (...)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, "Error");
    }

    return STOP;
}

bool ClearTexImageViewTest::runTest(void)
{
    const auto &gl            = m_context.getRenderContext().getFunctions();
    auto clearTexImageFunc    = getClearTexImageFunctionOptional(m_context);
    auto clearTexSubImageFunc = getClearTexSubImageFunctionOptional(m_context);
    auto textureViewFunc      = getTextureViewFunctionOptional(m_context);
    tcu::TestLog &log         = m_testCtx.getLog();

    glw::GLenum intInternalFormat = 0;
    glw::GLenum intFormat         = 0;
    glw::GLenum intType           = 0;
    getHelperIntegerFormat(m_spec.texelSizeBytes, intInternalFormat, intFormat, intType);

    glw::GLint prevUnpackAlign = 4;
    glw::GLint prevPackAlign   = 4;
    gl.getIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpackAlign);
    gl.getIntegerv(GL_PACK_ALIGNMENT, &prevPackAlign);

    struct GlCleanup
    {
        const glw::Functions &gl;
        glw::GLint prevUnpackAlign;
        glw::GLint prevPackAlign;
        glw::GLuint parentTex     = 0;
        glw::GLuint intHelperView = 0;
        glw::GLuint midView       = 0;
        glw::GLuint clearView     = 0;
        glw::GLuint fbo           = 0;
        glw::GLuint ssbo          = 0;
        glw::GLuint cs            = 0;
        glw::GLuint prog          = 0;

        ~GlCleanup()
        {
            gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
            gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            gl.useProgram(0);
            gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            gl.pixelStorei(GL_UNPACK_ALIGNMENT, prevUnpackAlign);
            gl.pixelStorei(GL_PACK_ALIGNMENT, prevPackAlign);
            if (prog != 0)
                gl.deleteProgram(prog);
            if (cs != 0)
                gl.deleteShader(cs);
            if (ssbo != 0)
                gl.deleteBuffers(1, &ssbo);
            if (fbo != 0)
                gl.deleteFramebuffers(1, &fbo);
            if (clearView != 0)
                gl.deleteTextures(1, &clearView);
            if (midView != 0)
                gl.deleteTextures(1, &midView);
            if (intHelperView != 0)
                gl.deleteTextures(1, &intHelperView);
            if (parentTex != 0)
                gl.deleteTextures(1, &parentTex);
        }
    } cleanup{gl, prevUnpackAlign, prevPackAlign};

    gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.pixelStorei(GL_PACK_ALIGNMENT, 1);

    const bool isMultisample =
        (m_spec.parentTarget == GL_TEXTURE_2D_MULTISAMPLE || m_spec.parentTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY);

    glw::GLsizei samples = 1;
    if (isMultisample)
    {
        glw::GLint maxColorSamples = 1;
        glw::GLint maxIntSamples   = 1;
        gl.getIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxColorSamples);
        gl.getIntegerv(GL_MAX_INTEGER_SAMPLES, &maxIntSamples);
        samples = std::min<glw::GLsizei>(4, std::min(maxColorSamples, maxIntSamples));
        if (samples == 3)
            samples = 2;
        if (samples < 1)
            samples = 1;
    }

    gl.genTextures(1, &cleanup.parentTex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(parentTex)");
    gl.bindTexture(m_spec.parentTarget, cleanup.parentTex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture(parentTex)");

    glw::GLenum helperTarget    = GL_TEXTURE_2D_ARRAY;
    glw::GLuint helperNumLayers = 1;
    switch (m_spec.parentTarget)
    {
    case GL_TEXTURE_1D:
        gl.texStorage1D(GL_TEXTURE_1D, m_spec.parentLevels, m_spec.parentInternalFormat, m_spec.parentWidth);
        helperTarget    = GL_TEXTURE_1D_ARRAY;
        helperNumLayers = 1;
        break;
    case GL_TEXTURE_1D_ARRAY:
        gl.texStorage2D(GL_TEXTURE_1D_ARRAY, m_spec.parentLevels, m_spec.parentInternalFormat, m_spec.parentWidth,
                        m_spec.parentHeight);
        helperTarget    = GL_TEXTURE_1D_ARRAY;
        helperNumLayers = static_cast<glw::GLuint>(m_spec.parentHeight);
        break;
    case GL_TEXTURE_2D:
        gl.texStorage2D(GL_TEXTURE_2D, m_spec.parentLevels, m_spec.parentInternalFormat, m_spec.parentWidth,
                        m_spec.parentHeight);
        helperTarget    = GL_TEXTURE_2D_ARRAY;
        helperNumLayers = 1;
        break;
    case GL_TEXTURE_RECTANGLE:
        gl.texStorage2D(GL_TEXTURE_RECTANGLE, m_spec.parentLevels, m_spec.parentInternalFormat, m_spec.parentWidth,
                        m_spec.parentHeight);
        helperTarget    = GL_TEXTURE_RECTANGLE;
        helperNumLayers = 1;
        break;
    case GL_TEXTURE_2D_ARRAY:
        gl.texStorage3D(GL_TEXTURE_2D_ARRAY, m_spec.parentLevels, m_spec.parentInternalFormat, m_spec.parentWidth,
                        m_spec.parentHeight, m_spec.parentDepth);
        helperTarget    = GL_TEXTURE_2D_ARRAY;
        helperNumLayers = static_cast<glw::GLuint>(m_spec.parentDepth);
        break;
    case GL_TEXTURE_CUBE_MAP:
        gl.texStorage2D(GL_TEXTURE_CUBE_MAP, m_spec.parentLevels, m_spec.parentInternalFormat, m_spec.parentWidth,
                        m_spec.parentHeight);
        helperTarget    = GL_TEXTURE_2D_ARRAY;
        helperNumLayers = 6;
        break;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        gl.texStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, m_spec.parentLevels, m_spec.parentInternalFormat, m_spec.parentWidth,
                        m_spec.parentHeight, m_spec.parentDepth);
        helperTarget    = GL_TEXTURE_2D_ARRAY;
        helperNumLayers = static_cast<glw::GLuint>(m_spec.parentDepth);
        break;
    case GL_TEXTURE_3D:
        gl.texStorage3D(GL_TEXTURE_3D, m_spec.parentLevels, m_spec.parentInternalFormat, m_spec.parentWidth,
                        m_spec.parentHeight, m_spec.parentDepth);
        helperTarget    = GL_TEXTURE_3D;
        helperNumLayers = 1;
        break;
    case GL_TEXTURE_2D_MULTISAMPLE:
        gl.texStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, m_spec.parentInternalFormat, m_spec.parentWidth,
                                   m_spec.parentHeight, GL_TRUE);
        helperTarget    = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        helperNumLayers = 1;
        break;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        gl.texStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples, m_spec.parentInternalFormat,
                                   m_spec.parentWidth, m_spec.parentHeight, m_spec.parentDepth, GL_TRUE);
        helperTarget    = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        helperNumLayers = static_cast<glw::GLuint>(m_spec.parentDepth);
        break;
    default:
        TCU_THROW(InternalError, "Unsupported parent texture target");
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage*(parentTex)");

    // Create integer helper view covering the entire parent resource for raw bit initialization and readback.
    gl.genTextures(1, &cleanup.intHelperView);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(intHelperView)");
    textureViewFunc(cleanup.intHelperView, helperTarget, cleanup.parentTex, intInternalFormat, 0,
                    static_cast<glw::GLuint>(m_spec.parentLevels), 0, helperNumLayers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureView(intHelperView)");

    glw::GLuint viewOrigTex = cleanup.parentTex;
    if (m_spec.useMidView)
    {
        gl.genTextures(1, &cleanup.midView);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(midView)");
        textureViewFunc(cleanup.midView, m_spec.midTarget, cleanup.parentTex, m_spec.midInternalFormat,
                        m_spec.midMinLevel, m_spec.midNumLevels, m_spec.midMinLayer, m_spec.midNumLayers);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureView(midView)");
        viewOrigTex = cleanup.midView;
    }

    // Create the format-cast texture view under test.
    gl.genTextures(1, &cleanup.clearView);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures(clearView)");
    textureViewFunc(cleanup.clearView, m_spec.viewTarget, viewOrigTex, m_spec.viewInternalFormat, m_spec.viewMinLevel,
                    m_spec.viewNumLevels, m_spec.viewMinLayer, m_spec.viewNumLayers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glTextureView(clearView)");

    uint8_t sentinelTexel[16];
    std::memset(sentinelTexel, 0x5a, sizeof(sentinelTexel));

    auto getMipDimensions = [&](glw::GLint mip, glw::GLsizei &w, glw::GLsizei &h, glw::GLsizei &d)
    {
        w = std::max<glw::GLsizei>(1, m_spec.parentWidth >> mip);
        if (helperTarget == GL_TEXTURE_1D_ARRAY)
        {
            h = static_cast<glw::GLsizei>(helperNumLayers);
            d = 1;
        }
        else if (helperTarget == GL_TEXTURE_RECTANGLE)
        {
            h = m_spec.parentHeight;
            d = 1;
        }
        else if (helperTarget == GL_TEXTURE_2D_ARRAY || helperTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
            h = std::max<glw::GLsizei>(1, m_spec.parentHeight >> mip);
            d = static_cast<glw::GLsizei>(helperNumLayers);
        }
        else
        {
            h = std::max<glw::GLsizei>(1, m_spec.parentHeight >> mip);
            d = std::max<glw::GLsizei>(1, m_spec.parentDepth >> mip);
        }
    };

    auto uploadHelperMip = [&](glw::GLint mip, const uint8_t *patternTexel)
    {
        glw::GLsizei w = 0, h = 0, d = 0;
        getMipDimensions(mip, w, h, d);
        const size_t texelCount = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(d);
        std::vector<uint8_t> buf(texelCount * static_cast<size_t>(m_spec.texelSizeBytes));
        for (size_t i = 0; i < texelCount; ++i)
            std::memcpy(buf.data() + i * static_cast<size_t>(m_spec.texelSizeBytes), patternTexel,
                        static_cast<size_t>(m_spec.texelSizeBytes));

        gl.bindTexture(helperTarget, cleanup.intHelperView);
        if (helperTarget == GL_TEXTURE_1D_ARRAY || helperTarget == GL_TEXTURE_RECTANGLE)
            gl.texSubImage2D(helperTarget, mip, 0, 0, w, h, intFormat, intType, buf.data());
        else
            gl.texSubImage3D(helperTarget, mip, 0, 0, 0, w, h, d, intFormat, intType, buf.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glTexSubImage*(intHelperView)");
    };

    auto readbackHelperMip = [&](glw::GLint mip, std::vector<uint8_t> &outBuf)
    {
        glw::GLsizei w = 0, h = 0, d = 0;
        getMipDimensions(mip, w, h, d);
        const size_t texelCount = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(d);
        outBuf.assign(texelCount * static_cast<size_t>(m_spec.texelSizeBytes), 0u);

        gl.bindTexture(helperTarget, cleanup.intHelperView);
        gl.getTexImage(helperTarget, mip, intFormat, intType, outBuf.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexImage(intHelperView)");
    };

    auto readbackMultisampleViaCompute = [&](std::vector<uint8_t> &outBuf) -> bool
    {
        if (cleanup.prog == 0)
        {
            const char *csSource = "#version 430 core\n"
                                   "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
                                   "uniform usampler2DMSArray u_tex;\n"
                                   "uniform int u_samples;\n"
                                   "uniform int u_uwords;\n"
                                   "layout(std430, binding = 0) buffer OutBuf {\n"
                                   "    uint data[];\n"
                                   "};\n"
                                   "void main() {\n"
                                   "    ivec3 sz = textureSize(u_tex);\n"
                                   "    int x = int(gl_GlobalInvocationID.x);\n"
                                   "    int y = int(gl_GlobalInvocationID.y);\n"
                                   "    int z = int(gl_GlobalInvocationID.z);\n"
                                   "    int texelIdx = (z * sz.y + y) * sz.x + x;\n"
                                   "    for (int s = 0; s < u_samples; ++s) {\n"
                                   "        uvec4 v = texelFetch(u_tex, ivec3(x, y, z), s);\n"
                                   "        int base = (texelIdx * u_samples + s) * u_uwords;\n"
                                   "        data[base + 0] = v.x;\n"
                                   "        if (u_uwords > 1) data[base + 1] = v.y;\n"
                                   "        if (u_uwords > 2) data[base + 2] = v.z;\n"
                                   "        if (u_uwords > 3) data[base + 3] = v.w;\n"
                                   "    }\n"
                                   "}\n";

            cleanup.cs = gl.createShader(GL_COMPUTE_SHADER);
            gl.shaderSource(cleanup.cs, 1, &csSource, nullptr);
            gl.compileShader(cleanup.cs);
            glw::GLint compiled = GL_FALSE;
            gl.getShaderiv(cleanup.cs, GL_COMPILE_STATUS, &compiled);
            if (compiled != GL_TRUE)
            {
                log << tcu::TestLog::Message << "Failed to compile MSAA readback compute shader"
                    << tcu::TestLog::EndMessage;
                return false;
            }

            cleanup.prog = gl.createProgram();
            gl.attachShader(cleanup.prog, cleanup.cs);
            gl.linkProgram(cleanup.prog);
            glw::GLint linked = GL_FALSE;
            gl.getProgramiv(cleanup.prog, GL_LINK_STATUS, &linked);
            if (linked != GL_TRUE)
            {
                log << tcu::TestLog::Message << "Failed to link MSAA readback compute program"
                    << tcu::TestLog::EndMessage;
                return false;
            }

            gl.genBuffers(1, &cleanup.ssbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers(ssbo)");
        }

        const size_t totalSamples = static_cast<size_t>(m_spec.parentWidth) * static_cast<size_t>(m_spec.parentHeight) *
                                    static_cast<size_t>(helperNumLayers) * static_cast<size_t>(samples);
        const size_t totalBytes = totalSamples * static_cast<size_t>(m_spec.texelSizeBytes);
        outBuf.assign(totalBytes, 0u);

        gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, cleanup.ssbo);
        gl.bufferData(GL_SHADER_STORAGE_BUFFER, static_cast<glw::GLsizeiptr>(totalBytes), outBuf.data(),
                      GL_DYNAMIC_READ);
        gl.bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cleanup.ssbo);

        gl.useProgram(cleanup.prog);
        gl.activeTexture(GL_TEXTURE0);
        gl.bindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, cleanup.intHelperView);
        gl.uniform1i(gl.getUniformLocation(cleanup.prog, "u_tex"), 0);
        gl.uniform1i(gl.getUniformLocation(cleanup.prog, "u_samples"), samples);
        gl.uniform1i(gl.getUniformLocation(cleanup.prog, "u_uwords"), m_spec.texelSizeBytes / 4);

        gl.dispatchCompute(static_cast<glw::GLuint>(m_spec.parentWidth), static_cast<glw::GLuint>(m_spec.parentHeight),
                           helperNumLayers);
        gl.memoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, cleanup.ssbo);
        gl.getBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<glw::GLsizeiptr>(totalBytes), outBuf.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "MSAA compute readback");
        return true;
    };

    const glw::GLuint effectiveViewMinLevel = (m_spec.useMidView ? m_spec.midMinLevel : 0u) + m_spec.viewMinLevel;
    const glw::GLuint effectiveViewMinLayer = (m_spec.useMidView ? m_spec.midMinLayer : 0u) + m_spec.viewMinLayer;
    const glw::GLint targetParentLevel      = static_cast<glw::GLint>(effectiveViewMinLevel) + m_spec.clearLevel;

    if (!isMultisample)
    {
        // Control check: verify integer upload + integer readback preserves the target bit pattern on this storage.
        uploadHelperMip(targetParentLevel, m_spec.expectedTexelBytes);
        std::vector<uint8_t> controlBuf;
        readbackHelperMip(targetParentLevel, controlBuf);
        if (controlBuf.size() < static_cast<size_t>(m_spec.texelSizeBytes) ||
            !bytesEqual(controlBuf.data(), m_spec.expectedTexelBytes, m_spec.texelSizeBytes))
        {
            log << tcu::TestLog::Message << "Integer view upload/readback control failed: expected ["
                << formatBytesHex(m_spec.expectedTexelBytes, m_spec.texelSizeBytes) << "], got ["
                << formatBytesHex(controlBuf.data(), m_spec.texelSizeBytes) << "]" << tcu::TestLog::EndMessage;
            return false;
        }

        // Fill every mip level of the parent texture with sentinel 0x5a bytes before clearing clearView.
        for (glw::GLint mip = 0; mip < m_spec.parentLevels; ++mip)
            uploadHelperMip(mip, sentinelTexel);
    }
    else
    {
        // Initialize all layers and samples of intHelperView via layered FBO integer clear.
        gl.genFramebuffers(1, &cleanup.fbo);
        gl.bindFramebuffer(GL_FRAMEBUFFER, cleanup.fbo);
        gl.framebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cleanup.intHelperView, 0);
        const glw::GLenum drawBuf = GL_COLOR_ATTACHMENT0;
        gl.drawBuffers(1, &drawBuf);
        if (gl.checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            log << tcu::TestLog::Message << "Integer helper multisample framebuffer is incomplete"
                << tcu::TestLog::EndMessage;
            return false;
        }
        gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gl.disable(GL_SCISSOR_TEST);
        const glw::GLuint sentinelVec[4] = {0x5a5a5a5au, 0x5a5a5a5au, 0x5a5a5a5au, 0x5a5a5a5au};
        gl.clearBufferuiv(GL_COLOR, 0, sentinelVec);
        gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glClearBufferuiv(MSAA sentinel)");

        std::vector<uint8_t> sentinelCheck;
        if (!readbackMultisampleViaCompute(sentinelCheck))
            return false;
        if (sentinelCheck.size() < static_cast<size_t>(m_spec.texelSizeBytes) ||
            !bytesEqual(sentinelCheck.data(), sentinelTexel, m_spec.texelSizeBytes))
        {
            log << tcu::TestLog::Message << "MSAA sentinel initialization check failed" << tcu::TestLog::EndMessage;
            return false;
        }
    }

    // Disabled color write masks must not affect glClearTexImage / glClearTexSubImage.
    gl.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    const void *clearPtr = m_spec.nullClearData ? nullptr : static_cast<const void *>(m_spec.clearDataBytes);
    if (m_spec.useSubImage)
    {
        clearTexSubImageFunc(cleanup.clearView, m_spec.clearLevel, m_spec.xoffset, m_spec.yoffset, m_spec.zoffset,
                             m_spec.clearWidth, m_spec.clearHeight, m_spec.clearDepth, m_spec.clearFormat,
                             m_spec.clearType, clearPtr);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glClearTexSubImage(clearView)");
    }
    else
    {
        clearTexImageFunc(cleanup.clearView, m_spec.clearLevel, m_spec.clearFormat, m_spec.clearType, clearPtr);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glClearTexImage(clearView)");
    }

    gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    auto isParentCoordCleared = [&](glw::GLint mip, glw::GLsizei x, glw::GLsizei y, glw::GLsizei z) -> bool
    {
        if (mip != targetParentLevel)
            return false;

        if (m_spec.useSubImage && (m_spec.clearWidth == 0 || m_spec.clearHeight == 0 || m_spec.clearDepth == 0))
            return false;

        if (helperTarget == GL_TEXTURE_1D_ARRAY)
        {
            const glw::GLuint layer = static_cast<glw::GLuint>(y);
            if (!m_spec.useSubImage)
                return layer >= effectiveViewMinLayer && layer < (effectiveViewMinLayer + m_spec.viewNumLayers);

            const glw::GLuint subStartLayer = effectiveViewMinLayer + static_cast<glw::GLuint>(m_spec.yoffset);
            const glw::GLuint subEndLayer   = subStartLayer + static_cast<glw::GLuint>(m_spec.clearHeight);
            return x >= m_spec.xoffset && x < (m_spec.xoffset + m_spec.clearWidth) && layer >= subStartLayer &&
                   layer < subEndLayer;
        }

        if (helperTarget == GL_TEXTURE_2D_ARRAY || helperTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
            const glw::GLuint layer = static_cast<glw::GLuint>(z);
            if (!m_spec.useSubImage)
                return layer >= effectiveViewMinLayer && layer < (effectiveViewMinLayer + m_spec.viewNumLayers);

            const glw::GLuint subStartLayer = effectiveViewMinLayer + static_cast<glw::GLuint>(m_spec.zoffset);
            const glw::GLuint subEndLayer   = subStartLayer + static_cast<glw::GLuint>(m_spec.clearDepth);
            return x >= m_spec.xoffset && x < (m_spec.xoffset + m_spec.clearWidth) && y >= m_spec.yoffset &&
                   y < (m_spec.yoffset + m_spec.clearHeight) && layer >= subStartLayer && layer < subEndLayer;
        }

        // GL_TEXTURE_3D or GL_TEXTURE_RECTANGLE
        if (!m_spec.useSubImage)
            return true;
        return x >= m_spec.xoffset && x < (m_spec.xoffset + m_spec.clearWidth) && y >= m_spec.yoffset &&
               y < (m_spec.yoffset + m_spec.clearHeight) && z >= m_spec.zoffset &&
               z < (m_spec.zoffset + m_spec.clearDepth);
    };

    size_t mismatchCount = 0;
    if (!isMultisample)
    {
        for (glw::GLint mip = 0; mip < m_spec.parentLevels; ++mip)
        {
            glw::GLsizei w = 0, h = 0, d = 0;
            getMipDimensions(mip, w, h, d);
            std::vector<uint8_t> actual;
            readbackHelperMip(mip, actual);

            for (glw::GLsizei z = 0; z < d; ++z)
            {
                for (glw::GLsizei y = 0; y < h; ++y)
                {
                    for (glw::GLsizei x = 0; x < w; ++x)
                    {
                        const size_t texelIdx =
                            (static_cast<size_t>(z) * static_cast<size_t>(h) + static_cast<size_t>(y)) *
                                static_cast<size_t>(w) +
                            static_cast<size_t>(x);
                        const uint8_t *got = actual.data() + texelIdx * static_cast<size_t>(m_spec.texelSizeBytes);
                        const bool cleared = isParentCoordCleared(mip, x, y, z);
                        const uint8_t *exp = cleared ? m_spec.expectedTexelBytes : sentinelTexel;
                        if (!bytesEqual(got, exp, m_spec.texelSizeBytes))
                        {
                            if (mismatchCount == 0)
                            {
                                log << tcu::TestLog::Message << "First mismatch at mip=" << mip << " (" << x << ", "
                                    << y << ", " << z << ") [" << (cleared ? "cleared" : "uncleared") << "]: expected ["
                                    << formatBytesHex(exp, m_spec.texelSizeBytes) << "], got ["
                                    << formatBytesHex(got, m_spec.texelSizeBytes) << "]" << tcu::TestLog::EndMessage;
                            }
                            ++mismatchCount;
                        }
                    }
                }
            }
        }
    }
    else
    {
        std::vector<uint8_t> actual;
        if (!readbackMultisampleViaCompute(actual))
            return false;

        const glw::GLsizei w = m_spec.parentWidth;
        const glw::GLsizei h = m_spec.parentHeight;
        const glw::GLsizei d = static_cast<glw::GLsizei>(helperNumLayers);
        for (glw::GLsizei z = 0; z < d; ++z)
        {
            for (glw::GLsizei y = 0; y < h; ++y)
            {
                for (glw::GLsizei x = 0; x < w; ++x)
                {
                    const bool cleared    = isParentCoordCleared(0, x, y, z);
                    const uint8_t *exp    = cleared ? m_spec.expectedTexelBytes : sentinelTexel;
                    const size_t texelIdx = (static_cast<size_t>(z) * static_cast<size_t>(h) + static_cast<size_t>(y)) *
                                                static_cast<size_t>(w) +
                                            static_cast<size_t>(x);
                    for (glw::GLsizei s = 0; s < samples; ++s)
                    {
                        const size_t sampleIdx = texelIdx * static_cast<size_t>(samples) + static_cast<size_t>(s);
                        const uint8_t *got     = actual.data() + sampleIdx * static_cast<size_t>(m_spec.texelSizeBytes);
                        if (!bytesEqual(got, exp, m_spec.texelSizeBytes))
                        {
                            if (mismatchCount == 0)
                            {
                                log << tcu::TestLog::Message << "First MSAA mismatch at (" << x << ", " << y << ", "
                                    << z << ") sample=" << s << " [" << (cleared ? "cleared" : "uncleared")
                                    << "]: expected [" << formatBytesHex(exp, m_spec.texelSizeBytes) << "], got ["
                                    << formatBytesHex(got, m_spec.texelSizeBytes) << "]" << tcu::TestLog::EndMessage;
                            }
                            ++mismatchCount;
                        }
                    }
                }
            }
        }
    }

    if (mismatchCount > 0)
    {
        log << tcu::TestLog::Message << "Total mismatched texels/samples: " << mismatchCount
            << tcu::TestLog::EndMessage;
        return false;
    }

    return true;
}

ClearTexImageViewNegativeTest::ClearTexImageViewNegativeTest(deqp::Context &context, const char *testName,
                                                             const char *testDescription,
                                                             std::function<bool(deqp::Context &)> testFunc)
    : TestCase(context, testName, testDescription)
    , m_testFunc(std::move(testFunc))
{
}

tcu::TestNode::IterateResult ClearTexImageViewNegativeTest::iterate(void)
{
    const auto apiType        = m_context.getRenderContext().getType();
    const auto &ctxInfo       = m_context.getContextInfo();
    const bool isAtLeastGl44  = glu::contextSupports(apiType, glu::ApiType::core(4, 4));
    const bool isAtLeastGl43  = glu::contextSupports(apiType, glu::ApiType::core(4, 3));
    const bool hasClearTex    = isAtLeastGl44 || ctxInfo.isExtensionSupported("GL_ARB_clear_texture");
    const bool hasTextureView = isAtLeastGl43 || ctxInfo.isExtensionSupported("GL_ARB_texture_view");

    if (!hasClearTex || !hasTextureView || getClearTexImageFunctionOptional(m_context) == nullptr ||
        getClearTexSubImageFunctionOptional(m_context) == nullptr ||
        getTextureViewFunctionOptional(m_context) == nullptr)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
        return STOP;
    }

    try
    {
        if (m_testFunc(m_context))
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }
    catch (const tcu::NotSupportedError &e)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, e.what());
    }
    catch (const tcu::TestError &e)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Test error: " << e.what() << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }
    catch (...)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, "Error");
    }

    return STOP;
}

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

    {
        const auto specs = getClearTexImageViewSpecs();
        for (const auto &spec : specs)
        {
            addChild(new ClearTexImageViewTest(m_context, spec));
        }

        addChild(new ClearTexImageViewNegativeTest(
            m_context, "negative_texture_view_integer_format_on_normalized_view_of_integer_parent",
            "glClearTex[Sub]Image with integer format on normalized GL_RGBA8 view of GL_R32UI returns "
            "GL_INVALID_OPERATION",
            negativeViewIntegerFormatOnNormalizedView));
        addChild(new ClearTexImageViewNegativeTest(
            m_context, "negative_texture_view_non_integer_format_on_integer_view_of_float_parent",
            "glClearTex[Sub]Image with float format on integer GL_R32UI view of GL_R32F returns GL_INVALID_OPERATION",
            negativeViewNonIntegerFormatOnIntegerView));
        addChild(new ClearTexImageViewNegativeTest(
            m_context, "negative_texture_view_level_out_of_view_bounds",
            "glClearTex[Sub]Image with level outside VIEW_NUM_LEVELS returns GL_INVALID_OPERATION",
            negativeViewLevelOutOfBounds));
        addChild(new ClearTexImageViewNegativeTest(
            m_context, "negative_texture_view_layer_out_of_view_bounds",
            "glClearTexSubImage with layer range outside VIEW_NUM_LAYERS returns GL_INVALID_OPERATION",
            negativeViewLayerOutOfBounds));
        addChild(new ClearTexImageViewNegativeTest(
            m_context, "negative_texture_view_2d_on_2d_array_zoffset_out_of_bounds",
            "glClearTexSubImage with zoffset >= 1 on single-layer 2D view of 2D_ARRAY returns GL_INVALID_OPERATION",
            negativeView2DOn2DArrayZOffsetOutOfBounds));
    }
}

} // namespace gl4cts
