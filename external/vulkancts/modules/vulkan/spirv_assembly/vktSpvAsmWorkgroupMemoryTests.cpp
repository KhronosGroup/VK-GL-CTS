/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 * \brief SPIR-V assembly tests for workgroup memory.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmWorkgroupMemoryTests.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuFloat.hpp"

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::map;
using std::string;
using std::vector;
using tcu::IVec3;
using tcu::Vec4;
using tcu::StringTemplate;
using tcu::Float16;
using tcu::Float32;

namespace
{

struct DataType
{
	string		name;
	string		type;
	deUint32	sizeBytes;
};

bool checkResultsFloat16 (const vector<Resource>&		inputs,
						  const vector<AllocationSp>&	outputAllocs,
						  const vector<Resource>&		expectedOutputs,
						  tcu::TestLog&					log)
{
	DE_UNREF(inputs);
	DE_UNREF(log);

	std::vector<deUint8> expectedBytes;
	expectedOutputs.front().getBuffer()->getPackedBytes(expectedBytes);

	const deUint16*	results		= reinterpret_cast<const deUint16*>(outputAllocs.front()->getHostPtr());
	const deUint16*	expected	= reinterpret_cast<const deUint16*>(&expectedBytes[0]);

	for (size_t i = 0; i < expectedBytes.size() / sizeof (deUint16); i++)
	{
		if (results[i] == expected[i])
			continue;

		if (Float16(results[i]).isNaN() && Float16(expected[i]).isNaN())
			continue;

		return false;
	}

	return true;
}


bool checkResultsFloat32 (const vector<Resource>&		inputs,
						  const vector<AllocationSp>&	outputAllocs,
						  const vector<Resource>&		expectedOutputs,
						  tcu::TestLog&					log)
{
	DE_UNREF(inputs);
	DE_UNREF(log);

	std::vector<deUint8> expectedBytes;
	expectedOutputs.front().getBuffer()->getPackedBytes(expectedBytes);

	const deUint32*	results		= reinterpret_cast<const deUint32*>(outputAllocs.front()->getHostPtr());
	const deUint32*	expected	= reinterpret_cast<const deUint32*>(&expectedBytes[0]);

	for (size_t i = 0; i < expectedBytes.size() / sizeof (deUint32); i++)
	{
		if (results[i] == expected[i])
			continue;

		if (Float32(results[i]).isNaN() && Float32(expected[i]).isNaN())
			continue;

		return false;
	}

	return true;
}

bool isNanFloat64 (deUint64 f)
{
	// NaN has full exponent bits and non-zero mantissa.
	const deUint64	exponentBits	= 0x7ff0000000000000;
	const deUint64	mantissaBits	= 0x000fffffffffffff;
	return ((f & exponentBits) == exponentBits && (f & mantissaBits) != 0);
}

bool checkResultsFloat64 (const vector<Resource>&		inputs,
						  const vector<AllocationSp>&	outputAllocs,
						  const vector<Resource>&		expectedOutputs,
						  tcu::TestLog&					log)
{
	DE_UNREF(inputs);
	DE_UNREF(log);

	std::vector<deUint8> expectedBytes;
	expectedOutputs.front().getBuffer()->getPackedBytes(expectedBytes);

	const deUint64*	results		= reinterpret_cast<const deUint64*>(outputAllocs.front()->getHostPtr());
	const deUint64*	expected	= reinterpret_cast<const deUint64*>(&expectedBytes[0]);

	for (size_t i = 0; i < expectedBytes.size() / sizeof (deUint64); i++)
	{
		if (results[i] == expected[i])
			continue;

		if (isNanFloat64(results[i]) && isNanFloat64(expected[i]))
			continue;

		return false;
	}

	return true;
}

void addComputeWorkgroupMemoryTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext&		testCtx			= group->getTestContext();
	de::Random				rnd				(deStringHash(group->getName()));
	const int				numElements		= 128;

