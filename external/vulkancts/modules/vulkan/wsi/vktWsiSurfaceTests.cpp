/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief VkSurface Tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiSurfaceTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktNativeObjectsUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

namespace vk
{

inline bool operator!= (const VkSurfaceFormatKHR& a, const VkSurfaceFormatKHR& b)
{
	return (a.format != b.format) || (a.colorSpace != b.colorSpace);
}

inline bool operator== (const VkSurfaceFormatKHR& a, const VkSurfaceFormatKHR& b)
{
	return !(a != b);
}

inline bool operator!= (const VkExtent2D& a, const VkExtent2D& b)
{
	return (a.width != b.width) || (a.height != b.height);
}

inline bool operator!= (const VkSurfaceCapabilitiesKHR& a, const VkSurfaceCapabilitiesKHR& b)
{
	return (a.minImageCount				!= b.minImageCount)				||
		   (a.maxImageCount				!= b.maxImageCount)				||
		   (a.currentExtent				!= b.currentExtent)				||
		   (a.minImageExtent			!= b.minImageExtent)			||
		   (a.maxImageExtent			!= b.maxImageExtent)			||
		   (a.maxImageArrayLayers		!= b.maxImageArrayLayers)		||
		   (a.supportedTransforms		!= b.supportedTransforms)		||
		   (a.currentTransform			!= b.currentTransform)			||
		   (a.supportedCompositeAlpha	!= b.supportedCompositeAlpha)	||
		   (a.supportedUsageFlags		!= b.supportedUsageFlags);
}

} // vk

namespace vkt
{
namespace wsi
{

namespace
{

using namespace vk;
using namespace vk::wsi;

using tcu::TestLog;
using tcu::Maybe;
using tcu::UVec2;

using de::MovePtr;
using de::UniquePtr;

using std::string;
using std::vector;

enum
{
	SURFACE_EXTENT_DETERMINED_BY_SWAPCHAIN_MAGIC	= 0xffffffff
};

enum
{
	GUARD_SIZE										= 0x20,			//!< Number of bytes to check
	GUARD_VALUE										= 0xcd,			//!< Data pattern
};

template<typename T>
class CheckIncompleteResult
{
public:
	virtual			~CheckIncompleteResult	(void) {}
	virtual void	getResult				(const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkSurfaceKHR surface, T* data) = 0;

	void operator() (tcu::ResultCollector&		results,
					 const InstanceInterface&	vki,
					 const VkPhysicalDevice		physDevice,
					 const VkSurfaceKHR			surface,
					 const std::size_t			expectedCompleteSize)
	{
		if (expectedCompleteSize == 0)
			return;

		vector<T>		outputData	(expectedCompleteSize);
		const deUint32	usedSize	= static_cast<deUint32>(expectedCompleteSize / 3);

		ValidateQueryBits::fillBits(outputData.begin(), outputData.end());	// unused entries should have this pattern intact
		m_count		= usedSize;
		m_result	= VK_SUCCESS;

		getResult(vki, physDevice, surface, &outputData[0]);				// update m_count and m_result

		if (m_count != usedSize || m_result != VK_INCOMPLETE || !ValidateQueryBits::checkBits(outputData.begin() + m_count, outputData.end()))
			results.fail("Query didn't return VK_INCOMPLETE");
	}

protected:
	deUint32	m_count;
	VkResult	m_result;
};

struct CheckPhysicalDeviceSurfaceFormatsIncompleteResult : public CheckIncompleteResult<VkSurfaceFormatKHR>
{
	void getResult (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkSurfaceKHR surface, VkSurfaceFormatKHR* data)
	{
		m_result = vki.getPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &m_count, data);
	}
};

struct CheckPhysicalDeviceSurfacePresentModesIncompleteResult : public CheckIncompleteResult<VkPresentModeKHR>
{
	void getResult (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkSurfaceKHR surface, VkPresentModeKHR* data)
	{
		m_result = vki.getPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &m_count, data);
	}
};

typedef vector<VkExtensionProperties> Extensions;

CustomInstance createInstanceWithWsi (Context&						context,
									  Type							wsiType,
									  const vector<string>			extraExtensions,
									  const VkAllocationCallbacks*	pAllocator	= DE_NULL)
{
	const deUint32	version		= context.getUsedApiVersion();
	vector<string>	extensions	= extraExtensions;

	extensions.push_back("VK_KHR_surface");
	extensions.push_back(getExtensionName(wsiType));
	if (isDisplaySurface(wsiType))
		extensions.push_back("VK_KHR_display");

	vector<string>	instanceExtensions;
	for (const auto& ext : extensions)
	{
		if (!context.isInstanceFunctionalitySupported(ext))
			TCU_THROW(NotSupportedError, (ext + " is not supported").c_str());

		if (!isCoreInstanceExtension(version, ext))
			instanceExtensions.push_back(ext);
	}

	return vkt::createCustomInstanceWithExtensions(context, instanceExtensions, pAllocator);
}

struct InstanceHelper
{
	const vector<VkExtensionProperties>	supportedExtensions;
	CustomInstance						instance;
	const InstanceDriver&				vki;

	InstanceHelper (Context& context, Type wsiType, const VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(),
																	  DE_NULL))
		, instance				(createInstanceWithWsi(context,
													   wsiType,
													   vector<string>(),
													   pAllocator))
		, vki					(instance.getDriver())
	{}

	InstanceHelper (Context& context, Type wsiType, const vector<string>& extensions, const VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(),
																	  DE_NULL))
		, instance				(createInstanceWithWsi(context,
													   wsiType,
													   extensions,
													   pAllocator))
		, vki					(instance.getDriver())
	{}
};

tcu::TestStatus createSurfaceTest (Context& context, Type wsiType)
{
	const InstanceHelper		instHelper	(context, wsiType);
	const NativeObjects			native		(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>	surface		(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));

	return tcu::TestStatus::pass("Creating surface succeeded");
}

