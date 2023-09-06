include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_DESCRIPTOR_INDEXING_SRCS
	vktDescriptorIndexingTests.hpp
	vktDescriptorIndexingTests.cpp
	vktDescriptorSetsIndexingTests.cpp
	vktDescriptorSetsIndexingTests.hpp
	vktDescriptorSetsIndexingTestsUtils.cpp
)

add_library(deqp-vk-descriptor-indexing STATIC ${DEQP_VK_DESCRIPTOR_INDEXING_SRCS})
target_link_libraries(deqp-vk-descriptor-indexing tcutil vkutil)

add_library(deqp-vksc-descriptor-indexing STATIC ${DEQP_VK_DESCRIPTOR_INDEXING_SRCS})
target_link_libraries(deqp-vksc-descriptor-indexing PUBLIC deqp-vksc-util tcutil vkscutil)
