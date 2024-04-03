/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Shader builtin variable tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderBuiltinVarTests.hpp"

#include "tcuFloat.hpp"
#include "deUniquePtr.hpp"
#include "vkDefs.hpp"
#include "vktShaderRender.hpp"
#include "gluShaderUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "vktDrawUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"

#include "deMath.h"
#include "deRandom.hpp"

#include <map>

using namespace std;
using namespace tcu;
using namespace vk;
using namespace de;

namespace vkt
{
using namespace drawutil;

namespace sr
{

namespace
{

enum
{
	FRONTFACE_RENDERWIDTH			= 16,
	FRONTFACE_RENDERHEIGHT			= 16
};

class FrontFacingVertexShader : public rr::VertexShader
{
public:
	FrontFacingVertexShader (void)
		: rr::VertexShader(1, 0)
	{
		m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			packets[packetNdx]->position = rr::readVertexAttribFloat(inputs[0],
																	 packets[packetNdx]->instanceNdx,
																	 packets[packetNdx]->vertexNdx);
		}
	}
};

class FrontFacingFragmentShader : public rr::FragmentShader
{
public:
	FrontFacingFragmentShader (void)
		: rr::FragmentShader(0, 1)
	{
		m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments (rr::FragmentPacket* , const int numPackets, const rr::FragmentShadingContext& context) const
	{
		tcu::Vec4 color;
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
			{
				if (context.visibleFace == rr::FACETYPE_FRONT)
					color = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
				else
					color = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
				rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
			}
		}
	}
};

class BuiltinGlFrontFacingCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlFrontFacingCaseInstance	(Context& context, VkPrimitiveTopology topology);

	TestStatus		iterate								(void);
private:
	const VkPrimitiveTopology							m_topology;
};

BuiltinGlFrontFacingCaseInstance::BuiltinGlFrontFacingCaseInstance (Context& context, VkPrimitiveTopology topology)
	: ShaderRenderCaseInstance	(context)
	, m_topology				(topology)
{
}


TestStatus BuiltinGlFrontFacingCaseInstance::iterate (void)
{
	TestLog&							log				= m_context.getTestContext().getLog();
	std::vector<Vec4>					vertices;
	std::vector<VulkanShader>			shaders;
	std::shared_ptr<rr::VertexShader>	vertexShader	= std::make_shared<FrontFacingVertexShader>();
	std::shared_ptr<rr::FragmentShader>	fragmentShader	= std::make_shared<FrontFacingFragmentShader>();
	std::string							testDesc;

	vertices.push_back(Vec4( -0.75f,	-0.75f,	0.0f,	1.0f));
	vertices.push_back(Vec4(  0.0f,		-0.75f,	0.0f,	1.0f));
	vertices.push_back(Vec4( -0.37f,	0.75f,	0.0f,	1.0f));
	vertices.push_back(Vec4(  0.37f,	0.75f,	0.0f,	1.0f));
	vertices.push_back(Vec4(  0.75f,	-0.75f,	0.0f,	1.0f));
	vertices.push_back(Vec4(  0.0f,		-0.75f,	0.0f,	1.0f));

	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT, m_context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_context.getBinaryCollection().get("frag")));

	testDesc = "gl_FrontFacing " + getPrimitiveTopologyShortName(m_topology) + " ";

	FrameBufferState			frameBufferState	(FRONTFACE_RENDERWIDTH, FRONTFACE_RENDERHEIGHT);
	PipelineState				pipelineState		(m_context.getDeviceProperties().limits.subPixelPrecisionBits);
	DrawCallData				drawCallData		(m_topology, vertices);
	VulkanProgram				vulkanProgram		(shaders);
	VulkanDrawContext			dc					(m_context, frameBufferState);
	dc.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
	dc.draw();

	ReferenceDrawContext		refDrawContext(frameBufferState);
	refDrawContext.registerDrawObject(pipelineState, vertexShader, fragmentShader, drawCallData);
	refDrawContext.draw();

	log << TestLog::Image( "reference",
							"reference",
							tcu::ConstPixelBufferAccess(tcu::TextureFormat(
									refDrawContext.getColorPixels().getFormat()),
									refDrawContext.getColorPixels().getWidth(),
									refDrawContext.getColorPixels().getHeight(),
									1,
									refDrawContext.getColorPixels().getDataPtr()));

	log << TestLog::Image(	"result",
							"result",
							tcu::ConstPixelBufferAccess(tcu::TextureFormat(
									dc.getColorPixels().getFormat()),
									dc.getColorPixels().getWidth(),
									dc.getColorPixels().getHeight(),
									1,
									dc.getColorPixels().getDataPtr()));

	if (tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
												  "ComparisonResult",
												  "Image comparison result",
												  refDrawContext.getColorPixels(),
												  dc.getColorPixels(),
												  UVec4(0u),
												  IVec3(1,1,0),
												  false,
												  tcu::COMPARE_LOG_RESULT))
	{
		testDesc += "passed";
		return tcu::TestStatus::pass(testDesc.c_str());
	}
	else
	{
		testDesc += "failed";
		return tcu::TestStatus::fail(testDesc.c_str());
	}
}

class BuiltinGlFrontFacingCase : public TestCase
{
public:
								BuiltinGlFrontFacingCase	(TestContext& testCtx, VkPrimitiveTopology topology, const char* name);
	virtual						~BuiltinGlFrontFacingCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	void						checkSupport				(Context& context) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinGlFrontFacingCase	(const BuiltinGlFrontFacingCase&);	// not allowed!
	BuiltinGlFrontFacingCase&	operator=					(const BuiltinGlFrontFacingCase&);	// not allowed!

	const VkPrimitiveTopology	m_topology;
};

BuiltinGlFrontFacingCase::BuiltinGlFrontFacingCase (TestContext& testCtx, VkPrimitiveTopology topology, const char* name)
	: TestCase					(testCtx, name)
	, m_topology				(topology)
{
}

BuiltinGlFrontFacingCase::~BuiltinGlFrontFacingCase (void)
{
}

void BuiltinGlFrontFacingCase::initPrograms (SourceCollections& programCollection) const
{
	{
		std::ostringstream vertexSource;
		vertexSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "void main()\n"
			<< "{\n"
			<< "gl_Position = position;\n"
			<< "gl_PointSize = 1.0;\n"
			<< "}\n";
		programCollection.glslSources.add("vert") << glu::VertexSource(vertexSource.str());
	}

	{
		std::ostringstream fragmentSource;
		fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) out mediump vec4 color;\n"
			<< "void main()\n"
			<< "{\n"
			<< "if (gl_FrontFacing)\n"
			<< "	color = vec4(1.0, 0.0, 0.0, 1.0);\n"
			<< "else\n"
			<< "	color = vec4(0.0, 1.0, 0.0, 1.0);\n"
			<< "}\n";
		programCollection.glslSources.add("frag") << glu::FragmentSource(fragmentSource.str());
	}
}

void BuiltinGlFrontFacingCase::checkSupport (Context& context) const
{
#ifndef CTS_USES_VULKANSC
	if (m_topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
		context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().triangleFans)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
	}
#else
	DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

TestInstance* BuiltinGlFrontFacingCase::createInstance (Context& context) const
{
	return new BuiltinGlFrontFacingCaseInstance(context, m_topology);
}

class BuiltinFragDepthCaseInstance : public TestInstance
{
public:
	enum
	{
		RENDERWIDTH		= 16,
		RENDERHEIGHT	= 16
	};
					BuiltinFragDepthCaseInstance		(Context& context, VkPrimitiveTopology topology, VkFormat format, bool largeDepthEnable, float defaultDepth, bool depthClampEnable, const VkSampleCountFlagBits samples);
	TestStatus		iterate								(void);

	bool			validateDepthBuffer					(const tcu::ConstPixelBufferAccess& validationBuffer, const tcu::ConstPixelBufferAccess& markerBuffer, const float tolerance) const;
private:
	const VkPrimitiveTopology		m_topology;
	const VkFormat					m_format;
	const bool						m_largeDepthEnable;
	const float						m_defaultDepthValue;
	const bool						m_depthClampEnable;
	const VkSampleCountFlagBits		m_samples;
	const tcu::UVec2				m_renderSize;
	const float						m_largeDepthBase;
};

BuiltinFragDepthCaseInstance::BuiltinFragDepthCaseInstance (Context& context, VkPrimitiveTopology topology, VkFormat format, bool largeDepthEnable, float defaultDepth, bool depthClampEnable, const VkSampleCountFlagBits samples)
	: TestInstance			(context)
	, m_topology			(topology)
	, m_format				(format)
	, m_largeDepthEnable	(largeDepthEnable)
	, m_defaultDepthValue	(defaultDepth)
	, m_depthClampEnable	(depthClampEnable)
	, m_samples				(samples)
	, m_renderSize			(RENDERWIDTH, RENDERHEIGHT)
	, m_largeDepthBase		(20.0f)
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();

	try
	{
		VkImageFormatProperties		imageFormatProperties;
		VkFormatProperties			formatProperties;

		if (m_context.getDeviceFeatures().fragmentStoresAndAtomics == VK_FALSE)
			throw tcu::NotSupportedError("fragmentStoresAndAtomics not supported");

		if (m_context.getDeviceFeatures().sampleRateShading == VK_FALSE)
			throw tcu::NotSupportedError("sampleRateShading not supported");

		imageFormatProperties = getPhysicalDeviceImageFormatProperties(vki, physicalDevice, m_format, VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, (VkImageCreateFlags)0);

		if ((imageFormatProperties.sampleCounts & m_samples) == 0)
			throw tcu::NotSupportedError("Image format and sample count not supported");

		formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, VK_FORMAT_R8G8B8A8_UINT);

		if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
			throw tcu::NotSupportedError("MarkerImage format not supported as storage image");

		if (m_largeDepthEnable && !de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_EXT_depth_range_unrestricted"))
			throw tcu::NotSupportedError("large_depth test variants require the VK_EXT_depth_range_unrestricted extension");

		if (m_context.getDeviceFeatures().depthClamp == VK_FALSE && m_depthClampEnable)
			throw tcu::NotSupportedError("Depthclamp is not supported.");
	}
	catch (const vk::Error& e)
	{
		if (e.getError() == VK_ERROR_FORMAT_NOT_SUPPORTED)
			throw tcu::NotSupportedError("Image format not supported");
		else
			throw;

	}
}

