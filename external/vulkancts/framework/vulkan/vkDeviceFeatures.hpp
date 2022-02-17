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

// Structure describing vulkan feature structure
struct FeatureDesc
{
	VkStructureType		sType;
	const char*			name;
	const deUint32		specVersion;
	const deUint32		typeId;
};

// Structure containg all feature blobs - this simplifies generated code
struct AllFeaturesBlobs
{
	VkPhysicalDeviceVulkan11Features& vk11;
	VkPhysicalDeviceVulkan12Features& vk12;
#ifndef CTS_USES_VULKANSC
	VkPhysicalDeviceVulkan13Features& vk13;
#endif // CTS_USES_VULKANSC
	// add blobs from future vulkan versions here
};

// Base class for all FeatureStructWrapper specializations
class FeatureStructWrapperBase
{
public:
	virtual					~FeatureStructWrapperBase	(void) {}
	virtual void			initializeFeatureFromBlob	(const AllFeaturesBlobs& allFeaturesBlobs) = 0;
	virtual deUint32		getFeatureTypeId			(void) const = 0;
	virtual FeatureDesc		getFeatureDesc				(void) const = 0;
	virtual void**			getFeatureTypeNext			(void) = 0;
	virtual void*			getFeatureTypeRaw			(void) = 0;
};

using FeatureStructWrapperCreator	= FeatureStructWrapperBase* (*) (void);
struct FeatureStructCreationData
{
	FeatureStructWrapperCreator	creatorFunction;
	const char*					name;
	deUint32					specVersion;
};

template<class FeatureType> class FeatureStructWrapper;
template<class FeatureType> FeatureDesc makeFeatureDesc (void);

template<class FeatureType>
FeatureStructWrapperBase* createFeatureStructWrapper (void)
{
	return new FeatureStructWrapper<FeatureType>(makeFeatureDesc<FeatureType>());
}

template<class FeatureType>
void initFeatureFromBlob(FeatureType& featureType, const AllFeaturesBlobs& allFeaturesBlobs);

template<class FeatureType>
void initFeatureFromBlobWrapper(FeatureType& featureType, const AllFeaturesBlobs& allFeaturesBlobs)
{
	initFeatureFromBlob<FeatureType>(featureType, allFeaturesBlobs);
}

class DeviceFeatures
{
public:
												DeviceFeatures				(const InstanceInterface&			vki,
																			 const deUint32						apiVersion,
																			 const VkPhysicalDevice				physicalDevice,
																			 const std::vector<std::string>&	instanceExtensions,
																			 const std::vector<std::string>&	deviceExtensions,
																			 const deBool						enableAllFeatures = DE_FALSE);

												~DeviceFeatures				(void);

	template<class FeatureType>
	const FeatureType&							getFeatureType				(void) const;

	const VkPhysicalDeviceFeatures2&			getCoreFeatures2			(void) const { return m_coreFeatures2; }
	const VkPhysicalDeviceVulkan11Features&		getVulkan11Features			(void) const { return m_vulkan11Features; }
	const VkPhysicalDeviceVulkan12Features&		getVulkan12Features			(void) const { return m_vulkan12Features; }
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceVulkan13Features&		getVulkan13Features			(void) const { return m_vulkan13Features; }
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
	const VkPhysicalDeviceVulkanSC10Features&	getVulkanSC10Features		(void) const { return m_vulkanSC10Features; }
#endif // CTS_USES_VULKANSC

	bool										contains					(const std::string& feature, bool throwIfNotExists = false) const;

	bool										isDeviceFeatureInitialized	(VkStructureType sType) const;

private:

	static bool							verifyFeatureAddCriteria	(const FeatureStructCreationData& item, const std::vector<VkExtensionProperties>& properties);

private:

	VkPhysicalDeviceFeatures2						m_coreFeatures2;
	mutable std::vector<FeatureStructWrapperBase*>	m_features;
	VkPhysicalDeviceVulkan11Features				m_vulkan11Features;
	VkPhysicalDeviceVulkan12Features				m_vulkan12Features;
#ifndef CTS_USES_VULKANSC
	VkPhysicalDeviceVulkan13Features				m_vulkan13Features;
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
	VkPhysicalDeviceVulkanSC10Features				m_vulkanSC10Features;
#endif // CTS_USES_VULKANSC
};

template<class FeatureType>
const FeatureType& DeviceFeatures::getFeatureType(void) const
{
	typedef FeatureStructWrapper<FeatureType>* FeatureWrapperPtr;

	const FeatureDesc		featDesc	= makeFeatureDesc<FeatureType>();
	const VkStructureType	sType		= featDesc.sType;

	// try to find feature by sType
	for (auto feature : m_features)
	{
		if (sType == feature->getFeatureDesc().sType)
			return static_cast<FeatureWrapperPtr>(feature)->getFeatureTypeRef();
	}

	// try to find feature by id that was assigned by gen_framework script
	const deUint32 featureId = featDesc.typeId;
	for (auto feature : m_features)
	{
		if (featureId == feature->getFeatureTypeId())
			return static_cast<FeatureWrapperPtr>(feature)->getFeatureTypeRef();
	}

	// if initialized feature structure was not found create empty one and return it
	m_features.push_back(vk::createFeatureStructWrapper<FeatureType>());
	return static_cast<FeatureWrapperPtr>(m_features.back())->getFeatureTypeRef();
}

template<class FeatureType>
class FeatureStructWrapper : public FeatureStructWrapperBase
{
public:
	FeatureStructWrapper (const FeatureDesc& featureDesc)
		: m_featureDesc(featureDesc)
	{
		deMemset(&m_featureType, 0, sizeof(m_featureType));
		m_featureType.sType = featureDesc.sType;
	}

	void initializeFeatureFromBlob (const AllFeaturesBlobs& allFeaturesBlobs)
	{
		initFeatureFromBlobWrapper(m_featureType, allFeaturesBlobs);
	}

	deUint32		getFeatureTypeId	(void) const	{ return m_featureDesc.typeId;	}
	FeatureDesc		getFeatureDesc		(void) const	{ return m_featureDesc;			}
	void**			getFeatureTypeNext	(void)			{ return &m_featureType.pNext;	}
	void*			getFeatureTypeRaw	(void)			{ return &m_featureType;		}
	FeatureType&	getFeatureTypeRef	(void)			{ return m_featureType;			}

public:
	// metadata about feature structure
	const FeatureDesc	m_featureDesc;

	// actual vulkan feature structure
	FeatureType			m_featureType;
};

} // vk

#endif // _VKDEVICEFEATURES_HPP
