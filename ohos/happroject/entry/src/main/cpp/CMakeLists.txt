# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(VncViewer)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/common
                    ${NATIVERENDER_ROOT_PATH}/libvncserver/include
                    ${NATIVERENDER_ROOT_PATH}/napi
                    ${NATIVERENDER_ROOT_PATH}/render
                    ${NATIVERENDER_ROOT_PATH}/ccrdp
)

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
#set(WITH_IPv6 OFF)
#add_subdirectory(FreeRDP/buildcc)

add_library(entry SHARED
            render/egl_core.cpp
            render/plugin_render.cpp
            render/app_context.cpp
            plugin_manager.cpp
            napi/napi_init.cpp
            render/napi_callback.cpp
            )

target_link_directories(entry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/arm64-v8a)

target_link_libraries(entry PUBLIC libEGL.so
    libGLESv3.so libace_napi.z.so libhilog_ndk.z.so
    libace_ndk.z.so libace_napi.z.so libuv.so libnative_vsync.so
    libvk_app_context.z.so libglcts_app.z.so librawfile.z.so)