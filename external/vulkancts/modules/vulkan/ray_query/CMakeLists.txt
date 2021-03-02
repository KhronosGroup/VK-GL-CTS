include_directories(..)

set(DEQP_VK_RAY_QUERY_SRCS
	vktRayQueryTests.cpp
	vktRayQueryTests.hpp
	vktRayQueryBuiltinTests.cpp
	vktRayQueryBuiltinTests.hpp
	vktRayQueryTraversalControlTests.cpp
	vktRayQueryTraversalControlTests.hpp
	vktRayQueryAccelerationStructuresTests.cpp
	vktRayQueryAccelerationStructuresTests.hpp
	vktRayQueryBarycentricCoordinatesTests.cpp
	vktRayQueryBarycentricCoordinatesTests.hpp
	vktRayQueryProceduralGeometryTests.cpp
	vktRayQueryProceduralGeometryTests.hpp
	vktRayQueryWatertightnessTests.cpp
	vktRayQueryWatertightnessTests.hpp
	vktRayQueryCullRayFlagsTests.cpp
	vktRayQueryCullRayFlagsTests.hpp
	vktRayQueryMiscTests.cpp
	vktRayQueryMiscTests.hpp
	vktRayQueryDirectionTests.cpp
	vktRayQueryDirectionTests.hpp
	)

set(DEQP_VK_RAY_QUERY_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_RAY_QUERY_SRCS ../pch.cpp)

add_library(deqp-vk-ray-query STATIC ${DEQP_VK_RAY_QUERY_SRCS})
target_link_libraries(deqp-vk-ray-query ${DEQP_VK_RAY_QUERY_LIBS})
