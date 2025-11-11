#ifndef _GLCMESHSHADERAPITESTS_HPP
#define _GLCMESHSHADERAPITESTS_HPP

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

#include "tcuDefs.hpp"
#include "glcTestCase.hpp"
#include "tcuMaybe.hpp"

namespace glc
{
namespace meshShader
{

enum class DrawType
{
    DRAW = 0,
    DRAW_INDIRECT,
    DRAW_INDIRECT_COUNT,
};

// This helps test the maxDrawCount rule for the DRAW_INDIRECT_COUNT case.
enum class IndirectCountLimitType
{
    BUFFER_VALUE = 0, // The actual count will be given by the count buffer.
    MAX_COUNT,        // The actual count will be given by the maxDrawCount argument passed to the draw command.
};

struct IndirectArgs
{
    uint32_t offset;
    uint32_t stride;
};

struct ApiTestParams
{
    DrawType drawType;
    uint32_t seed;
    uint32_t drawCount;                                    // Equivalent to taskCount or drawCount.
    tcu::Maybe<IndirectArgs> indirectArgs;                 // Only used for DRAW_INDIRECT*.
    tcu::Maybe<IndirectCountLimitType> indirectCountLimit; // Only used for DRAW_INDIRECT_COUNT.
    tcu::Maybe<uint32_t> indirectCountOffset;              // Only used for DRAW_INDIRECT_COUNT.
    bool useTask;
    glw::GLuint program;
};

class MeshApiCase : public deqp::TestCase
{
public:
    MeshApiCase(deqp::Context &context, const char *name, const ApiTestParams &params)
        : deqp::TestCase(context, name, "Mesh shader Api tests")
        , m_params(params)
    {
    }
    virtual ~MeshApiCase(void)
    {
    }

    bool initProgram();
    bool ExpectError(glw::GLenum expected_error, const glw::GLchar *function, const glw::GLchar *conditions);

    virtual void init() override;
    virtual IterateResult iterate() override;

protected:
    ApiTestParams m_params;
};

class MeshShaderApiTestsGroup : public deqp::TestCaseGroup
{
public:
    MeshShaderApiTestsGroup(deqp::Context &context);
    ~MeshShaderApiTestsGroup()
    {
    }

    virtual void init();
};

} // namespace meshShader
} // namespace glc

#endif // _GLCMESHSHADERAPITESTS_HPP
