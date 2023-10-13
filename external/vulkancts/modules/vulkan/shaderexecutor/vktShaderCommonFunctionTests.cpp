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
#include "vkQueryUtil.hpp"
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
#include <algorithm>

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

inline deUint64 getMaxUlpDiffFromBits (int numAccurateBits, int numTotalBits)
{
	const int		numGarbageBits	= numTotalBits-numAccurateBits;
	const deUint64	mask			= (1ull<<numGarbageBits)-1ull;

	return mask;
}

static int getNumMantissaBits (glu::DataType type)
{
	DE_ASSERT(glu::isDataTypeFloatOrVec(type) || glu::isDataTypeDoubleOrDVec(type));
	return (glu::isDataTypeFloatOrVec(type) ? 23 : 52);
}

static int getMinMantissaBits (glu::DataType type, glu::Precision precision)
{
	if (glu::isDataTypeDoubleOrDVec(type))
	{
		return tcu::Float64::MANTISSA_BITS;
	}

	// Float case.
	const int bits[] =
	{
		7,								// lowp
		tcu::Float16::MANTISSA_BITS,	// mediump
		tcu::Float32::MANTISSA_BITS,	// highp
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(bits) == glu::PRECISION_LAST);
	DE_ASSERT(de::inBounds<int>(precision, 0, DE_LENGTH_OF_ARRAY(bits)));
	return bits[precision];
}

static int getExponentBits (glu::DataType type)
{
	DE_ASSERT(glu::isDataTypeFloatOrVec(type) || glu::isDataTypeDoubleOrDVec(type));
	return (glu::isDataTypeFloatOrVec(type) ? static_cast<int>(tcu::Float32::EXPONENT_BITS) : static_cast<int>(tcu::Float64::EXPONENT_BITS));
}

static deUint32 getExponentMask (int exponentBits)
{
	DE_ASSERT(exponentBits > 0);
	return ((1u<<exponentBits) - 1u);
}

static int getComponentByteSize (glu::DataType type)
{
	const glu::DataType scalarType = glu::getDataTypeScalarType(type);

	DE_ASSERT(	scalarType == glu::TYPE_FLOAT	||
				scalarType == glu::TYPE_FLOAT16	||
				scalarType == glu::TYPE_DOUBLE	||
				scalarType == glu::TYPE_INT		||
				scalarType == glu::TYPE_UINT	||
				scalarType == glu::TYPE_INT8	||
				scalarType == glu::TYPE_UINT8	||
				scalarType == glu::TYPE_INT16	||
				scalarType == glu::TYPE_UINT16	||
				scalarType == glu::TYPE_BOOL	);

	switch (scalarType)
	{
	case glu::TYPE_INT8:
	case glu::TYPE_UINT8:
		return 1;
	case glu::TYPE_INT16:
	case glu::TYPE_UINT16:
	case glu::TYPE_FLOAT16:
		return 2;
	case glu::TYPE_BOOL:
	case glu::TYPE_INT:
	case glu::TYPE_UINT:
	case glu::TYPE_FLOAT:
		return 4;
	case glu::TYPE_DOUBLE:
		return 8;
	default:
		DE_ASSERT(false); break;
	}
	// Unreachable.
	return 0;
}

static vector<int> getScalarSizes (const vector<Symbol>& symbols)
{
	vector<int> sizes(symbols.size());
	for (int ndx = 0; ndx < (int)symbols.size(); ++ndx)
		sizes[ndx] = symbols[ndx].varType.getScalarSize();
	return sizes;
}

static vector<int> getComponentByteSizes (const vector<Symbol>& symbols)
{
	vector<int> sizes;
	sizes.reserve(symbols.size());
	for (const auto& sym : symbols)
		sizes.push_back(getComponentByteSize(sym.varType.getBasicType()));
	return sizes;
}

static int computeTotalByteSize (const vector<Symbol>& symbols)
{
	int totalSize = 0;
	for (const auto& sym : symbols)
		totalSize += getComponentByteSize(sym.varType.getBasicType()) * sym.varType.getScalarSize();
	return totalSize;
}

static vector<void*> getInputOutputPointers (const vector<Symbol>& symbols, vector<deUint8>& data, const int numValues)
{
	vector<void*>	pointers		(symbols.size());
	int				curScalarOffset	= 0;

	for (int varNdx = 0; varNdx < (int)symbols.size(); ++varNdx)
	{
		const Symbol&	var				= symbols[varNdx];
		const int		scalarSize		= var.varType.getScalarSize();
		const auto		componentBytes	= getComponentByteSize(var.varType.getBasicType());

		// Uses planar layout as input/output specs do not support strides.
		pointers[varNdx] = &data[curScalarOffset];
		curScalarOffset += scalarSize*numValues*componentBytes;
	}

	DE_ASSERT(curScalarOffset == (int)data.size());

	return pointers;
}

