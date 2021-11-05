/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Max Varying Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMaxVaryingsTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktPipelineSpecConstantUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include <string.h>

namespace vkt
{
namespace pipeline
{
namespace
{
using namespace vk;
using de::UniquePtr;
using de::MovePtr;

struct MaxVaryingsParam
{
	VkShaderStageFlags	outputStage;
	VkShaderStageFlags	inputStage;
	VkShaderStageFlags  stageToStressIO;
	MaxVaryingsParam(VkShaderStageFlags out, VkShaderStageFlags in, VkShaderStageFlags stageToTest)
		: outputStage(out), inputStage(in), stageToStressIO(stageToTest) {}
};

struct SelectedShaders
{
	VkShaderStageFlagBits	stage;
	std::string				shaderName;
	SelectedShaders(VkShaderStageFlagBits shaderStage, std::string name)
		: stage(shaderStage), shaderName(name) {}
};

// Helper functions
std::string getShaderStageName(VkShaderStageFlags stage)
{
	switch (stage)
	{
		default:
			DE_FATAL("Unhandled stage!");
			return "";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "compute";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "fragment";
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "vertex";
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return "geometry";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return "tess_control";
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return "tess_eval";
	}
}

const std::string generateTestName (struct MaxVaryingsParam param)
{
	std::ostringstream result;

	result << "test_" << getShaderStageName(param.stageToStressIO) << "_io_between_";
	result << getShaderStageName(param.outputStage) << "_";
	result << getShaderStageName(param.inputStage);
	return result.str();
}

const std::string generateTestDescription ()
{
	std::string result("Tests to check max varyings per stage");
	return result;
}

void initPrograms (SourceCollections& programCollection, MaxVaryingsParam param)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	// Vertex shader. SPIR-V generated from:
	// #version 450
	// layout(location = 0) in highp vec4 pos;
	// layout(constant_id = 0) const int arraySize = 1;
	// layout(location = 0) out ivec4 outputData[arraySize];
	// out gl_PerVertex {
	//    vec4 gl_Position;
	// };
	//
	// void main()
	// {
	//     gl_Position = pos;
	//     int i;
	//     for (i = 0; i &lt; arraySize; i++)
	//     {
	//         outputData[i] = ivec4(i);
	//     }
	// }
	std::ostringstream	vertex_out;
	vertex_out << "OpCapability Shader\n"
			   << "%1 = OpExtInstImport \"GLSL.std.450\"\n"
			   << "OpMemoryModel Logical GLSL450\n"
			   << "OpEntryPoint Vertex %4 \"main\" %10 %14 %32\n"
			   << "OpMemberDecorate %8 0 BuiltIn Position\n"
			   << "OpDecorate %8 Block\n"
			   << "OpDecorate %14 Location 0\n"
			   << "OpDecorate %26 SpecId 0\n"
			   << "OpDecorate %32 Location 0\n"
			   << "%2 = OpTypeVoid\n"
			   << "%3 = OpTypeFunction %2\n"
			   << "%6 = OpTypeFloat 32\n"
			   << "%7 = OpTypeVector %6 4\n"
			   << "%8 = OpTypeStruct %7\n"
			   << "%9 = OpTypePointer Output %8\n"
			   << "%10 = OpVariable %9 Output\n"
			   << "%11 = OpTypeInt 32 1\n"
			   << "%12 = OpConstant %11 0\n"
			   << "%13 = OpTypePointer Input %7\n"
			   << "%14 = OpVariable %13 Input\n"
			   << "%16 = OpTypePointer Output %7\n"
			   << "%18 = OpTypePointer Function %11\n"
			   << "%26 = OpSpecConstant %11 1\n"
			   << "%27 = OpTypeBool\n"
			   << "%29 = OpTypeVector %11 4\n"
			   << "%30 = OpTypeArray %29 %26\n"
			   << "%31 = OpTypePointer Output %30\n"
			   << "%32 = OpVariable %31 Output\n"
			   << "%36 = OpTypePointer Output %29\n"
			   << "%39 = OpConstant %11 1\n"
			   << "%4 = OpFunction %2 None %3\n"
			   << "%5 = OpLabel\n"
			   << "%19 = OpVariable %18 Function\n"
			   << "%15 = OpLoad %7 %14\n"
			   << "%17 = OpAccessChain %16 %10 %12\n"
			   << "OpStore %17 %15\n"
			   << "OpStore %19 %12\n"
			   << "OpBranch %20\n"
			   << "%20 = OpLabel\n"
			   << "OpLoopMerge %22 %23 None\n"
			   << "OpBranch %24\n"
			   << "%24 = OpLabel\n"
			   << "%25 = OpLoad %11 %19\n"
			   << "%28 = OpSLessThan %27 %25 %26\n"
			   << "OpBranchConditional %28 %21 %22\n"
			   << "%21 = OpLabel\n"
			   << "%33 = OpLoad %11 %19\n"
			   << "%34 = OpLoad %11 %19\n"
			   << "%35 = OpCompositeConstruct %29 %34 %34 %34 %34\n"
			   << "%37 = OpAccessChain %36 %32 %33\n"
			   << "OpStore %37 %35\n"
			   << "OpBranch %23\n"
			   << "%23 = OpLabel\n"
			   << "%38 = OpLoad %11 %19\n"
			   << "%40 = OpIAdd %11 %38 %39\n"
			   << "OpStore %19 %40\n"
			   << "OpBranch %20\n"
			   << "%22 = OpLabel\n"
			   << "OpReturn\n"
			   << "OpFunctionEnd\n";

