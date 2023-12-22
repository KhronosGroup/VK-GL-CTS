/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 ARM Ltd.
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
 * \file
 * \brief Pipeline Cache and Pipeline Binaries Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineCacheTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPipelineBinaryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"
#include "tcuTestLog.hpp"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

// helper functions

std::string getShaderFlagStr (const VkShaderStageFlags	shader,
							  bool						isDescription)
{
	std::ostringstream desc;
	if (shader & VK_SHADER_STAGE_COMPUTE_BIT)
	{
		desc << ((isDescription) ? "compute stage" : "compute_stage");
	}
	else
	{
		desc << ((isDescription) ? "vertex stage" : "vertex_stage");
		if (shader & VK_SHADER_STAGE_GEOMETRY_BIT)
			desc << ((isDescription) ? " geometry stage" : "_geometry_stage");
		if (shader & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
			desc << ((isDescription) ? " tessellation control stage" : "_tessellation_control_stage");
		if (shader & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
			desc << ((isDescription) ? " tessellation evaluation stage" : "_tessellation_evaluation_stage");
		desc << ((isDescription) ? " fragment stage" : "_fragment_stage");
	}

	return desc.str();
}

enum class TestMode
{
	CACHE = 0,
	BINARIES
};

// helper classes
class TestParam
{
public:
								TestParam						(TestMode					mode,
																 PipelineConstructionType	pipelineConstructionType,
																 const VkShaderStageFlags	shaders,
																 bool						compileMissShaders = false,
																 VkPipelineCacheCreateFlags	pipelineCacheCreateFlags = 0u,
																 bool						useShaderStagesWithBinaries = true);
	virtual						~TestParam						(void) = default;
	virtual const std::string	generateTestName				(void)	const;
	TestMode					getMode							(void)	const	{ return m_mode; }
	PipelineConstructionType	getPipelineConstructionType		(void)	const	{ return m_pipelineConstructionType; };
	VkShaderStageFlags			getShaderFlags					(void)	const	{ return m_shaders; }
	VkPipelineCacheCreateFlags	getPipelineCacheCreateFlags		(void)	const	{ return m_pipelineCacheCreateFlags; }
	bool						getCompileMissShaders			(void)	const	{ return m_compileMissShaders; }
	bool						getUseShaderStagesWithBinaries	(void)	const	{ return m_useShaderStagesWithBinaries; }

protected:

	TestMode					m_mode;
	PipelineConstructionType	m_pipelineConstructionType;
	VkShaderStageFlags			m_shaders;
	bool						m_compileMissShaders;
	VkPipelineCacheCreateFlags	m_pipelineCacheCreateFlags;
	bool						m_useShaderStagesWithBinaries;
};

TestParam::TestParam (TestMode						mode,
					  PipelineConstructionType		pipelineConstructionType,
					  const VkShaderStageFlags		shaders,
					  bool							compileMissShaders,
					  VkPipelineCacheCreateFlags	pipelineCacheCreateFlags,
					  bool							useShaderStagesWithBinaries)
	: m_mode						(mode)
	, m_pipelineConstructionType	(pipelineConstructionType)
	, m_shaders						(shaders)
	, m_compileMissShaders			(compileMissShaders)
	, m_pipelineCacheCreateFlags	(pipelineCacheCreateFlags)
	, m_useShaderStagesWithBinaries	(useShaderStagesWithBinaries)
{
}

const std::string TestParam::generateTestName (void) const
{
	std::string name = getShaderFlagStr(m_shaders, false);
	if (m_pipelineCacheCreateFlags == VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT) {
		name += "_externally_synchronized";
	}
	return name;
}

template <class Test>
vkt::TestCase* newTestCase (tcu::TestContext&		testContext,
							const TestParam*	testParam)
{
	return new Test(testContext,
					testParam->generateTestName().c_str(),
					testParam);
}

Move<VkBuffer> createBufferAndBindMemory (Context& context, VkDeviceSize size, VkBufferUsageFlags usage, de::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	const VkBufferCreateInfo vertexBufferParams
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType      sType;
		DE_NULL,									// const void*          pNext;
		0u,											// VkBufferCreateFlags  flags;
		size,										// VkDeviceSize         size;
		usage,										// VkBufferUsageFlags   usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode        sharingMode;
		1u,											// deUint32             queueFamilyCount;
		&queueFamilyIndex							// const deUint32*      pQueueFamilyIndices;
	};

	Move<VkBuffer> vertexBuffer = createBuffer(vk, vkDevice, &vertexBufferParams);

	*pAlloc = context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	return vertexBuffer;
}

Move<VkImage> createImage2DAndBindMemory (Context&							context,
										  VkFormat							format,
										  deUint32							width,
										  deUint32							height,
										  VkImageUsageFlags					usage,
										  VkSampleCountFlagBits				sampleCount,
										  de::details::MovePtr<Allocation>*	pAlloc)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	const VkImageCreateInfo colorImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType      sType;
		DE_NULL,								// const void*          pNext;
		0u,										// VkImageCreateFlags   flags;
		VK_IMAGE_TYPE_2D,						// VkImageType          imageType;
		format,									// VkFormat             format;
		{ width, height, 1u },					// VkExtent3D           extent;
		1u,										// deUint32             mipLevels;
		1u,										// deUint32             arraySize;
		sampleCount,							// deUint32             samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling        tiling;
		usage,									// VkImageUsageFlags    usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode        sharingMode;
		1u,										// deUint32             queueFamilyCount;
		&queueFamilyIndex,						// const deUint32*      pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout        initialLayout;
	};

	Move<VkImage> image = createImage(vk, vkDevice, &colorImageParams);

	*pAlloc = context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *image, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	return image;
}

// Test Classes
class BaseTestCase : public vkt::TestCase
{
public:
	BaseTestCase						(tcu::TestContext&		testContext,
										 const std::string&		name,
										 const TestParam*		param)
							  : vkt::TestCase (testContext, name)
							  , m_param (*param)
							  { }
	virtual					~BaseTestCase (void) = default;
	virtual void			checkSupport  (Context& context) const;
protected:
	const TestParam	m_param;
};

class BaseTestInstance : public vkt::TestInstance
{
public:
	enum
	{
		PIPELINE_NDX_NO_BLOBS,
		PIPELINE_NDX_USE_BLOBS,
		PIPELINE_NDX_COUNT,
	};
							BaseTestInstance			(Context&				context,
														 const TestParam*		param);
	virtual					~BaseTestInstance			(void) = default;
	virtual tcu::TestStatus	iterate						(void);
protected:
	virtual tcu::TestStatus	verifyTestResult			(void) = 0;
	virtual void			prepareCommandBuffer		(void) = 0;

protected:
	const TestParam*		m_param;
	Move<VkCommandPool>		m_cmdPool;
	Move<VkCommandBuffer>	m_cmdBuffer;

	// cache is only used when m_mode is set to TestMode::CACHE
	Move<VkPipelineCache>	m_cache;

	// binary related structures are used when m_mode is set to TestMode::BINARIES
	de::MovePtr<PipelineBinaryWrapper> m_binaries;
};

void BaseTestCase::checkSupport (Context& context) const
{
	if (m_param.getMode() == TestMode::BINARIES)
		context.requireDeviceFunctionality("VK_KHR_pipeline_binary");
}

BaseTestInstance::BaseTestInstance (Context&				context,
									const TestParam*		param)
	: TestInstance	(context)
	, m_param		(param)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Create the Pipeline Cache
	if (m_param->getMode() == TestMode::CACHE)
	{
		const VkPipelineCacheCreateInfo pipelineCacheCreateInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
			DE_NULL,										// const void*                 pNext;
			m_param->getPipelineCacheCreateFlags(),			// VkPipelineCacheCreateFlags  flags;
			0u,												// deUintptr                   initialDataSize;
			DE_NULL,										// const void*                 pInitialData;
		};

		m_cache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
	}
	else
		m_binaries = de::MovePtr<PipelineBinaryWrapper>(new PipelineBinaryWrapper(vk, vkDevice));
}

tcu::TestStatus BaseTestInstance::iterate (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();
	const VkQueue			queue		= m_context.getUniversalQueue();

	prepareCommandBuffer();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyTestResult();
}

class GraphicsTest : public BaseTestCase
{
public:
							GraphicsTest		(tcu::TestContext&		testContext,
												 const std::string&		name,
												 const TestParam*		param)
								: BaseTestCase(testContext, name, param)
								{ }
	virtual					~GraphicsTest		(void) { }
	virtual void			initPrograms		(SourceCollections&		programCollection) const;
	virtual void			checkSupport		(Context&				context) const;
	virtual TestInstance*	createInstance		(Context&				context) const;
};

class GraphicsTestInstance : public BaseTestInstance
{
public:
							GraphicsTestInstance		(Context&				context,
														 const TestParam*		param);
	virtual					~GraphicsTestInstance		(void) = default;
protected:

			void			preparePipelineWrapper		(GraphicsPipelineWrapper&	gpw,
														 VkPipelineCache			cache,
														 bool						useMissShaders,
														 bool						noShaderStages,
														 VkPipelineBinaryInfoKHR*	monolithicBinaryInfo,
														 VkPipelineBinaryInfoKHR*	vertexPartBinaryInfo,
														 VkPipelineBinaryInfoKHR*	preRasterizationPartBinaryInfo,
														 VkPipelineBinaryInfoKHR*	fragmentShaderBinaryInfo,
														 VkPipelineBinaryInfoKHR*	fragmentOutputBinaryInfo);
	virtual void			preparePipelines			(void);
			void			preparePipelinesForBinaries	(bool createFromBlobs, bool incompleteBlobs, bool useShaderStages);
			void			prepareRenderPass			(const RenderPassWrapper& renderPassFramebuffer, GraphicsPipelineWrapper& pipeline);
	virtual void			prepareCommandBuffer		(void);
	virtual tcu::TestStatus	verifyTestResult			(void);

protected:
	const tcu::UVec2				m_renderSize;
	const VkFormat					m_colorFormat;
	const VkFormat					m_depthFormat;
	PipelineLayoutWrapper			m_pipelineLayout;

