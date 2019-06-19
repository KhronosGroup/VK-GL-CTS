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
 * \brief Common built-in function tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderCommonFunctionTests.hpp"
#include "vktShaderExecutor.hpp"
#include "gluContextInfo.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuInterval.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuVectorUtil.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"
#include "deArrayUtil.hpp"
#include "deSharedPtr.hpp"

namespace vkt
{

namespace shaderexecutor
{


using std::vector;
using std::string;
using tcu::TestLog;

using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;

namespace
{

// Utilities

template<typename T, int Size>
struct VecArrayAccess
{
public:
									VecArrayAccess	(const void* ptr) : m_array((tcu::Vector<T, Size>*)ptr) {}
									~VecArrayAccess	(void) {}

	const tcu::Vector<T, Size>&		operator[]		(size_t offset) const	{ return m_array[offset];	}
	tcu::Vector<T, Size>&			operator[]		(size_t offset)			{ return m_array[offset];	}

private:
	tcu::Vector<T, Size>*			m_array;
};

template<typename T, int Size>
static void fillRandomVectors (de::Random& rnd, const tcu::Vector<T, Size>& minValue, const tcu::Vector<T, Size>& maxValue, void* dst, int numValues, int offset = 0)
{
	VecArrayAccess<T, Size> access(dst);
	for (int ndx = 0; ndx < numValues; ndx++)
		access[offset + ndx] = tcu::randomVector<T, Size>(rnd, minValue, maxValue);
}

template<typename T>
static void fillRandomScalars (de::Random& rnd, T minValue, T maxValue, void* dst, int numValues, int offset = 0)
{
	T* typedPtr = (T*)dst;
	for (int ndx = 0; ndx < numValues; ndx++)
		typedPtr[offset + ndx] = de::randomScalar<T>(rnd, minValue, maxValue);
}

inline deUint32 getUlpDiff (float a, float b)
{
	const deUint32	aBits	= tcu::Float32(a).bits();
	const deUint32	bBits	= tcu::Float32(b).bits();
	return aBits > bBits ? aBits - bBits : bBits - aBits;
}

inline deUint32 getUlpDiffIgnoreZeroSign (float a, float b)
{
	if (tcu::Float32(a).isZero())
		return getUlpDiff(tcu::Float32::construct(tcu::Float32(b).sign(), 0, 0).asFloat(), b);
	else if (tcu::Float32(b).isZero())
		return getUlpDiff(a, tcu::Float32::construct(tcu::Float32(a).sign(), 0, 0).asFloat());
	else
		return getUlpDiff(a, b);
}

inline deUint32 getMaxUlpDiffFromBits (int numAccurateBits)
{
	const int		numGarbageBits	= 23-numAccurateBits;
	const deUint32	mask			= (1u<<numGarbageBits)-1u;

	return mask;
}

static int getMinMantissaBits (glu::Precision precision)
{
	const int bits[] =
	{
		7,		// lowp
		10,		// mediump
		23		// highp
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(bits) == glu::PRECISION_LAST);
	DE_ASSERT(de::inBounds<int>(precision, 0, DE_LENGTH_OF_ARRAY(bits)));
	return bits[precision];
}

static vector<int> getScalarSizes (const vector<Symbol>& symbols)
{
	vector<int> sizes(symbols.size());
	for (int ndx = 0; ndx < (int)symbols.size(); ++ndx)
		sizes[ndx] = symbols[ndx].varType.getScalarSize();
	return sizes;
}

static int computeTotalScalarSize (const vector<Symbol>& symbols)
{
	int totalSize = 0;
	for (vector<Symbol>::const_iterator sym = symbols.begin(); sym != symbols.end(); ++sym)
		totalSize += sym->varType.getScalarSize();
	return totalSize;
}

static vector<void*> getInputOutputPointers (const vector<Symbol>& symbols, vector<deUint32>& data, const int numValues)
{
	vector<void*>	pointers		(symbols.size());
	int				curScalarOffset	= 0;

	for (int varNdx = 0; varNdx < (int)symbols.size(); ++varNdx)
	{
		const Symbol&	var				= symbols[varNdx];
		const int		scalarSize		= var.varType.getScalarSize();

		// Uses planar layout as input/output specs do not support strides.
		pointers[varNdx] = &data[curScalarOffset];
		curScalarOffset += scalarSize*numValues;
	}

	DE_ASSERT(curScalarOffset == (int)data.size());

	return pointers;
}

// \todo [2013-08-08 pyry] Make generic utility and move to glu?

struct HexFloat
{
	const float value;
	HexFloat (const float value_) : value(value_) {}
};

std::ostream& operator<< (std::ostream& str, const HexFloat& v)
{
	return str << v.value << " / " << tcu::toHex(tcu::Float32(v.value).bits());
}

struct HexBool
{
	const deUint32 value;
	HexBool (const deUint32 value_) : value(value_) {}
};

std::ostream& operator<< (std::ostream& str, const HexBool& v)
{
	return str << (v.value ? "true" : "false") << " / " << tcu::toHex(v.value);
}

struct VarValue
{
	const glu::VarType&	type;
	const void*			value;

