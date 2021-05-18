/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Testing writing and reading for mismatched formats
 *//*--------------------------------------------------------------------*/

#include "vktImageLoadStoreTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vktImageLoadStoreUtil.hpp"
#include "vktImageTexture.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageWithMemory.hpp"

#include "deMath.h"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuStringTemplate.hpp"

#include <string>
#include <vector>
#include <map>

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{

struct FormatInfo
{
	const char* GLSLFormat;
	int VectorWidth;
	int BytesPerPixel;
	tcu::TextureChannelClass ChannelClass;
};

FormatInfo getFormatInfo (VkFormat format)
{
	FormatInfo result;

	const tcu::TextureFormat texFormat = mapVkFormat(format);

	result.VectorWidth = getNumUsedChannels(texFormat.order);
	result.BytesPerPixel = getPixelSize(texFormat);
	result.ChannelClass = tcu::getTextureChannelClass(texFormat.type);

	return result;
}

std::string ChannelClassToImageType (tcu::TextureChannelClass channelClass)
{
	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER: return "uimage2D";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER: return "iimage2D";
		default: return "image2D";
	}
}

std::string ChannelClassToVecType (tcu::TextureChannelClass channelClass)
{
	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER: return "uvec4";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER: return "ivec4";
		default: return "vec4";
	}
}

std::string ChannelClassToDefaultVecValue (tcu::TextureChannelClass channelClass)
{
	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER: return "uvec4(1, 10, 100, 1000)";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER: return "ivec4(-1, 2, -1000, 2000)";
		default: return "vec4(0.25, 0.5, 0.0, 1.0)";
	}
}

