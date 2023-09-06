/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Valve Corporation.
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
 * \brief
 *//*--------------------------------------------------------------------*/

#include "vktPipelineImageSlicedViewOf3DTests.hpp"
#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTexture.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deRandom.hpp"

#include <sstream>
#include <vector>
#include <tuple>
#include <set>
#include <limits>
#include <string>
#include <algorithm>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

constexpr uint32_t	kWidth			= 8u;
constexpr uint32_t	kHeight			= 8u;
constexpr VkFormat	kFormat			= VK_FORMAT_R8G8B8A8_UINT;
constexpr uint32_t	kVertexCount	= 3u;
constexpr auto		kUsageLayout	= VK_IMAGE_LAYOUT_GENERAL;

enum class TestType
{
	LOAD = 0,
	STORE,
};

struct TestParams
{
	TestType				testType;
	VkShaderStageFlagBits	stage;
	uint32_t				width;
	uint32_t				height;
	uint32_t				depth;
	uint32_t				offset;

private:
	// We want to test both normal ranges and VK_REMAINING_3D_SLICES_EXT, but in the latter case we cannot blindly use the range
	// value for some operations. See getActualRange() and getSlicedViewRange().
	uint32_t				range;

public:
	tcu::Maybe<uint32_t>	mipLevel;
	bool					sampleImg;

	TestParams (TestType testType_, VkShaderStageFlagBits stage_, uint32_t width_, uint32_t height_, uint32_t depth_, uint32_t offset_, uint32_t range_,
				const tcu::Maybe<uint32_t>& mipLevel_, bool sampleImg_)
		: testType	(testType_)
		, stage		(stage_)
		, width		(width_)
		, height	(height_)
		, depth		(depth_)
		, offset	(offset_)
		, range		(range_)
		, mipLevel	(mipLevel_)
		, sampleImg	(sampleImg_)
	{
		DE_ASSERT(stage == VK_SHADER_STAGE_COMPUTE_BIT || stage == VK_SHADER_STAGE_FRAGMENT_BIT);
		DE_ASSERT(range > 0u);

		const auto selectedLevel = getSelectedLevel();

		if (useMipMaps())
		{
			// To simplify things.
			DE_ASSERT(width == height && width == depth);

			const auto maxMipLevelCount	= getMaxMipLevelCount();
			DE_ASSERT(selectedLevel < maxMipLevelCount);
			DE_UNREF(maxMipLevelCount); // For release builds.
		}

		const uint32_t selectedLevelDepth = (depth >> selectedLevel);
		DE_UNREF(selectedLevelDepth); // For release builds.

		if (!useRemainingSlices())
			DE_ASSERT(offset + range <= selectedLevelDepth);
		else
			DE_ASSERT(offset < selectedLevelDepth);
	}

	uint32_t getSelectedLevel (void) const
	{
		return (useMipMaps() ? mipLevel.get() : 0u);
	}

	uint32_t getFullImageLevels (void) const
	{
		return (useMipMaps() ? getMaxMipLevelCount() : 1u);
	}

	uint32_t getActualRange (void) const
	{
		const auto levelDepth = (depth >> getSelectedLevel());
		DE_ASSERT(levelDepth > 0u);

		return (useRemainingSlices() ? (levelDepth - offset) : range);
	}

	uint32_t getSlicedViewRange (void) const
	{
		return range;
	}

	VkExtent3D getSliceExtent (void) const
	{
		const auto selectedLevel	= getSelectedLevel();
		const auto extent			= makeExtent3D((width >> selectedLevel),
												   (height >> selectedLevel),
												   getActualRange());

		DE_ASSERT(extent.width > 0u);
		DE_ASSERT(extent.height > 0u);
		DE_ASSERT(extent.depth > 0u);
		return extent;
	}

	VkExtent3D getFullLevelExtent (void) const
	{
		const auto selectedLevel	= getSelectedLevel();
		const auto extent			= makeExtent3D((width >> selectedLevel),
												   (height >> selectedLevel),
												   (depth >> selectedLevel));

		DE_ASSERT(extent.width > 0u);
		DE_ASSERT(extent.height > 0u);
		DE_ASSERT(extent.depth > 0u);
		return extent;
	}

	static uint32_t getMaxMipLevelCountForSize (uint32_t size)
	{
		DE_ASSERT(size <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
		return static_cast<uint32_t>(deLog2Floor32(static_cast<int32_t>(size)) + 1);
	}

private:
	uint32_t getMaxMipLevelCount (void) const
	{
		return getMaxMipLevelCountForSize(depth);
	}

	bool useMipMaps (void) const
	{
		return static_cast<bool>(mipLevel);
	}

	bool useRemainingSlices (void) const
	{
		return (range == VK_REMAINING_3D_SLICES_EXT);
	}
};

class SlicedViewTestCase : public vkt::TestCase
{
public:
						SlicedViewTestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
							: vkt::TestCase(testCtx, name, description), m_params(params) {}
	virtual				~SlicedViewTestCase		(void) {}

	void				initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*		createInstance			(Context& context) const override;
	void				checkSupport			(Context& context) const override;

protected:
	const TestParams	m_params;
};

class SlicedViewTestInstance : public vkt::TestInstance
{
public:
						SlicedViewTestInstance	(Context& context, const TestParams& params)
							: vkt::TestInstance(context), m_params (params)
							{}
	virtual				~SlicedViewTestInstance	(void) {}

protected:
	virtual void		runPipeline				(const DeviceInterface& vkd, const VkDevice device, const VkCommandBuffer cmdBuffer, const VkImageView slicedImage, const VkImageView auxiliarImage);
	virtual void		runGraphicsPipeline		(const DeviceInterface& vkd, const VkDevice device, const VkCommandBuffer cmdBuffer);
	virtual void		runComputePipeline		(const DeviceInterface& vkd, const VkDevice device, const VkCommandBuffer cmdBuffer);
	bool				runSamplingPipeline		(const VkImage fullImage, const VkImageView slicedView, const VkExtent3D& levelExtent);

