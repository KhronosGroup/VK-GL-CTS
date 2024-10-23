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
 * \file  glcTextureStencil8Tests.cpp
 * \brief Conformance tests for the stencil texture functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "glcTextureStencil8Tests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace glu;

namespace
{

// clang-format off
/** @brief Vertex shader source code to test vertex stencil8 texture implementation. */
const glw::GLchar* texture_stencil8_vert =
	R"(${VERSION}
    layout (location = 0) in vec4 inVertex;
    out highp vec2 texCoords;

    void main()
    {
        gl_Position = inVertex;
        texCoords = inVertex.xy * 0.5 + vec2(0.5);
    }
	)";

/** @brief Fragment shader source code to test fragment lookup texture stencil8 simple color. */
const glw::GLchar* texture_stencil8_simple_color_frag =
	R"(${VERSION}
	${PRECISION}

    layout (location = 0) out lowp vec4 fragColor;

    void main()
    {
        fragColor = vec4(0.0);
    }
)";

/** @brief Fragment shader source code to test fragment lookup texture stencil8. */
const glw::GLchar* texture_stencil8_frag =
	R"(${VERSION}
	${PRECISION}

    uniform lowp usampler2D stencilTex;
    in highp vec2 texCoords;

    layout (location = 0) out lowp vec4 fragColor;

    void main()
    {
        lowp uint s = texture(stencilTex, texCoords).r;
        switch (s)
        {
        case 0u:
            fragColor = vec4(1.0, 0.0, 0.0, 1.0); break;
        case 64u:
            fragColor = vec4(0.0, 1.0, 0.0, 1.0); break;
        case 128u:
            fragColor = vec4(0.0, 0.0, 1.0, 1.0); break;
        case 255u:
            fragColor = vec4(1.0, 1.0, 1.0, 1.0); break;
        default:
            fragColor = vec4(0.0, 0.0, 0.0, 1.0); break;
        }
    }
)";

/** @brief Fragment shader source code to test fragment lookup texture stencil8 multisample. */
const glw::GLchar* texture_stencil8_multisample_frag =
    R"(${VERSION}

#if defined(GL_OES_texture_storage_multisample_2d_array)
#extension GL_OES_texture_storage_multisample_2d_array : require
    uniform lowp usampler2DMSArray  stencilTexArray;
#endif

    ${PRECISION}

    uniform lowp usampler2DMS  stencilTex;
    uniform int textureType;

    layout (location = 0) out lowp vec4 fragColor;

    void main()
    {
        //sample the lower left texel,  first 4 samples
        lowp uint s0 = 1u, s1 = 1u, s2 = 1u, s3 = 1u;
        if (textureType == 0) {
            s0 = texelFetch(stencilTex, ivec2(0), 0).r;
            s1 = texelFetch(stencilTex, ivec2(0), 1).r;
            s2 = texelFetch(stencilTex, ivec2(0), 2).r;
            s3 = texelFetch(stencilTex, ivec2(0), 3).r;
        }
#if defined(GL_OES_texture_storage_multisample_2d_array)
        else {
            // hardcoded to sample from layer 1
            s0 = texelFetch(stencilTexArray, ivec3(0, 0, 1), 0).r;
            s1 = texelFetch(stencilTexArray, ivec3(0, 0, 1), 1).r;
            s2 = texelFetch(stencilTexArray, ivec3(0, 0, 1), 2).r;
            s3 = texelFetch(stencilTexArray, ivec3(0, 0, 1), 3).r;
        }
#endif

        fragColor = (s0 == 0u && s1 == 64u && s2 == 128u && s3 == 255u)  ?
                                vec4(0.0, 1.0,  0.0,  1.0) : // green for success
                                vec4(1.0, 0.0,  0.0,  1.0);  // red for failure
    }
	)";
// clang-format on

constexpr glw::GLenum g_multisampleTexTargets[2] = {GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE_ARRAY};
constexpr size_t g_maxMultisampleTexTargets = (sizeof(g_multisampleTexTargets) / sizeof(g_multisampleTexTargets[0]));
constexpr GLubyte g_stencilRef[4]           = {0, 64, 128, 255};
constexpr unsigned g_numLayers              = 3;

