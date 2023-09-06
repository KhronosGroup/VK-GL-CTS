/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Protected memory stack tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemStackTests.hpp"

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
	deUint32	stackSize;
	deUint32	imageWidth;
	deUint32	imageHeight;

	Params (deUint32	stackSize_)
		: stackSize	(stackSize_)
	{
		// Find suitable image dimensions based on stack memory size
		imageWidth = 1;
		imageHeight = 1;
		bool increaseWidth = true;
		while (imageWidth * imageHeight < stackSize)
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
	return deInt32Hash(params.stackSize);
}

class StackTestInstance : public ProtectedTestInstance
{
public:
								StackTestInstance	(Context&				ctx,
													 const ImageValidator&	validator,
													 const Params&			params);
	virtual tcu::TestStatus		iterate				(void);

private:
	de::MovePtr<tcu::Texture2D>	createTestTexture2D	(void);
	bool						validateResult		(vk::VkImage			image,
													 vk::VkImageLayout imageLayout,
													 const tcu::Texture2D&	texture2D,
													 const tcu::Sampler&	refSampler);
	void						calculateRef		(tcu::Texture2D&		texture2D);

	const ImageValidator&		m_validator;
	const Params&				m_params;
};

class StackTestCase : public TestCase
{
public:
								StackTestCase	(tcu::TestContext&		testCtx,
												 const std::string&		name,
												 const std::string&		description,
												 const Params&			params)
									: TestCase		(testCtx, name, description)
									, m_validator	(vk::VK_FORMAT_R8G8B8A8_UNORM)
									, m_params		(params)
								{
								}

	virtual						~StackTestCase	(void) {}
	virtual TestInstance*		createInstance	(Context& ctx) const
								{
									return new StackTestInstance(ctx, m_validator, m_params);
								}
	virtual void				initPrograms	(vk::SourceCollections& programCollection) const;

private:
	ImageValidator				m_validator;
	Params						m_params;
};

void StackTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	m_validator.initPrograms(programCollection);

	// Test validates handling of protected memory allocated on stack.
	// The test copies protected memory content into temporary variable allocated inside function p.
	// Thus test forces protected content to appear on stack.
	// Function p() returns specified protected memory element from the variable allocated on stack.
	// Function u() returns specified protected memory element from the global variable.
	// Values returned by p() and u() should be same.
	// Test is repeated 2 times () in shader to avoid coincidental matches.
	// In case of any mismatches it is signalized to inherited verifier function by setting 0 in result store image.
	// Each invocation validates particular element (bytes) on stack.
	// Number of invocations matches stack size specified in test parameters.
	std::string comp =
		std::string() +
		"#version 450\n"
		"layout(local_size_x = " + de::toString(m_params.imageWidth) + ", local_size_y = " + de::toString(m_params.imageHeight) + ", local_size_z = 1) in;\n"
		"layout(set = 0, binding = 0, rgba8) writeonly uniform highp image2D u_resultImage;\n"
		"layout(set = 0, binding = 1, rgba8) readonly uniform highp image2D u_srcImage;\n"
		"vec4 protectedData[" + de::toString(m_params.stackSize) + "];\n"
		"\n"
		"vec4 p(int idx)\n"
		"{\n"
		"    vec4 localData[" + de::toString(m_params.stackSize) + "];\n"
		"    for (int i = 0; i < " + de::toString(m_params.stackSize) + "; i++)\n"
		"        localData[i] = protectedData[i];\n"
		"    return localData[idx];\n"
		"}\n"
		"\n"
		"vec4 u(int idx)\n"
		"{\n"
		"    return protectedData[idx];\n"
		"}\n"
		"\n"
		"void main() {\n"
		"    const int n = " + de::toString(m_params.stackSize) + ";\n"
		"    int m = 0;\n"
		"    int w = " + de::toString(m_params.imageWidth) + ";\n"
		"    int gx = int(gl_GlobalInvocationID.x);\n"
		"    int gy = int(gl_GlobalInvocationID.y);\n"
		"    int checked_ndx = gy * w + gx;\n"
		"    vec4 outColor;\n"
		"\n"
		"    for (int j = 0; j < 2; j++)\n"
		"    {\n"
		"        for (int i = 0; i < n; i++)\n"
		"        {\n"
		"            const int idx = (i + j) % n;\n"
		"            protectedData[i] = imageLoad(u_srcImage, ivec2(idx % w, idx / w));\n"
		"        }\n"
		"\n"
		"        vec4 vp = p(checked_ndx);\n"
		"        vec4 vu = u(checked_ndx);\n"
		"        if (any(notEqual(vp,vu)))\n"
		"            m++;\n"
		"    }\n"
		"\n"
		"    if (m <= 0)\n"
		"        outColor = vec4(0.0f);\n"
		"    else\n"
		"        outColor = vec4(1.0f);\n"
		"    imageStore(u_resultImage, ivec2(gx, gy), outColor);\n"
		"}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(comp);
}

