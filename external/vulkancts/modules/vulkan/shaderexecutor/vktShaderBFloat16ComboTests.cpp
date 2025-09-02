/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Composites, Access chains, Function call amd Swizzling
 *  tests for types introduced in VK_KHR_shader_bfloat16.
 *//*--------------------------------------------------------------------*/

#include "vktShaderBFloat16Tests.hpp"
#include "vktTestCase.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "tcuFloat.hpp"

#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>

namespace vkt
{
namespace shaderexecutor
{
namespace
{
using namespace vk;

enum TestType
{
    Composites,
    AccessChains,
    FunctionCall,
    Swizzling
};

struct Params
{
};

struct BFloat16ComboTestCase : public TestCase
{
    BFloat16ComboTestCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual void checkSupport(Context &context) const override;

protected:
    const Params m_params;
};

void BFloat16ComboTestCase::checkSupport(Context &context) const
{
    if (!context.get16BitStorageFeatures().storageBuffer16BitAccess)
    {
        TCU_THROW(NotSupportedError, "16-bit floats not supported for storage buffers");
    }

    if (!context.getShaderBfloat16Features().shaderBFloat16Type)
    {
        TCU_THROW(NotSupportedError, "shaderBFloat16Type not supported by device");
    }
}

struct BFloat16ComboTestInstance : public TestInstance
{
    BFloat16ComboTestInstance(Context &context, const Params &params) : TestInstance(context), m_params(params)
    {
    }
    typedef std::pair<uint32_t, const char *> VariantItem;
    virtual tcu::TestStatus iterate() override;
    virtual VkDeviceSize genInput(BufferWithMemory *buffer, bool calculateSizeOnly)                            = 0;
    virtual VkDeviceSize genOutput(BufferWithMemory *buffer, bool calculateSizeOnly)                           = 0;
    virtual bool verifyResult(const BufferWithMemory &in, const BufferWithMemory &out, std::string &msg) const = 0;
    virtual std::pair<const VariantItem *, uint32_t> getVariants() const                                       = 0;

    void clearBuffer(BufferWithMemory &buffer) const;

protected:
    const Params m_params;
};

struct A
{
    bf16::AlignedBFloat16_t f1;
    bf16::AlignedBF16Vec2 f2;
    bf16::AlignedBF16Vec3 f3;
    bf16::AlignedBF16Vec4 f4;
    bool operator==(const A &other) const
    {
        return other.f1 == f1 && (tcu::Vec2)other.f2 == f2 && (tcu::Vec3)other.f3 == f3 && (tcu::Vec4)other.f4 == f4;
    }
};
struct AA
{
    A a0, a1;
};
struct B
{
    A a;
    bf16::AlignedBF16Vec2 b[3];
    A c[3];
    bf16::AlignedBF16Vec3 d[2];
};

template <class VecOrScalar>
void fillVecOrScalar(VecOrScalar &u, float &val)
{
    tcu::Vector<float, VecOrScalar::count> v;
    for (int i = 0; i < VecOrScalar::count; ++i)
        v[i] = val++;
    u = v;
}

tcu::Vec1 swizzle(const tcu::Vec1 &v, uint32_t map[1])
{
    return tcu::Vec1(v[map[0]]);
}
tcu::Vec2 swizzle(const tcu::Vec2 &v, uint32_t map[2])
{
    return tcu::Vec2(v[map[0]], v[map[1]]);
}
tcu::Vec3 swizzle(const tcu::Vec3 &v, uint32_t map[3])
{
    return tcu::Vec3(v[map[0]], v[map[1]], v[map[2]]);
}
tcu::Vec4 swizzle(const tcu::Vec4 &v, uint32_t map[4])
{
    return tcu::Vec4(v[map[0]], v[map[1]], v[map[2]], v[map[3]]);
}

template <TestType>
struct ComboSelector;
template <>
struct ComboSelector<Composites>
{
    typedef AA in_type;
    typedef AA out_type;
};
template <>
struct ComboSelector<AccessChains>
{
    typedef std::vector<B> in_type;
    typedef std::vector<B> out_type;
};
template <>
struct ComboSelector<FunctionCall>
{
    typedef std::vector<tcu::BrainFloat16> in_type;
    typedef std::vector<tcu::BrainFloat16> out_type;
};
template <>
struct ComboSelector<Swizzling>
{
    typedef A in_type;
    typedef std::vector<A> out_type;
};

template <TestType _tt>
struct BFloat16ComboInstance : public BFloat16ComboTestInstance
{
    using BFloat16ComboTestInstance::BFloat16ComboTestInstance;
    virtual VkDeviceSize genInput(BufferWithMemory *buffer, bool calculateSizeOnly) override;
    virtual VkDeviceSize genOutput(BufferWithMemory *buffer, bool calculateSizeOnly) override;
    virtual bool verifyResult(const BufferWithMemory &in, const BufferWithMemory &out, std::string &msg) const override;
    virtual std::pair<const VariantItem *, uint32_t> getVariants() const override;

