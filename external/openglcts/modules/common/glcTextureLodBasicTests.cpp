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
 * \file  glcTextureLodBasicTests.cpp
 * \brief Conformance tests for the texture lod selection functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "glcTextureLodBasicTests.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace glu;

namespace
{
// clang-format off
/* full screen quad */
const float fs_quad[] = { -1.0f, -1.0f, 0.0f, 1.0f,
                          1.0f, -1.0f, 0.0f, 1.0f,
                          -1.0f, 1.0f, 0.0f, 1.0f,
                          1.0f, 1.0f,  0.0f, 1.0f };

GLubyte colorarray[][4] = {
    { 255, 0, 0, 255 },   // red
    { 0, 255, 0, 255 },   // green
    { 0, 0, 255, 255 },   // blue
    { 127, 255, 0, 255 }, // redish green
};
// clang-format on

/* maually calculate the result of texturing */
/* returned to parameter r. */
void colorTexturing(const glw::Functions &gl, float lodBase, float lodBiasSum, float lodMin, float lodMax,
                    int levelBase, int levelMax, int levelBaseMaxSize, GLenum magFilter, GLenum minFilter, bool mipmap,
                    GLubyte *colors, float *r)
{
    const float maxColor = 255.0f;

    float lodConstant = 0.f, lod = 0.f;
    int d1 = 0, d2 = 0;

    auto copy_pixel = [maxColor](float *dst, GLubyte *src)
    {
        dst[0] = static_cast<float>(src[0]) / maxColor;
        dst[1] = static_cast<float>(src[1]) / maxColor;
        dst[2] = static_cast<float>(src[2]) / maxColor;
        dst[3] = static_cast<float>(src[3]) / maxColor;
    };

    if (!mipmap)
    {
        /* When not mipmapped, level base is used */
        d1 = levelBase;
        copy_pixel(r, &colors[d1 * 4]);
        return;
    }

    /* Mipmapped */
    /* Check the constant divide the mag or min filter */
    if ((magFilter == GL_LINEAR) &&
        ((minFilter == GL_NEAREST_MIPMAP_NEAREST) || (minFilter == GL_NEAREST_MIPMAP_LINEAR)))
    {
        lodConstant = 0.5f;
    }
    else
    {
        lodConstant = 0.0f;
    }

    /* Get final lod which is clamp */
    lod = lodBase;
    if (lodBiasSum != 0.0f)
    {
        float maxLodBias;
        gl.getFloatv(GL_MAX_TEXTURE_LOD_BIAS, &maxLodBias);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getFloatv");

        if (lodBiasSum > maxLodBias)
        {
            lodBiasSum = maxLodBias;
        }
        else if (lodBiasSum < -maxLodBias)
        {
            lodBiasSum = -maxLodBias;
        }
        lod += lodBiasSum;
    }
    if (lod < lodMin)
    {
        lod = lodMin;
    }
    else if (lod > lodMax)
    {
        lod = lodMax;
    }

    /* Check which filter is to use */
    if (lod > lodConstant)
    {
        /* Min filter */
        int p, q, tempMaxSize = 1;
        int log2TempMaxSize = 0;

        /* Calculate the max level */

        while (tempMaxSize * 2 <= levelBaseMaxSize)
        {
            log2TempMaxSize += 1;
            tempMaxSize <<= 1;
        }

        p = log2TempMaxSize + levelBase;
        q = levelMax;
        if (p < q)
        {
            q = p;
        }

        if ((minFilter == GL_NEAREST) || (minFilter == GL_LINEAR))
        {
            /* base level is used */
            d1 = levelBase;
            copy_pixel(r, &colors[d1 * 4]);
            return;
        }
        else if ((minFilter == GL_NEAREST_MIPMAP_NEAREST) || (minFilter == GL_LINEAR_MIPMAP_NEAREST))
        {
            /* only one array is selected */
            if (lod <= 0.5f)
            {
                d1 = levelBase;
            }
            else if (levelBase + lod <= q + 0.5f)
            {
                d1 = ceil(levelBase + lod + 0.5f) - 1;
            }
            else
            {
                d1 = q;
            }

            copy_pixel(r, &colors[d1 * 4]);
            return;
        }
        else
        {
            float fracLod;
            int i;

            /* interplate between two arrays */
            if (levelBase + lod >= q)
            {
                d1 = q;
                d2 = q;
            }
            else
            {
                d1 = floor(levelBase + lod);
                d2 = d1 + 1;
            }

            fracLod = (float)fmod(lod, 1.0f);

            for (i = 0; i < 4; ++i)
            {
                const float d1norm = static_cast<float>(colors[d1 * 4 + i]) / maxColor;
                const float d2norm = static_cast<float>(colors[d2 * 4 + i]) / maxColor;
                r[i]               = (1.0f - fracLod) * d1norm + fracLod * d2norm;
            }

            return;
        }
    }
    else
    {
        /* Mag filter, base level is used */
        d1 = levelBase;
        copy_pixel(r, &colors[d1 * 4]);
        return;
    }
}

} // namespace

