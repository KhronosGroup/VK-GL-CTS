# dEQP-VK

set(DEQP_INL_DIR ${PROJECT_BINARY_DIR}/external/vulkancts/framework/vulkan)

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
add_subdirectory(conditional_rendering)
add_subdirectory(draw)
add_subdirectory(device_group)
add_subdirectory(compute)
add_subdirectory(image)
add_subdirectory(wsi)
add_subdirectory(sparse_resources)
add_subdirectory(tessellation)
add_subdirectory(rasterization)
add_subdirectory(synchronization)
add_subdirectory(clipping)
add_subdirectory(fragment_ops)
add_subdirectory(geometry)
add_subdirectory(texture)
add_subdirectory(robustness)
add_subdirectory(renderpass)
add_subdirectory(multiview)
add_subdirectory(subgroups)
add_subdirectory(ycbcr)
add_subdirectory(protected_memory)
add_subdirectory(memory_model)
add_subdirectory(transform_feedback)
add_subdirectory(util)
add_subdirectory(amber)
add_subdirectory(imageless_framebuffer)
add_subdirectory(descriptor_indexing)
add_subdirectory(fragment_shader_interlock)
add_subdirectory(modifiers)
add_subdirectory(ray_tracing)
add_subdirectory(ray_query)
add_subdirectory(postmortem)
add_subdirectory(fragment_shading_rate)
add_subdirectory(reconvergence)
add_subdirectory(mesh_shader)


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
	conditional_rendering
	draw
	device_group
	compute
	image
	wsi
	sparse_resources
	tessellation
	rasterization
	synchronization
	clipping
	fragment_ops
	texture
	geometry
	robustness
	renderpass
	multiview
	subgroups
	ycbcr
	protected_memory
	memory_model
	transform_feedback
	util
	amber
	imageless_framebuffer
	descriptor_indexing
	fragment_shader_interlock
	fragment_shading_rate
	modifiers
	ray_tracing
	ray_query
	postmortem
	reconvergence
	mesh_shader
	${DEQP_INL_DIR}
	)

set(DEQP_VK_SRCS
	vktTestCaseDefs.hpp
	vktTestCase.cpp
	vktTestCase.hpp
	vktTestCaseUtil.cpp
	vktTestCaseUtil.hpp
	vktTestPackage.cpp
	vktTestPackage.hpp
	vktShaderLibrary.cpp
	vktShaderLibrary.hpp
	vktTestGroupUtil.cpp
	vktTestGroupUtil.hpp
	vktInfoTests.cpp
	vktInfoTests.hpp
	vktCustomInstancesDevices.cpp
	vktCustomInstancesDevices.hpp
	)

set(DEQP_VK_LIBS
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
	deqp-vk-conditional-rendering
	deqp-vk-draw
	deqp-vk-device-group
	deqp-vk-compute
	deqp-vk-image
	deqp-vk-wsi
	deqp-vk-sparse-resources
	deqp-vk-tessellation
	deqp-vk-rasterization
	deqp-vk-synchronization
	deqp-vk-clipping
	deqp-vk-fragment-ops
	deqp-vk-texture
	deqp-vk-geometry
	deqp-vk-robustness
	deqp-vk-render-pass
	deqp-vk-multiview
	deqp-vk-subgroups
	deqp-vk-ycbcr
	deqp-vk-protected-memory
	deqp-vk-memory-model
	deqp-vk-amber
	deqp-vk-imageless-framebuffer
	deqp-vk-transform-feedback
	deqp-vk-descriptor-indexing
	deqp-vk-fragment-shader-interlock
	deqp-vk-modifiers
	deqp-vk-ray-tracing
	deqp-vk-ray-query
	deqp-vk-postmortem
	deqp-vk-fragment-shading-rate
	deqp-vk-reconvergence
	deqp-vk-mesh-shader
	)

if (DE_COMPILER_IS_MSC AND (DE_PTR_SIZE EQUAL 4))
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
endif ()

PCH(DEQP_VK_SRCS pch.cpp)

add_deqp_module(deqp-vk "${DEQP_VK_SRCS}" "${DEQP_VK_LIBS}" vktTestPackageEntry.cpp)


add_data_dir(deqp-vk ../../data/vulkan vulkan)
add_data_dir(deqp-vk ../../mustpass/master/vk-default vk-default)

# add all directories containing android mustpass
file(GLOB PATHS_MATCHING_PATTERN ${CMAKE_SOURCE_DIR}/android/cts/master/vk-*/)
FOREACH(FOUND_PATH ${PATHS_MATCHING_PATTERN})
	IF(IS_DIRECTORY ${FOUND_PATH})
		SET(LAST_DIRECTORY "")
		file(RELATIVE_PATH LAST_DIRECTORY ${CMAKE_SOURCE_DIR}/android/cts/master ${FOUND_PATH})
		add_data_dir(deqp-vk ../../../../android/cts/master/${LAST_DIRECTORY} ${LAST_DIRECTORY})
	ENDIF()
ENDFOREACH()

if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX OR DE_OS_IS_OSX)
	add_executable(vk-build-programs vktBuildPrograms.cpp)
	target_link_libraries(vk-build-programs deqp-vk${MODULE_LIB_TARGET_POSTFIX})
endif ()
