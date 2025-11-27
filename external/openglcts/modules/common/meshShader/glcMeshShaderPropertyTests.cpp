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

#include "glcMeshShaderPropertyTests.hpp"
#include "glcMeshShaderTestsUtils.hpp"

#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include <iostream>
#include <vector>

namespace glc
{
namespace meshShader
{

using namespace glw;

void MeshPropertyCase::getMSProperties(MeshShaderProperties &properties)
{
    const Functions &gl = m_context.getRenderContext().getFunctions();
    gl.getIntegerv(GL_MAX_TASK_PAYLOAD_SIZE_EXT, &properties.maxTaskPayloadSize);
    gl.getIntegerv(GL_MAX_TASK_SHARED_MEMORY_SIZE_EXT, &properties.maxTaskSharedMemorySize);
    gl.getIntegerv(GL_MAX_TASK_PAYLOAD_AND_SHARED_MEMORY_SIZE_EXT, &properties.maxTaskPayloadAndSharedMemorySize);
    gl.getIntegerv(GL_MAX_MESH_SHARED_MEMORY_SIZE_EXT, &properties.maxMeshSharedMemorySize);
    gl.getIntegerv(GL_MAX_MESH_PAYLOAD_AND_SHARED_MEMORY_SIZE_EXT, &properties.maxMeshPayloadAndSharedMemorySize);
    gl.getIntegerv(GL_MAX_MESH_OUTPUT_MEMORY_SIZE_EXT, &properties.maxMeshOutputMemorySize);
    gl.getIntegerv(GL_MAX_MESH_PAYLOAD_AND_OUTPUT_MEMORY_SIZE_EXT, &properties.maxMeshPayloadAndOutputMemorySize);
    gl.getIntegerv(GL_MAX_MESH_OUTPUT_COMPONENTS_EXT, &properties.maxMeshOutputComponents);
    gl.getIntegerv(GL_MAX_MESH_OUTPUT_VERTICES_EXT, &properties.maxMeshOutputVertices);
    gl.getIntegerv(GL_MAX_MESH_OUTPUT_PRIMITIVES_EXT, &properties.maxMeshOutputPrimitives);
    gl.getIntegerv(GL_MAX_MESH_OUTPUT_LAYERS_EXT, &properties.maxMeshOutputLayers);
    gl.getIntegerv(GL_MAX_MESH_MULTIVIEW_VIEW_COUNT_EXT, &properties.maxMeshMultiviewViewCount);
    gl.getIntegerv(GL_MESH_OUTPUT_PER_VERTEX_GRANULARITY_EXT, &properties.meshOutputPerVertexGranularity);
    gl.getIntegerv(GL_MESH_OUTPUT_PER_PRIMITIVE_GRANULARITY_EXT, &properties.meshOutputPerPrimitiveGranularity);
}

void MeshPropertyCase::init()
{
    // Extension check
    TCU_CHECK_AND_THROW(NotSupportedError, m_context.getContextInfo().isExtensionSupported("GL_EXT_mesh_shader"),
                        "GL_EXT_mesh_shader is not supported");
}

void TaskPayloadShMemSizeCase::init()
{
    MeshPropertyCase::init();

    MeshShaderProperties properties;
    getMSProperties(properties);

    checkSupport(properties);

    const auto maxMeshPayloadSize =
        std::min(properties.maxMeshPayloadAndOutputMemorySize, properties.maxMeshPayloadAndSharedMemorySize);
    const auto maxPayloadElements = std::min(properties.maxTaskPayloadSize, maxMeshPayloadSize) / kElementSize;
    const auto maxShMemElements   = properties.maxTaskSharedMemorySize / kElementSize;
    const auto maxTotalElements   = properties.maxTaskPayloadAndSharedMemorySize / kElementSize;

    if (m_params.testType == PayLoadShMemSizeType::PAYLOAD)
    {
        m_sharedMemoryElements = 0u;
        m_payloadElements      = std::min(maxTotalElements, maxPayloadElements);
    }
    else if (m_params.testType == PayLoadShMemSizeType::SHARED_MEMORY)
    {
        m_payloadElements      = 0u;
        m_sharedMemoryElements = std::min(maxTotalElements, maxShMemElements);
    }
    else
    {
        uint32_t *minPtr;
        uint32_t minVal;
        uint32_t *maxPtr;
        uint32_t maxVal;

        // Divide them as evenly as possible getting them as closest as possible to maxTotalElements.
        if (maxPayloadElements < maxShMemElements)
        {
            minPtr = &m_payloadElements;
            minVal = maxPayloadElements;

            maxPtr = &m_sharedMemoryElements;
            maxVal = maxShMemElements;
        }
        else
        {
            minPtr = &m_sharedMemoryElements;
            minVal = maxShMemElements;

            maxPtr = &m_payloadElements;
            maxVal = maxPayloadElements;
        }

        *minPtr = std::min(minVal, maxTotalElements / 2u);
        *maxPtr = std::min(maxTotalElements - (*minPtr), maxVal);
    }
}

void MeshPayloadShMemSizeCase::init()
{
    MeshPropertyCase::init();

    MeshShaderProperties properties;
    getMSProperties(properties);

    checkSupport(properties);

    const auto maxTaskPayloadSize =
        std::min(properties.maxTaskPayloadAndSharedMemorySize, properties.maxTaskPayloadSize);
    const auto maxMeshPayloadSize =
        std::min(properties.maxMeshPayloadAndOutputMemorySize, properties.maxMeshPayloadAndSharedMemorySize);
    const auto maxPayloadElements = std::min(maxTaskPayloadSize, maxMeshPayloadSize) / kElementSize;
    const auto maxShMemElements   = properties.maxMeshSharedMemorySize / kElementSize;
    const auto maxTotalElements   = properties.maxMeshPayloadAndSharedMemorySize / kElementSize;

    if (m_params.testType == PayLoadShMemSizeType::PAYLOAD)
    {
        m_sharedMemoryElements = 0u;
        m_payloadElements      = std::min(maxTotalElements, maxPayloadElements);
    }
    else if (m_params.testType == PayLoadShMemSizeType::SHARED_MEMORY)
    {
        m_payloadElements      = 0u;
        m_sharedMemoryElements = std::min(maxTotalElements, maxShMemElements);
    }
    else
    {
        uint32_t *minPtr;
        uint32_t minVal;
        uint32_t *maxPtr;
        uint32_t maxVal;

        // Divide them as evenly as possible getting them as closest as possible to maxTotalElements.
        if (maxPayloadElements < maxShMemElements)
        {
            minPtr = &m_payloadElements;
            minVal = maxPayloadElements;

            maxPtr = &m_sharedMemoryElements;
            maxVal = maxShMemElements;
        }
        else
        {
            minPtr = &m_sharedMemoryElements;
            minVal = maxShMemElements;

            maxPtr = &m_payloadElements;
            maxVal = maxPayloadElements;
        }

        *minPtr = std::min(minVal, maxTotalElements / 2u);
        *maxPtr = std::min(maxTotalElements - (*minPtr), maxVal);
    }
}

bool TaskPayloadShMemSizeCase::initProgram()
{
    std::ostringstream scStream;
    scStream << "const uint payloadElements =" << m_payloadElements << ";\n"
             << "const uint sharedMemoryElements =" << m_sharedMemoryElements << ";\n";

    const std::string scDecl = scStream.str();
    const std::string dsDecl = "layout (binding=0, std430) buffer ResultBlock {\n"
                               "    uint sharedOK;\n"
                               "    uint payloadOK;\n"
                               "} result;\n";

    std::string taskData;
    std::string taskPayloadBody;
    std::string meshPayloadBody;

    if (m_params.hasPayload())
    {
        std::ostringstream taskDataStream;
        taskDataStream << "struct TaskData {\n"
                       << "    uint elements[payloadElements];\n"
                       << "};\n"
                       << "taskPayloadSharedEXT TaskData td;\n";
        taskData = taskDataStream.str();

        std::ostringstream taskBodyStream;
        taskBodyStream << "    const uint payloadElementsPerInvocation = uint(ceil(float(payloadElements) / float("
                       << kLocalInvocations << ")));\n"
                       << "    for (uint i = 0u; i < payloadElementsPerInvocation; ++i) {\n"
                       << "        const uint elemIdx = payloadElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
                       << "        if (elemIdx < payloadElements) {\n"
                       << "            td.elements[elemIdx] = elemIdx + 2000u;\n"
                       << "        }\n"
                       << "    }\n"
                       << "\n";
        taskPayloadBody = taskBodyStream.str();

        std::ostringstream meshBodyStream;
        meshBodyStream << "    bool allOK = true;\n"
                       << "    for (uint i = 0u; i < payloadElements; ++i) {\n"
                       << "        if (td.elements[i] != i + 2000u) {\n"
                       << "            allOK = false;\n"
                       << "            break;\n"
                       << "        }\n"
                       << "    }\n"
                       << "    result.payloadOK = (allOK ? 1u : 0u);\n"
                       << "\n";
        meshPayloadBody = meshBodyStream.str();
    }
    else
    {
        meshPayloadBody = "    result.payloadOK = 1u;\n";
    }

    std::string sharedData;
    std::string taskSharedDataBody;

    if (m_params.hasSharedMemory())
    {
        sharedData = "shared uint sharedElements[sharedMemoryElements];\n";

        std::ostringstream bodyStream;
        bodyStream << "    const uint shMemElementsPerInvocation = uint(ceil(float(sharedMemoryElements) / float("
                   << kLocalInvocations << ")));\n"
                   << "    for (uint i = 0u; i < shMemElementsPerInvocation; ++i) {\n"
                   << "        const uint elemIdx = shMemElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
                   << "        if (elemIdx < sharedMemoryElements) {\n"
                   << "            sharedElements[elemIdx] = elemIdx * 2u + 1000u;\n" // Write
                   << "        }\n"
                   << "    }\n"
                   << "    memoryBarrierShared();\n"
                   << "    barrier();\n"
                   << "    for (uint i = 0u; i < shMemElementsPerInvocation; ++i) {\n"
                   << "        const uint elemIdx = shMemElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
                   << "        if (elemIdx < sharedMemoryElements) {\n"
                   << "            const uint accessIdx = sharedMemoryElements - 1u - elemIdx;\n"
                   << "            sharedElements[accessIdx] += accessIdx;\n" // Read+Write a different element.
                   << "        }\n"
                   << "    }\n"
                   << "    memoryBarrierShared();\n"
                   << "    barrier();\n"
                   << "    if (gl_LocalInvocationIndex == 0u) {\n"
                   << "        bool allOK = true;\n"
                   << "        for (uint i = 0u; i < sharedMemoryElements; ++i) {\n"
                   << "            if (sharedElements[i] != i*3u + 1000u) {\n"
                   << "                allOK = false;\n"
                   << "                break;\n"
                   << "            }\n"
                   << "        }\n"
                   << "        result.sharedOK = (allOK ? 1u : 0u);\n"
                   << "    }\n"
                   << "\n";
        taskSharedDataBody = bodyStream.str();
    }
    else
    {
        taskSharedDataBody = "    if (gl_LocalInvocationIndex == 0u) {\n"
                             "        result.sharedOK = 1u;\n"
                             "    }\n";
    }

    std::ostringstream task;
    task << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
         << scDecl << dsDecl << taskData << sharedData << "\n"
         << "void main () {\n"
         << taskSharedDataBody << taskPayloadBody << "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
         << "}\n";

    std::ostringstream mesh;
    mesh << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (triangles) out;\n"
         << "layout (max_vertices=3, max_primitives=1) out;\n"
         << scDecl << dsDecl << taskData << "\n"
         << "void main () {\n"
         << meshPayloadBody << "    SetMeshOutputsEXT(0u, 0u);\n"
         << "}\n";

    const std::string frag = "#version 460\n"
                             "\n"
                             "layout (location=0) out vec4 outColor;\n"
                             "\n"
                             "void main ()\n"
                             "{\n"
                             "    outColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                             "}\n";

