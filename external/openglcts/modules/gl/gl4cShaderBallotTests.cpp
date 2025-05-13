/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2017 The Khronos Group Inc.
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
* \file  gl4cShaderBallotTests.cpp
* \brief Conformance tests for the ARB_shader_ballot functionality.
*/

#include "gl4cShaderBallotTests.hpp"

#include "glcContext.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluDrawUtil.hpp"
#include "gluObjectWrapper.hpp"
#include "gluProgramInterfaceQuery.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"

#include <sstream>

// Uncomment below to dump the shaders used by the tests to disk
// #define DUMP_SHADERS

namespace gl4cts
{

ShaderBallotBaseTestCase::ShaderPipeline::ShaderPipeline(
    glu::ShaderType testedShader, const std::string &contentSnippet, const std::map<std::string, std::string> &specMap,
    const std::string &additionalLayout, const std::string &additionalFunctions, const uint32_t &fileNameSuffixOffset)
    : m_programRender(NULL)
    , m_programCompute(NULL)
    , m_testedShader(testedShader)
    , m_fileNameSuffixOffset(fileNameSuffixOffset)
    , m_shaders()
    , m_shaderChunks()
    , m_specializationMap(specMap)
{
    std::string testedHeadPart = "#extension GL_ARB_shader_ballot : enable\n"
                                 "#extension GL_ARB_gpu_shader_int64 : enable\n";

    std::string testedContentPart = contentSnippet;

    // vertex shader parts

    m_shaders[glu::SHADERTYPE_VERTEX].push_back("#version 450 core\n");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back(m_testedShader == glu::SHADERTYPE_VERTEX ? testedHeadPart : "");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back("layout(location = 0) in highp vec2 inPosition;\n"
                                                "layout(location = 0) flat out highp vec4 vsColor;\n"
                                                "layout(location = 1) out highp vec3 vsPosition;\n");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back(m_testedShader == glu::SHADERTYPE_VERTEX ? additionalLayout : "");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back("\n");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back(m_testedShader == glu::SHADERTYPE_VERTEX ? additionalFunctions : "");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back("\n");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back("void main()\n"
                                                "{\n"
                                                "    gl_Position = vec4(inPosition, 0.0, 1.0);\n"
                                                "    vsPosition = vec3(inPosition, 0.0);\n"
                                                "    vec4 outColor = vec4(0.0); \n");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back(m_testedShader == glu::SHADERTYPE_VERTEX ? testedContentPart : "");
    m_shaders[glu::SHADERTYPE_VERTEX].push_back("    vsColor = outColor;\n"
                                                "}\n");

    // fragment shader parts

    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back("#version 450 core\n");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back(m_testedShader == glu::SHADERTYPE_FRAGMENT ? testedHeadPart : "");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back("layout(location = 0) in flat highp vec4 gsColor;\n"
                                                  "layout(location = 0) out highp vec4 fsColor;\n");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back(m_testedShader == glu::SHADERTYPE_FRAGMENT ? additionalLayout : "");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back("\n");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back(m_testedShader == glu::SHADERTYPE_FRAGMENT ? additionalFunctions :
                                                                                               "");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back("\n");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back("void main()\n"
                                                  "{\n"
                                                  "    vec4 outColor = vec4(0.0); \n");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back(
        m_testedShader == glu::SHADERTYPE_FRAGMENT ? testedContentPart : "    outColor = gsColor;\n");
    m_shaders[glu::SHADERTYPE_FRAGMENT].push_back("    fsColor = outColor;\n"
                                                  "}\n");

    // tessellation control shader parts

    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back("#version 450 core\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back(
        m_testedShader == glu::SHADERTYPE_TESSELLATION_CONTROL ? testedHeadPart : "");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back("layout(vertices = 3) out;\n"
                                                              "layout(location = 0) in flat highp vec4 vsColor[];\n"
                                                              "layout(location = 1) in highp vec3 vsPosition[];\n"
                                                              "layout(location = 0) out flat highp vec4 tcsColor[];\n"
                                                              "layout(location = 1) out highp vec3 tcsPosition[];\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back(
        m_testedShader == glu::SHADERTYPE_TESSELLATION_CONTROL ? additionalLayout : "");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back("\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back(
        m_testedShader == glu::SHADERTYPE_TESSELLATION_CONTROL ? additionalFunctions : "");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back("\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back(
        "void main()\n"
        "{\n"
        "    tcsPosition[gl_InvocationID] = vsPosition[gl_InvocationID];\n"
        "    vec4 outColor = vec4(0.0);\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back(m_testedShader == glu::SHADERTYPE_TESSELLATION_CONTROL ?
                                                                  testedContentPart :
                                                                  "    outColor = vsColor[gl_InvocationID];\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_CONTROL].push_back(
        "    tcsColor[gl_InvocationID] = outColor;\n"
        "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "    gl_TessLevelInner[0] = 3;\n"
        "    gl_TessLevelInner[1] = 3;\n"
        "    gl_TessLevelOuter[0] = 3;\n"
        "    gl_TessLevelOuter[1] = 3;\n"
        "    gl_TessLevelOuter[2] = 3;\n"
        "    gl_TessLevelOuter[3] = 3;\n"
        "}\n");

    // tessellation evaluation shader parts

    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back("#version 450 core\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back(
        m_testedShader == glu::SHADERTYPE_TESSELLATION_EVALUATION ? testedHeadPart : "");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back(
        "layout(triangles, equal_spacing, cw) in;\n"
        "layout(location = 0) in flat highp vec4 tcsColor[];\n"
        "layout(location = 1) in highp vec3 tcsPosition[];\n"
        "layout(location = 0) out flat highp vec4 tesColor;\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back(
        m_testedShader == glu::SHADERTYPE_TESSELLATION_EVALUATION ? additionalLayout : "");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back("\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back(
        m_testedShader == glu::SHADERTYPE_TESSELLATION_EVALUATION ? additionalFunctions : "");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back("\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back("void main()\n"
                                                                 "{\n"
                                                                 "    float u = gl_TessCoord.x;\n"
                                                                 "    float v = gl_TessCoord.y;\n"
                                                                 "    float w = gl_TessCoord.z;\n"
                                                                 "    vec4 pos0 = gl_in[0].gl_Position;\n"
                                                                 "    vec4 pos1 = gl_in[1].gl_Position;\n"
                                                                 "    vec4 pos2 = gl_in[2].gl_Position;\n"
                                                                 "    gl_Position = u * pos0 + v * pos1 + w * pos2;\n"
                                                                 "    vec4 outColor = vec4(0.0);\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back(
        m_testedShader == glu::SHADERTYPE_TESSELLATION_EVALUATION ? testedContentPart :
                                                                    "    outColor = tcsColor[0];\n");
    m_shaders[glu::SHADERTYPE_TESSELLATION_EVALUATION].push_back("    tesColor = outColor;\n"
                                                                 "}\n");

    // geometry shader parts

    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back("#version 450 core\n");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back(m_testedShader == glu::SHADERTYPE_GEOMETRY ? testedHeadPart : "");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back("layout(triangles) in;\n"
                                                  "layout(triangle_strip, max_vertices = 3) out;\n"
                                                  "layout(location = 0) in flat highp vec4 tesColor[];\n"
                                                  "layout(location = 0) out flat highp vec4 gsColor;\n");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back(m_testedShader == glu::SHADERTYPE_GEOMETRY ? additionalLayout : "");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back("\n");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back(m_testedShader == glu::SHADERTYPE_GEOMETRY ? additionalFunctions :
                                                                                               "");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back("\n");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back("void main()\n"
                                                  "{\n"
                                                  "    vec4 outColor = vec4(0.0);\n");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back(m_testedShader == glu::SHADERTYPE_GEOMETRY ? testedContentPart : "");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back("    for (int i = 0; i<3; i++)\n"
                                                  "    {\n"
                                                  "        gl_Position = gl_in[i].gl_Position;\n");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back(
        m_testedShader == glu::SHADERTYPE_GEOMETRY ? "" : "        outColor = tesColor[i];\n");
    m_shaders[glu::SHADERTYPE_GEOMETRY].push_back("        gsColor = outColor;\n"
                                                  "        EmitVertex();\n"
                                                  "    }\n"
                                                  "    EndPrimitive();\n"
                                                  "}\n");

    // compute shader parts

    m_shaders[glu::SHADERTYPE_COMPUTE].push_back("#version 450 core\n");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back(m_testedShader == glu::SHADERTYPE_COMPUTE ? testedHeadPart : "");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back(
        "layout(rgba32f, binding = 1) writeonly uniform highp image2D destImage;\n"
        "layout (local_size_x = 16, local_size_y = 16) in;\n");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back(m_testedShader == glu::SHADERTYPE_COMPUTE ? additionalLayout : "");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back("\n");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back(m_testedShader == glu::SHADERTYPE_COMPUTE ? additionalFunctions : "");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back("\n");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back("void main (void)\n"
                                                 "{\n"
                                                 "vec4 outColor = vec4(0.0);\n");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back(m_testedShader == glu::SHADERTYPE_COMPUTE ? testedContentPart : "");
    m_shaders[glu::SHADERTYPE_COMPUTE].push_back("imageStore(destImage, ivec2(gl_GlobalInvocationID.xy), outColor);\n"
                                                 "}\n");

    // create shader chunks

    for (unsigned int shaderType = 0; shaderType <= glu::SHADERTYPE_COMPUTE; ++shaderType)
    {
        m_shaderChunks[shaderType] = new const char *[m_shaders[shaderType].size()];
        for (unsigned int i = 0; i < m_shaders[shaderType].size(); ++i)
        {
            m_shaderChunks[shaderType][i] = m_shaders[shaderType][i].data();
        }
    }
}

ShaderBallotBaseTestCase::ShaderPipeline::~ShaderPipeline()
{
    if (m_programRender)
    {
        delete m_programRender;
    }

    if (m_programCompute)
    {
        delete m_programCompute;
    }

    for (unsigned int shaderType = 0; shaderType <= glu::SHADERTYPE_COMPUTE; ++shaderType)
    {
        delete[] m_shaderChunks[shaderType];
    }
}

const char *const *ShaderBallotBaseTestCase::ShaderPipeline::getShaderParts(glu::ShaderType shaderType) const
{
    return m_shaderChunks[shaderType];
}

unsigned int ShaderBallotBaseTestCase::ShaderPipeline::getShaderPartsCount(glu::ShaderType shaderType) const
{
    return static_cast<unsigned int>(m_shaders[shaderType].size());
}

void ShaderBallotBaseTestCase::ShaderPipeline::renderQuad(deqp::Context &context)
{
    const glw::Functions &gl = context.getRenderContext().getFunctions();

    uint16_t const quadIndices[] = {0, 1, 2, 2, 1, 3};

    float const position[] = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    glu::VertexArrayBinding vertexArrays[] = {glu::va::Float("inPosition", 2, 4, 0, position)};

    this->use(context);

    glu::PrimitiveList primitiveList = glu::pr::Patches(DE_LENGTH_OF_ARRAY(quadIndices), quadIndices);

    glu::draw(context.getRenderContext(), m_programRender->getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
              primitiveList);

    GLU_EXPECT_NO_ERROR(gl.getError(), "glu::draw error");
}

void ShaderBallotBaseTestCase::ShaderPipeline::executeComputeShader(deqp::Context &context)
{
    const glw::Functions &gl = context.getRenderContext().getFunctions();

    const glu::Texture outputTexture(context.getRenderContext());

    gl.useProgram(m_programCompute->getProgram());

    // output image
    gl.bindTexture(GL_TEXTURE_2D, *outputTexture);
    gl.texStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Uploading image data failed");

    // bind image
    gl.bindImageTexture(1, *outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Image setup failed");

    // dispatch compute
    gl.dispatchCompute(1, 1, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDispatchCompute()");

    gl.memoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glMemoryBarrier()");

    // render output texture

    std::string vs = "#version 450 core\n"
                     "in highp vec2 position;\n"
                     "in vec2 inTexcoord;\n"
                     "out vec2 texcoord;\n"
                     "void main()\n"
                     "{\n"
                     "    texcoord = inTexcoord;\n"
                     "    gl_Position = vec4(position, 0.0, 1.0);\n"
                     "}\n";

    std::string fs = "#version 450 core\n"
                     "uniform sampler2D sampler;\n"
                     "in vec2 texcoord;\n"
                     "out vec4 color;\n"
                     "void main()\n"
                     "{\n"
                     "    color = texture(sampler, texcoord);\n"
                     "}\n";

    glu::ProgramSources sources;
    sources.sources[glu::SHADERTYPE_VERTEX].push_back(vs);
    sources.sources[glu::SHADERTYPE_FRAGMENT].push_back(fs);
    glu::ShaderProgram renderShader(context.getRenderContext(), sources);

    if (!m_programRender->isOk())
    {
        TCU_FAIL("Shader compilation failed");
    }

    gl.bindTexture(GL_TEXTURE_2D, *outputTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() call failed.");

    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    gl.useProgram(renderShader.getProgram());

    gl.uniform1i(gl.getUniformLocation(renderShader.getProgram(), "sampler"), 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");

    uint16_t const quadIndices[] = {0, 1, 2, 2, 1, 3};

    float const position[] = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    float const texCoord[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f};

    glu::VertexArrayBinding vertexArrays[] = {glu::va::Float("position", 2, 4, 0, position),
                                              glu::va::Float("inTexcoord", 2, 4, 0, texCoord)};

    glu::draw(context.getRenderContext(), renderShader.getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
              glu::pr::TriangleStrip(DE_LENGTH_OF_ARRAY(quadIndices), quadIndices));

    GLU_EXPECT_NO_ERROR(gl.getError(), "glu::draw error");
}

void ShaderBallotBaseTestCase::ShaderPipeline::use(deqp::Context &context)
{
    const glw::Functions &gl = context.getRenderContext().getFunctions();
    gl.useProgram(m_programRender->getProgram());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram failed");
}

void ShaderBallotBaseTestCase::ShaderPipeline::test(deqp::Context &context)
{
    if (m_testedShader == glu::SHADERTYPE_COMPUTE)
    {
        executeComputeShader(context);
    }
    else
    {
        renderQuad(context);
    }
}

void ShaderBallotBaseTestCase::createShaderPrograms(ShaderPipeline &pipeline, const std::string &name, uint32_t index)
{
    DE_UNREF(name);
    DE_UNREF(index);

    glu::ProgramSources sourcesRender;

    for (unsigned int i = 0; i < glu::SHADERTYPE_COMPUTE; ++i)
    {
        glu::ShaderType shaderType = (glu::ShaderType)i;

        m_specializationMap.clear();
        std::map<std::string, std::string>::const_iterator mapIter;
        for (mapIter = pipeline.getSpecializationMap().begin(); mapIter != pipeline.getSpecializationMap().end();
             mapIter++)
            m_specializationMap[mapIter->first] = mapIter->second;

        std::string shader =
            specializeShader(pipeline.getShaderPartsCount(shaderType), pipeline.getShaderParts(shaderType));
        sourcesRender.sources[i].push_back(shader);

#ifdef DUMP_SHADERS
        if (pipeline.getTestedShader() == shaderType)
        {
            std::ostringstream fileName;
            fileName << name << '_' << (index + pipeline.getFileNameSuffixOffset()) << '_'
                     << glu::getShaderTypeName(shaderType);
            glu::saveShader(fileName.str(), shader);
        }
#endif
    }

    glu::ShaderProgram *programRender = new glu::ShaderProgram(m_context.getRenderContext(), sourcesRender);

    if (!programRender->isOk())
    {
        TCU_FAIL("Shader compilation failed");
    }

    glu::ProgramSources sourcesCompute;

    m_specializationMap.clear();
    m_specializationMap.insert(pipeline.getSpecializationMap().begin(), pipeline.getSpecializationMap().end());
    std::string shaderCompute = specializeShader(pipeline.getShaderPartsCount(glu::SHADERTYPE_COMPUTE),
                                                 pipeline.getShaderParts(glu::SHADERTYPE_COMPUTE));
    sourcesCompute.sources[glu::SHADERTYPE_COMPUTE].push_back(shaderCompute);
#ifdef DUMP_SHADERS
    if (pipeline.getTestedShader() == glu::SHADERTYPE_COMPUTE)
    {
        std::ostringstream fileName;
        fileName << name << '_' << (index + pipeline.getFileNameSuffixOffset()) << '_'
                 << glu::getShaderTypeName(glu::SHADERTYPE_COMPUTE);
        glu::saveShader(fileName.str(), shaderCompute);
    }
#endif

    glu::ShaderProgram *programCompute = new glu::ShaderProgram(m_context.getRenderContext(), sourcesCompute);

    if (!programCompute->isOk())
    {
        TCU_FAIL("Shader compilation failed");
    }

    pipeline.setShaderPrograms(programRender, programCompute);
}

ShaderBallotBaseTestCase::~ShaderBallotBaseTestCase()
{
    for (ShaderPipelineIter iter = m_shaderPipelines.begin(); iter != m_shaderPipelines.end(); ++iter)
    {
        delete *iter;
    }
}

bool ShaderBallotBaseTestCase::validateScreenPixels(deqp::Context &context, tcu::Vec4 desiredColor,
                                                    const tcu::Vec4 &ignoredColor)
{
    const glw::Functions &gl             = context.getRenderContext().getFunctions();
    const tcu::RenderTarget renderTarget = context.getRenderContext().getRenderTarget();
    tcu::IVec2 size(renderTarget.getWidth(), renderTarget.getHeight());

    glw::GLfloat *pixels = new glw::GLfloat[size.x() * size.y() * 4];

    // clear buffer
    for (int x = 0; x < size.x(); ++x)
    {
        for (int y = 0; y < size.y(); ++y)
        {
            int mappedPixelPosition = y * size.x() + x;

            pixels[mappedPixelPosition * 4 + 0] = -1.0f;
            pixels[mappedPixelPosition * 4 + 1] = -1.0f;
            pixels[mappedPixelPosition * 4 + 2] = -1.0f;
            pixels[mappedPixelPosition * 4 + 3] = -1.0f;
        }
    }

    // read pixels
    gl.readPixels(0, 0, size.x(), size.y(), GL_RGBA, GL_FLOAT, pixels);

    // validate pixels
    bool rendered = false;
    for (int x = 0; x < size.x(); ++x)
    {
        for (int y = 0; y < size.y(); ++y)
        {
            int mappedPixelPosition = y * size.x() + x;

            tcu::Vec4 color(pixels[mappedPixelPosition * 4 + 0], pixels[mappedPixelPosition * 4 + 1],
                            pixels[mappedPixelPosition * 4 + 2], pixels[mappedPixelPosition * 4 + 3]);

            if (!ShaderBallotBaseTestCase::validateColor(color, ignoredColor))
            {
                rendered = true;
                if (!ShaderBallotBaseTestCase::validateColor(color, desiredColor))
                {
                    return false;
                }
            }
        }
    }

    delete[] pixels;

    return rendered;
}

bool ShaderBallotBaseTestCase::validateScreenPixelsSameColor(deqp::Context &context, const tcu::Vec4 &ignoredColor)
{
    const glw::Functions &gl = context.getRenderContext().getFunctions();

    glw::GLfloat topLeftPixel[]{-1.0f, -1.0f, -1.0f, -1.0f};

    // read pixel
    gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, topLeftPixel);

    tcu::Vec4 desiredColor(topLeftPixel[0], topLeftPixel[1], topLeftPixel[2], topLeftPixel[3]);

    // validation
    return ShaderBallotBaseTestCase::validateScreenPixels(context, desiredColor, ignoredColor);
}

bool ShaderBallotBaseTestCase::validateColor(tcu::Vec4 testedColor, const tcu::Vec4 &desiredColor)
{
    const float epsilon = 0.008f;
    return de::abs(testedColor.x() - desiredColor.x()) < epsilon &&
           de::abs(testedColor.y() - desiredColor.y()) < epsilon &&
           de::abs(testedColor.z() - desiredColor.z()) < epsilon &&
           de::abs(testedColor.w() - desiredColor.w()) < epsilon;
}

/** Constructor.
 *
 *  @param context Rendering context
 */
ShaderBallotAvailabilityTestCase::ShaderBallotAvailabilityTestCase(deqp::Context &context)
    : ShaderBallotBaseTestCase(context, "ShaderBallotAvailability",
                               "Implements verification of availability for new build-in features")
{
    std::string colorShaderSnippet =
        "    float red = gl_SubGroupSizeARB / 64.0f;\n"
        "    float green = 1.0f - (gl_SubGroupInvocationARB / float(gl_SubGroupSizeARB));\n"
        "    float blue = float(ballotARB(true) % 256) / 256.0f;\n"
        "    outColor = readInvocationARB(vec4(red, green, blue, 1.0f), gl_SubGroupInvocationARB);\n";

    for (unsigned int i = 0; i <= glu::SHADERTYPE_COMPUTE; ++i)
    {
        m_shaderPipelines.push_back(new ShaderPipeline((glu::ShaderType)i, colorShaderSnippet));
    }
}

/** Initializes the test
 */
void ShaderBallotAvailabilityTestCase::init()
{
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult ShaderBallotAvailabilityTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_ballot") ||
        !m_context.getContextInfo().isExtensionSupported("GL_ARB_gpu_shader_int64"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
                                "GL_ARB_shader_ballot or GL_ARB_gpu_shader_int64 not supported");
        return STOP;
    }

    for (ShaderPipelineIter begin = m_shaderPipelines.begin(), iter = begin; iter != m_shaderPipelines.end(); ++iter)
    {
        createShaderPrograms(**iter, "availability", uint32_t(std::distance(begin, iter)));
    }

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    for (ShaderPipelineIter pipelineIter = m_shaderPipelines.begin(); pipelineIter != m_shaderPipelines.end();
         ++pipelineIter)
    {
        gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);

        (*pipelineIter)->test(m_context);

        gl.flush();
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context
 */
ShaderBallotBitmasksTestCase::ShaderBallotBitmasksTestCase(deqp::Context &context)
    : ShaderBallotBaseTestCase(context, "ShaderBallotBitmasks",
                               "Implements verification of values of gl_SubGroup*MaskARB variables")
{
    m_maskVars["gl_SubGroupEqMaskARB"] = "==";
    m_maskVars["gl_SubGroupGeMaskARB"] = ">=";
    m_maskVars["gl_SubGroupGtMaskARB"] = ">";
    m_maskVars["gl_SubGroupLeMaskARB"] = "<=";
    m_maskVars["gl_SubGroupLtMaskARB"] = "<";

    std::string colorShaderSnippet = "    uint64_t mask = 0;\n"
                                     "    for(uint i = 0; i < gl_SubGroupSizeARB; ++i)\n"
                                     "    {\n"
                                     "        if(i ${MASK_OPERATOR} gl_SubGroupInvocationARB)\n"
                                     "            mask = mask | (1ul << i);\n"
                                     "    }\n"
                                     "    float color = (${MASK_VAR} ^ mask) == 0ul ? 1.0 : 0.0;\n"
                                     "    outColor = vec4(color, color, color, 1.0);\n";

    for (MaskVarIter maskIter = m_maskVars.begin(); maskIter != m_maskVars.end(); maskIter++)
    {
        for (unsigned int i = 0; i <= glu::SHADERTYPE_COMPUTE; ++i)
        {
            std::map<std::string, std::string> specMap;
            specMap["MASK_VAR"]      = maskIter->first;
            specMap["MASK_OPERATOR"] = maskIter->second;
            m_shaderPipelines.push_back(new ShaderPipeline((glu::ShaderType)i, colorShaderSnippet, specMap));
        }
    }
}

/** Initializes the test
 */
void ShaderBallotBitmasksTestCase::init()
{
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult ShaderBallotBitmasksTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_ballot") ||
        !m_context.getContextInfo().isExtensionSupported("GL_ARB_gpu_shader_int64"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
                                "GL_ARB_shader_ballot or GL_ARB_gpu_shader_int64 not supported");
        return STOP;
    }

    for (ShaderPipelineIter begin = m_shaderPipelines.begin(), iter = begin; iter != m_shaderPipelines.end(); ++iter)
    {
        createShaderPrograms(**iter, "bitmask", uint32_t(std::distance(begin, iter)));
    }

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    for (ShaderPipelineIter pipelineIter = m_shaderPipelines.begin(); pipelineIter != m_shaderPipelines.end();
         ++pipelineIter)
    {
        gl.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);

        (*pipelineIter)->test(m_context);

        gl.flush();

        bool validationResult = ShaderBallotBaseTestCase::validateScreenPixels(
            m_context, tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        TCU_CHECK_MSG(validationResult, "Bitmask value is not correct");
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context
 */
ShaderBallotFunctionBallotTestCase::ShaderBallotFunctionBallotTestCase(deqp::Context &context)
    : ShaderBallotBaseTestCase(context, "ShaderBallotFunctionBallot",
                               "Implements verification of ballotARB calls and returned results")
{
    std::string ballotFalseSnippet = "    uint64_t result = ballotARB(false);\n"
                                     "    float color = result == 0ul ? 1.0 : 0.0;\n"
                                     "    outColor = vec4(color, color, color, 1.0);\n";

    std::string ballotTrueSnippet = "    uint64_t result = ballotARB(true);\n"
                                    "    float color = result != 0ul ? 1.0 : 0.0;\n"
                                    "    uint64_t invocationBit = 1ul << gl_SubGroupInvocationARB;\n"
                                    "    color *= float(invocationBit & result);\n"
                                    "    outColor = vec4(color, color, color, 1.0);\n";

    std::string ballotMixedSnippet = "    bool param = (gl_SubGroupInvocationARB % 2) == 0ul;\n"
                                     "    uint64_t result = ballotARB(param);\n"
                                     "    float color = (param && result != 0ul) || !param ? 1.0 : 0.0;\n"
                                     "    outColor = vec4(color, color, color, 1.0);\n";

    for (unsigned int i = 0; i <= glu::SHADERTYPE_COMPUTE; ++i)
    {
        m_shaderPipelines.push_back(new ShaderPipeline((glu::ShaderType)i, ballotFalseSnippet));
        m_shaderPipelines.push_back(new ShaderPipeline((glu::ShaderType)i, ballotTrueSnippet));
        m_shaderPipelines.push_back(new ShaderPipeline((glu::ShaderType)i, ballotMixedSnippet));
    }
}

/** Initializes the test
 */
void ShaderBallotFunctionBallotTestCase::init()
{
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult ShaderBallotFunctionBallotTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_ballot") ||
        !m_context.getContextInfo().isExtensionSupported("GL_ARB_gpu_shader_int64"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
                                "GL_ARB_shader_ballot or GL_ARB_gpu_shader_int64 not supported");
        return STOP;
    }

    for (ShaderPipelineIter begin = m_shaderPipelines.begin(), iter = begin; iter != m_shaderPipelines.end(); ++iter)
    {
        createShaderPrograms(**iter, "function_ballot", uint32_t(std::distance(begin, iter)));
    }

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    for (ShaderPipelineIter pipelineIter = m_shaderPipelines.begin(); pipelineIter != m_shaderPipelines.end();
         ++pipelineIter)
    {
        gl.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);

        (*pipelineIter)->test(m_context);

        gl.flush();

        bool validationResult = ShaderBallotBaseTestCase::validateScreenPixels(
            m_context, tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        TCU_CHECK_MSG(validationResult, "Value returned from ballotARB function is not correct");
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context
 */
ShaderBallotFunctionReadTestCase::ShaderBallotFunctionReadTestCase(deqp::Context &context)
    : ShaderBallotBaseTestCase(context, "ShaderBallotFunctionRead",
                               "Implements verification of readInvocationARB and readFirstInvocationARB function calls")
{
    const std::string additionalFunctions(R"glsl(
    bool subgroupBallotBitExtract(uint64_t value, uint index)
    {
        if (index >= 64)
            return false;
        const uint64_t mask = 1ul << index;
        if (bool((value & mask)) == true)
            return true;
        return false;
    }
    )glsl");

    const std::string additionalLayout(R"glsl(
    layout(std430, binding = 0) readonly buffer buffer1
    {
        uint data1[];
    };
    )glsl");

    const std::string readFirstSnippet(R"glsl(
    const uint sgSize = gl_SubGroupSizeARB;
    const uint sgInvocation = gl_SubGroupInvocationARB;
    uint tempRes = 0;
    uint firstActive = sgSize;
    uint64_t mask = ballotARB(true);
    for (uint i = 0; i < sgSize; i++)
    {
        if (subgroupBallotBitExtract(mask, i))
        {
            firstActive = i;
            break;
        }
    }
    tempRes |= (readFirstInvocationARB(data1[sgInvocation]) == data1[firstActive]) ? 0x1 : 0;
    // make the firstActive invocation inactive now
    if (firstActive != sgInvocation)
    {
        mask = ballotARB(true);
        for (uint i = 0; i < sgSize; i++)
        {
            if (subgroupBallotBitExtract(mask, i))
            {
                firstActive = i;
                break;
            }
        }
        tempRes |= (readFirstInvocationARB(data1[sgInvocation]) == data1[firstActive]) ? 0x2 : 0;
    }
    else
    {
        // the firstActive invocation didn't partake in the second result so set it to true
        tempRes |= 0x2;
    }
    outColor = (0x3 == tempRes) ? vec4(1.0, 1.0, 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
    )glsl");

    const std::string readSnippet(R"glsl(
    const uint64_t mask = ballotARB(true);
    const uint sgSize = gl_SubGroupSizeARB;
    const uint sgInvocation = gl_SubGroupInvocationARB;
    float tempRes = 1.0;
    uint ops[64];
    const uint d = data1[sgInvocation];
    ops[0] = readInvocationARB(d, 0u);
    ops[1] = readInvocationARB(d, 1u);
    ops[2] = readInvocationARB(d, 2u);
    ops[3] = readInvocationARB(d, 3u);
    ops[4] = readInvocationARB(d, 4u);
    ops[5] = readInvocationARB(d, 5u);
    ops[6] = readInvocationARB(d, 6u);
    ops[7] = readInvocationARB(d, 7u);
    ops[8] = readInvocationARB(d, 8u);
    ops[9] = readInvocationARB(d, 9u);
    ops[10] = readInvocationARB(d, 10u);
    ops[11] = readInvocationARB(d, 11u);
    ops[12] = readInvocationARB(d, 12u);
    ops[13] = readInvocationARB(d, 13u);
    ops[14] = readInvocationARB(d, 14u);
    ops[15] = readInvocationARB(d, 15u);
    ops[16] = readInvocationARB(d, 16u);
    ops[17] = readInvocationARB(d, 17u);
    ops[18] = readInvocationARB(d, 18u);
    ops[19] = readInvocationARB(d, 19u);
    ops[20] = readInvocationARB(d, 20u);
    ops[21] = readInvocationARB(d, 21u);
    ops[22] = readInvocationARB(d, 22u);
    ops[23] = readInvocationARB(d, 23u);
    ops[24] = readInvocationARB(d, 24u);
    ops[25] = readInvocationARB(d, 25u);
    ops[26] = readInvocationARB(d, 26u);
    ops[27] = readInvocationARB(d, 27u);
    ops[28] = readInvocationARB(d, 28u);
    ops[29] = readInvocationARB(d, 29u);
    ops[30] = readInvocationARB(d, 30u);
    ops[31] = readInvocationARB(d, 31u);
    ops[32] = readInvocationARB(d, 32u);
    ops[33] = readInvocationARB(d, 33u);
    ops[34] = readInvocationARB(d, 34u);
    ops[35] = readInvocationARB(d, 35u);
    ops[36] = readInvocationARB(d, 36u);
    ops[37] = readInvocationARB(d, 37u);
    ops[38] = readInvocationARB(d, 38u);
    ops[39] = readInvocationARB(d, 39u);
    ops[40] = readInvocationARB(d, 40u);
    ops[41] = readInvocationARB(d, 41u);
    ops[42] = readInvocationARB(d, 42u);
    ops[43] = readInvocationARB(d, 43u);
    ops[44] = readInvocationARB(d, 44u);
    ops[45] = readInvocationARB(d, 45u);
    ops[46] = readInvocationARB(d, 46u);
    ops[47] = readInvocationARB(d, 47u);
    ops[48] = readInvocationARB(d, 48u);
    ops[49] = readInvocationARB(d, 49u);
    ops[50] = readInvocationARB(d, 50u);
    ops[51] = readInvocationARB(d, 51u);
    ops[52] = readInvocationARB(d, 52u);
    ops[53] = readInvocationARB(d, 53u);
    ops[54] = readInvocationARB(d, 54u);
    ops[55] = readInvocationARB(d, 55u);
    ops[56] = readInvocationARB(d, 56u);
    ops[57] = readInvocationARB(d, 57u);
    ops[58] = readInvocationARB(d, 58u);
    ops[59] = readInvocationARB(d, 59u);
    ops[60] = readInvocationARB(d, 60u);
    ops[61] = readInvocationARB(d, 61u);
    ops[62] = readInvocationARB(d, 62u);
    ops[63] = readInvocationARB(d, 63u);
    for (int id = 0; id < sgSize; id++)
    {
        if (subgroupBallotBitExtract(mask, id) && ops[id] != data1[id])
        {
            tempRes = 0.0;
        }
    }
    outColor = vec4(tempRes, tempRes, tempRes, 1.0);
    )glsl");

    for (unsigned int i = 0; i <= glu::SHADERTYPE_COMPUTE; ++i)
    {
        m_shaderPipelines.push_back(new ShaderPipeline((glu::ShaderType)i, additionalLayout, additionalFunctions,
                                                       readSnippet, readInvocationSuffix));
        m_shaderPipelines.push_back(new ShaderPipeline((glu::ShaderType)i, additionalLayout, additionalFunctions,
                                                       readFirstSnippet, readFirstInvocationSuffix));
    }
}

void ShaderBallotFunctionReadTestCase::ShaderPipeline::createAndBindBuffer(deqp::Context &context)
{
    const uint32_t elems     = 128u;
    glw::GLsizeiptr size     = elems * sizeof(glw::GLuint);
    const glw::Functions &gl = context.getRenderContext().getFunctions();

    std::vector<glw::GLuint> data1(elems);
    for (uint32_t i = 0u; i < elems; ++i)
    {
        data1.at(i) = i % 64u;
    }

    gl.genBuffers(1, &m_buffer);

    gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
    gl.bufferData(GL_SHADER_STORAGE_BUFFER, size, data1.data(), GL_STATIC_DRAW);
    gl.bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_buffer);
}

void ShaderBallotFunctionReadTestCase::ShaderPipeline::destroyBuffer(deqp::Context &context)
{
    const glw::Functions &gl = context.getRenderContext().getFunctions();
    gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    gl.bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    gl.deleteBuffers(1, &m_buffer);
}

void ShaderBallotFunctionReadTestCase::ShaderPipeline::renderQuad(deqp::Context &context)
{
    const glw::Functions &gl                = context.getRenderContext().getFunctions();
    const glu::RenderContext &renderContext = context.getRenderContext();

    this->use(context);

    uint16_t const quadIndices[]           = {0, 1, 2, 2, 1, 3};
    float const position[]                 = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
    glu::VertexArrayBinding vertexArrays[] = {glu::va::Float("inPosition", 2, 4, 0, position)};
    glu::PrimitiveList primitiveList       = glu::pr::Patches(DE_LENGTH_OF_ARRAY(quadIndices), quadIndices);

    createAndBindBuffer(context);

    glu::draw(renderContext, getRenderProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays, primitiveList);
    const auto drawStatus = gl.getError();

    destroyBuffer(context);

    GLU_EXPECT_NO_ERROR(drawStatus, "glu::draw error");
}

void ShaderBallotFunctionReadTestCase::ShaderPipeline::executeComputeShader(deqp::Context &context)
{
    glu::RenderContext &renderContext = context.getRenderContext();
    const glw::Functions &gl          = renderContext.getFunctions();
    const glu::Texture outputTexture(renderContext);
    const int width  = renderContext.getRenderTarget().getWidth();
    const int height = renderContext.getRenderTarget().getHeight();

    gl.useProgram(getComputeProgram());

    // output image
    gl.bindTexture(GL_TEXTURE_2D, *outputTexture);
    gl.texStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Uploading image data failed");

    // bind image
    gl.bindImageTexture(1, *outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Image setup failed");

    // create buffer
    createAndBindBuffer(context);

    // dispatch compute
    gl.dispatchCompute(width, height, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDispatchCompute()");

    gl.memoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glMemoryBarrier()");

    destroyBuffer(context);

    // render output texture

    const std::string vs = "#version 450 core\n"
                           "in highp vec2 position;\n"
                           "in vec2 inTexcoord;\n"
                           "out vec2 texcoord;\n"
                           "void main()\n"
                           "{\n"
                           "    texcoord = inTexcoord;\n"
                           "    gl_Position = vec4(position, 0.0, 1.0);\n"
                           "}\n";

    const std::string fs = "#version 450 core\n"
                           "uniform sampler2D sampler;\n"
                           "in vec2 texcoord;\n"
                           "out vec4 color;\n"
                           "void main()\n"
                           "{\n"
                           "    color = texture(sampler, texcoord);\n"
                           "}\n";

    glu::ProgramSources modules;
    modules.sources[glu::SHADERTYPE_VERTEX].push_back(vs);
    modules.sources[glu::SHADERTYPE_FRAGMENT].push_back(fs);
    glu::ShaderProgram renderProgram(renderContext, modules);

    if (!renderProgram.isOk())
    {
        TCU_FAIL("Shader compilation failed");
    }

    gl.bindTexture(GL_TEXTURE_2D, *outputTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() call failed.");

    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    gl.useProgram(renderProgram.getProgram());

    gl.uniform1i(gl.getUniformLocation(renderProgram.getProgram(), "sampler"), 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");

    uint16_t const quadIndices[]           = {0, 1, 2, 2, 1, 3};
    float const position[]                 = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
    float const texCoord[]                 = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    glu::VertexArrayBinding vertexArrays[] = {glu::va::Float("position", 2, 4, 0, position),
                                              glu::va::Float("inTexcoord", 2, 4, 0, texCoord)};

    gl.viewport(0, 0, width, height);

    glu::draw(context.getRenderContext(), renderProgram.getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
              glu::pr::TriangleStrip(DE_LENGTH_OF_ARRAY(quadIndices), quadIndices));

    GLU_EXPECT_NO_ERROR(gl.getError(), "glu::draw error");
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult ShaderBallotFunctionReadTestCase::iterate()
{
    if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_shader_ballot") ||
        !m_context.getContextInfo().isExtensionSupported("GL_ARB_gpu_shader_int64"))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
                                "GL_ARB_shader_ballot or GL_ARB_gpu_shader_int64 not supported");
        return STOP;
    }

    for (ShaderPipelineIter begin = m_shaderPipelines.begin(), iter = begin; iter != m_shaderPipelines.end(); ++iter)
    {
        createShaderPrograms(**iter, "function_read", uint32_t(std::distance(begin, iter)));
    }

    const glw::Functions &gl             = m_context.getRenderContext().getFunctions();
    const tcu::RenderTarget renderTarget = m_context.getRenderContext().getRenderTarget();

    std::vector<glu::ShaderType> failedShaders;

    gl.viewport(0, 0, renderTarget.getWidth(), renderTarget.getHeight());

    for (ShaderPipelineIter pipelineIter = m_shaderPipelines.begin(); pipelineIter != m_shaderPipelines.end();
         ++pipelineIter)
    {
        const float c =
            float(std::distance(m_shaderPipelines.begin(), pipelineIter) + 1) / float(m_shaderPipelines.size() * 2);
        gl.clearColor(c, c, c, 1.0f);
        const tcu::Vec4 clearColor(c, c, c, 1.0f);

        gl.clear(GL_COLOR_BUFFER_BIT);

        (*pipelineIter)->test(m_context);

        gl.flush();

        glw::GLfloat topLeftPixel[]{-1.0f, -1.0f, -1.0f, -1.0f};
        // read pixel
        gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, topLeftPixel);

        const bool validationResult = ShaderBallotBaseTestCase::validateScreenPixelsSameColor(m_context, clearColor);
        if (!validationResult)
        {
            failedShaders.push_back((*pipelineIter)->getTestedShader());
        }
    }

    if (failedShaders.size())
    {
        DE_ASSERT(m_shaderPipelines.size());
        std::ostringstream oss;
        oss << (m_shaderPipelines.at(0)->getFileNameSuffixOffset() == readInvocationSuffix ? "readInvocation()" :
                                                                                             "readFirstInvocation()")
            << " failed in the following shader(s): ";

        for (decltype(failedShaders.size()) i = 0; i < failedShaders.size(); ++i)
        {
            if (i)
                oss << ", ";
            oss << glu::getShaderTypeName(failedShaders.at(i));
        }

        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, oss.str().c_str());
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }

    return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context.
 **/
ShaderBallotTests::ShaderBallotTests(deqp::Context &context)
    : TestCaseGroup(context, "shader_ballot_tests", "Verify conformance of CTS_ARB_shader_ballot implementation")
{
}

/** Initializes the shader_ballot test group.
 *
 **/
void ShaderBallotTests::init(void)
{
    addChild(new ShaderBallotAvailabilityTestCase(m_context));
    addChild(new ShaderBallotBitmasksTestCase(m_context));
    addChild(new ShaderBallotFunctionBallotTestCase(m_context));
    addChild(new ShaderBallotFunctionReadTestCase(m_context));
}
} // namespace gl4cts
