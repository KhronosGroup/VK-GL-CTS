/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Miscellaneous pipeline tests.
 *//*--------------------------------------------------------------------*/

#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <array>
#include <numeric>
#include <memory>

#include "vkPipelineConstructionUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktPipelineMiscTests.hpp"

#include "vkDefs.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "deStringUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vkBuilderUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

enum AmberFeatureBits
{
	AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS	= (1 <<	0),
	AMBER_FEATURE_TESSELATION_SHADER					= (1 <<	1),
	AMBER_FEATURE_GEOMETRY_SHADER						= (1 <<	2),
};

using AmberFeatureFlags = deUint32;

#ifndef CTS_USES_VULKANSC
std::vector<std::string> getFeatureList (AmberFeatureFlags flags)
{
	std::vector<std::string> requirements;

	if (flags & AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS)
		requirements.push_back("Features.vertexPipelineStoresAndAtomics");

	if (flags & AMBER_FEATURE_TESSELATION_SHADER)
		requirements.push_back("Features.tessellationShader");

	if (flags & AMBER_FEATURE_GEOMETRY_SHADER)
		requirements.push_back("Features.geometryShader");

	return requirements;
}
#endif // CTS_USES_VULKANSC

void addMonolithicAmberTests (tcu::TestCaseGroup* tests)
{
#ifndef CTS_USES_VULKANSC
	tcu::TestContext& testCtx = tests->getTestContext();

	// Shader test files are saved in <path>/external/vulkancts/data/vulkan/amber/pipeline/<basename>.amber
	struct Case {
		const char*			basename;
		const char*			description;
		AmberFeatureFlags	flags;
	};

	const Case cases[] =
	{
		{
			"position_to_ssbo",
			"Write position data into ssbo using only the vertex shader in a pipeline",
			(AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS),
		},
		{
			"primitive_id_from_tess",
			"Read primitive id from tessellation shaders without a geometry shader",
			(AMBER_FEATURE_TESSELATION_SHADER | AMBER_FEATURE_GEOMETRY_SHADER),
		},
		{
			"layer_read_from_frag",
			"Read gl_layer from fragment shaders without previous writes",
			(AMBER_FEATURE_GEOMETRY_SHADER),
		},
	};
	for (unsigned i = 0; i < DE_LENGTH_OF_ARRAY(cases) ; ++i)
	{
		std::string					file			= std::string(cases[i].basename) + ".amber";
		std::vector<std::string>	requirements	= getFeatureList(cases[i].flags);
		cts_amber::AmberTestCase	*testCase		= cts_amber::createAmberTestCase(testCtx, cases[i].basename, cases[i].description, "pipeline", file, requirements);

		tests->addChild(testCase);
	}
#else
	DE_UNREF(tests);
#endif
}

class ImplicitPrimitiveIDPassthroughCase : public vkt::TestCase
{
public:
	ImplicitPrimitiveIDPassthroughCase		(tcu::TestContext&                  testCtx,
											 const std::string&                 name,
											 const std::string&                 description,
											 const PipelineConstructionType		pipelineConstructionType,
											 bool withTessellation)
		: vkt::TestCase(testCtx, name, description)
		, m_pipelineConstructionType(pipelineConstructionType)
		, m_withTessellationPassthrough(withTessellation)
	{
	}
	~ImplicitPrimitiveIDPassthroughCase		    (void) {}
	void			initPrograms				(SourceCollections& programCollection) const override;
	void			checkSupport				(Context& context) const override;
	TestInstance*	createInstance				(Context& context) const override;

	const PipelineConstructionType m_pipelineConstructionType;
private:
	bool m_withTessellationPassthrough;
};

class ImplicitPrimitiveIDPassthroughInstance : public vkt::TestInstance
{
public:
	ImplicitPrimitiveIDPassthroughInstance	(Context&                           context,
											 const PipelineConstructionType		pipelineConstructionType,
											 bool withTessellation)
		: vkt::TestInstance				(context)
		, m_pipelineConstructionType	(pipelineConstructionType)
		, m_renderSize					(2, 2)
		, m_extent(makeExtent3D			(m_renderSize.x(), m_renderSize.y(), 1u))
		, m_graphicsPipeline			(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType)
		, m_withTessellationPassthrough	(withTessellation)
	{
	}
	~ImplicitPrimitiveIDPassthroughInstance	(void) {}
	tcu::TestStatus		iterate				(void) override;

private:
	PipelineConstructionType	m_pipelineConstructionType;
	const tcu::UVec2            m_renderSize;
	const VkExtent3D		    m_extent;
	const VkFormat		        m_format = VK_FORMAT_R8G8B8A8_UNORM;
	GraphicsPipelineWrapper		m_graphicsPipeline;
	bool                        m_withTessellationPassthrough;
};

TestInstance* ImplicitPrimitiveIDPassthroughCase::createInstance (Context& context) const
{
	return new ImplicitPrimitiveIDPassthroughInstance(context, m_pipelineConstructionType, m_withTessellationPassthrough);
}