	VarValue (const glu::VarType& type_, const void* value_) : type(type_), value(value_) {}
};

std::ostream& operator<< (std::ostream& str, const VarValue& varValue)
{
	DE_ASSERT(varValue.type.isBasicType());

	const glu::DataType		basicType		= varValue.type.getBasicType();
	const glu::DataType		scalarType		= glu::getDataTypeScalarType(basicType);
	const int				numComponents	= glu::getDataTypeScalarSize(basicType);

	if (numComponents > 1)
		str << glu::getDataTypeName(basicType) << "(";

	for (int compNdx = 0; compNdx < numComponents; compNdx++)
	{
		if (compNdx != 0)
			str << ", ";

		switch (scalarType)
		{
			case glu::TYPE_FLOAT:	str << HexFloat(((const float*)varValue.value)[compNdx]);			break;
			case glu::TYPE_INT:		str << ((const deInt32*)varValue.value)[compNdx];					break;
			case glu::TYPE_UINT:	str << tcu::toHex(((const deUint32*)varValue.value)[compNdx]);		break;
			case glu::TYPE_BOOL:	str << HexBool(((const deUint32*)varValue.value)[compNdx]);			break;

			default:
				DE_ASSERT(false);
		}
	}

	if (numComponents > 1)
		str << ")";

	return str;
}

static std::string getCommonFuncCaseName (glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
{
	return string(glu::getDataTypeName(baseType)) + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType);
}

template<class TestClass>
static void addFunctionCases (tcu::TestCaseGroup* parent, const char* functionName, glu::DataType scalarType, deUint32 shaderBits)
{
	tcu::TestCaseGroup* group = new tcu::TestCaseGroup(parent->getTestContext(), functionName, functionName);
	parent->addChild(group);

	for (int vecSize = 1; vecSize <= 4; vecSize++)
	{
		for (int prec = glu::PRECISION_MEDIUMP; prec <= glu::PRECISION_HIGHP; prec++)
		{
			for (int shaderTypeNdx = 0; shaderTypeNdx < glu::SHADERTYPE_LAST; shaderTypeNdx++)
			{
				if (shaderBits & (1<<shaderTypeNdx))
					group->addChild(new TestClass(parent->getTestContext(), glu::DataType(scalarType + vecSize - 1), glu::Precision(prec), glu::ShaderType(shaderTypeNdx)));
			}
		}
	}
}

// CommonFunctionCase

class CommonFunctionCase : public TestCase
{
public:
										CommonFunctionCase			(tcu::TestContext& testCtx, const char* name, const char* description, glu::ShaderType shaderType);
										~CommonFunctionCase			(void);
	virtual	void						initPrograms				(vk::SourceCollections& programCollection) const
										{
											generateSources(m_shaderType, m_spec, programCollection);
										}

	virtual TestInstance*				createInstance				(Context& context) const = 0;

protected:
										CommonFunctionCase			(const CommonFunctionCase&);
	CommonFunctionCase&					operator=					(const CommonFunctionCase&);

