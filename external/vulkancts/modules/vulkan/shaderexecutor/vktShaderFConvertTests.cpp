/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Valve Corporation.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief OpFConvert tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderFConvertTests.hpp"
#include "vktTestCase.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkPrograms.hpp"

#include "deDefs.hpp"
#include "deRandom.hpp"

#include "tcuFloat.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuStringTemplate.hpp"

#include <vector>
#include <iterator>
#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>
#include <string>
#include <limits>
#include <cassert>
#include <type_traits>
#include <optional>
#include <cmath>

namespace vkt
{
namespace shaderexecutor
{

namespace
{

constexpr uint32_t kRandomSeed                          = 0xdeadbeef;
constexpr size_t kRandomSourcesPerType                  = 240;
constexpr size_t kMinVectorLength                       = 1;
constexpr size_t kMaxVectorLength                       = 4;
constexpr size_t kArrayAlignment                        = 16;              // Bytes.
constexpr size_t kEffectiveLength[kMaxVectorLength + 1] = {0, 1, 2, 4, 4}; // Effective length of a vector of size i.
constexpr size_t kGCFNumFloats = 12; // Greatest Common Factor of the number of floats in a test.

//#define SIMULATE_BRAIN_FLOAT16
#ifdef SIMULATE_BRAIN_FLOAT16
struct SimulateBrainFloat16 : tcu::Float16
{
    SimulateBrainFloat16()
    {
    }
    SimulateBrainFloat16(const tcu::Float16 &other) : tcu::Float16(other)
    {
    }
};
using BFloat16 = SimulateBrainFloat16;
#else
using BFloat16 = tcu::BrainFloat16;
#endif

constexpr bool isBFloat16SameBrainFloat16 = std::is_same_v<BFloat16, tcu::BrainFloat16>;

// Get a random normal number.
// Works for implementations of tcu::Float as T.
template <class T>
T getRandomNormal(de::Random &rnd)
{
    static constexpr typename T::StorageType kLeadingMantissaBit =
        (static_cast<typename T::StorageType>(1) << T::MANTISSA_BITS);
    static constexpr int kSignValues[] = {-1, 1};

    int signBit  = rnd.getInt(0, 1);
    int exponent = rnd.getInt(1 - T::EXPONENT_BIAS, T::EXPONENT_BIAS + 1);
    typename T::StorageType mantissa =
        static_cast<typename T::StorageType>(rnd.getUint64() & static_cast<uint64_t>(kLeadingMantissaBit - 1));

    // Construct number.
    return T::construct(kSignValues[signBit], exponent, (kLeadingMantissaBit | mantissa));
}

// Get a list of hand-picked interesting samples for tcu::Float class T.
template <class T>
const std::vector<T> &interestingSamples()
{
    static const std::vector<T> samples = {
        T::zero(-1),
        T::zero(1),
        //T::inf                (-1),
        //T::inf                ( 1),
        //T::nan                (  ),
        T::largestNormal(-1),
        T::largestNormal(1),
        T::smallestNormal(-1),
        T::smallestNormal(1),
    };

    return samples;
}

// Get some random interesting numbers.
// Works for implementations of tcu::Float as T.
template <class T>
std::vector<T> getRandomInteresting(de::Random &rnd, size_t numSamples)
{
    auto &samples = interestingSamples<T>();
    std::vector<T> result;

    result.reserve(numSamples);
    std::generate_n(std::back_inserter(result), numSamples,
                    [&rnd, &samples]() { return rnd.choose<T>(begin(samples), end(samples)); });

    return result;
}

template <class T>
std::vector<T> getExhaustive()
{
    std::vector<T> result;

    uint32_t storageBits = 8 * sizeof(typename T::StorageType);
    if (storageBits <= 16)
    {
        for (uint32_t i = 0; i < (1u << storageBits); ++i)
        {
            result.push_back(T(typename T::StorageType(i)));
        }
    }

    return result;
}

// Helper class to build each vector only once in a thread-safe way.
template <class T>
struct StaticVectorHelper
{
    std::vector<T> v;

