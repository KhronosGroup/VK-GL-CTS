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

inline deUint32 getUlpDiff (float a, float b)
{
	const deUint32	aBits	= tcu::Float32(a).bits();
	const deUint32	bBits	= tcu::Float32(b).bits();
	return aBits > bBits ? aBits - bBits : bBits - aBits;
}

struct HexFloat
{
	const float value;
	HexFloat (const float value_) : value(value_) {}
};

std::ostream& operator<< (std::ostream& str, const HexFloat& v)
{
	return str << v.value << " / " << tcu::toHex(tcu::Float32(v.value).bits());
}

} // anonymous

// ShaderPackingFunctionCase

class ShaderPackingFunctionCase : public TestCase
{
public:
										ShaderPackingFunctionCase			(tcu::TestContext& testCtx, const char* name, const char* description, glu::ShaderType shaderType);
										~ShaderPackingFunctionCase			(void);

	virtual	void						initPrograms						(vk::SourceCollections& programCollection) const
										{
											generateSources(m_shaderType, m_spec, programCollection);
										}

protected:
	const glu::ShaderType				m_shaderType;
	ShaderSpec							m_spec;

private:
										ShaderPackingFunctionCase			(const ShaderPackingFunctionCase& other);
	ShaderPackingFunctionCase&			operator=							(const ShaderPackingFunctionCase& other);
};

ShaderPackingFunctionCase::ShaderPackingFunctionCase (tcu::TestContext& testCtx, const char* name, const char* description, glu::ShaderType shaderType)
	: TestCase		(testCtx, name, description)
	, m_shaderType	(shaderType)
{
}

ShaderPackingFunctionCase::~ShaderPackingFunctionCase (void)
{
}

// ShaderPackingFunctionTestInstance

class ShaderPackingFunctionTestInstance : public TestInstance
{
public:
										ShaderPackingFunctionTestInstance	(Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, const char* name)
											: TestInstance	(context)
											, m_testCtx		(context.getTestContext())
											, m_shaderType	(shaderType)
											, m_spec		(spec)
											, m_name		(name)
											, m_executor	(createExecutor(context, m_shaderType, m_spec))
										{
										}
	virtual tcu::TestStatus				iterate								(void) = 0;
protected:
	tcu::TestContext&					m_testCtx;
	const glu::ShaderType				m_shaderType;
	ShaderSpec							m_spec;
	const char*							m_name;
	de::UniquePtr<ShaderExecutor>		m_executor;
};

// Test cases

class PackSnorm2x16CaseInstance: public ShaderPackingFunctionTestInstance
{
public:
	PackSnorm2x16CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, glu::Precision precision, const char* name)
		: ShaderPackingFunctionTestInstance	(context, shaderType, spec, name)
		, m_precision						(precision)
	{
	}

	tcu::TestStatus iterate (void)
	{
		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<tcu::Vec2>		inputs;
		std::vector<deUint32>		outputs;
		const int					maxDiff		= m_precision == glu::PRECISION_HIGHP	? 1		:		// Rounding only.
												  m_precision == glu::PRECISION_MEDIUMP	? 33	:		// (2^-10) * (2^15) + 1
												  m_precision == glu::PRECISION_LOWP	? 129	: 0;	// (2^-8) * (2^15) + 1

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

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < numValues; valNdx++)
			{
				const deUint16	ref0	= (deUint16)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].x(), -1.0f, 1.0f) * 32767.0f), -(1<<15), (1<<15)-1);
				const deUint16	ref1	= (deUint16)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].y(), -1.0f, 1.0f) * 32767.0f), -(1<<15), (1<<15)-1);
				const deUint32	ref		= (ref1 << 16) | ref0;
				const deUint32	res		= outputs[valNdx];
				const deUint16	res0	= (deUint16)(res & 0xffff);
				const deUint16	res1	= (deUint16)(res >> 16);
				const int		diff0	= de::abs((int)ref0 - (int)res0);
				const int		diff1	= de::abs((int)ref1 - (int)res1);

				if (diff0 > maxDiff || diff1 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx
															   << ", expected packSnorm2x16(" << inputs[valNdx] << ") = " << tcu::toHex(ref)
															   << ", got " << tcu::toHex(res)
															   << "\n  diffs = (" << diff0 << ", " << diff1 << "), max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	PackSnorm2x16Case (tcu::TestContext& testCtx, glu::ShaderType shaderType, glu::Precision precision)
		: ShaderPackingFunctionCase	(testCtx, (string("packsnorm2x16") + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType)).c_str(), "packSnorm2x16", shaderType)
		, m_precision				(precision)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC2, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = packSnorm2x16(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new PackSnorm2x16CaseInstance(ctx, m_shaderType, m_spec, m_precision, getName());
	}

