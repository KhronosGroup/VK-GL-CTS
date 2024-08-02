/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Queue bind sparse tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesQueueBindSparseTests.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include <string>
#include <vector>

using namespace vk;
using de::MovePtr;

namespace vkt
{
namespace sparse
{
namespace
{

typedef de::SharedPtr<Unique<VkSemaphore>> SemaphoreSp;
typedef de::SharedPtr<Unique<VkFence>> FenceSp;

struct TestParams
{
    uint32_t numQueues; //! use 2 or more to sync between different queues
    uint32_t numWaitSemaphores;
    uint32_t numSignalSemaphores;
    bool emptySubmission; //! will make an empty bind sparse submission
    bool bindSparseUseFence;
};

struct QueueSubmission
{
    union InfoUnion
    {
        VkSubmitInfo regular;
        VkBindSparseInfo sparse;
    };

    const Queue *queue;
    bool isSparseBinding;
    InfoUnion info;
};

QueueSubmission makeSubmissionRegular(const Queue *queue, const uint32_t numWaitSemaphores,
                                      const VkSemaphore *pWaitSemaphore, const VkPipelineStageFlags *pWaitDstStageMask,
                                      const uint32_t numSignalSemaphores, const VkSemaphore *pSignalSemaphore)
{
    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        numWaitSemaphores,             // uint32_t waitSemaphoreCount;
        pWaitSemaphore,                // const VkSemaphore* pWaitSemaphores;
        pWaitDstStageMask,             // const VkPipelineStageFlags* pWaitDstStageMask;
        0u,                            // uint32_t commandBufferCount;
        nullptr,                       // const VkCommandBuffer* pCommandBuffers;
        numSignalSemaphores,           // uint32_t signalSemaphoreCount;
        pSignalSemaphore,              // const VkSemaphore* pSignalSemaphores;
    };

    QueueSubmission submission;
    submission.isSparseBinding = false;
    submission.queue           = queue;
    submission.info.regular    = submitInfo;

    return submission;
}

QueueSubmission makeSubmissionSparse(const Queue *queue, const uint32_t numWaitSemaphores,
                                     const VkSemaphore *pWaitSemaphore, const uint32_t numSignalSemaphores,
                                     const VkSemaphore *pSignalSemaphore)
{
    const VkBindSparseInfo bindInfo = {
        VK_STRUCTURE_TYPE_BIND_SPARSE_INFO, // VkStructureType sType;
        nullptr,                            // const void* pNext;
        numWaitSemaphores,                  // uint32_t waitSemaphoreCount;
        pWaitSemaphore,                     // const VkSemaphore* pWaitSemaphores;
        0u,                                 // uint32_t bufferBindCount;
        nullptr,                            // const VkSparseBufferMemoryBindInfo* pBufferBinds;
        0u,                                 // uint32_t imageOpaqueBindCount;
        nullptr,                            // const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
        0u,                                 // uint32_t imageBindCount;
        nullptr,                            // const VkSparseImageMemoryBindInfo* pImageBinds;
        numSignalSemaphores,                // uint32_t signalSemaphoreCount;
        pSignalSemaphore,                   // const VkSemaphore* pSignalSemaphores;
    };

    QueueSubmission submission;
    submission.isSparseBinding = true;
    submission.queue           = queue;
    submission.info.sparse     = bindInfo;

    return submission;
}

bool waitForFences(const DeviceInterface &vk, const VkDevice device, const std::vector<FenceSp> &fences)
{
    for (std::vector<FenceSp>::const_iterator fenceSpIter = fences.begin(); fenceSpIter != fences.end(); ++fenceSpIter)
    {
        if (vk.waitForFences(device, 1u, &(***fenceSpIter), VK_TRUE, ~0ull) != VK_SUCCESS)
            return false;
    }
    return true;
}

class SparseQueueBindTestInstance : public SparseResourcesBaseInstance
{
public:
    SparseQueueBindTestInstance(Context &context, const TestParams &params)
        : SparseResourcesBaseInstance(context)
        , m_params(params)
    {
        DE_ASSERT(m_params.numQueues > 0u); // must use at least one queue
        DE_ASSERT(!m_params.emptySubmission ||
                  (m_params.numWaitSemaphores == 0u &&
                   m_params.numSignalSemaphores == 0u)); // can't use semaphores if we don't submit
    }

