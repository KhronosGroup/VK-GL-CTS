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

void checkRequired(Context &ctx)
{
    ctx.requireDeviceFunctionality("VK_ARM_tensors");
}

tcu::TestStatus testRequiredLimits(Context &ctx)
{
    const VkPhysicalDeviceTensorPropertiesARM props  = getTensorPhysicalDeviceProperties(ctx);
    const VkPhysicalDeviceTensorFeaturesARM features = getTensorPhysicalDeviceFeatures(ctx);

    if (props.maxTensorDimensionCount < 4)
    {
        return tcu::TestStatus::fail("VkPhysicalDeviceTensorPropertiesARM::maxTensorDimensionCount < 4");
    }

    if (props.maxTensorElements < 65536)
    {
        return tcu::TestStatus::fail("VkPhysicalDeviceTensorPropertiesARM::maxTensorElements < 65536");
    }

    if (props.maxPerDimensionTensorElements < 65536)
    {
        return tcu::TestStatus::fail("VkPhysicalDeviceTensorPropertiesARM::maxPerDimensionTensorElements < 65536");
    }

    if (props.maxTensorStride < 65536)
    {
        return tcu::TestStatus::fail("VkPhysicalDeviceTensorPropertiesARM::maxTensorStride < 65536");
    }

    if (props.maxTensorSize < 65536)
    {
        return tcu::TestStatus::fail("VkPhysicalDeviceTensorPropertiesARM::maxTensorSize < 65536");
    }

    if (props.maxDescriptorSetStorageTensors < 16)
    {
        return tcu::TestStatus::fail("VkPhysicalDeviceTensorPropertiesARM::maxDescriptorSetStorageTensors < 16");
    }

    if (features.descriptorBindingStorageTensorUpdateAfterBind)
    {
        if (props.maxDescriptorSetUpdateAfterBindStorageTensors < 500000)
        {
            return tcu::TestStatus::fail(
                "VkPhysicalDeviceTensorPropertiesARM::maxDescriptorSetUpdateAfterBindStorageTensors < 500000");
        }
    }

    if (features.descriptorBindingStorageTensorUpdateAfterBind && features.shaderTensorAccess)
    {
        if (props.maxPerStageDescriptorUpdateAfterBindStorageTensors < 500000)
        {
            return tcu::TestStatus::fail(
                "VkPhysicalDeviceTensorPropertiesARM::maxPerStageDescriptorUpdateAfterBindStorageTensors < 500000");
        }
    }

    if (features.shaderTensorAccess)
    {
        if (!(props.shaderTensorSupportedStages & VK_SHADER_STAGE_COMPUTE_BIT))
        {
            return tcu::TestStatus::fail(
                "VkPhysicalDeviceTensorPropertiesARM::shaderTensorSupportedStages does not have "
                "VK_SHADER_STAGE_COMPUTE_BIT set");
        }

        if (props.maxTensorShaderAccessArrayLength < 4)
        {
            return tcu::TestStatus::fail("VkPhysicalDeviceTensorPropertiesARM::maxTensorShaderAccessArrayLength < 4");
        }

        if (props.maxTensorShaderAccessSize < 4)
        {
            return tcu::TestStatus::fail("VkPhysicalDeviceTensorPropertiesARM::maxTensorShaderAccessSize < 4");
        }

        if (props.maxPerStageDescriptorSetStorageTensors < 16)
        {
            return tcu::TestStatus::fail(
                "VkPhysicalDeviceTensorPropertiesARM::maxPerStageDescriptorSetStorageTensors < 16");
        }
    }

    return tcu::TestStatus::pass("Success");
}

tcu::TestStatus testRequiredFeatures(Context &ctx)
{
    const VkPhysicalDeviceTensorFeaturesARM features = getTensorPhysicalDeviceFeatures(ctx);

    if (!features.tensors)
    {
        return tcu::TestStatus::fail("Required feature VkPhysicalDeviceTensorFeaturesARM::tensors is not supported");
    }

    return tcu::TestStatus::pass("Success");
}

} // namespace

tcu::TestCaseGroup *createTensorRequired(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> requiredTests(new tcu::TestCaseGroup(testCtx, "required"));

    addFunctionCase(requiredTests.get(), "limits", checkRequired, testRequiredLimits);
    addFunctionCase(requiredTests.get(), "features", checkRequired, testRequiredFeatures);

    return requiredTests.release();
}

} // namespace tensor
} // namespace vkt
