/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2021 The Khronos Group Inc.
* Copyright (c) 2023 LunarG, Inc.
* Copyright (c) 2023 Nintendo
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
*//*
 * \file
 * \brief Tests for Color Write Enable
*//*--------------------------------------------------------------------*/

#include "vktPipelineColorWriteEnableTests.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuVector.hpp"
#include "tcuMaybe.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <vector>
#include <sstream>
#include <algorithm>
#include <utility>
#include <iterator>
#include <string>
#include <limits>
#include <memory>
#include <functional>

namespace vkt
{
namespace pipeline
{

namespace
{

// Framebuffer size.
constexpr deUint32 kFramebufferWidth  = 64u;
constexpr deUint32 kFramebufferHeight = 64u;

// Image formats.
constexpr	vk::VkFormat	kColorFormat	= vk::VK_FORMAT_R8G8B8A8_UNORM;
const		tcu::Vec4		kColorThreshold	(0.005f); // 1/255 < 0.005 < 2/255.

constexpr	deUint32		kNumColorAttachments = 3u;

const vk::VkFormat kDepthStencilFormats[] =
{
	vk::VK_FORMAT_D32_SFLOAT_S8_UINT,
	vk::VK_FORMAT_D24_UNORM_S8_UINT
};

constexpr auto kCoordsSize = static_cast<deUint32>(2 * sizeof(float));

using Bool32Vec = std::vector<vk::VkBool32>;

// Generic, to be used with any state than can be set statically and, as an option, dynamically.
template<typename T>
struct StaticAndDynamicPair
{
	T				staticValue;
	tcu::Maybe<T>	dynamicValue;

	// Helper constructor to set a static value and no dynamic value.
	StaticAndDynamicPair (const T& value)
		: staticValue	(value)
		, dynamicValue	(tcu::Nothing)
	{
	}

	// Helper constructor to set both.
	StaticAndDynamicPair (const T& sVal, const T& dVal)
		: staticValue	(sVal)
		, dynamicValue	(tcu::just<T>(dVal))
	{
	}

	// If the dynamic value is present, swap static and dynamic values.
	void swapValues (void)
	{
		if (!dynamicValue)
			return;
		std::swap(staticValue, dynamicValue.get());
	}
};

const tcu::Vec4	kDefaultTriangleColor (0.0f, 0.0f, 1.0f, 1.0f);	// Opaque blue.
const tcu::Vec4	kDefaultClearColor    (0.0f, 0.0f, 0.0f, 1.0f);	// Opaque black.

struct MeshParams
{
	tcu::Vec4	color;
	float		depth;
	float		scaleX;
	float		scaleY;
	float		offsetX;
	float		offsetY;

	MeshParams (const tcu::Vec4&	color_		= kDefaultTriangleColor,
				float				depth_		= 0.0f,
				float				scaleX_		= 1.0f,
				float				scaleY_		= 1.0f,
				float				offsetX_	= 0.0f,
				float				offsetY_	= 0.0f)
		: color		(color_)
		, depth		(depth_)
		, scaleX	(scaleX_)
		, scaleY	(scaleY_)
		, offsetX	(offsetX_)
		, offsetY	(offsetY_)
	{}
};

enum class SequenceOrdering
{
	CMD_BUFFER_START	= 0,	// Set state at the start of the command buffer.
	BEFORE_DRAW			= 1,	// After binding dynamic pipeline and just before drawing.
	BETWEEN_PIPELINES	= 2,	// After a static state pipeline has been bound but before the dynamic state pipeline has been bound.
	AFTER_PIPELINES		= 3,	// After a static state pipeline and a second dynamic state pipeline have been bound.
	BEFORE_GOOD_STATIC	= 4,	// Before a static state pipeline with the correct values has been bound.
	TWO_DRAWS_DYNAMIC	= 5,	// Bind bad static pipeline and draw, followed by binding correct dynamic pipeline and drawing again.
	TWO_DRAWS_STATIC	= 6,	// Bind bad dynamic pipeline and draw, followed by binding correct static pipeline and drawing again.
};

struct TestConfig
{
	vk::PipelineConstructionType	pipelineConstructionType;

	// Main sequence ordering.
	SequenceOrdering				sequenceOrdering;

	// Drawing parameters.
	MeshParams						meshParams;

	// Clearing parameters for the framebuffer.
	tcu::Vec4						clearColorValue;
	float							clearDepthValue;

	// Channels to enable
	tcu::BVec4						channelMask;

	// Expected output in the attachments.
	std::vector<tcu::Vec4>			expectedColor;
	float							expectedDepth;

	// Static and dynamic pipeline configuration.
	StaticAndDynamicPair<Bool32Vec>	colorWriteEnableConfig;

	// Sane defaults.
	TestConfig (vk::PipelineConstructionType constructionType, SequenceOrdering ordering)
		: pipelineConstructionType		(constructionType)
		, sequenceOrdering				(ordering)
		, clearColorValue				(kDefaultClearColor)
		, clearDepthValue				(1.0f)
		, expectedColor					(kNumColorAttachments, kDefaultTriangleColor)
		, expectedDepth					(1.0f)
		, colorWriteEnableConfig		(Bool32Vec(1u, VK_TRUE))
		, m_swappedValues				(false)
	{
	}

	// Returns true if we should use the static and dynamic values exchanged.
	// This makes the static part of the pipeline have the actual expected values.
	bool isReversed () const
	{
		return (sequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				sequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC);
	}

	// Swaps static and dynamic configuration values.
	void swapValues ()
	{
		colorWriteEnableConfig.swapValues();
		m_swappedValues = !m_swappedValues;
	}

	// Returns the number of iterations when recording commands.
	deUint32 numIterations () const
	{
		deUint32 iterations = 0u;

		switch (sequenceOrdering)
		{
		case SequenceOrdering::TWO_DRAWS_DYNAMIC:
		case SequenceOrdering::TWO_DRAWS_STATIC:
			iterations = 2u;
			break;
		default:
			iterations = 1u;
			break;
		}

		return iterations;
	}

private:
	// Color Write Enable cases as created by createColorWriteEnableTests() are based on the assumption that, when a state
	// has a static and a dynamic value configured at the same time, the static value is wrong and the dynamic value will give
	// expected results. That's appropriate for most test variants, but in some others we want to reverse the situation: a dynamic
	// pipeline with wrong values and a static one with good values.
	//
	// Instead of modifying how tests are created, we use isReversed() and swapValues() above, allowing us to swap static and
	// dynamic values and to know if we should do it for a given test case. However, we need to know were the good value is at any
	// given point in time in order to correctly answer some questions while running the test. m_swappedValues tracks that state.
	bool m_swappedValues;
};

struct PushConstants
{
	tcu::Vec4	triangleColor;
	float		meshDepth;
	float		scaleX;
	float		scaleY;
	float		offsetX;
	float		offsetY;
};

class ColorWriteEnableTest : public vkt::TestCase
{
public:
							ColorWriteEnableTest		(tcu::TestContext& testCtx, const std::string& name, const TestConfig& testConfig);
	virtual					~ColorWriteEnableTest		(void) {}

	virtual void			checkSupport					(Context& context) const;
	virtual void			initPrograms					(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance					(Context& context) const;

private:
	TestConfig				m_testConfig;
};

class ColorWriteEnableInstance : public vkt::TestInstance
{
public:
								ColorWriteEnableInstance	(Context& context, const TestConfig& testConfig);
	virtual						~ColorWriteEnableInstance	(void) {}

	virtual tcu::TestStatus		iterate							(void);

private:
	TestConfig					m_testConfig;
};

ColorWriteEnableTest::ColorWriteEnableTest (tcu::TestContext& testCtx, const std::string& name, const TestConfig& testConfig)
	: vkt::TestCase	(testCtx, name)
	, m_testConfig	(testConfig)
{
}

void ColorWriteEnableTest::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	// This is always required.
	context.requireDeviceFunctionality("VK_EXT_color_write_enable");

	// Check color image format support (depth/stencil will be chosen at runtime).
	const vk::VkFormatFeatureFlags	kColorFeatures	= (vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
	const auto						colorProperties	= vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kColorFormat);

	if ((colorProperties.optimalTilingFeatures & kColorFeatures) != kColorFeatures)
		TCU_THROW(NotSupportedError, "Required color image features not supported");

	checkPipelineConstructionRequirements(vki, physicalDevice, m_testConfig.pipelineConstructionType);
}

void ColorWriteEnableTest::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream pushSource;
	pushSource
		<< "layout(push_constant, std430) uniform PushConstantsBlock {\n"
		<< "    vec4  triangleColor;\n"
		<< "    float depthValue;\n"
		<< "    float scaleX;\n"
		<< "    float scaleY;\n"
		<< "    float offsetX;\n"
		<< "    float offsetY;\n"
		<< "} pushConstants;\n"
		;

	std::ostringstream vertSource;
	vertSource
		<< "#version 450\n"
		<< pushSource.str()
		<< "layout(location=0) in vec2 position;\n"
		<< "out gl_PerVertex\n"
		<< "{\n"
		<< "    vec4 gl_Position;\n"
		<< "};\n"
		<< "void main() {\n"
		<< "    vec2 vertexCoords = position;\n"
		<< "    gl_Position = vec4(vertexCoords.x * pushConstants.scaleX + pushConstants.offsetX, vertexCoords.y * pushConstants.scaleY + pushConstants.offsetY, pushConstants.depthValue, 1.0);\n"
		<< "}\n"
		;

	std::ostringstream fragOutputs;
	std::ostringstream colorWrite;
	for (deUint32 i = 0u; i < kNumColorAttachments; ++i)
	{
		fragOutputs << "layout(location=" << i << ") out vec4 color" << i << ";\n";
		colorWrite << "    color" << i << " = pushConstants.triangleColor * " << powf(0.5f, static_cast<float>(i)) << ";\n";
	}

