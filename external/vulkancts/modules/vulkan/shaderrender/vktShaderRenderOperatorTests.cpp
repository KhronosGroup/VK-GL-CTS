/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Shader operators tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderOperatorTests.hpp"
#include "vktShaderRender.hpp"
#include "tcuVectorUtil.hpp"
#include "deStringUtil.hpp"

#include <limits>

using namespace tcu;
using namespace glu;

namespace vkt
{
namespace sr
{
namespace
{

using de::clamp;
using de::max;
using de::min;

inline bool logicalAnd(bool a, bool b)
{
    return (a && b);
}
inline bool logicalOr(bool a, bool b)
{
    return (a || b);
}
inline bool logicalXor(bool a, bool b)
{
    return (a != b);
}

// \note stdlib.h defines div() that is not compatible with the macros.
template <typename T>
inline T div(T a, T b)
{
    return a / b;
}

template <typename T>
inline T leftShift(T value, int amount)
{
    return value << amount;
}

inline uint32_t rightShift(uint32_t value, int amount)
{
    return value >> amount;
}
inline int rightShift(int value, int amount)
{
    return (value >> amount) | (value >= 0 ? 0 : ~(~0U >> amount));
} // \note Arithmetic shift.

template <typename T, int Size>
Vector<T, Size> leftShift(const Vector<T, Size> &value, const Vector<int, Size> &amount)
{
    Vector<T, Size> result;
    for (int i = 0; i < Size; i++)
        result[i] = leftShift(value[i], amount[i]);
    return result;
}

template <typename T, int Size>
Vector<T, Size> rightShift(const Vector<T, Size> &value, const Vector<int, Size> &amount)
{
    Vector<T, Size> result;
    for (int i = 0; i < Size; i++)
        result[i] = rightShift(value[i], amount[i]);
    return result;
}

template <typename T, int Size>
Vector<T, Size> leftShiftVecScalar(const Vector<T, Size> &value, int amount)
{
    return leftShift(value, Vector<int, Size>(amount));
}
template <typename T, int Size>
Vector<T, Size> rightShiftVecScalar(const Vector<T, Size> &value, int amount)
{
    return rightShift(value, Vector<int, Size>(amount));
}

template <typename T, int Size>
inline Vector<T, Size> minVecScalar(const Vector<T, Size> &v, T s)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res[i] = min(v[i], s);
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> maxVecScalar(const Vector<T, Size> &v, T s)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res[i] = max(v[i], s);
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> clampVecScalarScalar(const Vector<T, Size> &v, T s0, T s1)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res[i] = clamp(v[i], s0, s1);
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> mixVecVecScalar(const Vector<T, Size> &v0, const Vector<T, Size> &v1, T s)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res[i] = mix(v0[i], v1[i], s);
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> stepScalarVec(T s, const Vector<T, Size> &v)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res[i] = step(s, v[i]);
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> smoothStepScalarScalarVec(T s0, T s1, const Vector<T, Size> &v)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res[i] = smoothStep(s0, s1, v[i]);
    return res;
}

inline int addOne(int v)
{
    return v + 1;
}
inline int subOne(int v)
{
    return v - 1;
}
inline uint32_t addOne(uint32_t v)
{
    return v + 1;
}
inline uint32_t subOne(uint32_t v)
{
    return v - 1;
}

template <int Size>
inline Vector<float, Size> addOne(const Vector<float, Size> &v)
{
    return v + 1.0f;
}
template <int Size>
inline Vector<float, Size> subOne(const Vector<float, Size> &v)
{
    return v - 1.0f;
}
template <int Size>
inline Vector<int, Size> addOne(const Vector<int, Size> &v)
{
    return v + 1;
}
template <int Size>
inline Vector<int, Size> subOne(const Vector<int, Size> &v)
{
    return v - 1;
}
template <int Size>
inline Vector<uint32_t, Size> addOne(const Vector<uint32_t, Size> &v)
{
    return v + 1U;
}
template <int Size>
inline Vector<uint32_t, Size> subOne(const Vector<uint32_t, Size> &v)
{
    return (v.asInt() - 1).asUint();
}

template <typename T>
inline T selection(bool cond, T a, T b)
{
    return cond ? a : b;
}

// Vec-scalar and scalar-vec binary operators.

// \note This one is done separately due to how the overloaded minus operator is implemented for vector-scalar operands.
template <int Size>
inline Vector<uint32_t, Size> subVecScalar(const Vector<uint32_t, Size> &v, uint32_t s)
{
    return (v.asInt() - (int)s).asUint();
}

template <typename T, int Size>
inline Vector<T, Size> addVecScalar(const Vector<T, Size> &v, T s)
{
    return v + s;
}
template <typename T, int Size>
inline Vector<T, Size> subVecScalar(const Vector<T, Size> &v, T s)
{
    return v - s;
}
template <typename T, int Size>
inline Vector<T, Size> mulVecScalar(const Vector<T, Size> &v, T s)
{
    return v * s;
}
template <typename T, int Size>
inline Vector<T, Size> divVecScalar(const Vector<T, Size> &v, T s)
{
    return v / s;
}
template <typename T, int Size>
inline Vector<T, Size> modVecScalar(const Vector<T, Size> &v, T s)
{
    return mod(v, Vector<T, Size>(s));
}
template <typename T, int Size>
inline Vector<T, Size> bitwiseAndVecScalar(const Vector<T, Size> &v, T s)
{
    return bitwiseAnd(v, Vector<T, Size>(s));
}
template <typename T, int Size>
inline Vector<T, Size> bitwiseOrVecScalar(const Vector<T, Size> &v, T s)
{
    return bitwiseOr(v, Vector<T, Size>(s));
}
template <typename T, int Size>
inline Vector<T, Size> bitwiseXorVecScalar(const Vector<T, Size> &v, T s)
{
    return bitwiseXor(v, Vector<T, Size>(s));
}

template <typename T, int Size>
inline Vector<T, Size> addScalarVec(T s, const Vector<T, Size> &v)
{
    return s + v;
}
template <typename T, int Size>
inline Vector<T, Size> subScalarVec(T s, const Vector<T, Size> &v)
{
    return s - v;
}
template <typename T, int Size>
inline Vector<T, Size> mulScalarVec(T s, const Vector<T, Size> &v)
{
    return s * v;
}
template <typename T, int Size>
inline Vector<T, Size> divScalarVec(T s, const Vector<T, Size> &v)
{
    return s / v;
}
template <typename T, int Size>
inline Vector<T, Size> modScalarVec(T s, const Vector<T, Size> &v)
{
    return mod(Vector<T, Size>(s), v);
}
template <typename T, int Size>
inline Vector<T, Size> bitwiseAndScalarVec(T s, const Vector<T, Size> &v)
{
    return bitwiseAnd(Vector<T, Size>(s), v);
}
template <typename T, int Size>
inline Vector<T, Size> bitwiseOrScalarVec(T s, const Vector<T, Size> &v)
{
    return bitwiseOr(Vector<T, Size>(s), v);
}
template <typename T, int Size>
inline Vector<T, Size> bitwiseXorScalarVec(T s, const Vector<T, Size> &v)
{
    return bitwiseXor(Vector<T, Size>(s), v);
}

// Reference functions for specific sequence operations for the sequence operator tests.

// Reference for expression "in0, in2 + in1, in1 + in0"
inline Vec4 sequenceNoSideEffCase0(const Vec4 &in0, const Vec4 &in1, const Vec4 &in2)
{
    DE_UNREF(in2);
    return in1 + in0;
}
// Reference for expression "in0, in2 + in1, in1 + in0"
inline uint32_t sequenceNoSideEffCase1(float in0, uint32_t in1, float in2)
{
    DE_UNREF(in0);
    DE_UNREF(in2);
    return in1 + in1;
}
// Reference for expression "in0 && in1, in0, ivec2(vec2(in0) + in2)"
inline IVec2 sequenceNoSideEffCase2(bool in0, bool in1, const Vec2 &in2)
{
    DE_UNREF(in1);
    return IVec2((int)((float)in0 + in2.x()), (int)((float)in0 + in2.y()));
}
// Reference for expression "in0 + vec4(in1), in2, in1"
inline IVec4 sequenceNoSideEffCase3(const Vec4 &in0, const IVec4 &in1, const BVec4 &in2)
{
    DE_UNREF(in0);
    DE_UNREF(in2);
    return in1;
}
// Reference for expression "in0++, in1 = in0 + in2, in2 = in1"
inline Vec4 sequenceSideEffCase0(const Vec4 &in0, const Vec4 &in1, const Vec4 &in2)
{
    DE_UNREF(in1);
    return in0 + 1.0f + in2;
}
// Reference for expression "in1++, in0 = float(in1), in1 = uint(in0 + in2)"
inline uint32_t sequenceSideEffCase1(float in0, uint32_t in1, float in2)
{
    DE_UNREF(in0);
    return (uint32_t)(float(in1) + 1.0f + in2);
}
// Reference for expression "in1 = in0, in2++, in2 = in2 + vec2(in1), ivec2(in2)"
inline IVec2 sequenceSideEffCase2(bool in0, bool in1, const Vec2 &in2)
{
    DE_UNREF(in1);
    return (in2 + Vec2(1.0f) + Vec2((float)in0)).asInt();
}
// Reference for expression "in0 = in0 + vec4(in2), in1 = in1 + ivec4(in0), in1++"
inline IVec4 sequenceSideEffCase3(const Vec4 &in0, const IVec4 &in1, const BVec4 &in2)
{
    return in1 + (in0 + Vec4((float)in2.x(), (float)in2.y(), (float)in2.z(), (float)in2.w())).asInt();
}

// ShaderEvalFunc-type wrappers for the above functions.
void evalSequenceNoSideEffCase0(ShaderEvalContext &ctx)
{
    ctx.color = sequenceNoSideEffCase0(ctx.in[0].swizzle(1, 2, 3, 0), ctx.in[1].swizzle(3, 2, 1, 0),
                                       ctx.in[2].swizzle(0, 3, 2, 1));
}
void evalSequenceNoSideEffCase1(ShaderEvalContext &ctx)
{
    ctx.color.x() = (float)sequenceNoSideEffCase1(ctx.in[0].z(), (uint32_t)ctx.in[1].x(), ctx.in[2].y());
}
void evalSequenceNoSideEffCase2(ShaderEvalContext &ctx)
{
    ctx.color.yz() =
        sequenceNoSideEffCase2(ctx.in[0].z() > 0.0f, ctx.in[1].x() > 0.0f, ctx.in[2].swizzle(2, 1)).asFloat();
}
void evalSequenceNoSideEffCase3(ShaderEvalContext &ctx)
{
    ctx.color = sequenceNoSideEffCase3(ctx.in[0].swizzle(1, 2, 3, 0), ctx.in[1].swizzle(3, 2, 1, 0).asInt(),
                                       greaterThan(ctx.in[2].swizzle(0, 3, 2, 1), Vec4(0.0f, 0.0f, 0.0f, 0.0f)))
                    .asFloat();
}
void evalSequenceSideEffCase0(ShaderEvalContext &ctx)
{
    ctx.color = sequenceSideEffCase0(ctx.in[0].swizzle(1, 2, 3, 0), ctx.in[1].swizzle(3, 2, 1, 0),
                                     ctx.in[2].swizzle(0, 3, 2, 1));
}
void evalSequenceSideEffCase1(ShaderEvalContext &ctx)
{
    ctx.color.x() = (float)sequenceSideEffCase1(ctx.in[0].z(), (uint32_t)ctx.in[1].x(), ctx.in[2].y());
}
void evalSequenceSideEffCase2(ShaderEvalContext &ctx)
{
    ctx.color.yz() =
        sequenceSideEffCase2(ctx.in[0].z() > 0.0f, ctx.in[1].x() > 0.0f, ctx.in[2].swizzle(2, 1)).asFloat();
}
void evalSequenceSideEffCase3(ShaderEvalContext &ctx)
{
    ctx.color = sequenceSideEffCase3(ctx.in[0].swizzle(1, 2, 3, 0), ctx.in[1].swizzle(3, 2, 1, 0).asInt(),
                                     greaterThan(ctx.in[2].swizzle(0, 3, 2, 1), Vec4(0.0f, 0.0f, 0.0f, 0.0f)))
                    .asFloat();
}

static std::string stringJoin(const std::vector<std::string> &elems, const std::string &delim)
{
    std::string result;
    for (int i = 0; i < (int)elems.size(); i++)
        result += (i > 0 ? delim : "") + elems[i];
    return result;
}

static std::string twoValuedVec4(const std::string &first, const std::string &second, const BVec4 &firstMask)
{
    std::vector<std::string> elems(4);
    for (int i = 0; i < 4; i++)
        elems[i] = firstMask[i] ? first : second;

    return "vec4(" + stringJoin(elems, ", ") + ")";
}

enum
{
    MAX_INPUTS = 3
};

enum PrecisionMask
{
    PRECMASK_NA      = 0, //!< Precision not applicable (booleans)
    PRECMASK_MEDIUMP = (1 << PRECISION_MEDIUMP),
    PRECMASK_HIGHP   = (1 << PRECISION_HIGHP),

    PRECMASK_ALL = PRECMASK_MEDIUMP | PRECMASK_HIGHP
};

enum ValueType
{
    VALUE_NONE          = 0,
    VALUE_FLOAT         = (1 << 0),  // float scalar
    VALUE_FLOAT_VEC     = (1 << 1),  // float vector
    VALUE_FLOAT_GENTYPE = (1 << 2),  // float scalar/vector
    VALUE_VEC3          = (1 << 3),  // vec3 only
    VALUE_MATRIX        = (1 << 4),  // matrix
    VALUE_BOOL          = (1 << 5),  // boolean scalar
    VALUE_BOOL_VEC      = (1 << 6),  // boolean vector
    VALUE_BOOL_GENTYPE  = (1 << 7),  // boolean scalar/vector
    VALUE_INT           = (1 << 8),  // int scalar
    VALUE_INT_VEC       = (1 << 9),  // int vector
    VALUE_INT_GENTYPE   = (1 << 10), // int scalar/vector
    VALUE_UINT          = (1 << 11), // uint scalar
    VALUE_UINT_VEC      = (1 << 12), // uint vector
    VALUE_UINT_GENTYPE  = (1 << 13), // uint scalar/vector

    // Shorthands.
    F   = VALUE_FLOAT,
    FV  = VALUE_FLOAT_VEC,
    GT  = VALUE_FLOAT_GENTYPE,
    V3  = VALUE_VEC3,
    M   = VALUE_MATRIX,
    B   = VALUE_BOOL,
    BV  = VALUE_BOOL_VEC,
    BGT = VALUE_BOOL_GENTYPE,
    I   = VALUE_INT,
    IV  = VALUE_INT_VEC,
    IGT = VALUE_INT_GENTYPE,
    U   = VALUE_UINT,
    UV  = VALUE_UINT_VEC,
    UGT = VALUE_UINT_GENTYPE
};

static inline bool isScalarType(ValueType type)
{
    return type == VALUE_FLOAT || type == VALUE_BOOL || type == VALUE_INT || type == VALUE_UINT;
}

static inline bool isFloatType(ValueType type)
{
    return (type & (VALUE_FLOAT | VALUE_FLOAT_VEC | VALUE_FLOAT_GENTYPE)) != 0;
}

static inline bool isIntType(ValueType type)
{
    return (type & (VALUE_INT | VALUE_INT_VEC | VALUE_INT_GENTYPE)) != 0;
}

static inline bool isUintType(ValueType type)
{
    return (type & (VALUE_UINT | VALUE_UINT_VEC | VALUE_UINT_GENTYPE)) != 0;
}

static inline bool isBoolType(ValueType type)
{
    return (type & (VALUE_BOOL | VALUE_BOOL_VEC | VALUE_BOOL_GENTYPE)) != 0;
}

struct Value
{
    Value(ValueType valueType_, const float rangeMin_, const float rangeMax_)
        : valueType(valueType_)
        , rangeMin(rangeMin_)
        , rangeMax(rangeMax_)
    {
    }

    ValueType valueType;
    float rangeMin;
    float rangeMax;
};

enum OperationType
{
    FUNCTION = 0,
    OPERATOR,
    SIDE_EFFECT_OPERATOR // Test the side-effect (as opposed to the result) of a side-effect operator.
};

struct BuiltinFuncInfo
{
    BuiltinFuncInfo(const char *caseName_, const char *shaderFuncName_, ValueType outValue_, Value input0_,
                    Value input1_, Value input2_, const float resultScale_, const float resultBias_,
                    uint32_t precisionMask_, ShaderEvalFunc evalFuncScalar_, ShaderEvalFunc evalFuncVec2_,
                    ShaderEvalFunc evalFuncVec3_, ShaderEvalFunc evalFuncVec4_, OperationType type_ = FUNCTION,
                    bool isUnaryPrefix_ = true)
        : caseName(caseName_)
        , shaderFuncName(shaderFuncName_)
        , outValue(outValue_)
        , input0(input0_)
        , input1(input1_)
        , input2(input2_)
        , resultScale(resultScale_)
        , resultBias(resultBias_)
        , referenceScale(resultScale_)
        , referenceBias(resultBias_)
        , precisionMask(precisionMask_)
        , evalFuncScalar(evalFuncScalar_)
        , evalFuncVec2(evalFuncVec2_)
        , evalFuncVec3(evalFuncVec3_)
        , evalFuncVec4(evalFuncVec4_)
        , type(type_)
        , isUnaryPrefix(isUnaryPrefix_)
    {
    }