StackTestInstance::StackTestInstance (Context&				ctx,
									  const ImageValidator&	validator,
									  const Params&			params)
	: ProtectedTestInstance	(ctx)
	, m_validator			(validator)
	, m_params				(params)
{
}

de::MovePtr<tcu::Texture2D> StackTestInstance::createTestTexture2D (void)
{
	const tcu::TextureFormat		texFmt		= mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM);
	de::MovePtr<tcu::Texture2D>		texture2D	(new tcu::Texture2D(texFmt, m_params.imageWidth, m_params.imageHeight));

	texture2D->allocLevel(0);

	const tcu::PixelBufferAccess&	level		= texture2D->getLevel(0);

	fillWithUniqueColors(level, getSeedValue(m_params));

	return texture2D;
}

tcu::TestStatus StackTestInstance::iterate (void)
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

	// Calculate reference image
	calculateRef(*texture2D);

	bool result = true;

	// Create compute commands & submit
	// Command buffer load is repeated 8 times () to avoid coincidental matches.
	for (int i = 0; (i < 8) && (result == true); i++)
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

		VK_CHECK(vk.waitForFences(device, 1u, &*fence, VK_TRUE, ~0ull));

	    result = validateResult(**imageDst, vk::VK_IMAGE_LAYOUT_GENERAL, *texture2D, refSampler);
	}

	if (result == true)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Result validation failed");
}

void StackTestInstance::calculateRef (tcu::Texture2D& texture2D)
{
	const tcu::PixelBufferAccess&	reference	= texture2D.getLevel(0);
	const tcu::IVec4				zero;

	for (int x = 0; x < reference.getWidth(); ++x)
	for (int y = 0; y < reference.getHeight(); ++y)
		reference.setPixel(zero, x, y);
}

bool StackTestInstance::validateResult (vk::VkImage image, vk::VkImageLayout imageLayout, const tcu::Texture2D& texture2D, const tcu::Sampler& refSampler)
{
	de::Random			rnd			(getSeedValue(m_params));
	ValidationData		refData;

	for (int ndx = 0; ndx < 4; ++ndx)
	{
		const float	lod	= 0.0f;
		const float	cx	= rnd.getFloat(0.0f, 1.0f);
		const float	cy	= rnd.getFloat(0.0f, 1.0f);

		refData.coords[ndx] = tcu::Vec4(cx, cy, 0.0f, 0.0f);
		refData.values[ndx] = texture2D.sample(refSampler, cx, cy, lod);
	}

	if (!m_validator.validateImage(m_protectedContext, refData, image, vk::VK_FORMAT_R8G8B8A8_UNORM, imageLayout))
		return false;
	else
		return true;
}

} // anonymous

tcu::TestCaseGroup*	createStackTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> stackGroup (new tcu::TestCaseGroup(testCtx, "stack", "Protected memory stack tests"));

	static const deUint32 stackMemSizes[] = { 32, 64, 128, 256, 512, 1024 };

	for (int stackMemSizeIdx = 0; stackMemSizeIdx < DE_LENGTH_OF_ARRAY(stackMemSizes); ++stackMemSizeIdx)
	{
		std::string testName = std::string("stacksize_") + de::toString(stackMemSizes[stackMemSizeIdx]);

		stackGroup->addChild(new StackTestCase(testCtx, testName, "", Params(stackMemSizes[stackMemSizeIdx])));
	}

	return stackGroup.release();
}

} // ProtectedMem
} // vkt
