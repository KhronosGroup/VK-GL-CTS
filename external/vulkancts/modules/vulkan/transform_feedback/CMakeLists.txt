include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_TRANSFORM_FEEDBACK_SRCS
	vktTransformFeedbackTests.hpp
	vktTransformFeedbackTests.cpp
	vktTransformFeedbackSimpleTests.hpp
	vktTransformFeedbackSimpleTests.cpp
	vktTransformFeedbackFuzzLayoutCase.cpp
	vktTransformFeedbackFuzzLayoutCase.hpp
	vktTransformFeedbackFuzzLayoutTests.cpp
	vktTransformFeedbackFuzzLayoutTests.hpp
	vktTransformFeedbackRandomLayoutCase.cpp
	vktTransformFeedbackRandomLayoutCase.hpp
	vktPrimitivesGeneratedQueryTests.cpp
	vktPrimitivesGeneratedQueryTests.hpp
)

set(DEQP_VK_TRANSFORM_FEEDBACK_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-transform-feedback STATIC ${DEQP_VK_TRANSFORM_FEEDBACK_SRCS})
target_link_libraries(deqp-vk-transform-feedback ${DEQP_VK_TRANSFORM_FEEDBACK_LIBS})
