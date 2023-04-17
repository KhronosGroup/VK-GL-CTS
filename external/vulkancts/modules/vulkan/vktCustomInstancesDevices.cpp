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
#include "vkMemUtil.hpp"
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
		m_callback.~Move<vk::VkDebugReportCallbackEXT>();
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

#ifdef CTS_USES_VULKANSC
	// Add fault callback if there isn't one already.
	VkFaultCallbackInfo					faultCallbackInfo		=
	{
		VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO,					//	VkStructureType				sType;
		DE_NULL,												//	void*						pNext;
		0U,														//	uint32_t					faultCount;
		nullptr,												//	VkFaultData*				pFaults;
		Context::faultCallbackFunction							//	PFN_vkFaultCallbackFunction	pfnFaultCallback;
	};

	if (!findStructureInChain(createInfo.pNext, getStructureType<VkFaultCallbackInfo>()))
	{
		// XXX workaround incorrect constness on faultCallbackInfo.pNext.
		faultCallbackInfo.pNext = const_cast<void *>(createInfo.pNext);
		createInfo.pNext = &faultCallbackInfo;
	}
#endif // CTS_USES_VULKANSC

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

#ifdef CTS_USES_VULKANSC
	// Add fault callback if there isn't one already.
	VkFaultCallbackInfo					faultCallbackInfo		=
	{
		VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO,					//	VkStructureType				sType;
		DE_NULL,												//	void*						pNext;
		0U,														//	uint32_t					faultCount;
		nullptr,												//	VkFaultData*				pFaults;
		Context::faultCallbackFunction							//	PFN_vkFaultCallbackFunction	pfnFaultCallback;
	};

	if (!findStructureInChain(createInfo.pNext, getStructureType<VkFaultCallbackInfo>()))
	{
		// XXX workaround incorrect constness on faultCallbackInfo.pNext.
		faultCallbackInfo.pNext = const_cast<void *>(createInfo.pNext);
		createInfo.pNext = &faultCallbackInfo;
	}
#endif // CTS_USES_VULKANSC

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
void VideoDevice::checkSupport (Context&						context,
								const VideoCodecOperationFlags	videoCodecOperation)
{
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(videoCodecOperation != 0 && isVideoOperation(videoCodecOperation));

	if (isVideoOperation(videoCodecOperation))
		context.requireDeviceFunctionality("VK_KHR_video_queue");

	if (isVideoEncodeOperation(videoCodecOperation))
		context.requireDeviceFunctionality("VK_KHR_video_encode_queue");

	if (isVideoDecodeOperation(videoCodecOperation))
		context.requireDeviceFunctionality("VK_KHR_video_decode_queue");

	if ((videoCodecOperation & vk::VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT) != 0)
		context.requireDeviceFunctionality("VK_EXT_video_encode_h264");

	if ((videoCodecOperation & vk::VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT) != 0)
		context.requireDeviceFunctionality("VK_EXT_video_encode_h265");

	if ((videoCodecOperation & vk::VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) != 0)
		context.requireDeviceFunctionality("VK_KHR_video_decode_h264");

	if ((videoCodecOperation & vk::VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) != 0)
		context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
#else
	DE_UNREF(context);
	DE_UNREF(videoCodecOperation);
#endif
}

VideoDevice::VideoDevice (Context& context)
	: m_context				(context)
	, m_logicalDevice		()
	, m_deviceDriver		()
	, m_allocator			()
	, m_queueFamilyTransfer	(VK_QUEUE_FAMILY_IGNORED)
	, m_queueFamilyDecode	(VK_QUEUE_FAMILY_IGNORED)
	, m_queueFamilyEncode	(VK_QUEUE_FAMILY_IGNORED)
#ifndef CTS_USES_VULKANSC
	, m_videoCodecOperation	(vk::VK_VIDEO_CODEC_OPERATION_NONE_KHR)
#else
	, m_videoCodecOperation	(~0u)
#endif
{
}

VideoDevice::VideoDevice (Context&							context,
						  const VideoCodecOperationFlags	videoCodecOperation,
						  const VideoDeviceFlags			videoDeviceFlags)
	: VideoDevice	(context)
{
#ifndef CTS_USES_VULKANSC
	const vk::VkQueueFlags	queueFlagsRequired	= getQueueFlags(videoCodecOperation);
	const vk::VkDevice		result				= getDeviceSupportingQueue(queueFlagsRequired, videoCodecOperation, videoDeviceFlags);

	DE_ASSERT(result != DE_NULL);
	DE_UNREF(result);
#else
	DE_UNREF(videoCodecOperation);
	DE_UNREF(videoDeviceFlags);
#endif
}

