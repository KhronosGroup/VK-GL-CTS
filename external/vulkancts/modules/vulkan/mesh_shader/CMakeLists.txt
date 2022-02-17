include_directories(
	..
	${DEQP_INL_DIR}
	)

set(DEQP_VK_MESH_SHADER_SRCS
	vktMeshShaderTests.cpp
	vktMeshShaderTests.hpp
	vktMeshShaderSyncTests.cpp
	vktMeshShaderSyncTests.hpp
	vktMeshShaderSmokeTests.cpp
	vktMeshShaderSmokeTests.hpp
	vktMeshShaderApiTests.cpp
	vktMeshShaderApiTests.hpp
	vktMeshShaderPropertyTests.cpp
	vktMeshShaderPropertyTests.hpp
	vktMeshShaderBuiltinTests.cpp
	vktMeshShaderBuiltinTests.hpp
	vktMeshShaderMiscTests.cpp
	vktMeshShaderMiscTests.hpp
	)

set(DEQP_VK_MESH_SHADER_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_MESH_SHADER_SRCS ../pch.cpp)

add_library(deqp-vk-mesh-shader STATIC ${DEQP_VK_MESH_SHADER_SRCS})
target_link_libraries(deqp-vk-mesh-shader ${DEQP_VK_MESH_SHADER_LIBS})