TestStatus BuiltinFragDepthCaseInstance::iterate (void)
{
	const VkDevice					device				= m_context.getDevice();
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkQueue					queue				= m_context.getUniversalQueue();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	TestLog&						log					= m_context.getTestContext().getLog();
	const deUint32					scale				= 4;										// To account for std140 stride
	const VkDeviceSize				pixelCount			= m_renderSize.x() * m_renderSize.y();
	std::string						testDesc;
	Move<VkImage>					depthResolveImage;
	Move<VkImageView>				depthResolveImageView;
	MovePtr<Allocation>				depthResolveAllocation;
	Move<VkImage>					depthImage;
	Move<VkImageView>				depthImageView;
	MovePtr<Allocation>				depthImageAllocation;
	Move<VkBuffer>					controlBuffer;
	MovePtr<Allocation>				controlBufferAllocation;
	Move<VkImage>					markerImage;
	Move<VkImageView>				markerImageView;
	MovePtr<Allocation>				markerImageAllocation;
	Move<VkBuffer>					markerBuffer;
	MovePtr<Allocation>				markerBufferAllocation;
	Move<VkBuffer>					validationBuffer;
	MovePtr<Allocation>				validationAlloc;
	MovePtr<Allocation>				depthInitAllocation;
	Move<VkCommandPool>				cmdPool;
	Move<VkCommandBuffer>			transferCmdBuffer;
	Move<VkSampler>					depthSampler;

	// Create Buffer/Image for validation
	{
		VkFormat	resolvedBufferFormat = VK_FORMAT_R32_SFLOAT;
		const VkBufferCreateInfo validationBufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,										// VkStructureType		sType
			DE_NULL,																	// const void*			pNext
			(VkBufferCreateFlags)0,														// VkBufferCreateFlags	flags
			m_samples * pixelCount * getPixelSize(mapVkFormat(resolvedBufferFormat)),	// VkDeviceSize			size
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,											// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode		sharingMode
			0u,																			// uint32_t				queueFamilyIndexCount,
			DE_NULL																		// const uint32_t*		pQueueFamilyIndices
		};

		validationBuffer = createBuffer(vk, device, &validationBufferCreateInfo);
		validationAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *validationBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *validationBuffer, validationAlloc->getMemory(), validationAlloc->getOffset()));

		const VkImageCreateInfo depthResolveImageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,								// VkStructureType			sType
			DE_NULL,															// const void*				pNext
			(VkImageCreateFlags)0,												// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,													// VkIMageType				imageType
			resolvedBufferFormat,												// VkFormat					format
			makeExtent3D(m_samples * m_renderSize.x(), m_renderSize.y(), 1u),	// VkExtent3D				extent
			1u,																	// uint32_t					mipLevels
			1u,																	// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagsBits	samples
			VK_IMAGE_TILING_OPTIMAL,											// VkImageTiling			tiling
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |									// VkImageUsageFlags		usage
			VK_IMAGE_USAGE_STORAGE_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,											// VkSharingMode			sharingMode
			0u,																	// uint32_t					queueFamilyIndexCount
			DE_NULL,															// const uint32_t			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED											// VkImageLayout			initialLayout
		};

		depthResolveImage = createImage(vk, device, &depthResolveImageCreateInfo, DE_NULL);
		depthResolveAllocation = allocator.allocate(getImageMemoryRequirements(vk, device, *depthResolveImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(device, *depthResolveImage, depthResolveAllocation->getMemory(), depthResolveAllocation->getOffset()));

		const VkImageViewCreateInfo depthResolveImageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,								// VkStructureType			sType
			DE_NULL,																// const void*				pNext
			(VkImageViewCreateFlags)0,												// VkImageViewCreateFlags	flags
			*depthResolveImage,														// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,													// VkImageViewType			type
			resolvedBufferFormat,													// VkFormat					format
			makeComponentMappingRGBA(),												// VkComponentMapping		componentMapping
			makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)	// VkImageSUbresourceRange	subresourceRange
		};

		depthResolveImageView = createImageView(vk, device, &depthResolveImageViewCreateInfo, DE_NULL);
	}

	// Marker Buffer
	{
		const VkDeviceSize	size			= m_samples * m_renderSize.x() * m_renderSize.y() * getPixelSize(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT));

		const VkBufferCreateInfo markerBufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,			// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			(VkBufferCreateFlags)0,							// VkBufferCreateFlags		flags
			size,											// VkDeviceSize				size
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,				// VkBufferUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode
			0u,												// uint32_t					queueFamilyIndexCount
			DE_NULL											// const uint32_t*			pQueueFamilyIndices
		};

		markerBuffer = createBuffer(vk, device, &markerBufferCreateInfo, DE_NULL);
		markerBufferAllocation = allocator.allocate(getBufferMemoryRequirements(vk, device, *markerBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *markerBuffer, markerBufferAllocation->getMemory(), markerBufferAllocation->getOffset()));

		const VkImageCreateInfo markerImageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,							// VkStructureType			sType
			DE_NULL,														// const void*				pNext
			(VkImageCreateFlags)0,											// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,												// VkImageType				imageType
			VK_FORMAT_R8G8B8A8_UINT,										// VkFormat					format
			makeExtent3D(m_samples * m_renderSize.x(), m_renderSize.y(), 1),// VkExtent3D				extent
			1u,																// uint32_t					mipLevels
			1u,																// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagsBit	smaples
			VK_IMAGE_TILING_OPTIMAL,										// VkImageTiling			tiling
			VK_IMAGE_USAGE_STORAGE_BIT |									// VkImageUsageFlags		usage
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharing
			0u,																// uint32_t					queueFamilyIndexCount
			DE_NULL,														// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout
		};

		markerImage = createImage(vk, device, &markerImageCreateInfo, DE_NULL);
		markerImageAllocation = allocator.allocate(getImageMemoryRequirements(vk, device, *markerImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(device, *markerImage, markerImageAllocation->getMemory(), markerImageAllocation->getOffset()));

		const VkImageViewCreateInfo markerViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,				// VkStructureType			sType
			DE_NULL,												// const void*				pNext
			(VkImageViewCreateFlags)0,								// VkImageViewCreateFlags	flags
			*markerImage,											// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,									// VkImageViewType			viewType
			VK_FORMAT_R8G8B8A8_UINT,								// VkFormat					format
			makeComponentMappingRGBA(),								// VkComponentMapping		components
			makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)
		};

		markerImageView = createImageView(vk, device, &markerViewCreateInfo, DE_NULL);
	}

	// Control Buffer
	{
		const VkBufferCreateInfo controlBufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,					// VkStructureType		sType
			DE_NULL,												// const void*			pNext
			(VkBufferCreateFlags)0,									// VkBufferCreateFlags	flags
			pixelCount * sizeof(float)* scale,						// VkDeviceSize			size
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,						// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode		sharingMode
			0u,														// deUint32				queueFamilyIndexCount

			DE_NULL													// pQueueFamilyIndices
		};

		controlBuffer = createBuffer(vk, device, &controlBufferCreateInfo, DE_NULL);
		controlBufferAllocation = allocator.allocate( getBufferMemoryRequirements(vk, device, *controlBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *controlBuffer, controlBufferAllocation->getMemory(), controlBufferAllocation->getOffset()));

		{
			float* bufferData = (float*)(controlBufferAllocation->getHostPtr());
			float sign = m_depthClampEnable ? -1.0f : 1.0f;
			for (deUint32 ndx = 0; ndx < m_renderSize.x() * m_renderSize.y(); ndx++)
			{
				bufferData[ndx * scale] = (float)ndx / 256.0f * sign;
				if (m_largeDepthEnable)
					bufferData[ndx * scale] += m_largeDepthBase;
			}

			const VkMappedMemoryRange range =
			{
				VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				DE_NULL,
				controlBufferAllocation->getMemory(),
				0u,
				VK_WHOLE_SIZE
			};

			VK_CHECK(vk.flushMappedMemoryRanges(device, 1u, &range));
		}
	}

	// Depth Buffer
	{
		VkImageSubresourceRange depthSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
		const VkImageCreateInfo depthImageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType
			DE_NULL,												// const void*				pNext
			(VkImageCreateFlags)0,									// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,										// VkImageType				imageType
			m_format,												// VkFormat					format
			makeExtent3D(m_renderSize.x(), m_renderSize.y(), 1u),	// VkExtent3D				extent
			1u,														// uint32_t					mipLevels
			1u,														// uint32_t					arrayLayers
			m_samples,												// VkSampleCountFlagsBits	samples
			VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT      |
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,			// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,								// VkShaderingMode			sharingMode
			0u,														// uint32_t					queueFamilyIndexCount
			DE_NULL,												// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED								// VkImageLayout			initialLayout
		};

		depthImage = createImage(vk, device, &depthImageCreateInfo, DE_NULL);
		depthImageAllocation = allocator.allocate(getImageMemoryRequirements(vk, device, *depthImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(device, *depthImage, depthImageAllocation->getMemory(), depthImageAllocation->getOffset()));

		const VkImageViewCreateInfo imageViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			(VkImageViewCreateFlags)0,						// VkImageViewCreateFlags	flags;
			*depthImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_format,										// VkFormat					format;
			makeComponentMappingRGBA(),						// VkComponentMapping		components;
			depthSubresourceRange,							// VkImageSubresourceRange	subresourceRange;
		};
		depthImageView = createImageView(vk, device, &imageViewParams);

		const VkSamplerCreateInfo depthSamplerCreateInfo =
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,			// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			(VkSamplerCreateFlags)0,						// VkSamplerCreateFlags		flags
			VK_FILTER_NEAREST,								// VkFilter					minFilter
			VK_FILTER_NEAREST,								// VkFilter					magFilter
			VK_SAMPLER_MIPMAP_MODE_NEAREST,					// VkSamplerMipMapMode		mipMapMode
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// VkSamplerAddressMode		addressModeU
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// VkSamplerAddressMode		addressModeV
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// VkSamplerAddressMode		addressmodeW
			0.0f,											// float					mipLodBias
			VK_FALSE,										// VkBool32					anisotropyEnable
			0.0f,											// float					maxAnisotropy
			VK_FALSE,										// VkBool32					compareEnable
			VK_COMPARE_OP_NEVER,							// VkCompareOp				compareOp
			0.0f,											// float					minLod
			0.0f,											// float					maxLod
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,		// VkBorderColor			borderColor
			VK_FALSE										// VkBool32					unnormalizedCoordinates
		};

		depthSampler = createSampler(vk, device, &depthSamplerCreateInfo, DE_NULL);
	}

	// Command Pool
	{
		const VkCommandPoolCreateInfo cmdPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// VkStructureType			sType
			DE_NULL,											// const void*				pNext
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// VkCommandPoolCreateFlags	flags
			queueFamilyIndex									// uint32_t					queueFamilyIndex
		};

		cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
	}

	// Command buffer for data transfers
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType		sType,
			DE_NULL,										// const void*			pNext
			*cmdPool,										// VkCommandPool		commandPool
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel	level
			1u												// uint32_t				bufferCount
		};

		transferCmdBuffer = allocateCommandBuffer(vk, device, &cmdBufferAllocInfo);
	}

	// Initialize Marker Buffer
	{
		const VkImageMemoryBarrier imageBarrier[] =
		{
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType		sType
				DE_NULL,										// const void*			pNext
				0,												// VkAccessMask			srcAccessMask
				VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessMask			dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout		oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout		newLayout
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t				srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t				dstQueueFamilyIndex
				*markerImage,									// VkImage				image
				{
					VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
					0u,										// uint32_t				baseMipLevel
					1u,										// uint32_t				mipLevels
					0u,										// uint32_t				baseArray
					1u										// uint32_t				arraySize
				}
			},
		};

		const VkImageMemoryBarrier imagePostBarrier[] =
		{
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType		sType
				DE_NULL,										// const void*			pNext
				VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlagBits		srcAccessMask
				VK_ACCESS_SHADER_WRITE_BIT,						// VkAccessFlagBits		dstAccessMask
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout		oldLayout
				VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout		newLayout
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t				srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t				dstQueueFamilyIndex
				*markerImage,									// VkImage				image
				{
					VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
					0u,										// uint32_t				baseMipLevel
					1u,										// uint32_t				mipLevels
					0u,										// uint32_t				baseArray
					1u										// uint32_t				arraySize
				}
			},
		};

		beginCommandBuffer(vk, *transferCmdBuffer);
		vk.cmdPipelineBarrier(*transferCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				(VkDependencyFlags)0,
				0, (const VkMemoryBarrier*)DE_NULL,
				0, (const VkBufferMemoryBarrier*)DE_NULL,
				DE_LENGTH_OF_ARRAY(imageBarrier), imageBarrier);

		const VkClearValue				colorClearValue	= makeClearValueColor(Vec4(0.0f, 0.0f, 0.0f, 0.0f));
		const VkImageSubresourceRange	colorClearRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

		vk.cmdClearColorImage(*transferCmdBuffer, *markerImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &colorClearValue.color, 1u, &colorClearRange);

		vk.cmdPipelineBarrier(*transferCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				(VkDependencyFlags)0,
				0, (const VkMemoryBarrier*)DE_NULL,
				0, (const VkBufferMemoryBarrier*)DE_NULL,
				DE_LENGTH_OF_ARRAY(imagePostBarrier), imagePostBarrier);

		endCommandBuffer(vk, *transferCmdBuffer);

		submitCommandsAndWait(vk, device, queue, transferCmdBuffer.get());
		m_context.resetCommandPoolForVKSC(device, *cmdPool);
	}


	// Perform Draw
	{
		std::vector<Vec4>				vertices;
		std::vector<VulkanShader>		shaders;
		Move<VkDescriptorSetLayout>		descriptorSetLayout;
		Move<VkDescriptorPool>			descriptorPool;
		Move<VkDescriptorSet>			descriptorSet;

		// Descriptors
		{
			DescriptorSetLayoutBuilder	layoutBuilder;
			layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
			layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
			descriptorSetLayout = layoutBuilder.build(vk, device);
			descriptorPool = DescriptorPoolBuilder()
					.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
					.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			const VkDescriptorSetAllocateInfo descriptorSetAllocInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				DE_NULL,
				*descriptorPool,
				1u,
				&descriptorSetLayout.get()
			};

			descriptorSet = allocateDescriptorSet(vk, device, &descriptorSetAllocInfo);

			const VkDescriptorBufferInfo bufferInfo =
			{
				*controlBuffer,
				0u,
				VK_WHOLE_SIZE
			};

			const VkDescriptorImageInfo imageInfo =
			{
				(VkSampler)DE_NULL,
				*markerImageView,
				VK_IMAGE_LAYOUT_GENERAL
			};

			DescriptorSetUpdateBuilder()
				.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo)
				.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo)
				.update(vk, device);
		}

		vertices.push_back(Vec4( -0.70f,	0.5f,	0.0f,	1.0f));
		vertices.push_back(Vec4(  0.45f,	-0.75f,	0.0f,	1.0f));
		vertices.push_back(Vec4(  0.78f,	0.0f,	0.0f,	1.0f));
		vertices.push_back(Vec4( -0.1f,		0.6f,	0.0f,	1.0f));

		shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT, m_context.getBinaryCollection().get("FragDepthVert")));
		shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_context.getBinaryCollection().get("FragDepthFrag")));

		FrameBufferState			frameBufferState(m_renderSize.x(), m_renderSize.y());
		PipelineState				pipelineState(m_context.getDeviceProperties().limits.subPixelPrecisionBits);
		DrawCallData				drawCallData(m_topology, vertices);
		VulkanProgram				vulkanProgram(shaders);

		frameBufferState.depthFormat		= m_format;
		frameBufferState.numSamples			= m_samples;
		frameBufferState.depthImageView		= *depthImageView;
		pipelineState.depthClampEnable		= m_depthClampEnable;
		pipelineState.compareOp				= rr::TESTFUNC_ALWAYS;
		pipelineState.depthTestEnable		= true;
		pipelineState.depthWriteEnable		= true;
		pipelineState.sampleShadingEnable	= true;
		vulkanProgram.descriptorSetLayout	= *descriptorSetLayout;
		vulkanProgram.descriptorSet			= *descriptorSet;

		VulkanDrawContext			vulkanDrawContext(m_context, frameBufferState);
		vulkanDrawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
		vulkanDrawContext.draw();

		log << TestLog::Image(	"resultColor",
								"Result Color Buffer",
								tcu::ConstPixelBufferAccess(tcu::TextureFormat(
										vulkanDrawContext.getColorPixels().getFormat()),
										vulkanDrawContext.getColorPixels().getWidth(),
										vulkanDrawContext.getColorPixels().getHeight(),
										1,
										vulkanDrawContext.getColorPixels().getDataPtr()));
	}

	// Barrier to transition between first and second pass
	{
		const VkImageMemoryBarrier imageBarrier[] =
		{
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,						// VkStructureType		sType
				DE_NULL,													// const void*			pNext
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,				// VkAccessFlags		srcAccessMask
				VK_ACCESS_SHADER_READ_BIT,									// VkAccessFlags		dstAccessMask
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,			// VkImageLayout		oldLayout
				VK_IMAGE_LAYOUT_GENERAL,									// VkImageLayout		newLayout
				0u,															// deUint32				srcQueueFamilyIndex
				0u,															// deUint32				dstQueueFamilyIndex
				*depthImage,												// VkImage				image
				{
					VK_IMAGE_ASPECT_DEPTH_BIT,						// VkImageAspectFlags		aspectMask
					0u,												// deUint32					baseMipLevel
					1u,												// deUint32					levelCount
					0u,												// deUint32					baseArrayLayer
					1u												// deUint32					layerCount
				}
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,						// VkStructureType		sType
				DE_NULL,													// const void*			pNext
				0u,															// VkAccessFlags		srcAccessMask
				VK_ACCESS_HOST_READ_BIT,									// VkAccessFlags		dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout		oldLayout
				VK_IMAGE_LAYOUT_GENERAL,									// VkImageLayout		newLayout
				0u,															// deUint32				srcQueueFamilyIndex
				0u,															// deUint32				dstQueueFamilyIndex
				*depthResolveImage,											// VkImage				image
				{
					VK_IMAGE_ASPECT_COLOR_BIT,						// VkImageAspectFlags		aspectMask
					0u,												// deUint32					baseMipLevel
					1u,												// deUint32					levelCount
					0u,												// deUint32					baseArrayLayer
					1u,												// deUint32					layerCount

				}
			}
		};

		beginCommandBuffer(vk, *transferCmdBuffer);
		vk.cmdPipelineBarrier(*transferCmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_HOST_BIT,
				(VkDependencyFlags)0,
				0, (const VkMemoryBarrier*)DE_NULL,
				0, (const VkBufferMemoryBarrier*)DE_NULL,
				DE_LENGTH_OF_ARRAY(imageBarrier), imageBarrier);
		endCommandBuffer(vk, *transferCmdBuffer);

		submitCommandsAndWait(vk, device, queue, transferCmdBuffer.get());
		m_context.resetCommandPoolForVKSC(device, *cmdPool);
	}

	// Resolve Depth Buffer
	{
		std::vector<Vec4>				vertices;
		std::vector<VulkanShader>		shaders;
		Move<VkDescriptorSetLayout>		descriptorSetLayout;
		Move<VkDescriptorPool>			descriptorPool;
		Move<VkDescriptorSet>			descriptorSet;

		// Descriptors
		{
			DescriptorSetLayoutBuilder	layoutBuilder;
			layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
			descriptorSetLayout = layoutBuilder.build(vk, device);
			descriptorPool = DescriptorPoolBuilder()
					.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
					.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			const VkDescriptorSetAllocateInfo descriptorSetAllocInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				DE_NULL,
				*descriptorPool,
				1u,
				&descriptorSetLayout.get()
			};

			descriptorSet = allocateDescriptorSet(vk, device, &descriptorSetAllocInfo);

			const VkDescriptorImageInfo depthImageInfo =
			{
				*depthSampler,
				*depthImageView,
				VK_IMAGE_LAYOUT_GENERAL
			};

			const VkDescriptorImageInfo imageInfo =
			{
				(VkSampler)DE_NULL,
				*depthResolveImageView,
				VK_IMAGE_LAYOUT_GENERAL
			};

			DescriptorSetUpdateBuilder()
				.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthImageInfo)
				.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo)
				.update(vk, device);
		}

		vertices.push_back(Vec4( -1.0f,	-1.0f,	0.0f,	1.0f));
		vertices.push_back(Vec4( -1.0f,	 1.0f,	0.0f,	1.0f));
		vertices.push_back(Vec4(  1.0f,	-1.0f,	0.0f,	1.0f));
		vertices.push_back(Vec4(  1.0f,	 1.0f,	0.0f,	1.0f));

		shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT, m_context.getBinaryCollection().get("FragDepthVertPass2")));
		shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_context.getBinaryCollection().get("FragDepthFragPass2")));

		FrameBufferState			frameBufferState(m_renderSize.x(), m_renderSize.y());
		PipelineState				pipelineState(m_context.getDeviceProperties().limits.subPixelPrecisionBits);
		DrawCallData				drawCallData(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices);
		VulkanProgram				vulkanProgram(shaders);

		frameBufferState.numSamples				= m_samples;
		pipelineState.sampleShadingEnable		= true;
		vulkanProgram.descriptorSetLayout		= *descriptorSetLayout;
		vulkanProgram.descriptorSet				= *descriptorSet;

		VulkanDrawContext		vulkanDrawContext(m_context, frameBufferState);
		vulkanDrawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
		vulkanDrawContext.draw();
	}

	// Transfer marker buffer
	{
		beginCommandBuffer(vk, *transferCmdBuffer);
		copyImageToBuffer(vk, *transferCmdBuffer, *markerImage, *markerBuffer, tcu::IVec2(m_renderSize.x() * m_samples, m_renderSize.y()), VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
		endCommandBuffer(vk, *transferCmdBuffer);

		submitCommandsAndWait(vk, device, queue, transferCmdBuffer.get());
		m_context.resetCommandPoolForVKSC(device, *cmdPool);
	}

	// Verify depth buffer
	{
		bool status;

		beginCommandBuffer(vk, *transferCmdBuffer, 0u);
		copyImageToBuffer(vk, *transferCmdBuffer, *depthResolveImage, *validationBuffer, tcu::IVec2(m_renderSize.x() * m_samples, m_renderSize.y()), VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
		endCommandBuffer(vk, *transferCmdBuffer);

		submitCommandsAndWait(vk, device, queue, transferCmdBuffer.get());
		m_context.resetCommandPoolForVKSC(device, *cmdPool);

		invalidateMappedMemoryRange(vk, device, validationAlloc->getMemory(), validationAlloc->getOffset(), VK_WHOLE_SIZE);
		invalidateMappedMemoryRange(vk, device, markerBufferAllocation->getMemory(), markerBufferAllocation->getOffset(), VK_WHOLE_SIZE);

		tcu::ConstPixelBufferAccess resultPixelBuffer(mapVkFormat(VK_FORMAT_R32_SFLOAT), m_renderSize.x() * m_samples, m_renderSize.y(), 1u, validationAlloc->getHostPtr());
		tcu::ConstPixelBufferAccess markerPixelBuffer(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), m_renderSize.x() * m_samples, m_renderSize.y(), 1u, markerBufferAllocation->getHostPtr());
		status = validateDepthBuffer(resultPixelBuffer, markerPixelBuffer, 0.001f);
		testDesc = "gl_FragDepth " + getPrimitiveTopologyShortName(m_topology) + " ";
		if (status)
		{
			testDesc += "passed";
			return tcu::TestStatus::pass(testDesc.c_str());
		}
		else
		{
			log << TestLog::Image("resultDepth", "Result Depth Buffer", resultPixelBuffer);
			testDesc += "failed";
			return tcu::TestStatus::fail(testDesc.c_str());
		}
	}
}

