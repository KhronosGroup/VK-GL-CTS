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
 * \brief Tests mixing VK_EXT_mesh_shader and VK_EXT_provoking_vertex
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderProvokingVertexTestsEXT.hpp"
#include "vktTestCase.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>
#include <string>

namespace vkt
{
namespace MeshShader
{

namespace
{

using namespace vk;

enum class Geometry
{
	LINES = 0,
	TRIANGLES,
};

using ProvokingVertexModeVec = std::vector<VkProvokingVertexModeEXT>;

std::vector<tcu::UVec4>	getLineColors			(void)
{
	return std::vector<tcu::UVec4>{
		tcu::UVec4(1, 1, 0, 1),
		tcu::UVec4(1, 0, 1, 1),
	};
}

std::vector<tcu::UVec4>	getTriangleColors		(void)
{
	return std::vector<tcu::UVec4>{
		tcu::UVec4(1, 1, 0, 1),
		tcu::UVec4(0, 1, 1, 1),
		tcu::UVec4(1, 0, 1, 1),
	};
}

std::vector<tcu::Vec4>	getLinePositions		(void)
{
	return std::vector<tcu::Vec4>{
		tcu::Vec4(-1.0, 0.0, 0.0, 1.0),
		tcu::Vec4( 1.0, 0.0, 0.0, 1.0),
	};
}

std::vector<tcu::Vec4>	getTrianglePositions	(void)
{
	return std::vector<tcu::Vec4>{
		tcu::Vec4(-1.0, -1.0, 0.0, 1.0),
		tcu::Vec4(-1.0,  1.0, 0.0, 1.0),
		tcu::Vec4( 3.0, -1.0, 0.0, 1.0),
	};
}

tcu::Vec4				getClearColor			(void)
{
	return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

std::string getCaseName (Geometry geometry)
{
	switch (geometry)
	{
	case Geometry::LINES:		return "lines";
	case Geometry::TRIANGLES:	return "triangles";
	default:
		DE_ASSERT(false);
		break;
	}
	// Unreachable.
	return "";
}

std::string getCaseName (const ProvokingVertexModeVec& modes)
{
	std::string caseName;

	for (const auto& mode : modes)
	{
		caseName += (caseName.empty() ? "" : "_");
		switch (mode)
		{
		case VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT:		caseName += "first";	break;
		case VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT:		caseName += "last";		break;
		default:
			DE_ASSERT(false);
			break;
		}
	}

	return caseName;
}

struct TestParams
{
	ProvokingVertexModeVec	provokingVertices;	// In the same render pass. In practice 1 or 2 elements.
	Geometry				geometryType;
};

class ProvokingVertexCase : public vkt::TestCase
{
public:
					ProvokingVertexCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{
							DE_ASSERT(m_params.provokingVertices.size() <= 2);
						}

	virtual			~ProvokingVertexCase	(void) {}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
	void			checkSupport			(Context& context) const override;

protected:
	TestParams		m_params;
};

class ProvokingVertexInstance : public vkt::TestInstance
{
public:
						ProvokingVertexInstance		(Context& context, const TestParams& params)
							: vkt::TestInstance	(context)
							, m_params			(&params)
							{}
	virtual				~ProvokingVertexInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;

protected:
	const TestParams*	m_params;
};

TestInstance* ProvokingVertexCase::createInstance (Context& context) const
{
	return new ProvokingVertexInstance(context, m_params);
}

void ProvokingVertexCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "layout (location=0) flat in uvec4 inColor;\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    outColor = vec4(inColor);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

	const auto isLines		= (m_params.geometryType == Geometry::LINES);
	const auto vertCount	= (isLines ? 2u : 3u);
	const auto geometryName	= (isLines ? "lines" : "triangles");
	const auto primIndices	= (isLines
							? "gl_PrimitiveLineIndicesEXT[0] = uvec2(0, 1);"
							: "gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);");
	const auto colors		= (isLines ? getLineColors() : getTriangleColors());
	const auto positions	= (isLines ? getLinePositions() : getTrianglePositions());

	std::ostringstream mesh;
	mesh
		<< "#version 460\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << vertCount << ", local_size_y=1, local_size_z=1) in;\n"
		<< "layout (" << geometryName << ") out;\n"
		<< "layout (max_vertices=" << vertCount << ", max_primitives=1) out;\n"
		<< "\n"
		<< "layout (push_constant, std430) uniform PushConstantBlock {\n"
		<< "    int layer;\n"
		<< "} pc;\n"
		<< "\n"
		<< "perprimitiveEXT out gl_MeshPerPrimitiveEXT {\n"
		<< "   int gl_Layer;\n"
		<< "} gl_MeshPrimitivesEXT[];\n"
		<< "\n"
		<< "uvec4 colors[] = uvec4[](\n"
		;

