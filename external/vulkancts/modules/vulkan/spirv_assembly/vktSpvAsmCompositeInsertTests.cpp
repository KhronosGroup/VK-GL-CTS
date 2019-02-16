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
 * \brief SPIR-V Assembly tests for composite insert.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmCompositeInsertTests.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmGraphicsShaderTestUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::map;
using std::string;
using std::vector;
using tcu::IVec3;
using tcu::RGBA;

namespace
{

string getColType (deUint32 rows)
{
	return string("%v") + de::toString(rows) + "f32";
}

string getMatrixType (deUint32 cols, deUint32 rows)
{
	return string("%mat") + de::toString(cols) + "v" + de::toString(rows) + "f";
}

string getMatrixDeclarations (deUint32 cols, deUint32 rows, bool skipColDecl = false)
{
	string	colType		= getColType(rows);
	string	colDecl		= skipColDecl ? "" : string("                ") + colType + " = OpTypeVector %f32 " + de::toString(rows) + "\n";
	string	matType		= getMatrixType(cols, rows);
	string	matDecl		= string("              ") + matType + " = OpTypeMatrix " + colType + " " + de::toString(cols) + "\n";
	string	outputDecl	= string("               %Output = OpTypeStruct ") + matType + "\n";

	return colDecl + matDecl + outputDecl;
}

string getIdentityVectors (deUint32 cols, deUint32 rows)
{
	string ret;

	for (deUint32 c = 0; c < cols; c++)
	{
		string identity = "            %identity" + de::toString(c) + " = OpConstantComposite " + getColType(rows) + " ";

		for (deUint32 r = 0; r < rows; r++)
		{
			identity += string("%c_f32_") + (c == r ? "1" : "0") + " ";
		}

		identity += "\n";
		ret += identity;
	}

	return ret;
}

string getVectorCompositeInserts (deUint32 elements)
{
	string	ret		= "                 %tmp0 = OpLoad %v" + de::toString(elements) + "f32 %vec\n";

	for (deUint32 e = 0; e < elements; e++)
		ret += "                 %tmp" + de::toString(e + 1) + " = OpCompositeInsert %v" + de::toString(elements) + "f32 %c_f32_" + de::toString(e) + " %tmp" + de::toString(e) + " " + de::toString(e) + "\n";

	return ret;
}

string getMatrixCompositeInserts (deUint32 cols, deUint32 rows)
{
	string	matType	= getMatrixType(cols, rows);
	string	ret		= "                 %tmp0 = OpLoad " + matType + " %mat\n";

	for (deUint32 c = 0; c < cols; c++)
		ret += "                 %tmp" + de::toString(c + 1) + " = OpCompositeInsert " + matType + " %identity" + de::toString(c) + " %tmp" + de::toString(c) + " " + de::toString(c) + "\n";

	return ret;
}

bool verifyMatrixOutput (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, tcu::TestLog& log)
{
	DE_UNREF(inputs);

	if (outputAllocs.size() != 1)
		return false;

	vector<deUint8> expectedBytes;
	expectedOutputs[0].getBytes(expectedBytes);

	const float* const    expectedOutputAsFloat     = reinterpret_cast<const float*>(&expectedBytes.front());
	const float* const    outputAsFloat             = static_cast<const float*>(outputAllocs[0]->getHostPtr());
	bool ret										= true;

	for (size_t idx = 0; idx < expectedBytes.size() / sizeof(float); ++idx)
	{
		if (outputAsFloat[idx] != expectedOutputAsFloat[idx] && expectedOutputAsFloat[idx] != -1.0f)
		{
			log << tcu::TestLog::Message << "ERROR: Result data at index " << idx << " failed. Expected: " << expectedOutputAsFloat[idx] << ", got: " << outputAsFloat[idx] << tcu::TestLog::EndMessage;
			ret = false;
		}
	}
	return ret;
}

string getNestedStructCompositeInserts (deUint32 arraySize)
{
	string	ret;

	for (deUint32 arrayIdx = 0; arrayIdx < arraySize; arrayIdx++)
		for (deUint32 vectorIdx = 0; vectorIdx < 4; vectorIdx++)
			ret += string("%tmp") + de::toString(arrayIdx * 4 + vectorIdx + 1) + " = OpCompositeInsert %Output %identity" + de::toString(vectorIdx) + " %tmp" + de::toString(arrayIdx * 4 + vectorIdx) + " 0 0 " + de::toString(arrayIdx) + " " + de::toString(vectorIdx) + "\n";

	return ret;
}

void addComputeVectorCompositeInsertTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	for (deUint32 elements = 2; elements <= 4; elements++)
	{
		ComputeShaderSpec	spec;
		vector<float>		refData;
		const string		vecType			= string("%v") + de::toString(elements) + "f32";

		// Generate a vector using OpCompositeInsert
		const string		shaderSource	=
			"                         OpCapability Shader\n"
			"                    %1 = OpExtInstImport \"GLSL.std.450\"\n"
			"                         OpMemoryModel Logical GLSL450\n"
			"                         OpEntryPoint GLCompute %main \"main\"\n"
			"                         OpExecutionMode %main LocalSize 1 1 1\n"
			"                         OpSource GLSL 430\n"
			"                         OpMemberDecorate %Output 0 Offset 0\n"
			"                         OpDecorate %Output BufferBlock\n"
			"                         OpDecorate %dataOutput DescriptorSet 0\n"
			"                         OpDecorate %dataOutput Binding 0\n"
			"                  %f32 = OpTypeFloat 32\n"
			"                %v2f32 = OpTypeVector %f32 2\n"
			"                %v3f32 = OpTypeVector %f32 3\n"
			"                %v4f32 = OpTypeVector %f32 4\n"
			"               %Output = OpTypeStruct " + vecType + "\n"
			"  %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
			"           %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
			"    %_ptr_Function_vec = OpTypePointer Function " + vecType + "\n"
			"     %_ptr_Uniform_vec = OpTypePointer Uniform " + vecType + "\n"
			"              %c_f32_0 = OpConstant %f32 0\n"
			"              %c_f32_1 = OpConstant %f32 1\n"
			"              %c_f32_2 = OpConstant %f32 2\n"
			"              %c_f32_3 = OpConstant %f32 3\n"
			"                  %i32 = OpTypeInt 32 1\n"
			"              %c_i32_0 = OpConstant %i32 0\n"
			"                 %void = OpTypeVoid\n"
			"                    %3 = OpTypeFunction %void\n"
			"                 %main = OpFunction %void None %3\n"
			"                %entry = OpLabel\n"
			"                  %vec = OpVariable %_ptr_Function_vec Function\n"
			+ getVectorCompositeInserts(elements) +
			"            %vecOutPtr = OpAccessChain %_ptr_Uniform_vec %dataOutput %c_i32_0\n"
			"                         OpStore %vecOutPtr %tmp" + de::toString(elements) + "\n"
			"                         OpReturn\n"
			"                         OpFunctionEnd\n";

		spec.assembly		= shaderSource;
		spec.numWorkGroups	= IVec3(1, 1, 1);

		// Expect running counter
		for (deUint32 e = 0; e < elements; e++)
			refData.push_back((float)e);

		spec.outputs.push_back(Resource(BufferSp(new Float32Buffer(refData))));

		string testName = string("vec") + de::toString(elements);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), "Tests vector composite insert.", spec));
	}
}

