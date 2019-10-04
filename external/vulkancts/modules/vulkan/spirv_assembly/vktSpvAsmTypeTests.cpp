/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \brief SPIR-V Assembly Tests for Integer Types
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmTypeTests.hpp"

#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"

#include "deStringUtil.hpp"

#include "vktSpvAsmGraphicsShaderTestUtil.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "spirv/unified1/spirv.h"
#include "spirv/unified1/GLSL.std.450.h"

#include <cmath>

#define TEST_DATASET_SIZE 10

#define UNDEFINED_SPIRV_TEST_TYPE "testtype"

namespace de
{
	// Specialize template to have output as integers instead of chars
	template<>
	inline std::string toString<deInt8> (const deInt8& value)
	{
		std::ostringstream s;
		s << static_cast<deInt32>(value);
		return s.str();
	}

	template<>
	inline std::string toString<deUint8> (const deUint8& value)
	{
		std::ostringstream s;
		s << static_cast<deUint32>(value);
		return s.str();
	}
}

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using tcu::RGBA;
using std::map;
using std::string;
using std::vector;
using tcu::StringTemplate;

void createComputeTest(ComputeShaderSpec& computeResources, const tcu::StringTemplate& shaderTemplate, const map<string, string>& fragments, tcu::TestCaseGroup& group, const std::string& namePrefix)
{
	const string testName = namePrefix + "_comp";

	computeResources.assembly		= shaderTemplate.specialize(fragments);
	computeResources.numWorkGroups	= tcu::IVec3(1, 1, 1);

	group.addChild(new SpvAsmComputeShaderCase(group.getTestContext(), testName.c_str(), testName.c_str(), computeResources));
}

// The compute shader switch tests output a single 32-bit integer.
bool verifyComputeSwitchResult (const vector<Resource>&		,
								const vector<AllocationSp>&	outputAllocations,
								const vector<Resource>&		expectedOutputs,
								tcu::TestLog&				log)
{
	DE_ASSERT(outputAllocations.size()	== 1);
	DE_ASSERT(expectedOutputs.size()	== 1);

	vector<deUint8> expectedBytes;
	expectedOutputs[0].getBytes(expectedBytes);
	DE_ASSERT(expectedBytes.size() == sizeof(deInt32));

	const deInt32* obtained = reinterpret_cast<const deInt32*>(outputAllocations[0]->getHostPtr());
	const deInt32* expected = reinterpret_cast<const deInt32*>(expectedBytes.data());

	if (*obtained != *expected)
	{
		log << tcu::TestLog::Message
			<< "Error: found unexpected result for compute switch: expected " << *expected << ", obtained " << *obtained
			<< tcu::TestLog::EndMessage;
		return false;
	}

	return true;
}

enum InputRange
{
	RANGE_FULL = 0,
	RANGE_BIT_WIDTH,
	RANGE_BIT_WIDTH_SUM,

	RANGE_LAST
};

enum InputWidth
{
	WIDTH_DEFAULT = 0,
	WIDTH_8,
	WIDTH_16,
	WIDTH_32,
	WIDTH_64,
	WIDTH_8_8,
	WIDTH_8_16,
	WIDTH_8_32,
	WIDTH_8_64,
	WIDTH_16_8,
	WIDTH_16_16,
	WIDTH_16_32,
	WIDTH_16_64,
	WIDTH_32_8,
	WIDTH_32_16,
	WIDTH_32_32,
	WIDTH_32_64,
	WIDTH_64_8,
	WIDTH_64_16,
	WIDTH_64_32,
	WIDTH_64_64,

	WIDTH_LAST
};

enum InputType
{
	TYPE_I8 = 0,
	TYPE_U8,
	TYPE_I16,
	TYPE_U16,
	TYPE_I32,
	TYPE_U32,
	TYPE_I64,
	TYPE_U64,

	TYPE_LAST
};

deUint32 getConstituentIndex (deUint32 ndx, deUint32 vectorSize)
{
	DE_ASSERT(vectorSize != 0u);
	return (ndx / vectorSize) / (1u + (ndx % vectorSize));
}

bool isScalarInput (deUint32 spirvOperation, deUint32 numInput)
{
	switch (spirvOperation)
	{
		case SpvOpBitFieldInsert:
			return (numInput > 1);
		case SpvOpBitFieldSExtract:
			return (numInput > 0);
		case SpvOpBitFieldUExtract:
			return (numInput > 0);
		default:
			return false;
	}
}

bool isBooleanResultTest (deUint32 spirvOperation)
{
	switch (spirvOperation)
	{
		case SpvOpIEqual:
			return true;
		case SpvOpINotEqual:
			return true;
		case SpvOpUGreaterThan:
			return true;
		case SpvOpSGreaterThan:
			return true;
		case SpvOpUGreaterThanEqual:
			return true;
		case SpvOpSGreaterThanEqual:
			return true;
		case SpvOpULessThan:
			return true;
		case SpvOpSLessThan:
			return true;
		case SpvOpULessThanEqual:
			return true;
		case SpvOpSLessThanEqual:
			return true;
		default:
			return false;
	}
}

bool isConstantOrVariableTest (deUint32 spirvOperation)
{
	switch (spirvOperation)
	{
		case SpvOpConstantNull:
			return true;
		case SpvOpConstant:
			return true;
		case SpvOpConstantComposite:
			return true;
		case SpvOpVariable:
			return true;
		case SpvOpSpecConstant:
			return true;
		case SpvOpSpecConstantComposite:
			return true;
		default:
			return false;
	}
}

const char* getSpvOperationStr (deUint32 spirvOperation)
{
	switch (spirvOperation)
	{
		case SpvOpSNegate:
			return "OpSNegate";
		case SpvOpIAdd:
			return "OpIAdd";
		case SpvOpISub:
			return "OpISub";
		case SpvOpIMul:
			return "OpIMul";
		case SpvOpSDiv:
			return "OpSDiv";
		case SpvOpUDiv:
			return "OpUDiv";
		case SpvOpSRem:
			return "OpSRem";
		case SpvOpSMod:
			return "OpSMod";
		case SpvOpUMod:
			return "OpUMod";
		case SpvOpShiftRightLogical:
			return "OpShiftRightLogical";
		case SpvOpShiftRightArithmetic:
			return "OpShiftRightArithmetic";
		case SpvOpShiftLeftLogical:
			return "OpShiftLeftLogical";
		case SpvOpBitwiseOr:
			return "OpBitwiseOr";
		case SpvOpBitwiseXor:
			return "OpBitwiseXor";
		case SpvOpBitwiseAnd:
			return "OpBitwiseAnd";
		case SpvOpNot:
			return "OpNot";
		case SpvOpIEqual:
			return "OpIEqual";
		case SpvOpINotEqual:
			return "OpINotEqual";
		case SpvOpUGreaterThan:
			return "OpUGreaterThan";
		case SpvOpSGreaterThan:
			return "OpSGreaterThan";
		case SpvOpUGreaterThanEqual:
			return "OpUGreaterThanEqual";
		case SpvOpSGreaterThanEqual:
			return "OpSGreaterThanEqual";
		case SpvOpULessThan:
			return "OpULessThan";
		case SpvOpSLessThan:
			return "OpSLessThan";
		case SpvOpULessThanEqual:
			return "OpULessThanEqual";
		case SpvOpSLessThanEqual:
			return "OpSLessThanEqual";
		case SpvOpBitFieldInsert:
			return "OpBitFieldInsert";
		case SpvOpBitFieldSExtract:
			return "OpBitFieldSExtract";
		case SpvOpBitFieldUExtract:
			return "OpBitFieldUExtract";
		case SpvOpBitReverse:
			return "OpBitReverse";
		case SpvOpBitCount:
			return "OpBitCount";
		case SpvOpConstant:
			return "OpConstant";
		case SpvOpConstantComposite:
			return "OpConstantComposite";
		case SpvOpConstantNull:
			return "OpConstantNull";
		case SpvOpVariable:
			return "OpVariable";
		case SpvOpSpecConstant:
			return "OpSpecConstant";
		case SpvOpSpecConstantComposite:
			return "OpSpecConstantComposite";
		default:
			return "";
	}
}

const char* getGLSLstd450OperationStr (deUint32 spirvOperation)
{
	switch (spirvOperation)
	{
		case GLSLstd450SAbs:
			return "SAbs";
		case GLSLstd450SSign:
			return "SSign";
		case GLSLstd450SMin:
			return "SMin";
		case GLSLstd450UMin:
			return "UMin";
		case GLSLstd450SMax:
			return "SMax";
		case GLSLstd450UMax:
			return "UMax";
		case GLSLstd450SClamp:
			return "SClamp";
		case GLSLstd450UClamp:
			return "UClamp";
		case GLSLstd450FindILsb:
			return "FindILsb";
		case GLSLstd450FindSMsb:
			return "FindSMsb";
		case GLSLstd450FindUMsb:
			return "FindUMsb";
		default:
			DE_FATAL("Not implemented");
			return "";
	}
}

string getBooleanResultType (deUint32 vectorSize)
{
	if (vectorSize > 1)
		return "v" + de::toString(vectorSize) + "bool";
	else
		return "bool";
}

deUint32 getInputWidth (InputWidth inputWidth, deUint32 ndx)
{
	switch (inputWidth)
	{
		case WIDTH_8:
			DE_ASSERT(ndx < 1);
			return 8;
		case WIDTH_16:
			DE_ASSERT(ndx < 1);
			return 16;
		case WIDTH_32:
			DE_ASSERT(ndx < 1);
			return 32;
		case WIDTH_64:
			DE_ASSERT(ndx < 1);
			return 64;
		case WIDTH_8_8:
			DE_ASSERT(ndx < 2);
			return 8;
		case WIDTH_8_16:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 8 : 16;
		case WIDTH_8_32:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 8 : 32;
		case WIDTH_8_64:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 8 : 64;
		case WIDTH_16_8:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 16 : 8;
		case WIDTH_16_16:
			DE_ASSERT(ndx < 2);
			return 16;
		case WIDTH_16_32:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 16 : 32;
		case WIDTH_16_64:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 16 : 64;
		case WIDTH_32_8:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 32 : 8;
		case WIDTH_32_16:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 32 : 16;
		case WIDTH_32_32:
			DE_ASSERT(ndx < 2);
			return 32;
		case WIDTH_32_64:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 32 : 64;
		case WIDTH_64_8:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 64 : 8;
		case WIDTH_64_16:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 64 : 16;
		case WIDTH_64_32:
			DE_ASSERT(ndx < 2);
			return (ndx == 0) ? 64 : 32;
		case WIDTH_64_64:
			DE_ASSERT(ndx < 2);
			return 64;
		default:
			DE_FATAL("Not implemented");
			return 0;
	}
}

bool has8BitInputWidth (InputWidth inputWidth)
{
	switch (inputWidth)
	{
		case WIDTH_8:		return true;
		case WIDTH_16:		return false;
		case WIDTH_32:		return false;
		case WIDTH_64:		return false;
		case WIDTH_8_8:		return true;
		case WIDTH_8_16:	return true;
		case WIDTH_8_32:	return true;
		case WIDTH_8_64:	return true;
		case WIDTH_16_8:	return true;
		case WIDTH_16_16:	return false;
		case WIDTH_16_32:	return false;
		case WIDTH_16_64:	return false;
		case WIDTH_32_8:	return true;
		case WIDTH_32_16:	return false;
		case WIDTH_32_32:	return false;
		case WIDTH_32_64:	return false;
		case WIDTH_64_8:	return true;
		case WIDTH_64_16:	return false;
		case WIDTH_64_32:	return false;
		case WIDTH_64_64:	return false;
		default:			return false;
	}
}

bool has16BitInputWidth (InputWidth inputWidth)
{
	switch (inputWidth)
	{
		case WIDTH_8:		return false;
		case WIDTH_16:		return true;
		case WIDTH_32:		return false;
		case WIDTH_64:		return false;
		case WIDTH_8_8:		return false;
		case WIDTH_8_16:	return true;
		case WIDTH_8_32:	return false;
		case WIDTH_8_64:	return false;
		case WIDTH_16_8:	return true;
		case WIDTH_16_16:	return true;
		case WIDTH_16_32:	return true;
		case WIDTH_16_64:	return true;
		case WIDTH_32_8:	return false;
		case WIDTH_32_16:	return true;
		case WIDTH_32_32:	return false;
		case WIDTH_32_64:	return false;
		case WIDTH_64_8:	return false;
		case WIDTH_64_16:	return true;
		case WIDTH_64_32:	return false;
		case WIDTH_64_64:	return false;
		default:			return false;
	}
}

bool has64BitInputWidth (InputWidth inputWidth)
{
	switch (inputWidth)
	{
		case WIDTH_8:		return false;
		case WIDTH_16:		return false;
		case WIDTH_32:		return false;
		case WIDTH_64:		return true;
		case WIDTH_8_8:		return false;
		case WIDTH_8_16:	return false;
		case WIDTH_8_32:	return false;
		case WIDTH_8_64:	return true;
		case WIDTH_16_8:	return false;
		case WIDTH_16_16:	return false;
		case WIDTH_16_32:	return false;
		case WIDTH_16_64:	return true;
		case WIDTH_32_8:	return false;
		case WIDTH_32_16:	return false;
		case WIDTH_32_32:	return false;
		case WIDTH_32_64:	return true;
		case WIDTH_64_8:	return true;
		case WIDTH_64_16:	return true;
		case WIDTH_64_32:	return true;
		case WIDTH_64_64:	return true;
		default:			return false;
	}
}

InputType getInputType (deUint32 inputWidth, bool isSigned)
{
	switch (inputWidth)
	{
		case 8:
			return (isSigned) ? TYPE_I8  : TYPE_U8;
		case 16:
			return (isSigned) ? TYPE_I16 : TYPE_U16;
		case 32:
			return (isSigned) ? TYPE_I32 : TYPE_U32;
		case 64:
			return (isSigned) ? TYPE_I64 : TYPE_U64;
		default:
			DE_FATAL("Not possible");
			return TYPE_LAST;
	}
}

string getOtherSizeTypes (InputType inputType, deUint32 vectorSize, InputWidth inputWidth)
{
	const deUint32 inputWidthValues[] =
	{
		8, 16, 32, 64
	};

	for (deUint32 widthNdx = 0; widthNdx < DE_LENGTH_OF_ARRAY(inputWidthValues); widthNdx++)
	{
		const deUint32	typeWidth		= inputWidthValues[widthNdx];
		const InputType	typeUnsigned	= getInputType(typeWidth, false);
		const InputType	typeSigned		= getInputType(typeWidth, true);

		if ((inputType == typeUnsigned) || (inputType == typeSigned))
		{
			const bool		isSigned	= (inputType == typeSigned);
			const string	signPrefix	= (isSigned) ? "i" : "u";
			const string	signBit		= (isSigned) ? "1" : "0";

			string			str			= "";

			if (has8BitInputWidth(inputWidth) && typeWidth != 8)
			{
				// 8-bit scalar type
				str += "%" + signPrefix + "8 = OpTypeInt 8 " + signBit + "\n";

				// 8-bit vector type
				if (vectorSize > 1)
					str += "%v" + de::toString(vectorSize) + signPrefix + "8 = OpTypeVector %" + signPrefix + "8 " + de::toString(vectorSize) + "\n";
			}

			if (has16BitInputWidth(inputWidth) && typeWidth != 16)
			{
				// 16-bit scalar type
				str += "%" + signPrefix + "16 = OpTypeInt 16 " + signBit + "\n";

				// 16-bit vector type
				if (vectorSize > 1)
					str += "%v" + de::toString(vectorSize) + signPrefix + "16 = OpTypeVector %" + signPrefix + "16 " + de::toString(vectorSize) + "\n";
			}

			if (has64BitInputWidth(inputWidth) && typeWidth != 64)
			{
				// 64-bit scalar type
				str += "%" + signPrefix + "64 = OpTypeInt 64 " + signBit + "\n";

				// 64-bit vector type
				if (vectorSize > 1)
					str += "%v" + de::toString(vectorSize) + signPrefix + "64 = OpTypeVector %" + signPrefix + "64 " + de::toString(vectorSize) + "\n";
			}

			return str;
		}
	}

	DE_FATAL("Not possible");
	return "";
}

string getSpirvCapabilityStr (const char* spirvCapability, InputWidth inputWidth)
{
	string str = "";

	if (spirvCapability)
	{
		if (has8BitInputWidth(inputWidth) || deStringEqual("Int8", spirvCapability))
			str += "OpCapability Int8\n";

		if (has16BitInputWidth(inputWidth) || deStringEqual("Int16", spirvCapability))
			str += "OpCapability Int16\n";

		if (has64BitInputWidth(inputWidth) || deStringEqual("Int64", spirvCapability))
			str += "OpCapability Int64\n";

		if (deStringEqual("Int8", spirvCapability))
			str += "OpCapability UniformAndStorageBuffer8BitAccess\n";

		if (deStringEqual("Int16", spirvCapability))
			str += "OpCapability UniformAndStorageBuffer16BitAccess\n";
	}
	else
	{
		if (has8BitInputWidth(inputWidth))
			str += "OpCapability Int8\n";

		if (has16BitInputWidth(inputWidth))
			str += "OpCapability Int16\n";

		if (has64BitInputWidth(inputWidth))
			str += "OpCapability Int64\n";
	}


	return str;
}

