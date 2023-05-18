/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected memory workgroup storage tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemWorkgroupStorageTests.hpp"

#include "vktProtectedMemContext.hpp"
#include "vktProtectedMemUtils.hpp"
#include "vktProtectedMemImageValidator.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "gluTextureTestUtil.hpp"

#include "deRandom.hpp"

namespace vkt
{
namespace ProtectedMem
{

namespace
{

struct Params
{
	deUint32			sharedMemorySize;
	deUint32			imageWidth;
	deUint32			imageHeight;

	Params (deUint32	sharedMemorySize_)
		: sharedMemorySize	(sharedMemorySize_)
	{
		// Find suitable image dimensions based on shared memory size
		imageWidth = 1;
		imageHeight = 1;
		bool increaseWidth = true;
		while (imageWidth * imageHeight < sharedMemorySize)
		{
			if (increaseWidth)
				imageWidth *= 2;
			else
				imageHeight *= 2;

			increaseWidth = !increaseWidth;
		}
	}
};

deUint32 getSeedValue (const Params& params)
{
	return deInt32Hash(params.sharedMemorySize);
}

class WorkgroupStorageTestInstance : public ProtectedTestInstance
{
public:
								WorkgroupStorageTestInstance	(Context&				ctx,
																 const ImageValidator&	validator,
																 const Params&			params);
	virtual tcu::TestStatus		iterate							(void);

private:
	de::MovePtr<tcu::Texture2D>	createTestTexture2D				(void);
	tcu::TestStatus				validateResult					(vk::VkImage			image,
																 vk::VkImageLayout imageLayout,
																 const tcu::Texture2D&	texture2D,
																 const tcu::Sampler&	refSampler);
	void						calculateRef					(tcu::Texture2D&		texture2D);

	const ImageValidator&		m_validator;
	const Params&				m_params;
};

class WorkgroupStorageTestCase : public TestCase
{
public:
								WorkgroupStorageTestCase	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 const std::string&		description,
															 const Params&			params)
									: TestCase		(testCtx, name, description)
									, m_validator	(vk::VK_FORMAT_R8G8B8A8_UNORM)
									, m_params		(params)
								{
								}

	virtual						~WorkgroupStorageTestCase	(void) {}
	virtual TestInstance*		createInstance				(Context& ctx) const
								{
									return new WorkgroupStorageTestInstance(ctx, m_validator, m_params);
								}
	virtual void				initPrograms				(vk::SourceCollections& programCollection) const;
	virtual void				checkSupport				(Context& context) const
								{
									checkProtectedQueueSupport(context);
								}

private:
	ImageValidator				m_validator;
	Params						m_params;
};

void WorkgroupStorageTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	m_validator.initPrograms(programCollection);

	// Fill shared data array with source image data. Output result color with results from
	// shared memory written by another invocation.
	std::string comp =
		std::string() +
		"#version 450\n"
		"layout(local_size_x = " + de::toString(m_params.imageWidth) + ", local_size_y = " + de::toString(m_params.imageHeight) + ", local_size_z = 1) in;\n"
		"layout(set = 0, binding = 0, rgba8) writeonly uniform highp image2D u_resultImage;\n"
		"layout(set = 0, binding = 1, rgba8) readonly uniform highp image2D u_srcImage;\n"
		"shared vec4 sharedData[" + de::toString(m_params.sharedMemorySize) + "];\n"
		"\n"
		"void main() {\n"
		"    int gx = int(gl_GlobalInvocationID.x);\n"
		"    int gy = int(gl_GlobalInvocationID.y);\n"
		"    int s = " + de::toString(m_params.sharedMemorySize) + ";\n"
		"    int idx0 = gy * " + de::toString(m_params.imageWidth) + " + gx;\n"
		"    int idx1 = (idx0 + 1) % s;\n"
		"    vec4 color = imageLoad(u_srcImage, ivec2(gx, gy));\n"
		"    if (idx0 < s)\n"
		"    {\n"
		"        sharedData[idx0] = color;\n"
		"    }\n"
		"    barrier();\n"
		"    vec4 outColor = sharedData[idx1];\n"
		"    imageStore(u_resultImage, ivec2(gx, gy), outColor);\n"
		"}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(comp);
}

WorkgroupStorageTestInstance::WorkgroupStorageTestInstance (Context&				ctx,
															const ImageValidator&	validator,
															const Params&			params)
	: ProtectedTestInstance	(ctx)
	, m_validator			(validator)
	, m_params				(params)
{
}

de::MovePtr<tcu::Texture2D> WorkgroupStorageTestInstance::createTestTexture2D (void)
{
	const tcu::TextureFormat		texFmt		= mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM);
	const tcu::TextureFormatInfo	fmtInfo		= tcu::getTextureFormatInfo(texFmt);
	de::MovePtr<tcu::Texture2D>		texture2D	(new tcu::Texture2D(texFmt, m_params.imageWidth, m_params.imageHeight));

