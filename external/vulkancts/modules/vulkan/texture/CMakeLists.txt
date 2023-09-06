# dEQP-VK.texture

include_directories(
	..
	../shaderexecutor
	../amber
	${DEQP_INL_DIR}
	)

set(DEQP_VK_TEXTURE_SRCS
	vktSampleVerifier.cpp
	vktSampleVerifier.hpp
	vktSampleVerifierUtil.cpp
	vktSampleVerifierUtil.hpp
	vktTextureTests.cpp
	vktTextureTests.hpp
	vktTextureTestUtil.cpp
	vktTextureTestUtil.hpp
	vktTextureConversionTests.cpp
	vktTextureConversionTests.hpp
	vktTextureFilteringExplicitLodTests.cpp
	vktTextureFilteringExplicitLodTests.hpp
	vktTextureFilteringTests.cpp
	vktTextureFilteringTests.hpp
	vktTextureMipmapTests.cpp
	vktTextureMipmapTests.hpp
	vktTextureMultisampleTests.cpp
	vktTextureMultisampleTests.hpp
	vktTextureShadowTests.cpp
	vktTextureShadowTests.hpp
	vktTextureSubgroupLodTests.cpp
	vktTextureSubgroupLodTests.hpp
	vktTextureFilteringAnisotropyTests.cpp
	vktTextureFilteringAnisotropyTests.hpp
	vktTextureCompressedFormatTests.cpp
	vktTextureCompressedFormatTests.hpp
	vktTextureSwizzleTests.cpp
	vktTextureSwizzleTests.hpp
	vktTextureTexelBufferTests.cpp
	vktTextureTexelBufferTests.hpp
	vktTextureTexelOffsetTests.cpp
	vktTextureTexelOffsetTests.hpp
	)

PCH(DEQP_VK_TEXTURE_SRCS ../pch.cpp)

add_library(deqp-vk-texture STATIC ${DEQP_VK_TEXTURE_SRCS})
target_link_libraries(deqp-vk-texture deqp-vk-shaderexecutor tcutil vkutil)

add_library(deqp-vksc-texture STATIC ${DEQP_VK_TEXTURE_SRCS})
target_link_libraries(deqp-vksc-texture PUBLIC deqp-vksc-shaderexecutor deqp-vksc-util tcutil vkscutil)
