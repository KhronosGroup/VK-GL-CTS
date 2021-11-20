/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google LLC.
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
 * \brief Tests for provoking vertex
 *//*--------------------------------------------------------------------*/

#include "vktRasterizationProvokingVertexTests.hpp"

#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"

using namespace vk;

namespace vkt
{
namespace rasterization
{
namespace
{

enum ProvokingVertexMode
{
	PROVOKING_VERTEX_DEFAULT,
	PROVOKING_VERTEX_FIRST,
	PROVOKING_VERTEX_LAST,
	PROVOKING_VERTEX_PER_PIPELINE
};

struct Params
{
	VkFormat			format;
	tcu::UVec2			size;
	VkPrimitiveTopology	primitiveTopology;
	bool				requireGeometryShader;
	bool				transformFeedback;
	ProvokingVertexMode	provokingVertexMode;
};

static VkDeviceSize getXfbBufferSize (deUint32 vertexCount, VkPrimitiveTopology topology)
{
	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			return vertexCount * sizeof(tcu::Vec4);
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			return (vertexCount - 1) * 2 * sizeof(tcu::Vec4);
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			return (vertexCount - 2) * 3 * sizeof(tcu::Vec4);
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
			return vertexCount / 2 * sizeof(tcu::Vec4);
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			return (vertexCount - 3) * 2 * sizeof(tcu::Vec4);
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			return (vertexCount / 2 - 2) * 3 * sizeof(tcu::Vec4);
		default:
			DE_FATAL("Unknown primitive topology");
			return 0;
	}
}

static bool verifyXfbBuffer (const tcu::Vec4* const	xfbResults,
							 deUint32				count,
							 VkPrimitiveTopology	topology,
							 ProvokingVertexMode	mode,
							 std::string&			errorMessage)
{
	const deUint32	primitiveSize	= ((topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST) ||
									   (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) ||
									   (topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY) ||
									   (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY))
									? 2
									: 3;

	const tcu::Vec4	expected		(1.0f, 0.0f, 0.0f, 1.0f);
	const deUint32	start			= (mode == PROVOKING_VERTEX_LAST)
									? primitiveSize - 1
									: 0;

	DE_ASSERT(count % primitiveSize == 0);

	for (deUint32 ndx = start; ndx < count; ndx += primitiveSize)
	{
		if (xfbResults[ndx] != expected)
		{
			errorMessage =	"Vertex " + de::toString(ndx) +
							": Expected red, got " + de::toString(xfbResults[ndx]);
			return false;
		}
	}

	errorMessage = "";
	return true;
}

class ProvokingVertexTestInstance : public TestInstance
{
public:
						ProvokingVertexTestInstance	(Context& context, Params params);
	tcu::TestStatus		iterate						(void);
	Move<VkRenderPass>	makeRenderPass				(const DeviceInterface& vk, const VkDevice device);
private:
	Params	m_params;
};

ProvokingVertexTestInstance::ProvokingVertexTestInstance (Context& context, Params params)
	: TestInstance	(context)
	, m_params		(params)
{
}

class ProvokingVertexTestCase : public TestCase
{
public:
							ProvokingVertexTestCase	(tcu::TestContext&	testCtx,
													 const std::string&	name,
													 const std::string&	description,
													 const Params	params);
	virtual void			initPrograms			(SourceCollections& programCollection) const;
	virtual void			checkSupport			(Context& context) const;
	virtual TestInstance*	createInstance			(Context& context) const;
private:
	const Params			m_params;
};

ProvokingVertexTestCase::ProvokingVertexTestCase (tcu::TestContext& testCtx,
												  const std::string& name,
												  const std::string& description,
												  const Params params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
{
}

void ProvokingVertexTestCase::initPrograms (SourceCollections& programCollection) const
{
	std::ostringstream vertShader;

	vertShader	<< "#version 450\n"
				<< "layout(location = 0) in vec4 in_position;\n"
				<< "layout(location = 1) in vec4 in_color;\n"
				<< "layout(location = 0) flat out vec4 out_color;\n";

	if (m_params.transformFeedback)
		vertShader << "layout(xfb_buffer = 0, xfb_offset = 0, location = 1) out vec4 out_xfb;\n";

	vertShader	<< "void main()\n"
				<< "{\n";

	if (m_params.transformFeedback)
		vertShader << "    out_xfb = in_color;\n";

	vertShader	<< "    out_color = in_color;\n"
				<< "    gl_Position = in_position;\n"
				<< "}\n";

	const std::string	fragShader (
		"#version 450\n"
		"layout(location = 0) flat in vec4 in_color;\n"
		"layout(location = 0) out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		"    out_color = in_color;\n"
		"}\n");

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader);
}

void ProvokingVertexTestCase::checkSupport (Context& context) const
{
	if (m_params.requireGeometryShader)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (m_params.transformFeedback)
		context.requireDeviceFunctionality("VK_EXT_transform_feedback");

	if (m_params.provokingVertexMode != PROVOKING_VERTEX_DEFAULT)
	{
		const VkPhysicalDeviceProvokingVertexFeaturesEXT&	features	= context.getProvokingVertexFeaturesEXT();
		const VkPhysicalDeviceProvokingVertexPropertiesEXT&	properties	= context.getProvokingVertexPropertiesEXT();

		context.requireDeviceFunctionality("VK_EXT_provoking_vertex");

		if (m_params.transformFeedback && features.transformFeedbackPreservesProvokingVertex != VK_TRUE)
			TCU_THROW(NotSupportedError, "transformFeedbackPreservesProvokingVertex not supported");

		if (m_params.transformFeedback && (m_params.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) && (properties.transformFeedbackPreservesTriangleFanProvokingVertex != VK_TRUE))
			TCU_THROW(NotSupportedError, "transformFeedbackPreservesTriangleFanProvokingVertex not supported");

		if (m_params.provokingVertexMode != PROVOKING_VERTEX_FIRST)
		{
			if (features.provokingVertexLast != VK_TRUE)
				TCU_THROW(NotSupportedError, "VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT not supported");

			if ((m_params.provokingVertexMode == PROVOKING_VERTEX_PER_PIPELINE) && (properties.provokingVertexModePerPipeline != VK_TRUE))
				TCU_THROW(NotSupportedError, "provokingVertexModePerPipeline not supported");
		}
	}

	if (m_params.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
		context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().triangleFans)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
	}
}

TestInstance* ProvokingVertexTestCase::createInstance (Context& context) const
{
	return new ProvokingVertexTestInstance(context, m_params);
}

tcu::TestStatus ProvokingVertexTestInstance::iterate (void)
{
	const bool					useProvokingVertexExt	= (m_params.provokingVertexMode != PROVOKING_VERTEX_DEFAULT);
	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice				device					= m_context.getDevice();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const tcu::TextureFormat	textureFormat			= vk::mapVkFormat(m_params.format);
	const VkDeviceSize			counterBufferOffset		= 0u;
	Allocator&					allocator				= m_context.getDefaultAllocator();
	Move<VkImage>				image;
	Move<VkImageView>			imageView;
	de::MovePtr<Allocation>		imageMemory;
	Move<VkBuffer>				resultBuffer;
	de::MovePtr<Allocation>		resultBufferMemory;
	Move<VkBuffer>				xfbBuffer;
	de::MovePtr<Allocation>		xfbBufferMemory;
	VkDeviceSize				xfbBufferSize			= 0;
	Move<VkBuffer>				counterBuffer;
	de::MovePtr<Allocation>		counterBufferMemory;
	Move<VkBuffer>				vertexBuffer;
	de::MovePtr<Allocation>		vertexBufferMemory;
	Move<VkRenderPass>			renderPass;
	Move<VkFramebuffer>			framebuffer;
	Move<VkPipeline>			pipeline;
	Move<VkPipeline>			altPipeline;
	deUint32					vertexCount				= 0;

	// Image
	{
		const VkExtent3D		extent		= makeExtent3D(m_params.size.x(), m_params.size.y(), 1u);
		const VkImageUsageFlags	usage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
											  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		const VkImageCreateInfo	createInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// sType
			DE_NULL,								// pNext
			0u,										// flags
			VK_IMAGE_TYPE_2D,						// imageType
			m_params.format,						// format
			extent,									// extent
			1u,										// mipLevels
			1u,										// arrayLayers
			VK_SAMPLE_COUNT_1_BIT,					// samples
			VK_IMAGE_TILING_OPTIMAL,				// tiling
			usage,									// usage
			VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
			1u,										// queueFamilyIndexCount
			&queueFamilyIndex,						// pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED				// initialLayout
		};

		image = createImage(vk, device, &createInfo, DE_NULL);

		imageMemory	= allocator.allocate(getImageMemoryRequirements(vk, device, *image), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(device, *image, imageMemory->getMemory(), imageMemory->getOffset()));
	}