    m_params.program = createProgram(task.str().c_str(), mesh.str().c_str(), frag.c_str());
    return m_params.program != 0 ? true : false;
}

bool MeshPayloadShMemSizeCase::initProgram()
{
    std::ostringstream scStream;
    scStream << "const uint payloadElements =" << m_payloadElements << ";\n"
             << "const uint sharedMemoryElements =" << m_sharedMemoryElements << ";\n";

    const std::string scDecl = scStream.str();

    const std::string dsDecl = "layout (binding=0, std430) buffer ResultBlock {\n"
                               "    uint sharedOK;\n"
                               "    uint payloadOK;\n"
                               "} result;\n";

    std::string taskData;
    std::string taskPayloadBody;
    std::string meshPayloadBody;

    if (m_params.hasPayload())
    {
        std::ostringstream taskDataStream;
        taskDataStream << "struct TaskData {\n"
                       << "    uint elements[payloadElements];\n"
                       << "};\n"
                       << "taskPayloadSharedEXT TaskData td;\n";
        taskData = taskDataStream.str();

        std::ostringstream taskBodyStream;
        taskBodyStream << "    const uint payloadElementsPerInvocation = uint(ceil(float(payloadElements) / float("
                       << kLocalInvocations << ")));\n"
                       << "    for (uint i = 0u; i < payloadElementsPerInvocation; ++i) {\n"
                       << "        const uint elemIdx = payloadElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
                       << "        if (elemIdx < payloadElements) {\n"
                       << "            td.elements[elemIdx] = elemIdx + 2000u;\n"
                       << "        }\n"
                       << "    }\n"
                       << "\n";
        taskPayloadBody = taskBodyStream.str();

        std::ostringstream meshBodyStream;
        meshBodyStream << "    bool allOK = true;\n"
                       << "    for (uint i = 0u; i < payloadElements; ++i) {\n"
                       << "        if (td.elements[i] != i + 2000u) {\n"
                       << "            allOK = false;\n"
                       << "            break;\n"
                       << "        }\n"
                       << "    }\n"
                       << "    result.payloadOK = (allOK ? 1u : 0u);\n"
                       << "\n";
        meshPayloadBody = meshBodyStream.str();
    }
    else
    {
        meshPayloadBody = "    result.payloadOK = 1u;\n";
    }

    std::string sharedData;
    std::string meshSharedDataBody;

    if (m_params.hasSharedMemory())
    {
        sharedData = "shared uint sharedElements[sharedMemoryElements];\n";

        std::ostringstream bodyStream;
        bodyStream << "    const uint shMemElementsPerInvocation = uint(ceil(float(sharedMemoryElements) / float("
                   << kLocalInvocations << ")));\n"
                   << "    for (uint i = 0u; i < shMemElementsPerInvocation; ++i) {\n"
                   << "        const uint elemIdx = shMemElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
                   << "        if (elemIdx < sharedMemoryElements) {\n"
                   << "            sharedElements[elemIdx] = elemIdx * 2u + 1000u;\n" // Write
                   << "        }\n"
                   << "    }\n"
                   << "    memoryBarrierShared();\n"
                   << "    barrier();\n"
                   << "    for (uint i = 0u; i < shMemElementsPerInvocation; ++i) {\n"
                   << "        const uint elemIdx = shMemElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
                   << "        if (elemIdx < sharedMemoryElements) {\n"
                   << "            const uint accessIdx = sharedMemoryElements - 1u - elemIdx;\n"
                   << "            sharedElements[accessIdx] += accessIdx;\n" // Read+Write a different element.
                   << "        }\n"
                   << "    }\n"
                   << "    memoryBarrierShared();\n"
                   << "    barrier();\n"
                   << "    if (gl_LocalInvocationIndex == 0u) {\n"
                   << "        bool allOK = true;\n"
                   << "        for (uint i = 0u; i < sharedMemoryElements; ++i) {\n"
                   << "            if (sharedElements[i] != i*3u + 1000u) {\n"
                   << "                allOK = false;\n"
                   << "                break;\n"
                   << "            }\n"
                   << "        }\n"
                   << "        result.sharedOK = (allOK ? 1u : 0u);\n"
                   << "    }\n"
                   << "\n";
        meshSharedDataBody = bodyStream.str();
    }
    else
    {
        meshSharedDataBody = "    if (gl_LocalInvocationIndex == 0u) {\n"
                             "        result.sharedOK = 1u;\n"
                             "    }\n";
    }

    std::ostringstream task;
    if (m_params.hasPayload())
    {
        task << "#version 460\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "\n"
             << "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
             << scDecl << dsDecl << taskData << "\n"
             << "void main () {\n"
             << taskPayloadBody << "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
             << "}\n";
    }

    std::ostringstream mesh;
    mesh << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
         << "layout (triangles) out;\n"
         << "layout (max_vertices=3, max_primitives=1) out;\n"
         << scDecl << dsDecl << taskData << sharedData << "\n"
         << "void main () {\n"
         << meshSharedDataBody << meshPayloadBody << "    SetMeshOutputsEXT(0u, 0u);\n"
         << "}\n";

    const std::string frag = "#version 460\n"
                             "\n"
                             "layout (location=0) out vec4 outColor;\n"
                             "\n"
                             "void main ()\n"
                             "{\n"
                             "    outColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                             "}\n";

