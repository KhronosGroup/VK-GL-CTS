/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Arm Limited.
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
 * \brief Functional OpFmaKHR tests
 *//*--------------------------------------------------------------------*/

#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "deRandom.hpp"

#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmFmaTests.hpp"

#include <limits>
#include <string>
#include <array>
#include <cassert>
#include <cmath>

#include <fenv.h>

// VK_KHR_shader_fma tests

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::string;

namespace
{

// Rounding modes that are supported for Vulkan SPIR-V.
enum RoundingMode
{
    ROUND_RTZ,
    ROUND_RTE,
    ROUND_UNDEF
};

static string RoundingModeToNameString(RoundingMode m)
{
    switch (m)
    {
    case ROUND_RTZ:
        return "rtz";
    case ROUND_RTE:
        return "rte";
    case ROUND_UNDEF:
        return "undef";
    default:
        assert(false);
        return ""; // Not valid.
    }
}

static string RoundingModeToExecutionMode(RoundingMode m)
{
    switch (m)
    {
    case ROUND_RTZ:
        return "RoundingModeRTZ";
    case ROUND_RTE:
        return "RoundingModeRTE";
    default:
        assert(false);
        return ""; // ROUND_UNDEF doesn't have an execution mode, so this is not valid.
    }
}

enum DenormMode
{
    DENORM_PRESERVE,
    DENORM_FLUSH,
    DENORM_UNDEF
};

static string DenormModeToNameString(DenormMode d)
{
    switch (d)
    {
    case DENORM_PRESERVE:
        return "denorm_preserve";
    case DENORM_FLUSH:
        return "denorm_flush";
    case DENORM_UNDEF:
        return "denorm_none";
    default:
        assert(false);
        return ""; // Not valid.
    }
}

static string DenormModeToExecutionMode(DenormMode d)
{
    switch (d)
    {
    case DENORM_PRESERVE:
        return "DenormPreserve";
    case DENORM_FLUSH:
        return "DenormFlushToZero";
    default:
        assert(false);
        return ""; // DENORM_UNDEF doesn't have an execution mode, so not valid.
    }
}

using std::vector;
using tcu::IVec3;
using tcu::TestLog;

string getFmaCode(uint64_t bitDepth, uint32_t vecSz, RoundingMode m, DenormMode d, bool useSZInfNan)
{
    string capabilities("OpCapability FMAKHR\n");
    string extensions("OpExtension \"SPV_KHR_fma\"\n");

    if (m != ROUND_UNDEF || d != DENORM_UNDEF || useSZInfNan)
        extensions += "OpExtension \"SPV_KHR_float_controls\"\n";

    string execModes = "";
    if (m != ROUND_UNDEF)
    {
        capabilities += "OpCapability " + RoundingModeToExecutionMode(m) + "\n";
        execModes += "OpExecutionMode %main " + RoundingModeToExecutionMode(m) + " " + std::to_string(bitDepth) + "\n";
    }

    if (d != DENORM_UNDEF)
    {
        capabilities += "OpCapability " + DenormModeToExecutionMode(d) + "\n";
        execModes += "OpExecutionMode %main " + DenormModeToExecutionMode(d) + " " + std::to_string(bitDepth) + "\n";
    }

    if (useSZInfNan)
    {
        capabilities += "OpCapability SignedZeroInfNanPreserve\n";
        execModes += "OpExecutionMode %main SignedZeroInfNanPreserve " + std::to_string(bitDepth) + "\n";
    }

    if (bitDepth != 32)
        capabilities += "OpCapability Float" + std::to_string(bitDepth) + "\n";

    string fmaCode = string(getComputeAsmShaderPreamble(capabilities, extensions)) + execModes +
                     "OpName %main \"main\"\n"
                     "OpName %id   \"gl_GlobalInvocationID\"\n"

                     "OpDecorate %id BuiltIn GlobalInvocationId\n"
                     "OpDecorate %buf BufferBlock\n"
                     "OpDecorate %indata1 DescriptorSet 0\n"
                     "OpDecorate %indata1 Binding 0\n"
                     "OpDecorate %indata2 DescriptorSet 0\n"
                     "OpDecorate %indata2 Binding 1\n"
                     "OpDecorate %indata3 DescriptorSet 0\n"
                     "OpDecorate %indata3 Binding 2\n"

                     "OpDecorate %outdata DescriptorSet 0\n"
                     "OpDecorate %outdata Binding 3\n"

                     "OpDecorate %datarr ArrayStride " +
                     std::to_string(bitDepth / 8) +
                     "\n"

                     "OpMemberDecorate %buf 0 Offset 0\n"

                     "%void      = OpTypeVoid\n"
                     "%voidf     = OpTypeFunction %void\n"
                     "%u32       = OpTypeInt 32 0\n"
                     "%i32       = OpTypeInt 32 1\n"
                     "%uvec3     = OpTypeVector %u32 3\n"
                     "%uvec3ptr  = OpTypePointer Input %uvec3\n"

                     "%dat       = OpTypeFloat " +
                     std::to_string(bitDepth) +
                     "\n"
                     "%datptr    = OpTypePointer Uniform %dat\n"
                     "%datarr    = OpTypeRuntimeArray %dat\n"
                     "%vec2      = OpTypeVector %dat 2\n"
                     "%vec3      = OpTypeVector %dat 3\n"
                     "%vec4      = OpTypeVector %dat 4\n"

                     "%buf       = OpTypeStruct %datarr\n"
                     "%bufptr    = OpTypePointer Uniform %buf\n"
                     "%indata1   = OpVariable %bufptr Uniform\n"
                     "%indata2   = OpVariable %bufptr Uniform\n"
                     "%indata3   = OpVariable %bufptr Uniform\n"

                     "%outdata   = OpVariable %bufptr Uniform\n"

                     "%id        = OpVariable %uvec3ptr Input\n"
                     "%zero      = OpConstant %i32 0\n"
                     "%one       = OpConstant %i32 1\n"
                     "%two       = OpConstant %i32 2\n"
                     "%three     = OpConstant %i32 3\n"
                     "%stride    = OpConstant %u32 65536\n"
                     "%vec_sz    = OpConstant %i32 " +
                     std::to_string(vecSz) +
                     "\n"

                     "%main      = OpFunction %void None %voidf\n"
                     "%label     = OpLabel\n"
                     "%idval     = OpLoad %uvec3 %id\n"
                     "%x         = OpCompositeExtract %u32 %idval 0\n"
                     "%y         = OpCompositeExtract %u32 %idval 1\n"
                     "%scale_y   = OpIMul %u32 %y %stride\n"
                     "%vec_idx   = OpIAdd %u32 %scale_y %x\n"
                     "%idx       = OpIMul %u32 %vec_idx %vec_sz\n";

    if (vecSz == 1)
    {
        fmaCode += "%loc1      = OpAccessChain %datptr %indata1 %zero %idx\n"
                   "%loc2      = OpAccessChain %datptr %indata2 %zero %idx\n"
                   "%loc3      = OpAccessChain %datptr %indata3 %zero %idx\n"
                   "%val1      = OpLoad %dat %loc1\n"
                   "%val2      = OpLoad %dat %loc2\n"
                   "%val3      = OpLoad %dat %loc3\n"

                   "%res       = OpFmaKHR %dat %val1 %val2 %val3\n"
                   "%outloc    = OpAccessChain %datptr %outdata %zero %idx\n"
                   "             OpStore %outloc %res\n";
    }
    else
    {
        string typeStr[5]  = {"not used", "%dat", "%vec2", "%vec3", "%vec4"};
        string indexStr[4] = {"%zero", "%one", "%two", "%three"};

        for (uint32_t i = 0; i < vecSz; i++)
        {
            string strI = std::to_string(i);
            fmaCode += "%idx" + strI + " = OpIAdd %u32 %idx " + indexStr[i] +
                       "\n"
                       "%loc1" +
                       strI + " = OpAccessChain %datptr %indata1 %zero %idx" + strI +
                       "\n"
                       "%loc2" +
                       strI + " = OpAccessChain %datptr %indata2 %zero %idx" + strI +
                       "\n"
                       "%loc3" +
                       strI + " = OpAccessChain %datptr %indata3 %zero %idx" + strI +
                       "\n"
                       "%val1" +
                       strI + " = OpLoad %dat %loc1" + strI +
                       "\n"
                       "%val2" +
                       strI + " = OpLoad %dat %loc2" + strI +
                       "\n"
                       "%val3" +
                       strI + " = OpLoad %dat %loc3" + strI + "\n";
        }
        fmaCode += "%val1 = OpCompositeConstruct " + typeStr[vecSz];
        for (uint32_t i = 0; i < vecSz; i++)
            fmaCode += " %val1" + std::to_string(i);
        fmaCode += "\n";
        fmaCode += "%val2 = OpCompositeConstruct " + typeStr[vecSz];
        for (uint32_t i = 0; i < vecSz; i++)
            fmaCode += " %val2" + std::to_string(i);
        fmaCode += "\n";
        fmaCode += "%val3 = OpCompositeConstruct " + typeStr[vecSz];
        for (uint32_t i = 0; i < vecSz; i++)
            fmaCode += " %val3" + std::to_string(i);
        fmaCode += "\n";

        fmaCode += "%res       = OpFmaKHR " + typeStr[vecSz] + " %val1 %val2 %val3\n";

        for (uint32_t i = 0; i < vecSz; i++)
        {
            string strI = std::to_string(i);
            fmaCode += "%res" + strI + " = OpCompositeExtract %dat %res " + strI +
                       "\n"
                       "%outloc" +
                       strI + " = OpAccessChain %datptr %outdata %zero %idx" + strI +
                       "\n"
                       "             OpStore %outloc" +
                       strI + " %res" + strI + "\n";
        }
    }

    fmaCode += "             OpReturn\n"
               "             OpFunctionEnd\n";

    return fmaCode;
}

static bool isValid(deFloat16 a, deFloat16 b)
{
    return a == b || (deHalfIsIEEENaN(a) && deHalfIsIEEENaN(b));
}

static bool isValid(float a, float b)
{
    return deFloatBitsToUint32(a) == deFloatBitsToUint32(b) || (std::isnan(a) && std::isnan(b));
}

static bool isValid(double a, double b)
{
    return deDoubleBitsToUint64(a) == deDoubleBitsToUint64(b) || (std::isnan(a) && std::isnan(b));
}

template <typename T>
bool isZero(T a)
{
    return a == T(0);
}

template <>
bool isZero(deFloat16 a)
{
    return deHalfIsZero(a);
}

template <typename T>
static bool isDenorm(T f)
{
    return std::fpclassify(f) == FP_SUBNORMAL;
}

template <>
bool isDenorm(deFloat16 f)
{
    return deHalfIsDenormal(f);
}

template <typename T>
bool IsInfNan(T x)
{
    return (std::isinf(x) || std::isnan(x));
}

template <>
bool IsInfNan(deFloat16 x)
{
    return (deHalfIsInf(x) || deHalfIsIEEENaN(x));
}

template <typename T>
T negate(T x)
{
    return -x;
}

template <>
deFloat16 negate(deFloat16 x)
{
    return x ^ 0x8000;
}

template <typename T>
static vector<T> getValidFlushedValues(T a, DenormMode d)
{
    // DenormFlushToZero execution mode is not required to flush denormal inputs, so
    // behaves the same as undefined here. Any combination of flushed and non-flushed
    // inputs is valid.
    vector<T> valid = {a};

    if (isDenorm(a) && d != DENORM_PRESERVE)
    {
        valid.push_back(T(0));
        valid.push_back(negate(T(0)));
    }
    return valid;
}

template <typename T>
static vector<std::array<T, 3>> getAllowedInputs(T a, T b, T c, DenormMode d)
{
    vector<std::array<T, 3>> allowedInputs;

    vector<T> validA = getValidFlushedValues(a, d);
    vector<T> validB = getValidFlushedValues(b, d);
    vector<T> validC = getValidFlushedValues(c, d);

    for (T inA : validA)
        for (T inB : validB)
            for (T inC : validC)
                allowedInputs.push_back({inA, inB, inC});

    // If signed-zero is not being preserved then we should, in theory, have combinations with
    // the sign of zero changed as well but it turns out that this is never significant. If 'a'
    // or 'b' are 0 then the result is either NaN (which has no sign), or 0. In the add stage
    // the sign of 0 only matters if the result is 0, and we will account for the sign of a 0
    // result separately.

    return allowedInputs;
}

template <typename T>
T roundedFMA(T a, T b, T c, deRoundingMode m)
{
    deSetRoundingMode(m);
    return std::fma(a, b, c);
}

template <>
deFloat16 roundedFMA(deFloat16 a, deFloat16 b, deFloat16 c, deRoundingMode m)
{
    double x = deFloat16To64(a);
    double y = deFloat16To64(b);
    double z = deFloat16To64(c);
    deSetRoundingMode(m);
    double ref = std::fma(x, y, z);
    return deFloat64To16Round(ref, m);
}

template <typename T>
static vector<T> getRefValues(T a, T b, T c, RoundingMode m, DenormMode d, bool signedZero)
{
    vector<T> ret;

    // We will change the rounding mode, so save the current one and restore it later.
    deRoundingMode rm = deGetRoundingMode();

    vector<deRoundingMode> allowedRoundingModes;
    switch (m)
    {
    case ROUND_RTZ:
        allowedRoundingModes.push_back(DE_ROUNDINGMODE_TO_ZERO);
        break;
    case ROUND_RTE:
        allowedRoundingModes.push_back(DE_ROUNDINGMODE_TO_NEAREST_EVEN);
        break;
    case ROUND_UNDEF:
        allowedRoundingModes.push_back(DE_ROUNDINGMODE_TO_POSITIVE_INF);
        allowedRoundingModes.push_back(DE_ROUNDINGMODE_TO_NEGATIVE_INF);
        break;
    }

    // Multiple inputs may be valid if denorms are flushed, so get the complete set
    vector<std::array<T, 3>> allowedInputs = getAllowedInputs(a, b, c, d);

    // For each allowed input vector, calculate all valid results
    for (deRoundingMode mode : allowedRoundingModes)
    {
        for (auto inp : allowedInputs)
        {
            T r = roundedFMA(inp[0], inp[1], inp[2], mode);

            // Calculate variants rounded upward and downward for underflow detection.
            // (RTZ and the rounded result would do here for detecting the largest
            // denorms, but because 0.0 is not denormal we also need to check that both
            // values have not rounded down to 0.0. A tiny denorm that is flushed may be
            // more permissive with the sign of zero than an actual zero result).
            T rDown = roundedFMA(inp[0], inp[1], inp[2], DE_ROUNDINGMODE_TO_NEGATIVE_INF);
            T rUp   = roundedFMA(inp[0], inp[1], inp[2], DE_ROUNDINGMODE_TO_POSITIVE_INF);

            bool underflowAfterRounding  = isDenorm(r);
            bool underflowBeforeRounding = isDenorm(rUp) || isDenorm(rDown);

            // underflowAfterRounding => underflowBeforeRounding.
            assert(underflowBeforeRounding || !underflowAfterRounding);

            // If denorms are allowed to be preserved or if this might not have underflowed
            // (because of rounding) then the CPU-generated correctly rounded result is allowed.
            if (d != DENORM_FLUSH || !underflowAfterRounding)
                ret.push_back(r);
            // If denorms are allowed to be flushed and this might have underflowed, then flushing
            // is allowed.
            if (d != DENORM_PRESERVE && underflowBeforeRounding)
            {
                // Vulkan allows denorms to be flushed to either +/- 0.0f
                ret.push_back(T(0));
                ret.push_back(negate(T(0)));
            }
            if (isZero(r) && !signedZero)
                ret.push_back(static_cast<T>(-r)); // Casting otherwise GCC gives warning
        }
    }

    // Restore the mode we recorded before the beginning.
    deSetRoundingMode(rm);
    return ret;
}

template <typename T>
bool UsesInfNan(const vector<T> &r, T a, T b, T c)
{
    if (IsInfNan(a) || IsInfNan(b) || IsInfNan(c))
        return true;
    for (T res : r)
        if (IsInfNan(res))
            return true;

    return false;
}

template <typename T>
static bool verifyResult(const std::vector<Resource> &inputs, const std::vector<AllocationSp> &outputAllocations,
                         RoundingMode m, DenormMode d, bool szInfNan, tcu::TestLog &log)
{
    vector<uint8_t> aBytes, bBytes, cBytes;
    inputs[0].getBytes(aBytes);
    inputs[1].getBytes(bBytes);
    inputs[2].getBytes(cBytes);

    const T *a = reinterpret_cast<T *>(aBytes.data());
    const T *b = reinterpret_cast<T *>(bBytes.data());
    const T *c = reinterpret_cast<T *>(cBytes.data());

    const size_t count = aBytes.size() / sizeof(T);

    const T *res = reinterpret_cast<T *>(outputAllocations[0]->getHostPtr());

    const size_t errorsMax = 16u;
    size_t errors          = 0u;

    for (size_t ndx = 0; ndx < count; ++ndx)
    {
        vector<T> refValues = getRefValues(a[ndx], b[ndx], c[ndx], m, d, szInfNan);

        // If not using the SignedZeroInfNanPreserve execution mode then any input our output
        // that is inf/nan means that any value may be returned. Skip checking that case.
        if (!szInfNan && UsesInfNan(refValues, a[ndx], b[ndx], c[ndx]))
            continue;

        bool ok = false;
        for (T ref : refValues)
            ok = ok || isValid(res[ndx], ref);

        if (!ok)
        {
            std::ostringstream oss;
            if (refValues.size() == 1)
            {
                oss << std::hexfloat << refValues[0];
            }
            else
            {
                oss << "one of {";
                for (T ref : refValues)
                {
                    oss << " " << std::hexfloat << ref << " ";
                }
                oss << "}";
            }

            log << tcu::TestLog::Message << " fma(" << std::hexfloat << a[ndx] << ", " << b[ndx] << ", " << c[ndx]
                << ")"
                << " got: " << res[ndx] << ", expected: " << oss.str() << " at index " << std::dec << ndx
                << tcu::TestLog::EndMessage;
            errors++;

            if (errors >= errorsMax)
            {
                log << tcu::TestLog::Message << "Maximum error count reached (" << errors << "). Stop output."
                    << tcu::TestLog::EndMessage;
                break;
            }
        }
    }

    return (errors == 0);
}

template <typename T, RoundingMode m, DenormMode d, bool szInfNan>
static bool verify(const std::vector<Resource> &inputs, const std::vector<AllocationSp> &outputAllocations,
                   const std::vector<Resource> &expectedOutputs, tcu::TestLog &log)
{
    (void)expectedOutputs;
    return verifyResult<T>(inputs, outputAllocations, m, d, szInfNan, log);
}

enum InputMode
{
    INPUTS_RANDOM,
    INPUTS_DIRECTED
};

template <typename T>
T getRandomVal(de::Random &rnd)
{
    return de::randomScalar<T>(rnd, 0.0001f, 10000.0f);
}

template <>
deFloat16 getRandomVal(de::Random &rnd)
{
    return de::randomScalar<uint16_t>(rnd, 0x0400, 0x7BFF);
}

template <typename T>
vector<T> GetSpecialValues();

template <>
vector<deFloat16> GetSpecialValues()
{
    return {0x7C01, // quiet_NaN
            0x7C00, // infinity
            0x7BFF, // max
            0x5C01, //
            0x5C00, //
            0x5BFF, //
            0x5801, //              [1]
            0x5800, //
            0x57FF, //
            0x4201, // 3.0f + 1ULP
            0x4200, // 3.0f
            0x41FF, // 3.0f - 1ULP
            0x4100, // 2.5f
            0x4000, // 2.0f
            0x3f00, // 1.75f
            0x3E01, // 1.5f + 1ULP
            0x3E00, // 1.5f
            0x3DFF, // 1.5f - 1ULP
            0x3D00, // 1.25f
            0x3C20, //
            0x3C30, //
            0x3C02, // 1.0f + 2ULP
            0x3C01, // 1.0f + 1ULP
            0x3C00, // 1.0f
            0x3BFF, // 1.0f - 1ULP  [1]
            0x0401, // min + 1ULP   [1]
            0x0400, // min,
            0x03FF, // largest denorm
            0x007F, //
            0x001F, //
            0x0007, //
            0x0006, //
            0x0005, //
            0x0004, //
            0x0003, //
            0x0002, //
            0x0001, //
            0x0000};
    // [1] As well as being potentially interesting in their own right, these values give incorrect
    // results when fma is emulated using single precision (using RTNE):
    //           fma_half(0x5801, 0x3BFF, 0x0401) == 0x5801
    //     (half)fmaf    (0x5801, 0x3BFF, 0x0401) == 0x5800
}

template <>
vector<float> GetSpecialValues()
{
    // Special values which may provide interesting coverage. This list was taken (lightly
    // modified) from the OpenCL CTS.
    return {+std::numeric_limits<float>::quiet_NaN(),
            +std::numeric_limits<float>::infinity(),
            +std::numeric_limits<float>::max(),
            +0x1.000002p64f,
            +0x1.0p64f,
            +0x1.fffffep63f,
            +0x1.000002p63f,
            +0x1.0p63f,
            +0x1.fffffep62f,
            +0x1.800002p1f,
            +3.0f,
            +0x1.7ffffep1f,
            +2.5f,
            +2.0f,
            +1.75f,
            +0x1.800002p0f,
            +1.5f,
            +0x1.7ffffep0f,
            +1.25f, // [1]
            +0x1.003p0f,
            +0x1.001p0f,
            +0x1.000004p0f, // [1]
            +0x1.000002p0f,
            +1.0f,
            +0x1.fffffep-1f,
            +0x1.000002p-126f, // [1]
            +std::numeric_limits<float>::min(),
            +0x0.fffffep-126f,
            +0x0.000ffep-126f,
            +0x0.0000fep-126f,
            +0x0.00000ep-126f,
            +0x0.00000cp-126f,
            +0x0.00000ap-126f,
            +0x0.000008p-126f,
            +0x0.000006p-126f,
            +0x0.000004p-126f,
            +0x0.000002p-126f,
            +0.0f};
    // [1] As well as being potentially interesting in their own right, these values give incorrect
    // results when fma is emulated using double precision (using RTNE):
    //           fmaf(1.25, 0x1.000004p0f, 0x1.000002p-126f) == 0x1.400006p+0f
    //    (float)fma (1.25, 0x1.000004p0f, 0x1.000002p-126f) == 0x1.400004p+0f
}

template <>
vector<double> GetSpecialValues()
{
    return {+std::numeric_limits<double>::quiet_NaN(),
            +std::numeric_limits<double>::infinity(),
            +std::numeric_limits<double>::max(),
            +0x1.0000000000001p+512,
            +0x1.0p512,
            +0x1.fffffffffffffp+511 + 0x1.0000000000001p+511,
            +0x1.0p511,
            +0x1.fffffffffffffp+510,
            +0x1.8000000000001p+1,
            +3.0,
            +0x1.7ffffffffffffp+1,
            +2.5,
            +2.0,
            +1.75,
            +0x1.8000000000001p+0,
            +1.5,
            +0x1.7ffffffffffffp+0,
            +1.25,
            +0x1.0000006p+0,
            +0x1.0000004p+0,
            +0x1.0000000000002p+0,
            +0x1.0000000000001p+0,
            +1.0,
            +0x1.fffffffffffffp-1,
            +0x1.0000000000001p-1022,
            +std::numeric_limits<double>::min(),
            +0x0.fffffffffffffp-1022,
            +0x0.0000000000ff7p-1022,
            +0x0.00000000000f7p-1022,
            +0x0.0000000000007p-1022,
            +0x0.0000000000006p-1022,
            +0x0.0000000000005p-1022,
            +0x0.0000000000004p-1022,
            +0x0.0000000000003p-1022,
            +0x0.0000000000002p-1022,
            +0x0.0000000000001p-1022,
            +0.0f};
}

template <typename T>
T getCancellationValue(T a, T b)
{
    return -(a * b);
}

template <>
deFloat16 getCancellationValue(deFloat16 a, deFloat16 b)
{
    return deFloat32To16(-(deFloat16To32(a) * deFloat16To32(b)));
}

template <typename T>
class RandomBuffer : public BufferInterface
{
public:
    RandomBuffer(uint32_t numValues_, uint32_t seed_) : numValues(numValues_), seed(seed_)
    {
    }

