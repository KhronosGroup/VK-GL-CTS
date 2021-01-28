/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2018 NVIDIA Corporation
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

#include "vktSubgroupsPartitionedTests.hpp"
#include "vktSubgroupsScanHelpers.hpp"
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
	OPTYPE_ADD = 0,
	OPTYPE_MUL,
	OPTYPE_MIN,
	OPTYPE_MAX,
	OPTYPE_AND,
	OPTYPE_OR,
	OPTYPE_XOR,
	OPTYPE_INCLUSIVE_ADD,
	OPTYPE_INCLUSIVE_MUL,
	OPTYPE_INCLUSIVE_MIN,
	OPTYPE_INCLUSIVE_MAX,
	OPTYPE_INCLUSIVE_AND,
	OPTYPE_INCLUSIVE_OR,
	OPTYPE_INCLUSIVE_XOR,
	OPTYPE_EXCLUSIVE_ADD,
	OPTYPE_EXCLUSIVE_MUL,
	OPTYPE_EXCLUSIVE_MIN,
	OPTYPE_EXCLUSIVE_MAX,
	OPTYPE_EXCLUSIVE_AND,
	OPTYPE_EXCLUSIVE_OR,
	OPTYPE_EXCLUSIVE_XOR,
	OPTYPE_LAST
};

struct CaseDefinition
{
	Operator			op;
	ScanType			scanType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

static Operator getOperator (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_ADD:
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_ADD:
			return OPERATOR_ADD;
		case OPTYPE_MUL:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MUL:
			return OPERATOR_MUL;
		case OPTYPE_MIN:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MIN:
			return OPERATOR_MIN;
		case OPTYPE_MAX:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_MAX:
			return OPERATOR_MAX;
		case OPTYPE_AND:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_AND:
			return OPERATOR_AND;
		case OPTYPE_OR:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_OR:
			return OPERATOR_OR;
		case OPTYPE_XOR:
		case OPTYPE_INCLUSIVE_XOR:
		case OPTYPE_EXCLUSIVE_XOR:
			return OPERATOR_XOR;
		default:
			DE_FATAL("Unsupported op type");
			return OPERATOR_ADD;
	}
}

static ScanType getScanType (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_ADD:
		case OPTYPE_MUL:
		case OPTYPE_MIN:
		case OPTYPE_MAX:
		case OPTYPE_AND:
		case OPTYPE_OR:
		case OPTYPE_XOR:
			return SCAN_REDUCE;
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_INCLUSIVE_XOR:
			return SCAN_INCLUSIVE;
		case OPTYPE_EXCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_XOR:
			return SCAN_EXCLUSIVE;
		default:
			DE_FATAL("Unsupported op type");
			return SCAN_REDUCE;
	}
}

static bool checkVertexPipelineStages (const void*			internalData,
									   vector<const void*>	datas,
									   deUint32				width,
									   deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 0xFFFFFF);
}

static bool checkCompute (const void*			internalData,
						  vector<const void*>	datas,
						  const deUint32		numWorkgroups[3],
						  const deUint32		localSize[3],
						  deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkCompute(datas, numWorkgroups, localSize, 0xFFFFFF);
}

string getOpTypeName (Operator op, ScanType scanType)
{
	return getScanOpName("subgroup", "", op, scanType);
}

string getOpTypeNamePartitioned (Operator op, ScanType scanType)
{
	return getScanOpName("subgroupPartitioned", "NV", op, scanType);
}

