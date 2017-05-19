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

namespace vkt
{
namespace ProtectedMem
{

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> protectedTests (new tcu::TestCaseGroup(testCtx, "protected_memory", "Protected Memory Tests"));

	// Attachment test case group
	{
		de::MovePtr<tcu::TestCaseGroup> attachmentTestGroup (new tcu::TestCaseGroup(testCtx, "attachment", "Protected Memory Attachment Tests"));
		attachmentTestGroup->addChild(createAttachmentLoadTests(testCtx));
		protectedTests->addChild(attachmentTestGroup.release());
	}

	return protectedTests.release();

}

} // ProtectedMem
} // vkt