	texture2D->allocLevel(0);

	const tcu::PixelBufferAccess&	level		= texture2D->getLevel(0);

	fillWithRandomColorTiles(level, fmtInfo.valueMin, fmtInfo.valueMax, getSeedValue(m_params));

	return texture2D;
}

tcu::TestStatus WorkgroupStorageTestInstance::iterate (void)
{
	ProtectedContext&						ctx					(m_protectedContext);
	const vk::DeviceInterface&				vk					= ctx.getDeviceInterface();
	const vk::VkDevice						device				= ctx.getDevice();
	const vk::VkQueue						queue				= ctx.getQueue();
	const deUint32							queueFamilyIndex	= ctx.getQueueFamilyIndex();
	const vk::VkPhysicalDeviceProperties	properties			= vk::getPhysicalDeviceProperties(ctx.getInstanceDriver(), ctx.getPhysicalDevice());

	vk::Unique<vk::VkCommandPool>			cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));

	de::MovePtr<tcu::Texture2D>				texture2D			= createTestTexture2D();
	const tcu::Sampler						refSampler			= tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
																			   tcu::Sampler::NEAREST, tcu::Sampler::NEAREST,
																			   00.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_NONE,
																			   0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);

	vk::Unique<vk::VkShaderModule>			computeShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("comp"), 0));

	de::MovePtr<vk::ImageWithMemory>		imageSrc;
	de::MovePtr<vk::ImageWithMemory>		imageDst;
	vk::Move<vk::VkSampler>					sampler;
	vk::Move<vk::VkImageView>				imageViewSrc;
	vk::Move<vk::VkImageView>				imageViewDst;

	vk::Move<vk::VkDescriptorSetLayout>		descriptorSetLayout;
	vk::Move<vk::VkDescriptorPool>			descriptorPool;
	vk::Move<vk::VkDescriptorSet>			descriptorSet;

	// Check there is enough shared memory supported
	if (properties.limits.maxComputeSharedMemorySize < m_params.sharedMemorySize * 4 * 4)
		throw tcu::NotSupportedError("Not enough shared memory supported.");

	// Check the number of invocations supported
	if (properties.limits.maxComputeWorkGroupInvocations < m_params.imageWidth * m_params.imageHeight)
		throw tcu::NotSupportedError("Not enough compute workgroup invocations supported.");

	// Create src and dst images
	{
		vk::VkImageUsageFlags imageUsageFlags = vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT	|
												vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT	|
												vk::VK_IMAGE_USAGE_SAMPLED_BIT		|
												vk::VK_IMAGE_USAGE_STORAGE_BIT;

		imageSrc = createImage2D(ctx, PROTECTION_ENABLED, queueFamilyIndex,
								 m_params.imageWidth, m_params.imageHeight,
								 vk::VK_FORMAT_R8G8B8A8_UNORM,
								 imageUsageFlags);

		imageDst = createImage2D(ctx, PROTECTION_ENABLED, queueFamilyIndex,
								 m_params.imageWidth, m_params.imageHeight,
								 vk::VK_FORMAT_R8G8B8A8_UNORM,
								 imageUsageFlags);
	}

	// Upload source image
	{
		de::MovePtr<vk::ImageWithMemory>	unprotectedImage	= createImage2D(ctx, PROTECTION_DISABLED, queueFamilyIndex,
																				m_params.imageWidth, m_params.imageHeight,
																				vk::VK_FORMAT_R8G8B8A8_UNORM,
																				vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		// Upload data to an unprotected image
		uploadImage(m_protectedContext, **unprotectedImage, *texture2D);

		// Copy unprotected image to protected image
		copyToProtectedImage(m_protectedContext, **unprotectedImage, **imageSrc, vk::VK_IMAGE_LAYOUT_GENERAL, m_params.imageWidth, m_params.imageHeight);
	}

	// Clear dst image
	clearImage(m_protectedContext, **imageDst);

	// Create descriptors
	{
		vk::DescriptorSetLayoutBuilder	layoutBuilder;
		vk::DescriptorPoolBuilder		poolBuilder;

		layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
		layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
		poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2u);

		descriptorSetLayout		= layoutBuilder.build(vk, device);
		descriptorPool			= poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet			= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	// Create pipeline layout
	vk::Unique<vk::VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));

	// Create image views
	{
		imageViewSrc = createImageView(ctx, **imageSrc, vk::VK_FORMAT_R8G8B8A8_UNORM);
		imageViewDst = createImageView(ctx, **imageDst, vk::VK_FORMAT_R8G8B8A8_UNORM);
	}

	// Update descriptor set information
	{
		vk::DescriptorSetUpdateBuilder	updateBuilder;

		vk::VkDescriptorImageInfo		descStorageImgDst	= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewDst, vk::VK_IMAGE_LAYOUT_GENERAL);
		vk::VkDescriptorImageInfo		descStorageImgSrc	= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewSrc, vk::VK_IMAGE_LAYOUT_GENERAL);

		updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImgDst);
		updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImgSrc);

		updateBuilder.update(vk, device);
	}

	// Create compute commands & submit
	{
		const vk::Unique<vk::VkFence>		fence		(vk::createFence(vk, device));
		vk::Unique<vk::VkPipeline>			pipeline	(makeComputePipeline(vk, device, *pipelineLayout, *computeShader));
		vk::Unique<vk::VkCommandBuffer>		cmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);
		endCommandBuffer(vk, *cmdBuffer);

		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));
	}

	// Calculate reference image
	calculateRef(*texture2D);

	// Validate result
	return validateResult(**imageDst, vk::VK_IMAGE_LAYOUT_GENERAL, *texture2D, refSampler);
}