    StaticVectorHelper(de::Random &rnd)
    {
        v.reserve(kRandomSourcesPerType);
        for (size_t i = 0; i < kRandomSourcesPerType; ++i)
            v.push_back(getRandomNormal<T>(rnd));
    }
};

// Get a list of random normal input values for type T.
template <class T>
const std::vector<T> &getRandomNormals(de::Random &rnd)
{
    static StaticVectorHelper<T> helper(rnd);
    return helper.v;
}

// Convert a vector of tcu::Float elements of type T1 to type T2.
template <class T1, class T2>
std::vector<T2> convertVector(const std::vector<T1> &orig)
{
    std::vector<T2> result;
    result.reserve(orig.size());

    std::transform(begin(orig), end(orig), std::back_inserter(result), [](const T1 &f) { return T2::convert(f); });

    return result;
}

// Get converted normal values for other tcu::Float types smaller than T, which should be exact conversions when converting back to
// those types.
template <class T>
std::vector<T> getOtherNormals(de::Random &rnd);

#ifndef CTS_USES_VULKANSC
template <>
std::vector<tcu::FloatE5M2> getOtherNormals<tcu::FloatE5M2>(de::Random &)
{
    // Nothing below tcu::FloatE5M2.
    return std::vector<tcu::FloatE5M2>();
}

template <>
std::vector<tcu::FloatE4M3> getOtherNormals<tcu::FloatE4M3>(de::Random &)
{
    // Nothing below tcu::FloatE4M3.
    return std::vector<tcu::FloatE4M3>();
}
#endif

template <>
std::vector<tcu::Float16> getOtherNormals<tcu::Float16>(de::Random &rnd)
{
#ifndef CTS_USES_VULKANSC
    auto v1       = convertVector<tcu::FloatE5M2, tcu::Float16>(getRandomNormals<tcu::FloatE5M2>(rnd));
    const auto v2 = convertVector<tcu::FloatE4M3, tcu::Float16>(getRandomNormals<tcu::FloatE4M3>(rnd));

    v1.reserve(v1.size() + v2.size());
    std::copy(v2.begin(), v2.end(), std::back_inserter(v1));
    return v1;
#else
    DE_UNREF(rnd);
    return {};
#endif
}

#ifndef CTS_USES_VULKANSC
template <>
std::vector<BFloat16> getOtherNormals(de::Random &rnd)
{
    auto v1       = convertVector<tcu::FloatE5M2, BFloat16>(getRandomNormals<tcu::FloatE5M2>(rnd));
    const auto v2 = convertVector<tcu::FloatE4M3, BFloat16>(getRandomNormals<tcu::FloatE4M3>(rnd));

    v1.reserve(v1.size() + v2.size());
    std::copy(v2.begin(), v2.end(), std::back_inserter(v1));
    return v1;
}
#endif

template <>
std::vector<tcu::Float32> getOtherNormals<tcu::Float32>(de::Random &rnd)
{
    // The ones from tcu::Float16 and BrainFloat16
    auto v1 = convertVector<tcu::Float16, tcu::Float32>(getRandomNormals<tcu::Float16>(rnd));
#ifndef CTS_USES_VULKANSC
    const auto v2 = convertVector<BFloat16, tcu::Float32>(getRandomNormals<BFloat16>(rnd));
    const auto v3 = convertVector<tcu::FloatE5M2, tcu::Float32>(getRandomNormals<tcu::FloatE5M2>(rnd));
    const auto v4 = convertVector<tcu::FloatE4M3, tcu::Float32>(getRandomNormals<tcu::FloatE4M3>(rnd));
#else
    const std::vector<tcu::Float32> v2;
    const std::vector<tcu::Float32> v3;
    const std::vector<tcu::Float32> v4;
#endif

    v1.reserve(v1.size() + v2.size() + v3.size() + v4.size());
    std::copy(v2.begin(), v2.end(), std::back_inserter(v1));
    std::copy(v3.begin(), v3.end(), std::back_inserter(v1));
    std::copy(v4.begin(), v4.end(), std::back_inserter(v1));
    return v1;
}

template <>
std::vector<tcu::Float64> getOtherNormals<tcu::Float64>(de::Random &rnd)
{
    // The ones from BrainFloat16, tcu::Float16 and tcu::Float32.
    auto v1       = convertVector<tcu::Float16, tcu::Float64>(getRandomNormals<tcu::Float16>(rnd));
    const auto v2 = convertVector<tcu::Float32, tcu::Float64>(getRandomNormals<tcu::Float32>(rnd));
#ifndef CTS_USES_VULKANSC
    const auto v3 = convertVector<BFloat16, tcu::Float64>(getRandomNormals<BFloat16>(rnd));
    const auto v4 = convertVector<tcu::FloatE5M2, tcu::Float64>(getRandomNormals<tcu::FloatE5M2>(rnd));
    const auto v5 = convertVector<tcu::FloatE4M3, tcu::Float64>(getRandomNormals<tcu::FloatE4M3>(rnd));
#else
    const std::vector<tcu::Float64> v3;
    const std::vector<tcu::Float64> v4;
    const std::vector<tcu::Float64> v5;
#endif

    v1.reserve(v1.size() + v2.size() + v3.size() + v4.size() + v5.size());
    std::copy(v2.begin(), v2.end(), std::back_inserter(v1));
    std::copy(v3.begin(), v3.end(), std::back_inserter(v1));
    std::copy(v4.begin(), v4.end(), std::back_inserter(v1));
    std::copy(v5.begin(), v5.end(), std::back_inserter(v1));
    return v1;
}

// Get the full list of input values for type T.
template <class T>
std::vector<T> getInputValues(de::Random &rnd)
{
    auto &interesting = interestingSamples<T>();
    auto &normals     = getRandomNormals<T>(rnd);
    auto otherNormals = getOtherNormals<T>(rnd);
    auto exhaustive   = getExhaustive<T>();

    const size_t numValues   = interesting.size() + normals.size() + otherNormals.size() + exhaustive.size();
    const size_t extraValues = numValues % kGCFNumFloats;
    const size_t needed      = ((extraValues == 0) ? 0 : (kGCFNumFloats - extraValues));

    auto extra = getRandomInteresting<T>(rnd, needed);

    std::vector<T> values;
    values.reserve(interesting.size() + normals.size() + otherNormals.size() + exhaustive.size() + extra.size());

    std::copy(begin(interesting), end(interesting), std::back_inserter(values));
    std::copy(begin(normals), end(normals), std::back_inserter(values));
    std::copy(begin(otherNormals), end(otherNormals), std::back_inserter(values));
    std::copy(begin(exhaustive), end(exhaustive), std::back_inserter(values));
    std::copy(begin(extra), end(extra), std::back_inserter(values));

    // Shuffle samples around a bit to make it more interesting.
    rnd.shuffle(begin(values), end(values));

    return values;
}

// This singleton makes sure generated samples are stable no matter the test order.
class InputGenerator
{
public:
    static const InputGenerator &getInstance()
    {
        static InputGenerator instance;
        return instance;
    }

    template <class X>
    const std::vector<X> &getValues() const
    {
        DE_ASSERT(false);
        return {};
    }

private:
    InputGenerator()
        : m_rnd(kRandomSeed)
        , m_values16(getInputValues<tcu::Float16>(m_rnd))
        , m_values32(getInputValues<tcu::Float32>(m_rnd))
        , m_values64(getInputValues<tcu::Float64>(m_rnd))
#ifndef CTS_USES_VULKANSC
        , m_bFloatValues(getInputValues<BFloat16>(m_rnd))
        , m_valuesE5M2(getInputValues<tcu::FloatE5M2>(m_rnd))
        , m_valuesE4M3(getInputValues<tcu::FloatE4M3>(m_rnd))
#endif
    {
    }

    // Cannot copy or assign.
    InputGenerator(const InputGenerator &)            = delete;
    InputGenerator &operator=(const InputGenerator &) = delete;