namespace glcts
{

// clang-format off
/** @brief Vertex shader source code to test vertex lookup texture lod bias. */
const glw::GLchar* glcts::TextureLodSelectionTestCase::m_shader_basic_vert =
    R"(${VERSION}
    in vec4 vertex;
    out vec2    tex;

    void main(void)
    {
        gl_Position = vertex;
        tex = vertex.xy * 0.5 + 0.5;
    }
    )";

/** @brief Vertex shader source code to test fragment lookup texture lod bias. */
const glw::GLchar* glcts::TextureLodSelectionTestCase::m_shader_basic_1d_frag =
    R"(${VERSION}
    ${PRECISION}

    in vec2 tex;
    out vec4 frag;

    uniform float      scale;
    uniform sampler1D texture0;

    void main(void)
    {
        frag = texture(texture0, tex.x * scale);
    }
    )";

/** @brief Fragment shader source code to test fragment lookup texture lod bias. */
const glw::GLchar* glcts::TextureLodSelectionTestCase::m_shader_basic_2d_frag =
    R"(${VERSION}
    ${PRECISION}

    in vec2 tex;
    out vec4 frag;

    uniform float      scale;
    uniform sampler2D texture0;

    void main(void)
    {
        frag = texture(texture0, vec2(tex.x * scale, 0));
    }
    )";
// clang-format on

/** Constructor.
 *
 *  @param context     Rendering context
 */
TextureLodSelectionTestCase::TextureLodSelectionTestCase(deqp::Context &context)
    : TestCase(context, "lod_selection", "Verifies texture LOD selection functionality")
    , m_texture(0)
    , m_vao(0)
    , m_vbo(0)
    , m_isContextES(false)
{
}

