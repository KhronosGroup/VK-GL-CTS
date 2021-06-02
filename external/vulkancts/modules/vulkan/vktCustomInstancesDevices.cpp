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

#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkDebugReportUtil.hpp"
#include "tcuCommandLine.hpp"
#include "vktCustomInstancesDevices.hpp"

#include <algorithm>
#include <memory>
#include <set>

using std::vector;
using std::string;
using vk::Move;
using vk::VkInstance;
using vk::InstanceDriver;
using vk::DebugReportRecorder;
using vk::VkDebugReportCallbackCreateInfoEXT;
using vk::VkDebugReportCallbackEXT;

namespace vkt
{

namespace
{

vector<const char*> getValidationLayers (const vector<vk::VkLayerProperties>& supportedLayers)
{
	static const char*	s_magicLayer		= "VK_LAYER_KHRONOS_validation";
	static const char*	s_defaultLayers[]	=
	{
		"VK_LAYER_LUNARG_standard_validation",		// Deprecated by at least Vulkan SDK 1.1.121.
		"VK_LAYER_GOOGLE_threading",				// Deprecated by at least Vulkan SDK 1.1.121.
		"VK_LAYER_LUNARG_parameter_validation",		// Deprecated by at least Vulkan SDK 1.1.121.
		"VK_LAYER_LUNARG_device_limits",
		"VK_LAYER_LUNARG_object_tracker",			// Deprecated by at least Vulkan SDK 1.1.121.
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_LUNARG_core_validation",			// Deprecated by at least Vulkan SDK 1.1.121.
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_GOOGLE_unique_objects"			// Deprecated by at least Vulkan SDK 1.1.121.
	};

	vector<const char*>	enabledLayers;

	if (vk::isLayerSupported(supportedLayers, vk::RequiredLayer(s_magicLayer)))
		enabledLayers.push_back(s_magicLayer);
	else
	{
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_defaultLayers); ++ndx)
		{
			if (isLayerSupported(supportedLayers, vk::RequiredLayer(s_defaultLayers[ndx])))
				enabledLayers.push_back(s_defaultLayers[ndx]);
		}
	}

	return enabledLayers;
}

} // anonymous


vector<const char*> getValidationLayers (const vk::PlatformInterface& vkp)
{
	return getValidationLayers(enumerateInstanceLayerProperties(vkp));
}

vector<const char*> getValidationLayers (const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice)
{
	return getValidationLayers(enumerateDeviceLayerProperties(vki, physicalDevice));
}

CustomInstance::CustomInstance (Context& context, Move<VkInstance> instance, std::unique_ptr<vk::DebugReportRecorder>& recorder)
	: m_context		(&context)
	, m_recorder	(recorder.release())
	, m_instance	(instance)
	, m_driver		(new InstanceDriver(context.getPlatformInterface(), *m_instance))
	, m_callback	(m_recorder ? m_recorder->createCallback(*m_driver, *m_instance) : Move<VkDebugReportCallbackEXT>())
{
}

CustomInstance::CustomInstance ()
	: m_context		(nullptr)
	, m_recorder	(nullptr)
	, m_instance	()
	, m_driver		(nullptr)
	, m_callback	()
{
}

CustomInstance::CustomInstance (CustomInstance&& other)
	: CustomInstance()
{
	this->swap(other);
}

CustomInstance::~CustomInstance ()
{
	collectMessages();
}

CustomInstance&	CustomInstance::operator= (CustomInstance&& other)
{
	CustomInstance destroyer;
	destroyer.swap(other);
	this->swap(destroyer);
	return *this;
}

void CustomInstance::swap (CustomInstance& other)
{
	std::swap(m_context, other.m_context);
	m_recorder.swap(other.m_recorder);
	Move<VkInstance> aux = m_instance; m_instance = other.m_instance; other.m_instance = aux;
	m_driver.swap(other.m_driver);
	Move<VkDebugReportCallbackEXT> aux2 = m_callback; m_callback = other.m_callback; other.m_callback = aux2;
}

