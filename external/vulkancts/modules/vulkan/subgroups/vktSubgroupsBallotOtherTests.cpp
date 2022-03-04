/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
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
 */ /*!
 * \file
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsBallotOtherTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
enum OpType
{
	OPTYPE_INVERSE_BALLOT = 0,
	OPTYPE_BALLOT_BIT_EXTRACT,
	OPTYPE_BALLOT_BIT_COUNT,
	OPTYPE_BALLOT_INCLUSIVE_BIT_COUNT,
	OPTYPE_BALLOT_EXCLUSIVE_BIT_COUNT,
	OPTYPE_BALLOT_FIND_LSB,
	OPTYPE_BALLOT_FIND_MSB,
	OPTYPE_LAST
};

struct CaseDefinition
{
	OpType				opType;
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

bool checkVertexPipelineStages (const void*			internalData,
								vector<const void*>	datas,
								deUint32			width,
								deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 0xf);
}

bool checkCompute (const void*			internalData,
				   vector<const void*>	datas,
				   const deUint32		numWorkgroups[3],
				   const deUint32		localSize[3],
				   deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkCompute(datas, numWorkgroups, localSize, 0xf);
}

string getOpTypeName (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_INVERSE_BALLOT:				return "subgroupInverseBallot";
		case OPTYPE_BALLOT_BIT_EXTRACT:			return "subgroupBallotBitExtract";
		case OPTYPE_BALLOT_BIT_COUNT:			return "subgroupBallotBitCount";
		case OPTYPE_BALLOT_INCLUSIVE_BIT_COUNT:	return "subgroupBallotInclusiveBitCount";
		case OPTYPE_BALLOT_EXCLUSIVE_BIT_COUNT:	return "subgroupBallotExclusiveBitCount";
		case OPTYPE_BALLOT_FIND_LSB:			return "subgroupBallotFindLSB";
		case OPTYPE_BALLOT_FIND_MSB:			return "subgroupBallotFindMSB";
		default:								TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getExtHeader (const CaseDefinition&)
{
	return	"#extension GL_KHR_shader_subgroup_ballot: enable\n";
}

vector<string> getPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	const bool		fragment	= (caseDef.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
	vector<string>	result		(stageCount, string());

	if (fragment)
		result.reserve(result.size() + 1);

	for (size_t i = 0; i < result.size(); ++i)
	{
		result[i] =
			"layout(set = 0, binding = " + de::toString(i) + ", std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n";
	}

	if (fragment)
	{
		const string	fragPart	=
			"layout(location = 0) out uint result;\n"
			"precision highp int;\n";

		result.push_back(fragPart);
	}

	return result;
}

vector<string> getFramebufferPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	vector<string>	result;

	DE_UNREF(caseDef);

	result.push_back("layout(location = 0) out float result;\n");
	result.push_back("layout(location = 0) out float out_color;\n");
	result.push_back("layout(location = 0) out float out_color[];\n");
	result.push_back("layout(location = 0) out float out_color;\n");

	return result;
}

string getTestString (const CaseDefinition& caseDef)
{
	ostringstream bdy;

	bdy << "  uvec4 allOnes = uvec4(0xFFFFFFFF);\n"
		<< "  uvec4 allZeros = uvec4(0);\n"
		<< "  uint tempResult = 0;\n"
		<< "#define MAKE_HIGH_BALLOT_RESULT(i) uvec4("
		<< "i >= 32 ? 0 : (0xFFFFFFFF << i), "
		<< "i >= 64 ? 0 : (0xFFFFFFFF << ((i < 32) ? 0 : (i - 32))), "
		<< "i >= 96 ? 0 : (0xFFFFFFFF << ((i < 64) ? 0 : (i - 64))), "
		<< "i >= 128 ? 0 : (0xFFFFFFFF << ((i < 96) ? 0 : (i - 96))))\n"
		<< "#define MAKE_SINGLE_BIT_BALLOT_RESULT(i) uvec4("
		<< "i >= 32 ? 0 : 0x1 << i, "
		<< "i < 32 || i >= 64 ? 0 : 0x1 << (i - 32), "
		<< "i < 64 || i >= 96 ? 0 : 0x1 << (i - 64), "
		<< "i < 96 || i >= 128 ? 0 : 0x1 << (i - 96))\n";

	switch (caseDef.opType)
	{
		default:
			DE_FATAL("Unknown op type!");
			break;
		case OPTYPE_INVERSE_BALLOT:
			bdy << "  tempResult |= subgroupInverseBallot(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= subgroupInverseBallot(allZeros) ? 0 : 0x2;\n"
				<< "  tempResult |= subgroupInverseBallot(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n";
			break;
		case OPTYPE_BALLOT_BIT_EXTRACT:
			bdy << "  tempResult |= subgroupBallotBitExtract(allOnes, gl_SubgroupInvocationID) ? 0x1 : 0;\n"
				<< "  tempResult |= subgroupBallotBitExtract(allZeros, gl_SubgroupInvocationID) ? 0 : 0x2;\n"
				<< "  tempResult |= subgroupBallotBitExtract(subgroupBallot(true), gl_SubgroupInvocationID) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < gl_SubgroupSize; i++)\n"
				<< "  {\n"
				<< "    if (!subgroupBallotBitExtract(allOnes, gl_SubgroupInvocationID))\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
		case OPTYPE_BALLOT_BIT_COUNT:
			bdy << "  /* To ensure a 32-bit computation, use a variable with default highp precision. */\n"
				<< "  uint SubgroupSize = gl_SubgroupSize;\n"
				<< "  tempResult |= SubgroupSize == subgroupBallotBitCount(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotBitCount(allZeros) ? 0x2 : 0;\n"
				<< "  tempResult |= 0 < subgroupBallotBitCount(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotBitCount(MAKE_HIGH_BALLOT_RESULT(SubgroupSize)) ? 0x8 : 0;\n";
			break;
		case OPTYPE_BALLOT_INCLUSIVE_BIT_COUNT:
			bdy << "  uint inclusiveOffset = gl_SubgroupInvocationID + 1;\n"
				<< "  tempResult |= inclusiveOffset == subgroupBallotInclusiveBitCount(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotInclusiveBitCount(allZeros) ? 0x2 : 0;\n"
				<< "  tempResult |= 0 < subgroupBallotInclusiveBitCount(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < 128; i++)\n"
				<< "  {\n"
				<< "    uint ref = inclusiveOffset - min(inclusiveOffset, i);\n"
				<< "    uvec4 b = MAKE_HIGH_BALLOT_RESULT(i);\n"
				<< "    uint inclusiveBitCount = subgroupBallotInclusiveBitCount(b);\n"
				<< "    if (inclusiveBitCount != ref)\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
		case OPTYPE_BALLOT_EXCLUSIVE_BIT_COUNT:
			bdy << "  uint exclusiveOffset = gl_SubgroupInvocationID;\n"
				<< "  tempResult |= exclusiveOffset == subgroupBallotExclusiveBitCount(allOnes) ? 0x1 : 0;\n"
				<< "  tempResult |= 0 == subgroupBallotExclusiveBitCount(allZeros) ? 0x2 : 0;\n"
				<< "  tempResult |= 0x4;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < 128; i++)\n"
				<< "  {\n"
				<< "    uint ref = exclusiveOffset - min(exclusiveOffset, i);\n"
				<< "    uvec4 b = MAKE_HIGH_BALLOT_RESULT(i);\n"
				<< "    uint exclusiveBitCount = subgroupBallotExclusiveBitCount(b);\n"
				<< "    if (exclusiveBitCount != ref)\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
		case OPTYPE_BALLOT_FIND_LSB:
			bdy << "  tempResult |= 0 == subgroupBallotFindLSB(allOnes) ? 0x1 : 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    tempResult |= 0x2;\n"
				<< "  }\n"
				<< "  else\n"
				<< "  {\n"
				<< "    tempResult |= 0 < subgroupBallotFindLSB(subgroupBallot(true)) ? 0x2 : 0;\n"
				<< "  }\n"
				<< "  tempResult |= gl_SubgroupSize > subgroupBallotFindLSB(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < gl_SubgroupSize; i++)\n"
				<< "  {\n"
				<< "    if (i != subgroupBallotFindLSB(MAKE_HIGH_BALLOT_RESULT(i)))\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
		case OPTYPE_BALLOT_FIND_MSB:
			bdy << "  tempResult |= (gl_SubgroupSize - 1) == subgroupBallotFindMSB(allOnes) ? 0x1 : 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    tempResult |= 0x2;\n"
				<< "  }\n"
				<< "  else\n"
				<< "  {\n"
				<< "    tempResult |= 0 < subgroupBallotFindMSB(subgroupBallot(true)) ? 0x2 : 0;\n"
				<< "  }\n"
				<< "  tempResult |= gl_SubgroupSize > subgroupBallotFindMSB(subgroupBallot(true)) ? 0x4 : 0;\n"
				<< "  tempResult |= 0x8;\n"
				<< "  for (uint i = 0; i < gl_SubgroupSize; i++)\n"
				<< "  {\n"
				<< "    if (i != subgroupBallotFindMSB(MAKE_SINGLE_BIT_BALLOT_RESULT(i)))\n"
				<< "    {\n"
				<< "      tempResult &= ~0x8;\n"
				<< "    }\n"
				<< "  }\n";
			break;
	}

	bdy << "  tempRes = tempResult;\n";

	return bdy.str();
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getTestString(caseDef);
	const vector<string>		headDeclarations	= getFramebufferPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupported	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const SpirvVersion			spirvVersion		= isAllRayTracingStages(caseDef.shaderStage) ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, spirvVersion, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getTestString(caseDef);
	const vector<string>		headDeclarations	= getPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupported	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	if (caseDef.requiredSubgroupSize)
	{
		context.requireDeviceFunctionality("VK_EXT_subgroup_size_control");

		const VkPhysicalDeviceSubgroupSizeControlFeatures&		subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeatures();
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");

		if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
			TCU_THROW(NotSupportedError, "Required subgroup size is not supported for shader stage");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);

	if (isAllRayTracingStages(caseDef.shaderStage))
	{
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	}

	subgroups::supportedCheckShader(context, caseDef.shaderStage);
}

TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
	if (isAllComputeStages(caseDef.shaderStage))
	{
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
		TestLog&												log								= context.getTestContext().getLog();

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkCompute);

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0u, DE_NULL, checkCompute, size);
			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << TestLog::Message << "subgroupSize " << size << " failed" << TestLog::EndMessage;

				return result;
			}
		}

		return TestStatus::pass("OK");
	}
	else if (isAllGraphicsStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages	= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages, stages);
	}
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages	= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);

		return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages, stages);
	}
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");

	return TestStatus::pass("OK");
}
}

namespace vkt
{
namespace subgroups
{
TestCaseGroup* createSubgroupsBallotOtherTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "ballot_other", "Subgroup ballot other category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup ballot other category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup ballot other category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup ballot other category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup ballot other category tests: ray tracing"));
	const VkShaderStageFlags	stages[]			=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};
	const deBool				boolValues[]		=
	{
		DE_FALSE,
		DE_TRUE
	};

	for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
	{
		const OpType	opType	= static_cast<OpType>(opTypeIndex);
		const string	op		= de::toLower(getOpTypeName(opType));

		for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
		{
			const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
			const string			testName				= op + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
			const CaseDefinition	caseDef					=
			{
				opType,							//  OpType				opType;
				VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				requiredSubgroupSize			//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(computeGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
		}

		{
			const CaseDefinition	caseDef		=
			{
				opType,							//  OpType				opType;
				VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(graphicGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
		}

		{
			const CaseDefinition	caseDef		=
			{
				opType,							//  OpType				opType;
				SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(raytracingGroup.get(), op, "", supportedCheck, initPrograms, test, caseDef);
		}

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
		{
			const CaseDefinition	caseDef		=
			{
				opType,							//  OpType				opType;
				stages[stageIndex],				//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};
			const string			testName	= op + "_" + getShaderStageName(caseDef.shaderStage);

			addFunctionCaseWithPrograms(framebufferGroup.get(), testName, "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	group->addChild(raytracingGroup.release());

	return group.release();
}

} // subgroups
} // vkt