	Move<VkImage>					m_depthImage;
	de::MovePtr<Allocation>			m_depthImageAlloc;
	de::MovePtr<Allocation>			m_colorImageAlloc[PIPELINE_NDX_COUNT];
	Move<VkImageView>				m_depthAttachmentView;
	VkImageMemoryBarrier			m_imageLayoutBarriers[3];

	Move<VkBuffer>					m_vertexBuffer;
	de::MovePtr<Allocation>			m_vertexBufferMemory;
	std::vector<Vertex4RGBA>		m_vertices;

	GraphicsPipelineWrapper			m_pipeline[PIPELINE_NDX_COUNT];

	Move<VkImage>					m_colorImage[PIPELINE_NDX_COUNT];
	Move<VkImageView>				m_colorAttachmentView[PIPELINE_NDX_COUNT];
	RenderPassWrapper				m_renderPassFramebuffer[PIPELINE_NDX_COUNT];
};

void GraphicsTest::initPrograms (SourceCollections& programCollection) const
{
	enum ShaderCacheOpType
	{
		SHADERS_CACHE_OP_HIT = 0,
		SHADERS_CACHE_OP_MISS,

		SHADERS_CACHE_OP_LAST
	};

	for (deUint32 shaderOpNdx = 0u; shaderOpNdx < SHADERS_CACHE_OP_LAST; shaderOpNdx++)
	{
		const ShaderCacheOpType shaderOp = (ShaderCacheOpType)shaderOpNdx;

		if (shaderOp == SHADERS_CACHE_OP_MISS && !m_param.getCompileMissShaders())
			continue;

		const std::string missHitDiff = (shaderOp == SHADERS_CACHE_OP_HIT ? "" : " + 0.1");
		const std::string missSuffix = (shaderOp == SHADERS_CACHE_OP_HIT ? "" : "_miss");

		programCollection.glslSources.add("color_vert" + missSuffix) << glu::VertexSource(
			"#version 450\n"
			"layout(location = 0) in vec4 position;\n"
			"layout(location = 1) in vec4 color;\n"
			"layout(location = 0) out highp vec4 vtxColor;\n"
			"out gl_PerVertex { vec4 gl_Position; };\n"
			"void main (void)\n"
			"{\n"
			"  gl_Position = position;\n"
			"  vtxColor = color" + missHitDiff + ";\n"
			"}\n");

		programCollection.glslSources.add("color_frag" + missSuffix) << glu::FragmentSource(
			"#version 310 es\n"
			"layout(location = 0) in highp vec4 vtxColor;\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"void main (void)\n"
			"{\n"
			"  fragColor = vtxColor" + missHitDiff + ";\n"
			"}\n");

		VkShaderStageFlags shaderFlag = m_param.getShaderFlags();
		if (shaderFlag & VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			programCollection.glslSources.add("unused_geo" + missSuffix) << glu::GeometrySource(
				"#version 450 \n"
				"layout(triangles) in;\n"
				"layout(triangle_strip, max_vertices = 3) out;\n"
				"layout(location = 0) in highp vec4 in_vtxColor[];\n"
				"layout(location = 0) out highp vec4 vtxColor;\n"
				"out gl_PerVertex { vec4 gl_Position; };\n"
				"in gl_PerVertex { vec4 gl_Position; } gl_in[];\n"
				"void main (void)\n"
				"{\n"
				"  for(int ndx=0; ndx<3; ndx++)\n"
				"  {\n"
				"    gl_Position = gl_in[ndx].gl_Position;\n"
				"    vtxColor    = in_vtxColor[ndx]" + missHitDiff + ";\n"
				"    EmitVertex();\n"
				"  }\n"
				"  EndPrimitive();\n"
				"}\n");
		}
		if (shaderFlag & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		{
			programCollection.glslSources.add("basic_tcs" + missSuffix) << glu::TessellationControlSource(
				"#version 450 \n"
				"layout(vertices = 3) out;\n"
				"layout(location = 0) in highp vec4 color[];\n"
				"layout(location = 0) out highp vec4 vtxColor[];\n"
				"out gl_PerVertex { vec4 gl_Position; } gl_out[3];\n"
				"in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
				"void main()\n"
				"{\n"
				"  gl_TessLevelOuter[0] = 4.0;\n"
				"  gl_TessLevelOuter[1] = 4.0;\n"
				"  gl_TessLevelOuter[2] = 4.0;\n"
				"  gl_TessLevelInner[0] = 4.0;\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"  vtxColor[gl_InvocationID] = color[gl_InvocationID]" + missHitDiff + ";\n"
				"}\n");
		}
		if (shaderFlag & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		{
			programCollection.glslSources.add("basic_tes" + missSuffix) << glu::TessellationEvaluationSource(
				"#version 450 \n"
				"layout(triangles, fractional_even_spacing, ccw) in;\n"
				"layout(location = 0) in highp vec4 colors[];\n"
				"layout(location = 0) out highp vec4 vtxColor;\n"
				"out gl_PerVertex { vec4 gl_Position; };\n"
				"in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
				"void main() \n"
				"{\n"
				"  float u = gl_TessCoord.x;\n"
				"  float v = gl_TessCoord.y;\n"
				"  float w = gl_TessCoord.z;\n"
				"  vec4 pos = vec4(0);\n"
				"  vec4 color = vec4(0)" + missHitDiff + ";\n"
				"  pos.xyz += u * gl_in[0].gl_Position.xyz;\n"
				"  color.xyz += u * colors[0].xyz;\n"
				"  pos.xyz += v * gl_in[1].gl_Position.xyz;\n"
				"  color.xyz += v * colors[1].xyz;\n"
				"  pos.xyz += w * gl_in[2].gl_Position.xyz;\n"
				"  color.xyz += w * colors[2].xyz;\n"
				"  pos.w = 1.0;\n"
				"  color.w = 1.0;\n"
				"  gl_Position = pos;\n"
				"  vtxColor = color;\n"
				"}\n");
		}
	}
}

void GraphicsTest::checkSupport (Context& context) const
{
	if (m_param.getMode() == TestMode::BINARIES)
		context.requireDeviceFunctionality("VK_KHR_pipeline_binary");

	if (m_param.getShaderFlags() & VK_SHADER_STAGE_GEOMETRY_BIT)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	if ((m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
		(m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_param.getPipelineConstructionType());
}

TestInstance* GraphicsTest::createInstance (Context& context) const
{
	return new GraphicsTestInstance(context, &m_param);
}

GraphicsTestInstance::GraphicsTestInstance (Context&			context,
											const TestParam*	param)
	: BaseTestInstance		(context, param)
	, m_renderSize			(32u, 32u)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthFormat			(VK_FORMAT_D16_UNORM)
	, m_pipeline
	{
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), param->getPipelineConstructionType() },
		{ context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), param->getPipelineConstructionType() },
	}
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	if (param->getMode() == TestMode::BINARIES)
	{
		m_pipeline[PIPELINE_NDX_NO_BLOBS].setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR);
		m_pipeline[PIPELINE_NDX_USE_BLOBS].setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR);
	}

	// Create vertex buffer
	{
		m_vertexBuffer	= createBufferAndBindMemory(m_context, 1024u, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &m_vertexBufferMemory);

		m_vertices		= createOverlappingQuads();
		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferMemory->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(vk, vkDevice, *m_vertexBufferMemory);
	}

	// Create render pass
	m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS] = RenderPassWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, m_colorFormat, m_depthFormat);
	m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS] = RenderPassWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, m_colorFormat, m_depthFormat);

	const VkComponentMapping ComponentMappingRGBA = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	// Create color image
	{
		m_colorImage[PIPELINE_NDX_NO_BLOBS]		= createImage2DAndBindMemory(m_context,
																			 m_colorFormat,
																			 m_renderSize.x(),
																			 m_renderSize.y(),
																			 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																			 VK_SAMPLE_COUNT_1_BIT,
																			 &m_colorImageAlloc[PIPELINE_NDX_NO_BLOBS]);
		m_colorImage[PIPELINE_NDX_USE_BLOBS]	= createImage2DAndBindMemory(m_context,
																			 m_colorFormat,
																			 m_renderSize.x(),
																			 m_renderSize.y(),
																			 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																			 VK_SAMPLE_COUNT_1_BIT,
																			 &m_colorImageAlloc[PIPELINE_NDX_USE_BLOBS]);
	}

	// Create depth image
	{
		m_depthImage = createImage2DAndBindMemory(m_context,
												  m_depthFormat,
												  m_renderSize.x(),
												  m_renderSize.y(),
												  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
												  VK_SAMPLE_COUNT_1_BIT,
												  &m_depthImageAlloc);
	}

	// Set up image layout transition barriers
	{
		VkImageMemoryBarrier colorImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkAccessFlags			srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					dstQueueFamilyIndex;
			*m_colorImage[PIPELINE_NDX_NO_BLOBS],				// VkImage					image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },		// VkImageSubresourceRange	subresourceRange;
		};

		m_imageLayoutBarriers[0] = colorImageBarrier;

		colorImageBarrier.image = *m_colorImage[PIPELINE_NDX_USE_BLOBS];
		m_imageLayoutBarriers[1] = colorImageBarrier;

		const VkImageMemoryBarrier depthImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkAccessFlags			srcAccessMask;
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					dstQueueFamilyIndex;
			*m_depthImage,										// VkImage					image;
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u },		// VkImageSubresourceRange	subresourceRange;
		};

		m_imageLayoutBarriers[2] = depthImageBarrier;
	}
	// Create color attachment view
	{
		VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType          sType;
			DE_NULL,										// const void*              pNext;
			0u,												// VkImageViewCreateFlags   flags;
			*m_colorImage[PIPELINE_NDX_NO_BLOBS],			// VkImage                  image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType          viewType;
			m_colorFormat,									// VkFormat                 format;
			ComponentMappingRGBA,							// VkComponentMapping       components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange  subresourceRange;
		};

		m_colorAttachmentView[PIPELINE_NDX_NO_BLOBS] = createImageView(vk, vkDevice, &colorAttachmentViewParams);

		colorAttachmentViewParams.image = *m_colorImage[PIPELINE_NDX_USE_BLOBS];
		m_colorAttachmentView[PIPELINE_NDX_USE_BLOBS] = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create depth attachment view
	{
		const VkImageViewCreateInfo depthAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType          sType;
			DE_NULL,										// const void*              pNext;
			0u,												// VkImageViewCreateFlags   flags;
			*m_depthImage,									// VkImage                  image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType          viewType;
			m_depthFormat,									// VkFormat                 format;
			ComponentMappingRGBA,							// VkComponentMapping       components;
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange  subresourceRange;
		};

		m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
	}

	// Create framebuffer
	{
		std::vector<VkImage> images = {
			*m_colorImage[PIPELINE_NDX_NO_BLOBS],
			*m_depthImage,
		};
		VkImageView attachmentBindInfos[2] =
		{
			*m_colorAttachmentView[PIPELINE_NDX_NO_BLOBS],
			*m_depthAttachmentView,
		};

		VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				// VkStructureType              sType;
			DE_NULL,												// const void*                  pNext;
			0u,														// VkFramebufferCreateFlags     flags;
			*m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS],		// VkRenderPass                 renderPass;
			2u,														// deUint32                     attachmentCount;
			attachmentBindInfos,									// const VkImageView*           pAttachments;
			(deUint32)m_renderSize.x(),								// deUint32                     width;
			(deUint32)m_renderSize.y(),								// deUint32                     height;
			1u,														// deUint32                     layers;
		};

		m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS].createFramebuffer(vk, vkDevice, &framebufferParams, images);

		framebufferParams.renderPass = *m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS];
		images[0] = *m_colorImage[PIPELINE_NDX_USE_BLOBS];
		attachmentBindInfos[0] = *m_colorAttachmentView[PIPELINE_NDX_USE_BLOBS];
		m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS].createFramebuffer(vk, vkDevice, &framebufferParams, images);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkPipelineLayoutCreateFlags		flags;
			0u,												// deUint32							setLayoutCount;
			DE_NULL,										// const VkDescriptorSetLayout*		pSetLayouts;
			0u,												// deUint32							pushConstantRangeCount;
			DE_NULL											// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, &pipelineLayoutParams);
	}
}

