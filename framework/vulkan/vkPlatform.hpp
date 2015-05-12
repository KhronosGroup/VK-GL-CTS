#ifndef _VKPLATFORM_HPP
#define _VKPLATFORM_HPP
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

#include "vkDefs.hpp"
#include "tcuFunctionLibrary.hpp"

namespace vk
{

class Library
{
public:
										Library					(void) {}
	virtual								~Library				(void) {}

	virtual const PlatformInterface&	getPlatformInterface	(void) const = 0;
};

DE_BEGIN_EXTERN_C

#include "vkFunctionPointerTypes.inl"

DE_END_EXTERN_C

class PlatformDriver : public PlatformInterface
{
public:
				PlatformDriver	(GetProcAddrFunc getProc);
				~PlatformDriver	(void);

#include "vkConcretePlatformInterface.inl"

protected:
	struct Functions
	{
#include "vkPlatformFunctionPointers.inl"
	};

	Functions	m_vk;
};

class DeviceDriver : public DeviceInterface
{
public:
				DeviceDriver	(const PlatformInterface& platformInterface, VkPhysicalDevice device);
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
