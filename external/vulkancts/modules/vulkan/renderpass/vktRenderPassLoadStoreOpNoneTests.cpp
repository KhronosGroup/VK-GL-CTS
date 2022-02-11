/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google Inc.
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
 * \brief Tests load and store op "none"
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassLoadStoreOpNoneTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuPlatform.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include <cstring>
#include <cmath>
#include <vector>

namespace vkt
{
namespace renderpass
{

using namespace vk;

namespace
{

enum AttachmentInit
{
	ATTACHMENT_INIT_PRE = 1,
	ATTACHMENT_INIT_CMD_CLEAR = 2
};

enum AttachmentUsage
{
	ATTACHMENT_USAGE_UNDEFINED = 0,
	ATTACHMENT_USAGE_COLOR = 1,
	ATTACHMENT_USAGE_DEPTH = 2,
	ATTACHMENT_USAGE_STENCIL = 4,
	ATTACHMENT_USAGE_INPUT = 8,
	ATTACHMENT_USAGE_WRITE_OFF = 16,
	ATTACHMENT_USAGE_DEPTH_STENCIL = ATTACHMENT_USAGE_DEPTH | ATTACHMENT_USAGE_STENCIL,
	ATTACHMENT_USAGE_MULTISAMPLE = 32,
	ATTACHMENT_USAGE_RESOLVE_TARGET = 64,
	ATTACHMENT_USAGE_INTEGER = 128
};

struct AttachmentParams
{
	deUint32			usage;
	VkAttachmentLoadOp	loadOp;
	VkAttachmentStoreOp	storeOp;
	deUint32			init;
	bool				verifyInner;
	tcu::Vec4			innerRef;
	bool				verifyOuter;
	tcu::Vec4			outerRef;
};

struct AttachmentRef
{
	deUint32	idx;
	deUint32	usage;
};

struct SubpassParams
{
	std::vector<AttachmentRef>	attachmentRefs;
	deUint32					numDraws;
};

struct TestParams
{
	std::vector<AttachmentParams>	attachments;
	std::vector<SubpassParams>		subpasses;
	RenderingType					renderingType;
	VkFormat						depthStencilFormat;
	bool							alphaBlend;
};

struct Vertex4RGBA
{
	tcu::Vec4 position;
	tcu::Vec4 color;
};

template<typename T>
inline de::SharedPtr<vk::Move<T> > makeSharedPtr(vk::Move<T> move)
{
	return de::SharedPtr<vk::Move<T> >(new vk::Move<T>(move));
}

std::vector<Vertex4RGBA> createQuad (void)
{
	std::vector<Vertex4RGBA>	vertices;

	const float			size					= 1.0f;
	const tcu::Vec4		red						(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4		blue					(0.0f, 0.0f, 1.0f, 1.0f);
	const Vertex4RGBA	lowerLeftVertexRed		= {tcu::Vec4(-size, -size, 0.0f, 1.0f), red};
	const Vertex4RGBA	lowerRightVertexRed		= {tcu::Vec4(size, -size, 0.0f, 1.0f), red};
	const Vertex4RGBA	upperLeftVertexRed		= {tcu::Vec4(-size, size, 0.0f, 1.0f), red};
	const Vertex4RGBA	upperRightVertexRed		= {tcu::Vec4(size, size, 0.0f, 1.0f), red};
	const Vertex4RGBA	lowerLeftVertexBlue		= {tcu::Vec4(-size, -size, 0.0f, 1.0f), blue};
	const Vertex4RGBA	lowerRightVertexBlue	= {tcu::Vec4(size, -size, 0.0f, 1.0f), blue};
	const Vertex4RGBA	upperLeftVertexBlue		= {tcu::Vec4(-size, size, 0.0f, 1.0f), blue};
	const Vertex4RGBA	upperRightVertexBlue	= {tcu::Vec4(size, size, 0.0f, 1.0f), blue};

	vertices.push_back(lowerLeftVertexRed);
	vertices.push_back(lowerRightVertexRed);
	vertices.push_back(upperLeftVertexRed);
	vertices.push_back(upperLeftVertexRed);
	vertices.push_back(lowerRightVertexRed);
	vertices.push_back(upperRightVertexRed);

	vertices.push_back(lowerLeftVertexBlue);
	vertices.push_back(lowerRightVertexBlue);
	vertices.push_back(upperLeftVertexBlue);
	vertices.push_back(upperLeftVertexBlue);
	vertices.push_back(lowerRightVertexBlue);
	vertices.push_back(upperRightVertexBlue);

	return vertices;
}

deUint32 getFirstUsage (deUint32 attachmentIdx, const std::vector<SubpassParams>& subpasses)
{
	for (const auto& subpass : subpasses)
		for (const auto& ref : subpass.attachmentRefs)
			if (ref.idx == attachmentIdx)
				return ref.usage;

	return ATTACHMENT_USAGE_UNDEFINED;
}

std::string getFormatCaseName(VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

// Selects an image format based on the usage flags.
VkFormat getFormat (deUint32 usage, VkFormat depthStencilFormat)
{
	if (usage & ATTACHMENT_USAGE_DEPTH_STENCIL)
	{
		return depthStencilFormat;
	}

	if (usage & ATTACHMENT_USAGE_INTEGER)
	{
		// Color attachment using integer format.
		return VK_FORMAT_R8G8B8A8_UINT;
	}

	return VK_FORMAT_R8G8B8A8_UNORM;
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass (const DeviceInterface&	vk,
									 VkDevice				vkDevice,
									 const TestParams		testParams)
{
	const VkImageAspectFlags	aspectMask						= testParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY ? 0 : VK_IMAGE_ASPECT_COLOR_BIT;
	std::vector<AttachmentDesc>	attachmentDescriptions;
	std::vector<SubpassDesc>	subpassDescriptions;

	struct Refs
	{
		std::vector<AttachmentRef>	colorAttachmentRefs;
		std::vector<AttachmentRef>	resolveAttachmentRefs;
		std::vector<AttachmentRef>	depthStencilAttachmentRefs;
		std::vector<AttachmentRef>	inputAttachmentRefs;
	};

	std::vector<Refs>			subpassRefs;
	bool						hasInputAttachment				= false;

	for (size_t i = 0; i < testParams.attachments.size(); i++)
	{
		VkImageLayout	initialLayout;
		VkImageLayout	finalLayout;
		VkFormat		format			= getFormat(testParams.attachments[i].usage, testParams.depthStencilFormat);

		// Search for the first reference to determine the initial layout.
		deUint32 firstUsage = getFirstUsage((deUint32)i, testParams.subpasses);

		// No subpasses using this attachment. Use the usage flags of the attachment.
		if (firstUsage == ATTACHMENT_USAGE_UNDEFINED)
			firstUsage = testParams.attachments[i].usage;

		if (firstUsage & ATTACHMENT_USAGE_COLOR)
			initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		else if (firstUsage & ATTACHMENT_USAGE_DEPTH_STENCIL)
			initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		else
		{
			DE_ASSERT(firstUsage & ATTACHMENT_USAGE_INPUT);
			initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		// Set final layout to transfer src if it's being verified. Otherwise
		// just use the initial layout as it's known to be supported by
		// the usage flags.
		if (testParams.attachments[i].verifyInner || testParams.attachments[i].verifyOuter)
			finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		else
			finalLayout = initialLayout;

		const VkSampleCountFlagBits	sampleCount		= testParams.attachments[i].usage & ATTACHMENT_USAGE_MULTISAMPLE ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;

		const AttachmentDesc		attachmentDesc	=
		{
			DE_NULL,							// const void*						pNext
			(VkAttachmentDescriptionFlags) 0,	// VkAttachmentDescriptionFlags		flags
			format,								// VkFormat							format
			sampleCount,						// VkSampleCountFlagBits			samples
			testParams.attachments[i].loadOp,	// VkAttachmentLoadOp				loadOp
			testParams.attachments[i].storeOp,	// VkAttachmentStoreOp				storeOp
			testParams.attachments[i].loadOp,	// VkAttachmentLoadOp				stencilLoadOp
			testParams.attachments[i].storeOp,	// VkAttachmentStoreOp				stencilStoreOp
			initialLayout,						// VkImageLayout					initialLayout
			finalLayout							// VkImageLayout					finalLayout
		};

		attachmentDescriptions.push_back(attachmentDesc);
	}

	for (const auto& subpass : testParams.subpasses)
	{
		subpassRefs.push_back({});
		auto& refs = subpassRefs.back();

		for (const auto& ref : subpass.attachmentRefs)
		{
			VkImageLayout layout;

			if (ref.usage & ATTACHMENT_USAGE_RESOLVE_TARGET)
			{
				layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				refs.resolveAttachmentRefs.push_back({DE_NULL, ref.idx, layout, aspectMask});
			}
			else if (ref.usage & ATTACHMENT_USAGE_COLOR)
			{
				layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				refs.colorAttachmentRefs.push_back({DE_NULL, ref.idx, layout, aspectMask});
			}
			else if (ref.usage & ATTACHMENT_USAGE_DEPTH_STENCIL)
			{
				layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				const auto depthStencilAspectMask = testParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY ? 0 : getImageAspectFlags(mapVkFormat(testParams.depthStencilFormat));
				refs.depthStencilAttachmentRefs.push_back({DE_NULL, ref.idx, layout, depthStencilAspectMask});
			}
			else
			{
				DE_ASSERT(ref.usage & ATTACHMENT_USAGE_INPUT);
				layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				refs.inputAttachmentRefs.push_back({DE_NULL, ref.idx, layout, aspectMask});
				hasInputAttachment = true;
			}
		}

		const SubpassDesc			subpassDescription			=
		{
			DE_NULL,
			(VkSubpassDescriptionFlags)0,																// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,															// VkPipelineBindPoint				pipelineBindPoint
			0u,																							// deUint32							viewMask
			(deUint32)refs.inputAttachmentRefs.size(),													// deUint32							inputAttachmentCount
			refs.inputAttachmentRefs.empty() ? DE_NULL : refs.inputAttachmentRefs.data(),				// const VkAttachmentReference*		pInputAttachments
			(deUint32)refs.colorAttachmentRefs.size(),													// deUint32							colorAttachmentCount
			refs.colorAttachmentRefs.empty() ? DE_NULL : refs.colorAttachmentRefs.data(),				// const VkAttachmentReference*		pColorAttachments
			refs.resolveAttachmentRefs.empty() ? DE_NULL : refs.resolveAttachmentRefs.data(),			// const VkAttachmentReference*		pResolveAttachments
			refs.depthStencilAttachmentRefs.empty() ? DE_NULL : refs.depthStencilAttachmentRefs.data(),	// const VkAttachmentReference*		pDepthStencilAttachment
			0u,																							// deUint32							preserveAttachmentCount
			DE_NULL																						// const deUint32*					pPreserveAttachments
		};

		subpassDescriptions.push_back(subpassDescription);
	}

	// Dependency of color attachment of subpass 0 to input attachment of subpass 1.
	// Determined later if it's being used.
	const SubpassDep			subpassDependency				=
	{
		DE_NULL,										// const void*				pNext
		0u,												// uint32_t					srcSubpass
		1u,												// uint32_t					dstSubpass
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// VkPipelineStageFlags		srcStageMask
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,			// VkPipelineStageFlags		dstStageMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			// VkAccessFlags			dstAccessMask
		VK_DEPENDENCY_BY_REGION_BIT,					// VkDependencyFlags		dependencyFlags
		0u												// deInt32					viewOffset
	};

	const RenderPassCreateInfo	renderPassInfo					=
	{
		DE_NULL,											// const void*						pNext
		(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags
		(deUint32)attachmentDescriptions.size(),			// deUint32							attachmentCount
		attachmentDescriptions.data(),						// const VkAttachmentDescription*	pAttachments
		(deUint32)subpassDescriptions.size(),				// deUint32							subpassCount
		subpassDescriptions.data(),							// const VkSubpassDescription*		pSubpasses
		hasInputAttachment ? 1u : 0u,						// deUint32							dependencyCount
		hasInputAttachment ? &subpassDependency : DE_NULL,	// const VkSubpassDependency*		pDependencies
		0u,													// deUint32							correlatedViewMaskCount
		DE_NULL												// const deUint32*					pCorrelatedViewMasks
	};

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

class LoadStoreOpNoneTest : public vkt::TestCase
{
public:
							LoadStoreOpNoneTest		(tcu::TestContext&	testContext,
													 const std::string&	name,
													 const std::string&	description,
													 const TestParams&	testParams);
	virtual					~LoadStoreOpNoneTest	(void);
	virtual void			initPrograms			(SourceCollections&	sourceCollections) const;
	virtual void			checkSupport			(Context& context) const;
	virtual TestInstance*	createInstance			(Context& context) const;
private:
	const TestParams		m_testParams;
};

class LoadStoreOpNoneTestInstance : public vkt::TestInstance
{
public:
								LoadStoreOpNoneTestInstance		(Context&			context,
																 const TestParams&	testParams);
	virtual						~LoadStoreOpNoneTestInstance	(void);
	virtual tcu::TestStatus		iterate							(void);
	template<typename RenderpassSubpass>
	void						createCommandBuffer				(const DeviceInterface&					vk,
																 VkDevice								vkDevice,
																 std::vector<Move<VkImageView>>&		imageViews,
																 std::vector<Move<VkDescriptorSet>>&	descriptorSets,
																 std::vector<Move<VkPipelineLayout>>&	pipelineLayouts,
																 std::vector<Move<VkPipeline>>&			pipelines);
private:
	TestParams					m_testParams;

	const tcu::UVec2			m_imageSize;
	const tcu::UVec2			m_renderSize;

	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkRenderPass>			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;

	Move<VkBuffer>				m_vertexBuffer;
	std::vector<Vertex4RGBA>	m_vertices;
	de::MovePtr<Allocation>		m_vertexBufferAlloc;

	Move<VkCommandPool>			m_cmdPool;
	Move<VkCommandBuffer>		m_cmdBuffer;
};

LoadStoreOpNoneTest::LoadStoreOpNoneTest (tcu::TestContext&		testContext,
										  const std::string&	name,
										  const std::string&	description,
										  const TestParams&		testParams)
	: vkt::TestCase	(testContext, name, description)
	, m_testParams(testParams)
{
}

LoadStoreOpNoneTest::~LoadStoreOpNoneTest (void)
{
}

TestInstance* LoadStoreOpNoneTest::createInstance (Context& context) const
{
	return new LoadStoreOpNoneTestInstance(context, m_testParams);
}

void LoadStoreOpNoneTest::checkSupport (Context& ctx) const
{
	// Check for renderpass2 extension if used.
	if (m_testParams.renderingType == RENDERING_TYPE_RENDERPASS2)
		ctx.requireDeviceFunctionality("VK_KHR_create_renderpass2");

	// Check for dynamic_rendering extension if used
	if (m_testParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
		ctx.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	ctx.requireDeviceFunctionality("VK_EXT_load_store_op_none");

	// Check depth/stencil format support.
	for (const auto& att : m_testParams.attachments)
	{
		if (att.usage & ATTACHMENT_USAGE_DEPTH_STENCIL)
		{
			const VkFormat		format		= getFormat(att.usage, m_testParams.depthStencilFormat);
			VkImageUsageFlags	usage		= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			const auto			aspectFlags	= getImageAspectFlags(mapVkFormat(format));

			if (att.usage & ATTACHMENT_USAGE_DEPTH)
				DE_ASSERT((aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT) != 0u);

			if (att.usage & ATTACHMENT_USAGE_STENCIL)
				DE_ASSERT((aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT) != 0u);

			DE_UNREF(aspectFlags); // For release builds.

			if (att.verifyInner || att.verifyOuter)
				usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			if (att.init & ATTACHMENT_INIT_PRE)
				usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			const auto&				vki			= ctx.getInstanceInterface();
			const auto				physDev		= ctx.getPhysicalDevice();
			const auto				imgType		= VK_IMAGE_TYPE_2D;
			const auto				tiling		= VK_IMAGE_TILING_OPTIMAL;
			VkImageFormatProperties	properties;
			const auto				result		= vki.getPhysicalDeviceImageFormatProperties(physDev, format, imgType, tiling, usage, 0u, &properties);

			if (result != VK_SUCCESS)
				TCU_THROW(NotSupportedError, "Depth-stencil format not supported");
		}
	}
}

void LoadStoreOpNoneTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream fragmentSource;

	sourceCollections.glslSources.add("color_vert") << glu::VertexSource(
		"#version 450\n"
		"layout(location = 0) in highp vec4 position;\n"
		"layout(location = 1) in highp vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = position;\n"
		"	vtxColor = color;\n"
		"}\n");

	sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(
		"#version 450\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = vtxColor;\n"
		"	gl_FragDepth = 1.0;\n"
		"}\n");

	sourceCollections.glslSources.add("color_frag_uint") << glu::FragmentSource(
		"#version 450\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp uvec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = uvec4(vtxColor * vec4(255));\n"
		"	gl_FragDepth = 1.0;\n"
		"}\n");

	sourceCollections.glslSources.add("color_frag_blend") << glu::FragmentSource(
		"#version 450\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = vec4(vtxColor.rgb, 0.5);\n"
		"	gl_FragDepth = 1.0;\n"
		"}\n");

	sourceCollections.glslSources.add("color_frag_input") << glu::FragmentSource(
		"#version 450\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = subpassLoad(inputColor) + vtxColor;\n"
		"	gl_FragDepth = 1.0;\n"
		"}\n");
}

LoadStoreOpNoneTestInstance::LoadStoreOpNoneTestInstance	(Context&			context,
															 const TestParams&	testParams)
	: vkt::TestInstance	(context)
	, m_testParams		(testParams)
	, m_imageSize		(32u, 32u)
	, m_renderSize		(27u, 19u)
	, m_vertices		(createQuad())
{
}

LoadStoreOpNoneTestInstance::~LoadStoreOpNoneTestInstance (void)
{
}

template<typename RenderpassSubpass>
void LoadStoreOpNoneTestInstance::createCommandBuffer	(const DeviceInterface&					vk,
														 VkDevice								vkDevice,
														 std::vector<Move<VkImageView>>&		imageViews,
														 std::vector<Move<VkDescriptorSet>>&	descriptorSets,
														 std::vector<Move<VkPipelineLayout>>&	pipelineLayouts,
														 std::vector<Move<VkPipeline>>&			pipelines)
{
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo		(DE_NULL);

	const VkDeviceSize vertexBufferOffset = 0;

	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	if (m_testParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
	{
		std::vector<VkRenderingAttachmentInfoKHR>	colorAttachments;
		VkRenderingAttachmentInfoKHR				dsAttachment
		{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,				// VkStructureType						sType;
			DE_NULL,														// const void*							pNext;
			DE_NULL,														// VkImageView							imageView;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,				// VkImageLayout						imageLayout;
			VK_RESOLVE_MODE_NONE,											// VkResolveModeFlagBits				resolveMode;
			DE_NULL,														// VkImageView							resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout						resolveImageLayout;
			VK_ATTACHMENT_LOAD_OP_LOAD,										// VkAttachmentLoadOp					loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp					storeOp;
			makeClearValueDepthStencil(0.0f, 0u)							// VkClearValue							clearValue;
		};
		bool useDepth		= false;
		bool useStencil		= false;

		for (size_t i = 0; i < imageViews.size(); i++)
		{
			if (m_testParams.attachments[i].usage & ATTACHMENT_USAGE_MULTISAMPLE)
			{
				DE_ASSERT(m_testParams.attachments[i+1].usage & ATTACHMENT_USAGE_RESOLVE_TARGET);
				colorAttachments.push_back({
					VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,		// VkStructureType						sType;
					DE_NULL,												// const void*							pNext;
					*imageViews[i],											// VkImageView							imageView;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout						imageLayout;
					VK_RESOLVE_MODE_AVERAGE_BIT,							// VkResolveModeFlagBits				resolveMode;
					*imageViews[i+1],										// VkImageView							resolveImageView;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout						resolveImageLayout;
					m_testParams.attachments[i].loadOp,						// VkAttachmentLoadOp					loadOp;
					m_testParams.attachments[i].storeOp,					// VkAttachmentStoreOp					storeOp;
					makeClearValueColor(tcu::Vec4(0.0f))					// VkClearValue							clearValue;
					});
				i += 1;
			}
			else if (m_testParams.attachments[i].usage & ATTACHMENT_USAGE_COLOR)
			{
				colorAttachments.push_back({
					VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,		// VkStructureType						sType;
					DE_NULL,												// const void*							pNext;
					*imageViews[i],											// VkImageView							imageView;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout						imageLayout;
					VK_RESOLVE_MODE_NONE,									// VkResolveModeFlagBits				resolveMode;
					DE_NULL,												// VkImageView							resolveImageView;
					VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout						resolveImageLayout;
					m_testParams.attachments[i].loadOp,						// VkAttachmentLoadOp					loadOp;
					m_testParams.attachments[i].storeOp,					// VkAttachmentStoreOp					storeOp;
					makeClearValueColor(tcu::Vec4(0.0f))					// VkClearValue							clearValue;
					});
			}
			else
			{
				deUint32	  usage = m_testParams.attachments[i].usage;
				useDepth	= usage & ATTACHMENT_USAGE_DEPTH;
				useStencil	= usage & ATTACHMENT_USAGE_STENCIL;

				dsAttachment.imageView	= *imageViews[i];
				dsAttachment.loadOp		= m_testParams.attachments[i].loadOp;
				dsAttachment.storeOp	= m_testParams.attachments[i].storeOp;
			}
		}

		VkRenderingInfoKHR renderingInfo
		{
			VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
			DE_NULL,
			0u,																// VkRenderingFlagsKHR					flags;
			makeRect2D(m_renderSize),										// VkRect2D								renderArea;
			1u,																// deUint32								layerCount;
			0u,																// deUint32								viewMask;
			(deUint32)colorAttachments.size(),								// deUint32								colorAttachmentCount;
			de::dataOrNull(colorAttachments),								// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
			useDepth ? &dsAttachment : DE_NULL,								// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
			useStencil ? &dsAttachment : DE_NULL							// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
		};

		vk.cmdBeginRendering(*m_cmdBuffer, &renderingInfo);
	}
	else
	{
		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType
			DE_NULL,									// const void*			pNext
			*m_renderPass,								// VkRenderPass			renderPass
			*m_framebuffer,								// VkFramebuffer		framebuffer
			makeRect2D(m_renderSize),					// VkRect2D				renderArea
			0u,											// uint32_t				clearValueCount
			DE_NULL										// const VkClearValue*	pClearValues
		};
		RenderpassSubpass::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfo, &subpassBeginInfo);
	}

	// Add clear commands for selected attachments
	std::vector<VkClearAttachment> clearAttachments;
	deUint32 colorAttIdx = 0u;
	for (const auto& att : m_testParams.attachments)
	{
		if (att.init & ATTACHMENT_INIT_CMD_CLEAR)
		{
			if (att.usage & ATTACHMENT_USAGE_DEPTH_STENCIL)
			{
				clearAttachments.push_back({getImageAspectFlags(mapVkFormat(m_testParams.depthStencilFormat)), 0u,
											makeClearValueDepthStencil(0.25, 64)});
			}
			else
			{
				clearAttachments.push_back({VK_IMAGE_ASPECT_COLOR_BIT, colorAttIdx++,
											makeClearValueColorF32(0.0f, 0.0f, 0.5f, 1.0f)});
			}
		}
	}
	if (!clearAttachments.empty())
	{
		VkClearRect rect = { makeRect2D(m_renderSize), 0u, 1u };
		vk.cmdClearAttachments(*m_cmdBuffer, (deUint32) clearAttachments.size(), clearAttachments.data(), 1u, &rect);
	}

	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);

	deUint32	descriptorSetIdx	= 0u;
	deUint32	vertexOffset		= 0u;
	for (size_t i = 0; i < m_testParams.subpasses.size(); i++)
	{
		if (i != 0)
		{
			vk.cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
			DE_ASSERT(m_testParams.renderingType != RENDERING_TYPE_DYNAMIC_RENDERING);		// multi subpass tests should not be executed for dynamic rendering
		}

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines[i]);

		bool hasInput = false;
		for (const auto &ref : m_testParams.subpasses[i].attachmentRefs)
			if (ref.usage & ATTACHMENT_USAGE_INPUT)
				hasInput = true;

		if (hasInput)
			vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayouts[i], 0, 1,
									 &descriptorSets[descriptorSetIdx++].get(), 0, DE_NULL);

		for (deUint32 d = 0; d < m_testParams.subpasses[i].numDraws; d++)
		{
			vk.cmdDraw(*m_cmdBuffer, 6u, 1, vertexOffset, 0);
			vertexOffset += 6u;
		}
	}
	if (m_testParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
		vk.cmdEndRendering(*m_cmdBuffer);
	else
		RenderpassSubpass::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);
	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus LoadStoreOpNoneTestInstance::iterate (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							vkDevice				= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator							memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkComponentMapping				componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	std::vector<Move<VkImage>>				attachmentImages;
	std::vector<de::MovePtr<Allocation>>	attachmentImageAllocs;
	std::vector<Move<VkImageView>>			imageViews;
	std::vector<Move<VkPipeline>>			pipelines;

	for (const auto& att : m_testParams.attachments)
	{
		VkFormat				format				= getFormat(att.usage, m_testParams.depthStencilFormat);
		VkImageUsageFlags		usage				= 0;
		VkImageAspectFlags		aspectFlags;

		if (att.verifyInner || att.verifyOuter) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (att.init & ATTACHMENT_INIT_PRE) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		if (att.usage & ATTACHMENT_USAGE_DEPTH_STENCIL)
		{
			aspectFlags = getImageAspectFlags(mapVkFormat(format));
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		else
		{
			// Color and input attachments.
			aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

			if (att.usage & ATTACHMENT_USAGE_COLOR)
				usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			if (att.usage & ATTACHMENT_USAGE_INPUT)
				usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}

		const VkSampleCountFlagBits	sampleCount		= att.usage & ATTACHMENT_USAGE_MULTISAMPLE ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;

		const VkImageCreateInfo		imageParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,							// VkImageType				imageType
			format,										// VkFormat					format
			{ m_imageSize.x(), m_imageSize.y(), 1u },	// VkExtent3D				extent
			1u,											// deUint32					mipLevels
			1u,											// deUint32					arrayLayers
			sampleCount,								// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling
			usage,										// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode
			1u,											// deUint32					queueFamilyIndexCount
			&queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout
		};

		attachmentImages.push_back(createImage(vk, vkDevice, &imageParams));

		// Allocate and bind image memory.
		attachmentImageAllocs.push_back(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *attachmentImages.back()), MemoryRequirement::Any));
		VK_CHECK(vk.bindImageMemory(vkDevice, *attachmentImages.back(), attachmentImageAllocs.back()->getMemory(), attachmentImageAllocs.back()->getOffset()));

		// Create image view.
		const VkImageViewCreateInfo	imageViewParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageViewCreateFlags	flags
			*attachmentImages.back(),					// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType
			format,										// VkFormat					format
			componentMappingRGBA,						// VkChannelMapping			channels
			{ aspectFlags, 0u, 1u, 0u, 1u }				// VkImageSubresourceRange	subresourceRange
		};

		imageViews.push_back(createImageView(vk, vkDevice, &imageViewParams));

		if (att.init & ATTACHMENT_INIT_PRE)
		{
			// Preinitialize image
			deUint32 firstUsage = getFirstUsage((deUint32)attachmentImages.size() - 1, m_testParams.subpasses);
			if (firstUsage == ATTACHMENT_USAGE_UNDEFINED)
				firstUsage = att.usage;

			if (firstUsage & ATTACHMENT_USAGE_DEPTH_STENCIL)
			{
				const auto dstAccess	= (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
				const auto dstStage		= (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

				clearDepthStencilImage(vk, vkDevice, queue, queueFamilyIndex, *attachmentImages.back(), format, 0.5f, 128u,
									   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									   dstAccess, dstStage);
			}
			else
			{
				const auto dstAccess	= (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
				const auto dstStage		= (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
				const auto clearColor	= ((att.usage & ATTACHMENT_USAGE_INTEGER) ? makeClearValueColorU32(0u, 255u, 0u, 255u).color : makeClearValueColorF32(0.0f, 1.0f, 0.0f, 1.0f).color);
				const auto layout		= ((firstUsage & ATTACHMENT_USAGE_COLOR) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

				clearColorImage(vk, vkDevice, queue, queueFamilyIndex, *attachmentImages.back(), clearColor, VK_IMAGE_LAYOUT_UNDEFINED,
								layout, dstAccess, dstStage);
			}
		}
	}

	if (m_testParams.renderingType != RENDERING_TYPE_DYNAMIC_RENDERING)
	{
		// Create render pass.
		if (m_testParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
			m_renderPass = createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vk, vkDevice, m_testParams);
		else
			m_renderPass = createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, m_testParams);

		std::vector<VkImageView> views;
		for (const auto& view : imageViews)
			views.push_back(*view);

		// Create framebuffer.
		const VkFramebufferCreateInfo	framebufferParams	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkFramebufferCreateFlags	flags
			*m_renderPass,								// VkRenderPass				renderPass
			(deUint32)views.size(),						// deUint32					attachmentCount
			views.data(),								// const VkImageView*		pAttachments
			(deUint32)m_imageSize.x(),					// deUint32					width
			(deUint32)m_imageSize.y(),					// deUint32					height
			1u											// deUint32					layers
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create shader modules
	Unique<VkShaderModule>		vertexShaderModule			(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0));
	Unique<VkShaderModule>		fragmentShaderModule		(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0));
	Unique<VkShaderModule>		fragmentShaderModuleUint	(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag_uint"), 0));
	Unique<VkShaderModule>		fragmentShaderModuleBlend	(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag_blend"), 0));
	Unique<VkShaderModule>		fragmentShaderModuleInput	(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag_input"), 0));

	// Create descriptor pool. Prepare for using one input attachment at most.
	{
		const VkDescriptorPoolSize			descriptorPoolSize			=
		{
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	// VkDescriptorType		type
			1u										// deUint32				descriptorCount
		};

		const VkDescriptorPoolCreateInfo	descriptorPoolCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,		// VkStructureType				sType
			DE_NULL,											// const void*					pNext
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,	// VkDescriptorPoolCreateFlags	flags
			1u,													// deUint32						maxSets
			1u,													// deUint32						poolSizeCount
			&descriptorPoolSize									// const VkDescriptorPoolSize*	pPoolSizes
		};

		m_descriptorPool = createDescriptorPool(vk, vkDevice, &descriptorPoolCreateInfo);
	}

	std::vector<Move<VkDescriptorSetLayout>>		descriptorSetLayouts;
	std::vector<Move<VkDescriptorSet>>				descriptorSets;
	std::vector<Move<VkPipelineLayout>>				pipelineLayouts;

	for (const auto& subpass : m_testParams.subpasses)
	{
		deUint32	numInputAttachments	= 0u;
		bool		noColorWrite		= false;
		bool		depthTest			= false;
		bool		stencilTest			= false;
		bool		multisample			= false;
		bool		uintColorBuffer		= false;

		// Create pipeline layout.
		{
			std::vector<VkDescriptorSetLayoutBinding>	layoutBindings;

			for (const auto ref : subpass.attachmentRefs)
			{
				if (ref.usage & ATTACHMENT_USAGE_INPUT)
				{
					const VkDescriptorSetLayoutBinding	layoutBinding	=
					{
						0u,										// deUint32				binding
						VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	// VkDescriptorType		descriptorType
						1u,										// deUint32				descriptorCount
						VK_SHADER_STAGE_FRAGMENT_BIT,			// VkShaderStageFlags	stageFlags
						DE_NULL									// const VkSampler*		pImmutableSamplers
					};

					layoutBindings.push_back(layoutBinding);
					numInputAttachments++;
				}
				if (ref.usage & ATTACHMENT_USAGE_COLOR)
				{
					if (ref.usage & ATTACHMENT_USAGE_WRITE_OFF)
						noColorWrite = true;
				}
				if (ref.usage & ATTACHMENT_USAGE_DEPTH)
				{
					if (!(ref.usage & ATTACHMENT_USAGE_WRITE_OFF))
						depthTest = true;
				}
				if (ref.usage & ATTACHMENT_USAGE_STENCIL)
				{
					if (!(ref.usage & ATTACHMENT_USAGE_WRITE_OFF))
						stencilTest = true;
				}
				if (ref.usage & ATTACHMENT_USAGE_MULTISAMPLE)
				{
					multisample = true;
				}
				if (ref.usage & ATTACHMENT_USAGE_INTEGER)
				{
					uintColorBuffer = true;
				}
			}

			const VkDescriptorSetLayoutCreateInfo		descriptorSetLayoutParams	=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType
				DE_NULL,													// const void*							pNext
				0u,															// VkDescriptorSetLayoutCreateFlags		flags
				(deUint32) layoutBindings.size(),							// deUint32								bindingCount
				layoutBindings.empty() ? DE_NULL : layoutBindings.data()	// const VkDescriptorSetLayoutBinding*	pBindings
			};
			descriptorSetLayouts.push_back(createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams));

			const VkPipelineLayoutCreateInfo			pipelineLayoutParams		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
				DE_NULL,										// const void*					pNext
				0u,												// VkPipelineLayoutCreateFlags	flags
				1u,												// deUint32						setLayoutCount
				&descriptorSetLayouts.back().get(),				// const VkDescriptorSetLayout*	pSetLayouts
				0u,												// deUint32						pushConstantRangeCount
				DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
			};

			pipelineLayouts.push_back(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));
		}

		// Update descriptor set if needed.
		if (numInputAttachments > 0u)
		{
			// Assuming there's only one input attachment at most.
			DE_ASSERT(numInputAttachments == 1u);

			const VkDescriptorSetAllocateInfo	descriptorSetAllocateInfo	=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType
				DE_NULL,										// const void*					pNext
				*m_descriptorPool,								// VkDescriptorPool				descriptorPool
				1u,												// deUint32						descriptorSetCount
				&descriptorSetLayouts.back().get(),				// const VkDescriptorSetLayout*	pSetLayouts
			};

			descriptorSets.push_back(allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo));

			for (size_t i = 0; i < imageViews.size(); i++)
			{
				if (m_testParams.attachments[i].usage & ATTACHMENT_USAGE_INPUT)
				{
					const VkDescriptorImageInfo	inputImageInfo	=
					{
						DE_NULL,									// VkSampler		sampler
						*imageViews[i],								// VkImageView		imageView
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout
					};

					const VkWriteDescriptorSet	descriptorWrite	=
					{
						VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType
						DE_NULL,								// const void*						pNext
						*descriptorSets.back(),					// VkDescriptorSet					dstSet
						0u,										// deUint32							dstBinding
						0u,										// deUint32							dstArrayElement
						1u,										// deUint32							descriptorCount
						VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	// VkDescriptorType					descriptorType
						&inputImageInfo,						// const VkDescriptorImageInfo*		pImageInfo
						DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo
						DE_NULL									// const VkBufferView*				pTexelBufferView
					};
					vk.updateDescriptorSets(vkDevice, 1u, &descriptorWrite, 0u, DE_NULL);
				}
			}
		}

		// Create pipeline.
		{
			const VkVertexInputBindingDescription		vertexInputBindingDescription		=
			{
				0u,								// deUint32					binding
				(deUint32)sizeof(Vertex4RGBA),			// deUint32					strideInBytes
				VK_VERTEX_INPUT_RATE_VERTEX		// VkVertexInputStepRate	inputRate
			};

			const VkVertexInputAttributeDescription		vertexInputAttributeDescriptions[2]	=
			{
				{
					0u,								// deUint32	location
					0u,								// deUint32	binding
					VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format
					0u								// deUint32	offset
				},
				{
					1u,								// deUint32	location
					0u,								// deUint32	binding
					VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format
					(deUint32)(sizeof(float) * 4),	// deUint32	offset
				}
			};

			const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
			{
				VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType
				DE_NULL,													// const void*								pNext
				0u,															// VkPipelineVertexInputStateCreateFlags	flags
				1u,															// deUint32									vertexBindingDescriptionCount
				&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
				2u,															// deUint32									vertexAttributeDescriptionCount
				vertexInputAttributeDescriptions							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
			};

			const VkColorComponentFlags					writeMask							= noColorWrite ? 0 : VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags	colorWriteMask
																											   | VK_COLOR_COMPONENT_G_BIT
																											   | VK_COLOR_COMPONENT_B_BIT
																											   | VK_COLOR_COMPONENT_A_BIT;

			const VkPipelineColorBlendAttachmentState	colorBlendAttachmentState			=
			{
				m_testParams.alphaBlend,				// VkBool32					blendEnable
				VK_BLEND_FACTOR_SRC_ALPHA,				// VkBlendFactor			srcColorBlendFactor
				VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	// VkBlendFactor			dstColorBlendFactor
				VK_BLEND_OP_ADD,						// VkBlendOp				colorBlendOp
				VK_BLEND_FACTOR_ONE,					// VkBlendFactor			srcAlphaBlendFactor
				VK_BLEND_FACTOR_ZERO,					// VkBlendFactor			dstAlphaBlendFactor
				VK_BLEND_OP_ADD,						// VkBlendOp				alphaBlendOp
				writeMask								// VkColorComponentFlags	colorWriteMask
			};

			const VkPipelineColorBlendStateCreateInfo	colorBlendStateParams				=
			{
				VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType
				DE_NULL,													// const void*									pNext
				0u,															// VkPipelineColorBlendStateCreateFlags			flags
				VK_FALSE,													// VkBool32										logicOpEnable
				VK_LOGIC_OP_CLEAR,											// VkLogicOp									logicOp
				1u,															// deUint32										attachmentCount
				&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments
				{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4]
			};

			const VkStencilOpState							stencilOpState					=
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp
				VK_STENCIL_OP_REPLACE,	// VkStencilOp	passOp
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp
				VK_COMPARE_OP_GREATER,	// VkCompareOp	compareOp
				0xff,					// deUint32		compareMask
				0xff,					// deUint32		writeMask
				0xff					// deUint32		reference
			};

			const VkPipelineDepthStencilStateCreateInfo		depthStencilStateParams			=
			{
				VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType
				DE_NULL,													// const void*								pNext
				0u,															// VkPipelineDepthStencilStateCreateFlags	flags
				depthTest,													// VkBool32									depthTestEnable
				VK_TRUE,													// VkBool32									depthWriteEnable
				VK_COMPARE_OP_GREATER,										// VkCompareOp								depthCompareOp
				VK_FALSE,													// VkBool32									depthBoundsTestEnable
				stencilTest,												// VkBool32									stencilTestEnable
				stencilOpState,												// VkStencilOpState							front
				stencilOpState,												// VkStencilOpState							back
				0.0f,														// float									minDepthBounds
				1.0f,														// float									maxDepthBounds
			};

			const VkPipelineMultisampleStateCreateInfo		multisampleStateParams			=
			{
					VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType
					DE_NULL,														// const void*								pNext
					0u,																// VkPipelineMultisampleStateCreateFlags	flags
					multisample ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT,	// VkSampleCountFlagBits					rasterizationSamples
					VK_FALSE,														// VkBool32									sampleShadingEnable
					1.0f,															// float									minSampleShading
					DE_NULL,														// const VkSampleMask*						pSampleMask
					VK_FALSE,														// VkBool32									alphaToCoverageEnable
					VK_FALSE														// VkBool32									alphaToOneEnable
			};

			const std::vector<VkViewport>					viewports						(1, makeViewport(m_imageSize));
			const std::vector<VkRect2D>						scissors						(1, makeRect2D(m_renderSize));
			VkShaderModule									fragShader						= *fragmentShaderModule;

			if (numInputAttachments > 0u)
				fragShader = *fragmentShaderModuleInput;
			else if (uintColorBuffer)
				fragShader = *fragmentShaderModuleUint;
			else if (m_testParams.alphaBlend)
				fragShader = *fragmentShaderModuleBlend;

			VkPipelineRenderingCreateInfoKHR renderingCreateInfo
			{
				VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
				DE_NULL,
				0u,
				0u,
				DE_NULL,
				VK_FORMAT_UNDEFINED,
				VK_FORMAT_UNDEFINED
			};

			std::vector<VkFormat> colorVector;
			for (const auto& att : m_testParams.attachments)
			{
				VkFormat format = getFormat(att.usage, m_testParams.depthStencilFormat);

				if (att.usage & ATTACHMENT_USAGE_DEPTH_STENCIL)
				{
					const auto tcuFormat	= mapVkFormat(format);
					const auto hasDepth		= tcu::hasDepthComponent(tcuFormat.order);
					const auto hasStencil	= tcu::hasStencilComponent(tcuFormat.order);
					renderingCreateInfo.depthAttachmentFormat	= (hasDepth   ? format : VK_FORMAT_UNDEFINED);
					renderingCreateInfo.stencilAttachmentFormat	= (hasStencil ? format : VK_FORMAT_UNDEFINED);
				}
				else if (!(att.usage & ATTACHMENT_USAGE_RESOLVE_TARGET))
				{
					colorVector.push_back(format);
				}
			}

			vk::VkPipelineRenderingCreateInfoKHR* nextPtr = DE_NULL;
			if (m_testParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
			{
				renderingCreateInfo.colorAttachmentCount	= static_cast<deUint32>(colorVector.size());
				renderingCreateInfo.pColorAttachmentFormats = colorVector.data();

				nextPtr = &renderingCreateInfo;
			}

			pipelines.push_back(makeGraphicsPipeline(
				vk,										// const DeviceInterface&							vk
				vkDevice,								// const VkDevice									device
				*pipelineLayouts.back(),				// const VkPipelineLayout							pipelineLayout
				*vertexShaderModule,					// const VkShaderModule								vertexShaderModule
				DE_NULL,								// const VkShaderModule								tessellationControlModule
				DE_NULL,								// const VkShaderModule								tessellationEvalModule
				DE_NULL,								// const VkShaderModule								geometryShaderModule
				fragShader,								// const VkShaderModule								fragmentShaderModule
				*m_renderPass,							// const VkRenderPass								renderPass
				viewports,								// const std::vector<VkViewport>&					viewports
				scissors,								// const std::vector<VkRect2D>&						scissors
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology						topology
				(deUint32)pipelines.size(),				// const deUint32									subpass
				0u,										// const deUint32									patchControlPoints
				&vertexInputStateParams,				// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
				DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
				&multisampleStateParams,				// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
				&depthStencilStateParams,				// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
				&colorBlendStateParams,					// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
				DE_NULL,								// const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo
				nextPtr));								// const void*										pNext
		}
	}

