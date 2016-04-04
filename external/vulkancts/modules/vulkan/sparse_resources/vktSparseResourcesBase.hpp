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

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkRef.hpp"
#include "vkTypeUtil.hpp"

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

	typedef			std::map<vk::VkQueueFlags, std::vector<Queue> >								QueuesMap;
	typedef			std::vector<vk::VkQueueFamilyProperties>										QueueFamilyPropertiesVec;
	typedef			vk::Move<vk::VkDevice>															DevicePtr;

	bool			createDeviceSupportingQueues	(const QueueRequirementsVec&					queueRequirements);

	const Queue&	getQueue						(const vk::VkQueueFlags							queueFlags,
													 const deUint32									queueIndex);

	deUint32		findMatchingMemoryType			(const vk::VkPhysicalDeviceMemoryProperties&	deviceMemoryProperties,
													 const vk::VkMemoryRequirements&				objectMemoryRequirements,
													 const vk::MemoryRequirement&					memoryRequirement) const;
	DevicePtr		m_logicalDevice;

private:

	deUint32		findMatchingQueueFamilyIndex	(const QueueFamilyPropertiesVec&				queueFamilyProperties,
													 const vk::VkQueueFlags							queueFlags) const;
	QueuesMap		m_queues;
};

} // sparse
} // vkt

#endif // _VKTSPARSERESOURCESBASE_HPP