    m_params.program =
        createProgram(m_params.hasPayload() ? task.str().c_str() : nullptr, mesh.str().c_str(), frag.c_str());
    return m_params.program != 0 ? true : false;
}

void TaskPayloadShMemSizeCase::checkSupport(MeshShaderProperties &properties) const
{
    const int minSize = kLocalInvocations * kElementSize;

    // Note: the min required values for these properties in the spec would pass these checks.
    if (properties.maxTaskPayloadSize < minSize)
        TCU_FAIL("Invalid maxTaskPayloadSize");

    if (properties.maxTaskSharedMemorySize < minSize)
        TCU_FAIL("Invalid maxTaskSharedMemorySize");

    if (properties.maxTaskPayloadAndSharedMemorySize < minSize)
        TCU_FAIL("Invalid maxTaskPayloadAndSharedMemorySize");

    if (properties.maxMeshPayloadAndSharedMemorySize < minSize)
        TCU_FAIL("Invalid maxMeshPayloadAndSharedMemorySize");
}

void MeshPayloadShMemSizeCase::checkSupport(MeshShaderProperties &properties) const
{
    const bool requireTask = m_params.hasPayload();
    const int minSize      = kLocalInvocations * kElementSize;

    // Note: the min required values for these properties in the spec would pass these checks.
    if (requireTask)
    {
        if (properties.maxTaskPayloadSize < minSize)
            TCU_FAIL("Invalid maxTaskPayloadSize");

        if (properties.maxTaskPayloadAndSharedMemorySize < minSize)
            TCU_FAIL("Invalid maxTaskPayloadAndSharedMemorySize");
    }

    if (properties.maxMeshSharedMemorySize < minSize)
        TCU_FAIL("Invalid maxMeshSharedMemorySize");

    if (properties.maxMeshPayloadAndSharedMemorySize < minSize)
        TCU_FAIL("Invalid maxMeshPayloadAndSharedMemorySize");

    if (properties.maxMeshPayloadAndOutputMemorySize < minSize)
        TCU_FAIL("Invalid maxMeshPayloadAndOutputMemorySize");
}