	for (size_t i = 0; i < colors.size(); ++i)
		mesh << "    uvec4" << colors[i] << ((i < colors.size() - 1) ? "," : "") << "\n";

	mesh
		<< ");\n"
		<< "\n"
		<< "vec4 vertices[] = vec4[](\n"
		;

	for (size_t i = 0; i < positions.size(); ++i)
		mesh << "    vec4" << positions[i] << ((i < positions.size() - 1) ? "," : "") << "\n";

	mesh
		<< ");\n"
		<< "\n"
		<< "layout (location=0) flat out uvec4 vtxColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    SetMeshOutputsEXT(" << vertCount << ", 1);\n"
		<< "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vertices[gl_LocalInvocationIndex];\n"
		<< "    vtxColor[gl_LocalInvocationIndex] = colors[gl_LocalInvocationIndex];\n"
		<< "\n"
		<< "    if (gl_LocalInvocationIndex == 0u) {\n"
		<< "        " << primIndices << "\n"
		<< "        gl_MeshPrimitivesEXT[0].gl_Layer = pc.layer;\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

void ProvokingVertexCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportEXT(context, false/*requireTask*/, true/*requireMesh*/);

	context.requireDeviceFunctionality("VK_EXT_provoking_vertex");

	if (m_params.provokingVertices.size() > 1)
	{
		const auto& pvProperties = context.getProvokingVertexPropertiesEXT();
		if (!pvProperties.provokingVertexModePerPipeline)
			TCU_THROW(NotSupportedError, "Switching provoking vertex modes in the same render pass not supported");
	}
}

tcu::TestStatus ProvokingVertexInstance::iterate (void)
{
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue			= m_context.getUniversalQueue();
	const auto			colorExtent		= makeExtent3D(1u, 1u, 1u);
	const auto			colorLayers		= static_cast<uint32_t>(m_params->provokingVertices.size());
	const auto			colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			tcuFormat		= mapVkFormat(colorFormat);
	const auto			pixelSize		= static_cast<uint32_t>(tcu::getPixelSize(tcuFormat));
	const auto			viewType		= ((colorLayers > 1u) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
	const auto			clearColor		= getClearColor();

	// Color attachment.
	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		colorExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		colorLayers,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory	colorBuffer	(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto		colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorLayers);
	const auto		colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, colorLayers);
	const auto		colorView	= makeImageView(vkd, device, colorBuffer.get(), viewType, colorFormat, colorSRR);

	// Verification buffer.
	const auto			verificationBufferSize	= (pixelSize * colorExtent.width * colorExtent.height * colorLayers);
	const auto			verificationBufferInfo	= makeBufferCreateInfo(verificationBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory	verificationBuffer		(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);

	// Push constant range.
	const auto pcSize	= static_cast<uint32_t>(sizeof(int32_t));
	const auto pcStages	= VK_SHADER_STAGE_MESH_BIT_EXT;
	const auto pcRange	= makePushConstantRange(pcStages, 0u, pcSize);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, DE_NULL, &pcRange);

	// Modules.
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	meshModule	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragModule	= createShaderModule(vkd, device, binaries.get("frag"));

	// Render pass and framebuffer.
	const auto renderPass	= makeRenderPass(vkd, device, colorFormat);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), colorExtent.width, colorExtent.height, colorLayers);

