/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2021 The Khronos Group Inc.
* Copyright (c) 2016 The Android Open Source Project
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
* \brief VK_KHR_format_feature_flags2 Tests.
*//*--------------------------------------------------------------------*/

#include "vktApiFormatPropertiesExtendedKHRtests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkStrUtil.hpp"

#include <iostream>
#include <iomanip>

namespace
{
using namespace vk;
using namespace vkt;

void checkSupport (Context& context, const VkFormat format)
{
	DE_UNREF(format);
	context.requireDeviceFunctionality(VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME);
	context.requireInstanceFunctionality(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
}

void checkFlags (VkFlags64 reportedFlags, VkFlags64 requestedFlags, const char* setName)
{
	const auto andMask = (reportedFlags & requestedFlags);
	if (andMask != requestedFlags)
	{
		// Find which bits are missing.
		const auto missingBits = (andMask ^ requestedFlags);
		std::ostringstream msg;
		msg << setName << ": missing flags 0x" << std::hex << std::setw(16) << std::setfill('0') << missingBits;
		TCU_FAIL(msg.str());
	}
}

tcu::TestStatus test (Context& context, const VkFormat format)
{
	const VkFormatProperties3 formatProperties (context.getFormatProperties(format));
	const VkFormatProperties3 requiredProperties (context.getRequiredFormatProperties(format));

	checkFlags(formatProperties.bufferFeatures, requiredProperties.bufferFeatures, "Buffer features");
	checkFlags(formatProperties.linearTilingFeatures, requiredProperties.linearTilingFeatures, "Linear tiling features");
	checkFlags(formatProperties.optimalTilingFeatures, requiredProperties.optimalTilingFeatures, "Optimal tiling features");

	return tcu::TestStatus::pass("Pass");
}

void createTestCases (tcu::TestCaseGroup* group)
{
	for (VkFormat format = VK_FORMAT_R4G4_UNORM_PACK8; format < VK_CORE_FORMAT_LAST; format = static_cast<VkFormat>(format+1))
	{
		std::string testName = de::toLower(std::string(getFormatName(format)).substr(10));
		addFunctionCase(group, testName, std::string(), checkSupport, test, format);
	}
}

} // namespace

namespace vkt
{
namespace api
{

tcu::TestCaseGroup*	createFormatPropertiesExtendedKHRTests	(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "format_feature_flags2", "VK_KHR_format_feature_flags2 tests", createTestCases);
}

} // api
} // vkt