void ImplicitPrimitiveIDPassthroughCase::checkSupport (Context &context) const
{
	if (m_withTessellationPassthrough)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

void ImplicitPrimitiveIDPassthroughCase::initPrograms(SourceCollections& sources) const
{
	std::ostringstream vert;
	// Generate a vertically split framebuffer, filled with red on the
	// left, and a green on the right.
	vert
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    switch (gl_VertexIndex) {\n"
		<< "        case 0:\n"
		<< "            gl_Position = vec4(-3.0, -1.0, 0.0, 1.0);\n"
		<< "            break;\n"
		<< "        case 1:\n"
		<< "            gl_Position = vec4(0.0, 3.0, 0.0, 1.0);\n"
		<< "            break;\n"
		<< "        case 2:\n"
		<< "            gl_Position = vec4(0.0, -1.0, 0.0, 1.0);\n"
		<< "            break;\n"
		<< "        case 3:\n"
		<< "            gl_Position = vec4(0.0, -1.0, 0.0, 1.0);\n"
		<< "            break;\n"
		<< "        case 4:\n"
		<< "            gl_Position = vec4(3.0, -1.0, 0.0, 1.0);\n"
		<< "            break;\n"
		<< "        case 5:\n"
		<< "            gl_Position = vec4(0.0, 3.0, 0.0, 1.0);\n"
		<< "            break;\n"
		<< "    }\n"
		<< "}\n"
		;
	sources.glslSources.add("vert") << glu::VertexSource(vert.str());

	if (m_withTessellationPassthrough) {
		std::ostringstream tsc;
		tsc
			<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "layout (vertices = 3) out;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    if (gl_InvocationID == 0) {\n"
			<< "        gl_TessLevelInner[0] = 1.0;\n"
			<< "        gl_TessLevelInner[1] = 1.0;\n"
			<< "        gl_TessLevelOuter[0] = 1.0;\n"
			<< "        gl_TessLevelOuter[1] = 1.0;\n"
			<< "        gl_TessLevelOuter[2] = 1.0;\n"
			<< "        gl_TessLevelOuter[3] = 1.0;\n"
			<< "    }\n"
			<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "}\n";
		sources.glslSources.add("tsc") << glu::TessellationControlSource(tsc.str());

		std::ostringstream tse;
		tse
			<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "layout (triangles, equal_spacing, cw) in;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_Position = gl_in[0].gl_Position * gl_TessCoord.x +\n"
			<< "                  gl_in[1].gl_Position * gl_TessCoord.y +\n"
			<< "                  gl_in[2].gl_Position * gl_TessCoord.z;\n"
			<< "}\n"
			;
		sources.glslSources.add("tse") << glu::TessellationEvaluationSource(tse.str());
	}

	std::ostringstream frag;
	frag
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    const vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "    const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "    outColor = (gl_PrimitiveID % 2 == 0) ? red : green;\n"
		<< "}\n"
		;
	sources.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus ImplicitPrimitiveIDPassthroughInstance::iterate ()
{
	const auto&			vkd					= m_context.getDeviceInterface();
	const auto			device				= m_context.getDevice();
	auto&				alloc				= m_context.getDefaultAllocator();
	const auto			qIndex				= m_context.getUniversalQueueFamilyIndex();
	const auto			queue				= m_context.getUniversalQueue();
	const auto			tcuFormat			= mapVkFormat(m_format);
	const auto			colorUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			verifBufferUsage	= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const tcu::Vec4		clearColor			(0.0f, 0.0f, 0.0f, 1.0f);

	// Color attachment.
	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		m_format,								//	VkFormat				format;
		m_extent,								//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory		colorBuffer		(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto			colorSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto			colorSRL		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto			colorBufferView	= makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, m_format, colorSRR);

	// Verification buffer.
	const auto			verifBufferSize		= static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat)) * m_extent.width * m_extent.height;
	const auto			verifBufferInfo		= makeBufferCreateInfo(verifBufferSize, verifBufferUsage);
	BufferWithMemory	verifBuffer			(vkd, device, alloc, verifBufferInfo, MemoryRequirement::HostVisible);
	auto&				verifBufferAlloc	= verifBuffer.getAllocation();

	// Render pass and framebuffer.
	RenderPassWrapper	renderPass			(m_pipelineConstructionType, vkd, device, m_format);
	renderPass.createFramebuffer(vkd, device, colorBuffer.get(), colorBufferView.get(), m_extent.width, m_extent.height);

	// Shader modules.
	const auto&		binaries		= m_context.getBinaryCollection();
	const auto		vertModule		= ShaderWrapper(vkd, device, binaries.get("vert"));
	const auto		fragModule		= ShaderWrapper(vkd, device, binaries.get("frag"));
	ShaderWrapper tscModule;
	ShaderWrapper tseModule;

	if (m_withTessellationPassthrough) {
		tscModule = ShaderWrapper(vkd, device, binaries.get("tsc"));
		tseModule = ShaderWrapper(vkd, device, binaries.get("tse"));
	}

	// Viewports and scissors.
	const std::vector<VkViewport>	viewports	(1u, makeViewport(m_extent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(m_extent));

	const VkPipelineVertexInputStateCreateInfo		vertexInputState	= initVulkanStructure();
	const VkPipelineRasterizationStateCreateInfo    rasterizationState  =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags  flags;
		VK_FALSE,														// VkBool32                                 depthClampEnable;
		VK_FALSE,														// VkBool32                                 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_CLOCKWISE,								// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
	};

	// Pipeline layout and graphics pipeline.
	const PipelineLayoutWrapper pipelineLayout	(m_pipelineConstructionType, vkd, device);

	const auto topology = m_withTessellationPassthrough ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_graphicsPipeline.setDefaultRasterizationState()
		.setDefaultTopology(topology)
		.setupVertexInputState(&vertexInputState)
		.setDefaultDepthStencilState()
		.setDefaultMultisampleState()
		.setDefaultColorBlendState()
		.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule, &rasterizationState, tscModule, tseModule)
		.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
		.setupFragmentOutputState(*renderPass)
		.setMonolithicPipelineLayout(pipelineLayout)
		.buildPipeline();

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Draw.
	renderPass.begin(vkd, cmdBuffer, scissors.at(0u), clearColor);
	m_graphicsPipeline.bind(cmdBuffer);
	vkd.cmdDraw(cmdBuffer, 6, 1u, 0u, 0u);
	renderPass.end(vkd, cmdBuffer);

	// Copy to verification buffer.
	const auto copyRegion		= makeBufferImageCopy(m_extent, colorSRL);
	const auto transfer2Host	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const auto color2Transfer	= makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorBuffer.get(), colorSRR);

	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &color2Transfer);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer.get(), 1u, &copyRegion);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &transfer2Host);

	endCommandBuffer(vkd, cmdBuffer);

	// Submit and validate result.
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	auto& log = m_context.getTestContext().getLog();
	const tcu::IVec3					iExtent (static_cast<int>(m_extent.width), static_cast<int>(m_extent.height), static_cast<int>(m_extent.depth));
	void*								verifBufferData		= verifBufferAlloc.getHostPtr();
	const tcu::ConstPixelBufferAccess	verifAccess		(tcuFormat, iExtent, verifBufferData);
	invalidateAlloc(vkd, device, verifBufferAlloc);

	const auto red = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	const auto green = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);

	for (int x = 0; x < iExtent.x(); ++x)
		for (int y = 0; y < iExtent.y(); ++y) {
			const auto resultColor = verifAccess.getPixel(x, y);
			const auto expectedColor = (x < iExtent.x() / 2) ? red : green;
			if (resultColor != expectedColor) {
				log << tcu::TestLog::ImageSet("Result image", "Expect left side of framebuffer red, and right side green")
					<< tcu::TestLog::Image("Result", "Verification buffer", verifAccess)
					<< tcu::TestLog::EndImageSet;
				TCU_FAIL("Expected a vertically split framebuffer, filled with red on the left and green the right; see the log for the unexpected result");
			}
		}

	return tcu::TestStatus::pass("Pass");
}

#ifndef CTS_USES_VULKANSC
struct UnusedShaderStageParams
{
	PipelineConstructionType	pipelineConstructionType;
	bool						useTessShaders;
	bool						useGeomShader;
};

class UnusedShaderStagesCase : public vkt::TestCase
{
public:
					UnusedShaderStagesCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const UnusedShaderStageParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}
	virtual			~UnusedShaderStagesCase	(void) {}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
	void			checkSupport			(Context& context) const override;

protected:
	UnusedShaderStageParams m_params;
};

class UnusedShaderStagesInstance : public vkt::TestInstance
{
public:
						UnusedShaderStagesInstance	(Context& context, const UnusedShaderStageParams& params)
							: vkt::TestInstance	(context)
							, m_params			(params)
							{}
	virtual				~UnusedShaderStagesInstance	(void) {}
	tcu::TestStatus		iterate						(void) override;

protected:
	UnusedShaderStageParams m_params;
};

