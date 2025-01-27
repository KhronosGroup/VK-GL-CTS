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
 * \file  gl3cDrawBuffers.cpp
 * \brief Conformance tests for the DrawBuffers functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "gl3cDrawBuffers.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace glu;

namespace
{
// clang-format off

/* full screen quad */
const float quad[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
    1.0f, -1.0f, 0.0f, 1.0f,
    -1.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 1.0f
};

const GLubyte expected_results[][4] =
{
    { 255, 255, 255, 255 },
    { 127, 127, 127, 127 },
    { 255, 255, 255, 255 },
    { 255, 255, 255, 255 },
    { 255, 255, 255, 255 },
    { 255, 255, 255, 255 },
    { 255, 255, 255, 255 },
    { 255, 255, 255, 255 },
};

/** @brief Vertex shader source code to test vectors DrawBuffers functionality. */
const glw::GLchar *vert_shader_src =
    R"(${VERSION}
    in vec4 vertex;
    void main (void)
    {
        gl_Position = vertex;
    }
    )";

/** @brief Fragment shader source code to test vectors DrawBuffers functionality. */
const glw::GLchar *frag_shader_src =
    R"(${VERSION}
    ${EXTENSION}
${ATTACHMENTS}

    void main (void)
    {
${OUTPUT}
    }
    )";

// clang-format on
} // anonymous namespace

namespace gl3cts
{

/** Constructor.
 *
 *  @param context     Rendering context
 */
DrawBuffersTestCase::DrawBuffersTestCase(deqp::Context &context)
    : TestCase(context, "draw_buffers_1", "Verifies writing to all DrawBuffers except for NONE functionality")
    , m_vao(0)
    , m_vbo(0)
{
}

/** Stub deinit method. */
void DrawBuffersTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void DrawBuffersTestCase::init()
{
    glu::GLSLVersion glslVersion = glu::getContextTypeGLSLVersion(m_context.getRenderContext().getType());

    specializationMap["VERSION"]   = glu::getGLSLVersionDeclaration(glslVersion);
    specializationMap["EXTENSION"] = "";
}

bool DrawBuffersTestCase::setupProgram(const glw::GLint numAttachments)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    auto contextType         = m_context.getRenderContext().getType();

    const glu::ContextInfo &contextInfo = m_context.getContextInfo();
    bool exp_ext_supported              = contextInfo.isExtensionSupported("GL_ARB_explicit_attrib_location");
    if (glu::contextSupports(contextType, glu::ApiType::core(3, 3)) || exp_ext_supported)
    {
        if (exp_ext_supported)
            specializationMap["EXTENSION"] = "#extension GL_ARB_explicit_attrib_location : enable";

        std::ostringstream attachments, output;
        for (GLint i = 0; i < numAttachments; i++)
        {
            attachments << "layout(location = " << i << ") out vec4 fragColor" << i << ";\n";
            output << "        fragColor" << i << " = vec4(1.0, 1.0, 1.0, 1.0);\n";
        }

        specializationMap["ATTACHMENTS"] = attachments.str();
        specializationMap["OUTPUT"]      = output.str();
    }
    else if ((contextType.getFlags() & CONTEXT_FORWARD_COMPATIBLE) == 0)
    {
        specializationMap["ATTACHMENTS"] = "";
        specializationMap["OUTPUT"]      = "        gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n";
    }
    else
    {
        specializationMap["ATTACHMENTS"] = "out vec4 fragColor;";
        specializationMap["OUTPUT"]      = "        fragColor = vec4(1.0, 1.0, 1.0, 1.0);\n";
    }

    /* Building basic program. */
    std::string vert_shader = tcu::StringTemplate(vert_shader_src).specialize(specializationMap);
    std::string frag_shader = tcu::StringTemplate(frag_shader_src).specialize(specializationMap);

    ProgramSources sources = makeVtxFragSources(vert_shader, frag_shader);

    m_program.reset(new glu::ShaderProgram(gl, sources));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
                           << "Vertex: " << m_program->getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
                           << vert_shader << "\n"
                           << "Fragment: " << m_program->getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
                           << frag_shader << "\n"
                           << "Program: " << m_program->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
        TCU_FAIL("Invalid program");
    }
    return true;
}

