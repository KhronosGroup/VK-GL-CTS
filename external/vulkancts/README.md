Vulkan CTS README
=================

This document describes how to build and run Vulkan Conformance Test suite.

Vulkan CTS is built on dEQP framework. General dEQP documentation is available
at http://source.android.com/devices/graphics/testing.html


Requirements
------------

Common:
 * Git (for checking out sources)
 * Python 2.7.x (all recent versions in 2.x should work, 3.x is not supported)
 * CMake 2.8 or newer

Win32:
 * Visual Studio 2013 (glslang uses several C++11 features)

Linux:
 * Standard toolchain (make, gcc/clang)


Building
--------

To build dEQP, you need first to download sources for zlib, libpng, and glslang.

To download zlib and libpng, run:

$ python external/fetch_sources.py

Glslang is optional, but enables GLSL to SPIR-V compilation. Check out glslang
from the official repository to directory next to 'deqp':

$ cd .. # assuming you were in 'deqp' source directory
$ git clone https://github.com/KhronosGroup/glslang.git glslang

spirv-tools is optional, but enables SPIRV assembly. Check out spirv-tools
from the google branch of the gitlab repository next to 'deqp':

$ cd .. # assuming you were in 'deqp' source directory
$ git clone -b google https://gitlab.khronos.org/spirv/spirv-tools.git

I.e the final directory structure should look like this:

src/
    deqp/
    glslang/
    spirv-tools/

After downloading all dependencies, please follow instructions at
http://source.android.com/devices/graphics/build-tests.html

NOTE: glslang integration is not yet available on Android due to a toolchain
bug, so pre-compiled SPIR-V binaries must be used. See instructions below.


Running
-------

Win32:

> cd builddir/external/vulkancts/modules/vulkan
> Debug/deqp-vk.exe

Linux:

$ cd builddir/external/vulkancts/modules/vulkan
$ ./deqp-vk

Android:

Using Cherry is recommended. Alternatively you can follow instructions at
http://source.android.com/devices/graphics/run-tests.html


Pre-compiling SPIR-V binaries
-----------------------------

For distribution, and platforms that don't support GLSL to SPIR-V compilation,
SPIR-V binaries must be pre-built with following command:

$ python external/vulkancts/build_spirv_binaries.py

Binaries will be written to external/vulkancts/data/vulkan/prebuilt/.

Test modules (or in case of Android, the APK) must be re-built after building
SPIR-V programs in order for the binaries to be available.


Vulkan platform port
--------------------

Vulkan support from Platform implementation requires providing
getVulkanPlatform() method in tcu::Platform class implementation.

See framework/common/tcuPlatform.hpp and examples in
framework/platform/win32/tcuWin32Platform.cpp and
framework/platform/android/tcuAndroidPlatform.cpp.


Null (dummy) driver
-------------------

For testing and development purposes it might be useful to be able to run
tests on dummy Vulkan implementation. One such implementation is provided in
vkNullDriver.cpp. To use that, implement vk::Platform::createLibrary() with
vk::createNullDriver().


Cherry GUI
----------

Vulkan test module can be used with Cherry (GUI for test execution and
analysis). Cherry is available at
https://android.googlesource.com/platform/external/cherry. Please follow
instructions in README to get started.

To enable support for Vulkan tests, dEQP-VK module must be added to list of
test packages.

In cherry/testrunner.go, add following line to testPackageDescriptors list
(line 608 in NewTestRunner function):

{"dEQP-VK", "deqp-vk", "../external/vulkancts/modules/vulkan", dataDir + "dEQP-VK-cases.xml"},

Before first launch, and every time test hierarchy has been modified, test
case list must be refreshed by running:

$ python scripts/build_caselists.py path/to/cherry/data

Cherry must be restarted for the case list update to take effect.
