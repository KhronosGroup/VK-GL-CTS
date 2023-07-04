/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief VK_EXT_device_fault extension tests.
 *//*--------------------------------------------------------------------*/

#include "vktPostmortemDeviceFaultTests.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "deStringUtil.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include <functional>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#define ARRAY_LENGTH(a_) std::extent<decltype(a_)>::value

#ifndef VK_EXT_DEVICE_FAULT_EXTENSION_NAME
	#define VK_EXT_DEVICE_FAULT_EXTENSION_NAME "VK_EXT_device_fault"
#else
	// This should never have happened
	// static_assert(false, "Trying to redefine VK_EXT_DEVICE_FAULT_EXTENSION_NAME");
#endif

namespace vkt
{
namespace postmortem
{
namespace
{
using namespace vk;
using namespace tcu;

enum class TestType
{
	Fake,
	Real,
	CustomDevice
};

struct TestParams
{
	TestType	type;
};

class DeviceFaultCase : public TestCase
{
public:
							DeviceFaultCase		(TestContext&		testCtx,
												 const std::string&	name,
												 const TestParams&	params)
								: TestCase	(testCtx, name, std::string())
								, m_params	(params) {}
	virtual					~DeviceFaultCase	() = default;
	virtual TestInstance*	createInstance		(Context&			context) const override;
	virtual void			checkSupport		(Context&			context) const override;
private:
	const TestParams	m_params;
};

class DeviceFaultInstance : public TestInstance
{
public:
							DeviceFaultInstance	(Context& context, const TestParams& params)
								: TestInstance				(context)
								, m_params					(params) {}
	virtual					~DeviceFaultInstance() =  default;

	virtual TestStatus		iterate				(void) override;
	void					log					(const std::vector<VkDeviceFaultAddressInfoEXT>&	addressInfos,
												 const std::vector<VkDeviceFaultVendorInfoEXT>&		vendorInfos,
												 const std::vector<deUint8>&						vendorBinaryData) const;
private:
	const TestParams	m_params;
};

class DeviceFaultCustomInstance : public TestInstance
{
public:
							DeviceFaultCustomInstance	(Context& context)
								: TestInstance			(context) {}
	virtual					~DeviceFaultCustomInstance	() =  default;

	virtual TestStatus		iterate						(void) override;
};

TestInstance* DeviceFaultCase::createInstance (Context& context) const
{
	TestInstance* instance = nullptr;
	if (m_params.type == TestType::CustomDevice)
		instance = new DeviceFaultCustomInstance(context);
	else instance = new DeviceFaultInstance(context, m_params);
	return instance;
}

class CustomDevice
{
	Move<VkDevice>				m_logicalDevice;

public:
	CustomDevice (Context& context)
	{
		const bool								useValidation		= context.getTestContext().getCommandLine().isValidationEnabled();
		const PlatformInterface&				platformInterface	= context.getPlatformInterface();
		const VkInstance						instance			= context.getInstance();
		const InstanceInterface&				instanceInterface	= context.getInstanceInterface();
		const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
		const deUint32							queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
		const float								queuePriority		= 1.0f;

		const VkDeviceQueueCreateInfo			queueCreateInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,				// VkStructureType					sType;
			nullptr,												// const void*						pNext;
			0,														// VkDeviceQueueCreateFlags			flags;
			queueFamilyIndex,										// uint32_t							queueFamilyIndex;
			1u,														// uint32_t							queueCount;
			&queuePriority											// const float*						pQueuePriorities;
		};

		VkPhysicalDeviceFaultFeaturesEXT		deviceFaultFeatures
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT,	// VkStructureType					sType;
			nullptr,												// void*							pNext;
			VK_TRUE,												// VkBool32							deviceFault;
			VK_TRUE													// VkBool32							deviceFaultVendorBinary;
		};

		VkPhysicalDeviceFeatures2				deviceFeatures2
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,			// VkStructureType					sType;
			&deviceFaultFeatures,									// void*							pNext;
			{ /* zeroed automatically since c++11 */ }				// VkPhysicalDeviceFeatures			features;
		};
		instanceInterface.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

