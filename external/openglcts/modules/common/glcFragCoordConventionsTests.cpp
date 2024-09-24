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
 * \file  glcFragCoordConventionsTests.cpp
 * \brief Conformance tests for frag coord positioning functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "glcFragCoordConventionsTests.hpp"
#include "glcMisc.hpp"
#include "gluContextInfo.hpp"
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

#define NUM_SHADERS_MULTISAMPLE 2 // Case 3.4
#define NUM_SAMPLE_POINTS 4
#define OFFSET 7
#define FBO_X 512
#define FBO_Y 512

typedef GLuint sampleSet[NUM_SAMPLE_POINTS];

/*
 * Reference colors and shader names for case 3.4
 */
sampleSet refColorsMultisample[NUM_SHADERS_MULTISAMPLE] = {
    {0xff800404, 0xff8004fc, 0xff80fc04, 0xff80fcfc},
    {0xff800404, 0xff8004fc, 0xff80fc04, 0xff80fcfc},
};

/*
 * Quad for default rendering
 */
static const GLfloat defaultQuad[] = {
    -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
};

/*
 * Quad pair for cull test
 */
const GLfloat cullQuad[] = {
    -1.0f, -1.0f, 0.0f, 1.0f, 0.0f,  -1.0f, 0.0f, 1.0f, -1.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,

    -0.0f, -1.0f, 0.0f, 1.0f, -0.0f, 1.0f,  0.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
};

// clang-format off
/** @brief Vertex shader source code for default fragment coord conventions testing. */
const glw::GLchar* default_shader_vert =
    R"(${VERSION}
    ${EXTENSION}

    layout (location = 0) in vec4 pos;
    out vec4 i;

    uniform float windowWidth;
    uniform float windowHeight;
    uniform float n;
    uniform float f;

    void main()
    {
      gl_Position = pos;
      i = vec4((pos.x+1.0)*0.5*windowWidth, (pos.y+1.0)*0.5*windowHeight, (f-n)*0.5*pos.z + (f+n)*0.5, pos.w);
    }
    )";

/** @brief Fragment shader source code for default fragment coord conventions testing. */
const glw::GLchar* default_shader_frag =
    R"(${VERSION}
    ${EXTENSION}
    ${PRECISION}

    in vec4 i;

    ${COORD_LAYOUT}
    layout (location = 0) out vec4 myColor;

    void main()
    {
        float w = float(gl_SampleID+1)/4.0;
        w*=w;
        myColor = i * vec4(gl_SamplePosition.x*w, gl_SamplePosition.y*w, 1.0, 1.0);
    }
    )";



/** @brief Fragment shader source code to test changed frag coord convention. */
// from no_origin_upper_left_pixel_integer_center_multisample_shader.frag
const glw::GLchar* multisample_shader_frag =
    R"(${VERSION}
    ${EXTENSION}
    ${PRECISION}

    in vec4 i;

    layout (location = 0) out vec4 myColor;

    void main()
    {
        float w = float(gl_SampleID+1)/4.0;
        w*=w;
        myColor = i * vec4(gl_SamplePosition.x*w, gl_SamplePosition.y*w, 1.0, 1.0);
    }
    )";
// clang-format on

} // namespace

