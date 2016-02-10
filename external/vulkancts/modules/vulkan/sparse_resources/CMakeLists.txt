include_directories(..)

set(DEQP_VK_IMAGE_SRCS
	vktSparseResourcesBufferSparseResidency.cpp
	vktSparseResourcesBufferSparseResidency.hpp
	vktSparseResourcesImageSparseBinding.cpp
	vktSparseResourcesImageSparseBinding.hpp
	vktSparseResourcesBufferSparseBinding.cpp
	vktSparseResourcesBufferSparseBinding.hpp
	vktSparseResourcesBase.cpp
	vktSparseResourcesBase.hpp
	vktSparseResourcesTests.cpp
	vktSparseResourcesTests.hpp
	vktSparseResourcesTestsUtil.cpp
	vktSparseResourcesTestsUtil.hpp
	)

set(DEQP_VK_IMAGE_LIBS
	tcutil
	vkutil
	)

add_library(deqp-vk-sparse-resources STATIC ${DEQP_VK_IMAGE_SRCS})
target_link_libraries(deqp-vk-sparse-resources ${DEQP_VK_IMAGE_LIBS})