tcu::TestStatus querySurfaceCounterTest (Context& context, Type wsiType)
{
	const InstanceHelper			instHelper		(context, wsiType);
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vk::InstanceInterface&	vki				= context.getInstanceInterface();
	const vk::VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const bool						isDisplay		= isDisplaySurface(wsiType);

	if (!isInstanceExtensionSupported(context.getUsedApiVersion(), context.getInstanceExtensions(), "VK_EXT_display_surface_counter"))
		TCU_THROW(NotSupportedError, "VK_EXT_display_surface_counter not supported");

	const vk::VkSurfaceCapabilities2EXT	capsExt = getPhysicalDeviceSurfaceCapabilities2EXT	(vki, physicalDevice, surface.get());
	const vk::VkSurfaceCapabilitiesKHR	capsKhr = getPhysicalDeviceSurfaceCapabilities		(vki, physicalDevice, surface.get());

	if (!sameSurfaceCapabilities(capsKhr, capsExt))
	{
		return tcu::TestStatus::fail("KHR and EXT surface capabilities do not match");
	}

	if (!isDisplay && capsExt.supportedSurfaceCounters != 0)
	{
		return tcu::TestStatus::fail("supportedSurfaceCounters nonzero (" + de::toString(capsExt.supportedSurfaceCounters) + ") for non-display surface");
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus createSurfaceCustomAllocatorTest (Context& context, Type wsiType)
{
	AllocationCallbackRecorder	allocationRecorder	(getSystemAllocator());
	tcu::TestLog&				log					= context.getTestContext().getLog();

	{
		const InstanceHelper		instHelper	(context, wsiType, allocationRecorder.getCallbacks());
		const NativeObjects			native		(context, instHelper.supportedExtensions, wsiType);
		const Unique<VkSurfaceKHR>	surface		(createSurface(instHelper.vki,
															   instHelper.instance,
															   wsiType,
															   native.getDisplay(),
															   native.getWindow(),
															   context.getTestContext().getCommandLine(),
															   allocationRecorder.getCallbacks()));

		if (!validateAndLog(log,
							allocationRecorder,
							(1u<<VK_SYSTEM_ALLOCATION_SCOPE_OBJECT)		|
							(1u<<VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE)))
			return tcu::TestStatus::fail("Detected invalid system allocation callback");
	}

	if (!validateAndLog(log, allocationRecorder, 0u))
		return tcu::TestStatus::fail("Detected invalid system allocation callback");

	if (allocationRecorder.getRecordsBegin() == allocationRecorder.getRecordsEnd())
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Allocation callbacks were not used");
	else
		return tcu::TestStatus::pass("Creating surface succeeded using custom allocator");
}

tcu::TestStatus createSurfaceSimulateOOMTest (Context& context, Type wsiType)
{
	tcu::TestLog&	log	= context.getTestContext().getLog();

	for (deUint32 numPassingAllocs = 0; numPassingAllocs <= 1024u; ++numPassingAllocs)
	{
		AllocationCallbackRecorder	allocationRecorder	(getSystemAllocator());
		DeterministicFailAllocator	failingAllocator	(allocationRecorder.getCallbacks(),
														 DeterministicFailAllocator::MODE_DO_NOT_COUNT,
														 0);
		bool						gotOOM				= false;

		log << TestLog::Message << "Testing with " << numPassingAllocs << " first allocations succeeding" << TestLog::EndMessage;

		try
		{
			const InstanceHelper		instHelper	(context, wsiType, failingAllocator.getCallbacks());

			// OOM is not simulated for VkInstance as we don't want to spend time
			// testing OOM paths inside instance creation.
			failingAllocator.reset(DeterministicFailAllocator::MODE_COUNT_AND_FAIL, numPassingAllocs);

			const NativeObjects			native		(context, instHelper.supportedExtensions, wsiType);
			const Unique<VkSurfaceKHR>	surface		(createSurface(instHelper.vki,
																   instHelper.instance,
																   wsiType,
																   native.getDisplay(),
																   native.getWindow(),
																   context.getTestContext().getCommandLine(),
																   failingAllocator.getCallbacks()));

			if (!validateAndLog(log,
								allocationRecorder,
								(1u<<VK_SYSTEM_ALLOCATION_SCOPE_OBJECT)		|
								(1u<<VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE)))
				return tcu::TestStatus::fail("Detected invalid system allocation callback");
		}
		catch (const OutOfMemoryError& e)
		{
			log << TestLog::Message << "Got " << e.getError() << TestLog::EndMessage;
			gotOOM = true;
		}

		if (!validateAndLog(log, allocationRecorder, 0u))
			return tcu::TestStatus::fail("Detected invalid system allocation callback");

		if (!gotOOM)
		{
			log << TestLog::Message << "Creating surface succeeded!" << TestLog::EndMessage;

			if (numPassingAllocs == 0)
				return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Allocation callbacks were not used");
			else
				return tcu::TestStatus::pass("OOM simulation completed");
		}
	}

	return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Creating surface did not succeed, callback limit exceeded");
}

deUint32 getNumQueueFamilies (const InstanceInterface& vki, VkPhysicalDevice physicalDevice)
{
	deUint32	numFamilies		= 0;

	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numFamilies, DE_NULL);

	return numFamilies;
}

tcu::TestStatus querySurfaceSupportTest (Context& context, Type wsiType)
{
	tcu::TestLog&					log						= context.getTestContext().getLog();
	tcu::ResultCollector			results					(log);

	const InstanceHelper			instHelper				(context, wsiType);
	const NativeObjects				native					(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface					(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices			= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	// On Android surface must be supported by all devices and queue families
	const bool						expectSupportedOnAll	= wsiType == TYPE_ANDROID;

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		const VkPhysicalDevice		physicalDevice		= physicalDevices[deviceNdx];
		const deUint32				numQueueFamilies	= getNumQueueFamilies(instHelper.vki, physicalDevice);

		for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < numQueueFamilies; ++queueFamilyNdx)
		{
			const VkBool32	isSupported		= getPhysicalDeviceSurfaceSupport(instHelper.vki, physicalDevice, queueFamilyNdx, *surface);

			log << TestLog::Message << "Device " << deviceNdx << ", queue family " << queueFamilyNdx << ": "
									<< (isSupported == VK_FALSE ? "NOT " : "") << "supported"
				<< TestLog::EndMessage;

			if (expectSupportedOnAll && !isSupported)
				results.fail("Surface must be supported by all devices and queue families");
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus queryPresentationSupportTest(Context& context, Type wsiType)
{
	// There is no implementation of getPhysicalDevicePresentationSupport for DRM.
	if (wsiType == TYPE_DIRECT_DRM) {
		TCU_THROW(NotSupportedError, "No presentation support query for Drm.");
	}

	tcu::TestLog&					log						= context.getTestContext().getLog();
	tcu::ResultCollector			results					(log);

	const InstanceHelper			instHelper				(context, wsiType);
	const NativeObjects				native					(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface					(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices			= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	native.getDisplay();
	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		const VkPhysicalDevice		physicalDevice		= physicalDevices[deviceNdx];
		const deUint32				numQueueFamilies	= getNumQueueFamilies(instHelper.vki, physicalDevice);

		for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < numQueueFamilies; ++queueFamilyNdx)
		{
			VkBool32	isPresentationSupported	= getPhysicalDevicePresentationSupport(instHelper.vki, physicalDevice, queueFamilyNdx, wsiType, native.getDisplay());
			VkBool32	isSurfaceSupported		= getPhysicalDeviceSurfaceSupport(instHelper.vki, physicalDevice, queueFamilyNdx, *surface);

			log << TestLog::Message << "Device " << deviceNdx << ", queue family " << queueFamilyNdx << ": presentation "
									<< (isPresentationSupported == VK_FALSE ? "NOT " : "") << "supported. Surface "
									<< (isSurfaceSupported == VK_FALSE ? "NOT " : "") << "supported."
				<< TestLog::EndMessage;

			if (isPresentationSupported != isSurfaceSupported)
				results.fail("Presentation support is different from surface support");
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

bool isSupportedByAnyQueue (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	const deUint32	numQueueFamilies	= getNumQueueFamilies(vki, physicalDevice);

	for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < numQueueFamilies; ++queueFamilyNdx)
	{
		if (getPhysicalDeviceSurfaceSupport(vki, physicalDevice, queueFamilyNdx, surface) != VK_FALSE)
			return true;
	}

	return false;
}

void validateSurfaceCapabilities (tcu::ResultCollector& results, const VkSurfaceCapabilitiesKHR& capabilities)
{
	results.check(capabilities.minImageCount > 0,
				  "minImageCount must be larger than 0");

	results.check(capabilities.minImageExtent.width > 0 &&
				  capabilities.minImageExtent.height > 0,
				  "minImageExtent dimensions must be larger than 0");

	results.check(capabilities.maxImageExtent.width > 0 &&
				  capabilities.maxImageExtent.height > 0,
				  "maxImageExtent dimensions must be larger than 0");

	results.check(capabilities.minImageExtent.width <= capabilities.maxImageExtent.width &&
				  capabilities.minImageExtent.height <= capabilities.maxImageExtent.height,
				  "maxImageExtent must be larger or equal to minImageExtent");

	if (capabilities.currentExtent.width != SURFACE_EXTENT_DETERMINED_BY_SWAPCHAIN_MAGIC ||
		capabilities.currentExtent.height != SURFACE_EXTENT_DETERMINED_BY_SWAPCHAIN_MAGIC)
	{
		results.check(capabilities.currentExtent.width > 0 &&
					  capabilities.currentExtent.height > 0,
					  "currentExtent dimensions must be larger than 0");

		results.check(de::inRange(capabilities.currentExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width) &&
					  de::inRange(capabilities.currentExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
					  "currentExtent is not in supported extent limits");
	}

	results.check(capabilities.maxImageArrayLayers > 0,
				  "maxImageArrayLayers must be larger than 0");

	results.check((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0,
				  "VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT must be set in supportedUsageFlags");

	results.check(capabilities.supportedTransforms != 0,
				  "At least one transform must be supported");

	results.check(dePop32(capabilities.currentTransform) != 0,
				  "Invalid currentTransform");

	results.check((capabilities.supportedTransforms & capabilities.currentTransform) != 0,
				  "currentTransform is not supported by surface");

	results.check(capabilities.supportedCompositeAlpha != 0,
				  "At least one alpha mode must be supported");
}

tcu::TestStatus querySurfaceCapabilitiesTest (Context& context, Type wsiType)
{
	tcu::TestLog&					log						= context.getTestContext().getLog();
	tcu::ResultCollector			results					(log);

	const InstanceHelper			instHelper				(context, wsiType);
	const NativeObjects				native					(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface					(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices			= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			const VkSurfaceCapabilitiesKHR	capabilities	= getPhysicalDeviceSurfaceCapabilities(instHelper.vki,
																								   physicalDevices[deviceNdx],
																								   *surface);

			log << TestLog::Message << "Device " << deviceNdx << ": " << capabilities << TestLog::EndMessage;

			validateSurfaceCapabilities(results, capabilities);
		}
		// else skip query as surface is not supported by the device
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus querySurfaceCapabilities2Test (Context& context, Type wsiType)
{
	tcu::TestLog&					log						= context.getTestContext().getLog();
	tcu::ResultCollector			results					(log);

	const InstanceHelper			instHelper				(context, wsiType, vector<string>(1, string("VK_KHR_get_surface_capabilities2")));
	const NativeObjects				native					(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface					(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices			= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			const VkSurfaceCapabilitiesKHR	refCapabilities	= getPhysicalDeviceSurfaceCapabilities(instHelper.vki,
																								   physicalDevices[deviceNdx],
																								   *surface);
			VkSurfaceCapabilities2KHR		extCapabilities;

			deMemset(&extCapabilities, 0xcd, sizeof(VkSurfaceCapabilities2KHR));
			extCapabilities.sType	= VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
			extCapabilities.pNext	= DE_NULL;

			{
				const VkPhysicalDeviceSurfaceInfo2KHR	surfaceInfo	=
				{
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
					DE_NULL,
					*surface
				};
				VkPhysicalDeviceSurfaceInfo2KHR			infoCopy;

				deMemcpy(&infoCopy, &surfaceInfo, sizeof(VkPhysicalDeviceSurfaceInfo2KHR));

				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevices[deviceNdx], &surfaceInfo, &extCapabilities));

				results.check(deMemoryEqual(&surfaceInfo, &infoCopy, sizeof(VkPhysicalDeviceSurfaceInfo2KHR)) == DE_TRUE, "Driver wrote into input struct");
			}

			results.check(extCapabilities.sType == VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR &&
						  extCapabilities.pNext == DE_NULL,
						  "sType/pNext modified");

			if (refCapabilities != extCapabilities.surfaceCapabilities)
			{
				log << TestLog::Message
					<< "Device " << deviceNdx
					<< ": expected " << refCapabilities
					<< ", got " << extCapabilities.surfaceCapabilities
					<< TestLog::EndMessage;
				results.fail("Mismatch between VK_KHR_surface and VK_KHR_surface2 query results");
			}
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus querySurfaceProtectedCapabilitiesTest (Context& context, Type wsiType)
{
	tcu::TestLog&			log			= context.getTestContext().getLog();
	tcu::ResultCollector		results			(log);

	vector<string>			requiredExtensions;
	requiredExtensions.push_back("VK_KHR_get_surface_capabilities2");
	requiredExtensions.push_back("VK_KHR_surface_protected_capabilities");
	const InstanceHelper		instHelper		(context, wsiType, requiredExtensions);
	const NativeObjects		native			(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>	surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices		= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			VkSurfaceCapabilities2KHR		extCapabilities;
			VkSurfaceProtectedCapabilitiesKHR	extProtectedCapabilities;

			deMemset(&extProtectedCapabilities, 0xcd, sizeof(VkSurfaceProtectedCapabilitiesKHR));
			extProtectedCapabilities.sType		= VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR;
			extProtectedCapabilities.pNext		= DE_NULL;

			deMemset(&extCapabilities, 0xcd, sizeof(VkSurfaceCapabilities2KHR));
			extCapabilities.sType	= VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
			extCapabilities.pNext	= &extProtectedCapabilities;

			{
				VkPhysicalDeviceSurfaceInfo2KHR		infoCopy;
				const VkPhysicalDeviceSurfaceInfo2KHR	surfaceInfo =
				{
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
					DE_NULL,
					*surface
				};


				deMemcpy(&infoCopy, &surfaceInfo, sizeof(VkPhysicalDeviceSurfaceInfo2KHR));

				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevices[deviceNdx], &surfaceInfo, &extCapabilities));

				results.check(deMemoryEqual(&surfaceInfo, &infoCopy, sizeof(VkPhysicalDeviceSurfaceInfo2KHR)) == DE_TRUE, "Driver wrote into input struct");
			}

			results.check(extCapabilities.sType == VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR &&
					extCapabilities.pNext == &extProtectedCapabilities,
					"sType/pNext modified");

			results.check(extProtectedCapabilities.sType == VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR &&
					extProtectedCapabilities.pNext == DE_NULL,
					"sType/pNext modified");

			results.check(extProtectedCapabilities.supportsProtected == 0 ||
					extProtectedCapabilities.supportsProtected == 1,
					"supportsProtected ");
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

void validateSurfaceFormats (tcu::ResultCollector& results, Type wsiType, const vector<VkSurfaceFormatKHR>& formats)
{
	const VkSurfaceFormatKHR*	requiredFormats		= DE_NULL;
	size_t						numRequiredFormats	= 0;

	if (wsiType == TYPE_ANDROID)
	{
		static const VkSurfaceFormatKHR s_androidFormats[] =
		{
			{ VK_FORMAT_R8G8B8A8_UNORM,			VK_COLOR_SPACE_SRGB_NONLINEAR_KHR	},
			{ VK_FORMAT_R8G8B8A8_SRGB,			VK_COLOR_SPACE_SRGB_NONLINEAR_KHR	},
			{ VK_FORMAT_R5G6B5_UNORM_PACK16,	VK_COLOR_SPACE_SRGB_NONLINEAR_KHR	}
		};

		requiredFormats		= &s_androidFormats[0];
		numRequiredFormats	= DE_LENGTH_OF_ARRAY(s_androidFormats);
	}

	for (size_t ndx = 0; ndx < numRequiredFormats; ++ndx)
	{
		const VkSurfaceFormatKHR&	requiredFormat	= requiredFormats[ndx];

		if (!de::contains(formats.begin(), formats.end(), requiredFormat))
			results.fail(de::toString(requiredFormat) + " not supported");
	}

	// Check that there are no duplicates
	for (size_t ndx = 1; ndx < formats.size(); ++ndx)
	{
		if (de::contains(formats.begin(), formats.begin() + ndx, formats[ndx]))
			results.fail("Found duplicate entry " + de::toString(formats[ndx]));
	}
}

tcu::TestStatus querySurfaceFormatsTest (Context& context, Type wsiType)
{
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);

	const InstanceHelper			instHelper		(context, wsiType);
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			deUint32	numFormats = 0;

			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevices[deviceNdx], *surface, &numFormats, DE_NULL));

			std::vector<VkSurfaceFormatKHR>	formats	(numFormats + 1);

			if (numFormats > 0)
			{
				const deUint32 numFormatsOrig = numFormats;

				// check if below call properly overwrites formats count
				numFormats++;

				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevices[deviceNdx], *surface, &numFormats, &formats[0]));

				if (numFormats != numFormatsOrig)
					results.fail("Format count changed between calls");
			}

			formats.pop_back();

			log << TestLog::Message << "Device " << deviceNdx << ": " << tcu::formatArray(formats.begin(), formats.end()) << TestLog::EndMessage;

			validateSurfaceFormats(results, wsiType, formats);
			CheckPhysicalDeviceSurfaceFormatsIncompleteResult()(results, instHelper.vki, physicalDevices[deviceNdx], *surface, formats.size());
		}
		// else skip query as surface is not supported by the device
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus querySurfaceFormatsTestSurfaceless (Context& context, Type wsiType)
{
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);

	const InstanceHelper			instHelper		(context, wsiType, vector<string>(1, string("VK_GOOGLE_surfaceless_query")));
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const VkSurfaceKHR				nullSurface		= 0;
	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			deUint32	numFormatsSurface = 0;
			deUint32	numFormatsNull = 0;

			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevices[deviceNdx], *surface, &numFormatsSurface, DE_NULL));
			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevices[deviceNdx], nullSurface, &numFormatsNull, DE_NULL));

			if (numFormatsSurface != numFormatsNull)
			{
				results.fail("Number of formats do not match");
				continue;
			}

			std::vector<VkSurfaceFormatKHR>	formatsSurface(numFormatsSurface + 1);
			std::vector<VkSurfaceFormatKHR>	formatsNull(numFormatsSurface + 1);

			if (numFormatsSurface > 0)
			{
				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevices[deviceNdx], *surface, &numFormatsSurface, &formatsSurface[0]));
				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevices[deviceNdx], nullSurface, &numFormatsSurface, &formatsNull[0]));
			}

			formatsSurface.pop_back();
			formatsNull.pop_back();

			for (deUint32 i = 0; i < numFormatsSurface; i++)
			{
				if (formatsSurface[i].colorSpace != formatsNull[i].colorSpace ||
					formatsSurface[i].format     != formatsNull[i].format)
				{
					results.fail("Surface formats do not match");
				}
			}
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus querySurfaceFormats2Test (Context& context, Type wsiType)
{
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);

	const InstanceHelper			instHelper		(context, wsiType, vector<string>(1, string("VK_KHR_get_surface_capabilities2")));
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			const vector<VkSurfaceFormatKHR>		refFormats	= getPhysicalDeviceSurfaceFormats(instHelper.vki,
																								  physicalDevices[deviceNdx],
																								  *surface);
			const VkPhysicalDeviceSurfaceInfo2KHR	surfaceInfo	=
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
				DE_NULL,
				*surface
			};
			deUint32								numFormats	= 0;

			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(physicalDevices[deviceNdx], &surfaceInfo, &numFormats, DE_NULL));

			if ((size_t)numFormats != refFormats.size())
				results.fail("vkGetPhysicalDeviceSurfaceFormats2KHR() returned different number of formats");

			if (numFormats > 0)
			{
				vector<VkSurfaceFormat2KHR>	formats	(numFormats + 1);

				for (size_t ndx = 0; ndx < formats.size(); ++ndx)
				{
					formats[ndx].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
					formats[ndx].pNext = DE_NULL;
				}

				const deUint32 numFormatsOrig = numFormats;

				// check if below call properly overwrites formats count
				numFormats++;

				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(physicalDevices[deviceNdx], &surfaceInfo, &numFormats, &formats[0]));

				if ((size_t)numFormats != numFormatsOrig)
					results.fail("Format count changed between calls");

				formats.pop_back();

				{
					vector<VkSurfaceFormatKHR>	extFormats	(formats.size());

					for (size_t ndx = 0; ndx < formats.size(); ++ndx)
					{
						results.check(formats[ndx].sType == VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR &&
									  formats[ndx].pNext == DE_NULL,
									  "sType/pNext modified");
						extFormats[ndx] = formats[ndx].surfaceFormat;
					}

					for (size_t ndx = 0; ndx < refFormats.size(); ++ndx)
					{
						if (!de::contains(extFormats.begin(), extFormats.end(), refFormats[ndx]))
							results.fail(de::toString(refFormats[ndx]) + " missing from extended query");
					}
				}

				// Check VK_INCOMPLETE
				{
					vector<VkSurfaceFormat2KHR>	formatsClone	(formats);
					deUint32					numToSupply		= numFormats/2;
					VkResult					queryResult;

					ValidateQueryBits::fillBits(formatsClone.begin() + numToSupply, formatsClone.end());

					queryResult = instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(physicalDevices[deviceNdx], &surfaceInfo, &numToSupply, &formatsClone[0]);

					results.check(queryResult == VK_INCOMPLETE, "Expected VK_INCOMPLETE");
					results.check(ValidateQueryBits::checkBits(formatsClone.begin() + numToSupply, formatsClone.end()),
								  "Driver wrote past last element");

					for (size_t ndx = 0; ndx < (size_t)numToSupply; ++ndx)
					{
						results.check(formatsClone[ndx].sType == VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR &&
									  formatsClone[ndx].pNext == DE_NULL &&
									  formatsClone[ndx].surfaceFormat == formats[ndx].surfaceFormat,
									  "Returned element " + de::toString(ndx) + " is different");
					}
				}
			}
		}
		// else skip query as surface is not supported by the device
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

