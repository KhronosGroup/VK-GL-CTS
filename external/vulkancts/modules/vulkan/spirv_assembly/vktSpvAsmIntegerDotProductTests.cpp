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
 * \brief Functional integer dot product tests
 *//*--------------------------------------------------------------------*/


#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "deRandom.hpp"

#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmIntegerDotProductTests.hpp"

#include <limits>
#include <string>

// VK_KHR_shader_integer_dot_product tests

// Note: these tests make use of the following extensions that are not
// required by the VK_KHR_shader_integer_dot_product extension itself:
//    * VK_KHR_8bit_storage (VkPhysicalDevice8BitStorageFeatures) for shaderInt8
//    * VK_KHR_16bit_storage (VkPhysicalDevice16BitStorageFeatures) for shaderInt16

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::string;

namespace
{
using std::vector;
using tcu::IVec3;
using tcu::TestLog;

template<typename T>
static void fillRandomScalars(de::Random& rnd, T minValue, T maxValue, void* dst, int numValues, int offset = 0)
{
	T* const typedPtr = (T*)dst;
	for (int ndx = 0; ndx < numValues; ndx++)
		typedPtr[offset + ndx] = de::randomScalar<T>(rnd, minValue, maxValue);
}

template <typename T> T getEqualValue(T v1, T v2)
{
	DE_ASSERT(v1 == v2);
	(void)v2;
	return v1;
}

template <class T>
bool withinLimits(deInt64 val)
{
	return static_cast<deInt64>(std::numeric_limits<T>::min()) < val
		   && val < static_cast<deInt64>(std::numeric_limits<T>::max());
}

template <class T, class LHSOperandT, class RHSOperandT>
static T dotProduct(vector<LHSOperandT> lhs, vector<RHSOperandT> rhs)
{
	deInt64 res = 0;

	size_t size = getEqualValue(lhs.size(), rhs.size());

	for (size_t i = 0; i < size; ++i)
	{
		res += static_cast<deInt64>(lhs[i]) * static_cast<deInt64>(rhs[i]);
	}

	return static_cast<T>(res);
}

template <class AddendT, class LHSOperandT, class RHSOperandT>
bool compareDotProductAccSat(const std::vector<Resource> &inputs, const vector<AllocationSp>& outputAllocs,
							 const std::vector<Resource>&, TestLog&)
{
	if (inputs.size() != 3 || outputAllocs.size() != 1)
		return false;

	vector<deUint8>	lhsBytes;
	vector<deUint8>	rhsBytes;
	vector<deUint8>	addendBytes;

	inputs[0].getBytes(lhsBytes);
	inputs[1].getBytes(rhsBytes);
	inputs[2].getBytes(addendBytes);

	const AddendT* const	output	= static_cast<AddendT* const>(outputAllocs[0]->getHostPtr());
	const AddendT* const	addends	= reinterpret_cast<AddendT* const>(&addendBytes.front());
	const LHSOperandT* const	lhsInts = reinterpret_cast<LHSOperandT* const>(&lhsBytes.front());
	const RHSOperandT* const	rhsInts = reinterpret_cast<RHSOperandT* const>(&rhsBytes.front());

	for (size_t idx = 0; idx < inputs[2].getByteSize() / sizeof(AddendT); ++idx)
	{
		size_t vecLen = (inputs[0].getByteSize() / sizeof(LHSOperandT)) / (inputs[2].getByteSize() / sizeof(AddendT));

		std::vector<LHSOperandT> inputVec1Pos;
		std::vector<RHSOperandT> inputVec2Pos;
		inputVec1Pos.reserve(vecLen);
		inputVec2Pos.reserve(vecLen);

		std::vector<LHSOperandT> inputVec1Neg;
		std::vector<RHSOperandT> inputVec2Neg;
		inputVec1Neg.reserve(vecLen);
		inputVec2Neg.reserve(vecLen);

		for (unsigned int vecElem = 0; vecElem < vecLen; ++vecElem)
		{
			LHSOperandT elem1 = lhsInts[idx * vecLen + vecElem];
			RHSOperandT elem2 = rhsInts[idx * vecLen + vecElem];

			// Note: ordering of components does not matter, provided
			// that it is consistent between lhs and rhs.
			if ((elem1 < 0) == (elem2 < 0))
			{
				inputVec1Pos.push_back(elem1);
				inputVec2Pos.push_back(elem2);
				inputVec1Neg.push_back(0);
				inputVec2Neg.push_back(0);
			}
			else
			{
				inputVec1Pos.push_back(0);
				inputVec2Pos.push_back(0);
				inputVec1Neg.push_back(elem1);
				inputVec2Neg.push_back(elem2);
			}
		}

		deInt64 PosProduct = dotProduct<deInt64>(inputVec1Pos, inputVec2Pos);
		deInt64 NegProduct = dotProduct<deInt64>(inputVec1Neg, inputVec2Neg);
		bool outputOverflow = (!withinLimits<AddendT>(PosProduct) || !withinLimits<AddendT>(NegProduct));

		if (!outputOverflow)
		{
			AddendT expectedOutput = static_cast<AddendT>(PosProduct + NegProduct);

			if (addends[idx] < 0)
			{
				if (expectedOutput < std::numeric_limits<AddendT>::min() - addends[idx])
					expectedOutput = std::numeric_limits<AddendT>::min();
				else
					expectedOutput = static_cast<AddendT>(expectedOutput + addends[idx]);
			}
			else
			{
				if (expectedOutput > std::numeric_limits<AddendT>::max() - addends[idx])
					expectedOutput = std::numeric_limits<AddendT>::max();
				else
					expectedOutput = static_cast<AddendT>(expectedOutput + addends[idx]);
			}

			if (output[idx] != expectedOutput)
			{
				return false;
			}
		}
	}

	return true;
}

struct DotProductPackingInfo
{
	bool packed;
	bool signedLHS;
	bool signedRHS;
};

struct DotProductVectorInfo
{
	size_t vecElementSize;
	unsigned int vecLen;
};

void addDotProductExtensionAndFeatures(ComputeShaderSpec &spec,
									   const struct DotProductPackingInfo &packingInfo,
									   size_t elementSize, size_t outSize)
{
	spec.extensions.push_back("VK_KHR_shader_integer_dot_product");
	spec.requestedVulkanFeatures.extIntegerDotProduct.shaderIntegerDotProduct = VK_TRUE;

	DE_ASSERT(!packingInfo.packed || elementSize == 8);
	if ((!packingInfo.packed && elementSize == 8) || outSize == 8)
	{
		spec.requestedVulkanFeatures.extFloat16Int8 |= EXTFLOAT16INT8FEATURES_INT8;
		spec.requestedVulkanFeatures.ext8BitStorage = EXT8BITSTORAGEFEATURES_STORAGE_BUFFER;
		spec.extensions.push_back("VK_KHR_8bit_storage");
	}

	if (elementSize == 16 || outSize == 16)
	{
		spec.requestedVulkanFeatures.coreFeatures.shaderInt16 = VK_TRUE;
		spec.requestedVulkanFeatures.ext16BitStorage = EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK;
		spec.extensions.push_back("VK_KHR_16bit_storage");
	}
}

const struct DotProductPackingInfo dotProductPacking[] = {
	{ false, false, false },
	{ false, false, true  },
	{ false, true,  false },
	{ false, true,  true  },
	{ true,  true,  true  },
	{ true,  true,  false },
	{ true,  false, true  },
	{ true,  false, false },
};

const struct DotProductVectorInfo dotProductVector8[] = {
	{ 8, 2 },
	{ 8, 3 },
	{ 8, 4 },
};

const struct DotProductVectorInfo dotProductVector16[] = {
	{ 16, 2 },
	{ 16, 3 },
	{ 16, 4 },
};

const struct DotProductVectorInfo dotProductVector32[] = {
	{ 32, 2 },
	{ 32, 3 },
	{ 32, 4 },
};

unsigned int getAlignedVecLen(const DotProductVectorInfo& vectorInfo)
{
	return (vectorInfo.vecLen == 3 ? 4 : vectorInfo.vecLen);
}

void generateIntegerDotProductTypeDeclsAndStrideDecors(std::ostringstream &typeDeclsStream, std::ostringstream &strideDecorsStream,
													   const struct DotProductPackingInfo &packingInfo,
													   const struct DotProductVectorInfo &vectorInfo, size_t outSize,
													   bool signedLHSAndResult, bool signedRHS)
{
	size_t signedScalarArraysMask = 0;
	size_t unsignedScalarArraysMask = 0;
	bool signedIntVectorNeeded = false;
	bool unsignedIntVectorNeeded = false;

	if (signedLHSAndResult)
		signedScalarArraysMask |= static_cast<int>(outSize);
	else
		unsignedScalarArraysMask |= static_cast<int>(outSize);

	if (packingInfo.packed)
	{
		if (packingInfo.signedLHS || packingInfo.signedRHS)
			signedScalarArraysMask |= vectorInfo.vecElementSize * vectorInfo.vecLen;
		if (!packingInfo.signedLHS || !packingInfo.signedRHS)
			unsignedScalarArraysMask |= vectorInfo.vecElementSize * vectorInfo.vecLen;
	}
	else
	{
		if (signedLHSAndResult)
		{
			signedIntVectorNeeded = true;
			signedScalarArraysMask |= vectorInfo.vecElementSize;
		}
		if (!signedRHS)
		{
			unsignedIntVectorNeeded = true;
			unsignedScalarArraysMask |= vectorInfo.vecElementSize;
		}
	}

	size_t signedScalarTypesMask = signedScalarArraysMask;
	size_t unsignedScalarTypesMask = unsignedScalarArraysMask;

	for (unsigned int size = 8; size <= 64; size *= 2)
	{
		if (size != 32)
		{
			string sizeStr(de::toString(size));
			if ((signedScalarTypesMask & size))
				typeDeclsStream << "%i" << sizeStr << " = OpTypeInt " << sizeStr << " 1\n";
			if ((unsignedScalarTypesMask & size))
				typeDeclsStream << "%u" << sizeStr << " = OpTypeInt " << sizeStr << " 0\n";
		}
	}

	for (unsigned int size = 8; size <= 64; size *= 2)
	{
		string sizeStr = de::toString(size);
		if ((signedScalarArraysMask & size))
		{
			if (size != 32)
				typeDeclsStream << "%i" << sizeStr << "ptr = OpTypePointer Uniform %i" << sizeStr << "\n"
								   "%i" << sizeStr << "arr = OpTypeRuntimeArray %i" << sizeStr << "\n";
			strideDecorsStream << "OpDecorate %i" << sizeStr << "arr ArrayStride " << de::toString(size / 8) << "\n";
		}
		if ((unsignedScalarArraysMask & size))
		{
			typeDeclsStream << "%u" << sizeStr << "ptr = OpTypePointer Uniform %u" << sizeStr << "\n"
							   "%u" << sizeStr << "arr = OpTypeRuntimeArray %u" << sizeStr << "\n";
			strideDecorsStream << "OpDecorate %u" << sizeStr << "arr ArrayStride " << de::toString(size / 8) << "\n";
		}
	}

	if (signedIntVectorNeeded)
	{
		string vecType = "%i" + de::toString(vectorInfo.vecElementSize) + "vec" + de::toString(vectorInfo.vecLen);
		typeDeclsStream << vecType << " = OpTypeVector %i" << vectorInfo.vecElementSize << " " << vectorInfo.vecLen << "\n"
						<< vecType << "ptr = OpTypePointer Uniform " << vecType << "\n"
						<< vecType << "arr = OpTypeRuntimeArray " << vecType << "\n";
		strideDecorsStream << "OpDecorate " << vecType << "arr ArrayStride "
						   << (vectorInfo.vecLen == 3 ? 4 : vectorInfo.vecLen) * (vectorInfo.vecElementSize / 8) << "\n";
	}


	if (unsignedIntVectorNeeded)
	{
		string vecType = "%u" + de::toString(vectorInfo.vecElementSize) + "vec" + de::toString(vectorInfo.vecLen);
		bool changeTypeName = false;
		if (vectorInfo.vecElementSize == 32 && vectorInfo.vecLen == 3)
			changeTypeName = true;
		else
			typeDeclsStream << vecType << " = OpTypeVector %u" << vectorInfo.vecElementSize << " " << vectorInfo.vecLen << "\n";

		typeDeclsStream << vecType << "ptr = OpTypePointer Uniform " << (changeTypeName ? "%uvec3" : vecType) << "\n"
						<< vecType << "arr = OpTypeRuntimeArray " << (changeTypeName ? "%uvec3" : vecType) << "\n";
		strideDecorsStream << "OpDecorate " << vecType << "arr ArrayStride "
						   << (vectorInfo.vecLen == 3 ? 4 : vectorInfo.vecLen) * (vectorInfo.vecElementSize / 8) << "\n";
	}
}

string generateIntegerDotProductCode(const struct DotProductPackingInfo &packingInfo, const struct DotProductVectorInfo &vectorInfo,
									 size_t outSize, bool signedLHSAndResult, bool signedRHS, bool acc)
{
	DE_ASSERT(signedLHSAndResult || !signedRHS);

	const string insnSignedness(signedLHSAndResult ? (signedRHS ? "S" : "SU") : "U");
	const string insnName(string("Op") + insnSignedness + "Dot" + (acc ? "AccSat" : "") + "KHR");

	const string outputCapability(outSize != 32 ?
		"OpCapability Int" + de::toString(outSize) + "\n" : "");
	const string elementCapability(!packingInfo.packed && outSize != vectorInfo.vecElementSize && vectorInfo.vecElementSize != 32 ?
		"OpCapability Int" + de::toString(vectorInfo.vecElementSize) + "\n" : "");

	const string dotProductInputCapabilityName(
		packingInfo.packed ? "DotProductInput4x8BitPackedKHR" : (vectorInfo.vecElementSize > 8) ? "DotProductInputAllKHR" : "DotProductInput4x8BitKHR");

	const string capabilities(outputCapability +
							  elementCapability +
							  "OpCapability " + dotProductInputCapabilityName + "\n"
							  "OpCapability DotProductKHR\n");
	const string extensions("OpExtension \"SPV_KHR_integer_dot_product\"\n");

	const string outType((signedLHSAndResult ? "i" : "u") + de::toString(outSize));

	std::ostringstream typeDeclsStream;
	std::ostringstream strideDecorsStream;
	generateIntegerDotProductTypeDeclsAndStrideDecors(typeDeclsStream, strideDecorsStream,
													  packingInfo, vectorInfo, outSize, signedLHSAndResult, signedRHS);
	string typeDecls(typeDeclsStream.str());
	string strideDecors(strideDecorsStream.str());

	const string lhsVecType(packingInfo.packed
							? string(packingInfo.signedLHS ? "i" : "u") + de::toString(vectorInfo.vecElementSize * vectorInfo.vecLen)
							: (signedLHSAndResult ? "i" : "u") + ((!signedLHSAndResult && vectorInfo.vecElementSize == 32 && vectorInfo.vecLen == 3) ? "" : de::toString(vectorInfo.vecElementSize)) + "vec" + de::toString(vectorInfo.vecLen));
	const string rhsVecType(packingInfo.packed
							? string(packingInfo.signedRHS ? "i" : "u") + de::toString(vectorInfo.vecElementSize * vectorInfo.vecLen)
							: (signedRHS ? "i" : "u") + ((!signedRHS && vectorInfo.vecElementSize == 32 && vectorInfo.vecLen == 3) ? "" : de::toString(vectorInfo.vecElementSize)) + "vec" + de::toString(vectorInfo.vecLen));
	const string lhsVecTypeBase(packingInfo.packed
								? string(packingInfo.signedLHS ? "i" : "u") + de::toString(vectorInfo.vecElementSize * vectorInfo.vecLen)
								: (signedLHSAndResult ? "i" : "u") + de::toString(vectorInfo.vecElementSize) + "vec" + de::toString(vectorInfo.vecLen));
	const string rhsVecTypeBase(packingInfo.packed
								? string(packingInfo.signedRHS ? "i" : "u") + de::toString(vectorInfo.vecElementSize * vectorInfo.vecLen)
								: (signedRHS ? "i" : "u") + de::toString(vectorInfo.vecElementSize) + "vec" + de::toString(vectorInfo.vecLen));

	const string optFormatParam(packingInfo.packed ? " PackedVectorFormat4x8BitKHR" : "");

	bool bufferSignednessMatches = (packingInfo.packed
									? (packingInfo.signedLHS == packingInfo.signedRHS)
									: (signedLHSAndResult == signedRHS));

	return string(getComputeAsmShaderPreamble(capabilities, extensions)) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		+ (bufferSignednessMatches
		   ? "OpDecorate %bufin BufferBlock\n"
		   : "OpDecorate %buflhs BufferBlock\n"
			 "OpDecorate %bufrhs BufferBlock\n") +
		"OpDecorate %bufout BufferBlock\n"
		"OpDecorate %indatalhs DescriptorSet 0\n"
		"OpDecorate %indatalhs Binding 0\n"
		"OpDecorate %indatarhs DescriptorSet 0\n"
		"OpDecorate %indatarhs Binding 1\n"
		+ (acc
		   ? "OpDecorate %indataacc DescriptorSet 0\n"
			 "OpDecorate %indataacc Binding 2\n"
		   : "") +
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding " + (acc ? "3" : "2") + "\n"
		+ strideDecors

		+ (bufferSignednessMatches
		   ? "OpMemberDecorate %bufin 0 Offset 0\n"
		   : "OpMemberDecorate %buflhs 0 Offset 0\n"
			 "OpMemberDecorate %bufrhs 0 Offset 0\n") +
		"OpMemberDecorate %bufout 0 Offset 0\n"

		+ getComputeAsmCommonTypes()
		+ typeDecls

		+ (bufferSignednessMatches
		   ? "%bufin     = OpTypeStruct %" + lhsVecTypeBase + "arr\n"
			 "%bufinptr  = OpTypePointer Uniform %bufin\n"
		   : "%buflhs    = OpTypeStruct %" + lhsVecTypeBase + "arr\n"
			 "%buflhsptr = OpTypePointer Uniform %buflhs\n"
			 "%bufrhs    = OpTypeStruct %" + rhsVecTypeBase + "arr\n"
			 "%bufrhsptr = OpTypePointer Uniform %bufrhs\n") +
		"%bufout    = OpTypeStruct %" + outType + "arr\n"
		"%bufoutptr = OpTypePointer Uniform %bufout\n"
		"%indatalhs = OpVariable " + (bufferSignednessMatches ? "%bufinptr" : "%buflhsptr") + " Uniform\n"
		"%indatarhs = OpVariable " + (bufferSignednessMatches ? "%bufinptr" : "%bufrhsptr") + " Uniform\n"
		+ (acc ? "%indataacc = OpVariable %bufoutptr Uniform\n" : "") +
		"%outdata   = OpVariable %bufoutptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloclhs  = OpAccessChain %" + lhsVecTypeBase + "ptr %indatalhs %zero %x\n"
		"%invallhs  = OpLoad %" + lhsVecType + " %inloclhs\n"
		"%inlocrhs  = OpAccessChain %" + rhsVecTypeBase + "ptr %indatarhs %zero %x\n"
		"%invalrhs  = OpLoad %" + rhsVecType + " %inlocrhs\n"
		+ (acc
		   ? "%inlocacc  = OpAccessChain %" + outType + "ptr %indataacc %zero %x\n"
			 "%invalacc  = OpLoad %" + outType + " %inlocacc\n"
		   : ""
		  ) +
		"%res       = " + insnName + " %" + outType + " %invallhs %invalrhs" + (acc ? " %invalacc" : "") + optFormatParam + "\n"
		"%outloc    = OpAccessChain %" + outType + "ptr %outdata %zero %x\n"
		"             OpStore %outloc %res\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
}

struct DotProductInputInfo
{
	string			name;
	unsigned int	vecLen;
	size_t			vecElemSize;
};

template <class OutputT, class LHSOperandT, class RHSOperandT>
void fillDotProductOutputs(int numElements, vector<LHSOperandT> &inputInts1, vector<RHSOperandT> &inputInts2,
						   vector<OutputT> &outputInts, const struct DotProductInputInfo &inputInfo)
{
	unsigned int alignedVecLen = inputInfo.vecLen == 3 ? 4 : inputInfo.vecLen;
	for (int ndx = 0; ndx < numElements; ++ndx)
	{
		std::vector<LHSOperandT> inputVec1;
		std::vector<RHSOperandT> inputVec2;
		inputVec1.reserve(alignedVecLen);
		inputVec2.reserve(alignedVecLen);

		for (unsigned int vecElem = 0; vecElem < alignedVecLen; ++vecElem)
		{
			// Note: ordering of components does not matter, provided
			// that it is consistent between lhs and rhs.
			inputVec1.push_back(inputInts1[ndx * alignedVecLen + vecElem]);
			inputVec2.push_back(inputInts2[ndx * alignedVecLen + vecElem]);
		}

		outputInts[ndx] = dotProduct<OutputT>(inputVec1, inputVec2);
	}
}

string getDotProductTestName(const struct DotProductInputInfo &inputInfo,
							 const struct DotProductPackingInfo &packingInfo, size_t outSize)
{
	return inputInfo.name
		+ (packingInfo.packed ? string("_packed_") : "_")
		+ (packingInfo.signedLHS ? "s" : "u") + (packingInfo.signedRHS ? "s" : "u") +
		"_v" + de::toString(inputInfo.vecLen) + "i" + de::toString(inputInfo.vecElemSize) +
		"_out" + de::toString(outSize);
}

template <class InBufferT, class OutBufferT, class OutputT, class OperandT>
void addOpSDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group,
							  int numElements, vector<OperandT> &inputInts1, vector<OperandT> &inputInts2,
							  const struct DotProductInputInfo &inputInfo, const struct DotProductPackingInfo &packingInfo,
							  const struct DotProductVectorInfo &vectorInfo)
{
	ComputeShaderSpec	spec;
	size_t				outSize = sizeof(OutputT) * 8;
	vector<OutputT>		outputInts	(numElements, 0);

	fillDotProductOutputs(numElements, inputInts1, inputInts2, outputInts, inputInfo);

	spec.assembly = generateIntegerDotProductCode(packingInfo, vectorInfo, outSize, true, true, false);
	addDotProductExtensionAndFeatures(spec, packingInfo, vectorInfo.vecElementSize, outSize);

	spec.inputs.push_back	(BufferSp(new InBufferT(inputInts1)));
	spec.inputs.push_back	(BufferSp(new InBufferT(inputInts2)));
	spec.outputs.push_back	(BufferSp(new OutBufferT(outputInts)));
	spec.numWorkGroups		= IVec3(numElements, 1, 1);
	spec.failResult			= QP_TEST_RESULT_FAIL;
	spec.failMessage		= "Output doesn't match with expected";

	string qualTestName(getDotProductTestName(inputInfo, packingInfo, outSize));

	group->addChild(new SpvAsmComputeShaderCase(testCtx, qualTestName.data(), "", spec));
}