bool BuiltinFragDepthCaseInstance::validateDepthBuffer (const tcu::ConstPixelBufferAccess& validationBuffer, const tcu::ConstPixelBufferAccess& markerBuffer, const float tolerance) const
{
	TestLog& log = m_context.getTestContext().getLog();

	for (deUint32 rowNdx = 0; rowNdx < m_renderSize.y(); rowNdx++)
	{
		for (deUint32 colNdx = 0; colNdx < m_renderSize.x(); colNdx++)
		{
			const float multiplier		= m_depthClampEnable ? 0.0f : 1.0f;
			float expectedValue	= (float)(rowNdx * m_renderSize.x() + colNdx)/256.0f * multiplier;

			if (m_largeDepthEnable)
				expectedValue += m_largeDepthBase;

			for (deUint32 sampleNdx = 0; sampleNdx < (deUint32)m_samples; sampleNdx++)
			{
				const float	actualValue		= validationBuffer.getPixel(sampleNdx + m_samples * colNdx, rowNdx).x();
				const float	markerValue		= markerBuffer.getPixel(sampleNdx + m_samples * colNdx, rowNdx).x();

				if (markerValue != 0)
				{
					if (de::abs(expectedValue - actualValue) > tolerance)
					{
						log << TestLog::Message << "Mismatch at pixel (" << colNdx << "," << rowNdx << "," << sampleNdx << "): expected " << expectedValue << " but got " << actualValue << TestLog::EndMessage;
						return false;
					}
				}
				else
				{
					if (de::abs(actualValue - m_defaultDepthValue) > tolerance)
					{
						log << TestLog::Message << "Mismatch at pixel (" << colNdx << "," << rowNdx << "," << sampleNdx << "): expected " << expectedValue << " but got " << actualValue << TestLog::EndMessage;
						return false;
					}
				}
			}
		}
	}

	return true;
}

class BuiltinFragCoordMsaaCaseInstance : public TestInstance
{
public:
	enum
	{
		RENDERWIDTH		= 16,
		RENDERHEIGHT	= 16
	};
				BuiltinFragCoordMsaaCaseInstance	(Context& context, VkSampleCountFlagBits sampleCount, bool sampleShading, std::vector<uint32_t> sampleMaskArray, bool useEnable);
	TestStatus	iterate								(void);
private:
	bool		validateSampleLocations				(const ConstPixelBufferAccess& sampleLocationBuffer) const;

	const tcu::UVec2				m_renderSize;
	const VkSampleCountFlagBits		m_sampleCount;
	const bool						m_sampleShading;
	const std::vector<uint32_t>		m_sampleMaskArray;
	const bool						m_useEnable;
};

BuiltinFragCoordMsaaCaseInstance::BuiltinFragCoordMsaaCaseInstance (Context& context, VkSampleCountFlagBits sampleCount, bool sampleShading, std::vector<uint32_t> sampleMaskArray, bool useEnable)
	: TestInstance		(context)
	, m_renderSize		(RENDERWIDTH, RENDERHEIGHT)
	, m_sampleCount		(sampleCount)
	, m_sampleShading	(sampleShading)
	, m_sampleMaskArray	(sampleMaskArray)
	, m_useEnable		(useEnable)
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();

	if (!context.getDeviceFeatures().sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading not supported");

	try
	{
		VkImageFormatProperties		imageFormatProperties;
		VkFormatProperties			formatProperties;

		if (m_context.getDeviceFeatures().fragmentStoresAndAtomics == VK_FALSE)
			throw tcu::NotSupportedError("fragmentStoresAndAtomics not supported");

		if (m_context.getDeviceFeatures().sampleRateShading == VK_FALSE)
			throw tcu::NotSupportedError("sampleRateShading not supported");

		imageFormatProperties = getPhysicalDeviceImageFormatProperties(vki, physicalDevice, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, (VkImageCreateFlags)0);

		if ((imageFormatProperties.sampleCounts & m_sampleCount) == 0)
			throw tcu::NotSupportedError("Image format and sample count not supported");

		formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, VK_FORMAT_R32G32B32A32_SFLOAT);

		if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
			throw tcu::NotSupportedError("Output format not supported as storage image");
	}
	catch (const vk::Error& e)
	{
		if (e.getError() == VK_ERROR_FORMAT_NOT_SUPPORTED)
			throw tcu::NotSupportedError("Image format not supported");
		else
			throw;

	}
}

