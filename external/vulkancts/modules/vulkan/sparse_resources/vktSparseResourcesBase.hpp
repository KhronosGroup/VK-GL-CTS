#ifndef _VKTSPARSERESOURCESBASE_HPP
#define _VKTSPARSERESOURCESBASE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesBase.hpp	
 * \brief Sparse Resources Base Instance 
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSparseResourcesTestsUtil.hpp"

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkRef.hpp"
#include "vkTypeUtil.hpp"

#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <map>
#include <vector>

namespace vkt
{
namespace sparse
{

enum
{
	NO_MATCH_FOUND = ~((deUint32)0)
};

struct Queue
{
	vk::VkQueue	queueHandle;
	deUint32	queueFamilyIndex;
	deUint32	queueIndex;
};

struct QueueRequirements
{
	QueueRequirements(const vk::VkQueueFlags qFlags, const deUint32	qCount)
		: queueFlags(qFlags)
		, queueCount(qCount)
	{}

	vk::VkQueueFlags	queueFlags;
	deUint32			queueCount;
};

typedef std::vector<QueueRequirements> QueueRequirementsVec;

class SparseResourcesBaseInstance : public TestInstance
{
public:
					SparseResourcesBaseInstance		(Context &context);

protected:

	typedef			std::map<vk::VkQueueFlags, std::vector<Queue> >												QueuesMap;
	typedef			std::vector<vk::VkQueueFamilyProperties>													QueueFamilyPropertiesVec;
	typedef			vk::Move<vk::VkDevice>																		DevicePtr;
	typedef			de::SharedPtr< vk::Unique<vk::VkDeviceMemory> >												DeviceMemoryUniquePtr;

	void			createDeviceSupportingQueues		(const QueueRequirementsVec&							queueRequirements);

	const Queue&	getQueue							(const vk::VkQueueFlags									queueFlags,
														 const deUint32											queueIndex);

	deUint32		findMatchingMemoryType				(const vk::InstanceInterface&							instance,
														 const vk::VkPhysicalDevice								physicalDevice,
														 const vk::VkMemoryRequirements&						objectMemoryRequirements,
														 const vk::MemoryRequirement&							memoryRequirement) const;

	bool			checkSparseSupportForImageType		(const vk::InstanceInterface&							instance,
														 const vk::VkPhysicalDevice								physicalDevice,
														 const ImageType										imageType) const;

	bool			checkSparseSupportForImageFormat	(const vk::InstanceInterface&							instance,
														 const vk::VkPhysicalDevice								physicalDevice,
														 const vk::VkImageCreateInfo&							imageInfo) const;

	bool			checkImageFormatFeatureSupport		(const vk::InstanceInterface&							instance,
														 const vk::VkPhysicalDevice								physicalDevice,
														 const vk::VkFormat										format,
														 const vk::VkFormatFeatureFlags							featureFlags) const;

	deUint32		getSparseAspectRequirementsIndex	(const std::vector<vk::VkSparseImageMemoryRequirements>&requirements,
														 const vk::VkImageAspectFlags							aspectFlags) const;

	DevicePtr		m_logicalDevice;

private:

	deUint32		findMatchingQueueFamilyIndex		(const QueueFamilyPropertiesVec&						queueFamilyProperties,
														 const vk::VkQueueFlags									queueFlags,
														 const deUint32											startIndex) const;
	QueuesMap		m_queues;
};

} // sparse
} // vkt

#endif // _VKTSPARSERESOURCESBASE_HPP
