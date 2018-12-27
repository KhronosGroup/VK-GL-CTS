include_directories(..)

set(DEQP_VK_DESCRIPTOR_INDEXING_SRCS
	vktDescriptorIndexingTests.hpp
	vktDescriptorIndexingTests.cpp
	vktDescriptorSetsIndexingTests.cpp
	vktDescriptorSetsIndexingTests.hpp
	vktDescriptorSetsIndexingTestsUtils.cpp
)

set(DEQP_VK_DESCRIPTOR_INDEXING_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-descriptor-indexing STATIC ${DEQP_VK_DESCRIPTOR_INDEXING_SRCS})
target_link_libraries(deqp-vk-descriptor-indexing ${DEQP_VK_DESCRIPTOR_INDEXING_LIBS})
