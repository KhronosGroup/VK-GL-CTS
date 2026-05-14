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
    ProtectedContext(Context &ctx, const std::vector<std::string> instanceExtensions = std::vector<std::string>(),
                     const std::vector<std::string> deviceExtensions = std::vector<std::string>());

    ProtectedContext(Context &ctx, vk::wsi::Type wsiType, vk::wsi::Display &display, vk::wsi::Window &window,
                     const std::vector<std::string> instanceExtensions = std::vector<std::string>(),
                     const std::vector<std::string> deviceExtensions   = std::vector<std::string>());

    const vk::DeviceInterface &getDeviceInterface(void) const
    {
        return m_device.getDriver();
    }
    vk::VkDevice getDevice(void) const
    {
        return *m_device;
    }
    vk::VkPhysicalDevice getPhysicalDevice(void) const
    {
        return m_device.getPhysicalDevice();
    }
    vk::VkQueue getQueue(void) const
    {
        return m_queue;
    }
    uint32_t getQueueFamilyIndex(void) const
    {
        return m_queueFamilyIndex;
    }

    tcu::TestContext &getTestContext(void) const
    {
        return m_context.getTestContext();
    }
    vk::BinaryCollection &getBinaryCollection(void) const
    {
        return m_context.getBinaryCollection();
    }
    vk::Allocator &getDefaultAllocator(void) const
    {
        return m_device.getAllocator();
    }

    const vk::InstanceInterface &getInstanceInterface(void) const
    {
        return m_device.getInstanceDriver();
    }
    vk::VkInstance getInstance(void) const
    {
        return m_instance;
    }
    const vk::VkSurfaceKHR getSurface(void) const
    {
        return *m_surface;
    }

private:
    Context &m_context;
    const vk::PlatformInterface &m_interface;
    const InstanceWrapper m_instance;
    const vk::Move<vk::VkSurfaceKHR> m_surface;
    uint32_t m_queueFamilyIndex;
    const DeviceWrapper m_device;
    vk::VkQueue m_queue;
};

class ProtectedTestInstance : public TestInstance
{
public:
    ProtectedTestInstance(Context &ctx) : TestInstance(ctx), m_protectedContext(ctx)
    {
    }
    ProtectedTestInstance(Context &ctx, const std::vector<std::string> &deviceExtensions)
        : TestInstance(ctx)
        , m_protectedContext(ctx, {}, deviceExtensions)
    {
    }

protected:
    ProtectedContext m_protectedContext;
};

} // namespace ProtectedMem
} // namespace vkt

#endif // _VKTPROTECTEDMEMCONTEXT_HPP