	// Vertex shader passthrough. SPIR-V generated from:
	// #version 450
	// layout(location = 0) in highp vec4 pos;
	// out gl_PerVertex {
	//    vec4 gl_Position;
	// };
	// void main()
	// {
	//     gl_Position = pos;
	// }
	std::ostringstream	vertex_passthrough;
	vertex_passthrough << "OpCapability Shader\n"
					   << "%1 = OpExtInstImport \"GLSL.std.450\"\n"
					   << "OpMemoryModel Logical GLSL450\n"
					   << "OpEntryPoint Vertex %4 \"main\" %10 %14\n"
					   << "OpMemberDecorate %8 0 BuiltIn Position\n"
					   << "OpDecorate %8 Block\n"
					   << "OpDecorate %14 Location 0\n"
					   << "%2 = OpTypeVoid\n"
					   << "%3 = OpTypeFunction %2\n"
					   << "%6 = OpTypeFloat 32\n"
					   << "%7 = OpTypeVector %6 4\n"
					   << "%8 = OpTypeStruct %7\n"
					   << "%9 = OpTypePointer Output %8\n"
					   << "%10 = OpVariable %9 Output\n"
					   << "%11 = OpTypeInt 32 1\n"
					   << "%12 = OpConstant %11 0\n"
					   << "%13 = OpTypePointer Input %7\n"
					   << "%14 = OpVariable %13 Input\n"
					   << "%16 = OpTypePointer Output %7\n"
					   << "%4 = OpFunction %2 None %3\n"
					   << "%5 = OpLabel\n"
					   << "%15 = OpLoad %7 %14\n"
					   << "%17 = OpAccessChain %16 %10 %12\n"
					   << "OpStore %17 %15\n"
					   << "OpReturn\n"
					   << "OpFunctionEnd\n";

	// Tesselation Control shader. SPIR-V generated from:
	// #version 450
	// layout(vertices = 3) out;
	// in gl_PerVertex
	// {
	//   vec4 gl_Position;
	// } gl_in[];
	// out gl_PerVertex
	// {
	//   vec4 gl_Position;
	// } gl_out[];
	// void main(void)
	// {
	//     if (gl_InvocationID == 0) {
	//         gl_TessLevelInner[0] = 1.0;
	//         gl_TessLevelInner[1] = 1.0;
	//         gl_TessLevelOuter[0] = 1.0;
	//         gl_TessLevelOuter[1] = 1.0;
	//         gl_TessLevelOuter[2] = 1.0;
	//         gl_TessLevelOuter[3] = 1.0;
	//     }
	//     gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	// }
	std::ostringstream	tcs_passthrough;
	tcs_passthrough << "OpCapability Tessellation\n"
					<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
					<< "OpMemoryModel Logical GLSL450\n"
					<< "OpEntryPoint TessellationControl %4 \"main\" %8 %20 %29 %41 %47\n"
					<< "OpExecutionMode %4 OutputVertices 3\n"
					<< "OpDecorate %8 BuiltIn InvocationId\n"
					<< "OpDecorate %20 Patch\n"
					<< "OpDecorate %20 BuiltIn TessLevelInner\n"
					<< "OpDecorate %29 Patch\n"
					<< "OpDecorate %29 BuiltIn TessLevelOuter\n"
					<< "OpMemberDecorate %37 0 BuiltIn Position\n"
					<< "OpDecorate %37 Block\n"
					<< "OpMemberDecorate %43 0 BuiltIn Position\n"
					<< "OpDecorate %43 Block\n"
					<< "%2 = OpTypeVoid\n"
					<< "%3 = OpTypeFunction %2\n"
					<< "%6 = OpTypeInt 32 1\n"
					<< "%7 = OpTypePointer Input %6\n"
					<< "%8 = OpVariable %7 Input\n"
					<< "%10 = OpConstant %6 0\n"
					<< "%11 = OpTypeBool\n"
					<< "%15 = OpTypeFloat 32\n"
					<< "%16 = OpTypeInt 32 0\n"
					<< "%17 = OpConstant %16 2\n"
					<< "%18 = OpTypeArray %15 %17\n"
					<< "%19 = OpTypePointer Output %18\n"
					<< "%20 = OpVariable %19 Output\n"
					<< "%21 = OpConstant %15 1\n"
					<< "%22 = OpTypePointer Output %15\n"
					<< "%24 = OpConstant %6 1\n"
					<< "%26 = OpConstant %16 4\n"
					<< "%27 = OpTypeArray %15 %26\n"
					<< "%28 = OpTypePointer Output %27\n"
					<< "%29 = OpVariable %28 Output\n"
					<< "%32 = OpConstant %6 2\n"
					<< "%34 = OpConstant %6 3\n"
					<< "%36 = OpTypeVector %15 4\n"
					<< "%37 = OpTypeStruct %36\n"
					<< "%38 = OpConstant %16 3\n"
					<< "%39 = OpTypeArray %37 %38\n"
					<< "%40 = OpTypePointer Output %39\n"
					<< "%41 = OpVariable %40 Output\n"
					<< "%43 = OpTypeStruct %36\n"
					<< "%44 = OpConstant %16 32\n"
					<< "%45 = OpTypeArray %43 %44\n"
					<< "%46 = OpTypePointer Input %45\n"
					<< "%47 = OpVariable %46 Input\n"
					<< "%49 = OpTypePointer Input %36\n"
					<< "%52 = OpTypePointer Output %36\n"
					<< "%4 = OpFunction %2 None %3\n"
					<< "%5 = OpLabel\n"
					<< "%9 = OpLoad %6 %8\n"
					<< "%12 = OpIEqual %11 %9 %10\n"
					<< "OpSelectionMerge %14 None\n"
					<< "OpBranchConditional %12 %13 %14\n"
					<< "%13 = OpLabel\n"
					<< "%23 = OpAccessChain %22 %20 %10\n"
					<< "OpStore %23 %21\n"
					<< "%25 = OpAccessChain %22 %20 %24\n"
					<< "OpStore %25 %21\n"
					<< "%30 = OpAccessChain %22 %29 %10\n"
					<< "OpStore %30 %21\n"
					<< "%31 = OpAccessChain %22 %29 %24\n"
					<< "OpStore %31 %21\n"
					<< "%33 = OpAccessChain %22 %29 %32\n"
					<< "OpStore %33 %21\n"
					<< "%35 = OpAccessChain %22 %29 %34\n"
					<< "OpStore %35 %21\n"
					<< "OpBranch %14\n"
					<< "%14 = OpLabel\n"
					<< "%42 = OpLoad %6 %8\n"
					<< "%48 = OpLoad %6 %8\n"
					<< "%50 = OpAccessChain %49 %47 %48 %10\n"
					<< "%51 = OpLoad %36 %50\n"
					<< "%53 = OpAccessChain %52 %41 %42 %10\n"
					<< "OpStore %53 %51\n"
					<< "OpReturn\n"
					<< "OpFunctionEnd\n";

