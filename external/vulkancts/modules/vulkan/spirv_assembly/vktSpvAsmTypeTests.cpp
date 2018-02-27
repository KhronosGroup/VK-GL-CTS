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

#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"

#include "deStringUtil.hpp"

#include "vktSpvAsmGraphicsShaderTestUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include <cmath>

#define TEST_DATASET_SIZE 10

#define UNDEFINED_SPIRV_TEST_TYPE "testtype"

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

template <class T>
class SpvAsmTypeTests : public tcu::TestCaseGroup
{
public:
	typedef T		(*OpUnaryFuncType)			(T);
	typedef T		(*OpBinaryFuncType)			(T, T);
	typedef T		(*OpTernaryFuncType)		(T, T, T);
	typedef bool	(*UnaryFilterFuncType)		(T);
	typedef bool	(*BinaryFilterFuncType)		(T, T);
	typedef bool	(*TernaryFilterFuncType)	(T, T, T);
					SpvAsmTypeTests				(tcu::TestContext&	testCtx,
												 const char*		name,
												 const char*		description,
												 const char*		deviceFeature,
												 const char*		spirvCapability,
												 const char*		spirvType,
												 deUint32			typeSize,
												 deUint32			vectorSize);
					~SpvAsmTypeTests			(void);
	void			createTests					(const char*			testName,
												 const char*			spirvOperation,
												 OpUnaryFuncType		op,
												 UnaryFilterFuncType	filter,
												 const char*			spirvExtension	= DE_NULL);
	void			createTests					(const char*			testName,
												 const char*			spirvOperation,
												 OpBinaryFuncType		op,
												 BinaryFilterFuncType	filter,
												 const char*			spirvExtension	= DE_NULL);
	void			createTests					(const char*			testName,
												 const char*			spirvOperation,
												 OpTernaryFuncType		op,
												 TernaryFilterFuncType	filter,
												 const char*			spirvExtension	= DE_NULL);
	virtual void	getDataset					(vector<T>& input, deUint32 numElements)		= 0;
	virtual void	pushResource				(vector<Resource>& resource, vector<T>&	data)	= 0;

	static bool		filterNone					(T a);
	static bool		filterNone					(T a, T b);
	static bool		filterNone					(T a, T b, T c);
	static bool		filterZero					(T a, T b);
	static bool		filterNegativesAndZero		(T a, T b);
	static bool		filterMinGtMax				(T, T a, T b);

protected:
	de::Random	m_rnd;

private:
	std::string	createInputDecoration			(deUint32						numInput);
	std::string	createInputPreMain				(deUint32						numInput);
	std::string	createInputTestfun				(deUint32						numInput);
	deUint32	combine							(GraphicsResources&				resources,
												 vector<T>&						data,
												 OpUnaryFuncType				operation,
												 UnaryFilterFuncType			filter);
	deUint32	combine							(GraphicsResources&				resources,
												 vector<T>&						data,
												 OpBinaryFuncType				operation,
												 BinaryFilterFuncType			filter);
	deUint32	combine							(GraphicsResources&				resources,
												 vector<T>&						data,
												 OpTernaryFuncType				operation,
												 TernaryFilterFuncType			filter);
	void		createStageTests				(const char*					testName,
												 const char*					spirvOperation,
												 GraphicsResources&				resources,
												 deUint32						numElements,
												 vector<string>&				decorations,
												 vector<string>&				pre_mains,
												 vector<string>&				testfuns,
												 string&						operation,
												 const char*					spirvExtension	= DE_NULL);

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
									 deUint32			typeSize,
									 deUint32			vectorSize)
	: tcu::TestCaseGroup	(testCtx, name, description)
	, m_rnd					(deStringHash(name))
	, m_deviceFeature		(deviceFeature)
	, m_spirvCapability		(spirvCapability)
	, m_spirvType			(spirvType)
	, m_typeSize			(typeSize)
	, m_vectorSize			(vectorSize)
{
	std::string scalarType;

	DE_ASSERT(vectorSize >= 1 && vectorSize <= 4);

	if (deStringEqual("OpTypeInt 32 1", m_spirvType))
		scalarType = "i32";
	else if (deStringEqual("OpTypeInt 32 0", m_spirvType))
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
			m_spirvTestType = "v" + numberToString(m_vectorSize) + scalarType;
		else
			m_spirvTestType = scalarType;
	}
}

