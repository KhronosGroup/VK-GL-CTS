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
 * \brief Mesh Shader Misc Tests for VK_EXT_mesh_shader
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderMiscTests.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"

#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuDefs.hpp"
#include "tcuVectorType.hpp"
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
#include <type_traits>
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
	checkTaskMeshShaderSupportEXT(context, requireTaskShader, true);

	if (requireVertexStores)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
	}
}

struct MiscTestParams
{
	tcu::Maybe<tcu::UVec3>	taskCount;
	tcu::UVec3				meshCount;

	uint32_t				width;
	uint32_t				height;

	MiscTestParams (const tcu::Maybe<tcu::UVec3>& taskCount_, const tcu::UVec3& meshCount_, uint32_t width_, uint32_t height_)
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

	tcu::UVec3 drawCount () const
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
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	std::string frag =
		"#version 450\n"
		"#extension GL_EXT_mesh_shader : enable\n"
		"\n"
		"layout (location=0) in perprimitiveEXT vec4 primitiveColor;\n"
		"layout (location=0) out vec4 outColor;\n"
		"\n"
		"void main ()\n"
		"{\n"
		"    outColor = primitiveColor;\n"
		"}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag) << buildOptions;
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
	const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 0.0f);
	const auto		drawCount	= m_params->drawCount();
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCount.x(), drawCount.y(), drawCount.z());
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
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	// Add the generic fragment shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	const std::string taskDataDecl =
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
		"struct TaskData {\n"
		"    uint yes;\n"
		"    ExternalData externalData;\n"
		"};\n"
		"taskPayloadSharedEXT TaskData td;\n"
		;

	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "\n"
			<< taskDataDecl
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
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
			<< "    EmitMeshTasksEXT(2u, 1u, 1u);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
	}

	{
		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=2) in;\n"
			<< "layout(triangles) out;\n"
			<< "layout(max_vertices=4, max_primitives=2) out;\n"
			<< "\n"
			<< "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
			<< "\n"
			<< taskDataDecl
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
			<< "    uvec2 vertPrim = uvec2(0u, 0u);\n"
			<< "    if (dataOK) {\n"
			<< "        vertPrim = uvec2(4u, 2u);\n"
			<< "    }\n"
			<< "    SetMeshOutputsEXT(vertPrim.x, vertPrim.y);\n"
			<< "    if (vertPrim.y == 0u) {\n"
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
			<< "    float localInvocationOffsetY = float(gl_LocalInvocationIndex);\n"
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
			<< "    uint baseVertexId = 2*gl_LocalInvocationIndex;\n"
			<< "    gl_MeshVerticesEXT[baseVertexId + 0].gl_Position = left;\n"
			<< "    gl_MeshVerticesEXT[baseVertexId + 1].gl_Position = right;\n"
			<< "\n"
			<< "    // 0,1,2 or 1,2,3 (note: triangles alternate front face this way)\n"
			<< "    const uvec3 indices = uvec3(0 + gl_LocalInvocationIndex, 1 + gl_LocalInvocationIndex, 2 + gl_LocalInvocationIndex);\n"
			<< "    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex] = indices;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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
					SinglePointCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params, bool writePointSize = true)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
						, m_writePointSize(writePointSize)
					{}

	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

protected:
	const bool		m_writePointSize = true;
};

class SinglePointInstance : public MeshShaderMiscInstance
{
public:
	SinglePointInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

void SinglePointCase::checkSupport (Context& context) const
{
	MeshShaderMiscCase::checkSupport(context);

	if (!m_writePointSize)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

TestInstance* SinglePointCase::createInstance (Context& context) const
{
	return new SinglePointInstance (context, m_params.get());
}

void SinglePointCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_params->needsTaskShader());

	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	MeshShaderMiscCase::initPrograms(programCollection);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(points) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 pointColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(1u, 1u);\n"
		<< "    pointColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n";
	if (m_writePointSize)
	{
	mesh
		<< "    gl_MeshVerticesEXT[0].gl_PointSize = 1.0f;\n";
	}
	mesh
		<< "    gl_PrimitivePointIndicesEXT[0] = 0;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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

	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	MeshShaderMiscCase::initPrograms(programCollection);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(lines) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 lineColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(2u, 1u);\n"
		<< "    lineColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0f, 0.0f, 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4( 1.0f, 0.0f, 0.0f, 1.0f);\n"
		<< "    gl_PrimitiveLineIndicesEXT[gl_LocalInvocationIndex] = uvec2(0u, 1u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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

	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	MeshShaderMiscCase::initPrograms(programCollection);

	const float halfPixelX = 2.0f / static_cast<float>(m_params->width);
	const float halfPixelY = 2.0f / static_cast<float>(m_params->height);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(3u, 1u);\n"
		<< "    triangleColor[0] = vec4(0.0f, 1.0f, 1.0f, 1.0f);\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(" <<  halfPixelY << ", " << -halfPixelX << ", 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4(" <<  halfPixelY << ", " <<  halfPixelX << ", 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = vec4(" << -halfPixelY << ", 0.0f, 0.0f, 1.0f);\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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

	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	MeshShaderMiscCase::initPrograms(programCollection);

	// Fill a 16x16 image with 256 points. Each of the 64 local invocations will handle a segment of 4 pixels. 4 segments per row.
	DE_ASSERT(m_params->width == 16u && m_params->height == 16u);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=8, local_size_y=2, local_size_z=4) in;\n"
		<< "layout(points) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 pointColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(256u, 256u);\n"
		<< "    uint firstPixel = 4u * gl_LocalInvocationIndex;\n"
		<< "    uint row = firstPixel / 16u;\n"
		<< "    uint col = firstPixel % 16u;\n"
		<< "    float pixSize = 2.0f / 16.0f;\n"
		<< "    float yCoord = pixSize * (float(row) + 0.5f) - 1.0f;\n"
		<< "    float baseXCoord = pixSize * (float(col) + 0.5f) - 1.0f;\n"
		<< "    for (uint i = 0; i < 4u; i++) {\n"
		<< "        float xCoord = baseXCoord + pixSize * float(i);\n"
		<< "        uint pixId = firstPixel + i;\n"
		<< "        gl_MeshVerticesEXT[pixId].gl_Position = vec4(xCoord, yCoord, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesEXT[pixId].gl_PointSize = 1.0f;\n"
		<< "        gl_PrimitivePointIndicesEXT[pixId] = pixId;\n"
		<< "        pointColor[pixId] = vec4(((xCoord + 1.0f) / 2.0f), ((yCoord + 1.0f) / 2.0f), 0.0f, 1.0f);\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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

	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	MeshShaderMiscCase::initPrograms(programCollection);

	// Fill a 1x1020 image with 255 lines, each line being 4 pixels tall. Each invocation will generate ~4 lines.
	DE_ASSERT(m_params->width == 1u && m_params->height == 1020u);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=4, local_size_y=2, local_size_z=8) in;\n"
		<< "layout(lines) out;\n"
		<< "layout(max_vertices=256, max_primitives=255) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 lineColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(256u, 255u);\n"
		<< "    uint firstLine = 4u * gl_LocalInvocationIndex;\n"
		<< "    for (uint i = 0u; i < 4u; i++) {\n"
		<< "        uint lineId = firstLine + i;\n"
		<< "        uint topPixel = 4u * lineId;\n"
		<< "        uint bottomPixel = 3u + topPixel;\n"
		<< "        if (bottomPixel < 1020u) {\n"
		<< "            float bottomCoord = ((float(bottomPixel) + 1.0f) / 1020.0) * 2.0 - 1.0;\n"
		<< "            gl_MeshVerticesEXT[lineId + 1u].gl_Position = vec4(0.0, bottomCoord, 0.0f, 1.0f);\n"
		<< "            gl_PrimitiveLineIndicesEXT[lineId] = uvec2(lineId, lineId + 1u);\n"
		<< "            lineColor[lineId] = vec4(0.0f, 1.0f, float(lineId) / 255.0f, 1.0f);\n"
		<< "        } else {\n"
		<< "            // The last iteration of the last invocation emits the first point\n"
		<< "            gl_MeshVerticesEXT[0].gl_Position = vec4(0.0, -1.0, 0.0f, 1.0f);\n"
		<< "        }\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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
	struct Params : public MiscTestParams
	{
		tcu::UVec3 localSize;

		Params (const tcu::UVec3& meshCount_, uint32_t width_, uint32_t height_, const tcu::UVec3& localSize_)
			: MiscTestParams	(tcu::Nothing, meshCount_, width_, height_)
			, localSize			(localSize_)
			{}
	};

					MaxTrianglesCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

	static constexpr uint32_t kNumVertices	= 256u;
	static constexpr uint32_t kNumTriangles	= 254u;
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
	// Default frag shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	const auto buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto params		= dynamic_cast<const MaxTrianglesCase::Params*>(m_params.get());

	DE_ASSERT(params);
	DE_ASSERT(!params->needsTaskShader());

	const auto&	localSize		= params->localSize;
	const auto	workGroupSize	= (localSize.x() * localSize.y() * localSize.z());

	DE_ASSERT(kNumVertices % workGroupSize == 0u);
	const auto trianglesPerInvocation = kNumVertices / workGroupSize;

	// Fill a sufficiently large image with solid color. Generate a quarter of a circle with the center in the top left corner,
	// using a triangle fan that advances from top to bottom. Each invocation will generate ~trianglesPerInvocation triangles.
	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=" << localSize.x() << ", local_size_y=" << localSize.y() << ", local_size_z=" << localSize.z() << ") in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=" << kNumVertices << ", max_primitives=" << kNumTriangles << ") out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
		<< "\n"
		<< "const float PI_2 = 1.57079632679489661923;\n"
		<< "const float RADIUS = 4.5;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    const uint trianglesPerInvocation = " << trianglesPerInvocation << "u;\n"
		<< "    const uint numVertices = " << kNumVertices << "u;\n"
		<< "    const uint numTriangles = " << kNumTriangles << "u;\n"
		<< "    const float fNumTriangles = float(numTriangles);\n"
		<< "    SetMeshOutputsEXT(numVertices, numTriangles);\n"
		<< "    uint firstTriangle = trianglesPerInvocation * gl_LocalInvocationIndex;\n"
		<< "    for (uint i = 0u; i < trianglesPerInvocation; i++) {\n"
		<< "        uint triangleId = firstTriangle + i;\n"
		<< "        if (triangleId < numTriangles) {\n"
		<< "            uint vertexId = triangleId + 2u;\n"
		<< "            float angleProportion = float(vertexId - 1u) / fNumTriangles;\n"
		<< "            float angle = PI_2 * angleProportion;\n"
		<< "            float xCoord = cos(angle) * RADIUS - 1.0;\n"
		<< "            float yCoord = sin(angle) * RADIUS - 1.0;\n"
		<< "            gl_MeshVerticesEXT[vertexId].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
		<< "            gl_PrimitiveTriangleIndicesEXT[triangleId] = uvec3(0u, triangleId + 1u, triangleId + 2u);\n"
		<< "            triangleColor[triangleId] = vec4(0.0f, 0.0f, 1.0f, 1.0f);\n"
		<< "        } else {\n"
		<< "            // The last iterations of the last invocation emit the first two vertices\n"
		<< "            uint vertexId = triangleId - numTriangles;\n"
		<< "            if (vertexId == 0u) {\n"
		<< "                gl_MeshVerticesEXT[0u].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
		<< "            } else {\n"
		<< "                gl_MeshVerticesEXT[1u].gl_Position = vec4(RADIUS, -1.0, 0.0, 1.0);\n"
		<< "            }\n"
		<< "        }\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

void MaxTrianglesInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

struct LargeWorkGroupParams : public MiscTestParams
{
	LargeWorkGroupParams (const tcu::Maybe<tcu::UVec3>& taskCount_, const tcu::UVec3& meshCount_, uint32_t width_, uint32_t height_, const tcu::UVec3& localInvocations_)
		: MiscTestParams	(taskCount_, meshCount_, width_, height_)
		, localInvocations	(localInvocations_)
	{}

	tcu::UVec3 localInvocations;
};

// Large work groups with many threads.
class LargeWorkGroupCase : public MeshShaderMiscCase
{
public:
					LargeWorkGroupCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
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

// 'x', 'y' or 'z' depending on if dim is 0, 1 or 2, respectively.
char dimSuffix (int dim)
{
	const std::string suffixes = "xyz";
	DE_ASSERT(dim >= 0 && dim < static_cast<int>(suffixes.size()));
	return suffixes[dim];
}

void LargeWorkGroupCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto params		= dynamic_cast<LargeWorkGroupParams*>(m_params.get());
	DE_ASSERT(params);

	const auto	totalInvocations	= params->localInvocations.x() * params->localInvocations.y() * params->localInvocations.z();
	const auto	useTaskShader		= params->needsTaskShader();
	uint32_t	taskMultiplier		= 1u;
	const auto&	meshCount			= params->meshCount;
	const auto	meshMultiplier		= meshCount.x() * meshCount.y() * meshCount.z();

	if (useTaskShader)
	{
		const auto dim	= params->taskCount.get();
		taskMultiplier	= dim.x() * dim.y() * dim.z();
	}

	// Add the frag shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	std::ostringstream taskData;
	taskData
		<< "struct TaskData {\n"
		<< "    uint parentTask[" << totalInvocations << "];\n"
		<< "};\n"
		<< "taskPayloadSharedEXT TaskData td;\n"
		;
	const auto taskDataStr = taskData.str();

	const std::string localSizeStr = "layout ("
		"local_size_x=" + std::to_string(params->localInvocations.x()) + ", "
		"local_size_y=" + std::to_string(params->localInvocations.y()) + ", "
		"local_size_z=" + std::to_string(params->localInvocations.z())
		+ ") in;\n"
		;