	std::ostringstream fragSource;
	fragSource
		<< "#version 450\n"
		<< pushSource.str()
		<< fragOutputs.str()
		<< "void main() {\n"
		<< colorWrite.str()
		<< "}\n"
		;

	programCollection.glslSources.add("vert") << glu::VertexSource(vertSource.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragSource.str());
}

TestInstance* ColorWriteEnableTest::createInstance (Context& context) const
{
	return new ColorWriteEnableInstance(context, m_testConfig);
}

ColorWriteEnableInstance::ColorWriteEnableInstance(Context& context, const TestConfig& testConfig)
	: vkt::TestInstance	(context)
	, m_testConfig		(testConfig)
{
}

void logErrors(tcu::TestLog& log, const std::string& setName, const std::string& setDesc, const tcu::ConstPixelBufferAccess& result, const tcu::ConstPixelBufferAccess& errorMask)
{
	log << tcu::TestLog::ImageSet(setName, setDesc)
		<< tcu::TestLog::Image(setName + "Result", "Result image", result)
		<< tcu::TestLog::Image(setName + "ErrorMask", "Error mask with errors marked in red", errorMask)
		<< tcu::TestLog::EndImageSet;
}

// Sets values for dynamic states if needed according to the test configuration.
void setDynamicStates(const TestConfig& testConfig, const vk::DeviceInterface& vkd, vk::VkCommandBuffer cmdBuffer)
{
	if (testConfig.colorWriteEnableConfig.dynamicValue)
	{
		const auto& colorWriteEnables = testConfig.colorWriteEnableConfig.dynamicValue.get();
		vkd.cmdSetColorWriteEnableEXT(cmdBuffer, static_cast<deUint32>(colorWriteEnables.size()), colorWriteEnables.data());
	}
}

tcu::TestStatus ColorWriteEnableInstance::iterate (void)
{
	using ImageWithMemoryVec	= std::vector<std::unique_ptr<vk::ImageWithMemory>>;
	using ImageViewVec			= std::vector<vk::Move<vk::VkImageView>>;
	using RenderPassVec			= std::vector<vk::RenderPassWrapper>;

	const auto&	vki					= m_context.getInstanceInterface();
	const auto&	vkd					= m_context.getDeviceInterface();
	const auto	physicalDevice		= m_context.getPhysicalDevice();
	const auto	device				= m_context.getDevice();
	auto&		allocator			= m_context.getDefaultAllocator();
	const auto	queue				= m_context.getUniversalQueue();
	const auto	queueIndex			= m_context.getUniversalQueueFamilyIndex();
	auto&		log					= m_context.getTestContext().getLog();

	const auto	kReversed			= m_testConfig.isReversed();
	const auto	kNumIterations		= m_testConfig.numIterations();
	const auto	kSequenceOrdering	= m_testConfig.sequenceOrdering;

	const auto						kFramebufferExtent	= vk::makeExtent3D(kFramebufferWidth, kFramebufferHeight, 1u);
	const vk::VkImageUsageFlags		kColorUsage			= (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const vk::VkImageUsageFlags		kDSUsage			= (vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const vk::VkFormatFeatureFlags	kDSFeatures			= (vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);

	// Choose depth/stencil format.
	vk::VkFormat dsFormat = vk::VK_FORMAT_UNDEFINED;

	for (int formatIdx = 0; formatIdx < DE_LENGTH_OF_ARRAY(kDepthStencilFormats); ++formatIdx)
	{
		const auto dsProperties = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kDepthStencilFormats[formatIdx]);
		if ((dsProperties.optimalTilingFeatures & kDSFeatures) == kDSFeatures)
		{
			dsFormat = kDepthStencilFormats[formatIdx];
			break;
		}
	}

	// Note: Not Supported insted of Fail because the transfer feature is not mandatory.
	if (dsFormat == vk::VK_FORMAT_UNDEFINED)
		TCU_THROW(NotSupportedError, "Required depth/stencil image features not supported");
	log << tcu::TestLog::Message << "Chosen depth/stencil format: " << dsFormat << tcu::TestLog::EndMessage;

	// Swap static and dynamic values in the test configuration so the static pipeline ends up with the expected values for cases
	// where we will bind the static pipeline last before drawing.
	if (kReversed)
		m_testConfig.swapValues();

	// Create color and depth/stencil images.
	ImageWithMemoryVec colorImages;
	ImageWithMemoryVec dsImages;

	const vk::VkImageCreateInfo colorImageInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		kColorFormat,								//	VkFormat				format;
		kFramebufferExtent,							//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		kColorUsage,								//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		1u,											//	deUint32				queueFamilyIndexCount;
		&queueIndex,								//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	for (deUint32 i = 0u; i < kNumIterations * kNumColorAttachments; ++i)
		colorImages.emplace_back(new vk::ImageWithMemory(vkd, device, allocator, colorImageInfo, vk::MemoryRequirement::Any));

	const vk::VkImageCreateInfo dsImageInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		dsFormat,									//	VkFormat				format;
		kFramebufferExtent,							//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		kDSUsage,									//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		1u,											//	deUint32				queueFamilyIndexCount;
		&queueIndex,								//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	for (deUint32 i = 0u; i < kNumIterations; ++i)
		dsImages.emplace_back(new vk::ImageWithMemory(vkd, device, allocator, dsImageInfo, vk::MemoryRequirement::Any));

	const auto colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto dsSubresourceRange		= vk::makeImageSubresourceRange((vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);

	ImageViewVec colorImageViews;
	ImageViewVec dsImageViews;

	for (const auto& img : colorImages)
		colorImageViews.emplace_back(vk::makeImageView(vkd, device, img->get(), vk::VK_IMAGE_VIEW_TYPE_2D, kColorFormat, colorSubresourceRange));

	for (const auto& img : dsImages)
		dsImageViews.emplace_back(vk::makeImageView(vkd, device, img->get(), vk::VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSubresourceRange));

	// Vertex buffer.
	// Full-screen triangle fan with 6 vertices.
	//
	// 4        3        2
	//  +-------+-------+
	//  |X      X      X|
	//  | X     X     X |
	//  |  X    X    X  |
	//  |   X   X   X   |
	//  |    X  X  X    |
	//  |     X X X     |
	//  |      XXX      |
	//  +-------+-------+
	// 5        0        1
	std::vector<float> vertices = {
		 0.0f,  1.0f,
		 1.0f,  1.0f,
		 1.0f, -1.0f,
		 0.0f, -1.0f,
		-1.0f, -1.0f,
		-1.0f,  1.0f,
	};

	const auto vertDataSize				= vertices.size() * sizeof(float);
	const auto vertBufferInfo			= vk::makeBufferCreateInfo(static_cast<vk::VkDeviceSize>(vertDataSize), vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	vk::BufferWithMemory vertBuffer		(vkd, device, allocator, vertBufferInfo, vk::MemoryRequirement::HostVisible);
	auto& alloc							= vertBuffer.getAllocation();

	deMemcpy(reinterpret_cast<char*>(alloc.getHostPtr()), vertices.data(), vertDataSize);
	vk::flushAlloc(vkd, device, alloc);

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Pipeline layout.
	vk::VkShaderStageFlags pushConstantStageFlags = (vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT);

	const vk::VkPushConstantRange pushConstantRange =
	{
		pushConstantStageFlags,							//	VkShaderStageFlags	stageFlags;
		0u,												//	deUint32			offset;
		static_cast<deUint32>(sizeof(PushConstants)),	//	deUint32			size;
	};

	const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,											//	const void*						pNext;
		0u,													//	VkPipelineLayoutCreateFlags		flags;
		1u,													//	deUint32						setLayoutCount;
		&descriptorSetLayout.get(),							//	const VkDescriptorSetLayout*	pSetLayouts;
		1u,													//	deUint32						pushConstantRangeCount;
		&pushConstantRange,									//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const vk::PipelineLayoutWrapper pipelineLayout (m_testConfig.pipelineConstructionType, vkd, device, &pipelineLayoutCreateInfo);

	// Render pass with single subpass.
	std::vector<vk::VkAttachmentReference> colorAttachmentReference;
	for (deUint32 i = 0u; i < kNumColorAttachments; ++i)
	{
		colorAttachmentReference.push_back(vk::VkAttachmentReference
			{
				i,												//	deUint32		attachment;
				vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	//	VkImageLayout	layout;
			}
		);
	}

	const vk::VkAttachmentReference dsAttachmentReference =
	{
		kNumColorAttachments,									//	deUint32		attachment;
		vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	//	VkImageLayout	layout;
	};

	const vk::VkSubpassDescription subpassDescription =
	{
		0u,										//	VkSubpassDescriptionFlags		flags;
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,	//	VkPipelineBindPoint				pipelineBindPoint;
		0u,										//	deUint32						inputAttachmentCount;
		nullptr,								//	const VkAttachmentReference*	pInputAttachments;
		kNumColorAttachments,					//	deUint32						colorAttachmentCount;
		colorAttachmentReference.data(),		//	const VkAttachmentReference*	pColorAttachments;
		nullptr,								//	const VkAttachmentReference*	pResolveAttachments;
		&dsAttachmentReference,					//	const VkAttachmentReference*	pDepthStencilAttachment;
		0u,										//	deUint32						preserveAttachmentCount;
		nullptr,								//	const deUint32*					pPreserveAttachments;
	};

	std::vector<vk::VkAttachmentDescription> attachmentDescriptions(
		kNumColorAttachments,
		vk::VkAttachmentDescription
		{
			0u,												//	VkAttachmentDescriptionFlags	flags;
			kColorFormat,									//	VkFormat						format;
			vk::VK_SAMPLE_COUNT_1_BIT,						//	VkSampleCountFlagBits			samples;
			vk::VK_ATTACHMENT_LOAD_OP_CLEAR,				//	VkAttachmentLoadOp				loadOp;
			vk::VK_ATTACHMENT_STORE_OP_STORE,				//	VkAttachmentStoreOp				storeOp;
			vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
			vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
		}
	);

	attachmentDescriptions.push_back(vk::VkAttachmentDescription
	{
		0u,														//	VkAttachmentDescriptionFlags	flags;
		dsFormat,												//	VkFormat						format;
		vk::VK_SAMPLE_COUNT_1_BIT,								//	VkSampleCountFlagBits			samples;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						//	VkAttachmentLoadOp				loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						//	VkAttachmentStoreOp				storeOp;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						//	VkAttachmentLoadOp				stencilLoadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						//	VkAttachmentStoreOp				stencilStoreOp;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,							//	VkImageLayout					initialLayout;
		vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
	});

	const vk::VkRenderPassCreateInfo renderPassCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			//	VkStructureType					sType;
		nullptr,												//	const void*						pNext;
		0u,														//	VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachmentDescriptions.size()),	//	deUint32						attachmentCount;
		attachmentDescriptions.data(),							//	const VkAttachmentDescription*	pAttachments;
		1u,														//	deUint32						subpassCount;
		&subpassDescription,									//	const VkSubpassDescription*		pSubpasses;
		0u,														//	deUint32						dependencyCount;
		nullptr,												//	const VkSubpassDependency*		pDependencies;
	};

	// Framebuffers.
	RenderPassVec framebuffers;

	DE_ASSERT(colorImageViews.size() == dsImageViews.size() * kNumColorAttachments);
	for (size_t imgIdx = 0; imgIdx < dsImageViews.size(); ++imgIdx)
	{
		std::vector<vk::VkImage>		images;
		std::vector<vk::VkImageView>	attachments;
		for (deUint32 i = 0u; i < kNumColorAttachments; ++i)
		{
			images.push_back(colorImages[imgIdx * kNumColorAttachments + i].get()->get());
			attachments.push_back(colorImageViews[imgIdx * kNumColorAttachments + i].get());
		}

		images.push_back(**(dsImages[imgIdx]));
		attachments.push_back(dsImageViews[imgIdx].get());

		framebuffers.emplace_back(vk::RenderPassWrapper(m_testConfig.pipelineConstructionType, vkd, device, &renderPassCreateInfo));

		const vk::VkFramebufferCreateInfo framebufferCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	//	VkStructureType				sType;
			nullptr,										//	const void*					pNext;
			0u,												//	VkFramebufferCreateFlags	flags;
			framebuffers[imgIdx].get(),						//	VkRenderPass				renderPass;
			static_cast<deUint32>(attachments.size()),		//	deUint32					attachmentCount;
			attachments.data(),								//	const VkImageView*			pAttachments;
			kFramebufferWidth,								//	deUint32					width;
			kFramebufferHeight,								//	deUint32					height;
			1u,												//	deUint32					layers;
		};

		framebuffers[imgIdx].createFramebuffer(vkd, device, &framebufferCreateInfo, images);
	}

	// Shader modules.
	const auto	vertModule = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto	fragModule = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

	// Input state.
	const auto vertexBinding = vk::makeVertexInputBindingDescription(0u, kCoordsSize, vk::VK_VERTEX_INPUT_RATE_VERTEX);
	const std::vector<vk::VkVertexInputAttributeDescription> vertexAttributes = {
		vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u)
	};

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		1u,																//	deUint32									vertexBindingDescriptionCount;
		&vertexBinding,													//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		static_cast<deUint32>(vertexAttributes.size()),					//	deUint32									vertexAttributeDescriptionCount;
		vertexAttributes.data(),										//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	// Input assembly.
	const vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineInputAssemblyStateCreateFlags	flags;
		vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,								//	VkPrimitiveTopology						topology;
		VK_FALSE,															//	VkBool32								primitiveRestartEnable;
	};

