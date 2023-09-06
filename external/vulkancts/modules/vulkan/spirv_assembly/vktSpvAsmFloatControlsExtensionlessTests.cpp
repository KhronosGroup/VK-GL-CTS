/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief SPIR-V Float Control SPIR-V tokens test
 *//*--------------------------------------------------------------------*/

#include "vkApiVersion.hpp"

#include "vktSpvAsmFloatControlsExtensionlessTests.hpp"
#include "vktTestCase.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "tcuCommandLine.hpp"
#include "vkQueryUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

static const char* TEST_FEATURE_DENORM_PRESERVE					= "DenormPreserve";
static const char* TEST_FEATURE_DENORM_FLUSH_TO_ZERO			= "DenormFlushToZero";
static const char* TEST_FEATURE_SIGNED_ZERO_INF_NAN_PRESERVE	= "SignedZeroInfNanPreserve";
static const char* TEST_FEATURE_ROUNDING_MODE_RTE				= "RoundingModeRTE";
static const char* TEST_FEATURE_ROUNDING_MODE_RTZ				= "RoundingModeRTZ";

using namespace vk;
using std::map;
using std::string;
using std::vector;

static void getComputeSourceCode (std::string& computeSourceCode, const std::string& featureName, const int fpWideness)
{
	const std::string capability	= "OpCapability " + featureName + "\n";
	const std::string exeModes		= "OpExecutionMode %main " + featureName + " " + de::toString(fpWideness) + "\n";

	computeSourceCode =
		string(getComputeAsmShaderPreamble(capability, "", exeModes, "", "%indata %outdata")) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ getComputeAsmInputOutputBufferTraits("Block") + getComputeAsmCommonTypes("StorageBuffer") + getComputeAsmInputOutputBuffer("StorageBuffer") +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"             OpNop\n" // Inside a function body

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
}

static ComputeShaderSpec getComputeShaderSpec (Context& ctx, const std::string& testCaseName)
{
	const deUint32		baseSeed		= deStringHash(testCaseName.c_str()) + static_cast<deUint32>(ctx.getTestContext().getCommandLine().getBaseSeed());
	de::Random			rnd				(baseSeed);
	const int			numElements		= 64;
	vector<float>		inputFloats		(numElements, 0);
	vector<float>		outputFloats	(numElements, 0);
	ComputeShaderSpec	spec;

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		inputFloats[ndx] = rnd.getFloat(1.0f, 100.0f);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = -inputFloats[ndx];

	// Shader source code can be retrieved to complete definition of ComputeShaderSpec, though it is not required at this stage
	// getComputeSourceCode (spec.assembly);

	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

	spec.numWorkGroups	= tcu::IVec3(numElements, 1, 1);
	spec.verifyIO		= &verifyOutput;

	return spec;
}

VkBool32 getFloatControlsProperty(Context& context, const int fpWideness, const std::string& featureName)
{
	VkPhysicalDeviceFloatControlsProperties floatControlsProperties;
	deMemset(&floatControlsProperties, 0, sizeof(floatControlsProperties));
	floatControlsProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;

	VkPhysicalDeviceProperties2 properties;
	deMemset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &floatControlsProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	if (fpWideness == 16)
	{
		if (featureName == TEST_FEATURE_DENORM_PRESERVE				) return floatControlsProperties.shaderDenormPreserveFloat16;
		if (featureName == TEST_FEATURE_DENORM_FLUSH_TO_ZERO		) return floatControlsProperties.shaderDenormFlushToZeroFloat16;
		if (featureName == TEST_FEATURE_SIGNED_ZERO_INF_NAN_PRESERVE) return floatControlsProperties.shaderSignedZeroInfNanPreserveFloat16;
		if (featureName == TEST_FEATURE_ROUNDING_MODE_RTE			) return floatControlsProperties.shaderRoundingModeRTEFloat16;
		if (featureName == TEST_FEATURE_ROUNDING_MODE_RTZ			) return floatControlsProperties.shaderRoundingModeRTZFloat16;
	}

	if (fpWideness == 32)
	{
		if (featureName == TEST_FEATURE_DENORM_PRESERVE				) return floatControlsProperties.shaderDenormPreserveFloat32;
		if (featureName == TEST_FEATURE_DENORM_FLUSH_TO_ZERO		) return floatControlsProperties.shaderDenormFlushToZeroFloat32;
		if (featureName == TEST_FEATURE_SIGNED_ZERO_INF_NAN_PRESERVE) return floatControlsProperties.shaderSignedZeroInfNanPreserveFloat32;
		if (featureName == TEST_FEATURE_ROUNDING_MODE_RTE			) return floatControlsProperties.shaderRoundingModeRTEFloat32;
		if (featureName == TEST_FEATURE_ROUNDING_MODE_RTZ			) return floatControlsProperties.shaderRoundingModeRTZFloat32;
	}

	if (fpWideness == 64)
	{
		if (featureName == TEST_FEATURE_DENORM_PRESERVE				) return floatControlsProperties.shaderDenormPreserveFloat64;
		if (featureName == TEST_FEATURE_DENORM_FLUSH_TO_ZERO		) return floatControlsProperties.shaderDenormFlushToZeroFloat64;
		if (featureName == TEST_FEATURE_SIGNED_ZERO_INF_NAN_PRESERVE) return floatControlsProperties.shaderSignedZeroInfNanPreserveFloat64;
		if (featureName == TEST_FEATURE_ROUNDING_MODE_RTE			) return floatControlsProperties.shaderRoundingModeRTEFloat64;
		if (featureName == TEST_FEATURE_ROUNDING_MODE_RTZ			) return floatControlsProperties.shaderRoundingModeRTZFloat64;
	}

	TCU_THROW(InternalError, "Unknown property requested");
}

