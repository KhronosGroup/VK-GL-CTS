/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Test for VK_EXT_multi_draw
 *//*--------------------------------------------------------------------*/

#include "vktDrawMultiExtTests.hpp"

#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTexture.hpp"
#include "tcuMaybe.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deMath.h"
#include "deRandom.hpp"

#include <vector>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <limits>

using namespace vk;

namespace vkt
{
namespace Draw
{

namespace
{

// Normal or indexed draws.
enum class DrawType { NORMAL = 0, INDEXED };

// How to apply the vertex offset in indexed draws.
enum class VertexOffsetType
{
	MIXED = 0,			// Do not use pVertexOffset and mix values in struct-indicated offsets.
	CONSTANT_RANDOM,	// Use a constant value for pVertexOffset and fill offset struct members with random values.
	CONSTANT_PACK,		// Use a constant value for pVertexOffset and a stride that removes the vertex offset member in structs.
};

// Triangle mesh type.
enum class MeshType { MOSAIC = 0, OVERLAPPING };

// Vertex offset parameters.
struct VertexOffsetParams
{
	VertexOffsetType	offsetType;
	deUint32			offset;
};

// Test parameters.
struct TestParams
{
	MeshType						meshType;
	DrawType						drawType;
	deUint32						drawCount;
	deUint32						instanceCount;
	deUint32						firstInstance;
	deUint32						stride;
	tcu::Maybe<VertexOffsetParams>	vertexOffset;	// Only used for indexed draws.
	deUint32						seed;

	deUint32 maxInstanceIndex () const
	{
		if (instanceCount == 0u)
			return 0u;
		return (firstInstance + instanceCount - 1u);
	}
};

// For the color attachment. Must match what the fragment shader expects.
VkFormat getColorFormat ()
{
	return VK_FORMAT_R8G8B8A8_UINT;
}

// Compatible with getColorFormat() but better when used with the image logging facilities.
VkFormat getVerificationFormat ()
{
	return VK_FORMAT_R8G8B8A8_UNORM;
}

// Find a suitable format for the depth/stencil buffer.
VkFormat chooseDepthStencilFormat (const InstanceInterface& vki, VkPhysicalDevice physDev)
{
	// The spec mandates support for one of these two formats.
	const VkFormat candidates[] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };

	for (const auto& format : candidates)
	{
		const auto properties = getPhysicalDeviceFormatProperties(vki, physDev, format);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u)
			return format;
	}

	TCU_FAIL("No suitable depth/stencil format found");
	return VK_FORMAT_UNDEFINED; // Unreachable.
}

// Format used when verifying the stencil aspect.
VkFormat getStencilVerificationFormat ()
{
	return VK_FORMAT_S8_UINT;
}

deUint32 getTriangleCount ()
{
	return 1024u;	// This matches the minumum allowed limit for maxMultiDrawCount, so we can submit a single triangle per draw call.
}

// Base class for creating triangles.
class TriangleGenerator
{
public:
	// Append a new triangle for ID (x, y).
	virtual void appendTriangle(deUint32 x, deUint32 y, std::vector<tcu::Vec4>& vertices) = 0;
};

// Class that helps creating triangle vertices for each framebuffer pixel, forming a mosaic of triangles.
class TriangleMosaicGenerator : public TriangleGenerator
{
private:
	// Normalized width and height taking into account the framebuffer's width and height are two units (from -1 to 1).
	float	m_pixelWidth;
	float	m_pixelHeight;

	float	m_deltaX;
	float	m_deltaY;

public:
	TriangleMosaicGenerator (deUint32 width, deUint32 height)
		: m_pixelWidth	(2.0f / static_cast<float>(width))
		, m_pixelHeight	(2.0f / static_cast<float>(height))
		, m_deltaX		(m_pixelWidth * 0.25f)
		, m_deltaY		(m_pixelHeight * 0.25f)
	{}

	// Creates a triangle for framebuffer pixel (x, y) around its center. Appends the triangle vertices to the given list.
	void appendTriangle(deUint32 x, deUint32 y, std::vector<tcu::Vec4>& vertices) override
	{
		// Pixel center.
		const float coordX	= (static_cast<float>(x) + 0.5f) * m_pixelWidth - 1.0f;
		const float coordY	= (static_cast<float>(y) + 0.5f) * m_pixelHeight - 1.0f;

		// Triangle around it.
		const float topY	= coordY - m_deltaY;
		const float bottomY	= coordY + m_deltaY;

		const float leftX	= coordX - m_deltaX;
		const float rightX	= coordX + m_deltaX;

		// Note: clockwise.
		vertices.emplace_back(leftX,	bottomY,	0.0f, 1.0f);
		vertices.emplace_back(coordX,	topY,		0.0f, 1.0f);
		vertices.emplace_back(rightX,	bottomY,	0.0f, 1.0f);
	}
};

// Class that helps create full-screen triangles that overlap each other.
// This generator will generate width*height full-screen triangles with decreasing depth from 0.75 to 0.25.
class TriangleOverlapGenerator : public TriangleGenerator
{
private:
	// Normalized width and height taking into account the framebuffer's width and height are two units (from -1 to 1).
	deUint32	m_width;
	deUint32	m_totalPixels;
	float		m_depthStep;