	const TestParams	m_params;

	Move<VkDescriptorSetLayout>	m_setLayout;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSet>		m_descriptorSet;
	Move<VkPipelineLayout>		m_pipelineLayout;

	// Only for graphics pipelines.
	Move<VkRenderPass>			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;

	Move<VkPipeline>			m_pipeline;
};

class SlicedViewLoadTestInstance : public SlicedViewTestInstance
{
public:
					SlicedViewLoadTestInstance	(Context& context, const TestParams& params) : SlicedViewTestInstance(context, params) {}
	virtual			~SlicedViewLoadTestInstance	(void) {}

	tcu::TestStatus	iterate						(void);
};

class SlicedViewStoreTestInstance : public SlicedViewTestInstance
{
public:
					SlicedViewStoreTestInstance		(Context& context, const TestParams& params) : SlicedViewTestInstance(context, params) {}
	virtual			~SlicedViewStoreTestInstance	(void) {}

	tcu::TestStatus	iterate							(void);
};

void SlicedViewTestCase::checkSupport (Context &context) const
{
	context.requireDeviceFunctionality(VK_EXT_IMAGE_SLICED_VIEW_OF_3D_EXTENSION_NAME);

	if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
}

void SlicedViewTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
	const std::string bindings =
		"layout (rgba8ui, set=0, binding=0) uniform uimage3D slicedImage;\n"
		"layout (rgba8ui, set=0, binding=1) uniform uimage3D auxiliarImage;\n"
		;

	std::string loadFrom;
	std::string storeTo;

	// We may need to load stuff from the sliced image into an auxiliary image if we're testing load, or we may need to store stuff
	// to the sliced image, read from the auxiliary image if we're testing stores.
	if (m_params.testType == TestType::LOAD)
	{
		loadFrom	= "slicedImage";
		storeTo		= "auxiliarImage";
	}
	else if (m_params.testType == TestType::STORE)
	{
		loadFrom	= "auxiliarImage";
		storeTo		= "slicedImage";
	}
	else
		DE_ASSERT(false);

	std::ostringstream mainOperation;

	// Note: "coords" will vary depending on the shader stage.
	mainOperation
		<< "    const ivec3 size = imageSize(slicedImage);\n"
		<< "    const uvec4 badColor = uvec4(0, 0, 0, 0);\n"
		<< "    const uvec4 goodColor = imageLoad(" << loadFrom << ", coords);\n"
		<< "    const uvec4 storedColor = ((size.z == " << m_params.getActualRange() << ") ? goodColor : badColor);\n"
		<< "    imageStore(" << storeTo << ", coords, storedColor);\n"
		;

	if (m_params.stage == VK_SHADER_STAGE_COMPUTE_BIT)
	{
		// For compute, we'll launch as many workgroups as slices, and each invocation will handle one pixel.
		const auto sliceExtent = m_params.getSliceExtent();
		std::ostringstream comp;
		comp
			<< "#version 460\n"
			<< "layout (local_size_x=" << sliceExtent.width << ", local_size_y=" << sliceExtent.height << ", local_size_z=1) in;\n"
			<< bindings
			<< "void main (void) {\n"
			<< "    const ivec3 coords = ivec3(ivec2(gl_LocalInvocationID.xy), int(gl_WorkGroupID.x));\n"
			<< mainOperation.str()
			<< "}\n"
			;
		programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
	}
	else if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		// For fragment, we'll draw as many instances as slices, and each draw will use a full-screen triangle to generate as many
		// fragment shader invocations as pixels in the image (the framebuffer needs to have the same size as the storage images).
		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "layout (location=0) in flat int zCoord;\n"
			<< bindings
			<< "void main (void) {\n"
			<< "    const ivec3 coords = ivec3(ivec2(gl_FragCoord.xy), zCoord);\n"
			<< mainOperation.str()
			<< "}\n"
			;

		std::ostringstream vert;
		vert
			<< "#version 460\n"
			<< "layout (location=0) out flat int zCoord;\n"
			<< "vec2 positions[3] = vec2[](\n"
			<< "    vec2(-1.0, -1.0),\n"
			<< "    vec2( 3.0, -1.0),\n"
			<< "    vec2(-1.0,  3.0)\n"
			<< ");\n"
			<< "void main() {\n"
			<< "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
			<< "    zCoord = int(gl_InstanceIndex);\n"
			<< "}\n"
			;

		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
	else
	{
		DE_ASSERT(false);
	}

	if (m_params.sampleImg)
	{
		// Prepare a compute shader that will sample the whole level to verify it's available.
		const auto levelExtent = m_params.getFullLevelExtent();

		std::ostringstream comp;
		comp
			<< "#version 460\n"
			<< "layout (local_size_x=" << levelExtent.width << ", local_size_y=" << levelExtent.height << ", local_size_z=" << levelExtent.depth << ") in;\n"
			<< "layout (set=0, binding=0) uniform usampler3D combinedSampler;\n"		// The image being tested.
			<< "layout (set=0, binding=1, rgba8ui) uniform uimage3D auxiliarImage;\n"	// Verification storage image.
			<< "void main() {\n"
			<< "    const vec3 levelExtent = vec3(" << levelExtent.width << ", " << levelExtent.height << ", " << levelExtent.depth << ");\n"
			<< "    const vec3 sampleCoords = vec3(\n"
			<< "        (float(gl_LocalInvocationID.x) + 0.5) / levelExtent.x,\n"
			<< "        (float(gl_LocalInvocationID.y) + 0.5) / levelExtent.y,\n"
			<< "        (float(gl_LocalInvocationID.z) + 0.5) / levelExtent.z);\n"
			<< "    const ivec3 storeCoords = ivec3(int(gl_LocalInvocationID.x), int(gl_LocalInvocationID.y), int(gl_LocalInvocationID.z));\n"
			<< "    const uvec4 sampledColor = texture(combinedSampler, sampleCoords);\n"
			<< "    imageStore(auxiliarImage, storeCoords, sampledColor);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("compSample") << glu::ComputeSource(comp.str());
	}
}