	// Tessellation Evaluation shader. SPIR-V generated from:
	// #version 450
	// layout(triangles, equal_spacing, cw) in;
	// layout(constant_id = 0) const int arraySize = 1;
	// layout(location = 0) out ivec4 outputData[arraySize];
	// in gl_PerVertex {
	//    vec4 gl_Position;
	// } gl_in[];
	// out gl_PerVertex {
	//    vec4 gl_Position;
	// };
	// void main(void)
	// {
	//     gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position +
	//                    gl_TessCoord.y * gl_in[1].gl_Position +
	//                    gl_TessCoord.z * gl_in[2].gl_Position);
	//     int j;
	//     for (j = 0; j &lt; arraySize; j++)
	//     {
	//         outputData[j] = ivec4(j);
	//     }
	// }
	std::ostringstream	tes_out;
	tes_out << "OpCapability Tessellation\n"
			<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "OpMemoryModel Logical GLSL450\n"
			<< "OpEntryPoint TessellationEvaluation %4 \"main\" %10 %15 %25 %62\n"
			<< "OpExecutionMode %4 Triangles\n"
			<< "OpExecutionMode %4 SpacingEqual\n"
			<< "OpExecutionMode %4 VertexOrderCw\n"
			<< "OpMemberDecorate %8 0 BuiltIn Position\n"
			<< "OpDecorate %8 Block\n"
			<< "OpDecorate %15 BuiltIn TessCoord\n"
			<< "OpMemberDecorate %21 0 BuiltIn Position\n"
			<< "OpDecorate %21 Block\n"
			<< "OpDecorate %56 SpecId 0\n"
			<< "OpDecorate %62 Location 0\n"
			<< "%2 = OpTypeVoid\n"
			<< "%3 = OpTypeFunction %2\n"
			<< "%6 = OpTypeFloat 32\n"
			<< "%7 = OpTypeVector %6 4\n"
			<< "%8 = OpTypeStruct %7\n"
			<< "%9 = OpTypePointer Output %8\n"
			<< "%10 = OpVariable %9 Output\n"
			<< "%11 = OpTypeInt 32 1\n"
			<< "%12 = OpConstant %11 0\n"
			<< "%13 = OpTypeVector %6 3\n"
			<< "%14 = OpTypePointer Input %13\n"
			<< "%15 = OpVariable %14 Input\n"
			<< "%16 = OpTypeInt 32 0\n"
			<< "%17 = OpConstant %16 0\n"
			<< "%18 = OpTypePointer Input %6\n"
			<< "%21 = OpTypeStruct %7\n"
			<< "%22 = OpConstant %16 32\n"
			<< "%23 = OpTypeArray %21 %22\n"
			<< "%24 = OpTypePointer Input %23\n"
			<< "%25 = OpVariable %24 Input\n"
			<< "%26 = OpTypePointer Input %7\n"
			<< "%30 = OpConstant %16 1\n"
			<< "%33 = OpConstant %11 1\n"
			<< "%38 = OpConstant %16 2\n"
			<< "%41 = OpConstant %11 2\n"
			<< "%46 = OpTypePointer Output %7\n"
			<< "%48 = OpTypePointer Function %11\n"
			<< "%56 = OpSpecConstant %11 1\n"
			<< "%57 = OpTypeBool\n"
			<< "%59 = OpTypeVector %11 4\n"
			<< "%60 = OpTypeArray %59 %56\n"
			<< "%61 = OpTypePointer Output %60\n"
			<< "%62 = OpVariable %61 Output\n"
			<< "%66 = OpTypePointer Output %59\n"
			<< "%4 = OpFunction %2 None %3\n"
			<< "%5 = OpLabel\n"
			<< "%49 = OpVariable %48 Function\n"
			<< "%19 = OpAccessChain %18 %15 %17\n"
			<< "%20 = OpLoad %6 %19\n"
			<< "%27 = OpAccessChain %26 %25 %12 %12\n"
			<< "%28 = OpLoad %7 %27\n"
			<< "%29 = OpVectorTimesScalar %7 %28 %20\n"
			<< "%31 = OpAccessChain %18 %15 %30\n"
			<< "%32 = OpLoad %6 %31\n"
			<< "%34 = OpAccessChain %26 %25 %33 %12\n"
			<< "%35 = OpLoad %7 %34\n"
			<< "%36 = OpVectorTimesScalar %7 %35 %32\n"
			<< "%37 = OpFAdd %7 %29 %36\n"
			<< "%39 = OpAccessChain %18 %15 %38\n"
			<< "%40 = OpLoad %6 %39\n"
			<< "%42 = OpAccessChain %26 %25 %41 %12\n"
			<< "%43 = OpLoad %7 %42\n"
			<< "%44 = OpVectorTimesScalar %7 %43 %40\n"
			<< "%45 = OpFAdd %7 %37 %44\n"
			<< "%47 = OpAccessChain %46 %10 %12\n"
			<< "OpStore %47 %45\n"
			<< "OpStore %49 %12\n"
			<< "OpBranch %50\n"
			<< "%50 = OpLabel\n"
			<< "OpLoopMerge %52 %53 None\n"
			<< "OpBranch %54\n"
			<< "%54 = OpLabel\n"
			<< "%55 = OpLoad %11 %49\n"
			<< "%58 = OpSLessThan %57 %55 %56\n"
			<< "OpBranchConditional %58 %51 %52\n"
			<< "%51 = OpLabel\n"
			<< "%63 = OpLoad %11 %49\n"
			<< "%64 = OpLoad %11 %49\n"
			<< "%65 = OpCompositeConstruct %59 %64 %64 %64 %64\n"
			<< "%67 = OpAccessChain %66 %62 %63\n"
			<< "OpStore %67 %65\n"
			<< "OpBranch %53\n"
			<< "%53 = OpLabel\n"
			<< "%68 = OpLoad %11 %49\n"
			<< "%69 = OpIAdd %11 %68 %33\n"
			<< "OpStore %49 %69\n"
			<< "OpBranch %50\n"
			<< "%52 = OpLabel\n"
			<< "OpReturn\n"
			<< "OpFunctionEnd\n";

