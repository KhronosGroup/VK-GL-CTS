# This is a modified version of external/vulkan-video-samples/src/cmake/FindShaderc.cmake which
# does not force using system shaderc. See https://gitlab.khronos.org/Tracker/vk-gl-cts/-/issues/6220
# When this is resolved inside vulkan-video-samples this file can be removed.

# Now check if we need to build shaderc and its dependencies
if( USE_SYSTEM_SHADERC)
    if(WIN32)
        # Try to find shaderc in Vulkan SDK Bin directory
        if(DEFINED ENV{VULKAN_SDK})
            # Normalize path
            file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" VULKAN_SDK_PATH)

            # Look in the SDK's Bin directory
            find_library(SHADERC_SHARED_LIBRARY NAMES shaderc_shared
                         PATHS "${VULKAN_SDK_PATH}/lib"
                         NO_DEFAULT_PATH)

            if(SHADERC_SHARED_LIBRARY)
                message(STATUS "VVS: Found shaderc at: ${SHADERC_SHARED_LIBRARY}")
                set(shaderc_FOUND TRUE)
                # Store the Bin directory for later use
                get_filename_component(VULKAN_SDK_BIN_DIR "${SHADERC_SHARED_LIBRARY}" DIRECTORY)
                message(STATUS "VVS: Vulkan SDK Bin directory: ${VULKAN_SDK_BIN_DIR}")
            else()
                message(STATUS "VVS: Could not find shaderc_shared.dll in ${VULKAN_SDK_PATH}/Bin")
            endif()
        endif()
    else()
        find_library(SHADERC_SHARED_LIBRARY NAMES shaderc_shared shaderc)

        if(SHADERC_SHARED_LIBRARY)
            message(STATUS "VVS: Found shaderc at: ${SHADERC_SHARED_LIBRARY}")
            set(shaderc_FOUND TRUE)
            # Store the Bin directory for later use
            get_filename_component(VULKAN_SDK_BIN_DIR "${SHADERC_SHARED_LIBRARY}" DIRECTORY)
            message(STATUS "Vulkan SDK Bin directory: ${VULKAN_SDK_BIN_DIR}")
        else()
            message(STATUS "VVS: Could not find libshaderc_shared.so in filesystem")
	endif()
    endif()

    if(shaderc_FOUND)
        message(STATUS "VVS: Found system shaderc")
    else()
        message(STATUS "VVS: System shaderc not found")
        if(WIN32)
            message(STATUS "VVS: Make sure Vulkan SDK is installed and VULKAN_SDK environment variable is set")
        endif()
    endif()
endif()

if(USE_SYSTEM_SHADERC AND shaderc_FOUND)
    message(STATUS "VVS: Using system shaderc")
    set(SHADERC_LIB "")
