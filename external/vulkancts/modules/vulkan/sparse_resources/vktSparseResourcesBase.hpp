#ifndef _VKTSPARSERESOURCESBASE_HPP
#define _VKTSPARSERESOURCESBASE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
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
