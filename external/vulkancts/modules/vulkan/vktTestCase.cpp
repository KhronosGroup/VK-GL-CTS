/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
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
 * \brief Vulkan test case base classes
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkDebugReportUtil.hpp"
#include "vkDeviceFeatures.hpp"
#include "vkDeviceProperties.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deSTLUtil.hpp"
#include "deMemory.h"

#include <set>

namespace vkt
{

// Default device utilities

using std::vector;
using std::string;
using std::set;
using namespace vk;

namespace
{

vector<string> filterExtensions (const vector<VkExtensionProperties>& extensions)
{
	vector<string>	enabledExtensions;
	bool			khrBufferDeviceAddress	= false;

	const char*		extensionGroups[]		=
	{
		"VK_KHR_",
		"VK_EXT_",
		"VK_KHX_",
		"VK_NV_cooperative_matrix",
		"VK_NV_ray_tracing",
		"VK_NV_inherited_viewport_scissor",
		"VK_NV_mesh_shader",
		"VK_AMD_mixed_attachment_samples",
		"VK_AMD_shader_fragment_mask",
		"VK_AMD_buffer_marker",
		"VK_AMD_shader_explicit_vertex_parameter",
		"VK_AMD_shader_image_load_store_lod",
		"VK_AMD_shader_trinary_minmax",
		"VK_AMD_texture_gather_bias_lod",
		"VK_ANDROID_external_memory_android_hardware_buffer",
		"VK_VALVE_mutable_descriptor_type",
		"VK_NV_shader_subgroup_partitioned",
		"VK_NV_clip_space_w_scaling",
		"VK_NV_scissor_exclusive",
		"VK_NV_shading_rate_image",
		"VK_GOOGLE_surfaceless_query",
	};

	for (size_t extNdx = 0; extNdx < extensions.size(); extNdx++)
	{
		if (deStringEqual(extensions[extNdx].extensionName, "VK_KHR_buffer_device_address"))
		{
			khrBufferDeviceAddress = true;
			break;
		}
	}

	for (size_t extNdx = 0; extNdx < extensions.size(); extNdx++)
	{
		const auto& extName = extensions[extNdx].extensionName;

		// Skip enabling VK_KHR_pipeline_library unless needed.
		if (deStringEqual(extName, "VK_KHR_pipeline_library"))
			continue;

		// VK_EXT_buffer_device_address is deprecated and must not be enabled if VK_KHR_buffer_device_address is enabled
		if (khrBufferDeviceAddress && deStringEqual(extName, "VK_EXT_buffer_device_address"))
			continue;

		for (int extGroupNdx = 0; extGroupNdx < DE_LENGTH_OF_ARRAY(extensionGroups); extGroupNdx++)
		{
			if (deStringBeginsWith(extName, extensionGroups[extGroupNdx]))
				enabledExtensions.push_back(extName);
		}
	}

	return enabledExtensions;
}

vector<string> addExtensions (const vector<string>& a, const vector<const char*>& b)
{
	vector<string>	res		(a);

	for (vector<const char*>::const_iterator bIter = b.begin(); bIter != b.end(); ++bIter)
	{
		if (!de::contains(res.begin(), res.end(), string(*bIter)))
			res.push_back(string(*bIter));
	}

	return res;
}

vector<string> removeExtensions (const vector<string>& a, const vector<const char*>& b)
{
	vector<string>	res;
	set<string>		removeExts	(b.begin(), b.end());

	for (vector<string>::const_iterator aIter = a.begin(); aIter != a.end(); ++aIter)
	{
		if (!de::contains(removeExts, *aIter))
			res.push_back(*aIter);
	}

	return res;
}

vector<string> addCoreInstanceExtensions (const vector<string>& extensions, deUint32 instanceVersion)
{
	vector<const char*> coreExtensions;
	getCoreInstanceExtensions(instanceVersion, coreExtensions);
	return addExtensions(extensions, coreExtensions);
}

vector<string> addCoreDeviceExtensions(const vector<string>& extensions, deUint32 instanceVersion)
{
	vector<const char*> coreExtensions;
	getCoreDeviceExtensions(instanceVersion, coreExtensions);
	return addExtensions(extensions, coreExtensions);
}

deUint32 getTargetInstanceVersion (const PlatformInterface& vkp)
{
	deUint32 version = pack(ApiVersion(1, 0, 0));

	if (vkp.enumerateInstanceVersion(&version) != VK_SUCCESS)
		TCU_THROW(InternalError, "Enumerate instance version error");
	return version;
}

std::pair<deUint32, deUint32> determineDeviceVersions(const PlatformInterface& vkp, deUint32 apiVersion, const tcu::CommandLine& cmdLine)
{
	Move<VkInstance>						preinstance				= createDefaultInstance(vkp, apiVersion);
	InstanceDriver							preinterface			(vkp, preinstance.get());

	const vector<VkPhysicalDevice>			devices					= enumeratePhysicalDevices(preinterface, preinstance.get());
	deUint32								lowestDeviceVersion		= 0xFFFFFFFFu;
	for (deUint32 deviceNdx = 0u; deviceNdx < devices.size(); ++deviceNdx)
	{
		const VkPhysicalDeviceProperties	props					= getPhysicalDeviceProperties(preinterface, devices[deviceNdx]);
		if (props.apiVersion < lowestDeviceVersion)
			lowestDeviceVersion = props.apiVersion;
	}

	const vk::VkPhysicalDevice				choosenDevice			= chooseDevice(preinterface, *preinstance, cmdLine);
	const VkPhysicalDeviceProperties		props					= getPhysicalDeviceProperties(preinterface, choosenDevice);
	const deUint32							choosenDeviceVersion	= props.apiVersion;

	return std::make_pair(choosenDeviceVersion, lowestDeviceVersion);
}


Move<VkInstance> createInstance (const PlatformInterface& vkp, deUint32 apiVersion, const vector<string>& enabledExtensions, DebugReportRecorder* recorder)
{
	const bool			isValidationEnabled	= (recorder != nullptr);
	vector<const char*>	enabledLayers;

	// \note Extensions in core are not explicitly enabled even though
	//		 they are in the extension list advertised to tests.
	vector<const char*> coreExtensions;
	getCoreInstanceExtensions(apiVersion, coreExtensions);
	const auto nonCoreExtensions = removeExtensions(enabledExtensions, coreExtensions);

	if (isValidationEnabled)
	{
		if (!isDebugReportSupported(vkp))
			TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");

		enabledLayers = vkt::getValidationLayers(vkp);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	return createDefaultInstance(vkp, apiVersion, vector<string>(begin(enabledLayers), end(enabledLayers)), nonCoreExtensions, recorder);
}

static deUint32 findQueueFamilyIndexWithCaps (const InstanceInterface& vkInstance, VkPhysicalDevice physicalDevice, VkQueueFlags requiredCaps)
{
	const vector<VkQueueFamilyProperties>	queueProps	= getPhysicalDeviceQueueFamilyProperties(vkInstance, physicalDevice);

	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if ((queueProps[queueNdx].queueFlags & requiredCaps) == requiredCaps)
			return (deUint32)queueNdx;
	}

	TCU_THROW(NotSupportedError, "No matching queue found");
}

Move<VkDevice> createDefaultDevice (const PlatformInterface&			vkp,
									VkInstance							instance,
									const InstanceInterface&			vki,
									VkPhysicalDevice					physicalDevice,
									const deUint32						apiVersion,
									deUint32							queueIndex,
									deUint32							sparseQueueIndex,
									const VkPhysicalDeviceFeatures2&	enabledFeatures,
									const vector<string>&				enabledExtensions,
									const tcu::CommandLine&				cmdLine)
{
	VkDeviceQueueCreateInfo		queueInfo[2];
	VkDeviceCreateInfo			deviceInfo;
	vector<const char*>			enabledLayers;
	vector<const char*>			extensionPtrs;
	const float					queuePriority	= 1.0f;
	const deUint32				numQueues = (enabledFeatures.features.sparseBinding && (queueIndex != sparseQueueIndex)) ? 2 : 1;

	deMemset(&queueInfo,	0, sizeof(queueInfo));
	deMemset(&deviceInfo,	0, sizeof(deviceInfo));

	if (cmdLine.isValidationEnabled())
	{
		enabledLayers = vkt::getValidationLayers(vki, physicalDevice);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	// \note Extensions in core are not explicitly enabled even though
	//		 they are in the extension list advertised to tests.
	vector<const char*> coreExtensions;
	getCoreDeviceExtensions(apiVersion, coreExtensions);
	vector<string>	nonCoreExtensions(removeExtensions(enabledExtensions, coreExtensions));

	extensionPtrs.resize(nonCoreExtensions.size());

	for (size_t ndx = 0; ndx < nonCoreExtensions.size(); ++ndx)
		extensionPtrs[ndx] = nonCoreExtensions[ndx].c_str();

	queueInfo[0].sType						= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo[0].pNext						= DE_NULL;
	queueInfo[0].flags						= (VkDeviceQueueCreateFlags)0u;
	queueInfo[0].queueFamilyIndex			= queueIndex;
	queueInfo[0].queueCount					= 1u;
	queueInfo[0].pQueuePriorities			= &queuePriority;

	if (numQueues > 1)
	{
		queueInfo[1].sType						= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo[1].pNext						= DE_NULL;
		queueInfo[1].flags						= (VkDeviceQueueCreateFlags)0u;
		queueInfo[1].queueFamilyIndex			= sparseQueueIndex;
		queueInfo[1].queueCount					= 1u;
		queueInfo[1].pQueuePriorities			= &queuePriority;
	}

	// VK_KHR_get_physical_device_properties2 is used if enabledFeatures.pNext != 0
	deviceInfo.sType						= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext						= enabledFeatures.pNext ? &enabledFeatures : DE_NULL;
	deviceInfo.queueCreateInfoCount			= numQueues;
	deviceInfo.pQueueCreateInfos			= queueInfo;
	deviceInfo.enabledExtensionCount		= (deUint32)extensionPtrs.size();
	deviceInfo.ppEnabledExtensionNames		= (extensionPtrs.empty() ? DE_NULL : &extensionPtrs[0]);
	deviceInfo.enabledLayerCount			= (deUint32)enabledLayers.size();
	deviceInfo.ppEnabledLayerNames			= (enabledLayers.empty() ? DE_NULL : enabledLayers.data());
	deviceInfo.pEnabledFeatures				= enabledFeatures.pNext ? DE_NULL : &enabledFeatures.features;

	return createDevice(vkp, instance, vki, physicalDevice, &deviceInfo);
}

} // anonymous

class DefaultDevice
{
public:
																	DefaultDevice							(const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine);
																	~DefaultDevice							(void);