tcu::TestNode::IterateResult PayloadShMemSizeCase::iterate()
{
    const Functions &gl     = m_context.getRenderContext().getFunctions();
    const ExtFunctions &ext = ExtFunctions(m_context.getRenderContext());

    if (!initProgram())
    {
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return tcu::TestNode::STOP;
    }

    uint32_t result[2] = {0, 0};
    GLuint resultBlock;
    gl.genBuffers(1, &resultBlock);
    gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, resultBlock);
    gl.bufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(uint32_t), result, GL_STATIC_READ);

    gl.useProgram(m_params.program);
    gl.bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, resultBlock);

    ext.DrawMeshTasksEXT(1u, 1u, 1u);

    uint32_t *result1 =
        (uint32_t *)gl.mapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t) * 2, GL_MAP_READ_BIT);

    struct
    {
        uint32_t sharedOK;
        uint32_t payloadOK;
    } resultData;

    deMemcpy(&resultData, result1, sizeof(resultData));

    gl.unmapBuffer(GL_SHADER_STORAGE_BUFFER);

    if (resultData.sharedOK != 1u)
        TCU_FAIL("Unexpected shared memory result: " + std::to_string(resultData.sharedOK));

    if (resultData.payloadOK != 1u)
        TCU_FAIL("Unexpected payload result: " + std::to_string(resultData.payloadOK));

    m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");

    return tcu::TestNode::STOP;
}