    BuiltinFuncInfo(const char *caseName_, const char *shaderFuncName_, ValueType outValue_, Value input0_,
                    Value input1_, Value input2_, const float resultScale_, const float resultBias_,
                    const float referenceScale_, const float referenceBias_, uint32_t precisionMask_,
                    ShaderEvalFunc evalFuncScalar_, ShaderEvalFunc evalFuncVec2_, ShaderEvalFunc evalFuncVec3_,
                    ShaderEvalFunc evalFuncVec4_, OperationType type_ = FUNCTION, bool isUnaryPrefix_ = true)
        : caseName(caseName_)
        , shaderFuncName(shaderFuncName_)
        , outValue(outValue_)
        , input0(input0_)
        , input1(input1_)
        , input2(input2_)
        , resultScale(resultScale_)
        , resultBias(resultBias_)
        , referenceScale(referenceScale_)
        , referenceBias(referenceBias_)
        , precisionMask(precisionMask_)
        , evalFuncScalar(evalFuncScalar_)
        , evalFuncVec2(evalFuncVec2_)
        , evalFuncVec3(evalFuncVec3_)
        , evalFuncVec4(evalFuncVec4_)
        , type(type_)
        , isUnaryPrefix(isUnaryPrefix_)
    {
    }

    const char *caseName;       //!< Name of case.
    const char *shaderFuncName; //!< Name in shading language.
    ValueType outValue;
    Value input0;
    Value input1;
    Value input2;
    float resultScale;
    float resultBias;
    float referenceScale;
    float referenceBias;
    uint32_t precisionMask;
    ShaderEvalFunc evalFuncScalar;
    ShaderEvalFunc evalFuncVec2;
    ShaderEvalFunc evalFuncVec3;
    ShaderEvalFunc evalFuncVec4;
    OperationType type;
    bool isUnaryPrefix; //!< Whether a unary operator is a prefix operator; redundant unless unary.
};

static inline BuiltinFuncInfo BuiltinOperInfo(const char *caseName_, const char *shaderFuncName_, ValueType outValue_,
                                              Value input0_, Value input1_, Value input2_, const float resultScale_,
                                              const float resultBias_, uint32_t precisionMask_,
                                              ShaderEvalFunc evalFuncScalar_, ShaderEvalFunc evalFuncVec2_,
                                              ShaderEvalFunc evalFuncVec3_, ShaderEvalFunc evalFuncVec4_)
{
    return BuiltinFuncInfo(caseName_, shaderFuncName_, outValue_, input0_, input1_, input2_, resultScale_, resultBias_,
                           resultScale_, resultBias_, precisionMask_, evalFuncScalar_, evalFuncVec2_, evalFuncVec3_,
                           evalFuncVec4_, OPERATOR);
}

// For postfix (unary) operators.
static inline BuiltinFuncInfo BuiltinPostOperInfo(const char *caseName_, const char *shaderFuncName_,
                                                  ValueType outValue_, Value input0_, Value input1_, Value input2_,
                                                  const float resultScale_, const float resultBias_,
                                                  uint32_t precisionMask_, ShaderEvalFunc evalFuncScalar_,
                                                  ShaderEvalFunc evalFuncVec2_, ShaderEvalFunc evalFuncVec3_,
                                                  ShaderEvalFunc evalFuncVec4_)
{
    return BuiltinFuncInfo(caseName_, shaderFuncName_, outValue_, input0_, input1_, input2_, resultScale_, resultBias_,
                           resultScale_, resultBias_, precisionMask_, evalFuncScalar_, evalFuncVec2_, evalFuncVec3_,
                           evalFuncVec4_, OPERATOR, false);
}

static inline BuiltinFuncInfo BuiltinSideEffOperInfo(const char *caseName_, const char *shaderFuncName_,
                                                     ValueType outValue_, Value input0_, Value input1_, Value input2_,
                                                     const float resultScale_, const float resultBias_,
                                                     uint32_t precisionMask_, ShaderEvalFunc evalFuncScalar_,
                                                     ShaderEvalFunc evalFuncVec2_, ShaderEvalFunc evalFuncVec3_,
                                                     ShaderEvalFunc evalFuncVec4_)
{
    return BuiltinFuncInfo(caseName_, shaderFuncName_, outValue_, input0_, input1_, input2_, resultScale_, resultBias_,
                           resultScale_, resultBias_, precisionMask_, evalFuncScalar_, evalFuncVec2_, evalFuncVec3_,
                           evalFuncVec4_, SIDE_EFFECT_OPERATOR);
}

// For postfix (unary) operators, testing side-effect.
static inline BuiltinFuncInfo BuiltinPostSideEffOperInfo(const char *caseName_, const char *shaderFuncName_,
                                                         ValueType outValue_, Value input0_, Value input1_,
                                                         Value input2_, const float resultScale_,
                                                         const float resultBias_, uint32_t precisionMask_,
                                                         ShaderEvalFunc evalFuncScalar_, ShaderEvalFunc evalFuncVec2_,
                                                         ShaderEvalFunc evalFuncVec3_, ShaderEvalFunc evalFuncVec4_)
{
    return BuiltinFuncInfo(caseName_, shaderFuncName_, outValue_, input0_, input1_, input2_, resultScale_, resultBias_,
                           resultScale_, resultBias_, precisionMask_, evalFuncScalar_, evalFuncVec2_, evalFuncVec3_,
                           evalFuncVec4_, SIDE_EFFECT_OPERATOR, false);
}

// BuiltinFuncGroup

struct BuiltinFuncGroup
{
    BuiltinFuncGroup(const char *name_, const char *description_) : name(name_), description(description_)
    {
    }
    BuiltinFuncGroup &operator<<(const BuiltinFuncInfo &info)
    {
        funcInfos.push_back(info);
        return *this;
    }

    const char *name;
    const char *description;
    std::vector<BuiltinFuncInfo> funcInfos;
};

static const char *s_inSwizzles[MAX_INPUTS][4] = {{"z", "wy", "zxy", "yzwx"},
                                                  {"x", "yx", "yzx", "wzyx"},
                                                  {"y", "zy", "wyz", "xwzy"}};

static const char *s_outSwizzles[] = {"x", "yz", "xyz", "xyzw"};

static const BVec4 s_outSwizzleChannelMasks[] = {BVec4(true, false, false, false), BVec4(false, true, true, false),
                                                 BVec4(true, true, true, false), BVec4(true, true, true, true)};

// OperatorShaderEvaluator

class OperatorShaderEvaluator : public ShaderEvaluator
{
public:
    OperatorShaderEvaluator(const ShaderEvalFunc evalFunc, const float scale, const float bias, int resultScalarSize)
        : m_evalFunc(evalFunc)
        , m_resultScalarSize(resultScalarSize)
        , m_evaluatedScale(scale)
        , m_evaluatedBias(bias)
    {
        DE_ASSERT(de::inRange(resultScalarSize, 1, 4));
    }

    virtual ~OperatorShaderEvaluator(void)
    {
    }

    virtual void evaluate(ShaderEvalContext &ctx) const
    {
        m_evalFunc(ctx);

        for (int channelNdx = 0; channelNdx < 4; channelNdx++)
            if (s_outSwizzleChannelMasks[m_resultScalarSize - 1][channelNdx])
                ctx.color[channelNdx] = ctx.color[channelNdx] * m_evaluatedScale + m_evaluatedBias;
    }

private:
    const ShaderEvalFunc m_evalFunc;
    const int m_resultScalarSize;

    const float m_evaluatedScale;
    const float m_evaluatedBias;
};

// Concrete value.

struct ShaderValue
{
    ShaderValue(DataType type_, const float rangeMin_, const float rangeMax_)
        : type(type_)
        , rangeMin(rangeMin_)
        , rangeMax(rangeMax_)
    {
    }

    ShaderValue(void) : type(TYPE_LAST), rangeMin(0.0f), rangeMax(0.0f)
    {
    }

    DataType type;
    float rangeMin;
    float rangeMax;
};

struct ShaderDataSpec
{
    ShaderDataSpec(void)
        : resultScale(1.0f)
        , resultBias(0.0f)
        , referenceScale(1.0f)
        , referenceBias(0.0f)
        , precision(PRECISION_LAST)
        , output(TYPE_LAST)
        , numInputs(0)
    {
    }

    float resultScale;
    float resultBias;
    float referenceScale;
    float referenceBias;
    Precision precision;
    DataType output;
    int numInputs;
    ShaderValue inputs[MAX_INPUTS];
};

// ShaderOperatorInstance

class ShaderOperatorCaseInstance : public ShaderRenderCaseInstance
{
public:
    ShaderOperatorCaseInstance(Context &context, const bool isVertexCase, const ShaderEvaluator &evaluator,
                               const UniformSetup &uniformSetup, const ShaderDataSpec spec);
    virtual ~ShaderOperatorCaseInstance(void);

private:
    const ShaderDataSpec m_spec;
};

ShaderOperatorCaseInstance::ShaderOperatorCaseInstance(Context &context, const bool isVertexCase,
                                                       const ShaderEvaluator &evaluator,
                                                       const UniformSetup &uniformSetup, const ShaderDataSpec spec)
    : ShaderRenderCaseInstance(context, isVertexCase, evaluator, uniformSetup, DE_NULL, IMAGE_BACKING_MODE_REGULAR,
                               (isVertexCase ? 92 : GRID_SIZE_DEFAULT_FRAGMENT))
    , m_spec(spec)
{
    // Setup the user attributes.
    m_userAttribTransforms.resize(m_spec.numInputs);
    for (int inputNdx = 0; inputNdx < m_spec.numInputs; inputNdx++)
    {
        const ShaderValue &v = m_spec.inputs[inputNdx];
        DE_ASSERT(v.type != TYPE_LAST);

        const float rangeMin = v.rangeMin;
        const float rangeMax = v.rangeMax;
        const float scale    = rangeMax - rangeMin;
        const float minBias  = rangeMin;
        const float maxBias  = rangeMax;
        Mat4 attribMatrix;

        for (int rowNdx = 0; rowNdx < 4; rowNdx++)
        {
            Vec4 row;

            switch ((rowNdx + inputNdx) % 4)
            {
            case 0:
                row = Vec4(scale, 0.0f, 0.0f, minBias);
                break;
            case 1:
                row = Vec4(0.0f, scale, 0.0f, minBias);
                break;
            case 2:
                row = Vec4(-scale, 0.0f, 0.0f, maxBias);
                break;
            case 3:
                row = Vec4(0.0f, -scale, 0.0f, maxBias);
                break;
            default:
                DE_ASSERT(false);
            }

            attribMatrix.setRow(rowNdx, row);
        }

        m_userAttribTransforms[inputNdx] = attribMatrix;

        const uint32_t location = 4u + inputNdx;
        switch (inputNdx)
        {
        case 0:
            useAttribute(location, A_IN0);
            break;
        case 1:
            useAttribute(location, A_IN1);
            break;
        case 2:
            useAttribute(location, A_IN2);
            break;
        case 3:
            useAttribute(location, A_IN3);
            break;
        default:
            DE_ASSERT(false);
        }
    }
}

ShaderOperatorCaseInstance::~ShaderOperatorCaseInstance(void)
{
}

// ShaderOperatorCase

class ShaderOperatorCase : public ShaderRenderCase
{
public:
    ShaderOperatorCase(tcu::TestContext &testCtx, const char *caseName, const bool isVertexCase,
                       const ShaderEvalFunc evalFunc, const std::string &shaderOp, const ShaderDataSpec &spec);
    virtual ~ShaderOperatorCase(void);

    virtual TestInstance *createInstance(Context &context) const;

protected:
    void setupShaderData(void);

private:
    ShaderOperatorCase(const ShaderOperatorCase &);            // not allowed!
    ShaderOperatorCase &operator=(const ShaderOperatorCase &); // not allowed!

    const ShaderDataSpec m_spec;
    const std::string m_shaderOp;
};

ShaderOperatorCase::ShaderOperatorCase(tcu::TestContext &testCtx, const char *caseName, const bool isVertexCase,
                                       const ShaderEvalFunc evalFunc, const std::string &shaderOp,
                                       const ShaderDataSpec &spec)
    : ShaderRenderCase(testCtx, caseName, isVertexCase,
                       new OperatorShaderEvaluator(evalFunc, spec.referenceScale, spec.referenceBias,
                                                   getDataTypeScalarSize(spec.output)),
                       DE_NULL, DE_NULL)
    , m_spec(spec)
    , m_shaderOp(shaderOp)
{
    setupShaderData();
}

TestInstance *ShaderOperatorCase::createInstance(Context &context) const
{
    DE_ASSERT(m_evaluator != DE_NULL);
    DE_ASSERT(m_uniformSetup != DE_NULL);
    return new ShaderOperatorCaseInstance(context, m_isVertexCase, *m_evaluator, *m_uniformSetup, m_spec);
}

void ShaderOperatorCase::setupShaderData(void)
{
    const char *precision = m_spec.precision != PRECISION_LAST ? getPrecisionName(m_spec.precision) : DE_NULL;
    const char *inputPrecision[MAX_INPUTS];

    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = m_isVertexCase ? vtx : frag;

    std::string header = "#version 310 es\n";

    vtx << header;
    frag << header;

    // Compute precision for inputs.
    for (int inputNdx = 0; inputNdx < m_spec.numInputs; inputNdx++)
    {
        const bool isBoolVal = de::inRange<int>(m_spec.inputs[inputNdx].type, TYPE_BOOL, TYPE_BOOL_VEC4);
        const bool isIntVal  = de::inRange<int>(m_spec.inputs[inputNdx].type, TYPE_INT, TYPE_INT_VEC4);
        const bool isUintVal = de::inRange<int>(m_spec.inputs[inputNdx].type, TYPE_UINT, TYPE_UINT_VEC4);
        // \note Mediump interpolators are used for booleans, and highp for integers.
        const Precision prec     = isBoolVal             ? PRECISION_MEDIUMP :
                                   isIntVal || isUintVal ? PRECISION_HIGHP :
                                                           m_spec.precision;
        inputPrecision[inputNdx] = getPrecisionName(prec);
    }

    // Attributes.
    vtx << "layout(location = 0) in highp vec4 a_position;\n";
    for (int inputNdx = 0; inputNdx < m_spec.numInputs; inputNdx++)
        vtx << "layout(location = " << 4 + inputNdx << ") in " << inputPrecision[inputNdx] << " vec4 a_in" << inputNdx
            << ";\n";

    // Color output.
    frag << "layout(location = 0) out mediump vec4 o_color;\n";

    if (m_isVertexCase)
    {
        vtx << "layout(location = 0) out mediump vec4 v_color;\n";
        frag << "layout(location = 0) in mediump vec4 v_color;\n";
    }
    else
    {
        for (int inputNdx = 0; inputNdx < m_spec.numInputs; inputNdx++)
        {
            vtx << "layout(location = " << inputNdx + 1 << ") out " << inputPrecision[inputNdx] << " vec4 v_in"
                << inputNdx << ";\n";
            frag << "layout(location = " << inputNdx + 1 << ") in " << inputPrecision[inputNdx] << " vec4 v_in"
                 << inputNdx << ";\n";
        }
    }

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Expression inputs.
    const std::string prefix = m_isVertexCase ? "a_" : "v_";
    for (int inputNdx = 0; inputNdx < m_spec.numInputs; inputNdx++)
    {
        const DataType inType = m_spec.inputs[inputNdx].type;
        const int inSize      = getDataTypeScalarSize(inType);
        const bool isInt      = de::inRange<int>(inType, TYPE_INT, TYPE_INT_VEC4);
        const bool isUint     = de::inRange<int>(inType, TYPE_UINT, TYPE_UINT_VEC4);
        const bool isBool     = de::inRange<int>(inType, TYPE_BOOL, TYPE_BOOL_VEC4);
        const char *typeName  = getDataTypeName(inType);
        const char *swizzle   = s_inSwizzles[inputNdx][inSize - 1];

        op << "\t";
        if (precision && !isBool)
            op << precision << " ";

        op << typeName << " in" << inputNdx << " = ";

        if (isBool)
        {
            if (inSize == 1)
                op << "(";
            else
                op << "greaterThan(";
        }
        else if (isInt || isUint)
            op << typeName << "(";

        op << prefix << "in" << inputNdx << "." << swizzle;

        if (isBool)
        {
            if (inSize == 1)
                op << " > 0.0)";
            else
                op << ", vec" << inSize << "(0.0))";
        }
        else if (isInt || isUint)
            op << ")";

        op << ";\n";
    }

    // Result variable.
    {
        const char *outTypeName = getDataTypeName(m_spec.output);
        const bool isBoolOut    = de::inRange<int>(m_spec.output, TYPE_BOOL, TYPE_BOOL_VEC4);

        op << "\t";
        if (precision && !isBoolOut)
            op << precision << " ";
        op << outTypeName << " res = " << outTypeName << "(0.0);\n\n";
    }

    // Expression.
    op << "\t" << m_shaderOp << "\n\n";

    // Convert to color.
    const bool isResFloatVec = de::inRange<int>(m_spec.output, TYPE_FLOAT, TYPE_FLOAT_VEC4);
    const int outScalarSize  = getDataTypeScalarSize(m_spec.output);

    op << "\thighp vec4 color = vec4(0.0, 0.0, 0.0, 1.0);\n";
    op << "\tcolor." << s_outSwizzles[outScalarSize - 1] << " = ";

    if (!isResFloatVec && outScalarSize == 1)
        op << "float(res)";
    else if (!isResFloatVec)
        op << "vec" << outScalarSize << "(res)";
    else
        op << "res";

    op << ";\n";

    // Scale & bias.
    const float resultScale = m_spec.resultScale;
    const float resultBias  = m_spec.resultBias;
    if ((resultScale != 1.0f) || (resultBias != 0.0f))
    {
        op << "\tcolor = color";
        if (resultScale != 1.0f)
            op << " * " << twoValuedVec4(de::toString(resultScale), "1.0", s_outSwizzleChannelMasks[outScalarSize - 1]);
        if (resultBias != 0.0f)
            op << " + "
               << twoValuedVec4(de::floatToString(resultBias, 2), "0.0", s_outSwizzleChannelMasks[outScalarSize - 1]);
        op << ";\n";
    }

    // ..
    if (m_isVertexCase)
    {
        vtx << "    v_color = color;\n";
        frag << "    o_color = v_color;\n";
    }
    else
    {
        for (int inputNdx = 0; inputNdx < m_spec.numInputs; inputNdx++)
            vtx << "    v_in" << inputNdx << " = a_in" << inputNdx << ";\n";
        frag << "    o_color = color;\n";
    }

    vtx << "}\n";
    frag << "}\n";

    m_vertShaderSource = vtx.str();
    m_fragShaderSource = frag.str();
}

ShaderOperatorCase::~ShaderOperatorCase(void)
{
}

// Vector math functions.
template <typename T>
inline T nop(T f)
{
    return f;
}

template <typename T, int Size>
Vector<T, Size> nop(const Vector<T, Size> &v)
{
    return v;
}

#define DECLARE_UNARY_GENTYPE_FUNCS(FUNC_NAME)               \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)      \
    {                                                        \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2)).x();     \
    }                                                        \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)       \
    {                                                        \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1));     \
    }                                                        \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)       \
    {                                                        \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1)); \
    }                                                        \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)       \
    {                                                        \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0));    \
    }