template <class T>
SpvAsmTypeTests<T>::~SpvAsmTypeTests	(void)
{
}

template <class T>
std::string SpvAsmTypeTests<T>::createInputDecoration	(deUint32 numInput)
{
	const StringTemplate	decoration	("OpDecorate %input${n_input} DescriptorSet 0\n"
										 "OpDecorate %input${n_input} Binding ${n_input}\n");
	map<string, string>		specs;

	specs["n_input"] = numberToString(numInput);

	return decoration.specialize(specs);
}

template <class T>
std::string SpvAsmTypeTests<T>::createInputPreMain	(deUint32 numInput)
{
	return "%input" + numberToString(numInput) + " = OpVariable %bufptr Uniform\n";
}

template <class T>
std::string SpvAsmTypeTests<T>::createInputTestfun	(deUint32 numInput)
{
	const StringTemplate	testfun	("%input${n_input}_loc = OpAccessChain %up_testtype %input${n_input} "
									 "%c_i32_0 %counter_val\n"
									 "%input${n_input}_val = OpLoad %${testtype} %input${n_input}_loc\n");
	map<string, string>		specs;

	specs["n_input"] = numberToString(numInput);
	specs["testtype"] = m_spirvTestType;

	return testfun.specialize(specs);
}

template <class T>
deUint32 SpvAsmTypeTests<T>::combine	(GraphicsResources&		resources,
										 vector<T>&				data,
										 OpUnaryFuncType		operation,
										 UnaryFilterFuncType	filter)
{
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

	return outputsSize / sizeWithPadding;
}

template <class T>
deUint32 SpvAsmTypeTests<T>::combine	(GraphicsResources&		resources,
										 vector<T>&				data,
										 OpBinaryFuncType		operation,
										 BinaryFilterFuncType	filter)
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
			inputs0.push_back(data[elemNdx1]);
			inputs1.push_back(data[elemNdx2]);
			outputs.push_back(operation(data[elemNdx1], data[elemNdx2]));
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

	return outputsSize / sizeWithPadding;
}