template <class InBufferT, class T>
void addOpSDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name,
							  const struct DotProductPackingInfo dotProductPackingInfo[], unsigned dotProductPackingInfoSize,
							  const struct DotProductVectorInfo dotProductVectorInfo[], unsigned dotProductVectorInfoSize,
							  T vecMin, T vecMax)
{
	const int		numElements	= 200;
	// Note: this test does not currently cover 64-bit integer results
	for (unsigned int j = 0; j < dotProductVectorInfoSize; j++)
	{
		const struct DotProductVectorInfo &vectorInfo = dotProductVectorInfo[j];
		unsigned int alignedVecLen = getAlignedVecLen(vectorInfo);
		struct DotProductInputInfo inputInfo = { name, vectorInfo.vecLen, vectorInfo.vecElementSize };
		vector<T>		inputInts1	(numElements * alignedVecLen, 0);
		vector<T>		inputInts2	(numElements * alignedVecLen, 0);

		fillRandomScalars(rnd, vecMin, vecMax, &inputInts1[0], numElements * alignedVecLen);
		fillRandomScalars(rnd, vecMin, vecMax, &inputInts2[0], numElements * alignedVecLen);

		if (vectorInfo.vecLen == 3)
			for (unsigned int ndx = 0; ndx < numElements; ++ndx)
				inputInts1[ndx*4+3] = inputInts2[ndx*4+3] = 0;

		for (unsigned int i = 0; i < dotProductPackingInfoSize; i++)
		{
			const struct DotProductPackingInfo &packingInfo = dotProductPackingInfo[i];
			if (packingInfo.packed && (vectorInfo.vecElementSize != 8 || vectorInfo.vecLen != 4))
				continue;

			if (vectorInfo.vecElementSize <= 32)
				addOpSDotKHRComputeTests<InBufferT, Int32Buffer, deInt32>(testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
			if (vectorInfo.vecElementSize <= 16)
				addOpSDotKHRComputeTests<InBufferT, Int16Buffer, deInt16>(testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
			if (vectorInfo.vecElementSize <= 8)
				addOpSDotKHRComputeTests<InBufferT, Int8Buffer,  deInt8> (testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
		}
	}
}

template <class T>
void add32bitOpSDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax)
{
	addOpSDotKHRComputeTests<Int32Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
										  dotProductVector32, DE_LENGTH_OF_ARRAY(dotProductVector32), vecMin, vecMax);
}

template <class T>
void add16bitOpSDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax)
{
	addOpSDotKHRComputeTests<Int16Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
										  dotProductVector16, DE_LENGTH_OF_ARRAY(dotProductVector16), vecMin, vecMax);
}