void UnusedShaderStagesCase::initPrograms (vk::SourceCollections &programCollection) const
{
	// Shaders that produce bad results.
	{
		std::ostringstream vert;
		vert
			<< "#version 460\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "void main (void) {\n"
			<< "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("bad_vert") << glu::VertexSource(vert.str());

		std::ostringstream tesc;
		tesc
			<< "#version 460\n"
			<< "layout (vertices=3) out;\n"
			<< "in gl_PerVertex {\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_in[gl_MaxPatchVertices];\n"
			<< "out gl_PerVertex {\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_out[];\n"
			<< "void main (void) {\n"
			<< "    gl_TessLevelInner[0] = 1.0;\n"
			<< "    gl_TessLevelInner[1] = 1.0;\n"
			<< "    gl_TessLevelOuter[0] = 1.0;\n"
			<< "    gl_TessLevelOuter[1] = 1.0;\n"
			<< "    gl_TessLevelOuter[2] = 1.0;\n"
			<< "    gl_TessLevelOuter[3] = 1.0;\n"
			<< "    gl_out[gl_InvocationID].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("bad_tesc") << glu::TessellationControlSource(tesc.str());

		std::ostringstream tese;
		tese
			<< "#version 460\n"
			<< "layout (triangles, fractional_odd_spacing, cw) in;\n"
			<< "in gl_PerVertex {\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_in[gl_MaxPatchVertices];\n"
			<< "out gl_PerVertex {\n"
			<< "  vec4 gl_Position;\n"
			<< "};\n"
			<< "void main() {\n"
			<< "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("bad_tese") << glu::TessellationEvaluationSource(tese.str());

		std::ostringstream geom;
		geom
			<< "#version 460\n"
			<< "layout (triangles) in;\n"
			<< "layout (triangle_strip, max_vertices=3) out;\n"
			<< "in gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "} gl_in[3];\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "void main() {\n"
			// Avoid emitting any vertices.
			<< "}\n"
			;
		programCollection.glslSources.add("bad_geom") << glu::GeometrySource(geom.str());

		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "void main (void) {\n"
			<< "    outColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("bad_frag") << glu::FragmentSource(frag.str());
	}

	// Shaders that produce the expected results.
	{
		std::ostringstream vert;
		vert
			<< "#version 460\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "vec2 positions[3] = vec2[](\n"
			<< "    vec2(-1.0, -1.0),\n"
			<< "    vec2(-1.0,  3.0),\n"
			<< "    vec2( 3.0, -1.0)\n"
			<< ");\n"
			<< "void main (void) {\n"
			<< "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

		std::ostringstream tesc;
		tesc
			<< "#version 460\n"
			<< "layout (vertices=3) out;\n"
			<< "in gl_PerVertex {\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_in[gl_MaxPatchVertices];\n"
			<< "out gl_PerVertex {\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_out[];\n"
			<< "void main (void) {\n"
			<< "    gl_TessLevelInner[0] = 1.0;\n"
			<< "    gl_TessLevelInner[1] = 1.0;\n"
			<< "    gl_TessLevelOuter[0] = 1.0;\n"
			<< "    gl_TessLevelOuter[1] = 1.0;\n"
			<< "    gl_TessLevelOuter[2] = 1.0;\n"
			<< "    gl_TessLevelOuter[3] = 1.0;\n"
			<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());

		std::ostringstream tese;
		tese
			<< "#version 460\n"
			<< "layout (triangles, fractional_odd_spacing, cw) in;\n"
			<< "in gl_PerVertex {\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_in[gl_MaxPatchVertices];\n"
			<< "out gl_PerVertex {\n"
			<< "  vec4 gl_Position;\n"
			<< "};\n"
			<< "void main() {\n"
			<< "    gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
			<< "                  (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
			<< "                  (gl_TessCoord.z * gl_in[2].gl_Position);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());

		std::ostringstream geom;
		geom
			<< "#version 460\n"
			<< "layout (triangles) in;\n"
			<< "layout (triangle_strip, max_vertices=3) out;\n"
			<< "in gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "} gl_in[3];\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "void main() {\n"
			<< "    gl_Position = gl_in[0].gl_Position; EmitVertex();\n"
			<< "    gl_Position = gl_in[1].gl_Position; EmitVertex();\n"
			<< "    gl_Position = gl_in[2].gl_Position; EmitVertex();\n"
			<< "}\n"
			;
		programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());

		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "void main (void) {\n"
			<< "    outColor = vec4(0.0, 1.0, 0.0, 1.0);\n" // Blue instead of black.
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
}

TestInstance* UnusedShaderStagesCase::createInstance (Context &context) const
{
	return new UnusedShaderStagesInstance(context, m_params);
}

void UnusedShaderStagesCase::checkSupport (Context &context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);

	if (m_params.useTessShaders)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (m_params.useGeomShader)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