    de::Random m_rnd;
    std::vector<tcu::Float16> m_values16;
    std::vector<tcu::Float32> m_values32;
    std::vector<tcu::Float64> m_values64;
#ifndef CTS_USES_VULKANSC
    std::vector<BFloat16> m_bFloatValues;
    std::vector<tcu::FloatE5M2> m_valuesE5M2;
    std::vector<tcu::FloatE4M3> m_valuesE4M3;
#endif
};

template <>
const std::vector<tcu::Float16> &InputGenerator::getValues() const
{
    return m_values16;
}

template <>
const std::vector<tcu::Float32> &InputGenerator::getValues() const
{
    return m_values32;
}

template <>
const std::vector<tcu::Float64> &InputGenerator::getValues() const
{
    return m_values64;
}
#ifndef CTS_USES_VULKANSC
template <>
const std::vector<BFloat16> &InputGenerator::getValues() const
{
    return m_bFloatValues;
}
template <>
const std::vector<tcu::FloatE5M2> &InputGenerator::getValues() const
{
    return m_valuesE5M2;
}
template <>
const std::vector<tcu::FloatE4M3> &InputGenerator::getValues() const
{
    return m_valuesE4M3;
}
#endif

// Check single result is as expected.
// Works for implementations of tcu::Float as T1 and T2.
template <class T1, class T2>
bool validConversion(const T1 &orig, const T2 &result, bool sat)
{
    const T2 acceptedResults[] = {T2::convert(orig, tcu::ROUND_DOWNWARD), T2::convert(orig, tcu::ROUND_UPWARD)};
    bool valid                 = false;

    for (const auto &validResult : acceptedResults)
    {
        if (validResult.isNaN() && result.isNaN())
            valid = true;
        else if (!sat && validResult.isInf() && result.isInf())
            valid = true;
        else if (validResult.isZero() && result.isZero())
            valid = true;
        // XXX This line should not include "result.isDenorm() ||" and is hiding a bug in tcu::Float denorm handling
        else if (validResult.isDenorm() && (result.isDenorm() || result.isZero()))
            valid = true;
        // handle denorms being flushed
        else if (orig.isDenorm() && result.isZero())
            valid = true;
        else if (validResult.bits() == result.bits() && !(sat && result.isInf())) // Exact conversion, up or down.
            valid = true;
        else if (sat && fabs(orig.asFloat()) > T2::largestNormal(1).asFloat() && orig.sign() == result.sign() &&
                 result.asFloat() == T2::largestNormal(orig.sign()).asFloat())
            valid = true;
    }

    return valid;
}

// Check results vector is as expected.
template <class TIn, class TOut>
bool validConversion(const std::vector<TIn> &orig, const std::vector<TOut> &converted, tcu::TestLog &log, bool sat)
{
    DE_ASSERT(orig.size() == converted.size());

    bool allValid = true;

    for (size_t i = 0; i < orig.size(); ++i)
    {
        const bool valid = validConversion(orig[i], converted[i], sat);

        {
            double origD(-1.0);
            double convD(origD * -1.0);
            std::ostringstream msg;

#ifndef CTS_USES_VULKANSC
            if constexpr (std::is_same_v<TIn, BFloat16>)
            {
                origD = double(orig[i].asFloat());
            }
            else
#endif
            {
                origD = orig[i].asDouble();
            }

#ifndef CTS_USES_VULKANSC
            if constexpr (std::is_same_v<TOut, BFloat16>)
            {
                convD = double(converted[i].asFloat());
            }
            else
#endif
            {
                convD = converted[i].asDouble();
            }

            if (!valid)
            {
                msg << "[" << i << "] " << std::setprecision(std::numeric_limits<double>::digits10 + 2)
                    << std::scientific << origD << " converted to " << convD << ": " << (valid ? "OK" : "FAILURE");

                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            }
        }

        if (!valid)
            allValid = false;
    }

    return allValid;
}

// Helps calculate buffer sizes and other parameters for the given number of values and vector length using a given floating point
// type. This is mostly used in packFloats() below, but we also need this information in the iterate() method for the test instance,
// so it has been separated.
struct BufferSizeInfo
{
    template <class T>
    static BufferSizeInfo calculate(size_t numValues_, size_t vectorLength_)
    {
        // The vector length must be a known number.
        DE_ASSERT(vectorLength_ >= kMinVectorLength && vectorLength_ <= kMaxVectorLength);
        // The number of values must be appropriate for the vector length.
        DE_ASSERT(numValues_ % vectorLength_ == 0);

        BufferSizeInfo info;

        info.numValues    = numValues_;
        info.vectorLength = vectorLength_;
        info.totalVectors = numValues_ / vectorLength_;

        const size_t elementSize     = sizeof(typename T::StorageType);
        const size_t effectiveLength = kEffectiveLength[vectorLength_];
        const size_t vectorSize      = elementSize * effectiveLength;
        const size_t extraBytes      = vectorSize % kArrayAlignment;

        info.vectorStrideBytes = vectorSize + ((extraBytes == 0) ? 0 : (kArrayAlignment - extraBytes));
        info.memorySizeBytes   = info.vectorStrideBytes * info.totalVectors;

        return info;
    }