	// Viewport state.
	const std::vector<vk::VkViewport>	viewport	{ vk::makeViewport(kFramebufferWidth, kFramebufferHeight) };
	const std::vector<vk::VkRect2D>		scissor		{ vk::makeRect2D(kFramebufferWidth, kFramebufferHeight) };

	// Rasterization state.
	const vk::VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														//	VkBool32								depthClampEnable;
		VK_FALSE,														//	VkBool32								rasterizerDiscardEnable;
		vk::VK_POLYGON_MODE_FILL,										//	VkPolygonMode							polygonMode;
		vk::VK_CULL_MODE_NONE,											//	VkCullModeFlags							cullMode;
		vk::VK_FRONT_FACE_COUNTER_CLOCKWISE,							//	VkFrontFace								frontFace;
		VK_FALSE,														//	VkBool32								depthBiasEnable;
		0.0f,															//	float									depthBiasConstantFactor;
		0.0f,															//	float									depthBiasClamp;
		0.0f,															//	float									depthBiasSlopeFactor;
		1.0f,															//	float									lineWidth;
	};

	// Multisample state.
	const vk::VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineMultisampleStateCreateFlags	flags;
		vk::VK_SAMPLE_COUNT_1_BIT,										//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,														//	VkBool32								sampleShadingEnable;
		0.0f,															//	float									minSampleShading;
		nullptr,														//	const VkSampleMask*						pSampleMask;
		VK_FALSE,														//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,														//	VkBool32								alphaToOneEnable;
	};

	// Depth/stencil state.
	const vk::VkStencilOpState stencil =
	{
		vk::VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
		vk::VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
		vk::VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
		vk::VK_COMPARE_OP_ALWAYS,	// VkCompareOp	compareOp;
		0xFFu,						// deUint32		compareMask;
		0xFFu,						// deUint32		writeMask;
		0u,							// deUint32		reference;
	};

	const vk::VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,															//	VkBool32								depthTestEnable;
		VK_TRUE,															//	VkBool32								depthWriteEnable;
		vk::VK_COMPARE_OP_LESS,												//	VkCompareOp								depthCompareOp;
		VK_FALSE,															//	VkBool32								depthBoundsTestEnable;
		VK_FALSE,															//	VkBool32								stencilTestEnable;
		stencil,															//	VkStencilOpState						front;
		stencil,															//	VkStencilOpState						back;
		0.0f,																//	float									minDepthBounds;
		1.0f,																//	float									maxDepthBounds;
	};

	// Dynamic state. Here we will set all states which have a dynamic value.
	std::vector<vk::VkDynamicState> dynamicStates;

	if (m_testConfig.colorWriteEnableConfig.dynamicValue)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);

	const vk::VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(dynamicStates.size()),				//	deUint32							dynamicStateCount;
		dynamicStates.data(),										//	const VkDynamicState*				pDynamicStates;
	};

	std::vector<vk::VkPipelineColorBlendAttachmentState> colorBlendAttachmentState(
		kNumColorAttachments,
		vk::VkPipelineColorBlendAttachmentState
		{
			VK_FALSE,								// VkBool32                 blendEnable
			vk::VK_BLEND_FACTOR_ZERO,				// VkBlendFactor            srcColorBlendFactor
			vk::VK_BLEND_FACTOR_ZERO,				// VkBlendFactor            dstColorBlendFactor
			vk::VK_BLEND_OP_ADD,					// VkBlendOp                colorBlendOp
			vk::VK_BLEND_FACTOR_ZERO,				// VkBlendFactor            srcAlphaBlendFactor
			vk::VK_BLEND_FACTOR_ZERO,				// VkBlendFactor            dstAlphaBlendFactor
			vk::VK_BLEND_OP_ADD,					// VkBlendOp                alphaBlendOp
			static_cast<vk::VkColorComponentFlags>(	// VkColorComponentFlags    colorWriteMask
				(m_testConfig.channelMask.x() ? vk::VK_COLOR_COMPONENT_R_BIT : 0)
				| (m_testConfig.channelMask.y() ? vk::VK_COLOR_COMPONENT_G_BIT : 0)
				| (m_testConfig.channelMask.z() ? vk::VK_COLOR_COMPONENT_B_BIT : 0)
				| (m_testConfig.channelMask.w() ? vk::VK_COLOR_COMPONENT_A_BIT : 0)
			)
		}
	);

	const vk::VkPipelineColorWriteCreateInfoEXT colorWriteCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT,		// VkStructureType	sType;
		nullptr,														// const void*		pNext;
		kNumColorAttachments,											// deUint32			attachmentCount;
		m_testConfig.colorWriteEnableConfig.staticValue.data()			// const VkBool32*	pColorWriteEnables;
	};

	const vk::VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                               sType
		&colorWriteCreateInfo,											// const void*                                   pNext
		0u,																// VkPipelineColorBlendStateCreateFlags          flags
		VK_FALSE,														// VkBool32                                      logicOpEnable
		vk::VK_LOGIC_OP_CLEAR,											// VkLogicOp                                     logicOp
		kNumColorAttachments,											// deUint32                                      attachmentCount
		colorBlendAttachmentState.data(),								// const VkPipelineColorBlendAttachmentState*    pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }										// float                                         blendConstants[4]
	};

	vk::GraphicsPipelineWrapper	staticPipeline		(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_testConfig.pipelineConstructionType);
	const bool					bindStaticFirst		= (kSequenceOrdering == SequenceOrdering::BETWEEN_PIPELINES	||
													   kSequenceOrdering == SequenceOrdering::AFTER_PIPELINES	||
													   kSequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC);
	const bool					useStaticPipeline	= (bindStaticFirst || kReversed);

	// Create static pipeline when needed.
	if (useStaticPipeline)
	{
		staticPipeline.setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
					  .setupPreRasterizationShaderState(viewport,
								scissor,
								pipelineLayout,
								*framebuffers[0],
								0u,
								vertModule,
								&rasterizationStateCreateInfo)
					  .setupFragmentShaderState(pipelineLayout,
								*framebuffers[0],
								0u,
								fragModule,
								&depthStencilStateCreateInfo,
								&multisampleStateCreateInfo)
					  .setupFragmentOutputState(*framebuffers[0], 0u, &colorBlendStateCreateInfo, &multisampleStateCreateInfo)
					  .setMonolithicPipelineLayout(pipelineLayout)
					  .buildPipeline();
	}

	// Create dynamic pipeline.
	vk::GraphicsPipelineWrapper graphicsPipeline(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_testConfig.pipelineConstructionType);;
	graphicsPipeline.setDynamicState(&dynamicStateCreateInfo)
					.setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
					.setupPreRasterizationShaderState(viewport,
								scissor,
								pipelineLayout,
								*framebuffers[0],
								0u,
								vertModule,
								&rasterizationStateCreateInfo)
					.setupFragmentShaderState(pipelineLayout,
								*framebuffers[0],
								0u,
								fragModule,
								&depthStencilStateCreateInfo,
								&multisampleStateCreateInfo)
					.setupFragmentOutputState(*framebuffers[0], 0u, &colorBlendStateCreateInfo, &multisampleStateCreateInfo)
					.setMonolithicPipelineLayout(pipelineLayout)
					.buildPipeline();

	// Command buffer.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd , device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Clear values.
	std::vector<vk::VkClearValue> clearValues;
	auto colorClearValue = vk::makeClearValueColor(m_testConfig.clearColorValue);
	for (deUint32 i = 0u; i < kNumColorAttachments; ++i)
		clearValues.push_back(colorClearValue);
	clearValues.push_back(vk::makeClearValueDepthStencil(m_testConfig.clearDepthValue, 0u));

	// Record command buffer.
	vk::beginCommandBuffer(vkd, cmdBuffer);

	for (deUint32 iteration = 0u; iteration < kNumIterations; ++iteration)
	{
		// Maybe set dynamic state here.
		if (kSequenceOrdering == SequenceOrdering::CMD_BUFFER_START)
		{
			setDynamicStates(m_testConfig, vkd, cmdBuffer);
		}

		// Begin render pass.
		framebuffers[iteration].begin(vkd, cmdBuffer, vk::makeRect2D(kFramebufferWidth, kFramebufferHeight), static_cast<deUint32>(clearValues.size()), clearValues.data());

			// Bind a static pipeline first if needed.
			if (bindStaticFirst && iteration == 0u)
			{
				staticPipeline.bind(cmdBuffer);
			}

			// Maybe set dynamic state here.
			if (kSequenceOrdering == SequenceOrdering::BETWEEN_PIPELINES)
			{
				setDynamicStates(m_testConfig, vkd, cmdBuffer);
			}

			// Bind dynamic pipeline.
			if ((kSequenceOrdering != SequenceOrdering::TWO_DRAWS_DYNAMIC &&
				 kSequenceOrdering != SequenceOrdering::TWO_DRAWS_STATIC) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC && iteration > 0u) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration == 0u))
			{
				graphicsPipeline.bind(cmdBuffer);
			}

			if (kSequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC && iteration > 0u) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration == 0u))
			{
				setDynamicStates(m_testConfig, vkd, cmdBuffer);
			}

			// Bind a static pipeline last if needed.
			if (kSequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration > 0u))
			{
				staticPipeline.bind(cmdBuffer);
			}

			// Push constants.
			PushConstants pushConstants =
			{
				m_testConfig.meshParams.color,		//	tcu::Vec4	triangleColor;
				m_testConfig.meshParams.depth,		//	float		meshDepth;
				m_testConfig.meshParams.scaleX,		//	float		scaleX;
				m_testConfig.meshParams.scaleY,		//	float		scaleY;
				m_testConfig.meshParams.offsetX,	//	float		offsetX;
				m_testConfig.meshParams.offsetY,	//	float		offsetY;
			};
			vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pushConstantStageFlags, 0u, static_cast<deUint32>(sizeof(pushConstants)), &pushConstants);

			// Maybe set dynamic state here.
			if (kSequenceOrdering == SequenceOrdering::BEFORE_DRAW || kSequenceOrdering == SequenceOrdering::AFTER_PIPELINES)
			{
				setDynamicStates(m_testConfig, vkd, cmdBuffer);
			}

			// Bind vertex buffer and draw.
			vk::VkDeviceSize offset = 0ull;
			vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertBuffer.get(), &offset);
			vkd.cmdDraw(cmdBuffer, 6u, 1u, 0u, 0u);

			framebuffers[iteration].end(vkd, cmdBuffer);
	}

	vk::endCommandBuffer(vkd, cmdBuffer);

	// Submit commands.
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read result image aspects from the last used framebuffer.
	const tcu::UVec2	renderSize(kFramebufferWidth, kFramebufferHeight);

	const int kWidth	= static_cast<int>(kFramebufferWidth);
	const int kHeight	= static_cast<int>(kFramebufferHeight);

	const tcu::Vec4		kGood(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4		kBad(1.0f, 0.0f, 0.0f, 1.0f);

	const tcu::TextureFormat errorFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);

	bool colorMatchAll = true;

	// Check expected values.
	auto nextAttachmentImage = colorImages.end() - kNumColorAttachments;
	for (deUint32 attachmentIndex = 0u; attachmentIndex < kNumColorAttachments; ++attachmentIndex, ++nextAttachmentImage)
	{
		const auto			colorBuffer = readColorAttachment(vkd, device, queue, queueIndex, allocator, (*nextAttachmentImage)->get(), kColorFormat, renderSize);
		const auto			colorAccess = colorBuffer->getAccess();

		tcu::TextureLevel	colorError			(errorFormat, kWidth, kHeight);
		const auto			colorErrorAccess	= colorError.getAccess();

		bool colorMatch = true;

		for (int y = 0; y < kHeight; ++y)
		for (int x = 0; x < kWidth; ++x)
		{
			const auto colorPixel = colorAccess.getPixel(x, y);

			bool match = tcu::boolAll(tcu::lessThan(tcu::absDiff(colorPixel, m_testConfig.expectedColor[attachmentIndex]), kColorThreshold));
			colorErrorAccess.setPixel((match ? kGood : kBad), x, y);
			if (!match)
				colorMatch = false;
		}

		if (!colorMatch)
		{
			std::ostringstream desc;
			desc << "Result color image and error mask for attachment #" << attachmentIndex;
			logErrors(log, "Color", desc.str(), colorAccess, colorErrorAccess);
			colorMatchAll = false;
		}
	}

	const auto			depthBuffer = readDepthAttachment(vkd, device, queue, queueIndex, allocator, dsImages.back()->get(), dsFormat, renderSize);
	const auto			depthAccess = depthBuffer->getAccess();
	tcu::TextureLevel	depthError(errorFormat, kWidth, kHeight);
	const auto			depthErrorAccess = depthError.getAccess();

	const auto	minDepth	= m_testConfig.expectedDepth - 1.0e-07f;
	const auto	maxDepth	= m_testConfig.expectedDepth + 1.0e-07f;
	bool		depthMatch	= true;

	for (int y = 0; y < kHeight; ++y)
	for (int x = 0; x < kWidth; ++x)
	{
		const auto depthPixel = depthAccess.getPixDepth(x, y);
		bool match = de::inRange(depthPixel, minDepth, maxDepth);
		depthErrorAccess.setPixel((match ? kGood : kBad), x, y);
		if (!match)
			depthMatch = false;
	}

	if (!depthMatch)
	{
		logErrors(log, "Depth", "Result depth image and error mask", depthAccess, depthErrorAccess);
	}

	if (!(colorMatchAll && depthMatch))
	{
		return tcu::TestStatus::fail("Incorrect value found in attachments; please check logged images");
	}

	return tcu::TestStatus::pass("Pass");
}

