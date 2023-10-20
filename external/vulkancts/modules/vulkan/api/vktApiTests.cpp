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
 * \brief API Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiTests.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktApiDeviceInitializationTests.hpp"
#include "vktApiDriverPropertiesTests.hpp"
#include "vktApiObjectManagementTests.hpp"
#include "vktApiBufferTests.hpp"
#include "vktApiBufferViewCreateTests.hpp"
#include "vktApiBufferViewAccessTests.hpp"
#include "vktApiFeatureInfo.hpp"
#include "vktApiCommandBuffersTests.hpp"
#include "vktApiCopiesAndBlittingTests.hpp"
#include "vktApiImageClearingTests.hpp"
#include "vktApiFillBufferTests.hpp"
#include "vktApiDescriptorPoolTests.hpp"
#include "vktApiNullHandleTests.hpp"
#include "vktApiGranularityTests.hpp"
#include "vktApiGetMemoryCommitment.hpp"
#include "vktApiVersionCheck.hpp"
#include "vktApiMaintenance3Check.hpp"
#include "vktApiDescriptorSetTests.hpp"
#include "vktApiPipelineTests.hpp"
#include "vktApiMemoryRequirementInvarianceTests.hpp"
#include "vktApiBufferMemoryRequirementsTests.hpp"
#include "vktApiGetDeviceProcAddrTests.hpp"
#include "vktApiExtensionDuplicatesTests.hpp"

#ifndef CTS_USES_VULKANSC
#include "vktApiSmokeTests.hpp"
#include "vktApiBufferMarkerTests.hpp"
#include "vktApiDeviceDrmPropertiesTests.hpp"
#include "vktApiExternalMemoryTests.hpp"
#include "vktApiToolingInfoTests.hpp"
#include "vktApiFormatPropertiesExtendedKHRtests.hpp"
#include "vktApiImageCompressionControlTests.hpp"
#include "vktApiFrameBoundaryTests.hpp"
#include "vktApiPhysicalDeviceFormatPropertiesMaint5Tests.hpp"
#endif // CTS_USES_VULKANSC

namespace vkt
{
namespace api
{

namespace
{

void createBufferViewTests (tcu::TestCaseGroup* bufferViewTests)
{
	tcu::TestContext&	testCtx		= bufferViewTests->getTestContext();

	bufferViewTests->addChild(createBufferViewCreateTests	(testCtx));
	bufferViewTests->addChild(createBufferViewAccessTests	(testCtx));
}

void createApiTests (tcu::TestCaseGroup* apiTests)
{
	tcu::TestContext&	testCtx		= apiTests->getTestContext();

	apiTests->addChild(createVersionSanityCheckTests			(testCtx));
	apiTests->addChild(createDriverPropertiesTests				(testCtx));
#ifndef CTS_USES_VULKANSC
	apiTests->addChild(createSmokeTests							(testCtx));
#endif // CTS_USES_VULKANSC
	apiTests->addChild(api::createFeatureInfoTests				(testCtx));
#ifndef CTS_USES_VULKANSC
	apiTests->addChild(createDeviceDrmPropertiesTests			(testCtx));
#endif // CTS_USES_VULKANSC
	apiTests->addChild(createDeviceInitializationTests			(testCtx));
	apiTests->addChild(createObjectManagementTests				(testCtx));
	apiTests->addChild(createBufferTests						(testCtx));
#ifndef CTS_USES_VULKANSC
	apiTests->addChild(createBufferMarkerTests					(testCtx));
#endif // CTS_USES_VULKANSC
	apiTests->addChild(createTestGroup							(testCtx, "buffer_view",	"BufferView tests",		createBufferViewTests));
	apiTests->addChild(createCommandBuffersTests				(testCtx));
	apiTests->addChild(createCopiesAndBlittingTests				(testCtx));
	apiTests->addChild(createImageClearingTests					(testCtx));
	apiTests->addChild(createFillAndUpdateBufferTests			(testCtx));
	apiTests->addChild(createDescriptorPoolTests				(testCtx));
	apiTests->addChild(createNullHandleTests					(testCtx));
	apiTests->addChild(createGranularityQueryTests				(testCtx));
	apiTests->addChild(createMemoryCommitmentTests				(testCtx));
#ifndef CTS_USES_VULKANSC
	apiTests->addChild(createExternalMemoryTests				(testCtx));
#endif // CTS_USES_VULKANSC
	apiTests->addChild(createMaintenance3Tests					(testCtx));
	apiTests->addChild(createDescriptorSetTests					(testCtx));
	apiTests->addChild(createPipelineTests						(testCtx));
	apiTests->addChild(createMemoryRequirementInvarianceTests	(testCtx));
#ifndef CTS_USES_VULKANSC
	apiTests->addChild(createToolingInfoTests					(testCtx));
	apiTests->addChild(createFormatPropertiesExtendedKHRTests	(testCtx));
#endif // CTS_USES_VULKANSC
	apiTests->addChild(createBufferMemoryRequirementsTests		(testCtx));
#ifndef CTS_USES_VULKANSC
	apiTests->addChild(createImageCompressionControlTests		(testCtx));
	apiTests->addChild(createGetDeviceProcAddrTests				(testCtx));
	apiTests->addChild(createFrameBoundaryTests					(testCtx));
	apiTests->addChild(createMaintenance5Tests					(testCtx));
#endif
	apiTests->addChild(createExtensionDuplicatesTests			(testCtx));
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name, "API Tests", createApiTests);
}

} // api
} // vkt