template <class T>
void add8bitOpSDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax)
{
	addOpSDotKHRComputeTests<Int8Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
										 dotProductVector8, DE_LENGTH_OF_ARRAY(dotProductVector8), vecMin, vecMax);
}

template <class InBufferT, class OutBufferT, class OutputT, class OperandT>
void addOpUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group,
							  int numElements, vector<OperandT> &inputInts1, vector<OperandT> &inputInts2,
							  const struct DotProductInputInfo &inputInfo, const struct DotProductPackingInfo &packingInfo,
							  const struct DotProductVectorInfo &vectorInfo)
{
	ComputeShaderSpec	spec;
	size_t				outSize = sizeof(OutputT) * 8;
	vector<OutputT>		outputInts	(numElements, 0);

	fillDotProductOutputs(numElements, inputInts1, inputInts2, outputInts, inputInfo);

	spec.assembly = generateIntegerDotProductCode(packingInfo, vectorInfo, outSize, false, false, false);

	addDotProductExtensionAndFeatures(spec, packingInfo, vectorInfo.vecElementSize, outSize);

	spec.inputs.push_back	(BufferSp(new InBufferT(inputInts1)));
	spec.inputs.push_back	(BufferSp(new InBufferT(inputInts2)));
	spec.outputs.push_back	(BufferSp(new OutBufferT(outputInts)));
	spec.numWorkGroups		= IVec3(numElements, 1, 1);
	spec.failResult			= QP_TEST_RESULT_FAIL;
	spec.failMessage		= "Output doesn't match with expected";

	string qualTestName(getDotProductTestName(inputInfo, packingInfo, outSize));

	group->addChild(new SpvAsmComputeShaderCase(testCtx, qualTestName.data(), "", spec));
}