namespace glcts
{

/** Constructor.
 *
 *  @param context     Rendering context
 */
FragCoordConventionsMultisampleTestCase::FragCoordConventionsMultisampleTestCase(deqp::Context &context)
    : TestCase(context, "multisample",
               "Verify that sample positions are not affected when Frag Coord Convention is changed")
    , m_vao(0)
    , m_vbo(0)
    , m_isContextES(false)
    , m_testSupported(false)
{
}

/** Stub deinit method. */
void FragCoordConventionsMultisampleTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void FragCoordConventionsMultisampleTestCase::init()
{
    const glu::RenderContext &renderContext = m_context.getRenderContext();
    glu::GLSLVersion glslVersion            = glu::getContextTypeGLSLVersion(renderContext.getType());
    m_isContextES                           = glu::isContextTypeES(renderContext.getType());

    specializationMap["VERSION"]      = glu::getGLSLVersionDeclaration(glslVersion);
    specializationMap["PRECISION"]    = "";
    specializationMap["COORD_LAYOUT"] = "";
    specializationMap["EXTENSION"]    = "";

    if (m_isContextES)
    {
        specializationMap["PRECISION"] = "precision highp float;";
    }

    auto contextType = m_context.getRenderContext().getType();

    if (m_isContextES)
    {

        if (glu::contextSupports(contextType, glu::ApiType::es(3, 2)))
        {
            m_testSupported = true;
        }
        if (glu::contextSupports(contextType, glu::ApiType::es(3, 1)) &&
            m_context.getContextInfo().isExtensionSupported("GL_OES_sample_variables"))
        {
            specializationMap["EXTENSION"] = "#extension GL_OES_sample_variables : enable\n";
            m_testSupported                = true;
        }
    }
    else
    {
        auto versionGreaterOrEqual = [](ApiType a, ApiType b)
        {
            return a.getMajorVersion() > b.getMajorVersion() ||
                   (a.getMajorVersion() == b.getMajorVersion() && a.getMinorVersion() >= b.getMinorVersion());
        };

        /* This test should only be executed if we're running a GL>=3.0 context */
        if (glu::contextSupports(contextType, glu::ApiType::core(4, 0)))
        {
            m_testSupported = true;
        }
        else if (glu::contextSupports(contextType, glu::ApiType::core(3, 3)))
        {
            specializationMap["EXTENSION"] = R"(
                #extension GL_ARB_sample_shading : enable
                                               )";
            m_testSupported                = true;
        }
        else if (versionGreaterOrEqual(contextType.getAPI(), glu::ApiType::core(3, 0)) &&
                 m_context.getContextInfo().isExtensionSupported("GL_ARB_fragment_coord_conventions"))
        {
            specializationMap["EXTENSION"] = R"(
                #extension GL_ARB_fragment_coord_conventions: require
                #extension GL_ARB_explicit_attrib_location : enable
                #extension GL_ARB_sample_shading : require
                                               )";

            specializationMap["COORD_LAYOUT"] = "layout (origin_upper_left,pixel_center_integer) in vec4 gl_FragCoord;";
            m_testSupported                   = true;
        }
    }

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    auto make_program        = [&](const char *vs, const char *fs)
    {
        /* Building basic program. */
        std::string vert_shader = tcu::StringTemplate(vs).specialize(specializationMap);
        std::string frag_shader = tcu::StringTemplate(fs).specialize(specializationMap);

        ProgramSources sources = makeVtxFragSources(vert_shader, frag_shader);

        auto program = new ShaderProgram(gl, sources);

        if (!program->isOk())
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                               << "Vertex: " << program->getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                               << vert_shader << "\n"
                               << "Fragment: " << program->getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                               << frag_shader << "\n"
                               << "Program: " << program->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
            delete program;
            TCU_FAIL("Invalid program");
        }
        return program;
    };

    auto sources_list = {std::make_pair(default_shader_vert, default_shader_frag),
                         std::make_pair(default_shader_vert, multisample_shader_frag)};

    for (auto src : sources_list)
        m_programs.emplace_back(make_program(src.first, src.second));
}

void FragCoordConventionsMultisampleTestCase::getBufferBits(GLint colorBits[4])
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    getReadbufferBits(gl, m_isContextES, GL_RED_BITS, &colorBits[0]);
    getReadbufferBits(gl, m_isContextES, GL_GREEN_BITS, &colorBits[1]);
    getReadbufferBits(gl, m_isContextES, GL_BLUE_BITS, &colorBits[2]);
    getReadbufferBits(gl, m_isContextES, GL_ALPHA_BITS, &colorBits[3]);
}

GLfloat FragCoordConventionsMultisampleTestCase::calcEpsilon(long bits)
{
    GLfloat e = 0.f;
    if (bits == 0)
    {
        e = m_eps.zero;
    }
    else
    {
        e = (1.0f / (ldexp(1.0f, bits) - 1.0f)) + m_eps.zero;
        if (e > 1.0f)
            e = 1.0f;
    }
    return e;
}

void FragCoordConventionsMultisampleTestCase::initEpsilon()
{
    int i;
    GLint colorBits[4];

    getBufferBits(colorBits);

    m_eps.zero = ldexp(1.f, -13);

    for (i = 0; i < 4; i++)
        m_eps.color[i] = calcEpsilon(std::min(colorBits[i], 8));
}

/*
 * fheckColor
 * Checks the FB color at a specific location against reference color
 *
 * x,y       : location
 * reference : reference color value (RGBA, MSB is A)
 */