void checkTypeSupport (Context& context, glu::DataType dataType)
{
	if (glu::isDataTypeDoubleOrDVec(dataType))
	{
		const auto&	vki				= context.getInstanceInterface();
		const auto	physicalDevice	= context.getPhysicalDevice();

		const auto features = vk::getPhysicalDeviceFeatures(vki, physicalDevice);
		if (!features.shaderFloat64)
			TCU_THROW(NotSupportedError, "64-bit floats not supported by the implementation");
	}
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

struct HexDouble
{
	const double value;
	HexDouble (const double value_) : value(value_) {}
};

std::ostream& operator<< (std::ostream& str, const HexDouble& v)
{
	return str << v.value << " / " << tcu::toHex(tcu::Float64(v.value).bits());
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
			case glu::TYPE_DOUBLE:	str << HexDouble(((const double*)varValue.value)[compNdx]);			break;

			default:
				DE_ASSERT(false);
		}
	}

	if (numComponents > 1)
		str << ")";

	return str;
}

static std::string getCommonFuncCaseName (glu::DataType baseType, glu::Precision precision)
{
	const bool isDouble = glu::isDataTypeDoubleOrDVec(baseType);
	return string(glu::getDataTypeName(baseType)) + (isDouble ? "" : getPrecisionPostfix(precision)) + "_compute";
}

template<class TestClass>
static void addFunctionCases (tcu::TestCaseGroup* parent, const char* functionName, const std::vector<glu::DataType>& scalarTypes)
{
	tcu::TestCaseGroup* group = new tcu::TestCaseGroup(parent->getTestContext(), functionName, functionName);
	parent->addChild(group);

	for (const auto scalarType : scalarTypes)
	{
		const bool	isDouble	= glu::isDataTypeDoubleOrDVec(scalarType);
		const int	lowestPrec	= (isDouble ? glu::PRECISION_LAST : glu::PRECISION_MEDIUMP);
		const int	highestPrec	= (isDouble ? glu::PRECISION_LAST : glu::PRECISION_HIGHP);

		for (int vecSize = 1; vecSize <= 4; vecSize++)
		{
			for (int prec = lowestPrec; prec <= highestPrec; prec++)
			{
				group->addChild(new TestClass(parent->getTestContext(), glu::DataType(scalarType + vecSize - 1), glu::Precision(prec)));
			}
		}
	}
}

// CommonFunctionCase

class CommonFunctionCase : public TestCase
{
public:
										CommonFunctionCase			(tcu::TestContext& testCtx, const char* name, const char* description);
										~CommonFunctionCase			(void);
	virtual	void						initPrograms				(vk::SourceCollections& programCollection) const
										{
											generateSources(glu::SHADERTYPE_COMPUTE, m_spec, programCollection);
										}

	virtual TestInstance*				createInstance				(Context& context) const = 0;

protected:
										CommonFunctionCase			(const CommonFunctionCase&);
	CommonFunctionCase&					operator=					(const CommonFunctionCase&);

	ShaderSpec							m_spec;
	const int							m_numValues;
};

CommonFunctionCase::CommonFunctionCase (tcu::TestContext& testCtx, const char* name, const char* description)
	: TestCase		(testCtx, name, description)
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
										CommonFunctionTestInstance	(Context& context, const ShaderSpec& spec, int numValues, const char* name)
											: TestInstance	(context)
											, m_spec		(spec)
											, m_numValues	(numValues)
											, m_name		(name)
											, m_executor	(createExecutor(context, glu::SHADERTYPE_COMPUTE, spec))
										{
										}
	virtual tcu::TestStatus				iterate						(void);

protected:
	virtual void						getInputValues				(int numValues, void* const* values) const = 0;
	virtual bool						compare						(const void* const* inputs, const void* const* outputs) = 0;

	const ShaderSpec					m_spec;
	const int							m_numValues;

	// \todo [2017-03-07 pyry] Hack used to generate seeds for test cases - get rid of this.
	const char*							m_name;

	std::ostringstream					m_failMsg;					//!< Comparison failure help message.

	de::UniquePtr<ShaderExecutor>		m_executor;
};