	VkInstance														getInstance								(void) const { return *m_instance;											}
	const InstanceInterface&										getInstanceInterface					(void) const { return m_instanceInterface;									}
	deUint32														getMaximumFrameworkVulkanVersion		(void) const { return m_maximumFrameworkVulkanVersion;						}
	deUint32														getAvailableInstanceVersion				(void) const { return m_availableInstanceVersion;							}
	deUint32														getUsedInstanceVersion					(void) const { return m_usedInstanceVersion;								}
	const vector<string>&											getInstanceExtensions					(void) const { return m_instanceExtensions;									}

	VkPhysicalDevice												getPhysicalDevice						(void) const { return m_physicalDevice;										}
	deUint32														getDeviceVersion						(void) const { return m_deviceVersion;										}

	bool															isDeviceFeatureInitialized				(VkStructureType sType) const { return m_deviceFeatures.isDeviceFeatureInitialized(sType);		}
	const VkPhysicalDeviceFeatures&									getDeviceFeatures						(void) const { return m_deviceFeatures.getCoreFeatures2().features;			}
	const VkPhysicalDeviceFeatures2&								getDeviceFeatures2						(void) const { return m_deviceFeatures.getCoreFeatures2();					}
	const VkPhysicalDeviceVulkan11Features&							getVulkan11Features						(void) const { return m_deviceFeatures.getVulkan11Features();				}
	const VkPhysicalDeviceVulkan12Features&							getVulkan12Features						(void) const { return m_deviceFeatures.getVulkan12Features();				}
	const VkPhysicalDeviceVulkan13Features&							getVulkan13Features						(void) const { return m_deviceFeatures.getVulkan13Features();				}

#include "vkDeviceFeaturesForDefaultDeviceDefs.inl"