template <class InBufferT, class T>
void addOpUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name,
							  const struct DotProductPackingInfo dotProductPackingInfo[], unsigned dotProductPackingInfoSize,
							  const struct DotProductVectorInfo dotProductVectorInfo[], unsigned dotProductVectorInfoSize,
							  T vecMin, T vecMax)
{
	const int		numElements	= 200;

	for (unsigned int j = 0; j < dotProductVectorInfoSize; j++)
	{
		const struct DotProductVectorInfo &vectorInfo = dotProductVectorInfo[j];
		unsigned int alignedVecLen = getAlignedVecLen(vectorInfo);
		struct DotProductInputInfo inputInfo = { name, vectorInfo.vecLen, vectorInfo.vecElementSize };
		vector<T>		inputInts1	(numElements * alignedVecLen, 0);
		vector<T>		inputInts2	(numElements * alignedVecLen, 0);

		fillRandomScalars(rnd, vecMin, vecMax, &inputInts1[0], numElements * alignedVecLen);
		fillRandomScalars(rnd, vecMin, vecMax, &inputInts2[0], numElements * alignedVecLen);

		if (vectorInfo.vecLen == 3)
			for (unsigned int ndx = 0; ndx < numElements; ++ndx)
				inputInts1[ndx*4+3] = inputInts2[ndx*4+3] = 0;

		for (unsigned int i = 0; i < dotProductPackingInfoSize; i++)
		{
			const struct DotProductPackingInfo &packingInfo = dotProductPackingInfo[i];
			if (packingInfo.packed && (vectorInfo.vecElementSize != 8 || vectorInfo.vecLen != 4))
				continue;

			if (vectorInfo.vecElementSize <= 32)
				addOpUDotKHRComputeTests<InBufferT, Uint32Buffer, deUint32>(testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
			if (vectorInfo.vecElementSize <= 16)
				addOpUDotKHRComputeTests<InBufferT, Uint16Buffer, deUint16>(testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
			if (vectorInfo.vecElementSize <= 8)
				addOpUDotKHRComputeTests<InBufferT, Uint8Buffer,  deUint8> (testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
		}
	}
}

template <class T>
void add32bitOpUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax)
{
	addOpUDotKHRComputeTests<Uint32Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
										   dotProductVector32, DE_LENGTH_OF_ARRAY(dotProductVector32), vecMin, vecMax);
}

template <class T>
void add16bitOpUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax)
{
	addOpUDotKHRComputeTests<Uint16Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
										   dotProductVector16, DE_LENGTH_OF_ARRAY(dotProductVector16), vecMin, vecMax);
}

