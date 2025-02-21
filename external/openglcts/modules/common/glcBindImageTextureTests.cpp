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
 * \file  glcBindImageTextureTests.cpp
 * \brief Conformance tests for binding texture to an image unit functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcBindImageTextureTests.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace glu;

namespace glcts
{

// clang-format off

/* full screen quad */
const float fs_quad[] = { -1.0f, -1.0f, 0.0f, 1.0f,
                          1.0f, -1.0f, 0.0f, 1.0f,
                          -1.0f, 1.0f, 0.0f, 1.0f,
                          1.0f, 1.0f,  0.0f, 1.0f };

/** @brief Vertex shader source code to test non-layered bindings of shader images. */
const glw::GLchar* glcts::BindImageTextureSingleLayerTestCase::m_shader_vert =
    R"(${VERSION}
    in vec4 vertex;
    void main()
    {
      gl_Position = vertex;
    }
    )";

/** @brief Fragment shader source code to test non-layered bindings of 2D shader images. */
const glw::GLchar* glcts::BindImageTextureSingleLayerTestCase::m_shader_frag =
    R"(${VERSION}
    ${PRECISION}

    layout(binding = 0, rgba8) uniform readonly highp image2D img;
    layout(location = 0) out vec4 color;
    void main()
    {
        color = imageLoad(img, ivec2(0, 0));
    }
    )";

/** @brief Fragment shader source code to test non-layered bindings of 1D shader images. */
const glw::GLchar* glcts::BindImageTextureSingleLayerTestCase::m_shader_1d_frag =
    R"(${VERSION}
    ${PRECISION}

    layout(binding = 0, rgba8) uniform image1D img;
    layout(location = 0) out vec4 color;

    void main(void)
    {
        color = imageLoad(img, 0);
    }
    )";

// clang-format on

/** Constructor.
 *
 *  @param context     Rendering context
 */
BindImageTextureSingleLayerTestCase::BindImageTextureSingleLayerTestCase(deqp::Context &context)
    : TestCase(context, "single_layer", "Verifies single layer texture bound to an image unit functionality")
    , m_vao(0)
    , m_vbo(0)
    , m_target(0)
    , m_fbo(0)
    , m_isContextES(false)
{
}

/** Stub deinit method. */
void BindImageTextureSingleLayerTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void BindImageTextureSingleLayerTestCase::init()
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
tcu::TestNode::IterateResult BindImageTextureSingleLayerTestCase::iterate()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    std::vector<GLenum> textures = {GL_TEXTURE_2D};
    if (!m_isContextES)
        textures.push_back(GL_TEXTURE_1D);

    bool ret = true;

    auto create_program = [&](const std::string &vert, const std::string &frag)
    {
        std::string vshader = tcu::StringTemplate(vert).specialize(specializationMap);
        std::string fshader = tcu::StringTemplate(frag).specialize(specializationMap);

        ProgramSources sources = makeVtxFragSources(vshader, fshader);
        return new ShaderProgram(gl, sources);
    };

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

    for (size_t n = 0; n < textures.size(); ++n)
    {
        GLenum tex_target = textures[n];

        if (m_isContextES && tex_target == GL_TEXTURE_1D)
            continue;

        ShaderProgram *program = nullptr;
        if (tex_target == GL_TEXTURE_2D)
        {
            program = create_program(m_shader_vert, m_shader_frag);
        }
        else if (tex_target == GL_TEXTURE_1D)
        {
            program = create_program(m_shader_vert, m_shader_1d_frag);
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
            /* set up rendering resources */
            setBuffers(*program);

            if (!drawAndVerify(tex_target))
            {
                m_testCtx.getLog() << tcu::TestLog::Message
                                   << "BindImageTextureSingleLayerTestCase::iterate failed for target :"
                                   << glu::getTextureTargetName(tex_target) << "\n"
                                   << tcu::TestLog::EndMessage;
                ret = false;
            }

            // Release resources
            if (program)
                delete program;

            releaseBuffers();
        }
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

    m_target = 0;
    m_fbo    = 0;

    if (ret)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

/* function activates the program that is given as a argument */
/* and sets vertex attribute */
void BindImageTextureSingleLayerTestCase::setBuffers(const glu::ShaderProgram &program)
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

        GLint locVertices = -1;

        gl.useProgram(program.getProgram());
        GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

        locVertices = gl.getAttribLocation(program.getProgram(), "vertex");
        GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");
        if (locVertices != -1)
        {
            gl.enableVertexAttribArray(0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

            GLuint strideSize = sizeof(fs_quad) / 4;

            gl.vertexAttribPointer(locVertices, 4, GL_FLOAT, GL_FALSE, strideSize, nullptr);
            GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
        }
    }
}

/* function releases vertex buffers */
void BindImageTextureSingleLayerTestCase::releaseBuffers()
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

bool BindImageTextureSingleLayerTestCase::drawAndVerify(const glw::GLenum tex_target)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    GLubyte data[]      = {128, 128, 128, 128};
    GLubyte read_data[] = {0, 0, 0, 0};

    GLuint tex = 0;
    bool ret   = true;
    gl.genTextures(1, &tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

    gl.bindTexture(tex_target, tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

    if (tex_target == GL_TEXTURE_2D)
    {
        gl.texStorage2D(tex_target, 1, GL_RGBA8, 1, 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage2D");

        gl.texSubImage2D(tex_target, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texSubImage2D");
    }
    else
    {
        gl.texStorage1D(tex_target, 1, GL_RGBA8, 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texStorage1D");

        gl.texSubImage1D(tex_target, 0, 0, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texSubImage1D");
    }

    gl.texParameteri(tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

    gl.texParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

    std::vector<std::pair<GLboolean, int>> bindImgParams = {{GL_TRUE, 1}, {GL_TRUE, 0}, {GL_FALSE, 1}, {GL_FALSE, 0}};

    for (auto it : bindImgParams)
    {
        gl.bindImageTexture(0, tex, 0, it.first, it.second, GL_READ_ONLY, GL_RGBA8);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindImageTexture");

        gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

        gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

        gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, read_data);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        if (memcmp(data, read_data, sizeof(data)))
        {
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "BindImageTextureSingleLayerTestCase::drawAndVerify unexpected result :"
                               << "glBindImageTexture( layered: " << glu::getBooleanName(it.first)
                               << ", layer: " << it.second << ")\n"
                               << tcu::TestLog::EndMessage;

            ret = false;
        }
    }

    gl.deleteTextures(1, &tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

    /* Result of comparison of two arrays are returned */
    return ret;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
BindImageTextureTests::BindImageTextureTests(deqp::Context &context)
    : TestCaseGroup(context, "bind_image_texture", "Verify conformance of glBindImageTexture functionality")
{
}

/** Initializes the test group contents. */
void BindImageTextureTests::init()
{
    addChild(new BindImageTextureSingleLayerTestCase(m_context));
}

} // namespace glcts