TestStatus BuiltinFragCoordMsaaCaseInstance::iterate (void)
{
	const VkDevice					device				= m_context.getDevice();
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkQueue					queue				= m_context.getUniversalQueue();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	TestLog&						log					= m_context.getTestContext().getLog();
	Move<VkImage>					outputImage;
	Move<VkImageView>				outputImageView;
	MovePtr<Allocation>				outputImageAllocation;
	Move<VkDescriptorSetLayout>		descriptorSetLayout;
	Move<VkDescriptorPool>			descriptorPool;
	Move<VkDescriptorSet>			descriptorSet;
	Move<VkBuffer>					sampleLocationBuffer;
	MovePtr<Allocation>				sampleLocationBufferAllocation;
	Move<VkCommandPool>				cmdPool;
	Move<VkCommandBuffer>			transferCmdBuffer;

	// Coordinate result image
	{
		const VkImageCreateInfo outputImageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType
			DE_NULL,												// const void*				pNext
			(VkImageCreateFlags)0,									// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,										// VkImageType				imageType
			VK_FORMAT_R32G32B32A32_SFLOAT,							// VkFormat					format
			makeExtent3D(m_sampleCount * m_renderSize.x(), m_renderSize.y(), 1u),	// VkExtent3D				extent3d
			1u,														// uint32_t					mipLevels
			1u,														// uint32_t					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling
			VK_IMAGE_USAGE_STORAGE_BIT |							// VkImageUsageFlags		usage
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode
			0u,														// uint32_t					queueFamilyIndexCount
			DE_NULL,												// const uint32_t*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED								// VkImageLayout			initialLayout
		};

		outputImage = createImage(vk, device, &outputImageCreateInfo, DE_NULL);
		outputImageAllocation = allocator.allocate(getImageMemoryRequirements(vk, device, *outputImage), MemoryRequirement::Any);
		vk.bindImageMemory(device, *outputImage, outputImageAllocation->getMemory(), outputImageAllocation->getOffset());

		VkImageSubresourceRange imageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const VkImageViewCreateInfo outputImageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,				// VkStructureType			sType
			DE_NULL,												// const void*				pNext
			(VkImageViewCreateFlags)0,								// VkImageViewCreateFlags	flags
			*outputImage,											// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,									// VkImageViewType			viewType
			VK_FORMAT_R32G32B32A32_SFLOAT,							// VkFormat					format,
			makeComponentMappingRGBA(),								// VkComponentMapping		components
			imageSubresourceRange									// VkImageSubresourceRange	imageSubresourceRange
		};

		outputImageView = createImageView(vk, device, &outputImageViewCreateInfo);
	}

	// Validation buffer
	{
		VkDeviceSize  pixelSize = getPixelSize(mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT));
		const VkBufferCreateInfo sampleLocationBufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,				// VkStructureType		sType
			DE_NULL,											// const void*			pNext
			(VkBufferCreateFlags)0,								// VkBufferCreateFlags	flags
			m_sampleCount * m_renderSize.x() * m_renderSize.y() * pixelSize,	// VkDeviceSize			size
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,					// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,							// VkSharingMode		mode
			0u,													// uint32_t				queueFamilyIndexCount
			DE_NULL												// const uint32_t*		pQueueFamilyIndices
		};

		sampleLocationBuffer = createBuffer(vk, device, &sampleLocationBufferCreateInfo, DE_NULL);
		sampleLocationBufferAllocation = allocator.allocate(getBufferMemoryRequirements(vk, device, *sampleLocationBuffer), MemoryRequirement::HostVisible);
		vk.bindBufferMemory(device, *sampleLocationBuffer, sampleLocationBufferAllocation->getMemory(), sampleLocationBufferAllocation->getOffset());
	}

	// Descriptors
	{
		DescriptorSetLayoutBuilder		layoutBuilder;
		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayout = layoutBuilder.build(vk, device);
		descriptorPool = DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		const VkDescriptorSetAllocateInfo descriptorSetAllocInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			*descriptorPool,
			1u,
			&*descriptorSetLayout
		};

		descriptorSet = allocateDescriptorSet(vk, device, &descriptorSetAllocInfo);

		const VkDescriptorImageInfo imageInfo =
		{
			(VkSampler)DE_NULL,
			*outputImageView,
			VK_IMAGE_LAYOUT_GENERAL
		};

		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo)
			.update(vk, device);
	}

	// Command Pool
	{
		const VkCommandPoolCreateInfo cmdPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// VkStructureType			sType
			DE_NULL,											// const void*				pNext
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// VkCommandPoolCreateFlags	flags
			queueFamilyIndex									// uint32_t					queueFamilyIndex
		};

		cmdPool = createCommandPool(vk, device, &cmdPoolCreateInfo);
	}

	// Command buffer for data transfers
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType		sType,
			DE_NULL,										// const void*			pNext
			*cmdPool,										// VkCommandPool		commandPool
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel	level
			1u												// uint32_t				bufferCount
		};

		transferCmdBuffer = allocateCommandBuffer(vk, device, &cmdBufferAllocInfo);
	}

	// Transition the output image to LAYOUT_GENERAL
	{
		const VkImageMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType		sType
			DE_NULL,									// const void*			pNext
			0u,											// VkAccessFlags		srcAccessMask
			VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags		dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout		oldLayout
			VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout		newLayout
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t				srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t				dstQueueFamilyIndex
			*outputImage,								// VkImage				image
			{
				VK_IMAGE_ASPECT_COLOR_BIT,			// VkImageAspectFlags	aspectMask
				0u,									// uint32_t				baseMipLevel
				1u,									// uint32_t				mipLevels
				0u,									// uint32_t				baseArray
				1u									// uint32_t				arraySize
			}
		};

		beginCommandBuffer(vk, *transferCmdBuffer);
		vk.cmdPipelineBarrier(*transferCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				(VkDependencyFlags)0,
				0, (const VkMemoryBarrier*)DE_NULL,
				0, (const VkBufferMemoryBarrier*)DE_NULL,
				1, &barrier);

		endCommandBuffer(vk, *transferCmdBuffer);

		submitCommandsAndWait(vk, device, queue, transferCmdBuffer.get());
		m_context.resetCommandPoolForVKSC(device, *cmdPool);
	}

	// Perform draw
	{
		std::vector<Vec4>				vertices;
		std::vector<VulkanShader>		shaders;

		vertices.push_back(Vec4( -1.0f,	-1.0f,	0.0f,	1.0f));
		vertices.push_back(Vec4( -1.0f,	 1.0f,	0.0f,	1.0f));
		vertices.push_back(Vec4(  1.0f,	-1.0f,	0.0f,	1.0f));
		vertices.push_back(Vec4(  1.0f,	 1.0f,	0.0f,	1.0f));

		shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT, m_context.getBinaryCollection().get("FragCoordMsaaVert")));
		shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_context.getBinaryCollection().get("FragCoordMsaaFrag")));

		FrameBufferState			frameBufferState(m_renderSize.x(), m_renderSize.y());
		PipelineState				pipelineState(m_context.getDeviceProperties().limits.subPixelPrecisionBits);
		DrawCallData				drawCallData(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices);
		VulkanProgram				vulkanProgram(shaders);

		frameBufferState.numSamples				= m_sampleCount;
		pipelineState.sampleShadingEnable		= m_useEnable; // When m_useEnable is false, we rely on the gl_SampleID input to enable sample shading
		pipelineState.sampleMasks				= m_sampleMaskArray;
		vulkanProgram.descriptorSetLayout		= *descriptorSetLayout;
		vulkanProgram.descriptorSet				= *descriptorSet;

		VulkanDrawContext			vulkanDrawContext(m_context, frameBufferState);
		vulkanDrawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
		vulkanDrawContext.draw();

		log << TestLog::Image(	"result",
								"result",
								tcu::ConstPixelBufferAccess(tcu::TextureFormat(
										vulkanDrawContext.getColorPixels().getFormat()),
										vulkanDrawContext.getColorPixels().getWidth(),
										vulkanDrawContext.getColorPixels().getHeight(),
										1,
										vulkanDrawContext.getColorPixels().getDataPtr()));
	}

	// Transfer location image to buffer
	{
		beginCommandBuffer(vk, *transferCmdBuffer);
		copyImageToBuffer(vk, *transferCmdBuffer, *outputImage, *sampleLocationBuffer, tcu::IVec2(m_renderSize.x() * m_sampleCount, m_renderSize.y()), VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
		endCommandBuffer(vk, *transferCmdBuffer);

		submitCommandsAndWait(vk, device, queue, transferCmdBuffer.get());
		m_context.resetCommandPoolForVKSC(device, *cmdPool);

		invalidateAlloc(vk, device, *sampleLocationBufferAllocation);
	}

	// Validate result
	{
		bool status;

		ConstPixelBufferAccess sampleLocationPixelBuffer(mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT), m_sampleCount * m_renderSize.x(),
				m_renderSize.y(), 1u, sampleLocationBufferAllocation->getHostPtr());

		status = validateSampleLocations(sampleLocationPixelBuffer);
		if (status)
			return TestStatus::pass("FragCoordMsaa passed");
		else
			return TestStatus::fail("FragCoordMsaa failed");
	}
}

static bool pixelOffsetCompare (const Vec2& a, const Vec2& b)
{
	return a.x() < b.x();
}

