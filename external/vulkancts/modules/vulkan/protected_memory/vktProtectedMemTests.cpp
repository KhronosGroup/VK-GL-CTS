/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected memory tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vktProtectedMemAttachmentLoadTests.hpp"
#include "vktProtectedMemAttachmentClearTests.hpp"
#include "vktProtectedMemCopyImageTests.hpp"
#include "vktProtectedMemBlitImageTests.hpp"
#include "vktProtectedMemClearColorImageTests.hpp"
#include "vktProtectedMemFillUpdateCopyBufferTests.hpp"
#include "vktProtectedMemCopyImageToBufferTests.hpp"
#include "vktProtectedMemCopyBufferToImageTests.hpp"
#include "vktProtectedMemStorageBufferTests.hpp"
#include "vktProtectedMemShaderImageAccessTests.hpp"
#include "vktProtectedMemWsiSwapchainTests.hpp"
#include "vktProtectedMemYCbCrConversionTests.hpp"
#include "vktProtectedMemWorkgroupStorageTests.hpp"
#include "vktProtectedMemStackTests.hpp"

namespace vkt
{
namespace ProtectedMem
{

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup> protectedTests (new tcu::TestCaseGroup(testCtx, name.c_str(), "Protected Memory Tests"));

	// Attachment test case group
	{
		de::MovePtr<tcu::TestCaseGroup> attachmentTestGroup (new tcu::TestCaseGroup(testCtx, "attachment", "Protected Memory Attachment Tests"));
		attachmentTestGroup->addChild(createAttachmentLoadTests(testCtx));
		attachmentTestGroup->addChild(createAttachmentClearTests(testCtx));
		protectedTests->addChild(attachmentTestGroup.release());
	}

	// Image test case group
	{
		de::MovePtr<tcu::TestCaseGroup> imageTestGroup (new tcu::TestCaseGroup(testCtx, "image", "Protected Memory Image Tests"));
		imageTestGroup->addChild(createCopyImageTests(testCtx));
		imageTestGroup->addChild(createBlitImageTests(testCtx));
		imageTestGroup->addChild(createClearColorImageTests(testCtx));
		imageTestGroup->addChild(createCopyBufferToImageTests(testCtx));
		imageTestGroup->addChild(createShaderImageAccessTests(testCtx));
		protectedTests->addChild(imageTestGroup.release());
	}

	// Buffer test case group
	{
		de::MovePtr<tcu::TestCaseGroup> bufferTestGroup (new tcu::TestCaseGroup(testCtx, "buffer", "Protected Memory Buffer Tests"));
		bufferTestGroup->addChild(createFillBufferTests(testCtx));
		bufferTestGroup->addChild(createUpdateBufferTests(testCtx));
		bufferTestGroup->addChild(createCopyBufferTests(testCtx));
		bufferTestGroup->addChild(createCopyImageToFloatBufferTests(testCtx));
		protectedTests->addChild(bufferTestGroup.release());
	}

	// Storage buffer test case group
	{
		de::MovePtr<tcu::TestCaseGroup> ssboTestGroup (new tcu::TestCaseGroup(testCtx, "ssbo", "Storage Buffer Tests"));
		ssboTestGroup->addChild(createReadStorageBufferTests(testCtx));
		ssboTestGroup->addChild(createWriteStorageBufferTests(testCtx));
		ssboTestGroup->addChild(createAtomicStorageBufferTests(testCtx));
		protectedTests->addChild(ssboTestGroup.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> interactionTestGroup (new tcu::TestCaseGroup(testCtx, "interaction", "Various tests which interacts with other extensions"));
#ifndef CTS_USES_VULKANSC
		interactionTestGroup->addChild(createSwapchainTests(testCtx));
#endif
		interactionTestGroup->addChild(createYCbCrConversionTests(testCtx));
		protectedTests->addChild(interactionTestGroup.release());
	}

	protectedTests->addChild(createWorkgroupStorageTests(testCtx));
	protectedTests->addChild(createStackTests(testCtx));

	return protectedTests.release();

}

} // ProtectedMem
} // vkt