    size_t numValues;
    size_t vectorLength;
    size_t totalVectors;
    size_t vectorStrideBytes;
    size_t memorySizeBytes;
};

// Pack an array of tcu::Float values into a buffer to be read from a shader, as if it was an array of vectors with each vector
// having size vectorLength (e.g. 3 for a vec3). Note: assumes std140.
template <class T>
std::vector<uint8_t> packFloats(const std::vector<T> &values, size_t vectorLength)
{
    BufferSizeInfo sizeInfo = BufferSizeInfo::calculate<T>(values.size(), vectorLength);

    std::vector<uint8_t> memory(sizeInfo.memorySizeBytes);
    for (size_t i = 0; i < sizeInfo.totalVectors; ++i)
    {
        T *vectorPtr = reinterpret_cast<T *>(memory.data() + sizeInfo.vectorStrideBytes * i);
        for (size_t j = 0; j < vectorLength; ++j)
            vectorPtr[j] = values[i * vectorLength + j];
    }

    return memory;
}

// Unpack an array of vectors into an array of values, undoing what packFloats would do.
// expectedNumValues is used for verification.
template <class T>
std::vector<T> unpackFloats(const std::vector<uint8_t> &memory, size_t vectorLength, size_t expectedNumValues)
{
    DE_ASSERT(vectorLength >= kMinVectorLength && vectorLength <= kMaxVectorLength);

    const size_t effectiveLength = kEffectiveLength[vectorLength];
    const size_t elementSize     = sizeof(typename T::StorageType);
    const size_t vectorSize      = elementSize * effectiveLength;
    const size_t extraBytes      = vectorSize % kArrayAlignment;
    const size_t vectorBlockSize = vectorSize + ((extraBytes == 0) ? 0 : (kArrayAlignment - extraBytes));

    DE_ASSERT(memory.size() % vectorBlockSize == 0);
    const size_t numStoredVectors = memory.size() / vectorBlockSize;
    const size_t numStoredValues  = numStoredVectors * vectorLength;

    DE_UNREF(expectedNumValues); // For release builds.
    DE_ASSERT(numStoredValues == expectedNumValues);
    std::vector<T> values;
    values.reserve(numStoredValues);

    for (size_t i = 0; i < numStoredVectors; ++i)
    {
        const T *vectorPtr = reinterpret_cast<const T *>(memory.data() + vectorBlockSize * i);
        for (size_t j = 0; j < vectorLength; ++j)
            values.push_back(vectorPtr[j]);
    }

    return values;
}

enum FloatType
{
    FLOAT_TYPE_16_BITS = 0,
    FLOAT_TYPE_32_BITS,
    FLOAT_TYPE_64_BITS,
#ifndef CTS_USES_VULKANSC
    BRAIN_FLOAT_16_BITS,
    FLOAT_TYPE_E5M2,
    FLOAT_TYPE_E4M3,
#endif
    FLOAT_TYPE_MAX_ENUM,
};

template <class>
inline constexpr FloatType FloatTypeToEnum = FLOAT_TYPE_MAX_ENUM;
#ifndef CTS_USES_VULKANSC
template <>
inline constexpr FloatType FloatTypeToEnum<BFloat16> = BRAIN_FLOAT_16_BITS;
template <>
inline constexpr FloatType FloatTypeToEnum<tcu::FloatE5M2> = FLOAT_TYPE_E5M2;
template <>
inline constexpr FloatType FloatTypeToEnum<tcu::FloatE4M3> = FLOAT_TYPE_E4M3;
#endif
template <>
inline constexpr FloatType FloatTypeToEnum<tcu::Float16> = FLOAT_TYPE_16_BITS;
template <>
inline constexpr FloatType FloatTypeToEnum<tcu::Float32> = FLOAT_TYPE_32_BITS;
template <>
inline constexpr FloatType FloatTypeToEnum<tcu::Float64> = FLOAT_TYPE_64_BITS;

static const char *const kFloatNames[FLOAT_TYPE_MAX_ENUM] = {
    "f16",  "f32",   "f64",
#ifndef CTS_USES_VULKANSC
    "bf16", "fe5m2", "fe4m3",
#endif
};

template <class, uint32_t>
std::bad_typeid vtn;
template <>
[[maybe_unused]] constexpr const char *vtn<BFloat16, 4> = isBFloat16SameBrainFloat16 ? "bf16vec4" : "f16vec4";
template <>
[[maybe_unused]] constexpr const char *vtn<BFloat16, 3> = isBFloat16SameBrainFloat16 ? "bf16vec3" : "f16vec3";
template <>
[[maybe_unused]] constexpr const char *vtn<BFloat16, 2> = isBFloat16SameBrainFloat16 ? "bf16vec2" : "f16vec2";
template <>
[[maybe_unused]] constexpr const char *vtn<BFloat16, 1> = isBFloat16SameBrainFloat16 ? "bfloat16_t" : "float16_t";

struct TestParams
{
    static constexpr const char *kGLSLTypes[][kMaxVectorLength + 1] = {
        {nullptr, "float16_t", "f16vec2", "f16vec3", "f16vec4"},
        {nullptr, "float", "vec2", "vec3", "vec4"},
        {nullptr, "double", "dvec2", "dvec3", "dvec4"},
#ifndef CTS_USES_VULKANSC
        {nullptr, vtn<BFloat16, 1>, vtn<BFloat16, 2>, vtn<BFloat16, 3>, vtn<BFloat16, 4>},
        {nullptr, "floate5m2_t", "fe5m2vec2", "fe5m2vec3", "fe5m2vec4"},
        {nullptr, "floate4m3_t", "fe4m3vec2", "fe4m3vec3", "fe4m3vec4"},
#endif
    };

    FloatType from;
    FloatType to;
    size_t vectorLength;
    bool saturatedConvert;

    void validate() const
    {
        DE_ASSERT(from != to);
        DE_ASSERT(from >= 0 && from < FLOAT_TYPE_MAX_ENUM);
        DE_ASSERT(vectorLength >= kMinVectorLength && vectorLength <= kMaxVectorLength);
    }

    std::string getInputTypeStr() const
    {
        validate();
        return kGLSLTypes[from][vectorLength];
    }

    std::string getOutputTypeStr() const
    {
        validate();
        return kGLSLTypes[to][vectorLength];
    }

    bool usesBFloat16() const
    {
#ifdef CTS_USES_VULKANSC
        return false;
#else
        return (from == BRAIN_FLOAT_16_BITS) || (to == BRAIN_FLOAT_16_BITS);
#endif
    }

    bool usesFloat16Types() const
    {
        bool ok = (from == FLOAT_TYPE_16_BITS) || (to == FLOAT_TYPE_16_BITS);
#ifndef CTS_USES_VULKANSC
        ok |= usesBFloat16();
#endif
        return ok;
    }

    bool usesFP8() const
    {
#ifdef CTS_USES_VULKANSC
        return false;
#else
        return (from == FLOAT_TYPE_E5M2) || (to == FLOAT_TYPE_E5M2) || (from == FLOAT_TYPE_E4M3) ||
               (to == FLOAT_TYPE_E4M3);
#endif
    }