CustomInstance::operator VkInstance () const
{
	return *m_instance;
}

const vk::InstanceDriver& CustomInstance::getDriver() const
{
	return *m_driver;
}

void CustomInstance::collectMessages ()
{
	if (m_recorder)
		collectAndReportDebugMessages(*m_recorder, *m_context);
}

UncheckedInstance::UncheckedInstance ()
	: m_context		(nullptr)
	, m_recorder	(nullptr)
	, m_allocator	(nullptr)
	, m_instance	(DE_NULL)
	, m_driver		(nullptr)
	, m_callback	()
{
}

UncheckedInstance::UncheckedInstance (Context& context, vk::VkInstance instance, const vk::VkAllocationCallbacks* pAllocator, std::unique_ptr<DebugReportRecorder>& recorder)
	: m_context		(&context)
	, m_recorder	(recorder.release())
	, m_allocator	(pAllocator)
	, m_instance	(instance)
	, m_driver		((m_instance != DE_NULL) ? new InstanceDriver(context.getPlatformInterface(), m_instance) : nullptr)
	, m_callback	(m_recorder ? m_recorder->createCallback(*m_driver, m_instance) : Move<VkDebugReportCallbackEXT>())
{
}

UncheckedInstance::~UncheckedInstance ()
{
	if (m_recorder)
		collectAndReportDebugMessages(*m_recorder, *m_context);

	if (m_instance != DE_NULL)
	{
		m_recorder.reset(nullptr);
		m_driver->destroyInstance(m_instance, m_allocator);
	}
}

void UncheckedInstance::swap (UncheckedInstance& other)
{
	std::swap(m_context, other.m_context);
	m_recorder.swap(other.m_recorder);
	std::swap(m_allocator, other.m_allocator);
	vk::VkInstance aux = m_instance; m_instance = other.m_instance; other.m_instance = aux;
	m_driver.swap(other.m_driver);
	Move<VkDebugReportCallbackEXT> aux2 = m_callback; m_callback = other.m_callback; other.m_callback = aux2;
}

UncheckedInstance::UncheckedInstance (UncheckedInstance&& other)
	: UncheckedInstance()
{
	this->swap(other);
}

UncheckedInstance& UncheckedInstance::operator= (UncheckedInstance&& other)
{
	UncheckedInstance destroyer;
	destroyer.swap(other);
	this->swap(destroyer);
	return *this;
}

UncheckedInstance::operator vk::VkInstance () const
{
	return m_instance;
}
UncheckedInstance::operator bool () const
{
	return (m_instance != DE_NULL);
}

CustomInstance createCustomInstanceWithExtensions (Context& context, const std::vector<std::string>& extensions, const vk::VkAllocationCallbacks* pAllocator, bool allowLayers)
{
	vector<const char*>	enabledLayers;
	vector<string>		enabledLayersStr;
	const auto&			cmdLine					= context.getTestContext().getCommandLine();
	const bool			validationRequested		= (cmdLine.isValidationEnabled() && allowLayers);
	const bool			printValidationErrors	= cmdLine.printValidationErrors();

	if (validationRequested)
	{
		enabledLayers = getValidationLayers(context.getPlatformInterface());
		enabledLayersStr = vector<string>(begin(enabledLayers), end(enabledLayers));
	}

	const bool validationEnabled = !enabledLayers.empty();

	// Filter extension list and throw NotSupported if a required extension is not supported.
	const deUint32									apiVersion			= context.getUsedApiVersion();
	const vk::PlatformInterface&					vkp					= context.getPlatformInterface();
	const vector<vk::VkExtensionProperties>			availableExtensions	= vk::enumerateInstanceExtensionProperties(vkp, DE_NULL);
	std::set<string>								usedExtensions;

	// Get list of available extension names.
	vector<string> availableExtensionNames;
	for (const auto& ext : availableExtensions)
		availableExtensionNames.push_back(ext.extensionName);

	// Filter duplicates and remove core extensions.
	for (const auto& ext : extensions)
	{
		if (!vk::isCoreInstanceExtension(apiVersion, ext))
			usedExtensions.insert(ext);
	}

	// Add debug extension if validation is enabled.
	if (validationEnabled)
		usedExtensions.insert("VK_EXT_debug_report");

	// Check extension support.
	for (const auto& ext : usedExtensions)
	{
		if (!vk::isInstanceExtensionSupported(apiVersion, availableExtensionNames, ext))
			TCU_THROW(NotSupportedError, ext + " is not supported");
	}

	std::unique_ptr<DebugReportRecorder> debugReportRecorder;
	if (validationEnabled)
		debugReportRecorder.reset(new DebugReportRecorder(printValidationErrors));

	// Create custom instance.
	const vector<string> usedExtensionsVec(begin(usedExtensions), end(usedExtensions));
	Move<VkInstance> instance = vk::createDefaultInstance(vkp, apiVersion, enabledLayersStr, usedExtensionsVec, debugReportRecorder.get(), pAllocator);
	return CustomInstance(context, instance, debugReportRecorder);
}