    virtual void getBytes(std::vector<uint8_t> &bytes) const override
    {
        de::Random rnd(seed);

        std::vector<T> v(numValues);
        for (uint32_t ndx = 0; ndx < numValues; ndx++)
            v[ndx] = getRandomVal<T>(rnd);

        bytes.resize(getByteSize());
        memcpy(bytes.data(), v.data(), getByteSize());
    }

    virtual void getPackedBytes(std::vector<uint8_t> &bytes) const override
    {
        getBytes(bytes);
    }

    virtual size_t getByteSize(void) const override
    {
        return numValues * sizeof(T);
    }

private:
    uint32_t numValues;
    uint32_t seed;
};

template <typename T>
class DirectedBuffer : public BufferInterface
{
public:
    DirectedBuffer(uint32_t channel_, uint32_t vecSz_) : channel(channel_), vecSz(vecSz_)
    {
    }

    virtual void getBytes(std::vector<uint8_t> &bytes) const override
    {
        // Test all combinations of SpecialValues.
        std::vector<T> s;
        FillSpecialValueInputs(s);

        // Add cancellation cases (of the form a * b - (a*b)), which should give non-zero results
        // with FMA, returning the rounding error in calculating a*b (on the CPU -- the GPU may
        // round differently, but that doesn't affect the coverage of the test).
        std::vector<T> c;
        FillCancellationInputs(c);

        size_t sz = (s.size() + c.size()) * sizeof(T);
        bytes.resize(sz);
        memcpy(bytes.data(), s.data(), s.size() * sizeof(T));
        memcpy(bytes.data() + s.size() * sizeof(T), c.data(), c.size() * sizeof(T));
    }

