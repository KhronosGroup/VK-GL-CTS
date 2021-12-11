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
 * \brief Mesh Shader Misc Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderMiscTests.hpp"
#include "vktTestCase.hpp"

#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuMaybe.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

#include "deRandom.hpp"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <limits>

namespace vkt
{
namespace MeshShader
{

namespace
{

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

using namespace vk;

// Output images will use this format.
VkFormat getOutputFormat ()
{
	return VK_FORMAT_R8G8B8A8_UNORM;
}

// Threshold that's reasonable for the previous format.
float getCompareThreshold ()
{
	return 0.005f; // 1/256 < 0.005 < 2/256
}

// Check mesh shader support.
void genericCheckSupport (Context& context, bool requireTaskShader, bool requireVertexStores)
{
	context.requireDeviceFunctionality("VK_NV_mesh_shader");

	const auto& meshFeatures = context.getMeshShaderFeatures();

	if (!meshFeatures.meshShader)
		TCU_THROW(NotSupportedError, "Mesh shader not supported");

	if (requireTaskShader && !meshFeatures.taskShader)
		TCU_THROW(NotSupportedError, "Task shader not supported");

	if (requireVertexStores)
	{
		const auto& features = context.getDeviceFeatures();
		if (!features.vertexPipelineStoresAndAtomics)
			TCU_THROW(NotSupportedError, "Vertex pieline stores and atomics not supported");
	}
}

struct MiscTestParams
{
	tcu::Maybe<uint32_t>	taskCount;
	uint32_t				meshCount;

	uint32_t				width;
	uint32_t				height;

	MiscTestParams (const tcu::Maybe<uint32_t>& taskCount_, uint32_t meshCount_, uint32_t width_, uint32_t height_)
		: taskCount		(taskCount_)
		, meshCount		(meshCount_)
		, width			(width_)
		, height		(height_)
	{}

	// Makes the class polymorphic and allows the right destructor to be used for subclasses.
	virtual ~MiscTestParams () {}

	bool needsTaskShader () const
	{
		return static_cast<bool>(taskCount);
	}

	uint32_t drawCount () const
	{
		if (needsTaskShader())
			return taskCount.get();
		return meshCount;
	}
};

using ParamsPtr = std::unique_ptr<MiscTestParams>;

class MeshShaderMiscCase : public vkt::TestCase
{
public:
					MeshShaderMiscCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params);
	virtual			~MeshShaderMiscCase		(void) {}

	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;

protected:
	std::unique_ptr<MiscTestParams> m_params;
};

MeshShaderMiscCase::MeshShaderMiscCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params.release())
{}

void MeshShaderMiscCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, m_params->needsTaskShader(), /*requireVertexStores*/false);
}

// Adds the generic fragment shader. To be called by subclasses.
void MeshShaderMiscCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::string frag =
		"#version 450\n"
		"#extension GL_NV_mesh_shader : enable\n"
		"\n"
		"layout (location=0) in perprimitiveNV vec4 primitiveColor;\n"
		"layout (location=0) out vec4 outColor;\n"
		"\n"
		"void main ()\n"
		"{\n"
		"    outColor = primitiveColor;\n"
		"}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
}

class MeshShaderMiscInstance : public vkt::TestInstance
{
public:
					MeshShaderMiscInstance	(Context& context, const MiscTestParams* params)
						: vkt::TestInstance	(context)
						, m_params			(params)
						, m_referenceLevel	()
					{
					}

	void			generateSolidRefLevel	(const tcu::Vec4& color, std::unique_ptr<tcu::TextureLevel>& output);
	virtual void	generateReferenceLevel	() = 0;

	virtual bool	verifyResult			(const tcu::ConstPixelBufferAccess& resultAccess, const tcu::TextureLevel& referenceLevel) const;
	virtual bool	verifyResult			(const tcu::ConstPixelBufferAccess& resultAccess) const;
	tcu::TestStatus	iterate					() override;

protected:
	const MiscTestParams*				m_params;
	std::unique_ptr<tcu::TextureLevel>	m_referenceLevel;
};

void MeshShaderMiscInstance::generateSolidRefLevel (const tcu::Vec4& color, std::unique_ptr<tcu::TextureLevel>& output)
{
	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);

	output.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto access		= output->getAccess();

	// Fill with solid color.
	tcu::clear(access, color);
}

bool MeshShaderMiscInstance::verifyResult (const tcu::ConstPixelBufferAccess& resultAccess) const
{
	return verifyResult(resultAccess, *m_referenceLevel);
}

bool MeshShaderMiscInstance::verifyResult (const tcu::ConstPixelBufferAccess& resultAccess, const tcu::TextureLevel& referenceLevel) const
{
	const auto referenceAccess = referenceLevel.getAccess();

	const auto refWidth		= referenceAccess.getWidth();
	const auto refHeight	= referenceAccess.getHeight();
	const auto refDepth		= referenceAccess.getDepth();

	const auto resWidth		= resultAccess.getWidth();
	const auto resHeight	= resultAccess.getHeight();
	const auto resDepth		= resultAccess.getDepth();

	DE_ASSERT(resWidth == refWidth || resHeight == refHeight || resDepth == refDepth);

	// For release builds.
	DE_UNREF(refWidth);
	DE_UNREF(refHeight);
	DE_UNREF(refDepth);
	DE_UNREF(resWidth);
	DE_UNREF(resHeight);
	DE_UNREF(resDepth);

	const auto outputFormat		= getOutputFormat();
	const auto expectedFormat	= mapVkFormat(outputFormat);
	const auto resFormat		= resultAccess.getFormat();
	const auto refFormat		= referenceAccess.getFormat();

	DE_ASSERT(resFormat == expectedFormat && refFormat == expectedFormat);

	// For release builds
	DE_UNREF(expectedFormat);
	DE_UNREF(resFormat);
	DE_UNREF(refFormat);

	auto&			log				= m_context.getTestContext().getLog();
	const auto		threshold		= getCompareThreshold();
	const tcu::Vec4	thresholdVec	(threshold, threshold, threshold, threshold);

	return tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, thresholdVec, tcu::COMPARE_LOG_ON_ERROR);
}

tcu::TestStatus MeshShaderMiscInstance::iterate ()
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();

	const auto		imageFormat	= getOutputFormat();
	const auto		tcuFormat	= mapVkFormat(imageFormat);
	const auto		imageExtent	= makeExtent3D(m_params->width, m_params->height, 1u);
	const auto		imageUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		imageFormat,							//	VkFormat				format;
		imageExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		imageUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	// Create color image and view.
	ImageWithMemory	colorImage	(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto		colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto		colorView	= makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

	// Create a memory buffer for verification.
	const auto			verificationBufferSize	= static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
	const auto			verificationBufferUsage	= (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const auto			verificationBufferInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

	BufferWithMemory	verificationBuffer		(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc	= verificationBuffer.getAllocation();
	void*				verificationBufferData	= verificationBufferAlloc.getHostPtr();

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device);

	// Shader modules.
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	hasTask		= binaries.contains("task");

	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));

	Move<VkShaderModule> taskShader;
	if (hasTask)
		taskShader = createShaderModule(vkd, device, binaries.get("task"));

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, imageFormat);

	// Framebuffer.
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

	// Viewport and scissor.
	const std::vector<VkViewport>	viewports	(1u, makeViewport(imageExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(imageExtent));

	// Color blending.
	const auto									colorWriteMask	= (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
	const VkPipelineColorBlendAttachmentState	blendAttState	=
	{
		VK_TRUE,				//	VkBool32				blendEnable;
		VK_BLEND_FACTOR_ONE,	//	VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ONE,	//	VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,		//	VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,	//	VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ONE,	//	VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		//	VkBlendOp				alphaBlendOp;
		colorWriteMask,			//	VkColorComponentFlags	colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,													//	const void*									pNext;
		0u,															//	VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,													//	VkBool32									logicOpEnable;
		VK_LOGIC_OP_OR,												//	VkLogicOp									logicOp;
		1u,															//	uint32_t									attachmentCount;
		&blendAttState,												//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									//	float										blendConstants[4];
	};

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskShader.get(), meshShader.get(), fragShader.get(),
		renderPass.get(), viewports, scissors, 0u/*subpass*/,
		nullptr, nullptr, nullptr, &colorBlendInfo);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Run pipeline.
	const tcu::Vec4 clearColor (0.0f, 0.0f, 0.0f, 0.0f);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params->drawCount(), 0u);
	endRenderPass(vkd, cmdBuffer);

	// Copy color buffer to verification buffer.
	const auto colorAccess		= (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	const auto transferRead		= VK_ACCESS_TRANSFER_READ_BIT;
	const auto transferWrite	= VK_ACCESS_TRANSFER_WRITE_BIT;
	const auto hostRead			= VK_ACCESS_HOST_READ_BIT;

	const auto preCopyBarrier	= makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
	const auto postCopyBarrier	= makeMemoryBarrier(transferWrite, hostRead);
	const auto copyRegion		= makeBufferImageCopy(imageExtent, colorSRL);

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &postCopyBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Generate reference image and compare results.
	const tcu::IVec3					iExtent				(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
	const tcu::ConstPixelBufferAccess	verificationAccess	(tcuFormat, iExtent, verificationBufferData);

	generateReferenceLevel();
	invalidateAlloc(vkd, device, verificationBufferAlloc);
	if (!verifyResult(verificationAccess))
		TCU_FAIL("Result does not match reference; check log for details");

	return tcu::TestStatus::pass("Pass");
}

// Verify passing more complex data between the task and mesh shaders.
class ComplexTaskDataCase : public MeshShaderMiscCase
{
public:
					ComplexTaskDataCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class ComplexTaskDataInstance : public MeshShaderMiscInstance
{
public:
	ComplexTaskDataInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

void ComplexTaskDataInstance::generateReferenceLevel ()
{
	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);

	const auto halfWidth	= iWidth / 2;
	const auto halfHeight	= iHeight / 2;

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto access		= m_referenceLevel->getAccess();

	// Each image quadrant gets a different color.
	for (int y = 0; y < iHeight; ++y)
	for (int x = 0; x < iWidth; ++x)
	{
		const float	red			= ((y < halfHeight) ? 0.0f : 1.0f);
		const float	green		= ((x < halfWidth)  ? 0.0f : 1.0f);
		const auto	refColor	= tcu::Vec4(red, green, 1.0f, 1.0f);
		access.setPixel(refColor, x, y);
	}
}

void ComplexTaskDataCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Add the generic fragment shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	const std::string taskDataDeclTemplate =
		"struct RowId {\n"
		"    uint id;\n"
		"};\n"
		"\n"
		"struct WorkGroupData {\n"
		"    float WorkGroupIdPlusOnex1000Iota[10];\n"
		"    RowId rowId;\n"
		"    uvec3 WorkGroupIdPlusOnex2000Iota;\n"
		"    vec2  WorkGroupIdPlusOnex3000Iota;\n"
		"};\n"
		"\n"
		"struct ExternalData {\n"
		"    float OneMillion;\n"
		"    uint  TwoMillion;\n"
		"    WorkGroupData workGroupData;\n"
		"};\n"
		"\n"
		"${INOUT} taskNV TaskData {\n"
		"    uint yes;\n"
		"    ExternalData externalData;\n"
		"} td;\n"
		;
	const tcu::StringTemplate taskDataDecl(taskDataDeclTemplate);

	{
		std::map<std::string, std::string> taskMap;
		taskMap["INOUT"] = "out";
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "\n"
			<< taskDataDecl.specialize(taskMap)
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = 2u;\n"
			<< "    td.yes = 1u;\n"
			<< "    td.externalData.OneMillion = 1000000.0;\n"
			<< "    td.externalData.TwoMillion = 2000000u;\n"
			<< "    for (uint i = 0; i < 10; i++) {\n"
			<< "        td.externalData.workGroupData.WorkGroupIdPlusOnex1000Iota[i] = float((gl_WorkGroupID.x + 1u) * 1000 + i);\n"
			<< "    }\n"
			<< "    {\n"
			<< "        uint baseVal = (gl_WorkGroupID.x + 1u) * 2000;\n"
			<< "        td.externalData.workGroupData.WorkGroupIdPlusOnex2000Iota = uvec3(baseVal, baseVal + 1, baseVal + 2);\n"
			<< "    }\n"
			<< "    {\n"
			<< "        uint baseVal = (gl_WorkGroupID.x + 1u) * 3000;\n"
			<< "        td.externalData.workGroupData.WorkGroupIdPlusOnex3000Iota = vec2(baseVal, baseVal + 1);\n"
			<< "    }\n"
			<< "    td.externalData.workGroupData.rowId.id = gl_WorkGroupID.x;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	{
		std::map<std::string, std::string> meshMap;
		meshMap["INOUT"] = "in";
		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=2) in;\n"
			<< "layout(triangles) out;\n"
			<< "layout(max_vertices=4, max_primitives=2) out;\n"
			<< "\n"
			<< "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
			<< "\n"
			<< taskDataDecl.specialize(meshMap)
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    bool dataOK = true;\n"
			<< "    dataOK = (dataOK && (td.yes == 1u));\n"
			<< "    dataOK = (dataOK && (td.externalData.OneMillion == 1000000.0 && td.externalData.TwoMillion == 2000000u));\n"
			<< "    uint rowId = td.externalData.workGroupData.rowId.id;\n"
			<< "    dataOK = (dataOK && (rowId == 0u || rowId == 1u));\n"
			<< "\n"
			<< "    {\n"
			<< "        uint baseVal = (rowId + 1u) * 1000u;\n"
			<< "        for (uint i = 0; i < 10; i++) {\n"
			<< "            if (td.externalData.workGroupData.WorkGroupIdPlusOnex1000Iota[i] != float(baseVal + i)) {\n"
			<< "                dataOK = false;\n"
			<< "                break;\n"
			<< "            }\n"
			<< "        }\n"
			<< "    }\n"
			<< "\n"
			<< "    {\n"
			<< "        uint baseVal = (rowId + 1u) * 2000;\n"
			<< "        uvec3 expected = uvec3(baseVal, baseVal + 1, baseVal + 2);\n"
			<< "        if (td.externalData.workGroupData.WorkGroupIdPlusOnex2000Iota != expected) {\n"
			<< "            dataOK = false;\n"
			<< "        }\n"
			<< "    }\n"
			<< "\n"
			<< "    {\n"
			<< "        uint baseVal = (rowId + 1u) * 3000;\n"
			<< "        vec2 expected = vec2(baseVal, baseVal + 1);\n"
			<< "        if (td.externalData.workGroupData.WorkGroupIdPlusOnex3000Iota != expected) {\n"
			<< "            dataOK = false;\n"
			<< "        }\n"
			<< "    }\n"
			<< "\n"
			<< "    uint columnId = gl_WorkGroupID.x;\n"
			<< "\n"
			<< "    if (dataOK) {\n"
			<< "        gl_PrimitiveCountNV = 2u;\n"
			<< "    }\n"
			<< "    else {\n"
			<< "        gl_PrimitiveCountNV = 0u;\n"
			<< "        return;\n"
			<< "    }\n"
			<< "\n"
			<< "    const vec4 outColor = vec4(rowId, columnId, 1.0f, 1.0f);\n"
			<< "    triangleColor[0] = outColor;\n"
			<< "    triangleColor[1] = outColor;\n"
			<< "\n"
			<< "    // Each local invocation will generate two points and one triangle from the quad.\n"
			<< "    // The first local invocation will generate the top quad vertices.\n"
			<< "    // The second invocation will generate the two bottom vertices.\n"
			<< "    vec4 left  = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "    vec4 right = vec4(1.0, 0.0, 0.0, 1.0);\n"
			<< "\n"
			<< "    float localInvocationOffsetY = float(gl_LocalInvocationID.x);\n"
			<< "    left.y  += localInvocationOffsetY;\n"
			<< "    right.y += localInvocationOffsetY;\n"
			<< "\n"
			<< "    // The code above creates a quad from (0, 0) to (1, 1) but we need to offset it\n"
			<< "    // in X and/or Y depending on the row and column, to place it in other quadrants.\n"
			<< "    float quadrantOffsetX = float(int(columnId) - 1);\n"
			<< "    float quadrantOffsetY = float(int(rowId) - 1);\n"
			<< "\n"
			<< "    left.x  += quadrantOffsetX;\n"
			<< "    right.x += quadrantOffsetX;\n"
			<< "\n"
			<< "    left.y  += quadrantOffsetY;\n"
			<< "    right.y += quadrantOffsetY;\n"
			<< "\n"
			<< "    uint baseVertexId = 2*gl_LocalInvocationID.x;\n"
			<< "    gl_MeshVerticesNV[baseVertexId + 0].gl_Position = left;\n"
			<< "    gl_MeshVerticesNV[baseVertexId + 1].gl_Position = right;\n"
			<< "\n"
			<< "    uint baseIndexId = 3*gl_LocalInvocationID.x;\n"
			<< "    // 0,1,2 or 1,2,3 (note: triangles alternate front face this way)\n"
			<< "    gl_PrimitiveIndicesNV[baseIndexId + 0] = 0 + gl_LocalInvocationID.x;\n"
			<< "    gl_PrimitiveIndicesNV[baseIndexId + 1] = 1 + gl_LocalInvocationID.x;\n"
			<< "    gl_PrimitiveIndicesNV[baseIndexId + 2] = 2 + gl_LocalInvocationID.x;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}
}

TestInstance* ComplexTaskDataCase::createInstance (Context& context) const
{
	return new ComplexTaskDataInstance(context, m_params.get());
}

// Verify drawing a single point.
class SinglePointCase : public MeshShaderMiscCase
{
public:
					SinglePointCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class SinglePointInstance : public MeshShaderMiscInstance
{
public:
	SinglePointInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* SinglePointCase::createInstance (Context& context) const
{
	return new SinglePointInstance (context, m_params.get());
}

void SinglePointCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_params->needsTaskShader());

	MeshShaderMiscCase::initPrograms(programCollection);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(points) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 pointColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 1u;\n"
		<< "    pointColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
		<< "    gl_MeshVerticesNV[0].gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesNV[0].gl_PointSize = 1.0f;\n"
		<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void SinglePointInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_referenceLevel);

	const auto halfWidth	= static_cast<int>(m_params->width / 2u);
	const auto halfHeight	= static_cast<int>(m_params->height / 2u);
	const auto access		= m_referenceLevel->getAccess();

	access.setPixel(tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f), halfWidth, halfHeight);
}

// Verify drawing a single line.
class SingleLineCase : public MeshShaderMiscCase
{
public:
					SingleLineCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class SingleLineInstance : public MeshShaderMiscInstance
{
public:
	SingleLineInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* SingleLineCase::createInstance (Context& context) const
{
	return new SingleLineInstance (context, m_params.get());
}

void SingleLineCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_params->needsTaskShader());

	MeshShaderMiscCase::initPrograms(programCollection);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(lines) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 lineColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 1u;\n"
		<< "    lineColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
		<< "    gl_MeshVerticesNV[0].gl_Position = vec4(-1.0f, 0.0f, 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesNV[1].gl_Position = vec4( 1.0f, 0.0f, 0.0f, 1.0f);\n"
		<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
		<< "    gl_PrimitiveIndicesNV[1] = 1;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void SingleLineInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_referenceLevel);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto halfHeight	= static_cast<int>(m_params->height / 2u);
	const auto access		= m_referenceLevel->getAccess();

	// Center row.
	for (int x = 0; x < iWidth; ++x)
		access.setPixel(tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f), x, halfHeight);
}

// Verify drawing a single triangle.
class SingleTriangleCase : public MeshShaderMiscCase
{
public:
					SingleTriangleCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class SingleTriangleInstance : public MeshShaderMiscInstance
{
public:
	SingleTriangleInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* SingleTriangleCase::createInstance (Context& context) const
{
	return new SingleTriangleInstance (context, m_params.get());
}

void SingleTriangleCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_params->needsTaskShader());

	MeshShaderMiscCase::initPrograms(programCollection);

	const float halfPixelX = 2.0f / static_cast<float>(m_params->width);
	const float halfPixelY = 2.0f / static_cast<float>(m_params->height);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 1u;\n"
		<< "    triangleColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
		<< "    gl_MeshVerticesNV[0].gl_Position = vec4(" <<  halfPixelY << ", " << -halfPixelX << ", 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesNV[1].gl_Position = vec4(" <<  halfPixelY << ", " <<  halfPixelX << ", 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesNV[2].gl_Position = vec4(" << -halfPixelY << ", 0.0f, 0.0f, 1.0f);\n"
		<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
		<< "    gl_PrimitiveIndicesNV[1] = 1;\n"
		<< "    gl_PrimitiveIndicesNV[2] = 2;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void SingleTriangleInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_referenceLevel);

	const auto halfWidth	= static_cast<int>(m_params->width / 2u);
	const auto halfHeight	= static_cast<int>(m_params->height / 2u);
	const auto access		= m_referenceLevel->getAccess();

	// Single pixel in the center.
	access.setPixel(tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f), halfWidth, halfHeight);
}

// Verify drawing the maximum number of points.
class MaxPointsCase : public MeshShaderMiscCase
{
public:
					MaxPointsCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class MaxPointsInstance : public MeshShaderMiscInstance
{
public:
	MaxPointsInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* MaxPointsCase::createInstance (Context& context) const
{
	return new MaxPointsInstance (context, m_params.get());
}

void MaxPointsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_params->needsTaskShader());

	MeshShaderMiscCase::initPrograms(programCollection);