	const glu::ShaderType				m_shaderType;
	ShaderSpec							m_spec;
	const int							m_numValues;
};

CommonFunctionCase::CommonFunctionCase (tcu::TestContext& testCtx, const char* name, const char* description, glu::ShaderType shaderType)
	: TestCase		(testCtx, name, description)
	, m_shaderType	(shaderType)
	, m_numValues	(100)
{
}

CommonFunctionCase::~CommonFunctionCase (void)
{
}

// CommonFunctionTestInstance

class CommonFunctionTestInstance : public TestInstance
{
public:
										CommonFunctionTestInstance	(Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, int numValues, const char* name)
											: TestInstance	(context)
											, m_shaderType	(shaderType)
											, m_spec		(spec)
											, m_numValues	(numValues)
											, m_name		(name)
											, m_executor	(createExecutor(context, shaderType, spec))
										{
										}
	virtual tcu::TestStatus				iterate						(void);

protected:
	virtual void						getInputValues				(int numValues, void* const* values) const = 0;
	virtual bool						compare						(const void* const* inputs, const void* const* outputs) = 0;

	const glu::ShaderType				m_shaderType;
	const ShaderSpec					m_spec;
	const int							m_numValues;

	// \todo [2017-03-07 pyry] Hack used to generate seeds for test cases - get rid of this.
	const char*							m_name;

	std::ostringstream					m_failMsg;					//!< Comparison failure help message.

	de::UniquePtr<ShaderExecutor>		m_executor;
};

tcu::TestStatus CommonFunctionTestInstance::iterate (void)
{
	const int				numInputScalars			= computeTotalScalarSize(m_spec.inputs);
	const int				numOutputScalars		= computeTotalScalarSize(m_spec.outputs);
	vector<deUint32>		inputData				(numInputScalars * m_numValues);
	vector<deUint32>		outputData				(numOutputScalars * m_numValues);
	const vector<void*>		inputPointers			= getInputOutputPointers(m_spec.inputs, inputData, m_numValues);
	const vector<void*>		outputPointers			= getInputOutputPointers(m_spec.outputs, outputData, m_numValues);

	// Initialize input data.
	getInputValues(m_numValues, &inputPointers[0]);

	// Execute shader.
	m_executor->execute(m_numValues, &inputPointers[0], &outputPointers[0]);

	// Compare results.
	{
		const vector<int>		inScalarSizes		= getScalarSizes(m_spec.inputs);
		const vector<int>		outScalarSizes		= getScalarSizes(m_spec.outputs);
		vector<void*>			curInputPtr			(inputPointers.size());
		vector<void*>			curOutputPtr		(outputPointers.size());
		int						numFailed			= 0;
		tcu::TestContext&		testCtx				= m_context.getTestContext();

		for (int valNdx = 0; valNdx < m_numValues; valNdx++)
		{
			// Set up pointers for comparison.
			for (int inNdx = 0; inNdx < (int)curInputPtr.size(); ++inNdx)
				curInputPtr[inNdx] = (deUint32*)inputPointers[inNdx] + inScalarSizes[inNdx]*valNdx;

			for (int outNdx = 0; outNdx < (int)curOutputPtr.size(); ++outNdx)
				curOutputPtr[outNdx] = (deUint32*)outputPointers[outNdx] + outScalarSizes[outNdx]*valNdx;

			if (!compare(&curInputPtr[0], &curOutputPtr[0]))
			{
				// \todo [2013-08-08 pyry] We probably want to log reference value as well?

				testCtx.getLog() << TestLog::Message << "ERROR: comparison failed for value " << valNdx << ":\n  " << m_failMsg.str() << TestLog::EndMessage;

				testCtx.getLog() << TestLog::Message << "  inputs:" << TestLog::EndMessage;
				for (int inNdx = 0; inNdx < (int)curInputPtr.size(); inNdx++)
					testCtx.getLog() << TestLog::Message << "    " << m_spec.inputs[inNdx].name << " = "
														   << VarValue(m_spec.inputs[inNdx].varType, curInputPtr[inNdx])
									   << TestLog::EndMessage;

				testCtx.getLog() << TestLog::Message << "  outputs:" << TestLog::EndMessage;
				for (int outNdx = 0; outNdx < (int)curOutputPtr.size(); outNdx++)
					testCtx.getLog() << TestLog::Message << "    " << m_spec.outputs[outNdx].name << " = "
														   << VarValue(m_spec.outputs[outNdx].varType, curOutputPtr[outNdx])
									   << TestLog::EndMessage;

				m_failMsg.str("");
				m_failMsg.clear();
				numFailed += 1;
			}
		}

		testCtx.getLog() << TestLog::Message << (m_numValues - numFailed) << " / " << m_numValues << " values passed" << TestLog::EndMessage;

		if (numFailed == 0)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Result comparison failed");
	}
}

// Test cases

class AbsCaseInstance : public CommonFunctionTestInstance
{
public:
	AbsCaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, shaderType, spec, numValues, name)
	{
	}

