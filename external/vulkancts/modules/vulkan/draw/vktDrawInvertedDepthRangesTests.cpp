/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2017 Google Inc.
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
 * \brief Inverted depth ranges tests.
 *//*--------------------------------------------------------------------*/

#include "vktDrawInvertedDepthRangesTests.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuVector.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

#include "deSharedPtr.hpp"

#include <utility>
#include <array>
#include <vector>
#include <iterator>

namespace vkt
{
namespace Draw
{
namespace
{
using namespace vk;
using tcu::Vec4;
using de::SharedPtr;
using de::MovePtr;

struct TestParams
{
	float		minDepth;
	float		maxDepth;
	VkBool32	depthClampEnable;
	VkBool32	depthBiasEnable;
	float		depthBiasClamp;

};

constexpr deUint32			kImageDim		= 256u;
const VkExtent3D			kImageExtent	= makeExtent3D(kImageDim, kImageDim, 1u);
const Vec4					kClearColor		(0.0f, 0.0f, 0.0f, 1.0f);
constexpr float				kClearDepth		= 1.0f;
constexpr int				kClearStencil	= 0;
constexpr int				kMaskedStencil	= 1;
constexpr float				kDepthEpsilon	= 0.00025f;	// Used to decide if a calculated depth passes the depth test.
constexpr float				kDepthThreshold	= 0.0025f;	// Used when checking depth buffer values. Less than depth delta in each pixel (~= 1.4/205).
constexpr float				kMargin			= 0.2f;		// Space between triangle and image border. See kVertices.
constexpr float				kDiagonalMargin	= 0.00125f; // Makes sure the image diagonal falls inside the triangle. See kVertices.
const Vec4					kVertexColor	(0.0f, 0.5f, 0.5f, 1.0f); // Note: the first component will vary.

// Maximum depth slope is constant for triangle and the value here is true only for triangle used it this tests.
constexpr float				kMaxDepthSlope = 1.4f / 205;

const std::array<Vec4, 3u>	kVertices		=
{{
	Vec4(-1.0f + kMargin,                   -1.0f + kMargin,                    -0.2f, 1.0f),	//  0-----2
	Vec4(-1.0f + kMargin,                    1.0f - kMargin + kDiagonalMargin,   0.0f, 1.0f),	//   |  /
	Vec4( 1.0f - kMargin + kDiagonalMargin, -1.0f + kMargin,                     1.2f, 1.0f),	//  1|/
}};


class InvertedDepthRangesTestInstance : public TestInstance
{
public:
	enum class ReferenceImageType
	{
		COLOR = 0,
		DEPTH,
	};

	using ColorAndDepth = std::pair<tcu::ConstPixelBufferAccess, tcu::ConstPixelBufferAccess>;

												InvertedDepthRangesTestInstance	(Context& context, const TestParams& params);
	tcu::TestStatus								iterate							(void);
	ColorAndDepth								draw							(const VkViewport viewport);
	MovePtr<tcu::TextureLevel>					generateReferenceImage			(ReferenceImageType refType) const;

private:
	const TestParams				m_params;
	const VkFormat					m_colorAttachmentFormat;
	const VkFormat					m_depthAttachmentFormat;
	SharedPtr<Image>				m_colorTargetImage;
	Move<VkImageView>				m_colorTargetView;
	SharedPtr<Image>				m_depthTargetImage;
	Move<VkImageView>				m_depthTargetView;
	SharedPtr<Buffer>				m_vertexBuffer;
	Move<VkRenderPass>				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkPipeline>				m_pipeline;
};

InvertedDepthRangesTestInstance::InvertedDepthRangesTestInstance (Context& context, const TestParams& params)
	: TestInstance				(context)
	, m_params					(params)
	, m_colorAttachmentFormat	(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthAttachmentFormat	(VK_FORMAT_D16_UNORM)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();
	auto&					alloc	= m_context.getDefaultAllocator();
	auto					qIndex	= m_context.getUniversalQueueFamilyIndex();

	// Vertex data
	{
		const auto dataSize = static_cast<VkDeviceSize>(kVertices.size() * sizeof(decltype(kVertices)::value_type));
		m_vertexBuffer = Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
												alloc, MemoryRequirement::HostVisible);

		deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), kVertices.data(), static_cast<size_t>(dataSize));
		flushMappedMemoryRange(vk, device, m_vertexBuffer->getBoundMemory().getMemory(), m_vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
	}

