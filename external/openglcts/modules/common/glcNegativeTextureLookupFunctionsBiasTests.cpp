/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \file  glcNegativeTextureLookupFunctionsBiasTests.cpp
 * \brief Implements conformance negative texture lookup functions bias tests
 */ /*-------------------------------------------------------------------*/

#include "glcNegativeTextureLookupFunctionsBiasTests.hpp"

#include "tcuStringTemplate.hpp"
#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"

namespace glcts
{

std::string GLES_GLSL_VER = "300 es";
std::string GL_GLSL_VER   = "400 core";

NegativeTextureLookupFunctionsBiasTest::NegativeTextureLookupFunctionsBiasTest(
    deqp::Context &context, const std::string &test_name, const std::string &test_description,
    const std::string vertex_shader_txt, bool texture_shadow_lod_required, bool texture_cube_map_array_required,
    bool sparse_texture2_required)
    : TestCase(context, test_name.c_str(), test_description.c_str())
    , m_vertex_shader_txt(vertex_shader_txt)
    , m_texture_shadow_lod_required(texture_shadow_lod_required)
    , m_texture_cube_map_array_required(texture_cube_map_array_required)
    , m_sparse_texture2_required(sparse_texture2_required)
{
    tcu::StringTemplate vertShaderTemplate{m_vertex_shader_txt};
    std::map<std::string, std::string> replacements;

    if (glu::isContextTypeGLCore(m_context.getRenderContext().getType()))
        replacements["VERSION"] = GL_GLSL_VER;
    else
        replacements["VERSION"] = GLES_GLSL_VER;

    m_vertex_shader_txt = vertShaderTemplate.specialize(replacements);
}

tcu::TestNode::IterateResult NegativeTextureLookupFunctionsBiasTest::iterate(void)
{
    bool texture_shadow_lod_supported     = false;
    bool texture_cube_map_array_supported = false;
    bool sparse_texture2_supported        = false;

    if (m_texture_shadow_lod_required)
    {
        texture_shadow_lod_supported = m_context.getContextInfo().isExtensionSupported("GL_EXT_texture_shadow_lod");
    }
    if (m_texture_cube_map_array_required)
    {
        if (glu::isContextTypeGLCore(m_context.getRenderContext().getType()))
        {
            texture_cube_map_array_supported =
                m_context.getContextInfo().isExtensionSupported("GL_ARB_texture_cube_map_array");
        }
        else if (glu::isContextTypeES(m_context.getRenderContext().getType()))
        {
            texture_cube_map_array_supported =
                m_context.getContextInfo().isExtensionSupported("GL_EXT_texture_cube_map_array");
        }
    }
    if (m_sparse_texture2_required)
    {
        if (glu::isContextTypeGLCore(m_context.getRenderContext().getType()))
        {
            sparse_texture2_supported = m_context.getContextInfo().isExtensionSupported("GL_ARB_sparse_texture2");
        }
    }
    if (m_texture_shadow_lod_required && !texture_shadow_lod_supported)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "texture_shadow_lod extension not supported");
        return STOP;
    }
    if (m_texture_cube_map_array_required && !texture_cube_map_array_supported)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "texture_cube_map_array extension not supported");
        return STOP;
    }
    if (m_sparse_texture2_required && !sparse_texture2_supported)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "sparse_texture2 extension not supported");
        return STOP;
    }

    bool is_ok = test();

    if (is_ok)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Expected vertex shader compilation fail");
    }

    return STOP;
}

bool NegativeTextureLookupFunctionsBiasTest::test()
{
    glu::Shader vs_shader(m_context.getRenderContext(), glu::SHADERTYPE_VERTEX);
    const char *const source = m_vertex_shader_txt.c_str();
    const int length         = m_vertex_shader_txt.size();
    vs_shader.setSources(1, &source, &length);
    vs_shader.compile();

    //Expecting compile error
    return !vs_shader.getCompileStatus();
}

NegativeTextureLookupFunctionsBiasTests::NegativeTextureLookupFunctionsBiasTests(deqp::Context &context)
    : TestCaseGroup(context, "negative_texture_lookup_functions_with_bias_tests",
                    "Negative testes texture lookup functions with bias")
{
}

const std::string texture_sampler1D_bias_vs = "#version ${VERSION}\n"
                                              "uniform highp ${gsampler}1D texSampler;\n"
                                              "uniform highp float bias;\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_Position = vec4(0.0);\n"
                                              "    ${gvec4} color = texture(texSampler, 0.0, bias);\n"
                                              "}\n";