	void getInputValues (int numValues, void* const* values) const
	{
		const IVec2 intRanges[] =
		{
			IVec2(-(1<<7)+1,	(1<<7)-1),
			IVec2(-(1<<15)+1,	(1<<15)-1),
			IVec2(0x80000001,	0x7fffffff)
		};

		de::Random				rnd			(deStringHash(m_name) ^ 0x235facu);
		const glu::DataType		type		= m_spec.inputs[0].varType.getBasicType();
		const glu::Precision	precision	= m_spec.inputs[0].varType.getPrecision();
		const int				scalarSize	= glu::getDataTypeScalarSize(type);

		DE_ASSERT(!glu::isDataTypeFloatOrVec(type));

		fillRandomScalars(rnd, intRanges[precision].x(), intRanges[precision].y(), values[0], numValues*scalarSize);
	}

	bool compare (const void* const* inputs, const void* const* outputs)
	{
		const glu::DataType		type			= m_spec.inputs[0].varType.getBasicType();
		const int				scalarSize		= glu::getDataTypeScalarSize(type);

		DE_ASSERT(!glu::isDataTypeFloatOrVec(type));

		for (int compNdx = 0; compNdx < scalarSize; compNdx++)
		{
			const int	in0		= ((const int*)inputs[0])[compNdx];
			const int	out0	= ((const int*)outputs[0])[compNdx];
			const int	ref0	= de::abs(in0);

			if (out0 != ref0)
			{
				m_failMsg << "Expected [" << compNdx << "] = " << ref0;
				return false;
			}
		}

		return true;
	}
};

class AbsCase : public CommonFunctionCase
{
public:
	AbsCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "abs", shaderType)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
		m_spec.source = "out0 = abs(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new AbsCaseInstance(ctx, m_shaderType, m_spec, m_numValues, getName());
	}
};

class SignCaseInstance : public CommonFunctionTestInstance
{
public:
	SignCaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, shaderType, spec, numValues, name)
	{
	}

	void getInputValues (int numValues, void* const* values) const
	{
		const IVec2 intRanges[] =
		{
			IVec2(-(1<<7),		(1<<7)-1),
			IVec2(-(1<<15),		(1<<15)-1),
			IVec2(0x80000000,	0x7fffffff)
		};

		de::Random				rnd			(deStringHash(m_name) ^ 0x324u);
		const glu::DataType		type		= m_spec.inputs[0].varType.getBasicType();
		const glu::Precision	precision	= m_spec.inputs[0].varType.getPrecision();
		const int				scalarSize	= glu::getDataTypeScalarSize(type);

		DE_ASSERT(!glu::isDataTypeFloatOrVec(type));

		std::fill((int*)values[0] + scalarSize*0, (int*)values[0] + scalarSize*1, +1);
		std::fill((int*)values[0] + scalarSize*1, (int*)values[0] + scalarSize*2, -1);
		std::fill((int*)values[0] + scalarSize*2, (int*)values[0] + scalarSize*3,  0);
		fillRandomScalars(rnd, intRanges[precision].x(), intRanges[precision].y(), (int*)values[0] + scalarSize*3, (numValues-3)*scalarSize);
	}

	bool compare (const void* const* inputs, const void* const* outputs)
	{
		const glu::DataType		type			= m_spec.inputs[0].varType.getBasicType();
		const int				scalarSize		= glu::getDataTypeScalarSize(type);

		DE_ASSERT(!glu::isDataTypeFloatOrVec(type));

		for (int compNdx = 0; compNdx < scalarSize; compNdx++)
		{
			const int	in0		= ((const int*)inputs[0])[compNdx];
			const int	out0	= ((const int*)outputs[0])[compNdx];
			const int	ref0	= in0 < 0 ? -1 :
								  in0 > 0 ? +1 : 0;

			if (out0 != ref0)
			{
				m_failMsg << "Expected [" << compNdx << "] = " << ref0;
				return false;
			}
		}

		return true;
	}
};

