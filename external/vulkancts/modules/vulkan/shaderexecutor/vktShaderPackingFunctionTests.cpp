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
 * \brief Floating-point packing and unpacking function tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderPackingFunctionTests.hpp"
#include "vktShaderExecutor.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuVectorUtil.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"
#include "deSharedPtr.hpp"

namespace vkt
{
namespace shaderexecutor
{

using namespace shaderexecutor;

using std::string;
using tcu::TestLog;

namespace
{

inline uint32_t getUlpDiff(float a, float b)
{
    const uint32_t aBits = tcu::Float32(a).bits();
    const uint32_t bBits = tcu::Float32(b).bits();
    return aBits > bBits ? aBits - bBits : bBits - aBits;
}

struct HexFloat
{
    const float value;
    HexFloat(const float value_) : value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const HexFloat &v)
{
    return str << v.value << " / " << tcu::toHex(tcu::Float32(v.value).bits());
}

} // namespace

// ShaderPackingFunctionCase

class ShaderPackingFunctionCase : public TestCase
{
public:
    ShaderPackingFunctionCase(tcu::TestContext &testCtx, const char *name, glu::ShaderType shaderType);
    ~ShaderPackingFunctionCase(void);

    void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const
    {
        generateSources(m_shaderType, m_spec, programCollection);
    }

protected:
    const glu::ShaderType m_shaderType;
    ShaderSpec m_spec;

private:
    ShaderPackingFunctionCase(const ShaderPackingFunctionCase &other);
    ShaderPackingFunctionCase &operator=(const ShaderPackingFunctionCase &other);
};

ShaderPackingFunctionCase::ShaderPackingFunctionCase(tcu::TestContext &testCtx, const char *name,
                                                     glu::ShaderType shaderType)
    : TestCase(testCtx, name)
    , m_shaderType(shaderType)
{
}

ShaderPackingFunctionCase::~ShaderPackingFunctionCase(void)
{
}

void ShaderPackingFunctionCase::checkSupport(Context &context) const
{
    checkSupportShader(context, m_shaderType);
}

// ShaderPackingFunctionTestInstance

class ShaderPackingFunctionTestInstance : public TestInstance
{
public:
    ShaderPackingFunctionTestInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec,
                                      const char *name)
        : TestInstance(context)
        , m_testCtx(context.getTestContext())
        , m_shaderType(shaderType)
        , m_spec(spec)
        , m_name(name)
        , m_executor(createExecutor(context, m_shaderType, m_spec))
    {
    }
    virtual tcu::TestStatus iterate(void) = 0;

protected:
    tcu::TestContext &m_testCtx;
    const glu::ShaderType m_shaderType;
    ShaderSpec m_spec;
    const char *m_name;
    de::UniquePtr<ShaderExecutor> m_executor;
};

// Test cases

class PackSnorm2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    PackSnorm2x16CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec,
                              glu::Precision precision, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
        , m_precision(precision)
    {
    }

    tcu::TestStatus iterate(void)
    {
        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<tcu::Vec2> inputs;
        std::vector<uint32_t> outputs;
        const int                    maxDiff = m_precision == glu::PRECISION_HIGHP    ? 1        : // Rounding only.
                                                  m_precision == glu::PRECISION_MEDIUMP    ? 33    : // (2^-10) * (2^15) + 1
                                                  m_precision == glu::PRECISION_LOWP    ? 129    : 0;    // (2^-8) * (2^15) + 1

        // Special values to check.
        inputs.push_back(tcu::Vec2(0.0f, 0.0f));
        inputs.push_back(tcu::Vec2(-1.0f, 1.0f));
        inputs.push_back(tcu::Vec2(0.5f, -0.5f));
        inputs.push_back(tcu::Vec2(-1.5f, 1.5f));
        inputs.push_back(tcu::Vec2(0.25f, -0.75f));

        // Random values, mostly in range.
        for (int ndx = 0; ndx < 15; ndx++)
        {
            inputs.push_back(tcu::randomVector<float, 2>(rnd, tcu::Vec2(-1.25f), tcu::Vec2(1.25f)));
        }

        // Large random values.
        for (int ndx = 0; ndx < 80; ndx++)
        {
            inputs.push_back(tcu::randomVector<float, 2>(rnd, tcu::Vec2(-0.5e6f), tcu::Vec2(0.5e6f)));
        }

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < numValues; valNdx++)
            {
                const uint16_t ref0 =
                    (uint16_t)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].x(), -1.0f, 1.0f) * 32767.0f),
                                        -(1 << 15), (1 << 15) - 1);
                const uint16_t ref1 =
                    (uint16_t)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].y(), -1.0f, 1.0f) * 32767.0f),
                                        -(1 << 15), (1 << 15) - 1);
                const uint32_t ref  = (ref1 << 16) | ref0;
                const uint32_t res  = outputs[valNdx];
                const uint16_t res0 = (uint16_t)(res & 0xffff);
                const uint16_t res1 = (uint16_t)(res >> 16);
                const int diff0     = de::abs((int)ref0 - (int)res0);
                const int diff1     = de::abs((int)ref1 - (int)res1);

                if (diff0 > maxDiff || diff1 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx
                                           << ", expected packSnorm2x16(" << inputs[valNdx] << ") = " << tcu::toHex(ref)
                                           << ", got " << tcu::toHex(res) << "\n  diffs = (" << diff0 << ", " << diff1
                                           << "), max diff = " << maxDiff << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }

private:
    const glu::Precision m_precision;
};

