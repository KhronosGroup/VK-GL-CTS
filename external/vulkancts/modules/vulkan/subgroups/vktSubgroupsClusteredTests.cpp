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

#include "vktSubgroupsClusteredTests.hpp"
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
	OPTYPE_CLUSTERED_ADD = 0,
	OPTYPE_CLUSTERED_MUL,
	OPTYPE_CLUSTERED_MIN,
	OPTYPE_CLUSTERED_MAX,
	OPTYPE_CLUSTERED_AND,
	OPTYPE_CLUSTERED_OR,
	OPTYPE_CLUSTERED_XOR,
	OPTYPE_CLUSTERED_LAST
};

struct CaseDefinition
{
	Operator			op;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

static Operator getOperator (OpType opType)
{
	switch (opType)
	{
		case OPTYPE_CLUSTERED_ADD:	return OPERATOR_ADD;
		case OPTYPE_CLUSTERED_MUL:	return OPERATOR_MUL;
		case OPTYPE_CLUSTERED_MIN:	return OPERATOR_MIN;
		case OPTYPE_CLUSTERED_MAX:	return OPERATOR_MAX;
		case OPTYPE_CLUSTERED_AND:	return OPERATOR_AND;
		case OPTYPE_CLUSTERED_OR:	return OPERATOR_OR;
		case OPTYPE_CLUSTERED_XOR:	return OPERATOR_XOR;
		default:					TCU_THROW(InternalError, "Unsupported op type");
	}
}

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

string getOpTypeName (Operator op)
{
	return getScanOpName("subgroupClustered", "", op, SCAN_REDUCE);
}

string getExtHeader (CaseDefinition& caseDef)
{
	return	"#extension GL_KHR_shader_subgroup_clustered: enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n" +
			subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

string getTestSrc (CaseDefinition& caseDef)
{
	const string	formatName	= subgroups::getFormatNameForGLSL(caseDef.format);
	const string	opTypeName	= getOpTypeName(caseDef.op);
	const string	identity	= getIdentity(caseDef.op, caseDef.format);
	const string	opOperation	= getOpOperation(caseDef.op, caseDef.format, "ref", "data[index]");
	const string	compare		= getCompare(caseDef.op, caseDef.format, "ref", "op");
	ostringstream	bdy;

	bdy << "  bool tempResult = true;\n"
		<< "  uvec4 mask = subgroupBallot(true);\n";

	for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
	{
		bdy	<< "  {\n"
			<< "    const uint clusterSize = " << i << ";\n"
			<< "    if (clusterSize <= gl_SubgroupSize)\n"
			<< "    {\n"
			<< "      " << formatName << " op = "
			<< opTypeName + "(data[gl_SubgroupInvocationID], clusterSize);\n"
			<< "      for (uint clusterOffset = 0; clusterOffset < gl_SubgroupSize; clusterOffset += clusterSize)\n"
			<< "      {\n"
			<< "        " << formatName << " ref = "
			<< identity << ";\n"
			<< "        for (uint index = clusterOffset; index < (clusterOffset + clusterSize); index++)\n"
			<< "        {\n"
			<< "          if (subgroupBallotBitExtract(mask, index))\n"
			<< "          {\n"
			<< "            ref = " << opOperation << ";\n"
			<< "          }\n"
			<< "        }\n"
			<< "        if ((clusterOffset <= gl_SubgroupInvocationID) && (gl_SubgroupInvocationID < (clusterOffset + clusterSize)))\n"
			<< "        {\n"
			<< "          if (!" << compare << ")\n"
			<< "          {\n"
			<< "            tempResult = false;\n"
			<< "          }\n"
			<< "        }\n"
			<< "      }\n"
			<< "    }\n"
			<< "  }\n"
			<< "  tempRes = tempResult ? 1 : 0;\n";
	}

	return bdy.str();
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const string				extHeader		= getExtHeader(caseDef);
	const string				testSrc			= getTestSrc(caseDef);

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, "");
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const bool					spirv14required	= isAllRayTracingStages(caseDef.shaderStage);
	const SpirvVersion			spirvVersion	= spirv14required ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);
	const string				extHeader		= getExtHeader(caseDef);
	const string				testSrc			= getTestSrc(caseDef);

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, "");
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_CLUSTERED_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup clustered operations");

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

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
	const subgroups::SSBOData	inputData	=
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
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
		TestLog&												log								= context.getTestContext().getLog();

		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.layout = subgroups::SSBOData::LayoutStd430;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute);

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute, size);
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
		const subgroups::SSBOData	inputData	=
		{
			subgroups::SSBOData::InitializeNonZero,		//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd430,			//  InputDataLayoutType			layout;
			caseDef.format,								//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),		//  vk::VkDeviceSize			numElements;
			false,										//  bool						isImage;
			4u,											//  deUint32					binding;
			stages,										//  vk::VkShaderStageFlags		stages;
		};

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
TestCaseGroup* createSubgroupsClusteredTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "clustered", "Subgroup clustered category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup clustered category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup clustered category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup clustered category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup clustered category tests: ray tracing"));
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
		const vector<VkFormat> formats = subgroups::getAllFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);
			const bool		isBool		= subgroups::isFormatBool(format);
			const bool		isFloat		= subgroups::isFormatFloat(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_CLUSTERED_LAST; ++opTypeIndex)
			{
				const OpType	opType		= static_cast<OpType>(opTypeIndex);
				const Operator	op			= getOperator(opType);
				const bool		isBitwiseOp	= (op == OPERATOR_AND || op == OPERATOR_OR || op == OPERATOR_XOR);

				// Skip float with bitwise category.
				if (isFloat && isBitwiseOp)
					continue;

				// Skip bool when its not the bitwise category.
				if (isBool && !isBitwiseOp)
					continue;

				const string	name	= de::toLower(getOpTypeName(op)) +"_" + formatName;

				for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
				{
					const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
					const string			testName				= name + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
					const CaseDefinition	caseDef					=
					{
						op,								//  Operator			op;
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
						op,								//  Operator			op;
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
						stages[stageIndex],				//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE						//  deBool				requiredSubgroupSize;
					};
					const string			testName	= name +"_" + getShaderStageName(caseDef.shaderStage);

					addFunctionCaseWithPrograms(framebufferGroup.get(), testName, "", supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				}
			}
		}
	}

	{
		const vector<VkFormat>		formats		= subgroups::getAllRayTracingFormats();

		for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
		{
			const VkFormat	format		= formats[formatIndex];
			const string	formatName	= subgroups::getFormatNameForGLSL(format);
			const bool		isBool		= subgroups::isFormatBool(format);
			const bool		isFloat		= subgroups::isFormatFloat(format);

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_CLUSTERED_LAST; ++opTypeIndex)
			{
				const OpType	opType		= static_cast<OpType>(opTypeIndex);
				const Operator	op			= getOperator(opType);
				const bool		isBitwiseOp	= (op == OPERATOR_AND || op == OPERATOR_OR || op == OPERATOR_XOR);

				// Skip float with bitwise category.
				if (isFloat && isBitwiseOp)
					continue;

				// Skip bool when its not the bitwise category.
				if (isBool && !isBitwiseOp)
					continue;

				{
					const string			name		= de::toLower(getOpTypeName(op)) +"_" + formatName;
					const CaseDefinition	caseDef		=
					{
						op,								//  Operator			op;
						SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
						format,							//  VkFormat			format;
						de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
						DE_FALSE						//  deBool				requiredSubgroupSize;
					};

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
