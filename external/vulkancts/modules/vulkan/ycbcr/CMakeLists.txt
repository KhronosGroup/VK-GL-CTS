# YCbCr Tests

include_directories(
	..
	../shaderexecutor
	../util
	)

set(DEQP_VK_YCBCR_SRCS
	vktYCbCrConversionTests.cpp
	vktYCbCrConversionTests.hpp
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
	)

set(DEQP_VK_YCBCR_LIBS
	deqp-vk-shaderexecutor
	deqp-vk-util
	tcutil
	vkutil
	)

PCH(DEQP_VK_YCBCR_SRCS ../pch.cpp)

add_library(deqp-vk-ycbcr STATIC ${DEQP_VK_YCBCR_SRCS})
target_link_libraries(deqp-vk-ycbcr ${DEQP_VK_YCBCR_LIBS})
