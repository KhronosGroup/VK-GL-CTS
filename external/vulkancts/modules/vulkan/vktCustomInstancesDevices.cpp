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
#ifndef CTS_USES_VULKANSC
using vk::InstanceDriver;
using vk::DebugReportRecorder;
using vk::VkDebugReportCallbackCreateInfoEXT;
using vk::VkDebugReportCallbackEXT;
#else
using vk::InstanceDriverSC;
#endif // CTS_USES_VULKANSC

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

#ifndef CTS_USES_VULKANSC
CustomInstance::CustomInstance(Context& context, Move<VkInstance> instance, std::unique_ptr<vk::DebugReportRecorder>& recorder)
#else
CustomInstance::CustomInstance(Context& context, Move<VkInstance> instance)
#endif // CTS_USES_VULKANSC
	: m_context		(&context)
#ifndef CTS_USES_VULKANSC
	, m_recorder	(recorder.release())
#endif // CTS_USES_VULKANSC
	, m_instance	(instance)
#ifndef CTS_USES_VULKANSC
	, m_driver		(new InstanceDriver(context.getPlatformInterface(), *m_instance))
	, m_callback	(m_recorder ? m_recorder->createCallback(*m_driver, *m_instance) : Move<VkDebugReportCallbackEXT>())
#else
	, m_driver		(new InstanceDriverSC(context.getPlatformInterface(), *m_instance, context.getTestContext().getCommandLine(), context.getResourceInterface()))
#endif // CTS_USES_VULKANSC
{
}

CustomInstance::CustomInstance ()
	: m_context		(nullptr)
#ifndef CTS_USES_VULKANSC
	, m_recorder	(nullptr)
#endif // CTS_USES_VULKANSC
	, m_instance	()
	, m_driver		(nullptr)
#ifndef CTS_USES_VULKANSC
	, m_callback	()
#endif // CTS_USES_VULKANSC
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
#ifndef CTS_USES_VULKANSC
	m_recorder.swap(other.m_recorder);
#endif // CTS_USES_VULKANSC
	Move<VkInstance> aux = m_instance; m_instance = other.m_instance; other.m_instance = aux;
	m_driver.swap(other.m_driver);
#ifndef CTS_USES_VULKANSC
	Move<VkDebugReportCallbackEXT> aux2 = m_callback; m_callback = other.m_callback; other.m_callback = aux2;
#endif // CTS_USES_VULKANSC
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
#ifndef CTS_USES_VULKANSC
	if (m_recorder)
		collectAndReportDebugMessages(*m_recorder, *m_context);
#endif // CTS_USES_VULKANSC
}

UncheckedInstance::UncheckedInstance ()
	: m_context		(nullptr)
#ifndef CTS_USES_VULKANSC
	, m_recorder	(nullptr)
#endif // CTS_USES_VULKANSC
	, m_allocator	(nullptr)
	, m_instance	(DE_NULL)
	, m_driver		(nullptr)
#ifndef CTS_USES_VULKANSC
	, m_callback	()
#endif // CTS_USES_VULKANSC
{
}

#ifndef CTS_USES_VULKANSC
UncheckedInstance::UncheckedInstance (Context& context, vk::VkInstance instance, const vk::VkAllocationCallbacks* pAllocator, std::unique_ptr<DebugReportRecorder>& recorder)
#else
UncheckedInstance::UncheckedInstance(Context& context, vk::VkInstance instance, const vk::VkAllocationCallbacks* pAllocator)
#endif // CTS_USES_VULKANSC

	: m_context		(&context)
#ifndef CTS_USES_VULKANSC
	, m_recorder	(recorder.release())
#endif // CTS_USES_VULKANSC
	, m_allocator	(pAllocator)
	, m_instance	(instance)
#ifndef CTS_USES_VULKANSC
	, m_driver((m_instance != DE_NULL) ? new InstanceDriver(context.getPlatformInterface(), m_instance) : nullptr)
	, m_callback	((m_driver && m_recorder) ? m_recorder->createCallback(*m_driver, m_instance) : Move<VkDebugReportCallbackEXT>())
#else
	, m_driver((m_instance != DE_NULL) ? new InstanceDriverSC(context.getPlatformInterface(), m_instance, context.getTestContext().getCommandLine(), context.getResourceInterface()) : nullptr)
#endif // CTS_USES_VULKANSC
{
}

