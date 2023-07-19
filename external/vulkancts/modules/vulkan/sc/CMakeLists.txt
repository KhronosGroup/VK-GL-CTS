include_directories(..)

set(DEQP_VK_SC_SRCS
	vktSafetyCriticalTests.cpp
	vktSafetyCriticalTests.hpp
	vktSafetyCriticalApiTests.cpp
	vktSafetyCriticalApiTests.hpp
	vktDeviceObjectReservationTests.cpp
	vktDeviceObjectReservationTests.hpp
	vktPipelineIdentifierTests.cpp
	vktPipelineIdentifierTests.hpp
	vktFaultHandlingTests.cpp
	vktFaultHandlingTests.hpp
	vktCommandPoolMemoryReservationTests.cpp
	vktCommandPoolMemoryReservationTests.hpp
	vktObjectRefreshTests.cpp
	vktObjectRefreshTests.hpp
	vktPipelineCacheSCTests.cpp
	vktPipelineCacheSCTests.hpp
	vktApplicationParametersTests.cpp
	vktApplicationParametersTests.hpp
)

PCH(DEQP_VK_SC_SRCS ../pch.cpp)

add_library(deqp-vksc-sc STATIC ${DEQP_VK_SC_SRCS})
target_link_libraries(deqp-vksc-sc PUBLIC deqp-vksc-util tcutil vkscutil)
