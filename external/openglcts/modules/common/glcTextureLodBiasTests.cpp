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
 * \file  glcTextureLodBiasTests.cpp
 * \brief Conformance tests for the texture lod bias functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "glcTextureLodBiasTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"

#include <cmath>

using namespace glw;
using namespace glu;

namespace
{
const GLuint LEVELS = 8;

/* full screen quad */
const float quad[] = {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f,
                      -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f};

/* constant array table */
const GLubyte COLORS[LEVELS + 1][4] = {
    {255, 0, 0, 255},     // red
    {0, 255, 0, 255},     // green
    {0, 0, 255, 255},     // blue
    {255, 255, 0, 255},   // yellow
    {0, 255, 255, 255},   // cyan
    {255, 0, 255, 255},   // purple
    {255, 128, 128, 255}, // light red
    {128, 255, 128, 255}, // light green
    {128, 128, 255, 255}, // light blue
};

/* maually calculate the result of texturing */
/* returned to parameter r. */
void colorTexturing(const glw::Functions &gl, float lodBase, float lodBiasSum, float lodMin, float lodMax,
                    int levelBase, int levelMax, int levelBaseMaxSize, GLenum magFilter, GLenum minFilter, bool mipmap,
                    GLubyte *colors, GLubyte *r)
{
    float lodConstant = 0.f, lod = 0.f;
    int d1 = 0, d2 = 0;

    auto copy_pixel = [](GLubyte *dst, GLubyte *src)
    {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
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
                r[i] = (GLubyte)((1.0f - fracLod) * (float)colors[d1 * 4 + i] + fracLod * (float)colors[d2 * 4 + i]);
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
const glw::GLchar* glcts::TextureLodBiasAllTestCase::m_vert_shader_sampler_vert =
    R"(${VERSION}
    ${EXTENSION}

    in vec4 vertex;
    out vec4 tex;

    uniform float      lodbase;
    uniform sampler2D texture0;

    void main(void)
    {
        gl_Position = vertex;
        tex = textureLod(texture0, vertex.xy * 0.5 + 0.5, lodbase);
    }
    )";

/** @brief Fragment shader source code to test vertex lookup texture lod bias. */
const glw::GLchar* glcts::TextureLodBiasAllTestCase::m_frag_shader_sampler_vert =
    R"(${VERSION}
    ${PRECISION}

    in vec4     tex;
    out vec4 frag;

    void main(void)
    {
        frag = tex;
    }
    )";

/** @brief Vertex shader source code to test fragment lookup texture lod bias. */
const glw::GLchar* glcts::TextureLodBiasAllTestCase::m_vert_shader_sampler_frag =
    R"(${VERSION}
    ${EXTENSION}

    in vec4 vertex;
    out vec2 tex;

    void main(void)
    {
        gl_Position = vertex;
        tex.xy = vertex.xy * 0.5 + 0.5;
    }
    )";

/** @brief Fragment shader source code to test fragment lookup texture lod bias. */
const glw::GLchar* glcts::TextureLodBiasAllTestCase::m_frag_shader_sampler_frag =
    R"(${VERSION}
    ${PRECISION}

    in vec2 tex;
    out vec4 frag;

    uniform float      biasshader;
    uniform float      scale;
    uniform sampler2D texture0;

    void main(void)
    {
        frag = texture(texture0, vec2(scale * tex.x, 0), biasshader);
    }
    )";
// clang-format on

/** Constructor.
 *
 *  @param context     Rendering context
 */
TextureLodBiasAllTestCase::TextureLodBiasAllTestCase(deqp::Context &context)
    : TestCase(context, "texture_lod_bias_all", "Verifies most of biases combinations from the possible ranges")
    , m_texture(0)
    , m_target(0)
    , m_fbo(0)
    , m_vao(0)
    , m_vbo(0)
    , m_isContextES(false)
    , m_testSupported(false)
    , m_vertexLookupSupported(true)
{
}