void addGraphicsVectorCompositeInsertTests (tcu::TestCaseGroup* group)
{
	for (deUint32 elements = 2; elements <= 4; elements++)
	{
		map<string, string>	fragments;
		RGBA				defaultColors[4];
		GraphicsResources	resources;

		SpecConstants		noSpecConstants;
		PushConstants		noPushConstants;
		GraphicsInterfaces	noInterfaces;
		vector<string>		noExtensions;
		VulkanFeatures		vulkanFeatures	= VulkanFeatures();
		vector<float>		refData;
		const string		testName		= string("vec") + de::toString(elements);
		const string		vecType			= string("%v") + de::toString(elements) + "f32";

		// Expect running counter
		for (deUint32 e = 0; e < elements; e++)
			refData.push_back((float)e);
		resources.outputs.push_back(Resource(BufferSp(new Float32Buffer(refData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		getDefaultColors(defaultColors);

		// Generate a vector using OpCompositeInsert
		fragments["pre_main"]	=
			"               %Output = OpTypeStruct " + vecType + "\n"
			"  %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
			"           %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
			"             %fp_v2f32 = OpTypePointer Function %v2f32\n"
			"             %fp_v3f32 = OpTypePointer Function %v3f32\n"
			"     %_ptr_Uniform_vec = OpTypePointer Uniform " + vecType + "\n"
			"              %c_f32_2 = OpConstant %f32 2\n"
			"              %c_f32_3 = OpConstant %f32 3\n";

		fragments["decoration"]	=
			"                         OpMemberDecorate %Output 0 Offset 0\n"
			"                         OpDecorate %Output BufferBlock\n"
			"                         OpDecorate %dataOutput DescriptorSet 0\n"
			"                         OpDecorate %dataOutput Binding 0\n";

		fragments["testfun"]	=
			"            %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"                %param = OpFunctionParameter %v4f32\n"
			"                %entry = OpLabel\n"
			"                  %vec = OpVariable %fp_v" + de::toString(elements) + "f32 Function\n"
			+ getVectorCompositeInserts(elements) +
			"            %vecOutPtr = OpAccessChain %_ptr_Uniform_vec %dataOutput %c_i32_0\n"
			"                         OpStore %vecOutPtr %tmp" + de::toString(elements) + "\n"
			"                         OpReturnValue %param\n"
			"                         OpFunctionEnd\n";

		vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
		vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_FALSE;
		createTestForStage(VK_SHADER_STAGE_VERTEX_BIT, (testName + "_vert").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
				noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

		createTestForStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, (testName + "_tessc").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
				noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

		createTestForStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, (testName + "_tesse").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
				noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

		createTestForStage(VK_SHADER_STAGE_GEOMETRY_BIT, (testName + "_geom").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
				noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

		vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_FALSE;
		vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_TRUE;
		createTestForStage(VK_SHADER_STAGE_FRAGMENT_BIT, (testName + "_frag").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
				noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);
	}
}

void addComputeMatrixCompositeInsertTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext&		testCtx			= group->getTestContext();

	for (deUint32 rows = 2; rows <= 4; rows++)
	{
		const deUint32	matrixStride = rows == 3 ? 16 : rows * 4;

		for (deUint32 cols = 2; cols <= 4; cols++)
		{
			ComputeShaderSpec	spec;
			vector<float>		identityData;
			string				colType			= getColType(rows);
			string				matType			= getMatrixType(cols, rows);

			// Generate a matrix using OpCompositeInsert with identity vectors and write the matrix into output storage buffer.
			const string		shaderSource	=
				"                         OpCapability Shader\n"
				"                    %1 = OpExtInstImport \"GLSL.std.450\"\n"
				"                         OpMemoryModel Logical GLSL450\n"
				"                         OpEntryPoint GLCompute %main \"main\"\n"
				"                         OpExecutionMode %main LocalSize 1 1 1\n"
				"                         OpSource GLSL 430\n"
				"                         OpMemberDecorate %Output 0 Offset 0\n"
				"                         OpMemberDecorate %Output 0 ColMajor\n"
				"                         OpMemberDecorate %Output 0 MatrixStride " + de::toString(matrixStride) + "\n"
				"                         OpDecorate %Output BufferBlock\n"
				"                         OpDecorate %dataOutput DescriptorSet 0\n"
				"                         OpDecorate %dataOutput Binding 0\n"
				"                  %f32 = OpTypeFloat 32\n"
				+ getMatrixDeclarations(cols, rows) +
				"  %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
				"           %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
				"    %_ptr_Function_mat = OpTypePointer Function " + matType + "\n"
				"     %_ptr_Uniform_mat = OpTypePointer Uniform " + matType + "\n"
				"              %c_f32_0 = OpConstant %f32 0\n"
				"              %c_f32_1 = OpConstant %f32 1\n"
				"                  %i32 = OpTypeInt 32 1\n"
				"              %c_i32_0 = OpConstant %i32 0\n"
				+ getIdentityVectors(cols, rows) +
				"                 %void = OpTypeVoid\n"
				"                    %3 = OpTypeFunction %void\n"
				"                 %main = OpFunction %void None %3\n"
				"                %entry = OpLabel\n"
				"                  %mat = OpVariable %_ptr_Function_mat Function\n"
				+ getMatrixCompositeInserts(cols, rows) +
				"            %matOutPtr = OpAccessChain %_ptr_Uniform_mat %dataOutput %c_i32_0\n"
				"                         OpStore %matOutPtr %tmp" + de::toString(cols) + "\n"
				"                         OpReturn\n"
				"                         OpFunctionEnd\n";


			spec.assembly		= shaderSource;
			spec.numWorkGroups	= IVec3(1, 1, 1);

			// Expect identity matrix as output
			for (deUint32 c = 0; c < cols; c++)
			{
				for (deUint32 r = 0; r < rows; r++)
					identityData.push_back(c == r ? 1.0f : 0.0f);
				if (rows == 3)
					identityData.push_back(-1.0f); // Padding
			}

			spec.outputs.push_back(Resource(BufferSp(new Float32Buffer(identityData))));
			spec.verifyIO = verifyMatrixOutput;

			string testName = string("mat") + de::toString(cols) + "x" + de::toString(rows);

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), "Tests matrix composite insert.", spec));
		}
	}
}

void addGraphicsMatrixCompositeInsertTests (tcu::TestCaseGroup* group)
{
	for (deUint32 rows = 2; rows <= 4; rows++)
	{
		const deUint32	matrixStride = rows == 3 ? 16 : rows * 4;

		for (deUint32 cols = 2; cols <= 4; cols++)
		{
			map<string, string>		fragments;
			RGBA					defaultColors[4];
			GraphicsResources		resources;

			SpecConstants			noSpecConstants;
			PushConstants			noPushConstants;
			GraphicsInterfaces		noInterfaces;
			vector<string>			noExtensions;
			VulkanFeatures			vulkanFeatures	= VulkanFeatures();
			vector<float>			identityData;
			string					testName		= string("mat") + de::toString(cols) + "x" + de::toString(rows);
			string					matType			= getMatrixType(cols, rows);

			// Expect identity matrix as output
			for (deUint32 c = 0; c < cols; c++)
			{
				for (deUint32 r = 0; r < rows; r++)
					identityData.push_back(c == r ? 1.0f : 0.0f);
				if (rows == 3)
					identityData.push_back(-1.0f); // Padding
			}
			resources.outputs.push_back(Resource(BufferSp(new Float32Buffer(identityData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
			resources.verifyIO = verifyMatrixOutput;

			getDefaultColors(defaultColors);

			// Generate a matrix using OpCompositeInsert with identity vectors and write the matrix into output storage buffer.
			fragments["pre_main"]	=
				getMatrixDeclarations(cols, rows, true) +
				"  %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
				"           %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
				"    %_ptr_Function_mat = OpTypePointer Function " + matType + "\n"
				"     %_ptr_Uniform_mat = OpTypePointer Uniform " + matType + "\n"
				+ getIdentityVectors(cols, rows);

			fragments["decoration"]	=
				"                         OpMemberDecorate %Output 0 Offset 0\n"
				"                         OpMemberDecorate %Output 0 ColMajor\n"
				"                         OpMemberDecorate %Output 0 MatrixStride " + de::toString(matrixStride) + "\n"
				"                         OpDecorate %Output BufferBlock\n"
				"                         OpDecorate %dataOutput DescriptorSet 0\n"
				"                         OpDecorate %dataOutput Binding 0\n";

			fragments["testfun"]	=
				"            %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
				"                %param = OpFunctionParameter %v4f32\n"
				"                %entry = OpLabel\n"
				"                  %mat = OpVariable %_ptr_Function_mat Function\n"
				+ getMatrixCompositeInserts(cols, rows) +
				"            %matOutPtr = OpAccessChain %_ptr_Uniform_mat %dataOutput %c_i32_0\n"
				"                         OpStore %matOutPtr %tmp" + de::toString(cols) + "\n"
				"                         OpReturnValue %param\n"
				"                         OpFunctionEnd\n";

			vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
			vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_FALSE;
			createTestForStage(VK_SHADER_STAGE_VERTEX_BIT, (testName + "_vert").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

			createTestForStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, (testName + "_tessc").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

			createTestForStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, (testName + "_tesse").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

			createTestForStage(VK_SHADER_STAGE_GEOMETRY_BIT, (testName + "_geom").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

			vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_FALSE;
			vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_TRUE;
			createTestForStage(VK_SHADER_STAGE_FRAGMENT_BIT, (testName + "_frag").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);
		}
	}
}

void addComputeNestedStructCompositeInsertTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx			= group->getTestContext();

	ComputeShaderSpec	spec;
	vector<float>		identityData;
	const deUint32		arraySize		= 8u;

	const string		shaderSource	=
		"                         OpCapability Shader\n"
		"                    %1 = OpExtInstImport \"GLSL.std.450\"\n"
		"                         OpMemoryModel Logical GLSL450\n"
		"                         OpEntryPoint GLCompute %main \"main\"\n"
		"                         OpExecutionMode %main LocalSize 1 1 1\n"
		"                         OpSource GLSL 430\n"
		"                         OpDecorate %_arr_mat4v4f32_uint_8 ArrayStride 64\n"
		"                         OpMemberDecorate %S 0 ColMajor\n"
		"                         OpMemberDecorate %S 0 Offset 0\n"
		"                         OpMemberDecorate %S 0 MatrixStride 16\n"
		"                         OpMemberDecorate %Output 0 Offset 0\n"
		"                         OpDecorate %Output BufferBlock\n"
		"                         OpDecorate %dataOutput DescriptorSet 0\n"
		"                         OpDecorate %dataOutput Binding 0\n"
		"                  %f32 = OpTypeFloat 32\n"
		"                %v4f32 = OpTypeVector %f32 4\n"
		"            %mat4v4f32 = OpTypeMatrix %v4f32 4\n"
		"                 %uint = OpTypeInt 32 0\n"
		"               %uint_8 = OpConstant %uint 8\n"
		"%_arr_mat4v4f32_uint_8 = OpTypeArray %mat4v4f32 %uint_8\n"
		"                    %S = OpTypeStruct %_arr_mat4v4f32_uint_8\n"
		"               %Output = OpTypeStruct %S\n"
		"  %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
		" %_ptr_Function_Output = OpTypePointer Function %Output\n"
		"           %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
		"              %c_f32_0 = OpConstant %f32 0\n"
		"              %c_f32_1 = OpConstant %f32 1\n"
		"                  %i32 = OpTypeInt 32 1\n"
		"              %c_i32_0 = OpConstant %i32 0\n"
		+ getIdentityVectors(4, 4) +
		"                 %void = OpTypeVoid\n"
		"                    %3 = OpTypeFunction %void\n"
		"                 %main = OpFunction %void None %3\n"
		"                %entry = OpLabel\n"
		"         %nestedstruct = OpVariable %_ptr_Function_Output Function\n"
		"                 %tmp0 = OpLoad %Output %nestedstruct\n"
		+ getNestedStructCompositeInserts(arraySize) +
		"                         OpStore %dataOutput %tmp" + de::toString(arraySize * 4) + "\n"
		"                         OpReturn\n"
		"                         OpFunctionEnd\n";

	spec.assembly		= shaderSource;
	spec.numWorkGroups	= IVec3(1, 1, 1);

	// Expect an array of identity matrix as output
	for (deUint32 a = 0; a < arraySize; a++)
		for (deUint32 c = 0; c < 4; c++)
			for (deUint32 r = 0; r < 4; r++)
				identityData.push_back(c == r ? 1.0f : 0.0f);

	spec.outputs.push_back(Resource(BufferSp(new Float32Buffer(identityData))));

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "nested_struct", "Tests nested struct composite insert.", spec));
}

void addGraphicsNestedStructCompositeInsertTests (tcu::TestCaseGroup* group)
{
	map<string, string>	fragments;
	RGBA				defaultColors[4];
	GraphicsResources	resources;

	SpecConstants		noSpecConstants;
	PushConstants		noPushConstants;
	GraphicsInterfaces	noInterfaces;
	vector<string>		noExtensions;
	VulkanFeatures		vulkanFeatures	= VulkanFeatures();
	vector<float>		identityData;
	const deUint32		arraySize		= 8u;
	const string		testName		= "nested_struct";

	// Expect an array of identity matrix as output
	for (deUint32 a = 0; a < arraySize; a++)
		for (deUint32 c = 0; c < 4; c++)
			for (deUint32 r = 0; r < 4; r++)
				identityData.push_back(c == r ? 1.0f : 0.0f);
	resources.outputs.push_back(Resource(BufferSp(new Float32Buffer(identityData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	getDefaultColors(defaultColors);

	fragments["pre_main"]	=
		"               %uint_8 = OpConstant %u32 8\n"
		"            %mat4v4f32 = OpTypeMatrix %v4f32 4\n"
		"%_arr_mat4v4f32_uint_8 = OpTypeArray %mat4v4f32 %uint_8\n"
		"                    %S = OpTypeStruct %_arr_mat4v4f32_uint_8\n"
		"               %Output = OpTypeStruct %S\n"
		"  %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
		" %_ptr_Function_Output = OpTypePointer Function %Output\n"
		"           %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
		+ getIdentityVectors(4, 4);

	fragments["decoration"]	=
		"                         OpDecorate %_arr_mat4v4f32_uint_8 ArrayStride 64\n"
		"                         OpMemberDecorate %S 0 ColMajor\n"
		"                         OpMemberDecorate %S 0 Offset 0\n"
		"                         OpMemberDecorate %S 0 MatrixStride 16\n"
		"                         OpMemberDecorate %Output 0 Offset 0\n"
		"                         OpDecorate %Output BufferBlock\n"
		"                         OpDecorate %dataOutput DescriptorSet 0\n"
		"                         OpDecorate %dataOutput Binding 0\n";

	fragments["testfun"]	=
		"            %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"                %param = OpFunctionParameter %v4f32\n"
		"                %entry = OpLabel\n"
		"         %nestedstruct = OpVariable %_ptr_Function_Output Function\n"
		"                 %tmp0 = OpLoad %Output %nestedstruct\n"
		+ getNestedStructCompositeInserts(arraySize) +
		"                         OpStore %dataOutput %tmp" + de::toString(arraySize * 4) + "\n"
		"                         OpReturnValue %param\n"
		"                         OpFunctionEnd\n";

	vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
	vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_FALSE;
	createTestForStage(VK_SHADER_STAGE_VERTEX_BIT, (testName + "_vert").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
			noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

	createTestForStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, (testName + "_tessc").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
			noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

	createTestForStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, (testName + "_tesse").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
			noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

	createTestForStage(VK_SHADER_STAGE_GEOMETRY_BIT, (testName + "_geom").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
			noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);

	vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_FALSE;
	vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_TRUE;
	createTestForStage(VK_SHADER_STAGE_FRAGMENT_BIT, (testName + "_frag").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
			noPushConstants, resources, noInterfaces, noExtensions, vulkanFeatures, group);
}

} // anonymous

tcu::TestCaseGroup* createCompositeInsertComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "composite_insert", "Compute tests for composite insert."));
	addComputeVectorCompositeInsertTests(group.get());
	addComputeMatrixCompositeInsertTests(group.get());
	addComputeNestedStructCompositeInsertTests(group.get());

	return group.release();
}

tcu::TestCaseGroup* createCompositeInsertGraphicsGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "composite_insert", "Graphics tests for composite insert."));
	addGraphicsVectorCompositeInsertTests(group.get());
	addGraphicsMatrixCompositeInsertTests(group.get());
	addGraphicsNestedStructCompositeInsertTests(group.get());

	return group.release();
}

} // SpirVAssembly
} // vkt