	/*
	For each data type (TYPE) run the following shader:

	#version 430

	layout (local_size_x = 16, local_size_y = 4, local_size_z = 2) in;

	layout (binding = 0) buffer Input
	{
		TYPE data[128];
	} dataInput;

	layout (binding = 1) buffer Output
	{
		TYPE data[128];
	} dataOutput;

	shared TYPE sharedData[128];

	void main()
	{
		uint idx = gl_LocalInvocationID.z * 64 + gl_LocalInvocationID.y * 16 + gl_LocalInvocationID.x;
		sharedData[idx] = dataInput.data[idx];
		memoryBarrierShared();
		barrier();
		dataOutput.data[idx] = sharedData[127-idx];
	}
	*/

	const StringTemplate	shaderSource	(
			"                                     OpCapability Shader\n"
			"${capabilities:opt}"
			"${extensions:opt}"
			"                                %1 = OpExtInstImport \"GLSL.std.450\"\n"
			"                                     OpMemoryModel Logical GLSL450\n"
			"                                     OpEntryPoint GLCompute %main \"main\" %gl_LocalInvocationID\n"
			"                                     OpExecutionMode %main LocalSize 16 4 2\n"
			"                                     OpSource GLSL 430\n"
			"                                     OpDecorate %gl_LocalInvocationID BuiltIn LocalInvocationId\n"
			"                                     OpDecorate %_arr_uint_128_0 ArrayStride ${sizeBytes}\n"
			"                                     OpMemberDecorate %Input 0 Offset 0\n"
			"                                     OpDecorate %Input BufferBlock\n"
			"                                     OpDecorate %dataInput DescriptorSet 0\n"
			"                                     OpDecorate %dataInput Binding 0\n"
			"                                     OpDecorate %_arr_uint_128_1 ArrayStride ${sizeBytes}\n"
			"                                     OpMemberDecorate %Output 0 Offset 0\n"
			"                                     OpDecorate %Output BufferBlock\n"
			"                                     OpDecorate %dataOutput DescriptorSet 0\n"
			"                                     OpDecorate %dataOutput Binding 1\n"
			"                                     OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"
			"                             %void = OpTypeVoid\n"
			"                                %3 = OpTypeFunction %void\n"
			"                              %u32 = OpTypeInt 32 0\n"
			"               %_ptr_Function_uint = OpTypePointer Function %u32\n"
			"                           %v3uint = OpTypeVector %u32 3\n"
			"                %_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
			"             %gl_LocalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
			"                           %uint_2 = OpConstant %u32 2\n"
			"                  %_ptr_Input_uint = OpTypePointer Input %u32\n"
			"                          %uint_64 = OpConstant %u32 64\n"
			"                           %uint_1 = OpConstant %u32 1\n"
			"                          %uint_16 = OpConstant %u32 16\n"
			"                           %uint_0 = OpConstant %u32 0\n"
			"                         %uint_127 = OpConstant %u32 127\n"
			"                           %uint_4 = OpConstant %u32 4\n"
			"                              %i32 = OpTypeInt 32 1\n"
			"${dataTypeDecl}\n"
			"                         %uint_128 = OpConstant %u32 128\n"
			"                    %_arr_uint_128 = OpTypeArray %${dataType} %uint_128\n"
			"     %_ptr_Workgroup__arr_uint_128 = OpTypePointer Workgroup %_arr_uint_128\n"
			"                       %sharedData = OpVariable %_ptr_Workgroup__arr_uint_128 Workgroup\n"
			"                  %_arr_uint_128_0 = OpTypeArray %${dataType} %uint_128\n"
			"                            %Input = OpTypeStruct %_arr_uint_128_0\n"
			"               %_ptr_Uniform_Input = OpTypePointer Uniform %Input\n"
			"                        %dataInput = OpVariable %_ptr_Uniform_Input Uniform\n"
			"                            %int_0 = OpConstant %i32 0\n"
			"                     %_ptr_Uniform = OpTypePointer Uniform %${dataType}\n"
			"                   %_ptr_Workgroup = OpTypePointer Workgroup %${dataType}\n"
			"                         %uint_264 = OpConstant %u32 264\n"
			"                  %_arr_uint_128_1 = OpTypeArray %${dataType} %uint_128\n"
			"                           %Output = OpTypeStruct %_arr_uint_128_1\n"
			"              %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
			"                       %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
			"                 %gl_WorkGroupSize = OpConstantComposite %v3uint %uint_16 %uint_4 %uint_2\n"
			"                             %main = OpFunction %void None %3\n"
			"                                %5 = OpLabel\n"
			"                              %idx = OpVariable %_ptr_Function_uint Function\n"
			"                               %14 = OpAccessChain %_ptr_Input_uint %gl_LocalInvocationID %uint_2\n"
			"                               %15 = OpLoad %u32 %14\n"
			"                               %17 = OpIMul %u32 %15 %uint_64\n"
			"                               %19 = OpAccessChain %_ptr_Input_uint %gl_LocalInvocationID %uint_1\n"
			"                               %20 = OpLoad %u32 %19\n"
			"                               %22 = OpIMul %u32 %20 %uint_16\n"
			"                               %23 = OpIAdd %u32 %17 %22\n"
			"                               %25 = OpAccessChain %_ptr_Input_uint %gl_LocalInvocationID %uint_0\n"
			"                               %26 = OpLoad %u32 %25\n"
			"                               %27 = OpIAdd %u32 %23 %26\n"
			"                                     OpStore %idx %27\n"
			"                               %33 = OpLoad %u32 %idx\n"
			"                               %39 = OpLoad %u32 %idx\n"
			"                               %41 = OpAccessChain %_ptr_Uniform %dataInput %int_0 %39\n"
			"                               %42 = OpLoad %${dataType} %41\n"
			"                               %44 = OpAccessChain %_ptr_Workgroup %sharedData %33\n"
			"                                     OpStore %44 %42\n"
			"                                     OpMemoryBarrier %uint_1 %uint_264\n"
			"                                     OpControlBarrier %uint_2 %uint_2 %uint_264\n"
			"                               %50 = OpLoad %u32 %idx\n"
			"                               %52 = OpLoad %u32 %idx\n"
			"                               %53 = OpISub %u32 %uint_127 %52\n"
			"                               %54 = OpAccessChain %_ptr_Workgroup %sharedData %53\n"
			"                               %55 = OpLoad %${dataType} %54\n"
			"                               %56 = OpAccessChain %_ptr_Uniform %dataOutput %int_0 %50\n"
			"                                     OpStore %56 %55\n"
			"                                     OpReturn\n"
			"                                     OpFunctionEnd\n");