bool FragCoordConventionsMultisampleTestCase::checkColor(const int x, const int y, const glw::GLuint reference)
{
    glw::GLuint actual;
    glw::GLuint mask = 0xFFFFFFFF;
    glw::GLuint bits[4];

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    getReadbufferBits(gl, m_isContextES, GL_RED_BITS, (GLint *)&bits[0]);
    getReadbufferBits(gl, m_isContextES, GL_GREEN_BITS, (GLint *)&bits[1]);
    getReadbufferBits(gl, m_isContextES, GL_BLUE_BITS, (GLint *)&bits[2]);
    getReadbufferBits(gl, m_isContextES, GL_ALPHA_BITS, (GLint *)&bits[3]);

    // GL_RGB10_A2 support
    if (bits[0] == 10 && bits[1] == 10 && bits[2] == 10 && bits[3] == 2)
    {
        GLfloat color[4];
        GLuint tempColor;
        GLfloat colorRef[4];

        gl.readPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, &tempColor);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        colorRef[0] = (reference & 0xFF) / 255.0f;
        colorRef[1] = (reference >> 8 & 0xFF) / 255.0f;
        colorRef[2] = (reference >> 16 & 0xFF) / 255.0f;
        colorRef[3] = (reference >> 24 & 0xFF) / 255.0f;

        color[0] = (tempColor & 0x3FF) / 1023.0f;
        color[1] = (tempColor >> 10 & 0x3FF) / 1023.0f;
        color[2] = (tempColor >> 20 & 0x3FF) / 1023.0f;
        color[3] = (tempColor >> 30 & 0x3) / 3.0f;

        if (fabs(color[0] - colorRef[0]) > m_eps.color[0] || fabs(color[1] - colorRef[1]) > m_eps.color[1] ||
            fabs(color[2] - colorRef[2]) > m_eps.color[2] || fabs(color[3] - colorRef[3]) > m_eps.color[3])
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Reference and actual values don't match."
                               << tcu::TestLog::EndMessage;
            return false;
        }
    }
    else
    {
        GLubyte color[4];

        if (bits[0] < 8)
            mask &= ~(((1 << (8 - bits[0])) - 1) << 0);
        if (bits[1] < 8)
            mask &= ~(((1 << (8 - bits[1])) - 1) << 8);
        if (bits[2] < 8)
            mask &= ~(((1 << (8 - bits[2])) - 1) << 16);
        if (bits[3] < 8)
            mask &= ~(((1 << (8 - bits[3])) - 1) << 24);

        color[0] = color[1] = color[2] = color[3] = 0;
        gl.readPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        actual = color[0] + (color[1] << 8) + (color[2] << 16) + (color[3] << 24);

        m_testCtx.getLog() << tcu::TestLog::Message << "Reference color at (" << x << "," << y << "): " << std::hex
                           << reference << ", actual color: " << std::hex << actual << tcu::TestLog::EndMessage;

        if ((actual & mask) != (reference & mask))
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Reference and actual values don't match."
                               << tcu::TestLog::EndMessage;
            return false;
        }
    }
    return true;
}

/*
 * gatherColor
 * Gathers the FB color at a specific location into reference value
 *
 * x,y       : location
 * reference : reference color value to fill
 */
bool FragCoordConventionsMultisampleTestCase::gatherColor(const int x, const int y, glw::GLuint *reference)
{
    glw::GLubyte color[4] = {0, 0, 0, 0};
    int actual;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    color[0] = color[1] = color[2] = color[3] = 0;
    gl.readPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

    actual = color[0] + (color[1] << 8) + (color[2] << 16) + (color[3] << 24);

    m_testCtx.getLog() << tcu::TestLog::Message << "Gather color at (" << x << "," << y << "): " << std::hex << actual
                       << tcu::TestLog::EndMessage;

    *reference = actual;

    return true;
}

/*
 * drawQuad
 * Set up various parameters and draw a quad (or quads)
 *
 * parent : inherited parent argument
 * params : test setup parameters
 * windowWidth, windowHeight : window dimensions
 */
