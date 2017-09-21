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

#include "vktProtectedMemUtils.hpp"
#include "tcuCommandLine.hpp"
#include "vkMemUtil.hpp"

namespace vkt
{
namespace ProtectedMem
{


class ProtectedContext
{
public:
		ProtectedContext	(Context& ctx)
			: m_context				(ctx)
			, m_interface			(m_context.getPlatformInterface())
			, m_instance			(makeProtectedMemInstance(m_interface, m_context))
			, m_vki					(m_interface, *m_instance)
			, m_phyDevice			(vk::chooseDevice(m_vki, *m_instance, m_context.getTestContext().getCommandLine()))
			, m_queueFamilyIndex	(chooseProtectedMemQueueFamilyIndex(m_vki, m_phyDevice))
			, m_device				(makeProtectedMemDevice(m_vki, m_phyDevice, m_queueFamilyIndex, ctx.getUsedApiVersion()))
			, m_allocator			(createAllocator())
			, m_deviceDriver		(m_vki, *m_device)
			, m_queue				(getProtectedQueue(m_deviceDriver, *m_device, m_queueFamilyIndex, 0))
		{}

	const vk::DeviceInterface&					getDeviceInterface	(void) const	{ return m_deviceDriver;					}
	vk::VkDevice								getDevice			(void) const	{ return *m_device;							}
	vk::VkQueue									getQueue			(void) const	{ return m_queue;							}
	deUint32									getQueueFamilyIndex	(void) const	{ return m_queueFamilyIndex;				}

	tcu::TestContext&							getTestContext		(void) const	{ return m_context.getTestContext();		}
	vk::ProgramCollection<vk::ProgramBinary>&	getBinaryCollection	(void) const	{ return m_context.getBinaryCollection();	}
	vk::Allocator&								getDefaultAllocator	(void) const	{ return *m_allocator;	}

private:
	vk::Allocator* createAllocator (void)
	{
		const vk::VkPhysicalDeviceMemoryProperties memoryProperties =
			vk::getPhysicalDeviceMemoryProperties(m_vki, m_phyDevice);

		// \todo [2015-07-24 jarkko] support allocator selection/configuration from command line (or compile time)
		return new vk::SimpleAllocator(getDeviceInterface(), getDevice(), memoryProperties);
	}

	Context&						m_context;
	const vk::PlatformInterface&	m_interface;
	vk::Move<vk::VkInstance>		m_instance;
	vk::InstanceDriver				m_vki;
	vk::VkPhysicalDevice			m_phyDevice;
	deUint32						m_queueFamilyIndex;
	vk::Move<vk::VkDevice>			m_device;
	const de::UniquePtr<vk::Allocator>	m_allocator;
	vk::DeviceDriver				m_deviceDriver;
	vk::VkQueue						m_queue;
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