	if (useTaskShader)
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< localSizeStr
			<< "\n"
			<< taskDataStr
			<< "\n"
			<< "void main () {\n"
			<< "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z + gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
			<< "    td.parentTask[gl_LocalInvocationIndex] = workGroupIndex;\n"
			<< "    EmitMeshTasksEXT(" << meshCount.x() << ", " << meshCount.y() << ", " << meshCount.z() << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
	}

	// Needed for the code below to work.
	DE_ASSERT(params->width * params->height == taskMultiplier * meshMultiplier * totalInvocations);
	DE_UNREF(taskMultiplier); // For release builds.

	// Emit one point per framebuffer pixel. The number of jobs (params->localInvocations in each mesh shader work group, multiplied
	// by the number of mesh work groups emitted by each task work group) must be the same as the total framebuffer size. Calculate
	// a job ID corresponding to the current mesh shader invocation, and assign a pixel position to it. Draw a point at that
	// position.
	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< localSizeStr
		<< "layout (points) out;\n"
		<< "layout (max_vertices=" << totalInvocations << ", max_primitives=" << totalInvocations << ") out;\n"
		<< "\n"
		<< (useTaskShader ? taskDataStr : "")
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 pointColor[];\n"
		<< "\n"
		<< "void main () {\n"
		<< "    uint parentTask = " << (useTaskShader ? "td.parentTask[0]" : "0") << ";\n";
		;

	if (useTaskShader)
	{
		mesh
			<< "    if (td.parentTask[gl_LocalInvocationIndex] != parentTask || parentTask >= " << taskMultiplier << ") {\n"
			<< "        return;\n"
			<< "    }\n"
			;
	}

	mesh
		<< "    SetMeshOutputsEXT(" << totalInvocations << ", " << totalInvocations << ");\n"
		<< "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z + gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		<< "    uint jobId = ((parentTask * " << meshMultiplier << ") + workGroupIndex) * " << totalInvocations << " + gl_LocalInvocationIndex;\n"
		<< "    uint row = jobId / " << params->width << ";\n"
		<< "    uint col = jobId % " << params->width << ";\n"
		<< "    float yCoord = (float(row + 0.5) / " << params->height << ".0) * 2.0 - 1.0;\n"
		<< "    float xCoord = (float(col + 0.5) / " << params->width << ".0) * 2.0 - 1.0;\n"
		<< "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_PointSize = 1.0;\n"
		<< "    gl_PrimitivePointIndicesEXT[gl_LocalInvocationIndex] = gl_LocalInvocationIndex;\n"
		<< "    vec4 resultColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
		;

	mesh
		<< "    pointColor[gl_LocalInvocationIndex] = resultColor;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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
	NoPrimitivesParams (const tcu::Maybe<tcu::UVec3>& taskCount_, const tcu::UVec3& meshCount_, uint32_t width_, uint32_t height_, PrimitiveType primitiveType_)
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
	const auto buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto params		= dynamic_cast<NoPrimitivesParams*>(m_params.get());

	DE_ASSERT(params);
	DE_ASSERT(!params->needsTaskShader());

	const auto primitiveName = primitiveTypeName(params->primitiveType);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=128) in;\n"
		<< "layout (" << primitiveName << ") out;\n"
		<< "layout (max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 primitiveColor[];\n"
		<< "\n"
		<< "void main () {\n"
		<< "    SetMeshOutputsEXT(0u, 0u);\n"
		<< "}\n"
		;

	MeshShaderMiscCase::initPrograms(programCollection);
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

class NoPrimitivesExtraWritesCase : public NoPrimitivesCase
{
public:
					NoPrimitivesExtraWritesCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: NoPrimitivesCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;

	static constexpr uint32_t kLocalInvocations = 128u;
};

void NoPrimitivesExtraWritesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto params		= dynamic_cast<NoPrimitivesParams*>(m_params.get());

	DE_ASSERT(params);
	DE_ASSERT(m_params->needsTaskShader());

	std::ostringstream taskData;
	taskData
		<< "struct TaskData {\n"
		<< "    uint localInvocations[" << kLocalInvocations << "];\n"
		<< "};\n"
		<< "taskPayloadSharedEXT TaskData td;\n"
		;
	const auto taskDataStr = taskData.str();

	std::ostringstream task;
	task
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
		<< "\n"
		<< taskDataStr
		<< "\n"
		<< "void main () {\n"
		<< "    td.localInvocations[gl_LocalInvocationIndex] = gl_LocalInvocationIndex;\n"
		<< "    EmitMeshTasksEXT(" << params->meshCount.x() << ", " << params->meshCount.y() << ", " << params->meshCount.z() << ");\n"
		<< "}\n"
		;
	programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;

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
									? "        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_PointSize = 1.0;\n"
									: "");

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
		<< "layout (" << primitiveName << ") out;\n"
		<< "layout (max_vertices=" << kLocalInvocations << ", max_primitives=" << maxPrimitives << ") out;\n"
		<< "\n"
		<< taskDataStr
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 primitiveColor[];\n"
		<< "\n"
		<< "shared uint sumOfIds;\n"
		<< "\n"
		<< "const float PI_2 = 1.57079632679489661923;\n"
		<< "const float RADIUS = 1.0f;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    sumOfIds = 0u;\n"
		<< "    memoryBarrierShared();\n"
		<< "    barrier();\n"
		<< "    atomicAdd(sumOfIds, td.localInvocations[gl_LocalInvocationIndex]);\n"
		<< "    memoryBarrierShared();\n"
		<< "    barrier();\n"
		<< "    // This should dynamically give 0\n"
		<< "    uint primitiveCount = sumOfIds - (" << kLocalInvocations * (kLocalInvocations - 1u) / 2u << ");\n"
		<< "    SetMeshOutputsEXT(primitiveCount, primitiveCount);\n"
		<< "\n"
		<< "    // Emit points and primitives to the arrays in any case\n"
		<< "    if (gl_LocalInvocationIndex > 0u) {\n"
		<< "        float proportion = (float(gl_LocalInvocationIndex - 1u) + 0.5f) / float(" << kLocalInvocations << " - 1u);\n"
		<< "        float angle = PI_2 * proportion;\n"
		<< "        float xCoord = cos(angle) * RADIUS - 1.0;\n"
		<< "        float yCoord = sin(angle) * RADIUS - 1.0;\n"
		<< "        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vec4(xCoord, yCoord, 0.0, 1.0);\n"
		<< pointSizeDecl
		<< "    } else {\n"
		<< "        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
		<< pointSizeDecl
		<< "    }\n"
		<< "    uint primitiveId = max(gl_LocalInvocationIndex, " << (maxPrimitives - 1u) << ");\n"
		<< "    primitiveColor[primitiveId] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		;

	if (params->primitiveType == PrimitiveType::POINTS)
		mesh << "    gl_PrimitivePointIndicesEXT[primitiveId] = primitiveId;\n";
	else if (params->primitiveType == PrimitiveType::LINES)
		mesh << "    gl_PrimitiveLineIndicesEXT[primitiveId] = uvec2(primitiveId + 0u, primitiveId + 1u);\n";
	else if (params->primitiveType == PrimitiveType::TRIANGLES)
		mesh << "    gl_PrimitiveTriangleIndicesEXT[primitiveId] = uvec3(0u, primitiveId + 1u, primitiveId + 3u);\n";
	else
		DE_ASSERT(false);

	mesh << "}\n";

	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

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
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	// Generate frag shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	DE_ASSERT(m_params->meshCount == tcu::UVec3(1u, 1u, 1u));
	DE_ASSERT(m_params->width == 1u && m_params->height == 1u);

	const std::string taskOK		= "workGroupSize = uvec3(1u, 1u, 1u);\n";
	const std::string taskFAIL		= "workGroupSize = uvec3(0u, 0u, 0u);\n";

	const std::string meshOK		= "vertPrim = uvec2(1u, 1u);\n";
	const std::string meshFAIL		= "vertPrim = uvec2(0u, 0u);\n";

	const std::string okStatement	= (m_params->needsTaskShader() ? taskOK : meshOK);
	const std::string failStatement	= (m_params->needsTaskShader() ? taskFAIL : meshFAIL);

	const std::string	sharedDecl = "shared uint counter;\n\n";
	std::ostringstream	verification;
	verification
		<< "counter = 0;\n"
		<< "memoryBarrierShared();\n"
		<< "barrier();\n"
		<< "atomicAdd(counter, 1u);\n"
		<< "memoryBarrierShared();\n"
		<< "barrier();\n"
		<< "if (gl_LocalInvocationIndex == 0u) {\n"
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
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=${LOCAL_SIZE}) in;\n"
		<< "layout (points) out;\n"
		<< "layout (max_vertices=1, max_primitives=1) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 primitiveColor[];\n"
		<< "\n"
		<< "${GLOBALS:opt}"
		<< "void main ()\n"
		<< "{\n"
		<< "    uvec2 vertPrim = uvec2(0u, 0u);\n"
		<< "${BODY}"
		<< "    SetMeshOutputsEXT(vertPrim.x, vertPrim.y);\n"
		<< "    if (gl_LocalInvocationIndex == 0u && vertPrim.x > 0u) {\n"
		<< "        gl_MeshVerticesEXT[0].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
		<< "        gl_MeshVerticesEXT[0].gl_PointSize = 1.0;\n"
		<< "        primitiveColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "        gl_PrimitivePointIndicesEXT[0] = 0;\n"
		<< "    }\n"
		<< "}\n"
		;
	const tcu::StringTemplate meshTemplate = meshTemplateStr.str();

	if (m_params->needsTaskShader())
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
			<< "\n"
			<< sharedDecl
			<< "void main ()\n"
			<< "{\n"
			<< "    uvec3 workGroupSize = uvec3(0u, 0u, 0u);\n"
			<< verification.str()
			<< "    EmitMeshTasksEXT(workGroupSize.x, workGroupSize.y, workGroupSize.z);\n"
			<< "}\n"
			;

		std::map<std::string, std::string> replacements;
		replacements["LOCAL_SIZE"]	= "1";
		replacements["BODY"]		= meshOK;

		const auto meshStr = meshTemplate.specialize(replacements);

		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
		programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr) << buildOptions;
	}
	else
	{
		std::map<std::string, std::string> replacements;
		replacements["LOCAL_SIZE"]	= std::to_string(kLocalInvocations);
		replacements["BODY"]		= verification.str();
		replacements["GLOBALS"]		= sharedDecl;

		const auto meshStr = meshTemplate.specialize(replacements);

		programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr) << buildOptions;
	}
}

// Case testing memoryBarrierShared() and groupMemoryBarrier().
enum class MemoryBarrierType { SHARED = 0, GROUP };

struct MemoryBarrierParams : public MiscTestParams
{
	MemoryBarrierParams (const tcu::Maybe<tcu::UVec3>& taskCount_, const tcu::UVec3& meshCount_, uint32_t width_, uint32_t height_, MemoryBarrierType memBarrierType_)
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
	constexpr auto Message		= tcu::TestLog::Message;
	constexpr auto EndMessage	= tcu::TestLog::EndMessage;

	// Clarify what we are checking in the logs; otherwise, they could be confusing.
	auto& log = m_context.getTestContext().getLog();
	const std::vector<tcu::TextureLevel*> levels = { m_referenceLevel.get(), m_referenceLevel2.get() };

	bool good = false;
	for (size_t i = 0; i < levels.size(); ++i)
	{
		log << Message << "Comparing result with reference " << i << "..." << EndMessage;
		const auto success = MeshShaderMiscInstance::verifyResult(resultAccess, *levels[i]);
		if (success)
		{
			log << Message << "Match! The test has passed" << EndMessage;
			good = true;
			break;
		}
	}

	return good;
}

void MemoryBarrierCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto params		= dynamic_cast<MemoryBarrierParams*>(m_params.get());
	DE_ASSERT(params);

	// Generate frag shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	DE_ASSERT(params->meshCount == tcu::UVec3(1u, 1u, 1u));
	DE_ASSERT(params->width == 1u && params->height == 1u);

	const bool taskShader = params->needsTaskShader();

	const std::string	taskDataDecl	= "struct TaskData { float blue; }; taskPayloadSharedEXT TaskData td;\n\n";
	const auto			barrierFunc		= params->glslFunc();

	const std::string taskAction	= "td.blue = float(iterations % 2u);\nworkGroupSize = uvec3(1u, 1u, 1u);\n";
	const std::string meshAction	= "vertPrim = uvec2(1u, 1u);\n";
	const std::string action		= (taskShader ? taskAction : meshAction);

	const std::string	sharedDecl = "shared uint flags[2];\n\n";
	std::ostringstream	verification;
	verification
		<< "flags[gl_LocalInvocationIndex] = 0u;\n"
		<< "barrier();\n"
		<< "flags[gl_LocalInvocationIndex] = 1u;\n"
		<<  barrierFunc << "();\n"
		<< "uint otherInvocation = 1u - gl_LocalInvocationIndex;\n"
		<< "uint iterations = 0u;\n"
		<< "while (flags[otherInvocation] != 1u) {\n"
		<< "    iterations++;\n"
		<< "}\n"
		<< "if (gl_LocalInvocationIndex == 0u) {\n"
		<< "\n"
		<< action
		<< "\n"
		<< "}\n"
		;

	// The mesh shader is very similar in both cases, so we use a template.
	std::ostringstream meshTemplateStr;
	meshTemplateStr
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=${LOCAL_SIZE}) in;\n"
		<< "layout (points) out;\n"
		<< "layout (max_vertices=1, max_primitives=1) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 primitiveColor[];\n"
		<< "\n"
		<< "${GLOBALS}"
		<< "void main ()\n"
		<< "{\n"
		<< "    uvec2 vertPrim = uvec2(0u, 0u);\n"
		<< "${BODY}"
		<< "    SetMeshOutputsEXT(vertPrim.x, vertPrim.y);\n"
		<< "    if (gl_LocalInvocationIndex == 0u && vertPrim.x > 0u) {\n"
		<< "        gl_MeshVerticesEXT[0].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
		<< "        gl_MeshVerticesEXT[0].gl_PointSize = 1.0;\n"
		<< "        primitiveColor[0] = vec4(0.0, 0.0, ${BLUE}, 1.0);\n"
		<< "        gl_PrimitivePointIndicesEXT[0] = 0;\n"
		<< "    }\n"
		<< "}\n"
		;
	const tcu::StringTemplate meshTemplate = meshTemplateStr.str();

	if (params->needsTaskShader())
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
			<< "\n"
			<< sharedDecl
			<< taskDataDecl
			<< "void main ()\n"
			<< "{\n"
			<< "    uvec3 workGroupSize = uvec3(0u, 0u, 0u);\n"
			<< verification.str()
			<< "    EmitMeshTasksEXT(workGroupSize.x, workGroupSize.y, workGroupSize.z);\n"
			<< "}\n"
			;

		std::map<std::string, std::string> replacements;
		replacements["LOCAL_SIZE"]	= "1";
		replacements["BODY"]		= meshAction;
		replacements["GLOBALS"]		= taskDataDecl;
		replacements["BLUE"]		= "td.blue";

		const auto meshStr = meshTemplate.specialize(replacements);

		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
		programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr) << buildOptions;
	}
	else
	{
		std::map<std::string, std::string> replacements;
		replacements["LOCAL_SIZE"]	= std::to_string(kLocalInvocations);
		replacements["BODY"]		= verification.str();
		replacements["GLOBALS"]		= sharedDecl;
		replacements["BLUE"]		= "float(iterations % 2u)";

		const auto meshStr = meshTemplate.specialize(replacements);

		programCollection.glslSources.add("mesh") << glu::MeshSource(meshStr) << buildOptions;
	}
}