/** Stub deinit method. */
void TextureLodSelectionTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void TextureLodSelectionTestCase::init()
{
    const glu::RenderContext &renderContext = m_context.getRenderContext();
    glu::GLSLVersion glslVersion            = glu::getContextTypeGLSLVersion(renderContext.getType());
    m_isContextES                           = glu::isContextTypeES(renderContext.getType());

    specializationMap["VERSION"]   = glu::getGLSLVersionDeclaration(glslVersion);
    specializationMap["PRECISION"] = "";

    if (m_isContextES)
    {
        specializationMap["PRECISION"] = "precision highp float;";
    }
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TextureLodSelectionTestCase::iterate()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    std::vector<GLenum> textures = {GL_TEXTURE_2D};
    if (!m_isContextES)
        textures.push_back(GL_TEXTURE_1D);

    bool ret = true;

    auto create_program = [&](const std::string &vert, const std::string &frag)
    {
        /* vertex shader test */
        std::string vshader = tcu::StringTemplate(vert).specialize(specializationMap);
        std::string fshader = tcu::StringTemplate(frag).specialize(specializationMap);

        ProgramSources sources = makeVtxFragSources(vshader, fshader);
        return new ShaderProgram(gl, sources);
    };

    for (size_t n = 0; n < textures.size(); ++n)
    {
        GLenum tex_target = textures[n];

        if (m_isContextES && tex_target == GL_TEXTURE_1D)
            continue;

        ShaderProgram *program = nullptr;
        if (tex_target == GL_TEXTURE_2D)
        {
            program = create_program(m_shader_basic_vert, m_shader_basic_2d_frag);
        }
        else if (tex_target == GL_TEXTURE_1D)
        {
            program = create_program(m_shader_basic_vert, m_shader_basic_1d_frag);
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Texture target not supported " << tex_target
                               << tcu::TestLog::EndMessage;
        }

        if (!program->isOk())
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                               << "Vertex: " << program->getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                               << program->getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
                               << "Fragment: " << program->getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                               << program->getShader(SHADERTYPE_FRAGMENT)->getSource() << "\n"
                               << "Program: " << program->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
            TCU_FAIL("Compile failed");
        }
        else
        {
            /* fragment shader test */
            setBuffers(*program);

            GLint locScale = gl.getUniformLocation(program->getProgram(), "scale");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
            TCU_CHECK_MSG(locScale != -1, "scale location not valid");

            createLodTexture(tex_target);

            /*
            2. Set TEXTURE_BASE_LEVEL to 1 and TEXTURE_MAX_LEVEL to 2. Render
            with a LOD of -1, and check that level 1 is used.

            3. Set TEXTURE_BASE_LEVEL to 1 and TEXTURE_MAX_LEVEL to 2. Render
            with a LOD of 3, and check that level 2 is used.

            4. Set TEXTURE_BASE_LEVEL to 1 and TEXTURE_MAX_LEVEL to 2. Render
            with a LOD of 0.5, and check that this is correctly interpolated.
            */

            if (!drawAndVerify(locScale, -1.0f, 1, 2, 4, 0.0f, -1000.0f, 1000.0f, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR,
                               true))
            {
                ret = false;
            }

            if (!drawAndVerify(locScale, 3.0f, 1, 2, 4, 0.0f, -1000.0f, 1000.0f, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR,
                               true))
            {
                ret = false;
            }

            if (!drawAndVerify(locScale, 0.5f, 1, 2, 4, 0.0f, -1000.0f, 1000.0f, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR,
                               true))
            {
                ret = false;
            }

            /*
            5. Set TEXTURE_BASE_LEVEL to 1, TEXTURE_MAX_LEVEL to 2,
            TEXTURE_MIN_LOD to 0.5, TEXTURE_MAX_LOD to 1000. Render with a LOD
            of 0, and check that it is clamped to 0.5.
            */
            gl.texParameterf(tex_target, GL_TEXTURE_MIN_LOD, 0.5f);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");
            gl.texParameterf(tex_target, GL_TEXTURE_MAX_LOD, 1000.0f);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");
            if (!drawAndVerify(locScale, 0.0f, 1, 2, 4, 0.0f, 0.5f, 1000.0f, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, true))
            {
                ret = false;
            }

            /*
            6. Set TEXTURE_BASE_LEVEL to 1, TEXTURE_MAX_LEVEL to 2,
            TEXTURE_MIN_LOD to -1000, TEXTURE_MAX_LOD to 0.5. Render with a LOD
            of 1, and check that it is clamped to 0.5.
            */
            gl.texParameterf(tex_target, GL_TEXTURE_MIN_LOD, -1000.0f);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");
            gl.texParameterf(tex_target, GL_TEXTURE_MAX_LOD, 0.5f);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");
            if (!drawAndVerify(locScale, 1.0f, 1, 2, 4, 0.0f, -1000.0f, 0.5f, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, true))
            {
                ret = false;
            }

            // Release resources
            if (program)
                delete program;

            releaseBuffers();
            releaseTexture();
        }
    }

    if (ret)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

