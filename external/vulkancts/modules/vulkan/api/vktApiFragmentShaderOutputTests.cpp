/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Fragment Shader Output Tests.
 *//*--------------------------------------------------------------------*/

#include "vktApiFragmentShaderOutputTests.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkDebugReportUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "vkDeviceUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"

#include <map>
#include <tuple>
#include <vector>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <functional>
#include <iostream>

namespace
{
using namespace vk;
using namespace vkt;

enum ShaderOutputCases
{
	/**
	  Issue:	A fragment shader can have an output Location without a corresponding pColorAttachments[Location]
	  Approach: A fragment shader writes "location = 17" and pColorAttachments contains 5 elements only	*/
	LocationNoAttachment,
	/**
	  Issue:	There can be a pColorAttachments[N] where N is not a Location
	  Approach:	A fragment shader does not have an output to N location, but has other ones */
	AttachmentNoLocation,
	/**
	  Issue:	The fragment shader output can be a different type then the attachment format (eg. UNORM vs SINT vs UINT)
	  Approach:	Go through all cartesian product from R8{UNORM,SNORM,UINT,SINT} excluding the same formats and wait for validation layer answer */
	DifferentSignedness,
};

struct TestConfig
{
	//                     shaderFormat -> renderFormat
	typedef std::vector<std::pair<VkFormat,VkFormat>> Formats;
	ShaderOutputCases		theCase;
	Formats					formats;
	std::vector<VkFormat>	getShaderFormats () const;
	std::vector<VkFormat>	getRenderFormats () const;

	static const deUint32	unsignedIntColor	= 123u;
	static const deInt32	signedIntColor		= 111;
};

class FragmentShaderOutputInstance : public TestInstance
{
public:
	typedef std::vector<VkFormat>		Formats;
	typedef std::vector<VkClearValue>	ClearColors;
	typedef std::vector<BufferWithMemory*>	BufferPtrs;
	typedef std::vector<de::MovePtr<ImageWithMemory>>	Images;
	typedef std::vector<de::MovePtr<BufferWithMemory>>	Buffers;

								FragmentShaderOutputInstance	(Context&			context,
																 const TestConfig&	config)
									: TestInstance	(context)
									, m_config		(config)
								{
								}
	virtual						~FragmentShaderOutputInstance	(void) = default;
	virtual tcu::TestStatus		iterate							(void) override;

