/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2014 The Android Open Source Project
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
 * \brief Scissor tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentOperationsScissorTests.hpp"
#include "vktFragmentOperationsScissorMultiViewportTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

namespace vkt
{
namespace FragmentOperations
{
using namespace vk;
using de::UniquePtr;
using de::MovePtr;
using tcu::Vec4;
using tcu::Vec2;
using tcu::IVec2;
using tcu::IVec4;

namespace
{

//! What primitives will be drawn by the test case.
enum TestPrimitive
{
	TEST_PRIMITIVE_POINTS,			//!< Many points.
	TEST_PRIMITIVE_LINES,			//!< Many short lines.
	TEST_PRIMITIVE_TRIANGLES,		//!< Many small triangles.
	TEST_PRIMITIVE_BIG_LINE,		//!< One line crossing the whole render area.
	TEST_PRIMITIVE_BIG_TRIANGLE,	//!< One triangle covering the whole render area.
};

struct VertexData
{
	Vec4	position;
	Vec4	color;
};

//! Parameters used by the test case.
struct CaseDef
{
	Vec4			renderArea;		//!< (ox, oy, w, h), where origin (0,0) is the top-left corner of the viewport. Width and height are in range [0, 1].
	Vec4			scissorArea;	//!< scissored area (ox, oy, w, h)
	TestPrimitive	primitive;
};

template<typename T>
inline VkDeviceSize sizeInBytes(const std::vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

VkImageCreateInfo makeImageCreateInfo (const VkFormat format, const IVec2& size, VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		(VkImageCreateFlags)0,							// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		format,											// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),			// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return imageParams;
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&		vk,
									   const VkDevice				device,
									   const VkPipelineLayout		pipelineLayout,
									   const VkRenderPass			renderPass,
									   const VkShaderModule			vertexModule,
									   const VkShaderModule			fragmentModule,
									   const IVec2					renderSize,
									   const IVec4					scissorArea,	//!< (ox, oy, w, h)
									   const VkPrimitiveTopology	topology)
{
	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,								// uint32_t				binding;
		sizeof(VertexData),				// uint32_t				stride;
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,								// uint32_t		location;
			0u,								// uint32_t		binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat		format;
			0u,								// uint32_t		offset;
		},
		{
			1u,								// uint32_t		location;
			0u,								// uint32_t		binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat		format;
			sizeof(Vec4),					// uint32_t		offset;
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags;
		1u,															// uint32_t                                    vertexBindingDescriptionCount;
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),		// uint32_t                                    vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,							// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkRect2D			scissor		=
	{
		makeOffset2D(scissorArea.x(), scissorArea.y()),
		makeExtent2D(scissorArea.z(), scissorArea.w())
	};

	const std::vector<VkViewport>	viewports	(1, makeViewport(renderSize));
	const std::vector<VkRect2D>		scissors	(1, scissor);

	return vk::makeGraphicsPipeline(vk,						// const DeviceInterface&                        vk
									device,					// const VkDevice                                device
									pipelineLayout,			// const VkPipelineLayout                        pipelineLayout
									vertexModule,			// const VkShaderModule                          vertexShaderModule
									DE_NULL,				// const VkShaderModule                          tessellationControlModule
									DE_NULL,				// const VkShaderModule                          tessellationEvalModule
									DE_NULL,				// const VkShaderModule                          geometryShaderModule
									fragmentModule,			// const VkShaderModule                          fragmentShaderModule
									renderPass,				// const VkRenderPass                            renderPass
									viewports,				// const std::vector<VkViewport>&                viewports
									scissors,				// const std::vector<VkRect2D>&                  scissors
									topology,				// const VkPrimitiveTopology                     topology
									0u,						// const deUint32                                subpass
									0u,						// const deUint32                                patchControlPoints
									&vertexInputStateInfo);	// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
}

inline VertexData makeVertex (const float x, const float y, const Vec4& color)
{
	const VertexData data = { Vec4(x, y, 0.0f, 1.0f), color };
	return data;
}

std::vector<VertexData> genVertices (const TestPrimitive primitive, const Vec4& renderArea, const Vec4& primitiveColor)
{
	std::vector<VertexData> vertices;
	de::Random				rng			(1234);

	const float	x0		= 2.0f * renderArea.x() - 1.0f;
	const float y0		= 2.0f * renderArea.y() - 1.0f;
	const float	rx		= 2.0f * renderArea.z();
	const float	ry		= 2.0f * renderArea.w();
	const float	size	= 0.2f;

	switch (primitive)
	{
		case TEST_PRIMITIVE_POINTS:
			for (int i = 0; i < 50; ++i)
			{
				const float x = x0 + rng.getFloat(0.0f, rx);
				const float y = y0 + rng.getFloat(0.0f, ry);
				vertices.push_back(makeVertex(x, y, primitiveColor));
			}
			break;

		case TEST_PRIMITIVE_LINES:
			for (int i = 0; i < 30; ++i)
			{
				const float x = x0 + rng.getFloat(0.0f, rx - size);
				const float y = y0 + rng.getFloat(0.0f, ry - size);
				vertices.push_back(makeVertex(x,        y,        primitiveColor));
				vertices.push_back(makeVertex(x + size, y + size, primitiveColor));
			}
			break;

		case TEST_PRIMITIVE_TRIANGLES:
			for (int i = 0; i < 20; ++i)
			{
				const float x = x0 + rng.getFloat(0.0f, rx - size);
				const float y = y0 + rng.getFloat(0.0f, ry - size);
				vertices.push_back(makeVertex(x,             y,        primitiveColor));
				vertices.push_back(makeVertex(x + size/2.0f, y + size, primitiveColor));
				vertices.push_back(makeVertex(x + size,      y,        primitiveColor));
			}
			break;

		case TEST_PRIMITIVE_BIG_LINE:
			vertices.push_back(makeVertex(x0,      y0,      primitiveColor));
			vertices.push_back(makeVertex(x0 + rx, y0 + ry, primitiveColor));
			break;

		case TEST_PRIMITIVE_BIG_TRIANGLE:
			vertices.push_back(makeVertex(x0,           y0,      primitiveColor));
			vertices.push_back(makeVertex(x0 + rx/2.0f, y0 + ry, primitiveColor));
			vertices.push_back(makeVertex(x0 + rx,      y0,      primitiveColor));
			break;
	}

	return vertices;
}

VkPrimitiveTopology	getTopology (const TestPrimitive primitive)
{
	switch (primitive)
	{
		case TEST_PRIMITIVE_POINTS:			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

		case TEST_PRIMITIVE_LINES:
		case TEST_PRIMITIVE_BIG_LINE:		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

		case TEST_PRIMITIVE_TRIANGLES:
		case TEST_PRIMITIVE_BIG_TRIANGLE:	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		default:
			DE_ASSERT(0);
			return VK_PRIMITIVE_TOPOLOGY_LAST;
	}
}

//! Transform from normalized coords to framebuffer space.
inline IVec4 getAreaRect (const Vec4& area, const int width, const int height)
{
	return IVec4(static_cast<deInt32>(static_cast<float>(width)  * area.x()),
				 static_cast<deInt32>(static_cast<float>(height) * area.y()),
				 static_cast<deInt32>(static_cast<float>(width)  * area.z()),
				 static_cast<deInt32>(static_cast<float>(height) * area.w()));
}

void applyScissor (tcu::PixelBufferAccess imageAccess, const Vec4& floatScissorArea, const Vec4& clearColor)
{
	const IVec4	scissorRect	(getAreaRect(floatScissorArea, imageAccess.getWidth(), imageAccess.getHeight()));
	const int	sx0			= scissorRect.x();
	const int	sx1			= scissorRect.x() + scissorRect.z();
	const int	sy0			= scissorRect.y();
	const int	sy1			= scissorRect.y() + scissorRect.w();

	for (int y = 0; y < imageAccess.getHeight(); ++y)
	for (int x = 0; x < imageAccess.getWidth(); ++x)
	{
		// Fragments outside fail the scissor test.
		if (x < sx0 || x >= sx1 || y < sy0 || y >= sy1)
			imageAccess.setPixel(clearColor, x, y);
	}
}

void initPrograms (SourceCollections& programCollection, const CaseDef caseDef)
{
	DE_UNREF(caseDef);

	// Vertex shader
	{
		const bool usePointSize = (caseDef.primitive == TEST_PRIMITIVE_POINTS);

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "layout(location = 1) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4  gl_Position;\n"
			<< (usePointSize ? "    float gl_PointSize;\n" : "")
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position  = in_position;\n"
			<< (usePointSize ? "    gl_PointSize = 1.0;\n" : "")
			<< "    o_color      = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    o_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

class ScissorRenderer
{
public:
	ScissorRenderer (Context& context, const CaseDef caseDef, const IVec2& renderSize, const VkFormat colorFormat, const Vec4& primitiveColor, const Vec4& clearColor)
		: m_renderSize				(renderSize)
		, m_colorFormat				(colorFormat)
		, m_colorSubresourceRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u))
		, m_primitiveColor			(primitiveColor)
		, m_clearColor				(clearColor)
		, m_vertices				(genVertices(caseDef.primitive, caseDef.renderArea, m_primitiveColor))
		, m_vertexBufferSize		(sizeInBytes(m_vertices))
		, m_topology				(getTopology(caseDef.primitive))
	{
		const DeviceInterface&		vk					= context.getDeviceInterface();
		const VkDevice				device				= context.getDevice();
		const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
		Allocator&					allocator			= context.getDefaultAllocator();

		m_colorImage			= makeImage(vk, device, makeImageCreateInfo(m_colorFormat, m_renderSize, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
		m_colorImageAlloc		= bindImage(vk, device, allocator, *m_colorImage, MemoryRequirement::Any);
		m_colorAttachment		= makeImageView(vk, device, *m_colorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, m_colorSubresourceRange);

		m_vertexBuffer			= makeBuffer(vk, device, m_vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		m_vertexBufferAlloc		= bindBuffer(vk, device, allocator, *m_vertexBuffer, MemoryRequirement::HostVisible);

		{
			deMemcpy(m_vertexBufferAlloc->getHostPtr(), &m_vertices[0], static_cast<std::size_t>(m_vertexBufferSize));
			flushAlloc(vk, device, *m_vertexBufferAlloc);
		}

		m_vertexModule				= createShaderModule	(vk, device, context.getBinaryCollection().get("vert"), 0u);
		m_fragmentModule			= createShaderModule	(vk, device, context.getBinaryCollection().get("frag"), 0u);
		m_renderPass				= makeRenderPass		(vk, device, m_colorFormat);
		m_framebuffer				= makeFramebuffer		(vk, device, *m_renderPass, m_colorAttachment.get(),
															 static_cast<deUint32>(m_renderSize.x()),  static_cast<deUint32>(m_renderSize.y()));
		m_pipelineLayout			= makePipelineLayout	(vk, device);
		m_cmdPool					= createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
		m_cmdBuffer					= allocateCommandBuffer	(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	}

	void draw (Context& context, const Vec4& scissorAreaFloat, const VkBuffer colorBuffer) const
	{
		const DeviceInterface&		vk			= context.getDeviceInterface();
		const VkDevice				device		= context.getDevice();
		const VkQueue				queue		= context.getUniversalQueue();

		// New pipeline, because we're modifying scissor (we don't use dynamic state).
		const Unique<VkPipeline>	pipeline	(makeGraphicsPipeline(vk, device, *m_pipelineLayout, *m_renderPass, *m_vertexModule, *m_fragmentModule,
												 m_renderSize, getAreaRect(scissorAreaFloat, m_renderSize.x(), m_renderSize.y()), m_topology));

		beginCommandBuffer(vk, *m_cmdBuffer);

		beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), m_clearColor);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		{
			const VkDeviceSize vertexBufferOffset = 0ull;
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
		}

		vk.cmdDraw(*m_cmdBuffer, static_cast<deUint32>(m_vertices.size()), 1u, 0u, 0u);
		endRenderPass(vk, *m_cmdBuffer);

		copyImageToBuffer(vk, *m_cmdBuffer, *m_colorImage, colorBuffer, m_renderSize);

		endCommandBuffer(vk, *m_cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);
		context.resetCommandPoolForVKSC(device, *m_cmdPool);
	}

private:
	const IVec2						m_renderSize;
	const VkFormat					m_colorFormat;
	const VkImageSubresourceRange	m_colorSubresourceRange;
	const Vec4						m_primitiveColor;
	const Vec4						m_clearColor;
	const std::vector<VertexData>	m_vertices;
	const VkDeviceSize				m_vertexBufferSize;
	const VkPrimitiveTopology		m_topology;

	Move<VkImage>					m_colorImage;
	MovePtr<Allocation>				m_colorImageAlloc;
	Move<VkImageView>				m_colorAttachment;
	Move<VkBuffer>					m_vertexBuffer;
	MovePtr<Allocation>				m_vertexBufferAlloc;
	Move<VkShaderModule>			m_vertexModule;
	Move<VkShaderModule>			m_fragmentModule;
	Move<VkRenderPass>				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;

	// "deleted"
						ScissorRenderer	(const ScissorRenderer&);
	ScissorRenderer&	operator=		(const ScissorRenderer&);
};

tcu::TestStatus test (Context& context, const CaseDef caseDef)
{
	const DeviceInterface&			vk							= context.getDeviceInterface();
	const VkDevice					device						= context.getDevice();
	Allocator&						allocator					= context.getDefaultAllocator();

	const IVec2						renderSize					(128, 128);
	const VkFormat					colorFormat					= VK_FORMAT_R8G8B8A8_UNORM;
	const Vec4						scissorFullArea				(0.0f, 0.0f, 1.0f, 1.0f);
	const Vec4						primitiveColor				(1.0f, 1.0f, 1.0f, 1.0f);
	const Vec4						clearColor					(0.5f, 0.5f, 1.0f, 1.0f);

	const VkDeviceSize				colorBufferSize				= renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
	const Unique<VkBuffer>			colorBufferFull				(makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferFullAlloc		(bindBuffer(vk, device, allocator, *colorBufferFull, MemoryRequirement::HostVisible));

	const Unique<VkBuffer>			colorBufferScissored		(makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferScissoredAlloc	(bindBuffer(vk, device, allocator, *colorBufferScissored, MemoryRequirement::HostVisible));

	zeroBuffer(vk, device, *colorBufferFullAlloc, colorBufferSize);
	zeroBuffer(vk, device, *colorBufferScissoredAlloc, colorBufferSize);

	// Draw
	{
		const ScissorRenderer renderer (context, caseDef, renderSize, colorFormat, primitiveColor, clearColor);

		renderer.draw(context, scissorFullArea, *colorBufferFull);
		renderer.draw(context, caseDef.scissorArea, *colorBufferScissored);
	}

	// Log image
	{
		invalidateAlloc(vk, device, *colorBufferFullAlloc);
		invalidateAlloc(vk, device, *colorBufferScissoredAlloc);

		const tcu::ConstPixelBufferAccess	resultImage		(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1u, colorBufferScissoredAlloc->getHostPtr());
		tcu::PixelBufferAccess				referenceImage	(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1u, colorBufferFullAlloc->getHostPtr());

		// Apply scissor to the full image, so we can compare it with the result image.
		applyScissor (referenceImage, caseDef.scissorArea, clearColor);

		// Images should now match.
		if (!tcu::floatThresholdCompare(context.getTestContext().getLog(), "color", "Image compare", referenceImage, resultImage, Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Rendered image is not correct");
	}

	return tcu::TestStatus::pass("OK");
}

//! \note The ES 2.0 scissoring tests included color/depth/stencil clear cases, but these operations are not affected by scissor test in Vulkan.
//!       Scissor is part of the pipeline state and pipeline only affects the drawing commands.
void createTestsInGroup (tcu::TestCaseGroup* scissorGroup)
{
	tcu::TestContext& testCtx = scissorGroup->getTestContext();

	struct TestSpec
	{
		const char*		name;
		const char*		description;
		CaseDef			caseDef;
	};

	const Vec4	areaFull			(0.0f, 0.0f, 1.0f, 1.0f);
	const Vec4	areaCropped			(0.2f, 0.2f, 0.6f, 0.6f);
	const Vec4	areaCroppedMore		(0.4f, 0.4f, 0.2f, 0.2f);
	const Vec4	areaLeftHalf		(0.0f, 0.0f, 0.5f, 1.0f);
	const Vec4	areaRightHalf		(0.5f, 0.0f, 0.5f, 1.0f);

	// Points
	{
		MovePtr<tcu::TestCaseGroup> primitiveGroup (new tcu::TestCaseGroup(testCtx, "points", ""));

		const TestSpec	cases[] =
		{
			{ "inside",				"Points fully inside the scissor area",		{ areaFull,		areaFull,		TEST_PRIMITIVE_POINTS } },
			{ "partially_inside",	"Points partially inside the scissor area",	{ areaFull,		areaCropped,	TEST_PRIMITIVE_POINTS } },
			{ "outside",			"Points fully outside the scissor area",	{ areaLeftHalf,	areaRightHalf,	TEST_PRIMITIVE_POINTS } },
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
			addFunctionCaseWithPrograms(primitiveGroup.get(), cases[i].name, cases[i].description, initPrograms, test, cases[i].caseDef);

		scissorGroup->addChild(primitiveGroup.release());
	}

	// Lines
	{
		MovePtr<tcu::TestCaseGroup> primitiveGroup (new tcu::TestCaseGroup(testCtx, "lines", ""));

		const TestSpec	cases[] =
		{
			{ "inside",				"Lines fully inside the scissor area",		{ areaFull,		areaFull,			TEST_PRIMITIVE_LINES	} },
			{ "partially_inside",	"Lines partially inside the scissor area",	{ areaFull,		areaCropped,		TEST_PRIMITIVE_LINES	} },
			{ "outside",			"Lines fully outside the scissor area",		{ areaLeftHalf,	areaRightHalf,		TEST_PRIMITIVE_LINES	} },
			{ "crossing",			"A line crossing the scissor area",			{ areaFull,		areaCroppedMore,	TEST_PRIMITIVE_BIG_LINE	} },
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
			addFunctionCaseWithPrograms(primitiveGroup.get(), cases[i].name, cases[i].description, initPrograms, test, cases[i].caseDef);

		scissorGroup->addChild(primitiveGroup.release());
	}

	// Triangles
	{
		MovePtr<tcu::TestCaseGroup> primitiveGroup (new tcu::TestCaseGroup(testCtx, "triangles", ""));

		const TestSpec	cases[] =
		{
			{ "inside",				"Triangles fully inside the scissor area",		{ areaFull,		areaFull,			TEST_PRIMITIVE_TRIANGLES	} },
			{ "partially_inside",	"Triangles partially inside the scissor area",	{ areaFull,		areaCropped,		TEST_PRIMITIVE_TRIANGLES	} },
			{ "outside",			"Triangles fully outside the scissor area",		{ areaLeftHalf,	areaRightHalf,		TEST_PRIMITIVE_TRIANGLES	} },
			{ "crossing",			"A triangle crossing the scissor area",			{ areaFull,		areaCroppedMore,	TEST_PRIMITIVE_BIG_TRIANGLE	} },
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
			addFunctionCaseWithPrograms(primitiveGroup.get(), cases[i].name, cases[i].description, initPrograms, test, cases[i].caseDef);

		scissorGroup->addChild(primitiveGroup.release());
	}

	// Mulit-viewport scissor
	{
		scissorGroup->addChild(createScissorMultiViewportTests(testCtx));
	}
}

} // anonymous

tcu::TestCaseGroup* createScissorTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "scissor", "Scissor tests", createTestsInGroup);
}

} // FragmentOperations
} // vkt