void validateSurfacePresentModes (tcu::ResultCollector& results, Type wsiType, const vector<VkPresentModeKHR>& modes)
{
	results.check(de::contains(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_KHR),
				  "VK_PRESENT_MODE_FIFO_KHR is not supported");

	if (wsiType == TYPE_ANDROID)
		results.check(de::contains(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR),
					  "VK_PRESENT_MODE_MAILBOX_KHR is not supported");
}

tcu::TestStatus querySurfacePresentModes2Test (Context& context, Type wsiType)
{
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);

	const InstanceHelper			instHelper		(context, wsiType);
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);
	const std::vector<VkExtensionProperties>	deviceExtensions(enumerateDeviceExtensionProperties(instHelper.vki, context.getPhysicalDevice(), DE_NULL));
	if (!isExtensionStructSupported(deviceExtensions, RequiredExtension("VK_EXT_full_screen_exclusive")))
		TCU_THROW(NotSupportedError, "Extension VK_EXT_full_screen_exclusive not supported");

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			const VkPhysicalDeviceSurfaceInfo2KHR	surfaceInfo =
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
				DE_NULL,
				*surface
			};

			deUint32	numModesRef = 0;
			deUint32	numModes    = 0;

			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfacePresentModesKHR(physicalDevices[deviceNdx], *surface, &numModesRef, DE_NULL));
			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfacePresentModes2EXT(physicalDevices[deviceNdx], &surfaceInfo, &numModes, DE_NULL));

			if (numModes != numModesRef)
			{
				results.fail("Number of modes do not match");
				continue;
			}

			vector<VkPresentModeKHR>	modes	(numModes + 1);

			if (numModes > 0)
			{
				const deUint32 numModesOrig = numModes;

				// check if below call properly overwrites mode count
				numModes++;

				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfacePresentModes2EXT(physicalDevices[deviceNdx], &surfaceInfo, &numModes, &modes[0]));

				if ((size_t)numModes != numModesOrig)
					TCU_FAIL("Mode count changed between calls");
			}

			modes.pop_back();

			log << TestLog::Message << "Device " << deviceNdx << ": " << tcu::formatArray(modes.begin(), modes.end()) << TestLog::EndMessage;

			validateSurfacePresentModes(results, wsiType, modes);
			if (numModes > 1)
			{
				numModes /= 2;
				vk::VkResult res = instHelper.vki.getPhysicalDeviceSurfacePresentModes2EXT(physicalDevices[deviceNdx], &surfaceInfo, &numModes, &modes[0]);
				if (res != VK_INCOMPLETE)
					TCU_FAIL("Failed to fetch incomplete results");
			}
		}
		// else skip query as surface is not supported by the device
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus querySurfacePresentModes2TestSurfaceless (Context& context, Type wsiType)
{
	tcu::TestLog&									log					= context.getTestContext().getLog();
	tcu::ResultCollector							results				(log);

	const std::string								extensionName		= "VK_GOOGLE_surfaceless_query";
	const InstanceHelper							instHelper			(context, wsiType, vector<string>(1, extensionName), DE_NULL);
	const NativeObjects								native				(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>						surface				(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const VkSurfaceKHR								nullSurface			= 0;
	const vector<VkPhysicalDevice>					physicalDevices		= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);
	const std::vector<vk::VkExtensionProperties>	deviceExtensions	(enumerateDeviceExtensionProperties(instHelper.vki, context.getPhysicalDevice(), DE_NULL));
	const vector<VkPresentModeKHR>					validPresentModes	{ VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR };

	if (!isExtensionStructSupported(deviceExtensions, RequiredExtension("VK_EXT_full_screen_exclusive")))
		return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "Extension VK_EXT_full_screen_exclusive not supported");

	// Ensure "VK_GOOGLE_surfaceless_query" extension's spec version is at least 2
	{
		deUint32									propertyCount = 0u;
		std::vector<vk::VkExtensionProperties>		extensionsProperties;
		vk::VkResult								extensionResult;

		extensionResult = context.getPlatformInterface().enumerateInstanceExtensionProperties(DE_NULL, &propertyCount, DE_NULL);
		if (extensionResult != vk::VK_SUCCESS)
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Failed to retrieve spec version for extension " + extensionName);

		extensionsProperties.resize(propertyCount);

		extensionResult = context.getPlatformInterface().enumerateInstanceExtensionProperties(DE_NULL, &propertyCount, extensionsProperties.data());
		if (extensionResult != vk::VK_SUCCESS)
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Failed to retrieve spec version for extension " + extensionName);

		for (const auto& property : extensionsProperties)
		{
			if (property.extensionName == extensionName)
			{
				if (property.specVersion < 2)
					return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED, "VK_GOOGLE_surfaceless_query is version 1. Need version 2 or higher");
				break;
			}
		}
	}

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		const VkPhysicalDeviceSurfaceInfo2KHR	nullSurfaceInfo =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
			DE_NULL,
			nullSurface
		};
		deUint32	numModesNull	= 0u;

		VK_CHECK(instHelper.vki.getPhysicalDeviceSurfacePresentModes2EXT(physicalDevices[deviceNdx], &nullSurfaceInfo, &numModesNull, DE_NULL));

		if (numModesNull == 0u)
			continue;

		vector<VkPresentModeKHR>	modesNull(numModesNull);

		VK_CHECK(instHelper.vki.getPhysicalDeviceSurfacePresentModes2EXT(physicalDevices[deviceNdx], &nullSurfaceInfo, &numModesNull, &modesNull[0]));

		for (deUint32 i = 0; i < modesNull.size(); i++)
		{
			if (std::find(validPresentModes.begin(), validPresentModes.end(), modesNull[i]) == validPresentModes.end())
			{
				std::string error_string	= std::string("Present mode mismatch with mode: ") + getPresentModeKHRName(modesNull[i]);
				results.fail(error_string);
				break;
			}
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus querySurfaceFormats2TestSurfaceless (Context& context, Type wsiType)
{
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);

	const vector<std::string>		extensions		({"VK_KHR_get_surface_capabilities2", "VK_GOOGLE_surfaceless_query"});
	const InstanceHelper			instHelper		(context, wsiType, extensions );
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const VkSurfaceKHR				nullSurface		= 0;
	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			const VkPhysicalDeviceSurfaceInfo2KHR	surfaceInfo =
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
				DE_NULL,
				*surface
			};
			const VkPhysicalDeviceSurfaceInfo2KHR	nullSurfaceInfo =
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
				DE_NULL,
				nullSurface
			};
			deUint32								numFormatsSurface = 0;
			deUint32								numFormatsNull    = 0;

			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(physicalDevices[deviceNdx], &surfaceInfo, &numFormatsSurface, DE_NULL));
			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(physicalDevices[deviceNdx], &nullSurfaceInfo, &numFormatsNull, DE_NULL));

			if (numFormatsSurface != numFormatsNull)
			{
				results.fail("Number of formats do not match");
				continue;
			}

			if (numFormatsSurface > 0)
			{
				vector<VkSurfaceFormat2KHR>	formatsSurface(numFormatsSurface + 1);
				vector<VkSurfaceFormat2KHR>	formatsNull(numFormatsSurface + 1);

				for (size_t ndx = 0; ndx < formatsSurface.size(); ++ndx)
				{
					formatsSurface[ndx].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
					formatsSurface[ndx].pNext = DE_NULL;
					formatsNull[ndx].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
					formatsNull[ndx].pNext = DE_NULL;
				}

				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(physicalDevices[deviceNdx], &surfaceInfo, &numFormatsSurface, &formatsSurface[0]));
				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(physicalDevices[deviceNdx], &nullSurfaceInfo, &numFormatsSurface, &formatsNull[0]));

				formatsSurface.pop_back();
				formatsNull.pop_back();

				for (deUint32 i = 0; i < numFormatsSurface; i++)
				{
					if (formatsSurface[i].surfaceFormat != formatsNull[i].surfaceFormat)
					{
						results.fail("Surface formats do not match");
					}
				}
			}
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus querySurfacePresentModesTest (Context& context, Type wsiType)
{
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);

	const InstanceHelper			instHelper		(context, wsiType);
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);

	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
		{
			deUint32	numModes = 0;

			VK_CHECK(instHelper.vki.getPhysicalDeviceSurfacePresentModesKHR(physicalDevices[deviceNdx], *surface, &numModes, DE_NULL));

			vector<VkPresentModeKHR>	modes	(numModes + 1);

			if (numModes > 0)
			{
				const deUint32 numModesOrig = numModes;

				// check if below call properly overwrites mode count
				numModes++;

				VK_CHECK(instHelper.vki.getPhysicalDeviceSurfacePresentModesKHR(physicalDevices[deviceNdx], *surface, &numModes, &modes[0]));

				if ((size_t)numModes != numModesOrig)
					TCU_FAIL("Mode count changed between calls");
			}

			modes.pop_back();

			log << TestLog::Message << "Device " << deviceNdx << ": " << tcu::formatArray(modes.begin(), modes.end()) << TestLog::EndMessage;

			validateSurfacePresentModes(results, wsiType, modes);
			CheckPhysicalDeviceSurfacePresentModesIncompleteResult()(results, instHelper.vki, physicalDevices[deviceNdx], *surface, modes.size());
		}
		// else skip query as surface is not supported by the device
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

