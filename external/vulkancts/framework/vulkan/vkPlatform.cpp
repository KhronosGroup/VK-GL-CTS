/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \brief Vulkan platform abstraction.
 *//*--------------------------------------------------------------------*/

#include "vkPlatform.hpp"
#include "tcuFunctionLibrary.hpp"

namespace vk
{

PlatformDriver::PlatformDriver (const tcu::FunctionLibrary& library)
{
#define GET_PROC_ADDR(NAME) library.getFunction(NAME)
#include "vkInitPlatformFunctionPointers.inl"
#undef GET_PROC_ADDR
}

PlatformDriver::~PlatformDriver (void)
{
}

InstanceDriver::InstanceDriver (const PlatformInterface& platformInterface, VkInstance instance)
{
#define GET_PROC_ADDR(NAME) platformInterface.getInstanceProcAddr(instance, NAME)
#include "vkInitInstanceFunctionPointers.inl"
#undef GET_PROC_ADDR
}

InstanceDriver::~InstanceDriver (void)
{
}

DeviceDriver::DeviceDriver (const InstanceInterface& instanceInterface, VkDevice device)
{
#define GET_PROC_ADDR(NAME) instanceInterface.getDeviceProcAddr(device, NAME)
#include "vkInitDeviceFunctionPointers.inl"
#undef GET_PROC_ADDR
}

DeviceDriver::~DeviceDriver (void)
{
}

#include "vkPlatformDriverImpl.inl"
#include "vkInstanceDriverImpl.inl"
#include "vkDeviceDriverImpl.inl"

void Platform::describePlatform (std::ostream& dst) const
{
	dst << "vk::Platform::describePlatform() not implemented";
}

} // vk