    static bool isConversionDoable(FloatType from, FloatType to)
    {
        DE_UNREF(from);
        DE_UNREF(to);
        return true;
    }
};

template <class TIn, class Tuple, std::size_t... Is>
void verifyConversionTo(const std::vector<TIn> &inputValues, //
                        FloatType outType,                   //
                        const std::vector<uint8_t> &memory,  //
                        size_t vectorLength,                 //
                        size_t expectedNumValues,            //
                        bool sat,                            //
                        tcu::TestLog &testLog,               //
                        const Tuple &,                       //
                        std::index_sequence<Is...>)
{
    (...,
     [&]
     {
         using TOut = std::tuple_element_t<Is, Tuple>;
         if (FloatTypeToEnum<TOut> == outType)
         {
             const auto outputValues = unpackFloats<TOut>(memory, vectorLength, expectedNumValues);
             const bool conversionOk = validConversion(inputValues, outputValues, testLog, sat);
             throw std::optional(conversionOk);
         }
     }());
    throw std::nullopt;
}

template <class Tuple, std::size_t... Is>
void verifyConversionFrom(FloatType inType,                   //
                          FloatType outType,                  //
                          const std::vector<uint8_t> &memory, //
                          size_t vectorLength,                //
                          size_t expectedNumValues,           //
                          bool sat,                           //
                          tcu::TestLog &testLog,              //
                          const Tuple &t,                     //
                          std::index_sequence<Is...>)
{
    (...,
     [&]
     {
         using TIn = std::tuple_element_t<Is, Tuple>;
         if (FloatTypeToEnum<TIn> == inType)
         {
             const auto &inputValues = InputGenerator::getInstance().getValues<TIn>();
             verifyConversionTo(inputValues, outType, memory, vectorLength, expectedNumValues, sat, testLog, t,
                                std::index_sequence<Is...>{});
         }
     }());
    throw std::nullopt;
}

template <class... U>
std::optional<bool> verifyConversion(FloatType fromType,                 //
                                     FloatType toType,                   //
                                     const std::tuple<U...> available,   //
                                     const std::vector<uint8_t> &memory, //
                                     size_t vectorLength,                //
                                     size_t expectedNumValues,           //
                                     bool sat,                           //
                                     tcu::TestLog &log)
{
    try
    {
        verifyConversionFrom(fromType, toType, memory, vectorLength, expectedNumValues, sat, log, available,
                             std::index_sequence_for<U...>{});
    }
    catch (const std::optional<bool> &result)
    {
        return result;
    }
    return std::nullopt;
}

class FConvertTestInstance : public TestInstance
{
public:
    FConvertTestInstance(Context &context, const TestParams &params) : TestInstance(context), m_params(params)
    {
    }

    virtual tcu::TestStatus iterate(void);

private:
    TestParams m_params;
};

class FConvertTestCase : public TestCase
{
public:
    FConvertTestCase(tcu::TestContext &context, const std::string &name, const TestParams &params)
        : TestCase(context, name)
        , m_params(params)
    {
    }

    ~FConvertTestCase(void)
    {
    }
    virtual TestInstance *createInstance(Context &context) const
    {
        return new FConvertTestInstance(context, m_params);
    }
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual void checkSupport(Context &context) const;

private:
    TestParams m_params;
};

void FConvertTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const std::string inputType          = m_params.getInputTypeStr();
    const std::string outputType         = m_params.getOutputTypeStr();
    const InputGenerator &inputGenerator = InputGenerator::getInstance();