string getExtHeader (const CaseDefinition& caseDef)
{
	return	"#extension GL_NV_shader_subgroup_partitioned: enable\n"
			"#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			+ subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

string getTestString (const CaseDefinition& caseDef)
{
	Operator op = caseDef.op;
	ScanType st = caseDef.scanType;

	// NOTE: tempResult can't have anything in bits 31:24 to avoid int->float
	// conversion overflow in framebuffer tests.
	string fmt = subgroups::getFormatNameForGLSL(caseDef.format);
	string bdy =
		"  uvec4 mask = subgroupBallot(true);\n"
		"  uint tempResult = 0;\n"
		"  uint id = gl_SubgroupInvocationID;\n";

	// Test the case where the partition has a single subset with all invocations in it.
	// This should generate the same result as the non-partitioned function.
	bdy +=
		"  uvec4 allBallot = mask;\n"
		"  " + fmt + " allResult = " + getOpTypeNamePartitioned(op, st) + "(data[gl_SubgroupInvocationID], allBallot);\n"
		"  " + fmt + " refResult = " + getOpTypeName(op, st) + "(data[gl_SubgroupInvocationID]);\n"
		"  if (" + getCompare(op, caseDef.format, "allResult", "refResult") + ") {\n"
		"      tempResult |= 0x1;\n"
		"  }\n";

	// The definition of a partition doesn't forbid bits corresponding to inactive
	// invocations being in the subset with active invocations. In other words, test that
	// bits corresponding to inactive invocations are ignored.
	bdy +=
		"  if (0 == (gl_SubgroupInvocationID % 2)) {\n"
		"    " + fmt + " allResult = " + getOpTypeNamePartitioned(op, st) + "(data[gl_SubgroupInvocationID], allBallot);\n"
		"    " + fmt + " refResult = " + getOpTypeName(op, st) + "(data[gl_SubgroupInvocationID]);\n"
		"    if (" + getCompare(op, caseDef.format, "allResult", "refResult") + ") {\n"
		"        tempResult |= 0x2;\n"
		"    }\n"
		"  } else {\n"
		"    tempResult |= 0x2;\n"
		"  }\n";

	// Test the case where the partition has each invocation in a unique subset. For
	// exclusive ops, the result is identity. For reduce/inclusive, it's the original value.
	string expectedSelfResult = "data[gl_SubgroupInvocationID]";
	if (st == SCAN_EXCLUSIVE)
		expectedSelfResult = getIdentity(op, caseDef.format);

	bdy +=
		"  uvec4 selfBallot = subgroupPartitionNV(gl_SubgroupInvocationID);\n"
		"  " + fmt + " selfResult = " + getOpTypeNamePartitioned(op, st) + "(data[gl_SubgroupInvocationID], selfBallot);\n"
		"  if (" + getCompare(op, caseDef.format, "selfResult", expectedSelfResult) + ") {\n"
		"      tempResult |= 0x4;\n"
		"  }\n";

	// Test "random" partitions based on a hash of the invocation id.
	// This "hash" function produces interesting/randomish partitions.
	static const char *idhash = "((id%N)+(id%(N+1))-(id%2)+(id/2))%((N+1)/2)";

	bdy +=
		"  for (uint N = 1; N < 16; ++N) {\n"
		"    " + fmt + " idhashFmt = " + fmt + "(" + idhash + ");\n"
		"    uvec4 partitionBallot = subgroupPartitionNV(idhashFmt) & mask;\n"
		"    " + fmt + " partitionedResult = " + getOpTypeNamePartitioned(op, st) + "(data[gl_SubgroupInvocationID], partitionBallot);\n"
		"      for (uint i = 0; i < N; ++i) {\n"
		"        " + fmt + " iFmt = " + fmt + "(i);\n"
		"        if (" + getCompare(op, caseDef.format, "idhashFmt", "iFmt") + ") {\n"
		"          " + fmt + " subsetResult = " + getOpTypeName(op, st) + "(data[gl_SubgroupInvocationID]);\n"
		"          tempResult |= " + getCompare(op, caseDef.format, "partitionedResult", "subsetResult") + " ? (0x4 << N) : 0;\n"
		"        }\n"
		"      }\n"
		"  }\n"
		// tests in flow control:
		"  if (1 == (gl_SubgroupInvocationID % 2)) {\n"
		"    for (uint N = 1; N < 7; ++N) {\n"
		"      " + fmt + " idhashFmt = " + fmt + "(" + idhash + ");\n"
		"      uvec4 partitionBallot = subgroupPartitionNV(idhashFmt) & mask;\n"
		"      " + fmt + " partitionedResult = " + getOpTypeNamePartitioned(op, st) + "(data[gl_SubgroupInvocationID], partitionBallot);\n"
		"        for (uint i = 0; i < N; ++i) {\n"
		"          " + fmt + " iFmt = " + fmt + "(i);\n"
		"          if (" + getCompare(op, caseDef.format, "idhashFmt", "iFmt") + ") {\n"
		"            " + fmt + " subsetResult = " + getOpTypeName(op, st) + "(data[gl_SubgroupInvocationID]);\n"
		"            tempResult |= " + getCompare(op, caseDef.format, "partitionedResult", "subsetResult") + " ? (0x20000 << N) : 0;\n"
		"          }\n"
		"        }\n"
		"    }\n"
		"  } else {\n"
		"    tempResult |= 0xFC0000;\n"
		"  }\n"
		"  tempRes = tempResult;\n"
		;

	return bdy;
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getTestString(caseDef);
	const bool					pointSizeSupport	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, pointSizeSupport, extHeader, testSrc, "");
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const SpirvVersion			spirvVersion		= isAllRayTracingStages(caseDef.shaderStage) ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, spirvVersion, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getTestString(caseDef);
	const bool					pointSizeSupport	= false;

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, pointSizeSupport, extHeader, testSrc, "");
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV))
		TCU_THROW(NotSupportedError, "Device does not support subgroup partitioned operations");

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

	if (caseDef.requiredSubgroupSize)
	{
		context.requireDeviceFunctionality("VK_EXT_subgroup_size_control");

		const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT&	subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeaturesEXT();
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");

		if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
			TCU_THROW(NotSupportedError, "Required subgroup size is not supported for shader stage");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);

	subgroups::supportedCheckShader(context, caseDef.shaderStage);
}

TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	const subgroups::SSBOData	inputData
	{
		subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
		subgroups::SSBOData::LayoutStd140,		//  InputDataLayoutType			layout;
		caseDef.format,							//  vk::VkFormat				format;
		subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
	};

	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
	if (isAllComputeStages(caseDef.shaderStage))
	{
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
		TestLog&												log								= context.getTestContext().getLog();
		const subgroups::SSBOData								inputData						=
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
		};

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute);

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute,
																size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
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
		const VkShaderStageFlags	stages		= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
			false,									//  bool						isImage;
			4u,										//  deUint32					binding;
			stages,									//  vk::VkShaderStageFlags		stages;
		};

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, stages);
	}
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages		= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
			false,									//  bool						isImage;
			6u,										//  deUint32					binding;
			stages,									//  vk::VkShaderStageFlags		stages;
		};

		return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, stages);
	}
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");
}
}

namespace vkt
{
namespace subgroups
{
TestCaseGroup* createSubgroupsPartitionedTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "partitioned", "Subgroup partitioned category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup partitioned category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup partitioned category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup partitioned category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup partitioned category tests: ray tracing"));
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

	{
		const vector<VkFormat>		formats		= subgroups::getAllFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);
			const bool		isBool		= subgroups::isFormatBool(format);
			const bool		isFloat		= subgroups::isFormatFloat(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType	opType		= static_cast<OpType>(opTypeIndex);
				const Operator	op			= getOperator(opType);
				const ScanType	st			= getScanType(opType);
				const bool		isBitwiseOp	= (op == OPERATOR_AND || op == OPERATOR_OR || op == OPERATOR_XOR);

				// Skip float with bitwise category.
				if (isFloat && isBitwiseOp)
					continue;

				// Skip bool when its not the bitwise category.
				if (isBool && !isBitwiseOp)
					continue;

				const string	name = de::toLower(getOpTypeName(op, st)) + "_" + formatName;

				for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
				{
					const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
					const string			testName				= name + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
					const CaseDefinition	caseDef					=
					{
						op,								//  Operator			op;
						st,								//  ScanType			scanType;
						VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						requiredSubgroupSize			//  deBool				requiredSubgroupSize;
					};

					addFunctionCaseWithPrograms(computeGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
				}

				{
					const CaseDefinition	caseDef		=
					{
						op,								//  Operator			op;
						st,								//  ScanType			scanType;
						VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE						//  deBool				requiredSubgroupSize;
					};

					addFunctionCaseWithPrograms(graphicGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
				}

				for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
				{
					const CaseDefinition	caseDef		=
					{
						op,								//  Operator			op;
						st,								//  ScanType			scanType;
						stages[stageIndex],				//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE						//  deBool				requiredSubgroupSize;
					};
					const string			testName	= name + "_" + getShaderStageName(caseDef.shaderStage);

					addFunctionCaseWithPrograms(framebufferGroup.get(), testName, "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				}
			}
		}
	}

	{
		const vector<VkFormat>	formats		= subgroups::getAllRayTracingFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);
			const bool		isBool		= subgroups::isFormatBool(format);
			const bool		isFloat		= subgroups::isFormatFloat(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType	opType		= static_cast<OpType>(opTypeIndex);
				const Operator	op			= getOperator(opType);
				const ScanType	st			= getScanType(opType);
				const bool		isBitwiseOp	= (op == OPERATOR_AND || op == OPERATOR_OR || op == OPERATOR_XOR);

				// Skip float with bitwise category.
				if (isFloat && isBitwiseOp)
					continue;

				// Skip bool when its not the bitwise category.
				if (isBool && !isBitwiseOp)
					continue;

				{
					const CaseDefinition	caseDef		=
					{
						op,								//  Operator			op;
						st,								//  ScanType			scanType;
						SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE						//  deBool				requiredSubgroupSize;
					};
					const string			name		= de::toLower(getOpTypeName(op, st)) + "_" + formatName;

					addFunctionCaseWithPrograms(raytracingGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
				}
			}
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
