/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief YCbCr filtering tests.
 *//*--------------------------------------------------------------------*/

#include "tcuVectorUtil.hpp"
#include "tcuTexVerifierUtil.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktYCbCrFilteringTests.hpp"
#include "vktDrawUtil.hpp"
#include "vktYCbCrUtil.hpp"
#include "gluTextureTestUtil.hpp"
#include <string>
#include <vector>

using namespace vk;
using namespace vkt::drawutil;

namespace vkt
{
namespace ycbcr
{
namespace
{

using std::vector;
using std::string;
using tcu::TestLog;
using tcu::Sampler;
using namespace glu::TextureTestUtil;

class LinearFilteringTestInstance: public TestInstance
{
public:
	LinearFilteringTestInstance(Context& context, VkFormat format);
	~LinearFilteringTestInstance() = default;

protected:

	VkSamplerCreateInfo				getSamplerInfo				(VkFilter								minMagFilter,
																 const VkSamplerYcbcrConversionInfo*	samplerConversionInfo = DE_NULL);
	Move<VkDescriptorSetLayout>		createDescriptorSetLayout	(VkSampler sampler);
	Move<VkDescriptorPool>			createDescriptorPool		(void);
	Move<VkDescriptorSet>			createDescriptorSet			(VkDescriptorPool		descPool,
																 VkDescriptorSetLayout	descLayout);
	Move<VkSamplerYcbcrConversion>	createYCbCrConversion		(void);
	Move<VkImage>					createImage					(deUint32 width, deUint32 height);
	Move<VkImageView>				createImageView				(const VkSamplerYcbcrConversionInfo& samplerConversionInfo, VkImage image);
	void							bindImage					(VkDescriptorSet		descriptorSet,
																 VkImageView			imageView,
																 VkSampler				sampler);
	tcu::TestStatus					iterate						(void);
	void							getExplicitFilteringRefData	(const MultiPlaneImageData& imageData, vector<deUint8>& refData);
	void							getImplicitFilteringRefData	(const MultiPlaneImageData& imageData, vector<deUint8>& refData);


private:

	struct FilterCase
	{
		const tcu::UVec2 imageSize;
		const tcu::UVec2 renderSize;
	};

	const VkFormat				m_format;
	const DeviceInterface&		m_vkd;
	const VkDevice				m_device;
	int							m_caseIndex;
	const vector<FilterCase>	m_cases;
};

LinearFilteringTestInstance::LinearFilteringTestInstance(Context& context, VkFormat format)
	: TestInstance	(context)
	, m_format		(format)
	, m_vkd			(m_context.getDeviceInterface())
	, m_device		(m_context.getDevice())
	, m_caseIndex	(0)
	, m_cases		{
		{ { 8,  8}, {64, 64} },
		{ {64, 32}, {32, 64} }
	}
{
}

VkSamplerCreateInfo LinearFilteringTestInstance::getSamplerInfo(VkFilter minMagFilter, const VkSamplerYcbcrConversionInfo* samplerConversionInfo)
{
	return
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		samplerConversionInfo,
		0u,
		minMagFilter,								// magFilter
		minMagFilter,								// minFilter
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// mipmapMode
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeU
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeW
		0.0f,										// mipLodBias
		VK_FALSE,									// anisotropyEnable
		1.0f,										// maxAnisotropy
		VK_FALSE,									// compareEnable
		VK_COMPARE_OP_ALWAYS,						// compareOp
		0.0f,										// minLod
		0.0f,										// maxLod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// borderColor
		VK_FALSE,									// unnormalizedCoords
	};
}

Move<VkDescriptorSetLayout> LinearFilteringTestInstance::createDescriptorSetLayout(VkSampler sampler)
{
	const VkDescriptorSetLayoutBinding binding =
	{
		0u,												// binding
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1u,												// descriptorCount
		VK_SHADER_STAGE_ALL,
		&sampler
	};
	const VkDescriptorSetLayoutCreateInfo layoutInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		(VkDescriptorSetLayoutCreateFlags)0u,
		1u,
		&binding,
	};

