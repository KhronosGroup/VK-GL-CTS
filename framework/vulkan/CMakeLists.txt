# vk - Vulkan utilites

set(VKUTIL_SRCS
	vkDefs.cpp
	vkDefs.hpp
	vkRef.cpp
	vkRef.hpp
	vkPlatform.cpp
	vkPlatform.hpp
	vkPrograms.cpp
	vkPrograms.hpp
	vkStrUtil.cpp
	vkStrUtil.hpp
	vkQueryUtil.cpp
	vkQueryUtil.hpp
	vkMemUtil.cpp
	vkMemUtil.hpp
	vkDeviceUtil.cpp
	vkDeviceUtil.hpp
	)

set(VKUTIL_LIBS
	tcutil
	)

add_library(vkutil STATIC ${VKUTIL_SRCS})
target_link_libraries(vkutil ${VKUTIL_LIBS})