template <typename VectorType>
VectorType MaskVector(const VectorType& valueIfMaskIsFalse, const VectorType& valueIfMaskIsTrue, const std::vector<bool>& mask, bool inverse = false)
{
	DE_ASSERT(valueIfMaskIsFalse.size() == valueIfMaskIsTrue.size() && valueIfMaskIsFalse.size() == mask.size());

	VectorType ret(mask.size());

	for (size_t i = 0; i < mask.size(); ++i)
	{
		bool m = mask[i];
		if (inverse)
			m = !m;
		ret[i] = m ? valueIfMaskIsTrue[i] : valueIfMaskIsFalse[i];
	}

	return ret;
}

void ApplyChannelMask(std::vector<tcu::Vec4>& meshColors, const tcu::BVec4& channelMask, const tcu::Vec4& clearColor)
{
	for (auto&& attachmentColor : meshColors)
		attachmentColor = tcu::Vec4(
			channelMask.x() ? attachmentColor.x() : clearColor.x(),
			channelMask.y() ? attachmentColor.y() : clearColor.y(),
			channelMask.z() ? attachmentColor.z() : clearColor.z(),
			channelMask.w() ? attachmentColor.w() : clearColor.w()
		);
}

void AddSingleTestCaseStatic(const std::string&				name,
							 vk::PipelineConstructionType	pipelineConstructionType,
							 const std::vector<bool>		mask,
							 const tcu::BVec4				channelMask,
							 bool							inverse,
							 tcu::TestCaseGroup*			orderingGroup,
							 tcu::TestContext&				testCtx)
{
	TestConfig config(pipelineConstructionType, SequenceOrdering::CMD_BUFFER_START);

	// Enable writes and expect the mesh color, or disable writes and expect the clear color.

	config.clearColorValue						= tcu::Vec4(0.25f, 0.5f, 0.75f, 0.5f);
	config.meshParams.color						= tcu::Vec4(1.0f, 0.75f, 0.5f, 0.25f);

	const auto allVkFalse						= Bool32Vec(kNumColorAttachments, VK_FALSE);
	const auto allVkTrue						= Bool32Vec(kNumColorAttachments, VK_TRUE);

	config.channelMask = channelMask;

	config.colorWriteEnableConfig.staticValue	= MaskVector(allVkFalse, allVkTrue, mask, inverse);

	// Note colorWriteEnableConfig.dynamicValue is unset, defaults to an empty Maybe<T>

	std::vector<tcu::Vec4> meshColorsPerAttachment(kNumColorAttachments);
	meshColorsPerAttachment[0] = config.meshParams.color;
	for (deUint32 i = 1u; i < kNumColorAttachments; ++i)
		meshColorsPerAttachment[i] = meshColorsPerAttachment[i - 1] * 0.5f;

	std::vector<tcu::Vec4> clearColorsPerAttachment(kNumColorAttachments, config.clearColorValue);

	ApplyChannelMask(meshColorsPerAttachment, channelMask, config.clearColorValue);

	config.expectedColor = MaskVector(clearColorsPerAttachment, meshColorsPerAttachment, mask, inverse);

	// Depth should always be written even when color is not
	config.clearDepthValue	= 0.5f;
	config.meshParams.depth	= 0.25f;
	config.expectedDepth	= 0.25f;

	orderingGroup->addChild(new ColorWriteEnableTest(testCtx, name, config));
}

