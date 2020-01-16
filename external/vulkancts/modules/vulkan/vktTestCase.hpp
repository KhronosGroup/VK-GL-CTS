#ifndef _VKTTESTCASE_HPP
#define _VKTTESTCASE_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
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
 * \brief Vulkan test case base classes
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vkDefs.hpp"
#include "deUniquePtr.hpp"
#include "vkPrograms.hpp"
#include "vkApiVersion.hpp"
#include "vkDebugReportUtil.hpp"
#include "vkPlatform.hpp"
#include "vktTestCaseDefs.hpp"
#include <vector>
#include <string>

namespace glu
{
struct ProgramSources;
}

namespace vk
{
class PlatformInterface;
class Allocator;
struct SourceCollections;
}

namespace vkt
{

class DefaultDevice;

class Context
{
public:
												Context								(tcu::TestContext&				testCtx,
																					 const vk::PlatformInterface&	platformInterface,
																					 vk::BinaryCollection&			progCollection);
												~Context							(void);

	tcu::TestContext&							getTestContext						(void) const { return m_testCtx;			}
	const vk::PlatformInterface&				getPlatformInterface				(void) const { return m_platformInterface;	}
	vk::BinaryCollection&						getBinaryCollection					(void) const { return m_progCollection;		}

	// Default instance & device, selected with --deqp-vk-device-id=N
	deUint32									getMaximumFrameworkVulkanVersion	(void) const;
	deUint32									getAvailableInstanceVersion			(void) const;
	const std::vector<std::string>&				getInstanceExtensions				(void) const;
	vk::VkInstance								getInstance							(void) const;
	const vk::InstanceInterface&				getInstanceInterface				(void) const;
	vk::VkPhysicalDevice						getPhysicalDevice					(void) const;
	deUint32									getDeviceVersion					(void) const;
	bool										isDeviceFeatureInitialized			(vk::VkStructureType sType) const;
	const vk::VkPhysicalDeviceFeatures&			getDeviceFeatures					(void) const;
	const vk::VkPhysicalDeviceFeatures2&		getDeviceFeatures2					(void) const;

	bool										isInstanceFunctionalitySupported	(const std::string& extension) const;
	bool										isDeviceFunctionalitySupported		(const std::string& extension) const;

#include "vkDeviceFeaturesForContextDecl.inl"

	bool										isDevicePropertyInitialized			(vk::VkStructureType sType) const;
	const vk::VkPhysicalDeviceProperties&		getDeviceProperties					(void) const;
	const vk::VkPhysicalDeviceProperties2&		getDeviceProperties2				(void) const;

#include "vkDevicePropertiesForContextDecl.inl"

	const std::vector<std::string>&				getDeviceExtensions					(void) const;
	vk::VkDevice								getDevice							(void) const;
	const vk::DeviceInterface&					getDeviceInterface					(void) const;
	deUint32									getUniversalQueueFamilyIndex		(void) const;
	vk::VkQueue									getUniversalQueue					(void) const;
	deUint32									getUsedApiVersion					(void) const;
	deUint32									getSparseQueueFamilyIndex			(void) const;
	vk::VkQueue									getSparseQueue						(void) const;
	vk::Allocator&								getDefaultAllocator					(void) const;
	bool										contextSupports						(const deUint32 majorNum, const deUint32 minorNum, const deUint32 patchNum) const;
	bool										contextSupports						(const vk::ApiVersion version) const;
	bool										contextSupports						(const deUint32 requiredApiVersionBits) const;
	bool										requireDeviceFunctionality			(const std::string& required) const;
	bool										requireInstanceFunctionality		(const std::string& required) const;
	bool										requireDeviceCoreFeature			(const DeviceCoreFeature requiredDeviceCoreFeature);

	void*										getInstanceProcAddr					();

	bool										isBufferDeviceAddressSupported						(void) const;

	bool										resultSetOnValidation			() const		{ return m_resultSetOnValidation;	}
	void										resultSetOnValidation			(bool value)	{ m_resultSetOnValidation = value;	}

protected:
	tcu::TestContext&							m_testCtx;
	const vk::PlatformInterface&				m_platformInterface;
	vk::BinaryCollection&						m_progCollection;

	const de::UniquePtr<DefaultDevice>			m_device;
	const de::UniquePtr<vk::Allocator>			m_allocator;

	bool										m_resultSetOnValidation;

private:
												Context								(const Context&); // Not allowed
	Context&									operator=							(const Context&); // Not allowed
};

class TestInstance;

class TestCase : public tcu::TestCase
{
public:
							TestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description);
							TestCase		(tcu::TestContext& testCtx, tcu::TestNodeType type, const std::string& name, const std::string& description);
	virtual					~TestCase		(void) {}

	virtual void			delayedInit		(void); // non-const init called after checkSupport but before initPrograms
	virtual void			initPrograms	(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance	(Context& context) const = 0;
	virtual void			checkSupport	(Context& context) const;

	IterateResult			iterate			(void) { DE_ASSERT(false); return STOP; } // Deprecated in this module
};

class TestInstance
{
public:
								TestInstance	(Context& context) : m_context(context) {}
	virtual						~TestInstance	(void) {}

	virtual tcu::TestStatus		iterate			(void) = 0;

protected:
	Context&					m_context;

private:
								TestInstance	(const TestInstance&);
	TestInstance&				operator=		(const TestInstance&);
};

inline TestCase::TestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description)
	: tcu::TestCase(testCtx, name.c_str(), description.c_str())
{
}

inline TestCase::TestCase (tcu::TestContext& testCtx, tcu::TestNodeType type, const std::string& name, const std::string& description)
	: tcu::TestCase(testCtx, type, name.c_str(), description.c_str())
{
}

void collectAndReportDebugMessages(vk::DebugReportRecorder &debugReportRecorder, Context& context);

} // vkt

#endif // _VKTTESTCASE_HPP