void checkDeprecatedExtensionGoogleSurfacelessQuery(const vk::InstanceDriver& vk, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, tcu::ResultCollector& result)
{
	const VkSurfaceKHR				nullSurface		= DE_NULL;

	if (isSupportedByAnyQueue(vk, physicalDevice, surface))
	{
		deUint32	numModesSurface = 0;
		deUint32	numModesNull	= 0;

		VK_CHECK(vk.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numModesSurface, DE_NULL));
		VK_CHECK(vk.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, nullSurface, &numModesNull, DE_NULL));

		if (numModesNull == 0)
			return;

		// Actual surface needs to have at least the amount of modes that null surface has
		if (numModesSurface < numModesNull)
		{
			result.fail("Number of modes does not match");
			return;
		}

		vector<VkPresentModeKHR>	modesSurface(numModesSurface);
		vector<VkPresentModeKHR>	modesNull(numModesNull);

		VK_CHECK(vk.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numModesSurface, &modesSurface[0]));
		VK_CHECK(vk.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, nullSurface, &numModesNull, &modesNull[0]));

		// All modes present in null surface should also be present in actual surface
		for (deUint32 i = 0; i < modesNull.size(); i++)
		{
			if (std::find(modesSurface.begin(), modesSurface.end(), modesNull[i]) == modesSurface.end())
			{
				std::string error_string	= std::string("Present mode mismatch with mode: ") + getPresentModeKHRName(modesNull[i]);
				result.fail(error_string);
				break;
			}
		}
	}
}

