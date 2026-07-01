/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Pipeline barrier tests - transfer-only queue backend
 *//*--------------------------------------------------------------------*/

#include "vktMemoryPipelineBarrierTransferTests.hpp"

#include "vktTestCaseUtil.hpp"

namespace vkt
{
namespace memory
{
namespace pipelinebarrier
{
namespace
{

void checkSupport(vkt::Context &context, TestConfig config)
{
    DE_UNREF(config);

    if (context.getTransferQueueFamilyIndex() == -1)
        TCU_THROW(NotSupportedError, "No dedicated transfer queue available");
}

} // namespace

tcu::TestCaseGroup *createTransferTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "transfer"));
    const vk::VkDeviceSize sizes[] = {
        1024,         // 1K
        8 * 1024,     // 8K
        64 * 1024,    // 64K
        ONE_MEGABYTE, // 1M
    };
    const Usage readUsages[]  = {USAGE_HOST_READ, USAGE_TRANSFER_SRC};
    const Usage writeUsages[] = {USAGE_HOST_WRITE, USAGE_TRANSFER_DST};

    for (size_t writeUsageNdx = 0; writeUsageNdx < DE_LENGTH_OF_ARRAY(writeUsages); writeUsageNdx++)
    {
        const Usage writeUsage = writeUsages[writeUsageNdx];

        for (size_t readUsageNdx = 0; readUsageNdx < DE_LENGTH_OF_ARRAY(readUsages); readUsageNdx++)
        {
            const Usage readUsage = readUsages[readUsageNdx];
            const Usage usage     = writeUsage | readUsage;
            const string usageGroupName(usageToName(usage));
            de::MovePtr<tcu::TestCaseGroup> usageGroup(new tcu::TestCaseGroup(testCtx, usageGroupName.c_str()));

            for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); sizeNdx++)
            {
                const vk::VkDeviceSize size = sizes[sizeNdx];
                const string testName(de::toString((uint64_t)(size)));
                const TestConfig config = {usage, DEFAULT_VERTEX_BUFFER_STRIDE, size, vk::VK_SHARING_MODE_EXCLUSIVE,
                                           BACKEND_TRANSFER};

                usageGroup->addChild(
                    new InstanceFactory1WithSupport<MemoryTestInstance, TestConfig, FunctionSupport1<TestConfig>>(
                        testCtx, testName, config, typename FunctionSupport1<TestConfig>::Args(checkSupport, config)));
            }

            group->addChild(usageGroup.get());
            usageGroup.release();
        }
    }

    return group.release();
}

} // namespace pipelinebarrier
} // namespace memory
} // namespace vkt
