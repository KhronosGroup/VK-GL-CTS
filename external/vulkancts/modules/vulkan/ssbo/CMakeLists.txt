include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_SSBO_SRCS
	vktSSBOLayoutCase.cpp
	vktSSBOLayoutCase.hpp
	vktSSBOLayoutTests.cpp
	vktSSBOLayoutTests.hpp
	vktSSBOCornerCase.cpp
	vktSSBOCornerCase.hpp
	../util/vktTypeComparisonUtil.cpp
	../util/vktTypeComparisonUtil.hpp
)

set(DEQP_VK_SSBO_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-ssbo STATIC ${DEQP_VK_SSBO_SRCS})
target_link_libraries(deqp-vk-ssbo ${DEQP_VK_SSBO_LIBS})