    typedef typename ComboSelector<_tt>::in_type in_type;
    typedef typename ComboSelector<_tt>::out_type out_type;

private:
    in_type m_input;
    out_type m_output;
};

template <TestType _tt>
struct BFloat16ComboCase : public BFloat16ComboTestCase
{
    using BFloat16ComboTestCase::BFloat16ComboTestCase;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override
    {
        return new BFloat16ComboInstance<_tt>(context, m_params);
    }
};

// Composities
template <>
void BFloat16ComboCase<Composites>::initPrograms(SourceCollections &programCollection) const
{
    const std::string comp(R"glsl(
    #version 450
    #extension GL_EXT_bfloat16 : require
    #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout(push_constant) uniform PC { uint variant; };
    struct A
    {
        bfloat16_t f1;
        bf16vec2   f2;
        bf16vec3   f3;
        bf16vec4   f4;
    };
    layout(binding = 0) buffer Input { A a0; A a1; } inp;
    layout(binding = 1) buffer Output { A a0; A a1; } outp;

    void main()
    {
        outp.a0.f1 = inp.a1.f1;
        outp.a0.f2 = inp.a1.f2;
        outp.a0.f3 = inp.a1.f3;
        outp.a0.f4 = inp.a1.f4;

        outp.a1 = inp.a0;
    })glsl");

    programCollection.glslSources.add("test") << glu::ComputeSource(comp);
}

template <>
VkDeviceSize BFloat16ComboInstance<Composites>::genInput(BufferWithMemory *buffer, bool calculateSizeOnly)
{
    if (calculateSizeOnly)
    {
        float val = 1.0f;
        fillVecOrScalar(m_input.a0.f1, val);
        fillVecOrScalar(m_input.a0.f2, val);
        fillVecOrScalar(m_input.a0.f3, val);
        fillVecOrScalar(m_input.a0.f4, val);
        fillVecOrScalar(m_input.a1.f1, val);
        fillVecOrScalar(m_input.a1.f2, val);
        fillVecOrScalar(m_input.a1.f3, val);
        fillVecOrScalar(m_input.a1.f4, val);

        return sizeof(m_input) * 8;
    }

    *((in_type *)buffer->getAllocation().getHostPtr()) = m_input;

    return 0u;
}

template <>
VkDeviceSize BFloat16ComboInstance<Composites>::genOutput(BufferWithMemory *buffer, bool calculateSizeOnly)
{
    if (calculateSizeOnly)
    {
        return sizeof(m_output) * 8;
    }

    m_output = *((out_type *)buffer->getAllocation().getHostPtr());

    return 0u;
}

template <>
std::pair<const BFloat16ComboTestInstance::VariantItem *, uint32_t> BFloat16ComboInstance<Composites>::getVariants()
    const
{
    static const VariantItem variants[]{{0u, ""}};
    return {variants, 1u};
}

template <>
bool BFloat16ComboInstance<Composites>::verifyResult(const BufferWithMemory &in, const BufferWithMemory &out,
                                                     std::string &msg) const
{
    DE_MULTI_UNREF(in, out, msg);

    return m_output.a0 == m_input.a1 && m_output.a1 == m_input.a0;
}

// Swizzling
template <>
void BFloat16ComboCase<Swizzling>::initPrograms(SourceCollections &programCollection) const
{
    const std::string comp(R"glsl(
    #version 450
    #extension GL_EXT_bfloat16 : require
    #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout(push_constant) uniform PC { uint v; };
    struct A
    {
        bfloat16_t f1;
        bf16vec2   f2;
        bf16vec3   f3;
        bf16vec4   f4;
    };
    layout(binding = 0) buffer Input { A inp; };
    layout(binding = 1) buffer Output { A outp[]; };

    void next(bfloat16_t u, out bfloat16_t v, uint k) { v = u; }
    void next(bf16vec2   u, out bf16vec2   v, uint k) { v = (k % 2) == 1 ? u.yx : u.xy; }
    void next(bf16vec3   u, out bf16vec3   v, uint k) {
        switch (k) {
        case  0:   v = u.xyz;   break;
        case  1:   v = u.xzy;   break;
        case  2:   v = u.yxz;   break;
        case  3:   v = u.yzx;   break;
        case  4:   v = u.zxy;   break;
        case  5:   v = u.zyx;   break;
        }
    }
    void next(bf16vec4   u, out bf16vec4   v, uint k) {
        switch (k) {
        case  0:   v = u.xyzw;   break;
        case  1:   v = u.xywz;   break;
        case  2:   v = u.xzyw;   break;
        case  3:   v = u.xzwy;   break;
        case  4:   v = u.xwyz;   break;
        case  5:   v = u.xwzy;   break;
        case  6:   v = u.yxzw;   break;
        case  7:   v = u.yxwz;   break;
        case  8:   v = u.yzxw;   break;
        case  9:   v = u.yzwx;   break;
        case 10:   v = u.ywxz;   break;
        case 11:   v = u.ywzx;   break;
        case 12:   v = u.zxyw;   break;
        case 13:   v = u.zxwy;   break;
        case 14:   v = u.zyxw;   break;
        case 15:   v = u.zywx;   break;
        case 16:   v = u.zwxy;   break;
        case 17:   v = u.zwyx;   break;
        case 18:   v = u.wxyz;   break;
        case 19:   v = u.wxzy;   break;
        case 20:   v = u.wyxz;   break;
        case 21:   v = u.wyzx;   break;
        case 22:   v = u.wzxy;   break;
        case 23:   v = u.wzyx;   break;
        }
    }
    void main() {
        for (uint u = 0; u < v; ++u) {
            next(inp.f1, outp[u].f1, u);
            next(inp.f2, outp[u].f2, u);
            next(inp.f3, outp[u].f3, u % 6);
            next(inp.f4, outp[u].f4, u % 24);
        }
    })glsl");

    programCollection.glslSources.add("test") << glu::ComputeSource(comp);
}

template <>
VkDeviceSize BFloat16ComboInstance<Swizzling>::genInput(BufferWithMemory *buffer, bool calculateSizeOnly)
{
    if (calculateSizeOnly)
    {
        float val = 3.0f;
        fillVecOrScalar(m_input.f1, val);
        fillVecOrScalar(m_input.f2, val);
        fillVecOrScalar(m_input.f3, val);
        fillVecOrScalar(m_input.f4, val);
        return sizeof(A);
    }

    *((A *)buffer->getAllocation().getHostPtr()) = m_input;

    return 0u;
}

template <>
VkDeviceSize BFloat16ComboInstance<Swizzling>::genOutput(BufferWithMemory *buffer, bool calculateSizeOnly)
{
    if (calculateSizeOnly)
    {
        m_output.resize(24u + 6u + 2u + 1u + 91u);
        return m_output.size() * sizeof(A);
    }

    auto range = bf16::makeStdBeginEnd<A>(buffer->getAllocation().getHostPtr(), uint32_t(m_output.size()));
    std::copy(range.first, range.second, m_output.begin());

    return 0u;
}

template <>
std::pair<const BFloat16ComboTestInstance::VariantItem *, uint32_t> BFloat16ComboInstance<Swizzling>::getVariants()
    const
{
    static const VariantItem variants[]{{uint32_t(m_output.size()), "full"}};
    return {variants, 1u};
}

template <>
bool BFloat16ComboInstance<Swizzling>::verifyResult(const BufferWithMemory &in, const BufferWithMemory &out,
                                                    std::string &msg) const
{
    DE_MULTI_UNREF(in, out, msg);

    uint32_t map1[1]{0u};
    uint32_t map2[2]{0u, 1u};
    uint32_t map3[3]{0u, 1u, 2u};
    uint32_t map4[4]{0u, 1u, 2u, 3u};

    bool matches = true;
    for (uint32_t v = 0u; matches && v < uint32_t(m_output.size()); ++v)
    {
        matches &= swizzle(m_input.f1, map1) == m_output[v].f1;
        matches &= swizzle(m_input.f2, map2) == m_output[v].f2;
        matches &= swizzle(m_input.f3, map3) == m_output[v].f3;
        matches &= swizzle(m_input.f4, map4) == m_output[v].f4;

        std::next_permutation(std::begin(map1), std::end(map1));
        std::next_permutation(std::begin(map2), std::end(map2));
        std::next_permutation(std::begin(map3), std::end(map3));
        std::next_permutation(std::begin(map4), std::end(map4));
    }

    return matches;
}

// AccessChains
template <>
void BFloat16ComboCase<AccessChains>::initPrograms(SourceCollections &programCollection) const
{
    const std::string comp(R"glsl(
    #version 450
    #extension GL_EXT_bfloat16 : require
    #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout(push_constant) uniform PC { uint variant; };
    struct A
    {
        bfloat16_t f1;
        bf16vec2   f2;
        bf16vec3   f3;
        bf16vec4   f4;
    };
    struct B
    {
        A        a;
        bf16vec2 b[3];
        A        c[3];
        bf16vec3 d[2];
    };
    layout(binding = 0) buffer Input { B inp[3]; };
    layout(binding = 1) buffer Output { B outp[3]; };

    void main()
    {
        outp[0].a.f1 = inp[1].a.f1;
        outp[1].a.f1 = inp[2].a.f1;
        outp[2].a.f1 = inp[0].a.f1;

        outp[0].c[0].f1 = inp[1].c[1].f1;
        outp[1].c[1].f1 = inp[2].c[2].f1;
        outp[2].c[2].f1 = inp[0].c[0].f1;

        for (uint c = 0; c < 3; ++c)
        {
            const uint c_prim = (c + 1) % 3;
            for (uint b = 0; b < 2; ++b)
            {
                const uint b_prim = (b + 1) % 2;

                outp[c].b[c_prim][b] = inp[c_prim].b[c][b_prim];
                outp[c_prim].d[b_prim][c] = inp[c].d[b][c_prim];
            }
        }
    })glsl");

    programCollection.glslSources.add("test") << glu::ComputeSource(comp);
}

template <>
VkDeviceSize BFloat16ComboInstance<AccessChains>::genInput(BufferWithMemory *buffer, bool calculateSizeOnly)
{
    if (calculateSizeOnly)
    {
        float val = 1.0f;
        m_input.resize(3u);
        for (uint32_t j = 0u; j < 3u; ++j)
        {
            A *ta[]{&m_input[j].a, &m_input[j].c[0], &m_input[j].c[1], &m_input[j].c[2]};
            for (A *a : ta)
            {
                fillVecOrScalar(a->f1, val);
                fillVecOrScalar(a->f2, val);
                fillVecOrScalar(a->f3, val);
                fillVecOrScalar(a->f4, val);
            }
            for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(B::b); ++i)
            {
                fillVecOrScalar(m_input[j].b[i], val);
            }
            for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(B::d); ++i)
            {
                fillVecOrScalar(m_input[j].d[i], val);
            }
        }