template <class T>
void add8bitOpUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax)
{
	addOpUDotKHRComputeTests<Uint8Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
										  dotProductVector8, DE_LENGTH_OF_ARRAY(dotProductVector8), vecMin, vecMax);
}

template <class LHSBufferT, class RHSBufferT, class OutBufferT, class OutputT, class LHSOperandT, class RHSOperandT>
void addOpSUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group,
							   int numElements, vector<LHSOperandT> &inputInts1, vector<RHSOperandT> &inputInts2,
							   const struct DotProductInputInfo &inputInfo, const struct DotProductPackingInfo &packingInfo,
							   const struct DotProductVectorInfo &vectorInfo)
{
	ComputeShaderSpec	spec;
	size_t				outSize = sizeof(OutputT) * 8;
	vector<OutputT>		outputInts	(numElements, 0);

	fillDotProductOutputs(numElements, inputInts1, inputInts2, outputInts, inputInfo);

	spec.assembly = generateIntegerDotProductCode(packingInfo, vectorInfo, outSize, true, false, false);
	addDotProductExtensionAndFeatures(spec, packingInfo, vectorInfo.vecElementSize, outSize);

	spec.inputs.push_back	(BufferSp(new LHSBufferT(inputInts1)));
	spec.inputs.push_back	(BufferSp(new RHSBufferT(inputInts2)));
	spec.outputs.push_back	(BufferSp(new OutBufferT(outputInts)));
	spec.numWorkGroups		= IVec3(numElements, 1, 1);
	spec.failResult			= QP_TEST_RESULT_FAIL;
	spec.failMessage		= "Output doesn't match with expected";

	string qualTestName(getDotProductTestName(inputInfo, packingInfo, outSize));

	group->addChild(new SpvAsmComputeShaderCase(testCtx, qualTestName.data(), "", spec));
}

template <class LHSBufferT, class RHSBufferT, class LHSOperandT, class RHSOperandT>
void addOpSUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name,
							   const struct DotProductPackingInfo dotProductPackingInfo[], unsigned dotProductPackingInfoSize,
							   const struct DotProductVectorInfo dotProductVectorInfo[], unsigned dotProductVectorInfoSize,
							   LHSOperandT lhsVecMin, LHSOperandT lhsVecMax, RHSOperandT rhsVecMin, RHSOperandT rhsVecMax)
{
	const int		numElements	= 200;
	// Note: this test does not currently cover 64-bit integer results
	for (unsigned int j = 0; j < dotProductVectorInfoSize; j++)
	{
		const struct DotProductVectorInfo &vectorInfo = dotProductVectorInfo[j];
		unsigned int alignedVecLen = getAlignedVecLen(vectorInfo);
		struct DotProductInputInfo inputInfo = { name, vectorInfo.vecLen, vectorInfo.vecElementSize };
		vector<LHSOperandT>	inputInts1	(numElements * alignedVecLen, 0);
		vector<RHSOperandT>	inputInts2	(numElements * alignedVecLen, 0);

		fillRandomScalars(rnd, lhsVecMin, lhsVecMax, &inputInts1[0], numElements * alignedVecLen);
		fillRandomScalars(rnd, rhsVecMin, rhsVecMax, &inputInts2[0], numElements * alignedVecLen);

		if (vectorInfo.vecLen == 3)
			for (unsigned int ndx = 0; ndx < numElements; ++ndx)
				inputInts1[ndx*4+3] = inputInts2[ndx*4+3] = 0;


		for (unsigned int i = 0; i < dotProductPackingInfoSize; i++)
		{
			const struct DotProductPackingInfo &packingInfo = dotProductPackingInfo[i];
			if (packingInfo.packed && (vectorInfo.vecElementSize != 8 || vectorInfo.vecLen != 4))
				continue;

			if (vectorInfo.vecElementSize <= 32)
				addOpSUDotKHRComputeTests<LHSBufferT, RHSBufferT, Int32Buffer, deInt32>(testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
			if (vectorInfo.vecElementSize <= 16)
				addOpSUDotKHRComputeTests<LHSBufferT, RHSBufferT, Int16Buffer, deInt16>(testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
			if (vectorInfo.vecElementSize <= 8)
				addOpSUDotKHRComputeTests<LHSBufferT, RHSBufferT, Int8Buffer,  deInt8> (testCtx, group, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo);
		}
	}
}

template <class LHSOperandT, class RHSOperandT>
void add32bitOpSUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, LHSOperandT lhsVecMin, LHSOperandT lhsVecMax, RHSOperandT rhsVecMin, RHSOperandT rhsVecMax)
{
	addOpSUDotKHRComputeTests<Int32Buffer, Uint32Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
														 dotProductVector32, DE_LENGTH_OF_ARRAY(dotProductVector32), lhsVecMin, lhsVecMax, rhsVecMin, rhsVecMax);
}

template <class LHSOperandT, class RHSOperandT>
void add16bitOpSUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, LHSOperandT lhsVecMin, LHSOperandT lhsVecMax, RHSOperandT rhsVecMin, RHSOperandT rhsVecMax)
{
	addOpSUDotKHRComputeTests<Int16Buffer, Uint16Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
														 dotProductVector16, DE_LENGTH_OF_ARRAY(dotProductVector16), lhsVecMin, lhsVecMax, rhsVecMin, rhsVecMax);
}

template <class LHSOperandT, class RHSOperandT>
void add8bitOpSUDotKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, LHSOperandT lhsVecMin, LHSOperandT lhsVecMax, RHSOperandT rhsVecMin, RHSOperandT rhsVecMax)
{
	addOpSUDotKHRComputeTests<Int8Buffer, Uint8Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
													   dotProductVector8, DE_LENGTH_OF_ARRAY(dotProductVector8), lhsVecMin, lhsVecMax, rhsVecMin, rhsVecMax);
}

template <class InBufferT, class AddendBufferT, class AddendT, class OperandT>
void addOpSDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd,
									int numElements, vector<OperandT> &inputInts1, vector<OperandT> &inputInts2,
									const struct DotProductInputInfo &inputInfo, const struct DotProductPackingInfo &packingInfo,
									const struct DotProductVectorInfo &vectorInfo, bool useMaxAddend)
{
	ComputeShaderSpec	spec;
	size_t				addendSize = sizeof(AddendT) * 8;
	vector<AddendT>		inputInts3	(numElements, 0);
	vector<AddendT>		outputInts	(numElements, 0);

	if (useMaxAddend)
		fillRandomScalars(rnd, (AddendT)(std::numeric_limits<AddendT>::max()-20), (AddendT)(std::numeric_limits<AddendT>::max()),
					  &inputInts3[0], numElements);
	else
		fillRandomScalars(rnd, (AddendT)(std::numeric_limits<AddendT>::min()), (AddendT)(std::numeric_limits<AddendT>::min()+20),
					  &inputInts3[0], numElements);

	spec.assembly = generateIntegerDotProductCode(packingInfo, vectorInfo, addendSize, true, true, true);

	addDotProductExtensionAndFeatures(spec, packingInfo, vectorInfo.vecElementSize, addendSize);
	spec.inputs.push_back	(BufferSp(new InBufferT(inputInts1)));
	spec.inputs.push_back	(BufferSp(new InBufferT(inputInts2)));
	spec.inputs.push_back	(BufferSp(new AddendBufferT(inputInts3)));
	spec.outputs.push_back	(BufferSp(new AddendBufferT(outputInts)));
	spec.numWorkGroups		= IVec3(numElements, 1, 1);
	spec.verifyIO			= &compareDotProductAccSat<AddendT, OperandT, OperandT>;
	spec.failResult			= QP_TEST_RESULT_FAIL;
	spec.failMessage		= "Output doesn't match with expected";

	string qualTestName(getDotProductTestName(inputInfo, packingInfo, addendSize));

	group->addChild(new SpvAsmComputeShaderCase(testCtx, qualTestName.data(), "", spec));
}