	bool															isDevicePropertyInitialized				(VkStructureType sType) const { return m_deviceProperties.isDevicePropertyInitialized(sType);	}
	const VkPhysicalDeviceProperties&								getDeviceProperties						(void) const { return m_deviceProperties.getCoreProperties2().properties;	}
	const VkPhysicalDeviceProperties2&								getDeviceProperties2					(void) const { return m_deviceProperties.getCoreProperties2();				}
	const VkPhysicalDeviceVulkan11Properties&						getDeviceVulkan11Properties				(void) const { return m_deviceProperties.getVulkan11Properties();			}
	const VkPhysicalDeviceVulkan12Properties&						getDeviceVulkan12Properties				(void) const { return m_deviceProperties.getVulkan12Properties();			}
	const VkPhysicalDeviceVulkan13Properties&						getDeviceVulkan13Properties				(void) const { return m_deviceProperties.getVulkan13Properties();			}

#include "vkDevicePropertiesForDefaultDeviceDefs.inl"

	VkDevice														getDevice								(void) const { return *m_device;											}
	const DeviceInterface&											getDeviceInterface						(void) const { return m_deviceInterface;									}
	const vector<string>&											getDeviceExtensions						(void) const { return m_deviceExtensions;									}
	deUint32														getUsedApiVersion						(void) const { return m_usedApiVersion;										}
	deUint32														getUniversalQueueFamilyIndex			(void) const { return m_universalQueueFamilyIndex;							}
	VkQueue															getUniversalQueue						(void) const;
	deUint32														getSparseQueueFamilyIndex				(void) const { return m_sparseQueueFamilyIndex;								}
	VkQueue															getSparseQueue							(void) const;

	bool															hasDebugReportRecorder					(void) const { return m_debugReportRecorder.get() != nullptr;				}
	vk::DebugReportRecorder&										getDebugReportRecorder					(void) const { return *m_debugReportRecorder.get();							}

private:
	using DebugReportRecorderPtr		= de::UniquePtr<vk::DebugReportRecorder>;
	using DebugReportCallbackPtr		= vk::Move<VkDebugReportCallbackEXT>;

	const deUint32						m_maximumFrameworkVulkanVersion;
	const deUint32						m_availableInstanceVersion;
	const deUint32						m_usedInstanceVersion;

	const std::pair<deUint32, deUint32> m_deviceVersions;
	const deUint32						m_usedApiVersion;

	const DebugReportRecorderPtr		m_debugReportRecorder;
	const vector<string>				m_instanceExtensions;
	const Unique<VkInstance>			m_instance;
	const InstanceDriver				m_instanceInterface;
	const DebugReportCallbackPtr		m_debugReportCallback;