tcu::TestStatus CommonFunctionTestInstance::iterate (void)
{
	const int				numInputBytes			= computeTotalByteSize(m_spec.inputs);
	const int				numOutputBytes			= computeTotalByteSize(m_spec.outputs);
	vector<deUint8>			inputData				(numInputBytes * m_numValues);
	vector<deUint8>			outputData				(numOutputBytes * m_numValues);
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
		const vector<int>		inCompByteSizes		= getComponentByteSizes(m_spec.inputs);
		const vector<int>		outCompByteSizes	= getComponentByteSizes(m_spec.outputs);
		vector<void*>			curInputPtr			(inputPointers.size());
		vector<void*>			curOutputPtr		(outputPointers.size());
		int						numFailed			= 0;
		tcu::TestContext&		testCtx				= m_context.getTestContext();

		for (int valNdx = 0; valNdx < m_numValues; valNdx++)
		{
			// Set up pointers for comparison.
			for (int inNdx = 0; inNdx < (int)curInputPtr.size(); ++inNdx)
				curInputPtr[inNdx] = (deUint8*)inputPointers[inNdx] + inScalarSizes[inNdx]*inCompByteSizes[inNdx]*valNdx;

			for (int outNdx = 0; outNdx < (int)curOutputPtr.size(); ++outNdx)
				curOutputPtr[outNdx] = (deUint8*)outputPointers[outNdx] + outScalarSizes[outNdx]*outCompByteSizes[outNdx]*valNdx;

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
	AbsCaseInstance (Context& context, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, spec, numValues, name)
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
	AbsCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision).c_str(), "abs")
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
		m_spec.source = "out0 = abs(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new AbsCaseInstance(ctx, m_spec, m_numValues, getName());
	}
};

class SignCaseInstance : public CommonFunctionTestInstance
{
public:
	SignCaseInstance (Context& context, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, spec, numValues, name)
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
	SignCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision).c_str(), "sign")
	{
		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
		m_spec.source = "out0 = sign(in0);";
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new SignCaseInstance(ctx, m_spec, m_numValues, getName());
	}
};

static void infNanRandomFloats(int numValues, void* const* values, const char *name, const ShaderSpec& spec)
{
	constexpr deUint64		kOne			= 1;
	de::Random				rnd				(deStringHash(name) ^ 0xc2a39fu);
	const glu::DataType		type			= spec.inputs[0].varType.getBasicType();
	const glu::Precision	precision		= spec.inputs[0].varType.getPrecision();
	const int				scalarSize		= glu::getDataTypeScalarSize(type);
	const int				minMantissaBits	= getMinMantissaBits(type, precision);
	const int				numMantissaBits	= getNumMantissaBits(type);
	const deUint64			mantissaMask	= ~getMaxUlpDiffFromBits(minMantissaBits, numMantissaBits) & ((kOne<<numMantissaBits)-kOne);
	const int				exponentBits	= getExponentBits(type);
	const deUint32			exponentMask	= getExponentMask(exponentBits);
	const bool				isDouble		= glu::isDataTypeDoubleOrDVec(type);
	const deUint64			exponentBias	= (isDouble ? static_cast<deUint64>(tcu::Float64::EXPONENT_BIAS) : static_cast<deUint64>(tcu::Float32::EXPONENT_BIAS));

	int numInf = 0;
	int numNan = 0;
	for (int valNdx = 0; valNdx < numValues*scalarSize; valNdx++)
	{
		// Roughly 25% chance of each of Inf and NaN
		const bool		isInf		= rnd.getFloat() > 0.75f;
		const bool		isNan		= !isInf && rnd.getFloat() > 0.66f;
		const deUint64	m			= rnd.getUint64() & mantissaMask;
		const deUint64	e			= static_cast<deUint64>(rnd.getUint32() & exponentMask);
		const deUint64	sign		= static_cast<deUint64>(rnd.getUint32() & 0x1u);
		// Ensure the 'quiet' bit is set on NaNs (also ensures we don't generate inf by mistake)
		const deUint64	mantissa	= isInf ? 0 : (isNan ? ((kOne<<(numMantissaBits-1)) | m) : m);
		const deUint64	exp			= (isNan || isInf) ? exponentMask : std::min(e, exponentBias);
		const deUint64	value		= (sign << (numMantissaBits + exponentBits)) | (exp << numMantissaBits) | static_cast<deUint32>(mantissa);
		if (isInf) numInf++;
		if (isNan) numNan++;

		if (isDouble)
		{
			DE_ASSERT(tcu::Float64(value).isInf() == isInf && tcu::Float64(value).isNaN() == isNan);
			((deUint64*)values[0])[valNdx] = value;
		}
		else
		{
			const auto value32 = static_cast<deUint32>(value);
			DE_ASSERT(tcu::Float32(value32).isInf() == isInf && tcu::Float32(value32).isNaN() == isNan);
			((deUint32*)values[0])[valNdx] = value32;
		}
	}
	// Check for minimal coverage of intended cases.
	DE_ASSERT(0 < numInf);
	DE_ASSERT(0 < numNan);
	DE_ASSERT(numInf + numNan < numValues*scalarSize);

	// Release build does not use them
	DE_UNREF(numInf);
	DE_UNREF(numNan);
}

