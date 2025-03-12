/*-------------------------------------------------------------------------
 * OpenGL Driver Error Test Suite
 * -----------------------------
 *
 * Copyright (c) 2024 Google Inc.
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
* \file  es3cDriverErrorTests.hpp
* \brief Tests for known driver errors in GLSL ES 3.0.
*/ /*-------------------------------------------------------------------*/

#include "es3cDriverErrorTests.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <string.h>

namespace es3cts
{

namespace
{
class UpdateBufferAfterAnotherBufferUnmapCase : public deqp::TestCase
{
public:
    UpdateBufferAfterAnotherBufferUnmapCase(deqp::Context &context);
    IterateResult iterate();
};

UpdateBufferAfterAnotherBufferUnmapCase::UpdateBufferAfterAnotherBufferUnmapCase(deqp::Context &context)
    : TestCase(context, "update_buffer_after_another_buffer_unmap", "Update a buffer after another buffer unmap")
{
}

deqp::TestCase::IterateResult UpdateBufferAfterAnotherBufferUnmapCase::iterate(void)
{
    const auto &renderContext = m_context.getRenderContext();
    const auto &gl            = renderContext.getFunctions();

    glw::GLuint arrayBuffer;
    gl.genBuffers(1, &arrayBuffer);
    gl.bindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
    gl.bufferData(GL_ARRAY_BUFFER, 20, nullptr, GL_DYNAMIC_DRAW);
    gl.mapBufferRange(GL_ARRAY_BUFFER, 0, 20,
                      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
                          GL_MAP_UNSYNCHRONIZED_BIT);

    glw::GLuint elementArrayBuffer;
    gl.genBuffers(1, &elementArrayBuffer);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementArrayBuffer);
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, 20, nullptr, GL_DYNAMIC_DRAW);
    void *pe = gl.mapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, 20,
                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
                                     GL_MAP_UNSYNCHRONIZED_BIT);

    gl.unmapBuffer(GL_ARRAY_BUFFER);

    gl.flushMappedBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, 20);
    glw::GLubyte data[20] = {0};
    memcpy(pe, data, 20);
    gl.unmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

} // namespace

DriverErrorTests::DriverErrorTests(deqp::Context &context)
    : deqp::TestCaseGroup(context, "driver_error", "GLES3 known driver error tests")
{
}

DriverErrorTests::~DriverErrorTests(void)
{
}

void DriverErrorTests::init(void)
{
    addChild(new UpdateBufferAfterAnotherBufferUnmapCase(m_context));
}

} // namespace es3cts