private:
	const glu::Precision m_precision;
};

class UnpackSnorm2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
	UnpackSnorm2x16CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, const char* name)
	: ShaderPackingFunctionTestInstance (context, shaderType, spec, name)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const deUint32				maxDiff		= 1; // Rounding error.
		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<deUint32>		inputs;
		std::vector<tcu::Vec2>		outputs;

		inputs.push_back(0x00000000u);
		inputs.push_back(0x7fff8000u);
		inputs.push_back(0x80007fffu);
		inputs.push_back(0xffffffffu);
		inputs.push_back(0x0001fffeu);

		// Random values.
		for (int ndx = 0; ndx < 95; ndx++)
			inputs.push_back(rnd.getUint32());

		outputs.resize(inputs.size());

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
			{
				const deInt16	in0			= (deInt16)(deUint16)(inputs[valNdx] & 0xffff);
				const deInt16	in1			= (deInt16)(deUint16)(inputs[valNdx] >> 16);
				const float		ref0		= de::clamp(float(in0) / 32767.f, -1.0f, 1.0f);
				const float		ref1		= de::clamp(float(in1) / 32767.f, -1.0f, 1.0f);
				const float		res0		= outputs[valNdx].x();
				const float		res1		= outputs[valNdx].y();

				const deUint32	diff0	= getUlpDiff(ref0, res0);
				const deUint32	diff1	= getUlpDiff(ref1, res1);

				if (diff0 > maxDiff || diff1 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
															   << "  expected unpackSnorm2x16(" << tcu::toHex(inputs[valNdx]) << ") = "
															   << "vec2(" << HexFloat(ref0) << ", " << HexFloat(ref1) << ")"
															   << ", got vec2(" << HexFloat(res0) << ", " << HexFloat(res1) << ")"
															   << "\n  ULP diffs = (" << diff0 << ", " << diff1 << "), max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	UnpackSnorm2x16Case (tcu::TestContext& testCtx, glu::ShaderType shaderType)
		: ShaderPackingFunctionCase	(testCtx, (string("unpacksnorm2x16") + getShaderTypePostfix(shaderType)).c_str(), "unpackSnorm2x16", shaderType)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = unpackSnorm2x16(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new UnpackSnorm2x16CaseInstance(ctx, m_shaderType, m_spec, getName());
	}
};

class PackUnorm2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
	PackUnorm2x16CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, glu::Precision precision, const char* name)
	: ShaderPackingFunctionTestInstance	(context, shaderType, spec, name)
	, m_precision						(precision)
	{
	}

	tcu::TestStatus iterate (void)
	{
		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<tcu::Vec2>		inputs;
		std::vector<deUint32>		outputs;
		const int					maxDiff		= m_precision == glu::PRECISION_HIGHP	? 1		:		// Rounding only.
												  m_precision == glu::PRECISION_MEDIUMP	? 65	:		// (2^-10) * (2^16) + 1
												  m_precision == glu::PRECISION_LOWP	? 257	: 0;	// (2^-8) * (2^16) + 1

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

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
			{
				const deUint16	ref0	= (deUint16)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].x(), 0.0f, 1.0f) * 65535.0f), 0, (1<<16)-1);
				const deUint16	ref1	= (deUint16)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].y(), 0.0f, 1.0f) * 65535.0f), 0, (1<<16)-1);
				const deUint32	ref		= (ref1 << 16) | ref0;
				const deUint32	res		= outputs[valNdx];
				const deUint16	res0	= (deUint16)(res & 0xffff);
				const deUint16	res1	= (deUint16)(res >> 16);
				const int		diff0	= de::abs((int)ref0 - (int)res0);
				const int		diff1	= de::abs((int)ref1 - (int)res1);

				if (diff0 > maxDiff || diff1 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx
															   << ", expected packUnorm2x16(" << inputs[valNdx] << ") = " << tcu::toHex(ref)
															   << ", got " << tcu::toHex(res)
															   << "\n  diffs = (" << diff0 << ", " << diff1 << "), max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	PackUnorm2x16Case (tcu::TestContext& testCtx, glu::ShaderType shaderType, glu::Precision precision)
		: ShaderPackingFunctionCase	(testCtx, (string("packunorm2x16") + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType)).c_str(), "packUnorm2x16", shaderType)
		, m_precision				(precision)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC2, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = packUnorm2x16(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new PackUnorm2x16CaseInstance(ctx, m_shaderType, m_spec, m_precision, getName());
	}

private:
	const glu::Precision m_precision;
};

