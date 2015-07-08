#ifndef _VKQUERYUTIL_HPP
#define _VKQUERYUTIL_HPP
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
 * \brief Vulkan query utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "deMeta.hpp"

#include <vector>

namespace vk
{
namespace querydetails
{

enum QueryResultCount
{
	QUERY_RESULT_COUNT_SINGLE	= 0,
	QUERY_RESULT_COUNT_MULTIPLE	= 1
};

template<VkPhysicalDeviceInfoType InfoType>
struct PhysicalDeviceInfoTraits;

#define VK_DECLARE_QUERY_TRAITS(QUERY_CLASS, QUERY_PARAM, RESULT_TYPE, RESULT_COUNT)	\
template<>	\
struct QUERY_CLASS##Traits<QUERY_PARAM>	\
{	\
	typedef RESULT_TYPE	Type;	\
	enum { QUERY_RESULT_COUNT = QUERY_RESULT_COUNT_##RESULT_COUNT };	\
}

VK_DECLARE_QUERY_TRAITS(PhysicalDeviceInfo, VK_PHYSICAL_DEVICE_INFO_TYPE_PROPERTIES,		VkPhysicalDeviceProperties,			SINGLE);
VK_DECLARE_QUERY_TRAITS(PhysicalDeviceInfo, VK_PHYSICAL_DEVICE_INFO_TYPE_PERFORMANCE,		VkPhysicalDevicePerformance,		SINGLE);
VK_DECLARE_QUERY_TRAITS(PhysicalDeviceInfo,	VK_PHYSICAL_DEVICE_INFO_TYPE_QUEUE_PROPERTIES,	VkPhysicalDeviceQueueProperties,	MULTIPLE);
VK_DECLARE_QUERY_TRAITS(PhysicalDeviceInfo,	VK_PHYSICAL_DEVICE_INFO_TYPE_MEMORY_PROPERTIES,	VkPhysicalDeviceMemoryProperties,	MULTIPLE);

template<VkPhysicalDeviceInfoType InfoType>
std::vector<typename PhysicalDeviceInfoTraits<InfoType>::Type> getPhysicalDeviceInfoImpl (const DeviceInterface& vk, VkPhysicalDevice physicalDevice)
{
	std::vector<typename PhysicalDeviceInfoTraits<InfoType>::Type>	values;
	deUintptr														infoSize	= 0;

	VK_CHECK(vk.getPhysicalDeviceInfo(physicalDevice, InfoType, &infoSize, DE_NULL));

	if (infoSize % sizeof(typename PhysicalDeviceInfoTraits<InfoType>::Type) != 0)
		TCU_FAIL("Returned info size is not divisible by structure size");

	if (infoSize > 0)
	{
		values.resize(infoSize / sizeof(typename PhysicalDeviceInfoTraits<InfoType>::Type));
		VK_CHECK(vk.getPhysicalDeviceInfo(physicalDevice, InfoType, &infoSize, &values[0]));

		if (infoSize != values.size()*sizeof(typename PhysicalDeviceInfoTraits<InfoType>::Type))
			TCU_FAIL("Returned info size changed between queries");
	}

	return values;
}

template<VkPhysicalDeviceInfoType InfoType>
std::vector<typename PhysicalDeviceInfoTraits<InfoType>::Type> getPhysicalDeviceInfo (typename de::meta::EnableIf<const DeviceInterface&, PhysicalDeviceInfoTraits<InfoType>::QUERY_RESULT_COUNT == QUERY_RESULT_COUNT_MULTIPLE>::Type vk, VkPhysicalDevice physicalDevice)
{
	return getPhysicalDeviceInfoImpl<InfoType>(vk, physicalDevice);
}

template<VkPhysicalDeviceInfoType InfoType>
typename PhysicalDeviceInfoTraits<InfoType>::Type getPhysicalDeviceInfo (typename de::meta::EnableIf<const DeviceInterface&, PhysicalDeviceInfoTraits<InfoType>::QUERY_RESULT_COUNT == QUERY_RESULT_COUNT_SINGLE>::Type vk, VkPhysicalDevice physicalDevice)
{
	const std::vector<typename PhysicalDeviceInfoTraits<InfoType>::Type>	values	= getPhysicalDeviceInfoImpl<InfoType>(vk, physicalDevice);

	if (values.size() != 1)
		TCU_FAIL("Expected only single value");

	return values[0];
}

template<VkObjectInfoType InfoType>
struct ObjectInfoTraits;

VK_DECLARE_QUERY_TRAITS(ObjectInfo,	VK_OBJECT_INFO_TYPE_MEMORY_REQUIREMENTS,	VkMemoryRequirements,	MULTIPLE);

template<VkObjectInfoType InfoType>
std::vector<typename ObjectInfoTraits<InfoType>::Type> getObjectInfoImpl (const DeviceInterface& vk, VkDevice device, VkObjectType objectType, VkObject object)
{
	std::vector<typename ObjectInfoTraits<InfoType>::Type>	values;
	deUintptr												infoSize	= 0;

	VK_CHECK(vk.getObjectInfo(device, objectType, object, InfoType, &infoSize, DE_NULL));

	if (infoSize % sizeof(typename ObjectInfoTraits<InfoType>::Type) != 0)
		TCU_FAIL("Returned info size is not divisible by structure size");

	if (infoSize > 0)
	{
		values.resize(infoSize / sizeof(typename ObjectInfoTraits<InfoType>::Type));
		VK_CHECK(vk.getObjectInfo(device, objectType, object, InfoType, &infoSize, &values[0]));

		if (infoSize != values.size()*sizeof(typename ObjectInfoTraits<InfoType>::Type))
			TCU_FAIL("Returned info size changed between queries");
	}

	return values;
}

template<VkObjectInfoType InfoType>
std::vector<typename ObjectInfoTraits<InfoType>::Type> getObjectInfo (typename de::meta::EnableIf<const DeviceInterface&, ObjectInfoTraits<InfoType>::QUERY_RESULT_COUNT == QUERY_RESULT_COUNT_MULTIPLE>::Type vk, VkDevice device, VkObjectType objectType, VkObject object)
{
	return getObjectInfoImpl<InfoType>(vk, device, objectType, object);
}

template<VkObjectInfoType InfoType, typename T>
std::vector<typename ObjectInfoTraits<InfoType>::Type> getObjectInfo (typename de::meta::EnableIf<const DeviceInterface&, ObjectInfoTraits<InfoType>::QUERY_RESULT_COUNT == QUERY_RESULT_COUNT_MULTIPLE>::Type vk, VkDevice device, const Unique<T>& object)
{
	return getObjectInfo<InfoType>(vk, device, getObjectType<T>(), object.get());
}

} // querydetails

using querydetails::getPhysicalDeviceInfo;
using querydetails::getObjectInfo;

std::vector<VkPhysicalDevice>	enumeratePhysicalDevices	(const PlatformInterface& vk, VkInstance instance);

} // vk

#endif // _VKQUERYUTIL_HPP
