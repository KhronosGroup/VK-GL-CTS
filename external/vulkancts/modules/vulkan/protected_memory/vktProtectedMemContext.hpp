#ifndef _VKTPROTECTEDMEMCONTEXT_HPP
#define _VKTPROTECTEDMEMCONTEXT_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected Memory image validator helper
 *//*--------------------------------------------------------------------*/

#include "tcuVector.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vktProtectedMemUtils.hpp"
#include "tcuCommandLine.hpp"
#include "vkMemUtil.hpp"
#include "vkWsiUtil.hpp"

namespace vkt
{
namespace ProtectedMem
{

class ProtectedContext
{
public:
		ProtectedContext	(Context&						ctx,
							 const std::vector<std::string>	instanceExtensions = std::vector<std::string>(),
							 const std::vector<std::string>	deviceExtensions = std::vector<std::string>());

		ProtectedContext	(Context&						ctx,
							 vk::wsi::Type					wsiType,
							 vk::wsi::Display&				display,
							 vk::wsi::Window&				window,
							 const std::vector<std::string>	instanceExtensions = std::vector<std::string>(),
							 const std::vector<std::string>	deviceExtensions = std::vector<std::string>());

	const vk::DeviceInterface&					getDeviceInterface	(void) const	{ return m_deviceDriver;					}
	vk::VkDevice								getDevice			(void) const	{ return *m_device;							}
	const vk::DeviceDriver&						getDeviceDriver		(void) const	{ return m_deviceDriver;					}
	vk::VkPhysicalDevice						getPhysicalDevice	(void) const	{ return m_phyDevice;						}
	vk::VkQueue									getQueue			(void) const	{ return m_queue;							}
	deUint32									getQueueFamilyIndex	(void) const	{ return m_queueFamilyIndex;				}

	tcu::TestContext&							getTestContext		(void) const	{ return m_context.getTestContext();		}
	vk::BinaryCollection&						getBinaryCollection	(void) const	{ return m_context.getBinaryCollection();	}
	vk::Allocator&								getDefaultAllocator	(void) const	{ return *m_allocator;	}

	const vk::InstanceDriver&					getInstanceDriver	(void) const	{ return m_vki;								}
	vk::VkInstance								getInstance			(void) const	{ return m_instance;						}
	const vk::VkSurfaceKHR						getSurface			(void) const	{ return *m_surface;						}


private:
	vk::Allocator* createAllocator (void)
	{
		const vk::VkPhysicalDeviceMemoryProperties memoryProperties =
			vk::getPhysicalDeviceMemoryProperties(m_vki, m_phyDevice);

		// \todo [2015-07-24 jarkko] support allocator selection/configuration from command line (or compile time)
		return new vk::SimpleAllocator(getDeviceInterface(), getDevice(), memoryProperties);
	}

	Context&							m_context;
	const vk::PlatformInterface&		m_interface;
	CustomInstance						m_instance;
	const vk::InstanceDriver&			m_vki;
	vk::VkPhysicalDevice				m_phyDevice;
	const vk::Move<vk::VkSurfaceKHR>	m_surface;
	deUint32							m_queueFamilyIndex;
	vk::Move<vk::VkDevice>				m_device;
	const de::UniquePtr<vk::Allocator>	m_allocator;
	vk::DeviceDriver					m_deviceDriver;
	vk::VkQueue							m_queue;
};

class ProtectedTestInstance : public TestInstance
{
public:
				ProtectedTestInstance	(Context& ctx)
					: TestInstance			(ctx)
					, m_protectedContext	(ctx)
				{}
protected:
	ProtectedContext	m_protectedContext;
};

} // ProtectedMem
} // vkt

#endif // _VKTPROTECTEDMEMCONTEXT_HPP
