/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
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
 * \brief Ray Tracing Barycentric Coordinates Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingAmberTests.hpp"
#include "vktTestCase.hpp"
#include "amber/vktAmberTestCase.hpp"

namespace vkt
{
namespace RayTracing
{

tcu::TestCaseGroup *createAmberTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "amber", "Amber ray tracing test cases"));

#ifndef CTS_USES_VULKANSC
    const char *stdRayTracingList[] = {"AccelerationStructureFeaturesKHR.accelerationStructure",
                                       "BufferDeviceAddressFeatures.bufferDeviceAddress",
                                       "RayTracingPipelineFeaturesKHR.rayTracingPipeline",
                                       "VK_KHR_acceleration_structure",
                                       "VK_KHR_buffer_device_address",
                                       "VK_KHR_ray_tracing_pipeline",
                                       ""};
    const char *extRayTracingList[] = {"AccelerationStructureFeaturesKHR.accelerationStructure",
                                       "BufferDeviceAddressFeatures.bufferDeviceAddress",
                                       "RayTracingPipelineFeaturesKHR.rayTracingPipeline",
                                       "VK_KHR_acceleration_structure",
                                       "VK_KHR_buffer_device_address",
                                       "VK_KHR_ray_tracing_pipeline",
                                       "VK_KHR_deferred_host_operations",
                                       ""};
    const struct
    {
        const char *name;
        const char *const *exts;
    } amberTests[] = {
        {"basic", stdRayTracingList},
        {"basic2", stdRayTracingList},
        {"rt-sample", extRayTracingList},
    };

    const char *dataDir = "ray_tracing";
    for (auto test : amberTests)
    {
        const std::string fileName         = std::string(test.name) + ".amber";
        cts_amber::AmberTestCase *testCase = cts_amber::createAmberTestCase(testCtx, test.name, "", dataDir, fileName);

        for (size_t i = 0;; i++)
        {
            const char *ext = test.exts[i];
            if (ext[0] == 0)
                break;
            testCase->addRequirement(ext);
        }

        group->addChild(testCase);
    }
#endif

    return group.release();
}

} // namespace RayTracing
} // namespace vkt