// Test the task payload can be read by all invocations in the work group.
class PayloadReadCase : public MeshShaderMiscCase
{
public:
					PayloadReadCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

	static constexpr uint32_t kLocalInvocations = 128u;
};

class PayloadReadInstance : public MeshShaderMiscInstance
{
public:
	PayloadReadInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

TestInstance* PayloadReadCase::createInstance (Context &context) const
{
	return new PayloadReadInstance(context, m_params.get());
}

void PayloadReadCase::initPrograms (vk::SourceCollections &programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	// Add default fragment shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	std::ostringstream taskPayload;
	taskPayload
		<< "struct TaskData {\n"
		<< "    uint verificationCodes[" << kLocalInvocations << "];\n"
		<< "    vec4 color;\n"
		<< "};\n"
		<< "taskPayloadSharedEXT TaskData td;\n"
		;
	const std::string taskPayloadDecl = taskPayload.str();

	DE_ASSERT(m_params->needsTaskShader());

	const auto& meshCount = m_params->meshCount;
	DE_ASSERT(meshCount.x() == 1u && meshCount.y() == 1u && meshCount.z() == 1u);

	const auto kLocalInvocations2 = kLocalInvocations * 2u;

	std::ostringstream task;
	task
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ") in;\n"
		<< "\n"
		<< taskPayloadDecl
		<< "shared uint verificationOK[" << kLocalInvocations << "];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    td.verificationCodes[gl_LocalInvocationIndex] = (" << kLocalInvocations2 << " - gl_LocalInvocationIndex);\n"
		<< "    memoryBarrierShared();\n"
		<< "    barrier();\n"
		// Verify all codes from all invocations.
		<< "    uint verificationResult = 1u;\n"
		<< "    for (uint i = 0u; i < " << kLocalInvocations << "; ++i) {\n"
		<< "        if (td.verificationCodes[i] != (" << kLocalInvocations2 << " - i)) {\n"
		<< "            verificationResult = 0u;\n"
		<< "            break;\n"
		<< "        }\n"
		<< "    }\n"
		<< "    verificationOK[gl_LocalInvocationIndex] = verificationResult;\n"
		<< "    memoryBarrierShared();\n"
		<< "    barrier();\n"
		// Check all verifications were OK (from the first invocation).
		<< "    if (gl_LocalInvocationIndex == 0u) {\n"
		<< "        vec4 color = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "        for (uint i = 0u; i < " << kLocalInvocations << "; ++i) {\n"
		<< "            if (verificationOK[i] == 0u) {\n"
		<< "                color = vec4(0.0, 0.0, 0.0, 1.0);\n"
		<< "            }\n"
		<< "        }\n"
		<< "        td.color = color;\n"
		<< "    }\n"
		<< "    EmitMeshTasksEXT(" << meshCount.x() << ", " << meshCount.y() << ", " << meshCount.z() << ");\n"
		<< "}\n"
		;
	programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=1) in;\n"
		<< "layout (triangles) out;\n"
		<< "layout (max_vertices=3, max_primitives=1) out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 primitiveColor[];\n"
		<< taskPayloadDecl
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		// Verify data one more time from the mesh shader invocation.
		<< "    uint verificationResult = 1u;\n"
		<< "    for (uint i = 0u; i < " << kLocalInvocations << "; ++i) {\n"
		<< "        if (td.verificationCodes[i] != (" << kLocalInvocations2 << " - i)) {\n"
		<< "            verificationResult = 0u;\n"
		<< "            break;\n"
		<< "        }\n"
		<< "    }\n"
		<< "    const vec4 finalColor = ((verificationResult == 0u) ? vec4(0.0, 0.0, 0.0, 1.0) : td.color);\n"
		<< "\n"
		<< "    SetMeshOutputsEXT(3u, 1u);\n"
		<< "\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
		<< "\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
		<< "    primitiveColor[0] = finalColor;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

void PayloadReadInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

// Test with custom per-vertex and per-primitive attributes of different types.
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
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (location=0) in vec4 customAttribute1;\n"
		<< "layout (location=1) in flat float customAttribute2;\n"
		<< "layout (location=2) in flat int customAttribute3;\n"
		<< "\n"
		<< "layout (location=3) in perprimitiveEXT flat uvec4 customAttribute4;\n"
		<< "layout (location=4) in perprimitiveEXT float customAttribute5;\n"
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
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;

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
		<< "struct TaskData {\n"
		<< pvdDataDecl
		<< ppdDataDecl
		<< "};\n"
		<< "taskPayloadSharedEXT TaskData td;\n"
		<< "\n"
		;
	const auto taskDataDecl = taskDataStream.str();

	const auto taskShader = m_params->needsTaskShader();

	const auto meshPvdPrefix = (taskShader ? "td" : "pvd");
	const auto meshPpdPrefix = (taskShader ? "td" : "ppd");

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=1) in;\n"
		<< "layout (max_primitives=2, max_vertices=4) out;\n"
		<< "layout (triangles) out;\n"
		<< "\n"
		<< "out gl_MeshPerVertexEXT {\n"
		<< "    vec4  gl_Position;\n"
		<< "    float gl_PointSize;\n"
		<< "    float gl_ClipDistance[1];\n"
		<< "} gl_MeshVerticesEXT[];\n"
		<< "\n"
		<< "layout (location=0) out vec4 customAttribute1[];\n"
		<< "layout (location=1) out flat float customAttribute2[];\n"
		<< "layout (location=2) out int customAttribute3[];\n"
		<< "\n"
		<< "layout (location=3) out perprimitiveEXT uvec4 customAttribute4[];\n"
		<< "layout (location=4) out perprimitiveEXT float customAttribute5[];\n"
		<< "\n"
		<< "out perprimitiveEXT gl_MeshPerPrimitiveEXT {\n"
		<< "  int gl_PrimitiveID;\n"
		<< "  int gl_ViewportIndex;\n"
		<< "} gl_MeshPrimitivesEXT[];\n"
		<< "\n"
		<< (taskShader ? taskDataDecl : bindingsDecl)
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(4u, 2u);\n"
		<< "\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = " << meshPvdPrefix << ".positions[0]; //vec4(-1.0, -1.0, 0.0, 1.0)\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = " << meshPvdPrefix << ".positions[1]; //vec4( 1.0, -1.0, 0.0, 1.0)\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = " << meshPvdPrefix << ".positions[2]; //vec4(-1.0,  1.0, 0.0, 1.0)\n"
		<< "    gl_MeshVerticesEXT[3].gl_Position = " << meshPvdPrefix << ".positions[3]; //vec4( 1.0,  1.0, 0.0, 1.0)\n"
		<< "\n"
		<< "    gl_MeshVerticesEXT[0].gl_PointSize = " << meshPvdPrefix << ".pointSizes[0]; //1.0\n"
		<< "    gl_MeshVerticesEXT[1].gl_PointSize = " << meshPvdPrefix << ".pointSizes[1]; //1.0\n"
		<< "    gl_MeshVerticesEXT[2].gl_PointSize = " << meshPvdPrefix << ".pointSizes[2]; //1.0\n"
		<< "    gl_MeshVerticesEXT[3].gl_PointSize = " << meshPvdPrefix << ".pointSizes[3]; //1.0\n"
		<< "\n"
		<< "    // Remove geometry on the right side.\n"
		<< "    gl_MeshVerticesEXT[0].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[0]; // 1.0\n"
		<< "    gl_MeshVerticesEXT[1].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[1]; //-1.0\n"
		<< "    gl_MeshVerticesEXT[2].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[2]; // 1.0\n"
		<< "    gl_MeshVerticesEXT[3].gl_ClipDistance[0] = " << meshPvdPrefix << ".clipDistances[3]; //-1.0\n"
		<< "    \n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[1] = uvec3(2, 3, 1);\n"
		<< "\n"
		<< "    gl_MeshPrimitivesEXT[0].gl_PrimitiveID = " << meshPpdPrefix << ".primitiveIds[0]; //1000\n"
		<< "    gl_MeshPrimitivesEXT[1].gl_PrimitiveID = " << meshPpdPrefix << ".primitiveIds[1]; //1001\n"
		<< "\n"
		<< "    gl_MeshPrimitivesEXT[0].gl_ViewportIndex = " << meshPpdPrefix << ".viewportIndices[0]; //1\n"
		<< "    gl_MeshPrimitivesEXT[1].gl_ViewportIndex = " << meshPpdPrefix << ".viewportIndices[1]; //1\n"
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
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

	if (taskShader)
	{
		const auto& meshCount = m_params->meshCount;
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< taskDataDecl
			<< bindingsDecl
			<< "void main ()\n"
			<< "{\n"
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
			<< "\n"
			<< "    EmitMeshTasksEXT(" << meshCount.x() << ", " << meshCount.y() << ", " << meshCount.z() << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
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
	const auto		bufStages	= (hasTask ? VK_SHADER_STAGE_TASK_BIT_EXT : VK_SHADER_STAGE_MESH_BIT_EXT);

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
	const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 0.0f);
	const auto		drawCount	= m_params->drawCount();
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCount.x(), drawCount.y(), drawCount.z());
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
	const auto buildOptions		= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
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
		<< "struct TaskData {\n"
		<< "    float values[2];\n"
		<< "};\n"
		<< "taskPayloadSharedEXT TaskData td;\n"
		<< "\n"
		;
	const auto taskDataDecl = taskDataStream.str();

	if (useTaskShader)
	{
		TemplateMap taskMap;
		taskMap["PCOFFSET"] = std::to_string(2u * sizeof(float));

		const auto& meshCount = m_params->meshCount;
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=1) in;\n"
			<< "\n"
			<< taskDataDecl
			<< pushConstantsTemplate.specialize(taskMap)
			<< "void main ()\n"
			<< "{\n"
			<< "    td.values[0] = pc.values[0];\n"
			<< "    td.values[1] = pc.values[1];\n"
			<< "\n"
			<< "    EmitMeshTasksEXT(" << meshCount.x() << ", " << meshCount.y() << ", " << meshCount.z() << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
	}

	{
		const std::string blue	= (useTaskShader ? "td.values[0] + pc.values[0]" : "pc.values[0] + pc.values[2]");
		const std::string alpha	= (useTaskShader ? "td.values[1] + pc.values[1]" : "pc.values[1] + pc.values[3]");

		TemplateMap meshMap;
		meshMap["PCOFFSET"] = "0";

		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=1) in;\n"
			<< "layout(triangles) out;\n"
			<< "layout(max_vertices=3, max_primitives=1) out;\n"
			<< "\n"
			<< "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
			<< "\n"
			<< pushConstantsTemplate.specialize(meshMap)
			<< (useTaskShader ? taskDataDecl : "")
			<< "void main ()\n"
			<< "{\n"
			<< "    SetMeshOutputsEXT(3u, 1u);\n"
			<< "\n"
			<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
			<< "\n"
			<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
			<< "    triangleColor[0] = vec4(0.0, 0.0, " << blue << ", " << alpha << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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
		pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_MESH_BIT_EXT, 0u, pcHalfSize));
		pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_TASK_BIT_EXT, pcHalfSize, pcHalfSize));
	}
	else
	{
		pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_MESH_BIT_EXT, 0u, pcSize));
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
	const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 0.0f);
	const auto		drawCount	= m_params->drawCount();
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	for (const auto& range : pcRanges)
		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), range.stageFlags, range.offset, range.size, reinterpret_cast<const char*>(pcData.data()) + range.offset);
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCount.x(), drawCount.y(), drawCount.z());
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
	MaximizeThreadsParams	(const tcu::Maybe<tcu::UVec3>& taskCount_, const tcu::UVec3& meshCount_, uint32_t width_, uint32_t height_,
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
		const auto& properties = context.getMeshShaderPropertiesEXT();

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
	const auto buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto params		= dynamic_cast<MaximizeThreadsParams*>(m_params.get());

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
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=" << params->localSize << ") in;\n"
		<< "layout(points) out;\n"
		<< "layout(max_vertices=" << params->numVertices << ", max_primitives=" << params->numPrimitives << ") out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 pointColor[];\n"
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
		<< "    SetMeshOutputsEXT(" << params->numVertices << ", " << params->numPrimitives << ");\n"
		<< "    const uint firstVertex = gl_LocalInvocationIndex * verticesPerInvocation;\n"
		<< "    for (uint i = 0u; i < verticesPerInvocation; ++i)\n"
		<< "    {\n"
		<< "        const uint vertexNumber = firstVertex + i;\n"
		<< "        const float xCoord = ((float(vertexNumber) + 0.5) / " << params->width << ".0) * 2.0 - 1.0;\n"
		<< "        const float yCoord = 0.0;\n"
		<< "        gl_MeshVerticesEXT[vertexNumber].gl_Position = vec4(xCoord, yCoord, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesEXT[vertexNumber].gl_PointSize = 1.0f;\n"
		<< "        for (uint j = 0u; j < primitivesPerVertex; ++j)\n"
		<< "        {\n"
		<< "            const uint primitiveNumber = vertexNumber * primitivesPerVertex + j;\n"
		<< "            gl_PrimitivePointIndicesEXT[primitiveNumber] = vertexNumber;\n"
		<< "            pointColor[primitiveNumber] = colors[j];\n"
		<< "        }\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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
	const auto buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto params		= dynamic_cast<MaximizeThreadsParams*>(m_params.get());

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
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=" << params->localSize << ") in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=" << params->numVertices << ", max_primitives=" << params->numPrimitives << ") out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
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
		<< "    SetMeshOutputsEXT(" << params->numVertices << ", " << params->numPrimitives << ");\n"
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
		<< "        gl_MeshVerticesEXT[firstVertex + 0].gl_Position = vec4(left,  -1.0, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesEXT[firstVertex + 1].gl_Position = vec4(left,   1.0, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesEXT[firstVertex + 2].gl_Position = vec4(right, -1.0, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesEXT[firstVertex + 3].gl_Position = vec4(right,  1.0, 0.0f, 1.0f);\n"
		<< "\n"
		<< "        const uint firstPrimitive = gl_LocalInvocationIndex * primitivesPerInvocation + pixelIdx * primitivesPerPixel;\n"
		<< "        triangleColor[firstPrimitive + 0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "        triangleColor[firstPrimitive + 1] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "\n"
		<< "        const uint firstIndex = gl_LocalInvocationIndex * indicesPerInvocation + pixelIdx * indicesPerPixel;\n"
		<< "        gl_PrimitiveTriangleIndicesEXT[firstPrimitive + 0] = uvec3(firstVertex + 0, firstVertex + 1, firstVertex + 2);\n"
		<< "        gl_PrimitiveTriangleIndicesEXT[firstPrimitive + 1] = uvec3(firstVertex + 1, firstVertex + 3, firstVertex + 2);\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
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
	const auto buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto params		= dynamic_cast<MaximizeThreadsParams*>(m_params.get());

	DE_ASSERT(!params->needsTaskShader());
	MeshShaderMiscCase::initPrograms(programCollection);

	// Idea behind the test: use two invocations to generate one point per framebuffer pixel.
	DE_ASSERT(params->localSize == params->width * 2u);
	DE_ASSERT(params->localSize == params->numPrimitives * 2u);
	DE_ASSERT(params->localSize == params->numVertices * 2u);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=" << params->localSize << ") in;\n"
		<< "layout(points) out;\n"
		<< "layout(max_vertices=" << params->numVertices << ", max_primitives=" << params->numPrimitives << ") out;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveEXT vec4 pointColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(" << params->numVertices << ", " << params->numPrimitives << ");\n"
		<< "    const uint pixelId = gl_LocalInvocationIndex / 2u;\n"
		<< "    if (gl_LocalInvocationIndex % 2u == 0u)\n"
		<< "    {\n"
		<< "        const float xCoord = (float(pixelId) + 0.5) / float(" << params->width << ") * 2.0 - 1.0;\n"
		<< "        gl_MeshVerticesEXT[pixelId].gl_Position = vec4(xCoord, 0.0, 0.0f, 1.0f);\n"
		<< "        gl_MeshVerticesEXT[pixelId].gl_PointSize = 1.0f;\n"
		<< "    }\n"
		<< "    else\n"
		<< "    {\n"
		<< "        gl_PrimitivePointIndicesEXT[pixelId] = pixelId;\n"
		<< "        pointColor[pixelId] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

void MaximizeInvocationsInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

// Verify mixing classic and mesh shading pipelines in the same render pass.
struct MixedPipelinesParams : public MiscTestParams
{
public:
	bool dynamicTopology;

	MixedPipelinesParams (const tcu::Maybe<tcu::UVec3>& taskCount_, const tcu::UVec3& meshCount_, uint32_t width_, uint32_t height_, bool dynamicTopology_)
		: MiscTestParams	(taskCount_, meshCount_, width_, height_)
		, dynamicTopology	(dynamicTopology_)
	{}
};

// Global idea behind this case: draw 4 times with classic, mesh, classic and mesh pipelines. Each draw will use a full screen quad
// and a dynamic scissor to restrict drawing in the framebuffer to one specific quadrant of the color attachment. The color of each
// quadrant will be taken from a push constant that changes between steps, so each quadrant ends up with a different color.
class MixedPipelinesCase : public MeshShaderMiscCase
{
public:
					MixedPipelinesCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class MixedPipelinesInstance : public MeshShaderMiscInstance
{
public:
	MixedPipelinesInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	typedef std::pair<VkRect2D, tcu::Vec4>	RectColor;
	typedef std::vector<RectColor>			RectColorVec;
	RectColorVec	getQuadrantColors		();
	tcu::Vec4		getClearColor			();

	void			generateReferenceLevel	() override;
	tcu::TestStatus	iterate					() override;

};

TestInstance* MixedPipelinesCase::createInstance (Context& context) const
{
	return new MixedPipelinesInstance (context, m_params.get());
}

void MixedPipelinesCase::checkSupport (Context& context) const
{
	const auto params = dynamic_cast<MixedPipelinesParams*>(m_params.get());
	DE_ASSERT(params);

	MeshShaderMiscCase::checkSupport(context);

	if (params->dynamicTopology)
		context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");
}

void MixedPipelinesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	DE_ASSERT(!m_params->needsTaskShader());

	// The fragment shader will draw using the color indicated by the push constant.
	const std::string frag =
		"#version 450\n"
		"\n"
		"layout (location=0) out vec4 outColor;\n"
		"layout (push_constant, std430) uniform PushConstantBlock {\n"
		"    vec4 color;\n"
		"} pc;\n"
		"\n"
		"void main ()\n"
		"{\n"
		"    outColor = pc.color;\n"
		"}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag);

	const std::string vert =
		"#version 450\n"
		"\n"
		"void main()\n"
		"{\n"
		// Full-screen clockwise triangle strip with 4 vertices.
		"    const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
		"    const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
		"    gl_Position = vec4(x, y, 0.0, 1.0);\n"
		"}\n"
		;
	programCollection.glslSources.add("vert") << glu::VertexSource(vert);

	const std::string mesh =
		"#version 450\n"
		"#extension GL_EXT_mesh_shader : enable\n"
		"\n"
		"layout(local_size_x=4) in;\n"
		"layout(triangles) out;\n"
		"layout(max_vertices=4, max_primitives=2) out;\n"
		"\n"
		"void main ()\n"
		"{\n"
		"    SetMeshOutputsEXT(4u, 2u);\n"
		// Full-screen clockwise triangle strip with 4 vertices.
		"    const float x = (-1.0+2.0*((gl_LocalInvocationIndex & 2)>>1));\n"
		"    const float y = ( 1.0-2.0*((gl_LocalInvocationIndex & 1)   ));\n"
		"    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vec4(x, y, 0.0, 1.0);\n"
		"    if (gl_LocalInvocationIndex == 0u) {\n"
		"        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n"
		"        gl_PrimitiveTriangleIndicesEXT[1] = uvec3(2u, 1u, 3u);\n"
		"    }\n"
		"}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh) << buildOptions;
}

MixedPipelinesInstance::RectColorVec MixedPipelinesInstance::getQuadrantColors ()
{
	const auto width		= m_params->width;
	const auto height		= m_params->height;
	const auto halfWidth	= width / 2u;
	const auto halfHeight	= height / 2u;
	const auto iHalfWidth	= static_cast<int>(halfWidth);
	const auto iHalfHeight	= static_cast<int>(halfHeight);

	DE_ASSERT(width % 2u == 0u);
	DE_ASSERT(height % 2u == 0u);

	// Associate a different color to each rectangle.
	const RectColorVec quadrantColors {
		std::make_pair(makeRect2D(0,          0,           halfWidth, halfHeight), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f)),
		std::make_pair(makeRect2D(0,          iHalfHeight, halfWidth, halfHeight), tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f)),
		std::make_pair(makeRect2D(iHalfWidth, 0,           halfWidth, halfHeight), tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f)),
		std::make_pair(makeRect2D(iHalfWidth, iHalfHeight, halfWidth, halfHeight), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)),
	};
	return quadrantColors;
}

tcu::Vec4 MixedPipelinesInstance::getClearColor ()
{
	return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

void MixedPipelinesInstance::generateReferenceLevel ()
{
	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto access		= m_referenceLevel->getAccess();
	const auto quadColors	= getQuadrantColors();
	const auto clearColor	= getClearColor();

	// Each image quadrant gets a different color.
	tcu::clear(access, clearColor);

	for (int y = 0; y < iHeight; ++y)
	for (int x = 0; x < iWidth; ++x)
	{
		for (const auto& quadrant : quadColors)
		{
			const auto minX = quadrant.first.offset.x;
			const auto minY = quadrant.first.offset.y;
			const auto maxX = quadrant.first.offset.x + static_cast<int32_t>(quadrant.first.extent.width);
			const auto maxY = quadrant.first.offset.y + static_cast<int32_t>(quadrant.first.extent.height);

			if (x >= minX && x < maxX && y >= minY && y < maxY)
				access.setPixel(quadrant.second, x, y);
		}
	}
}

tcu::TestStatus MixedPipelinesInstance::iterate ()
{
	const auto params = dynamic_cast<const MixedPipelinesParams*>(m_params);
	DE_ASSERT(params);

	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();

	const auto		dynTopo		= params->dynamicTopology;
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

	// Pipeline layouts for the mesh and classic pipelines.
	const auto pcSize					= static_cast<uint32_t>(sizeof(tcu::Vec4));
	const auto pcRange					= makePushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pcSize);
	const auto classicPipelineLayout	= makePipelineLayout(vkd, device, DE_NULL, &pcRange);
	const auto meshPipelineLayout		= makePipelineLayout(vkd, device, DE_NULL, &pcRange);

	// Shader modules.
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	vertShader	= createShaderModule(vkd, device, binaries.get("vert"));
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));

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

	const std::vector<VkDynamicState>	meshDynamicStates		{ VK_DYNAMIC_STATE_SCISSOR };
	std::vector<VkDynamicState>			classicDynamicStates	(meshDynamicStates);
	if (dynTopo)
		classicDynamicStates.push_back(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT);

	const VkPipelineDynamicStateCreateInfo meshDynamicStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineDynamicStateCreateFlags	flags;
		static_cast<uint32_t>(meshDynamicStates.size()),		//	uint32_t							dynamicStateCount;
		de::dataOrNull(meshDynamicStates),						//	const VkDynamicState*				pDynamicStates;
	};
	const VkPipelineDynamicStateCreateInfo	classicDynamicStateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineDynamicStateCreateFlags	flags;
		static_cast<uint32_t>(classicDynamicStates.size()),		//	uint32_t							dynamicStateCount;
		de::dataOrNull(classicDynamicStates),					//	const VkDynamicState*				pDynamicStates;
	};

	const auto meshPipeline = makeGraphicsPipeline(vkd, device, meshPipelineLayout.get(),
		DE_NULL, meshShader.get(), fragShader.get(),
		renderPass.get(), viewports, scissors, 0u/*subpass*/,
		nullptr, nullptr, nullptr, &colorBlendInfo, &meshDynamicStateInfo);

	const VkPipelineVertexInputStateCreateInfo vertexInputInfo = initVulkanStructure();

	const auto staticTopo		= (dynTopo ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
	const auto classicPipeline	= makeGraphicsPipeline(vkd, device, classicPipelineLayout.get(),
		vertShader.get(), DE_NULL, DE_NULL, DE_NULL, fragShader.get(),
		renderPass.get(), viewports, scissors, staticTopo, 0u/*subpass*/, 0u/*patchControlPoints*/,
			&vertexInputInfo, nullptr, nullptr, nullptr, nullptr, &classicDynamicStateInfo);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Pipeline list.
	beginCommandBuffer(vkd, cmdBuffer);

	// Run pipeline.
	const auto clearColor	= getClearColor();
	const auto drawCount	= m_params->drawCount();
	const auto quadColors	= getQuadrantColors();
	DE_ASSERT(drawCount.x() == 1u && drawCount.y() == 1u && drawCount.z() == 1u);

	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	for (size_t idx = 0u; idx < quadColors.size(); ++idx)
	{
		const auto& rectColor = quadColors.at(idx);
		vkd.cmdSetScissor(cmdBuffer, 0u, 1u, &rectColor.first);

		if (idx % 2u == 0u)
		{
			vkd.cmdPushConstants(cmdBuffer, classicPipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pcSize, &rectColor.second);
			if (dynTopo)
				vkd.cmdSetPrimitiveTopology(cmdBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
			vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, classicPipeline.get());
			vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
		}
		else
		{
			vkd.cmdPushConstants(cmdBuffer, meshPipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pcSize, &rectColor.second);
			vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline.get());
			vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCount.x(), drawCount.y(), drawCount.z());
		}
	}
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

// Tests to check SetMeshOutputsEXT() and EmitMeshTasksEXT() take values from the first invocation.
class FirstInvocationCase : public MeshShaderMiscCase
{
public:
					FirstInvocationCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

	static constexpr uint32_t kColoredPixels = 120u;
};

class FirstInvocationInstance : public MeshShaderMiscInstance
{
public:
	FirstInvocationInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void	generateReferenceLevel	() override;
};