	return ::createDescriptorSetLayout(m_vkd, m_device, &layoutInfo);
}

Move<VkDescriptorPool> LinearFilteringTestInstance::createDescriptorPool()
{
	const VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1u	},
	};
	const VkDescriptorPoolCreateInfo poolInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		(VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		1u,		// maxSets
		DE_LENGTH_OF_ARRAY(poolSizes),
		poolSizes,
	};

	return ::createDescriptorPool(m_vkd, m_device, &poolInfo);
}

Move<VkDescriptorSet> LinearFilteringTestInstance::createDescriptorSet(VkDescriptorPool			descPool,
																	   VkDescriptorSetLayout	descLayout)
{
	const VkDescriptorSetAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		descPool,
		1u,
		&descLayout,
	};

	return allocateDescriptorSet(m_vkd, m_device, &allocInfo);
}

Move<VkSamplerYcbcrConversion> LinearFilteringTestInstance::createYCbCrConversion()
{
	const VkSamplerYcbcrConversionCreateInfo conversionInfo =
	{
		VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
		DE_NULL,
		m_format,
		VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,
		VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		VK_CHROMA_LOCATION_MIDPOINT,
		VK_CHROMA_LOCATION_MIDPOINT,
		VK_FILTER_NEAREST,							// chromaFilter
		VK_FALSE,									// forceExplicitReconstruction
	};

	return createSamplerYcbcrConversion(m_vkd, m_device, &conversionInfo);
}

Move<VkImage> LinearFilteringTestInstance::createImage(deUint32 width, deUint32 height)
{
	VkImageCreateFlags			createFlags = 0u;
	const VkImageCreateInfo		createInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		createFlags,
		VK_IMAGE_TYPE_2D,
		m_format,
		makeExtent3D(width, height, 1u),
		1u,		// mipLevels
		1u,		// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		VK_IMAGE_LAYOUT_UNDEFINED,
	};

	return ::createImage(m_vkd, m_device, &createInfo);
}

Move<VkImageView> LinearFilteringTestInstance::createImageView(const VkSamplerYcbcrConversionInfo& samplerConversionInfo, VkImage image)
{
	const VkImageViewCreateInfo	viewInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		&samplerConversionInfo,
		(VkImageViewCreateFlags)0,
		image,
		VK_IMAGE_VIEW_TYPE_2D,
		m_format,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },
	};

	return ::createImageView(m_vkd, m_device, &viewInfo);
}

void LinearFilteringTestInstance::bindImage(VkDescriptorSet	descriptorSet,
											VkImageView		imageView,
											VkSampler		sampler)
{
	const VkDescriptorImageInfo imageInfo =
	{
		sampler,
		imageView,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
	const VkWriteDescriptorSet descriptorWrite =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		DE_NULL,
		descriptorSet,
		0u,		// dstBinding
		0u,		// dstArrayElement
		1u,		// descriptorCount
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		&imageInfo,
		(const VkDescriptorBufferInfo*)DE_NULL,
		(const VkBufferView*)DE_NULL,
	};

	m_vkd.updateDescriptorSets(m_device, 1u, &descriptorWrite, 0u, DE_NULL);
}