		const VkDeviceCreateInfo				deviceCreateInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,					// VkStructureType					sType;
			&deviceFeatures2,										// const void*						pNext;
			0u,														// VkDeviceCreateFlags				flags;
			1,														// deUint32							queueCreateInfoCount;
			&queueCreateInfo,										// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
			0u,														// deUint32							enabledLayerCount;
			nullptr,												// const char* const*				ppEnabledLayerNames;
			0u,														// deUint32							enabledExtensionCount;
			nullptr,												// const char* const*				ppEnabledExtensionNames;
			nullptr													// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		m_logicalDevice = createCustomDevice(useValidation, platformInterface, instance, instanceInterface, physicalDevice, &deviceCreateInfo);
	}

	VkDevice	getDevice () const { return *m_logicalDevice;	}
};

class FakeInstanceInterface : public InstanceDriver
{
	const InstanceInterface&	m_instanceInterface;
public:

	FakeInstanceInterface (Context& ctx)
		: InstanceDriver		(ctx.getPlatformInterface(), ctx.getInstance())
		, m_instanceInterface	(ctx.getInstanceInterface()) {}

	virtual void getPhysicalDeviceFeatures2 (VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) const override
	{
		DE_ASSERT(pFeatures);

		InstanceDriver::getPhysicalDeviceFeatures(physicalDevice, &pFeatures->features);

		auto pBaseStructure = reinterpret_cast<VkBaseOutStructure*>(pFeatures)->pNext;
		while (pBaseStructure)
		{
			if (pBaseStructure->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT)
			{
				const VkPhysicalDeviceFaultFeaturesEXT deviceFaultFeatures
				{
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT,	// VkStructureType	sType;
					nullptr,												// void*			pNext;
					VK_TRUE,												// VkBool32		deviceFault;
					VK_TRUE													// VkBool32		deviceFaultVendorBinary;
				};
				*(VkPhysicalDeviceFaultFeaturesEXT*)pBaseStructure = deviceFaultFeatures;
				break;
			}
			pBaseStructure = pBaseStructure->pNext;
		}
	}
};

class FakeDeviceInterface : public DeviceDriver
{
public:
	FakeDeviceInterface (Context& ctx)
		: DeviceDriver(ctx.getPlatformInterface(), ctx.getInstance(), ctx.getDevice(), ctx.getUsedApiVersion()) {}

	struct Header : VkDeviceFaultVendorBinaryHeaderVersionOneEXT
	{
		char applicationName[32];
		char engineName[32];
		Header() {
			headerSize				= sizeof(VkDeviceFaultVendorBinaryHeaderVersionOneEXT);
			headerVersion			= VK_DEVICE_FAULT_VENDOR_BINARY_HEADER_VERSION_ONE_EXT;
			vendorID				= 0x9876;
			deviceID				= 0x5432;
			driverVersion			= VK_MAKE_VERSION(3,4,5);
			deMemcpy(pipelineCacheUUID, this, sizeof(pipelineCacheUUID));
			applicationNameOffset	= deUint32(sizeof(VkDeviceFaultVendorBinaryHeaderVersionOneEXT));
			applicationVersion		= VK_MAKE_API_VERSION(1,7,3,11);
			engineNameOffset		= deUint32(applicationNameOffset + sizeof(applicationName));

			strcpy(applicationName, "application.exe");
			strcpy(engineName, "driver.so.3.4.5");
		}
	};