void AddSingleTestCaseDynamic(const std::string&			name,
							  vk::PipelineConstructionType	pipelineConstructionType,
							  const std::vector<bool>		mask,
							  const tcu::BVec4				channelMask,
							  bool							inverse,
							  tcu::TestCaseGroup*			orderingGroup,
							  tcu::TestContext&				testCtx,
							  SequenceOrdering				ordering)
{
	TestConfig config(pipelineConstructionType, ordering);

	// Enable writes and expect the mesh color, or disable writes and expect the clear color.

	config.clearColorValue						= tcu::Vec4(0.25f, 0.5f, 0.75f, 0.5f);
	config.meshParams.color						= tcu::Vec4(1.0f, 0.75f, 0.5f, 0.25f);

	const auto allVkFalse						= Bool32Vec(kNumColorAttachments, VK_FALSE);
	const auto allVkTrue						= Bool32Vec(kNumColorAttachments, VK_TRUE);

	config.channelMask = channelMask;

	config.colorWriteEnableConfig.staticValue	= inverse ? allVkTrue : allVkFalse;
	config.colorWriteEnableConfig.dynamicValue	= MaskVector(allVkFalse, allVkTrue, mask, inverse);

	std::vector<tcu::Vec4> meshColorsPerAttachment(kNumColorAttachments);
	meshColorsPerAttachment[0] = config.meshParams.color;
	for (deUint32 i = 1u; i < kNumColorAttachments; ++i)
		meshColorsPerAttachment[i] = meshColorsPerAttachment[i - 1] * 0.5f;

	std::vector<tcu::Vec4> clearColorsPerAttachment(kNumColorAttachments, config.clearColorValue);

	ApplyChannelMask(meshColorsPerAttachment, channelMask, config.clearColorValue);

	config.expectedColor = MaskVector(clearColorsPerAttachment, meshColorsPerAttachment, mask, inverse);

	// Depth should always be written even when color is not
	config.clearDepthValue	= 0.5f;
	config.meshParams.depth	= 0.25f;
	config.expectedDepth	= 0.25f;

	orderingGroup->addChild(new ColorWriteEnableTest(testCtx, name, config));
}

} // anonymous namespace

namespace
{
using namespace vk;
using namespace tcu;

struct TestParams
{
	deUint32	width;
	deUint32	height;
	VkFormat	format;
	deUint32	attachmentCount;
	deUint32	attachmentMore;
	bool		setCweBeforePlBind;
	bool		colorWriteEnables;
	PipelineConstructionType pct;
	bool selectOptimalBlendableFormat (const InstanceInterface&, VkPhysicalDevice);
};

class ColorWriteEnable2Test : public vkt::TestCase
{
public:
								ColorWriteEnable2Test	(TestContext&		testCtx,
														 const std::string&	name,
														 const TestParams&	testParams)
									: vkt::TestCase		(testCtx, name)
									, m_params		(testParams) { }

	virtual						~ColorWriteEnable2Test	() = default;

	virtual void				checkSupport			(Context&			context) const override;
	virtual void				initPrograms			(SourceCollections&	programCollection) const override;
	virtual vkt::TestInstance*	createInstance			(Context&			context) const override;

private:
	mutable TestParams			m_params;

};

class ColorWriteEnable2Instance : public vkt::TestInstance
{
public:
	typedef std::vector<VkBool32>	ColorWriteEnables;
	struct Attachment
	{
		de::MovePtr<ImageWithMemory>	image;
		Move<VkImageView>				view;
		Attachment () = default;
		DE_UNUSED_FUNCTION Attachment (Attachment&& other);
	};
	struct Framebuffer
	{
		std::vector<Attachment>	attachments;
		RenderPassWrapper		framebuffer;
		Framebuffer	() = default;
		Framebuffer	(Framebuffer&& other);
	};
	struct GraphicsPipelineWrapperEx : public GraphicsPipelineWrapper
	{
		GraphicsPipelineWrapperEx			(const InstanceInterface&			vki,
											 const DeviceInterface&				vkd,
											 const VkPhysicalDevice				physDev,
											 const VkDevice						dev,
											 const std::vector<std::string>&	exts,
											 const PipelineConstructionType		pct)
			: GraphicsPipelineWrapper		(vki, vkd, physDev, dev, exts, pct)
			, m_isDynamicColorWriteEnable	(false) {}
		bool isDynamicColorWriteEnable		() const { return m_isDynamicColorWriteEnable; }
	private:
		friend class ColorWriteEnable2Instance;
		bool m_isDynamicColorWriteEnable;
	};
									ColorWriteEnable2Instance	(Context&						context,
																 const TestParams&				testParams);
	virtual							~ColorWriteEnable2Instance	() = default;

	de::MovePtr<BufferWithMemory>	createVerrtexBuffer			() const;
	RenderPassWrapper				createRenderPass			(deUint32						colorAttachmentCount) const;
	Framebuffer						createFramebuffer			(deUint32						colorAttachmentCount) const;
	void							setupAndBuildPipeline		(GraphicsPipelineWrapperEx&		owner,
																 PipelineLayoutWrapper&			pipelineLayout,
																 VkRenderPass					renderPass,
																 deUint32						colorAttachmentCount,
																 const ColorWriteEnables&		colorWriteEnables,
																 float							blendComp,
																 bool							dynamic) const;
	virtual TestStatus				iterate						() override;
	tcu::TestStatus					verifyAttachment			(const deUint32					attachmentIndex,
																 const deUint32					attachmentCount,
																 const ConstPixelBufferAccess&	attachmentContent,
																 const ColorWriteEnables&		colorWriteEnables,
																 const Vec4&					background,
																 const float					blendComp) const;
private:
	const TestParams			m_params;
	const DeviceInterface&		m_vkd;
	const VkDevice				m_device;
	Allocator&					m_allocator;
	const ShaderWrapper			m_vertex;
	const ShaderWrapper			m_fragment;
};

ColorWriteEnable2Instance::Attachment::Attachment (Attachment&& other)
	: image			(std::move(other.image))
	, view			(std::move(other.view))
{
}
ColorWriteEnable2Instance::Framebuffer::Framebuffer (Framebuffer&& other)
	: attachments	(std::move(other.attachments))
	, framebuffer	(std::move(other.framebuffer))
{
}

bool TestParams::selectOptimalBlendableFormat (const InstanceInterface& vk, VkPhysicalDevice dev)
{
	auto doesFormatMatch = [](const VkFormat fmt) -> bool
	{
		const auto tcuFmt = mapVkFormat(fmt);
		return tcuFmt.order == TextureFormat::ChannelOrder::RGBA
				|| tcuFmt.order == TextureFormat::ChannelOrder::sRGBA;
	};

	VkFormatProperties2 props{};
	const VkFormatFeatureFlags flags = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
										| VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
										| VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
	for (int f = VK_FORMAT_R64G64B64A64_SFLOAT; f > 0; --f)
	{
		props.sType				= VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
		props.pNext				= nullptr;
		props.formatProperties	= {};
		const VkFormat			fmt = static_cast<VkFormat>(f);
		vk.getPhysicalDeviceFormatProperties2(dev, fmt, &props);
		if (doesFormatMatch(fmt) && ((props.formatProperties.optimalTilingFeatures & flags) == flags))
		{
			this->format = fmt;
			return true;
		}
	}
	return false;
}

void ColorWriteEnable2Test::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	if (m_params.colorWriteEnables)
	{
		context.requireDeviceFunctionality("VK_EXT_color_write_enable");
	}

	DE_ASSERT(m_params.attachmentCount >= 1);
	auto maxColorAttachments = context.getDeviceProperties().limits.maxColorAttachments;
	if ((m_params.attachmentCount + m_params.attachmentMore) > maxColorAttachments)
	{
		std::stringstream ss;
		if (m_params.attachmentMore)
		{
			ss << "Sum of color attachments (" << m_params.attachmentCount << " + " << m_params.attachmentMore << ")";
		}
		else
		{
			ss << "Color attachment count of " << m_params.attachmentCount;
		}
		ss << " exceeds maximum number of color attachments supported by device which is " << maxColorAttachments;
		ss.flush();
		TCU_THROW(NotSupportedError, ss.str());
	}

	if ( ! m_params.selectOptimalBlendableFormat(vki, physicalDevice))
		TCU_THROW(NotSupportedError, "Required color image features not supported");

	checkPipelineConstructionRequirements(vki, physicalDevice, m_params.pct);
}

