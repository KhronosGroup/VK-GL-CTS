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
 * \brief VK_KHR_shader_float_controls tests.
 *//*--------------------------------------------------------------------*/


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
#include <cstring>
#include <vector>
#include <limits>
#include <fenv.h>

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
	B_STATEMENT_USAGE_ARGS_CONST_FLOAT		= (1<<0 ),
	B_STATEMENT_USAGE_ARGS_CONST_FP16		= (1<<1 ),
	B_STATEMENT_USAGE_ARGS_CONST_FP32		= (1<<2 ),
	B_STATEMENT_USAGE_ARGS_CONST_FP64		= (1<<3 ),
	B_STATEMENT_USAGE_TYPES_TYPE_FLOAT		= (1<<4 ),
	B_STATEMENT_USAGE_TYPES_TYPE_FP16		= (1<<5 ),
	B_STATEMENT_USAGE_TYPES_TYPE_FP32		= (1<<6 ),
	B_STATEMENT_USAGE_TYPES_TYPE_FP64		= (1<<7 ),
	B_STATEMENT_USAGE_CONSTS_TYPE_FLOAT		= (1<<8 ),
	B_STATEMENT_USAGE_CONSTS_TYPE_FP16		= (1<<9 ),
	B_STATEMENT_USAGE_CONSTS_TYPE_FP32		= (1<<10),
	B_STATEMENT_USAGE_CONSTS_TYPE_FP64		= (1<<11),
	B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT	= (1<<12),
	B_STATEMENT_USAGE_COMMANDS_CONST_FP16	= (1<<13),
	B_STATEMENT_USAGE_COMMANDS_CONST_FP32	= (1<<14),
	B_STATEMENT_USAGE_COMMANDS_CONST_FP64	= (1<<15),
	B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT	= (1<<16),
	B_STATEMENT_USAGE_COMMANDS_TYPE_FP16	= (1<<17),
	B_STATEMENT_USAGE_COMMANDS_TYPE_FP32	= (1<<18),
	B_STATEMENT_USAGE_COMMANDS_TYPE_FP64	= (1<<19),
};

typedef deUint32 FloatStatementUsageFlags;

// Enum containing float behaviors that its possible to test.
enum BehaviorFlagBits
{
	B_DENORM_PRESERVE	= 0x00000001,		// DenormPreserve
	B_DENORM_FLUSH		= 0x00000002,		// DenormFlushToZero
	B_ZIN_PRESERVE		= 0x00000004,		// SignedZeroInfNanPreserve
	B_RTE_ROUNDING		= 0x00000008,		// RoundingModeRTE
	B_RTZ_ROUNDING		= 0x00000010		// RoundingModeRTZ
};

typedef deUint32 BehaviorFlags;

// Codes for all float values used in tests as arguments and operation results
// This approach allows to replace values with different types reducing complexity of the tests implementation
enum ValueId
{
	// common values used as both arguments and results
	V_UNUSED = 0,		//  used to mark arguments that are not used in operation
	V_MINUS_INF,		//    or results of tests cases that should be skipped
	V_MINUS_ONE,		// -1.0
	V_MINUS_ZERO,		// -0.0
	V_ZERO,				//  0.0
	V_HALF,				//  0.5
	V_ONE,				//  1.0
	V_INF,
	V_DENORM,
	V_NAN,

	// arguments for rounding mode tests - used only when arguments are passed from input
	V_ADD_ARG_A,
	V_ADD_ARG_B,
	V_SUB_ARG_A,
	V_SUB_ARG_B,
	V_MUL_ARG_A,
	V_MUL_ARG_B,
	V_DOT_ARG_A,
	V_DOT_ARG_B,

	// arguments of conversion operations - used only when arguments are passed from input
	V_CONV_FROM_FP32_ARG,
	V_CONV_FROM_FP64_ARG,

	// arguments of rounding operations
	V_ADD_RTZ_RESULT,
	V_ADD_RTE_RESULT,
	V_SUB_RTZ_RESULT,
	V_SUB_RTE_RESULT,
	V_MUL_RTZ_RESULT,
	V_MUL_RTE_RESULT,
	V_DOT_RTZ_RESULT,
	V_DOT_RTE_RESULT,

	// non comon results of some operation - corner cases
	V_MINUS_ONE_OR_CLOSE,			// value used only fur fp16 subtraction result of preserved denorm and one
	V_PI_DIV_2,
	V_ZERO_OR_MINUS_ZERO,			// both +0 and -0 are accepted
	V_ZERO_OR_ONE,					// both +0 and 1 are accepted
	V_ZERO_OR_FP16_DENORM_TO_FP32,	// both 0 and fp32 representation of fp16 denorm are accepted
	V_ZERO_OR_FP16_DENORM_TO_FP64,
	V_ZERO_OR_FP32_DENORM_TO_FP64,
	V_DENORM_TIMES_TWO,
	V_DEGREES_DENORM,
	V_TRIG_ONE,						// 1.0 trigonometric operations, including precision margin
	V_MINUS_INF_OR_LOG_DENORM,
	V_MINUS_INF_OR_LOG2_DENORM,
	V_ZERO_OR_SQRT_DENORM,
	V_INF_OR_INV_SQRT_DENORM,

	//results of conversion operations
	V_CONV_TO_FP16_RTZ_RESULT,
	V_CONV_TO_FP16_RTE_RESULT,
	V_CONV_TO_FP32_RTZ_RESULT,
	V_CONV_TO_FP32_RTE_RESULT,
	V_CONV_DENORM_SMALLER,			// used e.g. when converting fp16 denorm to fp32
	V_CONV_DENORM_BIGGER,
};

// Enum containing all tested operatios. Operations are defined in generic way so that
// they can be used to generate tests operating on arguments with different values of
// specified float type.
enum OperationId
{
	// spir-v unary operations
	O_NEGATE = 0,
	O_COMPOSITE,
	O_COMPOSITE_INS,
	O_COPY,
	O_D_EXTRACT,
	O_D_INSERT,
	O_SHUFFLE,
	O_TRANSPOSE,
	O_CONV_FROM_FP16,
	O_CONV_FROM_FP32,
	O_CONV_FROM_FP64,
	O_SCONST_CONV_FROM_FP32_TO_FP16,
	O_SCONST_CONV_FROM_FP64_TO_FP32,
	O_SCONST_CONV_FROM_FP64_TO_FP16,
	O_RETURN_VAL,

	// spir-v binary operations
	O_ADD,
	O_SUB,
	O_MUL,
	O_DIV,
	O_REM,
	O_MOD,
	O_PHI,
	O_SELECT,
	O_DOT,
	O_VEC_MUL_S,
	O_VEC_MUL_M,
	O_MAT_MUL_S,
	O_MAT_MUL_V,
	O_MAT_MUL_M,
	O_OUT_PROD,
	O_ORD_EQ,
	O_UORD_EQ,
	O_ORD_NEQ,
	O_UORD_NEQ,
	O_ORD_LS,
	O_UORD_LS,
	O_ORD_GT,
	O_UORD_GT,
	O_ORD_LE,
	O_UORD_LE,
	O_ORD_GE,
	O_UORD_GE,

	// glsl unary operations
	O_ROUND,
	O_ROUND_EV,
	O_TRUNC,
	O_ABS,
	O_SIGN,
	O_FLOOR,
	O_CEIL,
	O_FRACT,
	O_RADIANS,
	O_DEGREES,
	O_SIN,
	O_COS,
	O_TAN,
	O_ASIN,
	O_ACOS,
	O_ATAN,
	O_SINH,
	O_COSH,
	O_TANH,
	O_ASINH,
	O_ACOSH,
	O_ATANH,
	O_EXP,
	O_LOG,
	O_EXP2,
	O_LOG2,
	O_SQRT,
	O_INV_SQRT,
	O_MODF,
	O_MODF_ST,
	O_FREXP,
	O_FREXP_ST,
	O_LENGHT,
	O_NORMALIZE,
	O_REFLECT,
	O_REFRACT,
	O_MAT_DET,
	O_MAT_INV,
	O_PH_DENORM,	// PackHalf2x16
	O_UPH_DENORM,
	O_PD_DENORM,	// PackDouble2x32
	O_UPD_DENORM_FLUSH,
	O_UPD_DENORM_PRESERVE,

	// glsl binary operations
	O_ATAN2,
	O_POW,
	O_MIX,
	O_FMA,
	O_MIN,
	O_MAX,
	O_CLAMP,
	O_STEP,
	O_SSTEP,
	O_DIST,
	O_CROSS,
	O_FACE_FWD,
	O_NMIN,
	O_NMAX,
	O_NCLAMP,

	O_ORTE_ROUND,
	O_ORTZ_ROUND
};

// Structures storing data required to test DenormPreserve and DenormFlushToZero modes.
// Operations are separated into binary and unary lists because binary operations can be tested with
// two attributes and thus denorms can be tested in combination with value, denorm, inf and nan.
// Unary operations are only tested with denorms.
struct BinaryCase
{
	OperationId	operationId;
	ValueId		opVarResult;
	ValueId		opDenormResult;
	ValueId		opInfResult;
	ValueId		opNanResult;
};
struct UnaryCase
{
	OperationId	operationId;
	ValueId		result;
};

// Function replacing all occurrences of substring with string passed in last parameter.
string replace(string str, const string& from, const string& to)
{
	// to keep spir-v code clean and easier to read parts of it are processed
	// with this method instead of StringTemplate; main usage of this method is the
	// replacement of "float_" with "f16_", "f32_" or "f64_" depending on test case

	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
	return str;
}

// Structure used to perform bits conversion int type <-> float type.
template<typename FLOAT_TYPE, typename UINT_TYPE>
struct RawConvert
{
	union Value
	{
		FLOAT_TYPE	fp;
		UINT_TYPE	ui;
	};
};

// Traits used to get int type that can store equivalent float type.
template<typename FLOAT_TYPE>
struct GetCoresponding
{
	typedef deUint16 uint_type;
};
template<>
struct GetCoresponding<float>
{
	typedef deUint32 uint_type;
};
template<>
struct GetCoresponding<double>
{
	typedef deUint64 uint_type;
};

// All values used for arguments and operation results are stored in single map.
// Each float type (fp16, fp32, fp64) has its own map that is used during
// test setup and during verification. TypeValuesBase is interface to that map.
class TypeValuesBase
{
public:
	TypeValuesBase();
	virtual ~TypeValuesBase() = default;

	virtual BufferSp	constructInputBuffer	(const ValueId* twoArguments) const = 0;
	virtual BufferSp	constructOutputBuffer	(ValueId result) const = 0;
	virtual void		fillInputData			(const ValueId* twoArguments, vector<deUint8>& bufferData, deUint32& offset) const = 0;

protected:
	const double	pi;
};

TypeValuesBase::TypeValuesBase()
	: pi(3.14159265358979323846)
{
}

typedef de::SharedPtr<TypeValuesBase> TypeValuesSP;

template <typename FLOAT_TYPE>
class TypeValues: public TypeValuesBase
{
public:
	TypeValues();

	BufferSp	constructInputBuffer	(const ValueId* twoArguments) const override;
	BufferSp	constructOutputBuffer	(ValueId result) const override;
	void		fillInputData			(const ValueId* twoArguments, vector<deUint8>& bufferData, deUint32& offset) const override;

	FLOAT_TYPE getValue(ValueId id) const;

	template <typename UINT_TYPE>
	FLOAT_TYPE exactByteEquivalent(UINT_TYPE byteValue) const;

private:
	typedef map<ValueId, FLOAT_TYPE> ValueMap;
	ValueMap m_valueIdToFloatType;
};

template <typename FLOAT_TYPE>
BufferSp TypeValues<FLOAT_TYPE>::constructInputBuffer(const ValueId* twoArguments) const
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

	std::vector<FLOAT_TYPE> outputData(1, exactByteEquivalent<uint_t>(value));
	return BufferSp(new Buffer<FLOAT_TYPE>(outputData));
}

