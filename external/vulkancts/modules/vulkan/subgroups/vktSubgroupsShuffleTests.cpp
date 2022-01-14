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

#include "vktSubgroupsShuffleTests.hpp"
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
	OPTYPE_SHUFFLE = 0,
	OPTYPE_SHUFFLE_XOR,
	OPTYPE_SHUFFLE_UP,
	OPTYPE_SHUFFLE_DOWN,
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
		case OPTYPE_SHUFFLE:		return "subgroupShuffle";
		case OPTYPE_SHUFFLE_XOR:	return "subgroupShuffleXor";
		case OPTYPE_SHUFFLE_UP:		return "subgroupShuffleUp";
		case OPTYPE_SHUFFLE_DOWN:	return "subgroupShuffleDown";
		default:					TCU_THROW(InternalError, "Unsupported op type");
	}
}

string getExtHeader (const CaseDefinition& caseDef)
{
	const string	eSource		= (OPTYPE_SHUFFLE == caseDef.opType || OPTYPE_SHUFFLE_XOR == caseDef.opType)
								? "#extension GL_KHR_shader_subgroup_shuffle: enable\n"
								: "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";

	return	eSource
			+ "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			+ subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

vector<string> getPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const string	formatName	= subgroups::getFormatNameForGLSL(caseDef.format);
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	const bool		fragment	= (caseDef.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
	const size_t	resultSize	= stageCount + (fragment ? 1 : 0);
	vector<string>	result		(resultSize, string());

	for (deUint32 i = 0; i < result.size(); ++i)
	{
		const deUint32	binding0	= i;
		const deUint32	binding1	= stageCount;
		const deUint32	binding2	= stageCount + 1;
		const string	buffer1		= (i == stageCount)
									? "layout(location = 0) out uint result;\n"
									: "layout(set = 0, binding = " + de::toString(binding0) + ", std430) buffer Buffer1\n"
									  "{\n"
									  "  uint result[];\n"
									  "};\n";

		result[i] =
			buffer1 +
			"layout(set = 0, binding = " + de::toString(binding1) + ", std430) readonly buffer Buffer2\n"
			"{\n"
			"  " + formatName + " data1[];\n"
			"};\n"
			"layout(set = 0, binding = " + de::toString(binding2) + ", std430) readonly buffer Buffer3\n"
			"{\n"
			"  uint data2[];\n"
			"};\n";
	}

	return result;
}

vector<string> getFramebufferPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const string	formatName	= subgroups::getFormatNameForGLSL(caseDef.format);
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	vector<string>	result		(stageCount, string());
	const string	buffer2
	{
		"layout(set = 0, binding = 0) uniform Buffer1\n"
		"{\n"
		"  " + formatName + " data1[" + de::toString(subgroups::maxSupportedSubgroupSize()) + "];\n"
		"};\n"
		"layout(set = 0, binding = 1) uniform Buffer2\n"
		"{\n"
		"  uint data2[" + de::toString(subgroups::maxSupportedSubgroupSize()) + "];\n"
		"};\n"
	};

	for (size_t i = 0; i < result.size(); ++i)
	{
		switch (i)
		{
			case 0: result[i] = "layout(location = 0) out float result;\n" + buffer2;		break;
			case 1: result[i] = "layout(location = 0) out float out_color;\n" + buffer2;	break;
			case 2: result[i] = "layout(location = 0) out float out_color[];\n" + buffer2;	break;
			case 3: result[i] = "layout(location = 0) out float out_color;\n" + buffer2;	break;
			default: TCU_THROW(InternalError, "Unknown stage");
		}
	}

	return result;
}

