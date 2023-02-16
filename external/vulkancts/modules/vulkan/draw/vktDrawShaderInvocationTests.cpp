/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC
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
 * \brief Shader invocations tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawShaderInvocationTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "amber/vktAmberTestCase.hpp"

#include "tcuTestCase.hpp"

using namespace vk;

namespace vkt
{
namespace Draw
{
namespace
{

enum TestType
{
	EXT,
	CORE,
	CORE_MEM_MODEL,
};

void checkSupport(Context& context, TestType type)
{
	if ((context.getSubgroupProperties().supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT) == 0u)
		TCU_THROW(NotSupportedError, "Device does not support subgroup quad operations");

#ifndef CTS_USES_VULKANSC
	if (!context.getShaderDemoteToHelperInvocationFeatures().shaderDemoteToHelperInvocation)
		TCU_THROW(NotSupportedError, "demoteToHelperInvocation not supported.");
#else
	if (!context.getShaderDemoteToHelperInvocationFeaturesEXT().shaderDemoteToHelperInvocation)
		TCU_THROW(NotSupportedError, "demoteToHelperInvocation not supported.");
#endif

	// EXT test requires that the extension be supported, because OpIsHelperInvocationEXT was not promoted to core.
	if (type == EXT && !context.isDeviceFunctionalitySupported("VK_EXT_shader_demote_to_helper_invocation"))
		TCU_THROW(NotSupportedError, "VK_EXT_shader_demote_to_helper_invocation not supported.");

	// CORE and CORE_MEM_MODEL tests require SPIR-V 1.6, but this is checked automatically.

	if (type == CORE_MEM_MODEL && !context.getVulkanMemoryModelFeatures().vulkanMemoryModel)
		TCU_THROW(NotSupportedError, "Vulkan memory model not supported.");

}

void checkExtTestSupport		(Context& context, std::string testName) { DE_UNREF(testName); checkSupport(context, EXT); }
void checkCoreTestSupport		(Context& context, std::string testName) { DE_UNREF(testName); checkSupport(context, CORE); }
void checkMemModelTestSupport	(Context& context, std::string testName) { DE_UNREF(testName); checkSupport(context, CORE_MEM_MODEL); }

void createTests(tcu::TestCaseGroup* testGroup)
{
	tcu::TestContext&			testCtx		= testGroup->getTestContext();
	static const char			dataDir[]	= "draw/shader_invocation";

	struct caseDef {
		const char *name;
		const char *file;
		std::function<void(Context&, std::string)> supportFunc;
	} cases[] =
	{
		{ "helper_invocation",						"helper_invocation.amber",						checkExtTestSupport			},
		{ "helper_invocation_volatile",				"helper_invocation_volatile.amber",				checkCoreTestSupport		},
		{ "helper_invocation_volatile_mem_model",	"helper_invocation_volatile_mem_model.amber",	checkMemModelTestSupport	},
	};

	for (unsigned i=0; i<sizeof(cases) / sizeof(caseDef); i++)
	{
		cts_amber::AmberTestCase*	testCase = cts_amber::createAmberTestCase(testCtx, cases[i].name, "", dataDir, cases[i].file);
		testCase->setCheckSupportCallback(cases[i].supportFunc);
		testGroup->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createShaderInvocationTests(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "shader_invocation", "Shader Invocation tests", createTests);
}

}	// DrawTests
}	// vkt