void WorkgroupStorageTestInstance::calculateRef (tcu::Texture2D& texture2D)
{
	const tcu::PixelBufferAccess&	reference	= texture2D.getLevel(0);

	std::vector<tcu::IVec4>	sharedData(m_params.sharedMemorySize);
	for (deUint32 dataIdx = 0; dataIdx < m_params.sharedMemorySize; ++dataIdx)
		sharedData[dataIdx] = reference.getPixelInt(dataIdx % reference.getWidth(), dataIdx / reference.getWidth());

	for (int x = 0; x < reference.getWidth(); ++x)
	for (int y = 0; y < reference.getHeight(); ++y)
	{
		const int idx = (y * reference.getWidth() + x + 1) % m_params.sharedMemorySize;

		reference.setPixel(sharedData[idx], x, y);
	}
}

tcu::TestStatus WorkgroupStorageTestInstance::validateResult (vk::VkImage image, vk::VkImageLayout imageLayout, const tcu::Texture2D& texture2D, const tcu::Sampler& refSampler)
{
	de::Random			rnd			(getSeedValue(m_params));
	ValidationData		refData;

	for (int ndx = 0; ndx < 4; ++ndx)
	{
		const float		lod		= 0.0f;
		const float		cx		= rnd.getFloat(0.0f, 1.0f);
		const float		cy		= rnd.getFloat(0.0f, 1.0f);

		refData.coords[ndx] = tcu::Vec4(cx, cy, 0.0f, 0.0f);
		refData.values[ndx] = texture2D.sample(refSampler, cx, cy, lod);
	}

	if (!m_validator.validateImage(m_protectedContext, refData, image, vk::VK_FORMAT_R8G8B8A8_UNORM, imageLayout))
		return tcu::TestStatus::fail("Result validation failed");
	else
		return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup*	createWorkgroupStorageTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> workgroupGroup (new tcu::TestCaseGroup(testCtx, "workgroupstorage", "Workgroup storage tests"));

	static const deUint32 sharedMemSizes[] = { 1, 4, 5, 60, 101, 503 };

	for (int sharedMemSizeIdx = 0; sharedMemSizeIdx < DE_LENGTH_OF_ARRAY(sharedMemSizes); ++sharedMemSizeIdx)
	{
		std::string testName = std::string("memsize_") + de::toString(sharedMemSizes[sharedMemSizeIdx]);
		workgroupGroup->addChild(new WorkgroupStorageTestCase(testCtx, testName, "", Params(sharedMemSizes[sharedMemSizeIdx])));
	}

	return workgroupGroup.release();
}

} // ProtectedMem
} // vkt