	// Fill a 16x16 image with 256 points. Each of the 32 local invocations will handle a segment of 8 pixels. Two segments per row.
	DE_ASSERT(m_params->width == 16u && m_params->height == 16u);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=32) in;\n"
		<< "layout(points) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 pointColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 256u;\n"
		<< "    uint firstPixel = 8u * gl_LocalInvocationID.x;\n"
		<< "    uint row = firstPixel / 16u;\n"
		<< "    uint col = firstPixel % 16u;\n"
		<< "    float pixSize = 2.0f / 16.0f;\n"
		<< "    float yCoord = pixSize * (float(row) + 0.5f) - 1.0f;\n"
		<< "    float baseXCoord = pixSize * (float(col) + 0.5f) - 1.0f;\n"
		<< "    for (uint i = 0; i < 8u; i++) {\n"
		<< "        float xCoord = baseXCoord + pixSize * float(i);\n"
		<< "        uint pixId = firstPixel + i;\n"
		<< "        gl_MeshVerticesNV[pixId].gl_Position = vec4(xCoord, yCoord, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesNV[pixId].gl_PointSize = 1.0f;\n"
		<< "        gl_PrimitiveIndicesNV[pixId] = pixId;\n"
		<< "        pointColor[pixId] = vec4(((xCoord + 1.0f) / 2.0f), ((yCoord + 1.0f) / 2.0f), 0.0f, 1.0f);\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaxPointsInstance::generateReferenceLevel ()
{
	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);
	const auto fWidth		= static_cast<float>(m_params->width);
	const auto fHeight		= static_cast<float>(m_params->height);

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto access		= m_referenceLevel->getAccess();

	// Fill with gradient like the shader does.
	for (int y = 0; y < iHeight; ++y)
	for (int x = 0; x < iWidth; ++x)
	{
		const tcu::Vec4 color (
			((static_cast<float>(x) + 0.5f) / fWidth),
			((static_cast<float>(y) + 0.5f) / fHeight),
			0.0f, 1.0f);
		access.setPixel(color, x, y);
	}
}

// Verify drawing the maximum number of lines.
class MaxLinesCase : public MeshShaderMiscCase
{
public:
					MaxLinesCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class MaxLinesInstance : public MeshShaderMiscInstance
{
public:
	MaxLinesInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* MaxLinesCase::createInstance (Context& context) const
{
	return new MaxLinesInstance (context, m_params.get());
}

void MaxLinesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_params->needsTaskShader());

	MeshShaderMiscCase::initPrograms(programCollection);

	// Fill a 1x1020 image with 255 lines, each line being 4 pixels tall. Each invocation will generate ~8 lines.
	DE_ASSERT(m_params->width == 1u && m_params->height == 1020u);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=32) in;\n"
		<< "layout(lines) out;\n"
		<< "layout(max_vertices=256, max_primitives=255) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 lineColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 255u;\n"
		<< "    uint firstLine = 8u * gl_LocalInvocationID.x;\n"
		<< "    for (uint i = 0u; i < 8u; i++) {\n"
		<< "        uint lineId = firstLine + i;\n"
		<< "        uint topPixel = 4u * lineId;\n"
		<< "        uint bottomPixel = 3u + topPixel;\n"
		<< "        if (bottomPixel < 1020u) {\n"
		<< "            float bottomCoord = ((float(bottomPixel) + 1.0f) / 1020.0) * 2.0 - 1.0;\n"
		<< "            gl_MeshVerticesNV[lineId + 1u].gl_Position = vec4(0.0, bottomCoord, 0.0f, 1.0f);\n"
		<< "            gl_PrimitiveIndicesNV[lineId * 2u] = lineId;\n"
		<< "            gl_PrimitiveIndicesNV[lineId * 2u + 1u] = lineId + 1u;\n"
		<< "            lineColor[lineId] = vec4(0.0f, 1.0f, float(lineId) / 255.0f, 1.0f);\n"
		<< "        } else {\n"
		<< "            // The last iteration of the last invocation emits the first point\n"
		<< "            gl_MeshVerticesNV[0].gl_Position = vec4(0.0, -1.0, 0.0f, 1.0f);\n"
		<< "        }\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaxLinesInstance::generateReferenceLevel ()
{
	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto access		= m_referenceLevel->getAccess();

	// Fill lines, 4 pixels per line.
	const uint32_t kNumLines = 255u;
	const uint32_t kLineHeight = 4u;

	for (uint32_t i = 0u; i < kNumLines; ++i)
	{
		const tcu::Vec4 color (0.0f, 1.0f, static_cast<float>(i) / static_cast<float>(kNumLines), 1.0f);
		for (uint32_t j = 0u; j < kLineHeight; ++j)
			access.setPixel(color, 0, i*kLineHeight + j);
	}
}

// Verify drawing the maximum number of triangles.
class MaxTrianglesCase : public MeshShaderMiscCase
{
public:
					MaxTrianglesCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class MaxTrianglesInstance : public MeshShaderMiscInstance
{
public:
	MaxTrianglesInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* MaxTrianglesCase::createInstance (Context& context) const
{
	return new MaxTrianglesInstance (context, m_params.get());
}

void MaxTrianglesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_params->needsTaskShader());

	MeshShaderMiscCase::initPrograms(programCollection);

	// Fill a sufficiently large image with solid color. Generate a quarter of a circle with the center in the top left corner,
	// using a triangle fan that advances from top to bottom. Each invocation will generate ~8 triangles.
	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=32) in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=256, max_primitives=254) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
		<< "\n"
		<< "const float PI_2 = 1.57079632679489661923;\n"
		<< "const float RADIUS = 4.5;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 254u;\n"
		<< "    uint firstTriangle = 8u * gl_LocalInvocationID.x;\n"
		<< "    for (uint i = 0u; i < 8u; i++) {\n"
		<< "        uint triangleId = firstTriangle + i;\n"
		<< "        if (triangleId < 254u) {\n"
		<< "            uint vertexId = triangleId + 2u;\n"
		<< "            float angleProportion = float(vertexId - 1u) / 254.0f;\n"
		<< "            float angle = PI_2 * angleProportion;\n"
		<< "            float xCoord = cos(angle) * RADIUS - 1.0;\n"
		<< "            float yCoord = sin(angle) * RADIUS - 1.0;\n"
		<< "            gl_MeshVerticesNV[vertexId].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
		<< "            gl_PrimitiveIndicesNV[triangleId * 3u + 0u] = 0u;\n"
		<< "            gl_PrimitiveIndicesNV[triangleId * 3u + 1u] = triangleId + 1u;\n"
		<< "            gl_PrimitiveIndicesNV[triangleId * 3u + 2u] = triangleId + 2u;\n"
		<< "            triangleColor[triangleId] = vec4(0.0f, 0.0f, 1.0f, 1.0f);\n"
		<< "        } else {\n"
		<< "            // The last iterations of the last invocation emit the first two vertices\n"
		<< "            uint vertexId = triangleId - 254u;\n"
		<< "            if (vertexId == 0u) {\n"
		<< "                gl_MeshVerticesNV[0u].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
		<< "            } else {\n"
		<< "                gl_MeshVerticesNV[1u].gl_Position = vec4(RADIUS, -1.0, 0.0, 1.0);\n"
		<< "            }\n"
		<< "        }\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaxTrianglesInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

// Large work groups with many threads.
class LargeWorkGroupCase : public MeshShaderMiscCase
{
public:
					LargeWorkGroupCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

	static constexpr uint32_t kLocalInvocations = 32u;
};

class LargeWorkGroupInstance : public MeshShaderMiscInstance
{
public:
	LargeWorkGroupInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* LargeWorkGroupCase::createInstance (Context& context) const
{
	return new LargeWorkGroupInstance(context, m_params.get());
}

void LargeWorkGroupInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

void LargeWorkGroupCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto useTaskShader	= m_params->needsTaskShader();
	const auto taskMultiplier	= (useTaskShader ? m_params->taskCount.get() : 1u);

	// Add the frag shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	std::ostringstream taskData;
	taskData
		<< "taskNV TaskData {\n"
		<< "    uint parentTask[" << kLocalInvocations << "];\n"
		<< "} td;\n"
		;
	const auto taskDataStr = taskData.str();

	if (useTaskShader)
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
			<< "\n"
			<< "out " << taskDataStr
			<< "\n"
			<< "void main () {\n"
			<< "    gl_TaskCountNV = " << m_params->meshCount << ";\n"
			<< "    td.parentTask[gl_LocalInvocationID.x] = gl_WorkGroupID.x;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	// Needed for the code below to work.
	DE_ASSERT(m_params->width * m_params->height == taskMultiplier * m_params->meshCount * kLocalInvocations);
	DE_UNREF(taskMultiplier); // For release builds.

	// Emit one point per framebuffer pixel. The number of jobs (kLocalInvocations in each mesh shader work group, multiplied by the
	// number of mesh work groups emitted by each task work group) must be the same as the total framebuffer size. Calculate a job
	// ID corresponding to the current mesh shader invocation, and assign a pixel position to it. Draw a point at that position.
	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
		<< "layout (points) out;\n"
		<< "layout (max_vertices=" << kLocalInvocations << ", max_primitives=" << kLocalInvocations << ") out;\n"
		<< "\n"
		<< (useTaskShader ? "in " + taskDataStr : "")
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 pointColor[];\n"
		<< "\n"
		<< "void main () {\n"
		;

	if (useTaskShader)
	{
		mesh
			<< "    uint parentTask = td.parentTask[0];\n"
			<< "    if (td.parentTask[gl_LocalInvocationID.x] != parentTask) {\n"
			<< "        return;\n"
			<< "    }\n"
			;
	}
	else
	{
		mesh << "    uint parentTask = 0;\n";
	}

	mesh
		<< "    gl_PrimitiveCountNV = " << kLocalInvocations << ";\n"
		<< "    uint jobId = ((parentTask * " << m_params->meshCount << ") + gl_WorkGroupID.x) * " << kLocalInvocations << " + gl_LocalInvocationID.x;\n"
		<< "    uint row = jobId / " << m_params->width << ";\n"
		<< "    uint col = jobId % " << m_params->width << ";\n"
		<< "    float yCoord = (float(row + 0.5) / " << m_params->height << ".0) * 2.0 - 1.0;\n"
		<< "    float xCoord = (float(col + 0.5) / " << m_params->width << ".0) * 2.0 - 1.0;\n"
		<< "    gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_PointSize = 1.0;\n"
		<< "    gl_PrimitiveIndicesNV[gl_LocalInvocationID.x] = gl_LocalInvocationID.x;\n"
		<< "    pointColor[gl_LocalInvocationID.x] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

// Tests that generate no primitives of a given type.
enum class PrimitiveType { POINTS=0, LINES, TRIANGLES };

std::string primitiveTypeName (PrimitiveType primitiveType)
{
	std::string primitiveName;

	switch (primitiveType)
	{
	case PrimitiveType::POINTS:		primitiveName = "points";		break;
	case PrimitiveType::LINES:		primitiveName = "lines";		break;
	case PrimitiveType::TRIANGLES:	primitiveName = "triangles";	break;
	default: DE_ASSERT(false); break;
	}

	return primitiveName;
}

struct NoPrimitivesParams : public MiscTestParams
{
	NoPrimitivesParams (const tcu::Maybe<uint32_t>& taskCount_, uint32_t meshCount_, uint32_t width_, uint32_t height_, PrimitiveType primitiveType_)
		: MiscTestParams	(taskCount_, meshCount_, width_, height_)
		, primitiveType		(primitiveType_)
		{}

	PrimitiveType primitiveType;
};

class NoPrimitivesCase : public MeshShaderMiscCase
{
public:
					NoPrimitivesCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class NoPrimitivesInstance : public MeshShaderMiscInstance
{
public:
	NoPrimitivesInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

void NoPrimitivesInstance::generateReferenceLevel ()
{
	// No primitives: clear color.
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), m_referenceLevel);
}

TestInstance* NoPrimitivesCase::createInstance (Context& context) const
{
	return new NoPrimitivesInstance(context, m_params.get());
}

void NoPrimitivesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto params = dynamic_cast<NoPrimitivesParams*>(m_params.get());

	DE_ASSERT(params);
	DE_ASSERT(!params->needsTaskShader());

	const auto primitiveName = primitiveTypeName(params->primitiveType);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=32) in;\n"
		<< "layout (" << primitiveName << ") out;\n"
		<< "layout (max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 primitiveColor[];\n"
		<< "\n"
		<< "void main () {\n"
		<< "    gl_PrimitiveCountNV = 0u;\n"
		<< "}\n"
		;

	MeshShaderMiscCase::initPrograms(programCollection);
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

class NoPrimitivesExtraWritesCase : public NoPrimitivesCase
{
public:
					NoPrimitivesExtraWritesCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: NoPrimitivesCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;

	static constexpr uint32_t kLocalInvocations = 32u;
};

void NoPrimitivesExtraWritesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto params = dynamic_cast<NoPrimitivesParams*>(m_params.get());

	DE_ASSERT(params);
	DE_ASSERT(m_params->needsTaskShader());

	std::ostringstream taskData;
	taskData
		<< "taskNV TaskData {\n"
		<< "    uint localInvocations[" << kLocalInvocations << "];\n"
		<< "} td;\n"
		;
	const auto taskDataStr = taskData.str();

	std::ostringstream task;
	task
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
		<< "\n"
		<< "out " << taskDataStr
		<< "\n"
		<< "void main () {\n"
		<< "    gl_TaskCountNV = " << params->meshCount << ";\n"
		<< "    td.localInvocations[gl_LocalInvocationID.x] = gl_LocalInvocationID.x;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("task") << glu::TaskSource(task.str());

	const auto primitiveName = primitiveTypeName(params->primitiveType);

	// Otherwise the shader would be illegal.
	DE_ASSERT(kLocalInvocations > 2u);

	uint32_t maxPrimitives = 0u;
	switch (params->primitiveType)
	{
	case PrimitiveType::POINTS:		maxPrimitives = kLocalInvocations - 0u;	break;
	case PrimitiveType::LINES:		maxPrimitives = kLocalInvocations - 1u;	break;
	case PrimitiveType::TRIANGLES:	maxPrimitives = kLocalInvocations - 2u;	break;
	default: DE_ASSERT(false); break;
	}

	const std::string pointSizeDecl	= ((params->primitiveType == PrimitiveType::POINTS)
									? "        gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_PointSize = 1.0;\n"
									: "");

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
		<< "layout (" << primitiveName << ") out;\n"
		<< "layout (max_vertices=" << kLocalInvocations << ", max_primitives=" << maxPrimitives << ") out;\n"
		<< "\n"
		<< "in " << taskDataStr
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 primitiveColor[];\n"
		<< "\n"
		<< "shared uint sumOfIds;\n"
		<< "\n"
		<< "const float PI_2 = 1.57079632679489661923;\n"
		<< "const float RADIUS = 1.0f;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    sumOfIds = 0u;\n"
		<< "    barrier();\n"
		<< "    atomicAdd(sumOfIds, td.localInvocations[gl_LocalInvocationID.x]);\n"
		<< "    barrier();\n"
		<< "    // This should dynamically give 0\n"
		<< "    gl_PrimitiveCountNV = sumOfIds - (" << kLocalInvocations * (kLocalInvocations - 1u) / 2u << ");\n"
		<< "\n"
		<< "    // Emit points and primitives to the arrays in any case\n"
		<< "    if (gl_LocalInvocationID.x > 0u) {\n"
		<< "        float proportion = (float(gl_LocalInvocationID.x - 1u) + 0.5f) / float(" << kLocalInvocations << " - 1u);\n"
		<< "        float angle = PI_2 * proportion;\n"
		<< "        float xCoord = cos(angle) * RADIUS - 1.0;\n"
		<< "        float yCoord = sin(angle) * RADIUS - 1.0;\n"
		<< "        gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
		<< pointSizeDecl
		<< "    } else {\n"
		<< "        gl_MeshVerticesNV[gl_LocalInvocationID.x].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
		<< pointSizeDecl
		<< "    }\n"
		<< "    uint primitiveId = max(gl_LocalInvocationID.x, " << (maxPrimitives - 1u) << ");\n"
		<< "    primitiveColor[primitiveId] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		;

	if (params->primitiveType == PrimitiveType::POINTS)
	{
		mesh
			<< "    gl_PrimitiveIndicesNV[primitiveId] = primitiveId;\n"
			;
	}
	else if (params->primitiveType == PrimitiveType::LINES)
	{
		mesh
			<< "    gl_PrimitiveIndicesNV[primitiveId * 2u + 0u] = primitiveId + 0u;\n"
			<< "    gl_PrimitiveIndicesNV[primitiveId * 2u + 1u] = primitiveId + 1u;\n"
			;
	}
	else if (params->primitiveType == PrimitiveType::TRIANGLES)
	{
		mesh
			<< "    gl_PrimitiveIndicesNV[primitiveId * 3u + 0u] = 0u;\n"
			<< "    gl_PrimitiveIndicesNV[primitiveId * 3u + 1u] = primitiveId + 1u;\n"
			<< "    gl_PrimitiveIndicesNV[primitiveId * 3u + 2u] = primitiveId + 3u;\n"
			;
	}
	else
		DE_ASSERT(false);

	mesh
		<< "}\n"
		;

	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());

	MeshShaderMiscCase::initPrograms(programCollection);
}

// Case testing barrier().
class SimpleBarrierCase : public MeshShaderMiscCase
{
public:
					SimpleBarrierCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

	static constexpr uint32_t kLocalInvocations = 32u;
};

class SimpleBarrierInstance : public MeshShaderMiscInstance
{
public:
	SimpleBarrierInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* SimpleBarrierCase::createInstance (Context& context) const
{
	return new SimpleBarrierInstance(context, m_params.get());
}

void SimpleBarrierInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

void SimpleBarrierCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Generate frag shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	DE_ASSERT(m_params->meshCount == 1u);
	DE_ASSERT(m_params->width == 1u && m_params->height == 1u);

	std::ostringstream meshPrimData;
	meshPrimData
			<< "gl_PrimitiveCountNV = 1u;\n"
			<< "gl_MeshVerticesNV[0].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "gl_MeshVerticesNV[0].gl_PointSize = 1.0;\n"
			<< "primitiveColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
			<< "gl_PrimitiveIndicesNV[0] = 0;\n"
			;
	const std::string meshPrimStr	= meshPrimData.str();

	const std::string taskOK		= "gl_TaskCountNV = 1u;\n";
	const std::string taskFAIL		= "gl_TaskCountNV = 0u;\n";

	const std::string meshOK		= meshPrimStr;
	const std::string meshFAIL		= "gl_PrimitiveCountNV = 0u;\n";

	const std::string okStatement	= (m_params->needsTaskShader() ? taskOK : meshOK);
	const std::string failStatement	= (m_params->needsTaskShader() ? taskFAIL : meshFAIL);

	const std::string	sharedDecl = "shared uint counter;\n\n";
	std::ostringstream	verification;
	verification
		<< "counter = 0;\n"
		<< "barrier();\n"
		<< "atomicAdd(counter, 1u);\n"
		<< "barrier();\n"
		<< "if (gl_LocalInvocationID.x == 0u) {\n"
		<< "    if (counter == " << kLocalInvocations << ") {\n"
		<< "\n"
		<< okStatement
		<< "\n"
		<< "    } else {\n"
		<< "\n"
		<< failStatement
		<< "\n"
		<< "    }\n"
		<< "}\n"
		;

	// The mesh shader is very similar in both cases, so we use a template.
	std::ostringstream meshTemplateStr;
	meshTemplateStr
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=${LOCAL_SIZE}) in;\n"
		<< "layout (points) out;\n"
		<< "layout (max_vertices=1, max_primitives=1) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 primitiveColor[];\n"
		<< "\n"
		<< "${GLOBALS:opt}"
		<< "void main ()\n"
		<< "{\n"
		<< "${BODY}"
		<< "}\n"
		;
	const tcu::StringTemplate meshTemplate = meshTemplateStr.str();

	if (m_params->needsTaskShader())
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
			<< "\n"
			<< sharedDecl
			<< "void main ()\n"
			<< "{\n"
			<< verification.str()
			<< "}\n"
			;

		std::map<std::string, std::string> replacements;
		replacements["LOCAL_SIZE"]	= "1";
		replacements["BODY"]		= meshPrimStr;

		const auto meshStr = meshTemplate.specialize(replacements);

		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
		programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr);
	}
	else
	{
		std::map<std::string, std::string> replacements;
		replacements["LOCAL_SIZE"]	= std::to_string(kLocalInvocations);
		replacements["BODY"]		= verification.str();
		replacements["GLOBALS"]		= sharedDecl;

		const auto meshStr = meshTemplate.specialize(replacements);

		programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr);
	}
}

// Case testing memoryBarrierShared() and groupMemoryBarrier().
enum class MemoryBarrierType { SHARED = 0, GROUP };

struct MemoryBarrierParams : public MiscTestParams
{
	MemoryBarrierParams (const tcu::Maybe<uint32_t>& taskCount_, uint32_t meshCount_, uint32_t width_, uint32_t height_, MemoryBarrierType memBarrierType_)
		: MiscTestParams	(taskCount_, meshCount_, width_, height_)
		, memBarrierType	(memBarrierType_)
	{}

	MemoryBarrierType memBarrierType;

	std::string glslFunc () const
	{
		std::string funcName;

		switch (memBarrierType)
		{
		case MemoryBarrierType::SHARED:		funcName = "memoryBarrierShared";	break;
		case MemoryBarrierType::GROUP:		funcName = "groupMemoryBarrier";	break;
		default: DE_ASSERT(false); break;
		}

		return funcName;
	}

};

class MemoryBarrierCase : public MeshShaderMiscCase
{
public:
					MemoryBarrierCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

	static constexpr uint32_t kLocalInvocations = 2u;
};

class MemoryBarrierInstance : public MeshShaderMiscInstance
{
public:
	MemoryBarrierInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
	bool	verifyResult			(const tcu::ConstPixelBufferAccess& resultAccess) const override;

protected:
	// Allow two possible outcomes.
	std::unique_ptr<tcu::TextureLevel>	m_referenceLevel2;
};