void GraphicsTestInstance::preparePipelineWrapper (GraphicsPipelineWrapper&	gpw,
												   VkPipelineCache			cache = DE_NULL,
												   bool						useMissShaders = false,
												   bool						useShaderStages = true,
												   VkPipelineBinaryInfoKHR* monolithicBinaryInfo = DE_NULL,
												   VkPipelineBinaryInfoKHR* vertexPartBinaryInfo = DE_NULL,
												   VkPipelineBinaryInfoKHR* preRasterizationPartBinaryInfo = DE_NULL,
												   VkPipelineBinaryInfoKHR* fragmentShaderBinaryInfo = DE_NULL,
												   VkPipelineBinaryInfoKHR* fragmentOutputBinaryInfo = DE_NULL)
{
	static const VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,														// VkBool32									depthTestEnable;
		VK_TRUE,														// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,									// VkCompareOp								depthCompareOp;
		VK_FALSE,														// VkBool32									depthBoundsTestEnable;
		VK_FALSE,														// VkBool32									stencilTestEnable;
		{																// VkStencilOpState		front;
			VK_STENCIL_OP_KEEP,												// VkStencilOp		failOp;
			VK_STENCIL_OP_KEEP,												// VkStencilOp		passOp;
			VK_STENCIL_OP_KEEP,												// VkStencilOp		depthFailOp;
			VK_COMPARE_OP_NEVER,											// VkCompareOp		compareOp;
			0u,																// deUint32			compareMask;
			0u,																// deUint32			writeMask;
			0u,																// deUint32			reference;
		},
		{																// VkStencilOpState		back;
			VK_STENCIL_OP_KEEP,												// VkStencilOp		failOp;
			VK_STENCIL_OP_KEEP,												// VkStencilOp		passOp;
			VK_STENCIL_OP_KEEP,												// VkStencilOp		depthFailOp;
			VK_COMPARE_OP_NEVER,											// VkCompareOp		compareOp;
			0u,																// deUint32			compareMask;
			0u,																// deUint32			writeMask;
			0u,																// deUint32			reference;
		},
		0.0f,															// float									minDepthBounds;
		1.0f,															// float									maxDepthBounds;
	};

	static const VkVertexInputBindingDescription defaultVertexInputBindingDescription
	{
		0u,																// deUint32					binding;
		sizeof(Vertex4RGBA),											// deUint32					strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX,									// VkVertexInputRate		inputRate;
	};

	static const VkVertexInputAttributeDescription defaultVertexInputAttributeDescriptions[]
	{
		{
			0u,															// deUint32					location;
			0u,															// deUint32					binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,								// VkFormat					format;
			0u															// deUint32					offsetInBytes;
		},
		{
			1u,															// deUint32					location;
			0u,															// deUint32					binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,								// VkFormat					format;
			DE_OFFSET_OF(Vertex4RGBA, color),							// deUint32					offsetInBytes;
		}
	};

	static const VkPipelineVertexInputStateCreateInfo defaultVertexInputStateParams
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags		flags;
		1u,																// deUint32										vertexBindingDescriptionCount;
		&defaultVertexInputBindingDescription,							// const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		2u,																// deUint32										vertexAttributeDescriptionCount;
		defaultVertexInputAttributeDescriptions,						// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions;
	};

	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();
	const std::string		postfix		= useMissShaders ? "_miss" : "";

	auto createModule = [&vk, vkDevice, &postfix](Context& context, std::string shaderName)
	{
		return ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get(shaderName + postfix), 0);
	};

	// Bind shader stages
	ShaderWrapper vertShaderModule = createModule(m_context, "color_vert");
	ShaderWrapper fragShaderModule = createModule(m_context, "color_frag");
	ShaderWrapper tescShaderModule;
	ShaderWrapper teseShaderModule;
	ShaderWrapper geomShaderModule;

	if (useShaderStages)
	{
		vertShaderModule = createModule(m_context, "color_vert");
		fragShaderModule = createModule(m_context, "color_frag");

		if (m_param->getShaderFlags() & VK_SHADER_STAGE_GEOMETRY_BIT)
			geomShaderModule = createModule(m_context, "unused_geo");
		if (m_param->getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
			tescShaderModule = createModule(m_context, "basic_tcs");
		if (m_param->getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
			teseShaderModule = createModule(m_context, "basic_tes");
	}

	const std::vector<VkViewport>	viewport	{ makeViewport(m_renderSize) };
	const std::vector<VkRect2D>		scissor		{ makeRect2D(m_renderSize) };

	gpw.setDefaultTopology((m_param->getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
						   ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.setDefaultRasterizationState()
		.setDefaultColorBlendState()
		.setDefaultMultisampleState()
		.setMonolithicPipelineLayout(m_pipelineLayout)
		.setMonolithicPipelineBinaries(monolithicBinaryInfo)
		.setupVertexInputState(&defaultVertexInputStateParams, 0, 0, 0, vertexPartBinaryInfo)
		.setupPreRasterizationShaderState3(viewport,
										  scissor,
										  m_pipelineLayout,
										  *m_renderPassFramebuffer[0],
										  0u,
										  vertShaderModule, 0,
										  DE_NULL,
										  tescShaderModule, 0,
										  teseShaderModule, 0,
										  geomShaderModule, 0,
										  0, 0, 0, 0, 0, 0, 0, 0, preRasterizationPartBinaryInfo)
		.setupFragmentShaderState2(m_pipelineLayout, *m_renderPassFramebuffer[0], 0u, fragShaderModule, 0, &defaultDepthStencilState,
								   0, 0, 0, 0, fragmentShaderBinaryInfo)
		.setupFragmentOutputState(*m_renderPassFramebuffer[0], 0, 0, 0, 0, 0, fragmentOutputBinaryInfo)
		.buildPipeline(cache);
}

void GraphicsTestInstance::preparePipelinesForBinaries (bool createFromBlobs = false, bool incompleteBlobs = false, bool useShaderStages = true)
{
	DE_ASSERT(m_param->getMode() == TestMode::BINARIES);

	preparePipelineWrapper(m_pipeline[PIPELINE_NDX_NO_BLOBS]);

	if (m_param->getPipelineConstructionType() != PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
	{
		const auto& pipelineCreateInfo = m_pipeline[PIPELINE_NDX_NO_BLOBS].getPipelineCreateInfo();
		m_binaries->generatePipelineBinaryKeys(&pipelineCreateInfo);

		if (createFromBlobs)
		{
			// read binaries data out of the device
			std::vector<VkPipelineBinaryDataKHR>	pipelineDataInfo;
			std::vector<std::vector<uint8_t> >		pipelineDataBlob;
			m_binaries->getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

			// clear pipeline binaries objects
			m_binaries->deletePipelineBinariesKeepKeys();

			// simulate incomplete data by modifying size
			if (incompleteBlobs)
			{
				for (auto& bd : pipelineDataInfo)
					--bd.size;
			}

			// recreate binaries from data blobs
			m_binaries->createPipelineBinariesFromBinaryData(pipelineDataInfo);
		}
		else
			m_binaries->createPipelineBinariesFromPipeline(m_pipeline[PIPELINE_NDX_NO_BLOBS].getPipeline());

		VkPipelineBinaryInfoKHR pipelineBinaryInfo = m_binaries->preparePipelineBinaryInfo();
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_USE_BLOBS], DE_NULL, false, useShaderStages, &pipelineBinaryInfo);
	}
	else
	{
		// grab keys from all pipeline parts separately
		deUint32 startingKey[5];
		for (deUint32 idx = 0; idx < 4; ++idx)
		{
			startingKey[idx] = static_cast<deUint32>(m_binaries->getKeyCount());
			const auto& pipelinePartCreateInfo = m_pipeline[PIPELINE_NDX_NO_BLOBS].getPartialPipelineCreateInfo(idx);
			m_binaries->generatePipelineBinaryKeys(&pipelinePartCreateInfo, false);
		}

		// add aditional element to avoid if statement in next loop
		startingKey[4] = static_cast<deUint32>(m_binaries->getKeyCount());

		if (createFromBlobs)
		{
			// read binaries data out of the device
			std::vector<VkPipelineBinaryDataKHR>	pipelineDataInfo;
			std::vector<std::vector<uint8_t> >		pipelineDataBlob;
			m_binaries->getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

			// clear pipeline binaries objects
			m_binaries->deletePipelineBinariesKeepKeys();

			// simulate incomplete data by modifying size
			if (incompleteBlobs)
			{
				for (auto& bd : pipelineDataInfo)
					--bd.size;
			}

			// recreate binaries from data blobs
			m_binaries->createPipelineBinariesFromBinaryData(pipelineDataInfo);
		}
		else
			m_binaries->createPipelineBinariesFromPipeline(m_pipeline[PIPELINE_NDX_NO_BLOBS].getPipeline());

		// use proper keys for each pipeline part
		VkPipelineBinaryInfoKHR pipelinePartBinaryInfo[4];
		for (deUint32 idx = 0; idx < 4; ++idx)
		{
			deUint32 binaryCount = startingKey[idx + 1] - startingKey[idx];
			pipelinePartBinaryInfo[idx] = m_binaries->preparePipelineBinaryInfo(startingKey[idx], binaryCount);
		}

		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_USE_BLOBS], DE_NULL, false,
			useShaderStages, DE_NULL, &pipelinePartBinaryInfo[0],
			&pipelinePartBinaryInfo[1], &pipelinePartBinaryInfo[2], &pipelinePartBinaryInfo[3]);
	}
}

