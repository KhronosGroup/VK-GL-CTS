include_directories(
	..
)

set(DEQP_VK_POSTMORTEM_SRCS
	vktPostmortemShaderTimeoutTests.cpp
	vktPostmortemShaderTimeoutTests.hpp
	vktPostmortemTests.hpp
	vktPostmortemTests.cpp
	vktPostmortemUseAfterFreeTests.hpp
	vktPostmortemUseAfterFreeTests.cpp
	vktPostmortemUtil.hpp
	vktPostmortemUtil.cpp
)

set(DEQP_VK_POSTMORTEM_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-postmortem STATIC ${DEQP_VK_POSTMORTEM_SRCS})
target_link_libraries(deqp-vk-postmortem ${DEQP_VK_POSTMORTEM_LIBS})
