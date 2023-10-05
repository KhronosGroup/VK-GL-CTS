/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief WSI Tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkWsiUtil.hpp"

#include "vktWsiAcquireDrmDisplayTests.hpp"
#include "vktWsiSurfaceTests.hpp"
#include "vktWsiSwapchainTests.hpp"
#include "vktWsiDisplayTests.hpp"
#include "vktWsiIncrementalPresentTests.hpp"
#include "vktWsiDisplayControlTests.hpp"
#include "vktWsiDisplayTimingTests.hpp"
#include "vktWsiSharedPresentableImageTests.hpp"
#include "vktWsiColorSpaceTests.hpp"
#include "vktWsiFullScreenExclusiveTests.hpp"
#include "vktWsiPresentIdWaitTests.hpp"
#include "vktWsiMaintenance1Tests.hpp"

namespace vkt
{
namespace wsi
{

namespace
{

void createTypeSpecificTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	// VkSurface Tests
	addTestGroup(testGroup, "surface", createSurfaceTests,					wsiType);
	// VkSwapchain Tests
	addTestGroup(testGroup, "swapchain", createSwapchainTests,				wsiType);
	// Incremental present tests
	addTestGroup(testGroup, "incremental_present", createIncrementalPresentTests,		wsiType);
	// Display Timing Tests
	addTestGroup(testGroup, "display_timing", createDisplayTimingTests,			wsiType);
	// VK_KHR_shared_presentable_image tests
	addTestGroup(testGroup, "shared_presentable_image", createSharedPresentableImageTests,	wsiType);
	// ColorSpace tests
	addTestGroup(testGroup, "colorspace", createColorSpaceTests,				wsiType);
	// ColorSpace compare tests
	addTestGroup(testGroup, "colorspace_compare", createColorspaceCompareTests,		wsiType);
	// VK_EXT_full_screen_exclusive tests
	addTestGroup(testGroup, "full_screen_exclusive", createFullScreenExclusiveTests,		wsiType);
	// VK_KHR_present_(id|wait) tests
	addTestGroup(testGroup, "present_id_wait", createPresentIdWaitTests,			wsiType);
	// VK_KHR_(surface|swapchain)_maintenance1 tests
	addTestGroup(testGroup, "maintenance1", createMaintenance1Tests,			wsiType);
}

void createWsiTests (tcu::TestCaseGroup* apiTests)
{
	for (int typeNdx = 0; typeNdx < vk::wsi::TYPE_LAST; ++typeNdx)
	{
		const vk::wsi::Type	wsiType	= (vk::wsi::Type)typeNdx;

		addTestGroup(apiTests, getName(wsiType), createTypeSpecificTests, wsiType);
	}

	// Display coverage tests
	addTestGroup(apiTests, "display", createDisplayCoverageTests);
	// Display Control Tests
	addTestGroup(apiTests, "display_control", createDisplayControlTests);
	// Acquire Drm display tests
	addTestGroup(apiTests, "acquire_drm_display", createAcquireDrmDisplayTests);
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name.c_str(), createWsiTests);
}

} // wsi
} // vkt