template <class T>
deUint32 SpvAsmTypeTests<T>::combine	(GraphicsResources&		resources,
										 vector<T>&				data,
										 OpTernaryFuncType		operation,
										 TernaryFilterFuncType	filter)
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

	/* According to spec, a three-component vector, with components of size N,
	   has a base alignment of 4 N */
	for (deUint32 elemNdx1 = 0; elemNdx1 < datasize; ++elemNdx1)
	for (deUint32 elemNdx2 = 0; elemNdx2 < datasize; ++elemNdx2)
	for (deUint32 elemNdx3 = 0; elemNdx3 < datasize; ++elemNdx3)
	{
		if (filter(data[elemNdx1], data[elemNdx2], data[elemNdx3]))
		{
			inputs0.push_back(data[elemNdx1]);
			inputs1.push_back(data[elemNdx2]);
			inputs2.push_back(data[elemNdx3]);
			outputs.push_back(operation(data[elemNdx1], data[elemNdx2], data[elemNdx3]));
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

	return outputsSize / sizeWithPadding;
}

template <class T>
void SpvAsmTypeTests<T>::createStageTests	(const char*			testName,
											 const char*			spirvOperation,
											 GraphicsResources&		resources,
											 deUint32				numElements,
											 vector<string>&		decorations,
											 vector<string>&		pre_mains,
											 vector<string>&		testfuns,
											 string&				operation,
											 const char*			spirvExtension)
{
	const StringTemplate		decoration		("OpDecorate %output DescriptorSet 0\n"
												 "OpDecorate %output Binding ${output_binding}\n"
												 "OpDecorate %a${num_elements}testtype ArrayStride ${typesize}\n"
												 "OpDecorate %buf BufferBlock\n"
												 "OpMemberDecorate %buf 0 Offset 0\n");

	const StringTemplate		pre_pre_main	("%c_u32_${num_elements} = OpConstant %u32 ${num_elements}\n"
												 "%c_i32_${num_elements} = OpConstant %i32 ${num_elements}\n");

	const StringTemplate		scalar_pre_main	("%testtype = ${scalartype}\n");

	const StringTemplate		vector_pre_main	("%scalartype = ${scalartype}\n"
												 "%testtype = OpTypeVector %scalartype ${vector_size}\n");

	const StringTemplate		post_pre_main	("%a${num_elements}testtype = OpTypeArray %${testtype} "
												 "%c_u32_${num_elements}\n"
												 "%up_testtype = OpTypePointer Uniform %${testtype}\n"
												 "%buf = OpTypeStruct %a${num_elements}testtype\n"
												 "%bufptr = OpTypePointer Uniform %buf\n"
												 "%output = OpVariable %bufptr Uniform\n");

	const StringTemplate		pre_testfun		("%test_code = OpFunction %v4f32 None %v4f32_function\n"
												 "%param = OpFunctionParameter %v4f32\n"
												 "%entry = OpLabel\n"
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

	std::vector<std::string>	noExtensions;
	std::vector<std::string>	features;
	RGBA						defaultColors[4];
	map<string, string>			fragments;
	map<string, string>			specs;
	VulkanFeatures				requiredFeatures;

	getDefaultColors(defaultColors);

	if (m_vectorSize == 3)
		resources.verifyIO = verifyVec3Result;
	else
		resources.verifyIO = verifyDefaultResult;

	// All of the following tests write their results into an output SSBO, therefore they require the following features.
	requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
	requiredFeatures.coreFeatures.fragmentStoresAndAtomics = DE_TRUE;

	if (m_deviceFeature)
		features.insert(features.begin(), m_deviceFeature);

	specs["testtype"] = m_spirvTestType;
	specs["scalartype"] = m_spirvType;
	specs["typesize"] = numberToString(((m_vectorSize == 3) ? 4 : m_vectorSize) * m_typeSize / 8);
	specs["vector_size"] = numberToString(m_vectorSize);
	specs["operation"] = spirvOperation;
	specs["num_elements"] = numberToString(numElements);
	specs["output_binding"] = numberToString(resources.inputs.size());

	if (spirvExtension)
		fragments["extension"] = "%ext1 = OpExtInstImport \"" + string(spirvExtension) + "\"";

	for (deUint32 elemNdx = 0; elemNdx < decorations.size(); ++elemNdx)
		fragments["decoration"] += decorations[elemNdx];
	fragments["decoration"] += decoration.specialize(specs);

	fragments["pre_main"] = pre_pre_main.specialize(specs);
	if (specs["testtype"].compare(UNDEFINED_SPIRV_TEST_TYPE) == 0)
	{
		if (m_vectorSize > 1)
			fragments["pre_main"] += vector_pre_main.specialize(specs);
		else
			fragments["pre_main"] += scalar_pre_main.specialize(specs);
	}

	fragments["pre_main"] += post_pre_main.specialize(specs);
	for (deUint32 elemNdx = 0; elemNdx < pre_mains.size(); ++elemNdx)
		fragments["pre_main"] += pre_mains[elemNdx];

	fragments["testfun"] = pre_testfun.specialize(specs);
	for (deUint32 elemNdx = 0; elemNdx < testfuns.size(); ++elemNdx)
		fragments["testfun"] += testfuns[elemNdx];
	fragments["testfun"] += operation + post_testfun.specialize(specs);

	if (m_spirvCapability)
		fragments["capability"] = "OpCapability " + string(m_spirvCapability);

	createTestsForAllStages(testName, defaultColors, defaultColors, fragments, resources, noExtensions, features, this, requiredFeatures);
}

template <class T>
bool SpvAsmTypeTests<T>::verifyResult	(const vector<Resource>&		inputs,
										 const vector<AllocationSp>&	outputAllocations,
										 const vector<Resource>&		expectedOutputs,
										 deUint32						skip,
										 tcu::TestLog&					log)
{
	DE_ASSERT(outputAllocations.size() == 1);
	DE_ASSERT(inputs.size() > 0 && inputs.size() < 4);

	const T*		input[3]		= { DE_NULL };
	vector<deUint8>	inputBytes[3];
	vector<deUint8>	expectedBytes;

	expectedOutputs[0].second->getBytes(expectedBytes);
	const deUint32 count	= static_cast<deUint32>(expectedBytes.size() / sizeof(T));
	const T* obtained		= static_cast<const T *>(outputAllocations[0]->getHostPtr());
	const T* expected		= reinterpret_cast<const T*>(&expectedBytes.front());

	for (deUint32 ndxCount = 0; ndxCount < inputs.size(); ndxCount++)
	{
		inputs[ndxCount].second->getBytes(inputBytes[ndxCount]);
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
				inputStream << input[ndxIndex][ndxCount];
				if (ndxIndex < inputs.size() - 1)
					inputStream << ",";
			}
			inputStream << ")";
			log << tcu::TestLog::Message
				<< "Error: found unexpected result for inputs " << inputStream.str()
				<< ": expected " << expected[ndxCount] << ", obtained "
				<< obtained[ndxCount] << tcu::TestLog::EndMessage;
			return false;
		}
	}

	return true;
}