void LinearFilteringTestInstance::getExplicitFilteringRefData(const MultiPlaneImageData& imageData, vector<deUint8>& refData)
{
	const tcu::UVec2					imageSize				= m_cases[m_caseIndex].imageSize;
	const vk::PlanarFormatDescription&	planarFormatDescription = imageData.getDescription();
	const deUint8*						lumaData				= static_cast<const deUint8*>(imageData.getPlanePtr(0));
	const deUint8*						chromaBData				= static_cast<const deUint8*>(imageData.getPlanePtr(1));
	const deUint8*						chromaRData				= chromaBData;		// assuming 2 planes
	deUint32							chromaStride			= 2;
	deUint32							chromaOffset			= 1;

	if (planarFormatDescription.numPlanes == 3)
	{
		chromaRData		= static_cast<const deUint8*>(imageData.getPlanePtr(2));
		chromaStride	= 1;
		chromaOffset	= 0;
	}

	// associate nearest chroma sample with each luma sample
	vector<deUint8> intermediateImageData(imageSize.x() * imageSize.y() * 4, 255);
	for (deUint32 y = 0; y < imageSize.y(); ++y)
	{
		for (deUint32 x = 0; x < imageSize.x(); ++x)
		{
			deUint32 component						= x * 4 + imageSize.x() * y * 4;
			deUint32 chromaIndex					= x / 2 + (imageSize.x() / 2) * (y / 2);
			intermediateImageData[component]		= lumaData[x + imageSize.x() * y];
			intermediateImageData[component + 1]	= chromaBData[chromaStride * chromaIndex];
			intermediateImageData[component + 2]	= chromaRData[chromaStride * chromaIndex + chromaOffset];
		}
	}

	tcu::ConstPixelBufferAccess intermediateImage	(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), imageSize.x(), imageSize.y(), 1, intermediateImageData.data());
	const tcu::Texture2DView	intermediateTexView	(1u, &intermediateImage);
	const tcu::Sampler			refSampler			(mapVkSampler(getSamplerInfo(VK_FILTER_LINEAR)));
	const tcu::UVec2			renderSize			(m_cases[m_caseIndex].renderSize);

	// sample intermediate image and convert to gbr to generate reference image
	for (deUint32 y = 0; y < renderSize.y(); ++y)
	{
		float yCoord = ((float)y + 0.5f) / (float)renderSize.y();
		for (deUint32 x = 0; x < renderSize.x(); ++x)
		{
			float		xCoord		= ((float)x + 0.5f) / (float)renderSize.x();
			tcu::Vec4	color		= intermediateTexView.sample(refSampler, xCoord, yCoord, 0.0f);
			deUint32	texelIndex	= x * 4 + renderSize.x() * y * 4;
			refData[texelIndex + 1] = static_cast<deUint8>(255 * color[0]);		// g
			refData[texelIndex + 2] = static_cast<deUint8>(255 * color[1]);		// b
			refData[texelIndex]		= static_cast<deUint8>(255 * color[2]);		// r
		}
	}
}

void LinearFilteringTestInstance::getImplicitFilteringRefData(const MultiPlaneImageData& imageData, vector<deUint8>& refData)
{
	const tcu::UVec2			renderSize			(m_cases[m_caseIndex].renderSize);
	const VkSamplerCreateInfo	nSamplerCreateInfo	(getSamplerInfo(VK_FILTER_NEAREST));
	const VkSamplerCreateInfo	lSamplerCreateInfo	(getSamplerInfo(VK_FILTER_LINEAR));
	const tcu::Sampler			refSamplerNearest	(mapVkSampler(nSamplerCreateInfo));
	const tcu::Sampler			refSamplerLinear	(mapVkSampler(lSamplerCreateInfo));
	const deUint32				channelRemap[]		= { 1, 0, 2 };		// remap to have channels in order: Y Cr Cb
	const tcu::Sampler*			refSampler[]		=
	{
		&refSamplerLinear,
		&refSamplerNearest,
		&refSamplerNearest
	};

	for (deUint32 channelNdx = 0; channelNdx < 3; channelNdx++)
	{
		const tcu::ConstPixelBufferAccess	channelAccess		(imageData.getChannelAccess(channelNdx));
		const tcu::Texture2DView			refTexView			(1u, &channelAccess);
		const deUint32						orderedChannelNdx	(channelRemap[channelNdx]);

		for (deUint32 y = 0; y < renderSize.y(); ++y)
		{
			float yCoord = ((float)y + 0.5f) / (float)renderSize.y();
			for (deUint32 x = 0; x < renderSize.x(); ++x)
			{
				deUint32	texelIndex	= x * 4 + renderSize.x() * y * 4 + channelNdx;
				float		xCoord		= ((float)x + 0.5f) / (float)renderSize.x();
				refData[texelIndex]		= static_cast<deUint8>(255.0f * refTexView.sample(*refSampler[orderedChannelNdx], xCoord, yCoord, 0.0f)[0]);
			}
		}
	}
}