const std::string texture_sampler2D_bias_vs = "#version ${VERSION}\n"
                                              "uniform highp ${gsampler}2D texSampler;\n"
                                              "uniform highp float bias;\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_Position = vec4(0.0);\n"
                                              "    ${gvec4} color = texture(texSampler, vec2(0.0), bias);\n"
                                              "}\n";

const std::string texture_sampler3D_bias_vs = "#version ${VERSION}\n"
                                              "uniform highp ${gsampler}3D texSampler;\n"
                                              "uniform highp float bias;\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_Position = vec4(0.0);\n"
                                              "    ${gvec4} color = texture(texSampler, vec3(0.0), bias);\n"
                                              "}\n";

const std::string texture_samplerCube_bias_vs = "#version ${VERSION}\n"
                                                "uniform highp ${gsampler}Cube texSampler;\n"
                                                "uniform highp float bias;\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    gl_Position = vec4(0.0);\n"
                                                "    ${gvec4} color = texture(texSampler, vec3(0.0), bias);\n"
                                                "}\n";

const std::string texture_sampler1DShadow_bias_vs = "#version ${VERSION}\n"
                                                    "uniform highp sampler1DShadow texSampler;\n"
                                                    "uniform highp float bias;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = vec4(0.0);\n"
                                                    "    float color = texture(texSampler, vec3(0.0), bias);\n"
                                                    "}\n";

const std::string texture_sampler2DShadow_bias_vs = "#version ${VERSION}\n"
                                                    "uniform highp sampler2DShadow texSampler;\n"
                                                    "uniform highp float bias;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = vec4(0.0);\n"
                                                    "    float color = texture(texSampler, vec3(0.0), bias);\n"
                                                    "}\n";

const std::string texture_samplerCubeShadow_bias_vs = "#version ${VERSION}\n"
                                                      "uniform highp samplerCubeShadow texSampler;\n"
                                                      "uniform highp float bias;\n"
                                                      "void main (void)\n"
                                                      "{\n"
                                                      "    gl_Position = vec4(0.0);\n"
                                                      "    float color = texture(texSampler, vec4(0.0), bias);\n"
                                                      "}\n";

const std::string texture_sampler2DArray_bias_vs = "#version ${VERSION}\n"
                                                   "uniform highp ${gsampler}2DArray texSampler;\n"
                                                   "uniform highp float bias;\n"
                                                   "void main (void)\n"
                                                   "{\n"
                                                   "    gl_Position = vec4(0.0);\n"
                                                   "    ${gvec4} color = texture(texSampler, vec3(0.0), bias);\n"
                                                   "}\n";

const std::string texture_samplerCubeArray_bias_vs = "#version ${VERSION}\n"
                                                     "#extension GL_EXT_texture_cube_map_array: enable\n"
                                                     "uniform highp ${gsampler}CubeArray texSampler;\n"
                                                     "uniform highp float bias;\n"
                                                     "void main (void)\n"
                                                     "{\n"
                                                     "    gl_Position = vec4(0.0);\n"
                                                     "    ${gvec4} color = texture(texSampler, vec4(0.0), bias);\n"
                                                     "}\n";

const std::string texture_samplerCubeArrayShadow_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_EXT_texture_shadow_lod: enable\n"
    "#extension GL_EXT_texture_cube_map_array: enable\n"
    "uniform highp samplerCubeArrayShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float color = texture(texSampler, vec4(0.0), 1.0, bias);\n"
    "}\n";

const std::string texture_sampler1DArray_bias_vs = "#version ${VERSION}\n"
                                                   "uniform highp ${gsampler}1DArray texSampler;\n"
                                                   "uniform highp float bias;\n"
                                                   "void main (void)\n"
                                                   "{\n"
                                                   "    gl_Position = vec4(0.0);\n"
                                                   "    ${gvec4} color = texture(texSampler, vec2(0.0), bias);\n"
                                                   "}\n";

const std::string texture_sampler1DArrayShadow_bias_vs = "#version ${VERSION}\n"
                                                         "uniform highp sampler1DArrayShadow texSampler;\n"
                                                         "uniform highp float bias;\n"
                                                         "void main (void)\n"
                                                         "{\n"
                                                         "    gl_Position = vec4(0.0);\n"
                                                         "    float color = texture(texSampler, vec3(0.0), bias);\n"
                                                         "}\n";