class SignCase : public CommonFunctionCase
{
public:
	SignCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "sign", shaderType)
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
		m_spec.source = "out0 = sign(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new SignCaseInstance(ctx, m_shaderType, m_spec, m_numValues, getName());
	}
};

static void infNanRandomFloats(int numValues, void* const* values, const char *name, const ShaderSpec& spec)
{
	de::Random				rnd				(deStringHash(name) ^ 0xc2a39fu);
	const glu::DataType		type			= spec.inputs[0].varType.getBasicType();
	const glu::Precision	precision		= spec.inputs[0].varType.getPrecision();
	const int				scalarSize		= glu::getDataTypeScalarSize(type);
	const int				mantissaBits	= getMinMantissaBits(precision);
	const deUint32			mantissaMask	= ~getMaxUlpDiffFromBits(mantissaBits) & ((1u<<23)-1u);

	for (int valNdx = 0; valNdx < numValues*scalarSize; valNdx++)
	{
		// Roughly 25% chance of each of Inf and NaN
		const bool		isInf		= rnd.getFloat() > 0.75f;
		const bool		isNan		= !isInf && rnd.getFloat() > 0.66f;
		const deUint32	m			= rnd.getUint32() & mantissaMask;
		const deUint32	e			= rnd.getUint32() & 0xffu;
		const deUint32	sign		= rnd.getUint32() & 0x1u;
		// Ensure the 'quiet' bit is set on NaNs (also ensures we don't generate inf by mistake)
		const deUint32	mantissa	= isInf ? 0 : (isNan ? ((1u<<22) | m) : m);
		const deUint32	exp			= (isNan || isInf) ? 0xffu : deMin32(e, 0x7fu);
		const deUint32	value		= (sign << 31) | (exp << 23) | mantissa;

		DE_ASSERT(tcu::Float32(value).isInf() == isInf && tcu::Float32(value).isNaN() == isNan);

		((deUint32*)values[0])[valNdx] = value;
	}
}

class IsnanCaseInstance : public CommonFunctionTestInstance
{
public:
	IsnanCaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, shaderType, spec, numValues, name)
	{
	}

	void getInputValues (int numValues, void* const* values) const
	{
		infNanRandomFloats(numValues, values, m_name, m_spec);
	}

	bool compare (const void* const* inputs, const void* const* outputs)
	{
		const glu::DataType		type			= m_spec.inputs[0].varType.getBasicType();
		const glu::Precision	precision		= m_spec.inputs[0].varType.getPrecision();
		const int				scalarSize		= glu::getDataTypeScalarSize(type);

		if (precision == glu::PRECISION_HIGHP)
		{
			// Only highp is required to support inf/nan
			for (int compNdx = 0; compNdx < scalarSize; compNdx++)
			{
				const float		in0		= ((const float*)inputs[0])[compNdx];
				const bool		out0	= ((const deUint32*)outputs[0])[compNdx] != 0;
				const bool		ref		= tcu::Float32(in0).isNaN();

				if (out0 != ref)
				{
					m_failMsg << "Expected [" << compNdx << "] = " << (ref ? "true" : "false");
					return false;
				}
			}
		}
		else if (precision == glu::PRECISION_MEDIUMP || precision == glu::PRECISION_LOWP)
		{
			// NaN support is optional, check that inputs that are not NaN don't result in true.
			for (int compNdx = 0; compNdx < scalarSize; compNdx++)
			{
				const float		in0		= ((const float*)inputs[0])[compNdx];
				const bool		out0	= ((const deUint32*)outputs[0])[compNdx] != 0;
				const bool		ref		= tcu::Float32(in0).isNaN();

				if (!ref && out0)
				{
					m_failMsg << "Expected [" << compNdx << "] = " << (ref ? "true" : "false");
					return false;
				}
			}
		}

		return true;
	}
};

