#ifndef _GLCFRAMEBUFFERBLITTESTS_HPP
#define _GLCFRAMEBUFFERBLITTESTS_HPP
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
 * \file  glcFramebufferBlitTests.hpp
 * \brief Conformance tests for framebuffer blit feature functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuVector.hpp"
#include "tcuVectorType.hpp"
#include "gluShaderProgram.hpp"

#include <map>

namespace glcts
{
namespace blt
{
// test utils
struct Rectangle
{
    Rectangle(glw::GLint x_, glw::GLint y_, glw::GLint width_, glw::GLint height_) : x(x_), y(y_), w(width_), h(height_)
    {
    }

    Rectangle() : x(0), y(0), w(0), h(0)
    {
    }

    glw::GLint x;
    glw::GLint y;
    glw::GLint w;
    glw::GLint h;
};

using Stencil = glw::GLuint;
using Depth   = glw::GLfloat;
using Color   = tcu::Vec4;
using Coord   = tcu::IVec2;

/* struct used in 4.8 - 4.14 tests to confirm that the negative
 * height, negative width, negative dimensions, magnifying and
 * minifying works properly */
struct MultiColorTestSetup
{
    Rectangle ul_rect;                                 /* upper left region to multicolor pattern */
    Rectangle ur_rect;                                 /* upper right region to multicolor pattern */
    Rectangle ll_rect;                                 /* lower left region to multicolor pattern */
    Rectangle lr_rect;                                 /* lower right region to multicolor pattern */
    Rectangle blt_src_rect;                            /* src rectangle to glBlitFramebuffer */
    Rectangle blt_dst_rect;                            /* dst rectangle to glBlitFramebuffer */
    Rectangle scissor_rect;                            /* scissor rectangle used in 4.14 test */
    Coord ul_coord                     = {0, 0};       /* coord from which the upper left pixel is read */
    Coord ur_coord                     = {0, 0};       /* coord from which the upper right pixel is read */
    Coord ll_coord                     = {0, 0};       /* coord from which the lower left pixel is read */
    Coord lr_coord                     = {0, 0};       /* coord from which the lower right pixel is read */
    Color ul_color                     = {0, 0, 0, 0}; /* expected upper left color */
    Color ur_color                     = {0, 0, 0, 0}; /* expected upper right color */
    Color ll_color                     = {0, 0, 0, 0}; /* expected lower left color */
    Color lr_color                     = {0, 0, 0, 0}; /* expected lower right color */
    Depth ul_depth                     = 0.f;          /* expected upper left depth */
    Depth ur_depth                     = 0.f;          /* expected upper right depth */
    Depth ll_depth                     = 0.f;          /* expected lower left depth */
    Depth lr_depth                     = 0.f;          /* expected lower right depth */
    Stencil ul_stcil                   = 0;            /* expected upper left stcil */
    Stencil ur_stcil                   = 0;            /* expected upper right stcil */
    Stencil ll_stcil                   = 0;            /* expected lower left stcil */
    Stencil lr_stcil                   = 0;            /* expected lower right stcil */
    glw::GLboolean negative_src_width  = 0;            /* negative src rectangle width */
    glw::GLboolean negative_src_height = 0;            /* negative src rectangle height */
    glw::GLboolean negative_dst_width  = 0;            /* negative dst rectangle width */
    glw::GLboolean negative_dst_height = 0;            /* negative dst rectangle height */
};

/* typedefs, structs */
struct BufferConfig
{
    glw::GLuint *src_fbo = nullptr;
    glw::GLuint *dst_fbo = nullptr;
    glw::GLuint src_type = 0;
    glw::GLuint dst_type;
    glw::GLuint *src_cbuf                 = nullptr;
    glw::GLuint *src_dbuf                 = nullptr;
    glw::GLuint *src_sbuf                 = nullptr;
    glw::GLuint *dst_cbuf                 = nullptr;
    glw::GLuint *dst_dbuf                 = nullptr;
    glw::GLuint *dst_sbuf                 = nullptr;
    glw::GLuint same_read_and_draw_buffer = 0;
};

struct MultisampleColorConfig
{
    glw::GLint internal_format     = 0;
    glw::GLenum format             = 0;
    glw::GLenum type               = 0;
    glw::GLuint color_channel_bits = 0;
    glw::GLboolean bIsFloat        = 0;
};

struct DepthConfig
{
    glw::GLenum internal_format = 0;
    glw::GLenum format          = 0;
    glw::GLenum type            = 0;
    glw::GLenum attachment      = 0;
    glw::GLuint precisionBits   = 0;
};
} // namespace blt

/** Base class for below test cases which encapsulates shared functionality and resources */
class FramebufferBlitBaseTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    FramebufferBlitBaseTestCase(deqp::Context &context, const char *, const char *);

