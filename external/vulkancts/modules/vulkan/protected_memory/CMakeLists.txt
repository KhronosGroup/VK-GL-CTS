include_directories(
	..
	../ycbcr
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_PROTECTED_MEMORY_SRCS
	vktProtectedMemContext.hpp
	vktProtectedMemContext.cpp
	vktProtectedMemUtils.cpp
	vktProtectedMemUtils.hpp
	vktProtectedMemTests.cpp
	vktProtectedMemTests.hpp
	vktProtectedMemImageValidator.cpp
	vktProtectedMemImageValidator.hpp
	vktProtectedMemAttachmentLoadTests.cpp
	vktProtectedMemAttachmentLoadTests.hpp
	vktProtectedMemAttachmentClearTests.cpp
	vktProtectedMemAttachmentClearTests.hpp
	vktProtectedMemCopyImageTests.cpp
	vktProtectedMemCopyImageTests.hpp
	vktProtectedMemBlitImageTests.cpp
	vktProtectedMemBlitImageTests.hpp
	vktProtectedMemClearColorImageTests.cpp
	vktProtectedMemClearColorImageTests.hpp
	vktProtectedMemBufferValidator.cpp
	vktProtectedMemBufferValidator.hpp
	vktProtectedMemFillUpdateCopyBufferTests.cpp
	vktProtectedMemFillUpdateCopyBufferTests.hpp
	vktProtectedMemCopyImageToBufferTests.hpp
	vktProtectedMemCopyImageToBufferTests.cpp
	vktProtectedMemCopyBufferToImageTests.hpp
	vktProtectedMemCopyBufferToImageTests.cpp
	vktProtectedMemStorageBufferTests.cpp
	vktProtectedMemStorageBufferTests.hpp
	vktProtectedMemShaderImageAccessTests.cpp
	vktProtectedMemShaderImageAccessTests.hpp
	vktProtectedMemWorkgroupStorageTests.cpp
	vktProtectedMemWorkgroupStorageTests.hpp
	vktProtectedMemTests.cpp
	vktProtectedMemTests.hpp
	vktProtectedMemStackTests.cpp
	vktProtectedMemStackTests.hpp
	vktProtectedMemYCbCrConversionTests.cpp
	vktProtectedMemYCbCrConversionTests.hpp
	)

set(DEQP_VK_PROTECTED_MEMORY_SRCS
	vktProtectedMemWsiSwapchainTests.cpp
	vktProtectedMemWsiSwapchainTests.hpp
	)

PCH(DEQP_VK_PROTECTED_MEMORY_SRCS ../pch.cpp)

add_library(deqp-vk-protected-memory STATIC ${DEQP_VK_VKSC_PROTECTED_MEMORY_SRCS} ${DEQP_VK_PROTECTED_MEMORY_SRCS})
target_link_libraries(deqp-vk-protected-memory tcutil vkutil)

add_library(deqp-vksc-protected-memory STATIC ${DEQP_VK_VKSC_PROTECTED_MEMORY_SRCS})
target_link_libraries(deqp-vksc-protected-memory PUBLIC deqp-vksc-util tcutil vkscutil)
