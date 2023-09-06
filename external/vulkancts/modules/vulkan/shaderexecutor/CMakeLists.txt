include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_SHADEREXECUTOR_SRCS
	vktShaderExecutor.cpp
	vktShaderExecutor.hpp
	vktShaderBuiltinTests.cpp
	vktShaderBuiltinTests.hpp
	vktShaderCommonFunctionTests.cpp
	vktShaderCommonFunctionTests.hpp
	vktShaderFConvertTests.cpp
	vktShaderFConvertTests.hpp
	vktShaderIntegerFunctionTests.cpp
	vktShaderIntegerFunctionTests.hpp
	vktShaderPackingFunctionTests.cpp
	vktShaderPackingFunctionTests.hpp
	vktShaderBuiltinPrecisionTests.cpp
	vktShaderBuiltinPrecisionTests.hpp
	vktOpaqueTypeIndexingTests.cpp
	vktOpaqueTypeIndexingTests.hpp
	vktAtomicOperationTests.cpp
	vktAtomicOperationTests.hpp
	vktShaderClockTests.cpp
	vktShaderClockTests.hpp
)

PCH(DEQP_VK_SHADEREXECUTOR_SRCS ../pch.cpp)

add_library(deqp-vk-shaderexecutor STATIC ${DEQP_VK_SHADEREXECUTOR_SRCS})
target_link_libraries(deqp-vk-shaderexecutor tcutil vkutil)

add_library(deqp-vksc-shaderexecutor STATIC ${DEQP_VK_SHADEREXECUTOR_SRCS})
target_link_libraries(deqp-vksc-shaderexecutor PUBLIC deqp-vksc-util tcutil vkscutil)