	virtual VkResult getDeviceFaultInfoEXT (VkDevice, VkDeviceFaultCountsEXT* pFaultCounts, VkDeviceFaultInfoEXT* pFaultInfo) const override
	{
		static std::vector<VkDeviceFaultAddressInfoEXT>	addressInfos;
		static std::vector<VkDeviceFaultVendorInfoEXT>	vendorInfos;
		static VkDeviceFaultAddressTypeEXT				addressTypes[]
		{
			VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT,
			VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT,
			VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT,
			VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT,
			VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT,
			VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT,
			VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT,
		};
		static VkDeviceSize								addressPrecisions[]
		{
			2, 4, 8, 16
		};
		static deUint64									vendorFaultCodes[]
		{
			0x11223344, 0x22334455, 0xAABBCCDD, 0xCCDDEEFF
		};
		static Header									vendorBinaryData;

		if (DE_NULL == pFaultInfo)
		{
			if (DE_NULL == pFaultCounts) return VK_ERROR_UNKNOWN;

			DE_ASSERT(pFaultCounts->sType == VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT);
			DE_ASSERT(pFaultCounts->pNext == nullptr);

			pFaultCounts->vendorBinarySize = sizeof(Header);
			pFaultCounts->vendorInfoCount = 2;
			pFaultCounts->addressInfoCount = 2;
		}
		else
		{
			DE_ASSERT(pFaultCounts);
			DE_ASSERT(pFaultCounts->sType == VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT);
			DE_ASSERT(pFaultCounts->pNext == nullptr);
			DE_ASSERT(pFaultInfo->sType == VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT);
			DE_ASSERT(pFaultInfo->pNext == nullptr);

			if (pFaultCounts->addressInfoCount && pFaultInfo->pAddressInfos)
			{
				VkDeviceAddress	deviceAddress = 1024;
				addressInfos.resize(pFaultCounts->addressInfoCount);
				for (deUint32 i = 0; i < pFaultCounts->addressInfoCount; ++i)
				{
					VkDeviceFaultAddressInfoEXT& info = addressInfos[i];
					info.addressType		= addressTypes[ i % ARRAY_LENGTH(addressTypes) ];
					info.addressPrecision	= addressPrecisions[ i % ARRAY_LENGTH(addressPrecisions) ];
					info.reportedAddress	= deviceAddress;
					deviceAddress			<<= 1;

					pFaultInfo->pAddressInfos[i] = info;
				}
			}

			if (pFaultCounts->vendorInfoCount && pFaultInfo->pVendorInfos)
			{
				vendorInfos.resize(pFaultCounts->vendorInfoCount);
				for (deUint32 i = 0; i < pFaultCounts->vendorInfoCount; ++i)
				{
					VkDeviceFaultVendorInfoEXT& info = vendorInfos[i];
					info.vendorFaultCode = vendorFaultCodes[ i % ARRAY_LENGTH(vendorFaultCodes) ];
					info.vendorFaultData = (i + 1) % ARRAY_LENGTH(vendorFaultCodes);
					deMemset(info.description, 0, sizeof(info.description));

					std::stringstream s;
					s << "VendorFaultDescription" << info.vendorFaultData;
					s.sync();
					const auto& str = s.str();
					deMemcpy(info.description, str.c_str(), str.length());

					pFaultInfo->pVendorInfos[i] = info;
				}
			}

			if (pFaultCounts->vendorBinarySize && pFaultInfo->pVendorBinaryData)
			{
				DE_ASSERT(pFaultCounts->vendorBinarySize >= sizeof(VkDeviceFaultVendorBinaryHeaderVersionOneEXT));
				deMemcpy(pFaultInfo->pVendorBinaryData, &vendorBinaryData,
						 deMaxu32(sizeof(Header), deUint32(pFaultCounts->vendorBinarySize)));
			}
		}

		return VK_SUCCESS;
	}
};

class FakeContext
{
	FakeDeviceInterface		m_deviceInterface;
	FakeInstanceInterface	m_instanceInterface;
public:

	FakeContext (Context& ctx)
		: m_deviceInterface		(ctx)
		, m_instanceInterface	(ctx) {}

	const DeviceInterface&		getDeviceInterface () const { return m_deviceInterface; }
	const InstanceInterface&	getInstanceInterface () const { return m_instanceInterface; }
};