class PackSnorm2x16Case : public ShaderPackingFunctionCase
{
public:
    PackSnorm2x16Case(tcu::TestContext &testCtx, glu::ShaderType shaderType, glu::Precision precision)
        : ShaderPackingFunctionCase(
              testCtx,
              (string("packsnorm2x16") + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType)).c_str(),
              shaderType)
        , m_precision(precision)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC2, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = packSnorm2x16(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new PackSnorm2x16CaseInstance(ctx, m_shaderType, m_spec, m_precision, getName());
    }

private:
    const glu::Precision m_precision;
};

class UnpackSnorm2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    UnpackSnorm2x16CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const uint32_t maxDiff = 1; // Rounding error.
        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<uint32_t> inputs;
        std::vector<tcu::Vec2> outputs;

        inputs.push_back(0x00000000u);
        inputs.push_back(0x7fff8000u);
        inputs.push_back(0x80007fffu);
        inputs.push_back(0xffffffffu);
        inputs.push_back(0x0001fffeu);

        // Random values.
        for (int ndx = 0; ndx < 95; ndx++)
            inputs.push_back(rnd.getUint32());

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
            {
                const int16_t in0 = (int16_t)(uint16_t)(inputs[valNdx] & 0xffff);
                const int16_t in1 = (int16_t)(uint16_t)(inputs[valNdx] >> 16);
                const float ref0  = de::clamp(float(in0) / 32767.f, -1.0f, 1.0f);
                const float ref1  = de::clamp(float(in1) / 32767.f, -1.0f, 1.0f);
                const float res0  = outputs[valNdx].x();
                const float res1  = outputs[valNdx].y();

                const uint32_t diff0 = getUlpDiff(ref0, res0);
                const uint32_t diff1 = getUlpDiff(ref1, res1);

                if (diff0 > maxDiff || diff1 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
                                           << "  expected unpackSnorm2x16(" << tcu::toHex(inputs[valNdx]) << ") = "
                                           << "vec2(" << HexFloat(ref0) << ", " << HexFloat(ref1) << ")"
                                           << ", got vec2(" << HexFloat(res0) << ", " << HexFloat(res1) << ")"
                                           << "\n  ULP diffs = (" << diff0 << ", " << diff1
                                           << "), max diff = " << maxDiff << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }
};

class UnpackSnorm2x16Case : public ShaderPackingFunctionCase
{
public:
    UnpackSnorm2x16Case(tcu::TestContext &testCtx, glu::ShaderType shaderType)
        : ShaderPackingFunctionCase(testCtx, (string("unpacksnorm2x16") + getShaderTypePostfix(shaderType)).c_str(),
                                    shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = unpackSnorm2x16(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new UnpackSnorm2x16CaseInstance(ctx, m_shaderType, m_spec, getName());
    }
};

class PackUnorm2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    PackUnorm2x16CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec,
                              glu::Precision precision, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
        , m_precision(precision)
    {
    }

    tcu::TestStatus iterate(void)
    {
        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<tcu::Vec2> inputs;
        std::vector<uint32_t> outputs;
        const int                    maxDiff = m_precision == glu::PRECISION_HIGHP    ? 1        : // Rounding only.
                                                  m_precision == glu::PRECISION_MEDIUMP    ? 65    : // (2^-10) * (2^16) + 1
                                                  m_precision == glu::PRECISION_LOWP    ? 257    : 0;    // (2^-8) * (2^16) + 1

        // Special values to check.
        inputs.push_back(tcu::Vec2(0.0f, 0.0f));
        inputs.push_back(tcu::Vec2(0.5f, 1.0f));
        inputs.push_back(tcu::Vec2(1.0f, 0.5f));
        inputs.push_back(tcu::Vec2(-0.5f, 1.5f));
        inputs.push_back(tcu::Vec2(0.25f, 0.75f));

        // Random values, mostly in range.
        for (int ndx = 0; ndx < 15; ndx++)
        {
            inputs.push_back(tcu::randomVector<float, 2>(rnd, tcu::Vec2(0.0f), tcu::Vec2(1.25f)));
        }

        // Large random values.
        for (int ndx = 0; ndx < 80; ndx++)
        {
            inputs.push_back(tcu::randomVector<float, 2>(rnd, tcu::Vec2(-1e5f), tcu::Vec2(0.9e6f)));
        }

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
            {
                const uint16_t ref0 = (uint16_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].x(), 0.0f, 1.0f) * 65535.0f), 0, (1 << 16) - 1);
                const uint16_t ref1 = (uint16_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].y(), 0.0f, 1.0f) * 65535.0f), 0, (1 << 16) - 1);
                const uint32_t ref  = (ref1 << 16) | ref0;
                const uint32_t res  = outputs[valNdx];
                const uint16_t res0 = (uint16_t)(res & 0xffff);
                const uint16_t res1 = (uint16_t)(res >> 16);
                const int diff0     = de::abs((int)ref0 - (int)res0);
                const int diff1     = de::abs((int)ref1 - (int)res1);

                if (diff0 > maxDiff || diff1 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx
                                           << ", expected packUnorm2x16(" << inputs[valNdx] << ") = " << tcu::toHex(ref)
                                           << ", got " << tcu::toHex(res) << "\n  diffs = (" << diff0 << ", " << diff1
                                           << "), max diff = " << maxDiff << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }

private:
    const glu::Precision m_precision;
};

