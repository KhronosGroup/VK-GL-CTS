/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Image processing API tests
 *//*--------------------------------------------------------------------*/

#include "vktImageProcessingBase.hpp"
#include "vktImageProcessingTests.hpp"
#include "vktImageProcessingApiTests.hpp"
#include "vktImageProcessingTestsUtil.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"

#include "deRandom.hpp"

#include "tcuTestCase.hpp"
#include "tcuVectorType.hpp"

using namespace vk;
using namespace tcu;

namespace vkt
{
namespace ImageProcessing
{

namespace
{

class ImageProcessingApiTest : public TestCase
{
public:
    ImageProcessingApiTest(TestContext &testCtx, const std::string &name);
    ~ImageProcessingApiTest(void);

    void checkSupport(Context &context) const;
    TestInstance *createInstance(Context &context) const;
};

ImageProcessingApiTest::ImageProcessingApiTest(TestContext &testCtx, const std::string &name) : TestCase(testCtx, name)
{
}

ImageProcessingApiTest::~ImageProcessingApiTest(void)
{
}

void ImageProcessingApiTest::checkSupport(Context &context) const
{
    if (context.getUsedApiVersion() < VK_API_VERSION_1_3)
        context.requireDeviceFunctionality("VK_KHR_format_feature_flags2");

    context.requireDeviceFunctionality("VK_QCOM_image_processing");
}

class ImageProcessingApiTestInstance : public TestInstance
{
public:
    ImageProcessingApiTestInstance(Context &context);
    ~ImageProcessingApiTestInstance(void);
    TestStatus iterate(void);

private:
    de::Random m_rnd;
};

ImageProcessingApiTestInstance::ImageProcessingApiTestInstance(Context &context) : TestInstance(context), m_rnd(1234)
{
}

ImageProcessingApiTestInstance::~ImageProcessingApiTestInstance(void)
{
}

TestStatus ImageProcessingApiTestInstance::iterate(void)
{
    const InstanceInterface &instInterface = m_context.getInstanceInterface();

    uint32_t testIterations = m_rnd.getInt(1u, 20u);

    for (uint32_t iterIdx = 0; iterIdx < testIterations; iterIdx++)
    {
        VkPhysicalDeviceImageProcessingPropertiesQCOM imgProcProperties;
        deMemset(&imgProcProperties, 0, sizeof(imgProcProperties));
        imgProcProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_PROPERTIES_QCOM;

        VkPhysicalDeviceProperties2 properties2;
        deMemset(&properties2, 0, sizeof(properties2));
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &imgProcProperties;

        instInterface.getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties2);

        if (imgProcProperties.maxWeightFilterPhases < 1024u)
            return TestStatus::fail("Property maxWeightFilterPhases is less than the minimum limit");

        if ((imgProcProperties.maxWeightFilterDimension.width < 64) ||
            (imgProcProperties.maxWeightFilterDimension.height < 64))
            return TestStatus::fail("Property maxWeightFilterDimension is less than the minimum limit");

        if ((imgProcProperties.maxBoxFilterBlockSize.width < 64) ||
            (imgProcProperties.maxBoxFilterBlockSize.height < 64))
            return TestStatus::fail("Property maxBoxFilterBlockSize is less than the minimum limit");

        if ((imgProcProperties.maxBlockMatchRegion.width < 64) || (imgProcProperties.maxBlockMatchRegion.height < 64))
            return TestStatus::fail("Property maxBlockMatchRegion is less than the minimum limit");
    }

    return TestStatus::pass("Pass");
}

TestInstance *ImageProcessingApiTest::createInstance(Context &context) const
{
    return new ImageProcessingApiTestInstance(context);
}

} // namespace

TestCaseGroup *createImageProcessingApiTests(TestContext &testCtx)
{
    de::MovePtr<TestCaseGroup> testGroup(new TestCaseGroup(testCtx, "api"));

    testGroup->addChild(new ImageProcessingApiTest(testCtx, "properties"));

    return testGroup.release();
}

} // namespace ImageProcessing
} // namespace vkt
