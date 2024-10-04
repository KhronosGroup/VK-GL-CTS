include_directories(
	..
	${DEQP_INL_DIR}
	)

set(DEQP_VK_DGC_SRCS
	vktDGCTests.cpp
	vktDGCTests.hpp
	vktDGCUtilCommon.cpp
	vktDGCUtilCommon.hpp
	vktDGCUtil.cpp
	vktDGCUtil.hpp
	vktDGCUtilExt.cpp
	vktDGCUtilExt.hpp
	vktDGCPropertyTests.cpp
	vktDGCPropertyTests.hpp
	vktDGCPropertyTestsExt.cpp
	vktDGCPropertyTestsExt.hpp
	vktDGCComputeGetInfoTests.cpp
	vktDGCComputeGetInfoTests.hpp
	vktDGCComputeGetInfoTestsExt.cpp
	vktDGCComputeGetInfoTestsExt.hpp
	vktDGCComputeSmokeTests.cpp
	vktDGCComputeSmokeTests.hpp
	vktDGCComputeSmokeTestsExt.cpp
	vktDGCComputeSmokeTestsExt.hpp
	vktDGCComputeLayoutTests.cpp
	vktDGCComputeLayoutTests.hpp
	vktDGCComputeLayoutTestsExt.cpp
	vktDGCComputeLayoutTestsExt.hpp
	vktDGCComputeMiscTests.cpp
	vktDGCComputeMiscTests.hpp
	vktDGCComputeMiscTestsExt.cpp
	vktDGCComputeMiscTestsExt.hpp
	vktDGCComputePreprocessTests.cpp
	vktDGCComputePreprocessTests.hpp
	vktDGCComputePreprocessTestsExt.cpp
	vktDGCComputePreprocessTestsExt.hpp
	vktDGCComputeSubgroupTests.cpp
	vktDGCComputeSubgroupTests.hpp
	vktDGCComputeSubgroupTestsExt.cpp
	vktDGCComputeSubgroupTestsExt.hpp
	vktDGCComputeConditionalTests.cpp
	vktDGCComputeConditionalTests.hpp
	vktDGCComputeConditionalTestsExt.cpp
	vktDGCComputeConditionalTestsExt.hpp
	vktDGCGraphicsDrawTestsExt.cpp
	vktDGCGraphicsDrawTestsExt.hpp
	vktDGCGraphicsDrawCountTestsExt.cpp
	vktDGCGraphicsDrawCountTestsExt.hpp
	vktDGCGraphicsConditionalTestsExt.cpp
	vktDGCGraphicsConditionalTestsExt.hpp
	vktDGCGraphicsMeshTestsExt.cpp
	vktDGCGraphicsMeshTestsExt.hpp
	vktDGCGraphicsMeshConditionalTestsExt.cpp
	vktDGCGraphicsMeshConditionalTestsExt.hpp
	vktDGCGraphicsMiscTestsExt.cpp
	vktDGCGraphicsMiscTestsExt.hpp
	vktDGCRayTracingTestsExt.cpp
	vktDGCRayTracingTestsExt.hpp
	vktDGCGraphicsXfbTestsExt.cpp
	vktDGCGraphicsXfbTestsExt.hpp
	)

set(DEQP_VK_DGC_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_DGC_SRCS ../pch.cpp)

add_library(deqp-vk-dgc STATIC ${DEQP_VK_DGC_SRCS})
target_link_libraries(deqp-vk-dgc ${DEQP_VK_DGC_LIBS})