    virtual void getPackedBytes(std::vector<uint8_t> &bytes) const override
    {
        getBytes(bytes);
    }

    virtual size_t getByteSize(void) const override
    {
        return (NumSpecialValueCases() + NumCancellationCases()) * sizeof(T);
    }

private:
    static size_t NumSpecialValueCases()
    {
        // SpecialValues only contains positive values, so *2 to include negative as well.
        size_t numValues = 2 * GetSpecialValues<T>().size();
        // FMA is a ternary op, so the total number of these cases is numValues ^ 3.
        return numValues * numValues * numValues;
    }

    size_t NumCancellationCases() const
    {
        // Add at least a minimum number (because they're a good test of fma), but this is also
        // used to round up to a valid work size (ie. a multiple of 65536), which is needed in
        // order to be able to launch all the work in a single 2D dispatch.
        size_t minCancellationCases = 100;

        size_t totalCases = NumSpecialValueCases() + minCancellationCases;
        if (totalCases % vecSz != 0)
            totalCases += (vecSz - (totalCases % vecSz));

        if (totalCases / vecSz > 65536)
            totalCases += vecSz * 65536 - (totalCases % (vecSz * 65536));

        assert(totalCases % vecSz == 0);
        assert((totalCases / vecSz <= 65536) || ((totalCases / vecSz) % 65536) == 0);

        return totalCases - NumSpecialValueCases();
    }