class IsnanCase : public CommonFunctionCase
{
public:
	IsnanCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "isnan", shaderType)
	{
		DE_ASSERT(glu::isDataTypeFloatOrVec(baseType));

		const int			vecSize		= glu::getDataTypeScalarSize(baseType);
		const glu::DataType	boolType	= vecSize > 1 ? glu::getDataTypeBoolVec(vecSize) : glu::TYPE_BOOL;

		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(boolType, glu::PRECISION_LAST)));
		m_spec.source = "out0 = isnan(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new IsnanCaseInstance(ctx, m_shaderType, m_spec, m_numValues, getName());
	}
};

class IsinfCaseInstance : public CommonFunctionTestInstance
{
public:
	IsinfCaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance(context, shaderType, spec, numValues, name)
	{
	}

	void getInputValues (int numValues, void* const* values) const
	{
		infNanRandomFloats(numValues, values, m_name, m_spec);
	}

	bool compare (const void* const* inputs, const void* const* outputs)
	{
		const glu::DataType		type			= m_spec.inputs[0].varType.getBasicType();
		const glu::Precision	precision		= m_spec.inputs[0].varType.getPrecision();
		const int				scalarSize		= glu::getDataTypeScalarSize(type);

		if (precision == glu::PRECISION_HIGHP)
		{
			// Only highp is required to support inf/nan
			for (int compNdx = 0; compNdx < scalarSize; compNdx++)
			{
				const float		in0		= ((const float*)inputs[0])[compNdx];
				const bool		out0	= ((const deUint32*)outputs[0])[compNdx] != 0;
				const bool		ref		= tcu::Float32(in0).isInf();

				if (out0 != ref)
				{
					m_failMsg << "Expected [" << compNdx << "] = " << HexBool(ref);
					return false;
				}
			}
		}
		else if (precision == glu::PRECISION_MEDIUMP)
		{
			// Inf support is optional, check that inputs that are not Inf in mediump don't result in true.
			for (int compNdx = 0; compNdx < scalarSize; compNdx++)
			{
				const float		in0		= ((const float*)inputs[0])[compNdx];
				const bool		out0	= ((const deUint32*)outputs[0])[compNdx] != 0;
				const bool		ref		= tcu::Float16(in0).isInf();

				if (!ref && out0)
				{
					m_failMsg << "Expected [" << compNdx << "] = " << (ref ? "true" : "false");
					return false;
				}
			}
		}
		// else: no verification can be performed

		return true;
	}
};

class IsinfCase : public CommonFunctionCase
{
public:
	IsinfCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "isinf", shaderType)
	{
		DE_ASSERT(glu::isDataTypeFloatOrVec(baseType));

		const int			vecSize		= glu::getDataTypeScalarSize(baseType);
		const glu::DataType	boolType	= vecSize > 1 ? glu::getDataTypeBoolVec(vecSize) : glu::TYPE_BOOL;

		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(boolType, glu::PRECISION_LAST)));
		m_spec.source = "out0 = isinf(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new IsinfCaseInstance(ctx, m_shaderType, m_spec, m_numValues, getName());
	}
};