const std::string texture_sampler2DArrayShadow_bias_vs = "#version ${VERSION}\n"
                                                         "#extension GL_EXT_texture_shadow_lod: enable\n"
                                                         "uniform highp sampler2DArrayShadow texSampler;\n"
                                                         "uniform highp float bias;\n"
                                                         "void main (void)\n"
                                                         "{\n"
                                                         "    gl_Position = vec4(0.0);\n"
                                                         "    float color = texture(texSampler, vec4(0.0), bias);\n"
                                                         "}\n";

const std::string textureProj_sampler1D_bias_vs = "#version ${VERSION}\n"
                                                  "uniform highp ${gsampler}1D texSampler;\n"
                                                  "uniform highp float bias;\n"
                                                  "void main (void)\n"
                                                  "{\n"
                                                  "    gl_Position = vec4(0.0);\n"
                                                  "    ${gvec4} color = textureProj(texSampler, vec2(0.0), bias);\n"
                                                  "}\n";

const std::string textureProj_sampler1D_vec4_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}1D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureProj(texSampler, vec4(0.0), bias);\n"
    "}\n";

const std::string textureProj_sampler2D_bias_vs = "#version ${VERSION}\n"
                                                  "uniform highp ${gsampler}2D texSampler;\n"
                                                  "uniform highp float bias;\n"
                                                  "void main (void)\n"
                                                  "{\n"
                                                  "    gl_Position = vec4(0.0);\n"
                                                  "    ${gvec4} color = textureProj(texSampler, vec3(0.0), bias);\n"
                                                  "}\n";

const std::string textureProj_sampler2D_vec4_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}2D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureProj(texSampler, vec4(0.0), bias);\n"
    "}\n";

const std::string textureProj_sampler3D_bias_vs = "#version ${VERSION}\n"
                                                  "uniform highp ${gsampler}3D texSampler;\n"
                                                  "uniform highp float bias;\n"
                                                  "void main (void)\n"
                                                  "{\n"
                                                  "    gl_Position = vec4(0.0);\n"
                                                  "    ${gvec4} color = textureProj(texSampler, vec4(0.0), bias);\n"
                                                  "}\n";

const std::string textureProj_sampler1DShadow_bias_vs = "#version ${VERSION}\n"
                                                        "uniform highp sampler1DShadow texSampler;\n"
                                                        "uniform highp float bias;\n"
                                                        "void main (void)\n"
                                                        "{\n"
                                                        "    gl_Position = vec4(0.0);\n"
                                                        "    float color = textureProj(texSampler, vec4(0.0), bias);\n"
                                                        "}\n";

const std::string textureProj_sampler2DShadow_bias_vs = "#version ${VERSION}\n"
                                                        "uniform highp sampler2DShadow texSampler;\n"
                                                        "uniform highp float bias;\n"
                                                        "void main (void)\n"
                                                        "{\n"
                                                        "    gl_Position = vec4(0.0);\n"
                                                        "    float color = textureProj(texSampler, vec4(0.0), bias);\n"
                                                        "}\n";

const std::string textureOffset_sampler1D_bias_vs = "#version ${VERSION}\n"
                                                    "uniform highp ${gsampler}1D texSampler;\n"
                                                    "uniform highp float bias;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = vec4(0.0);\n"
                                                    "    ${gvec4} color = textureOffset(texSampler, 0.0, 1, bias);\n"
                                                    "}\n";

const std::string textureOffset_sampler2D_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}2D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureOffset(texSampler, vec2(0.0), ivec2(1), bias);\n"
    "}\n";

const std::string textureOffset_sampler3D_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}3D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureOffset(texSampler, vec3(0.0), ivec3(1), bias);\n"
    "}\n";

const std::string textureOffset_sampler2DShadow_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp sampler2DShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float color = textureOffset(texSampler, vec3(0.0), ivec2(1), bias);\n"
    "}\n";

const std::string textureOffset_sampler1DShadow_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp sampler1DShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float color = textureOffset(texSampler, vec3(0.0), 1, bias);\n"
    "}\n";

const std::string textureOffset_sampler1DArray_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}1DArray texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureOffset(texSampler, vec2(0.0), 1, bias);\n"
    "}\n";

const std::string textureOffset_sampler2DArray_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}2DArray texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureOffset(texSampler, vec3(0.0), ivec2(1), bias);\n"
    "}\n";