    tcu::TestStatus iterate(void)
    {
        const Queue *sparseQueue = nullptr;
        std::vector<const Queue *> otherQueues;

        // Determine required queues and create a device that supports them
        {
            QueueRequirementsVec requirements;
            requirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
            requirements.push_back(QueueRequirements((VkQueueFlags)0, m_params.numQueues)); // any queue flags

            createDeviceSupportingQueues(requirements);

            sparseQueue = &getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0u);

            // We probably have picked the sparse queue again, so filter it out
            for (uint32_t queueNdx = 0u; queueNdx < m_params.numQueues; ++queueNdx)
            {
                const Queue *queue = &getQueue((VkQueueFlags)0, queueNdx);
                if (queue->queueHandle != sparseQueue->queueHandle)
                    otherQueues.push_back(queue);
            }
        }

        const DeviceInterface &vk = getDeviceInterface();

        std::vector<SemaphoreSp> allSemaphores;
        std::vector<VkSemaphore> waitSemaphores;
        std::vector<VkSemaphore> signalSemaphores;
        std::vector<VkPipelineStageFlags> signalSemaphoresWaitDstStageMask;
        std::vector<QueueSubmission> queueSubmissions;

        for (uint32_t i = 0; i < m_params.numWaitSemaphores; ++i)
        {
            allSemaphores.push_back(makeVkSharedPtr(createSemaphore(vk, getDevice())));
            waitSemaphores.push_back(**allSemaphores.back());
        }

        for (uint32_t i = 0; i < m_params.numSignalSemaphores; ++i)
        {
            allSemaphores.push_back(makeVkSharedPtr(createSemaphore(vk, getDevice())));
            signalSemaphores.push_back(**allSemaphores.back());
            signalSemaphoresWaitDstStageMask.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }

        // Prepare submissions: signal semaphores for the sparse bind operation
        {
            uint32_t numQueues     = 1u + static_cast<uint32_t>(otherQueues.size());
            uint32_t numSemaphores = m_params.numWaitSemaphores;

            while (numSemaphores > 0u && numQueues > 0u)
            {
                if (numQueues == 1u) // sparse queue is assigned last
                {
                    // sparse queue can handle regular submissions as well
                    queueSubmissions.push_back(makeSubmissionRegular(sparseQueue, 0u, nullptr, nullptr, numSemaphores,
                                                                     getDataOrNullptr(waitSemaphores)));
                    numSemaphores = 0u;
                    numQueues     = 0u;
                }
                else
                {
                    queueSubmissions.push_back(
                        makeSubmissionRegular(otherQueues[numQueues - 2], 0u, nullptr, nullptr, 1u,
                                              getDataOrNullptr(waitSemaphores, numSemaphores - 1)));
                    --numQueues;
                    --numSemaphores;
                }
            }
        }

        // Prepare submission: bind sparse
        if (!m_params.emptySubmission)
        {
            queueSubmissions.push_back(
                makeSubmissionSparse(sparseQueue, m_params.numWaitSemaphores, getDataOrNullptr(waitSemaphores),
                                     m_params.numSignalSemaphores, getDataOrNullptr(signalSemaphores)));
        }
        else
        {
            // an unused submission, won't be used in a call to vkQueueBindSparse
            queueSubmissions.push_back(makeSubmissionSparse(sparseQueue, 0u, nullptr, 0u, nullptr));
        }

        // Prepare submissions: wait on semaphores signaled by the sparse bind operation
        if (!m_params.emptySubmission)
        {
            uint32_t numQueues     = 1u + static_cast<uint32_t>(otherQueues.size());
            uint32_t numSemaphores = m_params.numSignalSemaphores;

            while (numSemaphores > 0u && numQueues > 0u)
            {
                if (numQueues == 1u)
                {
                    queueSubmissions.push_back(
                        makeSubmissionRegular(sparseQueue, numSemaphores, getDataOrNullptr(signalSemaphores),
                                              getDataOrNullptr(signalSemaphoresWaitDstStageMask), 0u, nullptr));
                    numSemaphores = 0u;
                    numQueues     = 0u;
                }
                else
                {
                    queueSubmissions.push_back(makeSubmissionRegular(
                        otherQueues[numQueues - 2], 1u, getDataOrNullptr(signalSemaphores, numSemaphores - 1),
                        getDataOrNullptr(signalSemaphoresWaitDstStageMask, numSemaphores - 1), 0u, nullptr));
                    --numQueues;
                    --numSemaphores;
                }
            }
        }

