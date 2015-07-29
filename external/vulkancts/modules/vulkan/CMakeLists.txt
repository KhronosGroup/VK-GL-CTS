# dEQP-VK

add_subdirectory(api)
add_subdirectory(pipeline)
add_subdirectory(binding_model)

include_directories(
	api
	pipeline
	binding_model
	)

set(DEQP_VK_COMMON_SRCS
	vktTestCase.cpp
	vktTestCase.hpp
	vktTestCaseUtil.cpp
	vktTestCaseUtil.hpp
	vktTestPackage.cpp
	vktTestPackage.hpp
	vktInfo.cpp
	vktInfo.hpp
	)

set(DEQP_VK_COMMON_LIBS
	tcutil
	vkutil
	glutil
	deqp-vk-api
	deqp-vk-pipeline
	deqp-vk-binding-model
	)

if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX OR DE_OS_IS_OSX)
	add_library(deqp-vk-common STATIC ${DEQP_VK_COMMON_SRCS})
	target_link_libraries(deqp-vk-common ${DEQP_VK_COMMON_LIBS})

	add_executable(vk-build-programs vktBuildPrograms.cpp)
	target_link_libraries(vk-build-programs deqp-vk-common)

	set(DEQP_VK_SRCS	)
	set(DEQP_VK_LIBS	deqp-vk-common)

else ()
	set(DEQP_VK_SRCS	${DEQP_VK_COMMON_SRCS})
	set(DEQP_VK_LIBS	${DEQP_VK_COMMON_LIBS})

endif ()

add_deqp_module(deqp-vk "${DEQP_VK_SRCS}" "${DEQP_VK_LIBS}" vktTestPackageEntry.cpp)

add_data_dir(deqp-vk ../../data/vulkan	vulkan)
