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

struct PropertyDesc
{
						PropertyDesc (VkStructureType sType_, const char* name_, deUint32 specVersion_, deUint32 typeId_)
							: name			(name_)
							, sType			(sType_)
							, specVersion	(specVersion_)
							, typeId		(typeId_)
						{}

	const char*			name;
	VkStructureType		sType;
	const deUint32		specVersion;
	const deUint32		typeId;
};

struct PropertyStruct
{
	virtual deUint32		getPropertyTypeId		(void) const = 0;
	virtual PropertyDesc	getPropertyDesc			(void) const = 0;
	virtual void**			getPropertyTypeNext		(void) = 0;
	virtual void*			getPropertyTypeRaw		(void) = 0;
	virtual					~PropertyStruct			(void) {}
};


struct PropertyStructMapItem
{
	PropertyStruct*	(*creator)(void);
	const char*		name;
	deUint32		specVersion;
};

template<class PropertyType> struct PropertyStructWrapper;
template<class PropertyType> PropertyDesc makePropertyDesc (void);

template<class PropertyType>
PropertyStruct* createPropertyStructWrapper (void)
{
	return new PropertyStructWrapper<PropertyType>(makePropertyDesc<PropertyType>());
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
	bool									getPropertyType					(PropertyType&						propertyType) const
	{
		typedef PropertyStructWrapper<PropertyType>	*PropertyWrapperPtr;

		const VkStructureType	sType		= makePropertyDesc<PropertyType>().sType;
		const size_t			propCount	= m_properties.size();

		for (size_t propIdx = 0; propIdx < propCount; ++propIdx)
		{
			if (sType == m_properties[propIdx]->getPropertyDesc().sType)
			{
				propertyType = static_cast<PropertyWrapperPtr>(m_properties[propIdx])->getPropertyTypeRef();
				return true;
			}
		}
		return false;
	}

	template<class PropertyType>
	const PropertyType&						getPropertyType					(void) const
	{
		typedef PropertyStructWrapper<PropertyType>	*PropertyWrapperPtr;

		const PropertyDesc		propDesc	= makePropertyDesc<PropertyType>();
		const VkStructureType	sType		= propDesc.sType;
		const size_t			propCount	= m_properties.size();

		for (size_t propIdx = 0; propIdx < propCount; ++propIdx)
		{
			if (sType == m_properties[propIdx]->getPropertyDesc().sType)
				return static_cast<PropertyWrapperPtr>(m_properties[propIdx])->getPropertyTypeRef();
		}

		const deUint32			propertyId = propDesc.typeId;

		for (size_t propIdx = 0; propIdx < propCount; ++propIdx)
		{
			if (propertyId == m_properties[propIdx]->getPropertyTypeId())
				return static_cast<PropertyWrapperPtr>(m_properties[propIdx])->getPropertyTypeRef();
		}

		PropertyStruct* p = vk::createPropertyStructWrapper<PropertyType>();
		m_properties.push_back(p);

		return static_cast<PropertyWrapperPtr>(p)->getPropertyTypeRef();
	}

	const VkPhysicalDeviceProperties2&		getCoreProperties2				(void) const { return m_coreProperties2; }

	bool									contains						(const std::string& property, bool throwIfNotExists = false) const;

	bool									isDevicePropertyInitialized		(VkStructureType sType) const;

private:
	static PropertyStruct*					createPropertyStructWrapper		(const std::string& s);

	VkPhysicalDeviceProperties2				m_coreProperties2;
	mutable std::vector<PropertyStruct*>	m_properties;
};

template<class PropertyType>
struct PropertyStructWrapper : PropertyStruct
{
	const PropertyDesc	m_propertyDesc;
	PropertyType		m_propertyType;

						PropertyStructWrapper	(void)
							: m_propertyDesc	(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, DE_NULL, ~0u, 0u)
						{
							deMemset(&m_propertyType, 0, sizeof(m_propertyType));
						}

						PropertyStructWrapper	(const PropertyDesc& propertyDesc)
							: m_propertyDesc	(propertyDesc)
						{
							deMemset(&m_propertyType, 0, sizeof(m_propertyType));
							m_propertyType.sType = propertyDesc.sType;
						}

	deUint32			getPropertyTypeId		(void) const	{ return m_propertyDesc.typeId;	}
	PropertyDesc		getPropertyDesc			(void) const	{ return m_propertyDesc;		}
	void**				getPropertyTypeNext		(void)			{ return &m_propertyType.pNext;	}
	void*				getPropertyTypeRaw		(void)			{ return &m_propertyType;		}
	PropertyType&		getPropertyTypeRef		(void)			{ return m_propertyType;		}
};

} // vk

#endif // _VKDEVICEPROPERTIES_HPP
