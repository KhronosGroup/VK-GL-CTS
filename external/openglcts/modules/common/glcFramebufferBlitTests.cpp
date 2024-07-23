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
 * \file  glcFramebufferBlitTests.cpp
 * \brief Conformance tests for the framebuffer blit functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "glcFramebufferBlitTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluTextureUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"

#include <cmath>

#define CHECK(actual, expected, info)                                                                          \
    {                                                                                                          \
        result &= ((actual) != (expected)) ? false : true;                                                     \
        if ((actual) != (expected))                                                                            \
        {                                                                                                      \
            m_testCtx.getLog() << tcu::TestLog::Message << #info << ": " << __FILE__ << ":" << __LINE__ << ":" \
                               << "expected " << getEnumName((GLenum)(expected)) << "but got "                 \
                               << getEnumName((GLenum)(actual)) << tcu::TestLog::EndMessage;                   \
        }                                                                                                      \
    }

#define CHECK_COLOR(actual, expected, info)                                                             \
    {                                                                                                   \
        result &= ((actual) != (expected)) ? false : true;                                              \
        if ((actual) != (expected))                                                                     \
        {                                                                                               \
            m_testCtx.getLog() << tcu::TestLog::Message << #info << ": " << __FILE__ << ":" << __LINE__ \
                               << tcu::TestLog::EndMessage;                                             \
            TCU_CHECK(result);                                                                          \
        }                                                                                               \
    }

#define CHECK_CONTINUE(actual, expected, info)                                                                 \
    {                                                                                                          \
        result &= ((actual) != (expected)) ? false : true;                                                     \
        if ((actual) != (expected))                                                                            \
        {                                                                                                      \
            m_testCtx.getLog() << tcu::TestLog::Message << #info << ": " << __FILE__ << ":" << __LINE__ << ":" \
                               << "expected " << getEnumName((GLenum)(expected)) << "but got "                 \
                               << getEnumName((GLenum)(actual)) << tcu::TestLog::EndMessage;                   \
            continue;                                                                                          \
        }                                                                                                      \
    }

#define CHECK_RET(actual, expected, info) \
    CHECK((actual), (expected), info);    \
    TCU_CHECK(result);

using namespace glw;
using namespace glu;
using namespace glcts;
using namespace blt;

namespace
{
/* multicolor pattern values */
const tcu::Vec4 RED   = {1.0f, 0.0f, 0.0f, 1.0f};
const tcu::Vec4 GREEN = {0.0f, 1.0f, 0.0f, 1.0f};
const tcu::Vec4 BLUE  = {0.0f, 0.0f, 1.0f, 1.0f};
const tcu::Vec4 WHITE = {1.0f, 1.0f, 1.0f, 1.0f};
const tcu::Vec4 BLACK = {0.0f, 0.0f, 0.0f, 1.0f};
const Depth Q1        = 0.25f;
const Depth Q2        = 0.5f;
const Depth Q3        = 0.75f;
const Depth Q4        = 1.0f;
const Stencil ONE     = 1;
const Stencil TWO     = 2;
const Stencil THREE   = 3;
const Stencil FOUR    = 4;

const GLuint DEFAULT         = 0x12345678;
const GLuint RED_CHANNEL     = 1 << 13;
const GLuint GREEN_CHANNEL   = 1 << 14;
const GLuint BLUE_CHANNEL    = 1 << 15;
const GLuint ALPHA_CHANNEL   = 1 << 16;
const GLuint MAX_BUF_OBJECTS = 256;

char STR_BUF[256];

const Color DST_COLOR   = {0.0f, 0.0f, 0.0f, 1.0f};
const Depth DST_DEPTH   = 0.0f;
const Stencil DST_STCIL = 0;

const char *getEnumName(const GLenum e)
{
    if (glu::getUncompressedTextureFormatName(e) != DE_NULL)
        return glu::getUncompressedTextureFormatName(e);
    else if (glu::getFaceName(e) != DE_NULL)
        return glu::getFaceName(e);
    else if (glu::getFramebufferAttachmentName(e) != DE_NULL)
        return glu::getFramebufferAttachmentName(e);
    else if (glu::getBooleanName((int)e) != DE_NULL)
        return glu::getBooleanName((int)e);
    else if (glu::getFramebufferStatusName(e) != DE_NULL)
        return glu::getFramebufferStatusName(e);
    else if (glu::getInternalFormatTargetName(e) != DE_NULL)
        return glu::getInternalFormatTargetName(e);
    else if (glu::getFramebufferTargetName(e) != DE_NULL)
        return glu::getFramebufferTargetName(e);
    else if (glu::getErrorName(e) != DE_NULL)
        return glu::getErrorName(e);
    else
    {
        switch (e)
        {
        case GL_LEFT:
            return "GL_LEFT";
        case RED_CHANNEL:
            return "RED_CHANNEL";
        case GREEN_CHANNEL:
            return "GREEN_CHANNEL";
        case BLUE_CHANNEL:
            return "BLUE_CHANNEL";
        case RED_CHANNEL | GREEN_CHANNEL:
            return "RG_CHANNELS";
        case RED_CHANNEL | GREEN_CHANNEL | BLUE_CHANNEL:
            return "RGB_CHANNELS";
        case RED_CHANNEL | GREEN_CHANNEL | BLUE_CHANNEL | ALPHA_CHANNEL:
            return "ALL_CHANNELS";
        case DEFAULT:
            return "DEFAULT";
        default:
            break;
        }
        snprintf(STR_BUF, sizeof(STR_BUF) - 1, "0x%04x", e);
        return STR_BUF;
    }
}

} // namespace

namespace glcts
{
// clang-format off

/** @brief Vertex shader source code to test framebuffer blit of color buffers. */
const glw::GLchar* glcts::FramebufferBlitMultiToSingleSampledTestCase::m_default_vert_shader =
    R"(${VERSION}
    ${EXTENSION}
    in vec4 pos;
    in vec2 UV;
    out vec2 vUV;
    void     main()
    {
        gl_Position = pos;
        vUV = UV;
    }
    )";

/** @brief Fragment shader source code to test framebuffer blit of color buffers. */
const glw::GLchar* glcts::FramebufferBlitMultiToSingleSampledTestCase::m_default_frag_shader =
    R"(${VERSION}
    ${PRECISION}
    in vec2 vUV;
    out vec4 color;
    uniform highp sampler2D tex;
    void main()
    {
        color = texture(tex, vUV);
    }
    )";

/** @brief Vertex shader source code to test framebuffer blit of depth buffers. */
const glw::GLchar* glcts::FramebufferBlitMultiToSingleSampledTestCase::m_render_vert_shader =
    R"(${VERSION}
    ${EXTENSION}
    in vec4 pos;
    void main()
    {
        gl_Position = pos;
    }
    )";

/** @brief Fragment shader source code to test framebuffer blit of depth buffers. */
const glw::GLchar* glcts::FramebufferBlitMultiToSingleSampledTestCase::m_render_frag_shader =
    R"(${VERSION}
    ${PRECISION}
    out vec4 color;
    uniform vec4 uColor;
    void main()
    {
        color = uColor;
    }
    )";

// clang-format on

/** Constructor.
 *
 *  @param context     Rendering context
 */
FramebufferBlitMultiToSingleSampledTestCase::FramebufferBlitMultiToSingleSampledTestCase(deqp::Context &context)
    : TestCase(
          context, "framebuffer_blit_functionality_multisampled_to_singlesampled_blit",
          "Confirm that blits from multisampled to single sampled framebuffers of various types are properly resolved.")
    , m_defaultCoord(0, 0)
    , m_fbos{0, 0}
    , m_color_tbos{0, 0}
    , m_depth_tbos{0, 0}
    , m_stcil_tbos{0, 0}
    , m_color_rbos{0, 0}
    , m_depth_rbos{0, 0}
    , m_stcil_rbos{0, 0}
    , m_dflt(0)
    , m_depth_internalFormat(0)
    , m_depth_type(0)
    , m_depth_format(0)
    , m_stcil_internalFormat(0)
    , m_stcil_type(0)
    , m_stcil_format(0)
    , m_cbfTestSupported(false)
    , m_msTbosSupported(false)
    , m_isContextES(false)
    , m_minDrawBuffers(0)
    , m_minColorAttachments(0)
    , m_defaultProg(nullptr)
    , m_renderProg(nullptr)
{
}

/** Stub deinit method. */
void FramebufferBlitMultiToSingleSampledTestCase::deinit()
{
    if (m_renderProg)
    {
        delete m_renderProg;
        m_renderProg = nullptr;
    }

    if (m_defaultProg)
    {
        delete m_defaultProg;
        m_defaultProg = nullptr;
    }
}

