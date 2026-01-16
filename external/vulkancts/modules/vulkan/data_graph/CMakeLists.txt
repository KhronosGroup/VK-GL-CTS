# DataGraph tests

include_directories(
	..
	../util
	./tosa
	${DEQP_INL_DIR}
	)

set(DEQP_VK_DATA_GRAPH_SRCS
	tosa/vktDataGraphTosaSpirv.cpp
	vktDataGraphBasicTests.cpp
	vktDataGraphPipelineCacheTests.cpp
	vktDataGraphPropertiesTests.cpp
	vktDataGraphTestProvider.cpp
	vktDataGraphTests.cpp
	vktDataGraphTestUtil.cpp
	)

PCH(DEQP_VK_DATA_GRAPH_SRCS ../pch.cpp)

add_library(deqp-vk-data-graph STATIC ${DEQP_VK_DATA_GRAPH_SRCS})
target_include_directories(deqp-vk-data-graph BEFORE PUBLIC)
target_link_libraries(deqp-vk-data-graph PUBLIC tcutil vkutil)