	const VkPhysicalDevice				m_physicalDevice;
	const deUint32						m_deviceVersion;

	const vector<string>				m_deviceExtensions;
	const DeviceFeatures				m_deviceFeatures;

	const deUint32						m_universalQueueFamilyIndex;
	const deUint32						m_sparseQueueFamilyIndex;
	const DeviceProperties				m_deviceProperties;

	const Unique<VkDevice>				m_device;
	const DeviceDriver					m_deviceInterface;
};

namespace
{

deUint32 sanitizeApiVersion(deUint32 v)
{
	return VK_MAKE_VERSION(VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), 0 );
}

de::MovePtr<vk::DebugReportRecorder> createDebugReportRecorder (const vk::PlatformInterface& vkp, bool printValidationErrors)
{
	if (isDebugReportSupported(vkp))
		return de::MovePtr<vk::DebugReportRecorder>(new vk::DebugReportRecorder(printValidationErrors));
	else
		TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");
}

} // anonymous

DefaultDevice::DefaultDevice (const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine)
	: m_maximumFrameworkVulkanVersion	(VK_API_MAX_FRAMEWORK_VERSION)
	, m_availableInstanceVersion		(getTargetInstanceVersion(vkPlatform))
	, m_usedInstanceVersion				(sanitizeApiVersion(deMinu32(m_availableInstanceVersion, m_maximumFrameworkVulkanVersion)))
	, m_deviceVersions					(determineDeviceVersions(vkPlatform, m_usedInstanceVersion, cmdLine))
	, m_usedApiVersion					(sanitizeApiVersion(deMinu32(m_usedInstanceVersion, m_deviceVersions.first)))

	, m_debugReportRecorder				(cmdLine.isValidationEnabled()
										 ? createDebugReportRecorder(vkPlatform, cmdLine.printValidationErrors())
										 : de::MovePtr<vk::DebugReportRecorder>())
	, m_instanceExtensions				(addCoreInstanceExtensions(filterExtensions(enumerateInstanceExtensionProperties(vkPlatform, DE_NULL)), m_usedApiVersion))
	, m_instance						(createInstance(vkPlatform, m_usedApiVersion, m_instanceExtensions, m_debugReportRecorder.get()))

	, m_instanceInterface				(vkPlatform, *m_instance)
	, m_debugReportCallback				(cmdLine.isValidationEnabled()
										 ? m_debugReportRecorder->createCallback(m_instanceInterface, m_instance.get())
										 : DebugReportCallbackPtr())
	, m_physicalDevice					(chooseDevice(m_instanceInterface, *m_instance, cmdLine))
	, m_deviceVersion					(getPhysicalDeviceProperties(m_instanceInterface, m_physicalDevice).apiVersion)