glw::GLint g_numMultisampleTexTargets =
    1; // will be raised to 2 depending on support for OES_texture_storage_multisample_2d_array

} // namespace

namespace glcts
{

/** Constructor.
 *
 *  @param context     Rendering context
 */
TextureMultisampledStencilTestCase::TextureMultisampledStencilTestCase(deqp::Context &context)
    : TestCase(context, "multisample", "Verifies rendering to multisampled stencil texture functionality")
    , m_vao(0)
    , m_vbo(0)
    , m_isContextES(false)
    , m_testSupported(false)
{
}

/** Stub deinit method. */
void TextureMultisampledStencilTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void TextureMultisampledStencilTestCase::init()
{
    const glu::RenderContext &renderContext = m_context.getRenderContext();
    glu::GLSLVersion glslVersion            = glu::getContextTypeGLSLVersion(renderContext.getType());
    m_isContextES                           = glu::isContextTypeES(renderContext.getType());
    const glw::Functions &gl                = m_context.getRenderContext().getFunctions();

    specializationMap["VERSION"]   = glu::getGLSLVersionDeclaration(glslVersion);
    specializationMap["PRECISION"] = "";

    auto contextType = m_context.getRenderContext().getType();
    if (m_isContextES)
    {
        specializationMap["PRECISION"] = "precision highp float;";
        m_testSupported                = glu::contextSupports(contextType, glu::ApiType::es(3, 1)) &&
                          m_context.getContextInfo().isExtensionSupported("GL_OES_texture_stencil8");

        if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_storage_multisample_2d_array"))
        {
            g_numMultisampleTexTargets = 2; // Incrementing this value will create 2D multisample arrays
        }
    }
    else
    {
        m_testSupported = glu::contextSupports(contextType, glu::ApiType::core(4, 4));
    }

    auto make_program = [&](const char *vs, const char *fs)
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

    m_stencilToColorProgram.reset(make_program(texture_stencil8_vert, texture_stencil8_frag));
    m_simpleColorProgram.reset(make_program(texture_stencil8_vert, texture_stencil8_simple_color_frag));
    m_checkStencilSampleProgram.reset(make_program(texture_stencil8_vert, texture_stencil8_multisample_frag));
}

bool TextureMultisampledStencilTestCase::testCreateTexturesMultisample()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint textures[g_maxMultisampleTexTargets];
    bool returnvalue = createTexturesTexStorageMultisample(textures, 0);

    gl.deleteTextures(g_numMultisampleTexTargets, textures);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

    return returnvalue;
}

/* numSamples = 0 will create a texture with MAX_SAMPLES samples */
bool TextureMultisampledStencilTestCase::createTexturesTexStorageMultisample(GLuint *textures, int numSamples)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    memset(textures, 0, sizeof(GLuint) * g_maxMultisampleTexTargets);

    for (unsigned t = 0; t < (unsigned)g_numMultisampleTexTargets; ++t)
    {
        textures[t] = createForTargetTexStorageMultisample(g_multisampleTexTargets[t], numSamples);
        success     = success && textures[t];
    }
    if (!success)
    {
        gl.deleteTextures(g_numMultisampleTexTargets, textures);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    return success;
}

/* numSamples = 0 will create a texture with MAX_SAMPLES samples */
GLuint TextureMultisampledStencilTestCase::createForTargetTexStorageMultisample(GLenum target, GLint numSamples)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint maxSamples         = 0;
    GLint maxSamplesIfq      = 0;
    GLuint tex               = 0;

    gl.getIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxSamples);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    gl.getInternalformativ(target, GL_STENCIL_INDEX8, GL_SAMPLES, 1, &maxSamplesIfq);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getInternalformativ");

    if (maxSamples > maxSamplesIfq)
    {
        TCU_FAIL(
            "The max GL_SAMPLES for GL_STENCIL_INDEX8 must be greater than or equal to GL_MAX_DEPTH_TEXTURE_SAMPLES.");
    }

    if (!numSamples)
        numSamples = maxSamples;

    gl.genTextures(1, &tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

    gl.bindTexture(target, tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

    switch (target)
    {
    case GL_TEXTURE_2D_MULTISAMPLE:
        gl.texStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_STENCIL_INDEX8, 2, 2, GL_TRUE);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2DMultisample");
        break;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        gl.texStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, numSamples, GL_STENCIL_INDEX8, 2, 2, 3, GL_TRUE);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage3DMultisample");
        break;
    default:
        gl.deleteTextures(1, &tex);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
        TCU_FAIL("Creating multisample stencil texture failed - unsupported texture target");
    }

    return tex;
}