UncheckedInstance::~UncheckedInstance ()
{
#ifndef CTS_USES_VULKANSC
	if (m_recorder)
		collectAndReportDebugMessages(*m_recorder, *m_context);
#endif // CTS_USES_VULKANSC

	if (m_instance != DE_NULL)
	{
#ifndef CTS_USES_VULKANSC
		m_callback = vk::Move<vk::VkDebugReportCallbackEXT>();
		m_recorder.reset(nullptr);
#endif // CTS_USES_VULKANSC
		m_driver->destroyInstance(m_instance, m_allocator);
	}
}

void UncheckedInstance::swap (UncheckedInstance& other)
{
	std::swap(m_context, other.m_context);
#ifndef CTS_USES_VULKANSC
	m_recorder.swap(other.m_recorder);
#endif // CTS_USES_VULKANSC
	std::swap(m_allocator, other.m_allocator);
	vk::VkInstance aux = m_instance; m_instance = other.m_instance; other.m_instance = aux;
	m_driver.swap(other.m_driver);
#ifndef CTS_USES_VULKANSC
	Move<VkDebugReportCallbackEXT> aux2 = m_callback; m_callback = other.m_callback; other.m_callback = aux2;
#endif // CTS_USES_VULKANSC
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
#ifndef CTS_USES_VULKANSC
	const bool			printValidationErrors	= cmdLine.printValidationErrors();
#endif // CTS_USES_VULKANSC

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

#ifndef CTS_USES_VULKANSC
	std::unique_ptr<DebugReportRecorder> debugReportRecorder;
	if (validationEnabled)
		debugReportRecorder.reset(new DebugReportRecorder(printValidationErrors));
#endif // CTS_USES_VULKANSC

	// Create custom instance.
	const vector<string> usedExtensionsVec(begin(usedExtensions), end(usedExtensions));
#ifndef CTS_USES_VULKANSC
	Move<VkInstance> instance = vk::createDefaultInstance(vkp, apiVersion, enabledLayersStr, usedExtensionsVec, cmdLine, debugReportRecorder.get(), pAllocator);
	return CustomInstance(context, instance, debugReportRecorder);
#else
	Move<VkInstance> instance = vk::createDefaultInstance(vkp, apiVersion, enabledLayersStr, usedExtensionsVec, cmdLine, pAllocator);
	return CustomInstance(context, instance);
#endif // CTS_USES_VULKANSC
}

CustomInstance createCustomInstanceWithExtension (Context& context, const std::string& extension, const vk::VkAllocationCallbacks* pAllocator, bool allowLayers)
{
	return createCustomInstanceWithExtensions(context, std::vector<std::string>(1, extension), pAllocator, allowLayers);
}

CustomInstance createCustomInstanceFromContext (Context& context, const vk::VkAllocationCallbacks* pAllocator, bool allowLayers)
{
	return createCustomInstanceWithExtensions(context, std::vector<std::string>(), pAllocator, allowLayers);
}

static std::vector<const char*> copyExtensions(const vk::VkInstanceCreateInfo& createInfo)
{
	std::vector<const char*> extensions(createInfo.enabledExtensionCount);
	for (size_t i = 0u; i < extensions.size(); ++i)
		extensions[i] = createInfo.ppEnabledExtensionNames[i];
	return extensions;
}

static void addExtension(std::vector<const char*>& presentExtensions, const char* extension)
{
	if (std::find_if(presentExtensions.cbegin(), presentExtensions.cend(), [extension](const char* name) { return (strcmp(name, extension) == 0); })
		== presentExtensions.cend())
	{
		presentExtensions.emplace_back(extension);
	}
}

vector<const char*> addDebugReportExt(const vk::PlatformInterface& vkp, const vk::VkInstanceCreateInfo& createInfo)
{
	if (!isDebugReportSupported(vkp))
		TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");

	vector<const char*> actualExtensions;
	if (createInfo.enabledExtensionCount != 0u)
		actualExtensions = copyExtensions(createInfo);

	addExtension(actualExtensions, "VK_EXT_debug_report");

	return actualExtensions;
}

CustomInstance createCustomInstanceFromInfo (Context& context, const vk::VkInstanceCreateInfo* instanceCreateInfo, const vk::VkAllocationCallbacks* pAllocator, bool allowLayers)
{
	vector<const char*>						enabledLayers;
	vector<const char*>						enabledExtensions;
	vk::VkInstanceCreateInfo				createInfo				= *instanceCreateInfo;
	const auto&								cmdLine					= context.getTestContext().getCommandLine();
	const bool								validationEnabled		= cmdLine.isValidationEnabled();
#ifndef CTS_USES_VULKANSC
	const bool								printValidationErrors	= cmdLine.printValidationErrors();
#endif // CTS_USES_VULKANSC
	const vk::PlatformInterface&			vkp						= context.getPlatformInterface();
#ifndef CTS_USES_VULKANSC
	std::unique_ptr<DebugReportRecorder>	recorder;
	VkDebugReportCallbackCreateInfoEXT		callbackInfo;
#endif // CTS_USES_VULKANSC

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

#ifndef CTS_USES_VULKANSC
		recorder.reset(new DebugReportRecorder(printValidationErrors));
		callbackInfo		= recorder->makeCreateInfo();
		callbackInfo.pNext	= createInfo.pNext;
		createInfo.pNext	= &callbackInfo;
#endif // CTS_USES_VULKANSC
	}

