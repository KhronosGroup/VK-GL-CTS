include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_VKSC_IMAGE_SRCS
	vktImageTests.cpp
	vktImageTests.hpp
	vktImageTestsUtil.cpp
	vktImageTestsUtil.hpp
	vktImageAstcDecodeModeTests.cpp
	vktImageAstcDecodeModeTests.hpp
	vktImageAtomicOperationTests.cpp
	vktImageAtomicOperationTests.hpp
	vktImageAtomicSpirvShaders.cpp
	vktImageAtomicSpirvShaders.hpp
	vktImageLoadStoreTests.cpp
	vktImageLoadStoreTests.hpp
	vktImageMismatchedFormatsTests.cpp
	vktImageMismatchedFormatsTests.hpp
	vktImageMutableTests.cpp
	vktImageMutableTests.hpp
	vktImageQualifiersTests.cpp
	vktImageQualifiersTests.hpp
	vktImageSizeTests.cpp
	vktImageSizeTests.hpp
	vktImageTexture.cpp
	vktImageTexture.hpp
	vktImageMultisampleLoadStoreTests.cpp
	vktImageMultisampleLoadStoreTests.hpp
	vktImageCompressionTranscodingSupport.cpp
	vktImageCompressionTranscodingSupport.hpp
	vktImageLoadStoreUtil.cpp
	vktImageLoadStoreUtil.hpp
	vktImageTranscodingSupportTests.cpp
	vktImageTranscodingSupportTests.hpp
	vktImageMisalignedCubeTests.cpp
	vktImageMisalignedCubeTests.hpp
	vktImageSampleDrawnCubeFaceTests.cpp
	vktImageSampleDrawnCubeFaceTests.hpp
	vktImageSubresourceLayoutTests.cpp
	vktImageSubresourceLayoutTests.hpp
	vktImageMismatchedWriteOpTests.cpp
	vktImageMismatchedWriteOpTests.hpp
	vktImageDepthStencilDescriptorTests.cpp
	vktImageDepthStencilDescriptorTests.hpp
	vktImageSampleCompressedTextureTests.cpp
	vktImageSampleCompressedTextureTests.hpp
	vktImageExtendedUsageBitTests.cpp
	vktImageExtendedUsageBitTests.hpp
	vktImageTransfer.cpp
	vktImageTransfer.hpp
	)

set(DEQP_VK_IMAGE_SRCS
	vktImageHostImageCopyTests.cpp
	vktImageHostImageCopyTests.hpp
	)

PCH(DEQP_VK_IMAGE_SRCS ../pch.cpp)

add_library(deqp-vk-image STATIC ${DEQP_VK_VKSC_IMAGE_SRCS} ${DEQP_VK_IMAGE_SRCS})
target_link_libraries(deqp-vk-image tcutil vkutil)

add_library(deqp-vksc-image STATIC ${DEQP_VK_VKSC_IMAGE_SRCS})
target_link_libraries(deqp-vksc-image PUBLIC deqp-vksc-util tcutil vkscutil)