template <class InBufferT, class T>
void addOpSDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name,
									const struct DotProductPackingInfo dotProductPackingInfo[], unsigned dotProductPackingInfoSize,
									const struct DotProductVectorInfo dotProductVectorInfo[], unsigned dotProductVectorInfoSize,
									T vecMin, T vecMax, bool useMaxAddend)
{
	const int		numElements	= 200;
	// Note: this test does not currently cover 64-bit integer results
	for (unsigned int j = 0 ; j < dotProductVectorInfoSize; j++)
	{
		const struct DotProductVectorInfo &vectorInfo = dotProductVectorInfo[j];
		unsigned int alignedVecLen = getAlignedVecLen(vectorInfo);
		struct DotProductInputInfo inputInfo = { name, vectorInfo.vecLen, vectorInfo.vecElementSize };
		vector<T>		inputInts1	(numElements * alignedVecLen, 0);
		vector<T>		inputInts2	(numElements * alignedVecLen, 0);

		fillRandomScalars(rnd, vecMin, vecMax, &inputInts1[0], numElements * alignedVecLen);
		fillRandomScalars(rnd, vecMin, vecMax, &inputInts2[0], numElements * alignedVecLen);

		if (vectorInfo.vecLen == 3)
			for (unsigned int ndx = 0; ndx < numElements; ++ndx)
				inputInts1[ndx*4+3] = inputInts2[ndx*4+3] = 0;


		for (unsigned int i = 0; i < dotProductPackingInfoSize; i++)
		{
			const struct DotProductPackingInfo &packingInfo = dotProductPackingInfo[i];
			if (packingInfo.packed && (vectorInfo.vecElementSize != 8 || vectorInfo.vecLen != 4))
				continue;

			if (vectorInfo.vecElementSize <= 32)
				addOpSDotAccSatKHRComputeTests<InBufferT, Int32Buffer, deInt32>(testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
			if (vectorInfo.vecElementSize <= 16)
				addOpSDotAccSatKHRComputeTests<InBufferT, Int16Buffer, deInt16>(testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
			if (vectorInfo.vecElementSize <= 8)
				addOpSDotAccSatKHRComputeTests<InBufferT, Int8Buffer,  deInt8> (testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
		}
	}
}

template <class T>
void add32bitOpSDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax, bool useMaxAddend = true)
{
	addOpSDotAccSatKHRComputeTests<Int32Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
												dotProductVector32, DE_LENGTH_OF_ARRAY(dotProductVector32), vecMin, vecMax, useMaxAddend);
}

template <class T>
void add16bitOpSDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax, bool useMaxAddend = true)
{
	addOpSDotAccSatKHRComputeTests<Int16Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
												dotProductVector16, DE_LENGTH_OF_ARRAY(dotProductVector16), vecMin, vecMax, useMaxAddend);
}

template <class T>
void add8bitOpSDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax, bool useMaxAddend = true)
{
	addOpSDotAccSatKHRComputeTests<Int8Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
											   dotProductVector8, DE_LENGTH_OF_ARRAY(dotProductVector8), vecMin, vecMax, useMaxAddend);
}

template <class InBufferT, class AddendBufferT, class AddendT, class OperandT>
void addOpUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd,
									int numElements, vector<OperandT> &inputInts1, vector<OperandT> &inputInts2,
									const struct DotProductInputInfo &inputInfo, const struct DotProductPackingInfo &packingInfo,
									const struct DotProductVectorInfo &vectorInfo, bool useMaxAddend)
{
	ComputeShaderSpec	spec;
	size_t				addendSize = sizeof(AddendT) * 8;
	vector<AddendT>		inputInts3	(numElements, 0);
	vector<AddendT>		outputInts	(numElements, 0);

	if (useMaxAddend)
		fillRandomScalars(rnd, (AddendT)(std::numeric_limits<AddendT>::max()-20), (AddendT)(std::numeric_limits<AddendT>::max()),
					&inputInts3[0], numElements);
	else
		fillRandomScalars(rnd, (AddendT)(std::numeric_limits<AddendT>::min()), (AddendT)(std::numeric_limits<AddendT>::min()+20),
					&inputInts3[0], numElements);

	spec.assembly = generateIntegerDotProductCode(packingInfo, vectorInfo, addendSize, false, false, true);

	addDotProductExtensionAndFeatures(spec, packingInfo, vectorInfo.vecElementSize, addendSize);
	spec.inputs.push_back	(BufferSp(new InBufferT(inputInts1)));
	spec.inputs.push_back	(BufferSp(new InBufferT(inputInts2)));
	spec.inputs.push_back	(BufferSp(new AddendBufferT(inputInts3)));
	spec.outputs.push_back	(BufferSp(new AddendBufferT(outputInts)));
	spec.numWorkGroups		= IVec3(numElements, 1, 1);
	spec.verifyIO			= &compareDotProductAccSat<AddendT, OperandT, OperandT>;
	spec.failResult			= QP_TEST_RESULT_FAIL;
	spec.failMessage		= "Output doesn't match with expected";

	string qualTestName(getDotProductTestName(inputInfo, packingInfo, addendSize));

	group->addChild(new SpvAsmComputeShaderCase(testCtx, qualTestName.data(), "", spec));
}

template <class InBufferT, class T>
void addOpUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name,
									const struct DotProductPackingInfo dotProductPackingInfo[], unsigned dotProductPackingInfoSize,
									const struct DotProductVectorInfo dotProductVectorInfo[], unsigned dotProductVectorInfoSize,
									T vecMin, T vecMax, bool useMaxAddend)
{
	const int		numElements	= 200;
	// Note: this test does not currently cover 64-bit integer results

	for (unsigned int j = 0 ; j < dotProductVectorInfoSize; j++)
	{
		const struct DotProductVectorInfo &vectorInfo = dotProductVectorInfo[j];
		unsigned int alignedVecLen = getAlignedVecLen(vectorInfo);
		struct DotProductInputInfo inputInfo = { name, vectorInfo.vecLen, vectorInfo.vecElementSize };
		vector<T>		inputInts1	(numElements * alignedVecLen, 0);
		vector<T>		inputInts2	(numElements * alignedVecLen, 0);

		fillRandomScalars(rnd, vecMin, vecMax, &inputInts1[0], numElements * alignedVecLen);
		fillRandomScalars(rnd, vecMin, vecMax, &inputInts2[0], numElements * alignedVecLen);

		if (vectorInfo.vecLen == 3)
			for (unsigned int ndx = 0; ndx < numElements; ++ndx)
				inputInts1[ndx*4+3] = inputInts2[ndx*4+3] = 0;

		for (unsigned int i = 0; i < dotProductPackingInfoSize; i++)
		{
			const struct DotProductPackingInfo &packingInfo = dotProductPackingInfo[i];
			if (packingInfo.packed && (vectorInfo.vecElementSize != 8 || vectorInfo.vecLen != 4))
				continue;

			if (vectorInfo.vecElementSize <= 32)
				addOpUDotAccSatKHRComputeTests<InBufferT, Uint32Buffer, deUint32>(testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
			if (vectorInfo.vecElementSize <= 16)
				addOpUDotAccSatKHRComputeTests<InBufferT, Uint16Buffer, deUint16>(testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
			if (vectorInfo.vecElementSize <= 8)
				addOpUDotAccSatKHRComputeTests<InBufferT, Uint8Buffer,  deUint8> (testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
		}
	}
}

template <class T>
void add32bitOpUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax, bool useMaxAddend = true)
{
	addOpUDotAccSatKHRComputeTests<Uint32Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
												 dotProductVector32, DE_LENGTH_OF_ARRAY(dotProductVector32), vecMin, vecMax, useMaxAddend);
}

template <class T>
void add16bitOpUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax, bool useMaxAddend = true)
{
	addOpUDotAccSatKHRComputeTests<Uint16Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
												 dotProductVector16, DE_LENGTH_OF_ARRAY(dotProductVector16), vecMin, vecMax, useMaxAddend);
}