void FirstInvocationInstance::generateReferenceLevel ()
{
	DE_ASSERT(m_params->height == 1u && m_params->width == 128u);
	DE_ASSERT(FirstInvocationCase::kColoredPixels < m_params->width);

	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	const auto iWidth		= static_cast<int>(m_params->width);
	const auto iHeight		= static_cast<int>(m_params->height);

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));

	const auto clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	const auto geomColor	= tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
	const auto access		= m_referenceLevel->getAccess();

	// Fill the expected amount of colored pixels with solid color.
	for (int i = 0; i < iWidth; ++i)
	{
		const auto& color = ((static_cast<uint32_t>(i) < FirstInvocationCase::kColoredPixels) ? geomColor : clearColor);
		access.setPixel(color, i, 0);
	}
}

TestInstance* FirstInvocationCase::createInstance (Context& context) const
{
	return new FirstInvocationInstance(context, m_params.get());
}

void FirstInvocationCase::checkSupport (Context &context) const
{
	MeshShaderMiscCase::checkSupport(context);

	if (context.getUsedApiVersion() < VK_MAKE_VERSION(1, 1, 0))
		TCU_THROW(NotSupportedError, "Vulkan API version >= 1.1 required");

	const auto &subgroupProperties = context.getSubgroupProperties();
	if (!(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT))
		TCU_THROW(NotSupportedError, "Subgroup basic features not supported");
}

void FirstInvocationCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(m_params->height == 1u && m_params->width == 128u);
	DE_ASSERT(kColoredPixels < m_params->width);

	// Add generic fragment shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	const bool					useTask			= m_params->needsTaskShader();
	const auto					fbWidth			= m_params->width;
	const auto					meshLocalSize	= (useTask ? 1u : fbWidth);
	const auto					taskLocalSize	= fbWidth;
	const auto					pointsPerMeshWG	= (useTask ? 1u : kColoredPixels);
	const auto					jobID			= (useTask ? "gl_WorkGroupID.x" : "gl_LocalInvocationIndex");
	const auto					buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	std::string taskDataDecl;
	if (useTask)
	{
		std::ostringstream aux;
		aux
			<< "struct TaskData {\n"
			<< "    uint values[" << taskLocalSize << "];\n"
			<< "};\n"
			<< "taskPayloadSharedEXT TaskData td;\n"
			;
		taskDataDecl = aux.str();
	}

	if (useTask)
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "#extension GL_KHR_shader_subgroup_basic : enable\n"
			<< "\n"
			<< "layout(local_size_x=" << taskLocalSize << ", local_size_y=1, local_size_z=1) in;\n"
			<< "\n"
			<< taskDataDecl
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    td.values[gl_LocalInvocationIndex] = gl_LocalInvocationIndex * 2u;\n"
			<< "\n"
			<< "    uint total_jobs = max(" << kColoredPixels << " / 2u, 1u);\n"
			<< "    if (gl_LocalInvocationIndex == 0u) {\n"
			<< "        total_jobs = " << kColoredPixels << ";\n"
			<< "    } else if (gl_SubgroupID > 0u) {\n"
			<< "        total_jobs = max(" << kColoredPixels << " / 4u, 1u);\n"
			<< "    }\n"
			<< "\n"
			<< "    EmitMeshTasksEXT(total_jobs, 1u, 1u);\n"
			<< "}\n"
			;

		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
	}

	{
		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "#extension GL_KHR_shader_subgroup_basic : enable\n"
			<< "\n"
			<< "layout(local_size_x=" << meshLocalSize << ", local_size_y=1, local_size_z=1) in;\n"
			<< "layout(points) out;\n"
			<< "layout(max_primitives=" << meshLocalSize << ", max_vertices=" << meshLocalSize << ") out;\n"
			<< "\n"
			<< "layout (location=0) out perprimitiveEXT vec4 pointColor[];\n"
			<< taskDataDecl
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    uint total_points = max(" << pointsPerMeshWG << " / 2u, 1u);\n"
			<< "    \n"
			;

		if (!useTask)
		{
			mesh
				<< "    if (gl_LocalInvocationIndex == 0u) {\n"
				<< "        total_points = " << pointsPerMeshWG << ";\n"
				<< "    } else if (gl_SubgroupID > 0u) {\n"
				<< "        total_points = max(" << pointsPerMeshWG << " / 4u, 1u);\n"
				<< "    }\n"
				<< "    \n"
				;
		}

		mesh
			<< "    SetMeshOutputsEXT(total_points, total_points);\n"
			<< "    if (gl_LocalInvocationIndex < " << pointsPerMeshWG << ") {\n"
			<< "        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_PointSize = 1.0;\n"
			<< "        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vec4(((float(" << jobID << ") + 0.5) / " << fbWidth << ") * 2.0 - 1.0, 0.0, 0.0, 1.0);\n"
			<< "        gl_PrimitivePointIndicesEXT[gl_LocalInvocationIndex] = gl_LocalInvocationIndex;\n"
			<< "        pointColor[gl_LocalInvocationIndex] = vec4(0.0, 0.0, 1.0, 1.0);\n"
			<< "    }\n"
			<< "}\n"
			;

		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
	}
}

// Tests that check LocalSizeId works as expected.
class LocalSizeIdCase : public MeshShaderMiscCase
{
public:
					LocalSizeIdCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{}

	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
};

class LocalSizeIdInstance : public MeshShaderMiscInstance
{
public:
	LocalSizeIdInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void			generateReferenceLevel	() override;
	tcu::TestStatus	iterate					() override;
};

TestInstance* LocalSizeIdCase::createInstance (Context& context) const
{
	return new LocalSizeIdInstance(context, m_params.get());
}

void LocalSizeIdInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

void LocalSizeIdCase::checkSupport (Context &context) const
{
	// Generic checks.
	MeshShaderMiscCase::checkSupport(context);

	// Needed for LocalSizeId.
	context.requireDeviceFunctionality("VK_KHR_maintenance4");
}

void LocalSizeIdCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const SpirVAsmBuildOptions	spvOptions	(programCollection.usedVulkanVersion, SPIRV_VERSION_1_5, false/*allowSpirv14*/, true/*allowMaintenance4*/);
	const auto					useTask		= m_params->needsTaskShader();

	DE_ASSERT(m_params->height == 1u && m_params->width == 32u);

	// Add generic fragment shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	if (useTask)
	{
		// Roughly equivalent to the following shader.
		//	#version 450
		//	#extension GL_EXT_mesh_shader : enable
		//
		//	layout(local_size_x_id=10, local_size_y_id=11, local_size_z_id=12) in;
		//	struct TaskData {
		//	    uint pixelID[32];
		//	};
		//	taskPayloadSharedEXT TaskData td;
		//
		//	void main ()
		//	{
		//	    td.pixelID[gl_LocalInvocationIndex] = gl_LocalInvocationIndex;
		//	    EmitMeshTasksEXT(1u, 1u, 1u);
		//	}

		std::ostringstream taskSPV;
		taskSPV
			<< "      ; SPIR-V\n"
			<< "      ; Version: 1.0\n"
			<< "      ; Generator: Khronos Glslang Reference Front End; 10\n"
			<< "      ; Bound: 26\n"
			<< "      ; Schema: 0\n"
			<< "      OpCapability MeshShadingEXT\n"
			<< "      OpExtension \"SPV_EXT_mesh_shader\"\n"
			<< " %1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "      OpMemoryModel Logical GLSL450\n"
			<< "      OpEntryPoint TaskEXT %4 \"main\" %11 %15\n"
			<< "      OpExecutionModeId %4 LocalSizeId %21 %22 %23\n"
			<< "      OpDecorate %15 BuiltIn LocalInvocationIndex\n"
			<< "      OpDecorate %21 SpecId 10\n"
			<< "      OpDecorate %22 SpecId 11\n"
			<< "      OpDecorate %23 SpecId 12\n"
			<< " %2 = OpTypeVoid\n"
			<< " %3 = OpTypeFunction %2\n"
			<< " %6 = OpTypeInt 32 0\n"
			<< " %7 = OpConstant %6 32\n"
			<< " %8 = OpTypeArray %6 %7\n"
			<< " %9 = OpTypeStruct %8\n"
			<< "%10 = OpTypePointer TaskPayloadWorkgroupEXT %9\n"
			<< "%11 = OpVariable %10 TaskPayloadWorkgroupEXT\n"
			<< "%12 = OpTypeInt 32 1\n"
			<< "%13 = OpConstant %12 0\n"
			<< "%14 = OpTypePointer Input %6\n"
			<< "%15 = OpVariable %14 Input\n"
			<< "%18 = OpTypePointer TaskPayloadWorkgroupEXT %6\n"
			<< "%20 = OpConstant %6 1\n"
			<< "%21 = OpSpecConstant %6 1\n"
			<< "%22 = OpSpecConstant %6 1\n"
			<< "%23 = OpSpecConstant %6 1\n"
			<< " %4 = OpFunction %2 None %3\n"
			<< " %5 = OpLabel\n"
			<< "%16 = OpLoad %6 %15\n"
			<< "%17 = OpLoad %6 %15\n"
			<< "%19 = OpAccessChain %18 %11 %13 %16\n"
			<< "      OpStore %19 %17\n"
			<< "      OpEmitMeshTasksEXT %20 %20 %20 %11\n"
			<< "      OpFunctionEnd\n"
			;

		programCollection.spirvAsmSources.add("task") << taskSPV.str() << spvOptions;
	}

	{
		// Roughly equivalent to the following shader.
		//	#version 450
		//	#extension GL_EXT_mesh_shader : enable
		//
		//	layout(local_size_x_id=20, local_size_y_id=21, local_size_z_id=22) in;
		//	layout(points) out;
		//	layout(max_primitives=32, max_vertices=32) out;
		//
		//	layout (location=0) out perprimitiveEXT vec4 pointColor[];
		//#if useTask
		//	struct TaskData {
		//	    uint pixelID[32];
		//	};
		//	taskPayloadSharedEXT TaskData td;
		//#endif
		//
		//	void main ()
		//	{
		//#if useTask
		//	    const uint pixelId = td.pixelID[gl_LocalInvocationIndex];
		//#else
		//	    const uint pixelId = gl_LocalInvocationIndex;
		//#endif
		//	    SetMeshOutputsEXT(32u, 32u);
		//	    gl_MeshVerticesEXT[pixelId].gl_PointSize = 1.0;
		//	    gl_MeshVerticesEXT[pixelId].gl_Position = vec4(((float(pixelId) + 0.5) / 32.0) * 2.0 - 1.0, 0.0, 0.0, 1.0);
		//	    gl_PrimitivePointIndicesEXT[pixelId] = pixelId;
		//	    pointColor[pixelId] = vec4(0.0, 0.0, 1.0, 1.0);
		//	}
		std::ostringstream meshSPV;
		meshSPV
			<< "                              OpCapability MeshShadingEXT\n"
			<< "                              OpExtension \"SPV_EXT_mesh_shader\"\n"
			<< "                         %1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "                              OpMemoryModel Logical GLSL450\n"
			<< "                              OpEntryPoint MeshEXT %main \"main\" %local_invocation_index %mesh_vertices %primitive_point_indices %primitive_colors" << (useTask ? " %task_data" : "") << "\n"
			<< "                              OpExecutionModeId %main LocalSizeId %constand_id_20 %constant_id_21 %constant_id_22\n"
			<< "                              OpExecutionMode %main OutputVertices 32\n"
			<< "                              OpExecutionMode %main OutputPrimitivesNV 32\n"
			<< "                              OpExecutionMode %main OutputPoints\n"
			<< "                              OpDecorate %local_invocation_index BuiltIn LocalInvocationIndex\n"
			<< "                              OpMemberDecorate %mesh_vertices_struct 0 BuiltIn Position\n"
			<< "                              OpMemberDecorate %mesh_vertices_struct 1 BuiltIn PointSize\n"
			<< "                              OpMemberDecorate %mesh_vertices_struct 2 BuiltIn ClipDistance\n"
			<< "                              OpMemberDecorate %mesh_vertices_struct 3 BuiltIn CullDistance\n"
			<< "                              OpDecorate %mesh_vertices_struct Block\n"
			<< "                              OpDecorate %primitive_point_indices BuiltIn PrimitivePointIndicesEXT\n"
			<< "                              OpDecorate %primitive_colors PerPrimitiveEXT\n"
			<< "                              OpDecorate %primitive_colors Location 0\n"
			<< "                              OpDecorate %constand_id_20 SpecId 20\n"
			<< "                              OpDecorate %constant_id_21 SpecId 21\n"
			<< "                              OpDecorate %constant_id_22 SpecId 22\n"
			<< "                 %type_void = OpTypeVoid\n"
			<< "                 %void_func = OpTypeFunction %type_void\n"
			<< "                       %int = OpTypeInt 32 1\n"
			<< "                      %uint = OpTypeInt 32 0\n"
			<< "                     %float = OpTypeFloat 32\n"
			<< "                      %vec4 = OpTypeVector %float 4\n"
			<< "                     %uvec3 = OpTypeVector %uint 3\n"
			<< "                     %int_0 = OpConstant %int 0\n"
			<< "                     %int_1 = OpConstant %int 1\n"
			<< "                    %uint_1 = OpConstant %uint 1\n"
			<< "                   %uint_32 = OpConstant %uint 32\n"
			<< "                   %float_0 = OpConstant %float 0\n"
			<< "                   %float_1 = OpConstant %float 1\n"
			<< "                 %float_0_5 = OpConstant %float 0.5\n"
			<< "                  %float_32 = OpConstant %float 32\n"
			<< "                   %float_2 = OpConstant %float 2\n"
			<< "             %float_array_1 = OpTypeArray %float %uint_1\n"
			<< "             %func_uint_ptr = OpTypePointer Function %uint\n"
			<< "            %input_uint_ptr = OpTypePointer Input %uint\n"
			<< "    %local_invocation_index = OpVariable %input_uint_ptr Input\n"
			<< "      %mesh_vertices_struct = OpTypeStruct %vec4 %float %float_array_1 %float_array_1\n"
			<< "       %mesh_vertices_array = OpTypeArray %mesh_vertices_struct %uint_32\n"
			<< "     %mesh_vertices_out_ptr = OpTypePointer Output %mesh_vertices_array\n"
			<< "             %mesh_vertices = OpVariable %mesh_vertices_out_ptr Output\n"
			<< "          %output_float_ptr = OpTypePointer Output %float\n"
			<< "           %output_vec4_ptr = OpTypePointer Output %vec4\n"
			<< "             %uint_array_32 = OpTypeArray %uint %uint_32\n"
			<< "\n"
			;

		if (useTask)
		{
			meshSPV
				<< "\n"
				<< "%uint_array_32_struct                  = OpTypeStruct %uint_array_32\n"
				<< "%task_payload_uint_array_32_struct_ptr = OpTypePointer TaskPayloadWorkgroupEXT %uint_array_32_struct\n"
				<< "%task_data                             = OpVariable %task_payload_uint_array_32_struct_ptr TaskPayloadWorkgroupEXT\n"
				<< "%task_payload_uint_ptr                 = OpTypePointer TaskPayloadWorkgroupEXT %uint\n"
				<< "\n"
				;
		}

		meshSPV
			<< "  %output_uint_array_32_ptr = OpTypePointer Output %uint_array_32\n"
			<< "   %primitive_point_indices = OpVariable %output_uint_array_32_ptr Output\n"
			<< "           %output_uint_ptr = OpTypePointer Output %uint\n"
			<< "             %vec4_array_32 = OpTypeArray %vec4 %uint_32\n"
			<< "  %output_vec4_array_32_ptr = OpTypePointer Output %vec4_array_32\n"
			<< "          %primitive_colors = OpVariable %output_vec4_array_32_ptr Output\n"
			<< "                      %blue = OpConstantComposite %vec4 %float_0 %float_0 %float_1 %float_1\n"
			<< "            %constand_id_20 = OpSpecConstant %uint 1\n"
			<< "            %constant_id_21 = OpSpecConstant %uint 1\n"
			<< "            %constant_id_22 = OpSpecConstant %uint 1\n"
			<< "                      %main = OpFunction %type_void None %void_func\n"
			<< "                %main_label = OpLabel\n"
			<< "                  %pixel_id = OpVariable %func_uint_ptr Function\n"
			<< "%local_invocation_index_val = OpLoad %uint %local_invocation_index\n"
			;

		if (useTask)
		{
			meshSPV
				<< "           %td_pixel_id_ptr = OpAccessChain %task_payload_uint_ptr %task_data %int_0 %local_invocation_index_val\n"
				<< "           %td_pixel_id_val = OpLoad %uint %td_pixel_id_ptr\n"
				<< "                              OpStore %pixel_id %td_pixel_id_val\n"
				;
		}
		else
		{
			meshSPV << "                              OpStore %pixel_id %local_invocation_index_val\n";
		}

		meshSPV
			<< "                              OpSetMeshOutputsEXT %uint_32 %uint_32\n"
			<< "              %pixel_id_val = OpLoad %uint %pixel_id\n"
			<< "                %point_size = OpAccessChain %output_float_ptr %mesh_vertices %pixel_id_val %int_1\n"
			<< "                              OpStore %point_size %float_1\n"
			<< "        %pixel_id_val_float = OpConvertUToF %float %pixel_id_val\n"
			<< "       %pixel_id_val_center = OpFAdd %float %pixel_id_val_float %float_0_5\n"
			<< "                   %x_unorm = OpFDiv %float %pixel_id_val_center %float_32\n"
			<< "                 %x_unorm_2 = OpFMul %float %x_unorm %float_2\n"
			<< "                    %x_norm = OpFSub %float %x_unorm_2 %float_1\n"
			<< "                 %point_pos = OpCompositeConstruct %vec4 %x_norm %float_0 %float_0 %float_1\n"
			<< "           %gl_position_ptr = OpAccessChain %output_vec4_ptr %mesh_vertices %pixel_id_val %int_0\n"
			<< "                              OpStore %gl_position_ptr %point_pos\n"
			<< "           %point_index_ptr = OpAccessChain %output_uint_ptr %primitive_point_indices %pixel_id_val\n"
			<< "                              OpStore %point_index_ptr %pixel_id_val\n"
			<< "           %point_color_ptr = OpAccessChain %output_vec4_ptr %primitive_colors %pixel_id_val\n"
			<< "                              OpStore %point_color_ptr %blue\n"
			<< "                              OpReturn\n"
			<< "                              OpFunctionEnd\n"
			;

		programCollection.spirvAsmSources.add("mesh") << meshSPV.str() << spvOptions;
	}
}