#ifndef CTS_USES_VULKANSC
	// Enable portability if available. Needed for portability drivers, otherwise loader will complain and make tests fail
	std::vector<vk::VkExtensionProperties> availableExtensions = vk::enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);
	if (vk::isExtensionStructSupported(availableExtensions, vk::RequiredExtension("VK_KHR_portability_enumeration")))
	{
		if (enabledExtensions.empty() && createInfo.enabledExtensionCount != 0u)
			enabledExtensions = copyExtensions(createInfo);

		addExtension(enabledExtensions, "VK_KHR_portability_enumeration");
		createInfo.enabledExtensionCount = static_cast<deUint32>(enabledExtensions.size());
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();
		createInfo.flags |= vk::VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	}

	return CustomInstance(context, vk::createInstance(vkp, &createInfo, pAllocator), recorder);
#else
	return CustomInstance(context, vk::createInstance(vkp, &createInfo, pAllocator));
#endif // CTS_USES_VULKANSC
}

vk::VkResult createUncheckedInstance (Context& context, const vk::VkInstanceCreateInfo* instanceCreateInfo, const vk::VkAllocationCallbacks* pAllocator, UncheckedInstance* instance, bool allowLayers)
{
	vector<const char*>						enabledLayers;
	vector<const char*>						enabledExtensions;
	vk::VkInstanceCreateInfo				createInfo				= *instanceCreateInfo;
	const auto&								cmdLine					= context.getTestContext().getCommandLine();
	const bool								validationEnabled		= cmdLine.isValidationEnabled();
#ifndef CTS_USES_VULKANSC
	const bool								printValidationErrors	= cmdLine.printValidationErrors();
#endif // CTS_USES_VULKANSC
	const vk::PlatformInterface&			vkp						= context.getPlatformInterface();
	const bool								addLayers				= (validationEnabled && allowLayers);
#ifndef CTS_USES_VULKANSC
	std::unique_ptr<DebugReportRecorder>	recorder;
#endif // CTS_USES_VULKANSC

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

#ifndef CTS_USES_VULKANSC
		recorder.reset(new DebugReportRecorder(printValidationErrors));
		// No need to add VkDebugReportCallbackCreateInfoEXT to VkInstanceCreateInfo since we
		// don't want to check for errors at instance creation. This is intended since we use
		// UncheckedInstance to try to create invalid instances for driver stability
#endif // CTS_USES_VULKANSC
	}

#ifndef CTS_USES_VULKANSC
	// Enable portability if available. Needed for portability drivers, otherwise loader will complain and make tests fail
	std::vector<vk::VkExtensionProperties> availableExtensions = vk::enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);
	if (vk::isExtensionStructSupported(availableExtensions, vk::RequiredExtension("VK_KHR_portability_enumeration")))
	{
		if (enabledExtensions.empty() && createInfo.enabledExtensionCount != 0u)
			enabledExtensions = copyExtensions(createInfo);

		addExtension(enabledExtensions, "VK_KHR_portability_enumeration");
		createInfo.enabledExtensionCount = static_cast<deUint32>(enabledExtensions.size());
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();
		createInfo.flags |= vk::VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	}
#endif // CTS_USES_VULKANSC

	vk::VkInstance	raw_instance = DE_NULL;
	vk::VkResult	result = vkp.createInstance(&createInfo, pAllocator, &raw_instance);

#ifndef CTS_USES_VULKANSC
	*instance = UncheckedInstance(context, raw_instance, pAllocator, recorder);
#else
	*instance = UncheckedInstance(context, raw_instance, pAllocator);
#endif // CTS_USES_VULKANSC

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

CustomInstanceWrapper::CustomInstanceWrapper(Context& context)
	: instance(vkt::createCustomInstanceFromContext(context))
{
}

CustomInstanceWrapper::CustomInstanceWrapper(Context& context, const std::vector<std::string> extensions)
	: instance(vkt::createCustomInstanceWithExtensions(context, extensions))
{
}

}