tcu::TestStatus UnusedShaderStagesInstance::iterate ()
{
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queue			= m_context.getUniversalQueue();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();

	const bool			isOptimized		= (m_params.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY);
	const auto			colorExtent		= makeExtent3D(1u, 1u, 1u);
	const tcu::IVec3	colorExtentVec	(static_cast<int>(colorExtent.width), static_cast<int>(colorExtent.height), static_cast<int>(colorExtent.depth));
	const auto			colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			colorSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto			colorSRL		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto			tcuFormat		= mapVkFormat(colorFormat);
	const tcu::Vec4		clearColor		(0.0f, 0.0f, 0.0f, 0.0f);
	const tcu::Vec4		expectedColor	(0.0f, 1.0f, 0.0f, 1.0f); // Must match the good frag shader.

	// Good and bad shader modules.
	const auto&	binaries			= m_context.getBinaryCollection();

	const auto	goodVertModule		= createShaderModule(vkd, device, binaries.get("vert"));
	const auto	goodTescModule		= (m_params.useTessShaders ?	createShaderModule(vkd, device, binaries.get("tesc")) : Move<VkShaderModule>());
	const auto	goodTeseModule		= (m_params.useTessShaders ?	createShaderModule(vkd, device, binaries.get("tese")) : Move<VkShaderModule>());
	const auto	goodGeomModule		= (m_params.useGeomShader ?		createShaderModule(vkd, device, binaries.get("geom")) : Move<VkShaderModule>());
	const auto	goodFragModule		= createShaderModule(vkd, device, binaries.get("frag"));

	const auto	goodVertShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, goodVertModule.get());
	const auto	goodTescShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, goodTescModule.get());
	const auto	goodTeseShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, goodTeseModule.get());
	const auto	goodGeomShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, goodGeomModule.get());
	const auto	goodFragShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, goodFragModule.get());

	const auto	badVertModule		= createShaderModule(vkd, device, binaries.get("bad_vert"));
	const auto	badTescModule		= (m_params.useTessShaders ?	createShaderModule(vkd, device, binaries.get("bad_tesc")) : Move<VkShaderModule>());
	const auto	badTeseModule		= (m_params.useTessShaders ?	createShaderModule(vkd, device, binaries.get("bad_tese")) : Move<VkShaderModule>());
	const auto	badGeomModule		= (m_params.useGeomShader ?		createShaderModule(vkd, device, binaries.get("bad_geom")) : Move<VkShaderModule>());
	const auto	badFragModule		= createShaderModule(vkd, device, binaries.get("bad_frag"));

	const auto	badVertShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, badVertModule.get());
	const auto	badTescShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, badTescModule.get());
	const auto	badTeseShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, badTeseModule.get());
	const auto	badGeomShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, badGeomModule.get());
	const auto	badFragShaderInfo	= makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, badFragModule.get());

	// Color attachment.
	const VkImageCreateInfo colorAttachmentCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		colorExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory colorAttachment (vkd, device, alloc, colorAttachmentCreateInfo, MemoryRequirement::Any);

	// Color attachment view.
	const auto colorAttachmentView = makeImageView(vkd, device, colorAttachment.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// Verification buffer.
	const auto			verificationBufferSize			= static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat)) * colorExtent.width * colorExtent.height * colorExtent.depth;
	const auto			verificationBufferCreateInfo	= makeBufferCreateInfo(verificationBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory	verificationBuffer				(vkd, device, alloc, verificationBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc			= verificationBuffer.getAllocation();

	// Render pass and framebuffer.
	RenderPassWrapper	renderPass						(m_params.pipelineConstructionType, vkd, device, colorFormat);
	renderPass.createFramebuffer(vkd, device, *colorAttachment, colorAttachmentView.get(), colorExtent.width, colorExtent.height);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device);

	// Pipeline state.

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = initVulkanStructure();

	const auto primitiveTopology = (m_params.useTessShaders ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineInputAssemblyStateCreateFlags	flags;
		primitiveTopology,												//	VkPrimitiveTopology						topology;
		VK_FALSE,														//	VkBool32								primitiveRestartEnable;
	};

	const VkPipelineTessellationStateCreateInfo tessellationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineTessellationStateCreateFlags	flags;
		3u,															//	uint32_t								patchControlPoints;
	};

	const std::vector<VkViewport>	viewports	(1u, makeViewport(colorExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(colorExtent));

	const VkPipelineViewportStateCreateInfo viewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineViewportStateCreateFlags	flags;
		de::sizeU32(viewports),									//	uint32_t							viewportCount;
		de::dataOrNull(viewports),								//	const VkViewport*					pViewports;
		de::sizeU32(scissors),									//	uint32_t							scissorCount;
		de::dataOrNull(scissors),								//	const VkRect2D*						pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														//	VkBool32								depthClampEnable;
		VK_FALSE,														//	VkBool32								rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											//	VkPolygonMode							polygonMode;
		VK_CULL_MODE_BACK_BIT,											//	VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								//	VkFrontFace								frontFace;
		VK_FALSE,														//	VkBool32								depthBiasEnable;
		0.0f,															//	float									depthBiasConstantFactor;
		0.0f,															//	float									depthBiasClamp;
		0.0f,															//	float									depthBiasSlopeFactor;
		1.0f,															//	float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo multisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													//	VkBool32								sampleShadingEnable;
		1.0f,														//	float									minSampleShading;
		nullptr,													//	const VkSampleMask*						pSampleMask;
		VK_FALSE,													//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,													//	VkBool32								alphaToOneEnable;
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = initVulkanStructure();

	const VkColorComponentFlags colorComponentFlags = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE,				//	VkBool32				blendEnable;
		VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,		//	VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		//	VkBlendOp				alphaBlendOp;
		colorComponentFlags,	//	VkColorComponentFlags	colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,													//	const void*									pNext;
		0u,															//	VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,													//	VkBool32									logicOpEnable;
		VK_LOGIC_OP_CLEAR,											//	VkLogicOp									logicOp;
		1u,															//	uint32_t									attachmentCount;
		&colorBlendAttachmentState,									//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									//	float										blendConstants[4];
	};

	// Make a few vectors with the wrong shader modules.
	std::vector<VkPipelineShaderStageCreateInfo> badPreRasterStages;
	badPreRasterStages.push_back(badVertShaderInfo);
	if (m_params.useTessShaders)
	{
		badPreRasterStages.push_back(badTescShaderInfo);
		badPreRasterStages.push_back(badTeseShaderInfo);
	}
	if (m_params.useGeomShader)
		badPreRasterStages.push_back(badGeomShaderInfo);

	std::vector<VkPipelineShaderStageCreateInfo> allBadStages (badPreRasterStages);
	allBadStages.push_back(badFragShaderInfo);

	// Make a few vectors with the right shader modules.
	std::vector<VkPipelineShaderStageCreateInfo> goodPreRasterStages;
	goodPreRasterStages.push_back(goodVertShaderInfo);
	if (m_params.useTessShaders)
	{
		goodPreRasterStages.push_back(goodTescShaderInfo);
		goodPreRasterStages.push_back(goodTeseShaderInfo);
	}
	if (m_params.useGeomShader)
		goodPreRasterStages.push_back(goodGeomShaderInfo);

	std::vector<VkPipelineShaderStageCreateInfo> allGoodStages (goodPreRasterStages);
	allGoodStages.push_back(goodFragShaderInfo);

	// Build the different pipeline pieces.
	Move<VkPipeline> vertexInputLib;
	Move<VkPipeline> preRasterShaderLib;
	Move<VkPipeline> fragShaderLib;
	Move<VkPipeline> fragOutputLib;

	VkPipelineCreateFlags libCreationFlags	= VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
	VkPipelineCreateFlags linkFlags			= 0u;

	if (isOptimized)
	{
		libCreationFlags	|= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
		linkFlags			|= VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
	}

	// Vertex input state library.
	{
		VkGraphicsPipelineLibraryCreateInfoEXT vertexInputLibInfo	= initVulkanStructure();
		vertexInputLibInfo.flags									|= VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

		VkGraphicsPipelineCreateInfo vertexInputPipelineInfo	= initVulkanStructure(&vertexInputLibInfo);
		vertexInputPipelineInfo.flags							= libCreationFlags;
		vertexInputPipelineInfo.pVertexInputState				= &vertexInputStateInfo;
		vertexInputPipelineInfo.pInputAssemblyState				= &inputAssemblyStateInfo;

		// Add all bad shaders (they should be ignored).
		vertexInputPipelineInfo.stageCount	= de::sizeU32(allBadStages);
		vertexInputPipelineInfo.pStages		= de::dataOrNull(allBadStages);

		vertexInputLib = createGraphicsPipeline(vkd, device, DE_NULL, &vertexInputPipelineInfo);
	}

	// Pre-rasterization shader state library.
	{
		VkGraphicsPipelineLibraryCreateInfoEXT preRasterShaderLibInfo	= initVulkanStructure();
		preRasterShaderLibInfo.flags									|= VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

		VkGraphicsPipelineCreateInfo preRasterShaderPipelineInfo	= initVulkanStructure(&preRasterShaderLibInfo);
		preRasterShaderPipelineInfo.flags							= libCreationFlags;
		preRasterShaderPipelineInfo.layout							= pipelineLayout.get();
		preRasterShaderPipelineInfo.pViewportState					= &viewportStateInfo;
		preRasterShaderPipelineInfo.pRasterizationState				= &rasterizationStateInfo;
		if (m_params.useTessShaders)
		{
			preRasterShaderPipelineInfo.pInputAssemblyState			= &inputAssemblyStateInfo;
			preRasterShaderPipelineInfo.pTessellationState			= &tessellationStateInfo;
		}
		preRasterShaderPipelineInfo.renderPass						= renderPass.get();

		// All good pre-rasterization stages.
		auto preRasterStagesVec = goodPreRasterStages;
		// The bad fragment shader state cannot be added here due to VUID-VkGraphicsPipelineCreateInfo-pStages-06894.
		//preRasterStagesVec.push_back(badFragShaderInfo);

		preRasterShaderPipelineInfo.stageCount	= de::sizeU32(preRasterStagesVec);
		preRasterShaderPipelineInfo.pStages		= de::dataOrNull(preRasterStagesVec);

		preRasterShaderLib = createGraphicsPipeline(vkd, device, DE_NULL, &preRasterShaderPipelineInfo);
	}

	// Fragment shader stage library.
	{
		VkGraphicsPipelineLibraryCreateInfoEXT fragShaderLibInfo	= initVulkanStructure();
		fragShaderLibInfo.flags										|= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

		VkGraphicsPipelineCreateInfo fragShaderPipelineInfo	= initVulkanStructure(&fragShaderLibInfo);
		fragShaderPipelineInfo.flags						= libCreationFlags;
		fragShaderPipelineInfo.layout						= pipelineLayout.get();
		fragShaderPipelineInfo.pMultisampleState			= &multisampleStateInfo;
		fragShaderPipelineInfo.pDepthStencilState			= &depthStencilStateInfo;
		fragShaderPipelineInfo.renderPass					= renderPass.get();

		// The good fragment shader stage.
		std::vector<VkPipelineShaderStageCreateInfo> fragShaderStagesVec;
		// We cannot add the bad pre-rasterization shader stages due to VUID-VkGraphicsPipelineCreateInfo-pStages-06895.
		//fragShaderStagesVec.insert(fragShaderStagesVec.end(), badPreRasterStages.begin(), badPreRasterStages.end())
		fragShaderStagesVec.push_back(goodFragShaderInfo);

		fragShaderPipelineInfo.stageCount	= de::sizeU32(fragShaderStagesVec);
		fragShaderPipelineInfo.pStages		= de::dataOrNull(fragShaderStagesVec);

		fragShaderLib = createGraphicsPipeline(vkd, device, DE_NULL, &fragShaderPipelineInfo);
	}

	// Fragment output library.
	{
		VkGraphicsPipelineLibraryCreateInfoEXT fragOutputLibInfo	= initVulkanStructure();
		fragOutputLibInfo.flags										|= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

		VkGraphicsPipelineCreateInfo fragOutputPipelineInfo	= initVulkanStructure(&fragOutputLibInfo);
		fragOutputPipelineInfo.flags						= libCreationFlags;
		fragOutputPipelineInfo.pColorBlendState				= &colorBlendStateInfo;
		fragOutputPipelineInfo.renderPass					= renderPass.get();
		fragOutputPipelineInfo.pMultisampleState			= &multisampleStateInfo;

		// Add all bad shaders (they should be ignored).
		fragOutputPipelineInfo.stageCount	= de::sizeU32(allBadStages);
		fragOutputPipelineInfo.pStages		= de::dataOrNull(allBadStages);

		fragOutputLib = createGraphicsPipeline(vkd, device, DE_NULL, &fragOutputPipelineInfo);
	}

	// Linked pipeline.
	const std::vector<VkPipeline> libraryHandles
	{
		vertexInputLib.get(),
		preRasterShaderLib.get(),
		fragShaderLib.get(),
		fragOutputLib.get(),
	};

	VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo	= initVulkanStructure();
	linkedPipelineLibraryInfo.libraryCount						= de::sizeU32(libraryHandles);
	linkedPipelineLibraryInfo.pLibraries						= de::dataOrNull(libraryHandles);

	VkGraphicsPipelineCreateInfo linkedPipelineInfo	= initVulkanStructure(&linkedPipelineLibraryInfo);
	linkedPipelineInfo.flags						= linkFlags;
	linkedPipelineInfo.layout						= pipelineLayout.get();
	linkedPipelineInfo.stageCount					= de::sizeU32(allBadStages);
	linkedPipelineInfo.pStages						= de::dataOrNull(allBadStages);

	const auto pipeline = createGraphicsPipeline(vkd, device, DE_NULL, &linkedPipelineInfo);

	// Command pool, command buffer and draw.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	renderPass.begin(vkd, cmdBuffer, scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	renderPass.end(vkd, cmdBuffer);

	// Copy color attachment to verification buffer.
	const auto preCopyBarrier	= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
														 VK_ACCESS_TRANSFER_READ_BIT,
														 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
														 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
														 colorAttachment.get(), colorSRR);
	const auto copyRegion		= makeBufferImageCopy(colorExtent, colorSRL);
	const auto postCopyBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorAttachment.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyBarrier);

	endCommandBuffer(vkd, cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify pixel contents.
	invalidateAlloc(vkd, device, verificationBufferAlloc);
	tcu::PixelBufferAccess resultAccess (tcuFormat, colorExtentVec, verificationBufferAlloc.getHostPtr());

	for (int z = 0; z < colorExtentVec.z(); ++z)
		for (int y = 0; y < colorExtentVec.y(); ++y)
			for (int x = 0; x < colorExtentVec.x(); ++x)
			{
				const auto resultColor = resultAccess.getPixel(x, y, z);
				if (resultColor != expectedColor)
				{
					const tcu::IVec3 position(x, y, z);
					std::ostringstream msg;
					msg << "Bad color found at pixel " << position << ": expected " << expectedColor << " but found " << resultColor;
					TCU_FAIL(msg.str());
				}
			}

	return tcu::TestStatus::pass("Pass");
}
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
class PipelineLibraryInterpolateAtSampleTestCase : public vkt::TestCase
{
public:
	PipelineLibraryInterpolateAtSampleTestCase(tcu::TestContext& context, const std::string& name, const std::string& description);
	void            initPrograms            (vk::SourceCollections& programCollection) const override;
	TestInstance*   createInstance          (Context& context) const override;
	void            checkSupport            (Context& context) const override;
	//there are 4 sample points, which may have a shader invocation each, each of them writes 5 values
	//and we render a 2x2 grid.
	static constexpr uint32_t				width		= 2;
	static constexpr uint32_t				height		= 2;
	static constexpr VkSampleCountFlagBits	sampleCount = VK_SAMPLE_COUNT_4_BIT;
	static constexpr uint32_t ResultCount = (sampleCount + 1) * sampleCount * width * height;
};

