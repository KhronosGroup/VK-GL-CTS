# Memory tests

include_directories(
	..
	../util
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_MEMORY_SRCS
	vktMemoryTests.cpp
	vktMemoryTests.hpp
	vktMemoryAllocationTests.cpp
	vktMemoryAllocationTests.hpp
	vktMemoryPipelineBarrierTests.hpp
	vktMemoryPipelineBarrierTests.cpp
	vktMemoryRequirementsTests.cpp
	vktMemoryRequirementsTests.hpp
	vktMemoryBindingTests.cpp
	vktMemoryBindingTests.hpp
	vktMemoryExternalMemoryHostTests.cpp
	vktMemoryExternalMemoryHostTests.hpp
	)

set(DEQP_VK_MEMORY_SRCS
	vktMemoryMappingTests.cpp
	vktMemoryMappingTests.hpp
	vktMemoryAddressBindingTests.cpp
	vktMemoryAddressBindingTests.hpp
	vktMemoryDeviceMemoryReportTests.cpp
	vktMemoryDeviceMemoryReportTests.hpp
	)

PCH(DEQP_VK_MEMORY_SRCS ../pch.cpp)

add_library(deqp-vk-memory STATIC ${DEQP_VK_VKSC_MEMORY_SRCS} ${DEQP_VK_MEMORY_SRCS})
target_link_libraries(deqp-vk-memory tcutil vkutil)

add_library(deqp-vksc-memory STATIC ${DEQP_VK_VKSC_MEMORY_SRCS})
target_link_libraries(deqp-vksc-memory PUBLIC deqp-vksc-util tcutil vkscutil)