class PackUnorm2x16Case : public ShaderPackingFunctionCase
{
public:
    PackUnorm2x16Case(tcu::TestContext &testCtx, glu::ShaderType shaderType, glu::Precision precision)
        : ShaderPackingFunctionCase(
              testCtx,
              (string("packunorm2x16") + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType)).c_str(),
              shaderType)
        , m_precision(precision)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC2, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = packUnorm2x16(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new PackUnorm2x16CaseInstance(ctx, m_shaderType, m_spec, m_precision, getName());
    }

private:
    const glu::Precision m_precision;
};

class UnpackUnorm2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    UnpackUnorm2x16CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const uint32_t maxDiff = 1; // Rounding error.
        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<uint32_t> inputs;
        std::vector<tcu::Vec2> outputs;

        inputs.push_back(0x00000000u);
        inputs.push_back(0x7fff8000u);
        inputs.push_back(0x80007fffu);
        inputs.push_back(0xffffffffu);
        inputs.push_back(0x0001fffeu);

        // Random values.
        for (int ndx = 0; ndx < 95; ndx++)
            inputs.push_back(rnd.getUint32());

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
            {
                const uint16_t in0 = (uint16_t)(inputs[valNdx] & 0xffff);
                const uint16_t in1 = (uint16_t)(inputs[valNdx] >> 16);
                const float ref0   = float(in0) / 65535.0f;
                const float ref1   = float(in1) / 65535.0f;
                const float res0   = outputs[valNdx].x();
                const float res1   = outputs[valNdx].y();

                const uint32_t diff0 = getUlpDiff(ref0, res0);
                const uint32_t diff1 = getUlpDiff(ref1, res1);

                if (diff0 > maxDiff || diff1 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
                                           << "  expected unpackUnorm2x16(" << tcu::toHex(inputs[valNdx]) << ") = "
                                           << "vec2(" << HexFloat(ref0) << ", " << HexFloat(ref1) << ")"
                                           << ", got vec2(" << HexFloat(res0) << ", " << HexFloat(res1) << ")"
                                           << "\n  ULP diffs = (" << diff0 << ", " << diff1
                                           << "), max diff = " << maxDiff << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }
};

class UnpackUnorm2x16Case : public ShaderPackingFunctionCase
{
public:
    UnpackUnorm2x16Case(tcu::TestContext &testCtx, glu::ShaderType shaderType)
        : ShaderPackingFunctionCase(testCtx, (string("unpackunorm2x16") + getShaderTypePostfix(shaderType)).c_str(),
                                    shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = unpackUnorm2x16(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new UnpackUnorm2x16CaseInstance(ctx, m_shaderType, m_spec, getName());
    }
};

class PackHalf2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    PackHalf2x16CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const int maxDiff = 0; // Values can be represented exactly in mediump.
        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<tcu::Vec2> inputs;
        std::vector<uint32_t> outputs;

        // Special values to check.
        inputs.push_back(tcu::Vec2(0.0f, 0.0f));
        inputs.push_back(tcu::Vec2(0.5f, 1.0f));
        inputs.push_back(tcu::Vec2(1.0f, 0.5f));
        inputs.push_back(tcu::Vec2(-0.5f, 1.5f));
        inputs.push_back(tcu::Vec2(0.25f, 0.75f));

        // Random values.
        {
            const int minExp = -14;
            const int maxExp = 15;

            for (int ndx = 0; ndx < 95; ndx++)
            {
                tcu::Vec2 v;
                for (int c = 0; c < 2; c++)
                {
                    const int s             = rnd.getBool() ? 1 : -1;
                    const int exp           = rnd.getInt(minExp, maxExp);
                    const uint32_t mantissa = rnd.getUint32() & ((1 << 23) - 1);

                    v[c] = tcu::Float32::construct(s, exp ? exp : 1 /* avoid denormals */, (1u << 23) | mantissa)
                               .asFloat();
                }
                inputs.push_back(v);
            }
        }

        // Convert input values to fp16 and back to make sure they can be represented exactly in mediump.
        for (std::vector<tcu::Vec2>::iterator inVal = inputs.begin(); inVal != inputs.end(); ++inVal)
            *inVal = tcu::Vec2(tcu::Float16(inVal->x()).asFloat(), tcu::Float16(inVal->y()).asFloat());

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
            {
                const uint16_t ref0 = (uint16_t)tcu::Float16(inputs[valNdx].x()).bits();
                const uint16_t ref1 = (uint16_t)tcu::Float16(inputs[valNdx].y()).bits();
                const uint32_t ref  = (ref1 << 16) | ref0;
                const uint32_t res  = outputs[valNdx];
                const uint16_t res0 = (uint16_t)(res & 0xffff);
                const uint16_t res1 = (uint16_t)(res >> 16);
                const int diff0     = de::abs((int)ref0 - (int)res0);
                const int diff1     = de::abs((int)ref1 - (int)res1);

                if (diff0 > maxDiff || diff1 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx
                                           << ", expected packHalf2x16(" << inputs[valNdx] << ") = " << tcu::toHex(ref)
                                           << ", got " << tcu::toHex(res) << "\n  diffs = (" << diff0 << ", " << diff1
                                           << "), max diff = " << maxDiff << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }
};

class PackHalf2x16Case : public ShaderPackingFunctionCase
{
public:
    PackHalf2x16Case(tcu::TestContext &testCtx, glu::ShaderType shaderType)
        : ShaderPackingFunctionCase(testCtx, (string("packhalf2x16") + getShaderTypePostfix(shaderType)).c_str(),
                                    shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = packHalf2x16(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new PackHalf2x16CaseInstance(ctx, m_shaderType, m_spec, getName());
    }
};

class UnpackHalf2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
    enum Sign
    {
        POSITIVE = 0,
        NEGATIVE
    };
    enum SubnormalizedConversionType
    {
        UNKNOWN = 0,
        CONVERTED,
        ZERO_FLUSHED,
    };

public:
    UnpackHalf2x16CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const int minExp           = -14;
        const int maxExp           = 15;
        const int mantBits         = 10;
        const uint32_t mantBitMask = (1u << mantBits) - 1u;
        tcu::TestLog &log          = m_testCtx.getLog();

        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<uint32_t> inputs;
        std::vector<tcu::Vec2> outputs;