        return sizeof(m_input[0]) * m_input.size();
        ;
    }

    auto range = bf16::makeStdBeginEnd<std::remove_reference_t<decltype(m_input[0])>>(
        buffer->getAllocation().getHostPtr(), uint32_t(m_input.size()));
    std::copy(m_input.begin(), m_input.end(), range.first);

    return 0u;
}

template <>
VkDeviceSize BFloat16ComboInstance<AccessChains>::genOutput(BufferWithMemory *buffer, bool calculateSizeOnly)
{
    if (calculateSizeOnly)
    {
        m_output.resize(m_input.size());
        return sizeof(m_output[0]) * m_output.size();
        ;
    }

    auto range = bf16::makeStdBeginEnd<std::remove_reference_t<decltype(m_output[0])>>(
        buffer->getAllocation().getHostPtr(), uint32_t(m_output.size()));
    std::copy(range.first, range.second, m_output.begin());

    return 0u;
}

template <>
std::pair<const BFloat16ComboTestInstance::VariantItem *, uint32_t> BFloat16ComboInstance<AccessChains>::getVariants()
    const
{
    static const VariantItem variants[]{{0, ""}};
    return {variants, 1u};
}

template <>
bool BFloat16ComboInstance<AccessChains>::verifyResult(const BufferWithMemory &in, const BufferWithMemory &out,
                                                       std::string &msg) const
{
    DE_MULTI_UNREF(in, out, msg);

    bool matches         = true;
    const uint32_t c_max = 3;
    const uint32_t b_max = 2;
    for (uint32_t c = 0u; matches && c < c_max; ++c)
    {
        const auto c_prim = (c + 1u) % c_max;
        matches &= m_output[c].a.f1 == m_input[c_prim].a.f1;
        matches &= m_output[c].c[c].f1 == m_input[c_prim].c[c_prim].f1;
        for (uint32_t b = 0u; matches && b < b_max; ++b)
        {
            const auto b_prim = (b + 1u) % b_max;
            matches &= m_output[c].b[c_prim][b].asFloat() == m_input[c_prim].b[c][b_prim].asFloat();
            matches &= m_output[c_prim].d[b_prim][c].asFloat() == m_input[c].d[b][c_prim].asFloat();
        }
    }

    return matches;
}

// FunctionCall
template <>
void BFloat16ComboCase<FunctionCall>::initPrograms(SourceCollections &programCollection) const
{
    const std::string comp(R"glsl(
    #version 450
    #extension GL_EXT_bfloat16 : require
    #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
    #define RET_IN 0
    #define RET_REF 1
    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout(push_constant) uniform PC { uint variant; };
    layout(binding = 0) buffer Input { bfloat16_t inp[]; };
    layout(binding = 1) buffer Output { bfloat16_t outp[]; };

    bfloat16_t ret_in_1(bfloat16_t x) { return x; }
    bf16vec2   ret_in_2(bf16vec2   xy) { return bf16vec2(xy.y, xy.x); }
    bf16vec3   ret_in_3(bf16vec3   xyz) { return bf16vec3(xyz.z, xyz.y, xyz.x); }
    bf16vec4   ret_in_4(bf16vec4   xyzw) { return bf16vec4(xyzw.w, xyzw.z, xyzw.y, xyzw.x); }

    void ret_ref_1(bfloat16_t x,    out bfloat16_t ref) { ref = ret_in_1(x); }
    void ret_ref_2(bf16vec2   xy,   out bf16vec2   ref) { ref = ret_in_2(xy); }
    void ret_ref_3(bf16vec3   xyz,  out bf16vec3   ref) { ref = ret_in_3(xyz); }
    void ret_ref_4(bf16vec4   xyzw, out bf16vec4   ref) { ref = ret_in_4(xyzw); }

    void main() {

        float16_t aaa = float16_t(1.0);
        float16_t bbb = float16_t(1.0);
        float16_t ccc = aaa * bbb;

        bfloat16_t x_in,   x_res;
        bf16vec2   xy_in, xy_res;
        bf16vec3   xyz_in, xyz_res;
        bf16vec4   xyzw_in, xyzw_res;

        x_in = inp[0];
        xy_in.x = inp[1];
        xy_in.y = inp[2];
        xyz_in.x = inp[3];
        xyz_in.y = inp[4];
        xyz_in.z = inp[5];
        xyzw_in.x = inp[6];
        xyzw_in.y = inp[7];
        xyzw_in.z = inp[8];
        xyzw_in.w = inp[9];

        if (variant == RET_IN)
        {
            x_res = ret_in_1(x_in);
            outp[(variant * 8 * 4) + (0 * 4) + 0] = x_res;

            xy_res = ret_in_2(xy_in);
            outp[(variant * 8 * 4) + (1 * 4) + 0] = xy_res.x;
            outp[(variant * 8 * 4) + (1 * 4) + 1] = xy_res.y;

            xyz_res = ret_in_3(xyz_in);
            outp[(variant * 8 * 4) + (2 * 4) + 0] = xyz_res.x;
            outp[(variant * 8 * 4) + (2 * 4) + 1] = xyz_res.y;
            outp[(variant * 8 * 4) + (2 * 4) + 2] = xyz_res.z;

            xyzw_res = ret_in_4(xyzw_in);
            outp[(variant * 8 * 4) + (3 * 4) + 0] = xyzw_res.x;
            outp[(variant * 8 * 4) + (3 * 4) + 1] = xyzw_res.y;
            outp[(variant * 8 * 4) + (3 * 4) + 2] = xyzw_res.z;
            outp[(variant * 8 * 4) + (3 * 4) + 3] = xyzw_res.w;
        }
        else if (variant == RET_REF)
        {
            ret_ref_1(x_in, x_res);
            outp[(variant * 8 * 4) + (0 * 4) + 0] = x_res;

            ret_ref_2(xy_in, xy_res);
            outp[(variant * 8 * 4) + (1 * 4) + 0] = xy_res.x;
            outp[(variant * 8 * 4) + (1 * 4) + 1] = xy_res.y;

            ret_ref_3(xyz_in, xyz_res);
            outp[(variant * 8 * 4) + (2 * 4) + 0] = xyz_res.x;
            outp[(variant * 8 * 4) + (2 * 4) + 1] = xyz_res.y;
            outp[(variant * 8 * 4) + (2 * 4) + 2] = xyz_res.z;

            ret_ref_4(xyzw_in, xyzw_res);
            outp[(variant * 8 * 4) + (3 * 4) + 0] = xyzw_res.x;
            outp[(variant * 8 * 4) + (3 * 4) + 1] = xyzw_res.y;
            outp[(variant * 8 * 4) + (3 * 4) + 2] = xyzw_res.z;
            outp[(variant * 8 * 4) + (3 * 4) + 3] = xyzw_res.w;
        }
    })glsl");

    programCollection.glslSources.add("test") << glu::ComputeSource(comp);
}

template <>
VkDeviceSize BFloat16ComboInstance<FunctionCall>::genInput(BufferWithMemory *buffer, bool calculateSizeOnly)
{
    if (calculateSizeOnly)
    {
        m_input.resize(1u + 2u + 3u + 4u);
        for (uint32_t i = 0; i < m_input.size(); ++i)
            m_input[i] = tcu::BrainFloat16(float(i + 1));
        return m_input.size() * sizeof(tcu::BrainFloat16);
    }

    const VkDeviceSize size = m_input.size() * sizeof(tcu::BrainFloat16);
    auto range =
        bf16::makeStdBeginEnd<tcu::BrainFloat16>(buffer->getAllocation().getHostPtr(), uint32_t(m_input.size()));
    std::copy(m_input.begin(), m_input.end(), range.first);

    return size;
}

template <>
VkDeviceSize BFloat16ComboInstance<FunctionCall>::genOutput(BufferWithMemory *buffer, bool calculateSizeOnly)
{
    if (calculateSizeOnly)
    {
        m_output.resize(256);
        return m_output.size() * sizeof(tcu::BrainFloat16);
    }

    auto range = bf16::makeStdBeginEnd<tcu::BrainFloat16>(buffer->getAllocation().getHostPtr(), 256);
    std::copy(range.first, range.second, m_output.begin());

    return 0;
}

template <>
std::pair<const BFloat16ComboTestInstance::VariantItem *, uint32_t> BFloat16ComboInstance<FunctionCall>::getVariants()
    const
{
    static const VariantItem variants[]{{0, "ret_in"}, {1, "ret_ref"}};
    return {variants, 2};
}

template <>
bool BFloat16ComboInstance<FunctionCall>::verifyResult(const BufferWithMemory &in, const BufferWithMemory &out,
                                                       std::string &msg) const
{
    DE_MULTI_UNREF(in, out, msg);

    std::vector<float> ref(1u + 2u + 3u + 4u);
    for (uint32_t i = 0; i < ref.size(); ++i)
        ref[i] = float(i + 1);

    auto compareGenType = [&](uint32_t variant, uint32_t width, uint32_t refIndex) -> bool
    {
        tcu::Vec1 y_v1, x_v1;
        tcu::Vec2 y_v2, x_v2;
        tcu::Vec3 y_v3, x_v3;
        tcu::Vec4 y_v4, x_v4;
        for (uint32_t i = 0; i < width; ++i)
        {
            const float y = m_output[(variant * 8u * 4u) + ((width - 1u) * 4u) + i].asFloat();
            const float x = ref[refIndex + i];
            switch (width)
            {
            case 1:
                y_v1[i] = y;
                x_v1[i] = x;
                [[fallthrough]];
            case 2:
                y_v2[i] = y;
                x_v2[i] = x;
                [[fallthrough]];
            case 3:
                y_v3[i] = y;
                x_v3[i] = x;
                [[fallthrough]];
            case 4:
                y_v4[i] = y;
                x_v4[i] = x;
            }
        }
        float tmp  = 0.0f;
        bool equal = false;
        switch (width)
        {
        case 1:
            equal = y_v1 == x_v1;
            break;
        case 2:
            tmp      = x_v2.x();
            x_v2.x() = x_v2.y();
            x_v2.y() = tmp;
            equal    = y_v2 == x_v2;
            break;
        case 3:
            tmp      = x_v3.x();
            x_v3.x() = x_v3.z();
            x_v3.z() = tmp;
            equal    = y_v3 == x_v3;
            break;
        case 4:
            tmp      = x_v4.x();
            x_v4.x() = x_v4.w();
            x_v4.w() = tmp;
            tmp      = x_v4.y();
            x_v4.y() = x_v4.z();
            x_v4.z() = tmp;
            equal    = y_v4 == x_v4;
        }
        return equal;
    };
    auto compareVariant = [&](uint32_t variant) -> bool
    {
        bool equal        = true;
        uint32_t refIndex = 0u;
        for (uint32_t width = 1u; width <= 4u; ++width)
        {
            equal &= compareGenType(variant, width, refIndex);
            refIndex += width;
        }
        return equal;
    };

    const bool ret_in_matches  = compareVariant(0);
    const bool ret_ref_matches = compareVariant(1);

    return ret_in_matches && ret_ref_matches;
}

void BFloat16ComboTestInstance::clearBuffer(BufferWithMemory &buffer) const
{
    deMemset(buffer.getAllocation().getHostPtr(), 0, size_t(buffer.getBufferSize()));
}

tcu::TestStatus BFloat16ComboTestInstance::iterate()
{
    const DeviceInterface &di = m_context.getDeviceInterface();
    const uint32_t queueIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue       = m_context.getUniversalQueue();
    const VkDevice dev        = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    const std::vector<uint32_t> queueIndices{queueIndex};
    const VkDeviceSize inBytesSize  = genInput(nullptr, true);
    const VkDeviceSize outBytesSize = genOutput(nullptr, true);
    const VkBufferCreateInfo inBufferCI =
        makeBufferCreateInfo(inBytesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT /*, queueIndices*/);
    BufferWithMemory inBuffer(di, dev, allocator, inBufferCI, MemoryRequirement::HostVisible);
    const VkBufferCreateInfo outBufferCI =
        makeBufferCreateInfo(outBytesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT /*, queueIndices*/);
    BufferWithMemory outBuffer(di, dev, allocator, outBufferCI, MemoryRequirement::HostVisible);
    const VkDescriptorBufferInfo inBufferDBI  = makeDescriptorBufferInfo(*inBuffer, 0u, inBytesSize);
    const VkDescriptorBufferInfo outBufferDBI = makeDescriptorBufferInfo(*outBuffer, 0u, outBytesSize);
    Move<VkDescriptorPool> dsPool             = DescriptorPoolBuilder()
                                        .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u)
                                        .build(di, dev, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSetLayout> dsLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(di, dev);
    Move<VkDescriptorSet> ds = makeDescriptorSet(di, dev, *dsPool, *dsLayout);
    DescriptorSetUpdateBuilder()
        .writeSingle(*ds, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &inBufferDBI)
        .writeSingle(*ds, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &outBufferDBI)
        .update(di, dev);
    const VkPushConstantRange range{
        VK_SHADER_STAGE_COMPUTE_BIT, //
        0u,                          //
        (uint32_t)sizeof(uint32_t)   //
    };
    Move<VkShaderModule> shader           = createShaderModule(di, dev, m_context.getBinaryCollection().get("test"), 0);
    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(di, dev, *dsLayout, &range);
    Move<VkPipeline> pipeline             = makeComputePipeline(di, dev, *pipelineLayout, *shader);
    Move<VkCommandPool> cmdPool           = makeCommandPool(di, dev, queueIndex);
    Move<VkCommandBuffer> cmd             = allocateCommandBuffer(di, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    clearBuffer(outBuffer);
    genInput(&inBuffer, false);
    auto variantInfo  = getVariants();
    auto variantRange = bf16::makeStdBeginEnd<VariantItem>(variantInfo.first, variantInfo.second);

    beginCommandBuffer(di, *cmd);
    di.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    di.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &ds.get(), 0u, nullptr);
    for (auto variant = variantRange.first; variant != variantRange.second; ++variant)
    {
        di.cmdPushConstants(*cmd, *pipelineLayout, range.stageFlags, range.offset, range.size, &variant->first);
        di.cmdDispatch(*cmd, 1u, 1u, 1u);
    }
    endCommandBuffer(di, *cmd);
    submitCommandsAndWait(di, dev, queue, *cmd);

    invalidateAlloc(di, dev, outBuffer.getAllocation());
    genOutput(&outBuffer, false);

    std::string msg;
    if (const bool verdict = verifyResult(inBuffer, outBuffer, msg); verdict == false)
    {
        return tcu::TestStatus::fail(msg);
    }

    return tcu::TestStatus::pass({});
}

} // namespace

void createBFloat16ComboTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *bfloat16)
{
    de::MovePtr<tcu::TestCaseGroup> various(
        new tcu::TestCaseGroup(testCtx, "various", "Various tests for bfloat16 type"));

    Params p;
    various->addChild(new BFloat16ComboCase<Composites>(testCtx, "composites", p));
    various->addChild(new BFloat16ComboCase<AccessChains>(testCtx, "access_chains", p));
    various->addChild(new BFloat16ComboCase<FunctionCall>(testCtx, "function_call", p));
    various->addChild(new BFloat16ComboCase<Swizzling>(testCtx, "swizzling", p));

    return bfloat16->addChild(various.release());
}

} // namespace shaderexecutor
} // namespace vkt