	// Create vertex buffer.
	{
		const VkBufferCreateInfo	vertexBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType		sType
			DE_NULL,													// const void*			pNext
			0u,															// VkBufferCreateFlags	flags
			(VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()),	// VkDeviceSize			size
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,							// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode		sharingMode
			1u,															// deUint32				queueFamilyIndexCount
			&queueFamilyIndex											// const deUint32*		pQueueFamilyIndices
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Upload vertex data.
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool.
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer.
	if (m_testParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
		createCommandBuffer<RenderpassSubpass1>(vk, vkDevice, imageViews, descriptorSets, pipelineLayouts, pipelines);
	else
		createCommandBuffer<RenderpassSubpass2>(vk, vkDevice, imageViews, descriptorSets, pipelineLayouts, pipelines);

	// Submit commands.
	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	bool pass = true;

	// Verify selected attachments.
	for (size_t i = 0; i < m_testParams.attachments.size(); i++)
	{
		if (m_testParams.attachments[i].verifyInner || m_testParams.attachments[i].verifyOuter)
		{
			de::MovePtr<tcu::TextureLevel>		textureLevelResult;

			SimpleAllocator						allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
			VkFormat							format				= getFormat(m_testParams.attachments[i].usage, m_testParams.depthStencilFormat);

			if (m_testParams.attachments[i].usage & ATTACHMENT_USAGE_DEPTH)
			{
				VkImageLayout layout = (m_testParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				textureLevelResult = pipeline::readDepthAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *attachmentImages[i], m_testParams.depthStencilFormat, m_imageSize, layout);
			}
			else if (m_testParams.attachments[i].usage & ATTACHMENT_USAGE_STENCIL)
			{
				VkImageLayout layout = (m_testParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				textureLevelResult = pipeline::readStencilAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *attachmentImages[i], m_testParams.depthStencilFormat, m_imageSize, layout);
			}
			else
			{
				VkImageLayout layout = ((m_testParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				textureLevelResult = pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *attachmentImages[i], format, m_imageSize, layout);
			}

			const tcu::ConstPixelBufferAccess&	access				= textureLevelResult->getAccess();

			// Log attachment contents
			m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Attachment " + de::toString(i), "")
												<< tcu::TestLog::Image("Attachment " + de::toString(i), "", access)
												<< tcu::TestLog::EndImageSet;

			for (int y = 0; y < access.getHeight(); y++)
				for (int x = 0; x < access.getWidth(); x++)
				{
					const bool		inner	= x < (int)m_renderSize.x() && y < (int)m_renderSize.y();

					if (inner && !m_testParams.attachments[i].verifyInner)
						continue;
					if (!inner && !m_testParams.attachments[i].verifyOuter)
						continue;

					const tcu::Vec4	ref		= inner ? m_testParams.attachments[i].innerRef : m_testParams.attachments[i].outerRef;
					const tcu::Vec4	p		= access.getPixel(x, y);

					for (int c = 0; c < 4; c++)
						if (fabs(p[c] - ref[c]) > 0.01f)
							pass = false;
				}

		}
	}

	if (pass)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

} // anonymous

tcu::TestCaseGroup* createRenderPassLoadStoreOpNoneTests (tcu::TestContext& testCtx, const RenderingType renderingType)
{
	de::MovePtr<tcu::TestCaseGroup>		opNoneTests		(new tcu::TestCaseGroup(testCtx, "load_store_op_none", ""));

	const tcu::Vec4	red			(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4	green		(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4	magenta		(1.0f, 0.0f, 1.0f, 1.0f);
	const tcu::Vec4	darkBlue	(0.0f, 0.0f, 0.5f, 1.0f);
	const tcu::Vec4	blend		(0.5f, 0.0f, 0.25f, 0.5f);
	const tcu::Vec4	depthInit	(0.5f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4	depthFull	(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4	stencilInit	(128.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4	stencilFull	(255.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4	redUint		(255.0f, 0.0f, 0.0f, 255.0f);
	const tcu::Vec4	greenUint	(0.0f, 255.0f, 0.0f, 255.0f);

	// Preinitialize attachments 0 and 1 to green.
	// Subpass 0: draw a red rectangle inside attachment 0.
	// Subpass 1: use the attachment 0 as input and add blue channel to it resulting in magenta. Write the results to
	// attachment 1.
	// After the render pass attachment 0 has undefined values inside the render area because of the shader writes with
	// store op 'none', but outside should still have the preinitialized value of green. Attachment 1 should have the
	// preinitialized green outside the render area and magenta inside.
	if (renderingType != RENDERING_TYPE_DYNAMIC_RENDERING)
	{
		TestParams params;
		params.alphaBlend = false;
		params.renderingType = renderingType;
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR | ATTACHMENT_USAGE_INPUT,
									  VK_ATTACHMENT_LOAD_OP_LOAD,
									  VK_ATTACHMENT_STORE_OP_NONE_EXT,
									  ATTACHMENT_INIT_PRE,
									  false, green,
									  true, green});
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
									  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
									  VK_ATTACHMENT_STORE_OP_STORE,
									  ATTACHMENT_INIT_PRE,
									  true, magenta,
									  true, green});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}}, 1u});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_INPUT}, {1u, ATTACHMENT_USAGE_COLOR}}, 1u});

		opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "color_load_op_load_store_op_none", "", params));
	}

	// Preinitialize color attachment to green. Use a render pass with load and store ops none, but
	// disable color writes using an empty color mask. The color attachment image should have the original
	// preinitialized value after the render pass.
	{
		TestParams params;
		params.alphaBlend = false;
		params.renderingType = renderingType;
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
									  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
									  VK_ATTACHMENT_STORE_OP_NONE_EXT,
									  ATTACHMENT_INIT_PRE,
									  true, green,
									  true, green});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR | ATTACHMENT_USAGE_WRITE_OFF}}, 1u});

		opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "color_load_op_none_store_op_none_write_off", "", params));
	}

	// Preinitialize color attachment to green. Use a render pass with load and store ops none, and
	// write a rectange to the color buffer. The render area is undefined, but the outside area should
	// still have the preinitialized color.
	{
		TestParams params;
		params.alphaBlend = false;
		params.renderingType = renderingType;
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
									  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
									  VK_ATTACHMENT_STORE_OP_NONE_EXT,
									  ATTACHMENT_INIT_PRE,
									  false, green,
									  true, green});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}}, 1u});

		opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "color_load_op_none_store_op_none", "", params));
	}

	// Preinitialize color attachment to green. Use a subpass with no draw calls but instead
	// do an attachment clear command using dark blue color. Using load op none preserves the preinitialized
	// data and store op store causes the cleared blue render area to be present after the render pass.
	{
		TestParams params;
		params.alphaBlend = false;
		params.renderingType = renderingType;
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
									  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
									  VK_ATTACHMENT_STORE_OP_STORE,
									  ATTACHMENT_INIT_PRE | ATTACHMENT_INIT_CMD_CLEAR,
									  true, darkBlue,
									  true, green});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}}, 0u});

		opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "color_load_op_none_store_op_store", "", params));
	}

	// Preinitialize color attachment to green. Use a subpass with a dark blue attachment clear followed
	// by an alpha blender draw. Load op is none preserves the preinitialized data and store op store
	// keeps the blended color inside the render area after the render pass.
	{
		TestParams params;
		params.alphaBlend = true;
		params.renderingType = renderingType;
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
									  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
									  VK_ATTACHMENT_STORE_OP_STORE,
									  ATTACHMENT_INIT_PRE | ATTACHMENT_INIT_CMD_CLEAR,
									  true, blend,
									  true, green});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}}, 1u});

		opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "color_load_op_none_store_op_store_alphablend", "", params));
	}

	// Preinitialize attachments 0 and 1 to green. Attachment 0 contents inside render area is undefined  because load op 'none'.
	// Subpass 0: draw a red rectangle inside attachment 0 overwriting all undefined values.
	// Subpass 1: use the attachment 0 as input and add blue to it resulting in magenta. Write the results to attachment 1.
	// After the render pass attachment 0 contents inside the render area are undefined because of store op 'don't care',
	// but the outside area should still have the preinitialized content.
	// Attachment 1 should have the preinitialized green outside render area and magenta inside.
	if (renderingType != RENDERING_TYPE_DYNAMIC_RENDERING)
	{
		TestParams params;
		params.alphaBlend = false;
		params.renderingType = renderingType;
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR | ATTACHMENT_USAGE_INPUT,
									  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
									  VK_ATTACHMENT_STORE_OP_DONT_CARE,
									  ATTACHMENT_INIT_PRE,
									  false, green,
									  true, green});
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
									  VK_ATTACHMENT_LOAD_OP_LOAD,
									  VK_ATTACHMENT_STORE_OP_STORE,
									  ATTACHMENT_INIT_PRE,
									  true, magenta,
									  true, green});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}}, 1u});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_INPUT}, {1u, ATTACHMENT_USAGE_COLOR}}, 1u});

		opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "color_load_op_none_store_op_dontcare", "", params));
	}

	// Preinitialize color attachment to green. Use a render pass with load and store ops none for a multisample color
	// target. Write a red rectangle and check it ends up in the resolved buffer even though the multisample attachment
	// doesn't store the results.
	{
		TestParams params;
		params.alphaBlend = false;
		params.renderingType = renderingType;
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR | ATTACHMENT_USAGE_MULTISAMPLE | ATTACHMENT_USAGE_INTEGER,
									  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
									  VK_ATTACHMENT_STORE_OP_NONE_EXT,
									  ATTACHMENT_INIT_PRE,
									  false, green,
									  false, green});
		params.attachments.push_back({ATTACHMENT_USAGE_COLOR | ATTACHMENT_USAGE_RESOLVE_TARGET | ATTACHMENT_USAGE_INTEGER,
									  VK_ATTACHMENT_LOAD_OP_LOAD,
									  VK_ATTACHMENT_STORE_OP_STORE,
									  ATTACHMENT_INIT_PRE,
									  true, redUint,
									  true, greenUint});
		params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR | ATTACHMENT_USAGE_MULTISAMPLE | ATTACHMENT_USAGE_INTEGER}, {1u, ATTACHMENT_USAGE_COLOR | ATTACHMENT_USAGE_RESOLVE_TARGET}}, 1u});

		opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "color_load_op_none_store_op_none_resolve", "", params));
	}

	std::vector<VkFormat>	formats = { VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };

	for (deUint32 f = 0; f < formats.size(); ++f)
	{
		const std::string	formatName = getFormatCaseName(formats[f]);

		// Preinitialize attachment 0 (color) to green and attachment 1 (depth) to 0.5.
		// Draw a red rectangle using depth 1.0 and depth op 'greater'. Depth test will pass and update
		// depth buffer to 1.0.
		// This is followed by another draw with a blue rectangle using the same depth of 1.0. This time
		// the depth test fails and nothing is written.
		// After the renderpass the red color should remain inside the render area of the color buffer.
		// Store op 'store' for depth buffer makes the written values undefined, but the pixels outside
		// render area should still contain the original value of 0.5.
		{
			TestParams params;
			params.alphaBlend = false;
			params.depthStencilFormat = formats[f];
			params.renderingType = renderingType;
			params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE,
										  true, red,
										  true, green});
			params.attachments.push_back({ATTACHMENT_USAGE_DEPTH,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_NONE_EXT,
										  ATTACHMENT_INIT_PRE,
										  false, depthInit,
										  true, depthInit});
			params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}, {1u, ATTACHMENT_USAGE_DEPTH}}, 2u});

			opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "depth_"+formatName+"_load_op_load_store_op_none", "", params));
		}

		// Preinitialize depth attachment to 0.5. Use a render pass with load and store ops none for the depth, but
		// disable depth test which also disables depth writes. The depth attachment should have the original
		// preinitialized value after the render pass.
		{
			TestParams params;
			params.alphaBlend = false;
			params.depthStencilFormat = formats[f];
			params.renderingType = renderingType;
			params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE,
										  true, red,
										  true, green});
			params.attachments.push_back({ATTACHMENT_USAGE_DEPTH,
										  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
										  VK_ATTACHMENT_STORE_OP_NONE_EXT,
										  ATTACHMENT_INIT_PRE,
										  true, depthInit,
										  true, depthInit});
			params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}, {1u, ATTACHMENT_USAGE_DEPTH | ATTACHMENT_USAGE_WRITE_OFF}}, 1u});

			opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "depth_" + formatName + "_load_op_none_store_op_none_write_off", "", params));
		}

		// Preinitialize attachment 0 (color) to green and depth buffer to 0.5. During the render pass initialize attachment 1 (depth) to 0.25
		// using cmdClearAttachments. Draw a red rectangle using depth 1.0 and depth op 'greater'. Depth test will pass and update
		// depth buffer to 1.0. After the renderpass the color buffer should have red inside the render area and depth should have the
		// shader updated value of 1.0.
		{
			TestParams params;
			params.alphaBlend = false;
			params.depthStencilFormat = formats[f];
			params.renderingType = renderingType;
			params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE,
										  true, red,
										  true, green});
			params.attachments.push_back({ATTACHMENT_USAGE_DEPTH,
										  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE | ATTACHMENT_INIT_CMD_CLEAR,
										  true, depthFull,
										  true, depthInit});
			params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}, {1u, ATTACHMENT_USAGE_DEPTH}}, 1u});

			opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "depth_" + formatName + "_load_op_none_store_op_store", "", params));
		}

		// Preinitialize attachment 0 (color) to green and depth buffer to 0.5. During the render pass initialize attachment 1 (depth) to 0.25
		// using cmdClearAttachments. Draw a red rectangle using depth 1.0 and depth op 'greater' which will pass.
		// After the renderpass the color buffer should have red inside the render area. Depth buffer contents inside render
		// are is undefined because of store op 'don't care', but the outside should have the original value of 0.5.
		{
			TestParams params;
			params.alphaBlend = false;
			params.depthStencilFormat = formats[f];
			params.renderingType = renderingType;
			params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE,
										  true, red,
										  true, green});
			params.attachments.push_back({ATTACHMENT_USAGE_DEPTH,
										  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
										  VK_ATTACHMENT_STORE_OP_DONT_CARE,
										  ATTACHMENT_INIT_PRE | ATTACHMENT_INIT_CMD_CLEAR,
										  false, depthFull,
										  true, depthInit});
			params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}, {1u, ATTACHMENT_USAGE_DEPTH}}, 1u});

			opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "depth_" + formatName + "_load_op_none_store_op_dontcare", "", params));
		}

		// Preinitialize attachment 0 (color) to green and attachment 1 (stencil) to 128.
		// Draw a red rectangle using stencil testing with compare op 'greater' and reference of 255. The stencil test
		// will pass. This is followed by another draw with a blue rectangle using the same stencil settings. This time
		// the stencil test fails and nothing is written.
		// After the renderpass the red color should remain inside the render area of the color buffer.
		// Store op 'store' for stencil buffer makes the written values undefined, but the pixels outside
		// render area should still contain the original value of 128.
		{
			TestParams params;
			params.alphaBlend = false;
			params.depthStencilFormat = formats[f];
			params.renderingType = renderingType;
			params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE,
										  true, red,
										  true, green});
			params.attachments.push_back({ATTACHMENT_USAGE_STENCIL,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_NONE_EXT,
										  ATTACHMENT_INIT_PRE,
										  false, stencilInit,
										  true, stencilInit});
			params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}, {1u, ATTACHMENT_USAGE_STENCIL}}, 2u});

			opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "stencil_" + formatName + "_load_op_load_store_op_none", "", params));
		}

		// Preinitialize stencil attachment to 128. Use a render pass with load and store ops none for the stencil, but
		// disable stencil test which also disables stencil writes. The stencil attachment should have the original
		// preinitialized value after the render pass.
		{
			TestParams params;
			params.alphaBlend = false;
			params.depthStencilFormat = formats[f];
			params.renderingType = renderingType;
			params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE,
										  true, red,
										  true, green});
			params.attachments.push_back({ATTACHMENT_USAGE_STENCIL,
										  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
										  VK_ATTACHMENT_STORE_OP_NONE_EXT,
										  ATTACHMENT_INIT_PRE,
										  true, stencilInit,
										  true, stencilInit});
			params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}, {1u, ATTACHMENT_USAGE_STENCIL | ATTACHMENT_USAGE_WRITE_OFF}}, 1u});

			opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "stencil_" + formatName + "_load_op_none_store_op_none_write_off", "", params));
		}

		// Preinitialize attachment 0 (color) to green and stencil buffer to 128. During the render pass initialize attachment 1 (stencil) to 64
		// using cmdClearAttachments. Draw a red rectangle using stencil reference of 255 and stencil op 'greater'. Stencil test will pass and update
		// stencil buffer to 255. After the renderpass the color buffer should have red inside the render area and stencil should have the
		// shader updated value of 255.
		{
			TestParams params;
			params.alphaBlend = false;
			params.depthStencilFormat = formats[f];
			params.renderingType = renderingType;
			params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE,
										  true, red,
										  true, green});
			params.attachments.push_back({ATTACHMENT_USAGE_STENCIL,
										  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE | ATTACHMENT_INIT_CMD_CLEAR,
										  true, stencilFull,
										  true, stencilInit});
			params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}, {1u, ATTACHMENT_USAGE_STENCIL}}, 1u});

			opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "stencil_" + formatName + "_load_op_none_store_op_store", "", params));
		}

		// Preinitialize attachment 0 (color) to green and stencil buffer to 128. During the render pass initialize attachment 1 (stencil) to 64
		// using cmdClearAttachments. Draw a red rectangle using stencil reference 255 and stencil op 'greater' which will pass.
		// After the renderpass the color buffer should have red inside the render area. Stencil buffer contents inside render
		// are is undefined because of store op 'don't care', but the outside should have the original value of 128.
		{
			TestParams params;
			params.alphaBlend = false;
			params.depthStencilFormat = formats[f];
			params.renderingType = renderingType;
			params.attachments.push_back({ATTACHMENT_USAGE_COLOR,
										  VK_ATTACHMENT_LOAD_OP_LOAD,
										  VK_ATTACHMENT_STORE_OP_STORE,
										  ATTACHMENT_INIT_PRE,
										  true, red,
										  true, green});
			params.attachments.push_back({ATTACHMENT_USAGE_STENCIL,
										  VK_ATTACHMENT_LOAD_OP_NONE_EXT,
										  VK_ATTACHMENT_STORE_OP_DONT_CARE,
										  ATTACHMENT_INIT_PRE | ATTACHMENT_INIT_CMD_CLEAR,
										  false, stencilFull,
										  true, stencilInit});
			params.subpasses.push_back({{{0u, ATTACHMENT_USAGE_COLOR}, {1u, ATTACHMENT_USAGE_STENCIL}}, 1u});

			opNoneTests->addChild(new LoadStoreOpNoneTest(testCtx, "stencil_" + formatName + "_load_op_none_store_op_dontcare", "", params));
		}
	}

	return opNoneTests.release();
}

} // renderpass
} // vkt