class PipelineLibraryInterpolateAtSampleTestInstance : public vkt::TestInstance
{
public:
	PipelineLibraryInterpolateAtSampleTestInstance(Context& context);
	void runTest(BufferWithMemory& index, BufferWithMemory& values, size_t bufferSize, PipelineConstructionType type);
	virtual tcu::TestStatus iterate(void);
};

PipelineLibraryInterpolateAtSampleTestCase::PipelineLibraryInterpolateAtSampleTestCase(tcu::TestContext& context, const std::string& name, const std::string& description):
	vkt::TestCase(context, name, description) { }

void PipelineLibraryInterpolateAtSampleTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY);
}

void PipelineLibraryInterpolateAtSampleTestCase::initPrograms(vk::SourceCollections& collection) const
{
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "vec2 positions[6] = vec2[](\n"
		<< "		vec2(1.0, 1.0),"
		<< "		vec2(-1.0, 1.0),"
		<< "		vec2(-1.0, -1.0),"
		<< "		vec2(-1.0, -1.0),"
		<< "		vec2(1.0, -1.0),"
		<< "		vec2(1.0, 1.0)"
		<< ");\n"
		<< "float values[6] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6};\n"
		<< "layout (location=0) out float verify;"
		<< "void main() {\n"
		<< "		gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
		<< "		verify = values[gl_VertexIndex];\n"
		<< "}";
		collection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) out vec4 outColor;\n"
		<< "layout (location=0) in float verify;"
		<< "layout(std430, binding = 0) buffer Index {"
		<< "	uint writeIndex;"
		<< "} index;\n"
		<< "layout(std430, binding = 1) buffer Values {"
		<< "	float num[" << PipelineLibraryInterpolateAtSampleTestCase::ResultCount << "];"
		<< "} values;\n"
		<< "void main() {\n"
		<< "	uint index = atomicAdd(index.writeIndex, 5);"
		<< "	float iSample1 = interpolateAtSample(verify, 0);\n"
		<< "	float iSample2 = interpolateAtSample(verify, 1);\n"
		<< "	float iSample3 = interpolateAtSample(verify, 2);\n"
		<< "	float iSample4 = interpolateAtSample(verify, 3);\n"
		<< "	values.num[index] = verify;"
		<< "	values.num[index + 1] = iSample1;"
		<< "	values.num[index + 2] = iSample2;"
		<< "	values.num[index + 3] = iSample3;"
		<< "	values.num[index + 4] = iSample4;"
		<< "	outColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
		<< "}";
		collection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

TestInstance* PipelineLibraryInterpolateAtSampleTestCase::createInstance(Context& context) const
{
	return new PipelineLibraryInterpolateAtSampleTestInstance(context);
}

PipelineLibraryInterpolateAtSampleTestInstance::PipelineLibraryInterpolateAtSampleTestInstance(Context& context) : vkt::TestInstance(context) { }