const std::string textureOffset_sampler1DArrayShadow_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp sampler1DArrayShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float color = textureOffset(texSampler, vec3(0.0), 1, bias);\n"
    "}\n";

const std::string textureOffset_sampler2DArrayShadow_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_EXT_texture_shadow_lod: enable\n"
    "uniform highp sampler2DArrayShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float color = textureOffset(texSampler, vec4(0.0), ivec2(1), bias);\n"
    "}\n";

const std::string textureProjOffset_sampler1D_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}1D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureProjOffset(texSampler, vec2(0.0), 1, bias);\n"
    "}\n";

const std::string textureProjOffset_sampler1D_vec4_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}1D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureProjOffset(texSampler, vec4(0.0), 1, bias);\n"
    "}\n";

const std::string textureProjOffset_sampler2D_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}2D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureProjOffset(texSampler, vec3(0.0), ivec2(1), bias);\n"
    "}\n";

const std::string textureProjOffset_sampler2D_vec4_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}2D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureProjOffset(texSampler, vec4(0.0), ivec2(1), bias);\n"
    "}\n";

const std::string textureProjOffset_sampler3D_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp ${gsampler}3D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} color = textureProjOffset(texSampler, vec4(0.0), ivec3(1), bias);\n"
    "}\n";

const std::string textureProjOffset_sampler1DShadow_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp sampler1DShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float color = textureProjOffset(texSampler, vec4(0.0), 1, bias);\n"
    "}\n";

const std::string textureProjOffset_sampler2DShadow_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp sampler2DShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float color = textureProjOffset(texSampler, vec4(0.0), ivec2(1), bias);\n"
    "}\n";

const std::string texture1D_sampler1D_bias_vs = "#version ${VERSION}\n"
                                                "uniform highp sampler1D texSampler;\n"
                                                "uniform highp float bias;\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    gl_Position = vec4(0.0);\n"
                                                "    vec4 color = texture1D(texSampler, 1.0, bias);\n"
                                                "}\n";

const std::string texture1DProj_sampler1D_bias_vs = "#version ${VERSION}\n"
                                                    "uniform highp sampler1D texSampler;\n"
                                                    "uniform highp float bias;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = vec4(0.0);\n"
                                                    "    vec4 color = texture1DProj(texSampler, vec2(1.0), bias);\n"
                                                    "}\n";

const std::string texture1DProj_sampler1D_vec4_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp sampler1D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    vec4 color = texture1DProj(texSampler, vec4(1.0), bias);\n"
    "}\n";

const std::string texture2D_sampler2D_bias_vs = "#version ${VERSION}\n"
                                                "uniform highp sampler2D texSampler;\n"
                                                "uniform highp float bias;\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    gl_Position = vec4(0.0);\n"
                                                "    vec4 color = texture2D(texSampler, vec2(1.0), bias);\n"
                                                "}\n";

const std::string texture2DProj_sampler2D_bias_vs = "#version ${VERSION}\n"
                                                    "uniform highp sampler2D texSampler;\n"
                                                    "uniform highp float bias;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = vec4(0.0);\n"
                                                    "    vec4 color = texture2DProj(texSampler, vec3(1.0), bias);\n"
                                                    "}\n";

const std::string texture2DProj_sampler2D_vec4_bias_vs =
    "#version ${VERSION}\n"
    "uniform highp sampler2D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    vec4 color = texture2DProj(texSampler, vec4(1.0), bias);\n"
    "}\n";

const std::string texture3D_sampler3D_bias_vs = "#version ${VERSION}\n"
                                                "uniform highp sampler3D texSampler;\n"
                                                "uniform highp float bias;\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    gl_Position = vec4(0.0);\n"
                                                "    vec4 color = texture3D(texSampler, vec3(1.0), bias);\n"
                                                "}\n";

const std::string texture3DProj_sampler3D_bias_vs = "#version ${VERSION}\n"
                                                    "uniform highp sampler3D texSampler;\n"
                                                    "uniform highp float bias;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = vec4(0.0);\n"
                                                    "    vec4 color = texture3DProj(texSampler, vec4(1.0), bias);\n"
                                                    "}\n";

const std::string textureCube_samplerCube_bias_vs = "#version ${VERSION}\n"
                                                    "uniform highp samplerCube texSampler;\n"
                                                    "uniform highp float bias;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "    gl_Position = vec4(0.0);\n"
                                                    "    vec4 color = textureCube(texSampler, vec3(1.0), bias);\n"
                                                    "}\n";