        // Submit to queues
        {
            std::vector<FenceSp> regularFences;
            std::vector<FenceSp> bindSparseFences;

            for (std::vector<QueueSubmission>::const_iterator submissionIter = queueSubmissions.begin();
                 submissionIter != queueSubmissions.end(); ++submissionIter)
            {
                if (submissionIter->isSparseBinding)
                {
                    VkFence fence = VK_NULL_HANDLE;

                    if (m_params.bindSparseUseFence)
                    {
                        bindSparseFences.push_back(makeVkSharedPtr(createFence(vk, getDevice())));
                        fence = **bindSparseFences.back();
                    }

                    if (m_params.emptySubmission)
                        VK_CHECK(vk.queueBindSparse(submissionIter->queue->queueHandle, 0u, nullptr, fence));
                    else
                        VK_CHECK(vk.queueBindSparse(submissionIter->queue->queueHandle, 1u,
                                                    &submissionIter->info.sparse, fence));
                }
                else
                {
                    regularFences.push_back(makeVkSharedPtr(createFence(vk, getDevice())));
                    VK_CHECK(vk.queueSubmit(submissionIter->queue->queueHandle, 1u, &submissionIter->info.regular,
                                            **regularFences.back()));
                }
            }

            if (!waitForFences(vk, getDevice(), bindSparseFences))
                return tcu::TestStatus::fail("vkQueueBindSparse didn't signal the fence");

            if (!waitForFences(vk, getDevice(), regularFences))
                return tcu::TestStatus::fail(
                    "Some fences weren't signaled (vkQueueBindSparse didn't signal semaphores?)");
        }

        // May return an error if some waitSemaphores didn't get signaled
        VK_CHECK(vk.deviceWaitIdle(getDevice()));

        return tcu::TestStatus::pass("Pass");
    }

private:
    const TestParams m_params;
};

class SparseQueueBindTest : public TestCase
{
public:
    SparseQueueBindTest(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
        DE_ASSERT(params.numQueues > 0u);
        DE_ASSERT(params.numQueues == 1u || m_params.numWaitSemaphores > 0u ||
                  m_params.numSignalSemaphores > 0u); // without any semaphores, only sparse queue will be used
    }

    TestInstance *createInstance(Context &context) const
    {
        return new SparseQueueBindTestInstance(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
    }

private:
    const TestParams m_params;
};

void populateTestGroup(tcu::TestCaseGroup *group)
{
    const struct
    {
        std::string name;
        TestParams params;
    } cases[] = {
        // case name                                    // numQueues, numWaitSems, numSignalSems, emptySubmission, checkFence
        {"no_dependency",
         {
             1u,
             0u,
             0u,
             false,
             false,
         }},
        {"no_dependency_fence",
         {
             1u,
             0u,
             0u,
             false,
             true,
         }},

        {"single_queue_wait_one",
         {
             1u,
             1u,
             0u,
             false,
             true,
         }},
        {"single_queue_wait_many",
         {
             1u,
             3u,
             0u,
             false,
             true,
         }},
        {"single_queue_signal_one",
         {
             1u,
             0u,
             1u,
             false,
             true,
         }},
        {"single_queue_signal_many",
         {
             1u,
             0u,
             3u,
             false,
             true,
         }},
        {"single_queue_wait_one_signal_one",
         {
             1u,
             1u,
             1u,
             false,
             true,
         }},
        {"single_queue_wait_many_signal_many",
         {
             1u,
             2u,
             3u,
             false,
             true,
         }},

        {"multi_queue_wait_one",
         {
             2u,
             1u,
             0u,
             false,
             true,
         }},
        {"multi_queue_wait_many",
         {
             2u,
             2u,
             0u,
             false,
             true,
         }},
        {"multi_queue_signal_one",
         {
             2u,
             0u,
             1u,
             false,
             true,
         }},
        {"multi_queue_signal_many",
         {
             2u,
             0u,
             2u,
             false,
             true,
         }},
        {"multi_queue_wait_one_signal_one",
         {
             2u,
             1u,
             1u,
             false,
             true,
         }},
        {"multi_queue_wait_many_signal_many",
         {
             2u,
             2u,
             2u,
             false,
             true,
         }},
        {"multi_queue_wait_one_signal_one_other",
         {
             2u,
             1u,
             1u,
             false,
             true,
         }},
        {"multi_queue_wait_many_signal_many_other",
         {
             3u,
             2u,
             2u,
             false,
             true,
         }},

        {"empty",
         {
             1u,
             0u,
             0u,
             true,
             false,
         }},
        {"empty_fence",
         {
             1u,
             0u,
             0u,
             true,
             true,
         }},
    };

    for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
        group->addChild(new SparseQueueBindTest(group->getTestContext(), cases[caseNdx].name, cases[caseNdx].params));
}

} // namespace

//! Sparse queue binding edge cases and synchronization with semaphores/fences.
//! Actual binding and usage is tested by other test groups.
tcu::TestCaseGroup *createQueueBindSparseTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "queue_bind", populateTestGroup);
}

} // namespace sparse
} // namespace vkt