TestInstance* SlicedViewTestCase::createInstance (Context& context) const
{
	if (m_params.testType == TestType::LOAD)
		return new SlicedViewLoadTestInstance(context, m_params);
	if (m_params.testType == TestType::STORE)
		return new SlicedViewStoreTestInstance(context, m_params);

	DE_ASSERT(false);
	return nullptr;
}

tcu::IVec3 makeIVec3 (uint32_t width, uint32_t height, uint32_t depth)
{
	return tcu::IVec3(static_cast<int>(width), static_cast<int>(height), static_cast<int>(depth));
}

de::MovePtr<tcu::PixelBufferAccess> makePixelBufferAccess (const BufferWithMemory& buffer, const tcu::IVec3& size, const tcu::TextureFormat& format)
{
	de::MovePtr<tcu::PixelBufferAccess> bufferImage (new tcu::PixelBufferAccess(format, size, buffer.getAllocation().getHostPtr()));
	return bufferImage;
}

de::MovePtr<BufferWithMemory> makeTransferBuffer (const VkExtent3D& extent, const tcu::TextureFormat& format,
												  const DeviceInterface& vkd, const VkDevice device, Allocator& alloc)
{
	DE_ASSERT(extent.width > 0u);
	DE_ASSERT(extent.height > 0u);
	DE_ASSERT(extent.depth > 0u);

	const auto pixelSizeBytes	= tcu::getPixelSize(format);
	const auto pixelCount		= extent.width * extent.height * extent.depth;
	const auto bufferSize		= static_cast<VkDeviceSize>(pixelCount) * static_cast<VkDeviceSize>(pixelSizeBytes);
	const auto bufferUsage		= (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const auto bufferCreateInfo	= makeBufferCreateInfo(bufferSize, bufferUsage);

	de::MovePtr<BufferWithMemory> buffer (new BufferWithMemory(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible));
	return buffer;
}

de::MovePtr<BufferWithMemory> makeAndFillTransferBuffer (const VkExtent3D& extent, const tcu::TextureFormat& format,
														 const DeviceInterface& vkd, const VkDevice device, Allocator& alloc)
{
	DE_ASSERT(tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER);

	auto		buffer		= makeTransferBuffer(extent, format, vkd, device, alloc);
	const auto	size		= makeIVec3(extent.width, extent.height, extent.depth);
	auto		bufferImg	= makePixelBufferAccess(*buffer, size, format);

	// Fill image with predefined pattern.
	for (int z = 0; z < size.z(); ++z)
		for (int y = 0; y < size.y(); ++y)
			for (int x = 0; x < size.x(); ++x)
			{
				const tcu::UVec4 color (
					static_cast<uint32_t>(0x80 | x),
					static_cast<uint32_t>(0x80 | y),
					static_cast<uint32_t>(0x80 | z),
					1u
				);
				bufferImg->setPixel(color, x, y, z);
			}

	return buffer;
}

de::MovePtr<ImageWithMemory> make3DImage (const DeviceInterface &vkd, const VkDevice device, Allocator& alloc, const VkFormat format, const VkExtent3D& extent, uint32_t mipLevels, const bool sampling)
{
	const VkImageUsageFlags imageUsage	= (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
										| (sampling ? VK_IMAGE_USAGE_SAMPLED_BIT : static_cast<VkImageUsageFlagBits>(0)));

	const VkImageCreateInfo imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_3D,						//	VkImageType				imageType;
		format,									//	VkFormat				format;
		extent,									//	VkExtent3D				extent;
		mipLevels,								//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		imageUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	de::MovePtr<ImageWithMemory> image (new ImageWithMemory(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any));
	return image;
}

VkImageSubresourceRange makeCommonImageSubresourceRange (uint32_t baseLevel, uint32_t levelCount)
{
	return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, baseLevel, levelCount, 0u, 1u);
}

VkImageSubresourceLayers makeCommonImageSubresourceLayers (uint32_t mipLevel)
{
	return makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0u, 1u);
}

Move<VkImageView> make3DImageView (const DeviceInterface &vkd, const VkDevice device, const VkImage image, const VkFormat format,
								   const tcu::Maybe<tcu::UVec2>& slices/*x=offset, y=range)*/, uint32_t mipLevel, uint32_t levelCount)
{
	const bool subSlice = static_cast<bool>(slices);

	VkImageViewSlicedCreateInfoEXT sliceCreateInfo = initVulkanStructure();

	if (subSlice)
	{
		sliceCreateInfo.sliceOffset	= slices->x();
		sliceCreateInfo.sliceCount	= slices->y();
	}

	const VkImageViewCreateInfo viewCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,				//	VkStructureType			sType;
		(subSlice ? &sliceCreateInfo : nullptr),				//	const void*				pNext;
		0u,														//	VkImageViewCreateFlags	flags;
		image,													//	VkImage					image;
		VK_IMAGE_VIEW_TYPE_3D,									//	VkImageViewType			viewType;
		format,													//	VkFormat				format;
		makeComponentMappingRGBA(),								//	VkComponentMapping		components;
		makeCommonImageSubresourceRange(mipLevel, levelCount),	//	VkImageSubresourceRange	subresourceRange;
	};

	return createImageView(vkd, device, &viewCreateInfo);
}

