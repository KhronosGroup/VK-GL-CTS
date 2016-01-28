include_directories(..)

set(DEQP_VK_QUERY_POOL_SRCS
	vktQueryPoolTests.hpp
	vktQueryPoolTests.cpp
	vktQueryPoolOcclusionTests.hpp
	vktQueryPoolOcclusionTests.cpp
	vktQueryPoolCreateInfoUtil.hpp
	vktQueryPoolCreateInfoUtil.cpp
	vktQueryPoolBufferObjectUtil.hpp
	vktQueryPoolBufferObjectUtil.cpp
	vktQueryPoolImageObjectUtil.hpp
	vktQueryPoolImageObjectUtil.cpp
)

set(DEQP_VK_QUERY_POOL_LIBS
	deqp-vk-common
	tcutil
	vkutil
)

add_library(deqp-vk-query-pool STATIC ${DEQP_VK_QUERY_POOL_SRCS})
target_link_libraries(deqp-vk-query-pool ${DEQP_VK_QUERY_POOL_LIBS})

