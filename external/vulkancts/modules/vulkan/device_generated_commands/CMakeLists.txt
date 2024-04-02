include_directories(
	..
	${DEQP_INL_DIR}
	)

set(DEQP_VK_DGC_SRCS
	vktDGCTests.cpp
	vktDGCTests.hpp
	vktDGCUtil.cpp
	vktDGCUtil.hpp
	vktDGCPropertyTests.cpp
	vktDGCPropertyTests.hpp
	vktDGCComputeGetInfoTests.cpp
	vktDGCComputeGetInfoTests.hpp
	vktDGCComputeSmokeTests.cpp
	vktDGCComputeSmokeTests.hpp
	vktDGCComputeLayoutTests.cpp
	vktDGCComputeLayoutTests.hpp
	vktDGCComputeMiscTests.cpp
	vktDGCComputeMiscTests.hpp
	vktDGCComputePreprocessTests.cpp
	vktDGCComputePreprocessTests.hpp
	vktDGCComputeSubgroupTests.cpp
	vktDGCComputeSubgroupTests.hpp
	vktDGCComputeConditionalTests.cpp
	vktDGCComputeConditionalTests.hpp
	)

set(DEQP_VK_DGC_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_DGC_SRCS ../pch.cpp)

add_library(deqp-vk-dgc STATIC ${DEQP_VK_DGC_SRCS})
target_link_libraries(deqp-vk-dgc ${DEQP_VK_DGC_LIBS})