string getBinaryFullOperationWithInputWidthStr (string resultName, string spirvOperation, InputType inputType, string spirvTestType, deUint32 vectorSize, InputWidth inputWidth)
{
	const deUint32 inputWidthValues[] =
	{
		8, 16, 32, 64
	};

	for (deUint32 widthNdx = 0; widthNdx < DE_LENGTH_OF_ARRAY(inputWidthValues); widthNdx++)
	{
		const deUint32	typeWidth		= inputWidthValues[widthNdx];
		const InputType	typeUnsigned	= getInputType(typeWidth, false);
		const InputType	typeSigned		= getInputType(typeWidth, true);

		if ((inputType == typeUnsigned) || (inputType == typeSigned))
		{
			const bool		isSigned		= (inputType == typeSigned);
			const string	signPrefix		= (isSigned) ? "i" : "u";
			const string	typePrefix		= (vectorSize == 1) ? "%" : "%v" + de::toString(vectorSize);
			const deUint32	input1Width		= getInputWidth(inputWidth, 0);

			const string	inputTypeStr	= (input1Width == typeWidth) ? "%testtype"
											: typePrefix + signPrefix + de::toString(input1Width);

			string str = "";

			// Create intermediate value with different width
			if (input1Width != typeWidth)
				str += "%input1_val_" + de::toString(input1Width) + " = OpSConvert " + inputTypeStr + " %input1_val\n";

			// Input with potentially different width
			const string input1Str = "%input1_val" + ((input1Width != typeWidth) ? "_" + de::toString(input1Width) : "");

			str += resultName + " = " + spirvOperation + " %" + spirvTestType + " %input0_val " + input1Str + "\n";

			return str;
		}
	}

	DE_FATAL("Not possible");
	return "";
}

string getFullOperationWithDifferentInputWidthStr (string resultName, string spirvOperation, InputType inputType, string spirvTestType, InputWidth inputWidth, bool isQuaternary)
{
	const bool		isSigned	= (inputType == TYPE_I32);

	const deUint32	offsetWidth	= getInputWidth(inputWidth, 0);
	const deUint32	countWidth	= getInputWidth(inputWidth, 1);

	const string	offsetType	= ((isSigned) ? "i" : "u") + de::toString(offsetWidth);
	const string	countType	= ((isSigned) ? "i" : "u") + de::toString(countWidth);

	const string	offsetNdx	= (isQuaternary) ? "2" : "1";
	const string	countNdx	= (isQuaternary) ? "3" : "2";

	string str = "";

	// Create intermediate values with different width
	if (offsetWidth != 32)
		str += "%input" + offsetNdx + "_val_" + de::toString(offsetWidth) + " = OpSConvert %" + offsetType + " %input" + offsetNdx + "_val\n";
	if (countWidth != 32)
		str += "%input" + countNdx + "_val_" + de::toString(countWidth) + " = OpSConvert %" + countType + " %input" + countNdx + "_val\n";

	// Inputs with potentially different width
	const string offsetStr	= "%input" + offsetNdx + "_val" + ((offsetWidth != 32) ? "_" + de::toString(offsetWidth) : "");
	const string countStr	= "%input" + countNdx + "_val" + ((countWidth != 32) ? "_" + de::toString(countWidth) : "");

	if (isQuaternary)
		str += resultName + " = " + spirvOperation + " %" + spirvTestType + " %input0_val %input1_val " + offsetStr + " " + countStr +"\n";
	else
		str += resultName + " = " + spirvOperation + " %" + spirvTestType + " %input0_val " + offsetStr + " " + countStr +"\n";

	return str;
}

static inline void requiredFeaturesFromStrings(const std::vector<std::string> &features, VulkanFeatures &requestedFeatures)
{
	for (deUint32 featureNdx = 0; featureNdx < features.size(); ++featureNdx)
	{
		const std::string& feature = features[featureNdx];

		if (feature == "shaderInt16")
			requestedFeatures.coreFeatures.shaderInt16 = VK_TRUE;
		else if (feature == "shaderInt64")
			requestedFeatures.coreFeatures.shaderInt64 = VK_TRUE;
		else
			DE_ASSERT(0);  // Not implemented. Don't add to here. Just use VulkanFeatures
	}
}

template <class T>
class SpvAsmTypeTests : public tcu::TestCaseGroup
{
public:
	typedef T		(*OpUnaryFuncType)			(T);
	typedef T		(*OpBinaryFuncType)			(T, T);
	typedef T		(*OpTernaryFuncType)		(T, T, T);
	typedef T		(*OpQuaternaryFuncType)		(T, T, T, T);
	typedef bool	(*UnaryFilterFuncType)		(T);
	typedef bool	(*BinaryFilterFuncType)		(T, T);
	typedef bool	(*TernaryFilterFuncType)	(T, T, T);
	typedef bool	(*QuaternaryFilterFuncType)	(T, T, T, T);
					SpvAsmTypeTests				(tcu::TestContext&			testCtx,
												 const char*				name,
												 const char*				description,
												 const char*				deviceFeature,
												 const char*				spirvCapability,
												 const char*				spirvType,
												 InputType					inputType,
												 deUint32					typeSize,
												 deUint32					vectorSize);
					~SpvAsmTypeTests			(void);
	void			createTests					(const char*				testName,
												 deUint32					spirvOperation,
												 OpUnaryFuncType			op,
												 UnaryFilterFuncType		filter,
												 InputRange					inputRange,
												 InputWidth					inputWidth,
												 const char*				spirvExtension,
												 const bool					returnHighPart	= false);
	void			createTests					(const char*				testName,
												 deUint32					spirvOperation,
												 OpBinaryFuncType			op,
												 BinaryFilterFuncType		filter,
												 InputRange					inputRange,
												 InputWidth					inputWidth,
												 const char*				spirvExtension,
												 const bool					returnHighPart	= false);
	void			createTests					(const char*				testName,
												 deUint32					spirvOperation,
												 OpTernaryFuncType			op,
												 TernaryFilterFuncType		filter,
												 InputRange					inputRange,
												 InputWidth					inputWidth,
												 const char*				spirvExtension,
												 const bool					returnHighPart	= false);
	void			createTests					(const char*				testName,
												 deUint32					spirvOperation,
												 OpQuaternaryFuncType		op,
												 QuaternaryFilterFuncType	filter,
												 InputRange					inputRange,
												 InputWidth					inputWidth,
												 const char*				spirvExtension,
												 const bool					returnHighPart	= false);
	void			createSwitchTests			(void);
	void			getConstantDataset			(vector<T>					inputDataset,
												 vector<T>&					outputDataset,
												 deUint32					spirvOperation);
	virtual void	getDataset					(vector<T>& input,			deUint32 numElements) = 0;
	virtual void	pushResource				(vector<Resource>&			resource,
												 const vector<T>&			data) = 0;

	static bool		filterNone					(T a);
	static bool		filterNone					(T a, T b);
	static bool		filterNone					(T a, T b, T c);
	static bool		filterNone					(T a, T b, T c, T d);
	static bool		filterZero					(T a, T b);
	static bool		filterNegativesAndZero		(T a, T b);
	static bool		filterMinGtMax				(T, T a, T b);

	static T		zero						(T);
	static T		zero						(T, T);
	static T		zero						(T, T, T);
	static T		zero						(T, T, T, T);

	static string	replicate					(const std::string&			replicant,
												 const deUint32				count);

protected:
	de::Random	m_rnd;
	T			m_cases[3];

private:
	std::string	createInputDecoration			(deUint32						numInput);
	std::string	createInputPreMain				(deUint32						numInput,
												 deUint32						spirvOpertaion);
	std::string	createConstantDeclaration		(vector<T>&						dataset,
												 deUint32						spirvOperation);
	std::string	createInputTestfun				(deUint32						numInput,
												 deUint32						spirvOpertaion);
	deUint32	combine							(GraphicsResources&				resources,
												 ComputeShaderSpec&				computeResources,
												 vector<T>&						data,
												 OpUnaryFuncType				operation,
												 UnaryFilterFuncType			filter,
												 InputRange						inputRange);
	deUint32	combine							(GraphicsResources&				resources,
												 ComputeShaderSpec&				computeResources,
												 vector<T>&						data,
												 OpBinaryFuncType				operation,
												 BinaryFilterFuncType			filter,
												 InputRange						inputRange);
	deUint32	combine							(GraphicsResources&				resources,
												 ComputeShaderSpec&				computeResources,
												 vector<T>&						data,
												 OpTernaryFuncType				operation,
												 TernaryFilterFuncType			filter,
												 InputRange						inputRange);
	deUint32	combine							(GraphicsResources&				resources,
												 ComputeShaderSpec&				computeResources,
												 vector<T>&						data,
												 OpQuaternaryFuncType			operation,
												 QuaternaryFilterFuncType		filter,
												 InputRange						inputRange);
	deUint32	fillResources					(GraphicsResources&				resources,
												 ComputeShaderSpec&				computeResources,
												 const vector<T>&				data);
	void		createStageTests				(const char*					testName,
												 GraphicsResources&				resources,
												 ComputeShaderSpec&				computeResources,
												 deUint32						numElements,
												 vector<string>&				decorations,
												 vector<string>&				pre_mains,
												 vector<string>&				testfuns,
												 string&						operation,
												 InputWidth						inputWidth,
												 const char*					funVariables,
												 const char*					spirvExtension	= DE_NULL);
	void		finalizeFullOperation			(string&						fullOperation,
												 const string&					resultName,
												 const bool						returnHighPart,
												 const bool						isBooleanResult);

	static bool	verifyResult					(const vector<Resource>&		inputs,
												 const vector<AllocationSp>&	outputAllocations,
												 const vector<Resource>&		expectedOutputs,
												 deUint32						skip,
												 tcu::TestLog&					log);
	static bool	verifyDefaultResult				(const vector<Resource>&		inputs,
												 const vector<AllocationSp>&	outputAllocations,
												 const vector<Resource>&		expectedOutputs,
												 tcu::TestLog&					log);
	static bool	verifyVec3Result				(const vector<Resource>&		inputs,
												 const vector<AllocationSp>&	outputAllocations,
												 const vector<Resource>&		expectedOutputs,
												 tcu::TestLog&					log);
	const char* const	m_deviceFeature;
	const char* const	m_spirvCapability;
	const char* const	m_spirvType;
	InputType			m_inputType;
	deUint32			m_typeSize;
	deUint32			m_vectorSize;
	std::string			m_spirvTestType;
};

template <class T>
SpvAsmTypeTests<T>::SpvAsmTypeTests	(tcu::TestContext&	testCtx,
									 const char*		name,
									 const char*		description,
									 const char*		deviceFeature,
									 const char*		spirvCapability,
									 const char*		spirvType,
									 InputType			inputType,
									 deUint32			typeSize,
									 deUint32			vectorSize)
	: tcu::TestCaseGroup	(testCtx, name, description)
	, m_rnd					(deStringHash(name))
	, m_deviceFeature		(deviceFeature)
	, m_spirvCapability		(spirvCapability)
	, m_spirvType			(spirvType)
	, m_inputType			(inputType)
	, m_typeSize			(typeSize)
	, m_vectorSize			(vectorSize)
{
	std::string scalarType;

	DE_ASSERT(vectorSize >= 1 && vectorSize <= 4);

	if (m_inputType == TYPE_I32)
		scalarType = "i32";
	else if (m_inputType == TYPE_U32)
		scalarType = "u32";
	else
		scalarType = "";

	if (scalarType.empty())
	{
		m_spirvTestType = UNDEFINED_SPIRV_TEST_TYPE;
	}
	else
	{
		if (m_vectorSize > 1)
			m_spirvTestType = "v" + de::toString(m_vectorSize) + scalarType;
		else
			m_spirvTestType = scalarType;
	}
}

template <class T>
SpvAsmTypeTests<T>::~SpvAsmTypeTests	(void)
{
}

template <class T>
std::string SpvAsmTypeTests<T>::createInputDecoration (deUint32 numInput)
{
	const StringTemplate	decoration	("OpDecorate %input${n_input} DescriptorSet 0\n"
										 "OpDecorate %input${n_input} Binding ${n_input}\n");
	map<string, string>		specs;

	specs["n_input"] = de::toString(numInput);

	return decoration.specialize(specs);
}

template <class T>
std::string SpvAsmTypeTests<T>::createInputPreMain (deUint32 numInput, deUint32 spirvOpertaion)
{
	const bool				scalarInput = (m_vectorSize != 1) && isScalarInput(spirvOpertaion, numInput);
	const string			bufferType	= (scalarInput) ? "%scalarbufptr" : "%bufptr";

	return "%input" + de::toString(numInput) + " = OpVariable " + bufferType + " Uniform\n";
}

template <class T>
std::string SpvAsmTypeTests<T>::createInputTestfun (deUint32 numInput, deUint32 spirvOpertaion)
{
	const bool				scalarInput	= (m_vectorSize != 1) && isScalarInput(spirvOpertaion, numInput);
	const string			pointerType	= (scalarInput) ? "%up_scalartype" : "%up_testtype";
	const string			valueType	= (scalarInput) ? "%u32" : "%${testtype}";

	const StringTemplate	testfun		("%input${n_input}_loc = OpAccessChain " + pointerType + " %input${n_input} "
										 "%c_i32_0 %counter_val\n"
										 "%input${n_input}_val = OpLoad " + valueType + " %input${n_input}_loc\n");
	map<string, string>		specs;

	specs["n_input"] = de::toString(numInput);
	specs["testtype"] = m_spirvTestType;

	return testfun.specialize(specs);
}

template <class T>
deUint32 SpvAsmTypeTests<T>::combine (GraphicsResources&	resources,
									  ComputeShaderSpec&	computeResources,
									  vector<T>&			data,
									  OpUnaryFuncType		operation,
									  UnaryFilterFuncType	filter,
									  InputRange			inputRange)
{
	DE_UNREF(inputRange);
	const deUint32	datasize		= static_cast<deUint32>(data.size());
	const deUint32	sizeWithPadding = (m_vectorSize == 3) ? 4 : m_vectorSize;
	const deUint32	totalPadding	= (m_vectorSize == 3) ? (datasize / m_vectorSize) : 0;
	const deUint32	total			= datasize + totalPadding;
	deUint32		padCount		= m_vectorSize;
	deUint32		outputsSize;
	vector<T>		inputs;
	vector<T>		outputs;

	inputs.reserve(total);
	outputs.reserve(total);

	/* According to spec, a three-component vector, with components of size N,
	   has a base alignment of 4 N */
	for (deUint32 elemNdx = 0; elemNdx < datasize; ++elemNdx)
	{
		if (filter(data[elemNdx]))
		{
			inputs.push_back(data[elemNdx]);
			outputs.push_back(operation(data[elemNdx]));
			if (m_vectorSize == 3)
			{
				padCount--;
				if (padCount == 0)
				{
					inputs.push_back(0);
					outputs.push_back(0);
					padCount = m_vectorSize;
				}
			}
		}
	}

	outputsSize = static_cast<deUint32>(outputs.size());

	/* Ensure we have pushed a multiple of vector size, including padding if
	   required */
	while (outputsSize % sizeWithPadding != 0)
	{
		inputs.pop_back();
		outputs.pop_back();
		outputsSize--;
	}

	pushResource(resources.inputs, inputs);
	pushResource(resources.outputs, outputs);

	pushResource(computeResources.inputs, inputs);
	pushResource(computeResources.outputs, outputs);

	return outputsSize / sizeWithPadding;
}

template <class T>
deUint32 SpvAsmTypeTests<T>::combine (GraphicsResources&	resources,
									  ComputeShaderSpec&	computeResources,
									  vector<T>&			data,
									  OpBinaryFuncType		operation,
									  BinaryFilterFuncType	filter,
									  InputRange			inputRange)
{
	const deUint32	datasize		= static_cast<deUint32>(data.size());
	const deUint32	sizeWithPadding = (m_vectorSize == 3) ? 4 : m_vectorSize;
	const deUint32	totalData		= datasize * datasize;
	const deUint32	totalPadding	= (m_vectorSize == 3) ? (totalData / m_vectorSize) : 0;
	const deUint32	total			= totalData + totalPadding;
	deUint32		padCount		= m_vectorSize;
	deUint32		outputsSize;
	vector<T>		inputs0;
	vector<T>		inputs1;
	vector<T>		outputs;

	inputs0.reserve(total);
	inputs1.reserve(total);
	outputs.reserve(total);

	/* According to spec, a three-component vector, with components of size N,
	   has a base alignment of 4 N */
	for (deUint32 elemNdx1 = 0; elemNdx1 < datasize; ++elemNdx1)
	for (deUint32 elemNdx2 = 0; elemNdx2 < datasize; ++elemNdx2)
	{
		if (filter(data[elemNdx1], data[elemNdx2]))
		{
			switch (inputRange)
			{
				case RANGE_FULL:
				{
					inputs0.push_back(data[elemNdx1]);
					inputs1.push_back(data[elemNdx2]);
					outputs.push_back(operation(data[elemNdx1], data[elemNdx2]));
					break;
				}
				case RANGE_BIT_WIDTH:
				{
					// Make sure shift count doesn't exceed the bit width
					const T shift = data[elemNdx2] & static_cast<T>(m_typeSize - 1u);
					inputs0.push_back(data[elemNdx1]);
					inputs1.push_back(shift);
					outputs.push_back(operation(data[elemNdx1], shift));
					break;
				}
				default:
					DE_FATAL("Not implemented");
			}

			if (m_vectorSize == 3)
			{
				padCount--;
				if (padCount == 0)
				{
					inputs0.push_back(0);
					inputs1.push_back(0);
					outputs.push_back(0);
					padCount = m_vectorSize;
				}
			}
		}
	}

	outputsSize = static_cast<deUint32>(outputs.size());

	/* Ensure we have pushed a multiple of vector size, including padding if
	   required */
	while (outputsSize % sizeWithPadding != 0)
	{
		inputs0.pop_back();
		inputs1.pop_back();
		outputs.pop_back();
		outputsSize--;
	}

	pushResource(resources.inputs, inputs0);
	pushResource(resources.inputs, inputs1);
	pushResource(resources.outputs, outputs);

	pushResource(computeResources.inputs, inputs0);
	pushResource(computeResources.inputs, inputs1);
	pushResource(computeResources.outputs, outputs);

	return outputsSize / sizeWithPadding;
}