else()
    set(SHADERC_LIB "shaderc_shared" CACHE PATH "The name of the shaderc library target decoder/encoder are using." FORCE)
    message(STATUS "VVS: Building shaderc from source using existing dependencies")
    set(SHADERC_VERSION v2024.4)

    # Define minimum version requirements for shaderc v2024.4 compatibility
    # These versions are based on shaderc v2024.4's known dependencies
    set(REQUIRED_GLSLANG_VERSION "14.0.0")
    set(VULKAN_SDK_VERSION vulkan-sdk-1.4.313)

    # Check if we already have SPIRV-Tools and glslang targets from the main project
    if(TARGET SPIRV-Tools)
        message(STATUS "VVS: Using existing SPIRV-Tools target from main project")
        message(WARNING "VVS: Cannot determine version - assuming compatible with shaderc ${SHADERC_VERSION}")
        set(USE_EXISTING_SPIRV_TOOLS TRUE)
        # When using existing targets, no need to set library directories
        # The target already contains all necessary information
    else()
        message(STATUS "VVS: SPIRV-Tools target not found - will build from source")
        set(USE_EXISTING_SPIRV_TOOLS FALSE)
    endif()

    if(TARGET glslang)
        # Try to detect glslang version if possible
        set(USE_EXISTING_GLSLANG TRUE)

        # Check if we can find version info from build_info.h
        set(GLSLANG_VERSION_FOUND FALSE)

        # Check for build_info.h (generated during glslang build)
        if(EXISTS "${CMAKE_BINARY_DIR}/include/glslang/build_info.h")

            # Read version lines separately
            file(STRINGS "${CMAKE_BINARY_DIR}/include/glslang/build_info.h" GLSLANG_MAJOR_LINE
                 REGEX "^#define GLSLANG_VERSION_MAJOR")
            file(STRINGS "${CMAKE_BINARY_DIR}/include/glslang/build_info.h" GLSLANG_MINOR_LINE
                 REGEX "^#define GLSLANG_VERSION_MINOR")
            file(STRINGS "${CMAKE_BINARY_DIR}/include/glslang/build_info.h" GLSLANG_PATCH_LINE
                 REGEX "^#define GLSLANG_VERSION_PATCH")

            set(GLSLANG_MAJOR "")
            set(GLSLANG_MINOR "")
            set(GLSLANG_PATCH "")

            # Extract version numbers
            if(GLSLANG_MAJOR_LINE)
                string(REGEX MATCH "([0-9]+)" GLSLANG_MAJOR "${GLSLANG_MAJOR_LINE}")
            endif()
            if(GLSLANG_MINOR_LINE)
                string(REGEX MATCH "([0-9]+)" GLSLANG_MINOR "${GLSLANG_MINOR_LINE}")
            endif()
            if(GLSLANG_PATCH_LINE)
                string(REGEX MATCH "([0-9]+)" GLSLANG_PATCH "${GLSLANG_PATCH_LINE}")
            endif()

            if(NOT "${GLSLANG_MAJOR}" STREQUAL "" AND NOT "${GLSLANG_MINOR}" STREQUAL "" AND NOT "${GLSLANG_PATCH}" STREQUAL "")
                set(GLSLANG_VERSION "${GLSLANG_MAJOR}.${GLSLANG_MINOR}.${GLSLANG_PATCH}")
                set(GLSLANG_VERSION_FOUND TRUE)
                message(STATUS "VVS: Found existing glslang target from main project (version: ${GLSLANG_VERSION})")
                # Check if major version is at least 14 (minimum for shaderc v2024.4)
                if(GLSLANG_MAJOR GREATER_EQUAL 14)
                    message(STATUS "VVS: glslang ${GLSLANG_VERSION} should be compatible with shaderc ${SHADERC_VERSION}")
                else()
                    message(WARNING "VVS: glslang ${GLSLANG_VERSION} may not be compatible with shaderc ${SHADERC_VERSION}")
                    message(WARNING "VVS: Required minimum version: ${REQUIRED_GLSLANG_VERSION}")
                    message(WARNING "VVS: Consider setting USE_EXISTING_GLSLANG to FALSE if build fails")
                endif()
            endif()
        endif()

        if(NOT GLSLANG_VERSION_FOUND)
            message(STATUS "VVS: Using existing glslang target from main project")
            message(WARNING "VVS: Cannot determine version - assuming compatible with shaderc ${SHADERC_VERSION}")
            message(STATUS "VVS: Note: Version info will be available after glslang is built")
        endif()

        # When using existing targets, no need to set library directories
        # The target already contains all necessary information
    else()
        message(STATUS "VVS: glslang target not found - will build from source")
        set(USE_EXISTING_GLSLANG FALSE)
    endif()

    # If building standalone, need to fetch and build dependencies
    if(NOT USE_EXISTING_SPIRV_TOOLS OR NOT USE_EXISTING_GLSLANG)
        # VULKAN_SDK_VERSION already defined above with other version requirements

        # Fetch SPIRV-Headers first (needed by SPIRV-Tools)
        FetchContent_Declare(
            spirv-headers
            GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
            GIT_TAG ${VULKAN_SDK_VERSION}
        )
        FetchContent_MakeAvailable(spirv-headers)

        if(NOT USE_EXISTING_SPIRV_TOOLS)
            # SPIRV-Tools settings
            set(SPIRV_SKIP_TESTS ON CACHE BOOL "Disable SPIRV-Tools tests" FORCE)
            set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Disable SPIRV-Tools executables" FORCE)
            set(SPIRV_BUILD_SHARED ON CACHE BOOL "Build shared SPIRV-Tools" FORCE)
            set(SPIRV_USE_STATIC_LIBS OFF CACHE BOOL "Use dynamic CRT for SPIRV-Tools" FORCE)
            set(SPIRV_TOOLS_INSTALL_EMACS_HELPERS OFF CACHE BOOL "Skip emacs helpers" FORCE)
            set(SPIRV_TOOLS_BUILD_STATIC OFF CACHE BOOL "Build static SPIRV-Tools" FORCE)
            set(SPIRV_TOOLS_BUILD_SHARED ON CACHE BOOL "Build shared SPIRV-Tools" FORCE)
            set(SPIRV_WERROR OFF CACHE BOOL "Enable error on warning" FORCE)

            # Fetch SPIRV-Tools (required for Shaderc)
            FetchContent_Declare(
                spirv-tools
                GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
                GIT_TAG ${VULKAN_SDK_VERSION}
            )
            FetchContent_MakeAvailable(spirv-tools)

            set(SPIRV_TOOLS_LIBRARY_DIR "${CMAKE_BINARY_DIR}/_deps/spirv-tools-build/lib")
            set(SPIRV_TOOLS_BINARY_ROOT "${spirv-tools_BINARY_DIR}")
        endif()

        if(NOT USE_EXISTING_GLSLANG)
            # GLSLang settings
            set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "Disable GLSLang binaries" FORCE)
            set(ENABLE_SPVREMAPPER ON CACHE BOOL "Disable SPVREMAPPER" FORCE)
            set(ENABLE_GLSLANG_JS OFF CACHE BOOL "Disable JavaScript" FORCE)
            set(ENABLE_GLSLANG_WEBMIN OFF CACHE BOOL "Disable WebMin" FORCE)
            set(ENABLE_GLSLANG_WEB OFF CACHE BOOL "Disable Web" FORCE)
            set(ENABLE_GLSLANG_WEB_DEVEL OFF CACHE BOOL "Disable Web Development" FORCE)
            set(ENABLE_HLSL ON CACHE BOOL "Enable HLSL" FORCE)
            set(GLSLANG_ENABLE_WERROR OFF CACHE BOOL "Enable error on warning" FORCE)

            # Configure glslang to find SPIRV-Tools
            set(ENABLE_OPT ON CACHE BOOL "Enable SPIRV-Tools optimizer" FORCE)
            set(ALLOW_EXTERNAL_SPIRV_TOOLS ON CACHE BOOL "Allow external SPIRV-Tools" FORCE)
            set(SPIRV_TOOLS_BINARY_ROOT "${SPIRV_TOOLS_BINARY_ROOT}" CACHE PATH "SPIRV-Tools binary root" FORCE)
            set(SPIRV_TOOLS_OPT_LIBRARY_PATH "${SPIRV_TOOLS_BINARY_ROOT}/source/opt" CACHE PATH "SPIRV-Tools opt library path" FORCE)

            # Fetch GLSLang (required for Shaderc)
            FetchContent_Declare(
                glslang
                GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
                GIT_TAG ${VULKAN_SDK_VERSION}
            )
            FetchContent_MakeAvailable(glslang)

            set(GLSLANG_LIBRARY_DIR "${CMAKE_BINARY_DIR}/_deps/glslang-build")
            set(GLSLANG_BINARY_ROOT "${glslang_BINARY_DIR}")
        endif()

        # Ensure the linker knows where to find these libraries
        link_directories(${SPIRV_TOOLS_LIBRARY_DIR})
        link_directories(${GLSLANG_LIBRARY_DIR})
    endif()

    # Shaderc settings
    set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "Skip examples" FORCE)
    set(SHADERC_SKIP_COPYRIGHT_CHECK ON CACHE BOOL "Disable Shaderc copyright check" FORCE)
    set(SHADERC_SKIP_TESTS ON CACHE BOOL "Skip tests" FORCE)
    set(SHADERC_ENABLE_SHARED_CRT ON CACHE BOOL "Use shared CRT" FORCE)
    set(SHADERC_STATIC_CRT OFF CACHE BOOL "Don't use static CRT" FORCE)
    set(SHADERC_ENABLE_WERROR OFF CACHE BOOL "Enable error on warning" FORCE)

    # Enable glslc build explicitly
    set(SHADERC_SKIP_INSTALL OFF CACHE BOOL "Don't skip installation" FORCE)
    set(SHADERC_ENABLE_GLSLC ON CACHE BOOL "Enable glslc" FORCE)
    set(SHADERC_ENABLE_INSTALL ON CACHE BOOL "Enable install" FORCE)

    if(USE_EXISTING_GLSLANG)
        set(SHADERC_GLSLANG_DIR ${CMAKE_SOURCE_DIR}/external/glslang/src CACHE PATH "Source directory for glslang" FORCE)
    else()
        set(SHADERC_GLSLANG_DIR ${glslang_SOURCE_DIR} CACHE PATH "Source directory for glslang" FORCE)
    endif()
    set(SHADERC_THIRD_PARTY_ROOT_DIR ${shaderc_SOURCE_DIR}/third_party CACHE PATH "Root location of shaderc third party dependencies" FORCE)

    # Fetch shaderc
    FetchContent_Declare(
        shaderc
        GIT_REPOSITORY https://github.com/google/shaderc.git
        GIT_TAG ${SHADERC_VERSION}
    )
    FetchContent_MakeAvailable(shaderc)

    # Disable werror for all shaderc targets when using GCC or Clang
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(TARGET shaderc_util)
            target_compile_options(shaderc_util PRIVATE -Wno-error -Wno-shadow)
        endif()
        if(TARGET shaderc)
            target_compile_options(shaderc PRIVATE -Wno-error -Wno-shadow)
        endif()
        if(TARGET shaderc_shared)
            target_compile_options(shaderc_shared PRIVATE -Wno-error -Wno-shadow)
        endif()
    endif()

    set(SHADERC_LIBRARY_DIR "${CMAKE_BINARY_DIR}/_deps/shaderc-build/libshaderc")
    # Ensure the linker knows where to find these libraries
    link_directories(${SHADERC_LIBRARY_DIR})

    set(SHADERC_SHARED_LIBRARY ${SHADERC_LIB})

    # Set explicit dependencies - shaderc depends on existing targets
    if(TARGET shaderc_shared)
        add_dependencies(shaderc_shared glslang SPIRV-Tools)
    endif()

    find_path(SHADERC_INCLUDE_DIR NAMES shaderc/shaderc.h PATHS "${CMAKE_BINARY_DIR}/_deps/shaderc-src/libshaderc/include" NO_DEFAULT_PATH)
    message(STATUS "VVS: shaderc include directory: " ${SHADERC_INCLUDE_DIR})

    # After all the FetchContent_MakeAvailable calls and dependencies setup, add:
    if(WIN32)
        # Install Shaderc
        install(DIRECTORY "${shaderc_BINARY_DIR}/libshaderc/$<CONFIG>/"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
                FILES_MATCHING
                PATTERN "*.lib"
                PATTERN "*.dll")
        install(DIRECTORY "${shaderc_BINARY_DIR}/libshaderc/$<CONFIG>/"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                FILES_MATCHING
                PATTERN "*.dll")
        # Install glslc
        install(DIRECTORY "${shaderc_BINARY_DIR}/glslc/$<CONFIG>/"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                FILES_MATCHING
                PATTERN "glslc.exe")
    endif()
