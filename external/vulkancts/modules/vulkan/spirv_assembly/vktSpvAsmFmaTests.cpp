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

static bool isValid(float a, float b)
{
    return deFloatBitsToUint32(a) == deFloatBitsToUint32(b) || (std::isnan(a) && std::isnan(b));
}

static bool isValid(double a, double b)
{
    return deDoubleBitsToUint64(a) == deDoubleBitsToUint64(b) || (std::isnan(a) && std::isnan(b));
}

template <typename T>
static bool isDenorm(T f)
{
    return std::fpclassify(f) == FP_SUBNORMAL;
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
        valid.push_back(0.0f);
        valid.push_back(-0.0f);
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
static vector<T> getRefValues(T a, T b, T c, RoundingMode m, DenormMode d, bool signedZero)
{
    vector<T> ret;

    // We will change the rounding mode, so save the current one and restore it later.
    int rm = fegetround();

    vector<int> allowedRoundingModes;
    switch (m)
    {
    case ROUND_RTZ:
        allowedRoundingModes.push_back(FE_TOWARDZERO);
        break;
    case ROUND_RTE:
        allowedRoundingModes.push_back(FE_TONEAREST);
        break;
    case ROUND_UNDEF:
        allowedRoundingModes.push_back(FE_UPWARD);
        allowedRoundingModes.push_back(FE_DOWNWARD);
        break;
    }

    // Multiple inputs may be valid if denorms are flushed, so get the complete set
    vector<std::array<T, 3>> allowedInputs = getAllowedInputs(a, b, c, d);

    // For each allowed input vector, calculate all valid results
    for (int mode : allowedRoundingModes)
    {
        for (auto inp : allowedInputs)
        {
            fesetround(mode);
            T r = std::fma(inp[0], inp[1], inp[2]);

            // Calculate variants rounded upward and downward for underflow detection.
            // (RTZ and the rounded result would do here for detecting the largest
            // denorms, but because 0.0 is not denormal we also need to check that both
            // values have not rounded down to 0.0. A tiny denorm that is flushed may be
            // more permissive with the sign of zero than an actual zero result).
            fesetround(FE_DOWNWARD);
            T rDown = std::fma(inp[0], inp[1], inp[2]);
            fesetround(FE_UPWARD);
            T rUp = std::fma(inp[0], inp[1], inp[2]);

            bool underflowAfterRounding  = isDenorm(r);
            bool underflowBeforeRounding = isDenorm(rUp) || isDenorm(rDown);

            // underflowAfterRounding => underflowBeforeRounding.
            assert(underflowBeforeRounding || !underflowAfterRounding);

            // If denorms are allowed to be preserved or if this might not have underflowed
            // (because of rounding) then the CPU-generated correctly-rounded result is allowed.
            if (d != DENORM_FLUSH || !underflowAfterRounding)
                ret.push_back(r);
            // If denorms are allowed to be flushed and this might have underflowed then flushing
            // is allowed.
            if (d != DENORM_PRESERVE && underflowBeforeRounding)
            {
                // Vulkan allows denorms to be flushed to either +/- 0.0f
                ret.push_back(0.0f);
                ret.push_back(-0.0f);
            }
            if (r == 0.0f && !signedZero)
                ret.push_back(-r);
        }
    }

    // Restore the mode we recorded before beginning.
    fesetround(rm);
    return ret;
}

template <typename T>
static bool verifyResult(const std::vector<Resource> &inputs, const std::vector<AllocationSp> &outputAllocations,
                         RoundingMode m, DenormMode d, bool signedZero, tcu::TestLog &log)
{
    vector<uint8_t> aBytes, bBytes, cBytes;
    inputs[0].getBuffer()->getBytes(aBytes);
    inputs[1].getBuffer()->getBytes(bBytes);
    inputs[2].getBuffer()->getBytes(cBytes);

    const T *a = reinterpret_cast<T *>(aBytes.data());
    const T *b = reinterpret_cast<T *>(bBytes.data());
    const T *c = reinterpret_cast<T *>(cBytes.data());

    const size_t count = aBytes.size() / sizeof(T);

    const T *res = reinterpret_cast<T *>(outputAllocations[0]->getHostPtr());

    const size_t errorsMax = 16u;
    size_t errors          = 0u;

    for (size_t ndx = 0; ndx < count; ++ndx)
    {
        vector<T> refValues = getRefValues(a[ndx], b[ndx], c[ndx], m, d, signedZero);

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

template <typename T, RoundingMode m, DenormMode d, bool signedZero>
static bool verify(const std::vector<Resource> &inputs, const std::vector<AllocationSp> &outputAllocations,
                   const std::vector<Resource> &expectedOutputs, tcu::TestLog &log)
{
    (void)expectedOutputs;
    return verifyResult<T>(inputs, outputAllocations, m, d, signedZero, log);
}

enum InputMode
{
    INPUTS_RANDOM,
    INPUTS_DIRECTED
};

template <typename T>
void FillInputsRandom(vector<T> &a, vector<T> &b, vector<T> &c)
{
    de::Random rnd(deStringHash("fma.random_inputs"));

    const int numElements = 768;

    a.resize(numElements);
    b.resize(numElements);
    c.resize(numElements);

    for (int ndx = 0; ndx < numElements; ndx++)
    {
        a[ndx] = de::randomScalar<T>(rnd, 0.001f, 10000.0f);
        b[ndx] = de::randomScalar<T>(rnd, 0.001f, 10000.0f);
        c[ndx] = de::randomScalar<T>(rnd, 0.001f, 10000.0f);
    }
}

template <typename T>
bool IsInfNan(T x)
{
    return (std::isinf(x) || std::isnan(x));
}

template <typename T>
void AddDirectedCase(vector<std::array<T, 3>> &cases, T a, T b, T c, RoundingMode m, DenormMode d, bool useSZInfNan)
{
    bool usesInfNan    = IsInfNan(a) || IsInfNan(b) || IsInfNan(c);
    bool usesPlusZero  = false;
    bool usesMinusZero = false;

    // Get the reference values treating signedZero as important
    vector<T> ref = getRefValues(a, b, c, m, d, true);
    for (T r : ref)
    {
        usesInfNan = usesInfNan || IsInfNan(r);
        if (r == 0.0f && std::signbit(r))
            usesMinusZero = true;
        if (r == 0.0f && !std::signbit(r))
            usesPlusZero = true;
    }

    // If there are no inf/nan then the relaxed case will be correct, so add that.
    if (!useSZInfNan && !usesInfNan)
        cases.push_back({a, b, c});

    // If there are inf/nan, or if the sign of zero is significant in the result then add the strict case
    // as well. Note that this doesn't check any non-zero reference values, so would not be correct for
    // operations where the sign of zero can affect any non-zero results (eg. recip, etc).
    if (useSZInfNan && (usesInfNan || (usesPlusZero != usesMinusZero)))
        cases.push_back({a, b, c});
}

template <typename T>
vector<T> GetSpecialValues();

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
void FillInputsDirected(vector<T> &a, vector<T> &b, vector<T> &c, uint32_t vecSz, RoundingMode m, DenormMode d,
                        bool useSZInfNan)
{
    vector<T> values;
    for (T f : GetSpecialValues<T>())
    {
        values.push_back(f);
        values.push_back(-f);
    }

    vector<std::array<T, 3>> cases;

    // Test all combinations of the special values, dividing them according to whether we're
    // creating a test using signed-zero-inf-nan-preserve or not. We create one test with the
    // feature and one without, so divide the values up according to whether the feature is
    // required to get correct results or not. This means every combination gets tested in one
    // test and every implementation runs as many cases as it says it supports.
    for (T inA : values)
        for (T inB : values)
            for (T inC : values)
                AddDirectedCase(cases, inA, inB, inC, m, d, useSZInfNan);

    // Add cancellation cases (of the form a * b - (a*b)), which should give non-zero results
    // with FMA, returning the rounding error in calculating a*b. We add at least a minumum
    // number (because they're a good test of fma), but we also use this to round up to a valid
    // work size (ie. a multiple of 65536), which we need in order to be able to launch all the
    // work in a single 2D dispatch. Because of this rounding up, we add these cases to both
    // tests regardless of useSZInfNan.
    size_t minCancellationCases = 100;
    size_t numCancellationCases = minCancellationCases;
    if ((cases.size() + numCancellationCases) % vecSz != 0)
        numCancellationCases += (vecSz - ((cases.size() + numCancellationCases) % vecSz));

    if ((cases.size() + minCancellationCases) / vecSz > 65536)
        numCancellationCases += vecSz * 65536 - ((cases.size() + numCancellationCases) % (vecSz * 65536));

    de::Random rnd(deStringHash("fma.directed_inputs_cancellation"));
    for (unsigned i = 0; i < numCancellationCases; i++)
    {
        T inA = de::randomScalar<T>(rnd, 0.0001f, 1000.0f);
        T inB = de::randomScalar<T>(rnd, 0.0001f, 1000.0f);
        cases.push_back({inA, inB, -(inA * inB)});
    }

    a.resize(cases.size());
    b.resize(cases.size());
    c.resize(cases.size());

    for (size_t i = 0; i < cases.size(); i++)
    {
        a[i] = cases[i][0];
        b[i] = cases[i][1];
        c[i] = cases[i][2];
    }
}

template <typename T>
void FillInputs(vector<T> &a, vector<T> &b, vector<T> &c, InputMode mode, uint32_t vecSz, RoundingMode m, DenormMode d,
                bool useSZInfNan)
{
    assert(mode == INPUTS_RANDOM || mode == INPUTS_DIRECTED);
    // Don't try to create random tests for SZInfNan because we don't generate those values anyway
    assert(mode != INPUTS_RANDOM || !useSZInfNan);

    if (mode == INPUTS_RANDOM)
        FillInputsRandom(a, b, c);
    else
        FillInputsDirected(a, b, c, vecSz, m, d, useSZInfNan);
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

template <typename T>
size_t addInputOutputBuffers(ComputeShaderSpec &spec, InputMode inputMode, uint32_t vecSz, RoundingMode m, DenormMode d,
                             bool useSZInfNan)
{
    vector<T> inputs1, inputs2, inputs3;

    FillInputs(inputs1, inputs2, inputs3, inputMode, vecSz, m, d, useSZInfNan);

    // A buffer must be provided for the outputs (probably?) but they're not going to be used, so nothing
    // is filled in here. The reference value is computed from the inputs in the verification function.
    vector<T> outputs(inputs1.size(), 0);

    spec.inputs.push_back(BufferSp(new Buffer<T>(inputs1)));
    spec.inputs.push_back(BufferSp(new Buffer<T>(inputs2)));
    spec.inputs.push_back(BufferSp(new Buffer<T>(inputs3)));
    spec.outputs.push_back(BufferSp(new Buffer<T>(outputs)));

    return inputs1.size();
}

ComputeShaderSpec createFmaTestSpec(uint32_t bitDepth, uint32_t vecSz, RoundingMode m, DenormMode d, bool useSZInfNan,
                                    InputMode inputMode)
{
    assert(bitDepth == 32 || bitDepth == 64);

    ComputeShaderSpec spec;
    spec.assembly = getFmaCode(bitDepth, vecSz, m, d, useSZInfNan);

    spec.requestedVulkanFeatures.extFma.shaderFmaFloat32 = (bitDepth == 32);
    spec.requestedVulkanFeatures.extFma.shaderFmaFloat64 = (bitDepth == 64);

    if (bitDepth == 64)
        spec.requestedVulkanFeatures.coreFeatures.shaderFloat64 = VK_TRUE;

    FillFloatControlsProps(&spec.requestedVulkanFeatures.floatControlsProperties, bitDepth, m, d, useSZInfNan);

    size_t numElements;
    if (bitDepth == 32)
        numElements = addInputOutputBuffers<float>(spec, inputMode, vecSz, m, d, useSZInfNan);
    else
        numElements = addInputOutputBuffers<double>(spec, inputMode, vecSz, m, d, useSZInfNan);

    assert(numElements % vecSz == 0);
    assert(numElements <= UINT32_MAX);
    const uint32_t numThreads = uint32_t(numElements / vecSz);

    assert(numThreads <= 65536 || (numThreads & 65535) == 0);
    unsigned x = std::min(numThreads, 65536u);
    unsigned y = divRoundUp(numThreads, 65536u);

    spec.numWorkGroups = IVec3(x, y, 1);
    spec.failResult    = QP_TEST_RESULT_FAIL;
    spec.failMessage   = "Output doesn't match with expected";

    if (bitDepth == 32)
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

    for (uint32_t bitDepth : {32, 64})
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