VkPipelineStageFlagBits makePipelineStage (VkShaderStageFlagBits shaderStage)
{
	if (shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if (shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
		return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	DE_ASSERT(false);
	return VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;
}

void SlicedViewTestInstance::runPipeline (const DeviceInterface& vkd, const VkDevice device, const VkCommandBuffer cmdBuffer, const VkImageView slicedImage, const VkImageView auxiliarImage)
{
	// The layouts created and used here must match the shaders.
	const auto descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(descriptorType, m_params.stage);
	layoutBuilder.addSingleBinding(descriptorType, m_params.stage);
	m_setLayout = layoutBuilder.build(vkd, device);

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(descriptorType, 2u);
	m_descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	m_descriptorSet		= makeDescriptorSet(vkd, device, m_descriptorPool.get(), m_setLayout.get());
	m_pipelineLayout	= makePipelineLayout(vkd, device, m_setLayout.get());

	DescriptorSetUpdateBuilder updateBuilder;
	const auto slicedImageDescInfo		= makeDescriptorImageInfo(DE_NULL, slicedImage, kUsageLayout);
	const auto auxiliarImageDescInfo	= makeDescriptorImageInfo(DE_NULL, auxiliarImage, kUsageLayout);
	updateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &slicedImageDescInfo);
	updateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), descriptorType, &auxiliarImageDescInfo);
	updateBuilder.update(vkd, device);

	if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
		runGraphicsPipeline(vkd, device, cmdBuffer);
	else if (m_params.stage == VK_SHADER_STAGE_COMPUTE_BIT)
		runComputePipeline(vkd, device, cmdBuffer);
	else
		DE_ASSERT(false);
}

void SlicedViewTestInstance::runGraphicsPipeline (const DeviceInterface& vkd, const VkDevice device, const VkCommandBuffer cmdBuffer)
{
	const auto	sliceExtent	= m_params.getSliceExtent();
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	vertShader	= createShaderModule(vkd, device, binaries.get("vert"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));
	const auto	extent		= makeExtent3D(sliceExtent.width, sliceExtent.height, 1u);
	const auto	bindPoint	= VK_PIPELINE_BIND_POINT_GRAPHICS;

	m_renderPass	= makeRenderPass(vkd, device);
	m_framebuffer	= makeFramebuffer(vkd, device, m_renderPass.get(), 0u, nullptr, sliceExtent.width, sliceExtent.height);

	const std::vector<VkViewport>	viewports	(1u, makeViewport(extent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(extent));

	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

	m_pipeline = makeGraphicsPipeline(vkd, device, m_pipelineLayout.get(),
									  vertShader.get(), DE_NULL, DE_NULL, DE_NULL, fragShader.get(),
									  m_renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
									  &vertexInputStateCreateInfo);

	beginRenderPass(vkd, cmdBuffer, m_renderPass.get(), m_framebuffer.get(), scissors.at(0u));
	vkd.cmdBindPipeline(cmdBuffer, bindPoint, m_pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
	vkd.cmdDraw(cmdBuffer, kVertexCount, sliceExtent.depth, 0u, 0u);
	endRenderPass(vkd, cmdBuffer);
}

void SlicedViewTestInstance::runComputePipeline (const DeviceInterface& vkd, const VkDevice device, const VkCommandBuffer cmdBuffer)
{
	const auto bindPoint	= VK_PIPELINE_BIND_POINT_COMPUTE;
	const auto compShader	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"));

	m_pipeline = makeComputePipeline(vkd, device, m_pipelineLayout.get(), compShader.get());

	vkd.cmdBindPipeline(cmdBuffer, bindPoint, m_pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, m_pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
	vkd.cmdDispatch(cmdBuffer, m_params.getActualRange(), 1u, 1u);
}

bool SlicedViewTestInstance::runSamplingPipeline (const VkImage fullImage, const VkImageView slicedView, const VkExtent3D& levelExtent)
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	const auto		qfIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();
	auto&			alloc		= m_context.getDefaultAllocator();

	const auto bindPoint		= VK_PIPELINE_BIND_POINT_COMPUTE;
	const auto shaderStage		= VK_SHADER_STAGE_COMPUTE_BIT;
	const auto pipelineStage	= makePipelineStage(shaderStage);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qfIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Descriptor set layout and pipeline layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderStage);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, shaderStage);
	const auto setLayout		= setLayoutBuilder.build(vkd, device);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get());

	// Pipeline.
	const auto compShader	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("compSample"));
	const auto pipeline		= makeComputePipeline(vkd, device, pipelineLayout.get(), compShader.get());

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	// Update descriptor set.
	const VkSamplerCreateInfo samplerCreateInfo =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkSamplerCreateFlags	flags;
		VK_FILTER_NEAREST,							//	VkFilter				magFilter;
		VK_FILTER_NEAREST,							//	VkFilter				minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				//	VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				//	VkSamplerAddressMode	addressModeU;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				//	VkSamplerAddressMode	addressModeV;
		VK_SAMPLER_ADDRESS_MODE_REPEAT,				//	VkSamplerAddressMode	addressModeW;
		0.0f,										//	float					mipLodBias;
		VK_FALSE,									//	VkBool32				anisotropyEnable;
		1.0f,										//	float					maxAnisotropy;
		VK_FALSE,									//	VkBool32				compareEnable;
		VK_COMPARE_OP_NEVER,						//	VkCompareOp				compareOp;
		0.0f,										//	float					minLod;
		0.0f,										//	float					maxLod;
		VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,		//	VkBorderColor			borderColor;
		VK_FALSE,									//	VkBool32				unnormalizedCoordinates;
	};
	const auto sampler = createSampler(vkd, device, &samplerCreateInfo);

	// This will be used as a storage image to verify the sampling results.
	// It has the same size as the full level extent, but only a single level and not sliced.
	const auto auxiliarImage	= make3DImage(vkd, device, alloc, kFormat, levelExtent, 1u, false/*sampling*/);
	const auto auxiliarView		= make3DImageView(vkd, device, auxiliarImage->get(), kFormat, tcu::Nothing, 0u, 1u);

	DescriptorSetUpdateBuilder updateBuilder;
	const auto sampledImageInfo = makeDescriptorImageInfo(sampler.get(), slicedView, kUsageLayout);
	const auto storageImageInfo = makeDescriptorImageInfo(DE_NULL, auxiliarView.get(), kUsageLayout);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sampledImageInfo);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageImageInfo);
	updateBuilder.update(vkd, device);

	const auto tcuFormat	= mapVkFormat(kFormat);
	const auto verifBuffer	= makeTransferBuffer(levelExtent, tcuFormat, vkd, device, alloc);
	const auto refBuffer	= makeTransferBuffer(levelExtent, tcuFormat, vkd, device, alloc);

	beginCommandBuffer(vkd, cmdBuffer);

	// Move auxiliar image to the proper layout.
	const auto shaderAccess			= (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
	const auto colorSRR				= makeCommonImageSubresourceRange(0u, 1u);
	const auto preDispatchBarrier	= makeImageMemoryBarrier(0u, shaderAccess, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, auxiliarImage->get(), colorSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipelineStage, &preDispatchBarrier);

	vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

	// Sync shader writes before copying to verification buffer.
	const auto preCopyBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, pipelineStage, VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyBarrier);

	// Copy storage image to verification buffer.
	const auto colorSRL		= makeCommonImageSubresourceLayers(0u);
	const auto copyRegion	= makeBufferImageCopy(levelExtent, colorSRL);
	vkd.cmdCopyImageToBuffer(cmdBuffer, auxiliarImage->get(), kUsageLayout, verifBuffer->get(), 1u, &copyRegion);

	// Copy full level from the original full image to the reference buffer to compare them.
	const auto refSRL		= makeCommonImageSubresourceLayers(m_params.getSelectedLevel());
	const auto refCopy		= makeBufferImageCopy(levelExtent, refSRL);
	vkd.cmdCopyImageToBuffer(cmdBuffer, fullImage, kUsageLayout, refBuffer->get(), 1u, &refCopy);

	// Sync copies to host.
	const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Compare both buffers.
	auto& verifBufferAlloc	= verifBuffer->getAllocation();
	auto& refBufferAlloc	= refBuffer->getAllocation();
	invalidateAlloc(vkd, device, verifBufferAlloc);
	invalidateAlloc(vkd, device, refBufferAlloc);

	const auto iExtent = makeIVec3(levelExtent.width, levelExtent.height, levelExtent.depth);
	const tcu::ConstPixelBufferAccess verifAcces	(tcuFormat, iExtent, verifBufferAlloc.getHostPtr());
	const tcu::ConstPixelBufferAccess refAccess		(tcuFormat, iExtent, refBufferAlloc.getHostPtr());

	auto&				log			= m_context.getTestContext().getLog();
	const tcu::UVec4	threshold	(0u, 0u, 0u, 0u);
	return tcu::intThresholdCompare(log, "SamplingResult", "", refAccess, verifAcces, threshold, tcu::COMPARE_LOG_ON_ERROR);
}

