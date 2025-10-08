#ifndef _GLCMESHSHADERSMOKETESTS_HPP
#define _GLCMESHSHADERSMOKETESTS_HPP

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

#include "tcuDefs.hpp"
#include "glcTestCase.hpp"
#include "tcuVector.hpp"

namespace glc
{
namespace meshShader
{

class MeshTriangleCase : public deqp::TestCase
{
public:
    MeshTriangleCase(deqp::Context &context, const char *name, const char *desc) : deqp::TestCase(context, name, desc)
    {
    }

    virtual ~MeshTriangleCase()
    {
    }

    virtual void init() override;

    virtual bool initProgram() = 0;

    virtual IterateResult iterate() override;

protected:
    struct MeshTriangleRendererParams
    {
        std::vector<tcu::Vec4> vertexCoords;
        std::vector<uint32_t> vertexIndices;
        uint32_t taskCount;
        tcu::Vec4 expectedColor;
        uint32_t program;
    } m_params;
};

class MeshOnlyTriangleCase : public MeshTriangleCase
{
public:
    MeshOnlyTriangleCase(deqp::Context &context, const char *name, const char *desc)
        : MeshTriangleCase(context, name, desc)
    {
        m_params.vertexCoords = {
            tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(-1.0f, 3.0f, 0.0f, 1.0f),
            tcu::Vec4(3.0f, -1.0f, 0.0f, 1.0f),
        };
        m_params.vertexIndices = {0u, 1u, 2u};
        m_params.taskCount     = 1u;
        m_params.expectedColor = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    virtual ~MeshOnlyTriangleCase(void)
    {
    }

    virtual bool initProgram() override;
};

class MeshTaskTriangleCase : public MeshTriangleCase
{
public:
    MeshTaskTriangleCase(deqp::Context &context, const char *name, const char *desc)
        : MeshTriangleCase(context, name, desc)
    {
        m_params.vertexCoords = {
            tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
            tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
        };
        m_params.vertexIndices = {2u, 0u, 1u, 1u, 3u, 2u};
        m_params.taskCount     = 2u;
        m_params.expectedColor = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    virtual ~MeshTaskTriangleCase()
    {
    }

    virtual bool initProgram() override;
};

// Note: not actually task-only. The task shader will not emit mesh shader work groups.
class TaskOnlyTriangleCase : public MeshTriangleCase
{
public:
    TaskOnlyTriangleCase(deqp::Context &context, const char *name, const char *desc)
        : MeshTriangleCase(context, name, desc)
    {
        m_params.vertexCoords = {
            tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(-1.0f, 3.0f, 0.0f, 1.0f),
            tcu::Vec4(3.0f, -1.0f, 0.0f, 1.0f),
        };
        m_params.vertexIndices = {0u, 1u, 2u};
        m_params.taskCount     = 1u;
        m_params.expectedColor = tcu::Vec4(0.1f, 0.1f, 0.1f, 1.0f);
    }

    virtual ~TaskOnlyTriangleCase()
    {
    }

    virtual bool initProgram() override;
};

class MeshShaderSmokeTestsGroup : public deqp::TestCaseGroup
{
public:
    MeshShaderSmokeTestsGroup(deqp::Context &context);
    ~MeshShaderSmokeTestsGroup()
    {
    }

    void init();
};

} // namespace meshShader
} // namespace glc

#endif // _GLCMESHSHADERSMOKETESTS_HPP