tcu::TestStatus LinearFilteringTestInstance::iterate(void)
{
	const tcu::UVec2						imageSize			(m_cases[m_caseIndex].imageSize);
	const tcu::UVec2						renderSize			(m_cases[m_caseIndex].renderSize);
	const auto&								instInt				(m_context.getInstanceInterface());
	auto									physicalDevice		(m_context.getPhysicalDevice());
	const Unique<VkSamplerYcbcrConversion>	conversion			(createYCbCrConversion());
	const VkSamplerYcbcrConversionInfo		samplerConvInfo		{ VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO, DE_NULL, *conversion };
	const VkSamplerCreateInfo				samplerCreateInfo	(getSamplerInfo(VK_FILTER_LINEAR, &samplerConvInfo));
	const Unique<VkSampler>					sampler				(createSampler(m_vkd, m_device, &samplerCreateInfo));
	const Unique<VkDescriptorSetLayout>		descLayout			(createDescriptorSetLayout(*sampler));
	const Unique<VkDescriptorPool>			descPool			(createDescriptorPool());
	const Unique<VkDescriptorSet>			descSet				(createDescriptorSet(*descPool, *descLayout));
	const Unique<VkImage>					testImage			(createImage(imageSize.x(), imageSize.y()));
	const vector<AllocationSp>				allocations			(allocateAndBindImageMemory(m_vkd, m_device, m_context.getDefaultAllocator(), *testImage, m_format, 0u));
	const Unique<VkImageView>				imageView			(createImageView(samplerConvInfo, *testImage));

	// create and bind image with test data
	MultiPlaneImageData imageData(m_format, imageSize);
	fillGradient(&imageData, tcu::Vec4(0.0f), tcu::Vec4(1.0f));
	uploadImage(m_vkd,
				m_device,
				m_context.getUniversalQueueFamilyIndex(),
				m_context.getDefaultAllocator(),
				*testImage,
				imageData,
				(VkAccessFlags)VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				0);
	bindImage(*descSet, *imageView, *sampler);

	const vector<tcu::Vec4> vertices =
	{
		{ -1.0f, -1.0f, 0.0f, 1.0f },
		{ +1.0f, -1.0f, 0.0f, 1.0f },
		{ -1.0f, +1.0f, 0.0f, 1.0f },
		{ +1.0f, +1.0f, 0.0f, 1.0f }
	};
	VulkanProgram program({
		VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,	m_context.getBinaryCollection().get("vert")),
		VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	m_context.getBinaryCollection().get("frag"))
	});
	program.descriptorSet		= *descSet;
	program.descriptorSetLayout = *descLayout;

	PipelineState		pipelineState		(m_context.getDeviceProperties().limits.subPixelPrecisionBits);
	const DrawCallData	drawCallData		(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices);
	FrameBufferState	frameBufferState	(renderSize.x(), renderSize.y());
	VulkanDrawContext	renderer			(m_context, frameBufferState);

	// render full screen quad
	renderer.registerDrawObject(pipelineState, program, drawCallData);
	renderer.draw();

	// get rendered image
	tcu::ConstPixelBufferAccess resImage(renderer.getColorPixels());

	vector<deUint8>					refData				(renderSize.x() * renderSize.y() * 4, 255);
	const VkFormatProperties		formatProperties	(getPhysicalDeviceFormatProperties(instInt, physicalDevice, m_format));
	const VkFormatFeatureFlags		featureFlags		(formatProperties.optimalTilingFeatures);
	const bool						explicitFiltering	(featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT);

	// generate reference image data
	if (explicitFiltering)
		getExplicitFilteringRefData(imageData, refData);
	else
		getImplicitFilteringRefData(imageData, refData);

	float							threshold			(0.01f);
	tcu::Vec4						thresholdVec		(threshold, threshold, threshold, 1.0f);
	tcu::TextureFormat				refFormat			(vk::mapVkFormat(frameBufferState.colorFormat));
	tcu::ConstPixelBufferAccess		refImage			(refFormat, renderSize.x(), renderSize.y(), 1, refData.data());

	// compare reference with the rendered image
	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "", refImage, resImage, thresholdVec, tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Invalid result");

	if (++m_caseIndex < (int)m_cases.size())
		return tcu::TestStatus::incomplete();
	return tcu::TestStatus::pass("Pass");
}