class IsnanCaseInstance : public CommonFunctionTestInstance
{
public:
	IsnanCaseInstance (Context& context, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, spec, numValues, name)
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
		const bool				isDouble		= glu::isDataTypeDoubleOrDVec(type);

		for (int compNdx = 0; compNdx < scalarSize; compNdx++)
		{
			const bool	out0	= reinterpret_cast<const deUint32*>(outputs[0])[compNdx] != 0;
			bool		ok;
			bool		ref;

			if (isDouble)
			{
				const double in0 = reinterpret_cast<const double*>(inputs[0])[compNdx];
				ref	= tcu::Float64(in0).isNaN();
				ok = (out0 == ref);
			}
			else
			{
				const float	in0	= reinterpret_cast<const float*>(inputs[0])[compNdx];
				ref	= tcu::Float32(in0).isNaN();

				// NaN support only required for highp. Otherwise just check for false positives.
				if (precision == glu::PRECISION_HIGHP)
					ok = (out0 == ref);
				else
					ok = ref || !out0;
			}

			if (!ok)
			{
				m_failMsg << "Expected [" << compNdx << "] = " << (ref ? "true" : "false");
				return false;
			}
		}

		return true;
	}
};

class IsnanCase : public CommonFunctionCase
{
public:
	IsnanCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision).c_str(), "isnan")
	{
		DE_ASSERT(glu::isDataTypeFloatOrVec(baseType) || glu::isDataTypeDoubleOrDVec(baseType));

		const int			vecSize		= glu::getDataTypeScalarSize(baseType);
		const glu::DataType	boolType	= vecSize > 1 ? glu::getDataTypeBoolVec(vecSize) : glu::TYPE_BOOL;

		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(boolType, glu::PRECISION_LAST)));
		m_spec.source = "out0 = isnan(in0);";
	}

	void checkSupport (Context& context) const
	{
		checkTypeSupport(context, m_spec.inputs[0].varType.getBasicType());
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new IsnanCaseInstance(ctx, m_spec, m_numValues, getName());
	}
};

class IsinfCaseInstance : public CommonFunctionTestInstance
{
public:
	IsinfCaseInstance (Context& context, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance(context, spec, numValues, name)
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
		const bool				isDouble		= glu::isDataTypeDoubleOrDVec(type);

		for (int compNdx = 0; compNdx < scalarSize; compNdx++)
		{
			const bool	out0 = reinterpret_cast<const deUint32*>(outputs[0])[compNdx] != 0;
			bool		ref;
			bool		ok;

			if (isDouble)
			{
				const double in0 = reinterpret_cast<const double*>(inputs[0])[compNdx];
				ref = tcu::Float64(in0).isInf();
				ok = (out0 == ref);
			}
			else
			{
				const float in0 = reinterpret_cast<const float*>(inputs[0])[compNdx];
				if (precision == glu::PRECISION_HIGHP)
				{
					// Only highp is required to support inf/nan
					ref = tcu::Float32(in0).isInf();
					ok = (out0 == ref);
				}
				else
				{
					// Inf support is optional, check that inputs that are not Inf in mediump don't result in true.
					ref = tcu::Float16(in0).isInf();
					ok = (out0 || !ref);
				}
			}

			if (!ok)
			{
				m_failMsg << "Expected [" << compNdx << "] = " << HexBool(ref);
				return false;
			}
		}

		return true;
	}
};

