# vksc-pipeline-compiler

set(VKSC_PIPELINE_COMPILER_SRCS
	vkscpc.cpp
	)

add_executable(vksc-pipeline-compiler ${VKSC_PIPELINE_COMPILER_SRCS})
target_include_directories(vksc-pipeline-compiler PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/modules/vulkan)
target_link_libraries(vksc-pipeline-compiler decpp tcutil tcutil-sc-platform vkscutil deqp-vksc-package jsoncpp_static)