#define DECLARE_BINARY_GENTYPE_FUNCS(FUNC_NAME)                                        \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                \
    {                                                                                  \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2), c.in[1].swizzle(0)).x();           \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0));        \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0)); \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0)); \
    }

#define DECLARE_TERNARY_GENTYPE_FUNCS(FUNC_NAME)                                                                    \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                                             \
    {                                                                                                               \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2), c.in[1].swizzle(0), c.in[2].swizzle(1)).x();                    \
    }                                                                                                               \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                                              \
    {                                                                                                               \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0), c.in[2].swizzle(2, 1));              \
    }                                                                                                               \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                                              \
    {                                                                                                               \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0), c.in[2].swizzle(3, 1, 2));    \
    }                                                                                                               \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                                              \
    {                                                                                                               \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0), c.in[2].swizzle(0, 3, 2, 1)); \
    }

#define DECLARE_UNARY_SCALAR_GENTYPE_FUNCS(FUNC_NAME)         \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)       \
    {                                                         \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2));          \
    }                                                         \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)        \
    {                                                         \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(3, 1));       \
    }                                                         \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)        \
    {                                                         \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2, 0, 1));    \
    }                                                         \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)        \
    {                                                         \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0)); \
    }

#define DECLARE_BINARY_SCALAR_GENTYPE_FUNCS(FUNC_NAME)                                     \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                    \
    {                                                                                      \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2), c.in[1].swizzle(0));                   \
    }                                                                                      \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                     \
    {                                                                                      \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0));             \
    }                                                                                      \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                     \
    {                                                                                      \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0));       \
    }                                                                                      \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                     \
    {                                                                                      \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0)); \
    }

#define DECLARE_BINARY_BOOL_FUNCS(FUNC_NAME)                                    \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                          \
    {                                                                           \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f); \
    }

#define DECLARE_UNARY_BOOL_GENTYPE_FUNCS(FUNC_NAME)                                             \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                                          \
    {                                                                                           \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f);                                     \
    }                                                                                           \
    void eval_##FUNC_NAME##_bvec2(ShaderEvalContext &c)                                         \
    {                                                                                           \
        c.color.yz() = FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f))).asFloat();     \
    }                                                                                           \
    void eval_##FUNC_NAME##_bvec3(ShaderEvalContext &c)                                         \
    {                                                                                           \
        c.color.xyz() = FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f))).asFloat(); \
    }                                                                                           \
    void eval_##FUNC_NAME##_bvec4(ShaderEvalContext &c)                                         \
    {                                                                                           \
        c.color = FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f))).asFloat();    \
    }

#define DECLARE_TERNARY_BOOL_GENTYPE_FUNCS(FUNC_NAME)                                                                 \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                                                                \
    {                                                                                                                 \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f, c.in[2].y() > 0.0f);                   \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec2(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.yz() =                                                                                                \
            FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f)), greaterThan(c.in[1].swizzle(1, 0), Vec2(0.0f)), \
                      greaterThan(c.in[2].swizzle(2, 1), Vec2(0.0f)))                                                 \
                .asFloat();                                                                                           \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec3(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.xyz() = FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f)),                                  \
                                  greaterThan(c.in[1].swizzle(1, 2, 0), Vec3(0.0f)),                                  \
                                  greaterThan(c.in[2].swizzle(3, 1, 2), Vec3(0.0f)))                                  \
                            .asFloat();                                                                               \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec4(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color = FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f)),                                     \
                            greaterThan(c.in[1].swizzle(3, 2, 1, 0), Vec4(0.0f)),                                     \
                            greaterThan(c.in[2].swizzle(0, 3, 2, 1), Vec4(0.0f)))                                     \
                      .asFloat();                                                                                     \
    }

#define DECLARE_UNARY_INT_GENTYPE_FUNCS(FUNC_NAME)                             \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                          \
    {                                                                          \
        c.color.x() = (float)FUNC_NAME((int)c.in[0].z());                      \
    }                                                                          \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                        \
    {                                                                          \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asInt()).asFloat();     \
    }                                                                          \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                        \
    {                                                                          \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt()).asFloat(); \
    }                                                                          \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                        \
    {                                                                          \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt()).asFloat();    \
    }

#define DECLARE_BINARY_INT_GENTYPE_FUNCS(FUNC_NAME)                                                              \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                                                            \
    {                                                                                                            \
        c.color.x() = (float)FUNC_NAME((int)c.in[0].z(), (int)c.in[1].x());                                      \
    }                                                                                                            \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asInt(), c.in[1].swizzle(1, 0).asInt()).asFloat();        \
    }                                                                                                            \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt(), c.in[1].swizzle(1, 2, 0).asInt()).asFloat(); \
    }                                                                                                            \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt(), c.in[1].swizzle(3, 2, 1, 0).asInt()).asFloat(); \
    }

#define DECLARE_UNARY_UINT_GENTYPE_FUNCS(FUNC_NAME)                             \
    void eval_##FUNC_NAME##_uint(ShaderEvalContext &c)                          \
    {                                                                           \
        c.color.x() = (float)FUNC_NAME((uint32_t)c.in[0].z());                  \
    }                                                                           \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                         \
    {                                                                           \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asUint()).asFloat();     \
    }                                                                           \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                         \
    {                                                                           \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint()).asFloat(); \
    }                                                                           \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                         \
    {                                                                           \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint()).asFloat();    \
    }

#define DECLARE_BINARY_UINT_GENTYPE_FUNCS(FUNC_NAME)                                                               \
    void eval_##FUNC_NAME##_uint(ShaderEvalContext &c)                                                             \
    {                                                                                                              \
        c.color.x() = (float)FUNC_NAME((uint32_t)c.in[0].z(), (uint32_t)c.in[1].x());                              \
    }                                                                                                              \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), c.in[1].swizzle(1, 0).asUint()).asFloat();        \
    }                                                                                                              \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), c.in[1].swizzle(1, 2, 0).asUint()).asFloat(); \
    }                                                                                                              \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), c.in[1].swizzle(3, 2, 1, 0).asUint()).asFloat(); \
    }

#define DECLARE_TERNARY_INT_GENTYPE_FUNCS(FUNC_NAME)                                                               \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                                                              \
    {                                                                                                              \
        c.color.x() = (float)FUNC_NAME((int)c.in[0].z(), (int)c.in[1].x(), (int)c.in[2].y());                      \
    }                                                                                                              \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.yz() =                                                                                             \
            FUNC_NAME(c.in[0].swizzle(3, 1).asInt(), c.in[1].swizzle(1, 0).asInt(), c.in[2].swizzle(2, 1).asInt()) \
                .asFloat();                                                                                        \
    }                                                                                                              \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt(), c.in[1].swizzle(1, 2, 0).asInt(),              \
                                  c.in[2].swizzle(3, 1, 2).asInt())                                                \
                            .asFloat();                                                                            \
    }                                                                                                              \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt(), c.in[1].swizzle(3, 2, 1, 0).asInt(),              \
                            c.in[2].swizzle(0, 3, 2, 1).asInt())                                                   \
                      .asFloat();                                                                                  \
    }

#define DECLARE_TERNARY_UINT_GENTYPE_FUNCS(FUNC_NAME)                                                                 \
    void eval_##FUNC_NAME##_uint(ShaderEvalContext &c)                                                                \
    {                                                                                                                 \
        c.color.x() = (float)FUNC_NAME((uint32_t)c.in[0].z(), (uint32_t)c.in[1].x(), (uint32_t)c.in[2].y());          \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.yz() =                                                                                                \
            FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), c.in[1].swizzle(1, 0).asUint(), c.in[2].swizzle(2, 1).asUint()) \
                .asFloat();                                                                                           \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), c.in[1].swizzle(1, 2, 0).asUint(),               \
                                  c.in[2].swizzle(3, 1, 2).asUint())                                                  \
                            .asFloat();                                                                               \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), c.in[1].swizzle(3, 2, 1, 0).asUint(),               \
                            c.in[2].swizzle(0, 3, 2, 1).asUint())                                                     \
                      .asFloat();                                                                                     \
    }

#define DECLARE_VEC_FLOAT_FUNCS(FUNC_NAME)                                \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].x());     \
    }                                                                     \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].x()); \
    }                                                                     \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].x());    \
    }

#define DECLARE_VEC_FLOAT_FLOAT_FUNCS(FUNC_NAME)                                       \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].x(), c.in[2].y());     \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].x(), c.in[2].y()); \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].x(), c.in[2].y());    \
    }

#define DECLARE_VEC_VEC_FLOAT_FUNCS(FUNC_NAME)                                                      \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                              \
    {                                                                                               \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0), c.in[2].y());        \
    }                                                                                               \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                              \
    {                                                                                               \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0), c.in[2].y()); \
    }                                                                                               \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                              \
    {                                                                                               \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0), c.in[2].y()); \
    }

#define DECLARE_FLOAT_FLOAT_VEC_FUNCS(FUNC_NAME)                                       \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.yz() = FUNC_NAME(c.in[0].z(), c.in[1].x(), c.in[2].swizzle(2, 1));     \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.xyz() = FUNC_NAME(c.in[0].z(), c.in[1].x(), c.in[2].swizzle(3, 1, 2)); \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color = FUNC_NAME(c.in[0].z(), c.in[1].x(), c.in[2].swizzle(0, 3, 2, 1));    \
    }

#define DECLARE_FLOAT_VEC_FUNCS(FUNC_NAME)                                \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color.yz() = FUNC_NAME(c.in[0].z(), c.in[1].swizzle(1, 0));     \
    }                                                                     \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color.xyz() = FUNC_NAME(c.in[0].z(), c.in[1].swizzle(1, 2, 0)); \
    }                                                                     \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color = FUNC_NAME(c.in[0].z(), c.in[1].swizzle(3, 2, 1, 0));    \
    }

#define DECLARE_IVEC_INT_FUNCS(FUNC_NAME)                                                        \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asInt(), (int)c.in[1].x()).asFloat();     \
    }                                                                                            \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt(), (int)c.in[1].x()).asFloat(); \
    }                                                                                            \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt(), (int)c.in[1].x()).asFloat();    \
    }

#define DECLARE_IVEC_INT_INT_FUNCS(FUNC_NAME)                                                                      \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asInt(), (int)c.in[1].x(), (int)c.in[2].y()).asFloat();     \
    }                                                                                                              \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt(), (int)c.in[1].x(), (int)c.in[2].y()).asFloat(); \
    }                                                                                                              \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt(), (int)c.in[1].x(), (int)c.in[2].y()).asFloat();    \
    }

#define DECLARE_INT_IVEC_FUNCS(FUNC_NAME)                                                        \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.yz() = FUNC_NAME((int)c.in[0].z(), c.in[1].swizzle(1, 0).asInt()).asFloat();     \
    }                                                                                            \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.xyz() = FUNC_NAME((int)c.in[0].z(), c.in[1].swizzle(1, 2, 0).asInt()).asFloat(); \
    }                                                                                            \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color = FUNC_NAME((int)c.in[0].z(), c.in[1].swizzle(3, 2, 1, 0).asInt()).asFloat();    \
    }

#define DECLARE_UVEC_UINT_FUNCS(FUNC_NAME)                                                             \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                \
    {                                                                                                  \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), (uint32_t)c.in[1].x()).asFloat();     \
    }                                                                                                  \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                \
    {                                                                                                  \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), (uint32_t)c.in[1].x()).asFloat(); \
    }                                                                                                  \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                \
    {                                                                                                  \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), (uint32_t)c.in[1].x()).asFloat();    \
    }

#define DECLARE_UVEC_UINT_UINT_FUNCS(FUNC_NAME)                                                                      \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                              \
    {                                                                                                                \
        c.color.yz() =                                                                                               \
            FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), (uint32_t)c.in[1].x(), (uint32_t)c.in[2].y()).asFloat();       \
    }                                                                                                                \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                              \
    {                                                                                                                \
        c.color.xyz() =                                                                                              \
            FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), (uint32_t)c.in[1].x(), (uint32_t)c.in[2].y()).asFloat();    \
    }                                                                                                                \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                              \
    {                                                                                                                \
        c.color =                                                                                                    \
            FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), (uint32_t)c.in[1].x(), (uint32_t)c.in[2].y()).asFloat(); \
    }

#define DECLARE_UINT_UVEC_FUNCS(FUNC_NAME)                                                             \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                \
    {                                                                                                  \
        c.color.yz() = FUNC_NAME((uint32_t)c.in[0].z(), c.in[1].swizzle(1, 0).asUint()).asFloat();     \
    }                                                                                                  \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                \
    {                                                                                                  \
        c.color.xyz() = FUNC_NAME((uint32_t)c.in[0].z(), c.in[1].swizzle(1, 2, 0).asUint()).asFloat(); \
    }                                                                                                  \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                \
    {                                                                                                  \
        c.color = FUNC_NAME((uint32_t)c.in[0].z(), c.in[1].swizzle(3, 2, 1, 0).asUint()).asFloat();    \
    }

#define DECLARE_BINARY_INT_VEC_FUNCS(FUNC_NAME)                                                                  \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asInt(), c.in[1].swizzle(1, 0).asInt()).asFloat();        \
    }                                                                                                            \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt(), c.in[1].swizzle(1, 2, 0).asInt()).asFloat(); \
    }                                                                                                            \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt(), c.in[1].swizzle(3, 2, 1, 0).asInt()).asFloat(); \
    }

#define DECLARE_BINARY_UINT_VEC_FUNCS(FUNC_NAME)                                                                   \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), c.in[1].swizzle(1, 0).asUint()).asFloat();        \
    }                                                                                                              \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), c.in[1].swizzle(1, 2, 0).asUint()).asFloat(); \
    }                                                                                                              \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), c.in[1].swizzle(3, 2, 1, 0).asUint()).asFloat(); \
    }

#define DECLARE_UINT_INT_GENTYPE_FUNCS(FUNC_NAME)                                                                 \
    void eval_##FUNC_NAME##_uint(ShaderEvalContext &c)                                                            \
    {                                                                                                             \
        c.color.x() = (float)FUNC_NAME((uint32_t)c.in[0].z(), (int)c.in[1].x());                                  \
    }                                                                                                             \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), c.in[1].swizzle(1, 0).asInt()).asFloat();        \
    }                                                                                                             \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), c.in[1].swizzle(1, 2, 0).asInt()).asFloat(); \
    }                                                                                                             \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), c.in[1].swizzle(3, 2, 1, 0).asInt()).asFloat(); \
    }

#define DECLARE_UVEC_INT_FUNCS(FUNC_NAME)                                                         \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                           \
    {                                                                                             \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), (int)c.in[1].x()).asFloat();     \
    }                                                                                             \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                           \
    {                                                                                             \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), (int)c.in[1].x()).asFloat(); \
    }                                                                                             \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                           \
    {                                                                                             \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), (int)c.in[1].x()).asFloat();    \
    }

// Operators.

DECLARE_UNARY_GENTYPE_FUNCS(nop)
DECLARE_UNARY_GENTYPE_FUNCS(negate)
DECLARE_UNARY_GENTYPE_FUNCS(addOne)
DECLARE_UNARY_GENTYPE_FUNCS(subOne)
DECLARE_BINARY_GENTYPE_FUNCS(add)
DECLARE_BINARY_GENTYPE_FUNCS(sub)
DECLARE_BINARY_GENTYPE_FUNCS(mul)
DECLARE_BINARY_GENTYPE_FUNCS(div)

void eval_selection_float(ShaderEvalContext &c)
{
    c.color.x() = selection(c.in[0].z() > 0.0f, c.in[1].x(), c.in[2].y());
}
void eval_selection_vec2(ShaderEvalContext &c)
{
    c.color.yz() = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 0), c.in[2].swizzle(2, 1));
}
void eval_selection_vec3(ShaderEvalContext &c)
{
    c.color.xyz() = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 2, 0), c.in[2].swizzle(3, 1, 2));
}
void eval_selection_vec4(ShaderEvalContext &c)
{
    c.color = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(3, 2, 1, 0), c.in[2].swizzle(0, 3, 2, 1));
}

DECLARE_UNARY_INT_GENTYPE_FUNCS(nop)
DECLARE_UNARY_INT_GENTYPE_FUNCS(negate)
DECLARE_UNARY_INT_GENTYPE_FUNCS(addOne)
DECLARE_UNARY_INT_GENTYPE_FUNCS(subOne)
DECLARE_UNARY_INT_GENTYPE_FUNCS(bitwiseNot)
DECLARE_BINARY_INT_GENTYPE_FUNCS(add)
DECLARE_BINARY_INT_GENTYPE_FUNCS(sub)
DECLARE_BINARY_INT_GENTYPE_FUNCS(mul)
DECLARE_BINARY_INT_GENTYPE_FUNCS(div)
DECLARE_BINARY_INT_GENTYPE_FUNCS(mod)
DECLARE_BINARY_INT_GENTYPE_FUNCS(bitwiseAnd)
DECLARE_BINARY_INT_GENTYPE_FUNCS(bitwiseOr)
DECLARE_BINARY_INT_GENTYPE_FUNCS(bitwiseXor)

