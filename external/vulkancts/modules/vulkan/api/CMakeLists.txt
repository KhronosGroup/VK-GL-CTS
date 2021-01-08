# API layer tests

include_directories(
	..
	../util
	)

set(DEQP_VK_API_SRCS
	vktApiVersionCheck.cpp
	vktApiVersionCheck.hpp
	vktApiTests.cpp
	vktApiTests.hpp
	vktApiSmokeTests.cpp
	vktApiSmokeTests.hpp
	vktApiDeviceDrmPropertiesTests.cpp
	vktApiDeviceDrmPropertiesTests.hpp
	vktApiDeviceInitializationTests.cpp
	vktApiDeviceInitializationTests.hpp
	vktApiDriverPropertiesTests.cpp
	vktApiDriverPropertiesTests.hpp
	vktApiObjectManagementTests.cpp
	vktApiObjectManagementTests.hpp
	vktApiBufferTests.cpp
	vktApiBufferTests.hpp
	vktApiBufferMarkerTests.cpp
	vktApiBufferMarkerTests.hpp
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
	vktApiExternalMemoryTests.cpp
	vktApiExternalMemoryTests.hpp
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
    vktApiToolingInfoTests.cpp
    vktApiToolingInfoTests.hpp
	)

set(DEQP_VK_API_LIBS
	deqp-vk-util
	tcutil
	vkutil
	)

PCH(DEQP_VK_API_SRCS ../pch.cpp)

add_library(deqp-vk-api STATIC ${DEQP_VK_API_SRCS})
target_link_libraries(deqp-vk-api ${DEQP_VK_API_LIBS})