class UnpackUnorm2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
	UnpackUnorm2x16CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, const char* name)
		: ShaderPackingFunctionTestInstance (context, shaderType, spec, name)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const deUint32				maxDiff		= 1; // Rounding error.
		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<deUint32>		inputs;
		std::vector<tcu::Vec2>		outputs;

		inputs.push_back(0x00000000u);
		inputs.push_back(0x7fff8000u);
		inputs.push_back(0x80007fffu);
		inputs.push_back(0xffffffffu);
		inputs.push_back(0x0001fffeu);

		// Random values.
		for (int ndx = 0; ndx < 95; ndx++)
			inputs.push_back(rnd.getUint32());

		outputs.resize(inputs.size());

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
			{
				const deUint16	in0			= (deUint16)(inputs[valNdx] & 0xffff);
				const deUint16	in1			= (deUint16)(inputs[valNdx] >> 16);
				const float		ref0		= float(in0) / 65535.0f;
				const float		ref1		= float(in1) / 65535.0f;
				const float		res0		= outputs[valNdx].x();
				const float		res1		= outputs[valNdx].y();

				const deUint32	diff0		= getUlpDiff(ref0, res0);
				const deUint32	diff1		= getUlpDiff(ref1, res1);

				if (diff0 > maxDiff || diff1 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
															   << "  expected unpackUnorm2x16(" << tcu::toHex(inputs[valNdx]) << ") = "
															   << "vec2(" << HexFloat(ref0) << ", " << HexFloat(ref1) << ")"
															   << ", got vec2(" << HexFloat(res0) << ", " << HexFloat(res1) << ")"
															   << "\n  ULP diffs = (" << diff0 << ", " << diff1 << "), max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	UnpackUnorm2x16Case (tcu::TestContext& testCtx, glu::ShaderType shaderType)
		: ShaderPackingFunctionCase(testCtx, (string("unpackunorm2x16") + getShaderTypePostfix(shaderType)).c_str(), "unpackUnorm2x16", shaderType)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = unpackUnorm2x16(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new UnpackUnorm2x16CaseInstance(ctx, m_shaderType, m_spec, getName());
	}

};

class PackHalf2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
	PackHalf2x16CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, const char* name)
	: ShaderPackingFunctionTestInstance (context, shaderType, spec, name)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const int					maxDiff		= 0; // Values can be represented exactly in mediump.
		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<tcu::Vec2>		inputs;
		std::vector<deUint32>		outputs;

		// Special values to check.
		inputs.push_back(tcu::Vec2(0.0f, 0.0f));
		inputs.push_back(tcu::Vec2(0.5f, 1.0f));
		inputs.push_back(tcu::Vec2(1.0f, 0.5f));
		inputs.push_back(tcu::Vec2(-0.5f, 1.5f));
		inputs.push_back(tcu::Vec2(0.25f, 0.75f));

		// Random values.
		{
			const int	minExp	= -14;
			const int	maxExp	= 15;

			for (int ndx = 0; ndx < 95; ndx++)
			{
				tcu::Vec2 v;
				for (int c = 0; c < 2; c++)
				{
					const int		s			= rnd.getBool() ? 1 : -1;
					const int		exp			= rnd.getInt(minExp, maxExp);
					const deUint32	mantissa	= rnd.getUint32() & ((1<<23)-1);

					v[c] = tcu::Float32::construct(s, exp ? exp : 1 /* avoid denormals */, (1u<<23) | mantissa).asFloat();
				}
				inputs.push_back(v);
			}
		}

		// Convert input values to fp16 and back to make sure they can be represented exactly in mediump.
		for (std::vector<tcu::Vec2>::iterator inVal = inputs.begin(); inVal != inputs.end(); ++inVal)
			*inVal = tcu::Vec2(tcu::Float16(inVal->x()).asFloat(), tcu::Float16(inVal->y()).asFloat());

		outputs.resize(inputs.size());

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
			{
				const deUint16	ref0	= (deUint16)tcu::Float16(inputs[valNdx].x()).bits();
				const deUint16	ref1	= (deUint16)tcu::Float16(inputs[valNdx].y()).bits();
				const deUint32	ref		= (ref1 << 16) | ref0;
				const deUint32	res		= outputs[valNdx];
				const deUint16	res0	= (deUint16)(res & 0xffff);
				const deUint16	res1	= (deUint16)(res >> 16);
				const int		diff0	= de::abs((int)ref0 - (int)res0);
				const int		diff1	= de::abs((int)ref1 - (int)res1);

				if (diff0 > maxDiff || diff1 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx
															   << ", expected packHalf2x16(" << inputs[valNdx] << ") = " << tcu::toHex(ref)
															   << ", got " << tcu::toHex(res)
															   << "\n  diffs = (" << diff0 << ", " << diff1 << "), max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	PackHalf2x16Case (tcu::TestContext& testCtx, glu::ShaderType shaderType)
		: ShaderPackingFunctionCase	(testCtx, (string("packhalf2x16") + getShaderTypePostfix(shaderType)).c_str(), "packHalf2x16", shaderType)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_HIGHP)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = packHalf2x16(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new PackHalf2x16CaseInstance(ctx, m_shaderType, m_spec, getName());
	}

};

