#ifndef _VKPLATFORM_HPP
#define _VKPLATFORM_HPP
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

#include "vkDefs.hpp"

namespace tcu
{
class FunctionLibrary;
}

namespace vk
{

class Library
{
public:
										Library					(void) {}
	virtual								~Library				(void) {}

	virtual const PlatformInterface&	getPlatformInterface	(void) const = 0;
};

class PlatformDriver : public PlatformInterface
{
public:
				PlatformDriver	(const tcu::FunctionLibrary& library);
				~PlatformDriver	(void);

#include "vkConcretePlatformInterface.inl"

protected:
	struct Functions
	{
#include "vkPlatformFunctionPointers.inl"
	};

	Functions	m_vk;
};

class InstanceDriver : public InstanceInterface
{
public:
				InstanceDriver	(const PlatformInterface& platformInterface, VkInstance instance);
				~InstanceDriver	(void);

#include "vkConcreteInstanceInterface.inl"

protected:
	struct Functions
	{
#include "vkInstanceFunctionPointers.inl"
	};

	Functions	m_vk;
};

class DeviceDriver : public DeviceInterface
{
public:
				DeviceDriver	(const InstanceInterface& instanceInterface, VkDevice device);
				~DeviceDriver	(void);

#include "vkConcreteDeviceInterface.inl"

protected:
	struct Functions
	{
#include "vkDeviceFunctionPointers.inl"
	};

	Functions	m_vk;
};

/*--------------------------------------------------------------------*//*!
 * \brief Vulkan platform interface
 *//*--------------------------------------------------------------------*/
class Platform
{
public:
						Platform		(void) {}
						~Platform		(void) {}

	// \todo [2015-01-05 pyry] Parametrize this to select for example debug library / interface?
	virtual Library*	createLibrary	(void) const = 0;
};

} // vk

#endif // _VKPLATFORM_HPP
