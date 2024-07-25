/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 *//*!
 * \file
 * \brief Buffer test utilities.
 *//*--------------------------------------------------------------------*/

#include "es2fBufferTestUtil.hpp"
#include "tcuRandomValueIterator.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVector.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "gluPixelTransfer.hpp"
#include "gluRenderContext.hpp"
#include "gluStrUtil.hpp"
#include "gluShaderProgram.hpp"
#include "deMemory.h"
#include "deStringUtil.hpp"

#include <algorithm>

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{
namespace BufferTestUtil
{

enum
{
    VERIFY_QUAD_SIZE                 = 8,   //!< Quad size in VertexArrayVerifier
    MAX_LINES_PER_INDEX_ARRAY_DRAW   = 128, //!< Maximum number of lines per one draw in IndexArrayVerifier
    INDEX_ARRAY_DRAW_VIEWPORT_WIDTH  = 128,
    INDEX_ARRAY_DRAW_VIEWPORT_HEIGHT = 128
};

static const bool LOG_VERIFIER_CALLS = false; //! \note Especially array verifier generates a lot of calls.

using std::set;
using std::string;
using std::vector;
using tcu::TestLog;

// Helper functions.

void fillWithRandomBytes(uint8_t *ptr, int numBytes, uint32_t seed)
{
    std::copy(tcu::RandomValueIterator<uint8_t>::begin(seed, numBytes), tcu::RandomValueIterator<uint8_t>::end(), ptr);
}

bool compareByteArrays(tcu::TestLog &log, const uint8_t *resPtr, const uint8_t *refPtr, int numBytes)
{
    bool isOk              = true;
    const int maxSpanLen   = 8;
    const int maxDiffSpans = 4;
    int numDiffSpans       = 0;
    int diffSpanStart      = -1;
    int ndx                = 0;

    log << TestLog::Section("Verify", "Verification result");

    for (; ndx < numBytes; ndx++)
    {
        if (resPtr[ndx] != refPtr[ndx])
        {
            if (diffSpanStart < 0)
                diffSpanStart = ndx;

            isOk = false;
        }
        else if (diffSpanStart >= 0)
        {
            if (numDiffSpans < maxDiffSpans)
            {
                int len      = ndx - diffSpanStart;
                int printLen = de::min(len, maxSpanLen);

                log << TestLog::Message << len << " byte difference at offset " << diffSpanStart << "\n"
                    << "  expected "
                    << tcu::formatArray(tcu::Format::HexIterator<uint8_t>(refPtr + diffSpanStart),
                                        tcu::Format::HexIterator<uint8_t>(refPtr + diffSpanStart + printLen))
                    << "\n"
                    << "  got "
                    << tcu::formatArray(tcu::Format::HexIterator<uint8_t>(resPtr + diffSpanStart),
                                        tcu::Format::HexIterator<uint8_t>(resPtr + diffSpanStart + printLen))
                    << TestLog::EndMessage;
            }
            else
                log << TestLog::Message << "(output too long, truncated)" << TestLog::EndMessage;

            numDiffSpans += 1;
            diffSpanStart = -1;
        }
    }

    if (diffSpanStart >= 0)
    {
        if (numDiffSpans < maxDiffSpans)
        {
            int len      = ndx - diffSpanStart;
            int printLen = de::min(len, maxSpanLen);

            log << TestLog::Message << len << " byte difference at offset " << diffSpanStart << "\n"
                << "  expected "
                << tcu::formatArray(tcu::Format::HexIterator<uint8_t>(refPtr + diffSpanStart),
                                    tcu::Format::HexIterator<uint8_t>(refPtr + diffSpanStart + printLen))
                << "\n"
                << "  got "
                << tcu::formatArray(tcu::Format::HexIterator<uint8_t>(resPtr + diffSpanStart),
                                    tcu::Format::HexIterator<uint8_t>(resPtr + diffSpanStart + printLen))
                << TestLog::EndMessage;
        }
        else
            log << TestLog::Message << "(output too long, truncated)" << TestLog::EndMessage;
    }

    log << TestLog::Message << (isOk ? "Verification passed." : "Verification FAILED!") << TestLog::EndMessage;
    log << TestLog::EndSection;

    return isOk;
}

const char *getBufferTargetName(uint32_t target)
{
    switch (target)
    {
    case GL_ARRAY_BUFFER:
        return "array";
    case GL_ELEMENT_ARRAY_BUFFER:
        return "element_array";
    default:
        DE_ASSERT(false);
        return DE_NULL;
    }
}

const char *getUsageHintName(uint32_t hint)
{
    switch (hint)
    {
    case GL_STREAM_DRAW:
        return "stream_draw";
    case GL_STATIC_DRAW:
        return "static_draw";
    case GL_DYNAMIC_DRAW:
        return "dynamic_draw";
    default:
        DE_ASSERT(false);
        return DE_NULL;
    }
}

// BufferCase

BufferCase::BufferCase(Context &context, const char *name, const char *description)
    : TestCase(context, name, description)
    , CallLogWrapper(context.getRenderContext().getFunctions(), m_context.getTestContext().getLog())
{
}

BufferCase::~BufferCase(void)
{
    enableLogging(false);
    BufferCase::deinit();
}

void BufferCase::init(void)
{
    enableLogging(true);
}

void BufferCase::deinit(void)
{
    for (set<uint32_t>::const_iterator bufIter = m_allocatedBuffers.begin(); bufIter != m_allocatedBuffers.end();
         bufIter++)
        glDeleteBuffers(1, &(*bufIter));
}

uint32_t BufferCase::genBuffer(void)
{
    uint32_t buf = 0;
    glGenBuffers(1, &buf);
    if (buf != 0)
    {
        try
        {
            m_allocatedBuffers.insert(buf);
        }
        catch (const std::exception &)
        {
            glDeleteBuffers(1, &buf);
            throw;
        }
    }
    return buf;
}

void BufferCase::deleteBuffer(uint32_t buffer)
{
    glDeleteBuffers(1, &buffer);
    m_allocatedBuffers.erase(buffer);
}

void BufferCase::checkError(void)
{
    glw::GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        throw tcu::TestError(string("Got ") + glu::getErrorStr(err).toString());
}

// ReferenceBuffer

void ReferenceBuffer::setSize(int numBytes)
{
    m_data.resize(numBytes);
}

void ReferenceBuffer::setData(int numBytes, const uint8_t *bytes)
{
    m_data.resize(numBytes);
    std::copy(bytes, bytes + numBytes, m_data.begin());
}

void ReferenceBuffer::setSubData(int offset, int numBytes, const uint8_t *bytes)
{
    DE_ASSERT(de::inBounds(offset, 0, (int)m_data.size()) &&
              de::inRange(offset + numBytes, offset, (int)m_data.size()));
    std::copy(bytes, bytes + numBytes, m_data.begin() + offset);
}

// BufferVerifierBase

BufferVerifierBase::BufferVerifierBase(Context &context)
    : CallLogWrapper(context.getRenderContext().getFunctions(), context.getTestContext().getLog())
    , m_context(context)
{
    enableLogging(LOG_VERIFIER_CALLS);
}

// BufferVerifier

BufferVerifier::BufferVerifier(Context &context, VerifyType verifyType) : m_verifier(DE_NULL)
{
    switch (verifyType)
    {
    case VERIFY_AS_VERTEX_ARRAY:
        m_verifier = new VertexArrayVerifier(context);
        break;
    case VERIFY_AS_INDEX_ARRAY:
        m_verifier = new IndexArrayVerifier(context);
        break;
    default:
        TCU_FAIL("Unsupported verifier");
    }
}

BufferVerifier::~BufferVerifier(void)
{
    delete m_verifier;
}

bool BufferVerifier::verify(uint32_t buffer, const uint8_t *reference, int offset, int numBytes)
{
    DE_ASSERT(numBytes >= getMinSize());
    DE_ASSERT(offset % getAlignment() == 0);
    DE_ASSERT((offset + numBytes) % getAlignment() == 0);
    return m_verifier->verify(buffer, reference, offset, numBytes);
}

// VertexArrayVerifier

VertexArrayVerifier::VertexArrayVerifier(Context &context)
    : BufferVerifierBase(context)
    , m_program(DE_NULL)
    , m_posLoc(0)
    , m_byteVecLoc(0)
{
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources("attribute highp vec2 a_position;\n"
                                                               "attribute mediump vec3 a_byteVec;\n"
                                                               "varying mediump vec3 v_byteVec;\n"
                                                               "void main (void)\n"
                                                               "{\n"
                                                               "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                                               "    v_byteVec = a_byteVec;\n"
                                                               "}\n",

                                                               "varying mediump vec3 v_byteVec;\n"
                                                               "void main (void)\n"
                                                               "{\n"
                                                               "    gl_FragColor = vec4(v_byteVec, 1.0);\n"
                                                               "}\n"));

    if (!m_program->isOk())
    {
        m_context.getTestContext().getLog() << *m_program;
        delete m_program;
        TCU_FAIL("Compile failed");
    }

    const glw::Functions &funcs = context.getRenderContext().getFunctions();
    m_posLoc                    = funcs.getAttribLocation(m_program->getProgram(), "a_position");
    m_byteVecLoc                = funcs.getAttribLocation(m_program->getProgram(), "a_byteVec");
}

VertexArrayVerifier::~VertexArrayVerifier(void)
{
    delete m_program;
}

static void computePositions(vector<tcu::Vec2> &positions, int gridSizeX, int gridSizeY)
{
    positions.resize(gridSizeX * gridSizeY * 4);

    for (int y = 0; y < gridSizeY; y++)
        for (int x = 0; x < gridSizeX; x++)
        {
            float sx0   = (float)(x + 0) / (float)gridSizeX;
            float sy0   = (float)(y + 0) / (float)gridSizeY;
            float sx1   = (float)(x + 1) / (float)gridSizeX;
            float sy1   = (float)(y + 1) / (float)gridSizeY;
            float fx0   = 2.0f * sx0 - 1.0f;
            float fy0   = 2.0f * sy0 - 1.0f;
            float fx1   = 2.0f * sx1 - 1.0f;
            float fy1   = 2.0f * sy1 - 1.0f;
            int baseNdx = (y * gridSizeX + x) * 4;

            positions[baseNdx + 0] = tcu::Vec2(fx0, fy0);
            positions[baseNdx + 1] = tcu::Vec2(fx0, fy1);
            positions[baseNdx + 2] = tcu::Vec2(fx1, fy0);
            positions[baseNdx + 3] = tcu::Vec2(fx1, fy1);
        }
}

static void computeIndices(vector<uint16_t> &indices, int gridSizeX, int gridSizeY)
{
    indices.resize(3 * 2 * gridSizeX * gridSizeY);

    for (int quadNdx = 0; quadNdx < gridSizeX * gridSizeY; quadNdx++)
    {
        int v00 = quadNdx * 4 + 0;
        int v01 = quadNdx * 4 + 1;
        int v10 = quadNdx * 4 + 2;
        int v11 = quadNdx * 4 + 3;

        DE_ASSERT(v11 < (1 << 16));

        indices[quadNdx * 6 + 0] = (uint16_t)v10;
        indices[quadNdx * 6 + 1] = (uint16_t)v00;
        indices[quadNdx * 6 + 2] = (uint16_t)v01;

        indices[quadNdx * 6 + 3] = (uint16_t)v10;
        indices[quadNdx * 6 + 4] = (uint16_t)v01;
        indices[quadNdx * 6 + 5] = (uint16_t)v11;
    }
}

static inline tcu::Vec4 fetchVtxColor(const uint8_t *ptr, int vtxNdx)
{
    return tcu::RGBA(*(ptr + vtxNdx * 3 + 0), *(ptr + vtxNdx * 3 + 1), *(ptr + vtxNdx * 3 + 2), 255).toVec();
}

static void renderQuadGridReference(tcu::Surface &dst, int numQuads, int rowLength, const uint8_t *inPtr)
{
    using tcu::Vec4;

    dst.setSize(rowLength * VERIFY_QUAD_SIZE,
                (numQuads / rowLength + (numQuads % rowLength != 0 ? 1 : 0)) * VERIFY_QUAD_SIZE);

    tcu::PixelBufferAccess dstAccess = dst.getAccess();
    tcu::clear(dstAccess, tcu::IVec4(0, 0, 0, 0xff));

    for (int quadNdx = 0; quadNdx < numQuads; quadNdx++)
    {
        int x0   = (quadNdx % rowLength) * VERIFY_QUAD_SIZE;
        int y0   = (quadNdx / rowLength) * VERIFY_QUAD_SIZE;
        Vec4 v00 = fetchVtxColor(inPtr, quadNdx * 4 + 0);
        Vec4 v10 = fetchVtxColor(inPtr, quadNdx * 4 + 1);
        Vec4 v01 = fetchVtxColor(inPtr, quadNdx * 4 + 2);
        Vec4 v11 = fetchVtxColor(inPtr, quadNdx * 4 + 3);

        for (int y = 0; y < VERIFY_QUAD_SIZE; y++)
            for (int x = 0; x < VERIFY_QUAD_SIZE; x++)
            {
                float fx = ((float)x + 0.5f) / (float)VERIFY_QUAD_SIZE;
                float fy = ((float)y + 0.5f) / (float)VERIFY_QUAD_SIZE;

                bool tri       = fx + fy <= 1.0f;
                float tx       = tri ? fx : (1.0f - fx);
                float ty       = tri ? fy : (1.0f - fy);
                const Vec4 &t0 = tri ? v00 : v11;
                const Vec4 &t1 = tri ? v01 : v10;
                const Vec4 &t2 = tri ? v10 : v01;
                Vec4 color     = t0 + (t1 - t0) * tx + (t2 - t0) * ty;

                dstAccess.setPixel(color, x0 + x, y0 + y);
            }
    }
}

bool VertexArrayVerifier::verify(uint32_t buffer, const uint8_t *refPtr, int offset, int numBytes)
{
    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    const int numBytesInVtx               = 3;
    const int numBytesInQuad              = numBytesInVtx * 4;
    int maxQuadsX                         = de::min(128, renderTarget.getWidth() / VERIFY_QUAD_SIZE);
    int maxQuadsY                         = de::min(128, renderTarget.getHeight() / VERIFY_QUAD_SIZE);
    int maxQuadsPerBatch                  = maxQuadsX * maxQuadsY;
    int numVerified                       = 0;
    uint32_t program                      = m_program->getProgram();
    tcu::RGBA threshold                   = renderTarget.getPixelFormat().getColorThreshold() + tcu::RGBA(4, 4, 4, 4);
    bool isOk                             = true;

    vector<tcu::Vec2> positions;
    vector<uint16_t> indices;

    tcu::Surface rendered;
    tcu::Surface reference;

    DE_ASSERT(numBytes >= numBytesInQuad); // Can't render full quad with smaller buffers.

    computePositions(positions, maxQuadsX, maxQuadsY);
    computeIndices(indices, maxQuadsX, maxQuadsY);

    // Reset buffer bindings.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Setup rendering state.
    glViewport(0, 0, maxQuadsX * VERIFY_QUAD_SIZE, maxQuadsY * VERIFY_QUAD_SIZE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glUseProgram(program);
    glEnableVertexAttribArray(m_posLoc);
    glVertexAttribPointer(m_posLoc, 2, GL_FLOAT, GL_FALSE, 0, &positions[0]);
    glEnableVertexAttribArray(m_byteVecLoc);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);

    while (numVerified < numBytes)
    {
        int numRemaining     = numBytes - numVerified;
        bool isLeftoverBatch = numRemaining < numBytesInQuad;
        int numBytesToVerify =
            isLeftoverBatch ? numBytesInQuad :
                              de::min(maxQuadsPerBatch * numBytesInQuad, numRemaining - numRemaining % numBytesInQuad);
        int curOffset       = isLeftoverBatch ? (numBytes - numBytesInQuad) : numVerified;
        int numQuads        = numBytesToVerify / numBytesInQuad;
        int numCols         = de::min(maxQuadsX, numQuads);
        int numRows         = numQuads / maxQuadsX + (numQuads % maxQuadsX != 0 ? 1 : 0);
        string imageSetDesc = string("Bytes ") + de::toString(offset + curOffset) + " to " +
                              de::toString(offset + curOffset + numBytesToVerify - 1);

        DE_ASSERT(numBytesToVerify > 0 && numBytesToVerify % numBytesInQuad == 0);
        DE_ASSERT(de::inBounds(curOffset, 0, numBytes));
        DE_ASSERT(de::inRange(curOffset + numBytesToVerify, curOffset, numBytes));

        // Render batch.
        glClear(GL_COLOR_BUFFER_BIT);
        glVertexAttribPointer(m_byteVecLoc, 3, GL_UNSIGNED_BYTE, GL_TRUE, 0,
                              (const glw::GLvoid *)(uintptr_t)(offset + curOffset));
        glDrawElements(GL_TRIANGLES, numQuads * 6, GL_UNSIGNED_SHORT, &indices[0]);

        renderQuadGridReference(reference, numQuads, numCols, refPtr + offset + curOffset);

        rendered.setSize(numCols * VERIFY_QUAD_SIZE, numRows * VERIFY_QUAD_SIZE);
        glu::readPixels(m_context.getRenderContext(), 0, 0, rendered.getAccess());

        if (!tcu::pixelThresholdCompare(m_context.getTestContext().getLog(), "RenderResult", imageSetDesc.c_str(),
                                        reference, rendered, threshold, tcu::COMPARE_LOG_RESULT))
        {
            isOk = false;
            break;
        }

        numVerified += isLeftoverBatch ? numRemaining : numBytesToVerify;
    }

    glDisableVertexAttribArray(m_posLoc);
    glDisableVertexAttribArray(m_byteVecLoc);

    return isOk;
}

// IndexArrayVerifier

IndexArrayVerifier::IndexArrayVerifier(Context &context)
    : BufferVerifierBase(context)
    , m_program(DE_NULL)
    , m_posLoc(0)
    , m_colorLoc(0)
{
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources("attribute highp vec2 a_position;\n"
                                                               "attribute mediump vec3 a_color;\n"
                                                               "varying mediump vec3 v_color;\n"
                                                               "void main (void)\n"
                                                               "{\n"
                                                               "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                                               "    v_color = a_color;\n"
                                                               "}\n",

                                                               "varying mediump vec3 v_color;\n"
                                                               "void main (void)\n"
                                                               "{\n"
                                                               "    gl_FragColor = vec4(v_color, 1.0);\n"
                                                               "}\n"));

    if (!m_program->isOk())
    {
        m_context.getTestContext().getLog() << *m_program;
        delete m_program;
        TCU_FAIL("Compile failed");
    }

    const glw::Functions &funcs = context.getRenderContext().getFunctions();
    m_posLoc                    = funcs.getAttribLocation(m_program->getProgram(), "a_position");
    m_colorLoc                  = funcs.getAttribLocation(m_program->getProgram(), "a_color");
}