void DeviceFaultCase::checkSupport (Context& context) const
{
	FakeContext					fakeContext			(context);
	VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
	const InstanceInterface&	instanceInterface	= (m_params.type == TestType::Real) ? context.getInstanceInterface() : fakeContext.getInstanceInterface();

	context.requireInstanceFunctionality(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	if (m_params.type == TestType::Real)
	{
		context.requireDeviceFunctionality(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
	}

	VkPhysicalDeviceFaultFeaturesEXT deviceFaultFeatures{};
	deviceFaultFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;

	VkPhysicalDeviceFeatures2		deviceFeatures2{};
	deviceFeatures2.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext		= &deviceFaultFeatures;

	instanceInterface.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

	if (VK_FALSE == deviceFaultFeatures.deviceFault)
		TCU_THROW(NotSupportedError, "VK_EXT_device_fault extension is not supported by device");
}

TestStatus DeviceFaultCustomInstance::iterate (void)
{
	CustomDevice		customDevice	(m_context);
	const VkDevice		device			= customDevice.getDevice();
	return (device != DE_NULL) ? TestStatus::pass("") : TestStatus::fail("");
}

void DeviceFaultInstance::log (const std::vector<VkDeviceFaultAddressInfoEXT>&	addressInfos,
							   const std::vector<VkDeviceFaultVendorInfoEXT>&	vendorInfos,
							   const std::vector<deUint8>&						vendorBinaryData) const
{
	const char*	nl = "\n";
	deUint32	cnt = 0;
	TestLog&	log = m_context.getTestContext().getLog();

	if (addressInfos.size())
	{
		log << TestLog::Section("addressInfos", "");
		auto msg = log << TestLog::Message;
		cnt = 0;
		for (const auto& addressInfo : addressInfos)
		{
			if (cnt++) msg << nl;
			msg << addressInfo;
		}
		msg << TestLog::EndMessage << TestLog::EndSection;
	}

	if (vendorInfos.size())
	{
		log << TestLog::Section("vendorInfos", "");
		auto msg = log << TestLog::Message;
		cnt = 0;
		for (const auto& vendorInfo : vendorInfos)
		{
			if (cnt++) msg << nl;
			msg << vendorInfo;
		}
		msg << TestLog::EndMessage << TestLog::EndSection;
	}

	if (vendorBinaryData.size())
	{
		DE_ASSERT(vendorBinaryData.size() >= sizeof(VkDeviceFaultVendorBinaryHeaderVersionOneEXT));

		log << TestLog::Section("vendorBinaryData", "");
		auto msg = log << TestLog::Message;
		auto pHeader = reinterpret_cast<VkDeviceFaultVendorBinaryHeaderVersionOneEXT const*>(vendorBinaryData.data());
		msg << *pHeader;
		msg << TestLog::EndMessage << TestLog::EndSection;
	}
}

TestStatus DeviceFaultInstance::iterate (void)
{
	FakeContext					fakeContext			(m_context);
	const VkDevice				device				= m_context.getDevice();
	const VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();
	const DeviceInterface&		deviceInterface		= (m_params.type == TestType::Fake) ? fakeContext.getDeviceInterface() : m_context.getDeviceInterface();
	const InstanceInterface&	instanceInterface	= (m_params.type == TestType::Fake) ? fakeContext.getInstanceInterface() : m_context.getInstanceInterface();

	VkDeviceFaultCountsEXT	fc{};
	fc.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT;
	fc.pNext = nullptr;
	deviceInterface.getDeviceFaultInfoEXT(device, &fc, nullptr);

	const deUint32 vendorBinarySize = std::min(deUint32(fc.vendorBinarySize), std::numeric_limits<deUint32>::max());

	VkPhysicalDeviceFaultFeaturesEXT	deviceFaultFeatures{};
	deviceFaultFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;

	VkPhysicalDeviceFeatures2			deviceFeatures2{};
	deviceFeatures2.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext		= &deviceFaultFeatures;

	instanceInterface.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

	fc.vendorBinarySize = deviceFaultFeatures.deviceFaultVendorBinary ? vendorBinarySize : 0;

	std::vector<VkDeviceFaultAddressInfoEXT>	addressInfos	(fc.addressInfoCount);
	std::vector<VkDeviceFaultVendorInfoEXT>		vendorInfos		(fc.vendorInfoCount);
	std::vector<deUint8>						vendorBinaryData(vendorBinarySize);

	VkDeviceFaultInfoEXT fi{};
	fi.sType				= VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT;
	fi.pNext				= nullptr;
	fi.pAddressInfos		= addressInfos.data();
	fi.pVendorInfos			= vendorInfos.data();
	fi.pVendorBinaryData	= deviceFaultFeatures.deviceFaultVendorBinary ? vendorBinaryData.data() : nullptr;

	const VkResult result = deviceInterface.getDeviceFaultInfoEXT(device, &fc, &fi);

	log(addressInfos, vendorInfos, vendorBinaryData);

	return (result == VK_SUCCESS) ? TestStatus::pass("") : TestStatus::fail("");
}

} // unnamed

tcu::TestCaseGroup*	createDeviceFaultTests (tcu::TestContext& testCtx)
{
	TestParams p;
	struct {
		TestType	type;
		const char*	name;
	} const types[] = { { TestType::Real, "real" }, { TestType::Fake, "fake" }, { TestType::CustomDevice, "custom_device" } };

	auto rootGroup = new TestCaseGroup(testCtx, "device_fault", "VK_EXT_device_fault extension tests.");
	for (const auto& type : types)
	{
		p.type = type.type;
		rootGroup->addChild(new DeviceFaultCase(testCtx, type.name, p));
	}
	return rootGroup;
}

} // postmortem
} // vkt
