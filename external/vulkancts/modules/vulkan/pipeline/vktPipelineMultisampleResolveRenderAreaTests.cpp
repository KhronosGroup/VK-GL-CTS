/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google Inc.
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
 *//*!
 * \file vktPipelineMultisampleResolveRenderAreaTests.hpp
 * \brief Multisample resolve tests where a render area is less than an
 *        attachment size.
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleResolveRenderAreaTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace pipeline
{
using namespace vk;
using de::UniquePtr;

namespace
{

enum class TestShape
{
	SHAPE_RECTANGLE,
	SHAPE_DIAMOND,
	SHAPE_PARALLELOGRAM
};

class MultisampleRenderAreaTestInstance : public TestInstance
{
public:
					MultisampleRenderAreaTestInstance	(Context&							context,
														 const PipelineConstructionType		pipelineConstructionType,
														 const deUint32						sampleCount,
														 const tcu::IVec2					framebufferSize,
														 const TestShape					testShape,
														 const VkFormat						colorFormat)
														: TestInstance					(context)
														, m_pipelineConstructionType	(pipelineConstructionType)
														, m_sampleCount					(sampleCount)
														, m_framebufferSize				(framebufferSize)
														, m_testShape					(testShape)
														, m_colorFormat					(colorFormat)
														{}

	tcu::TestStatus	iterate								(void);

private:
	VkImageCreateInfo	makeImageCreateInfo		(const tcu::IVec2& imageSize, const deUint32 sampleCount);

	RenderPassWrapper	makeRenderPass			(const DeviceInterface&			vk,
												 const VkDevice					device,
												 const VkFormat					colorFormat,
												 const VkImageLayout			initialLayout);

	void				preparePipelineWrapper	(GraphicsPipelineWrapper&		gpw,
												 const PipelineLayoutWrapper&	pipelineLayout,
												 const VkRenderPass				renderPass,
												 const ShaderWrapper			vertexModule,
												 const ShaderWrapper			fragmentModule,
												 const tcu::IVec2&				framebufferSize);

	const PipelineConstructionType		m_pipelineConstructionType;
	const deUint32						m_sampleCount;
	const tcu::IVec2					m_framebufferSize;
	const TestShape						m_testShape;
	const VkFormat						m_colorFormat;
};

VkImageCreateInfo MultisampleRenderAreaTestInstance::makeImageCreateInfo(const tcu::IVec2& imageSize, const deUint32 sampleCount)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,												// VkStructureType			sType;
		DE_NULL,																			// const void*				pNext;
		(VkImageCreateFlags)0,																// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,																	// VkImageType				imageType;
		m_colorFormat,																		// VkFormat					format;
		makeExtent3D(imageSize.x(), imageSize.y(), 1),										// VkExtent3D				extent;
		1u,																					// deUint32					mipLevels;
		1u,																					// deUint32					arrayLayers;
		sampleCount < 2u ? VK_SAMPLE_COUNT_1_BIT : (VkSampleCountFlagBits)m_sampleCount,	// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,															// VkImageTiling			tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,				// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,															// VkSharingMode			sharingMode;
		0u,																					// deUint32					queueFamilyIndexCount;
		DE_NULL,																			// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,															// VkImageLayout			initialLayout;
	};

	return imageParams;
}