VideoDevice::~VideoDevice (void)
{
}

vk::VkQueueFlags VideoDevice::getQueueFlags (const VideoCodecOperationFlags videoCodecOperation)
{
#ifndef CTS_USES_VULKANSC
	const vk::VkQueueFlags	queueFlagsRequired	= (isVideoEncodeOperation(videoCodecOperation) ? vk::VK_QUEUE_VIDEO_ENCODE_BIT_KHR : 0)
												| (isVideoDecodeOperation(videoCodecOperation) ? vk::VK_QUEUE_VIDEO_DECODE_BIT_KHR : 0);

	return queueFlagsRequired;
#else
	DE_UNREF(videoCodecOperation);

	return 0;
#endif
}

bool VideoDevice::isVideoEncodeOperation (const VideoCodecOperationFlags videoCodecOperationFlags)
{
#ifndef CTS_USES_VULKANSC
	const vk::VkVideoCodecOperationFlagsKHR	encodeOperations	= vk::VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT
																| vk::VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT;

	return (encodeOperations & videoCodecOperationFlags) != 0;
#else
	DE_UNREF(videoCodecOperationFlags);

	return false;
#endif
}

bool VideoDevice::isVideoDecodeOperation (const VideoCodecOperationFlags videoCodecOperationFlags)
{
#ifndef CTS_USES_VULKANSC
	const vk::VkVideoCodecOperationFlagsKHR	decodeOperations	= vk::VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR
																| vk::VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;

	return (decodeOperations & videoCodecOperationFlags) != 0;
#else
	DE_UNREF(videoCodecOperationFlags);

	return false;
#endif
}

bool VideoDevice::isVideoOperation (const VideoCodecOperationFlags videoCodecOperationFlags)
{
#ifndef CTS_USES_VULKANSC
	return isVideoDecodeOperation(videoCodecOperationFlags) || isVideoEncodeOperation(videoCodecOperationFlags);
#else
	DE_UNREF(videoCodecOperationFlags);

	return false;
#endif
}

