include_directories(..)

set(DEQP_VK_SHADEREXECUTOR_SRCS
   vktShaderExecutor.cpp
   vktShaderExecutor.hpp
   vktShaderExecutorTests.cpp
   vktShaderExecutorTests.hpp

   vktShaderCommonFunctionTests.cpp
   vktShaderCommonFunctionTests.hpp
   vktShaderIntegerFunctionTests.cpp
   vktShaderIntegerFunctionTests.hpp
   vktShaderPackingFunctionTests.cpp
   vktShaderPackingFunctionTests.hpp
)

set(DEQP_VK_SHADEREXECUTOR_LIBS
   tcutil
   vkutil
)

add_library(deqp-vk-shaderexecutor STATIC ${DEQP_VK_SHADEREXECUTOR_SRCS})
target_link_libraries(deqp-vk-shaderexecutor ${DEQP_VK_SHADEREXECUTOR_LIBS})
