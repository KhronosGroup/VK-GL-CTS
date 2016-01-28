include_directories(..)

set(DEQP_VK_SHADEREXECUTOR_SRCS
   vktShaderExecutor.cpp
   vktShaderExecutor.hpp
   vktShaderBuiltinTests.cpp
   vktShaderBuiltinTests.hpp
   vktShaderCommonFunctionTests.cpp
   vktShaderCommonFunctionTests.hpp
   vktShaderIntegerFunctionTests.cpp
   vktShaderIntegerFunctionTests.hpp
   vktShaderPackingFunctionTests.cpp
   vktShaderPackingFunctionTests.hpp
   vktShaderBuiltinPrecisionTests.cpp
   vktShaderBuiltinPrecisionTests.hpp
   vktOpaqueTypeIndexingTests.cpp
   vktOpaqueTypeIndexingTests.hpp
)

set(DEQP_VK_SHADEREXECUTOR_LIBS
   deqp-vk-common
   tcutil
   vkutil
)

add_library(deqp-vk-shaderexecutor STATIC ${DEQP_VK_SHADEREXECUTOR_SRCS})
target_link_libraries(deqp-vk-shaderexecutor ${DEQP_VK_SHADEREXECUTOR_LIBS})