void eval_leftShift_int(ShaderEvalContext &c)
{
    c.color.x() = (float)leftShift((int)c.in[0].z(), (int)c.in[1].x());
}
DECLARE_BINARY_INT_VEC_FUNCS(leftShift)
void eval_rightShift_int(ShaderEvalContext &c)
{
    c.color.x() = (float)rightShift((int)c.in[0].z(), (int)c.in[1].x());
}
DECLARE_BINARY_INT_VEC_FUNCS(rightShift)
DECLARE_IVEC_INT_FUNCS(leftShiftVecScalar)
DECLARE_IVEC_INT_FUNCS(rightShiftVecScalar)

void eval_selection_int(ShaderEvalContext &c)
{
    c.color.x() = (float)selection(c.in[0].z() > 0.0f, (int)c.in[1].x(), (int)c.in[2].y());
}
void eval_selection_ivec2(ShaderEvalContext &c)
{
    c.color.yz() =
        selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 0).asInt(), c.in[2].swizzle(2, 1).asInt()).asFloat();
}
void eval_selection_ivec3(ShaderEvalContext &c)
{
    c.color.xyz() =
        selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 2, 0).asInt(), c.in[2].swizzle(3, 1, 2).asInt()).asFloat();
}
void eval_selection_ivec4(ShaderEvalContext &c)
{
    c.color = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(3, 2, 1, 0).asInt(), c.in[2].swizzle(0, 3, 2, 1).asInt())
                  .asFloat();
}

DECLARE_UNARY_UINT_GENTYPE_FUNCS(nop)
DECLARE_UNARY_UINT_GENTYPE_FUNCS(negate)
DECLARE_UNARY_UINT_GENTYPE_FUNCS(bitwiseNot)
DECLARE_UNARY_UINT_GENTYPE_FUNCS(addOne)
DECLARE_UNARY_UINT_GENTYPE_FUNCS(subOne)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(add)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(sub)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(mul)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(div)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(mod)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(bitwiseAnd)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(bitwiseOr)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(bitwiseXor)

DECLARE_UINT_INT_GENTYPE_FUNCS(leftShift)
DECLARE_UINT_INT_GENTYPE_FUNCS(rightShift)
DECLARE_UVEC_INT_FUNCS(leftShiftVecScalar)
DECLARE_UVEC_INT_FUNCS(rightShiftVecScalar)

void eval_selection_uint(ShaderEvalContext &c)
{
    c.color.x() = (float)selection(c.in[0].z() > 0.0f, (uint32_t)c.in[1].x(), (uint32_t)c.in[2].y());
}
void eval_selection_uvec2(ShaderEvalContext &c)
{
    c.color.yz() =
        selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 0).asUint(), c.in[2].swizzle(2, 1).asUint()).asFloat();
}
void eval_selection_uvec3(ShaderEvalContext &c)
{
    c.color.xyz() =
        selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 2, 0).asUint(), c.in[2].swizzle(3, 1, 2).asUint()).asFloat();
}
void eval_selection_uvec4(ShaderEvalContext &c)
{
    c.color = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(3, 2, 1, 0).asUint(), c.in[2].swizzle(0, 3, 2, 1).asUint())
                  .asFloat();
}

DECLARE_UNARY_BOOL_GENTYPE_FUNCS(boolNot)
DECLARE_BINARY_BOOL_FUNCS(logicalAnd)
DECLARE_BINARY_BOOL_FUNCS(logicalOr)
DECLARE_BINARY_BOOL_FUNCS(logicalXor)

void eval_selection_bool(ShaderEvalContext &c)
{
    c.color.x() = (float)selection(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f, c.in[2].y() > 0.0f);
}
void eval_selection_bvec2(ShaderEvalContext &c)
{
    c.color.yz() = selection(c.in[0].z() > 0.0f, greaterThan(c.in[1].swizzle(1, 0), Vec2(0.0f, 0.0f)),
                             greaterThan(c.in[2].swizzle(2, 1), Vec2(0.0f, 0.0f)))
                       .asFloat();
}
void eval_selection_bvec3(ShaderEvalContext &c)
{
    c.color.xyz() = selection(c.in[0].z() > 0.0f, greaterThan(c.in[1].swizzle(1, 2, 0), Vec3(0.0f, 0.0f, 0.0f)),
                              greaterThan(c.in[2].swizzle(3, 1, 2), Vec3(0.0f, 0.0f, 0.0f)))
                        .asFloat();
}
void eval_selection_bvec4(ShaderEvalContext &c)
{
    c.color = selection(c.in[0].z() > 0.0f, greaterThan(c.in[1].swizzle(3, 2, 1, 0), Vec4(0.0f, 0.0f, 0.0f, 0.0f)),
                        greaterThan(c.in[2].swizzle(0, 3, 2, 1), Vec4(0.0f, 0.0f, 0.0f, 0.0f)))
                  .asFloat();
}

DECLARE_VEC_FLOAT_FUNCS(addVecScalar)
DECLARE_VEC_FLOAT_FUNCS(subVecScalar)
DECLARE_VEC_FLOAT_FUNCS(mulVecScalar)
DECLARE_VEC_FLOAT_FUNCS(divVecScalar)

DECLARE_FLOAT_VEC_FUNCS(addScalarVec)
DECLARE_FLOAT_VEC_FUNCS(subScalarVec)
DECLARE_FLOAT_VEC_FUNCS(mulScalarVec)
DECLARE_FLOAT_VEC_FUNCS(divScalarVec)

DECLARE_IVEC_INT_FUNCS(addVecScalar)
DECLARE_IVEC_INT_FUNCS(subVecScalar)
DECLARE_IVEC_INT_FUNCS(mulVecScalar)
DECLARE_IVEC_INT_FUNCS(divVecScalar)
DECLARE_IVEC_INT_FUNCS(modVecScalar)
DECLARE_IVEC_INT_FUNCS(bitwiseAndVecScalar)
DECLARE_IVEC_INT_FUNCS(bitwiseOrVecScalar)
DECLARE_IVEC_INT_FUNCS(bitwiseXorVecScalar)

DECLARE_INT_IVEC_FUNCS(addScalarVec)
DECLARE_INT_IVEC_FUNCS(subScalarVec)
DECLARE_INT_IVEC_FUNCS(mulScalarVec)
DECLARE_INT_IVEC_FUNCS(divScalarVec)
DECLARE_INT_IVEC_FUNCS(modScalarVec)
DECLARE_INT_IVEC_FUNCS(bitwiseAndScalarVec)
DECLARE_INT_IVEC_FUNCS(bitwiseOrScalarVec)
DECLARE_INT_IVEC_FUNCS(bitwiseXorScalarVec)

DECLARE_UVEC_UINT_FUNCS(addVecScalar)
DECLARE_UVEC_UINT_FUNCS(subVecScalar)
DECLARE_UVEC_UINT_FUNCS(mulVecScalar)
DECLARE_UVEC_UINT_FUNCS(divVecScalar)
DECLARE_UVEC_UINT_FUNCS(modVecScalar)
DECLARE_UVEC_UINT_FUNCS(bitwiseAndVecScalar)
DECLARE_UVEC_UINT_FUNCS(bitwiseOrVecScalar)
DECLARE_UVEC_UINT_FUNCS(bitwiseXorVecScalar)

DECLARE_UINT_UVEC_FUNCS(addScalarVec)
DECLARE_UINT_UVEC_FUNCS(subScalarVec)
DECLARE_UINT_UVEC_FUNCS(mulScalarVec)
DECLARE_UINT_UVEC_FUNCS(divScalarVec)
DECLARE_UINT_UVEC_FUNCS(modScalarVec)
DECLARE_UINT_UVEC_FUNCS(bitwiseAndScalarVec)
DECLARE_UINT_UVEC_FUNCS(bitwiseOrScalarVec)
DECLARE_UINT_UVEC_FUNCS(bitwiseXorScalarVec)

// Built-in functions.
DECLARE_BINARY_INT_GENTYPE_FUNCS(min)
DECLARE_IVEC_INT_FUNCS(minVecScalar)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(min)
DECLARE_UVEC_UINT_FUNCS(minVecScalar)
DECLARE_BINARY_INT_GENTYPE_FUNCS(max)
DECLARE_IVEC_INT_FUNCS(maxVecScalar)
DECLARE_BINARY_UINT_GENTYPE_FUNCS(max)
DECLARE_UVEC_UINT_FUNCS(maxVecScalar)
DECLARE_TERNARY_INT_GENTYPE_FUNCS(clamp)
DECLARE_IVEC_INT_INT_FUNCS(clampVecScalarScalar)
DECLARE_TERNARY_UINT_GENTYPE_FUNCS(clamp)
DECLARE_UVEC_UINT_UINT_FUNCS(clampVecScalarScalar)

// Compare functions.

#define DECLARE_FLOAT_COMPARE_FUNCS(FUNC_NAME)                                                    \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                           \
    {                                                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].z(), c.in[1].x());                                 \
    }                                                                                             \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                            \
    {                                                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0));             \
    }                                                                                             \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                            \
    {                                                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0));       \
    }                                                                                             \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                            \
    {                                                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0)); \
    }

#define DECLARE_FLOAT_CWISE_COMPARE_FUNCS(FUNC_NAME)              \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)           \
    {                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].z(), c.in[1].x()); \
    }                                                             \
    DECLARE_FLOAT_VEC_CWISE_COMPARE_FUNCS(FUNC_NAME)

#define DECLARE_FLOAT_VEC_CWISE_COMPARE_FUNCS(FUNC_NAME)                                         \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                           \
    {                                                                                            \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0)).asFloat();        \
    }                                                                                            \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                           \
    {                                                                                            \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0)).asFloat(); \
    }                                                                                            \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                           \
    {                                                                                            \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0)).asFloat(); \
    }

#define DECLARE_INT_COMPARE_FUNCS(FUNC_NAME)                                                                      \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                                                             \
    {                                                                                                             \
        c.color.x() = (float)FUNC_NAME(chopToInt(c.in[0].z()), chopToInt(c.in[1].x()));                           \
    }                                                                                                             \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color.x() = (float)FUNC_NAME(chopToInt(c.in[0].swizzle(3, 1)), chopToInt(c.in[1].swizzle(1, 0)));       \
    }                                                                                                             \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color.x() = (float)FUNC_NAME(chopToInt(c.in[0].swizzle(2, 0, 1)), chopToInt(c.in[1].swizzle(1, 2, 0))); \
    }                                                                                                             \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color.x() =                                                                                             \
            (float)FUNC_NAME(chopToInt(c.in[0].swizzle(1, 2, 3, 0)), chopToInt(c.in[1].swizzle(3, 2, 1, 0)));     \
    }

#define DECLARE_INT_CWISE_COMPARE_FUNCS(FUNC_NAME)                                      \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                                   \
    {                                                                                   \
        c.color.x() = (float)FUNC_NAME(chopToInt(c.in[0].z()), chopToInt(c.in[1].x())); \
    }                                                                                   \
    DECLARE_INT_VEC_CWISE_COMPARE_FUNCS(FUNC_NAME)

#define DECLARE_INT_VEC_CWISE_COMPARE_FUNCS(FUNC_NAME)                                                                 \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                                \
    {                                                                                                                  \
        c.color.yz() = FUNC_NAME(chopToInt(c.in[0].swizzle(3, 1)), chopToInt(c.in[1].swizzle(1, 0))).asFloat();        \
    }                                                                                                                  \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                                \
    {                                                                                                                  \
        c.color.xyz() = FUNC_NAME(chopToInt(c.in[0].swizzle(2, 0, 1)), chopToInt(c.in[1].swizzle(1, 2, 0))).asFloat(); \
    }                                                                                                                  \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                                \
    {                                                                                                                  \
        c.color = FUNC_NAME(chopToInt(c.in[0].swizzle(1, 2, 3, 0)), chopToInt(c.in[1].swizzle(3, 2, 1, 0))).asFloat(); \
    }

#define DECLARE_UINT_COMPARE_FUNCS(FUNC_NAME)                                                                       \
    void eval_##FUNC_NAME##_uint(ShaderEvalContext &c)                                                              \
    {                                                                                                               \
        c.color.x() = (float)FUNC_NAME((uint32_t)c.in[0].z(), (uint32_t)c.in[1].x());                               \
    }                                                                                                               \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                             \
    {                                                                                                               \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), c.in[1].swizzle(1, 0).asUint());             \
    }                                                                                                               \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                             \
    {                                                                                                               \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), c.in[1].swizzle(1, 2, 0).asUint());       \
    }                                                                                                               \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                             \
    {                                                                                                               \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), c.in[1].swizzle(3, 2, 1, 0).asUint()); \
    }

#define DECLARE_UINT_CWISE_COMPARE_FUNCS(FUNC_NAME)                                                                \
    DECLARE_UINT_SCALAR_CWISE_COMPARE_FUNCS(FUNC_NAME)                                                             \
    void eval_##FUNC_NAME##_uvec2(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asUint(), c.in[1].swizzle(1, 0).asUint()).asFloat();        \
    }                                                                                                              \
    void eval_##FUNC_NAME##_uvec3(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asUint(), c.in[1].swizzle(1, 2, 0).asUint()).asFloat(); \
    }                                                                                                              \
    void eval_##FUNC_NAME##_uvec4(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asUint(), c.in[1].swizzle(3, 2, 1, 0).asUint()).asFloat(); \
    }

#define DECLARE_UINT_SCALAR_CWISE_COMPARE_FUNCS(FUNC_NAME)                            \
    void eval_##FUNC_NAME##_uint(ShaderEvalContext &c)                                \
    {                                                                                 \
        c.color.x() = (float)FUNC_NAME((uint32_t)c.in[0].z(), (uint32_t)c.in[1].x()); \
    }

#define DECLARE_BOOL_COMPARE_FUNCS(FUNC_NAME)                                                 \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                                        \
    {                                                                                         \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f);               \
    }                                                                                         \
    void eval_##FUNC_NAME##_bvec2(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = (float)FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f)),        \
                                       greaterThan(c.in[1].swizzle(1, 0), Vec2(0.0f)));       \
    }                                                                                         \
    void eval_##FUNC_NAME##_bvec3(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = (float)FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f)),     \
                                       greaterThan(c.in[1].swizzle(1, 2, 0), Vec3(0.0f)));    \
    }                                                                                         \
    void eval_##FUNC_NAME##_bvec4(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = (float)FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f)),  \
                                       greaterThan(c.in[1].swizzle(3, 2, 1, 0), Vec4(0.0f))); \
    }

#define DECLARE_BOOL_CWISE_COMPARE_FUNCS(FUNC_NAME)                             \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                          \
    {                                                                           \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f); \
    }                                                                           \
    DECLARE_BOOL_VEC_CWISE_COMPARE_FUNCS(FUNC_NAME)

#define DECLARE_BOOL_VEC_CWISE_COMPARE_FUNCS(FUNC_NAME)                                                               \
    void eval_##FUNC_NAME##_bvec2(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.yz() =                                                                                                \
            FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f)), greaterThan(c.in[1].swizzle(1, 0), Vec2(0.0f))) \
                .asFloat();                                                                                           \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec3(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.xyz() = FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f)),                                  \
                                  greaterThan(c.in[1].swizzle(1, 2, 0), Vec3(0.0f)))                                  \
                            .asFloat();                                                                               \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec4(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color = FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f)),                                     \
                            greaterThan(c.in[1].swizzle(3, 2, 1, 0), Vec4(0.0f)))                                     \
                      .asFloat();                                                                                     \
    }

DECLARE_FLOAT_COMPARE_FUNCS(allEqual)
DECLARE_FLOAT_COMPARE_FUNCS(anyNotEqual)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(lessThan)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(lessThanEqual)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(greaterThan)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(greaterThanEqual)
DECLARE_FLOAT_VEC_CWISE_COMPARE_FUNCS(equal)
DECLARE_FLOAT_VEC_CWISE_COMPARE_FUNCS(notEqual)

DECLARE_INT_COMPARE_FUNCS(allEqual)
DECLARE_INT_COMPARE_FUNCS(anyNotEqual)
DECLARE_INT_CWISE_COMPARE_FUNCS(lessThan)
DECLARE_INT_CWISE_COMPARE_FUNCS(lessThanEqual)
DECLARE_INT_CWISE_COMPARE_FUNCS(greaterThan)
DECLARE_INT_CWISE_COMPARE_FUNCS(greaterThanEqual)
DECLARE_INT_VEC_CWISE_COMPARE_FUNCS(equal)
DECLARE_INT_VEC_CWISE_COMPARE_FUNCS(notEqual)

DECLARE_UINT_COMPARE_FUNCS(allEqual)
DECLARE_UINT_COMPARE_FUNCS(anyNotEqual)
DECLARE_UINT_SCALAR_CWISE_COMPARE_FUNCS(lessThan)
DECLARE_UINT_SCALAR_CWISE_COMPARE_FUNCS(lessThanEqual)
DECLARE_UINT_SCALAR_CWISE_COMPARE_FUNCS(greaterThan)
DECLARE_UINT_SCALAR_CWISE_COMPARE_FUNCS(greaterThanEqual)

DECLARE_BOOL_COMPARE_FUNCS(allEqual)
DECLARE_BOOL_COMPARE_FUNCS(anyNotEqual)
DECLARE_BOOL_VEC_CWISE_COMPARE_FUNCS(equal)
DECLARE_BOOL_VEC_CWISE_COMPARE_FUNCS(notEqual)

// Boolean functions.

#define DECLARE_UNARY_SCALAR_BVEC_FUNCS(GLSL_NAME, FUNC_NAME)                                 \
    void eval_##GLSL_NAME##_bvec2(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = float(FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f))));       \
    }                                                                                         \
    void eval_##GLSL_NAME##_bvec3(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = float(FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f))));    \
    }                                                                                         \
    void eval_##GLSL_NAME##_bvec4(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = float(FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f)))); \
    }

