#ifndef _GL4CES31COMPATIBILITYTESTS_HPP
#define _GL4CES31COMPATIBILITYTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2015-2016 The Khronos Group Inc.
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
 * \file  gl4cES31CompatibilityTests.hpp
 * \brief Conformance tests for ES3.1 Compatibility feature functionality.
 */ /*------------------------------------------------------------------------------*/

/* Includes. */

#include "glcTestCase.hpp"
#include "glcTestSubcase.hpp"
#include "gluShaderUtil.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "glcShaderImageLoadStoreTests.hpp"
#include "glcSampleVariablesTests.hpp"
#include "glcShaderStorageBufferObjectTests.hpp"

/* Interface. */

namespace gl4cts
{
namespace es31compatibility
{
/** @class Tests
 *
 *  @brief OpenGL ES 3.1 Compatibility Tests Suite.
 */
class Tests : public deqp::TestCaseGroup
{
public:
    /* Public member functions. */
    Tests(deqp::Context &context);

    void init();

private:
    /* Private member functions. */
    Tests(const Tests &other);
    Tests &operator=(const Tests &other);
};

/* Tests class */

/** @class ShaderCompilationCompatibilityTests
 *
 *  @brief Test verifies that vertex, fragment and compute shaders
 *         containing line "#version 310 es" compile without
 *         error.
 *
 *         Test verifies that fragment shader using gl_HelperInvocation
 *         variable compiles without errors.
 */
class ShaderCompilationCompatibilityTests : public deqp::TestCase
{
public:
    /* Public member functions. */
    ShaderCompilationCompatibilityTests(deqp::Context &context);

    virtual tcu::TestNode::IterateResult iterate();

private:
    /* Private member functions */
    ShaderCompilationCompatibilityTests(const ShaderCompilationCompatibilityTests &other);
    ShaderCompilationCompatibilityTests &operator=(const ShaderCompilationCompatibilityTests &other);

    /* Static member constants. */
    static const struct TestShader
    {
        glw::GLenum type;
        const glw::GLchar *type_name;
        const glw::GLchar *source;
    } s_shaders[]; //!< Test cases shaders.

    static const glw::GLsizei s_shaders_count; //!< Test cases shaders count.
};

/* ShaderCompilationCompatibilityTests */

/** @class ShaderFunctionalCompatibilityTests
 *
 *  @brief Test veryifies that GLSL mix(T, T, Tboolean) function
 *         works with int, uint and boolean types. Test is performed
 *         for highp, mediump and lowp precision qualifiers.
 */
class ShaderFunctionalCompatibilityTest : public deqp::TestCase
{
public:
    /* Public member functions. */
    ShaderFunctionalCompatibilityTest(deqp::Context &context);

    virtual tcu::TestNode::IterateResult iterate();

private:
    /* Private member variables. */
    glw::GLuint m_po_id;  //!< Program object name.
    glw::GLuint m_fbo_id; //!< Framebuffer object name.
    glw::GLuint m_rbo_id; //!< Renderbuffer object name.
    glw::GLuint m_vao_id; //!< Vertex Array Object name.

    /* Static member constants. */
    static const glw::GLchar *s_shader_version;       //!< Shader version string.
    static const glw::GLchar *s_vertex_shader_body;   //!< Vertex shader body template.
    static const glw::GLchar *s_fragment_shader_body; //!< Fragment shader body template.
    static const struct Shader
    {
        const glw::GLchar *vertex[3];
        const glw::GLchar *fragment[3];
    } s_shaders[];                             //!< Template parameter cases.
    static const glw::GLsizei s_shaders_count; //!< Number of template parameter cases.

    /* Private member functions */
    ShaderFunctionalCompatibilityTest(const ShaderFunctionalCompatibilityTest &other);
    ShaderFunctionalCompatibilityTest &operator=(const ShaderFunctionalCompatibilityTest &other);

    bool createProgram(const struct Shader shader_source);
    void createFramebufferAndVertexArrayObject();
    bool test();
    void cleanProgram();
    void cleanFramebufferAndVertexArrayObject();
};

} // namespace es31compatibility
} // namespace gl4cts

#endif // _GL4CES31COMPATIBILITYTESTS_HPP