	// Render pass
	{
		const VkImageUsageFlags	targetImageUsageFlags	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		const VkImageUsageFlags depthTargeUsageFlags	= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		const ImageCreateInfo	targetImageCreateInfo(
			VK_IMAGE_TYPE_2D,						// imageType,
			m_colorAttachmentFormat,				// format,
			kImageExtent,							// extent,
			1u,										// mipLevels,
			1u,										// arrayLayers,
			VK_SAMPLE_COUNT_1_BIT,					// samples,
			VK_IMAGE_TILING_OPTIMAL,				// tiling,
			targetImageUsageFlags);					// usage,

		m_colorTargetImage = Image::createAndAlloc(vk, device, targetImageCreateInfo, alloc, qIndex);

		const ImageCreateInfo	depthTargetImageCreateInfo(
			VK_IMAGE_TYPE_2D,						// imageType,
			m_depthAttachmentFormat,				// format,
			kImageExtent,							// extent,
			1u,										// mipLevels,
			1u,										// arrayLayers,
			VK_SAMPLE_COUNT_1_BIT,					// samples,
			VK_IMAGE_TILING_OPTIMAL,				// tiling,
			depthTargeUsageFlags);					// usage,

		m_depthTargetImage = Image::createAndAlloc(vk, device, depthTargetImageCreateInfo, alloc, qIndex);

		RenderPassCreateInfo	renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(
			m_colorAttachmentFormat,				// format
			VK_SAMPLE_COUNT_1_BIT,					// samples
			VK_ATTACHMENT_LOAD_OP_LOAD,				// loadOp
			VK_ATTACHMENT_STORE_OP_STORE,			// storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,		// stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,		// stencilStoreOp
			VK_IMAGE_LAYOUT_GENERAL,				// initialLayout
			VK_IMAGE_LAYOUT_GENERAL));				// finalLayout

		renderPassCreateInfo.addAttachment(AttachmentDescription(
			m_depthAttachmentFormat,				// format
			VK_SAMPLE_COUNT_1_BIT,					// samples
			VK_ATTACHMENT_LOAD_OP_LOAD,				// loadOp
			VK_ATTACHMENT_STORE_OP_STORE,			// storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,		// stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,		// stencilStoreOp
			VK_IMAGE_LAYOUT_GENERAL,				// initialLayout
			VK_IMAGE_LAYOUT_GENERAL));				// finalLayout

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,
			VK_IMAGE_LAYOUT_GENERAL
		};

		const VkAttachmentReference depthAttachmentReference =
		{
			1u,
			VK_IMAGE_LAYOUT_GENERAL
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(
			VK_PIPELINE_BIND_POINT_GRAPHICS,		// pipelineBindPoint
			(VkSubpassDescriptionFlags)0,			// flags
			0u,										// inputAttachmentCount
			DE_NULL,								// inputAttachments
			1u,										// colorAttachmentCount
			&colorAttachmentReference,				// colorAttachments
			DE_NULL,								// resolveAttachments
			depthAttachmentReference,				// depthStencilAttachment
			0u,										// preserveAttachmentCount
			DE_NULL));								// preserveAttachments