void MaxMeshOutputSizeCase::init()
{
    MeshPropertyCase::init();

    m_numViews = (m_params.isMultiView() ? kNumViews : 1u);

    MeshShaderProperties properties;
    getMSProperties(properties);

    checkSupport(properties);

    const uint32_t maxOutSize =
        std::min(properties.maxMeshOutputMemorySize, properties.maxMeshPayloadAndOutputMemorySize);
    const uint32_t maxMeshPayloadSize =
        std::min(properties.maxMeshPayloadAndSharedMemorySize, properties.maxMeshPayloadAndOutputMemorySize);
    const uint32_t maxTaskPayloadSize =
        std::min(properties.maxTaskPayloadSize, properties.maxTaskPayloadAndSharedMemorySize);
    const uint32_t maxPayloadSize = std::min(maxMeshPayloadSize, maxTaskPayloadSize);
    const uint32_t numViewFactor  = (m_params.viewIndexInMesh() ? kNumViews : 1u);

    uint32_t payloadSize;
    uint32_t outSize;

    if (m_params.usePayload)
    {
        const uint32_t totalMax = maxOutSize + maxPayloadSize;

        if (totalMax <= static_cast<uint32_t>(properties.maxMeshPayloadAndOutputMemorySize))
        {
            payloadSize = maxPayloadSize;
            outSize     = maxOutSize;
        }
        else
        {
            payloadSize = maxPayloadSize;
            outSize     = properties.maxMeshPayloadAndOutputMemorySize - payloadSize;
        }
    }
    else
    {
        payloadSize = 0u;
        outSize     = maxOutSize;
    }

    // This uses the equation in "Mesh Shader Output" spec section. Note per-vertex data already has gl_Position and gl_PointSize.
    // Also note gl_PointSize uses 1 effective location (4 scalar components) despite being a float.
    const uint32_t granularity =
        ((m_params.locationType == LocationType::PER_PRIMITIVE) ? properties.meshOutputPerPrimitiveGranularity :
                                                                  properties.meshOutputPerVertexGranularity);
    const uint32_t actualPoints      = de::roundUp(kMaxPoints, granularity);
    const uint32_t sizeMultiplier    = actualPoints * kUvec4Size;
    const uint32_t builtinDataSize   = (16u /*gl_Position*/ + 16u /*gl_PointSize*/) * actualPoints;
    const uint32_t locationsDataSize = (outSize - builtinDataSize) / numViewFactor;
    const uint32_t maxTotalLocations =
        properties.maxMeshOutputComponents / kUvec4Comp - 2u; // gl_Position and gl_PointSize use 1 location each.
    const uint32_t locationCount = std::min(locationsDataSize / sizeMultiplier, maxTotalLocations);

    m_payloadElements = payloadSize / kPayloadElementSize;
    m_locationCount   = locationCount;

    auto &log = m_context.getTestContext().getLog();
    {
        const auto actualOuputSize = builtinDataSize + locationCount * sizeMultiplier * numViewFactor;

        log << tcu::TestLog::Message << "Payload elements: " << m_payloadElements << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << "Location count: " << m_locationCount << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message
            << "Max mesh payload and output size (bytes): " << properties.maxMeshPayloadAndOutputMemorySize
            << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << "Max output size (bytes): " << maxOutSize << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << "Payload size (bytes): " << payloadSize << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << "Output data size (bytes): " << actualOuputSize << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << "Output + payload size (bytes): " << (payloadSize + actualOuputSize)
            << tcu::TestLog::EndMessage;
    }
}

