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
 * \brief DYnamic State Line Width Tests.
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateLineWidthTests.hpp"
#include "vktDynamicStateBaseClass.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include <array>
#include <functional>
#include <sstream>

using namespace vk;

namespace vkt
{
namespace DynamicState
{
namespace
{
struct TestLineWidthParams
{
	VkPrimitiveTopology	staticTopo;
	VkPrimitiveTopology	dynamicTopo;
	deUint32			staticWidth;
	deUint32			dynamicWidth;
	bool				dynamicFirst;
	VkFormat			format;
	deUint32			width;
	deUint32			height;
	std::string	rep () const;
};

struct LineWidthInstance : public DynamicStateBaseClass
{
									LineWidthInstance		(Context&					context,
															 PipelineConstructionType	pipelineConstructionType,
															 const TestLineWidthParams&	params)
										: DynamicStateBaseClass(context, pipelineConstructionType, "vert", "frag")
										, m_params	(params) { /* Intentionally empty */ }
	de::MovePtr<BufferWithMemory>	buildVertices			(VkPrimitiveTopology		lineTopology,
															 bool						horizontal,
															 deUint32*					vertexCount);
	Move<VkRenderPass>				buildRenderPass			(VkFormat					format);
	void							beginColorRenderPass	(VkCommandBuffer			commandBuffer,
															 VkRenderPass				renderPass,
															 VkFramebuffer				framebuffer,
															 const deUint32				width,
															 const deUint32				height);
	de::MovePtr<ImageWithMemory>	buildImage				(VkFormat					format,
															 deUint32					width,
															 deUint32					height);
	Move<VkImageView>				buildView				(const VkImage				image,
															 VkFormat					format);
	Move<VkPipeline>				buildPipeline			(VkPrimitiveTopology		lineTopology,
															 float						lineWidth,
															 bool						dynamic,
															 deUint32					subpass,
															 VkPipelineLayout			layout,
															 VkShaderModule				vertexModule,
															 VkShaderModule				fragmentModule,
															 VkRenderPass				renderPass,
															 deUint32					width,
															 deUint32					height);
	tcu::TestStatus iterate									() override;
	bool							verifyResults			(const BufferWithMemory&	resultBuffer,
															 const tcu::Vec4&			dynamicColor,
															 const tcu::Vec4&			staticColor,
															 const VkFormat				format,
															 const deUint32				width,
															 const deUint32				height,
															 const deUint32				dynamicWidth,
															 const deUint32				staticWidth);
private:
	const TestLineWidthParams	m_params;
};

template<class T, class P = T(*)[1], class R = decltype(std::begin(*std::declval<P>()))>
auto makeStdBeginEnd(void* p, deUint32 n) -> std::pair<R, R>
{
	auto tmp = std::begin(*P(p));
	auto begin = tmp;
	std::advance(tmp, n);
	return { begin, tmp };
}

de::MovePtr<BufferWithMemory> LineWidthInstance::buildVertices (VkPrimitiveTopology	lineTopology,
																bool				horizontal,
																deUint32*			vertexCount)
{
	typedef tcu::Vec4	ElementType;
	std::vector<ElementType>	vertices;
	if (lineTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
	{
		if (horizontal) {
			vertices.emplace_back(-1,0,0,0);
			vertices.emplace_back(0,0,0,0);
			vertices.emplace_back(0,0,0,0);
			vertices.emplace_back(+1,0,0,0);
		} else {
			vertices.emplace_back(0,-1,0,0);
			vertices.emplace_back(0,0,0,0);
			vertices.emplace_back(0,0,0,0);
			vertices.emplace_back(0,+1,0,0);
		}
	}
	else if (lineTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
	{
		if (horizontal) {
			vertices.emplace_back(-1,0,0,0);
			vertices.emplace_back(0,0,0,0);
			vertices.emplace_back(+1,0,0,0);
		} else {
			vertices.emplace_back(0,-1,0,0);
			vertices.emplace_back(0,0,0,0);
			vertices.emplace_back(0,+1,0,0);
		}
	}
	else { DE_ASSERT(VK_FALSE); }
	const VkBufferCreateInfo	createInfo	= makeBufferCreateInfo(vertices.size() * sizeof(ElementType), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	BufferWithMemory*			pBuffer		= new BufferWithMemory(m_context.getDeviceInterface(), m_context.getDevice(),
																   m_context.getDefaultAllocator(), createInfo,
																   (MemoryRequirement::HostVisible | MemoryRequirement::Coherent));
	DE_ASSERT(vertexCount);
	*vertexCount = static_cast<deUint32>(vertices.size());
	auto range = makeStdBeginEnd<ElementType>(pBuffer->getAllocation().getHostPtr(), *vertexCount);
	std::copy(vertices.begin(), vertices.end(), range.first);

	return de::MovePtr<BufferWithMemory>(pBuffer);
}

Move<VkRenderPass> LineWidthInstance::buildRenderPass (VkFormat format)
{
	VkAttachmentDescription desc{};
	desc.flags			= VkAttachmentDescriptionFlags(0);
	desc.format			= format;
	desc.samples		= VK_SAMPLE_COUNT_1_BIT;
	desc.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	desc.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	desc.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	desc.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	desc.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	desc.finalLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference ref{};
	ref.attachment	= 0u;
	ref.layout		= desc.finalLayout;

	VkSubpassDescription subpassTemplate{};
	subpassTemplate.flags					= VkSubpassDescriptionFlags(0);
	subpassTemplate.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassTemplate.colorAttachmentCount	= 1u;
	subpassTemplate.pColorAttachments		= &ref;
	subpassTemplate.pDepthStencilAttachment	= nullptr;
	subpassTemplate.inputAttachmentCount	= 0;
	subpassTemplate.pInputAttachments		= nullptr;
	subpassTemplate.preserveAttachmentCount	= 0;
	subpassTemplate.pPreserveAttachments	= nullptr;
	subpassTemplate.pResolveAttachments		= nullptr;

	std::array<VkSubpassDescription, 2> subpasses { subpassTemplate, subpassTemplate };

	VkSubpassDependency	dependency{};
	dependency.srcSubpass		= 0u;
	dependency.dstSubpass		= 1u;
	dependency.srcAccessMask	= VK_ACCESS_MEMORY_WRITE_BIT;
	dependency.dstAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkRenderPassCreateInfo renderPassInfo = initVulkanStructure();
	renderPassInfo.attachmentCount	= 1u;
	renderPassInfo.pAttachments		= &desc;
	renderPassInfo.subpassCount		= 2u;
	renderPassInfo.pSubpasses		= subpasses.data();
	renderPassInfo.dependencyCount	= 1u;
	renderPassInfo.pDependencies	= &dependency;

	return createRenderPass(m_context.getDeviceInterface(), m_context.getDevice(), &renderPassInfo, nullptr);
}

void LineWidthInstance::beginColorRenderPass (VkCommandBuffer		commandBuffer,
											  VkRenderPass			renderPass,
											  VkFramebuffer			framebuffer,
											  const deUint32		width,
											  const deUint32		height)
{
	const VkClearValue clearColor { { { 0.f, 0.f, 0.f, 0.f } } };
	const VkRenderPassBeginInfo	renderPassBeginInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType         sType;
		nullptr,									// const void*             pNext;
		renderPass,									// VkRenderPass            renderPass;
		framebuffer,								// VkFramebuffer           framebuffer;
		makeRect2D(width, height),					// VkRect2D                renderArea;
		1u,											// deUint32                clearValueCount;
		&clearColor									// const VkClearValue*     pClearValues;
	};
	m_context.getDeviceInterface()
			.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

de::MovePtr<ImageWithMemory> LineWidthInstance::buildImage (VkFormat format, deUint32 width, deUint32 height)
{
	VkImageCreateInfo createInfo = initVulkanStructure();
	createInfo.flags					= VkImageCreateFlags(0);
	createInfo.imageType				= VK_IMAGE_TYPE_2D;
	createInfo.format					= format;
	createInfo.extent					= makeExtent3D(width, height, 1u);
	createInfo.mipLevels				= 1u;
	createInfo.arrayLayers				= 1u;
	createInfo.samples					= VK_SAMPLE_COUNT_1_BIT;
	createInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
	createInfo.usage					= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
											| VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	createInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount	= 0u;
	createInfo.pQueueFamilyIndices		= nullptr;
	createInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;

	return de::MovePtr<ImageWithMemory>(new ImageWithMemory(m_context.getDeviceInterface(),
															m_context.getDevice(),
															m_context.getDefaultAllocator(),
															createInfo,
															MemoryRequirement::Any));
}

Move<VkImageView> LineWidthInstance::buildView (const VkImage image, VkFormat format)
{
	return makeImageView(m_context.getDeviceInterface(),
						 m_context.getDevice(),
						 image,
						 VK_IMAGE_VIEW_TYPE_2D,
						 format,
						 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u),
						 nullptr);
}

Move<VkPipeline> LineWidthInstance::buildPipeline (VkPrimitiveTopology	lineTopology,
												   float				lineWidth,
												   bool					dynamic,
												   deUint32				subpass,
												   VkPipelineLayout		layout,
												   VkShaderModule		vertexModule,
												   VkShaderModule		fragmentModule,
												   VkRenderPass			renderPass,
												   deUint32				width,
												   deUint32				height)
{
	const std::vector<VkRect2D>			scissors			{ makeRect2D(width, height) };
	const std::vector<VkViewport>		viewports			{ makeViewport(0.f, 0.f, float(width), float(height), 0.f, 1.f) };

	VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo	= initVulkanStructure();
	rasterizationCreateInfo.lineWidth	= dynamic ? 0.0f : lineWidth;

	const VkDynamicState dynamicStates[1] { VK_DYNAMIC_STATE_LINE_WIDTH };
	VkPipelineDynamicStateCreateInfo dynamicCreateInfo	= initVulkanStructure();
	dynamicCreateInfo.pDynamicStates	= dynamicStates;
	dynamicCreateInfo.dynamicStateCount	= 1u;

	const auto attribute	= makeVertexInputAttributeDescription(0u, subpass, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);
	const auto binding		= makeVertexInputBindingDescription(subpass, static_cast<deUint32>(sizeof(tcu::Vec4)), VK_VERTEX_INPUT_RATE_VERTEX);
	VkPipelineVertexInputStateCreateInfo inputCreateInfo = initVulkanStructure();
	inputCreateInfo.flags	= VkPipelineVertexInputStateCreateFlags(0);
	inputCreateInfo.vertexAttributeDescriptionCount	= 1u;
	inputCreateInfo.pVertexAttributeDescriptions	= &attribute;
	inputCreateInfo.vertexBindingDescriptionCount	= 1u;
	inputCreateInfo.pVertexBindingDescriptions		= &binding;

	return makeGraphicsPipeline(m_context.getDeviceInterface(), m_context.getDevice(), layout,
								vertexModule, VkShaderModule(0), VkShaderModule(0), VkShaderModule(0), fragmentModule,
								renderPass, viewports, scissors, lineTopology, subpass,
								0u,			// patchControlPoints
								&inputCreateInfo,
								&rasterizationCreateInfo,
								nullptr,	// multisampleStateCreateInfo
								nullptr,	// depthStencilStateCreateInfo
								nullptr,	// colorBlendStateCreateInfo
								dynamic ? &dynamicCreateInfo : nullptr);
}

bool LineWidthInstance::verifyResults (const BufferWithMemory&	resultBuffer,
									   const tcu::Vec4&			dynamicColor,
									   const tcu::Vec4&			staticColor,
									   const VkFormat			format,
									   const deUint32			width,
									   const deUint32			height,
									   const deUint32			dynamicWidth,
									   const deUint32			staticWidth)
{
	tcu::ConstPixelBufferAccess	pixels(mapVkFormat(format), deInt32(width), deInt32(height), 1, resultBuffer.getAllocation().getHostPtr());

	// count pixels in vertical line
	deUint32 staticLineWidth = 0u;
	for (deInt32 x = 0; x < deInt32(width); ++x)
	{
		if (pixels.getPixel(x, 0) == staticColor)
			++staticLineWidth;
	}

	// count pixels in horizontal line
	deUint32 dynamicLineWidth = 0u;
	for (deInt32 y = 0; y < deInt32(height); ++y)
	{
		if (pixels.getPixel(0, y) == dynamicColor)
			++dynamicLineWidth;
	}

	return ((dynamicWidth == dynamicLineWidth) && (staticWidth == staticLineWidth));
}

tcu::TestStatus LineWidthInstance::iterate ()
{
	const DeviceInterface&			vkd				= m_context.getDeviceInterface();
	const VkDevice					device			= m_context.getDevice();
	Allocator&						allocator		= m_context.getDefaultAllocator();
	const deUint32					familyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue			= m_context.getUniversalQueue();

	deUint32						dynamicVertCount(0);
	deUint32						staticVertCount	(0);
	Move<VkShaderModule>			vertex			= createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"));
	Move<VkShaderModule>			fragment		= createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"));
													// note that dynamic lines are always drawn horizontally
	de::MovePtr<BufferWithMemory>	dynamicVertices	= buildVertices(m_params.dynamicTopo, (m_params.dynamicFirst == true), &dynamicVertCount); DE_ASSERT(dynamicVertCount);
	de::MovePtr<BufferWithMemory>	staticVertices	= buildVertices(m_params.staticTopo, (m_params.dynamicFirst == false), &staticVertCount); DE_ASSERT(staticVertCount);
	const VkBuffer					dynamicBuffs[2]	{ **dynamicVertices, **staticVertices };
	const VkBuffer*					vertexBuffers	= dynamicBuffs;
	const VkDeviceSize				vertexOffsets[]	{ 0u, 0u };
	const tcu::Vec4					dynamicColor	(1, 0, 1, 1);
	const tcu::Vec4					staticColor		(0, 1, 0, 1);
	de::MovePtr<ImageWithMemory>	image			= buildImage(m_params.format, m_params.width, m_params.height);
	const VkImageMemoryBarrier		prepareCopy		= makeImageMemoryBarrier(
														VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
														VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
														**image, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const VkBufferImageCopy			copyRegion		= makeBufferImageCopy(makeExtent3D(m_params.width, m_params.height, 1u),
														makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	Move<VkImageView>				attachment		= buildView(**image, m_params.format);
	Move<VkRenderPass>				renderPass		= buildRenderPass(m_params.format);
	Move<VkFramebuffer>				framebuffer		= makeFramebuffer(vkd, device, *renderPass, *attachment, m_params.width, m_params.height);
	const VkDeviceSize				resultByteSize	= m_params.width * m_params.height * tcu::getPixelSize(mapVkFormat(m_params.format));
	const VkBufferCreateInfo		resultInfo		= makeBufferCreateInfo(resultByteSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<BufferWithMemory>	resultBuffer	= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator,
														resultInfo, MemoryRequirement::HostVisible | MemoryRequirement::Coherent));
	const VkPushConstantRange		pcRange			{ VK_SHADER_STAGE_FRAGMENT_BIT, 0u, static_cast<deUint32>(sizeof(tcu::Vec4)) };
	Move<VkPipelineLayout>			pipelineLayout	= makePipelineLayout(vkd, device, VK_NULL_HANDLE, &pcRange);
	Move<VkPipeline>				dynamicPipeline	= buildPipeline(m_params.dynamicTopo, float(m_params.dynamicWidth), true,
																	m_params.dynamicFirst ? 0u : 1u,
																	*pipelineLayout, *vertex, *fragment, *renderPass,
																	m_params.width, m_params.height);
	Move<VkPipeline>				staticPipeline	= buildPipeline(m_params.staticTopo, float(m_params.staticWidth), false,
																	m_params.dynamicFirst ? 1u : 0u,
																	*pipelineLayout, *vertex, *fragment, *renderPass,
																	m_params.width, m_params.height);
	Move<VkCommandPool>				cmdPool			= makeCommandPool(vkd, device, familyIndex);
	Move<VkCommandBuffer>			cmdBuffer		= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	auto putDynamics	= [&]() -> void
	{
		vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *dynamicPipeline);
		vkd.cmdPushConstants(*cmdBuffer, *pipelineLayout, pcRange.stageFlags, pcRange.offset, pcRange.size, &dynamicColor);
		vkd.cmdSetLineWidth(*cmdBuffer, float(m_params.dynamicWidth));
		vkd.cmdDraw(*cmdBuffer, dynamicVertCount, 1u, 0u, 0u);
	};
	std::function<void()> putDynamicsRecords(putDynamics);
	auto putStatics		= [&]() -> void
	{
		vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *staticPipeline);
		vkd.cmdPushConstants(*cmdBuffer, *pipelineLayout, pcRange.stageFlags, pcRange.offset, pcRange.size, &staticColor);
		vkd.cmdDraw(*cmdBuffer, staticVertCount, 1u, 0u, 0u);
	};
	std::function<void()> putStaticsRecords(putStatics);

	beginCommandBuffer(vkd, *cmdBuffer);
		vkd.cmdBindVertexBuffers(*cmdBuffer, 0u, 2u, vertexBuffers, vertexOffsets);
		beginColorRenderPass(*cmdBuffer, *renderPass, *framebuffer, m_params.width, m_params.height);
			(m_params.dynamicFirst ? putDynamicsRecords : putStaticsRecords)();
		vkd.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
			(m_params.dynamicFirst ? putStaticsRecords : putDynamicsRecords)();
		endRenderPass(vkd, *cmdBuffer);
		vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
						   VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 0u, nullptr, 1u, &prepareCopy);
		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **resultBuffer, 1u, &copyRegion);
	endCommandBuffer(vkd, *cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

	const bool status = verifyResults(*resultBuffer, dynamicColor, staticColor,
									  m_params.format, m_params.width, m_params.height,
									  m_params.dynamicWidth, m_params.staticWidth);
	return status ? tcu::TestStatus::pass(std::string()) : tcu::TestStatus::fail(std::string());
}

struct LineWidthCase : public TestCase
{
					LineWidthCase	(tcu::TestContext&			testCtx,
									 const std::string&			name,
									 const std::string&			description,
									 PipelineConstructionType	pipelineConstructionType,
									 const TestLineWidthParams&	params)
						: TestCase(testCtx, name, description)
						, m_pipelineConstructionType	(pipelineConstructionType)
						, m_params						(params) { /* Intentionally empty */ }

