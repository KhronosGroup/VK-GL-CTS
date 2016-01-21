Vulkan CTS README
=================

This document describes how to build and run Vulkan Conformance Test suite.

Vulkan CTS is built on dEQP framework. dEQP documentation is available
at http://source.android.com/devices/graphics/testing.html


Requirements
------------

Common:
 * Git (for checking out sources)
 * Python 2.7.x (all recent versions in 2.x should work, 3.x is not supported)
 * CMake 2.8 or newer

Win32:
 * Visual Studio 2013 or newer (glslang uses several C++11 features)

Linux:
 * Standard toolchain (make, gcc/clang)

Android:
 * Android NDK r10e
 * Android SDK with following packages:
   + SDK Tools
   + SDK Platform-tools
   + SDK Build-tools
   + API 22
 * Apache Ant
 * Windows: either NMake or JOM in PATH


Building CTS
------------

To build dEQP, you need first to download sources for zlib, libpng, glslang,
and spirv-tools.

To download sources, run:

$ python external/fetch_sources.py

You may need to re-run fetch_sources.py to update to the latest glslang and
spirv-tools revisions occasionally.

NOTE: glslang integration is not yet available on Android due to a toolchain
bug, so pre-compiled SPIR-V binaries must be used. See instructions below.

With CMake out-of-source builds are always recommended. Create a build directory
of your choosing, and in that directory generate Makefiles or IDE project
using cmake.


Windows x86-32:

	> cmake <path to vulkancts> -G"Visual Studio 12"
	> start dEQP-Core-default.sln


Windows x86-64:

	> cmake <path to vulkancts> -G"Visual Studio 12 Win64"
	> start dEQP-Core-default.sln


Linux 32-bit Debug:

	$ cmake <path to vulkancts> -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32
	$ make -j

Release build can be done by using -DCMAKE_BUILD_TYPE=Release


Linux 64-bit Debug:

	$ cmake <path to vulkancts> -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64
	$ make -j


Android:

	$ python android/scripts/build.py
	$ python android/scripts/install.py


Building Mustpass
-----------------

Vulkan CTS mustpass can be built by running:

	$ python <vulkancts>/external/vulkancts/build_mustpass.py


Running CTS
-----------

Following command line options MUST be used when running CTS:

	--deqp-caselist-file=<vulkancts>/external/vulkancts/mustpass/1.0.0/vk-default.txt
	--deqp-log-images=disable
	--deqp-log-shader-sources=disable

In addition on multi-device systems the device for which conformance is claimed
can be selected with:

	--deqp-vk-device-id=<value>

No other command line options are allowed.


Win32:

	> cd <builddir>/external/vulkancts/modules/vulkan
	> Debug/deqp-vk.exe --deqp-caselist-file=...

Test log will be written into TestResults.qpa


Linux:

	$ cd <builddir>/external/vulkancts/modules/vulkan
	$ ./deqp-vk --deqp-vk-caselist-file=...


Android:

	$ adb push <vulkancts>/external/vulkancts/mustpass/1.0.0/vk-default.txt /sdcard/vk-default.txt
	$ adb shell

In device shell:

	$ am start -n com.drawelements.deqp/android.app.NativeActivity -e cmdLine "deqp --deqp-caselist-file=/sdcard/vk-default.txt --deqp-log-images=disable --deqp-log-filename=/sdcard/TestResults.qpa"

Process can be followed by running:

	$ adb logcat -s dEQP

Test log will be written into /sdcard/TestResults.qpa


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
