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
 * \brief MeshShader api tests
 */ /*--------------------------------------------------------------------*/

#include "glcMeshShaderApiTests.hpp"
#include "glcMeshShaderTestsUtils.hpp"

#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"
#include "gluStrUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"

#include <iostream>
#include <vector>

namespace glc
{
namespace meshShader
{

using namespace glw;

const uint32_t width  = 32;
const uint32_t height = 64;

/** Check if error is equal to the expected, log if not.
 *
 *  @param [in] expected_error      Error to be expected.
 *  @param [in] function            Function name which is being tested (to be logged).
 *  @param [in] conditions          Conditions when the expected error shall occure (to be logged).
 *
 *  @return True if there is no error, false otherwise.
 */
bool MeshApiCase::ExpectError(glw::GLenum expected_error, const glw::GLchar *function, const glw::GLchar *conditions)
{
    /* Shortcut for GL functionality. */
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    bool is_ok = true;

    glw::GLenum error = GL_NO_ERROR;

    if (expected_error != (error = gl.getError()))
    {
        m_context.getTestContext().getLog() << tcu::TestLog::Message << function << " was expected to generate "
                                            << glu::getErrorStr(expected_error) << ", but " << glu::getErrorStr(error)
                                            << " was observed instead when " << conditions << tcu::TestLog::EndMessage;

        is_ok = false;
    }

    /* Clean additional possible errors. */
    while (gl.getError())
        ;

    return is_ok;
}

DrawMeshTasksIndirectCommandStruct getIndirectCommand(uint32_t blockSize, uint32_t dimCoord)
{
    DrawMeshTasksIndirectCommandStruct indirectCmd{1u, 1u, 1u};

    switch (dimCoord)
    {
    case 0u:
        indirectCmd.x = blockSize;
        break;
    case 1u:
        indirectCmd.y = blockSize;
        break;
    case 2u:
        indirectCmd.z = blockSize;
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    return indirectCmd;
}

std::ostream &operator<<(std::ostream &stream, DrawType drawType)
{
    switch (drawType)
    {
    case DrawType::DRAW:
        stream << "draw";
        break;
    case DrawType::DRAW_INDIRECT:
        stream << "draw_indirect";
        break;
    case DrawType::DRAW_INDIRECT_COUNT:
        stream << "draw_indirect_count";
        break;
    default:
        DE_ASSERT(false);
        break;
    }
    return stream;
}

bool MeshApiCase::initProgram()
{
    const std::string taskDataDecl = "struct TaskData {\n"
                                     "    uint blockNumber;\n"
                                     "    uint blockRow;\n"
                                     "};\n"
                                     "taskPayloadSharedEXT TaskData td;\n";
    std::ostringstream task;
    // Task shader if needed.
    if (m_params.useTask)
    {
        task << "#version 460\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "\n"
             << "layout (local_size_x=1) in;\n"
             << "\n"
             << "layout (location = 0) uniform uint dimCoord;\n"
             << "\n"
             << taskDataDecl << "\n"
             << "void main ()\n"
             << "{\n"
             << "    const uint workGroupID = ((dimCoord == 2) ? gl_WorkGroupID.z : ((dimCoord == 1) ? "
                "gl_WorkGroupID.y : gl_WorkGroupID.x));\n"
             << "    td.blockNumber         = uint(gl_DrawID);\n"
             << "    td.blockRow            = workGroupID;\n"
             << "    EmitMeshTasksEXT(1u, 1u, 1u);"
             << "}\n";
    }

    // Mesh shader.
    std::ostringstream mesh;
    mesh << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "// 32 local invocations in total.\n"
         << "layout (local_size_x=4, local_size_y=2, local_size_z=4) in;\n"
         << "layout (triangles) out;\n"
         << "layout (max_vertices=96, max_primitives=32) out;\n"
         << "\n"
         << "layout (location = 0) uniform uint dimCoord;\n"
         << "layout (location = 1) uniform uint width;\n"
         << "layout (location = 2) uniform uint height;\n"
         << "\n"
         << "layout (location=0) perprimitiveEXT out vec4 primitiveColor[];\n"
         << "\n"
         << (m_params.useTask ? taskDataDecl : "") << "\n"
         << "layout (binding=0, std430) readonly buffer BlockSizes {\n"
         << "    uint blockSize[];\n"
         << "} bsz;\n"
         << "\n"
         << "uint startOfBlock (uint blockNumber)\n"
         << "{\n"
         << "    uint start = 0;\n"
         << "    for (uint i = 0; i < blockNumber; i++)\n"
         << "        start += bsz.blockSize[i];\n"
         << "    return start;\n"
         << "}\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    const uint workGroupID = ((dimCoord == 2) ? gl_WorkGroupID.z : ((dimCoord == 1) ? "
            "gl_WorkGroupID.y : gl_WorkGroupID.x));\n"
         << "    const uint blockNumber = " << (m_params.useTask ? "td.blockNumber" : "uint(gl_DrawID)") << ";\n"
         << "    const uint blockRow = " << (m_params.useTask ? "td.blockRow" : "workGroupID") << ";\n"
         << "\n"
         << "    // Each workgroup will fill one row, and each invocation will generate a\n"
         << "    // triangle around the pixel center in each column.\n"
         << "    const uint row = startOfBlock(blockNumber) + blockRow;\n"
         << "    const uint col = gl_LocalInvocationIndex;\n"
         << "\n"
         << "    const float fHeight = float(height);\n"
         << "    const float fWidth = float(width);\n"
         << "\n"
         << "    // Pixel coordinates, normalized.\n"
         << "    const float rowNorm = (float(row) + 0.5) / fHeight;\n"
         << "    const float colNorm = (float(col) + 0.5) / fWidth;\n"
         << "\n"
         << "    // Framebuffer coordinates.\n"
         << "    const float coordX = (colNorm * 2.0) - 1.0;\n"
         << "    const float coordY = (rowNorm * 2.0) - 1.0;\n"
         << "\n"
         << "    const float pixelWidth = 2.0 / fWidth;\n"
         << "    const float pixelHeight = 2.0 / fHeight;\n"
         << "\n"
         << "    const float offsetX = pixelWidth / 2.0;\n"
         << "    const float offsetY = pixelHeight / 2.0;\n"
         << "\n"
         << "    const uint baseIndex = col*3;\n"
         << "    const uvec3 indices = uvec3(baseIndex, baseIndex + 1, baseIndex + 2);\n"
         << "\n"
         << "    SetMeshOutputsEXT(96u, 32u);\n"
         << "    primitiveColor[col] = vec4(rowNorm, colNorm, 0.0, 1.0);\n"
         << "    gl_PrimitiveTriangleIndicesEXT[col] = uvec3(indices.x, indices.y, indices.z);\n"
         << "\n"
         << "    gl_MeshVerticesEXT[indices.x].gl_Position = vec4(coordX - offsetX, coordY + offsetY, 0.0, 1.0);\n"
         << "    gl_MeshVerticesEXT[indices.y].gl_Position = vec4(coordX + offsetX, coordY + offsetY, 0.0, 1.0);\n"
         << "    gl_MeshVerticesEXT[indices.z].gl_Position = vec4(coordX, coordY - offsetY, 0.0, 1.0);\n"
         << "}\n";

    // Frag shader.
    std::ostringstream frag;
    frag << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout (location=0) perprimitiveEXT in vec4 primitiveColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    outColor = primitiveColor;\n"
         << "}\n";

    m_params.program =
        createProgram(m_params.useTask ? task.str().c_str() : nullptr, mesh.str().c_str(), frag.str().c_str());
    return m_params.program != 0 ? true : false;
}

tcu::TestNode::IterateResult MeshApiCase::iterate()
{
    const Functions &gl     = m_context.getRenderContext().getFunctions();
    const ExtFunctions &ext = ExtFunctions(m_context.getRenderContext());

    if (!initProgram())
    {
        m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return tcu::TestNode::STOP;
    }

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const float colorThres = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(colorThres, colorThres, 0.0f, 0.0f);

    // Prepare buffer containing the array of block sizes.
    de::Random rnd(m_params.seed);
    std::vector<uint32_t> blockSizes;

    const uint32_t vectorSize = std::max(1u, m_params.drawCount);
    const uint32_t largeDrawCount =
        vectorSize + 1u; // The indirect buffer needs to have some padding at the end. See below.
    const uint32_t evenBlockSize = height / vectorSize;
    uint32_t remainingRows       = height;

    blockSizes.reserve(vectorSize);
    for (uint32_t i = 0; i < vectorSize - 1u; ++i)
    {
        const auto blockSize = static_cast<uint32_t>(rnd.getInt(1, evenBlockSize));
        remainingRows -= blockSize;
        blockSizes.push_back(blockSize);
    }
    blockSizes.push_back(remainingRows);

    GLuint blockSizesBuffer;
    gl.genBuffers(1, &blockSizesBuffer);
    gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, blockSizesBuffer);
    gl.bufferData(GL_SHADER_STORAGE_BUFFER, de::dataSize(blockSizes), blockSizes.data(), GL_STATIC_DRAW);
    gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    gl.bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, blockSizesBuffer);

