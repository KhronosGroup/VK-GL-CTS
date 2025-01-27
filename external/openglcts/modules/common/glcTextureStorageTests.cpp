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
 * \file  glcTextureStorageCompressedDataTests.cpp
 * \brief Conformance tests for the textureStorage functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "glcTextureStorageTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"

#include <cmath>

using namespace glw;
using namespace glu;

namespace glcts
{
/** Constructor.
 *
 *  @param context     Rendering context
 */
TextureStorageCompressedDataTestCase::TextureStorageCompressedDataTestCase(deqp::Context &context)
    : TestCase(context, "compressed_data", "Verifies compressed texture data loading functionality")
    , m_isContextES(false)
    , m_testSupported(false)
    , m_texture2D(0)
    , m_textureCubeMap(0)
    , m_texture3D(0)
    , m_texture2DArray(0)
    , m_textureSize2D(512)
    , m_textureSize3D(64)
    , m_maxTexturePixels(0)
{
}

/** Stub deinit method. */
void TextureStorageCompressedDataTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void TextureStorageCompressedDataTestCase::init()
{
    const glu::RenderContext &renderContext = m_context.getRenderContext();
    m_isContextES                           = glu::isContextTypeES(renderContext.getType());

    m_textureLevels2D = (int)std::floor(std::log2f((float)m_textureSize2D)) + 1;
    m_textureLevels3D = (int)std::floor(std::log2f((float)m_textureSize3D)) + 1;

    /* create largest used 2D/3D texture */
    m_maxTexturePixels = std::max(4 * m_textureSize2D * m_textureSize2D,
                                  4 * m_textureSize3D * m_textureSize3D * m_textureSize3D); /* RGBA, thus 4x */
    m_texData = std::vector<GLfloat>(4 * m_maxTexturePixels, 0.f); /* f32 or (u)i32, which are 4 bytes per pixel */

    auto contextType = m_context.getRenderContext().getType();
    if (!m_isContextES)
    {
        m_testSupported = (m_context.getContextInfo().isExtensionSupported("GL_EXT_texture_storage") &&
                           (glu::contextSupports(contextType, glu::ApiType::core(3, 0)) ||
                            glu::contextSupports(contextType, glu::ApiType::core(3, 1)))) ||
                          glu::contextSupports(contextType, glu::ApiType::core(4, 2));
    }
    else
        m_testSupported = true;
}

bool TextureStorageCompressedDataTestCase::iterate_gl()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool ret                 = true;
    int curTextureSize       = 0;

    struct formats
    {
        GLenum intFormat;
        GLenum format;
        GLenum type;
        GLboolean allowedWithTex3D;
    } formats[] = {{GL_COMPRESSED_RG_RGTC2, GL_RG, GL_UNSIGNED_BYTE, GL_FALSE},
                   {GL_COMPRESSED_SIGNED_RG_RGTC2, GL_RG, GL_UNSIGNED_BYTE, GL_FALSE},
                   {GL_R16_SNORM, GL_RED, GL_UNSIGNED_BYTE, GL_TRUE},
                   {GL_R8_SNORM, GL_RED, GL_UNSIGNED_BYTE, GL_TRUE},
                   {GL_COMPRESSED_RED_RGTC1, GL_RED, GL_UNSIGNED_BYTE, GL_FALSE},
                   {GL_COMPRESSED_SIGNED_RED_RGTC1, GL_RED, GL_UNSIGNED_BYTE, GL_FALSE}};

    GLenum format[] = {GL_RED, GL_RG, GL_RGB, GL_RGBA};

    GLenum cubeMapTarget[] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                              GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                              GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