void PipelineLibraryInterpolateAtSampleTestInstance::runTest(BufferWithMemory& index, BufferWithMemory& values, size_t bufferSize, PipelineConstructionType type)
{
	const auto& vki			= m_context.getInstanceInterface();
	const auto& vkd			= m_context.getDeviceInterface();
	const auto  physDevice	= m_context.getPhysicalDevice();
	const auto  device		= m_context.getDevice();
	auto& alloc				= m_context.getDefaultAllocator();
	auto imageFormat		= vk::VK_FORMAT_R8G8B8A8_UNORM;
	auto imageExtent		= vk::makeExtent3D(2, 2, 1u);

	const std::vector<vk::VkViewport>	viewports	{ makeViewport(imageExtent) };
	const std::vector<vk::VkRect2D>		scissors	{ makeRect2D(imageExtent) };

	de::MovePtr<vk::ImageWithMemory>  colorAttachment;

	vk::GraphicsPipelineWrapper pipeline1(vki, vkd, physDevice, device, m_context.getDeviceExtensions(), type);
	const auto  qIndex	= m_context.getUniversalQueueFamilyIndex();

	const auto  subresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto  imageUsage			= static_cast<vk::VkImageUsageFlags>(vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const vk::VkImageCreateInfo imageCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType				sType;
		nullptr,									//	const void*					pNext;
		0u,											//	VkImageCreateFlags			flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType					imageType;
		imageFormat,								//	VkFormat					format;
		imageExtent,								//	VkExtent3D					extent;
		1u,											//	deUint32					mipLevels;
		1u,											//	deUint32					arrayLayers;
		vk::VK_SAMPLE_COUNT_4_BIT,					//	VkSampleCountFlagBits		samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling				tiling;
		imageUsage,									//	VkImageUsageFlags			usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode				sharingMode;
		1u,											//	deUint32					queueFamilyIndexCount;
		&qIndex,									//	const deUint32*				pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout				initialLayout;
	};

	colorAttachment               = de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vkd, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any));
	auto colorAttachmentView      = vk::makeImageView(vkd, device, colorAttachment->get(), vk::VK_IMAGE_VIEW_TYPE_2D, imageFormat, subresourceRange);

	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

	auto descriptorSetLayout    = layoutBuilder.build(vkd, device);
	vk::PipelineLayoutWrapper	graphicsPipelineLayout (type, vkd, device, descriptorSetLayout.get());

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
	const auto descriptorSetBuffer		= makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor sets.
	DescriptorSetUpdateBuilder updater;

	const auto indexBufferInfo = makeDescriptorBufferInfo(index.get(), 0ull, sizeof(uint32_t));
	const auto valueBufferInfo = makeDescriptorBufferInfo(values.get(), 0ull, bufferSize);
	updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indexBufferInfo);
	updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &valueBufferInfo);

	updater.update(vkd, device);

	auto vtxshader  = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"));
	auto frgshader  = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"));

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputState =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType sType
		nullptr,														// const void*                                 pNext
		0u,																// VkPipelineVertexInputStateCreateFlags       flags
		0u,																// deUint32                                    vertexBindingDescriptionCount
		nullptr,														// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		0u,																// deUint32                                    vertexAttributeDescriptionCount
		nullptr,														// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	VkPipelineMultisampleStateCreateInfo multisampling = initVulkanStructure();
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = NULL; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	pipeline1.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.setDefaultRasterizationState()
		.setDefaultDepthStencilState()
		.setDefaultColorBlendState()
		.setupVertexInputState(&vertexInputState)
		.setupPreRasterizationShaderState(
			viewports,
			scissors,
			graphicsPipelineLayout,
			DE_NULL,
			0u,
			vtxshader)
		.setupFragmentShaderState(graphicsPipelineLayout, DE_NULL, 0u,
			frgshader)
		.setupFragmentOutputState(DE_NULL, 0u, DE_NULL, &multisampling)
		.setMonolithicPipelineLayout(graphicsPipelineLayout).buildPipeline();

	auto commandPool = createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, qIndex);
	auto commandBuffer = vk::allocateCommandBuffer(vkd, device, commandPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const auto		clearValueColor		= vk::makeClearValueColor(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	const vk::VkRect2D renderArea =
	{
		{ 0u, 0u },
		{ imageExtent.width, imageExtent.height }
	};

	const vk::VkRenderingAttachmentInfoKHR colorAttachments = {
		vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		colorAttachmentView.get(),								// VkImageView							imageView;
		vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,				// VkImageLayout						imageLayout;
		vk::VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits				resolveMode;
		DE_NULL,												// VkImageView							resolveImageView;
		vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,				// VkImageLayout						resolveImageLayout;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp					loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp					storeOp;
		clearValueColor											// VkClearValue							clearValue;
	};
	const VkRenderingInfoKHR render_info = {
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		0,
		0,
		renderArea,
		1,
		0,
		1,
		&colorAttachments,
		DE_NULL,
		DE_NULL
	};

	vk::beginCommandBuffer(vkd, commandBuffer.get());
	vk::VkImageMemoryBarrier initialBarrier = makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
		(*colorAttachment).get(), subresourceRange);
	vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0, nullptr,
						  0, nullptr, 1, &initialBarrier);
	vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0u, 1, &descriptorSetBuffer.get(), 0u, nullptr);

	vkd.cmdBeginRendering(*commandBuffer, &render_info);
	pipeline1.bind(commandBuffer.get());
	vkd.cmdSetPatchControlPointsEXT(commandBuffer.get(), 3);
	vkd.cmdDraw(commandBuffer.get(), 6, 1, 0, 0);
	vkd.cmdEndRendering(*commandBuffer);

	const VkBufferMemoryBarrier indexBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, index.get(), 0ull, sizeof(uint32_t));
	vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		0u, 0, nullptr, 1, &indexBufferBarrier, 0, nullptr);

	const VkBufferMemoryBarrier valueBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, values.get(), 0ull, bufferSize);
	vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		0u, 0, nullptr, 1, &valueBufferBarrier, 0, nullptr);

	vk::endCommandBuffer(vkd, commandBuffer.get());
	vk::submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), commandBuffer.get());
}

