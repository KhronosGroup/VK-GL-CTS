include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_IMAGE_PROCESSING_SRCS
	vktImageProcessingTests.cpp
	vktImageProcessingTests.hpp
	vktImageProcessingBase.cpp
	vktImageProcessingBase.hpp
	vktImageProcessingBlockMatchingTests.cpp
	vktImageProcessingBlockMatchingTests.hpp
	vktImageProcessingTestsUtil.cpp
	vktImageProcessingTestsUtil.hpp
	vktImageProcessingApiTests.cpp
	vktImageProcessingApiTests.hpp
	)

PCH(DEQP_VK_IMAGE_PROCESSING_SRCS ../pch.cpp)

add_library(deqp-vk-image-processing STATIC ${DEQP_VK_IMAGE_PROCESSING_SRCS})
target_link_libraries(deqp-vk-image-processing tcutil vkutil)