const string getTestSource (const CaseDefinition& caseDef)
{
	const string	id			= caseDef.opType == OPTYPE_SHUFFLE		? "id_in"
								: caseDef.opType == OPTYPE_SHUFFLE_XOR	? "gl_SubgroupInvocationID ^ id_in"
								: caseDef.opType == OPTYPE_SHUFFLE_UP	? "gl_SubgroupInvocationID - id_in"
								: caseDef.opType == OPTYPE_SHUFFLE_DOWN	? "gl_SubgroupInvocationID + id_in"
								: "";
	const string	testSource	=
		"  uint temp_res;\n"
		"  uvec4 mask = subgroupBallot(true);\n"
		"  uint id_in = data2[gl_SubgroupInvocationID] & (gl_SubgroupSize - 1);\n"
		"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " op = "
		+ getOpTypeName(caseDef.opType) + "(data1[gl_SubgroupInvocationID], id_in);\n"
		"  uint id = " + id + ";\n"
		"  if ((id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
		"  {\n"
		"    temp_res = (op == data1[id]) ? 1 : 0;\n"
		"  }\n"
		"  else\n"
		"  {\n"
		"    temp_res = 1; // Invocation we read from was inactive, so we can't verify results!\n"
		"  }\n"
		"  tempRes = temp_res;\n";

	return testSource;
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getTestSource(caseDef);
	const vector<string>		headDeclarations	= getFramebufferPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupported	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const SpirvVersion			spirvVersion		= isAllRayTracingStages(caseDef.shaderStage) ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3;
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, spirvVersion, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getTestSource(caseDef);
	const vector<string>		headDeclarations	= getPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupported	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	switch (caseDef.opType)
	{
		case OPTYPE_SHUFFLE:
		case OPTYPE_SHUFFLE_XOR:
			if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_SHUFFLE_BIT))
			{
				TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle operations");
			}
			break;
		default:
			if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT))
			{
				TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle relative operations");
			}
			break;
	}

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
	const subgroups::SSBOData	inputData[2]
	{
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd140,		//  InputDataLayoutType			layout;
			caseDef.format,							//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
		},
		{
			subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
			subgroups::SSBOData::LayoutStd140,		//  InputDataLayoutType			layout;
			VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
			subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
		}
	};

	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context,  VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages, caseDef.shaderStage);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
	if (isAllComputeStages(caseDef.shaderStage))
	{
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
		TestLog&												log								= context.getTestContext().getLog();
		const subgroups::SSBOData								inputData[2]
		{
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				caseDef.format,							//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
			},
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
			},
		};

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkCompute);

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkCompute,
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
		const VkShaderStageFlags	stages			= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData[2]
		{
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				caseDef.format,							//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
				false,									//  bool						isImage;
				4u,										//  deUint32					binding;
				stages,									//  vk::VkShaderStageFlags		stages;
			},
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
				false,									//  bool						isImage;
				5u,										//  deUint32					binding;
				stages,									//  vk::VkShaderStageFlags		stages;
			},
		};

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages, stages);
	}
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages			= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);
		const subgroups::SSBOData	inputData[2]
		{
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				caseDef.format,							//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
				false,									//  bool						isImage;
				6u,										//  deUint32					binding;
				stages,									//  vk::VkShaderStageFlags		stages;
			},
			{
				subgroups::SSBOData::InitializeNonZero,	//  InputDataInitializeType		initializeType;
				subgroups::SSBOData::LayoutStd430,		//  InputDataLayoutType			layout;
				VK_FORMAT_R32_UINT,						//  vk::VkFormat				format;
				subgroups::maxSupportedSubgroupSize(),	//  vk::VkDeviceSize			numElements;
				false,									//  bool						isImage;
				7u,										//  deUint32					binding;
				stages,									//  vk::VkShaderStageFlags		stages;
			},
		};

		return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, inputData, 2, DE_NULL, checkVertexPipelineStages, stages);
	}
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");
}
}

namespace vkt
{
namespace subgroups
{
TestCaseGroup* createSubgroupsShuffleTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "shuffle", "Subgroup shuffle category tests"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics", "Subgroup shuffle category tests: graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute", "Subgroup shuffle category tests: compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer", "Subgroup shuffle category tests: framebuffer"));
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing", "Subgroup shuffle category tests: ray tracing"));
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
				const string	name	= de::toLower(getOpTypeName(opType)) + "_" + formatName;

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

				for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
				{
					const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
					const string			testName				= name + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
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
				const OpType			opType	= static_cast<OpType>(opTypeIndex);
				const string			name	= de::toLower(getOpTypeName(opType)) + "_" + formatName;
				const CaseDefinition	caseDef	=
				{
					opType,							//  OpType				opType;
					SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
					format,							//  VkFormat			format;
					de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
					DE_FALSE						//  deBool				requiredSubgroupSize;
				};

				addFunctionCaseWithPrograms(raytracingGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
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
