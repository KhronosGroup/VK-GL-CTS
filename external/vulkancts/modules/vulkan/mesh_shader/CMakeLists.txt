include_directories(
	..
	${DEQP_INL_DIR}
	)

set(DEQP_VK_MESH_SHADER_SRCS
	vktMeshShaderTests.cpp
	vktMeshShaderTests.hpp
	vktMeshShaderSyncTests.cpp
	vktMeshShaderSyncTests.hpp
	vktMeshShaderSyncTestsEXT.cpp
	vktMeshShaderSyncTestsEXT.hpp
	vktMeshShaderSmokeTests.cpp
	vktMeshShaderSmokeTests.hpp
	vktMeshShaderSmokeTestsEXT.cpp
	vktMeshShaderSmokeTestsEXT.hpp
	vktMeshShaderApiTests.cpp
	vktMeshShaderApiTests.hpp
	vktMeshShaderApiTestsEXT.cpp
	vktMeshShaderApiTestsEXT.hpp
	vktMeshShaderPropertyTests.cpp
	vktMeshShaderPropertyTests.hpp
	vktMeshShaderBuiltinTests.cpp
	vktMeshShaderBuiltinTests.hpp
	vktMeshShaderBuiltinTestsEXT.cpp
	vktMeshShaderBuiltinTestsEXT.hpp
	vktMeshShaderMiscTests.cpp
	vktMeshShaderMiscTests.hpp
	vktMeshShaderMiscTestsEXT.cpp
	vktMeshShaderMiscTestsEXT.hpp
	vktMeshShaderInOutTestsEXT.cpp
	vktMeshShaderInOutTestsEXT.hpp
	vktMeshShaderPropertyTestsEXT.cpp
	vktMeshShaderPropertyTestsEXT.hpp
	vktMeshShaderConditionalRenderingTestsEXT.cpp
	vktMeshShaderConditionalRenderingTestsEXT.hpp
	vktMeshShaderProvokingVertexTestsEXT.cpp
	vktMeshShaderProvokingVertexTestsEXT.hpp
	vktMeshShaderQueryTestsEXT.cpp
	vktMeshShaderQueryTestsEXT.hpp
	vktMeshShaderUtil.cpp
	vktMeshShaderUtil.hpp
	)

set(DEQP_VK_MESH_SHADER_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_MESH_SHADER_SRCS ../pch.cpp)

add_library(deqp-vk-mesh-shader STATIC ${DEQP_VK_MESH_SHADER_SRCS})
target_link_libraries(deqp-vk-mesh-shader ${DEQP_VK_MESH_SHADER_LIBS})