class UnpackHalf2x16CaseInstance : public ShaderPackingFunctionTestInstance
{
	enum Sign
	{
		POSITIVE								= 0,
		NEGATIVE
	};
	enum SubnormalizedConversionType
	{
		UNKNOWN									= 0,
		CONVERTED,
		ZERO_FLUSHED,
	};
public:
	UnpackHalf2x16CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, const char* name)
	: ShaderPackingFunctionTestInstance (context, shaderType, spec, name)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const int					minExp		= -14;
		const int					maxExp		= 15;
		const int					mantBits	= 10;
		const deUint32				mantBitMask = (1u << mantBits) - 1u;
		tcu::TestLog&				log			= m_testCtx.getLog();

		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<deUint32>		inputs;
		std::vector<tcu::Vec2>		outputs;

		// Special values.
		inputs.push_back((tcu::Float16( 0.0f).bits() << 16) | tcu::Float16( 1.0f).bits());
		inputs.push_back((tcu::Float16( 1.0f).bits() << 16) | tcu::Float16( 0.0f).bits());
		inputs.push_back((tcu::Float16(-1.0f).bits() << 16) | tcu::Float16( 0.5f).bits());
		inputs.push_back((tcu::Float16( 0.5f).bits() << 16) | tcu::Float16(-0.5f).bits());
		// Special subnormal value: single lowest bit set
		inputs.push_back((tcu::Float16(composeHalfFloat(POSITIVE, 0u, 1u)).bits() << 16)
						| tcu::Float16(composeHalfFloat(NEGATIVE, 0u, 1u)).bits());
		// Special subnormal value: single highest fraction bit set
		inputs.push_back((tcu::Float16(composeHalfFloat(NEGATIVE, 0u, 1u << (mantBits - 1u))).bits() << 16)
						| tcu::Float16(composeHalfFloat(POSITIVE, 0u, 1u << (mantBits - 1u))).bits());
		// Special subnormal value: all fraction bits set
		inputs.push_back((tcu::Float16(composeHalfFloat(POSITIVE, 0u, mantBitMask)).bits() << 16)
						| tcu::Float16(composeHalfFloat(NEGATIVE, 0u, mantBitMask)).bits());

		// Construct random values.
		for (int ndx = 0; ndx < 90; ndx++)
		{
			deUint32 inVal = 0;
			for (int c = 0; c < 2; c++)
			{
				const int		s			= rnd.getBool() ? 1 : -1;
				const int		exp			= rnd.getInt(minExp, maxExp);
				const deUint32	mantissa	= rnd.getUint32() & mantBitMask;
				const deUint16	value		= tcu::Float16::construct(s, exp != 0 ? exp : 1 /* avoid denorm */, static_cast<deUint16>((1u<<10) | mantissa)).bits();

				inVal |= value << (16u * c);
			}
			inputs.push_back(inVal);
		}
		for (int ndx = 0; ndx < 15; ndx++)
		{
			deUint32 inVal = 0;
			for (int c = 0; c < 2; c++)
			{
				const Sign		sign		= rnd.getBool()? POSITIVE : NEGATIVE;
				const deUint32	mantissa	= rnd.getUint32() & mantBitMask;
				const deUint16	value		= tcu::Float16(composeHalfFloat(sign, 0u /* force denorm */, mantissa)).bits();

				inVal |= value << (16u * c);
			}
			inputs.push_back(inVal);
		}

		outputs.resize(inputs.size());

		log << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int					numValues	= (int)inputs.size();
			const int					maxPrints	= 10;
			int							numFailed	= 0;
			SubnormalizedConversionType conversion	= UNKNOWN;

			for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
			{
				const deUint16			in0			= (deUint16)(inputs[valNdx] & 0xffff);
				const deUint16			in1			= (deUint16)(inputs[valNdx] >> 16);
				const float				res0		= outputs[valNdx].x();
				const float				res1		= outputs[valNdx].y();

				const deBool			value0		= checkValue(in0, res0, conversion);
				// note: do not avoid calling checkValue for in1 if it failed for in0 by using && laziness
				// checkValue may potentially change 'conversion' parameter if it was set to UNKNOWN so far
				const deBool			value1		= checkValue(in1, res1, conversion);
				const deBool			valuesOK	= value0 && value1;

				if (!valuesOK)
				{
					if (numFailed < maxPrints)
						printErrorMessage(log, valNdx, in0, in1, res0, res1);
					else if (numFailed == maxPrints)
						log << TestLog::Message << "..." << TestLog::EndMessage;
					++numFailed;
				}
			}

			log << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

			if (numFailed == 0)
				return tcu::TestStatus::pass("Pass");
			else
				return tcu::TestStatus::fail("Result comparison failed");
		}
	}
