/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 The Android Open Source Project
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
 * \brief Cube image with misaligned baseArrayLayer tests
 *//*--------------------------------------------------------------------*/

#include "vktImageMisalignedCubeTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vktImageTexture.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"

#include <string>

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{

inline VkImageCreateInfo makeImageCreateInfo (const tcu::IVec3& size, const VkFormat format)
{
	const VkImageUsageFlags	usage		= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageCreateInfo	imageParams	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType			sType;
		DE_NULL,								//  const void*				pNext;
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,	//  VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//  VkImageType				imageType;
		format,									//  VkFormat				format;
		makeExtent3D(size.x(), size.y(), 1u),	//  VkExtent3D				extent;
		1u,										//  deUint32				mipLevels;
		(deUint32)size.z(),						//  deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling			tiling;
		usage,									//  VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode			sharingMode;
		0u,										//  deUint32				queueFamilyIndexCount;
		DE_NULL,								//  const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//  VkImageLayout			initialLayout;
	};

	return imageParams;
}

void fillBuffer (const DeviceInterface& vk, const VkDevice device, const Allocation& alloc, const VkDeviceSize offset, const VkDeviceSize size, const VkFormat format, const tcu::Vec4& color)
{
	const tcu::TextureFormat	textureFormat		= mapVkFormat(format);
	const deUint32				colorPixelSize		= static_cast<deUint32>(tcu::getPixelSize(textureFormat));
	tcu::TextureLevel			colorPixelBuffer	(textureFormat, 1, 1);
	tcu::PixelBufferAccess		colorPixel			(colorPixelBuffer);

	colorPixel.setPixel(color, 0, 0);

	const deUint8*	src		= static_cast<deUint8*>(colorPixel.getDataPtr());
	deUint8*		dstBase	= static_cast<deUint8*>(alloc.getHostPtr());
	deUint8*		dst		= &dstBase[offset];

	for (deUint32 pixelPos = 0; pixelPos < size; pixelPos += colorPixelSize)
		deMemcpy(&dst[pixelPos], src, colorPixelSize);

	flushMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset() + offset, size);
}

VkBufferImageCopy makeBufferImageCopy (const vk::VkDeviceSize&				bufferOffset,
									   const vk::VkImageSubresourceLayers&	imageSubresource,
									   const vk::VkOffset3D&				imageOffset,
									   const vk::VkExtent3D&				imageExtent)
{
	const VkBufferImageCopy copyParams =
	{
		bufferOffset,								//	VkDeviceSize				bufferOffset;
		0u,											//	deUint32					bufferRowLength;
		0u,											//	deUint32					bufferImageHeight;
		imageSubresource,							//	VkImageSubresourceLayers	imageSubresource;
		imageOffset,								//	VkOffset3D					imageOffset;
		imageExtent,								//	VkExtent3D					imageExtent;
	};
	return copyParams;
}

//! Interpret the memory as IVec4
inline tcu::Vec4 readVec4 (const void* const data, const deUint32 ndx)
{
	const float* const	p	= reinterpret_cast<const float*>(data);
	const deUint32		ofs	= 4 * ndx;

	return tcu::Vec4(p[ofs+0], p[ofs+1], p[ofs+2], p[ofs+3]);
}

class MisalignedCubeTestInstance : public TestInstance
{
public:
					MisalignedCubeTestInstance	(Context&			context,
												 const tcu::IVec3&	size,
												 const VkFormat		format);
	tcu::TestStatus	iterate						(void);

private:
	const tcu::IVec3&	m_size;
	const VkFormat		m_format;
};

MisalignedCubeTestInstance::MisalignedCubeTestInstance (Context& context, const tcu::IVec3& size, const VkFormat format)
	: TestInstance	(context)
	, m_size		(size)
	, m_format		(format)
{
}

