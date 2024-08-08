# YCbCr Tests

include_directories(
	..
	../shaderexecutor
	../util
	${DEQP_INL_DIR}
	)

set(DEQP_VK_YCBCR_SRCS
	vktYCbCrConversionTests.cpp
	vktYCbCrConversionTests.hpp
	vktYCbCrFilteringTests.cpp
	vktYCbCrFilteringTests.hpp
	vktYCbCrFormatTests.cpp
	vktYCbCrFormatTests.hpp
	vktYCbCrImageQueryTests.cpp
	vktYCbCrImageQueryTests.hpp
	vktYCbCrViewTests.cpp
	vktYCbCrViewTests.hpp
	vktYCbCrTests.cpp
	vktYCbCrTests.hpp
	vktYCbCrUtil.cpp
	vktYCbCrUtil.hpp
	vktYCbCrCopyTests.cpp
	vktYCbCrCopyTests.hpp
	vktYCbCrStorageImageWriteTests.cpp
	vktYCbCrStorageImageWriteTests.hpp
	vktYCbCrImageOffsetTests.cpp
	vktYCbCrImageOffsetTests.hpp
	)

PCH(DEQP_VK_YCBCR_SRCS ../pch.cpp)

add_library(deqp-vk-ycbcr STATIC ${DEQP_VK_YCBCR_SRCS})
target_link_libraries(deqp-vk-ycbcr deqp-vk-shaderexecutor deqp-vk-util tcutil vkutil)

add_library(deqp-vksc-ycbcr STATIC ${DEQP_VK_YCBCR_SRCS})
target_link_libraries(deqp-vksc-ycbcr PUBLIC deqp-vksc-shaderexecutor deqp-vksc-util tcutil vkscutil)
