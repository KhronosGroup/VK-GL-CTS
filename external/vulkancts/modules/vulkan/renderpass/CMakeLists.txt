# Render pass tests

include_directories(
	..
	../draw
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_RENDER_PASS_SRCS
	vktRenderPassTests.cpp
	vktRenderPassTests.hpp
	vktRenderPassTestsUtil.cpp
	vktRenderPassTestsUtil.hpp
	vktRenderPassGroupParams.hpp
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
	vktRenderPassLoadStoreOpNoneTests.cpp
	vktRenderPassLoadStoreOpNoneTests.hpp
	vktRenderPassDitheringTests.cpp
	vktRenderPassDitheringTests.hpp
	vktDynamicRenderingTests.cpp
	vktDynamicRenderingTests.hpp
	vktRenderPassDepthStencilWriteConditionsTests.cpp
	vktRenderPassDepthStencilWriteConditionsTests.hpp
	vktRenderPassSubpassMergeFeedbackTests.cpp
	vktRenderPassSubpassMergeFeedbackTests.hpp
	vktDynamicRenderingRandomTests.cpp
	vktDynamicRenderingRandomTests.hpp
	vktDynamicRenderingUnusedAttachmentsTests.cpp
	vktDynamicRenderingUnusedAttachmentsTests.hpp
	)

PCH(DEQP_VK_RENDER_PASS_SRCS ../pch.cpp)

add_library(deqp-vk-render-pass STATIC ${DEQP_VK_VKSC_RENDER_PASS_SRCS} ${DEQP_VK_RENDER_PASS_SRCS})
target_link_libraries(deqp-vk-render-pass tcutil vkutil deqp-vk-draw)

add_library(deqp-vksc-render-pass STATIC ${DEQP_VK_VKSC_RENDER_PASS_SRCS})
target_link_libraries(deqp-vksc-render-pass PUBLIC deqp-vksc-util tcutil vkscutil deqp-vksc-draw)