template <class T>
bool SpvAsmTypeTests<T>::verifyDefaultResult	(const vector<Resource>&		inputs,
												 const vector<AllocationSp>&	outputAllocations,
												 const vector<Resource>&		expectedOutputs,
												 tcu::TestLog&					log)
{
	return verifyResult(inputs, outputAllocations, expectedOutputs, ~0, log);
}

template <class T>
bool SpvAsmTypeTests<T>::verifyVec3Result	(const vector<Resource>&		inputs,
											 const vector<AllocationSp>&	outputAllocations,
											 const vector<Resource>&		expectedOutputs,
											 tcu::TestLog&					log)
{
	return verifyResult(inputs, outputAllocations, expectedOutputs, 4, log);
}

template <class T>
void SpvAsmTypeTests<T>::createTests	(const char*			testName,
										 const char*			spirvOperation,
										 OpUnaryFuncType		operation,
										 UnaryFilterFuncType	filter,
										 const char*			spirvExtension)
{
	vector<T>			dataset;
	vector<string>		decorations;
	vector<string>		pre_mains;
	vector<string>		testfuns;
	GraphicsResources	resources;
	map<string, string>	fragments;
	map<string, string>	specs;

	dataset.reserve(TEST_DATASET_SIZE * m_vectorSize);
	getDataset(dataset, TEST_DATASET_SIZE * m_vectorSize);
	deUint32 totalElements = combine(resources, dataset, operation, filter);

	decorations.reserve(1);
	pre_mains.reserve(1);
	testfuns.reserve(1);

	decorations.push_back(createInputDecoration(0));
	pre_mains.push_back(createInputPreMain(0));
	testfuns.push_back(createInputTestfun(0));

	string	full_operation	(spirvExtension ? "%op_result = OpExtInst %" + m_spirvTestType + " %ext1 " + string(spirvOperation) + " %input0_val\n"
							                : "%op_result = " + string(spirvOperation) + " %" + m_spirvTestType + " %input0_val\n");

	createStageTests(testName, spirvOperation, resources, totalElements, decorations,
					 pre_mains, testfuns, full_operation, spirvExtension);
}

template <class T>
void SpvAsmTypeTests<T>::createTests	(const char*			testName,
										 const char*			spirvOperation,
										 OpBinaryFuncType		operation,
										 BinaryFilterFuncType	filter,
										 const char*			spirvExtension)
{
	vector<T>			dataset;
	vector<string>		decorations;
	vector<string>		pre_mains;
	vector<string>		testfuns;
	GraphicsResources	resources;
	map<string, string>	fragments;
	map<string, string>	specs;

	dataset.reserve(TEST_DATASET_SIZE * m_vectorSize);
	getDataset(dataset, TEST_DATASET_SIZE * m_vectorSize);
	deUint32 totalElements = combine(resources, dataset, operation, filter);

	decorations.reserve(2);
	pre_mains.reserve(2);
	testfuns.reserve(2);

	for (deUint32 elemNdx = 0; elemNdx < 2; ++elemNdx)
	{
		decorations.push_back(createInputDecoration(elemNdx));
		pre_mains.push_back(createInputPreMain(elemNdx));
		testfuns.push_back(createInputTestfun(elemNdx));
	}

	string	full_operation	(spirvExtension ? "%op_result = OpExtInst %" + m_spirvTestType + " %ext1 " + string(spirvOperation) + " %input0_val %input1_val\n"
							                : "%op_result = " + string(spirvOperation) + " %" + m_spirvTestType + " %input0_val %input1_val\n");

	createStageTests(testName, spirvOperation, resources, totalElements, decorations,
					 pre_mains, testfuns, full_operation, spirvExtension);
}

