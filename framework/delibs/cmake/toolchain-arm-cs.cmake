# Platform defines.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CROSSCOMPILING 1)
set(DE_CPU "DE_CPU_ARM")

# Toolchain/compiler base.
set(ARM_CC_BASE "/opt/arm-2011.03" CACHE STRING "CodeSourcery GCC cross-compiler path")
set(CROSS_COMPILE "${ARM_CC_BASE}/bin/arm-none-linux-gnueabi-" CACHE STRING "Cross compiler prefix")

set(CMAKE_C_COMPILER "${CROSS_COMPILE}gcc")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILE}g++")

# Search libs and include files (but not programs) from toolchain dir.
set(CMAKE_FIND_ROOT_PATH "${ARM_CC_BASE}/arm-none-linux-gnueabi/libc/" ${ARM_CC_EXTRA_ROOT_PATHS})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
