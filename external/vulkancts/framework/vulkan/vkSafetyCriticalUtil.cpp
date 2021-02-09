/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Vulkan SC utilities
 *//*--------------------------------------------------------------------*/

#include "vkSafetyCriticalUtil.hpp"

#ifdef CTS_USES_VULKANSC

namespace vk
{

VkDeviceObjectReservationCreateInfo resetDeviceObjectReservationCreateInfo ()
{
	VkDeviceObjectReservationCreateInfo result =
	{
		VK_STRUCTURE_TYPE_DEVICE_OBJECT_RESERVATION_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,													// const void*						pNext;
		0u,															// deUint32							pipelineCacheCreateInfoCount;
		DE_NULL,													// const VkPipelineCacheCreateInfo*	pPipelineCacheCreateInfos;
		0u,															// deUint32							pipelinePoolSizeCount;
		DE_NULL,													// const VkPipelinePoolSize*		pPipelinePoolSizes;
		0u,															// deUint32							semaphoreRequestCount;
		0u,															// deUint32							commandBufferRequestCount;
		0u,															// deUint32							fenceRequestCount;
		0u,															// deUint32							deviceMemoryRequestCount;
		0u,															// deUint32							bufferRequestCount;
		0u,															// deUint32							imageRequestCount;
		0u,															// deUint32							eventRequestCount;
		0u,															// deUint32							queryPoolRequestCount;
		0u,															// deUint32							bufferViewRequestCount;
		0u,															// deUint32							imageViewRequestCount;
		0u,															// deUint32							layeredImageViewRequestCount;
		0u,															// deUint32							pipelineCacheRequestCount;
		0u,															// deUint32							pipelineLayoutRequestCount;
		0u,															// deUint32							renderPassRequestCount;
		0u,															// deUint32							graphicsPipelineRequestCount;
		0u,															// deUint32							computePipelineRequestCount;
		0u,															// deUint32							descriptorSetLayoutRequestCount;
		0u,															// deUint32							samplerRequestCount;
		0u,															// deUint32							descriptorPoolRequestCount;
		0u,															// deUint32							descriptorSetRequestCount;
		0u,															// deUint32							framebufferRequestCount;
		0u,															// deUint32							commandPoolRequestCount;
		0u,															// deUint32							samplerYcbcrConversionRequestCount;
		0u,															// deUint32							surfaceRequestCount;
		0u,															// deUint32							swapchainRequestCount;
		0u,															// deUint32							displayModeRequestCount;
		0u,															// deUint32							subpassDescriptionRequestCount;
		0u,															// deUint32							attachmentDescriptionRequestCount;
		0u,															// deUint32							descriptorSetLayoutBindingRequestCount;
		0u,															// deUint32							descriptorSetLayoutBindingLimit;
		0u,															// deUint32							maxImageViewMipLevels;
		0u,															// deUint32							maxImageViewArrayLayers;
		0u,															// deUint32							maxLayeredImageViewMipLevels;
		0u,															// deUint32							maxOcclusionQueriesPerPool;
		0u,															// deUint32							maxPipelineStatisticsQueriesPerPool;
		0u,															// deUint32							maxTimestampQueriesPerPool;
	};
	return result;
}

} // vk

#else
	DE_EMPTY_CPP_FILE
#endif // CTS_USES_VULKANSC

