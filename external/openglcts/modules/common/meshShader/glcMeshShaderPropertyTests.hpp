#ifndef _GLCMESHSHADERPROPERTYTESTS_HPP
#define _GLCMESHSHADERPROPERTYTESTS_HPP

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
 * \brief MeshShader property tests
 */ /*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "glcTestCase.hpp"

namespace glc
{
namespace meshShader
{

struct MeshShaderProperties
{
    glw::GLint maxTaskPayloadSize;
    glw::GLint maxTaskSharedMemorySize;
    glw::GLint maxTaskPayloadAndSharedMemorySize;
    glw::GLint maxMeshSharedMemorySize;
    glw::GLint maxMeshPayloadAndSharedMemorySize;
    glw::GLint maxMeshOutputMemorySize;
    glw::GLint maxMeshPayloadAndOutputMemorySize;
    glw::GLint maxMeshOutputComponents;
    glw::GLint maxMeshOutputVertices;
    glw::GLint maxMeshOutputPrimitives;
    glw::GLint maxMeshOutputLayers;
    glw::GLint maxMeshMultiviewViewCount;
    glw::GLint meshOutputPerVertexGranularity;
    glw::GLint meshOutputPerPrimitiveGranularity;
};

enum class PayLoadShMemSizeType
{
    PAYLOAD = 0,
    SHARED_MEMORY,
    BOTH,
};

struct PayloadShMemSizeParams
{
    PayLoadShMemSizeType testType;
    glw::GLuint program;

    bool hasPayload(void) const
    {
        return testType != PayLoadShMemSizeType::SHARED_MEMORY;
    }
    bool hasSharedMemory(void) const
    {
        return testType != PayLoadShMemSizeType::PAYLOAD;
    }
};

using TaskPayloadShMemSizeParams = PayloadShMemSizeParams;
using MeshPayloadShMemSizeParams = PayloadShMemSizeParams;

enum class LocationType
{
    PER_VERTEX,
    PER_PRIMITIVE,
};

enum class ViewIndexType
{
    NO_VIEW_INDEX,
    VIEW_INDEX_FRAG,
    VIEW_INDEX_BOTH,
};

struct MaxMeshOutputParams
{
    bool usePayload;
    LocationType locationType;
    ViewIndexType viewIndexType;
    glw::GLuint program;

    bool isMultiView(void) const
    {
        return (viewIndexType != ViewIndexType::NO_VIEW_INDEX);
    }

    bool viewIndexInMesh(void) const
    {
        return (viewIndexType == ViewIndexType::VIEW_INDEX_BOTH);
    }
};

class MeshShaderPropertyTestsGroup : public deqp::TestCaseGroup
{
public:
    MeshShaderPropertyTestsGroup(deqp::Context &context);
    ~MeshShaderPropertyTestsGroup()
    {
    }

    virtual void init();
};

class MeshPropertyCase : public deqp::TestCase
{
public:
    MeshPropertyCase(deqp::Context &context, const char *name)
        : deqp::TestCase(context, name, "Mesh shader property tests")
    {
    }

    virtual ~MeshPropertyCase()
    {
    }

    virtual void init() override;

    virtual bool initProgram()                                        = 0;
    virtual void checkSupport(MeshShaderProperties &properties) const = 0;

    void getMSProperties(MeshShaderProperties &properties);
};

class PayloadShMemSizeCase : public MeshPropertyCase
{
public:
    PayloadShMemSizeCase(deqp::Context &context, const char *name, const TaskPayloadShMemSizeParams &params)
        : MeshPropertyCase(context, name)
        , m_params(params)
    {
    }
    virtual ~PayloadShMemSizeCase(void)
    {
    }

    virtual IterateResult iterate() override;

protected:
    TaskPayloadShMemSizeParams m_params;

    uint32_t m_payloadElements                  = 0;
    uint32_t m_sharedMemoryElements             = 0;
    static constexpr uint32_t kElementSize      = static_cast<uint32_t>(sizeof(uint32_t));
    static constexpr uint32_t kLocalInvocations = 128u;
};

class TaskPayloadShMemSizeCase : public PayloadShMemSizeCase
{
public:
    TaskPayloadShMemSizeCase(deqp::Context &context, const char *name, const TaskPayloadShMemSizeParams &params)
        : PayloadShMemSizeCase(context, name, params)
    {
    }
    virtual ~TaskPayloadShMemSizeCase(void)
    {
    }

    void init() override;
    bool initProgram() override;
    virtual void checkSupport(MeshShaderProperties &properties) const override;
};

class MeshPayloadShMemSizeCase : public PayloadShMemSizeCase
{
public:
    MeshPayloadShMemSizeCase(deqp::Context &context, const char *name, const TaskPayloadShMemSizeParams &params)
        : PayloadShMemSizeCase(context, name, params)
    {
    }
    virtual ~MeshPayloadShMemSizeCase(void)
    {
    }

    void init() override;
    bool initProgram() override;
    virtual void checkSupport(MeshShaderProperties &properties) const override;
};

class MaxMeshOutputSizeCase : public MeshPropertyCase
{
public:
    MaxMeshOutputSizeCase(deqp::Context &context, const char *name, const MaxMeshOutputParams &params)
        : MeshPropertyCase(context, name)
        , m_params(params)
    {
    }
    virtual ~MaxMeshOutputSizeCase(void)
    {
    }

    IterateResult iterate() override;

    void init() override;
    bool initProgram() override;
    void checkSupport(MeshShaderProperties &properties) const override;

    // Small-ish numbers allow for more fine-grained control in the amount of memory, but it can't be too small or we hit the locations limit.
    static constexpr uint32_t kMaxPoints = 96u;
    static constexpr uint32_t kNumViews  = 2u; // For the multiView case.

protected:
    static constexpr uint32_t kUvec4Size          = 16u; // We'll use 4 scalars at a time in the form of a uvec4.
    static constexpr uint32_t kUvec4Comp          = 4u;  // 4 components per uvec4.
    static constexpr uint32_t kPayloadElementSize = 4u;  // Each payload element will be a uint.

    uint32_t m_payloadElements = 0;
    uint32_t m_locationCount   = 0;
    uint32_t m_numViews        = 1;

    MaxMeshOutputParams m_params;
};

} // namespace meshShader
} // namespace glc

#endif // _GLCMESHSHADERPROPERTYTESTS_HPP