bool DrawBuffersTestCase::checkResults(const int DB, const bool bCheckNone)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool ret                 = true;

    GLubyte readcolor[4] = {0, 0, 0, 0};

    for (GLint i = 0; i < DB; ++i)
    {
        for (GLint j = 0; j < 4; ++j)
        {
            readcolor[j] = 0;
        }

        GLint ca = 0;
        gl.getIntegerv(GL_DRAW_BUFFER0 + i, &ca);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

        if (ca == GL_NONE)
        {
            if (bCheckNone)
            {
                ca = GL_COLOR_ATTACHMENT0 + i;
            }
            else
            {
                continue;
            }
        }
        gl.readBuffer(ca);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readBuffer");

        gl.readPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, readcolor);
        GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");

        for (GLint j = 0; j < 4; ++j)
        {
            if (fabs(static_cast<GLint>(readcolor[j]) - static_cast<GLint>(expected_results[i % 8][j])) > 1)
            {
                ret = false;
                m_testCtx.getLog() << tcu::TestLog::Message << "DrawBuffersTestCase::checkResults: Color attachment  \n"
                                   << i << ", component " << j << ": " << readcolor[j]
                                   << " != " << expected_results[i % 8][j] << tcu::TestLog::EndMessage;
            }
        }
    }

    return ret;
}

