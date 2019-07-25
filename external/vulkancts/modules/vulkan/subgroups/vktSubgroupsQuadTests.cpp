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

static bool checkVertexPipelineStages(const void* internalData, std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::check(datas, width, 1);
}

static bool checkCompute(const void* internalData, std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 1);
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_QUAD_BROADCAST:
		case OPTYPE_QUAD_BROADCAST_NONCONST:
			return "subgroupQuadBroadcast";
		case OPTYPE_QUAD_SWAP_HORIZONTAL:
			return "subgroupQuadSwapHorizontal";
		case OPTYPE_QUAD_SWAP_VERTICAL:
			return "subgroupQuadSwapVertical";
		case OPTYPE_QUAD_SWAP_DIAGONAL:
			return "subgroupQuadSwapDiagonal";
	}
}

std::string getOpTypeCaseName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_QUAD_BROADCAST:
			return "subgroupquadbroadcast";
		case OPTYPE_QUAD_BROADCAST_NONCONST:
			return "subgroupquadbroadcast_nonconst";
		case OPTYPE_QUAD_SWAP_HORIZONTAL:
			return "subgroupquadswaphorizontal";
		case OPTYPE_QUAD_SWAP_VERTICAL:
			return "subgroupquadswapvertical";
		case OPTYPE_QUAD_SWAP_DIAGONAL:
			return "subgroupquadswapdiagonal";
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

std::string getExtHeader(VkFormat format)
{
	return	"#extension GL_KHR_shader_subgroup_quad: enable\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n" +
			subgroups::getAdditionalExtensionForFormat(format);
}

std::string getTestSrc(const CaseDefinition &caseDef)
{
	const std::string swapTable[OPTYPE_LAST] = {
		"",
		"",
		"  const uint swapTable[4] = {1, 0, 3, 2};\n",
		"  const uint swapTable[4] = {2, 3, 0, 1};\n",
		"  const uint swapTable[4] = {3, 2, 1, 0};\n",
	};

	const std::string validate =
		"  if (subgroupBallotBitExtract(mask, otherID) && op !=data[otherID])\n"
		"    tempRes = 0;\n";

	std::string fmt	= subgroups::getFormatNameForGLSL(caseDef.format);
	std::string op	= getOpTypeName(caseDef.opType);

	std::ostringstream testSrc;
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

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::SpirvVersion			spirvVersion = (caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST) ? vk::SPIRV_VERSION_1_5 : vk::SPIRV_VERSION_1_3;
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);

   subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, getExtHeader(caseDef.format), getTestSrc(caseDef), "");
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::SpirvVersion			spirvVersion = (caseDef.opType == OPTYPE_QUAD_BROADCAST_NONCONST) ? vk::SPIRV_VERSION_1_5 : vk::SPIRV_VERSION_1_3;
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);

	std::string extHeader = getExtHeader(caseDef.format);
	std::string testSrc = getTestSrc(caseDef);

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
		if (!context.requireDeviceFunctionality("VK_EXT_subgroup_size_control"))
			TCU_THROW(NotSupportedError, "Device does not support VK_EXT_subgroup_size_control extension");
		VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
		subgroupSizeControlFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
		subgroupSizeControlFeatures.pNext = DE_NULL;

		VkPhysicalDeviceFeatures2 features;
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &subgroupSizeControlFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);
}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " +
					   subgroups::getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}

	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					"Shader stage " +
					subgroups::getShaderStageName(caseDef.shaderStage) +
					" is required to support subgroup operations!");
		}
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.layout = subgroups::SSBOData::LayoutStd430;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute);

		tcu::TestLog& log = context.getTestContext().getLog();
		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;
		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupSizeControlProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		log << tcu::TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << tcu::TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute,
																size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << tcu::TestLog::Message << "subgroupSize " << size << " failed" << tcu::TestLog::EndMessage;
				return result;
			}
		}

		return tcu::TestStatus::pass("OK");
	}
	else
	{
		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		VkShaderStageFlagBits stages = (VkShaderStageFlagBits)(caseDef.shaderStage  & subgroupProperties.supportedStages);

		if (VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((VkShaderStageFlagBits)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		subgroups::SSBOData inputData;
		inputData.format			= caseDef.format;
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, stages);
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsQuadTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup arithmetic category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup arithmetic category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup arithmetic category tests: framebuffer"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	const std::vector<VkFormat> formats = subgroups::getAllFormats();

	for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
	{
		const VkFormat format = formats[formatIndex];

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{
			std::ostringstream name;
			name << getOpTypeCaseName(opTypeIndex);

			name << "_" << subgroups::getFormatNameForGLSL(format);

			{
				CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, de::SharedPtr<bool>(new bool), DE_FALSE};
				addFunctionCaseWithPrograms(computeGroup.get(), name.str(), "", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef =
				{
					opTypeIndex,
					VK_SHADER_STAGE_ALL_GRAPHICS,
					format,
					de::SharedPtr<bool>(new bool),
					DE_FALSE
				};
				addFunctionCaseWithPrograms(graphicGroup.get(), name.str(), "", supportedCheck, initPrograms, test, caseDef);
			}
			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, de::SharedPtr<bool>(new bool), DE_FALSE};
				addFunctionCaseWithPrograms(framebufferGroup.get(), name.str()+"_"+ getShaderStageName(caseDef.shaderStage), "",
											supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "quad", "Subgroup quad category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}
} // subgroups
} // vkt