class SpvAsmFloatControlsExtensionlessInstance : public ComputeShaderSpec, public SpvAsmComputeShaderInstance
{
public:
	SpvAsmFloatControlsExtensionlessInstance	(Context& ctx, const std::string& testCaseName);
};

SpvAsmFloatControlsExtensionlessInstance::SpvAsmFloatControlsExtensionlessInstance (Context& ctx, const std::string& testCaseName)
	: ComputeShaderSpec(getComputeShaderSpec(ctx, testCaseName))
	, SpvAsmComputeShaderInstance(ctx, *this)
{
}

SpvAsmFloatControlsExtensionlessCase::SpvAsmFloatControlsExtensionlessCase (tcu::TestContext& testCtx, const char* name, const char* description, const char* featureName, const int fpWideness, const bool spirv14)
	: TestCase		(testCtx, name, description)
	, m_featureName	(featureName)
	, m_fpWideness	(fpWideness)
	, m_spirv14		(spirv14)
{
}

void SpvAsmFloatControlsExtensionlessCase::initPrograms (SourceCollections& programCollection) const
{
	const bool	allowSpirv14	= true;
	std::string	comp;

	getComputeSourceCode(comp, m_featureName, m_fpWideness);

	programCollection.spirvAsmSources.add("compute") << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_4, allowSpirv14) << comp;
}

void SpvAsmFloatControlsExtensionlessCase::checkSupport (Context& context) const
{
	if (m_spirv14)
	{
		context.requireDeviceFunctionality("VK_KHR_spirv_1_4");
	}
	else
	{
		if (!context.contextSupports(vk::ApiVersion(0, 1, 2, 0)))
			TCU_THROW(NotSupportedError, "Test requires Vulkan 1.2");
	}

	if (m_fpWideness == 16)
	{
		context.requireDeviceFunctionality("VK_KHR_shader_float16_int8");
		const VkPhysicalDeviceShaderFloat16Int8Features& extensionFeatures = context.getShaderFloat16Int8Features();
		if (!extensionFeatures.shaderFloat16)
			TCU_THROW(NotSupportedError, "Floating point number of width 16 bit are not supported");
	}

	if (m_fpWideness == 64)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_FLOAT64);
	}

	if (!getFloatControlsProperty(context, m_fpWideness, m_featureName))
		TCU_THROW(NotSupportedError, "Property is not supported");
}

TestInstance* SpvAsmFloatControlsExtensionlessCase::createInstance (Context& context) const
{
	return new SpvAsmFloatControlsExtensionlessInstance(context, getName());
}

tcu::TestCaseGroup* createFloatControlsExtensionlessGroup (tcu::TestContext& testCtx)
{
	const char*						spirVersions[]			= { "spirv1p4", "vulkan1_2" };
	const int						floatingPointWideness[]	= { 16, 32, 64 };
	const struct FpFeatures
	{
		const char* testName;
		const char* featureName;
	}
	fpFeatures[] =
	{
		{ "denorm_preserve",				TEST_FEATURE_DENORM_PRESERVE				},
		{ "denorm_flush_to_zero",			TEST_FEATURE_DENORM_FLUSH_TO_ZERO			},
		{ "signed_zero_inf_nan_preserve",	TEST_FEATURE_SIGNED_ZERO_INF_NAN_PRESERVE	},
		{ "rounding_mode_rte",				TEST_FEATURE_ROUNDING_MODE_RTE				},
		{ "rounding_mode_rtz",				TEST_FEATURE_ROUNDING_MODE_RTZ				},
	};
	de::MovePtr<tcu::TestCaseGroup>	group					(new tcu::TestCaseGroup(testCtx, "float_controls_extensionless", "Tests float controls without extension"));

	for (int spirVersionsNdx = 0; spirVersionsNdx < DE_LENGTH_OF_ARRAY(spirVersions); ++spirVersionsNdx)
	{
		const bool						spirv14				= (spirVersionsNdx == 0);
		de::MovePtr<tcu::TestCaseGroup>	spirVersionGroup	(new tcu::TestCaseGroup(testCtx, spirVersions[spirVersionsNdx], ""));

		for (int fpWidenessNdx = 0; fpWidenessNdx < DE_LENGTH_OF_ARRAY(floatingPointWideness); ++fpWidenessNdx)
		for (int execModeNdx = 0; execModeNdx < DE_LENGTH_OF_ARRAY(fpFeatures); ++execModeNdx)
		{
			const int			fpWideness		= floatingPointWideness[fpWidenessNdx];
			const std::string	testName		= fpFeatures[execModeNdx].testName;
			const char*			featureName		= fpFeatures[execModeNdx].featureName;
			const std::string	fullTestName	= "fp" + de::toString(fpWideness) + "_" + testName;

			spirVersionGroup->addChild(new SpvAsmFloatControlsExtensionlessCase(testCtx, fullTestName.c_str(), "", featureName, fpWideness, spirv14));
		}

		group->addChild(spirVersionGroup.release());
	}

	return group.release();
}


} // SpirVAssembly
} // vkt