bool FragCoordConventionsMultisampleTestCase::drawQuad(const TestParams &params, int windowWidth, int windowHeight)
{
    GLint attrib;
    GLint locWindowWidth;
    GLint locWindowHeight;
    GLint locN;
    GLint locF;
    const float n = 0.0;
    const float f = 1.0;

    m_testCtx.getLog() << tcu::TestLog::Message << "Case " << params.index << tcu::TestLog::EndMessage;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.disable(GL_SCISSOR_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    // Clear screen
    gl.viewport(0, 0, windowWidth, windowHeight);
    GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

    gl.clearColor(0.6f, 0.4f, 0.6f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    if (params.scissorTest)
    {
        gl.scissor(5, windowHeight - 15, 10, 10);
        GLU_EXPECT_NO_ERROR(gl.getError(), "scissor");

        gl.enable(GL_SCISSOR_TEST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "enable");
    }

    auto shader = m_programs[params.index]->getProgram();

    // use shader by index
    gl.useProgram(m_programs[params.index]->getProgram());
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

    locWindowWidth = gl.getUniformLocation(shader, "windowWidth");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

    m_testCtx.getLog() << tcu::TestLog::Message << "Uniform windowWidth: " << windowWidth << " (loc: " << locWindowWidth
                       << ")" << tcu::TestLog::EndMessage;
    if (locWindowWidth == -1)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "windowWidth wasnt found, may have been optimised away"
                           << tcu::TestLog::EndMessage;
    }
    else
    {
        gl.uniform1f(locWindowWidth, (GLfloat)windowWidth);
        GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");
    }

    locWindowHeight = gl.getUniformLocation(shader, "windowHeight");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

    m_testCtx.getLog() << tcu::TestLog::Message << "Uniform windowHeight: " << windowHeight
                       << " (loc: " << locWindowHeight << ")" << tcu::TestLog::EndMessage;
    if (locWindowHeight == -1)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "windowHeight wasnt found, may have been optimised away"
                           << tcu::TestLog::EndMessage;
    }
    else
    {
        gl.uniform1f(locWindowHeight, (GLfloat)windowHeight);
        GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");
    }

    locN = gl.getUniformLocation(shader, "n");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

    m_testCtx.getLog() << tcu::TestLog::Message << "Uniform n: " << n << " (loc: " << locN << ")"
                       << tcu::TestLog::EndMessage;

    if (locN == -1)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Error while setting uniform n" << tcu::TestLog::EndMessage;
        return false;
    }
    gl.uniform1f(locN, n);
    GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");

    locF = gl.getUniformLocation(shader, "f");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

    m_testCtx.getLog() << tcu::TestLog::Message << "Uniform f: " << f << " (loc: " << locF << ")"
                       << tcu::TestLog::EndMessage;

    if (locF == -1)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Error while setting uniform f" << tcu::TestLog::EndMessage;
        return false;
    }
    gl.uniform1f(locF, f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1f");

    if (m_isContextES)
    {
        gl.depthRangef(n, f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "depthRangef");
    }
    else
    {
        gl.depthRange(n, f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "depthRange");
    }

    // Setup shader attributes
    attrib = gl.getAttribLocation(shader, "pos");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

    m_testCtx.getLog() << tcu::TestLog::Message << "attrib: " << attrib << tcu::TestLog::EndMessage;

    if (attrib == -1)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Error getting attribute location" << tcu::TestLog::EndMessage;
        return false;
    }

    gl.vertexAttribPointer(attrib, 4, GL_FLOAT, GL_FALSE, 0, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    gl.enableVertexAttribArray(attrib);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

    // Draw quad
    gl.disable(GL_DEPTH_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

    if (params.useCull)
    {
        gl.drawArrays(GL_TRIANGLE_STRIP, 4, 4);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");
    }

    return true;
}

/*
 * doQuadCase
 * Performs a test case: set up and destroy buffers (also FBO/RBO if needed)
 * and perform color value comparisons
 *
 * parent : inherited parent argument
 * params : test setup parameters
 */

bool FragCoordConventionsMultisampleTestCase::doQuadCase(const TestParams &params)
{
    GLuint fbo   = 0;
    GLuint fboMS = 0;
    GLuint rbo   = 0;
    GLuint rboMS = 0;
    bool result  = true;

    int windowWidth  = m_context.getRenderTarget().getWidth();
    int windowHeight = m_context.getRenderTarget().getHeight();

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genVertexArrays(1, &m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    // Create buffer
    gl.genBuffers(1, &m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

    gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    if (params.useCull)
    {
        gl.bufferData(GL_ARRAY_BUFFER, sizeof(cullQuad), cullQuad, GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");
    }
    else
    {
        gl.bufferData(GL_ARRAY_BUFFER, sizeof(defaultQuad), defaultQuad, GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");
    }

    // Set up FBO if needed
    if (params.useFBO)
    {
        gl.genFramebuffers(1, &fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
        gl.bindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

        gl.genRenderbuffers(1, &rbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
        gl.bindRenderbuffer(GL_RENDERBUFFER, rbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
        gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, FBO_X, FBO_Y);
        GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

        if (params.useMultisample)
        {
            gl.genFramebuffers(1, &fboMS);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
            gl.bindFramebuffer(GL_FRAMEBUFFER, fboMS);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            gl.genRenderbuffers(1, &rboMS);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
            gl.bindRenderbuffer(GL_RENDERBUFFER, rboMS);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
            gl.renderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, FBO_X, FBO_Y);
            GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorageMultisample");
            gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboMS);
            GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");
            if (!m_isContextES)
            {
                gl.enable(GL_MULTISAMPLE);
                GLU_EXPECT_NO_ERROR(gl.getError(), "enable");
            }
        }

        windowWidth  = FBO_X;
        windowHeight = FBO_Y;
    }

    result = drawQuad(params, windowWidth, windowHeight);
    if (!result)
        return false;

    if (params.useFBO && params.useMultisample)
    {
        gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fboMS);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
        gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
        gl.blitFramebuffer(0, 0, windowWidth, windowHeight, 0, 0, windowWidth, windowHeight, GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "blitFramebuffer");
        gl.bindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    }

    // Read back pixel value and compare against reference
    if (params.scissorTest)
    {
        result &= checkColor(10, windowHeight - 15 - 3, 0xff996699);
        result &= checkColor(10, windowHeight - 15 + 3, refColorsMultisample[params.index][0]);
        result &= checkColor(10, windowHeight - 5 - 3, refColorsMultisample[params.index][1]);
        result &= checkColor(10, windowHeight - 5 + 3, 0xff996699);
        result &= checkColor(3, windowHeight - 10, 0xff996699);
        result &= checkColor(18, windowHeight - 10, 0xff996699);
    }
    else
    {
        int i = params.index;
        if (params.overrideCheckIndex != -1)
            i = params.overrideCheckIndex;

        if (params.gatherSamples)
        {
            result &= gatherColor(OFFSET, OFFSET, &(refColorsMultisample[i][0]));
            result &= gatherColor(windowWidth - OFFSET, OFFSET, &(refColorsMultisample[i][1]));
            result &= gatherColor(OFFSET, windowHeight - OFFSET, &(refColorsMultisample[i][2]));
            result &= gatherColor(windowWidth - OFFSET, windowHeight - OFFSET, &(refColorsMultisample[i][3]));
        }
        else
        {
            result &= checkColor(OFFSET, OFFSET, refColorsMultisample[i][0]);
            result &= checkColor(windowWidth - OFFSET, OFFSET, refColorsMultisample[i][1]);
            result &= checkColor(OFFSET, windowHeight - OFFSET, refColorsMultisample[i][2]);
            result &= checkColor(windowWidth - OFFSET, windowHeight - OFFSET, refColorsMultisample[i][3]);
        }
    }

    // Destroy FBO or swap buffers
    if (params.useFBO)
    {
        gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
        gl.deleteRenderbuffers(1, &rbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");
        gl.deleteFramebuffers(1, &fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

        if (params.useMultisample)
        {
            gl.deleteRenderbuffers(1, &rboMS);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");
            gl.deleteFramebuffers(1, &fboMS);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
        }
    }
    else
    {
        m_context.getRenderContext().postIterate();
    }

    // Delete buffer
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

    return result;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult FragCoordConventionsMultisampleTestCase::iterate()
{
    if (!m_testSupported)
    {
        throw tcu::NotSupportedError("Test frag_coord_conventions.multisample is not supported");
        return STOP;
    }

    bool ret = true;
    TestParams params;
    params.useFBO             = true;
    params.useMultisample     = true;
    params.useCull            = false;
    params.scissorTest        = false;
    params.gatherSamples      = true;
    params.overrideCheckIndex = -1;

    for (size_t s = 0; s < m_programs.size(); s++)
    {
        params.index = s;
        ret &= doQuadCase(params);

        m_testCtx.getLog() << tcu::TestLog::Message << "Case " << s << " result " << ret << "\n----------"
                           << tcu::TestLog::EndMessage;

        // switch to compare mode after first pass
        if (params.gatherSamples)
        {
            params.gatherSamples      = false;
            params.overrideCheckIndex = s;
        }
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
FragCoordConventionsTests::FragCoordConventionsTests(deqp::Context &context)
    : TestCaseGroup(context, "frag_coord_conventions", "Verify fragment coord convention functionality")
{
}

/** Initializes the test group contents. */
void FragCoordConventionsTests::init()
{
    addChild(new FragCoordConventionsMultisampleTestCase(m_context));
}

} // namespace glcts