template <class T>
deUint32 SpvAsmTypeTests<T>::combine (GraphicsResources&	resources,
									  ComputeShaderSpec&	computeResources,
									  vector<T>&			data,
									  OpTernaryFuncType		operation,
									  TernaryFilterFuncType	filter,
									  InputRange			inputRange)
{
	const deUint32	datasize		= static_cast<deUint32>(data.size());
	const deUint32	sizeWithPadding = (m_vectorSize == 3) ? 4 : m_vectorSize;
	const deUint32	totalData		= datasize * datasize * datasize;
	const deUint32	totalPadding	= (m_vectorSize == 3) ? (totalData / m_vectorSize) : 0;
	const deUint32	total			= totalData + totalPadding;
	deUint32		padCount		= m_vectorSize;
	deUint32		outputsSize;
	vector<T>		inputs0;
	vector<T>		inputs1;
	vector<T>		inputs2;
	vector<T>		outputs;

	inputs0.reserve(total);
	inputs1.reserve(total);
	inputs2.reserve(total);
	outputs.reserve(total);

	// Reduce the amount of input data in tests without filtering
	deUint32		datasize2		= (inputRange == RANGE_BIT_WIDTH_SUM) ? 4u * m_vectorSize : datasize;
	T				bitOffset		= static_cast<T>(0);
	T				bitCount		= static_cast<T>(0);

	/* According to spec, a three-component vector, with components of size N,
	   has a base alignment of 4 N */
	for (deUint32 elemNdx1 = 0; elemNdx1 < datasize; ++elemNdx1)
	for (deUint32 elemNdx2 = 0; elemNdx2 < datasize2; ++elemNdx2)
	for (deUint32 elemNdx3 = 0; elemNdx3 < datasize2; ++elemNdx3)
	{
		if (filter(data[elemNdx1], data[elemNdx2], data[elemNdx3]))
		{
			switch (inputRange)
			{
				case RANGE_FULL:
				{
					inputs0.push_back(data[elemNdx1]);
					inputs1.push_back(data[elemNdx2]);
					inputs2.push_back(data[elemNdx3]);
					outputs.push_back(operation(data[elemNdx1], data[elemNdx2], data[elemNdx3]));
					break;
				}
				case RANGE_BIT_WIDTH_SUM:
				{
					if (elemNdx3 % m_vectorSize == 0)
					{
						bitOffset	= static_cast<T>(m_rnd.getUint32() & (m_typeSize - 1u));
						bitCount	= static_cast<T>(m_rnd.getUint32() & (m_typeSize - 1u));
					}

					// Make sure the sum of offset and count doesn't exceed bit width
					if ((deUint32)(bitOffset + bitCount) > m_typeSize)
						bitCount = static_cast<T>(m_typeSize - bitOffset);

					inputs0.push_back(data[elemNdx1]);
					inputs1.push_back(bitOffset);
					inputs2.push_back(bitCount);
					outputs.push_back(operation(data[elemNdx1], bitOffset, bitCount));
					break;
				}
				default:
					DE_FATAL("Not implemented");
			}
			if (m_vectorSize == 3)
			{
				padCount--;
				if (padCount == 0)
				{
					inputs0.push_back(0);
					inputs1.push_back(0);
					inputs2.push_back(0);
					outputs.push_back(0);
					padCount = m_vectorSize;
				}
			}
		}
	}
	outputsSize = static_cast<deUint32>(outputs.size());

	/* Ensure we have pushed a multiple of vector size, including padding if
	   required */
	while (outputsSize % sizeWithPadding != 0)
	{
		inputs0.pop_back();
		inputs1.pop_back();
		inputs2.pop_back();
		outputs.pop_back();
		outputsSize--;
	}

	pushResource(resources.inputs, inputs0);
	pushResource(resources.inputs, inputs1);
	pushResource(resources.inputs, inputs2);
	pushResource(resources.outputs, outputs);

	pushResource(computeResources.inputs, inputs0);
	pushResource(computeResources.inputs, inputs1);
	pushResource(computeResources.inputs, inputs2);
	pushResource(computeResources.outputs, outputs);

	return outputsSize / sizeWithPadding;
}

template <class T>
deUint32 SpvAsmTypeTests<T>::combine (GraphicsResources&		resources,
									  ComputeShaderSpec&		computeResources,
									  vector<T>&				data,
									  OpQuaternaryFuncType		operation,
									  QuaternaryFilterFuncType	filter,
									  InputRange				inputRange)
{
	const deUint32	datasize		= static_cast<deUint32>(data.size());
	const deUint32	sizeWithPadding = (m_vectorSize == 3) ? 4 : m_vectorSize;
	const deUint32	totalData		= datasize * datasize;
	const deUint32	totalPadding	= (m_vectorSize == 3) ? (totalData / m_vectorSize) : 0;
	const deUint32	total			= totalData + totalPadding;
	deUint32		padCount		= m_vectorSize;
	deUint32		outputsSize;
	vector<T>		inputs0;
	vector<T>		inputs1;
	vector<T>		inputs2;
	vector<T>		inputs3;
	vector<T>		outputs;

	inputs0.reserve(total);
	inputs1.reserve(total);
	inputs2.reserve(total);
	inputs3.reserve(total);
	outputs.reserve(total);

	// Reduce the amount of input data in tests without filtering
	deUint32		datasize2		= (inputRange == RANGE_BIT_WIDTH_SUM) ? 2u * m_vectorSize : datasize;
	T				bitOffset		= static_cast<T>(0);
	T				bitCount		= static_cast<T>(0);

	/* According to spec, a three-component vector, with components of size N,
	   has a base alignment of 4 N */
	for (deUint32 elemNdx1 = 0; elemNdx1 < datasize; ++elemNdx1)
	for (deUint32 elemNdx2 = 0; elemNdx2 < datasize2; ++elemNdx2)
	for (deUint32 elemNdx3 = 0; elemNdx3 < datasize2; ++elemNdx3)
	for (deUint32 elemNdx4 = 0; elemNdx4 < datasize2; ++elemNdx4)
	{
		if (filter(data[elemNdx1], data[elemNdx2], data[elemNdx3], data[elemNdx4]))
		{
			switch (inputRange)
			{
				case RANGE_FULL:
				{
					inputs0.push_back(data[elemNdx1]);
					inputs1.push_back(data[elemNdx2]);
					inputs2.push_back(data[elemNdx3]);
					inputs3.push_back(data[elemNdx3]);
					outputs.push_back(operation(data[elemNdx1], data[elemNdx2], data[elemNdx3], data[elemNdx4]));
					break;
				}
				case RANGE_BIT_WIDTH_SUM:
				{
					if (elemNdx4 % m_vectorSize == 0)
					{
						bitOffset	= static_cast<T>(m_rnd.getUint32() & (m_typeSize - 1u));
						bitCount	= static_cast<T>(m_rnd.getUint32() & (m_typeSize - 1u));
					}

					// Make sure the sum of offset and count doesn't exceed bit width
					if ((deUint32)(bitOffset + bitCount) > m_typeSize)
						bitCount -= bitOffset + bitCount - static_cast<T>(m_typeSize);

					inputs0.push_back(data[elemNdx1]);
					inputs1.push_back(data[elemNdx2]);
					inputs2.push_back(bitOffset);
					inputs3.push_back(bitCount);
					outputs.push_back(operation(data[elemNdx1], data[elemNdx2], bitOffset, bitCount));
					break;
				}
				default:
					DE_FATAL("Not implemented");
			}
			if (m_vectorSize == 3)
			{
				padCount--;
				if (padCount == 0)
				{
					inputs0.push_back(0);
					inputs1.push_back(0);
					inputs2.push_back(0);
					inputs3.push_back(0);
					outputs.push_back(0);
					padCount = m_vectorSize;
				}
			}
		}
	}

	outputsSize = static_cast<deUint32>(outputs.size());

	/* Ensure we have pushed a multiple of vector size, including padding if
	   required */
	while (outputsSize % sizeWithPadding != 0)
	{
		inputs0.pop_back();
		inputs1.pop_back();
		inputs2.pop_back();
		inputs3.pop_back();
		outputs.pop_back();
		outputsSize--;
	}

	pushResource(resources.inputs, inputs0);
	pushResource(resources.inputs, inputs1);
	pushResource(resources.inputs, inputs2);
	pushResource(resources.inputs, inputs3);
	pushResource(resources.outputs, outputs);

	pushResource(computeResources.inputs, inputs0);
	pushResource(computeResources.inputs, inputs1);
	pushResource(computeResources.inputs, inputs2);
	pushResource(computeResources.inputs, inputs3);
	pushResource(computeResources.outputs, outputs);

	return outputsSize / sizeWithPadding;
}

// This one is used for switch tests.
template <class T>
deUint32 SpvAsmTypeTests<T>::fillResources (GraphicsResources&	resources,
											ComputeShaderSpec&	computeResources,
											const vector<T>&	data)
{
	vector<T>	outputs;

	outputs.reserve(data.size());

	for (deUint32 elemNdx = 0; elemNdx < data.size(); ++elemNdx)
	{
		if (data[elemNdx] == m_cases[0])
			outputs.push_back(100);
		else if (data[elemNdx] == m_cases[1])
			outputs.push_back(110);
		else if (data[elemNdx] == m_cases[2])
			outputs.push_back(120);
		else
			outputs.push_back(10);
	}

	pushResource(resources.inputs, data);
	pushResource(resources.inputs, outputs);

	pushResource(computeResources.inputs, data);
	pushResource(computeResources.inputs, outputs);

	// Prepare an array of 32-bit integer values with a single integer. The expected value is 1.
	vector<deInt32> expectedOutput;
	expectedOutput.push_back(1);
	computeResources.outputs.push_back(Resource(BufferSp(new Int32Buffer(expectedOutput))));
	computeResources.verifyIO = verifyComputeSwitchResult;

	return static_cast<deUint32>(outputs.size());
}

template <class T>
void SpvAsmTypeTests<T>::createStageTests (const char*			testName,
										   GraphicsResources&	resources,
										   ComputeShaderSpec&	computeResources,
										   deUint32				numElements,
										   vector<string>&		decorations,
										   vector<string>&		pre_mains,
										   vector<string>&		testfuns,
										   string&				operation,
										   InputWidth			inputWidth,
										   const char*			funVariables,
										   const char*			spirvExtension)
{
	// Roughly equivalent to the following GLSL compute shader:
	//
	//      vec4 testfun(in vec4 param);
	//
	//      void main()
	//      {
	//          vec4 in_color	= vec4(0.0, 0.0, 0.0, 1.0);
	//          vec4 out_color	= testfun(in_color);
	//      }
	//
	// The input and output colors are irrelevant, but testfun will iterate over the input buffers and calculate results on the output
	// buffer. After the compute shader has run, we can verify the output buffer contains the expected results.
	const tcu::StringTemplate computeShaderTemplate(R"(
					OpCapability Shader
					${capability:opt}
					${extension:opt}
					OpMemoryModel Logical GLSL450
					OpEntryPoint GLCompute %BP_main "main"
					OpExecutionMode %BP_main LocalSize 1 1 1
					${execution_mode:opt}
					${debug:opt}
					${moduleprocessed:opt}
					${IF_decoration:opt}
					${decoration:opt}
	)"
					SPIRV_ASSEMBLY_TYPES
					SPIRV_ASSEMBLY_CONSTANTS
					SPIRV_ASSEMBLY_ARRAYS
	R"(
		%BP_color = OpConstantComposite %v4f32 %c_f32_0 %c_f32_0 %c_f32_0 %c_f32_1
					${pre_main:opt}
					${IF_variable:opt}
		 %BP_main = OpFunction %void None %voidf
   %BP_label_main = OpLabel
					${IF_carryforward:opt}
					${post_interface_op_comp:opt}
	 %BP_in_color = OpVariable %fp_v4f32 Function
	%BP_out_color = OpVariable %fp_v4f32 Function
					OpStore %BP_in_color %BP_color
		 %BP_tmp1 = OpLoad %v4f32 %BP_in_color
		 %BP_tmp2 = OpFunctionCall %v4f32 %test_code %BP_tmp1
					OpStore %BP_out_color %BP_tmp2
					OpReturn
					OpFunctionEnd

					${testfun}
	)");

	const StringTemplate		decoration		("OpDecorate %output DescriptorSet 0\n"
												 "OpDecorate %output Binding ${output_binding}\n"
												 "OpDecorate %a${num_elements}testtype ArrayStride ${typesize}\n"
												 "OpDecorate %buf BufferBlock\n"
												 "OpMemberDecorate %buf 0 Offset 0\n");

	const StringTemplate		vecDecoration	("OpDecorate %a${num_elements}scalartype ArrayStride ${typesize}\n"
												 "OpDecorate %scalarbuf BufferBlock\n"
												 "OpMemberDecorate %scalarbuf 0 Offset 0\n");

	const StringTemplate		pre_pre_main	("%c_u32_${num_elements} = OpConstant %u32 ${num_elements}\n"
												 "%c_i32_${num_elements} = OpConstant %i32 ${num_elements}\n");

	const StringTemplate		scalar_pre_main	("%testtype = ${scalartype}\n");

	const StringTemplate		vector_pre_main	("%scalartype = ${scalartype}\n"
												 "%testtype = OpTypeVector %scalartype ${vector_size}\n");

	const StringTemplate		pre_main_consts	("%c_shift  = OpConstant %u32 16\n"
												 "${constant_zero}\n"
												 "${constant_one}\n");

	const StringTemplate		pre_main_constv	("%c_shift1 = OpConstant %u32 16\n"
												 "%c_shift  = OpConstantComposite %v${vector_size}u32 ${shift_initializers}\n"
												 "${bvec}\n"
												 "${constant_zero}\n"
												 "${constant_one}\n"
												 "%a${num_elements}scalartype = OpTypeArray %u32 %c_u32_${num_elements}\n"
												 "%up_scalartype = OpTypePointer Uniform %u32\n"
												 "%scalarbuf = OpTypeStruct %a${num_elements}scalartype\n"
												 "%scalarbufptr = OpTypePointer Uniform %scalarbuf\n");

	const StringTemplate		post_pre_main	("%a${num_elements}testtype = OpTypeArray %${testtype} "
												 "%c_u32_${num_elements}\n"
												 "%up_testtype = OpTypePointer Uniform %${testtype}\n"
												 "%buf = OpTypeStruct %a${num_elements}testtype\n"
												 "%bufptr = OpTypePointer Uniform %buf\n"
												 "%output = OpVariable %bufptr Uniform\n"
												 "${other_size_types}\n"
												 "${u32_function_pointer}\n");

	const StringTemplate		pre_testfun		("%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
												 "%param = OpFunctionParameter %v4f32\n"
												 "%entry = OpLabel\n"
												 "%op_constant = OpVariable %fp_${testtype} Function\n"
												 + string(funVariables) +
												 "%counter = OpVariable %fp_i32 Function\n"
												 "OpStore %counter %c_i32_0\n"
												 "OpBranch %loop\n"

												 "%loop = OpLabel\n"
												 "%counter_val = OpLoad %i32 %counter\n"
												 "%lt = OpSLessThan %bool %counter_val %c_i32_${num_elements}\n"
												 "OpLoopMerge %exit %inc None\n"
												 "OpBranchConditional %lt %write %exit\n"

												 "%write = OpLabel\n"
												 "%output_loc = OpAccessChain %up_testtype %output %c_i32_0 "
												 "%counter_val\n");

	const StringTemplate		post_testfun	("OpStore %output_loc %op_result\n"
												 "OpBranch %inc\n"

												 "%inc = OpLabel\n"
												 "%counter_val_next = OpIAdd %i32 %counter_val %c_i32_1\n"
												 "OpStore %counter %counter_val_next\n"
												 "OpBranch %loop\n"

												 "%exit = OpLabel\n"
												 "OpReturnValue %param\n"

												 "OpFunctionEnd\n");

	const bool					uses8bit		(m_inputType == TYPE_I8 || m_inputType == TYPE_U8 || has8BitInputWidth(inputWidth));
	const string				vectorSizeStr	(de::toString(m_vectorSize));
	std::vector<std::string>	noExtensions;
	std::vector<std::string>	features;
	RGBA						defaultColors[4];
	map<string, string>			fragments;
	map<string, string>			specs;
	VulkanFeatures				requiredFeatures;
	std::string					spirvExtensions;
	std::string					spirvCapabilities;

	getDefaultColors(defaultColors);

	if (m_vectorSize == 3)
	{
		resources.verifyIO = verifyVec3Result;
		computeResources.verifyIO = verifyVec3Result;
	}
	else
	{
		resources.verifyIO = verifyDefaultResult;
		computeResources.verifyIO = verifyDefaultResult;
	}

	// All of the following tests write their results into an output SSBO, therefore they require the following features.
	requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
	requiredFeatures.coreFeatures.fragmentStoresAndAtomics = DE_TRUE;

	if (m_deviceFeature)
		features.insert(features.begin(), m_deviceFeature);

	if (inputWidth != WIDTH_DEFAULT)
	{
		if (has16BitInputWidth(inputWidth))
			features.insert(features.begin(), "shaderInt16");
		if (has64BitInputWidth(inputWidth))
			features.insert(features.begin(), "shaderInt64");
	}

	if (uses8bit)
	{
		requiredFeatures.extFloat16Int8	|= EXTFLOAT16INT8FEATURES_INT8;
	}

	if (m_inputType == TYPE_I8 || m_inputType == TYPE_U8)
	{
		requiredFeatures.ext8BitStorage	|= EXT8BITSTORAGEFEATURES_UNIFORM_STORAGE_BUFFER;
		spirvExtensions					+= "OpExtension \"SPV_KHR_8bit_storage\"\n";
	}

	if (m_inputType == TYPE_I16 || m_inputType == TYPE_U16)
	{
		requiredFeatures.ext16BitStorage	|= EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK;
		spirvExtensions						+= "OpExtension \"SPV_KHR_16bit_storage\"\n";
	}

	specs["testtype"]				= m_spirvTestType;
	specs["scalartype"]				= m_spirvType;
	specs["typesize"]				= de::toString(((m_vectorSize == 3) ? 4 : m_vectorSize) * m_typeSize / 8);
	specs["vector_size"]			= vectorSizeStr;
	specs["num_elements"]			= de::toString(numElements);
	specs["output_binding"]			= de::toString(resources.inputs.size());
	specs["shift_initializers"]		= replicate(" %c_shift1", m_vectorSize);

	specs["bvec"]					= (m_vectorSize == 1 || m_vectorSize == 4) ? ("")
									: ("%v" + vectorSizeStr + "bool = OpTypeVector %bool " + vectorSizeStr);

	specs["constant_zero"]			= (m_vectorSize == 1)
									? ("%c_zero = OpConstant %u32 0\n")
									: ("%c_zero = OpConstantComposite %v" + vectorSizeStr + "u32" + replicate(" %c_u32_0", m_vectorSize));

	specs["constant_one"]			= (m_vectorSize == 1)
									? ("%c_one = OpConstant %u32 1\n")
									: ("%c_one = OpConstantComposite %v" + vectorSizeStr + "u32" + replicate(" %c_u32_1", m_vectorSize));

	specs["other_size_types"]		= (inputWidth == WIDTH_DEFAULT) ? ("")
									: getOtherSizeTypes(m_inputType, m_vectorSize, inputWidth);

	specs["u32_function_pointer"]	= deStringEqual(m_spirvTestType.c_str(), "i32") ? ("")
									: ("%fp_" + m_spirvTestType + " = OpTypePointer Function %" + m_spirvTestType + "\n");

	if (spirvExtension)
		spirvExtensions += "%ext1 = OpExtInstImport \"" + string(spirvExtension) + "\"";

	for (deUint32 elemNdx = 0; elemNdx < decorations.size(); ++elemNdx)
		fragments["decoration"] += decorations[elemNdx];
	fragments["decoration"] += decoration.specialize(specs);

	if (m_vectorSize > 1)
		fragments["decoration"] += vecDecoration.specialize(specs);

	fragments["pre_main"] = pre_pre_main.specialize(specs);
	if (specs["testtype"].compare(UNDEFINED_SPIRV_TEST_TYPE) == 0)
	{
		if (m_vectorSize > 1)
			fragments["pre_main"] += vector_pre_main.specialize(specs);
		else
			fragments["pre_main"] += scalar_pre_main.specialize(specs);
	}

	if (m_vectorSize > 1)
		fragments["pre_main"] += pre_main_constv.specialize(specs);
	else
		fragments["pre_main"] += pre_main_consts.specialize(specs);

	fragments["pre_main"] += post_pre_main.specialize(specs);
	for (deUint32 elemNdx = 0; elemNdx < pre_mains.size(); ++elemNdx)
		fragments["pre_main"] += pre_mains[elemNdx];

	fragments["testfun"] = pre_testfun.specialize(specs);
	for (deUint32 elemNdx = 0; elemNdx < testfuns.size(); ++elemNdx)
		fragments["testfun"] += testfuns[elemNdx];
	fragments["testfun"] += operation + post_testfun.specialize(specs);

	spirvCapabilities += getSpirvCapabilityStr(m_spirvCapability, inputWidth);

	fragments["extension"]	= spirvExtensions;
	fragments["capability"]	= spirvCapabilities;

	requiredFeaturesFromStrings(features, requiredFeatures);
	computeResources.requestedVulkanFeatures = requiredFeatures;

	createTestsForAllStages(testName, defaultColors, defaultColors, fragments, resources, noExtensions, this, requiredFeatures);
	createComputeTest(computeResources, computeShaderTemplate, fragments, *this, testName);
}