template <class T>
void SpvAsmTypeTests<T>::createTests	(const char*			testName,
										 const char*			spirvOperation,
										 OpTernaryFuncType		operation,
										 TernaryFilterFuncType	filter,
										 const char*			spirvExtension)
{
	vector<T>			dataset;
	vector<string>		decorations;
	vector<string>		pre_mains;
	vector<string>		testfuns;
	GraphicsResources	resources;
	map<string, string>	fragments;
	map<string, string>	specs;

	dataset.reserve(TEST_DATASET_SIZE * m_vectorSize);
	getDataset(dataset, TEST_DATASET_SIZE * m_vectorSize);
	deUint32 totalElements = combine(resources, dataset, operation, filter);

	decorations.reserve(3);
	pre_mains.reserve(3);
	testfuns.reserve(3);

	for (deUint32 elemNdx = 0; elemNdx < 3; ++elemNdx)
	{
		decorations.push_back(createInputDecoration(elemNdx));
		pre_mains.push_back(createInputPreMain(elemNdx));
		testfuns.push_back(createInputTestfun(elemNdx));
	}

	string	full_operation	(spirvExtension ? "%op_result = OpExtInst %" + m_spirvTestType + " %ext1 " + string(spirvOperation) + " %input0_val %input1_val %input2_val\n"
										    : "%op_result = " + string(spirvOperation) + " %" + m_spirvTestType + " %input0_val %input1_val %input2_val\n");

	createStageTests(testName, spirvOperation, resources, totalElements, decorations,
					 pre_mains, testfuns, full_operation, spirvExtension);
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

class SpvAsmTypeInt32Tests : public SpvAsmTypeTests<deInt32>
{
public:
				SpvAsmTypeInt32Tests	(tcu::TestContext&	testCtx,
										 deUint32			vectorSize);
				~SpvAsmTypeInt32Tests	(void);
	void		getDataset				(vector<deInt32>&	input,
										 deUint32			numElements);
	void		pushResource			(vector<Resource>&	resource,
										 vector<deInt32>&	data);
};

SpvAsmTypeInt32Tests::SpvAsmTypeInt32Tests	(tcu::TestContext&	testCtx,
											 deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "i32", "int32 tests", DE_NULL, DE_NULL, "OpTypeInt 32 1", 32, vectorSize)
{
}

SpvAsmTypeInt32Tests::~SpvAsmTypeInt32Tests (void)
{
}

void SpvAsmTypeInt32Tests::getDataset	(vector<deInt32>&	input,
										 deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);
	input.push_back(deIntMinValue32(32) + 1); // So MIN = -MAX
	input.push_back(deIntMaxValue32(32));
	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(static_cast<deInt32>(m_rnd.getUint32()));
}

void SpvAsmTypeInt32Tests::pushResource	(vector<Resource>&	resource,
										 vector<deInt32>&	data)
{
	resource.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					  BufferSp(new Int32Buffer(data))));
}

class SpvAsmTypeInt64Tests : public SpvAsmTypeTests<deInt64>
{
public:
				SpvAsmTypeInt64Tests	(tcu::TestContext&	testCtx,
										 deUint32			vectorSize);
				~SpvAsmTypeInt64Tests	(void);
	void		getDataset				(vector<deInt64>&	input,
										 deUint32			numElements);
	void		pushResource			(vector<Resource>&	resource,
										 vector<deInt64>&	data);
};

SpvAsmTypeInt64Tests::SpvAsmTypeInt64Tests	(tcu::TestContext&	testCtx,
											 deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "i64", "int64 tests", "shaderInt64", "Int64", "OpTypeInt 64 1", 64, vectorSize)
{
}

SpvAsmTypeInt64Tests::~SpvAsmTypeInt64Tests (void)
{
}

void SpvAsmTypeInt64Tests::getDataset	(vector<deInt64>&	input,
										 deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);
	input.push_back(0xFFFF859A3BF78592);// A 64-bit negative number
	input.push_back(0x7FFF859A3BF78592);// A 64-bit positive number
	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(static_cast<deInt64>(m_rnd.getUint64()));
}

void SpvAsmTypeInt64Tests::pushResource	(vector<Resource>&	resource,
										 vector<deInt64>&	data)
{
	resource.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					   BufferSp(new Int64Buffer(data))));
}

class SpvAsmTypeUint32Tests : public SpvAsmTypeTests<deUint32>
{
public:
				SpvAsmTypeUint32Tests	(tcu::TestContext&	testCtx,
										 deUint32			vectorSize);
				~SpvAsmTypeUint32Tests	(void);
	void		getDataset				(vector<deUint32>&	input,
										 deUint32			numElements);
	void		pushResource			(vector<Resource>&	resource,
										 vector<deUint32>&	data);
};