CustomInstance createCustomInstanceWithExtension (Context& context, const std::string& extension, const vk::VkAllocationCallbacks* pAllocator, bool allowLayers)
{
	return createCustomInstanceWithExtensions(context, std::vector<std::string>(1, extension), pAllocator, allowLayers);
}

CustomInstance createCustomInstanceFromContext (Context& context, const vk::VkAllocationCallbacks* pAllocator, bool allowLayers)
{
	return createCustomInstanceWithExtensions(context, std::vector<std::string>(), pAllocator, allowLayers);
}

const char kDebugReportExt[] = "VK_EXT_debug_report";

vector<const char*> addDebugReportExt(const vk::PlatformInterface& vkp, const vk::VkInstanceCreateInfo& createInfo)
{
	if (!isDebugReportSupported(vkp))
		TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");

	vector<const char*> actualExtensions;
	if (createInfo.enabledExtensionCount != 0u)
	{
		for (deUint32 i = 0u; i < createInfo.enabledExtensionCount; ++i)
			actualExtensions.push_back(createInfo.ppEnabledExtensionNames[i]);
	}

	if (std::find_if(begin(actualExtensions), end(actualExtensions), [](const char* name) { return (strcmp(name, kDebugReportExt) == 0); })
		== end(actualExtensions))
	{
		actualExtensions.push_back(kDebugReportExt);
	}

	return actualExtensions;
}

CustomInstance createCustomInstanceFromInfo (Context& context, const vk::VkInstanceCreateInfo* instanceCreateInfo, const vk::VkAllocationCallbacks* pAllocator, bool allowLayers)
{
	vector<const char*>						enabledLayers;
	vector<const char*>						enabledExtensions;
	vk::VkInstanceCreateInfo				createInfo				= *instanceCreateInfo;
	const auto&								cmdLine					= context.getTestContext().getCommandLine();
	const bool								validationEnabled		= cmdLine.isValidationEnabled();
	const bool								printValidationErrors	= cmdLine.printValidationErrors();
	const vk::PlatformInterface&			vkp						= context.getPlatformInterface();
	std::unique_ptr<DebugReportRecorder>	recorder;
	VkDebugReportCallbackCreateInfoEXT		callbackInfo;

	if (validationEnabled && allowLayers)
	{
		// Activate some layers if requested.
		if (createInfo.enabledLayerCount == 0u)
		{
			enabledLayers = getValidationLayers(vkp);
			createInfo.enabledLayerCount = static_cast<deUint32>(enabledLayers.size());
			createInfo.ppEnabledLayerNames = (enabledLayers.empty() ? DE_NULL : enabledLayers.data());
		}

		// Make sure the debug report extension is enabled when validation is enabled.
		enabledExtensions = addDebugReportExt(vkp, createInfo);
		createInfo.enabledExtensionCount = static_cast<deUint32>(enabledExtensions.size());
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();

		recorder.reset(new DebugReportRecorder(printValidationErrors));
		callbackInfo		= recorder->makeCreateInfo();
		callbackInfo.pNext	= createInfo.pNext;
		createInfo.pNext	= &callbackInfo;
	}

	return CustomInstance(context, vk::createInstance(vkp, &createInfo, pAllocator), recorder);
}

