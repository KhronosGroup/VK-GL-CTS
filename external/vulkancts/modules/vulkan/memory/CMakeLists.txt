# Memory tests

include_directories(..)

set(DEQP_VK_MEMORY_SRCS
	vktMemoryTests.cpp
	vktMemoryTests.hpp
	vktMemoryAllocationTests.cpp
	vktMemoryAllocationTests.hpp
	vktMemoryMappingTests.cpp
	vktMemoryMappingTests.hpp
	vktMemoryPipelineBarrierTests.hpp
	vktMemoryPipelineBarrierTests.cpp
	)

set(DEQP_VK_MEMORY_LIBS
	deqp-vk-common
	tcutil
	vkutil
	)

add_library(deqp-vk-memory STATIC ${DEQP_VK_MEMORY_SRCS})
target_link_libraries(deqp-vk-memory ${DEQP_VK_MEMORY_LIBS})