/* function activates the program that is given as a argument */
/* and sets vertex and texture attributes */
void TextureLodSelectionTestCase::setBuffers(const glu::ShaderProgram &program)
{
    if (program.isOk())
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();

        gl.genVertexArrays(1, &m_vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
        gl.bindVertexArray(m_vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

        gl.genBuffers(1, &m_vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
        gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

        gl.bufferData(GL_ARRAY_BUFFER, sizeof(fs_quad), (GLvoid *)fs_quad, GL_DYNAMIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

        GLint locVertices = -1, locTexture = -1;

        gl.useProgram(program.getProgram());
        GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

        locVertices = gl.getAttribLocation(program.getProgram(), "vertex");
        GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");
        if (locVertices != -1)
        {
            gl.enableVertexAttribArray(0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

            GLuint strideSize = sizeof(fs_quad) / 4;

            gl.vertexAttribPointer(locVertices, 4, GL_FLOAT, GL_FALSE, strideSize, DE_NULL);
            GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
        }

        locTexture = gl.getUniformLocation(program.getProgram(), "texture0");
        GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
        if (locTexture != -1)
        {
            gl.uniform1i(locTexture, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");
        }
    }
}

/* function releases vertex buffers */
void TextureLodSelectionTestCase::releaseBuffers()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.disableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

    if (m_vbo)
    {
        gl.deleteBuffers(1, &m_vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
        m_vbo = 0;
    }

    if (m_vao)
    {
        gl.deleteVertexArrays(1, &m_vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");
        m_vao = 0;
    }
}

/** Texture is generated from constant color array.
 */
void TextureLodSelectionTestCase::createLodTexture(const GLenum target)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

    gl.bindTexture(target, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

    gl.viewport(0, 0, 4, 4);
    GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");
    /*
    1. Create a texture with 8x8, 4x4, 2x2 and 1x1 images in levels 0, 1, 2
    and 3 respectively, with consistent formats and types. Set
    TEXTURE_MAG_FILTER to LINEAR and TEXTURE_MIN_FILTER to
    LINEAR_MIPMAP_LINEAR.
    */
    for (int i = 0; i < 4; ++i)
    {
        createSolidTexture(target, i, 8 >> i, colorarray[i]);
    }

    gl.texParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

    gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

    gl.texParameterf(target, GL_TEXTURE_BASE_LEVEL, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");

    gl.texParameterf(target, GL_TEXTURE_MAX_LEVEL, 2.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");

    gl.texParameterf(target, GL_TEXTURE_MIN_LOD, -1000.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");

    gl.texParameterf(target, GL_TEXTURE_MAX_LOD, 1000.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");
}

/* Creates a texture that has solid color in given lod level */
void TextureLodSelectionTestCase::createSolidTexture(const GLenum tex_target, const int level, const int size,
                                                     const GLubyte *const color)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    std::vector<GLubyte> data(size * size * 4);

    for (int i = 0; i < size * size; ++i)
    {
        data[i * 4 + 0] = color[0];
        data[i * 4 + 1] = color[1];
        data[i * 4 + 2] = color[2];
        data[i * 4 + 3] = color[3];
    }

    if (tex_target == GL_TEXTURE_2D)
    {
        gl.texImage2D(tex_target, level, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
    }
    else if (!m_isContextES && tex_target == GL_TEXTURE_1D)
    {
        gl.texImage1D(tex_target, level, GL_RGBA8, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage1D");
    }
}

/*
Function draws a quad using given lod. Minlod and maxlod values are used
to calculate reference value.
*/
bool TextureLodSelectionTestCase::drawAndVerify(const GLint locScale, const float lodLevel, const int levelBase,
                                                const int levelMax, const int levelBaseMaxSize,
                                                const float lodBiasshader, const float lodMin, const float lodMax,
                                                const GLenum magFilter, const GLenum minFilter, const bool mipmap)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    /* is this selection ok? */
    float scale = 8.0f * (float)pow(2.0f, lodLevel) / 8.0f;

    float result[4] = {0, 0, 0, 0};

    gl.uniform1f(locScale, scale);
    GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

    /* Expected result */
    colorTexturing(gl, lodLevel, lodBiasshader, lodMin, lodMax, levelBase, levelMax, levelBaseMaxSize, magFilter,
                   minFilter, mipmap, (GLubyte *)colorarray, result);

    /* Result of comparison of two arrays are returned */
    return doComparison(4, result);
}

/* Compares given expected result and framebuffer output. Pixel epsilon is one. */
bool TextureLodSelectionTestCase::doComparison(const int size, const float *const expectedcolor)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool ret                 = true;

    std::vector<GLuint> data(size * size);
    data.assign(data.size(), 0u);

    /* one pixel is read */
    const tcu::PixelFormat &pixelFormat = m_context.getRenderTarget().getPixelFormat();
    const bool use10Bits                = ((pixelFormat.redBits == 10) && (pixelFormat.greenBits == 10) &&
                            (pixelFormat.blueBits == 10) && (pixelFormat.alphaBits == 2 || pixelFormat.alphaBits == 0));
    GLenum type                         = (use10Bits ? GL_UNSIGNED_INT_2_10_10_10_REV : GL_UNSIGNED_BYTE);
    const auto numChannels              = (pixelFormat.alphaBits == 0 ? 3 : 4);

    gl.readPixels(0, 0, size, size, GL_RGBA, type, data.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

    GLint col_bits[] = {pixelFormat.redBits, pixelFormat.greenBits, pixelFormat.blueBits, pixelFormat.alphaBits};

    auto calcEpsilon = [](const GLint bits)
    {
        auto zero = ldexp(1.f, -13);
        GLfloat e = zero;
        if (bits != 0)
        {
            e = (1.0f / (ldexp(1.0f, bits) - 1.0f)) + zero;
            if (e > 1.0f)
                e = 1.f;
        }
        return e;
    };

    float epsilon[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < numChannels; ++i)
        epsilon[i] = calcEpsilon(col_bits[i]);

    for (int i = 0; i < size * size; ++i)
    {
        const GLuint &pixel    = data.at(i);
        const GLubyte *pxBytes = reinterpret_cast<const GLubyte *>(&pixel);
        float resultColor[4]   = {0.0f, 0.0f, 0.0f, 0.0f};

        if (use10Bits)
        {
            // Note this is a strange way to store RGB10A2 but it matches what implementations do.
            resultColor[0] = static_cast<float>(pixel & 0x3FF) / 1023.0f;
            resultColor[1] = static_cast<float>((pixel >> 10) & 0x3FF) / 1023.0f;
            resultColor[2] = static_cast<float>((pixel >> 20) & 0x3FF) / 1023.0f;
            resultColor[3] = static_cast<float>((pixel >> 30) & 0x3) / 3.0f;
        }
        else
        {
            // If not 10-bit then we already converted to 8-bit (UNSIGNED_BYTE) in the ReadPixels call, above.
            resultColor[0] = static_cast<float>(pxBytes[0]) / 255.0f;
            resultColor[1] = static_cast<float>(pxBytes[1]) / 255.0f;
            resultColor[2] = static_cast<float>(pxBytes[2]) / 255.0f;
            resultColor[3] = static_cast<float>(pxBytes[3]) / 255.0f;
        }

        for (int j = 0; j < numChannels; ++j)
        {
            if (std::abs(resultColor[j] - expectedcolor[j]) > epsilon[j])
            {
                m_testCtx.getLog() << tcu::TestLog::Message
                                   << "TextureLodSelectionTestCase: Unexpected result of color comparison at pixel "
                                   << i << ": " << expectedcolor[0] << " " << expectedcolor[1] << " "
                                   << expectedcolor[2] << " " << expectedcolor[3] << " != " << resultColor[0] << " "
                                   << resultColor[1] << " " << resultColor[2] << " " << resultColor[3]
                                   << tcu::TestLog::EndMessage;
                ret = false;
                break;
            }
        }
    }

    return ret;
}

/** Release texture.
 *
 *  @param gl  OpenGL functions wrapper
 */
void TextureLodSelectionTestCase::releaseTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    if (m_texture)
    {
        gl.deleteTextures(1, &m_texture);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    m_texture = 0;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
TextureLodBasicTests::TextureLodBasicTests(deqp::Context &context)
    : TestCaseGroup(context, "texture_lod_basic", "Verify conformance of texture lod basic functionality")
{
}

/** Initializes the test group contents. */
void TextureLodBasicTests::init()
{
    addChild(new TextureLodSelectionTestCase(m_context));
}

} // namespace glcts
