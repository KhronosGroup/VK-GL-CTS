/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015-2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google Inc.
 * Copyright (c) 2015-2016 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Copies And Blitting Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingTests.hpp"
#include "vktApiCopiesAndBlittingUtil.hpp"

#include "vktApiCopyImageToImageTests.hpp"
#include "vktApiCopyBufferToBufferTests.hpp"
#include "vktApiCopyImageToBufferTests.hpp"
#include "vktApiCopyBufferToImageTests.hpp"

#include "vktApiCopyBufferToDepthStencilTests.hpp"
#include "vktApiCopyDepthStencilToBufferTests.hpp"
#include "vktApiCopyDepthStencilMSAATests.hpp"

#include "vktApiBlittingTests.hpp"

#include "vktApiResolveTests.hpp"

#include "vktApiCopiesAndBlittingDynamicStateMetaOpsTests.hpp"
#include "vktApiCopiesAndBlittingReinterpretTests.hpp"
#include "vktApiCopyMultiplaneImageTransferQueueTests.hpp"
#include "vktApiCopyMemoryIndirectTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

void addSparseCopyTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    DE_ASSERT((extensionFlags & COPY_COMMANDS_2) && (extensionFlags & SPARSE_BINDING));

    {
        TestGroupParamsPtr universalGroupParams(new TestGroupParams{
            allocationKind,
            extensionFlags,
            QueueSelectionOptions::Universal,
            false,
            true,
            false,
        });
        addTestGroup(group, "image_to_image", addCopyImageToImageTests, universalGroupParams);
    }
}

void addIndirectCopyTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    (void)group;
    TestGroupParamsPtr universalGroupParams(new TestGroupParams{
        allocationKind,
        extensionFlags,
        QueueSelectionOptions::Universal,
        false,
        false,
        false,
    });
#ifndef CTS_USES_VULKANSC
    addTestGroup(group, "memory_to_image_indirect", addCopyMemoryToImageTests, universalGroupParams);
    addTestGroup(group, "memory_to_depthstencil_indirect", addCopyBufferToDepthStencilTests, universalGroupParams);
    addTestGroup(group, "image_to_buffer_indirect", addCopyImageToBufferIndirectTests, universalGroupParams);
#endif

    TestGroupParamsPtr transferOnlyGroup(new TestGroupParams{
        allocationKind,
        extensionFlags,
        QueueSelectionOptions::TransferOnly,
        false,
        false,
        false,
    });
#ifndef CTS_USES_VULKANSC
    addTestGroup(group, "memory_to_image_indirect_transfer_queue", addCopyMemoryToImageTests, transferOnlyGroup);
    addTestGroup(group, "image_to_buffer_indirect_transfer_queue", addCopyImageToBufferIndirectTests,
                 transferOnlyGroup);
#endif

    TestGroupParamsPtr computeOnlyGroup(new TestGroupParams{
        allocationKind,
        extensionFlags,
        QueueSelectionOptions::ComputeOnly,
        false,
        false,
        false,
    });
#ifndef CTS_USES_VULKANSC
    addTestGroup(group, "memory_to_image_indirect_compute_queue", addCopyMemoryToImageTests, computeOnlyGroup);
    addTestGroup(group, "image_to_buffer_indirect_compute_queue", addCopyImageToBufferIndirectTests, computeOnlyGroup);
#endif
}

void addCopiesAndBlittingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestGroupParamsPtr universalGroupParams(new TestGroupParams{
        allocationKind,
        extensionFlags,
        QueueSelectionOptions::Universal,
        false,
        false,
        false,
    });

    addTestGroup(group, "image_to_image", addCopyImageToImageTests, universalGroupParams);
    addTestGroup(group, "image_to_buffer", addCopyImageToBufferTests, universalGroupParams);
    addTestGroup(group, "buffer_to_image", addCopyBufferToImageTests, universalGroupParams);
    addTestGroup(group, "buffer_to_depthstencil", addCopyBufferToDepthStencilTests, universalGroupParams);
    addTestGroup(group, "depthstencil_to_buffer", addCopyDepthStencilToBufferTests, universalGroupParams);
    addTestGroup(group, "buffer_to_buffer", addCopyBufferToBufferTests, universalGroupParams);
    addTestGroup(group, "blit_image", addBlittingImageTests, allocationKind, extensionFlags);
    addTestGroup(group, "resolve_image", addResolveImageTests, allocationKind, extensionFlags);
    addTestGroup(group, "depth_stencil_msaa_copy", addCopyDepthStencilMSAATests, allocationKind, extensionFlags);

    TestGroupParamsPtr computeOnlyMaint10Group(new TestGroupParams{
        allocationKind,
        extensionFlags | MAINTENANCE_10,
        QueueSelectionOptions::ComputeOnly,
        false,
        false,
        false,
    });
    addTestGroup(group, "buffer_to_depthstencil_compute_queue", addCopyBufferToDepthStencilTests,
                 computeOnlyMaint10Group);
    addTestGroup(group, "depthstencil_to_buffer_compute_queue", addCopyDepthStencilToBufferTests,
                 computeOnlyMaint10Group);

    TestGroupParamsPtr transferOnlyMaint10Group(new TestGroupParams{
        allocationKind,
        extensionFlags | MAINTENANCE_10,
        QueueSelectionOptions::TransferOnly,
        false,
        false,
        false,
    });
    addTestGroup(group, "buffer_to_depthstencil_transfer_queue", addCopyBufferToDepthStencilTests,
                 transferOnlyMaint10Group);
    addTestGroup(group, "depthstencil_to_buffer_transfer_queue", addCopyDepthStencilToBufferTests,
                 transferOnlyMaint10Group);

    TestGroupParamsPtr transferOnlyGroup(new TestGroupParams{
        allocationKind,
        extensionFlags,
        QueueSelectionOptions::TransferOnly,
        false,
        false,
        false,
    });
    addTestGroup(group, "image_to_buffer_transfer_queue", addCopyImageToBufferTests, transferOnlyGroup);
    addTestGroup(group, "buffer_to_image_transfer_queue", addCopyBufferToImageTests, transferOnlyGroup);
    addTestGroup(group, "buffer_to_buffer_transfer_queue", addCopyBufferToBufferTests, transferOnlyGroup);

    TestGroupParamsPtr computeOnlyGroup(new TestGroupParams{
        allocationKind,
        extensionFlags,
        QueueSelectionOptions::ComputeOnly,
        false,
        false,
        false,
    });
    addTestGroup(group, "image_to_buffer_compute_queue", addCopyImageToBufferTests, computeOnlyGroup);
    addTestGroup(group, "buffer_to_image_compute_queue", addCopyBufferToImageTests, computeOnlyGroup);

    if (extensionFlags == COPY_COMMANDS_2)
    {
        addTestGroup(group, "image_to_image_transfer_queue", addCopyImageToImageTests, transferOnlyGroup);

        TestGroupParamsPtr transferWithSecondaryBuffer(new TestGroupParams{
            allocationKind,
            extensionFlags,
            QueueSelectionOptions::TransferOnly,
            true,
            false,
            false,
        });
        addTestGroup(group, "image_to_image_transfer_queue_secondary", addCopyImageToImageTestsSimpleOnly,
                     transferWithSecondaryBuffer);

        TestGroupParamsPtr transferWithSparse(new TestGroupParams{
            allocationKind,
            extensionFlags | SPARSE_BINDING,
            QueueSelectionOptions::TransferOnly,
            false,
            true,
            false,
        });
        addTestGroup(group, "image_to_image_transfer_sparse", addCopyImageToImageTestsSimpleOnly, transferWithSparse);
    }

    if (allocationKind == ALLOCATION_KIND_SUBALLOCATED && extensionFlags == 0)
    {
        TestGroupParamsPtr generalLayoutGroupParams(new TestGroupParams{
            allocationKind,
            extensionFlags,
            QueueSelectionOptions::Universal,
            false,
            false,
            true,
        });
        addTestGroup(group, "image_to_image_general_layout", addCopyImageToImageTestsSimpleOnly,
                     generalLayoutGroupParams);
        addTestGroup(group, "image_to_buffer_general_layout", addCopyImageToBufferTests, generalLayoutGroupParams);
        addTestGroup(group, "buffer_to_image_general_layout", addCopyBufferToImageTests, generalLayoutGroupParams);
    }
}

void addCoreCopiesAndBlittingTests(tcu::TestCaseGroup *group)
{
    uint32_t extensionFlags = 0;
    addCopiesAndBlittingTests(group, ALLOCATION_KIND_SUBALLOCATED, extensionFlags);
    addIndirectCopyTests(group, ALLOCATION_KIND_SUBALLOCATED, INDIRECT_COPY);
    addCopyBufferToBufferOffsetTests(group);
}

void addDedicatedAllocationCopiesAndBlittingTests(tcu::TestCaseGroup *group)
{
    uint32_t extensionFlags = 0;
    addCopiesAndBlittingTests(group, ALLOCATION_KIND_DEDICATED, extensionFlags);
    addIndirectCopyTests(group, ALLOCATION_KIND_DEDICATED, INDIRECT_COPY);
}

static void cleanupGroup(tcu::TestCaseGroup *)
{
}

} // namespace

tcu::TestCaseGroup *createCopiesAndBlittingTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> copiesAndBlittingTests(new tcu::TestCaseGroup(testCtx, "copy_and_blit"));

    copiesAndBlittingTests->addChild(createTestGroup(testCtx, "core", addCoreCopiesAndBlittingTests, cleanupGroup));
    copiesAndBlittingTests->addChild(
        createTestGroup(testCtx, "dedicated_allocation", addDedicatedAllocationCopiesAndBlittingTests, cleanupGroup));
    copiesAndBlittingTests->addChild(createTestGroup(
        testCtx, "copy_commands2",
        [](tcu::TestCaseGroup *group) { addCopiesAndBlittingTests(group, ALLOCATION_KIND_DEDICATED, COPY_COMMANDS_2); },
        cleanupGroup));
    copiesAndBlittingTests->addChild(createTestGroup(
        testCtx, "sparse",
        [](tcu::TestCaseGroup *group)
        { addSparseCopyTests(group, ALLOCATION_KIND_DEDICATED, COPY_COMMANDS_2 | SPARSE_BINDING); },
        cleanupGroup));
    copiesAndBlittingTests->addChild(createCopyMultiplaneImageTransferQueueTests(testCtx));

#ifndef CTS_USES_VULKANSC
    copiesAndBlittingTests->addChild(createDynamicStateMetaOperationsTests(testCtx));
    copiesAndBlittingTests->addChild(createCopyMemoryIndirectTests(testCtx));
#endif
    copiesAndBlittingTests->addChild(createReinterpretationTests(testCtx));

    return copiesAndBlittingTests.release();
}

} // namespace api
} // namespace vkt