bool MaxMeshOutputSizeCase::initProgram()
{
    const std::string locationQualifier =
        ((m_params.locationType == LocationType::PER_PRIMITIVE) ? "perprimitiveEXT" : "");
    const std::string multiViewExtDecl = "#extension GL_OVR_multiview2 : enable\n";

    std::ostringstream scStream;
    scStream << "const uint payloadElements =" << m_payloadElements << ";\n"
             << "const uint locationCount =" << m_locationCount << ";\n";

    const std::string scDecl = scStream.str();

    std::string taskPayload;
    std::string payloadVerification = "    bool payloadOK = true;\n";
    std::string locStruct           = "struct LocationBlock {\n"
                                      "    uvec4 elements[locationCount];\n"
                                      "};\n";
    std::string taskStr;
    std::string meshStr;
    std::string fragStr;

    if (m_params.usePayload)
    {
        taskPayload = "struct TaskData {\n"
                      "    uint elements[payloadElements];\n"
                      "};\n"
                      "taskPayloadSharedEXT TaskData td;\n";

        std::ostringstream task;
        task << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "\n"
             << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << scDecl << taskPayload << "\n"
             << "void main (void) {\n"
             << "    for (uint i = 0; i < payloadElements; ++i) {\n"
             << "        td.elements[i] = 1000000u + i;\n"
             << "    }\n"
             << "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
             << "}\n";

        payloadVerification += "    for (uint i = 0; i < payloadElements; ++i) {\n"
                               "        if (td.elements[i] != 1000000u + i) {\n"
                               "            payloadOK = false;\n"
                               "            break;\n"
                               "        }\n"
                               "    }\n";
        taskStr = task.str();
    }

    // Do values depend on view indices?
    const bool valFromViewIndex       = m_params.viewIndexInMesh();
    const std::string extraCompOffset = (valFromViewIndex ? "(4u * uint(gl_ViewID_OVR))" : "0u");

    {
        const std::string multiViewExt = (valFromViewIndex ? multiViewExtDecl : "");

        std::ostringstream mesh;
        mesh << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << multiViewExt << "\n"
             << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "layout (points) out;\n"
             << "layout (max_vertices=" << kMaxPoints << ", max_primitives=" << kMaxPoints << ") out;\n"
             << "\n"
             << "out gl_MeshPerVertexEXT {\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_MeshVerticesEXT[];\n"
             << "\n"
             << scDecl << taskPayload << "\n"
             << locStruct << "layout (location=0) out " << locationQualifier << " LocationBlock loc[];\n"
             << "\n"
             << "void main (void) {\n"
             << payloadVerification << "\n"
             << "    SetMeshOutputsEXT(" << kMaxPoints << ", " << kMaxPoints << ");\n"
             << "    const uint payloadOffset = (payloadOK ? 10u : 0u);\n"
             << "    const uint compOffset = " << extraCompOffset << ";\n"
             << "    for (uint pointIdx = 0u; pointIdx < " << kMaxPoints << "; ++pointIdx) {\n"
             << "        const float xCoord = ((float(pointIdx) + 0.5) / float(" << kMaxPoints << ")) * 2.0 - 1.0;\n"
             << "        gl_MeshVerticesEXT[pointIdx].gl_Position = vec4(xCoord, 0.0, 0.0, 1.0);\n"
             << "        gl_MeshVerticesEXT[pointIdx].gl_PointSize = 1.0f;\n"
             << "        gl_PrimitivePointIndicesEXT[pointIdx] = pointIdx;\n"
             << "        for (uint elemIdx = 0; elemIdx < locationCount; ++elemIdx) {\n"
             << "            const uint baseVal = 200000000u + 100000u * pointIdx + 1000u * elemIdx + payloadOffset;\n"
             << "            loc[pointIdx].elements[elemIdx] = uvec4(baseVal + 1u + compOffset, baseVal + 2u + "
                "compOffset, baseVal + 3u + compOffset, baseVal + 4u + compOffset);\n"
             << "        }\n"
             << "    }\n"
             << "}\n";

        meshStr = mesh.str();
    }

    {
        const std::string multiViewExt = (m_params.isMultiView() ? multiViewExtDecl : "");
        const std::string outColorMod  = (m_params.isMultiView() ? "    outColor.r += float(gl_ViewID_OVR);\n" : "");

        std::ostringstream frag;
        frag << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << multiViewExt << "\n"
             << "layout (location=0) out vec4 outColor;\n"
             << scDecl << locStruct << "layout (location=0) in "
             << (m_params.locationType == LocationType::PER_PRIMITIVE ? "" : "flat ") << locationQualifier
             << " LocationBlock loc;\n"
             << "\n"
             << "void main (void) {\n"
             << "    bool pointOK = true;\n"
             << "    const uint pointIdx = uint(gl_FragCoord.x);\n"
             << "    const uint expectedPayloadOffset = 10u;\n"
             << "    const uint compOffset = " << extraCompOffset << ";\n"
             << "    for (uint elemIdx = 0; elemIdx < locationCount; ++elemIdx) {\n"
             << "        const uint baseVal = 200000000u + 100000u * pointIdx + 1000u * elemIdx + "
                "expectedPayloadOffset;\n"
             << "        const uvec4 expectedVal = uvec4(baseVal + 1u + compOffset, baseVal + 2u + compOffset, baseVal "
                "+ 3u + compOffset, baseVal + 4u + compOffset);\n"
             << "        if (loc.elements[elemIdx] != expectedVal) {\n"
             << "            pointOK = false;\n"
             << "            break;\n"
             << "        }\n"
             << "    }\n"
             << "    const vec4 okColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
             << "    const vec4 failColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
             << "    outColor = (pointOK ? okColor : failColor);\n"
             << outColorMod << "}\n";

        fragStr = frag.str();
    }

    m_params.program = createProgram(m_params.usePayload ? taskStr.c_str() : nullptr, meshStr.c_str(), fragStr.c_str());
    return m_params.program != 0 ? true : false;
}

