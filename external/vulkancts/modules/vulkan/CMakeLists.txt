# dEQP-VK

add_subdirectory(api)
add_subdirectory(pipeline)
add_subdirectory(binding_model)
add_subdirectory(spirv_assembly)
add_subdirectory(shaderrender)
add_subdirectory(shaderexecutor)
add_subdirectory(memory)
add_subdirectory(ubo)
add_subdirectory(dynamic_state)
add_subdirectory(ssbo)
add_subdirectory(query_pool)
add_subdirectory(draw)
add_subdirectory(compute)
add_subdirectory(image)
add_subdirectory(wsi)
add_subdirectory(sparse_resources)

include_directories(
	api
	pipeline
	binding_model
	spirv_assembly
	shaderrender
	shaderexecutor
	memory
	ubo
	dynamic_state
	ssbo
	query_pool
	draw
	compute
	image
	wsi
	sparse_resources
	)

set(DEQP_VK_COMMON_SRCS
	vktTestCase.cpp
	vktTestCase.hpp
	vktTestCaseUtil.cpp
	vktTestCaseUtil.hpp
	vktTestPackage.cpp
	vktTestPackage.hpp
	vktShaderLibrary.cpp
	vktShaderLibrary.hpp
	vktRenderPassTests.cpp
	vktRenderPassTests.hpp
	vktTestGroupUtil.cpp
	vktTestGroupUtil.hpp
	vktInfoTests.cpp
	vktInfoTests.hpp
	vktSynchronization.cpp
	vktSynchronization.hpp
	)

set(DEQP_VK_COMMON_LIBS
	tcutil
	vkutil
	glutil
	deqp-vk-api
	deqp-vk-pipeline
	deqp-vk-binding-model
	deqp-vk-spirv-assembly
	deqp-vk-shaderrender
	deqp-vk-shaderexecutor
	deqp-vk-memory
	deqp-vk-ubo
	deqp-vk-dynamic-state
	deqp-vk-ssbo
	deqp-vk-query-pool
	deqp-vk-draw
	deqp-vk-compute
	deqp-vk-image
	deqp-vk-wsi
	deqp-vk-sparse-resources
	)

add_library(deqp-vk-common STATIC ${DEQP_VK_COMMON_SRCS})
target_link_libraries(deqp-vk-common ${DEQP_VK_COMMON_LIBS})
add_dependencies(deqp-vk-common deqp-vk-data)

set(DEQP_VK_SRCS	)
set(DEQP_VK_LIBS	deqp-vk-common)

if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX OR DE_OS_IS_OSX)
	add_executable(vk-build-programs vktBuildPrograms.cpp)
	target_link_libraries(vk-build-programs deqp-vk-common)
endif ()

add_deqp_module(deqp-vk "${DEQP_VK_SRCS}" "${DEQP_VK_LIBS}" vktTestPackageEntry.cpp)

add_data_dir(deqp-vk ../../data/vulkan	vulkan)