private:
	deBool checkValue (deUint16 inValue, float outValue, SubnormalizedConversionType& conversion)
	{
		const tcu::Float16		temp			= tcu::Float16(inValue);
		const float				ref				= temp.asFloat();
		const deUint32			refBits			= tcu::Float32(ref).bits();
		const deUint32			resBits			= tcu::Float32(outValue).bits();
		const deBool			bitMatch		= (refBits ^ resBits) == 0u;
		const deBool			denorm			= temp.isDenorm();

		if (conversion != CONVERTED && denorm)
		{
			if (resBits == 0 || (ref < 0 && resBits == 0x80000000UL))
			{
				conversion = ZERO_FLUSHED;
				return DE_TRUE;
			}
			if (conversion != ZERO_FLUSHED && bitMatch)
			{
				conversion = CONVERTED;
				return DE_TRUE;
			}
			return DE_FALSE;
		}
		else if (bitMatch)
			return DE_TRUE;
		return DE_FALSE;
	}
	void printErrorMessage (tcu::TestLog& log, deUint32 valNdx, deUint16 in0, deUint16 in1, float out0, float out1)
	{
		const float			ref0		= tcu::Float16(in0).asFloat();
		const deUint32		refBits0	= tcu::Float32(ref0).bits();
		const deUint32		resBits0	= tcu::Float32(out0).bits();
		const float			ref1		= tcu::Float16(in1).asFloat();
		const deUint32		refBits1	= tcu::Float32(ref1).bits();
		const deUint32		resBits1	= tcu::Float32(out1).bits();
		log << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
			<< "  expected unpackHalf2x16(" << tcu::toHex((in1 << 16u) | in0) << ") = "
			<< "vec2(" << ref0 << " / " << tcu::toHex(refBits0) << ", " << ref1 << " / " << tcu::toHex(refBits1) << ")"
			<< ", got vec2(" << out0 << " / " << tcu::toHex(resBits0) << ", " << out1 << " / " << tcu::toHex(resBits1) << ")"
			<< TestLog::EndMessage;
	}
	deUint16 composeHalfFloat (Sign sign, deUint32 exponent, deUint32 significand)
	{
		const deUint32		BitMask_05	= (1u << 5u)  - 1u;
		const deUint32		BitMask_10	= (1u << 10u) - 1u;
		const deUint32		BitMask_16	= (1u << 16u) - 1u;
		DE_UNREF(BitMask_05);
		DE_UNREF(BitMask_10);
		DE_UNREF(BitMask_16);
		DE_ASSERT((exponent & ~BitMask_05) == 0u);
		DE_ASSERT((significand & ~BitMask_10) == 0u);
		const deUint32		value		= (((sign == NEGATIVE ? 1u : 0u) << 5u | exponent) << 10u) | significand;
		DE_ASSERT((value & ~BitMask_16) == 0u);
		return static_cast<deUint16>(value);
	}
};

class UnpackHalf2x16Case : public ShaderPackingFunctionCase
{
public:
	UnpackHalf2x16Case (tcu::TestContext& testCtx, glu::ShaderType shaderType)
		: ShaderPackingFunctionCase	(testCtx, (string("unpackhalf2x16") + getShaderTypePostfix(shaderType)).c_str(), "unpackHalf2x16", shaderType)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC2, glu::PRECISION_MEDIUMP)));

		m_spec.source = "out0 = unpackHalf2x16(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new UnpackHalf2x16CaseInstance(ctx, m_shaderType, m_spec, getName());
	}

};

