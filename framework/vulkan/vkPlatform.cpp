/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Utilities
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan platform abstraction.
 *//*--------------------------------------------------------------------*/

#include "vkPlatform.hpp"

namespace vk
{

PlatformDriver::PlatformDriver (GetProcAddrFunc getProc)
{
	m_vk.getProcAddr = getProc;

#define GET_PROC_ADDR(NAME) getProc(DE_NULL, NAME)
#include "vkInitPlatformFunctionPointers.inl"
#undef GET_PROC_ADDR
}

PlatformDriver::~PlatformDriver (void)
{
}

DeviceDriver::DeviceDriver (const PlatformInterface& platformInterface, VkPhysicalDevice device)
{
#define GET_PROC_ADDR(NAME) platformInterface.getProcAddr(device, NAME)
#include "vkInitDeviceFunctionPointers.inl"
#undef GET_PROC_ADDR
}

DeviceDriver::~DeviceDriver (void)
{
}

#include "vkPlatformDriverImpl.inl"
#include "vkDeviceDriverImpl.inl"

} // vk