	// Geometry shader. SPIR-V generated from:
	// #version 450
	// layout (triangles) in;
	// layout (triangle_strip, max_vertices = 3) out;
	// layout(constant_id = 0) const int arraySize = 1;
	// layout(location = 0) out ivec4 outputData[arraySize];
	// in gl_PerVertex {
	//    vec4 gl_Position;
	// } gl_in[];
	// void main()
	// {
	//     int i;
	//     int j;
	//     for(i = 0; i &lt; gl_in.length(); i++)
	//     {
	//         gl_Position = gl_in[i].gl_Position;
	//         for (j = 0; j &lt; arraySize; j++)
	//         {
	//             outputData[j] = ivec4(j);
	//         }
	//         EmitVertex();
	//     }
	//     EndPrimitive();
	// }
	std::ostringstream	geom_out;
	geom_out << "OpCapability Geometry\n"
			 << "%1 = OpExtInstImport \"GLSL.std.450\"\n"
			 << "OpMemoryModel Logical GLSL450\n"
			 << "OpEntryPoint Geometry %4 \"main\" %26 %31 %50\n"
			 << "OpExecutionMode %4 Triangles\n"
			 << "OpExecutionMode %4 Invocations 1\n"
			 << "OpExecutionMode %4 OutputTriangleStrip\n"
			 << "OpExecutionMode %4 OutputVertices 3\n"
			 << "OpMemberDecorate %24 0 BuiltIn Position\n"
			 << "OpDecorate %24 Block\n"
			 << "OpMemberDecorate %27 0 BuiltIn Position\n"
			 << "OpDecorate %27 Block\n"
			 << "OpDecorate %45 SpecId 0\n"
			 << "OpDecorate %50 Location 0\n"
			 << "%2 = OpTypeVoid\n"
			 << "%3 = OpTypeFunction %2\n"
			 << "%6 = OpTypeInt 32 1\n"
			 << "%7 = OpTypePointer Function %6\n"
			 << "%9 = OpConstant %6 0\n"
			 << "%16 = OpConstant %6 3\n"
			 << "%17 = OpTypeBool\n"
			 << "%19 = OpTypeFloat 32\n"
			 << "%20 = OpTypeVector %19 4\n"
			 << "%21 = OpTypeInt 32 0\n"
			 << "%22 = OpConstant %21 1\n"
			 << "%23 = OpTypeArray %19 %22\n"
			 << "%24 = OpTypeStruct %20\n"
			 << "%25 = OpTypePointer Output %24\n"
			 << "%26 = OpVariable %25 Output\n"
			 << "%27 = OpTypeStruct %20\n"
			 << "%28 = OpConstant %21 3\n"
			 << "%29 = OpTypeArray %27 %28\n"
			 << "%30 = OpTypePointer Input %29\n"
			 << "%31 = OpVariable %30 Input\n"
			 << "%33 = OpTypePointer Input %20\n"
			 << "%36 = OpTypePointer Output %20\n"
			 << "%45 = OpSpecConstant %6 1\n"
			 << "%47 = OpTypeVector %6 4\n"
			 << "%48 = OpTypeArray %47 %45\n"
			 << "%49 = OpTypePointer Output %48\n"
			 << "%50 = OpVariable %49 Output\n"
			 << "%54 = OpTypePointer Output %47\n"
			 << "%57 = OpConstant %6 1\n"
			 << "%4 = OpFunction %2 None %3\n"
			 << "%5 = OpLabel\n"
			 << "%8 = OpVariable %7 Function\n"
			 << "%38 = OpVariable %7 Function\n"
			 << "OpStore %8 %9\n"
			 << "OpBranch %10\n"
			 << "%10 = OpLabel\n"
			 << "OpLoopMerge %12 %13 None\n"
			 << "OpBranch %14\n"
			 << "%14 = OpLabel\n"
			 << "%15 = OpLoad %6 %8\n"
			 << "%18 = OpSLessThan %17 %15 %16\n"
			 << "OpBranchConditional %18 %11 %12\n"
			 << "%11 = OpLabel\n"
			 << "%32 = OpLoad %6 %8\n"
			 << "%34 = OpAccessChain %33 %31 %32 %9\n"
			 << "%35 = OpLoad %20 %34\n"
			 << "%37 = OpAccessChain %36 %26 %9\n"
			 << "OpStore %37 %35\n"
			 << "OpStore %38 %9\n"
			 << "OpBranch %39\n"
			 << "%39 = OpLabel\n"
			 << "OpLoopMerge %41 %42 None\n"
			 << "OpBranch %43\n"
			 << "%43 = OpLabel\n"
			 << "%44 = OpLoad %6 %38\n"
			 << "%46 = OpSLessThan %17 %44 %45\n"
			 << "OpBranchConditional %46 %40 %41\n"
			 << "%40 = OpLabel\n"
			 << "%51 = OpLoad %6 %38\n"
			 << "%52 = OpLoad %6 %38\n"
			 << "%53 = OpCompositeConstruct %47 %52 %52 %52 %52\n"
			 << "%55 = OpAccessChain %54 %50 %51\n"
			 << "OpStore %55 %53\n"
			 << "OpBranch %42\n"
			 << "%42 = OpLabel\n"
			 << "%56 = OpLoad %6 %38\n"
			 << "%58 = OpIAdd %6 %56 %57\n"
			 << "OpStore %38 %58\n"
			 << "OpBranch %39\n"
			 << "%41 = OpLabel\n"
			 << "OpEmitVertex\n"
			 << "OpBranch %13\n"
			 << "%13 = OpLabel\n"
			 << "%59 = OpLoad %6 %8\n"
			 << "%60 = OpIAdd %6 %59 %57\n"
			 << "OpStore %8 %60\n"
			 << "OpBranch %10\n"
			 << "%12 = OpLabel\n"
			 << "OpEndPrimitive\n"
			 << "OpReturn\n"
			 << "OpFunctionEnd\n";