    size_t numValues = 0;
    switch (m_params.from)
    {
#ifndef CTS_USES_VULKANSC
    case BRAIN_FLOAT_16_BITS:
        numValues = inputGenerator.getValues<BFloat16>().size();
        break;
    case FLOAT_TYPE_E5M2:
        numValues = inputGenerator.getValues<tcu::FloatE5M2>().size();
        break;
    case FLOAT_TYPE_E4M3:
        numValues = inputGenerator.getValues<tcu::FloatE4M3>().size();
        break;
#endif
    case FLOAT_TYPE_16_BITS:
        numValues = inputGenerator.getValues<tcu::Float16>().size();
        break;
    case FLOAT_TYPE_32_BITS:
        numValues = inputGenerator.getValues<tcu::Float32>().size();
        break;
    case FLOAT_TYPE_64_BITS:
        numValues = inputGenerator.getValues<tcu::Float64>().size();
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    const size_t arraySize = numValues / m_params.vectorLength;

    std::ostringstream glslShader;

    auto s = [&](const auto &...x) -> void
    {
        ((glslShader << x), ...);
        glslShader << std::endl;
    };

    s("#version 450 core");
    if (m_params.usesFloat16Types())
    {
        s("#extension GL_EXT_shader_16bit_storage: require"); // This is needed to use 16-bit float types in buffers.
        s("#extension GL_EXT_shader_explicit_arithmetic_types: require"); // This is needed for some conversions.
    }
    if (m_params.usesBFloat16())
    {
        s("#extension GL_EXT_bfloat16: require"); // This is needed for bfloat16 type.
    }
    if (m_params.usesFP8())
    {
        s("#extension GL_EXT_float_e4m3 : enable");
        s("#extension GL_EXT_float_e5m2 : enable");
    }
    s("layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;");
    s("layout(set = 0, binding = 0, std140) buffer issbodef { ", inputType, " val[", arraySize, "]; } issbo;");
    s("layout(set = 0, binding = 1, std140) buffer ossbodef { ", outputType, " val[", arraySize, "]; } ossbo;");
    s("void main()");
    s("{");
    if (m_params.saturatedConvert)
    {
        s("    saturatedConvertEXT(ossbo.val[gl_WorkGroupID.x], issbo.val[gl_WorkGroupID.x]);");
    }
    else
    {
        s("    ossbo.val[gl_WorkGroupID.x] = ", outputType, "(issbo.val[gl_WorkGroupID.x]);");
    }
    s("}");

    programCollection.glslSources.add("comp") << glu::ComputeSource(glslShader.str());
}

void FConvertTestCase::checkSupport(Context &context) const
{
    if (m_params.from == FLOAT_TYPE_64_BITS || m_params.to == FLOAT_TYPE_64_BITS)
    {
        // Check for 64-bit float support.
        auto features = context.getDeviceFeatures();
        if (!features.shaderFloat64)
            TCU_THROW(NotSupportedError, "64-bit floats not supported in shader code");
    }

    if (m_params.from == FLOAT_TYPE_16_BITS || m_params.to == FLOAT_TYPE_16_BITS)
    {
        // Check for 16-bit float support.
        auto &features16 = context.getShaderFloat16Int8Features();
        if (!features16.shaderFloat16)
            TCU_THROW(NotSupportedError, "16-bit floats not supported in shader code");

        auto &storage16 = context.get16BitStorageFeatures();
        if (!storage16.storageBuffer16BitAccess)
            TCU_THROW(NotSupportedError, "16-bit floats not supported for storage buffers");
    }

    if (m_params.usesBFloat16())
    {
#ifdef CTS_USES_VULKANSC
        TCU_THROW(NotSupportedError, "VK_KHR_shader_bfloat16 not available in VulkanSC");
#else
        if constexpr (isBFloat16SameBrainFloat16)
        {
            auto &bfeatures16 = context.getShaderBfloat16Features();
            if (!bfeatures16.shaderBFloat16Type)
                TCU_THROW(NotSupportedError, VK_KHR_SHADER_BFLOAT16_EXTENSION_NAME " not supported by device");
        }
#endif
    }
    if (m_params.usesFP8())
    {
#ifdef CTS_USES_VULKANSC
        TCU_THROW(NotSupportedError, "VK_EXT_shader_float8 not available in VulkanSC");
#else
        auto &features8 = context.getShaderFloat8FeaturesEXT();
        if (!features8.shaderFloat8)
            TCU_THROW(NotSupportedError, VK_EXT_SHADER_FLOAT8_EXTENSION_NAME " not supported by device");
#endif
    }
}

tcu::TestStatus FConvertTestInstance::iterate(void)
{
    BufferSizeInfo inputBufferSizeInfo;
    BufferSizeInfo outputBufferSizeInfo;
    std::vector<uint8_t> inputMemory;

    m_params.validate();

    if (m_params.from == FLOAT_TYPE_16_BITS)
    {
        auto &inputValues   = InputGenerator::getInstance().getValues<tcu::Float16>();
        inputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float16>(inputValues.size(), m_params.vectorLength);
        inputMemory         = packFloats(inputValues, m_params.vectorLength);
    }
    else if (m_params.from == FLOAT_TYPE_32_BITS)
    {
        auto &inputValues   = InputGenerator::getInstance().getValues<tcu::Float32>();
        inputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float32>(inputValues.size(), m_params.vectorLength);
        inputMemory         = packFloats(inputValues, m_params.vectorLength);
    }
#ifndef CTS_USES_VULKANSC
    else if (m_params.from == BRAIN_FLOAT_16_BITS)
    {
        auto &inputValues   = InputGenerator::getInstance().getValues<BFloat16>();
        inputBufferSizeInfo = BufferSizeInfo::calculate<BFloat16>(inputValues.size(), m_params.vectorLength);
        inputMemory         = packFloats(inputValues, m_params.vectorLength);
    }
    else if (m_params.from == FLOAT_TYPE_E5M2)
    {
        auto &inputValues   = InputGenerator::getInstance().getValues<tcu::FloatE5M2>();
        inputBufferSizeInfo = BufferSizeInfo::calculate<tcu::FloatE5M2>(inputValues.size(), m_params.vectorLength);
        inputMemory         = packFloats(inputValues, m_params.vectorLength);
    }
    else if (m_params.from == FLOAT_TYPE_E4M3)
    {
        auto &inputValues   = InputGenerator::getInstance().getValues<tcu::FloatE4M3>();
        inputBufferSizeInfo = BufferSizeInfo::calculate<tcu::FloatE4M3>(inputValues.size(), m_params.vectorLength);
        inputMemory         = packFloats(inputValues, m_params.vectorLength);
    }
#endif
    else
    {
        auto &inputValues   = InputGenerator::getInstance().getValues<tcu::Float64>();
        inputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float64>(inputValues.size(), m_params.vectorLength);
        inputMemory         = packFloats(inputValues, m_params.vectorLength);
    }

    switch (m_params.to)
    {
#ifndef CTS_USES_VULKANSC
    case BRAIN_FLOAT_16_BITS:
        outputBufferSizeInfo =
            BufferSizeInfo::calculate<BFloat16>(inputBufferSizeInfo.numValues, m_params.vectorLength);
        break;
    case FLOAT_TYPE_E5M2:
        outputBufferSizeInfo =
            BufferSizeInfo::calculate<tcu::FloatE5M2>(inputBufferSizeInfo.numValues, m_params.vectorLength);
        break;
    case FLOAT_TYPE_E4M3:
        outputBufferSizeInfo =
            BufferSizeInfo::calculate<tcu::FloatE4M3>(inputBufferSizeInfo.numValues, m_params.vectorLength);
        break;
#endif
    case FLOAT_TYPE_16_BITS:
        outputBufferSizeInfo =
            BufferSizeInfo::calculate<tcu::Float16>(inputBufferSizeInfo.numValues, m_params.vectorLength);
        break;
    case FLOAT_TYPE_32_BITS:
        outputBufferSizeInfo =
            BufferSizeInfo::calculate<tcu::Float32>(inputBufferSizeInfo.numValues, m_params.vectorLength);
        break;
    case FLOAT_TYPE_64_BITS:
        outputBufferSizeInfo =
            BufferSizeInfo::calculate<tcu::Float64>(inputBufferSizeInfo.numValues, m_params.vectorLength);
        break;
    default:
        assert(false);
        break;
    }

    // Prepare input and output buffers.
    auto &vkd       = m_context.getDeviceInterface();
    auto device     = m_context.getDevice();
    auto &allocator = m_context.getDefaultAllocator();

    de::MovePtr<vk::BufferWithMemory> inputBuffer(new vk::BufferWithMemory(
        vkd, device, allocator,
        vk::makeBufferCreateInfo(inputBufferSizeInfo.memorySizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        vk::MemoryRequirement::HostVisible));

    de::MovePtr<vk::BufferWithMemory> outputBuffer(new vk::BufferWithMemory(
        vkd, device, allocator,
        vk::makeBufferCreateInfo(outputBufferSizeInfo.memorySizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        vk::MemoryRequirement::HostVisible));

    // Copy values to input buffer.
    {
        auto &alloc = inputBuffer->getAllocation();
        deMemcpy(reinterpret_cast<uint8_t *>(alloc.getHostPtr()) + alloc.getOffset(), inputMemory.data(),
                 inputMemory.size());
        vk::flushAlloc(vkd, device, alloc);
    }

    // Create an array with the input and output buffers to make it easier to iterate below.
    const vk::VkBuffer buffers[] = {inputBuffer->get(), outputBuffer->get()};

    // Create descriptor set layout.
    std::vector<vk::VkDescriptorSetLayoutBinding> bindings;
    for (int i = 0; i < DE_LENGTH_OF_ARRAY(buffers); ++i)
    {
        const vk::VkDescriptorSetLayoutBinding binding = {
            static_cast<uint32_t>(i),              // uint32_t              binding;
            vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // VkDescriptorType      descriptorType;
            1u,                                    // uint32_t              descriptorCount;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,       // VkShaderStageFlags    stageFlags;
            nullptr,                               // const VkSampler*      pImmutableSamplers;
        };
        bindings.push_back(binding);
    }

    const vk::VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
        vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
        nullptr,                                                 // const void*                            pNext;
        0,                                                       // VkDescriptorSetLayoutCreateFlags       flags;
        static_cast<uint32_t>(bindings.size()),                  // uint32_t                               bindingCount;
        bindings.data()                                          // const VkDescriptorSetLayoutBinding*    pBindings;
    };
    auto descriptorSetLayout = vk::createDescriptorSetLayout(vkd, device, &layoutCreateInfo);

    // Create descriptor set.
    vk::DescriptorPoolBuilder poolBuilder;
    for (const auto &b : bindings)
        poolBuilder.addType(b.descriptorType, 1u);
    auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    const vk::VkDescriptorSetAllocateInfo allocateInfo = {
        vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType;
        nullptr,                                            // const void*                     pNext;
        *descriptorPool,                                    // VkDescriptorPool                descriptorPool;
        1u,                                                 // uint32_t                        descriptorSetCount;
        &descriptorSetLayout.get()                          // const VkDescriptorSetLayout*    pSetLayouts;
    };
    auto descriptorSet = vk::allocateDescriptorSet(vkd, device, &allocateInfo);

    // Update descriptor set.
    std::vector<vk::VkDescriptorBufferInfo> descriptorBufferInfos;
    std::vector<vk::VkWriteDescriptorSet> descriptorWrites;

    for (const auto &buffer : buffers)
    {
        const vk::VkDescriptorBufferInfo bufferInfo = {
            buffer,        // VkBuffer        buffer;
            0u,            // VkDeviceSize    offset;
            VK_WHOLE_SIZE, // VkDeviceSize    range;
        };
        descriptorBufferInfos.push_back(bufferInfo);
    }

    for (size_t i = 0; i < bindings.size(); ++i)
    {
        const vk::VkWriteDescriptorSet write = {
            vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                  sType;
            nullptr,                                    // const void*                      pNext;
            *descriptorSet,                             // VkDescriptorSet                  dstSet;
            static_cast<uint32_t>(i),                   // uint32_t                         dstBinding;
            0u,                                         // uint32_t                         dstArrayElement;
            1u,                                         // uint32_t                         descriptorCount;
            bindings[i].descriptorType,                 // VkDescriptorType                 descriptorType;
            nullptr,                                    // const VkDescriptorImageInfo*     pImageInfo;
            &descriptorBufferInfos[i],                  // const VkDescriptorBufferInfo*    pBufferInfo;
            nullptr,                                    // const VkBufferView*              pTexelBufferView;
        };
        descriptorWrites.push_back(write);
    }
    vkd.updateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0u,
                             nullptr);

    // Prepare barriers in advance so data is visible to the shaders and the host.
    std::vector<vk::VkBufferMemoryBarrier> hostToDevBarriers;
    std::vector<vk::VkBufferMemoryBarrier> devToHostBarriers;
    for (int i = 0; i < DE_LENGTH_OF_ARRAY(buffers); ++i)
    {
        const vk::VkBufferMemoryBarrier hostToDev = {
            vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                      // VkStructureType sType;
            nullptr,                                                          // const void* pNext;
            vk::VK_ACCESS_HOST_WRITE_BIT,                                     // VkAccessFlags srcAccessMask;
            (vk::VK_ACCESS_SHADER_READ_BIT | vk::VK_ACCESS_SHADER_WRITE_BIT), // VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                                          // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                                          // uint32_t dstQueueFamilyIndex;
            buffers[i],                                                       // VkBuffer buffer;
            0u,                                                               // VkDeviceSize offset;
            VK_WHOLE_SIZE,                                                    // VkDeviceSize size;
        };
        hostToDevBarriers.push_back(hostToDev);

        const vk::VkBufferMemoryBarrier devToHost = {
            vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            vk::VK_ACCESS_SHADER_WRITE_BIT,              // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
            buffers[i],                                  // VkBuffer buffer;
            0u,                                          // VkDeviceSize offset;
            VK_WHOLE_SIZE,                               // VkDeviceSize size;
        };
        devToHostBarriers.push_back(devToHost);
    }

    // Create command pool and command buffer.
    auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const vk::VkCommandPoolCreateInfo cmdPoolCreateInfo = {
        vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,       // VkCommandPoolCreateFlags flags;
        queueFamilyIndex,                               // uint32_t queueFamilyIndex;
    };
    auto cmdPool = vk::createCommandPool(vkd, device, &cmdPoolCreateInfo);

    const vk::VkCommandBufferAllocateInfo cmdBufferAllocateInfo = {
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                            // const void* pNext;
        *cmdPool,                                           // VkCommandPool commandPool;
        vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
        1u,                                                 // uint32_t commandBufferCount;
    };
    auto cmdBuffer = vk::allocateCommandBuffer(vkd, device, &cmdBufferAllocateInfo);

    // Create pipeline layout.
    const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                           // const void* pNext;
        0,                                                 // VkPipelineLayoutCreateFlags flags;
        1u,                                                // uint32_t setLayoutCount;
        &descriptorSetLayout.get(),                        // const VkDescriptorSetLayout* pSetLayouts;
        0u,                                                // uint32_t pushConstantRangeCount;
        nullptr,                                           // const VkPushConstantRange* pPushConstantRanges;
    };
    auto pipelineLayout = vk::createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);

