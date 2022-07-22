/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Google Inc.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Fragment output location tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawOutputLocationTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "amber/vktAmberTestCase.hpp"

#include "tcuTestCase.hpp"

#include <string>

namespace vkt
{
namespace Draw
{
namespace
{

void checkSupport (Context& context, std::string testName)
{
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset")
		&& context.getPortabilitySubsetProperties().minVertexInputBindingStrideAlignment == 4
		&& (testName.find("r8g8") != std::string::npos || testName.find("inputs-outputs-mod") != std::string::npos))
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Stride is not a multiple of minVertexInputBindingStrideAlignment");
	}
}

void createTests (tcu::TestCaseGroup* testGroup)
{
#ifndef CTS_USES_VULKANSC
	tcu::TestContext& testCtx = testGroup->getTestContext();

	// .array
	{
		tcu::TestCaseGroup* const	array		= new tcu::TestCaseGroup(testCtx, "array", "Test output location array");
		static const char			dataDir[]	= "draw/output_location/array";

		static const std::string	cases[]		=
		{
			"b10g11r11-ufloat-pack32-highp",
			"b10g11r11-ufloat-pack32-highp-output-float",
			"b10g11r11-ufloat-pack32-highp-output-vec2",
			"b10g11r11-ufloat-pack32-mediump",
			"b10g11r11-ufloat-pack32-mediump-output-float",
			"b10g11r11-ufloat-pack32-mediump-output-vec2",
			"b8g8r8a8-unorm-highp",
			"b8g8r8a8-unorm-highp-output-vec2",
			"b8g8r8a8-unorm-highp-output-vec3",
			"b8g8r8a8-unorm-mediump",
			"b8g8r8a8-unorm-mediump-output-vec2",
			"b8g8r8a8-unorm-mediump-output-vec3",
			"r16g16-sfloat-highp",
			"r16g16-sfloat-highp-output-float",
			"r16g16-sfloat-mediump",
			"r16g16-sfloat-mediump-output-float",
			"r32g32b32a32-sfloat-highp",
			"r32g32b32a32-sfloat-highp-output-vec2",
			"r32g32b32a32-sfloat-highp-output-vec3",
			"r32g32b32a32-sfloat-mediump",
			"r32g32b32a32-sfloat-mediump-output-vec2",
			"r32g32b32a32-sfloat-mediump-output-vec3",
			"r32-sfloat-highp",
			"r32-sfloat-mediump",
			"r8g8-uint-highp",
			"r8g8-uint-highp-output-uint",
			"r8g8-uint-mediump",
			"r8g8-uint-mediump-output-uint"
		};

		testGroup->addChild(array);

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
		{
			const std::string			fileName	= cases[i] + ".amber";
			cts_amber::AmberTestCase*	testCase	= cts_amber::createAmberTestCase(testCtx, cases[i].c_str(), "", dataDir, fileName);

			testCase->setCheckSupportCallback(checkSupport);
			array->addChild(testCase);
		}
	}

	// .shuffle
	{
		tcu::TestCaseGroup* const	shuffle		= new tcu::TestCaseGroup(testCtx, "shuffle", "Test output location shuffling");
		static const char			dataDir[]	= "draw/output_location/shuffle";

		static const std::string	cases[]		=
		{
			"inputs-outputs",
			"inputs-outputs-mod"
		};

		testGroup->addChild(shuffle);
		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
		{
			const std::string			fileName	= cases[i] + ".amber";
			cts_amber::AmberTestCase*	testCase	= cts_amber::createAmberTestCase(testCtx, cases[i].c_str(), "", dataDir, fileName);

			shuffle->addChild(testCase);
		}
	}
#else
	DE_UNREF(testGroup);
#endif
}

} // anonymous

tcu::TestCaseGroup* createOutputLocationTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "output_location", "Fragment output location tests", createTests);
}

}	// Draw
}	// vkt
