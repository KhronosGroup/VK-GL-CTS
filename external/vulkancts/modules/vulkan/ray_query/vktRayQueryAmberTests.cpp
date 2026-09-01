/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 Google LLC
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
 * \brief Ray Query Amber Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryAmberTests.hpp"
#include "vktTestCase.hpp"
#include "amber/vktAmberTestCase.hpp"

namespace vkt
{
namespace RayQuery
{

tcu::TestCaseGroup *createAmberTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "amber", "Amber ray query test cases"));

#ifndef CTS_USES_VULKANSC
    const char *rayQueryList[] = {"AccelerationStructureFeaturesKHR.accelerationStructure",
                                  "RayQueryFeaturesKHR.rayQuery",
                                  "BufferDeviceAddressFeatures.bufferDeviceAddress",
                                  "VK_KHR_acceleration_structure",
                                  "VK_KHR_ray_query",
                                  "VK_KHR_buffer_device_address",
                                  "VK_KHR_spirv_1_4",
                                  ""};
    const struct
    {
        const char *name;
        const char *fileName;
        const char *const *requirements;
    } amberTests[] = {
        {"workgroup_barrier", "ray_query_workgroup_barrier.amber", rayQueryList},
    };

    const char *dataDir = "ray_query";
    for (auto test : amberTests)
    {
        cts_amber::AmberTestCase *testCase = cts_amber::createAmberTestCase(testCtx, test.name, "", dataDir, test.fileName);

        for (size_t i = 0;; i++)
        {
            const char *requirement = test.requirements[i];
            if (requirement[0] == 0)
                break;
            testCase->addRequirement(requirement);
        }

        group->addChild(testCase);
    }
#endif

    return group.release();
}

} // namespace RayQuery
} // namespace vkt
