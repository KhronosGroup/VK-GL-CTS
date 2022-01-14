include_directories(
	..
	../draw
	${DEQP_INL_DIR}
	)

set(DEQP_VK_QUERY_POOL_SRCS
	vktQueryPoolTests.hpp
	vktQueryPoolTests.cpp
	vktQueryPoolOcclusionTests.hpp
	vktQueryPoolOcclusionTests.cpp
	vktQueryPoolPerformanceTests.hpp
	vktQueryPoolPerformanceTests.cpp
	vktQueryPoolStatisticsTests.hpp
	vktQueryPoolStatisticsTests.cpp
	vktQueryPoolConcurrentTests.hpp
	vktQueryPoolConcurrentTests.cpp
)

set(DEQP_VK_QUERY_POOL_LIBS
	deqp-vk-draw
	tcutil
	vkutil
)

PCH(DEQP_VK_QUERY_POOL_SRCS ../pch.cpp)

add_library(deqp-vk-query-pool STATIC ${DEQP_VK_QUERY_POOL_SRCS})
target_link_libraries(deqp-vk-query-pool ${DEQP_VK_QUERY_POOL_LIBS})