void MaxMeshOutputSizeCase::checkSupport(MeshShaderProperties &properties) const
{
    if (m_numViews > 1 && properties.maxMeshMultiviewViewCount == 1)
    {
        TCU_THROW(NotSupportedError, "maxMeshMultiviewViewCount too low");
    }
}

tcu::TestNode::IterateResult MaxMeshOutputSizeCase::iterate()
{
    const uint32_t width  = MaxMeshOutputSizeCase::kMaxPoints;
    const uint32_t height = 1u;
    const tcu::Vec4 expectedColor(0.0f, 0.0f, 1.0f, 1.0f);
    const tcu::Vec4 colorThreshold(0.0f, 0.0f, 0.0f, 0.0f);

    const Functions &gl     = m_context.getRenderContext().getFunctions();
    const ExtFunctions &ext = ExtFunctions(m_context.getRenderContext());

    // First create the array texture and multiview FBO.
    GLuint arrayTexture;
    GLuint multiviewFbo;
    gl.genTextures(1, &arrayTexture);
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, arrayTexture);
    gl.texStorage3D(GL_TEXTURE_2D_ARRAY, 1 /* num mipmaps */, GL_RGBA8, width, height, m_numViews /* num levels */);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Create array texture");
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, 0);

    gl.genFramebuffers(1, &multiviewFbo);
    gl.bindFramebuffer(GL_FRAMEBUFFER, multiviewFbo);
    if (m_numViews > 1)
    {
        gl.framebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, arrayTexture, 0 /* mip level */,
                                          0 /* base view index */, m_numViews /* num views */);
    }
    else
    {
        gl.framebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, arrayTexture, 0);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Create multiview FBO");
    uint32_t fboStatus = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus == GL_FRAMEBUFFER_UNSUPPORTED)
    {
        throw tcu::NotSupportedError("Framebuffer unsupported", "", __FILE__, __LINE__);
    }
    else if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        throw tcu::TestError("Failed to create framebuffer object", "", __FILE__, __LINE__);
    }

    if (!initProgram())
    {
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return tcu::TestNode::STOP;
    }

    gl.viewport(0, 0, width, height);
    gl.scissor(0, 0, width, height);
    gl.enable(GL_SCISSOR_TEST);

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gl.useProgram(m_params.program);

    ext.DrawMeshTasksEXT(1u, 1u, 1u);

    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);

    uint32_t bufSize = width * height * m_numViews * 4;
    std::vector<GLubyte> pixels(bufSize);
    gl.getTextureImage(arrayTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufSize, pixels.data());
    auto fmt = tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
    tcu::ConstPixelBufferAccess resultAccess(fmt, width, height, m_numViews, pixels.data());
    tcu::TextureLevel referenceLevel(fmt, width, height, m_numViews);
    tcu::PixelBufferAccess referenceAccess = referenceLevel.getAccess();

    for (uint32_t z = 0; z < m_numViews; ++z)
    {
        const auto layer = tcu::getSubregion(referenceAccess, 0, 0, z, width, height, 1);
        const tcu::Vec4 expectedLayerColor(static_cast<float>(z), expectedColor.y(), expectedColor.z(),
                                           expectedColor.w());
        tcu::clear(layer, expectedLayerColor);
    }

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, colorThreshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Check log for details");

    m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");

    return tcu::TestNode::STOP;
}

