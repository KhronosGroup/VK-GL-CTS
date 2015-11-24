/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \file
 * \brief Null (dummy) Vulkan implementation.
 *//*--------------------------------------------------------------------*/

#include "vkNullDriver.hpp"
#include "vkPlatform.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deMemory.h"

#include <stdexcept>

namespace vk
{

namespace
{

#define VK_NULL_RETURN(STMT)					\
	do {										\
		try {									\
			STMT;								\
			return VK_SUCCESS;					\
		} catch (const std::bad_alloc&) {		\
			return VK_ERROR_OUT_OF_HOST_MEMORY;	\
		} catch (VkResult res) {				\
			return res;							\
		}										\
	} while (deGetFalse())

// \todo [2015-07-14 pyry] Check FUNC type by checkedCastToPtr<T>() or similar
#define VK_NULL_FUNC_ENTRY(NAME, FUNC)	{ #NAME, (deFunctionPtr)FUNC }

#define VK_NULL_DEFINE_DEVICE_OBJ(NAME)				\
struct NAME											\
{													\
	NAME (VkDevice, const Vk##NAME##CreateInfo*) {}	\
}

class Instance
{
public:
										Instance		(const VkInstanceCreateInfo* instanceInfo);
										~Instance		(void) {}

	PFN_vkVoidFunction					getProcAddr		(const char* name) const { return (PFN_vkVoidFunction)m_functions.getFunction(name); }

private:
	const tcu::StaticFunctionLibrary	m_functions;
};

class Device
{
public:
										Device			(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* deviceInfo);
										~Device			(void) {}

	PFN_vkVoidFunction					getProcAddr		(const char* name) const { return (PFN_vkVoidFunction)m_functions.getFunction(name); }

private:
	const tcu::StaticFunctionLibrary	m_functions;
};

class DescriptorSet
{
public:
	DescriptorSet (VkDevice, VkDescriptorPool, VkDescriptorSetUsage, VkDescriptorSetLayout) {}
};

class Pipeline
{
public:
	Pipeline (VkDevice, const VkGraphicsPipelineCreateInfo*) {}
	Pipeline (VkDevice, const VkComputePipelineCreateInfo*) {}
};

class DeviceMemory
{
public:
						DeviceMemory	(VkDevice, const VkMemoryAllocInfo* pAllocInfo)
							: m_memory(deMalloc((size_t)pAllocInfo->allocationSize))
						{
							if (!m_memory)
								throw std::bad_alloc();
						}
						~DeviceMemory	(void)
						{
							deFree(m_memory);
						}

	void*				getPtr			(void) const { return m_memory; }

private:
	void* const			m_memory;
};

class Buffer
{
public:
						Buffer		(VkDevice, const VkBufferCreateInfo* pCreateInfo)
							: m_size(pCreateInfo->size)
						{}