/** Stub deinit method. */
void TextureLodBiasAllTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void TextureLodBiasAllTestCase::init()
{
    const glu::RenderContext &renderContext = m_context.getRenderContext();
    glu::GLSLVersion glslVersion            = glu::getContextTypeGLSLVersion(renderContext.getType());
    m_isContextES                           = glu::isContextTypeES(renderContext.getType());

    specializationMap["VERSION"]   = glu::getGLSLVersionDeclaration(glslVersion);
    specializationMap["EXTENSION"] = "";
    specializationMap["PRECISION"] = "";

    if (m_isContextES)
    {
        specializationMap["PRECISION"] = "precision highp float;";
    }

    auto contextType = m_context.getRenderContext().getType();
    if (m_isContextES)
    {
        m_maxErrorTolerance = 11;
        if (glu::contextSupports(contextType, glu::ApiType::es(3, 0)))
        {
            m_testSupported = true;
        }
        else
        {
            m_testSupported = m_context.getContextInfo().isExtensionSupported("GL_EXT_texture_lod_bias");
            if (m_testSupported)
                specializationMap["EXTENSION"] = "#extension GL_EXT_texture_lod_bias : enable";
            m_vertexLookupSupported = false;
        }
    }
    else
    {
        m_maxErrorTolerance = 5;
        /* This test should only be executed if we're running a GL>=3.0 context */
        if (glu::contextSupports(contextType, glu::ApiType::core(3, 0)))
        {
            m_testSupported = true;
        }
    }
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TextureLodBiasAllTestCase::iterate()
{
    if (!m_testSupported)
    {
        throw tcu::NotSupportedError("Test texture_lod_bias_all is not supported");
        return STOP;
    }

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    bool returnvalue  = true;
    const int SAMPLES = 128;
    int failedbiases  = 0;
    GLfloat m         = 0.f;

    gl.getFloatv(GL_MAX_TEXTURE_LOD_BIAS, &m);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getFloatv");

    createRenderingResources();

    failedbiases = 0;

    auto create_program = [&](const std::string &vert, const std::string &frag)
    {
        /* vertex shader test */
        std::string vshader = tcu::StringTemplate(vert).specialize(specializationMap);
        std::string fshader = tcu::StringTemplate(frag).specialize(specializationMap);

        ProgramSources sources = makeVtxFragSources(vshader, fshader);
        return ShaderProgram(gl, sources);
    };

    ShaderProgram program_vert = create_program(m_vert_shader_sampler_vert, m_frag_shader_sampler_vert);

    if (!program_vert.isOk())
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                           << "Vertex: " << program_vert.getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                           << program_vert.getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
                           << "Fragment: " << program_vert.getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                           << program_vert.getShader(SHADERTYPE_FRAGMENT)->getSource() << "\n"
                           << "Program: " << program_vert.getProgramInfo().infoLog << tcu::TestLog::EndMessage;
        TCU_FAIL("Compile failed");
    }

    /* fragment shader test */
    setBuffers(program_vert);

    for (int i = 0; i < SAMPLES; ++i)
    {
        /* this is the texture object bias */
        float f = m * (((float)i / (float)(SAMPLES - 1)) * 2.0f - 1.0f);

        for (int j = 0; j < SAMPLES; ++j)
        {
            /* this is the shader bias */
            float g = m * (((float)j / (float)(SAMPLES - 1)) * 2.0f - 1.0f);

            if (drawQuad(program_vert.getProgram(), true, 0.0f, f, g, -1000.0f, 1000.0f) == false)
            {
                returnvalue = false;
                ++failedbiases;
                break;
            }
        }
        if (returnvalue == false)
            break;
    }

    releaseBuffers();

    ShaderProgram program_frag = create_program(m_vert_shader_sampler_frag, m_frag_shader_sampler_frag);

    if (!program_frag.isOk())
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                           << "Vertex: " << program_frag.getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                           << program_frag.getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
                           << "Fragment: " << program_frag.getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                           << program_frag.getShader(SHADERTYPE_FRAGMENT)->getSource() << "\n"
                           << "Program: " << program_frag.getProgramInfo().infoLog << tcu::TestLog::EndMessage;
        TCU_FAIL("Compile failed");
    }

    /* fragment shader test */
    setBuffers(program_frag);

    for (int i = 0; i < SAMPLES; ++i)
    {
        /* this is the texture object bias */
        float f = m * (((float)i / (float)(SAMPLES - 1)) * 2.0f - 1.0f);

        for (int j = 0; j < SAMPLES; ++j)
        {
            /* this is the shader bias */
            float g = m * (((float)j / (float)(SAMPLES - 1)) * 2.0f - 1.0f);

            if (drawQuad(program_frag.getProgram(), false, 0.0f, f, g, -1000.0f, 1000.0f) == false)
            {
                returnvalue = false;
                ++failedbiases;
                break;
            }
        }
        if (returnvalue == false)
            break;
    }

    releaseBuffers();

    if (failedbiases > 0)
    {
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Total failed lod bias combinations (vertex shader): " << failedbiases
                           << tcu::TestLog::EndMessage;
    }

    // Release texture
    releaseRenderingResources();

    if (returnvalue)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

