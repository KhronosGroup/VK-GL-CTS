/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
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
 * \brief Null (dummy) Vulkan implementation.
 *//*--------------------------------------------------------------------*/

#include "vkNullDriver.hpp"
#include "vkPlatform.hpp"
#include "vkImageUtil.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deMemory.h"

#include <stdexcept>
#include <algorithm>

namespace vk
{

namespace
{

using std::vector;

// Memory management

template<typename T>
void* allocateSystemMem (const VkAllocationCallbacks* pAllocator, VkSystemAllocationScope scope)
{
	void* ptr = pAllocator->pfnAllocation(pAllocator->pUserData, sizeof(T), sizeof(void*), scope);
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}

void freeSystemMem (const VkAllocationCallbacks* pAllocator, void* mem)
{
	pAllocator->pfnFree(pAllocator->pUserData, mem);
}

template<typename Object, typename Handle, typename Parent, typename CreateInfo>
Handle allocateHandle (Parent parent, const CreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	Object* obj = DE_NULL;

	if (pAllocator)
	{
		void* mem = allocateSystemMem<Object>(pAllocator, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
		try
		{
			obj = new (mem) Object(parent, pCreateInfo);
			DE_ASSERT(obj == mem);
		}
		catch (...)
		{
			pAllocator->pfnFree(pAllocator->pUserData, mem);
			throw;
		}
	}
	else
		obj = new Object(parent, pCreateInfo);

	return reinterpret_cast<Handle>(obj);
}

template<typename Object, typename Handle, typename CreateInfo>
Handle allocateHandle (const CreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	Object* obj = DE_NULL;

	if (pAllocator)
	{
		void* mem = allocateSystemMem<Object>(pAllocator, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
		try
		{
			obj = new (mem) Object(pCreateInfo);
			DE_ASSERT(obj == mem);
		}
		catch (...)
		{
			pAllocator->pfnFree(pAllocator->pUserData, mem);
			throw;
		}
	}
	else
		obj = new Object(pCreateInfo);

	return reinterpret_cast<Handle>(obj);
}

template<typename Object, typename Handle>
void freeHandle (Handle handle, const VkAllocationCallbacks* pAllocator)
{
	Object* obj = reinterpret_cast<Object*>(handle);

	if (pAllocator)
	{
		obj->~Object();
		freeSystemMem(pAllocator, reinterpret_cast<void*>(obj));
	}
	else
		delete obj;
}

template<typename Object, typename Handle, typename Parent, typename CreateInfo>
Handle allocateNonDispHandle (Parent parent, const CreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	Object* const	obj		= allocateHandle<Object, Object*>(parent, pCreateInfo, pAllocator);
	return Handle((deUint64)(deUintptr)obj);
}

template<typename Object, typename Handle>
void freeNonDispHandle (Handle handle, const VkAllocationCallbacks* pAllocator)
{
	freeHandle<Object>(reinterpret_cast<Object*>((deUintptr)handle.getInternal()), pAllocator);
}

// Object definitions

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

VK_NULL_DEFINE_DEVICE_OBJ(Fence);
VK_NULL_DEFINE_DEVICE_OBJ(Semaphore);
VK_NULL_DEFINE_DEVICE_OBJ(Event);
VK_NULL_DEFINE_DEVICE_OBJ(QueryPool);
VK_NULL_DEFINE_DEVICE_OBJ(BufferView);
VK_NULL_DEFINE_DEVICE_OBJ(ImageView);
VK_NULL_DEFINE_DEVICE_OBJ(ShaderModule);
VK_NULL_DEFINE_DEVICE_OBJ(PipelineCache);
VK_NULL_DEFINE_DEVICE_OBJ(PipelineLayout);
VK_NULL_DEFINE_DEVICE_OBJ(RenderPass);
VK_NULL_DEFINE_DEVICE_OBJ(DescriptorSetLayout);
VK_NULL_DEFINE_DEVICE_OBJ(Sampler);
VK_NULL_DEFINE_DEVICE_OBJ(Framebuffer);
VK_NULL_DEFINE_DEVICE_OBJ(CommandPool);

class Instance
{
public:
										Instance		(const VkInstanceCreateInfo* instanceInfo);
										~Instance		(void) {}

	PFN_vkVoidFunction					getProcAddr		(const char* name) const { return (PFN_vkVoidFunction)m_functions.getFunction(name); }

private:
	const tcu::StaticFunctionLibrary	m_functions;
};

class SurfaceKHR
{
public:
										SurfaceKHR		(VkInstance, const VkXlibSurfaceCreateInfoKHR*)		{}
										SurfaceKHR		(VkInstance, const VkXcbSurfaceCreateInfoKHR*)		{}
										SurfaceKHR		(VkInstance, const VkWaylandSurfaceCreateInfoKHR*)	{}
										SurfaceKHR		(VkInstance, const VkMirSurfaceCreateInfoKHR*)		{}
										SurfaceKHR		(VkInstance, const VkAndroidSurfaceCreateInfoKHR*)	{}
										SurfaceKHR		(VkInstance, const VkWin32SurfaceCreateInfoKHR*)	{}
										SurfaceKHR		(VkInstance, const VkDisplaySurfaceCreateInfoKHR*)	{}
										~SurfaceKHR		(void) {}
};

class DisplayModeKHR
{
public:
										DisplayModeKHR	(VkDisplayKHR, const VkDisplayModeCreateInfoKHR*) {}
										~DisplayModeKHR	(void) {}
};

class DebugReportCallbackEXT
{
public:
										DebugReportCallbackEXT	(VkInstance, const VkDebugReportCallbackCreateInfoEXT*) {}
										~DebugReportCallbackEXT	(void) {}
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

class Pipeline
{
public:
	Pipeline (VkDevice, const VkGraphicsPipelineCreateInfo*) {}
	Pipeline (VkDevice, const VkComputePipelineCreateInfo*) {}
};

class SwapchainKHR
{
public:
										SwapchainKHR	(VkDevice, const VkSwapchainCreateInfoKHR*) {}
										~SwapchainKHR	(void) {}
};

void* allocateHeap (const VkMemoryAllocateInfo* pAllocInfo)
{
	// \todo [2015-12-03 pyry] Alignment requirements?
	// \todo [2015-12-03 pyry] Empty allocations okay?
	if (pAllocInfo->allocationSize > 0)
	{
		void* const heapPtr = deMalloc((size_t)pAllocInfo->allocationSize);
		if (!heapPtr)
			throw std::bad_alloc();
		return heapPtr;
	}
	else
		return DE_NULL;
}

void freeHeap (void* ptr)
{
	deFree(ptr);
}

class DeviceMemory
{
public:
						DeviceMemory	(VkDevice, const VkMemoryAllocateInfo* pAllocInfo)
							: m_memory(allocateHeap(pAllocInfo))
						{
						}
						~DeviceMemory	(void)
						{
							freeHeap(m_memory);
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

class Image
{
public:
								Image			(VkDevice, const VkImageCreateInfo* pCreateInfo)
									: m_imageType	(pCreateInfo->imageType)
									, m_format		(pCreateInfo->format)
									, m_extent		(pCreateInfo->extent)
									, m_samples		(pCreateInfo->samples)
								{}

	VkImageType					getImageType	(void) const { return m_imageType;	}
	VkFormat					getFormat		(void) const { return m_format;		}
	VkExtent3D					getExtent		(void) const { return m_extent;		}
	VkSampleCountFlagBits		getSamples		(void) const { return m_samples;	}

private:
	const VkImageType			m_imageType;
	const VkFormat				m_format;
	const VkExtent3D			m_extent;
	const VkSampleCountFlagBits	m_samples;
};

class CommandBuffer
{
public:
						CommandBuffer(VkDevice, VkCommandPool, VkCommandBufferLevel)
						{}
};

class DescriptorSet
{
public:
	DescriptorSet (VkDevice, VkDescriptorPool, VkDescriptorSetLayout) {}
};

class DescriptorPool
{
public:
										DescriptorPool	(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo)
											: m_device	(device)
											, m_flags	(pCreateInfo->flags)
										{}
										~DescriptorPool	(void)
										{
											reset();
										}

	VkDescriptorSet						allocate		(VkDescriptorSetLayout setLayout);
	void								free			(VkDescriptorSet set);

	void								reset			(void);

private:
	const VkDevice						m_device;
	const VkDescriptorPoolCreateFlags	m_flags;

	vector<DescriptorSet*>				m_managedSets;
};

VkDescriptorSet DescriptorPool::allocate (VkDescriptorSetLayout setLayout)
{
	DescriptorSet* const	impl	= new DescriptorSet(m_device, VkDescriptorPool(reinterpret_cast<deUintptr>(this)), setLayout);

	try
	{
		m_managedSets.push_back(impl);
	}
	catch (...)
	{
		delete impl;
		throw;
	}

	return VkDescriptorSet(reinterpret_cast<deUintptr>(impl));
}

void DescriptorPool::free (VkDescriptorSet set)
{
	DescriptorSet* const	impl	= reinterpret_cast<DescriptorSet*>((deUintptr)set.getInternal());

	DE_ASSERT(m_flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

	delete impl;

	for (size_t ndx = 0; ndx < m_managedSets.size(); ++ndx)
	{
		if (m_managedSets[ndx] == impl)
		{
			std::swap(m_managedSets[ndx], m_managedSets.back());
			m_managedSets.pop_back();
			return;
		}
	}

	DE_FATAL("VkDescriptorSet not owned by VkDescriptorPool");
}

void DescriptorPool::reset (void)
{
	for (size_t ndx = 0; ndx < m_managedSets.size(); ++ndx)
		delete m_managedSets[ndx];
	m_managedSets.clear();
}

// API implementation

extern "C"
{

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL getInstanceProcAddr (VkInstance instance, const char* pName)
{
	return reinterpret_cast<Instance*>(instance)->getProcAddr(pName);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL getDeviceProcAddr (VkDevice device, const char* pName)
{
	return reinterpret_cast<Device*>(device)->getProcAddr(pName);
}

VKAPI_ATTR VkResult VKAPI_CALL createGraphicsPipelines (VkDevice device, VkPipelineCache, deUint32 count, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
{
	deUint32 allocNdx;
	try
	{
		for (allocNdx = 0; allocNdx < count; allocNdx++)
			pPipelines[allocNdx] = allocateNonDispHandle<Pipeline, VkPipeline>(device, pCreateInfos+allocNdx, pAllocator);

		return VK_SUCCESS;
	}
	catch (const std::bad_alloc&)
	{
		for (deUint32 freeNdx = 0; freeNdx < allocNdx; freeNdx++)
			freeNonDispHandle<Pipeline, VkPipeline>(pPipelines[freeNdx], pAllocator);

		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}
	catch (VkResult err)
	{
		for (deUint32 freeNdx = 0; freeNdx < allocNdx; freeNdx++)
			freeNonDispHandle<Pipeline, VkPipeline>(pPipelines[freeNdx], pAllocator);

		return err;
	}
}

VKAPI_ATTR VkResult VKAPI_CALL createComputePipelines (VkDevice device, VkPipelineCache, deUint32 count, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
{
	deUint32 allocNdx;
	try
	{
		for (allocNdx = 0; allocNdx < count; allocNdx++)
			pPipelines[allocNdx] = allocateNonDispHandle<Pipeline, VkPipeline>(device, pCreateInfos+allocNdx, pAllocator);

		return VK_SUCCESS;
	}
	catch (const std::bad_alloc&)
	{
		for (deUint32 freeNdx = 0; freeNdx < allocNdx; freeNdx++)
			freeNonDispHandle<Pipeline, VkPipeline>(pPipelines[freeNdx], pAllocator);

		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}
	catch (VkResult err)
	{
		for (deUint32 freeNdx = 0; freeNdx < allocNdx; freeNdx++)
			freeNonDispHandle<Pipeline, VkPipeline>(pPipelines[freeNdx], pAllocator);

		return err;
	}
}

VKAPI_ATTR VkResult VKAPI_CALL enumeratePhysicalDevices (VkInstance, deUint32* pPhysicalDeviceCount, VkPhysicalDevice* pDevices)
{
	if (pDevices && *pPhysicalDeviceCount >= 1u)
		*pDevices = reinterpret_cast<VkPhysicalDevice>((void*)(deUintptr)1u);

	*pPhysicalDeviceCount = 1;

	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceProperties (VkPhysicalDevice, VkPhysicalDeviceProperties* props)
{
	deMemset(props, 0, sizeof(VkPhysicalDeviceProperties));

	props->apiVersion		= VK_API_VERSION;
	props->driverVersion	= 1u;
	props->deviceType		= VK_PHYSICAL_DEVICE_TYPE_OTHER;

	deMemcpy(props->deviceName, "null", 5);

	// \todo [2015-09-25 pyry] Fill in reasonable limits
	props->limits.maxTexelBufferElements	= 8096;
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceQueueFamilyProperties (VkPhysicalDevice, deUint32* count, VkQueueFamilyProperties* props)
{
	if (props && *count >= 1u)
	{
		deMemset(props, 0, sizeof(VkQueueFamilyProperties));

		props->queueCount			= 1u;
		props->queueFlags			= VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT;
		props->timestampValidBits	= 64;
	}

	*count = 1u;
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceMemoryProperties (VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* props)
{
	deMemset(props, 0, sizeof(VkPhysicalDeviceMemoryProperties));

	props->memoryTypeCount				= 1u;
	props->memoryTypes[0].heapIndex		= 0u;
	props->memoryTypes[0].propertyFlags	= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	props->memoryHeapCount				= 1u;
	props->memoryHeaps[0].size			= 1ull << 31;
	props->memoryHeaps[0].flags			= 0u;
}

VKAPI_ATTR void VKAPI_CALL getPhysicalDeviceFormatProperties (VkPhysicalDevice, VkFormat, VkFormatProperties* pFormatProperties)
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
											| VK_FORMAT_FEATURE_BLIT_SRC_BIT
											| VK_FORMAT_FEATURE_BLIT_DST_BIT;

	pFormatProperties->linearTilingFeatures		= allFeatures;
	pFormatProperties->optimalTilingFeatures	= allFeatures;
	pFormatProperties->bufferFeatures			= allFeatures;
}

VKAPI_ATTR void VKAPI_CALL getBufferMemoryRequirements (VkDevice, VkBuffer bufferHandle, VkMemoryRequirements* requirements)
{
	const Buffer*	buffer	= reinterpret_cast<const Buffer*>(bufferHandle.getInternal());

	requirements->memoryTypeBits	= 1u;
	requirements->size				= buffer->getSize();
	requirements->alignment			= (VkDeviceSize)1u;
}

VkDeviceSize getPackedImageDataSize (VkFormat format, VkExtent3D extent, VkSampleCountFlagBits samples)
{
	return (VkDeviceSize)getPixelSize(mapVkFormat(format))
			* (VkDeviceSize)extent.width
			* (VkDeviceSize)extent.height
			* (VkDeviceSize)extent.depth
			* (VkDeviceSize)samples;
}

VkDeviceSize getCompressedImageDataSize (VkFormat format, VkExtent3D extent)
{
	try
	{
		const tcu::CompressedTexFormat	tcuFormat		= mapVkCompressedFormat(format);
		const size_t					blockSize		= tcu::getBlockSize(tcuFormat);
		const tcu::IVec3				blockPixelSize	= tcu::getBlockPixelSize(tcuFormat);
		const int						numBlocksX		= deDivRoundUp32((int)extent.width, blockPixelSize.x());
		const int						numBlocksY		= deDivRoundUp32((int)extent.height, blockPixelSize.y());
		const int						numBlocksZ		= deDivRoundUp32((int)extent.depth, blockPixelSize.z());

		return blockSize*numBlocksX*numBlocksY*numBlocksZ;
	}
	catch (...)
	{
		return 0; // Unsupported compressed format
	}
}

VKAPI_ATTR void VKAPI_CALL getImageMemoryRequirements (VkDevice, VkImage imageHandle, VkMemoryRequirements* requirements)
{
	const Image*	image	= reinterpret_cast<const Image*>(imageHandle.getInternal());

	requirements->memoryTypeBits	= 1u;
	requirements->alignment			= 16u;

	if (isCompressedFormat(image->getFormat()))
		requirements->size = getCompressedImageDataSize(image->getFormat(), image->getExtent());
	else
		requirements->size = getPackedImageDataSize(image->getFormat(), image->getExtent(), image->getSamples());
}

VKAPI_ATTR VkResult VKAPI_CALL mapMemory (VkDevice, VkDeviceMemory memHandle, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)
{
	const DeviceMemory*	memory	= reinterpret_cast<DeviceMemory*>(memHandle.getInternal());

	DE_UNREF(size);
	DE_UNREF(flags);

	*ppData = (deUint8*)memory->getPtr() + offset;

	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL allocateDescriptorSets (VkDevice, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets)
{
	DescriptorPool* const	poolImpl	= reinterpret_cast<DescriptorPool*>((deUintptr)pAllocateInfo->descriptorPool.getInternal());

	for (deUint32 ndx = 0; ndx < pAllocateInfo->descriptorSetCount; ++ndx)
	{
		try
		{
			pDescriptorSets[ndx] = poolImpl->allocate(pAllocateInfo->pSetLayouts[ndx]);
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

VKAPI_ATTR void VKAPI_CALL freeDescriptorSets (VkDevice, VkDescriptorPool descriptorPool, deUint32 count, const VkDescriptorSet* pDescriptorSets)
{
	DescriptorPool* const	poolImpl	= reinterpret_cast<DescriptorPool*>((deUintptr)descriptorPool.getInternal());

	for (deUint32 ndx = 0; ndx < count; ++ndx)
		poolImpl->free(pDescriptorSets[ndx]);
}

VKAPI_ATTR VkResult VKAPI_CALL resetDescriptorPool (VkDevice, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags)
{
	DescriptorPool* const	poolImpl	= reinterpret_cast<DescriptorPool*>((deUintptr)descriptorPool.getInternal());

	poolImpl->reset();

	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL allocateCommandBuffers (VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers)
{
	if (pAllocateInfo && pCommandBuffers)
	{
		for (deUint32 ndx = 0; ndx < pAllocateInfo->commandBufferCount; ++ndx)
		{
			pCommandBuffers[ndx] = reinterpret_cast<VkCommandBuffer>(new CommandBuffer(device, pAllocateInfo->commandPool, pAllocateInfo->level));
		}
	}

	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL freeCommandBuffers (VkDevice device, VkCommandPool commandPool, deUint32 commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
	DE_UNREF(device);
	DE_UNREF(commandPool);

	for (deUint32 ndx = 0; ndx < commandBufferCount; ++ndx)
		delete reinterpret_cast<CommandBuffer*>(pCommandBuffers[ndx]);
}


VKAPI_ATTR VkResult VKAPI_CALL createDisplayModeKHR (VkPhysicalDevice, VkDisplayKHR display, const VkDisplayModeCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDisplayModeKHR* pMode)
{
	DE_UNREF(pAllocator);
	VK_NULL_RETURN((*pMode = allocateNonDispHandle<DisplayModeKHR, VkDisplayModeKHR>(display, pCreateInfo, pAllocator)));
}

VKAPI_ATTR VkResult VKAPI_CALL createSharedSwapchainsKHR (VkDevice device, deUint32 swapchainCount, const VkSwapchainCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchains)
{
	for (deUint32 ndx = 0; ndx < swapchainCount; ++ndx)
	{
		pSwapchains[ndx] = allocateNonDispHandle<SwapchainKHR, VkSwapchainKHR>(device, pCreateInfos+ndx, pAllocator);
	}

	return VK_SUCCESS;
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

	const PlatformInterface&			getPlatformInterface	(void) const { return m_driver;	}

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