bool DrawBuffersTestCase::applyFBO(GL3FBO &fbo)
{
    bool result = true;
    GLenum framebufferStatus;
    GL3FBOAttachment *att = nullptr;

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.genFramebuffers(1, &fbo.object);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");

    gl.bindFramebuffer(fbo.target, fbo.object);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");

    /* Attach renderbuffers or textures. */
    for (GLint i = 0; i < MAX_ATTACHMENTS; i++)
    {
        att = (GL3FBOAttachment *)&fbo.attachments[i];
        if (0 != att->object)
        {
            switch (att->target)
            {
            case GL_RENDERBUFFER:
            {
                gl.framebufferRenderbuffer(fbo.target, att->attachment, att->target, att->object);
                GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");
                break;
            }

            case GL_TEXTURE_2D:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            // !GLES
            case GL_PROXY_TEXTURE_2D:
            case GL_PROXY_TEXTURE_CUBE_MAP:
            {
                gl.framebufferTexture2D(fbo.target, att->attachment, att->target, att->object, att->level);
                GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");
                break;
            }

            case GL_TEXTURE_3D:
            // !GLES
            /* TODO: symbio: tsone: add check for proxy texture extension */
            case GL_PROXY_TEXTURE_3D:
            {
                result = false;
                m_testCtx.getLog() << tcu::TestLog::Message
                                   << "DrawBuffersTestCase::applyFBO: texture target at index \n"
                                   << i << ". This feature is not supported yet." << tcu::TestLog::EndMessage;
                break;
            }

            default:
            {
                result = false;
                m_testCtx.getLog() << tcu::TestLog::Message << "Unsupported attachment target \n"
                                   << "0x" << std::hex << att->attachment << " at attachment index " << i << "."
                                   << tcu::TestLog::EndMessage;
                break;
            }
            }
        }
    }

    /* Check FBO completeness. */
    framebufferStatus = gl.checkFramebufferStatus(fbo.target);
    GLU_EXPECT_NO_ERROR(gl.getError(), "checkFramebufferStatus");

    static const std::map<GLenum, std::string> statusNames = {
        {GL_FRAMEBUFFER_COMPLETE, "GL_FRAMEBUFFER_COMPLETE"},
        {GL_FRAMEBUFFER_UNDEFINED, "GL_FRAMEBUFFER_UNDEFINED"},
        {GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"},
        {GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"},
        {GL_FRAMEBUFFER_UNSUPPORTED, "GL_FRAMEBUFFER_UNSUPPORTED"},
        {GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"},
        {GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS, "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS"}};

    if (GL_FRAMEBUFFER_COMPLETE != framebufferStatus)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Framebuffer status is  \n"
                           << statusNames.at(framebufferStatus).c_str() << "." << tcu::TestLog::EndMessage;
        result = false;
    }

    return result;
}

bool DrawBuffersTestCase::setupBuffers(const GLint maxColAtt, GL3RBO *rbo, GL3FBO &fbo)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    /*
    // Create CA renderbuffers.
    */
    for (GLint i = 0; i < maxColAtt; ++i)
    {
        rbo[i].target         = GL_RENDERBUFFER;
        rbo[i].object         = 0;
        rbo[i].width          = 4;
        rbo[i].height         = 4;
        rbo[i].internalformat = GL_RGBA8;

        gl.genRenderbuffers(1, &rbo[i].object);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");

        gl.bindRenderbuffer(rbo->target, rbo[i].object);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");

        gl.renderbufferStorage(rbo[i].target, rbo[i].internalformat, rbo[i].width, rbo[i].height);
        GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
    }

    fbo.target = GL_FRAMEBUFFER;
    fbo.object = 0;

    /*
    // Setup and bind an FBO with these renderbuffers attached.
    */
    for (GLint i = 0; i < maxColAtt; ++i)
    {
        fbo.attachments[i].target     = GL_RENDERBUFFER;
        fbo.attachments[i].attachment = GL_COLOR_ATTACHMENT0 + i;
        fbo.attachments[i].object     = rbo[i].object;
    }

    return applyFBO(fbo);
}

void DrawBuffersTestCase::deleteBuffers(const int CA, GL3RBO *rbo, GL3FBO &fbo)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.deleteFramebuffers(1, &fbo.object);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    gl.bindFramebuffer(fbo.target, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    for (GLint i = 0; i < CA; ++i)
    {
        gl.deleteRenderbuffers(1, &rbo[i].object);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");

        gl.bindRenderbuffer(rbo[i].target, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
    }
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult DrawBuffersTestCase::iterate()
{
    bool result = true;

    GLint maxColorAttachments = 0, maxDrawbuffers = 0;

    GL3FBO fbo;
    GL3RBO rbo[MAX_ATTACHMENTS];

    GLenum drawbuffers[MAX_ATTACHMENTS];

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.viewport(0, 0, 4, 4);
    GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

    gl.getIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawbuffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    gl.getIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    if (maxDrawbuffers <= 0 || maxColorAttachments <= 0)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    if (maxDrawbuffers < 2 || maxColorAttachments < 2)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
        return STOP;
    }

    maxDrawbuffers = std::min(maxDrawbuffers, 8);

    GLint numAttachments = std::min(maxDrawbuffers, maxColorAttachments);

    if (!setupProgram(numAttachments))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    if (!setupBuffers(maxColorAttachments, rbo, fbo))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    for (GLint i = 0; i < maxDrawbuffers; ++i)
    {
        drawbuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        if (drawbuffers[i] >= GL_COLOR_ATTACHMENT0 + (GLuint)maxColorAttachments)
        {
            drawbuffers[i] = GL_NONE;
        }
    }

    gl.drawBuffers(maxDrawbuffers, drawbuffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawBuffers");

    float clrv = 0.502f;
    gl.clearColor(clrv, clrv, clrv, clrv);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

    // Setup DrawBuffers(maxDrawbuffers, buffers) where
    //  buffers[i] = COLOR_ATTACHMENT0 + i for i = { 0, 2..maxDrawbuffers-1 }
    //  buffers[1] = NONE
    drawbuffers[1] = GL_NONE;

    gl.drawBuffers(maxDrawbuffers, drawbuffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawBuffers");

    /*
    // Draw a triangle covering some reasonable fraction of the
    // color buffer
    */
    {
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

        gl.useProgram(m_program->getProgram());
        GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

        locVertices = gl.getAttribLocation(m_program->getProgram(), "vertex");
        GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");
        if (locVertices != -1)
        {
            gl.vertexAttribPointer(locVertices, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
            GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");

            gl.enableVertexAttribArray(locVertices);
            GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
        }

        gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");
    }

    result = checkResults(maxDrawbuffers <= maxColorAttachments ? maxDrawbuffers : maxColorAttachments, true);

    deleteBuffers(maxColorAttachments, rbo, fbo);

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
DrawBuffersTests::DrawBuffersTests(deqp::Context &context)
    : TestCaseGroup(context, "draw_buffers", "Verify conformance of DrawBuffers implementation")
{
}

/** Initializes the test group contents. */
void DrawBuffersTests::init()
{
    addChild(new DrawBuffersTestCase(m_context));
}

} // namespace gl3cts