        // Special values.
        inputs.push_back((tcu::Float16(0.0f).bits() << 16) | tcu::Float16(1.0f).bits());
        inputs.push_back((tcu::Float16(1.0f).bits() << 16) | tcu::Float16(0.0f).bits());
        inputs.push_back((tcu::Float16(-1.0f).bits() << 16) | tcu::Float16(0.5f).bits());
        inputs.push_back((tcu::Float16(0.5f).bits() << 16) | tcu::Float16(-0.5f).bits());
        // Special subnormal value: single lowest bit set
        inputs.push_back((tcu::Float16(composeHalfFloat(POSITIVE, 0u, 1u)).bits() << 16) |
                         tcu::Float16(composeHalfFloat(NEGATIVE, 0u, 1u)).bits());
        // Special subnormal value: single highest fraction bit set
        inputs.push_back((tcu::Float16(composeHalfFloat(NEGATIVE, 0u, 1u << (mantBits - 1u))).bits() << 16) |
                         tcu::Float16(composeHalfFloat(POSITIVE, 0u, 1u << (mantBits - 1u))).bits());
        // Special subnormal value: all fraction bits set
        inputs.push_back((tcu::Float16(composeHalfFloat(POSITIVE, 0u, mantBitMask)).bits() << 16) |
                         tcu::Float16(composeHalfFloat(NEGATIVE, 0u, mantBitMask)).bits());

        // Construct random values.
        for (int ndx = 0; ndx < 90; ndx++)
        {
            uint32_t inVal = 0;
            for (int c = 0; c < 2; c++)
            {
                const int s             = rnd.getBool() ? 1 : -1;
                const int exp           = rnd.getInt(minExp, maxExp);
                const uint32_t mantissa = rnd.getUint32() & mantBitMask;
                const uint16_t value    = tcu::Float16::construct(s, exp != 0 ? exp : 1 /* avoid denorm */,
                                                                  static_cast<uint16_t>((1u << 10) | mantissa))
                                           .bits();

                inVal |= value << (16u * c);
            }
            inputs.push_back(inVal);
        }
        for (int ndx = 0; ndx < 15; ndx++)
        {
            uint32_t inVal = 0;
            for (int c = 0; c < 2; c++)
            {
                const Sign sign         = rnd.getBool() ? POSITIVE : NEGATIVE;
                const uint32_t mantissa = rnd.getUint32() & mantBitMask;
                const uint16_t value    = tcu::Float16(composeHalfFloat(sign, 0u /* force denorm */, mantissa)).bits();

                inVal |= value << (16u * c);
            }
            inputs.push_back(inVal);
        }

        outputs.resize(inputs.size());

        log << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
            << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues                    = (int)inputs.size();
            const int maxPrints                    = 10;
            int numFailed                          = 0;
            SubnormalizedConversionType conversion = UNKNOWN;

            for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
            {
                const uint16_t in0 = (uint16_t)(inputs[valNdx] & 0xffff);
                const uint16_t in1 = (uint16_t)(inputs[valNdx] >> 16);
                const float res0   = outputs[valNdx].x();
                const float res1   = outputs[valNdx].y();

                const bool value0 = checkValue(in0, res0, conversion);
                // note: do not avoid calling checkValue for in1 if it failed for in0 by using && laziness
                // checkValue may potentially change 'conversion' parameter if it was set to UNKNOWN so far
                const bool value1   = checkValue(in1, res1, conversion);
                const bool valuesOK = value0 && value1;

                if (!valuesOK)
                {
                    if (numFailed < maxPrints)
                        printErrorMessage(log, valNdx, in0, in1, res0, res1);
                    else if (numFailed == maxPrints)
                        log << TestLog::Message << "..." << TestLog::EndMessage;
                    ++numFailed;
                }
            }