			Move<VkPipeline>	createGraphicsPipeline			(VkPipelineLayout	layout,
																 VkShaderModule		vertexModule,
																 VkShaderModule		fragmentModule,
																 VkRenderPass		renderPass,
																 const deUint32		subpass,
																 const deUint32		width,
																 const deUint32		height,
																 const deUint32		attachmentCount);
			Move<VkRenderPass>	createColorRenderPass			(const Formats&		colorFormats,
																 VkImageLayout		finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			void				beginColorRenderPass			(VkCommandBuffer	commandBuffer,
																 VkRenderPass		renderPass,
																 VkFramebuffer		framebuffer,
																 const deUint32		width,
																 const deUint32		height,
																 const ClearColors&	clearColors);
			bool				verifyResults					(const BufferPtrs&	buffers,
																 const ClearColors&	clearColors,
																 const deUint32		width,
																 const deUint32		height,
																 std::string&		error);
protected:
	const TestConfig			m_config;
};

Move<VkRenderPass> FragmentShaderOutputInstance::createColorRenderPass (const Formats&		formats,
																		VkImageLayout		finalLayout)
{
	const deUint32 attachmentCount = static_cast<deUint32>(formats.size());

	const VkAttachmentDescription			attachmentDescriptionTemplate
	{
		(VkAttachmentDescriptionFlags)0,	// VkAttachmentDescriptionFlags    flags
		VK_FORMAT_UNDEFINED,				// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,				// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,		// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,		// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,	// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,	// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,			// VkImageLayout                   initialLayout
		finalLayout							// VkImageLayout                   finalLayout
	};
	const VkAttachmentReference				attachmentReferenceTemplate
	{
		0,									// uint32_t			attachment;
		finalLayout,						// VkImageLayout	layout;
	};

	std::vector<VkAttachmentDescription>	attachmentDescriptions(attachmentCount, attachmentDescriptionTemplate);
	std::vector<VkAttachmentReference>		attachmentReferences(attachmentCount, attachmentReferenceTemplate);

	for (deUint32 a = 0; a < attachmentCount; ++a)
	{
		attachmentReferences[a].attachment = a;
		attachmentDescriptions[a].format = formats.at(a);
	}

	const VkSubpassDescription				subpassDescription
	{
		(VkSubpassDescriptionFlags)0,				// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,			// VkPipelineBindPoint             pipelineBindPoint
		0u,											// deUint32                        inputAttachmentCount
		nullptr,									// const VkAttachmentReference*    pInputAttachments
		attachmentCount,							// deUint32                        colorAttachmentCount
		attachmentReferences.data(),				// const VkAttachmentReference*    pColorAttachments
		nullptr,									// const VkAttachmentReference*    pResolveAttachments
		nullptr,									// const VkAttachmentReference*    pDepthStencilAttachment
		0u,											// deUint32                        preserveAttachmentCount
		nullptr										// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo			renderPassInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType
		nullptr,									// const void*                       pNext
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags           flags
		attachmentCount,							// deUint32                          attachmentCount
		attachmentDescriptions.data(),				// const VkAttachmentDescription*    pAttachments
		1u,											// deUint32                          subpassCount
		&subpassDescription,						// const VkSubpassDescription*       pSubpasses
		0u,											// deUint32                          dependencyCount
		nullptr										// const VkSubpassDependency*        pDependencies
	};

	return createRenderPass(m_context.getDeviceInterface(), m_context.getDevice(), &renderPassInfo, nullptr);
}

Move<VkPipeline> FragmentShaderOutputInstance::createGraphicsPipeline	(VkPipelineLayout	layout,
																		 VkShaderModule		vertexModule,
																		 VkShaderModule		fragmentModule,
																		 VkRenderPass		renderPass,
																		 const deUint32		subpass,
																		 const deUint32		width,
																		 const deUint32		height,
																		 const deUint32		attachmentCount)
{
	const std::vector<VkViewport>	viewports	{ makeViewport(width, height) };
	const std::vector<VkRect2D>		scissors	{ makeRect2D(width, height) };

	VkPipelineColorBlendAttachmentState colorBlendAttachmentStateTemplate{};
	colorBlendAttachmentStateTemplate.colorWriteMask = (VK_COLOR_COMPONENT_R_BIT
														| VK_COLOR_COMPONENT_G_BIT
														| VK_COLOR_COMPONENT_B_BIT
														| VK_COLOR_COMPONENT_A_BIT);
	std::vector<VkPipelineColorBlendAttachmentState>	attachments(attachmentCount, colorBlendAttachmentStateTemplate);

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
	colorBlendStateCreateInfo.attachmentCount	= attachmentCount;
	colorBlendStateCreateInfo.pAttachments		= attachments.data();

	return makeGraphicsPipeline(m_context.getDeviceInterface(), m_context.getDevice(), layout,
								vertexModule, VkShaderModule(0), VkShaderModule(0), VkShaderModule(0), fragmentModule,
								renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, subpass,
								0u, nullptr, nullptr, nullptr, nullptr, &colorBlendStateCreateInfo);
}

void FragmentShaderOutputInstance::beginColorRenderPass (VkCommandBuffer	commandBuffer,
														 VkRenderPass		renderPass,
														 VkFramebuffer		framebuffer,
														 const deUint32		width,
														 const deUint32		height,
														 const ClearColors&	clearColors)
{
	const VkRenderPassBeginInfo	renderPassBeginInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType         sType;
		nullptr,									// const void*             pNext;
		renderPass,									// VkRenderPass            renderPass;
		framebuffer,								// VkFramebuffer           framebuffer;
		makeRect2D(width, height),					// VkRect2D                renderArea;
		deUint32(clearColors.size()),				// deUint32                clearValueCount;
		clearColors.data()							// const VkClearValue*     pClearValues;
	};