tcu::TestStatus LocalSizeIdInstance::iterate ()
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

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, 0u, nullptr, 0u, nullptr);

	// Shader modules.
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));

	Move<VkShaderModule> taskShader;
	if (hasTask)
		taskShader = createShaderModule(vkd, device, binaries.get("task"));

	// Spec constant data (must match shaders).
	const std::vector<uint32_t> scData {
		//	10		11		12		20		21		22
			32u,	1u,		1u,		32u,	1u,		1u
	};
	const auto scSize = static_cast<uint32_t>(sizeof(uint32_t));
	const std::vector<VkSpecializationMapEntry> scMapEntries {
		makeSpecializationMapEntry(10u, 0u * scSize, scSize),
		makeSpecializationMapEntry(11u, 1u * scSize, scSize),
		makeSpecializationMapEntry(12u, 2u * scSize, scSize),
		makeSpecializationMapEntry(20u, 3u * scSize, scSize),
		makeSpecializationMapEntry(21u, 4u * scSize, scSize),
		makeSpecializationMapEntry(22u, 5u * scSize, scSize),
	};

	const auto scMapInfo = makeSpecializationInfo(
		static_cast<uint32_t>(scMapEntries.size()), de::dataOrNull(scMapEntries),
		static_cast<uint32_t>(de::dataSize(scData)), de::dataOrNull(scData));

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	shaderStages.push_back(makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_MESH_BIT_EXT, meshShader.get(), &scMapInfo));
	shaderStages.push_back(makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader.get()));
	if (hasTask)
		shaderStages.push_back(makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_TASK_BIT_EXT, taskShader.get(), &scMapInfo));

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, imageFormat);

	// Framebuffer.
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

	// Viewport and scissor.
	const std::vector<VkViewport>	viewports	(1u, makeViewport(imageExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(imageExtent));

	// Pipeline with specialization constants.
	const auto pipeline = makeGraphicsPipeline(vkd, device, DE_NULL, pipelineLayout.get(), 0u, shaderStages, renderPass.get(), viewports, scissors);

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
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCount.x(), drawCount.y(), drawCount.z());
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

// Test multiple task payloads.
class MultipleTaskPayloadsCase : public MeshShaderMiscCase
{
public:
					MultipleTaskPayloadsCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase (testCtx, name, description, std::move(params))
					{
					}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

	static constexpr uint32_t kGoodKeyIdx	= 1u;
};

class MultipleTaskPayloadsInstance : public MeshShaderMiscInstance
{
public:
	MultipleTaskPayloadsInstance (Context& context, const MiscTestParams* params)
		: MeshShaderMiscInstance (context, params)
	{}

	void			generateReferenceLevel	() override;
	tcu::TestStatus	iterate					() override;
};

TestInstance* MultipleTaskPayloadsCase::createInstance (Context& context) const
{
	return new MultipleTaskPayloadsInstance (context, m_params.get());
}

void MultipleTaskPayloadsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(m_params->needsTaskShader());

	const auto					buildOptions	= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const auto					spvBuildOptions	= getMinMeshEXTSpvBuildOptions(programCollection.usedVulkanVersion);
	const std::vector<uint32_t>	keys			{ 3717945376u, 2325956828u, 433982700u };
	//const std::vector<uint32_t> keys { 85u, 170u, 255u };

	// Generic fragment shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	const std::string taskDataDecl =
		"struct TaskData {\n"
		"    uint key;\n"
		"};\n"
		"taskPayloadSharedEXT TaskData td;\n"
		;

	// Idea behind this test: verify that the right payload was passed to the mesh shader and set the geometry color based on that.
	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=3, max_primitives=1) out;\n"
		<< "\n"
		<< "layout(location=0) out perprimitiveEXT vec4 triangleColor[];\n"
		<< taskDataDecl
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(3, 1);\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0f, 1.0f);\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0f, 1.0f);\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
		<< "    const vec4 color = ((td.key == " << keys[kGoodKeyIdx] << "u) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0));\n"
		//<< "    const vec4 color = vec4(0.0, 0.0, (float(td.key) / 255.0), 1.0);\n"
		<< "    triangleColor[0] = color;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

	const auto& meshCount = m_params->meshCount;
	DE_ASSERT(meshCount.x() == 1u && meshCount.y() == 1u && meshCount.z() == 1u);
	DE_UNREF(meshCount); // For release builds.

#if 0
#if 0
	// Note: pseudocode, this actually does not compile with glslang.
	std::ostringstream task;
	task
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(push_constant, std430) uniform PCBlock {\n"
		<< "    uint index;\n"
		<< "} pc;\n"
		<< "struct TaskData {\n"
		<< "    uint key;\n"
		<< "};\n"
		<< "taskPayloadSharedEXT TaskData td0;\n"
		<< "taskPayloadSharedEXT TaskData td1;\n"
		<< "taskPayloadSharedEXT TaskData td2;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    td0.key = " << keys.at(0) << "u;\n"
		<< "    td1.key = " << keys.at(1) << "u;\n"
		<< "    td2.key = " << keys.at(2) << "u;\n"
		<< "    if (pc.index == 0u)      EmitMeshTasksEXT(1u, 1u, 1u, td0);\n"
		<< "    else if (pc.index == 1u) EmitMeshTasksEXT(1u, 1u, 1u, td1);\n"
		<< "    else                     EmitMeshTasksEXT(1u, 1u, 1u, td2);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("task") << glu::TaskSource(task.str());
#else
	// Similar shader to check the setup works.
	std::ostringstream task;
	task
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "layout(push_constant, std430) uniform PCBlock {\n"
		<< "    uint index;\n"
		<< "} pc;\n"
		<< "struct TaskData {\n"
		<< "    uint key;\n"
		<< "};\n"
		<< "taskPayloadSharedEXT TaskData td;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    if (pc.index == 0u)      td.key = " << keys.at(0) << "u;\n"
		<< "    else if (pc.index == 1u) td.key = " << keys.at(1) << "u;\n"
		<< "    else                     td.key = " << keys.at(2) << "u;\n"
		<< "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("task") << glu::TaskSource(task.str());
#endif
#else
	std::ostringstream taskSPV;
	taskSPV
		<< "                    OpCapability MeshShadingEXT\n"
		<< "                    OpExtension \"SPV_EXT_mesh_shader\"\n"
		<< "               %1 = OpExtInstImport \"GLSL.std.450\"\n"
		<< "                    OpMemoryModel Logical GLSL450\n"
		<< "                    OpEntryPoint TaskEXT %main \"main\"\n"
		<< "                    OpExecutionMode %main LocalSize 1 1 1\n"
		<< "                    OpMemberDecorate %PCBlock 0 Offset 0\n"
		<< "                    OpDecorate %PCBlock Block\n"
		<< "                    OpDecorate %work_group_size BuiltIn WorkgroupSize\n"
		<< "               %2 = OpTypeVoid\n"
		<< "               %3 = OpTypeFunction %2\n"
		<< "            %uint = OpTypeInt 32 0\n"
		<< "        %TaskData = OpTypeStruct %uint\n"
		<< "    %TaskData_ptr = OpTypePointer TaskPayloadWorkgroupEXT %TaskData\n"
		<< "       %payload_0 = OpVariable %TaskData_ptr TaskPayloadWorkgroupEXT\n"
		<< "       %payload_1 = OpVariable %TaskData_ptr TaskPayloadWorkgroupEXT\n"
		<< "       %payload_2 = OpVariable %TaskData_ptr TaskPayloadWorkgroupEXT\n"
		<< "             %int = OpTypeInt 32 1\n"
		<< "           %int_0 = OpConstant %int 0\n"
		<< "           %key_0 = OpConstant %uint " << keys.at(0) << "\n"
		<< "           %key_1 = OpConstant %uint " << keys.at(1) << "\n"
		<< "           %key_2 = OpConstant %uint " << keys.at(2) << "\n"
		<< "%payload_uint_ptr = OpTypePointer TaskPayloadWorkgroupEXT %uint\n"
		<< "         %PCBlock = OpTypeStruct %uint\n"
		<< "     %PCBlock_ptr = OpTypePointer PushConstant %PCBlock\n"
		<< "              %pc = OpVariable %PCBlock_ptr PushConstant\n"
		<< "     %pc_uint_ptr = OpTypePointer PushConstant %uint\n"
		<< "          %uint_0 = OpConstant %uint 0\n"
		<< "          %uint_1 = OpConstant %uint 1\n"
		<< "            %bool = OpTypeBool\n"
		<< "           %uvec3 = OpTypeVector %uint 3\n"
		<< " %work_group_size = OpConstantComposite %uvec3 %uint_1 %uint_1 %uint_1\n"
		<< "            %main = OpFunction %2 None %3\n"
		<< "               %5 = OpLabel\n"
		<< "   %payload_0_key = OpAccessChain %payload_uint_ptr %payload_0 %int_0\n"
		<< "   %payload_1_key = OpAccessChain %payload_uint_ptr %payload_1 %int_0\n"
		<< "   %payload_2_key = OpAccessChain %payload_uint_ptr %payload_2 %int_0\n"
		<< "                    OpStore %payload_0_key %key_0\n"
		<< "                    OpStore %payload_1_key %key_1\n"
		<< "                    OpStore %payload_2_key %key_2\n"
		<< "    %pc_index_ptr = OpAccessChain %pc_uint_ptr %pc %int_0\n"
		<< "        %pc_index = OpLoad %uint %pc_index_ptr\n"
		<< "              %23 = OpIEqual %bool %pc_index %uint_0\n"
		<< "                    OpSelectionMerge %25 None\n"
		<< "                    OpBranchConditional %23 %24 %27\n"
		<< "              %24 = OpLabel\n"
		<< "                    OpEmitMeshTasksEXT %uint_1 %uint_1 %uint_1 %payload_0\n"
		<< "                    OpBranch %25\n"
		<< "              %27 = OpLabel\n"
		<< "              %30 = OpIEqual %bool %pc_index %uint_1\n"
		<< "                    OpSelectionMerge %32 None\n"
		<< "                    OpBranchConditional %30 %31 %33\n"
		<< "              %31 = OpLabel\n"
		<< "                    OpEmitMeshTasksEXT %uint_1 %uint_1 %uint_1 %payload_1\n"
		<< "                    OpBranch %32\n"
		<< "              %33 = OpLabel\n"
		<< "                    OpEmitMeshTasksEXT %uint_1 %uint_1 %uint_1 %payload_2\n"
		<< "                    OpBranch %32\n"
		<< "              %32 = OpLabel\n"
		<< "                    OpBranch %25\n"
		<< "              %25 = OpLabel\n"
		<< "                    OpReturn\n"
		<< "                    OpFunctionEnd\n"
		;
	programCollection.spirvAsmSources.add("task") << taskSPV.str() << spvBuildOptions;
#endif
}

