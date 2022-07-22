#ifndef _VKDEVICEPROPERTIES_HPP
#define _VKDEVICEPROPERTIES_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
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
 *//*!
 * \file
 * \brief Vulkan DeviceProperties class utility.
 *//*--------------------------------------------------------------------*/

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "deMemory.h"
#include "vkDefs.hpp"

namespace vk
{

// Structure describing vulkan property structure
struct PropertyDesc
{
	VkStructureType		sType;
	const char*			name;
	const deUint32		specVersion;
	const deUint32		typeId;
};

// Structure containg all property blobs - this simplifies generated code
struct AllPropertiesBlobs
{
	VkPhysicalDeviceVulkan11Properties& vk11;
	VkPhysicalDeviceVulkan12Properties& vk12;
#ifndef CTS_USES_VULKANSC
	VkPhysicalDeviceVulkan13Properties& vk13;
#endif // CTS_USES_VULKANSC
	// add blobs from future vulkan versions here
};

// Base class for all PropertyStructWrapper specialization
struct PropertyStructWrapperBase
{
	virtual					~PropertyStructWrapperBase	(void) {}
	virtual void			initializePropertyFromBlob	(const AllPropertiesBlobs& allPropertiesBlobs) = 0;
	virtual deUint32		getPropertyTypeId			(void) const = 0;
	virtual PropertyDesc	getPropertyDesc				(void) const = 0;
	virtual void**			getPropertyTypeNext			(void) = 0;
	virtual void*			getPropertyTypeRaw			(void) = 0;
};

using PropertyStructWrapperCreator	= PropertyStructWrapperBase* (*) (void);
struct PropertyStructCreationData
{
	PropertyStructWrapperCreator creatorFunction;
	const char*					 name;
	deUint32					 specVersion;
};

template<class PropertyType> class PropertyStructWrapper;
template<class PropertyType> PropertyDesc makePropertyDesc (void);

template<class PropertyType>
PropertyStructWrapperBase* createPropertyStructWrapper (void)
{
	return new PropertyStructWrapper<PropertyType>(makePropertyDesc<PropertyType>());
}

template<class PropertyType>
void initPropertyFromBlob(PropertyType& propertyType, const AllPropertiesBlobs& allPropertiesBlobs);

template<class PropertyType>
void initPropertyFromBlobWrapper(PropertyType& propertyType, const AllPropertiesBlobs& allPropertiesBlobs)
{
	initPropertyFromBlob<PropertyType>(propertyType, allPropertiesBlobs);
}

class DeviceProperties
{
public:
												DeviceProperties				(const InstanceInterface&			vki,
																				 const deUint32						apiVersion,
																				 const VkPhysicalDevice				physicalDevice,
																				 const std::vector<std::string>&	instanceExtensions,
																				 const std::vector<std::string>&	deviceExtensions);

												~DeviceProperties				(void);

	template<class PropertyType>
	const PropertyType&							getPropertyType				(void) const;

	const VkPhysicalDeviceProperties2&			getCoreProperties2			(void) const { return m_coreProperties2; }
	const VkPhysicalDeviceVulkan11Properties&	getVulkan11Properties		(void) const { return m_vulkan11Properties; }
	const VkPhysicalDeviceVulkan12Properties&	getVulkan12Properties		(void) const { return m_vulkan12Properties; }
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceVulkan13Properties&	getVulkan13Properties		(void) const { return m_vulkan13Properties; }
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
	const VkPhysicalDeviceVulkanSC10Properties&	getVulkanSC10Properties		(void) const { return m_vulkanSC10Properties; }
#endif // CTS_USES_VULKANSC

	bool										contains					(const std::string& property, bool throwIfNotExists = false) const;

	bool										isDevicePropertyInitialized	(VkStructureType sType) const;

private:

	static void									addToChainStructWrapper(void*** chainPNextPtr, PropertyStructWrapperBase* structWrapper);

private:

	VkPhysicalDeviceProperties2						m_coreProperties2;
	mutable std::vector<PropertyStructWrapperBase*>	m_properties;
	VkPhysicalDeviceVulkan11Properties				m_vulkan11Properties;
	VkPhysicalDeviceVulkan12Properties				m_vulkan12Properties;
#ifndef CTS_USES_VULKANSC
	VkPhysicalDeviceVulkan13Properties				m_vulkan13Properties;
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
	VkPhysicalDeviceVulkanSC10Properties			m_vulkanSC10Properties;
#endif // CTS_USES_VULKANSC
};

template<class PropertyType>
const PropertyType&	DeviceProperties::getPropertyType(void) const
{
	typedef PropertyStructWrapper<PropertyType>* PropertyWrapperPtr;

	const PropertyDesc		propDesc	= makePropertyDesc<PropertyType>();
	const VkStructureType	sType		= propDesc.sType;

	// try to find property by sType
	for (auto property : m_properties)
	{
		if (sType == property->getPropertyDesc().sType)
			return static_cast<PropertyWrapperPtr>(property)->getPropertyTypeRef();
	}

	// try to find property by id that was assigned by gen_framework script
	const deUint32 propertyId = propDesc.typeId;
	for (auto property : m_properties)
	{
		if (propertyId == property->getPropertyTypeId())
			return static_cast<PropertyWrapperPtr>(property)->getPropertyTypeRef();
	}

	// if initialized property structure was not found create empty one and return it
	m_properties.push_back(vk::createPropertyStructWrapper<PropertyType>());
	return static_cast<PropertyWrapperPtr>(m_properties.back())->getPropertyTypeRef();
}

template<class PropertyType>
class PropertyStructWrapper : public PropertyStructWrapperBase
{
public:
	PropertyStructWrapper (const PropertyDesc& propertyDesc)
		: m_propertyDesc(propertyDesc)
	{
		deMemset(&m_propertyType, 0, sizeof(m_propertyType));
		m_propertyType.sType = propertyDesc.sType;
	}

	void initializePropertyFromBlob (const AllPropertiesBlobs& allPropertiesBlobs)
	{
		initPropertyFromBlobWrapper(m_propertyType, allPropertiesBlobs);
	}

	deUint32		getPropertyTypeId	(void) const	{ return m_propertyDesc.typeId;	}
	PropertyDesc	getPropertyDesc		(void) const	{ return m_propertyDesc;			}
	void**			getPropertyTypeNext	(void)			{ return &m_propertyType.pNext;	}
	void*			getPropertyTypeRaw	(void)			{ return &m_propertyType;		}
	PropertyType&	getPropertyTypeRef	(void)			{ return m_propertyType;			}

public:
	// metadata about property structure
	const PropertyDesc	m_propertyDesc;

	// actual vulkan property structure
	PropertyType			m_propertyType;
};
} // vk

#endif // _VKDEVICEPROPERTIES_HPP
