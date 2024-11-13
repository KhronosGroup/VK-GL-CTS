/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 *//*!
 * \file
 * \brief Tests for types introduced in VK_KHR_shader_bfloat16.
 *//*--------------------------------------------------------------------*/

#include "vktShaderBFloat16Tests.hpp"
#include "vktTestCase.hpp"
#include "tcuFloat.hpp"

#include <numeric>
#include <typeinfo>

namespace vkt
{
namespace shaderexecutor
{
namespace bf16
{
template <>
const char *getExtensionName<tcu::BrainFloat16>()
{
    return "GL_EXT_bfloat16";
}
template <>
const char *getExtensionName<tcu::Float16>()
{
    return "GL_EXT_shader_explicit_arithmetic_types_float16";
}

template <>
const char *getVecTypeName<tcu::BrainFloat16, 1>()
{
    return "bfloat16_t";
}
template <>
const char *getVecTypeName<tcu::BrainFloat16, 2>()
{
    return "bf16vec2";
}
template <>
const char *getVecTypeName<tcu::BrainFloat16, 3>()
{
    return "bf16vec3";
}
template <>
const char *getVecTypeName<tcu::BrainFloat16, 4>()
{
    return "bf16vec4";
}

template <>
const char *getVecTypeName<tcu::Float16, 1>()
{
    return "float16_t";
}
template <>
const char *getVecTypeName<tcu::Float16, 2>()
{
    return "f16vec2";
}
template <>
const char *getVecTypeName<tcu::Float16, 3>()
{
    return "f16vec3";
}
template <>
const char *getVecTypeName<tcu::Float16, 4>()
{
    return "f16vec4";
}

} // namespace bf16

extern void createBFloat16DotTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *bfloat16);

tcu::TestCaseGroup *createBFloat16Tests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> bfloat16(new tcu::TestCaseGroup(testCtx, "bfloat16", "Tests for bfloat16 type"));
    createBFloat16DotTests(testCtx, bfloat16.operator->());
    return bfloat16.release();
}

} // namespace shaderexecutor
} // namespace vkt