IndexArrayVerifier::~IndexArrayVerifier(void)
{
    delete m_program;
}

static void computeIndexVerifierPositions(std::vector<tcu::Vec2> &dst)
{
    const int numPosX = 16;
    const int numPosY = 16;

    dst.resize(numPosX * numPosY);

    for (int y = 0; y < numPosY; y++)
    {
        for (int x = 0; x < numPosX; x++)
        {
            float xf = float(x) / float(numPosX - 1);
            float yf = float(y) / float(numPosY - 1);

            dst[y * numPosX + x] = tcu::Vec2(2.0f * xf - 1.0f, 2.0f * yf - 1.0f);
        }
    }
}

static void computeIndexVerifierColors(std::vector<tcu::Vec3> &dst)
{
    const int numColors = 256;
    const float minVal  = 0.1f;
    const float maxVal  = 0.5f;
    de::Random rnd(0xabc231);

    dst.resize(numColors);

    for (std::vector<tcu::Vec3>::iterator i = dst.begin(); i != dst.end(); ++i)
    {
        i->x() = rnd.getFloat(minVal, maxVal);
        i->y() = rnd.getFloat(minVal, maxVal);
        i->z() = rnd.getFloat(minVal, maxVal);
    }
}

template <typename T>
static void execVertexFetch(T *dst, const T *src, const uint8_t *indices, int numIndices)
{
    for (int i = 0; i < numIndices; ++i)
        dst[i] = src[indices[i]];
}