	, m_deviceExtensions				(addCoreDeviceExtensions(filterExtensions(enumerateDeviceExtensionProperties(m_instanceInterface, m_physicalDevice, DE_NULL)), m_usedApiVersion))
	, m_deviceFeatures					(m_instanceInterface, m_usedApiVersion, m_physicalDevice, m_instanceExtensions, m_deviceExtensions)
	, m_universalQueueFamilyIndex		(findQueueFamilyIndexWithCaps(m_instanceInterface, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))
	, m_sparseQueueFamilyIndex			(m_deviceFeatures.getCoreFeatures2().features.sparseBinding ? findQueueFamilyIndexWithCaps(m_instanceInterface, m_physicalDevice, VK_QUEUE_SPARSE_BINDING_BIT) : 0)
	, m_deviceProperties				(m_instanceInterface, m_usedApiVersion, m_physicalDevice, m_instanceExtensions, m_deviceExtensions)
	, m_device							(createDefaultDevice(vkPlatform, *m_instance, m_instanceInterface, m_physicalDevice, m_usedApiVersion, m_universalQueueFamilyIndex, m_sparseQueueFamilyIndex, m_deviceFeatures.getCoreFeatures2(), m_deviceExtensions, cmdLine))
	, m_deviceInterface					(vkPlatform, *m_instance, *m_device)
{
	DE_ASSERT(m_deviceVersions.first == m_deviceVersion);
}

DefaultDevice::~DefaultDevice (void)
{
}

VkQueue DefaultDevice::getUniversalQueue (void) const
{
	return getDeviceQueue(m_deviceInterface, *m_device, m_universalQueueFamilyIndex, 0);
}

VkQueue DefaultDevice::getSparseQueue (void) const
{
	if (!m_deviceFeatures.getCoreFeatures2().features.sparseBinding)
		TCU_THROW(NotSupportedError, "Sparse binding not supported.");

	return getDeviceQueue(m_deviceInterface, *m_device, m_sparseQueueFamilyIndex, 0);
}

namespace
{
// Allocator utilities

vk::Allocator* createAllocator (DefaultDevice* device)
{
	const VkPhysicalDeviceMemoryProperties memoryProperties = vk::getPhysicalDeviceMemoryProperties(device->getInstanceInterface(), device->getPhysicalDevice());

	// \todo [2015-07-24 jarkko] support allocator selection/configuration from command line (or compile time)
	return new SimpleAllocator(device->getDeviceInterface(), device->getDevice(), memoryProperties);
}

} // anonymous

// Context

Context::Context (tcu::TestContext&				testCtx,
				  const vk::PlatformInterface&	platformInterface,
				  vk::BinaryCollection&			progCollection)
	: m_testCtx					(testCtx)
	, m_platformInterface		(platformInterface)
	, m_progCollection			(progCollection)
	, m_device					(new DefaultDevice(m_platformInterface, testCtx.getCommandLine()))
	, m_allocator				(createAllocator(m_device.get()))
	, m_resultSetOnValidation	(false)
{
}

Context::~Context (void)
{
}

deUint32										Context::getMaximumFrameworkVulkanVersion		(void) const { return m_device->getMaximumFrameworkVulkanVersion();			}
deUint32										Context::getAvailableInstanceVersion			(void) const { return m_device->getAvailableInstanceVersion();				}
const vector<string>&							Context::getInstanceExtensions					(void) const { return m_device->getInstanceExtensions();					}
vk::VkInstance									Context::getInstance							(void) const { return m_device->getInstance();								}
const vk::InstanceInterface&					Context::getInstanceInterface					(void) const { return m_device->getInstanceInterface();						}
vk::VkPhysicalDevice							Context::getPhysicalDevice						(void) const { return m_device->getPhysicalDevice();						}
deUint32										Context::getDeviceVersion						(void) const { return m_device->getDeviceVersion();							}
const vk::VkPhysicalDeviceFeatures&				Context::getDeviceFeatures						(void) const { return m_device->getDeviceFeatures();						}
const vk::VkPhysicalDeviceFeatures2&			Context::getDeviceFeatures2						(void) const { return m_device->getDeviceFeatures2();						}
const vk::VkPhysicalDeviceVulkan11Features&		Context::getDeviceVulkan11Features				(void) const { return m_device->getVulkan11Features();						}
const vk::VkPhysicalDeviceVulkan12Features&		Context::getDeviceVulkan12Features				(void) const { return m_device->getVulkan12Features();						}
const vk::VkPhysicalDeviceVulkan13Features&		Context::getDeviceVulkan13Features				(void) const { return m_device->getVulkan13Features();						}

bool Context::isDeviceFunctionalitySupported (const std::string& extension) const
{
	// check if extension was promoted to core
	deUint32 apiVersion = getUsedApiVersion();
	if (isCoreDeviceExtension(apiVersion, extension))
	{
		if (apiVersion < VK_MAKE_VERSION(1, 2, 0))
		{
			// Check feature bits in extension-specific structures.
			if (extension == "VK_KHR_multiview")
				return !!m_device->getMultiviewFeatures().multiview;
			if (extension == "VK_KHR_variable_pointers")
				return !!m_device->getVariablePointersFeatures().variablePointersStorageBuffer;
			if (extension == "VK_KHR_sampler_ycbcr_conversion")
				return !!m_device->getSamplerYcbcrConversionFeatures().samplerYcbcrConversion;
			if (extension == "VK_KHR_shader_draw_parameters")
				return !!m_device->getShaderDrawParametersFeatures().shaderDrawParameters;
		}
		else
		{
			// Check feature bits using the new Vulkan 1.2 structures.
			const auto& vk11Features = m_device->getVulkan11Features();
			if (extension == "VK_KHR_multiview")
				return !!vk11Features.multiview;
			if (extension == "VK_KHR_variable_pointers")
				return !!vk11Features.variablePointersStorageBuffer;
			if (extension == "VK_KHR_sampler_ycbcr_conversion")
				return !!vk11Features.samplerYcbcrConversion;
			if (extension == "VK_KHR_shader_draw_parameters")
				return !!vk11Features.shaderDrawParameters;

			const auto& vk12Features = m_device->getVulkan12Features();
			if (extension == "VK_KHR_timeline_semaphore")
				return !!vk12Features.timelineSemaphore;
			if (extension == "VK_KHR_buffer_device_address")
				return !!vk12Features.bufferDeviceAddress;
			if (extension == "VK_EXT_descriptor_indexing")
				return !!vk12Features.descriptorIndexing;
			if (extension == "VK_KHR_draw_indirect_count")
				return !!vk12Features.drawIndirectCount;
			if (extension == "VK_KHR_sampler_mirror_clamp_to_edge")
				return !!vk12Features.samplerMirrorClampToEdge;
			if (extension == "VK_EXT_sampler_filter_minmax")
				return !!vk12Features.samplerFilterMinmax;
			if (extension == "VK_EXT_shader_viewport_index_layer")
				return !!vk12Features.shaderOutputViewportIndex && !!vk12Features.shaderOutputLayer;

			const auto& vk13Features = m_device->getVulkan13Features();
			if (extension == "VK_EXT_image_robustness")
				return !!vk13Features.robustImageAccess;
			if (extension == "VK_EXT_inline_uniform_block")
				return !!vk13Features.inlineUniformBlock;
			if (extension == "VK_EXT_pipeline_creation_cache_control")
				return !!vk13Features.pipelineCreationCacheControl;
			if (extension == "VK_EXT_private_data")
				return !!vk13Features.privateData;
			if (extension == "VK_EXT_shader_demote_to_helper_invocation")
				return !!vk13Features.shaderDemoteToHelperInvocation;
			if (extension == "VK_KHR_shader_terminate_invocation")
				return !!vk13Features.shaderTerminateInvocation;
			if (extension == "VK_EXT_subgroup_size_control")
				return !!vk13Features.subgroupSizeControl;
			if (extension == "VK_KHR_synchronization2")
				return !!vk13Features.synchronization2;
			if (extension == "VK_EXT_texture_compression_astc_hdr")
				return !!vk13Features.textureCompressionASTC_HDR;
			if (extension == "VK_KHR_zero_initialize_workgroup_memory")
				return !!vk13Features.shaderZeroInitializeWorkgroupMemory;
			if (extension == "VK_KHR_dynamic_rendering")
				return !!vk13Features.dynamicRendering;
			if (extension == "VK_KHR_shader_integer_dot_product")
				return !!vk13Features.shaderIntegerDotProduct;
			if (extension == "VK_KHR_maintenance4")
				return !!vk13Features.maintenance4;
		}

		// No feature flags to check.
		return true;
	}

	// check if extension is on the list of extensions for current device
	const auto& extensions = getDeviceExtensions();
	if (de::contains(extensions.begin(), extensions.end(), extension))
	{
		if (extension == "VK_KHR_timeline_semaphore")
			return !!getTimelineSemaphoreFeatures().timelineSemaphore;
		if (extension == "VK_KHR_synchronization2")
			return !!getSynchronization2Features().synchronization2;
		if (extension == "VK_EXT_extended_dynamic_state")
			return !!getExtendedDynamicStateFeaturesEXT().extendedDynamicState;
		if (extension == "VK_EXT_shader_demote_to_helper_invocation")
			return !!getShaderDemoteToHelperInvocationFeatures().shaderDemoteToHelperInvocation;
		if (extension == "VK_KHR_workgroup_memory_explicit_layout")
			return !!getWorkgroupMemoryExplicitLayoutFeatures().workgroupMemoryExplicitLayout;

		return true;
	}

	return false;
}

bool Context::isInstanceFunctionalitySupported(const std::string& extension) const
{
	// NOTE: current implementation uses isInstanceExtensionSupported but
	// this will change when some instance extensions will be promoted to the
	// core; don't use isInstanceExtensionSupported directly, use this method instead
	return isInstanceExtensionSupported(getUsedApiVersion(), getInstanceExtensions(), extension);
}

#include "vkDeviceFeaturesForContextDefs.inl"

const vk::VkPhysicalDeviceProperties&			Context::getDeviceProperties				(void) const { return m_device->getDeviceProperties();			}
const vk::VkPhysicalDeviceProperties2&			Context::getDeviceProperties2				(void) const { return m_device->getDeviceProperties2();			}
const vk::VkPhysicalDeviceVulkan11Properties&	Context::getDeviceVulkan11Properties		(void) const { return m_device->getDeviceVulkan11Properties();	}
const vk::VkPhysicalDeviceVulkan12Properties&	Context::getDeviceVulkan12Properties		(void) const { return m_device->getDeviceVulkan12Properties();	}
const vk::VkPhysicalDeviceVulkan13Properties&	Context::getDeviceVulkan13Properties		(void) const { return m_device->getDeviceVulkan13Properties();	}

#include "vkDevicePropertiesForContextDefs.inl"

const vector<string>&					Context::getDeviceExtensions				(void) const { return m_device->getDeviceExtensions();			}
vk::VkDevice							Context::getDevice							(void) const { return m_device->getDevice();					}
const vk::DeviceInterface&				Context::getDeviceInterface					(void) const { return m_device->getDeviceInterface();			}
deUint32								Context::getUniversalQueueFamilyIndex		(void) const { return m_device->getUniversalQueueFamilyIndex();	}
vk::VkQueue								Context::getUniversalQueue					(void) const { return m_device->getUniversalQueue();			}
deUint32								Context::getSparseQueueFamilyIndex			(void) const { return m_device->getSparseQueueFamilyIndex();	}
vk::VkQueue								Context::getSparseQueue						(void) const { return m_device->getSparseQueue();				}
vk::Allocator&							Context::getDefaultAllocator				(void) const { return *m_allocator;								}
deUint32								Context::getUsedApiVersion					(void) const { return m_device->getUsedApiVersion();			}
bool									Context::contextSupports					(const deUint32 majorNum, const deUint32 minorNum, const deUint32 patchNum) const
																							{ return m_device->getUsedApiVersion() >= VK_MAKE_VERSION(majorNum, minorNum, patchNum); }
bool									Context::contextSupports					(const ApiVersion version) const
																							{ return m_device->getUsedApiVersion() >= pack(version); }
bool									Context::contextSupports					(const deUint32 requiredApiVersionBits) const
																							{ return m_device->getUsedApiVersion() >= requiredApiVersionBits; }
bool									Context::isDeviceFeatureInitialized			(vk::VkStructureType sType) const
																							{ return m_device->isDeviceFeatureInitialized(sType);	}
bool									Context::isDevicePropertyInitialized		(vk::VkStructureType sType) const
																							{ return m_device->isDevicePropertyInitialized(sType);	}

bool Context::requireDeviceFunctionality (const std::string& required) const
{
	if (!isDeviceFunctionalitySupported(required))
		TCU_THROW(NotSupportedError, required + " is not supported");

	return true;
}

bool Context::requireInstanceFunctionality (const std::string& required) const
{
	if (!isInstanceFunctionalitySupported(required))
		TCU_THROW(NotSupportedError, required + " is not supported");

	return true;
}

struct DeviceCoreFeaturesTable
{
	const char*		featureName;
	const deUint32	featureArrayIndex;
	const deUint32	featureArrayOffset;
};

#define DEVICE_CORE_FEATURE_OFFSET(FEATURE_FIELD_NAME)	DE_OFFSET_OF(VkPhysicalDeviceFeatures, FEATURE_FIELD_NAME)
#define DEVICE_CORE_FEATURE_ENTRY(BITNAME, FIELDNAME)	{ #FIELDNAME, BITNAME, DEVICE_CORE_FEATURE_OFFSET(FIELDNAME) }

const DeviceCoreFeaturesTable	deviceCoreFeaturesTable[] =
{
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_ROBUST_BUFFER_ACCESS							,	robustBufferAccess						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FULL_DRAW_INDEX_UINT32						,	fullDrawIndexUint32						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY								,	imageCubeArray							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_INDEPENDENT_BLEND								,	independentBlend						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_GEOMETRY_SHADER								,	geometryShader							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TESSELLATION_SHADER							,	tessellationShader						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING							,	sampleRateShading						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DUAL_SRC_BLEND								,	dualSrcBlend							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_LOGIC_OP										,	logicOp									),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_MULTI_DRAW_INDIRECT							,	multiDrawIndirect						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DRAW_INDIRECT_FIRST_INSTANCE					,	drawIndirectFirstInstance				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_CLAMP									,	depthClamp								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_BIAS_CLAMP								,	depthBiasClamp							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FILL_MODE_NON_SOLID							,	fillModeNonSolid						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_BOUNDS									,	depthBounds								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_WIDE_LINES									,	wideLines								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_LARGE_POINTS									,	largePoints								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_ALPHA_TO_ONE									,	alphaToOne								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_MULTI_VIEWPORT								,	multiViewport							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SAMPLER_ANISOTROPY							,	samplerAnisotropy						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_ETC2						,	textureCompressionETC2					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_ASTC_LDR					,	textureCompressionASTC_LDR				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_BC						,	textureCompressionBC					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_OCCLUSION_QUERY_PRECISE						,	occlusionQueryPrecise					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY						,	pipelineStatisticsQuery					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS			,	vertexPipelineStoresAndAtomics			),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS					,	fragmentStoresAndAtomics				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE	,	shaderTessellationAndGeometryPointSize	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_IMAGE_GATHER_EXTENDED					,	shaderImageGatherExtended				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_EXTENDED_FORMATS			,	shaderStorageImageExtendedFormats		),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_MULTISAMPLE				,	shaderStorageImageMultisample			),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT		,	shaderStorageImageReadWithoutFormat		),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT		,	shaderStorageImageWriteWithoutFormat	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING	,	shaderUniformBufferArrayDynamicIndexing	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING	,	shaderSampledImageArrayDynamicIndexing	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING	,	shaderStorageBufferArrayDynamicIndexing	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING	,	shaderStorageImageArrayDynamicIndexing	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_CLIP_DISTANCE							,	shaderClipDistance						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_CULL_DISTANCE							,	shaderCullDistance						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_FLOAT64								,	shaderFloat64							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_INT64									,	shaderInt64								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_INT16									,	shaderInt16								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_RESOURCE_RESIDENCY						,	shaderResourceResidency					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_RESOURCE_MIN_LOD						,	shaderResourceMinLod					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_BINDING								,	sparseBinding							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER						,	sparseResidencyBuffer					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE2D						,	sparseResidencyImage2D					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE3D						,	sparseResidencyImage3D					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY2_SAMPLES						,	sparseResidency2Samples					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY4_SAMPLES						,	sparseResidency4Samples					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY8_SAMPLES						,	sparseResidency8Samples					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY16_SAMPLES					,	sparseResidency16Samples				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED						,	sparseResidencyAliased					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_VARIABLE_MULTISAMPLE_RATE						,	variableMultisampleRate					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_INHERITED_QUERIES								,	inheritedQueries						),
};