	m_context.getDeviceInterface()
			.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

class FragmentShaderOutputCase : public TestCase
{
public:
							FragmentShaderOutputCase	(tcu::TestContext&	testCtx,
														 const TestConfig&	conf,
														 const std::string& name)
								: TestCase	(testCtx, name)
								, m_config	(conf) {}
	virtual	TestInstance*	createInstance				(Context&			context) const override;
	virtual void			checkSupport				(Context&			context) const override;
	virtual void			initPrograms				(SourceCollections& programs) const override;

private:
	const TestConfig	m_config;
};

std::vector<VkFormat> TestConfig::getShaderFormats () const
{
	std::vector<VkFormat>	result(formats.size());
	std::transform(formats.begin(), formats.end(), result.begin(),
				   [](const TestConfig::Formats::value_type& pair) { return pair.first; });
	return result;
}

std::vector<VkFormat> TestConfig::getRenderFormats () const
{
	std::vector<VkFormat>	result(formats.size());
	std::transform(formats.begin(), formats.end(), result.begin(),
				   [](const TestConfig::Formats::value_type& pair) { return pair.second; });
	return result;
}

void FragmentShaderOutputCase::checkSupport (Context& context) const
{
	const std::vector<VkFormat>	renderFormats	= m_config.getRenderFormats();

	VkPhysicalDeviceProperties	deviceProps		{};
	context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), &deviceProps);
	if (m_config.theCase == LocationNoAttachment)
	{
		if (deUint32(renderFormats.size() + 1u) > deviceProps.limits.maxColorAttachments)
		{
			std::stringstream s;
			s << "Shader output location (" << (renderFormats.size() + 1u) << ") exceeds "
			  << "VkPhysicalDeviceLimits::maxColorAttachments (" << deviceProps.limits.maxColorAttachments << ')';
			s.flush();
			TCU_THROW(NotSupportedError, s.str());
		}
	}
	else
	{
		if (deUint32(renderFormats.size()) > deviceProps.limits.maxColorAttachments)
		{
			std::stringstream s;
			s << "Used attachment count (" << renderFormats.size() << ") exceeds "
			  << "VkPhysicalDeviceLimits::maxColorAttachments (" << deviceProps.limits.maxColorAttachments << ')';
			s.flush();
			TCU_THROW(NotSupportedError, s.str());
		}
	}