TestInstance* MemoryBarrierCase::createInstance (Context& context) const
{
	return new MemoryBarrierInstance(context, m_params.get());
}

void MemoryBarrierInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), m_referenceLevel2);
}

bool MemoryBarrierInstance::verifyResult (const tcu::ConstPixelBufferAccess& resultAccess) const
{
	// Any of the two results is considered valid.
	// Clarify what we are checking in the logs; otherwise, they could be confusing.
	auto& log = m_context.getTestContext().getLog();
	const std::vector<tcu::TextureLevel*> levels = { m_referenceLevel.get(), m_referenceLevel2.get() };

	bool good = false;
	for (size_t i = 0; i < levels.size(); ++i)
	{
		log << tcu::TestLog::Message << "Comparing result with reference " << i << "..." << tcu::TestLog::EndMessage;
		const auto success = MeshShaderMiscInstance::verifyResult(resultAccess, *levels[i]);
		if (success)
		{
			log << tcu::TestLog::Message << "Match! The test has passed" << tcu::TestLog::EndMessage;
			good = true;
			break;
		}
	}

	return good;
}

void MemoryBarrierCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto params = dynamic_cast<MemoryBarrierParams*>(m_params.get());
	DE_ASSERT(params);

	// Generate frag shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	DE_ASSERT(params->meshCount == 1u);
	DE_ASSERT(params->width == 1u && params->height == 1u);

	const bool taskShader = params->needsTaskShader();

	const std::string	taskDataDecl	= "taskNV TaskData { float blue; } td;\n\n";
	const std::string	inTaskData		= "in " + taskDataDecl;
	const std::string	outTaskData		= "out " + taskDataDecl;
	const auto			barrierFunc		= params->glslFunc();

	std::ostringstream meshPrimData;
	meshPrimData
			<< "gl_PrimitiveCountNV = 1u;\n"
			<< "gl_MeshVerticesNV[0].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "gl_MeshVerticesNV[0].gl_PointSize = 1.0;\n"
			<< "primitiveColor[0] = vec4(0.0, 0.0, " << (taskShader ? "td.blue" : "float(iterations % 2u)") << ", 1.0);\n"
			<< "gl_PrimitiveIndicesNV[0] = 0;\n"
			;
	const std::string meshPrimStr	= meshPrimData.str();

	const std::string taskAction	= "gl_TaskCountNV = 1u;\ntd.blue = float(iterations % 2u);\n";
	const std::string meshAction	= meshPrimStr;
	const std::string action		= (taskShader ? taskAction : meshAction);

	const std::string	sharedDecl = "shared uint flags[2];\n\n";
	std::ostringstream	verification;
	verification
		<< "flags[gl_LocalInvocationID.x] = 0u;\n"
		<< "barrier();\n"
		<< "flags[gl_LocalInvocationID.x] = 1u;\n"
		<<  barrierFunc << "();\n"
		<< "uint otherInvocation = 1u - gl_LocalInvocationID.x;\n"
		<< "uint iterations = 0u;\n"
		<< "while (flags[otherInvocation] != 1u) {\n"
		<< "    iterations++;\n"
		<< "}\n"
		<< "if (gl_LocalInvocationID.x == 0u) {\n"
		<< "\n"
		<< action
		<< "\n"
		<< "}\n"
		;

	// The mesh shader is very similar in both cases, so we use a template.
	std::ostringstream meshTemplateStr;
	meshTemplateStr
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=${LOCAL_SIZE}) in;\n"
		<< "layout (points) out;\n"
		<< "layout (max_vertices=1, max_primitives=1) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 primitiveColor[];\n"
		<< "\n"
		<< "${GLOBALS}"
		<< "void main ()\n"
		<< "{\n"
		<< "${BODY}"
		<< "}\n"
		;
	const tcu::StringTemplate meshTemplate = meshTemplateStr.str();

	if (params->needsTaskShader())
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
			<< "\n"
			<< sharedDecl
			<< outTaskData
			<< "void main ()\n"
			<< "{\n"
			<< verification.str()
			<< "}\n"
			;

		std::map<std::string, std::string> replacements;
		replacements["LOCAL_SIZE"]	= "1";
		replacements["BODY"]		= meshPrimStr;
		replacements["GLOBALS"]		= inTaskData;

		const auto meshStr = meshTemplate.specialize(replacements);

		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
		programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr);
	}
	else
	{
		std::map<std::string, std::string> replacements;
		replacements["LOCAL_SIZE"]	= std::to_string(kLocalInvocations);
		replacements["BODY"]		= verification.str();
		replacements["GLOBALS"]		= sharedDecl;

		const auto meshStr = meshTemplate.specialize(replacements);

		programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr);
	}
}

class CustomAttributesCase : public MeshShaderMiscCase
{
public:
					CustomAttributesCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase(testCtx, name, description, std::move(params)) {}
	virtual			~CustomAttributesCase		(void) {}

	TestInstance*	createInstance				(Context& context) const override;
	void			checkSupport				(Context& context) const override;
	void			initPrograms				(vk::SourceCollections& programCollection) const override;
};

class CustomAttributesInstance : public MeshShaderMiscInstance
{
public:
						CustomAttributesInstance	(Context& context, const MiscTestParams* params)
							: MeshShaderMiscInstance(context, params) {}
	virtual				~CustomAttributesInstance	(void) {}

	void				generateReferenceLevel		() override;
	tcu::TestStatus		iterate						(void) override;
};

TestInstance* CustomAttributesCase::createInstance (Context& context) const
{
	return new CustomAttributesInstance(context, m_params.get());
}

void CustomAttributesCase::checkSupport (Context& context) const
{
	MeshShaderMiscCase::checkSupport(context);

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_CLIP_DISTANCE);
}

void CustomAttributesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout (location=0) in vec4 customAttribute1;\n"
		<< "layout (location=1) in flat float customAttribute2;\n"
		<< "layout (location=2) in flat int customAttribute3;\n"
		<< "\n"
		<< "layout (location=3) in perprimitiveNV flat uvec4 customAttribute4;\n"
		<< "layout (location=4) in perprimitiveNV float customAttribute5;\n"
		<< "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    bool goodPrimitiveID = (gl_PrimitiveID == 1000 || gl_PrimitiveID == 1001);\n"
		<< "    bool goodViewportIndex = (gl_ViewportIndex == 1);\n"
		<< "    bool goodCustom1 = (customAttribute1.x >= 0.25 && customAttribute1.x <= 0.5 &&\n"
		<< "                        customAttribute1.y >= 0.5  && customAttribute1.y <= 1.0 &&\n"
		<< "                        customAttribute1.z >= 10.0 && customAttribute1.z <= 20.0 &&\n"
		<< "                        customAttribute1.w == 3.0);\n"
		<< "    bool goodCustom2 = (customAttribute2 == 1.0 || customAttribute2 == 2.0);\n"
		<< "    bool goodCustom3 = (customAttribute3 == 3 || customAttribute3 == 4);\n"
		<< "    bool goodCustom4 = ((gl_PrimitiveID == 1000 && customAttribute4 == uvec4(100, 101, 102, 103)) ||\n"
		<< "                        (gl_PrimitiveID == 1001 && customAttribute4 == uvec4(200, 201, 202, 203)));\n"
		<< "    bool goodCustom5 = ((gl_PrimitiveID == 1000 && customAttribute5 == 6.0) ||\n"
		<< "                        (gl_PrimitiveID == 1001 && customAttribute5 == 7.0));\n"
		<< "    \n"
		<< "    if (goodPrimitiveID && goodViewportIndex && goodCustom1 && goodCustom2 && goodCustom3 && goodCustom4 && goodCustom5) {\n"
		<< "        outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "    } else {\n"
		<< "        outColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

	std::ostringstream pvdDataDeclStream;
	pvdDataDeclStream
		<< "    vec4 positions[4];\n"
		<< "    float pointSizes[4];\n"
		<< "    float clipDistances[4];\n"
		<< "    vec4 custom1[4];\n"
		<< "    float custom2[4];\n"
		<< "    int custom3[4];\n"
		;
	const auto pvdDataDecl = pvdDataDeclStream.str();

	std::ostringstream ppdDataDeclStream;
	ppdDataDeclStream
		<< "    int primitiveIds[2];\n"
		<< "    int viewportIndices[2];\n"
		<< "    uvec4 custom4[2];\n"
		<< "    float custom5[2];\n"
		;
	const auto ppdDataDecl = ppdDataDeclStream.str();

	std::ostringstream bindingsDeclStream;
	bindingsDeclStream
		<< "layout (set=0, binding=0, std430) buffer PerVertexData {\n"
		<< pvdDataDecl
		<< "} pvd;\n"
		<< "layout (set=0, binding=1) uniform PerPrimitiveData {\n"
		<< ppdDataDecl
		<< "} ppd;\n"
		<< "\n"
		;
	const auto bindingsDecl = bindingsDeclStream.str();

	std::ostringstream taskDataStream;
	taskDataStream
		<< "taskNV TaskData {\n"
		<< pvdDataDecl
		<< ppdDataDecl
		<< "} td;\n"
		<< "\n"
		;
	const auto taskDataDecl = taskDataStream.str();

	const auto taskShader = m_params->needsTaskShader();

	const auto meshPvdPrefix = (taskShader ? "td" : "pvd");
	const auto meshPpdPrefix = (taskShader ? "td" : "ppd");

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=1) in;\n"
		<< "layout (max_primitives=2, max_vertices=4) out;\n"
		<< "layout (triangles) out;\n"
		<< "\n"
		<< "out gl_MeshPerVertexNV {\n"
		<< "    vec4  gl_Position;\n"
		<< "    float gl_PointSize;\n"
		<< "    float gl_ClipDistance[1];\n"
		<< "} gl_MeshVerticesNV[];\n"
		<< "\n"
		<< "layout (location=0) out vec4 customAttribute1[];\n"
		<< "layout (location=1) out flat float customAttribute2[];\n"
		<< "layout (location=2) out int customAttribute3[];\n"
		<< "\n"
		<< "layout (location=3) out perprimitiveNV uvec4 customAttribute4[];\n"
		<< "layout (location=4) out perprimitiveNV float customAttribute5[];\n"
		<< "\n"
		<< "out perprimitiveNV gl_MeshPerPrimitiveNV {\n"
		<< "  int gl_PrimitiveID;\n"
		<< "  int gl_ViewportIndex;\n"
		<< "} gl_MeshPrimitivesNV[];\n"
		<< "\n"
		<< (taskShader ? "in " + taskDataDecl : bindingsDecl)
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 2u;\n"
		<< "\n"
		<< "    gl_MeshVerticesNV[0].gl_Position = " << meshPvdPrefix << ".positions[0]; //vec4(-1.0, -1.0, 0.0, 1.0)\n"
		<< "    gl_MeshVerticesNV[1].gl_Position = " << meshPvdPrefix << ".positions[1]; //vec4( 1.0, -1.0, 0.0, 1.0)\n"
		<< "    gl_MeshVerticesNV[2].gl_Position = " << meshPvdPrefix << ".positions[2]; //vec4(-1.0,  1.0, 0.0, 1.0)\n"
		<< "    gl_MeshVerticesNV[3].gl_Position = " << meshPvdPrefix << ".positions[3]; //vec4( 1.0,  1.0, 0.0, 1.0)\n"
		<< "\n"
		<< "    gl_MeshVerticesNV[0].gl_PointSize = " << meshPvdPrefix << ".pointSizes[0]; //1.0\n"
		<< "    gl_MeshVerticesNV[1].gl_PointSize = " << meshPvdPrefix << ".pointSizes[1]; //1.0\n"
		<< "    gl_MeshVerticesNV[2].gl_PointSize = " << meshPvdPrefix << ".pointSizes[2]; //1.0\n"
		<< "    gl_MeshVerticesNV[3].gl_PointSize = " << meshPvdPrefix << ".pointSizes[3]; //1.0\n"
		<< "\n"
		<< "    // Remove geometry on the right side.\n"
		<< "    gl_MeshVerticesNV[0].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[0]; // 1.0\n"
		<< "    gl_MeshVerticesNV[1].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[1]; //-1.0\n"
		<< "    gl_MeshVerticesNV[2].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[2]; // 1.0\n"
		<< "    gl_MeshVerticesNV[3].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[3]; //-1.0\n"
		<< "    \n"
		<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
		<< "    gl_PrimitiveIndicesNV[1] = 2;\n"
		<< "    gl_PrimitiveIndicesNV[2] = 1;\n"
		<< "\n"
		<< "    gl_PrimitiveIndicesNV[3] = 2;\n"
		<< "    gl_PrimitiveIndicesNV[4] = 3;\n"
		<< "    gl_PrimitiveIndicesNV[5] = 1;\n"
		<< "\n"
		<< "    gl_MeshPrimitivesNV[0].gl_PrimitiveID = " << meshPpdPrefix << ".primitiveIds[0]; //1000\n"
		<< "    gl_MeshPrimitivesNV[1].gl_PrimitiveID = " << meshPpdPrefix << ".primitiveIds[1]; //1001\n"
		<< "\n"
		<< "    gl_MeshPrimitivesNV[0].gl_ViewportIndex = " << meshPpdPrefix << ".viewportIndices[0]; //1\n"
		<< "    gl_MeshPrimitivesNV[1].gl_ViewportIndex = " << meshPpdPrefix << ".viewportIndices[1]; //1\n"
		<< "\n"
		<< "    // Custom per-vertex attributes\n"
		<< "    customAttribute1[0] = " << meshPvdPrefix << ".custom1[0]; //vec4(0.25, 0.5, 10.0, 3.0)\n"
		<< "    customAttribute1[1] = " << meshPvdPrefix << ".custom1[1]; //vec4(0.25, 1.0, 20.0, 3.0)\n"
		<< "    customAttribute1[2] = " << meshPvdPrefix << ".custom1[2]; //vec4( 0.5, 0.5, 20.0, 3.0)\n"
		<< "    customAttribute1[3] = " << meshPvdPrefix << ".custom1[3]; //vec4( 0.5, 1.0, 10.0, 3.0)\n"
		<< "\n"
		<< "    customAttribute2[0] = " << meshPvdPrefix << ".custom2[0]; //1.0f\n"
		<< "    customAttribute2[1] = " << meshPvdPrefix << ".custom2[1]; //1.0f\n"
		<< "    customAttribute2[2] = " << meshPvdPrefix << ".custom2[2]; //2.0f\n"
		<< "    customAttribute2[3] = " << meshPvdPrefix << ".custom2[3]; //2.0f\n"
		<< "\n"
		<< "    customAttribute3[0] = " << meshPvdPrefix << ".custom3[0]; //3\n"
		<< "    customAttribute3[1] = " << meshPvdPrefix << ".custom3[1]; //3\n"
		<< "    customAttribute3[2] = " << meshPvdPrefix << ".custom3[2]; //4\n"
		<< "    customAttribute3[3] = " << meshPvdPrefix << ".custom3[3]; //4\n"
		<< "\n"
		<< "    // Custom per-primitive attributes.\n"
		<< "    customAttribute4[0] = " << meshPpdPrefix << ".custom4[0]; //uvec4(100, 101, 102, 103)\n"
		<< "    customAttribute4[1] = " << meshPpdPrefix << ".custom4[1]; //uvec4(200, 201, 202, 203)\n"
		<< "\n"
		<< "    customAttribute5[0] = " << meshPpdPrefix << ".custom5[0]; //6.0\n"
		<< "    customAttribute5[1] = " << meshPpdPrefix << ".custom5[1]; //7.0\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());

	if (taskShader)
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "out " << taskDataDecl
			<< bindingsDecl
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = " << m_params->meshCount << ";\n"
			<< "\n"
			<< "    td.positions[0] = pvd.positions[0];\n"
			<< "    td.positions[1] = pvd.positions[1];\n"
			<< "    td.positions[2] = pvd.positions[2];\n"
			<< "    td.positions[3] = pvd.positions[3];\n"
			<< "\n"
			<< "    td.pointSizes[0] = pvd.pointSizes[0];\n"
			<< "    td.pointSizes[1] = pvd.pointSizes[1];\n"
			<< "    td.pointSizes[2] = pvd.pointSizes[2];\n"
			<< "    td.pointSizes[3] = pvd.pointSizes[3];\n"
			<< "\n"
			<< "    td.clipDistances[0] = pvd.clipDistances[0];\n"
			<< "    td.clipDistances[1] = pvd.clipDistances[1];\n"
			<< "    td.clipDistances[2] = pvd.clipDistances[2];\n"
			<< "    td.clipDistances[3] = pvd.clipDistances[3];\n"
			<< "\n"
			<< "    td.custom1[0] = pvd.custom1[0];\n"
			<< "    td.custom1[1] = pvd.custom1[1];\n"
			<< "    td.custom1[2] = pvd.custom1[2];\n"
			<< "    td.custom1[3] = pvd.custom1[3];\n"
			<< "\n"
			<< "    td.custom2[0] = pvd.custom2[0];\n"
			<< "    td.custom2[1] = pvd.custom2[1];\n"
			<< "    td.custom2[2] = pvd.custom2[2];\n"
			<< "    td.custom2[3] = pvd.custom2[3];\n"
			<< "\n"
			<< "    td.custom3[0] = pvd.custom3[0];\n"
			<< "    td.custom3[1] = pvd.custom3[1];\n"
			<< "    td.custom3[2] = pvd.custom3[2];\n"
			<< "    td.custom3[3] = pvd.custom3[3];\n"
			<< "\n"
			<< "    td.primitiveIds[0] = ppd.primitiveIds[0];\n"
			<< "    td.primitiveIds[1] = ppd.primitiveIds[1];\n"
			<< "\n"
			<< "    td.viewportIndices[0] = ppd.viewportIndices[0];\n"
			<< "    td.viewportIndices[1] = ppd.viewportIndices[1];\n"
			<< "\n"
			<< "    td.custom4[0] = ppd.custom4[0];\n"
			<< "    td.custom4[1] = ppd.custom4[1];\n"
			<< "\n"
			<< "    td.custom5[0] = ppd.custom5[0];\n"
			<< "    td.custom5[1] = ppd.custom5[1];\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}
}

void CustomAttributesInstance::generateReferenceLevel ()
{
	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);

	const auto halfWidth	= iWidth / 2;
	const auto halfHeight	= iHeight / 2;

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto access		= m_referenceLevel->getAccess();
	const auto clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	const auto blueColor	= tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);

	tcu::clear(access, clearColor);

	// Fill the top left quarter.
	for (int y = 0; y < halfWidth; ++y)
	for (int x = 0; x < halfHeight; ++x)
	{
		access.setPixel(blueColor, x, y);
	}
}