MeshShaderPropertyTestsGroup::MeshShaderPropertyTestsGroup(deqp::Context &context)
    : TestCaseGroup(context, "propertyTests", "Mesh shader property tests")
{
}

void MeshShaderPropertyTestsGroup::init()
{
    const struct
    {
        PayLoadShMemSizeType testType;
        const char *name;
    } taskPayloadShMemCases[] = {
        {PayLoadShMemSizeType::PAYLOAD, "task_payload_size"},
        {PayLoadShMemSizeType::SHARED_MEMORY, "task_shared_memory_size"},
        {PayLoadShMemSizeType::BOTH, "task_payload_and_shared_memory_size"},
    };

    for (const auto &taskPayloadShMemCase : taskPayloadShMemCases)
    {
        const TaskPayloadShMemSizeParams params{taskPayloadShMemCase.testType, 0};
        addChild(new TaskPayloadShMemSizeCase(m_context, taskPayloadShMemCase.name, params));
    }

    const struct
    {
        PayLoadShMemSizeType testType;
        const char *name;
    } meshPayloadShMemCases[] = {
        // No actual property for the first one, combines the two properties involving payload size.
        {PayLoadShMemSizeType::PAYLOAD, "mesh_payload_size"},
        {PayLoadShMemSizeType::SHARED_MEMORY, "mesh_shared_memory_size"},
        {PayLoadShMemSizeType::BOTH, "mesh_payload_and_shared_memory_size"},
    };
    for (const auto &meshPayloadShMemCase : meshPayloadShMemCases)
    {
        const MeshPayloadShMemSizeParams params{meshPayloadShMemCase.testType, 0};
        addChild(new MeshPayloadShMemSizeCase(m_context, meshPayloadShMemCase.name, params));
    }

    const struct
    {
        bool usePayload;
        const char *suffix;
    } meshOutputPayloadCases[] = {
        {false, "_without_payload"},
        {true, "_with_payload"},
    };

    const struct
    {
        LocationType locationType;
        const char *suffix;
    } locationTypeCases[] = {
        {LocationType::PER_PRIMITIVE, "_per_primitive"},
        {LocationType::PER_VERTEX, "_per_vertex"},
    };

    const struct
    {
        ViewIndexType viewIndexType;
        const char *suffix;
    } multiviewCases[] = {
        {ViewIndexType::NO_VIEW_INDEX, "_no_view_index"},
        {ViewIndexType::VIEW_INDEX_FRAG, "_view_index_in_frag"},
        {ViewIndexType::VIEW_INDEX_BOTH, "_view_index_in_mesh_and_frag"},
    };

    for (const auto &meshOutputPayloadCase : meshOutputPayloadCases)
    {
        for (const auto &locationTypeCase : locationTypeCases)
        {
            for (const auto &multiviewCase : multiviewCases)
            {
                const std::string name = std::string("max_mesh_output_size") + meshOutputPayloadCase.suffix +
                                         locationTypeCase.suffix + multiviewCase.suffix;
                const MaxMeshOutputParams params = {
                    meshOutputPayloadCase.usePayload, // bool usePayload;
                    locationTypeCase.locationType,    // LocationType locationType;
                    multiviewCase.viewIndexType,      // ViewIndexType viewIndexType;
                    0                                 // Program ID
                };

                addChild(new MaxMeshOutputSizeCase(m_context, name.c_str(), params));
            }
        }
    }
}

} // namespace meshShader
} // namespace glc
