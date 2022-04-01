#ifndef _VKPLATFORM_HPP
#define _VKPLATFORM_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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

#include "vkDefs.hpp"

#include <ostream>

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
	virtual const tcu::FunctionLibrary&	getFunctionLibrary		(void) const = 0;
};

class PlatformDriver : public PlatformInterface
{
public:
				PlatformDriver	(const tcu::FunctionLibrary& library);
				~PlatformDriver	(void);

#include "vkConcretePlatformInterface.inl"

				virtual	GetInstanceProcAddrFunc	getGetInstanceProcAddr  () const {
					return m_vk.getInstanceProcAddr;
				}

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
	void		loadFunctions	(const PlatformInterface& platformInterface, VkInstance instance);

	struct Functions
	{
#include "vkInstanceFunctionPointers.inl"
	};

	Functions	m_vk;
};

class DeviceDriver : public DeviceInterface
{
public:
				DeviceDriver	(const PlatformInterface& platformInterface, VkInstance instance, VkDevice device);
				~DeviceDriver	(void);

#include "vkConcreteDeviceInterface.inl"

protected:
	struct Functions
	{
#include "vkDeviceFunctionPointers.inl"
	};

	Functions	m_vk;
};

// Defined in vkWsiPlatform.hpp
namespace wsi
{
class Display;
} // wsi

/*--------------------------------------------------------------------*//*!
 * \brief Vulkan platform interface
 *//*--------------------------------------------------------------------*/
class Platform
{
public:
							Platform			(void) {}
							~Platform			(void) {}

	virtual Library*		createLibrary		(void) const = 0;
	virtual wsi::Display*	createWsiDisplay	(wsi::Type wsiType) const;
	virtual bool			hasDisplay			(wsi::Type wsiType) const;
	virtual void			describePlatform	(std::ostream& dst) const;
};

} // vk

#endif // _VKPLATFORM_HPP
