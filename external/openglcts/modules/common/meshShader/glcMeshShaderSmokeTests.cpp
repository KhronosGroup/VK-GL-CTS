/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 AMD Corporation.
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
 * \brief MeshShader smoke tests
 */ /*--------------------------------------------------------------------*/

#include "glcMeshShaderSmokeTests.hpp"
#include "glcMeshShaderTestsUtils.hpp"

#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

#include <iostream>
#include <vector>

namespace glc
{
namespace meshShader
{

const std::string frag = "#version 460\n"
                         "#extension GL_EXT_mesh_shader : enable\n"
                         "\n"
                         "layout (location=0) in perprimitiveEXT vec4 triangleColor;\n"
                         "layout (location=0) out vec4 outColor;\n"
                         "\n"
                         "void main ()\n"
                         "{\n"
                         "    outColor = triangleColor;\n"
                         "}\n";

bool MeshOnlyTriangleCase::initProgram()
{
    std::ostringstream mesh;
    mesh << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         // We will actually output a single triangle and most invocations will do no work.
         << "layout(local_size_x=8, local_size_y=4, local_size_z=4) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         // Unique vertex coordinates.
         << "layout (binding=0) uniform CoordsBuffer {\n"
         << "    vec4 coords[3];\n"
         << "} cb;\n"
         // Unique vertex indices.
         << "layout (binding=1, std430) readonly buffer IndexBuffer {\n"
         << "    uint indices[3];\n"
         << "} ib;\n"
         << "\n"
         // Triangle color.
         << "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    SetMeshOutputsEXT(3u, 1u);\n"
         << "    triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "\n"
         << "    const uint vertexIndex = gl_LocalInvocationIndex;\n"
         << "    if (vertexIndex < 3u)\n"
         << "    {\n"
         << "        const uint coordsIndex = ib.indices[vertexIndex];\n"
         << "        gl_MeshVerticesEXT[vertexIndex].gl_Position = cb.coords[coordsIndex];\n"
         << "    }\n"
         << "    if (vertexIndex == 0u)\n"
         << "    {\n"
         << "        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
         << "    }\n"
         << "}\n";

    m_params.program = createProgram(nullptr, mesh.str().c_str(), frag.c_str());
    return m_params.program != 0 ? true : false;
}

bool MeshTaskTriangleCase::initProgram()
{
    std::string taskDataDecl = "struct TaskData {\n"
                               "    uint triangleIndex;\n"
                               "};\n"
                               "taskPayloadSharedEXT TaskData td;\n";

    std::ostringstream task;
    task
        // Each work group spawns 1 task each (2 in total) and each task will draw 1 triangle.
        << "#version 460\n"
        << "#extension GL_EXT_mesh_shader : enable\n"
        << "\n"
        << "layout(local_size_x=8, local_size_y=4, local_size_z=4) in;\n"
        << "\n"
        << taskDataDecl << "\n"
        << "void main ()\n"
        << "{\n"
        << "    if (gl_LocalInvocationIndex == 0u)\n"
        << "    {\n"
        << "        td.triangleIndex = gl_WorkGroupID.x;\n"
        << "    }\n"
        << "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
        << "}\n";
    ;

    std::ostringstream mesh;
    mesh << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         // We will actually output a single triangle and most invocations will do no work.
         << "layout(local_size_x=32, local_size_y=1, local_size_z=1) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         // Unique vertex coordinates.
         << "layout (binding=0) uniform CoordsBuffer {\n"
         << "    vec4 coords[4];\n"
         << "} cb;\n"
         // Unique vertex indices.
         << "layout (binding=1, std430) readonly buffer IndexBuffer {\n"
         << "    uint indices[6];\n"
         << "} ib;\n"
         << "\n"
         // Triangle color.
         << "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
         << "\n"
         << taskDataDecl << "\n"
         << "void main ()\n"
         << "{\n"
         << "    SetMeshOutputsEXT(3u, 1u);\n"
         << "\n"
         // Each "active" invocation will copy one vertex.
         << "    const uint triangleVertex = gl_LocalInvocationIndex;\n"
         << "    const uint indexArrayPos  = td.triangleIndex * 3u + triangleVertex;\n"
         << "\n"
         << "    if (triangleVertex < 3u)\n"
         << "    {\n"
         << "        const uint coordsIndex = ib.indices[indexArrayPos];\n"
         // Copy vertex coordinates.
         << "        gl_MeshVerticesEXT[triangleVertex].gl_Position = cb.coords[coordsIndex];\n"
         // Index renumbering: final indices will always be 0, 1, 2.
         << "    }\n"
         << "    if (triangleVertex == 0u)\n"
         << "    {\n"
         << "        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
         << "        triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "    }\n"
         << "}\n";