void GraphicsTestInstance::preparePipelines (void)
{
	if (m_param->getMode() == TestMode::CACHE)
	{
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_NO_BLOBS], *m_cache);
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_USE_BLOBS], *m_cache);
	}
	else
		preparePipelinesForBinaries();
}

void GraphicsTestInstance::prepareRenderPass (const RenderPassWrapper& renderPassFramebuffer, GraphicsPipelineWrapper& pipeline)
{
	const DeviceInterface&	vk							= m_context.getDeviceInterface();

	const VkClearValue		attachmentClearValues[2]
	{
		defaultClearValue(m_colorFormat),
		defaultClearValue(m_depthFormat),
	};

	renderPassFramebuffer.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u, attachmentClearValues);

	pipeline.bind(*m_cmdBuffer);
	VkDeviceSize offsets = 0u;
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &offsets);
	vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1u, 0u, 0u);

	renderPassFramebuffer.end(vk, *m_cmdBuffer);
}

void GraphicsTestInstance::prepareCommandBuffer (void)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	preparePipelines();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
		0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(m_imageLayoutBarriers), m_imageLayoutBarriers);

	prepareRenderPass(m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS], m_pipeline[PIPELINE_NDX_NO_BLOBS]);

	// After the first render pass, the images are in correct layouts

	prepareRenderPass(m_renderPassFramebuffer[PIPELINE_NDX_USE_BLOBS], m_pipeline[PIPELINE_NDX_USE_BLOBS]);

	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus GraphicsTestInstance::verifyTestResult (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					vkDevice			= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const VkQueue					queue				= m_context.getUniversalQueue();
	de::MovePtr<tcu::TextureLevel>	resultNoCache		= readColorAttachment(vk,
																			  vkDevice,
																			  queue,
																			  queueFamilyIndex,
																			  m_context.getDefaultAllocator(),
																			  *m_colorImage[PIPELINE_NDX_NO_BLOBS],
																			  m_colorFormat,
																			  m_renderSize);
	de::MovePtr<tcu::TextureLevel>	resultCache			= readColorAttachment(vk,
																			  vkDevice,
																			  queue,
																			  queueFamilyIndex,
																			  m_context.getDefaultAllocator(),
																			  *m_colorImage[PIPELINE_NDX_USE_BLOBS],
																			  m_colorFormat,
																			  m_renderSize);

	bool compareOk = tcu::intThresholdCompare(m_context.getTestContext().getLog(),
											  "IntImageCompare",
											  "Image comparison",
											  resultNoCache->getAccess(),
											  resultCache->getAccess(),
											  tcu::UVec4(1, 1, 1, 1),
											  tcu::COMPARE_LOG_RESULT);

	if (compareOk)
		return tcu::TestStatus::pass("Render images w/o cached pipeline match.");
	else
		return tcu::TestStatus::fail("Render Images mismatch.");
}

class ComputeTest : public BaseTestCase
{
public:
							ComputeTest			(tcu::TestContext&		testContext,
												 const std::string&		name,
												 const TestParam*		param)
								: BaseTestCase (testContext, name, param)
								{ }
	virtual					~ComputeTest		(void) = default;
	virtual void			initPrograms		(SourceCollections&		programCollection) const;
	virtual TestInstance*	createInstance		(Context&				context) const;
};

class ComputeTestInstance : public BaseTestInstance
{
public:
							ComputeTestInstance			(Context&			context,
														 const TestParam*	param);
	virtual					~ComputeTestInstance		(void) = default;
	virtual void			prepareCommandBuffer		(void);
protected:
	virtual tcu::TestStatus	verifyTestResult			(void);
			void			buildBuffers				(void);
			void			buildDescriptorSets			(deUint32 ndx);
			void			buildShader					(void);
			void			buildPipeline				(deUint32 ndx);
protected:
	Move<VkBuffer>				m_inputBuf;
	de::MovePtr<Allocation>		m_inputBufferAlloc;
	Move<VkShaderModule>		m_computeShaderModule;

	Move<VkBuffer>				m_outputBuf[PIPELINE_NDX_COUNT];
	de::MovePtr<Allocation>		m_outputBufferAlloc[PIPELINE_NDX_COUNT];

	Move<VkDescriptorPool>		m_descriptorPool[PIPELINE_NDX_COUNT];
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout[PIPELINE_NDX_COUNT];
	Move<VkDescriptorSet>		m_descriptorSet[PIPELINE_NDX_COUNT];

	Move<VkPipelineLayout>		m_pipelineLayout[PIPELINE_NDX_COUNT];
	Move<VkPipeline>			m_pipeline[PIPELINE_NDX_COUNT];
};

void ComputeTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("basic_compute") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(local_size_x = 1) in;\n"
		"layout(std430) buffer;\n"
		"layout(binding = 0) readonly buffer Input0\n"
		"{\n"
		"  vec4 elements[];\n"
		"} input_data0;\n"
		"layout(binding = 1) writeonly buffer Output\n"
		"{\n"
		"  vec4 elements[];\n"
		"} output_data;\n"
		"void main()\n"
		"{\n"
		"  uint ident = gl_GlobalInvocationID.x;\n"
		"  output_data.elements[ident] = input_data0.elements[ident] * input_data0.elements[ident];\n"
		"}");
}

TestInstance* ComputeTest::createInstance (Context& context) const
{
	return new ComputeTestInstance(context, &m_param);
}

void ComputeTestInstance::buildBuffers (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create buffer object, allocate storage, and generate input data
	const VkDeviceSize		size		= sizeof(tcu::Vec4) * 128u;
	m_inputBuf = createBufferAndBindMemory(m_context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_inputBufferAlloc);

	// Initialize input buffer
	tcu::Vec4* pVec = reinterpret_cast<tcu::Vec4*>(m_inputBufferAlloc->getHostPtr());
	for (deUint32 ndx = 0u; ndx < 128u; ndx++)
	{
		for (deUint32 component = 0u; component < 4u; component++)
			pVec[ndx][component]= (float)(ndx * (component + 1u));
	}
	flushAlloc(vk, vkDevice, *m_inputBufferAlloc);

	// Clear the output buffer
	for (deUint32 ndx = 0; ndx < PIPELINE_NDX_COUNT; ndx++)
	{
		m_outputBuf[ndx] = createBufferAndBindMemory(m_context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &m_outputBufferAlloc[ndx]);

		pVec = reinterpret_cast<tcu::Vec4*>(m_outputBufferAlloc[ndx]->getHostPtr());

		for (deUint32 i = 0; i < (size / sizeof(tcu::Vec4)); i++)
			pVec[i] = tcu::Vec4(0.0f);

		flushAlloc(vk, vkDevice, *m_outputBufferAlloc[ndx]);
	}
}

void ComputeTestInstance::buildDescriptorSets (deUint32 ndx)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create descriptor set layout
	DescriptorSetLayoutBuilder descLayoutBuilder;

	for (deUint32 bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
		descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

	m_descriptorSetLayout[ndx] = descLayoutBuilder.build(vk, vkDevice);

	std::vector<VkDescriptorBufferInfo> descriptorInfos;
	descriptorInfos.push_back(makeDescriptorBufferInfo(*m_inputBuf, 0u, sizeof(tcu::Vec4) * 128u));
	descriptorInfos.push_back(makeDescriptorBufferInfo(*m_outputBuf[ndx], 0u, sizeof(tcu::Vec4) * 128u));

	// Create descriptor pool
	m_descriptorPool[ndx] = DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u).build(vk,
																										 vkDevice,
																										 VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
																										 1u);

	// Create descriptor set
	const VkDescriptorSetAllocateInfo descriptorSetAllocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		*m_descriptorPool[ndx],							// VkDescriptorPool                descriptorPool;
		1u,												// deUint32                        setLayoutCount;
		&m_descriptorSetLayout[ndx].get(),				// const VkDescriptorSetLayout*    pSetLayouts;
	};
	m_descriptorSet[ndx] = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocInfo);

	DescriptorSetUpdateBuilder  builder;
	for (deUint32 descriptorNdx = 0u; descriptorNdx < 2u; descriptorNdx++)
	{
		builder.writeSingle(*m_descriptorSet[ndx],
							DescriptorSetUpdateBuilder::Location::binding(descriptorNdx),
							VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							&descriptorInfos[descriptorNdx]);
	}
	builder.update(vk, vkDevice);
}

void ComputeTestInstance::buildShader (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create compute shader
	VkShaderModuleCreateInfo shaderModuleCreateInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,									// VkStructureType             sType;
		DE_NULL,																		// const void*                 pNext;
		0u,																				// VkShaderModuleCreateFlags   flags;
		m_context.getBinaryCollection().get("basic_compute").getSize(),					// deUintptr                   codeSize;
		(deUint32*)m_context.getBinaryCollection().get("basic_compute").getBinary(),	// const deUint32*             pCode;
	};
	m_computeShaderModule = createShaderModule(vk, vkDevice, &shaderModuleCreateInfo);
}