tcu::TestStatus CustomAttributesInstance::iterate ()
{
	struct PerVertexData
	{
		tcu::Vec4	positions[4];
		float		pointSizes[4];
		float		clipDistances[4];
		tcu::Vec4	custom1[4];
		float		custom2[4];
		int32_t		custom3[4];
	};

	struct PerPrimitiveData
	{
		// Note some of these are declared as vectors to match the std140 layout.
		tcu::IVec4	primitiveIds[2];
		tcu::IVec4	viewportIndices[2];
		tcu::UVec4	custom4[2];
		tcu::Vec4	custom5[2];
	};

	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();

	const auto		imageFormat	= getOutputFormat();
	const auto		tcuFormat	= mapVkFormat(imageFormat);
	const auto		imageExtent	= makeExtent3D(m_params->width, m_params->height, 1u);
	const auto		imageUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	const auto&		binaries	= m_context.getBinaryCollection();
	const auto		hasTask		= binaries.contains("task");
	const auto		bufStages	= (hasTask ? VK_SHADER_STAGE_TASK_BIT_NV : VK_SHADER_STAGE_MESH_BIT_NV);

	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		imageFormat,							//	VkFormat				format;
		imageExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		imageUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	// Create color image and view.
	ImageWithMemory	colorImage	(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto		colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto		colorView	= makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

	// Create a memory buffer for verification.
	const auto			verificationBufferSize	= static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
	const auto			verificationBufferUsage	= (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const auto			verificationBufferInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

	BufferWithMemory	verificationBuffer		(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc	= verificationBuffer.getAllocation();
	void*				verificationBufferData	= verificationBufferAlloc.getHostPtr();

	// This needs to match what the fragment shader will expect.
	const PerVertexData perVertexData =
	{
		//	tcu::Vec4	positions[4];
		{
			tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
			tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f),
			tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f),
			tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f),
		},
		//	float		pointSizes[4];
		{ 1.0f, 1.0f, 1.0f, 1.0f, },
		//	float		clipDistances[4];
		{
			1.0f,
			-1.0f,
			1.0f,
			-1.0f,
		},
		//	tcu::Vec4	custom1[4];
		{
			tcu::Vec4(0.25, 0.5, 10.0, 3.0),
			tcu::Vec4(0.25, 1.0, 20.0, 3.0),
			tcu::Vec4( 0.5, 0.5, 20.0, 3.0),
			tcu::Vec4( 0.5, 1.0, 10.0, 3.0),
		},
		//	float		custom2[4];
		{ 1.0f, 1.0f, 2.0f, 2.0f, },
		//	int32_t		custom3[4];
		{ 3, 3, 4, 4 },
	};

	// This needs to match what the fragment shader will expect. Reminder: some of these are declared as gvec4 to match the std140
	// layout, but only the first component is actually used.
	const PerPrimitiveData perPrimitiveData =
	{
		//	int			primitiveIds[2];
		{
			tcu::IVec4(1000, 0, 0, 0),
			tcu::IVec4(1001, 0, 0, 0),
		},
		//	int			viewportIndices[2];
		{
			tcu::IVec4(1, 0, 0, 0),
			tcu::IVec4(1, 0, 0, 0),
		},
		//	uvec4		custom4[2];
		{
			tcu::UVec4(100u, 101u, 102u, 103u),
			tcu::UVec4(200u, 201u, 202u, 203u),
		},
		//	float		custom5[2];
		{
			tcu::Vec4(6.0f, 0.0f, 0.0f, 0.0f),
			tcu::Vec4(7.0f, 0.0f, 0.0f, 0.0f),
		},
	};

	// Create and fill buffers with this data.
	const auto			pvdSize		= static_cast<VkDeviceSize>(sizeof(perVertexData));
	const auto			pvdInfo		= makeBufferCreateInfo(pvdSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	pvdData		(vkd, device, alloc, pvdInfo, MemoryRequirement::HostVisible);
	auto&				pvdAlloc	= pvdData.getAllocation();
	void*				pvdPtr		= pvdAlloc.getHostPtr();

	const auto			ppdSize		= static_cast<VkDeviceSize>(sizeof(perPrimitiveData));
	const auto			ppdInfo		= makeBufferCreateInfo(ppdSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	BufferWithMemory	ppdData		(vkd, device, alloc, ppdInfo, MemoryRequirement::HostVisible);
	auto&				ppdAlloc	= ppdData.getAllocation();
	void*				ppdPtr		= ppdAlloc.getHostPtr();

	deMemcpy(pvdPtr, &perVertexData, sizeof(perVertexData));
	deMemcpy(ppdPtr, &perPrimitiveData, sizeof(perPrimitiveData));

	flushAlloc(vkd, device, pvdAlloc);
	flushAlloc(vkd, device, ppdAlloc);

	// Descriptor set layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufStages);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufStages);
	const auto setLayout = setLayoutBuilder.build(vkd, device);

	// Create and update descriptor set.
	DescriptorPoolBuilder descriptorPoolBuilder;
	descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	const auto descriptorPool	= descriptorPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	DescriptorSetUpdateBuilder updateBuilder;
	const auto storageBufferInfo = makeDescriptorBufferInfo(pvdData.get(), 0ull, pvdSize);
	const auto uniformBufferInfo = makeDescriptorBufferInfo(ppdData.get(), 0ull, ppdSize);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferInfo);
	updateBuilder.update(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Shader modules.
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));

	Move<VkShaderModule> taskShader;
	if (hasTask)
		taskShader = createShaderModule(vkd, device, binaries.get("task"));

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, imageFormat);

	// Framebuffer.
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

	// Viewport and scissor.
	const auto						topHalf		= makeViewport(imageExtent.width, imageExtent.height / 2u);
	const std::vector<VkViewport>	viewports	{ makeViewport(imageExtent), topHalf };
	const std::vector<VkRect2D>		scissors	(2u, makeRect2D(imageExtent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskShader.get(), meshShader.get(), fragShader.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Run pipeline.
	const tcu::Vec4 clearColor (0.0f, 0.0f, 0.0f, 0.0f);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params->drawCount(), 0u);
	endRenderPass(vkd, cmdBuffer);

	// Copy color buffer to verification buffer.
	const auto colorAccess		= (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	const auto transferRead		= VK_ACCESS_TRANSFER_READ_BIT;
	const auto transferWrite	= VK_ACCESS_TRANSFER_WRITE_BIT;
	const auto hostRead			= VK_ACCESS_HOST_READ_BIT;

	const auto preCopyBarrier	= makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
	const auto postCopyBarrier	= makeMemoryBarrier(transferWrite, hostRead);
	const auto copyRegion		= makeBufferImageCopy(imageExtent, colorSRL);

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &postCopyBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Generate reference image and compare results.
	const tcu::IVec3					iExtent				(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
	const tcu::ConstPixelBufferAccess	verificationAccess	(tcuFormat, iExtent, verificationBufferData);

	generateReferenceLevel();
	invalidateAlloc(vkd, device, verificationBufferAlloc);
	if (!verifyResult(verificationAccess))
		TCU_FAIL("Result does not match reference; check log for details");

	return tcu::TestStatus::pass("Pass");
}

// Tests that use push constants in the new stages.
class PushConstantCase : public MeshShaderMiscCase
{
public:
					PushConstantCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class PushConstantInstance : public MeshShaderMiscInstance
{
public:
	PushConstantInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void			generateReferenceLevel	() override;
	tcu::TestStatus	iterate					() override;
};

TestInstance* PushConstantCase::createInstance (Context& context) const
{
	return new PushConstantInstance(context, m_params.get());
}

void PushConstantInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

void PushConstantCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto useTaskShader	= m_params->needsTaskShader();
	const auto pcNumFloats		= (useTaskShader ? 2u : 4u);

	std::ostringstream pushConstantStream;
	pushConstantStream
		<< "layout (push_constant, std430) uniform PushConstantBlock {\n"
		<< "    layout (offset=${PCOFFSET}) float values[" << pcNumFloats << "];\n"
		<< "} pc;\n"
		<< "\n"
		;
	const tcu::StringTemplate pushConstantsTemplate (pushConstantStream.str());
	using TemplateMap = std::map<std::string, std::string>;

	std::ostringstream taskDataStream;
	taskDataStream
		<< "taskNV TaskData {\n"
		<< "    float values[2];\n"
		<< "} td;\n"
		<< "\n"
		;
	const auto taskDataDecl = taskDataStream.str();

	if (useTaskShader)
	{
		TemplateMap taskMap;
		taskMap["PCOFFSET"] = std::to_string(2u * sizeof(float));

		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=1) in;\n"
			<< "\n"
			<< "out " << taskDataDecl
			<< pushConstantsTemplate.specialize(taskMap)
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = " << m_params->meshCount << ";\n"
			<< "\n"
			<< "    td.values[0] = pc.values[0];\n"
			<< "    td.values[1] = pc.values[1];\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	{
		const std::string blue	= (useTaskShader ? "td.values[0] + pc.values[0]" : "pc.values[0] + pc.values[2]");
		const std::string alpha	= (useTaskShader ? "td.values[1] + pc.values[1]" : "pc.values[1] + pc.values[3]");

		TemplateMap meshMap;
		meshMap["PCOFFSET"] = "0";

		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=1) in;\n"
			<< "layout(triangles) out;\n"
			<< "layout(max_vertices=3, max_primitives=1) out;\n"
			<< "\n"
			<< "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
			<< "\n"
			<< pushConstantsTemplate.specialize(meshMap)
			<< (useTaskShader ? "in " + taskDataDecl : "")
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 1;\n"
			<< "\n"
			<< "    gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
			<< "\n"
			<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
			<< "    gl_PrimitiveIndicesNV[1] = 1;\n"
			<< "    gl_PrimitiveIndicesNV[2] = 2;\n"
			<< "\n"
			<< "    triangleColor[0] = vec4(0.0, 0.0, " << blue << ", " << alpha << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Add default fragment shader.
	MeshShaderMiscCase::initPrograms(programCollection);
}

tcu::TestStatus PushConstantInstance::iterate ()
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();

	const auto		imageFormat	= getOutputFormat();
	const auto		tcuFormat	= mapVkFormat(imageFormat);
	const auto		imageExtent	= makeExtent3D(m_params->width, m_params->height, 1u);
	const auto		imageUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	const auto&		binaries	= m_context.getBinaryCollection();
	const auto		hasTask		= binaries.contains("task");

	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		imageFormat,							//	VkFormat				format;
		imageExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		imageUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	// Create color image and view.
	ImageWithMemory	colorImage	(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto		colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto		colorView	= makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

	// Create a memory buffer for verification.
	const auto			verificationBufferSize	= static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
	const auto			verificationBufferUsage	= (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const auto			verificationBufferInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

	BufferWithMemory	verificationBuffer		(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc	= verificationBuffer.getAllocation();
	void*				verificationBufferData	= verificationBufferAlloc.getHostPtr();

	// Push constant ranges.
	std::vector<float> pcData { 0.25f, 0.25f, 0.75f, 0.75f };
	const auto pcSize		= static_cast<uint32_t>(de::dataSize(pcData));
	const auto pcHalfSize	= pcSize / 2u;

	std::vector<VkPushConstantRange> pcRanges;
	if (hasTask)
	{
		pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_MESH_BIT_NV, 0u, pcHalfSize));
		pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_TASK_BIT_NV, pcHalfSize, pcHalfSize));
	}
	else
	{
		pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_MESH_BIT_NV, 0u, pcSize));
	}

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, 0u, nullptr, static_cast<uint32_t>(pcRanges.size()), de::dataOrNull(pcRanges));

	// Shader modules.
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));

	Move<VkShaderModule> taskShader;
	if (hasTask)
		taskShader = createShaderModule(vkd, device, binaries.get("task"));

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, imageFormat);

	// Framebuffer.
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

	// Viewport and scissor.
	const std::vector<VkViewport>	viewports	(1u, makeViewport(imageExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(imageExtent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskShader.get(), meshShader.get(), fragShader.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Run pipeline.
	const tcu::Vec4 clearColor (0.0f, 0.0f, 0.0f, 0.0f);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	for (const auto& range : pcRanges)
		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), range.stageFlags, range.offset, range.size, reinterpret_cast<const char*>(pcData.data()) + range.offset);
	vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params->drawCount(), 0u);
	endRenderPass(vkd, cmdBuffer);

	// Copy color buffer to verification buffer.
	const auto colorAccess		= (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	const auto transferRead		= VK_ACCESS_TRANSFER_READ_BIT;
	const auto transferWrite	= VK_ACCESS_TRANSFER_WRITE_BIT;
	const auto hostRead			= VK_ACCESS_HOST_READ_BIT;

	const auto preCopyBarrier	= makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
	const auto postCopyBarrier	= makeMemoryBarrier(transferWrite, hostRead);
	const auto copyRegion		= makeBufferImageCopy(imageExtent, colorSRL);

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &postCopyBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Generate reference image and compare results.
	const tcu::IVec3					iExtent				(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
	const tcu::ConstPixelBufferAccess	verificationAccess	(tcuFormat, iExtent, verificationBufferData);

	generateReferenceLevel();
	invalidateAlloc(vkd, device, verificationBufferAlloc);
	if (!verifyResult(verificationAccess))
		TCU_FAIL("Result does not match reference; check log for details");

	return tcu::TestStatus::pass("Pass");
}

// Use large work group size, large number of vertices and large number of primitives.
struct MaximizeThreadsParams : public MiscTestParams
{
	MaximizeThreadsParams	(const tcu::Maybe<uint32_t>& taskCount_, uint32_t meshCount_, uint32_t width_, uint32_t height_,
							 uint32_t localSize_, uint32_t numVertices_, uint32_t numPrimitives_)
		: MiscTestParams	(taskCount_, meshCount_, width_, height_)
		, localSize			(localSize_)
		, numVertices		(numVertices_)
		, numPrimitives		(numPrimitives_)
		{}

	uint32_t localSize;
	uint32_t numVertices;
	uint32_t numPrimitives;

	void checkSupport (Context& context) const
	{
		const auto& properties = context.getMeshShaderProperties();

		if (localSize > properties.maxMeshWorkGroupSize[0])
			TCU_THROW(NotSupportedError, "Required local size not supported");

		if (numVertices > properties.maxMeshOutputVertices)
			TCU_THROW(NotSupportedError, "Required number of output vertices not supported");

		if (numPrimitives > properties.maxMeshOutputPrimitives)
			TCU_THROW(NotSupportedError, "Required number of output primitives not supported");
	}
};

// Focus on the number of primitives.
class MaximizePrimitivesCase : public MeshShaderMiscCase
{
public:
					MaximizePrimitivesCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{
						const auto mtParams = dynamic_cast<MaximizeThreadsParams*>(m_params.get());
						DE_ASSERT(mtParams);
						DE_UNREF(mtParams); // For release builds.
					}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	void			checkSupport			(Context& context) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class MaximizePrimitivesInstance : public MeshShaderMiscInstance
{
public:
	MaximizePrimitivesInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* MaximizePrimitivesCase::createInstance (Context& context) const
{
	return new MaximizePrimitivesInstance (context, m_params.get());
}

void MaximizePrimitivesCase::checkSupport (Context& context) const
{
	MeshShaderMiscCase::checkSupport(context);

	const auto params = dynamic_cast<MaximizeThreadsParams*>(m_params.get());
	params->checkSupport(context);
}

void MaximizePrimitivesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto params = dynamic_cast<MaximizeThreadsParams*>(m_params.get());

	DE_ASSERT(!params->needsTaskShader());
	MeshShaderMiscCase::initPrograms(programCollection);

	// Idea behind the test: generate 128 vertices, 1 per each pixel in a 128x1 image. Then, use each vertex to generate two points,
	// adding the colors of each point using color blending to make sure every point is properly generated.

	DE_ASSERT(params->numPrimitives == params->numVertices * 2u);
	DE_ASSERT(params->numVertices == params->width);

	const auto verticesPerInvocation	= params->numVertices / params->localSize;
	const auto primitivesPerVertex		= params->numPrimitives / params->numVertices;

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=" << params->localSize << ") in;\n"
		<< "layout(points) out;\n"
		<< "layout(max_vertices=" << params->numVertices << ", max_primitives=" << params->numPrimitives << ") out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 pointColor[];\n"
		<< "\n"
		<< "const uint verticesPerInvocation = " << verticesPerInvocation << ";\n"
		<< "const uint primitivesPerVertex   = " << primitivesPerVertex << ";\n"
		<< "\n"
		<< "vec4 colors[primitivesPerVertex] = vec4[](\n"
		<< "    vec4(0.0, 0.0, 1.0, 1.0),\n"
		<< "    vec4(1.0, 0.0, 0.0, 1.0)\n"
		<< ");\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = " << params->numPrimitives << ";\n"
		<< "    const uint firstVertex = gl_LocalInvocationIndex * verticesPerInvocation;\n"
		<< "    for (uint i = 0u; i < verticesPerInvocation; ++i)\n"
		<< "    {\n"
		<< "        const uint vertexNumber = firstVertex + i;\n"
		<< "        const float xCoord = ((float(vertexNumber) + 0.5) / " << params->width << ".0) * 2.0 - 1.0;\n"
		<< "        const float yCoord = 0.0;\n"
		<< "        gl_MeshVerticesNV[vertexNumber].gl_Position = vec4(xCoord, yCoord, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesNV[vertexNumber].gl_PointSize = 1.0f;\n"
		<< "        for (uint j = 0u; j < primitivesPerVertex; ++j)\n"
		<< "        {\n"
		<< "            const uint primitiveNumber = vertexNumber * primitivesPerVertex + j;\n"
		<< "            gl_PrimitiveIndicesNV[primitiveNumber] = vertexNumber;\n"
		<< "            pointColor[primitiveNumber] = colors[j];\n"
		<< "        }\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaximizePrimitivesInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

// Focus on the number of vertices.
class MaximizeVerticesCase : public MeshShaderMiscCase
{
public:
					MaximizeVerticesCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{
						const auto mtParams = dynamic_cast<MaximizeThreadsParams*>(m_params.get());
						DE_ASSERT(mtParams);
						DE_UNREF(mtParams); // For release builds.
					}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	void			checkSupport			(Context& context) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class MaximizeVerticesInstance : public MeshShaderMiscInstance
{
public:
	MaximizeVerticesInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* MaximizeVerticesCase::createInstance (Context& context) const
{
	return new MaximizeVerticesInstance (context, m_params.get());
}

void MaximizeVerticesCase::checkSupport (Context& context) const
{
	MeshShaderMiscCase::checkSupport(context);

	const auto params = dynamic_cast<MaximizeThreadsParams*>(m_params.get());
	params->checkSupport(context);
}

void MaximizeVerticesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto params = dynamic_cast<MaximizeThreadsParams*>(m_params.get());

	DE_ASSERT(!params->needsTaskShader());
	MeshShaderMiscCase::initPrograms(programCollection);

	// Idea behind the test: cover a framebuffer using a triangle quad per pixel (4 vertices, 2 triangles).
	DE_ASSERT(params->numVertices == params->numPrimitives * 2u);
	DE_ASSERT(params->numPrimitives == params->width * 2u);

	const auto pixelsPerInvocation		= params->width / params->localSize;
	const auto verticesPerPixel			= 4u;
	const auto primitivesPerPixel		= 2u;
	const auto verticesPerInvocation	= pixelsPerInvocation * verticesPerPixel;
	const auto primitivesPerInvocation	= pixelsPerInvocation * primitivesPerPixel;

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=" << params->localSize << ") in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=" << params->numVertices << ", max_primitives=" << params->numPrimitives << ") out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
		<< "\n"
		<< "const uint pixelsPerInvocation     = " << pixelsPerInvocation << ";\n"
		<< "const uint verticesPerInvocation   = " << verticesPerInvocation << ";\n"
		<< "const uint primitivesPerInvocation = " << primitivesPerInvocation << ";\n"
		<< "const uint indicesPerInvocation    = primitivesPerInvocation * 3u;\n"
		<< "const uint verticesPerPixel        = " << verticesPerPixel << ";\n"
		<< "const uint primitivesPerPixel      = " << primitivesPerPixel << ";\n"
		<< "const uint indicesPerPixel         = primitivesPerPixel * 3u;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = " << params->numPrimitives << ";\n"
		<< "\n"
		<< "    const uint firstPixel    = gl_LocalInvocationIndex * pixelsPerInvocation;\n"
		<< "    const float pixelWidth   = 2.0 / float(" << params->width << ");\n"
		<< "    const float quarterWidth = pixelWidth / 4.0;\n"
		<< "\n"
		<< "    for (uint pixelIdx = 0u; pixelIdx < pixelsPerInvocation; ++pixelIdx)\n"
		<< "    {\n"
		<< "        const uint pixelId      = firstPixel + pixelIdx;\n"
		<< "        const float pixelCenter = (float(pixelId) + 0.5) / float(" << params->width << ") * 2.0 - 1.0;\n"
		<< "        const float left        = pixelCenter - quarterWidth;\n"
		<< "        const float right       = pixelCenter + quarterWidth;\n"
		<< "\n"
		<< "        const uint firstVertex = gl_LocalInvocationIndex * verticesPerInvocation + pixelIdx * verticesPerPixel;\n"
		<< "        gl_MeshVerticesNV[firstVertex + 0].gl_Position = vec4(left,  -1.0, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesNV[firstVertex + 1].gl_Position = vec4(left,   1.0, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesNV[firstVertex + 2].gl_Position = vec4(right, -1.0, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesNV[firstVertex + 3].gl_Position = vec4(right,  1.0, 0.0f, 1.0f);\n"
		<< "\n"
		<< "        const uint firstPrimitive = gl_LocalInvocationIndex * primitivesPerInvocation + pixelIdx * primitivesPerPixel;\n"
		<< "        triangleColor[firstPrimitive + 0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "        triangleColor[firstPrimitive + 1] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "\n"
		<< "        const uint firstIndex = gl_LocalInvocationIndex * indicesPerInvocation + pixelIdx * indicesPerPixel;\n"
		<< "        gl_PrimitiveIndicesNV[firstIndex + 0] = firstVertex + 0;\n"
		<< "        gl_PrimitiveIndicesNV[firstIndex + 1] = firstVertex + 1;\n"
		<< "        gl_PrimitiveIndicesNV[firstIndex + 2] = firstVertex + 2;\n"
		<< "        gl_PrimitiveIndicesNV[firstIndex + 3] = firstVertex + 1;\n"
		<< "        gl_PrimitiveIndicesNV[firstIndex + 4] = firstVertex + 3;\n"
		<< "        gl_PrimitiveIndicesNV[firstIndex + 5] = firstVertex + 2;\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaximizeVerticesInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

// Focus on the number of invocations.
class MaximizeInvocationsCase : public MeshShaderMiscCase
{
public:
					MaximizeInvocationsCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{
						const auto mtParams = dynamic_cast<MaximizeThreadsParams*>(m_params.get());
						DE_ASSERT(mtParams);
						DE_UNREF(mtParams); // For release builds.
					}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	void			checkSupport			(Context& context) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class MaximizeInvocationsInstance : public MeshShaderMiscInstance
{
public:
	MaximizeInvocationsInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* MaximizeInvocationsCase::createInstance (Context& context) const
{
	return new MaximizeInvocationsInstance (context, m_params.get());
}

void MaximizeInvocationsCase::checkSupport (Context& context) const
{
	MeshShaderMiscCase::checkSupport(context);

	const auto params = dynamic_cast<MaximizeThreadsParams*>(m_params.get());
	params->checkSupport(context);
}

void MaximizeInvocationsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto params = dynamic_cast<MaximizeThreadsParams*>(m_params.get());

	DE_ASSERT(!params->needsTaskShader());
	MeshShaderMiscCase::initPrograms(programCollection);

	// Idea behind the test: use two invocations to generate one point per framebuffer pixel.
	DE_ASSERT(params->localSize == params->width * 2u);
	DE_ASSERT(params->localSize == params->numPrimitives * 2u);
	DE_ASSERT(params->localSize == params->numVertices * 2u);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=" << params->localSize << ") in;\n"
		<< "layout(points) out;\n"
		<< "layout(max_vertices=" << params->numVertices << ", max_primitives=" << params->numPrimitives << ") out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 pointColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = " << params->numPrimitives << ";\n"
		<< "    const uint pixelId = gl_LocalInvocationIndex / 2u;\n"
		<< "    if (gl_LocalInvocationIndex % 2u == 0u)\n"
		<< "    {\n"
		<< "        const float xCoord = (float(pixelId) + 0.5) / float(" << params->width << ") * 2.0 - 1.0;\n"
		<< "        gl_MeshVerticesNV[pixelId].gl_Position = vec4(xCoord, 0.0, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesNV[pixelId].gl_PointSize = 1.0f;\n"
		<< "    }\n"
		<< "    else\n"
		<< "    {\n"
		<< "        gl_PrimitiveIndicesNV[pixelId] = pixelId;\n"
		<< "        pointColor[pixelId] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
}

void MaximizeInvocationsInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

// Tests checking varied interfaces between task, mesh and frag.

enum class Owner
{
	VERTEX = 0,
	PRIMITIVE,
};

enum class DataType
{
	INTEGER = 0,
	FLOAT,
};

// Note: 8-bit variables not available for Input/Output.
enum class BitWidth
{
	B64 = 64,
	B32 = 32,
	B16 = 16,
};

enum class DataDim
{
	SCALAR = 1,
	VEC2   = 2,
	VEC3   = 3,
	VEC4   = 4,
};

enum class Interpolation
{
	NORMAL = 0,
	FLAT,
};

enum class Direction
{
	IN = 0,
	OUT,
};

// Interface variable.
struct IfaceVar
{
	static constexpr uint32_t kNumVertices		= 4u;
	static constexpr uint32_t kNumPrimitives	= 2u;
	static constexpr uint32_t kVarsPerType		= 2u;

	IfaceVar (Owner owner_, DataType dataType_, BitWidth bitWidth_, DataDim dataDim_, Interpolation interpolation_, uint32_t index_)
		: owner			(owner_)
		, dataType		(dataType_)
		, bitWidth		(bitWidth_)
		, dataDim		(dataDim_)
		, interpolation	(interpolation_)
		, index			(index_)
		{
			DE_ASSERT(!(dataType == DataType::INTEGER && interpolation == Interpolation::NORMAL));
			DE_ASSERT(!(owner == Owner::PRIMITIVE && interpolation == Interpolation::NORMAL));
			DE_ASSERT(!(dataType == DataType::FLOAT && bitWidth == BitWidth::B64 && interpolation == Interpolation::NORMAL));
			DE_ASSERT(index < kVarsPerType);
		}

	// This constructor needs to be defined for the code to compile, but it should never be actually called.
	// To make sure it's not used, the index is defined to be very large, which should trigger the assertion in getName() below.
	IfaceVar ()
		: owner			(Owner::VERTEX)
		, dataType		(DataType::FLOAT)
		, bitWidth		(BitWidth::B32)
		, dataDim		(DataDim::VEC4)
		, interpolation	(Interpolation::NORMAL)
		, index			(std::numeric_limits<uint32_t>::max())
		{
		}

	Owner			owner;
	DataType		dataType;
	BitWidth		bitWidth;
	DataDim			dataDim;
	Interpolation	interpolation;
	uint32_t		index; // In case there are several variables matching this type.

	// The variable name will be unique and depend on its type.
	std::string getName () const
	{
		DE_ASSERT(index < kVarsPerType);

		std::ostringstream name;
		name
			<< ((owner == Owner::VERTEX) ? "vert" : "prim") << "_"
			<< ((dataType == DataType::INTEGER) ? "i" : "f") << static_cast<int>(bitWidth)
			<< "d" << static_cast<int>(dataDim) << "_"
			<< ((interpolation == Interpolation::NORMAL) ? "inter" : "flat") << "_"
			<< index
			;
		return name.str();
	}

	// Get location size according to the type.
	uint32_t getLocationSize () const
	{
		return ((bitWidth == BitWidth::B64 && dataDim >= DataDim::VEC3) ? 2u : 1u);
	}

	// Get the variable type in GLSL.
	std::string getGLSLType () const
	{
		const auto widthStr		= std::to_string(static_cast<int>(bitWidth));
		const auto dimStr		= std::to_string(static_cast<int>(dataDim));
		const auto shortTypeStr	= ((dataType == DataType::INTEGER) ? "i" : "f");
		const auto typeStr		= ((dataType == DataType::INTEGER) ? "int" : "float");

		if (dataDim == DataDim::SCALAR)
			return typeStr + widthStr + "_t";				// e.g. int32_t or float16_t
		return shortTypeStr + widthStr + "vec" + dimStr;	// e.g. i16vec2 or f64vec4.
	}

	// Get a simple declaration of type and name. This can be reused for several things.
	std::string getTypeAndName () const
	{
		return getGLSLType() + " " + getName();
	}

	std::string getTypeAndNameDecl (bool arrayDecl = false) const
	{
		std::ostringstream decl;
		decl << "    " << getTypeAndName();
		if (arrayDecl)
			decl << "[" << ((owner == Owner::PRIMITIVE) ? IfaceVar::kNumPrimitives : IfaceVar::kNumVertices) << "]";
		decl << ";\n";
		return decl.str();
	}

	// Variable declaration statement given its location and direction.
	std::string getLocationDecl (size_t location, Direction direction) const
	{
		std::ostringstream decl;
		decl
			<< "layout (location=" << location << ") "
			<< ((direction == Direction::IN) ? "in" : "out") << " "
			<< ((owner == Owner::PRIMITIVE) ? "perprimitiveNV " : "")
			<< ((interpolation == Interpolation::FLAT) ? "flat " : "")
			<< getTypeAndName()
			<< ((direction == Direction::OUT) ? "[]" : "") << ";\n"
			;
		return decl.str();
	}

	// Get the name of the source data for this variable. Tests will use a storage buffer for the per-vertex data and a uniform
	// buffer for the per-primitive data. The names in those will match.
	std::string getDataSourceName () const
	{
		// per-primitive data or per-vertex data buffers.
		return ((owner == Owner::PRIMITIVE) ? "ppd" : "pvd") + ("." + getName());
	}

	// Get the boolean check variable name (see below).
	std::string getCheckName () const
	{
		return "good_" + getName();
	}

	// Get the check statement that would be used in the fragment shader.
	std::string getCheckStatement () const
	{
		std::ostringstream	check;
		const auto			sourceName	= getDataSourceName();
		const auto			glslType	= getGLSLType();
		const auto			name		= getName();

		check << "    bool " << getCheckName() << " = ";
		if (owner == Owner::VERTEX)
		{
			// There will be 4 values in the buffers.
			std::ostringstream maxElem;
			std::ostringstream minElem;

			maxElem << glslType << "(max(max(max(" << sourceName << "[0], " << sourceName << "[1]), " << sourceName  << "[2]), " << sourceName << "[3]))";
			minElem << glslType << "(min(min(min(" << sourceName << "[0], " << sourceName << "[1]), " << sourceName  << "[2]), " << sourceName << "[3]))";

			if (dataDim == DataDim::SCALAR)
			{
				check << "(" << name << " <= " << maxElem.str() << ") && (" << name << " >= " << minElem.str() << ")";
			}
			else
			{
				check << "all(lessThanEqual(" << name << ", " << maxElem.str() << ")) && "
				      << "all(greaterThanEqual(" << name << ", " << minElem.str() << "))";
			}
		}
		else if (owner == Owner::PRIMITIVE)
		{
			// There will be 2 values in the buffers.
			check << "((gl_PrimitiveID == 0 || gl_PrimitiveID == 1) && ("
			      << "(gl_PrimitiveID == 0 && " << name << " == " << sourceName << "[0]) || "
				  << "(gl_PrimitiveID == 1 && " << name << " == " << sourceName << "[1])))";
		}
		check << ";\n";

		return check.str();
	}

	// Get an assignment statement for an out variable.
	std::string getAssignmentStatement (size_t arrayIndex, const std::string& leftPrefix, const std::string& rightPrefix) const
	{
		const auto			name	= getName();
		const auto			typeStr	= getGLSLType();
		std::ostringstream	stmt;

		stmt << "    " << leftPrefix << (leftPrefix.empty() ? "" : ".") << name << "[" << arrayIndex << "] = " << typeStr << "(" << rightPrefix << (rightPrefix.empty() ? "" : ".") << name << "[" << arrayIndex << "]);\n";
		return stmt.str();
	}

	// Get the corresponding array size based on the owner (vertex or primitive)
	uint32_t getArraySize () const
	{
		return ((owner == Owner::PRIMITIVE) ? IfaceVar::kNumPrimitives : IfaceVar::kNumVertices);
	}

};

using IfaceVarVec		= std::vector<IfaceVar>;
using IfaceVarVecPtr	= std::unique_ptr<IfaceVarVec>;

struct InterfaceVariableParams : public MiscTestParams
{
	InterfaceVariableParams (const tcu::Maybe<uint32_t>& taskCount_, uint32_t meshCount_, uint32_t width_, uint32_t height_,
							 bool useInt64_, bool useFloat64_, bool useInt16_, bool useFloat16_, IfaceVarVecPtr vars_)
		: MiscTestParams	(taskCount_, meshCount_, width_, height_)
		, useInt64			(useInt64_)
		, useFloat64		(useFloat64_)
		, useInt16			(useInt16_)
		, useFloat16		(useFloat16_)
		, ifaceVars			(std::move(vars_))
	{}

	// These need to match the list of interface variables.
	bool			useInt64;
	bool			useFloat64;
	bool			useInt16;
	bool			useFloat16;

	IfaceVarVecPtr	ifaceVars;
};

class InterfaceVariablesCase : public MeshShaderMiscCase
{
public:
					InterfaceVariablesCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase(testCtx, name, description, std::move(params))
						{

						}
	virtual			~InterfaceVariablesCase		(void) {}

	TestInstance*	createInstance				(Context& context) const override;
	void			checkSupport				(Context& context) const override;
	void			initPrograms				(vk::SourceCollections& programCollection) const override;

	// Note data types in the input buffers are always plain floats or ints. They will be converted to the appropriate type when
	// copying them in or out of output variables. Note we have two variables per type, as per IfaceVar::kVarsPerType.

	struct PerVertexData
	{
		// Interpolated floats.

		tcu::Vec4	vert_f64d4_inter_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f64d4_inter_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f64d3_inter_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f64d3_inter_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f64d2_inter_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f64d2_inter_1[IfaceVar::kNumVertices];

		float		vert_f64d1_inter_0[IfaceVar::kNumVertices];
		float		vert_f64d1_inter_1[IfaceVar::kNumVertices];

		tcu::Vec4	vert_f32d4_inter_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f32d4_inter_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f32d3_inter_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f32d3_inter_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f32d2_inter_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f32d2_inter_1[IfaceVar::kNumVertices];

		float		vert_f32d1_inter_0[IfaceVar::kNumVertices];
		float		vert_f32d1_inter_1[IfaceVar::kNumVertices];

		tcu::Vec4	vert_f16d4_inter_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f16d4_inter_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f16d3_inter_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f16d3_inter_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f16d2_inter_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f16d2_inter_1[IfaceVar::kNumVertices];

		float		vert_f16d1_inter_0[IfaceVar::kNumVertices];
		float		vert_f16d1_inter_1[IfaceVar::kNumVertices];

		// Flat floats.

		tcu::Vec4	vert_f64d4_flat_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f64d4_flat_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f64d3_flat_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f64d3_flat_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f64d2_flat_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f64d2_flat_1[IfaceVar::kNumVertices];

		float		vert_f64d1_flat_0[IfaceVar::kNumVertices];
		float		vert_f64d1_flat_1[IfaceVar::kNumVertices];

		tcu::Vec4	vert_f32d4_flat_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f32d4_flat_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f32d3_flat_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f32d3_flat_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f32d2_flat_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f32d2_flat_1[IfaceVar::kNumVertices];

		float		vert_f32d1_flat_0[IfaceVar::kNumVertices];
		float		vert_f32d1_flat_1[IfaceVar::kNumVertices];

		tcu::Vec4	vert_f16d4_flat_0[IfaceVar::kNumVertices];
		tcu::Vec4	vert_f16d4_flat_1[IfaceVar::kNumVertices];

		tcu::Vec3	vert_f16d3_flat_0[IfaceVar::kNumVertices];
		tcu::Vec3	vert_f16d3_flat_1[IfaceVar::kNumVertices];

		tcu::Vec2	vert_f16d2_flat_0[IfaceVar::kNumVertices];
		tcu::Vec2	vert_f16d2_flat_1[IfaceVar::kNumVertices];

		float		vert_f16d1_flat_0[IfaceVar::kNumVertices];
		float		vert_f16d1_flat_1[IfaceVar::kNumVertices];

		// Flat ints.

		tcu::IVec4	vert_i64d4_flat_0[IfaceVar::kNumVertices];
		tcu::IVec4	vert_i64d4_flat_1[IfaceVar::kNumVertices];

		tcu::IVec3	vert_i64d3_flat_0[IfaceVar::kNumVertices];
		tcu::IVec3	vert_i64d3_flat_1[IfaceVar::kNumVertices];

		tcu::IVec2	vert_i64d2_flat_0[IfaceVar::kNumVertices];
		tcu::IVec2	vert_i64d2_flat_1[IfaceVar::kNumVertices];

		int32_t		vert_i64d1_flat_0[IfaceVar::kNumVertices];
		int32_t		vert_i64d1_flat_1[IfaceVar::kNumVertices];

		tcu::IVec4	vert_i32d4_flat_0[IfaceVar::kNumVertices];
		tcu::IVec4	vert_i32d4_flat_1[IfaceVar::kNumVertices];

		tcu::IVec3	vert_i32d3_flat_0[IfaceVar::kNumVertices];
		tcu::IVec3	vert_i32d3_flat_1[IfaceVar::kNumVertices];

		tcu::IVec2	vert_i32d2_flat_0[IfaceVar::kNumVertices];
		tcu::IVec2	vert_i32d2_flat_1[IfaceVar::kNumVertices];

		int32_t		vert_i32d1_flat_0[IfaceVar::kNumVertices];
		int32_t		vert_i32d1_flat_1[IfaceVar::kNumVertices];

		tcu::IVec4	vert_i16d4_flat_0[IfaceVar::kNumVertices];
		tcu::IVec4	vert_i16d4_flat_1[IfaceVar::kNumVertices];

		tcu::IVec3	vert_i16d3_flat_0[IfaceVar::kNumVertices];
		tcu::IVec3	vert_i16d3_flat_1[IfaceVar::kNumVertices];

		tcu::IVec2	vert_i16d2_flat_0[IfaceVar::kNumVertices];
		tcu::IVec2	vert_i16d2_flat_1[IfaceVar::kNumVertices];

		int32_t		vert_i16d1_flat_0[IfaceVar::kNumVertices];
		int32_t		vert_i16d1_flat_1[IfaceVar::kNumVertices];

	};

	struct PerPrimitiveData
	{
		// Flat floats.

		tcu::Vec4	prim_f64d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec4	prim_f64d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec3	prim_f64d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec3	prim_f64d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec2	prim_f64d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec2	prim_f64d2_flat_1[IfaceVar::kNumPrimitives];

		float		prim_f64d1_flat_0[IfaceVar::kNumPrimitives];
		float		prim_f64d1_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec4	prim_f32d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec4	prim_f32d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec3	prim_f32d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec3	prim_f32d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec2	prim_f32d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec2	prim_f32d2_flat_1[IfaceVar::kNumPrimitives];

		float		prim_f32d1_flat_0[IfaceVar::kNumPrimitives];
		float		prim_f32d1_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec4	prim_f16d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec4	prim_f16d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec3	prim_f16d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec3	prim_f16d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::Vec2	prim_f16d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::Vec2	prim_f16d2_flat_1[IfaceVar::kNumPrimitives];

		float		prim_f16d1_flat_0[IfaceVar::kNumPrimitives];
		float		prim_f16d1_flat_1[IfaceVar::kNumPrimitives];

		// Flat ints.

		tcu::IVec4	prim_i64d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec4	prim_i64d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec3	prim_i64d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec3	prim_i64d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec2	prim_i64d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec2	prim_i64d2_flat_1[IfaceVar::kNumPrimitives];

		int32_t		prim_i64d1_flat_0[IfaceVar::kNumPrimitives];
		int32_t		prim_i64d1_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec4	prim_i32d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec4	prim_i32d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec3	prim_i32d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec3	prim_i32d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec2	prim_i32d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec2	prim_i32d2_flat_1[IfaceVar::kNumPrimitives];

		int32_t		prim_i32d1_flat_0[IfaceVar::kNumPrimitives];
		int32_t		prim_i32d1_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec4	prim_i16d4_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec4	prim_i16d4_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec3	prim_i16d3_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec3	prim_i16d3_flat_1[IfaceVar::kNumPrimitives];

		tcu::IVec2	prim_i16d2_flat_0[IfaceVar::kNumPrimitives];
		tcu::IVec2	prim_i16d2_flat_1[IfaceVar::kNumPrimitives];

		int32_t		prim_i16d1_flat_0[IfaceVar::kNumPrimitives];
		int32_t		prim_i16d1_flat_1[IfaceVar::kNumPrimitives];

	};

	static constexpr uint32_t kGlslangBuiltInCount	= 11u;
	static constexpr uint32_t kMaxLocations			= 16u;
};

class InterfaceVariablesInstance : public MeshShaderMiscInstance
{
public:
						InterfaceVariablesInstance	(Context& context, const MiscTestParams* params)
							: MeshShaderMiscInstance(context, params) {}
	virtual				~InterfaceVariablesInstance	(void) {}

	void				generateReferenceLevel		() override;
	tcu::TestStatus		iterate						(void) override;
};

TestInstance* InterfaceVariablesCase::createInstance (Context& context) const
{
	return new InterfaceVariablesInstance(context, m_params.get());
}

void InterfaceVariablesCase::checkSupport (Context& context) const
{
	const auto params = dynamic_cast<InterfaceVariableParams*>(m_params.get());
	DE_ASSERT(params);

	MeshShaderMiscCase::checkSupport(context);

	if (params->useFloat64)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_FLOAT64);

	if (params->useInt64)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT64);

	if (params->useInt16)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT16);

	if (params->useFloat16)
	{
		const auto& features = context.getShaderFloat16Int8Features();
		if (!features.shaderFloat16)
			TCU_THROW(NotSupportedError, "shaderFloat16 feature not supported");
	}

	if (params->useInt16 || params->useFloat16)
	{
		const auto& features = context.get16BitStorageFeatures();
		if (!features.storageInputOutput16)
			TCU_THROW(NotSupportedError, "storageInputOutput16 feature not supported");
	}

	// glslang will use several built-ins in the generated mesh code, which count against the location and component limits.
	{
		const auto	neededComponents	= (kGlslangBuiltInCount + kMaxLocations) * 4u;
		const auto&	properties			= context.getDeviceProperties();

		if (neededComponents > properties.limits.maxFragmentInputComponents)
			TCU_THROW(NotSupportedError, "maxFragmentInputComponents too low to run this test");
	}
}

void InterfaceVariablesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Bindings needs to match the PerVertexData and PerPrimitiveData structures.
	std::ostringstream bindings;
	bindings
		<< "layout(set=0, binding=0, std430) readonly buffer PerVertexBlock {\n"
		<< "    vec4   vert_f64d4_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f64d4_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f64d3_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f64d3_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f64d2_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f64d2_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f64d1_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f64d1_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f32d4_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f32d4_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f32d3_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f32d3_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f32d2_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f32d2_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f32d1_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f32d1_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f16d4_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f16d4_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f16d3_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f16d3_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f16d2_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f16d2_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f16d1_inter_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f16d1_inter_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f64d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f64d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f64d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f64d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f64d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f64d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f64d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f64d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f32d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f32d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f32d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f32d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f32d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f32d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f32d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f32d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f16d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec4   vert_f16d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f16d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec3   vert_f16d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f16d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    vec2   vert_f16d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f16d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    float  vert_f16d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i64d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i64d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i64d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i64d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i64d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i64d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i64d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i64d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i32d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i32d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i32d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i32d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i32d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i32d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i32d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i32d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i16d4_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec4  vert_i16d4_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i16d3_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec3  vert_i16d3_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i16d2_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    ivec2  vert_i16d2_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i16d1_flat_0[" << IfaceVar::kNumVertices << "];\n"
		<< "    int    vert_i16d1_flat_1[" << IfaceVar::kNumVertices << "];\n"
		<< " } pvd;\n"
		<< "\n"
		<< "layout(set=0, binding=1, std430) readonly buffer PerPrimitiveBlock {\n"
		<< "    vec4   prim_f64d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f64d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f64d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f64d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f64d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f64d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f64d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f64d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f32d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f32d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f32d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f32d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f32d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f32d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f32d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f32d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f16d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec4   prim_f16d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f16d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec3   prim_f16d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f16d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    vec2   prim_f16d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f16d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    float  prim_f16d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i64d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i64d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i64d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i64d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i64d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i64d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i64d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i64d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i32d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i32d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i32d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i32d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i32d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i32d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i32d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i32d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i16d4_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec4  prim_i16d4_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i16d3_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec3  prim_i16d3_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i16d2_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    ivec2  prim_i16d2_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i16d1_flat_0[" << IfaceVar::kNumPrimitives << "];\n"
		<< "    int    prim_i16d1_flat_1[" << IfaceVar::kNumPrimitives << "];\n"
		<< " } ppd;\n"
		<< "\n"
		;
	const auto bindingsDecl = bindings.str();

	const auto	params	= dynamic_cast<InterfaceVariableParams*>(m_params.get());
	DE_ASSERT(params);
	const auto&	varVec	= *(params->ifaceVars);

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
		<< "\n"
		<< bindingsDecl
		;

	// Declare interface variables as Input in the fragment shader.
	{
		uint32_t usedLocations = 0u;
		for (const auto& var : varVec)
		{
			frag << var.getLocationDecl(usedLocations, Direction::IN);
			usedLocations += var.getLocationSize();
		}
	}

	frag
		<< "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		;

	// Emit checks for each variable value in the fragment shader.
	std::ostringstream allConditions;

	for (size_t i = 0; i < varVec.size(); ++i)
	{
		frag << varVec[i].getCheckStatement();
		allConditions << ((i == 0) ? "" : " && ") << varVec[i].getCheckName();
	}

	// Emit final check.
	frag
		<< "    if (" << allConditions.str() << ") {\n"
		<< "        outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "    } else {\n"
		<< "        outColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

	std::ostringstream pvdDataDeclStream;
	pvdDataDeclStream
		<< "    vec4 positions[4];\n"
		<< "    float pointSizes[4];\n"
		<< "    float clipDistances[4];\n"
		<< "    vec4 custom1[4];\n"
		<< "    float custom2[4];\n"
		<< "    int custom3[4];\n"
		;
	const auto pvdDataDecl = pvdDataDeclStream.str();

	std::ostringstream ppdDataDeclStream;
	ppdDataDeclStream
		<< "    int primitiveIds[2];\n"
		<< "    int viewportIndices[2];\n"
		<< "    uvec4 custom4[2];\n"
		<< "    float custom5[2];\n"
		;
	const auto ppdDataDecl = ppdDataDeclStream.str();

	std::ostringstream taskDataStream;
	taskDataStream << "taskNV TaskData {\n";
	for (size_t i = 0; i < varVec.size(); ++i)
		taskDataStream << varVec[i].getTypeAndNameDecl(/*arrayDecl*/true);
	taskDataStream << "} td;\n\n";

	const auto taskShader		= m_params->needsTaskShader();
	const auto taskDataDecl		= taskDataStream.str();
	const auto meshPvdPrefix	= (taskShader ? "td" : "pvd");
	const auto meshPpdPrefix	= (taskShader ? "td" : "ppd");

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
		<< "\n"
		<< "layout (local_size_x=1) in;\n"
		<< "layout (max_primitives=" << IfaceVar::kNumPrimitives << ", max_vertices=" << IfaceVar::kNumVertices << ") out;\n"
		<< "layout (triangles) out;\n"
		<< "\n"
		;

	// Declare interface variables as Output variables.
	{
		uint32_t usedLocations = 0u;
		for (const auto& var : varVec)
		{
			mesh << var.getLocationDecl(usedLocations, Direction::OUT);
			usedLocations += var.getLocationSize();
		}
	}

	mesh
		<< "out gl_MeshPerVertexNV {\n"
		<< "   vec4  gl_Position;\n"
		<< "} gl_MeshVerticesNV[];\n"
		<< "out perprimitiveNV gl_MeshPerPrimitiveNV {\n"
		<< "  int gl_PrimitiveID;\n"
		<< "} gl_MeshPrimitivesNV[];\n"
		<< "\n"
		<< (taskShader ? "in " + taskDataDecl : bindingsDecl)
		<< "vec4 positions[" << IfaceVar::kNumVertices << "] = vec4[](\n"
		<< "    vec4(-1.0, -1.0, 0.0, 1.0),\n"
		<< "    vec4( 1.0, -1.0, 0.0, 1.0),\n"
		<< "    vec4(-1.0,  1.0, 0.0, 1.0),\n"
		<< "    vec4( 1.0,  1.0, 0.0, 1.0)\n"
		<< ");\n"
		<< "\n"
		<< "int indices[" << (IfaceVar::kNumPrimitives * 3u) << "] = int[](\n"
		<< "    0, 1, 2, 2, 3, 1\n"
		<< ");\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = " << IfaceVar::kNumPrimitives << ";\n"
		<< "\n"
		;

	// Emit positions, indices and primitive IDs.
	for (uint32_t i = 0; i < IfaceVar::kNumVertices; ++i)
		mesh << "    gl_MeshVerticesNV[" << i << "].gl_Position = positions[" << i << "];\n";
	mesh << "\n";

	for (uint32_t i = 0; i < IfaceVar::kNumPrimitives; ++i)
	for (uint32_t j = 0; j < 3u; ++j) // 3 vertices per triangle
	{
		const auto arrayPos = i*3u + j;
		mesh << "    gl_PrimitiveIndicesNV[" << arrayPos << "] = indices[" << arrayPos << "];\n";
	}
	mesh << "\n";

	for (uint32_t i = 0; i < IfaceVar::kNumPrimitives; ++i)
		mesh << "    gl_MeshPrimitivesNV[" << i << "].gl_PrimitiveID = " << i << ";\n";
	mesh << "\n";

	// Copy data to output variables, either from the task data or the bindings.
	for (size_t i = 0; i < varVec.size(); ++i)
	{
		const auto arraySize	= varVec[i].getArraySize();
		const auto prefix		= ((varVec[i].owner == Owner::VERTEX) ? meshPvdPrefix : meshPpdPrefix);
		for (uint32_t arrayIndex = 0u; arrayIndex < arraySize; ++arrayIndex)
			mesh << varVec[i].getAssignmentStatement(arrayIndex, "", prefix);
	}

	mesh
		<< "\n"
		<< "}\n"
		;

	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());

	// Task shader if needed.
	if (taskShader)
	{
		const auto& meshCount		= m_params->meshCount;
		const auto	taskPvdPrefix	= "pvd";
		const auto	taskPpdPrefix	= "ppd";

		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
			<< "\n"
			<< "out " << taskDataDecl
			<< bindingsDecl
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = " << meshCount << ";\n"
			<< "\n"
			;

		// Copy data from bindings to the task data structure.
		for (size_t i = 0; i < varVec.size(); ++i)
		{
			const auto arraySize	= varVec[i].getArraySize();
			const auto prefix		= ((varVec[i].owner == Owner::VERTEX) ? taskPvdPrefix : taskPpdPrefix);

			for (uint32_t arrayIndex = 0u; arrayIndex < arraySize; ++arrayIndex)
				task << varVec[i].getAssignmentStatement(arrayIndex, "td", prefix);
		}

		task << "}\n";
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}
}

