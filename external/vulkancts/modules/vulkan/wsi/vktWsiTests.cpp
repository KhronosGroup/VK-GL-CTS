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
	addTestGroup(testGroup, "surface",					"VkSurface Tests",									createSurfaceTests,					wsiType);
	addTestGroup(testGroup, "swapchain",				"VkSwapchain Tests",								createSwapchainTests,				wsiType);
	addTestGroup(testGroup, "incremental_present",		"Incremental present tests",						createIncrementalPresentTests,		wsiType);
	addTestGroup(testGroup, "display_timing",			"Display Timing Tests",								createDisplayTimingTests,			wsiType);
	addTestGroup(testGroup, "shared_presentable_image",	"VK_KHR_shared_presentable_image tests",			createSharedPresentableImageTests,	wsiType);
	addTestGroup(testGroup, "colorspace",				"ColorSpace tests",									createColorSpaceTests,				wsiType);
	addTestGroup(testGroup, "colorspace_compare",		"ColorSpace compare tests",							createColorspaceCompareTests,		wsiType);
	addTestGroup(testGroup, "full_screen_exclusive",	"VK_EXT_full_screen_exclusive tests",				createFullScreenExclusiveTests,		wsiType);
	addTestGroup(testGroup, "present_id_wait",			"VK_KHR_present_(id|wait) tests",					createPresentIdWaitTests,			wsiType);
	addTestGroup(testGroup, "maintenance1",			    "VK_KHR_(surface|swapchain)_maintenance1 tests",	createMaintenance1Tests,			wsiType);
}

void createWsiTests (tcu::TestCaseGroup* apiTests)
{
	for (int typeNdx = 0; typeNdx < vk::wsi::TYPE_LAST; ++typeNdx)
	{
		const vk::wsi::Type	wsiType	= (vk::wsi::Type)typeNdx;

		addTestGroup(apiTests, getName(wsiType), "", createTypeSpecificTests, wsiType);
	}

	addTestGroup(apiTests, "display", "Display coverage tests", createDisplayCoverageTests);
	addTestGroup(apiTests, "display_control", "Display Control Tests", createDisplayControlTests);
	addTestGroup(apiTests, "acquire_drm_display", "Acquire Drm display tests", createAcquireDrmDisplayTests);
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name.c_str(), "WSI Tests", createWsiTests);
}

} // wsi
} // vkt