void ComputeTestInstance::buildPipeline (deUint32 ndx)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create compute pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		0u,												// VkPipelineLayoutCreateFlags     flags;
		1u,												// deUint32                        setLayoutCount;
		&m_descriptorSetLayout[ndx].get(),				// const VkDescriptorSetLayout*    pSetLayouts;
		0u,												// deUint32                        pushConstantRangeCount;
		DE_NULL,										// const VkPushConstantRange*      pPushConstantRanges;
	};

	m_pipelineLayout[ndx] = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

	const VkPipelineShaderStageCreateInfo stageCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
		DE_NULL,												// const void*                         pNext;
		0u,														// VkPipelineShaderStageCreateFlags    flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits               stage;
		*m_computeShaderModule,									// VkShaderModule                      module;
		"main",													// const char*                         pName;
		DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
	};

	VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = initVulkanStructure();
	if (m_param->getMode() == TestMode::BINARIES)
		pipelineFlags2CreateInfo.flags = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;
	VkComputePipelineCreateInfo pipelineCreateInfo
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType						sType;
		&pipelineFlags2CreateInfo,								// const void*							pNext;
		0,														// VkPipelineCreateFlags				flags;
		stageCreateInfo,										// VkPipelineShaderStageCreateInfo		stage;
		*m_pipelineLayout[ndx],									// VkPipelineLayout						layout;
		(VkPipeline)0,											// VkPipeline							basePipelineHandle;
		0u,														// deInt32								basePipelineIndex;
	};

	if (m_param->getMode() == TestMode::CACHE)
		m_pipeline[ndx] = createComputePipeline(vk, vkDevice, *m_cache, &pipelineCreateInfo);
	else
	{
		if (ndx == PIPELINE_NDX_NO_BLOBS)
		{
			// create pipeline
			m_pipeline[ndx] = createComputePipeline(vk, vkDevice, DE_NULL, &pipelineCreateInfo);

			m_binaries->generatePipelineBinaryKeys(&pipelineCreateInfo);

			// prepare pipeline binaries
			m_binaries->createPipelineBinariesFromPipeline(*m_pipeline[ndx]);
		}
		else
		{
			// create pipeline using binary data and use pipelineCreateInfo with no shader stage
			VkPipelineBinaryInfoKHR pipelineBinaryInfo = m_binaries->preparePipelineBinaryInfo();
			pipelineCreateInfo.pNext = &pipelineBinaryInfo;
			pipelineCreateInfo.stage.module = 0;
			m_pipeline[ndx] = createComputePipeline(vk, vkDevice, DE_NULL, &pipelineCreateInfo);
		}
	}
}

ComputeTestInstance::ComputeTestInstance (Context&			context,
										  const TestParam*	param)
	: BaseTestInstance (context, param)
{
	buildBuffers();

	buildDescriptorSets(PIPELINE_NDX_NO_BLOBS);

	buildDescriptorSets(PIPELINE_NDX_USE_BLOBS);

	buildShader();

	buildPipeline(PIPELINE_NDX_NO_BLOBS);

	buildPipeline(PIPELINE_NDX_USE_BLOBS);
}

void ComputeTestInstance::prepareCommandBuffer (void)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	for (deUint32 ndx = 0; ndx < PIPELINE_NDX_COUNT; ndx++)
	{
		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline[ndx]);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout[ndx], 0u, 1u, &m_descriptorSet[ndx].get(), 0u, DE_NULL);
		vk.cmdDispatch(*m_cmdBuffer, 128u, 1u, 1u);
	}

	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus ComputeTestInstance::verifyTestResult (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Read the content of output buffers
	invalidateAlloc(vk, vkDevice, *m_outputBufferAlloc[PIPELINE_NDX_NO_BLOBS]);

	invalidateAlloc(vk, vkDevice, *m_outputBufferAlloc[PIPELINE_NDX_USE_BLOBS]);
	// Compare the content
	deUint8* bufNoCache = reinterpret_cast<deUint8*>(m_outputBufferAlloc[PIPELINE_NDX_NO_BLOBS]->getHostPtr());
	deUint8* bufCached  = reinterpret_cast<deUint8*>(m_outputBufferAlloc[PIPELINE_NDX_USE_BLOBS]->getHostPtr());
	for (deUint32 ndx = 0u; ndx < sizeof(tcu::Vec4) * 128u; ndx++)
	{
		if (bufNoCache[ndx] != bufCached[ndx])
		{
			return tcu::TestStatus::fail("Output buffers w/o pipeline blobs mismatch.");
		}
	}

	return tcu::TestStatus::pass("Output buffers w/o pipeline blobs match.");
}

class PipelineFromBlobsTest : public GraphicsTest
{
public:
							PipelineFromBlobsTest		(tcu::TestContext& testContext, const std::string& name, const TestParam* param);
	virtual					~PipelineFromBlobsTest		(void) = default;
	virtual TestInstance*	createInstance				(Context& context) const;
};

PipelineFromBlobsTest::PipelineFromBlobsTest (tcu::TestContext& testContext, const std::string& name, const TestParam* param)
	: GraphicsTest(testContext, name, param)
{
}

class PipelineFromBlobsTestInstance : public GraphicsTestInstance
{
public:
							PipelineFromBlobsTestInstance	(Context& context, const TestParam* param);
	virtual					~PipelineFromBlobsTestInstance	(void);

protected:
	void					preparePipelines(void);

protected:
	Move<VkPipelineCache>	m_newCache;
	deUint8*				m_data;
};

TestInstance* PipelineFromBlobsTest::createInstance (Context& context) const
{
	return new PipelineFromBlobsTestInstance(context, &m_param);
}

PipelineFromBlobsTestInstance::PipelineFromBlobsTestInstance (Context& context, const TestParam* param)
	: GraphicsTestInstance	(context, param)
	, m_data				(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	if (m_param->getMode() == TestMode::CACHE)
	{
		size_t dataSize	= 0u;

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		m_data = new deUint8[dataSize];
		DE_ASSERT(m_data);
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data));

		const VkPipelineCacheCreateInfo pipelineCacheCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
			DE_NULL,										// const void*                 pNext;
			0u,												// VkPipelineCacheCreateFlags  flags;
			dataSize,										// deUintptr                   initialDataSize;
			m_data,											// const void*                 pInitialData;
		};
		m_newCache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
	}
}

PipelineFromBlobsTestInstance::~PipelineFromBlobsTestInstance(void)
{
	delete[] m_data;
}

void PipelineFromBlobsTestInstance::preparePipelines (void)
{
	if (m_param->getMode() == TestMode::CACHE)
	{
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_NO_BLOBS], *m_cache);
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_USE_BLOBS], *m_newCache);
	}
	else
		preparePipelinesForBinaries(true, false, m_param->getUseShaderStagesWithBinaries());
}

class PipelineFromIncompleteBlobsTest : public GraphicsTest
{
public:
							PipelineFromIncompleteBlobsTest		(tcu::TestContext& testContext, const std::string& name, const TestParam* param);
	virtual					~PipelineFromIncompleteBlobsTest	(void) = default;
	virtual TestInstance*	createInstance						(Context& context) const;
};

PipelineFromIncompleteBlobsTest::PipelineFromIncompleteBlobsTest (tcu::TestContext& testContext, const std::string& name, const TestParam* param)
	: GraphicsTest(testContext, name, param)
{
}

class PipelineFromIncompleteBlobsTestInstance : public GraphicsTestInstance
{
public:
							PipelineFromIncompleteBlobsTestInstance	(Context& context, const TestParam* param);
	virtual					~PipelineFromIncompleteBlobsTestInstance(void);
protected:
	void					preparePipelines(void);
protected:
	Move<VkPipelineCache>	m_newCache;
	deUint8*				m_data;
};

TestInstance* PipelineFromIncompleteBlobsTest::createInstance (Context& context) const
{
	return new PipelineFromIncompleteBlobsTestInstance(context, &m_param);
}

PipelineFromIncompleteBlobsTestInstance::PipelineFromIncompleteBlobsTestInstance (Context& context, const TestParam* param)
	: GraphicsTestInstance		(context, param)
	, m_data					(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	if (m_param->getMode() == TestMode::CACHE)
	{
		size_t dataSize = 0u;
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		if (dataSize == 0)
			TCU_THROW(NotSupportedError, "Empty pipeline cache - unable to test");

		dataSize--;

		m_data = new deUint8[dataSize];
		DE_ASSERT(m_data);
		if (vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data) != VK_INCOMPLETE)
			TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

		const VkPipelineCacheCreateInfo pipelineCacheCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
			DE_NULL,										// const void*                 pNext;
			0u,												// VkPipelineCacheCreateFlags  flags;
			dataSize,										// deUintptr                   initialDataSize;
			m_data,											// const void*                 pInitialData;
		};
		m_newCache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
	}
}

PipelineFromIncompleteBlobsTestInstance::~PipelineFromIncompleteBlobsTestInstance(void)
{
	delete[] m_data;
}

void PipelineFromIncompleteBlobsTestInstance::preparePipelines (void)
{
	if (m_param->getMode() == TestMode::CACHE)
	{
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_NO_BLOBS], *m_cache);
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_USE_BLOBS], *m_newCache);
	}
	else
		preparePipelinesForBinaries(true, true);
}

enum class MergeBlobsType
{
	EMPTY = 0,
	FROM_DATA,
	HIT,
	MISS,
	MISS_AND_HIT,
	MERGED,

	LAST = MERGED
};

std::string getMergeBlobsTypeStr (MergeBlobsType type)
{
	switch (type)
	{
		case MergeBlobsType::EMPTY:
			return "empty";
		case MergeBlobsType::FROM_DATA:
			return "from_data";
		case MergeBlobsType::HIT:
			return "hit";
		case MergeBlobsType::MISS_AND_HIT:
			return "misshit";
		case MergeBlobsType::MISS:
			return "miss";
		case MergeBlobsType::MERGED:
			return "merged";
	}
	TCU_FAIL("unhandled merge cache type");
}

std::string getMergeBlobsTypesStr (const std::vector<MergeBlobsType>& types)
{
	std::string ret;
	for (size_t idx = 0; idx < types.size(); ++idx)
	{
		if (ret.size())
			ret += '_';
		ret += getMergeBlobsTypeStr(types[idx]);
	}
	return ret;
}