    void FillSpecialValueInputs(std::vector<T> &inputs) const
    {
        vector<T> values;
        for (T f : GetSpecialValues<T>())
        {
            values.push_back(f);
            values.push_back(negate(f));
        }

        // The different channels iterate over the values at different speeds so that all combinations are tested.
        size_t numConsecutive = (channel == 0) ? values.size() * values.size() : (channel == 1) ? values.size() : 1;
        size_t numReps        = values.size() * values.size() / numConsecutive;
        for (size_t i = 0; i < numReps; i++)
            for (T v : values)
                inputs.insert(inputs.end(), numConsecutive, v);

        assert(inputs.size() == NumSpecialValueCases());
    }

    void FillCancellationInputs(std::vector<T> &inputs) const
    {
        // Cancellation cases are very simple, (a, b, -(a*b)), but because the buffers are
        // generated separately and the random numbers must match, generating them is more complex.
        size_t numCancellationCases = NumCancellationCases();
        std::vector<T> c[2];
        for (uint32_t i = 0; i < 2; i++)
        {
            if (channel != i && channel != 2)
                continue;

            de::Random rnd(deStringHash("fma.directed_inputs_cancellation") + i);
            c[i].resize(numCancellationCases);
            for (unsigned j = 0; j < numCancellationCases; j++)
                c[i][j] = getRandomVal<T>(rnd);
        }

        if (channel == 2)
        {
            inputs.resize(numCancellationCases);
            for (unsigned j = 0; j < numCancellationCases; j++)
                inputs[j] = getCancellationValue(c[0][j], c[1][j]);
        }
        else
            inputs = c[channel];
    }

