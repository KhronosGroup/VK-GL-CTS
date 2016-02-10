/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Build and Device Tests
 *//*--------------------------------------------------------------------*/

#include "vktInfoTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPlatform.hpp"
#include "vkApiVersion.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "deStringUtil.hpp"

namespace vkt
{

namespace
{

using tcu::TestLog;
using std::string;

std::string getOsName (int os)
{
	switch (os)
	{
		case DE_OS_VANILLA:		return "DE_OS_VANILLA";
		case DE_OS_WIN32:		return "DE_OS_WIN32";
		case DE_OS_UNIX:		return "DE_OS_UNIX";
		case DE_OS_WINCE:		return "DE_OS_WINCE";
		case DE_OS_OSX:			return "DE_OS_OSX";
		case DE_OS_ANDROID:		return "DE_OS_ANDROID";
		case DE_OS_SYMBIAN:		return "DE_OS_SYMBIAN";
		case DE_OS_IOS:			return "DE_OS_IOS";
		default:
			return de::toString(os);
	}
}

std::string getCompilerName (int compiler)
{
	switch (compiler)
	{
		case DE_COMPILER_VANILLA:	return "DE_COMPILER_VANILLA";
		case DE_COMPILER_MSC:		return "DE_COMPILER_MSC";
		case DE_COMPILER_GCC:		return "DE_COMPILER_GCC";
		case DE_COMPILER_CLANG:		return "DE_COMPILER_CLANG";
		default:
			return de::toString(compiler);
	}
}

std::string getCpuName (int cpu)
{
	switch (cpu)
	{
		case DE_CPU_VANILLA:	return "DE_CPU_VANILLA";
		case DE_CPU_ARM:		return "DE_CPU_ARM";
		case DE_CPU_X86:		return "DE_CPU_X86";
		case DE_CPU_X86_64:		return "DE_CPU_X86_64";
		case DE_CPU_ARM_64:		return "DE_CPU_ARM_64";
		case DE_CPU_MIPS:		return "DE_CPU_MIPS";
		case DE_CPU_MIPS_64:	return "DE_CPU_MIPS_64";
		default:
			return de::toString(cpu);
	}
}

std::string getEndiannessName (int endianness)
{
	switch (endianness)
	{
		case DE_BIG_ENDIAN:		return "DE_BIG_ENDIAN";
		case DE_LITTLE_ENDIAN:	return "DE_LITTLE_ENDIAN";
		default:
			return de::toString(endianness);
	}
}

tcu::TestStatus logBuildInfo (Context& context)
{
#if defined(DE_DEBUG)
	const bool	isDebug	= true;
#else
	const bool	isDebug	= false;
#endif

	context.getTestContext().getLog()
		<< TestLog::Message
		<< "DE_OS: " << getOsName(DE_OS) << "\n"
		<< "DE_CPU: " << getCpuName(DE_CPU) << "\n"
		<< "DE_PTR_SIZE: " << DE_PTR_SIZE << "\n"
		<< "DE_ENDIANNESS: " << getEndiannessName(DE_ENDIANNESS) << "\n"
		<< "DE_COMPILER: " << getCompilerName(DE_COMPILER) << "\n"
		<< "DE_DEBUG: " << (isDebug ? "true" : "false") << "\n"
		<< TestLog::EndMessage;

	return tcu::TestStatus::pass("Not validated");
}

tcu::TestStatus logDeviceInfo (Context& context)
{
	TestLog&								log			= context.getTestContext().getLog();
	const vk::VkPhysicalDeviceProperties&	properties	= context.getDeviceProperties();

	log << TestLog::Message
		<< "Using --deqp-vk-device-id="
		<< context.getTestContext().getCommandLine().getVKDeviceId()
		<< TestLog::EndMessage;

	log << TestLog::Message
		<< "apiVersion: " << vk::unpackVersion(properties.apiVersion) << "\n"
		<< "driverVersion: " << tcu::toHex(properties.driverVersion) << "\n"
		<< "deviceName: " << (const char*)properties.deviceName << "\n"
		<< "vendorID: " << tcu::toHex(properties.vendorID) << "\n"
		<< "deviceID: " << tcu::toHex(properties.deviceID) << "\n"
		<< TestLog::EndMessage;

	return tcu::TestStatus::pass("Not validated");
}

tcu::TestStatus logPlatformInfo (Context& context)
{
	std::ostringstream details;

	context.getTestContext().getPlatform().getVulkanPlatform().describePlatform(details);

	context.getTestContext().getLog()
		<< TestLog::Message
		<< details.str()
		<< TestLog::EndMessage;

	return tcu::TestStatus::pass("Not validated");
}

} // anonymous

void createInfoTests (tcu::TestCaseGroup* testGroup)
{
	addFunctionCase(testGroup, "build",		"Build Info",		logBuildInfo);
	addFunctionCase(testGroup, "device",	"Device Info",		logDeviceInfo);
	addFunctionCase(testGroup, "platform",	"Platform Info",	logPlatformInfo);
}

} // vkt
