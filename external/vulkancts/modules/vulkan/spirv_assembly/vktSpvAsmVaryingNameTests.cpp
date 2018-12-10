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
 * \brief SPIR-V Assembly Tests for varying names.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmVaryingNameTests.hpp"
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
using tcu::RGBA;

namespace
{

struct TestParams
{
	string											name;
	FunctionPrograms1<InstanceContext>::Function	createShaders;
};

void createShaders (SourceCollections& dst, InstanceContext& context, string dataNameVertShader, string dataNameFragShader)
{
	SpirvVersion	targetSpirvVersion	= context.resources.spirvVersion;
	const deUint32	vulkanVersion		= dst.usedVulkanVersion;
	const string	opNameVert			= dataNameVertShader.empty() ? "" : string("                             OpName %dataOut \"") + dataNameVertShader + "\"\n";
	const string	opNameFrag			= dataNameFragShader.empty() ? "" : string("                        OpName %dataIn \"") + dataNameFragShader + "\"\n";

	// A float data of 1.0 is passed from vertex shader to fragment shader. This test checks the
	// mapping between shaders is based on location index and not using the name of the varying.
	// Variations of this test include same OpName in both shader, different OpNames and no
	// OpNames at all.
	const string	vertexShader		= string(
		"                             OpCapability Shader\n"
		"                        %1 = OpExtInstImport \"GLSL.std.450\"\n"
		"                             OpMemoryModel Logical GLSL450\n"
		"                             OpEntryPoint Vertex %main \"main\" %_ %position %vtxColor %color %dataOut\n"
		"                             OpSource GLSL 450\n"
		"                             OpName %main \"main\"\n"
		"                             OpName %gl_PerVertex \"gl_PerVertex\"\n"
		"                             OpMemberName %gl_PerVertex 0 \"gl_Position\"\n"
		"                             OpMemberName %gl_PerVertex 1 \"gl_PointSize\"\n"
		"                             OpMemberName %gl_PerVertex 2 \"gl_ClipDistance\"\n"
		"                             OpMemberName %gl_PerVertex 3 \"gl_CullDistance\"\n"
		"                             OpName %_ \"\"\n"
		"                             OpName %position \"position\"\n"
		"                             OpName %vtxColor \"vtxColor\"\n"
		"                             OpName %color \"color\"\n")
		+ opNameVert +
		"                             OpMemberDecorate %gl_PerVertex 0 BuiltIn Position\n"
		"                             OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize\n"
		"                             OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance\n"
		"                             OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance\n"
		"                             OpDecorate %gl_PerVertex Block\n"
		"                             OpDecorate %position Location 0\n"
		"                             OpDecorate %vtxColor Location 1\n"
		"                             OpDecorate %color Location 1\n"
		"                             OpDecorate %dataOut Location 0\n"
		"                     %void = OpTypeVoid\n"
		"                        %3 = OpTypeFunction %void\n"
		"                    %float = OpTypeFloat 32\n"
		"                  %v4float = OpTypeVector %float 4\n"
		"                     %uint = OpTypeInt 32 0\n"
		"                   %uint_1 = OpConstant %uint 1\n"
		"        %_arr_float_uint_1 = OpTypeArray %float %uint_1\n"
		"             %gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n"
		" %_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex\n"
		"                        %_ = OpVariable %_ptr_Output_gl_PerVertex Output\n"
		"                      %int = OpTypeInt 32 1\n"
		"                    %int_0 = OpConstant %int 0\n"
		"       %_ptr_Input_v4float = OpTypePointer Input %v4float\n"
		"                 %position = OpVariable %_ptr_Input_v4float Input\n"
		"      %_ptr_Output_v4float = OpTypePointer Output %v4float\n"
		"                 %vtxColor = OpVariable %_ptr_Output_v4float Output\n"
		"                    %color = OpVariable %_ptr_Input_v4float Input\n"
		"        %_ptr_Output_float = OpTypePointer Output %float\n"
		"                  %dataOut = OpVariable %_ptr_Output_float Output\n"
		"                  %float_1 = OpConstant %float 1\n"
		"                     %main = OpFunction %void None %3\n"
		"                        %5 = OpLabel\n"
		"                       %18 = OpLoad %v4float %position\n"
		"                       %20 = OpAccessChain %_ptr_Output_v4float %_ %int_0\n"
		"                             OpStore %20 %18\n"
		"                       %23 = OpLoad %v4float %color\n"
		"                             OpStore %vtxColor %23\n"
		"                             OpStore %dataOut %float_1\n"
		"                             OpReturn\n"
		"                             OpFunctionEnd\n";

	const string	fragmentShader		= string(
		"                        OpCapability Shader\n"
		"                   %1 = OpExtInstImport \"GLSL.std.450\"\n"
		"                        OpMemoryModel Logical GLSL450\n"
		"                        OpEntryPoint Fragment %main \"main\" %dataIn %fragColor %vtxColor\n"
		"                        OpExecutionMode %main OriginUpperLeft\n"
		"                        OpSource GLSL 450\n"
		"                        OpName %main \"main\"\n"
		"                        OpName %Output \"Output\"\n"
		"                        OpMemberName %Output 0 \"dataOut\"\n"
		"                        OpName %dataOutput \"dataOutput\"\n")
		+ opNameFrag +
		"                        OpName %fragColor \"fragColor\"\n"
		"                        OpName %vtxColor \"vtxColor\"\n"
		"                        OpMemberDecorate %Output 0 Offset 0\n"
		"                        OpDecorate %Output BufferBlock\n"
		"                        OpDecorate %dataOutput DescriptorSet 0\n"
		"                        OpDecorate %dataOutput Binding 0\n"
		"                        OpDecorate %dataIn Location 0\n"
		"                        OpDecorate %fragColor Location 0\n"
		"                        OpDecorate %vtxColor Location 1\n"
		"                %void = OpTypeVoid\n"
		"                   %3 = OpTypeFunction %void\n"
		"               %float = OpTypeFloat 32\n"
		"              %Output = OpTypeStruct %float\n"
		" %_ptr_Uniform_Output = OpTypePointer Uniform %Output\n"
		"          %dataOutput = OpVariable %_ptr_Uniform_Output Uniform\n"
		"                 %int = OpTypeInt 32 1\n"
		"               %int_0 = OpConstant %int 0\n"
		"    %_ptr_Input_float = OpTypePointer Input %float\n"
		"              %dataIn = OpVariable %_ptr_Input_float Input\n"
		"  %_ptr_Uniform_float = OpTypePointer Uniform %float\n"
		"             %v4float = OpTypeVector %float 4\n"
		" %_ptr_Output_v4float = OpTypePointer Output %v4float\n"
		"           %fragColor = OpVariable %_ptr_Output_v4float Output\n"
		"  %_ptr_Input_v4float = OpTypePointer Input %v4float\n"
		"            %vtxColor = OpVariable %_ptr_Input_v4float Input\n"
		"                %main = OpFunction %void None %3\n"
		"                   %5 = OpLabel\n"
		"                  %14 = OpLoad %float %dataIn\n"
		"                  %16 = OpAccessChain %_ptr_Uniform_float %dataOutput %int_0\n"
		"                        OpStore %16 %14\n"
		"                  %22 = OpLoad %v4float %vtxColor\n"
		"                        OpStore %fragColor %22\n"
		"                        OpReturn\n"
		"                        OpFunctionEnd\n";

	dst.spirvAsmSources.add("vert", DE_NULL) << vertexShader << SpirVAsmBuildOptions(vulkanVersion, targetSpirvVersion);
	dst.spirvAsmSources.add("frag", DE_NULL) << fragmentShader << SpirVAsmBuildOptions(vulkanVersion, targetSpirvVersion);
}

void createShadersNamesMatch (vk::SourceCollections& dst, InstanceContext context)
{
	createShaders(dst, context, "data", "data");
}

void createShadersNamesDiffer (vk::SourceCollections& dst, InstanceContext context)
{
	createShaders(dst, context, "dataOut", "dataIn");
}

void createShadersNoNames (vk::SourceCollections& dst, InstanceContext context)
{
	createShaders(dst, context, "", "");
}

void addGraphicsVaryingNameTest (tcu::TestCaseGroup* group, const TestParams& params)
{
	map<string, string>		fragments;
	RGBA					defaultColors[4];
	SpecConstants			noSpecConstants;
	PushConstants			noPushConstants;
	GraphicsInterfaces		noInterfaces;
	vector<string>			extensions;
	VulkanFeatures			features;
	map<string, string>		noFragments;
	StageToSpecConstantMap	specConstantMap;
	GraphicsResources		resources;
	const vector<float>		expectedOutput(1, 1.0f);

	const ShaderElement		pipelineStages[]	=
	{
		ShaderElement("vert", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	specConstantMap[VK_SHADER_STAGE_VERTEX_BIT]		= noSpecConstants;
	specConstantMap[VK_SHADER_STAGE_FRAGMENT_BIT]	= noSpecConstants;

	getDefaultColors(defaultColors);

	features.coreFeatures.fragmentStoresAndAtomics = VK_TRUE;
	extensions.push_back("VK_KHR_storage_buffer_storage_class");

	resources.outputs.push_back(Resource(BufferSp(new Float32Buffer(expectedOutput)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	{
		const InstanceContext& instanceContext = createInstanceContext(pipelineStages,
				defaultColors,
				defaultColors,
				noFragments,
				specConstantMap,
				noPushConstants,
				resources,
				noInterfaces,
				extensions,
				features,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				QP_TEST_RESULT_FAIL,
				string());

		addFunctionCaseWithPrograms<InstanceContext>(group,
				params.name.c_str(),
				"",
				params.createShaders,
				runAndVerifyDefaultPipeline,
				instanceContext);
	}
}

} // anonymous

tcu::TestCaseGroup* createVaryingNameGraphicsGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group		(new tcu::TestCaseGroup(testCtx, "varying_name", "Graphics tests for varying names."));

	static const TestParams			params[]	=
	{
		{ "names_match",	createShadersNamesMatch		},
		{ "names_differ",	createShadersNamesDiffer	},
		{ "no_names",		createShadersNoNames		}
	};

	for (deUint32 paramIdx = 0; paramIdx < DE_LENGTH_OF_ARRAY(params); paramIdx++)
		addGraphicsVaryingNameTest(group.get(), params[paramIdx]);

	return group.release();
}

} // SpirVAssembly
} // vkt