template <class T>
std::string valueToStr(const T v)
{
	std::stringstream s;
	s << v;
	return s.str();
}

template <>
std::string valueToStr<deUint8> (const deUint8 v)
{
	std::stringstream s;
	s << (deUint16)v;
	return s.str();
}

template <>
std::string valueToStr<deInt8> ( const deInt8 v)
{
	std::stringstream s;
	s << (deInt16)v;
	return s.str();
}

template <class T>
bool SpvAsmTypeTests<T>::verifyResult (const vector<Resource>&		inputs,
									   const vector<AllocationSp>&	outputAllocations,
									   const vector<Resource>&		expectedOutputs,
									   deUint32						skip,
									   tcu::TestLog&				log)
{
	DE_ASSERT(outputAllocations.size() == 1);
	DE_ASSERT(inputs.size() > 0 && inputs.size() < 5);

	const T*		input[4]		= { DE_NULL };
	vector<deUint8>	inputBytes[4];
	vector<deUint8>	expectedBytes;

	expectedOutputs[0].getBytes(expectedBytes);
	const deUint32 count	= static_cast<deUint32>(expectedBytes.size() / sizeof(T));
	const T* obtained		= static_cast<const T *>(outputAllocations[0]->getHostPtr());
	const T* expected		= reinterpret_cast<const T*>(&expectedBytes.front());

	for (deUint32 ndxCount = 0; ndxCount < inputs.size(); ndxCount++)
	{
		inputs[ndxCount].getBytes(inputBytes[ndxCount]);
		input[ndxCount]	= reinterpret_cast<const T*>(&inputBytes[ndxCount].front());
	}

	for (deUint32 ndxCount = 0 ; ndxCount < count; ++ndxCount)
	{
		/* Skip padding */
		if (((ndxCount + 1) % skip) == 0)
			continue;

		if (obtained[ndxCount] != expected[ndxCount])
		{
			std::stringstream inputStream;
			inputStream << "(";
			for (deUint32 ndxIndex = 0 ; ndxIndex < inputs.size(); ++ndxIndex)
			{
				inputStream << valueToStr(input[ndxIndex][ndxCount]);
				if (ndxIndex < inputs.size() - 1)
					inputStream << ",";
			}
			inputStream << ")";
			log << tcu::TestLog::Message
				<< "Error: found unexpected result for inputs " << inputStream.str()
				<< ": expected " << valueToStr(expected[ndxCount]) << ", obtained "
				<< valueToStr(obtained[ndxCount]) << tcu::TestLog::EndMessage;
			return false;
		}
	}

	return true;
}

template <class T>
bool SpvAsmTypeTests<T>::verifyDefaultResult (const vector<Resource>&		inputs,
											  const vector<AllocationSp>&	outputAllocations,
											  const vector<Resource>&		expectedOutputs,
											  tcu::TestLog&					log)
{
	return verifyResult(inputs, outputAllocations, expectedOutputs, ~0, log);
}

template <class T>
bool SpvAsmTypeTests<T>::verifyVec3Result (const vector<Resource>&		inputs,
										   const vector<AllocationSp>&	outputAllocations,
										   const vector<Resource>&		expectedOutputs,
										   tcu::TestLog&				log)
{
	return verifyResult(inputs, outputAllocations, expectedOutputs, 4, log);
}

template <class T>
string SpvAsmTypeTests<T>::createConstantDeclaration (vector<T>& dataset, deUint32 spirvOperation)
{
	const bool		isVariableTest				= (SpvOpVariable == spirvOperation);
	const bool		isConstantNullTest			= (SpvOpConstantNull == spirvOperation) || isVariableTest;
	const bool		isConstantCompositeTest		= (SpvOpConstantComposite == spirvOperation) || (isConstantNullTest && m_vectorSize > 1);
	const bool		isConstantTest				= (SpvOpConstant == spirvOperation) || isConstantCompositeTest || isConstantNullTest;
	const bool		isSpecConstantTest			= (SpvOpSpecConstant == spirvOperation);
	const bool		isSpecConstantCompositeTest	= (SpvOpSpecConstantComposite == spirvOperation);

	const string	testScalarType				= (m_inputType == TYPE_I32) ? "i32"
												: (m_inputType == TYPE_U32) ? "u32"
												: "scalartype";
	const string	constantType				= (m_vectorSize > 1) ? testScalarType : m_spirvTestType;
	const string	constantName				= (m_vectorSize > 1) ? "%c_constituent_" : "%c_testtype_";

	string			str							= "";

	// Declare scalar specialization constants
	if (isSpecConstantTest)
	{
		for (size_t constantNdx = 0u; constantNdx < dataset.size(); constantNdx++)
			str += constantName + de::toString(constantNdx) + " = OpSpecConstant %" + constantType + " " + de::toString(dataset[constantNdx]) + "\n";
	}

	// Declare specialization constant composites
	if (isSpecConstantCompositeTest)
	{
		// Constituents are a mix of OpConstantNull, OpConstants and OpSpecializationConstants
		for (size_t constantNdx = 0u; constantNdx < dataset.size(); constantNdx++)
		{
			const char* constantOp[] =
			{
				"OpConstant",
				"OpSpecConstant"
			};

			if (constantNdx == 0u)
				str += constantName + de::toString(constantNdx) + " = OpConstantNull %" + constantType + "\n";
			else
				str += constantName + de::toString(constantNdx) + " = " + constantOp[constantNdx % 2] + " %" + constantType + " " + de::toString(dataset[constantNdx]) + "\n";
		}

		for (deUint32 compositeNdx = 0u; compositeNdx < (deUint32)dataset.size(); compositeNdx++)
		{
			str += "%c_testtype_" + de::toString(compositeNdx) + " = OpSpecConstantComposite %" + m_spirvTestType;

			for (deUint32 componentNdx = 0u; componentNdx < m_vectorSize; componentNdx++)
				str += " %c_constituent_" + de::toString(getConstituentIndex(compositeNdx * m_vectorSize + componentNdx, m_vectorSize));

			str += "\n";
		}
	}

	// Declare scalar constants
	if (isConstantTest || isVariableTest)
	{
		for (size_t constantNdx = 0u; constantNdx < dataset.size(); constantNdx++)
		{
			if (isConstantNullTest && constantNdx == 0u)
				str += constantName + de::toString(constantNdx) + " = OpConstantNull %" + constantType + "\n";
			else
				str += constantName + de::toString(constantNdx) + " = OpConstant %" + constantType + " " + de::toString(dataset[constantNdx]) + "\n";
		}
	}

	// Declare constant composites
	if (isConstantCompositeTest)
	{
		for (deUint32 compositeNdx = 0u; compositeNdx < (deUint32)dataset.size(); compositeNdx++)
		{
			str += "%c_testtype_" + de::toString(compositeNdx) + " = OpConstantComposite %" + m_spirvTestType;

			for (deUint32 componentNdx = 0u; componentNdx < m_vectorSize; componentNdx++)
				str += " %c_constituent_" + de::toString(getConstituentIndex(compositeNdx * m_vectorSize + componentNdx, m_vectorSize));

			str += "\n";
		}
	}

	return str;
}

template <class T>
string getVariableStr (vector<T>& dataset, const char* spirvType, deUint32 spirvOperation)
{
	const bool		isVariableTest = (SpvOpVariable == spirvOperation);
	string			str = "";

	// Declare variables with initializers
	if (isVariableTest)
		for (size_t i = 0u; i < dataset.size(); i++)
			str += "%testvariable_" + de::toString(i) + " = OpVariable %fp_" + spirvType + " Function %c_testtype_" + de::toString(i) + "\n";

	return str;
}

template <class T>
void SpvAsmTypeTests<T>::createTests (const char*			testName,
									  deUint32				spirvOperation,
									  OpUnaryFuncType		operation,
									  UnaryFilterFuncType	filter,
									  InputRange			inputRange,
									  InputWidth			inputWidth,
									  const char*			spirvExtension,
									  const bool			returnHighPart)
{
	DE_ASSERT(!isBooleanResultTest(spirvOperation));

	const string		resultName	= returnHighPart ? "%op_result_pre" : "%op_result";
	OpUnaryFuncType		zeroFunc	= &zero;
	vector<T>			dataset;
	vector<string>		decorations;
	vector<string>		pre_mains;
	vector<string>		testfuns;
	GraphicsResources	resources;
	ComputeShaderSpec	computeResources;
	map<string, string>	fragments;
	map<string, string>	specs;

	if (isConstantOrVariableTest(spirvOperation))
	{
		DE_ASSERT(!spirvExtension);

		const deUint32		inputSize		= TEST_DATASET_SIZE;
		const deUint32		outputSize		= TEST_DATASET_SIZE * m_vectorSize;
		vector<T>			inputDataset;

		inputDataset.reserve(inputSize);
		dataset.reserve(outputSize);

		getDataset(inputDataset, inputSize);
		getConstantDataset(inputDataset, dataset, spirvOperation);

		const deUint32		totalElements	= combine(resources, computeResources, dataset, (returnHighPart ? zeroFunc : operation), filter, inputRange);

		pre_mains.reserve(1);
		pre_mains.push_back(createConstantDeclaration(inputDataset, spirvOperation));

		string				fullOperation	= "OpBranch %switchStart\n"
											  "%switchStart = OpLabel\n"
											  "OpSelectionMerge %switchEnd None\n"
											  "OpSwitch %counter_val %caseDefault";

		for (deUint32 caseNdx = 0u; caseNdx < inputSize; caseNdx++)
			fullOperation += " " + de::toString(caseNdx) + " " + "%case" + de::toString(caseNdx);

		fullOperation += "\n";

		const string		funVariables	= getVariableStr(inputDataset, m_spirvTestType.c_str(), spirvOperation);

		if (SpvOpVariable == spirvOperation)
		{
			for (deUint32 caseNdx = 0u; caseNdx < inputSize; caseNdx++)
				fullOperation +=	"%case" + de::toString(caseNdx) + " = OpLabel\n"
									"%temp_" + de::toString(caseNdx) + " = OpLoad %" + m_spirvTestType + " %testvariable_" + de::toString(caseNdx) + "\n"
									"OpStore %op_constant %temp_" + de::toString(caseNdx) + "\n"
									"OpBranch %switchEnd\n";
		}
		else
		{
			for (deUint32 caseNdx = 0u; caseNdx < inputSize; caseNdx++)
				fullOperation +=	"%case" + de::toString(caseNdx) + " = OpLabel\n"
									"OpStore %op_constant %c_testtype_" + de::toString(caseNdx) + "\n"
									"OpBranch %switchEnd\n";
		}

		fullOperation +=	"%caseDefault = OpLabel\n"
							"OpBranch %switchEnd\n"
							"%switchEnd = OpLabel\n"
							+ resultName + " = OpLoad %" + m_spirvTestType + " %op_constant\n";


		finalizeFullOperation(fullOperation, resultName, returnHighPart, false);

		createStageTests(testName, resources, computeResources, totalElements, decorations,
						 pre_mains, testfuns, fullOperation, inputWidth, funVariables.c_str(), spirvExtension);
	}
	else
	{
		dataset.reserve(TEST_DATASET_SIZE * m_vectorSize);
		getDataset(dataset, TEST_DATASET_SIZE * m_vectorSize);
		const deUint32	totalElements	= combine(resources, computeResources, dataset, (returnHighPart ? zeroFunc : operation), filter, inputRange);

		decorations.reserve(1);
		pre_mains.reserve(1);
		testfuns.reserve(1);

		decorations.push_back(createInputDecoration(0));
		pre_mains.push_back(createInputPreMain(0, spirvOperation));
		testfuns.push_back(createInputTestfun(0, spirvOperation));

		string			full_operation	(spirvExtension ? resultName + " = OpExtInst %" + m_spirvTestType + " %ext1 " + getGLSLstd450OperationStr(spirvOperation) + " %input0_val\n"
										: resultName + " = " + getSpvOperationStr(spirvOperation) + " %" + m_spirvTestType + " %input0_val\n");

		finalizeFullOperation(full_operation, resultName, returnHighPart, false);

		createStageTests(testName, resources, computeResources, totalElements, decorations,
						 pre_mains, testfuns, full_operation, inputWidth, "", spirvExtension);
	}
}

template <class T>
void SpvAsmTypeTests<T>::createTests (const char*			testName,
									  deUint32				spirvOperation,
									  OpBinaryFuncType		operation,
									  BinaryFilterFuncType	filter,
									  InputRange			inputRange,
									  InputWidth			inputWidth,
									  const char*			spirvExtension,
									  const bool			returnHighPart)
{
	const bool			isBoolean		= isBooleanResultTest(spirvOperation);
	const string		resultName		= (returnHighPart || isBoolean) ? "%op_result_pre" : "%op_result";
	const string		resultType		= isBoolean ? getBooleanResultType(m_vectorSize) : m_spirvTestType;
	OpBinaryFuncType	zeroFunc		= &zero;
	vector<T>			dataset;
	vector<string>		decorations;
	vector<string>		pre_mains;
	vector<string>		testfuns;
	GraphicsResources	resources;
	ComputeShaderSpec	computeResources;
	map<string, string>	fragments;
	map<string, string>	specs;
	string				full_operation;

	dataset.reserve(TEST_DATASET_SIZE * m_vectorSize);
	getDataset(dataset, TEST_DATASET_SIZE * m_vectorSize);
	const deUint32		totalElements	= combine(resources, computeResources, dataset, (returnHighPart ? zeroFunc : operation), filter, inputRange);

	decorations.reserve(2);
	pre_mains.reserve(2);
	testfuns.reserve(2);

	for (deUint32 elemNdx = 0; elemNdx < 2; ++elemNdx)
	{
		decorations.push_back(createInputDecoration(elemNdx));
		pre_mains.push_back(createInputPreMain(elemNdx, spirvOperation));
		testfuns.push_back(createInputTestfun(elemNdx, spirvOperation));
	}

	if (spirvOperation != DE_NULL)
	{
		if (inputWidth == WIDTH_DEFAULT)
			full_operation = spirvExtension	? resultName + " = OpExtInst %" + resultType + " %ext1 " + getGLSLstd450OperationStr(spirvOperation) + " %input0_val %input1_val\n"
											: resultName + " = " + getSpvOperationStr(spirvOperation) + " %" + resultType + " %input0_val %input1_val\n";
		else
			full_operation = getBinaryFullOperationWithInputWidthStr(resultName, getSpvOperationStr(spirvOperation), m_inputType, m_spirvTestType, m_vectorSize, inputWidth);
	}
	else
	{
		if (deStringBeginsWith(testName, "mul_sdiv"))
		{
			DE_ASSERT(spirvExtension == DE_NULL);
			full_operation = "%op_result2 = OpIMul %" + m_spirvTestType + " %input0_val %input1_val\n";
			full_operation += resultName + " = OpSDiv %" + m_spirvTestType + " %op_result2 %input1_val\n";
		}
		if (deStringBeginsWith(testName, "mul_udiv"))
		{
			DE_ASSERT(spirvExtension == DE_NULL);
			full_operation = "%op_result2 = OpIMul %" + m_spirvTestType + " %input0_val %input1_val\n";
			full_operation += resultName + " = OpUDiv %" + m_spirvTestType + " %op_result2 %input1_val\n";
		}
	}

	finalizeFullOperation(full_operation, resultName, returnHighPart, isBoolean);

	createStageTests(testName, resources, computeResources, totalElements, decorations,
					 pre_mains, testfuns, full_operation, inputWidth, "", spirvExtension);
}