void checkExtensionGoogleSurfacelessQuery(const vk::InstanceDriver& vk, VkPhysicalDevice physicalDevice, tcu::ResultCollector& result)
{
	const VkSurfaceKHR				nullSurface		= DE_NULL;
	const vector<VkPresentModeKHR>	validPresentModes	{ VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR };

	deUint32	numModesNull	= 0;
	VK_CHECK(vk.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, nullSurface, &numModesNull, DE_NULL));

	if (numModesNull == 0u)
		return;

	vector<VkPresentModeKHR>	modesNull(numModesNull);

	VK_CHECK(vk.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, nullSurface, &numModesNull, &modesNull[0]));

	for (deUint32 i = 0; i < modesNull.size(); i++)
	{
		if (std::find(validPresentModes.begin(), validPresentModes.end(), modesNull[i]) == validPresentModes.end())
		{
			std::string error_string	= std::string("Present mode mismatch with mode: ") + getPresentModeKHRName(modesNull[i]);
			result.fail(error_string);
			break;
		}
	}
}

tcu::TestStatus querySurfacePresentModesTestSurfaceless (Context& context, Type wsiType)
{
	tcu::TestLog&					log						= context.getTestContext().getLog();
	tcu::ResultCollector			results					(log);

	const std::string				extensionName			= "VK_GOOGLE_surfaceless_query";
	const InstanceHelper			instHelper				(context, wsiType, vector<string>(1, extensionName), DE_NULL);
	const NativeObjects				native					(context, instHelper.supportedExtensions, wsiType);
	const Unique<vk::VkSurfaceKHR>	surface					(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));

	deUint32						extensionVersion		= 1u;

	// Get "VK_GOOGLE_surfaceless_query" extension's spec version
	{
		deUint32									propertyCount = 0u;
		std::vector<vk::VkExtensionProperties>		extensionsProperties;
		vk::VkResult								extensionResult;

		extensionResult = context.getPlatformInterface().enumerateInstanceExtensionProperties(DE_NULL, &propertyCount, DE_NULL);
		if (extensionResult != vk::VK_SUCCESS)
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Failed to retrieve spec version for extension " + extensionName);

		extensionsProperties.resize(propertyCount);

		extensionResult = context.getPlatformInterface().enumerateInstanceExtensionProperties(DE_NULL, &propertyCount, extensionsProperties.data());
		if (extensionResult != vk::VK_SUCCESS)
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Failed to retrieve spec version for extension " + extensionName);

		for (const auto& property : extensionsProperties)
		{
			if (property.extensionName == extensionName)
			{
				extensionVersion = property.specVersion;
				break;
			}
		}
	}

	log << TestLog::Message << "Checking spec version " << extensionVersion << " for VK_GOOGLE_surfaceless_query" << TestLog::EndMessage;

	const bool						checkDeprecatedVersion	= extensionVersion < 2;
	const vector<VkPhysicalDevice>	physicalDevices			= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);
	for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
	{
		if (checkDeprecatedVersion)
			checkDeprecatedExtensionGoogleSurfacelessQuery(instHelper.vki, physicalDevices[deviceNdx], *surface, results);
		else
			checkExtensionGoogleSurfacelessQuery(instHelper.vki, physicalDevices[deviceNdx], results);
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus queryDevGroupSurfacePresentCapabilitiesTest (Context& context, Type wsiType)
{
	tcu::TestLog&									log						= context.getTestContext().getLog();
	const InstanceHelper							instHelper				(context, wsiType, vector<string>(1, string("VK_KHR_device_group_creation")));
	const float										queuePriority			= 1.0f;
	const tcu::CommandLine&							cmdLine					= context.getTestContext().getCommandLine();
	const deUint32									devGroupIdx				= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32									deviceIdx				= context.getTestContext().getCommandLine().getVKDeviceId() - 1u;
	const VkDeviceGroupPresentModeFlagsKHR			requiredFlag			= VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
	const VkDeviceGroupPresentModeFlagsKHR			maxValidFlag			= VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR|VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHR |
																				VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR|VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR;
	deUint8											buffer					[sizeof(VkDeviceGroupPresentCapabilitiesKHR) + GUARD_SIZE];
	deUint32										queueFamilyIndex		= 0;
	VkDeviceGroupPresentCapabilitiesKHR*			presentCapabilities;
	std::vector<const char*>						deviceExtensions;

	if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_device_group"))
		deviceExtensions.push_back("VK_KHR_device_group");
	deviceExtensions.push_back("VK_KHR_swapchain");

	for (int ndx = 0; ndx < int(deviceExtensions.size()); ++ndx)
	{
		if (!context.isDeviceFunctionalitySupported(deviceExtensions[ndx]))
			TCU_THROW(NotSupportedError, (string(deviceExtensions[ndx]) + " is not supported").c_str());
	}

	const vector<VkPhysicalDeviceGroupProperties>	deviceGroupProps		= enumeratePhysicalDeviceGroups(instHelper.vki, instHelper.instance);

	const std::vector<VkQueueFamilyProperties>		queueProps				= getPhysicalDeviceQueueFamilyProperties(instHelper.vki, deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx]);
	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if (queueProps[queueNdx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			queueFamilyIndex = (deUint32)queueNdx;
	}
	const VkDeviceQueueCreateInfo					deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,				//type
		DE_NULL,												//pNext
		(VkDeviceQueueCreateFlags)0u,							//flags
		queueFamilyIndex,										//queueFamilyIndex;
		1u,														//queueCount;
		&queuePriority,											//pQueuePriorities;
	};
	const VkDeviceGroupDeviceCreateInfo				deviceGroupInfo			=
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR,	//stype
		DE_NULL,												//pNext
		deviceGroupProps[devGroupIdx].physicalDeviceCount,		//physicalDeviceCount
		deviceGroupProps[devGroupIdx].physicalDevices			//physicalDevices
	};

	const VkDeviceCreateInfo						deviceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//sType;
		&deviceGroupInfo,												//pNext;
		(VkDeviceCreateFlags)0u,										//flags
		1,																//queueRecordCount;
		&deviceQueueCreateInfo,											//pRequestedQueues;
		0,																//layerCount;
		DE_NULL,														//ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),								//enabledExtensionCount;
		(deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0]),	//ppEnabledExtensionNames;
		DE_NULL,														//pEnabledFeatures;
	};

	Move<VkDevice>		deviceGroup = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), instHelper.instance, instHelper.vki, deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx], &deviceCreateInfo);
	const DeviceDriver	vk	(context.getPlatformInterface(), instHelper.instance, *deviceGroup, context.getUsedApiVersion());


	presentCapabilities = reinterpret_cast<VkDeviceGroupPresentCapabilitiesKHR*>(buffer);
	deMemset(buffer, GUARD_VALUE, sizeof(buffer));
	presentCapabilities->sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR;
	presentCapabilities->pNext = DE_NULL;
	VK_CHECK(vk.getDeviceGroupPresentCapabilitiesKHR(deviceGroup.get(), presentCapabilities));

	// Guard check
	for (deInt32 ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkDeviceGroupPresentCapabilitiesKHR)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceGroupPresentCapabilities - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceGroupPresentCapabilities buffer overflow");
		}
	}

	// Check each physical device can present on itself
	for (size_t physDevIdx = 0; physDevIdx < VK_MAX_DEVICE_GROUP_SIZE; physDevIdx++)
	{
		if (presentCapabilities->presentMask[physDevIdx])
			if (!((1 << physDevIdx) & (presentCapabilities->presentMask[physDevIdx])))
				return tcu::TestStatus::fail("deviceGroupPresentCapabilities, device can not present on itself, invalid present mask");
	}

	// Check if flags are valid
	if ((!(presentCapabilities->modes & requiredFlag)) ||
		presentCapabilities->modes > maxValidFlag)
		return tcu::TestStatus::fail("deviceGroupPresentCapabilities flag not valid");

	return tcu::TestStatus::pass("Querying deviceGroup present capabilities succeeded");
}

