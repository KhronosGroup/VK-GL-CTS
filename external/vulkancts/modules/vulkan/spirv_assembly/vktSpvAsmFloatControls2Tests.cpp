/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief VK_KHR_shader_float_controls2 tests.
 *//*--------------------------------------------------------------------*/

#define _USE_MATH_DEFINES

#include "vktSpvAsmFloatControlsTests.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmGraphicsShaderTestUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuStringTemplate.hpp"
#include "deUniquePtr.hpp"
#include "deFloat16.h"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "spirv/unified1/spirv.hpp11"
#include <cstring>
#include <vector>
#include <limits>
#include <cstdint>
#include <fenv.h>
#include <cmath>
#include <cassert>

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

using namespace std;
using namespace tcu;

enum FloatType
{
    FP16 = 0,
    FP32,
    FP64
};

enum class BufferDataType
{
    DATA_UNKNOWN = 0,
    DATA_FP16    = 1,
    DATA_FP32    = 2,
    DATA_FP64    = 3,
};

enum FloatUsage
{
    // If the float type is 16bit, then the use of the type is supported by
    // VK_KHR_16bit_storage.
    FLOAT_STORAGE_ONLY = 0,
    // Use of the float type goes beyond VK_KHR_16bit_storage.
    FLOAT_ARITHMETIC
};

enum FloatStatementUsageBits
{
    B_STATEMENT_USAGE_ARGS_CONST_FLOAT     = (1 << 0),
    B_STATEMENT_USAGE_ARGS_CONST_FP16      = (1 << 1),
    B_STATEMENT_USAGE_ARGS_CONST_FP32      = (1 << 2),
    B_STATEMENT_USAGE_ARGS_CONST_FP64      = (1 << 3),
    B_STATEMENT_USAGE_TYPES_TYPE_FLOAT     = (1 << 4),
    B_STATEMENT_USAGE_TYPES_TYPE_FP16      = (1 << 5),
    B_STATEMENT_USAGE_TYPES_TYPE_FP32      = (1 << 6),
    B_STATEMENT_USAGE_TYPES_TYPE_FP64      = (1 << 7),
    B_STATEMENT_USAGE_CONSTS_TYPE_FLOAT    = (1 << 8),
    B_STATEMENT_USAGE_CONSTS_TYPE_FP16     = (1 << 9),
    B_STATEMENT_USAGE_CONSTS_TYPE_FP32     = (1 << 10),
    B_STATEMENT_USAGE_CONSTS_TYPE_FP64     = (1 << 11),
    B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT = (1 << 12),
    B_STATEMENT_USAGE_COMMANDS_CONST_FP16  = (1 << 13),
    B_STATEMENT_USAGE_COMMANDS_CONST_FP32  = (1 << 14),
    B_STATEMENT_USAGE_COMMANDS_CONST_FP64  = (1 << 15),
    B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT  = (1 << 16),
    B_STATEMENT_USAGE_COMMANDS_TYPE_FP16   = (1 << 17),
    B_STATEMENT_USAGE_COMMANDS_TYPE_FP32   = (1 << 18),
    B_STATEMENT_USAGE_COMMANDS_TYPE_FP64   = (1 << 19),
};

typedef uint32_t FloatStatementUsageFlags;

using FP = spv::FPFastMathModeMask;

typedef map<FP, string> BehaviorNameMap;
BehaviorNameMap behaviorToName = {{FP::MaskNone, "None"},
                                  {FP::NotNaN, "NotNaN"},
                                  {FP::NotInf, "NotInf"},
                                  {FP::NSZ, "NSZ"},
                                  {FP::AllowRecip, "AllowRecip"},
                                  {FP::AllowContract, "AllowContract"},
                                  {FP::AllowReassoc, "AllowReassoc"},
                                  {FP::AllowTransform, "AllowTransform"}};

const FP allBits =
    FP::NotNaN | FP::NotInf | FP::NSZ | FP::AllowRecip | FP::AllowContract | FP::AllowReassoc | FP::AllowTransform;
const FP allBitsExceptTransform = ~FP::AllowTransform & allBits;
FP invert(FP bfb)
{
    // AllowTransform requires AllowReassoc and AllowContract to also be set
    if ((bfb & (FP::AllowReassoc | FP::AllowContract)) != FP::MaskNone)
        return ~bfb & allBitsExceptTransform;
    else
        return ~bfb & allBits;
}

string getBehaviourName(FP flagbits, const char *separator)
{
    string behaviorName = "";
    bool needOrInName   = false;
    if (flagbits == FP::MaskNone)
        behaviorName = "None";
    else
        for (auto it = behaviorToName.begin(); it != behaviorToName.end(); it++)
        {
            if ((it->first & flagbits) != FP::MaskNone)
            {
                if (needOrInName)
                    behaviorName += separator;
                behaviorName += it->second;
                needOrInName = true;
            }
        }
    return behaviorName;
}

// Codes for all float values used in tests as arguments and operation results
enum ValueId
{
    V_UNUSED = 0, // used to mark arguments that are not used in operation
    V_MINUS_INF,  //    or results of tests cases that should be skipped
    V_MINUS_ONE,  // -1.0
    V_MINUS_ZERO, // -0.0
    V_ZERO,       //  0.0
    V_HALF,       //  0.5
    V_ONE,        //  1.0
    V_TWO,
    V_INF,
    V_ZERO_POINT_ONE,
    V_TWENTY_FIVE_POINT_EIGHT,
    V_HUGE, // a large number that if doubled will result in infinity but that is not equal to the maximum
    V_TINY, // a number that if squared will underflow to 0.
    V_MINUS_TINY,
    V_MAX,
    V_NAN,

    // non comon results of some operation - corner cases
    V_PI,
    V_MINUS_PI,
    V_PI_DIV_2,
    V_MINUS_PI_DIV_2,
    V_PI_DIV_4,
    V_MINUS_PI_DIV_4,
    V_3_PI_DIV_4,
    V_MINUS_3_PI_DIV_4,
    V_ONE_OR_NAN,
    V_SIGN_NAN,           // Can be any of -1, -0, +0, +1
    V_ZERO_OR_MINUS_ZERO, // both +0 and -0 are accepted
    V_ZERO_OR_ONE,        // both +0 and 1 are accepted
    V_TRIG_ONE,           // 1.0 trigonometric operations, including precision margin
};

string getValueName(ValueId value)
{
    switch (value)
    {
    case V_UNUSED:
        return "unused";
    case V_MINUS_INF:
        return "minusInf";
    case V_MINUS_ONE:
        return "minusOne";
    case V_MINUS_ZERO:
        return "minusZero";
    case V_ZERO:
        return "zero";
    case V_HALF:
        return "half";
    case V_ONE:
        return "one";
    case V_TWO:
        return "two";
    case V_INF:
        return "inf";
    case V_ZERO_POINT_ONE:
        return "zeroPtOne";
    case V_TWENTY_FIVE_POINT_EIGHT:
        return "twentyFivePtEight";
    case V_HUGE:
        return "huge";
    case V_TINY:
        return "tiny";
    case V_MINUS_TINY:
        return "minusTiny";
    case V_MAX:
        return "max";
    case V_NAN:
        return "nan";
    case V_PI:
        return "pi";
    case V_MINUS_PI:
        return "minusPi";
    case V_PI_DIV_2:
        return "piDiv2";
    case V_MINUS_PI_DIV_2:
        return "minusPiDiv2";
    case V_PI_DIV_4:
        return "piDiv4";
    case V_MINUS_PI_DIV_4:
        return "minusPiDiv4";
    case V_3_PI_DIV_4:
        return "3PiDiv4";
    case V_MINUS_3_PI_DIV_4:
        return "minus3PiDiv4";
    case V_ONE_OR_NAN:
        return "oneORnan";
    case V_SIGN_NAN:
        return "signNan";
    case V_ZERO_OR_MINUS_ZERO:
        return "zeroOrMinusZero";
    case V_ZERO_OR_ONE:
        return "zeroOrOne";
    case V_TRIG_ONE:
        return "trigOne";
    }
    assert(false);
    return "";
}

// Enum containing all tested operatios. Operations are defined in generic way so that
// they can be used to generate tests operating on arguments with different values of
// specified float type.
enum OperationId
{
    // spir-v unary operations
    OID_NEGATE = 0,
    OID_COMPOSITE,
    OID_COMPOSITE_INS,
    OID_COPY,
    OID_D_EXTRACT,
    OID_D_INSERT,
    OID_SHUFFLE,
    OID_TRANSPOSE,
    OID_CONV_FROM_FP16,
    OID_CONV_FROM_FP32,
    OID_CONV_FROM_FP64,
    OID_RETURN_VAL,

    // spir-v binary operations
    OID_ADD,
    OID_SUB,
    OID_MUL,
    OID_DIV,
    OID_REM,
    OID_MOD,
    OID_PHI,
    OID_SELECT,
    OID_DOT,
    OID_VEC_MUL_S,
    OID_VEC_MUL_M,
    OID_MAT_MUL_S,
    OID_MAT_MUL_V,
    OID_MAT_MUL_M,
    OID_OUT_PROD,
    OID_ORD_EQ,
    OID_UORD_EQ,
    OID_ORD_NEQ,
    OID_UORD_NEQ,
    OID_ORD_LS,
    OID_UORD_LS,
    OID_ORD_GT,
    OID_UORD_GT,
    OID_ORD_LE,
    OID_UORD_LE,
    OID_ORD_GE,
    OID_UORD_GE,

    // glsl unary operations
    OID_ROUND,
    OID_ROUND_EV,
    OID_TRUNC,
    OID_ABS,
    OID_SIGN,
    OID_FLOOR,
    OID_CEIL,
    OID_FRACT,
    OID_RADIANS,
    OID_DEGREES,
    OID_SIN,
    OID_COS,
    OID_TAN,
    OID_ASIN,
    OID_ACOS,
    OID_ATAN,
    OID_SINH,
    OID_COSH,
    OID_TANH,
    OID_ASINH,
    OID_ACOSH,
    OID_ATANH,
    OID_EXP,
    OID_LOG,
    OID_EXP2,
    OID_LOG2,
    OID_SQRT,
    OID_INV_SQRT,
    OID_MODF,
    OID_MODF_ST_WH, // Whole number part of modf
    OID_MODF_ST_FR, // Fractional part of modf
    OID_LDEXP,
    OID_FREXP,
    OID_FREXP_ST,
    OID_LENGTH,
    OID_NORMALIZE,
    OID_REFLECT,
    OID_REFRACT,
    OID_MAT_DET,
    OID_MAT_INV,

    // glsl binary operations
    OID_ATAN2,
    OID_POW,
    OID_MIX,
    OID_FMA,
    OID_FMA2PT58,
    OID_SZ_FMA,
    OID_MIN,
    OID_MAX,
    OID_CLAMP,
    OID_STEP,
    OID_SSTEP,
    OID_DIST,
    OID_CROSS,
    OID_FACE_FWD,
    OID_NMIN,
    OID_NMAX,
    OID_NCLAMP,

    OID_ADD_SUB_REASSOCIABLE,
};