template <class T>
void SpvAsmTypeTests<T>::createTests (const char*			testName,
									  deUint32				spirvOperation,
									  OpTernaryFuncType		operation,
									  TernaryFilterFuncType	filter,
									  InputRange			inputRange,
									  InputWidth			inputWidth,
									  const char*			spirvExtension,
									  const bool			returnHighPart)
{
	DE_ASSERT(!isBooleanResultTest(spirvOperation));

	const string		resultName		= returnHighPart ? "%op_result_pre" : "%op_result";
	OpTernaryFuncType	zeroFunc		= &zero;
	vector<T>			dataset;
	vector<string>		decorations;
	vector<string>		pre_mains;
	vector<string>		testfuns;
	GraphicsResources	resources;
	ComputeShaderSpec	computeResources;
	map<string, string>	fragments;
	map<string, string>	specs;

	dataset.reserve(TEST_DATASET_SIZE * m_vectorSize);
	getDataset(dataset, TEST_DATASET_SIZE * m_vectorSize);
	const deUint32		totalElements	= combine(resources, computeResources, dataset, (returnHighPart ? zeroFunc : operation), filter, inputRange);

	decorations.reserve(3);
	pre_mains.reserve(3);
	testfuns.reserve(3);

	for (deUint32 elemNdx = 0; elemNdx < 3; ++elemNdx)
	{
		decorations.push_back(createInputDecoration(elemNdx));
		pre_mains.push_back(createInputPreMain(elemNdx, spirvOperation));
		testfuns.push_back(createInputTestfun(elemNdx, spirvOperation));
	}

	string				full_operation	= "";

	if (inputWidth == WIDTH_DEFAULT)
		full_operation = (spirvExtension ? resultName + " = OpExtInst %" + m_spirvTestType + " %ext1 " + getGLSLstd450OperationStr(spirvOperation) + " %input0_val %input1_val %input2_val\n"
										 : resultName + " = " + getSpvOperationStr(spirvOperation) + " %" + m_spirvTestType + " %input0_val %input1_val %input2_val\n");
	else
		full_operation = getFullOperationWithDifferentInputWidthStr(resultName, getSpvOperationStr(spirvOperation), m_inputType, m_spirvTestType, inputWidth, false);

	finalizeFullOperation(full_operation, resultName, returnHighPart, false);

	createStageTests(testName, resources, computeResources, totalElements, decorations,
					 pre_mains, testfuns, full_operation, inputWidth, "", spirvExtension);
}

template <class T>
void SpvAsmTypeTests<T>::createTests (const char*				testName,
									  deUint32					spirvOperation,
									  OpQuaternaryFuncType		operation,
									  QuaternaryFilterFuncType	filter,
									  InputRange				inputRange,
									  InputWidth				inputWidth,
									  const char*				spirvExtension,
									  const bool				returnHighPart)
{
	DE_ASSERT(!spirvExtension);
	DE_ASSERT(!isBooleanResultTest(spirvOperation));

	const string			resultName		= returnHighPart ? "%op_result_pre" : "%op_result";
	OpQuaternaryFuncType	zeroFunc		= &zero;
	vector<T>				dataset;
	vector<string>			decorations;
	vector<string>			pre_mains;
	vector<string>			testfuns;
	GraphicsResources		resources;
	ComputeShaderSpec		computeResources;
	map<string, string>		fragments;
	map<string, string>		specs;
	string					full_operation;

	dataset.reserve(TEST_DATASET_SIZE * m_vectorSize);
	getDataset(dataset, TEST_DATASET_SIZE * m_vectorSize);
	const deUint32			totalElements	= combine(resources, computeResources, dataset, (returnHighPart ? zeroFunc : operation), filter, inputRange);

	decorations.reserve(4);
	pre_mains.reserve(4);
	testfuns.reserve(4);

	for (deUint32 elemNdx = 0; elemNdx < 4; ++elemNdx)
	{
		decorations.push_back(createInputDecoration(elemNdx));
		pre_mains.push_back(createInputPreMain(elemNdx, spirvOperation));
		testfuns.push_back(createInputTestfun(elemNdx, spirvOperation));
	}

	if (inputWidth == WIDTH_DEFAULT)
		full_operation	= resultName + " = " + getSpvOperationStr(spirvOperation) + " %" + m_spirvTestType + " %input0_val %input1_val %input2_val %input3_val\n";
	else
		full_operation = getFullOperationWithDifferentInputWidthStr(resultName, getSpvOperationStr(spirvOperation), m_inputType, m_spirvTestType, inputWidth, true);

	finalizeFullOperation(full_operation, resultName, returnHighPart, false);

	createStageTests(testName, resources, computeResources, totalElements, decorations,
					 pre_mains, testfuns, full_operation, inputWidth, "", spirvExtension);
}

template <class T>
void SpvAsmTypeTests<T>::createSwitchTests (void)
{
	// The switch case test function is a bit different from the normal one. It uses two input buffers for input data and expected
	// results. The shader itself will calculate results based on input data and compare them to the expected results in the second
	// buffer, instead of verifying results on the CPU.
	//
	// The test function will return the color passed to it if the obtained results match the expected results, and will return (0.5,
	// 0.5, 0.5, 1.0) if they do not. For graphic stages, this returned color will be used to draw things and we can verify the output
	// image as usual with the graphics shader test utils. For compute shaders, this does not work.
	//
	// In this case, we will pass black as the input color for the test function, and will verify it returns black. We will write a
	// single integer in an output storage buffer as a boolean value indicating if the returned color matches the input color, to be
	// checked after the shader runs. Roughly equivalent to the following GLSL code:
	//
	//      layout(binding = 2) buffer BlockType { int values[]; } block;
	//
	//      vec4 testfun(in vec4 param);
	//
	//      void main()
	//      {
	//              vec4 in_color   = vec4(0.0, 0.0, 0.0, 1.0);
	//              vec4 out_color  = testfun(in_color);
	//              block.values[0] = int(all(equal(in_color, out_color)));
	//      }
	const tcu::StringTemplate computeShaderSwitchTemplate(R"(
					OpCapability Shader
					${capability:opt}
					${extension:opt}
					OpMemoryModel Logical GLSL450
					OpEntryPoint GLCompute %BP_main "main"
					OpExecutionMode %BP_main LocalSize 1 1 1
					${execution_mode:opt}
					${debug:opt}
					${moduleprocessed:opt}
					${IF_decoration:opt}
					${decoration:opt}
					OpDecorate %rta_i32 ArrayStride 4
					OpMemberDecorate %BlockType 0 Offset 0
					OpDecorate %BlockType BufferBlock
					OpDecorate %block DescriptorSet 0
					OpDecorate %block Binding 2
	)"
					SPIRV_ASSEMBLY_TYPES
					SPIRV_ASSEMBLY_CONSTANTS
					SPIRV_ASSEMBLY_ARRAYS
	R"(
		 %rta_i32 = OpTypeRuntimeArray %i32
	   %BlockType = OpTypeStruct %rta_i32
	%up_BlockType = OpTypePointer Uniform %BlockType
		   %block = OpVariable %up_BlockType Uniform
		%BP_color = OpConstantComposite %v4f32 %c_f32_0 %c_f32_0 %c_f32_0 %c_f32_1
					${pre_main:opt}
					${IF_variable:opt}
		  %up_i32 = OpTypePointer Uniform %i32
		 %BP_main = OpFunction %void None %voidf
   %BP_label_main = OpLabel
					${IF_carryforward:opt}
					${post_interface_op_comp:opt}
	 %BP_in_color = OpVariable %fp_v4f32 Function
	%BP_out_color = OpVariable %fp_v4f32 Function
					OpStore %BP_in_color %BP_color
		 %BP_tmp1 = OpLoad %v4f32 %BP_in_color
		 %BP_tmp2 = OpFunctionCall %v4f32 %test_code %BP_tmp1
					OpStore %BP_out_color %BP_tmp2

		 %BP_tmp3 = OpLoad %v4f32 %BP_in_color
		 %BP_tmp4 = OpLoad %v4f32 %BP_out_color
		 %BP_tmp5 = OpFOrdEqual %v4bool %BP_tmp3 %BP_tmp4
		 %BP_tmp6 = OpAll %bool %BP_tmp5
		 %BP_tmp7 = OpSelect %i32 %BP_tmp6 %c_i32_1 %c_i32_0
		 %BP_tmp8 = OpAccessChain %up_i32 %block %c_i32_0 %c_i32_0
					OpStore %BP_tmp8 %BP_tmp7

					OpReturn
					OpFunctionEnd

					${testfun}
	)");

	const StringTemplate	decoration		("OpDecorate %input DescriptorSet 0\n"
											 "OpDecorate %input Binding 0\n"
											 "OpDecorate %input NonWritable\n"
											 "OpDecorate %expectedOutput DescriptorSet 0\n"
											 "OpDecorate %expectedOutput Binding 1\n"
											 "OpDecorate %expectedOutput NonWritable\n"
											 "OpDecorate %a${num_elements}testtype ArrayStride ${typesize}\n"
											 "OpDecorate %buf BufferBlock\n"
											 "OpMemberDecorate %buf 0 Offset 0\n");

	const StringTemplate	pre_pre_main	("%fp_bool = OpTypePointer Function %bool\n"
											 "%c_u32_${num_elements} = OpConstant %u32 ${num_elements}\n"
											 "%c_i32_${num_elements} = OpConstant %i32 ${num_elements}\n");

	const StringTemplate	scalar_pre_main	("%testtype = ${scalartype}\n");

	const StringTemplate	post_pre_main	("%c_casedefault = OpConstant %${testtype} 10\n"
											 "%c_case0 = OpConstant %${testtype} 100\n"
											 "%c_case1 = OpConstant %${testtype} 110\n"
											 "%c_case2 = OpConstant %${testtype} 120\n"
											 "%fail_color = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_1\n"
											 "%a${num_elements}testtype = OpTypeArray %${testtype} %c_u32_${num_elements}\n"
											 "%up_testtype = OpTypePointer Uniform %${testtype}\n"
											 "%buf = OpTypeStruct %a${num_elements}testtype\n"
											 "%bufptr = OpTypePointer Uniform %buf\n"
											 "%input = OpVariable %bufptr Uniform\n"
											 "%expectedOutput = OpVariable %bufptr Uniform\n");

	const StringTemplate	testfun			("%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
											 "%param = OpFunctionParameter %v4f32\n"

											 "%entry = OpLabel\n"
											 "%counter = OpVariable %fp_i32 Function\n"
											 "%return = OpVariable %fp_v4f32 Function\n"
											 "%works = OpVariable %fp_bool Function\n"
											 "OpStore %counter %c_i32_0\n"
											 "OpStore %return %param\n"
											 "OpBranch %loop\n"

											 "%loop = OpLabel\n"
											 "%counter_val = OpLoad %i32 %counter\n"
											 "%lt = OpSLessThan %bool %counter_val %c_i32_${num_elements}\n"
											 "OpLoopMerge %loop_exit %inc None\n"
											 "OpBranchConditional %lt %load %loop_exit\n"

											 "%load = OpLabel\n"
											 "%input_loc = OpAccessChain %up_testtype %input %c_i32_0 %counter_val\n"
											 "%input_val = OpLoad %${testtype} %input_loc\n"
											 "%expectedOutput_loc = OpAccessChain %up_testtype %expectedOutput %c_i32_0 %counter_val\n"
											 "%expectedOutput_val = OpLoad %${testtype} %expectedOutput_loc\n"

											 "OpSelectionMerge %switch_exit None\n"
											 "OpSwitch %input_val %default ${case0} %case0 ${case1} %case1 ${case2} %case2\n"

											 "%default = OpLabel\n"
											 "%is_default = OpIEqual %bool %expectedOutput_val %c_casedefault\n"
											 "OpBranch %switch_exit\n"

											 "%case0 = OpLabel\n"
											 "%is_case0 = OpIEqual %bool %expectedOutput_val %c_case0\n"
											 "OpBranch %switch_exit\n"

											 "%case1 = OpLabel\n"
											 "%is_case1 = OpIEqual %bool %expectedOutput_val %c_case1\n"
											 "OpBranch %switch_exit\n"

											 "%case2 = OpLabel\n"
											 "%is_case2 = OpIEqual %bool %expectedOutput_val %c_case2\n"
											 "OpBranch %switch_exit\n"

											 "%switch_exit = OpLabel\n"
											 "%case_result = OpPhi %bool %is_default %default %is_case0 %case0 %is_case1 %case1 %is_case2 %case2\n"
											 "OpSelectionMerge %result_end None\n"
											 "OpBranchConditional %case_result %result_correct %result_incorrect\n"

											 "%result_correct = OpLabel\n"
											 "OpBranch %result_end\n"

											 "%result_incorrect = OpLabel\n"
											 "%counter_val_end = OpIAdd %i32 %counter_val %c_i32_${num_elements}\n"
											 "OpStore %counter %counter_val_end\n"
											 "OpStore %return %fail_color\n"
											 "OpBranch %result_end\n"

											 "%result_end = OpLabel\n"
											 "OpBranch %inc\n"

											 "%inc = OpLabel\n"
											 "%counter_val_next = OpIAdd %i32 %counter_val %c_i32_1\n"
											 "OpStore %counter %counter_val_next\n"
											 "OpBranch %loop\n"

											 "%loop_exit = OpLabel\n"
											 "%return_val = OpLoad %v4f32 %return\n"
											 "OpReturnValue %return_val\n"

											 "OpFunctionEnd\n");

	const bool				uses8bit		(m_inputType == TYPE_I8 || m_inputType == TYPE_U8);

	GraphicsResources		resources;
	ComputeShaderSpec		computeResources;
	RGBA					defaultColors[4];
	map<string, string>		fragments;
	map<string, string>		specs;
	std::vector<string>		noExtensions;
	std::vector<string>		features;
	VulkanFeatures			requiredFeatures;
	vector<T>				dataset;
	deUint32				numElements;
	std::string				spirvExtensions;
	std::string				spirvCapabilities;

	getDefaultColors(defaultColors);

	dataset.reserve(TEST_DATASET_SIZE);
	getDataset(dataset, TEST_DATASET_SIZE);
	numElements = fillResources(resources, computeResources, dataset);

	if (m_deviceFeature)
		features.insert(features.begin(), m_deviceFeature);

	if (uses8bit)
	{
		requiredFeatures.extFloat16Int8	|= EXTFLOAT16INT8FEATURES_INT8;
	}

	if (m_inputType == TYPE_I8 || m_inputType == TYPE_U8)
	{
		requiredFeatures.ext8BitStorage		|= EXT8BITSTORAGEFEATURES_UNIFORM_STORAGE_BUFFER;
		spirvExtensions						+= "OpExtension \"SPV_KHR_8bit_storage\"\n";
	}

	if (m_inputType == TYPE_I16 || m_inputType == TYPE_U16)
	{
		requiredFeatures.ext16BitStorage	|= EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK;
		spirvExtensions						+= "OpExtension \"SPV_KHR_16bit_storage\"\n";
	}

	specs["testtype"] = m_spirvTestType;
	specs["scalartype"] = m_spirvType;
	specs["typesize"] = de::toString(m_typeSize / 8);
	specs["num_elements"] = de::toString(numElements);
	specs["case0"] = de::toString(m_cases[0]);
	specs["case1"] = de::toString(m_cases[1]);
	specs["case2"] = de::toString(m_cases[2]);

	fragments["decoration"] = decoration.specialize(specs);

	fragments["pre_main"] = pre_pre_main.specialize(specs);
	if (specs["testtype"].compare(UNDEFINED_SPIRV_TEST_TYPE) == 0)
		fragments["pre_main"] += scalar_pre_main.specialize(specs);
	fragments["pre_main"] += post_pre_main.specialize(specs);

	fragments["testfun"] = testfun.specialize(specs);

	spirvCapabilities += getSpirvCapabilityStr(m_spirvCapability, WIDTH_DEFAULT);

	fragments["extension"]	= spirvExtensions;
	fragments["capability"]	= spirvCapabilities;

	requiredFeaturesFromStrings(features, requiredFeatures);
	computeResources.requestedVulkanFeatures = requiredFeatures;

	const string testName = "switch";

	createTestsForAllStages(testName, defaultColors, defaultColors, fragments, resources, noExtensions, this, requiredFeatures);
	createComputeTest(computeResources, computeShaderSwitchTemplate, fragments, *this, testName);
}

template <class T>
void SpvAsmTypeTests<T>::getConstantDataset (vector<T> inputDataset, vector<T>& outputDataset, deUint32 spirvOperation)
{
	const deUint32 numElements = (deUint32)inputDataset.size();

	if ((SpvOpConstant == spirvOperation) || (SpvOpSpecConstant == spirvOperation))
	{
		for (deUint32 elementNdx = 0u; elementNdx < numElements; elementNdx++)
			outputDataset.push_back(inputDataset[elementNdx]);
	}
	else
	{
		for (deUint32 elementNdx = 0; elementNdx < numElements * m_vectorSize; elementNdx++)
			outputDataset.push_back(inputDataset[getConstituentIndex(elementNdx, m_vectorSize)]);
	}
}