void InterfaceVariablesInstance::generateReferenceLevel ()
{
	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto access		= m_referenceLevel->getAccess();
	const auto blueColor	= tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);

	tcu::clear(access, blueColor);
}

tcu::TestStatus InterfaceVariablesInstance::iterate ()
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();

	const auto		imageFormat	= getOutputFormat();
	const auto		tcuFormat	= mapVkFormat(imageFormat);
	const auto		imageExtent	= makeExtent3D(m_params->width, m_params->height, 1u);
	const auto		imageUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	const auto&		binaries	= m_context.getBinaryCollection();
	const auto		hasTask		= binaries.contains("task");
	const auto		bufStages	= (VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_MESH_BIT_NV | (hasTask ? VK_SHADER_STAGE_TASK_BIT_NV : 0));

	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		imageFormat,							//	VkFormat				format;
		imageExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		imageUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	// Create color image and view.
	ImageWithMemory	colorImage	(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto		colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto		colorView	= makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

	// Create a memory buffer for verification.
	const auto			verificationBufferSize	= static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
	const auto			verificationBufferUsage	= (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const auto			verificationBufferInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

	BufferWithMemory	verificationBuffer		(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc	= verificationBuffer.getAllocation();
	void*				verificationBufferData	= verificationBufferAlloc.getHostPtr();

	// Bindings data.
	// The initialization statements below were generated automatically with a Python script.
	// Note: it works with stdin/stdout.
#if 0
import re
import sys

# Lines look like: tcu::Vec4 vert_f64d4_inter_0[IfaceVar::kNumVertices];
lineRE = re.compile(r'^\s*(\S+)\s+(\w+)\[(\S+)\];.*$')
vecRE = re.compile(r'^.*Vec(\d)$')
floatSuffixes = (
    (0.25, 0.50, 0.875, 0.0),
    (0.25, 0.75, 0.875, 0.0),
    (0.50, 0.50, 0.875, 0.0),
    (0.50, 0.75, 0.875, 0.0),
)
lineCounter = 0

for line in sys.stdin:
    match = lineRE.search(line)
    if not match:
        continue

    varType = match.group(1)
    varName = match.group(2)
    varSize = match.group(3)

    arraySize = (4 if varSize == 'IfaceVar::kNumVertices' else 2)
    vecMatch = vecRE.match(varType)
    numComponents = (1 if not vecMatch else vecMatch.group(1))
    isFlat = '_flat_' in varName

    lineCounter += 1
    varBaseVal = 1000 + 10 * lineCounter
    valueTemplate = ('%s' if numComponents == 1 else '%s(%%s)' % (varType,))

    for index in range(arraySize):
        valueStr = ''
        for comp in range(numComponents):
            compValue = varBaseVal + comp + 1
            if not isFlat:
                compValue += floatSuffixes[index][comp]
            valueStr += ('' if comp == 0 else ', ') + str(compValue)
        value = valueTemplate % (valueStr,)
        statement = '%s[%s] = %s;' % (varName, index, value)
        print('%s' % (statement,))
#endif
	InterfaceVariablesCase::PerVertexData perVertexData;
	{
		perVertexData.vert_f64d4_inter_0[0] = tcu::Vec4(1011.25, 1012.5, 1013.875, 1014.0);
		perVertexData.vert_f64d4_inter_0[1] = tcu::Vec4(1011.25, 1012.75, 1013.875, 1014.0);
		perVertexData.vert_f64d4_inter_0[2] = tcu::Vec4(1011.5, 1012.5, 1013.875, 1014.0);
		perVertexData.vert_f64d4_inter_0[3] = tcu::Vec4(1011.5, 1012.75, 1013.875, 1014.0);
		perVertexData.vert_f64d4_inter_1[0] = tcu::Vec4(1021.25, 1022.5, 1023.875, 1024.0);
		perVertexData.vert_f64d4_inter_1[1] = tcu::Vec4(1021.25, 1022.75, 1023.875, 1024.0);
		perVertexData.vert_f64d4_inter_1[2] = tcu::Vec4(1021.5, 1022.5, 1023.875, 1024.0);
		perVertexData.vert_f64d4_inter_1[3] = tcu::Vec4(1021.5, 1022.75, 1023.875, 1024.0);
		perVertexData.vert_f64d3_inter_0[0] = tcu::Vec3(1031.25, 1032.5, 1033.875);
		perVertexData.vert_f64d3_inter_0[1] = tcu::Vec3(1031.25, 1032.75, 1033.875);
		perVertexData.vert_f64d3_inter_0[2] = tcu::Vec3(1031.5, 1032.5, 1033.875);
		perVertexData.vert_f64d3_inter_0[3] = tcu::Vec3(1031.5, 1032.75, 1033.875);
		perVertexData.vert_f64d3_inter_1[0] = tcu::Vec3(1041.25, 1042.5, 1043.875);
		perVertexData.vert_f64d3_inter_1[1] = tcu::Vec3(1041.25, 1042.75, 1043.875);
		perVertexData.vert_f64d3_inter_1[2] = tcu::Vec3(1041.5, 1042.5, 1043.875);
		perVertexData.vert_f64d3_inter_1[3] = tcu::Vec3(1041.5, 1042.75, 1043.875);
		perVertexData.vert_f64d2_inter_0[0] = tcu::Vec2(1051.25, 1052.5);
		perVertexData.vert_f64d2_inter_0[1] = tcu::Vec2(1051.25, 1052.75);
		perVertexData.vert_f64d2_inter_0[2] = tcu::Vec2(1051.5, 1052.5);
		perVertexData.vert_f64d2_inter_0[3] = tcu::Vec2(1051.5, 1052.75);
		perVertexData.vert_f64d2_inter_1[0] = tcu::Vec2(1061.25, 1062.5);
		perVertexData.vert_f64d2_inter_1[1] = tcu::Vec2(1061.25, 1062.75);
		perVertexData.vert_f64d2_inter_1[2] = tcu::Vec2(1061.5, 1062.5);
		perVertexData.vert_f64d2_inter_1[3] = tcu::Vec2(1061.5, 1062.75);
		perVertexData.vert_f64d1_inter_0[0] = 1071.25;
		perVertexData.vert_f64d1_inter_0[1] = 1071.25;
		perVertexData.vert_f64d1_inter_0[2] = 1071.5;
		perVertexData.vert_f64d1_inter_0[3] = 1071.5;
		perVertexData.vert_f64d1_inter_1[0] = 1081.25;
		perVertexData.vert_f64d1_inter_1[1] = 1081.25;
		perVertexData.vert_f64d1_inter_1[2] = 1081.5;
		perVertexData.vert_f64d1_inter_1[3] = 1081.5;
		perVertexData.vert_f32d4_inter_0[0] = tcu::Vec4(1091.25, 1092.5, 1093.875, 1094.0);
		perVertexData.vert_f32d4_inter_0[1] = tcu::Vec4(1091.25, 1092.75, 1093.875, 1094.0);
		perVertexData.vert_f32d4_inter_0[2] = tcu::Vec4(1091.5, 1092.5, 1093.875, 1094.0);
		perVertexData.vert_f32d4_inter_0[3] = tcu::Vec4(1091.5, 1092.75, 1093.875, 1094.0);
		perVertexData.vert_f32d4_inter_1[0] = tcu::Vec4(1101.25, 1102.5, 1103.875, 1104.0);
		perVertexData.vert_f32d4_inter_1[1] = tcu::Vec4(1101.25, 1102.75, 1103.875, 1104.0);
		perVertexData.vert_f32d4_inter_1[2] = tcu::Vec4(1101.5, 1102.5, 1103.875, 1104.0);
		perVertexData.vert_f32d4_inter_1[3] = tcu::Vec4(1101.5, 1102.75, 1103.875, 1104.0);
		perVertexData.vert_f32d3_inter_0[0] = tcu::Vec3(1111.25, 1112.5, 1113.875);
		perVertexData.vert_f32d3_inter_0[1] = tcu::Vec3(1111.25, 1112.75, 1113.875);
		perVertexData.vert_f32d3_inter_0[2] = tcu::Vec3(1111.5, 1112.5, 1113.875);
		perVertexData.vert_f32d3_inter_0[3] = tcu::Vec3(1111.5, 1112.75, 1113.875);
		perVertexData.vert_f32d3_inter_1[0] = tcu::Vec3(1121.25, 1122.5, 1123.875);
		perVertexData.vert_f32d3_inter_1[1] = tcu::Vec3(1121.25, 1122.75, 1123.875);
		perVertexData.vert_f32d3_inter_1[2] = tcu::Vec3(1121.5, 1122.5, 1123.875);
		perVertexData.vert_f32d3_inter_1[3] = tcu::Vec3(1121.5, 1122.75, 1123.875);
		perVertexData.vert_f32d2_inter_0[0] = tcu::Vec2(1131.25, 1132.5);
		perVertexData.vert_f32d2_inter_0[1] = tcu::Vec2(1131.25, 1132.75);
		perVertexData.vert_f32d2_inter_0[2] = tcu::Vec2(1131.5, 1132.5);
		perVertexData.vert_f32d2_inter_0[3] = tcu::Vec2(1131.5, 1132.75);
		perVertexData.vert_f32d2_inter_1[0] = tcu::Vec2(1141.25, 1142.5);
		perVertexData.vert_f32d2_inter_1[1] = tcu::Vec2(1141.25, 1142.75);
		perVertexData.vert_f32d2_inter_1[2] = tcu::Vec2(1141.5, 1142.5);
		perVertexData.vert_f32d2_inter_1[3] = tcu::Vec2(1141.5, 1142.75);
		perVertexData.vert_f32d1_inter_0[0] = 1151.25;
		perVertexData.vert_f32d1_inter_0[1] = 1151.25;
		perVertexData.vert_f32d1_inter_0[2] = 1151.5;
		perVertexData.vert_f32d1_inter_0[3] = 1151.5;
		perVertexData.vert_f32d1_inter_1[0] = 1161.25;
		perVertexData.vert_f32d1_inter_1[1] = 1161.25;
		perVertexData.vert_f32d1_inter_1[2] = 1161.5;
		perVertexData.vert_f32d1_inter_1[3] = 1161.5;
		perVertexData.vert_f16d4_inter_0[0] = tcu::Vec4(1171.25, 1172.5, 1173.875, 1174.0);
		perVertexData.vert_f16d4_inter_0[1] = tcu::Vec4(1171.25, 1172.75, 1173.875, 1174.0);
		perVertexData.vert_f16d4_inter_0[2] = tcu::Vec4(1171.5, 1172.5, 1173.875, 1174.0);
		perVertexData.vert_f16d4_inter_0[3] = tcu::Vec4(1171.5, 1172.75, 1173.875, 1174.0);
		perVertexData.vert_f16d4_inter_1[0] = tcu::Vec4(1181.25, 1182.5, 1183.875, 1184.0);
		perVertexData.vert_f16d4_inter_1[1] = tcu::Vec4(1181.25, 1182.75, 1183.875, 1184.0);
		perVertexData.vert_f16d4_inter_1[2] = tcu::Vec4(1181.5, 1182.5, 1183.875, 1184.0);
		perVertexData.vert_f16d4_inter_1[3] = tcu::Vec4(1181.5, 1182.75, 1183.875, 1184.0);
		perVertexData.vert_f16d3_inter_0[0] = tcu::Vec3(1191.25, 1192.5, 1193.875);
		perVertexData.vert_f16d3_inter_0[1] = tcu::Vec3(1191.25, 1192.75, 1193.875);
		perVertexData.vert_f16d3_inter_0[2] = tcu::Vec3(1191.5, 1192.5, 1193.875);
		perVertexData.vert_f16d3_inter_0[3] = tcu::Vec3(1191.5, 1192.75, 1193.875);
		perVertexData.vert_f16d3_inter_1[0] = tcu::Vec3(1201.25, 1202.5, 1203.875);
		perVertexData.vert_f16d3_inter_1[1] = tcu::Vec3(1201.25, 1202.75, 1203.875);
		perVertexData.vert_f16d3_inter_1[2] = tcu::Vec3(1201.5, 1202.5, 1203.875);
		perVertexData.vert_f16d3_inter_1[3] = tcu::Vec3(1201.5, 1202.75, 1203.875);
		perVertexData.vert_f16d2_inter_0[0] = tcu::Vec2(1211.25, 1212.5);
		perVertexData.vert_f16d2_inter_0[1] = tcu::Vec2(1211.25, 1212.75);
		perVertexData.vert_f16d2_inter_0[2] = tcu::Vec2(1211.5, 1212.5);
		perVertexData.vert_f16d2_inter_0[3] = tcu::Vec2(1211.5, 1212.75);
		perVertexData.vert_f16d2_inter_1[0] = tcu::Vec2(1221.25, 1222.5);
		perVertexData.vert_f16d2_inter_1[1] = tcu::Vec2(1221.25, 1222.75);
		perVertexData.vert_f16d2_inter_1[2] = tcu::Vec2(1221.5, 1222.5);
		perVertexData.vert_f16d2_inter_1[3] = tcu::Vec2(1221.5, 1222.75);
		perVertexData.vert_f16d1_inter_0[0] = 1231.25;
		perVertexData.vert_f16d1_inter_0[1] = 1231.25;
		perVertexData.vert_f16d1_inter_0[2] = 1231.5;
		perVertexData.vert_f16d1_inter_0[3] = 1231.5;
		perVertexData.vert_f16d1_inter_1[0] = 1241.25;
		perVertexData.vert_f16d1_inter_1[1] = 1241.25;
		perVertexData.vert_f16d1_inter_1[2] = 1241.5;
		perVertexData.vert_f16d1_inter_1[3] = 1241.5;
		perVertexData.vert_f64d4_flat_0[0] = tcu::Vec4(1251, 1252, 1253, 1254);
		perVertexData.vert_f64d4_flat_0[1] = tcu::Vec4(1251, 1252, 1253, 1254);
		perVertexData.vert_f64d4_flat_0[2] = tcu::Vec4(1251, 1252, 1253, 1254);
		perVertexData.vert_f64d4_flat_0[3] = tcu::Vec4(1251, 1252, 1253, 1254);
		perVertexData.vert_f64d4_flat_1[0] = tcu::Vec4(1261, 1262, 1263, 1264);
		perVertexData.vert_f64d4_flat_1[1] = tcu::Vec4(1261, 1262, 1263, 1264);
		perVertexData.vert_f64d4_flat_1[2] = tcu::Vec4(1261, 1262, 1263, 1264);
		perVertexData.vert_f64d4_flat_1[3] = tcu::Vec4(1261, 1262, 1263, 1264);
		perVertexData.vert_f64d3_flat_0[0] = tcu::Vec3(1271, 1272, 1273);
		perVertexData.vert_f64d3_flat_0[1] = tcu::Vec3(1271, 1272, 1273);
		perVertexData.vert_f64d3_flat_0[2] = tcu::Vec3(1271, 1272, 1273);
		perVertexData.vert_f64d3_flat_0[3] = tcu::Vec3(1271, 1272, 1273);
		perVertexData.vert_f64d3_flat_1[0] = tcu::Vec3(1281, 1282, 1283);
		perVertexData.vert_f64d3_flat_1[1] = tcu::Vec3(1281, 1282, 1283);
		perVertexData.vert_f64d3_flat_1[2] = tcu::Vec3(1281, 1282, 1283);
		perVertexData.vert_f64d3_flat_1[3] = tcu::Vec3(1281, 1282, 1283);
		perVertexData.vert_f64d2_flat_0[0] = tcu::Vec2(1291, 1292);
		perVertexData.vert_f64d2_flat_0[1] = tcu::Vec2(1291, 1292);
		perVertexData.vert_f64d2_flat_0[2] = tcu::Vec2(1291, 1292);
		perVertexData.vert_f64d2_flat_0[3] = tcu::Vec2(1291, 1292);
		perVertexData.vert_f64d2_flat_1[0] = tcu::Vec2(1301, 1302);
		perVertexData.vert_f64d2_flat_1[1] = tcu::Vec2(1301, 1302);
		perVertexData.vert_f64d2_flat_1[2] = tcu::Vec2(1301, 1302);
		perVertexData.vert_f64d2_flat_1[3] = tcu::Vec2(1301, 1302);
		perVertexData.vert_f64d1_flat_0[0] = 1311;
		perVertexData.vert_f64d1_flat_0[1] = 1311;
		perVertexData.vert_f64d1_flat_0[2] = 1311;
		perVertexData.vert_f64d1_flat_0[3] = 1311;
		perVertexData.vert_f64d1_flat_1[0] = 1321;
		perVertexData.vert_f64d1_flat_1[1] = 1321;
		perVertexData.vert_f64d1_flat_1[2] = 1321;
		perVertexData.vert_f64d1_flat_1[3] = 1321;
		perVertexData.vert_f32d4_flat_0[0] = tcu::Vec4(1331, 1332, 1333, 1334);
		perVertexData.vert_f32d4_flat_0[1] = tcu::Vec4(1331, 1332, 1333, 1334);
		perVertexData.vert_f32d4_flat_0[2] = tcu::Vec4(1331, 1332, 1333, 1334);
		perVertexData.vert_f32d4_flat_0[3] = tcu::Vec4(1331, 1332, 1333, 1334);
		perVertexData.vert_f32d4_flat_1[0] = tcu::Vec4(1341, 1342, 1343, 1344);
		perVertexData.vert_f32d4_flat_1[1] = tcu::Vec4(1341, 1342, 1343, 1344);
		perVertexData.vert_f32d4_flat_1[2] = tcu::Vec4(1341, 1342, 1343, 1344);
		perVertexData.vert_f32d4_flat_1[3] = tcu::Vec4(1341, 1342, 1343, 1344);
		perVertexData.vert_f32d3_flat_0[0] = tcu::Vec3(1351, 1352, 1353);
		perVertexData.vert_f32d3_flat_0[1] = tcu::Vec3(1351, 1352, 1353);
		perVertexData.vert_f32d3_flat_0[2] = tcu::Vec3(1351, 1352, 1353);
		perVertexData.vert_f32d3_flat_0[3] = tcu::Vec3(1351, 1352, 1353);
		perVertexData.vert_f32d3_flat_1[0] = tcu::Vec3(1361, 1362, 1363);
		perVertexData.vert_f32d3_flat_1[1] = tcu::Vec3(1361, 1362, 1363);
		perVertexData.vert_f32d3_flat_1[2] = tcu::Vec3(1361, 1362, 1363);
		perVertexData.vert_f32d3_flat_1[3] = tcu::Vec3(1361, 1362, 1363);
		perVertexData.vert_f32d2_flat_0[0] = tcu::Vec2(1371, 1372);
		perVertexData.vert_f32d2_flat_0[1] = tcu::Vec2(1371, 1372);
		perVertexData.vert_f32d2_flat_0[2] = tcu::Vec2(1371, 1372);
		perVertexData.vert_f32d2_flat_0[3] = tcu::Vec2(1371, 1372);
		perVertexData.vert_f32d2_flat_1[0] = tcu::Vec2(1381, 1382);
		perVertexData.vert_f32d2_flat_1[1] = tcu::Vec2(1381, 1382);
		perVertexData.vert_f32d2_flat_1[2] = tcu::Vec2(1381, 1382);
		perVertexData.vert_f32d2_flat_1[3] = tcu::Vec2(1381, 1382);
		perVertexData.vert_f32d1_flat_0[0] = 1391;
		perVertexData.vert_f32d1_flat_0[1] = 1391;
		perVertexData.vert_f32d1_flat_0[2] = 1391;
		perVertexData.vert_f32d1_flat_0[3] = 1391;
		perVertexData.vert_f32d1_flat_1[0] = 1401;
		perVertexData.vert_f32d1_flat_1[1] = 1401;
		perVertexData.vert_f32d1_flat_1[2] = 1401;
		perVertexData.vert_f32d1_flat_1[3] = 1401;
		perVertexData.vert_f16d4_flat_0[0] = tcu::Vec4(1411, 1412, 1413, 1414);
		perVertexData.vert_f16d4_flat_0[1] = tcu::Vec4(1411, 1412, 1413, 1414);
		perVertexData.vert_f16d4_flat_0[2] = tcu::Vec4(1411, 1412, 1413, 1414);
		perVertexData.vert_f16d4_flat_0[3] = tcu::Vec4(1411, 1412, 1413, 1414);
		perVertexData.vert_f16d4_flat_1[0] = tcu::Vec4(1421, 1422, 1423, 1424);
		perVertexData.vert_f16d4_flat_1[1] = tcu::Vec4(1421, 1422, 1423, 1424);
		perVertexData.vert_f16d4_flat_1[2] = tcu::Vec4(1421, 1422, 1423, 1424);
		perVertexData.vert_f16d4_flat_1[3] = tcu::Vec4(1421, 1422, 1423, 1424);
		perVertexData.vert_f16d3_flat_0[0] = tcu::Vec3(1431, 1432, 1433);
		perVertexData.vert_f16d3_flat_0[1] = tcu::Vec3(1431, 1432, 1433);
		perVertexData.vert_f16d3_flat_0[2] = tcu::Vec3(1431, 1432, 1433);
		perVertexData.vert_f16d3_flat_0[3] = tcu::Vec3(1431, 1432, 1433);
		perVertexData.vert_f16d3_flat_1[0] = tcu::Vec3(1441, 1442, 1443);
		perVertexData.vert_f16d3_flat_1[1] = tcu::Vec3(1441, 1442, 1443);
		perVertexData.vert_f16d3_flat_1[2] = tcu::Vec3(1441, 1442, 1443);
		perVertexData.vert_f16d3_flat_1[3] = tcu::Vec3(1441, 1442, 1443);
		perVertexData.vert_f16d2_flat_0[0] = tcu::Vec2(1451, 1452);
		perVertexData.vert_f16d2_flat_0[1] = tcu::Vec2(1451, 1452);
		perVertexData.vert_f16d2_flat_0[2] = tcu::Vec2(1451, 1452);
		perVertexData.vert_f16d2_flat_0[3] = tcu::Vec2(1451, 1452);
		perVertexData.vert_f16d2_flat_1[0] = tcu::Vec2(1461, 1462);
		perVertexData.vert_f16d2_flat_1[1] = tcu::Vec2(1461, 1462);
		perVertexData.vert_f16d2_flat_1[2] = tcu::Vec2(1461, 1462);
		perVertexData.vert_f16d2_flat_1[3] = tcu::Vec2(1461, 1462);
		perVertexData.vert_f16d1_flat_0[0] = 1471;
		perVertexData.vert_f16d1_flat_0[1] = 1471;
		perVertexData.vert_f16d1_flat_0[2] = 1471;
		perVertexData.vert_f16d1_flat_0[3] = 1471;
		perVertexData.vert_f16d1_flat_1[0] = 1481;
		perVertexData.vert_f16d1_flat_1[1] = 1481;
		perVertexData.vert_f16d1_flat_1[2] = 1481;
		perVertexData.vert_f16d1_flat_1[3] = 1481;
		perVertexData.vert_i64d4_flat_0[0] = tcu::IVec4(1491, 1492, 1493, 1494);
		perVertexData.vert_i64d4_flat_0[1] = tcu::IVec4(1491, 1492, 1493, 1494);
		perVertexData.vert_i64d4_flat_0[2] = tcu::IVec4(1491, 1492, 1493, 1494);
		perVertexData.vert_i64d4_flat_0[3] = tcu::IVec4(1491, 1492, 1493, 1494);
		perVertexData.vert_i64d4_flat_1[0] = tcu::IVec4(1501, 1502, 1503, 1504);
		perVertexData.vert_i64d4_flat_1[1] = tcu::IVec4(1501, 1502, 1503, 1504);
		perVertexData.vert_i64d4_flat_1[2] = tcu::IVec4(1501, 1502, 1503, 1504);
		perVertexData.vert_i64d4_flat_1[3] = tcu::IVec4(1501, 1502, 1503, 1504);
		perVertexData.vert_i64d3_flat_0[0] = tcu::IVec3(1511, 1512, 1513);
		perVertexData.vert_i64d3_flat_0[1] = tcu::IVec3(1511, 1512, 1513);
		perVertexData.vert_i64d3_flat_0[2] = tcu::IVec3(1511, 1512, 1513);
		perVertexData.vert_i64d3_flat_0[3] = tcu::IVec3(1511, 1512, 1513);
		perVertexData.vert_i64d3_flat_1[0] = tcu::IVec3(1521, 1522, 1523);
		perVertexData.vert_i64d3_flat_1[1] = tcu::IVec3(1521, 1522, 1523);
		perVertexData.vert_i64d3_flat_1[2] = tcu::IVec3(1521, 1522, 1523);
		perVertexData.vert_i64d3_flat_1[3] = tcu::IVec3(1521, 1522, 1523);
		perVertexData.vert_i64d2_flat_0[0] = tcu::IVec2(1531, 1532);
		perVertexData.vert_i64d2_flat_0[1] = tcu::IVec2(1531, 1532);
		perVertexData.vert_i64d2_flat_0[2] = tcu::IVec2(1531, 1532);
		perVertexData.vert_i64d2_flat_0[3] = tcu::IVec2(1531, 1532);
		perVertexData.vert_i64d2_flat_1[0] = tcu::IVec2(1541, 1542);
		perVertexData.vert_i64d2_flat_1[1] = tcu::IVec2(1541, 1542);
		perVertexData.vert_i64d2_flat_1[2] = tcu::IVec2(1541, 1542);
		perVertexData.vert_i64d2_flat_1[3] = tcu::IVec2(1541, 1542);
		perVertexData.vert_i64d1_flat_0[0] = 1551;
		perVertexData.vert_i64d1_flat_0[1] = 1551;
		perVertexData.vert_i64d1_flat_0[2] = 1551;
		perVertexData.vert_i64d1_flat_0[3] = 1551;
		perVertexData.vert_i64d1_flat_1[0] = 1561;
		perVertexData.vert_i64d1_flat_1[1] = 1561;
		perVertexData.vert_i64d1_flat_1[2] = 1561;
		perVertexData.vert_i64d1_flat_1[3] = 1561;
		perVertexData.vert_i32d4_flat_0[0] = tcu::IVec4(1571, 1572, 1573, 1574);
		perVertexData.vert_i32d4_flat_0[1] = tcu::IVec4(1571, 1572, 1573, 1574);
		perVertexData.vert_i32d4_flat_0[2] = tcu::IVec4(1571, 1572, 1573, 1574);
		perVertexData.vert_i32d4_flat_0[3] = tcu::IVec4(1571, 1572, 1573, 1574);
		perVertexData.vert_i32d4_flat_1[0] = tcu::IVec4(1581, 1582, 1583, 1584);
		perVertexData.vert_i32d4_flat_1[1] = tcu::IVec4(1581, 1582, 1583, 1584);
		perVertexData.vert_i32d4_flat_1[2] = tcu::IVec4(1581, 1582, 1583, 1584);
		perVertexData.vert_i32d4_flat_1[3] = tcu::IVec4(1581, 1582, 1583, 1584);
		perVertexData.vert_i32d3_flat_0[0] = tcu::IVec3(1591, 1592, 1593);
		perVertexData.vert_i32d3_flat_0[1] = tcu::IVec3(1591, 1592, 1593);
		perVertexData.vert_i32d3_flat_0[2] = tcu::IVec3(1591, 1592, 1593);
		perVertexData.vert_i32d3_flat_0[3] = tcu::IVec3(1591, 1592, 1593);
		perVertexData.vert_i32d3_flat_1[0] = tcu::IVec3(1601, 1602, 1603);
		perVertexData.vert_i32d3_flat_1[1] = tcu::IVec3(1601, 1602, 1603);
		perVertexData.vert_i32d3_flat_1[2] = tcu::IVec3(1601, 1602, 1603);
		perVertexData.vert_i32d3_flat_1[3] = tcu::IVec3(1601, 1602, 1603);
		perVertexData.vert_i32d2_flat_0[0] = tcu::IVec2(1611, 1612);
		perVertexData.vert_i32d2_flat_0[1] = tcu::IVec2(1611, 1612);
		perVertexData.vert_i32d2_flat_0[2] = tcu::IVec2(1611, 1612);
		perVertexData.vert_i32d2_flat_0[3] = tcu::IVec2(1611, 1612);
		perVertexData.vert_i32d2_flat_1[0] = tcu::IVec2(1621, 1622);
		perVertexData.vert_i32d2_flat_1[1] = tcu::IVec2(1621, 1622);
		perVertexData.vert_i32d2_flat_1[2] = tcu::IVec2(1621, 1622);
		perVertexData.vert_i32d2_flat_1[3] = tcu::IVec2(1621, 1622);
		perVertexData.vert_i32d1_flat_0[0] = 1631;
		perVertexData.vert_i32d1_flat_0[1] = 1631;
		perVertexData.vert_i32d1_flat_0[2] = 1631;
		perVertexData.vert_i32d1_flat_0[3] = 1631;
		perVertexData.vert_i32d1_flat_1[0] = 1641;
		perVertexData.vert_i32d1_flat_1[1] = 1641;
		perVertexData.vert_i32d1_flat_1[2] = 1641;
		perVertexData.vert_i32d1_flat_1[3] = 1641;
		perVertexData.vert_i16d4_flat_0[0] = tcu::IVec4(1651, 1652, 1653, 1654);
		perVertexData.vert_i16d4_flat_0[1] = tcu::IVec4(1651, 1652, 1653, 1654);
		perVertexData.vert_i16d4_flat_0[2] = tcu::IVec4(1651, 1652, 1653, 1654);
		perVertexData.vert_i16d4_flat_0[3] = tcu::IVec4(1651, 1652, 1653, 1654);
		perVertexData.vert_i16d4_flat_1[0] = tcu::IVec4(1661, 1662, 1663, 1664);
		perVertexData.vert_i16d4_flat_1[1] = tcu::IVec4(1661, 1662, 1663, 1664);
		perVertexData.vert_i16d4_flat_1[2] = tcu::IVec4(1661, 1662, 1663, 1664);
		perVertexData.vert_i16d4_flat_1[3] = tcu::IVec4(1661, 1662, 1663, 1664);
		perVertexData.vert_i16d3_flat_0[0] = tcu::IVec3(1671, 1672, 1673);
		perVertexData.vert_i16d3_flat_0[1] = tcu::IVec3(1671, 1672, 1673);
		perVertexData.vert_i16d3_flat_0[2] = tcu::IVec3(1671, 1672, 1673);
		perVertexData.vert_i16d3_flat_0[3] = tcu::IVec3(1671, 1672, 1673);
		perVertexData.vert_i16d3_flat_1[0] = tcu::IVec3(1681, 1682, 1683);
		perVertexData.vert_i16d3_flat_1[1] = tcu::IVec3(1681, 1682, 1683);
		perVertexData.vert_i16d3_flat_1[2] = tcu::IVec3(1681, 1682, 1683);
		perVertexData.vert_i16d3_flat_1[3] = tcu::IVec3(1681, 1682, 1683);
		perVertexData.vert_i16d2_flat_0[0] = tcu::IVec2(1691, 1692);
		perVertexData.vert_i16d2_flat_0[1] = tcu::IVec2(1691, 1692);
		perVertexData.vert_i16d2_flat_0[2] = tcu::IVec2(1691, 1692);
		perVertexData.vert_i16d2_flat_0[3] = tcu::IVec2(1691, 1692);
		perVertexData.vert_i16d2_flat_1[0] = tcu::IVec2(1701, 1702);
		perVertexData.vert_i16d2_flat_1[1] = tcu::IVec2(1701, 1702);
		perVertexData.vert_i16d2_flat_1[2] = tcu::IVec2(1701, 1702);
		perVertexData.vert_i16d2_flat_1[3] = tcu::IVec2(1701, 1702);
		perVertexData.vert_i16d1_flat_0[0] = 1711;
		perVertexData.vert_i16d1_flat_0[1] = 1711;
		perVertexData.vert_i16d1_flat_0[2] = 1711;
		perVertexData.vert_i16d1_flat_0[3] = 1711;
		perVertexData.vert_i16d1_flat_1[0] = 1721;
		perVertexData.vert_i16d1_flat_1[1] = 1721;
		perVertexData.vert_i16d1_flat_1[2] = 1721;
		perVertexData.vert_i16d1_flat_1[3] = 1721;
	}

	InterfaceVariablesCase::PerPrimitiveData perPrimitiveData;
	{
		perPrimitiveData.prim_f64d4_flat_0[0] = tcu::Vec4(1011, 1012, 1013, 1014);
		perPrimitiveData.prim_f64d4_flat_0[1] = tcu::Vec4(1011, 1012, 1013, 1014);
		perPrimitiveData.prim_f64d4_flat_1[0] = tcu::Vec4(1021, 1022, 1023, 1024);
		perPrimitiveData.prim_f64d4_flat_1[1] = tcu::Vec4(1021, 1022, 1023, 1024);
		perPrimitiveData.prim_f64d3_flat_0[0] = tcu::Vec3(1031, 1032, 1033);
		perPrimitiveData.prim_f64d3_flat_0[1] = tcu::Vec3(1031, 1032, 1033);
		perPrimitiveData.prim_f64d3_flat_1[0] = tcu::Vec3(1041, 1042, 1043);
		perPrimitiveData.prim_f64d3_flat_1[1] = tcu::Vec3(1041, 1042, 1043);
		perPrimitiveData.prim_f64d2_flat_0[0] = tcu::Vec2(1051, 1052);
		perPrimitiveData.prim_f64d2_flat_0[1] = tcu::Vec2(1051, 1052);
		perPrimitiveData.prim_f64d2_flat_1[0] = tcu::Vec2(1061, 1062);
		perPrimitiveData.prim_f64d2_flat_1[1] = tcu::Vec2(1061, 1062);
		perPrimitiveData.prim_f64d1_flat_0[0] = 1071;
		perPrimitiveData.prim_f64d1_flat_0[1] = 1071;
		perPrimitiveData.prim_f64d1_flat_1[0] = 1081;
		perPrimitiveData.prim_f64d1_flat_1[1] = 1081;
		perPrimitiveData.prim_f32d4_flat_0[0] = tcu::Vec4(1091, 1092, 1093, 1094);
		perPrimitiveData.prim_f32d4_flat_0[1] = tcu::Vec4(1091, 1092, 1093, 1094);
		perPrimitiveData.prim_f32d4_flat_1[0] = tcu::Vec4(1101, 1102, 1103, 1104);
		perPrimitiveData.prim_f32d4_flat_1[1] = tcu::Vec4(1101, 1102, 1103, 1104);
		perPrimitiveData.prim_f32d3_flat_0[0] = tcu::Vec3(1111, 1112, 1113);
		perPrimitiveData.prim_f32d3_flat_0[1] = tcu::Vec3(1111, 1112, 1113);
		perPrimitiveData.prim_f32d3_flat_1[0] = tcu::Vec3(1121, 1122, 1123);
		perPrimitiveData.prim_f32d3_flat_1[1] = tcu::Vec3(1121, 1122, 1123);
		perPrimitiveData.prim_f32d2_flat_0[0] = tcu::Vec2(1131, 1132);
		perPrimitiveData.prim_f32d2_flat_0[1] = tcu::Vec2(1131, 1132);
		perPrimitiveData.prim_f32d2_flat_1[0] = tcu::Vec2(1141, 1142);
		perPrimitiveData.prim_f32d2_flat_1[1] = tcu::Vec2(1141, 1142);
		perPrimitiveData.prim_f32d1_flat_0[0] = 1151;
		perPrimitiveData.prim_f32d1_flat_0[1] = 1151;
		perPrimitiveData.prim_f32d1_flat_1[0] = 1161;
		perPrimitiveData.prim_f32d1_flat_1[1] = 1161;
		perPrimitiveData.prim_f16d4_flat_0[0] = tcu::Vec4(1171, 1172, 1173, 1174);
		perPrimitiveData.prim_f16d4_flat_0[1] = tcu::Vec4(1171, 1172, 1173, 1174);
		perPrimitiveData.prim_f16d4_flat_1[0] = tcu::Vec4(1181, 1182, 1183, 1184);
		perPrimitiveData.prim_f16d4_flat_1[1] = tcu::Vec4(1181, 1182, 1183, 1184);
		perPrimitiveData.prim_f16d3_flat_0[0] = tcu::Vec3(1191, 1192, 1193);
		perPrimitiveData.prim_f16d3_flat_0[1] = tcu::Vec3(1191, 1192, 1193);
		perPrimitiveData.prim_f16d3_flat_1[0] = tcu::Vec3(1201, 1202, 1203);
		perPrimitiveData.prim_f16d3_flat_1[1] = tcu::Vec3(1201, 1202, 1203);
		perPrimitiveData.prim_f16d2_flat_0[0] = tcu::Vec2(1211, 1212);
		perPrimitiveData.prim_f16d2_flat_0[1] = tcu::Vec2(1211, 1212);
		perPrimitiveData.prim_f16d2_flat_1[0] = tcu::Vec2(1221, 1222);
		perPrimitiveData.prim_f16d2_flat_1[1] = tcu::Vec2(1221, 1222);
		perPrimitiveData.prim_f16d1_flat_0[0] = 1231;
		perPrimitiveData.prim_f16d1_flat_0[1] = 1231;
		perPrimitiveData.prim_f16d1_flat_1[0] = 1241;
		perPrimitiveData.prim_f16d1_flat_1[1] = 1241;
		perPrimitiveData.prim_i64d4_flat_0[0] = tcu::IVec4(1251, 1252, 1253, 1254);
		perPrimitiveData.prim_i64d4_flat_0[1] = tcu::IVec4(1251, 1252, 1253, 1254);
		perPrimitiveData.prim_i64d4_flat_1[0] = tcu::IVec4(1261, 1262, 1263, 1264);
		perPrimitiveData.prim_i64d4_flat_1[1] = tcu::IVec4(1261, 1262, 1263, 1264);
		perPrimitiveData.prim_i64d3_flat_0[0] = tcu::IVec3(1271, 1272, 1273);
		perPrimitiveData.prim_i64d3_flat_0[1] = tcu::IVec3(1271, 1272, 1273);
		perPrimitiveData.prim_i64d3_flat_1[0] = tcu::IVec3(1281, 1282, 1283);
		perPrimitiveData.prim_i64d3_flat_1[1] = tcu::IVec3(1281, 1282, 1283);
		perPrimitiveData.prim_i64d2_flat_0[0] = tcu::IVec2(1291, 1292);
		perPrimitiveData.prim_i64d2_flat_0[1] = tcu::IVec2(1291, 1292);
		perPrimitiveData.prim_i64d2_flat_1[0] = tcu::IVec2(1301, 1302);
		perPrimitiveData.prim_i64d2_flat_1[1] = tcu::IVec2(1301, 1302);
		perPrimitiveData.prim_i64d1_flat_0[0] = 1311;
		perPrimitiveData.prim_i64d1_flat_0[1] = 1311;
		perPrimitiveData.prim_i64d1_flat_1[0] = 1321;
		perPrimitiveData.prim_i64d1_flat_1[1] = 1321;
		perPrimitiveData.prim_i32d4_flat_0[0] = tcu::IVec4(1331, 1332, 1333, 1334);
		perPrimitiveData.prim_i32d4_flat_0[1] = tcu::IVec4(1331, 1332, 1333, 1334);
		perPrimitiveData.prim_i32d4_flat_1[0] = tcu::IVec4(1341, 1342, 1343, 1344);
		perPrimitiveData.prim_i32d4_flat_1[1] = tcu::IVec4(1341, 1342, 1343, 1344);
		perPrimitiveData.prim_i32d3_flat_0[0] = tcu::IVec3(1351, 1352, 1353);
		perPrimitiveData.prim_i32d3_flat_0[1] = tcu::IVec3(1351, 1352, 1353);
		perPrimitiveData.prim_i32d3_flat_1[0] = tcu::IVec3(1361, 1362, 1363);
		perPrimitiveData.prim_i32d3_flat_1[1] = tcu::IVec3(1361, 1362, 1363);
		perPrimitiveData.prim_i32d2_flat_0[0] = tcu::IVec2(1371, 1372);
		perPrimitiveData.prim_i32d2_flat_0[1] = tcu::IVec2(1371, 1372);
		perPrimitiveData.prim_i32d2_flat_1[0] = tcu::IVec2(1381, 1382);
		perPrimitiveData.prim_i32d2_flat_1[1] = tcu::IVec2(1381, 1382);
		perPrimitiveData.prim_i32d1_flat_0[0] = 1391;
		perPrimitiveData.prim_i32d1_flat_0[1] = 1391;
		perPrimitiveData.prim_i32d1_flat_1[0] = 1401;
		perPrimitiveData.prim_i32d1_flat_1[1] = 1401;
		perPrimitiveData.prim_i16d4_flat_0[0] = tcu::IVec4(1411, 1412, 1413, 1414);
		perPrimitiveData.prim_i16d4_flat_0[1] = tcu::IVec4(1411, 1412, 1413, 1414);
		perPrimitiveData.prim_i16d4_flat_1[0] = tcu::IVec4(1421, 1422, 1423, 1424);
		perPrimitiveData.prim_i16d4_flat_1[1] = tcu::IVec4(1421, 1422, 1423, 1424);
		perPrimitiveData.prim_i16d3_flat_0[0] = tcu::IVec3(1431, 1432, 1433);
		perPrimitiveData.prim_i16d3_flat_0[1] = tcu::IVec3(1431, 1432, 1433);
		perPrimitiveData.prim_i16d3_flat_1[0] = tcu::IVec3(1441, 1442, 1443);
		perPrimitiveData.prim_i16d3_flat_1[1] = tcu::IVec3(1441, 1442, 1443);
		perPrimitiveData.prim_i16d2_flat_0[0] = tcu::IVec2(1451, 1452);
		perPrimitiveData.prim_i16d2_flat_0[1] = tcu::IVec2(1451, 1452);
		perPrimitiveData.prim_i16d2_flat_1[0] = tcu::IVec2(1461, 1462);
		perPrimitiveData.prim_i16d2_flat_1[1] = tcu::IVec2(1461, 1462);
		perPrimitiveData.prim_i16d1_flat_0[0] = 1471;
		perPrimitiveData.prim_i16d1_flat_0[1] = 1471;
		perPrimitiveData.prim_i16d1_flat_1[0] = 1481;
		perPrimitiveData.prim_i16d1_flat_1[1] = 1481;
	}

	// Create and fill buffers with this data.
	const auto			pvdSize		= static_cast<VkDeviceSize>(sizeof(perVertexData));
	const auto			pvdInfo		= makeBufferCreateInfo(pvdSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	pvdData		(vkd, device, alloc, pvdInfo, MemoryRequirement::HostVisible);
	auto&				pvdAlloc	= pvdData.getAllocation();
	void*				pvdPtr		= pvdAlloc.getHostPtr();

	const auto			ppdSize		= static_cast<VkDeviceSize>(sizeof(perPrimitiveData));
	const auto			ppdInfo		= makeBufferCreateInfo(ppdSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	ppdData		(vkd, device, alloc, ppdInfo, MemoryRequirement::HostVisible);
	auto&				ppdAlloc	= ppdData.getAllocation();
	void*				ppdPtr		= ppdAlloc.getHostPtr();

	deMemcpy(pvdPtr, &perVertexData, sizeof(perVertexData));
	deMemcpy(ppdPtr, &perPrimitiveData, sizeof(perPrimitiveData));

	flushAlloc(vkd, device, pvdAlloc);
	flushAlloc(vkd, device, ppdAlloc);

	// Descriptor set layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufStages);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufStages);
	const auto setLayout = setLayoutBuilder.build(vkd, device);

	// Create and update descriptor set.
	DescriptorPoolBuilder descriptorPoolBuilder;
	descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u);
	const auto descriptorPool	= descriptorPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	DescriptorSetUpdateBuilder updateBuilder;
	const auto pvdBufferInfo = makeDescriptorBufferInfo(pvdData.get(), 0ull, pvdSize);
	const auto ppdBufferInfo = makeDescriptorBufferInfo(ppdData.get(), 0ull, ppdSize);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pvdBufferInfo);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ppdBufferInfo);
	updateBuilder.update(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Shader modules.
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));

	Move<VkShaderModule> taskShader;
	if (hasTask)
		taskShader = createShaderModule(vkd, device, binaries.get("task"));

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, imageFormat);

	// Framebuffer.
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

	// Viewport and scissor.
	const auto						topHalf		= makeViewport(imageExtent.width, imageExtent.height / 2u);
	const std::vector<VkViewport>	viewports	{ makeViewport(imageExtent), topHalf };
	const std::vector<VkRect2D>		scissors	(2u, makeRect2D(imageExtent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskShader.get(), meshShader.get(), fragShader.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Run pipeline.
	const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 0.0f);
	const auto		drawCount	= m_params->drawCount();
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDrawMeshTasksNV(cmdBuffer, drawCount, 0u);
	endRenderPass(vkd, cmdBuffer);

	// Copy color buffer to verification buffer.
	const auto colorAccess		= (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	const auto transferRead		= VK_ACCESS_TRANSFER_READ_BIT;
	const auto transferWrite	= VK_ACCESS_TRANSFER_WRITE_BIT;
	const auto hostRead			= VK_ACCESS_HOST_READ_BIT;

	const auto preCopyBarrier	= makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
	const auto postCopyBarrier	= makeMemoryBarrier(transferWrite, hostRead);
	const auto copyRegion		= makeBufferImageCopy(imageExtent, colorSRL);

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &postCopyBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Generate reference image and compare results.
	const tcu::IVec3					iExtent				(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
	const tcu::ConstPixelBufferAccess	verificationAccess	(tcuFormat, iExtent, verificationBufferData);

	generateReferenceLevel();
	invalidateAlloc(vkd, device, verificationBufferAlloc);
	if (!verifyResult(verificationAccess))
		TCU_FAIL("Result does not match reference; check log for details");

	return tcu::TestStatus::pass("Pass");
}

}

tcu::TestCaseGroup* createMeshShaderMiscTests (tcu::TestContext& testCtx)
{
	GroupPtr miscTests (new tcu::TestCaseGroup(testCtx, "misc", "Mesh Shader Misc Tests"));

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::just(2u),
			/*meshCount*/	2u,
			/*width*/		8u,
			/*height*/		8u));

		miscTests->addChild(new ComplexTaskDataCase(testCtx, "complex_task_data", "Pass a complex structure from the task to the mesh shader", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	1u,
			/*width*/		5u,		// Use an odd value so there's a pixel in the exact center.
			/*height*/		7u));	// Idem.

		miscTests->addChild(new SinglePointCase(testCtx, "single_point", "Draw a single point", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	1u,
			/*width*/		8u,
			/*height*/		5u));	// Use an odd value so there's a center line.

		miscTests->addChild(new SingleLineCase(testCtx, "single_line", "Draw a single line", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	1u,
			/*width*/		5u,		// Use an odd value so there's a pixel in the exact center.
			/*height*/		7u));	// Idem.

		miscTests->addChild(new SingleTriangleCase(testCtx, "single_triangle", "Draw a single triangle", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	1u,
			/*width*/		16u,
			/*height*/		16u));

		miscTests->addChild(new MaxPointsCase(testCtx, "max_points", "Draw the maximum number of points", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	1u,
			/*width*/		1u,
			/*height*/		1020u));

		miscTests->addChild(new MaxLinesCase(testCtx, "max_lines", "Draw the maximum number of lines", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	1u,
			/*width*/		512u,
			/*height*/		512u));

		miscTests->addChild(new MaxTrianglesCase(testCtx, "max_triangles", "Draw the maximum number of triangles", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::just(65535u),
			/*meshCount*/	1u,
			/*width*/		1360u,
			/*height*/		1542u));

		miscTests->addChild(new LargeWorkGroupCase(testCtx, "many_task_work_groups", "Generate a large number of task work groups", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	65535u,
			/*width*/		1360u,
			/*height*/		1542u));

		miscTests->addChild(new LargeWorkGroupCase(testCtx, "many_mesh_work_groups", "Generate a large number of mesh work groups", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::just(512u),
			/*meshCount*/	512u,
			/*width*/		4096u,
			/*height*/		2048u));

		miscTests->addChild(new LargeWorkGroupCase(testCtx, "many_task_mesh_work_groups", "Generate a large number of task and mesh work groups", std::move(paramsPtr)));
	}

	{
		const PrimitiveType types[] = {
			PrimitiveType::POINTS,
			PrimitiveType::LINES,
			PrimitiveType::TRIANGLES,
		};

		for (int i = 0; i < 2; ++i)
		{
			const bool extraWrites = (i > 0);

			for (const auto primType : types)
			{
				std::unique_ptr<NoPrimitivesParams> params (new NoPrimitivesParams(
				/*taskCount*/		(extraWrites ? tcu::just(1u) : tcu::Nothing),
				/*meshCount*/		1u,
				/*width*/			16u,
				/*height*/			16u,
				/*primitiveType*/	primType));

				ParamsPtr			paramsPtr	(params.release());
				const auto			primName	= primitiveTypeName(primType);
				const std::string	name		= "no_" + primName + (extraWrites ? "_extra_writes" : "");
				const std::string	desc		= "Run a pipeline that generates no " + primName + (extraWrites ? " but generates primitive data" : "");

				miscTests->addChild(extraWrites
					? (new NoPrimitivesExtraWritesCase(testCtx, name, desc, std::move(paramsPtr)))
					: (new NoPrimitivesCase(testCtx, name, desc, std::move(paramsPtr))));
			}
		}
	}

	{
		for (int i = 0; i < 2; ++i)
		{
			const bool useTaskShader = (i == 0);

			ParamsPtr paramsPtr (new MiscTestParams(
				/*taskCount*/	(useTaskShader ? tcu::just(1u) : tcu::Nothing),
				/*meshCount*/	1u,
				/*width*/		1u,
				/*height*/		1u));

			const std::string shader	= (useTaskShader ? "task" : "mesh");
			const std::string name		= "barrier_in_" + shader;
			const std::string desc		= "Use a control barrier in the " + shader + " shader";

			miscTests->addChild(new SimpleBarrierCase(testCtx, name, desc, std::move(paramsPtr)));
		}
	}

	{
		const struct
		{
			MemoryBarrierType	memBarrierType;
			std::string			caseName;
		} barrierTypes[] =
		{
			{ MemoryBarrierType::SHARED,	"memory_barrier_shared"	},
			{ MemoryBarrierType::GROUP,		"group_memory_barrier"	},
		};

		for (const auto& barrierCase : barrierTypes)
		{
			for (int i = 0; i < 2; ++i)
			{
				const bool useTaskShader = (i == 0);

				std::unique_ptr<MemoryBarrierParams> paramsPtr (new MemoryBarrierParams(
					/*taskCount*/		(useTaskShader ? tcu::just(1u) : tcu::Nothing),
					/*meshCount*/		1u,
					/*width*/			1u,
					/*height*/			1u,
					/*memBarrierType*/	barrierCase.memBarrierType));

				const std::string shader	= (useTaskShader ? "task" : "mesh");
				const std::string name		= barrierCase.caseName + "_in_" + shader;
				const std::string desc		= "Use " + paramsPtr->glslFunc() + "() in the " + shader + " shader";

				miscTests->addChild(new MemoryBarrierCase(testCtx, name, desc, std::move(paramsPtr)));
			}
		}
	}

	{
		for (int i = 0; i < 2; ++i)
		{
			const bool useTaskShader	= (i > 0);
			const auto name				= std::string("custom_attributes") + (useTaskShader ? "_and_task_shader" : "");
			const auto desc				= std::string("Use several custom vertex and primitive attributes") + (useTaskShader ? " and also a task shader" : "");

			ParamsPtr paramsPtr (new MiscTestParams(
				/*taskCount*/	(useTaskShader ? tcu::just(1u) : tcu::Nothing),
				/*meshCount*/	1u,
				/*width*/		32u,
				/*height*/		32u));

			miscTests->addChild(new CustomAttributesCase(testCtx, name, desc, std::move(paramsPtr)));
		}
	}

	{
		for (int i = 0; i < 2; ++i)
		{
			const bool useTaskShader	= (i > 0);
			const auto name				= std::string("push_constant") + (useTaskShader ? "_and_task_shader" : "");
			const auto desc				= std::string("Use push constants in the mesh shader stage") + (useTaskShader ? " and also in the task shader stage" : "");

			ParamsPtr paramsPtr (new MiscTestParams(
				/*taskCount*/	(useTaskShader ? tcu::just(1u) : tcu::Nothing),
				/*meshCount*/	1u,
				/*width*/		16u,
				/*height*/		16u));

			miscTests->addChild(new PushConstantCase(testCtx, name, desc, std::move(paramsPtr)));
		}
	}

	{
		ParamsPtr paramsPtr (new MaximizeThreadsParams(
			/*taskCount*/		tcu::Nothing,
			/*meshCount*/		1u,
			/*width*/			128u,
			/*height*/			1u,
			/*localSize*/		32u,
			/*numVertices*/		128u,
			/*numPrimitives*/	256u));

		miscTests->addChild(new MaximizePrimitivesCase(testCtx, "maximize_primitives", "Use a large number of primitives compared to other sizes", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MaximizeThreadsParams(
			/*taskCount*/		tcu::Nothing,
			/*meshCount*/		1u,
			/*width*/			64u,
			/*height*/			1u,
			/*localSize*/		32u,
			/*numVertices*/		256u,
			/*numPrimitives*/	128u));

		miscTests->addChild(new MaximizeVerticesCase(testCtx, "maximize_vertices", "Use a large number of vertices compared to other sizes", std::move(paramsPtr)));
	}

	{
		const uint32_t kInvocationCases[] = { 32u, 64u, 128u, 256u };

		for (const auto& invocationCase : kInvocationCases)
		{
			const auto invsStr		= std::to_string(invocationCase);
			const auto numPixels	= invocationCase / 2u;

			ParamsPtr paramsPtr (new MaximizeThreadsParams(
				/*taskCount*/		tcu::Nothing,
				/*meshCount*/		1u,
				/*width*/			numPixels,
				/*height*/			1u,
				/*localSize*/		invocationCase,
				/*numVertices*/		numPixels,
				/*numPrimitives*/	numPixels));

			miscTests->addChild(new MaximizeInvocationsCase(testCtx, "maximize_invocations_" + invsStr, "Use a large number of invocations compared to other sizes: " + invsStr, std::move(paramsPtr)));
		}
	}

	return miscTests.release();
}