#define DECLARE_UNARY_BVEC_BVEC_FUNCS(GLSL_NAME, FUNC_NAME)                                         \
    void eval_##GLSL_NAME##_bvec2(ShaderEvalContext &c)                                             \
    {                                                                                               \
        c.color.yz() = FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f))).asFloat();         \
    }                                                                                               \
    void eval_##GLSL_NAME##_bvec3(ShaderEvalContext &c)                                             \
    {                                                                                               \
        c.color.xyz() = FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f))).asFloat();     \
    }                                                                                               \
    void eval_##GLSL_NAME##_bvec4(ShaderEvalContext &c)                                             \
    {                                                                                               \
        c.color.xyzw() = FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f))).asFloat(); \
    }

DECLARE_UNARY_SCALAR_BVEC_FUNCS(any, boolAny)
DECLARE_UNARY_SCALAR_BVEC_FUNCS(all, boolAll)

// ShaderOperatorTests.

class ShaderOperatorTests : public tcu::TestCaseGroup
{
public:
    ShaderOperatorTests(tcu::TestContext &context);
    virtual ~ShaderOperatorTests(void);

    virtual void init(void);

private:
    ShaderOperatorTests(const ShaderOperatorTests &);            // not allowed!
    ShaderOperatorTests &operator=(const ShaderOperatorTests &); // not allowed!
};

ShaderOperatorTests::ShaderOperatorTests(tcu::TestContext &testCtx) : TestCaseGroup(testCtx, "operator")
{
}

ShaderOperatorTests::~ShaderOperatorTests(void)
{
}

