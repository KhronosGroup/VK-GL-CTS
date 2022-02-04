/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 * Copyright (c) 2019-2020 NVIDIA Corporation
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
 * \brief Fragment Shading Rate tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentShadingRateTests.hpp"
#include "vktFragmentShadingRateBasic.hpp"
#include "vktFragmentShadingRatePixelConsistency.hpp"
#include "vktAttachmentRateTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "tcuTestLog.hpp"
#include <limits>

namespace vkt
{
namespace FragmentShadingRate
{

namespace
{

tcu::TestStatus testLimits(Context& context)
{
	bool			allChecksPassed					= true;
	tcu::TestLog&	log								= context.getTestContext().getLog();
	const auto&		features						= context.getDeviceFeatures();
	const auto&		properties						= context.getDeviceProperties();
	const auto&		vulkan12Features				= context.getDeviceVulkan12Features();
	const auto&		fragmentShadingRateFeatures		= context.getFragmentShadingRateFeatures();
	const auto&		fragmentShadingRateProperties	= context.getFragmentShadingRateProperties();

	if (!fragmentShadingRateFeatures.pipelineFragmentShadingRate)
	{
		log << tcu::TestLog::Message << "pipelineFragmentShadingRate is not supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (context.getFragmentShadingRateProperties().primitiveFragmentShadingRateWithMultipleViewports && !context.getFragmentShadingRateFeatures().primitiveFragmentShadingRate)
	{
		log << tcu::TestLog::Message << "primitiveFragmentShadingRateWithMultipleViewports "
										"limit should only be supported if primitiveFragmentShadingRate is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	bool requiredFeatures = features.geometryShader || vulkan12Features.shaderOutputViewportIndex || context.isDeviceFunctionalitySupported("VK_EXT_shader_viewport_index_layer");
	if (context.getFragmentShadingRateProperties().primitiveFragmentShadingRateWithMultipleViewports && !requiredFeatures)
	{
		log << tcu::TestLog::Message << "primitiveFragmentShadingRateWithMultipleViewports limit should only "
										"be supported if at least one of the geometryShader feature, shaderOutputViewportIndex feature, "
										"or VK_EXT_shader_viewport_index_layer extension is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.layeredShadingRateAttachments && !fragmentShadingRateFeatures.attachmentFragmentShadingRate)
	{
		log << tcu::TestLog::Message << "layeredShadingRateAttachments should only be supported if attachmentFragmentShadingRate is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	requiredFeatures = features.geometryShader || context.getMultiviewFeatures().multiview || vulkan12Features.shaderOutputViewportIndex ||
						context.isDeviceFunctionalitySupported("VK_EXT_shader_viewport_index_layer");
	if (fragmentShadingRateProperties.layeredShadingRateAttachments && !requiredFeatures)
	{
		log << tcu::TestLog::Message << "layeredShadingRateAttachments should only be supported if at least one of the geometryShader feature, multiview feature, "
										"shaderOutputViewportIndex feature, or VK_EXT_shader_viewport_index_layer extension is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	requiredFeatures = fragmentShadingRateFeatures.primitiveFragmentShadingRate || fragmentShadingRateFeatures.attachmentFragmentShadingRate;
	if (fragmentShadingRateProperties.fragmentShadingRateNonTrivialCombinerOps && !requiredFeatures)
	{
		log << tcu::TestLog::Message << "fragmentShadingRateNonTrivialCombinerOps should only be supported if at least one of primitiveFragmentShadingRate "
										"or attachmentFragmentShadingRate is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.maxFragmentSizeAspectRatio > std::max(fragmentShadingRateProperties.maxFragmentSize.width, fragmentShadingRateProperties.maxFragmentSize.height))
	{
		log << tcu::TestLog::Message << "maxFragmentSizeAspectRatio should be less than or equal to the maximum width / height of maxFragmentSize" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.maxFragmentSizeAspectRatio < 2)
	{
		log << tcu::TestLog::Message << "maxFragmentSizeAspectRatio should be at least 2" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (!deIntIsPow2(static_cast<int>(fragmentShadingRateProperties.maxFragmentSizeAspectRatio)))
	{
		log << tcu::TestLog::Message << "maxFragmentSizeAspectRatio should be power of 2" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.fragmentShadingRateWithShaderSampleMask && (fragmentShadingRateProperties.maxFragmentShadingRateCoverageSamples > (properties.limits.maxSampleMaskWords * 32)))
	{
		log << tcu::TestLog::Message << "maxFragmentShadingRateCoverageSamples should be less than or equal maxSampleMaskWords * 32 "
										"if fragmentShadingRateWithShaderSampleMask is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	deUint32 requiredValue = fragmentShadingRateProperties.maxFragmentSize.width * fragmentShadingRateProperties.maxFragmentSize.height *
								fragmentShadingRateProperties.maxFragmentShadingRateRasterizationSamples;
	if (fragmentShadingRateProperties.maxFragmentShadingRateCoverageSamples > requiredValue)
	{
		log << tcu::TestLog::Message << "maxFragmentShadingRateCoverageSamples should be less than or equal to the product of the width and height of "
										"maxFragmentSize and the samples reported by maxFragmentShadingRateRasterizationSamples" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.maxFragmentShadingRateCoverageSamples < 16)
	{
		log << tcu::TestLog::Message << "maxFragmentShadingRateCoverageSamples should at least be 16" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.maxFragmentShadingRateRasterizationSamples < vk::VK_SAMPLE_COUNT_4_BIT)
	{
		log << tcu::TestLog::Message << "maxFragmentShadingRateRasterizationSamples should supports at least VK_SAMPLE_COUNT_4_BIT" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.fragmentShadingRateWithConservativeRasterization && !context.isDeviceFunctionalitySupported("VK_EXT_conservative_rasterization"))
	{
		log << tcu::TestLog::Message << "fragmentShadingRateWithConservativeRasterization should only be supported if VK_EXT_conservative_rasterization is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.fragmentShadingRateWithFragmentShaderInterlock && !context.isDeviceFunctionalitySupported("VK_EXT_fragment_shader_interlock"))
	{
		log << tcu::TestLog::Message << "fragmentShadingRateWithFragmentShaderInterlock should only be supported if VK_EXT_fragment_shader_interlock is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateProperties.fragmentShadingRateWithCustomSampleLocations && !context.isDeviceFunctionalitySupported("VK_EXT_sample_locations"))
	{
		log << tcu::TestLog::Message << "fragmentShadingRateWithCustomSampleLocations should only be supported if VK_EXT_sample_locations is supported" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (fragmentShadingRateFeatures.attachmentFragmentShadingRate)
	{
		if ((fragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.width < 8) ||
			(fragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.height < 8))
		{
			log << tcu::TestLog::Message << "maxFragmentShadingRateAttachmentTexelSize should at least be { 8,8 }" << tcu::TestLog::EndMessage;
			allChecksPassed = false;
		}

		if ((fragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.width > 32) ||
			(fragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.height > 32))
		{
			log << tcu::TestLog::Message << "minFragmentShadingRateAttachmentTexelSize should't be greater than { 32,32 }" << tcu::TestLog::EndMessage;
			allChecksPassed = false;
		}

		if ((fragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.width < fragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.width) ||
			(fragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.height < fragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.height))
		{
			log << tcu::TestLog::Message << "maxFragmentShadingRateAttachmentTexelSize should be greater than or equal to "
				"minFragmentShadingRateAttachmentTexelSize in each dimension" << tcu::TestLog::EndMessage;
			allChecksPassed = false;
		}

		if (!deIntIsPow2(static_cast<int>(fragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.width)) ||
			!deIntIsPow2(static_cast<int>(fragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.height)))
		{
			log << tcu::TestLog::Message << "maxFragmentShadingRateAttachmentTexelSize should be power of 2" << tcu::TestLog::EndMessage;
			allChecksPassed = false;
		}

		if (!deIntIsPow2(static_cast<int>(fragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.width)) ||
			!deIntIsPow2(static_cast<int>(fragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.height)))
		{
			log << tcu::TestLog::Message << "minFragmentShadingRateAttachmentTexelSize should be power of 2" << tcu::TestLog::EndMessage;
			allChecksPassed = false;
		}
	}
	else
	{
		if ((fragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.width != 0) ||
			(fragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.height != 0))
		{
			log << tcu::TestLog::Message << "maxFragmentShadingRateAttachmentTexelSize should be { 0,0 } when "
											"attachmentFragmentShadingRate is not supported" << tcu::TestLog::EndMessage;
			allChecksPassed = false;
		}

		if ((fragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.width != 0) ||
			(fragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.height != 0))
		{
			log << tcu::TestLog::Message << "minFragmentShadingRateAttachmentTexelSize should be { 0,0 } when "
											"attachmentFragmentShadingRate is not supported" << tcu::TestLog::EndMessage;
			allChecksPassed = false;
		}
	}

	if ((fragmentShadingRateProperties.maxFragmentSize.width < 2) ||
		(fragmentShadingRateProperties.maxFragmentSize.height < 2))
	{
		log << tcu::TestLog::Message << "maxFragmentSize should at least be { 2,2 }" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if ((fragmentShadingRateProperties.maxFragmentSize.width > 4) ||
		(fragmentShadingRateProperties.maxFragmentSize.height > 4))
	{
		log << tcu::TestLog::Message << "maxFragmentSize should't be greater than{ 4,4 }" << tcu::TestLog::EndMessage;
		allChecksPassed = false;
	}

	if (allChecksPassed)
		return tcu::TestStatus::pass("pass");
	return tcu::TestStatus::fail("fail");
}

tcu::TestStatus testShadingRates(Context& context)
{
	bool							someChecksFailed					= false;
	tcu::TestLog&					log									= context.getTestContext().getLog();
	const vk::InstanceInterface&	vki									= context.getInstanceInterface();
	vk::VkPhysicalDevice			physicalDevice						= context.getPhysicalDevice();
	const auto&						fragmentShadingRateProperties		= context.getFragmentShadingRateProperties();
	deUint32						supportedFragmentShadingRateCount	= 0;

	vk::VkResult result = vki.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &supportedFragmentShadingRateCount, DE_NULL);
	if ((result != vk::VK_SUCCESS) && (result != vk::VK_ERROR_OUT_OF_HOST_MEMORY))
	{
		someChecksFailed = true;
		log << tcu::TestLog::Message << "vkGetPhysicalDeviceFragmentShadingRatesKHR returned invalid result" << tcu::TestLog::EndMessage;
	}

	std::vector<vk::VkPhysicalDeviceFragmentShadingRateKHR> fragmentShadingRateVect(supportedFragmentShadingRateCount);
	for (auto& fragmentShadingRate : fragmentShadingRateVect)
	{
		fragmentShadingRate.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR;
		fragmentShadingRate.pNext = DE_NULL;
	}

	// Pass a value of 1 into pFragmentShadingRateCount, and an array of at least length one into pFragmentShadingRates.
	// Check that the returned value is either VK_INCOMPLETE or VK_ERROR_OUT_OF_HOST_MEMORY(and issue a quality warning in the latter case).
	supportedFragmentShadingRateCount = 1u;
	result = vki.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &supportedFragmentShadingRateCount, fragmentShadingRateVect.data());
	if ((result != vk::VK_INCOMPLETE) && (result != vk::VK_ERROR_OUT_OF_HOST_MEMORY))
	{
		someChecksFailed = true;
		log << tcu::TestLog::Message << "vkGetPhysicalDeviceFragmentShadingRatesKHR returned invalid result" << tcu::TestLog::EndMessage;
	}

	// Get all available fragment shading rates
	supportedFragmentShadingRateCount = static_cast<deUint32>(fragmentShadingRateVect.size());
	result = vki.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &supportedFragmentShadingRateCount, fragmentShadingRateVect.data());
	if ((result != vk::VK_SUCCESS) && (result != vk::VK_ERROR_OUT_OF_HOST_MEMORY))
	{
		someChecksFailed = true;
		log << tcu::TestLog::Message << "vkGetPhysicalDeviceFragmentShadingRatesKHR returned invalid result" << tcu::TestLog::EndMessage;
	}

	bool		widthCheckPassed	= true;
	bool		heightCheckPassed	= true;
	deUint32	previousWidth		= std::numeric_limits<deUint32>::max();
	deUint32	previousHeight		= std::numeric_limits<deUint32>::max();

	for (const auto& fsr : fragmentShadingRateVect)
	{
		const auto& fragmentSize = fsr.fragmentSize;

		// Check that rate width and height are power-of-two
		if (!deIntIsPow2(static_cast<int>(fragmentSize.width)) ||
			!deIntIsPow2(static_cast<int>(fragmentSize.height)))
		{
			log << tcu::TestLog::Message << "fragmentSize should be power of 2" << tcu::TestLog::EndMessage;
			someChecksFailed = true;
		}

		// Check that the width and height are less than the values in the maxFragmentSize limit
		if ((fragmentSize.width > fragmentShadingRateProperties.maxFragmentSize.width) ||
			(fragmentSize.height > fragmentShadingRateProperties.maxFragmentSize.height))
		{
			log << tcu::TestLog::Message << "fragmentSize width and height are not less than the values in the maxFragmentSize" << tcu::TestLog::EndMessage;
			someChecksFailed = true;
		}

		if ((fragmentSize.width * fragmentSize.height) == 1)
		{
			// special case for fragmentSize {1, 1}
			if (fsr.sampleCounts != ~0u)
			{
				log << tcu::TestLog::Message << "implementations must support sampleCounts equal to ~0 for fragmentSize {1, 1}" << tcu::TestLog::EndMessage;
				someChecksFailed = true;
			}
		}
		else
		{
			// get highest sample count value
			deUint32 highestSampleCount = 0x80000000;
			while (highestSampleCount)
			{
				if (fsr.sampleCounts & highestSampleCount)
					break;
				highestSampleCount >>= 1;
			}

			// Check that the highest sample count in sampleCounts is less than or equal to maxFragmentShadingRateRasterizationSamples limit
			if (highestSampleCount > static_cast<deUint32>(fragmentShadingRateProperties.maxFragmentShadingRateRasterizationSamples))
			{
				log << tcu::TestLog::Message << "highest sample count value is not less than or equal to the maxFragmentShadingRateRasterizationSamples limit" << tcu::TestLog::EndMessage;
				someChecksFailed = true;
			}

			// Check that the product of the width, height, and highest sample count value is less than the maxFragmentShadingRateCoverageSamples limit
			if ((fragmentSize.width * fragmentSize.height * highestSampleCount) > fragmentShadingRateProperties.maxFragmentShadingRateCoverageSamples)
			{
				log << tcu::TestLog::Message << "product of the width, height, and highest sample count value is not less than the maxFragmentShadingRateCoverageSamples limit" << tcu::TestLog::EndMessage;
				someChecksFailed = true;
			}
		}

		// Check that the entries in the array are ordered first by largest to smallest width, then largest to smallest height
		{
			const deUint32 currentWidth = fragmentSize.width;
			if (widthCheckPassed && (currentWidth > previousWidth))
			{
				log << tcu::TestLog::Message << "vkGetPhysicalDeviceFragmentShadingRatesKHR returned entries that are not ordered by largest to smallest width" << tcu::TestLog::EndMessage;
				widthCheckPassed = false;
			}

			deUint32 currentHeight = fragmentSize.height;
			if (heightCheckPassed)
			{
				// we can check order of height only for entries that have same width
				if (currentWidth == previousWidth)
				{
					if (currentHeight > previousHeight)
					{
						log << tcu::TestLog::Message << "vkGetPhysicalDeviceFragmentShadingRatesKHR returned entries with same width but height is not ordered by largest to smallest" << tcu::TestLog::EndMessage;
						heightCheckPassed = false;
					}
				}
				else
					currentHeight = std::numeric_limits<deUint32>::max();
			}

			previousWidth = currentWidth;
			previousHeight = currentHeight;
		}

		// Check that no two entries in the array have the same fragmentSize.width and fragmentSize.height value
		{
			deUint32 count = 0;
			for (const auto& fsrB : fragmentShadingRateVect)
			{
				if ((fragmentSize.width  == fsrB.fragmentSize.width) &&
					(fragmentSize.height == fsrB.fragmentSize.height))
				{
					if (++count > 1)
					{
						log << tcu::TestLog::Message << "vkGetPhysicalDeviceFragmentShadingRatesKHR returned entries with same fragmentSize" << tcu::TestLog::EndMessage;
						someChecksFailed = true;
						break;
					}
				}
			}
		}

		// Check that 1x1, 1x2, 2x1, and 2x2 rates are supported with sample counts of 1 and 4
		if ((fragmentSize.width < 3) && (fragmentSize.height < 3) &&
			(!(fsr.sampleCounts & vk::VK_SAMPLE_COUNT_1_BIT) || !(fsr.sampleCounts & vk::VK_SAMPLE_COUNT_4_BIT)))
		{
			log << tcu::TestLog::Message << "vkGetPhysicalDeviceFragmentShadingRatesKHR returned 1x1, 1x2, 2x1, and 2x2 rates with sample counts not supporting 1 and 4" << tcu::TestLog::EndMessage;
			someChecksFailed = true;
		}

		// If the framebufferColorSampleCounts limit includes a sample count of 2, ensure that a sample count of 2 is also reported for the 1x1, 1x2, 2x1, and 2x2 rates.
		if (context.getDeviceProperties().limits.framebufferColorSampleCounts & vk::VK_SAMPLE_COUNT_2_BIT)
		{
			if ((fragmentSize.width < 3) && (fragmentSize.height < 3) &&
				!(fsr.sampleCounts & vk::VK_SAMPLE_COUNT_2_BIT))
			{
				log << tcu::TestLog::Message << "vkGetPhysicalDeviceFragmentShadingRatesKHR returned 1x1, 1x2, 2x1, and 2x2 rates with sample counts not supporting 2 while framebufferColorSampleCounts does" << tcu::TestLog::EndMessage;
				someChecksFailed = true;
			}
		}
	}

	if (someChecksFailed || !widthCheckPassed || !heightCheckPassed)
		return tcu::TestStatus::fail("fail");

	return tcu::TestStatus::pass("pass");
}

void checkSupport(Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");
}

void createMiscTests(tcu::TestContext& testCtx, tcu::TestCaseGroup* parentGroup)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "misc", ""));

	addFunctionCase(group.get(), "limits",			"", checkSupport, testLimits);
	addFunctionCase(group.get(), "shading_rates",	"", checkSupport, testShadingRates);

	parentGroup->addChild(group.release());
}

void createChildren (tcu::TestCaseGroup* group, bool useDynamicRendering)
{
	tcu::TestContext&	testCtx		= group->getTestContext();
	createBasicTests(testCtx, group, useDynamicRendering);
	createAttachmentRateTests(testCtx, group, useDynamicRendering);

	if (!useDynamicRendering)
	{
		// there is no point in duplicating those tests for dynamic rendering
		createMiscTests(testCtx, group);

		// subpasses can't be translated to dynamic rendering
		createPixelConsistencyTests(testCtx, group);
	}
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> mainGroup				(new tcu::TestCaseGroup(testCtx, "fragment_shading_rate", "Fragment shading rate tests"));
	de::MovePtr<tcu::TestCaseGroup> renderpass2Group		(createTestGroup(testCtx, "renderpass2", "Draw using render pass object", createChildren, false));
	de::MovePtr<tcu::TestCaseGroup> dynamicRenderingGroup	(createTestGroup(testCtx, "dynamic_rendering", "Draw using VK_KHR_dynamic_rendering", createChildren, true));

	mainGroup->addChild(renderpass2Group.release());
	mainGroup->addChild(dynamicRenderingGroup.release());

	return mainGroup.release();
}

} // FragmentShadingRate
} // vkt