endif()

# Add after finding Vulkan SDK
# Find Shaderc
if(DEFINED ENV{VULKAN_SDK})
    # Try to find shaderc in Vulkan SDK first
    find_path(SHADERC_INCLUDE_DIR
        NAMES shaderc/shaderc.h
        PATHS
        "$ENV{VULKAN_SDK}/Include"
        "$ENV{VULKAN_SDK}/include"
        NO_DEFAULT_PATH
    )

    find_library(SHADERC_LIBRARY
        NAMES shaderc_combined
        PATHS
        "$ENV{VULKAN_SDK}/Lib"
        "$ENV{VULKAN_SDK}/lib"
        NO_DEFAULT_PATH
    )
endif()

# If not found in SDK, try system paths
if(NOT SHADERC_INCLUDE_DIR)
    find_path(SHADERC_INCLUDE_DIR
        NAMES shaderc/shaderc.h
    )
endif()

if(NOT SHADERC_LIBRARY)
    find_library(SHADERC_LIBRARY
        NAMES shaderc_combined
    )
endif()

# If still not found, build from source
if(NOT SHADERC_INCLUDE_DIR OR NOT SHADERC_LIBRARY)
    message(STATUS "VVS: Shaderc not found in SDK or system, building from source...")
    include(FetchContent)
    FetchContent_Declare(
        shaderc
        GIT_REPOSITORY https://github.com/google/shaderc
        GIT_TAG ${SHADERC_VERSION}
    )

    set(SHADERC_SKIP_TESTS ON)
    set(SHADERC_SKIP_EXAMPLES ON)
    set(SHADERC_SKIP_COPYRIGHT_CHECK ON)

    FetchContent_MakeAvailable(shaderc)

    set(SHADERC_INCLUDE_DIR ${shaderc_SOURCE_DIR}/libshaderc/include)
    set(SHADERC_LIBRARY shaderc)
endif()

if(SHADERC_INCLUDE_DIR AND SHADERC_LIBRARY)
    message(STATUS "VVS: Found Shaderc: ${SHADERC_LIBRARY}")
    message(STATUS "VVS: Shaderc include: ${SHADERC_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "VVS: Could not find or build Shaderc")
endif()