    // Pipeline layout.
    const auto dimCoord = rnd.getUint32() % 3u;
    gl.useProgram(m_params.program);
    gl.programUniform1ui(m_params.program, 0, dimCoord);
    gl.programUniform1ui(m_params.program, 1, width);
    gl.programUniform1ui(m_params.program, 2, height);

    // Indirect and count buffers if needed.
    GLuint indrectBuffer;
    GLuint countBuffer;

    if (m_params.drawType != DrawType::DRAW)
    {
        // Indirect draws.
        DE_ASSERT(static_cast<bool>(m_params.indirectArgs));
        const auto &indirectArgs = m_params.indirectArgs.get();
        const auto padding       = static_cast<uint32_t>(sizeof(DrawMeshTasksIndirectCommand));

        // Check stride and offset validity.
        DE_ASSERT(indirectArgs.offset % 4u == 0u);
        DE_ASSERT(indirectArgs.stride % 4u == 0u && (indirectArgs.stride == 0u || indirectArgs.stride >= padding));

        // Prepare struct vector, which will be converted to a buffer with the proper stride and offset later.
        std::vector<DrawMeshTasksIndirectCommand> commands;
        commands.reserve(blockSizes.size());

        std::transform(begin(blockSizes), end(blockSizes), std::back_inserter(commands),
                       [dimCoord](uint32_t blockSize) { return getIndirectCommand(blockSize, dimCoord); });

        const auto indirectBufferSize = indirectArgs.offset + indirectArgs.stride * commands.size() + padding;

        gl.genBuffers(1, &indrectBuffer);
        gl.bindBuffer(GL_DRAW_INDIRECT_BUFFER, indrectBuffer);
        gl.bufferStorage(GL_DRAW_INDIRECT_BUFFER, indirectBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
        for (uint32_t i = 0; i < commands.size(); i++)
        {
            gl.bufferSubData(GL_DRAW_INDIRECT_BUFFER, indirectArgs.offset + (indirectArgs.stride * i), padding,
                             &commands[i]);
        }
        gl.bindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        // Prepare count buffer if needed.
        if (m_params.drawType == DrawType::DRAW_INDIRECT_COUNT)
        {
            DE_ASSERT(static_cast<bool>(m_params.indirectCountLimit));
            DE_ASSERT(static_cast<bool>(m_params.indirectCountOffset));

            const auto countBufferValue =
                ((m_params.indirectCountLimit.get() == IndirectCountLimitType::BUFFER_VALUE) ? m_params.drawCount :
                                                                                               largeDrawCount);

            const std::vector<uint32_t> singleCount(1u, countBufferValue);

            const auto &indirectCountOffset = m_params.indirectCountOffset.get();
            const auto countBufferSize      = indirectCountOffset + de::dataSize(singleCount);

            gl.genBuffers(1, &countBuffer);
            gl.bindBuffer(GL_PARAMETER_BUFFER, countBuffer);
            gl.bufferStorage(GL_PARAMETER_BUFFER, countBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
            gl.bufferSubData(GL_PARAMETER_BUFFER, indirectCountOffset, de::dataSize(singleCount), singleCount.data());
            gl.bindBuffer(GL_PARAMETER_BUFFER, 0);
        }
    }

    // Graphics pipeline.
    gl.viewport(0, 0, width, height);
    gl.scissor(0, 0, width, height);
    gl.enable(GL_SCISSOR_TEST);

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    bool is_ok = true;
    // Draw triangle.
    if (m_params.drawType == DrawType::DRAW)
    {
        const auto drawArgs = getIndirectCommand(m_params.drawCount, dimCoord);

        ext.DrawMeshTasksEXT(drawArgs.x, drawArgs.y, drawArgs.z);
    }
    else if (m_params.drawType == DrawType::DRAW_INDIRECT)
    {
        const auto &indirectArgs = m_params.indirectArgs.get();

        gl.bindBuffer(GL_DRAW_INDIRECT_BUFFER, indrectBuffer);
        ext.MultiDrawMeshTasksIndirectEXT(indirectArgs.offset, m_params.drawCount, indirectArgs.stride);

        if (m_params.drawCount == 0)
        {
            is_ok &= ExpectError(GL_INVALID_VALUE, "MultiDrawMeshTasksIndirectEXT", "draw count is not positive");
            if (is_ok)
            {
                m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
            }
            else
            {
                m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Failed");
            }

            return tcu::TestNode::STOP;
        }
    }
    else if (m_params.drawType == DrawType::DRAW_INDIRECT_COUNT)
    {
        const auto &indirectArgs        = m_params.indirectArgs.get();
        const auto &indirectCountOffset = m_params.indirectCountOffset.get();
        const auto &indirectCountLimit  = m_params.indirectCountLimit.get();
        const auto maxCount =
            ((indirectCountLimit == IndirectCountLimitType::MAX_COUNT) ? m_params.drawCount : largeDrawCount);

        gl.bindBuffer(GL_DRAW_INDIRECT_BUFFER, indrectBuffer);
        gl.bindBuffer(GL_PARAMETER_BUFFER, countBuffer);
        ext.MultiDrawMeshTasksIndirectCountEXT(indirectArgs.offset, indirectCountOffset, maxCount, indirectArgs.stride);

        if (m_params.drawCount == 0)
        {
            is_ok &= ExpectError(GL_INVALID_VALUE, "MultiDrawMeshTasksIndirectEXT", "draw count is not positive");
            if (is_ok)
            {
                m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
            }
            else
            {
                m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Failed");
            }

            return tcu::TestNode::STOP;
        }
    }
    else
        DE_ASSERT(false);

    // Output buffer.
    GLubyte pixels[width * height * 4];
    gl.readPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    auto fmt = tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
    tcu::ConstPixelBufferAccess outPixels(fmt, width, height, 1, pixels);

    // Generate reference image and compare.
    {
        auto &log = m_context.getTestContext().getLog();
        tcu::TextureLevel referenceLevel(fmt, width, height);
        const auto reference = referenceLevel.getAccess();
        const auto setName   = de::toString(m_params.drawType) + "_draw_count_" + de::toString(m_params.drawCount) +
                             (m_params.useTask ? "_with_task" : "_no_task");
        const auto fHeight = static_cast<float>(height);
        const auto fWidth  = static_cast<float>(width);

        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const tcu::Vec4 refColor =
                    ((m_params.drawCount == 0u || (m_params.drawType == DrawType::DRAW && y >= m_params.drawCount)) ?
                         clearColor :
                         tcu::Vec4(
                             // These match the per-primitive color set by the mesh shader.
                             (static_cast<float>(y) + 0.5f) / fHeight, (static_cast<float>(x) + 0.5f) / fWidth, 0.0f,
                             1.0f));
                reference.setPixel(refColor, x, y);
            }
        }

        if (!tcu::floatThresholdCompare(log, setName.c_str(), "", reference, outPixels, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
        {
            m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL,
                                                     "Image comparison failed; check log for details");
            return tcu::TestNode::STOP;
        }
    }

    m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");

    return tcu::TestNode::STOP;
}

void MeshApiCase::init()
{
    // Extension check
    TCU_CHECK_AND_THROW(NotSupportedError, m_context.getContextInfo().isExtensionSupported("GL_EXT_mesh_shader"),
                        "GL_EXT_mesh_shader is not supported");
}

MeshShaderApiTestsGroup::MeshShaderApiTestsGroup(deqp::Context &context)
    : TestCaseGroup(context, "apiTests", "Mesh shader api tests")
{
}

void MeshShaderApiTestsGroup::init()
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    const DrawType drawCases[] = {
        DrawType::DRAW,
        DrawType::DRAW_INDIRECT,
        DrawType::DRAW_INDIRECT_COUNT,
    };