	// Fragment shader. SPIR-V code generated from:
	//
	// #version 450
	// layout(constant_id = 0) const int arraySize = 1;
	// layout(location = 0) flat in ivec4 inputData[arraySize];
	// layout(location = 0) out vec4 color;
	// void main()
	// {
	//    color = vec4(1.0, 0.0, 0.0, 1.0);
	//    int i;
	//    bool result = true;
	//    for (i = 0; i &lt; arraySize; i++)
	//    {
	//        if (result &amp;&amp; inputData[i] != ivec4(i))
	//            result = false;
	//    }
	//    if (result)
	//      color = vec4(0.0, 1.0, 0.0, 1.0);
	// }
	std::ostringstream	fragment_in;
	fragment_in << "OpCapability Shader\n"
				<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
				<< "OpMemoryModel Logical GLSL450\n"
				<< "OpEntryPoint Fragment %4 \"main\" %9 %35\n"
				<< "OpExecutionMode %4 OriginUpperLeft\n"
				<< "OpDecorate %9 Location 0\n"
				<< "OpDecorate %27 SpecId 0\n"
				<< "OpDecorate %35 Flat\n"
				<< "OpDecorate %35 Location 0\n"
				<< "%2 = OpTypeVoid\n"
				<< "%3 = OpTypeFunction %2\n"
				<< "%6 = OpTypeFloat 32\n"
				<< "%7 = OpTypeVector %6 4\n"
				<< "%8 = OpTypePointer Output %7\n"
				<< "%9 = OpVariable %8 Output\n"
				<< "%10 = OpConstant %6 1\n"
				<< "%11 = OpConstant %6 0\n"
				<< "%12 = OpConstantComposite %7 %10 %11 %11 %10\n"
				<< "%13 = OpTypeBool\n"
				<< "%14 = OpTypePointer Function %13\n"
				<< "%16 = OpConstantTrue %13\n"
				<< "%17 = OpTypeInt 32 1\n"
				<< "%18 = OpTypePointer Function %17\n"
				<< "%20 = OpConstant %17 0\n"
				<< "%27 = OpSpecConstant %17 1\n"
				<< "%32 = OpTypeVector %17 4\n"
				<< "%33 = OpTypeArray %32 %27\n"
				<< "%34 = OpTypePointer Input %33\n"
				<< "%35 = OpVariable %34 Input\n"
				<< "%37 = OpTypePointer Input %32\n"
				<< "%42 = OpTypeVector %13 4\n"
				<< "%48 = OpConstantFalse %13\n"
				<< "%50 = OpConstant %17 1\n"
				<< "%55 = OpConstantComposite %7 %11 %10 %11 %10\n"
				<< "%4 = OpFunction %2 None %3\n"
				<< "%5 = OpLabel\n"
				<< "%15 = OpVariable %14 Function\n"
				<< "%19 = OpVariable %18 Function\n"
				<< "OpStore %9 %12\n"
				<< "OpStore %15 %16\n"
				<< "OpStore %19 %20\n"
				<< "OpBranch %21\n"
				<< "%21 = OpLabel\n"
				<< "OpLoopMerge %23 %24 None\n"
				<< "OpBranch %25\n"
				<< "%25 = OpLabel\n"
				<< "%26 = OpLoad %17 %19\n"
				<< "%28 = OpSLessThan %13 %26 %27\n"
				<< "OpBranchConditional %28 %22 %23\n"
				<< "%22 = OpLabel\n"
				<< "%29 = OpLoad %13 %15\n"
				<< "OpSelectionMerge %31 None\n"
				<< "OpBranchConditional %29 %30 %31\n"
				<< "%30 = OpLabel\n"
				<< "%36 = OpLoad %17 %19\n"
				<< "%38 = OpAccessChain %37 %35 %36\n"
				<< "%39 = OpLoad %32 %38\n"
				<< "%40 = OpLoad %17 %19\n"
				<< "%41 = OpCompositeConstruct %32 %40 %40 %40 %40\n"
				<< "%43 = OpINotEqual %42 %39 %41\n"
				<< "%44 = OpAny %13 %43\n"
				<< "OpBranch %31\n"
				<< "%31 = OpLabel\n"
				<< "%45 = OpPhi %13 %29 %22 %44 %30\n"
				<< "OpSelectionMerge %47 None\n"
				<< "OpBranchConditional %45 %46 %47\n"
				<< "%46 = OpLabel\n"
				<< "OpStore %15 %48\n"
				<< "OpBranch %47\n"
				<< "%47 = OpLabel\n"
				<< "OpBranch %24\n"
				<< "%24 = OpLabel\n"
				<< "%49 = OpLoad %17 %19\n"
				<< "%51 = OpIAdd %17 %49 %50\n"
				<< "OpStore %19 %51\n"
				<< "OpBranch %21\n"
				<< "%23 = OpLabel\n"
				<< "%52 = OpLoad %13 %15\n"
				<< "OpSelectionMerge %54 None\n"
				<< "OpBranchConditional %52 %53 %54\n"
				<< "%53 = OpLabel\n"
				<< "OpStore %9 %55\n"
				<< "OpBranch %54\n"
				<< "%54 = OpLabel\n"
				<< "OpReturn\n"
				<< "OpFunctionEnd\n";

	if (param.outputStage == VK_SHADER_STAGE_VERTEX_BIT)
	{
		programCollection.spirvAsmSources.add("vert")
			<< vertex_out.str().c_str();

		if (param.inputStage == VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			programCollection.spirvAsmSources.add("frag")
				<< fragment_in.str().c_str();
			return;
		}
	}