class MergeBlobsTestParam
{
public:
	MergeBlobsType				destBlobsType;
	std::vector<MergeBlobsType> srcBlobTypes;
};

class MergeBlobsTest : public GraphicsTest
{
public:
								MergeBlobsTest	(tcu::TestContext&			testContext,
												 const std::string&			name,
												 const TestParam*			param,
												 const MergeBlobsTestParam* mergeBlobsParam)
									: GraphicsTest		(testContext, name, param)
									, m_mergeBlobsParam	(*mergeBlobsParam)
									{ }
	virtual						~MergeBlobsTest	(void) { }
	virtual TestInstance*		createInstance	(Context& context) const;
private:
	const MergeBlobsTestParam	m_mergeBlobsParam;
};

class MergeBlobsTestInstance : public GraphicsTestInstance
{
public:
							MergeBlobsTestInstance	(Context&					context,
													 const TestParam*			param,
													 const MergeBlobsTestParam* mergeBlobsParam);
private:
	Move<VkPipelineCache>	createPipelineCache		(const InstanceInterface& vki, const DeviceInterface& vk, VkPhysicalDevice physicalDevice, VkDevice device, MergeBlobsType type);
	void					createPipelineBinaries	(const InstanceInterface& vki, const DeviceInterface& vk, VkPhysicalDevice physicalDevice, VkDevice device, PipelineBinaryWrapper* binaries, MergeBlobsType type);

protected:
	void					preparePipelines		(void);

protected:
	const MergeBlobsTestParam			m_mergeBlobsParam;
	Move<VkPipelineCache>				m_cacheMerged;
	de::MovePtr<PipelineBinaryWrapper>	m_secondBinaries;

};

TestInstance* MergeBlobsTest::createInstance (Context& context) const
{
	return new MergeBlobsTestInstance(context, &m_param, &m_mergeBlobsParam);
}

MergeBlobsTestInstance::MergeBlobsTestInstance(Context& context, const TestParam* param, const MergeBlobsTestParam* mergeBlobsParam)
	: GraphicsTestInstance	(context, param)
	, m_mergeBlobsParam		(*mergeBlobsParam)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const VkDevice				vkDevice		= m_context.getDevice();

	if (m_param->getMode() == TestMode::CACHE)
	{
		// Create a merge destination cache
		m_cacheMerged = createPipelineCache(vki, vk, physicalDevice, vkDevice, mergeBlobsParam->destBlobsType);

		// Create more pipeline caches
		std::vector<VkPipelineCache>	sourceCaches	(mergeBlobsParam->srcBlobTypes.size());
		typedef de::SharedPtr<Move<VkPipelineCache> > PipelineCachePtr;
		std::vector<PipelineCachePtr>	sourceCachePtrs	(sourceCaches.size());
		{
			for (size_t sourceIdx = 0; sourceIdx < mergeBlobsParam->srcBlobTypes.size(); sourceIdx++)
			{
				// vk::Move is not copyable, so create it on heap and wrap into de::SharedPtr
				PipelineCachePtr	pipelineCachePtr	(new Move<VkPipelineCache>());
				*pipelineCachePtr = createPipelineCache	(vki, vk, physicalDevice, vkDevice, mergeBlobsParam->srcBlobTypes[sourceIdx]);

				sourceCachePtrs[sourceIdx]	= pipelineCachePtr;
				sourceCaches[sourceIdx]		= **pipelineCachePtr;
			}
		}

		// Merge the caches
		VK_CHECK(vk.mergePipelineCaches(vkDevice, *m_cacheMerged, static_cast<deUint32>(sourceCaches.size()), &sourceCaches[0]));
	}
	else
		m_secondBinaries = de::MovePtr<PipelineBinaryWrapper>(new PipelineBinaryWrapper(vk, vkDevice));
}

Move<VkPipelineCache> MergeBlobsTestInstance::createPipelineCache (const InstanceInterface& vki, const DeviceInterface& vk, VkPhysicalDevice physicalDevice, VkDevice device, MergeBlobsType type)
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
		DE_NULL,										// const void*                 pNext;
		0u,												// VkPipelineCacheCreateFlags  flags;
		0u,												// deUintptr                   initialDataSize;
		DE_NULL,										// const void*                 pInitialData;
	};

	GraphicsPipelineWrapper localPipeline		(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), m_param->getPipelineConstructionType());
	GraphicsPipelineWrapper localMissPipeline	(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), m_param->getPipelineConstructionType());

	switch (type)
	{
		case MergeBlobsType::EMPTY:
		{
			return vk::createPipelineCache(vk, device, &pipelineCacheCreateInfo);
		}
		case MergeBlobsType::FROM_DATA:
		{
			// Create a cache with init data from m_cache
			size_t  dataSize = 0u;
			VK_CHECK(vk.getPipelineCacheData(device, *m_cache, (deUintptr*)&dataSize, DE_NULL));

			std::vector<deUint8> data(dataSize);
			VK_CHECK(vk.getPipelineCacheData(device, *m_cache, (deUintptr*)&dataSize, &data[0]));

			pipelineCacheCreateInfo.initialDataSize = data.size();
			pipelineCacheCreateInfo.pInitialData = &data[0];
			return vk::createPipelineCache(vk, device, &pipelineCacheCreateInfo);
		}
		case MergeBlobsType::HIT:
		{
			Move<VkPipelineCache> ret = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::EMPTY);

			preparePipelineWrapper(localPipeline, *ret);

			return ret;
		}
		case MergeBlobsType::MISS:
		{
			Move<VkPipelineCache> ret = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::EMPTY);

			preparePipelineWrapper(localMissPipeline, *ret, true);

			return ret;
		}
		case MergeBlobsType::MISS_AND_HIT:
		{
			Move<VkPipelineCache> ret = createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::EMPTY);

			preparePipelineWrapper(localPipeline, *ret);
			preparePipelineWrapper(localMissPipeline, *ret, true);

			return ret;
		}
		case MergeBlobsType::MERGED:
		{
			Move<VkPipelineCache>	cache1			= createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::FROM_DATA);
			Move<VkPipelineCache>	cache2			= createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::HIT);
			Move<VkPipelineCache>	cache3			= createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::MISS);

			const VkPipelineCache	sourceCaches[]	=
			{
				*cache1,
				*cache2,
				*cache3
			};

			Move<VkPipelineCache>	ret				= createPipelineCache(vki, vk, physicalDevice, device, MergeBlobsType::EMPTY);

			// Merge the caches
			VK_CHECK(vk.mergePipelineCaches(device, *ret, DE_LENGTH_OF_ARRAY(sourceCaches), sourceCaches));

			return ret;
		}
	}
	TCU_FAIL("unhandled merge cache type");
}

void MergeBlobsTestInstance::createPipelineBinaries(const InstanceInterface& vki, const DeviceInterface& vk, VkPhysicalDevice physicalDevice, VkDevice device, PipelineBinaryWrapper* binaries, MergeBlobsType type)
{
	GraphicsPipelineWrapper localPipeline(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), m_param->getPipelineConstructionType());
	localPipeline.setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR);

	switch (type)
	{
	case MergeBlobsType::FROM_DATA:
	{
		const std::vector<VkViewport>	viewport{ makeViewport(m_renderSize) };
		const std::vector<VkRect2D>		scissor	{ makeRect2D(m_renderSize) };

		ShaderWrapper vertShaderModule(vk, device, m_context.getBinaryCollection().get("color_vert"), 0);
		ShaderWrapper fragShaderModule(vk, device, m_context.getBinaryCollection().get("color_frag"), 0);

		// create local pipeline that has same shaders as final pipeline
		localPipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
			.setDefaultRasterizationState()
			.setDefaultDepthStencilState()
			.setDefaultColorBlendState()
			.setDefaultMultisampleState()
			.setMonolithicPipelineLayout(m_pipelineLayout)
			.setupVertexInputState()
			.setupPreRasterizationShaderState3(viewport,
				scissor,
				m_pipelineLayout,
				*m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS],
				0u,
				vertShaderModule)
			.setupFragmentShaderState2(m_pipelineLayout, *m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS], 0u, fragShaderModule)
			.setupFragmentOutputState(*m_renderPassFramebuffer[PIPELINE_NDX_NO_BLOBS])
			.buildPipeline();

		const auto& pipelineCreateInfo = localPipeline.getPipelineCreateInfo();
		binaries->generatePipelineBinaryKeys(&pipelineCreateInfo);
		binaries->createPipelineBinariesFromPipeline(localPipeline.getPipeline());

		// read binaries data out of the device
		std::vector<VkPipelineBinaryDataKHR>	pipelineDataInfo;
		std::vector<std::vector<uint8_t> >		pipelineDataBlob;
		binaries->getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

		// clear pipeline binaries objects
		binaries->deletePipelineBinariesKeepKeys();

		// create binaries from data blobs
		binaries->createPipelineBinariesFromBinaryData(pipelineDataInfo);
		return;
	}
	case MergeBlobsType::HIT:
	{
		preparePipelineWrapper(localPipeline);

		const auto& pipelineCreateInfo = localPipeline.getPipelineCreateInfo();
		binaries->generatePipelineBinaryKeys(&pipelineCreateInfo);
		binaries->createPipelineBinariesFromPipeline(localPipeline.getPipeline());
		return;
	}
	case MergeBlobsType::MISS:
	{
		preparePipelineWrapper(localPipeline, DE_NULL, true);

		const auto& pipelineCreateInfo = localPipeline.getPipelineCreateInfo();
		binaries->generatePipelineBinaryKeys(&pipelineCreateInfo);
		binaries->createPipelineBinariesFromPipeline(localPipeline.getPipeline());
		return;
	}
	case MergeBlobsType::EMPTY:
	case MergeBlobsType::MISS_AND_HIT:
	case MergeBlobsType::MERGED:
	{
		// those opions are intended only for pipeline cache
		DE_ASSERT(DE_FALSE);
		return;
	}
	}
	TCU_FAIL("unhandled merge binary type");
}