vk::VkResult createUncheckedInstance (Context& context, const vk::VkInstanceCreateInfo* instanceCreateInfo, const vk::VkAllocationCallbacks* pAllocator, UncheckedInstance* instance, bool allowLayers)
{
	vector<const char*>						enabledLayers;
	vector<const char*>						enabledExtensions;
	vk::VkInstanceCreateInfo				createInfo				= *instanceCreateInfo;
	const auto&								cmdLine					= context.getTestContext().getCommandLine();
	const bool								validationEnabled		= cmdLine.isValidationEnabled();
	const bool								printValidationErrors	= cmdLine.printValidationErrors();
	const vk::PlatformInterface&			vkp						= context.getPlatformInterface();
	const bool								addLayers				= (validationEnabled && allowLayers);
	std::unique_ptr<DebugReportRecorder>	recorder;
	VkDebugReportCallbackCreateInfoEXT		callbackInfo;

	if (addLayers)
	{
		// Activate some layers if requested.
		if (createInfo.enabledLayerCount == 0u)
		{
			enabledLayers = getValidationLayers(vkp);
			createInfo.enabledLayerCount = static_cast<deUint32>(enabledLayers.size());
			createInfo.ppEnabledLayerNames = (enabledLayers.empty() ? DE_NULL : enabledLayers.data());
		}

		// Make sure the debug report extension is enabled when validation is enabled.
		enabledExtensions = addDebugReportExt(vkp, createInfo);
		createInfo.enabledExtensionCount = static_cast<deUint32>(enabledExtensions.size());
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();

		// Prepare debug report recorder also for instance creation.
		recorder.reset(new DebugReportRecorder(printValidationErrors));
		callbackInfo		= recorder->makeCreateInfo();
		callbackInfo.pNext	= createInfo.pNext;
		createInfo.pNext	= &callbackInfo;
	}

	vk::VkInstance	raw_instance = DE_NULL;
	vk::VkResult	result = vkp.createInstance(&createInfo, pAllocator, &raw_instance);

	*instance = UncheckedInstance(context, raw_instance, pAllocator, recorder);

	return result;
}

vk::Move<vk::VkDevice> createCustomDevice (bool validationEnabled, const vk::PlatformInterface& vkp, vk::VkInstance instance, const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo* pCreateInfo, const vk::VkAllocationCallbacks* pAllocator)
{
	vector<const char*>		enabledLayers;
	vk::VkDeviceCreateInfo	createInfo		= *pCreateInfo;

	if (createInfo.enabledLayerCount == 0u && validationEnabled)
	{
		enabledLayers = getValidationLayers(vki, physicalDevice);
		createInfo.enabledLayerCount = static_cast<deUint32>(enabledLayers.size());
		createInfo.ppEnabledLayerNames = (enabledLayers.empty() ? DE_NULL : enabledLayers.data());
	}

	return createDevice(vkp, instance, vki, physicalDevice, &createInfo, pAllocator);
}

vk::VkResult createUncheckedDevice (bool validationEnabled, const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo* pCreateInfo, const vk::VkAllocationCallbacks* pAllocator, vk::VkDevice* pDevice)
{
	vector<const char*>		enabledLayers;
	vk::VkDeviceCreateInfo	createInfo		= *pCreateInfo;

	if (createInfo.enabledLayerCount == 0u && validationEnabled)
	{
		enabledLayers = getValidationLayers(vki, physicalDevice);
		createInfo.enabledLayerCount = static_cast<deUint32>(enabledLayers.size());
		createInfo.ppEnabledLayerNames = (enabledLayers.empty() ? DE_NULL : enabledLayers.data());
	}

	return vki.createDevice(physicalDevice, &createInfo, pAllocator, pDevice);
}


}