template <class T>
void SpvAsmTypeTests<T>::finalizeFullOperation (string&			fullOperation,
												const string&	resultName,
												const bool		returnHighPart,
												const bool		isBooleanResult)
{
	DE_ASSERT(!fullOperation.empty());

	if (returnHighPart)
	{
		DE_ASSERT(sizeof(T) == sizeof(deInt16));
		DE_ASSERT((m_inputType == TYPE_I16) || (m_inputType == TYPE_U16));

		const bool		signedness		= (m_inputType == TYPE_I16);
		const string	convertOp		= signedness ? "OpSConvert" : "OpUConvert";
		const string	convertPrefix	= (m_vectorSize == 1) ? "" : "v" + de::toString(m_vectorSize);
		const string	convertType		= convertPrefix + "u32";

		// Zero extend value to double-width value, then return high part
		fullOperation += "%op_result_a = OpUConvert %" + convertType + " " + resultName + "\n";
		fullOperation += "%op_result_b = OpShiftRightLogical %" + convertType + " %op_result_a %c_shift\n";
		fullOperation += "%op_result   = " + convertOp + " %" + m_spirvTestType + " %op_result_b\n";
	}
	else if (isBooleanResult)
	{
		const string selectType = (m_vectorSize == 1) ? ("u32") : ("v" + de::toString(m_vectorSize) + "u32");

		// Convert boolean values to result format
		if (m_inputType == TYPE_U32)
		{
			fullOperation += "%op_result     = OpSelect %" + selectType + " %op_result_pre %c_one %c_zero\n";
		}
		else
		{
			fullOperation += "%op_result_u32 = OpSelect %" + selectType + " %op_result_pre %c_one %c_zero\n";

			if (m_typeSize == 32)
				fullOperation += "%op_result     = OpBitcast %" + m_spirvTestType + " %op_result_u32\n";
			else
				fullOperation += "%op_result     = OpSConvert %" + m_spirvTestType + " %op_result_u32\n";
		}
	}
	else
	{
		DE_ASSERT(resultName == "%op_result");
	}
}

template <class T>
bool SpvAsmTypeTests<T>::filterNone	(T)
{
	return true;
}

template <class T>
bool SpvAsmTypeTests<T>::filterNone (T, T)
{
	return true;
}

template <class T>
bool SpvAsmTypeTests<T>::filterNone (T, T, T)
{
	return true;
}

template <class T>
bool SpvAsmTypeTests<T>::filterNone (T, T, T, T)
{
	return true;
}

template <class T>
bool SpvAsmTypeTests<T>::filterZero	(T, T b)
{
	if (b == static_cast<T>(0))
		return false;
	else
		return true;
}

template <class T>
bool SpvAsmTypeTests<T>::filterNegativesAndZero	(T a, T b)
{
	if (a < static_cast<T>(0) || b <= static_cast<T>(0))
		return false;
	else
		return true;
}

template <class T>
bool SpvAsmTypeTests<T>::filterMinGtMax	(T, T a, T b)
{
	if (a > b)
		return false;
	else
		return true;
}

template <class T>
T SpvAsmTypeTests<T>::zero (T)
{
	return static_cast<T>(0.0);
}

template <class T>
T SpvAsmTypeTests<T>::zero (T, T)
{
	return static_cast<T>(0.0);
}

template <class T>
T SpvAsmTypeTests<T>::zero (T, T, T)
{
	return static_cast<T>(0.0);
}

template <class T>
T SpvAsmTypeTests<T>::zero (T, T, T, T)
{
	return static_cast<T>(0.0);
}

template <class T>
std::string	SpvAsmTypeTests<T>::replicate (const std::string&	replicant,
										   const deUint32		count)
{
	std::string result;

	for (deUint32 i = 0; i < count; ++i)
		result += replicant;

	return result;
}

class SpvAsmTypeInt8Tests : public SpvAsmTypeTests<deInt8>
{
public:
				SpvAsmTypeInt8Tests		(tcu::TestContext&		testCtx,
										 deUint32				vectorSize);
				~SpvAsmTypeInt8Tests	(void);
	void		getDataset				(vector<deInt8>&		input,
										 deUint32				numElements);
	void		pushResource			(vector<Resource>&		resource,
										 const vector<deInt8>&	data);
};

SpvAsmTypeInt8Tests::SpvAsmTypeInt8Tests	(tcu::TestContext&	testCtx,
											 deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "i8", "int8 tests", DE_NULL, "Int8", "OpTypeInt 8 1", TYPE_I8, 8, vectorSize)
{
	m_cases[0] = -42;
	m_cases[1] = 73;
	m_cases[2] = 121;
}

SpvAsmTypeInt8Tests::~SpvAsmTypeInt8Tests (void)
{
}

void SpvAsmTypeInt8Tests::getDataset (vector<deInt8>&	input,
									  deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);
	input.push_back(static_cast<deInt8>(deIntMinValue32(8)));// A 8-bit negative number
	input.push_back(static_cast<deInt8>(deIntMaxValue32(8)));// A 8-bit positive number

	// Push switch cases
	input.push_back(m_cases[0]);
	input.push_back(m_cases[1]);
	input.push_back(m_cases[2]);

	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(static_cast<deInt8>(m_rnd.getUint8()));
}