    // Create compute pipeline.
    const vk::Unique<vk::VkShaderModule> shader(
        vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0));

    const vk::VkComputePipelineCreateInfo computeCreateInfo = {
        vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                    sType;
        nullptr,                                            // const void*                        pNext;
        0,                                                  // VkPipelineCreateFlags              flags;
        {
            // VkPipelineShaderStageCreateInfo    stage;
            vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType;
            nullptr,                                                 // const void*                         pNext;
            0,                                                       // VkPipelineShaderStageCreateFlags    flags;
            vk::VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits               stage;
            *shader,                                                 // VkShaderModule                      module;
            "main",                                                  // const char*                         pName;
            nullptr, // const VkSpecializationInfo*         pSpecializationInfo;
        },
        *pipelineLayout, // VkPipelineLayout                   layout;
        VK_NULL_HANDLE,  // VkPipeline                         basePipelineHandle;
        0,               // int32_t                            basePipelineIndex;
    };
    auto computePipeline = vk::createComputePipeline(vkd, device, VK_NULL_HANDLE, &computeCreateInfo);

    // Run the shader.
    vk::beginCommandBuffer(vkd, *cmdBuffer);
    vkd.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vkd.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0u,
                           nullptr, static_cast<uint32_t>(hostToDevBarriers.size()), hostToDevBarriers.data(), 0u,
                           nullptr);
    vkd.cmdDispatch(*cmdBuffer, static_cast<uint32_t>(inputBufferSizeInfo.totalVectors), 1u, 1u);
    vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0, 0u,
                           nullptr, static_cast<uint32_t>(devToHostBarriers.size()), devToHostBarriers.data(), 0u,
                           nullptr);
    vk::endCommandBuffer(vkd, *cmdBuffer);
    vk::submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *cmdBuffer);

    // Invalidate output allocation.
    vk::invalidateAlloc(vkd, device, outputBuffer->getAllocation());

    // Copy output buffer data.
    std::vector<uint8_t> outputMemory(outputBufferSizeInfo.memorySizeBytes);
    {
        auto &alloc = outputBuffer->getAllocation();
        deMemcpy(outputMemory.data(), reinterpret_cast<uint8_t *>(alloc.getHostPtr()) + alloc.getOffset(),
                 outputBufferSizeInfo.memorySizeBytes);
    }

    // Unpack and verify output data.
    auto &testLog = m_context.getTestContext().getLog();

