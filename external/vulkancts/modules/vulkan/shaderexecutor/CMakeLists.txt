include_directories(..)

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

set(DEQP_VK_SHADEREXECUTOR_LIBS
	tcutil
	vkutil
)

PCH(DEQP_VK_SHADEREXECUTOR_SRCS ../pch.cpp)

add_library(deqp-vk-shaderexecutor STATIC ${DEQP_VK_SHADEREXECUTOR_SRCS})
target_link_libraries(deqp-vk-shaderexecutor ${DEQP_VK_SHADEREXECUTOR_LIBS})
