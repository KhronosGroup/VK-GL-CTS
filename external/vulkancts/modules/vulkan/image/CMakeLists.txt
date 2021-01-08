include_directories(..)

set(DEQP_VK_IMAGE_SRCS
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
	vktImageSubresourceLayoutTests.cpp
	vktImageSubresourceLayoutTests.hpp
	vktImageMismatchedWriteOpTests.cpp
	vktImageMismatchedWriteOpTests.hpp
	)

set(DEQP_VK_IMAGE_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_IMAGE_SRCS ../pch.cpp)

add_library(deqp-vk-image STATIC ${DEQP_VK_IMAGE_SRCS})
target_link_libraries(deqp-vk-image ${DEQP_VK_IMAGE_LIBS})