SpvAsmTypeUint32Tests::SpvAsmTypeUint32Tests	(tcu::TestContext&	testCtx,
												 deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "u32", "uint32 tests", DE_NULL, DE_NULL, "OpTypeInt 32 0", 32, vectorSize)
{
}

SpvAsmTypeUint32Tests::~SpvAsmTypeUint32Tests (void)
{
}

void SpvAsmTypeUint32Tests::getDataset	(vector<deUint32>&	input,
										 deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);  // Min value
	input.push_back(~0); // Max value
	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(m_rnd.getUint32());
}

void SpvAsmTypeUint32Tests::pushResource	(vector<Resource>&	resource,
											 vector<deUint32>&	data)
{
	resource.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					   BufferSp(new Uint32Buffer(data))));
}

class SpvAsmTypeUint64Tests : public SpvAsmTypeTests<deUint64>
{
public:
				SpvAsmTypeUint64Tests	(tcu::TestContext&	testCtx,
										 deUint32			vectorSize);
				~SpvAsmTypeUint64Tests	(void);
	void		getDataset				(vector<deUint64>&	input,
										 deUint32			numElements);
	void		pushResource			(vector<Resource>&	resource,
										 vector<deUint64>&	data);
};

SpvAsmTypeUint64Tests::SpvAsmTypeUint64Tests	(tcu::TestContext&	testCtx,
												 deUint32			vectorSize)
	: SpvAsmTypeTests	(testCtx, "u64", "uint64 tests", "shaderInt64", "Int64", "OpTypeInt 64 0", 64, vectorSize)
{
}

SpvAsmTypeUint64Tests::~SpvAsmTypeUint64Tests (void)
{
}

void SpvAsmTypeUint64Tests::getDataset	(vector<deUint64>&	input,
										 deUint32			numElements)
{
	// Push first special cases
	input.push_back(0);  // Min value
	input.push_back(~0); // Max value
	numElements -= static_cast<deUint32>(input.size());

	// Random values
	for (deUint32 elemNdx = 0; elemNdx < numElements; ++elemNdx)
		input.push_back(m_rnd.getUint64());
}

void SpvAsmTypeUint64Tests::pushResource	(vector<Resource>&	resource,
											 vector<deUint64>&	data)
{
	resource.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					   BufferSp(new Uint64Buffer(data))));
}

template <class T>
class TestMath
{
public:
	static inline T	test_abs	(T x)
	{
		T t0 = static_cast<T>(0.0);

		if (x >= t0)
			return x;
		else
			return test_negate(x);
	}

	static inline T	test_add	(T x, T y)
	{
		return x + y;
	}

	static inline T	test_clamp	(T x, T minVal, T maxVal)
	{
		return test_min(test_max(x, minVal), maxVal);
	}

	static inline T	test_div	(T x, T y)
	{
		// In SPIR-V, if "y" is 0, then the result is undefined. In our case,
		// let's return 0
		if (y == static_cast<T>(0))
			return 0;
		else
			return static_cast<T>(x / y);
	}

	static inline T	test_lsb	(T x)
	{
		for (deUint32 i = 0; i < 8 * sizeof(T); i++)
		{
			if (x & (1u << i))
				return static_cast<T>(i);
		}

		return static_cast<T>(-1.0);
	}

	static inline T	test_max	(T x, T y)
	{
		if (x < y)
			return y;
		else
			return x;
	}

	static inline T	test_min	(T x, T y)
	{
		if (y < x)
			return y;
		else
			return x;
	}

	static inline T	test_mod	(T x, T y)
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

		return (x - y * (x / y)) * sign_y * sign_x;
	}

	static inline T	test_mul	(T x, T y)
	{
		return x * y;
	}

	static inline T	test_negate	(T x)
	{
		return static_cast<T>(0.0) - x;
	}

	static inline T	test_rem	(T x, T y)
	{
		// In SPIR-V, if "y" is 0, then the result is undefined. In our case,
		// let's return 0
		if (y == static_cast<T>(0))
			return 0;

		return x % y;
	}

	static inline T	test_sign	(T x)
	{
		T t0 = static_cast<T>(0.0);

		if (x > t0)
			return static_cast<T>(1.0);
		else if (x < t0)
			return static_cast<T>(-1.0);
		else
			return t0;
	}

	static inline T	test_sub	(T x, T y)
	{
		return x - y;
	}

	static inline T	test_msb	(T)
	{
		TCU_THROW(InternalError, "Not implemented");
	}
};