void ShaderOperatorTests::init(void)
{
#define BOOL_FUNCS(FUNC_NAME) eval_##FUNC_NAME##_bool, DE_NULL, DE_NULL, DE_NULL

#define FLOAT_VEC_FUNCS(FUNC_NAME) DE_NULL, eval_##FUNC_NAME##_vec2, eval_##FUNC_NAME##_vec3, eval_##FUNC_NAME##_vec4
#define INT_VEC_FUNCS(FUNC_NAME) DE_NULL, eval_##FUNC_NAME##_ivec2, eval_##FUNC_NAME##_ivec3, eval_##FUNC_NAME##_ivec4
#define UINT_VEC_FUNCS(FUNC_NAME) DE_NULL, eval_##FUNC_NAME##_uvec2, eval_##FUNC_NAME##_uvec3, eval_##FUNC_NAME##_uvec4
#define BOOL_VEC_FUNCS(FUNC_NAME) DE_NULL, eval_##FUNC_NAME##_bvec2, eval_##FUNC_NAME##_bvec3, eval_##FUNC_NAME##_bvec4

#define FLOAT_GENTYPE_FUNCS(FUNC_NAME) \
    eval_##FUNC_NAME##_float, eval_##FUNC_NAME##_vec2, eval_##FUNC_NAME##_vec3, eval_##FUNC_NAME##_vec4
#define INT_GENTYPE_FUNCS(FUNC_NAME) \
    eval_##FUNC_NAME##_int, eval_##FUNC_NAME##_ivec2, eval_##FUNC_NAME##_ivec3, eval_##FUNC_NAME##_ivec4
#define UINT_GENTYPE_FUNCS(FUNC_NAME) \
    eval_##FUNC_NAME##_uint, eval_##FUNC_NAME##_uvec2, eval_##FUNC_NAME##_uvec3, eval_##FUNC_NAME##_uvec4
#define BOOL_GENTYPE_FUNCS(FUNC_NAME) \
    eval_##FUNC_NAME##_bool, eval_##FUNC_NAME##_bvec2, eval_##FUNC_NAME##_bvec3, eval_##FUNC_NAME##_bvec4

    // Shorthands.
    Value notUsed = Value(VALUE_NONE, 0.0f, 0.0f);

    std::vector<BuiltinFuncGroup> funcInfoGroups;

    // Unary operators.
    funcInfoGroups.push_back(
        BuiltinFuncGroup("unary_operator", "Unary operator tests")
        << BuiltinOperInfo("minus", "-", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.5f, PRECMASK_ALL,
                           FLOAT_GENTYPE_FUNCS(negate))
        << BuiltinOperInfo("minus", "-", IGT, Value(IGT, -5.0f, 5.0f), notUsed, notUsed, 0.1f, 0.5f, PRECMASK_ALL,
                           INT_GENTYPE_FUNCS(negate))
        << BuiltinOperInfo("minus", "-", UGT, Value(UGT, 0.0f, 4e9f), notUsed, notUsed, 2e-10f, 0.0f, PRECMASK_HIGHP,
                           UINT_GENTYPE_FUNCS(negate))
        << BuiltinOperInfo("not", "!", B, Value(B, -1.0f, 1.0f), notUsed, notUsed, 1.0f, 0.0f, PRECMASK_NA,
                           eval_boolNot_bool, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("bitwise_not", "~", IGT, Value(IGT, -1e5f, 1e5f), notUsed, notUsed, 5e-5f, 0.5f,
                           PRECMASK_HIGHP, INT_GENTYPE_FUNCS(bitwiseNot))
        << BuiltinOperInfo("bitwise_not", "~", UGT, Value(UGT, 0.0f, 2e9f), notUsed, notUsed, 2e-10f, 0.0f,
                           PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(bitwiseNot))

        // Pre/post incr/decr side effect cases.
        << BuiltinSideEffOperInfo("pre_increment_effect", "++", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f,
                                  0.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(addOne))
        << BuiltinSideEffOperInfo("pre_increment_effect", "++", IGT, Value(IGT, -6.0f, 4.0f), notUsed, notUsed, 0.1f,
                                  0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(addOne))
        << BuiltinSideEffOperInfo("pre_increment_effect", "++", UGT, Value(UGT, 0.0f, 9.0f), notUsed, notUsed, 0.1f,
                                  0.0f, PRECMASK_ALL, UINT_GENTYPE_FUNCS(addOne))
        << BuiltinSideEffOperInfo("pre_decrement_effect", "--", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f,
                                  1.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(subOne))
        << BuiltinSideEffOperInfo("pre_decrement_effect", "--", IGT, Value(IGT, -4.0f, 6.0f), notUsed, notUsed, 0.1f,
                                  0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(subOne))
        << BuiltinSideEffOperInfo("pre_decrement_effect", "--", UGT, Value(UGT, 1.0f, 10.0f), notUsed, notUsed, 0.1f,
                                  0.0f, PRECMASK_ALL, UINT_GENTYPE_FUNCS(subOne))
        << BuiltinPostSideEffOperInfo("post_increment_effect", "++", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f,
                                      0.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(addOne))
        << BuiltinPostSideEffOperInfo("post_increment_effect", "++", IGT, Value(IGT, -6.0f, 4.0f), notUsed, notUsed,
                                      0.1f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(addOne))
        << BuiltinPostSideEffOperInfo("post_increment_effect", "++", UGT, Value(UGT, 0.0f, 9.0f), notUsed, notUsed,
                                      0.1f, 0.0f, PRECMASK_ALL, UINT_GENTYPE_FUNCS(addOne))
        << BuiltinPostSideEffOperInfo("post_decrement_effect", "--", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f,
                                      1.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(subOne))
        << BuiltinPostSideEffOperInfo("post_decrement_effect", "--", IGT, Value(IGT, -4.0f, 6.0f), notUsed, notUsed,
                                      0.1f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(subOne))
        << BuiltinPostSideEffOperInfo("post_decrement_effect", "--", UGT, Value(UGT, 1.0f, 10.0f), notUsed, notUsed,
                                      0.1f, 0.0f, PRECMASK_ALL, UINT_GENTYPE_FUNCS(subOne))

        // Pre/post incr/decr result cases.
        << BuiltinOperInfo("pre_increment_result", "++", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(addOne))
        << BuiltinOperInfo("pre_increment_result", "++", IGT, Value(IGT, -6.0f, 4.0f), notUsed, notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(addOne))
        << BuiltinOperInfo("pre_increment_result", "++", UGT, Value(UGT, 0.0f, 9.0f), notUsed, notUsed, 0.1f, 0.0f,
                           PRECMASK_ALL, UINT_GENTYPE_FUNCS(addOne))
        << BuiltinOperInfo("pre_decrement_result", "--", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 1.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(subOne))
        << BuiltinOperInfo("pre_decrement_result", "--", IGT, Value(IGT, -4.0f, 6.0f), notUsed, notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(subOne))
        << BuiltinOperInfo("pre_decrement_result", "--", UGT, Value(UGT, 1.0f, 10.0f), notUsed, notUsed, 0.1f, 0.0f,
                           PRECMASK_ALL, UINT_GENTYPE_FUNCS(subOne))
        << BuiltinPostOperInfo("post_increment_result", "++", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.5f,
                               PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(nop))
        << BuiltinPostOperInfo("post_increment_result", "++", IGT, Value(IGT, -5.0f, 5.0f), notUsed, notUsed, 0.1f,
                               0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(nop))
        << BuiltinPostOperInfo("post_increment_result", "++", UGT, Value(UGT, 0.0f, 9.0f), notUsed, notUsed, 0.1f, 0.0f,
                               PRECMASK_ALL, UINT_GENTYPE_FUNCS(nop))
        << BuiltinPostOperInfo("post_decrement_result", "--", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.5f,
                               PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(nop))
        << BuiltinPostOperInfo("post_decrement_result", "--", IGT, Value(IGT, -5.0f, 5.0f), notUsed, notUsed, 0.1f,
                               0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(nop))
        << BuiltinPostOperInfo("post_decrement_result", "--", UGT, Value(UGT, 1.0f, 10.0f), notUsed, notUsed, 0.1f,
                               0.0f, PRECMASK_ALL, UINT_GENTYPE_FUNCS(nop)));

    BuiltinFuncGroup binaryOpGroup("binary_operator", "Binary operator tests");

    // Normal binary operations and their corresponding assignment operations have lots in common; generate both in the following loop.

    for (int binaryOperatorType = 0; binaryOperatorType <= 2;
         binaryOperatorType++) // 0: normal op test, 1: assignment op side-effect test, 2: assignment op result test
    {
        const bool isNormalOp  = binaryOperatorType == 0;
        const bool isAssignEff = binaryOperatorType == 1;
        const bool isAssignRes = binaryOperatorType == 2;

        DE_ASSERT(isNormalOp || isAssignEff || isAssignRes);
        DE_UNREF(isAssignRes);

        const char *addName        = isNormalOp ? "add" : isAssignEff ? "add_assign_effect" : "add_assign_result";
        const char *subName        = isNormalOp ? "sub" : isAssignEff ? "sub_assign_effect" : "sub_assign_result";
        const char *mulName        = isNormalOp ? "mul" : isAssignEff ? "mul_assign_effect" : "mul_assign_result";
        const char *divName        = isNormalOp ? "div" : isAssignEff ? "div_assign_effect" : "div_assign_result";
        const char *modName        = isNormalOp ? "mod" : isAssignEff ? "mod_assign_effect" : "mod_assign_result";
        const char *andName        = isNormalOp  ? "bitwise_and" :
                                     isAssignEff ? "bitwise_and_assign_effect" :
                                                   "bitwise_and_assign_result";
        const char *orName         = isNormalOp  ? "bitwise_or" :
                                     isAssignEff ? "bitwise_or_assign_effect" :
                                                   "bitwise_or_assign_result";
        const char *xorName        = isNormalOp  ? "bitwise_xor" :
                                     isAssignEff ? "bitwise_xor_assign_effect" :
                                                   "bitwise_xor_assign_result";
        const char *leftShiftName  = isNormalOp  ? "left_shift" :
                                     isAssignEff ? "left_shift_assign_effect" :
                                                   "left_shift_assign_result";
        const char *rightShiftName = isNormalOp  ? "right_shift" :
                                     isAssignEff ? "right_shift_assign_effect" :
                                                   "right_shift_assign_result";
        const char *addOp          = isNormalOp ? "+" : "+=";
        const char *subOp          = isNormalOp ? "-" : "-=";
        const char *mulOp          = isNormalOp ? "*" : "*=";
        const char *divOp          = isNormalOp ? "/" : "/=";
        const char *modOp          = isNormalOp ? "%" : "%=";
        const char *andOp          = isNormalOp ? "&" : "&=";
        const char *orOp           = isNormalOp ? "|" : "|=";
        const char *xorOp          = isNormalOp ? "^" : "^=";
        const char *leftShiftOp    = isNormalOp ? "<<" : "<<=";
        const char *rightShiftOp   = isNormalOp ? ">>" : ">>=";

        // Pointer to appropriate OperInfo function.
        BuiltinFuncInfo (*operInfoFunc)(const char *, const char *, ValueType, Value, Value, Value, const float,
                                        const float, uint32_t, ShaderEvalFunc, ShaderEvalFunc, ShaderEvalFunc,
                                        ShaderEvalFunc) = isAssignEff ? BuiltinSideEffOperInfo : BuiltinOperInfo;

        DE_ASSERT(operInfoFunc != DE_NULL);

        // The following cases will be added for each operator, precision and fundamental type (float, int, uint) combination, where applicable:
        // gentype <op> gentype
        // vector <op> scalar
        // For normal (non-assigning) operators only:
        //   scalar <op> vector

        // The add operator.

        binaryOpGroup << operInfoFunc(addName, addOp, GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f,
                                      0.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(add))
                      << operInfoFunc(addName, addOp, IGT, Value(IGT, -4.0f, 6.0f), Value(IGT, -6.0f, 5.0f), notUsed,
                                      0.1f, 0.5f, PRECMASK_MEDIUMP, INT_GENTYPE_FUNCS(add))
                      << operInfoFunc(addName, addOp, IGT, Value(IGT, -2e9f, 2e9f), Value(IGT, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_GENTYPE_FUNCS(add))
                      << operInfoFunc(addName, addOp, UGT, Value(UGT, 0.0f, 1e2f), Value(UGT, 0.0f, 1e2f), notUsed,
                                      5e-3f, 0.0f, PRECMASK_MEDIUMP, UINT_GENTYPE_FUNCS(add))
                      << operInfoFunc(addName, addOp, UGT, Value(UGT, 0.0f, 4e9f), Value(UGT, 0.0f, 4e9f), notUsed,
                                      2e-10f, 0.0f, PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(add))
                      << operInfoFunc(addName, addOp, FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f,
                                      0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(addVecScalar))
                      << operInfoFunc(addName, addOp, IV, Value(IV, -4.0f, 6.0f), Value(I, -6.0f, 5.0f), notUsed, 0.1f,
                                      0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(addVecScalar))
                      << operInfoFunc(addName, addOp, IV, Value(IV, -2e9f, 2e9f), Value(I, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(addVecScalar))
                      << operInfoFunc(addName, addOp, UV, Value(UV, 0.0f, 1e2f), Value(U, 0.0f, 1e2f), notUsed, 5e-3f,
                                      0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(addVecScalar))
                      << operInfoFunc(addName, addOp, UV, Value(UV, 0.0f, 4e9f), Value(U, 0.0f, 4e9f), notUsed, 2e-10f,
                                      0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(addVecScalar));

        if (isNormalOp)
            binaryOpGroup << operInfoFunc(addName, addOp, FV, Value(F, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed,
                                          1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(addScalarVec))
                          << operInfoFunc(addName, addOp, IV, Value(I, -4.0f, 6.0f), Value(IV, -6.0f, 5.0f), notUsed,
                                          0.1f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(addScalarVec))
                          << operInfoFunc(addName, addOp, IV, Value(I, -2e9f, 2e9f), Value(IV, -2e9f, 2e9f), notUsed,
                                          4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(addScalarVec))
                          << operInfoFunc(addName, addOp, UV, Value(U, 0.0f, 1e2f), Value(UV, 0.0f, 1e2f), notUsed,
                                          5e-3f, 0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(addScalarVec))
                          << operInfoFunc(addName, addOp, UV, Value(U, 0.0f, 4e9f), Value(UV, 0.0f, 4e9f), notUsed,
                                          2e-10f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(addScalarVec));

        // The subtract operator.

        binaryOpGroup << operInfoFunc(subName, subOp, GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f,
                                      0.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(sub))
                      << operInfoFunc(subName, subOp, IGT, Value(IGT, -4.0f, 6.0f), Value(IGT, -6.0f, 5.0f), notUsed,
                                      0.1f, 0.5f, PRECMASK_MEDIUMP, INT_GENTYPE_FUNCS(sub))
                      << operInfoFunc(subName, subOp, IGT, Value(IGT, -2e9f, 2e9f), Value(IGT, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_GENTYPE_FUNCS(sub))
                      << operInfoFunc(subName, subOp, UGT, Value(UGT, 1e2f, 2e2f), Value(UGT, 0.0f, 1e2f), notUsed,
                                      5e-3f, 0.0f, PRECMASK_MEDIUMP, UINT_GENTYPE_FUNCS(sub))
                      << operInfoFunc(subName, subOp, UGT, Value(UGT, .5e9f, 3.7e9f), Value(UGT, 0.0f, 3.9e9f), notUsed,
                                      2e-10f, 0.0f, PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(sub))
                      << operInfoFunc(subName, subOp, FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f,
                                      0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(subVecScalar))
                      << operInfoFunc(subName, subOp, IV, Value(IV, -4.0f, 6.0f), Value(I, -6.0f, 5.0f), notUsed, 0.1f,
                                      0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(subVecScalar))
                      << operInfoFunc(subName, subOp, IV, Value(IV, -2e9f, 2e9f), Value(I, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(subVecScalar))
                      << operInfoFunc(subName, subOp, UV, Value(UV, 1e2f, 2e2f), Value(U, 0.0f, 1e2f), notUsed, 5e-3f,
                                      0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(subVecScalar))
                      << operInfoFunc(subName, subOp, UV, Value(UV, 0.0f, 4e9f), Value(U, 0.0f, 4e9f), notUsed, 2e-10f,
                                      0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(subVecScalar));

        if (isNormalOp)
            binaryOpGroup << operInfoFunc(subName, subOp, FV, Value(F, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed,
                                          1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(subScalarVec))
                          << operInfoFunc(subName, subOp, IV, Value(I, -4.0f, 6.0f), Value(IV, -6.0f, 5.0f), notUsed,
                                          0.1f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(subScalarVec))
                          << operInfoFunc(subName, subOp, IV, Value(I, -2e9f, 2e9f), Value(IV, -2e9f, 2e9f), notUsed,
                                          4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(subScalarVec))
                          << operInfoFunc(subName, subOp, UV, Value(U, 1e2f, 2e2f), Value(UV, 0.0f, 1e2f), notUsed,
                                          5e-3f, 0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(subScalarVec))
                          << operInfoFunc(subName, subOp, UV, Value(U, 0.0f, 4e9f), Value(UV, 0.0f, 4e9f), notUsed,
                                          2e-10f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(subScalarVec));

        // The multiply operator.

        binaryOpGroup << operInfoFunc(mulName, mulOp, GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f,
                                      0.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(mul))
                      << operInfoFunc(mulName, mulOp, IGT, Value(IGT, -4.0f, 6.0f), Value(IGT, -6.0f, 5.0f), notUsed,
                                      0.1f, 0.5f, PRECMASK_MEDIUMP, INT_GENTYPE_FUNCS(mul))
                      << operInfoFunc(mulName, mulOp, IGT, Value(IGT, -3e5f, 3e5f), Value(IGT, -3e4f, 3e4f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_GENTYPE_FUNCS(mul))
                      << operInfoFunc(mulName, mulOp, UGT, Value(UGT, 0.0f, 16.0f), Value(UGT, 0.0f, 16.0f), notUsed,
                                      4e-3f, 0.0f, PRECMASK_MEDIUMP, UINT_GENTYPE_FUNCS(mul))
                      << operInfoFunc(mulName, mulOp, UGT, Value(UGT, 0.0f, 6e5f), Value(UGT, 0.0f, 6e4f), notUsed,
                                      2e-10f, 0.0f, PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(mul))
                      << operInfoFunc(mulName, mulOp, FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f,
                                      0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(mulVecScalar))
                      << operInfoFunc(mulName, mulOp, IV, Value(IV, -4.0f, 6.0f), Value(I, -6.0f, 5.0f), notUsed, 0.1f,
                                      0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(mulVecScalar))
                      << operInfoFunc(mulName, mulOp, IV, Value(IV, -3e5f, 3e5f), Value(I, -3e4f, 3e4f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(mulVecScalar))
                      << operInfoFunc(mulName, mulOp, UV, Value(UV, 0.0f, 16.0f), Value(U, 0.0f, 16.0f), notUsed, 4e-3f,
                                      0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(mulVecScalar))
                      << operInfoFunc(mulName, mulOp, UV, Value(UV, 0.0f, 6e5f), Value(U, 0.0f, 6e4f), notUsed, 2e-10f,
                                      0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(mulVecScalar));

        if (isNormalOp)
            binaryOpGroup << operInfoFunc(mulName, mulOp, FV, Value(F, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed,
                                          1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(mulScalarVec))
                          << operInfoFunc(mulName, mulOp, IV, Value(I, -4.0f, 6.0f), Value(IV, -6.0f, 5.0f), notUsed,
                                          0.1f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(mulScalarVec))
                          << operInfoFunc(mulName, mulOp, IV, Value(I, -3e5f, 3e5f), Value(IV, -3e4f, 3e4f), notUsed,
                                          4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(mulScalarVec))
                          << operInfoFunc(mulName, mulOp, UV, Value(U, 0.0f, 16.0f), Value(UV, 0.0f, 16.0f), notUsed,
                                          4e-3f, 0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(mulScalarVec))
                          << operInfoFunc(mulName, mulOp, UV, Value(U, 0.0f, 6e5f), Value(UV, 0.0f, 6e4f), notUsed,
                                          2e-10f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(mulScalarVec));

        // The divide operator.

        binaryOpGroup << operInfoFunc(divName, divOp, GT, Value(GT, -1.0f, 1.0f), Value(GT, -2.0f, -0.5f), notUsed,
                                      1.0f, 0.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(div))
                      << operInfoFunc(divName, divOp, IGT, Value(IGT, 24.0f, 24.0f), Value(IGT, -4.0f, -1.0f), notUsed,
                                      0.04f, 1.0f, PRECMASK_MEDIUMP, INT_GENTYPE_FUNCS(div))
                      << operInfoFunc(divName, divOp, IGT, Value(IGT, 40320.0f, 40320.0f), Value(IGT, -8.0f, -1.0f),
                                      notUsed, 1e-5f, 0.5f, PRECMASK_HIGHP, INT_GENTYPE_FUNCS(div))
                      << operInfoFunc(divName, divOp, UGT, Value(UGT, 0.0f, 24.0f), Value(UGT, 1.0f, 4.0f), notUsed,
                                      0.04f, 0.0f, PRECMASK_MEDIUMP, UINT_GENTYPE_FUNCS(div))
                      << operInfoFunc(divName, divOp, UGT, Value(UGT, 0.0f, 40320.0f), Value(UGT, 1.0f, 8.0f), notUsed,
                                      1e-5f, 0.0f, PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(div))
                      << operInfoFunc(divName, divOp, FV, Value(FV, -1.0f, 1.0f), Value(F, -2.0f, -0.5f), notUsed, 1.0f,
                                      0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(divVecScalar))
                      << operInfoFunc(divName, divOp, IV, Value(IV, 24.0f, 24.0f), Value(I, -4.0f, -1.0f), notUsed,
                                      0.04f, 1.0f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(divVecScalar))
                      << operInfoFunc(divName, divOp, IV, Value(IV, 40320.0f, 40320.0f), Value(I, -8.0f, -1.0f),
                                      notUsed, 1e-5f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(divVecScalar))
                      << operInfoFunc(divName, divOp, UV, Value(UV, 0.0f, 24.0f), Value(U, 1.0f, 4.0f), notUsed, 0.04f,
                                      0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(divVecScalar))
                      << operInfoFunc(divName, divOp, UV, Value(UV, 0.0f, 40320.0f), Value(U, 1.0f, 8.0f), notUsed,
                                      1e-5f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(divVecScalar));

        if (isNormalOp)
            binaryOpGroup << operInfoFunc(divName, divOp, FV, Value(F, -1.0f, 1.0f), Value(FV, -2.0f, -0.5f), notUsed,
                                          1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(divScalarVec))
                          << operInfoFunc(divName, divOp, IV, Value(I, 24.0f, 24.0f), Value(IV, -4.0f, -1.0f), notUsed,
                                          0.04f, 1.0f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(divScalarVec))
                          << operInfoFunc(divName, divOp, IV, Value(I, 40320.0f, 40320.0f), Value(IV, -8.0f, -1.0f),
                                          notUsed, 1e-5f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(divScalarVec))
                          << operInfoFunc(divName, divOp, UV, Value(U, 0.0f, 24.0f), Value(UV, 1.0f, 4.0f), notUsed,
                                          0.04f, 0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(divScalarVec))
                          << operInfoFunc(divName, divOp, UV, Value(U, 0.0f, 40320.0f), Value(UV, 1.0f, 8.0f), notUsed,
                                          1e-5f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(divScalarVec));

        // The modulus operator.

        binaryOpGroup << operInfoFunc(modName, modOp, IGT, Value(IGT, 0.0f, 6.0f), Value(IGT, 1.1f, 6.1f), notUsed,
                                      0.25f, 0.5f, PRECMASK_MEDIUMP, INT_GENTYPE_FUNCS(mod))
                      << operInfoFunc(modName, modOp, IGT, Value(IGT, 0.0f, 14.0f), Value(IGT, 1.1f, 11.1f), notUsed,
                                      0.1f, 0.5f, PRECMASK_HIGHP, INT_GENTYPE_FUNCS(mod))
                      << operInfoFunc(modName, modOp, UGT, Value(UGT, 0.0f, 6.0f), Value(UGT, 1.1f, 6.1f), notUsed,
                                      0.25f, 0.0f, PRECMASK_MEDIUMP, UINT_GENTYPE_FUNCS(mod))
                      << operInfoFunc(modName, modOp, UGT, Value(UGT, 0.0f, 24.0f), Value(UGT, 1.1f, 11.1f), notUsed,
                                      0.1f, 0.0f, PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(mod))
                      << operInfoFunc(modName, modOp, IV, Value(IV, 0.0f, 6.0f), Value(I, 1.1f, 6.1f), notUsed, 0.25f,
                                      0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(modVecScalar))
                      << operInfoFunc(modName, modOp, IV, Value(IV, 0.0f, 6.0f), Value(I, 1.1f, 11.1f), notUsed, 0.1f,
                                      0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(modVecScalar))
                      << operInfoFunc(modName, modOp, UV, Value(UV, 0.0f, 6.0f), Value(U, 1.1f, 6.1f), notUsed, 0.25f,
                                      0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(modVecScalar))
                      << operInfoFunc(modName, modOp, UV, Value(UV, 0.0f, 24.0f), Value(U, 1.1f, 11.1f), notUsed, 0.1f,
                                      0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(modVecScalar));

        if (isNormalOp)
            binaryOpGroup << operInfoFunc(modName, modOp, IV, Value(I, 0.0f, 6.0f), Value(IV, 1.1f, 6.1f), notUsed,
                                          0.25f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(modScalarVec))
                          << operInfoFunc(modName, modOp, IV, Value(I, 0.0f, 6.0f), Value(IV, 1.1f, 11.1f), notUsed,
                                          0.1f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(modScalarVec))
                          << operInfoFunc(modName, modOp, UV, Value(U, 0.0f, 6.0f), Value(UV, 1.1f, 6.1f), notUsed,
                                          0.25f, 0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(modScalarVec))
                          << operInfoFunc(modName, modOp, UV, Value(U, 0.0f, 24.0f), Value(UV, 1.1f, 11.1f), notUsed,
                                          0.1f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(modScalarVec));

        // The bitwise and operator.

        binaryOpGroup << operInfoFunc(andName, andOp, IGT, Value(IGT, -16.0f, 16.0f), Value(IGT, -16.0f, 16.0f),
                                      notUsed, 0.03f, 0.5f, PRECMASK_MEDIUMP, INT_GENTYPE_FUNCS(bitwiseAnd))
                      << operInfoFunc(andName, andOp, IGT, Value(IGT, -2e9f, 2e9f), Value(IGT, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_GENTYPE_FUNCS(bitwiseAnd))
                      << operInfoFunc(andName, andOp, UGT, Value(UGT, 0.0f, 32.0f), Value(UGT, 0.0f, 32.0f), notUsed,
                                      0.03f, 0.0f, PRECMASK_MEDIUMP, UINT_GENTYPE_FUNCS(bitwiseAnd))
                      << operInfoFunc(andName, andOp, UGT, Value(UGT, 0.0f, 4e9f), Value(UGT, 0.0f, 4e9f), notUsed,
                                      2e-10f, 0.0f, PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(bitwiseAnd))
                      << operInfoFunc(andName, andOp, IV, Value(IV, -16.0f, 16.0f), Value(I, -16.0f, 16.0f), notUsed,
                                      0.03f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(bitwiseAndVecScalar))
                      << operInfoFunc(andName, andOp, IV, Value(IV, -2e9f, 2e9f), Value(I, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(bitwiseAndVecScalar))
                      << operInfoFunc(andName, andOp, UV, Value(UV, 0.0f, 32.0f), Value(U, 0.0f, 32.0f), notUsed, 0.03f,
                                      0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(bitwiseAndVecScalar))
                      << operInfoFunc(andName, andOp, UV, Value(UV, 0.0f, 4e9f), Value(U, 0.0f, 4e9f), notUsed, 2e-10f,
                                      0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(bitwiseAndVecScalar));

        if (isNormalOp)
            binaryOpGroup << operInfoFunc(andName, andOp, IV, Value(I, -16.0f, 16.0f), Value(IV, -16.0f, 16.0f),
                                          notUsed, 0.03f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(bitwiseAndScalarVec))
                          << operInfoFunc(andName, andOp, IV, Value(I, -2e9f, 2e9f), Value(IV, -2e9f, 2e9f), notUsed,
                                          4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(bitwiseAndScalarVec))
                          << operInfoFunc(andName, andOp, UV, Value(U, 0.0f, 32.0f), Value(UV, 0.0f, 32.0f), notUsed,
                                          0.03f, 0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(bitwiseAndScalarVec))
                          << operInfoFunc(andName, andOp, UV, Value(U, 0.0f, 4e9f), Value(UV, 0.0f, 4e9f), notUsed,
                                          2e-10f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(bitwiseAndScalarVec));

        // The bitwise or operator.

        binaryOpGroup << operInfoFunc(orName, orOp, IGT, Value(IGT, -16.0f, 16.0f), Value(IGT, -16.0f, 16.0f), notUsed,
                                      0.03f, 0.5f, PRECMASK_MEDIUMP, INT_GENTYPE_FUNCS(bitwiseOr))
                      << operInfoFunc(orName, orOp, IGT, Value(IGT, -2e9f, 2e9f), Value(IGT, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_GENTYPE_FUNCS(bitwiseOr))
                      << operInfoFunc(orName, orOp, UGT, Value(UGT, 0.0f, 32.0f), Value(UGT, 0.0f, 32.0f), notUsed,
                                      0.03f, 0.0f, PRECMASK_MEDIUMP, UINT_GENTYPE_FUNCS(bitwiseOr))
                      << operInfoFunc(orName, orOp, UGT, Value(UGT, 0.0f, 4e9f), Value(UGT, 0.0f, 4e9f), notUsed,
                                      2e-10f, 0.0f, PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(bitwiseOr))
                      << operInfoFunc(orName, orOp, IV, Value(IV, -16.0f, 16.0f), Value(I, -16.0f, 16.0f), notUsed,
                                      0.03f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(bitwiseOrVecScalar))
                      << operInfoFunc(orName, orOp, IV, Value(IV, -2e9f, 2e9f), Value(I, -2e9f, 2e9f), notUsed, 4e-10f,
                                      0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(bitwiseOrVecScalar))
                      << operInfoFunc(orName, orOp, UV, Value(UV, 0.0f, 32.0f), Value(U, 0.0f, 32.0f), notUsed, 0.03f,
                                      0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(bitwiseOrVecScalar))
                      << operInfoFunc(orName, orOp, UV, Value(UV, 0.0f, 4e9f), Value(U, 0.0f, 4e9f), notUsed, 2e-10f,
                                      0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(bitwiseOrVecScalar));

        if (isNormalOp)
            binaryOpGroup << operInfoFunc(orName, orOp, IV, Value(I, -16.0f, 16.0f), Value(IV, -16.0f, 16.0f), notUsed,
                                          0.03f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(bitwiseOrScalarVec))
                          << operInfoFunc(orName, orOp, IV, Value(I, -2e9f, 2e9f), Value(IV, -2e9f, 2e9f), notUsed,
                                          4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(bitwiseOrScalarVec))
                          << operInfoFunc(orName, orOp, UV, Value(U, 0.0f, 32.0f), Value(UV, 0.0f, 32.0f), notUsed,
                                          0.03f, 0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(bitwiseOrScalarVec))
                          << operInfoFunc(orName, orOp, UV, Value(U, 0.0f, 4e9f), Value(UV, 0.0f, 4e9f), notUsed,
                                          2e-10f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(bitwiseOrScalarVec));

        // The bitwise xor operator.

        binaryOpGroup << operInfoFunc(xorName, xorOp, IGT, Value(IGT, -16.0f, 16.0f), Value(IGT, -16.0f, 16.0f),
                                      notUsed, 0.03f, 0.5f, PRECMASK_MEDIUMP, INT_GENTYPE_FUNCS(bitwiseXor))
                      << operInfoFunc(xorName, xorOp, IGT, Value(IGT, -2e9f, 2e9f), Value(IGT, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_GENTYPE_FUNCS(bitwiseXor))
                      << operInfoFunc(xorName, xorOp, UGT, Value(UGT, 0.0f, 32.0f), Value(UGT, 0.0f, 32.0f), notUsed,
                                      0.03f, 0.0f, PRECMASK_MEDIUMP, UINT_GENTYPE_FUNCS(bitwiseXor))
                      << operInfoFunc(xorName, xorOp, UGT, Value(UGT, 0.0f, 4e9f), Value(UGT, 0.0f, 4e9f), notUsed,
                                      2e-10f, 0.0f, PRECMASK_HIGHP, UINT_GENTYPE_FUNCS(bitwiseXor))
                      << operInfoFunc(xorName, xorOp, IV, Value(IV, -16.0f, 16.0f), Value(I, -16.0f, 16.0f), notUsed,
                                      0.03f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(bitwiseXorVecScalar))
                      << operInfoFunc(xorName, xorOp, IV, Value(IV, -2e9f, 2e9f), Value(I, -2e9f, 2e9f), notUsed,
                                      4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(bitwiseXorVecScalar))
                      << operInfoFunc(xorName, xorOp, UV, Value(UV, 0.0f, 32.0f), Value(U, 0.0f, 32.0f), notUsed, 0.03f,
                                      0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(bitwiseXorVecScalar))
                      << operInfoFunc(xorName, xorOp, UV, Value(UV, 0.0f, 4e9f), Value(U, 0.0f, 4e9f), notUsed, 2e-10f,
                                      0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(bitwiseXorVecScalar));

        if (isNormalOp)
            binaryOpGroup << operInfoFunc(xorName, xorOp, IV, Value(I, -16.0f, 16.0f), Value(IV, -16.0f, 16.0f),
                                          notUsed, 0.03f, 0.5f, PRECMASK_MEDIUMP, INT_VEC_FUNCS(bitwiseXorScalarVec))
                          << operInfoFunc(xorName, xorOp, IV, Value(I, -2e9f, 2e9f), Value(IV, -2e9f, 2e9f), notUsed,
                                          4e-10f, 0.5f, PRECMASK_HIGHP, INT_VEC_FUNCS(bitwiseXorScalarVec))
                          << operInfoFunc(xorName, xorOp, UV, Value(U, 0.0f, 32.0f), Value(UV, 0.0f, 32.0f), notUsed,
                                          0.03f, 0.0f, PRECMASK_MEDIUMP, UINT_VEC_FUNCS(bitwiseXorScalarVec))
                          << operInfoFunc(xorName, xorOp, UV, Value(U, 0.0f, 4e9f), Value(UV, 0.0f, 4e9f), notUsed,
                                          2e-10f, 0.0f, PRECMASK_HIGHP, UINT_VEC_FUNCS(bitwiseXorScalarVec));

        // The left shift operator. Second operand (shift amount) can be either int or uint, even for uint and int first operand, respectively.

        for (int isSignedAmount = 0; isSignedAmount <= 1; isSignedAmount++)
        {
            ValueType gType = isSignedAmount == 0 ? UGT : IGT;
            ValueType sType = isSignedAmount == 0 ? U : I;
            binaryOpGroup << operInfoFunc(leftShiftName, leftShiftOp, IGT, Value(IGT, -7.0f, 7.0f),
                                          Value(gType, 0.0f, 4.0f), notUsed, 4e-3f, 0.5f, PRECMASK_MEDIUMP,
                                          INT_GENTYPE_FUNCS(leftShift))
                          << operInfoFunc(leftShiftName, leftShiftOp, IGT, Value(IGT, -7.0f, 7.0f),
                                          Value(gType, 0.0f, 27.0f), notUsed, 5e-10f, 0.5f, PRECMASK_HIGHP,
                                          INT_GENTYPE_FUNCS(leftShift))
                          << operInfoFunc(leftShiftName, leftShiftOp, UGT, Value(UGT, 0.0f, 7.0f),
                                          Value(gType, 0.0f, 5.0f), notUsed, 4e-3f, 0.0f, PRECMASK_MEDIUMP,
                                          UINT_GENTYPE_FUNCS(leftShift))
                          << operInfoFunc(leftShiftName, leftShiftOp, UGT, Value(UGT, 0.0f, 7.0f),
                                          Value(gType, 0.0f, 28.0f), notUsed, 5e-10f, 0.0f, PRECMASK_HIGHP,
                                          UINT_GENTYPE_FUNCS(leftShift))
                          << operInfoFunc(leftShiftName, leftShiftOp, IV, Value(IV, -7.0f, 7.0f),
                                          Value(sType, 0.0f, 4.0f), notUsed, 4e-3f, 0.5f, PRECMASK_MEDIUMP,
                                          INT_VEC_FUNCS(leftShiftVecScalar))
                          << operInfoFunc(leftShiftName, leftShiftOp, IV, Value(IV, -7.0f, 7.0f),
                                          Value(sType, 0.0f, 27.0f), notUsed, 5e-10f, 0.5f, PRECMASK_HIGHP,
                                          INT_VEC_FUNCS(leftShiftVecScalar))
                          << operInfoFunc(leftShiftName, leftShiftOp, UV, Value(UV, 0.0f, 7.0f),
                                          Value(sType, 0.0f, 5.0f), notUsed, 4e-3f, 0.0f, PRECMASK_MEDIUMP,
                                          UINT_VEC_FUNCS(leftShiftVecScalar))
                          << operInfoFunc(leftShiftName, leftShiftOp, UV, Value(UV, 0.0f, 7.0f),
                                          Value(sType, 0.0f, 28.0f), notUsed, 5e-10f, 0.0f, PRECMASK_HIGHP,
                                          UINT_VEC_FUNCS(leftShiftVecScalar));
        }

        // The right shift operator. Second operand (shift amount) can be either int or uint, even for uint and int first operand, respectively.

        for (int isSignedAmount = 0; isSignedAmount <= 1; isSignedAmount++)
        {
            ValueType gType = isSignedAmount == 0 ? UGT : IGT;
            ValueType sType = isSignedAmount == 0 ? U : I;
            binaryOpGroup << operInfoFunc(rightShiftName, rightShiftOp, IGT, Value(IGT, -127.0f, 127.0f),
                                          Value(gType, 0.0f, 8.0f), notUsed, 4e-3f, 0.5f, PRECMASK_MEDIUMP,
                                          INT_GENTYPE_FUNCS(rightShift))
                          << operInfoFunc(rightShiftName, rightShiftOp, IGT, Value(IGT, -2e9f, 2e9f),
                                          Value(gType, 0.0f, 31.0f), notUsed, 5e-10f, 0.5f, PRECMASK_HIGHP,
                                          INT_GENTYPE_FUNCS(rightShift))
                          << operInfoFunc(rightShiftName, rightShiftOp, UGT, Value(UGT, 0.0f, 255.0f),
                                          Value(gType, 0.0f, 8.0f), notUsed, 4e-3f, 0.0f, PRECMASK_MEDIUMP,
                                          UINT_GENTYPE_FUNCS(rightShift))
                          << operInfoFunc(rightShiftName, rightShiftOp, UGT, Value(UGT, 0.0f, 4e9f),
                                          Value(gType, 0.0f, 31.0f), notUsed, 5e-10f, 0.0f, PRECMASK_HIGHP,
                                          UINT_GENTYPE_FUNCS(rightShift))
                          << operInfoFunc(rightShiftName, rightShiftOp, IV, Value(IV, -127.0f, 127.0f),
                                          Value(sType, 0.0f, 8.0f), notUsed, 4e-3f, 0.5f, PRECMASK_MEDIUMP,
                                          INT_VEC_FUNCS(rightShiftVecScalar))
                          << operInfoFunc(rightShiftName, rightShiftOp, IV, Value(IV, -2e9f, 2e9f),
                                          Value(sType, 0.0f, 31.0f), notUsed, 5e-10f, 0.5f, PRECMASK_HIGHP,
                                          INT_VEC_FUNCS(rightShiftVecScalar))
                          << operInfoFunc(rightShiftName, rightShiftOp, UV, Value(UV, 0.0f, 255.0f),
                                          Value(sType, 0.0f, 8.0f), notUsed, 4e-3f, 0.0f, PRECMASK_MEDIUMP,
                                          UINT_VEC_FUNCS(rightShiftVecScalar))
                          << operInfoFunc(rightShiftName, rightShiftOp, UV, Value(UV, 0.0f, 4e9f),
                                          Value(sType, 0.0f, 31.0f), notUsed, 5e-10f, 0.0f, PRECMASK_HIGHP,
                                          UINT_VEC_FUNCS(rightShiftVecScalar));
        }
    }

    // Rest of binary operators.

    binaryOpGroup
        // Scalar relational operators.
        << BuiltinOperInfo("less", "<", B, Value(F, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThan_float, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("less", "<", B, Value(I, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThan_int, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("less", "<", B, Value(U, 0.0f, 16.0f), Value(U, 0.0f, 16.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThan_uint, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("less_or_equal", "<=", B, Value(F, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThanEqual_float, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("less_or_equal", "<=", B, Value(I, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThanEqual_int, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("less_or_equal", "<=", B, Value(U, 0.0f, 16.0f), Value(U, 0.0f, 16.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThanEqual_uint, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("greater", ">", B, Value(F, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_greaterThan_float, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("greater", ">", B, Value(I, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_greaterThan_int, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("greater", ">", B, Value(U, 0.0f, 16.0f), Value(U, 0.0f, 16.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_greaterThan_uint, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("greater_or_equal", ">=", B, Value(F, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, eval_greaterThanEqual_float, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("greater_or_equal", ">=", B, Value(I, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, eval_greaterThanEqual_int, DE_NULL, DE_NULL, DE_NULL)
        << BuiltinOperInfo("greater_or_equal", ">=", B, Value(U, 0.0f, 16.0f), Value(U, 0.0f, 16.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, eval_greaterThanEqual_uint, DE_NULL, DE_NULL, DE_NULL)

        // Equality comparison operators.
        << BuiltinOperInfo("equal", "==", B, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(allEqual))
        << BuiltinOperInfo("equal", "==", B, Value(IGT, -5.5f, 4.7f), Value(IGT, -2.1f, 0.1f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(allEqual))
        << BuiltinOperInfo("equal", "==", B, Value(UGT, 0.0f, 8.0f), Value(UGT, 3.5f, 4.5f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, UINT_GENTYPE_FUNCS(allEqual))
        << BuiltinOperInfo("equal", "==", B, Value(BGT, -2.1f, 2.1f), Value(BGT, -1.1f, 3.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_GENTYPE_FUNCS(allEqual))
        << BuiltinOperInfo("not_equal", "!=", B, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(anyNotEqual))
        << BuiltinOperInfo("not_equal", "!=", B, Value(IGT, -5.5f, 4.7f), Value(IGT, -2.1f, 0.1f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(anyNotEqual))
        << BuiltinOperInfo("not_equal", "!=", B, Value(UGT, 0.0f, 8.0f), Value(UGT, 3.5f, 4.5f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, UINT_GENTYPE_FUNCS(anyNotEqual))
        << BuiltinOperInfo("not_equal", "!=", B, Value(BGT, -2.1f, 2.1f), Value(BGT, -1.1f, 3.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_GENTYPE_FUNCS(anyNotEqual))

        // Logical operators.
        << BuiltinOperInfo("logical_and", "&&", B, Value(B, -1.0f, 1.0f), Value(B, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_FUNCS(logicalAnd))
        << BuiltinOperInfo("logical_or", "||", B, Value(B, -1.0f, 1.0f), Value(B, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_FUNCS(logicalOr))
        << BuiltinOperInfo("logical_xor", "^^", B, Value(B, -1.0f, 1.0f), Value(B, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_FUNCS(logicalXor));

    funcInfoGroups.push_back(binaryOpGroup);

    // Common Functions.
    funcInfoGroups.push_back(
        BuiltinFuncGroup("common_functions", "Common function tests.")
        << BuiltinFuncInfo("min", "min", IGT, Value(IGT, -4.0f, 4.0f), Value(IGT, -4.0f, 4.0f), notUsed, 0.125f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(min))
        << BuiltinFuncInfo("min", "min", IGT, Value(IV, -4.0f, 4.0f), Value(I, -4.0f, 4.0f), notUsed, 0.125f, 0.5f,
                           PRECMASK_ALL, INT_VEC_FUNCS(minVecScalar))
        << BuiltinFuncInfo("min", "min", UGT, Value(UGT, 0.0f, 8.0f), Value(UGT, 0.0f, 8.0f), notUsed, 0.125f, 0.0f,
                           PRECMASK_ALL, UINT_GENTYPE_FUNCS(min))
        << BuiltinFuncInfo("min", "min", UGT, Value(UV, 0.0f, 8.0f), Value(U, 0.0f, 8.0f), notUsed, 0.125f, 0.0f,
                           PRECMASK_ALL, UINT_VEC_FUNCS(minVecScalar))
        << BuiltinFuncInfo("max", "max", IGT, Value(IGT, -4.0f, 4.0f), Value(IGT, -4.0f, 4.0f), notUsed, 0.125f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(max))
        << BuiltinFuncInfo("max", "max", IGT, Value(IV, -4.0f, 4.0f), Value(I, -4.0f, 4.0f), notUsed, 0.125f, 0.5f,
                           PRECMASK_ALL, INT_VEC_FUNCS(maxVecScalar))
        << BuiltinFuncInfo("max", "max", UGT, Value(UGT, 0.0f, 8.0f), Value(UGT, 0.0f, 8.0f), notUsed, 0.125f, 0.0f,
                           PRECMASK_ALL, UINT_GENTYPE_FUNCS(max))
        << BuiltinFuncInfo("max", "max", UGT, Value(UV, 0.0f, 8.0f), Value(U, 0.0f, 8.0f), notUsed, 0.125f, 0.0f,
                           PRECMASK_ALL, UINT_VEC_FUNCS(maxVecScalar))
        << BuiltinFuncInfo("clamp", "clamp", IGT, Value(IGT, -4.0f, 4.0f), Value(IGT, -2.0f, 2.0f),
                           Value(IGT, 2.0f, 4.0f), 0.125f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(clamp))
        << BuiltinFuncInfo("clamp", "clamp", IGT, Value(IV, -4.0f, 4.0f), Value(I, -2.0f, 2.0f), Value(I, 2.0f, 4.0f),
                           0.125f, 0.5f, PRECMASK_ALL, INT_VEC_FUNCS(clampVecScalarScalar))
        << BuiltinFuncInfo("clamp", "clamp", UGT, Value(UGT, 0.0f, 8.0f), Value(UGT, 2.0f, 6.0f),
                           Value(UGT, 6.0f, 8.0f), 0.125f, 0.0f, PRECMASK_ALL, UINT_GENTYPE_FUNCS(clamp))
        << BuiltinFuncInfo("clamp", "clamp", UGT, Value(UV, 0.0f, 8.0f), Value(U, 2.0f, 6.0f), Value(U, 6.0f, 8.0f),
                           0.125f, 0.0f, PRECMASK_ALL, UINT_VEC_FUNCS(clampVecScalarScalar)));

    // Vector Relational Functions.
    funcInfoGroups.push_back(
        BuiltinFuncGroup("float_compare", "Floating point comparison tests.")
        << BuiltinFuncInfo("lessThan", "lessThan", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(lessThan))
        << BuiltinFuncInfo("lessThanEqual", "lessThanEqual", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f),
                           notUsed, 1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(lessThanEqual))
        << BuiltinFuncInfo("greaterThan", "greaterThan", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed,
                           1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(greaterThan))
        << BuiltinFuncInfo("greaterThanEqual", "greaterThanEqual", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f),
                           notUsed, 1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(greaterThanEqual))
        << BuiltinFuncInfo("equal", "equal", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(equal))
        << BuiltinFuncInfo("notEqual", "notEqual", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(notEqual)));

    funcInfoGroups.push_back(
        BuiltinFuncGroup("int_compare", "Integer comparison tests.")
        << BuiltinFuncInfo("lessThan", "lessThan", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, INT_VEC_FUNCS(lessThan))
        << BuiltinFuncInfo("lessThanEqual", "lessThanEqual", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f),
                           notUsed, 1.0f, 0.0f, PRECMASK_ALL, INT_VEC_FUNCS(lessThanEqual))
        << BuiltinFuncInfo("greaterThan", "greaterThan", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f), notUsed,
                           1.0f, 0.0f, PRECMASK_ALL, INT_VEC_FUNCS(greaterThan))
        << BuiltinFuncInfo("greaterThanEqual", "greaterThanEqual", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f),
                           notUsed, 1.0f, 0.0f, PRECMASK_ALL, INT_VEC_FUNCS(greaterThanEqual))
        << BuiltinFuncInfo("equal", "equal", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, INT_VEC_FUNCS(equal))
        << BuiltinFuncInfo("notEqual", "notEqual", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, INT_VEC_FUNCS(notEqual)));

    funcInfoGroups.push_back(BuiltinFuncGroup("bool_compare", "Boolean comparison tests.")
                             << BuiltinFuncInfo("equal", "equal", BV, Value(BV, -5.2f, 4.9f), Value(BV, -5.0f, 5.0f),
                                                notUsed, 1.0f, 0.0f, PRECMASK_NA, BOOL_VEC_FUNCS(equal))
                             << BuiltinFuncInfo("notEqual", "notEqual", BV, Value(BV, -5.2f, 4.9f),
                                                Value(BV, -5.0f, 5.0f), notUsed, 1.0f, 0.0f, PRECMASK_NA,
                                                BOOL_VEC_FUNCS(notEqual))
                             << BuiltinFuncInfo("any", "any", B, Value(BV, -1.0f, 0.3f), notUsed, notUsed, 1.0f, 0.0f,
                                                PRECMASK_NA, BOOL_VEC_FUNCS(any))
                             << BuiltinFuncInfo("all", "all", B, Value(BV, -0.3f, 1.0f), notUsed, notUsed, 1.0f, 0.0f,
                                                PRECMASK_NA, BOOL_VEC_FUNCS(all))
                             << BuiltinFuncInfo("not", "not", BV, Value(BV, -1.0f, 1.0f), notUsed, notUsed, 1.0f, 0.0f,
                                                PRECMASK_NA, BOOL_VEC_FUNCS(boolNot)));

    static const ShaderType s_shaderTypes[] = {SHADERTYPE_VERTEX, SHADERTYPE_FRAGMENT};

    static const DataType s_floatTypes[] = {TYPE_FLOAT, TYPE_FLOAT_VEC2, TYPE_FLOAT_VEC3, TYPE_FLOAT_VEC4};

    static const DataType s_intTypes[] = {TYPE_INT, TYPE_INT_VEC2, TYPE_INT_VEC3, TYPE_INT_VEC4};

    static const DataType s_uintTypes[] = {TYPE_UINT, TYPE_UINT_VEC2, TYPE_UINT_VEC3, TYPE_UINT_VEC4};

    static const DataType s_boolTypes[] = {TYPE_BOOL, TYPE_BOOL_VEC2, TYPE_BOOL_VEC3, TYPE_BOOL_VEC4};

    for (int outerGroupNdx = 0; outerGroupNdx < (int)funcInfoGroups.size(); outerGroupNdx++)
    {
        // Create outer group.
        const BuiltinFuncGroup &outerGroupInfo = funcInfoGroups[outerGroupNdx];
        TestCaseGroup *outerGroup              = new TestCaseGroup(m_testCtx, outerGroupInfo.name);
        addChild(outerGroup);

        // Only create new group if name differs from previous one.
        TestCaseGroup *innerGroup = DE_NULL;

        for (int funcInfoNdx = 0; funcInfoNdx < (int)outerGroupInfo.funcInfos.size(); funcInfoNdx++)
        {
            const BuiltinFuncInfo &funcInfo = outerGroupInfo.funcInfos[funcInfoNdx];
            const char *shaderFuncName      = funcInfo.shaderFuncName;
            const bool isBoolCase           = (funcInfo.precisionMask == PRECMASK_NA);
            const bool isBoolOut  = (funcInfo.outValue & (VALUE_BOOL | VALUE_BOOL_VEC | VALUE_BOOL_GENTYPE)) != 0;
            const bool isIntOut   = (funcInfo.outValue & (VALUE_INT | VALUE_INT_VEC | VALUE_INT_GENTYPE)) != 0;
            const bool isUintOut  = (funcInfo.outValue & (VALUE_UINT | VALUE_UINT_VEC | VALUE_UINT_GENTYPE)) != 0;
            const bool isFloatOut = !isBoolOut && !isIntOut && !isUintOut;

            if (!innerGroup || (std::string(innerGroup->getName()) != funcInfo.caseName))
            {
                std::string groupDesc = std::string("Built-in function ") + shaderFuncName + "() tests.";
                innerGroup            = new TestCaseGroup(m_testCtx, funcInfo.caseName);
                outerGroup->addChild(innerGroup);
            }

            for (int inScalarSize = 1; inScalarSize <= 4; inScalarSize++)
            {
                const int outScalarSize =
                    ((funcInfo.outValue == VALUE_FLOAT) || (funcInfo.outValue == VALUE_BOOL)) ? 1 : inScalarSize;
                const DataType outDataType = isFloatOut ? s_floatTypes[outScalarSize - 1] :
                                             isIntOut   ? s_intTypes[outScalarSize - 1] :
                                             isUintOut  ? s_uintTypes[outScalarSize - 1] :
                                             isBoolOut  ? s_boolTypes[outScalarSize - 1] :
                                                          TYPE_LAST;

                ShaderEvalFunc evalFunc = DE_NULL;
                switch (inScalarSize)
                {
                case 1:
                    evalFunc = funcInfo.evalFuncScalar;
                    break;
                case 2:
                    evalFunc = funcInfo.evalFuncVec2;
                    break;
                case 3:
                    evalFunc = funcInfo.evalFuncVec3;
                    break;
                case 4:
                    evalFunc = funcInfo.evalFuncVec4;
                    break;
                default:
                    DE_ASSERT(false);
                }

                // Skip if no valid eval func.
                if (evalFunc == DE_NULL)
                    continue;

                for (int precision = PRECISION_MEDIUMP; precision < PRECISION_LAST; precision++)
                {
                    if ((funcInfo.precisionMask & (1 << precision)) ||
                        (funcInfo.precisionMask == PRECMASK_NA &&
                         precision == PRECISION_MEDIUMP)) // use mediump interpolators for booleans
                    {
                        const char *precisionStr          = getPrecisionName((Precision)precision);
                        const std::string precisionPrefix = isBoolCase ? "" : (std::string(precisionStr) + "_");

                        for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
                        {
                            const ShaderType shaderType = s_shaderTypes[shaderTypeNdx];
                            const char *shaderTypeName  = getShaderTypeName(shaderType);
                            const bool isVertexCase     = (ShaderType)shaderType == SHADERTYPE_VERTEX;
                            const bool isUnaryOp        = (funcInfo.input1.valueType == VALUE_NONE);

                            // \note Data type names will be added to name in a following loop.
                            std::string name = precisionPrefix;

                            // Generate shader op.
                            std::string shaderOp = std::string("res = ");

                            // Setup shader data info.
                            ShaderDataSpec shaderSpec;
                            shaderSpec.numInputs      = 0;
                            shaderSpec.precision      = isBoolCase ? PRECISION_LAST : (Precision)precision;
                            shaderSpec.output         = outDataType;
                            shaderSpec.resultScale    = funcInfo.resultScale;
                            shaderSpec.resultBias     = funcInfo.resultBias;
                            shaderSpec.referenceScale = funcInfo.referenceScale;
                            shaderSpec.referenceBias  = funcInfo.referenceBias;

                            if (funcInfo.type == OPERATOR)
                            {
                                if (isUnaryOp && funcInfo.isUnaryPrefix)
                                    shaderOp += shaderFuncName;
                            }
                            else if (funcInfo.type == FUNCTION)
                                shaderOp += std::string(shaderFuncName) + "(";
                            else // SIDE_EFFECT_OPERATOR
                                shaderOp += "in0;\n\t";

                            for (int inputNdx = 0; inputNdx < MAX_INPUTS; inputNdx++)
                            {
                                const Value &prevV = (inputNdx == 1) ? funcInfo.input0 :
                                                     (inputNdx == 2) ? funcInfo.input1 :
                                                                       funcInfo.input2;
                                const Value &v     = (inputNdx == 0) ? funcInfo.input0 :
                                                     (inputNdx == 1) ? funcInfo.input1 :
                                                                       funcInfo.input2;

                                if (v.valueType == VALUE_NONE)
                                    continue; // Skip unused input.

                                const int prevInScalarSize = isScalarType(prevV.valueType) ? 1 : inScalarSize;
                                const DataType prevInDataType =
                                    isFloatType(prevV.valueType) ? s_floatTypes[prevInScalarSize - 1] :
                                    isIntType(prevV.valueType)   ? s_intTypes[prevInScalarSize - 1] :
                                    isUintType(prevV.valueType)  ? s_uintTypes[prevInScalarSize - 1] :
                                    isBoolType(prevV.valueType)  ? s_boolTypes[prevInScalarSize - 1] :
                                                                   TYPE_LAST;

                                const int curInScalarSize = isScalarType(v.valueType) ? 1 : inScalarSize;
                                const DataType curInDataType =
                                    isFloatType(v.valueType) ? s_floatTypes[curInScalarSize - 1] :
                                    isIntType(v.valueType)   ? s_intTypes[curInScalarSize - 1] :
                                    isUintType(v.valueType)  ? s_uintTypes[curInScalarSize - 1] :
                                    isBoolType(v.valueType)  ? s_boolTypes[curInScalarSize - 1] :
                                                               TYPE_LAST;

                                // Write input type(s) to name.
                                if (inputNdx == 0 ||
                                    prevInDataType !=
                                        curInDataType) // \note Only write input type to case name if different from previous input type (avoid overly long names).
                                    name += std::string("") + getDataTypeName(curInDataType) + "_";

                                // Generate op input source.
                                if (funcInfo.type == OPERATOR || funcInfo.type == FUNCTION)
                                {
                                    if (inputNdx != 0)
                                    {
                                        if (funcInfo.type == OPERATOR && !isUnaryOp)
                                            shaderOp += " " + std::string(shaderFuncName) + " ";
                                        else
                                            shaderOp += ", ";
                                    }

                                    shaderOp += "in" + de::toString(inputNdx);

                                    if (funcInfo.type == OPERATOR && isUnaryOp && !funcInfo.isUnaryPrefix)
                                        shaderOp += std::string(shaderFuncName);
                                }
                                else
                                {
                                    DE_ASSERT(funcInfo.type == SIDE_EFFECT_OPERATOR);

                                    if (inputNdx != 0 || (isUnaryOp && funcInfo.isUnaryPrefix))
                                        shaderOp += std::string("") + (isUnaryOp ? "" : " ") + shaderFuncName +
                                                    (isUnaryOp ? "" : " ");

                                    shaderOp +=
                                        inputNdx == 0 ?
                                            "res" :
                                            "in" +
                                                de::toString(
                                                    inputNdx); // \note in0 has already been assigned to res, so start from in1.

                                    if (isUnaryOp && !funcInfo.isUnaryPrefix)
                                        shaderOp += shaderFuncName;
                                }

                                // Fill in shader info.
                                shaderSpec.inputs[shaderSpec.numInputs++] =
                                    ShaderValue(curInDataType, v.rangeMin, v.rangeMax);
                            }

                            if (funcInfo.type == FUNCTION)
                                shaderOp += ")";

                            shaderOp += ";";

                            name += shaderTypeName;

                            // Create the test case.
                            innerGroup->addChild(new ShaderOperatorCase(m_testCtx, name.c_str(), isVertexCase, evalFunc,
                                                                        shaderOp, shaderSpec));
                        }
                    }
                }
            }
        }
    }

    // The ?: selection operator.

    static const struct
    {
        const DataType type; // The type of "Y" and "Z" operands in "X ? Y : Z" (X is always bool).
        const ShaderEvalFunc evalFunc;
    } s_selectionInfo[] = {{TYPE_FLOAT, eval_selection_float},     {TYPE_FLOAT_VEC2, eval_selection_vec2},
                           {TYPE_FLOAT_VEC3, eval_selection_vec3}, {TYPE_FLOAT_VEC4, eval_selection_vec4},
                           {TYPE_INT, eval_selection_int},         {TYPE_INT_VEC2, eval_selection_ivec2},
                           {TYPE_INT_VEC3, eval_selection_ivec3},  {TYPE_INT_VEC4, eval_selection_ivec4},
                           {TYPE_UINT, eval_selection_uint},       {TYPE_UINT_VEC2, eval_selection_uvec2},
                           {TYPE_UINT_VEC3, eval_selection_uvec3}, {TYPE_UINT_VEC4, eval_selection_uvec4},
                           {TYPE_BOOL, eval_selection_bool},       {TYPE_BOOL_VEC2, eval_selection_bvec2},
                           {TYPE_BOOL_VEC3, eval_selection_bvec3}, {TYPE_BOOL_VEC4, eval_selection_bvec4}};

    TestCaseGroup *selectionGroup = new TestCaseGroup(m_testCtx, "selection");
    addChild(selectionGroup);

    for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(s_selectionInfo); typeNdx++)
    {
        const DataType curType        = s_selectionInfo[typeNdx].type;
        const ShaderEvalFunc evalFunc = s_selectionInfo[typeNdx].evalFunc;
        const bool isBoolCase         = isDataTypeBoolOrBVec(curType);
        const bool isFloatCase        = isDataTypeFloatOrVec(curType);
        const bool isIntCase          = isDataTypeIntOrIVec(curType);
        const bool isUintCase         = isDataTypeUintOrUVec(curType);
        const char *dataTypeStr       = getDataTypeName(curType);

        DE_ASSERT(isBoolCase || isFloatCase || isIntCase || isUintCase);
        DE_UNREF(isIntCase);

        for (int precision = (int)PRECISION_MEDIUMP; precision < (int)PRECISION_LAST; precision++)
        {
            if (isBoolCase && precision != PRECISION_MEDIUMP) // Use mediump interpolators for booleans.
                continue;

            const char *precisionStr    = getPrecisionName((Precision)precision);
            std::string precisionPrefix = isBoolCase ? "" : (std::string(precisionStr) + "_");

            for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
            {
                const ShaderType shaderType = s_shaderTypes[shaderTypeNdx];
                const char *shaderTypeName  = getShaderTypeName(shaderType);
                const bool isVertexCase     = (ShaderType)shaderType == SHADERTYPE_VERTEX;

                std::string name = precisionPrefix + dataTypeStr + "_" + shaderTypeName;

                ShaderDataSpec shaderSpec;
                shaderSpec.numInputs      = 3;
                shaderSpec.precision      = isBoolCase ? PRECISION_LAST : (Precision)precision;
                shaderSpec.output         = curType;
                shaderSpec.resultScale    = (isBoolCase ? 1.0f : (isFloatCase ? 0.5f : (isUintCase ? 0.5f : 0.1f)));
                shaderSpec.resultBias     = (isBoolCase ? 0.0f : (isFloatCase ? 0.5f : (isUintCase ? 0.0f : 0.5f)));
                shaderSpec.referenceScale = shaderSpec.resultScale;
                shaderSpec.referenceBias  = shaderSpec.resultBias;

                const float rangeMin = (isBoolCase ? -1.0f : (isFloatCase ? -1.0f : (isUintCase ? 0.0f : -5.0f)));
                const float rangeMax = (isBoolCase ? 1.0f : (isFloatCase ? 1.0f : (isUintCase ? 2.0f : 5.0f)));

                shaderSpec.inputs[0] = ShaderValue(TYPE_BOOL, -1.0f, 1.0f);
                shaderSpec.inputs[1] = ShaderValue(curType, rangeMin, rangeMax);
                shaderSpec.inputs[2] = ShaderValue(curType, rangeMin, rangeMax);

                selectionGroup->addChild(new ShaderOperatorCase(m_testCtx, name.c_str(), isVertexCase, evalFunc,
                                                                "res = in0 ? in1 : in2;", shaderSpec));
            }
        }
    }

    // The sequence operator (comma).

    TestCaseGroup *sequenceGroup = new TestCaseGroup(m_testCtx, "sequence");
    addChild(sequenceGroup);

    TestCaseGroup *sequenceNoSideEffGroup = new TestCaseGroup(m_testCtx, "no_side_effects");
    TestCaseGroup *sequenceSideEffGroup   = new TestCaseGroup(m_testCtx, "side_effects");
    sequenceGroup->addChild(sequenceNoSideEffGroup);
    sequenceGroup->addChild(sequenceSideEffGroup);

    static const struct
    {
        const bool containsSideEffects;
        const char *caseName;
        const char *expressionStr;
        const int numInputs;
        const DataType inputTypes[MAX_INPUTS];
        const DataType resultType;
        const ShaderEvalFunc evalFunc;
    } s_sequenceCases[] = {{false,
                            "vec4",
                            "in0, in2 + in1, in1 + in0",
                            3,
                            {TYPE_FLOAT_VEC4, TYPE_FLOAT_VEC4, TYPE_FLOAT_VEC4},
                            TYPE_FLOAT_VEC4,
                            evalSequenceNoSideEffCase0},
                           {false,
                            "float_uint",
                            "in0 + in2, in1 + in1",
                            3,
                            {TYPE_FLOAT, TYPE_UINT, TYPE_FLOAT},
                            TYPE_UINT,
                            evalSequenceNoSideEffCase1},
                           {false,
                            "bool_vec2",
                            "in0 && in1, in0, ivec2(vec2(in0) + in2)",
                            3,
                            {TYPE_BOOL, TYPE_BOOL, TYPE_FLOAT_VEC2},
                            TYPE_INT_VEC2,
                            evalSequenceNoSideEffCase2},
                           {false,
                            "vec4_ivec4_bvec4",
                            "in0 + vec4(in1), in2, in1",
                            3,
                            {TYPE_FLOAT_VEC4, TYPE_INT_VEC4, TYPE_BOOL_VEC4},
                            TYPE_INT_VEC4,
                            evalSequenceNoSideEffCase3},

                           {true,
                            "vec4",
                            "in0++, in1 = in0 + in2, in2 = in1",
                            3,
                            {TYPE_FLOAT_VEC4, TYPE_FLOAT_VEC4, TYPE_FLOAT_VEC4},
                            TYPE_FLOAT_VEC4,
                            evalSequenceSideEffCase0},
                           {true,
                            "float_uint",
                            "in1++, in0 = float(in1), in1 = uint(in0 + in2)",
                            3,
                            {TYPE_FLOAT, TYPE_UINT, TYPE_FLOAT},
                            TYPE_UINT,
                            evalSequenceSideEffCase1},
                           {true,
                            "bool_vec2",
                            "in1 = in0, in2++, in2 = in2 + vec2(in1), ivec2(in2)",
                            3,
                            {TYPE_BOOL, TYPE_BOOL, TYPE_FLOAT_VEC2},
                            TYPE_INT_VEC2,
                            evalSequenceSideEffCase2},
                           {true,
                            "vec4_ivec4_bvec4",
                            "in0 = in0 + vec4(in2), in1 = in1 + ivec4(in0), in1++",
                            3,
                            {TYPE_FLOAT_VEC4, TYPE_INT_VEC4, TYPE_BOOL_VEC4},
                            TYPE_INT_VEC4,
                            evalSequenceSideEffCase3}};

    for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(s_sequenceCases); caseNdx++)
    {
        for (int precision = (int)PRECISION_MEDIUMP; precision < (int)PRECISION_LAST; precision++)
        {
            for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
            {
                const ShaderType shaderType = s_shaderTypes[shaderTypeNdx];
                const char *shaderTypeName  = getShaderTypeName(shaderType);
                const bool isVertexCase     = (ShaderType)shaderType == SHADERTYPE_VERTEX;

                std::string name = std::string("") + getPrecisionName((Precision)precision) + "_" +
                                   s_sequenceCases[caseNdx].caseName + "_" + shaderTypeName;

                ShaderDataSpec shaderSpec;
                shaderSpec.numInputs      = s_sequenceCases[caseNdx].numInputs;
                shaderSpec.precision      = (Precision)precision;
                shaderSpec.output         = s_sequenceCases[caseNdx].resultType;
                shaderSpec.resultScale    = 0.5f;
                shaderSpec.resultBias     = 0.0f;
                shaderSpec.referenceScale = shaderSpec.resultScale;
                shaderSpec.referenceBias  = shaderSpec.resultBias;

                for (int inputNdx = 0; inputNdx < s_sequenceCases[caseNdx].numInputs; inputNdx++)
                {
                    const DataType type = s_sequenceCases[caseNdx].inputTypes[inputNdx];
                    const float rangeMin =
                        (isDataTypeFloatOrVec(type) ?
                             -0.5f :
                             (isDataTypeIntOrIVec(type) ? -2.0f : (isDataTypeUintOrUVec(type) ? 0.0f : -1.0f)));
                    const float rangeMax =
                        (isDataTypeFloatOrVec(type) ?
                             0.5f :
                             (isDataTypeIntOrIVec(type) ? 2.0f : (isDataTypeUintOrUVec(type) ? 2.0f : 1.0f)));

                    shaderSpec.inputs[inputNdx] = ShaderValue(type, rangeMin, rangeMax);
                }

                const std::string expression =
                    std::string("") + "res = (" + s_sequenceCases[caseNdx].expressionStr + ");";

                TestCaseGroup *group =
                    s_sequenceCases[caseNdx].containsSideEffects ? sequenceSideEffGroup : sequenceNoSideEffGroup;
                group->addChild(new ShaderOperatorCase(m_testCtx, name.c_str(), isVertexCase,
                                                       s_sequenceCases[caseNdx].evalFunc, expression.c_str(),
                                                       shaderSpec));
            }
        }
    }
}

} // namespace

tcu::TestCaseGroup *createOperatorTests(tcu::TestContext &testCtx)
{
    return new ShaderOperatorTests(testCtx);
}

} // namespace sr
} // namespace vkt
