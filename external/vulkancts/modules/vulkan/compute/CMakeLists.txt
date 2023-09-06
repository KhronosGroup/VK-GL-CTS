include_directories(
	..
	../amber
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_COMPUTE_SRCS
	vktComputeTests.cpp
	vktComputeTests.hpp
	vktComputeBasicComputeShaderTests.cpp
	vktComputeBasicComputeShaderTests.hpp
	vktComputeIndirectComputeDispatchTests.cpp
	vktComputeIndirectComputeDispatchTests.hpp
	vktComputeShaderBuiltinVarTests.cpp
	vktComputeShaderBuiltinVarTests.hpp
	vktComputeTestsUtil.cpp
	vktComputeTestsUtil.hpp
	vktComputeZeroInitializeWorkgroupMemoryTests.cpp
	vktComputeZeroInitializeWorkgroupMemoryTests.hpp
	)

set(DEQP_VK_COMPUTE_SRCS
	vktComputeCooperativeMatrixTests.cpp
	vktComputeCooperativeMatrixTests.hpp
	vktComputeWorkgroupMemoryExplicitLayoutTests.cpp
	vktComputeWorkgroupMemoryExplicitLayoutTests.hpp
	)

PCH(DEQP_VK_COMPUTE_SRCS ../pch.cpp)

add_library(deqp-vk-compute STATIC ${DEQP_VK_VKSC_COMPUTE_SRCS} ${DEQP_VK_COMPUTE_SRCS})
target_link_libraries(deqp-vk-compute tcutil vkutil)

add_library(deqp-vksc-compute STATIC ${DEQP_VK_VKSC_COMPUTE_SRCS})
target_link_libraries(deqp-vksc-compute PUBLIC deqp-vksc-util tcutil vkscutil)