	static constexpr float kMinDepth	= 0.25f;
	static constexpr float kMaxDepth	= 0.75f;
	static constexpr float kDepthRange	= kMaxDepth - kMinDepth;

public:
	TriangleOverlapGenerator (deUint32 width, deUint32 height)
		: m_width		(width)
		, m_totalPixels (width * height)
		, m_depthStep	(kDepthRange / static_cast<float>(m_totalPixels))
	{}

	// Creates full-screen triangle with 2D id (x, y) and decreasing depth with increasing ids.
	void appendTriangle(deUint32 x, deUint32 y, std::vector<tcu::Vec4>& vertices) override
	{
		const auto pixelId	= static_cast<float>(y * m_width + x);
		const auto depth	= kMaxDepth - m_depthStep * pixelId;

		// Note: clockwise.
		vertices.emplace_back(-1.0f,	-1.0f,	depth, 1.0f);
		vertices.emplace_back(4.0f,		-1.0f,	depth, 1.0f);
		vertices.emplace_back(-1.0f,	4.0f,	depth, 1.0f);
	}
};

// Class that helps creating a suitable draw info vector.
class DrawInfoPacker
{
private:
	DrawType						m_drawType;
	tcu::Maybe<VertexOffsetType>	m_offsetType;	// Offset type when m_drawType is DrawType::INDEXED.
	deUint32						m_stride;		// Desired stride. Must be zero or at least as big as the needed VkMultiDraw*InfoExt.
	deUint32						m_extraBytes;	// Used to match the desired stride.
	de::Random						m_random;		// Used to generate random offsets.
	deUint32						m_infoCount;	// How many infos have we appended so far?
	std::vector<deUint8>			m_dataVec;		// Data vector in generic form.

	// Are draws indexed and using the offset member of VkMultiDrawIndexedInfoEXT?
	static bool indexedWithOffset (DrawType drawType, const tcu::Maybe<VertexOffsetType>& offsetType)
	{
		return (drawType == DrawType::INDEXED && *offsetType != VertexOffsetType::CONSTANT_PACK);
	}

	// Size in bytes for the base structure used with the given draw type.
	static deUint32 baseSize (DrawType drawType, const tcu::Maybe<VertexOffsetType>& offsetType)
	{
		return static_cast<deUint32>(indexedWithOffset(drawType, offsetType) ? sizeof(VkMultiDrawIndexedInfoEXT) : sizeof(VkMultiDrawInfoEXT));
	}

	// Number of extra bytes per entry according to the given stride.
	static deUint32 calcExtraBytes (DrawType drawType, const tcu::Maybe<VertexOffsetType>& offsetType, deUint32 stride)
	{
		// Stride 0 is a special allowed case.
		if (stride == 0u)
			return 0u;

		const auto minStride = baseSize(drawType, offsetType);
		DE_ASSERT(stride >= minStride);
		return (stride - minStride);
	}

	// Entry size in bytes taking into account the number of extra bytes due to stride.
	deUint32 entrySize () const
	{
		return baseSize(m_drawType, m_offsetType) + m_extraBytes;
	}

public:
	DrawInfoPacker	(DrawType drawType, const tcu::Maybe<VertexOffsetType>& offsetType, deUint32 stride, deUint32 estimatedInfoCount, deUint32 seed)
		: m_drawType	(drawType)
		, m_offsetType	(offsetType)
		, m_stride		(stride)
		, m_extraBytes	(calcExtraBytes(drawType, offsetType, stride))
		, m_random		(seed)
		, m_infoCount	(0u)
		, m_dataVec		()
	{
		// estimatedInfoCount is used to avoid excessive reallocation.
		if (estimatedInfoCount > 0u)
			m_dataVec.reserve(estimatedInfoCount * entrySize());
	}

	void addDrawInfo (deUint32 first, deUint32 count, deInt32 offset)
	{
		std::vector<deUint8> entry(entrySize(), 0);

		if (indexedWithOffset(m_drawType, m_offsetType))
		{
			const auto usedOffset = ((*m_offsetType == VertexOffsetType::CONSTANT_RANDOM) ? m_random.getInt32() : offset);
			const VkMultiDrawIndexedInfoEXT info = { first, count, usedOffset };
			deMemcpy(entry.data(), &info, sizeof(info));
		}
		else
		{
			const VkMultiDrawInfoEXT info = { first, count };
			deMemcpy(entry.data(), &info, sizeof(info));
		}

		std::copy(begin(entry), end(entry), std::back_inserter(m_dataVec));
		++m_infoCount;
	}

	deUint32 drawInfoCount () const
	{
		return m_infoCount;
	}

