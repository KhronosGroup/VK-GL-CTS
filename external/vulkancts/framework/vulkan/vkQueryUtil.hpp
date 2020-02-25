#ifndef _VKQUERYUTIL_HPP
#define _VKQUERYUTIL_HPP
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
 * \brief Vulkan query utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuMaybe.hpp"
#include "deMemory.h"

#include <vector>
#include <string>

namespace vk
{

// API version introspection

void											getCoreInstanceExtensions						(deUint32 apiVersion, std::vector<const char*>& dst);
void											getCoreDeviceExtensions							(deUint32 apiVersion, std::vector<const char*>& dst);
bool											isCoreInstanceExtension							(const deUint32 apiVersion, const std::string& extension);
bool											isCoreDeviceExtension							(const deUint32 apiVersion, const std::string& extension);

// API queries

std::vector<VkPhysicalDevice>					enumeratePhysicalDevices						(const InstanceInterface& vk, VkInstance instance);
std::vector<VkPhysicalDeviceGroupProperties>	enumeratePhysicalDeviceGroups					(const InstanceInterface& vk, VkInstance instance);
std::vector<VkQueueFamilyProperties>			getPhysicalDeviceQueueFamilyProperties			(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceFeatures						getPhysicalDeviceFeatures						(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceFeatures2						getPhysicalDeviceFeatures2						(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkan11Features				getPhysicalDeviceVulkan11Features				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkan12Features				getPhysicalDeviceVulkan12Features				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkan11Properties				getPhysicalDeviceVulkan11Properties				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkan12Properties				getPhysicalDeviceVulkan12Properties				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceProperties						getPhysicalDeviceProperties						(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceMemoryProperties				getPhysicalDeviceMemoryProperties				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkFormatProperties								getPhysicalDeviceFormatProperties				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice, VkFormat format);
VkImageFormatProperties							getPhysicalDeviceImageFormatProperties			(const InstanceInterface& vk, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags);
std::vector<VkSparseImageFormatProperties>		getPhysicalDeviceSparseImageFormatProperties	(const InstanceInterface& vk, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling);

VkMemoryRequirements							getBufferMemoryRequirements						(const DeviceInterface& vk, VkDevice device, VkBuffer buffer);
VkMemoryRequirements							getImageMemoryRequirements						(const DeviceInterface& vk, VkDevice device, VkImage image);
VkMemoryRequirements							getImagePlaneMemoryRequirements					(const DeviceInterface& vk, VkDevice device, VkImage image, VkImageAspectFlagBits planeAspect);
std::vector<VkSparseImageMemoryRequirements>	getImageSparseMemoryRequirements				(const DeviceInterface& vk, VkDevice device, VkImage image);

std::vector<VkLayerProperties>					enumerateInstanceLayerProperties				(const PlatformInterface& vkp);
std::vector<VkExtensionProperties>				enumerateInstanceExtensionProperties			(const PlatformInterface& vkp, const char* layerName);
std::vector<VkLayerProperties>					enumerateDeviceLayerProperties					(const InstanceInterface& vki, VkPhysicalDevice physicalDevice);
std::vector<VkExtensionProperties>				enumerateDeviceExtensionProperties				(const InstanceInterface& vki, VkPhysicalDevice physicalDevice, const char* layerName);

VkQueue											getDeviceQueue									(const DeviceInterface& vkd, VkDevice device, deUint32 queueFamilyIndex, deUint32 queueIndex);
VkQueue											getDeviceQueue2									(const DeviceInterface& vkd, VkDevice device, const VkDeviceQueueInfo2 *queueInfo);

// Feature / extension support

bool											isShaderStageSupported							(const VkPhysicalDeviceFeatures& deviceFeatures, VkShaderStageFlagBits stage);

struct RequiredExtension
{
	std::string				name;
	tcu::Maybe<deUint32>	minVersion;
	tcu::Maybe<deUint32>	maxVersion;

	explicit RequiredExtension (const std::string&		name_,
								tcu::Maybe<deUint32>	minVersion_ = tcu::nothing<deUint32>(),
								tcu::Maybe<deUint32>	maxVersion_ = tcu::nothing<deUint32>())
		: name			(name_)
		, minVersion	(minVersion_)
		, maxVersion	(maxVersion_)
	{}
};

struct RequiredLayer
{
	std::string				name;
	tcu::Maybe<deUint32>	minSpecVersion;
	tcu::Maybe<deUint32>	maxSpecVersion;
	tcu::Maybe<deUint32>	minImplVersion;
	tcu::Maybe<deUint32>	maxImplVersion;

	explicit RequiredLayer (const std::string&			name_,
							tcu::Maybe<deUint32>		minSpecVersion_		= tcu::nothing<deUint32>(),
							tcu::Maybe<deUint32>		maxSpecVersion_		= tcu::nothing<deUint32>(),
							tcu::Maybe<deUint32>		minImplVersion_		= tcu::nothing<deUint32>(),
							tcu::Maybe<deUint32>		maxImplVersion_		= tcu::nothing<deUint32>())
		: name			(name_)
		, minSpecVersion(minSpecVersion_)
		, maxSpecVersion(maxSpecVersion_)
		, minImplVersion(minImplVersion_)
		, maxImplVersion(maxImplVersion_)
	{}
};

bool										isCompatible							(const VkExtensionProperties& extensionProperties, const RequiredExtension& required);
bool										isCompatible							(const VkLayerProperties& layerProperties, const RequiredLayer& required);

template<typename ExtensionIterator>
bool										isExtensionSupported					(ExtensionIterator begin, ExtensionIterator end, const RequiredExtension& required);
bool										isExtensionSupported					(const std::vector<VkExtensionProperties>& extensions, const RequiredExtension& required);

bool										isInstanceExtensionSupported			(const deUint32 instanceVersion, const std::vector<std::string>& extensions, const std::string& required);

template<typename LayerIterator>
bool										isLayerSupported						(LayerIterator begin, LayerIterator end, const RequiredLayer& required);
bool										isLayerSupported						(const std::vector<VkLayerProperties>& layers, const RequiredLayer& required);

const void*									findStructureInChain					(const void* first, VkStructureType type);
void*										findStructureInChain					(void* first, VkStructureType type);

template<typename StructType>
VkStructureType								getStructureType						(void);

template<typename StructType>
const StructType*							findStructure							(const void* first)
{
	return reinterpret_cast<const StructType*>(findStructureInChain(first, getStructureType<StructType>()));
}

template<typename StructType>
StructType*									findStructure							(void* first)
{
	return reinterpret_cast<StructType*>(findStructureInChain(first, getStructureType<StructType>()));
}

struct initVulkanStructure
{
	initVulkanStructure	(void*	pNext = DE_NULL)	: m_next(pNext)	{};

	template<class StructType>
	operator StructType()
	{
		StructType result;

		deMemset(&result, 0x00, sizeof(StructType));

		result.sType	= getStructureType<StructType>();
		result.pNext	= m_next;

		return result;
	}

private:
	void*	m_next;
};

template<class StructType>
void addToChainVulkanStructure (void***	chainPNextPtr, StructType&	structType)
{
	DE_ASSERT(chainPNextPtr != DE_NULL);

	(**chainPNextPtr) = &structType;

	(*chainPNextPtr) = &structType.pNext;
}

struct initVulkanStructureConst
{
	initVulkanStructureConst	(const void*	pNext = DE_NULL)	: m_next(pNext)	{};

	template<class StructType>
	operator const StructType()
	{
		StructType result;

		deMemset(&result, 0x00, sizeof(StructType));

		result.sType	= getStructureType<StructType>();
		result.pNext	= const_cast<void*>(m_next);

		return result;
	}

private:
	const void*	m_next;
};

struct getPhysicalDeviceExtensionProperties
{
	getPhysicalDeviceExtensionProperties (const InstanceInterface&	vki, VkPhysicalDevice physicalDevice) : m_vki(vki), m_physicalDevice(physicalDevice) {};

	template<class ExtensionProperties>
	operator ExtensionProperties ()
	{
		VkPhysicalDeviceProperties2	properties2;
		ExtensionProperties			extensionProperties;

		deMemset(&extensionProperties, 0x00, sizeof(ExtensionProperties));
		extensionProperties.sType = getStructureType<ExtensionProperties>();

		deMemset(&properties2, 0x00, sizeof(properties2));
		properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties2.pNext = &extensionProperties;

		m_vki.getPhysicalDeviceProperties2(m_physicalDevice, &properties2);

		return extensionProperties;
	}

	operator VkPhysicalDeviceProperties2 ()
	{
		VkPhysicalDeviceProperties2	properties2;

		deMemset(&properties2, 0x00, sizeof(properties2));
		properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

		m_vki.getPhysicalDeviceProperties2(m_physicalDevice, &properties2);

		return properties2;
	}

private:
	const InstanceInterface&	m_vki;
	const VkPhysicalDevice		m_physicalDevice;
};

namespace ValidateQueryBits
{

typedef struct
{
	size_t		offset;
	size_t		size;
} QueryMemberTableEntry;

template <typename Context, typename Interface, typename Type>
//!< Return variable initialization validation
bool validateInitComplete(Context context, void (Interface::*Function)(Context, Type*)const, const Interface& interface, const QueryMemberTableEntry* queryMemberTableEntry)
{
	const QueryMemberTableEntry	*iterator;
	Type vec[2];
	deMemset(&vec[0], 0x00, sizeof(Type));
	deMemset(&vec[1], 0xFF, sizeof(Type));

	(interface.*Function)(context, &vec[0]);
	(interface.*Function)(context, &vec[1]);

	for (iterator = queryMemberTableEntry; iterator->size != 0; iterator++)
	{
		if (deMemCmp(((deUint8*)(&vec[0]))+iterator->offset, ((deUint8*)(&vec[1]))+iterator->offset, iterator->size) != 0)
			return false;
	}

	return true;
}

template <typename Type>
//!< Return variable initialization validation
bool validateStructsWithGuard (const QueryMemberTableEntry* queryMemberTableEntry, Type* vec[2], const deUint8 guardValue, const deUint32 guardSize)
{
	const QueryMemberTableEntry	*iterator;

	for (iterator = queryMemberTableEntry; iterator->size != 0; iterator++)
	{
		if (deMemCmp(((deUint8*)(vec[0]))+iterator->offset, ((deUint8*)(vec[1]))+iterator->offset, iterator->size) != 0)
			return false;
	}

	for (deUint32 vecNdx = 0; vecNdx < 2; ++vecNdx)
	{
		for (deUint32 ndx = 0; ndx < guardSize; ndx++)
		{
			if (((deUint8*)(vec[vecNdx]))[ndx + sizeof(Type)] != guardValue)
				return false;
		}
	}

	return true;
}

template<typename IterT>
//! Overwrite a range of objects with an 8-bit pattern.
inline void fillBits (IterT beg, const IterT end, const deUint8 pattern = 0xdeu)
{
	for (; beg < end; ++beg)
		deMemset(&(*beg), static_cast<int>(pattern), sizeof(*beg));
}

template<typename IterT>
//! Verify that each byte of a range of objects is equal to an 8-bit pattern.
bool checkBits (IterT beg, const IterT end, const deUint8 pattern = 0xdeu)
{
	for (; beg < end; ++beg)
	{
		const deUint8* elementBytes = reinterpret_cast<const deUint8*>(&(*beg));
		for (std::size_t i = 0u; i < sizeof(*beg); ++i)
		{
			if (elementBytes[i] != pattern)
				return false;
		}
	}
	return true;
}

} // ValidateQueryBits

// Template implementations

template<typename ExtensionIterator>
bool isExtensionSupported (ExtensionIterator begin, ExtensionIterator end, const RequiredExtension& required)
{
	for (ExtensionIterator cur = begin; cur != end; ++cur)
	{
		if (isCompatible(*cur, required))
			return true;
	}
	return false;
}

template<typename LayerIterator>
bool isLayerSupported (LayerIterator begin, LayerIterator end, const RequiredLayer& required)
{
	for (LayerIterator cur = begin; cur != end; ++cur)
	{
		if (isCompatible(*cur, required))
			return true;
	}
	return false;
}

} // vk

#endif // _VKQUERYUTIL_HPP