/** Stub init method */
void FramebufferBlitMultiToSingleSampledTestCase::init()
{
    const glu::RenderContext &renderContext = m_context.getRenderContext();
    glu::GLSLVersion glslVersion            = glu::getContextTypeGLSLVersion(renderContext.getType());
    m_isContextES                           = glu::isContextTypeES(renderContext.getType());

    specializationMap["VERSION"] = glu::getGLSLVersionDeclaration(glslVersion);
    if (m_isContextES)
    {
        specializationMap["EXTENSION"] = "#extension GL_EXT_clip_cull_distance : enable";
        specializationMap["PRECISION"] = "precision highp float;";
    }
    else
    {
        specializationMap["EXTENSION"] = "";
        specializationMap["PRECISION"] = "";
    }

    auto contextType = m_context.getRenderContext().getType();
    if (m_isContextES)
    {
        m_cbfTestSupported = m_context.getContextInfo().isExtensionSupported("GL_EXT_color_buffer_float") ||
                             glu::contextSupports(contextType, glu::ApiType::es(3, 2));

        m_msTbosSupported     = glu::contextSupports(contextType, glu::ApiType::es(3, 1));
        m_minDrawBuffers      = 4;
        m_minColorAttachments = 4;
    }
    else
    {
        m_cbfTestSupported    = true;
        m_msTbosSupported     = true;
        m_minDrawBuffers      = 8;
        m_minColorAttachments = 8;
    }

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    /* Building programs. */
    auto setup_shaders = [&](const std::string &vert, const std::string &frag)
    {
        std::string vert_shader = tcu::StringTemplate(vert).specialize(specializationMap);
        std::string frag_shader = tcu::StringTemplate(frag).specialize(specializationMap);

        ProgramSources sources = makeVtxFragSources(vert_shader, frag_shader);
        auto prog              = new ShaderProgram(gl, sources);

        if (!prog->isOk())
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                               << "Vertex: " << prog->getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                               << prog->getShader(glu::SHADERTYPE_VERTEX) << "\n"
                               << "Fragment: " << prog->getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                               << prog->getShader(glu::SHADERTYPE_FRAGMENT) << "\n"
                               << "Program: " << prog->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
            TCU_FAIL("FramebufferBlitMultiToSingleSampledTestCase::init: shader build failed");
        };
        return prog;
    };

    m_defaultProg = setup_shaders(m_default_vert_shader, m_default_frag_shader);
    m_renderProg  = setup_shaders(m_render_vert_shader, m_render_frag_shader);

    m_defaultFBO  = m_context.getRenderContext().getDefaultFramebuffer();
    int bufWidth  = m_context.getRenderTarget().getWidth();
    int bufHeight = m_context.getRenderTarget().getHeight();

    m_fullRect     = {0, 0, bufWidth, bufHeight};
    m_defaultCoord = {bufWidth / 2, bufHeight / 2};

    //    /* multicolor pattern rectangles for all quadrants */
    m_setup.ul_rect = {0, bufHeight / 2, bufWidth / 2,
                       bufHeight - bufHeight / 2}; /* upper left  (x, y, width, height) */
    m_setup.ur_rect = {bufWidth / 2, bufHeight / 2, bufWidth - bufWidth / 2,
                       bufHeight - bufHeight / 2};                               /* upper right (x, y, width, height) */
    m_setup.ll_rect = {0, 0, bufWidth / 2, bufHeight / 2};                       /* lower left  (x, y, width, height) */
    m_setup.lr_rect = {bufWidth / 2, 0, bufWidth - bufWidth / 2, bufHeight / 2}; /* lower right (x, y, width, height) */
    m_setup.blt_src_rect        = m_fullRect;
    m_setup.negative_src_width  = false;
    m_setup.negative_src_height = false;
    m_setup.blt_dst_rect        = m_fullRect;
    m_setup.negative_dst_width  = false;
    m_setup.negative_dst_height = false;
    m_setup.scissor_rect        = m_fullRect;
    //    /* corner coordinates */
    m_setup.ul_coord = {0, bufHeight - 1};            /* upper left corner */
    m_setup.ur_coord = {bufWidth - 1, bufHeight - 1}; /* upper right corner */
    m_setup.ll_coord = {0, 0};                        /* lower left corner */
    m_setup.lr_coord = {bufWidth - 1, 0};             /* lower right corner */
    m_setup.ul_color = RED;
    m_setup.ur_color = GREEN;
    m_setup.ll_color = BLUE;
    m_setup.lr_color = WHITE;
    m_setup.ul_depth = Q1;
    m_setup.ur_depth = Q2;
    m_setup.ll_depth = Q3;
    m_setup.lr_depth = Q4;
    m_setup.ul_stcil = ONE;
    m_setup.ur_stcil = TWO;
    m_setup.ll_stcil = THREE;
    m_setup.lr_stcil = FOUR;

    // clang-format off
    /* buffer configs used in functionality tests */
    m_bufferCfg = {
        /* src_fbo    dst_fbo     src_type         dst_type         src_color_buf     src_depth_buf     src_stcil_buf     dst_color_buf     dst_depth_buf     dst_stcil_buf     same_read_and_draw_buffer */
        { &m_fbos[0], &m_fbos[1], GL_TEXTURE_2D,   GL_TEXTURE_2D,   &m_color_tbos[0], &m_depth_tbos[0], &m_stcil_tbos[0], &m_color_tbos[1], &m_depth_tbos[1], &m_stcil_tbos[1], false }, /* texture READ_BUFFER, texture DRAW_BUFFER */
        { &m_fbos[0], &m_fbos[1], GL_RENDERBUFFER, GL_TEXTURE_2D,   &m_color_rbos[0], &m_depth_rbos[0], &m_stcil_rbos[0], &m_color_tbos[1], &m_depth_tbos[1], &m_stcil_tbos[1], false }, /* renderbuffer READ_BUFFER, texture DRAW_BUFFER */
        { &m_fbos[0], &m_fbos[1], GL_TEXTURE_2D,   GL_RENDERBUFFER, &m_color_tbos[0], &m_depth_tbos[0], &m_stcil_tbos[0], &m_color_rbos[1], &m_depth_rbos[1], &m_stcil_rbos[1], false }, /* texture READ_BUFFER, renderbuffer DRAW_BUFFER */
        { &m_fbos[0], &m_fbos[1], GL_RENDERBUFFER, GL_RENDERBUFFER, &m_color_rbos[0], &m_depth_rbos[0], &m_stcil_rbos[0], &m_color_rbos[1], &m_depth_rbos[1], &m_stcil_rbos[1], false }, /* renderbuffer READ_BUFFER, renderbuffer DRAW_BUFFER */
        { &m_dflt,    &m_fbos[1], 0,               GL_TEXTURE_2D,   &m_dflt,          &m_dflt,          &m_dflt,          &m_color_tbos[1], &m_depth_tbos[1], &m_stcil_tbos[1], false }, /* default READ_BUFFER, texture DRAW_BUFFER */
        { &m_dflt,    &m_fbos[1], 0,               GL_RENDERBUFFER, &m_dflt,          &m_dflt,          &m_dflt,          &m_color_rbos[1], &m_depth_rbos[1], &m_stcil_rbos[1], false }, /* default READ_BUFFER, renderbuffer DRAW_BUFFER */
        { &m_fbos[0], &m_dflt,    GL_TEXTURE_2D,   0,               &m_color_tbos[0], &m_depth_tbos[0], &m_stcil_tbos[0], &m_dflt,          &m_dflt,          &m_dflt,          false }, /* texture READ_BUFFER, default DRAW_BUFFER */
        { &m_fbos[0], &m_dflt,    GL_RENDERBUFFER, 0,               &m_color_rbos[0], &m_depth_rbos[0], &m_stcil_rbos[0], &m_dflt,          &m_dflt,          &m_dflt,          false }  /* renderbuffer READ_BUFFER, default DRAW_BUFFER */
    };

    if (!m_isContextES)
    {
        m_bufferCfg.push_back({ &m_fbos[0], &m_fbos[1], GL_TEXTURE_2D, GL_TEXTURE_2D, &m_color_tbos[0], &m_depth_tbos[0],
                                &m_stcil_tbos[0], &m_color_tbos[1], &m_depth_tbos[1], &m_stcil_tbos[1],
                                true }); /* same texture in READ_BUFFER and DRAW_BUFFER */
        m_bufferCfg.push_back({ &m_fbos[0], &m_fbos[1], GL_RENDERBUFFER, GL_RENDERBUFFER, &m_color_rbos[0], &m_depth_rbos[0],
                                &m_stcil_rbos[0], &m_color_rbos[1], &m_depth_rbos[1], &m_stcil_rbos[1],
                                true }); /* same renderbuffer in READ_BUFFER and DRAW_BUFFER */
        m_bufferCfg.push_back({ &m_dflt, &m_dflt, 0, 0, &m_dflt, &m_dflt, &m_dflt, &m_dflt, &m_dflt, &m_dflt,
                                true }); /* default READ_BUFFER and DRAW_BUFFER */
    }

    m_multisampleColorCfg = {
        /* internal format, format, type, color channel bits */
        { GL_R8, GL_RED, GL_UNSIGNED_BYTE, RED_CHANNEL, false },
        { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, RED_CHANNEL|GREEN_CHANNEL, false },
        { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, RED_CHANNEL|GREEN_CHANNEL|BLUE_CHANNEL|ALPHA_CHANNEL, false },
        { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, RED_CHANNEL|GREEN_CHANNEL|BLUE_CHANNEL|ALPHA_CHANNEL, false },
        { GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, RED_CHANNEL|GREEN_CHANNEL|BLUE_CHANNEL|ALPHA_CHANNEL, false },
        { GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, RED_CHANNEL|GREEN_CHANNEL|BLUE_CHANNEL|ALPHA_CHANNEL, false },
        { GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, RED_CHANNEL|GREEN_CHANNEL|BLUE_CHANNEL, true },
        { GL_RG16F, GL_RG, GL_HALF_FLOAT, RED_CHANNEL|GREEN_CHANNEL, true },
        { GL_R16F, GL_RED, GL_HALF_FLOAT, RED_CHANNEL, true },
        { GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, RED_CHANNEL|GREEN_CHANNEL|BLUE_CHANNEL, false }, /* Texture only format */
    };

    if (!m_isContextES)
    {
        m_multisampleColorCfg.push_back({ GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2,
                                          RED_CHANNEL | GREEN_CHANNEL | BLUE_CHANNEL | ALPHA_CHANNEL, false });
    }
    else
    {
        m_multisampleColorCfg.push_back({ GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV,
                                          RED_CHANNEL | GREEN_CHANNEL | BLUE_CHANNEL | ALPHA_CHANNEL, false });
        m_multisampleColorCfg.push_back(
            { GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, RED_CHANNEL | GREEN_CHANNEL | BLUE_CHANNEL, false });
    }

    m_depthCfg = {
        /* From table 3.13 */
        /* internal format, format, type, attachment, depth bits */
        { GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_DEPTH_ATTACHMENT, 24 },
        { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_ATTACHMENT, 16 },
        { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, GL_DEPTH_ATTACHMENT, 32 },
        { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL_ATTACHMENT, 24 },
        { GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, GL_DEPTH_STENCIL_ATTACHMENT, 32 },
    };

    if (!m_isContextES)
    {
        m_depthCfg.push_back({ GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_DEPTH_ATTACHMENT, 32 });
    }
    // clang-format on
}