            log << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }

private:
    bool checkValue(uint16_t inValue, float outValue, SubnormalizedConversionType &conversion)
    {
        const tcu::Float16 temp = tcu::Float16(inValue);
        const float ref         = temp.asFloat();
        const uint32_t refBits  = tcu::Float32(ref).bits();
        const uint32_t resBits  = tcu::Float32(outValue).bits();
        const bool bitMatch     = (refBits ^ resBits) == 0u;
        const bool denorm       = temp.isDenorm();

        if (conversion != CONVERTED && denorm)
        {
            if (resBits == 0 || (ref < 0 && resBits == 0x80000000UL))
            {
                conversion = ZERO_FLUSHED;
                return true;
            }
            if (conversion != ZERO_FLUSHED && bitMatch)
            {
                conversion = CONVERTED;
                return true;
            }
            return false;
        }
        else if (bitMatch)
            return true;
        return false;
    }
    void printErrorMessage(tcu::TestLog &log, uint32_t valNdx, uint16_t in0, uint16_t in1, float out0, float out1)
    {
        const float ref0        = tcu::Float16(in0).asFloat();
        const uint32_t refBits0 = tcu::Float32(ref0).bits();
        const uint32_t resBits0 = tcu::Float32(out0).bits();
        const float ref1        = tcu::Float16(in1).asFloat();
        const uint32_t refBits1 = tcu::Float32(ref1).bits();
        const uint32_t resBits1 = tcu::Float32(out1).bits();
        log << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
            << "  expected unpackHalf2x16(" << tcu::toHex((in1 << 16u) | in0) << ") = "
            << "vec2(" << ref0 << " / " << tcu::toHex(refBits0) << ", " << ref1 << " / " << tcu::toHex(refBits1) << ")"
            << ", got vec2(" << out0 << " / " << tcu::toHex(resBits0) << ", " << out1 << " / " << tcu::toHex(resBits1)
            << ")" << TestLog::EndMessage;
    }
    uint16_t composeHalfFloat(Sign sign, uint32_t exponent, uint32_t significand)
    {
        const uint32_t BitMask_05 = (1u << 5u) - 1u;
        const uint32_t BitMask_10 = (1u << 10u) - 1u;
        const uint32_t BitMask_16 = (1u << 16u) - 1u;
        DE_UNREF(BitMask_05);
        DE_UNREF(BitMask_10);
        DE_UNREF(BitMask_16);
        DE_ASSERT((exponent & ~BitMask_05) == 0u);
        DE_ASSERT((significand & ~BitMask_10) == 0u);
        const uint32_t value = (((sign == NEGATIVE ? 1u : 0u) << 5u | exponent) << 10u) | significand;
        DE_ASSERT((value & ~BitMask_16) == 0u);
        return static_cast<uint16_t>(value);
    }
};

class UnpackHalf2x16Case : public ShaderPackingFunctionCase
{
public:
    UnpackHalf2x16Case(tcu::TestContext &testCtx, glu::ShaderType shaderType)
        : ShaderPackingFunctionCase(testCtx, (string("unpackhalf2x16") + getShaderTypePostfix(shaderType)).c_str(),
                                    shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_MEDIUMP)));

        m_spec.source = "out0 = unpackHalf2x16(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new UnpackHalf2x16CaseInstance(ctx, m_shaderType, m_spec, getName());
    }
};

class PackSnorm4x8CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    PackSnorm4x8CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec,
                             glu::Precision precision, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
        , m_precision(precision)
    {
    }

    tcu::TestStatus iterate(void)
    {
        de::Random rnd(deStringHash(m_name) ^ 0x42f2c0);
        std::vector<tcu::Vec4> inputs;
        std::vector<uint32_t> outputs;
        const int                    maxDiff = m_precision == glu::PRECISION_HIGHP    ? 1    : // Rounding only.
                                                  m_precision == glu::PRECISION_MEDIUMP    ? 1    : // (2^-10) * (2^7) + 1
                                                  m_precision == glu::PRECISION_LOWP    ? 2    : 0;    // (2^-8) * (2^7) + 1

        // Special values to check.
        inputs.push_back(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
        inputs.push_back(tcu::Vec4(-1.0f, 1.0f, -1.0f, 1.0f));
        inputs.push_back(tcu::Vec4(0.5f, -0.5f, -0.5f, 0.5f));
        inputs.push_back(tcu::Vec4(-1.5f, 1.5f, -1.5f, 1.5f));
        inputs.push_back(tcu::Vec4(0.25f, -0.75f, -0.25f, 0.75f));

        // Random values, mostly in range.
        for (int ndx = 0; ndx < 15; ndx++)
        {
            inputs.push_back(tcu::randomVector<float, 4>(rnd, tcu::Vec4(-1.25f), tcu::Vec4(1.25f)));
        }

        // Large random values.
        for (int ndx = 0; ndx < 80; ndx++)
        {
            inputs.push_back(tcu::randomVector<float, 4>(rnd, tcu::Vec4(-0.5e6f), tcu::Vec4(0.5e6f)));
        }

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < numValues; valNdx++)
            {
                const uint16_t ref0 = (uint8_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].x(), -1.0f, 1.0f) * 127.0f), -(1 << 7), (1 << 7) - 1);
                const uint16_t ref1 = (uint8_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].y(), -1.0f, 1.0f) * 127.0f), -(1 << 7), (1 << 7) - 1);
                const uint16_t ref2 = (uint8_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].z(), -1.0f, 1.0f) * 127.0f), -(1 << 7), (1 << 7) - 1);
                const uint16_t ref3 = (uint8_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].w(), -1.0f, 1.0f) * 127.0f), -(1 << 7), (1 << 7) - 1);
                const uint32_t ref =
                    (uint32_t(ref3) << 24) | (uint32_t(ref2) << 16) | (uint32_t(ref1) << 8) | uint32_t(ref0);
                const uint32_t res  = outputs[valNdx];
                const uint16_t res0 = (uint8_t)(res & 0xff);
                const uint16_t res1 = (uint8_t)((res >> 8) & 0xff);
                const uint16_t res2 = (uint8_t)((res >> 16) & 0xff);
                const uint16_t res3 = (uint8_t)((res >> 24) & 0xff);
                const int diff0     = de::abs((int)ref0 - (int)res0);
                const int diff1     = de::abs((int)ref1 - (int)res1);
                const int diff2     = de::abs((int)ref2 - (int)res2);
                const int diff3     = de::abs((int)ref3 - (int)res3);

                if (diff0 > maxDiff || diff1 > maxDiff || diff2 > maxDiff || diff3 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog()
                            << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ", expected packSnorm4x8("
                            << inputs[valNdx] << ") = " << tcu::toHex(ref) << ", got " << tcu::toHex(res)
                            << "\n  diffs = " << tcu::IVec4(diff0, diff1, diff2, diff3) << ", max diff = " << maxDiff
                            << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }

private:
    const glu::Precision m_precision;
};

class PackSnorm4x8Case : public ShaderPackingFunctionCase
{
public:
    PackSnorm4x8Case(tcu::TestContext &testCtx, glu::ShaderType shaderType, glu::Precision precision)
        : ShaderPackingFunctionCase(
              testCtx,
              (string("packsnorm4x8") + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType)).c_str(),
              shaderType)
        , m_precision(precision)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC4, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = packSnorm4x8(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new PackSnorm4x8CaseInstance(ctx, m_shaderType, m_spec, m_precision, getName());
    }

private:
    const glu::Precision m_precision;
};

class UnpackSnorm4x8CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    UnpackSnorm4x8CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const uint32_t maxDiff = 1; // Rounding error.
        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<uint32_t> inputs;
        std::vector<tcu::Vec4> outputs;

        inputs.push_back(0x00000000u);
        inputs.push_back(0x7fff8000u);
        inputs.push_back(0x80007fffu);
        inputs.push_back(0xffffffffu);
        inputs.push_back(0x0001fffeu);

        // Random values.
        for (int ndx = 0; ndx < 95; ndx++)
            inputs.push_back(rnd.getUint32());

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
            {
                const int8_t in0 = (int8_t)(uint8_t)(inputs[valNdx] & 0xff);
                const int8_t in1 = (int8_t)(uint8_t)((inputs[valNdx] >> 8) & 0xff);
                const int8_t in2 = (int8_t)(uint8_t)((inputs[valNdx] >> 16) & 0xff);
                const int8_t in3 = (int8_t)(uint8_t)(inputs[valNdx] >> 24);
                const float ref0 = de::clamp(float(in0) / 127.f, -1.0f, 1.0f);
                const float ref1 = de::clamp(float(in1) / 127.f, -1.0f, 1.0f);
                const float ref2 = de::clamp(float(in2) / 127.f, -1.0f, 1.0f);
                const float ref3 = de::clamp(float(in3) / 127.f, -1.0f, 1.0f);
                const float res0 = outputs[valNdx].x();
                const float res1 = outputs[valNdx].y();
                const float res2 = outputs[valNdx].z();
                const float res3 = outputs[valNdx].w();

                const uint32_t diff0 = getUlpDiff(ref0, res0);
                const uint32_t diff1 = getUlpDiff(ref1, res1);
                const uint32_t diff2 = getUlpDiff(ref2, res2);
                const uint32_t diff3 = getUlpDiff(ref3, res3);

                if (diff0 > maxDiff || diff1 > maxDiff || diff2 > maxDiff || diff3 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
                                           << "  expected unpackSnorm4x8(" << tcu::toHex(inputs[valNdx]) << ") = "
                                           << "vec4(" << HexFloat(ref0) << ", " << HexFloat(ref1) << ", "
                                           << HexFloat(ref2) << ", " << HexFloat(ref3) << ")"
                                           << ", got vec4(" << HexFloat(res0) << ", " << HexFloat(res1) << ", "
                                           << HexFloat(res2) << ", " << HexFloat(res3) << ")"
                                           << "\n  ULP diffs = (" << diff0 << ", " << diff1 << ", " << diff2 << ", "
                                           << diff3 << "), max diff = " << maxDiff << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }
};

class UnpackSnorm4x8Case : public ShaderPackingFunctionCase
{
public:
    UnpackSnorm4x8Case(tcu::TestContext &testCtx, glu::ShaderType shaderType)
        : ShaderPackingFunctionCase(testCtx, (string("unpacksnorm4x8") + getShaderTypePostfix(shaderType)).c_str(),
                                    shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = unpackSnorm4x8(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new UnpackSnorm4x8CaseInstance(ctx, m_shaderType, m_spec, getName());
    }
};

class PackUnorm4x8CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    PackUnorm4x8CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec,
                             glu::Precision precision, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
        , m_precision(precision)
    {
    }