class IsinfCase : public CommonFunctionCase
{
public:
	IsinfCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision).c_str(), "isinf")
	{
		DE_ASSERT(glu::isDataTypeFloatOrVec(baseType) || glu::isDataTypeDoubleOrDVec(baseType));

		const int			vecSize		= glu::getDataTypeScalarSize(baseType);
		const glu::DataType	boolType	= vecSize > 1 ? glu::getDataTypeBoolVec(vecSize) : glu::TYPE_BOOL;

		m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
		m_spec.outputs.push_back(Symbol("out0", glu::VarType(boolType, glu::PRECISION_LAST)));
		m_spec.source = "out0 = isinf(in0);";
	}

	void checkSupport (Context& context) const
	{
		checkTypeSupport(context, m_spec.inputs[0].varType.getBasicType());
	}

	TestInstance* createInstance (Context& ctx) const
	{
		return new IsinfCaseInstance(ctx, m_spec, m_numValues, getName());
	}
};

class FloatBitsToUintIntCaseInstance : public CommonFunctionTestInstance
{
public:
	FloatBitsToUintIntCaseInstance (Context& context, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, spec, numValues, name)
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

		const int				minMantissaBits	= getMinMantissaBits(type, precision);
		const int				numMantissaBits	= getNumMantissaBits(type);
		const int				maxUlpDiff		= static_cast<int>(getMaxUlpDiffFromBits(minMantissaBits, numMantissaBits));

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
	FloatBitsToUintIntCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision, bool outIsSigned)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, precision).c_str(), outIsSigned ? "floatBitsToInt" : "floatBitsToUint")
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
		return new FloatBitsToUintIntCaseInstance(ctx, m_spec, m_numValues, getName());
	}
};

class FloatBitsToIntCase : public FloatBitsToUintIntCase
{
public:
	FloatBitsToIntCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision)
		: FloatBitsToUintIntCase	(testCtx, baseType, precision, true)
	{
	}

};

class FloatBitsToUintCase : public FloatBitsToUintIntCase
{
public:
	FloatBitsToUintCase (tcu::TestContext& testCtx, glu::DataType baseType, glu::Precision precision)
		: FloatBitsToUintIntCase	(testCtx, baseType, precision, false)
	{
	}
};

class BitsToFloatCaseInstance : public CommonFunctionTestInstance
{
public:
	BitsToFloatCaseInstance (Context& context, const ShaderSpec& spec, int numValues, const char* name)
		: CommonFunctionTestInstance	(context, spec, numValues, name)
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
	BitsToFloatCase (tcu::TestContext& testCtx, glu::DataType baseType)
		: CommonFunctionCase	(testCtx, getCommonFuncCaseName(baseType, glu::PRECISION_HIGHP).c_str(), glu::isDataTypeIntOrIVec(baseType) ? "intBitsToFloat" : "uintBitsToFloat")
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
		return new BitsToFloatCaseInstance(ctx, m_spec, m_numValues, getName());
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
	static const std::vector<glu::DataType>	kIntOnly		(1u, glu::TYPE_INT);
	static const std::vector<glu::DataType>	kFloatOnly		(1u, glu::TYPE_FLOAT);
	static const std::vector<glu::DataType>	kFloatAndDouble	{glu::TYPE_FLOAT, glu::TYPE_DOUBLE};

	addFunctionCases<AbsCase>				(this,	"abs",				kIntOnly);
	addFunctionCases<SignCase>				(this,	"sign",				kIntOnly);
	addFunctionCases<IsnanCase>				(this,	"isnan",			kFloatAndDouble);
	addFunctionCases<IsinfCase>				(this,	"isinf",			kFloatAndDouble);
	addFunctionCases<FloatBitsToIntCase>	(this,	"floatbitstoint",	kFloatOnly);
	addFunctionCases<FloatBitsToUintCase>	(this,	"floatbitstouint",	kFloatOnly);

	// (u)intBitsToFloat()
	{
		tcu::TestCaseGroup* intGroup	= new tcu::TestCaseGroup(m_testCtx, "intbitstofloat",	"intBitsToFloat() Tests");
		tcu::TestCaseGroup* uintGroup	= new tcu::TestCaseGroup(m_testCtx, "uintbitstofloat",	"uintBitsToFloat() Tests");

		addChild(intGroup);
		addChild(uintGroup);

		for (int vecSize = 1; vecSize < 4; vecSize++)
		{
			const glu::DataType		intType		= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;
			const glu::DataType		uintType	= vecSize > 1 ? glu::getDataTypeUintVec(vecSize) : glu::TYPE_UINT;

			intGroup->addChild(new BitsToFloatCase(getTestContext(), intType));
			uintGroup->addChild(new BitsToFloatCase(getTestContext(), uintType));
		}
	}
}

} // shaderexecutor
} // vkt