    uint32_t channel;
    uint32_t vecSz;
};

template <typename T>
size_t addInputOutputBuffers(ComputeShaderSpec &spec, InputMode inputMode, uint32_t vecSz)
{
    BufferSp aBuf, bBuf, cBuf;

    assert(inputMode == INPUTS_RANDOM || inputMode == INPUTS_DIRECTED);

    if (inputMode == INPUTS_RANDOM)
    {
        const size_t numRandomInputs = 768;
        de::Random rnd(deStringHash("fma.random_inputs"));
        aBuf = BufferSp(new RandomBuffer<T>(numRandomInputs, rnd.getUint32()));
        bBuf = BufferSp(new RandomBuffer<T>(numRandomInputs, rnd.getUint32()));
        cBuf = BufferSp(new RandomBuffer<T>(numRandomInputs, rnd.getUint32()));
    }
    else
    {
        aBuf = BufferSp(new DirectedBuffer<T>(0, vecSz));
        bBuf = BufferSp(new DirectedBuffer<T>(1, vecSz));
        cBuf = BufferSp(new DirectedBuffer<T>(2, vecSz));
    }

    spec.inputs.push_back(aBuf);
    spec.inputs.push_back(bBuf);
    spec.inputs.push_back(cBuf);

    size_t bufSize = aBuf->getByteSize();
    // Not used. The reference value is computed from the inputs in the verification function.
    spec.outputs.push_back(BufferSp(new UninitializedBuffer(bufSize)));

    return bufSize / sizeof(T);
}

static inline uint32_t divRoundUp(uint32_t x, uint32_t y)
{
    return x == 0 ? 0 : (((x - 1) / y) + 1);
}

void FillFloatControlsProps(vk::VkPhysicalDeviceFloatControlsProperties *props, uint32_t bitDepth, RoundingMode m,
                            DenormMode d, bool useSZInfNan)
{
    assert(bitDepth == 16 || bitDepth == 32 || bitDepth == 64);
    if (bitDepth == 16)
    {
        props->shaderRoundingModeRTEFloat16 = (m == ROUND_RTE);
        props->shaderRoundingModeRTZFloat16 = (m == ROUND_RTZ);

        props->shaderDenormPreserveFloat16    = (d == DENORM_PRESERVE);
        props->shaderDenormFlushToZeroFloat16 = (d == DENORM_FLUSH);

        props->shaderSignedZeroInfNanPreserveFloat16 = useSZInfNan;
    }
    else if (bitDepth == 32)
    {
        props->shaderRoundingModeRTEFloat32 = (m == ROUND_RTE);
        props->shaderRoundingModeRTZFloat32 = (m == ROUND_RTZ);

        props->shaderDenormPreserveFloat32    = (d == DENORM_PRESERVE);
        props->shaderDenormFlushToZeroFloat32 = (d == DENORM_FLUSH);

        props->shaderSignedZeroInfNanPreserveFloat32 = useSZInfNan;
    }
    else
    {
        props->shaderRoundingModeRTEFloat64 = (m == ROUND_RTE);
        props->shaderRoundingModeRTZFloat64 = (m == ROUND_RTZ);

        props->shaderDenormPreserveFloat64    = (d == DENORM_PRESERVE);
        props->shaderDenormFlushToZeroFloat64 = (d == DENORM_FLUSH);

        props->shaderSignedZeroInfNanPreserveFloat64 = useSZInfNan;
    }
}

ComputeShaderSpec createFmaTestSpec(uint32_t bitDepth, uint32_t vecSz, RoundingMode m, DenormMode d, bool useSZInfNan,
                                    InputMode inputMode)
{
    assert(bitDepth == 16 || bitDepth == 32 || bitDepth == 64);

    ComputeShaderSpec spec;
    spec.assembly = getFmaCode(bitDepth, vecSz, m, d, useSZInfNan);

    spec.requestedVulkanFeatures.extFma.shaderFmaFloat16 = (bitDepth == 16);
    spec.requestedVulkanFeatures.extFma.shaderFmaFloat32 = (bitDepth == 32);
    spec.requestedVulkanFeatures.extFma.shaderFmaFloat64 = (bitDepth == 64);

    if (bitDepth == 16)
        spec.requestedVulkanFeatures.extFloat16Int8.shaderFloat16 = VK_TRUE;
    if (bitDepth == 64)
        spec.requestedVulkanFeatures.coreFeatures.shaderFloat64 = VK_TRUE;

    FillFloatControlsProps(&spec.requestedVulkanFeatures.floatControlsProperties, bitDepth, m, d, useSZInfNan);

    size_t numElements;
    if (bitDepth == 16)
        numElements = addInputOutputBuffers<deFloat16>(spec, inputMode, vecSz);
    else if (bitDepth == 32)
        numElements = addInputOutputBuffers<float>(spec, inputMode, vecSz);
    else
        numElements = addInputOutputBuffers<double>(spec, inputMode, vecSz);

    assert(numElements % vecSz == 0);
    assert(numElements <= UINT32_MAX);
    const uint32_t numThreads = uint32_t(numElements / vecSz);

    assert(numThreads <= 65536 || (numThreads & 65535) == 0);
    unsigned x = std::min(numThreads, 65536u);
    unsigned y = divRoundUp(numThreads, 65536u);

    spec.numWorkGroups = IVec3(x, y, 1);
    spec.failResult    = QP_TEST_RESULT_FAIL;
    spec.failMessage   = "Output doesn't match with expected";

    if (bitDepth == 16)
    {
        switch (m)
        {
        case ROUND_UNDEF:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_UNDEF, DENORM_PRESERVE, true> :
                                              verify<deFloat16, ROUND_UNDEF, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_UNDEF, DENORM_FLUSH, true> :
                                              verify<deFloat16, ROUND_UNDEF, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_UNDEF, DENORM_UNDEF, true> :
                                              verify<deFloat16, ROUND_UNDEF, DENORM_UNDEF, false>;
                break;
            }
            break;
        case ROUND_RTE:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_RTE, DENORM_PRESERVE, true> :
                                              verify<deFloat16, ROUND_RTE, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_RTE, DENORM_FLUSH, true> :
                                              verify<deFloat16, ROUND_RTE, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_RTE, DENORM_UNDEF, true> :
                                              verify<deFloat16, ROUND_RTE, DENORM_UNDEF, false>;
                break;
            }
            break;
        case ROUND_RTZ:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_RTZ, DENORM_PRESERVE, true> :
                                              verify<deFloat16, ROUND_RTZ, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_RTZ, DENORM_FLUSH, true> :
                                              verify<deFloat16, ROUND_RTZ, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<deFloat16, ROUND_RTZ, DENORM_UNDEF, true> :
                                              verify<deFloat16, ROUND_RTZ, DENORM_UNDEF, false>;
                break;
            }
            break;
        }
    }
    else if (bitDepth == 32)
    {
        switch (m)
        {
        case ROUND_UNDEF:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_UNDEF, DENORM_PRESERVE, true> :
                                              verify<float, ROUND_UNDEF, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_UNDEF, DENORM_FLUSH, true> :
                                              verify<float, ROUND_UNDEF, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_UNDEF, DENORM_UNDEF, true> :
                                              verify<float, ROUND_UNDEF, DENORM_UNDEF, false>;
                break;
            }
            break;
        case ROUND_RTE:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_RTE, DENORM_PRESERVE, true> :
                                              verify<float, ROUND_RTE, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_RTE, DENORM_FLUSH, true> :
                                              verify<float, ROUND_RTE, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_RTE, DENORM_UNDEF, true> :
                                              verify<float, ROUND_RTE, DENORM_UNDEF, false>;
                break;
            }
            break;
        case ROUND_RTZ:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_RTZ, DENORM_PRESERVE, true> :
                                              verify<float, ROUND_RTZ, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_RTZ, DENORM_FLUSH, true> :
                                              verify<float, ROUND_RTZ, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<float, ROUND_RTZ, DENORM_UNDEF, true> :
                                              verify<float, ROUND_RTZ, DENORM_UNDEF, false>;
                break;
            }
            break;
        }
    }
    else
    {
        switch (m)
        {
        case ROUND_UNDEF:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_UNDEF, DENORM_PRESERVE, true> :
                                              verify<double, ROUND_UNDEF, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_UNDEF, DENORM_FLUSH, true> :
                                              verify<double, ROUND_UNDEF, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_UNDEF, DENORM_UNDEF, true> :
                                              verify<double, ROUND_UNDEF, DENORM_UNDEF, false>;
                break;
            }
            break;
        case ROUND_RTE:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_RTE, DENORM_PRESERVE, true> :
                                              verify<double, ROUND_RTE, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_RTE, DENORM_FLUSH, true> :
                                              verify<double, ROUND_RTE, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_RTE, DENORM_UNDEF, true> :
                                              verify<double, ROUND_RTE, DENORM_UNDEF, false>;
                break;
            }
            break;
        case ROUND_RTZ:
            switch (d)
            {
            case DENORM_PRESERVE:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_RTZ, DENORM_PRESERVE, true> :
                                              verify<double, ROUND_RTZ, DENORM_PRESERVE, false>;
                break;
            case DENORM_FLUSH:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_RTZ, DENORM_FLUSH, true> :
                                              verify<double, ROUND_RTZ, DENORM_FLUSH, false>;
                break;
            case DENORM_UNDEF:
                spec.verifyIO = useSZInfNan ? verify<double, ROUND_RTZ, DENORM_UNDEF, true> :
                                              verify<double, ROUND_RTZ, DENORM_UNDEF, false>;
                break;
            }
            break;
        }
    }

    return spec;
}

} // namespace