void MultipleTaskPayloadsInstance::generateReferenceLevel ()
{
	generateSolidRefLevel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), m_referenceLevel);
}

tcu::TestStatus MultipleTaskPayloadsInstance::iterate ()
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
	const auto pcSize			= static_cast<uint32_t>(sizeof(uint32_t));
	const auto pcRange			= makePushConstantRange(VK_SHADER_STAGE_TASK_BIT_EXT, 0u, pcSize);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, DE_NULL, &pcRange);

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
	const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 0.0f);
	const auto		drawCount	= m_params->drawCount();
	const uint32_t	pcData		= MultipleTaskPayloadsCase::kGoodKeyIdx;
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_TASK_BIT_EXT, 0u, pcSize, &pcData);
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCount.x(), drawCount.y(), drawCount.z());
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

// Test multiple task/mesh draw calls and updating push constants and descriptors in between. We will divide the output image in 4
// quadrants, and use each task/mesh draw call to draw on a particular quadrant. The output color in each quadrant will be composed
// of data from different sources: storage buffer, sampled image or push constant value, and those will change before each draw
// call. We'll prepare different descriptors for each quadrant.
class RebindSetsCase : public MeshShaderMiscCase
{
public:
					RebindSetsCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr params)
						: MeshShaderMiscCase(testCtx, name, description, std::move(params))
						{
							const auto drawCount = m_params->drawCount();
							DE_ASSERT(drawCount.x() == 1u && drawCount.y() == 1u && drawCount.z() == 1u);
							DE_UNREF(drawCount); // For release builds.
						}
	virtual			~RebindSetsCase		(void) {}

	TestInstance*	createInstance		(Context& context) const override;
	void			checkSupport		(Context& context) const override;
	void			initPrograms		(vk::SourceCollections& programCollection) const override;
};

class RebindSetsInstance : public MeshShaderMiscInstance
{
public:
						RebindSetsInstance		(Context& context, const MiscTestParams* params)
							: MeshShaderMiscInstance(context, params) {}
	virtual				~RebindSetsInstance		(void) {}

	void				generateReferenceLevel	() override;
	tcu::TestStatus		iterate					(void) override;

protected:
	struct QuadrantInfo
	{
		// Offsets in framebuffer coordinates (0 to 2, final coordinates in range -1 to 1)
		float		offsetX;
		float		offsetY;
		tcu::Vec4	color;

		QuadrantInfo (float offsetX_, float offsetY_, float red, float green, float blue)
			: offsetX	(offsetX_)
			, offsetY	(offsetY_)
			, color		(red, green, blue, 1.0f)
		{}
	};

	static std::vector<QuadrantInfo> getQuadrantInfos ()
	{
		std::vector<QuadrantInfo> infos;
		infos.reserve(4u);

		//                 offsets     rgb
		infos.emplace_back(0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		infos.emplace_back(1.0f, 0.0f, 1.0f, 1.0f, 0.0f);
		infos.emplace_back(0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
		infos.emplace_back(1.0f, 1.0f, 0.0f, 1.0f, 1.0f);

		return infos;
	}

	struct PushConstants
	{
		float offsetX;
		float offsetY;
		float blueComponent;
	};
};

TestInstance* RebindSetsCase::createInstance (Context &context) const
{
	return new RebindSetsInstance(context, m_params.get());
}

void RebindSetsCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, true, false);
}

void RebindSetsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	// Generic fragment shader.
	MeshShaderMiscCase::initPrograms(programCollection);

	const std::string ssbo		= "layout (set=0, binding=0, std430) readonly buffer SSBOBlock { float redComponent; } ssbo;\n";
	const std::string combined	= "layout (set=0, binding=1) uniform sampler2D greenComponent;\n";
	const std::string pc		= "layout (push_constant, std430) uniform PCBlock { float offsetX; float offsetY; float blueComponent; } pc;\n";
	const std::string payload	= "struct TaskData { float redComponent; }; taskPayloadSharedEXT TaskData td;\n";

	std::ostringstream task;
	task
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "\n"
		<< ssbo
		<< payload
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    td.redComponent = ssbo.redComponent;\n"
		<< "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (triangles) out;\n"
		<< "layout (max_vertices=4, max_primitives=2) out;\n"
		<< "\n"
		<< combined
		<< pc
		<< payload
		<< "layout (location=0) out perprimitiveEXT vec4 primitiveColor[];\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(4u, 2u);\n"
		<< "\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0 + pc.offsetX, -1.0 + pc.offsetY, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4( 0.0 + pc.offsetX, -1.0 + pc.offsetY, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0 + pc.offsetX,  0.0 + pc.offsetY, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[3].gl_Position = vec4( 0.0 + pc.offsetX,  0.0 + pc.offsetY, 0.0, 1.0);\n"
		<< "\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(2u, 1u, 0u);\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[1] = uvec3(2u, 3u, 1u);\n"
		<< "\n"
		<< "    const vec4 primColor = vec4(td.redComponent, texture(greenComponent, vec2(0.5, 0.5)).x, pc.blueComponent, 1.0);\n"
		<< "    primitiveColor[0] = primColor;\n"
		<< "    primitiveColor[1] = primColor;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

void RebindSetsInstance::generateReferenceLevel ()
{
	const auto iWidth	= static_cast<int>(m_params->width);
	const auto iHeight	= static_cast<int>(m_params->height);
	const auto fWidth	= static_cast<float>(iWidth);
	const auto fHeight	= static_cast<float>(iHeight);

	DE_ASSERT(iWidth % 2 == 0);
	DE_ASSERT(iHeight % 2 == 0);

	const auto halfWidth	= iWidth / 2;
	const auto halfHeight	= iHeight / 2;

	const auto format		= getOutputFormat();
	const auto tcuFormat	= mapVkFormat(format);

	m_referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidth, iHeight));
	const auto access = m_referenceLevel->getAccess();

	const auto quadrantInfos = getQuadrantInfos();
	DE_ASSERT(quadrantInfos.size() == 4u);

	for (const auto& quadrantInfo : quadrantInfos)
	{
		const auto xCorner		= static_cast<int>(quadrantInfo.offsetX / 2.0f * fWidth);
		const auto yCorner		= static_cast<int>(quadrantInfo.offsetY / 2.0f * fHeight);
		const auto subregion	= tcu::getSubregion(access, xCorner, yCorner, halfWidth, halfHeight);

		tcu::clear(subregion, quadrantInfo.color);
	}
}