class PackSnorm4x8CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
	PackSnorm4x8CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, glu::Precision precision, const char* name)
		: ShaderPackingFunctionTestInstance	(context, shaderType, spec, name)
		, m_precision						(precision)
	{
	}

	tcu::TestStatus iterate (void)
	{
		de::Random					rnd			(deStringHash(m_name) ^ 0x42f2c0);
		std::vector<tcu::Vec4>		inputs;
		std::vector<deUint32>		outputs;
		const int					maxDiff		= m_precision == glu::PRECISION_HIGHP	? 1	:		// Rounding only.
												  m_precision == glu::PRECISION_MEDIUMP	? 1	:		// (2^-10) * (2^7) + 1
												  m_precision == glu::PRECISION_LOWP	? 2	: 0;	// (2^-8) * (2^7) + 1

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

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < numValues; valNdx++)
			{
				const deUint16	ref0	= (deUint8)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].x(), -1.0f, 1.0f) * 127.0f), -(1<<7), (1<<7)-1);
				const deUint16	ref1	= (deUint8)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].y(), -1.0f, 1.0f) * 127.0f), -(1<<7), (1<<7)-1);
				const deUint16	ref2	= (deUint8)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].z(), -1.0f, 1.0f) * 127.0f), -(1<<7), (1<<7)-1);
				const deUint16	ref3	= (deUint8)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].w(), -1.0f, 1.0f) * 127.0f), -(1<<7), (1<<7)-1);
				const deUint32	ref		= (deUint32(ref3) << 24) | (deUint32(ref2) << 16) | (deUint32(ref1) << 8) | deUint32(ref0);
				const deUint32	res		= outputs[valNdx];
				const deUint16	res0	= (deUint8)(res & 0xff);
				const deUint16	res1	= (deUint8)((res >> 8) & 0xff);
				const deUint16	res2	= (deUint8)((res >> 16) & 0xff);
				const deUint16	res3	= (deUint8)((res >> 24) & 0xff);
				const int		diff0	= de::abs((int)ref0 - (int)res0);
				const int		diff1	= de::abs((int)ref1 - (int)res1);
				const int		diff2	= de::abs((int)ref2 - (int)res2);
				const int		diff3	= de::abs((int)ref3 - (int)res3);

				if (diff0 > maxDiff || diff1 > maxDiff || diff2 > maxDiff || diff3 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx
															   << ", expected packSnorm4x8(" << inputs[valNdx] << ") = " << tcu::toHex(ref)
															   << ", got " << tcu::toHex(res)
															   << "\n  diffs = " << tcu::IVec4(diff0, diff1, diff2, diff3) << ", max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	PackSnorm4x8Case (tcu::TestContext& testCtx, glu::ShaderType shaderType, glu::Precision precision)
		: ShaderPackingFunctionCase	(testCtx, (string("packsnorm4x8") + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType)).c_str(), "packSnorm4x8", shaderType)
		, m_precision				(precision)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC4, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = packSnorm4x8(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new PackSnorm4x8CaseInstance(ctx, m_shaderType, m_spec, m_precision, getName());
	}

private:
	const glu::Precision m_precision;
};