// Function replacing all occurrences of substring with string passed in last parameter.
string replace(string str, const string &from, const string &to)
{
    // to keep spir-v code clean and easier to read parts of it are processed
    // with this method instead of StringTemplate; main usage of this method is the
    // replacement of "float_" with "f16_", "f32_" or "f64_" depending on test case

    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

// Structure used to perform bits conversion int type <-> float type.
template <typename FLOAT_TYPE, typename UINT_TYPE>
struct RawConvert
{
    union Value
    {
        FLOAT_TYPE fp;
        UINT_TYPE ui;
    };
};

// Traits used to get int type that can store equivalent float type.
template <typename FLOAT_TYPE>
struct GetCoresponding
{
    typedef uint16_t uint_type;
};
template <>
struct GetCoresponding<float>
{
    typedef uint32_t uint_type;
};
template <>
struct GetCoresponding<double>
{
    typedef uint64_t uint_type;
};

// All values used for arguments and operation results are stored in single map.
// Each float type (fp16, fp32, fp64) has its own map that is used during
// test setup and during verification. TypeValuesBase is interface to that map.
class TypeValuesBase
{
public:
    TypeValuesBase();
    virtual ~TypeValuesBase() = default;

    virtual BufferSp constructInputBuffer(const ValueId *twoArguments) const                                     = 0;
    virtual BufferSp constructOutputBuffer(ValueId result) const                                                 = 0;
    virtual void fillInputData(const ValueId *twoArguments, vector<uint8_t> &bufferData, uint32_t &offset) const = 0;
};

TypeValuesBase::TypeValuesBase()
{
}

typedef de::SharedPtr<TypeValuesBase> TypeValuesSP;

template <typename FLOAT_TYPE>
class TypeValues : public TypeValuesBase
{
public:
    TypeValues();

    BufferSp constructInputBuffer(const ValueId *twoArguments) const override;
    BufferSp constructOutputBuffer(ValueId result) const override;
    void fillInputData(const ValueId *twoArguments, vector<uint8_t> &bufferData, uint32_t &offset) const override;

    FLOAT_TYPE getValue(ValueId id) const;

    template <typename UINT_TYPE>
    FLOAT_TYPE exactByteEquivalent(UINT_TYPE byteValue) const;

private:
    typedef map<ValueId, FLOAT_TYPE> ValueMap;
    ValueMap m_valueIdToFloatType;
};

template <typename FLOAT_TYPE>
BufferSp TypeValues<FLOAT_TYPE>::constructInputBuffer(const ValueId *twoArguments) const
{
    std::vector<FLOAT_TYPE> inputData(2);
    inputData[0] = m_valueIdToFloatType.at(twoArguments[0]);
    inputData[1] = m_valueIdToFloatType.at(twoArguments[1]);
    return BufferSp(new Buffer<FLOAT_TYPE>(inputData));
}

template <typename FLOAT_TYPE>
BufferSp TypeValues<FLOAT_TYPE>::constructOutputBuffer(ValueId result) const
{
    // note: we are not doing maping here, ValueId is directly saved in
    // float type in order to be able to retireve it during verification

    typedef typename GetCoresponding<FLOAT_TYPE>::uint_type uint_t;
    uint_t value = static_cast<uint_t>(result);

    // For FP16 we increase the buffer size to hold an unsigned integer, as
    // we can be in the no 16bit_storage case.
    const uint_t outputSize = sizeof(FLOAT_TYPE) == 2u ? 2u : 1u;
    std::vector<FLOAT_TYPE> outputData(outputSize, exactByteEquivalent<uint_t>(value));
    return BufferSp(new Buffer<FLOAT_TYPE>(outputData));
}

template <typename FLOAT_TYPE>
void TypeValues<FLOAT_TYPE>::fillInputData(const ValueId *twoArguments, vector<uint8_t> &bufferData,
                                           uint32_t &offset) const
{
    uint32_t typeSize = sizeof(FLOAT_TYPE);

    FLOAT_TYPE argA = getValue(twoArguments[0]);
    deMemcpy(&bufferData[offset], &argA, typeSize);
    offset += typeSize;

    FLOAT_TYPE argB = getValue(twoArguments[1]);
    deMemcpy(&bufferData[offset], &argB, typeSize);
    offset += typeSize;
}

template <typename FLOAT_TYPE>
FLOAT_TYPE TypeValues<FLOAT_TYPE>::getValue(ValueId id) const
{
    return m_valueIdToFloatType.at(id);
}

template <typename FLOAT_TYPE>
template <typename UINT_TYPE>
FLOAT_TYPE TypeValues<FLOAT_TYPE>::exactByteEquivalent(UINT_TYPE byteValue) const
{
    typename RawConvert<FLOAT_TYPE, UINT_TYPE>::Value value;
    value.ui = byteValue;
    return value.fp;
}

template <>
TypeValues<deFloat16>::TypeValues() : TypeValuesBase()
{
    // NOTE: when updating entries in m_valueIdToFloatType make sure to
    // update also valueIdToSnippetArgMap defined in updateSpirvSnippets()
    ValueMap &vm                  = m_valueIdToFloatType;
    vm[V_UNUSED]                  = deFloat32To16(0.0f);
    vm[V_MINUS_INF]               = 0xfc00;
    vm[V_MINUS_ONE]               = deFloat32To16(-1.0f);
    vm[V_MINUS_ZERO]              = 0x8000;
    vm[V_ZERO]                    = 0x0000;
    vm[V_HALF]                    = deFloat32To16(0.5f);
    vm[V_ONE]                     = deFloat32To16(1.0f);
    vm[V_TWO]                     = deFloat32To16(2.0f);
    vm[V_ZERO_POINT_ONE]          = deFloat32To16(0.1f);
    vm[V_TWENTY_FIVE_POINT_EIGHT] = deFloat32To16(25.8f);
    vm[V_HUGE]                    = 0x7bfd;
    vm[V_TINY]                    = 0x0400;
    vm[V_MINUS_TINY]              = 0x8400;
    vm[V_MAX]                     = 0x7bff;
    vm[V_INF]                     = 0x7c00;
    vm[V_NAN]                     = 0x7cf0;

    vm[V_PI]               = deFloat32To16((float)M_PI);
    vm[V_MINUS_PI]         = deFloat32To16(-(float)M_PI);
    vm[V_PI_DIV_2]         = deFloat32To16((float)M_PI_2);
    vm[V_MINUS_PI_DIV_2]   = deFloat32To16(-(float)M_PI_2);
    vm[V_PI_DIV_4]         = deFloat32To16((float)M_PI_4);
    vm[V_MINUS_PI_DIV_4]   = deFloat32To16(-(float)M_PI_4);
    vm[V_3_PI_DIV_4]       = deFloat32To16((float)(3 * M_PI_4));
    vm[V_MINUS_3_PI_DIV_4] = deFloat32To16(-(float)(3 * M_PI_4));
}

template <>
TypeValues<float>::TypeValues() : TypeValuesBase()
{
    // NOTE: when updating entries in m_valueIdToFloatType make sure to
    // update also valueIdToSnippetArgMap defined in updateSpirvSnippets()
    ValueMap &vm                  = m_valueIdToFloatType;
    vm[V_UNUSED]                  = 0.0f;
    vm[V_MINUS_INF]               = -std::numeric_limits<float>::infinity();
    vm[V_MINUS_ONE]               = -1.0f;
    vm[V_MINUS_ZERO]              = -0.0f;
    vm[V_ZERO]                    = 0.0f;
    vm[V_HALF]                    = 0.5f;
    vm[V_ONE]                     = 1.0f;
    vm[V_TWO]                     = 2.0f;
    vm[V_ZERO_POINT_ONE]          = 0.1f;
    vm[V_TWENTY_FIVE_POINT_EIGHT] = 25.8f;
    vm[V_HUGE]                    = 3.40282306073709652508e+38;
    vm[V_TINY]                    = 1.17549435082228750797e-38;
    vm[V_MINUS_TINY]              = -1.17549435082228750797e-38;
    vm[V_MAX]                     = std::numeric_limits<float>::max();
    vm[V_INF]                     = std::numeric_limits<float>::infinity();
    vm[V_NAN]                     = std::numeric_limits<float>::quiet_NaN();

    vm[V_PI]               = static_cast<float>(M_PI);
    vm[V_MINUS_PI]         = -static_cast<float>(M_PI);
    vm[V_PI_DIV_2]         = static_cast<float>(M_PI_2);
    vm[V_MINUS_PI_DIV_2]   = -static_cast<float>(M_PI_2);
    vm[V_PI_DIV_4]         = static_cast<float>(M_PI_4);
    vm[V_MINUS_PI_DIV_4]   = -static_cast<float>(M_PI_4);
    vm[V_3_PI_DIV_4]       = static_cast<float>(3 * M_PI_4);
    vm[V_MINUS_3_PI_DIV_4] = -static_cast<float>(3 * M_PI_4);
}

template <>
TypeValues<double>::TypeValues() : TypeValuesBase()
{
    // NOTE: when updating entries in m_valueIdToFloatType make sure to
    // update also valueIdToSnippetArgMap defined in updateSpirvSnippets()
    ValueMap &vm                  = m_valueIdToFloatType;
    vm[V_UNUSED]                  = 0.0;
    vm[V_MINUS_INF]               = -std::numeric_limits<double>::infinity();
    vm[V_MINUS_ONE]               = -1.0;
    vm[V_MINUS_ZERO]              = -0.0;
    vm[V_ZERO]                    = 0.0;
    vm[V_HALF]                    = 0.5;
    vm[V_ONE]                     = 1.0;
    vm[V_TWO]                     = 2.0;
    vm[V_ZERO_POINT_ONE]          = 0.1;
    vm[V_TWENTY_FIVE_POINT_EIGHT] = 25.8;
    vm[V_HUGE]                    = 1.79769313486231530898e+308;
    vm[V_TINY]                    = 2.22507385850720138309e-308;
    vm[V_MINUS_TINY]              = -2.22507385850720138309e-308;
    vm[V_MAX]                     = std::numeric_limits<double>::max();
    vm[V_INF]                     = std::numeric_limits<double>::infinity();
    vm[V_NAN]                     = std::numeric_limits<double>::quiet_NaN();

    vm[V_PI]               = M_PI;
    vm[V_MINUS_PI]         = -M_PI;
    vm[V_PI_DIV_2]         = M_PI_2;
    vm[V_MINUS_PI_DIV_2]   = -M_PI_2;
    vm[V_PI_DIV_4]         = M_PI_4;
    vm[V_MINUS_PI_DIV_4]   = -M_PI_4;
    vm[V_3_PI_DIV_4]       = 3 * M_PI_4;
    vm[V_MINUS_3_PI_DIV_4] = -3 * M_PI_4;
}

// Each float type (fp16, fp32, fp64) has specific set of SPIR-V snippets
// that was extracted to separate template specialization. Those snippets
// are used to compose final test shaders. With this approach
// parameterization can be done just once per type and reused for many tests.
class TypeSnippetsBase
{
public:
    virtual ~TypeSnippetsBase() = default;

protected:
    void updateSpirvSnippets();

public: // Type specific data:
    // Number of bits consumed by float type
    string bitWidth;
    // Minimum positive normal
    string epsilon;
    string capabilities;
    string extensions;
    string capabilitiesFp16Without16BitStorage;
    string extensionsFp16Without16BitStorage;
    string arrayStride;

    bool loadStoreRequiresShaderFloat16;

public: // Type specific spir-v snippets:
    // Common annotations
    string typeAnnotationsSnippet;

    // Definitions of all types commonly used by operation tests
    string typeDefinitionsSnippet;

    // Definitions of all types commonly used by settings tests
    string minTypeDefinitionsSnippet;

    // Definitions of all constants commonly used by tests
    string constantsDefinitionsSnippet;

    // Map that stores instructions that generate arguments of specified value.
    // Every test that uses generated inputod will select up to two items from this map
    typedef map<ValueId, string> SnippetMap;
    SnippetMap valueIdToSnippetArgMap;

    // Spir-v snippets that read argument from SSBO
    string argumentsFromInputSnippet;
    string multiArgumentsFromInputSnippet;

    // SSBO with stage input/output definitions
    string inputAnnotationsSnippet;
    string inputDefinitionsSnippet;
    string outputAnnotationsSnippet;
    string multiOutputAnnotationsSnippet;
    string outputDefinitionsSnippet;
    string multiOutputDefinitionsSnippet;

    // Varying is required to pass result from vertex stage to fragment stage,
    // one of requirements was to not use SSBO writes in vertex stage so we
    // need to do that in fragment stage; we also cant pass operation result
    // directly because of interpolation, to avoid it we do a bitcast to uint
    string varyingsTypesSnippet;
    string inputVaryingsSnippet;
    string outputVaryingsSnippet;
    string storeVertexResultSnippet;
    string loadVertexResultSnippet;

    string storeResultsSnippet;
    string multiStoreResultsSnippet;

    string argumentsFromInputFp16Snippet;
    string storeResultsFp16Snippet;
    string multiArgumentsFromInputFp16Snippet;
    string multiOutputAnnotationsFp16Snippet;
    string multiStoreResultsFp16Snippet;
    string multiOutputDefinitionsFp16Snippet;
    string inputDefinitionsFp16Snippet;
    string outputDefinitionsFp16Snippet;
    string typeAnnotationsFp16Snippet;
    string typeDefinitionsFp16Snippet;
};

void TypeSnippetsBase::updateSpirvSnippets()
{
    // annotations to types that are commonly used by tests
    const string typeAnnotationsTemplate = "OpDecorate %type_float_arr_1 ArrayStride " + arrayStride +
                                           "\n"
                                           "OpDecorate %type_float_arr_2 ArrayStride " +
                                           arrayStride + "\n";

    // definition off all types that are commonly used by tests
    const string typeDefinitionsTemplate = "%type_float             = OpTypeFloat " + bitWidth +
                                           "\n"
                                           "%type_float_uptr        = OpTypePointer Uniform %type_float\n"
                                           "%type_float_fptr        = OpTypePointer Function %type_float\n"
                                           "%type_float_vec2        = OpTypeVector %type_float 2\n"
                                           "%type_float_vec3        = OpTypeVector %type_float 3\n"
                                           "%type_float_vec4        = OpTypeVector %type_float 4\n"
                                           "%type_float_vec4_iptr   = OpTypePointer Input %type_float_vec4\n"
                                           "%type_float_vec4_optr   = OpTypePointer Output %type_float_vec4\n"
                                           "%type_float_mat2x2      = OpTypeMatrix %type_float_vec2 2\n"
                                           "%type_float_arr_1       = OpTypeArray %type_float %c_i32_1\n"
                                           "%type_float_arr_2       = OpTypeArray %type_float %c_i32_2\n";

    // minimal type definition set that is used by settings tests
    const string minTypeDefinitionsTemplate = "%type_float             = OpTypeFloat " + bitWidth +
                                              "\n"
                                              "%type_float_uptr        = OpTypePointer Uniform %type_float\n"
                                              "%type_float_arr_2       = OpTypeArray %type_float %c_i32_2\n";

    // definition off all constants that are used by tests
    const string constantsDefinitionsTemplate = "%c_float_n1             = OpConstant %type_float -1\n"
                                                "%c_float_n2pt58         = OpConstant %type_float -2.58\n"
                                                "%c_float_0              = OpConstant %type_float 0.0\n"
                                                "%c_float_0_5            = OpConstant %type_float 0.5\n"
                                                "%c_float_1              = OpConstant %type_float 1\n"
                                                "%c_float_2              = OpConstant %type_float 2\n"
                                                "%c_float_3              = OpConstant %type_float 3\n"
                                                "%c_float_4              = OpConstant %type_float 4\n"
                                                "%c_float_5              = OpConstant %type_float 5\n"
                                                "%c_float_6              = OpConstant %type_float 6\n"
                                                "%c_float_eps            = OpConstant %type_float " +
                                                epsilon + "\n";

    // when arguments are read from SSBO this snipped is placed in main function
    const string argumentsFromInputTemplate =
        "%arg1loc                = OpAccessChain %type_float_uptr %ssbo_in %c_i32_0 %c_i32_0\n"
        "%arg1                   = OpLoad %type_float %arg1loc\n"
        "%arg2loc                = OpAccessChain %type_float_uptr %ssbo_in %c_i32_0 %c_i32_1\n"
        "%arg2                   = OpLoad %type_float %arg2loc\n";

    const string multiArgumentsFromInputTemplate =
        "%arg1_float_loc         = OpAccessChain %type_float_uptr %ssbo_in %c_i32_${attr} %c_i32_0\n"
        "%arg2_float_loc         = OpAccessChain %type_float_uptr %ssbo_in %c_i32_${attr} %c_i32_1\n"
        "%arg1_float             = OpLoad %type_float %arg1_float_loc\n"
        "%arg2_float             = OpLoad %type_float %arg2_float_loc\n";

    // when tested shader stage reads from SSBO it has to have this snippet
    inputAnnotationsSnippet = "OpMemberDecorate %SSBO_in 0 Offset 0\n"
                              "OpDecorate %SSBO_in BufferBlock\n"
                              "OpDecorate %ssbo_in DescriptorSet 0\n"
                              "OpDecorate %ssbo_in Binding 0\n"
                              "OpDecorate %ssbo_in NonWritable\n";

    const string inputDefinitionsTemplate = "%SSBO_in              = OpTypeStruct %type_float_arr_2\n"
                                            "%up_SSBO_in           = OpTypePointer Uniform %SSBO_in\n"
                                            "%ssbo_in              = OpVariable %up_SSBO_in Uniform\n";

    outputAnnotationsSnippet = "OpMemberDecorate %SSBO_out 0 Offset 0\n"
                               "OpDecorate %SSBO_out BufferBlock\n"
                               "OpDecorate %ssbo_out DescriptorSet 0\n"
                               "OpDecorate %ssbo_out Binding 1\n";

    const string multiOutputAnnotationsTemplate = "OpMemberDecorate %SSBO_float_out 0 Offset 0\n"
                                                  "OpDecorate %type_float_arr_2 ArrayStride " +
                                                  arrayStride +
                                                  "\n"
                                                  "OpDecorate %SSBO_float_out BufferBlock\n"
                                                  "OpDecorate %ssbo_float_out DescriptorSet 0\n";

    const string outputDefinitionsTemplate = "%SSBO_out             = OpTypeStruct %type_float_arr_1\n"
                                             "%up_SSBO_out          = OpTypePointer Uniform %SSBO_out\n"
                                             "%ssbo_out             = OpVariable %up_SSBO_out Uniform\n";

    const string multiOutputDefinitionsTemplate = "%SSBO_float_out         = OpTypeStruct %type_float\n"
                                                  "%up_SSBO_float_out      = OpTypePointer Uniform %SSBO_float_out\n"
                                                  "%ssbo_float_out         = OpVariable %up_SSBO_float_out Uniform\n";

    // this snippet is used by compute and fragment stage but not by vertex stage
    const string storeResultsTemplate =
        "%outloc               = OpAccessChain %type_float_uptr %ssbo_out %c_i32_0 %c_i32_0\n"
        "OpStore %outloc %result\n";

    const string multiStoreResultsTemplate = "%outloc" + bitWidth +
                                             "             = OpAccessChain %type_float_uptr %ssbo_float_out %c_i32_0\n"
                                             "                        OpStore %outloc" +
                                             bitWidth + " %result" + bitWidth + "\n";

    const string typeToken = "_float";
    const string typeName  = "_f" + bitWidth;

    typeAnnotationsSnippet         = replace(typeAnnotationsTemplate, typeToken, typeName);
    typeDefinitionsSnippet         = replace(typeDefinitionsTemplate, typeToken, typeName);
    minTypeDefinitionsSnippet      = replace(minTypeDefinitionsTemplate, typeToken, typeName);
    constantsDefinitionsSnippet    = replace(constantsDefinitionsTemplate, typeToken, typeName);
    argumentsFromInputSnippet      = replace(argumentsFromInputTemplate, typeToken, typeName);
    multiArgumentsFromInputSnippet = replace(multiArgumentsFromInputTemplate, typeToken, typeName);
    inputDefinitionsSnippet        = replace(inputDefinitionsTemplate, typeToken, typeName);
    multiOutputAnnotationsSnippet  = replace(multiOutputAnnotationsTemplate, typeToken, typeName);
    outputDefinitionsSnippet       = replace(outputDefinitionsTemplate, typeToken, typeName);
    multiOutputDefinitionsSnippet  = replace(multiOutputDefinitionsTemplate, typeToken, typeName);
    storeResultsSnippet            = replace(storeResultsTemplate, typeToken, typeName);
    multiStoreResultsSnippet       = replace(multiStoreResultsTemplate, typeToken, typeName);

    argumentsFromInputFp16Snippet      = "";
    storeResultsFp16Snippet            = "";
    multiArgumentsFromInputFp16Snippet = "";
    multiOutputAnnotationsFp16Snippet  = "";
    multiStoreResultsFp16Snippet       = "";
    multiOutputDefinitionsFp16Snippet  = "";
    inputDefinitionsFp16Snippet        = "";
    typeAnnotationsFp16Snippet         = "";
    outputDefinitionsFp16Snippet       = "";
    typeDefinitionsFp16Snippet         = "";

    if (bitWidth.compare("16") == 0)
    {
        typeDefinitionsFp16Snippet = "%type_u32_uptr       = OpTypePointer Uniform %type_u32\n"
                                     "%type_u32_arr_1      = OpTypeArray %type_u32 %c_i32_1\n";

        typeAnnotationsFp16Snippet  = "OpDecorate %type_u32_arr_1 ArrayStride 4\n";
        const string inputToken     = "_f16_arr_2";
        const string inputName      = "_u32_arr_1";
        inputDefinitionsFp16Snippet = replace(inputDefinitionsSnippet, inputToken, inputName);

        argumentsFromInputFp16Snippet = "%argloc            = OpAccessChain %type_u32_uptr %ssbo_in %c_i32_0 %c_i32_0\n"
                                        "%inval             = OpLoad %type_u32 %argloc\n"
                                        "%arg               = OpBitcast %type_f16_vec2 %inval\n"
                                        "%arg1              = OpCompositeExtract %type_f16 %arg 0\n"
                                        "%arg2              = OpCompositeExtract %type_f16 %arg 1\n";

        const string outputToken     = "_f16_arr_1";
        const string outputName      = "_u32_arr_1";
        outputDefinitionsFp16Snippet = replace(outputDefinitionsSnippet, outputToken, outputName);

        storeResultsFp16Snippet = "%result_f16_vec2   = OpCompositeConstruct %type_f16_vec2 %result %c_f16_0\n"
                                  "%result_u32 = OpBitcast %type_u32 %result_f16_vec2\n"
                                  "%outloc            = OpAccessChain %type_u32_uptr %ssbo_out %c_i32_0 %c_i32_0\n"
                                  "OpStore %outloc %result_u32\n";

        multiArgumentsFromInputFp16Snippet =
            "%arg_u32_loc         = OpAccessChain %type_u32_uptr %ssbo_in %c_i32_${attr} %c_i32_0\n"
            "%arg_u32             = OpLoad %type_u32 %arg_u32_loc\n"
            "%arg_f16_vec2        = OpBitcast %type_f16_vec2 %arg_u32\n"
            "%arg1_f16            = OpCompositeExtract %type_f16 %arg_f16_vec2 0\n"
            "%arg2_f16            = OpCompositeExtract %type_f16 %arg_f16_vec2 1\n";

        multiOutputAnnotationsFp16Snippet = "OpMemberDecorate %SSBO_u32_out 0 Offset 0\n"
                                            "OpDecorate %type_u32_arr_1 ArrayStride 4\n"
                                            "OpDecorate %SSBO_u32_out BufferBlock\n"
                                            "OpDecorate %ssbo_u32_out DescriptorSet 0\n";

        multiStoreResultsFp16Snippet = "%outloc_u32            = OpAccessChain %type_u32_uptr %ssbo_u32_out %c_i32_0\n"
                                       "%result16_vec2 = OpCompositeConstruct %type_f16_vec2 %result16 %c_f16_0\n"
                                       "%result_u32            = OpBitcast %type_u32 %result16_vec2\n"
                                       "                        OpStore %outloc_u32 %result_u32\n";

        multiOutputDefinitionsFp16Snippet = "%c_f16_0              = OpConstant %type_f16 0.0\n"
                                            "%SSBO_u32_out         = OpTypeStruct %type_u32\n"
                                            "%up_SSBO_u32_out      = OpTypePointer Uniform %SSBO_u32_out\n"
                                            "%ssbo_u32_out         = OpVariable %up_SSBO_u32_out Uniform\n";
    }

    // NOTE: only values used as _generated_ arguments in test operations
    // need to be in this map, arguments that are only used by tests, ??????? generated vs input
    // that grab arguments from input, do need to be in this map
    // NOTE: when updating entries in valueIdToSnippetArgMap make
    // sure to update also m_valueIdToFloatType for all float width
    SnippetMap &sm   = valueIdToSnippetArgMap;
    sm[V_UNUSED]     = "OpFSub %type_float %c_float_0 %c_float_0\n";
    sm[V_MINUS_INF]  = "OpFDiv %type_float %c_float_n1 %c_float_0\n";
    sm[V_MINUS_ONE]  = "OpFAdd %type_float %c_float_n1 %c_float_0\n";
    sm[V_MINUS_ZERO] = "OpFMul %type_float %c_float_n1 %c_float_0\n";
    sm[V_ZERO]       = "OpFMul %type_float %c_float_0 %c_float_0\n";
    sm[V_HALF]       = "OpFAdd %type_float %c_float_0_5 %c_float_0\n";
    sm[V_ONE]        = "OpFAdd %type_float %c_float_1 %c_float_0\n";
    sm[V_INF]        = "OpFDiv %type_float %c_float_1 %c_float_0\n"; // x / 0 == Inf
    sm[V_NAN]        = "OpFDiv %type_float %c_float_0 %c_float_0\n"; // 0 / 0 == Nan

    map<ValueId, string>::iterator it;
    for (it = sm.begin(); it != sm.end(); it++)
        sm[it->first] = replace(it->second, typeToken, typeName);
}

typedef de::SharedPtr<TypeSnippetsBase> TypeSnippetsSP;

template <typename FLOAT_TYPE>
class TypeSnippets : public TypeSnippetsBase
{
public:
    TypeSnippets();
};

template <>
TypeSnippets<deFloat16>::TypeSnippets()
{
    bitWidth = "16";
    epsilon  = "6.104e-5"; // 2^-14 = 0x0400

    // NOTE: constants in SPIR-V cant be specified as exact fp16 - there is conversion from double to fp16
    capabilities = "OpCapability StorageUniform16\n";
    extensions   = "OpExtension \"SPV_KHR_16bit_storage\"\n";

    capabilitiesFp16Without16BitStorage = "OpCapability Float16\n";
    extensionsFp16Without16BitStorage   = "";

    arrayStride = "2";

    varyingsTypesSnippet     = "%type_u32_iptr        = OpTypePointer Input %type_u32\n"
                               "%type_u32_optr        = OpTypePointer Output %type_u32\n";
    inputVaryingsSnippet     = "%BP_vertex_result    = OpVariable %type_u32_iptr Input\n";
    outputVaryingsSnippet    = "%BP_vertex_result    = OpVariable %type_u32_optr Output\n";
    storeVertexResultSnippet = "%tmp_vec2            = OpCompositeConstruct %type_f16_vec2 %result %c_f16_0\n"
                               "%packed_result       = OpBitcast %type_u32 %tmp_vec2\n"
                               "OpStore %BP_vertex_result %packed_result\n";
    loadVertexResultSnippet  = "%packed_result       = OpLoad %type_u32 %BP_vertex_result\n"
                               "%tmp_vec2            = OpBitcast %type_f16_vec2 %packed_result\n"
                               "%result              = OpCompositeExtract %type_f16 %tmp_vec2 0\n";

    loadStoreRequiresShaderFloat16 = true;

    updateSpirvSnippets();
}

template <>
TypeSnippets<float>::TypeSnippets()
{
    bitWidth                            = "32";
    epsilon                             = "1.175494351e-38";
    capabilities                        = "";
    extensions                          = "";
    capabilitiesFp16Without16BitStorage = "";
    extensionsFp16Without16BitStorage   = "";
    arrayStride                         = "4";

    varyingsTypesSnippet     = "%type_u32_iptr        = OpTypePointer Input %type_u32\n"
                               "%type_u32_optr        = OpTypePointer Output %type_u32\n";
    inputVaryingsSnippet     = "%BP_vertex_result    = OpVariable %type_u32_iptr Input\n";
    outputVaryingsSnippet    = "%BP_vertex_result    = OpVariable %type_u32_optr Output\n";
    storeVertexResultSnippet = "%packed_result       = OpBitcast %type_u32 %result\n"
                               "OpStore %BP_vertex_result %packed_result\n";
    loadVertexResultSnippet  = "%packed_result       = OpLoad %type_u32 %BP_vertex_result\n"
                               "%result              = OpBitcast %type_f32 %packed_result\n";

    loadStoreRequiresShaderFloat16 = false;

    updateSpirvSnippets();
}

template <>
TypeSnippets<double>::TypeSnippets()
{
    bitWidth                            = "64";
    epsilon                             = "2.2250738585072014e-308"; // 0x0010000000000000
    capabilities                        = "OpCapability Float64\n";
    extensions                          = "";
    capabilitiesFp16Without16BitStorage = "";
    extensionsFp16Without16BitStorage   = "";
    arrayStride                         = "8";

    varyingsTypesSnippet     = "%type_u32_vec2_iptr   = OpTypePointer Input %type_u32_vec2\n"
                               "%type_u32_vec2_optr   = OpTypePointer Output %type_u32_vec2\n";
    inputVaryingsSnippet     = "%BP_vertex_result     = OpVariable %type_u32_vec2_iptr Input\n";
    outputVaryingsSnippet    = "%BP_vertex_result     = OpVariable %type_u32_vec2_optr Output\n";
    storeVertexResultSnippet = "%packed_result        = OpBitcast %type_u32_vec2 %result\n"
                               "OpStore %BP_vertex_result %packed_result\n";
    loadVertexResultSnippet  = "%packed_result        = OpLoad %type_u32_vec2 %BP_vertex_result\n"
                               "%result               = OpBitcast %type_f64 %packed_result\n";

    loadStoreRequiresShaderFloat16 = false;

    updateSpirvSnippets();
}

// Operation structure holds data needed to test specified SPIR-V operation. This class contains
// additional annotations, additional types and aditional constants that should be properly included
// in SPIR-V code. Commands attribute in this structure contains code that performs tested operation
// on given arguments, in some cases verification is also performed there.
// All snipets stroed in this structure are generic and can be specialized for fp16, fp32 or fp64,
// thanks to that this data can be shared by many OperationTestCase instances (testing diferent
// float behaviors on diferent float widths).
struct Operation
{
    // operation name is included in test case name
    const char *name;

    // How extensively is the floating point type used?
    FloatUsage floatUsage;

    // operation specific spir-v snippets that will be
    // placed in proper places in final test shader
    const char *annotations;
    const char *types;
    const char *constants;
    const char *variables;
    const char *functions;
    const char *commands;
    vector<string> IDsToDecorate;

    // conversion operations operate on one float type and produce float
    // type with different bit width; restrictedInputType is used only when
    // isInputTypeRestricted is set to true and it restricts usage of this
    // operation to specified input type
    bool isInputTypeRestricted;
    FloatType restrictedInputType;

    // arguments for OpSpecConstant need to be specified also as constant
    bool isSpecConstant;

    // set if c_float* constant is used in operation
    FloatStatementUsageFlags statementUsageFlags;

    Operation()
    {
    }

    // Minimal constructor - used by most of operations
    Operation(const char *_name, FloatUsage _floatUsage, const char *_commands,
              const FloatStatementUsageFlags _statementUsageFlags = 0, vector<string> _IDsToDecorate = {"result"})
        : name(_name)
        , floatUsage(_floatUsage)
        , annotations("")
        , types("")
        , constants("")
        , variables("")
        , functions("")
        , commands(_commands)
        , IDsToDecorate(_IDsToDecorate)
        , isInputTypeRestricted(false)
        , restrictedInputType(FP16) // not used as isInputTypeRestricted is false
        , isSpecConstant(false)
        , statementUsageFlags(_statementUsageFlags)
    {
    }

    // Conversion operations constructor (used also by conversions done in SpecConstantOp)
    Operation(const char *_name, FloatUsage _floatUsage, bool specConstant, FloatType _inputType,
              const char *_constants, const char *_commands, const FloatStatementUsageFlags _statementUsageFlags = 0,
              vector<string> _IDsToDecorate = {"result"})
        : name(_name)
        , floatUsage(_floatUsage)
        , annotations("")
        , types("")
        , constants(_constants)
        , variables("")
        , functions("")
        , commands(_commands)
        , IDsToDecorate(_IDsToDecorate)
        , isInputTypeRestricted(true)
        , restrictedInputType(_inputType)
        , isSpecConstant(specConstant)
        , statementUsageFlags(_statementUsageFlags)
    {
    }

    // Full constructor - used by few operations, that are more complex to test
    Operation(const char *_name, FloatUsage _floatUsage, const char *_annotations, const char *_types,
              const char *_constants, const char *_variables, const char *_functions, const char *_commands,
              const FloatStatementUsageFlags _statementUsageFlags = 0, vector<string> _IDsToDecorate = {"result"})
        : name(_name)
        , floatUsage(_floatUsage)
        , annotations(_annotations)
        , types(_types)
        , constants(_constants)
        , variables(_variables)
        , functions(_functions)
        , commands(_commands)
        , IDsToDecorate(_IDsToDecorate)
        , isInputTypeRestricted(false)
        , restrictedInputType(FP16) // not used as isInputTypeRestricted is false
        , isSpecConstant(false)
        , statementUsageFlags(_statementUsageFlags)
    {
    }

    // Full constructor - used by rounding override cases
    Operation(const char *_name, FloatUsage _floatUsage, FloatType _inputType, const char *_annotations,
              const char *_types, const char *_constants, const char *_commands,
              const FloatStatementUsageFlags _statementUsageFlags = 0, vector<string> _IDsToDecorate = {"result"})
        : name(_name)
        , floatUsage(_floatUsage)
        , annotations(_annotations)
        , types(_types)
        , constants(_constants)
        , variables("")
        , functions("")
        , commands(_commands)
        , IDsToDecorate(_IDsToDecorate)
        , isInputTypeRestricted(true)
        , restrictedInputType(_inputType)
        , isSpecConstant(false)
        , statementUsageFlags(_statementUsageFlags)
    {
    }
};

// Class storing input that will be passed to operation and expected
// output that should be generated for specified behavior.
class OperationTestCase
{
public:
    OperationTestCase()
    {
    }

    OperationTestCase(const char *_baseName, FP _behaviorFlags, bool _useDecorationFlags, OperationId _operatinId,
                      ValueId _input1, ValueId _input2, ValueId _expectedOutput, bool _fp16Without16BitStorage = false,
                      bool _requireRte = false)
        : baseName(_baseName)
        , useDecorationFlags(_useDecorationFlags)
        , operationId(_operatinId)
        , expectedOutput(_expectedOutput)
        , fp16Without16BitStorage(_fp16Without16BitStorage)
        , requireRte(_requireRte)
    {
        if (useDecorationFlags)
        {
            behaviorFlagsExecMode   = allBits;
            behaviorFlagsDecoration = _behaviorFlags;
        }
        else
        {
            behaviorFlagsExecMode   = _behaviorFlags;
            behaviorFlagsDecoration = FP::MaskNone;
        }
        input[0] = _input1;
        input[1] = _input2;
    }

public:
    string baseName;
    FP behaviorFlagsExecMode;
    FP behaviorFlagsDecoration;
    bool useDecorationFlags;
    OperationId operationId;
    ValueId input[2];
    ValueId expectedOutput;
    bool fp16Without16BitStorage;
    bool requireRte;
};

struct OperationTestCaseInputs
{
    OperationId operationId;
    ValueId operandFirst;
    ValueId operandSecond;
    ValueId result;
    FP testedFlagBits;
    bool requireRte = false;
};

// op1 is SPECIAL VALUE (SZ/INF/NAN)
// op2 is 1
// tested flagbits are NSZ,NotInf,NotNaN
struct standardOperationTestCase
{
    OperationId operationId;
    ValueId resultSZ;
    ValueId resultInf;
    ValueId resultNaN;
};

// Helper structure used to store specialized operation
// data. This data is ready to be used during shader assembly.
struct SpecializedOperation
{
    string constants;
    string annotations;
    string types;
    string arguments;
    string variables;
    string functions;
    string commands;

    FloatType inFloatType;
    TypeSnippetsSP inTypeSnippets;
    TypeSnippetsSP outTypeSnippets;
    FloatStatementUsageFlags argumentsUsesFloatConstant;
};

class TypeTestResultsBase
{
public:
    TypeTestResultsBase()
    {
        // tests common for all fp types

        // this array contains only special cases not conforming to standardOperationTestCase
        const OperationTestCaseInputs testCaseInputsArr[] = {
            {OID_NEGATE, V_ZERO, V_UNUSED, V_MINUS_ZERO, FP::NSZ},
            {OID_NEGATE, V_MINUS_INF, V_UNUSED, V_INF, FP::NotInf},

            {OID_ADD, V_MINUS_ZERO, V_MINUS_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_ADD, V_ZERO, V_MINUS_ZERO, V_ZERO, FP::NSZ},
            {OID_ADD, V_MINUS_ONE, V_ONE, V_ZERO, FP::NSZ},
            {OID_ADD, V_HUGE, V_HUGE, V_INF, FP::NotInf, true},
            {OID_ADD, V_ZERO, V_MINUS_INF, V_MINUS_INF, FP::NotInf},
            {OID_ADD, V_ZERO, V_NAN, V_NAN, FP::NotNaN},
            {OID_ADD, V_INF, V_MINUS_INF, V_NAN, FP::NotNaN | FP::NotInf},

            {OID_SUB, V_MINUS_ZERO, V_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_SUB, V_MINUS_ZERO, V_MINUS_ZERO, V_ZERO, FP::NSZ},
            {OID_SUB, V_ZERO, V_MINUS_INF, V_INF, FP::NotInf},
            {OID_SUB, V_ZERO, V_NAN, V_NAN, FP::NotNaN},
            {OID_SUB, V_INF, V_INF, V_NAN, FP::NotNaN | FP::NotInf},

            {OID_MUL, V_MINUS_ONE, V_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_MUL, V_ZERO, V_MINUS_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_MUL, V_TINY, V_MINUS_TINY, V_MINUS_ZERO, FP::NSZ},
            {OID_MUL, V_HUGE, V_HUGE, V_INF, FP::NotInf, true},
            {OID_MUL, V_ZERO, V_INF, V_NAN, FP::NotInf | FP::NotNaN},
            {OID_MUL, V_ZERO, V_NAN, V_NAN, FP::NotNaN},

            {OID_DIV, V_ONE, V_MINUS_INF, V_MINUS_ZERO, FP::NSZ | FP::NotInf},
            {OID_DIV, V_ZERO, V_INF, V_ZERO, FP::NotInf},
            {OID_DIV, V_INF, V_MINUS_ZERO, V_MINUS_INF, FP::NSZ | FP::NotInf},
            {OID_DIV, V_ZERO, V_NAN, V_NAN, FP::NotNaN},
            {OID_DIV, V_INF, V_INF, V_NAN, FP::NotInf | FP::NotNaN},

            {OID_DOT, V_MINUS_ZERO, V_MINUS_ZERO, V_ZERO, FP::NSZ},

            {OID_ABS, V_MINUS_INF, V_UNUSED, V_INF, FP::NotInf},

            {OID_SIGN, V_MINUS_INF, V_UNUSED, V_MINUS_ONE, FP::NotInf},

            {OID_FRACT, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
            {OID_FRACT, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

            {OID_SQRT, V_MINUS_ONE, V_UNUSED, V_NAN, FP::NotNaN},
            {OID_SQRT, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotNaN},

            {OID_INV_SQRT, V_ZERO, V_UNUSED, V_INF, FP::NotInf},
            {OID_INV_SQRT, V_MINUS_ZERO, V_UNUSED, V_MINUS_INF, FP::NSZ | FP::NotInf},
            {OID_INV_SQRT, V_MINUS_ONE, V_UNUSED, V_NAN, FP::NotNaN},
            {OID_INV_SQRT, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotNaN},

            {OID_MODF_ST_WH, V_MINUS_INF, V_UNUSED, V_MINUS_INF, FP::NotInf},
            {OID_MODF_ST_FR, V_MINUS_INF, V_UNUSED, V_MINUS_ZERO, FP::NSZ | FP::NotInf},
            {OID_MODF_ST_FR, V_MINUS_ONE, V_UNUSED, V_MINUS_ZERO, FP::NSZ},

            {OID_LENGTH, V_MINUS_INF, V_UNUSED, V_INF, FP::NotInf},

            {OID_NORMALIZE, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

            {OID_REFLECT, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

            {OID_REFRACT, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

            {OID_MAT_INV, V_ZERO, V_UNUSED, V_MINUS_ZERO, FP::NSZ},

            {OID_MIX, V_NAN, V_ONE, V_NAN, FP::NotNaN},
            {OID_MIX, V_ONE, V_NAN, V_NAN, FP::NotNaN},

            {OID_FMA2PT58, V_ZERO_POINT_ONE, V_TWENTY_FIVE_POINT_EIGHT, V_ZERO,
             FP::AllowContract},                                                     // 0.1 * 25.8 - 2.58 == 0.0
            {OID_SZ_FMA, V_MINUS_ZERO, V_ZERO, V_ZERO, FP::AllowContract | FP::NSZ}, // -0.0 * 1 +  0.0 ==  0.0
            {OID_SZ_FMA, V_MINUS_ZERO, V_MINUS_ZERO, V_MINUS_ZERO,
             FP::AllowContract | FP::NSZ}, // -0.0 * 1 + -0.0 == -0.0

            {OID_MIN, V_MINUS_ZERO, V_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_MIN, V_ZERO, V_MINUS_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_MIN, V_MINUS_INF, V_ONE, V_MINUS_INF, FP::NotInf},

            {OID_MAX, V_MINUS_ZERO, V_ZERO, V_ZERO, FP::NSZ},
            {OID_MAX, V_ZERO, V_MINUS_ZERO, V_ZERO, FP::NSZ},
            {OID_MAX, V_MINUS_INF, V_ONE, V_ONE, FP::NotInf},

            {OID_CLAMP, V_MINUS_ONE, V_MINUS_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_CLAMP, V_MINUS_ZERO, V_ZERO, V_ZERO, FP::NSZ},
            {OID_CLAMP, V_ZERO, V_MINUS_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_CLAMP, V_INF, V_ONE, V_ONE, FP::NotInf},
            {OID_CLAMP, V_ONE, V_INF, V_INF, FP::NotInf},
            {OID_CLAMP, V_ONE, V_MINUS_INF, V_MINUS_INF, FP::NotInf},
            {OID_CLAMP, V_NAN, V_ONE, V_ONE_OR_NAN, FP::NotNaN},
            {OID_CLAMP, V_ONE, V_NAN, V_ONE_OR_NAN, FP::NotNaN},

            {OID_CROSS, V_MINUS_ZERO, V_MINUS_ZERO, V_ZERO, FP::NSZ},
            {OID_CROSS, V_INF, V_ONE, V_UNUSED, FP::NotInf},
            {OID_CROSS, V_NAN, V_ONE, V_NAN, FP::NotNaN},

            {OID_NMIN, V_MINUS_ZERO, V_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_NMIN, V_ZERO, V_MINUS_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_NMIN, V_MINUS_INF, V_ONE, V_MINUS_INF, FP::NotInf},

            {OID_NMAX, V_MINUS_ZERO, V_ZERO, V_ZERO, FP::NSZ},
            {OID_NMAX, V_ZERO, V_MINUS_ZERO, V_ZERO, FP::NSZ},
            {OID_NMAX, V_MINUS_INF, V_ONE, V_ONE, FP::NotInf},

            {OID_NCLAMP, V_MINUS_ONE, V_MINUS_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_NCLAMP, V_MINUS_ZERO, V_ZERO, V_ZERO, FP::NSZ},
            {OID_NCLAMP, V_ZERO, V_MINUS_ZERO, V_MINUS_ZERO, FP::NSZ},
            {OID_NCLAMP, V_INF, V_ONE, V_ONE, FP::NotInf},
            {OID_NCLAMP, V_ONE, V_INF, V_INF, FP::NotInf},
            {OID_NCLAMP, V_ONE, V_MINUS_INF, V_MINUS_INF, FP::NotInf},
            {OID_NCLAMP, V_NAN, V_ONE, V_ONE, FP::NotNaN},
            {OID_NCLAMP, V_ONE, V_NAN, V_ONE, FP::NotNaN},

            // a + b + (-a)
            {OID_ADD_SUB_REASSOCIABLE, V_MAX, V_HUGE, V_INF, FP::AllowReassoc | FP::NotInf, true},
            // a + a + (-a)
            {OID_ADD_SUB_REASSOCIABLE, V_MAX, V_MAX, V_INF, FP::AllowReassoc | FP::NotInf, true},
        };

        testCaseInputs.insert(testCaseInputs.begin(), testCaseInputsArr,
                              testCaseInputsArr + DE_LENGTH_OF_ARRAY(testCaseInputsArr));

        const standardOperationTestCase stcArr[] = {
            {OID_NEGATE, V_ZERO, V_MINUS_INF, V_NAN},
            {OID_COMPOSITE, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_COMPOSITE_INS, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_COPY, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_D_EXTRACT, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_D_INSERT, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_SHUFFLE, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_TRANSPOSE, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_RETURN_VAL, V_MINUS_ZERO, V_INF, V_NAN},

            {OID_ADD, V_ONE, V_INF, V_NAN},
            {OID_SUB, V_MINUS_ONE, V_INF, V_NAN},
            {OID_MUL, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_DIV, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_REM, V_UNUSED, V_UNUSED, V_NAN},
            {OID_MOD, V_UNUSED, V_UNUSED, V_NAN},
            {OID_PHI, V_MINUS_ZERO, V_ONE, V_NAN},
            {OID_SELECT, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_DOT, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_VEC_MUL_S, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_VEC_MUL_M, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_MAT_MUL_S, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_MAT_MUL_V, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_MAT_MUL_M, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_OUT_PROD, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_ORD_EQ, V_ZERO, V_ZERO, V_ZERO},
            {OID_UORD_EQ, V_ZERO, V_ZERO, V_ONE},
            {OID_ORD_NEQ, V_ONE, V_ONE, V_ZERO},
            {OID_UORD_NEQ, V_ONE, V_ONE, V_ONE},
            {OID_ORD_LS, V_ONE, V_ZERO, V_ZERO},
            {OID_UORD_LS, V_ONE, V_ZERO, V_ONE},
            {OID_ORD_GT, V_ZERO, V_ONE, V_ZERO},
            {OID_UORD_GT, V_ZERO, V_ONE, V_ONE},
            {OID_ORD_LE, V_ONE, V_ZERO, V_ZERO},
            {OID_UORD_LE, V_ONE, V_ZERO, V_ONE},
            {OID_ORD_GE, V_ZERO, V_ONE, V_ZERO},
            {OID_UORD_GE, V_ZERO, V_ONE, V_ONE},
            {OID_ROUND, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_ROUND_EV, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_TRUNC, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_ABS, V_ZERO, V_INF, V_NAN},
            {OID_SIGN, V_ZERO_OR_MINUS_ZERO, V_ONE, V_SIGN_NAN},
            {OID_FLOOR, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_CEIL, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_FRACT, V_ZERO, V_UNUSED, V_NAN}, // fract(Inf) == NaN, so needs non-standard flags.
            {OID_SQRT, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_INV_SQRT, V_UNUSED, V_ZERO, V_NAN}, // -0 needs NotInf, so handled as special case.
            {OID_MODF, V_MINUS_ZERO, V_UNUSED, V_NAN},
            {OID_MODF_ST_WH, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_MODF_ST_FR, V_MINUS_ZERO, V_ZERO, V_NAN},
            {OID_LDEXP, V_MINUS_ZERO, V_INF, V_NAN},
            {OID_FREXP, V_MINUS_ZERO, V_UNUSED, V_UNUSED},
            {OID_FREXP_ST, V_MINUS_ZERO, V_UNUSED, V_UNUSED},
            {OID_LENGTH, V_ZERO, V_INF, V_NAN},
            {OID_NORMALIZE, V_MINUS_ZERO, V_UNUSED, V_NAN},
            {OID_REFLECT, V_MINUS_ZERO, V_UNUSED, V_NAN},
            {OID_REFRACT, V_MINUS_ZERO, V_UNUSED, V_NAN},
            {OID_MAT_DET, V_ZERO, V_UNUSED, V_NAN},
            {OID_MAT_INV, V_ZERO, V_UNUSED, V_NAN},
            {OID_FMA, V_MINUS_ONE, V_INF, V_NAN},
            {OID_MIN, V_MINUS_ZERO, V_ONE, V_ONE_OR_NAN},
            {OID_MAX, V_ONE, V_INF, V_ONE_OR_NAN},
            {OID_STEP, V_ONE, V_ZERO, V_UNUSED},
            {OID_SSTEP, V_HALF, V_UNUSED, V_UNUSED},
            {OID_DIST, V_ONE, V_INF, V_NAN},
            {OID_FACE_FWD, V_MINUS_ONE, V_MINUS_ONE, V_UNUSED},
            {OID_NMIN, V_MINUS_ZERO, V_ONE, V_ONE},
            {OID_NMAX, V_ONE, V_INF, V_ONE},
        };

        appendStandardCases(stcArr, DE_LENGTH_OF_ARRAY(stcArr));
    }

    void appendStandardCases(const standardOperationTestCase *cases, size_t count)
    {
        for (size_t i = 0; i < count; i++)
        {
            testCaseInputs.push_back({cases[i].operationId, V_MINUS_ZERO, V_ONE, cases[i].resultSZ, FP::NSZ});
            testCaseInputs.push_back({cases[i].operationId, V_INF, V_ONE, cases[i].resultInf, FP::NotInf});
            testCaseInputs.push_back({cases[i].operationId, V_NAN, V_ONE, cases[i].resultNaN, FP::NotNaN});
        }
    }

    virtual ~TypeTestResultsBase()
    {
    }

    FloatType floatType() const;

protected:
    FloatType m_floatType;

public:
    vector<OperationTestCaseInputs> testCaseInputs;
};

FloatType TypeTestResultsBase::floatType() const
{
    return m_floatType;
}

typedef de::SharedPtr<TypeTestResultsBase> TypeTestResultsSP;

template <typename FLOAT_TYPE>
class TypeTestResults : public TypeTestResultsBase
{
public:
    TypeTestResults();
};

const OperationTestCaseInputs nonStc16and32Only[] = {
    {OID_SIN, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_SIN, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_COS, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_COS, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_TAN, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_TAN, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

    {OID_ASIN, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_ASIN, V_TWO, V_UNUSED, V_NAN, FP::NotNaN},
    {OID_ASIN, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_ACOS, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_ACOS, V_TWO, V_UNUSED, V_NAN, FP::NotNaN},
    {OID_ACOS, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

    {OID_ATAN, V_MINUS_INF, V_UNUSED, V_MINUS_PI_DIV_2, FP::NotInf},

    {OID_SINH, V_MINUS_INF, V_UNUSED, V_MINUS_INF, FP::NotInf},
    {OID_COSH, V_MINUS_INF, V_UNUSED, V_INF, FP::NotInf},
    {OID_TANH, V_MINUS_INF, V_UNUSED, V_MINUS_ONE, FP::NotInf},

    {OID_ASINH, V_MINUS_INF, V_UNUSED, V_MINUS_INF, FP::NotInf},

    {OID_ACOSH, V_ZERO, V_UNUSED, V_NAN, FP::NotNaN},
    {OID_ACOSH, V_MINUS_ZERO, V_UNUSED, V_NAN, FP::NSZ | FP::NotNaN},
    {OID_ACOSH, V_HALF, V_UNUSED, V_NAN, FP::NotNaN},
    {OID_ACOSH, V_INF, V_UNUSED, V_INF, FP::NotInf},
    {OID_ACOSH, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

    {OID_ATANH, V_TWO, V_UNUSED, V_NAN, FP::NotNaN},
    {OID_ATANH, V_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},
    {OID_ATANH, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

    {OID_EXP, V_MINUS_INF, V_UNUSED, V_ZERO, FP::NotInf},

    {OID_LOG, V_ZERO, V_UNUSED, V_MINUS_INF, FP::NSZ | FP::NotInf},
    {OID_LOG, V_MINUS_ZERO, V_UNUSED, V_MINUS_INF, FP::NSZ | FP::NotInf},
    {OID_LOG, V_MINUS_ONE, V_UNUSED, V_NAN, FP::NotNaN},
    {OID_LOG, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

    {OID_EXP2, V_MINUS_INF, V_UNUSED, V_ZERO, FP::NotInf},

    {OID_LOG2, V_ZERO, V_UNUSED, V_MINUS_INF, FP::NSZ | FP::NotInf},
    {OID_LOG2, V_MINUS_ZERO, V_UNUSED, V_MINUS_INF, FP::NSZ | FP::NotInf},
    {OID_LOG2, V_MINUS_ONE, V_UNUSED, V_NAN, FP::NotNaN},
    {OID_LOG2, V_MINUS_INF, V_UNUSED, V_NAN, FP::NotInf | FP::NotNaN},

    {OID_ATAN2, V_ZERO, V_MINUS_ONE, V_PI, FP::NSZ},
    {OID_ATAN2, V_MINUS_ZERO, V_MINUS_ONE, V_MINUS_PI, FP::NSZ},
    // SPIR-V explicitly says that atan(0, 0) is undefined, so these next 2 tests would not be valid.
    // The expected behaviour given is the one from POSIX, OpenCL and IEEE-754.
    //{ OID_ATAN2, V_ZERO, V_MINUS_ZERO, V_PI, FP::NSZ },
    //{ OID_ATAN2, V_MINUS_ZERO, V_MINUS_ZERO, V_MINUS_PI, FP::NSZ },
    {OID_ATAN2, V_ZERO, V_MINUS_INF, V_PI, FP::NSZ | FP::NotInf},
    {OID_ATAN2, V_MINUS_ZERO, V_MINUS_INF, V_MINUS_PI, FP::NSZ | FP::NotInf},
    {OID_ATAN2, V_ONE, V_MINUS_INF, V_PI, FP::NSZ | FP::NotInf},
    {OID_ATAN2, V_MINUS_ONE, V_MINUS_INF, V_MINUS_PI, FP::NSZ | FP::NotInf},
    {OID_ATAN2, V_ONE, V_INF, V_ZERO_OR_MINUS_ZERO, FP::NotInf},
    {OID_ATAN2, V_MINUS_ONE, V_INF, V_ZERO_OR_MINUS_ZERO, FP::NotInf},
    {OID_ATAN2, V_INF, V_ONE, V_PI_DIV_2, FP::NotInf},
    {OID_ATAN2, V_MINUS_INF, V_ONE, V_MINUS_PI_DIV_2, FP::NotInf},
    {OID_ATAN2, V_INF, V_MINUS_INF, V_3_PI_DIV_4, FP::NotInf},
    {OID_ATAN2, V_MINUS_INF, V_MINUS_INF, V_MINUS_3_PI_DIV_4, FP::NotInf},
    {OID_ATAN2, V_INF, V_INF, V_PI_DIV_4, FP::NotInf},
    {OID_ATAN2, V_MINUS_INF, V_INF, V_MINUS_PI_DIV_4, FP::NotInf},
    {OID_ATAN2, V_NAN, V_ONE, V_NAN, FP::NotNaN},
    {OID_ATAN2, V_ONE, V_NAN, V_NAN, FP::NotNaN},
};

// Most of these operations are not accurate enough at 0 to resolve the difference between
// +0 and -0 so the test is skipped. sin, cos and tan are also explicitly low precision for
// large inputs, so are not tested at infinity.
const standardOperationTestCase stc16and32only[] = {
    {OID_RADIANS, V_MINUS_ZERO, V_INF, V_NAN},
    {OID_DEGREES, V_MINUS_ZERO, V_INF, V_NAN},
    {OID_SIN, V_UNUSED, V_UNUSED, V_NAN},
    {OID_COS, V_TRIG_ONE, V_UNUSED, V_NAN},
    {OID_TAN, V_UNUSED, V_UNUSED, V_NAN},
    {OID_ASIN, V_UNUSED, V_UNUSED, V_NAN},
    {OID_ACOS, V_PI_DIV_2, V_UNUSED, V_NAN},
    {OID_ATAN, V_UNUSED, V_PI_DIV_2, V_NAN},
    {OID_SINH, V_UNUSED, V_INF, V_NAN},
    {OID_COSH, V_ONE, V_INF, V_NAN},
    {OID_TANH, V_UNUSED, V_ONE, V_NAN},
    {OID_ASINH, V_UNUSED, V_INF, V_NAN},
    {OID_ACOSH, V_UNUSED, V_INF, V_NAN},
    {OID_ATANH, V_UNUSED, V_UNUSED, V_NAN},
    {OID_EXP, V_ONE, V_INF, V_NAN},
    {OID_LOG, V_UNUSED, V_INF, V_NAN},
    {OID_EXP2, V_ONE, V_INF, V_NAN},
    {OID_LOG2, V_UNUSED, V_INF, V_NAN},
    // OID_ATAN2 -- All handled as special cases
    {OID_POW, V_UNUSED, V_INF, V_NAN},
};

template <>
TypeTestResults<deFloat16>::TypeTestResults() : TypeTestResultsBase()
{
    m_floatType = FP16;

    const standardOperationTestCase stcConvTo16[] = {
        {OID_CONV_FROM_FP32, V_MINUS_ZERO, V_INF, V_NAN},
        {OID_CONV_FROM_FP64, V_MINUS_ZERO, V_INF, V_NAN},
    };

    testCaseInputs.insert(testCaseInputs.end(), nonStc16and32Only,
                          nonStc16and32Only + DE_LENGTH_OF_ARRAY(nonStc16and32Only));

    appendStandardCases(stcConvTo16, DE_LENGTH_OF_ARRAY(stcConvTo16));
    appendStandardCases(stc16and32only, DE_LENGTH_OF_ARRAY(stc16and32only));
}

template <>
TypeTestResults<float>::TypeTestResults() : TypeTestResultsBase()
{
    m_floatType = FP32;

    const standardOperationTestCase stcConvTo32[] = {
        {OID_CONV_FROM_FP16, V_MINUS_ZERO, V_INF, V_NAN},
        {OID_CONV_FROM_FP64, V_MINUS_ZERO, V_INF, V_NAN},
    };

    testCaseInputs.insert(testCaseInputs.end(), nonStc16and32Only,
                          nonStc16and32Only + DE_LENGTH_OF_ARRAY(nonStc16and32Only));

    appendStandardCases(stcConvTo32, DE_LENGTH_OF_ARRAY(stcConvTo32));
    appendStandardCases(stc16and32only, DE_LENGTH_OF_ARRAY(stc16and32only));
}

template <>
TypeTestResults<double>::TypeTestResults() : TypeTestResultsBase()
{
    m_floatType = FP64;

    const standardOperationTestCase stcConvTo64[] = {
        {OID_CONV_FROM_FP16, V_MINUS_ZERO, V_INF, V_NAN},
        {OID_CONV_FROM_FP32, V_MINUS_ZERO, V_INF, V_NAN},
    };

    appendStandardCases(stcConvTo64, DE_LENGTH_OF_ARRAY(stcConvTo64));
}

// Class responsible for constructing list of test cases for specified
// float type and specified way of preparation of arguments.
// Arguments can be either read from input SSBO or generated via math
// operations in spir-v code.
class TestCasesBuilder
{
public:
    void init();
    void build(vector<OperationTestCase> &testCases, TypeTestResultsSP typeTestResults);
    const Operation &getOperation(OperationId id) const;

private:
    void createUnaryTestCases(vector<OperationTestCase> &testCases, OperationId operationId,
                              ValueId denormPreserveResult, ValueId denormFTZResult,
                              bool fp16WithoutStorage = false) const;

private:
    // Operations are shared betwean test cases so they are
    // passed to them as pointers to data stored in TestCasesBuilder.
    typedef OperationTestCase OTC;
    typedef Operation Op;
    map<int, Op> m_operations;
};

void TestCasesBuilder::init()
{
    map<int, Op> &mo = m_operations;

    // predefine operations repeatedly used in tests; note that "_float"
    // in every operation command will be replaced with either "_f16",
    // "_f32" or "_f64" - StringTemplate is not used here because it
    // would make code less readable
    // m_operations contains generic operation definitions that can be
    // used for all float types

    mo[OID_NEGATE]    = Op("negate", FLOAT_ARITHMETIC, "%result             = OpFNegate %type_float %arg1\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_COMPOSITE] = Op("composite", FLOAT_ARITHMETIC,
                           "%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                           "%result             = OpCompositeExtract %type_float %vec1 0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"vec1", "result"});
    mo[OID_COMPOSITE_INS] =
        Op("comp_ins", FLOAT_ARITHMETIC,
           "%vec1               = OpCompositeConstruct %type_float_vec2 %c_float_0 %c_float_0\n"
           "%vec2               = OpCompositeInsert %type_float_vec2 %arg1 %vec1 0\n"
           "%result             = OpCompositeExtract %type_float %vec2 0\n",
           B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"vec2", "result"});
    mo[OID_COPY]      = Op("copy", FLOAT_STORAGE_ONLY, "%result             = OpCopyObject %type_float %arg1\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_D_EXTRACT] = Op("extract", FLOAT_ARITHMETIC,
                           "%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                           "%result             = OpVectorExtractDynamic %type_float %vec1 %c_i32_0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"vec1", "result"});
    mo[OID_D_INSERT]  = Op("insert", FLOAT_ARITHMETIC,
                           "%tmpVec             = OpCompositeConstruct %type_float_vec2 %c_float_2 %c_float_2\n"
                            "%vec1               = OpVectorInsertDynamic %type_float_vec2 %tmpVec %arg1 %c_i32_0\n"
                            "%result             = OpCompositeExtract %type_float %vec1 0\n",
                           B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT,
                           {"tmpVec", "vec1", "result"});
    mo[OID_SHUFFLE]   = Op(
        "shuffle", FLOAT_ARITHMETIC,
        "%tmpVec1            = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
          "%tmpVec2            = OpCompositeConstruct %type_float_vec2 %c_float_2 %c_float_2\n" // NOTE: its impossible to test shuffle with denorms flushed
        "%vec1               = OpVectorShuffle %type_float_vec2 %tmpVec1 %tmpVec2 0 2\n" //       to zero as this will be done by earlier operation
        "%result             = OpCompositeExtract %type_float %vec1 0\n", //       (this also applies to few other operations)
        B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"tmpVec1", "vec1", "result"});
    mo[OID_TRANSPOSE] = Op("transpose", FLOAT_ARITHMETIC,
                           "%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                           "%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
                           "%tmat               = OpTranspose %type_float_mat2x2 %mat\n"
                           "%tcol               = OpCompositeExtract %type_float_vec2 %tmat 0\n"
                           "%result             = OpCompositeExtract %type_float %tcol 0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"col", "mat", "tmat", "tcol", "result"});
    mo[OID_RETURN_VAL] =
        Op("ret_val", FLOAT_ARITHMETIC, "", "%type_test_fun      = OpTypeFunction %type_float %type_float\n", "", "",
           "%test_fun = OpFunction %type_float None %type_test_fun\n"
           "%param = OpFunctionParameter %type_float\n"
           "%entry = OpLabel\n"
           "OpReturnValue %param\n"
           "OpFunctionEnd\n",
           "%result             = OpFunctionCall %type_float %test_fun %arg1\n",
           B_STATEMENT_USAGE_TYPES_TYPE_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"param", "entry", "result"});

    // conversion operations that are meant to be used only for single output type (defined by the second number in name)
    const char *convertSource = "%result             = OpFConvert %type_float %arg1\n";
    mo[OID_CONV_FROM_FP16] =
        Op("conv_from_fp16", FLOAT_STORAGE_ONLY, false, FP16, "", convertSource, B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_CONV_FROM_FP32] =
        Op("conv_from_fp32", FLOAT_STORAGE_ONLY, false, FP32, "", convertSource, B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_CONV_FROM_FP64] =
        Op("conv_from_fp64", FLOAT_STORAGE_ONLY, false, FP64, "", convertSource, B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

    mo[OID_ADD] = Op("add", FLOAT_ARITHMETIC, "%result             = OpFAdd %type_float %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_SUB] = Op("sub", FLOAT_ARITHMETIC, "%result             = OpFSub %type_float %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_MUL] = Op("mul", FLOAT_ARITHMETIC, "%result             = OpFMul %type_float %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_DIV] = Op("div", FLOAT_ARITHMETIC, "%result             = OpFDiv %type_float %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_REM] = Op("rem", FLOAT_ARITHMETIC, "%result             = OpFRem %type_float %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_MOD] = Op("mod", FLOAT_ARITHMETIC, "%result             = OpFMod %type_float %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

    mo[OID_ADD_SUB_REASSOCIABLE] = Op("add_sub_reassociable", FLOAT_ARITHMETIC,
                                      "%temp               = OpFAdd %type_float %arg1 %arg2\n"
                                      "%result             = OpFSub %type_float %temp %arg1\n",
                                      B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

    mo[OID_PHI]       = Op("phi", FLOAT_ARITHMETIC,
                           "%comp               = OpFOrdGreaterThan %type_bool %arg1 %arg2\n"
                                 "                      OpSelectionMerge %comp_merge None\n"
                                 "                      OpBranchConditional %comp %true_branch %false_branch\n"
                                 "%true_branch        = OpLabel\n"
                                 "                      OpBranch %comp_merge\n"
                                 "%false_branch       = OpLabel\n"
                                 "                      OpBranch %comp_merge\n"
                                 "%comp_merge         = OpLabel\n"
                                 "%result             = OpPhi %type_float %arg2 %true_branch %arg1 %false_branch\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"arg1", "arg2", "comp", "result"});
    mo[OID_SELECT]    = Op("select", FLOAT_ARITHMETIC,
                           "%always_true        = OpFOrdGreaterThan %type_bool %c_float_1 %c_float_0\n"
                              "%result             = OpSelect %type_float %always_true %arg1 %arg2\n",
                           B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_DOT]       = Op("dot", FLOAT_ARITHMETIC,
                           "%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                                 "%vec2               = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
                                 "%result             = OpDot %type_float %vec1 %vec2\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"vec1", "vec2", "result"});
    mo[OID_VEC_MUL_S] = Op("vmuls", FLOAT_ARITHMETIC,
                           "%vec                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                           "%tmpVec             = OpVectorTimesScalar %type_float_vec2 %vec %arg2\n"
                           "%result             = OpCompositeExtract %type_float %tmpVec 0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"vec", "tmpVec", "result"});
    mo[OID_VEC_MUL_M] = Op("vmulm", FLOAT_ARITHMETIC,
                           "%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                           "%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
                           "%vec                = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
                           "%tmpVec             = OpVectorTimesMatrix %type_float_vec2 %vec %mat\n"
                           "%result             = OpCompositeExtract %type_float %tmpVec 0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"col", "mat", "vec", "tmpVec", "result"});
    mo[OID_MAT_MUL_S] = Op("mmuls", FLOAT_ARITHMETIC,
                           "%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                           "%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
                           "%mulMat             = OpMatrixTimesScalar %type_float_mat2x2 %mat %arg2\n"
                           "%extCol             = OpCompositeExtract %type_float_vec2 %mulMat 0\n"
                           "%result             = OpCompositeExtract %type_float %extCol 0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"col", "mat", "mulMat", "result"});
    mo[OID_MAT_MUL_V] = Op("mmulv", FLOAT_ARITHMETIC,
                           "%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                           "%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
                           "%vec                = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
                           "%mulVec             = OpMatrixTimesVector %type_float_vec2 %mat %vec\n"
                           "%result             = OpCompositeExtract %type_float %mulVec 0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"col", "mat", "vec", "mulVec", "result"});
    mo[OID_MAT_MUL_M] = Op("mmulm", FLOAT_ARITHMETIC,
                           "%col1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                           "%mat1               = OpCompositeConstruct %type_float_mat2x2 %col1 %col1\n"
                           "%col2               = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
                           "%mat2               = OpCompositeConstruct %type_float_mat2x2 %col2 %col2\n"
                           "%mulMat             = OpMatrixTimesMatrix %type_float_mat2x2 %mat1 %mat2\n"
                           "%extCol             = OpCompositeExtract %type_float_vec2 %mulMat 0\n"
                           "%result             = OpCompositeExtract %type_float %extCol 0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"col1", "mat1", "col2", "mat2", "mulMat", "result"});
    mo[OID_OUT_PROD]  = Op("out_prod", FLOAT_ARITHMETIC,
                           "%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                            "%vec2               = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
                            "%mulMat             = OpOuterProduct %type_float_mat2x2 %vec1 %vec2\n"
                            "%extCol             = OpCompositeExtract %type_float_vec2 %mulMat 0\n"
                            "%result             = OpCompositeExtract %type_float %extCol 0\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"vec1", "vec2", "mulMat", "result"});

    // comparison operations
    mo[OID_ORD_EQ]   = Op("ord_eq", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFOrdEqual %type_bool %arg1 %arg2\n"
                            "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_UORD_EQ]  = Op("uord_eq", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFUnordEqual %type_bool %arg1 %arg2\n"
                           "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_ORD_NEQ]  = Op("ord_neq", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFOrdNotEqual %type_bool %arg1 %arg2\n"
                           "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_UORD_NEQ] = Op("uord_neq", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFUnordNotEqual %type_bool %arg1 %arg2\n"
                          "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_ORD_LS]   = Op("ord_ls", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFOrdLessThan %type_bool %arg1 %arg2\n"
                            "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_UORD_LS]  = Op("uord_ls", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFUnordLessThan %type_bool %arg1 %arg2\n"
                           "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_ORD_GT]   = Op("ord_gt", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFOrdGreaterThan %type_bool %arg1 %arg2\n"
                            "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_UORD_GT]  = Op("uord_gt", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFUnordGreaterThan %type_bool %arg1 %arg2\n"
                           "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_ORD_LE]   = Op("ord_le", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFOrdLessThanEqual %type_bool %arg1 %arg2\n"
                            "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_UORD_LE]  = Op("uord_le", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFUnordLessThanEqual %type_bool %arg1 %arg2\n"
                           "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_ORD_GE]   = Op("ord_ge", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFOrdGreaterThanEqual %type_bool %arg1 %arg2\n"
                            "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});
    mo[OID_UORD_GE]  = Op("uord_ge", FLOAT_ARITHMETIC,
                          "%boolVal           = OpFUnordGreaterThanEqual %type_bool %arg1 %arg2\n"
                           "%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"boolVal"});

    mo[OID_ATAN2] =
        Op("atan2", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Atan2 %arg1 %arg2\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_POW] = Op("pow", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Pow %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_MIX] = Op("mix", FLOAT_ARITHMETIC,
                     "%result             = OpExtInst %type_float %std450 FMix %arg1 %arg2 %c_float_0_5\n",
                     B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    // OID_FMA is testing that operations don't get merged into fma, so they deliberately don't use fma here.
    // The fast-math mode for the Add determines whether these operations can be contracted, so the OpFMul is not decorated.
    mo[OID_FMA] = Op("fma", FLOAT_ARITHMETIC,
                     "%temp               = OpFMul %type_float %arg1 %arg2\n"
                     "%result             = OpFAdd %type_float %temp %c_float_n1\n",
                     B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    // OID_FMA2PT58 is testing that operations don't get merged into fma, so they deliberately don't use fma here.
    // The fast-math mode for the Add determines whether these operations can be contracted, so the OpFMul is not decorated.
    mo[OID_FMA2PT58] = Op("fma", FLOAT_ARITHMETIC,
                          "%temp               = OpFMul %type_float %arg1 %arg2\n"
                          "%result             = OpFAdd %type_float %temp %c_float_n2pt58\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_SZ_FMA]   = Op("sz_fma", FLOAT_ARITHMETIC,
                          "%result             = OpExtInst %type_float %std450 Fma %arg1 %c_float_1 %arg2\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_MIN] = Op("min", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 FMin %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_MAX] = Op("max", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 FMax %arg1 %arg2\n",
                     B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_CLAMP] =
        Op("clamp", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 FClamp %arg1 %arg2 %arg2\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_STEP] =
        Op("step", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Step %arg1 %arg2\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_SSTEP] = Op("sstep", FLOAT_ARITHMETIC,
                       "%result             = OpExtInst %type_float %std450 SmoothStep %arg1 %arg2 %c_float_0_5\n",
                       B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_DIST] =
        Op("distance", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Distance %arg1 %arg2\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_CROSS]    = Op("cross", FLOAT_ARITHMETIC,
                          "%vec1               = OpCompositeConstruct %type_float_vec3 %arg1 %arg1 %arg1\n"
                             "%vec2               = OpCompositeConstruct %type_float_vec3 %arg2 %arg2 %arg2\n"
                             "%tmpVec             = OpExtInst %type_float_vec3 %std450 Cross %vec1 %vec2\n"
                             "%result             = OpCompositeExtract %type_float %tmpVec 0\n",
                          B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"vec1", "vec2", "tmpVec", "result"});
    mo[OID_FACE_FWD] = Op("face_fwd", FLOAT_ARITHMETIC,
                          "%result             = OpExtInst %type_float %std450 FaceForward %c_float_1 %arg1 %arg2\n",
                          B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_NMIN] =
        Op("nmin", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 NMin %arg1 %arg2\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_NMAX] =
        Op("nmax", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 NMax %arg1 %arg2\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_NCLAMP] =
        Op("nclamp", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 NClamp %arg2 %arg1 %arg2\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

    mo[OID_ROUND] = Op("round", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Round %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_ROUND_EV] =
        Op("round_ev", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 RoundEven %arg1\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_TRUNC] = Op("trunc", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Trunc %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_ABS]   = Op("abs", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 FAbs %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_SIGN]  = Op("sign", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 FSign %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_FLOOR] = Op("floor", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Floor %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_CEIL]  = Op("ceil", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Ceil %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_FRACT] = Op("fract", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Fract %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_RADIANS] =
        Op("radians", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Radians %arg1\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_DEGREES] =
        Op("degrees", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Degrees %arg1\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_SIN]   = Op("sin", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Sin %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_COS]   = Op("cos", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Cos %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_TAN]   = Op("tan", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Tan %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_ASIN]  = Op("asin", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Asin %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_ACOS]  = Op("acos", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Acos %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_ATAN]  = Op("atan", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Atan %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_SINH]  = Op("sinh", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Sinh %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_COSH]  = Op("cosh", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Cosh %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_TANH]  = Op("tanh", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Tanh %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_ASINH] = Op("asinh", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Asinh %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_ACOSH] = Op("acosh", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Acosh %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_ATANH] = Op("atanh", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Atanh %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_EXP]   = Op("exp", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Exp %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_LOG]   = Op("log", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Log %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_EXP2]  = Op("exp2", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Exp2 %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_LOG2]  = Op("log2", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Log2 %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_SQRT]  = Op("sqrt", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Sqrt %arg1\n",
                       B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_INV_SQRT] =
        Op("inv_sqrt", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 InverseSqrt %arg1\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_MODF] =
        Op("modf", FLOAT_ARITHMETIC, "", "", "", "%tmpVarPtr          = OpVariable %type_float_fptr Function\n", "",
           "%result             = OpExtInst %type_float %std450 Modf %arg1 %tmpVarPtr\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_MODF_ST_WH] = Op("modf_st_wh", FLOAT_ARITHMETIC,
                            "OpMemberDecorate %struct_ff 0 Offset 0\n"
                            "OpMemberDecorate %struct_ff 1 Offset ${float_width}\n",
                            "%struct_ff          = OpTypeStruct %type_float %type_float\n"
                            "%struct_ff_fptr     = OpTypePointer Function %struct_ff\n",
                            "", "%tmpStructPtr       = OpVariable %struct_ff_fptr Function\n", "",
                            "%tmpStruct          = OpExtInst %struct_ff %std450 ModfStruct %arg1\n"
                            "                      OpStore %tmpStructPtr %tmpStruct\n"
                            "%tmpLoc             = OpAccessChain %type_float_fptr %tmpStructPtr %c_i32_1\n"
                            "%result             = OpLoad %type_float %tmpLoc\n",
                            B_STATEMENT_USAGE_TYPES_TYPE_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT,
                            {"tmpStruct", "tmpLoc", "result"});
    mo[OID_MODF_ST_FR] = Op("modf_st_fr", FLOAT_ARITHMETIC,
                            "OpMemberDecorate %struct_ff 0 Offset 0\n"
                            "OpMemberDecorate %struct_ff 1 Offset ${float_width}\n",
                            "%struct_ff          = OpTypeStruct %type_float %type_float\n"
                            "%struct_ff_fptr     = OpTypePointer Function %struct_ff\n",
                            "", "%tmpStructPtr       = OpVariable %struct_ff_fptr Function\n", "",
                            "%tmpStruct          = OpExtInst %struct_ff %std450 ModfStruct %arg1\n"
                            "                      OpStore %tmpStructPtr %tmpStruct\n"
                            "%tmpLoc             = OpAccessChain %type_float_fptr %tmpStructPtr %c_i32_0\n"
                            "%result             = OpLoad %type_float %tmpLoc\n",
                            B_STATEMENT_USAGE_TYPES_TYPE_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT,
                            {"tmpStruct", "tmpLoc", "result"});
    mo[OID_LDEXP] =
        Op("ldexp", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Ldexp %arg1 %c_i32_1\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_FREXP] =
        Op("frexp", FLOAT_ARITHMETIC, "", "", "", "%tmpVarPtr          = OpVariable %type_i32_fptr Function\n", "",
           "%result             = OpExtInst %type_float %std450 Frexp %arg1 %tmpVarPtr\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_FREXP_ST] = Op("frexp_st", FLOAT_ARITHMETIC,
                          "OpMemberDecorate %struct_fi 0 Offset 0\n"
                          "OpMemberDecorate %struct_fi 1 Offset ${float_width}\n",
                          "%struct_fi          = OpTypeStruct %type_float %type_i32\n"
                          "%struct_fi_fptr     = OpTypePointer Function %struct_fi\n",
                          "", "%tmpStructPtr       = OpVariable %struct_fi_fptr Function\n", "",
                          "%tmpStruct          = OpExtInst %struct_fi %std450 FrexpStruct %arg1\n"
                          "                      OpStore %tmpStructPtr %tmpStruct\n"
                          "%tmpLoc             = OpAccessChain %type_float_fptr %tmpStructPtr %c_i32_0\n"
                          "%result             = OpLoad %type_float %tmpLoc\n",
                          B_STATEMENT_USAGE_TYPES_TYPE_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT,
                          {"struct_fi", "tmpStruct", "tmpLoc", "result"});
    mo[OID_LENGTH] =
        Op("length", FLOAT_ARITHMETIC, "%result             = OpExtInst %type_float %std450 Length %arg1\n",
           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
    mo[OID_NORMALIZE] = Op("normalize", FLOAT_ARITHMETIC,
                           "%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %c_float_2\n"
                           "%tmpVec             = OpExtInst %type_float_vec2 %std450 Normalize %vec1\n"
                           "%result             = OpCompositeExtract %type_float %tmpVec 0\n",
                           B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT,
                           {"vec1", "tmpVec", "result"});
    mo[OID_REFLECT]   = Op("reflect", FLOAT_ARITHMETIC,
                           "%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                             "%vecN               = OpCompositeConstruct %type_float_vec2 %c_float_0 %c_float_n1\n"
                             "%tmpVec             = OpExtInst %type_float_vec2 %std450 Reflect %vec1 %vecN\n"
                             "%result             = OpCompositeExtract %type_float %tmpVec 0\n",
                           B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT,
                           {"vec1", "vecN", "tmpVec", "result"});
    mo[OID_REFRACT]   = Op("refract", FLOAT_ARITHMETIC,
                           "%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                             "%vecN               = OpCompositeConstruct %type_float_vec2 %c_float_0 %c_float_n1\n"
                             "%tmpVec             = OpExtInst %type_float_vec2 %std450 Refract %vec1 %vecN %c_float_0_5\n"
                             "%result             = OpCompositeExtract %type_float %tmpVec 0\n",
                           B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT,
                           {"vec1", "vecN", "tmpVec", "result"});
    mo[OID_MAT_DET]   = Op("mat_det", FLOAT_ARITHMETIC,
                           "%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
                             "%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
                             "%result             = OpExtInst %type_float %std450 Determinant %mat\n",
                           B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT, {"col", "mat", "result"});
    mo[OID_MAT_INV]   = Op("mat_inv", FLOAT_ARITHMETIC,
                           "%col1               = OpCompositeConstruct %type_float_vec2 %arg1 %c_float_1\n"
                             "%col2               = OpCompositeConstruct %type_float_vec2 %c_float_1 %c_float_1\n"
                             "%mat                = OpCompositeConstruct %type_float_mat2x2 %col1 %col2\n"
                             "%invMat             = OpExtInst %type_float_mat2x2 %std450 MatrixInverse %mat\n"
                             "%extCol             = OpCompositeExtract %type_float_vec2 %invMat 1\n"
                             "%result             = OpCompositeExtract %type_float %extCol 1\n",
                           B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT,
                           {"col1", "col2", "mat", "invMat", "result"});
}

void TestCasesBuilder::build(vector<OperationTestCase> &testCases, TypeTestResultsSP typeTestResults)
{
    bool isFP16 = typeTestResults->floatType() == FP16;

    for (auto it = typeTestResults->testCaseInputs.begin(); it != typeTestResults->testCaseInputs.end(); it++)
    {
        string OTCname = "op_testedWithout_" + getBehaviourName(it->testedFlagBits, "_OR_") + "_arg1_" +
                         getValueName(it->operandFirst) + "_arg2_" + getValueName(it->operandSecond) + "_res_" +
                         getValueName(it->result);

        testCases.push_back(OTC((OTCname + "_exec").c_str(), invert(it->testedFlagBits), false, it->operationId,
                                it->operandFirst, it->operandSecond, it->result, isFP16, it->requireRte));
        testCases.push_back(OTC((OTCname + "_deco").c_str(), invert(it->testedFlagBits), true, it->operationId,
                                it->operandFirst, it->operandSecond, it->result, isFP16, it->requireRte));
    }

    // test None, AllowTransform and AllowRecip gramatically
    testCases.push_back(
        OTC("op_None_exec_grammar_test", FP::MaskNone, false, OID_ADD, V_MAX, V_HUGE, V_INF, isFP16, true));
    testCases.push_back(OTC("op_AllowTransform_OR_AllowReassoc_OR_AllowContract_exec_grammar_test",
                            FP::AllowTransform | FP::AllowReassoc | FP::AllowContract, false, OID_ADD, V_MAX, V_HUGE,
                            V_INF, isFP16, true));
    // the test for AllowRecip gives the same result with or without the flag
    testCases.push_back(
        OTC("op_AllowRecip_exec_grammar_test", FP::AllowRecip, false, OID_DIV, V_ONE, V_TWO, V_HALF, isFP16));
}

const Operation &TestCasesBuilder::getOperation(OperationId id) const
{
    return m_operations.at(id);
}

template <typename TYPE, typename FLOAT_TYPE>
bool valMatches(const TYPE &ret, ValueId expected)
{
    TypeValues<FLOAT_TYPE> typeValues;

    if (expected == V_NAN && ret.isNaN())
        return true;

    typename RawConvert<FLOAT_TYPE, typename TYPE::StorageType>::Value val;
    val.fp = typeValues.getValue(expected);
    return ret.bits() == val.ui;
}

template <typename TYPE, typename FLOAT_TYPE>
bool isEither(const TYPE &returnedFloat, ValueId expected1, ValueId expected2, TestLog &log)
{
    TypeValues<FLOAT_TYPE> typeValues;

    if (valMatches<TYPE, FLOAT_TYPE>(returnedFloat, expected1) ||
        valMatches<TYPE, FLOAT_TYPE>(returnedFloat, expected2))
        return true;

    typename RawConvert<FLOAT_TYPE, typename TYPE::StorageType>::Value val1, val2;
    val1.fp = typeValues.getValue(expected1);
    val2.fp = typeValues.getValue(expected2);

    log << TestLog::Message << "Expected " << toHex(val1.ui) << " (" << val1.fp << ")"
        << " or " << toHex(val2.ui) << " (" << val2.fp << ")" << TestLog::EndMessage;
    return false;
}

template <typename TYPE>
bool isTrigULPResultCorrect(const TYPE &returnedFloat, ValueId expected, TestLog &log)
{
    // The trig ULP results are used for things like the inverse trig functions. The spec gives
    // precisions for these based on atan, so that precision is used here.

    // This functions doesn't give correct results for fp64 at present, but this is never used.
    assert(returnedFloat.MANTISSA_BITS == 23 || returnedFloat.MANTISSA_BITS == 10);

    FloatFormat fp32Format(-126, 127, 23, true, tcu::MAYBE, tcu::YES, tcu::MAYBE);
    FloatFormat fp16Format(-14, 15, 10, true, tcu::MAYBE);

    const FloatFormat *fmt = (returnedFloat.MANTISSA_BITS == 10) ? &fp16Format : &fp32Format;

    // The ULP range is based on the exact result, which we approximate using the double value.
    TypeValues<double> typeValues;
    const double ref = typeValues.getValue(expected);
    uint32_t ulp     = (returnedFloat.MANTISSA_BITS == 10) ? 5 : 4096;

    double precision = fmt->ulp(ref, ulp);

    if (deAbs(returnedFloat.asDouble() - ref) < precision)
        return true;

    log << TestLog::Message << "Expected result to be in range"
        << " (" << ref - precision << ", " << ref + precision << "), got " << returnedFloat.asDouble()
        << TestLog::EndMessage;
    return false;
}

template <typename TYPE>
bool isTrigAbsResultCorrect(const TYPE &returnedFloat, TestLog &log)
{
    // for cos(x) with x between -pi and pi, the precision error is 2^-11 for fp32 and 2^-7 for fp16.
    double precision      = returnedFloat.MANTISSA_BITS == 23 ? dePow(2, -11) : dePow(2, -7);
    const double expected = 1.0;

    if (deAbs(returnedFloat.asDouble() - expected) < precision)
        return true;

    log << TestLog::Message << "Expected result to be in range"
        << " (" << expected - precision << ", " << expected + precision << "), got " << returnedFloat.asDouble()
        << TestLog::EndMessage;
    return false;
}

// Function used to compare test result with expected output.
// TYPE can be Float16, Float32 or Float64.
// FLOAT_TYPE can be deFloat16, float, double.
template <typename TYPE, typename FLOAT_TYPE>
bool compareBytes(vector<uint8_t> &expectedBytes, AllocationSp outputAlloc, TestLog &log)
{
    const TYPE *returned = static_cast<const TYPE *>(outputAlloc->getHostPtr());
    const TYPE *fValueId = reinterpret_cast<const TYPE *>(&expectedBytes.front());

    // all test return single value
    // Fp16 nostorage tests get their values from a uint32_t value, but we create the
    // buffer with the same size for both cases: 4 bytes.
    if (sizeof(TYPE) == 2u)
        DE_ASSERT((expectedBytes.size() / sizeof(TYPE)) == 2);
    else
        DE_ASSERT((expectedBytes.size() / sizeof(TYPE)) == 1);

    // during test setup we do not store expected value but id that can be used to
    // retrieve actual value - this is done to handle special cases like multiple
    // allowed results or epsilon checks for some cases
    // note that this is workaround - this should be done by changing
    // ComputerShaderCase and GraphicsShaderCase so that additional arguments can
    // be passed to this verification callback
    typedef typename TYPE::StorageType SType;
    SType expectedInt       = fValueId[0].bits();
    ValueId expectedValueId = static_cast<ValueId>(expectedInt);

    // something went wrong, expected value cant be V_UNUSED,
    // if this is the case then test shouldn't be created at all
    DE_ASSERT(expectedValueId != V_UNUSED);

    TYPE returnedFloat = returned[0];

    log << TestLog::Message << "Calculated result: " << toHex(returnedFloat.bits()) << " (" << returnedFloat.asFloat()
        << ")" << TestLog::EndMessage;

    // handle multiple acceptable results cases
    if (expectedValueId == V_SIGN_NAN)
    {
        if (valMatches<TYPE, FLOAT_TYPE>(returnedFloat, V_MINUS_ONE) ||
            valMatches<TYPE, FLOAT_TYPE>(returnedFloat, V_MINUS_ZERO) ||
            valMatches<TYPE, FLOAT_TYPE>(returnedFloat, V_ZERO) || valMatches<TYPE, FLOAT_TYPE>(returnedFloat, V_ONE))
            return true;

        log << TestLog::Message << "Expected -1, -0, +0 or +1" << TestLog::EndMessage;
        return false;
    }

    if (expectedValueId == V_ZERO_OR_MINUS_ZERO)
        return isEither<TYPE, FLOAT_TYPE>(returnedFloat, V_ZERO, V_MINUS_ZERO, log);

    if (expectedValueId == V_ZERO_OR_ONE)
        return isEither<TYPE, FLOAT_TYPE>(returnedFloat, V_ZERO, V_ONE, log);

    if (expectedValueId == V_ONE_OR_NAN)
        return isEither<TYPE, FLOAT_TYPE>(returnedFloat, V_ONE, V_NAN, log);

    // handle trigonometric operations precision errors
    if (expectedValueId == V_TRIG_ONE)
        return isTrigAbsResultCorrect<TYPE>(returnedFloat, log);

    // handle cases with large ULP precision bounds.
    if (expectedValueId == V_PI || expectedValueId == V_MINUS_PI || expectedValueId == V_PI_DIV_2 ||
        expectedValueId == V_MINUS_PI_DIV_2 || expectedValueId == V_PI_DIV_4 || expectedValueId == V_MINUS_PI_DIV_4 ||
        expectedValueId == V_3_PI_DIV_4 || expectedValueId == V_MINUS_3_PI_DIV_4)
        return isTrigULPResultCorrect(returnedFloat, expectedValueId, log);

    if (valMatches<TYPE, FLOAT_TYPE>(returnedFloat, expectedValueId))
        return true;

    TypeValues<FLOAT_TYPE> typeValues;

    typename RawConvert<FLOAT_TYPE, SType>::Value value;
    value.fp = typeValues.getValue(expectedValueId);

    log << TestLog::Message << "Expected " << toHex(value.ui) << " (" << value.fp << ")" << TestLog::EndMessage;
    return false;
}

template <typename TYPE, typename FLOAT_TYPE>
bool checkFloats(const vector<Resource> &, const vector<AllocationSp> &outputAllocs,
                 const vector<Resource> &expectedOutputs, TestLog &log)
{
    if (outputAllocs.size() != expectedOutputs.size())
        return false;

    for (uint32_t outputNdx = 0; outputNdx < outputAllocs.size(); ++outputNdx)
    {
        vector<uint8_t> expectedBytes;
        expectedOutputs[outputNdx].getBytes(expectedBytes);

        if (!compareBytes<TYPE, FLOAT_TYPE>(expectedBytes, outputAllocs[outputNdx], log))
            return false;
    }

    return true;
}

// Base class for ComputeTestGroupBuilder and GrephicstestGroupBuilder classes.
// It contains all functionalities that are used by both child classes.
class TestGroupBuilderBase
{
public:
    TestGroupBuilderBase();
    virtual ~TestGroupBuilderBase() = default;

    virtual void createOperationTests(TestCaseGroup *parentGroup, const char *groupName, FloatType floatType,
                                      bool argumentsFromInput) = 0;

protected:
    typedef vector<OperationTestCase> TestCaseVect;

    // Structure containing all data required to create single operation test.
    struct OperationTestCaseInfo
    {
        FloatType outFloatType;
        bool argumentsFromInput;
        VkShaderStageFlagBits testedStage;
        const Operation &operation;
        const OperationTestCase &testCase;
    };

    void specializeOperation(const OperationTestCaseInfo &testCaseInfo,
                             SpecializedOperation &specializedOperation) const;

    void getBehaviorCapabilityAndExecutionModeDecoration(FP behaviorFlagsExecMode, FP behaviorFlagsDecoration,
                                                         bool useDecorationFlags, vector<string> IDsToDecorate,
                                                         const string inBitWidth, string &capability,
                                                         string &executionMode, string &decoration,
                                                         string &constant) const;

    void FillFloatControlsProperties(vk::VkPhysicalDeviceFloatControlsProperties &fc, const OperationTestCase &testCase,
                                     FloatType floatType) const;

protected:
    struct TypeData
    {
        TypeValuesSP values;
        TypeSnippetsSP snippets;
        TypeTestResultsSP testResults;
    };

    // Type specific parameters are stored in this map.
    map<FloatType, TypeData> m_typeData;
};

TestGroupBuilderBase::TestGroupBuilderBase()
{
    m_typeData[FP16]             = TypeData();
    m_typeData[FP16].values      = TypeValuesSP(new TypeValues<deFloat16>);
    m_typeData[FP16].snippets    = TypeSnippetsSP(new TypeSnippets<deFloat16>);
    m_typeData[FP16].testResults = TypeTestResultsSP(new TypeTestResults<deFloat16>);
    m_typeData[FP32]             = TypeData();
    m_typeData[FP32].values      = TypeValuesSP(new TypeValues<float>);
    m_typeData[FP32].snippets    = TypeSnippetsSP(new TypeSnippets<float>);
    m_typeData[FP32].testResults = TypeTestResultsSP(new TypeTestResults<float>);
    m_typeData[FP64]             = TypeData();
    m_typeData[FP64].values      = TypeValuesSP(new TypeValues<double>);
    m_typeData[FP64].snippets    = TypeSnippetsSP(new TypeSnippets<double>);
    m_typeData[FP64].testResults = TypeTestResultsSP(new TypeTestResults<double>);
}

void TestGroupBuilderBase::specializeOperation(const OperationTestCaseInfo &testCaseInfo,
                                               SpecializedOperation &specializedOperation) const
{
    const string typeToken  = "_float";
    const string widthToken = "${float_width}";

    FloatType outFloatType               = testCaseInfo.outFloatType;
    const Operation &operation           = testCaseInfo.operation;
    const TypeSnippetsSP outTypeSnippets = m_typeData.at(outFloatType).snippets;
    const bool inputRestricted           = operation.isInputTypeRestricted;
    FloatType inFloatType                = operation.restrictedInputType;

    // usually input type is same as output but this is not the case for conversion
    // operations; in those cases operation definitions have restricted input type
    inFloatType = inputRestricted ? inFloatType : outFloatType;

    TypeSnippetsSP inTypeSnippets = m_typeData.at(inFloatType).snippets;

    const string inTypePrefix  = string("_f") + inTypeSnippets->bitWidth;
    const string outTypePrefix = string("_f") + outTypeSnippets->bitWidth;

    std::string byteWidthToken = std::to_string(std::stoi(outTypeSnippets->bitWidth) / 8);

    specializedOperation.constants   = replace(operation.constants, typeToken, inTypePrefix);
    specializedOperation.annotations = replace(operation.annotations, widthToken, byteWidthToken);
    specializedOperation.types       = replace(operation.types, typeToken, outTypePrefix);
    specializedOperation.variables   = replace(operation.variables, typeToken, outTypePrefix);
    specializedOperation.functions   = replace(operation.functions, typeToken, outTypePrefix);
    specializedOperation.commands    = replace(operation.commands, typeToken, outTypePrefix);

    specializedOperation.inFloatType                = inFloatType;
    specializedOperation.inTypeSnippets             = inTypeSnippets;
    specializedOperation.outTypeSnippets            = outTypeSnippets;
    specializedOperation.argumentsUsesFloatConstant = 0;

    if (operation.isSpecConstant)
        return;

    // select way arguments are prepared
    if (testCaseInfo.argumentsFromInput)
    {
        // read arguments from input SSBO in main function
        specializedOperation.arguments = inTypeSnippets->argumentsFromInputSnippet;

        if (inFloatType == FP16 && testCaseInfo.testCase.fp16Without16BitStorage)
            specializedOperation.arguments = inTypeSnippets->argumentsFromInputFp16Snippet;
    }
    else
    {
        // generate proper values in main function
        const string arg1 = "%arg1                 = ";
        const string arg2 = "%arg2                 = ";

        const ValueId *inputArguments = testCaseInfo.testCase.input;
        if (inputArguments[0] != V_UNUSED)
        {
            specializedOperation.arguments = arg1 + inTypeSnippets->valueIdToSnippetArgMap.at(inputArguments[0]);
            specializedOperation.argumentsUsesFloatConstant |= B_STATEMENT_USAGE_ARGS_CONST_FLOAT;
        }
        if (inputArguments[1] != V_UNUSED)
        {
            specializedOperation.arguments += arg2 + inTypeSnippets->valueIdToSnippetArgMap.at(inputArguments[1]);
            specializedOperation.argumentsUsesFloatConstant |= B_STATEMENT_USAGE_ARGS_CONST_FLOAT;
        }
    }
}

void TestGroupBuilderBase::getBehaviorCapabilityAndExecutionModeDecoration(
    FP behaviorFlagsExecMode, FP behaviorFlagsDecoration, bool useDecorationFlags, vector<string> IDsToDecorate,
    const string inBitWidth, string &capability, string &executionMode, string &decoration, string &constant) const
{
    capability += "OpCapability FloatControls2\n";
    constant += "%bc_u32_fp_exec_mode = OpConstant %type_u32 " + to_string((unsigned)behaviorFlagsExecMode) + "\n";
    executionMode += "OpExecutionModeId %main FPFastMathDefault %type_f" + inBitWidth + " %bc_u32_fp_exec_mode\n";

    if (useDecorationFlags)
        for (size_t i = 0; i < IDsToDecorate.size(); i++)
        {
            decoration.append("OpDecorate %").append(IDsToDecorate[i]).append(" FPFastMathMode ");
            decoration += getBehaviourName(behaviorFlagsDecoration, "|");
            decoration += "\n";
        }

    DE_ASSERT(!capability.empty() && !executionMode.empty());
}

void TestGroupBuilderBase::FillFloatControlsProperties(vk::VkPhysicalDeviceFloatControlsProperties &fc,
                                                       const OperationTestCase &testCase, FloatType floatType) const
{
    const FP SZInfNaNPreserveBits = (FP::NSZ | FP::NotInf | FP::NotNaN);
    bool requiresSZInfNaNPreserve;
    requiresSZInfNaNPreserve = ((testCase.behaviorFlagsExecMode & SZInfNaNPreserveBits) != SZInfNaNPreserveBits) ||
                               ((testCase.behaviorFlagsDecoration & SZInfNaNPreserveBits) != SZInfNaNPreserveBits);

    switch (floatType)
    {
    case FP16:
        fc.shaderSignedZeroInfNanPreserveFloat16 = requiresSZInfNaNPreserve;
        break;
    case FP32:
        fc.shaderSignedZeroInfNanPreserveFloat32 = requiresSZInfNaNPreserve;
        break;
    case FP64:
        fc.shaderSignedZeroInfNanPreserveFloat64 = requiresSZInfNaNPreserve;
        break;
    }

    switch (floatType)
    {
    case FP16:
        fc.shaderRoundingModeRTEFloat16 = testCase.requireRte;
        break;
    case FP32:
        fc.shaderRoundingModeRTEFloat32 = testCase.requireRte;
        break;
    case FP64:
        fc.shaderRoundingModeRTEFloat64 = testCase.requireRte;
        break;
    }
}

// ComputeTestGroupBuilder contains logic that creates compute shaders
// for all test cases. As most tests in spirv-assembly it uses functionality
// implemented in vktSpvAsmComputeShaderTestUtil.cpp.
class ComputeTestGroupBuilder : public TestGroupBuilderBase
{
public:
    void init();

    void createOperationTests(TestCaseGroup *parentGroup, const char *groupName, FloatType floatType,
                              bool argumentsFromInput) override;

protected:
    void fillShaderSpec(const OperationTestCaseInfo &testCaseInfo, ComputeShaderSpec &csSpec) const;

private:
    StringTemplate m_operationShaderTemplate;
    TestCasesBuilder m_operationTestCaseBuilder;
};

void ComputeTestGroupBuilder::init()
{
    m_operationTestCaseBuilder.init();

    // generic compute shader template with common code for all
    // float types and all possible operations listed in OperationId enum
    m_operationShaderTemplate.setString("OpCapability Shader\n"
                                        "${capabilities}"

                                        "OpExtension \"SPV_KHR_float_controls2\"\n"
                                        "${extensions}"

                                        "%std450            = OpExtInstImport \"GLSL.std.450\"\n"
                                        "OpMemoryModel Logical GLSL450\n"
                                        "OpEntryPoint GLCompute %main \"main\" %id\n"
                                        "OpExecutionMode %main LocalSize 1 1 1\n"
                                        "${execution_mode}"

                                        "OpDecorate %id BuiltIn GlobalInvocationId\n"

                                        "${decorations}"

                                        // some tests require additional annotations
                                        "${annotations}"

                                        "%type_void            = OpTypeVoid\n"
                                        "%type_voidf           = OpTypeFunction %type_void\n"
                                        "%type_bool            = OpTypeBool\n"
                                        "%type_u32             = OpTypeInt 32 0\n"
                                        "%type_i32             = OpTypeInt 32 1\n"
                                        "%type_i32_fptr        = OpTypePointer Function %type_i32\n"
                                        "%type_u32_vec2        = OpTypeVector %type_u32 2\n"
                                        "%type_u32_vec3        = OpTypeVector %type_u32 3\n"
                                        "%type_u32_vec3_ptr    = OpTypePointer Input %type_u32_vec3\n"

                                        "%c_i32_0              = OpConstant %type_i32 0\n"
                                        "%c_i32_1              = OpConstant %type_i32 1\n"
                                        "%c_i32_2              = OpConstant %type_i32 2\n"

                                        // if input float type has different width then output then
                                        // both types are defined here along with all types derived from
                                        // them that are commonly used by tests; some tests also define
                                        // their own types (those that are needed just by this single test)
                                        "${types}"

                                        // SSBO definitions
                                        "${io_definitions}"

                                        "%id                   = OpVariable %type_u32_vec3_ptr Input\n"

                                        // set of default constants per float type is placed here,
                                        // operation tests can also define additional constants.
                                        "${constants}"
                                        "${behaviorConstants}"

                                        // O_RETURN_VAL defines function here and becouse
                                        // of that this token needs to be directly before main function
                                        "${functions}"

                                        "%main                 = OpFunction %type_void None %type_voidf\n"
                                        "%label                = OpLabel\n"

                                        "${variables}"

                                        // depending on test case arguments are either read from input ssbo
                                        // or generated in spir-v code - in later case shader input is not used
                                        "${arguments}"

                                        // perform test commands
                                        "${commands}"

                                        // save result to SSBO
                                        "${save_result}"

                                        "OpReturn\n"
                                        "OpFunctionEnd\n");
}

void ComputeTestGroupBuilder::createOperationTests(TestCaseGroup *parentGroup, const char *groupName,
                                                   FloatType floatType, bool argumentsFromInput)
{
    TestContext &testCtx = parentGroup->getTestContext();
    TestCaseGroup *group = new TestCaseGroup(testCtx, groupName, "");
    parentGroup->addChild(group);

    TestCaseVect testCases;
    m_operationTestCaseBuilder.build(testCases, m_typeData[floatType].testResults);

    for (auto &testCase : testCases)
    {
        // skip cases with undefined output
        if (testCase.expectedOutput == V_UNUSED)
            continue;

        OperationTestCaseInfo testCaseInfo = {floatType, argumentsFromInput, VK_SHADER_STAGE_COMPUTE_BIT,
                                              m_operationTestCaseBuilder.getOperation(testCase.operationId), testCase};

        ComputeShaderSpec csSpec;

        fillShaderSpec(testCaseInfo, csSpec);

        string testName = replace(testCase.baseName, "op", testCaseInfo.operation.name);
        group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), csSpec));
    }
}

void ComputeTestGroupBuilder::fillShaderSpec(const OperationTestCaseInfo &testCaseInfo, ComputeShaderSpec &csSpec) const
{
    // LUT storing functions used to verify test results
    const VerifyIOFunc checkFloatsLUT[] = {checkFloats<Float16, deFloat16>, checkFloats<Float32, float>,
                                           checkFloats<Float64, double>};

    const Operation &testOperation    = testCaseInfo.operation;
    const OperationTestCase &testCase = testCaseInfo.testCase;
    FloatType outFloatType            = testCaseInfo.outFloatType;

    SpecializedOperation specOpData;
    specializeOperation(testCaseInfo, specOpData);

    TypeSnippetsSP inTypeSnippets  = specOpData.inTypeSnippets;
    TypeSnippetsSP outTypeSnippets = specOpData.outTypeSnippets;
    FloatType inFloatType          = specOpData.inFloatType;

    bool outFp16WithoutStorage = (outFloatType == FP16) && testCase.fp16Without16BitStorage;
    bool inFp16WithoutStorage  = (inFloatType == FP16) && testCase.fp16Without16BitStorage;

    // UnpackHalf2x16 is a corner case - it returns two 32-bit floats but
    // internaly operates on fp16 and this type should be used by float controls
    string inFloatWidthForCaps = inTypeSnippets->bitWidth;
    string behaviorCapability;
    string behaviorExecutionMode;
    string behaviorDecorations;
    string behaviorConstants;
    getBehaviorCapabilityAndExecutionModeDecoration(testCase.behaviorFlagsExecMode, testCase.behaviorFlagsDecoration,
                                                    testCase.useDecorationFlags, testOperation.IDsToDecorate,
                                                    inFloatWidthForCaps, behaviorCapability, behaviorExecutionMode,
                                                    behaviorDecorations, behaviorConstants);

    string capabilities = behaviorCapability + outTypeSnippets->capabilities;
    string extensions   = outTypeSnippets->extensions;
    string annotations  = inTypeSnippets->inputAnnotationsSnippet + outTypeSnippets->outputAnnotationsSnippet +
                         outTypeSnippets->typeAnnotationsSnippet;
    string types         = outTypeSnippets->typeDefinitionsSnippet;
    string constants     = outTypeSnippets->constantsDefinitionsSnippet;
    string ioDefinitions = "";

    // Getting rid of 16bit_storage dependency imply replacing lots of snippets.
    {
        if (inFp16WithoutStorage)
            ioDefinitions = inTypeSnippets->inputDefinitionsFp16Snippet;
        else
            ioDefinitions = inTypeSnippets->inputDefinitionsSnippet;

        if (outFp16WithoutStorage)
        {
            extensions   = outTypeSnippets->extensionsFp16Without16BitStorage;
            capabilities = behaviorCapability + outTypeSnippets->capabilitiesFp16Without16BitStorage;
            types += outTypeSnippets->typeDefinitionsFp16Snippet;
            annotations += outTypeSnippets->typeAnnotationsFp16Snippet;
            ioDefinitions += outTypeSnippets->outputDefinitionsFp16Snippet;
        }
        else
            ioDefinitions += outTypeSnippets->outputDefinitionsSnippet;
    }

    bool outFp16TypeUsage = outTypeSnippets->loadStoreRequiresShaderFloat16;
    bool inFp16TypeUsage  = false;

    if (testOperation.isInputTypeRestricted)
    {
        annotations += inTypeSnippets->typeAnnotationsSnippet;
        types += inTypeSnippets->typeDefinitionsSnippet;
        constants += inTypeSnippets->constantsDefinitionsSnippet;

        if (inFp16WithoutStorage)
        {
            annotations += inTypeSnippets->typeAnnotationsFp16Snippet;
            types += inTypeSnippets->typeDefinitionsFp16Snippet;
            capabilities += inTypeSnippets->capabilitiesFp16Without16BitStorage;
            extensions += inTypeSnippets->extensionsFp16Without16BitStorage;
        }
        else
        {
            capabilities += inTypeSnippets->capabilities;
            extensions += inTypeSnippets->extensions;
        }

        inFp16TypeUsage = inTypeSnippets->loadStoreRequiresShaderFloat16;
    }

    map<string, string> specializations;
    specializations["behaviorConstants"] = behaviorConstants;
    specializations["decorations"]       = behaviorDecorations;
    specializations["annotations"]       = annotations + specOpData.annotations;
    specializations["types"]             = types + specOpData.types;
    specializations["io_definitions"]    = ioDefinitions;
    specializations["variables"]         = specOpData.variables;
    specializations["functions"]         = specOpData.functions;
    specializations["save_result"] =
        (outFp16WithoutStorage ? outTypeSnippets->storeResultsFp16Snippet : outTypeSnippets->storeResultsSnippet);
    specializations["arguments"] = specOpData.arguments;
    specializations["commands"]  = specOpData.commands;

    // Build constants. They are only needed sometimes.
    const FloatStatementUsageFlags argsAnyFloatConstMask =
        B_STATEMENT_USAGE_ARGS_CONST_FLOAT | B_STATEMENT_USAGE_ARGS_CONST_FP16 | B_STATEMENT_USAGE_ARGS_CONST_FP32 |
        B_STATEMENT_USAGE_ARGS_CONST_FP64;
    const bool argsUseFPConstants = (specOpData.argumentsUsesFloatConstant & argsAnyFloatConstMask) != 0;
    const FloatStatementUsageFlags commandsAnyFloatConstMask =
        B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_CONST_FP16 |
        B_STATEMENT_USAGE_COMMANDS_CONST_FP32 | B_STATEMENT_USAGE_COMMANDS_CONST_FP64;
    const bool commandsUseFPConstants = (testCaseInfo.operation.statementUsageFlags & commandsAnyFloatConstMask) != 0;
    const bool needConstants          = argsUseFPConstants || commandsUseFPConstants;
    const FloatStatementUsageFlags constsFloatTypeMask =
        B_STATEMENT_USAGE_CONSTS_TYPE_FLOAT | B_STATEMENT_USAGE_CONSTS_TYPE_FP16;
    const bool constsUsesFP16Type             = (testCaseInfo.operation.statementUsageFlags & constsFloatTypeMask) != 0;
    const bool loadStoreRequiresShaderFloat16 = inFp16TypeUsage || outFp16TypeUsage;
    const bool usesFP16Constants              = constsUsesFP16Type || (needConstants && loadStoreRequiresShaderFloat16);

    specializations["constants"] = "";
    if (needConstants || outFp16WithoutStorage)
    {
        specializations["constants"] = constants;
    }
    specializations["constants"] += specOpData.constants;

    // check which format features are needed
    bool float16FeatureRequired = (outFloatType == FP16) || (inFloatType == FP16);
    bool float64FeatureRequired = (outFloatType == FP64) || (inFloatType == FP64);

    // Determine required capabilities.
    bool float16CapabilityAlreadyAdded = inFp16WithoutStorage || outFp16WithoutStorage;
    if ((testOperation.floatUsage == FLOAT_ARITHMETIC && float16FeatureRequired && !float16CapabilityAlreadyAdded) ||
        usesFP16Constants)
    {
        capabilities += "OpCapability Float16\n";
    }

    if (testCase.requireRte)
    {
        extensions += "OpExtension \"SPV_KHR_float_controls\"\n";
        capabilities += "OpCapability RoundingModeRTE\n";
        behaviorExecutionMode += "OpExecutionMode %main RoundingModeRTE " + inTypeSnippets->bitWidth + "\n";
    }

    specializations["execution_mode"] = behaviorExecutionMode;
    specializations["extensions"]     = extensions;
    specializations["capabilities"]   = capabilities;

    // specialize shader
    const string shaderCode = m_operationShaderTemplate.specialize(specializations);

    // construct input and output buffers of proper types
    TypeValuesSP inTypeValues  = m_typeData.at(inFloatType).values;
    TypeValuesSP outTypeValues = m_typeData.at(outFloatType).values;
    BufferSp inBufferSp        = inTypeValues->constructInputBuffer(testCase.input);
    BufferSp outBufferSp       = outTypeValues->constructOutputBuffer(testCase.expectedOutput);
    csSpec.inputs.push_back(Resource(inBufferSp, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
    csSpec.outputs.push_back(Resource(outBufferSp));

    csSpec.assembly      = shaderCode;
    csSpec.numWorkGroups = IVec3(1, 1, 1);
    csSpec.verifyIO      = checkFloatsLUT[outFloatType];

    csSpec.spirvVersion                                       = SPIRV_VERSION_1_2;
    csSpec.requestedVulkanFeatures.coreFeatures.shaderFloat64 = float64FeatureRequired;
    csSpec.requestedVulkanFeatures.ext16BitStorage.storageBuffer16BitAccess =
        float16FeatureRequired && !testCase.fp16Without16BitStorage;
    csSpec.requestedVulkanFeatures.ext16BitStorage.uniformAndStorageBuffer16BitAccess =
        csSpec.requestedVulkanFeatures.ext16BitStorage.storageBuffer16BitAccess;
    csSpec.requestedVulkanFeatures.extFloat16Int8.shaderFloat16 =
        float16CapabilityAlreadyAdded || usesFP16Constants ||
        (float16FeatureRequired && !testCase.fp16Without16BitStorage && testOperation.floatUsage == FLOAT_ARITHMETIC);
    csSpec.requestedVulkanFeatures.extFloatControls2.shaderFloatControls2 = true;

    // Float controls 2 still requires that the original float controls properties are supported
    FillFloatControlsProperties(csSpec.requestedVulkanFeatures.floatControlsProperties, testCase, inFloatType);
}

void getGraphicsShaderCode(vk::SourceCollections &dst, InstanceContext context)
{
    // this function is used only by GraphicsTestGroupBuilder but it couldn't
    // be implemented as a method because of how addFunctionCaseWithPrograms
    // was implemented

    SpirvVersion targetSpirvVersion = context.resources.spirvVersion;
    const uint32_t vulkanVersion    = dst.usedVulkanVersion;

    static const string vertexTemplate =
        "OpCapability Shader\n"
        "${vert_capabilities}"

        "OpExtension \"SPV_KHR_float_controls2\"\n"
        "${vert_extensions}"

        "%std450            = OpExtInstImport \"GLSL.std.450\"\n"
        "OpMemoryModel Logical GLSL450\n"
        "OpEntryPoint Vertex %main \"main\" %BP_stream %BP_position %BP_color %BP_gl_VertexIndex %BP_gl_InstanceIndex "
        "%BP_vertex_color %BP_vertex_result \n"
        "${vert_execution_mode}"

        "OpMemberDecorate %BP_gl_PerVertex 0 BuiltIn Position\n"
        "OpMemberDecorate %BP_gl_PerVertex 1 BuiltIn PointSize\n"
        "OpMemberDecorate %BP_gl_PerVertex 2 BuiltIn ClipDistance\n"
        "OpMemberDecorate %BP_gl_PerVertex 3 BuiltIn CullDistance\n"
        "OpDecorate %BP_gl_PerVertex Block\n"
        "OpDecorate %BP_position Location 0\n"
        "OpDecorate %BP_color Location 1\n"
        "OpDecorate %BP_vertex_color Location 1\n"
        "OpDecorate %BP_vertex_result Location 2\n"
        "OpDecorate %BP_vertex_result Flat\n"
        "OpDecorate %BP_gl_VertexIndex BuiltIn VertexIndex\n"
        "OpDecorate %BP_gl_InstanceIndex BuiltIn InstanceIndex\n"
        "${vert_decorations}"

        // some tests require additional annotations
        "${vert_annotations}"

        // types required by most of tests
        "%type_void            = OpTypeVoid\n"
        "%type_voidf           = OpTypeFunction %type_void\n"
        "%type_bool            = OpTypeBool\n"
        "%type_i32             = OpTypeInt 32 1\n"
        "%type_u32             = OpTypeInt 32 0\n"
        "%type_u32_vec2        = OpTypeVector %type_u32 2\n"
        "%type_i32_iptr        = OpTypePointer Input %type_i32\n"
        "%type_i32_optr        = OpTypePointer Output %type_i32\n"
        "%type_i32_fptr        = OpTypePointer Function %type_i32\n"

        // constants required by most of tests
        "%c_i32_0              = OpConstant %type_i32 0\n"
        "%c_i32_1              = OpConstant %type_i32 1\n"
        "%c_i32_2              = OpConstant %type_i32 2\n"
        "%c_u32_1              = OpConstant %type_u32 1\n"

        // if input float type has different width then output then
        // both types are defined here along with all types derived from
        // them that are commonly used by tests; some tests also define
        // their own types (those that are needed just by this single test)
        "${vert_types}"

        // SSBO is not universally supported for storing
        // data in vertex stages - it is onle read here
        "${vert_io_definitions}"

        "%BP_gl_PerVertex      = OpTypeStruct %type_f32_vec4 %type_f32 %type_f32_arr_1 %type_f32_arr_1\n"
        "%BP_gl_PerVertex_optr = OpTypePointer Output %BP_gl_PerVertex\n"
        "%BP_stream            = OpVariable %BP_gl_PerVertex_optr Output\n"
        "%BP_position          = OpVariable %type_f32_vec4_iptr Input\n"
        "%BP_color             = OpVariable %type_f32_vec4_iptr Input\n"
        "%BP_gl_VertexIndex    = OpVariable %type_i32_iptr Input\n"
        "%BP_gl_InstanceIndex  = OpVariable %type_i32_iptr Input\n"
        "%BP_vertex_color      = OpVariable %type_f32_vec4_optr Output\n"

        // set of default constants per float type is placed here,
        // operation tests can also define additional constants.
        "${vert_constants}"
        "${behaviorConstants}"

        // O_RETURN_VAL defines function here and because
        // of that this token needs to be directly before main function.
        "${vert_functions}"

        "%main                 = OpFunction %type_void None %type_voidf\n"
        "%label                = OpLabel\n"

        "${vert_variables}"

        "%position             = OpLoad %type_f32_vec4 %BP_position\n"
        "%gl_pos               = OpAccessChain %type_f32_vec4_optr %BP_stream %c_i32_0\n"
        "OpStore %gl_pos %position\n"
        "%color                = OpLoad %type_f32_vec4 %BP_color\n"
        "OpStore %BP_vertex_color %color\n"

        // this token is filled only when vertex stage is tested;
        // depending on test case arguments are either read from input ssbo
        // or generated in spir-v code - in later case ssbo is not used
        "${vert_arguments}"

        // when vertex shader is tested then test operations are performed
        // here and passed to fragment stage; if fragment stage ts tested
        // then ${comands} and ${vert_process_result} are rplaced with nop
        "${vert_commands}"

        "${vert_process_result}"

        "OpReturn\n"
        "OpFunctionEnd\n";

    static const string fragmentTemplate =
        "OpCapability Shader\n"
        "${frag_capabilities}"

        "OpExtension \"SPV_KHR_float_controls2\"\n"
        "${frag_extensions}"

        "%std450            = OpExtInstImport \"GLSL.std.450\"\n"
        "OpMemoryModel Logical GLSL450\n"
        "OpEntryPoint Fragment %main \"main\" %BP_vertex_color %BP_vertex_result %BP_fragColor %BP_gl_FragCoord \n"
        "OpExecutionMode %main OriginUpperLeft\n"
        "${frag_execution_mode}"

        "OpDecorate %BP_fragColor Location 0\n"
        "OpDecorate %BP_vertex_color Location 1\n"
        "OpDecorate %BP_vertex_result Location 2\n"
        "OpDecorate %BP_vertex_result Flat\n"
        "OpDecorate %BP_gl_FragCoord BuiltIn FragCoord\n"
        "${frag_decorations}"

        // some tests require additional annotations
        "${frag_annotations}"

        // types required by most of tests
        "%type_void            = OpTypeVoid\n"
        "%type_voidf           = OpTypeFunction %type_void\n"
        "%type_bool            = OpTypeBool\n"
        "%type_i32             = OpTypeInt 32 1\n"
        "%type_u32             = OpTypeInt 32 0\n"
        "%type_u32_vec2        = OpTypeVector %type_u32 2\n"
        "%type_i32_iptr        = OpTypePointer Input %type_i32\n"
        "%type_i32_optr        = OpTypePointer Output %type_i32\n"
        "%type_i32_fptr        = OpTypePointer Function %type_i32\n"

        // constants required by most of tests
        "%c_i32_0              = OpConstant %type_i32 0\n"
        "%c_i32_1              = OpConstant %type_i32 1\n"
        "%c_i32_2              = OpConstant %type_i32 2\n"
        "%c_u32_1              = OpConstant %type_u32 1\n"

        // if input float type has different width then output then
        // both types are defined here along with all types derived from
        // them that are commonly used by tests; some tests also define
        // their own types (those that are needed just by this single test)
        "${frag_types}"

        "%BP_gl_FragCoord      = OpVariable %type_f32_vec4_iptr Input\n"
        "%BP_vertex_color      = OpVariable %type_f32_vec4_iptr Input\n"
        "%BP_fragColor         = OpVariable %type_f32_vec4_optr Output\n"

        // SSBO definitions
        "${frag_io_definitions}"

        // set of default constants per float type is placed here,
        // operation tests can also define additional constants.
        "${frag_constants}"
        "${behaviorConstants}"

        // O_RETURN_VAL defines function here and because
        // of that this token needs to be directly before main function.
        "${frag_functions}"

        "%main                 = OpFunction %type_void None %type_voidf\n"
        "%label                = OpLabel\n"

        "${frag_variables}"

        // just pass vertex color - rendered image is not important in our case
        "%vertex_color         = OpLoad %type_f32_vec4 %BP_vertex_color\n"
        "OpStore %BP_fragColor %vertex_color\n"

        // this token is filled only when fragment stage is tested;
        // depending on test case arguments are either read from input ssbo or
        // generated in spir-v code - in later case ssbo is used only for output
        "${frag_arguments}"

        // when fragment shader is tested then test operations are performed
        // here and saved to ssbo; if vertex stage was tested then its
        // result is just saved to ssbo here
        "${frag_commands}"
        "${frag_process_result}"

        "OpReturn\n"
        "OpFunctionEnd\n";

    dst.spirvAsmSources.add("vert", nullptr) << StringTemplate(vertexTemplate).specialize(context.testCodeFragments)
                                             << SpirVAsmBuildOptions(vulkanVersion, targetSpirvVersion);
    dst.spirvAsmSources.add("frag", nullptr) << StringTemplate(fragmentTemplate).specialize(context.testCodeFragments)
                                             << SpirVAsmBuildOptions(vulkanVersion, targetSpirvVersion);
}

// GraphicsTestGroupBuilder iterates over all test cases and creates test for both
// vertex and fragment stages. As in most spirv-assembly tests, tests here are also
// executed using functionality defined in vktSpvAsmGraphicsShaderTestUtil.cpp but
// because one of requirements during development was that SSBO wont be used in
// vertex stage we couldn't use createTestForStage functions - we need a custom
// version for both vertex and fragmen shaders at the same time. This was required
// as we needed to pass result from vertex stage to fragment stage where it could
// be saved to ssbo. To achieve that InstanceContext is created manually in
// createInstanceContext method.
class GraphicsTestGroupBuilder : public TestGroupBuilderBase
{
public:
    void init();

    void createOperationTests(TestCaseGroup *parentGroup, const char *groupName, FloatType floatType,
                              bool argumentsFromInput) override;

protected:
    InstanceContext createInstanceContext(const OperationTestCaseInfo &testCaseInfo) const;

private:
    TestCasesBuilder m_testCaseBuilder;
};

void GraphicsTestGroupBuilder::init()
{
    m_testCaseBuilder.init();
}

void GraphicsTestGroupBuilder::createOperationTests(TestCaseGroup *parentGroup, const char *groupName,
                                                    FloatType floatType, bool argumentsFromInput)
{
    TestContext &testCtx = parentGroup->getTestContext();
    TestCaseGroup *group = new TestCaseGroup(testCtx, groupName, "");
    parentGroup->addChild(group);

    // create test cases for vertex stage
    TestCaseVect testCases;
    m_testCaseBuilder.build(testCases, m_typeData[floatType].testResults);

    for (auto &testCase : testCases)
    {
        // skip cases with undefined output
        if (testCase.expectedOutput == V_UNUSED)
            continue;

        VkShaderStageFlagBits stages[] = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};
        string stageNames[]            = {"_vert", "_frag"};
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(stages); i++)
        {
            OperationTestCaseInfo testCaseInfo = {floatType, argumentsFromInput, stages[i],
                                                  m_testCaseBuilder.getOperation(testCase.operationId), testCase};

            InstanceContext ctxVertex = createInstanceContext(testCaseInfo);
            string testName           = replace(testCase.baseName, "op", testCaseInfo.operation.name);
            addFunctionCaseWithPrograms<InstanceContext>(group, testName + stageNames[i], getGraphicsShaderCode,
                                                         runAndVerifyDefaultPipeline, ctxVertex);
        }
    }
}

InstanceContext GraphicsTestGroupBuilder::createInstanceContext(const OperationTestCaseInfo &testCaseInfo) const
{
    // LUT storing functions used to verify test results
    const VerifyIOFunc checkFloatsLUT[] = {checkFloats<Float16, deFloat16>, checkFloats<Float32, float>,
                                           checkFloats<Float64, double>};

    // 32-bit float types are always needed for standard operations on color
    // if tested operation does not require fp32 for either input or output
    // then this minimal type definitions must be appended to types section
    const string f32TypeMinimalRequired = "%type_f32             = OpTypeFloat 32\n"
                                          "%type_f32_arr_1       = OpTypeArray %type_f32 %c_i32_1\n"
                                          "%type_f32_iptr        = OpTypePointer Input %type_f32\n"
                                          "%type_f32_optr        = OpTypePointer Output %type_f32\n"
                                          "%type_f32_vec4        = OpTypeVector %type_f32 4\n"
                                          "%type_f32_vec4_iptr   = OpTypePointer Input %type_f32_vec4\n"
                                          "%type_f32_vec4_optr   = OpTypePointer Output %type_f32_vec4\n";

    const Operation &testOperation    = testCaseInfo.operation;
    const OperationTestCase &testCase = testCaseInfo.testCase;
    FloatType outFloatType            = testCaseInfo.outFloatType;
    VkShaderStageFlagBits testedStage = testCaseInfo.testedStage;

    DE_ASSERT((testedStage == VK_SHADER_STAGE_VERTEX_BIT) || (testedStage == VK_SHADER_STAGE_FRAGMENT_BIT));

    SpecializedOperation specOpData;
    specializeOperation(testCaseInfo, specOpData);

    TypeSnippetsSP inTypeSnippets  = specOpData.inTypeSnippets;
    TypeSnippetsSP outTypeSnippets = specOpData.outTypeSnippets;
    FloatType inFloatType          = specOpData.inFloatType;

    bool outFp16WithoutStorage = (outFloatType == FP16) && testCase.fp16Without16BitStorage;
    bool inFp16WithoutStorage  = (inFloatType == FP16) && testCase.fp16Without16BitStorage;

    // There may be several reasons why we need the shaderFloat16 Vulkan feature.
    bool needsShaderFloat16 = inFp16WithoutStorage || outFp16WithoutStorage;
    // There are some weird cases where we need the constants, but would otherwise drop them.
    bool needsSpecialConstants = false;

    // UnpackHalf2x16 is a corner case - it returns two 32-bit floats but
    // internaly operates on fp16 and this type should be used by float controls
    string inFloatWidthForCaps = inTypeSnippets->bitWidth;
    string behaviorCapability;
    string behaviorExecutionMode;
    string behaviorDecorations;
    string behaviorConstants;
    getBehaviorCapabilityAndExecutionModeDecoration(testCase.behaviorFlagsExecMode, testCase.behaviorFlagsDecoration,
                                                    testCase.useDecorationFlags, testOperation.IDsToDecorate,
                                                    inFloatWidthForCaps, behaviorCapability, behaviorExecutionMode,
                                                    behaviorDecorations, behaviorConstants);

    // check which format features are needed
    bool float16FeatureRequired = (inFloatType == FP16) || (outFloatType == FP16);
    bool float64FeatureRequired = (inFloatType == FP64) || (outFloatType == FP64);

    string vertExecutionMode;
    string fragExecutionMode;
    string vertCapabilities;
    string fragCapabilities;
    string vertExtensions;
    string fragExtensions;
    string vertAnnotations;
    string fragAnnotations;
    string vertTypes;
    string fragTypes;
    string vertConstants;
    string fragConstants;
    string vertFunctions;
    string fragFunctions;
    string vertIODefinitions;
    string fragIODefinitions;
    string vertArguments;
    string fragArguments;
    string vertVariables;
    string fragVariables;
    string vertCommands;
    string fragCommands;
    string vertProcessResult;
    string fragProcessResult;

    // check if operation should be executed in vertex stage
    if (testedStage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        vertAnnotations = inTypeSnippets->inputAnnotationsSnippet + inTypeSnippets->typeAnnotationsSnippet;
        fragAnnotations = outTypeSnippets->outputAnnotationsSnippet + outTypeSnippets->typeAnnotationsSnippet;
        vertFunctions   = specOpData.functions;

        // check if input type is different from tested type (conversion operations)
        if (testOperation.isInputTypeRestricted)
        {
            vertCapabilities = inTypeSnippets->capabilities + outTypeSnippets->capabilities;
            fragCapabilities = outTypeSnippets->capabilities;
            vertExtensions   = inTypeSnippets->extensions + outTypeSnippets->extensions;
            fragExtensions   = outTypeSnippets->extensions;
            vertTypes        = inTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->typeDefinitionsSnippet +
                        outTypeSnippets->varyingsTypesSnippet;
            if (inFp16WithoutStorage)
                vertTypes += inTypeSnippets->typeDefinitionsFp16Snippet;

            fragTypes     = outTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->varyingsTypesSnippet;
            vertConstants = inTypeSnippets->constantsDefinitionsSnippet + outTypeSnippets->constantsDefinitionsSnippet;
            fragConstants = outTypeSnippets->constantsDefinitionsSnippet;
        }
        else
        {
            // input and output types are the same (majority of operations)

            vertCapabilities = outTypeSnippets->capabilities;
            fragCapabilities = vertCapabilities;
            vertExtensions   = outTypeSnippets->extensions;
            fragExtensions   = vertExtensions;
            vertTypes        = outTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->varyingsTypesSnippet;
            fragTypes        = vertTypes;
            vertConstants    = outTypeSnippets->constantsDefinitionsSnippet;
            fragConstants    = outTypeSnippets->constantsDefinitionsSnippet;
        }

        if (outFloatType != FP32)
        {
            fragTypes += f32TypeMinimalRequired;
            if (inFloatType != FP32)
                vertTypes += f32TypeMinimalRequired;
        }

        vertAnnotations += specOpData.annotations;
        vertTypes += specOpData.types;
        vertConstants += specOpData.constants;

        vertExecutionMode = behaviorExecutionMode;
        fragExecutionMode = "";
        vertIODefinitions = inTypeSnippets->inputDefinitionsSnippet + outTypeSnippets->outputVaryingsSnippet;
        fragIODefinitions = outTypeSnippets->inputVaryingsSnippet + outTypeSnippets->outputDefinitionsSnippet;
        vertArguments     = specOpData.arguments;
        fragArguments     = "";
        vertVariables     = specOpData.variables;
        fragVariables     = "";
        vertCommands      = specOpData.commands;
        fragCommands      = "";
        vertProcessResult = outTypeSnippets->storeVertexResultSnippet;
        fragProcessResult = outTypeSnippets->loadVertexResultSnippet + outTypeSnippets->storeResultsSnippet;

        if (inFp16WithoutStorage)
        {
            vertAnnotations += inTypeSnippets->typeAnnotationsFp16Snippet;
            vertIODefinitions = inTypeSnippets->inputDefinitionsFp16Snippet + outTypeSnippets->outputVaryingsSnippet;
        }

        if (outFp16WithoutStorage)
        {
            vertTypes += outTypeSnippets->typeDefinitionsFp16Snippet;
            fragTypes += outTypeSnippets->typeDefinitionsFp16Snippet;
            fragAnnotations += outTypeSnippets->typeAnnotationsFp16Snippet;
            fragIODefinitions = outTypeSnippets->inputVaryingsSnippet + outTypeSnippets->outputDefinitionsFp16Snippet;
            fragProcessResult = outTypeSnippets->loadVertexResultSnippet + outTypeSnippets->storeResultsFp16Snippet;
        }

        needsShaderFloat16 |= outTypeSnippets->loadStoreRequiresShaderFloat16;
    }
    else // perform test in fragment stage - vertex stage is empty
    {
        fragFunctions = specOpData.functions;
        // check if input type is different from tested type
        if (testOperation.isInputTypeRestricted)
        {
            fragAnnotations = inTypeSnippets->inputAnnotationsSnippet + inTypeSnippets->typeAnnotationsSnippet +
                              outTypeSnippets->outputAnnotationsSnippet + outTypeSnippets->typeAnnotationsSnippet;
            fragCapabilities = (inFp16WithoutStorage ? inTypeSnippets->capabilitiesFp16Without16BitStorage :
                                                       inTypeSnippets->capabilities) +
                               (outFp16WithoutStorage ? outTypeSnippets->capabilitiesFp16Without16BitStorage :
                                                        outTypeSnippets->capabilities);
            fragExtensions = (inFp16WithoutStorage ? inTypeSnippets->extensionsFp16Without16BitStorage :
                                                     inTypeSnippets->extensions) +
                             (outFp16WithoutStorage ? outTypeSnippets->extensionsFp16Without16BitStorage :
                                                      outTypeSnippets->extensions);
            fragTypes     = inTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->typeDefinitionsSnippet;
            fragConstants = inTypeSnippets->constantsDefinitionsSnippet + outTypeSnippets->constantsDefinitionsSnippet;
        }
        else
        {
            // input and output types are the same

            fragAnnotations = inTypeSnippets->inputAnnotationsSnippet + inTypeSnippets->typeAnnotationsSnippet +
                              outTypeSnippets->outputAnnotationsSnippet;
            fragCapabilities = (outFp16WithoutStorage ? outTypeSnippets->capabilitiesFp16Without16BitStorage :
                                                        outTypeSnippets->capabilities);
            fragExtensions   = (outFp16WithoutStorage ? outTypeSnippets->extensionsFp16Without16BitStorage :
                                                        outTypeSnippets->extensions);
            fragTypes        = outTypeSnippets->typeDefinitionsSnippet;
            fragConstants    = outTypeSnippets->constantsDefinitionsSnippet;
        }

        // varying is not used but it needs to be specified so lets use type_i32 for it
        string unusedVertVarying = "%BP_vertex_result     = OpVariable %type_i32_optr Output\n";
        string unusedFragVarying = "%BP_vertex_result     = OpVariable %type_i32_iptr Input\n";

        vertCapabilities = "";
        vertExtensions   = "";
        vertAnnotations  = "OpDecorate %type_f32_arr_1 ArrayStride 4\n";
        vertTypes        = f32TypeMinimalRequired;
        vertConstants    = "";

        if ((outFloatType != FP32) && (inFloatType != FP32))
            fragTypes += f32TypeMinimalRequired;

        fragAnnotations += specOpData.annotations;
        fragTypes += specOpData.types;
        fragConstants += specOpData.constants;

        vertExecutionMode = "";
        fragExecutionMode = behaviorExecutionMode;
        vertIODefinitions = unusedVertVarying;
        fragIODefinitions = unusedFragVarying;

        vertArguments     = "";
        fragArguments     = specOpData.arguments;
        vertVariables     = "";
        fragVariables     = specOpData.variables;
        vertCommands      = "";
        fragCommands      = specOpData.commands;
        vertProcessResult = "";
        fragProcessResult = outTypeSnippets->storeResultsSnippet;

        if (inFp16WithoutStorage)
        {
            fragAnnotations += inTypeSnippets->typeAnnotationsFp16Snippet;
            if (testOperation.isInputTypeRestricted)
            {
                fragTypes += inTypeSnippets->typeDefinitionsFp16Snippet;
            }
            fragIODefinitions += inTypeSnippets->inputDefinitionsFp16Snippet;
        }
        else
            fragIODefinitions += inTypeSnippets->inputDefinitionsSnippet;

        if (outFp16WithoutStorage)
        {
            if (testOperation.isInputTypeRestricted)
            {
                fragAnnotations += outTypeSnippets->typeAnnotationsFp16Snippet;
            }
            fragTypes += outTypeSnippets->typeDefinitionsFp16Snippet;
            fragIODefinitions += outTypeSnippets->outputDefinitionsFp16Snippet;
            fragProcessResult = outTypeSnippets->storeResultsFp16Snippet;
        }
        else
            fragIODefinitions += outTypeSnippets->outputDefinitionsSnippet;

        if (!testCaseInfo.argumentsFromInput)
        {
            switch (testCaseInfo.testCase.operationId)
            {
            case OID_CONV_FROM_FP32:
            case OID_CONV_FROM_FP64:
                needsSpecialConstants = true;
                break;
            default:
                break;
            }
        }
    }

    // Another reason we need shaderFloat16 is the executable instructions uses fp16
    // in a way not supported by the 16bit storage extension.
    needsShaderFloat16 |= float16FeatureRequired && testOperation.floatUsage == FLOAT_ARITHMETIC;

    // Constants are only needed sometimes.  Drop them in the fp16 case if the code doesn't need
    // them, and if we don't otherwise need shaderFloat16.
    bool needsFP16Constants = needsShaderFloat16 || needsSpecialConstants || outFp16WithoutStorage;

    if (!needsFP16Constants && float16FeatureRequired)
    {
        // Check various code fragments
        const FloatStatementUsageFlags commandsFloatConstMask =
            B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_CONST_FP16;
        const bool commandsUsesFloatConstant =
            (testCaseInfo.operation.statementUsageFlags & commandsFloatConstMask) != 0;
        const FloatStatementUsageFlags argumentsFloatConstMask =
            B_STATEMENT_USAGE_ARGS_CONST_FLOAT | B_STATEMENT_USAGE_ARGS_CONST_FP16;
        const bool argumentsUsesFloatConstant = (specOpData.argumentsUsesFloatConstant & argumentsFloatConstMask) != 0;
        bool hasFP16ConstsInCommandsOrArguments = commandsUsesFloatConstant || argumentsUsesFloatConstant;

        needsFP16Constants |= hasFP16ConstsInCommandsOrArguments;

        if (!needsFP16Constants)
        {
            vertConstants = "";
            fragConstants = "";
        }
    }
    needsShaderFloat16 |= needsFP16Constants;

    if (needsShaderFloat16)
    {
        vertCapabilities += "OpCapability Float16\n";
        fragCapabilities += "OpCapability Float16\n";
    }

    if (testCase.requireRte)
    {
        vertExtensions += "OpExtension \"SPV_KHR_float_controls\"\n";
        vertCapabilities += "OpCapability RoundingModeRTE\n";
        vertExecutionMode += "OpExecutionMode %main RoundingModeRTE " + inTypeSnippets->bitWidth + "\n";

        fragExtensions += "OpExtension \"SPV_KHR_float_controls\"\n";
        fragCapabilities += "OpCapability RoundingModeRTE\n";
        fragExecutionMode += "OpExecutionMode %main RoundingModeRTE " + inTypeSnippets->bitWidth + "\n";
    }

    map<string, string> specializations;
    if (testCaseInfo.testedStage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        vertCapabilities += behaviorCapability;

        specializations["vert_decorations"] = behaviorDecorations;
        specializations["frag_decorations"] = "";
    }
    else
    {
        fragCapabilities += behaviorCapability;

        specializations["vert_decorations"] = "";
        specializations["frag_decorations"] = behaviorDecorations;
    }
    specializations["behaviorConstants"] = behaviorConstants;
    specializations["vert_capabilities"] = vertCapabilities, specializations["vert_extensions"] = vertExtensions;
    specializations["vert_execution_mode"] = vertExecutionMode;
    specializations["vert_annotations"]    = vertAnnotations;
    specializations["vert_types"]          = vertTypes;
    specializations["vert_constants"]      = vertConstants;
    specializations["vert_io_definitions"] = vertIODefinitions;
    specializations["vert_arguments"]      = vertArguments;
    specializations["vert_variables"]      = vertVariables;
    specializations["vert_functions"]      = vertFunctions;
    specializations["vert_commands"]       = vertCommands;
    specializations["vert_process_result"] = vertProcessResult;
    specializations["frag_capabilities"]   = fragCapabilities;
    specializations["frag_extensions"]     = fragExtensions;
    specializations["frag_execution_mode"] = fragExecutionMode;
    specializations["frag_annotations"]    = fragAnnotations;
    specializations["frag_types"]          = fragTypes;
    specializations["frag_constants"]      = fragConstants;
    specializations["frag_functions"]      = fragFunctions;
    specializations["frag_io_definitions"] = fragIODefinitions;
    specializations["frag_arguments"]      = fragArguments;
    specializations["frag_variables"]      = fragVariables;
    specializations["frag_commands"]       = fragCommands;
    specializations["frag_process_result"] = fragProcessResult;

    // colors are not used by the test - input is passed via uniform buffer
    RGBA defaultColors[4] = {RGBA::white(), RGBA::red(), RGBA::green(), RGBA::blue()};

    // construct input and output buffers of proper types
    TypeValuesSP inTypeValues  = m_typeData.at(inFloatType).values;
    TypeValuesSP outTypeValues = m_typeData.at(outFloatType).values;
    BufferSp inBufferSp        = inTypeValues->constructInputBuffer(testCase.input);
    BufferSp outBufferSp       = outTypeValues->constructOutputBuffer(testCase.expectedOutput);

    vkt::SpirVAssembly::GraphicsResources resources;
    resources.inputs.push_back(Resource(inBufferSp, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
    resources.outputs.push_back(Resource(outBufferSp, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
    resources.verifyIO = checkFloatsLUT[outFloatType];

    StageToSpecConstantMap noSpecConstants;
    PushConstants noPushConstants;
    GraphicsInterfaces noInterfaces;

    VulkanFeatures vulkanFeatures;
    vulkanFeatures.coreFeatures.shaderFloat64             = float64FeatureRequired;
    vulkanFeatures.coreFeatures.fragmentStoresAndAtomics  = true;
    vulkanFeatures.extFloatControls2.shaderFloatControls2 = true;
    vulkanFeatures.extFloat16Int8.shaderFloat16           = needsShaderFloat16;
    vulkanFeatures.ext16BitStorage.storageBuffer16BitAccess =
        float16FeatureRequired &&
        (!testCase.fp16Without16BitStorage || testCaseInfo.testedStage == VK_SHADER_STAGE_VERTEX_BIT);
    vulkanFeatures.ext16BitStorage.uniformAndStorageBuffer16BitAccess =
        vulkanFeatures.ext16BitStorage.storageBuffer16BitAccess;

    // Float controls 2 still requires that the original float controls properties are supported
    FillFloatControlsProperties(vulkanFeatures.floatControlsProperties, testCase, inFloatType);

    InstanceContext ctx(defaultColors, defaultColors, specializations, noSpecConstants, noPushConstants, resources,
                        noInterfaces, {}, vulkanFeatures, testedStage);

    ctx.moduleMap["vert"].push_back(std::make_pair("main", VK_SHADER_STAGE_VERTEX_BIT));
    ctx.moduleMap["frag"].push_back(std::make_pair("main", VK_SHADER_STAGE_FRAGMENT_BIT));

    ctx.requiredStages = static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    ctx.failResult     = QP_TEST_RESULT_FAIL;
    ctx.failMessageTemplate = "Output doesn't match with expected";

    ctx.resources.spirvVersion = SPIRV_VERSION_1_2;

    return ctx;
}

} // namespace

tcu::TestCaseGroup *createFloatControls2TestGroup(TestContext &testCtx, TestGroupBuilderBase *groupBuilder)
{
    de::MovePtr<TestCaseGroup> group(
        new TestCaseGroup(testCtx, "float_controls2", "Tests for VK_KHR_shader_float_controls2 extension"));

    struct TestGroup
    {
        FloatType floatType;
        const char *groupName;
    };
    TestGroup testGroups[] = {
        {FP16, "fp16"},
        {FP32, "fp32"},
        {FP64, "fp64"},
    };

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(testGroups); ++i)
    {
        const TestGroup &testGroup = testGroups[i];
        TestCaseGroup *typeGroup   = new TestCaseGroup(testCtx, testGroup.groupName, "");
        group->addChild(typeGroup);

        groupBuilder->createOperationTests(typeGroup, "input_args", testGroup.floatType, true);
    }

    return group.release();
}

tcu::TestCaseGroup *createFloatControls2ComputeGroup(TestContext &testCtx)
{
    ComputeTestGroupBuilder computeTestGroupBuilder;
    computeTestGroupBuilder.init();

    return createFloatControls2TestGroup(testCtx, &computeTestGroupBuilder);
}

tcu::TestCaseGroup *createFloatControls2GraphicsGroup(TestContext &testCtx)
{
    GraphicsTestGroupBuilder graphicsTestGroupBuilder;
    graphicsTestGroupBuilder.init();

    return createFloatControls2TestGroup(testCtx, &graphicsTestGroupBuilder);
}

} // namespace SpirVAssembly
} // namespace vkt