tcu::TestStatus MisalignedCubeTestInstance::iterate (void)
{
	DE_ASSERT(de::inRange(m_size.z(), 6, 16));
	DE_ASSERT(m_format == VK_FORMAT_R8G8B8A8_UNORM);

	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	Allocator&						allocator				= m_context.getDefaultAllocator();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const deUint32					numLayers				= m_size.z();
	const deUint32					cube0LayerStart			= 0;
	const deUint32					cube1LayerStart			= numLayers - 6u;
	const VkDeviceSize				resultBufferSizeBytes	= 2 * 6 * 4 * sizeof(float);	// vec4[6] in shader
	const VkExtent3D				imageExtent				= makeExtent3D(m_size.x(), m_size.y(), 1u);
	const deUint32					pixelSize				= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(m_format)));
	const deUint32					layerSize				= imageExtent.width * imageExtent.height * pixelSize;
	const float						eps						= 1.0f / float(2 * 256);

	const VkBufferCreateInfo		resultBufferCreateInfo	= makeBufferCreateInfo(resultBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	de::MovePtr<Buffer>				resultBuffer			= de::MovePtr<Buffer>(new Buffer(vk, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));
	const Allocation&				resultBufferAlloc		= resultBuffer->getAllocation();
	const VkImageCreateInfo			imageCreateInfo			= makeImageCreateInfo(m_size, m_format);
	de::MovePtr<Image>				image					= de::MovePtr<Image>(new Image(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const VkImageSubresourceRange	imageSubresourceRange0	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, cube0LayerStart, 6u);
	Move<VkImageView>				imageView0				= makeImageView(vk, device, image->get(), VK_IMAGE_VIEW_TYPE_CUBE, m_format, imageSubresourceRange0);
	const VkImageSubresourceRange	imageSubresourceRange1	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, cube1LayerStart, 6u);
	Move<VkImageView>				imageView1				= makeImageView(vk, device, image->get(), VK_IMAGE_VIEW_TYPE_CUBE, m_format, imageSubresourceRange1);

	Move<VkDescriptorSetLayout>		descriptorSetLayout		= DescriptorSetLayoutBuilder()
																.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
																.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
																.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
																.build(vk, device);
	Move<VkDescriptorPool>			descriptorPool			= DescriptorPoolBuilder()
																.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSet>			descriptorSet			= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	const VkDescriptorImageInfo		descriptorImageInfo0	= makeDescriptorImageInfo(DE_NULL, *imageView0, VK_IMAGE_LAYOUT_GENERAL);
	const VkDescriptorImageInfo		descriptorImageInfo1	= makeDescriptorImageInfo(DE_NULL, *imageView1, VK_IMAGE_LAYOUT_GENERAL);
	const VkDescriptorBufferInfo	descriptorBufferInfo	= makeDescriptorBufferInfo(resultBuffer->get(), 0ull, resultBufferSizeBytes);

	const Move<VkShaderModule>		shaderModule			= createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0);
	const Move<VkPipelineLayout>	pipelineLayout			= makePipelineLayout(vk, device, *descriptorSetLayout);
	const Move<VkPipeline>			pipeline				= makeComputePipeline(vk, device, *pipelineLayout, *shaderModule);
	const Move<VkCommandPool>		cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>		cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const VkDeviceSize				clearBufferSize			= layerSize * numLayers;
	const Move<VkBuffer>			clearBuffer				= makeBuffer(vk, device, clearBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	const de::MovePtr<Allocation>	clearBufferAlloc		= bindBuffer(vk, device, allocator, *clearBuffer, MemoryRequirement::HostVisible);
	const VkImageSubresourceRange	clearSubresRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, numLayers);
	const VkImageMemoryBarrier		clearBarrier			= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																					 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																					 image->get(), clearSubresRange);
	const VkImageMemoryBarrier		preShaderImageBarrier	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
																					 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																					 image->get(), clearSubresRange);
	const VkBufferMemoryBarrier		postShaderBarrier		= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
																					  resultBuffer->get(), 0ull, VK_WHOLE_SIZE);
	bool							result					= true;

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo0)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo1)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo)
		.update(vk, device);

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &clearBarrier);

	// Clear layers with predefined values
	for (deUint32 layerNdx = 0; layerNdx < numLayers; ++layerNdx)
	{
		const float						componentValue			= float(16 * layerNdx) / 255.0f;
		const tcu::Vec4					clearColor				= tcu::Vec4(componentValue, componentValue, componentValue, 1.0f);
		const VkDeviceSize				bufferOffset			= layerNdx * layerSize;
		const VkImageSubresourceLayers	imageSubresource		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, layerNdx, 1u);
		const VkBufferImageCopy			bufferImageCopyRegion	= makeBufferImageCopy(bufferOffset, imageSubresource, makeOffset3D(0u, 0u, 0u), imageExtent);

		fillBuffer(vk, device, *clearBufferAlloc, bufferOffset, layerSize, m_format, clearColor);

		vk.cmdCopyBufferToImage(*cmdBuffer, *clearBuffer, image->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferImageCopyRegion);
	}

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preShaderImageBarrier);

	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, DE_NULL, 1, &postShaderBarrier, 0, DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateAlloc(vk, device, resultBufferAlloc);

	// Check cube 0
	for (deUint32 layerNdx = 0; layerNdx < 6; ++layerNdx)
	{
		const deUint32	layerUsed		= cube0LayerStart + layerNdx;
		const float		componentValue	= float(16 * layerUsed) / 255.0f;
		const tcu::Vec4	expectedColor	= tcu::Vec4(componentValue, componentValue, componentValue, 1.0f);
		const tcu::Vec4	resultColor		= readVec4(resultBufferAlloc.getHostPtr(), layerNdx);
		const tcu::Vec4	delta			= expectedColor - resultColor;

		if (deFloatAbs(delta.x()) > eps || deFloatAbs(delta.y()) > eps || deFloatAbs(delta.z()) > eps || deFloatAbs(delta.w()) > eps)
			result = false;
	}

	// Check cube 1
	for (deUint32 layerNdx = 0; layerNdx < 6; ++layerNdx)
	{
		const deUint32	layerUsed		= cube1LayerStart + layerNdx;
		const float		componentValue	= float(16 * layerUsed) / 255.0f;
		const tcu::Vec4	expectedColor	= tcu::Vec4(componentValue, componentValue, componentValue, 1.0f);
		const tcu::Vec4 resultColor		= readVec4(resultBufferAlloc.getHostPtr(), layerNdx + 6u);
		const tcu::Vec4	delta			= expectedColor - resultColor;

		if (deFloatAbs(delta.x()) > eps || deFloatAbs(delta.y()) > eps || deFloatAbs(delta.z()) > eps || deFloatAbs(delta.w()) > eps)
			result = false;
	}

	if (result)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