bool IndexArrayVerifier::verify(uint32_t buffer, const uint8_t *refPtr, int offset, int numBytes)
{
    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();
    const int viewportW                   = de::min<int>(INDEX_ARRAY_DRAW_VIEWPORT_WIDTH, renderTarget.getWidth());
    const int viewportH                   = de::min<int>(INDEX_ARRAY_DRAW_VIEWPORT_HEIGHT, renderTarget.getHeight());
    const int minBytesPerBatch            = 2;
    const tcu::RGBA threshold(0, 0, 0, 0);

    std::vector<tcu::Vec2> positions;
    std::vector<tcu::Vec3> colors;

    std::vector<tcu::Vec2> fetchedPos(MAX_LINES_PER_INDEX_ARRAY_DRAW + 1);
    std::vector<tcu::Vec3> fetchedColor(MAX_LINES_PER_INDEX_ARRAY_DRAW + 1);

    tcu::Surface indexBufferImg(viewportW, viewportH);
    tcu::Surface referenceImg(viewportW, viewportH);

    int numVerified = 0;
    bool isOk       = true;

    DE_STATIC_ASSERT(sizeof(tcu::Vec2) == sizeof(float) * 2);
    DE_STATIC_ASSERT(sizeof(tcu::Vec3) == sizeof(float) * 3);

    computeIndexVerifierPositions(positions);
    computeIndexVerifierColors(colors);

    // Reset buffer bindings.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);

    // Setup rendering state.
    glViewport(0, 0, viewportW, viewportH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glUseProgram(m_program->getProgram());
    glEnableVertexAttribArray(m_posLoc);
    glEnableVertexAttribArray(m_colorLoc);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    while (numVerified < numBytes)
    {
        int numRemaining     = numBytes - numVerified;
        bool isLeftoverBatch = numRemaining < minBytesPerBatch;
        int numBytesToVerify =
            isLeftoverBatch ? minBytesPerBatch : de::min(MAX_LINES_PER_INDEX_ARRAY_DRAW + 1, numRemaining);
        int curOffset       = isLeftoverBatch ? (numBytes - minBytesPerBatch) : numVerified;
        string imageSetDesc = string("Bytes ") + de::toString(offset + curOffset) + " to " +
                              de::toString(offset + curOffset + numBytesToVerify - 1);

        // Step 1: Render using index buffer.
        glClear(GL_COLOR_BUFFER_BIT);
        glVertexAttribPointer(m_posLoc, 2, GL_FLOAT, GL_FALSE, 0, &positions[0]);
        glVertexAttribPointer(m_colorLoc, 3, GL_FLOAT, GL_FALSE, 0, &colors[0]);
        glDrawElements(GL_LINE_STRIP, numBytesToVerify, GL_UNSIGNED_BYTE, (void *)(uintptr_t)(offset + curOffset));
        glu::readPixels(m_context.getRenderContext(), 0, 0, indexBufferImg.getAccess());

        // Step 2: Do manual fetch and render without index buffer.
        execVertexFetch(&fetchedPos[0], &positions[0], refPtr + offset + curOffset, numBytesToVerify);
        execVertexFetch(&fetchedColor[0], &colors[0], refPtr + offset + curOffset, numBytesToVerify);

        glClear(GL_COLOR_BUFFER_BIT);
        glVertexAttribPointer(m_posLoc, 2, GL_FLOAT, GL_FALSE, 0, &fetchedPos[0]);
        glVertexAttribPointer(m_colorLoc, 3, GL_FLOAT, GL_FALSE, 0, &fetchedColor[0]);
        glDrawArrays(GL_LINE_STRIP, 0, numBytesToVerify);
        glu::readPixels(m_context.getRenderContext(), 0, 0, referenceImg.getAccess());

        if (!tcu::pixelThresholdCompare(m_context.getTestContext().getLog(), "RenderResult", imageSetDesc.c_str(),
                                        referenceImg, indexBufferImg, threshold, tcu::COMPARE_LOG_RESULT))
        {
            isOk = false;
            break;
        }

        numVerified += isLeftoverBatch ? numRemaining : numBytesToVerify;
    }

    return isOk;
}

} // namespace BufferTestUtil
} // namespace Functional
} // namespace gles2
} // namespace deqp