bool BuiltinFragCoordMsaaCaseInstance::validateSampleLocations (const ConstPixelBufferAccess& sampleLocationBuffer) const
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	TestLog&					log					= m_context.getTestContext().getLog();
	const VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();
	deUint32					logSampleCount		= deLog2Floor32(m_sampleCount);
	VkPhysicalDeviceProperties	physicalDeviceProperties;

	static const Vec2 sampleCount1Bit[] =
	{
		Vec2(0.5f, 0.5f)
	};

	static const Vec2 sampleCount2Bit[] =
	{
		Vec2(0.25f, 0.25f), Vec2(0.75f, 0.75f)
	};

	static const Vec2 sampleCount4Bit[] =
	{
		Vec2(0.375f, 0.125f), Vec2(0.875f, 0.375f), Vec2(0.125f, 0.625f), Vec2(0.625f, 0.875f)
	};

	static const Vec2 sampleCount8Bit[] =
	{
		Vec2(0.5625f, 0.3125f), Vec2(0.4375f, 0.6875f), Vec2(0.8125f,0.5625f), Vec2(0.3125f, 0.1875f),
		Vec2(0.1875f, 0.8125f), Vec2(0.0625f, 0.4375f), Vec2(0.6875f,0.9375f), Vec2(0.9375f, 0.0625f)
	};

	static const Vec2 sampleCount16Bit[] =
	{
		Vec2(0.5625f, 0.5625f), Vec2(0.4375f, 0.3125f), Vec2(0.3125f,0.6250f), Vec2(0.7500f, 0.4375f),
		Vec2(0.1875f, 0.3750f), Vec2(0.6250f, 0.8125f), Vec2(0.8125f,0.6875f), Vec2(0.6875f, 0.1875f),
		Vec2(0.3750f, 0.8750f), Vec2(0.5000f, 0.0625f), Vec2(0.2500f,0.1250f), Vec2(0.1250f, 0.7500f),
		Vec2(0.0000f, 0.5000f), Vec2(0.9375f, 0.2500f), Vec2(0.8750f,0.9375f), Vec2(0.0625f, 0.0000f)
	};

	static const Vec2* standardSampleLocationTable[] =
	{
		sampleCount1Bit,
		sampleCount2Bit,
		sampleCount4Bit,
		sampleCount8Bit,
		sampleCount16Bit
	};

	vki.getPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	for (deInt32 rowNdx = 0; rowNdx < (deInt32)m_renderSize.y(); rowNdx++)
	{
		for (deInt32 colNdx = 0; colNdx < (deInt32)m_renderSize.x(); colNdx++)
		{
			// Check standard sample locations
			if (m_sampleShading == true)
			{
				std::vector<Vec2> locations;

				for (deUint32 sampleNdx = 0; sampleNdx < (deUint32)m_sampleCount; sampleNdx++)
				{
					const UVec2 pixelAddress	= UVec2(sampleNdx + m_sampleCount * colNdx, rowNdx);
					const Vec4  pixelData		= sampleLocationBuffer.getPixel(pixelAddress.x(), pixelAddress.y());

					if (pixelData.z() != 0.0f)
					{
						log << TestLog::Message << "Pixel (" << colNdx << "," << rowNdx << "): has unexpected .z component, expected: 0.0, got: " << pixelData.z() << TestLog::EndMessage;
						return false;
					}

					if (pixelData.w() != 1.0f)
					{
						log << TestLog::Message << "Pixel (" << colNdx << "," << rowNdx << "): has unexpected .w component, expected: 1.0, got: " << pixelData.w() << TestLog::EndMessage;
						return false;
					}

					locations.push_back(Vec2(pixelData.x(), pixelData.y()));
				}
				std::sort(locations.begin(), locations.end(), pixelOffsetCompare);
				for (std::vector<Vec2>::const_iterator sampleIt = locations.begin(); sampleIt != locations.end(); sampleIt++)
				{
					IVec2	sampleFloor(deFloorFloatToInt32((*sampleIt).x()), deFloorFloatToInt32((*sampleIt).y()));
					IVec2	sampleCeil(deCeilFloatToInt32((*sampleIt).x()), deCeilFloatToInt32((*sampleIt).y()));

					if ((sampleFloor.x() < colNdx) || (sampleCeil.x() > colNdx + 1) || (sampleFloor.y() < rowNdx) || (sampleCeil.y() > rowNdx + 1))
					{
						log << TestLog::Message << "Pixel (" << colNdx << "," << rowNdx << "): " << *sampleIt << TestLog::EndMessage;
						return false;
					}
				}

				std::vector<Vec2>::iterator last = std::unique(locations.begin(), locations.end());
				if (last != locations.end())
				{
					log << TestLog::Message << "Fail: Sample locations contains non-unique entry" << TestLog::EndMessage;
					return false;
				}

				if (logSampleCount < DE_LENGTH_OF_ARRAY(standardSampleLocationTable))
				{
					if (physicalDeviceProperties.limits.standardSampleLocations)
					{
						for (deUint32 sampleNdx = 0; sampleNdx < (deUint32)m_sampleCount; sampleNdx++)
						{
							if (!de::contains(locations.begin(), locations.end(), standardSampleLocationTable[logSampleCount][sampleNdx] + Vec2(float(colNdx), float(rowNdx))))
							{
								log << TestLog::Message << "Didn't match sample locations " << standardSampleLocationTable[logSampleCount][sampleNdx] << TestLog::EndMessage;
								return false;
							}
						}
					}
				}
			}
			else
			{
				// Check the sample location is at the pixel center when sample shading is disabled.
				const Vec4 pixelData = sampleLocationBuffer.getPixel(colNdx, rowNdx);

				if (pixelData.z() != 0.0f)
				{
					log << TestLog::Message << "Pixel (" << colNdx << "," << rowNdx << "): has unexpected .z component, expected: 0.0, got: " << pixelData.z() << TestLog::EndMessage;
					return false;
				}

				if (pixelData.w() != 1.0f)
				{
					log << TestLog::Message << "Pixel (" << colNdx << "," << rowNdx << "): has unexpected .w component, expected: 1.0, got: " << pixelData.w() << TestLog::EndMessage;
					return false;
				}

				if (!(deFloatFrac(pixelData.x()) == 0.5f && deFloatFrac(pixelData.y()) == 0.5f))
				{
					log << TestLog::Message << "Didn't match sample locations (" << pixelData.x() << ", " << pixelData.y() << "): " << Vec2(float(colNdx) + 0.5f, float(rowNdx) + 0.5f) << TestLog::EndMessage;
					return false;
				}
			}
		}
	}

	return true;
}

class BuiltinFragCoordMsaaTestCase : public TestCase
{
public:
					BuiltinFragCoordMsaaTestCase	(TestContext& testCtx, const char* name, VkSampleCountFlagBits sampleCount, bool sampleShading, std::vector<uint32_t> sampleMaskArray, bool useCentroid, bool useEnable);
	virtual			~BuiltinFragCoordMsaaTestCase	(void);
	void			initPrograms					(SourceCollections& sourceCollections) const;
	TestInstance*	createInstance					(Context& context) const;
private:
	const VkSampleCountFlagBits		m_sampleCount;
	const bool						m_sampleShading;	// Enable or disable Sample Shading.
	const std::vector<uint32_t>		m_sampleMaskArray;
	const bool						m_useCentroid;		// Use Centroid interpolation decoration.
	const bool						m_useEnable;
};

BuiltinFragCoordMsaaTestCase::BuiltinFragCoordMsaaTestCase (TestContext& testCtx, const char* name, VkSampleCountFlagBits sampleCount, bool sampleShading, std::vector<uint32_t> sampleMaskArray, bool useCentroid, bool useEnable)
	: TestCase			(testCtx, name)
	, m_sampleCount		(sampleCount)
	, m_sampleShading	(sampleShading)
	, m_sampleMaskArray	(sampleMaskArray)
	, m_useCentroid		(useCentroid)
	, m_useEnable		(useEnable)
{
}

BuiltinFragCoordMsaaTestCase::~BuiltinFragCoordMsaaTestCase (void)
{
}

void BuiltinFragCoordMsaaTestCase::initPrograms (SourceCollections& programCollection) const
{
	{
		std::ostringstream vertexSource;
		vertexSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout (location = 0) in vec4 position;\n"
			<< "void main()\n"
			<< "{\n"
			<< "	gl_Position = position;\n"
			<< "}\n";
		programCollection.glslSources.add("FragCoordMsaaVert") << glu::VertexSource(vertexSource.str());
	}

	if(m_sampleShading == true)
	{
		std::ostringstream fragmentSource;
		fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out mediump vec4 color;\n"
			<< "layout (set = 0, binding = 0, rgba32f) writeonly uniform image2D storageImage;\n"
			<< "void main()\n"
			<< "{\n"
			<< "	const int sampleNdx = int(gl_SampleID);\n"
			<< "	ivec2 imageCoord = ivec2(sampleNdx + int(gl_FragCoord.x) * " << m_sampleCount << ", int(gl_FragCoord.y));\n"
			<< "	imageStore(storageImage, imageCoord, gl_FragCoord);\n"
			<< "	color = vec4(1.0, 0.0, 0.0, 1.0);\n"
			<< "}\n";
		programCollection.glslSources.add("FragCoordMsaaFrag") << glu::FragmentSource(fragmentSource.str());
	}
	else
	{
		if (m_useCentroid == false)
		{
			std::ostringstream src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout (location = 0) out mediump vec4 color;\n"
				<< "layout (set = 0, binding = 0, rgba32f) writeonly uniform image2D storageImage;\n"
				<< "void main()\n"
				<< "{\n"
				<< "	ivec2 imageCoord = ivec2(int(gl_FragCoord.x), int(gl_FragCoord.y));\n"
				<< "	imageStore(storageImage, imageCoord, gl_FragCoord);\n"
				<< "	color = vec4(1.0, 0.0, 0.0, 1.0);\n"
				<< "}\n";

			programCollection.glslSources.add("FragCoordMsaaFrag") << glu::FragmentSource(src.str());
		}
		else
		{
			// This SPIR-V shader is identical to GLSL shader above but with the exception of that added cendroid decoration line.
			std::ostringstream src;
			src << "; SPIR - V\n"
				<< "; Version: 1.0\n"
				<< "; Generator: Khronos Glslang Reference Front End; 10\n"
				<< "; Bound: 36\n"
				<< "; Schema: 0\n"
				<< "OpCapability Shader\n"
				<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
				<< "OpMemoryModel Logical GLSL450\n"
				<< "OpEntryPoint Fragment %main \"main\" %gl_FragCoord %color\n"
				<< "OpExecutionMode %main OriginUpperLeft\n"
				<< "OpSource GLSL 450\n"
				<< "OpName %main \"main\"\n"
				<< "OpName %imageCoord \"imageCoord\"\n"
				<< "OpName %gl_FragCoord \"gl_FragCoord\"\n"
				<< "OpName %storageImage \"storageImage\"\n"
				<< "OpName %color \"color\"\n"
				<< "OpDecorate %gl_FragCoord BuiltIn FragCoord\n"
				<< "OpDecorate %gl_FragCoord Centroid\n"
				<< "OpDecorate %storageImage DescriptorSet 0\n"
				<< "OpDecorate %storageImage Binding 0\n"
				<< "OpDecorate %storageImage NonReadable\n"
				<< "OpDecorate %color RelaxedPrecision\n"
				<< "OpDecorate %color Location 0\n"
				<< "%void = OpTypeVoid\n"
				<< "%3 = OpTypeFunction %void\n"
				<< "%int = OpTypeInt 32 1\n"
				<< "%v2int = OpTypeVector %int 2\n"
				<< "%_ptr_Function_v2int = OpTypePointer Function %v2int\n"
				<< "%float = OpTypeFloat 32\n"
				<< "%v4float = OpTypeVector %float 4\n"
				<< "%_ptr_Input_v4float = OpTypePointer Input %v4float\n"
				<< "%gl_FragCoord = OpVariable %_ptr_Input_v4float Input\n"
				<< "%uint = OpTypeInt 32 0\n"
				<< "%uint_0 = OpConstant %uint 0\n"
				<< "%_ptr_Input_float = OpTypePointer Input %float\n"
				<< "%uint_1 = OpConstant %uint 1\n"
				<< "%25 = OpTypeImage %float 2D 0 0 0 2 Rgba32f\n"
				<< "%_ptr_UniformConstant_25 = OpTypePointer UniformConstant %25\n"
				<< "%storageImage = OpVariable %_ptr_UniformConstant_25 UniformConstant\n"
				<< "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
				<< "%color = OpVariable %_ptr_Output_v4float Output\n"
				<< "%float_1 = OpConstant %float 1\n"
				<< "%float_0 = OpConstant %float 0\n"
				<< "%35 = OpConstantComposite %v4float %float_1 %float_0 %float_0 %float_1\n"
				<< "%main = OpFunction %void None %3\n"
				<< "%5 = OpLabel\n"
				<< "%imageCoord = OpVariable %_ptr_Function_v2int Function\n"
				<< "%17 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_0\n"
				<< "%18 = OpLoad %float %17\n"
				<< "%19 = OpConvertFToS %int %18\n"
				<< "%21 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_1\n"
				<< "%22 = OpLoad %float %21\n"
				<< "%23 = OpConvertFToS %int %22\n"
				<< "%24 = OpCompositeConstruct %v2int %19 %23\n"
				<< "OpStore %imageCoord %24\n"
				<< "%28 = OpLoad %25 %storageImage\n"
				<< "%29 = OpLoad %v2int %imageCoord\n"
				<< "%30 = OpLoad %v4float %gl_FragCoord\n"
				<< "OpImageWrite %28 %29 %30\n"
				<< "OpStore %color %35\n"
				<< "OpReturn\n"
				<< "OpFunctionEnd\n";

			programCollection.spirvAsmSources.add("FragCoordMsaaFrag") << src.str();
		}
	}
}

TestInstance* BuiltinFragCoordMsaaTestCase::createInstance (Context& context) const
{
	return new BuiltinFragCoordMsaaCaseInstance(context, m_sampleCount, m_sampleShading, m_sampleMaskArray, m_useEnable);
}

class BuiltinFragDepthCase : public TestCase
{
public:
					BuiltinFragDepthCase		(TestContext& testCtx, const char* name, VkPrimitiveTopology topology,  VkFormat format, bool largeDepthEnable, bool depthClampEnable, const VkSampleCountFlagBits samples);
	virtual			~BuiltinFragDepthCase		(void);

	void			initPrograms				(SourceCollections& dst) const;
	TestInstance*	createInstance				(Context& context) const;

private:
	const VkPrimitiveTopology		m_topology;
	const VkFormat					m_format;
	const bool						m_largeDepthEnable;
	const float						m_defaultDepth;
	const bool						m_depthClampEnable;
	const VkSampleCountFlagBits		m_samples;
};

BuiltinFragDepthCase::BuiltinFragDepthCase (TestContext& testCtx, const char* name, VkPrimitiveTopology topology, VkFormat format, bool largeDepthEnable, bool depthClampEnable, const VkSampleCountFlagBits  samples)
	: TestCase				(testCtx, name)
	, m_topology			(topology)
	, m_format				(format)
	, m_largeDepthEnable	(largeDepthEnable)
	, m_defaultDepth		(0.0f)
	, m_depthClampEnable	(depthClampEnable)
	, m_samples				(samples)
{
}

