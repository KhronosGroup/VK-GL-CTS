/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Extension duplicates tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkPlatform.hpp"
#include "vkCmdUtil.hpp"
#include "tcuCommandLine.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktApiExtensionDuplicatesTests.hpp"

#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <iostream>

namespace vkt
{
namespace api
{
namespace
{
using namespace vk;

namespace ut
{
std::set<const char*> distinct (const std::vector<const char*>& src)
{
	std::set<const char*> result;
	for (const char* const p : src) {
		result.insert(p);
	}
	return result;
}

struct StringDuplicator
{
			StringDuplicator	(const std::vector<const char*>& src)
				: m_source(distinct(src)), m_strings() {}
	auto	duplicatePointers	() -> std::vector<const char*>;
	// NOTE: Use carefully, realtime counterparts for returned pointers are held in m_strings
	auto	duplicateStrings	() -> std::vector<const char*>;
	auto	getInputCount		() const -> typename std::vector<const char*>::size_type { return m_source.size(); }
private:
	const std::set<const char*>	m_source;
	std::vector<std::string>	m_strings;
};

std::vector<const char*> StringDuplicator::duplicatePointers ()
{
	deUint32					i{};
	std::vector<const char*>	r{};

	for (const char* const p : m_source)
	{
		if ((i % 2) == 0) {
			r.push_back(p);
			r.push_back(p);
		} else if ((i % 3) == 0) {
			r.push_back(p);
			r.push_back(p);
			r.push_back(p);
		} else {
			r.push_back(p);
			r.push_back(p);
			r.push_back(p);
			r.push_back(p);
		}

		i = i + 1u;
	}

	return r;
}
std::vector<const char*> StringDuplicator::duplicateStrings ()
{
	deUint32					i{};
	std::vector<const char*>	r{};

	m_strings.clear();

	for (const char* p : m_source)
	{
		if ((i % 2) == 0) {
			m_strings.push_back(std::string(p));
			m_strings.push_back(std::string(p));
		} else if ((i % 3) == 0) {
			m_strings.push_back(std::string(p));
			m_strings.push_back(std::string(p));
			m_strings.push_back(std::string(p));
		} else {
			m_strings.push_back(std::string(p));
			m_strings.push_back(std::string(p));
			m_strings.push_back(std::string(p));
			m_strings.push_back(std::string(p));
		}

		i = i + 1u;
	}

	for (std::string& s : m_strings)
	{
		r.push_back(s.c_str());
	}

	return r;
}
} // namespace ut

class InstanceExtensionDuplicatesInstance : public TestInstance
{
public:
							InstanceExtensionDuplicatesInstance	(Context&	ctx,
																 bool		byPointersOrNames)
								: TestInstance			(ctx)
								, m_byPointersOrNames	(byPointersOrNames) {}
	virtual tcu::TestStatus	iterate								(void) override;
private:
	const bool	m_byPointersOrNames;
};

class DeviceExtensionDuplicatesInstance : public TestInstance
{
public:
							DeviceExtensionDuplicatesInstance	(Context&	ctx,
																 bool		byPointersOrNames)
								: TestInstance			(ctx)
								, m_byPointersOrNames	(byPointersOrNames) {}
	virtual tcu::TestStatus	iterate								(void) override;
private:
	const bool	m_byPointersOrNames;
};

class ExtensionDuplicatesCase : public TestCase
{
public:
							ExtensionDuplicatesCase	(tcu::TestContext&		testCtx,
													 const std::string&		name,
													 bool					instanceOrDevice,
													 bool					byPointersOrNames)
								: TestCase				(testCtx, name, std::string())
								, m_instanceOrDevice	(instanceOrDevice)
								, m_byPointersOrNames	(byPointersOrNames) {}
	virtual TestInstance*	createInstance			(Context&				ctx) const override
	{
		if (m_instanceOrDevice)
			return new InstanceExtensionDuplicatesInstance(ctx, m_byPointersOrNames);
		return new DeviceExtensionDuplicatesInstance(ctx, m_byPointersOrNames);
	}

private:
	const bool	m_instanceOrDevice;
	const bool	m_byPointersOrNames;
};

tcu::TestStatus InstanceExtensionDuplicatesInstance::iterate (void)
{
	const vk::PlatformInterface&				vkp						= m_context.getPlatformInterface();
	const tcu::CommandLine&						cmd						= m_context.getTestContext().getCommandLine();
	const std::vector<VkExtensionProperties>	availableExtensions		= enumerateInstanceExtensionProperties(vkp, nullptr);

	if (0u == availableExtensions.size())
	{
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Unable to perform test due to empty instance extension list");
	}

	std::vector<std::string>					availableExtensionNames	(availableExtensions.size());
	std::vector<const char*>					enabledExtensions		(availableExtensions.size());

	std::transform(availableExtensions.begin(), availableExtensions.end(), availableExtensionNames.begin(),
				   [](const VkExtensionProperties& props) { return std::string(props.extensionName); });
	std::transform(availableExtensionNames.begin(), availableExtensionNames.end(), enabledExtensions.begin(),
				   [](const std::string& ext) { return ext.c_str(); } );

	ut::StringDuplicator						duplicator				(enabledExtensions);
	const std::vector<const char*>				duplicatedExtensions	= m_byPointersOrNames
																			? duplicator.duplicatePointers()
																			: duplicator.duplicateStrings();
	const deUint32								duplicatedExtensionCnt	= static_cast<deUint32>(duplicatedExtensions.size());

	const VkApplicationInfo		applicationInfo
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,		// VkStructureType	sType;
		nullptr,								// const void*		pNext;
		"extension_duplicates_instance",		// const char*		pApplicationName;
		VK_API_VERSION_1_0,						// uint32_t			applicationVersion;
		"extension_duplicates_instance_engine",	// const char*		pEngineName;
		VK_API_VERSION_1_0,						// uint32_t			engineVersion;
		m_context.getUsedApiVersion()			// uint32_t			apiVersion;
	};