class TestMathInt32 : public TestMath<deInt32>
{
public:
	static inline deInt32	test_msb	(deInt32 x)
	{
		if (x > 0)
			return 31 - deClz32((deUint32)x);
		else if (x < 0)
			return 31 - deClz32(~(deUint32)x);
		else
			return -1;
	}
};

class TestMathInt64 : public TestMath<deInt64>
{
};

class TestMathUint32 : public TestMath<deUint32>
{
public:
	static inline deUint32	test_msb	(deUint32 x)
	{
		if (x > 0)
			return 31 - deClz32(x);
		else
			return -1;
	}
};

class TestMathUint64 : public TestMath<deUint64>
{
};

#define I32_FILTER_NONE SpvAsmTypeInt32Tests::filterNone
#define I64_FILTER_NONE SpvAsmTypeInt64Tests::filterNone
#define U32_FILTER_NONE SpvAsmTypeUint32Tests::filterNone
#define U64_FILTER_NONE SpvAsmTypeUint64Tests::filterNone

#define I32_FILTER_ZERO SpvAsmTypeInt32Tests::filterZero
#define I64_FILTER_ZERO SpvAsmTypeInt64Tests::filterZero
#define U32_FILTER_ZERO SpvAsmTypeUint32Tests::filterZero
#define U64_FILTER_ZERO SpvAsmTypeUint64Tests::filterZero

#define I32_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeInt32Tests::filterNegativesAndZero
#define I64_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeInt64Tests::filterNegativesAndZero
#define U32_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeUint32Tests::filterNegativesAndZero
#define U64_FILTER_NEGATIVES_AND_ZERO SpvAsmTypeUint64Tests::filterNegativesAndZero

#define I32_FILTER_MIN_GT_MAX SpvAsmTypeInt32Tests::filterMinGtMax
#define I64_FILTER_MIN_GT_MAX SpvAsmTypeInt64Tests::filterMinGtMax
#define U32_FILTER_MIN_GT_MAX SpvAsmTypeUint32Tests::filterMinGtMax
#define U64_FILTER_MIN_GT_MAX SpvAsmTypeUint64Tests::filterMinGtMax

// Macro to create tests.
// Syntax: MAKE_TEST_{S,V}_{I,U}_{3,6}
//
//  'S': create scalar test
//  'V': create vector test
//
//  'I': create integer test
//  'U': create unsigned integer test
//
//  '3': create 32-bit test
//  '6': create 64-bit test

#define MAKE_TEST_SV_I_36(name, spirvOp, op, filter, extension)  \
	for (deUint32 ndx = 0; ndx < 4; ++ndx)	\
	{	\
		int32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, (extension)); \
		int64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt64::test_##op, I64_##filter, (extension)); \
	}

#define MAKE_TEST_SV_I_3(name, spirvOp, op, filter, extension)   \
	for (deUint32 ndx = 0; ndx < 4; ++ndx)	\
		int32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathInt32::test_##op, I32_##filter, (extension));

#define MAKE_TEST_SV_U_36(name, spirvOp, op, filter, extension)  \
	for (deUint32 ndx = 0; ndx < 4; ++ndx)	\
	{	\
		uint32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, (extension));	\
		uint64Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint64::test_##op, U64_##filter, (extension));	\
	}