class FloatBitsToUintIntCaseInstance : public CommonFunctionTestInstance
{
public:
	FloatBitsToUintIntCaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, shaderType, spec, numValues, name)
	{
	}

	void getInputValues (int numValues, void* const* values) const
	{
		const Vec2 ranges[] =
		{
			Vec2(-2.0f,		2.0f),	// lowp
			Vec2(-1e3f,		1e3f),	// mediump
			Vec2(-1e7f,		1e7f)	// highp
		};

		de::Random				rnd			(deStringHash(m_name) ^ 0x2790au);
		const glu::DataType		type		= m_spec.inputs[0].varType.getBasicType();
		const glu::Precision	precision	= m_spec.inputs[0].varType.getPrecision();
		const int				scalarSize	= glu::getDataTypeScalarSize(type);

		fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(), values[0], numValues*scalarSize);
	}

	bool compare (const void* const* inputs, const void* const* outputs)
	{
		const glu::DataType		type			= m_spec.inputs[0].varType.getBasicType();
		const glu::Precision	precision		= m_spec.inputs[0].varType.getPrecision();
		const int				scalarSize		= glu::getDataTypeScalarSize(type);

		const int				mantissaBits	= getMinMantissaBits(precision);
		const int				maxUlpDiff		= getMaxUlpDiffFromBits(mantissaBits);

		for (int compNdx = 0; compNdx < scalarSize; compNdx++)
		{
			const float		in0			= ((const float*)inputs[0])[compNdx];
			const deUint32	out0		= ((const deUint32*)outputs[0])[compNdx];
			const deUint32	refOut0		= tcu::Float32(in0).bits();
			const int		ulpDiff		= de::abs((int)out0 - (int)refOut0);

			if (ulpDiff > maxUlpDiff)
			{
				m_failMsg << "Expected [" << compNdx << "] = " << tcu::toHex(refOut0) << " with threshold "
							<< tcu::toHex(maxUlpDiff) << ", got diff " << tcu::toHex(ulpDiff);
				return false;
			}
		}

		return true;
	}
};

class FloatBitsToUintIntCase : public CommonFunctionCase
{
public:
	FloatBitsToUintIntCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType, bool outIsSigned)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), outIsSigned ? "floatBitsToInt" : "floatBitsToUint", shaderType)
	{
		const int			vecSize		= glu::getDataTypeScalarSize(baseType);
		const glu::DataType	intType		= outIsSigned ? (vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT)
													  : (vecSize > 1 ? glu::getDataTypeUintVec(vecSize) : glu::TYPE_UINT);

		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(intType, glu::PRECISION_HIGHP)));
		m_spec.source = outIsSigned ? "out0 = floatBitsToInt(in0);" : "out0 = floatBitsToUint(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new FloatBitsToUintIntCaseInstance(ctx, m_shaderType, m_spec, m_numValues, getName());
	}
};

class FloatBitsToIntCase : public FloatBitsToUintIntCase
{
public:
	FloatBitsToIntCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
		: FloatBitsToUintIntCase	(testCtx, baseType, precision, shaderType, true)
	{
	}

};

class FloatBitsToUintCase : public FloatBitsToUintIntCase
{
public:
	FloatBitsToUintCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
		: FloatBitsToUintIntCase	(testCtx, baseType, precision, shaderType, false)
	{
	}
};

class BitsToFloatCaseInstance : public CommonFunctionTestInstance
{
public:
	BitsToFloatCaseInstance (Context& context, glu::ShaderType shaderType, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, shaderType, spec, numValues, name)
	{
	}

	void getInputValues (int numValues, void* const* values) const
	{
		de::Random				rnd			(deStringHash(m_name) ^ 0xbbb225u);
		const glu::DataType		type		= m_spec.inputs[0].varType.getBasicType();
		const int				scalarSize	= glu::getDataTypeScalarSize(type);
		const Vec2				range		(-1e8f, +1e8f);

		// \note Filled as floats.
		fillRandomScalars(rnd, range.x(), range.y(), values[0], numValues*scalarSize);
	}

	bool compare (const void* const* inputs, const void* const* outputs)
	{
		const glu::DataType		type			= m_spec.inputs[0].varType.getBasicType();
		const int				scalarSize		= glu::getDataTypeScalarSize(type);
		const deUint32			maxUlpDiff		= 0;

		for (int compNdx = 0; compNdx < scalarSize; compNdx++)
		{
			const float		in0			= ((const float*)inputs[0])[compNdx];
			const float		out0		= ((const float*)outputs[0])[compNdx];
			const deUint32	ulpDiff		= getUlpDiffIgnoreZeroSign(in0, out0);

			if (ulpDiff > maxUlpDiff)
			{
				m_failMsg << "Expected [" << compNdx << "] = " << tcu::toHex(tcu::Float32(in0).bits()) << " with ULP threshold "
							<< tcu::toHex(maxUlpDiff) << ", got ULP diff " << tcu::toHex(ulpDiff);
				return false;
			}
		}

		return true;
	}
};

