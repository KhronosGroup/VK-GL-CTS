/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2017 Khronos Group
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
* \brief API Maintenance6 Check test - checks structs and function from VK_KHR_maintenance6
*//*--------------------------------------------------------------------*/

#include "tcuTestLog.hpp"

#include "vkQueryUtil.hpp"

#include "vktApiMaintenance6Check.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include <sstream>
#include <limits>
#include <utility>
#include <algorithm>
#include <map>
#include <set>

#ifndef CTS_USES_VULKANSC

using namespace vk;

namespace vkt
{

    namespace api
    {

        namespace
        {

            class Maintenance6MaxCombinedImageSamplerDescriptorCountTestInstance : public TestInstance
            {
            public:
                Maintenance6MaxCombinedImageSamplerDescriptorCountTestInstance(Context& ctx)
                    : TestInstance(ctx)
                {}
                virtual tcu::TestStatus iterate(void)
                {
                    const InstanceInterface& vki(m_context.getInstanceInterface());
                    const VkPhysicalDevice physicalDevice(m_context.getPhysicalDevice());

                    VkPhysicalDeviceMaintenance6PropertiesKHR maintProp6 = initVulkanStructure();
                    VkPhysicalDeviceProperties2 prop2 = initVulkanStructure(&maintProp6);

                    vki.getPhysicalDeviceProperties2(physicalDevice, &prop2);

                    static const struct
                    {
                        VkFormat begin;
                        VkFormat end;
                    }
                    s_formatRanges[] =
                    {
                        // YCbCr formats
                        { VK_FORMAT_G8B8G8R8_422_UNORM, (VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM + 1) },

                        // YCbCr extended formats
                        { VK_FORMAT_G8_B8R8_2PLANE_444_UNORM, (VkFormat)(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM + 1) },

                        // VK_FORMAT_R16G16_S10_5_NV
                        { VK_FORMAT_R16G16_S10_5_NV, (VkFormat)(VK_FORMAT_R16G16_S10_5_NV + 1) },
                    };

                    for (int rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(s_formatRanges); ++rangeNdx)
                    {
                        const VkFormat rangeBegin = s_formatRanges[rangeNdx].begin;
                        const VkFormat rangeEnd = s_formatRanges[rangeNdx].end;

                        for (VkFormat format = rangeBegin; format != rangeEnd; format = (VkFormat)(format + 1))
                        {
                            VkSamplerYcbcrConversionImageFormatProperties conversionImageFormatProps = initVulkanStructure();
                            VkImageFormatProperties2 formatProps = initVulkanStructure(&conversionImageFormatProps);
                            VkPhysicalDeviceImageFormatInfo2 imageInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, nullptr, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0U };
                            vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageInfo, &formatProps);
                            if (conversionImageFormatProps.combinedImageSamplerDescriptorCount > maintProp6.maxCombinedImageSamplerDescriptorCount)
                            {
                                return tcu::TestStatus::fail("Fail: format " + std::string(getFormatName(format)) + " requires a larger combinedImageSamplerDescriptorCount=" + std::to_string(conversionImageFormatProps.combinedImageSamplerDescriptorCount) +
                                    " than maxCombinedImageSamplerDescriptorCount=" + std::to_string(maintProp6.maxCombinedImageSamplerDescriptorCount));
                            }
                        }
                    }

                    return tcu::TestStatus::pass("Pass");
                }
            };

            class Maintenance6MaxCombinedImageSamplerDescriptorCountTestCase : public TestCase
            {
            public:
                Maintenance6MaxCombinedImageSamplerDescriptorCountTestCase(tcu::TestContext& testCtx)
                    : TestCase(testCtx, "maintenance6_properties")
                {}

                virtual ~Maintenance6MaxCombinedImageSamplerDescriptorCountTestCase(void)
                {}
                virtual void checkSupport(Context& ctx) const
                {
                    ctx.requireDeviceFunctionality("VK_KHR_maintenance6");
                }
                virtual TestInstance* createInstance(Context& ctx) const
                {
                    return new Maintenance6MaxCombinedImageSamplerDescriptorCountTestInstance(ctx);
                }

            private:
            };

        } // anonymous

        tcu::TestCaseGroup* createMaintenance6Tests(tcu::TestContext& testCtx)
        {
            de::MovePtr<tcu::TestCaseGroup> main6Tests(new tcu::TestCaseGroup(testCtx, "maintenance6_check", "Maintenance6 Tests"));
            main6Tests->addChild(new Maintenance6MaxCombinedImageSamplerDescriptorCountTestCase(testCtx));

            return main6Tests.release();
        }

    } // api
} // vkt

#endif // CTS_USES_VULKANSC