    virtual void deinit() override;
    virtual void init() override;

    bool GetBits(const glw::GLenum target, const glw::GLenum bits, glw::GLint *value);

    bool GetDefaultFramebufferBlitFormat(bool *noDepth, bool *noStencil);

    bool GetDrawbuffer32DepthComponentType(glw::GLint *value);

    template <typename... Args>
    void tcu_fail_msg(const std::string &format, Args... args);

    template <glw::GLenum E, glw::GLuint S, typename F>
    bool init_gl_objs(F f, const glw::GLuint count, const glw::GLuint *buf, const glw::GLint format);

    bool check_param(const glw::GLboolean, const char *);

    bool clearColorBuffer(const glw::GLuint, const glw::GLenum, const glw::GLenum, const glw::GLuint,
                          const blt::Color &, const blt::Rectangle &, const blt::Coord &, const glw::GLuint,
                          const bool);

    bool attachBufferToFramebuffer(const glw::GLenum, const glw::GLenum, const glw::GLenum, const glw::GLuint);

    bool getColor(const blt::Coord &, blt::Color *, const bool);

    bool checkColor(const blt::Color &, const blt::Color &, const glw::GLuint);

    glw::GLubyte floatToByte(const glw::GLfloat f);

    bool clearDepthBuffer(const glw::GLuint, const glw::GLenum, const glw::GLenum, const glw::GLuint, const glw::GLuint,
                          const blt::Depth, const blt::Rectangle &, const blt::Coord &);

    bool getDepth(const blt::Coord &, blt::Depth *, glw::GLuint *, const glw::GLuint, const glw::GLuint,
                  const blt::Rectangle &);

    glw::GLuint GetDepthPrecisionBits(const glw::GLenum depthInternalFormat);

    bool checkDepth(const blt::Depth actual, const blt::Depth expected, const glw::GLfloat epsilon);

    bool setupDefaultShader(glw::GLuint &vao, glw::GLuint &vbo);

    bool getStencil(const blt::Coord &, blt::Stencil *, const glw::GLuint, const glw::GLuint, const blt::Rectangle &);

    bool clearStencilBuffer(const glw::GLuint, const glw::GLenum, const glw::GLenum, const glw::GLuint,
                            const glw::GLuint, const blt::Stencil, const blt::Rectangle &, const blt::Coord &);

    bool checkStencil(const blt::Stencil actual, const blt::Stencil expected);

    bool setupRenderShader(glw::GLuint &vao, glw::GLuint &vbo, glw::GLint *uColor);

    void printGlobalBufferInfo();

protected:
    /* Private members */
    glw::GLuint m_fbos[2];

    glw::GLuint m_color_tbos[2];
    glw::GLuint m_depth_tbos[2];
    glw::GLuint m_stcil_tbos[2];
    glw::GLuint m_color_rbos[2];
    glw::GLuint m_depth_rbos[2];
    glw::GLuint m_stcil_rbos[2];

    glw::GLuint m_dflt;

    blt::Rectangle m_fullRect;
    tcu::IVec2 m_defaultCoord;

    static const glw::GLchar *m_default_vert_shader;
    static const glw::GLchar *m_default_frag_shader;

    static const glw::GLchar *m_render_vert_shader;
    static const glw::GLchar *m_render_frag_shader;

    std::map<std::string, std::string> specializationMap;

    glu::ShaderProgram *m_defaultProg;
    glu::ShaderProgram *m_renderProg;

    blt::MultiColorTestSetup m_setup;

    bool m_isContextES;

    glw::GLuint m_defaultFBO;

    bool m_cbfTestSupported;
    bool m_msTbosSupported;

    glw::GLint m_minDrawBuffers;
    glw::GLint m_minColorAttachments;

    std::vector<blt::BufferConfig> m_bufferCfg;
    std::vector<blt::DepthConfig> m_depthCfg;

    glw::GLuint m_depth_internalFormat;
    glw::GLuint m_depth_type;
    glw::GLuint m_depth_format;

    glw::GLuint m_stcil_internalFormat;
    glw::GLuint m_stcil_type;
    glw::GLuint m_stcil_format;
};