RenderPassWrapper MultisampleRenderAreaTestInstance::makeRenderPass (const DeviceInterface&			vk,
																	 const VkDevice					device,
																	 const VkFormat					colorFormat,
																	 const VkImageLayout			initialLayout)
{
	const VkAttachmentDescription			colorAttachmentDescription		=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags;
		colorFormat,								// VkFormat						format;
		(VkSampleCountFlagBits)m_sampleCount,		// VkSampleCountFlagBits		samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp			loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp;
		initialLayout,								// VkImageLayout				initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout;
	};

	const VkAttachmentDescription			resolveAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags	flags;
		colorFormat,								// VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp			loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp;
		initialLayout,								// VkImageLayout				initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout;
	};

	std::vector<VkAttachmentDescription>	attachmentDescriptions;

	attachmentDescriptions.push_back(colorAttachmentDescription);
	attachmentDescriptions.push_back(resolveAttachmentDescription);

	const VkAttachmentReference				colorAttachmentRef				=
	{
		0u,											// uint32_t			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference				resolveAttachmentRef			=
	{
		1u,											// uint32_t			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkSubpassDescription				subpassDescription				=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint;
		0u,									// uint32_t							inputAttachmentCount;
		DE_NULL,							// const VkAttachmentReference*		pInputAttachments;
		1u,									// uint32_t							colorAttachmentCount;
		&colorAttachmentRef,				// const VkAttachmentReference*		pColorAttachments;
		&resolveAttachmentRef,				// const VkAttachmentReference*		pResolveAttachments;
		DE_NULL,							// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,									// uint32_t							preserveAttachmentCount;
		DE_NULL								// const uint32_t*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo			renderPassInfo					=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags;
		(deUint32)attachmentDescriptions.size(),	// uint32_t							attachmentCount;
		attachmentDescriptions.data(),				// const VkAttachmentDescription*	pAttachments;
		1u,											// uint32_t							subpassCount;
		&subpassDescription,						// const VkSubpassDescription*		pSubpasses;
		0u,											// uint32_t							dependencyCount;
		DE_NULL,									// const VkSubpassDependency*		pDependencies;
	};

	return RenderPassWrapper(m_pipelineConstructionType, vk, device, &renderPassInfo);
}

void MultisampleRenderAreaTestInstance::preparePipelineWrapper (GraphicsPipelineWrapper&		gpw,
																const PipelineLayoutWrapper&	pipelineLayout,
																const VkRenderPass				renderPass,
																const ShaderWrapper				vertexModule,
																const ShaderWrapper				fragmentModule,
																const tcu::IVec2&				framebufferSize)
{
	const std::vector<VkViewport>	viewports		{ makeViewport(framebufferSize) };
	const std::vector<VkRect2D>		scissors		{ makeRect2D(framebufferSize) };
	VkSampleMask					sampleMask		= 0xffff;

	const VkPipelineMultisampleStateCreateInfo	multisampleStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0u,															// VkPipelineMultisampleStateCreateFlags	flags
		(VkSampleCountFlagBits)m_sampleCount,						// VkSampleCountFlagBits					rasterizationSamples
		DE_FALSE,													// VkBool32									sampleShadingEnable
		0.0f,														// float									minSampleShading
		&sampleMask,												// const VkSampleMask*						pSampleMask
		DE_FALSE,													// VkBool32									alphaToCoverageEnable
		DE_FALSE,													// VkBool32									alphaToOneEnable
	};

	gpw.setDefaultDepthStencilState()
	   .setDefaultColorBlendState()
	   .setDefaultRasterizationState()
	   .setupVertexInputState()
	   .setupPreRasterizationShaderState(viewports,
			scissors,
			pipelineLayout,
			renderPass,
			0u,
			vertexModule)
	   .setupFragmentShaderState(pipelineLayout,
			renderPass,
			0u,
			fragmentModule,
			DE_NULL,
			&multisampleStateCreateInfo)
	   .setupFragmentOutputState(renderPass, 0u, DE_NULL, &multisampleStateCreateInfo)
	   .setMonolithicPipelineLayout(pipelineLayout)
	   .buildPipeline();
}

