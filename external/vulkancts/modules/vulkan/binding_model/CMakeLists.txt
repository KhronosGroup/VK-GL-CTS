# dEQP-VK.binding_model

include_directories(
	..
	../amber
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_BINDING_MODEL_SRCS
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

set(DEQP_VK_BINDING_MODEL_SRCS
	vktBindingDescriptorUpdateASTests.cpp
	vktBindingDescriptorUpdateASTests.hpp
	vktBindingDynamicOffsetTests.cpp
	vktBindingDynamicOffsetTests.hpp
	vktBindingMutableTests.cpp
	vktBindingMutableTests.hpp
	vktBindingDescriptorBufferTests.cpp
	vktBindingDescriptorBufferTests.hpp
	)

PCH(DEQP_VK_BINDING_MODEL_SRCS ../pch.cpp)

add_library(deqp-vk-binding-model STATIC ${DEQP_VK_VKSC_BINDING_MODEL_SRCS} ${DEQP_VK_BINDING_MODEL_SRCS} )
target_link_libraries(deqp-vk-binding-model tcutil vkutil)

add_library(deqp-vksc-binding-model STATIC ${DEQP_VK_VKSC_BINDING_MODEL_SRCS})
target_link_libraries(deqp-vksc-binding-model PUBLIC deqp-vksc-util tcutil vkscutil)
