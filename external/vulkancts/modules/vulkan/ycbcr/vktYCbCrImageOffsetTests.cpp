/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google Inc.
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
 * \brief YCbCr image offset tests
 *//*--------------------------------------------------------------------*/

#include "vktYCbCrImageOffsetTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktYCbCrUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include <string>
#include <vector>

using tcu::UVec2;
using tcu::TestLog;

using std::string;
using std::vector;

using namespace vk;

namespace vkt
{
namespace ycbcr
{
namespace
{

	struct TestConfig
	{
		TestConfig (const vk::VkFormat format_) : format (format_) {}
		vk::VkFormat format;
	};

	void checkSupport (Context& context, const TestConfig config)
	{
		const vk::VkFormatProperties properties = vk::getPhysicalDeviceFormatProperties(context.getInstanceInterface(),
																						context.getPhysicalDevice(), config.format);

		if ((properties.linearTilingFeatures & vk::VK_FORMAT_FEATURE_DISJOINT_BIT) == 0)
			TCU_THROW(NotSupportedError, "Format doesn't support disjoint planes");
	}

	vk::Move<vk::VkImage> createImage (const vk::DeviceInterface&	vkd,
									   vk::VkDevice					device,
									   vk::VkFormat					format,
									   const UVec2&					size)
	{
		const vk::VkImageCreateInfo createInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			DE_NULL,
			vk::VK_IMAGE_CREATE_DISJOINT_BIT,
			vk::VK_IMAGE_TYPE_2D,
			format,
			vk::makeExtent3D(size.x(), size.y(), 1u),
			1u,
			1u,
			vk::VK_SAMPLE_COUNT_1_BIT,
			vk::VK_IMAGE_TILING_LINEAR,
			vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			vk::VK_SHARING_MODE_EXCLUSIVE,
			0u,
			(const deUint32*)DE_NULL,
			vk::VK_IMAGE_LAYOUT_PREINITIALIZED
		};

		return vk::createImage(vkd, device, &createInfo);
	}

	tcu::TestStatus imageOffsetTest (Context& context, const TestConfig config)
	{
		const vk::DeviceInterface&				vkd						(context.getDeviceInterface());
		const vk::VkDevice						device					(context.getDevice());

		const vk::Unique<vk::VkImage>			srcImage				(createImage(vkd, device, config.format, UVec2(8u, 8u)));
		const vk::MemoryRequirement				srcMemoryRequirement	(vk::MemoryRequirement::HostVisible);
		vector<AllocationSp>					srcImageMemory;

		const deUint32							numPlanes				= getPlaneCount(config.format);
		vector<vk::VkBindImageMemoryInfo>		coreInfos;
		vector<vk::VkBindImagePlaneMemoryInfo>	planeInfos;

		coreInfos.reserve(numPlanes);
		planeInfos.reserve(numPlanes);

		for (deUint32 planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
		{
			const vk::VkImageAspectFlagBits	planeAspect	= (vk::VkImageAspectFlagBits)(vk::VK_IMAGE_ASPECT_PLANE_0_BIT << planeNdx);
			vk::VkMemoryRequirements		reqs		= getImagePlaneMemoryRequirements(vkd, device, srcImage.get(), planeAspect);
			const VkDeviceSize				offset		= deAlign64(reqs.size, reqs.alignment);
			reqs.size *= 2;

			srcImageMemory.push_back(AllocationSp(context.getDefaultAllocator().allocate(reqs, srcMemoryRequirement).release()));

			vk::VkBindImagePlaneMemoryInfo	planeInfo	=
			{
				vk::VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
				DE_NULL,
				planeAspect
			};
			planeInfos.push_back(planeInfo);

			vk::VkBindImageMemoryInfo		coreInfo	=
			{
				vk::VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
				&planeInfos.back(),
				srcImage.get(),
				srcImageMemory.back()->getMemory(),
				offset,
			};
			coreInfos.push_back(coreInfo);
		}

		VK_CHECK(vkd.bindImageMemory2(device, numPlanes, coreInfos.data()));

		vk::VkImageAspectFlags					aspectMasks[3] = {vk::VK_IMAGE_ASPECT_PLANE_0_BIT, vk::VK_IMAGE_ASPECT_PLANE_1_BIT,
																  vk::VK_IMAGE_ASPECT_PLANE_2_BIT};
		for (deUint32 i = 0; i < numPlanes; i++)
		{
			vk::VkSubresourceLayout	subresourceLayout;
			auto					subresource		= vk::makeImageSubresource(aspectMasks[i], 0u, 0u);
			vkd.getImageSubresourceLayout(device, srcImage.get(), &subresource, &subresourceLayout);

			// VkSubresourceLayout::offset is the byte offset from the start of the image or the plane
			// where the image subresource begins. For disjoint images, it should be 0 since each plane
			// has been separately bound to memory.
			if (subresourceLayout.offset != 0)
				return tcu::TestStatus::fail("Failed, subresource layout offset != 0");
		}

		return tcu::TestStatus::pass("Pass");
	}

	void initYcbcrImageOffsetTests (tcu::TestCaseGroup* testGroup)
	{
		const vk::VkFormat ycbcrFormats[] =
		{
			vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
			vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
			vk::VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
			vk::VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
			vk::VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
			vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
			vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
			vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
			vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
			vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
			vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
			vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
			vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
			vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
			vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
			vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
			vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
			vk::VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
			vk::VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
			vk::VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
#ifndef CTS_USES_VULKANSC
			vk::VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
			vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16,
			vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16,
			vk::VK_FORMAT_G16_B16R16_2PLANE_444_UNORM,
#endif // CTS_USES_VULKANSC
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(ycbcrFormats); i++)
		{
			const vk::VkFormat	srcFormat		(ycbcrFormats[i]);
			const string		srcFormatName	(de::toLower(std::string(getFormatName(srcFormat)).substr(10)));

			const TestConfig	config			(srcFormat);
			addFunctionCase(testGroup, srcFormatName.c_str(), "", checkSupport, imageOffsetTest, config);
		}
	}

} // anonymous

tcu::TestCaseGroup* createImageOffsetTests  (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "subresource_offset", "Subresourcelayout::offset tests for YCbCr images", initYcbcrImageOffsetTests);
}

} // ycbcr
} // vkt