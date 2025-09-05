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

AlignedBFloat16_t::AlignedBFloat16_t(float val) : tcu::BrainFloat16(val)
{
}
AlignedBFloat16_t::AlignedBFloat16_t(const tcu::Vector<float, 1> &v) : AlignedBFloat16_t(v.x())
{
}
AlignedBFloat16_t &AlignedBFloat16_t::operator[](uint32_t)
{
    return *this;
}
const AlignedBFloat16_t &AlignedBFloat16_t::operator[](uint32_t) const
{
    return *this;
}
void AlignedBFloat16_t::revert()
{
}

AlignedBF16Vec2::AlignedBF16Vec2(float x, float y)
    : tcu::Vector<tcu::BrainFloat16, 2>(tcu::BrainFloat16(x), tcu::BrainFloat16(y))
{
}
AlignedBF16Vec2::AlignedBF16Vec2(const tcu::Vec2 &v) : AlignedBF16Vec2(v.x(), v.y())
{
}
void AlignedBF16Vec2::revert()
{
    std::swap(x(), y());
}

AlignedBF16Vec3::AlignedBF16Vec3(float x, float y, float z)
    : tcu::Vector<tcu::BrainFloat16, 3>(tcu::BrainFloat16(x), tcu::BrainFloat16(y), tcu::BrainFloat16(z))
{
}
AlignedBF16Vec3::AlignedBF16Vec3(const tcu::Vec3 &v) : AlignedBF16Vec3(v.x(), v.y(), v.z())
{
}
void AlignedBF16Vec3::revert()
{
    std::swap(x(), z());
}

AlignedBF16Vec4::AlignedBF16Vec4(float x, float y, float z, float w)
    : tcu::Vector<tcu::BrainFloat16, 4>(tcu::BrainFloat16(x), tcu::BrainFloat16(y), tcu::BrainFloat16(z),
                                        tcu::BrainFloat16(w))
{
}
AlignedBF16Vec4::AlignedBF16Vec4(const tcu::Vec4 &v) : AlignedBF16Vec4(v.x(), v.y(), v.z(), v.w())
{
}
void AlignedBF16Vec4::revert()
{
    std::swap(x(), w());
    std::swap(y(), z());
}

template <>
const char *getExtensionName<tcu::FloatE5M2>()
{
    return "GL_EXT_float_e5m2";
}
template <>
const char *getVecTypeName<tcu::FloatE5M2, 1>()
{
    return "floate5m2_t";
}
template <>
const char *getVecTypeName<tcu::FloatE5M2, 2>()
{
    return "fe5m2vec2";
}
template <>
const char *getVecTypeName<tcu::FloatE5M2, 3>()
{
    return "fe5m2vec3";
}
template <>
const char *getVecTypeName<tcu::FloatE5M2, 4>()
{
    return "fe5m2vec4";
}

template <>
const char *getExtensionName<tcu::FloatE4M3>()
{
    return "GL_EXT_float_e4m3";
}
template <>
const char *getVecTypeName<tcu::FloatE4M3, 1>()
{
    return "floate4m3_t";
}
template <>
const char *getVecTypeName<tcu::FloatE4M3, 2>()
{
    return "fe4m3vec2";
}
template <>
const char *getVecTypeName<tcu::FloatE4M3, 3>()
{
    return "fe4m3vec3";
}
template <>
const char *getVecTypeName<tcu::FloatE4M3, 4>()
{
    return "fe4m3vec4";
}

} // namespace bf16

extern void createBFloat16DotTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *bfloat16);
extern void createBFloat16ConstantTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *bfloat16);
extern void createBFloat16ComboTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *bfloat16);

tcu::TestCaseGroup *createBFloat16Tests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> bfloat16(new tcu::TestCaseGroup(testCtx, "bfloat16", "Tests for bfloat16 type"));
    createBFloat16DotTests(testCtx, bfloat16.operator->());
    createBFloat16ConstantTests(testCtx, bfloat16.operator->());
    createBFloat16ComboTests(testCtx, bfloat16.operator->());

    return bfloat16.release();
}

} // namespace shaderexecutor
} // namespace vkt