template <class T>
void add8bitOpUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, T vecMin, T vecMax, bool useMaxAddend = true)
{
	addOpUDotAccSatKHRComputeTests<Uint8Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
												dotProductVector8, DE_LENGTH_OF_ARRAY(dotProductVector8), vecMin, vecMax, useMaxAddend);
}

template <class LHSBufferT, class RHSBufferT, class AddendBufferT, class AddendT, class LHSOperandT, class RHSOperandT>
void addOpSUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd,
									 int numElements, vector<LHSOperandT> &inputInts1, vector<RHSOperandT> &inputInts2,
									 const struct DotProductInputInfo &inputInfo, const struct DotProductPackingInfo &packingInfo,
									 const struct DotProductVectorInfo &vectorInfo, bool useMaxAddend)
{
	ComputeShaderSpec	spec;
	size_t				addendSize = sizeof(AddendT) * 8;
	vector<AddendT>		inputInts3	(numElements, 0);
	vector<AddendT>		outputInts	(numElements, 0);

	// Populate the accumulation buffer with large values to attempt to guarantee saturation
	if (useMaxAddend)
		fillRandomScalars(rnd, (AddendT)(std::numeric_limits<AddendT>::max()-20), (AddendT)(std::numeric_limits<AddendT>::max()),
					&inputInts3[0], numElements);
	else
		fillRandomScalars(rnd, (AddendT)(std::numeric_limits<AddendT>::min()), (AddendT)(std::numeric_limits<AddendT>::min()+20),
					&inputInts3[0], numElements);

	spec.assembly = generateIntegerDotProductCode(packingInfo, vectorInfo, addendSize, true, false, true);
	addDotProductExtensionAndFeatures(spec, packingInfo, vectorInfo.vecElementSize, addendSize);
	spec.inputs.push_back	(BufferSp(new LHSBufferT(inputInts1)));
	spec.inputs.push_back	(BufferSp(new RHSBufferT(inputInts2)));
	spec.inputs.push_back	(BufferSp(new AddendBufferT(inputInts3)));
	spec.outputs.push_back	(BufferSp(new AddendBufferT(outputInts)));
	spec.numWorkGroups		= IVec3(numElements, 1, 1);
	spec.verifyIO			= &compareDotProductAccSat<AddendT, LHSOperandT, RHSOperandT>;
	spec.failResult			= QP_TEST_RESULT_FAIL;
	spec.failMessage		= "Output doesn't match with expected";

	string qualTestName(getDotProductTestName(inputInfo, packingInfo, addendSize));

	group->addChild(new SpvAsmComputeShaderCase(testCtx, qualTestName.data(), "", spec));
}

template <class LHSBufferT, class RHSBufferT, class LHSOperandT, class RHSOperandT>
void addOpSUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name,
									 const struct DotProductPackingInfo dotProductPackingInfo[], unsigned dotProductPackingInfoSize,
									 const struct DotProductVectorInfo dotProductVectorInfo[], unsigned dotProductVectorInfoSize,
									 LHSOperandT lhsVecMin, LHSOperandT lhsVecMax, RHSOperandT rhsVecMin, RHSOperandT rhsVecMax, bool useMaxAddend)
{
	const int	numElements	= 200;
	// Note: this test does not currently cover 64-bit integer results

	for (unsigned int j = 0; j < dotProductVectorInfoSize; j++)
	{
		const struct DotProductVectorInfo &vectorInfo = dotProductVectorInfo[j];
		unsigned int alignedVecLen = getAlignedVecLen(vectorInfo);
		struct DotProductInputInfo inputInfo = { name, vectorInfo.vecLen, vectorInfo.vecElementSize };
		vector<LHSOperandT>	inputInts1	(numElements * alignedVecLen, 0);
		vector<RHSOperandT>	inputInts2	(numElements * alignedVecLen, 0);

		fillRandomScalars(rnd, lhsVecMin, lhsVecMax, &inputInts1[0], numElements * alignedVecLen);
		fillRandomScalars(rnd, rhsVecMin, rhsVecMax, &inputInts2[0], numElements * alignedVecLen);

		if (vectorInfo.vecLen == 3)
			for (unsigned int ndx = 0; ndx < numElements; ++ndx)
				inputInts1[ndx*4+3] = inputInts2[ndx*4+3] = 0;

		for (unsigned int i = 0; i < dotProductPackingInfoSize; i++)
		{
			const struct DotProductPackingInfo &packingInfo = dotProductPackingInfo[i];
			if (packingInfo.packed && (vectorInfo.vecElementSize != 8 || vectorInfo.vecLen != 4))
				continue;

			if (vectorInfo.vecElementSize <= 32)
				addOpSUDotAccSatKHRComputeTests<LHSBufferT, RHSBufferT, Int32Buffer, deInt32>(testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
			if (vectorInfo.vecElementSize <= 16)
				addOpSUDotAccSatKHRComputeTests<LHSBufferT, RHSBufferT, Int16Buffer, deInt16>(testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
			if (vectorInfo.vecElementSize <= 8)
				addOpSUDotAccSatKHRComputeTests<LHSBufferT, RHSBufferT, Int8Buffer,  deInt8> (testCtx, group, rnd, numElements, inputInts1, inputInts2, inputInfo, packingInfo, vectorInfo, useMaxAddend);
		}
	}
}

template <class LHSOperandT, class RHSOperandT>
void add32bitOpSUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, LHSOperandT lhsVecMin, LHSOperandT lhsVecMax, RHSOperandT rhsVecMin, RHSOperandT rhsVecMax, bool useMaxAddend = true)
{
    addOpSUDotAccSatKHRComputeTests<Int32Buffer, Uint32Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
															   dotProductVector32, DE_LENGTH_OF_ARRAY(dotProductVector32), lhsVecMin, lhsVecMax, rhsVecMin, rhsVecMax, useMaxAddend);
}

template <class LHSOperandT, class RHSOperandT>
void add16bitOpSUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, LHSOperandT lhsVecMin, LHSOperandT lhsVecMax, RHSOperandT rhsVecMin, RHSOperandT rhsVecMax, bool useMaxAddend = true)
{
	addOpSUDotAccSatKHRComputeTests<Int16Buffer, Uint16Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
															   dotProductVector16, DE_LENGTH_OF_ARRAY(dotProductVector16), lhsVecMin, lhsVecMax, rhsVecMin, rhsVecMax, useMaxAddend);
}

template <class LHSOperandT, class RHSOperandT>
void add8bitOpSUDotAccSatKHRComputeTests(tcu::TestContext& testCtx, tcu::TestCaseGroup *group, de::Random &rnd, string name, LHSOperandT lhsVecMin, LHSOperandT lhsVecMax, RHSOperandT rhsVecMin, RHSOperandT rhsVecMax, bool useMaxAddend = true)
{
	addOpSUDotAccSatKHRComputeTests<Int8Buffer, Uint8Buffer>(testCtx, group, rnd, name, dotProductPacking, DE_LENGTH_OF_ARRAY(dotProductPacking),
															 dotProductVector8, DE_LENGTH_OF_ARRAY(dotProductVector8), lhsVecMin, lhsVecMax, rhsVecMin, rhsVecMax, useMaxAddend);
}

} // anon namespace

