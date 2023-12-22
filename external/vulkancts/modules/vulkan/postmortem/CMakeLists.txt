include_directories(
	..
	${DEQP_INL_DIR}
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
	vktPostmortemDeviceFaultTests.cpp
	vktPostmortemDeviceFaultTests.hpp
	vktPostmortemDeviceLossTests.cpp
	vktPostmortemDeviceLossTests.hpp
)

set(DEQP_VK_POSTMORTEM_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-postmortem STATIC ${DEQP_VK_POSTMORTEM_SRCS})
target_link_libraries(deqp-vk-postmortem ${DEQP_VK_POSTMORTEM_LIBS})