tcu::TestStatus RebindSetsInstance::iterate (void)
{
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue			= m_context.getUniversalQueue();
	const auto			quadrantInfos	= getQuadrantInfos();
	const auto			setCount		= static_cast<uint32_t>(quadrantInfos.size());
	const auto			textureExtent	= makeExtent3D(1u, 1u, 1u);
	const tcu::IVec3	iTexExtent		(static_cast<int>(textureExtent.width), static_cast<int>(textureExtent.height), static_cast<int>(textureExtent.depth));
	const auto			textureFormat	= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuTexFormat	= mapVkFormat(textureFormat);
	const auto			textureUsage	= (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const auto			colorExtent		= makeExtent3D(m_params->width, m_params->height, 1u);
	const auto			colorFormat		= getOutputFormat();
	const auto			tcuColorFormat	= mapVkFormat(colorFormat);
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	DE_ASSERT(quadrantInfos.size() == 4u);

	// We need 4 descriptor sets: 4 buffers, 4 images and 1 sampler.
	const VkSamplerCreateInfo	samplerCreateInfo	= initVulkanStructure();
	const auto					sampler				= createSampler(vkd, device, &samplerCreateInfo);

	// Buffers.
	const auto					ssboSize			= static_cast<VkDeviceSize>(sizeof(float));
	const auto					ssboCreateInfo		= makeBufferCreateInfo(ssboSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	std::vector<std::unique_ptr<BufferWithMemory>> ssbos;
	ssbos.reserve(quadrantInfos.size());
	for (const auto& quadrantInfo : quadrantInfos)
	{
		ssbos.emplace_back(new BufferWithMemory(vkd, device, alloc, ssboCreateInfo, MemoryRequirement::HostVisible));
		void* data = ssbos.back()->getAllocation().getHostPtr();
		const auto redComponent = quadrantInfo.color.x();
		deMemcpy(data, &redComponent, sizeof(redComponent));
	}

	// Textures.
	const VkImageCreateInfo textureCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		textureFormat,							//	VkFormat				format;
		textureExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		textureUsage,							//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	const auto textureSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto textureSRL			= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto textureCopyRegion	= makeBufferImageCopy(textureExtent, textureSRL);

	std::vector<std::unique_ptr<ImageWithMemory>> textures;
	for (size_t i = 0u; i < quadrantInfos.size(); ++i)
		textures.emplace_back(new ImageWithMemory(vkd, device, alloc, textureCreateInfo, MemoryRequirement::Any));

	std::vector<Move<VkImageView>> textureViews;
	textureViews.reserve(quadrantInfos.size());
	for (const auto& texture : textures)
		textureViews.push_back(makeImageView(vkd, device, texture->get(), VK_IMAGE_VIEW_TYPE_2D, textureFormat, textureSRR));

	// Auxiliar buffers to fill the images with the right colors.
	const auto pixelSize				= tcu::getPixelSize(tcuTexFormat);
	const auto pixelCount				= textureExtent.width * textureExtent.height * textureExtent.depth;
	const auto auxiliarBufferSize		= static_cast<VkDeviceSize>(static_cast<VkDeviceSize>(pixelSize) * static_cast<VkDeviceSize>(pixelCount));
	const auto auxiliarBufferCreateInfo	= makeBufferCreateInfo(auxiliarBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	std::vector<std::unique_ptr<BufferWithMemory>> auxiliarBuffers;
	auxiliarBuffers.reserve(quadrantInfos.size());
	for (const auto& quadrantInfo : quadrantInfos)
	{
		auxiliarBuffers.emplace_back(new BufferWithMemory(vkd, device, alloc, auxiliarBufferCreateInfo, MemoryRequirement::HostVisible));

		void*					data			= auxiliarBuffers.back()->getAllocation().getHostPtr();
		tcu::PixelBufferAccess	access			(tcuTexFormat, iTexExtent, data);
		const tcu::Vec4			quadrantColor	(quadrantInfo.color.y(), 0.0f, 0.0f, 1.0f);

		tcu::clear(access, quadrantColor);
	}

	// Descriptor set layout.
	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_EXT);
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MESH_BIT_EXT);
	const auto setLayout = layoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pcSize			= static_cast<uint32_t>(sizeof(PushConstants));
	const auto pcRange			= makePushConstantRange(VK_SHADER_STAGE_MESH_BIT_EXT, 0u, pcSize);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get(), &pcRange);

	// Descriptor pool and sets.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, setCount);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setCount);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, setCount);

	std::vector<Move<VkDescriptorSet>> descriptorSets;
	for (size_t i = 0; i < quadrantInfos.size(); ++i)
		descriptorSets.push_back(makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get()));

	// Update descriptor sets.
	DescriptorSetUpdateBuilder updateBuilder;
	for (size_t i = 0; i < descriptorSets.size(); ++i)
	{
		const auto&	descriptorSet	= descriptorSets.at(i);
		const auto&	ssbo			= ssbos.at(i);
		const auto&	textureView		= textureViews.at(i);
		const auto	descBufferInfo	= makeDescriptorBufferInfo(ssbo->get(), 0ull, ssboSize);
		const auto	descImageInfo	= makeDescriptorImageInfo(sampler.get(), textureView.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descBufferInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descImageInfo);
	}
	updateBuilder.update(vkd, device);

	// Color attachment.
	const VkImageCreateInfo colorCreateInfo =
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
	const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto colorSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

	ImageWithMemory	colorAttachment	(vkd, device, alloc, colorCreateInfo, MemoryRequirement::Any);
	const auto		colorView		= makeImageView(vkd, device, colorAttachment.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// Create a memory buffer for verification.
	const auto			verificationBufferSize	= static_cast<VkDeviceSize>(colorExtent.width * colorExtent.height * tcu::getPixelSize(tcuColorFormat));
	const auto			verificationBufferUsage	= (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const auto			verificationBufferInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

	BufferWithMemory	verificationBuffer		(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc	= verificationBuffer.getAllocation();
	void*				verificationBufferData	= verificationBufferAlloc.getHostPtr();

	// Render pass and framebuffer.
	const auto renderPass	= makeRenderPass(vkd, device, colorFormat);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), colorExtent.width, colorExtent.height);

	const std::vector<VkViewport>	viewports	(1u, makeViewport(colorExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(colorExtent));

	// Shader modules and pipeline.
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	taskShader	= createShaderModule(vkd, device, binaries.get("task"));
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));
	const auto	pipeline	= makeGraphicsPipeline(
		vkd, device, pipelineLayout.get(),
		taskShader.get(), meshShader.get(), fragShader.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Copy data from auxiliar buffers to textures.
	for (const auto& texture : textures)
	{
		const auto prepareTextureForCopy = makeImageMemoryBarrier(
			0u, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			texture->get(), textureSRR);
		cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &prepareTextureForCopy);
	}

	for (size_t i = 0; i < auxiliarBuffers.size(); ++i)
	{
		const auto& auxBuffer	= auxiliarBuffers.at(i);
		const auto& texture		= textures.at(i);
		vkd.cmdCopyBufferToImage(cmdBuffer, auxBuffer->get(), texture->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &textureCopyRegion);
	}

	// Prepare textures for sampling.
	for (const auto& texture : textures)
	{
		const auto prepareTextureForSampling = makeImageMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			texture->get(), textureSRR);
		cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, &prepareTextureForSampling);
	}

	// Render stuff.
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());

	const auto drawCount = m_params->drawCount();
	for (size_t i = 0; i < quadrantInfos.size(); ++i)
	{
		const auto& quadrantInfo = quadrantInfos.at(i);
		const auto& descriptorSet = descriptorSets.at(i);

		PushConstants pcData;
		pcData.blueComponent = quadrantInfo.color.z();
		pcData.offsetX = quadrantInfo.offsetX;
		pcData.offsetY = quadrantInfo.offsetY;

		vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_MESH_BIT_EXT, 0u, pcSize, &pcData);
		vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCount.x(), drawCount.y(), drawCount.z());
	}

	endRenderPass(vkd, cmdBuffer);

	// Copy color attachment to verification buffer.
	const auto preCopyBarrier	= makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorAttachment.get(), colorSRR);
	const auto postCopyBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const auto copyRegion		= makeBufferImageCopy(colorExtent, colorSRL);

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorAttachment.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &postCopyBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Generate reference image and compare results.
	const tcu::IVec3					iColorExtent		(static_cast<int>(colorExtent.width), static_cast<int>(colorExtent.height), 1);
	const tcu::ConstPixelBufferAccess	verificationAccess	(tcuColorFormat, iColorExtent, verificationBufferData);

	generateReferenceLevel();
	invalidateAlloc(vkd, device, verificationBufferAlloc);
	if (!verifyResult(verificationAccess))
		TCU_FAIL("Result does not match reference; check log for details");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup* createMeshShaderMiscTestsEXT (tcu::TestContext& testCtx)
{
	GroupPtr miscTests (new tcu::TestCaseGroup(testCtx, "misc", "Mesh Shader Misc Tests"));

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::just(tcu::UVec3(2u, 1u, 1u)),
			/*meshCount*/	tcu::UVec3(2u, 1u, 1u),
			/*width*/		8u,
			/*height*/		8u));

		miscTests->addChild(new ComplexTaskDataCase(testCtx, "complex_task_data", "Pass a complex structure from the task to the mesh shader", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		5u,		// Use an odd value so there's a pixel in the exact center.
			/*height*/		7u));	// Idem.

		miscTests->addChild(new SinglePointCase(testCtx, "single_point", "Draw a single point", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		5u,		// Use an odd value so there's a pixel in the exact center.
			/*height*/		7u));	// Idem.

		// VK_KHR_maintenance5: Test default point size is 1.0f
		miscTests->addChild(new SinglePointCase(testCtx, "single_point_default_size", "Draw a single point without writing to PointSize", std::move(paramsPtr), false));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		8u,
			/*height*/		5u));	// Use an odd value so there's a center line.

		miscTests->addChild(new SingleLineCase(testCtx, "single_line", "Draw a single line", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		5u,	// Use an odd value so there's a pixel in the exact center.
			/*height*/		7u));	// Idem.

		miscTests->addChild(new SingleTriangleCase(testCtx, "single_triangle", "Draw a single triangle", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		16u,
			/*height*/		16u));

		miscTests->addChild(new MaxPointsCase(testCtx, "max_points", "Draw the maximum number of points", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::Nothing,
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		1u,
			/*height*/		1020u));

		miscTests->addChild(new MaxLinesCase(testCtx, "max_lines", "Draw the maximum number of lines", std::move(paramsPtr)));
	}

	{
		const tcu::UVec3 localSizes[] =
		{
			tcu::UVec3(2u, 4u, 8u),
			tcu::UVec3(4u, 2u, 4u),
			tcu::UVec3(2u, 2u, 4u),
		};

		for (const auto& localSize : localSizes)
		{
			const auto workGroupSize	= (localSize.x() * localSize.y() * localSize.z());
			const auto wgsStr			= std::to_string(workGroupSize);
			const auto testName			= "max_triangles_workgroupsize_" + wgsStr;
			const auto testDesc			= "Draw the maximum number of triangles using a work group size of " + wgsStr;

			ParamsPtr paramsPtr (new MaxTrianglesCase::Params(
				/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
				/*width*/		512u,
				/*height*/		512u,
				/*localSize*/	localSize));

			miscTests->addChild(new MaxTrianglesCase(testCtx, testName, testDesc, std::move(paramsPtr)));
		}
	}

	using LargeWorkGroupParamsPtr = std::unique_ptr<LargeWorkGroupParams>;
	const int dimensionCases[] = { 0, 1, 2 };

	for (const auto& dim : dimensionCases)
	{
		const auto dimChar = dimSuffix(dim);

		{
			tcu::UVec3 taskCount (8u, 8u, 8u);
			taskCount[dim] = 65535u;

			LargeWorkGroupParamsPtr lwgParamsPtr	(new LargeWorkGroupParams(
				/*taskCount*/						tcu::just(taskCount),
				/*meshCount*/						tcu::UVec3(1u, 1u, 1u),
				/*width*/							2040u,
				/*height*/							2056u,
				/*localInvocations*/				tcu::UVec3(1u, 1u, 1u)));

			ParamsPtr paramsPtr (lwgParamsPtr.release());

			const auto name = std::string("many_task_work_groups_") + dimChar;
			const auto desc = std::string("Generate a large number of task work groups in the ") + dimChar + " dimension";

			miscTests->addChild(new LargeWorkGroupCase(testCtx, name, desc, std::move(paramsPtr)));
		}

		{
			tcu::UVec3 meshCount (8u, 8u, 8u);
			meshCount[dim] = 65535u;

			LargeWorkGroupParamsPtr lwgParamsPtr	(new LargeWorkGroupParams(
				/*taskCount*/						tcu::Nothing,
				/*meshCount*/						meshCount,
				/*width*/							2040u,
				/*height*/							2056u,
				/*localInvocations*/				tcu::UVec3(1u, 1u, 1u)));

			ParamsPtr paramsPtr (lwgParamsPtr.release());

			const auto name = std::string("many_mesh_work_groups_") + dimChar;
			const auto desc = std::string("Generate a large number of mesh work groups in the ") + dimChar + " dimension";

			miscTests->addChild(new LargeWorkGroupCase(testCtx, name, desc, std::move(paramsPtr)));
		}

		{
			tcu::UVec3 meshCount (1u, 1u, 1u);
			tcu::UVec3 taskCount (1u, 1u, 1u);
			tcu::UVec3 localInvs (1u, 1u, 1u);

			meshCount[dim] = 256u;
			taskCount[dim] = 128u;
			localInvs[dim] = 128u;

			LargeWorkGroupParamsPtr lwgParamsPtr	(new LargeWorkGroupParams(
				/*taskCount*/						tcu::just(taskCount),
				/*meshCount*/						meshCount,
				/*width*/							2048u,
				/*height*/							2048u,
				/*localInvocations*/				localInvs));

			ParamsPtr paramsPtr (lwgParamsPtr.release());

			const auto name = std::string("many_task_mesh_work_groups_") + dimChar;
			const auto desc = std::string("Generate a large number of task and mesh work groups in the ") + dimChar + " dimension";

			miscTests->addChild(new LargeWorkGroupCase(testCtx, name, desc, std::move(paramsPtr)));
		}
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

			// XXX Is this test legal? [https://gitlab.khronos.org/GLSL/GLSL/-/merge_requests/77#note_348252]
			if (extraWrites)
				continue;

			for (const auto primType : types)
			{
				std::unique_ptr<NoPrimitivesParams> params	(new NoPrimitivesParams(
					/*taskCount*/							(extraWrites ? tcu::just(tcu::UVec3(1u, 1u, 1u)) : tcu::Nothing),
					/*meshCount*/							tcu::UVec3(1u, 1u, 1u),
					/*width*/								16u,
					/*height*/								16u,
					/*primitiveType*/						primType));

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
				/*taskCount*/		(useTaskShader ? tcu::just(tcu::UVec3(1u, 1u, 1u)) : tcu::Nothing),
				/*meshCount*/		tcu::UVec3(1u, 1u, 1u),
				/*width*/			1u,
				/*height*/			1u));

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

				std::unique_ptr<MemoryBarrierParams> paramsPtr	(new MemoryBarrierParams(
					/*taskCount*/								(useTaskShader ? tcu::just(tcu::UVec3(1u, 1u, 1u)) : tcu::Nothing),
					/*meshCount*/								tcu::UVec3(1u, 1u, 1u),
					/*width*/									1u,
					/*height*/									1u,
					/*memBarrierType*/							barrierCase.memBarrierType));

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
				/*taskCount*/		(useTaskShader ? tcu::just(tcu::UVec3(1u, 1u, 1u)) : tcu::Nothing),
				/*meshCount*/		tcu::UVec3(1u, 1u, 1u),
				/*width*/			32u,
				/*height*/			32u));

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
				/*taskCount*/		(useTaskShader ? tcu::just(tcu::UVec3(1u, 1u, 1u)) : tcu::Nothing),
				/*meshCount*/		tcu::UVec3(1u, 1u, 1u),
				/*width*/			16u,
				/*height*/			16u));

			miscTests->addChild(new PushConstantCase(testCtx, name, desc, std::move(paramsPtr)));
		}
	}

	{
		ParamsPtr paramsPtr (new MaximizeThreadsParams(
			/*taskCount*/		tcu::Nothing,
			/*meshCount*/		tcu::UVec3(1u, 1u, 1u),
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
			/*meshCount*/		tcu::UVec3(1u, 1u, 1u),
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
				/*meshCount*/		tcu::UVec3(1u, 1u, 1u),
				/*width*/			numPixels,
				/*height*/			1u,
				/*localSize*/		invocationCase,
				/*numVertices*/		numPixels,
				/*numPrimitives*/	numPixels));

			miscTests->addChild(new MaximizeInvocationsCase(testCtx, "maximize_invocations_" + invsStr, "Use a large number of invocations compared to other sizes: " + invsStr, std::move(paramsPtr)));
		}
	}

	{
		for (int i = 0; i < 2; ++i)
		{
			const bool useDynamicTopology = (i > 0);

			ParamsPtr paramsPtr (new MixedPipelinesParams(
				/*taskCount*/		tcu::Nothing,
				/*meshCount*/		tcu::UVec3(1u, 1u, 1u),
				/*width*/			8u,
				/*height*/			8u,
				/*dynamicTopology*/	useDynamicTopology));

			const std::string nameSuffix = (useDynamicTopology ? "_dynamic_topology" : "");
			const std::string descSuffix = (useDynamicTopology ? " and use dynamic topology" : "");

			miscTests->addChild(new MixedPipelinesCase(testCtx, "mixed_pipelines" + nameSuffix, "Test mixing classic and mesh pipelines in the same render pass" + descSuffix, std::move(paramsPtr)));
		}
	}

	for (int i = 0; i < 2; ++i)
	{
		const bool						useTask		= (i > 0);
		const tcu::Maybe<tcu::UVec3>	taskCount	= (useTask ? tcu::just(tcu::UVec3(1u, 1u, 1u)) : tcu::nothing<tcu::UVec3>());
		const std::string				testName	= std::string("first_invocation_") + (useTask ? "task" : "mesh");

		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	taskCount,
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		128u,
			/*height*/		1u));

		miscTests->addChild(new FirstInvocationCase(testCtx, testName, "Check only the first invocation is used in EmitMeshTasksEXT() and SetMeshOutputsEXT()", std::move(paramsPtr)));
	}

	for (int i = 0; i < 2; ++i)
	{
		const bool						useTask		= (i > 0);
		const tcu::Maybe<tcu::UVec3>	taskCount	= (useTask ? tcu::just(tcu::UVec3(1u, 1u, 1u)) : tcu::nothing<tcu::UVec3>());
		const std::string				testName	= std::string("local_size_id_") + (useTask ? "task" : "mesh");

		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	taskCount,
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		32u,
			/*height*/		1u));

		miscTests->addChild(new LocalSizeIdCase(testCtx, testName, "Check LocalSizeId can be used with task and mesh shaders", std::move(paramsPtr)));
	}

	if (false) // Disabled. This may be illegal.
	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::UVec3(1u, 1u, 1u),
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		8u,
			/*height*/		8u));

		miscTests->addChild(new MultipleTaskPayloadsCase(testCtx, "multiple_task_payloads", "Check the task payload can be chosen among several ones", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::UVec3(1u, 1u, 1u),
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		8u,
			/*height*/		8u));

		miscTests->addChild(new PayloadReadCase(testCtx, "payload_read", "Check the task payload can be read from all task shader instances", std::move(paramsPtr)));
	}

	{
		ParamsPtr paramsPtr (new MiscTestParams(
			/*taskCount*/	tcu::UVec3(1u, 1u, 1u),
			/*meshCount*/	tcu::UVec3(1u, 1u, 1u),
			/*width*/		8u,
			/*height*/		8u));

		miscTests->addChild(new RebindSetsCase(testCtx, "rebind_sets", "Use several draw calls binding new descriptor sets and updating push constants between them", std::move(paramsPtr)));
	}

	return miscTests.release();
}

} // MeshShader
} // vkt