#ifdef CTS_USES_VULKANSC
    const std::tuple<tcu::Float16, tcu::Float32, tcu::Float64>
#else
    const std::tuple<tcu::Float16, tcu::Float32, tcu::Float64, BFloat16, tcu::FloatE5M2, tcu::FloatE4M3>
#endif
        availableTypes;

    const std::optional<bool> res =
        verifyConversion(m_params.from, m_params.to, availableTypes, outputMemory, m_params.vectorLength,
                         inputBufferSizeInfo.numValues, m_params.saturatedConvert, testLog);
    if (false == res.has_value())
    {
        // this should never happen
        TCU_THROW(NotSupportedError, "No types to perform test");
    }

    return (*res ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail"));
}

} // namespace

tcu::TestCaseGroup *createPrecisionFconvertGroup(tcu::TestContext &testCtx)
{
    tcu::TestCaseGroup *newGroup = new tcu::TestCaseGroup(testCtx, "precision_fconvert");

    const FloatType floatTypes[]{
#ifdef CTS_USES_VULKANSC
        FLOAT_TYPE_16_BITS, FLOAT_TYPE_32_BITS, FLOAT_TYPE_64_BITS
#else
        FLOAT_TYPE_E5M2,    FLOAT_TYPE_E4M3,   BRAIN_FLOAT_16_BITS, FLOAT_TYPE_16_BITS,
        FLOAT_TYPE_32_BITS, FLOAT_TYPE_64_BITS
#endif
    };

    for (const FloatType from : floatTypes)
        for (const FloatType to : floatTypes)
            for (size_t k = kMinVectorLength; k <= kMaxVectorLength; ++k)
                // XXX TODO Do we need any floatcontrols stuff for inf/nan testing?
                for (bool sat : {false, true})
                {
                    // No actual conversion if the types are the same.
                    if (from == to)
                        continue;

                    // Skip unimplemened conversion.
                    if (false == TestParams::isConversionDoable(from, to))
                    {
                        continue;
                    }

                    if (sat &&
#ifdef CTS_USES_VULKANSC
                        true
#else
                        ((to != FLOAT_TYPE_E5M2 && to != FLOAT_TYPE_E4M3) ||
                         (from == FLOAT_TYPE_E5M2 || from == FLOAT_TYPE_E4M3))
#endif
                    )
                        continue;

                    TestParams params = {from, to, k, sat};

                    std::string testName = std::string() + kFloatNames[from] + "_to_" + kFloatNames[to] + "_size_" +
                                           std::to_string(k) + (sat ? "_sat" : "");

                    newGroup->addChild(new FConvertTestCase(testCtx, testName, params));
                }

    return newGroup;
}

} // namespace shaderexecutor
} // namespace vkt
