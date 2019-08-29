# dEQP-VK.amber

include_directories(..)

set(DEQP_VK_AMBER_SRCS
	vktAmberHelper.hpp
	vktAmberHelper.cpp
	vktAmberTestCase.hpp
	vktAmberTestCase.cpp
	vktAmberTestCaseUtil.cpp
	vktAmberExampleTests.hpp
	vktAmberExampleTests.cpp
	vktAmberGraphicsFuzzTests.hpp
	vktAmberGraphicsFuzzTests.cpp
	)

set(DEQP_VK_AMBER_LIBS
	tcutil
	libamber
	)

add_definitions(-DAMBER_CTS_VULKAN_HEADER=1)

if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	set(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
	set_source_files_properties(
		FILES vktAmberHelper.cpp
		PROPERTIES COMPILE_FLAGS "${DE_3RD_PARTY_CXX_FLAGS}")
elseif (DE_COMPILER_IS_MSC)
	set(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} /wd4068")
endif ()

include_directories("../../../../amber/src/include")
add_library(deqp-vk-amber STATIC ${DEQP_VK_AMBER_SRCS})
target_link_libraries(deqp-vk-amber ${DEQP_VK_AMBER_LIBS})

add_custom_target(deqp-vk-amber-data)
add_dependencies(deqp-vk-amber deqp-vk-amber-data)
add_data_dir(deqp-vk-amber ../../../data/vulkan/amber amber)
