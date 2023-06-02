# API layer tests

include_directories(
	..
	../util
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_API_SRCS
	vktApiVersionCheck.cpp
	vktApiVersionCheck.hpp
	vktApiTests.cpp
	vktApiTests.hpp
	vktApiDeviceInitializationTests.cpp
	vktApiDeviceInitializationTests.hpp
	vktApiDriverPropertiesTests.cpp
	vktApiDriverPropertiesTests.hpp
	vktApiObjectManagementTests.cpp
	vktApiObjectManagementTests.hpp
	vktApiBufferTests.cpp
	vktApiBufferTests.hpp
	vktApiBufferViewCreateTests.cpp
	vktApiBufferViewCreateTests.hpp
	vktApiBufferViewAccessTests.cpp
	vktApiBufferViewAccessTests.hpp
	vktApiFeatureInfo.cpp
	vktApiFeatureInfo.hpp
	vktApiCommandBuffersTests.cpp
	vktApiCommandBuffersTests.hpp
	vktApiComputeInstanceResultBuffer.cpp
	vktApiComputeInstanceResultBuffer.hpp
	vktApiBufferComputeInstance.cpp
	vktApiBufferComputeInstance.hpp
	vktApiCopiesAndBlittingTests.cpp
	vktApiCopiesAndBlittingTests.hpp
	vktApiImageClearingTests.cpp
	vktApiImageClearingTests.hpp
	vktApiFillBufferTests.cpp
	vktApiFillBufferTests.hpp
	vktApiDescriptorPoolTests.cpp
	vktApiDescriptorPoolTests.hpp
	vktApiNullHandleTests.cpp
	vktApiNullHandleTests.hpp
	vktApiGranularityTests.cpp
	vktApiGranularityTests.hpp
	vktApiGetMemoryCommitment.cpp
	vktApiGetMemoryCommitment.hpp
	vktApiBufferAndImageAllocationUtil.hpp
	vktApiBufferAndImageAllocationUtil.cpp
	vktApiMaintenance3Check.hpp
	vktApiMaintenance3Check.cpp
	vktApiDescriptorSetTests.hpp
	vktApiDescriptorSetTests.cpp
	vktApiPipelineTests.hpp
	vktApiPipelineTests.cpp
	vktApiMemoryRequirementInvarianceTests.cpp
	vktApiMemoryRequirementInvarianceTests.hpp
	vktApiBufferMemoryRequirementsTests.cpp
	vktApiBufferMemoryRequirementsTests.hpp
	vktApiBufferMemoryRequirementsTestsUtils.hpp
	vktApiExtensionDuplicatesTests.cpp
	vktApiExtensionDuplicatesTests.hpp
	)

set(DEQP_VK_API_SRCS
	vktApiSmokeTests.cpp
	vktApiSmokeTests.hpp
	vktApiBufferMarkerTests.cpp
	vktApiBufferMarkerTests.hpp
	vktApiDeviceDrmPropertiesTests.cpp
	vktApiDeviceDrmPropertiesTests.hpp
	vktApiExternalMemoryTests.cpp
	vktApiExternalMemoryTests.hpp
	vktApiToolingInfoTests.cpp
	vktApiToolingInfoTests.hpp
	vktApiFormatPropertiesExtendedKHRtests.cpp
	vktApiFormatPropertiesExtendedKHRtests.hpp
	vktApiImageCompressionControlTests.cpp
	vktApiImageCompressionControlTests.hpp
	vktApiGetDeviceProcAddrTests.hpp
	vktApiGetDeviceProcAddrTests.cpp
	vktApiPhysicalDeviceFormatPropertiesMaint5Tests.cpp
	vktApiPhysicalDeviceFormatPropertiesMaint5Tests.hpp
	vktApiFrameBoundaryTests.cpp
	vktApiFrameBoundaryTests.hpp
	vktApiExtensionDuplicatesTests.cpp
	vktApiExtensionDuplicatesTests.hpp
	)

PCH(DEQP_VK_API_SRCS ../pch.cpp)

add_library(deqp-vk-api STATIC ${DEQP_VK_VKSC_API_SRCS} ${DEQP_VK_API_SRCS})
target_link_libraries(deqp-vk-api PUBLIC deqp-vk-util tcutil vkutil)

add_library(deqp-vksc-api STATIC ${DEQP_VK_VKSC_API_SRCS})
# HACK: For some reason vkApiExtensionDependencyInfo.inl is only included in the Vulkan generated files, but it used for the SC
# version of the test. That means we need to add generated/vulkan but have it appear after generated/vulkansc (so that files that
# do exist still come from the right place). Bodge that in here.
target_include_directories(deqp-vksc-api BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkansc)
target_include_directories(deqp-vksc-api AFTER PRIVATE ${PROJECT_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkan)
target_link_libraries(deqp-vksc-api PUBLIC deqp-vksc-util tcutil vkscutil)
