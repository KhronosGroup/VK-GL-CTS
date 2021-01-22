# vk - Vulkan utilites
# Split into two libraries - one that depends on spirv/glslang libraries
# which have long build times, and one that can build in parallel with those.

include(CheckIncludeFileCXX)
check_include_file_cxx(
	${CMAKE_SOURCE_DIR}/external/renderdoc/src/renderdoc_app.h
	HAVE_RENDERDOC_APP_H)
if(HAVE_RENDERDOC_APP_H)
	set(VKRENDERDOC_SRC vkRenderDocUtil.cpp)
else()
	set(VKRENDERDOC_SRC vkNoRenderDocUtil.cpp)
endif(HAVE_RENDERDOC_APP_H)

set(VKUTILNOSHADER_SRCS
	vkApiVersion.cpp
	vkApiVersion.hpp
	vkBuilderUtil.cpp
	vkBuilderUtil.hpp
	vkBarrierUtil.cpp
	vkBarrierUtil.hpp
	vkCmdUtil.cpp
	vkCmdUtil.hpp
	vkDefs.cpp
	vkDefs.hpp
	vkRef.cpp
	vkRef.hpp
	vkRefUtil.cpp
	vkRefUtil.hpp
	vkPlatform.cpp
	vkPlatform.hpp
	vkStrUtil.cpp
	vkStrUtil.hpp
	vkQueryUtil.cpp
	vkQueryUtil.hpp
	vkMemUtil.cpp
	vkMemUtil.hpp
	vkDeviceUtil.cpp
	vkDeviceUtil.hpp
	vkBinaryRegistry.cpp
	vkBinaryRegistry.hpp
	vkNullDriver.cpp
	vkNullDriver.hpp
	vkImageUtil.cpp
	vkImageUtil.hpp
	vkTypeUtil.cpp
	vkTypeUtil.hpp
	vkAllocationCallbackUtil.cpp
	vkAllocationCallbackUtil.hpp
	vkWsiPlatform.cpp
	vkWsiPlatform.hpp
	vkWsiUtil.cpp
	vkWsiUtil.hpp
	vkDebugReportUtil.cpp
	vkDebugReportUtil.hpp
	vkBufferWithMemory.cpp
	vkBufferWithMemory.hpp
	vkImageWithMemory.cpp
	vkImageWithMemory.hpp
	vkImageWithMemory.cpp
	vkImageWithMemory.hpp
	vkShaderProgram.cpp
	vkShaderProgram.hpp
	vkValidatorOptions.hpp
	vkYCbCrImageWithMemory.cpp
	vkYCbCrImageWithMemory.hpp
	vkObjUtil.cpp
	vkObjUtil.hpp
	${VKRENDERDOC_SRC}
	vkRenderDocUtil.hpp
	vkDeviceFeatures.hpp
	vkDeviceFeatures.cpp
	vkDeviceProperties.hpp
	vkDeviceProperties.cpp
	vkRayTracingUtil.hpp
	vkRayTracingUtil.cpp
	)

set(VKUTILNOSHADER_INLS
	generated/vulkan/vkDeviceFeatures.inl
	generated/vulkan/vkDeviceProperties.inl
	)

set(VKSCUTILNOSHADER_INLS
	generated/vulkansc/vkDeviceFeatures.inl
	generated/vulkansc/vkDeviceProperties.inl
	)

set(VKUTIL_SRCS
	vkPrograms.cpp
	vkPrograms.hpp
	vkShaderToSpirV.cpp
	vkShaderToSpirV.hpp
	vkSpirVAsm.hpp
	vkSpirVAsm.cpp
	vkSpirVProgram.hpp
	vkSpirVProgram.cpp
	)

set(VKUTILNOSHADER_LIBS
	glutil
	tcutil
	)

# \note Code interfacing with glslang needs to include third-party headers
#       that cause all sorts of warnings to appear.
if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	set_source_files_properties(
		FILES vkShaderToSpirV.cpp
		PROPERTIES COMPILE_FLAGS "${DE_3RD_PARTY_CXX_FLAGS}")
endif ()


# \note Code interfacing with spirv-opt needs to include third-party headers
#       that cause all sorts of warnings to appear.
if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	set_source_files_properties(
		FILES vkPrograms.cpp
		PROPERTIES COMPILE_FLAGS "${DE_3RD_PARTY_CXX_FLAGS}")
endif ()

PCH(VKUTILNOSHADER_SRCS ../../modules/vulkan/pch.cpp)
PCH(VKUTIL_SRCS ../../modules/vulkan/pch.cpp)
PCH(VKUTILNOSHADER_INLS ../../modules/vulkan/pch.cpp)
PCH(VKSCUTILNOSHADER_INLS ../../modules/vulkan/pch.cpp)

add_library(vkutilnoshader STATIC ${VKUTILNOSHADER_SRCS} ${VKUTILNOSHADER_INLS})
target_compile_definitions(vkutilnoshader PUBLIC CTS_USES_VULKAN)
target_link_libraries(vkutilnoshader PUBLIC ${VKUTILNOSHADER_LIBS})

add_library(vkscutilnoshader STATIC ${VKUTILNOSHADER_SRCS} ${VKSCUTILNOSHADER_INLS})
target_include_directories(vkscutilnoshader BEFORE PUBLIC generated/vulkansc)
target_compile_definitions(vkscutilnoshader PUBLIC CTS_USES_VULKANSC)
target_link_libraries(vkscutilnoshader PUBLIC ${VKUTILNOSHADER_LIBS})

if (DEQP_USE_X11)
	find_package(X11 REQUIRED)

	target_compile_definitions(vkutilnoshader PUBLIC DEQP_SUPPORT_X11=1)
	target_link_libraries(vkutilnoshader ${X11_LIBRARIES})

	target_compile_definitions(vkscutilnoshader PUBLIC DEQP_SUPPORT_X11=1)
	target_link_libraries(vkscutilnoshader ${X11_LIBRARIES})

	if (DEQP_USE_XCB)
		find_package(XCB REQUIRED)

		target_compile_definitions(vkutilnoshader PUBLIC DEQP_SUPPORT_XCB=1)
		target_link_libraries(vkutilnoshader ${XCB_LIBRARIES})

		target_compile_definitions(vkscutilnoshader PUBLIC DEQP_SUPPORT_XCB=1)
		target_link_libraries(vkscutilnoshader ${XCB_LIBRARIES})

	endif ()
endif()

add_library(vkutil STATIC ${VKUTIL_SRCS})
target_include_directories(vkutil PUBLIC ${GLSLANG_INCLUDE_PATH} ${spirv-tools_SOURCE_DIR}/include ${spirv-tools_SOURCE_DIR}/external/include )
target_link_libraries(vkutil PUBLIC vkutilnoshader ${GLSLANG_LIBRARIES} SPIRV-Tools SPIRV-Tools-opt)

add_library(vkscutil STATIC ${VKUTIL_SRCS})
target_include_directories(vkscutil BEFORE PUBLIC generated/vulkansc)
target_include_directories(vkscutil PUBLIC ${GLSLANG_INCLUDE_PATH} ${spirv-tools_SOURCE_DIR}/include ${spirv-tools_SOURCE_DIR}/external/include )
target_link_libraries(vkscutil PUBLIC vkscutilnoshader ${GLSLANG_LIBRARIES} SPIRV-Tools SPIRV-Tools-opt)
