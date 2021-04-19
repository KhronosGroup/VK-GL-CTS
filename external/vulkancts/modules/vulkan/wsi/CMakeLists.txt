# WSI tests

include_directories(..)

set(DEQP_VK_WSI_SRCS
	vktNativeObjectsUtil.cpp
	vktNativeObjectsUtil.hpp
	vktWsiTests.cpp
	vktWsiTests.hpp
	vktWsiSurfaceTests.cpp
	vktWsiSurfaceTests.hpp
	vktWsiSwapchainTests.cpp
	vktWsiSwapchainTests.hpp
	vktWsiIncrementalPresentTests.cpp
	vktWsiIncrementalPresentTests.hpp
	vktWsiDisplayControlTests.cpp
	vktWsiDisplayControlTests.hpp
	vktWsiDisplayTimingTests.cpp
	vktWsiDisplayTimingTests.hpp
	vktWsiSharedPresentableImageTests.cpp
	vktWsiSharedPresentableImageTests.hpp
	vktWsiDisplayTests.hpp
	vktWsiDisplayTests.cpp
	vktWsiColorSpaceTests.cpp
	vktWsiColorSpaceTests.hpp
	vktWsiFullScreenExclusiveTests.cpp
	vktWsiFullScreenExclusiveTests.hpp
	vktWsiPresentIdWaitTests.cpp
	vktWsiPresentIdWaitTests.hpp
	)

set(DEQP_VK_WSI_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_WSI_SRCS ../pch.cpp)

add_library(deqp-vk-wsi STATIC ${DEQP_VK_WSI_SRCS})
target_link_libraries(deqp-vk-wsi ${DEQP_VK_WSI_LIBS})
