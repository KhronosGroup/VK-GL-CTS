/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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
 *//*--------------------------------------------------------------------*/

#include "vktAmberHelper.hpp"

#include "amber/amber_vulkan.h"

namespace vkt
{
namespace cts_amber
{

amber::EngineConfig* GetVulkanConfig	(void*			instance,
										 void*			physicalDevice,
										 void*			device,
										 const void*	features,
										 const void*	features2,
										 const			std::vector<std::string>& instance_extensions,
										 const			std::vector<std::string>& device_extensions,
										 deUint32		queueIdx,
										 void*			queue,
										 void*			getInstanceProcAddr)
{
	amber::VulkanEngineConfig *cfg = new amber::VulkanEngineConfig();
	cfg->vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(getInstanceProcAddr);
	cfg->instance = static_cast<VkInstance>(instance);
	cfg->physical_device = static_cast<VkPhysicalDevice>(physicalDevice);
	cfg->available_features = *static_cast<const VkPhysicalDeviceFeatures*>(features);
	cfg->available_features2 = *static_cast<const VkPhysicalDeviceFeatures2KHR*>(features2);
	cfg->available_instance_extensions = instance_extensions;
	cfg->available_device_extensions = device_extensions;
	cfg->queue_family_index = queueIdx;
	cfg->device = static_cast<VkDevice>(device);
	cfg->queue = static_cast<VkQueue>(queue);
	return cfg;
}

} // cts_amber
} // vkt