	const void* drawInfoData () const
	{
		return m_dataVec.data();
	}

	deUint32 stride () const
	{
		return m_stride;
	}
};

class MultiDrawTest : public vkt::TestCase
{
public:
					MultiDrawTest	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual			~MultiDrawTest	(void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

private:
	TestParams		m_params;
};

class MultiDrawInstance : public vkt::TestInstance
{
public:
						MultiDrawInstance	(Context& context, const TestParams& params);
	virtual				~MultiDrawInstance	(void) {}

	tcu::TestStatus		iterate				(void) override;

private:
	TestParams			m_params;
};

MultiDrawTest::MultiDrawTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

TestInstance* MultiDrawTest::createInstance (Context& context) const
{
	return new MultiDrawInstance(context, m_params);
}

void MultiDrawTest::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_multi_draw");
}

void MultiDrawTest::initPrograms (vk::SourceCollections& programCollection) const
{
	// The general idea behind these tests is to have a 32x32 framebuffer with 1024 pixels and 1024 triangles to draw.
	//
	// When using a mosaic mesh, the tests will generally draw a single triangle around the center of each of these pixels. When
	// using an overlapping mesh, each single triangle will cover the whole framebuffer using a different depth value, and the depth
	// test will be enabled.
	//
	// The color of each triangle will depend on the instance index and the draw index. This way, it's possible to draw those 1024
	// triangles with a single draw call or to draw each triangle with a separate draw call, with up to 1024 draw calls.
	// Combinations in between are possible.
	//
	// With overlapping meshes, the resulting color buffer will be uniform in color. With mosaic meshes, it depends on the submitted
	// draw count. In some cases, all pixels will be slightly different in color.
	//
	// The color buffer will be cleared to transparent black when beginning the render pass, and in some special cases some or all
	// pixels will preserve that clear color because they will not be drawn into. This happens, for example, if the instance count
	// or draw count is zero and in some cases of meshed geometry with stride zero.
	//
	// The output color for each pixel will:
	// - Have the draw index split into the R and G components.
	// - Have the instance index I stored into the B component as 255-I.
	//
	// In addition, the tests will use a depth/stencil buffer. The stencil buffer will be cleared to zero and the depth buffer to an
	// appropriate initial value (0.0 or 1.0, depending on triangle order). The stencil component will be increased with each draw
	// on each pixel. This will allow us to verify that not only the last draw for the last instance has set the proper color, but
	// that all draw operations have taken place.

	// Make sure the blue channel can be calculated without issues.
	DE_ASSERT(m_params.maxInstanceIndex() <= 255u);

	std::ostringstream vert;
	vert
		<< "#version 460\n"
		<< "\n"
		<< "layout (location=0) in vec4 inPos;\n"
		<< "layout (location=0) out uvec4 outColor;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "    gl_Position = inPos;\n"
		<< "    const uint uDrawIndex = uint(gl_DrawID);\n"
		<< "    outColor.r = ((uDrawIndex >> 8u) & 0xFFu);\n"
		<< "    outColor.g = ((uDrawIndex      ) & 0xFFu);\n"
		<< "    outColor.b = 255u - uint(gl_InstanceIndex);\n"
		<< "    outColor.a = 255u;\n"
		<< "}\n"
		;

	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "\n"
		<< "layout (location=0) flat in uvec4 inColor;\n"
		<< "layout (location=0) out uvec4 outColor;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    outColor = inColor;\n"
		<< "}\n"
		;

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

MultiDrawInstance::MultiDrawInstance (Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

void appendPaddingVertices (std::vector<tcu::Vec4>& vertices, deUint32 count)
{
	for (deUint32 i = 0u; i < count; ++i)
		vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f);
}

tcu::TestStatus MultiDrawInstance::iterate (void)
{
	const auto&	vki				= m_context.getInstanceInterface();
	const auto	physDev			= m_context.getPhysicalDevice();
	const auto&	vkd				= m_context.getDeviceInterface();
	const auto	device			= m_context.getDevice();
	auto&		alloc			= m_context.getDefaultAllocator();
	const auto	queue			= m_context.getUniversalQueue();
	const auto	qIndex			= m_context.getUniversalQueueFamilyIndex();

	const auto	colorFormat		= getColorFormat();
	const auto	dsFormat		= chooseDepthStencilFormat(vki, physDev);
	const auto	tcuColorFormat	= mapVkFormat(colorFormat);
	const auto	triangleCount	= getTriangleCount();
	const auto	imageDim		= static_cast<deUint32>(deSqrt(static_cast<double>(triangleCount)));
	const auto	imageExtent		= makeExtent3D(imageDim, imageDim, 1u);
	const auto	colorUsage		= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	const auto	dsUsage			= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	const auto	pixelCount		= imageExtent.width * imageExtent.height;
	const auto	vertexCount		= pixelCount * 3u; // Triangle list.
	const auto	isIndexed		= (m_params.drawType == DrawType::INDEXED);
	const auto	isMixedMode		= (isIndexed && m_params.vertexOffset && m_params.vertexOffset->offsetType == VertexOffsetType::MIXED);
	const auto	extraVertices	= (m_params.vertexOffset ? m_params.vertexOffset->offset : 0u);
	const auto	isMosaic		= (m_params.meshType == MeshType::MOSAIC);

	// Make sure we're providing a vertex offset for indexed cases.
	DE_ASSERT(!isIndexed || static_cast<bool>(m_params.vertexOffset));

	// Make sure overlapping draws use a single instance.
	DE_ASSERT(isMosaic || m_params.instanceCount <= 1u);

	// Color buffer.
	const VkImageCreateInfo imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		imageExtent,							//	VkExtent3D				extent;
		1u,										//	deUint32				mipLevels;
		1u,										//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	deUint32				queueFamilyIndexCount;
		nullptr,								//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	ImageWithMemory	colorBuffer				(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any);
	const auto		colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorBufferView			= makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange);

	// Depth/stencil buffer.
	const VkImageCreateInfo dsCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		dsFormat,								//	VkFormat				format;
		imageExtent,							//	VkExtent3D				extent;
		1u,										//	deUint32				mipLevels;
		1u,										//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		dsUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	deUint32				queueFamilyIndexCount;
		nullptr,								//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	ImageWithMemory dsBuffer			(vkd, device, alloc, dsCreateInfo, MemoryRequirement::Any);
	const auto		dsSubresourceRange	= makeImageSubresourceRange((VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);
	const auto		dsBufferView		= makeImageView(vkd, device, dsBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSubresourceRange);

	// Buffer to read color attachment.
	const auto outputBufferSize = pixelCount * static_cast<VkDeviceSize>(tcu::getPixelSize(tcuColorFormat));
	const auto bufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory outputBuffer (vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible);

	// Buffer to read depth/stencil attachment. Note: this supposes we'll only copy the stencil aspect. See below.
	const auto			tcuStencilFmt			= mapVkFormat(getStencilVerificationFormat());
	const auto			stencilOutBufferSize	= pixelCount * static_cast<VkDeviceSize>(tcu::getPixelSize(tcuStencilFmt));
	const auto			stencilOutCreateInfo	= makeBufferCreateInfo(stencilOutBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory	stencilOutBuffer		(vkd, device, alloc, stencilOutCreateInfo, MemoryRequirement::HostVisible);

	// Shaders.
	const auto vertModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto fragModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

	DescriptorSetLayoutBuilder	layoutBuilder;
	const auto					descriptorSetLayout	= layoutBuilder.build(vkd, device);
	const auto					pipelineLayout		= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, colorFormat, dsFormat);

	// Framebuffer.
	const std::vector<VkImageView> attachments { colorBufferView.get(), dsBufferView.get() };
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), static_cast<deUint32>(attachments.size()), de::dataOrNull(attachments), imageExtent.width, imageExtent.height);