void MergeBlobsTestInstance::preparePipelines(void)
{
	if (m_param->getMode() == TestMode::CACHE)
	{
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_NO_BLOBS], *m_cache);

		// Create pipeline from merged cache
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_USE_BLOBS], *m_cacheMerged);
	}
	else
	{
		const InstanceInterface&	vki				= m_context.getInstanceInterface();
		const DeviceInterface&		vk				= m_context.getDeviceInterface();
		const VkPhysicalDevice		physicalDevice	= m_context.getPhysicalDevice();
		const VkDevice				vkDevice		= m_context.getDevice();

		createPipelineBinaries(vki, vk, physicalDevice, vkDevice, m_binaries.get(), m_mergeBlobsParam.destBlobsType);
		createPipelineBinaries(vki, vk, physicalDevice, vkDevice, m_secondBinaries.get(), m_mergeBlobsParam.srcBlobTypes[0]);

		VkPipelineBinaryInfoKHR pipelineBinaryInfo = m_binaries->preparePipelineBinaryInfo();
		VkPipelineBinaryInfoKHR secondPipelineBinaryInfo = m_secondBinaries->preparePipelineBinaryInfo();

		// merge binary keys
		std::vector<VkPipelineBinaryKeyKHR> mergedKeys(pipelineBinaryInfo.pPipelineBinaryKeys,
													   pipelineBinaryInfo.pPipelineBinaryKeys + pipelineBinaryInfo.binaryCount);
		mergedKeys.insert(
			mergedKeys.end(),
			std::make_move_iterator(secondPipelineBinaryInfo.pPipelineBinaryKeys),
			std::make_move_iterator(secondPipelineBinaryInfo.pPipelineBinaryKeys + secondPipelineBinaryInfo.binaryCount)
		);

		// merge binaries
		std::vector<VkPipelineBinaryKHR> mergedBinaries(pipelineBinaryInfo.pPipelineBinaries,
														pipelineBinaryInfo.pPipelineBinaries + pipelineBinaryInfo.binaryCount);
		mergedBinaries.insert(
			mergedBinaries.end(),
			std::make_move_iterator(secondPipelineBinaryInfo.pPipelineBinaries),
			std::make_move_iterator(secondPipelineBinaryInfo.pPipelineBinaries + secondPipelineBinaryInfo.binaryCount)
		);

		// reuse pipelineBinaryInfo for merged binaries
		pipelineBinaryInfo.binaryCount			= (deUint32)mergedKeys.size();
		pipelineBinaryInfo.pPipelineBinaryKeys	= mergedKeys.data();
		pipelineBinaryInfo.pPipelineBinaries	= mergedBinaries.data();

		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_USE_BLOBS], DE_NULL, false, true, &pipelineBinaryInfo);
		preparePipelineWrapper(m_pipeline[PIPELINE_NDX_NO_BLOBS]);
	}
}

class CacheHeaderTest : public GraphicsTest
{
public:
			CacheHeaderTest		(tcu::TestContext&		testContext,
								 const std::string&		name,
								 const TestParam*		param)
								: GraphicsTest(testContext, name, param)
	{ }
	virtual	~CacheHeaderTest	(void) { }
	virtual	TestInstance*		createInstance(Context& context) const;
};

class CacheHeaderTestInstance : public GraphicsTestInstance
{
public:
				CacheHeaderTestInstance		(Context& context, const TestParam* param);
	virtual		~CacheHeaderTestInstance	(void);
protected:
	deUint8*	m_data;

	struct CacheHeader
	{
		deUint32	HeaderLength;
		deUint32	HeaderVersion;
		deUint32	VendorID;
		deUint32	DeviceID;
		deUint8		PipelineCacheUUID[VK_UUID_SIZE];
	} m_header;
};

TestInstance* CacheHeaderTest::createInstance (Context& context) const
{
	return new CacheHeaderTestInstance(context, &m_param);
}

CacheHeaderTestInstance::CacheHeaderTestInstance (Context& context, const TestParam* param)
	: GraphicsTestInstance		(context, param)
	, m_data					(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	{
		// Create a cache with init data from m_cache
		size_t dataSize = 0u;
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		if (dataSize < sizeof(m_header))
			TCU_THROW(TestError, "Pipeline cache size is smaller than header size");

		m_data = new deUint8[dataSize];
		DE_ASSERT(m_data);
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data));

		deMemcpy(&m_header, m_data, sizeof(m_header));

		if (m_header.HeaderLength - VK_UUID_SIZE != 16)
			TCU_THROW(TestError, "Invalid header size!");

		if (m_header.HeaderVersion != 1)
			TCU_THROW(TestError, "Invalid header version!");

		if (m_header.VendorID != m_context.getDeviceProperties().vendorID)
			TCU_THROW(TestError, "Invalid header vendor ID!");

		if (m_header.DeviceID != m_context.getDeviceProperties().deviceID)
			TCU_THROW(TestError, "Invalid header device ID!");

		if (deMemCmp(&m_header.PipelineCacheUUID, &m_context.getDeviceProperties().pipelineCacheUUID, VK_UUID_SIZE) != 0)
			TCU_THROW(TestError, "Invalid header pipeline cache UUID!");
	}
}

CacheHeaderTestInstance::~CacheHeaderTestInstance (void)
{
	delete[] m_data;
}

class InvalidSizeTest : public GraphicsTest
{
public:
							InvalidSizeTest		(tcu::TestContext& testContext, const std::string& name, const TestParam* param);
	virtual					~InvalidSizeTest	(void) {}
	virtual TestInstance*	createInstance		(Context& context) const;
};

InvalidSizeTest::InvalidSizeTest (tcu::TestContext& testContext, const std::string& name, const TestParam* param)
	: GraphicsTest(testContext, name, param)
{
}

class InvalidSizeTestInstance : public GraphicsTestInstance
{
public:
							InvalidSizeTestInstance		(Context& context, const TestParam*  param);
	virtual					~InvalidSizeTestInstance	(void);
protected:
	deUint8*				m_data;
	deUint8*				m_zeroBlock;
};

TestInstance* InvalidSizeTest::createInstance (Context& context) const
{
	return new InvalidSizeTestInstance(context, &m_param);
}

InvalidSizeTestInstance::InvalidSizeTestInstance (Context& context, const TestParam* param)
	: GraphicsTestInstance		(context, param)
	, m_data					(DE_NULL)
	, m_zeroBlock				(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	try
	{
		// Create a cache with init data from m_cache
		size_t dataSize			= 0u;
		size_t savedDataSize	= 0u;
		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));
		savedDataSize = dataSize;

		// If the value of dataSize is less than the maximum size that can be retrieved by the pipeline cache,
		// at most pDataSize bytes will be written to pData, and vkGetPipelineCacheData will return VK_INCOMPLETE.
		dataSize--;

		m_data = new deUint8[savedDataSize];
		deMemset(m_data, 0, savedDataSize);
		DE_ASSERT(m_data);
		if (vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data) != VK_INCOMPLETE)
			TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

		delete[] m_data;
		m_data = DE_NULL;

		// If the value of dataSize is less than what is necessary to store the header,
		// nothing will be written to pData and zero will be written to dataSize.
		dataSize = 16 + VK_UUID_SIZE - 1;

		m_data = new deUint8[savedDataSize];
		deMemset(m_data, 0, savedDataSize);
		DE_ASSERT(m_data);
		if (vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data) != VK_INCOMPLETE)
			TCU_THROW(TestError, "GetPipelineCacheData should return VK_INCOMPLETE state!");

		m_zeroBlock = new deUint8[savedDataSize];
		deMemset(m_zeroBlock, 0, savedDataSize);
		if (deMemCmp(m_data, m_zeroBlock, savedDataSize) != 0 || dataSize != 0)
			TCU_THROW(TestError, "Data needs to be empty and data size should be 0 when invalid size is passed to GetPipelineCacheData!");
	}
	catch (...)
	{
		delete[] m_data;
		delete[] m_zeroBlock;
		throw;
	}
}

InvalidSizeTestInstance::~InvalidSizeTestInstance (void)
{
	delete[] m_data;
	delete[] m_zeroBlock;
}

class ZeroSizeTest : public GraphicsTest
{
public:
							ZeroSizeTest	(tcu::TestContext& testContext, const std::string& name, const TestParam* param);
	virtual					~ZeroSizeTest	(void) {}
	virtual TestInstance*	createInstance	(Context& context) const;
};

ZeroSizeTest::ZeroSizeTest (tcu::TestContext& testContext, const std::string& name, const TestParam* param)
	: GraphicsTest(testContext, name, param)
{
}

class ZeroSizeTestInstance : public GraphicsTestInstance
{
public:
							ZeroSizeTestInstance	(Context& context, const TestParam* param);
	virtual					~ZeroSizeTestInstance	(void);
protected:
	deUint8*				m_data;
	deUint8*				m_zeroBlock;
};

TestInstance* ZeroSizeTest::createInstance (Context& context) const
{
	return new ZeroSizeTestInstance(context, &m_param);
}

ZeroSizeTestInstance::ZeroSizeTestInstance (Context& context, const TestParam* param)
	: GraphicsTestInstance		(context, param)
	, m_data					(DE_NULL)
	, m_zeroBlock				(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	try
	{
		// Create a cache with init data from m_cache
		size_t dataSize = 0u;

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		m_data = new deUint8[dataSize];
		deMemset(m_data, 0, dataSize);
		DE_ASSERT(m_data);

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data));

		{
			// Create a cache with initialDataSize = 0 & pInitialData != NULL
			const VkPipelineCacheCreateInfo	pipelineCacheCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
				DE_NULL,										// const void*                 pNext;
				0u,												// VkPipelineCacheCreateFlags  flags;
				0u,												// deUintptr                   initialDataSize;
				m_data,											// const void*                 pInitialData;
			};

			const Unique<VkPipelineCache>	pipelineCache			(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));
		}
	}
	catch (...)
	{
		delete[] m_data;
		delete[] m_zeroBlock;
		throw;
	}
}

ZeroSizeTestInstance::~ZeroSizeTestInstance (void)
{
	delete[] m_data;
	delete[] m_zeroBlock;
}

class InvalidBlobTest : public GraphicsTest
{
public:
							InvalidBlobTest		(tcu::TestContext& testContext, const std::string& name, const TestParam* param);
	virtual					~InvalidBlobTest	(void) {}
	virtual TestInstance*	createInstance		(Context& context) const;
};