/* 4.15 Confirm that blits from multisampled to single sampled
 * framebuffers of various types are properly resolved.
 *
 * Initialize the full source framebuffer to a multicolor pattern
 * using each of color formats R8, RG8, RGB8, RGBA8, SRGB8_ALPHA8,
 * RGB10_A2, RGBA4, RGB5_A1, RGB565, R11F_G11F_B10F, RG16F, and R16F
 * and depth formats DEPTH_COMPONENT24, DEPTH_COMPONENT16,
 * DEPTH24_STENCIL8, DEPTH_COMPONENT32F, and DEPTH32F_STENCIL8 at
 * least once. Initialize the destination framebuffer to color black,
 * depth value zero, and stencil value zero. Blit the pixels from the
 * source to the destination using a color, depth, and stencil mask,
 * nearest filter, and source dimensions covering the complete
 * framebuffer size and equal source and destination dimensions. Read
 * four pixels from each of the corners of the lower left quadrant of
 * the destination. Confirm that the read color, depth, and stencil
 * values are as follows:
 *
 *       upper left:  red  , 0.25, 1
 *       upper right: green, 0.50, 2
 *       lower left:  blue , 0.75, 3
 *       lower right: white, 1.00, 4
 *
 * Procedure:
 *
 * Run a modified multicolor pattern test (see
 * runMultiColorPatternTest function) for the color and depth buffer
 * formats. Test runs through all the appropriate buffer
 * configurations (see BltBufferConfigs struct) of the default
 * framebuffer and user framebuffers (with the texture and
 * renderbuffer attachments) as read and draw framebuffers.
 *
 * Test is divided in two sections. First, blitting in color buffers
 * of various formats are verified without any depth buffers
 * attached. Then, blitting in depth buffers of various formats are
 * verified without any color buffers attached.
 *
 * Test prepares the color and depth (and depth_stencil) buffers to
 * the multicolor pattern, binds framebuffer objects for reading and
 * drawing, attaches the user-generated buffers to the framebuffers,
 * checks framebuffer status, blits from the read framebuffer to the
 * draw framebuffer, and finally checks the appropriate color, depth,
 * and stencil values.
 *
 * Notes:
 *
 * 1) Is it OK to test all formats separately so that missing buffers
 * are ignored? For example, by setting up only one
 * color/depth/stencil buffer without assigning any other buffers, and
 * letting glFramebufferBlit with color/depth/stencil bits to ignore
 * other buffers. Or should we test the color, depth and stencil
 * buffers in same glFramebufferBlit call?
 *
 * 2) Should we read pixels from the corners of the whole buffer
 * instead of the corners of the lower left quadrant?
 */

/** Test case which encapsulates blit multisampled to single sampled targets of
 *  all available formats */
class FramebufferBlitMultiToSingleSampledTestCase : public FramebufferBlitBaseTestCase
{
public:
    /* Public methods */
    FramebufferBlitMultiToSingleSampledTestCase(deqp::Context &context);

    void deinit() override;
    void init() override;
    tcu::TestNode::IterateResult iterate() override;

protected:
    template <glw::GLuint>
    bool testColorBlitConfig(const tcu::IVec2 &, const tcu::IVec2 &, const tcu::IVec2 &, const tcu::IVec2 &,
                             const glw::GLint);

    template <glw::GLuint>
    bool testDepthBlitConfig(const tcu::IVec2 &, const tcu::IVec2 &, const tcu::IVec2 &, const tcu::IVec2 &);

private:
    /* Private members */
    std::vector<blt::MultisampleColorConfig> m_multisampleColorCfg;
};

/* 4.14 Confirm that the scissor test is properly respected.
 *
 * Initialize the source framebuffer to color green, depth value one,
 * and stencil value one. Initialize the destination framebuffer to
 * color black, depth value zero, and stencil value zero. Define a
 * scissor box that encompasses the lower left quadrant of the
 * destination. Blit from the source to the destination using a color,
 * depth, and stencil mask, nearest filter, and equal source and
 * destination dimensions. Read four pixels from each of the corners
 * of the lower left quadrant of the destination. Confirm that the
 * lower left pixel is green with stencil and depth values of one, and
 * that the other pixels are all red with stencil and depth values of
 * zero.
 *
 * Procedure:
 *
 * Setup the MultiColorTestSetup struct with appropriate quadrant
 * rectangles, source and destination rectangles for blitting, scissor
 * rectangle, guadrant coordinates from which to verify the quadrant
 * pixel values, and expected quadrant color/depth/stencil
 * values. Then, run the multicolor pattern test with this setup.
 *
 * Notes:
 *
 * Should we read the pixels from the buffer corners instead of the
 * lower left quadrant corners? In addition, inconsistency between the
 * green source color and the expected red color.
 */

class FramebufferBlitScissorTestCase : public FramebufferBlitBaseTestCase
{
public:
    /* Public methods */
    FramebufferBlitScissorTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

    bool runMultiColorPatternTest(const glw::GLint);

private:
    /* Private members */
};

/** Test group which encapsulates all conformance tests */
class FramebufferBlitTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    FramebufferBlitTests(deqp::Context &context);

    void init();

private:
    FramebufferBlitTests(const FramebufferBlitTests &other);
    FramebufferBlitTests &operator=(const FramebufferBlitTests &other);
};

} // namespace glcts

#endif // _GLCFRAMEBUFFERBLITTESTS_HPP