#define MAKE_TEST_SV_U_3(name, spirvOp, op, filter, extension)   \
	for (deUint32 ndx = 0; ndx < 4; ++ndx)	\
		uint32Tests[ndx]->createTests((name), (spirvOp), \
			TestMathUint32::test_##op, U32_##filter, (extension));

tcu::TestCaseGroup* createTypeTests	(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		typeTests			(new tcu::TestCaseGroup(testCtx, "type", "Test types"));
	de::MovePtr<tcu::TestCaseGroup>		typeScalarTests		(new tcu::TestCaseGroup(testCtx, "scalar", "scalar tests"));
	de::MovePtr<tcu::TestCaseGroup>		typeVectorTests[3];

	de::MovePtr<SpvAsmTypeInt32Tests>	int32Tests[4];
	de::MovePtr<SpvAsmTypeInt64Tests>	int64Tests[4];
	de::MovePtr<SpvAsmTypeUint32Tests>	uint32Tests[4];
	de::MovePtr<SpvAsmTypeUint64Tests>	uint64Tests[4];

	for (deUint32 ndx = 0; ndx < 3; ++ndx)
	{
		std::string testName = "vec" + numberToString(ndx + 2);
		typeVectorTests[ndx] = de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, testName.c_str(), "vector tests"));
	}

	for (deUint32 ndx = 0; ndx < 4; ++ndx)
	{
		int32Tests[ndx]		= de::MovePtr<SpvAsmTypeInt32Tests>(new SpvAsmTypeInt32Tests(testCtx, ndx + 1));
		int64Tests[ndx]		= de::MovePtr<SpvAsmTypeInt64Tests>(new SpvAsmTypeInt64Tests(testCtx, ndx + 1));
		uint32Tests[ndx]	= de::MovePtr<SpvAsmTypeUint32Tests>(new SpvAsmTypeUint32Tests(testCtx, ndx + 1));
		uint64Tests[ndx]	= de::MovePtr<SpvAsmTypeUint64Tests>(new SpvAsmTypeUint64Tests(testCtx, ndx + 1));
	}

	MAKE_TEST_SV_I_36("negate", "OpSNegate", negate, FILTER_NONE, DE_NULL);
	MAKE_TEST_SV_I_36("add", "OpIAdd", add, FILTER_NONE, DE_NULL);
	MAKE_TEST_SV_I_36("sub", "OpISub", sub, FILTER_NONE, DE_NULL);
	MAKE_TEST_SV_I_36("mul", "OpIMul", mul, FILTER_NONE, DE_NULL);
	MAKE_TEST_SV_I_36("div", "OpSDiv", div, FILTER_ZERO, DE_NULL);
	MAKE_TEST_SV_U_36("div", "OpUDiv", div, FILTER_ZERO, DE_NULL);
	MAKE_TEST_SV_I_36("rem", "OpSRem", rem, FILTER_NEGATIVES_AND_ZERO, DE_NULL);
	MAKE_TEST_SV_I_36("mod", "OpSMod", mod, FILTER_NEGATIVES_AND_ZERO, DE_NULL);
	MAKE_TEST_SV_U_36("mod", "OpUMod", mod, FILTER_ZERO, DE_NULL);
	MAKE_TEST_SV_I_36("abs", "SAbs", abs, FILTER_NONE, "GLSL.std.450");
	MAKE_TEST_SV_I_36("sign", "SSign", sign, FILTER_NONE, "GLSL.std.450");
	MAKE_TEST_SV_I_36("min", "SMin", min, FILTER_NONE, "GLSL.std.450");
	MAKE_TEST_SV_U_36("min", "UMin", min, FILTER_NONE, "GLSL.std.450");
	MAKE_TEST_SV_I_36("max", "SMax", max, FILTER_NONE, "GLSL.std.450");
	MAKE_TEST_SV_U_36("max", "UMax", max, FILTER_NONE, "GLSL.std.450");
	MAKE_TEST_SV_I_36("clamp", "SClamp", clamp, FILTER_MIN_GT_MAX, "GLSL.std.450");
	MAKE_TEST_SV_U_36("clamp", "UClamp", clamp, FILTER_MIN_GT_MAX, "GLSL.std.450");
	MAKE_TEST_SV_I_3("find_lsb", "FindILsb", lsb, FILTER_NONE, "GLSL.std.450");
	MAKE_TEST_SV_I_3("find_msb", "FindSMsb", msb, FILTER_NONE, "GLSL.std.450");
	MAKE_TEST_SV_U_3("find_msb", "FindUMsb", msb, FILTER_NONE, "GLSL.std.450");

	typeScalarTests->addChild(int32Tests[0].release());
	typeScalarTests->addChild(int64Tests[0].release());
	typeScalarTests->addChild(uint32Tests[0].release());
	typeScalarTests->addChild(uint64Tests[0].release());

	typeTests->addChild(typeScalarTests.release());

	for (deUint32 ndx = 0; ndx < 3; ++ndx)
	{
		typeVectorTests[ndx]->addChild(int32Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(int64Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(uint32Tests[ndx + 1].release());
		typeVectorTests[ndx]->addChild(uint64Tests[ndx + 1].release());

		typeTests->addChild(typeVectorTests[ndx].release());
	}

	return typeTests.release();
}

} // SpirVAssembly
} // vkt