class MisalignedCubeTest : public TestCase
{
public:
						MisalignedCubeTest	(tcu::TestContext&	testCtx,
											 const std::string&	name,
											 const std::string&	description,
											 const tcu::IVec3&	size,
											 const VkFormat		format);

	void				initPrograms		(SourceCollections& programCollection) const;
	TestInstance*		createInstance		(Context&			context) const;

private:
	const tcu::IVec3	m_size;
	const VkFormat		m_format;
};

MisalignedCubeTest::MisalignedCubeTest (tcu::TestContext&	testCtx,
										const std::string&	name,
										const std::string&	description,
										const tcu::IVec3&	size,
										const VkFormat		format)
	: TestCase	(testCtx, name, description)
	, m_size	(size)
	, m_format	(format)
{
}

void MisalignedCubeTest::initPrograms (SourceCollections& programCollection) const
{
	const std::string formatQualifierStr = getShaderImageFormatQualifier(mapVkFormat(m_format));

	std::ostringstream src;
	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
		<< "\n"
		<< "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout (binding = 0, " << formatQualifierStr << ") " << "readonly uniform highp imageCube u_cubeImage0;\n"
		<< "layout (binding = 1, " << formatQualifierStr << ") " << "readonly uniform highp imageCube u_cubeImage1;\n"
		<< "layout (binding = 2) writeonly buffer Output\n"
		<< "{\n"
		<< "    vec4 cube0_color0;\n"
		<< "    vec4 cube0_color1;\n"
		<< "    vec4 cube0_color2;\n"
		<< "    vec4 cube0_color3;\n"
		<< "    vec4 cube0_color4;\n"
		<< "    vec4 cube0_color5;\n"
		<< "    vec4 cube1_color0;\n"
		<< "    vec4 cube1_color1;\n"
		<< "    vec4 cube1_color2;\n"
		<< "    vec4 cube1_color3;\n"
		<< "    vec4 cube1_color4;\n"
		<< "    vec4 cube1_color5;\n"
		<< "} sb_out;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    sb_out.cube0_color0 = imageLoad(u_cubeImage0, ivec3(1, 1, 0));\n"
		<< "    sb_out.cube0_color1 = imageLoad(u_cubeImage0, ivec3(1, 1, 1));\n"
		<< "    sb_out.cube0_color2 = imageLoad(u_cubeImage0, ivec3(1, 1, 2));\n"
		<< "    sb_out.cube0_color3 = imageLoad(u_cubeImage0, ivec3(1, 1, 3));\n"
		<< "    sb_out.cube0_color4 = imageLoad(u_cubeImage0, ivec3(1, 1, 4));\n"
		<< "    sb_out.cube0_color5 = imageLoad(u_cubeImage0, ivec3(1, 1, 5));\n"
		<< "    sb_out.cube1_color0 = imageLoad(u_cubeImage1, ivec3(1, 1, 0));\n"
		<< "    sb_out.cube1_color1 = imageLoad(u_cubeImage1, ivec3(1, 1, 1));\n"
		<< "    sb_out.cube1_color2 = imageLoad(u_cubeImage1, ivec3(1, 1, 2));\n"
		<< "    sb_out.cube1_color3 = imageLoad(u_cubeImage1, ivec3(1, 1, 3));\n"
		<< "    sb_out.cube1_color4 = imageLoad(u_cubeImage1, ivec3(1, 1, 4));\n"
		<< "    sb_out.cube1_color5 = imageLoad(u_cubeImage1, ivec3(1, 1, 5));\n"
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* MisalignedCubeTest::createInstance (Context& context) const
{
	return new MisalignedCubeTestInstance(context, m_size, m_format);
}

//! Base sizes used to generate actual imager sizes in the test.
static const tcu::IVec3 s_baseImageSizes[] =
{
	tcu::IVec3(16, 16,  7),
	tcu::IVec3(16, 16,  8),
	tcu::IVec3(16, 16,  9),
	tcu::IVec3(16, 16, 10),
	tcu::IVec3(16, 16, 11),
};

} // anonymous ns

tcu::TestCaseGroup* createMisalignedCubeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "misaligned_cube", "Cube image with misaligned baseArrayLayer test cases"));

	const VkFormat	format	= VK_FORMAT_R8G8B8A8_UNORM;

	for (int imageSizeNdx = 0; imageSizeNdx < DE_LENGTH_OF_ARRAY(s_baseImageSizes); ++imageSizeNdx)
	{
		const tcu::IVec3	size	= s_baseImageSizes[imageSizeNdx];

		testGroup->addChild(new MisalignedCubeTest(testCtx, de::toString(size.z()), "", size, format));
	}

	return testGroup.release();
}

} // image
} // vkt