    m_params.program = createProgram(task.str().c_str(), mesh.str().c_str(), frag.c_str());
    return m_params.program != 0 ? true : false;
}

bool TaskOnlyTriangleCase::initProgram()
{
    // The task shader does not spawn any mesh shader invocations.
    std::ostringstream task;
    task << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout(local_size_x=1) in;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    EmitMeshTasksEXT(0u, 0u, 0u);\n"
         << "}\n";

    // Same shader as the mesh only case, but it should not be launched.
    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         // We will actually output a single triangle and most invocations will do no work.
         << "layout(local_size_x=8, local_size_y=4, local_size_z=4) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         << "layout (binding=0) uniform CoordsBuffer {\n"
         << "    vec4 coords[3];\n"
         << "} cb;\n"
         << "layout (binding=1, std430) readonly buffer IndexBuffer {\n"
         << "    uint indices[3];\n"
         << "} ib;\n"
         << "\n"
         << "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    SetMeshOutputsEXT(3u, 1u);\n"
         << "    triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "\n"
         << "    const uint vertexIndex = gl_LocalInvocationIndex;\n"
         << "    if (vertexIndex < 3u)\n"
         << "    {\n"
         << "        const uint coordsIndex = ib.indices[vertexIndex];\n"
         << "        gl_MeshVerticesEXT[vertexIndex].gl_Position = cb.coords[coordsIndex];\n"
         << "    }\n"
         << "    if (vertexIndex == 0u)\n"
         << "    {\n"
         << "        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
         << "    }\n"
         << "}\n";

    m_params.program = createProgram(task.str().c_str(), mesh.str().c_str(), frag.c_str());
    return m_params.program != 0 ? true : false;
}

void MeshTriangleCase::init()
{
    // Extension check
    TCU_CHECK_AND_THROW(NotSupportedError, m_context.getContextInfo().isExtensionSupported("GL_EXT_mesh_shader"),
                        "GL_EXT_mesh_shader is not supported");
}

tcu::TestNode::IterateResult MeshTriangleCase::iterate()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const ExtFunctions &ext  = ExtFunctions(m_context.getRenderContext());

    const uint32_t width  = 8;
    const uint32_t height = 8;

    // initialize the program
    if (!initProgram())
    {
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return tcu::TestNode::STOP;
    }
    gl.useProgram(m_params.program);

    // bind buffers
    glw::GLuint coordsBuffer;
    gl.genBuffers(1, &coordsBuffer);
    gl.bindBuffer(GL_UNIFORM_BUFFER, coordsBuffer);
    gl.bufferData(GL_UNIFORM_BUFFER, m_params.vertexCoords.size() * sizeof(tcu::Vec4), m_params.vertexCoords.data(),
                  GL_STATIC_DRAW);
    gl.bindBuffer(GL_UNIFORM_BUFFER, 0);

    glw::GLuint indicesBuffer;
    gl.genBuffers(1, &indicesBuffer);
    gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, indicesBuffer);
    gl.bufferData(GL_SHADER_STORAGE_BUFFER, m_params.vertexIndices.size() * sizeof(uint32_t),
                  m_params.vertexIndices.data(), GL_STATIC_DRAW);
    gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    gl.bindBufferBase(GL_UNIFORM_BUFFER, 0, coordsBuffer);
    gl.bindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indicesBuffer);

    // Set pipeline states
    gl.scissor(0, 0, width, height);
    gl.enable(GL_SCISSOR_TEST);
    gl.viewport(0, 0, width, height);

    gl.clearColor(0.1f, 0.1f, 0.1f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw triangle.
    ext.DrawMeshTasksEXT(m_params.taskCount, 1, 1);

    // Output buffer.
    glw::GLubyte pixels[width * height * 4];
    gl.readPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Invalidate alloc.
    auto fmt = tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
    tcu::ConstPixelBufferAccess outPixels(fmt, 8, 8, 1, pixels);

    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.01f); // The color can be represented exactly.

    if (!tcu::floatThresholdCompare(log, "Result", "", m_params.expectedColor, outPixels, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
    {
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return tcu::TestNode::STOP;
    }

    m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");

    return tcu::TestNode::STOP;
}

MeshShaderSmokeTestsGroup::MeshShaderSmokeTestsGroup(deqp::Context &context)
    : TestCaseGroup(context, "smokeTests", "Mesh shader smoke tests")
{
}

void MeshShaderSmokeTestsGroup::init()
{
    addChild(new MeshOnlyTriangleCase(m_context, "mesh_only_shader_triangle", "Test mesh shader only"));
    addChild(new MeshTaskTriangleCase(m_context, "mesh_task_shader_triangle", "Test task and mesh shader"));
    addChild(new TaskOnlyTriangleCase(m_context, "task_only_shader_triangle", "Test task shader only"));
}

} // namespace meshShader
} // namespace glc