	VkFormatProperties			formatProps		{};
	for (VkFormat format : renderFormats)
	{
		context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), format, &formatProps);
		if ((formatProps.optimalTilingFeatures & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
			!= (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
		{
			TCU_THROW(NotSupportedError, "Unable to find a format with "
				"VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT supported");
		}
		formatProps = {};
	}
}

void FragmentShaderOutputCase::initPrograms (SourceCollections& programs) const
{
	const deUint32	attachments	= static_cast<deUint32>(m_config.formats.size());
	const auto		formats		= m_config.getShaderFormats();
	auto type	= [](VkFormat format)
	{
		if (isUintFormat(format))
			return "uvec4";
		else if (isIntFormat(format))
			return "ivec4";
		else
			return "vec4";
	};
	auto value	= [](VkFormat format)
	{
		std::ostringstream os;
		if (isUintFormat(format))
		{
			os << '(' << TestConfig::unsignedIntColor << ','
					  << TestConfig::unsignedIntColor << ','
					  << TestConfig::unsignedIntColor << ','
					  << TestConfig::unsignedIntColor << ')';
		}
		else if (isIntFormat(format))
		{
			os << '(' << TestConfig::signedIntColor << ','
					  << TestConfig::signedIntColor << ','
					  << TestConfig::signedIntColor << ','
					  << TestConfig::signedIntColor << ')';
		}
		else
		{
			os << "(1.0,1.0,1.0,1.0)";
		}
		return os.str();
	};

	const deUint32	magicLoc	= attachments / 2;

	std::ostringstream frag;
	frag << "#version 450" << std::endl;
	for (deUint32 loc = 0; loc < attachments; ++loc)
	{
		if (magicLoc == loc)
		{
			if (m_config.theCase == LocationNoAttachment)
			{
				frag << "layout(location = " << attachments << ") out "
					  << type(formats.at(loc)) << " color" << loc << ';' << std::endl;
				continue;
			}
			else if (m_config.theCase == AttachmentNoLocation)
			{
				continue;
			}
		}
		frag << "layout(location = " << loc << ") out "
			  << type(formats.at(loc)) << " color" << loc << ';' << std::endl;
	}
	frag << "void main() {"	<< std::endl;
	for (deUint32 loc = 0; loc < attachments; ++loc)
	{
		if (magicLoc == loc && m_config.theCase == AttachmentNoLocation)
		{
			continue;
		}
		frag << "  color" << loc << " = "
			 << type(formats.at(loc)) << value(formats.at(loc)) << ';' << std::endl;
	}
	frag << '}' << std::endl;
	frag.flush();

	// traditional pass-through vertex shader
	const std::string vert(R"glsl(
	#version 450
	layout(location=0) in vec4 pos;
	void main() {
		gl_Position = vec4(pos.xyz, 1.0);
	}
	)glsl");

	programs.glslSources.add("frag") << glu::FragmentSource(frag.str());
	programs.glslSources.add("vert") << glu::VertexSource(vert);
}

TestInstance* FragmentShaderOutputCase::createInstance (Context& context) const
{
	return (new FragmentShaderOutputInstance(context, m_config));
}

namespace ut
{

de::MovePtr<ImageWithMemory> createImage (const DeviceInterface& vkd,
										  VkDevice				device,
										  Allocator&			allocator,
										  VkFormat				format,
										  deUint32				width,
										  deUint32				height)
{
	const VkImageUsageFlags	imageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const VkImageCreateInfo	imageCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		nullptr,								// const void*				pNext;
		VkImageCreateFlags(0),					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,									// VkFormat					format;
		{ width, height, 1u },					// VkExtent3D				extent;
		1u,										// uint32_t					mipLevels;
		1u,										// uint32_t					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		imageUsage,								// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// uint32_t					queueFamilyIndexCount;
		nullptr,								// const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	return de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
}

Move<VkImageView> createImageView (const DeviceInterface&	vkd,
								   VkDevice					device,
								   VkFormat					format,
								   VkImage					image)
{
	const VkImageSubresourceRange	subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkImageViewCreateInfo		viewCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		nullptr,									// const void*				pNext;
		VkImageViewCreateFlags(0),					// VkImageViewCreateFlags	flags;
		image,										// VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
		format,										// VkFormat					format;
		makeComponentMappingRGBA(),					// VkComponentMapping		 components;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	return ::vk::createImageView(vkd, device, &viewCreateInfo, nullptr);
}

de::MovePtr<BufferWithMemory> createBuffer (const DeviceInterface&	vkd,
											VkDevice				device,
											Allocator&				allocator,
											VkFormat				format,
											deUint32				width,
											deUint32				height)
{
	const VkBufferCreateInfo info = makeBufferCreateInfo(tcu::getPixelSize(mapVkFormat(format)) * width * height, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	return de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, info, (MemoryRequirement::HostVisible | MemoryRequirement::Coherent)));
}

FragmentShaderOutputInstance::ClearColors makeClearColors (const std::vector<VkFormat>& formats)
{
	FragmentShaderOutputInstance::ClearColors	clearColors;
	float		floatStep		= 0.5f;
	deUint32	unsignedStep	= 128u;
	deInt32		signedStep		= 64;

	for (VkFormat format : formats)
	{
		if (isUnormFormat(format))
		{
			clearColors.push_back(makeClearValueColorF32(
				(floatStep / 2.0f), (floatStep / 4.0f), (floatStep / 8.0f), 1.0f));
			floatStep /= 2.0f;
		}
		else if (isUintFormat(format))
		{
			clearColors.push_back(makeClearValueColorU32(
				(unsignedStep / 2), (unsignedStep / 4), (unsignedStep / 8), 255u));
			unsignedStep /= 2;
		}
		else
		{
			clearColors.push_back(makeClearValueColorI32(
				(signedStep / 2), (signedStep / 4), (signedStep / 8), 127));
			signedStep /= 2;
		}
	}
	return clearColors;
}

} // namespace ut