bool Context::requireDeviceCoreFeature (const DeviceCoreFeature requiredFeature)
{
	const vk::VkPhysicalDeviceFeatures& featuresAvailable		= getDeviceFeatures();
	const vk::VkBool32*					featuresAvailableArray	= (vk::VkBool32*)(&featuresAvailable);
	const deUint32						requiredFeatureIndex	= static_cast<deUint32>(requiredFeature);

	DE_ASSERT(requiredFeatureIndex * sizeof(vk::VkBool32) < sizeof(featuresAvailable));
	DE_ASSERT(deviceCoreFeaturesTable[requiredFeatureIndex].featureArrayIndex * sizeof(vk::VkBool32) == deviceCoreFeaturesTable[requiredFeatureIndex].featureArrayOffset);

	if (featuresAvailableArray[requiredFeatureIndex] == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requested core feature is not supported: " + std::string(deviceCoreFeaturesTable[requiredFeatureIndex].featureName));

	return true;
}

static bool isExtendedStorageFormat (VkFormat format)
{
	switch(format)
	{
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R8_UINT:
			return true;
		default:
			return false;
	}
}

static bool isDepthFormat (VkFormat format)
{
	switch(format)
	{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default:
			return false;
	}
}

vk::VkFormatProperties3 Context::getRequiredFormatProperties(const vk::VkFormat& format) const
{
	vk::VkFormatProperties3 p;
	p.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
	p.pNext = DE_NULL;

	vk::VkFormatProperties properties;
	getInstanceInterface().getPhysicalDeviceFormatProperties(getPhysicalDevice(), format, &properties);
	p.linearTilingFeatures	= properties.linearTilingFeatures;
	p.optimalTilingFeatures	= properties.optimalTilingFeatures;
	p.bufferFeatures		= properties.bufferFeatures;

	const vk::VkPhysicalDeviceFeatures& featuresAvailable = getDeviceFeatures();
	if (isExtendedStorageFormat(format) && featuresAvailable.shaderStorageImageReadWithoutFormat)
	{
		if (p.linearTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR)
			p.linearTilingFeatures	|= VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT_KHR;
		if (p.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR)
			p.optimalTilingFeatures	|= VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT_KHR;
	}
	if (isExtendedStorageFormat(format) && featuresAvailable.shaderStorageImageWriteWithoutFormat)
	{
		if (p.linearTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR)
			p.linearTilingFeatures	|= VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR;
		if (p.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR)
			p.optimalTilingFeatures	|= VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR;
	}
	if (isDepthFormat(format) && (p.linearTilingFeatures & (VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT_KHR)))
		p.linearTilingFeatures |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_DEPTH_COMPARISON_BIT_KHR;
	if (isDepthFormat(format) && (p.optimalTilingFeatures & (VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT_KHR)))
		p.optimalTilingFeatures |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_DEPTH_COMPARISON_BIT_KHR;

	return p;
}

vk::VkFormatProperties3 Context::getFormatProperties(const vk::VkFormat& format) const
{
	if (isDeviceFunctionalitySupported("VK_KHR_format_feature_flags2"))
	{
		vk::VkFormatProperties3 p;
		p.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
		p.pNext = DE_NULL;

		vk::VkFormatProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
		properties.pNext = &p;

		getInstanceInterface().getPhysicalDeviceFormatProperties2(getPhysicalDevice(), format, &properties);
		return p;
	}
	else
		return Context::getRequiredFormatProperties(format);
}

void* Context::getInstanceProcAddr	()
{
	return (void*)m_platformInterface.getGetInstanceProcAddr();
}

bool Context::isBufferDeviceAddressSupported(void) const
{
	return isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") ||
		   isDeviceFunctionalitySupported("VK_EXT_buffer_device_address");
}

bool Context::hasDebugReportRecorder () const
{
	return m_device->hasDebugReportRecorder();
}

vk::DebugReportRecorder& Context::getDebugReportRecorder () const
{
	return m_device->getDebugReportRecorder();
}

// TestCase

void TestCase::initPrograms (SourceCollections&) const
{
}

void TestCase::checkSupport (Context&) const
{
}

void TestCase::delayedInit (void)
{
}

void collectAndReportDebugMessages(vk::DebugReportRecorder &debugReportRecorder, Context& context)
{
	using DebugMessages = vk::DebugReportRecorder::MessageList;

	const DebugMessages&	messages	= debugReportRecorder.getMessages();
	tcu::TestLog&			log			= context.getTestContext().getLog();

	if (messages.size() > 0)
	{
		const tcu::ScopedLogSection	section		(log, "DebugMessages", "Debug Messages");
		int							numErrors	= 0;

		for (const auto& msg : messages)
		{
			if (msg.shouldBeLogged())
				log << tcu::TestLog::Message << msg << tcu::TestLog::EndMessage;

			if (msg.isError())
				numErrors += 1;
		}

		debugReportRecorder.clearMessages();

		if (numErrors > 0)
		{
			string errorMsg = de::toString(numErrors) + " API usage errors found";
			context.resultSetOnValidation(true);
			context.getTestContext().setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, errorMsg.c_str());
		}
	}
}

} // vkt
