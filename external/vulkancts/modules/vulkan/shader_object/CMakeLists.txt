include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_SHADER_OBJECT_SRCS
	vktShaderObjectTests.cpp
	vktShaderObjectTests.hpp
	vktShaderObjectApiTests.cpp
	vktShaderObjectApiTests.hpp
	vktShaderObjectCreateTests.cpp
	vktShaderObjectCreateTests.hpp
	vktShaderObjectLinkTests.cpp
	vktShaderObjectLinkTests.hpp
	vktShaderObjectCreateUtil.cpp
	vktShaderObjectCreateUtil.hpp
	vktShaderObjectBinaryTests.cpp
	vktShaderObjectBinaryTests.hpp
	vktShaderObjectPipelineInteractionTests.cpp
	vktShaderObjectPipelineInteractionTests.hpp
	vktShaderObjectBindingTests.cpp
	vktShaderObjectBindingTests.hpp
	vktShaderObjectPerformanceTests.cpp
	vktShaderObjectPerformanceTests.hpp
	vktShaderObjectRenderingTests.cpp
	vktShaderObjectRenderingTests.hpp
	vktShaderObjectMiscTests.cpp
	vktShaderObjectMiscTests.hpp
)

PCH(DEQP_VK_SHADER_OBJECT_SRCS ../pch.cpp)

add_library(deqp-vk-shader-object STATIC ${DEQP_VK_SHADER_OBJECT_SRCS})
target_link_libraries(deqp-vk-shader-object tcutil vkutil)