void TextureMultisampledStencilTestCase::attachStencilTexture(GLenum fboTarget, GLenum texTarget, GLuint texture)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    switch (texTarget)
    {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_2D_MULTISAMPLE:
        gl.framebufferTexture2D(fboTarget, GL_STENCIL_ATTACHMENT, texTarget, texture, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");
        break;
    case GL_TEXTURE_CUBE_MAP:
        gl.framebufferTexture2D(fboTarget, GL_STENCIL_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, texture, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");
        break;
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        gl.framebufferTextureLayer(fboTarget, GL_STENCIL_ATTACHMENT, texture, 0, 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTextureLayer");
        break;
    default:
        TCU_FAIL("unsupported texture target");
    }
}

void TextureMultisampledStencilTestCase::fillStencilSamplePattern(GLuint program)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    unsigned s               = 0;

    gl.stencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "stencilOp");

    gl.enable(GL_STENCIL_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    gl.clearStencil(1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearStencil");

    gl.clear(GL_STENCIL_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    gl.enable(GL_SAMPLE_MASK);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    gl.viewport(0, 0, 2, 2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

    // initialize samples 0...3 to known stencil values
    s = 0;
    for (; s < 4; ++s)
    {
        gl.sampleMaski(0, 1 << s);
        GLU_EXPECT_NO_ERROR(gl.getError(), "sampleMaski");

        gl.stencilFunc(GL_ALWAYS, g_stencilRef[s], ~0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "stencilFunc");

        drawScreenQuad(program);
    }

    gl.disable(GL_SAMPLE_MASK);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    gl.disable(GL_STENCIL_TEST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disable");

    gl.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    GLU_EXPECT_NO_ERROR(gl.getError(), "stencilOp");

    gl.clearStencil(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearStencil");
}

bool TextureMultisampledStencilTestCase::checkMultisampledPattern(GLenum texTarget, GLuint texture,
                                                                  GLuint stencilSampleToColorProg)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint color             = 0;

    // Read back the MS texture samples and compare them against
    // the known pattern. The shader will write green, if the per-sample values
    // match, red otherwise.
    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    gl.bindTexture(texTarget, texture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

    gl.viewport(0, 0, 1, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    drawScreenQuad(stencilSampleToColorProg);

    color = 0;
    gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (GLuint *)&color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

    m_testCtx.getLog() << tcu::TestLog::Message
                       << "TextureMultisampledStencilTestCase::CheckMultisampledPattern: read back color: 0x"
                       << std::hex << color << tcu::TestLog::EndMessage;

    return (color == 0xFF00FF00); // green indicates success
}

bool TextureMultisampledStencilTestCase::drawScreenQuad(GLuint program)
{
    const glw::Functions &gl     = m_context.getRenderContext().getFunctions();
    GLint locVertices            = 0;
    const GLfloat quadVertices[] = {1.0, -1.0, 1.0, 1.0, -1.0, -1.0, -1.0, 1.0};

    gl.genVertexArrays(1, &m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");

    gl.bindVertexArray(m_vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    gl.genBuffers(1, &m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

    gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

    gl.bufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    gl.useProgram(program);
    locVertices = gl.getAttribLocation(program, "inVertex");
    if (locVertices == -1)
    {
        TCU_FAIL("TextureMultisampledStencilTestCase::DrawScreenQuad shader does not have vertex input.");
    }

    gl.vertexAttribPointer(locVertices, 2, GL_FLOAT, GL_FALSE, 0, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");

    gl.enableVertexAttribArray(locVertices);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

    gl.disableVertexAttribArray(locVertices);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

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

    return true;
}

GLuint TextureMultisampledStencilTestCase::createForTargetTexImage(GLenum target)
{
    return createForTargetTexImageWithType(target, GL_UNSIGNED_BYTE);
}

GLuint TextureMultisampledStencilTestCase::createForTargetTexImageWithType(GLenum target, GLenum type)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint tex               = 0;

    gl.genTextures(1, &tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

    gl.bindTexture(target, tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

    switch (target)
    {
    case GL_TEXTURE_2D:
        gl.texImage2D(GL_TEXTURE_2D, 0, GL_STENCIL_INDEX8, 2, 2, 0, GL_STENCIL_INDEX, type, NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
        break;
    case GL_TEXTURE_2D_ARRAY:
        gl.texImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_STENCIL_INDEX8, 2, 2, g_numLayers, 0, GL_STENCIL_INDEX, type, NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage3D");
        break;
    case GL_TEXTURE_CUBE_MAP:
    {
        for (unsigned f = 0; f < 6; ++f)
        {
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_STENCIL_INDEX8, 2, 2, 0, GL_STENCIL_INDEX, type,
                          NULL);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
        }
    }
    break;
    default:
        gl.deleteTextures(1, &tex);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
        TCU_FAIL("unsupported texture target");
    }

    gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

    gl.texParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

    return tex;
}

bool TextureMultisampledStencilTestCase::testRenderToMultisampledStencilTexture()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    GLuint stencilTex;
    bool returnvalue = true;
    GLenum status;
    GLuint color;
    GLuint fbos[3] = {0, 0, 0};
    int t;

    gl.genFramebuffers(3, fbos);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");

    stencilTex = createForTargetTexImage(GL_TEXTURE_2D);
    gl.bindFramebuffer(GL_FRAMEBUFFER, fbos[2]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    attachStencilTexture(GL_FRAMEBUFFER, GL_TEXTURE_2D, stencilTex);
    if (GL_FRAMEBUFFER_COMPLETE != gl.checkFramebufferStatus(GL_FRAMEBUFFER))
    {
        TCU_FAIL("Unexpected FBO status");
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");

    gl.clearStencil(1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearStencil");

    gl.clear(GL_STENCIL_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    for (t = 0; t < g_numMultisampleTexTargets; ++t)
    {
        GLuint textures[2] = {0, 0};
        GLenum texTarget   = g_multisampleTexTargets[t];

        textures[0] = createForTargetTexStorageMultisample(texTarget, 4);
        returnvalue &= textures[0] != 0;

        gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

        attachStencilTexture(GL_DRAW_FRAMEBUFFER, texTarget, textures[0]);
        status = gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::ostringstream sstr;
            sstr << "Multisampled stencil attachment causes incomplete framebuffer, status: 0x" << std::hex << status
                 << ".\n";
            TCU_FAIL(sstr.str());
        }

        if (!m_isContextES)
        {
            textures[1] = createForTargetTexStorageMultisample(texTarget, 4);
            returnvalue &= textures[1] != 0;

            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbos[1]);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            attachStencilTexture(GL_READ_FRAMEBUFFER, texTarget, textures[1]);

            status = gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER);
            GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");

            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                std::ostringstream sstr;
                sstr << "Multisampled stencil attachment causes incomplete framebuffer, status: 0x" << std::hex
                     << status << ".\n";
                TCU_FAIL(sstr.str());
            }
        }

        // make shader use right sampler
        gl.useProgram(m_checkStencilSampleProgram->getProgram());
        GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

        auto tex_type_loc = gl.getUniformLocation(m_checkStencilSampleProgram->getProgram(), "textureType");
        GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

        if (tex_type_loc < 0)
        {
            TCU_FAIL("unkown textureType location");
        }

        gl.uniform1i(tex_type_loc, t);
        GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");

        switch (t)
        {
        case 0:
        {
            GLint stencil_tex_loc = gl.getUniformLocation(m_checkStencilSampleProgram->getProgram(), "stencilTex");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

            if (stencil_tex_loc < 0)
            {
                TCU_FAIL("unkown stencilTex location");
            }
            gl.uniform1i(stencil_tex_loc, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");

            if (g_numMultisampleTexTargets > 1)
            {
                GLint stencil_tex_array_loc =
                    gl.getUniformLocation(m_checkStencilSampleProgram->getProgram(), "stencilTexArray");
                GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

                if (stencil_tex_array_loc < 0)
                {
                    TCU_FAIL("unkown stencilTexArray location");
                }
                gl.uniform1i(stencil_tex_array_loc, 1);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");
            }
        }
        break;
        case 1:
        {
            GLint stencil_tex_loc = gl.getUniformLocation(m_checkStencilSampleProgram->getProgram(), "stencilTex");
            GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

            if (stencil_tex_loc < 0)
            {
                TCU_FAIL("unkown stencilTex location");
            }

            gl.uniform1i(stencil_tex_loc, 1);
            GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");

            if (g_numMultisampleTexTargets > 1)
            {
                GLint stencil_tex_array_loc =
                    gl.getUniformLocation(m_checkStencilSampleProgram->getProgram(), "stencilTexArray");
                GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

                if (stencil_tex_array_loc < 0)
                {
                    TCU_FAIL("unkown stencilTexArray location");
                }

                gl.uniform1i(stencil_tex_array_loc, 0);
                GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");
            }
        }
        break;
        default:
            break;
        }

        // first try rendering to a multisampled texture and verify the result
        fillStencilSamplePattern(m_simpleColorProgram->getProgram());
        returnvalue &= checkMultisampledPattern(texTarget, textures[0], m_checkStencilSampleProgram->getProgram());

        if (!m_isContextES)
        {
            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            // now test blitting between MS stencil textures
            gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            gl.clearStencil(1);
            GLU_EXPECT_NO_ERROR(gl.getError(), "clearStencil");

            gl.clear(GL_STENCIL_BUFFER_BIT);
            GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

            gl.blitFramebuffer(0, 0, 2, 2, 0, 0, 2, 2, GL_STENCIL_BUFFER_BIT, GL_NEAREST);
            GLU_EXPECT_NO_ERROR(gl.getError(), "blitFramebuffer");

            returnvalue &= checkMultisampledPattern(texTarget, textures[1], m_checkStencilSampleProgram->getProgram());

            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbos[1]);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            // now test MS stencil texture resolve blits
            gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[2]);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
        }
        else
        {
            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

            // now test MS stencil texture resolve blits
            gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[2]);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
        }

        gl.clearStencil(1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clearStencil");

        gl.clear(GL_STENCIL_BUFFER_BIT);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

        gl.blitFramebuffer(0, 0, 2, 2, 0, 0, 2, 2, GL_STENCIL_BUFFER_BIT, GL_NEAREST);
        GLU_EXPECT_NO_ERROR(gl.getError(), "blitFramebuffer");

        // turn resolved stencil value into a color and read the color back.
        gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

        gl.bindTexture(GL_TEXTURE_2D, stencilTex);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

        gl.viewport(0, 0, 1, 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

        gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

        gl.clear(GL_COLOR_BUFFER_BIT);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

        drawScreenQuad(m_stencilToColorProgram->getProgram());
        color = 0;

        gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (GLuint *)&color);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        m_testCtx.getLog() << tcu::TestLog::Message
                           << "TextureMultisampledStencilTestCase::TestRenderToMultisampledStencilTexture: read back "
                              "color after resolve: 0x"
                           << std::hex << color << tcu::TestLog::EndMessage;

        // the resolve blit might have chosen any of the samples
        returnvalue &= (color == 0xFF0000FF) || (color == 0xFF00FF00) || (color == 0xFFFF0000) || (color == 0xFFFFFFFF);

        gl.deleteTextures(m_isContextES ? 1 : 2, textures);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");
    }

    gl.deleteTextures(1, &stencilTex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

    gl.deleteFramebuffers(3, fbos);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    return returnvalue;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TextureMultisampledStencilTestCase::iterate()
{
    if (!m_testSupported)
    {
        throw tcu::NotSupportedError("Test TextureMultisampledStencilTestCase.is not supported");
        return STOP;
    }

    bool ret = true;

    ret &= testCreateTexturesMultisample();
    ret &= testRenderToMultisampledStencilTexture();

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
TextureStencil8Tests::TextureStencil8Tests(deqp::Context &context)
    : TestCaseGroup(context, "texture_stencil8", "Verify conformance of stencil texture functionality")
{
}

/** Initializes the test group contents. */
void TextureStencil8Tests::init()
{
    addChild(new TextureMultisampledStencilTestCase(m_context));
}

} // namespace glcts