InvalidBlobTest::InvalidBlobTest (tcu::TestContext& testContext, const std::string& name, const TestParam* param)
	: GraphicsTest(testContext, name, param)
{
}

class InvalidBlobTestInstance : public GraphicsTestInstance
{
public:
							InvalidBlobTestInstance		(Context& context, const TestParam* param);
	virtual					~InvalidBlobTestInstance	(void);
protected:
	deUint8*				m_data;
	deUint8*				m_zeroBlock;
};

TestInstance* InvalidBlobTest::createInstance (Context& context) const
{
	return new InvalidBlobTestInstance(context, &m_param);
}

InvalidBlobTestInstance::InvalidBlobTestInstance (Context& context, const TestParam* param)
	: GraphicsTestInstance		(context, param)
	, m_data					(DE_NULL)
	, m_zeroBlock				(DE_NULL)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	// Create more pipeline caches
	try
	{
		// Create a cache with init data from m_cache
		size_t dataSize = 0u;

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, DE_NULL));

		m_data = new deUint8[dataSize];
		deMemset(m_data, 0, dataSize);
		DE_ASSERT(m_data);

		VK_CHECK(vk.getPipelineCacheData(vkDevice, *m_cache, (deUintptr*)&dataSize, (void*)m_data));

		const struct
		{
			deUint32	offset;
			std::string	name;
		} headerLayout[] =
		{
			{ 4u,	"pipeline cache header version"	},
			{ 8u,	"vendor ID"						},
			{ 12u,	"device ID"						},
			{ 16u,	"pipeline cache ID"				}
		};

		for (deUint32 i = 0u; i < DE_LENGTH_OF_ARRAY(headerLayout); i++)
		{
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "Creating pipeline cache using previously retrieved data with invalid " << headerLayout[i].name << tcu::TestLog::EndMessage;

			m_data[headerLayout[i].offset] = (deUint8)(m_data[headerLayout[i].offset] + 13u);	// Add arbitrary number to create an invalid value

			const VkPipelineCacheCreateInfo	pipelineCacheCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType             sType;
				DE_NULL,										// const void*                 pNext;
				0u,												// VkPipelineCacheCreateFlags  flags;
				dataSize,										// deUintptr                   initialDataSize;
				m_data,											// const void*                 pInitialData;
			};

			const Unique<VkPipelineCache>	pipelineCache			(createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo));

			m_data[headerLayout[i].offset] = (deUint8)(m_data[headerLayout[i].offset] - 13u);	// Return to original value
		}
	}
	catch (...)
	{
		delete[] m_data;
		delete[] m_zeroBlock;
		throw;
	}
}

InvalidBlobTestInstance::~InvalidBlobTestInstance (void)
{
	delete[] m_data;
	delete[] m_zeroBlock;
}
} // anonymous

tcu::TestCaseGroup* createPipelineBlobTestsInternal(tcu::TestContext& testCtx, TestMode testMode, PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> blobTests (new tcu::TestCaseGroup(testCtx, testMode == TestMode::CACHE ? "cache" : "binaries"));

	const VkShaderStageFlags vertFragStages			= VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	const VkShaderStageFlags vertGeomFragStages		= vertFragStages | VK_SHADER_STAGE_GEOMETRY_BIT;
	const VkShaderStageFlags vertTesFragStages		= vertFragStages | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests (new tcu::TestCaseGroup(testCtx, "graphics_tests"));

		const TestParam testParams[]
		{
			{ testMode, pipelineConstructionType, vertFragStages,		false },
			{ testMode, pipelineConstructionType, vertGeomFragStages,	false },
			{ testMode, pipelineConstructionType, vertTesFragStages,	false },
			{ testMode, pipelineConstructionType, vertFragStages,		false, VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT },
			{ testMode, pipelineConstructionType, vertGeomFragStages,	false, VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT },
			{ testMode, pipelineConstructionType, vertTesFragStages,	false, VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT },
		};

		for (const auto& testParam : testParams)
		{
			if ((testMode == TestMode::BINARIES) && testParam.getPipelineCacheCreateFlags())
				continue;
			graphicsTests->addChild(newTestCase<GraphicsTest>(testCtx, &testParam));
		}

		blobTests->addChild(graphicsTests.release());
	}

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "pipeline_from_get_data"));

		const TestParam testParams[]
		{
			{ testMode, pipelineConstructionType, vertFragStages,		false },
			{ testMode, pipelineConstructionType, vertGeomFragStages,	false },
			{ testMode, pipelineConstructionType, vertTesFragStages,	false },
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			graphicsTests->addChild(newTestCase<PipelineFromBlobsTest>(testCtx, &testParams[i]));

		blobTests->addChild(graphicsTests.release());
	}

	// Graphics Pipeline Tests
	if ((testMode == TestMode::BINARIES) && (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC))
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "pipeline_from_get_data_without_shaders", "Test pipeline blob with graphics pipeline."));

		const TestParam testParams[]
		{
			{ testMode, pipelineConstructionType, vertFragStages,		false, 0, false },
			{ testMode, pipelineConstructionType, vertGeomFragStages,	false, 0, false },
			{ testMode, pipelineConstructionType, vertTesFragStages,	false, 0, false },
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			graphicsTests->addChild(newTestCase<PipelineFromBlobsTest>(testCtx, &testParams[i]));

		blobTests->addChild(graphicsTests.release());
	}

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "pipeline_from_incomplete_get_data"));

		const TestParam testParams[]
		{
			{ testMode, pipelineConstructionType, vertFragStages,		false },
			{ testMode, pipelineConstructionType, vertGeomFragStages,	false },
			{ testMode, pipelineConstructionType, vertTesFragStages,	false },
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			graphicsTests->addChild(newTestCase<PipelineFromIncompleteBlobsTest>(testCtx, &testParams[i]));

		blobTests->addChild(graphicsTests.release());
	}

	// Compute Pipeline Tests - don't repeat those tests for graphics pipeline library
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		de::MovePtr<tcu::TestCaseGroup> computeTests (new tcu::TestCaseGroup(testCtx, "compute_tests"));

		const TestParam testParams = { testMode, pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, false };
		computeTests->addChild(newTestCase<ComputeTest>(testCtx, &testParams));

		blobTests->addChild(computeTests.release());
	}

	// Merge blobs tests
	{
		de::MovePtr<tcu::TestCaseGroup> mergeTests (new tcu::TestCaseGroup(testCtx, "merge"));

		const TestParam testParams[]
		{
			{ testMode, pipelineConstructionType, vertFragStages,		true },
			{ testMode, pipelineConstructionType, vertGeomFragStages,	true },
			{ testMode, pipelineConstructionType, vertTesFragStages,	true },
		};

		deUint32 firstTypeIdx	= 0u;
		deUint32 lastTypeIdx	= static_cast<deUint32>(MergeBlobsType::LAST);

		// for pipeline binaries we need to skip some MergeBlobsTypes
		if (testMode == TestMode::BINARIES)
		{
			firstTypeIdx	= static_cast<deUint32>(MergeBlobsType::FROM_DATA);
			lastTypeIdx		= static_cast<deUint32>(MergeBlobsType::MISS);
		}

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
		{
			de::MovePtr<tcu::TestCaseGroup> mergeStagesTests(new tcu::TestCaseGroup(testCtx, testParams[i].generateTestName().c_str()));

			for (deUint32 destTypeIdx = firstTypeIdx; destTypeIdx <= lastTypeIdx; destTypeIdx++)
			for (deUint32 srcType1Idx = firstTypeIdx; srcType1Idx <= lastTypeIdx; srcType1Idx++)
			{
				MergeBlobsTestParam mergeTestParam;
				mergeTestParam.destBlobsType = MergeBlobsType(destTypeIdx);
				mergeTestParam.srcBlobTypes.push_back(MergeBlobsType(srcType1Idx));

				// merge with one cache / binaries
				{
					std::string testName = "src_" + getMergeBlobsTypesStr(mergeTestParam.srcBlobTypes) + "_dst_" + getMergeBlobsTypeStr(mergeTestParam.destBlobsType);
					mergeStagesTests->addChild(new MergeBlobsTest(testCtx,
															testName.c_str(),
															&testParams[i],
															&mergeTestParam));
				}

				// merge with two caches
				if (testMode == TestMode::CACHE)
				{
					for (deUint32 srcType2Idx = 0u; srcType2Idx <= static_cast<deUint32>(MergeBlobsType::LAST); srcType2Idx++)
					{
						MergeBlobsTestParam cacheTestParamTwoCaches = mergeTestParam;

						cacheTestParamTwoCaches.srcBlobTypes.push_back(MergeBlobsType(srcType2Idx));

						std::string testName = "src_" + getMergeBlobsTypesStr(cacheTestParamTwoCaches.srcBlobTypes) + "_dst_" + getMergeBlobsTypeStr(cacheTestParamTwoCaches.destBlobsType);
						mergeStagesTests->addChild(new MergeBlobsTest(testCtx,
															   testName.c_str(),
															   &testParams[i],
															   &cacheTestParamTwoCaches));
					}
				}
			}
			mergeTests->addChild(mergeStagesTests.release());
		}
		blobTests->addChild(mergeTests.release());
	}

	// Misc Tests
	if (testMode == TestMode::CACHE)
	{
		de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "misc_tests"));

		const TestParam testParam(testMode, pipelineConstructionType, vertFragStages, false);

		miscTests->addChild(new CacheHeaderTest(testCtx,
												"cache_header_test",
												&testParam));

		miscTests->addChild(new InvalidSizeTest(testCtx,
												"invalid_size_test",
												&testParam));

		miscTests->addChild(new ZeroSizeTest(testCtx,
												"zero_size_test",
												&testParam));

		miscTests->addChild(new InvalidBlobTest(testCtx,
												"invalid_blob_test",
												&testParam));

		blobTests->addChild(miscTests.release());
	}

	return blobTests.release();
}

tcu::TestCaseGroup* createCacheTests(tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	return createPipelineBlobTestsInternal(testCtx, TestMode::CACHE, pipelineConstructionType);
}

tcu::TestCaseGroup* createBinariesTests(tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	return createPipelineBlobTestsInternal(testCtx, TestMode::BINARIES, pipelineConstructionType);
}

} // pipeline

} // vkt