class BitsToFloatCase : public CommonFunctionCase
{
public:
	BitsToFloatCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::ShaderType shaderType)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, glu::PRECISION_HIGHP, shaderType).c_str(), glu::isDataTypeIntOrIVec(baseType) ? "intBitsToFloat" : "uintBitsToFloat", shaderType)
	{
		const bool			inIsSigned	= glu::isDataTypeIntOrIVec(baseType);
		const int			vecSize		= glu::getDataTypeScalarSize(baseType);
		const glu::DataType	floatType	= vecSize > 1 ? glu::getDataTypeFloatVec(vecSize) : glu::TYPE_FLOAT;

		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, glu::PRECISION_HIGHP)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(floatType, glu::PRECISION_HIGHP)));
		m_spec.source = inIsSigned ? "out0 = intBitsToFloat(in0);" : "out0 = uintBitsToFloat(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new BitsToFloatCaseInstance(ctx, m_shaderType, m_spec, m_numValues, getName());
	}
};

} // anonymous

ShaderCommonFunctionTests::ShaderCommonFunctionTests (tcu::TestContext& testCtx)
	: tcu::TestCaseGroup	(testCtx, "common", "Common function tests")
{
}

ShaderCommonFunctionTests::~ShaderCommonFunctionTests (void)
{
}

void ShaderCommonFunctionTests::init (void)
{
	enum
	{
		VS = (1<<glu::SHADERTYPE_VERTEX),
		TC = (1<<glu::SHADERTYPE_TESSELLATION_CONTROL),
		TE = (1<<glu::SHADERTYPE_TESSELLATION_EVALUATION),
		GS = (1<<glu::SHADERTYPE_GEOMETRY),
		FS = (1<<glu::SHADERTYPE_FRAGMENT),
		CS = (1<<glu::SHADERTYPE_COMPUTE),

		ALL_SHADERS = VS|TC|TE|GS|FS|CS,
		NEW_SHADERS = TC|TE|GS|CS,
	};

	addFunctionCases<AbsCase>				(this,	"abs",				glu::TYPE_INT,		ALL_SHADERS);
	addFunctionCases<SignCase>				(this,	"sign",				glu::TYPE_INT,		ALL_SHADERS);
	addFunctionCases<IsnanCase>				(this,	"isnan",			glu::TYPE_FLOAT,	ALL_SHADERS);
	addFunctionCases<IsinfCase>				(this,	"isinf",			glu::TYPE_FLOAT,	ALL_SHADERS);
	addFunctionCases<FloatBitsToIntCase>	(this,	"floatbitstoint",	glu::TYPE_FLOAT,	ALL_SHADERS);
	addFunctionCases<FloatBitsToUintCase>	(this,	"floatbitstouint",	glu::TYPE_FLOAT,	ALL_SHADERS);

	// (u)intBitsToFloat()
	{
		const deUint32		shaderBits	= NEW_SHADERS;
		tcu::TestCaseGroup* intGroup	= new tcu::TestCaseGroup(m_testCtx, "intbitstofloat",	"intBitsToFloat() Tests");
		tcu::TestCaseGroup* uintGroup	= new tcu::TestCaseGroup(m_testCtx, "uintbitstofloat",	"uintBitsToFloat() Tests");

		addChild(intGroup);
		addChild(uintGroup);

		for (int vecSize = 1; vecSize < 4; vecSize++)
		{
			const glu::DataType		intType		= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;
			const glu::DataType		uintType	= vecSize > 1 ? glu::getDataTypeUintVec(vecSize) : glu::TYPE_UINT;

			for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
			{
				if (shaderBits & (1<<shaderType))
				{
					intGroup->addChild(new BitsToFloatCase(getTestContext(), intType, glu::ShaderType(shaderType)));
					uintGroup->addChild(new BitsToFloatCase(getTestContext(), uintType, glu::ShaderType(shaderType)));
				}
			}
		}
	}
}

} // shaderexecutor
} // vkt