tcu::TestStatus PipelineLibraryInterpolateAtSampleTestInstance::iterate(void)
{
	const auto& vkd			= m_context.getDeviceInterface();
	const auto  device		= m_context.getDevice();
	auto& alloc				= m_context.getDefaultAllocator();

	struct ValueBuffer {
		float values[PipelineLibraryInterpolateAtSampleTestCase::ResultCount];
	};

	size_t resultSize = PipelineLibraryInterpolateAtSampleTestCase::ResultCount;

	const auto			indexBufferSize	= static_cast<VkDeviceSize>(sizeof(uint32_t));
	const auto			valueBufferSize	= static_cast<VkDeviceSize>(sizeof(ValueBuffer));

	auto indexCreateInfo	= makeBufferCreateInfo(indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	auto valuesCreateInfo	= makeBufferCreateInfo(valueBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	BufferWithMemory	indexBufferMonolithic		(vkd, device, alloc, indexCreateInfo, MemoryRequirement::HostVisible);
	BufferWithMemory	valuesBufferMonolithic		(vkd, device, alloc, valuesCreateInfo, MemoryRequirement::HostVisible);
	BufferWithMemory	indexBufferGPL				(vkd, device, alloc, indexCreateInfo, MemoryRequirement::HostVisible);
	BufferWithMemory	valuesBufferGPL				(vkd, device, alloc, valuesCreateInfo, MemoryRequirement::HostVisible);

	auto&				indexBufferMonolithicAlloc	= indexBufferMonolithic.getAllocation();
	auto&				valuesBufferMonolithicAlloc	= valuesBufferMonolithic.getAllocation();
	auto&				indexBufferGPLAlloc	= indexBufferGPL.getAllocation();
	auto&				valuesBufferGPLAlloc	= valuesBufferGPL.getAllocation();

	void*				indexBufferMonolithicData	= indexBufferMonolithicAlloc.getHostPtr();
	void*				valuesBufferMonolithicData	= valuesBufferMonolithicAlloc.getHostPtr();
	void*				indexBufferGPLData	= indexBufferGPLAlloc.getHostPtr();
	void*				valuesBufferGPLData	= valuesBufferGPLAlloc.getHostPtr();

	deMemset(indexBufferMonolithicData, 0, sizeof(uint32_t));
	deMemset(valuesBufferMonolithicData, 0, sizeof(ValueBuffer));
	deMemset(indexBufferGPLData, 0, sizeof(uint32_t));
	deMemset(valuesBufferGPLData, 0, sizeof(ValueBuffer));

	flushAlloc(vkd, device, indexBufferMonolithicAlloc);
	flushAlloc(vkd, device, valuesBufferMonolithicAlloc);
	flushAlloc(vkd, device, indexBufferGPLAlloc);
	flushAlloc(vkd, device, valuesBufferGPLAlloc);

	runTest(indexBufferMonolithic, valuesBufferMonolithic, valueBufferSize, vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
	runTest(indexBufferGPL, valuesBufferGPL, valueBufferSize, vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY);

	invalidateAlloc(vkd, device, indexBufferMonolithicAlloc);
	invalidateAlloc(vkd, device, valuesBufferMonolithicAlloc);
	invalidateAlloc(vkd, device, indexBufferGPLAlloc);
	invalidateAlloc(vkd, device, valuesBufferGPLAlloc);

	uint32_t monolithicIndex;
	uint32_t GPLIndex;
	struct ValueBuffer monolithicResult		= ValueBuffer();
	struct ValueBuffer GPLResult			= ValueBuffer();
	memcpy((void*)&monolithicIndex, indexBufferMonolithicData, sizeof(uint32_t));
	memcpy((void*)&GPLIndex, indexBufferGPLData, sizeof(uint32_t));
	memcpy((void*)&monolithicResult, valuesBufferMonolithicData, sizeof(ValueBuffer));
	memcpy((void*)&GPLResult, valuesBufferGPLData, sizeof(ValueBuffer));

	//we can't know which order the shaders will run in
	std::sort(monolithicResult.values, monolithicResult.values + resultSize);
	std::sort(GPLResult.values, GPLResult.values + resultSize);

	//check that the atomic counters are at enough for the number of invocations
	constexpr int expected = (PipelineLibraryInterpolateAtSampleTestCase::sampleCount + 1) *
		PipelineLibraryInterpolateAtSampleTestCase::width * PipelineLibraryInterpolateAtSampleTestCase::height;

	if (monolithicIndex < expected && GPLIndex < expected) {
			return tcu::TestStatus::fail("Atomic counter value lower than expected");
	}

	for (uint32_t i = 1; i < PipelineLibraryInterpolateAtSampleTestCase::ResultCount; i++) {
		if (monolithicResult.values[i] != monolithicResult.values[i]) {
			return tcu::TestStatus::fail("Comparison failed");
		}
	}

	return tcu::TestStatus::pass("Pass");
}
#endif

struct BindingTestConfig {
	PipelineConstructionType construction;
	bool backwardsBinding;
	bool holes;
};

/*
 * Test the following behaviours:
 * Descriptor sets updated/bound in backwards order
 * Descriptor sets with index holes updated/bound/used
 */
class PipelineLayoutBindingTestCases : public vkt::TestCase
{
public:
	PipelineLayoutBindingTestCases		(tcu::TestContext&                  testCtx,
											 const std::string&                 name,
											 const std::string&                 description,
											 const BindingTestConfig&			config)
		: vkt::TestCase(testCtx, name, description)
		, m_config(config)
	{
	}
	~PipelineLayoutBindingTestCases		    (void) {}
	void			initPrograms				(SourceCollections& programCollection) const override;
	void			checkSupport				(Context& context) const override;
	TestInstance*	createInstance				(Context& context) const override;

	const BindingTestConfig m_config;
};

class PipelineLayoutBindingTestInstance : public vkt::TestInstance
{
public:
	PipelineLayoutBindingTestInstance	(Context& context,
										 const BindingTestConfig& config)
		: vkt::TestInstance				(context)
		, m_renderSize					(2, 2)
		, m_extent						(makeExtent3D(m_renderSize.x(), m_renderSize.y(), 1u))
		, m_format						(VK_FORMAT_R8G8B8A8_UNORM)
		, m_graphicsPipeline			(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), config.construction)
		, m_config						(config)
	{
	}
	~PipelineLayoutBindingTestInstance	(void) {}
	tcu::TestStatus		iterate				(void) override;

private:
	const tcu::UVec2            m_renderSize;
	const VkExtent3D		    m_extent;
	const VkFormat		        m_format;
	GraphicsPipelineWrapper		m_graphicsPipeline;
	const BindingTestConfig		m_config;
};

TestInstance* PipelineLayoutBindingTestCases::createInstance (Context& context) const
{
	return new PipelineLayoutBindingTestInstance(context, m_config);
}

void PipelineLayoutBindingTestCases::checkSupport (Context &context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_config.construction);
}

void PipelineLayoutBindingTestCases::initPrograms(SourceCollections& sources) const
{
	std::ostringstream src;
	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "vec2 positions[3] = vec2[](\n"
		<< "		vec2(-1.0, -1.0),"
		<< "		vec2(3.0, -1.0),"
		<< "		vec2(-1.0, 3.0)"
		<< ");\n"
		<< "void main() {\n"
		<< "		gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
		<< "}";
	sources.glslSources.add("vert") << glu::VertexSource(src.str());

	std::ostringstream frag;
	frag
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "layout(set = 0, binding = 0) uniform Output0 {"
		<< "	uint data;"
		<< "} buf0;\n";
		if (!m_config.holes) {
			frag
				<< "layout(set = 1, binding = 0) uniform Output1 {"
				<< "	uint data;"
				<< "} buf1;\n"
				<< "layout(set = 2, binding = 0) uniform Output2 {"
				<< "	uint data;"
				<< "} buf2;\n"
				<< "\n";
		}
	frag
				<< "layout(set = 3, binding = 0) uniform Output3 {"
				<< "	uint data;"
				<< "} buf3;\n"
				<< "void main ()\n"
				<< "{\n"
				<< "    const vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
				<< "    const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n";
		if (!m_config.holes) {
			frag
				<< "    outColor = ((buf0.data == 0) && (buf1.data == 1) && (buf2.data == 2) && (buf3.data == 3)) ? green : red;\n";
		} else {
			frag
				<< "    outColor = ((buf0.data == 0) && (buf3.data == 3)) ? green : red;\n";
		}
		frag << "}\n";
	sources.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus PipelineLayoutBindingTestInstance::iterate ()
{
	const auto&			vkd					= m_context.getDeviceInterface();
	const auto			device				= m_context.getDevice();
	auto&				alloc				= m_context.getDefaultAllocator();
	const auto			qIndex				= m_context.getUniversalQueueFamilyIndex();
	const auto			queue				= m_context.getUniversalQueue();
	const auto			tcuFormat			= mapVkFormat(m_format);
	const auto			colorUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			verifBufferUsage	= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const tcu::Vec4		clearColor			(0.0f, 0.0f, 0.0f, 1.0f);

	// Color attachment.
	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		m_format,								//	VkFormat				format;
		m_extent,								//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory		colorBuffer		(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto			colorSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto			colorSRL		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto			colorBufferView	= makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, m_format, colorSRR);

	// Verification buffer.
	const auto			verifBufferSize		= static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat)) * m_extent.width * m_extent.height;
	const auto			verifBufferInfo		= makeBufferCreateInfo(verifBufferSize, verifBufferUsage);
	BufferWithMemory	verifBuffer			(vkd, device, alloc, verifBufferInfo, MemoryRequirement::HostVisible);
	auto&				verifBufferAlloc	= verifBuffer.getAllocation();

	// Render pass and framebuffer.
	RenderPassWrapper	renderPass			(m_config.construction, vkd, device, m_format);
	renderPass.createFramebuffer(vkd, device, colorBuffer.get(), colorBufferView.get(), m_extent.width, m_extent.height);

	// Shader modules.
	const auto&		binaries		= m_context.getBinaryCollection();
	const auto		vertModule		= ShaderWrapper(vkd, device, binaries.get("vert"));
	const auto		fragModule		= ShaderWrapper(vkd, device, binaries.get("frag"));

	// Viewports and scissors.
	const std::vector<VkViewport>	viewports	(1u, makeViewport(m_extent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(m_extent));

	const VkPipelineVertexInputStateCreateInfo		vertexInputState	= initVulkanStructure();
	const VkPipelineRasterizationStateCreateInfo    rasterizationState  =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags  flags;
		VK_FALSE,														// VkBool32                                 depthClampEnable;
		VK_FALSE,														// VkBool32                                 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
	};

	std::array<int, 4> tmpIndices = {};
	std::array<int, 4> indices = {};
	std::iota(tmpIndices.begin(), tmpIndices.end(), 0);
	if (m_config.backwardsBinding) {
		std::copy(tmpIndices.rbegin(), tmpIndices.rend(), indices.begin());
	} else {
		std::copy(tmpIndices.begin(), tmpIndices.end(), indices.begin());
	}

	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

	std::vector<vk::Move<VkDescriptorSetLayout>>  descriptorSetLayouts = {};

	for (size_t i = 0; i < indices.size(); i++) {
		descriptorSetLayouts.emplace_back(layoutBuilder.build(vkd, device));
	}

	// Pipeline layout and graphics pipeline.
	uint32_t setAndDescriptorCount = de::sizeU32(indices);
	const vk::PipelineLayoutWrapper pipelineLayout	(m_config.construction, vkd, device, descriptorSetLayouts);
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setAndDescriptorCount);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, setAndDescriptorCount);
	std::vector<vk::Move<VkDescriptorSet>> descriptorSetsWrap = {};
	std::vector<VkDescriptorSet> descriptorSets = {};

	for (const auto& setLayout : descriptorSetLayouts) {
		descriptorSetsWrap.emplace_back(makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get()));
	}

	for (size_t i = 0; i < indices.size(); i++) {
		descriptorSets.emplace_back(descriptorSetsWrap[i].get());
	}

	const auto			bufferSize		= static_cast<VkDeviceSize>(sizeof(uint32_t));
	std::vector<std::unique_ptr<BufferWithMemory>> buffers;
	//create uniform buffers
	for (size_t i = 0; i < indices.size(); i++) {
		auto outBufferInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		auto buffer = std::unique_ptr<vk::BufferWithMemory>(new vk::BufferWithMemory(vkd, device, alloc, outBufferInfo, vk::MemoryRequirement::HostVisible));
		auto&				bufferAlloc		= buffer->getAllocation();
		uint32_t*			bufferData		= (uint32_t*)bufferAlloc.getHostPtr();
		*bufferData = (uint32_t)i;
		flushAlloc(vkd, device, bufferAlloc);
		buffers.push_back(std::move(buffer));
	}

	DescriptorSetUpdateBuilder updater;

	for (auto i : indices) {
		const auto bufferInfo = makeDescriptorBufferInfo(buffers[i]->get(), 0ull, bufferSize);
		updater.writeSingle(descriptorSets[i], DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo);
		updater.update(vkd, device);
	}

	const auto topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_graphicsPipeline.setDefaultRasterizationState()
		.setDefaultTopology(topology)
		.setupVertexInputState(&vertexInputState)
		.setDefaultDepthStencilState()
		.setDefaultMultisampleState()
		.setDefaultColorBlendState()
		.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule, &rasterizationState)
		.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
		.setupFragmentOutputState(*renderPass)
		.setMonolithicPipelineLayout(pipelineLayout)
		.buildPipeline();

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Draw.
	renderPass.begin(vkd, cmdBuffer, scissors.at(0u), clearColor);
	for (auto i : indices) {
		if (m_config.holes && ((i == 1) || (i == 2))) {
			continue;
		}
		vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), i, 1, &descriptorSets[i], 0u, nullptr);
	}
	m_graphicsPipeline.bind(cmdBuffer);
	vkd.cmdDraw(cmdBuffer, 3, 1u, 0u, 0u);
	renderPass.end(vkd, cmdBuffer);

	// Copy to verification buffer.
	const auto copyRegion		= makeBufferImageCopy(m_extent, colorSRL);
	const auto transfer2Host	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const auto color2Transfer	= makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorBuffer.get(), colorSRR);

	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &color2Transfer);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer.get(), 1u, &copyRegion);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &transfer2Host);

	endCommandBuffer(vkd, cmdBuffer);

	// Submit and validate result.
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	const tcu::IVec3					iExtent (static_cast<int>(m_extent.width), static_cast<int>(m_extent.height), static_cast<int>(m_extent.depth));
	void*								verifBufferData		= verifBufferAlloc.getHostPtr();
	const tcu::ConstPixelBufferAccess	verifAccess		(tcuFormat, iExtent, verifBufferData);
	invalidateAlloc(vkd, device, verifBufferAlloc);

	const auto green = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	tcu::TextureLevel referenceLevel(mapVkFormat(m_format), m_extent.height, m_extent.height);
	tcu::PixelBufferAccess reference = referenceLevel.getAccess();
	tcu::clear(reference, green);

	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", reference, verifAccess, tcu::Vec4(0.0), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Image comparison failed");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createMiscTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> miscTests (new tcu::TestCaseGroup(testCtx, "misc", ""));

	// Location of the Amber script files under the data/vulkan/amber source tree.
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
		addMonolithicAmberTests(miscTests.get());

	miscTests->addChild(new ImplicitPrimitiveIDPassthroughCase(testCtx, "implicit_primitive_id", "Verify implicit access to gl_PrimtiveID works", pipelineConstructionType, false));
	miscTests->addChild(new ImplicitPrimitiveIDPassthroughCase(testCtx, "implicit_primitive_id_with_tessellation", "Verify implicit access to gl_PrimtiveID works with a tessellation shader", pipelineConstructionType, true));
	#ifndef CTS_USES_VULKANSC
	if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY) {
		miscTests->addChild(new PipelineLibraryInterpolateAtSampleTestCase(testCtx, "interpolate_at_sample_no_sample_shading", "Check if interpolateAtSample works as expected when using a pipeline library and null MSAA state in the fragment shader"));
	}
	#endif