	programCollection.spirvAsmSources.add("vert")
		<< vertex_passthrough.str().c_str();

	if (param.outputStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		programCollection.spirvAsmSources.add("tcs")
			<< tcs_passthrough.str().c_str();
		programCollection.spirvAsmSources.add("tes")
			<< tes_out.str().c_str();

		if (param.inputStage == VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			programCollection.spirvAsmSources.add("frag")
				<< fragment_in.str().c_str();
			return;
		}
	}

	if (param.outputStage == VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		programCollection.spirvAsmSources.add("geom")
				<< geom_out.str().c_str();
		programCollection.spirvAsmSources.add("frag")
			<< fragment_in.str().c_str();
		return;
	}

	DE_FATAL("Unsupported combination");
}

void supportedCheck (Context& context, MaxVaryingsParam param)
{

	const vk::InstanceInterface&	vki = context.getInstanceInterface();
	VkPhysicalDeviceFeatures		features;
	vki.getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

	// Check support for the tessellation and geometry shaders on the device
	if ((param.inputStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
		 param.inputStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ||
		 param.outputStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
		 param.outputStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		&& !features.tessellationShader)
	{
		TCU_THROW(NotSupportedError, "Device does not support tessellation shaders");
	}

	if ((param.inputStage == VK_SHADER_STAGE_GEOMETRY_BIT || param.outputStage == VK_SHADER_STAGE_GEOMETRY_BIT) && !features.geometryShader)
	{
		TCU_THROW(NotSupportedError, "Device does not support geometry shaders");
	}

	// Check data sizes, throw unsupported if the case cannot be tested.
	VkPhysicalDeviceProperties properties;
	vki.getPhysicalDeviceProperties(context.getPhysicalDevice(), &properties);
	std::ostringstream	error;
	if (param.stageToStressIO == VK_SHADER_STAGE_VERTEX_BIT)
	{
		DE_ASSERT(param.outputStage == VK_SHADER_STAGE_VERTEX_BIT);
		if (param.inputStage == VK_SHADER_STAGE_FRAGMENT_BIT && properties.limits.maxFragmentInputComponents < (properties.limits.maxVertexOutputComponents - 4))
		{
			error << "Device supports smaller number of FS inputs (" << properties.limits.maxFragmentInputComponents << ") than VS outputs (" << properties.limits.maxVertexOutputComponents << " - 4 built-ins)";
			TCU_THROW(NotSupportedError, error.str().c_str());
		}
	}

	if (param.stageToStressIO == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		if (param.inputStage == VK_SHADER_STAGE_FRAGMENT_BIT && properties.limits.maxFragmentInputComponents < (properties.limits.maxTessellationEvaluationOutputComponents - 4))
		{
			error << "Device supports smaller number of FS inputs (" << properties.limits.maxFragmentInputComponents << ") than TES outputs (" << properties.limits.maxTessellationEvaluationOutputComponents << " - 4 builtins)";
			TCU_THROW(NotSupportedError, error.str().c_str());
		}
	}

	if (param.stageToStressIO == VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		if (param.inputStage == VK_SHADER_STAGE_FRAGMENT_BIT && properties.limits.maxFragmentInputComponents < (properties.limits.maxGeometryOutputComponents - 4))
		{
			error << "Device supports smaller number of FS inputs (" << properties.limits.maxFragmentInputComponents << ") than GS outputs (" << properties.limits.maxGeometryOutputComponents << " - 4 built-ins)";
			TCU_THROW(NotSupportedError, error.str().c_str());
		}
	}

	if (param.stageToStressIO == VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		DE_ASSERT(param.inputStage == VK_SHADER_STAGE_FRAGMENT_BIT);

		if (param.outputStage == VK_SHADER_STAGE_VERTEX_BIT && (properties.limits.maxVertexOutputComponents - 4) < properties.limits.maxFragmentInputComponents)
		{
			error << "Device supports smaller number of VS outputs (" << properties.limits.maxVertexOutputComponents << " - 4 built-ins) than FS inputs (" << properties.limits.maxFragmentInputComponents << ")";
			TCU_THROW(NotSupportedError, error.str().c_str());
		}
		if (param.outputStage == VK_SHADER_STAGE_GEOMETRY_BIT && (properties.limits.maxGeometryOutputComponents - 4) < properties.limits.maxFragmentInputComponents)
		{
			error << "Device supports smaller number of GS outputs (" << properties.limits.maxGeometryOutputComponents << " - 4 built-ins) than FS inputs (" << properties.limits.maxFragmentInputComponents << ")";
			TCU_THROW(NotSupportedError, error.str().c_str());
		}
		if (param.outputStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT && (properties.limits.maxTessellationEvaluationOutputComponents - 4) < properties.limits.maxFragmentInputComponents)
		{
			error << "Device supports smaller number of TES outputs (" << properties.limits.maxTessellationEvaluationOutputComponents << " - 4 built-ins) than FS inputs (" << properties.limits.maxFragmentInputComponents << ")";
			TCU_THROW(NotSupportedError, error.str().c_str());
		}
	}
}

VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(VkImageCreateFlags)0,						// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,							// VkImageType				imageType;
		format,										// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),		// VkExtent3D				extent;
		1u,											// uint32_t					mipLevels;
		1u,											// uint32_t					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
		usage,										// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
		0u,											// uint32_t					queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			initialLayout;
	};
	return imageInfo;
}

Move<VkBuffer> makeBuffer (const DeviceInterface& vk, const VkDevice device, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage)
{
	const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferSize, usage);
	return createBuffer(vk, device, &bufferCreateInfo);
}

