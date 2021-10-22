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
	vkDeviceFeatures.inl
	vkDeviceProperties.hpp
	vkDeviceProperties.cpp
	vkDeviceProperties.inl
	vkRayTracingUtil.hpp
	vkRayTracingUtil.cpp
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

set(VKUTIL_LIBS
	glutil
	tcutil
	vkutilnoshader
	)

include_directories(${GLSLANG_INCLUDE_PATH})

# \note Code interfacing with glslang needs to include third-party headers
#       that cause all sorts of warnings to appear.
if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	set_source_files_properties(
		FILES vkShaderToSpirV.cpp
		PROPERTIES COMPILE_FLAGS "${DE_3RD_PARTY_CXX_FLAGS}")
endif ()

set(VKUTIL_LIBS ${VKUTIL_LIBS} ${GLSLANG_LIBRARIES})

include_directories(${spirv-tools_SOURCE_DIR}/include)
include_directories(${spirv-tools_SOURCE_DIR}/external/include)

# \note Code interfacing with spirv-opt needs to include third-party headers
#       that cause all sorts of warnings to appear.
if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	set_source_files_properties(
		FILES vkPrograms.cpp
		PROPERTIES COMPILE_FLAGS "${DE_3RD_PARTY_CXX_FLAGS}")
endif ()

set(VKUTIL_LIBS ${VKUTIL_LIBS} SPIRV-Tools SPIRV-Tools-opt)

if(DEQP_HAVE_RENDERDOC_HEADER)
	add_definitions(-DDEQP_HAVE_RENDERDOC_HEADER=1)
endif()

PCH(VKUTILNOSHADER_SRCS ../../modules/vulkan/pch.cpp)
PCH(VKUTIL_SRCS ../../modules/vulkan/pch.cpp)

add_library(vkutilnoshader STATIC ${VKUTILNOSHADER_SRCS})
target_link_libraries(vkutilnoshader ${VKUTILNOSHADER_LIBS})

if (DEQP_USE_X11)
	find_package(X11 REQUIRED)
	target_link_libraries(vkutilnoshader ${X11_LIBRARIES})
	add_definitions(-DDEQP_SUPPORT_X11=1)
	if (DEQP_USE_XCB)
		find_package(XCB REQUIRED)
		target_link_libraries(vkutilnoshader ${XCB_LIBRARIES})
		add_definitions(-DDEQP_SUPPORT_XCB=1)
	endif ()
endif()

add_library(vkutil STATIC ${VKUTIL_SRCS})
target_link_libraries(vkutil ${VKUTIL_LIBS})