class UnpackSnorm4x8CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
	UnpackSnorm4x8CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, const char* name)
		: ShaderPackingFunctionTestInstance	(context, shaderType, spec, name)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const deUint32				maxDiff		= 1; // Rounding error.
		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<deUint32>		inputs;
		std::vector<tcu::Vec4>		outputs;

		inputs.push_back(0x00000000u);
		inputs.push_back(0x7fff8000u);
		inputs.push_back(0x80007fffu);
		inputs.push_back(0xffffffffu);
		inputs.push_back(0x0001fffeu);

		// Random values.
		for (int ndx = 0; ndx < 95; ndx++)
			inputs.push_back(rnd.getUint32());

		outputs.resize(inputs.size());

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
			{
				const deInt8	in0		= (deInt8)(deUint8)(inputs[valNdx] & 0xff);
				const deInt8	in1		= (deInt8)(deUint8)((inputs[valNdx] >> 8) & 0xff);
				const deInt8	in2		= (deInt8)(deUint8)((inputs[valNdx] >> 16) & 0xff);
				const deInt8	in3		= (deInt8)(deUint8)(inputs[valNdx] >> 24);
				const float		ref0	= de::clamp(float(in0) / 127.f, -1.0f, 1.0f);
				const float		ref1	= de::clamp(float(in1) / 127.f, -1.0f, 1.0f);
				const float		ref2	= de::clamp(float(in2) / 127.f, -1.0f, 1.0f);
				const float		ref3	= de::clamp(float(in3) / 127.f, -1.0f, 1.0f);
				const float		res0	= outputs[valNdx].x();
				const float		res1	= outputs[valNdx].y();
				const float		res2	= outputs[valNdx].z();
				const float		res3	= outputs[valNdx].w();

				const deUint32	diff0	= getUlpDiff(ref0, res0);
				const deUint32	diff1	= getUlpDiff(ref1, res1);
				const deUint32	diff2	= getUlpDiff(ref2, res2);
				const deUint32	diff3	= getUlpDiff(ref3, res3);

				if (diff0 > maxDiff || diff1 > maxDiff || diff2 > maxDiff || diff3 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
															   << "  expected unpackSnorm4x8(" << tcu::toHex(inputs[valNdx]) << ") = "
															   << "vec4(" << HexFloat(ref0) << ", " << HexFloat(ref1) << ", " << HexFloat(ref2) << ", " << HexFloat(ref3) << ")"
															   << ", got vec4(" << HexFloat(res0) << ", " << HexFloat(res1) << ", " << HexFloat(res2) << ", " << HexFloat(res3) << ")"
															   << "\n  ULP diffs = (" << diff0 << ", " << diff1 << ", " << diff2 << ", " << diff3 << "), max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	UnpackSnorm4x8Case (tcu::TestContext& testCtx, glu::ShaderType shaderType)
		: ShaderPackingFunctionCase	(testCtx, (string("unpacksnorm4x8") + getShaderTypePostfix(shaderType)).c_str(), "unpackSnorm4x8", shaderType)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = unpackSnorm4x8(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new UnpackSnorm4x8CaseInstance(ctx, m_shaderType, m_spec, getName());
	}

};

class PackUnorm4x8CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
	PackUnorm4x8CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, glu::Precision precision, const char* name)
		: ShaderPackingFunctionTestInstance	(context, shaderType, spec, name)
		, m_precision						(precision)
	{
	}

	tcu::TestStatus iterate (void)
	{
		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<tcu::Vec4>		inputs;
		std::vector<deUint32>		outputs;
		const int					maxDiff		= m_precision == glu::PRECISION_HIGHP	? 1	:		// Rounding only.
												  m_precision == glu::PRECISION_MEDIUMP	? 1	:		// (2^-10) * (2^8) + 1
												  m_precision == glu::PRECISION_LOWP	? 2	: 0;	// (2^-8) * (2^8) + 1

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

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
			{
				const deUint16	ref0	= (deUint8)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].x(), 0.0f, 1.0f) * 255.0f), 0, (1<<8)-1);
				const deUint16	ref1	= (deUint8)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].y(), 0.0f, 1.0f) * 255.0f), 0, (1<<8)-1);
				const deUint16	ref2	= (deUint8)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].z(), 0.0f, 1.0f) * 255.0f), 0, (1<<8)-1);
				const deUint16	ref3	= (deUint8)de::clamp(deRoundFloatToInt32(de::clamp(inputs[valNdx].w(), 0.0f, 1.0f) * 255.0f), 0, (1<<8)-1);
				const deUint32	ref		= (deUint32(ref3) << 24) | (deUint32(ref2) << 16) | (deUint32(ref1) << 8) | deUint32(ref0);
				const deUint32	res		= outputs[valNdx];
				const deUint16	res0	= (deUint8)(res & 0xff);
				const deUint16	res1	= (deUint8)((res >> 8) & 0xff);
				const deUint16	res2	= (deUint8)((res >> 16) & 0xff);
				const deUint16	res3	= (deUint8)((res >> 24) & 0xff);
				const int		diff0	= de::abs((int)ref0 - (int)res0);
				const int		diff1	= de::abs((int)ref1 - (int)res1);
				const int		diff2	= de::abs((int)ref2 - (int)res2);
				const int		diff3	= de::abs((int)ref3 - (int)res3);

				if (diff0 > maxDiff || diff1 > maxDiff || diff2 > maxDiff || diff3 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx
															   << ", expected packUnorm4x8(" << inputs[valNdx] << ") = " << tcu::toHex(ref)
															   << ", got " << tcu::toHex(res)
															   << "\n  diffs = " << tcu::IVec4(diff0, diff1, diff2, diff3) << ", max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	PackUnorm4x8Case (tcu::TestContext& testCtx, glu::ShaderType shaderType, glu::Precision precision)
		: ShaderPackingFunctionCase	(testCtx, (string("packunorm4x8") + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType)).c_str(), "packUnorm4x8", shaderType)
		, m_precision				(precision)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_FLOAT_VEC4, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = packUnorm4x8(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new PackUnorm4x8CaseInstance(ctx, m_shaderType, m_spec, m_precision, getName());
	}

private:
	const glu::Precision m_precision;
};

