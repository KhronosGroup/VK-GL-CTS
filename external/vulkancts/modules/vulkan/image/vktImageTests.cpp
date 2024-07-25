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
 * \brief Image Tests
 *//*--------------------------------------------------------------------*/

#include "vktImageTests.hpp"
#include "vktImageLoadStoreTests.hpp"
#include "vktImageMultisampleLoadStoreTests.hpp"
#include "vktImageMutableTests.hpp"
#include "vktImageQualifiersTests.hpp"
#include "vktImageSizeTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktImageAtomicOperationTests.hpp"
#include "vktImageCompressionTranscodingSupport.hpp"
#include "vktImageTranscodingSupportTests.hpp"
#include "vktImageAstcDecodeModeTests.hpp"
#include "vktImageMisalignedCubeTests.hpp"
#include "vktImageSubresourceLayoutTests.hpp"
#include "vktImageMismatchedDimensionalityTests.hpp"
#include "vktImageMismatchedFormatsTests.hpp"
#include "vktImageMismatchedWriteOpTests.hpp"
#include "vktImageSampleDrawnCubeFaceTests.hpp"
#include "vktImageDepthStencilDescriptorTests.hpp"
#include "vktImageSampleCompressedTextureTests.hpp"
#include "vktImageExtendedUsageBitTests.hpp"
#include "vktImageTransfer.hpp"
#include "vktImageDepthStencilSeparateTests.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktImageHostImageCopyTests.hpp"
#endif

namespace vkt
{
namespace image
{

namespace
{

void createChildren(tcu::TestCaseGroup *imageTests)
{
    tcu::TestContext &testCtx = imageTests->getTestContext();

    imageTests->addChild(createImageStoreTests(testCtx));
    imageTests->addChild(createImageLoadStoreTests(testCtx));
    imageTests->addChild(createImageMultisampleLoadStoreTests(testCtx));
    imageTests->addChild(createImageMutableTests(testCtx));
    imageTests->addChild(createSwapchainImageMutableTests(testCtx));
    imageTests->addChild(createImageFormatReinterpretTests(testCtx));
    imageTests->addChild(createImageQualifiersTests(testCtx));
    imageTests->addChild(createImageSizeTests(testCtx));
    imageTests->addChild(createImageAtomicOperationTests(testCtx));
    imageTests->addChild(createImageCompressionTranscodingTests(testCtx));
    imageTests->addChild(createImageTranscodingSupportTests(testCtx));
    imageTests->addChild(createImageExtendOperandsTests(testCtx));
#ifndef CTS_USES_VULKANSC
    imageTests->addChild(createImageNontemporalOperandTests(testCtx));
#endif // CTS_USES_VULKANSC
    imageTests->addChild(createImageAstcDecodeModeTests(testCtx));
    imageTests->addChild(createMisalignedCubeTests(testCtx));
    imageTests->addChild(createImageLoadStoreLodAMDTests(testCtx));
    imageTests->addChild(createImageSubresourceLayoutTests(testCtx));
    imageTests->addChild(createImageMismatchedFormatsTests(testCtx));
    imageTests->addChild(createImageWriteOpTests(testCtx));
    imageTests->addChild(createImageSampleDrawnCubeFaceTests(testCtx));
    imageTests->addChild(createImageDepthStencilDescriptorTests(testCtx));
    imageTests->addChild(createImageSampleDrawnTextureTests(testCtx));
    imageTests->addChild(createImageExtendedUsageBitTests(testCtx));
    imageTests->addChild(createTransferQueueImageTests(testCtx));
#ifndef CTS_USES_VULKANSC
    imageTests->addChild(createImageMismatchedDimensionalityTests(testCtx));
    imageTests->addChild(createImageHostImageCopyTests(testCtx));
#endif // CTS_USES_VULKANSC
    imageTests->addChild(createImageDepthStencilSeparateTests(testCtx));
}

} // namespace

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name.c_str(), createChildren);
}

} // namespace image
} // namespace vkt
