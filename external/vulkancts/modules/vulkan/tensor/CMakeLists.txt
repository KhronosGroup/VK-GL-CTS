# Tensor tests

include_directories(
	..
	../util
	${DEQP_INL_DIR}
	)

set(DEQP_VK_TENSOR_SRCS
	vktTensorTests.cpp
	vktTensorTestsUtil.cpp
	vktTensorArrayAccess.cpp
	vktTensorCreateRequirements.cpp
	vktTensorDimensionQuery.cpp
	vktTensorCopies.cpp
	vktTensorGraphicsPipeline.cpp
	vktTensorBasicShaderAccess.cpp
	vktTensorBool.cpp
	shaders/vktTensorQueryDimensionsShaders.cpp
	shaders/vktTensorAccessShaders.cpp
	shaders/vktTensorArrayAccessShaders.cpp
	shaders/vktTensorShaderUtil.cpp
	shaders/vktTensorBooleanShader.cpp
	)

PCH(DEQP_VK_TENSOR_SRCS ../pch.cpp)

add_library(deqp-vk-tensor STATIC ${DEQP_VK_TENSOR_SRCS})
target_include_directories(deqp-vk-tensor BEFORE PUBLIC)
target_link_libraries(deqp-vk-tensor PUBLIC tcutil vkutil)
