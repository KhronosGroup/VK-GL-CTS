/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \brief SPIR-V Versions check cases
 *//*--------------------------------------------------------------------*/

#include "vkApiVersion.hpp"

#include "vktSpvAsmSpirvVersionTests.hpp"
#include "vktTestCase.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
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

enum Operation
{
	OPERATION_COMPUTE = 0,
	OPERATION_GRAPHICS_VERTEX,
	OPERATION_GRAPHICS_TESSELATION_EVALUATION,
	OPERATION_GRAPHICS_TESSELATION_CONTROL,
	OPERATION_GRAPHICS_GEOMETRY,
	OPERATION_GRAPHICS_FRAGMENT,
	OPERATION_LAST
};

Operation& operator++ (Operation& operation)
{
	if (operation == OPERATION_LAST)
		operation = OPERATION_COMPUTE;
	else
		operation = static_cast<Operation>(static_cast<deUint32>(operation) + 1);

	return operation;
}

struct TestParameters
{
	Operation		operation;
	SpirvVersion	spirvVersion;
};

static InstanceContext initGraphicsInstanceContext (const TestParameters& testParameters)
{
	static const ShaderElement	vertFragPipelineStages[]		=
	{
		ShaderElement("vert", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};
	static const ShaderElement	tessPipelineStages[]			=
	{
		ShaderElement("vert",  "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("tessc", "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
		ShaderElement("tesse", "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
		ShaderElement("frag",  "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};
	static const ShaderElement	geomPipelineStages[]			=
	{
		ShaderElement("vert", "main", VK_SHADER_STAGE_VERTEX_BIT),
		ShaderElement("geom", "main", VK_SHADER_STAGE_GEOMETRY_BIT),
		ShaderElement("frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
	};
	map<string, string>			opSimpleTest;

	opSimpleTest["testfun"]	=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b = OpFAdd %f32 %a %a\n"
		"%c = OpFSub %f32 %b %a\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %c %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	if (testParameters.spirvVersion > SPIRV_VERSION_1_3)
		opSimpleTest["GL_entrypoint"] = "%BP_vertexIdInCurrentPatch";

	switch (testParameters.operation)
	{
		case OPERATION_GRAPHICS_VERTEX:					return createInstanceContext(vertFragPipelineStages, opSimpleTest);
		case OPERATION_GRAPHICS_TESSELATION_EVALUATION:	return createInstanceContext(tessPipelineStages, opSimpleTest);
		case OPERATION_GRAPHICS_TESSELATION_CONTROL:	return createInstanceContext(tessPipelineStages, opSimpleTest);
		case OPERATION_GRAPHICS_GEOMETRY:				return createInstanceContext(geomPipelineStages, opSimpleTest);
		case OPERATION_GRAPHICS_FRAGMENT:				return createInstanceContext(vertFragPipelineStages, opSimpleTest);
		default:										TCU_THROW(InternalError, "Invalid operation specified");
	}
}

static void getComputeSourceCode (std::string& computeSourceCode, SpirvVersion spirvVersion)
{
	computeSourceCode = "";
	if (spirvVersion > SPIRV_VERSION_1_3)
		computeSourceCode += string(getComputeAsmShaderPreamble("", "", "", "", "%indata %outdata"));
	else
		computeSourceCode += string(getComputeAsmShaderPreamble());

	computeSourceCode +=
		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n" +

		string(getComputeAsmInputOutputBufferTraits((spirvVersion > SPIRV_VERSION_1_3) ? "Block" : "BufferBlock")) +
		string(getComputeAsmCommonTypes((spirvVersion > SPIRV_VERSION_1_3) ? "StorageBuffer" : "Uniform")) +
		string(getComputeAsmInputOutputBuffer((spirvVersion > SPIRV_VERSION_1_3) ? "StorageBuffer" : "Uniform")) +

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

static ComputeShaderSpec getComputeShaderSpec (const TestParameters& testParameters)
{
	ComputeShaderSpec	spec;
	const deUint32		seed			= (static_cast<deUint32>(testParameters.operation)<<16) ^ static_cast<deUint32>(testParameters.spirvVersion);
	de::Random			rnd				(seed);
	const int			numElements		= 100;
	vector<float>		positiveFloats	(numElements, 0);
	vector<float>		negativeFloats	(numElements, 0);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		positiveFloats[ndx] = rnd.getFloat(1.0f, 100.0f);
		negativeFloats[ndx] = -positiveFloats[ndx];
	}

	// Shader source code can be retrieved to complete definition of ComputeShaderSpec, though it is not required at this stage
	// getComputeSourceCode (spec.assembly);

	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = tcu::IVec3(numElements, 1, 1);

	return spec;
}

static bool isSpirVersionsAsRequested (const BinaryCollection& binaryCollection, SpirvVersion requestedSpirvVersion)
{
	bool	result	= true;

	DE_ASSERT(!binaryCollection.empty());

	for (vk::BinaryCollection::Iterator binaryIt = binaryCollection.begin(); binaryIt != binaryCollection.end(); ++binaryIt)
	{
		SpirvVersion	binarySpirvVersion	= extractSpirvVersion	(binaryIt.getProgram());

		if (binarySpirvVersion != requestedSpirvVersion)
			result = false;
	}

	return result;
}

class SpvAsmGraphicsSpirvVersionsInstance : public TestInstance
{
public:
					SpvAsmGraphicsSpirvVersionsInstance	(Context& ctx, const TestParameters& testParameters);
	tcu::TestStatus	iterate								(void);

private:
	TestParameters	m_testParameters;
};

SpvAsmGraphicsSpirvVersionsInstance::SpvAsmGraphicsSpirvVersionsInstance (Context& ctx, const TestParameters& testParameters)
	: TestInstance		(ctx)
	, m_testParameters	(testParameters)
{
}

tcu::TestStatus SpvAsmGraphicsSpirvVersionsInstance::iterate (void)
{
	InstanceContext instanceContext = initGraphicsInstanceContext(m_testParameters);

	if (!isSpirVersionsAsRequested(m_context.getBinaryCollection(), m_testParameters.spirvVersion))
		return tcu::TestStatus::fail("Binary SPIR-V version is different from requested");

	return runAndVerifyDefaultPipeline(m_context, instanceContext);
}


class SpvAsmComputeSpirvVersionsInstance : public ComputeShaderSpec, public SpvAsmComputeShaderInstance
{
public:
					SpvAsmComputeSpirvVersionsInstance	(Context& ctx, const TestParameters& testParameters);
	tcu::TestStatus	iterate								(void);

private:
	TestParameters	m_testParameters;
};

SpvAsmComputeSpirvVersionsInstance::SpvAsmComputeSpirvVersionsInstance (Context& ctx, const TestParameters& testParameters)
	: ComputeShaderSpec(getComputeShaderSpec(testParameters))
	, SpvAsmComputeShaderInstance(ctx, *this)
	, m_testParameters(testParameters)
{
	if (m_testParameters.operation != OPERATION_COMPUTE)
		TCU_THROW(InternalError, "Invalid operation specified");
}

tcu::TestStatus SpvAsmComputeSpirvVersionsInstance::iterate (void)
{
	if (!isSpirVersionsAsRequested(m_context.getBinaryCollection(), m_testParameters.spirvVersion))
		return tcu::TestStatus::fail("Binary SPIR-V version is different from requested");

	return SpvAsmComputeShaderInstance::iterate();
}


class SpvAsmSpirvVersionsCase : public TestCase
{
public:
							SpvAsmSpirvVersionsCase	(tcu::TestContext& testCtx, const char* name, const char* description, const TestParameters& testParameters);
	void					initPrograms			(vk::SourceCollections& programCollection) const;
	TestInstance*			createInstance			(Context& context) const;

private:
	const TestParameters	m_testParameters;
};

SpvAsmSpirvVersionsCase::SpvAsmSpirvVersionsCase (tcu::TestContext& testCtx, const char* name, const char* description, const TestParameters& testParameters)
	: TestCase			(testCtx, name, description)
	, m_testParameters	(testParameters)
{
}

void validateVulkanVersion (const deUint32 usedVulkanVersion, const SpirvVersion testedSpirvVersion)
{
	const SpirvVersion	usedSpirvVersionForAsm	= getMaxSpirvVersionForAsm(usedVulkanVersion);

	if (testedSpirvVersion > usedSpirvVersionForAsm)
		TCU_THROW(NotSupportedError, "Specified SPIR-V version is not supported by the device/instance");
}

void SpvAsmSpirvVersionsCase::initPrograms (SourceCollections& programCollection) const
{
	const SpirVAsmBuildOptions	spirVAsmBuildOptions	(programCollection.usedVulkanVersion, m_testParameters.spirvVersion);

	validateVulkanVersion(programCollection.usedVulkanVersion, m_testParameters.spirvVersion);

	switch (m_testParameters.operation)
	{
		case OPERATION_COMPUTE:
		{
			std::string comp;

			getComputeSourceCode(comp, m_testParameters.spirvVersion);

			programCollection.spirvAsmSources.add("compute", &spirVAsmBuildOptions) << comp;

			break;
		}

		case OPERATION_GRAPHICS_VERTEX:
		{
			InstanceContext instanceContext = initGraphicsInstanceContext(m_testParameters);

			addShaderCodeCustomVertex(programCollection, instanceContext, &spirVAsmBuildOptions);

			break;
		}

		case OPERATION_GRAPHICS_TESSELATION_EVALUATION:
		{
			InstanceContext instanceContext = initGraphicsInstanceContext(m_testParameters);

			addShaderCodeCustomTessEval(programCollection, instanceContext, &spirVAsmBuildOptions);

			break;
		}

		case OPERATION_GRAPHICS_TESSELATION_CONTROL:
		{
			InstanceContext instanceContext = initGraphicsInstanceContext(m_testParameters);

			addShaderCodeCustomTessControl(programCollection, instanceContext, &spirVAsmBuildOptions);

			break;
		}

		case OPERATION_GRAPHICS_GEOMETRY:
		{
			InstanceContext instanceContext = initGraphicsInstanceContext(m_testParameters);

			addShaderCodeCustomGeometry(programCollection, instanceContext, &spirVAsmBuildOptions);

			break;
		}

		case OPERATION_GRAPHICS_FRAGMENT:
		{
			InstanceContext instanceContext = initGraphicsInstanceContext(m_testParameters);

			addShaderCodeCustomFragment(programCollection, instanceContext, &spirVAsmBuildOptions);

			break;
		}

		default:
			TCU_THROW(InternalError, "Invalid operation specified");
	}
}

TestInstance* SpvAsmSpirvVersionsCase::createInstance (Context& context) const
{
	validateVulkanVersion(context.getUsedApiVersion(), m_testParameters.spirvVersion);

	switch (m_testParameters.operation)
	{
		case OPERATION_COMPUTE:
			return new SpvAsmComputeSpirvVersionsInstance(context, m_testParameters);

		case OPERATION_GRAPHICS_VERTEX:
		case OPERATION_GRAPHICS_TESSELATION_EVALUATION:
		case OPERATION_GRAPHICS_TESSELATION_CONTROL:
		case OPERATION_GRAPHICS_GEOMETRY:
		case OPERATION_GRAPHICS_FRAGMENT:
			return new SpvAsmGraphicsSpirvVersionsInstance(context, m_testParameters);

		default:
			TCU_THROW(InternalError, "Invalid operation specified");
	}
}

tcu::TestCaseGroup* createSpivVersionCheckTests (tcu::TestContext& testCtx, const bool compute)
{
	const char*	operationNames[OPERATION_LAST]	=
	{
		"compute",
		"vertex",
		"tesselation_evaluation",
		"tesselation_control",
		"geometry",
		"fragment",
	};

	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "spirv_version", "Test SPIR-V version is supported"));

	for (SpirvVersion spirvVersion = SPIRV_VERSION_1_0; spirvVersion < SPIRV_VERSION_LAST; ++spirvVersion)
	{
		std::string spirvVersionName = getSpirvVersionName(spirvVersion);

		std::replace(spirvVersionName.begin(), spirvVersionName.end(), '.', '_');

		for (Operation operation = OPERATION_COMPUTE; operation < OPERATION_LAST; ++operation)
		{
			if ((compute && operation == OPERATION_COMPUTE) || (!compute && operation != OPERATION_COMPUTE))
			{
				const std::string		testName		= spirvVersionName + "_" + operationNames[static_cast<deUint32>(operation)];
				const TestParameters	testParameters	=
				{
					operation,
					spirvVersion
				};

				group->addChild(new SpvAsmSpirvVersionsCase(testCtx, testName.c_str(), "", testParameters));
			}
		}
	}

	return group.release();
}

} // SpirVAssembly
} // vkt
