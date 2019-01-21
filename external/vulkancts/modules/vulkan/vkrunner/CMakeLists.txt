# dEQP-VK.vkrunner

include_directories(..)

set(DEQP_VK_VKRUNNER_SRCS
	vktVkRunnerTestCase.cpp
	vktVkRunnerTestCase.hpp
	vktVkRunnerExampleTests.cpp
	vktVkRunnerExampleTests.hpp
	vktVkRunnerGraphicsFuzzTests.cpp
	vktVkRunnerGraphicsFuzzTests.hpp
	)

set(DEQP_VK_VKRUNNER_LIBS
	tcutil
	vkutil
	vkrunner
	)

add_library(deqp-vk-vkrunner STATIC ${DEQP_VK_VKRUNNER_SRCS})
target_link_libraries(deqp-vk-vkrunner ${DEQP_VK_VKRUNNER_LIBS})
add_custom_target(deqp-vk-vkrunner-data)
add_dependencies(deqp-vk-vkrunner deqp-vk-vkrunner-data)
add_data_dir(deqp-vk-vkrunner ../../../data/vulkan/vkrunner vkrunner)