		m_renderPass = createRenderPass(vk, device, &renderPassCreateInfo);
	}

	// Framebuffer
	{
		const ImageViewCreateInfo colorTargetViewInfo (m_colorTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
		m_colorTargetView = createImageView(vk, device, &colorTargetViewInfo);

		const ImageViewCreateInfo depthTargetViewInfo (m_depthTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_depthAttachmentFormat);
		m_depthTargetView = createImageView(vk, device, &depthTargetViewInfo);

		std::vector<VkImageView> fbAttachments(2);
		fbAttachments[0] = *m_colorTargetView;
		fbAttachments[1] = *m_depthTargetView;

		const FramebufferCreateInfo	framebufferCreateInfo(*m_renderPass, fbAttachments, kImageExtent.width, kImageExtent.height, 1u);
		m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	// Vertex input

	const VkVertexInputBindingDescription		vertexInputBindingDescription =
	{
		0u,										// uint32_t             binding;
		sizeof(Vec4),							// uint32_t             stride;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate    inputRate;
	};

	const VkVertexInputAttributeDescription		vertexInputAttributeDescription =
	{
		0u,										// uint32_t    location;
		0u,										// uint32_t    binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat    format;
		0u										// uint32_t    offset;
	};

	const PipelineCreateInfo::VertexInputState	vertexInputState = PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription,
																										1, &vertexInputAttributeDescription);

	// Graphics pipeline

	const auto scissor = makeRect2D(kImageExtent);

	std::vector<VkDynamicState>		dynamicStates;
	dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

	const Unique<VkShaderModule>	vertexModule	(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>	fragmentModule	(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

	const PipelineLayoutCreateInfo	pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const PipelineCreateInfo::ColorBlendState::Attachment colorBlendAttachmentState;

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vertexModule,   "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fragmentModule, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState (PipelineCreateInfo::VertexInputState	(vertexInputState));
	pipelineCreateInfo.addState (PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
	pipelineCreateInfo.addState (PipelineCreateInfo::ColorBlendState	(1, &colorBlendAttachmentState));
	pipelineCreateInfo.addState (PipelineCreateInfo::ViewportState		(1, std::vector<VkViewport>(), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState (PipelineCreateInfo::DepthStencilState	(true, true));
	pipelineCreateInfo.addState (PipelineCreateInfo::RasterizerState	(
		m_params.depthClampEnable,										// depthClampEnable
		VK_FALSE,														// rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,											// polygonMode
		VK_CULL_MODE_NONE,												// cullMode
		VK_FRONT_FACE_CLOCKWISE,										// frontFace
		m_params.depthBiasEnable,										// depthBiasEnable
		0.0f,															// depthBiasConstantFactor
		m_params.depthBiasEnable ? m_params.depthBiasClamp : 0.0f,		// depthBiasClamp
		m_params.depthBiasEnable ? 1.0f : 0.0f,							// depthBiasSlopeFactor
		1.0f));															// lineWidth
	pipelineCreateInfo.addState (PipelineCreateInfo::MultiSampleState	());
	pipelineCreateInfo.addState (PipelineCreateInfo::DynamicState		(dynamicStates));

	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

InvertedDepthRangesTestInstance::ColorAndDepth InvertedDepthRangesTestInstance::draw (const VkViewport viewport)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	auto&					alloc				= m_context.getDefaultAllocator();

	// Command buffer

	const CmdPoolCreateInfo			cmdPoolCreateInfo	(queueFamilyIndex);
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Draw

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdSetViewport(*cmdBuffer, 0u, 1u, &viewport);

	{
		const VkClearColorValue			clearColor				= makeClearValueColor(kClearColor).color;
		const ImageSubresourceRange		subresourceRange		(VK_IMAGE_ASPECT_COLOR_BIT);

		const VkClearDepthStencilValue	clearDepth				= makeClearValueDepthStencil(kClearDepth, 0u).depthStencil;
		const ImageSubresourceRange		depthSubresourceRange	(VK_IMAGE_ASPECT_DEPTH_BIT);

		initialTransitionColor2DImage(vk, *cmdBuffer, m_colorTargetImage->object(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vk.cmdClearColorImage(*cmdBuffer, m_colorTargetImage->object(), VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRange);
		vk.cmdClearDepthStencilImage(*cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_GENERAL, &clearDepth, 1u, &depthSubresourceRange);
	}
	{
		const VkMemoryBarrier memBarrier =
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,												// VkStructureType    sType;
			DE_NULL,																		// const void*        pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,													// VkAccessFlags      srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT		// VkAccessFlags      dstAccessMask;
		};

		const VkMemoryBarrier depthBarrier =
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,												// VkStructureType    sType;
			DE_NULL,																		// const void*        pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,													// VkAccessFlags      srcAccessMask;
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT		// VkAccessFlags      dstAccessMask;
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT), 0, 1, &depthBarrier, 0, DE_NULL, 0, DE_NULL);
	}

	beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(kImageExtent));

	{
		const VkDeviceSize	offset	= 0;
		const VkBuffer		buffer	= m_vertexBuffer->object();

		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &buffer, &offset);
	}

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	vk.cmdDraw(*cmdBuffer, 3, 1, 0, 0);
	endRenderPass(vk, *cmdBuffer);
	endCommandBuffer(vk, *cmdBuffer);

	// Submit
	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Get result
	{
		const auto zeroOffset	= makeOffset3D(0, 0, 0);
		const auto iWidth		= static_cast<int>(kImageExtent.width);
		const auto iHeight		= static_cast<int>(kImageExtent.height);
		const auto colorPixels	= m_colorTargetImage->readSurface(queue, alloc, VK_IMAGE_LAYOUT_GENERAL, zeroOffset, iWidth, iHeight, VK_IMAGE_ASPECT_COLOR_BIT);
		const auto depthPixels	= m_depthTargetImage->readSurface(queue, alloc, VK_IMAGE_LAYOUT_GENERAL, zeroOffset, iWidth, iHeight, VK_IMAGE_ASPECT_DEPTH_BIT);

		return ColorAndDepth(colorPixels, depthPixels);
	}
}

MovePtr<tcu::TextureLevel> InvertedDepthRangesTestInstance::generateReferenceImage (ReferenceImageType refType) const
{
	const auto						iWidth			= static_cast<int>(kImageExtent.width);
	const auto						iHeight			= static_cast<int>(kImageExtent.height);
	const bool						color			= (refType == ReferenceImageType::COLOR);
	const auto						tcuFormat		= mapVkFormat(color ? m_colorAttachmentFormat : VK_FORMAT_D16_UNORM_S8_UINT);
	MovePtr<tcu::TextureLevel>		image			(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));
	const tcu::PixelBufferAccess	access			(image->getAccess());
	const float						fImageDim		= static_cast<float>(kImageDim);
	const float						p1f				= fImageDim * kMargin / 2.0f;
	const float						p2f				= fImageDim * (2.0f - kMargin + kDiagonalMargin) / 2.0f;
	const float						triangleSide	= fImageDim * (2.0f - (2.0f*kMargin - kDiagonalMargin)) / 2.0f;
	const float						clampMin		= de::min(m_params.minDepth, m_params.maxDepth);
	const float						clampMax		= de::max(m_params.minDepth, m_params.maxDepth);
	std::array<float, 3>			depthValues;
	float							depthBias		= 0.0f;

	// Depth value of each vertex in kVertices.
	DE_ASSERT(depthValues.size() == kVertices.size());
	std::transform(begin(kVertices), end(kVertices), begin(depthValues), [](const Vec4& coord) { return coord.z(); });

	if (color)
		tcu::clear(access, kClearColor);
	else
	{
		tcu::clearDepth(access, kClearDepth);
		tcu::clearStencil(access, kClearStencil);

		if (m_params.depthBiasEnable)
		{
			const float	depthBiasSlopeFactor	= 1.0f;
			const float	r						= 0.000030518f;		// minimum resolvable difference is an implementation-dependent parameter
			const float	depthBiasConstantFactor	= 0.0f;				// so we use factor 0.0 to not include it; same as in PipelineCreateInfo

			// Equations taken from vkCmdSetDepthBias manual page
			depthBias = kMaxDepthSlope * depthBiasSlopeFactor + r * depthBiasConstantFactor;

			// dbclamp(x) function depends on the sign of the depthBiasClamp
			if (m_params.depthBiasClamp < 0.0f)
				depthBias = de::max(depthBias, m_params.depthBiasClamp);
			else if (m_params.depthBiasClamp > 0.0f)
				depthBias = de::min(depthBias, m_params.depthBiasClamp);

			if (m_params.maxDepth < m_params.minDepth)
				depthBias *= -1.0f;
		}
	}

	for (int y = 0; y < iHeight; ++y)
	for (int x = 0; x < iWidth; ++x)
	{
		const float xcoord = static_cast<float>(x) + 0.5f;
		const float ycoord = static_cast<float>(y) + 0.5f;

		if (xcoord < p1f || xcoord > p2f)
			continue;

		if (ycoord < p1f || ycoord > p2f)
			continue;

		if (ycoord > -xcoord + fImageDim)
			continue;

		// Interpolate depth value taking the 3 triangle corners into account.
		const float b				= (ycoord - p1f) / triangleSide;
		const float c				= (xcoord - p1f) / triangleSide;
		const float a				= 1.0f - b - c;
		const float depth			= a * depthValues[0] + b * depthValues[1] + c * depthValues[2];

		// Depth values are always limited to the range [0,1] by clamping after depth bias addition is performed
		const float depthClamped	= de::clamp(depth + depthBias, 0.0f, 1.0f);
		const float depthFinal		= depthClamped * m_params.maxDepth + (1.0f - depthClamped) * m_params.minDepth;
		const float storedDepth		= (m_params.depthClampEnable ? de::clamp(depthFinal, clampMin, clampMax) : depthFinal);

		if (m_params.depthClampEnable || de::inRange(depth, -kDepthEpsilon, 1.0f + kDepthEpsilon))
		{
			if (color)
				access.setPixel(Vec4(depthFinal, kVertexColor.y(), kVertexColor.z(), kVertexColor.w()), x, y);
			else
			{
				if (!m_params.depthClampEnable &&
					(de::inRange(depth, -kDepthEpsilon, kDepthEpsilon) ||
					 de::inRange(depth, 1.0f - kDepthEpsilon, 1.0f + kDepthEpsilon)))
				{
					// We should avoid comparing this pixel due to possible rounding problems.
					// Pixels that should not be compared will be marked in the stencil aspect.
					access.setPixStencil(kMaskedStencil, x, y);
				}
				access.setPixDepth(storedDepth, x, y);
			}
		}
	}

	return image;
}

tcu::TestStatus InvertedDepthRangesTestInstance::iterate (void)
{
	// Set up the viewport and draw

	const VkViewport viewport =
	{
		0.0f,										// float    x;
		0.0f,										// float    y;
		static_cast<float>(kImageExtent.width),		// float    width;
		static_cast<float>(kImageExtent.height),	// float    height;
		m_params.minDepth,							// float    minDepth;
		m_params.maxDepth,							// float    maxDepth;
	};

	ColorAndDepth	results		= draw(viewport);
	auto&			resultImage	= results.first;
	auto&			resultDepth	= results.second;

	// Verify results
	auto&	log				= m_context.getTestContext().getLog();
	auto	referenceImage	= generateReferenceImage(ReferenceImageType::COLOR);
	auto	referenceDepth	= generateReferenceImage(ReferenceImageType::DEPTH);

	bool fail = false;
	// Color aspect.
	if (!tcu::fuzzyCompare(log, "Image compare", "Image compare", referenceImage->getAccess(), resultImage, 0.02f, tcu::COMPARE_LOG_RESULT))
		fail = true;

	// Depth aspect.
	bool depthFail = false;

	const auto refWidth			= referenceDepth->getWidth();
	const auto refHeight		= referenceDepth->getHeight();
	const auto refAccess		= referenceDepth->getAccess();

	tcu::TextureLevel errorMask	(mapVkFormat(VK_FORMAT_R8G8B8_UNORM), refWidth, refHeight);
	auto errorAccess			= errorMask.getAccess();
	const tcu::Vec4 kGreen		(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4 kRed		(1.0f, 0.0f, 0.0f, 1.0f);

	tcu::clear(errorAccess, kGreen);

	for (int y = 0; y < refHeight; ++y)
	for (int x = 0; x < refWidth; ++x)
	{
		// Ignore pixels that could be too close to having or not having coverage.
		const auto stencil = refAccess.getPixStencil(x, y);
		if (stencil == kMaskedStencil)
			continue;

		// Compare the rest using a known threshold.
		const auto refValue = refAccess.getPixDepth(x, y);
		const auto resValue = resultDepth.getPixDepth(x, y);
		if (!de::inRange(resValue, refValue - kDepthThreshold, refValue + kDepthThreshold))
		{
			depthFail = true;
			errorAccess.setPixel(kRed, x, y);
		}
	}

	if (depthFail)
	{
		log << tcu::TestLog::Message << "Depth Image comparison failed" << tcu::TestLog::EndMessage;
		log	<< tcu::TestLog::Image("Result", "Result", resultDepth)
			<< tcu::TestLog::Image("Reference",	"Reference", refAccess)
			<< tcu::TestLog::Image("ErrorMask",	"Error mask", errorAccess);
	}

	if (fail || depthFail)
		return tcu::TestStatus::fail("Result images are incorrect");

	return tcu::TestStatus::pass("Pass");
}

class InvertedDepthRangesTest : public TestCase
{
public:
	InvertedDepthRangesTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
		: TestCase	(testCtx, name, description)
		, m_params	(params)
	{
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in highp vec4 in_position;\n"
				<< "\n"
				<< "out gl_PerVertex {\n"
				<< "    highp vec4 gl_Position;\n"
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position = in_position;\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Fragment shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) out highp vec4 out_color;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    out_color = vec4(gl_FragCoord.z, " << kVertexColor.y() << ", " << kVertexColor.z() << ", " << kVertexColor.w() << ");\n"
				<< "}\n";

			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}
	}

	virtual void checkSupport (Context& context) const
	{
		if (m_params.depthClampEnable)
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_CLAMP);

		if (m_params.minDepth > 1.0f || m_params.minDepth < 0.0f || m_params.maxDepth > 1.0f || m_params.maxDepth < 0.0f)
			context.requireDeviceFunctionality("VK_EXT_depth_range_unrestricted");
	}

	virtual TestInstance* createInstance (Context& context) const
	{
		return new InvertedDepthRangesTestInstance(context, m_params);
	}