class UnpackUnorm4x8CaseInstance : public ShaderPackingFunctionTestInstance
{
public:
	UnpackUnorm4x8CaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, const char* name)
		: ShaderPackingFunctionTestInstance	(context, shaderType, spec, name)
	{
	}

	tcu::TestStatus iterate (void)
	{
		const deUint32				maxDiff		= 1; // Rounding error.
		de::Random					rnd			(deStringHash(m_name) ^ 0x776002);
		std::vector<deUint32>		inputs;
		std::vector<tcu::Vec4>		outputs;

		inputs.push_back(0x00000000u);
		inputs.push_back(0x7fff8000u);
		inputs.push_back(0x80007fffu);
		inputs.push_back(0xffffffffu);
		inputs.push_back(0x0001fffeu);

		// Random values.
		for (int ndx = 0; ndx < 95; ndx++)
			inputs.push_back(rnd.getUint32());

		outputs.resize(inputs.size());

		m_testCtx.getLog() << TestLog::Message << "Executing shader for " << inputs.size() << " input values" << tcu::TestLog::EndMessage;

		{
			const void*	in	= &inputs[0];
			void*		out	= &outputs[0];

			m_executor->execute((int)inputs.size(), &in, &out);
		}

		// Verify
		{
			const int	numValues	= (int)inputs.size();
			const int	maxPrints	= 10;
			int			numFailed	= 0;

			for (int valNdx = 0; valNdx < (int)inputs.size(); valNdx++)
			{
				const deUint8	in0		= (deUint8)(inputs[valNdx] & 0xff);
				const deUint8	in1		= (deUint8)((inputs[valNdx] >> 8) & 0xff);
				const deUint8	in2		= (deUint8)((inputs[valNdx] >> 16) & 0xff);
				const deUint8	in3		= (deUint8)(inputs[valNdx] >> 24);
				const float		ref0	= de::clamp(float(in0) / 255.f, 0.0f, 1.0f);
				const float		ref1	= de::clamp(float(in1) / 255.f, 0.0f, 1.0f);
				const float		ref2	= de::clamp(float(in2) / 255.f, 0.0f, 1.0f);
				const float		ref3	= de::clamp(float(in3) / 255.f, 0.0f, 1.0f);
				const float		res0	= outputs[valNdx].x();
				const float		res1	= outputs[valNdx].y();
				const float		res2	= outputs[valNdx].z();
				const float		res3	= outputs[valNdx].w();

				const deUint32	diff0	= getUlpDiff(ref0, res0);
				const deUint32	diff1	= getUlpDiff(ref1, res1);
				const deUint32	diff2	= getUlpDiff(ref2, res2);
				const deUint32	diff3	= getUlpDiff(ref3, res3);

				if (diff0 > maxDiff || diff1 > maxDiff || diff2 > maxDiff || diff3 > maxDiff)
				{
					if (numFailed < maxPrints)
					{
						m_testCtx.getLog() << TestLog::Message << "ERROR: Mismatch in value " << valNdx << ",\n"
															   << "  expected unpackUnorm4x8(" << tcu::toHex(inputs[valNdx]) << ") = "
															   << "vec4(" << HexFloat(ref0) << ", " << HexFloat(ref1) << ", " << HexFloat(ref2) << ", " << HexFloat(ref3) << ")"
															   << ", got vec4(" << HexFloat(res0) << ", " << HexFloat(res1) << ", " << HexFloat(res2) << ", " << HexFloat(res3) << ")"
															   << "\n  ULP diffs = (" << diff0 << ", " << diff1 << ", " << diff2 << ", " << diff3 << "), max diff = " << maxDiff
										   << TestLog::EndMessage;
					}
					else if (numFailed == maxPrints)
						m_testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

					numFailed += 1;
				}
			}

			m_testCtx.getLog() << TestLog::Message << (numValues - numFailed) << " / " << numValues << " values passed" << TestLog::EndMessage;

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
	UnpackUnorm4x8Case (tcu::TestContext& testCtx, glu::ShaderType shaderType)
		: ShaderPackingFunctionCase	(testCtx, (string("unpackunorm4x8") + getShaderTypePostfix(shaderType)).c_str(), "unpackUnorm4x8", shaderType)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));

		m_spec.source = "out0 = unpackUnorm4x8(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new UnpackUnorm4x8CaseInstance(ctx, m_shaderType, m_spec, getName());
	}

};

ShaderPackingFunctionTests::ShaderPackingFunctionTests (tcu::TestContext& testCtx)
	: tcu::TestCaseGroup	(testCtx, "pack_unpack", "Floating-point pack and unpack function tests")
{
}

ShaderPackingFunctionTests::~ShaderPackingFunctionTests (void)
{
}

void ShaderPackingFunctionTests::init (void)
{
	// New built-in functions in GLES 3.1
	{
		const glu::ShaderType allShaderTypes[] =
		{
			glu::SHADERTYPE_VERTEX,
			glu::SHADERTYPE_TESSELLATION_CONTROL,
			glu::SHADERTYPE_TESSELLATION_EVALUATION,
			glu::SHADERTYPE_GEOMETRY,
			glu::SHADERTYPE_FRAGMENT,
			glu::SHADERTYPE_COMPUTE
		};

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
		const glu::ShaderType newShaderTypes[] =
		{
			glu::SHADERTYPE_GEOMETRY,
			glu::SHADERTYPE_COMPUTE
		};

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

} // shaderexecutor
} // vkt