BuiltinFragDepthCase::~BuiltinFragDepthCase(void)
{
}

void BuiltinFragDepthCase::initPrograms (SourceCollections& programCollection) const
{
	// Vertex
	{
		// Pass 1
		{
			std::ostringstream vertexSource;
			vertexSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<<  "layout (location = 0) in vec4 position;\n"
				<< "void main()\n"
				<< "{\n"
				<< "	gl_Position = position;\n"
				<< "	gl_PointSize = 1.0;\n"
				<< "}\n";
			programCollection.glslSources.add("FragDepthVert") << glu::VertexSource(vertexSource.str());
		}

		// Pass 2
		{
			std::ostringstream vertexSource;
			vertexSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<<  "layout (location = 0) in vec4 position;\n"
				<<  "layout (location = 1) out vec2 texCoord;\n"
				<< "void main()\n"
				<< "{\n"
				<< "	gl_Position = position;\n"
				<< "	gl_PointSize = 1.0;\n"
				<< "	texCoord = position.xy/2 + vec2(0.5);\n"
				<< "}\n";
			programCollection.glslSources.add("FragDepthVertPass2") << glu::VertexSource(vertexSource.str());
		}
	}

	// Fragment
	{
		// Pass 1
		{
			std::ostringstream	fragmentSource;
			fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) out mediump vec4 color;\n"
				<< "layout (std140, set = 0, binding = 0) uniform control_buffer_t\n"
				<< "{\n"
				<< "	float data[256];\n"
				<< "} control_buffer;\n"
				<< "layout (set = 0, binding = 1, rgba8ui) writeonly uniform uimage2D storageImage;\n"
				<< "float controlDepthValue;\n"
				<< "void recheck(float controlDepthValue)\n"
				<< "{\n"
				<< "	if (gl_FragDepth != controlDepthValue)\n"
				<< "		gl_FragDepth = 1.0;\n"
				<< "}\n"
				<< "void main()\n"
				<< "{\n"
				<< "	const int numSamples = " << m_samples << ";\n"
				<< "	if (int(gl_FragCoord.x) == " << BuiltinFragDepthCaseInstance::RENDERWIDTH/4 << ")\n"
				<< "		discard;\n"
				<< "	highp int index =int(gl_FragCoord.y) * " << BuiltinFragDepthCaseInstance::RENDERHEIGHT << " + int(gl_FragCoord.x);\n"
				<< "	controlDepthValue = control_buffer.data[index];\n"
				<< "	gl_FragDepth = controlDepthValue;\n"
				<< "	const int sampleNdx = int(gl_SampleID);\n"
				<< "	ivec2 imageCoord = ivec2(sampleNdx + int(gl_FragCoord.x) * " << m_samples << ", int(gl_FragCoord.y));\n"
				<< "	imageStore(storageImage, imageCoord, uvec4(1));\n"
				<< "	recheck(controlDepthValue);\n"
				<< "	color = vec4(1.0, 0.0, 0.0, 1.0);\n"
				<< "}\n";
			programCollection.glslSources.add("FragDepthFrag") << glu::FragmentSource(fragmentSource.str());
		}

		// Pass 2
		{
			const char* multisampleDecoration = m_samples != VK_SAMPLE_COUNT_1_BIT ? "MS" : "";
			std::ostringstream fragmentSource;
			fragmentSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout (location = 0) out mediump vec4 color;\n"
				<< "layout (location = 1) in vec2 texCoord;\n"
				<< "layout (binding = 0, set = 0) uniform sampler2D" << multisampleDecoration << " u_depthTex;\n"
				<< "layout (binding = 1, set = 0, r32f) writeonly uniform image2D u_outImage;\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "	const int numSamples = " << m_samples << ";\n"
				<< "	const int sampleNdx = int(gl_SampleID);\n"
				<< "	ivec2 renderSize = ivec2(" << BuiltinFragDepthCaseInstance::RENDERWIDTH << "," << BuiltinFragDepthCaseInstance::RENDERHEIGHT << ");\n"
				<< "	ivec2 imageCoord = ivec2(int(texCoord.x * renderSize.x), int(texCoord.y * renderSize.y));\n"
				<< "	vec4 depthVal = texelFetch(u_depthTex, imageCoord, sampleNdx);\n"
				<< "	imageStore(u_outImage, ivec2(sampleNdx + int(texCoord.x * renderSize.x) * numSamples, int(texCoord.y * renderSize.y)), depthVal);\n"
				<< "	color = vec4(1.0, 0.0, 0.0, 1.0);\n"
				<< "}\n";
			programCollection.glslSources.add("FragDepthFragPass2") << glu::FragmentSource(fragmentSource.str());
		}
	}
}

TestInstance* BuiltinFragDepthCase::createInstance (Context& context) const
{
	return new BuiltinFragDepthCaseInstance(context, m_topology, m_format, m_largeDepthEnable, m_defaultDepth, m_depthClampEnable, m_samples);
}

class BuiltinGlFragCoordXYZCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlFragCoordXYZCaseInstance	(Context& context);

	TestStatus		iterate								(void);
	virtual void	setupDefaultInputs					(void);
};

BuiltinGlFragCoordXYZCaseInstance::BuiltinGlFragCoordXYZCaseInstance (Context& context)
	: ShaderRenderCaseInstance	(context)
{
	m_colorFormat = VK_FORMAT_R16G16B16A16_UNORM;
}

TestStatus BuiltinGlFragCoordXYZCaseInstance::iterate (void)
{
	const UVec2		viewportSize	= getViewportSize();
	const int		width			= viewportSize.x();
	const int		height			= viewportSize.y();
	const tcu::Vec3	scale			(1.0f / float(width), 1.0f / float(height), 1.0f);
	const float		precision		= 0.00001f;
	const deUint16	indices[6]		=
	{
		2, 1, 3,
		0, 1, 2,
	};

	setup();
	addUniform(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, scale);

	render(4, 2, indices);

	// Reference image
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			const float	xf			= (float(x) + .5f) / float(width);
			const float	yf			= (float(height - y - 1) + .5f) / float(height);
			const float	z			= (xf + yf) / 2.0f;
			const Vec3	fragCoord	(float(x) + .5f, float(y) + .5f, z);
			const Vec3	scaledFC	= fragCoord*scale;
			const Vec4	color		(scaledFC.x(), scaledFC.y(), scaledFC.z(), 1.0f);
			const Vec4	resultColor	= getResultImage().getAccess().getPixel(x, y);

			if (de::abs(color.x() - resultColor.x()) > precision ||
				de::abs(color.y() - resultColor.y()) > precision ||
				de::abs(color.z() - resultColor.z()) > precision)
			return TestStatus::fail("Image mismatch");
		}
	}

	return TestStatus::pass("Result image matches reference");
}

void BuiltinGlFragCoordXYZCaseInstance::setupDefaultInputs (void)
{
	const float		vertices[]		=
	{
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.5f, 1.0f,
		 1.0f,  1.0f,  0.5f, 1.0f,
		 1.0f, -1.0f,  1.0f, 1.0f,
	};

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 4, vertices);
}

class BuiltinGlFragCoordXYZCase : public TestCase
{
public:
								BuiltinGlFragCoordXYZCase	(TestContext& testCtx, const string& name);
	virtual						~BuiltinGlFragCoordXYZCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinGlFragCoordXYZCase	(const BuiltinGlFragCoordXYZCase&);	// not allowed!
	BuiltinGlFragCoordXYZCase&	operator=					(const BuiltinGlFragCoordXYZCase&);	// not allowed!
};

BuiltinGlFragCoordXYZCase::BuiltinGlFragCoordXYZCase (TestContext& testCtx, const string& name)
	: TestCase(testCtx, name)
{
}

BuiltinGlFragCoordXYZCase::~BuiltinGlFragCoordXYZCase (void)
{
}

void BuiltinGlFragCoordXYZCase::initPrograms (SourceCollections& dst) const
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void)\n"
		"{\n"
		"       gl_Position = a_position;\n"
		"}\n");

	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(set=0, binding=0) uniform Scale { highp vec3 u_scale; };\n"
		"layout(location = 0) out highp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"       o_color = vec4(gl_FragCoord.xyz * u_scale, 1.0);\n"
		"}\n");
}

TestInstance* BuiltinGlFragCoordXYZCase::createInstance (Context& context) const
{
	return new BuiltinGlFragCoordXYZCaseInstance(context);
}

inline float projectedTriInterpolate (const Vec3& s, const Vec3& w, float nx, float ny)
{
	return (s[0]*(1.0f-nx-ny)/w[0] + s[1]*ny/w[1] + s[2]*nx/w[2]) / ((1.0f-nx-ny)/w[0] + ny/w[1] + nx/w[2]);
}

class BuiltinGlFragCoordWCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlFragCoordWCaseInstance	(Context& context);

	TestStatus		iterate							(void);
	virtual void	setupDefaultInputs				(void);

private:

	const Vec4		m_w;

};

BuiltinGlFragCoordWCaseInstance::BuiltinGlFragCoordWCaseInstance (Context& context)
	: ShaderRenderCaseInstance	(context)
	, m_w						(1.7f, 2.0f, 1.2f, 1.0f)
{
	m_colorFormat = VK_FORMAT_R16G16B16A16_UNORM;
}

TestStatus BuiltinGlFragCoordWCaseInstance::iterate (void)
{
	const UVec2		viewportSize	= getViewportSize();
	const int		width			= viewportSize.x();
	const int		height			= viewportSize.y();
	const float		precision		= 0.00001f;
	const deUint16	indices[6]		=
	{
		2, 1, 3,
		0, 1, 2,
	};

	setup();
	render(4, 2, indices);

	// Reference image
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			const float	xf			= (float(x) + .5f) / float(width);
			const float	yf			= (float(height - y - 1) +.5f) / float(height);
			const float	oow			= ((xf + yf) < 1.0f)
										? projectedTriInterpolate(Vec3(m_w[0], m_w[1], m_w[2]), Vec3(m_w[0], m_w[1], m_w[2]), xf, yf)
										: projectedTriInterpolate(Vec3(m_w[3], m_w[2], m_w[1]), Vec3(m_w[3], m_w[2], m_w[1]), 1.0f - xf, 1.0f - yf);
			const Vec4	color		(0.0f, oow - 1.0f, 0.0f, 1.0f);
			const Vec4	resultColor	= getResultImage().getAccess().getPixel(x, y);

			if (de::abs(color.x() - resultColor.x()) > precision ||
				de::abs(color.y() - resultColor.y()) > precision ||
				de::abs(color.z() - resultColor.z()) > precision)
			return TestStatus::fail("Image mismatch");
		}
	}

	return TestStatus::pass("Result image matches reference");
}

void BuiltinGlFragCoordWCaseInstance::setupDefaultInputs (void)
{
	const float vertices[] =
	{
		-m_w[0],  m_w[0], 0.0f, m_w[0],
		-m_w[1], -m_w[1], 0.0f, m_w[1],
		 m_w[2],  m_w[2], 0.0f, m_w[2],
		 m_w[3], -m_w[3], 0.0f, m_w[3]
	};

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 4, vertices);
}

class BuiltinGlFragCoordWCase : public TestCase
{
public:
								BuiltinGlFragCoordWCase		(TestContext& testCtx, const string& name);
	virtual						~BuiltinGlFragCoordWCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinGlFragCoordWCase		(const BuiltinGlFragCoordWCase&);	// not allowed!
	BuiltinGlFragCoordWCase&	operator=					(const BuiltinGlFragCoordWCase&);	// not allowed!
};

BuiltinGlFragCoordWCase::BuiltinGlFragCoordWCase (TestContext& testCtx, const string& name)
	: TestCase(testCtx, name)
{
}

BuiltinGlFragCoordWCase::~BuiltinGlFragCoordWCase (void)
{
}

void BuiltinGlFragCoordWCase::initPrograms (SourceCollections& dst) const
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void)\n"
		"{\n"
		"       gl_Position = a_position;\n"
		"}\n");

	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) out highp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"       o_color = vec4(0.0, 1.0 / gl_FragCoord.w - 1.0, 0.0, 1.0);\n"
		"}\n");
}

TestInstance* BuiltinGlFragCoordWCase::createInstance (Context& context) const
{
	return new BuiltinGlFragCoordWCaseInstance(context);
}

enum
{
	POINTCOORD_VARIANT_DEFAULT,
	POINTCOORD_VARIANT_UNIFORM_VERTEX,
	POINTCOORD_VARIANT_UNIFORM_FRAGMENT
};


class BuiltinGlPointCoordCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlPointCoordCaseInstance	(Context& context, int testVariant);

	TestStatus		iterate								(void);
	virtual void	setupDefaultInputs					(void);
private:
	int				variant;
};

BuiltinGlPointCoordCaseInstance::BuiltinGlPointCoordCaseInstance (Context& context, int testVariant)
	: ShaderRenderCaseInstance	(context),
	  variant(testVariant)
{
}

TestStatus BuiltinGlPointCoordCaseInstance::iterate (void)
{
	const UVec2				viewportSize	= getViewportSize();
	const int				width			= viewportSize.x();
	const int				height			= viewportSize.y();
	const float				threshold		= 0.02f;
	const int				numPoints		= 16;
	vector<Vec3>			coords			(numPoints);
	de::Random				rnd				(0x145fa);
	Surface					resImage		(width, height);
	Surface					refImage		(width, height);
	bool					compareOk		= false;
	const tcu::Vec3			scale(1.0f / float(width), 1.0f / float(height), 1.0f);

	// Compute coordinates.
	{
		const VkPhysicalDeviceLimits&	limits					= m_context.getDeviceProperties().limits;
		const float						minPointSize			= limits.pointSizeRange[0];
		const float						maxPointSize			= limits.pointSizeRange[1];
		const int						pointSizeDeltaMultiples	= de::max(1, deCeilFloatToInt32((maxPointSize - minPointSize) / limits.pointSizeGranularity));

		TCU_CHECK(minPointSize <= maxPointSize);

		for (vector<Vec3>::iterator coord = coords.begin(); coord != coords.end(); ++coord)
		{
			coord->x() = rnd.getFloat(-0.9f, 0.9f);
			coord->y() = rnd.getFloat(-0.9f, 0.9f);
			coord->z() = de::min(maxPointSize, minPointSize + float(rnd.getInt(0, pointSizeDeltaMultiples)) * limits.pointSizeGranularity);
		}
	}

	setup();

	if (variant == POINTCOORD_VARIANT_UNIFORM_VERTEX || variant == POINTCOORD_VARIANT_UNIFORM_FRAGMENT)
		addUniform(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, scale);

	addAttribute(0u, VK_FORMAT_R32G32B32_SFLOAT, deUint32(sizeof(Vec3)), numPoints, &coords[0]);
	render(numPoints, 0, DE_NULL, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	copy(resImage.getAccess(), getResultImage().getAccess());

	// Draw reference
	clear(refImage.getAccess(), m_clearColor);

	for (vector<Vec3>::const_iterator pointIter = coords.begin(); pointIter != coords.end(); ++pointIter)
	{
		float x = pointIter->x();
		float y = pointIter->y();
		if (variant == POINTCOORD_VARIANT_UNIFORM_VERTEX)
		{
			x *= scale.m_data[0];
			y *= scale.m_data[1];
		}
		const float	centerX	= float(width) *(x*0.5f + 0.5f);
		const float	centerY	= float(height)*(y*0.5f + 0.5f);
		const float	size	= pointIter->z();
		const int	x0		= deRoundFloatToInt32(centerX - size*0.5f);
		const int	y0		= deRoundFloatToInt32(centerY - size*0.5f);
		const int	x1		= deRoundFloatToInt32(centerX + size*0.5f);
		const int	y1		= deRoundFloatToInt32(centerY + size*0.5f);
		const int	w		= x1-x0;
		const int	h		= y1-y0;

		for (int yo = 0; yo < h; yo++)
		{
			for (int xo = 0; xo < w; xo++)
			{
				const int		dx		= x0+xo;
				const int		dy		= y0+yo;
				const float		fragX	= float(dx) + 0.5f;
				const float		fragY	= float(dy) + 0.5f;
				const float		s		= 0.5f + (fragX - centerX) / size;
				const float		t		= 0.5f + (fragY - centerY) / size;
				Vec4			color	(s, t, 0.0f, 1.0f);

				if (variant == POINTCOORD_VARIANT_UNIFORM_FRAGMENT)
				{
					color.m_data[0] *= scale.m_data[0];
					color.m_data[1] *= scale.m_data[1];
					color.m_data[2] *= scale.m_data[2];
				}

				if (de::inBounds(dx, 0, refImage.getWidth()) && de::inBounds(dy, 0, refImage.getHeight()))
					refImage.setPixel(dx, dy, RGBA(color));
			}
		}
	}

	compareOk = fuzzyCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void BuiltinGlPointCoordCaseInstance::setupDefaultInputs (void)
{
}

class BuiltinGlPointCoordCase : public TestCase
{
public:
								BuiltinGlPointCoordCase	(TestContext& testCtx, const string& name, int testVariant);
	virtual						~BuiltinGlPointCoordCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
	int							variant;
								BuiltinGlPointCoordCase	(const BuiltinGlPointCoordCase&);	// not allowed!
	BuiltinGlPointCoordCase&	operator=					(const BuiltinGlPointCoordCase&);	// not allowed!
};

BuiltinGlPointCoordCase::BuiltinGlPointCoordCase (TestContext& testCtx, const string& name, int testVariant)
	: TestCase(testCtx, name),
	  variant(testVariant)
{
}

BuiltinGlPointCoordCase::~BuiltinGlPointCoordCase (void)
{
}

void BuiltinGlPointCoordCase::initPrograms (SourceCollections& dst) const
{
	switch (variant)
	{
	case POINTCOORD_VARIANT_UNIFORM_FRAGMENT:
		dst.glslSources.add("vert") << glu::VertexSource(
			"#version 310 es\n"
			"layout(location = 0) in highp vec3 a_position;\n"
			"void main (void)\n"
			"{\n"
			"    gl_Position = vec4(a_position.xy, 0.0, 1.0);\n"
			"    gl_PointSize = a_position.z;\n"
			"}\n");

		dst.glslSources.add("frag") << glu::FragmentSource(
			"#version 310 es\n"
			"layout(set=0, binding=0) uniform Scale { highp vec3 u_scale; };\n"
			"layout(location = 0) out lowp vec4 o_color;\n"
			"void main (void)\n"
			"{\n"
			"    o_color = vec4(gl_PointCoord, 0.0, 1.0) * vec4(u_scale, 1.0);\n"
			"}\n");
		break;
	case POINTCOORD_VARIANT_UNIFORM_VERTEX:
		dst.glslSources.add("vert") << glu::VertexSource(
			"#version 310 es\n"
			"layout(set=0, binding=0) uniform Scale { highp vec3 u_scale; };\n"
			"layout(location = 0) in highp vec3 a_position;\n"
			"void main (void)\n"
			"{\n"
			"    gl_Position = vec4(a_position.xy, 0.0, 1.0) * vec4(u_scale, 1.0);\n"
			"    gl_PointSize = a_position.z;\n"
			"}\n");

		dst.glslSources.add("frag") << glu::FragmentSource(
			"#version 310 es\n"
			"layout(location = 0) out lowp vec4 o_color;\n"
			"void main (void)\n"
			"{\n"
			"    o_color = vec4(gl_PointCoord, 0.0, 1.0);\n"
			"}\n");
		break;
	default: // POINTCOORD_VARIANT_DEFAULT
		dst.glslSources.add("vert") << glu::VertexSource(
			"#version 310 es\n"
			"layout(location = 0) in highp vec3 a_position;\n"
			"void main (void)\n"
			"{\n"
			"    gl_Position = vec4(a_position.xy, 0.0, 1.0);\n"
			"    gl_PointSize = a_position.z;\n"
			"}\n");

		dst.glslSources.add("frag") << glu::FragmentSource(
			"#version 310 es\n"
			"layout(location = 0) out lowp vec4 o_color;\n"
			"void main (void)\n"
			"{\n"
			"    o_color = vec4(gl_PointCoord, 0.0, 1.0);\n"
			"}\n");
	}
}

TestInstance* BuiltinGlPointCoordCase::createInstance (Context& context) const
{
	return new BuiltinGlPointCoordCaseInstance(context, variant);
}

enum ShaderInputTypeBits
{
	SHADER_INPUT_BUILTIN_BIT	= 0x01,
	SHADER_INPUT_VARYING_BIT	= 0x02,
	SHADER_INPUT_CONSTANT_BIT	= 0x04
};

typedef deUint16 ShaderInputTypes;

string shaderInputTypeToString (ShaderInputTypes type)
{
	string typeString = "input";

	if (type == 0)
		return "input_none";

	if (type & SHADER_INPUT_BUILTIN_BIT)
		typeString += "_builtin";

	if (type & SHADER_INPUT_VARYING_BIT)
		typeString += "_varying";

	if (type & SHADER_INPUT_CONSTANT_BIT)
		typeString += "_constant";

	return typeString;
}

class BuiltinInputVariationsCaseInstance : public ShaderRenderCaseInstance
{
public:
							BuiltinInputVariationsCaseInstance	(Context& context, const ShaderInputTypes shaderInputTypes);

	TestStatus				iterate								(void);
	virtual void			setupDefaultInputs					(void);
	virtual void			updatePushConstants					(vk::VkCommandBuffer commandBuffer, vk::VkPipelineLayout pipelineLayout);

private:
	const ShaderInputTypes	m_shaderInputTypes;
	const Vec4				m_constantColor;
};

BuiltinInputVariationsCaseInstance::BuiltinInputVariationsCaseInstance (Context& context, const ShaderInputTypes shaderInputTypes)
	: ShaderRenderCaseInstance	(context)
	, m_shaderInputTypes		(shaderInputTypes)
	, m_constantColor			(0.1f, 0.05f, 0.2f, 0.0f)
{
}

TestStatus BuiltinInputVariationsCaseInstance::iterate (void)
{
	const UVec2					viewportSize	= getViewportSize();
	const int					width			= viewportSize.x();
	const int					height			= viewportSize.y();
	const tcu::RGBA				threshold		(2, 2, 2, 2);
	Surface						resImage		(width, height);
	Surface						refImage		(width, height);
	bool						compareOk		= false;
	const VkPushConstantRange	pcRanges		=
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,	// VkShaderStageFlags	stageFlags;
		0u,								// deUint32				offset;
		sizeof(Vec4)					// deUint32				size;
	};
	const deUint16				indices[12]		=
	{
		0, 4, 1,
		0, 5, 4,
		1, 2, 3,
		1, 3, 4
	};

	setup();

	if (m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT)
		setPushConstantRanges(1, &pcRanges);

	render(6, 4, indices);
	copy(resImage.getAccess(), getResultImage().getAccess());

	// Reference image
	for (int y = 0; y < refImage.getHeight(); y++)
	{
		for (int x = 0; x < refImage.getWidth(); x++)
		{
			Vec4 color (0.1f, 0.2f, 0.3f, 1.0f);

			if (((m_shaderInputTypes & SHADER_INPUT_BUILTIN_BIT) && (x < refImage.getWidth() / 2)) ||
				!(m_shaderInputTypes & SHADER_INPUT_BUILTIN_BIT))
			{
				if (m_shaderInputTypes & SHADER_INPUT_VARYING_BIT)
				{
					const float xf = (float(x)+.5f) / float(refImage.getWidth());
					color += Vec4(0.6f * (1 - xf), 0.6f * xf, 0.0f, 0.0f);
				}
				else
					color += Vec4(0.3f, 0.2f, 0.1f, 0.0f);
			}

			if (m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT)
				color += m_constantColor;

			refImage.setPixel(x, y, RGBA(color));
		}
	}

	compareOk = pixelThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void BuiltinInputVariationsCaseInstance::setupDefaultInputs (void)
{
	const float vertices[] =
	{
		-1.0f, -1.0f, 0.0f, 1.0f,
		 0.0f, -1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 0.0f, 1.0f,
		 0.0f,  1.0f, 0.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f
	};

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 6, vertices);

	if (m_shaderInputTypes & SHADER_INPUT_VARYING_BIT)
	{
		const float colors[] =
		{
			 0.6f,  0.0f, 0.0f, 1.0f,
			 0.3f,  0.3f, 0.0f, 1.0f,
			 0.0f,  0.6f, 0.0f, 1.0f,
			 0.0f,  0.6f, 0.0f, 1.0f,
			 0.3f,  0.3f, 0.0f, 1.0f,
			 0.6f,  0.0f, 0.0f, 1.0f
		};
		addAttribute(1u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 6, colors);
	}
}

void BuiltinInputVariationsCaseInstance::updatePushConstants (vk::VkCommandBuffer commandBuffer, vk::VkPipelineLayout pipelineLayout)
{
	if (m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT)
	{
		const DeviceInterface& vk = m_context.getDeviceInterface();
		vk.cmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4), &m_constantColor);
	}
}