	const VkInstanceCreateInfo	instanceCreateInfo
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,	// VkStructureType				sType;
		nullptr,								// const void*					pNext;
		VkInstanceCreateFlags(0),				// VkInstanceCreateFlags		flags;
		&applicationInfo,						// const VkApplicationInfo*		pApplicationInfo;
		0u,										// uint32_t						enabledLayerCount;
		nullptr,								// const char* const*			ppEnabledLayerNames;
		duplicatedExtensionCnt,					// uint32_t						enabledExtensionCount;
		duplicatedExtensions.data()				// const char* const*			ppEnabledExtensionNames;
	};


	UncheckedInstance	uncheckedInstance;
	const VkResult res = createUncheckedInstance(m_context, &instanceCreateInfo, nullptr, &uncheckedInstance, cmd.isValidationEnabled());

	auto failMessage = [&]() -> std::string
	{
		std::ostringstream os;
		os << "vkCreateInstance returned " << getResultName(res);
		os.flush();
		return os.str();
	};

	auto passMessage = [&]() -> std::string
	{
		std::ostringstream os;
		os << "Created " << duplicatedExtensionCnt << " duplicates of " << duplicator.getInputCount() << " extensions";
		os.flush();
		return os.str();
	};

	return (VK_SUCCESS == res) ? tcu::TestStatus::pass(passMessage()) : tcu::TestStatus::fail(failMessage());
}

tcu::TestStatus DeviceExtensionDuplicatesInstance::iterate (void)
{
	const InstanceInterface&		vki	= m_context.getInstanceInterface();
	const DeviceInterface&			vkd	= m_context.getDeviceInterface();
	const VkPhysicalDevice			phd = m_context.getPhysicalDevice();
	const deUint32					idx	= m_context.getUniversalQueueFamilyIndex();
	const tcu::CommandLine&			cmd	= m_context.getTestContext().getCommandLine();
	const float						qpr	= 1.0f;
	ut::StringDuplicator			sd	(m_context.getDeviceCreationExtensions());

	const std::vector<const char*>	dup	= m_byPointersOrNames
											? sd.duplicatePointers()
											: sd.duplicateStrings();
	const deUint32					cnt	= static_cast<deUint32>(dup.size());
	if (0u == cnt)
	{
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Unable to perform test due to empty device extension list");
	}

	const VkDeviceQueueCreateInfo	queueCreateInfo
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType			sType;
		nullptr,									// const void*				pNext;
		VkDeviceQueueCreateFlags(0),				// VkDeviceQueueCreateFlags	flags;
		idx,										// uint32_t					queueFamilyIndex;
		1u,											// uint32_t					queueCount;
		&qpr										// const float*				pQueuePriorities;
	};

	const VkDeviceCreateInfo		deviceCreateInfo
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,		// VkStructureType					sType;
		nullptr,									// const void*						pNext;
		VkDeviceCreateFlags(0),						// VkDeviceCreateFlags				flags;
		1u,											// uint32_t							queueCreateInfoCount;
		&queueCreateInfo,							// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,											// uint32_t							enabledLayerCount;
		nullptr,									// const char* const*				ppEnabledLayerNames;
		cnt,										// uint32_t							enabledExtensionCount;
		dup.data(),									// const char* const*				ppEnabledExtensionNames;
		nullptr										// const VkPhysicalDeviceFeatures*	pEnabledFeatures;

	};

	VkDevice device = VK_NULL_HANDLE;
	const VkResult res = createUncheckedDevice(cmd.isValidationEnabled(), vki, phd, &deviceCreateInfo, nullptr, &device);
	if (VK_SUCCESS == res && VK_NULL_HANDLE != device)
	{
		vkd.destroyDevice(device, nullptr);
	}

	auto failMessage = [&]() -> std::string
	{
		std::ostringstream os;
		os << "vkCreateDevice returned " << getResultName(res);
		os.flush();
		return os.str();
	};

	auto passMessage = [&]() -> std::string
	{
		std::ostringstream os;
		os << "Created " << cnt << " duplicates of " << sd.getInputCount() << " extensions";
		os.flush();
		return os.str();
	};

	return (VK_SUCCESS == res) ? tcu::TestStatus::pass(passMessage()) : tcu::TestStatus::fail(failMessage());
}

} // unnamed namespace

tcu::TestCaseGroup* createExtensionDuplicatesTests (tcu::TestContext& testCtx)
{
	typedef std::pair<const char*, bool>	item_t;
	item_t const types[]
	{
		{ "instance",	true	},
		{ "device",		false	}
	};

	item_t const methods[]
	{
		{ "by_pointers",	true	},
		{ "by_names",		false	}
	};

	de::MovePtr<tcu::TestCaseGroup>	rootGroup(new tcu::TestCaseGroup(testCtx, "extension_duplicates", "Verifies that we can create a device or an instance with duplicate extensions"));
	for (const item_t& type : types)
	{
		de::MovePtr<tcu::TestCaseGroup>	typeGroup(new tcu::TestCaseGroup(testCtx, type.first, ""));
		for (const item_t& meth : methods)
		{
			typeGroup->addChild(new ExtensionDuplicatesCase(testCtx, meth.first, type.second, meth.second));
		}
		rootGroup->addChild(typeGroup.release());
	}

	return rootGroup.release();
}

} // api
} // vkt
