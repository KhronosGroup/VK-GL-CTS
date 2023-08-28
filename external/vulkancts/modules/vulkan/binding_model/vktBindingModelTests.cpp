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
 * \brief Binding Model tests
 *//*--------------------------------------------------------------------*/

#include "vktBindingModelTests.hpp"

#include "vktBindingShaderAccessTests.hpp"
#include "vktBindingDescriptorUpdateTests.hpp"
#include "vktBindingDescriptorSetRandomTests.hpp"
#include "vktBindingDescriptorCopyTests.hpp"
#include "vktBindingBufferDeviceAddressTests.hpp"
#include "vktTestGroupUtil.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktBindingDynamicOffsetTests.hpp"
#include "vktBindingMutableTests.hpp"
#include "vktBindingDescriptorBufferTests.hpp"
#endif // CTS_USES_VULKANSC

namespace vkt
{
namespace BindingModel
{

namespace
{

void createChildren (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx		= group->getTestContext();

	group->addChild(createShaderAccessTests(testCtx));
	group->addChild(createDescriptorUpdateTests(testCtx));
	group->addChild(createDescriptorSetRandomTests(testCtx));
	group->addChild(createDescriptorCopyTests(testCtx));
	group->addChild(createBufferDeviceAddressTests(testCtx));
#ifndef CTS_USES_VULKANSC
	group->addChild(createDynamicOffsetTests(testCtx));
	group->addChild(createDescriptorMutableTests(testCtx));
	group->addChild(createDescriptorBufferTests(testCtx));
#endif

	// \todo [2015-07-30 jarkko] .change_binding.{between_renderpasses, within_pass}
	// \todo [2015-07-30 jarkko] .descriptor_set_chain
	// \todo [2015-07-30 jarkko] .update_descriptor_set
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name, "Resource binding tests", createChildren);
}

} // BindingModel
} // vkt