	// Image view
	{
		const VkImageSubresourceRange	subresourceRange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// aspectMask
			0u,							// baseMipLevel
			1u,							// mipLevels
			0u,							// baseArrayLayer
			1u							// arraySize
		};

		imageView = makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, m_params.format, subresourceRange, DE_NULL);
	}

	// Result Buffer
	{
		const VkDeviceSize			bufferSize	= textureFormat.getPixelSize() * m_params.size.x() * m_params.size.y();
		const VkBufferCreateInfo	createInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		resultBuffer		= createBuffer(vk, device, &createInfo);
		resultBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));
	}

	// Render pass, framebuffer and pipelines
	{
		const Unique<VkShaderModule>									vertexShader					(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
		const Unique<VkShaderModule>									fragmentShader					(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));
		const std::vector<VkViewport>									viewports						(1, makeViewport(tcu::UVec2(m_params.size)));
		const std::vector<VkRect2D>										scissors						(1, makeRect2D(tcu::UVec2(m_params.size)));
		const Move<VkPipelineLayout>									pipelineLayout					= makePipelineLayout(vk, device, 0, DE_NULL);

		const VkVertexInputBindingDescription							vertexInputBindingDescription	=
		{
			0,							// binding
			sizeof(tcu::Vec4) * 2,		// strideInBytes
			VK_VERTEX_INPUT_RATE_VERTEX	// stepRate
		};

		const VkVertexInputAttributeDescription							vertexAttributeDescriptions[2]	=
		{
			// Position
			{
				0u,								// location
				0u,								// binding
				VK_FORMAT_R32G32B32A32_SFLOAT,	// format
				0u								// offsetInBytes
			},
			// Color
			{
				1u,								// location
				0u,								// binding
				VK_FORMAT_R32G32B32A32_SFLOAT,	// format
				sizeof(tcu::Vec4)				// offsetInBytes
			}
		};

		const VkPipelineVertexInputStateCreateInfo						vertexInputStateParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// sType
			DE_NULL,													// pNext
			0,															// flags
			1u,															// bindingCount
			&vertexInputBindingDescription,								// pVertexBindingDescriptions
			2u,															// attributeCount
			vertexAttributeDescriptions									// pVertexAttributeDescriptions
		};

		const VkProvokingVertexModeEXT									provokingVertexMode				= m_params.provokingVertexMode == PROVOKING_VERTEX_LAST
																										? VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT
																										: VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;

		const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT	provokingVertexCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,	// sType
			DE_NULL,																			// pNext
			provokingVertexMode																	// provokingVertexMode
		};

		const VkPipelineRasterizationStateCreateInfo					rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// sType
			useProvokingVertexExt ? &provokingVertexCreateInfo : DE_NULL,	// pNext
			0,																// flags
			false,															// depthClipEnable
			false,															// rasterizerDiscardEnable
			VK_POLYGON_MODE_FILL,											// fillMode
			VK_CULL_MODE_NONE,												// cullMode
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// frontFace
			VK_FALSE,														// depthBiasEnable
			0.0f,															// depthBias
			0.0f,															// depthBiasClamp
			0.0f,															// slopeScaledDepthBias
			1.0f															// lineWidth
		};

		renderPass	= ProvokingVertexTestInstance::makeRenderPass(vk, device);
		framebuffer	= makeFramebuffer(vk, device, *renderPass, *imageView, m_params.size.x(), m_params.size.y(), 1u);
		pipeline	= makeGraphicsPipeline(vk,
										   device,
										   *pipelineLayout,
										   *vertexShader,
										   DE_NULL,							// tessellationControlShaderModule
										   DE_NULL,							// tessellationEvalShaderModule
										   DE_NULL,							// geometryShaderModule
										   *fragmentShader,
										   *renderPass,
										   viewports,
										   scissors,
										   m_params.primitiveTopology,
										   0u,								// subpass
										   0u,								// patchControlPoints
										   &vertexInputStateParams,
										   &rasterizationStateCreateInfo,
										   DE_NULL,							// multisampleStateCreateInfo
										   DE_NULL,							// depthStencilStateCreateInfo
										   DE_NULL,							// colorBlendStateCreateInfo
										   DE_NULL);						// dynamicStateCreateInfo

		if (m_params.provokingVertexMode == PROVOKING_VERTEX_PER_PIPELINE)
		{
			const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT	altProvokingVertexCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,	// sType
				DE_NULL,																			// pNext
				VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT											// provokingVertexMode
			};

			const VkPipelineRasterizationStateCreateInfo					altRasterizationStateCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// sType
				&altProvokingVertexCreateInfo,								// pNext
				0,															// flags
				false,														// depthClipEnable
				false,														// rasterizerDiscardEnable
				VK_POLYGON_MODE_FILL,										// fillMode
				VK_CULL_MODE_NONE,											// cullMode
				VK_FRONT_FACE_COUNTER_CLOCKWISE,							// frontFace
				VK_FALSE,													// depthBiasEnable
				0.0f,														// depthBias
				0.0f,														// depthBiasClamp
				0.0f,														// slopeScaledDepthBias
				1.0f,														// lineWidth
			};

			altPipeline = makeGraphicsPipeline(vk,
											   device,
											   *pipelineLayout,
											   *vertexShader,
											   DE_NULL,							// tessellationControlShaderModule
											   DE_NULL,							// tessellationEvalShaderModule
											   DE_NULL,							// geometryShaderModule
											   *fragmentShader,
											   *renderPass,
											   viewports,
											   scissors,
											   m_params.primitiveTopology,
											   0u,								// subpass
											   0u,								// patchControlPoints
											   &vertexInputStateParams,
											   &altRasterizationStateCreateInfo,
											   DE_NULL,							// multisampleStateCreateInfo
											   DE_NULL,							// depthStencilStateCreateInfo
											   DE_NULL,							// colorBlendStateCreateInfo
											   DE_NULL);						// dynamicStateCreateInfo
		}
	}

	// Vertex buffer
	{
		const tcu::Vec4			red		(1.0f, 0.0f, 0.0f, 1.0f);
		const tcu::Vec4			green	(0.0f, 1.0f, 0.0f, 1.0f);
		const tcu::Vec4			blue	(0.0f, 0.0f, 1.0f, 1.0f);
		const tcu::Vec4			yellow	(1.0f, 1.0f, 0.0f, 1.0f);
		const tcu::Vec4			white	(1.0f, 1.0f, 1.0f, 1.0f);

		std::vector<tcu::Vec4>	vertices;

		switch (m_params.primitiveTopology)
		{
			case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
				// Position												//Color
				vertices.push_back(tcu::Vec4(-1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(red);	// line 0
				vertices.push_back(tcu::Vec4( 1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4(-1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(red);	// line 1
				vertices.push_back(tcu::Vec4( 1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(blue);

				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);	// line 1 reverse
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);	// line 0 reverse
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				break;
			case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(red); // line strip
				vertices.push_back(tcu::Vec4( 1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4(-1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(green);

				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(green); // line strip reverse
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
				// Position												// Color
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);	// triangle 0
				vertices.push_back(tcu::Vec4(-0.6f,-1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4(-0.2f, 1.0f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4( 0.2f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);	// triangle 1
				vertices.push_back(tcu::Vec4( 0.6f,-1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(blue);

				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);	// triangle 1 reverse
				vertices.push_back(tcu::Vec4(-0.6f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4(-0.2f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.2f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);	// triangle 0 reverse
				vertices.push_back(tcu::Vec4( 0.6f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);	// triangle strip
				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(blue);

				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);	// triangle strip reverse
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 0.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
				// Position												// Color
				vertices.push_back(tcu::Vec4( 0.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);	// triangle fan
				vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(blue);

				vertices.push_back(tcu::Vec4( 0.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(green); // triangle fan reverse
				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				break;
			case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(green);	// line 0
				vertices.push_back(tcu::Vec4(-0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4( 1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(yellow);
				vertices.push_back(tcu::Vec4(-1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(green);	// line 1
				vertices.push_back(tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4( 1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(yellow);

				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(yellow);	// line 1 reverse
				vertices.push_back(tcu::Vec4(-0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(yellow);	// line 0 reverse
				vertices.push_back(tcu::Vec4( 0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4( 0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				break;
			case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f,-0.5f, 0.0f, 1.0f));	vertices.push_back(green);	// line strip
				vertices.push_back(tcu::Vec4(-0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4( 1.0f, 0.5f, 0.0f, 1.0f));	vertices.push_back(yellow);

				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(yellow);	// line strip reverse
				vertices.push_back(tcu::Vec4(-0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f,-0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f, 0.5f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);	// triangle 0
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4(-0.6f,-1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4(-0.2f, 1.0f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 0.2f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);	// triangle 1
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 0.6f,-1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);

				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);	// triangle 1 reverse
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4(-0.6f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4(-0.2f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 0.2f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);	// triangle 0 reverse
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 0.6f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				break;
			case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
				// Position												// Color
				vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);	// triangle strip
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4(-0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 0.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 0.5f,-1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f));	vertices.push_back(blue);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);

				vertices.push_back(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(blue);	// triangle strip reverse
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4(-0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(green);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 0.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 0.5f, 1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				vertices.push_back(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f));	vertices.push_back(red);
				vertices.push_back(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f));	vertices.push_back(white);
				break;
			default:
				DE_FATAL("Unknown primitive topology");
		}

		const size_t				bufferSize	= vertices.size() * sizeof(tcu::Vec4);
		const VkBufferCreateInfo	createInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		vertexCount			= (deUint32)vertices.size() / 4;
		vertexBuffer		= createBuffer(vk, device, &createInfo);
		vertexBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vk, device, *vertexBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *vertexBuffer, vertexBufferMemory->getMemory(), vertexBufferMemory->getOffset()));
		deMemcpy(vertexBufferMemory->getHostPtr(), &vertices[0], bufferSize);
		flushAlloc(vk, device, *vertexBufferMemory);
	}

	// Transform feedback and counter buffers
	if (m_params.transformFeedback)
	{
		xfbBufferSize	= getXfbBufferSize(vertexCount, m_params.primitiveTopology);

		if (m_params.provokingVertexMode ==PROVOKING_VERTEX_PER_PIPELINE)
			xfbBufferSize = xfbBufferSize * 2;

		const int					xfbBufferUsage		= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
		const VkBufferCreateInfo	xfbCreateInfo		= makeBufferCreateInfo(xfbBufferSize, xfbBufferUsage);
		const VkDeviceSize			counterBufferSize	= 16 * sizeof(deUint32);
		const VkBufferCreateInfo	counterBufferInfo	= makeBufferCreateInfo(counterBufferSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT);

		xfbBuffer			= createBuffer(vk, device, &xfbCreateInfo);
		xfbBufferMemory		= allocator.allocate(getBufferMemoryRequirements(vk, device, *xfbBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *xfbBuffer, xfbBufferMemory->getMemory(), xfbBufferMemory->getOffset()));

		counterBuffer		= createBuffer(vk, device, &counterBufferInfo);
		counterBufferMemory	= allocator.allocate(getBufferMemoryRequirements(vk, device, *counterBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *counterBuffer, counterBufferMemory->getMemory(), counterBufferMemory->getOffset()));
		// Make sure uninitialized values are not read when starting XFB for the first time.
		deMemset(counterBufferMemory->getHostPtr(), 0, static_cast<size_t>(counterBufferSize));
		flushAlloc(vk, device, *counterBufferMemory);
	}

	// Clear the color buffer to red and check the drawing doesn't add any
	// other colors from non-provoking vertices
	{
		const VkQueue					queue				= m_context.getUniversalQueue();
		const VkRect2D					renderArea			= makeRect2D(m_params.size.x(), m_params.size.y());
		const VkDeviceSize				vertexBufferOffset	= 0;
		const VkClearValue				clearValue			= makeClearValueColor(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
		const VkPipelineStageFlags		srcStageMask		= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		const VkPipelineStageFlags		dstStageMask		= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		const VkImageSubresourceRange	subResourcerange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// aspectMask
			0,							// baseMipLevel
			1,							// levelCount
			0,							// baseArrayLayer
			1							// layerCount
		};

		const VkImageMemoryBarrier		imageBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// sType
			DE_NULL,									// pNext
			0,											// srcAccessMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,					// oldLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// newLayout
			VK_QUEUE_FAMILY_IGNORED,					// srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,					// destQueueFamilyIndex
			*image,										// image
			subResourcerange							// subresourceRange
		};

		const VkMemoryBarrier			xfbMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
		const VkMemoryBarrier			counterBarrier		= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT, VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT);

		// The first half of the vertex buffer is for PROVOKING_VERTEX_FIRST,
		// the second half for PROVOKING_VERTEX_LAST
		const deUint32					firstVertex			= m_params.provokingVertexMode == PROVOKING_VERTEX_LAST
															? vertexCount
															: 0u;

		Move<VkCommandPool>				commandPool			= makeCommandPool(vk, device, queueFamilyIndex);
		Move<VkCommandBuffer>			commandBuffer		= allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *commandBuffer, 0u);
		{
			vk.cmdPipelineBarrier(*commandBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &imageBarrier);

			beginRenderPass(vk, *commandBuffer, *renderPass, *framebuffer, renderArea, 1, &clearValue);
			{
				vk.cmdBindVertexBuffers(*commandBuffer, 0, 1, &*vertexBuffer, &vertexBufferOffset);
				vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

				if (m_params.transformFeedback)
				{
					const VkDeviceSize	xfbBufferOffset	= 0;

					vk.cmdBindTransformFeedbackBuffersEXT(*commandBuffer, 0, 1, &*xfbBuffer, &xfbBufferOffset, &xfbBufferSize);
					vk.cmdBeginTransformFeedbackEXT(*commandBuffer, 0, 1, &*counterBuffer, &counterBufferOffset);
				}

				vk.cmdDraw(*commandBuffer, vertexCount, 1u, firstVertex, 0u);

				if (m_params.provokingVertexMode == PROVOKING_VERTEX_PER_PIPELINE)
				{
					// vkCmdBindPipeline must not be recorded when transform feedback is active.
					if (m_params.transformFeedback)
					{
						vk.cmdEndTransformFeedbackEXT(*commandBuffer, 0, 1, &*counterBuffer, &counterBufferOffset);
						vk.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, 0u, 1u, &counterBarrier, 0u, DE_NULL, 0u, DE_NULL);
					}

					vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *altPipeline);

					if (m_params.transformFeedback)
						vk.cmdBeginTransformFeedbackEXT(*commandBuffer, 0, 1, &*counterBuffer, &counterBufferOffset);

					vk.cmdDraw(*commandBuffer, vertexCount, 1u, vertexCount, 0u);
				}

				if (m_params.transformFeedback)
					vk.cmdEndTransformFeedbackEXT(*commandBuffer, 0, 1, &*counterBuffer, &counterBufferOffset);
			}
			endRenderPass(vk, *commandBuffer);

			if (m_params.transformFeedback)
				vk.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &xfbMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);

			copyImageToBuffer(vk, *commandBuffer, *image, *resultBuffer, tcu::IVec2(m_params.size.x(), m_params.size.y()));
		}
		endCommandBuffer(vk, *commandBuffer);

		submitCommandsAndWait(vk, device, queue, commandBuffer.get());
		invalidateAlloc(vk, device, *resultBufferMemory);

		if (m_params.transformFeedback)
			invalidateAlloc(vk, device, *xfbBufferMemory);
	}

	// Verify result
	{
		tcu::TestLog&				log					= m_context.getTestContext().getLog();
		const size_t				bufferSize			= textureFormat.getPixelSize() * m_params.size.x() * m_params.size.y();
		tcu::Surface				referenceSurface	(m_params.size.x(), m_params.size.y());
		tcu::ConstPixelBufferAccess	referenceAccess		= referenceSurface.getAccess();
		tcu::Surface				resultSurface		(m_params.size.x(), m_params.size.y());
		tcu::ConstPixelBufferAccess	resultAccess		(textureFormat,
														 tcu::IVec3(m_params.size.x(), m_params.size.y(), 1),
														 resultBufferMemory->getHostPtr());

		// Verify transform feedback buffer
		if (m_params.transformFeedback)
		{
			const tcu::Vec4* const	xfbResults		= static_cast<tcu::Vec4*>(xfbBufferMemory->getHostPtr());
			const deUint32			count			= static_cast<deUint32>(xfbBufferSize / sizeof(tcu::Vec4));
			std::string				errorMessage	= "";

			log << tcu::TestLog::Section("XFB Vertex colors", "vertex colors");

			for (deUint32 i = 0; i < count; i++)
			{
				log	<< tcu::TestLog::Message
					<< "[" << de::toString(i) << "]\t"
					<< de::toString(xfbResults[i])
					<< tcu::TestLog::EndMessage;
			}

			log << tcu::TestLog::EndSection;

			if (m_params.provokingVertexMode != PROVOKING_VERTEX_PER_PIPELINE)
			{
				if (!verifyXfbBuffer(xfbResults, count, m_params.primitiveTopology, m_params.provokingVertexMode, errorMessage))
					return tcu::TestStatus::fail(errorMessage);
			}
			else
			{
				const deUint32 halfCount = count / 2;

				if (!verifyXfbBuffer(xfbResults, halfCount, m_params.primitiveTopology, PROVOKING_VERTEX_FIRST, errorMessage))
					return tcu::TestStatus::fail(errorMessage);

				if (!verifyXfbBuffer(&xfbResults[halfCount], halfCount, m_params.primitiveTopology, PROVOKING_VERTEX_LAST, errorMessage))
					return tcu::TestStatus::fail(errorMessage);
			}
		}

		// Create reference
		for (deUint32 y = 0; y < m_params.size.y(); y++)
		for (deUint32 x = 0; x < m_params.size.x(); x++)
			referenceSurface.setPixel(x, y, tcu::RGBA::red());

		// Copy result
		tcu::copy(resultSurface.getAccess(), resultAccess);

		// Compare
		if (deMemCmp(referenceAccess.getDataPtr(), resultAccess.getDataPtr(), bufferSize) != 0)
		{
			log	<< tcu::TestLog::ImageSet("Result of rendering", "Result of rendering")
				<< tcu::TestLog::Image("Result", "Result", resultSurface)
				<< tcu::TestLog::EndImageSet;
			return tcu::TestStatus::fail("Incorrect rendering");
		}
	}

	return tcu::TestStatus::pass("Solid red");
}

// Copied from vkObjUtil.cpp with an additional subpass.
Move<VkRenderPass> ProvokingVertexTestInstance::makeRenderPass (const DeviceInterface& vk, const VkDevice device)
{
	const VkAttachmentDescription		colorAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags    flags
		m_params.format,							// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout                   finalLayout
	};

	const VkAttachmentReference			colorAttachmentRef			=
	{
		0u,											// deUint32         attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout    layout
	};

	const VkSubpassDescription			subpassDescription			=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&colorAttachmentRef,				// const VkAttachmentReference*    pColorAttachments
		DE_NULL,							// const VkAttachmentReference*    pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		DE_NULL								// const deUint32*                 pPreserveAttachments
	};

	const VkSubpassDependency			selfDependency			=
	{
		0u,													// deUint32                srcSubpass
		0u,													// deUint32                dstSubpass
		VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,		// VkPipelineStageFlags    srcStageMask
		VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,		// VkPipelineStageFlags    dstStageMask
		VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,	// VkAccessFlags           srcAccessMask
		VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,	// VkAccessFlags           dstAccessMask
		0u													// VkDependencyFlags       dependencyFlags
	};

	const bool							xfbPerPipeline				= m_params.transformFeedback && m_params.provokingVertexMode == PROVOKING_VERTEX_PER_PIPELINE;

	const VkRenderPassCreateInfo		renderPassInfo				=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType
		DE_NULL,									// const void*                       pNext
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags           flags
		1u,											// deUint32                          attachmentCount
		&colorAttachmentDescription,				// const VkAttachmentDescription*    pAttachments
		1u,											// deUint32                          subpassCount
		&subpassDescription,						// const VkSubpassDescription*       pSubpasses
		xfbPerPipeline ? 1u : 0u,					// deUint32                          dependencyCount
		xfbPerPipeline ? &selfDependency : DE_NULL	// const VkSubpassDependency*        pDependencies
	};

	return createRenderPass(vk, device, &renderPassInfo, DE_NULL);
}

void createTests (tcu::TestCaseGroup* testGroup)
{
	tcu::TestContext&	testCtx	= testGroup->getTestContext();

	const struct Provoking
	{
		const char*			name;
		const char*			desc;
		ProvokingVertexMode	mode;
	} provokingVertexModes[] =
	{
		{ "default",		"Default provoking vertex convention",				PROVOKING_VERTEX_DEFAULT,		},
		{ "first",			"VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT",		PROVOKING_VERTEX_FIRST,			},
		{ "last",			"VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT",			PROVOKING_VERTEX_LAST,			},
		{ "per_pipeline",	"Pipelines with different provokingVertexModes",	PROVOKING_VERTEX_PER_PIPELINE	}
	};

	const struct Topology
	{
		std::string			name;
		VkPrimitiveTopology	type;
		bool				requiresGeometryShader;
	} topologies[] =
	{
		{ "line_list",						VK_PRIMITIVE_TOPOLOGY_LINE_LIST,						false	},
		{ "line_strip",						VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,						false	},
		{ "triangle_list",					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,					false	},
		{ "triangle_strip",					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,					false	},
		{ "triangle_fan",					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,						false	},
		{ "line_list_with_adjacency",		VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,			true	},
		{ "line_strip_with_adjacency",		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,		true	},
		{ "triangle_list_with_adjacency",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,		true	},
		{ "triangle_strip_with_adjacency",	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,	true	}
	};

	const struct TestType
	{
		const char*	name;
		const char* desc;
		bool		transformFeedback;
	} testTypes[] =
	{
		{ "draw",				"Test that primitives are flat shaded with the provoking vertex color",			false	},
		{ "transform_feedback",	"Test that transform feedback preserves the position of the provoking vertex",	true	}
	};

	for (const TestType& testType: testTypes)
	{
		tcu::TestCaseGroup* const typeGroup = new tcu::TestCaseGroup(testCtx, testType.name, testType.desc);

		for (const Provoking& provoking : provokingVertexModes)
		{
			// Only test transformFeedbackPreservesProvokingVertex with VK_EXT_provoking_vertex
			if (testType.transformFeedback && (provoking.mode == PROVOKING_VERTEX_DEFAULT))
				continue;

			tcu::TestCaseGroup* const provokingGroup = new tcu::TestCaseGroup(testCtx, provoking.name, provoking.desc);

			for (const Topology& topology : topologies)
			{
				const std::string	caseName	= topology.name;
				const std::string	caseDesc	= getPrimitiveTopologyName(topology.type);

				const Params		params		=
				{
					VK_FORMAT_R8G8B8A8_UNORM,			// format
					tcu::UVec2(32, 32),					// size
					topology.type,						// primitiveTopology
					topology.requiresGeometryShader,	// requireGeometryShader
					testType.transformFeedback,			// transformFeedback
					provoking.mode						// provokingVertexMode
				};

				provokingGroup->addChild(new ProvokingVertexTestCase(testCtx, caseName, caseDesc, params));
			}

			typeGroup->addChild(provokingGroup);
		}

		testGroup->addChild(typeGroup);
	}
}

}	// anonymous

tcu::TestCaseGroup* createProvokingVertexTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx,
						   "provoking_vertex",
						   "Tests for provoking vertex",
						   createTests);
}

}	// rasterization
}	// vkt