const std::map<std::string, FormatInfo> SpirvFormats {
	{ "Rgba32f",		{ nullptr,			4, 16,		tcu::TEXTURECHANNELCLASS_FLOATING_POINT } },
	{ "Rg32f",			{ nullptr,			2, 8,		tcu::TEXTURECHANNELCLASS_FLOATING_POINT } },
	{ "R32f",			{ nullptr,			1, 4,		tcu::TEXTURECHANNELCLASS_FLOATING_POINT } },
	{ "Rgba16f",		{ nullptr,			4, 8,		tcu::TEXTURECHANNELCLASS_FLOATING_POINT } },
	{ "Rg16f",			{ nullptr,			2, 4,		tcu::TEXTURECHANNELCLASS_FLOATING_POINT } },
	{ "R16f",			{ nullptr,			1, 2,		tcu::TEXTURECHANNELCLASS_FLOATING_POINT } },
	{ "Rgba16",			{ nullptr,			4, 8,		tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT } },
	{ "Rg16",			{ nullptr,			2, 4,		tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT } },
	{ "R16",			{ nullptr,			1, 2,		tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT } },
	{ "Rgba16Snorm",	{ "rgba16_snorm",	4, 8,		tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT } },
	{ "Rg16Snorm",		{ "rg16_snorm",		2, 4,		tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT } },
	{ "R16Snorm",		{ "r16_snorm",		1, 2,		tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT } },
	{ "Rgb10A2",		{ "rgb10_a2",		4, 4,		tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT } },
	{ "R11fG11fB10f",	{ "r11f_g11f_b10f", 3, 4,		tcu::TEXTURECHANNELCLASS_FLOATING_POINT } },
	{ "Rgba8",			{ nullptr,			4, 4,		tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT } },
	{ "Rg8",			{ nullptr,			2, 2,		tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT } },
	{ "R8",				{ nullptr,			1, 1,		tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT } },
	{ "Rgba8Snorm",		{ "rgba8_snorm",	4, 4,		tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT } },
	{ "Rg8Snorm",		{ "rg8_snorm",		2, 2,		tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT } },
	{ "R8Snorm",		{ "r8_snorm",		1, 1,		tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT } },
	{ "Rgba32i",		{ nullptr,			4, 16,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "Rg32i",			{ nullptr,			2, 2,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "R32i",			{ nullptr,			1, 1,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "Rgba16i",		{ nullptr,			4, 8,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "Rg16i",			{ nullptr,			2, 4,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "R16i",			{ nullptr,			1, 2,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "Rgba8i",			{ nullptr,			4, 4,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "Rg8i",			{ nullptr,			2, 2,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "R8i",			{ nullptr,			1, 1,		tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER } },
	{ "Rgba32ui",		{ nullptr,			4, 16,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "Rg32ui",			{ nullptr,			2, 8,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "R32ui",			{ nullptr,			1, 4,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "Rgba16ui",		{ nullptr,			4, 8,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "Rg16ui",			{ nullptr,			2, 4,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "R16ui",			{ nullptr,			1, 2,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "Rgb10a2ui",		{ "rgb10_a2ui",		4, 4,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "Rgba8ui",		{ nullptr,			4, 4,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "Rg8ui",			{ nullptr,			2, 2,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } },
	{ "R8ui",			{ nullptr,			1, 1,		tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER } }
};

FormatInfo getFormatInfo (const std::string& spirvFormat)
{
	auto it = SpirvFormats.find(spirvFormat);
	if (it != SpirvFormats.end()) return it->second;
	else return {"", 0, 0, tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT};
}

bool matching (VkFormat format, const std::string& spirvFormat)
{
	try
	{
		FormatInfo	baseFormat		=	getFormatInfo(format);
		FormatInfo	shaderFormat	=	getFormatInfo(spirvFormat);

		return (baseFormat.VectorWidth == shaderFormat.VectorWidth &&
				baseFormat.BytesPerPixel == shaderFormat.BytesPerPixel &&
				baseFormat.ChannelClass == shaderFormat.ChannelClass);
	}
	catch (const tcu::InternalError&)
	{
		return false;
	}
}

enum class TestType
{
	READ = 0,
	SPARSE_READ,
	WRITE
};

void fillImageCreateInfo (VkImageCreateInfo& imageCreateInfo, TestType testType, VkFormat format)
{
	const VkImageCreateFlags	imageFlags		= ((testType == TestType::SPARSE_READ) ? (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) : 0u);
	const VkImageCreateInfo		createInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,																// VkStructureType			sType;
		nullptr,																							// const void*				pNext;
		imageFlags,																							// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,																					// VkImageType				imageType;
		format,																								// VkFormat					format;
		makeExtent3D(8, 8, 1),																				// VkExtent3D				extent;
		1u,																									// deUint32					mipLevels;
		1u,																									// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,																				// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,																			// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,																			// VkSharingMode			sharingMode;
		0u,																									// deUint32					queueFamilyIndexCount;
		nullptr,																							// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED																			// VkImageLayout			initialLayout;
	};

	imageCreateInfo = createInfo;
}

class MismatchedFormatTest : public TestCase
{
public:
						MismatchedFormatTest (tcu::TestContext&		testCtx,
											  const std::string&	name,
											  const std::string&	description,
											  const TestType		type,
											  const VkFormat		format,
											  const std::string&	spirvFormat);

	virtual void		checkSupport		(Context&			context) const;
	void				initPrograms		(SourceCollections&	programCollection) const;
	TestInstance*		createInstance		(Context&			context) const;

private:
	const TestType		m_type;
	const VkFormat		m_format;
	const std::string	m_spirvFormat;
};

MismatchedFormatTest::MismatchedFormatTest (tcu::TestContext&	testCtx,
											const std::string&	name,
											const std::string&	description,
											const TestType		type,
											const VkFormat		format,
											const std::string&	spirvFormat)
	: TestCase						(testCtx, name, description)
	, m_type						(type)
	, m_format						(format)
	, m_spirvFormat					(spirvFormat)
{
}

void MismatchedFormatTest::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	if (m_type == TestType::SPARSE_READ)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

		if (!getPhysicalDeviceFeatures(vki, physicalDevice).sparseResidencyBuffer)
			TCU_THROW(NotSupportedError, "Sparse partially resident buffers not supported");

		// Check sparse operations support before creating the image.
		VkImageCreateInfo imageCreateInfo;
		fillImageCreateInfo(imageCreateInfo, m_type, m_format);

		if (!checkSparseImageFormatSupport(physicalDevice, vki, imageCreateInfo))
		{
			TCU_THROW(NotSupportedError, "The image format does not support sparse operations.");
		}

		if (!getPhysicalDeviceFeatures(context.getInstanceInterface(), context.getPhysicalDevice()).shaderResourceResidency)
		{
			TCU_THROW(NotSupportedError, "Shader resource residency not supported");
		}
	}

	VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, m_format);

	if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
	{
		TCU_THROW(NotSupportedError, "Creating storage image with this format is not supported");
	}
}

void MismatchedFormatTest::initPrograms (SourceCollections& programCollection) const
{
	std::string	source;

	if (m_type == TestType::READ)
	{
		source = R"(
			#version 460 core

			layout (${FORMAT}, binding=0) uniform ${IMAGE} inputImage;

			void main()
			{
				${VECT} value = imageLoad(inputImage, ivec2(gl_GlobalInvocationID.xy));
			}
		)";
	}
	else if (m_type == TestType::WRITE)
	{
		source = R"(
			#version 460 core

			layout (${FORMAT}, binding=0) uniform ${IMAGE} inputImage;

			void main()
			{
				imageStore(inputImage, ivec2(gl_GlobalInvocationID.xy), ${VALUE});
			}
		)";
	}
	else if (m_type == TestType::SPARSE_READ)
	{
		source = R"(
			#version 460 core
			#extension GL_ARB_sparse_texture2 : require

			layout (${FORMAT}, binding=0) uniform ${IMAGE} inputImage;

			void main()
			{
				${VECT} result;
				int r = sparseImageLoadARB(inputImage, ivec2(gl_GlobalInvocationID.xy), result);
			}
		)";
	}

	const FormatInfo	spirvFormatInfo		=	getFormatInfo(m_spirvFormat);

	const std::string	glslFormat			=	spirvFormatInfo.GLSLFormat ?
												spirvFormatInfo.GLSLFormat : de::toLower(m_spirvFormat);

	std::map<std::string, std::string>			specializations;

	specializations["FORMAT"]				=	glslFormat;
	specializations["VECT"]					=	ChannelClassToVecType(spirvFormatInfo.ChannelClass);
	specializations["IMAGE"]				=	ChannelClassToImageType(spirvFormatInfo.ChannelClass);
	specializations["VALUE"]				=	ChannelClassToDefaultVecValue(spirvFormatInfo.ChannelClass);

	programCollection.glslSources.add("comp") << glu::ComputeSource( tcu::StringTemplate{source}.specialize(specializations) );
}

class MismatchedFormatTestInstance : public TestInstance
{
public:
						MismatchedFormatTestInstance (Context&				context,
													  const TestType		type,
													  const VkFormat		format,
													  const std::string&	spirvFormat);

	tcu::TestStatus		iterate					(void);

protected:
	const TestType		m_type;
	const VkFormat		m_format;
	const std::string	m_spirvFormat;

};

MismatchedFormatTestInstance::MismatchedFormatTestInstance (Context& context, const TestType type, const VkFormat format, const std::string& spirvFormat)
	: TestInstance		(context)
	, m_type			(type)
	, m_format			(format)
	, m_spirvFormat		(spirvFormat)
{
}

tcu::TestStatus MismatchedFormatTestInstance::iterate (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	auto&							allocator			= m_context.getDefaultAllocator();
	const auto						physicalDevice		= m_context.getPhysicalDevice();
	const auto&						instance			= m_context.getInstanceInterface();

	Move<VkShaderModule>			shaderModule		= createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0);

	Move<VkDescriptorSetLayout>		descriptorSetLayout	= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
															.build(vk, device);
	Move<VkDescriptorPool>			descriptorPool		= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
															.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	Move<VkDescriptorSet>			descriptorSet		= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	Move<VkPipelineLayout>			pipelineLayout		= makePipelineLayout(vk, device, descriptorSetLayout.get());

	Move<VkPipeline>				pipeline			= makeComputePipeline(vk, device, *pipelineLayout, *shaderModule);

	VkImageCreateInfo				imageCreateInfo;
	fillImageCreateInfo(imageCreateInfo, m_type, m_format);

	vk::Move<vk::VkImage>			storageImage		= createImage(vk, device, &imageCreateInfo);
	const auto						tcuFormat			= mapVkFormat(m_format);

	de::MovePtr<vk::Allocation>					storageAllocation;
	vk::Move<vk::VkSemaphore>					bindSemaphore;
	std::vector<de::SharedPtr<Allocation> >		allocations;

	if (m_type == TestType::SPARSE_READ)
	{
		bindSemaphore = createSemaphore(vk, device);

		allocateAndBindSparseImage(	vk, device, physicalDevice, instance,
									imageCreateInfo, *bindSemaphore, m_context.getSparseQueue(),
									allocator, allocations, tcuFormat, *storageImage	);
	}
	else
	{
		storageAllocation = allocator.allocate(getImageMemoryRequirements(vk, device, *storageImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(device, *storageImage, storageAllocation->getMemory(), storageAllocation->getOffset()));
	}

	const auto						subresourceRange	= makeImageSubresourceRange(getImageAspectFlags(tcuFormat), 0u, 1u, 0u, 1u);
	Move<VkImageView>				storageImageView	= makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_format, subresourceRange);
	VkDescriptorImageInfo			storageImageInfo	= makeDescriptorImageInfo(DE_NULL, *storageImageView, VK_IMAGE_LAYOUT_GENERAL);

	DescriptorSetUpdateBuilder		builder;
	builder
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageImageInfo)
		.update(vk, device);

	Move<VkCommandPool>				cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const auto						layoutBarrier		= makeImageMemoryBarrier(0u, (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *storageImage, subresourceRange);

	beginCommandBuffer(vk, *cmdBuffer);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &layoutBarrier);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, 8, 8, 1);
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	return tcu::TestStatus::pass("Passed");
}

TestInstance* MismatchedFormatTest::createInstance (Context& context) const
{
	return new MismatchedFormatTestInstance(context, m_type, m_format, m_spirvFormat);
}

} // anonymous ns

tcu::TestCaseGroup* createImageMismatchedFormatsTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "mismatched_formats", "Test image load/store operations on mismatched formats"));
	de::MovePtr<tcu::TestCaseGroup> testGroupOpRead(new tcu::TestCaseGroup(testCtx, "image_read", "perform OpImageRead"));
	de::MovePtr<tcu::TestCaseGroup> testGroupOpWrite(new tcu::TestCaseGroup(testCtx, "image_write", "perform OpImageWrite"));
	de::MovePtr<tcu::TestCaseGroup> testGroupOpSparseRead(new tcu::TestCaseGroup(testCtx, "sparse_image_read", "perform OpSparseImageRead"));

	for (VkFormat format = VK_FORMAT_R4G4_UNORM_PACK8; format < VK_CORE_FORMAT_LAST; format = static_cast<VkFormat>(format+1))
	{
		for (auto& pair : SpirvFormats)
		{
			const std::string&	spirvFormat = pair.first;

			if (matching(format, spirvFormat))
			{
				const std::string	enumName	= getFormatName(format);
				const std::string	testName	= de::toLower( enumName.substr(10) + "_with_" + spirvFormat );

				testGroupOpRead->addChild(new MismatchedFormatTest(	testCtx, testName, "",
																	TestType::READ,
																	format, spirvFormat) );

				testGroupOpWrite->addChild(new MismatchedFormatTest(testCtx, testName, "",
																	TestType::WRITE,
																	format, spirvFormat) );

				testGroupOpSparseRead->addChild(new MismatchedFormatTest(	testCtx, testName, "",
																			TestType::SPARSE_READ,
																			format, spirvFormat) );
			}
		}
	}

	testGroup->addChild(testGroupOpRead.release());
	testGroup->addChild(testGroupOpWrite.release());
	testGroup->addChild(testGroupOpSparseRead.release());

	return testGroup.release();
}

} // image
} // vkt
