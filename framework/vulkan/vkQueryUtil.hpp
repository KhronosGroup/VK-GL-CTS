#ifndef _VKQUERYUTIL_HPP
#define _VKQUERYUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Utilities
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
VK_DECLARE_QUERY_TRAITS(PhysicalDeviceInfo,	VK_PHYSICAL_DEVICE_INFO_TYPE_QUEUE_PROPERTIES,	VkPhysicalDeviceQueueProperties,	MULTIPLE);

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

} // vk

#endif // _VKQUERYUTIL_HPP