    const uint32_t drawCountCases[] = {0u, 1u, 2u, height / 2u, height};

    const uint32_t normalStride = static_cast<uint32_t>(sizeof(DrawMeshTasksIndirectCommand));
    const uint32_t largeStride  = 2u * normalStride + 4u;
    const uint32_t altOffset    = 20u;

    const struct
    {
        tcu::Maybe<IndirectArgs> indirectArgs;
        const char *name;
    } indirectArgsCases[] = {
        {tcu::nothing<IndirectArgs>(), "no_indirect_args"},

        // Offset 0, varying strides.
        {tcu::just(IndirectArgs{0u, 0u}), "offset_0_stride_0"},
        {tcu::just(IndirectArgs{0u, normalStride}), "offset_0_stride_normal"},
        {tcu::just(IndirectArgs{0u, largeStride}), "offset_0_stride_large"},

        // Nonzero offset, varying strides.
        {tcu::just(IndirectArgs{altOffset, 0u}), "offset_alt_stride_0"},
        {tcu::just(IndirectArgs{altOffset, normalStride}), "offset_alt_stride_normal"},
        {tcu::just(IndirectArgs{altOffset, largeStride}), "offset_alt_stride_large"},
    };

    const struct
    {
        tcu::Maybe<IndirectCountLimitType> limitType;
        const char *name;
    } countLimitCases[] = {
        {tcu::nothing<IndirectCountLimitType>(), "no_count_limit"},
        {tcu::just(IndirectCountLimitType::BUFFER_VALUE), "count_limit_buffer"},
        {tcu::just(IndirectCountLimitType::MAX_COUNT), "count_limit_max_count"},
    };