tcu::TestStatus	MultisampleRenderAreaTestInstance::iterate (void)
{
	const InstanceInterface&		vki						= m_context.getInstanceInterface();
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice			= m_context.getPhysicalDevice();
	const VkDevice					device					= m_context.getDevice();
	Allocator&						allocator				= m_context.getDefaultAllocator();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkImageSubresourceRange	colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const ShaderWrapper				vertexModule			(ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const ShaderWrapper				fragmentModule			(ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"), 0u));

	const Unique<VkImage>			colorImage				(makeImage(vk, device, makeImageCreateInfo(m_framebufferSize, m_sampleCount)));
	const UniquePtr<Allocation>		colorImageAlloc			(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorImageView			(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, colorSubresourceRange));

	const Unique<VkImage>			resolveColorImage		(makeImage(vk, device, makeImageCreateInfo(m_framebufferSize, 1u)));
	const UniquePtr<Allocation>		resolveColorImageAlloc	(bindImage(vk, device, allocator, *resolveColorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		resolveColorImageView	(makeImageView(vk, device, *resolveColorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, colorSubresourceRange));

	const VkImage					images[]				= { *colorImage, *resolveColorImage };
	const VkImageView				attachmentImages[]		= { *colorImageView, *resolveColorImageView };
	const deUint32					numUsedAttachmentImages	= DE_LENGTH_OF_ARRAY(attachmentImages);

	const VkDeviceSize				colorBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(m_colorFormat)) * m_framebufferSize.x() * m_framebufferSize.y();
	const Unique<VkBuffer>			colorBufferResults		(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc		(bindBuffer(vk, device, allocator, *colorBufferResults, MemoryRequirement::HostVisible));

	RenderPassWrapper				renderPassOne			(makeRenderPass(vk, device, m_colorFormat, VK_IMAGE_LAYOUT_UNDEFINED));
	RenderPassWrapper				renderPassTwo			(makeRenderPass(vk, device, m_colorFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	renderPassOne.createFramebuffer(vk, device, numUsedAttachmentImages, images, attachmentImages, m_framebufferSize.x(), m_framebufferSize.y());
	renderPassTwo.createFramebuffer(vk, device, numUsedAttachmentImages, images, attachmentImages, m_framebufferSize.x(), m_framebufferSize.y());

	const PipelineLayoutWrapper		pipelineLayout			(m_pipelineConstructionType, vk, device);
	GraphicsPipelineWrapper			graphicsPipeline		{vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), m_pipelineConstructionType};

	const Unique<VkCommandPool>		cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	commandBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Main vertex buffer
	const deUint32					numVertices				= 6;
	const VkDeviceSize				vertexBufferSizeBytes	= 256;
	const Unique<VkBuffer>			vertexBuffer			(makeBuffer(vk, device, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc		(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	preparePipelineWrapper(graphicsPipeline, pipelineLayout, *renderPassOne, vertexModule, fragmentModule, m_framebufferSize);

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		// The shapes should fit just and just inside the renderArea.
		if (m_testShape == TestShape::SHAPE_RECTANGLE)
		{
			float size = 0.5f;

			pVertices[0] = tcu::Vec4(size, -size, 0.0f, 1.0f);
			pVertices[1] = tcu::Vec4(-size, -size, 0.0f, 1.0f);
			pVertices[2] = tcu::Vec4(-size, size, 0.0f, 1.0f);

			pVertices[3] = tcu::Vec4(-size, size, 0.0f, 1.0f);
			pVertices[4] = tcu::Vec4(size, size, 0.0f, 1.0f);
			pVertices[5] = tcu::Vec4(size, -size, 0.0f, 1.0f);
		}

		if (m_testShape == TestShape::SHAPE_DIAMOND)
		{
			float size		= 0.5f;

			pVertices[0]	= tcu::Vec4( size,  0.0f, 0.0f, 1.0f);
			pVertices[1]	= tcu::Vec4(-0.0f, -size, 0.0f, 1.0f);
			pVertices[2]	= tcu::Vec4(-size,  0.0f, 0.0f, 1.0f);

			pVertices[3]	= tcu::Vec4( size,  0.0f, 0.0f, 1.0f);
			pVertices[4]	= tcu::Vec4(-size,  0.0f, 0.0f, 1.0f);
			pVertices[5]	= tcu::Vec4(-0.0f,  size, 0.0f, 1.0f);
		}

		if (m_testShape == TestShape::SHAPE_PARALLELOGRAM)
		{
			float size = 0.3125f;

			pVertices[0] = tcu::Vec4(size, -size, 0.0f, 1.0f);
			pVertices[1] = tcu::Vec4(-0.5f, -size, 0.0f, 1.0f);
			pVertices[2] = tcu::Vec4(-size, size, 0.0f, 1.0f);

			pVertices[3] = tcu::Vec4(-size, size, 0.0f, 1.0f);
			pVertices[4] = tcu::Vec4(0.5f, size, 0.0f, 1.0f);
			pVertices[5] = tcu::Vec4(size, -size, 0.0f, 1.0f);
		}

		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	const VkDeviceSize				vertexBufferOffset		= 0ull;

	const VkRect2D					testRenderArea			=
	{
		makeOffset2D(m_framebufferSize.x() / 4u, m_framebufferSize.x() / 4u),
		makeExtent2D(m_framebufferSize.x() / 2u, m_framebufferSize.y() / 2u),
	};

	const std::vector<VkClearValue>	clearValuesFullArea		= { makeClearValueColor(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)), makeClearValueColor(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)) };
	const std::vector<VkClearValue>	clearValuesTestArea		= { makeClearValueColor(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)), makeClearValueColor(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)) };

	beginCommandBuffer(vk, *commandBuffer);

	const VkRect2D					fullRenderArea			=
	{
		makeOffset2D(0u, 0u),
		makeExtent2D(m_framebufferSize.x(), m_framebufferSize.y()),
	};

	// Clear whole render area with red color.
	renderPassOne.begin(vk, *commandBuffer, fullRenderArea, static_cast<deUint32>(clearValuesFullArea.size()), clearValuesFullArea.data());
	renderPassTwo.end(vk, *commandBuffer);

	// Draw shape when render area size is halved.
	renderPassTwo.begin(vk, *commandBuffer, testRenderArea, static_cast<deUint32>(clearValuesTestArea.size()), clearValuesTestArea.data());
	vk.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	graphicsPipeline.bind(*commandBuffer);
	vk.cmdDraw(*commandBuffer, numVertices, 1u, 0u, 0u);
	renderPassTwo.end(vk, *commandBuffer);

	copyImageToBuffer(vk, *commandBuffer, *resolveColorImage, *colorBufferResults, m_framebufferSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	endCommandBuffer(vk, *commandBuffer);
	submitCommandsAndWait(vk, device, queue, *commandBuffer);

	// Verify color output
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);

		tcu::TestLog&						log					= m_context.getTestContext().getLog();
		const Unique<VkBuffer>				testBufferResults	(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
		tcu::ConstPixelBufferAccess			imageAccess			(mapVkFormat(m_colorFormat), m_framebufferSize.x(), m_framebufferSize.y(), 1, colorBufferAlloc->getHostPtr());

		// Color check for rendered shape. Shape color is yellow.
		if (imageAccess.getPixel(m_framebufferSize.x() / 2, m_framebufferSize.y() / 2) != tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f))
		{
			log << tcu::TestLog::Image("color0", "Rendered image", imageAccess);
			return tcu::TestStatus::fail("Pixel check failed: shape color");
		}

		// Color check for the second render area. Clear color should be green.
		if (m_testShape != TestShape::SHAPE_RECTANGLE)	// In this case the shape has covered the whole render area.
		{
			if (imageAccess.getPixel(m_framebufferSize.x() / 4 + 1, m_framebufferSize.y() / 4 + 1) != tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f))
			{
				log << tcu::TestLog::Image("color0", "Rendered image", imageAccess);
				return tcu::TestStatus::fail("Pixel check failed inside the render area");
			}
		}

		// Color check for possible overflowed multisample pixels outside of the second render area.
		// Clear color after the first beginRenderPass should be red.
		const int							minValue			= m_framebufferSize.y() / 4 - 1;
		const int							maxValue			= m_framebufferSize.y() - m_framebufferSize.y() / 4;

		for (int y = 0; y < m_framebufferSize.y(); y++)
		{
			for (int x = 0; x < m_framebufferSize.x(); x++)
			{
				if (!(x > minValue && y > minValue && x < maxValue && y < maxValue))
				{
					if (imageAccess.getPixel(x, y) != tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f))
					{
						log << tcu::TestLog::Message << "Incorrect color value " << imageAccess.getPixel(x, y) << " at location (" << x << ", " << y << ")" << tcu::TestLog::EndMessage;
						log << tcu::TestLog::Image("color0", "Rendered image", imageAccess);
						return tcu::TestStatus::fail("Pixel check failed outside the render area");
					}
				}
			}
		}
	}

	return tcu::TestStatus::pass("Success");
}

class MultisampleRenderAreaTest : public TestCase
{
public:
						MultisampleRenderAreaTest	(tcu::TestContext&					testCtx,
													 const std::string					name,
													 const PipelineConstructionType		pipelineConstructionType,
													 const deUint32						sampleCount,
													 const tcu::IVec2					framebufferSize,
													 const TestShape					testShape,
													 const VkFormat						colorFormat	= VK_FORMAT_R8G8B8A8_UNORM)
													: TestCase(testCtx,	name, "")
													, m_pipelineConstructionType	(pipelineConstructionType)
													, m_sampleCount					(sampleCount)
													, m_framebufferSize				(framebufferSize)
													, m_testShape					(testShape)
													, m_colorFormat					(colorFormat)
													{}

	void				initPrograms				(SourceCollections&	programCollection) const;
	TestInstance*		createInstance				(Context&			context) const;
	virtual void		checkSupport				(Context&			context) const;

private:

	const PipelineConstructionType	m_pipelineConstructionType;
	const deUint32					m_sampleCount;
	const tcu::IVec2				m_framebufferSize;
	const TestShape					m_testShape;
	const VkFormat					m_colorFormat;
};

void MultisampleRenderAreaTest::initPrograms(SourceCollections& programCollection) const
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 position;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream frg;
		frg << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 fragColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(frg.str());
	}
}

