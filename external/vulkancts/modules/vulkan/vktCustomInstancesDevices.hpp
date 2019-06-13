#ifndef _VKTCUSTOMINSTANCESDEVICES_HPP
#define _VKTCUSTOMINSTANCESDEVICES_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Auxiliar functions to help create custom devices and instances.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCase.hpp"

#include <vector>
#include <memory>

namespace vk
{
	class PlatformInterface;
	class InstanceInterface;
}

namespace tcu
{
	class CommandLine;
}

namespace vkt
{

std::vector<const char*> getValidationLayers (const vk::PlatformInterface& vkp);

std::vector<const char*> getValidationLayers (const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice);

class CustomInstance
{
public:
								CustomInstance			();
								CustomInstance			(Context& context, vk::Move<vk::VkInstance> instance, bool enableDebugReportRecorder);
								CustomInstance			(CustomInstance&& other);
								~CustomInstance			();
	CustomInstance&				operator=				(CustomInstance&& other);
								operator vk::VkInstance	() const;
	void						swap					(CustomInstance& other);
	const vk::InstanceDriver&	getDriver				() const;
	void						collectMessages			();

								CustomInstance			(const CustomInstance& other) = delete;
	CustomInstance&				operator=				(const CustomInstance& other) = delete;
private:
	Context*									m_context;
	vk::Move<vk::VkInstance>					m_instance;
	std::unique_ptr<vk::InstanceDriver>			m_driver;
	std::unique_ptr<vk::DebugReportRecorder>	m_recorder;
};

class UncheckedInstance
{
public:
						UncheckedInstance		();
						UncheckedInstance		(Context& context, vk::VkInstance instance, const vk::VkAllocationCallbacks* pAllocator, bool enableDebugReportRecorder);
						UncheckedInstance		(UncheckedInstance&& other);
						~UncheckedInstance		();
	UncheckedInstance&	operator=				(UncheckedInstance&& other);
						operator vk::VkInstance	() const;
						operator bool			() const;
	void				swap					(UncheckedInstance& other);

						UncheckedInstance		(const UncheckedInstance& other) = delete;
	UncheckedInstance&	operator=				(const UncheckedInstance& other) = delete;
private:
	Context*									m_context;
	const vk::VkAllocationCallbacks*			m_allocator;
	vk::VkInstance								m_instance;
	std::unique_ptr<vk::InstanceDriver>			m_driver;
	std::unique_ptr<vk::DebugReportRecorder>	m_recorder;
};

// Custom instances.

CustomInstance createCustomInstanceWithExtensions (Context& context, const std::vector<std::string>& extension, const vk::VkAllocationCallbacks* pAllocator = DE_NULL, bool allowLayers = true);

CustomInstance createCustomInstanceWithExtension (Context& context, const std::string& extension, const vk::VkAllocationCallbacks* pAllocator = DE_NULL, bool allowLayers = true);

CustomInstance createCustomInstanceFromContext (Context& context, const vk::VkAllocationCallbacks* pAllocator = DE_NULL, bool allowLayers = true);

CustomInstance createCustomInstanceFromInfo (Context& context, const vk::VkInstanceCreateInfo* instanceCreateInfo, const vk::VkAllocationCallbacks* pAllocator = DE_NULL, bool allowLayers = true);

// Unchecked instance: creation allowed to fail.

vk::VkResult createUncheckedInstance (Context& context, const vk::VkInstanceCreateInfo* instanceCreateInfo, const vk::VkAllocationCallbacks* pAllocator, UncheckedInstance* instance, bool allowLayers = true);

// Custom devices.

vk::Move<vk::VkDevice> createCustomDevice (bool validationEnabled, const vk::PlatformInterface& vkp, vk::VkInstance instance, const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo* pCreateInfo, const vk::VkAllocationCallbacks* pAllocator = DE_NULL);

// Unchecked device: creation allowed to fail.

vk::VkResult createUncheckedDevice (bool validationEnabled, const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo* pCreateInfo, const vk::VkAllocationCallbacks* pAllocator, vk::VkDevice* pDevice);

}

#endif // _VKTCUSTOMINSTANCESDEVICES_HPP