void recordImageBarrier (const DeviceInterface&				vk,
						 const VkCommandBuffer				cmdBuffer,
						 const VkImage						image,
						 const VkImageAspectFlags			aspect,
						 const VkPipelineStageFlags			srcStageMask,
						 const VkPipelineStageFlags			dstStageMask,
						 const VkAccessFlags				srcAccessMask,
						 const VkAccessFlags				dstAccessMask,
						 const VkImageLayout				oldLayout,
						 const VkImageLayout				newLayout,
						 const VkSampleLocationsInfoEXT*	pSampleLocationsInfo = DE_NULL)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,						// VkStructureType			sType;
		pSampleLocationsInfo,										// const void*				pNext;
		srcAccessMask,												// VkAccessFlags			srcAccessMask;
		dstAccessMask,												// VkAccessFlags			dstAccessMask;
		oldLayout,													// VkImageLayout			oldLayout;
		newLayout,													// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,									// uint32_t					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// uint32_t					dstQueueFamilyIndex;
		image,														// VkImage					image;
		makeImageSubresourceRange(aspect, 0u, 1u, 0u, 1u),			// VkImageSubresourceRange	subresourceRange;
	};

	vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (VkDependencyFlags)0, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
}

void recordCopyImageToBuffer (const DeviceInterface&	vk,
							  const VkCommandBuffer		cmdBuffer,
							  const tcu::IVec2&			imageSize,
							  const VkImage				srcImage,
							  const VkBuffer			dstBuffer)
{
	// Resolve image -> host buffer
	{
		const VkBufferImageCopy region =
		{
			0ull,																// VkDeviceSize				bufferOffset;
			0u,																	// uint32_t					bufferRowLength;
			0u,																	// uint32_t					bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	// VkImageSubresourceLayers	imageSubresource;
			makeOffset3D(0, 0, 0),												// VkOffset3D				imageOffset;
			makeExtent3D(imageSize.x(), imageSize.y(), 1u),						// VkExtent3D				imageExtent;
		};

		vk.cmdCopyImageToBuffer(cmdBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuffer, 1u, &region);
	}
	// Buffer write barrier
	{
		const VkBufferMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType	sType;
			DE_NULL,										// const void*		pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags	srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,						// VkAccessFlags	dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t			srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t			dstQueueFamilyIndex;
			dstBuffer,										// VkBuffer			buffer;
			0ull,											// VkDeviceSize		offset;
			VK_WHOLE_SIZE,									// VkDeviceSize		size;
		};

		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
							  0u, DE_NULL, 1u, &barrier, DE_NULL, 0u);
	}
}

Move<VkBuffer> createBufferAndBindMemory (Context& context, VkDeviceSize size, VkBufferUsageFlags usage, de::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		size,										// VkDeviceSize			size;
		usage,										// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer> vertexBuffer = createBuffer(vk, vkDevice, &vertexBufferParams);

	*pAlloc = context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	return vertexBuffer;
}

deInt32 getMaxIOComponents(deBool input, VkShaderStageFlags stage, VkPhysicalDeviceProperties properties)
{
	deInt32 data = 0u;
	switch (stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:
		DE_ASSERT(!input);
		data = (properties.limits.maxVertexOutputComponents / 4) - 1; // outputData + gl_Position
		break;

	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		if (input)
			data = properties.limits.maxTessellationEvaluationInputComponents / 4;
		else
			data = (properties.limits.maxTessellationEvaluationOutputComponents / 4) - 1; // outputData + gl_Position
		break;

	case VK_SHADER_STAGE_GEOMETRY_BIT:
		if (input)
			data = properties.limits.maxGeometryInputComponents / 4;
		else
			data = (properties.limits.maxGeometryOutputComponents / 4) - 1; // outputData + gl_Position
		break;

	case VK_SHADER_STAGE_FRAGMENT_BIT:
		DE_ASSERT(input);
		data = (properties.limits.maxFragmentInputComponents / 4); // inputData
		break;
	default:
		DE_FATAL("Unsupported shader");
	}

	return data;
}