    /* test with TEXTURE_2D target */
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "testing TEXTURE_2D compressed texture loading with each internal format\n"
                       << tcu::TestLog::EndMessage;

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        gl.genTextures(1, &m_texture2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

        gl.bindTexture(GL_TEXTURE_2D, m_texture2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

        gl.texStorage2D(GL_TEXTURE_2D, m_textureLevels2D, formats[i].intFormat, m_textureSize2D, m_textureSize2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2D");

        /* test each format and level*/
        for (size_t j = 0; j < sizeof(format) / sizeof(format[0]); j++)
        {
            curTextureSize = m_textureSize2D;

            for (size_t k = 0; k < (size_t)m_textureLevels2D; k++)
            {
                gl.texSubImage2D(GL_TEXTURE_2D, k, 0, 0, curTextureSize, curTextureSize, format[j], formats[i].type,
                                 m_texData.data());
                GLU_EXPECT_NO_ERROR(gl.getError(), "texSubImage2D");

                curTextureSize /= 2;
            }
        }
        gl.deleteTextures(1, &m_texture2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    /* test with TEXTURE_CUBE_MAP target */
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "testing TEXTURE_CUBE_MAP compressed texture loading with each internal format\n"
                       << tcu::TestLog::EndMessage;

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        gl.genTextures(1, &m_textureCubeMap);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

        gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_textureCubeMap);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

        gl.texStorage2D(GL_TEXTURE_CUBE_MAP, m_textureLevels2D, formats[i].intFormat, m_textureSize2D, m_textureSize2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2D");

        /* test each format, cubemap face and level */
        for (size_t j = 0; j < sizeof(format) / sizeof(format[0]); j++)
        {
            for (size_t k = 0; k < 6; k++)
            {
                curTextureSize = m_textureSize2D;

                for (size_t l = 0; l < (size_t)m_textureLevels2D; l++)
                {
                    gl.texSubImage2D(cubeMapTarget[k], l, 0, 0, curTextureSize, curTextureSize, format[j],
                                     formats[i].type, m_texData.data());
                    GLU_EXPECT_NO_ERROR(gl.getError(), "texSubImage2D");

                    curTextureSize /= 2;
                }
            }
        }
        gl.deleteTextures(1, &m_textureCubeMap);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    /* test with TEXTURE_3D target */
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "testing TEXTURE_3D compressed texture loading with each internal format\n"
                       << tcu::TestLog::EndMessage;

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        if (formats[i].format != GL_DEPTH_COMPONENT && formats[i].format != GL_DEPTH_STENCIL)
        {
            gl.genTextures(1, &m_texture3D);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

            gl.bindTexture(GL_TEXTURE_3D, m_texture3D);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

            gl.texStorage3D(GL_TEXTURE_3D, m_textureLevels3D, formats[i].intFormat, m_textureSize3D, m_textureSize3D,
                            m_textureSize3D);

            if (formats[i].allowedWithTex3D)
            {
                GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage3D");

                /* test each format and level */
                for (size_t j = 0; j < sizeof(format) / sizeof(format[0]); j++)
                {
                    curTextureSize = m_textureSize3D;

                    for (size_t k = 0; k < (size_t)m_textureLevels3D; k++)
                    {
                        gl.texSubImage3D(GL_TEXTURE_3D, k, 0, 0, 0, curTextureSize, curTextureSize, curTextureSize,
                                         format[j], formats[i].type, m_texData.data());
                        GLU_EXPECT_NO_ERROR(gl.getError(), "texSubImage3D");

                        curTextureSize /= 2;
                    }
                }
            }
            else
            {
                /* Using glTexStorage3D with a TEXTURE_3D target and a compressed internal
                   format should generate INVALID_OPERATION. See Khronos bug 11239. */
                auto err = gl.getError();
                if (err != GL_INVALID_OPERATION)
                {
                    m_testCtx.getLog() << tcu::TestLog::Message
                                       << "texStorage3D failed, expected GL_INVALID_OPERATION got "
                                       << glu::getErrorName(err) << tcu::TestLog::EndMessage;
                    ret = false;
                }
            }
            gl.deleteTextures(1, &m_texture3D);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
        }
    }

    /* test with TEXTURE_2D_ARRAY target */
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "testing TEXTURE_2D_ARRAY compressed texture loading with each internal format\n"
                       << tcu::TestLog::EndMessage;

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        gl.genTextures(1, &m_texture2DArray);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

        gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_texture2DArray);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

        gl.texStorage3D(GL_TEXTURE_2D_ARRAY, m_textureLevels3D, formats[i].intFormat, m_textureSize3D, m_textureSize3D,
                        m_textureSize3D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage3D");

        /* test each format and level */
        for (size_t j = 0; j < sizeof(format) / sizeof(format[0]); j++)
        {
            curTextureSize = m_textureSize3D;

            for (size_t k = 0; k < (size_t)m_textureLevels3D; k++)
            {
                gl.texSubImage3D(GL_TEXTURE_2D_ARRAY, k, 0, 0, 0, curTextureSize, curTextureSize, curTextureSize,
                                 format[j], formats[i].type, m_texData.data());
                GLU_EXPECT_NO_ERROR(gl.getError(), "texSubImage3D");

                curTextureSize /= 2;
            }
        }
        gl.deleteTextures(1, &m_texture2DArray);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    return ret;
}

bool TextureStorageCompressedDataTestCase::iterate_gles()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool ret                 = true;

    int curTextureSize = 0, curDataSize = 0;

    struct formats
    {
        GLenum intFormat;
        int bytesPerBlock;
    } formats[] = {{GL_COMPRESSED_R11_EAC, 8},
                   {GL_COMPRESSED_SIGNED_R11_EAC, 8},
                   {GL_COMPRESSED_RG11_EAC, 16},
                   {GL_COMPRESSED_SIGNED_RG11_EAC, 16},
                   {GL_COMPRESSED_RGB8_ETC2, 8},
                   {GL_COMPRESSED_SRGB8_ETC2, 8},
                   {GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 8},
                   {GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 8},
                   {GL_COMPRESSED_RGBA8_ETC2_EAC, 16},
                   {GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, 16}};

    GLenum cubeMapTarget[] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                              GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                              GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

    /* test with TEXTURE_2D target */
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "testing TEXTURE_2D compressed texture loading with each internal format\n"
                       << tcu::TestLog::EndMessage;

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        gl.genTextures(1, &m_texture2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

        gl.bindTexture(GL_TEXTURE_2D, m_texture2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

        gl.texStorage2D(GL_TEXTURE_2D, m_textureLevels2D, formats[i].intFormat, m_textureSize2D, m_textureSize2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2D");

        curTextureSize = m_textureSize2D;

        for (size_t k = 0; k < (size_t)m_textureLevels2D; k++)
        {
            curDataSize = std::max(curTextureSize / 4, 1) * std::max(curTextureSize / 4, 1);
            curDataSize *= formats[i].bytesPerBlock;

            gl.compressedTexSubImage2D(GL_TEXTURE_2D, k, 0, 0, curTextureSize, curTextureSize, formats[i].intFormat,
                                       curDataSize, m_texData.data());
            GLU_EXPECT_NO_ERROR(gl.getError(), "texSubImage3D");

            curTextureSize /= 2;
        }

        gl.deleteTextures(1, &m_texture2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    /* test with TEXTURE_CUBE_MAP target */
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "testing TEXTURE_CUBE_MAP compressed texture loading with each internal format\n"
                       << tcu::TestLog::EndMessage;

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        gl.genTextures(1, &m_textureCubeMap);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

        gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_textureCubeMap);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

        gl.texStorage2D(GL_TEXTURE_CUBE_MAP, m_textureLevels2D, formats[i].intFormat, m_textureSize2D, m_textureSize2D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2D");

        /* test each cubemap face and level */
        for (size_t k = 0; k < 6; k++)
        {
            curTextureSize = m_textureSize2D;

            for (size_t l = 0; l < (size_t)m_textureLevels2D; l++)
            {
                curDataSize = std::max(curTextureSize / 4, 1) * std::max(curTextureSize / 4, 1);
                curDataSize *= formats[i].bytesPerBlock;

                gl.compressedTexSubImage2D(cubeMapTarget[k], l, 0, 0, curTextureSize, curTextureSize,
                                           formats[i].intFormat, curDataSize, m_texData.data());
                GLU_EXPECT_NO_ERROR(gl.getError(), "texSubImage2D");

                curTextureSize /= 2;
            }
        }

        gl.deleteTextures(1, &m_textureCubeMap);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    /* test with TEXTURE_2D_ARRAY target */
    m_testCtx.getLog() << tcu::TestLog::Message
                       << "testing TEXTURE_2D_ARRAY compressed texture loading with each internal format\n"
                       << tcu::TestLog::EndMessage;

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        gl.genTextures(1, &m_texture2DArray);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

        gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_texture2DArray);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

        gl.texStorage3D(GL_TEXTURE_2D_ARRAY, m_textureLevels3D, formats[i].intFormat, m_textureSize3D, m_textureSize3D,
                        m_textureSize3D);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage3D");

        /* test each level */
        curTextureSize = m_textureSize3D;

        for (size_t k = 0; k < (size_t)m_textureLevels3D; k++)
        {
            curDataSize = std::max(curTextureSize / 4, 1) * std::max(curTextureSize / 4, 1) * curTextureSize;
            curDataSize *= formats[i].bytesPerBlock;

            gl.compressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, k, 0, 0, 0, curTextureSize, curTextureSize, curTextureSize,
                                       formats[i].intFormat, curDataSize, m_texData.data());
            GLU_EXPECT_NO_ERROR(gl.getError(), "compressedTexSubImage3D");

            curTextureSize /= 2;
        }
        gl.deleteTextures(1, &m_texture2DArray);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    return ret;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TextureStorageCompressedDataTestCase::iterate()
{
    bool ret = true;

    if (!m_testSupported)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
        /* This test should only be executed if we're running a GL4.2 context or
         * GL_EXT_texture_storage extension is supported */
        throw tcu::NotSupportedError("TextureStorageCompressedDataTestCase is not supported");
    }

    if (m_isContextES)
    {
        ret = iterate_gles();
    }
    else
    {
        ret = iterate_gl();
    }

    if (ret)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
TextureStorageTests::TextureStorageTests(deqp::Context &context)
    : TestCaseGroup(context, "texture_storage", "Verify conformance of texture storage functionality")
{
}

/** Initializes the test group contents. */
void TextureStorageTests::init()
{
    addChild(new TextureStorageCompressedDataTestCase(m_context));
}

} // namespace glcts