tcu::TestCaseGroup* createMeshShaderInOutTests (tcu::TestContext& testCtx)
{
	GroupPtr inOutTests (new tcu::TestCaseGroup(testCtx, "in_out", "Mesh Shader Tests checking Input/Output interfaces"));

	const struct
	{
		bool i64; bool f64; bool i16; bool f16;
		const char* name;
	} requiredFeatures[] =
	{
		// Restrict the number of combinations to avoid creating too many tests.
		//	i64		f64		i16		f16		name
		{	false,	false,	false,	false,	"32_bits_only"		},
		{	true,	false,	false,	false,	"with_i64"			},
		{	false,	true,	false,	false,	"with_f64"			},
		{	true,	true,	false,	false,	"all_but_16_bits"	},
		{	false,	false,	true,	false,	"with_i16"			},
		{	false,	false,	false,	true,	"with_f16"			},
		{	true,	true,	true,	true,	"all_types"			},
	};

	Owner			ownerCases[]			= { Owner::VERTEX, Owner::PRIMITIVE };
	DataType		dataTypeCases[]			= { DataType::FLOAT, DataType::INTEGER };
	BitWidth		bitWidthCases[]			= { BitWidth::B64, BitWidth::B32, BitWidth::B16 };
	DataDim			dataDimCases[]			= { DataDim::SCALAR, DataDim::VEC2, DataDim::VEC3, DataDim::VEC4 };
	Interpolation	interpolationCases[]	= { Interpolation::NORMAL, Interpolation::FLAT };
	de::Random		rnd(1636723398u);

	for (const auto& reqs : requiredFeatures)
	{
		GroupPtr reqsGroup (new tcu::TestCaseGroup(testCtx, reqs.name, ""));

		// Generate the variable list according to the group requirements.
		IfaceVarVecPtr varsPtr(new IfaceVarVec);

		for (const auto& ownerCase : ownerCases)
		for (const auto& dataTypeCase : dataTypeCases)
		for (const auto& bitWidthCase : bitWidthCases)
		for (const auto& dataDimCase : dataDimCases)
		for (const auto& interpolationCase : interpolationCases)
		{
			if (dataTypeCase == DataType::FLOAT)
			{
				if (bitWidthCase == BitWidth::B64 && !reqs.f64)
					continue;
				if (bitWidthCase == BitWidth::B16 && !reqs.f16)
					continue;
			}
			else if (dataTypeCase == DataType::INTEGER)
			{
				if (bitWidthCase == BitWidth::B64 && !reqs.i64)
					continue;
				if (bitWidthCase == BitWidth::B16 && !reqs.i16)
					continue;
			}

			if (dataTypeCase == DataType::INTEGER && interpolationCase == Interpolation::NORMAL)
				continue;

			if (ownerCase == Owner::PRIMITIVE && interpolationCase == Interpolation::NORMAL)
				continue;

			if (dataTypeCase == DataType::FLOAT && bitWidthCase == BitWidth::B64 && interpolationCase == Interpolation::NORMAL)
				continue;

			for (uint32_t idx = 0u; idx < IfaceVar::kVarsPerType; ++idx)
				varsPtr->push_back(IfaceVar(ownerCase, dataTypeCase, bitWidthCase, dataDimCase, interpolationCase, idx));
		}

		// Generating all permutations of the variables above would mean millions of tests, so we just generate some pseudorandom permutations.
		constexpr uint32_t kPermutations = 40u;
		for (uint32_t combIdx = 0; combIdx < kPermutations; ++combIdx)
		{
			const auto caseName = "permutation_" + std::to_string(combIdx);
			GroupPtr rndGroup(new tcu::TestCaseGroup(testCtx, caseName.c_str(), ""));

			// Duplicate and shuffle vector.
			IfaceVarVecPtr permutVec (new IfaceVarVec(*varsPtr));
			rnd.shuffle(begin(*permutVec), end(*permutVec));

			// Cut the vector short to the usable number of locations.
			{
				uint32_t	usedLocations	= 0u;
				size_t		vectorEnd		= 0u;
				auto&		varVec			= *permutVec;

				for (size_t i = 0; i < varVec.size(); ++i)
				{
					vectorEnd = i;
					const auto varSize = varVec[i].getLocationSize();
					if (usedLocations + varSize > InterfaceVariablesCase::kMaxLocations)
						break;
					usedLocations += varSize;
				}

				varVec.resize(vectorEnd);
			}

			for (int i = 0; i < 2; ++i)
			{
				const bool useTaskShader	= (i > 0);
				const auto name				= (useTaskShader ? "task_mesh" : "mesh_only");

				// Duplicate vector for this particular case so both variants have the same shuffle.
				IfaceVarVecPtr paramsVec(new IfaceVarVec(*permutVec));

				ParamsPtr paramsPtr (new InterfaceVariableParams(
					/*taskCount*/	(useTaskShader ? tcu::just(1u) : tcu::Nothing),
					/*meshCount*/	1u,
					/*width*/		8u,
					/*height*/		8u,
					/*useInt64*/	reqs.i64,
					/*useFloat64*/	reqs.f64,
					/*useInt16*/	reqs.i16,
					/*useFloat16*/	reqs.f16,
					/*vars*/		std::move(paramsVec)));

				rndGroup->addChild(new InterfaceVariablesCase(testCtx, name, "", std::move(paramsPtr)));
			}

			reqsGroup->addChild(rndGroup.release());
		}

		inOutTests->addChild(reqsGroup.release());
	}

	return inOutTests.release();
}

} // MeshShader
} // vkt