/* function activates the program that is given as a argument */
/* and sets vertex and texture attributes */
void TextureLodBiasAllTestCase::setBuffers(const glu::ShaderProgram &program)
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

        gl.bufferData(GL_ARRAY_BUFFER, sizeof(quad), (GLvoid *)quad, GL_DYNAMIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

        GLint locVertices = -1;
        GLint locTexture  = -1;

        gl.useProgram(program.getProgram());
        GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

        locVertices = gl.getAttribLocation(program.getProgram(), "vertex");
        if (locVertices != -1)
        {
            gl.enableVertexAttribArray(0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

            GLuint strideSize = sizeof(quad) / 4;

            gl.vertexAttribPointer(locVertices, 4, GL_FLOAT, GL_FALSE, strideSize, nullptr);
            GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
        }

        locTexture = gl.getUniformLocation(program.getProgram(), "texture0");
        if (locTexture != -1)
        {
            gl.uniform1i(locTexture, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");
        }
    }
}

/* function releases vertex buffers */
void TextureLodBiasAllTestCase::releaseBuffers()
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
void TextureLodBiasAllTestCase::createRenderingResources()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // setup fbo along with attached color texture
    gl.genTextures(1, &m_target);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

    gl.bindTexture(GL_TEXTURE_2D, m_target);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

    gl.texStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2D");

    gl.genFramebuffers(1, &m_fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");

    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    gl.framebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_target, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");

    // setup testing texture
    GLuint texturesize = 1 << LEVELS;
    std::vector<GLubyte> data(texturesize * texturesize * 3);

    gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "pixelStorei");

    gl.genTextures(1, &m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

    gl.bindTexture(GL_TEXTURE_2D, m_texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

    gl.viewport(0, 0, texturesize, texturesize);
    GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

    for (GLuint j = 0; j <= LEVELS; ++j)
    {
        for (GLuint i = 0; i < texturesize * texturesize; ++i)
        {
            data[i * 3 + 0] = COLORS[j][0];
            data[i * 3 + 1] = COLORS[j][1];
            data[i * 3 + 2] = COLORS[j][2];
        }

        gl.texImage2D(GL_TEXTURE_2D, j, GL_RGB, texturesize, texturesize, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");

        texturesize = texturesize >> 1;
    }

    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");
}

/** Render full screen textured quad.
 *  @return Returns true if no error occured, false otherwise.
 */
bool TextureLodBiasAllTestCase::drawQuad(GLuint program, bool bVertexshader, float lodbase, float statebias,
                                         float shaderbias, float lodMin, float lodMax)
{
    const glw::Functions &gl  = m_context.getRenderContext().getFunctions();
    GLubyte expectedresult[4] = {0, 0, 0, 0};
    GLubyte readdata[]        = {0, 0, 0, 0};
    GLfloat biasSum           = statebias + shaderbias;
    GLint lodbaseLoc = 0, biasLoc = 0, scaleLoc = 0;
    GLint epsilon   = 0;
    GLint precision = 0;

    float savedStatebias  = statebias;
    float savedShaderbias = shaderbias;

    if (bVertexshader)
    {
        if (!m_vertexLookupSupported)
        {
            /* Vertex shader is tested with textureLod and TEXTURE_LOD_BIAS,
               which should be skipped for GLES ES version prior 3.0.
             */
            return true;
        }

        lodbaseLoc = gl.getUniformLocation(program, "lodbase");
        if (lodbaseLoc == -1)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Couldn't get shader uniform lodbase."
                               << tcu::TestLog::EndMessage;
            return false;
        }

        /* Shader bias is not used and is accumulated into state bias */
        statebias = biasSum;
        if (!m_isContextES)
        {
            gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, statebias);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");

            /* Explicit lodbase value for textureLod */
            gl.uniform1f(lodbaseLoc, lodbase);
        }
        else
        {
            /* ES does not have state bias, so accumulate it into shader bias */
            gl.uniform1f(lodbaseLoc, biasSum);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");
    }
    else
    {
        biasLoc  = gl.getUniformLocation(program, "biasshader");
        scaleLoc = gl.getUniformLocation(program, "scale");
        if ((biasLoc) == -1 || (scaleLoc) == -1)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Couldn't get shader uniform(s) biasshader or/and scale."
                               << tcu::TestLog::EndMessage;
            return false;
        }

        if (!m_isContextES)
        {
            gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, statebias);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");
        }
        else
        {
            /* ES does not have state bias, so accumulate it into shader bias */
            shaderbias = biasSum;
        }

        gl.uniform1f(biasLoc, shaderbias);
        GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");

        /* Setup scale to get proper lodbase */
        gl.uniform1f(scaleLoc, (float)pow(2.0f, lodbase));
        GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");
    }

    gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, lodMin);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");

    gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, lodMax);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameterf");

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

    /* one pixel is read */
    gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, readdata);
    GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

    colorTexturing(gl, lodbase, biasSum, lodMin, lodMax, 0, 8, 1 << LEVELS, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, true,
                   (GLubyte *)COLORS, expectedresult);

    precision = m_context.getRenderTarget().getPixelFormat().redBits;
    epsilon   = std::max(256 / (1 << precision), m_maxErrorTolerance);

    if (std::fabs(readdata[0] - expectedresult[0]) > epsilon || std::fabs(readdata[1] - expectedresult[1]) > epsilon ||
        std::fabs(readdata[2] - expectedresult[2]) > epsilon || std::fabs(readdata[3] - expectedresult[3]) > epsilon)
    {
        char textBuf[512] = {0};
        std::snprintf(textBuf, 512, "texture bias (%f), shader bias(%f), sum(%f): %d %d %d %d != %d %d %d %d",
                      savedStatebias, savedShaderbias, savedStatebias + savedShaderbias, readdata[0], readdata[1],
                      readdata[2], readdata[3], expectedresult[0], expectedresult[1], expectedresult[2],
                      expectedresult[3]);
        m_testCtx.getLog() << tcu::TestLog::Message << textBuf << tcu::TestLog::EndMessage;
        return false;
    }
    else
    {
        return true;
    }
}

/** Release texture.
 *
 *  @param gl  OpenGL functions wrapper
 */
void TextureLodBiasAllTestCase::releaseRenderingResources()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (m_texture)
    {
        gl.deleteTextures(1, &m_texture);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    if (m_target)
    {
        gl.deleteTextures(1, &m_target);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    if (m_fbo)
    {
        gl.deleteFramebuffers(1, &m_fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    m_texture = 0;
    m_target  = 0;
    m_fbo     = 0;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
TextureLodBiasTests::TextureLodBiasTests(deqp::Context &context)
    : TestCaseGroup(context, "texture_lod_bias", "Verify conformance of texture lod bias functionality")
{
}

/** Initializes the test group contents. */
void TextureLodBiasTests::init()
{
    addChild(new TextureLodBiasAllTestCase(m_context));
}

} // namespace glcts
