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

string getCompositeInserts (deUint32 cols, deUint32 rows)
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
				+ getCompositeInserts(cols, rows) +
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
			vector<string>			noFeatures;
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
				+ getCompositeInserts(cols, rows) +
				"            %matOutPtr = OpAccessChain %_ptr_Uniform_mat %dataOutput %c_i32_0\n"
				"                         OpStore %matOutPtr %tmp" + de::toString(cols) + "\n"
				"                         OpReturnValue %param\n"
				"                         OpFunctionEnd\n";

			vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
			vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_FALSE;
			createTestForStage(VK_SHADER_STAGE_VERTEX_BIT, (testName + "_vert").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, noFeatures, vulkanFeatures, group);

			createTestForStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, (testName + "_tessc").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, noFeatures, vulkanFeatures, group);

			createTestForStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, (testName + "_tesse").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, noFeatures, vulkanFeatures, group);

			createTestForStage(VK_SHADER_STAGE_GEOMETRY_BIT, (testName + "_geom").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, noFeatures, vulkanFeatures, group);

			vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_FALSE;
			vulkanFeatures.coreFeatures.fragmentStoresAndAtomics = DE_TRUE;
			createTestForStage(VK_SHADER_STAGE_FRAGMENT_BIT, (testName + "_frag").c_str(), defaultColors, defaultColors, fragments, noSpecConstants,
					noPushConstants, resources, noInterfaces, noExtensions, noFeatures, vulkanFeatures, group);
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createCompositeInsertComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "composite_insert", "Compute tests for composite insert."));
	addComputeMatrixCompositeInsertTests(group.get());

	return group.release();
}

tcu::TestCaseGroup* createCompositeInsertGraphicsGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "composite_insert", "Graphics tests for composite insert."));
	addGraphicsMatrixCompositeInsertTests(group.get());

	return group.release();
}

} // SpirVAssembly
} // vkt