private:
	const TestParams	m_params;
};

void populateTestGroup (tcu::TestCaseGroup* testGroup)
{
	const struct
	{
		std::string		name;
		VkBool32		depthClamp;
	} depthClamp[] =
	{
		{ "depthclamp",		VK_TRUE		},
		{ "nodepthclamp",	VK_FALSE	},
	};

	const struct
	{
		std::string		name;
		float			delta;
		VkBool32		depthBiasEnable;
		float			depthBiasClamp;
	} depthParams[] =
	{
		{ "deltazero",					0.0f,		DE_FALSE,	 0.0f },
		{ "deltasmall",					0.3f,		DE_FALSE,	 0.0f },
		{ "deltaone",					1.0f,		DE_FALSE,	 0.0f },

		// depthBiasClamp must be smaller then maximum depth slope to make a difference
		{ "deltaone_bias_clamp_neg",	1.0f,		DE_TRUE,	-0.003f },
		{ "deltasmall_bias_clamp_pos",	0.3f,		DE_TRUE,	 0.003f },

		// Range > 1.0 requires VK_EXT_depth_range_unrestricted extension
		{ "depth_range_unrestricted",	2.7f,		DE_FALSE,	 0.0f },
	};

	for (int ndxDepthClamp = 0; ndxDepthClamp < DE_LENGTH_OF_ARRAY(depthClamp); ++ndxDepthClamp)
	for (int ndxParams = 0; ndxParams < DE_LENGTH_OF_ARRAY(depthParams); ++ndxParams)
	{
		const auto& cDepthClamp		= depthClamp[ndxDepthClamp];
		const auto& cDepthParams	= depthParams[ndxParams];
		const float minDepth		= 0.5f + cDepthParams.delta / 2.0f;
		const float maxDepth		 = minDepth - cDepthParams.delta;
		DE_ASSERT(minDepth >= maxDepth);

		const TestParams params =
		{
			minDepth,
			maxDepth,
			cDepthClamp.depthClamp,
			cDepthParams.depthBiasEnable,
			cDepthParams.depthBiasClamp,
		};

		std::string name = cDepthClamp.name + "_" + cDepthParams.name;
		testGroup->addChild(new InvertedDepthRangesTest(testGroup->getTestContext(), name, "", params));
	}
}

}	// anonymous

tcu::TestCaseGroup*	createInvertedDepthRangesTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "inverted_depth_ranges", "Inverted depth ranges", populateTestGroup);
}

}	// Draw
}	// vkt
