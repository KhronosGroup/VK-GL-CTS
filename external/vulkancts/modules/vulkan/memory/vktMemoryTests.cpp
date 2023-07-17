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
 * \brief Memory Tests
 *//*--------------------------------------------------------------------*/

#include "vktMemoryTests.hpp"

#include "vktMemoryAllocationTests.hpp"
#include "vktMemoryPipelineBarrierTests.hpp"
#include "vktMemoryRequirementsTests.hpp"
#include "vktMemoryBindingTests.hpp"
#include "vktMemoryExternalMemoryHostTests.hpp"
#include "vktTestGroupUtil.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktMemoryMappingTests.hpp"
#include "vktMemoryAddressBindingTests.hpp"
#include "vktMemoryDeviceMemoryReportTests.hpp"
#endif // CTS_USES_VULKANSC

namespace vkt
{
namespace memory
{

namespace
{

void createChildren (tcu::TestCaseGroup* memoryTests)
{
	tcu::TestContext&	testCtx		= memoryTests->getTestContext();

#ifndef CTS_USES_VULKANSC
	// In Vulkan SC subsequent tests allocate memory but do not make it free, because vkFreeMemory was removed.
	// As a consequence - random memory allocation tests start to report ResourceError ( VK_ERROR_OUT_OF_*_MEMORY )
	memoryTests->addChild(createAllocationTests					(testCtx));
	memoryTests->addChild(createDeviceGroupAllocationTests		(testCtx));
	memoryTests->addChild(createPageableAllocationTests			(testCtx));
	memoryTests->addChild(createMappingTests					(testCtx));
	memoryTests->addChild(createPipelineBarrierTests			(testCtx));
#endif // CTS_USES_VULKANSC
	memoryTests->addChild(createRequirementsTests				(testCtx));
	memoryTests->addChild(createMemoryBindingTests				(testCtx));
	memoryTests->addChild(createMemoryExternalMemoryHostTests	(testCtx));
#ifndef CTS_USES_VULKANSC
	memoryTests->addChild(createDeviceMemoryReportTests			(testCtx));
	memoryTests->addChild(createAddressBindingReportTests		(testCtx));
#endif
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name, "Memory Tests", createChildren);
}

} // memory
} // vkt
