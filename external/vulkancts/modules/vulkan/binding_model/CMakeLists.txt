# dEQP-VK.binding_model

include_directories(..)

set(DEQP_VK_BINDING_MODEL_SRCS
	vktBindingModelTests.cpp
	vktBindingModelTests.hpp
	vktBindingShaderAccessTests.cpp
	vktBindingShaderAccessTests.hpp
	vktBindingDescriptorUpdateTests.cpp
	vktBindingDescriptorUpdateTests.hpp
	vktBindingDescriptorSetRandomTests.cpp
	vktBindingDescriptorSetRandomTests.hpp
	vktBindingDescriptorCopyTests.cpp
	vktBindingDescriptorCopyTests.hpp
	vktBindingBufferDeviceAddressTests.cpp
	vktBindingBufferDeviceAddressTests.hpp
	)

set(DEQP_VK_BINDING_MODEL_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_BINDING_MODEL_SRCS ../pch.cpp)

add_library(deqp-vk-binding-model STATIC ${DEQP_VK_BINDING_MODEL_SRCS})
target_link_libraries(deqp-vk-binding-model ${DEQP_VK_BINDING_MODEL_LIBS})