    tcu::TestStatus iterate(void)
    {
        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<tcu::Vec4> inputs;
        std::vector<uint32_t> outputs;
        const int                    maxDiff = m_precision == glu::PRECISION_HIGHP    ? 1    : // Rounding only.
                                                  m_precision == glu::PRECISION_MEDIUMP    ? 1    : // (2^-10) * (2^8) + 1
                                                  m_precision == glu::PRECISION_LOWP    ? 2    : 0;    // (2^-8) * (2^8) + 1

        // Special values to check.
        inputs.push_back(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
        inputs.push_back(tcu::Vec4(-1.0f, 1.0f, -1.0f, 1.0f));
        inputs.push_back(tcu::Vec4(0.5f, -0.5f, -0.5f, 0.5f));
        inputs.push_back(tcu::Vec4(-1.5f, 1.5f, -1.5f, 1.5f));
        inputs.push_back(tcu::Vec4(0.25f, -0.75f, -0.25f, 0.75f));

        // Random values, mostly in range.
        for (int ndx = 0; ndx < 15; ndx++)
        {
            inputs.push_back(tcu::randomVector<float, 4>(rnd, tcu::Vec4(-0.125f), tcu::Vec4(1.125f)));
        }

        // Large random values.
        for (int ndx = 0; ndx < 80; ndx++)
        {
            inputs.push_back(tcu::randomVector<float, 4>(rnd, tcu::Vec4(-1e5f), tcu::Vec4(0.9e6f)));
        }

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
            {
                const uint16_t ref0 = (uint8_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].x(), 0.0f, 1.0f) * 255.0f), 0, (1 << 8) - 1);
                const uint16_t ref1 = (uint8_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].y(), 0.0f, 1.0f) * 255.0f), 0, (1 << 8) - 1);
                const uint16_t ref2 = (uint8_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].z(), 0.0f, 1.0f) * 255.0f), 0, (1 << 8) - 1);
                const uint16_t ref3 = (uint8_t)de::clamp(
                    deRoundFloatToInt32(de::clamp(inputs[valNdx].w(), 0.0f, 1.0f) * 255.0f), 0, (1 << 8) - 1);
                const uint32_t ref =
                    (uint32_t(ref3) << 24) | (uint32_t(ref2) << 16) | (uint32_t(ref1) << 8) | uint32_t(ref0);
                const uint32_t res  = outputs[valNdx];
                const uint16_t res0 = (uint8_t)(res & 0xff);
                const uint16_t res1 = (uint8_t)((res >> 8) & 0xff);
                const uint16_t res2 = (uint8_t)((res >> 16) & 0xff);
                const uint16_t res3 = (uint8_t)((res >> 24) & 0xff);
                const int diff0     = de::abs((int)ref0 - (int)res0);
                const int diff1     = de::abs((int)ref1 - (int)res1);
                const int diff2     = de::abs((int)ref2 - (int)res2);
                const int diff3     = de::abs((int)ref3 - (int)res3);

                if (diff0 > maxDiff || diff1 > maxDiff || diff2 > maxDiff || diff3 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog()
                            << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ", expected packUnorm4x8("
                            << inputs[valNdx] << ") = " << tcu::toHex(ref) << ", got " << tcu::toHex(res)
                            << "\n  diffs = " << tcu::IVec4(diff0, diff1, diff2, diff3) << ", max diff = " << maxDiff
                            << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }

private:
    const glu::Precision m_precision;
};

class PackUnorm4x8Case : public ShaderPackingFunctionCase
{
public:
    PackUnorm4x8Case(tcu::TestContext &testCtx, glu::ShaderType shaderType, glu::Precision precision)
        : ShaderPackingFunctionCase(
              testCtx,
              (string("packunorm4x8") + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType)).c_str(),
              shaderType)
        , m_precision(precision)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC4, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = packUnorm4x8(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new PackUnorm4x8CaseInstance(ctx, m_shaderType, m_spec, m_precision, getName());
    }

private:
    const glu::Precision m_precision;
};

class UnpackUnorm4x8CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
    UnpackUnorm4x8CaseInstance(Context &context, glu::ShaderType shaderType, const ShaderSpec &spec, const char *name)
        : ShaderPackingFunctionTestInstance(context, shaderType, spec, name)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const uint32_t maxDiff = 1; // Rounding error.
        de::Random rnd(deStringHash(m_name) ^ 0x776002);
        std::vector<uint32_t> inputs;
        std::vector<tcu::Vec4> outputs;

        inputs.push_back(0x00000000u);
        inputs.push_back(0x7fff8000u);
        inputs.push_back(0x80007fffu);
        inputs.push_back(0xffffffffu);
        inputs.push_back(0x0001fffeu);

        // Random values.
        for (int ndx = 0; ndx < 95; ndx++)
            inputs.push_back(rnd.getUint32());

        outputs.resize(inputs.size());

        m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values"
                           << tcu::TestLog::EndMessage;

        {
            const void *in = &inputs[0];
            void *out      = &outputs[0];

            m_executor->execute((int)inputs.size(), &in, &out);
        }

        // Verify
        {
            const int numValues = (int)inputs.size();
            const int maxPrints = 10;
            int numFailed       = 0;

            for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
            {
                const uint8_t in0 = (uint8_t)(inputs[valNdx] & 0xff);
                const uint8_t in1 = (uint8_t)((inputs[valNdx] >> 8) & 0xff);
                const uint8_t in2 = (uint8_t)((inputs[valNdx] >> 16) & 0xff);
                const uint8_t in3 = (uint8_t)(inputs[valNdx] >> 24);
                const float ref0  = de::clamp(float(in0) / 255.f, 0.0f, 1.0f);
                const float ref1  = de::clamp(float(in1) / 255.f, 0.0f, 1.0f);
                const float ref2  = de::clamp(float(in2) / 255.f, 0.0f, 1.0f);
                const float ref3  = de::clamp(float(in3) / 255.f, 0.0f, 1.0f);
                const float res0  = outputs[valNdx].x();
                const float res1  = outputs[valNdx].y();
                const float res2  = outputs[valNdx].z();
                const float res3  = outputs[valNdx].w();

                const uint32_t diff0 = getUlpDiff(ref0, res0);
                const uint32_t diff1 = getUlpDiff(ref1, res1);
                const uint32_t diff2 = getUlpDiff(ref2, res2);
                const uint32_t diff3 = getUlpDiff(ref3, res3);

                if (diff0 > maxDiff || diff1 > maxDiff || diff2 > maxDiff || diff3 > maxDiff)
                {
                    if (numFailed < maxPrints)
                    {
                        m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
                                           << "  expected unpackUnorm4x8(" << tcu::toHex(inputs[valNdx]) << ") = "
                                           << "vec4(" << HexFloat(ref0) << ", " << HexFloat(ref1) << ", "
                                           << HexFloat(ref2) << ", " << HexFloat(ref3) << ")"
                                           << ", got vec4(" << HexFloat(res0) << ", " << HexFloat(res1) << ", "
                                           << HexFloat(res2) << ", " << HexFloat(res3) << ")"
                                           << "\n  ULP diffs = (" << diff0 << ", " << diff1 << ", " << diff2 << ", "
                                           << diff3 << "), max diff = " << maxDiff << TestLog::EndMessage;
                    }
                    else if (numFailed == maxPrints)
                        m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

                    numFailed += 1;
                }
            }

            m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed"
                               << TestLog::EndMessage;

            if (numFailed == 0)
                return tcu::TestStatus::pass("Pass");
            else
                return tcu::TestStatus::fail("Result comparison failed");
        }
    }
};