tcu::TestCaseGroup *createOpFmaComputeGroup(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "opfma"));

    std::vector<RoundingMode> roundingTests = {ROUND_RTZ, ROUND_RTE, ROUND_UNDEF};
    std::vector<DenormMode> denormTests     = {DENORM_PRESERVE, DENORM_FLUSH, DENORM_UNDEF};

    for (uint32_t bitDepth : {16, 32, 64})
    {
        de::MovePtr<tcu::TestCaseGroup> bitsGroup(
            new tcu::TestCaseGroup(testCtx, (std::string("fp") + std::to_string(bitDepth)).c_str()));

        for (uint32_t vecSz : {1, 2, 3, 4})
        {
            de::MovePtr<tcu::TestCaseGroup> vecGroup(new tcu::TestCaseGroup(
                testCtx, vecSz == 1 ? "scalar" : (std::string("vec") + std::to_string(vecSz)).c_str()));

            for (RoundingMode m : roundingTests)
            {
                de::MovePtr<tcu::TestCaseGroup> roundingGroup(
                    new tcu::TestCaseGroup(testCtx, RoundingModeToNameString(m).c_str()));

                for (DenormMode d : denormTests)
                {
                    de::MovePtr<tcu::TestCaseGroup> denormGroup(
                        new tcu::TestCaseGroup(testCtx, DenormModeToNameString(d).c_str()));

                    denormGroup->addChild(new SpvAsmComputeShaderCase(
                        testCtx, "random", createFmaTestSpec(bitDepth, vecSz, m, d, false, INPUTS_RANDOM)));
                    denormGroup->addChild(new SpvAsmComputeShaderCase(
                        testCtx, "directed", createFmaTestSpec(bitDepth, vecSz, m, d, false, INPUTS_DIRECTED)));
                    denormGroup->addChild(new SpvAsmComputeShaderCase(
                        testCtx, "float_controls", createFmaTestSpec(bitDepth, vecSz, m, d, true, INPUTS_DIRECTED)));

                    roundingGroup->addChild(denormGroup.release());
                }

                vecGroup->addChild(roundingGroup.release());
            }

            bitsGroup->addChild(vecGroup.release());
        }

        group->addChild(bitsGroup.release());
    }

    return group.release();
}

} // namespace SpirVAssembly
} // namespace vkt
