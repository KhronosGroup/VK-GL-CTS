include_directories(..)

set(DEQP_VK_RAY_TRACING_SRCS
	vktRayTracingBuildIndirectTests.cpp
	vktRayTracingBuildIndirectTests.hpp
	vktRayTracingBuiltinTests.cpp
	vktRayTracingBuiltinTests.hpp
	vktRayTracingCallableShadersTests.cpp
	vktRayTracingCallableShadersTests.hpp
	vktRayTracingTraceRaysIndirectTests.cpp
	vktRayTracingTraceRaysIndirectTests.hpp
	vktRayTracingShaderBindingTableTests.cpp
	vktRayTracingShaderBindingTableTests.hpp
	vktRayTracingTraversalControlTests.cpp
	vktRayTracingTraversalControlTests.hpp
	vktRayTracingAccelerationStructuresTests.cpp
	vktRayTracingAccelerationStructuresTests.hpp
	vktRayTracingTests.cpp
	vktRayTracingTests.hpp
	)

set(DEQP_VK_RAY_TRACING_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_RAY_TRACING_SRCS ../pch.cpp)

add_library(deqp-vk-ray-tracing STATIC ${DEQP_VK_RAY_TRACING_SRCS})
target_link_libraries(deqp-vk-ray-tracing ${DEQP_VK_RAY_TRACING_LIBS})