const std::string shadow1D_sampler1DShadow_bias_vs = "#version ${VERSION}\n"
                                                     "uniform highp sampler1DShadow texSampler;\n"
                                                     "uniform highp float bias;\n"
                                                     "void main (void)\n"
                                                     "{\n"
                                                     "    gl_Position = vec4(0.0);\n"
                                                     "    vec4 color = shadow1D(texSampler, vec3(1.0), bias);\n"
                                                     "}\n";

const std::string shadow2D_sampler2DShadow_bias_vs = "#version ${VERSION}\n"
                                                     "uniform highp sampler2DShadow texSampler;\n"
                                                     "uniform highp float bias;\n"
                                                     "void main (void)\n"
                                                     "{\n"
                                                     "    gl_Position = vec4(0.0);\n"
                                                     "    vec4 color = shadow2D(texSampler, vec3(1.0), bias);\n"
                                                     "}\n";

const std::string shadow1DProj_sampler1DShadow_bias_vs = "#version ${VERSION}\n"
                                                         "uniform highp sampler1DShadow texSampler;\n"
                                                         "uniform highp float bias;\n"
                                                         "void main (void)\n"
                                                         "{\n"
                                                         "    gl_Position = vec4(0.0);\n"
                                                         "    vec4 color = shadow1DProj(texSampler, vec4(1.0), bias);\n"
                                                         "}\n";

const std::string shadow2DProj_sampler2DShadow_bias_vs = "#version ${VERSION}\n"
                                                         "uniform highp sampler2DShadow texSampler;\n"
                                                         "uniform highp float bias;\n"
                                                         "void main (void)\n"
                                                         "{\n"
                                                         "    gl_Position = vec4(0.0);\n"
                                                         "    vec4 color = shadow2DProj(texSampler, vec4(1.0), bias);\n"
                                                         "}\n";

const std::string sparseTextureARB_sampler2D_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp ${gsampler}2D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} texel = ${gvec4}(1.0);\n"
    "    int color = sparseTextureARB(texSampler, vec2(1.0), texel, bias);\n"
    "}\n";

const std::string sparseTextureARB_sampler3D_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp ${gsampler}3D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} texel = ${gvec4}(1.0);\n"
    "    int color = sparseTextureARB(texSampler, vec3(1.0), texel, bias);\n"
    "}\n";

const std::string sparseTextureARB_samplerCube_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp ${gsampler}Cube texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} texel = ${gvec4}(1.0);\n"
    "    int color = sparseTextureARB(texSampler, vec3(1.0), texel, bias);\n"
    "}\n";

const std::string sparseTextureARB_sampler2DShadow_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp sampler2DShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float texel = float(1.0);\n"
    "    int color = sparseTextureARB(texSampler, vec3(1.0), texel, bias);\n"
    "}\n";

const std::string sparseTextureARB_samplerCubeShadow_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp samplerCubeShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float texel = float(1.0);\n"
    "    int color = sparseTextureARB(texSampler, vec4(1.0), texel, bias);\n"
    "}\n";

const std::string sparseTextureARB_sampler2DArray_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp ${gsampler}2DArray texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} texel = ${gvec4}(1.0);\n"
    "    int color = sparseTextureARB(texSampler, vec3(1.0), texel, bias);\n"
    "}\n";

const std::string sparseTextureARB_samplerCubeArray_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp ${gsampler}CubeArray texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} texel = ${gvec4}(1.0);\n"
    "    int color = sparseTextureARB(texSampler, vec4(1.0), texel, bias);\n"
    "}\n";

const std::string sparseTextureOffsetARB_sampler2D_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp ${gsampler}2D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} texel = ${gvec4}(1.0);\n"
    "    int color = sparseTextureOffsetARB(texSampler, vec2(1.0), ivec2(1), texel, bias);\n"
    "}\n";

const std::string sparseTextureOffsetARB_sampler3D_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp ${gsampler}3D texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} texel = ${gvec4}(1.0);\n"
    "    int color = sparseTextureOffsetARB(texSampler, vec3(1.0), ivec3(1), texel, bias);\n"
    "}\n";