bool FramebufferBlitMultiToSingleSampledTestCase::GetBits(GLenum target, GLenum bits, GLint *value)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    if (!m_isContextES)
    {
        GLint colorAttachment    = 0;
        GLenum depthAttachment   = GL_DEPTH;
        GLenum stencilAttachment = GL_STENCIL;
        GLint fbo                = 0;
        if (target == GL_READ_FRAMEBUFFER)
        {
            gl.getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        }
        else
        {
            gl.getIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        }

        if (fbo)
        {
            depthAttachment   = GL_DEPTH_ATTACHMENT;
            stencilAttachment = GL_STENCIL_ATTACHMENT;
        }
        if (target == GL_READ_FRAMEBUFFER)
        {
            gl.getIntegerv(GL_READ_BUFFER, &colorAttachment);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        }
        else
        {
            gl.getIntegerv(GL_DRAW_BUFFER, &colorAttachment);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        }

        if (colorAttachment == GL_BACK)
            colorAttachment = GL_BACK_LEFT;
        else if (colorAttachment == GL_FRONT)
            colorAttachment = GL_FRONT_LEFT;

        switch (bits)
        {
        case GL_RED_BITS:
            gl.getFramebufferAttachmentParameteriv(target, colorAttachment, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
            break;
        case GL_GREEN_BITS:
            gl.getFramebufferAttachmentParameteriv(target, colorAttachment, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                                   value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
            break;
        case GL_BLUE_BITS:
            gl.getFramebufferAttachmentParameteriv(target, colorAttachment, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
            break;
        case GL_ALPHA_BITS:
            gl.getFramebufferAttachmentParameteriv(target, colorAttachment, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
                                                   value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
            break;
        case GL_DEPTH_BITS:
        case GL_STENCIL_BITS:
            /*
             * OPENGL SPECS 4.5: Paragraph  9.2. BINDING AND MANAGING FRAMEBUFFER OBJECTS p.335
             * If the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE, then either no framebuffer is bound to target;
             * or a default framebuffer is queried, attachment is GL_DEPTH or GL_STENCIL,
             * and the number of depth or stencil bits, respectively, is zero....
             * and all other queries will generate an INVALID_OPERATION error.
             * */
            if (fbo == 0)
            { //default framebuffer
                gl.getFramebufferAttachmentParameteriv(target, (bits == GL_DEPTH_BITS ? GL_DEPTH : GL_STENCIL),
                                                       GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, value);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                if (*value == GL_NONE)
                {
                    *value = 0;
                    break;
                }
            }
            switch (bits)
            {
            case GL_DEPTH_BITS:
                gl.getFramebufferAttachmentParameteriv(target, depthAttachment, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
                                                       value);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                break;
            case GL_STENCIL_BITS:
                gl.getFramebufferAttachmentParameteriv(target, stencilAttachment,
                                                       GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, value);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                break;
            }
            break;
        default:
            gl.getIntegerv(bits, value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
            break;
        }
    }
    else
    {
        gl.getIntegerv(bits, value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
    }
    return true;
}

bool FramebufferBlitMultiToSingleSampledTestCase::GetDrawbuffer32DepthComponentType(glw::GLint *value)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    if (!m_isContextES)
    {
        GLenum target          = GL_DRAW_FRAMEBUFFER;
        GLenum depthAttachment = GL_DEPTH;
        GLint fbo              = 0;
        gl.getIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

        if (fbo)
        {
            depthAttachment = GL_DEPTH_ATTACHMENT;
        }

        /*
         * OPENGL SPECS 4.5: Paragraph  9.2. BINDING AND MANAGING FRAMEBUFFER OBJECTS p.335
         * If the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE, then either no framebuffer is bound to target;
         * or a default framebuffer is queried, attachment is GL_DEPTH or GL_STENCIL,
         * and the number of depth or stencil bits, respectively, is zero....
         * and all other queries will generate an INVALID_OPERATION error.
         * */
        if (fbo == 0)
        { //default framebuffer
            gl.getFramebufferAttachmentParameteriv(target, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");

            if (*value == GL_NONE)
            {
                *value = GL_FLOAT;
                return false;
            }
        }
        gl.getFramebufferAttachmentParameteriv(target, depthAttachment, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,
                                               value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
    }
    else
    {
        *value = GL_FLOAT;
    }

    return true;
}

/* Get default frame buffer's compatible bliting format */
bool FramebufferBlitMultiToSingleSampledTestCase::GetDefaultFramebufferBlitFormat(bool *noDepth, bool *noStencil)
{
    int depthBits = 0, stencilBits = 0;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.bindFramebuffer(GL_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    GetBits(GL_DRAW_FRAMEBUFFER, GL_DEPTH_BITS, &depthBits);
    GetBits(GL_DRAW_FRAMEBUFFER, GL_STENCIL_BITS, &stencilBits);

    m_depth_internalFormat = 0;
    m_depth_type           = 0;
    m_depth_format         = 0;

    m_stcil_internalFormat = 0;
    m_stcil_type           = 0;
    m_stcil_format         = 0;

    *noDepth   = (depthBits == 0);
    *noStencil = (stencilBits == 0);

    /* Check if running under FBO config */
    if (m_defaultFBO != 0)
    {
        m_stcil_internalFormat = m_depth_internalFormat = GL_DEPTH24_STENCIL8;
        m_stcil_type = m_depth_type = GL_UNSIGNED_INT_24_8;
        m_stcil_format = m_depth_format = GL_DEPTH_STENCIL;

        return true;
    }

    if (depthBits == 16)
    {
        if (stencilBits == 0)
        {
            m_depth_internalFormat = GL_DEPTH_COMPONENT16;
            m_depth_type           = GL_UNSIGNED_SHORT;
            m_depth_format         = GL_DEPTH_COMPONENT;

            return true;
        }
    }
    else if (depthBits == 24)
    {
        if (stencilBits == 0)
        {
            m_depth_internalFormat = GL_DEPTH_COMPONENT24;
            m_depth_type           = GL_UNSIGNED_INT;
            m_depth_format         = GL_DEPTH_COMPONENT;

            return true;
        }
        else if (stencilBits == 8)
        {
            m_stcil_internalFormat = m_depth_internalFormat = GL_DEPTH24_STENCIL8;
            m_stcil_type = m_depth_type = GL_UNSIGNED_INT_24_8;
            m_stcil_format = m_depth_format = GL_DEPTH_STENCIL;

            return true;
        }
    }
    else if (depthBits == 32)
    {
        if (stencilBits == 0)
        {
            GLint type = 0;
            GetDrawbuffer32DepthComponentType(&type);
            if (type == GL_FLOAT)
            {
                m_depth_internalFormat = GL_DEPTH_COMPONENT32F;
                m_depth_type           = GL_FLOAT;
            }
            else
            {
                m_depth_internalFormat = GL_DEPTH_COMPONENT32;
                m_depth_type           = GL_UNSIGNED_INT;
            }
            m_depth_format = GL_DEPTH_COMPONENT;

            return true;
        }
        else if (stencilBits == 8)
        {
            m_stcil_internalFormat = m_depth_internalFormat = GL_DEPTH32F_STENCIL8;
            m_stcil_type = m_depth_type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
            m_stcil_format = m_depth_format = GL_DEPTH_STENCIL;

            return true;
        }
    }

    return false;
}

bool FramebufferBlitMultiToSingleSampledTestCase::check_param(glw::GLboolean expr, const char *str)
{
    if (!expr)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << ":" << __FILE__ << ":" << __LINE__ << str
                           << tcu::TestLog::EndMessage;
        return false;
    }
    return true;
}

/* Convert float [0,1] to byte [0,255].
 */
GLubyte FramebufferBlitMultiToSingleSampledTestCase::floatToByte(GLfloat f)
{
    if (f < 0.0f || f > 1.0f)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << ":" << __FILE__ << ":" << __LINE__
                           << "float not in range [0.0f, 1.0f]" << tcu::TestLog::EndMessage;
        return 0;
    }
    return (GLubyte)std::floor(f == 1.0f ? 255 : f * 255.0);
}

///* Initialize textures or renderbuffers. Return true if succeed, false otherwise.
// *
// * count: number of textures to init
// * buf: pointer to tex objects
// * internal_format: tex internal format
// */
template <GLenum E, GLuint samples, typename F>
bool FramebufferBlitMultiToSingleSampledTestCase::init_gl_objs(F f, const GLuint count, const GLuint *buf,
                                                               const GLint format)
{
    if (!check_param(buf != NULL, "invalid buf pointer"))
        return false;
    if (!check_param(count < MAX_BUF_OBJECTS, "invalid count"))
        return false;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    for (GLuint i = 0; i < count; i++)
    {
        f(E, buf[i]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");
        if (E == GL_TEXTURE_2D)
        {
            gl.texStorage2D(E, 1, format, m_fullRect.w, m_fullRect.h);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2D");
        }
        else if (E == GL_TEXTURE_2D_MULTISAMPLE)
        {
            gl.texStorage2DMultisample(E, samples, format, m_fullRect.w, m_fullRect.h, GL_TRUE);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2DMultisample");
        }
        else if (E == GL_RENDERBUFFER)
        {
            if (samples == 0)
            {
                gl.renderbufferStorage(E, format, m_fullRect.w, m_fullRect.h);
                GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
            }
            else
            {
                gl.renderbufferStorageMultisample(E, samples, format, m_fullRect.w, m_fullRect.h);
                GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorageMultisample");
            }
        }
    }
    return true;
}

/* Attach a buffer object to framebuffer. Return true if succeed,
 * false otherwise.
 *
 * target: fbo target
 * attachment: color/depth/stencil attachment
 * type: buf type
 * buf: buf to attach
 */
bool FramebufferBlitMultiToSingleSampledTestCase::attachBufferToFramebuffer(GLenum target, GLenum attachment,
                                                                            GLenum type, GLuint buf)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    if (type == GL_TEXTURE_2D || type == GL_TEXTURE_2D_MULTISAMPLE)
    {
        gl.framebufferTexture2D(target, attachment, type, buf, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");

        if (buf)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "attaching texbuf" << buf << " to "
                               << getEnumName(attachment) << " of " << getEnumName(target) << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "detaching " << getEnumName(attachment) << " of "
                               << getEnumName(target) << tcu::TestLog::EndMessage;
        }
    }
    else if (type == GL_RENDERBUFFER)
    {
        gl.framebufferRenderbuffer(target, attachment, type, buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

        if (buf)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "attaching renbuf" << buf << " to "
                               << getEnumName(attachment) << " of " << getEnumName(target) << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "detaching " << getEnumName(attachment) << " of "
                               << getEnumName(target) << tcu::TestLog::EndMessage;
        }
    }
    return true;
}

/* Get the color value from the given coordinates. Return true if
 * succeed, false otherwise.
 */
bool FramebufferBlitMultiToSingleSampledTestCase::getColor(const Coord &coord, Color *color,
                                                           const bool bFloatInternalFormat)
{
    bool result = true;

    GLuint status;
    GLint x = coord.x();
    GLint y = coord.y();

    if (!check_param(color != NULL, "invalid color pointer"))
        return false;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    status = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
    if (status != GL_FRAMEBUFFER_COMPLETE)
        m_testCtx.getLog() << tcu::TestLog::Message << "checkFramebufferStatus unexpected status"
                           << tcu::TestLog::EndMessage;

    if (bFloatInternalFormat)
    {
        GLfloat tmp_fcolor[4] = {0.6f, 0.6f, 0.6f, 0.6f};
        gl.readPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, &tmp_fcolor[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        color->x() = tmp_fcolor[0];
        color->y() = tmp_fcolor[1];
        color->z() = tmp_fcolor[2];
        color->w() = tmp_fcolor[3];
    }
    else
    {
        GLubyte tmp_color[4] = {100, 100, 100, 100};
        gl.readPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &tmp_color[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        color->x() = tmp_color[0] / 255.0f;
        color->y() = tmp_color[1] / 255.0f;
        color->z() = tmp_color[2] / 255.0f;
        color->w() = tmp_color[3] / 255.0f;
    }

    m_testCtx.getLog() << tcu::TestLog::Message << "getColor: XY=[" << x << "," << y << "] RGBA=[" << color->x() << ","
                       << color->y() << "," << color->z() << "," << color->w() << "]" << tcu::TestLog::EndMessage;

    return result;
}

/* Verify the actual color and the expected color match in given
 * channels. Return true if succeed, false otherwise.
 *
 * actual: actual color to be checked
 * expect: expected color
 * channels: bitfield combination of RED_CHANNEL, GREEN_CHANNEL,
 *           BLUE_CHANNEL, and ALPHA_CHANNEL
 */
bool FramebufferBlitMultiToSingleSampledTestCase::checkColor(const Color &actual, const Color &expect,
                                                             const GLuint channels)
{
    GLubyte expect_r = 0, expect_g = 0, expect_b = 0, expect_a = 0;
    GLubyte actual_r = 0, actual_g = 0, actual_b = 0, actual_a = 0;

    if (channels & RED_CHANNEL)
    {
        expect_r = floatToByte(expect.x());
        actual_r = floatToByte(actual.x());
    }
    if (channels & GREEN_CHANNEL)
    {
        expect_g = floatToByte(expect.y());
        actual_g = floatToByte(actual.y());
    }
    if (channels & BLUE_CHANNEL)
    {
        expect_b = floatToByte(expect.z());
        actual_b = floatToByte(actual.z());
    }
    if (channels & ALPHA_CHANNEL)
    {
        expect_a = floatToByte(expect.w());
        actual_a = floatToByte(actual.w());
    }

    if (actual_r != expect_r || actual_g != expect_g || actual_b != expect_b || actual_a != expect_a)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "ERROR: expected  RGBA=[" << expect_r << "," << expect_g << ","
                           << expect_b << "," << expect_a << "] but got RGBA[" << actual_r << "," << actual_g << ","
                           << actual_b << "," << actual_a << "]" << tcu::TestLog::EndMessage;
        return false;
    }
    return true;
}

/* Clear the color buffer to given color. Prior to return, unbind all
 * used attachments and setup default read and draw framebuffers.
 * Return true if succeed, false otherwise.
 *
 * fbo: framebuffer to use
 * attachment: framebuffer attachment to attach buffer
 * type: buffer type
 * buf: colorbuffer to be cleared
 * color: clear color
 * rect: region of colorbuffer to be cleared
 * check_coord: coord to verify buffer value after clearing
 * check_channels: channels to be verified
 */
bool FramebufferBlitMultiToSingleSampledTestCase::clearColorBuffer(
    const GLuint fbo, const GLenum attachment, const GLenum type, const GLuint buf, const Color &color,
    const Rectangle &rect, const Coord &check_coord, const GLuint check_channels, const bool bFloatInternalFormat)
{
    bool result          = true;
    Color tmp_color      = {0.5f, 0.5f, 0.5f, 0.5f};
    GLint sample_buffers = 0;

    if (!check_param(
            (type == 0 || type == GL_TEXTURE_2D || type == GL_TEXTURE_2D_MULTISAMPLE || type == GL_RENDERBUFFER),
            "invalid type"))
        return false;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    if (fbo && (fbo != m_defaultFBO))
    {
        result &= attachBufferToFramebuffer(GL_DRAW_FRAMEBUFFER, attachment, type, buf);
        result &= attachBufferToFramebuffer(GL_READ_FRAMEBUFFER, attachment, type, buf);
        gl.readBuffer(attachment);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readBuffer");

        gl.drawBuffers(1, &attachment);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawBuffers");
    }

    // clear color rectangle
    gl.scissor(rect.x, rect.y, rect.w, rect.h);
    GLU_EXPECT_NO_ERROR(gl.getError(), "scissor");

    gl.enable(GL_SCISSOR_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    GLuint status = gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
    if (status != GL_FRAMEBUFFER_COMPLETE)
        m_testCtx.getLog() << tcu::TestLog::Message << "checkFramebufferStatus unexpected status"
                           << tcu::TestLog::EndMessage;

    gl.clearColor(color[0], color[1], color[2], color[3]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

    m_testCtx.getLog() << tcu::TestLog::Message << "clearing color to [" << color[0] << "," << color[1] << ","
                       << color[2] << "," << color[3] << "]" << tcu::TestLog::EndMessage;

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    gl.disable(GL_SCISSOR_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    /* Verify the color in cleared buffer in case of single-sampled
     * buffers. Don't verify in case of multisampled buffer since
     * glReadPixels generates GL_INVALID_OPERATION if
     * GL_SAMPLE_BUFFERS is greater than zero. */
    gl.getIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    if (sample_buffers == 0)
    {
        if (fbo && (fbo != m_defaultFBO))
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "verifying initial "
                               << ((type == GL_RENDERBUFFER) ? "ren" : "tex") << "buf" << buf << " color [" << color[0]
                               << "," << color[1] << "," << color[2] << "," << color[3] << "]"
                               << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "verifying initial default buf color [" << color[0] << ","
                               << color[1] << "," << color[2] << "," << color[3] << "]" << tcu::TestLog::EndMessage;
        }

        getColor(check_coord, &tmp_color, bFloatInternalFormat);
        bool ret = checkColor(tmp_color, color, check_channels);
        CHECK(ret, true, checkColor);
    }
    else
    {
        if (fbo && (fbo != m_defaultFBO))
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "no verification of multisampled "
                               << ((type == GL_RENDERBUFFER) ? "ren" : "tex") << "buf" << buf
                               << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "no verification of multisampled dfltbuf"
                               << tcu::TestLog::EndMessage;
        }
    }

    if (fbo && (fbo != m_defaultFBO))
    {
        result &= attachBufferToFramebuffer(GL_DRAW_FRAMEBUFFER, attachment, type, 0);
        result &= attachBufferToFramebuffer(GL_READ_FRAMEBUFFER, attachment, type, 0);
    }

    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    return result;
}

bool FramebufferBlitMultiToSingleSampledTestCase::setupDefaultShader(GLuint &vao, GLuint &vbo)
{
    bool result = true;

    // clang-format off
    const std::vector <GLfloat> vboData =
    {
        -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    };
    // clang-format on

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genVertexArrays(1, &vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    gl.genBuffers(1, &vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    gl.bufferData(GL_ARRAY_BUFFER, vboData.size() * sizeof(GLfloat), (GLvoid *)vboData.data(), GL_DYNAMIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    gl.useProgram(m_defaultProg->getProgram());
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

    // Setup shader attributes
    GLint attribPos = gl.getAttribLocation(m_defaultProg->getProgram(), "pos");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");
    CHECK_RET((attribPos != -1), true, getAttribLocation);

    GLint attribUV = gl.getAttribLocation(m_defaultProg->getProgram(), "UV");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");
    CHECK_RET((attribUV != -1), true, getAttribLocation);

    const GLsizei vertSize = (vboData.size() / 4) * sizeof(GLfloat);
    const GLsizei uvOffset = (4 * sizeof(GLfloat));

    gl.vertexAttribPointer(attribPos, 4, GL_FLOAT, GL_FALSE, vertSize, DE_NULL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    gl.enableVertexAttribArray(attribPos);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

    gl.vertexAttribPointer(attribUV, 2, GL_FLOAT, GL_FALSE, vertSize, (const GLvoid *)uvOffset);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    gl.enableVertexAttribArray(attribUV);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

    // Setup shader uniform
    GLint uniformTex = gl.getUniformLocation(m_defaultProg->getProgram(), "tex");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
    CHECK_RET((uniformTex != -1), true, getUniformLocation);
    gl.uniform1i(uniformTex, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");

    return result;
}

/* Get the depth value from the given coordinates. Return true if
 * succeed, false otherwise.
 */
bool FramebufferBlitMultiToSingleSampledTestCase::getDepth(const Coord &coord, Depth *depth, GLuint *precisionBits,
                                                           const GLuint fbo, const GLuint internalFormat,
                                                           const Rectangle &rect)
{
    bool result   = true;
    GLuint status = 0;
    GLint x = coord.x(), y = coord.y();

    if (!check_param(depth != NULL, "invalid depth pointer"))
        return false;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    if (m_isContextES)
    {
        GLuint fbo_0, tex_0 = 0, tex_1;
        const GLenum attachment_0 = GL_COLOR_ATTACHMENT0;
        GLubyte dataColor[4];

        {
            /* Blit to a depth texture for later sampling in the shader to get the depth value */
            const auto psInternalFormat = glu::getTransferFormat(glu::mapGLInternalFormat(internalFormat));

            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            gl.genFramebuffers(1, &fbo_0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
            gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
            gl.genTextures(1, &tex_0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");
            gl.bindTexture(GL_TEXTURE_2D, tex_0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");
            gl.texImage2D(GL_TEXTURE_2D, 0, internalFormat, rect.w, rect.h, 0, psInternalFormat.format,
                          psInternalFormat.dataType, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
            gl.framebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex_0, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");

            status = gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
            CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);
            status = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
            GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
            CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);

            gl.blitFramebuffer(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, 0, 0, rect.w, rect.h,
                               GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            GLU_EXPECT_NO_ERROR(gl.getError(), "blitFramebuffer");

            gl.deleteFramebuffers(1, &fbo_0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
        }

        gl.genFramebuffers(1, &fbo_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
        gl.bindFramebuffer(GL_FRAMEBUFFER, fbo_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

        gl.genTextures(1, &tex_1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");
        gl.bindTexture(GL_TEXTURE_2D, tex_1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");
        gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rect.w, rect.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
        gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_1, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");
        gl.drawBuffers(1, &attachment_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawBuffers");
        gl.readBuffer(attachment_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readBuffer");
        status = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
        GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
        CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);

        GLuint vao = 0, vbo = 0;
        if (!setupDefaultShader(vao, vbo))
            return false;

        gl.activeTexture(GL_TEXTURE0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");
        gl.bindTexture(GL_TEXTURE_2D, tex_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");
        gl.disable(GL_DEPTH_TEST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
        gl.depthMask(GL_FALSE);
        GLU_EXPECT_NO_ERROR(gl.getError(), "depthMask");
        gl.disable(GL_STENCIL_TEST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
        gl.viewport(0, 0, rect.w, rect.h);
        GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");
        gl.clearColor(0.8f, 0.8f, 0.8f, 0.8f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");
        gl.clear(GL_COLOR_BUFFER_BIT);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clear");
        gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

        gl.readPixels(x - rect.x, y - rect.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, dataColor);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        gl.deleteFramebuffers(1, &fbo_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
        gl.deleteTextures(1, &tex_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
        gl.deleteTextures(1, &tex_1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

        gl.depthMask(GL_TRUE);
        GLU_EXPECT_NO_ERROR(gl.getError(), "depthMask");
        // restore viewport
        gl.viewport(m_fullRect.x, m_fullRect.y, m_fullRect.w, m_fullRect.h);
        GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

        gl.disableVertexAttribArray(0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");
        gl.disableVertexAttribArray(1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

        if (vbo)
        {
            gl.deleteBuffers(1, &vbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
        }

        if (vao)
        {
            gl.deleteVertexArrays(1, &vao);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");
        }

        *depth         = dataColor[0] / 255.0f;
        *precisionBits = 8;

        gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    }
    else
    {
        GLfloat tmp_depth = 0.2f;
        status            = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
        GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
        CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);
        gl.readPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &tmp_depth);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        *depth         = tmp_depth;
        *precisionBits = 24;
    }

    m_testCtx.getLog() << tcu::TestLog::Message << "getDepth: XY[" << x << "," << y << "] DEPTH_COMPONENT[" << *depth
                       << "]" << tcu::TestLog::EndMessage;

    return result;
}

bool FramebufferBlitMultiToSingleSampledTestCase::setupRenderShader(GLuint &vao, GLuint &vbo, GLint *uColor)
{
    bool result = true;

    // clang-format off
    const std::vector<GLfloat> vboData = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
    };
    // clang-format on

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genVertexArrays(1, &vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    gl.genBuffers(1, &vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    gl.bufferData(GL_ARRAY_BUFFER, vboData.size() * sizeof(GLfloat), (GLvoid *)vboData.data(), GL_DYNAMIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    // setup shader
    gl.useProgram(m_renderProg->getProgram());
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

    GLint attribPos = gl.getAttribLocation(m_renderProg->getProgram(), "pos");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");
    CHECK_RET((attribPos != -1), true, getAttribLocation);

    gl.vertexAttribPointer(attribPos, 4, GL_FLOAT, GL_FALSE, 0, DE_NULL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");

    // Setup shader attributes
    gl.enableVertexAttribArray(attribPos);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

    // Setup shader uniform
    *uColor = gl.getUniformLocation(m_renderProg->getProgram(), "uColor");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
    CHECK_RET((*uColor != -1), true, getUniformLocation);
    gl.uniform4f(*uColor, 1.0f, 1.0f, 1.0f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "uniform4f");

    return result;
}

/* Get the stencil value from the given coordinates. Return true if
 * succeed, false otherwise.
 */
bool FramebufferBlitMultiToSingleSampledTestCase::getStencil(const Coord &coord, Stencil *stcil, const GLuint fbo,
                                                             const GLuint internalFormat, const Rectangle &rect)
{
    bool result   = true;
    GLuint status = 0;
    GLint x = coord.x(), y = coord.y();

    if (!check_param(stcil != NULL, "invalid stcil pointer"))
        return false;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    if (m_isContextES)
    {
        GLuint fbo_0 = 0, stencil_buf = 0, tex_0 = 0;
        GLint uColor              = 0;
        const GLenum attachment_0 = GL_COLOR_ATTACHMENT0;
        GLubyte dataColor[4]      = {50, 50, 50, 50};

        {
            /* Blit to a stencil renderbuffer anyway to prevent buf is multisampled and to use this whole stencil renderbuffer for rendering */
            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            gl.genFramebuffers(1, &fbo_0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
            gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            gl.genRenderbuffers(1, &stencil_buf);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
            gl.bindRenderbuffer(GL_RENDERBUFFER, stencil_buf);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
            gl.renderbufferStorage(GL_RENDERBUFFER, internalFormat, rect.w, rect.h);
            GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
            gl.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil_buf);
            GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

            status = gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
            CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);
            status = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
            GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
            CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);

            gl.blitFramebuffer(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, 0, 0, rect.w, rect.h,
                               GL_STENCIL_BUFFER_BIT, GL_NEAREST);
            GLU_EXPECT_NO_ERROR(gl.getError(), "blitFramebuffer");

            gl.deleteFramebuffers(1, &fbo_0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
        }

        gl.genFramebuffers(1, &fbo_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
        gl.bindFramebuffer(GL_FRAMEBUFFER, fbo_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

        gl.genTextures(1, &tex_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");
        gl.bindTexture(GL_TEXTURE_2D, tex_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");
        gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rect.w, rect.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
        gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_0, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");
        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil_buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

        gl.drawBuffers(1, &attachment_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawBuffers");
        gl.readBuffer(attachment_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readBuffer");
        status = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
        GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");

        CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);

        GLuint vao = 0, vbo = 0;
        if (!setupRenderShader(vao, vbo, &uColor))
            return false;

        gl.viewport(0, 0, rect.w, rect.h);
        GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");
        gl.clearColor(0.8f, 0.8f, 0.8f, 0.8f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");
        gl.clear(GL_COLOR_BUFFER_BIT);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

        gl.enable(GL_STENCIL_TEST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "enable");
        gl.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        GLU_EXPECT_NO_ERROR(gl.getError(), "stencilOp");
        for (GLuint i = 0; i < 256; i++)
        {
            float v = i / 255.0f;
            gl.uniform4f(uColor, v, v, v, 1.0f);
            GLU_EXPECT_NO_ERROR(gl.getError(), "uniform4f");
            gl.stencilFunc(GL_EQUAL, i, 0xFF);
            GLU_EXPECT_NO_ERROR(gl.getError(), "stencilFunc");
            gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
            GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");
        }

        gl.disable(GL_STENCIL_TEST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
        gl.readPixels(x - rect.x, y - rect.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, dataColor);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        // restore viewport
        gl.viewport(m_fullRect.x, m_fullRect.y, m_fullRect.w, m_fullRect.h);
        GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

        gl.deleteFramebuffers(1, &fbo_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
        gl.deleteRenderbuffers(1, &stencil_buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");
        gl.deleteTextures(1, &tex_0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

        gl.disableVertexAttribArray(0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

        if (vbo)
        {
            gl.deleteBuffers(1, &vbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
        }

        if (vao)
        {
            gl.deleteVertexArrays(1, &vao);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteVertexArrays");
        }

        *stcil = dataColor[0];

        gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    }
    else
    {
        GLuint tmp_stcil = 50;

        status = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
        GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
        CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);
        gl.readPixels(x, y, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_INT, &tmp_stcil);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        *stcil = tmp_stcil;
    }

    m_testCtx.getLog() << tcu::TestLog::Message << "getStencil: XY[" << x << "," << y << "] STENCIL_INDEX[" << *stcil
                       << "]" << tcu::TestLog::EndMessage;

    return result;
}

/* Get depth precision bit from a depth internal format
 */
GLuint FramebufferBlitMultiToSingleSampledTestCase::GetDepthPrecisionBits(const GLenum depthInternalFormat)
{
    for (GLuint i = 0; i < m_depthCfg.size(); ++i)
        if (m_depthCfg[i].internal_format == depthInternalFormat)
            return m_depthCfg[i].precisionBits;
    return 0;
}

/* Verify the actual and the expected depth match. Return true if
 * succeed, false otherwise.
 */
bool FramebufferBlitMultiToSingleSampledTestCase::checkDepth(const Depth actual, const Depth expected,
                                                             const GLfloat eps)
{
    if (std::fabs(actual - expected) > eps)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "ERROR: expected DEPTH[" << expected << "] but got DEPTH["
                           << actual << "], epsilon[" << eps << "]" << tcu::TestLog::EndMessage;
        return false;
    }
    return true;
}

/* Verify the actual and the expected stencil match. Return true if
 * succeed, false otherwise.
 */
bool FramebufferBlitMultiToSingleSampledTestCase::checkStencil(const Stencil actual, const Stencil expected)
{
    if (actual != expected)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "ERROR: expected STENCIL[" << expected << "] but got STENCIL["
                           << actual << "]" << tcu::TestLog::EndMessage;
        return false;
    }
    return true;
}

/* Clear the depth buffer to given depth. Prior to return, unbind all
 * used attachments and setup default read and draw framebuffers.
 * Return true if succeed, false otherwise.
 *
 * fbo: framebuffer to use
 * attachment: framebuffer attachment to attach buffer
 * type: buffer type
 * buf: depthbuffer to be cleared
 * depth: clear depth
 * rect: region of depthbuffer to be cleared
 * check_coord: coord to verify buffer value after clearing
 */
bool FramebufferBlitMultiToSingleSampledTestCase::clearDepthBuffer(const GLuint fbo, const GLenum attachment,
                                                                   const GLenum type, const GLuint buf,
                                                                   const GLuint internalFormat, const Depth depth,
                                                                   const Rectangle &rect, const Coord &check_coord)
{
    bool result     = true;
    Depth tmp_depth = 0.2f;
    GLint sample_buffers;

    /* Get epsilon based on format precision */
    auto get_epsilon = [](const GLuint resultPreBits, const GLuint sourcePreBits)
    {
        GLuint tolerance = std::min(resultPreBits, sourcePreBits);
        tolerance        = std::min(tolerance, GLuint(23)); // don't exceed the amount of mantissa bits in a float
        return (float)1.0 / (1 << tolerance);
    };

    if (!check_param(
            (type == 0 || type == GL_TEXTURE_2D || type == GL_TEXTURE_2D_MULTISAMPLE || type == GL_RENDERBUFFER),
            "invalid type"))
        return false;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    if (fbo && (fbo != m_defaultFBO))
    {
        result &= attachBufferToFramebuffer(GL_DRAW_FRAMEBUFFER, attachment, type, buf);
        result &= attachBufferToFramebuffer(GL_READ_FRAMEBUFFER, attachment, type, buf);
    }

    // clear depth rectangle
    gl.scissor(rect.x, rect.y, rect.w, rect.h);
    GLU_EXPECT_NO_ERROR(gl.getError(), "scissor");
    gl.enable(GL_SCISSOR_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    GLuint status = gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
    CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);

    if (!m_isContextES)
    {
        gl.clearDepth(depth);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clearDepth");
    }
    else
    {
        gl.clearDepthf(depth);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clearDepthf");
    }

    m_testCtx.getLog() << tcu::TestLog::Message << "clearing depth to [" << depth << "]" << tcu::TestLog::EndMessage;

    gl.clear(GL_DEPTH_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");
    gl.disable(GL_SCISSOR_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    /* Verify the depth in cleared depth in case of single-sampled
     * buffers. Don't verify in case of multisampled buffer since
     * glReadPixels generates GL_INVALID_OPERATION if
     * GL_SAMPLE_BUFFERS is greater than zero. */
    gl.getIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);

    if (sample_buffers == 0)
    {
        if (fbo && (fbo != m_defaultFBO))
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "verifying initial "
                               << ((type == GL_TEXTURE_2D) ? "tex" : "ren") << "buf" << buf << " depth [" << depth
                               << "]" << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "verifying initial dfltbuf depth [" << depth << "]"
                               << tcu::TestLog::EndMessage;
        }

        GLuint precisionBits[2] = {0, 0};
        getDepth(check_coord, &tmp_depth, &precisionBits[0], fbo, internalFormat, rect);

        /* Calculate precision */
        precisionBits[1] = GetDepthPrecisionBits(internalFormat);
        GLfloat epsilon  = get_epsilon(precisionBits[0], precisionBits[1]);

        result = checkDepth(tmp_depth, depth, epsilon);
        CHECK(result, true, checkDepth);
    }
    else
    {
        if (fbo && (fbo != m_defaultFBO))
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "no verification of multisampled "
                               << ((type == GL_RENDERBUFFER) ? "ren" : "tex") << "buf" << buf
                               << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "no verification of multisampled dfltbuf"
                               << tcu::TestLog::EndMessage;
        }
    }

    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    return result;
}

/* Clear the stencil buffer to given stencil. Prior to return, unbind
 * all used attachments and setup default read and draw framebuffers.
 * Return true if succeed, false otherwise.
 *
 * fbo: framebuffer to use
 * attachment: framebuffer attachment to attach buffer
 * type: buffer type
 * buf: stencilbuffer to be cleared
 * stcil: clear stcil
 * rect: region of stencilbuffer to be cleared
 * check_coord: coord to verify buffer value after clearing
 */
bool FramebufferBlitMultiToSingleSampledTestCase::clearStencilBuffer(const GLuint fbo, const GLenum attachment,
                                                                     const GLenum type, const GLuint buf,
                                                                     const GLuint internalFormat, const Stencil stcil,
                                                                     const Rectangle &rect, const Coord &check_coord)
{
    bool result          = true;
    Stencil tmp_stcil    = 50;
    GLint sample_buffers = 0;

    if (!check_param(
            (type == 0 || type == GL_TEXTURE_2D || type == GL_TEXTURE_2D_MULTISAMPLE || type == GL_RENDERBUFFER),
            "invalid type"))
        return false;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    if (fbo && (fbo != m_defaultFBO))
    {
        result &= attachBufferToFramebuffer(GL_DRAW_FRAMEBUFFER, attachment, type, buf);
        result &= attachBufferToFramebuffer(GL_READ_FRAMEBUFFER, attachment, type, buf);
    }

    // clear stencil rectangle
    gl.scissor(rect.x, rect.y, rect.w, rect.h);
    GLU_EXPECT_NO_ERROR(gl.getError(), "scissor");
    gl.enable(GL_SCISSOR_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    GLuint status = gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
    CHECK_RET(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);

    gl.clearStencil(stcil);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearStencil");

    m_testCtx.getLog() << tcu::TestLog::Message << "clearing stencil to [" << stcil << "]" << tcu::TestLog::EndMessage;

    gl.clear(GL_STENCIL_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");
    gl.disable(GL_SCISSOR_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    /* Verify the stencil in cleared stencil in case of single-sampled
     * buffers. Don't verify in case of multisampled buffer since
     * glReadPixels generates GL_INVALID_OPERATION if
     * GL_SAMPLE_BUFFERS is greater than zero. */
    gl.getIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
    if (sample_buffers == 0)
    {
        if (fbo && (fbo != m_defaultFBO))
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "verifying initial "
                               << ((type == GL_TEXTURE_2D) ? "tex" : "ren") << "buf" << buf << " stencil [" << stcil
                               << "]" << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "verifying initial dfltbuf stencil [" << stcil << "]"
                               << tcu::TestLog::EndMessage;
        }

        getStencil(check_coord, &tmp_stcil, fbo, internalFormat, rect);
        result = checkStencil(tmp_stcil, stcil);
        CHECK(result, true, checkStencil);
    }
    else
    {
        if (fbo && (fbo != m_defaultFBO))
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "no verification of multisampled "
                               << ((type == GL_RENDERBUFFER) ? "ren" : "tex") << "buf" << buf
                               << tcu::TestLog::EndMessage;
        }
        else
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "no verification of multisampled dfltbuf"
                               << tcu::TestLog::EndMessage;
        }
    }

    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    return result;
}

/* print values in global variables
 */
void FramebufferBlitMultiToSingleSampledTestCase::printGlobalBufferInfo()
{
    auto print_info = [&](GLuint h0, GLuint h1, const char *str)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "CONFIG: " << str << "[0]=" << h0 << ", " << str << "[1]=" << h1
                           << tcu::TestLog::EndMessage;
    };

    print_info(m_fbos[0], m_fbos[1], "fbos");
    print_info(m_color_tbos[0], m_color_tbos[1], "color_tbos");
    print_info(m_depth_tbos[0], m_depth_tbos[1], "depth_tbos");
    print_info(m_stcil_tbos[0], m_stcil_tbos[1], "stcil_tbos");
    print_info(m_color_rbos[0], m_color_rbos[1], "color_rbos");
    print_info(m_depth_rbos[0], m_depth_rbos[1], "depth_rbos");
    print_info(m_stcil_rbos[0], m_stcil_rbos[1], "stcil_rbos");

    m_testCtx.getLog() << tcu::TestLog::Message
                       << "\nCONFIG: depth_internalFormat=" << getEnumName(m_depth_internalFormat)
                       << "\nCONFIG: stcil_internalFormat=" << getEnumName(m_stcil_internalFormat)
                       << tcu::TestLog::EndMessage;
}

/** Executes color configuration framebuffer blit tests.
 *
 *  @return Returns false if test went wrong.
 */
template <GLuint samples>
bool FramebufferBlitMultiToSingleSampledTestCase::testColorBlitConfig(const tcu::IVec2 &ul_center,
                                                                      const tcu::IVec2 &ur_center,
                                                                      const tcu::IVec2 &ll_center,
                                                                      const tcu::IVec2 &lr_center,
                                                                      const glw::GLint max_color_attachments)
{
    bool result  = true;
    GLint status = 0;
    tcu::Vec4 tmp_color(0.0f, 0.0f, 0.0f, 0.0f);
    /* default color for multicolor pattern */
    tcu::Vec4 ul_color = RED, ur_color = GREEN, ll_color = BLUE, lr_color = WHITE;
    GLuint bits   = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    GLuint filter = GL_NEAREST;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    /* test buffer combinations (texture to texture, texture to
     * renderbuffer, etc.) */
    for (GLuint i = 0; i < m_bufferCfg.size(); i++)
    {
        /* test color attachments */
        for (GLint j = 0; j < max_color_attachments; j++)
        {
            /* test color formats */
            for (GLuint k = 0; k < m_multisampleColorCfg.size(); k++)
            {
                const GLenum &attachment                   = GL_COLOR_ATTACHMENT0 + j;
                const BufferConfig &buf_config             = m_bufferCfg[i];
                const MultisampleColorConfig &color_config = m_multisampleColorCfg[k];

                if (m_isContextES)
                {
                    /* If the format is FP, skip if the extension is not present */
                    if (color_config.bIsFloat && !m_cbfTestSupported)
                    {
                        continue;
                    }
                }

                // Check default framebuffer
                if ((buf_config.src_type == 0) || (buf_config.dst_type == 0))
                {
                    GLint sample_buffers = 0;
                    GLint red_bits = 0, green_bits = 0, blue_bits = 0, alpha_bits = 0;

                    gl.bindFramebuffer(GL_FRAMEBUFFER, m_defaultFBO);
                    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
                    gl.getIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
                    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

                    // Skip if default is used as src but not multisampled
                    // or if default is used as dst but multisampled
                    if (((buf_config.src_type == 0) && (sample_buffers == 0)) ||
                        ((buf_config.dst_type == 0) && (sample_buffers != 0)))
                    {
                        continue;
                    }

                    if (m_isContextES)
                    {
                        // check if default framebuffer supports GL_SRGB encoding
                        if (GL_SRGB8_ALPHA8 == color_config.internal_format)
                        {
                            int encoding = GL_NONE;

                            gl.getFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                                   m_defaultFBO ? GL_COLOR_ATTACHMENT0 : GL_BACK,
                                                                   GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
                            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");

                            if (GL_SRGB != encoding)
                            {
                                continue;
                            }
                        }

                        {
                            // Multisample color format and type must match that of the default framebuffer when blitting
                            GLint format = 0;
                            // framebuffer_blit_functionality_multisampled_to_singlesampled_blit was attempting to
                            // match the format and type of a framebuffer by querying GL_IMPLEMENTATION_COLOR_READ_FORMAT and
                            // GL_IMPLEMENTATION_COLOR_READ_TYPE, which could return anything the implementation
                            // chooses, not necessarily the actual format and type of the framebuffer.
                            // Currently there is no api to determine format and type of the default (EGL) framebuffer id 0
                            if (m_defaultFBO)
                            {
                                // defaultFBO is bound to both read and draw framebuffers, so there is no need to specify.
                                GLint object_type = 0;

                                gl.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                                       GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                                       &object_type);
                                GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                                if (object_type == GL_RENDERBUFFER)
                                {
                                    GLint renderbuffer = 0;
                                    gl.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                                           GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                                           &renderbuffer);
                                    GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                                    gl.bindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
                                    GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
                                    gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT,
                                                                  &format);
                                    GLU_EXPECT_NO_ERROR(gl.getError(), "getRenderbufferParameteriv");
                                    if (color_config.internal_format != format)
                                    {
                                        continue;
                                    }
                                }
                                else
                                {

                                    m_testCtx.getLog() << tcu::TestLog::Message
                                                       << "Could not read default FBO type and format because color "
                                                          "attachment 0 is not a renderbuffer."
                                                       << tcu::TestLog::EndMessage;
                                    continue;
                                }
                            }
                            else
                            {
                                m_testCtx.getLog() << tcu::TestLog::Message
                                                   << "Could not read default FBO type and format because FBO ID is 0."
                                                   << tcu::TestLog::EndMessage;
                                continue;
                            }
                        }

                        // Check that the default framebuffer has all the channels we will need.
                        gl.getFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                               m_defaultFBO ? GL_COLOR_ATTACHMENT0 : GL_BACK,
                                                               GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &red_bits);
                        GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");

                        gl.getFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                               m_defaultFBO ? GL_COLOR_ATTACHMENT0 : GL_BACK,
                                                               GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &green_bits);
                        GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                        gl.getFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                               m_defaultFBO ? GL_COLOR_ATTACHMENT0 : GL_BACK,
                                                               GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &blue_bits);
                        GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                        gl.getFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                               m_defaultFBO ? GL_COLOR_ATTACHMENT0 : GL_BACK,
                                                               GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alpha_bits);
                        GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");

                        if (((color_config.color_channel_bits & RED_CHANNEL) && red_bits == 0) ||
                            ((color_config.color_channel_bits & GREEN_CHANNEL) && green_bits == 0) ||
                            ((color_config.color_channel_bits & BLUE_CHANNEL) && blue_bits == 0) ||
                            ((color_config.color_channel_bits & ALPHA_CHANNEL) && alpha_bits == 0))
                        {
                            m_testCtx.getLog()
                                << tcu::TestLog::Message << "Required channel for "
                                << getEnumName(color_config.internal_format)
                                << " not present in default framebuffer. Skipping." << tcu::TestLog::EndMessage;
                            continue;
                        }
                    }
                }

                /* skip the configs where same buffer is used as a
                 * read and draw buffer (different samplings) */
                if (buf_config.same_read_and_draw_buffer)
                    continue;

                if (m_isContextES)
                {
                    /* ES3.0 does not support multi-sample texture */
                    if (!m_msTbosSupported && buf_config.src_type == GL_TEXTURE_2D)
                    {
                        continue;
                    }
                }

                m_testCtx.getLog() << tcu::TestLog::Message
                                   << "BEGIN ------------------------------------------------------------------"
                                   << "BLITTING in " << getEnumName(attachment) << " from "
                                   << ((!buf_config.src_type) ? getEnumName(DEFAULT) : getEnumName(buf_config.src_type))
                                   << " to "
                                   << ((!buf_config.dst_type) ? getEnumName(DEFAULT) : getEnumName(buf_config.dst_type))
                                   << "[" << getEnumName(color_config.internal_format) << "] buffers"
                                   << tcu::TestLog::EndMessage;

                if (!m_isContextES)
                {
                    gl.enable(GL_MULTISAMPLE);
                    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");
                }

                gl.genFramebuffers(2, m_fbos);
                GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");

                gl.genTextures(2, m_color_tbos);
                GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

                /* init multisampled texture for reading and
                 * single-sampled texture for drawing */
                if (m_msTbosSupported)
                    result &= init_gl_objs<GL_TEXTURE_2D_MULTISAMPLE, samples>(gl.bindTexture, 1, &m_color_tbos[0],
                                                                               color_config.internal_format);

                result &=
                    init_gl_objs<GL_TEXTURE_2D, 0>(gl.bindTexture, 1, &m_color_tbos[1], color_config.internal_format);

                gl.genRenderbuffers(2, m_color_rbos);
                GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");

                result &= init_gl_objs<GL_RENDERBUFFER, samples>(gl.bindRenderbuffer, 1, &m_color_rbos[0],
                                                                 color_config.internal_format);

                result &= init_gl_objs<GL_RENDERBUFFER, 0>(gl.bindRenderbuffer, 1, &m_color_rbos[1],
                                                           color_config.internal_format);

                /* multicolor pattern to the source */
                if (m_msTbosSupported && buf_config.src_type == GL_TEXTURE_2D)
                {
                    result &= clearColorBuffer(m_fbos[0], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                                               m_color_tbos[0], ul_color, m_setup.ul_rect, ul_center,
                                               color_config.color_channel_bits, color_config.bIsFloat);
                    result &= clearColorBuffer(m_fbos[0], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                                               m_color_tbos[0], ur_color, m_setup.ur_rect, ur_center,
                                               color_config.color_channel_bits, color_config.bIsFloat);
                    result &= clearColorBuffer(m_fbos[0], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                                               m_color_tbos[0], ll_color, m_setup.ll_rect, ll_center,
                                               color_config.color_channel_bits, color_config.bIsFloat);
                    result &= clearColorBuffer(m_fbos[0], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                                               m_color_tbos[0], lr_color, m_setup.lr_rect, lr_center,
                                               color_config.color_channel_bits, color_config.bIsFloat);
                }
                else if (buf_config.src_type == GL_RENDERBUFFER)
                {
                    result &= clearColorBuffer(m_fbos[0], GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_color_rbos[0],
                                               ul_color, m_setup.ul_rect, ul_center, color_config.color_channel_bits,
                                               color_config.bIsFloat);
                    result &= clearColorBuffer(m_fbos[0], GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_color_rbos[0],
                                               ur_color, m_setup.ur_rect, ur_center, color_config.color_channel_bits,
                                               color_config.bIsFloat);
                    result &= clearColorBuffer(m_fbos[0], GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_color_rbos[0],
                                               ll_color, m_setup.ll_rect, ll_center, color_config.color_channel_bits,
                                               color_config.bIsFloat);
                    result &= clearColorBuffer(m_fbos[0], GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_color_rbos[0],
                                               lr_color, m_setup.lr_rect, lr_center, color_config.color_channel_bits,
                                               color_config.bIsFloat);
                }
                else
                {
                    result &=
                        clearColorBuffer(m_defaultFBO, GL_NONE, 0, 0, ul_color, m_setup.ul_rect, ul_center,
                                         color_config.color_channel_bits, color_config.bIsFloat); /* default buffer */
                    result &=
                        clearColorBuffer(m_defaultFBO, GL_NONE, 0, 0, ur_color, m_setup.ur_rect, ur_center,
                                         color_config.color_channel_bits, color_config.bIsFloat); /* default buffer */
                    result &=
                        clearColorBuffer(m_defaultFBO, GL_NONE, 0, 0, ll_color, m_setup.ll_rect, ll_center,
                                         color_config.color_channel_bits, color_config.bIsFloat); /* default buffer */
                    result &=
                        clearColorBuffer(m_defaultFBO, GL_NONE, 0, 0, lr_color, m_setup.lr_rect, lr_center,
                                         color_config.color_channel_bits, color_config.bIsFloat); /* default buffer */
                }

                /* initial destination color to the destination */
                if (buf_config.dst_type == GL_TEXTURE_2D)
                {
                    result &= clearColorBuffer(m_fbos[1], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color_tbos[1],
                                               DST_COLOR, m_fullRect, m_defaultCoord, color_config.color_channel_bits,
                                               color_config.bIsFloat);
                }
                else if (buf_config.dst_type == GL_RENDERBUFFER)
                {
                    result &= clearColorBuffer(m_fbos[1], GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_color_rbos[1],
                                               DST_COLOR, m_fullRect, m_defaultCoord, color_config.color_channel_bits,
                                               color_config.bIsFloat);
                }
                else
                {
                    result &= clearColorBuffer(m_defaultFBO, GL_NONE, 0, 0, DST_COLOR, m_fullRect, m_defaultCoord,
                                               color_config.color_channel_bits, color_config.bIsFloat);
                }

                printGlobalBufferInfo();

                /* bind framebuffer objects */
                gl.bindFramebuffer(GL_READ_FRAMEBUFFER, buf_config.src_type == 0 ? m_defaultFBO : *buf_config.src_fbo);
                GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

                gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo);
                GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

                /* attach color buffers */
                if (m_msTbosSupported && buf_config.src_type == GL_TEXTURE_2D)
                {
                    result &= attachBufferToFramebuffer(GL_READ_FRAMEBUFFER, attachment, GL_TEXTURE_2D_MULTISAMPLE,
                                                        *buf_config.src_cbuf);
                }
                else
                {
                    result &= attachBufferToFramebuffer(GL_READ_FRAMEBUFFER, attachment, buf_config.src_type,
                                                        *buf_config.src_cbuf);
                }
                result &= attachBufferToFramebuffer(GL_DRAW_FRAMEBUFFER, attachment, buf_config.dst_type,
                                                    *buf_config.dst_cbuf);

                if (buf_config.src_type != 0)
                {
                    gl.readBuffer(attachment);
                    GLU_EXPECT_NO_ERROR(gl.getError(), "readBuffer");
                }

                if (buf_config.dst_type != 0)
                {
                    if (m_isContextES)
                    {
                        std::vector<GLenum> draw_attachments(max_color_attachments);
                        draw_attachments.assign(max_color_attachments, GL_NONE);
                        draw_attachments[j] = attachment;
                        gl.drawBuffers(j + 1, draw_attachments.data());
                    }
                    else
                    {
                        gl.drawBuffers(1, &attachment);
                    }
                    GLU_EXPECT_NO_ERROR(gl.getError(), "drawBuffers");
                }

                status = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
                GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
                CHECK_CONTINUE(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);
                status = gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
                GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
                CHECK_CONTINUE(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);

                m_testCtx.getLog() << tcu::TestLog::Message
                                   << "BLIT -------------------------------------------------------------------"
                                   << "BLIT SRC_RECT=[" << m_setup.blt_src_rect.x << "," << m_setup.blt_src_rect.y
                                   << "," << m_setup.blt_src_rect.w << "," << m_setup.blt_src_rect.h << "] DST_RECT=["
                                   << m_setup.blt_dst_rect.x << "," << m_setup.blt_dst_rect.y << ","
                                   << m_setup.blt_dst_rect.w << "," << m_setup.blt_dst_rect.h << "]"
                                   << tcu::TestLog::EndMessage;

                /* blit */
                gl.blitFramebuffer(m_setup.blt_src_rect.x, m_setup.blt_src_rect.y,
                                   m_setup.blt_src_rect.x + m_setup.blt_src_rect.w,
                                   m_setup.blt_src_rect.y + m_setup.blt_src_rect.h, m_setup.blt_dst_rect.x,
                                   m_setup.blt_dst_rect.y, m_setup.blt_dst_rect.x + m_setup.blt_dst_rect.w,
                                   m_setup.blt_dst_rect.y + m_setup.blt_dst_rect.h, bits, filter);
                GLU_EXPECT_NO_ERROR(gl.getError(), "blitFramebuffer");

                /* bind dst_fbo to GL_READ_FRAMEBUFFER */
                gl.bindFramebuffer(GL_READ_FRAMEBUFFER, buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo);
                GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

                /* setup the read color buffer again if the destination
                 * buffer is an user fbo */
                if (buf_config.dst_type != 0)
                {
                    gl.readBuffer(attachment);
                    GLU_EXPECT_NO_ERROR(gl.getError(), "readBuffer");
                }

                std::ostringstream sstr;
                sstr << "BITS [" << std::hex << color_config.color_channel_bits << "]=["
                     << getEnumName(color_config.color_channel_bits) << "]";
                m_testCtx.getLog() << tcu::TestLog::Message << sstr.str() << tcu::TestLog::EndMessage;

                /* read and verify color values */
                getColor(m_setup.ul_coord, &tmp_color, color_config.bIsFloat);
                result &= checkColor(tmp_color, ul_color, color_config.color_channel_bits);
                CHECK_COLOR(result, true, checkColor);

                getColor(m_setup.ur_coord, &tmp_color, color_config.bIsFloat);
                result &= checkColor(tmp_color, ur_color, color_config.color_channel_bits);
                CHECK_COLOR(result, true, checkColor);

                getColor(m_setup.ll_coord, &tmp_color, color_config.bIsFloat);
                result &= checkColor(tmp_color, ll_color, color_config.color_channel_bits);
                CHECK_COLOR(result, true, checkColor);

                getColor(m_setup.lr_coord, &tmp_color, color_config.bIsFloat);
                result &= checkColor(tmp_color, lr_color, color_config.color_channel_bits);
                CHECK_COLOR(result, true, checkColor);

                gl.deleteTextures(2, m_color_tbos);
                GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

                m_color_tbos[0] = 0;
                m_color_tbos[1] = 0;
                gl.deleteRenderbuffers(2, m_color_rbos);
                GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");

                m_color_rbos[0] = 0;
                m_color_rbos[1] = 0;
                gl.deleteFramebuffers(2, m_fbos);
                GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

                m_fbos[0] = 0;
                m_fbos[1] = 0;

                if (!m_isContextES)
                {
                    gl.disable(GL_MULTISAMPLE);
                    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
                }

                m_testCtx.getLog() << tcu::TestLog::Message
                                   << "END --------------------------------------------------------------------"
                                   << tcu::TestLog::EndMessage;
            }
        }
    }
    return result;
}

/** Executes depth configuration framebuffer blit tests.
 *
 *  @return Returns false if test went wrong.
 */
template <GLuint samples>
bool FramebufferBlitMultiToSingleSampledTestCase::testDepthBlitConfig(const tcu::IVec2 &ul_center,
                                                                      const tcu::IVec2 &ur_center,
                                                                      const tcu::IVec2 &ll_center,
                                                                      const tcu::IVec2 &lr_center)
{
    bool result       = true;
    GLint status      = 0;
    GLfloat tmp_depth = 0.0f;
    /* default depth for multicolor pattern */
    GLfloat ul_depth = Q1, ur_depth = Q2, ll_depth = Q3, lr_depth = Q4;
    /* default stcil for multicolor pattern */
    GLuint ul_stcil = ONE, ur_stcil = TWO, ll_stcil = THREE, lr_stcil = FOUR;
    GLuint bits   = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    GLuint filter = GL_NEAREST;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    auto get_blit_epsilon = [](const GLuint srcPreBits, const GLuint resultPreBits, const GLuint dstPreBits)
    {
        GLuint tolerance = std::min(srcPreBits, std::min(resultPreBits, dstPreBits));
        tolerance        = std::min(tolerance, GLuint(23)); // don't exceed the amount of mantissa bits in a float
        return (GLfloat)1.0 / (1 << tolerance);
    };

    /* test buffer combinations (texture to texture, texture to
     * renderbuffer, etc.) */
    for (GLuint i = 0; i < m_bufferCfg.size(); i++)
    {
        /* test depth formats */
        for (GLuint j = 0; j < m_depthCfg.size(); j++)
        {
            BufferConfig buf_config  = m_bufferCfg[i];
            DepthConfig depth_config = m_depthCfg[j];

            // Check default framebuffer
            if ((buf_config.src_type == 0) || (buf_config.dst_type == 0))
            {
                GLint sample_buffers = 0;
                gl.bindFramebuffer(GL_FRAMEBUFFER, m_defaultFBO);
                GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

                gl.getIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

                // Skip if default is used as src but not multisampled
                // or if default is used as dst but multisampled
                if (((buf_config.src_type == 0) && (sample_buffers == 0)) ||
                    ((buf_config.dst_type == 0) && (sample_buffers != 0)))
                {
                    continue;
                }

                {
                    // Check format
                    bool noDefaultDepth, noDefaultStcil;

                    if (!GetDefaultFramebufferBlitFormat(&noDefaultDepth, &noDefaultStcil))
                    {
                        continue;
                    }

                    if (noDefaultDepth || (depth_config.internal_format != m_depth_internalFormat))
                    {
                        continue;
                    }
                }
            }

            /* skip the configs where same buffer is used as a
             * read and draw buffer (different samplings) */
            if (buf_config.same_read_and_draw_buffer)
                continue;

            if (m_isContextES)
            {
                /* ES3.0 does not support multi-sample texture */
                if (!m_msTbosSupported && buf_config.src_type == GL_TEXTURE_2D)
                {
                    continue;
                }
            }

            m_testCtx.getLog() << tcu::TestLog::Message
                               << "BEGIN ------------------------------------------------------------------"
                               << "[" << getEnumName(depth_config.internal_format) << "] buffers"
                               << tcu::TestLog::EndMessage;

            if (!m_isContextES)
            {
                gl.enable(GL_MULTISAMPLE);
                GLU_EXPECT_NO_ERROR(gl.getError(), "enable");
            }

            gl.genFramebuffers(2, m_fbos);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");

            gl.genTextures(2, m_depth_tbos);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

            /* init multisampled texture for reading and
             * single-sampled texture for drawing */
            if (m_msTbosSupported)
                result &= init_gl_objs<GL_TEXTURE_2D_MULTISAMPLE, samples>(gl.bindTexture, 1, &m_depth_tbos[0],
                                                                           depth_config.internal_format);

            result &= init_gl_objs<GL_TEXTURE_2D, 0>(gl.bindTexture, 1, &m_depth_tbos[1], depth_config.internal_format);

            gl.genRenderbuffers(2, m_depth_rbos);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");

            result &= init_gl_objs<GL_RENDERBUFFER, samples>(gl.bindRenderbuffer, 1, &m_depth_rbos[0],
                                                             depth_config.internal_format);

            result &= init_gl_objs<GL_RENDERBUFFER, 0>(gl.bindRenderbuffer, 1, &m_depth_rbos[1],
                                                       depth_config.internal_format);

            /* prepare depth-only buffers */

            /* multicolor pattern to the source texture */
            if (m_msTbosSupported && buf_config.src_type == GL_TEXTURE_2D)
            {
                result &=
                    clearDepthBuffer(m_fbos[0], depth_config.attachment, GL_TEXTURE_2D_MULTISAMPLE, m_depth_tbos[0],
                                     depth_config.internal_format, ul_depth, m_setup.ul_rect, ul_center);
                result &=
                    clearDepthBuffer(m_fbos[0], depth_config.attachment, GL_TEXTURE_2D_MULTISAMPLE, m_depth_tbos[0],
                                     depth_config.internal_format, ur_depth, m_setup.ur_rect, ur_center);
                result &=
                    clearDepthBuffer(m_fbos[0], depth_config.attachment, GL_TEXTURE_2D_MULTISAMPLE, m_depth_tbos[0],
                                     depth_config.internal_format, ll_depth, m_setup.ll_rect, ll_center);
                result &=
                    clearDepthBuffer(m_fbos[0], depth_config.attachment, GL_TEXTURE_2D_MULTISAMPLE, m_depth_tbos[0],
                                     depth_config.internal_format, lr_depth, m_setup.lr_rect, lr_center);
            }
            else if (buf_config.src_type == GL_RENDERBUFFER)
            {
                result &= clearDepthBuffer(m_fbos[0], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[0],
                                           depth_config.internal_format, ul_depth, m_setup.ul_rect, ul_center);
                result &= clearDepthBuffer(m_fbos[0], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[0],
                                           depth_config.internal_format, ur_depth, m_setup.ur_rect, ur_center);
                result &= clearDepthBuffer(m_fbos[0], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[0],
                                           depth_config.internal_format, ll_depth, m_setup.ll_rect, ll_center);
                result &= clearDepthBuffer(m_fbos[0], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[0],
                                           depth_config.internal_format, lr_depth, m_setup.lr_rect, lr_center);
            }
            else
            {
                /* multicolor pattern to the default buffer */
                result &= clearDepthBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, ul_depth,
                                           m_setup.ul_rect, ul_center); /* default buffer */
                result &= clearDepthBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, ur_depth,
                                           m_setup.ur_rect, ur_center); /* default buffer */
                result &= clearDepthBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, ll_depth,
                                           m_setup.ll_rect, ll_center); /* default buffer */
                result &= clearDepthBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, lr_depth,
                                           m_setup.lr_rect, lr_center); /* default buffer */
            }

            /* initial destination depth to the destination */
            if (buf_config.dst_type == GL_TEXTURE_2D)
            {
                result &= clearDepthBuffer(m_fbos[1], depth_config.attachment, GL_TEXTURE_2D, m_depth_tbos[1],
                                           depth_config.internal_format, DST_DEPTH, m_fullRect, m_defaultCoord);
            }
            else if (buf_config.dst_type == GL_RENDERBUFFER)
            {
                result &= clearDepthBuffer(m_fbos[1], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[1],
                                           depth_config.internal_format, DST_DEPTH, m_fullRect, m_defaultCoord);
            }
            else
            {
                result &= clearDepthBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, DST_DEPTH, m_fullRect,
                                           m_defaultCoord);
            }

            /* prepare depth-stencil buffers */
            if (depth_config.attachment == GL_DEPTH_STENCIL_ATTACHMENT)
            {
                if (m_msTbosSupported && buf_config.src_type == GL_TEXTURE_2D)
                {
                    result &= clearStencilBuffer(m_fbos[0], depth_config.attachment, GL_TEXTURE_2D_MULTISAMPLE,
                                                 m_depth_tbos[0], depth_config.internal_format, ul_stcil,
                                                 m_setup.ul_rect, ul_center);
                    result &= clearStencilBuffer(m_fbos[0], depth_config.attachment, GL_TEXTURE_2D_MULTISAMPLE,
                                                 m_depth_tbos[0], depth_config.internal_format, ur_stcil,
                                                 m_setup.ur_rect, ur_center);
                    result &= clearStencilBuffer(m_fbos[0], depth_config.attachment, GL_TEXTURE_2D_MULTISAMPLE,
                                                 m_depth_tbos[0], depth_config.internal_format, ll_stcil,
                                                 m_setup.ll_rect, ll_center);
                    result &= clearStencilBuffer(m_fbos[0], depth_config.attachment, GL_TEXTURE_2D_MULTISAMPLE,
                                                 m_depth_tbos[0], depth_config.internal_format, lr_stcil,
                                                 m_setup.lr_rect, lr_center);
                }
                else if (buf_config.src_type == GL_RENDERBUFFER)
                {
                    result &= clearStencilBuffer(m_fbos[0], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[0],
                                                 depth_config.internal_format, ul_stcil, m_setup.ul_rect, ul_center);
                    result &= clearStencilBuffer(m_fbos[0], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[0],
                                                 depth_config.internal_format, ur_stcil, m_setup.ur_rect, ur_center);
                    result &= clearStencilBuffer(m_fbos[0], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[0],
                                                 depth_config.internal_format, ll_stcil, m_setup.ll_rect, ll_center);
                    result &= clearStencilBuffer(m_fbos[0], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[0],
                                                 depth_config.internal_format, lr_stcil, m_setup.lr_rect, lr_center);
                }
                else
                {
                    /* multicolor pattern to the default buffer */
                    result &= clearStencilBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, ul_stcil,
                                                 m_setup.ul_rect, ul_center);
                    result &= clearStencilBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, ur_stcil,
                                                 m_setup.ur_rect, ur_center);
                    result &= clearStencilBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, ll_stcil,
                                                 m_setup.ll_rect, ll_center);
                    result &= clearStencilBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, lr_stcil,
                                                 m_setup.lr_rect, lr_center);
                }

                /* initial destination stencil to the destination texture */
                if (buf_config.dst_type == GL_TEXTURE_2D)
                {
                    result &= clearStencilBuffer(m_fbos[1], depth_config.attachment, GL_TEXTURE_2D, m_depth_tbos[1],
                                                 depth_config.internal_format, DST_STCIL, m_fullRect, m_defaultCoord);
                }
                else if (buf_config.dst_type == GL_RENDERBUFFER)
                {
                    result &= clearStencilBuffer(m_fbos[1], depth_config.attachment, GL_RENDERBUFFER, m_depth_rbos[1],
                                                 depth_config.internal_format, DST_STCIL, m_fullRect, m_defaultCoord);
                }
                else
                {
                    result &= clearStencilBuffer(m_defaultFBO, 0, 0, 0, depth_config.internal_format, DST_STCIL,
                                                 m_fullRect, m_defaultCoord);
                }
            }

            printGlobalBufferInfo();

            /* bind and attach */
            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, buf_config.src_type == 0 ? m_defaultFBO : *buf_config.src_fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
            gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            if (m_msTbosSupported && buf_config.src_type == GL_TEXTURE_2D)
            {
                result &= attachBufferToFramebuffer(GL_READ_FRAMEBUFFER, depth_config.attachment,
                                                    GL_TEXTURE_2D_MULTISAMPLE, *buf_config.src_dbuf);
            }
            else
            {
                result &= attachBufferToFramebuffer(GL_READ_FRAMEBUFFER, depth_config.attachment, buf_config.src_type,
                                                    *buf_config.src_dbuf);
            }
            result &= attachBufferToFramebuffer(GL_DRAW_FRAMEBUFFER, depth_config.attachment, buf_config.dst_type,
                                                *buf_config.dst_dbuf);

            /* check status */
            status = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
            GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
            CHECK_CONTINUE(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);
            status = gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");
            CHECK_CONTINUE(status, GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus);

            m_testCtx.getLog() << tcu::TestLog::Message << "BLITTING in " << getEnumName(depth_config.attachment)
                               << " from "
                               << ((!buf_config.src_type) ? getEnumName(DEFAULT) : getEnumName(buf_config.src_type))
                               << " to "
                               << ((!buf_config.dst_type) ? getEnumName(DEFAULT) : getEnumName(buf_config.dst_type))
                               << tcu::TestLog::EndMessage;

            m_testCtx.getLog() << tcu::TestLog::Message
                               << "BLIT -------------------------------------------------------------------"
                               << tcu::TestLog::EndMessage;

            m_testCtx.getLog() << tcu::TestLog::Message << "BLIT SRC_RECT=[" << m_setup.blt_src_rect.x << ","
                               << m_setup.blt_src_rect.y << "," << m_setup.blt_src_rect.x + m_setup.blt_src_rect.w
                               << "," << m_setup.blt_src_rect.y + m_setup.blt_src_rect.h << "] DST_RECT=["
                               << m_setup.blt_dst_rect.x << "," << m_setup.blt_dst_rect.y << ","
                               << m_setup.blt_dst_rect.x + m_setup.blt_dst_rect.w << ","
                               << m_setup.blt_dst_rect.y + m_setup.blt_dst_rect.h << "]" << tcu::TestLog::EndMessage;

            /* blit */
            gl.blitFramebuffer(m_setup.blt_src_rect.x, m_setup.blt_src_rect.y,
                               m_setup.blt_src_rect.x + m_setup.blt_src_rect.w,
                               m_setup.blt_src_rect.y + m_setup.blt_src_rect.h, m_setup.blt_dst_rect.x,
                               m_setup.blt_dst_rect.y, m_setup.blt_dst_rect.x + m_setup.blt_dst_rect.w,
                               m_setup.blt_dst_rect.y + m_setup.blt_dst_rect.h, bits, filter);
            GLU_EXPECT_NO_ERROR(gl.getError(), "blitFramebuffer");

            /* bind destination framebuffer for reading */
            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            /* read and verify depth values */
            {
                GLuint resultPreBits = 0;
                GLint srcPreBits = 0, dstPreBits = 0;
                GLfloat epsilon = 0.f;

                if (buf_config.src_type)
                    srcPreBits = GetDepthPrecisionBits(depth_config.internal_format);
                else
                    GetBits(GL_READ_FRAMEBUFFER, GL_DEPTH_BITS, &srcPreBits);

                if (buf_config.dst_type)
                    dstPreBits = GetDepthPrecisionBits(depth_config.internal_format);
                else
                    GetBits(GL_READ_FRAMEBUFFER, GL_DEPTH_BITS, &dstPreBits);

                getDepth(m_setup.ul_coord, &tmp_depth, &resultPreBits,
                         buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo, depth_config.internal_format,
                         m_setup.ul_rect);
                epsilon = get_blit_epsilon(srcPreBits, resultPreBits, dstPreBits);
                result &= checkDepth(tmp_depth, ul_depth, epsilon);
                CHECK_COLOR(result, true, checkDepth);

                getDepth(m_setup.ur_coord, &tmp_depth, &resultPreBits,
                         buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo, depth_config.internal_format,
                         m_setup.ur_rect);
                epsilon = get_blit_epsilon(srcPreBits, resultPreBits, dstPreBits);
                result &= checkDepth(tmp_depth, ur_depth, epsilon);
                CHECK_COLOR(result, true, checkDepth);

                getDepth(m_setup.ll_coord, &tmp_depth, &resultPreBits,
                         buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo, depth_config.internal_format,
                         m_setup.ll_rect);
                epsilon = get_blit_epsilon(srcPreBits, resultPreBits, dstPreBits);
                result &= checkDepth(tmp_depth, ll_depth, epsilon);
                CHECK_COLOR(result, true, checkDepth);

                getDepth(m_setup.lr_coord, &tmp_depth, &resultPreBits,
                         buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo, depth_config.internal_format,
                         m_setup.lr_rect);
                epsilon = get_blit_epsilon(srcPreBits, resultPreBits, dstPreBits);
                result &= checkDepth(tmp_depth, lr_depth, epsilon);
                CHECK_COLOR(result, true, checkDepth);
            }

            /* read and verify stencil values */
            if (depth_config.attachment == GL_DEPTH_STENCIL_ATTACHMENT)
            {
                Stencil tmp_stcil = 0;
                getStencil(m_setup.ul_coord, &tmp_stcil, buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo,
                           depth_config.internal_format, m_setup.ul_rect);
                result &= checkStencil(tmp_stcil, ul_stcil);
                CHECK_COLOR(result, true, checkStencil);

                getStencil(m_setup.ur_coord, &tmp_stcil, buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo,
                           depth_config.internal_format, m_setup.ur_rect);
                result &= checkStencil(tmp_stcil, ur_stcil);
                CHECK_COLOR(result, true, checkStencil);

                getStencil(m_setup.ll_coord, &tmp_stcil, buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo,
                           depth_config.internal_format, m_setup.ll_rect);
                result &= checkStencil(tmp_stcil, ll_stcil);
                CHECK_COLOR(result, true, checkStencil);

                getStencil(m_setup.lr_coord, &tmp_stcil, buf_config.dst_type == 0 ? m_defaultFBO : *buf_config.dst_fbo,
                           depth_config.internal_format, m_setup.lr_rect);
                result &= checkStencil(tmp_stcil, lr_stcil);
                CHECK_COLOR(result, true, checkStencil);
            }

            gl.deleteTextures(2, m_depth_tbos);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
            m_color_tbos[0] = 0;
            m_color_tbos[1] = 0;
            gl.deleteRenderbuffers(2, m_depth_rbos);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");
            m_depth_rbos[0] = 0;
            m_depth_rbos[1] = 0;
            m_color_rbos[0] = 0;
            m_color_rbos[1] = 0;
            gl.deleteFramebuffers(2, m_fbos);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
            m_fbos[0] = 0;
            m_fbos[1] = 0;

            if (!m_isContextES)
            {
                gl.disable(GL_MULTISAMPLE);
                GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
            }

            m_testCtx.getLog() << tcu::TestLog::Message
                               << "END --------------------------------------------------------------------"
                               << tcu::TestLog::EndMessage;
        }
    }

    return result;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult FramebufferBlitMultiToSingleSampledTestCase::iterate()
{
    bool result                 = true;
    GLint max_color_attachments = 0;
    /* quadrant centers for verifying initial colors */
    tcu::IVec2 ul_center, ur_center, ll_center, lr_center;
    constexpr GLuint samples = 4;

    /* quadrant centers to verify initial colors */
    ul_center[0] = m_setup.ul_rect.x + m_setup.ul_rect.w / 2;
    ul_center[1] = m_setup.ul_rect.y + m_setup.ul_rect.h / 2;
    ur_center[0] = m_setup.ur_rect.x + m_setup.ur_rect.w / 2;
    ur_center[1] = m_setup.ur_rect.y + m_setup.ur_rect.h / 2;
    ll_center[0] = m_setup.ll_rect.x + m_setup.ll_rect.w / 2;
    ll_center[1] = m_setup.ll_rect.y + m_setup.ll_rect.h / 2;
    lr_center[0] = m_setup.lr_rect.x + m_setup.lr_rect.w / 2;
    lr_center[1] = m_setup.lr_rect.y + m_setup.lr_rect.h / 2;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.getIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    CHECK_RET(max_color_attachments >= m_minColorAttachments, GL_TRUE, glGetIntegerv);

    if (m_isContextES)
    {
        GLint max_draw_buffers = 0;
        gl.getIntegerv(GL_MAX_DRAW_BUFFERS, &max_draw_buffers);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

        CHECK_RET(max_draw_buffers >= m_minDrawBuffers, GL_TRUE, glGetIntegerv);
        if (max_draw_buffers < max_color_attachments)
        {
            max_color_attachments = max_draw_buffers;
        }
    }

    /* 1. Test all color buffer formats, no depth or stencil buffers
     * attached here */

    CHECK_RET(testColorBlitConfig<samples>(ul_center, ur_center, ll_center, lr_center, max_color_attachments), true,
              "color blit test failed");

    /* 2. Test all depth buffer formats, no color or stencil buffers
     * attached here */

    CHECK_RET(testDepthBlitConfig<samples>(ul_center, ur_center, ll_center, lr_center), true, "depth blit test failed");

    if (result)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
FramebufferBlitTests::FramebufferBlitTests(deqp::Context &context)
    : TestCaseGroup(context, "framebuffer_blit", "Verify conformance of framebuffer blit implementation")
{
}

/** Initializes the test group contents. */
void FramebufferBlitTests::init()
{
    addChild(new FramebufferBlitMultiToSingleSampledTestCase(m_context));
}

} // namespace glcts