void ColorWriteEnable2Test::initPrograms (SourceCollections& programCollection) const
{
	const char			nl = '\n';
	const deUint32		ac = m_params.attachmentCount;
	std::ostringstream	vs;
	std::ostringstream	fs;

	vs	<< "#version 450"									<<	nl
		<< "layout(location = 0) in vec4 position;"			<<	nl
		<< "layout(location = 0) out flat int instance;"	<<	nl
		<< "void main() {"									<<	nl
		<< "    gl_Position = vec4(position.xy, 0.0, 1.0);"	<<	nl
		<< "	instance = gl_InstanceIndex;"				<<	nl
		<< "}"												<<	nl;
	programCollection.glslSources.add("vert") << glu::VertexSource(vs.str());

	fs	<< "#version 450"											<<	nl
		<< "layout(location = 0) in flat int attachments;"			<<	nl
		<< "layout(location = 0) out vec4 colors[" << ac << "];"	<<	nl
		<< "void main() {"											<<	nl
		<< "    for (int a = 0; a < attachments; ++a) {"			<<	nl
		<< "		float c = float(attachments - a);"				<<	nl
		<< "		colors[a] = vec4(pow(0.5, c));"					<<	nl
		<< "}}"														<<	nl;
	programCollection.glslSources.add("frag") << glu::FragmentSource(fs.str());
}

TestInstance* ColorWriteEnable2Test::createInstance (Context& context) const
{
	return new ColorWriteEnable2Instance(context, m_params);
}

ColorWriteEnable2Instance::ColorWriteEnable2Instance (Context& context, const TestParams& testParams)
	: vkt::TestInstance	(context)
	, m_params		(testParams)
	, m_vkd				(context.getDeviceInterface())
	, m_device			(context.getDevice())
	, m_allocator		(context.getDefaultAllocator())
	, m_vertex			(ShaderWrapper(m_vkd, m_device, context.getBinaryCollection().get("vert")))
	, m_fragment		(ShaderWrapper(m_vkd, m_device, context.getBinaryCollection().get("frag")))
{
}

RenderPassWrapper ColorWriteEnable2Instance::createRenderPass (deUint32 colorAttachmentCount) const
{
	const std::vector<VkAttachmentDescription> attachmentDescriptions(
		colorAttachmentCount,
		VkAttachmentDescription
		{
			0u,											//	VkAttachmentDescriptionFlags	flags;
			m_params.format,							//	VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,						//	VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,				//	VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,				//	VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
		}
	);

	std::vector<VkAttachmentReference> colorAttachmentReference;
	for (deUint32 i = 0u; i < colorAttachmentCount; ++i)
	{
		colorAttachmentReference.push_back(VkAttachmentReference
			{
				i,											//	deUint32		attachment;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	//	VkImageLayout	layout;
			}
		);
	}

	const VkSubpassDescription subpassDescription
	{
		0u,												//	VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,				//	VkPipelineBindPoint				pipelineBindPoint;
		0u,												//	deUint32						inputAttachmentCount;
		nullptr,										//	const VkAttachmentReference*	pInputAttachments;
		colorAttachmentCount,							//	deUint32						colorAttachmentCount;
		colorAttachmentReference.data(),				//	const VkAttachmentReference*	pColorAttachments;
		nullptr,										//	const VkAttachmentReference*	pResolveAttachments;
		nullptr,										//	const VkAttachmentReference*	pDepthStencilAttachment;
		0u,												//	deUint32						preserveAttachmentCount;
		nullptr,										//	const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassCreateInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkRenderPassCreateFlags			flags;
		colorAttachmentCount,							//	deUint32						attachmentCount;
		attachmentDescriptions.data(),					//	const VkAttachmentDescription*	pAttachments;
		1u,												//	deUint32						subpassCount;
		&subpassDescription,							//	const VkSubpassDescription*		pSubpasses;
		0u,												//	deUint32						dependencyCount;
		nullptr,										//	const VkSubpassDependency*		pDependencies;
	};

	return RenderPassWrapper(m_params.pct, m_vkd, m_device, &renderPassCreateInfo);
}

de::MovePtr<BufferWithMemory> ColorWriteEnable2Instance::createVerrtexBuffer () const
{
	const std::vector<float> quad
	{
		-1.0f, -1.0f, 0.0f, 0.0f,
		+1.0f, -1.0f, 0.0f, 0.0f,
		-1.0f, +1.0f, 0.0f, 0.0f,
		-1.0f, +1.0f, 0.0f, 0.0f,
		+1.0f, -1.0f, 0.0f, 0.0f,
		+1.0f, +1.0f, 0.0f, 0.0f
	};

	const auto						vertDataSize	= quad.size() * sizeof(float);
	const auto						vertBufferInfo	= makeBufferCreateInfo(static_cast<VkDeviceSize>(vertDataSize), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>	vertBuffer		(new BufferWithMemory(m_vkd, m_device, m_allocator, vertBufferInfo, vk::MemoryRequirement::HostVisible));
	auto&							alloc			= vertBuffer->getAllocation();

	deMemcpy(reinterpret_cast<char*>(alloc.getHostPtr()), quad.data(), vertDataSize);
	flushAlloc(m_vkd, m_device, alloc);

	return vertBuffer;
}

ColorWriteEnable2Instance::Framebuffer ColorWriteEnable2Instance::createFramebuffer (deUint32 colorAttachmentCount) const
{
	const VkExtent3D			extent				{ m_params.width, m_params.height, 1u };
	const VkImageUsageFlags		imageUsage			= (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto					imageSubresource	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const deUint32				queueIndex			= m_context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= m_context.getDefaultAllocator();

	std::vector<Attachment>		attachments			(colorAttachmentCount);
	std::vector<VkImage>		images				(colorAttachmentCount);
	std::vector<VkImageView>	views				(colorAttachmentCount);

	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		auto& attachment = attachments[i];

		const VkImageCreateInfo imageCreateInfo
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		//	VkStructureType			sType;
			nullptr,									//	const void*				pNext;
			0u,											//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,							//	VkImageType				imageType;
			m_params.format,							//	VkFormat				format;
			extent,										//	VkExtent3D				extent;
			1u,											//	deUint32				mipLevels;
			1u,											//	deUint32				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,						//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,					//	VkImageTiling			tiling;
			imageUsage,									//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,					//	VkSharingMode			sharingMode;
			1u,											//	deUint32				queueFamilyIndexCount;
			&queueIndex,								//	const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout			initialLayout;
		};
		attachment.image = de::MovePtr<ImageWithMemory>(new ImageWithMemory(m_vkd, m_device, allocator, imageCreateInfo, MemoryRequirement::Any));

		attachment.view	= makeImageView(m_vkd, m_device, **attachment.image, VK_IMAGE_VIEW_TYPE_2D, m_params.format, imageSubresource);

		images[i] = **attachment.image;
		views[i] = *attachment.view;
	}

	Framebuffer result;
	result.attachments = std::move(attachments);
	result.framebuffer = createRenderPass(colorAttachmentCount);

	const VkFramebufferCreateInfo framebufferCreateInfo
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		//	VkStructureType				sType;
		nullptr,										//	const void*					pNext;
		0u,												//	VkFramebufferCreateFlags	flags;
		result.framebuffer.get(),						//	VkRenderPass				renderPass;
		colorAttachmentCount,							//	deUint32					attachmentCount;
		views.data(),									//	const VkImageView*			pAttachments;
		m_params.width,									//	deUint32					width;
		m_params.height,								//	deUint32					height;
		1u,												//	deUint32					layers;
	};

	result.framebuffer.createFramebuffer(m_vkd, m_device, &framebufferCreateInfo, images);

	return result;
}

