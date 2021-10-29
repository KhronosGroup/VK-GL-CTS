/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2018 Advanced Micro Devices, Inc.
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
 * \brief Test cases for VK_KHR_shader_clock. Ensure that values are
          being read from the OpReadClockKHR OpCode.
 *//*--------------------------------------------------------------------*/

#include "vktShaderClockTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktShaderExecutor.hpp"

#include "vkQueryUtil.hpp"

#include "tcuStringTemplate.hpp"

#include "vktAtomicOperationTests.hpp"
#include "vktShaderExecutor.hpp"

#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuResultCollector.hpp"

#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"
#include "deArrayUtil.hpp"

#include <cassert>
#include <string>

namespace vkt
{
namespace shaderexecutor
{

namespace
{

enum
{
	NUM_ELEMENTS = 32
};

enum clockType
{
	SUBGROUP = 0,
	DEVICE
};

enum bitType
{
	BIT_32 = 0,
	BIT_64
};

struct testType
{
	clockType   testClockType;
	bitType     testBitType;
	const char* testName;
};

static inline void* getPtrOfVar(deUint64& var)
{
	return &var;
}

using namespace vk;

class ShaderClockTestInstance : public TestInstance
{
public:
	ShaderClockTestInstance(Context& context, const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
		: TestInstance(context)
		, m_executor(createExecutor(m_context, shaderType, shaderSpec))
	{
	}

	virtual tcu::TestStatus iterate(void)
	{
		const deUint64 initValue = 0xcdcdcdcd;

		std::vector<deUint64>	outputs		(NUM_ELEMENTS, initValue);
		std::vector<void*>		outputPtr	(NUM_ELEMENTS, nullptr);

		std::transform(std::begin(outputs), std::end(outputs), std::begin(outputPtr), getPtrOfVar);

		m_executor->execute(NUM_ELEMENTS, nullptr, outputPtr.data());

		if (validateOutput(outputs))
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Result comparison failed");
	}

private:
	bool validateOutput(std::vector<deUint64>& outputs)
	{
		// The shader will write a 1 in the output if the clock did not increase
		return (outputs.size() == deUint64(std::count(std::begin(outputs), std::end(outputs), 0)));
	}

	de::UniquePtr<ShaderExecutor>		m_executor;
};

class ShaderClockCase : public TestCase
{
public:
	ShaderClockCase(tcu::TestContext& testCtx, testType operation, glu::ShaderType shaderType)
		: TestCase(testCtx, operation.testName, operation.testName)
		, m_operation(operation)
		, m_shaderSpec()
		, m_shaderType(shaderType)
	{
		initShaderSpec();
	}

	TestInstance* createInstance (Context& ctx) const override
	{
		return new ShaderClockTestInstance(ctx, m_shaderSpec, m_shaderType);
	}

	void initPrograms (vk::SourceCollections& programCollection) const override
	{
		generateSources(m_shaderType, m_shaderSpec, programCollection);
	}

	void checkSupport (Context& context) const override
	{
		context.requireDeviceFunctionality("VK_KHR_shader_clock");

		if (m_operation.testBitType == BIT_64)
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT64);

		const auto&	shaderClockFeatures	= context.getShaderClockFeatures();
		const auto	realTimeTest		= (m_operation.testClockType == DEVICE);

		if (realTimeTest && !shaderClockFeatures.shaderDeviceClock)
			TCU_THROW(NotSupportedError, "Shader device clock is not supported");

		if (!realTimeTest && !shaderClockFeatures.shaderSubgroupClock)
			TCU_THROW(NotSupportedError, "Shader subgroup clock is not supported");
	}

private:
	void initShaderSpec()
	{
		std::stringstream extensions;
		std::stringstream source;

		if (m_operation.testBitType == BIT_64)
		{
			extensions	<< "#extension GL_ARB_gpu_shader_int64 : require        \n";

			source << "uint64_t time1 = " << m_operation.testName << "();   \n";
			source << "uint64_t time2 = " << m_operation.testName << "();   \n";
			source << "out0  = uvec2(0, 0);                                 \n";
			source << "if (time1 > time2) {                                 \n";
			source << "    out0.x = 1;                                      \n";
			source << "}                                                    \n";
		}
		else
		{
			source << "uvec2 time1 = " << m_operation.testName << "();                      \n";
			source << "uvec2 time2 = " << m_operation.testName << "();                      \n";
			source << "out0  = uvec2(0, 0);                                                 \n";
			source << "if (time1.y > time2.y || (time1.y == time2.y && time1.x > time2.x)){ \n";
			source << "    out0.x = 1;                                                      \n";
			source << "}                                                                    \n";
		}

		if (m_operation.testClockType == DEVICE)
		{
			extensions << "#extension GL_EXT_shader_realtime_clock : require	\n";
		}
		else
		{
			extensions << "#extension GL_ARB_shader_clock : enable				\n";
		}

		std::map<std::string, std::string> specializations = {
			{	"EXTENSIONS",	extensions.str()    },
			{	"SOURCE",		source.str()        }
		};

		m_shaderSpec.globalDeclarations = tcu::StringTemplate("${EXTENSIONS}").specialize(specializations);
		m_shaderSpec.source             = tcu::StringTemplate("${SOURCE}	").specialize(specializations);

		m_shaderSpec.outputs.push_back(Symbol("out0", glu::VarType(glu::TYPE_UINT_VEC2, glu::PRECISION_HIGHP)));
	}

private:
	ShaderClockCase				(const ShaderClockCase&);
	ShaderClockCase& operator=	(const ShaderClockCase&);

	testType							m_operation;
	ShaderSpec							m_shaderSpec;
	glu::ShaderType						m_shaderType;
};

void addShaderClockTests (tcu::TestCaseGroup* testGroup)
{
	static glu::ShaderType stages[] =
	{
		glu::SHADERTYPE_VERTEX,
		glu::SHADERTYPE_FRAGMENT,
		glu::SHADERTYPE_COMPUTE
	};

	static testType operations[] =
    {
		{SUBGROUP, BIT_64, "clockARB"},
		{SUBGROUP, BIT_32, "clock2x32ARB" },
		{DEVICE,   BIT_64, "clockRealtimeEXT"},
		{DEVICE,   BIT_32, "clockRealtime2x32EXT"}
	};

	tcu::TestContext& testCtx = testGroup->getTestContext();

	for (size_t i = 0; i != DE_LENGTH_OF_ARRAY(stages); ++i)
	{
		const char* stageName = (stages[i] == glu::SHADERTYPE_VERTEX) ? ("vertex")
								: (stages[i] == glu::SHADERTYPE_FRAGMENT) ? ("fragment")
								: (stages[i] == glu::SHADERTYPE_COMPUTE) ? ("compute")
								: (DE_NULL);

		const std::string setName = std::string() + stageName;
		de::MovePtr<tcu::TestCaseGroup> stageGroupTest(new tcu::TestCaseGroup(testCtx, setName.c_str(), "Shader Clock Tests"));

		for (size_t j = 0; j != DE_LENGTH_OF_ARRAY(operations); ++j)
		{
			stageGroupTest->addChild(new ShaderClockCase(testCtx, operations[j], stages[i]));
		}

		testGroup->addChild(stageGroupTest.release());
	}
}

} // anonymous

tcu::TestCaseGroup* createShaderClockTests(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "shader_clock", "Shader Clock Tests", addShaderClockTests);
}

} // shaderexecutor
} // vkt