TestInstance* MultisampleRenderAreaTest::createInstance(Context& context) const
{
	return new MultisampleRenderAreaTestInstance(context, m_pipelineConstructionType, m_sampleCount, m_framebufferSize, m_testShape, m_colorFormat);
}

void MultisampleRenderAreaTest::checkSupport(Context& context) const
{
	// Check support for MSAA image format used in the test.
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkPhysicalDevice		physDevice		= context.getPhysicalDevice();

	VkImageFormatProperties		formatProperties;

	vki.getPhysicalDeviceImageFormatProperties	(physDevice, m_colorFormat,
												VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
												VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
												0u, &formatProperties);

	if ((formatProperties.sampleCounts & m_sampleCount) == 0)
		TCU_THROW(NotSupportedError, "Format does not support this number of samples");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

} // anonymous

tcu::TestCaseGroup* createMultisampleResolveRenderpassRenderAreaTests(tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> testGroupResolve(new tcu::TestCaseGroup(testCtx, "resolve", "resolving multisample image tests"));

	de::MovePtr<tcu::TestCaseGroup> testGroupRenderArea(new tcu::TestCaseGroup(testCtx, "renderpass_renderarea", "renderpass render area tests"));

	static const struct
	{
		std::string					shapeName;
		TestShape					testShape;
	} shapes[]	=
	{
		{ "rectangle",		TestShape::SHAPE_RECTANGLE		},
		{ "diamond",		TestShape::SHAPE_DIAMOND		},
		{ "parallelogram",	TestShape::SHAPE_PARALLELOGRAM	}
	};

	static const struct
	{
		std::string		caseName;
		const deUint32	sampleCount;
	} cases[]	=
	{
		{ "samples_2",	2u,	},
		{ "samples_4",	4u,	},
		{ "samples_8",	8u,	},
		{ "samples_16",	16u	},
	};

	for (const auto& testShape : shapes)
	{
		for (const auto& testCase : cases)
		{
			testGroupRenderArea->addChild(new MultisampleRenderAreaTest(testCtx, testShape.shapeName + "_" + testCase.caseName, pipelineConstructionType, testCase.sampleCount, tcu::IVec2(32, 32), testShape.testShape));
		}
	}

	testGroupResolve->addChild(testGroupRenderArea.release());

	return testGroupResolve.release();
}

} // pipeline
} // vkt
