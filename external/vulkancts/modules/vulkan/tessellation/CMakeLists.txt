include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_TESSELLATION_SRCS
	vktTessellationTests.cpp
	vktTessellationTests.hpp
	vktTessellationUtil.cpp
	vktTessellationUtil.hpp
	vktTessellationLimitsTests.hpp
	vktTessellationLimitsTests.cpp
	vktTessellationCoordinatesTests.hpp
	vktTessellationCoordinatesTests.cpp
	vktTessellationWindingTests.hpp
	vktTessellationWindingTests.cpp
	vktTessellationShaderInputOutputTests.hpp
	vktTessellationShaderInputOutputTests.cpp
	vktTessellationMiscDrawTests.hpp
	vktTessellationMiscDrawTests.cpp
	vktTessellationCommonEdgeTests.hpp
	vktTessellationCommonEdgeTests.cpp
	vktTessellationFractionalSpacingTests.hpp
	vktTessellationFractionalSpacingTests.cpp
	vktTessellationPrimitiveDiscardTests.hpp
	vktTessellationPrimitiveDiscardTests.cpp
	vktTessellationInvarianceTests.hpp
	vktTessellationInvarianceTests.cpp
	vktTessellationUserDefinedIO.hpp
	vktTessellationUserDefinedIO.cpp
	vktTessellationGeometryPassthroughTests.hpp
	vktTessellationGeometryPassthroughTests.cpp
	vktTessellationGeometryPointSizeTests.hpp
	vktTessellationGeometryPointSizeTests.cpp
	vktTessellationGeometryGridRenderTests.hpp
	vktTessellationGeometryGridRenderTests.cpp
	)

PCH(DEQP_VK_TESSELLATION_SRCS ../pch.cpp)

add_library(deqp-vk-tessellation STATIC ${DEQP_VK_TESSELLATION_SRCS})
target_link_libraries(deqp-vk-tessellation tcutil vkutil)

add_library(deqp-vksc-tessellation STATIC ${DEQP_VK_TESSELLATION_SRCS})
target_link_libraries(deqp-vksc-tessellation PUBLIC deqp-vksc-util tcutil vkscutil)