tcu::TestStatus queryDevGroupSurfacePresentModesTest (Context& context, Type wsiType)
{
	tcu::TestLog&							log					= context.getTestContext().getLog();
	tcu::ResultCollector					results				(log);
	const InstanceHelper					instHelper			(context, wsiType, vector<string>(1, string("VK_KHR_device_group_creation")));
	const NativeObjects						native				(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>				surface				(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow(), context.getTestContext().getCommandLine()));
	const float								queuePriority		= 1.0f;
	const tcu::CommandLine&					cmdLine				= context.getTestContext().getCommandLine();
	const deUint32							devGroupIdx			= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32							deviceIdx			= context.getTestContext().getCommandLine().getVKDeviceId() - 1u;
	const VkDeviceGroupPresentModeFlagsKHR	requiredFlag		= VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
	const VkDeviceGroupPresentModeFlagsKHR	maxValidFlag		= VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR|VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHR |
																	VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR|VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR;
	VkResult								result				= VK_SUCCESS;
	deUint8									buffer				[sizeof(VkDeviceGroupPresentModeFlagsKHR) + GUARD_SIZE];
	deUint32								rectCount			= 0;
	deUint32								incompleteRectCount	= 0;
	deUint32								queueFamilyIndex	= 0;
	VkRect2D*								presentRectangles;
	VkDeviceGroupPresentModeFlagsKHR*		presentModeFlags;
	vector<deUint8>							rectanglesBuffer;
	VkPhysicalDevice						physicalDevice		= chooseDevice(instHelper.vki, instHelper.instance, cmdLine);
	const Extensions&						supportedExtensions	= enumerateDeviceExtensionProperties(instHelper.vki, physicalDevice, DE_NULL);
	std::vector<const char*>				deviceExtensions;

	if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_device_group"))
		deviceExtensions.push_back("VK_KHR_device_group");
	deviceExtensions.push_back("VK_KHR_swapchain");

	for (int ndx = 0; ndx < int(deviceExtensions.size()); ++ndx)
	{
		if (!isExtensionStructSupported(supportedExtensions, RequiredExtension(deviceExtensions[ndx])))
			TCU_THROW(NotSupportedError, (string(deviceExtensions[ndx]) + " is not supported").c_str());
	}

	const vector<VkPhysicalDeviceGroupProperties>	deviceGroupProps = enumeratePhysicalDeviceGroups(instHelper.vki, instHelper.instance);
	const std::vector<VkQueueFamilyProperties>	queueProps		= getPhysicalDeviceQueueFamilyProperties(instHelper.vki, deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx]);
	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if (queueProps[queueNdx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			queueFamilyIndex = (deUint32)queueNdx;
	}
	const VkDeviceQueueCreateInfo			deviceQueueCreateInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,					//type
		DE_NULL,													//pNext
		(VkDeviceQueueCreateFlags)0u,								//flags
		queueFamilyIndex,											//queueFamilyIndex;
		1u,															//queueCount;
		&queuePriority,												//pQueuePriorities;
	};
	const VkDeviceGroupDeviceCreateInfo			deviceGroupInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR,	//stype
		DE_NULL,												//pNext
		deviceGroupProps[devGroupIdx].physicalDeviceCount,		//physicalDeviceCount
		deviceGroupProps[devGroupIdx].physicalDevices			//physicalDevices
	};

	const VkDeviceCreateInfo					deviceCreateInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//sType;
		&deviceGroupInfo,												//pNext;
		(VkDeviceCreateFlags)0u,										//flags
		1,																//queueRecordCount;
		&deviceQueueCreateInfo,											//pRequestedQueues;
		0,																//layerCount;
		DE_NULL,														//ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),								//enabledExtensionCount;
		(deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0]),	//ppEnabledExtensionNames;
		DE_NULL,														//pEnabledFeatures;
	};

	Move<VkDevice>		deviceGroup = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), instHelper.instance, instHelper.vki, deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx], &deviceCreateInfo);
	const DeviceDriver	vk	(context.getPlatformInterface(), instHelper.instance, *deviceGroup, context.getUsedApiVersion());
	presentModeFlags = reinterpret_cast<VkDeviceGroupPresentModeFlagsKHR*>(buffer);
	deMemset(buffer, GUARD_VALUE, sizeof(buffer));

	VK_CHECK(vk.getDeviceGroupSurfacePresentModesKHR(deviceGroup.get(), *surface, presentModeFlags));

	// Guard check
	for (deInt32 ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkDeviceGroupPresentModeFlagsKHR)] != GUARD_VALUE)
		{
			log << TestLog::Message << "queryDevGroupSurfacePresentModesTest - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("queryDevGroupSurfacePresentModesTest buffer overflow");
		}
	}

	// Check if flags are valid
	if ((!(*presentModeFlags & requiredFlag)) ||
		*presentModeFlags > maxValidFlag)
		return tcu::TestStatus::fail("queryDevGroupSurfacePresentModesTest flag not valid");

	// getPhysicalDevicePresentRectanglesKHR is supported only when VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR is set
	if ((*presentModeFlags & VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR))
	{
		for (size_t physDevIdx = 0; physDevIdx < deviceGroupProps[devGroupIdx].physicalDeviceCount; physDevIdx++)
		{
			VK_CHECK(instHelper.vki.getPhysicalDevicePresentRectanglesKHR(deviceGroupProps[devGroupIdx].physicalDevices[physDevIdx], *surface, &rectCount, DE_NULL));
			rectanglesBuffer.resize(sizeof(VkRect2D) * rectCount + GUARD_SIZE);
			presentRectangles = reinterpret_cast<VkRect2D*>(rectanglesBuffer.data());
			deMemset(rectanglesBuffer.data(), GUARD_VALUE, rectanglesBuffer.size());

			VK_CHECK(instHelper.vki.getPhysicalDevicePresentRectanglesKHR(deviceGroupProps[devGroupIdx].physicalDevices[physDevIdx], *surface, &rectCount, presentRectangles));

			// Guard check
			for (deInt32 ndx = 0; ndx < GUARD_SIZE; ndx++)
			{
				if (rectanglesBuffer[ndx + sizeof(VkRect2D) * rectCount] != GUARD_VALUE)
				{
					log << TestLog::Message << "getPhysicalDevicePresentRectanglesKHR - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
					return tcu::TestStatus::fail("getPhysicalDevicePresentRectanglesKHR buffer overflow");
				}
			}

			// Check rectangles do not overlap
			for (size_t rectIdx1 = 0; rectIdx1 < rectCount; rectIdx1++)
			{
				for (size_t rectIdx2 = 0; rectIdx2 < rectCount; rectIdx2++)
				{
					if (rectIdx1 != rectIdx2)
					{
						deUint32 rectATop		= presentRectangles[rectIdx1].offset.y;
						deUint32 rectALeft		= presentRectangles[rectIdx1].offset.x;
						deUint32 rectABottom	= presentRectangles[rectIdx1].offset.y + presentRectangles[rectIdx1].extent.height;
						deUint32 rectARight		= presentRectangles[rectIdx1].offset.x + presentRectangles[rectIdx1].extent.width;

						deUint32 rectBTop		= presentRectangles[rectIdx2].offset.y;
						deUint32 rectBLeft		= presentRectangles[rectIdx2].offset.x;
						deUint32 rectBBottom	= presentRectangles[rectIdx2].offset.y + presentRectangles[rectIdx2].extent.height;
						deUint32 rectBRight		= presentRectangles[rectIdx2].offset.x + presentRectangles[rectIdx2].extent.width;

						if (rectALeft < rectBRight && rectARight > rectBLeft &&
							rectATop < rectBBottom && rectABottom > rectBTop)
							return tcu::TestStatus::fail("getPhysicalDevicePresentRectanglesKHR rectangles overlap");
					}
				}
			}

			// Check incomplete
			incompleteRectCount = rectCount / 2;
			result = instHelper.vki.getPhysicalDevicePresentRectanglesKHR(deviceGroupProps[devGroupIdx].physicalDevices[physDevIdx], *surface, &incompleteRectCount, presentRectangles);
			results.check(result == VK_INCOMPLETE, "Expected VK_INCOMPLETE");
		}
	}

		return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus createSurfaceInitialSizeTest (Context& context, Type wsiType)
{
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);

	const InstanceHelper			instHelper		(context, wsiType);

	const UniquePtr<Display>		nativeDisplay	(NativeObjects::createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(),
																				  instHelper.supportedExtensions,
																				  wsiType));

	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);
	const UVec2						sizes[]			=
	{
		UVec2(64, 64),
		UVec2(124, 119),
		UVec2(256, 512)
	};

	DE_ASSERT(getPlatformProperties(wsiType).features & PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE);

	for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizeNdx)
	{
		const UVec2&				testSize		= sizes[sizeNdx];
		const UniquePtr<Window>		nativeWindow	(NativeObjects::createWindow(*nativeDisplay, tcu::just(testSize)));
		const Unique<VkSurfaceKHR>	surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, *nativeDisplay, *nativeWindow, context.getTestContext().getCommandLine()));

		for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
		{
			if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
			{
				const VkSurfaceCapabilitiesKHR	capabilities	= getPhysicalDeviceSurfaceCapabilities(instHelper.vki, physicalDevices[deviceNdx], *surface);

				// \note Assumes that surface size is NOT set by swapchain if initial window size is honored by platform
				results.check(capabilities.currentExtent.width == testSize.x() &&
								capabilities.currentExtent.height == testSize.y(),
								"currentExtent " + de::toString(capabilities.currentExtent) + " doesn't match requested size " + de::toString(testSize));
			}
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus resizeSurfaceTest (Context& context, Type wsiType)
{
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);

	const InstanceHelper			instHelper		(context, wsiType);

	const UniquePtr<Display>		nativeDisplay	(NativeObjects::createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(),
																				  instHelper.supportedExtensions,
																				  wsiType));
	UniquePtr<Window>				nativeWindow	(NativeObjects::createWindow(*nativeDisplay, tcu::Nothing));

	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(instHelper.vki, instHelper.instance);
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, *nativeDisplay, *nativeWindow, context.getTestContext().getCommandLine()));

	const UVec2						sizes[]			=
	{
		UVec2(64, 64),
		UVec2(124, 119),
		UVec2(256, 512)
	};

	DE_ASSERT(getPlatformProperties(wsiType).features & PlatformProperties::FEATURE_RESIZE_WINDOW);

	for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizeNdx)
	{
		const UVec2		testSize	= sizes[sizeNdx];

		try
		{
			nativeWindow->resize(testSize);
		}
		catch (const tcu::Exception& e)
		{
			// Make sure all exception types result in a test failure
			results.fail(e.getMessage());
		}

		for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
		{
			if (isSupportedByAnyQueue(instHelper.vki, physicalDevices[deviceNdx], *surface))
			{
				const VkSurfaceCapabilitiesKHR	capabilities	= getPhysicalDeviceSurfaceCapabilities(instHelper.vki, physicalDevices[deviceNdx], *surface);

				// \note Assumes that surface size is NOT set by swapchain if initial window size is honored by platform
				results.check(capabilities.currentExtent.width == testSize.x() &&
								capabilities.currentExtent.height == testSize.y(),
								"currentExtent " + de::toString(capabilities.currentExtent) + " doesn't match requested size " + de::toString(testSize));
			}
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus destroyNullHandleSurfaceTest (Context& context, Type wsiType)
{
	const InstanceHelper	instHelper	(context, wsiType);
	const VkSurfaceKHR		nullHandle	= DE_NULL;

	// Default allocator
	instHelper.vki.destroySurfaceKHR(instHelper.instance, nullHandle, DE_NULL);

	// Custom allocator
	{
		AllocationCallbackRecorder	recordingAllocator	(getSystemAllocator(), 1u);

		instHelper.vki.destroySurfaceKHR(instHelper.instance, nullHandle, recordingAllocator.getCallbacks());

		if (recordingAllocator.getNumRecords() != 0u)
			return tcu::TestStatus::fail("Implementation allocated/freed the memory");
	}

	return tcu::TestStatus::pass("Destroying a VK_NULL_HANDLE surface has no effect");
}

} // anonymous

void createSurfaceTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	const PlatformProperties&	platformProperties	= getPlatformProperties(wsiType);

	addFunctionCase(testGroup, "create",								"Create surface",											createSurfaceTest,							wsiType);
	addFunctionCase(testGroup, "create_custom_allocator",				"Create surface with custom allocator",						createSurfaceCustomAllocatorTest,			wsiType);
	addFunctionCase(testGroup, "create_simulate_oom",					"Create surface with simulating OOM",						createSurfaceSimulateOOMTest,				wsiType);
	addFunctionCase(testGroup, "query_support",							"Query surface support",									querySurfaceSupportTest,					wsiType);
	addFunctionCase(testGroup, "query_presentation_support",			"Query native presentation support",						queryPresentationSupportTest,				wsiType);
	addFunctionCase(testGroup, "query_capabilities",					"Query surface capabilities",								querySurfaceCapabilitiesTest,				wsiType);
	addFunctionCase(testGroup, "query_capabilities2",					"Query extended surface capabilities",						querySurfaceCapabilities2Test,				wsiType);
	addFunctionCase(testGroup, "query_protected_capabilities",			"Query protected surface capabilities",						querySurfaceProtectedCapabilitiesTest,		wsiType);
	addFunctionCase(testGroup, "query_surface_counters",				"Query and check available surface counters",				querySurfaceCounterTest,					wsiType);
	addFunctionCase(testGroup, "query_formats",							"Query surface formats",									querySurfaceFormatsTest,					wsiType);
	addFunctionCase(testGroup, "query_formats2",						"Query extended surface formats",							querySurfaceFormats2Test,					wsiType);
	addFunctionCase(testGroup, "query_present_modes",					"Query surface present modes",								querySurfacePresentModesTest,				wsiType);
	addFunctionCase(testGroup, "query_present_modes2",					"Query extended surface present modes",						querySurfacePresentModes2Test,				wsiType);
	addFunctionCase(testGroup, "query_devgroup_present_capabilities",	"Query surface present modes capabilities in device groups",queryDevGroupSurfacePresentCapabilitiesTest,wsiType);
	addFunctionCase(testGroup, "query_devgroup_present_modes",			"Query surface present modes for device groups",			queryDevGroupSurfacePresentModesTest,		wsiType);
	addFunctionCase(testGroup, "destroy_null_handle",					"Destroy VK_NULL_HANDLE surface",							destroyNullHandleSurfaceTest,				wsiType);

	if ((platformProperties.features & PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE) != 0)
		addFunctionCase(testGroup, "initial_size",	"Create surface with initial window size set",	createSurfaceInitialSizeTest,	wsiType);

	if ((platformProperties.features & PlatformProperties::FEATURE_RESIZE_WINDOW) != 0)
		addFunctionCase(testGroup, "resize",		"Resize window and surface",					resizeSurfaceTest,				wsiType);

	addFunctionCase(testGroup, "query_formats_surfaceless", "Query surface formats without surface", querySurfaceFormatsTestSurfaceless, wsiType);
	addFunctionCase(testGroup, "query_present_modes_surfaceless", "Query surface present modes without surface", querySurfacePresentModesTestSurfaceless, wsiType);
	addFunctionCase(testGroup, "query_present_modes2_surfaceless", "Query extended surface present modes without surface", querySurfacePresentModes2TestSurfaceless, wsiType);
	addFunctionCase(testGroup, "query_formats2_surfaceless", "Query extended surface formats without surface", querySurfaceFormats2TestSurfaceless, wsiType);
}

} // wsi
} // vkt