void SpvAsmTypeInt8Tests::pushResource (vector<Resource>&		resource,
										const vector<deInt8>&	data)
{
	resource.push_back(Resource(BufferSp(new Int8Buffer(data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

class SpvAsmTypeInt16Tests : public SpvAsmTypeTests<deInt16>
{
public:
				SpvAsmTypeInt16Tests	(tcu::TestContext&		testCtx,
										 deUint32				vectorSize);
				~SpvAsmTypeInt16Tests	(void);
	void		getDataset				(vector<deInt16>&		input,
										 deUint32				numElements);
	void		pushResource			(vector<Resource>&		resource,
										 const vector<deInt16>&	data);
};

SpvAsmTypeInt16Tests::SpvAsmTypeInt16Tests	(tcu::TestContext&	testCtx,
											 deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "i16", "int16 tests", "shaderInt16", "Int16", "OpTypeInt 16 1", TYPE_I16, 16, vectorSize)
{
	m_cases[0] = -3221;
	m_cases[1] = 3210;
	m_cases[2] = 19597;
}

SpvAsmTypeInt16Tests::~SpvAsmTypeInt16Tests (void)
{
}

void SpvAsmTypeInt16Tests::getDataset (vector<deInt16>&	input,
									   deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);
	input.push_back(static_cast<deInt16>(deIntMinValue32(16)));// A 16-bit negative number
	input.push_back(static_cast<deInt16>(deIntMaxValue32(16)));// A 16-bit positive number

	// Push switch cases
	input.push_back(m_cases[0]);
	input.push_back(m_cases[1]);
	input.push_back(m_cases[2]);

	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(static_cast<deInt16>(m_rnd.getUint16()));
}

void SpvAsmTypeInt16Tests::pushResource (vector<Resource>&		resource,
										 const vector<deInt16>&	data)
{
	resource.push_back(Resource(BufferSp(new Int16Buffer(data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

class SpvAsmTypeInt32Tests : public SpvAsmTypeTests<deInt32>
{
public:
				SpvAsmTypeInt32Tests	(tcu::TestContext&		testCtx,
										 deUint32				vectorSize);
				~SpvAsmTypeInt32Tests	(void);
	void		getDataset				(vector<deInt32>&		input,
										 deUint32				numElements);
	void		pushResource			(vector<Resource>&		resource,
										 const vector<deInt32>&	data);
};

SpvAsmTypeInt32Tests::SpvAsmTypeInt32Tests (tcu::TestContext&	testCtx,
											deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "i32", "int32 tests", DE_NULL, DE_NULL, "OpTypeInt 32 1", TYPE_I32, 32, vectorSize)
{
	m_cases[0] = -3221;
	m_cases[1] = 3210;
	m_cases[2] = 268438669;
}

SpvAsmTypeInt32Tests::~SpvAsmTypeInt32Tests (void)
{
}

void SpvAsmTypeInt32Tests::getDataset (vector<deInt32>&	input,
									   deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);
	input.push_back(deIntMinValue32(32) + 1); // So MIN = -MAX
	input.push_back(deIntMaxValue32(32));

	// Push switch cases
	input.push_back(m_cases[0]);
	input.push_back(m_cases[1]);
	input.push_back(m_cases[2]);

	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(static_cast<deInt32>(m_rnd.getUint32()));
}

void SpvAsmTypeInt32Tests::pushResource (vector<Resource>&		resource,
										 const vector<deInt32>&	data)
{
	resource.push_back(Resource(BufferSp(new Int32Buffer(data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

class SpvAsmTypeInt64Tests : public SpvAsmTypeTests<deInt64>
{
public:
				SpvAsmTypeInt64Tests	(tcu::TestContext&		testCtx,
										 deUint32				vectorSize);
				~SpvAsmTypeInt64Tests	(void);
	void		getDataset				(vector<deInt64>&		input,
										 deUint32				numElements);
	void		pushResource			(vector<Resource>&		resource,
										 const vector<deInt64>&	data);
};

SpvAsmTypeInt64Tests::SpvAsmTypeInt64Tests (tcu::TestContext&	testCtx,
											deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "i64", "int64 tests", "shaderInt64", "Int64", "OpTypeInt 64 1", TYPE_I64, 64, vectorSize)
{
	m_cases[0] = 3210;
	m_cases[1] = -268438669;
	m_cases[2] = 26843866939192872;
}

SpvAsmTypeInt64Tests::~SpvAsmTypeInt64Tests (void)
{
}

void SpvAsmTypeInt64Tests::getDataset (vector<deInt64>&	input,
									   deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);
	input.push_back(0xFFFF859A3BF78592);// A 64-bit negative number
	input.push_back(0x7FFF859A3BF78592);// A 64-bit positive number

	// Push switch cases
	input.push_back(m_cases[0]);
	input.push_back(m_cases[1]);
	input.push_back(m_cases[2]);

	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(static_cast<deInt64>(m_rnd.getUint64()));
}

void SpvAsmTypeInt64Tests::pushResource	(vector<Resource>&		resource,
										 const vector<deInt64>&	data)
{
	resource.push_back(Resource(BufferSp(new Int64Buffer(data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

class SpvAsmTypeUint8Tests : public SpvAsmTypeTests<deUint8>
{
public:
				SpvAsmTypeUint8Tests	(tcu::TestContext&		testCtx,
										 deUint32				vectorSize);
				~SpvAsmTypeUint8Tests	(void);
	void		getDataset				(vector<deUint8>&		input,
										 deUint32				numElements);
	void		pushResource			(vector<Resource>&		resource,
										 const vector<deUint8>&	data);
};

SpvAsmTypeUint8Tests::SpvAsmTypeUint8Tests	(tcu::TestContext&	testCtx,
											 deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "u8", "uint8 tests", DE_NULL, "Int8", "OpTypeInt 8 0", TYPE_U8, 8, vectorSize)
{
	m_cases[0] = 0;
	m_cases[1] = 73;
	m_cases[2] = 193;
}

SpvAsmTypeUint8Tests::~SpvAsmTypeUint8Tests (void)
{
}

void SpvAsmTypeUint8Tests::getDataset (vector<deUint8>&	input,
									   deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);  // Min value
	input.push_back(~0); // Max value

	//Push switch cases
	input.push_back(m_cases[0]);
	input.push_back(m_cases[1]);
	input.push_back(m_cases[2]);

	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(m_rnd.getUint8());
}

void SpvAsmTypeUint8Tests::pushResource (vector<Resource>&		resource,
										 const vector<deUint8>&	data)
{
	resource.push_back(Resource(BufferSp(new Uint8Buffer(data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

class SpvAsmTypeUint16Tests : public SpvAsmTypeTests<deUint16>
{
public:
				SpvAsmTypeUint16Tests	(tcu::TestContext&			testCtx,
										 deUint32					vectorSize);
				~SpvAsmTypeUint16Tests	(void);
	void		getDataset				(vector<deUint16>&			input,
										 deUint32					numElements);
	void		pushResource			(vector<Resource>&			resource,
										 const vector<deUint16>&	data);
};

SpvAsmTypeUint16Tests::SpvAsmTypeUint16Tests	(tcu::TestContext&	testCtx,
												 deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "u16", "uint16 tests", "shaderInt16", "Int16", "OpTypeInt 16 0", TYPE_U16, 16, vectorSize)
{
	m_cases[0] = 0;
	m_cases[1] = 3210;
	m_cases[2] = 19597;
}

SpvAsmTypeUint16Tests::~SpvAsmTypeUint16Tests (void)
{
}

void SpvAsmTypeUint16Tests::getDataset (vector<deUint16>&	input,
										deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);  // Min value
	input.push_back(~0); // Max value

	//Push switch cases
	input.push_back(m_cases[0]);
	input.push_back(m_cases[1]);
	input.push_back(m_cases[2]);

	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(m_rnd.getUint16());
}

void SpvAsmTypeUint16Tests::pushResource (vector<Resource>&			resource,
										  const vector<deUint16>&	data)
{
	resource.push_back(Resource(BufferSp(new Uint16Buffer(data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

class SpvAsmTypeUint32Tests : public SpvAsmTypeTests<deUint32>
{
public:
				SpvAsmTypeUint32Tests	(tcu::TestContext&			testCtx,
										 deUint32					vectorSize);
				~SpvAsmTypeUint32Tests	(void);
	void		getDataset				(vector<deUint32>&			input,
										 deUint32					numElements);
	void		pushResource			(vector<Resource>&			resource,
										 const vector<deUint32>&	data);
};

SpvAsmTypeUint32Tests::SpvAsmTypeUint32Tests (tcu::TestContext&	testCtx,
											  deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "u32", "uint32 tests", DE_NULL, DE_NULL, "OpTypeInt 32 0", TYPE_U32, 32, vectorSize)
{
	m_cases[0] = 0;
	m_cases[1] = 3210;
	m_cases[2] = 268438669;
}

SpvAsmTypeUint32Tests::~SpvAsmTypeUint32Tests (void)
{
}

void SpvAsmTypeUint32Tests::getDataset (vector<deUint32>&	input,
										deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);  // Min value
	input.push_back(~0); // Max value

	// Push switch cases
	input.push_back(m_cases[0]);
	input.push_back(m_cases[1]);
	input.push_back(m_cases[2]);

	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(m_rnd.getUint32());
}

void SpvAsmTypeUint32Tests::pushResource (vector<Resource>&			resource,
										  const vector<deUint32>&	data)
{
	resource.push_back(Resource(BufferSp(new Uint32Buffer(data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

class SpvAsmTypeUint64Tests : public SpvAsmTypeTests<deUint64>
{
public:
				SpvAsmTypeUint64Tests	(tcu::TestContext&			testCtx,
										 deUint32					vectorSize);
				~SpvAsmTypeUint64Tests	(void);
	void		getDataset				(vector<deUint64>&			input,
										 deUint32					numElements);
	void		pushResource			(vector<Resource>&			resource,
										 const vector<deUint64>&	data);
};

SpvAsmTypeUint64Tests::SpvAsmTypeUint64Tests (tcu::TestContext&	testCtx,
											  deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "u64", "uint64 tests", "shaderInt64", "Int64", "OpTypeInt 64 0", TYPE_U64, 64, vectorSize)
{
	m_cases[0] = 3210;
	m_cases[1] = 268438669;
	m_cases[2] = 26843866939192872;
}

SpvAsmTypeUint64Tests::~SpvAsmTypeUint64Tests (void)
{
}

void SpvAsmTypeUint64Tests::getDataset (vector<deUint64>&	input,
										deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);  // Min value
	input.push_back(~0); // Max value

	// Push switch cases
	input.push_back(m_cases[0]);
	input.push_back(m_cases[1]);
	input.push_back(m_cases[2]);

	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(m_rnd.getUint64());
}

void SpvAsmTypeUint64Tests::pushResource (vector<Resource>&			resource,
										  const vector<deUint64>&	data)
{
	resource.push_back(Resource(BufferSp(new Uint64Buffer(data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
}

template <class T>
class TestMath
{
public:
	static inline T test_abs (T x)
	{
		T t0 = static_cast<T>(0.0);

		if (x >= t0)
			return x;
		else
			return test_negate(x);
	}

	static inline T test_add (T x, T y)
	{
		return static_cast<T>(x + y);
	}

	static inline T test_clamp (T x, T minVal, T maxVal)
	{
		return test_min(test_max(x, minVal), maxVal);
	}

	static inline T test_div (T x, T y)
	{
		// In SPIR-V, if "y" is 0, then the result is undefined. In our case,
		// let's return 0
		if (y == static_cast<T>(0))
			return 0;
		else
			return static_cast<T>(x / y);
	}

	static inline T test_lsb (T x)
	{
		for (deUint32 i = 0; i < 8 * sizeof(T); i++)
		{
			if (x & (1u << i))
				return static_cast<T>(i);
		}

		return static_cast<T>(-1.0);
	}

	static inline T test_max (T x, T y)
	{
		if (x < y)
			return y;
		else
			return x;
	}

	static inline T test_min (T x, T y)
	{
		if (y < x)
			return y;
		else
			return x;
	}

	static inline T test_mod (T x, T y)
	{
		T sign_x, sign_y;

		// In SPIR-V, if "y" is 0, then the result is undefined. In our case,
		// let's return 0
		if (y == static_cast<T>(0))
			return 0;

		if (x >= static_cast<T>(0))
			sign_x = 1;
		else
			sign_x = -1;

		if (y >= static_cast<T>(0))
			sign_y = 1;
		else
			sign_y = -1;

		return static_cast<T>(static_cast<T>(static_cast<T>(x) - static_cast<T>(y * static_cast<T>(x / y))) * static_cast<T>(sign_y * sign_x));
	}

	static inline T test_mul (T x, T y)
	{
		return static_cast<T>(x * y);
	}

	static inline T test_negate (T x)
	{
		return static_cast<T>(static_cast<T>(0.0) - static_cast<T>(x));
	}

	static inline T test_rem (T x, T y)
	{
		// In SPIR-V, if "y" is 0, then the result is undefined. In our case,
		// let's return 0
		if (y == static_cast<T>(0))
			return 0;

		return static_cast<T>(x % y);
	}

	static inline T test_sign (T x)
	{
		T t0 = static_cast<T>(0.0);

		if (x > t0)
			return static_cast<T>(1.0);
		else if (x < t0)
			return static_cast<T>(-1.0);
		else
			return t0;
	}

	static inline T test_sub (T x, T y)
	{
		return static_cast<T>(x - y);
	}

	static inline T test_msb (T)
	{
		TCU_THROW(InternalError, "Not implemented");
	}

	static inline T test_lsr (T x, T y)
	{
		if (x >= static_cast<T>(0) || y == static_cast<T>(0))
		{
			return static_cast<T>(x >> y);
		}
		else
		{
			const T	mask = de::leftZeroMask(y);
			return static_cast<T>((x >> y) & mask);
		}
	}

	static inline T test_asr (T x, T y)
	{
		const T bitmask = static_cast<T>(deUint64(1) << (sizeof(T) * 8u - 1u));

		if ((x & bitmask) && y > 0)
		{
			const T	mask	= de::leftSetMask(y);
			const T	result	= static_cast<T>((x >> y) | mask);
			return result;
		}
		else
		{
			return static_cast<T>(x >> y);
		}
	}

	static inline T test_lsl (T x, T y)
	{
		return static_cast<T>(x << y);
	}

	static inline T test_bitwise_or (T x, T y)
	{
		return static_cast<T>(x | y);
	}

	static inline T test_bitwise_xor (T x, T y)
	{
		return static_cast<T>(x ^ y);
	}

	static inline T test_bitwise_and (T x, T y)
	{
		return static_cast<T>(x & y);
	}

	static inline T test_not (T x)
	{
		return static_cast<T>(~x);
	}

	static inline T test_iequal (T x, T y)
	{
		if (x == y)
			return static_cast<T>(1);
		else
			return static_cast<T>(0);
	}

	static inline T test_inotequal (T x, T y)
	{
		if (x != y)
			return static_cast<T>(1);
		else
			return static_cast<T>(0);
	}

	static inline T test_ugreaterthan (T x, T y)
	{
		if (x > y)
			return static_cast<T>(1);
		else
			return static_cast<T>(0);
	}

	static inline T test_ulessthan (T x, T y)
	{
		return test_ugreaterthan(y, x);
	}

	static inline T test_sgreaterthan (T x, T y)
	{
		if (x > y)
			return static_cast<T>(1);
		else
			return static_cast<T>(0);
	}

	static inline T test_slessthan (T x, T y)
	{
		return test_sgreaterthan(y, x);
	}

	static inline T test_ugreaterthanequal (T x, T y)
	{
		if (x >= y)
			return static_cast<T>(1);
		else
			return static_cast<T>(0);
	}

	static inline T test_ulessthanequal (T x, T y)
	{
		return test_ugreaterthanequal(y, x);
	}

	static inline T test_sgreaterthanequal (T x, T y)
	{
		if (x >= y)
			return static_cast<T>(1);
		else
			return static_cast<T>(0);
	}

	static inline T test_slessthanequal (T x, T y)
	{
		return test_sgreaterthanequal(y, x);
	}

	static inline T test_bitFieldInsert (T base, T insert, T offset, T count)
	{
		const T	insertMask	= de::rightSetMask(count);

		return static_cast<T>((base & ~(insertMask << offset)) | ((insert & insertMask) << offset));
	}

	static inline T test_bitFieldSExtract (T x, T y, T z)
	{
		const T allZeros	= static_cast<T>(0);

		// Count can be 0, in which case the result will be 0
		if (z == allZeros)
			return allZeros;

		const T extractMask	= de::rightSetMask(z);
		const T signBit		= static_cast<T>(x & (1 << (y + z - 1)));
		const T signMask	= static_cast<T>(signBit ? ~extractMask : allZeros);

		return static_cast<T>((signMask & ~extractMask) | ((x >> y) & extractMask));
	}

	static inline T test_bitFieldUExtract (T x, T y, T z)
	{
		const T allZeros	= (static_cast<T>(0));

		// Count can be 0, in which case the result will be 0
		if (z == allZeros)
			return allZeros;

		const T extractMask	= de::rightSetMask(z);

		return static_cast<T>((x >> y) & extractMask);
	}

	static inline T test_bitReverse (T x)
	{
		T		base	= x;
		T		result	= static_cast<T>(0);

		for (size_t bitNdx = 0u; bitNdx < sizeof(T) * 8u; bitNdx++)
		{
			result = static_cast<T>(result << 1) | (base & 1);
			base >>= 1;
		}

		return result;
	}

	static inline T test_bitCount (T x)
	{
		T count	= static_cast<T>(0);

		for (deUint32 bitNdx = 0u; bitNdx < (deUint32)sizeof(T) * 8u; bitNdx++)
			if (x & (static_cast<T>(1) << bitNdx))
				count++;

		return count;
	}

	static inline T test_constant (T a)
	{
		return a;
	}
};

class TestMathInt8 : public TestMath<deInt8>
{
public:
	static inline deInt8 test_msb (deInt8 x)
	{
		if (x > 0)
			return static_cast<deInt8>(7 - deClz32((deUint32)x));
		else if (x < 0)
			return static_cast<deInt8>(7 - deClz32(~(deUint32)x));
		else
			return -1;
	}

	static inline deInt8 test_mul_div (deInt8 x, deInt8 y)
	{
		deInt32	x32	= static_cast<deInt32>(x);
		deInt32	y32	= static_cast<deInt32>(y);

		// In SPIR-V, if "y" is 0, then the result is undefined. In our case, let's return 0
		if (y == static_cast<deInt8>(0))
			return 0;
		else
			return static_cast<deInt8>(static_cast<deInt8>(x32 * y32) / y32);
	}

	static inline deInt8 test_ugreaterthan (deInt8 x, deInt8 y)
	{
		// Consume signed integers as unsigned integers
		if ((x & 0x80) ^ (y & 0x80))
			std::swap(x,y);

		if (x > y)
			return static_cast<deInt8>(1);
		else
			return static_cast<deInt8>(0);
	}

	static inline deInt8 test_ulessthan (deInt8 x, deInt8 y)
	{
		return test_ugreaterthan(y, x);
	}

	static inline deInt8 test_ugreaterthanequal (deInt8 x, deInt8 y)
	{
		// Consume signed integers as unsigned integers
		if ((x & 0x80) ^ (y & 0x80))
			std::swap(x,y);

		if (x >= y)
			return static_cast<deInt8>(1);
		else
			return static_cast<deInt8>(0);
	}

	static inline deInt8 test_ulessthanequal (deInt8 x, deInt8 y)
	{
		return test_ugreaterthanequal(y, x);
	}
};

class TestMathInt16 : public TestMath<deInt16>
{
public:
	static inline deInt16 test_msb (deInt16 x)
	{
		if (x > 0)
			return static_cast<deInt16>(15 - deClz32((deUint32)x));
		else if (x < 0)
			return static_cast<deInt16>(15 - deClz32(~(deUint32)x));
		else
			return -1;
	}

	static inline deInt16 test_mul_div (deInt16 x, deInt16 y)
	{
		deInt32	x32	= static_cast<deInt32>(x);
		deInt32	y32	= static_cast<deInt32>(y);

		// In SPIR-V, if "y" is 0, then the result is undefined. In our case, let's return 0
		if (y == static_cast<deInt16>(0))
			return 0;
		else
			return static_cast<deInt16>(static_cast<deInt16>(x32 * y32) / y32);
	}

	static inline deInt16 test_ugreaterthan (deInt16 x, deInt16 y)
	{
		// Consume signed integers as unsigned integers
		if ((x & 0x8000) ^ (y & 0x8000))
			std::swap(x,y);

		if (x > y)
			return static_cast<deInt16>(1);
		else
			return static_cast<deInt16>(0);
	}

	static inline deInt16 test_ulessthan (deInt16 x, deInt16 y)
	{
		return test_ugreaterthan(y, x);
	}

	static inline deInt16 test_ugreaterthanequal (deInt16 x, deInt16 y)
	{
		// Consume signed integers as unsigned integers
		if ((x & 0x8000) ^ (y & 0x8000))
			std::swap(x,y);

		if (x >= y)
			return static_cast<deInt16>(1);
		else
			return static_cast<deInt16>(0);
	}

	static inline deInt16 test_ulessthanequal (deInt16 x, deInt16 y)
	{
		return test_ugreaterthanequal(y, x);
	}
};

class TestMathInt32 : public TestMath<deInt32>
{
public:
	static inline deInt32 test_msb (deInt32 x)
	{
		if (x > 0)
			return 31 - deClz32((deUint32)x);
		else if (x < 0)
			return 31 - deClz32(~(deUint32)x);
		else
			return -1;
	}

	static inline deInt32 test_ugreaterthan (deInt32 x, deInt32 y)
	{
		// Consume signed integers as unsigned integers
		if ((x & 0x80000000) ^ (y & 0x80000000))
			std::swap(x,y);

		if (x > y)
			return static_cast<deInt32>(1);
		else
			return static_cast<deInt32>(0);
	}

	static inline deInt32 test_ulessthan (deInt32 x, deInt32 y)
	{
		return test_ugreaterthan(y, x);
	}

	static inline deInt32 test_ugreaterthanequal (deInt32 x, deInt32 y)
	{
		// Consume signed integers as unsigned integers
		if ((x & 0x80000000) ^ (y & 0x80000000))
			std::swap(x,y);

		if (x >= y)
			return static_cast<deInt32>(1);
		else
			return static_cast<deInt32>(0);
	}

	static inline deInt32 test_ulessthanequal (deInt32 x, deInt32 y)
	{
		return test_ugreaterthanequal(y, x);
	}
};

class TestMathInt64 : public TestMath<deInt64>
{
public:
	static inline deInt64 test_ugreaterthan (deInt64 x, deInt64 y)
	{
		// Consume signed integers as unsigned integers
		if ((x & 0x8000000000000000) ^ (y & 0x8000000000000000))
			std::swap(x,y);

		if (x > y)
			return static_cast<deInt64>(1);
		else
			return static_cast<deInt64>(0);
	}

	static inline deInt64 test_ulessthan (deInt64 x, deInt64 y)
	{
		return test_ugreaterthan(y, x);
	}

	static inline deInt64 test_ugreaterthanequal (deInt64 x, deInt64 y)
	{
		// Consume signed integers as unsigned integers
		if ((x & 0x8000000000000000) ^ (y & 0x8000000000000000))
			std::swap(x,y);

		if (x >= y)
			return static_cast<deInt64>(1);
		else
			return static_cast<deInt64>(0);
	}

	static inline deInt64 test_ulessthanequal (deInt64 x, deInt64 y)
	{
		return test_ugreaterthanequal(y, x);
	}
};

class TestMathUint8 : public TestMath<deUint8>
{
public:
	static inline deUint32 test_msb (deUint8 x)
	{
		if (x > 0)
			return 7 - deClz32((deUint32)x);
		else
			return -1;
	}

	static inline deUint8 test_mul_div (deUint8 x, deUint8 y)
	{
		const deUint32 x32 = static_cast<deUint32>(x);
		const deUint32 y32 = static_cast<deUint32>(y);

		// In SPIR-V, if "y" is 0, then the result is undefined. In our case, let's return 0
		if (y == static_cast<deUint8>(0))
			return 0;
		else
			return static_cast<deUint8>(static_cast<deUint8>(x32 * y32) / y32);
	}

	static inline deUint8 test_sgreaterthan (deUint8 x, deUint8 y)
	{
		// Consume unsigned integers as signed integers
		if ((x & 0x80) ^ (y & 0x80))
			std::swap(x,y);

		if (x > y)
			return static_cast<deUint8>(1);
		else
			return static_cast<deUint8>(0);
	}

	static inline deUint8 test_slessthan (deUint8 x, deUint8 y)
	{
		return test_sgreaterthan(y, x);
	}

	static inline deUint8 test_sgreaterthanequal (deUint8 x, deUint8 y)
	{
		// Consume unsigned integers as signed integers
		if ((x & 0x80) ^ (y & 0x80))
			std::swap(x,y);

		if (x >= y)
			return static_cast<deUint8>(1);
		else
			return static_cast<deUint8>(0);
	}

	static inline deUint8 test_slessthanequal (deUint8 x, deUint8 y)
	{
		return test_sgreaterthanequal(y, x);
	}
};

class TestMathUint16 : public TestMath<deUint16>
{
public:
	static inline deUint32 test_msb (deUint16 x)
	{
		if (x > 0)
			return 15 - deClz32((deUint32)x);
		else
			return -1;
	}

	static inline deUint16 test_mul_div (deUint16 x, deUint16 y)
	{
		const deUint32 x32 = static_cast<deUint32>(x);
		const deUint32 y32 = static_cast<deUint32>(y);

		// In SPIR-V, if "y" is 0, then the result is undefined. In our case, let's return 0
		if (y == static_cast<deUint16>(0))
			return 0;
		else
			return static_cast<deUint16>(static_cast<deUint16>(x32 * y32) / y32);
	}

	static inline deUint16 test_sgreaterthan (deUint16 x, deUint16 y)
	{
		// Consume unsigned integers as signed integers
		if ((x & 0x8000) ^ (y & 0x8000))
			std::swap(x,y);

		if (x > y)
			return static_cast<deUint16>(1);
		else
			return static_cast<deUint16>(0);
	}

	static inline deUint16 test_slessthan (deUint16 x, deUint16 y)
	{
		return test_sgreaterthan(y, x);
	}

	static inline deUint16 test_sgreaterthanequal (deUint16 x, deUint16 y)
	{
		// Consume unsigned integers as signed integers
		if ((x & 0x8000) ^ (y & 0x8000))
			std::swap(x,y);

		if (x >= y)
			return static_cast<deUint16>(1);
		else
			return static_cast<deUint16>(0);
	}

	static inline deUint16 test_slessthanequal (deUint16 x, deUint16 y)
	{
		return test_sgreaterthanequal(y, x);
	}
};

class TestMathUint32 : public TestMath<deUint32>
{
public:
	static inline deUint32 test_msb (deUint32 x)
	{
		if (x > 0)
			return 31 - deClz32(x);
		else
			return -1;
	}

	static inline deUint32 test_sgreaterthan (deUint32 x, deUint32 y)
	{
		// Consume unsigned integers as signed integers
		if ((x & 0x80000000) ^ (y & 0x80000000))
			std::swap(x,y);

		if (x > y)
			return static_cast<deUint32>(1);
		else
			return static_cast<deUint32>(0);
	}

	static inline deUint32 test_slessthan (deUint32 x, deUint32 y)
	{
		return test_sgreaterthan(y, x);
	}

	static inline deUint32 test_sgreaterthanequal (deUint32 x, deUint32 y)
	{
		// Consume unsigned integers as signed integers
		if ((x & 0x80000000) ^ (y & 0x80000000))
			std::swap(x,y);

		if (x >= y)
			return static_cast<deUint32>(1);
		else
			return static_cast<deUint32>(0);
	}

	static inline deUint32 test_slessthanequal (deUint32 x, deUint32 y)
	{
		return test_sgreaterthanequal(y, x);
	}

};

class TestMathUint64 : public TestMath<deUint64>
{
public:
	static inline deUint64 test_sgreaterthan (deUint64 x, deUint64 y)
	{
		// Consume unsigned integers as signed integers
		if ((x & 0x8000000000000000) ^ (y & 0x8000000000000000))
			std::swap(x,y);

		if (x > y)
			return static_cast<deUint64>(1);
		else
			return static_cast<deUint64>(0);
	}

	static inline deUint64 test_slessthan (deUint64 x, deUint64 y)
	{
		return test_sgreaterthan(y, x);
	}

	static inline deUint64 test_sgreaterthanequal (deUint64 x, deUint64 y)
	{
		// Consume unsigned integers as signed integers
		if ((x & 0x8000000000000000) ^ (y & 0x8000000000000000))
			std::swap(x,y);

		if (x >= y)
			return static_cast<deUint64>(1);
		else
			return static_cast<deUint64>(0);
	}

	static inline deUint64 test_slessthanequal (deUint64 x, deUint64 y)
	{
		return test_sgreaterthanequal(y, x);
	}
};

#define I8_FILTER_NONE SpvAsmTypeInt8Tests::filterNone
#define I16_FILTER_NONE SpvAsmTypeInt16Tests::filterNone
#define I32_FILTER_NONE SpvAsmTypeInt32Tests::filterNone
#define I64_FILTER_NONE SpvAsmTypeInt64Tests::filterNone
#define U8_FILTER_NONE SpvAsmTypeUint8Tests::filterNone
#define U16_FILTER_NONE SpvAsmTypeUint16Tests::filterNone
#define U32_FILTER_NONE SpvAsmTypeUint32Tests::filterNone
#define U64_FILTER_NONE SpvAsmTypeUint64Tests::filterNone

#define I8_FILTER_ZERO SpvAsmTypeInt8Tests::filterZero
#define I16_FILTER_ZERO SpvAsmTypeInt16Tests::filterZero
#define I32_FILTER_ZERO SpvAsmTypeInt32Tests::filterZero
#define I64_FILTER_ZERO SpvAsmTypeInt64Tests::filterZero
#define U8_FILTER_ZERO SpvAsmTypeUint8Tests::filterZero
#define U16_FILTER_ZERO SpvAsmTypeUint16Tests::filterZero
#define U32_FILTER_ZERO SpvAsmTypeUint32Tests::filterZero
#define U64_FILTER_ZERO SpvAsmTypeUint64Tests::filterZero

#define I8_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeInt8Tests::filterNegativesAndZero
#define I16_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeInt16Tests::filterNegativesAndZero
#define I32_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeInt32Tests::filterNegativesAndZero
#define I64_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeInt64Tests::filterNegativesAndZero
#define U8_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeUint8Tests::filterNegativesAndZero
#define U16_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeUint16Tests::filterNegativesAndZero
#define U32_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeUint32Tests::filterNegativesAndZero
#define U64_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeUint64Tests::filterNegativesAndZero

#define I8_FILTER_MIN_GT_MAX SpvAsmTypeInt8Tests::filterMinGtMax
#define I16_FILTER_MIN_GT_MAX SpvAsmTypeInt16Tests::filterMinGtMax
#define I32_FILTER_MIN_GT_MAX SpvAsmTypeInt32Tests::filterMinGtMax
#define I64_FILTER_MIN_GT_MAX SpvAsmTypeInt64Tests::filterMinGtMax
#define U8_FILTER_MIN_GT_MAX SpvAsmTypeUint8Tests::filterMinGtMax
#define U16_FILTER_MIN_GT_MAX SpvAsmTypeUint16Tests::filterMinGtMax
#define U32_FILTER_MIN_GT_MAX SpvAsmTypeUint32Tests::filterMinGtMax
#define U64_FILTER_MIN_GT_MAX SpvAsmTypeUint64Tests::filterMinGtMax

const string bitShiftTestPostfix[] =
{
	"_shift8",
	"_shift16",
	"_shift32",
	"_shift64"
};

const string bitFieldTestPostfix[] =
{
	"_offset8_count8",
	"_offset8_count16",
	"_offset8_count32",
	"_offset8_count64",
	"_offset16_count8",
	"_offset16_count16",
	"_offset16_count32",
	"_offset16_count64",
	"_offset32_count8",
	"_offset32_count16",
	"_offset32_count32",
	"_offset32_count64",
	"_offset64_count8",
	"_offset64_count16",
	"_offset64_count32",
	"_offset64_count64",
};

// Macro to create tests.
// Syntax: MAKE_TEST_{S,V}_{I,U}_{8,1,3,6}
//
//  'S': create scalar test
//  'V': create vector test
//
//  'I': create integer test
//  'U': create unsigned integer test
//
//  '8': create 8-bit test
//  '1': create 16-bit test
//  '3': create 32-bit test
//  '6': create 64-bit test
//
//  'W': bit width of some parameters in bit field and shift operations can be different from Result and Base
//  'N': create 16-bit tests without 'test_high_part_zero' variants

#define MAKE_TEST_S_I_8136(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 1; ++ndx) \
	{ \
		int8Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt8::test_##op, I8_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int16Tests[ndx]->createTests((name "_test_high_part_zero"), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension), true); \
		int32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt64::test_##op, I64_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
	} \

#define MAKE_TEST_V_I_8136(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 1; ndx < 4; ++ndx) \
	{ \
		int8Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt8::test_##op, I8_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int16Tests[ndx]->createTests((name "_test_high_part_zero"), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension), true); \
		int32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt64::test_##op, I64_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
	} \

#define MAKE_TEST_SV_I_8136(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	{ \
		int8Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt8::test_##op, I8_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int16Tests[ndx]->createTests((name "_test_high_part_zero"), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension), true); \
		int32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt64::test_##op, I64_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
	} \

#define MAKE_TEST_SV_I_8136_N(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	{ \
		int8Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt8::test_##op, I8_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt64::test_##op, I64_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
	} \

#define MAKE_TEST_SV_I_8136_W(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	for (deUint32 widthNdx = 0; widthNdx < DE_LENGTH_OF_ARRAY(bitShiftTestPostfix); ++widthNdx) \
	{ \
		const InputWidth inputWidth = static_cast<InputWidth>(WIDTH_8 + widthNdx); \
		\
		int8Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx]).c_str(), (spirvOp), \
			TestMathInt8::test_##op, I8_##filter, inputRange, inputWidth, (extension)); \
		int16Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx]).c_str(), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, inputWidth, (extension)); \
		int16Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx] + "_test_high_part_zero").c_str(), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, inputWidth, (extension), true); \
		int32Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx]).c_str(), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, inputRange, inputWidth, (extension)); \
		int64Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx]).c_str(), (spirvOp), \
			TestMathInt64::test_##op, I64_##filter, inputRange, inputWidth, (extension)); \
	} \

#define MAKE_TEST_SV_I_1(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	{ \
		int16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		int16Tests[ndx]->createTests((name "_test_high_part_zero"), (spirvOp), \
			TestMathInt16::test_##op, I16_##filter, inputRange, WIDTH_DEFAULT, (extension), true); \
	} \

#define MAKE_TEST_SV_I_3(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
		int32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \

#define MAKE_TEST_SV_I_3_W(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	for (deUint32 width = 0; width < DE_LENGTH_OF_ARRAY(bitFieldTestPostfix); ++width) \
	{ \
		int32Tests[ndx]->createTests(string(name + bitFieldTestPostfix[width]).c_str(), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, inputRange, InputWidth(WIDTH_8_8 + width), (extension)); \
	} \

#define MAKE_TEST_S_U_8136(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 1; ++ndx) \
	{ \
		uint8Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint8::test_##op, U8_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint16Tests[ndx]->createTests((name "_test_high_part_zero"), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension), true); \
		uint32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint64::test_##op, U64_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
	} \

#define MAKE_TEST_V_U_8136(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 1; ndx < 4; ++ndx) \
	{ \
		uint16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint16Tests[ndx]->createTests((name "_test_high_part_zero"), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension), true); \
		uint32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint64::test_##op, U64_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
	} \

#define MAKE_TEST_SV_U_8136(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	{ \
		uint8Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint8::test_##op, U8_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint16Tests[ndx]->createTests((name "_test_high_part_zero"), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension), true); \
		uint32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint64::test_##op, U64_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
	} \

#define MAKE_TEST_SV_U_8136_N(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	{ \
		uint8Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint8::test_##op, U8_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint64::test_##op, U64_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
	} \

#define MAKE_TEST_SV_U_8136_W(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	for (deUint32 widthNdx = 0; widthNdx < DE_LENGTH_OF_ARRAY(bitShiftTestPostfix); ++widthNdx) \
	{ \
		const InputWidth inputWidth = static_cast<InputWidth>(WIDTH_8 + widthNdx); \
		\
		uint8Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx]).c_str(), (spirvOp), \
			TestMathUint8::test_##op, U8_##filter, inputRange, inputWidth, (extension)); \
		uint16Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx]).c_str(), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, inputWidth, (extension)); \
		uint16Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx] + "_test_high_part_zero").c_str(), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, inputWidth, (extension), true); \
		uint32Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx]).c_str(), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, inputRange, inputWidth, (extension)); \
		uint64Tests[ndx]->createTests(string(name + bitShiftTestPostfix[widthNdx]).c_str(), (spirvOp), \
			TestMathUint64::test_##op, U64_##filter, inputRange, inputWidth, (extension)); \
	} \

#define MAKE_TEST_SV_U_1(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	{ \
		uint16Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension)); \
		uint16Tests[ndx]->createTests((name "_test_high_part_zero"), (spirvOp), \
			TestMathUint16::test_##op, U16_##filter, inputRange, WIDTH_DEFAULT, (extension), true); \
	} \

#define MAKE_TEST_SV_U_3(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
		uint32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, inputRange, WIDTH_DEFAULT, (extension)); \

#define MAKE_TEST_SV_U_3_W(name, spirvOp, op, filter, inputRange, extension) \
	for (deUint32 ndx = 0; ndx < 4; ++ndx) \
	for (deUint32 width = 0; width < DE_LENGTH_OF_ARRAY(bitFieldTestPostfix); ++width) \
	{ \
		uint32Tests[ndx]->createTests(string(name + bitFieldTestPostfix[width]).c_str(), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, inputRange, InputWidth(WIDTH_8_8 + width), (extension)); \
	} \

tcu::TestCaseGroup* createTypeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		typeTests			(new tcu::TestCaseGroup(testCtx, "type", "Test types"));
	de::MovePtr<tcu::TestCaseGroup>		typeScalarTests		(new tcu::TestCaseGroup(testCtx, "scalar", "scalar tests"));
	de::MovePtr<tcu::TestCaseGroup>		typeVectorTests[3];

	de::MovePtr<SpvAsmTypeInt8Tests>	int8Tests[4];
	de::MovePtr<SpvAsmTypeInt16Tests>	int16Tests[4];
	de::MovePtr<SpvAsmTypeInt32Tests>	int32Tests[4];
	de::MovePtr<SpvAsmTypeInt64Tests>	int64Tests[4];
	de::MovePtr<SpvAsmTypeUint8Tests>	uint8Tests[4];
	de::MovePtr<SpvAsmTypeUint16Tests>	uint16Tests[4];
	de::MovePtr<SpvAsmTypeUint32Tests>	uint32Tests[4];
	de::MovePtr<SpvAsmTypeUint64Tests>	uint64Tests[4];

	for (deUint32 ndx = 0; ndx < 3; ++ndx)
	{
		std::string testName = "vec" + de::toString(ndx + 2);
		typeVectorTests[ndx] = de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, testName.c_str(), "vector tests"));
	}

	for (deUint32 ndx = 0; ndx < 4; ++ndx)
	{
		int8Tests[ndx]		= de::MovePtr<SpvAsmTypeInt8Tests>(new SpvAsmTypeInt8Tests(testCtx, ndx + 1));
		int16Tests[ndx]		= de::MovePtr<SpvAsmTypeInt16Tests>(new SpvAsmTypeInt16Tests(testCtx, ndx + 1));
		int32Tests[ndx]		= de::MovePtr<SpvAsmTypeInt32Tests>(new SpvAsmTypeInt32Tests(testCtx, ndx + 1));
		int64Tests[ndx]		= de::MovePtr<SpvAsmTypeInt64Tests>(new SpvAsmTypeInt64Tests(testCtx, ndx + 1));
		uint8Tests[ndx]		= de::MovePtr<SpvAsmTypeUint8Tests>(new SpvAsmTypeUint8Tests(testCtx, ndx + 1));
		uint16Tests[ndx]	= de::MovePtr<SpvAsmTypeUint16Tests>(new SpvAsmTypeUint16Tests(testCtx, ndx + 1));
		uint32Tests[ndx]	= de::MovePtr<SpvAsmTypeUint32Tests>(new SpvAsmTypeUint32Tests(testCtx, ndx + 1));
		uint64Tests[ndx]	= de::MovePtr<SpvAsmTypeUint64Tests>(new SpvAsmTypeUint64Tests(testCtx, ndx + 1));
	}

	MAKE_TEST_SV_I_8136("negate", SpvOpSNegate, negate, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("add", SpvOpIAdd, add, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("sub", SpvOpISub, sub, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("mul", SpvOpIMul, mul, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("div", SpvOpSDiv, div, FILTER_ZERO, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136("div", SpvOpUDiv, div, FILTER_ZERO, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("rem", SpvOpSRem, rem, FILTER_NEGATIVES_AND_ZERO, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("mod", SpvOpSMod, mod, FILTER_NEGATIVES_AND_ZERO, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136("mod", SpvOpUMod, mod, FILTER_ZERO, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("abs", GLSLstd450SAbs, abs, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_I_8136("sign", GLSLstd450SSign, sign, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_I_8136("min", GLSLstd450SMin, min, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_U_8136("min", GLSLstd450UMin, min, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_I_8136("max", GLSLstd450SMax, max, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_U_8136("max", GLSLstd450UMax, max, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_I_8136("clamp", GLSLstd450SClamp, clamp, FILTER_MIN_GT_MAX, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_U_8136("clamp", GLSLstd450UClamp, clamp, FILTER_MIN_GT_MAX, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_I_3("find_lsb", GLSLstd450FindILsb, lsb, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_I_3("find_msb", GLSLstd450FindSMsb, msb, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_U_3("find_msb", GLSLstd450FindUMsb, msb, FILTER_NONE, RANGE_FULL, "GLSL.std.450");
	MAKE_TEST_SV_I_1("mul_sdiv", DE_NULL, mul_div, FILTER_ZERO, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_1("mul_udiv", DE_NULL, mul_div, FILTER_ZERO, RANGE_FULL, DE_NULL);

	MAKE_TEST_SV_U_8136_W("shift_right_logical", SpvOpShiftRightLogical, lsr, FILTER_NONE, RANGE_BIT_WIDTH, DE_NULL);
	MAKE_TEST_SV_I_8136_W("shift_right_logical", SpvOpShiftRightLogical, lsr, FILTER_NONE, RANGE_BIT_WIDTH, DE_NULL);
	MAKE_TEST_SV_U_8136_W("shift_right_arithmetic", SpvOpShiftRightArithmetic, asr, FILTER_NONE, RANGE_BIT_WIDTH, DE_NULL);
	MAKE_TEST_SV_I_8136_W("shift_right_arithmetic", SpvOpShiftRightArithmetic, asr, FILTER_NONE, RANGE_BIT_WIDTH, DE_NULL);
	MAKE_TEST_SV_U_8136_W("shift_left_logical", SpvOpShiftLeftLogical, lsl, FILTER_NONE, RANGE_BIT_WIDTH, DE_NULL);
	MAKE_TEST_SV_I_8136_W("shift_left_logical", SpvOpShiftLeftLogical, lsl, FILTER_NONE, RANGE_BIT_WIDTH, DE_NULL);

	MAKE_TEST_SV_U_8136("bitwise_or", SpvOpBitwiseOr, bitwise_or, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("bitwise_or", SpvOpBitwiseOr, bitwise_or , FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136("bitwise_xor", SpvOpBitwiseXor, bitwise_xor, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("bitwise_xor", SpvOpBitwiseXor, bitwise_xor, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136("bitwise_and", SpvOpBitwiseAnd, bitwise_and, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("bitwise_and", SpvOpBitwiseAnd, bitwise_and, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136("not", SpvOpNot, not, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("not", SpvOpNot, not, FILTER_NONE, RANGE_FULL, DE_NULL);

	MAKE_TEST_SV_U_8136_N("iequal", SpvOpIEqual, iequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("iequal", SpvOpIEqual, iequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("inotequal", SpvOpINotEqual, inotequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("inotequal", SpvOpINotEqual, inotequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("ugreaterthan", SpvOpUGreaterThan, ugreaterthan, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("ugreaterthan", SpvOpUGreaterThan, ugreaterthan, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("sgreaterthan", SpvOpSGreaterThan, sgreaterthan, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("sgreaterthan", SpvOpSGreaterThan, sgreaterthan, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("ugreaterthanequal", SpvOpUGreaterThanEqual, ugreaterthanequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("ugreaterthanequal", SpvOpUGreaterThanEqual, ugreaterthanequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("sgreaterthanequal", SpvOpSGreaterThanEqual, sgreaterthanequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("sgreaterthanequal", SpvOpSGreaterThanEqual, sgreaterthanequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("ulessthan", SpvOpULessThan, ulessthan, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("ulessthan", SpvOpULessThan, ulessthan, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("slessthan", SpvOpSLessThan, slessthan, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("slessthan", SpvOpSLessThan, slessthan, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("ulessthanequal", SpvOpULessThanEqual, ulessthanequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("ulessthanequal", SpvOpULessThanEqual, ulessthanequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136_N("slessthanequal", SpvOpSLessThanEqual, slessthanequal, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136_N("slessthanequal", SpvOpSLessThanEqual, slessthanequal, FILTER_NONE, RANGE_FULL, DE_NULL);

	MAKE_TEST_SV_U_3_W("bit_field_insert", SpvOpBitFieldInsert, bitFieldInsert, FILTER_NONE, RANGE_BIT_WIDTH_SUM, DE_NULL);
	MAKE_TEST_SV_I_3_W("bit_field_insert", SpvOpBitFieldInsert, bitFieldInsert, FILTER_NONE, RANGE_BIT_WIDTH_SUM, DE_NULL);
	MAKE_TEST_SV_U_3_W("bit_field_s_extract", SpvOpBitFieldSExtract, bitFieldSExtract, FILTER_NONE, RANGE_BIT_WIDTH_SUM, DE_NULL);
	MAKE_TEST_SV_I_3_W("bit_field_s_extract", SpvOpBitFieldSExtract, bitFieldSExtract, FILTER_NONE, RANGE_BIT_WIDTH_SUM, DE_NULL);
	MAKE_TEST_SV_U_3_W("bit_field_u_extract", SpvOpBitFieldUExtract, bitFieldUExtract, FILTER_NONE, RANGE_BIT_WIDTH_SUM, DE_NULL);
	MAKE_TEST_SV_I_3_W("bit_field_u_extract", SpvOpBitFieldUExtract, bitFieldUExtract, FILTER_NONE, RANGE_BIT_WIDTH_SUM, DE_NULL);
	MAKE_TEST_SV_U_3("bit_reverse", SpvOpBitReverse, bitReverse, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_3("bit_reverse", SpvOpBitReverse, bitReverse, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_3("bit_count", SpvOpBitCount, bitCount, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_3("bit_count", SpvOpBitCount, bitCount, FILTER_NONE, RANGE_FULL, DE_NULL);

	MAKE_TEST_S_U_8136("constant", SpvOpConstant, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_S_I_8136("constant", SpvOpConstant, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_V_U_8136("constant_composite", SpvOpConstantComposite, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_V_I_8136("constant_composite", SpvOpConstantComposite, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_V_U_8136("constant_null", SpvOpConstantNull, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_V_I_8136("constant_null", SpvOpConstantNull, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_U_8136("variable_initializer", SpvOpVariable, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_SV_I_8136("variable_initializer", SpvOpVariable, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_S_U_8136("spec_constant_initializer", SpvOpSpecConstant, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_S_I_8136("spec_constant_initializer", SpvOpSpecConstant, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_V_U_8136("spec_constant_composite_initializer", SpvOpSpecConstantComposite, constant, FILTER_NONE, RANGE_FULL, DE_NULL);
	MAKE_TEST_V_I_8136("spec_constant_composite_initializer", SpvOpSpecConstantComposite, constant, FILTER_NONE, RANGE_FULL, DE_NULL);

	int8Tests[0]->createSwitchTests();
	int16Tests[0]->createSwitchTests();
	int32Tests[0]->createSwitchTests();
	int64Tests[0]->createSwitchTests();
	uint8Tests[0]->createSwitchTests();
	uint16Tests[0]->createSwitchTests();
	uint32Tests[0]->createSwitchTests();
	uint64Tests[0]->createSwitchTests();

	typeScalarTests->addChild(int8Tests[0].release());
	typeScalarTests->addChild(int16Tests[0].release());
	typeScalarTests->addChild(int32Tests[0].release());
	typeScalarTests->addChild(int64Tests[0].release());
	typeScalarTests->addChild(uint8Tests[0].release());
	typeScalarTests->addChild(uint16Tests[0].release());
	typeScalarTests->addChild(uint32Tests[0].release());
	typeScalarTests->addChild(uint64Tests[0].release());

	typeTests->addChild(typeScalarTests.release());

	for (deUint32 ndx = 0; ndx < 3; ++ndx)
	{
		typeVectorTests[ndx]->addChild(int8Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(int16Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(int32Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(int64Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(uint8Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(uint16Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(uint32Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(uint64Tests[ndx + 1].release());

		typeTests->addChild(typeVectorTests[ndx].release());
	}

	return typeTests.release();
}

} // SpirVAssembly
} // vkt
