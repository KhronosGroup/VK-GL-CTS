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

#include "vktSubgroupsQuadTests.hpp"
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
	OPTYPE_QUAD_BROADCAST = 0,
	OPTYPE_QUAD_BROADCAST_NONCONST,
	OPTYPE_QUAD_SWAP_HORIZONTAL,
	OPTYPE_QUAD_SWAP_VERTICAL,
	OPTYPE_QUAD_SWAP_DIAGONAL,
	OPTYPE_LAST
};

struct CaseDefinition
{
	OpType				opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

static bool checkVertexPipelineStages (const void*			internalData,
									   vector<const void*>	datas,
									   deUint32				width,
									   deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 1);
}

static bool checkCompute (const void*			internalData,
						  vector<const void*>	datas,
						  const deUint32		numWorkgroups[3],
						  const deUint32		localSize[3],
						  deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkCompute(datas, numWorkgroups, localSize, 1);
}

string getOpTypeName (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_QUAD_BROADCAST:				return "subgroupQuadBroadcast";
		case OPTYPE_QUAD_BROADCAST_NONCONST:	return "subgroupQuadBroadcast";
		case OPTYPE_QUAD_SWAP_HORIZONTAL:		return "subgroupQuadSwapHorizontal";
		case OPTYPE_QUAD_SWAP_VERTICAL:			return "subgroupQuadSwapVertical";
		case OPTYPE_QUAD_SWAP_DIAGONAL:			return "subgroupQuadSwapDiagonal";
		default:								TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getOpTypeCaseName (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_QUAD_BROADCAST:				return "subgroupquadbroadcast";
		case OPTYPE_QUAD_BROADCAST_NONCONST:	return "subgroupquadbroadcast_nonconst";
		case OPTYPE_QUAD_SWAP_HORIZONTAL:		return "subgroupquadswaphorizontal";
		case OPTYPE_QUAD_SWAP_VERTICAL:			return "subgroupquadswapvertical";
		case OPTYPE_QUAD_SWAP_DIAGONAL:			return "subgroupquadswapdiagonal";
		default:								TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getExtHeader (VkFormat format)
{
	return	"#extension GL_KHR_shader_subgroup_quad: enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n" +
			subgroups::getAdditionalExtensionForFormat(format);
}

string getTestSrc (const CaseDefinition &caseDef)
{
	const string	swapTable[OPTYPE_LAST]	=
	{
		"",
		"",
		"  const uint swapTable[4] = {1, 0, 3, 2};\n",
		"  const uint swapTable[4] = {2, 3, 0, 1};\n",
		"  const uint swapTable[4] = {3, 2, 1, 0};\n",
	};
	const string	validate				=
		"  if (subgroupBallotBitExtract(mask, otherID) && op !=data[otherID])\n"
		"    tempRes = 0;\n";
	const string	fmt						= subgroups::getFormatNameForGLSL(caseDef.format);
	const string	op						= getOpTypeName(caseDef.opType);
	ostringstream	testSrc;

	testSrc	<< "  uvec4 mask = subgroupBallot(true);\n"
			<< swapTable[caseDef.opType]
			<< "  tempRes = 1;\n";

	if (caseDef.opType == OPTYPE_QUAD_BROADCAST)
	{
		for (int i=0; i<4; i++)
		{
			testSrc << "  {\n"
					<< "  " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID], " << i << ");\n"
					<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + " << i << ";\n"
					<< validate
					<< "  }\n";
		}
	}
	else if (caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST)
	{
		testSrc << "  for (int i=0; i<4; i++)"
				<< "  {\n"
				<< "  " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID], i);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + i;\n"
				<< validate
				<< "  }\n"
				<< "  uint quadID = gl_SubgroupInvocationID >> 2;\n"
				<< "  uint quadInvocation = gl_SubgroupInvocationID & 0x3;\n"
				<< "  // Test lane ID that is only uniform in active lanes\n"
				<< "  if (quadInvocation >= 2)\n"
				<< "  {\n"
				<< "    uint id = quadInvocation & ~1;\n"
				<< "    " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID], id);\n"
				<< "    uint otherID = 4*quadID + id;\n"
				<< validate
				<< "  }\n"
				<< "  // Test lane ID that is only quad uniform, not subgroup uniform\n"
				<< "  {\n"
				<< "    uint id = quadID & 0x3;\n"
				<< "    " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID], id);\n"
				<< "    uint otherID = 4*quadID + id;\n"
				<< validate
				<< "  }\n";
	}
	else
	{
		testSrc << "  " << fmt << " op = " << op << "(data[gl_SubgroupInvocationID]);\n"
				<< "  uint otherID = (gl_SubgroupInvocationID & ~0x3) + swapTable[gl_SubgroupInvocationID & 0x3];\n"
				<< validate;
	}

	return testSrc.str();
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const SpirvVersion			spirvVersion	= (caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST) ? SPIRV_VERSION_1_5 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, getExtHeader(caseDef.format), getTestSrc(caseDef), "");
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const bool					spirv15required	= caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST;
	const bool					spirv14required	= isAllRayTracingStages(caseDef.shaderStage);
	const SpirvVersion			spirvVersion	= spirv15required ? SPIRV_VERSION_1_5
												: spirv14required ? SPIRV_VERSION_1_4
												: SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);
	const string				extHeader		= getExtHeader(caseDef.format);
	const string				testSrc			= getTestSrc(caseDef);

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, "");
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_QUAD_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup quad operations");

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

	if ((caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST) && !subgroups::isSubgroupBroadcastDynamicIdSupported(context))
		TCU_THROW(NotSupportedError, "Device does not support SubgroupBroadcastDynamicId");

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
	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

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
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
		TestLog&												log								= context.getTestContext().getLog();
		const subgroups::SSBOData								inputData
		{
			subgroups::SSBOData::InitializeNonZero,	// InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,		// InputDataLayoutType			layout;
			caseDef.format,							// vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	// vk::VkDeviceSize			numElements;
		};

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute);

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus	result	= subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute,
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
		subgroups::SSBOData			inputData;

		inputData.format			= caseDef.format;
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, stages);
	}
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages		= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData	=
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
TestCaseGroup* createSubgroupsQuadTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "quad", "Subgroup quad category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup arithmetic category tests: ray tracing"));
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
		const vector<VkFormat>	formats	= subgroups::getAllFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType	opType	= static_cast<OpType>(opTypeIndex);
				const string	name	= getOpTypeCaseName(opType) + "_" + formatName;

				for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
				{
					const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
					const string			testNameSuffix			= requiredSubgroupSize ? "_requiredsubgroupsize" : "";
					const string			testName				= name + testNameSuffix;
					const CaseDefinition	caseDef					=
					{
						opType,							//  OpType				opType;
						VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
					};

					addFunctionCaseWithPrograms(computeGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
				}

				{
					const CaseDefinition	caseDef		=
					{
						opType,							//  OpType				opType;
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
						opType,							//  OpType				opType;
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
		const vector<VkFormat>	formats	= subgroups::getAllRayTracingFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
			{
				const OpType			opType		= static_cast<OpType>(opTypeIndex);
				const string			testName	= getOpTypeCaseName(opType) + "_" + formatName;
				const CaseDefinition	caseDef		=
				{
					opType,							//  OpType				opType;
					SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
					format,							//  VkFormat			format;
					de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
					DE_FALSE						//  deBool				requiredSubgroupSize;
				};

				addFunctionCaseWithPrograms(raytracingGroup.get(), testName, "", supportedCheck, initPrograms, test, caseDef);
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