void VideoDevice::addVideoDeviceExtensions (std::vector<const char*>&		deviceExtensions,
											const uint32_t					apiVersion,
											const vk::VkQueueFlags			queueFlagsRequired,
											const VideoCodecOperationFlags	videoCodecOperationFlags)
{
#ifndef CTS_USES_VULKANSC
	static const char videoQueue[]			= "VK_KHR_video_queue";
	static const char videoEncodeQueue[]	= "VK_KHR_video_encode_queue";
	static const char videoDecodeQueue[]	= "VK_KHR_video_decode_queue";
	static const char videoEncodeH264[]		= "VK_EXT_video_encode_h264";
	static const char videoEncodeH265[]		= "VK_EXT_video_encode_h265";
	static const char videoDecodeH264[]		= "VK_KHR_video_decode_h264";
	static const char videoDecodeH265[]		= "VK_KHR_video_decode_h265";

	if (!vk::isCoreDeviceExtension(apiVersion, videoQueue))
		deviceExtensions.push_back(videoQueue);

	if ((queueFlagsRequired & vk::VK_QUEUE_VIDEO_ENCODE_BIT_KHR) != 0)
		if (!vk::isCoreDeviceExtension(apiVersion, videoEncodeQueue))
			deviceExtensions.push_back(videoEncodeQueue);

	if ((queueFlagsRequired & vk::VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0)
		if (!vk::isCoreDeviceExtension(apiVersion, videoDecodeQueue))
			deviceExtensions.push_back(videoDecodeQueue);

	if ((videoCodecOperationFlags & vk::VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT) != 0)
		if (!vk::isCoreDeviceExtension(apiVersion, videoEncodeH264))
			deviceExtensions.push_back(videoEncodeH264);

	if ((videoCodecOperationFlags & vk::VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_EXT) != 0)
		if (!vk::isCoreDeviceExtension(apiVersion, videoEncodeH265))
			deviceExtensions.push_back(videoEncodeH265);

	if ((videoCodecOperationFlags & vk::VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) != 0)
		if (!vk::isCoreDeviceExtension(apiVersion, videoDecodeH265))
			deviceExtensions.push_back(videoDecodeH265);

	if ((videoCodecOperationFlags & vk::VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) != 0)
		if (!vk::isCoreDeviceExtension(apiVersion, videoDecodeH264))
			deviceExtensions.push_back(videoDecodeH264);
#else
	DE_UNREF(deviceExtensions);
	DE_UNREF(apiVersion);
	DE_UNREF(queueFlagsRequired);
	DE_UNREF(videoCodecOperationFlags);
#endif
}

vk::VkDevice VideoDevice::getDeviceSupportingQueue (const vk::VkQueueFlags					queueFlagsRequired,
													const VideoCodecOperationFlags			videoCodecOperationFlags,
													const VideoDevice::VideoDeviceFlags		videoDeviceFlags)
{
#ifndef CTS_USES_VULKANSC
	if (*m_logicalDevice == DE_NULL)
	{
		DE_ASSERT(static_cast<deUint32>(queueFlagsRequired) != 0u);
		DE_ASSERT(static_cast<deUint32>(videoCodecOperationFlags) != 0u);

		if (!createDeviceSupportingQueue(queueFlagsRequired, videoCodecOperationFlags, videoDeviceFlags))
			TCU_THROW(NotSupportedError, "Cannot create device with required parameters");
	}

	return *m_logicalDevice;
#else
	DE_UNREF(queueFlagsRequired);
	DE_UNREF(videoCodecOperationFlags);
	DE_UNREF(videoDeviceFlags);

	TCU_THROW(NotSupportedError, "Video is not supported for Vulkan SC");
#endif
}

bool VideoDevice::createDeviceSupportingQueue (const vk::VkQueueFlags			queueFlagsRequired,
											   const VideoCodecOperationFlags	videoCodecOperationFlags,
											   const VideoDeviceFlags			videoDeviceFlags)
{
#ifndef CTS_USES_VULKANSC
	const vk::PlatformInterface&								vkp											= m_context.getPlatformInterface();
	const vk::InstanceInterface&								vki											= m_context.getInstanceInterface();
	const vk::VkPhysicalDevice									physicalDevice								= m_context.getPhysicalDevice();
	const vk::VkInstance										instance									= m_context.getInstance();
	const deUint32												apiVersion									= m_context.getUsedApiVersion();
	const bool													validationEnabled							= m_context.getTestContext().getCommandLine().isValidationEnabled();
	const bool													queryWithStatusForDecodeSupport				= (videoDeviceFlags & VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_DECODE_SUPPORT) != 0;
	const bool													requireYCBCRorNotSupported					= (videoDeviceFlags & VIDEO_DEVICE_FLAG_REQUIRE_YCBCR_OR_NOT_SUPPORTED) != 0;
	const bool													requireSync2orNotSupported					= (videoDeviceFlags & VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED) != 0;
	const float													queueFamilyPriority							= 1.0f;
	deUint32													queueFamilyPropertiesCount					= 0u;
	deUint32													queueFamilyTransfer							= VK_QUEUE_FAMILY_IGNORED;
	deUint32													queueFamilyDecode							= VK_QUEUE_FAMILY_IGNORED;
	deUint32													queueFamilyEncode							= VK_QUEUE_FAMILY_IGNORED;
	vk::VkQueueFlags											queueFlagsFound								= 0;
	vector<vk::VkQueueFamilyProperties2>						queueFamilyProperties2;
	vector<vk::VkQueueFamilyVideoPropertiesKHR>				videoQueueFamilyProperties2;
	vector<vk::VkQueueFamilyQueryResultStatusPropertiesKHR>	VkQueueFamilyQueryResultStatusPropertiesKHR;
	vector<const char*>											deviceExtensions;
	vector<vk::VkDeviceQueueCreateInfo>							queueInfos;

	DE_ASSERT(queueFlagsRequired != 0);
	DE_ASSERT(videoCodecOperationFlags != 0);

	vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount, DE_NULL);

	if(queueFamilyPropertiesCount == 0u)
		TCU_FAIL("Device reports an empty set of queue family properties");

	queueFamilyProperties2.resize(queueFamilyPropertiesCount);
	videoQueueFamilyProperties2.resize(queueFamilyPropertiesCount);
	VkQueueFamilyQueryResultStatusPropertiesKHR.resize(queueFamilyPropertiesCount);

	for (size_t ndx = 0; ndx < queueFamilyPropertiesCount; ++ndx)
	{
		queueFamilyProperties2[ndx].sType											= vk::VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		queueFamilyProperties2[ndx].pNext											= &videoQueueFamilyProperties2[ndx];
		videoQueueFamilyProperties2[ndx].sType										= vk::VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
		videoQueueFamilyProperties2[ndx].pNext										= &VkQueueFamilyQueryResultStatusPropertiesKHR[ndx];
		videoQueueFamilyProperties2[ndx].videoCodecOperations						= 0;
		VkQueueFamilyQueryResultStatusPropertiesKHR[ndx].sType						= vk::VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR;
		VkQueueFamilyQueryResultStatusPropertiesKHR[ndx].pNext						= DE_NULL;
		VkQueueFamilyQueryResultStatusPropertiesKHR[ndx].queryResultStatusSupport	= DE_FALSE;
	}

	vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties2.data());

	if (queueFamilyPropertiesCount != queueFamilyProperties2.size())
		TCU_FAIL("Device returns less queue families than initially reported");

	for (uint32_t ndx = 0; ndx < queueFamilyPropertiesCount; ++ndx)
	{
		const vk::VkQueueFamilyProperties&	queueFamilyProperties	= queueFamilyProperties2[ndx].queueFamilyProperties;
		const vk::VkQueueFlags				usefulQueueFlags		= queueFamilyProperties.queueFlags & queueFlagsRequired & ~queueFlagsFound;

		if (usefulQueueFlags != 0)
		{
			bool	assigned	= false;

			if ((usefulQueueFlags & vk::VK_QUEUE_TRANSFER_BIT) != 0 && queueFamilyTransfer == VK_QUEUE_FAMILY_IGNORED)
			{
				queueFamilyTransfer	= ndx;
				assigned			= true;
			}

			if ((videoQueueFamilyProperties2[ndx].videoCodecOperations & videoCodecOperationFlags) != 0)
			{
				if ((usefulQueueFlags & vk::VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0 && queueFamilyDecode == VK_QUEUE_FAMILY_IGNORED)
				{
					if (!queryWithStatusForDecodeSupport || (queryWithStatusForDecodeSupport && VkQueueFamilyQueryResultStatusPropertiesKHR[ndx].queryResultStatusSupport))
					{
						queueFamilyDecode	= ndx;
						assigned			= true;
					}
				}

				if ((usefulQueueFlags & vk::VK_QUEUE_VIDEO_ENCODE_BIT_KHR) != 0 && queueFamilyEncode == VK_QUEUE_FAMILY_IGNORED)
				{
					queueFamilyEncode	= ndx;
					assigned			= true;
				}
			}

			if (assigned)
			{
				const vk::VkDeviceQueueCreateInfo	queueInfo =
				{
					vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	//  VkStructureType				sType;
					DE_NULL,										//  const void*					pNext;
					(vk::VkDeviceQueueCreateFlags)0u,				//  VkDeviceQueueCreateFlags	flags;
					ndx,											//  deUint32					queueFamilyIndex;
					1u,												//  deUint32					queueCount;
					&queueFamilyPriority,							//  const float*				pQueuePriorities;
				};

				if (queueFamilyProperties.queueCount == 0)
					TCU_FAIL("Video queue returned queueCount is zero");

				queueInfos.push_back(queueInfo);

				queueFlagsFound |= usefulQueueFlags;

				if (queueFlagsFound == queueFlagsRequired)
					break;
			}
		}
	}

	if (queueFlagsFound != queueFlagsRequired)
		return false;

	addVideoDeviceExtensions(deviceExtensions, apiVersion, queueFlagsRequired, videoCodecOperationFlags);

	if (requireYCBCRorNotSupported)
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_KHR_sampler_ycbcr_conversion"))
			deviceExtensions.push_back("VK_KHR_sampler_ycbcr_conversion");

	if (requireSync2orNotSupported)
		if (!vk::isCoreDeviceExtension(apiVersion, "VK_KHR_synchronization2"))
			deviceExtensions.push_back("VK_KHR_synchronization2");

	vk::VkPhysicalDeviceSynchronization2FeaturesKHR		synchronization2Features		=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,	//  VkStructureType	sType;
		DE_NULL,																//  void*			pNext;
		DE_FALSE,																//  VkBool32		synchronization2;
	};
	vk::VkPhysicalDeviceSamplerYcbcrConversionFeatures	samplerYcbcrConversionFeatures	=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,	//  VkStructureType	sType;
		DE_NULL,																	//  void*			pNext;
		DE_FALSE,																	//  VkBool32		samplerYcbcrConversion;
	};
	vk::VkPhysicalDeviceFeatures2						features2						=
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,	//  VkStructureType				sType;
		DE_NULL,											//  void*						pNext;
		vk::VkPhysicalDeviceFeatures(),						//  VkPhysicalDeviceFeatures	features;
	};

	if (requireYCBCRorNotSupported)
		appendStructurePtrToVulkanChain((const void**)&features2.pNext, &samplerYcbcrConversionFeatures);

	if (requireSync2orNotSupported)
		appendStructurePtrToVulkanChain((const void**)&features2.pNext, &synchronization2Features);

	vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

	if (requireYCBCRorNotSupported && samplerYcbcrConversionFeatures.samplerYcbcrConversion == DE_FALSE)
		TCU_THROW(NotSupportedError, "samplerYcbcrConversionFeatures.samplerYcbcrConversion is required");

	if (requireSync2orNotSupported && synchronization2Features.synchronization2 == DE_FALSE)
		TCU_THROW(NotSupportedError, "synchronization2Features.synchronization2 is required");

	features2.features.robustBufferAccess = DE_FALSE;

	const vk::VkDeviceCreateInfo						deviceCreateInfo				=
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	//  VkStructureType					sType;
		&features2,									//  const void*						pNext;
		(vk::VkDeviceCreateFlags)0,					//  VkDeviceCreateFlags				flags;
		static_cast<uint32_t>(queueInfos.size()),	//  deUint32						queueCreateInfoCount;
		de::dataOrNull(queueInfos),					//  const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,											//  deUint32						enabledLayerCount;
		DE_NULL,									//  const char* const*				ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),			//  deUint32						enabledExtensionCount;
		de::dataOrNull(deviceExtensions),			//  const char* const*				ppEnabledExtensionNames;
		DE_NULL,									//  const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	m_logicalDevice			= createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceCreateInfo);
	m_deviceDriver			= de::MovePtr<vk::DeviceDriver>(new vk::DeviceDriver(vkp, instance, *m_logicalDevice));
	m_allocator				= de::MovePtr<vk::Allocator>(new vk::SimpleAllocator(*m_deviceDriver, *m_logicalDevice, getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	m_queueFamilyTransfer	= queueFamilyTransfer;
	m_queueFamilyDecode		= queueFamilyDecode;
	m_queueFamilyEncode		= queueFamilyEncode;
	m_videoCodecOperation	= videoCodecOperationFlags;

	return true;
#else
	DE_UNREF(queueFlagsRequired);
	DE_UNREF(videoCodecOperationFlags);
	DE_UNREF(videoDeviceFlags);

	TCU_THROW(NotSupportedError, "Video is not supported for Vulkan SC");
#endif
}

const vk::DeviceDriver& VideoDevice::getDeviceDriver (void)
{
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(m_deviceDriver.get() != DE_NULL);

	return *m_deviceDriver;
#else
	TCU_THROW(NotSupportedError, "Video is not supported for Vulkan SC");
#endif
}

deUint32 VideoDevice::getQueueFamilyIndexTransfer (void) const
{
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(m_queueFamilyTransfer != VK_QUEUE_FAMILY_IGNORED);

	return m_queueFamilyTransfer;
#else
	TCU_THROW(NotSupportedError, "Video is not supported for Vulkan SC");
#endif
}

deUint32 VideoDevice::getQueueFamilyIndexDecode (void) const
{
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(m_queueFamilyDecode != VK_QUEUE_FAMILY_IGNORED);

	return m_queueFamilyDecode;
#else
	TCU_THROW(NotSupportedError, "Video is not supported for Vulkan SC");
#endif
}

deUint32 VideoDevice::getQueueFamilyIndexEncode (void) const
{
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(m_queueFamilyEncode != VK_QUEUE_FAMILY_IGNORED);

	return m_queueFamilyEncode;
#else
	TCU_THROW(NotSupportedError, "Video is not supported for Vulkan SC");
#endif
}

deUint32 VideoDevice::getQueueFamilyVideo (void) const
{
#ifndef CTS_USES_VULKANSC
	const bool encode = isVideoEncodeOperation(m_videoCodecOperation);
	const bool decode = isVideoDecodeOperation(m_videoCodecOperation);

	DE_ASSERT((encode && !decode) || (!encode && decode));
	DE_UNREF(decode);

	return encode ? getQueueFamilyIndexEncode() : getQueueFamilyIndexDecode();
#else
	TCU_THROW(NotSupportedError, "Video is not supported for Vulkan SC");
#endif
}

vk::Allocator& VideoDevice::getAllocator (void)
{
#ifndef CTS_USES_VULKANSC
	DE_ASSERT(m_allocator != DE_NULL);

	return *m_allocator;
#else
	TCU_THROW(NotSupportedError, "Video is not supported for Vulkan SC");
#endif
}

}