	// float64
	{
		VulkanFeatures		features;
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "8";
		shaderSpec["dataTypeDecl"]	= "%f64 = OpTypeFloat 64";
		shaderSpec["dataType"]		= "f64";
		shaderSpec["capabilities"]	= "OpCapability Float64\n";

		features.coreFeatures.shaderFloat64 = VK_TRUE;

		vector<double>		inputData	= getFloat64s(rnd, numElements);
		vector<double>		outputData;
		ComputeShaderSpec	spec;

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly					= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups				= IVec3(1, 1, 1);
		spec.verifyIO					= checkResultsFloat64;
		spec.requestedVulkanFeatures	= features;

		spec.inputs.push_back(Resource(BufferSp(new Float64Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Float64Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "float64", "", spec));
	}

	// float32
	{
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "4";
		shaderSpec["dataTypeDecl"]	= "%f32 = OpTypeFloat 32";
		shaderSpec["dataType"]		= "f32";

		vector<float>		inputData	= getFloat32s(rnd, numElements);
		vector<float>		outputData;
		ComputeShaderSpec	spec;

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly		= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups	= IVec3(1, 1, 1);
		spec.verifyIO		= checkResultsFloat32;

		spec.inputs.push_back(Resource(BufferSp(new Float32Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Float32Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "float32", "", spec));
	}

	// float16
	{
		VulkanFeatures		features;
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "2";
		shaderSpec["dataTypeDecl"]	= "%f16 = OpTypeFloat 16";
		shaderSpec["dataType"]		= "f16";
		shaderSpec["extensions"]	= "OpExtension \"SPV_KHR_16bit_storage\"\n";
		shaderSpec["capabilities"]	= "OpCapability StorageUniformBufferBlock16\nOpCapability Float16\n";

		features.ext16BitStorage.storageBuffer16BitAccess = true;
		features.extFloat16Int8.shaderFloat16 = true;

		vector<deFloat16>	inputData	= getFloat16s(rnd, numElements);
		vector<deFloat16>	outputData;
		ComputeShaderSpec	spec;

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly		= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups	= IVec3(1, 1, 1);
		spec.extensions.push_back("VK_KHR_16bit_storage");
		spec.extensions.push_back("VK_KHR_shader_float16_int8");
		spec.requestedVulkanFeatures = features;
		spec.verifyIO		= checkResultsFloat16;

		spec.inputs.push_back(Resource(BufferSp(new Float16Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Float16Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "float16", "", spec));
	}

	// int64
	{
		VulkanFeatures		features;
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "8";
		shaderSpec["dataTypeDecl"]	= "%i64 = OpTypeInt 64 1";
		shaderSpec["dataType"]		= "i64";
		shaderSpec["capabilities"]	= "OpCapability Int64\n";

		features.coreFeatures.shaderInt64 = VK_TRUE;

		vector<deInt64>		inputData	= getInt64s(rnd, numElements);
		vector<deInt64>		outputData;
		ComputeShaderSpec	spec;

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly					= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups				= IVec3(1, 1, 1);
		spec.requestedVulkanFeatures	= features;

		spec.inputs.push_back(Resource(BufferSp(new Int64Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Int64Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "int64", "", spec));
	}

	// int32
	{
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "4";
		shaderSpec["dataTypeDecl"]	= "";
		shaderSpec["dataType"]		= "i32";

		vector<deInt32>		inputData	= getInt32s(rnd, numElements);
		vector<deInt32>		outputData;
		ComputeShaderSpec	spec;

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly		= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups	= IVec3(1, 1, 1);

		spec.inputs.push_back(Resource(BufferSp(new Int32Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Int32Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "int32", "", spec));
	}

	// int16
	{
		VulkanFeatures		features;
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "2";
		shaderSpec["dataTypeDecl"]	= "%i16 = OpTypeInt 16 1";
		shaderSpec["dataType"]		= "i16";
		shaderSpec["extensions"]	= "OpExtension \"SPV_KHR_16bit_storage\"\n";
		shaderSpec["capabilities"]	= "OpCapability Int16\n";

		features.coreFeatures.shaderInt16 = true;
		features.ext16BitStorage.storageBuffer16BitAccess = true;

		vector<deInt16>		inputData	= getInt16s(rnd, numElements);
		vector<deInt16>		outputData;
		ComputeShaderSpec	spec;

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly		= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups	= IVec3(1, 1, 1);
		spec.extensions.push_back("VK_KHR_16bit_storage");
		spec.requestedVulkanFeatures = features;

		spec.inputs.push_back(Resource(BufferSp(new Int16Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Int16Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "int16", "", spec));
	}

	// int8
	{
		VulkanFeatures		features;
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "1";
		shaderSpec["dataTypeDecl"]	= "%i8 = OpTypeInt 8 1";
		shaderSpec["dataType"]		= "i8";
		shaderSpec["capabilities"]	= "OpCapability UniformAndStorageBuffer8BitAccess\nOpCapability Int8\n";
		shaderSpec["extensions"]	= "OpExtension \"SPV_KHR_8bit_storage\"\n";

		features.ext8BitStorage.storageBuffer8BitAccess = true;
		features.extFloat16Int8.shaderInt8 = true;

		vector<deInt8>		inputData	= getInt8s(rnd, numElements);
		vector<deInt8>		outputData;
		ComputeShaderSpec	spec;

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly		= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups	= IVec3(1, 1, 1);
		spec.extensions.push_back("VK_KHR_8bit_storage");
		spec.extensions.push_back("VK_KHR_shader_float16_int8");
		spec.requestedVulkanFeatures = features;

		spec.inputs.push_back(Resource(BufferSp(new Int8Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Int8Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "int8", "", spec));
	}

	// uint64
	{
		VulkanFeatures		features;
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "8";
		shaderSpec["dataTypeDecl"]	= "%u64 = OpTypeInt 64 0";
		shaderSpec["dataType"]		= "u64";
		shaderSpec["capabilities"]	= "OpCapability Int64\n";

		features.coreFeatures.shaderInt64 = VK_TRUE;

		vector<deUint64>	inputData;
		vector<deUint64>	outputData;
		ComputeShaderSpec	spec;

		inputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			inputData.push_back(rnd.getUint64());

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly					= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups				= IVec3(1, 1, 1);
		spec.requestedVulkanFeatures	= features;

		spec.inputs.push_back(Resource(BufferSp(new Uint64Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Uint64Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "uint64", "", spec));
	}

	// uint32
	{
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "4";
		shaderSpec["dataTypeDecl"]	= "";
		shaderSpec["dataType"]		= "u32";

		vector<deUint32>	inputData;
		vector<deUint32>	outputData;
		ComputeShaderSpec	spec;

		inputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			inputData.push_back(rnd.getUint32());

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly		= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups	= IVec3(1, 1, 1);

		spec.inputs.push_back(Resource(BufferSp(new Uint32Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Uint32Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "uint32", "", spec));
	}

	// uint16
	{
		VulkanFeatures		features;
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "2";
		shaderSpec["dataTypeDecl"]	= "%u16 = OpTypeInt 16 0";
		shaderSpec["dataType"]		= "u16";
		shaderSpec["capabilities"]	= "OpCapability Int16\n";
		shaderSpec["extensions"]	= "OpExtension \"SPV_KHR_16bit_storage\"\n";

		features.coreFeatures.shaderInt16 = true;
		features.ext16BitStorage.storageBuffer16BitAccess = true;

		vector<deUint16>	inputData;
		vector<deUint16>	outputData;
		ComputeShaderSpec	spec;

		inputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			inputData.push_back(rnd.getUint16());

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly		= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups	= IVec3(1, 1, 1);
		spec.extensions.push_back("VK_KHR_16bit_storage");
		spec.requestedVulkanFeatures = features;

		spec.inputs.push_back(Resource(BufferSp(new Uint16Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Uint16Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "uint16", "", spec));
	}

	// uint8
	{
		VulkanFeatures		features;
		map<string, string>	shaderSpec;

		shaderSpec["sizeBytes"]		= "1";
		shaderSpec["dataTypeDecl"]	= "%u8 = OpTypeInt 8 0";
		shaderSpec["dataType"]		= "u8";
		shaderSpec["capabilities"]	= "OpCapability UniformAndStorageBuffer8BitAccess\nOpCapability Int8\n";
		shaderSpec["extensions"]	= "OpExtension \"SPV_KHR_8bit_storage\"\n";

		features.ext8BitStorage.storageBuffer8BitAccess = true;
		features.extFloat16Int8.shaderInt8 = true;

		vector<deUint8>		inputData;
		vector<deUint8>		outputData;
		ComputeShaderSpec	spec;

		inputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			inputData.push_back(rnd.getUint8());

		outputData.reserve(numElements);
		for (deUint32 numIdx = 0; numIdx < numElements; ++numIdx)
			outputData.push_back(inputData[numElements - numIdx - 1]);

		spec.assembly		= shaderSource.specialize(shaderSpec);
		spec.numWorkGroups	= IVec3(1, 1, 1);
		spec.extensions.push_back("VK_KHR_8bit_storage");
		spec.extensions.push_back("VK_KHR_shader_float16_int8");
		spec.requestedVulkanFeatures = features;

		spec.inputs.push_back(Resource(BufferSp(new Uint8Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		spec.outputs.push_back(Resource(BufferSp(new Uint8Buffer(outputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		group->addChild(new SpvAsmComputeShaderCase(testCtx, "uint8", "", spec));
	}
}

} // anonymous

tcu::TestCaseGroup* createWorkgroupMemoryComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "workgroup_memory", "Compute tests for workgroup memory.."));
	addComputeWorkgroupMemoryTests(group.get());

	return group.release();
}

} // SpirVAssembly
} // vkt