	VkDeviceSize		getSize		(void) const { return m_size;	}

private:
	const VkDeviceSize	m_size;
};

VK_NULL_DEFINE_DEVICE_OBJ(CmdBuffer);
VK_NULL_DEFINE_DEVICE_OBJ(Fence);
VK_NULL_DEFINE_DEVICE_OBJ(Image);
VK_NULL_DEFINE_DEVICE_OBJ(Semaphore);
VK_NULL_DEFINE_DEVICE_OBJ(Event);
VK_NULL_DEFINE_DEVICE_OBJ(QueryPool);
VK_NULL_DEFINE_DEVICE_OBJ(BufferView);
VK_NULL_DEFINE_DEVICE_OBJ(ImageView);
VK_NULL_DEFINE_DEVICE_OBJ(ShaderModule);
VK_NULL_DEFINE_DEVICE_OBJ(Shader);
VK_NULL_DEFINE_DEVICE_OBJ(PipelineCache);
VK_NULL_DEFINE_DEVICE_OBJ(PipelineLayout);
VK_NULL_DEFINE_DEVICE_OBJ(RenderPass);
VK_NULL_DEFINE_DEVICE_OBJ(DescriptorSetLayout);
VK_NULL_DEFINE_DEVICE_OBJ(Sampler);
VK_NULL_DEFINE_DEVICE_OBJ(Framebuffer);
VK_NULL_DEFINE_DEVICE_OBJ(CmdPool);
VK_NULL_DEFINE_DEVICE_OBJ(DescriptorPool);

extern "C"
{

PFN_vkVoidFunction getInstanceProcAddr (VkInstance instance, const char* pName)
{
	return reinterpret_cast<Instance*>(instance)->getProcAddr(pName);
}

PFN_vkVoidFunction getDeviceProcAddr (VkDevice device, const char* pName)
{
	return reinterpret_cast<Device*>(device)->getProcAddr(pName);
}

VkResult createGraphicsPipelines (VkDevice device, VkPipelineCache, deUint32 count, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines)
{
	for (deUint32 ndx = 0; ndx < count; ndx++)
		pPipelines[ndx] = VkPipeline((deUint64)(deUintptr)new Pipeline(device, pCreateInfos+ndx));
	return VK_SUCCESS;
}

VkResult createComputePipelines (VkDevice device, VkPipelineCache, deUint32 count, const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines)
{
	for (deUint32 ndx = 0; ndx < count; ndx++)
		pPipelines[ndx] = VkPipeline((deUint64)(deUintptr)new Pipeline(device, pCreateInfos+ndx));
	return VK_SUCCESS;
}

VkResult enumeratePhysicalDevices (VkInstance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pDevices)
{
	if (pDevices && *pPhysicalDeviceCount >= 1u)
		*pDevices = reinterpret_cast<VkPhysicalDevice>((void*)(deUintptr)1u);

	*pPhysicalDeviceCount = 1;

	return VK_SUCCESS;
}

VkResult getPhysicalDeviceProperties (VkPhysicalDevice, VkPhysicalDeviceProperties* props)
{
	deMemset(props, 0, sizeof(VkPhysicalDeviceProperties));

	props->apiVersion		= VK_API_VERSION;
	props->driverVersion	= 1u;
	props->deviceType		= VK_PHYSICAL_DEVICE_TYPE_OTHER;

	deMemcpy(props->deviceName, "null", 5);

	// \todo [2015-09-25 pyry] Fill in reasonable limits

	return VK_SUCCESS;
}

VkResult getPhysicalDeviceQueueFamilyProperties (VkPhysicalDevice, deUint32* count, VkQueueFamilyProperties* props)
{
	if (props && *count >= 1u)
	{
		deMemset(props, 0, sizeof(VkQueueFamilyProperties));

		props->queueCount			= 1u;
		props->queueFlags			= VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT|VK_QUEUE_DMA_BIT;
		props->supportsTimestamps	= DE_TRUE;
	}

	*count = 1u;

	return VK_SUCCESS;
}

VkResult getPhysicalDeviceMemoryProperties (VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* props)
{
	deMemset(props, 0, sizeof(VkPhysicalDeviceMemoryProperties));

	props->memoryTypeCount				= 1u;
	props->memoryTypes[0].heapIndex		= 0u;
	props->memoryTypes[0].propertyFlags	= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	props->memoryHeapCount				= 1u;
	props->memoryHeaps[0].size			= 1ull << 31;
	props->memoryHeaps[0].flags			= VK_MEMORY_HEAP_HOST_LOCAL_BIT;

	return VK_SUCCESS;
}

VkResult getPhysicalDeviceFormatProperties (VkPhysicalDevice, VkFormat, VkFormatProperties* pFormatProperties)
{
	const VkFormatFeatureFlags	allFeatures	= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
											| VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
											| VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT
											| VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT
											| VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT
											| VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT
											| VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT
											| VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
											| VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
											| VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
											| VK_FORMAT_FEATURE_BLIT_SOURCE_BIT
											| VK_FORMAT_FEATURE_BLIT_DESTINATION_BIT;

	pFormatProperties->linearTilingFeatures		= allFeatures;
	pFormatProperties->optimalTilingFeatures	= allFeatures;
	pFormatProperties->bufferFeatures			= allFeatures;

	return VK_SUCCESS;
}

VkResult getBufferMemoryRequirements (VkDevice, VkBuffer bufferHandle, VkMemoryRequirements* requirements)
{
	const Buffer*	buffer	= reinterpret_cast<Buffer*>(bufferHandle.getInternal());

	requirements->memoryTypeBits	= 1u;
	requirements->size				= buffer->getSize();
	requirements->alignment			= (VkDeviceSize)1u;

	return VK_SUCCESS;
}

VkResult getImageMemoryRequirements (VkDevice, VkImage, VkMemoryRequirements* requirements)
{
	requirements->memoryTypeBits	= 1u;
	requirements->size				= 4u;
	requirements->alignment			= 4u;

	return VK_SUCCESS;
}

VkResult mapMemory (VkDevice, VkDeviceMemory memHandle, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)
{
	const DeviceMemory*	memory	= reinterpret_cast<DeviceMemory*>(memHandle.getInternal());

	DE_UNREF(size);
	DE_UNREF(flags);

	*ppData = (deUint8*)memory->getPtr() + offset;

	return VK_SUCCESS;
}

VkResult allocDescriptorSets (VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, deUint32 count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets)
{
	for (deUint32 ndx = 0; ndx < count; ++ndx)
	{
		try
		{
			pDescriptorSets[ndx] = VkDescriptorSet((deUint64)(deUintptr)new DescriptorSet(device, descriptorPool, setUsage, pSetLayouts[ndx]));
		}
		catch (const std::bad_alloc&)
		{
			for (deUint32 freeNdx = 0; freeNdx < ndx; freeNdx++)
				delete reinterpret_cast<DescriptorSet*>((deUintptr)pDescriptorSets[freeNdx].getInternal());

			return VK_ERROR_OUT_OF_HOST_MEMORY;
		}
		catch (VkResult res)
		{
			for (deUint32 freeNdx = 0; freeNdx < ndx; freeNdx++)
				delete reinterpret_cast<DescriptorSet*>((deUintptr)pDescriptorSets[freeNdx].getInternal());

			return res;
		}
	}

	return VK_SUCCESS;
}

void freeDescriptorSets (VkDevice, VkDescriptorPool, deUint32 count, const VkDescriptorSet* pDescriptorSets)
{
	for (deUint32 ndx = 0; ndx < count; ++ndx)
	{
		// \note: delete cannot fail
		delete reinterpret_cast<DescriptorSet*>((deUintptr)pDescriptorSets[ndx].getInternal());
	}
}

#include "vkNullDriverImpl.inl"

} // extern "C"

Instance::Instance (const VkInstanceCreateInfo*)
	: m_functions(s_instanceFunctions, DE_LENGTH_OF_ARRAY(s_instanceFunctions))
{
}

Device::Device (VkPhysicalDevice, const VkDeviceCreateInfo*)
	: m_functions(s_deviceFunctions, DE_LENGTH_OF_ARRAY(s_deviceFunctions))
{
}

class NullDriverLibrary : public Library
{
public:
								NullDriverLibrary (void)
									: m_library	(s_platformFunctions, DE_LENGTH_OF_ARRAY(s_platformFunctions))
									, m_driver	(m_library)
								{}

	const PlatformInterface&	getPlatformInterface	(void) const { return m_driver;	}

private:
	const tcu::StaticFunctionLibrary	m_library;
	const PlatformDriver				m_driver;
};

} // anonymous

Library* createNullDriver (void)
{
	return new NullDriverLibrary();
}

} // vk