const std::string sparseTextureOffsetARB_sampler2DShadow_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp sampler2DShadow texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    float texel = float(1.0);\n"
    "    int color = sparseTextureOffsetARB(texSampler, vec3(1.0), ivec2(1), texel, bias);\n"
    "}\n";

const std::string sparseTextureOffsetARB_sampler2DArray_bias_vs =
    "#version ${VERSION}\n"
    "#extension GL_ARB_sparse_texture2: enable\n"
    "uniform highp ${gsampler}2DArray texSampler;\n"
    "uniform highp float bias;\n"
    "void main (void)\n"
    "{\n"
    "    gl_Position = vec4(0.0);\n"
    "    ${gvec4} texel = ${gvec4}(1.0);\n"
    "    int color = sparseTextureOffsetARB(texSampler, vec3(1.0), ivec2(1), texel, bias);\n"
    "}\n";

void NegativeTextureLookupFunctionsBiasTests::init(void)
{
    bool is_gles = glu::isContextTypeES(m_context.getRenderContext().getType());

    auto addTest = [&](const std::string &testName, const std::string &testDescription, const std::string &shaderTxt,
                       bool texture_shadow_lod_required = false, bool texture_cube_map_array_required = false,
                       bool sparse_texture2_required = false)
    {
        tcu::StringTemplate vertShaderTemplate{shaderTxt};
        std::map<std::string, std::string> replacements;

        std::string samplers[3]{"sampler", "usampler", "isampler"};
        std::string vecTypes[3]{"vec4", "uvec4", "ivec4"};
        if (shaderTxt.find("gsampler") != std::string::npos)
        {
            for (int i = 0; i < 3; ++i)
            {
                replacements["VERSION"]  = is_gles ? GLES_GLSL_VER : GL_GLSL_VER;
                replacements["gsampler"] = samplers[i];
                replacements["gvec4"]    = vecTypes[i];

                auto shader = vertShaderTemplate.specialize(replacements);

                std::string name = testName;
                std::string dsc  = testDescription;
                std::string str2 = "sampler";
                std::string str3 = samplers[i];

                name.replace(name.find(str2), str2.length(), str3);
                dsc.replace(dsc.find(str2), str2.length(), str3);

                addChild(new NegativeTextureLookupFunctionsBiasTest(
                    m_context, name, dsc, shader, texture_shadow_lod_required, texture_cube_map_array_required,
                    sparse_texture2_required));
            }
        }
    };

    if (!is_gles)
    {
        addTest("texture_sampler1D_bias", "tests texture() with sampler1D and bias", texture_sampler1D_bias_vs);
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture_sampler1DShadow_bias",
                                                            "tests texture() with sampler1DShadow and bias",
                                                            texture_sampler1DShadow_bias_vs));
        addTest("texture_sampler1DArray_bias", "tests texture() with sampler1DArray and bias",
                texture_sampler1DArray_bias_vs);
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture_sampler1DArrayShadow_bias",
                                                            "tests texture() with sampler1DArrayShadow and bias",
                                                            texture_sampler1DArrayShadow_bias_vs));
        addTest("textureProj_sampler1D_bias", "tests textureProj() with sampler1D and bias",
                textureProj_sampler1D_bias_vs);
        addTest("textureProj_sampler1D_vec4_bias", "tests textureProj() with sampler1D and bias",
                textureProj_sampler1D_vec4_bias_vs);
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureProj_sampler1DShadow_bias",
                                                            "tests textureProj() with sampler1DShadow and bias",
                                                            textureProj_sampler1DShadow_bias_vs));
        addTest("textureOffset_sampler1D_bias", "tests textureOffset() with sampler1D and bias",
                textureOffset_sampler1D_bias_vs);
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureOffset_sampler1DShadow_bias",
                                                            "tests textureOffset() with sampler1DShadow and bias",
                                                            textureOffset_sampler1DShadow_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureOffset_sampler1DArrayShadow_bias",
                                                            "tests textureOffset() with sampler1DArrayShadow and bias",
                                                            textureOffset_sampler1DArrayShadow_bias_vs));
        addTest("textureOffset_sampler1DArray_bias", "tests textureOffset() with sampler1DArray and bias",
                textureOffset_sampler1DArray_bias_vs);
        addTest("textureProjOffset_sampler1D_bias", "tests textureProjOffset() with sampler1D and bias",
                textureProjOffset_sampler1D_bias_vs);
        addTest("textureProjOffset_sampler1D_vec4_bias", "tests textureProjOffset() with sampler1D and bias",
                textureProjOffset_sampler1D_vec4_bias_vs);
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureProjOffset_sampler1DShadow_bias",
                                                            "tests textureProjOffset() with sampler1DShadow and bias",
                                                            textureProjOffset_sampler1DShadow_bias_vs));
        // global function shadow1D*, shadow2D* is removed after version 140
        GL_GLSL_VER = "120";
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture1D_sampler1D_bias",
                                                            "tests texture1D() with sampler1D and bias",
                                                            texture1D_sampler1D_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture1DProj_sampler1D_bias",
                                                            "tests texture1DProj() with sampler1D and bias",
                                                            texture1DProj_sampler1D_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture1DProj_sampler1D_vec4_bias",
                                                            "tests texture1DProj() with sampler1D and bias",
                                                            texture1DProj_sampler1D_vec4_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture2D_sampler2D_bias",
                                                            "tests texture2D() with sampler2D and bias",
                                                            texture2D_sampler2D_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture2DProj_sampler2D_bias",
                                                            "tests texture1DProj() with sampler1D and bias",
                                                            texture2DProj_sampler2D_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture2DProj_sampler2D_vec4_bias",
                                                            "tests texture1DProj() with sampler1D and bias",
                                                            texture2DProj_sampler2D_vec4_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture3D_sampler3D_bias",
                                                            "tests texture3D() with sampler3D and bias",
                                                            texture3D_sampler3D_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture3DProj_sampler3D_bias",
                                                            "tests texture3DProj() with sampler3D and bias",
                                                            texture3DProj_sampler3D_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureCube_samplerCube_bias",
                                                            "tests textureCube() with samplerCube and bias",
                                                            textureCube_samplerCube_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "shadow1D_sampler1DShadow_bias",
                                                            "tests shadow1D() with sampler1DShadow and bias",
                                                            shadow1D_sampler1DShadow_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "shadow2D_sampler2DShadow_bias",
                                                            "tests shadow2D() with sampler2DShadow and bias",
                                                            shadow2D_sampler2DShadow_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "shadow1DProj_sampler1DShadow_bias",
                                                            "tests shadow1DProj() with sampler1DShadow and bias",
                                                            shadow1DProj_sampler1DShadow_bias_vs));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "shadow2DProj_sampler2DShadow_bias",
                                                            "tests shadow2DProj() with sampler2DShadow and bias",
                                                            shadow2DProj_sampler2DShadow_bias_vs));
        // revert version
        GL_GLSL_VER = "400 core";
        addTest("sparseTextureARB_sampler2D_bias", "tests sparseTextureARB() with sampler2D and bias",
                sparseTextureARB_sampler2D_bias_vs, false, false, true);
        addTest("sparseTextureARB_sampler3D_bias", "tests sparseTextureARB() with sampler3D and bias",
                sparseTextureARB_sampler3D_bias_vs, false, false, true);
        addTest("sparseTextureARB_samplerCube_bias", "tests sparseTextureARB() with samplerCube and bias",
                sparseTextureARB_samplerCube_bias_vs, false, false, true);
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "sparseTextureARB_sampler2DShadow_bias",
                                                            "tests sparseTextureARB() with sampler2DShadow and bias",
                                                            sparseTextureARB_sampler2DShadow_bias_vs, false, false,
                                                            true));
        addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "sparseTextureARB_samplerCubeShadow_bias",
                                                            "tests sparseTextureARB() with samplerCubeShadow and bias",
                                                            sparseTextureARB_samplerCubeShadow_bias_vs, false, false,
                                                            true));
        addTest("sparseTextureARB_sampler2DArray_bias", "tests sparseTextureARB() with sampler2DArray and bias",
                sparseTextureARB_sampler2DArray_bias_vs, false, false, true);
        addTest("sparseTextureARB_samplerCubeArray_bias", "tests sparseTextureARB() with samplerCubeArray and bias",
                sparseTextureARB_samplerCubeArray_bias_vs, false, false, true);
        addTest("sparseTextureOffsetARB_sampler2D_bias", "tests sparseTextureOffsetARB() with sampler2D and bias",
                sparseTextureOffsetARB_sampler2D_bias_vs, false, false, true);
        addTest("sparseTextureOffsetARB_sampler3D_bias", "tests sparseTextureOffsetARB() with sampler3D and bias",
                sparseTextureOffsetARB_sampler3D_bias_vs, false, false, true);
        addChild(new NegativeTextureLookupFunctionsBiasTest(
            m_context, "sparseTextureOffsetARB_sampler2DShadow_bias",
            "tests sparseTextureOffsetARB() with sampler2DShadow and bias",
            sparseTextureOffsetARB_sampler2DShadow_bias_vs, false, false, true));
        addTest("sparseTextureOffsetARB_sampler2DArray_bias",
                "tests sparseTextureOffsetARB() with sampler2DArray and bias",
                sparseTextureOffsetARB_sampler2DArray_bias_vs, false, false, true);
    }
    addTest("texture_sampler2D_bias", "tests texture() with sampler2D and bias", texture_sampler2D_bias_vs);
    addTest("texture_sampler3D_bias", "tests texture() with sampler3D and bias", texture_sampler3D_bias_vs);
    addTest("texture_samplerCube_bias", "tests texture() with samplerCube and bias", texture_samplerCube_bias_vs);
    addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture_sampler2DShadow_bias",
                                                        "tests texture() with sampler2DShadow and bias",
                                                        texture_sampler2DShadow_bias_vs));
    addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture_samplerCubeShadow_bias",
                                                        "tests texture() with samplerCubeShadow and bias",
                                                        texture_samplerCubeShadow_bias_vs));
    addTest("texture_sampler2DArray_bias", "tests texture() with sampler2DArray and bias",
            texture_sampler2DArray_bias_vs);
    addTest("textureProj_sampler2D_bias", "tests textureProj() with sampler2D and bias", textureProj_sampler2D_bias_vs);
    addTest("textureProj_sampler2D_vec4_bias", "tests textureProj() with sampler2D and bias",
            textureProj_sampler2D_vec4_bias_vs);
    addTest("textureProj_sampler3D_bias", "tests textureProj() with sampler3D and bias", textureProj_sampler3D_bias_vs);
    addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureProj_sampler2DShadow_bias",
                                                        "tests textureProj() with sampler2DShadow and bias",
                                                        textureProj_sampler2DShadow_bias_vs));
    addTest("textureOffset_sampler2D_bias", "tests textureOffset() with sampler2D and bias",
            textureOffset_sampler2D_bias_vs);
    addTest("textureOffset_sampler3D_bias", "tests textureOffset() with sampler3D and bias",
            textureOffset_sampler3D_bias_vs);
    addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureOffset_sampler2DShadow_bias",
                                                        "tests textureOffset() with sampler2DShadow and bias",
                                                        textureOffset_sampler2DShadow_bias_vs));
    addTest("textureOffset_sampler2DArray_bias", "tests textureOffset() with sampler2DArray and bias",
            textureOffset_sampler2DArray_bias_vs);
    addTest("textureProjOffset_sampler2D_bias", "tests textureProjOffset() with sampler2D and bias",
            textureProjOffset_sampler2D_bias_vs);
    addTest("textureProjOffset_sampler2D_vec4_bias", "tests textureProjOffset() with sampler2D and bias",
            textureProjOffset_sampler2D_vec4_bias_vs);
    addTest("textureProjOffset_sampler3D_bias", "tests textureProjOffset() with sampler3D and bias",
            textureProjOffset_sampler3D_bias_vs);
    addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureProjOffset_sampler2DShadow_bias",
                                                        "tests textureProjOffset() with sampler2DShadow and bias",
                                                        textureProjOffset_sampler2DShadow_bias_vs));

    addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture_sampler2DArrayShadow_bias",
                                                        "tests texture() with sampler2DArrayShadow and bias",
                                                        texture_sampler2DArrayShadow_bias_vs, true));
    addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "textureOffset_sampler2DArrayShadow_bias",
                                                        "tests textureOffset with sampler2DArrayShadow and bias",
                                                        textureOffset_sampler2DArrayShadow_bias_vs, true));

    GLES_GLSL_VER = "310 es";
    addChild(new NegativeTextureLookupFunctionsBiasTest(m_context, "texture_samplerCubeArrayShadow_bias",
                                                        "tests texture() with samplerCubeArrayShadow and bias",
                                                        texture_samplerCubeArrayShadow_bias_vs, true, true));
    addTest("texture_samplerCubeArray_bias", "tests texture() with samplerCubeArray and bias",
            texture_samplerCubeArray_bias_vs, false, true);
}
} /* namespace glcts */
