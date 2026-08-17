/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Test creating tensors and sanity check tensor memory requirement
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTensorTestsUtil.hpp"

#include "vkObjUtil.hpp"

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

struct TestParams
{
    VkFormat format;
    VkFormatFeatureFlags2 linearTilingFeatures;
    VkFormatFeatureFlags2 optimalTilingFeatures;
};

void checkRequiredFormatFeatures(Context &ctx, const TestParams)
{
    ctx.requireDeviceFunctionality("VK_ARM_tensors");
}

tcu::TestStatus testRequiredFormatFeatures(Context &ctx, const TestParams params)
{
    const VkTensorFormatPropertiesARM formatProperties = getTensorFormatProperties(ctx, params.format);

    const bool hasRequiredLinearTilingFeatures =
        (formatProperties.linearTilingTensorFeatures & params.linearTilingFeatures) == params.linearTilingFeatures;
    const bool hasRequiredOptimalTilingFeatures =
        (formatProperties.optimalTilingTensorFeatures & params.optimalTilingFeatures) == params.optimalTilingFeatures;

    if (!hasRequiredLinearTilingFeatures || !hasRequiredOptimalTilingFeatures)
    {
        return tcu::TestStatus::fail("Supported features do not contain all required features");
    }

    return tcu::TestStatus::pass("Success");
}

std::string generateTestNameRequiredFormatFeatures(const TestParams &params)
{
    return de::toString(params.format);
}

} // namespace

tcu::TestCaseGroup *createTensorRequiredFormatsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> requiredFormatsTests(new tcu::TestCaseGroup(testCtx, "required_formats"));

    constexpr VkFormatFeatureFlags2 noFeatures = 0;

    const TestParams testCases[] = {
        {VK_FORMAT_UNDEFINED, noFeatures, noFeatures},   {VK_FORMAT_R8_BOOL_ARM, noFeatures, noFeatures},
        {VK_FORMAT_R8_UNORM, noFeatures, noFeatures},    {VK_FORMAT_R8_SNORM, noFeatures, noFeatures},
        {VK_FORMAT_R8_USCALED, noFeatures, noFeatures},  {VK_FORMAT_R8_SSCALED, noFeatures, noFeatures},
        {VK_FORMAT_R8_UINT, noFeatures, noFeatures},     {VK_FORMAT_R8_SINT, noFeatures, noFeatures},
        {VK_FORMAT_R16_UNORM, noFeatures, noFeatures},   {VK_FORMAT_R16_SNORM, noFeatures, noFeatures},
        {VK_FORMAT_R16_USCALED, noFeatures, noFeatures}, {VK_FORMAT_R16_SSCALED, noFeatures, noFeatures},
        {VK_FORMAT_R16_UINT, noFeatures, noFeatures},    {VK_FORMAT_R16_SINT, noFeatures, noFeatures},
        {VK_FORMAT_R16_SFLOAT, noFeatures, noFeatures},  {VK_FORMAT_R32_UINT, noFeatures, noFeatures},
        {VK_FORMAT_R32_SINT, noFeatures, noFeatures},    {VK_FORMAT_R32_SFLOAT, noFeatures, noFeatures},
        {VK_FORMAT_R64_UINT, noFeatures, noFeatures},    {VK_FORMAT_R64_SINT, noFeatures, noFeatures},
        {VK_FORMAT_R64_SFLOAT, noFeatures, noFeatures},
    };

    for (const auto &params : testCases)
    {
        addFunctionCase(requiredFormatsTests.get(), generateTestNameRequiredFormatFeatures(params),
                        checkRequiredFormatFeatures, testRequiredFormatFeatures, params);
    }

    return requiredFormatsTests.release();
}

} // namespace tensor
} // namespace vkt