tcu::TestCaseGroup* createOpSDotKHRComputeGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "opsdotkhr", "Test the OpSDotKHR instruction"));
	de::Random						rnd		(deStringHash(group->getName()));

	add8bitOpSDotKHRComputeTests(testCtx, group.get(), rnd, string("all"),   std::numeric_limits<deInt8>::min(), std::numeric_limits<deInt8>::max());
	add8bitOpSDotKHRComputeTests(testCtx, group.get(), rnd, string("small"), (deInt8)-20,  (deInt8)20);
	add16bitOpSDotKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deInt16>::min(), std::numeric_limits<deInt16>::max());
	add32bitOpSDotKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deInt32>::min(), std::numeric_limits<deInt32>::max());

	return group.release();
}

tcu::TestCaseGroup* createOpUDotKHRComputeGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "opudotkhr", "Test the OpUDotKHR instruction"));
	de::Random						rnd		(deStringHash(group->getName()));

	add8bitOpUDotKHRComputeTests(testCtx, group.get(), rnd, string("all"),   std::numeric_limits<deUint8>::min(), std::numeric_limits<deUint8>::max());
	add8bitOpUDotKHRComputeTests(testCtx, group.get(), rnd, string("small"), (deUint8)0,  (deUint8)20);
	add16bitOpUDotKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deUint16>::min(), std::numeric_limits<deUint16>::max());
	add32bitOpUDotKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deUint32>::min(), std::numeric_limits<deUint32>::max());

	return group.release();
}

tcu::TestCaseGroup* createOpSUDotKHRComputeGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "opsudotkhr", "Test the OpSUDotKHR instruction"));
	de::Random						rnd		(deStringHash(group->getName()));

	add8bitOpSUDotKHRComputeTests(testCtx, group.get(), rnd, string("all"),   std::numeric_limits<deInt8>::min(), std::numeric_limits<deInt8>::max(), std::numeric_limits<deUint8>::min(), std::numeric_limits<deUint8>::max());
	add8bitOpSUDotKHRComputeTests(testCtx, group.get(), rnd, string("small"), (deInt8)-20,  (deInt8)20,  (deUint8)0, (deUint8)20);
	add16bitOpSUDotKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deInt16>::min(), std::numeric_limits<deInt16>::max(),  std::numeric_limits<deUint16>::min(), std::numeric_limits<deUint16>::max());
	add32bitOpSUDotKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deInt32>::min(), std::numeric_limits<deInt32>::max(),  std::numeric_limits<deUint32>::min(), std::numeric_limits<deUint32>::max());

	return group.release();
}

tcu::TestCaseGroup* createOpSDotAccSatKHRComputeGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "opsdotaccsatkhr", "Test the OpSDotAccSatKHR instruction"));
	de::Random						rnd		(deStringHash(group->getName()));

	add8bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"),   std::numeric_limits<deInt8>::min(), std::numeric_limits<deInt8>::max());
	add8bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deInt8)(12), (deInt8)(20));
	add8bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits-neg"),   (deInt8)(-20), (deInt8)(-12), false);
	add8bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("small"), (deInt8)-20,  (deInt8)20);
	add16bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deInt16>::min(), std::numeric_limits<deInt16>::max());
	add16bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deInt16)(std::numeric_limits<deInt8>::max()-20), (deInt16)(std::numeric_limits<deInt8>::max()+20));
	add16bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits-neg"),   (deInt16)(std::numeric_limits<deInt8>::min()-20), (deInt16)(std::numeric_limits<deInt8>::min()+20), false);
	add32bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deInt32>::min(), std::numeric_limits<deInt32>::max());
	add32bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deInt32)(std::numeric_limits<deInt16>::max()-20), (deInt32)(std::numeric_limits<deInt16>::max()+20));
	add32bitOpSDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits-neg"),   (deInt32)(std::numeric_limits<deInt16>::min()-20), (deInt32)(std::numeric_limits<deInt16>::min()+20), false);

	return group.release();
}

tcu::TestCaseGroup* createOpUDotAccSatKHRComputeGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "opudotaccsatkhr", "Test the OpUDotAccSatKHR instruction"));
	de::Random						rnd		(deStringHash(group->getName()));

	add8bitOpUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"),   std::numeric_limits<deUint8>::min(), std::numeric_limits<deUint8>::max());
	add8bitOpUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deUint8)(12), (deUint8)(20));
	add8bitOpUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("small"), (deUint8)0,  (deUint8)20);
	add16bitOpUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deUint16>::min(), std::numeric_limits<deUint16>::max());
	add16bitOpUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deUint16)(std::numeric_limits<deUint8>::max()-40), (deUint16)(std::numeric_limits<deUint8>::max()-20));
	add32bitOpUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deUint32>::min(), std::numeric_limits<deUint32>::max());
	add32bitOpUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deUint32)(std::numeric_limits<deUint16>::max()-40), (deUint32)(std::numeric_limits<deUint16>::max()-20));

	return group.release();
}

tcu::TestCaseGroup* createOpSUDotAccSatKHRComputeGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "opsudotaccsatkhr", "Test the OpSUDotAccSatKHR instruction"));
	de::Random						rnd		(deStringHash(group->getName()));

	add8bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"),   std::numeric_limits<deInt8>::min(), std::numeric_limits<deInt8>::max(), std::numeric_limits<deUint8>::min(), std::numeric_limits<deUint8>::max());
	add8bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deInt8)(12), (deInt8)(20), (deUint8)(12), (deUint8)(20));
	add8bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits-neg"),   (deInt8)(-20), (deInt8)(-12), (deUint8)(12), (deUint8)(20), false);
	add8bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("small"), (deInt8)-20,  (deInt8)20,  (deUint8)0, (deUint8)20);
	add16bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deInt16>::min(), std::numeric_limits<deInt16>::max(),  std::numeric_limits<deUint16>::min(), std::numeric_limits<deUint16>::max());
	add16bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deInt16)(std::numeric_limits<deInt8>::max()-20), (deInt16)(std::numeric_limits<deInt8>::max()+20), (deUint16)(std::numeric_limits<deUint8>::max()-40), (deUint16)(std::numeric_limits<deUint8>::max()-20));
	add16bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits-neg"),   (deInt16)(std::numeric_limits<deInt8>::min()-20), (deInt16)(std::numeric_limits<deInt8>::min()+20), (deUint16)(std::numeric_limits<deUint8>::max()-40), (deUint16)(std::numeric_limits<deUint8>::max()-20), false);
	add32bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("all"), std::numeric_limits<deInt32>::min(), std::numeric_limits<deInt32>::max(),  std::numeric_limits<deUint32>::min(), std::numeric_limits<deUint32>::max());
	add32bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits"),   (deInt32)(std::numeric_limits<deInt16>::max()-20), (deInt32)(std::numeric_limits<deInt16>::max()+20), (deUint32)(std::numeric_limits<deUint16>::max()-40), (deUint32)(std::numeric_limits<deUint16>::max()-20));
	add32bitOpSUDotAccSatKHRComputeTests(testCtx, group.get(), rnd, string("limits-neg"),   (deInt32)(std::numeric_limits<deInt16>::min()-20), (deInt32)(std::numeric_limits<deInt16>::min()+20), (deUint32)(std::numeric_limits<deUint16>::max()-40), (deUint32)(std::numeric_limits<deUint16>::max()-20), false);

	return group.release();
}

} // SpirVAssembly
} // vkt