class BuiltinInputVariationsCase : public TestCase
{
public:
								BuiltinInputVariationsCase	(TestContext& testCtx, const string& name, const ShaderInputTypes shaderInputTypes);
	virtual						~BuiltinInputVariationsCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinInputVariationsCase	(const BuiltinInputVariationsCase&);	// not allowed!
	BuiltinInputVariationsCase&	operator=					(const BuiltinInputVariationsCase&);	// not allowed!
	const ShaderInputTypes		m_shaderInputTypes;
};

BuiltinInputVariationsCase::BuiltinInputVariationsCase (TestContext& testCtx, const string& name, ShaderInputTypes shaderInputTypes)
	: TestCase				(testCtx, name)
	, m_shaderInputTypes	(shaderInputTypes)
{
}

BuiltinInputVariationsCase::~BuiltinInputVariationsCase (void)
{
}

void BuiltinInputVariationsCase::initPrograms (SourceCollections& dst) const
{
	map<string, string>			vertexParams;
	map<string, string>			fragmentParams;
	const tcu::StringTemplate	vertexCodeTemplate		(
		"#version 450\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"out gl_PerVertex {\n"
		"	vec4 gl_Position;\n"
		"};\n"
		"${VARYING_DECL}"
		"void main (void)\n"
		"{\n"
		"    gl_Position = a_position;\n"
		"    ${VARYING_USAGE}"
		"}\n");

	const tcu::StringTemplate	fragmentCodeTemplate	(
		"#version 450\n"
		"${VARYING_DECL}"
		"${CONSTANT_DECL}"
		"layout(location = 0) out highp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"    o_color = vec4(0.1, 0.2, 0.3, 1.0);\n"
		"    ${BUILTIN_USAGE}"
		"    ${VARYING_USAGE}"
		"    ${CONSTANT_USAGE}"
		"}\n");

	vertexParams["VARYING_DECL"]		=
		m_shaderInputTypes & SHADER_INPUT_VARYING_BIT	? "layout(location = 1) in highp vec4 a_color;\n"
														  "layout(location = 0) out highp vec4 v_color;\n"
														: "";

	vertexParams["VARYING_USAGE"]		=
		m_shaderInputTypes & SHADER_INPUT_VARYING_BIT	? "v_color = a_color;\n"
														: "";

	fragmentParams["VARYING_DECL"]		=
		m_shaderInputTypes & SHADER_INPUT_VARYING_BIT	? "layout(location = 0) in highp vec4 a_color;\n"
														: "";

	fragmentParams["CONSTANT_DECL"]		=
		m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT	? "layout(push_constant) uniform PCBlock {\n"
														  "  vec4 color;\n"
														  "} pc;\n"
														: "";

	fragmentParams["BUILTIN_USAGE"]		=
		m_shaderInputTypes & SHADER_INPUT_BUILTIN_BIT	? "if (gl_FrontFacing)\n"
														: "";

	fragmentParams["VARYING_USAGE"]		=
		m_shaderInputTypes & SHADER_INPUT_VARYING_BIT	? "o_color += vec4(a_color.xyz, 0.0);\n"
														: "o_color += vec4(0.3, 0.2, 0.1, 0.0);\n";


	fragmentParams["CONSTANT_USAGE"]	=
		m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT	? "o_color += pc.color;\n"
														: "";

	dst.glslSources.add("vert") << glu::VertexSource(vertexCodeTemplate.specialize(vertexParams));
	dst.glslSources.add("frag") << glu::FragmentSource(fragmentCodeTemplate.specialize(fragmentParams));
}

TestInstance* BuiltinInputVariationsCase::createInstance (Context& context) const
{
	return new BuiltinInputVariationsCaseInstance(context, m_shaderInputTypes);
}

} // anonymous

TestCaseGroup* createBuiltinVarTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup> builtinGroup			(new TestCaseGroup(testCtx, "builtin_var"));
	de::MovePtr<TestCaseGroup> simpleGroup			(new TestCaseGroup(testCtx, "simple"));
	de::MovePtr<TestCaseGroup> inputVariationsGroup	(new TestCaseGroup(testCtx, "input_variations"));
	de::MovePtr<TestCaseGroup> frontFacingGroup		(new TestCaseGroup(testCtx, "frontfacing"));
	de::MovePtr<TestCaseGroup> fragDepthGroup		(new TestCaseGroup(testCtx, "fragdepth"));
	de::MovePtr<TestCaseGroup> fragCoordMsaaGroup	(new TestCaseGroup(testCtx, "fragcoord_msaa"));
	de::MovePtr<TestCaseGroup> fragCoordMsaaInputGroup	(new TestCaseGroup(testCtx, "fragcoord_msaa_input"));

	// FragCoord xyz test
	simpleGroup->addChild(new BuiltinGlFragCoordXYZCase(testCtx, "fragcoord_xyz"));
	// FragCoord w test
	simpleGroup->addChild(new BuiltinGlFragCoordWCase(testCtx, "fragcoord_w"));
	// PointCoord test
	simpleGroup->addChild(new BuiltinGlPointCoordCase(testCtx, "pointcoord", POINTCOORD_VARIANT_DEFAULT));
	// PointCoord test with fragment uniform
	simpleGroup->addChild(new BuiltinGlPointCoordCase(testCtx, "pointcoord_uniform_frag", POINTCOORD_VARIANT_UNIFORM_FRAGMENT));
	// PointCoord test with vertex uniform
	simpleGroup->addChild(new BuiltinGlPointCoordCase(testCtx, "pointcoord_uniform_vert", POINTCOORD_VARIANT_UNIFORM_VERTEX));

	// FragCoord_msaa
	{
		static const struct FragCoordMsaaCaseList
		{
			const char*				name;
			VkSampleCountFlagBits	sampleCount;
		} fragCoordMsaaCaseList[] =
		{
			{ "1_bit",	VK_SAMPLE_COUNT_1_BIT },
			{ "2_bit",	VK_SAMPLE_COUNT_2_BIT },
			{ "4_bit",	VK_SAMPLE_COUNT_4_BIT },
			{ "8_bit",	VK_SAMPLE_COUNT_8_BIT },
			{ "16_bit",	VK_SAMPLE_COUNT_16_BIT },
			{ "32_bit",	VK_SAMPLE_COUNT_32_BIT },
			{ "64_bit",	VK_SAMPLE_COUNT_64_BIT },
		};

		// Standard sample tests
		std::vector<uint32_t> sampleMaskArray;

		for (deUint32 caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(fragCoordMsaaCaseList); caseNdx++)
		{
			fragCoordMsaaGroup->addChild(new BuiltinFragCoordMsaaTestCase(testCtx, fragCoordMsaaCaseList[caseNdx].name, fragCoordMsaaCaseList[caseNdx].sampleCount, true, sampleMaskArray, false, true));
			fragCoordMsaaInputGroup->addChild(new BuiltinFragCoordMsaaTestCase(testCtx, fragCoordMsaaCaseList[caseNdx].name, fragCoordMsaaCaseList[caseNdx].sampleCount, true, sampleMaskArray, false, false));
		}

		sampleMaskArray.push_back(1u);

		// No sample shading tests
		for (deUint32 caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(fragCoordMsaaCaseList); caseNdx++)
		{
			fragCoordMsaaInputGroup->addChild(new BuiltinFragCoordMsaaTestCase(testCtx, (std::string(fragCoordMsaaCaseList[caseNdx].name) + "_no_sample_shading").c_str(), fragCoordMsaaCaseList[caseNdx].sampleCount, false, sampleMaskArray, false, false));
		}

		// No sample shading tests with centroid interpolation decoration
		for (deUint32 caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(fragCoordMsaaCaseList); caseNdx++)
		{
			fragCoordMsaaInputGroup->addChild(new BuiltinFragCoordMsaaTestCase(testCtx, (std::string(fragCoordMsaaCaseList[caseNdx].name) + "_no_sample_shading_centroid_interpolation").c_str(), fragCoordMsaaCaseList[caseNdx].sampleCount, false, sampleMaskArray, true, false));
		}
	}

	// gl_FrontFacing tests
	{
		static const struct PrimitiveTable
		{
			const char*				name;
			VkPrimitiveTopology		primitive;
		} frontfacingCases[] =
		{
			// Test that points are frontfacing
			{ "point_list",							VK_PRIMITIVE_TOPOLOGY_POINT_LIST },
			// Test that lines are frontfacing
			{ "line_list",							VK_PRIMITIVE_TOPOLOGY_LINE_LIST },
			// Test that triangles can be frontfacing or backfacing
			{ "triangle_list",						VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST },
			// Test that traiangle strips can be front or back facing
			{ "triangle_strip",						VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP },
			// Test that triangle fans can be front or back facing
			{ "triangle_fan",						VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN },
		};

		for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(frontfacingCases); ndx++)
			frontFacingGroup->addChild(new BuiltinGlFrontFacingCase(testCtx, frontfacingCases[ndx].primitive, frontfacingCases[ndx].name));
	}

	// gl_FragDepth
	{
		static const struct PrimitiveTopologyTable
		{
			std::string			name;
			VkPrimitiveTopology	prim;
		} primitiveTopologyTable[] =
		{
			// test that points respect gl_fragdepth
			{ "point_list", VK_PRIMITIVE_TOPOLOGY_POINT_LIST },
			// test taht lines respect gl_fragdepth
			{ "line_list", VK_PRIMITIVE_TOPOLOGY_LINE_LIST },
			// test that triangles respect gl_fragdepth
			{ "triangle_list", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP },
		};

		static const struct TestCaseTable
		{
			VkFormat				format;
			std::string				name;
			bool					largeDepthEnable;
			bool					depthClampEnable;
			VkSampleCountFlagBits	samples;
		} testCaseTable[] =
		{
			{ VK_FORMAT_D16_UNORM,				"d16_unorm_no_depth_clamp",				false,	false,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_X8_D24_UNORM_PACK32,	"x8_d24_unorm_pack32_no_depth_clamp",	false,	false,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat_no_depth_clamp",			false,	false,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_D16_UNORM_S8_UINT,		"d16_unorm_s8_uint_no_depth_clamp",		false,	false,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_D24_UNORM_S8_UINT,		"d24_unorm_s8_uint_no_depth_clamp",		false,	false,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_D32_SFLOAT_S8_UINT,		"d32_sfloat_s8_uint_no_depth_clamp",	false,	false,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat_large_depth",				true,	false,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat",							false,	true,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_D32_SFLOAT_S8_UINT,		"d32_sfloat_s8_uint",					false,	true,	VK_SAMPLE_COUNT_1_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat_multisample_2",				false,	false,	VK_SAMPLE_COUNT_2_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat_multisample_4",				false,	false,	VK_SAMPLE_COUNT_4_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat_multisample_8",				false,	false,	VK_SAMPLE_COUNT_8_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat_multisample_16",			false,	false,	VK_SAMPLE_COUNT_16_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat_multisample_32",			false,	false,	VK_SAMPLE_COUNT_32_BIT },
			{ VK_FORMAT_D32_SFLOAT,				"d32_sfloat_multisample_64",			false,	false,	VK_SAMPLE_COUNT_64_BIT },
		};

		for (deUint32 primNdx = 0;  primNdx < DE_LENGTH_OF_ARRAY(primitiveTopologyTable); primNdx++)
		{
			for (deUint32 caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(testCaseTable); caseNdx++)
				fragDepthGroup->addChild(new BuiltinFragDepthCase(testCtx, (primitiveTopologyTable[primNdx].name+"_" + testCaseTable[caseNdx].name).c_str(),
							primitiveTopologyTable[primNdx].prim, testCaseTable[caseNdx].format, testCaseTable[caseNdx].largeDepthEnable, testCaseTable[caseNdx].depthClampEnable, testCaseTable[caseNdx].samples));

		}
	}

	builtinGroup->addChild(frontFacingGroup.release());
	builtinGroup->addChild(fragDepthGroup.release());
	builtinGroup->addChild(fragCoordMsaaGroup.release());
	builtinGroup->addChild(fragCoordMsaaInputGroup.release());
	builtinGroup->addChild(simpleGroup.release());

	for (deUint16 shaderType = 0; shaderType <= (SHADER_INPUT_BUILTIN_BIT | SHADER_INPUT_VARYING_BIT | SHADER_INPUT_CONSTANT_BIT); ++shaderType)
	{
		inputVariationsGroup->addChild(new BuiltinInputVariationsCase(testCtx, shaderInputTypeToString(shaderType), shaderType));
	}

	builtinGroup->addChild(inputVariationsGroup.release());
	return builtinGroup.release();
}

} // sr
} // vkt