#ifndef CTS_USES_VULKANSC
	if (isConstructionTypeLibrary(pipelineConstructionType))
	{
		for (int useTessIdx = 0; useTessIdx < 2; ++useTessIdx)
			for (int useGeomIdx = 0; useGeomIdx < 2; ++useGeomIdx)
			{
				const bool useTess = (useTessIdx > 0);
				const bool useGeom = (useGeomIdx > 0);

				std::string testName = "unused_shader_stages";

				if (useTess)
					testName += "_include_tess";

				if (useGeom)
					testName += "_include_geom";

				const UnusedShaderStageParams params { pipelineConstructionType, useTess, useGeom };
				miscTests->addChild(new UnusedShaderStagesCase(testCtx, testName, "", params));
			}
	}
#endif // CTS_USES_VULKANSC

	BindingTestConfig config0 = {pipelineConstructionType, true, false};
	BindingTestConfig config1 = {pipelineConstructionType, false, true};
	BindingTestConfig config2 = {pipelineConstructionType, true, true};

	miscTests->addChild(new PipelineLayoutBindingTestCases(testCtx, "descriptor_bind_test_backwards", "Verify implicit access to gl_PrimtiveID works with a tessellation shader", config0));
	miscTests->addChild(new PipelineLayoutBindingTestCases(testCtx, "descriptor_bind_test_holes", "Verify implicit access to gl_PrimtiveID works with a tessellation shader", config1));
	miscTests->addChild(new PipelineLayoutBindingTestCases(testCtx, "descriptor_bind_test_backwards_holes", "Verify implicit access to gl_PrimtiveID works with a tessellation shader", config2));

	return miscTests.release();
}

} // pipeline
} // vkt