// Enable definition if you want to see attachments content
// #define PRINT_BUFFER
bool FragmentShaderOutputInstance::verifyResults	(const BufferPtrs&	buffers,
													 const ClearColors&	clearColors,
													 const deUint32		width,
													 const deUint32		height,
													 std::string&		error)
{
	bool						result			= false;
	const std::vector<VkFormat>	shaderFormats	= m_config.getShaderFormats();
	const std::vector<VkFormat>	renderFormats	= m_config.getRenderFormats();
	const deUint32				attachments		= static_cast<deUint32>(buffers.size());

	auto toleq = [](float a, float b, float tol)
	{
		return ((b >= (a - tol)) && (b <= (a + tol)));
	};

	auto isBufferUnchanged = [&](const deUint32 bufferIndex) -> bool
	{
		const VkFormat renderFormat = renderFormats.at(bufferIndex);
		tcu::ConstPixelBufferAccess	pixels(mapVkFormat(renderFormat), deInt32(width), deInt32(height), 1,
										   buffers.at(bufferIndex)->getAllocation().getHostPtr());
		for (int y = 0; y < deInt32(height); ++y)
		for (int x = 0; x < deInt32(width); ++x)
		{
			if (isUintFormat(renderFormat)) {
				if (pixels.getPixelUint(x, y).x() != clearColors.at(bufferIndex).color.uint32[0])
					return false;
			}
			else if (isIntFormat(renderFormat)) {
				if (pixels.getPixelInt(x, y).x() != clearColors.at(bufferIndex).color.int32[0])
					return false;
			}
			else {
				DE_ASSERT(isUnormFormat(renderFormat) || isSnormFormat(renderFormat));
				if (!toleq(pixels.getPixel(x, y).x(), clearColors.at(bufferIndex).color.float32[0], 0.001f))
					return false;
			}
		}
		return true;
	};
	auto isBufferRendered = [&](const deUint32 bufferIndex) -> bool
	{
		const VkFormat shaderFormat = shaderFormats.at(bufferIndex);
		const VkFormat renderFormat = renderFormats.at(bufferIndex);
		tcu::ConstPixelBufferAccess	pixels(mapVkFormat(renderFormat), deInt32(width), deInt32(height), 1,
										   buffers.at(bufferIndex)->getAllocation().getHostPtr());
		for (deInt32 y = 0; y < deInt32(height); ++y)
		for (deInt32 x = 0; x < deInt32(width); ++x)
		{
			if (isUintFormat(renderFormat)) {
				const deUint32 expected = isIntFormat(shaderFormat)
											? deUint32(TestConfig::signedIntColor)
											: TestConfig::unsignedIntColor;
				if (pixels.getPixelUint(x, y).x() != expected)
					return false;
			}
			else if (isIntFormat(renderFormat)) {
				const deInt32 expected = isIntFormat(shaderFormat)
											? TestConfig::signedIntColor
											: deInt32(TestConfig::unsignedIntColor);
				if (pixels.getPixelInt(x, y).x() != expected)
					return false;
			}
			else {
				DE_ASSERT(isUnormFormat(renderFormat) || isSnormFormat(renderFormat));
				if (pixels.getPixel(x, y).x() != 1.0f)
					return false;
			}
		}
		return true;
	};

#ifdef PRINT_BUFFER
	auto printBuffer = [&](const deUint32 bufferIndex) -> void
	{
		const VkFormat format = renderFormats.at(bufferIndex);
		tcu::ConstPixelBufferAccess	pixels(mapVkFormat(format), deInt32(width), deInt32(height), 1,
										   buffers.at(bufferIndex)->getAllocation().getHostPtr());
		std::cout << "Attachment[" << bufferIndex << "] { ";
		for (int x = 0; x < 4 && x < deInt32(width); ++x)
		{
			if (x) std::cout << "; ";
			if (isUintFormat(format)) {
				std::cout << pixels.getPixelUint(x, 0)[x] << " | "
				<< clearColors.at(bufferIndex).color.uint32[x];
			}
			else if (isIntFormat(format)) {
				std::cout << pixels.getPixelInt(x, 0)[x] << " | "
				<< clearColors.at(bufferIndex).color.int32[x];
			}
			else {
				DE_ASSERT(isUnormFormat(format) || isSnormFormat(format));
				std::cout << pixels.getPixel(x, 0)[x] << " | "
				<< clearColors.at(bufferIndex).color.float32[x];
			}
		}
		std::cout << " }" << std::endl;
	};
	for (deUint32 i = 0; i < attachments; ++i) printBuffer(i);
#endif

	if (m_config.theCase == LocationNoAttachment || m_config.theCase == AttachmentNoLocation)
	{
		const deUint32 expectedLocation = attachments / 2;
		result = isBufferUnchanged(expectedLocation);
		for (deUint32 a = 0; result && a < attachments; ++a)
		{
			if (expectedLocation == a) continue;
			result = isBufferRendered(a);
		}
	}
	else
	{
		DE_ASSERT(m_config.theCase == DifferentSignedness);
		result = true;
		for (deUint32 a = 0; result && a < attachments; ++a)
			result = isBufferRendered(a);
	}

	if (!result) error = "One or more attachments rendered incorrectly";

	return result;
}