void ColorWriteEnable2Instance::setupAndBuildPipeline (GraphicsPipelineWrapperEx&	owner,
													   PipelineLayoutWrapper&		pipelineLayout,
													   VkRenderPass					renderPass,
													   deUint32						colorAttachmentCount,
													   const ColorWriteEnables&		colorWriteEnables,
													   float						blendComp,
													   bool							dynamic) const
{
	const std::vector<VkViewport>	viewports		{ makeViewport(m_params.width, m_params.height) };
	const std::vector<VkRect2D>		scissors		{ makeRect2D(m_params.width, m_params.height) };

	const auto						vertexBinding	= makeVertexInputBindingDescription(0u, static_cast<deUint32>(4 * sizeof(float)), VK_VERTEX_INPUT_RATE_VERTEX);
	const auto						vertexAttrib	= makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		1u,																//	deUint32									vertexBindingDescriptionCount;
		&vertexBinding,													//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		1u,																//	deUint32									vertexAttributeDescriptionCount;
		&vertexAttrib													//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							//	VkPrimitiveTopology						topology;
		VK_FALSE,														//	VkBool32								primitiveRestartEnable;
	};

	const VkDynamicState cweDynamicStates[1] { VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT };
	const VkPipelineDynamicStateCreateInfo	dynamicStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineDynamicStateCreateFlags	flags;
		1u,														//	deUint32							dynamicStateCount;
		cweDynamicStates										//	const VkDynamicState*				pDynamicStates;
	};

	DE_ASSERT(colorAttachmentCount <= colorWriteEnables.size());

	std::vector<vk::VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(
		colorAttachmentCount,
		VkPipelineColorBlendAttachmentState
		{
			VK_TRUE,											// VkBool32                 blendEnable
			VK_BLEND_FACTOR_CONSTANT_COLOR,						// VkBlendFactor            srcColorBlendFactor
			VK_BLEND_FACTOR_ZERO,								// VkBlendFactor            dstColorBlendFactor
			VK_BLEND_OP_ADD,									// VkBlendOp                colorBlendOp
			VK_BLEND_FACTOR_CONSTANT_ALPHA,						// VkBlendFactor            srcAlphaBlendFactor
			VK_BLEND_FACTOR_ZERO,								// VkBlendFactor            dstAlphaBlendFactor
			VK_BLEND_OP_ADD,									// VkBlendOp                alphaBlendOp
			VkColorComponentFlags(0)							// VkColorComponentFlags    colorWriteMask
		}
	);
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		VkColorComponentFlags colorWriteMask(  VK_COLOR_COMPONENT_R_BIT
											 | VK_COLOR_COMPONENT_G_BIT
											 | VK_COLOR_COMPONENT_B_BIT
											 | VK_COLOR_COMPONENT_A_BIT);
		switch (i % 4)
		{
			case 0: colorWriteMask &= (~(VK_COLOR_COMPONENT_R_BIT)); break;
			case 1: colorWriteMask &= (~(VK_COLOR_COMPONENT_G_BIT)); break;
			case 2: colorWriteMask &= (~(VK_COLOR_COMPONENT_B_BIT)); break;
			case 3: colorWriteMask &= (~(VK_COLOR_COMPONENT_A_BIT)); break;
		}
		colorBlendAttachmentStates[i].colorWriteMask = colorWriteMask;
		colorBlendAttachmentStates[i].blendEnable = colorWriteEnables[i];
	}

	const VkPipelineColorWriteCreateInfoEXT colorWriteCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT,			// VkStructureType	sType;
		nullptr,														// const void*		pNext;
		colorAttachmentCount,											// deUint32			attachmentCount;
		colorWriteEnables.data()										// const VkBool32*	pColorWriteEnables;
	};

	const bool cweAllowed = (dynamic && m_params.colorWriteEnables);
	owner.m_isDynamicColorWriteEnable = cweAllowed;

	const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType                               sType
		cweAllowed ? nullptr : &colorWriteCreateInfo,					// const void*                                   pNext
		0u,																// VkPipelineColorBlendStateCreateFlags          flags
		VK_FALSE,														// VkBool32                                      logicOpEnable
		VK_LOGIC_OP_CLEAR,												// VkLogicOp                                     logicOp
		colorAttachmentCount,											// deUint32                                      attachmentCount
		colorBlendAttachmentStates.data(),								// const VkPipelineColorBlendAttachmentState*    pAttachments
		{ blendComp, blendComp, blendComp, blendComp }					// float                                         blendConstants[4]
	};

	owner
		.setDefaultRasterizationState()
		.setDefaultDepthStencilState()
		.setDefaultMultisampleState()
		.setDynamicState(cweAllowed ? &dynamicStateCreateInfo : nullptr)
		.setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
		.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass, 0u, m_vertex)
		.setupFragmentShaderState(pipelineLayout, renderPass, 0u, m_fragment)
		.setupFragmentOutputState(renderPass, 0u, &colorBlendStateCreateInfo)
		.setMonolithicPipelineLayout(pipelineLayout)
		.buildPipeline();
}