	// Viewports and scissors.
	const std::vector<VkViewport>	viewports	(1u, makeViewport(colorExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(colorExtent));

	// Pipelines with different provoking vertex modes.
	std::vector<Move<VkPipeline>>	pipelines;

	VkPipelineRasterizationProvokingVertexStateCreateInfoEXT pvInfo = initVulkanStructure();

	const VkPipelineRasterizationStateCreateInfo rasterState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		//	VkStructureType							sType;
		&pvInfo,														//	const void*								pNext;
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

	for (const auto& pvMode : m_params->provokingVertices)
	{
		pvInfo.provokingVertexMode = pvMode;

		pipelines.push_back(makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
			DE_NULL, meshModule.get(), fragModule.get(),
			renderPass.get(), viewports, scissors, 0u, &rasterState));
	}

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor);
	for (int32_t layer = 0; layer < static_cast<int32_t>(pipelines.size()); ++layer)
	{
		const auto& pipeline = pipelines.at(static_cast<size_t>(layer));
		vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &layer);
		vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
	}
	endRenderPass(vkd, cmdBuffer);
	{
		// Copy data to verification buffer.
		const auto preTransferBarrier = makeImageMemoryBarrier(
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			colorBuffer.get(), colorSRR);

		cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);

		const auto copyRegion = makeBufferImageCopy(colorExtent, colorSRL);
		vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);

		const auto postTransferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

		cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postTransferBarrier);
	}
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify colors.
	auto& verificationBufferAlloc	= verificationBuffer.getAllocation();
	void* verificationBufferData	= verificationBufferAlloc.getHostPtr();
	invalidateAlloc(vkd, device, verificationBufferAlloc);

	const tcu::IVec3					iExtent			(static_cast<int>(colorExtent.width), static_cast<int>(colorExtent.height), static_cast<int>(colorLayers));
	const tcu::ConstPixelBufferAccess	resultAccess	(tcuFormat, iExtent, verificationBufferData);

	const auto isLines		= (m_params->geometryType == Geometry::LINES);
	const auto colors		= (isLines ? getLineColors() : getTriangleColors());

	bool	fail	= false;
	auto&	log		= m_context.getTestContext().getLog();

	for (int z = 0; z < iExtent.z(); ++z)
	{
		const auto		pvMode				= m_params->provokingVertices.at(static_cast<size_t>(z));
		const auto		expectedIntColor	= (pvMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT ? colors.front() : colors.back());
		const tcu::Vec4	expectedColor		(static_cast<float>(expectedIntColor.x()),
											 static_cast<float>(expectedIntColor.y()),
											 static_cast<float>(expectedIntColor.z()),
											 static_cast<float>(expectedIntColor.w()));

		for (int y = 0; y < iExtent.y(); ++y)
			for (int x = 0; x < iExtent.x(); ++x)
			{
				const auto resultColor = resultAccess.getPixel(x, y, z);
				if (resultColor != expectedColor)
				{
					fail = true;
					std::ostringstream msg;
					msg << "Unexpected color found at layer " << z << " coordinates (" << x << ", " << y << "): expected " << expectedColor << " found " << resultColor;
					log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
				}
			}
	}

	if (fail)
		return tcu::TestStatus::fail("Failed -- check log for details");
	return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // anonymous namespace

tcu::TestCaseGroup* createMeshShaderProvokingVertexTestsEXT (tcu::TestContext& testCtx)
{
	const std::vector<Geometry> geometries { Geometry::LINES, Geometry::TRIANGLES };

	const std::vector<ProvokingVertexModeVec> testModeCases
	{
		ProvokingVertexModeVec{VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT},
		ProvokingVertexModeVec{VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT},
		ProvokingVertexModeVec{VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT},
		ProvokingVertexModeVec{VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT},
	};

	GroupPtr provokingVertexGroup (new tcu::TestCaseGroup(testCtx, "provoking_vertex", ""));

	for (const auto& geometry : geometries)
	{
		const auto	geometryName	= getCaseName(geometry);
		GroupPtr	geometryGroup	(new tcu::TestCaseGroup(testCtx, geometryName.c_str(), ""));

		for (const auto& testModes : testModeCases)
		{
			const auto	modeName = getCaseName(testModes);
			TestParams	params
			{
				testModes,	//	ProvokingVertexModeVec	provokingVertices;
				geometry,	//	Geometry				geometryType;
			};

			geometryGroup->addChild(new ProvokingVertexCase(testCtx, modeName, "", params));
		}

		provokingVertexGroup->addChild(geometryGroup.release());
	}

	return provokingVertexGroup.release();
}

} // MeshShader
} // vkt
