#ifndef _VKDEVICEFEATURES_HPP
#define _VKDEVICEFEATURES_HPP
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
 * \brief Vulkan DeviceFeatures class utility.
 *//*--------------------------------------------------------------------*/

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "deMemory.h"
#include "vkDefs.hpp"

namespace vk
{

struct FeatureDesc
{
						FeatureDesc (VkStructureType sType_, const char* name_, deUint32 specVersion_, deUint32 typeId_)
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

struct FeatureStruct
{
	virtual deUint32		getFeatureTypeId		(void) const = 0;
	virtual FeatureDesc		getFeatureDesc			(void) const = 0;
	virtual void**			getFeatureTypeNext		(void) = 0;
	virtual void*			getFeatureTypeRaw		(void) = 0;
	virtual					~FeatureStruct			(void) {}
};


struct FeatureStructMapItem
{
	FeatureStruct*	(*creator)(void);
	const char*		name;
	deUint32		specVersion;
};

template<class FeatureType> struct FeatureStructWrapper;
template<class FeatureType> FeatureDesc makeFeatureDesc (void);

template<class FeatureType>
FeatureStruct* createFeatureStructWrapper (void)
{
	return new FeatureStructWrapper<FeatureType>(makeFeatureDesc<FeatureType>());
}

class DeviceFeatures
{
public:
										DeviceFeatures		(const InstanceInterface&			vki,
															 const deUint32						apiVersion,
															 const VkPhysicalDevice				physicalDevice,
															 const std::vector<std::string>&	instanceExtensions,
															 const std::vector<std::string>&	deviceExtensions);

										~DeviceFeatures		(void);

	template<class FeatureType>
	bool								getFeatureType		(FeatureType&						featureType) const
	{
		typedef FeatureStructWrapper<FeatureType>	*FeatureWrapperPtr;

		const VkStructureType	sType		= makeFeatureDesc<FeatureType>().sType;
		const size_t			featCount	= m_features.size();

		for (size_t featIdx = 0; featIdx < featCount; ++featIdx)
		{
			if (sType == m_features[featIdx]->getFeatureDesc().sType)
			{
				featureType = static_cast<FeatureWrapperPtr>(m_features[featIdx])->getFeatureTypeRef();
				return true;
			}
		}
		return false;
	}

	template<class FeatureType>
	const FeatureType&					getFeatureType		(void) const
	{
		typedef FeatureStructWrapper<FeatureType>	*FeatureWrapperPtr;

		const FeatureDesc		featDesc	= makeFeatureDesc<FeatureType>();
		const VkStructureType	sType		= featDesc.sType;
		const size_t			featCount	= m_features.size();

		for (size_t featIdx = 0; featIdx < featCount; ++featIdx)
		{
			if (sType == m_features[featIdx]->getFeatureDesc().sType)
				return static_cast<FeatureWrapperPtr>(m_features[featIdx])->getFeatureTypeRef();
		}

		const deUint32			featureId = featDesc.typeId;

		for (size_t featIdx = 0; featIdx < featCount; ++featIdx)
		{
			if (featureId == m_features[featIdx]->getFeatureTypeId())
				return static_cast<FeatureWrapperPtr>(m_features[featIdx])->getFeatureTypeRef();
		}

		FeatureWrapperPtr p = new FeatureStructWrapper<FeatureType>;
		m_features.push_back(p);

		return p->getFeatureTypeRef();
	}

	const VkPhysicalDeviceFeatures2&	getCoreFeatures2	(void) const { return m_coreFeatures2; }

	bool								contains			(const std::string& feature, bool throwIfNotExists = false) const;

private:

	static FeatureStruct*				createFeatureStructWrapper	(const std::string& s);

	static bool							verifyFeatureAddCriteria	(const FeatureStructMapItem& item, const std::vector<VkExtensionProperties>& properties);

	VkPhysicalDeviceFeatures2			m_coreFeatures2;
	mutable std::vector<FeatureStruct*>	m_features;
};

template<class FeatureType>
struct FeatureStructWrapper : FeatureStruct
{
	const FeatureDesc	m_featureDesc;
	FeatureType			m_featureType;

						FeatureStructWrapper	(void)
							: m_featureDesc	(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, DE_NULL, ~0u, 0u)
						{
							deMemset(&m_featureType, 0, sizeof(m_featureType));
						}

						FeatureStructWrapper	(const FeatureDesc& featureDesc)
							: m_featureDesc	(featureDesc)
						{
							deMemset(&m_featureType, 0, sizeof(m_featureType));
							m_featureType.sType = featureDesc.sType;
						}

	deUint32			getFeatureTypeId		(void) const	{ return m_featureDesc.typeId;	}
	FeatureDesc			getFeatureDesc			(void) const	{ return m_featureDesc;			}
	void**				getFeatureTypeNext		(void)			{ return &m_featureType.pNext;	}
	void*				getFeatureTypeRaw		(void)			{ return &m_featureType;		}
	FeatureType&		getFeatureTypeRef		(void)			{ return m_featureType;			}
};

} // vk

#endif // _VKDEVICEFEATURES_HPP