tcu::TestStatus	FragmentShaderOutputInstance::iterate (void)
{
	const DeviceInterface&			vkd					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					familyIndex			= m_context.getUniversalQueueFamilyIndex();
		  Allocator&				allocator			= m_context.getDefaultAllocator();

	const deUint32					width				= 64u;
	const deUint32					height				= 64u;

	Move<VkShaderModule>			vertex		= createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"));
	Move<VkShaderModule>			fragment	= createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"));

	const std::vector<float>		vertices	{ +1.f, -1.f, 0.f, 0.f,   -1.f, -1.f, 0.f, 0.f,   -1.f, +1.f, 0.f, 0.f,
												  -1.f, +1.f, 0.f, 0.f,   +1.f, +1.f, 0.f, 0.f,   +1.f, -1.f, 0.f, 0.f };
	const VkBufferCreateInfo		vertexInfo	= makeBufferCreateInfo(sizeof(float) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	BufferWithMemory				vertexBuff	(vkd, device, allocator, vertexInfo, (MemoryRequirement::HostVisible | MemoryRequirement::Coherent));
	deMemcpy(vertexBuff.getAllocation().getHostPtr(), vertices.data(), static_cast<size_t>(vertexInfo.size));

	const deUint32					attachments	= static_cast<deUint32>(m_config.formats.size());
	const Formats					formats		= m_config.getRenderFormats();
	const ClearColors				clearColors	= ut::makeClearColors(formats);

	Images							images		(attachments);
	Buffers							buffers		(attachments);
	BufferPtrs						bufferPtrs	(attachments);
	std::vector<Move<VkImageView>>	imageViews	(attachments);
	std::vector<VkImageView>		views		(attachments);
	for (deUint32 i = 0; i < attachments; ++i)
	{
		images[i]		= ut::createImage(vkd, device, allocator, m_config.formats.at(i).second, width, height);
		buffers[i]		= ut::createBuffer(vkd, device, allocator, m_config.formats.at(i).second, width, height);
		imageViews[i]	= ut::createImageView(vkd, device, m_config.formats.at(i).second, **images[i]);
		views[i]		= *imageViews[i];
		bufferPtrs[i]	= buffers[i].get();
	}

	const VkImageSubresourceLayers	sresLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1u);
	const VkBufferImageCopy			copyRegion	= makeBufferImageCopy(makeExtent3D(width, height, 1u), sresLayers);
	const VkMemoryBarrier			preCopy		= makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

	Move<VkRenderPass>				renderPass	= createColorRenderPass(formats);
	Move<VkFramebuffer>				framebuffer	= makeFramebuffer(vkd, device, *renderPass, attachments, views.data(), width, height);
	Move<VkPipelineLayout>			layout		= makePipelineLayout(vkd, device);
	Move<VkPipeline>				pipeline	= createGraphicsPipeline(*layout, *vertex, *fragment, *renderPass, 0u, width, height, attachments);

	Move<VkCommandPool>				cmdPool		= makeCommandPool(vkd, device, familyIndex);
	Move<VkCommandBuffer>			cmdbuffer	= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vkd, *cmdbuffer);
		vkd.cmdBindPipeline(*cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vkd.cmdBindVertexBuffers(*cmdbuffer, 0u, 1u, &vertexBuff.get(), &static_cast<const VkDeviceSize&>(0));
		beginColorRenderPass(*cmdbuffer, *renderPass, *framebuffer, width, height, clearColors);
			vkd.cmdDraw(*cmdbuffer, deUint32(vertices.size() / 4), 1u, 0u, 0u);
		endRenderPass(vkd, *cmdbuffer);
		vkd.cmdPipelineBarrier(*cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							   VK_DEPENDENCY_BY_REGION_BIT, 1u, &preCopy, 0u, nullptr, 0u, nullptr);
		for (deUint32 i = 0; i < attachments; ++i)
		{
			vkd.cmdCopyImageToBuffer(*cmdbuffer, **images[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **buffers[i], 1u, &copyRegion);
		}
	endCommandBuffer(vkd, *cmdbuffer);
	submitCommandsAndWait(vkd, device, queue, *cmdbuffer);

	std::string error;
	const bool verdict = verifyResults(bufferPtrs, clearColors, width, height, error);

	return verdict ? tcu::TestStatus::pass(std::string()) : tcu::TestStatus::fail(error);
}

typedef std::tuple<VkFormat, std::string, bool> FormatWithName;
std::string makeTitle (const TestConfig::Formats& formats, const std::vector<FormatWithName>& names)
{
	auto findName = [&](VkFormat format)
	{
		return std::find_if(names.begin(), names.end(),
							[&](const FormatWithName& name)
							{
								return std::get<VkFormat>(name) == format;
							});
	};
	std::ostringstream os;
	for (const auto& pair : formats)
	{
		auto shaderName = findName(pair.first);		DE_ASSERT(shaderName != names.end());
		auto renderName = findName(pair.second);	DE_ASSERT(renderName != names.end());
		if (os.tellp() > 0) os << '_';
		os << std::get<std::string>(*shaderName) << 2 << std::get<std::string>(*renderName);
	}
	os.flush();
	return os.str();
}

} // unnamed namespace