class UnpackUnorm4x8Case : public ShaderPackingFunctionCase
{
public:
    UnpackUnorm4x8Case(tcu::TestContext &testCtx, glu::ShaderType shaderType)
        : ShaderPackingFunctionCase(testCtx, (string("unpackunorm4x8") + getShaderTypePostfix(shaderType)).c_str(),
                                    shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

        m_spec.source = "out0 = unpackUnorm4x8(in0);";
    }

    TestInstance *createInstance(Context &ctx) const
    {
        return new UnpackUnorm4x8CaseInstance(ctx, m_shaderType, m_spec, getName());
    }
};

ShaderPackingFunctionTests::ShaderPackingFunctionTests(tcu::TestContext &testCtx)
    : tcu::TestCaseGroup(testCtx, "pack_unpack")
{
}

ShaderPackingFunctionTests::~ShaderPackingFunctionTests(void)
{
}

void ShaderPackingFunctionTests::init(void)
{
    // New built-in functions in GLES 3.1
    {
        const glu::ShaderType allShaderTypes[] = {glu::SHADERTYPE_VERTEX,
                                                  glu::SHADERTYPE_TESSELLATION_CONTROL,
                                                  glu::SHADERTYPE_TESSELLATION_EVALUATION,
                                                  glu::SHADERTYPE_GEOMETRY,
                                                  glu::SHADERTYPE_FRAGMENT,
                                                  glu::SHADERTYPE_COMPUTE};

        // packSnorm4x8
        for (int prec = glu::PRECISION_MEDIUMP; prec < glu::PRECISION_LAST; prec++)
        {
            for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(allShaderTypes); shaderTypeNdx++)
                addChild(new PackSnorm4x8Case(m_testCtx, allShaderTypes[shaderTypeNdx], glu::Precision(prec)));
        }

        // unpackSnorm4x8
        for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(allShaderTypes); shaderTypeNdx++)
            addChild(new UnpackSnorm4x8Case(m_testCtx, allShaderTypes[shaderTypeNdx]));

        // packUnorm4x8
        for (int prec = glu::PRECISION_MEDIUMP; prec < glu::PRECISION_LAST; prec++)
        {
            for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(allShaderTypes); shaderTypeNdx++)
                addChild(new PackUnorm4x8Case(m_testCtx, allShaderTypes[shaderTypeNdx], glu::Precision(prec)));
        }

        // unpackUnorm4x8
        for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(allShaderTypes); shaderTypeNdx++)
            addChild(new UnpackUnorm4x8Case(m_testCtx, allShaderTypes[shaderTypeNdx]));
    }

    // GLES 3 functions in new shader types.
    {
        const glu::ShaderType newShaderTypes[] = {glu::SHADERTYPE_GEOMETRY, glu::SHADERTYPE_COMPUTE};

        // packSnorm2x16
        for (int prec = glu::PRECISION_MEDIUMP; prec < glu::PRECISION_LAST; prec++)
        {
            for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(newShaderTypes); shaderTypeNdx++)
                addChild(new PackSnorm2x16Case(m_testCtx, newShaderTypes[shaderTypeNdx], glu::Precision(prec)));
        }

        // unpackSnorm2x16
        for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(newShaderTypes); shaderTypeNdx++)
            addChild(new UnpackSnorm2x16Case(m_testCtx, newShaderTypes[shaderTypeNdx]));

        // packUnorm2x16
        for (int prec = glu::PRECISION_MEDIUMP; prec < glu::PRECISION_LAST; prec++)
        {
            for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(newShaderTypes); shaderTypeNdx++)
                addChild(new PackUnorm2x16Case(m_testCtx, newShaderTypes[shaderTypeNdx], glu::Precision(prec)));
        }

        // unpackUnorm2x16
        for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(newShaderTypes); shaderTypeNdx++)
            addChild(new UnpackUnorm2x16Case(m_testCtx, newShaderTypes[shaderTypeNdx]));

        // packHalf2x16
        for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(newShaderTypes); shaderTypeNdx++)
            addChild(new PackHalf2x16Case(m_testCtx, newShaderTypes[shaderTypeNdx]));

        // unpackHalf2x16
        for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(newShaderTypes); shaderTypeNdx++)
            addChild(new UnpackHalf2x16Case(m_testCtx, newShaderTypes[shaderTypeNdx]));
    }
}

} // namespace shaderexecutor
} // namespace vkt
