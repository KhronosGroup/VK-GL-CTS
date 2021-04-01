# Render pass tests

include_directories(..)

set(DEQP_VK_VKSC_RENDER_PASS_SRCS
	vktRenderPassTests.cpp
	vktRenderPassTests.hpp
	vktRenderPassTestsUtil.cpp
	vktRenderPassTestsUtil.hpp
	vktRenderPassMultisampleTests.cpp
	vktRenderPassMultisampleTests.hpp
	vktRenderPassMultisampleResolveTests.cpp
	vktRenderPassMultisampleResolveTests.hpp
	vktRenderPassDepthStencilResolveTests.cpp
	vktRenderPassDepthStencilResolveTests.hpp
	vktRenderPassSampleReadTests.cpp
	vktRenderPassSampleReadTests.hpp
	vktRenderPassSubpassDependencyTests.cpp
	vktRenderPassSubpassDependencyTests.hpp
	vktRenderPassUnusedAttachmentSparseFillingTests.hpp
	vktRenderPassUnusedAttachmentSparseFillingTests.cpp
	vktRenderPassUnusedAttachmentTests.cpp
	vktRenderPassUnusedAttachmentTests.hpp
	vktRenderPassUnusedClearAttachmentTests.cpp
	vktRenderPassUnusedClearAttachmentTests.hpp
	vktRenderPassMultipleSubpassesMultipleCommandBuffersTests.cpp
	vktRenderPassMultipleSubpassesMultipleCommandBuffersTests.hpp
	)

set(DEQP_VK_RENDER_PASS_SRCS
	vktRenderPassFragmentDensityMapTests.cpp
	vktRenderPassFragmentDensityMapTests.hpp
	vktRenderPassSparseRenderTargetTests.cpp
	vktRenderPassSparseRenderTargetTests.hpp
	)

PCH(DEQP_VK_RENDER_PASS_SRCS ../pch.cpp)

add_library(deqp-vk-render-pass STATIC ${DEQP_VK_VKSC_RENDER_PASS_SRCS} ${DEQP_VK_RENDER_PASS_SRCS})
target_link_libraries(deqp-vk-render-pass tcutil vkutil)

add_library(deqp-vksc-render-pass STATIC ${DEQP_VK_VKSC_RENDER_PASS_SRCS})
target_include_directories(deqp-vksc-render-pass BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkansc)
target_link_libraries(deqp-vksc-render-pass PUBLIC deqp-vksc-util tcutil vkscutil)
