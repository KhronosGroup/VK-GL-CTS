include_directories(
	..
	../draw
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_QUERY_POOL_SRCS
	vktQueryPoolTests.hpp
	vktQueryPoolTests.cpp
	vktQueryPoolOcclusionTests.hpp
	vktQueryPoolOcclusionTests.cpp
	vktQueryPoolStatisticsTests.hpp
	vktQueryPoolStatisticsTests.cpp
	vktQueryPoolConcurrentTests.hpp
	vktQueryPoolConcurrentTests.cpp
	vktQueryPoolFragInvocationTests.hpp
	vktQueryPoolFragInvocationTests.cpp
)

set(DEQP_VK_QUERY_POOL_SRCS
	vktQueryPoolPerformanceTests.hpp
	vktQueryPoolPerformanceTests.cpp
	)

PCH(DEQP_VK_QUERY_POOL_SRCS ../pch.cpp)

add_library(deqp-vk-query-pool STATIC ${DEQP_VK_VKSC_QUERY_POOL_SRCS} ${DEQP_VK_QUERY_POOL_SRCS})
target_link_libraries(deqp-vk-query-pool deqp-vk-draw tcutil vkutil)

add_library(deqp-vksc-query-pool STATIC ${DEQP_VK_VKSC_QUERY_POOL_SRCS})
target_link_libraries(deqp-vksc-query-pool PUBLIC deqp-vksc-draw deqp-vksc-util tcutil vkscutil)