    const struct
    {
        tcu::Maybe<uint32_t> countOffset;
        const char *name;
    } countOffsetCases[] = {
        {tcu::nothing<uint32_t>(), "no_count_offset"},
        {tcu::just(uint32_t{0u}), "count_offset_0"},
        {tcu::just(altOffset), "count_offset_alt"},
    };

    const struct
    {
        bool useTask;
        const char *name;
    } taskCases[] = {
        {false, "no_task_shader"},
        {true, "with_task_shader"},
    };

    uint32_t seed = 1628678795u;

    for (const auto &drawCase : drawCases)
    {
        const auto drawCaseName      = de::toString(drawCase);
        const bool isIndirect        = (drawCase != DrawType::DRAW);
        const bool isIndirectNoCount = (drawCase == DrawType::DRAW_INDIRECT);
        const bool isIndirectCount   = (drawCase == DrawType::DRAW_INDIRECT_COUNT);

        GroupPtr drawGroup(new tcu::TestCaseGroup(m_context.getTestContext(), drawCaseName.c_str()));

        for (const auto &drawCountCase : drawCountCases)
        {
            const auto drawCountName = "draw_count_" + de::toString(drawCountCase);
            GroupPtr drawCountGroup(new tcu::TestCaseGroup(m_context.getTestContext(), drawCountName.c_str()));

            for (const auto &indirectArgsCase : indirectArgsCases)
            {
                const bool hasIndirectArgs = static_cast<bool>(indirectArgsCase.indirectArgs);
                const bool strideZero      = (hasIndirectArgs && indirectArgsCase.indirectArgs.get().stride == 0u);

                if (isIndirect != hasIndirectArgs)
                    continue;

                if (((isIndirectNoCount && drawCountCase > 1u) || isIndirectCount) && strideZero)
                    continue;

                GroupPtr indirectArgsGroup(new tcu::TestCaseGroup(m_context.getTestContext(), indirectArgsCase.name));

                for (const auto &countLimitCase : countLimitCases)
                {
                    const bool hasCountLimit = static_cast<bool>(countLimitCase.limitType);

                    if (isIndirectCount != hasCountLimit)
                        continue;

                    GroupPtr countLimitGroup(new tcu::TestCaseGroup(m_context.getTestContext(), countLimitCase.name));

                    for (const auto &countOffsetCase : countOffsetCases)
                    {
                        const bool hasCountOffsetType = static_cast<bool>(countOffsetCase.countOffset);

                        if (isIndirectCount != hasCountOffsetType)
                            continue;

                        GroupPtr countOffsetGroup(
                            new tcu::TestCaseGroup(m_context.getTestContext(), countOffsetCase.name));

                        for (const auto &taskCase : taskCases)
                        {
                            const auto testName        = std::string(taskCase.name);
                            const ApiTestParams params = {
                                drawCase,                      // DrawType drawType;
                                seed++,                        // uint32_t seed;
                                drawCountCase,                 // uint32_t drawCount;
                                indirectArgsCase.indirectArgs, // tcu::Maybe<IndirectArgs> indirectArgs;
                                countLimitCase.limitType,      // tcu::Maybe<IndirectCountLimitType> indirectCountLimit;
                                countOffsetCase.countOffset,   // tcu::Maybe<uint32_t> indirectCountOffset;
                                taskCase.useTask,              // bool useTask;
                                0,                             // program;
                            };

                            countOffsetGroup->addChild(new MeshApiCase(m_context, testName.c_str(), params));
                        }

                        countLimitGroup->addChild(countOffsetGroup.release());
                    }

                    indirectArgsGroup->addChild(countLimitGroup.release());
                }

                drawCountGroup->addChild(indirectArgsGroup.release());
            }

            drawGroup->addChild(drawCountGroup.release());
        }

        addChild(drawGroup.release());
    }
}

} // namespace meshShader
} // namespace glc