class LinearFilteringTestCase : public vkt::TestCase
{
public:
	LinearFilteringTestCase(tcu::TestContext &context, const char* name, const char* description, VkFormat format);

protected:
	void				checkSupport(Context& context) const;
	vkt::TestInstance*	createInstance(vkt::Context& context) const;
	void				initPrograms(SourceCollections& programCollection) const;

private:
	VkFormat			m_format;
};

LinearFilteringTestCase::LinearFilteringTestCase(tcu::TestContext &context, const char* name, const char* description, VkFormat format)
	: TestCase(context, name, description)
	, m_format(format)
{
}

void LinearFilteringTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");

	const auto&					instInt				= context.getInstanceInterface();
	auto						physicalDevice		= context.getPhysicalDevice();
	const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(instInt, physicalDevice, m_format);
	const VkFormatFeatureFlags	featureFlags		= formatProperties.optimalTilingFeatures;

	if ((featureFlags & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) == 0)
		TCU_THROW(NotSupportedError, "YCbCr conversion is not supported for format");

	if ((featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0)
		TCU_THROW(NotSupportedError, "Linear filtering not supported for format");

	if ((featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT) == 0)
		TCU_THROW(NotSupportedError, "Different chroma, min, and mag filters not supported for format");
}

vkt::TestInstance* LinearFilteringTestCase::createInstance(vkt::Context& context) const
{
	return new LinearFilteringTestInstance(context, m_format);
}

void LinearFilteringTestCase::initPrograms(SourceCollections& programCollection) const
{
	static const char* vertShader =
		"#version 450\n"
		"precision mediump int; precision highp float;\n"
		"layout(location = 0) in vec4 a_position;\n"
		"layout(location = 0) out vec2 v_texCoord;\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  v_texCoord = a_position.xy * 0.5 + 0.5;\n"
		"  gl_Position = a_position;\n"
		"}\n";

	static const char* fragShader =
		"#version 450\n"
		"precision mediump int; precision highp float;\n"
		"layout(location = 0) in vec2 v_texCoord;\n"
		"layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
		"layout (set=0, binding=0) uniform sampler2D u_sampler;\n"
		"void main (void)\n"
		"{\n"
		"  dEQP_FragColor = vec4(texture(u_sampler, v_texCoord));\n"
		"}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader);
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader);
}

} // anonymous

tcu::TestCaseGroup* createFilteringTests (tcu::TestContext& testCtx)
{
	struct YCbCrFormatData
	{
		const char* const	name;
		const VkFormat		format;
	};

	static const std::vector<YCbCrFormatData> ycbcrFormats =
	{
		{ "g8_b8_r8_3plane_420_unorm",	VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM		},
		{ "g8_b8r8_2plane_420_unorm",	VK_FORMAT_G8_B8R8_2PLANE_420_UNORM		},
	};

	de::MovePtr<tcu::TestCaseGroup> filteringTests(new tcu::TestCaseGroup(testCtx, "filtering",	"YCbCr filtering tests"));

	for (const auto& ycbcrFormat : ycbcrFormats)
	{
		const std::string name = std::string("linear_sampler_") + ycbcrFormat.name;
		filteringTests->addChild(new LinearFilteringTestCase(filteringTests->getTestContext(), name.c_str(), "", ycbcrFormat.format));
	}

	return filteringTests.release();
}

} // ycbcr
} // vkt