	// Viewports and scissors.
	const auto						viewport	= makeViewport(imageExtent);
	const std::vector<VkViewport>	viewports	(1u, viewport);
	const auto						scissor		= makeRect2D(imageExtent);
	const std::vector<VkRect2D>		scissors	(1u, scissor);

	// Indexed draws will have triangle vertices in reverse order. See index buffer creation below.
	const auto										frontFace			= (isIndexed ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE);
	const VkPipelineRasterizationStateCreateInfo	rasterizationInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,													//	VkBool32								depthClampEnable;
		VK_FALSE,													//	VkBool32								rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										//	VkPolygonMode							polygonMode;
		VK_CULL_MODE_BACK_BIT,										//	VkCullModeFlags							cullMode;
		frontFace,													//	VkFrontFace								frontFace;
		VK_FALSE,													//	VkBool32								depthBiasEnable;
		0.0f,														//	float									depthBiasConstantFactor;
		0.0f,														//	float									depthBiasClamp;
		0.0f,														//	float									depthBiasSlopeFactor;
		1.0f,														//	float									lineWidth;
	};

	const auto frontStencilState	= makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);
	const auto backStencilState		= makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0xFFu, 0xFFu, 0u);
	const auto depthTestEnable		= (isMosaic ? VK_FALSE : VK_TRUE);
	const auto depthWriteEnable		= depthTestEnable;
	const auto depthCompareOp		= (isMosaic ? VK_COMPARE_OP_ALWAYS : (isIndexed ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS));

	const VkPipelineDepthStencilStateCreateInfo depthStencilInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineDepthStencilStateCreateFlags	flags;
		depthTestEnable,												//	VkBool32								depthTestEnable;
		depthWriteEnable,												//	VkBool32								depthWriteEnable;
		depthCompareOp,													//	VkCompareOp								depthCompareOp;
		VK_FALSE,														//	VkBool32								depthBoundsTestEnable;
		VK_TRUE,														//	VkBool32								stencilTestEnable;
		frontStencilState,												//	VkStencilOpState						front;
		backStencilState,												//	VkStencilOpState						back;
		0.0f,															//	float									minDepthBounds;
		1.0f,															//	float									maxDepthBounds;
	};

	// Pipeline.
	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		vertModule.get(), DE_NULL, DE_NULL, DE_NULL, fragModule.get(),
		renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u/*subpass*/, 0u/*patchControlPoints*/,
		nullptr/*vertexInputStateCreateInfo*/, &rasterizationInfo, nullptr/*multisampleStateCreateInfo*/, &depthStencilInfo);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Create vertex buffer.
	std::vector<tcu::Vec4> triangleVertices;
	triangleVertices.reserve(vertexCount + extraVertices);

	// Vertex count per draw call.
	const bool atLeastOneDraw	= (m_params.drawCount > 0u);
	const bool moreThanOneDraw	= (m_params.drawCount > 1u);
	const auto trianglesPerDraw	= (atLeastOneDraw ? pixelCount / m_params.drawCount : 0u);
	const auto verticesPerDraw	= trianglesPerDraw * 3u;

	if (atLeastOneDraw)
		DE_ASSERT(pixelCount % m_params.drawCount == 0u);

	{
		using TriangleGeneratorPtr = de::MovePtr<TriangleGenerator>;
		TriangleGeneratorPtr triangleGen;

		if (m_params.meshType == MeshType::MOSAIC)
			triangleGen = TriangleGeneratorPtr(new TriangleMosaicGenerator(imageExtent.width, imageExtent.height));
		else if (m_params.meshType == MeshType::OVERLAPPING)
			triangleGen = TriangleGeneratorPtr(new TriangleOverlapGenerator(imageExtent.width, imageExtent.height));
		else
			DE_ASSERT(false);

		// When applying a vertex offset in nonmixed modes, there will be a few extra vertices at the start of the vertex buffer.
		if (isIndexed && !isMixedMode)
			appendPaddingVertices(triangleVertices, extraVertices);

		for (deUint32 y = 0u; y < imageExtent.height; ++y)
		for (deUint32 x = 0u; x < imageExtent.width; ++x)
		{
			// When applying a vertex offset in mixed mode, there will be some extra padding between the triangles for the first
			// block and the rest, so that the vertex offset will not be constant in all draw info structures. This way, the first
			// triangles will always have offset zero, and the number of them depends on the given draw count.
			const auto pixelIndex = y * imageExtent.width + x;
			if (isIndexed && isMixedMode && moreThanOneDraw && pixelIndex == trianglesPerDraw)
				appendPaddingVertices(triangleVertices, extraVertices);

			triangleGen->appendTriangle(x, y, triangleVertices);
		}
	}

	const auto			vertexBufferSize	= static_cast<VkDeviceSize>(de::dataSize(triangleVertices));
	const auto			vertexBufferInfo	= makeBufferCreateInfo(vertexBufferSize, (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	BufferWithMemory	vertexBuffer		(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
	auto&				vertexBufferAlloc	= vertexBuffer.getAllocation();
	const auto			vertexBufferOffset	= vertexBufferAlloc.getOffset();
	void*				vertexBufferData	= vertexBufferAlloc.getHostPtr();

	deMemcpy(vertexBufferData, triangleVertices.data(), de::dataSize(triangleVertices));
	flushAlloc(vkd, device, vertexBufferAlloc);

	// Index buffer if needed.
	de::MovePtr<BufferWithMemory>	indexBuffer;
	VkDeviceSize					indexBufferOffset = 0ull;

	if (isIndexed)
	{
		// Indices will be given in reverse order, so they effectively also make the triangles have reverse winding order.
		std::vector<deUint32> indices;
		indices.reserve(vertexCount);
		for (deUint32 i = 0u; i < vertexCount; ++i)
			indices.push_back(vertexCount - i - 1u);

		const auto	indexBufferSize		= static_cast<VkDeviceSize>(de::dataSize(indices));
		const auto	indexBufferInfo		= makeBufferCreateInfo(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
					indexBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, alloc, indexBufferInfo, MemoryRequirement::HostVisible));
		auto&		indexBufferAlloc	= indexBuffer->getAllocation();
					indexBufferOffset	= indexBufferAlloc.getOffset();
		void*		indexBufferData		= indexBufferAlloc.getHostPtr();

		deMemcpy(indexBufferData, indices.data(), de::dataSize(indices));
		flushAlloc(vkd, device, indexBufferAlloc);
	}

	beginCommandBuffer(vkd, cmdBuffer);

	// Transition depth/stencil attachment to the proper initial layout for the render pass.
	const auto dsPreBarrier = makeImageMemoryBarrier(
		0u,
		(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		dsBuffer.get(), dsSubresourceRange);

	vkd.cmdPipelineBarrier(
		cmdBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT),
		0u, 0u, nullptr, 0u, nullptr, 1u, &dsPreBarrier);

	// Draw stuff.
	std::vector<VkClearValue> clearValues;
	clearValues.reserve(2u);
	clearValues.push_back(makeClearValueColorU32(0u, 0u, 0u, 0u));
	clearValues.push_back(makeClearValueDepthStencil(((isMosaic || isIndexed) ? 0.0f : 1.0f), 0u));

	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissor, static_cast<deUint32>(clearValues.size()), de::dataOrNull(clearValues));

	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	if (isIndexed)
		vkd.cmdBindIndexBuffer(cmdBuffer, indexBuffer->get(), indexBufferOffset, VK_INDEX_TYPE_UINT32);

	// Draw stuff.
	const auto offsetType	= (m_params.vertexOffset ? m_params.vertexOffset->offsetType : tcu::nothing<VertexOffsetType>());
	const auto vertexOffset	= static_cast<deInt32>(extraVertices);

	DrawInfoPacker drawInfos(m_params.drawType, offsetType, m_params.stride, m_params.drawCount, m_params.seed);

	if (m_params.drawCount > 0u)
	{
		deUint32 vertexIndex = 0u;
		for (deUint32 drawIdx = 0u; drawIdx < m_params.drawCount; ++drawIdx)
		{
			// For indexed draws in mixed offset mode, taking into account vertex indices have been stored in reversed order and
			// there may be a padding in the vertex buffer after the first verticesPerDraw vertices, we need to use offset 0 in the
			// last draw call. That draw will contain the indices for the first verticesPerDraw vertices, which are stored without
			// any offset, while other draw calls will use indices which are off by extraVertices vertices. This will make sure not
			// every draw call will use the same offset and the implementation handles that.
			const auto drawOffset = ((isIndexed && (!isMixedMode || (moreThanOneDraw && drawIdx < m_params.drawCount - 1u))) ? vertexOffset : 0);
			drawInfos.addDrawInfo(vertexIndex, verticesPerDraw, drawOffset);
			vertexIndex += verticesPerDraw;
		}
	}

	if (isIndexed)
	{
		const auto drawInfoPtr	= reinterpret_cast<const VkMultiDrawIndexedInfoEXT*>(drawInfos.drawInfoData());
		const auto offsetPtr	= (isMixedMode ? nullptr : &vertexOffset);
		vkd.cmdDrawMultiIndexedEXT(cmdBuffer, drawInfos.drawInfoCount(), drawInfoPtr, m_params.instanceCount, m_params.firstInstance, drawInfos.stride(), offsetPtr);
	}
	else
	{
		const auto drawInfoPtr = reinterpret_cast<const VkMultiDrawInfoEXT*>(drawInfos.drawInfoData());
		vkd.cmdDrawMultiEXT(cmdBuffer, drawInfos.drawInfoCount(), drawInfoPtr, m_params.instanceCount, m_params.firstInstance, drawInfos.stride());
	}

	endRenderPass(vkd, cmdBuffer);

	// Prepare images for copying.
	const auto colorBufferBarrier = makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorBuffer.get(), colorSubresourceRange);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &colorBufferBarrier);

	const auto dsBufferBarrier = makeImageMemoryBarrier(
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dsBuffer.get(), dsSubresourceRange);
	vkd.cmdPipelineBarrier(cmdBuffer, (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT), VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &dsBufferBarrier);

	// Copy images to output buffers.
	const auto colorSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto colorCopyRegion			= makeBufferImageCopy(imageExtent, colorSubresourceLayers);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outputBuffer.get(), 1u, &colorCopyRegion);

	// Note: this only copies the stencil aspect. See stencilOutBuffer creation.
	const auto stencilSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u);
	const auto stencilCopyRegion		= makeBufferImageCopy(imageExtent, stencilSubresourceLayers);
	vkd.cmdCopyImageToBuffer(cmdBuffer, dsBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stencilOutBuffer.get(), 1u, &stencilCopyRegion);

	// Prepare buffers for host reading.
	const auto outputBufferBarrier		= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &outputBufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read output buffers and verify their contents.
	auto& outputBufferAlloc = outputBuffer.getAllocation();
	invalidateAlloc(vkd, device, outputBufferAlloc);
	const void* outputBufferData = outputBufferAlloc.getHostPtr();

	auto& stencilOutBufferAlloc = stencilOutBuffer.getAllocation();
	invalidateAlloc(vkd, device, stencilOutBufferAlloc);
	const void* stencilOutBufferData = stencilOutBufferAlloc.getHostPtr();

	const auto iWidth	= static_cast<int>(imageExtent.width);
	const auto iHeight	= static_cast<int>(imageExtent.height);

	const auto					colorVerificationFormat	= mapVkFormat(getVerificationFormat());
	tcu::ConstPixelBufferAccess	colorAccess				(colorVerificationFormat, iWidth, iHeight, 1, outputBufferData);
	tcu::ConstPixelBufferAccess	stencilAccess			(tcuStencilFmt, iWidth, iHeight, 1, stencilOutBufferData);

	// Generate reference images.
	tcu::TextureLevel			refColorLevel		(colorVerificationFormat, iWidth, iHeight);
	tcu::PixelBufferAccess		refColorAccess		= refColorLevel.getAccess();
	tcu::TextureLevel			refStencilLevel		(tcuStencilFmt, iWidth, iHeight);
	tcu::PixelBufferAccess		refStencilAccess	= refStencilLevel.getAccess();
	tcu::IVec4					referenceColor;
	int							referenceStencil;
	const auto					maxInstanceIndex	= m_params.maxInstanceIndex();

	// With stride zero, mosaic meshes increment the stencil buffer as many times as draw operations for affected pixels and
	// overlapping meshes increment the stencil buffer only in the first draw operation (the rest fail the depth test) as many times
	// as triangles per draw.
	//
	// With nonzero stride, mosaic meshes increment the stencil buffer once per pixel. Overlapping meshes increment it once per
	// triangle.
	const auto					stencilIncrements	=	((m_params.stride == 0u)
														? (isMosaic ? drawInfos.drawInfoCount() : trianglesPerDraw)
														: (isMosaic ? 1u : triangleCount));

	for (int y = 0; y < iHeight; ++y)
	for (int x = 0; x < iWidth; ++x)
	{
		const auto pixelNumber		= static_cast<deUint32>(y * iWidth + x);
		const auto triangleIndex	= (isIndexed ? (pixelCount - 1u - pixelNumber) : pixelNumber); // Reverse order for indexed draws.

		if (m_params.instanceCount == 0u || drawInfos.drawInfoCount() == 0u ||
			(m_params.stride == 0u && triangleIndex >= trianglesPerDraw && isMosaic))
		{
			// Some pixels may not be drawn into when there are no instances or draws, or when the stride is zero in mosaic mode.
			referenceColor		= tcu::IVec4(0, 0, 0, 0);
			referenceStencil	= 0;
		}
		else
		{
			// This must match the vertex shader.
			//
			// With stride zero, the same block is drawn over and over again in each draw call. This affects both the draw index and
			// the values in the depth/stencil buffer and, with overlapping meshes, only the first draw passes the depth test.
			//
			// With nonzero stride, the draw index depends on the triangle index and the number of triangles per draw and, for
			// overlapping meshes, the draw index is always the last one.
			const auto drawIndex =	(m_params.stride == 0u
									? (isMosaic ? (drawInfos.drawInfoCount() - 1u) : 0u)
									: (isMosaic ? (triangleIndex / trianglesPerDraw) : (drawInfos.drawInfoCount() - 1u)));
			referenceColor = tcu::IVec4(
				static_cast<int>((drawIndex >> 8) & 0xFFu),
				static_cast<int>((drawIndex     ) & 0xFFu),
				static_cast<int>(255u - maxInstanceIndex),
				255);

			referenceStencil = static_cast<int>((m_params.instanceCount * stencilIncrements) % 256u); // VK_STENCIL_OP_INCREMENT_AND_WRAP.
		}

		refColorAccess.setPixel(referenceColor, x, y);
		refStencilAccess.setPixStencil(referenceStencil, x, y);
	}

	{
		auto&		log		= m_context.getTestContext().getLog();
		const auto	logMode	= tcu::CompareLogMode::COMPARE_LOG_ON_ERROR;

		if (!tcu::intThresholdCompare(log, "ColorTestResult", "", refColorAccess, colorAccess, tcu::UVec4(0u, 0u, 0u, 0u), logMode))
			return tcu::TestStatus::fail("Color image comparison failed; check log for more details");

		if (!tcu::dsThresholdCompare(log, "StencilTestResult", "", refStencilAccess, stencilAccess, 0.0f, logMode))
			return tcu::TestStatus::fail("Stencil image comparison failed; check log for more details");
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup*	createDrawMultiExtTests (tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr drawMultiGroup (new tcu::TestCaseGroup(testCtx, "multi_draw", "VK_EXT_multi_draw tests"));

	struct
	{
		MeshType	meshType;
		const char*	name;
	} meshTypeCases[] =
	{
		{ MeshType::MOSAIC,			"mosaic"		},
		{ MeshType::OVERLAPPING,	"overlapping"	},
	};

	struct
	{
		DrawType	drawType;
		const char*	name;
	} drawTypeCases[] =
	{
		{ DrawType::NORMAL,		"normal"	},
		{ DrawType::INDEXED,	"indexed"	},
	};

	struct
	{
		tcu::Maybe<VertexOffsetType>	vertexOffsetType;
		const char*						name;
	} offsetTypeCases[] =
	{
		{ tcu::nothing<VertexOffsetType>(),		""			},
		{ VertexOffsetType::MIXED,				"mixed"		},
		{ VertexOffsetType::CONSTANT_RANDOM,	"random"	},
		{ VertexOffsetType::CONSTANT_PACK,		"packed"	},
	};

	struct
	{
		deUint32	drawCount;
		const char*	name;
	} drawCountCases[] =
	{
		{ 0u,					"no_draws"	},
		{ 1u,					"one_draw"	},
		{ 16u,					"16_draws"	},
		{ getTriangleCount(),	"max_draws"	},
	};

	struct
	{
		int			extraBytes;
		const char*	name;
	} strideCases[] =
	{
		{ -1,		"stride_zero"		},
		{  0,		"standard_stride"	},
		{  4,		"stride_extra_4"	},
		{ 12,		"stride_extra_12"	},
	};

	struct
	{
		deUint32	firstInstance;
		deUint32	instanceCount;
		const char*	name;
	} instanceCases[] =
	{
		{	0u,		0u,		"no_instances"			},
		{	0u,		1u,		"1_instance"			},
		{	0u,		10u,	"10_instances"			},
		{	3u,		2u,		"2_instances_base_3"	},
	};

	constexpr deUint32 kSeed = 1621260419u;

	for (const auto& meshTypeCase : meshTypeCases)
	{
		GroupPtr meshTypeGroup(new tcu::TestCaseGroup(testCtx, meshTypeCase.name, ""));

		for (const auto& drawTypeCase : drawTypeCases)
		{
			for (const auto& offsetTypeCase : offsetTypeCases)
			{
				const auto hasOffsetType = static_cast<bool>(offsetTypeCase.vertexOffsetType);
				if ((drawTypeCase.drawType == DrawType::NORMAL && hasOffsetType) ||
					(drawTypeCase.drawType == DrawType::INDEXED && !hasOffsetType))
				{
					continue;
				}

				std::string drawGroupName = drawTypeCase.name;
				if (hasOffsetType)
					drawGroupName += std::string("_") + offsetTypeCase.name;

				GroupPtr drawTypeGroup(new tcu::TestCaseGroup(testCtx, drawGroupName.c_str(), ""));

				for (const auto& drawCountCase : drawCountCases)
				{
					GroupPtr drawCountGroup(new tcu::TestCaseGroup(testCtx, drawCountCase.name, ""));

					for (const auto& strideCase : strideCases)
					{
						GroupPtr strideGroup(new tcu::TestCaseGroup(testCtx, strideCase.name, ""));

						for (const auto& instanceCase : instanceCases)
						{
							GroupPtr instanceGroup(new tcu::TestCaseGroup(testCtx, instanceCase.name, ""));

							const auto	isIndexed	= (drawTypeCase.drawType == DrawType::INDEXED);
							const auto	isPacked	= (offsetTypeCase.vertexOffsetType && *offsetTypeCase.vertexOffsetType == VertexOffsetType::CONSTANT_PACK);
							const auto	baseStride	= ((isIndexed && !isPacked) ? sizeof(VkMultiDrawIndexedInfoEXT) : sizeof(VkMultiDrawInfoEXT));
							const auto&	extraBytes	= strideCase.extraBytes;
							const auto	testOffset	= (isIndexed ? VertexOffsetParams{*offsetTypeCase.vertexOffsetType, 0u } : tcu::nothing<VertexOffsetParams>());
							deUint32	testStride	= 0u;

							if (extraBytes >= 0)
								testStride = static_cast<deUint32>(baseStride) + static_cast<deUint32>(extraBytes);

							// For overlapping triangles we will skip instanced drawing.
							if (instanceCase.instanceCount > 1u && meshTypeCase.meshType == MeshType::OVERLAPPING)
								continue;

							TestParams params =
							{
								meshTypeCase.meshType,			//	MeshType						meshType;
								drawTypeCase.drawType,			//	DrawType						drawType;
								drawCountCase.drawCount,		//	deUint32						drawCount;
								instanceCase.instanceCount,		//	deUint32						instanceCount;
								instanceCase.firstInstance,		//	deUint32						firstInstance;
								testStride,						//	deUint32						stride;
								testOffset,						//	tcu::Maybe<VertexOffsetParams>>	vertexOffset;	// Only used for indexed draws.
								kSeed,							//	deUint32						seed;
							};

							instanceGroup->addChild(new MultiDrawTest(testCtx, "no_offset", "", params));

							if (isIndexed)
							{
								params.vertexOffset->offset = 6u;
								instanceGroup->addChild(new MultiDrawTest(testCtx, "offset_6", "", params));
							}

							strideGroup->addChild(instanceGroup.release());
						}

						drawCountGroup->addChild(strideGroup.release());
					}

					drawTypeGroup->addChild(drawCountGroup.release());
				}

				meshTypeGroup->addChild(drawTypeGroup.release());
			}
		}

		drawMultiGroup->addChild(meshTypeGroup.release());
	}

	return drawMultiGroup.release();
}

} // Draw
} // vkt