	void			checkSupport	(Context&					context) const override;
	void			initPrograms	(SourceCollections&			programs) const override;
	TestInstance*	createInstance	(Context&					context) const override {
										return new LineWidthInstance(context, m_pipelineConstructionType, m_params); }
private:
	const PipelineConstructionType	m_pipelineConstructionType;
	const TestLineWidthParams		m_params;
};

void LineWidthCase::initPrograms (SourceCollections& programs) const
{
	const std::string vert(
	R"glsl(#version 450
	layout(location = 0) in vec4 pos;
	void main() {
		gl_Position = vec4(pos.xy, 0.0, 1.0);
	})glsl");

	const std::string frag(
	R"glsl(#version 450
	layout(push_constant) uniform PC { vec4 color; };
	layout(location = 0) out vec4 attachment;
	void main() {
		attachment = vec4(color.rgb, 1.0);
	})glsl");

	programs.glslSources.add("frag") << glu::FragmentSource(frag);
	programs.glslSources.add("vert") << glu::VertexSource(vert);
}

void LineWidthCase::checkSupport (Context& context) const
{
	VkPhysicalDeviceFeatures	fts	{};
	const InstanceInterface&	vki	= context.getInstanceInterface();
	const VkPhysicalDevice		dev	= context.getPhysicalDevice();

	checkPipelineConstructionRequirements(vki, dev, m_pipelineConstructionType);

	if (float(m_params.staticWidth) < context.getDeviceProperties().limits.lineWidthRange[0]
		|| float(m_params.staticWidth) > context.getDeviceProperties().limits.lineWidthRange[1]
		|| float(m_params.dynamicWidth) < context.getDeviceProperties().limits.lineWidthRange[0]
		|| float(m_params.dynamicWidth) > context.getDeviceProperties().limits.lineWidthRange[1])
	{
		TCU_THROW(NotSupportedError, "Line widths don't meet VkPhysicalDeviceLimits::lineWidthRange");
	}

	vki.getPhysicalDeviceFeatures(dev, &fts);
	if (!context.getDeviceFeatures().wideLines)
	{
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceFeatures::wideLines not supported");
	}

	DE_ASSERT(context.getDeviceFeatures().wideLines);
}

} // anonymous namespace

DynamicStateLWTests::DynamicStateLWTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
	: tcu::TestCaseGroup	(testCtx, "line_width", "Test for VK_DYNAMIC_STATE_LINE_WIDTH")
	, m_pipelineConstructionType	(pipelineConstructionType)
{
	/* Consciously empty */
}

std::string TestLineWidthParams::rep () const
{
	auto topo = [](VkPrimitiveTopology topology) -> const char*	{
		switch (topology) {
			case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
				return "list";
			case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
				return "strip";
			default: DE_ASSERT(VK_FALSE);
		}
		return "";
	};
	std::ostringstream os;
	if (dynamicFirst)
		os << topo(dynamicTopo) << dynamicWidth << '_' << topo(staticTopo) << staticWidth;
	else
		os << topo(staticTopo) << staticWidth << '_' << topo(dynamicTopo) << dynamicWidth;
	os.flush();
	return os.str();
}

void DynamicStateLWTests::init (void)
{
	TestLineWidthParams const params[] {
		{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, 0u, 0u, true,  VK_FORMAT_R32G32B32A32_SFLOAT, 128, 128 },
		{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, 0u, 0u, false, VK_FORMAT_R32G32B32A32_SFLOAT, 128, 128 },
		{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  0u, 0u, true,  VK_FORMAT_R32G32B32A32_SFLOAT, 128, 128 },
		{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  0u, 0u, false, VK_FORMAT_R32G32B32A32_SFLOAT, 128, 128 },

		{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  0u, 0u, true,  VK_FORMAT_R32G32B32A32_SFLOAT, 128, 128 },
		{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,  0u, 0u, false, VK_FORMAT_R32G32B32A32_SFLOAT, 128, 128 },
		{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, 0u, 0u, true,  VK_FORMAT_R32G32B32A32_SFLOAT, 128, 128 },
		{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, 0u, 0u, false, VK_FORMAT_R32G32B32A32_SFLOAT, 128, 128 },
	};
	de::MovePtr<tcu::TestCaseGroup>	dynaStatic(new tcu::TestCaseGroup(m_testCtx, "dyna_static", ""));
	de::MovePtr<tcu::TestCaseGroup>	staticDyna(new tcu::TestCaseGroup(m_testCtx, "static_dyna", ""));
	deUint32 lineWidth = 0u;
	for (const TestLineWidthParams& param : params)
	{
		TestLineWidthParams p(param);
		p.staticWidth	= ++lineWidth;
		p.dynamicWidth	= ++lineWidth;
		if (param.dynamicFirst)
			dynaStatic->addChild(new LineWidthCase(m_testCtx, p.rep(), std::string(), m_pipelineConstructionType, p));
		else
			staticDyna->addChild(new LineWidthCase(m_testCtx, p.rep(), std::string(), m_pipelineConstructionType, p));
	}
	addChild(dynaStatic.release());
	addChild(staticDyna.release());
}

} // DynamicState
} // vkt
