/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2017 Khronos Group
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
* \brief Checks vkGetPhysicalDevice*FormatProperties* API functions
*//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktApiPhysicalDeviceFormatPropertiesMaint5Tests.hpp"

#include <algorithm>
#include <array>

using namespace vk;

namespace vkt
{
namespace api
{

namespace
{

constexpr deUint32 HAS_FORMAT_PARAM	= 1u << 30;
constexpr deUint32 HAS_FLAGS_PARAM	= 1u << 31;
enum class FuncIDs : deUint32
{
	DeviceFormatProps					= 100u | HAS_FORMAT_PARAM,
	DeviceFormatPropsSecond				= 101u | HAS_FORMAT_PARAM,
	DeviceImageFormatProps				= 200u | HAS_FORMAT_PARAM | HAS_FLAGS_PARAM,
	DeviceImageFormatPropsSecond		= 201u | HAS_FORMAT_PARAM | HAS_FLAGS_PARAM,
	DeviceSparseImageFormatProps		= 300u | HAS_FORMAT_PARAM | HAS_FLAGS_PARAM,
	DeviceSparseImageFormatPropsSecond	= 301u | HAS_FORMAT_PARAM | HAS_FLAGS_PARAM,
};

struct TestParams
{
	FuncIDs	funcID;
};

class UnsupportedParametersMaintenance5FormatInstance : public TestInstance
{
public:
							UnsupportedParametersMaintenance5FormatInstance		(Context& context, const TestParams& params)
								: TestInstance	(context)
								, m_params	(params) {}
	virtual					~UnsupportedParametersMaintenance5FormatInstance	(void) override = default;
	virtual tcu::TestStatus	iterate												(void) override;
protected:
	TestParams	m_params;
};

class UnsupportedParametersMaintenance5FlagsInstance : public TestInstance
{
public:
							UnsupportedParametersMaintenance5FlagsInstance	(Context& context, const TestParams& params)
								: TestInstance	(context)
								, m_params	(params) {}
	virtual					~UnsupportedParametersMaintenance5FlagsInstance	(void) override = default;
	virtual tcu::TestStatus	iterate											(void) override;
protected:
	TestParams	m_params;
};

class UnsupportedParametersMaintenance5TestCase : public TestCase
{
public:
						UnsupportedParametersMaintenance5TestCase	(tcu::TestContext&		testContext,
																	 const std::string&		name,
																	 const TestParams&		params,
																	 bool					testFormatOrFlags)
							: TestCase				(testContext, name, "")
							, m_params				(params)
							, m_testFormatOrFlags	(testFormatOrFlags) { }
	virtual				~UnsupportedParametersMaintenance5TestCase	(void) = default;
	void				checkSupport								(Context&				context) const override;
	TestInstance*		createInstance								(Context&				context) const override;

protected:
	const TestParams	m_params;
	const bool			m_testFormatOrFlags;
};

TestInstance* UnsupportedParametersMaintenance5TestCase::createInstance (Context& context) const
{
	if (m_testFormatOrFlags)
		return new UnsupportedParametersMaintenance5FormatInstance(context, m_params);
	return new UnsupportedParametersMaintenance5FlagsInstance(context, m_params);
}

void UnsupportedParametersMaintenance5TestCase::checkSupport (Context &context) const
{
	context.requireDeviceFunctionality("VK_KHR_maintenance5");
	if (context.getMaintenance5Features().maintenance5 != VK_TRUE)
	{
		TCU_THROW(NotSupportedError, "Maintenance5 feature is not supported by this implementation");
	}
}

bool operator==(const VkFormatProperties& l, const VkFormatProperties& r)
{
	return	l.bufferFeatures		== r.bufferFeatures
		&&	l.linearTilingFeatures	== r.linearTilingFeatures
		&&	l.optimalTilingFeatures	== r.optimalTilingFeatures;
}

bool operator==(const VkImageFormatProperties& l, const VkImageFormatProperties& r)
{
	return	l.maxMipLevels		== r.maxMipLevels
		&&	l.maxArrayLayers	== r.maxArrayLayers
		&&	l.sampleCounts		== r.sampleCounts
		&&	l.maxResourceSize	== r.maxResourceSize;
}

template<class StructType>
StructType makeInvalidVulkanStructure (void* pNext = DE_NULL)
{
	StructType s;
	deMemset(&s, 0xFF, (size_t)(sizeof(s)));
	s.sType	= getStructureType<StructType>();
	s.pNext	= pNext;
	return s;
}

tcu::TestStatus UnsupportedParametersMaintenance5FormatInstance::iterate (void)
{
	const VkPhysicalDevice				dev				= m_context.getPhysicalDevice();
	const InstanceInterface&			inst			= m_context.getInstanceInterface();
	VkResult							res				= VK_ERROR_FORMAT_NOT_SUPPORTED;
	uint32_t							propsCount		= 0;
	const VkImageUsageFlags				usage			= VK_IMAGE_USAGE_STORAGE_BIT;
	const VkImageType					imageType		= VK_IMAGE_TYPE_2D;
	const VkImageTiling					tiling			= VK_IMAGE_TILING_OPTIMAL;
	const VkImageCreateFlags			createFlags		= 0;
	const VkSampleCountFlagBits			sampling		= VK_SAMPLE_COUNT_1_BIT;
	VkFormatProperties2					props2			= initVulkanStructure();
	VkFormatProperties&					props1			= props2.formatProperties;
	const VkFormatProperties			invalidProps	= makeInvalidVulkanStructure<VkFormatProperties2>().formatProperties;
	const VkFormatProperties			emptyProps		{};
	VkImageFormatProperties2			imageProps2		= initVulkanStructure();
	VkImageFormatProperties&			imageProps1		= imageProps2.imageFormatProperties;
	const VkImageFormatProperties		invalidImgProps	= makeInvalidVulkanStructure<VkImageFormatProperties2>().imageFormatProperties;
	const VkImageFormatProperties		emptyImgProps	{};

	VkPhysicalDeviceImageFormatInfo2	imageFormatInfo	= initVulkanStructure();
	imageFormatInfo.format	= VK_FORMAT_UNDEFINED;
	imageFormatInfo.type	= imageType;
	imageFormatInfo.tiling	= tiling;
	imageFormatInfo.usage	= usage;
	imageFormatInfo.flags	= createFlags;

	VkPhysicalDeviceSparseImageFormatInfo2	sparseFormatInfo	= initVulkanStructure();
	sparseFormatInfo.format		= VK_FORMAT_UNDEFINED;
	sparseFormatInfo.type		= imageType;
	sparseFormatInfo.samples	= sampling;
	sparseFormatInfo.usage		= usage;
	sparseFormatInfo.tiling		= tiling;

	const deUint32						n				= 5;
	std::array<bool, n>					verdicts;

	DE_ASSERT(deUint32(m_params.funcID) & HAS_FORMAT_PARAM);

	for (deUint32 i = 0; i < n; ++i)
	{
		const VkFormat format = VkFormat(VK_FORMAT_MAX_ENUM - i);

		switch (m_params.funcID)
		{
		case FuncIDs::DeviceFormatProps:
			props2 = makeInvalidVulkanStructure<VkFormatProperties2>();
			inst.getPhysicalDeviceFormatProperties(dev, format, &props1);
			verdicts[i] = (emptyProps == props1 || invalidProps == props1);
			break;

		case FuncIDs::DeviceFormatPropsSecond:
			props2 = makeInvalidVulkanStructure<VkFormatProperties2>();
			inst.getPhysicalDeviceFormatProperties2(dev, format, &props2);
			verdicts[i] = (emptyProps == props1 || invalidProps == props1);
			break;

		case FuncIDs::DeviceImageFormatProps:
			imageProps2 = makeInvalidVulkanStructure<VkImageFormatProperties2>();
			res = inst.getPhysicalDeviceImageFormatProperties(dev, format, imageType, tiling, usage, createFlags, &imageProps1);
			verdicts[i] = (emptyImgProps == imageProps1 || invalidImgProps == imageProps1);
			break;

		case FuncIDs::DeviceImageFormatPropsSecond:
			imageProps2 = makeInvalidVulkanStructure<VkImageFormatProperties2>();
			imageFormatInfo.format = format;
			res = inst.getPhysicalDeviceImageFormatProperties2(dev, &imageFormatInfo, &imageProps2);
			verdicts[i] = (emptyImgProps == imageProps1 || invalidImgProps == imageProps1);
			break;

		case FuncIDs::DeviceSparseImageFormatProps:
			propsCount = 0;
			inst.getPhysicalDeviceSparseImageFormatProperties(dev, format, imageType, sampling, usage, tiling, &propsCount, nullptr);
			verdicts[i] = (0 == propsCount);
			break;

		case FuncIDs::DeviceSparseImageFormatPropsSecond:
			propsCount = 0;
			sparseFormatInfo.format = format;
			inst.getPhysicalDeviceSparseImageFormatProperties2(dev, &sparseFormatInfo, &propsCount, nullptr);
			verdicts[i] = (0 == propsCount);
			break;

		default: DE_ASSERT(0); break;
		}
	}

	return (VK_ERROR_FORMAT_NOT_SUPPORTED == res && std::all_of(verdicts.begin(), verdicts.end(), [](bool x){ return x; }))
			? tcu::TestStatus::pass("")
			: tcu::TestStatus::fail("");
}

tcu::TestStatus UnsupportedParametersMaintenance5FlagsInstance::iterate (void)
{
	const VkPhysicalDevice				dev				= m_context.getPhysicalDevice();
	const InstanceInterface&			inst			= m_context.getInstanceInterface();
	VkResult							res				= VK_ERROR_FORMAT_NOT_SUPPORTED;
	const VkFormat						format			= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageType					imageType		= VK_IMAGE_TYPE_2D;
	const VkImageTiling					tiling			= VK_IMAGE_TILING_OPTIMAL;
	const VkImageCreateFlags			createFlags		= 0;
	const VkSampleCountFlagBits			sampling		= VK_SAMPLE_COUNT_1_BIT;
	uint32_t							propsCount		= 0;
	VkImageFormatProperties2			imageProps2		= initVulkanStructure();
	VkImageFormatProperties&			imageProps1		= imageProps2.imageFormatProperties;
	const VkImageFormatProperties		invalidImgProps	= makeInvalidVulkanStructure<VkImageFormatProperties2>().imageFormatProperties;
	const VkImageFormatProperties		emptyImgProps	{};

	VkPhysicalDeviceImageFormatInfo2	imageFormatInfo	= initVulkanStructure();
	imageFormatInfo.format	= format;
	imageFormatInfo.type	= imageType;
	imageFormatInfo.tiling	= tiling;
	imageFormatInfo.usage	= VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
	imageFormatInfo.flags	= createFlags;

	VkPhysicalDeviceSparseImageFormatInfo2	sparseFormatInfo	= initVulkanStructure();
	sparseFormatInfo.format		= format;
	sparseFormatInfo.type		= imageType;
	sparseFormatInfo.samples	= sampling;
	sparseFormatInfo.usage		= VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
	sparseFormatInfo.tiling		= tiling;

	const deUint32						n				= 5;
	std::array<bool, n>					verdicts;

	DE_ASSERT(deUint32(m_params.funcID) & HAS_FLAGS_PARAM);

	for (deUint32 i = 0; i < n; ++i)
	{
		const VkImageUsageFlags usage = VkImageUsageFlags(VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM - i);

		switch (m_params.funcID)
		{
		case FuncIDs::DeviceImageFormatProps:
			imageProps2 = makeInvalidVulkanStructure<VkImageFormatProperties2>();
			res = inst.getPhysicalDeviceImageFormatProperties(dev, format, imageType, tiling, usage, createFlags, &imageProps1);
			verdicts[i] = (emptyImgProps == imageProps1 || invalidImgProps == imageProps1);
			break;

		case FuncIDs::DeviceImageFormatPropsSecond:
			imageProps2 = makeInvalidVulkanStructure<VkImageFormatProperties2>();
			imageFormatInfo.usage = usage;
			res = inst.getPhysicalDeviceImageFormatProperties2(dev, &imageFormatInfo, &imageProps2);
			verdicts[i] = (emptyImgProps == imageProps1 || invalidImgProps == imageProps1);
			break;

		case FuncIDs::DeviceSparseImageFormatProps:
			propsCount = 0;
			inst.getPhysicalDeviceSparseImageFormatProperties(dev, format, imageType, sampling, usage, tiling, &propsCount, nullptr);
			/*
			 * Some of the Implementations ignore wrong flags, so at this point we consider the test passed.
			 */
			verdicts[i] = true;
			break;

		case FuncIDs::DeviceSparseImageFormatPropsSecond:
			propsCount = 0;
			sparseFormatInfo.usage = usage;
			inst.getPhysicalDeviceSparseImageFormatProperties2(dev, &sparseFormatInfo, &propsCount, nullptr);
			/*
			 * Some of the Implementations ignore wrong formats, so at this point we consider the test passed.
			 */
			verdicts[i] = true;
			break;

		default: DE_ASSERT(0); break;
		}
	}

	return (VK_ERROR_FORMAT_NOT_SUPPORTED == res && std::all_of(verdicts.begin(), verdicts.end(), [](bool x){ return x; }))
			? tcu::TestStatus::pass("")
			: tcu::TestStatus::fail("");
}

} // unnamed namespace

tcu::TestCaseGroup*	createMaintenance5Tests	(tcu::TestContext& testCtx)
{
	const std::pair<std::string, FuncIDs> funcs[]
	{
		{ "device_format_props",		FuncIDs::DeviceFormatProps	},
		{ "device_format_props2",		FuncIDs::DeviceFormatPropsSecond	},
		{ "image_format_props",			FuncIDs::DeviceImageFormatProps		},
		{ "image_format_props2",		FuncIDs::DeviceImageFormatPropsSecond	},
		{ "sparse_image_format_props",	FuncIDs::DeviceSparseImageFormatProps	},
		{ "sparse_image_format_props2",	FuncIDs::DeviceSparseImageFormatPropsSecond	}
	};
	de::MovePtr<tcu::TestCaseGroup> gRoot(new tcu::TestCaseGroup(testCtx, "maintenance5", "Checks vkGetPhysicalDevice*FormatProperties* API functions"));
	de::MovePtr<tcu::TestCaseGroup> gFormat(new tcu::TestCaseGroup(testCtx, "format", ""));
	de::MovePtr<tcu::TestCaseGroup> gFlags(new tcu::TestCaseGroup(testCtx, "flags", ""));
	for (const auto& func : funcs)
	{
		TestParams p;
		p.funcID = func.second;

		if (deUint32(func.second) & HAS_FORMAT_PARAM)
			gFormat->addChild(new UnsupportedParametersMaintenance5TestCase(testCtx, func.first, p, true));
		if (deUint32(func.second) & HAS_FLAGS_PARAM)
			gFlags->addChild(new UnsupportedParametersMaintenance5TestCase(testCtx, func.first, p, false));
	}
	gRoot->addChild(gFormat.release());
	gRoot->addChild(gFlags.release());
	return gRoot.release();
}

} // api
} // vkt