tcu::TestStatus SlicedViewLoadTestInstance::iterate (void)
{
	const auto&			vkd			= m_context.getDeviceInterface();
	const auto			device		= m_context.getDevice();
	auto&				alloc		= m_context.getDefaultAllocator();
	const auto			qfIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue		= m_context.getUniversalQueue();

	const auto mipLevel			= m_params.getSelectedLevel();
	const auto fullExtent		= makeExtent3D(m_params.width, m_params.height, m_params.depth);
	const auto sliceExtent		= m_params.getSliceExtent();
	const auto tcuFormat		= mapVkFormat(kFormat);
	const auto auxiliarBuffer	= makeAndFillTransferBuffer(sliceExtent, tcuFormat, vkd, device, alloc);
	const auto verifBuffer		= makeTransferBuffer(sliceExtent, tcuFormat, vkd, device, alloc);
	const auto fullImage		= make3DImage(vkd, device, alloc, kFormat, fullExtent, m_params.getFullImageLevels(), m_params.sampleImg);
	const auto fullSRR			= makeCommonImageSubresourceRange(0u, VK_REMAINING_MIP_LEVELS);
	const auto singleSRR		= makeCommonImageSubresourceRange(0u, 1u);
	const auto targetLevelSRL	= makeCommonImageSubresourceLayers(mipLevel);
	const auto baseLevelSRL		= makeCommonImageSubresourceLayers(0u);
	const auto clearColor		= makeClearValueColorU32(0u, 0u, 0u, 0u);
	const auto pipelineStage	= makePipelineStage(m_params.stage);

	const auto cmdPool			= makeCommandPool(vkd, device, qfIndex);
	const auto cmdBufferPtr		= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer		= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Zero-out full image.
	const auto preClearBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
														VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
														fullImage->get(), fullSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, 0u, VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
	vkd.cmdClearColorImage(cmdBuffer, fullImage->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor.color, 1u, &fullSRR);

	// Copy reference buffer to full image at the right offset.
	const auto preCopyBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
													   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
													   fullImage->get(), fullSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyBarrier);

	const VkBufferImageCopy sliceCopy =
	{
		0ull,														//	VkDeviceSize				bufferOffset;
		0u,															//	deUint32					bufferRowLength;
		0u,															//	deUint32					bufferImageHeight;
		targetLevelSRL,												//	VkImageSubresourceLayers	imageSubresource;
		makeOffset3D(0, 0, static_cast<int32_t>(m_params.offset)),	//	VkOffset3D					imageOffset;
		sliceExtent,												//	VkExtent3D					imageExtent;
	};
	vkd.cmdCopyBufferToImage(cmdBuffer, auxiliarBuffer->get(), fullImage->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &sliceCopy);

	// Move full image to the general layout to be able to read from or write to it from the shader.
	// Note: read-only optimal is not a valid layout for this.
	const auto postCopyBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
														VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kUsageLayout,
														fullImage->get(), fullSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipelineStage, &postCopyBarrier);

	// Create sliced view of the full image.
	const auto slicedView = make3DImageView(vkd, device, fullImage->get(), kFormat, tcu::just(tcu::UVec2(m_params.offset, m_params.getSlicedViewRange())), mipLevel, 1u);

	// Create storage image and view with reduced size (this will be the destination image in the shader).
	const auto auxiliarImage	= make3DImage(vkd, device, alloc, kFormat, sliceExtent, 1u, false/*sampling*/);
	const auto auxiliarView		= make3DImageView(vkd, device, auxiliarImage->get(), kFormat, tcu::Nothing, 0u, 1u);

	// Move the auxiliar image to the general layout for writing.
	const auto preWriteBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, kUsageLayout, auxiliarImage->get(), singleSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, 0u, pipelineStage, &preWriteBarrier);

	// Run load operation.
	runPipeline(vkd, device, cmdBuffer, slicedView.get(), auxiliarView.get());

	// Copy auxiliar image (result) to verification buffer.
	const auto preVerifCopyBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, pipelineStage, VK_PIPELINE_STAGE_TRANSFER_BIT, &preVerifCopyBarrier);
	const auto verifCopyRegion = makeBufferImageCopy(sliceExtent, baseLevelSRL);
	vkd.cmdCopyImageToBuffer(cmdBuffer, auxiliarImage->get(), kUsageLayout, verifBuffer->get(), 1u, &verifCopyRegion);

	// Sync verification buffer with host reads.
	const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	const auto	sliceExtentIV3		= makeIVec3(sliceExtent.width, sliceExtent.height, sliceExtent.depth);
	auto&		auxiliarBufferAlloc	= auxiliarBuffer->getAllocation();
	auto&		verifBufferAlloc	= verifBuffer->getAllocation();

	// Invalidate verification buffer allocation.
	invalidateAlloc(vkd, device, verifBufferAlloc);

	// Compare auxiliar buffer and verification buffer.
	const tcu::ConstPixelBufferAccess initialImage (tcuFormat, sliceExtentIV3, auxiliarBufferAlloc.getHostPtr());
	const tcu::ConstPixelBufferAccess finalImage (tcuFormat, sliceExtentIV3, verifBufferAlloc.getHostPtr());

	auto& log = m_context.getTestContext().getLog();
	const tcu::UVec4 threshold(0u, 0u, 0u, 0u);

	if (!tcu::intThresholdCompare(log, "Comparison", "Comparison of reference and result", initialImage, finalImage, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Image comparison failed; check log for details");

	if (m_params.sampleImg && !runSamplingPipeline(fullImage->get(), slicedView.get(), m_params.getFullLevelExtent()))
		return tcu::TestStatus::fail("Sampling full level failed; check log for details");

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus SlicedViewStoreTestInstance::iterate (void)
{
	const auto&			vkd			= m_context.getDeviceInterface();
	const auto			device		= m_context.getDevice();
	auto&				alloc		= m_context.getDefaultAllocator();
	const auto			qfIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue		= m_context.getUniversalQueue();

	const auto mipLevel			= m_params.getSelectedLevel();
	const auto fullExtent		= makeExtent3D(m_params.width, m_params.height, m_params.depth);
	const auto sliceExtent		= m_params.getSliceExtent();
	const auto tcuFormat		= mapVkFormat(kFormat);
	const auto auxiliarBuffer	= makeAndFillTransferBuffer(sliceExtent, tcuFormat, vkd, device, alloc);
	const auto verifBuffer		= makeTransferBuffer(sliceExtent, tcuFormat, vkd, device, alloc);
	const auto fullImage		= make3DImage(vkd, device, alloc, kFormat, fullExtent, m_params.getFullImageLevels(), m_params.sampleImg);
	const auto fullSRR			= makeCommonImageSubresourceRange(0u, VK_REMAINING_MIP_LEVELS);
	const auto singleSRR		= makeCommonImageSubresourceRange(0u, 1u);
	const auto targetLevelSRL	= makeCommonImageSubresourceLayers(mipLevel);
	const auto baseLevelSRL		= makeCommonImageSubresourceLayers(0u);
	const auto clearColor		= makeClearValueColorU32(0u, 0u, 0u, 0u);
	const auto pipelineStage	= makePipelineStage(m_params.stage);

	const auto cmdPool			= makeCommandPool(vkd, device, qfIndex);
	const auto cmdBufferPtr		= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer		= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Zero-out full image.
	const auto preClearBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, fullImage->get(), fullSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, 0u, VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
	vkd.cmdClearColorImage(cmdBuffer, fullImage->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor.color, 1u, &fullSRR);

	// Create sliced view of the full image.
	const auto slicedView = make3DImageView(vkd, device, fullImage->get(), kFormat, tcu::just(tcu::UVec2(m_params.offset, m_params.getSlicedViewRange())), mipLevel, 1u);

	// Create storage image and view with reduced size (this will be the source image in the shader).
	const auto auxiliarImage	= make3DImage(vkd, device, alloc, kFormat, sliceExtent, 1u, false/*sampling*/);
	const auto auxiliarView		= make3DImageView(vkd, device, auxiliarImage->get(), kFormat, tcu::Nothing, 0u, 1u);

	// Copy reference buffer into auxiliar image.
	const auto preCopyBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, auxiliarImage->get(), singleSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, 0u, VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyBarrier);
	const auto sliceCopy = makeBufferImageCopy(sliceExtent, baseLevelSRL);
	vkd.cmdCopyBufferToImage(cmdBuffer, auxiliarBuffer->get(), auxiliarImage->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &sliceCopy);

	// Move both images to the general layout for reading and writing.
	// Note: read-only optimal is not a valid layout for the read image.
	const auto preShaderBarrierAux	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kUsageLayout, auxiliarImage->get(), singleSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipelineStage, &preShaderBarrierAux);
	const auto preShaderBarrierFull	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kUsageLayout, fullImage->get(), fullSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipelineStage, &preShaderBarrierFull);

	// Run store operation.
	runPipeline(vkd, device, cmdBuffer, slicedView.get(), auxiliarView.get());

	// Copy the right section of the full image (result) to verification buffer.
	const auto preVerifCopyBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, pipelineStage, VK_PIPELINE_STAGE_TRANSFER_BIT, &preVerifCopyBarrier);

	const VkBufferImageCopy verifCopy =
	{
		0ull,														//	VkDeviceSize				bufferOffset;
		0u,															//	deUint32					bufferRowLength;
		0u,															//	deUint32					bufferImageHeight;
		targetLevelSRL,												//	VkImageSubresourceLayers	imageSubresource;
		makeOffset3D(0, 0, static_cast<int32_t>(m_params.offset)),	//	VkOffset3D					imageOffset;
		sliceExtent,												//	VkExtent3D					imageExtent;
	};
	vkd.cmdCopyImageToBuffer(cmdBuffer, fullImage->get(), kUsageLayout, verifBuffer->get(), 1u, &verifCopy);

	// Sync verification buffer with host reads.
	const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	const auto	sliceExtentIV3		= makeIVec3(sliceExtent.width, sliceExtent.height, sliceExtent.depth);
	auto&		auxiliarBufferAlloc	= auxiliarBuffer->getAllocation();
	auto&		verifBufferAlloc	= verifBuffer->getAllocation();

	// Invalidate verification buffer allocation.
	invalidateAlloc(vkd, device, verifBufferAlloc);

	// Compare auxiliar buffer and verification buffer.
	const tcu::ConstPixelBufferAccess initialImage (tcuFormat, sliceExtentIV3, auxiliarBufferAlloc.getHostPtr());
	const tcu::ConstPixelBufferAccess finalImage (tcuFormat, sliceExtentIV3, verifBufferAlloc.getHostPtr());

	auto& log = m_context.getTestContext().getLog();
	const tcu::UVec4 threshold(0u, 0u, 0u, 0u);

	if (!tcu::intThresholdCompare(log, "Comparison", "Comparison of reference and result", initialImage, finalImage, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Image comparison failed; check log for details");

	if (m_params.sampleImg && !runSamplingPipeline(fullImage->get(), slicedView.get(), m_params.getFullLevelExtent()))
		return tcu::TestStatus::fail("Sampling full level failed; check log for details");

	return tcu::TestStatus::pass("Pass");
}

using TestCaseGroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // anonymous

tcu::TestCaseGroup* createImageSlicedViewOf3DTests (tcu::TestContext& testCtx)
{
	TestCaseGroupPtr imageTests (new tcu::TestCaseGroup(testCtx, "sliced_view_of_3d_image", "Sliced view of 3D image tests"));

	const struct
	{
		VkShaderStageFlagBits	stage;
		const char*				name;
	} stageCases[] =
	{
		{ VK_SHADER_STAGE_COMPUTE_BIT,	"comp"	},
		{ VK_SHADER_STAGE_FRAGMENT_BIT,	"frag"	},
	};

	const struct
	{
		TestType			testType;
		const char*			name;
	} testTypeCases[] =
	{
		{ TestType::LOAD,		"load"	},
		{ TestType::STORE,		"store"	},
	};

	const struct
	{
		bool				sampleImg;
		const char*			suffix;
	} samplingCases[] =
	{
		{ false,			""					},
		{ true,				"_with_sampling"	},
	};

	const uint32_t	seed	= 1667817299u;
	de::Random		rnd		(seed);

	// Basic tests with 2 slices and a view of the first or second slice.
	{
		const uint32_t basicDepth = 2u;
		const uint32_t basicRange = 1u;

		TestCaseGroupPtr basicTests (new tcu::TestCaseGroup(testCtx, "basic", "Basic 3D slice tests"));

		for (const auto& testTypeCase : testTypeCases)
		{
			TestCaseGroupPtr testTypeGroup (new tcu::TestCaseGroup(testCtx, testTypeCase.name, ""));

			for (const auto& stageCase : stageCases)
			{
				TestCaseGroupPtr stageGroup (new tcu::TestCaseGroup(testCtx, stageCase.name, ""));

				for (uint32_t offset = 0u; offset < basicDepth; ++offset)
				{
					for (const auto& samplingCase : samplingCases)
					{
						const auto	testName	= "offset_" + std::to_string(offset) + samplingCase.suffix;
						TestParams	params		(testTypeCase.testType, stageCase.stage, kWidth, kHeight, basicDepth, offset, basicRange, tcu::Nothing, samplingCase.sampleImg);

						stageGroup->addChild(new SlicedViewTestCase(testCtx, testName, "", params));
					}
				}

				testTypeGroup->addChild(stageGroup.release());
			}

			basicTests->addChild(testTypeGroup.release());
		}

		imageTests->addChild(basicTests.release());
	}

	// Full slice tests.
	{
		const uint32_t fullDepth = 4u;

		TestCaseGroupPtr fullSliceTests (new tcu::TestCaseGroup(testCtx, "full_slice", "Full 3D slice tests"));

		for (const auto& testTypeCase : testTypeCases)
		{
			TestCaseGroupPtr testTypeGroup (new tcu::TestCaseGroup(testCtx, testTypeCase.name, ""));

			for (const auto& stageCase : stageCases)
			{
				for (const auto& samplingCase : samplingCases)
				{
					const auto testName = std::string(stageCase.name) + samplingCase.suffix;
					TestParams params (testTypeCase.testType, stageCase.stage, kWidth, kHeight, fullDepth, 0u, fullDepth, tcu::Nothing, samplingCase.sampleImg);
					testTypeGroup->addChild(new SlicedViewTestCase(testCtx, testName, "", params));
				}
			}

			fullSliceTests->addChild(testTypeGroup.release());
		}

		imageTests->addChild(fullSliceTests.release());
	}

	// Pseudorandom test cases.
	{
		using CaseId	= std::tuple<uint32_t, uint32_t, uint32_t>; // depth, offset, range
		using CaseIdSet	= std::set<CaseId>;

		const uint32_t	depthCases	= 5u;
		const uint32_t	rangeCases	= 5u;
		const int		minDepth	= 10u;
		const int		maxDepth	= 32u;

		TestCaseGroupPtr randomTests (new tcu::TestCaseGroup(testCtx, "random", "Pseudorandom 3D slice test cases"));

		for (const auto& testTypeCase : testTypeCases)
		{
			TestCaseGroupPtr testTypeGroup (new tcu::TestCaseGroup(testCtx, testTypeCase.name, ""));

			for (const auto& stageCase : stageCases)
			{
				TestCaseGroupPtr stageGroup (new tcu::TestCaseGroup(testCtx, stageCase.name, ""));

				CaseIdSet generatedCases;

				for (uint32_t i = 0u; i < depthCases; ++i)
				{
					const uint32_t depth = static_cast<uint32_t>(rnd.getInt(minDepth, maxDepth));

					for (uint32_t j = 0u; j < rangeCases; ++j)
					{
						uint32_t offset	= 0u;
						uint32_t range	= 0u;

						for (;;)
						{
							DE_ASSERT(depth > 0u);
							offset = static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(depth - 1u)));

							DE_ASSERT(offset < depth);
							range = static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(depth - offset)));

							// 0 is interpreted as VK_REMAINING_3D_SLICES_EXT.
							if (range == 0u)
								range = VK_REMAINING_3D_SLICES_EXT;

							// The current seed may generate duplicate cases with non-unique names, so we filter those out.
							const CaseId currentCase (depth, offset, range);
							if (de::contains(begin(generatedCases), end(generatedCases), currentCase))
								continue;

							generatedCases.insert(currentCase);
							break;
						}

						const auto	rangeStr	= ((range == VK_REMAINING_3D_SLICES_EXT) ? "remaining_3d_slices" : std::to_string(range));
						const auto	testName	= "depth_" + std::to_string(depth) + "_offset_" + std::to_string(offset) + "_range_" + rangeStr;
						TestParams	params		(testTypeCase.testType, stageCase.stage, kWidth, kHeight, depth, offset, range, tcu::Nothing, false);

						stageGroup->addChild(new SlicedViewTestCase(testCtx, testName, "", params));
					}
				}

				testTypeGroup->addChild(stageGroup.release());
			}

			randomTests->addChild(testTypeGroup.release());
		}

		imageTests->addChild(randomTests.release());
	}

	// Mip level test cases.
	{
		using CaseId	= std::tuple<uint32_t, uint32_t>; // depth, offset, range
		using CaseIdSet	= std::set<CaseId>;

		const uint32_t	casesPerLevel	= 2u;
		const uint32_t	width			= kWidth;
		const uint32_t	height			= kWidth;
		const uint32_t	depth			= kWidth;
		const uint32_t	maxLevels		= TestParams::getMaxMipLevelCountForSize(kWidth);

		TestCaseGroupPtr mipLevelTests (new tcu::TestCaseGroup(testCtx, "mip_level", "3D slice test cases using mip levels"));

		for (const auto& testTypeCase : testTypeCases)
		{
			TestCaseGroupPtr testTypeGroup (new tcu::TestCaseGroup(testCtx, testTypeCase.name, ""));

			for (const auto& stageCase : stageCases)
			{
				TestCaseGroupPtr stageGroup (new tcu::TestCaseGroup(testCtx, stageCase.name, ""));

				for (uint32_t level = 0u; level < maxLevels; ++level)
				{
					const auto	levelSize		= (depth >> level);
					const auto	groupName		= "level_" + std::to_string(level);
					CaseIdSet	generatedCases;

					DE_ASSERT(levelSize > 0u);

					TestCaseGroupPtr levelGroup (new tcu::TestCaseGroup(testCtx, groupName.c_str(), ""));

					// Generate a few pseudorandom cases per mip level.
					for (uint32_t i = 0u; i < casesPerLevel; ++i)
					{
						uint32_t offset = 0u;
						uint32_t range = 0u;

						for (;;)
						{
							offset = static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(levelSize - 1u)));
							DE_ASSERT(offset < levelSize);

							range = static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(levelSize - offset)));

							// 0 is interpreted as VK_REMAINING_3D_SLICES_EXT.
							if (range == 0u)
								range = VK_REMAINING_3D_SLICES_EXT;

							const CaseId currentCase (offset, range);
							if (de::contains(begin(generatedCases), end(generatedCases), currentCase))
								continue;

							generatedCases.insert(currentCase);
							break;
						}

						const auto rangeStr	= ((range == VK_REMAINING_3D_SLICES_EXT) ? "remaining_3d_slices" : std::to_string(range));
						const auto testName	= "offset_" + std::to_string(offset) + "_range_" + rangeStr;
						TestParams params	(testTypeCase.testType, stageCase.stage, width, height, depth, offset, range, tcu::just(level), false);

						levelGroup->addChild(new SlicedViewTestCase(testCtx, testName, "", params));
					}

					stageGroup->addChild(levelGroup.release());
				}

				testTypeGroup->addChild(stageGroup.release());
			}

			mipLevelTests->addChild(testTypeGroup.release());
		}

		imageTests->addChild(mipLevelTests.release());
	}

	return imageTests.release();
}

} // pipeline
} // vkt