tcu::TestStatus test(Context& context, const MaxVaryingsParam param)
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				device				= context.getDevice();
	const VkQueue				queue				= context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= context.getDefaultAllocator();
	tcu::TestLog				&log				= context.getTestContext().getLog();


	// Color attachment
	const tcu::IVec2			renderSize		= tcu::IVec2(32, 32);
	const VkFormat				imageFormat	= VK_FORMAT_R8G8B8A8_UNORM;
	const Image				colorImage		(vk, device, allocator, makeImageCreateInfo(renderSize, imageFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), MemoryRequirement::Any);
	const Unique<VkImageView> colorImageView	(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, imageFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));
	const VkDeviceSize	colorBufferSize		= renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(imageFormat));
	Move<VkBuffer>		colorBuffer			= vkt::pipeline::makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	MovePtr<Allocation>	colorBufferAlloc	= bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible);


	// Create vertex buffer
	de::MovePtr<Allocation>				vertexBufferMemory;
	Move<VkBuffer>		vertexBuffer	= createBufferAndBindMemory(context, sizeof(tcu::Vec4) * 6u, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vertexBufferMemory);
	std::vector<tcu::Vec4>			vertices;
	{
		vertices.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
		vertices.push_back(tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
		vertices.push_back(tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f));
		vertices.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
		vertices.push_back(tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f));
		vertices.push_back(tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f));
		// Load vertices into vertex buffer
		deMemcpy(vertexBufferMemory->getHostPtr(), vertices.data(), vertices.size() * sizeof(tcu::Vec4));
		flushAlloc(vk, device, *vertexBufferMemory);
	}

	// Specialization
	VkPhysicalDeviceProperties properties;
	vki.getPhysicalDeviceProperties(context.getPhysicalDevice(), &properties);
	VkPhysicalDeviceFeatures features;
	vki.getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

	deInt32		data		= 0u;
	size_t		dataSize	= sizeof(data);
	deInt32		maxOutput	= getMaxIOComponents(false, param.outputStage, properties);
	deInt32		maxInput	= getMaxIOComponents(true, param.inputStage, properties);

	data = deMin32(maxOutput, maxInput);

	DE_ASSERT(data != 0u);

	log << tcu::TestLog::Message << "Testing " << data * 4 << " input components for stage " << getShaderStageName(param.stageToStressIO).c_str() << tcu::TestLog::EndMessage;

	VkSpecializationMapEntry	mapEntries =
	{
		0u,							// deUint32	constantID;
		0u,							// deUint32	offset;
		dataSize					// size_t	size;
	};

	VkSpecializationInfo		pSpecInfo =
	{
		1u,							// deUint32							mapEntryCount;
		&mapEntries,				// const VkSpecializationMapEntry*	pMapEntries;
		dataSize,					// size_t							dataSize;
		&data						// const void*						pData;
	};

	// Pipeline

	const Unique<VkRenderPass>		renderPass		(makeRenderPass	(vk, device, imageFormat));
	const Unique<VkFramebuffer>	framebuffer	(makeFramebuffer	(vk, device, *renderPass, 1u, &colorImageView.get(), static_cast<deUint32>(renderSize.x()), static_cast<deUint32>(renderSize.y())));
	const Unique<VkPipelineLayout>	pipelineLayout	(makePipelineLayout(vk, device));
	const Unique<VkCommandPool>	cmdPool		(createCommandPool (vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer		(makeCommandBuffer (vk, device, *cmdPool));

	GraphicsPipelineBuilder pipelineBuilder;
	pipelineBuilder
		.setRenderSize(renderSize);

	// Get the shaders to run
	std::vector<SelectedShaders>	shaders;
	shaders.push_back(SelectedShaders(VK_SHADER_STAGE_VERTEX_BIT, "vert"));
	shaders.push_back(SelectedShaders(VK_SHADER_STAGE_FRAGMENT_BIT, "frag"));

	if (param.inputStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || param.outputStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
		param.inputStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT || param.outputStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		shaders.push_back(SelectedShaders(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tcs"));
		shaders.push_back(SelectedShaders(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tes"));
	}
	if (param.inputStage == VK_SHADER_STAGE_GEOMETRY_BIT || param.outputStage == VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		shaders.push_back(SelectedShaders(VK_SHADER_STAGE_GEOMETRY_BIT, "geom"));
	}

	for (deUint32 i = 0; i < (deUint32)shaders.size(); i++)
	{
		pipelineBuilder.setShader(vk, device, shaders[i].stage, context.getBinaryCollection().get(shaders[i].shaderName.c_str()), &pSpecInfo);
	}

	const Unique<VkPipeline> pipeline (pipelineBuilder.build(vk, device, *pipelineLayout, *renderPass));

	// Draw commands

	const VkRect2D		renderArea			= makeRect2D(renderSize);
	const tcu::Vec4		clearColor			(0.0f, 0.0f, 0.0f, 1.0f);

	beginCommandBuffer(vk, *cmdBuffer);

	{
		const VkImageSubresourceRange imageFullSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const VkImageMemoryBarrier    barrierColorAttachmentSetInitialLayout	= makeImageMemoryBarrier(
			0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			*colorImage, imageFullSubresourceRange);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
			0u, DE_NULL, 0u, DE_NULL, 1u, &barrierColorAttachmentSetInitialLayout);
	}

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	const VkDeviceSize vertexBufferOffset = 0ull;
	vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

	// Draw one vertex
	vk.cmdDraw(*cmdBuffer, (deUint32)vertices.size(), 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuffer);
	// Resolve image -> host buffer
	recordImageBarrier(vk, *cmdBuffer, *colorImage,
						VK_IMAGE_ASPECT_COLOR_BIT,								// VkImageAspectFlags	aspect,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,			// VkPipelineStageFlags srcStageMask,
						VK_PIPELINE_STAGE_TRANSFER_BIT,							// VkPipelineStageFlags dstStageMask,
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,					// VkAccessFlags		srcAccessMask,
						VK_ACCESS_TRANSFER_READ_BIT,							// VkAccessFlags		dstAccessMask,
						VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout		oldLayout,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);					// VkImageLayout		newLayout)

	recordCopyImageToBuffer(vk, *cmdBuffer, renderSize, *colorImage, *colorBuffer);
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify results
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);

		const tcu::ConstPixelBufferAccess	resultImage		(mapVkFormat(imageFormat), renderSize.x(), renderSize.y(), 1u, colorBufferAlloc->getHostPtr());
		tcu::TextureLevel	referenceImage (mapVkFormat(imageFormat), renderSize.x(), renderSize.y());
		tcu::clear(referenceImage.getAccess(), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));

		if (!tcu::floatThresholdCompare(log, "Compare", "Result comparison", referenceImage.getAccess(), resultImage, tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			TCU_FAIL("Rendered image is not correct");
	}
	return tcu::TestStatus::pass("OK");
}
} // anonymous

tcu::TestCaseGroup* createMaxVaryingsTests (tcu::TestContext& testCtx)
{
	std::vector<MaxVaryingsParam> tests;

	tests.push_back(MaxVaryingsParam(VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_VERTEX_BIT)); // Test max vertex outputs: VS-FS
	tests.push_back(MaxVaryingsParam(VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_FRAGMENT_BIT)); // Test max FS inputs: VS-FS
	tests.push_back(MaxVaryingsParam(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)); // Test max tess evaluation outputs: VS-TCS-TES-FS
	tests.push_back(MaxVaryingsParam(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_FRAGMENT_BIT)); // Test fragment inputs: VS-TCS-TES-FS
	tests.push_back(MaxVaryingsParam(VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_GEOMETRY_BIT)); // Test geometry outputs: VS-GS-FS
	tests.push_back(MaxVaryingsParam(VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_FRAGMENT_BIT)); // Test fragment inputs: VS-GS-FS

	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "max_varyings", "Max Varyings tests"));

	for (deUint32 testIndex = 0; testIndex < (deUint32)tests.size(); ++testIndex)
	{
		MaxVaryingsParam testParams = tests[testIndex];
		addFunctionCaseWithPrograms(group.get(), generateTestName(testParams), generateTestDescription(),
									supportedCheck, initPrograms, test, testParams);
	}

	return group.release();
}

} // pipeline
} // vkt