tcu::TestStatus ColorWriteEnable2Instance::verifyAttachment (const deUint32				attachmentIndex,
												  const deUint32				attachmentCount,
												  const ConstPixelBufferAccess&	attachmentContent,
												  const ColorWriteEnables&		colorWriteEnables,
												  const Vec4&					background,
												  const float					blendComp) const
{
	const auto	maskColor	= [&](Vec4 color) -> Vec4 {
		color[attachmentIndex % 4] = background[attachmentIndex % 4];
		return color;
	};
	const Vec4	source		(powf(0.5f, static_cast<float>(attachmentCount - attachmentIndex)));
	const Vec4	expected	= colorWriteEnables[attachmentIndex] ? maskColor(source * blendComp) : background;

	for (deUint32 y = 0; y < m_params.height; ++y)
	{
		for (deUint32 x = 0; x < m_params.width; ++x)
		{
			const auto result = attachmentContent.getPixel(x, y);
			if (!tcu::boolAll(tcu::lessThan(tcu::absDiff(result, expected), kColorThreshold))) {
				std::ostringstream msg;
				msg << "Unexpected output value found at position (" << x << ", " << y << "): expected\n" <<
					expected <<" but got\n" << result << ")";
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}

	return tcu::TestStatus::pass("");
}

TestStatus ColorWriteEnable2Instance::iterate (void)
{
	const InstanceInterface&				vki					= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice		= m_context.getPhysicalDevice();
	const VkQueue							queue				= m_context.getUniversalQueue();
	const deUint32							queueIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkRect2D							renderArea			= makeRect2D(m_params.width, m_params.height);
	const deUint32							attachmentCount		= m_params.attachmentCount;

	const float								blendComp			= 0.5f;
	const Vec4								background			(0.75f, 0.75f, 0.75f, 0.75f);
	std::vector<VkClearValue>				clearValues			(attachmentCount, makeClearValueColor(background));
	de::MovePtr<BufferWithMemory>			vertexBuffer		= createVerrtexBuffer();
	ColorWriteEnables						writeEnables		(attachmentCount + m_params.attachmentMore, VK_TRUE);
	for (deUint32 i = 0; i < attachmentCount; ++i)	writeEnables[i] = (i % 2) ? VK_TRUE : VK_FALSE;

	PipelineLayoutWrapper					pipelineLayout		(m_params.pct, m_vkd, m_device, 0u, nullptr, 0u, nullptr);
	std::vector<Framebuffer>				framebuffers;
	std::vector<GraphicsPipelineWrapperEx>	pipelines;
	for (deUint32 i = 0; i < attachmentCount; ++i)
	{
		framebuffers.emplace_back(createFramebuffer(i+1));

		const bool dynamicColorWriteEnable = (((attachmentCount - i) % 2) == 1);

		// build dynamics and statics pipelines alternately in reverse order
		pipelines.emplace_back(vki, m_vkd, physicalDevice, m_device, m_context.getDeviceExtensions(), m_params.pct);
		setupAndBuildPipeline(pipelines.back(), pipelineLayout, framebuffers[i].framebuffer.get(), (i+1), writeEnables, blendComp, dynamicColorWriteEnable);
	}

	Move<VkCommandPool>						cmdPool				= makeCommandPool(m_vkd, m_device, queueIndex);
	Move<VkCommandBuffer>					cmdBuff				= allocateCommandBuffer(m_vkd, m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(m_vkd, *cmdBuff);
		m_vkd.cmdBindVertexBuffers(*cmdBuff, 0u, 1u, &vertexBuffer->get(), &static_cast<const VkDeviceSize&>(0));

		for (deUint32 a = 0; a < attachmentCount; ++a)
		{
			if (m_params.setCweBeforePlBind)
			{
				if (pipelines[a].isDynamicColorWriteEnable())
					m_vkd.cmdSetColorWriteEnableEXT(*cmdBuff, static_cast<deUint32>(writeEnables.size()), writeEnables.data());
				pipelines[a].bind(*cmdBuff);
			}
			else
			{
				pipelines[a].bind(*cmdBuff);
				if (pipelines[a].isDynamicColorWriteEnable())
					m_vkd.cmdSetColorWriteEnableEXT(*cmdBuff, static_cast<deUint32>(writeEnables.size()), writeEnables.data());
			}

			framebuffers[a].framebuffer.begin(m_vkd, *cmdBuff, renderArea, attachmentCount, clearValues.data());
				m_vkd.cmdDraw(*cmdBuff, 6u, 1u, 0u, (a + 1));
			framebuffers[a].framebuffer.end(m_vkd, *cmdBuff);
		}

	endCommandBuffer(m_vkd, *cmdBuff);
	submitCommandsAndWait(m_vkd, m_device, queue, *cmdBuff);

	for (deUint32 i = 0; i < attachmentCount; ++i)
	for (deUint32 a = 0; a < (i+1); ++a)
	{
		const auto	colorBuffer = readColorAttachment(m_vkd, m_device, queue, queueIndex, m_allocator,
													  **framebuffers.at(i).attachments.at(a).image, m_params.format,
													  UVec2(m_params.width, m_params.height));
		tcu::TestStatus status = verifyAttachment(a, (i+1), colorBuffer->getAccess(), writeEnables, background, blendComp);
		if (status.isFail()) return status;
	}

	return TestStatus::pass("");
}

} // unnamed namespace

tcu::TestCaseGroup* createColorWriteEnableTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pct)
{
	de::MovePtr<tcu::TestCaseGroup> colorWriteEnableGroup(new tcu::TestCaseGroup(testCtx, "color_write_enable"));

	DE_ASSERT(kNumColorAttachments >= 2);

	std::vector<bool> mask_all				(kNumColorAttachments, true);
	std::vector<bool> mask_first			(kNumColorAttachments, false);	mask_first[0]												= true;
	std::vector<bool> mask_second			(kNumColorAttachments, false);	mask_second[1]												= true;
	std::vector<bool> mask_last				(kNumColorAttachments, false);	mask_last.back()											= true;
	std::vector<bool> mask_first_and_second	(kNumColorAttachments, false);	mask_first_and_second[0]	= mask_first_and_second[1]		= true;
	std::vector<bool> mask_second_and_last	(kNumColorAttachments, false);	mask_second_and_last[1]		= mask_second_and_last.back()	= true;

	// Test cases for channel enables
	static const struct
	{
		tcu::BVec4		enabledChannels;
		std::string		name;
		std::string		desc;
	} kChannelCases[] =
	{
		{ tcu::BVec4(true, true, true, true), "all_channels", "Enable all channels in colorWriteMask"},
		{ tcu::BVec4(true, false, false, false), "red_channel", "Red channel enabled in colorWriteMask"},
		{ tcu::BVec4(false, true, false, false), "green_channel", "Green channel enabled in colorWriteMask"},
		{ tcu::BVec4(false, false, true, false), "blue_channel", "Blue channel enabled in colorWriteMask"},
		{ tcu::BVec4(false, false, false, true), "alpha_channel", "Alpha channel enabled in colorWriteMask"},
		{ tcu::BVec4(false, false, false, false), "no_channels", "Disable all channels in colorWriteMask"},
	};

	// Test cases for the dynamic state
	static const struct
	{
		SequenceOrdering	ordering;
		std::string			name;
	} kOrderingCases[] =
	{
		// Dynamic state set after command buffer start
		{ SequenceOrdering::CMD_BUFFER_START,	"cmd_buffer_start"},
		// Dynamic state set just before drawing
		{ SequenceOrdering::BEFORE_DRAW,		"before_draw"},
		// Dynamic after a pipeline with static states has been bound and before a pipeline with dynamic states has been bound
		{ SequenceOrdering::BETWEEN_PIPELINES,	"between_pipelines"},
		// Dynamic state set after both a static-state pipeline and a second dynamic-state pipeline have been bound
		{ SequenceOrdering::AFTER_PIPELINES,	"after_pipelines"},
		// Dynamic state set after a dynamic pipeline has been bound and before a second static-state pipeline with the right values has been bound
		{ SequenceOrdering::BEFORE_GOOD_STATIC,	"before_good_static"},
		// Bind bad static pipeline and draw, followed by binding correct dynamic pipeline and drawing again
		{ SequenceOrdering::TWO_DRAWS_DYNAMIC,	"two_draws_dynamic"},
		// Bind bad dynamic pipeline and draw, followed by binding correct static pipeline and drawing again
		{ SequenceOrdering::TWO_DRAWS_STATIC,	"two_draws_static"},
	};

	for (int channelCaseIdx = 0; channelCaseIdx < DE_LENGTH_OF_ARRAY(kChannelCases); ++channelCaseIdx)
	{
		const auto& kChannelCase	 = kChannelCases[channelCaseIdx];
		de::MovePtr<tcu::TestCaseGroup> channelGroup(new tcu::TestCaseGroup(testCtx, kChannelCase.name.c_str()));

		for (int orderingIdx = 0; orderingIdx < DE_LENGTH_OF_ARRAY(kOrderingCases); ++orderingIdx)
		{
			const auto& kOrderingCase	= kOrderingCases[orderingIdx];
			const auto& kOrdering		= kOrderingCase.ordering;

			if (vk::isConstructionTypeShaderObject(pct) && (kOrderingCase.ordering == SequenceOrdering::BETWEEN_PIPELINES || kOrderingCase.ordering == SequenceOrdering::AFTER_PIPELINES))
				continue;

			de::MovePtr<tcu::TestCaseGroup> orderingGroup(new tcu::TestCaseGroup(testCtx, kOrderingCase.name.c_str()));

			// Dynamically enable writes to all color attachments
			AddSingleTestCaseDynamic("enable_all", pct,	mask_all,				kChannelCase.enabledChannels, false, orderingGroup.get(), testCtx, kOrdering);
			// Dynamically enable writes to the first color attachment
			AddSingleTestCaseDynamic("enable_first", pct,	mask_first,				kChannelCase.enabledChannels, false, orderingGroup.get(), testCtx, kOrdering);
			// Dynamically enable writes to the second color attachment
			AddSingleTestCaseDynamic("enable_second", pct,	mask_second,			kChannelCase.enabledChannels, false, orderingGroup.get(), testCtx, kOrdering);
			// Dynamically enable writes to the last color attachment
			AddSingleTestCaseDynamic("enable_last", pct,	mask_last,				kChannelCase.enabledChannels, false, orderingGroup.get(), testCtx, kOrdering);
			// Dynamically enable writes to the first two color attachments
			AddSingleTestCaseDynamic("enable_first_and_second", pct,	mask_first_and_second,	kChannelCase.enabledChannels, false, orderingGroup.get(), testCtx, kOrdering);
			// Dynamically enable writes to the second and last color attachments
			AddSingleTestCaseDynamic("enable_second_and_last", pct,	mask_second_and_last,	kChannelCase.enabledChannels, false, orderingGroup.get(), testCtx, kOrdering);

			// Dynamically disable writes to all color attachments
			AddSingleTestCaseDynamic("disable_all", pct,	mask_all,				kChannelCase.enabledChannels, true,  orderingGroup.get(), testCtx, kOrdering);
			// Dynamically disable writes to the first color attachment
			AddSingleTestCaseDynamic("disable_first", pct,	mask_first,				kChannelCase.enabledChannels, true,  orderingGroup.get(), testCtx, kOrdering);
			// Dynamically disable writes to the second color attachment
			AddSingleTestCaseDynamic("disable_second", pct,	mask_second,			kChannelCase.enabledChannels, true,  orderingGroup.get(), testCtx, kOrdering);
			// Dynamically disable writes to the last color attachment
			AddSingleTestCaseDynamic("disable_last", pct,	mask_last,				kChannelCase.enabledChannels, true,  orderingGroup.get(), testCtx, kOrdering);
			// Dynamically disable writes to the first two color attachments
			AddSingleTestCaseDynamic("disable_first_and_second", pct,	mask_first_and_second,	kChannelCase.enabledChannels, true,  orderingGroup.get(), testCtx, kOrdering);
			// Dynamically disable writes to the second and last color attachments
			AddSingleTestCaseDynamic("disable_second_and_last", pct,	mask_second_and_last,	kChannelCase.enabledChannels, true,  orderingGroup.get(), testCtx, kOrdering);

			channelGroup->addChild(orderingGroup.release());
		}

		// Test cases for the static state
		// Note that the dynamic state test cases above also test pipelines with static state (when ordering is BEFORE_GOOD_STATIC and TWO_DRAWS_STATIC).
		// However they all bind a pipeline with the static state AFTER binding a pipeline with the dynamic state.
		// The only case missing, then, is static state alone without any dynamic pipelines in the same render pass or command buffer.
		de::MovePtr<tcu::TestCaseGroup> staticOrderingGroup(new tcu::TestCaseGroup(testCtx, "static"));

		// Statically enable writes to all color attachments
		AddSingleTestCaseStatic("enable_all", pct,	mask_all,				kChannelCase.enabledChannels, false, staticOrderingGroup.get(), testCtx);
		// Statically enable writes to the first color attachment
		AddSingleTestCaseStatic("enable_first", pct,	mask_first,				kChannelCase.enabledChannels, false, staticOrderingGroup.get(), testCtx);
		// Statically enable writes to the second color attachment
		AddSingleTestCaseStatic("enable_second", pct,	mask_second,			kChannelCase.enabledChannels, false, staticOrderingGroup.get(), testCtx);
		// Statically enable writes to the last color attachment
		AddSingleTestCaseStatic("enable_last", pct,	mask_last,				kChannelCase.enabledChannels, false, staticOrderingGroup.get(), testCtx);
		// Statically enable writes to the first two color attachments
		AddSingleTestCaseStatic("enable_first_and_second", pct,	mask_first_and_second,	kChannelCase.enabledChannels, false, staticOrderingGroup.get(), testCtx);
		// Statically enable writes to the second and last color attachments
		AddSingleTestCaseStatic("enable_second_and_last", pct,	mask_second_and_last,	kChannelCase.enabledChannels, false, staticOrderingGroup.get(), testCtx);

		// Statically disable writes to all color attachments
		AddSingleTestCaseStatic("disable_all", pct,	mask_all,				kChannelCase.enabledChannels, true,  staticOrderingGroup.get(), testCtx);
		// Statically disable writes to the first color attachment
		AddSingleTestCaseStatic("disable_first", pct,	mask_first,				kChannelCase.enabledChannels, true,  staticOrderingGroup.get(), testCtx);
		// Statically disable writes to the second color attachment
		AddSingleTestCaseStatic("disable_second", pct,	mask_second,			kChannelCase.enabledChannels, true,  staticOrderingGroup.get(), testCtx);
		// Statically disable writes to the last color attachment
		AddSingleTestCaseStatic("disable_last", pct,	mask_last,				kChannelCase.enabledChannels, true,  staticOrderingGroup.get(), testCtx);
		// Statically disable writes to the first two color attachments
		AddSingleTestCaseStatic("disable_first_and_second", pct,	mask_first_and_second,	kChannelCase.enabledChannels, true,  staticOrderingGroup.get(), testCtx);
		// Statically disable writes to the second and last color attachments
		AddSingleTestCaseStatic("disable_second_and_last", pct,	mask_second_and_last,	kChannelCase.enabledChannels, true,  staticOrderingGroup.get(), testCtx);

		channelGroup->addChild(staticOrderingGroup.release());

		colorWriteEnableGroup->addChild(channelGroup.release());
	}

	return colorWriteEnableGroup.release();
}

tcu::TestCaseGroup* createColorWriteEnable2Tests (tcu::TestContext& testCtx, vk::PipelineConstructionType pct)
{
	const deUint32		attachmentCounts[]	{ 3,4,5 };
	const deUint32		attachentMores[]	{ 0,1,2,3 };

	std::pair<bool, const char*>
		static const	setCweMoments[]
	{
		{ true,		"cwe_before_bind" },
		{ false,	"cwe_after_bind" }
	};


	tcu::TestCaseGroup* rootGroup = new tcu::TestCaseGroup(testCtx, "color_write_enable_maxa");

	for (const auto& setCweMoment : setCweMoments)
	{
		// A moment when cmdSetColorWriteEnableEXT() is called
		tcu::TestCaseGroup* setCweGroup = new tcu::TestCaseGroup(testCtx, setCweMoment.second);

		for (auto attachmentCount : attachmentCounts)
		{
			for (auto attachentMore : attachentMores)
			{
				const std::string title = "attachments" + std::to_string(attachmentCount) + "_more" + std::to_string(attachentMore);

				TestParams p;
				p.format				= VK_FORMAT_UNDEFINED;
				p.width					= 32;
				p.height				= 32;
				p.setCweBeforePlBind	= setCweMoment.first;
				p.colorWriteEnables		= true;
				p.attachmentCount		= attachmentCount;
				p.attachmentMore		= attachentMore;
				p.pct					= pct;
				setCweGroup->addChild(new ColorWriteEnable2Test(testCtx, title, p));
			}
		}
		rootGroup->addChild(setCweGroup);
	}
	return rootGroup;
}

} // pipeline
} // vkt