template <typename FLOAT_TYPE>
void TypeValues<FLOAT_TYPE>::fillInputData(const ValueId* twoArguments, vector<deUint8>& bufferData, deUint32& offset) const
{
	deUint32 typeSize = sizeof(FLOAT_TYPE);

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
TypeValues<deFloat16>::TypeValues()
	: TypeValuesBase()
{
	// NOTE: when updating entries in m_valueIdToFloatType make sure to
	// update also valueIdToSnippetArgMap defined in updateSpirvSnippets()
	ValueMap& vm = m_valueIdToFloatType;
	vm[V_UNUSED]			= deFloat32To16(0.0f);
	vm[V_MINUS_INF]			= 0xfc00;
	vm[V_MINUS_ONE]			= deFloat32To16(-1.0f);
	vm[V_MINUS_ZERO]		= 0x8000;
	vm[V_ZERO]				= 0x0000;
	vm[V_HALF]				= deFloat32To16(0.5f);
	vm[V_ONE]				= deFloat32To16(1.0f);
	vm[V_INF]				= 0x7c00;
	vm[V_DENORM]			= 0x03f0; // this value should be the same as the result of denormBase - epsilon
	vm[V_NAN]				= 0x7cf0;

	vm[V_PI_DIV_2]			= 0x3e48;
	vm[V_DENORM_TIMES_TWO]	= 0x07e0;
	vm[V_DEGREES_DENORM]	= 0x1b0c;

	vm[V_ADD_ARG_A]					= 0x3c03;
	vm[V_ADD_ARG_B]					= vm[V_ONE];
	vm[V_SUB_ARG_A]					= vm[V_ADD_ARG_A];
	vm[V_SUB_ARG_B]					= 0x4203;
	vm[V_MUL_ARG_A]					= vm[V_ADD_ARG_A];
	vm[V_MUL_ARG_B]					= 0x1900;
	vm[V_DOT_ARG_A]					= vm[V_ADD_ARG_A];
	vm[V_DOT_ARG_B]					= vm[V_MUL_ARG_B];
	vm[V_CONV_FROM_FP32_ARG]		= vm[V_UNUSED];
	vm[V_CONV_FROM_FP64_ARG]		= vm[V_UNUSED];

	vm[V_ADD_RTZ_RESULT]			= 0x4001;	// deFloat16Add(vm[V_ADD_ARG_A], vm[V_ADD_ARG_B], rtz)
	vm[V_SUB_RTZ_RESULT]			= 0xc001;	// deFloat16Sub(vm[V_SUB_ARG_A], vm[V_SUB_ARG_B], rtz)
	vm[V_MUL_RTZ_RESULT]			= 0x1903;	// deFloat16Mul(vm[V_MUL_ARG_A], vm[V_MUL_ARG_B], rtz)
	vm[V_DOT_RTZ_RESULT]			= 0x1d03;
	vm[V_CONV_TO_FP16_RTZ_RESULT]	= deFloat32To16Round(1.22334445f, DE_ROUNDINGMODE_TO_ZERO);
	vm[V_CONV_TO_FP32_RTZ_RESULT]	= vm[V_UNUSED];

	vm[V_ADD_RTE_RESULT]			= 0x4002;	// deFloat16Add(vm[V_ADD_ARG_A], vm[V_ADD_ARG_B], rte)
	vm[V_SUB_RTE_RESULT]			= 0xc002;	// deFloat16Sub(vm[V_SUB_ARG_A], vm[V_SUB_ARG_B], rte)
	vm[V_MUL_RTE_RESULT]			= 0x1904;	// deFloat16Mul(vm[V_MUL_ARG_A], vm[V_MUL_ARG_B], rte)
	vm[V_DOT_RTE_RESULT]			= 0x1d04;
	vm[V_CONV_TO_FP16_RTE_RESULT]	= deFloat32To16Round(1.22334445f, DE_ROUNDINGMODE_TO_NEAREST_EVEN);
	vm[V_CONV_TO_FP32_RTE_RESULT]	= vm[V_UNUSED];

	// there is no precision to store fp32 denorm nor fp64 denorm
	vm[V_CONV_DENORM_SMALLER]		= vm[V_ZERO];
	vm[V_CONV_DENORM_BIGGER]		= vm[V_ZERO];
}

template <>
TypeValues<float>::TypeValues()
	: TypeValuesBase()
{
	// NOTE: when updating entries in m_valueIdToFloatType make sure to
	// update also valueIdToSnippetArgMap defined in updateSpirvSnippets()
	ValueMap& vm = m_valueIdToFloatType;
	vm[V_UNUSED]			=  0.0f;
	vm[V_MINUS_INF]			= -std::numeric_limits<float>::infinity();
	vm[V_MINUS_ONE]			= -1.0f;
	vm[V_MINUS_ZERO]		= -0.0f;
	vm[V_ZERO]				=  0.0f;
	vm[V_HALF]				=  0.5f;
	vm[V_ONE]				=  1.0f;
	vm[V_INF]				=  std::numeric_limits<float>::infinity();
	vm[V_DENORM]			=  static_cast<float>(1.413e-42); // 0x000003f0
	vm[V_NAN]				=  std::numeric_limits<float>::quiet_NaN();

	vm[V_PI_DIV_2]			=  static_cast<float>(pi / 2);
	vm[V_DENORM_TIMES_TWO]	=  vm[V_DENORM] + vm[V_DENORM];
	vm[V_DEGREES_DENORM]	=  deFloatDegrees(vm[V_DENORM]);

	float e = std::numeric_limits<float>::epsilon();
	vm[V_ADD_ARG_A]					= 1.0f + 3 * e;
	vm[V_ADD_ARG_B]					= 1.0f;
	vm[V_SUB_ARG_A]					= vm[V_ADD_ARG_A];
	vm[V_SUB_ARG_B]					= 3.0f + 6 * e;
	vm[V_MUL_ARG_A]					= vm[V_ADD_ARG_A];
	vm[V_MUL_ARG_B]					= 5 * e;
	vm[V_DOT_ARG_A]					= vm[V_ADD_ARG_A];
	vm[V_DOT_ARG_B]					= 5 * e;
	vm[V_CONV_FROM_FP32_ARG]		= 1.22334445f;
	vm[V_CONV_FROM_FP64_ARG]		= vm[V_UNUSED];

	int prevRound = fegetround();
	fesetround(FE_TOWARDZERO);
	vm[V_ADD_RTZ_RESULT]			= vm[V_ADD_ARG_A] + vm[V_ADD_ARG_B];
	vm[V_SUB_RTZ_RESULT]			= vm[V_SUB_ARG_A] - vm[V_SUB_ARG_B];
	vm[V_MUL_RTZ_RESULT]			= vm[V_MUL_ARG_A] * vm[V_MUL_ARG_B];
	vm[V_DOT_RTZ_RESULT]			= vm[V_MUL_RTZ_RESULT] + vm[V_MUL_RTZ_RESULT];
	vm[V_CONV_TO_FP16_RTZ_RESULT]	= vm[V_UNUSED];
	vm[V_CONV_TO_FP32_RTZ_RESULT]	= exactByteEquivalent<deUint32>(0x3f9c968d); // result of conversion from double(1.22334455)

	fesetround(FE_TONEAREST);
	vm[V_ADD_RTE_RESULT]			= vm[V_ADD_ARG_A] + vm[V_ADD_ARG_B];
	vm[V_SUB_RTE_RESULT]			= vm[V_SUB_ARG_A] - vm[V_SUB_ARG_B];
	vm[V_MUL_RTE_RESULT]			= vm[V_MUL_ARG_A] * vm[V_MUL_ARG_B];
	vm[V_DOT_RTE_RESULT]			= vm[V_MUL_RTE_RESULT] + vm[V_MUL_RTE_RESULT];
	vm[V_CONV_TO_FP16_RTE_RESULT]	= vm[V_UNUSED];
	vm[V_CONV_TO_FP32_RTE_RESULT]	= exactByteEquivalent<deUint32>(0x3f9c968e); // result of conversion from double(1.22334455)
	fesetround(prevRound);

	// there is no precision to store fp64 denorm
	vm[V_CONV_DENORM_SMALLER]		= exactByteEquivalent<deUint32>(0x387c0000); // fp16 denorm
	vm[V_CONV_DENORM_BIGGER]		= vm[V_ZERO];
}

template <>
TypeValues<double>::TypeValues()
	: TypeValuesBase()
{
	// NOTE: when updating entries in m_valueIdToFloatType make sure to
	// update also valueIdToSnippetArgMap defined in updateSpirvSnippets()
	ValueMap& vm = m_valueIdToFloatType;
	vm[V_UNUSED]			=  0.0;
	vm[V_MINUS_INF]			= -std::numeric_limits<double>::infinity();
	vm[V_MINUS_ONE]			= -1.0;
	vm[V_MINUS_ZERO]		= -0.0;
	vm[V_ZERO]				=  0.0;
	vm[V_HALF]				=  0.5;
	vm[V_ONE]				=  1.0;
	vm[V_INF]				=  std::numeric_limits<double>::infinity();
	vm[V_DENORM]			=  4.98e-321; // 0x00000000000003F0
	vm[V_NAN]				=  std::numeric_limits<double>::quiet_NaN();

	vm[V_PI_DIV_2]			=  pi / 2;
	vm[V_DENORM_TIMES_TWO]	=  vm[V_DENORM] + vm[V_DENORM];
	vm[V_DEGREES_DENORM]	=  vm[V_UNUSED];

	double e = std::numeric_limits<double>::epsilon();
	vm[V_ADD_ARG_A]				= 1.0 + 3 * e;
	vm[V_ADD_ARG_B]				= 1.0;
	vm[V_SUB_ARG_A]				= vm[V_ADD_ARG_A];
	vm[V_SUB_ARG_B]				= 3.0 + 6 * e;
	vm[V_MUL_ARG_A]				= vm[V_ADD_ARG_A];
	vm[V_MUL_ARG_B]				= 5 * e;
	vm[V_DOT_ARG_A]				= vm[V_ADD_ARG_A];
	vm[V_DOT_ARG_B]				= 5 * e;
	vm[V_CONV_FROM_FP32_ARG]	= vm[V_UNUSED];
	vm[V_CONV_FROM_FP64_ARG]	= 1.22334455;

	int prevRound = fegetround();
	fesetround(FE_TOWARDZERO);
	vm[V_ADD_RTZ_RESULT]			= vm[V_ADD_ARG_A] + vm[V_ADD_ARG_B];
	vm[V_SUB_RTZ_RESULT]			= vm[V_SUB_ARG_A] - vm[V_SUB_ARG_B];
	vm[V_MUL_RTZ_RESULT]			= vm[V_MUL_ARG_A] * vm[V_MUL_ARG_B];
	vm[V_DOT_RTZ_RESULT]			= vm[V_MUL_RTZ_RESULT] + vm[V_MUL_RTZ_RESULT];
	vm[V_CONV_TO_FP16_RTZ_RESULT]	= vm[V_UNUSED];
	vm[V_CONV_TO_FP32_RTZ_RESULT]	= vm[V_UNUSED];

	fesetround(FE_TONEAREST);
	vm[V_ADD_RTE_RESULT]			= vm[V_ADD_ARG_A] + vm[V_ADD_ARG_B];
	vm[V_SUB_RTE_RESULT]			= vm[V_SUB_ARG_A] - vm[V_SUB_ARG_B];
	vm[V_MUL_RTE_RESULT]			= vm[V_MUL_ARG_A] * vm[V_MUL_ARG_B];
	vm[V_DOT_RTE_RESULT]			= vm[V_MUL_RTE_RESULT] + vm[V_MUL_RTE_RESULT];
	vm[V_CONV_TO_FP16_RTE_RESULT]	= vm[V_UNUSED];
	vm[V_CONV_TO_FP32_RTE_RESULT]	= vm[V_UNUSED];
	fesetround(prevRound);

	vm[V_CONV_DENORM_SMALLER]		= exactByteEquivalent<deUint64>(0x3f0f800000000000); // 0x03f0 is fp16 denorm
	vm[V_CONV_DENORM_BIGGER]		= exactByteEquivalent<deUint64>(0x373f800000000000); // 0x000003f0 is fp32 denorm
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

	// denormBase is a normal value (found empirically) used to generate denorm value.
	// Denorm is generated by substracting epsilon from denormBase.
	// denormBase is not a denorm - it is used to create denorm.
	// This value is needed when operations are tested with arguments that were
	// generated in the code. Generated denorm should be the same as denorm
	// used when arguments are passed via input (m_valueIdToFloatType[V_DENORM]).
	// This is required as result of some operations depends on actual denorm value
	// e.g. OpRadians(0x0001) is 0 but OpRadians(0x03f0) is denorm.
	string denormBase;

	string capabilities;
	string extensions;
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
};

void TypeSnippetsBase::updateSpirvSnippets()
{
	// annotations to types that are commonly used by tests
	const string typeAnnotationsTemplate =
		"OpDecorate %type_float_arr_1 ArrayStride " + arrayStride + "\n"
		"OpDecorate %type_float_arr_2 ArrayStride " + arrayStride + "\n";

	// definition off all types that are commonly used by tests
	const string typeDefinitionsTemplate =
		"%type_float             = OpTypeFloat " + bitWidth + "\n"
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
	const string minTypeDefinitionsTemplate =
		"%type_float             = OpTypeFloat " + bitWidth + "\n"
		"%type_float_uptr        = OpTypePointer Uniform %type_float\n"
		"%type_float_arr_2       = OpTypeArray %type_float %c_i32_2\n";

	// definition off all constants that are used by tests
	const string constantsDefinitionsTemplate =
		"%c_float_n1             = OpConstant %type_float -1\n"
		"%c_float_0              = OpConstant %type_float 0.0\n"
		"%c_float_0_5            = OpConstant %type_float 0.5\n"
		"%c_float_1              = OpConstant %type_float 1\n"
		"%c_float_2              = OpConstant %type_float 2\n"
		"%c_float_3              = OpConstant %type_float 3\n"
		"%c_float_4              = OpConstant %type_float 4\n"
		"%c_float_5              = OpConstant %type_float 5\n"
		"%c_float_6              = OpConstant %type_float 6\n"
		"%c_float_eps            = OpConstant %type_float " + epsilon + "\n"
		"%c_float_denorm_base    = OpConstant %type_float " + denormBase + "\n";

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
	inputAnnotationsSnippet =
		"OpMemberDecorate %SSBO_in 0 Offset 0\n"
		"OpDecorate %SSBO_in BufferBlock\n"
		"OpDecorate %ssbo_in DescriptorSet 0\n"
		"OpDecorate %ssbo_in Binding 0\n"
		"OpDecorate %ssbo_in NonWritable\n";

	const string inputDefinitionsTemplate =
		"%SSBO_in              = OpTypeStruct %type_float_arr_2\n"
		"%up_SSBO_in           = OpTypePointer Uniform %SSBO_in\n"
		"%ssbo_in              = OpVariable %up_SSBO_in Uniform\n";

	outputAnnotationsSnippet =
		"OpMemberDecorate %SSBO_out 0 Offset 0\n"
		"OpDecorate %SSBO_out BufferBlock\n"
		"OpDecorate %ssbo_out DescriptorSet 0\n"
		"OpDecorate %ssbo_out Binding 1\n";

	const string multiOutputAnnotationsTemplate =
		"OpMemberDecorate %SSBO_float_out 0 Offset 0\n"
		"OpDecorate %type_float_arr_2 ArrayStride "+ arrayStride + "\n"
		"OpDecorate %SSBO_float_out BufferBlock\n"
		"OpDecorate %ssbo_float_out DescriptorSet 0\n";

	const string outputDefinitionsTemplate =
		"%SSBO_out             = OpTypeStruct %type_float_arr_1\n"
		"%up_SSBO_out          = OpTypePointer Uniform %SSBO_out\n"
		"%ssbo_out             = OpVariable %up_SSBO_out Uniform\n";

	const string multiOutputDefinitionsTemplate =
		"%SSBO_float_out         = OpTypeStruct %type_float\n"
		"%up_SSBO_float_out      = OpTypePointer Uniform %SSBO_float_out\n"
		"%ssbo_float_out         = OpVariable %up_SSBO_float_out Uniform\n";

	// this snippet is used by compute and fragment stage but not by vertex stage
	const string storeResultsTemplate =
		"%outloc               = OpAccessChain %type_float_uptr %ssbo_out %c_i32_0 %c_i32_0\n"
		"OpStore %outloc %result\n";

	const string multiStoreResultsTemplate =
		"%outloc" + bitWidth + "             = OpAccessChain %type_float_uptr %ssbo_float_out %c_i32_0\n"
		"                        OpStore %outloc" + bitWidth + " %result" + bitWidth + "\n";

	const string typeToken	= "_float";
	const string typeName	= "_f" + bitWidth;

	typeAnnotationsSnippet			= replace(typeAnnotationsTemplate, typeToken, typeName);
	typeDefinitionsSnippet			= replace(typeDefinitionsTemplate, typeToken, typeName);
	minTypeDefinitionsSnippet		= replace(minTypeDefinitionsTemplate, typeToken, typeName);
	constantsDefinitionsSnippet		= replace(constantsDefinitionsTemplate, typeToken, typeName);
	argumentsFromInputSnippet		= replace(argumentsFromInputTemplate, typeToken, typeName);
	multiArgumentsFromInputSnippet	= replace(multiArgumentsFromInputTemplate, typeToken, typeName);
	inputDefinitionsSnippet			= replace(inputDefinitionsTemplate, typeToken, typeName);
	multiOutputAnnotationsSnippet	= replace(multiOutputAnnotationsTemplate, typeToken, typeName);
	outputDefinitionsSnippet		= replace(outputDefinitionsTemplate, typeToken, typeName);
	multiOutputDefinitionsSnippet	= replace(multiOutputDefinitionsTemplate, typeToken, typeName);
	storeResultsSnippet				= replace(storeResultsTemplate, typeToken, typeName);
	multiStoreResultsSnippet		= replace(multiStoreResultsTemplate, typeToken, typeName);

	// NOTE: only values used as _generated_ arguments in test operations
	// need to be in this map, arguments that are only used by tests,
	// that grab arguments from input, do need to be in this map
	// NOTE: when updating entries in valueIdToSnippetArgMap make
	// sure to update also m_valueIdToFloatType for all float width
	SnippetMap& sm = valueIdToSnippetArgMap;
	sm[V_UNUSED]		= "OpFSub %type_float %c_float_0 %c_float_0\n";
	sm[V_MINUS_INF]		= "OpFDiv %type_float %c_float_n1 %c_float_0\n";
	sm[V_MINUS_ONE]		= "OpFAdd %type_float %c_float_n1 %c_float_0\n";
	sm[V_MINUS_ZERO]	= "OpFMul %type_float %c_float_n1 %c_float_0\n";
	sm[V_ZERO]			= "OpFMul %type_float %c_float_0 %c_float_0\n";
	sm[V_HALF]			= "OpFAdd %type_float %c_float_0_5 %c_float_0\n";
	sm[V_ONE]			= "OpFAdd %type_float %c_float_1 %c_float_0\n";
	sm[V_INF]			= "OpFDiv %type_float %c_float_1 %c_float_0\n";					// x / 0		== Inf
	sm[V_DENORM]		= "OpFSub %type_float %c_float_denorm_base %c_float_eps\n";
	sm[V_NAN]			= "OpFDiv %type_float %c_float_0 %c_float_0\n";					// 0 / 0		== Nan

	map<ValueId, string>::iterator it;
	for ( it = sm.begin(); it != sm.end(); it++ )
		sm[it->first] = replace(it->second, typeToken, typeName);
}

typedef de::SharedPtr<TypeSnippetsBase> TypeSnippetsSP;

template<typename FLOAT_TYPE>
class TypeSnippets: public TypeSnippetsBase
{
public:
	TypeSnippets();
};

template<>
TypeSnippets<deFloat16>::TypeSnippets()
{
	bitWidth		= "16";
	epsilon			= "6.104e-5";	// 2^-14 = 0x0400

	// 1.2113e-4 is 0x07f0 which after substracting epsilon will give 0x03f0 (same as vm[V_DENORM])
	// NOTE: constants in SPIR-V cant be specified as exact fp16 - there is conversion from double to fp16
	denormBase		= "1.2113e-4";

	capabilities	= "OpCapability StorageUniform16\n";
	extensions		= "OpExtension \"SPV_KHR_16bit_storage\"\n";
	arrayStride		= "2";

	varyingsTypesSnippet =
					"%type_u32_iptr        = OpTypePointer Input %type_u32\n"
					"%type_u32_optr        = OpTypePointer Output %type_u32\n";
	inputVaryingsSnippet =
					"%BP_vertex_result    = OpVariable %type_u32_iptr Input\n";
	outputVaryingsSnippet =
					"%BP_vertex_result    = OpVariable %type_u32_optr Output\n";
	storeVertexResultSnippet =
					"%tmp_vec2            = OpCompositeConstruct %type_f16_vec2 %result %c_f16_0\n"
					"%packed_result       = OpBitcast %type_u32 %tmp_vec2\n"
					"OpStore %BP_vertex_result %packed_result\n";
	loadVertexResultSnippet =
					"%packed_result       = OpLoad %type_u32 %BP_vertex_result\n"
					"%tmp_vec2            = OpBitcast %type_f16_vec2 %packed_result\n"
					"%result              = OpCompositeExtract %type_f16 %tmp_vec2 0\n";

	loadStoreRequiresShaderFloat16 = true;

	updateSpirvSnippets();
}

template<>
TypeSnippets<float>::TypeSnippets()
{
	bitWidth		= "32";
	epsilon			= "1.175494351e-38";
	denormBase		= "1.1756356e-38";
	capabilities	= "";
	extensions		= "";
	arrayStride		= "4";

	varyingsTypesSnippet =
					"%type_u32_iptr        = OpTypePointer Input %type_u32\n"
					"%type_u32_optr        = OpTypePointer Output %type_u32\n";
	inputVaryingsSnippet =
					"%BP_vertex_result    = OpVariable %type_u32_iptr Input\n";
	outputVaryingsSnippet =
					"%BP_vertex_result    = OpVariable %type_u32_optr Output\n";
	storeVertexResultSnippet =
					"%packed_result       = OpBitcast %type_u32 %result\n"
					"OpStore %BP_vertex_result %packed_result\n";
	loadVertexResultSnippet =
					"%packed_result       = OpLoad %type_u32 %BP_vertex_result\n"
					"%result              = OpBitcast %type_f32 %packed_result\n";

	loadStoreRequiresShaderFloat16 = false;

	updateSpirvSnippets();
}

template<>
TypeSnippets<double>::TypeSnippets()
{
	bitWidth		= "64";
	epsilon			= "2.2250738585072014e-308"; // 0x0010000000000000
	denormBase		= "2.2250738585076994e-308"; // 0x00100000000003F0
	capabilities	= "OpCapability Float64\n";
	extensions		= "";
	arrayStride		= "8";

	varyingsTypesSnippet =
					"%type_u32_vec2_iptr   = OpTypePointer Input %type_u32_vec2\n"
					"%type_u32_vec2_optr   = OpTypePointer Output %type_u32_vec2\n";
	inputVaryingsSnippet =
					"%BP_vertex_result     = OpVariable %type_u32_vec2_iptr Input\n";
	outputVaryingsSnippet =
					"%BP_vertex_result     = OpVariable %type_u32_vec2_optr Output\n";
	storeVertexResultSnippet =
					"%packed_result        = OpBitcast %type_u32_vec2 %result\n"
					"OpStore %BP_vertex_result %packed_result\n";
	loadVertexResultSnippet =
					"%packed_result        = OpLoad %type_u32_vec2 %BP_vertex_result\n"
					"%result               = OpBitcast %type_f64 %packed_result\n";

	loadStoreRequiresShaderFloat16 = false;

	updateSpirvSnippets();
}

class TypeTestResultsBase
{
public:
	virtual ~TypeTestResultsBase() {}
	FloatType floatType() const;

protected:
	FloatType m_floatType;

public:
	// Vectors containing test data for float controls
	vector<BinaryCase>	binaryOpFTZ;
	vector<UnaryCase>	unaryOpFTZ;
	vector<BinaryCase>	binaryOpDenormPreserve;
	vector<UnaryCase>	unaryOpDenormPreserve;
};

FloatType TypeTestResultsBase::floatType() const
{
	return m_floatType;
}

typedef de::SharedPtr<TypeTestResultsBase> TypeTestResultsSP;

template<typename FLOAT_TYPE>
class TypeTestResults: public TypeTestResultsBase
{
public:
	TypeTestResults();
};

template<>
TypeTestResults<deFloat16>::TypeTestResults()
{
	m_floatType = FP16;

	// note: there are many FTZ test cases that can produce diferent result depending
	// on input denorm being flushed or not; because of that FTZ tests can be limited
	// to those that return denorm as those are the ones affected by tested extension
	const BinaryCase binaryOpFTZArr[] = {
		//operation		den op one		den op den		den op inf		den op nan
		{ O_ADD,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_SUB,		V_MINUS_ONE,	V_ZERO,			V_MINUS_INF,	V_UNUSED },
		{ O_MUL,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_DIV,		V_ZERO,			V_UNUSED,		V_ZERO,			V_UNUSED },
		{ O_REM,		V_ZERO,			V_UNUSED,		V_UNUSED,		V_UNUSED },
		{ O_MOD,		V_ZERO,			V_UNUSED,		V_UNUSED,		V_UNUSED },
		{ O_VEC_MUL_S,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_VEC_MUL_M,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_S,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_V,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_M,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_OUT_PROD,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_DOT,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_ATAN2,		V_ZERO,			V_UNUSED,		V_ZERO,			V_UNUSED },
		{ O_POW,		V_ZERO,			V_UNUSED,		V_ZERO,			V_UNUSED },
		{ O_MIX,		V_HALF,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_MIN,		V_ZERO,			V_ZERO,			V_ZERO,			V_UNUSED },
		{ O_MAX,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_CLAMP,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_STEP,		V_ONE,			V_ONE,			V_ONE,			V_UNUSED },
		{ O_SSTEP,		V_HALF,			V_ONE,			V_ZERO,			V_UNUSED },
		{ O_FMA,		V_HALF,			V_HALF,			V_UNUSED,		V_UNUSED },
		{ O_FACE_FWD,	V_MINUS_ONE,	V_MINUS_ONE,	V_MINUS_ONE,	V_MINUS_ONE },
		{ O_NMIN,		V_ZERO,			V_ZERO,			V_ZERO,			V_ZERO },
		{ O_NMAX,		V_ONE,			V_ZERO,			V_INF,			V_ZERO },
		{ O_NCLAMP,		V_ONE,			V_ZERO,			V_INF,			V_ZERO },
		{ O_DIST,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_CROSS,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
	};

	const UnaryCase unaryOpFTZArr[] = {
		//operation			op den
		{ O_NEGATE,			V_MINUS_ZERO },
		{ O_ROUND,			V_ZERO },
		{ O_ROUND_EV,		V_ZERO },
		{ O_TRUNC,			V_ZERO },
		{ O_ABS,			V_ZERO },
		{ O_FLOOR,			V_ZERO },
		{ O_CEIL,			V_ZERO_OR_ONE },
		{ O_FRACT,			V_ZERO },
		{ O_RADIANS,		V_ZERO },
		{ O_DEGREES,		V_ZERO },
		{ O_SIN,			V_ZERO },
		{ O_COS,			V_TRIG_ONE },
		{ O_TAN,			V_ZERO },
		{ O_ASIN,			V_ZERO },
		{ O_ACOS,			V_PI_DIV_2 },
		{ O_ATAN,			V_ZERO },
		{ O_SINH,			V_ZERO },
		{ O_COSH,			V_ONE },
		{ O_TANH,			V_ZERO },
		{ O_ASINH,			V_ZERO },
		{ O_ACOSH,			V_UNUSED },
		{ O_ATANH,			V_ZERO },
		{ O_EXP,			V_ONE },
		{ O_LOG,			V_MINUS_INF_OR_LOG_DENORM },
		{ O_EXP2,			V_ONE },
		{ O_LOG2,			V_MINUS_INF_OR_LOG2_DENORM },
		{ O_SQRT,			V_ZERO_OR_SQRT_DENORM },
		{ O_INV_SQRT,		V_INF_OR_INV_SQRT_DENORM },
		{ O_MAT_DET,		V_ZERO },
		{ O_MAT_INV,		V_ZERO_OR_MINUS_ZERO },
		{ O_MODF,			V_ZERO },
		{ O_MODF_ST,		V_ZERO },
		{ O_NORMALIZE,		V_ZERO },
		{ O_REFLECT,		V_ZERO },
		{ O_REFRACT,		V_ZERO },
		{ O_LENGHT,			V_ZERO },
	};

	const BinaryCase binaryOpDenormPreserveArr[] = {
		//operation			den op one				den op den				den op inf		den op nan
		{ O_PHI,			V_DENORM,				V_DENORM,				V_DENORM,		V_DENORM },
		{ O_SELECT,			V_DENORM,				V_DENORM,				V_DENORM,		V_DENORM },
		{ O_ADD,			V_ONE,					V_DENORM_TIMES_TWO,		V_INF,			V_NAN },
		{ O_SUB,			V_MINUS_ONE_OR_CLOSE,	V_ZERO,					V_MINUS_INF,	V_NAN },
		{ O_MUL,			V_DENORM,				V_ZERO,					V_INF,			V_NAN },
		{ O_VEC_MUL_S,		V_DENORM,				V_ZERO,					V_INF,			V_NAN },
		{ O_VEC_MUL_M,		V_DENORM_TIMES_TWO,		V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_S,		V_DENORM,				V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_V,		V_DENORM_TIMES_TWO,		V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_M,		V_DENORM_TIMES_TWO,		V_ZERO,					V_INF,			V_NAN },
		{ O_OUT_PROD,		V_DENORM,				V_ZERO,					V_INF,			V_NAN },
		{ O_DOT,			V_DENORM_TIMES_TWO,		V_ZERO,					V_INF,			V_NAN },
		{ O_MIX,			V_HALF,					V_DENORM,				V_INF,			V_NAN },
		{ O_FMA,			V_HALF,					V_HALF,					V_INF,			V_NAN },
		{ O_MIN,			V_DENORM,				V_DENORM,				V_DENORM,		V_UNUSED },
		{ O_MAX,			V_ONE,					V_DENORM,				V_INF,			V_UNUSED },
		{ O_CLAMP,			V_ONE,					V_DENORM,				V_INF,			V_UNUSED },
		{ O_NMIN,			V_DENORM,				V_DENORM,				V_DENORM,		V_DENORM },
		{ O_NMAX,			V_ONE,					V_DENORM,				V_INF,			V_DENORM },
		{ O_NCLAMP,			V_ONE,					V_DENORM,				V_INF,			V_DENORM },
	};

	const UnaryCase unaryOpDenormPreserveArr[] = {
		//operation			op den
		{ O_RETURN_VAL,		V_DENORM },
		{ O_D_EXTRACT,		V_DENORM },
		{ O_D_INSERT,		V_DENORM },
		{ O_SHUFFLE,		V_DENORM },
		{ O_COMPOSITE,		V_DENORM },
		{ O_COMPOSITE_INS,	V_DENORM },
		{ O_COPY,			V_DENORM },
		{ O_TRANSPOSE,		V_DENORM },
		{ O_NEGATE,			V_DENORM },
		{ O_ABS,			V_DENORM },
		{ O_SIGN,			V_ONE },
		{ O_RADIANS,		V_DENORM },
		{ O_DEGREES,		V_DEGREES_DENORM },
	};

	binaryOpFTZ.insert(binaryOpFTZ.begin(), binaryOpFTZArr,
					   binaryOpFTZArr + DE_LENGTH_OF_ARRAY(binaryOpFTZArr));
	unaryOpFTZ.insert(unaryOpFTZ.begin(), unaryOpFTZArr,
					  unaryOpFTZArr + DE_LENGTH_OF_ARRAY(unaryOpFTZArr));
	binaryOpDenormPreserve.insert(binaryOpDenormPreserve.begin(), binaryOpDenormPreserveArr,
								  binaryOpDenormPreserveArr + DE_LENGTH_OF_ARRAY(binaryOpDenormPreserveArr));
	unaryOpDenormPreserve.insert(unaryOpDenormPreserve.begin(), unaryOpDenormPreserveArr,
								 unaryOpDenormPreserveArr + DE_LENGTH_OF_ARRAY(unaryOpDenormPreserveArr));
}

template<>
TypeTestResults<float>::TypeTestResults()
{
	m_floatType = FP32;

	const BinaryCase binaryOpFTZArr[] = {
		//operation		den op one		den op den		den op inf		den op nan
		{ O_ADD,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_SUB,		V_MINUS_ONE,	V_ZERO,			V_MINUS_INF,	V_UNUSED },
		{ O_MUL,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_DIV,		V_ZERO,			V_UNUSED,		V_ZERO,			V_UNUSED },
		{ O_REM,		V_ZERO,			V_UNUSED,		V_UNUSED,		V_UNUSED },
		{ O_MOD,		V_ZERO,			V_UNUSED,		V_UNUSED,		V_UNUSED },
		{ O_VEC_MUL_S,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_VEC_MUL_M,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_S,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_V,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_M,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_OUT_PROD,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_DOT,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_ATAN2,		V_ZERO,			V_UNUSED,		V_ZERO,			V_UNUSED },
		{ O_POW,		V_ZERO,			V_UNUSED,		V_ZERO,			V_UNUSED },
		{ O_MIX,		V_HALF,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_MIN,		V_ZERO,			V_ZERO,			V_ZERO,			V_UNUSED },
		{ O_MAX,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_CLAMP,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_STEP,		V_ONE,			V_ONE,			V_ONE,			V_UNUSED },
		{ O_SSTEP,		V_HALF,			V_ONE,			V_ZERO,			V_UNUSED },
		{ O_FMA,		V_HALF,			V_HALF,			V_UNUSED,		V_UNUSED },
		{ O_FACE_FWD,	V_MINUS_ONE,	V_MINUS_ONE,	V_MINUS_ONE,	V_MINUS_ONE },
		{ O_NMIN,		V_ZERO,			V_ZERO,			V_ZERO,			V_ZERO },
		{ O_NMAX,		V_ONE,			V_ZERO,			V_INF,			V_ZERO },
		{ O_NCLAMP,		V_ONE,			V_ZERO,			V_INF,			V_ZERO },
		{ O_DIST,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_CROSS,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
	};

	const UnaryCase unaryOpFTZArr[] = {
		//operation			op den
		{ O_NEGATE,			V_MINUS_ZERO },
		{ O_ROUND,			V_ZERO },
		{ O_ROUND_EV,		V_ZERO },
		{ O_TRUNC,			V_ZERO },
		{ O_ABS,			V_ZERO },
		{ O_FLOOR,			V_ZERO },
		{ O_CEIL,			V_ZERO_OR_ONE },
		{ O_FRACT,			V_ZERO },
		{ O_RADIANS,		V_ZERO },
		{ O_DEGREES,		V_ZERO },
		{ O_SIN,			V_ZERO },
		{ O_COS,			V_TRIG_ONE },
		{ O_TAN,			V_ZERO },
		{ O_ASIN,			V_ZERO },
		{ O_ACOS,			V_PI_DIV_2 },
		{ O_ATAN,			V_ZERO },
		{ O_SINH,			V_ZERO },
		{ O_COSH,			V_ONE },
		{ O_TANH,			V_ZERO },
		{ O_ASINH,			V_ZERO },
		{ O_ACOSH,			V_UNUSED },
		{ O_ATANH,			V_ZERO },
		{ O_EXP,			V_ONE },
		{ O_LOG,			V_MINUS_INF_OR_LOG_DENORM },
		{ O_EXP2,			V_ONE },
		{ O_LOG2,			V_MINUS_INF_OR_LOG2_DENORM },
		{ O_SQRT,			V_ZERO_OR_SQRT_DENORM },
		{ O_INV_SQRT,		V_INF_OR_INV_SQRT_DENORM },
		{ O_MAT_DET,		V_ZERO },
		{ O_MAT_INV,		V_ZERO_OR_MINUS_ZERO },
		{ O_MODF,			V_ZERO },
		{ O_MODF_ST,		V_ZERO },
		{ O_NORMALIZE,		V_ZERO },
		{ O_REFLECT,		V_ZERO },
		{ O_REFRACT,		V_ZERO },
		{ O_LENGHT,			V_ZERO },
	};

	const BinaryCase binaryOpDenormPreserveArr[] = {
		//operation			den op one			den op den				den op inf		den op nan
		{ O_PHI,			V_DENORM,			V_DENORM,				V_DENORM,		V_DENORM },
		{ O_SELECT,			V_DENORM,			V_DENORM,				V_DENORM,		V_DENORM },
		{ O_ADD,			V_ONE,				V_DENORM_TIMES_TWO,		V_INF,			V_NAN },
		{ O_SUB,			V_MINUS_ONE,		V_ZERO,					V_MINUS_INF,	V_NAN },
		{ O_MUL,			V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_VEC_MUL_S,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_VEC_MUL_M,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_S,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_V,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_M,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_OUT_PROD,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_DOT,			V_DENORM_TIMES_TWO,	V_ZERO,					V_INF,			V_NAN },
		{ O_MIX,			V_HALF,				V_DENORM,				V_INF,			V_NAN },
		{ O_FMA,			V_HALF,				V_HALF,					V_INF,			V_NAN },
		{ O_MIN,			V_DENORM,			V_DENORM,				V_DENORM,		V_UNUSED },
		{ O_MAX,			V_ONE,				V_DENORM,				V_INF,			V_UNUSED },
		{ O_CLAMP,			V_ONE,				V_DENORM,				V_INF,			V_UNUSED },
		{ O_NMIN,			V_DENORM,			V_DENORM,				V_DENORM,		V_DENORM },
		{ O_NMAX,			V_ONE,				V_DENORM,				V_INF,			V_DENORM },
		{ O_NCLAMP,			V_ONE,				V_DENORM,				V_INF,			V_DENORM },
	};

	const UnaryCase unaryOpDenormPreserveArr[] = {
		//operation			op den
		{ O_RETURN_VAL,		V_DENORM },
		{ O_D_EXTRACT,		V_DENORM },
		{ O_D_INSERT,		V_DENORM },
		{ O_SHUFFLE,		V_DENORM },
		{ O_COMPOSITE,		V_DENORM },
		{ O_COMPOSITE_INS,	V_DENORM },
		{ O_COPY,			V_DENORM },
		{ O_TRANSPOSE,		V_DENORM },
		{ O_NEGATE,			V_DENORM },
		{ O_ABS,			V_DENORM },
		{ O_SIGN,			V_ONE },
		{ O_RADIANS,		V_DENORM },
		{ O_DEGREES,		V_DEGREES_DENORM },
	};

	binaryOpFTZ.insert(binaryOpFTZ.begin(), binaryOpFTZArr,
					   binaryOpFTZArr + DE_LENGTH_OF_ARRAY(binaryOpFTZArr));
	unaryOpFTZ.insert(unaryOpFTZ.begin(), unaryOpFTZArr,
					  unaryOpFTZArr + DE_LENGTH_OF_ARRAY(unaryOpFTZArr));
	binaryOpDenormPreserve.insert(binaryOpDenormPreserve.begin(), binaryOpDenormPreserveArr,
								  binaryOpDenormPreserveArr + DE_LENGTH_OF_ARRAY(binaryOpDenormPreserveArr));
	unaryOpDenormPreserve.insert(unaryOpDenormPreserve.begin(), unaryOpDenormPreserveArr,
								 unaryOpDenormPreserveArr + DE_LENGTH_OF_ARRAY(unaryOpDenormPreserveArr));
}

template<>
TypeTestResults<double>::TypeTestResults()
{
	m_floatType = FP64;

	// fp64 is supported by fewer operations then fp16 and fp32
	// e.g. Radians and Degrees functions are not supported
	const BinaryCase binaryOpFTZArr[] = {
		//operation		den op one		den op den		den op inf		den op nan
		{ O_ADD,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_SUB,		V_MINUS_ONE,	V_ZERO,			V_MINUS_INF,	V_UNUSED },
		{ O_MUL,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_DIV,		V_ZERO,			V_UNUSED,		V_ZERO,			V_UNUSED },
		{ O_REM,		V_ZERO,			V_UNUSED,		V_UNUSED,		V_UNUSED },
		{ O_MOD,		V_ZERO,			V_UNUSED,		V_UNUSED,		V_UNUSED },
		{ O_VEC_MUL_S,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_VEC_MUL_M,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_S,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_V,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MAT_MUL_M,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_OUT_PROD,	V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_DOT,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
		{ O_MIX,		V_HALF,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_MIN,		V_ZERO,			V_ZERO,			V_ZERO,			V_UNUSED },
		{ O_MAX,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_CLAMP,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_STEP,		V_ONE,			V_ONE,			V_ONE,			V_UNUSED },
		{ O_SSTEP,		V_HALF,			V_ONE,			V_ZERO,			V_UNUSED },
		{ O_FMA,		V_HALF,			V_HALF,			V_UNUSED,		V_UNUSED },
		{ O_FACE_FWD,	V_MINUS_ONE,	V_MINUS_ONE,	V_MINUS_ONE,	V_MINUS_ONE },
		{ O_NMIN,		V_ZERO,			V_ZERO,			V_ZERO,			V_ZERO },
		{ O_NMAX,		V_ONE,			V_ZERO,			V_INF,			V_ZERO },
		{ O_NCLAMP,		V_ONE,			V_ZERO,			V_INF,			V_ZERO },
		{ O_DIST,		V_ONE,			V_ZERO,			V_INF,			V_UNUSED },
		{ O_CROSS,		V_ZERO,			V_ZERO,			V_UNUSED,		V_UNUSED },
	};

	const UnaryCase unaryOpFTZArr[] = {
		//operation			op den
		{ O_NEGATE,			V_MINUS_ZERO },
		{ O_ROUND,			V_ZERO },
		{ O_ROUND_EV,		V_ZERO },
		{ O_TRUNC,			V_ZERO },
		{ O_ABS,			V_ZERO },
		{ O_FLOOR,			V_ZERO },
		{ O_CEIL,			V_ZERO_OR_ONE },
		{ O_FRACT,			V_ZERO },
		{ O_SQRT,			V_ZERO_OR_SQRT_DENORM },
		{ O_INV_SQRT,		V_INF_OR_INV_SQRT_DENORM },
		{ O_MAT_DET,		V_ZERO },
		{ O_MAT_INV,		V_ZERO_OR_MINUS_ZERO },
		{ O_MODF,			V_ZERO },
		{ O_MODF_ST,		V_ZERO },
		{ O_NORMALIZE,		V_ZERO },
		{ O_REFLECT,		V_ZERO },
		{ O_LENGHT,			V_ZERO },
	};

	const BinaryCase binaryOpDenormPreserveArr[] = {
		//operation			den op one			den op den				den op inf		den op nan
		{ O_PHI,			V_DENORM,			V_DENORM,				V_DENORM,		V_DENORM },
		{ O_SELECT,			V_DENORM,			V_DENORM,				V_DENORM,		V_DENORM },
		{ O_ADD,			V_ONE,				V_DENORM_TIMES_TWO,		V_INF,			V_NAN },
		{ O_SUB,			V_MINUS_ONE,		V_ZERO,					V_MINUS_INF,	V_NAN },
		{ O_MUL,			V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_VEC_MUL_S,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_VEC_MUL_M,		V_DENORM_TIMES_TWO,	V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_S,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_V,		V_DENORM_TIMES_TWO,	V_ZERO,					V_INF,			V_NAN },
		{ O_MAT_MUL_M,		V_DENORM_TIMES_TWO,	V_ZERO,					V_INF,			V_NAN },
		{ O_OUT_PROD,		V_DENORM,			V_ZERO,					V_INF,			V_NAN },
		{ O_DOT,			V_DENORM_TIMES_TWO,	V_ZERO,					V_INF,			V_NAN },
		{ O_MIX,			V_HALF,				V_DENORM,				V_INF,			V_NAN },
		{ O_FMA,			V_HALF,				V_HALF,					V_INF,			V_NAN },
		{ O_MIN,			V_DENORM,			V_DENORM,				V_DENORM,		V_UNUSED },
		{ O_MAX,			V_ONE,				V_DENORM,				V_INF,			V_UNUSED },
		{ O_CLAMP,			V_ONE,				V_DENORM,				V_INF,			V_UNUSED },
		{ O_NMIN,			V_DENORM,			V_DENORM,				V_DENORM,		V_DENORM },
		{ O_NMAX,			V_ONE,				V_DENORM,				V_INF,			V_DENORM },
		{ O_NCLAMP,			V_ONE,				V_DENORM,				V_INF,			V_DENORM },
	};

	const UnaryCase unaryOpDenormPreserveArr[] = {
		//operation			op den
		{ O_RETURN_VAL,		V_DENORM },
		{ O_D_EXTRACT,		V_DENORM },
		{ O_D_INSERT,		V_DENORM },
		{ O_SHUFFLE,		V_DENORM },
		{ O_COMPOSITE,		V_DENORM },
		{ O_COMPOSITE_INS,	V_DENORM },
		{ O_COPY,			V_DENORM },
		{ O_TRANSPOSE,		V_DENORM },
		{ O_NEGATE,			V_DENORM },
		{ O_ABS,			V_DENORM },
		{ O_SIGN,			V_ONE },
	};

	binaryOpFTZ.insert(binaryOpFTZ.begin(), binaryOpFTZArr,
					   binaryOpFTZArr + DE_LENGTH_OF_ARRAY(binaryOpFTZArr));
	unaryOpFTZ.insert(unaryOpFTZ.begin(), unaryOpFTZArr,
					  unaryOpFTZArr + DE_LENGTH_OF_ARRAY(unaryOpFTZArr));
	binaryOpDenormPreserve.insert(binaryOpDenormPreserve.begin(), binaryOpDenormPreserveArr,
								  binaryOpDenormPreserveArr + DE_LENGTH_OF_ARRAY(binaryOpDenormPreserveArr));
	unaryOpDenormPreserve.insert(unaryOpDenormPreserve.begin(), unaryOpDenormPreserveArr,
								 unaryOpDenormPreserveArr + DE_LENGTH_OF_ARRAY(unaryOpDenormPreserveArr));
}

// Operation structure holds data needed to test specified SPIR-V operation. This class contains
// additional annotations, additional types and aditional constants that should be properly included
// in SPIR-V code. Commands attribute in this structure contains code that performs tested operation
// on given arguments, in some cases verification is also performed there.
// All snipets stroed in this structure are generic and can be specialized for fp16, fp32 or fp64,
// thanks to that this data can be shared by many OperationTestCase instances (testing diferent
// float behaviours on diferent float widths).
struct Operation
{
	// operation name is included in test case name
	const char*	name;

	// How extensively is the floating point type used?
	FloatUsage floatUsage;

	// operation specific spir-v snippets that will be
	// placed in proper places in final test shader
	const char*	annotations;
	const char*	types;
	const char*	constants;
	const char*	variables;
	const char*	functions;
	const char*	commands;

	// conversion operations operate on one float type and produce float
	// type with different bit width; restrictedInputType is used only when
	// isInputTypeRestricted is set to true and it restricts usage of this
	// operation to specified input type
	bool		isInputTypeRestricted;
	FloatType	restrictedInputType;

	// arguments for OpSpecConstant need to be specified also as constant
	bool		isSpecConstant;

	// set if c_float* constant is used in operation
	FloatStatementUsageFlags	statementUsageFlags;

	Operation()		{}

	// Minimal constructor - used by most of operations
	Operation(const char* _name, FloatUsage _floatUsage, const char* _commands, const FloatStatementUsageFlags _statementUsageFlags = 0)
		: name(_name)
		, floatUsage(_floatUsage)
		, annotations("")
		, types("")
		, constants("")
		, variables("")
		, functions("")
		, commands(_commands)
		, isInputTypeRestricted(false)
		, restrictedInputType(FP16)		// not used as isInputTypeRestricted is false
		, isSpecConstant(false)
		, statementUsageFlags(_statementUsageFlags)
	{}

	// Conversion operations constructor (used also by conversions done in SpecConstantOp)
	Operation(const char* _name,
			  FloatUsage _floatUsage,
			  bool specConstant,
			  FloatType _inputType,
			  const char* _constants,
			  const char* _commands,
			  const FloatStatementUsageFlags _statementUsageFlags = 0)
		: name(_name)
		, floatUsage(_floatUsage)
		, annotations("")
		, types("")
		, constants(_constants)
		, variables("")
		, functions("")
		, commands(_commands)
		, isInputTypeRestricted(true)
		, restrictedInputType(_inputType)
		, isSpecConstant(specConstant)
		, statementUsageFlags(_statementUsageFlags)
	{}

	// Full constructor - used by few operations, that are more complex to test
	Operation(const char* _name,
			  FloatUsage _floatUsage,
			  const char* _annotations,
			  const char* _types,
			  const char* _constants,
			  const char* _variables,
			  const char* _functions,
			  const char* _commands,
			  const FloatStatementUsageFlags _statementUsageFlags = 0)
		: name(_name)
		, floatUsage(_floatUsage)
		, annotations(_annotations)
		, types(_types)
		, constants(_constants)
		, variables(_variables)
		, functions(_functions)
		, commands(_commands)
		, isInputTypeRestricted(false)
		, restrictedInputType(FP16)		// not used as isInputTypeRestricted is false
		, isSpecConstant(false)
		, statementUsageFlags(_statementUsageFlags)
	{}

	// Full constructor - used by rounding override cases
	Operation(const char* _name,
			  FloatUsage _floatUsage,
			  FloatType _inputType,
			  const char* _annotations,
			  const char* _types,
			  const char* _constants,
			  const char* _commands,
			  const FloatStatementUsageFlags _statementUsageFlags = 0)
		: name(_name)
		, floatUsage(_floatUsage)
		, annotations(_annotations)
		, types(_types)
		, constants(_constants)
		, variables("")
		, functions("")
		, commands(_commands)
		, isInputTypeRestricted(true)
		, restrictedInputType(_inputType)
		, isSpecConstant(false)
		, statementUsageFlags(_statementUsageFlags)
	{}
};

// Class storing input that will be passed to operation and expected
// output that should be generated for specified behaviour.
class OperationTestCase
{
public:

	OperationTestCase()		{}

	OperationTestCase(const char*	_baseName,
					  BehaviorFlags	_behaviorFlags,
					  OperationId	_operatinId,
					  ValueId		_input1,
					  ValueId		_input2,
					  ValueId		_expectedOutput)
		: baseName(_baseName)
		, behaviorFlags(_behaviorFlags)
		, operationId(_operatinId)
		, expectedOutput(_expectedOutput)
	{
		input[0] = _input1;
		input[1] = _input2;
	}

public:

	string					baseName;
	BehaviorFlags			behaviorFlags;
	OperationId				operationId;
	ValueId					input[2];
	ValueId					expectedOutput;
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

	FloatType					inFloatType;
	TypeSnippetsSP				inTypeSnippets;
	TypeSnippetsSP				outTypeSnippets;
	FloatStatementUsageFlags	argumentsUsesFloatConstant;
};

// Class responsible for constructing list of test cases for specified
// float type and specified way of preparation of arguments.
// Arguments can be either read from input SSBO or generated via math
// operations in spir-v code.
class TestCasesBuilder
{
public:

	void init();
	void build(vector<OperationTestCase>& testCases, TypeTestResultsSP typeTestResults, bool argumentsFromInput);
	const Operation& getOperation(OperationId id) const;

private:

	void createUnaryTestCases(vector<OperationTestCase>& testCases,
							  OperationId operationId,
							  ValueId denormPreserveResult,
							  ValueId denormFTZResult) const;

private:

	// Operations are shared betwean test cases so they are
	// passed to them as pointers to data stored in TestCasesBuilder.
	typedef OperationTestCase OTC;
	typedef Operation Op;
	map<int, Op> m_operations;
};

void TestCasesBuilder::init()
{
	map<int, Op>& mo = m_operations;

	// predefine operations repeatedly used in tests; note that "_float"
	// in every operation command will be replaced with either "_f16",
	// "_f32" or "_f64" - StringTemplate is not used here because it
	// would make code less readable
	// m_operations contains generic operation definitions that can be
	// used for all float types

	mo[O_NEGATE]		= Op("negate",		FLOAT_ARITHMETIC,
											"%result             = OpFNegate %type_float %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_COMPOSITE]		= Op("composite",	FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%result             = OpCompositeExtract %type_float %vec1 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_COMPOSITE_INS]	= Op("comp_ins",	FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec2 %c_float_0 %c_float_0\n"
											"%vec2               = OpCompositeInsert %type_float_vec2 %arg1 %vec1 0\n"
											"%result             = OpCompositeExtract %type_float %vec2 0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_COPY]			= Op("copy",		FLOAT_STORAGE_ONLY,
											"%result             = OpCopyObject %type_float %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_D_EXTRACT]		= Op("extract",		FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%result             = OpVectorExtractDynamic %type_float %vec1 %c_i32_0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_D_INSERT]		= Op("insert",		FLOAT_ARITHMETIC,
											"%tmpVec             = OpCompositeConstruct %type_float_vec2 %c_float_2 %c_float_2\n"
											"%vec1               = OpVectorInsertDynamic %type_float_vec2 %tmpVec %arg1 %c_i32_0\n"
											"%result             = OpCompositeExtract %type_float %vec1 0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_SHUFFLE]		= Op("shuffle",		FLOAT_ARITHMETIC,
											"%tmpVec1            = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%tmpVec2            = OpCompositeConstruct %type_float_vec2 %c_float_2 %c_float_2\n"	// NOTE: its impossible to test shuffle with denorms flushed
											"%vec1               = OpVectorShuffle %type_float_vec2 %tmpVec1 %tmpVec2 0 2\n"		//       to zero as this will be done by earlier operation
											"%result             = OpCompositeExtract %type_float %vec1 0\n",						//       (this also applies to few other operations)
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_TRANSPOSE]		= Op("transpose",	FLOAT_ARITHMETIC,
											"%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
											"%tmat               = OpTranspose %type_float_mat2x2 %mat\n"
											"%tcol               = OpCompositeExtract %type_float_vec2 %tmat 0\n"
											"%result             = OpCompositeExtract %type_float %tcol 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_RETURN_VAL]	= Op("ret_val",		FLOAT_ARITHMETIC,
											"",
											"%type_test_fun      = OpTypeFunction %type_float %type_float\n",
											"",
											"",
											"%test_fun = OpFunction %type_float None %type_test_fun\n"
											"%param = OpFunctionParameter %type_float\n"
											"%entry = OpLabel\n"
											"OpReturnValue %param\n"
											"OpFunctionEnd\n",
											"%result             = OpFunctionCall %type_float %test_fun %arg1\n",
											B_STATEMENT_USAGE_TYPES_TYPE_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

	// conversion operations that are meant to be used only for single output type (defined by the second number in name)
	const char* convertSource =				"%result             = OpFConvert %type_float %arg1\n";
	mo[O_CONV_FROM_FP16]	= Op("conv_from_fp16", FLOAT_STORAGE_ONLY, false, FP16, "", convertSource, B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_CONV_FROM_FP32]	= Op("conv_from_fp32", FLOAT_STORAGE_ONLY, false, FP32, "", convertSource, B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_CONV_FROM_FP64]	= Op("conv_from_fp64", FLOAT_STORAGE_ONLY, false, FP64, "", convertSource, B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

	// from all operands supported by OpSpecConstantOp we can only test FConvert opcode with literals as everything
	// else requires Karnel capability (OpenCL); values of literals used in SPIR-V code must be equiwalent to
	// V_CONV_FROM_FP32_ARG and V_CONV_FROM_FP64_ARG so we can use same expected rounded values as for regular OpFConvert
	mo[O_SCONST_CONV_FROM_FP32_TO_FP16]
						= Op("sconst_conv_from_fp32", FLOAT_ARITHMETIC, true, FP32,
											"%c_arg              = OpConstant %type_f32 1.22334445\n"
											"%result             = OpSpecConstantOp %type_f16 FConvert %c_arg\n",
											"",
											B_STATEMENT_USAGE_CONSTS_TYPE_FP16 | B_STATEMENT_USAGE_CONSTS_TYPE_FP32);
	mo[O_SCONST_CONV_FROM_FP64_TO_FP32]
						= Op("sconst_conv_from_fp64", FLOAT_ARITHMETIC, true, FP64,
											"%c_arg              = OpConstant %type_f64 1.22334455\n"
											"%result             = OpSpecConstantOp %type_f32 FConvert %c_arg\n",
											"",
											B_STATEMENT_USAGE_CONSTS_TYPE_FP32 | B_STATEMENT_USAGE_CONSTS_TYPE_FP64);
	mo[O_SCONST_CONV_FROM_FP64_TO_FP16]
						= Op("sconst_conv_from_fp64", FLOAT_ARITHMETIC, true, FP64,
											"%c_arg              = OpConstant %type_f64 1.22334445\n"
											"%result             = OpSpecConstantOp %type_f16 FConvert %c_arg\n",
											"",
											B_STATEMENT_USAGE_CONSTS_TYPE_FP16 | B_STATEMENT_USAGE_CONSTS_TYPE_FP64);

	mo[O_ADD]			= Op("add",			FLOAT_ARITHMETIC, "%result             = OpFAdd %type_float %arg1 %arg2\n", B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_SUB]			= Op("sub",			FLOAT_ARITHMETIC, "%result             = OpFSub %type_float %arg1 %arg2\n", B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MUL]			= Op("mul",			FLOAT_ARITHMETIC, "%result             = OpFMul %type_float %arg1 %arg2\n", B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_DIV]			= Op("div",			FLOAT_ARITHMETIC, "%result             = OpFDiv %type_float %arg1 %arg2\n", B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_REM]			= Op("rem",			FLOAT_ARITHMETIC, "%result             = OpFRem %type_float %arg1 %arg2\n", B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MOD]			= Op("mod",			FLOAT_ARITHMETIC, "%result             = OpFMod %type_float %arg1 %arg2\n", B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_PHI]			= Op("phi",			FLOAT_ARITHMETIC,
											"%comp               = OpFOrdGreaterThan %type_bool %arg1 %arg2\n"
											"                      OpSelectionMerge %comp_merge None\n"
											"                      OpBranchConditional %comp %true_branch %false_branch\n"
											"%true_branch        = OpLabel\n"
											"                      OpBranch %comp_merge\n"
											"%false_branch       = OpLabel\n"
											"                      OpBranch %comp_merge\n"
											"%comp_merge         = OpLabel\n"
											"%result             = OpPhi %type_float %arg2 %true_branch %arg1 %false_branch\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_SELECT]		= Op("select",		FLOAT_ARITHMETIC,
											"%always_true        = OpFOrdGreaterThan %type_bool %c_float_1 %c_float_0\n"
											"%result             = OpSelect %type_float %always_true %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_DOT]			= Op("dot",			FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%vec2               = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
											"%result             = OpDot %type_float %vec1 %vec2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_VEC_MUL_S]		= Op("vmuls",		FLOAT_ARITHMETIC,
											"%vec                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%tmpVec             = OpVectorTimesScalar %type_float_vec2 %vec %arg2\n"
											"%result             = OpCompositeExtract %type_float %tmpVec 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_VEC_MUL_M]		= Op("vmulm",		FLOAT_ARITHMETIC,
											"%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
											"%vec                = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
											"%tmpVec             = OpVectorTimesMatrix %type_float_vec2 %vec %mat\n"
											"%result             = OpCompositeExtract %type_float %tmpVec 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MAT_MUL_S]		= Op("mmuls",		FLOAT_ARITHMETIC,
											"%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
											"%mulMat             = OpMatrixTimesScalar %type_float_mat2x2 %mat %arg2\n"
											"%extCol             = OpCompositeExtract %type_float_vec2 %mulMat 0\n"
											"%result             = OpCompositeExtract %type_float %extCol 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MAT_MUL_V]		= Op("mmulv",		FLOAT_ARITHMETIC,
											"%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
											"%vec                = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
											"%mulVec             = OpMatrixTimesVector %type_float_vec2 %mat %vec\n"
											"%result             = OpCompositeExtract %type_float %mulVec 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MAT_MUL_M]		= Op("mmulm",		FLOAT_ARITHMETIC,
											"%col1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%mat1               = OpCompositeConstruct %type_float_mat2x2 %col1 %col1\n"
											"%col2               = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
											"%mat2               = OpCompositeConstruct %type_float_mat2x2 %col2 %col2\n"
											"%mulMat             = OpMatrixTimesMatrix %type_float_mat2x2 %mat1 %mat2\n"
											"%extCol             = OpCompositeExtract %type_float_vec2 %mulMat 0\n"
											"%result             = OpCompositeExtract %type_float %extCol 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_OUT_PROD]		= Op("out_prod",	FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%vec2               = OpCompositeConstruct %type_float_vec2 %arg2 %arg2\n"
											"%mulMat             = OpOuterProduct %type_float_mat2x2 %vec1 %vec2\n"
											"%extCol             = OpCompositeExtract %type_float_vec2 %mulMat 0\n"
											"%result             = OpCompositeExtract %type_float %extCol 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

	// comparison operations
	mo[O_ORD_EQ]		= Op("ord_eq",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFOrdEqual %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_UORD_EQ]		= Op("uord_eq",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFUnordEqual %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ORD_NEQ]		= Op("ord_neq",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFOrdNotEqual %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_UORD_NEQ]		= Op("uord_neq",	FLOAT_ARITHMETIC,
											"%boolVal           = OpFUnordNotEqual %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ORD_LS]		= Op("ord_ls",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFOrdLessThan %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_UORD_LS]		= Op("uord_ls",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFUnordLessThan %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ORD_GT]		= Op("ord_gt",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFOrdGreaterThan %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_UORD_GT]		= Op("uord_gt",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFUnordGreaterThan %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ORD_LE]		= Op("ord_le",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFOrdLessThanEqual %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_UORD_LE]		= Op("uord_le",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFUnordLessThanEqual %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ORD_GE]		= Op("ord_ge",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFOrdGreaterThanEqual %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_UORD_GE]		= Op("uord_ge",		FLOAT_ARITHMETIC,
											"%boolVal           = OpFUnordGreaterThanEqual %type_bool %arg1 %arg2\n"
											"%result            = OpSelect %type_float %boolVal %c_float_1 %c_float_0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

	mo[O_ATAN2]			= Op("atan2",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Atan2 %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_POW]			= Op("pow",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Pow %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MIX]			= Op("mix",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 FMix %arg1 %arg2 %c_float_0_5\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_FMA]			= Op("fma",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Fma %arg1 %arg2 %c_float_0_5\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MIN]			= Op("min",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 FMin %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MAX]			= Op("max",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 FMax %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_CLAMP]			= Op("clamp",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 FClamp %arg1 %arg2 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_STEP]			= Op("step",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Step %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_SSTEP]			= Op("sstep",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 SmoothStep %arg1 %arg2 %c_float_0_5\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_DIST]			= Op("distance",	FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Distance %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_CROSS]			= Op("cross",		FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec3 %arg1 %arg1 %arg1\n"
											"%vec2               = OpCompositeConstruct %type_float_vec3 %arg2 %arg2 %arg2\n"
											"%tmpVec             = OpExtInst %type_float_vec3 %std450 Cross %vec1 %vec2\n"
											"%result             = OpCompositeExtract %type_float %tmpVec 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_FACE_FWD]		= Op("face_fwd",	FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 FaceForward %c_float_1 %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_NMIN]			= Op("nmin",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 NMin %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_NMAX]			= Op("nmax",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 NMax %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_NCLAMP]		= Op("nclamp",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 NClamp %arg2 %arg1 %arg2\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

	mo[O_ROUND]			= Op("round",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Round %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ROUND_EV]		= Op("round_ev",	FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 RoundEven %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_TRUNC]			= Op("trunc",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Trunc %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ABS]			= Op("abs",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 FAbs %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_SIGN]			= Op("sign",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 FSign %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_FLOOR]			= Op("floor",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Floor %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_CEIL]			= Op("ceil",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Ceil %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_FRACT]			= Op("fract",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Fract %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_RADIANS]		= Op("radians",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Radians %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_DEGREES]		= Op("degrees",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Degrees %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_SIN]			= Op("sin",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Sin %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_COS]			= Op("cos",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Cos %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_TAN]			= Op("tan",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Tan %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ASIN]			= Op("asin",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Asin %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ACOS]			= Op("acos",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Acos %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ATAN]			= Op("atan",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Atan %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_SINH]			= Op("sinh",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Sinh %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_COSH]			= Op("cosh",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Cosh %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_TANH]			= Op("tanh",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Tanh %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ASINH]			= Op("asinh",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Asinh %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ACOSH]			= Op("acosh",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Acosh %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_ATANH]			= Op("atanh",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Atanh %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_EXP]			= Op("exp",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Exp %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_LOG]			= Op("log",			FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Log %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_EXP2]			= Op("exp2",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Exp2 %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_LOG2]			= Op("log2",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Log2 %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_SQRT]			= Op("sqrt",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Sqrt %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_INV_SQRT]		= Op("inv_sqrt",	FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 InverseSqrt %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MODF]			= Op("modf",		FLOAT_ARITHMETIC,
											"",
											"",
											"",
											"%tmpVarPtr          = OpVariable %type_float_fptr Function\n",
											"",
											"%result             = OpExtInst %type_float %std450 Modf %arg1 %tmpVarPtr\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MODF_ST]		= Op("modf_st",		FLOAT_ARITHMETIC,
											"OpMemberDecorate %struct_ff 0 Offset ${float_width}\n"
											"OpMemberDecorate %struct_ff 1 Offset ${float_width}\n",
											"%struct_ff          = OpTypeStruct %type_float %type_float\n"
											"%struct_ff_fptr     = OpTypePointer Function %struct_ff\n",
											"",
											"%tmpStructPtr       = OpVariable %struct_ff_fptr Function\n",
											"",
											"%tmpStruct          = OpExtInst %struct_ff %std450 ModfStruct %arg1\n"
											"                      OpStore %tmpStructPtr %tmpStruct\n"
											"%tmpLoc             = OpAccessChain %type_float_fptr %tmpStructPtr %c_i32_0\n"
											"%result             = OpLoad %type_float %tmpLoc\n",
											B_STATEMENT_USAGE_TYPES_TYPE_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_FREXP]			= Op("frexp",		FLOAT_ARITHMETIC,
											"",
											"",
											"",
											"%tmpVarPtr          = OpVariable %type_i32_fptr Function\n",
											"",
											"%result             = OpExtInst %type_float %std450 Frexp %arg1 %tmpVarPtr\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_FREXP_ST]		= Op("frexp_st",	FLOAT_ARITHMETIC,
											"OpMemberDecorate %struct_fi 0 Offset ${float_width}\n"
											"OpMemberDecorate %struct_fi 1 Offset 32\n",
											"%struct_fi          = OpTypeStruct %type_float %type_i32\n"
											"%struct_fi_fptr     = OpTypePointer Function %struct_fi\n",
											"",
											"%tmpStructPtr       = OpVariable %struct_fi_fptr Function\n",
											"",
											"%tmpStruct          = OpExtInst %struct_fi %std450 FrexpStruct %arg1\n"
											"                      OpStore %tmpStructPtr %tmpStruct\n"
											"%tmpLoc             = OpAccessChain %type_float_fptr %tmpStructPtr %c_i32_0\n"
											"%result             = OpLoad %type_float %tmpLoc\n",
											B_STATEMENT_USAGE_TYPES_TYPE_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_LENGHT]		= Op("length",		FLOAT_ARITHMETIC,
											"%result             = OpExtInst %type_float %std450 Length %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_NORMALIZE]		= Op("normalize",	FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %c_float_2\n"
											"%tmpVec             = OpExtInst %type_float_vec2 %std450 Normalize %vec1\n"
											"%result             = OpCompositeExtract %type_float %tmpVec 0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_REFLECT]		= Op("reflect",		FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%vecN               = OpCompositeConstruct %type_float_vec2 %c_float_0 %c_float_n1\n"
											"%tmpVec             = OpExtInst %type_float_vec2 %std450 Reflect %vec1 %vecN\n"
											"%result             = OpCompositeExtract %type_float %tmpVec 0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_REFRACT]		= Op("refract",		FLOAT_ARITHMETIC,
											"%vec1               = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%vecN               = OpCompositeConstruct %type_float_vec2 %c_float_0 %c_float_n1\n"
											"%tmpVec             = OpExtInst %type_float_vec2 %std450 Refract %vec1 %vecN %c_float_0_5\n"
											"%result             = OpCompositeExtract %type_float %tmpVec 0\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MAT_DET]		= Op("mat_det",		FLOAT_ARITHMETIC,
											"%col                = OpCompositeConstruct %type_float_vec2 %arg1 %arg1\n"
											"%mat                = OpCompositeConstruct %type_float_mat2x2 %col %col\n"
											"%result             = OpExtInst %type_float %std450 Determinant %mat\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);
	mo[O_MAT_INV]		= Op("mat_inv",		FLOAT_ARITHMETIC,
											"%col1               = OpCompositeConstruct %type_float_vec2 %arg1 %c_float_1\n"
											"%col2               = OpCompositeConstruct %type_float_vec2 %c_float_1 %c_float_1\n"
											"%mat                = OpCompositeConstruct %type_float_mat2x2 %col1 %col2\n"
											"%invMat             = OpExtInst %type_float_mat2x2 %std450 MatrixInverse %mat\n"
											"%extCol             = OpCompositeExtract %type_float_vec2 %invMat 1\n"
											"%result             = OpCompositeExtract %type_float %extCol 1\n",
											B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_TYPE_FLOAT);

	// PackHalf2x16 is a special case as it operates on fp32 vec2 and returns unsigned int,
	// the verification is done in SPIR-V code (if result is correct 1.0 will be written to SSBO)
	mo[O_PH_DENORM]		= Op("ph_denorm",	FLOAT_STORAGE_ONLY,
											"",
											"",
											"%c_fp32_denorm_fp16 = OpConstant %type_f32 6.01e-5\n"		// fp32 representation of fp16 denorm value
											"%c_ref              = OpConstant %type_u32 66061296\n",
											"",
											"",
											"%srcVec             = OpCompositeConstruct %type_f32_vec2 %c_fp32_denorm_fp16 %c_fp32_denorm_fp16\n"
											"%packedInt          = OpExtInst %type_u32 %std450 PackHalf2x16 %srcVec\n"
											"%boolVal            = OpIEqual %type_bool %c_ref %packedInt\n"
											"%result             = OpSelect %type_f32 %boolVal %c_f32_1 %c_f32_0\n",
											B_STATEMENT_USAGE_CONSTS_TYPE_FP32 | B_STATEMENT_USAGE_COMMANDS_CONST_FP32 | B_STATEMENT_USAGE_COMMANDS_TYPE_FP32);

	// UnpackHalf2x16 is a special case that operates on uint32 and returns two 32-bit floats,
	// this function is tested using constants
	mo[O_UPH_DENORM]	= Op("uph_denorm",	FLOAT_STORAGE_ONLY,
											"",
											"",
											"%c_u32_2_16_pack    = OpConstant %type_u32 66061296\n", // == packHalf2x16(vec2(denorm))
											"",
											"",
											"%tmpVec             = OpExtInst %type_f32_vec2 %std450 UnpackHalf2x16 %c_u32_2_16_pack\n"
											"%result             = OpCompositeExtract %type_f32 %tmpVec 0\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FP32);

	// PackDouble2x32 is a special case that operates on two uint32 and returns
	// double, this function is tested using constants
	mo[O_PD_DENORM]		= Op("pd_denorm",	FLOAT_STORAGE_ONLY,
											"",
											"",
											"%c_p1               = OpConstant %type_u32 0\n"
											"%c_p2               = OpConstant %type_u32 262144\n",		// == UnpackDouble2x32(denorm)
											"",
											"",
											"%srcVec             = OpCompositeConstruct %type_u32_vec2 %c_p1 %c_p2\n"
											"%result             = OpExtInst %type_f64 %std450 PackDouble2x32 %srcVec\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FP64);

	// UnpackDouble2x32 is a special case as it operates only on FP64 and returns two ints,
	// the verification is done in SPIR-V code (if result is correct 1.0 will be written to SSBO)
	const char* unpackDouble2x32Types	=	"%type_bool_vec2     = OpTypeVector %type_bool 2\n";
	const char* unpackDouble2x32Source	=	"%refVec2            = OpCompositeConstruct %type_u32_vec2 %c_p1 %c_p2\n"
											"%resVec2            = OpExtInst %type_u32_vec2 %std450 UnpackDouble2x32 %arg1\n"
											"%boolVec2           = OpIEqual %type_bool_vec2 %refVec2 %resVec2\n"
											"%boolVal            = OpAll %type_bool %boolVec2\n"
											"%result             = OpSelect %type_f64 %boolVal %c_f64_1 %c_f64_0\n";
	mo[O_UPD_DENORM_FLUSH]		= Op("upd_denorm",	FLOAT_STORAGE_ONLY, "",
											unpackDouble2x32Types,
											"%c_p1               = OpConstant %type_u32 0\n"
											"%c_p2               = OpConstant %type_u32 0\n",
											"",
											"",
											unpackDouble2x32Source,
											B_STATEMENT_USAGE_COMMANDS_CONST_FP64 | B_STATEMENT_USAGE_COMMANDS_TYPE_FP64);
	mo[O_UPD_DENORM_PRESERVE]	= Op("upd_denorm",	FLOAT_STORAGE_ONLY, "",
											unpackDouble2x32Types,
											"%c_p1               = OpConstant %type_u32 1008\n"
											"%c_p2               = OpConstant %type_u32 0\n",
											"",
											"",
											unpackDouble2x32Source,
											B_STATEMENT_USAGE_COMMANDS_CONST_FP64 | B_STATEMENT_USAGE_COMMANDS_TYPE_FP64);

	mo[O_ORTE_ROUND]	= Op("orte_round",	FLOAT_STORAGE_ONLY, FP32,
											"OpDecorate %result FPRoundingMode RTE\n",
											"",
											"",
											"%result             = OpFConvert %type_f16 %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FP16);
	mo[O_ORTZ_ROUND]	= Op("ortz_round",	FLOAT_STORAGE_ONLY, FP32,
											"OpDecorate %result FPRoundingMode RTZ\n",
											"",
											"",
											"%result             = OpFConvert %type_f16 %arg1\n",
											B_STATEMENT_USAGE_COMMANDS_TYPE_FP16);
}

void TestCasesBuilder::build(vector<OperationTestCase>& testCases, TypeTestResultsSP typeTestResults, bool argumentsFromInput)
{
	// this method constructs a list of test cases; this list is a bit different
	// for every combination of float type, arguments preparation method and tested float control

	testCases.reserve(750);

	// Denorm - FlushToZero - binary operations
	for (size_t i = 0 ; i < typeTestResults->binaryOpFTZ.size() ; ++i)
	{
		const BinaryCase&	binaryCase	= typeTestResults->binaryOpFTZ[i];
		OperationId			operation	= binaryCase.operationId;
		testCases.push_back(OTC("denorm_op_var_flush_to_zero",		B_DENORM_FLUSH,					 operation, V_DENORM, V_ONE,		binaryCase.opVarResult));
		testCases.push_back(OTC("denorm_op_denorm_flush_to_zero",	B_DENORM_FLUSH,					 operation, V_DENORM, V_DENORM,		binaryCase.opDenormResult));
		testCases.push_back(OTC("denorm_op_inf_flush_to_zero",		B_DENORM_FLUSH | B_ZIN_PRESERVE, operation, V_DENORM, V_INF,		binaryCase.opInfResult));
		testCases.push_back(OTC("denorm_op_nan_flush_to_zero",		B_DENORM_FLUSH | B_ZIN_PRESERVE, operation, V_DENORM, V_NAN,		binaryCase.opNanResult));
	}

	// Denorm - FlushToZero - unary operations
	for (size_t i = 0 ; i < typeTestResults->unaryOpFTZ.size() ; ++i)
	{
		const UnaryCase&	unaryCase = typeTestResults->unaryOpFTZ[i];
		OperationId			operation = unaryCase.operationId;
		testCases.push_back(OTC("op_denorm_flush_to_zero", B_DENORM_FLUSH, operation, V_DENORM, V_UNUSED, unaryCase.result));
	}

	// Denom - Preserve - binary operations
	for (size_t i = 0 ; i < typeTestResults->binaryOpDenormPreserve.size() ; ++i)
	{
		const BinaryCase&	binaryCase	= typeTestResults->binaryOpDenormPreserve[i];
		OperationId			operation	= binaryCase.operationId;
		testCases.push_back(OTC("denorm_op_var_preserve",			B_DENORM_PRESERVE,					operation, V_DENORM,	V_ONE,		binaryCase.opVarResult));
		testCases.push_back(OTC("denorm_op_denorm_preserve",		B_DENORM_PRESERVE,					operation, V_DENORM,	V_DENORM,	binaryCase.opDenormResult));
		testCases.push_back(OTC("denorm_op_inf_preserve",			B_DENORM_PRESERVE | B_ZIN_PRESERVE, operation, V_DENORM,	V_INF,		binaryCase.opInfResult));
		testCases.push_back(OTC("denorm_op_nan_preserve",			B_DENORM_PRESERVE | B_ZIN_PRESERVE, operation, V_DENORM,	V_NAN,		binaryCase.opNanResult));
	}

	// Denom - Preserve - unary operations
	for (size_t i = 0 ; i < typeTestResults->unaryOpDenormPreserve.size() ; ++i)
	{
		const UnaryCase&	unaryCase	= typeTestResults->unaryOpDenormPreserve[i];
		OperationId			operation	= unaryCase.operationId;
		testCases.push_back(OTC("op_denorm_preserve", B_DENORM_PRESERVE, operation, V_DENORM, V_UNUSED, unaryCase.result));
	}

	struct ZINCase
	{
		OperationId	operationId;
		bool		supportedByFP64;
		ValueId		secondArgument;
		ValueId		preserveZeroResult;
		ValueId		preserveSZeroResult;
		ValueId		preserveInfResult;
		ValueId		preserveSInfResult;
		ValueId		preserveNanResult;
	};

	const ZINCase binaryOpZINPreserve[] = {
		// operation		fp64	second arg		preserve zero	preserve szero		preserve inf	preserve sinf		preserve nan
		{ O_PHI,			true,	V_INF,			V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_SELECT,			true,	V_ONE,			V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_ADD,			true,	V_ZERO,			V_ZERO,			V_ZERO,				V_INF,			V_MINUS_INF,		V_NAN },
		{ O_SUB,			true,	V_ZERO,			V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_MUL,			true,	V_ONE,			V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
	};

	const ZINCase unaryOpZINPreserve[] = {
		// operation				fp64	second arg		preserve zero	preserve szero		preserve inf	preserve sinf		preserve nan
		{ O_RETURN_VAL,				true,	V_UNUSED,		V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_D_EXTRACT,				true,	V_UNUSED,		V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_D_INSERT,				true,	V_UNUSED,		V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_SHUFFLE,				true,	V_UNUSED,		V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_COMPOSITE,				true,	V_UNUSED,		V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_COMPOSITE_INS,			true,	V_UNUSED,		V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_COPY,					true,	V_UNUSED,		V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_TRANSPOSE,				true,	V_UNUSED,		V_ZERO,			V_MINUS_ZERO,		V_INF,			V_MINUS_INF,		V_NAN },
		{ O_NEGATE,					true,	V_UNUSED,		V_MINUS_ZERO,	V_ZERO,				V_MINUS_INF,	V_INF,				V_NAN },
	};

	bool isFP64 = typeTestResults->floatType() == FP64;

	// Signed Zero Inf Nan - Preserve - binary operations
	for (size_t i = 0 ; i < DE_LENGTH_OF_ARRAY(binaryOpZINPreserve) ; ++i)
	{
		const ZINCase& zc = binaryOpZINPreserve[i];
		if (isFP64 && !zc.supportedByFP64)
			continue;

		testCases.push_back(OTC("zero_op_var_preserve",				B_ZIN_PRESERVE, zc.operationId, V_ZERO,			zc.secondArgument,	zc.preserveZeroResult));
		testCases.push_back(OTC("signed_zero_op_var_preserve",		B_ZIN_PRESERVE, zc.operationId, V_MINUS_ZERO,	zc.secondArgument,	zc.preserveSZeroResult));
		testCases.push_back(OTC("inf_op_var_preserve",				B_ZIN_PRESERVE, zc.operationId, V_INF,			zc.secondArgument,	zc.preserveInfResult));
		testCases.push_back(OTC("signed_inf_op_var_preserve",		B_ZIN_PRESERVE, zc.operationId, V_MINUS_INF,	zc.secondArgument,	zc.preserveSInfResult));
		testCases.push_back(OTC("nan_op_var_preserve",				B_ZIN_PRESERVE, zc.operationId, V_NAN,			zc.secondArgument,	zc.preserveNanResult));
	}

	// Signed Zero Inf Nan - Preserve - unary operations
	for (size_t i = 0 ; i < DE_LENGTH_OF_ARRAY(unaryOpZINPreserve) ; ++i)
	{
		const ZINCase& zc = unaryOpZINPreserve[i];
		if (isFP64 && !zc.supportedByFP64)
			continue;

		testCases.push_back(OTC("op_zero_preserve",			B_ZIN_PRESERVE,zc.operationId, V_ZERO,			V_UNUSED,	zc.preserveZeroResult));
		testCases.push_back(OTC("op_signed_zero_preserve",	B_ZIN_PRESERVE,zc.operationId, V_MINUS_ZERO,	V_UNUSED,	zc.preserveSZeroResult));
		testCases.push_back(OTC("op_inf_preserve",			B_ZIN_PRESERVE,zc.operationId, V_INF,			V_UNUSED,	zc.preserveInfResult));
		testCases.push_back(OTC("op_signed_inf_preserve",	B_ZIN_PRESERVE,zc.operationId, V_MINUS_INF,		V_UNUSED,	zc.preserveSInfResult));
		testCases.push_back(OTC("op_nan_preserve",			B_ZIN_PRESERVE,zc.operationId, V_NAN,			V_UNUSED,	zc.preserveNanResult));
	}

	// comparison operations - tested differently because they return true/false
	struct ComparisonCase
	{
		OperationId	operationId;
		ValueId		denormPreserveResult;
	};
	const ComparisonCase comparisonCases[] =
	{
		// operation	denorm
		{ O_ORD_EQ,		V_ZERO },
		{ O_UORD_EQ,	V_ZERO },
		{ O_ORD_NEQ,	V_ONE  },
		{ O_UORD_NEQ,	V_ONE  },
		{ O_ORD_LS,		V_ONE  },
		{ O_UORD_LS,	V_ONE  },
		{ O_ORD_GT,		V_ZERO },
		{ O_UORD_GT,	V_ZERO },
		{ O_ORD_LE,		V_ONE  },
		{ O_UORD_LE,	V_ONE  },
		{ O_ORD_GE,		V_ZERO },
		{ O_UORD_GE,	V_ZERO }
	};
	for (int op = 0 ; op < DE_LENGTH_OF_ARRAY(comparisonCases) ; ++op)
	{
		const ComparisonCase& cc = comparisonCases[op];
		testCases.push_back(OTC("denorm_op_var_preserve", B_DENORM_PRESERVE, cc.operationId, V_DENORM, V_ONE, cc.denormPreserveResult));
	}

	if (argumentsFromInput)
	{
		struct RoundingModeCase
		{
			OperationId	operationId;
			ValueId		arg1;
			ValueId		arg2;
			ValueId		expectedRTEResult;
			ValueId		expectedRTZResult;
		};

		const RoundingModeCase roundingCases[] =
		{
			{ O_ADD,			V_ADD_ARG_A,	V_ADD_ARG_B,	V_ADD_RTE_RESULT,	V_ADD_RTZ_RESULT },
			{ O_SUB,			V_SUB_ARG_A,	V_SUB_ARG_B,	V_SUB_RTE_RESULT,	V_SUB_RTZ_RESULT },
			{ O_MUL,			V_MUL_ARG_A,	V_MUL_ARG_B,	V_MUL_RTE_RESULT,	V_MUL_RTZ_RESULT },
			{ O_DOT,			V_DOT_ARG_A,	V_DOT_ARG_B,	V_DOT_RTE_RESULT,	V_DOT_RTZ_RESULT },

			// in vect/mat multiplication by scalar operations only first element of result is checked
			// so argument and result values prepared for multiplication can be reused for those cases
			{ O_VEC_MUL_S,		V_MUL_ARG_A,	V_MUL_ARG_B,	V_MUL_RTE_RESULT,	V_MUL_RTZ_RESULT },
			{ O_MAT_MUL_S,		V_MUL_ARG_A,	V_MUL_ARG_B,	V_MUL_RTE_RESULT,	V_MUL_RTZ_RESULT },
			{ O_OUT_PROD,		V_MUL_ARG_A,	V_MUL_ARG_B,	V_MUL_RTE_RESULT,	V_MUL_RTZ_RESULT },

			// in SPIR-V code we return first element of operation result so for following
			// cases argument and result values prepared for dot product can be reused
			{ O_VEC_MUL_M,		V_DOT_ARG_A,	V_DOT_ARG_B,	V_DOT_RTE_RESULT,	V_DOT_RTZ_RESULT },
			{ O_MAT_MUL_V,		V_DOT_ARG_A,	V_DOT_ARG_B,	V_DOT_RTE_RESULT,	V_DOT_RTZ_RESULT },
			{ O_MAT_MUL_M,		V_DOT_ARG_A,	V_DOT_ARG_B,	V_DOT_RTE_RESULT,	V_DOT_RTZ_RESULT },

			// conversion operations are added separately - depending on float type width
		};

		for (int c = 0 ; c < DE_LENGTH_OF_ARRAY(roundingCases) ; ++c)
		{
			const RoundingModeCase& rmc = roundingCases[c];
			testCases.push_back(OTC("rounding_rte_op", B_RTE_ROUNDING, rmc.operationId, rmc.arg1, rmc.arg2, rmc.expectedRTEResult));
			testCases.push_back(OTC("rounding_rtz_op", B_RTZ_ROUNDING, rmc.operationId, rmc.arg1, rmc.arg2, rmc.expectedRTZResult));
		}
	}

	// special cases
	if (typeTestResults->floatType() == FP16)
	{
		if (argumentsFromInput)
		{
			testCases.push_back(OTC("rounding_rte_conv_from_fp32", B_RTE_ROUNDING, O_CONV_FROM_FP32, V_CONV_FROM_FP32_ARG, V_UNUSED, V_CONV_TO_FP16_RTE_RESULT));
			testCases.push_back(OTC("rounding_rtz_conv_from_fp32", B_RTZ_ROUNDING, O_CONV_FROM_FP32, V_CONV_FROM_FP32_ARG, V_UNUSED, V_CONV_TO_FP16_RTZ_RESULT));
			testCases.push_back(OTC("rounding_rte_conv_from_fp64", B_RTE_ROUNDING, O_CONV_FROM_FP64, V_CONV_FROM_FP64_ARG, V_UNUSED, V_CONV_TO_FP16_RTE_RESULT));
			testCases.push_back(OTC("rounding_rtz_conv_from_fp64", B_RTZ_ROUNDING, O_CONV_FROM_FP64, V_CONV_FROM_FP64_ARG, V_UNUSED, V_CONV_TO_FP16_RTZ_RESULT));

			testCases.push_back(OTC("rounding_rte_sconst_conv_from_fp32", B_RTE_ROUNDING, O_SCONST_CONV_FROM_FP32_TO_FP16, V_UNUSED, V_UNUSED, V_CONV_TO_FP16_RTE_RESULT));
			testCases.push_back(OTC("rounding_rtz_sconst_conv_from_fp32", B_RTZ_ROUNDING, O_SCONST_CONV_FROM_FP32_TO_FP16, V_UNUSED, V_UNUSED, V_CONV_TO_FP16_RTZ_RESULT));
			testCases.push_back(OTC("rounding_rte_sconst_conv_from_fp64", B_RTE_ROUNDING, O_SCONST_CONV_FROM_FP64_TO_FP16, V_UNUSED, V_UNUSED, V_CONV_TO_FP16_RTE_RESULT));
			testCases.push_back(OTC("rounding_rtz_sconst_conv_from_fp64", B_RTZ_ROUNDING, O_SCONST_CONV_FROM_FP64_TO_FP16, V_UNUSED, V_UNUSED, V_CONV_TO_FP16_RTZ_RESULT));

			// verify that VkShaderFloatingPointRoundingModeKHR can be overridden for a given instruction by the FPRoundingMode decoration
			testCases.push_back(OTC("rounding_rte_override", B_RTE_ROUNDING, O_ORTZ_ROUND, V_CONV_FROM_FP32_ARG, V_UNUSED, V_CONV_TO_FP16_RTZ_RESULT));
			testCases.push_back(OTC("rounding_rtz_override", B_RTZ_ROUNDING, O_ORTE_ROUND, V_CONV_FROM_FP32_ARG, V_UNUSED, V_CONV_TO_FP16_RTE_RESULT));
		}

		createUnaryTestCases(testCases, O_CONV_FROM_FP32, V_CONV_DENORM_SMALLER, V_ZERO);
		createUnaryTestCases(testCases, O_CONV_FROM_FP64, V_CONV_DENORM_BIGGER, V_ZERO);
	}
	else if (typeTestResults->floatType() == FP32)
	{
		if (argumentsFromInput)
		{
			// convert from fp64 to fp32
			testCases.push_back(OTC("rounding_rte_conv_from_fp64", B_RTE_ROUNDING, O_CONV_FROM_FP64, V_CONV_FROM_FP64_ARG, V_UNUSED, V_CONV_TO_FP32_RTE_RESULT));
			testCases.push_back(OTC("rounding_rtz_conv_from_fp64", B_RTZ_ROUNDING, O_CONV_FROM_FP64, V_CONV_FROM_FP64_ARG, V_UNUSED, V_CONV_TO_FP32_RTZ_RESULT));

			testCases.push_back(OTC("rounding_rte_sconst_conv_from_fp64", B_RTE_ROUNDING, O_SCONST_CONV_FROM_FP64_TO_FP32, V_UNUSED, V_UNUSED, V_CONV_TO_FP32_RTE_RESULT));
			testCases.push_back(OTC("rounding_rtz_sconst_conv_from_fp64", B_RTZ_ROUNDING, O_SCONST_CONV_FROM_FP64_TO_FP32, V_UNUSED, V_UNUSED, V_CONV_TO_FP32_RTZ_RESULT));
		}
		else
		{
			// PackHalf2x16 - verification done in SPIR-V
			testCases.push_back(OTC("pack_half_denorm_preserve",		B_DENORM_PRESERVE,	O_PH_DENORM,	V_UNUSED, V_UNUSED, V_ONE));

			// UnpackHalf2x16 - custom arguments defined as constants
			testCases.push_back(OTC("upack_half_denorm_flush_to_zero",	B_DENORM_FLUSH,		O_UPH_DENORM,	V_UNUSED, V_UNUSED, V_ZERO));
			testCases.push_back(OTC("upack_half_denorm_preserve",		B_DENORM_PRESERVE,	O_UPH_DENORM,	V_UNUSED, V_UNUSED, V_CONV_DENORM_SMALLER));
		}

		createUnaryTestCases(testCases, O_CONV_FROM_FP16, V_CONV_DENORM_SMALLER, V_ZERO_OR_FP16_DENORM_TO_FP32);
		createUnaryTestCases(testCases, O_CONV_FROM_FP64, V_CONV_DENORM_BIGGER, V_ZERO);
	}
	else // FP64
	{
		if (!argumentsFromInput)
		{
			// PackDouble2x32 - custom arguments defined as constants
			testCases.push_back(OTC("pack_double_denorm_preserve",			B_DENORM_PRESERVE,	O_PD_DENORM,			V_UNUSED, V_UNUSED, V_DENORM));

			// UnpackDouble2x32 - verification done in SPIR-V
			testCases.push_back(OTC("upack_double_denorm_flush_to_zero",	B_DENORM_FLUSH,		O_UPD_DENORM_FLUSH,		V_DENORM, V_UNUSED, V_ONE));
			testCases.push_back(OTC("upack_double_denorm_preserve",			B_DENORM_PRESERVE,	O_UPD_DENORM_PRESERVE,	V_DENORM, V_UNUSED, V_ONE));
		}

		createUnaryTestCases(testCases, O_CONV_FROM_FP16, V_CONV_DENORM_SMALLER, V_ZERO_OR_FP16_DENORM_TO_FP64);
		createUnaryTestCases(testCases, O_CONV_FROM_FP32, V_CONV_DENORM_BIGGER, V_ZERO_OR_FP32_DENORM_TO_FP64);
	}
}

const Operation& TestCasesBuilder::getOperation(OperationId id) const
{
	return m_operations.at(id);
}

void TestCasesBuilder::createUnaryTestCases(vector<OperationTestCase>& testCases, OperationId operationId, ValueId denormPreserveResult, ValueId denormFTZResult) const
{
	// Denom - Preserve
	testCases.push_back(OTC("op_denorm_preserve",		B_DENORM_PRESERVE,	operationId, V_DENORM,	V_UNUSED, denormPreserveResult));

	// Denorm - FlushToZero
	testCases.push_back(OTC("op_denorm_flush_to_zero",	B_DENORM_FLUSH,		operationId, V_DENORM,	V_UNUSED, denormFTZResult));

	// Signed Zero Inf Nan - Preserve
	testCases.push_back(OTC("op_zero_preserve",			B_ZIN_PRESERVE,		operationId, V_ZERO,		V_UNUSED, V_ZERO));
	testCases.push_back(OTC("op_signed_zero_preserve",	B_ZIN_PRESERVE,		operationId, V_MINUS_ZERO,	V_UNUSED, V_MINUS_ZERO));
	testCases.push_back(OTC("op_inf_preserve",			B_ZIN_PRESERVE,		operationId, V_INF,			V_UNUSED, V_INF));
	testCases.push_back(OTC("op_nan_preserve",			B_ZIN_PRESERVE,		operationId, V_NAN,			V_UNUSED, V_NAN));
}

template <typename TYPE, typename FLOAT_TYPE>
bool isZeroOrOtherValue(const TYPE& returnedFloat, ValueId secondAcceptableResult, TestLog& log)
{
	if (returnedFloat.isZero() && !returnedFloat.signBit())
		return true;

	TypeValues<FLOAT_TYPE> typeValues;
	typedef typename TYPE::StorageType SType;
	typename RawConvert<FLOAT_TYPE, SType>::Value value;
	value.fp = typeValues.getValue(secondAcceptableResult);

	if (returnedFloat.bits() == value.ui)
		return true;

	log << TestLog::Message << "Expected 0 or " << toHex(value.ui)
		<< " (" << value.fp << ")" << TestLog::EndMessage;
	return false;
}

template <typename TYPE>
bool isAcosResultCorrect(const TYPE& returnedFloat, TestLog& log)
{
	// pi/2 is result of acos(0) which in the specs is defined as equivalent to
	// atan2(sqrt(1.0 - x^2), x), where atan2 has 4096 ULP, sqrt is equivalent to
	// 1.0 /inversesqrt(), inversesqrt() is 2 ULP and rcp is another 2.5 ULP

	double precision = 0;
	const double piDiv2 = 3.14159265358979323846 / 2;
	if (returnedFloat.MANTISSA_BITS == 23)
	{
		FloatFormat fp32Format(-126, 127, 23, true, tcu::MAYBE, tcu::YES, tcu::MAYBE);
		precision = fp32Format.ulp(piDiv2, 4096.0);
	}
	else
	{
		FloatFormat fp16Format(-14, 15, 10, true, tcu::MAYBE);
		precision = fp16Format.ulp(piDiv2, 5.0);
	}

	if (deAbs(returnedFloat.asDouble() - piDiv2) < precision)
		return true;

	log << TestLog::Message << "Expected result to be in range"
		<< " (" << piDiv2 - precision << ", " << piDiv2 + precision << "), got "
		<< returnedFloat.asDouble() << TestLog::EndMessage;
	return false;
}

template <typename TYPE>
bool isCosResultCorrect(const TYPE& returnedFloat, TestLog& log)
{
	// for cos(x) with x between -pi and pi, the precision error is 2^-11 for fp32 and 2^-7 for fp16.
	double precision = returnedFloat.MANTISSA_BITS == 23 ? dePow(2, -11) : dePow(2, -7);
	const double expected = 1.0;

	if (deAbs(returnedFloat.asDouble() - expected) < precision)
		return true;

	log << TestLog::Message << "Expected result to be in range"
		<< " (" << expected - precision << ", " << expected + precision << "), got "
		<< returnedFloat.asDouble() << TestLog::EndMessage;
	return false;
}

template <typename FLOAT_TYPE>
double getFloatTypeAsDouble(FLOAT_TYPE param)
{
	return param;
}
template<> double getFloatTypeAsDouble(deFloat16 param)
{
	return deFloat16To64(param);
}


double getPrecisionAt(double value, float ulp, int mantissaBits)
{
	if (mantissaBits == 23)
	{
		FloatFormat fp32Format(-126, 127, 23, true, tcu::MAYBE, tcu::YES, tcu::MAYBE);
		return fp32Format.ulp(value, ulp);
	}
	else if (mantissaBits == 52)
	{
		FloatFormat fp32Format(-1022, 1023, 52, true, tcu::MAYBE, tcu::YES, tcu::MAYBE);
		return fp32Format.ulp(value, ulp);
	}
	else
	{
		DE_ASSERT(mantissaBits == 10);
		FloatFormat fp16Format(-14, 15, 10, true, tcu::MAYBE);
		return fp16Format.ulp(value, ulp);
	}
}

template <typename TYPE, typename FLOAT_TYPE, typename REF_FUNCTION>
bool isLogResultCorrect(const TYPE& returnedFloat, FLOAT_TYPE param, REF_FUNCTION refFunction, TestLog& log)
{
	if (returnedFloat.isInf() && returnedFloat.signBit())
		return true;

	const double expected	= refFunction(getFloatTypeAsDouble(param));
	const double precision	= getPrecisionAt(expected, 3.0, returnedFloat.MANTISSA_BITS);

	if (deAbs(returnedFloat.asDouble() - expected) < precision)
		return true;

	log << TestLog::Message << "Expected result to be -INF or in range"
		<< " (" << expected - precision << ", " << expected + precision << "), got "
		<< returnedFloat.asDouble() << TestLog::EndMessage;
	return false;
}

template <typename TYPE, typename FLOAT_TYPE>
bool isInverseSqrtResultCorrect(const TYPE& returnedFloat, FLOAT_TYPE param, TestLog& log)
{
	if (returnedFloat.isInf() && !returnedFloat.signBit())
		return true;

	const double expected	= 1.0/ deSqrt(getFloatTypeAsDouble(param));
	const double precision	= getPrecisionAt(expected, 2.0, returnedFloat.MANTISSA_BITS);

	if (deAbs(returnedFloat.asDouble() - expected) < precision)
		return true;

	log << TestLog::Message << "Expected result to be INF or in range"
		<< " (" << expected - precision << ", " << expected + precision << "), got "
		<< returnedFloat.asDouble() << TestLog::EndMessage;
	return false;
}

template <typename TYPE, typename FLOAT_TYPE>
bool isSqrtResultCorrect(const TYPE& returnedFloat, FLOAT_TYPE param, TestLog& log)
{
	if (returnedFloat.isZero() && !returnedFloat.signBit())
		return true;


	const double expected				= deSqrt(getFloatTypeAsDouble(param));
	const double expectedInverseSqrt	= 1.0 / expected;
	const double inverseSqrtPrecision	= getPrecisionAt(expectedInverseSqrt, 2.0, returnedFloat.MANTISSA_BITS);

	double expectedMin = deMin(1.0 / (expectedInverseSqrt - inverseSqrtPrecision), 1.0 / (expectedInverseSqrt + inverseSqrtPrecision));
	double expectedMax = deMax(1.0 / (expectedInverseSqrt - inverseSqrtPrecision), 1.0 / (expectedInverseSqrt + inverseSqrtPrecision));

	expectedMin -= getPrecisionAt(expectedMin, 2.5, returnedFloat.MANTISSA_BITS);
	expectedMax += getPrecisionAt(expectedMax, 2.5, returnedFloat.MANTISSA_BITS);

	if (returnedFloat.asDouble() >= expectedMin  && returnedFloat.asDouble() <= expectedMax)
		return true;

	log << TestLog::Message << "Expected result to be +0 or in range"
		<< " (" << expectedMin << ", " << expectedMax << "), got "
		<< returnedFloat.asDouble() << TestLog::EndMessage;
	return false;
}

// Function used to compare test result with expected output.
// TYPE can be Float16, Float32 or Float64.
// FLOAT_TYPE can be deFloat16, float, double.
template <typename TYPE, typename FLOAT_TYPE>
bool compareBytes(vector<deUint8>& expectedBytes, AllocationSp outputAlloc, TestLog& log)
{
	const TYPE* returned	= static_cast<const TYPE*>(outputAlloc->getHostPtr());
	const TYPE* fValueId	= reinterpret_cast<const TYPE*>(&expectedBytes.front());

	// all test return single value
	DE_ASSERT((expectedBytes.size() / sizeof(TYPE)) == 1);

	// during test setup we do not store expected value but id that can be used to
	// retrieve actual value - this is done to handle special cases like multiple
	// allowed results or epsilon checks for some cases
	// note that this is workaround - this should be done by changing
	// ComputerShaderCase and GraphicsShaderCase so that additional arguments can
	// be passed to this verification callback
	typedef typename TYPE::StorageType SType;
	SType		expectedInt		= fValueId[0].bits();
	ValueId		expectedValueId	= static_cast<ValueId>(expectedInt);

	// something went wrong, expected value cant be V_UNUSED,
	// if this is the case then test shouldn't be created at all
	DE_ASSERT(expectedValueId != V_UNUSED);

	TYPE returnedFloat = returned[0];

	log << TestLog::Message << "Calculated result: " << toHex(returnedFloat.bits())
		<< " (" << returnedFloat.asFloat() << ")" << TestLog::EndMessage;

	if (expectedValueId == V_NAN)
	{
		if (returnedFloat.isNaN())
			return true;

		log << TestLog::Message << "Expected NaN" << TestLog::EndMessage;
		return false;
	}

	if (expectedValueId == V_DENORM)
	{
		if (returnedFloat.isDenorm())
			return true;

		log << TestLog::Message << "Expected Denorm" << TestLog::EndMessage;
		return false;
	}

	// handle multiple acceptable results cases
	if (expectedValueId == V_ZERO_OR_MINUS_ZERO)
	{
		if (returnedFloat.isZero())
			return true;

		log << TestLog::Message << "Expected 0 or -0" << TestLog::EndMessage;
		return false;
	}
	if (expectedValueId == V_ZERO_OR_ONE)
		return isZeroOrOtherValue<TYPE, FLOAT_TYPE>(returnedFloat, V_ONE, log);
	if ((expectedValueId == V_ZERO_OR_FP16_DENORM_TO_FP32) || (expectedValueId == V_ZERO_OR_FP16_DENORM_TO_FP64))
		return isZeroOrOtherValue<TYPE, FLOAT_TYPE>(returnedFloat, V_CONV_DENORM_SMALLER, log);
	if (expectedValueId == V_ZERO_OR_FP32_DENORM_TO_FP64)
		return isZeroOrOtherValue<TYPE, FLOAT_TYPE>(returnedFloat, V_CONV_DENORM_BIGGER, log);
	if (expectedValueId == V_MINUS_ONE_OR_CLOSE)
	{
		// this expected value is only needed for fp16
		DE_ASSERT(returnedFloat.EXPONENT_BIAS == 15);
		typename TYPE::StorageType returnedValue = returnedFloat.bits();
		return (returnedValue == 0xbc00) || (returnedValue == 0xbbff);
	}

	// handle trigonometric operations precision errors
	if (expectedValueId == V_TRIG_ONE)
		return isCosResultCorrect<TYPE>(returnedFloat, log);

	// handle acos(0) case
	if (expectedValueId == V_PI_DIV_2)
		return isAcosResultCorrect<TYPE>(returnedFloat, log);

	TypeValues<FLOAT_TYPE> typeValues;

	if (expectedValueId == V_MINUS_INF_OR_LOG_DENORM)
		return isLogResultCorrect<TYPE>(returnedFloat, typeValues.getValue(V_DENORM), deLog, log);

	if (expectedValueId == V_MINUS_INF_OR_LOG2_DENORM)
		return isLogResultCorrect<TYPE>(returnedFloat, typeValues.getValue(V_DENORM), deLog2, log);

	if (expectedValueId == V_ZERO_OR_SQRT_DENORM)
		return isSqrtResultCorrect<TYPE>(returnedFloat, typeValues.getValue(V_DENORM), log);

	if (expectedValueId == V_INF_OR_INV_SQRT_DENORM)
		return isInverseSqrtResultCorrect<TYPE>(returnedFloat, typeValues.getValue(V_DENORM), log);


	typename RawConvert<FLOAT_TYPE, SType>::Value value;
	value.fp = typeValues.getValue(expectedValueId);

	if (returnedFloat.bits() == value.ui)
		return true;

	log << TestLog::Message << "Expected " << toHex(value.ui)
		<< " (" << value.fp << ")" << TestLog::EndMessage;
	return false;
}

template <typename TYPE, typename FLOAT_TYPE>
bool checkFloats (const vector<Resource>&		,
				  const vector<AllocationSp>&	outputAllocs,
				  const vector<Resource>&		expectedOutputs,
				  TestLog&						log)
{
	if (outputAllocs.size() != expectedOutputs.size())
		return false;

	for (deUint32 outputNdx = 0; outputNdx < outputAllocs.size(); ++outputNdx)
	{
		vector<deUint8> expectedBytes;
		expectedOutputs[outputNdx].getBytes(expectedBytes);

		if (!compareBytes<TYPE, FLOAT_TYPE>(expectedBytes, outputAllocs[outputNdx], log))
			return false;
	}

	return true;
}

bool checkMixedFloats (const vector<Resource>&		,
					   const vector<AllocationSp>&	outputAllocs,
					   const vector<Resource>&		expectedOutputs,
					   TestLog&						log)
{
	// this function validates buffers containing floats of diferent widths, order is not important

	if (outputAllocs.size() != expectedOutputs.size())
		return false;

	// create map storing functions that should be used for comparision
	// depending on float width in bytes; this lets us later to avoid switch in while
	typedef bool (*compareFun)(vector<deUint8>& expectedBytes, AllocationSp outputAlloc, TestLog& log);
	const map<size_t, compareFun> compareMap =
	{
		{ 2, compareBytes<Float16, deFloat16> },
		{ 4, compareBytes<Float32, float> },
		{ 8, compareBytes<Float64, double>},
	};

	vector<deUint8> expectedBytes;
	bool			allResultsAreCorrect	= true;
	int				resultIndex				= static_cast<int>(outputAllocs.size());

	while (resultIndex--)
	{
		expectedOutputs[resultIndex].getBytes(expectedBytes);
		size_t byteWidth		 = expectedOutputs[resultIndex].getByteSize();
		allResultsAreCorrect	&= compareMap.at(byteWidth)(expectedBytes, outputAllocs[resultIndex], log);
	}

	return allResultsAreCorrect;
}

// Base class for ComputeTestGroupBuilder and GrephicstestGroupBuilder classes.
// It contains all functionalities that are used by both child classes.
class TestGroupBuilderBase
{
public:

	TestGroupBuilderBase();
	virtual ~TestGroupBuilderBase() = default;

	virtual void createOperationTests(TestCaseGroup* parentGroup,
									  const char* groupName,
									  FloatType floatType,
									  bool argumentsFromInput) = 0;

	virtual void createSettingsTests(TestCaseGroup* parentGroup) = 0;

protected:

	typedef vector<OperationTestCase> TestCaseVect;

	// Structure containing all data required to create single operation test.
	struct OperationTestCaseInfo
	{
		FloatType					outFloatType;
		bool						argumentsFromInput;
		VkShaderStageFlagBits		testedStage;
		const Operation&			operation;
		const OperationTestCase&	testCase;
	};

	// Mode used by SettingsTestCaseInfo to specify what settings do we want to test.
	enum SettingsMode
	{
		SM_ROUNDING			= 0,
		SM_DENORMS
	};

	// Enum containing available options. When rounding is tested only SO_RTE and SO_RTZ
	// should be used. SO_FLUSH and SO_PRESERVE should be used only for denorm tests.
	enum SettingsOption
	{
		SO_UNUSED			= 0,
		SO_RTE,
		SO_RTZ,
		SO_FLUSH,
		SO_PRESERVE
	};

	// Structure containing all data required to create single settings test.
	struct SettingsTestCaseInfo
	{
		const char*								name;
		SettingsMode							testedMode;
		VkShaderFloatControlsIndependence		independenceSetting;

		SettingsOption							fp16Option;
		SettingsOption							fp32Option;
		SettingsOption							fp64Option;
	};

	void specializeOperation(const OperationTestCaseInfo&	testCaseInfo,
							 SpecializedOperation&			specializedOperation) const;

	void getBehaviorCapabilityAndExecutionMode(BehaviorFlags behaviorFlags,
											   const string inBitWidth,
											   const string outBitWidth,
											   string& capability,
											   string& executionMode) const;

	void setupVulkanFeatures(FloatType			inFloatType,
							 FloatType			outFloatType,
							 BehaviorFlags		behaviorFlags,
							 bool				float64FeatureRequired,
							 VulkanFeatures&	features) const;

protected:

	struct TypeData
	{
		TypeValuesSP		values;
		TypeSnippetsSP		snippets;
		TypeTestResultsSP	testResults;
	};

	// Type specific parameters are stored in this map.
	map<FloatType, TypeData> m_typeData;

	// Map converting behaviuor id to OpCapability instruction
	typedef map<BehaviorFlagBits, string> BehaviorNameMap;
	BehaviorNameMap m_behaviorToName;
};

TestGroupBuilderBase::TestGroupBuilderBase()
{
	m_typeData[FP16] = TypeData();
	m_typeData[FP16].values			= TypeValuesSP(new TypeValues<deFloat16>);
	m_typeData[FP16].snippets		= TypeSnippetsSP(new TypeSnippets<deFloat16>);
	m_typeData[FP16].testResults	= TypeTestResultsSP(new TypeTestResults<deFloat16>);
	m_typeData[FP32] = TypeData();
	m_typeData[FP32].values			= TypeValuesSP(new TypeValues<float>);
	m_typeData[FP32].snippets		= TypeSnippetsSP(new TypeSnippets<float>);
	m_typeData[FP32].testResults	= TypeTestResultsSP(new TypeTestResults<float>);
	m_typeData[FP64] = TypeData();
	m_typeData[FP64].values			= TypeValuesSP(new TypeValues<double>);
	m_typeData[FP64].snippets		= TypeSnippetsSP(new TypeSnippets<double>);
	m_typeData[FP64].testResults	= TypeTestResultsSP(new TypeTestResults<double>);

	m_behaviorToName[B_DENORM_PRESERVE]	= "DenormPreserve";
	m_behaviorToName[B_DENORM_FLUSH]	= "DenormFlushToZero";
	m_behaviorToName[B_ZIN_PRESERVE]	= "SignedZeroInfNanPreserve";
	m_behaviorToName[B_RTE_ROUNDING]	= "RoundingModeRTE";
	m_behaviorToName[B_RTZ_ROUNDING]	= "RoundingModeRTZ";
}

void TestGroupBuilderBase::specializeOperation (const OperationTestCaseInfo&	testCaseInfo,
												SpecializedOperation&			specializedOperation) const
{
	const string		typeToken		= "_float";
	const string		widthToken		= "${float_width}";

	FloatType				outFloatType	= testCaseInfo.outFloatType;
	const Operation&		operation		= testCaseInfo.operation;
	const TypeSnippetsSP	outTypeSnippets	= m_typeData.at(outFloatType).snippets;
	const bool				inputRestricted	= operation.isInputTypeRestricted;
	FloatType				inFloatType		= operation.restrictedInputType;

	// usually input type is same as output but this is not the case for conversion
	// operations; in those cases operation definitions have restricted input type
	inFloatType = inputRestricted ? inFloatType : outFloatType;

	TypeSnippetsSP inTypeSnippets = m_typeData.at(inFloatType).snippets;

	const string inTypePrefix	= string("_f") + inTypeSnippets->bitWidth;
	const string outTypePrefix	= string("_f") + outTypeSnippets->bitWidth;

	specializedOperation.constants		= replace(operation.constants, typeToken, inTypePrefix);
	specializedOperation.annotations	= replace(operation.annotations, widthToken, outTypeSnippets->bitWidth);
	specializedOperation.types			= replace(operation.types, typeToken, outTypePrefix);
	specializedOperation.variables		= replace(operation.variables, typeToken, outTypePrefix);
	specializedOperation.functions		= replace(operation.functions, typeToken, outTypePrefix);
	specializedOperation.commands		= replace(operation.commands, typeToken, outTypePrefix);

	specializedOperation.inFloatType				= inFloatType;
	specializedOperation.inTypeSnippets				= inTypeSnippets;
	specializedOperation.outTypeSnippets			= outTypeSnippets;
	specializedOperation.argumentsUsesFloatConstant	= 0;

	if (operation.isSpecConstant)
		return;

	// select way arguments are prepared
	if (testCaseInfo.argumentsFromInput)
	{
		// read arguments from input SSBO in main function
		specializedOperation.arguments = inTypeSnippets->argumentsFromInputSnippet;
	}
	else
	{
		// generate proper values in main function
		const string arg1 = "%arg1                 = ";
		const string arg2 = "%arg2                 = ";

		const ValueId* inputArguments = testCaseInfo.testCase.input;
		if (inputArguments[0] != V_UNUSED)
		{
			specializedOperation.arguments					= arg1 + inTypeSnippets->valueIdToSnippetArgMap.at(inputArguments[0]);
			specializedOperation.argumentsUsesFloatConstant	|= B_STATEMENT_USAGE_ARGS_CONST_FLOAT;
		}
		if (inputArguments[1] != V_UNUSED)
		{
			specializedOperation.arguments					+= arg2 + inTypeSnippets->valueIdToSnippetArgMap.at(inputArguments[1]);
			specializedOperation.argumentsUsesFloatConstant	|= B_STATEMENT_USAGE_ARGS_CONST_FLOAT;
		}
	}
}


void TestGroupBuilderBase::getBehaviorCapabilityAndExecutionMode(BehaviorFlags behaviorFlags,
																 const string inBitWidth,
																 const string outBitWidth,
																 string& capability,
																 string& executionMode) const
{
	// iterate over all behaviours and request those that are needed
	BehaviorNameMap::const_iterator it = m_behaviorToName.begin();
	while (it != m_behaviorToName.end())
	{
		BehaviorFlagBits	behaviorId		= it->first;
		string				behaviorName	= it->second;

		if (behaviorFlags & behaviorId)
		{
			capability += "OpCapability " + behaviorName + "\n";

			// rounding mode should be obeyed for destination type
			bool rounding = (behaviorId == B_RTE_ROUNDING) || (behaviorId == B_RTZ_ROUNDING);
			executionMode += "OpExecutionMode %main " + behaviorName + " " +
							 (rounding ? outBitWidth : inBitWidth) + "\n";
		}

		++it;
	}

	DE_ASSERT(!capability.empty() && !executionMode.empty());
}

void TestGroupBuilderBase::setupVulkanFeatures(FloatType		inFloatType,
											   FloatType		outFloatType,
											   BehaviorFlags	behaviorFlags,
											   bool				float64FeatureRequired,
											   VulkanFeatures&	features) const
{
	features.coreFeatures.shaderFloat64 = float64FeatureRequired;

	// request proper float controls features
	ExtensionFloatControlsFeatures& floatControls = features.floatControlsProperties;

	// rounding mode should obey the destination type
	bool rteRounding = (behaviorFlags & B_RTE_ROUNDING) != 0;
	bool rtzRounding = (behaviorFlags & B_RTZ_ROUNDING) != 0;
	if (rteRounding || rtzRounding)
	{
		switch(outFloatType)
		{
		case FP16:
			floatControls.shaderRoundingModeRTEFloat16 = rteRounding;
			floatControls.shaderRoundingModeRTZFloat16 = rtzRounding;
			return;
		case FP32:
			floatControls.shaderRoundingModeRTEFloat32 = rteRounding;
			floatControls.shaderRoundingModeRTZFloat32 = rtzRounding;
			return;
		case FP64:
			floatControls.shaderRoundingModeRTEFloat64 = rteRounding;
			floatControls.shaderRoundingModeRTZFloat64 = rtzRounding;
			return;
		}
	}

	switch(inFloatType)
	{
	case FP16:
		floatControls.shaderDenormPreserveFloat16			= behaviorFlags & B_DENORM_PRESERVE;
		floatControls.shaderDenormFlushToZeroFloat16		= behaviorFlags & B_DENORM_FLUSH;
		floatControls.shaderSignedZeroInfNanPreserveFloat16	= behaviorFlags & B_ZIN_PRESERVE;
		return;
	case FP32:
		floatControls.shaderDenormPreserveFloat32			= behaviorFlags & B_DENORM_PRESERVE;
		floatControls.shaderDenormFlushToZeroFloat32		= behaviorFlags & B_DENORM_FLUSH;
		floatControls.shaderSignedZeroInfNanPreserveFloat32	= behaviorFlags & B_ZIN_PRESERVE;
		return;
	case FP64:
		floatControls.shaderDenormPreserveFloat64			= behaviorFlags & B_DENORM_PRESERVE;
		floatControls.shaderDenormFlushToZeroFloat64		= behaviorFlags & B_DENORM_FLUSH;
		floatControls.shaderSignedZeroInfNanPreserveFloat64	= behaviorFlags & B_ZIN_PRESERVE;
		return;
	}
}

// Test case not related to SPIR-V but executed with compute tests. It checks if specified
// features are set to the same value when specific independence settings are used.
tcu::TestStatus verifyIndependenceSettings(Context& context)
{
	if (!context.isDeviceFunctionalitySupported("VK_KHR_shader_float_controls"))
		TCU_THROW(NotSupportedError, "VK_KHR_shader_float_controls not supported");

	vk::VkPhysicalDeviceFloatControlsPropertiesKHR	fcProperties;
	fcProperties.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR;
	fcProperties.pNext	= DE_NULL;

	vk::VkPhysicalDeviceProperties2 deviceProperties;
	deviceProperties.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties.pNext	= &fcProperties;

	auto fail = [](const string& featureGroup)
	{
		return tcu::TestStatus::fail(featureGroup + " features should be set to the same value");
	};

	const VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
	const vk::InstanceInterface&	instanceInterface	= context.getInstanceInterface();
	instanceInterface.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

	if (fcProperties.roundingModeIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE_KHR)
	{
		vk::VkBool32 fp16rte = fcProperties.shaderRoundingModeRTEFloat16;
		vk::VkBool32 fp32rte = fcProperties.shaderRoundingModeRTEFloat32;
		vk::VkBool32 fp64rte = fcProperties.shaderRoundingModeRTEFloat64;
		if ((fp16rte != fp32rte) || (fp32rte != fp64rte))
			return fail("shaderRoundingModeRTEFloat*");

		vk::VkBool32 fp16rtz = fcProperties.shaderRoundingModeRTZFloat16;
		vk::VkBool32 fp32rtz = fcProperties.shaderRoundingModeRTZFloat32;
		vk::VkBool32 fp64rtz = fcProperties.shaderRoundingModeRTZFloat64;
		if ((fp16rtz != fp32rtz) || (fp32rtz != fp64rtz))
			return fail("shaderRoundingModeRTZFloat*");
	}
	else if (fcProperties.roundingModeIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY_KHR)
	{
		vk::VkBool32 fp16rte = fcProperties.shaderRoundingModeRTEFloat16;
		vk::VkBool32 fp64rte = fcProperties.shaderRoundingModeRTEFloat64;
		if ((fp16rte != fp64rte))
			return fail("shaderRoundingModeRTEFloat16 and 64");

		vk::VkBool32 fp16rtz = fcProperties.shaderRoundingModeRTZFloat16;
		vk::VkBool32 fp64rtz = fcProperties.shaderRoundingModeRTZFloat64;
		if ((fp16rtz != fp64rtz))
			return fail("shaderRoundingModeRTZFloat16 and 64");
	}

	if (fcProperties.denormBehaviorIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE_KHR)
	{
		vk::VkBool32 fp16flush = fcProperties.shaderDenormFlushToZeroFloat16;
		vk::VkBool32 fp32flush = fcProperties.shaderDenormFlushToZeroFloat32;
		vk::VkBool32 fp64flush = fcProperties.shaderDenormFlushToZeroFloat64;
		if ((fp16flush != fp32flush) || (fp32flush != fp64flush))
			return fail("shaderDenormFlushToZeroFloat*");

		vk::VkBool32 fp16preserve = fcProperties.shaderDenormPreserveFloat16;
		vk::VkBool32 fp32preserve = fcProperties.shaderDenormPreserveFloat32;
		vk::VkBool32 fp64preserve = fcProperties.shaderDenormPreserveFloat64;
		if ((fp16preserve != fp32preserve) || (fp32preserve != fp64preserve))
			return fail("shaderDenormPreserveFloat*");
	}
	else if (fcProperties.denormBehaviorIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY_KHR)
	{
		vk::VkBool32 fp16flush = fcProperties.shaderDenormFlushToZeroFloat16;
		vk::VkBool32 fp64flush = fcProperties.shaderDenormFlushToZeroFloat64;
		if ((fp16flush != fp64flush))
			return fail("shaderDenormFlushToZeroFloat16 and 64");

		vk::VkBool32 fp16preserve = fcProperties.shaderDenormPreserveFloat16;
		vk::VkBool32 fp64preserve = fcProperties.shaderDenormPreserveFloat64;
		if ((fp16preserve != fp64preserve))
			return fail("shaderDenormPreserveFloat16 and 64");
	}

	return tcu::TestStatus::pass("Pass");
}

// ComputeTestGroupBuilder contains logic that creates compute shaders
// for all test cases. As most tests in spirv-assembly it uses functionality
// implemented in vktSpvAsmComputeShaderTestUtil.cpp.
class ComputeTestGroupBuilder: public TestGroupBuilderBase
{
public:

	void init();

	void createOperationTests(TestCaseGroup* parentGroup,
							  const char* groupName,
							  FloatType floatType,
							  bool argumentsFromInput) override;

	void createSettingsTests(TestCaseGroup* parentGroup) override;

protected:

	void fillShaderSpec(const OperationTestCaseInfo&	testCaseInfo,
						ComputeShaderSpec&				csSpec) const;
	void fillShaderSpec(const SettingsTestCaseInfo&		testCaseInfo,
						ComputeShaderSpec&				csSpec) const;

private:


	StringTemplate		m_operationShaderTemplate;
	StringTemplate		m_settingsShaderTemplate;
	TestCasesBuilder	m_operationTestCaseBuilder;
};

void ComputeTestGroupBuilder::init()
{
	m_operationTestCaseBuilder.init();

	// generic compute shader template with common code for all
	// float types and all possible operations listed in OperationId enum
	m_operationShaderTemplate.setString(
		"OpCapability Shader\n"
		"${capabilities}"

		"OpExtension \"SPV_KHR_float_controls\"\n"
		"${extensions}"

		"%std450            = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"${execution_mode}"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

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
		"%c_u32_1              = OpConstant %type_u32 1\n"

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

	m_settingsShaderTemplate.setString(
		"OpCapability Shader\n"
		"${capabilities}"

		"OpExtension \"SPV_KHR_float_controls\"\n"
		"${extensions}"

		"%std450 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"${execution_modes}"

		// annotations
		"OpDecorate %SSBO_in BufferBlock\n"
		"OpDecorate %ssbo_in DescriptorSet 0\n"
		"OpDecorate %ssbo_in Binding 0\n"
		"OpDecorate %ssbo_in NonWritable\n"
		"${io_annotations}"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		// types
		"%type_void            = OpTypeVoid\n"
		"%type_voidf           = OpTypeFunction %type_void\n"
		"%type_u32             = OpTypeInt 32 0\n"
		"%type_i32             = OpTypeInt 32 1\n"
		"%type_i32_fptr        = OpTypePointer Function %type_i32\n"
		"%type_u32_vec3        = OpTypeVector %type_u32 3\n"
		"%type_u32_vec3_ptr    = OpTypePointer Input %type_u32_vec3\n"

		"%c_i32_0              = OpConstant %type_i32 0\n"
		"%c_i32_1              = OpConstant %type_i32 1\n"
		"%c_i32_2              = OpConstant %type_i32 2\n"

		"${types}"

		// in SSBO definition
		"%SSBO_in              = OpTypeStruct ${in_struct}\n"
		"%up_SSBO_in           = OpTypePointer Uniform %SSBO_in\n"
		"%ssbo_in              = OpVariable %up_SSBO_in Uniform\n"

		// out SSBO definitions
		"${out_definitions}"

		"%id                   = OpVariable %type_u32_vec3_ptr Input\n"
		"%main                 = OpFunction %type_void None %type_voidf\n"
		"%label                = OpLabel\n"

		"${commands}"

		"${save_result}"

		"OpReturn\n"
		"OpFunctionEnd\n");
}

void ComputeTestGroupBuilder::createOperationTests(TestCaseGroup* parentGroup, const char* groupName, FloatType floatType, bool argumentsFromInput)
{
	TestContext&	testCtx	= parentGroup->getTestContext();
	TestCaseGroup*	group	= new TestCaseGroup(testCtx, groupName, "");
	parentGroup->addChild(group);

	TestCaseVect testCases;
	m_operationTestCaseBuilder.build(testCases, m_typeData[floatType].testResults, argumentsFromInput);

	TestCaseVect::const_iterator currTestCase = testCases.begin();
	TestCaseVect::const_iterator lastTestCase = testCases.end();
	while(currTestCase != lastTestCase)
	{
		const OperationTestCase& testCase = *currTestCase;
		++currTestCase;

		// skip cases with undefined output
		if (testCase.expectedOutput == V_UNUSED)
			continue;

		OperationTestCaseInfo testCaseInfo =
		{
			floatType,
			argumentsFromInput,
			VK_SHADER_STAGE_COMPUTE_BIT,
			m_operationTestCaseBuilder.getOperation(testCase.operationId),
			testCase
		};

		ComputeShaderSpec	csSpec;

		fillShaderSpec(testCaseInfo, csSpec);

		string testName = replace(testCase.baseName, "op", testCaseInfo.operation.name);
		group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), "", csSpec));
	}
}

void ComputeTestGroupBuilder::createSettingsTests(TestCaseGroup* parentGroup)
{
	TestContext&	testCtx	= parentGroup->getTestContext();
	TestCaseGroup*	group	= new TestCaseGroup(testCtx, "independence_settings", "");
	parentGroup->addChild(group);

	using SFCI = VkShaderFloatControlsIndependence;
	const SFCI independence32	= VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY_KHR;
	const SFCI independenceAll	= VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR;

	vector<SettingsTestCaseInfo> testCases =
	{
		// name															mode			independenceSetting		fp16Option		fp32Option		fp64Option

		// test rounding modes when only two float widths are available
		{ "rounding_ind_all_fp16_rte_fp32_rtz",							SM_ROUNDING,	independenceAll,		SO_RTE,			SO_RTZ,			SO_UNUSED },
		{ "rounding_ind_all_fp16_rtz_fp32_rte",							SM_ROUNDING,	independenceAll,		SO_RTZ,			SO_RTE,			SO_UNUSED },
		{ "rounding_ind_32_fp16_rte_fp32_rtz",							SM_ROUNDING,	independence32,			SO_RTE,			SO_RTZ,			SO_UNUSED },
		{ "rounding_ind_32_fp16_rtz_fp32_rte",							SM_ROUNDING,	independence32,			SO_RTZ,			SO_RTE,			SO_UNUSED },
		{ "rounding_ind_all_fp16_rte_fp64_rtz",							SM_ROUNDING,	independenceAll,		SO_RTE,			SO_UNUSED,		SO_RTZ },
		{ "rounding_ind_all_fp16_rtz_fp64_rte",							SM_ROUNDING,	independenceAll,		SO_RTZ,			SO_UNUSED,		SO_RTE },
		{ "rounding_ind_all_fp32_rte_fp64_rtz",							SM_ROUNDING,	independenceAll,		SO_UNUSED,		SO_RTE,			SO_RTZ },
		{ "rounding_ind_all_fp32_rtz_fp64_rte",							SM_ROUNDING,	independenceAll,		SO_UNUSED,		SO_RTZ,			SO_RTE },
		{ "rounding_ind_32_fp32_rte_fp64_rtz",							SM_ROUNDING,	independence32,			SO_UNUSED,		SO_RTE,			SO_RTZ },
		{ "rounding_ind_32_fp32_rtz_fp64_rte",							SM_ROUNDING,	independence32,			SO_UNUSED,		SO_RTZ,			SO_RTE },

		// test rounding modes when three widths are available
		{ "rounding_ind_all_fp16_rtz_fp32_rte_fp64_rtz",				SM_ROUNDING,	independenceAll,		SO_RTZ,			SO_RTE,			SO_RTZ },
		{ "rounding_ind_32_fp16_rtz_fp32_rte_fp64_rtz",					SM_ROUNDING,	independence32,			SO_RTZ,			SO_RTE,			SO_RTZ },
		{ "rounding_ind_all_fp16_rte_fp32_rtz_fp64_rte",				SM_ROUNDING,	independenceAll,		SO_RTE,			SO_RTZ,			SO_RTE },
		{ "rounding_ind_32_fp16_rte_fp32_rtz_fp64_rte",					SM_ROUNDING,	independence32,			SO_RTE,			SO_RTZ,			SO_RTE },
		{ "rounding_ind_all_fp16_rtz_fp32_rtz_fp64_rte",				SM_ROUNDING,	independenceAll,		SO_RTZ,			SO_RTZ,			SO_RTE },
		{ "rounding_ind_all_fp16_rtz_fp32_rte_fp64_rte",				SM_ROUNDING,	independenceAll,		SO_RTZ,			SO_RTE,			SO_RTE },
		{ "rounding_ind_all_fp16_rte_fp32_rte_fp64_rtz",				SM_ROUNDING,	independenceAll,		SO_RTE,			SO_RTE,			SO_RTZ },
		{ "rounding_ind_all_fp16_rte_fp32_rtz_fp64_rtz",				SM_ROUNDING,	independenceAll,		SO_RTE,			SO_RTZ,			SO_RTZ },

		// test denorm settings when only two float widths are available
		{ "denorm_ind_all_fp16_flush_fp32_preserve",					SM_DENORMS,		independenceAll,		SO_FLUSH,		SO_PRESERVE,	SO_UNUSED },
		{ "denorm_ind_all_fp16_preserve_fp32_flush",					SM_DENORMS,		independenceAll,		SO_PRESERVE,	SO_FLUSH,		SO_UNUSED },
		{ "denorm_ind_32_fp16_flush_fp32_preserve",						SM_DENORMS,		independence32,			SO_FLUSH,		SO_PRESERVE,	SO_UNUSED },
		{ "denorm_ind_32_fp16_preserve_fp32_flush",						SM_DENORMS,		independence32,			SO_PRESERVE,	SO_FLUSH,		SO_UNUSED },
		{ "denorm_ind_all_fp16_flush_fp64_preserve",					SM_DENORMS,		independenceAll,		SO_FLUSH,		SO_UNUSED,		SO_PRESERVE },
		{ "denorm_ind_all_fp16_preserve_fp64_flush",					SM_DENORMS,		independenceAll,		SO_PRESERVE,	SO_UNUSED,		SO_FLUSH },
		{ "denorm_ind_all_fp32_flush_fp64_preserve",					SM_DENORMS,		independenceAll,		SO_UNUSED,		SO_FLUSH,		SO_PRESERVE },
		{ "denorm_ind_all_fp32_preserve_fp64_flush",					SM_DENORMS,		independenceAll,		SO_UNUSED,		SO_PRESERVE,	SO_FLUSH },
		{ "denorm_ind_32_fp32_flush_fp64_preserve",						SM_DENORMS,		independence32,			SO_UNUSED,		SO_FLUSH,		SO_PRESERVE },
		{ "denorm_ind_32_fp32_preserve_fp64_flush",						SM_DENORMS,		independence32,			SO_UNUSED,		SO_PRESERVE,	SO_FLUSH },

		// test denorm settings when three widths are available
		{ "denorm_ind_all_fp16_preserve_fp32_flush_fp64_preserve",		SM_DENORMS,		independenceAll,		SO_PRESERVE,	SO_FLUSH,		SO_PRESERVE },
		{ "denorm_ind_32_fp16_preserve_fp32_flush_fp64_preserve",		SM_DENORMS,		independence32,			SO_PRESERVE,	SO_FLUSH,		SO_PRESERVE },
		{ "denorm_ind_all_fp16_flush_fp32_preserve_fp64_flush",			SM_DENORMS,		independenceAll,		SO_FLUSH,		SO_PRESERVE,	SO_FLUSH },
		{ "denorm_ind_32_fp16_flush_fp32_preserve_fp64_flush",			SM_DENORMS,		independence32,			SO_FLUSH,		SO_PRESERVE,	SO_FLUSH },
		{ "denorm_ind_all_fp16_preserve_fp32_preserve_fp64_flush",		SM_DENORMS,		independenceAll,		SO_PRESERVE,	SO_PRESERVE,	SO_FLUSH },
		{ "denorm_ind_all_fp16_preserve_fp32_flush_fp64_flush",			SM_DENORMS,		independenceAll,		SO_PRESERVE,	SO_FLUSH,		SO_FLUSH },
		{ "denorm_ind_all_fp16_flush_fp32_flush_fp64_preserve",			SM_DENORMS,		independenceAll,		SO_FLUSH,		SO_FLUSH,		SO_PRESERVE },
		{ "denorm_ind_all_fp16_flush_fp32_preserve_fp64_preserve",		SM_DENORMS,		independenceAll,		SO_FLUSH,		SO_PRESERVE,	SO_PRESERVE }
	};

	for(const auto& testCase : testCases)
	{
		ComputeShaderSpec	csSpec;
		fillShaderSpec(testCase, csSpec);
		group->addChild(new SpvAsmComputeShaderCase(testCtx, testCase.name, "", csSpec));
	}

	addFunctionCase(group, "independence_settings", "", verifyIndependenceSettings);
}

void ComputeTestGroupBuilder::fillShaderSpec(const OperationTestCaseInfo&	testCaseInfo,
											 ComputeShaderSpec&				csSpec) const
{
	// LUT storing functions used to verify test results
	const VerifyIOFunc checkFloatsLUT[] =
	{
		checkFloats<Float16, deFloat16>,
		checkFloats<Float32, float>,
		checkFloats<Float64, double>
	};

	const Operation&			testOperation	= testCaseInfo.operation;
	const OperationTestCase&	testCase		= testCaseInfo.testCase;
	FloatType					outFloatType	= testCaseInfo.outFloatType;

	SpecializedOperation specOpData;
	specializeOperation(testCaseInfo, specOpData);

	TypeSnippetsSP	inTypeSnippets		= specOpData.inTypeSnippets;
	TypeSnippetsSP	outTypeSnippets		= specOpData.outTypeSnippets;
	FloatType		inFloatType			= specOpData.inFloatType;

	// UnpackHalf2x16 is a corner case - it returns two 32-bit floats but
	// internaly operates on fp16 and this type should be used by float controls
	FloatType		inFloatTypeForCaps		= inFloatType;
	string			inFloatWidthForCaps		= inTypeSnippets->bitWidth;
	if (testCase.operationId == O_UPH_DENORM)
	{
		inFloatTypeForCaps	= FP16;
		inFloatWidthForCaps	= "16";
	}

	string behaviorCapability;
	string behaviorExecutionMode;
	getBehaviorCapabilityAndExecutionMode(testCase.behaviorFlags,
										  inFloatWidthForCaps,
										  outTypeSnippets->bitWidth,
										  behaviorCapability,
										  behaviorExecutionMode);

	string capabilities		= behaviorCapability + outTypeSnippets->capabilities;
	string extensions		= outTypeSnippets->extensions;
	string annotations		= inTypeSnippets->inputAnnotationsSnippet + outTypeSnippets->outputAnnotationsSnippet +
							  outTypeSnippets->typeAnnotationsSnippet;
	string types			= outTypeSnippets->typeDefinitionsSnippet;
	string constants		= outTypeSnippets->constantsDefinitionsSnippet;
	string ioDefinitions	= inTypeSnippets->inputDefinitionsSnippet + outTypeSnippets->outputDefinitionsSnippet;

	bool outFp16TypeUsage	= outTypeSnippets->loadStoreRequiresShaderFloat16;
	bool inFp16TypeUsage	= false;

	if (testOperation.isInputTypeRestricted)
	{
		annotations		+= inTypeSnippets->typeAnnotationsSnippet;
		capabilities	+= inTypeSnippets->capabilities;
		extensions		+= inTypeSnippets->extensions;
		types			+= inTypeSnippets->typeDefinitionsSnippet;
		constants		+= inTypeSnippets->constantsDefinitionsSnippet;

		inFp16TypeUsage	= inTypeSnippets->loadStoreRequiresShaderFloat16;
	}

	map<string, string> specializations;
	specializations["extensions"]		= extensions;
	specializations["execution_mode"]	= behaviorExecutionMode;
	specializations["annotations"]		= annotations + specOpData.annotations;
	specializations["types"]			= types + specOpData.types;
	specializations["io_definitions"]	= ioDefinitions;
	specializations["variables"]		= specOpData.variables;
	specializations["functions"]		= specOpData.functions;
	specializations["save_result"]		= outTypeSnippets->storeResultsSnippet;
	specializations["arguments"]		= specOpData.arguments;
	specializations["commands"]			= specOpData.commands;

	// Build constants. They are only needed sometimes.
	const FloatStatementUsageFlags	argsAnyFloatConstMask				= B_STATEMENT_USAGE_ARGS_CONST_FLOAT | B_STATEMENT_USAGE_ARGS_CONST_FP16 | B_STATEMENT_USAGE_ARGS_CONST_FP32 | B_STATEMENT_USAGE_ARGS_CONST_FP64;
	const bool						argsUseFPConstants					= (specOpData.argumentsUsesFloatConstant & argsAnyFloatConstMask) != 0;
	const FloatStatementUsageFlags	commandsAnyFloatConstMask			= B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_CONST_FP16 | B_STATEMENT_USAGE_COMMANDS_CONST_FP32 | B_STATEMENT_USAGE_COMMANDS_CONST_FP64;
	const bool						commandsUseFPConstants				= (testCaseInfo.operation.statementUsageFlags & commandsAnyFloatConstMask) != 0;
	const bool						needConstants						= argsUseFPConstants || commandsUseFPConstants;
	const FloatStatementUsageFlags	constsFloatTypeMask					= B_STATEMENT_USAGE_CONSTS_TYPE_FLOAT | B_STATEMENT_USAGE_CONSTS_TYPE_FP16;
	const bool						constsUsesFP16Type					= (testCaseInfo.operation.statementUsageFlags & constsFloatTypeMask) != 0;
	const bool						loadStoreRequiresShaderFloat16		= inFp16TypeUsage || outFp16TypeUsage;
	const bool						usesFP16Constants					= constsUsesFP16Type || (needConstants && loadStoreRequiresShaderFloat16);

	specializations["constants"]		= "";
	if (needConstants)
	{
		specializations["constants"]	= constants;
	}
	specializations["constants"]		+= specOpData.constants;

	// check which format features are needed
	bool float16FeatureRequired = (outFloatType == FP16) || (inFloatType == FP16);
	bool float64FeatureRequired = (outFloatType == FP64) || (inFloatType == FP64);

	// Determine required capabilities.
	if ((testOperation.floatUsage == FLOAT_ARITHMETIC && float16FeatureRequired) || usesFP16Constants)
	{
		capabilities += "OpCapability Float16\n";
	}
	specializations["capabilities"]		= capabilities;

	// specialize shader
	const string shaderCode = m_operationShaderTemplate.specialize(specializations);

	// construct input and output buffers of proper types
	TypeValuesSP inTypeValues	= m_typeData.at(inFloatType).values;
	TypeValuesSP outTypeValues	= m_typeData.at(outFloatType).values;
	BufferSp inBufferSp			= inTypeValues->constructInputBuffer(testCase.input);
	BufferSp outBufferSp		= outTypeValues->constructOutputBuffer(testCase.expectedOutput);
	csSpec.inputs.push_back(Resource(inBufferSp, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	csSpec.outputs.push_back(Resource(outBufferSp));

	// check which format features are needed
	setupVulkanFeatures(inFloatTypeForCaps,		// usualy same as inFloatType - different only for UnpackHalf2x16
						outFloatType,
						testCase.behaviorFlags,
						float64FeatureRequired,
						csSpec.requestedVulkanFeatures);

	csSpec.assembly			= shaderCode;
	csSpec.numWorkGroups	= IVec3(1, 1, 1);
	csSpec.verifyIO			= checkFloatsLUT[outFloatType];

	csSpec.extensions.push_back("VK_KHR_shader_float_controls");
	bool needShaderFloat16 = false;
	if (float16FeatureRequired)
	{
		csSpec.extensions.push_back("VK_KHR_16bit_storage");
		csSpec.requestedVulkanFeatures.ext16BitStorage = EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK;
		needShaderFloat16 |= testOperation.floatUsage == FLOAT_ARITHMETIC;
	}
	needShaderFloat16 |= usesFP16Constants;
	if (needShaderFloat16)
	{
		csSpec.extensions.push_back("VK_KHR_shader_float16_int8");
		csSpec.requestedVulkanFeatures.extFloat16Int8 = EXTFLOAT16INT8FEATURES_FLOAT16;
	}
	if (float64FeatureRequired)
		csSpec.requestedVulkanFeatures.coreFeatures.shaderFloat64 = VK_TRUE;
}

void ComputeTestGroupBuilder::fillShaderSpec(const SettingsTestCaseInfo&	testCaseInfo,
											 ComputeShaderSpec&				csSpec) const
{
	string		capabilities;
	string		fp16behaviorName;
	string		fp32behaviorName;
	string		fp64behaviorName;

	ValueId		addArgs[2];
	ValueId		fp16resultValue;
	ValueId		fp32resultValue;
	ValueId		fp64resultValue;

	ExtensionFloatControlsFeatures& floatControls = csSpec.requestedVulkanFeatures.floatControlsProperties;
	bool fp16Required	= testCaseInfo.fp16Option != SO_UNUSED;
	bool fp32Required	= testCaseInfo.fp32Option != SO_UNUSED;
	bool fp64Required	= testCaseInfo.fp64Option != SO_UNUSED;

	if (testCaseInfo.testedMode == SM_ROUNDING)
	{
		// make sure that only rounding options are used
		DE_ASSERT((testCaseInfo.fp16Option != SO_FLUSH) ||
				  (testCaseInfo.fp16Option != SO_PRESERVE) ||
				  (testCaseInfo.fp32Option != SO_FLUSH) ||
				  (testCaseInfo.fp32Option != SO_PRESERVE) ||
				  (testCaseInfo.fp64Option != SO_FLUSH) ||
				  (testCaseInfo.fp64Option != SO_PRESERVE));

		bool fp16RteRounding	= testCaseInfo.fp16Option == SO_RTE;
		bool fp32RteRounding	= testCaseInfo.fp32Option == SO_RTE;
		bool fp64RteRounding	= testCaseInfo.fp64Option == SO_RTE;

		const string& rte		= m_behaviorToName.at(B_RTE_ROUNDING);
		const string& rtz		= m_behaviorToName.at(B_RTZ_ROUNDING);

		fp16behaviorName		= fp16RteRounding ? rte : rtz;
		fp32behaviorName		= fp32RteRounding ? rte : rtz;
		fp64behaviorName		= fp64RteRounding ? rte : rtz;

		addArgs[0]				= V_ADD_ARG_A;
		addArgs[1]				= V_ADD_ARG_B;
		fp16resultValue			= fp16RteRounding ? V_ADD_RTE_RESULT : V_ADD_RTZ_RESULT;
		fp32resultValue			= fp32RteRounding ? V_ADD_RTE_RESULT : V_ADD_RTZ_RESULT;
		fp64resultValue			= fp64RteRounding ? V_ADD_RTE_RESULT : V_ADD_RTZ_RESULT;

		capabilities			= "OpCapability " + rte + "\n"
								  "OpCapability " + rtz + "\n";

		floatControls.roundingModeIndependence		= testCaseInfo.independenceSetting;
		floatControls.denormBehaviorIndependence	= VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE_KHR;
		floatControls.shaderRoundingModeRTEFloat16	= fp16RteRounding;
		floatControls.shaderRoundingModeRTZFloat16	= fp16Required && !fp16RteRounding;
		floatControls.shaderRoundingModeRTEFloat32	= fp32RteRounding;
		floatControls.shaderRoundingModeRTZFloat32	= fp32Required && !fp32RteRounding;
		floatControls.shaderRoundingModeRTEFloat64	= fp64RteRounding;
		floatControls.shaderRoundingModeRTZFloat64	= fp64Required && !fp64RteRounding;
	}
	else // SM_DENORMS
	{
		// make sure that only denorm options are used
		DE_ASSERT((testCaseInfo.fp16Option != SO_RTE) ||
				  (testCaseInfo.fp16Option != SO_RTZ) ||
				  (testCaseInfo.fp32Option != SO_RTE) ||
				  (testCaseInfo.fp32Option != SO_RTZ) ||
				  (testCaseInfo.fp64Option != SO_RTE) ||
				  (testCaseInfo.fp64Option != SO_RTZ));

		bool fp16DenormPreserve		= testCaseInfo.fp16Option == SO_PRESERVE;
		bool fp32DenormPreserve		= testCaseInfo.fp32Option == SO_PRESERVE;
		bool fp64DenormPreserve		= testCaseInfo.fp64Option == SO_PRESERVE;

		const string& preserve		= m_behaviorToName.at(B_DENORM_PRESERVE);
		const string& flush			= m_behaviorToName.at(B_DENORM_FLUSH);

		fp16behaviorName			= fp16DenormPreserve ? preserve : flush;
		fp32behaviorName			= fp32DenormPreserve ? preserve : flush;
		fp64behaviorName			= fp64DenormPreserve ? preserve : flush;

		addArgs[0]					= V_DENORM;
		addArgs[1]					= V_DENORM;
		fp16resultValue				= fp16DenormPreserve ? V_DENORM_TIMES_TWO : V_ZERO;
		fp32resultValue				= fp32DenormPreserve ? V_DENORM_TIMES_TWO : V_ZERO;
		fp64resultValue				= fp64DenormPreserve ? V_DENORM_TIMES_TWO : V_ZERO;

		capabilities				= "OpCapability " + preserve + "\n"
									  "OpCapability " + flush + "\n";

		floatControls.denormBehaviorIndependence		= testCaseInfo.independenceSetting;
		floatControls.roundingModeIndependence			= VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE_KHR;
		floatControls.shaderDenormPreserveFloat16		= fp16DenormPreserve;
		floatControls.shaderDenormFlushToZeroFloat16	= fp16Required && !fp16DenormPreserve;
		floatControls.shaderDenormPreserveFloat32		= fp32DenormPreserve;
		floatControls.shaderDenormFlushToZeroFloat32	= fp32Required && !fp32DenormPreserve;
		floatControls.shaderDenormPreserveFloat64		= fp64DenormPreserve;
		floatControls.shaderDenormFlushToZeroFloat64	= fp64Required && !fp64DenormPreserve;
	}

	const auto&	fp64Data			= m_typeData.at(FP64);
	const auto&	fp32Data			= m_typeData.at(FP32);
	const auto&	fp16Data			= m_typeData.at(FP16);

	deUint32	attributeIndex		= 0;
	deUint32	attributeOffset		= 0;
	string		attribute;
	string		extensions			= "";
	string		executionModes		= "";
	string		ioAnnotations		= "";
	string		types				= "";
	string		inStruct			= "";
	string		outDefinitions		= "";
	string		commands			= "";
	string		saveResult			= "";

	// construct single input buffer containing arguments for all float widths
	// (maxPerStageDescriptorStorageBuffers can be min 4 and we need 3 for outputs)
	deUint32				inputOffset	= 0;
	std::vector<deUint8>	inputData	((fp64Required * sizeof(double) + sizeof(float) + fp16Required * sizeof(deFloat16)) * 2);

	// to follow storage buffer layout rules we store data in ssbo in order 64 -> 16
	if (fp64Required)
	{
		capabilities	+= fp64Data.snippets->capabilities;
		executionModes	+= "OpExecutionMode %main " + fp64behaviorName + " 64\n";
		attribute		 = to_string(attributeIndex);
		ioAnnotations	+= "OpMemberDecorate %SSBO_in " + attribute + " Offset " + to_string(attributeOffset) +"\n" +
						   fp64Data.snippets->multiOutputAnnotationsSnippet +
						   "OpDecorate %ssbo_f64_out Binding " + to_string(attributeIndex+1) + "\n";
		types			+= fp64Data.snippets->minTypeDefinitionsSnippet;
		inStruct		+= " %type_f64_arr_2";
		outDefinitions	+= fp64Data.snippets->multiOutputDefinitionsSnippet;
		commands		+= replace(fp64Data.snippets->multiArgumentsFromInputSnippet, "${attr}", attribute) +
						   "%result64             = OpFAdd %type_f64 %arg1_f64 %arg2_f64\n";
		saveResult		+= fp64Data.snippets->multiStoreResultsSnippet;
		attributeOffset += 2 * static_cast<deUint32>(sizeof(double));
		attributeIndex++;

		fp64Data.values->fillInputData(addArgs, inputData, inputOffset);

		// construct separate buffers for outputs to make validation easier
		BufferSp fp64OutBufferSp = fp64Data.values->constructOutputBuffer(fp64resultValue);
		csSpec.outputs.push_back(Resource(fp64OutBufferSp));

		csSpec.requestedVulkanFeatures.coreFeatures.shaderFloat64 = VK_TRUE;
	}
	if (fp32Required)
	{
		executionModes		+= "OpExecutionMode %main " + fp32behaviorName + " 32\n";
		attribute			 = to_string(attributeIndex);
		ioAnnotations		+= "OpMemberDecorate %SSBO_in " + attribute + " Offset " + to_string(attributeOffset) +"\n" +
							   fp32Data.snippets->multiOutputAnnotationsSnippet +
							   "OpDecorate %ssbo_f32_out Binding " + to_string(attributeIndex+1) + "\n";
		types				+= fp32Data.snippets->minTypeDefinitionsSnippet;
		inStruct			+= " %type_f32_arr_2";
		outDefinitions		+= fp32Data.snippets->multiOutputDefinitionsSnippet;
		commands			+= replace(fp32Data.snippets->multiArgumentsFromInputSnippet, "${attr}", attribute) +
							   "%result32             = OpFAdd %type_f32 %arg1_f32 %arg2_f32\n";
		saveResult			+= fp32Data.snippets->multiStoreResultsSnippet;
		attributeOffset		+= 2 * static_cast<deUint32>(sizeof(float));
		attributeIndex++;

		fp32Data.values->fillInputData(addArgs, inputData, inputOffset);

		BufferSp fp32OutBufferSp = fp32Data.values->constructOutputBuffer(fp32resultValue);
		csSpec.outputs.push_back(Resource(fp32OutBufferSp));
	}
	if (fp16Required)
	{
		capabilities	+= fp16Data.snippets->capabilities +
						   "OpCapability Float16\n";
		extensions		+= fp16Data.snippets->extensions;
		executionModes	+= "OpExecutionMode %main " + fp16behaviorName + " 16\n";
		attribute		 = to_string(attributeIndex);
		ioAnnotations	+= "OpMemberDecorate %SSBO_in " + attribute + " Offset " + to_string(attributeOffset) +"\n" +
						   fp16Data.snippets->multiOutputAnnotationsSnippet +
						   "OpDecorate %ssbo_f16_out Binding " + to_string(attributeIndex+1) + "\n";
		types			+= fp16Data.snippets->minTypeDefinitionsSnippet;
		inStruct		+= " %type_f16_arr_2";
		outDefinitions	+= fp16Data.snippets->multiOutputDefinitionsSnippet;
		commands		+= replace(fp16Data.snippets->multiArgumentsFromInputSnippet, "${attr}", attribute) +
						   "%result16             = OpFAdd %type_f16 %arg1_f16 %arg2_f16\n";
		saveResult		+= fp16Data.snippets->multiStoreResultsSnippet;

		fp16Data.values->fillInputData(addArgs, inputData, inputOffset);

		BufferSp fp16OutBufferSp = fp16Data.values->constructOutputBuffer(fp16resultValue);
		csSpec.outputs.push_back(Resource(fp16OutBufferSp));

		csSpec.extensions.push_back("VK_KHR_16bit_storage");
		csSpec.requestedVulkanFeatures.ext16BitStorage = EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK;
	}

	BufferSp inBufferSp(new Buffer<deUint8>(inputData));
	csSpec.inputs.push_back(Resource(inBufferSp, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	map<string, string> specializations =
	{
		{ "capabilities",		capabilities },
		{ "extensions",			extensions },
		{ "execution_modes",	executionModes },
		{ "io_annotations",		ioAnnotations },
		{ "types",				types },
		{ "in_struct",			inStruct },
		{ "out_definitions",	outDefinitions },
		{ "commands",			commands },
		{ "save_result",		saveResult }
	};

	// specialize shader
	const string shaderCode = m_settingsShaderTemplate.specialize(specializations);

	csSpec.assembly			= shaderCode;
	csSpec.numWorkGroups	= IVec3(1, 1, 1);
	csSpec.verifyIO			= checkMixedFloats;
	csSpec.extensions.push_back("VK_KHR_shader_float_controls");
}

void getGraphicsShaderCode (vk::SourceCollections& dst, InstanceContext context)
{
	// this function is used only by GraphicsTestGroupBuilder but it couldn't
	// be implemented as a method because of how addFunctionCaseWithPrograms
	// was implemented

	SpirvVersion	targetSpirvVersion	= context.resources.spirvVersion;
	const deUint32	vulkanVersion		= dst.usedVulkanVersion;

	static const string vertexTemplate =
		"OpCapability Shader\n"
		"${vert_capabilities}"

		"OpExtension \"SPV_KHR_float_controls\"\n"
		"${vert_extensions}"

		"%std450            = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %main \"main\" %BP_stream %BP_position %BP_color %BP_gl_VertexIndex %BP_gl_InstanceIndex %BP_vertex_color %BP_vertex_result \n"
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

		"OpExtension \"SPV_KHR_float_controls\"\n"
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

	dst.spirvAsmSources.add("vert", DE_NULL)
		<< StringTemplate(vertexTemplate).specialize(context.testCodeFragments)
		<< SpirVAsmBuildOptions(vulkanVersion, targetSpirvVersion);
	dst.spirvAsmSources.add("frag", DE_NULL)
		<< StringTemplate(fragmentTemplate).specialize(context.testCodeFragments)
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
class GraphicsTestGroupBuilder: public TestGroupBuilderBase
{
public:

	void init();

	void createOperationTests(TestCaseGroup* parentGroup, const char* groupName, FloatType floatType, bool argumentsFromInput) override;
	void createSettingsTests(TestCaseGroup* parentGroup) override;

protected:

	InstanceContext createInstanceContext(const OperationTestCaseInfo& testCaseInfo) const;

private:

	TestCasesBuilder	m_testCaseBuilder;
};

void GraphicsTestGroupBuilder::init()
{
	m_testCaseBuilder.init();
}

void GraphicsTestGroupBuilder::createOperationTests(TestCaseGroup* parentGroup, const char* groupName, FloatType floatType, bool argumentsFromInput)
{
	TestContext&	testCtx	= parentGroup->getTestContext();
	TestCaseGroup*	group	= new TestCaseGroup(testCtx, groupName, "");
	parentGroup->addChild(group);

	// create test cases for vertex stage
	TestCaseVect testCases;
	m_testCaseBuilder.build(testCases, m_typeData[floatType].testResults, argumentsFromInput);

	TestCaseVect::const_iterator currTestCase = testCases.begin();
	TestCaseVect::const_iterator lastTestCase = testCases.end();
	while(currTestCase != lastTestCase)
	{
		const OperationTestCase& testCase = *currTestCase;
		++currTestCase;

		// skip cases with undefined output
		if (testCase.expectedOutput == V_UNUSED)
			continue;

		// FPRoundingMode decoration can be applied only to conversion instruction that is used as the object
		// argument of an OpStore storing through a pointer to a 16-bit floating-point object in Uniform, or
		// PushConstant, or Input, or Output Storage Classes. SSBO writes are not commonly supported
		// in VS so this test case needs to be skiped for vertex stage.
		if ((testCase.operationId == O_ORTZ_ROUND) || (testCase.operationId == O_ORTE_ROUND))
			continue;

		OperationTestCaseInfo testCaseInfo =
		{
			floatType,
			argumentsFromInput,
			VK_SHADER_STAGE_VERTEX_BIT,
			m_testCaseBuilder.getOperation(testCase.operationId),
			testCase
		};

		InstanceContext ctxVertex	= createInstanceContext(testCaseInfo);
		string			testName	= replace(testCase.baseName, "op", testCaseInfo.operation.name);

		addFunctionCaseWithPrograms<InstanceContext>(group, testName + "_vert", "", getGraphicsShaderCode, runAndVerifyDefaultPipeline, ctxVertex);
	}

	// create test cases for fragment stage
	testCases.clear();
	m_testCaseBuilder.build(testCases, m_typeData[floatType].testResults, argumentsFromInput);

	currTestCase = testCases.begin();
	lastTestCase = testCases.end();
	while(currTestCase != lastTestCase)
	{
		const OperationTestCase& testCase = *currTestCase;
		++currTestCase;

		// skip cases with undefined output
		if (testCase.expectedOutput == V_UNUSED)
			continue;

		OperationTestCaseInfo testCaseInfo =
		{
			floatType,
			argumentsFromInput,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			m_testCaseBuilder.getOperation(testCase.operationId),
			testCase
		};

		InstanceContext ctxFragment	= createInstanceContext(testCaseInfo);
		string			testName	= replace(testCase.baseName, "op", testCaseInfo.operation.name);

		addFunctionCaseWithPrograms<InstanceContext>(group, testName + "_frag", "", getGraphicsShaderCode, runAndVerifyDefaultPipeline, ctxFragment);
	}
}

void GraphicsTestGroupBuilder::createSettingsTests(TestCaseGroup* parentGroup)
{
	DE_UNREF(parentGroup);

	// WG decided that testing settings only for compute stage is sufficient
}

InstanceContext GraphicsTestGroupBuilder::createInstanceContext(const OperationTestCaseInfo& testCaseInfo) const
{
	// LUT storing functions used to verify test results
	const VerifyIOFunc checkFloatsLUT[] =
	{
		checkFloats<Float16, deFloat16>,
		checkFloats<Float32, float>,
		checkFloats<Float64, double>
	};

	// 32-bit float types are always needed for standard operations on color
	// if tested operation does not require fp32 for either input or output
	// then this minimal type definitions must be appended to types section
	const string f32TypeMinimalRequired =
		"%type_f32             = OpTypeFloat 32\n"
		"%type_f32_arr_1       = OpTypeArray %type_f32 %c_i32_1\n"
		"%type_f32_iptr        = OpTypePointer Input %type_f32\n"
		"%type_f32_optr        = OpTypePointer Output %type_f32\n"
		"%type_f32_vec4        = OpTypeVector %type_f32 4\n"
		"%type_f32_vec4_iptr   = OpTypePointer Input %type_f32_vec4\n"
		"%type_f32_vec4_optr   = OpTypePointer Output %type_f32_vec4\n";

	const Operation&			testOperation	= testCaseInfo.operation;
	const OperationTestCase&	testCase		= testCaseInfo.testCase;
	FloatType					outFloatType	= testCaseInfo.outFloatType;
	VkShaderStageFlagBits		testedStage		= testCaseInfo.testedStage;

	DE_ASSERT((testedStage == VK_SHADER_STAGE_VERTEX_BIT) || (testedStage == VK_SHADER_STAGE_FRAGMENT_BIT));

	SpecializedOperation specOpData;
	specializeOperation(testCaseInfo, specOpData);

	TypeSnippetsSP	inTypeSnippets		= specOpData.inTypeSnippets;
	TypeSnippetsSP	outTypeSnippets		= specOpData.outTypeSnippets;
	FloatType		inFloatType			= specOpData.inFloatType;

	// There may be several reasons why we need the shaderFloat16 Vulkan feature.
	bool needsShaderFloat16 = false;
	// There are some weird cases where we need the constants, but would otherwise drop them.
	bool needsSpecialConstants = false;

	// UnpackHalf2x16 is a corner case - it returns two 32-bit floats but
	// internaly operates on fp16 and this type should be used by float controls
	FloatType		inFloatTypeForCaps		= inFloatType;
	string			inFloatWidthForCaps		= inTypeSnippets->bitWidth;
	if (testCase.operationId == O_UPH_DENORM)
	{
		inFloatTypeForCaps	= FP16;
		inFloatWidthForCaps	= "16";
	}

	string behaviorCapability;
	string behaviorExecutionMode;
	getBehaviorCapabilityAndExecutionMode(testCase.behaviorFlags,
										  inFloatWidthForCaps,
										  outTypeSnippets->bitWidth,
										  behaviorCapability,
										  behaviorExecutionMode);

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
		vertFunctions = specOpData.functions;

		// check if input type is different from tested type (conversion operations)
		if (testOperation.isInputTypeRestricted)
		{
			vertCapabilities	= behaviorCapability + inTypeSnippets->capabilities + outTypeSnippets->capabilities;
			fragCapabilities	= outTypeSnippets->capabilities;
			vertExtensions		= inTypeSnippets->extensions + outTypeSnippets->extensions;
			fragExtensions		= outTypeSnippets->extensions;
			vertTypes			= inTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->varyingsTypesSnippet;
			fragTypes			= outTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->varyingsTypesSnippet;
			vertConstants		= inTypeSnippets->constantsDefinitionsSnippet + outTypeSnippets->constantsDefinitionsSnippet;
			fragConstants		= outTypeSnippets->constantsDefinitionsSnippet;
		}
		else
		{
			// input and output types are the same (majority of operations)

			vertCapabilities	= behaviorCapability + outTypeSnippets->capabilities;
			fragCapabilities	= vertCapabilities;
			vertExtensions		= outTypeSnippets->extensions;
			fragExtensions		= vertExtensions;
			vertTypes			= outTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->varyingsTypesSnippet;
			fragTypes			= vertTypes;
			vertConstants		= outTypeSnippets->constantsDefinitionsSnippet;
			fragConstants		= outTypeSnippets->constantsDefinitionsSnippet;
		}

		if (outFloatType != FP32)
		{
			fragTypes += f32TypeMinimalRequired;
			if (inFloatType != FP32)
				vertTypes += f32TypeMinimalRequired;
		}

		vertAnnotations	+= specOpData.annotations;
		vertTypes		+= specOpData.types;
		vertConstants	+= specOpData.constants;

		vertExecutionMode		= behaviorExecutionMode;
		fragExecutionMode		= "";
		vertIODefinitions		= inTypeSnippets->inputDefinitionsSnippet + outTypeSnippets->outputVaryingsSnippet;
		fragIODefinitions		= outTypeSnippets->outputDefinitionsSnippet + outTypeSnippets->inputVaryingsSnippet;
		vertArguments			= specOpData.arguments;
		fragArguments			= "";
		vertVariables			= specOpData.variables;
		fragVariables			= "";
		vertCommands			= specOpData.commands;
		fragCommands			= "";
		vertProcessResult		= outTypeSnippets->storeVertexResultSnippet;
		fragProcessResult		= outTypeSnippets->loadVertexResultSnippet + outTypeSnippets->storeResultsSnippet;
		needsShaderFloat16		|= outTypeSnippets->loadStoreRequiresShaderFloat16;
	}
	else // perform test in fragment stage - vertex stage is empty
	{
		fragFunctions = specOpData.functions;
		// check if input type is different from tested type
		if (testOperation.isInputTypeRestricted)
		{
			fragAnnotations		= inTypeSnippets->inputAnnotationsSnippet + inTypeSnippets->typeAnnotationsSnippet +
								  outTypeSnippets->outputAnnotationsSnippet + outTypeSnippets->typeAnnotationsSnippet;
			fragCapabilities	= behaviorCapability + inTypeSnippets->capabilities + outTypeSnippets->capabilities;
			fragExtensions		= inTypeSnippets->extensions + outTypeSnippets->extensions;
			fragTypes			= inTypeSnippets->typeDefinitionsSnippet + outTypeSnippets->typeDefinitionsSnippet;
			fragConstants		= inTypeSnippets->constantsDefinitionsSnippet + outTypeSnippets->constantsDefinitionsSnippet;
		}
		else
		{
			// input and output types are the same

			fragAnnotations		= inTypeSnippets->inputAnnotationsSnippet + inTypeSnippets->typeAnnotationsSnippet +
								  outTypeSnippets->outputAnnotationsSnippet;
			fragCapabilities	= behaviorCapability + outTypeSnippets->capabilities;
			fragExtensions		= outTypeSnippets->extensions;
			fragTypes			= outTypeSnippets->typeDefinitionsSnippet;
			fragConstants		= outTypeSnippets->constantsDefinitionsSnippet;
		}

		// varying is not used but it needs to be specified so lets use type_i32 for it
		string dummyVertVarying = "%BP_vertex_result     = OpVariable %type_i32_optr Output\n";
		string dummyFragVarying = "%BP_vertex_result     = OpVariable %type_i32_iptr Input\n";

		vertCapabilities	= "";
		vertExtensions		= "";
		vertAnnotations		= "OpDecorate %type_f32_arr_1 ArrayStride 4\n";
		vertTypes			= f32TypeMinimalRequired;
		vertConstants		= "";

		if ((outFloatType != FP32) && (inFloatType != FP32))
			fragTypes += f32TypeMinimalRequired;

		fragAnnotations += specOpData.annotations;
		fragTypes		+= specOpData.types;
		fragConstants	+= specOpData.constants;

		vertExecutionMode	= "";
		fragExecutionMode	= behaviorExecutionMode;
		vertIODefinitions	= dummyVertVarying;
		fragIODefinitions	= inTypeSnippets->inputDefinitionsSnippet +
							  outTypeSnippets->outputDefinitionsSnippet + dummyFragVarying;
		vertArguments		= "";
		fragArguments		= specOpData.arguments;
		vertVariables		= "";
		fragVariables		= specOpData.variables;
		vertCommands		= "";
		fragCommands		= specOpData.commands;
		vertProcessResult	= "";
		fragProcessResult	= outTypeSnippets->storeResultsSnippet;

		if (!testCaseInfo.argumentsFromInput)
		{
			switch(testCaseInfo.testCase.operationId)
			{
				case O_CONV_FROM_FP32:
				case O_CONV_FROM_FP64:
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
	bool needsFP16Constants = needsShaderFloat16 || needsSpecialConstants;

	if (!needsFP16Constants && float16FeatureRequired)
	{
		// Check various code fragments
		const FloatStatementUsageFlags	commandsFloatConstMask				= B_STATEMENT_USAGE_COMMANDS_CONST_FLOAT | B_STATEMENT_USAGE_COMMANDS_CONST_FP16;
		const bool						commandsUsesFloatConstant			= (testCaseInfo.operation.statementUsageFlags & commandsFloatConstMask) != 0;;
		const FloatStatementUsageFlags	argumentsFloatConstMask				= B_STATEMENT_USAGE_ARGS_CONST_FLOAT | B_STATEMENT_USAGE_ARGS_CONST_FP16;
		const bool						argumentsUsesFloatConstant			= (specOpData.argumentsUsesFloatConstant & argumentsFloatConstMask) != 0;
		bool							hasFP16ConstsInCommandsOrArguments	= commandsUsesFloatConstant || argumentsUsesFloatConstant;

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

	map<string, string> specializations;
	specializations["vert_capabilities"]	= vertCapabilities;
	specializations["vert_extensions"]		= vertExtensions;
	specializations["vert_execution_mode"]	= vertExecutionMode;
	specializations["vert_annotations"]		= vertAnnotations;
	specializations["vert_types"]			= vertTypes;
	specializations["vert_constants"]		= vertConstants;
	specializations["vert_io_definitions"]	= vertIODefinitions;
	specializations["vert_arguments"]		= vertArguments;
	specializations["vert_variables"]		= vertVariables;
	specializations["vert_functions"]		= vertFunctions;
	specializations["vert_commands"]		= vertCommands;
	specializations["vert_process_result"]	= vertProcessResult;
	specializations["frag_capabilities"]	= fragCapabilities;
	specializations["frag_extensions"]		= fragExtensions;
	specializations["frag_execution_mode"]	= fragExecutionMode;
	specializations["frag_annotations"]		= fragAnnotations;
	specializations["frag_types"]			= fragTypes;
	specializations["frag_constants"]		= fragConstants;
	specializations["frag_functions"]		= fragFunctions;
	specializations["frag_io_definitions"]	= fragIODefinitions;
	specializations["frag_arguments"]		= fragArguments;
	specializations["frag_variables"]		= fragVariables;
	specializations["frag_commands"]		= fragCommands;
	specializations["frag_process_result"]	= fragProcessResult;

	// colors are not used by the test - input is passed via uniform buffer
	RGBA defaultColors[4] = { RGBA::white(), RGBA::red(), RGBA::green(), RGBA::blue() };

	// construct input and output buffers of proper types
	TypeValuesSP inTypeValues	= m_typeData.at(inFloatType).values;
	TypeValuesSP outTypeValues	= m_typeData.at(outFloatType).values;
	BufferSp inBufferSp			= inTypeValues->constructInputBuffer(testCase.input);
	BufferSp outBufferSp		= outTypeValues->constructOutputBuffer(testCase.expectedOutput);

	vkt::SpirVAssembly::GraphicsResources resources;
	resources.inputs.push_back( Resource(inBufferSp, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	resources.outputs.push_back(Resource(outBufferSp, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	resources.verifyIO = checkFloatsLUT[outFloatType];

	StageToSpecConstantMap	noSpecConstants;
	PushConstants			noPushConstants;
	GraphicsInterfaces		noInterfaces;

	VulkanFeatures vulkanFeatures;
	setupVulkanFeatures(inFloatTypeForCaps,		// usualy same as inFloatType - different only for UnpackHalf2x16
						outFloatType,
						testCase.behaviorFlags,
						float64FeatureRequired,
						vulkanFeatures);
	vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = true;

	vector<string> extensions;
	extensions.push_back("VK_KHR_shader_float_controls");
	if (needsShaderFloat16)
	{
		extensions.push_back("VK_KHR_shader_float16_int8");
		vulkanFeatures.extFloat16Int8 = EXTFLOAT16INT8FEATURES_FLOAT16;
	}
	if (float16FeatureRequired)
	{
		extensions.push_back("VK_KHR_16bit_storage");
		vulkanFeatures.ext16BitStorage = EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK;
	}

	InstanceContext ctx(defaultColors,
						defaultColors,
						specializations,
						noSpecConstants,
						noPushConstants,
						resources,
						noInterfaces,
						extensions,
						vulkanFeatures,
						testedStage);

	ctx.moduleMap["vert"].push_back(std::make_pair("main", VK_SHADER_STAGE_VERTEX_BIT));
	ctx.moduleMap["frag"].push_back(std::make_pair("main", VK_SHADER_STAGE_FRAGMENT_BIT));

	ctx.requiredStages			= static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	ctx.failResult				= QP_TEST_RESULT_FAIL;
	ctx.failMessageTemplate		= "Output doesn't match with expected";

	return ctx;
}

} // anonymous

tcu::TestCaseGroup* createFloatControlsTestGroup (TestContext& testCtx, TestGroupBuilderBase* groupBuilder)
{
	de::MovePtr<TestCaseGroup>	group(new TestCaseGroup(testCtx, "float_controls", "Tests for VK_KHR_shader_float_controls extension"));

	struct TestGroup
	{
		FloatType		floatType;
		const char*		groupName;
	};
	TestGroup testGroups[] =
	{
		{ FP16, "fp16" },
		{ FP32, "fp32" },
		{ FP64, "fp64" },
	};

	for (int i = 0 ; i < DE_LENGTH_OF_ARRAY(testGroups) ; ++i)
	{
		const TestGroup& testGroup = testGroups[i];
		TestCaseGroup* typeGroup = new TestCaseGroup(testCtx, testGroup.groupName, "");
		group->addChild(typeGroup);

		groupBuilder->createOperationTests(typeGroup, "input_args", testGroup.floatType, true);
		groupBuilder->createOperationTests(typeGroup, "generated_args", testGroup.floatType, false);
	}

	groupBuilder->createSettingsTests(group.get());

	return group.release();
}

tcu::TestCaseGroup* createFloatControlsComputeGroup (TestContext& testCtx)
{
	ComputeTestGroupBuilder computeTestGroupBuilder;
	computeTestGroupBuilder.init();

	return createFloatControlsTestGroup(testCtx, &computeTestGroupBuilder);
}

tcu::TestCaseGroup* createFloatControlsGraphicsGroup (TestContext& testCtx)
{
	GraphicsTestGroupBuilder graphicsTestGroupBuilder;
	graphicsTestGroupBuilder.init();

	return createFloatControlsTestGroup(testCtx, &graphicsTestGroupBuilder);
}

} // SpirVAssembly
} // vkt