namespace vkt
{
namespace api
{

tcu::TestCaseGroup*	createFragmentShaderOutputTests	(tcu::TestContext& testCtx)
{
	const std::vector<FormatWithName> formatsWithNames
	{
		{ VK_FORMAT_R8_UNORM,	"unorm",	false	},
		{ VK_FORMAT_R8_SNORM,	"snorm",	false	},
		{ VK_FORMAT_R8_UINT,	"uint",		true	},
		{ VK_FORMAT_R8_SINT,	"sint",		true	},
	};
	struct
	{
		ShaderOutputCases		aCase;
		const std::string		name;
		const bool				signedness;
	}
	const cases[]
	{
		{ LocationNoAttachment,	"location_no_attachment",	false	},
		{ AttachmentNoLocation,	"attachment_no_location",	false	},
		{ DifferentSignedness,	"different_signedness",		true	},
	};

	TestConfig::Formats signednessFormats;
	signednessFormats.reserve(formatsWithNames.size() * formatsWithNames.size());
	for (const FormatWithName& shaderFormat : formatsWithNames)
	for (const FormatWithName& renderFormat : formatsWithNames)
	{
		if (!(std::get<bool>(shaderFormat) ^ std::get<bool>(renderFormat)))
		{
			signednessFormats.emplace_back(std::get<VkFormat>(shaderFormat), std::get<VkFormat>(renderFormat));
		}
	}

	de::MovePtr<tcu::TestCaseGroup>	root(new tcu::TestCaseGroup(testCtx, "fragment_shader_output",
																"Verify fragment shader output with multiple attachments"));
	for (const auto& aCase : cases)
	{
		de::MovePtr<tcu::TestCaseGroup>	formatGroup(new tcu::TestCaseGroup(testCtx, aCase.name.c_str(), ""));
		if (aCase.signedness)
		{
			for (TestConfig::Formats::size_type i = 0; i < signednessFormats.size(); ++i)
			for (TestConfig::Formats::size_type j = 0; j < signednessFormats.size(); ++j)
			{
				if ((signednessFormats[i].first == signednessFormats[j].first)
					|| (signednessFormats[i].second == signednessFormats[j].second))
						continue;

				TestConfig config;
				config.theCase	= aCase.aCase;
				config.formats.emplace_back(signednessFormats[i]);
				config.formats.emplace_back(signednessFormats[j]);

				const std::string title = makeTitle(config.formats, formatsWithNames);
				formatGroup->addChild(new FragmentShaderOutputCase(testCtx, config, title));
			}
		}
		else
		{
			std::vector<FormatWithName> fwns(formatsWithNames);

			while (std::next_permutation(fwns.begin(), fwns.end()))
			{
				TestConfig config;
				config.theCase	= aCase.aCase;

				for (const FormatWithName& name : fwns)
				{
					config.formats.emplace_back(std::get<VkFormat>(name), std::get<VkFormat>(name));
				}
				const std::string title = makeTitle(config.formats, formatsWithNames);
				formatGroup->addChild(new FragmentShaderOutputCase(testCtx, config, title));
			}
		}
		root->addChild(formatGroup.release());
	}

	return root.release();
}

} // api
} // vkt
