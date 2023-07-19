/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Tessellation Miscellaneous Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktTessellationMiscDrawTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTessellationUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageIO.hpp"
#include "tcuTexture.hpp"
#include "tcuImageCompare.hpp"

#include "vkDefs.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>
#include <vector>
#include <utility>

namespace vkt
{
namespace tessellation
{

using namespace vk;

namespace
{

struct CaseDefinition
{
	TessPrimitiveType	primitiveType;
	SpacingMode			spacingMode;
	std::string			referenceImagePathPrefix;	//!< without case suffix and extension (e.g. "_1.png")
};

inline CaseDefinition makeCaseDefinition (const TessPrimitiveType	primitiveType,
										  const SpacingMode			spacingMode,
										  const std::string&		referenceImagePathPrefix)
{
	CaseDefinition caseDef;
	caseDef.primitiveType = primitiveType;
	caseDef.spacingMode = spacingMode;
	caseDef.referenceImagePathPrefix = referenceImagePathPrefix;
	return caseDef;
}

std::vector<TessLevels> genTessLevelCases (const SpacingMode spacingMode)
{
	static const TessLevels tessLevelCases[] =
	{
		{ { 9.0f,	9.0f	},	{ 9.0f,		9.0f,	9.0f,	9.0f	} },
		{ { 8.0f,	11.0f	},	{ 13.0f,	15.0f,	18.0f,	21.0f	} },
		{ { 17.0f,	14.0f	},	{ 3.0f,		6.0f,	9.0f,	12.0f	} },
	};

	std::vector<TessLevels> resultTessLevels(DE_LENGTH_OF_ARRAY(tessLevelCases));

	for (int tessLevelCaseNdx = 0; tessLevelCaseNdx < DE_LENGTH_OF_ARRAY(tessLevelCases); ++tessLevelCaseNdx)
	{
		TessLevels& tessLevels = resultTessLevels[tessLevelCaseNdx];

		for (int i = 0; i < 2; ++i)
			tessLevels.inner[i] = static_cast<float>(getClampedRoundedTessLevel(spacingMode, tessLevelCases[tessLevelCaseNdx].inner[i]));

		for (int i = 0; i < 4; ++i)
			tessLevels.outer[i] = static_cast<float>(getClampedRoundedTessLevel(spacingMode, tessLevelCases[tessLevelCaseNdx].outer[i]));
	}

	return resultTessLevels;
}

std::vector<tcu::Vec2> genVertexPositions (const TessPrimitiveType primitiveType)
{
	std::vector<tcu::Vec2> positions;
	positions.reserve(4);

	if (primitiveType == TESSPRIMITIVETYPE_TRIANGLES)
	{
		positions.push_back(tcu::Vec2( 0.8f,    0.6f));
		positions.push_back(tcu::Vec2( 0.0f, -0.786f));
		positions.push_back(tcu::Vec2(-0.8f,    0.6f));
	}
	else if (primitiveType == TESSPRIMITIVETYPE_QUADS || primitiveType == TESSPRIMITIVETYPE_ISOLINES)
	{
		positions.push_back(tcu::Vec2(-0.8f, -0.8f));
		positions.push_back(tcu::Vec2( 0.8f, -0.8f));
		positions.push_back(tcu::Vec2(-0.8f,  0.8f));
		positions.push_back(tcu::Vec2( 0.8f,  0.8f));
	}
	else
		DE_ASSERT(false);

	return positions;
}

//! Common test function used by all test cases.
tcu::TestStatus runTest (Context& context, const CaseDefinition caseDef)
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_TESSELLATION_SHADER);

	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			device				= context.getDevice();
	const VkQueue			queue				= context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= context.getDefaultAllocator();

	const std::vector<TessLevels> tessLevelCases = genTessLevelCases(caseDef.spacingMode);
	const std::vector<tcu::Vec2>  vertexData	 = genVertexPositions(caseDef.primitiveType);
	const deUint32				  inPatchSize	 = (caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ? 3 : 4);

	// Vertex input: positions

	const VkFormat	   vertexFormat		   = VK_FORMAT_R32G32_SFLOAT;
	const deUint32	   vertexStride		   = tcu::getPixelSize(mapVkFormat(vertexFormat));
	const VkDeviceSize vertexDataSizeBytes = sizeInBytes(vertexData);

	const BufferWithMemory vertexBuffer(vk, device, allocator,
		makeBufferCreateInfo(vertexDataSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible);

	DE_ASSERT(inPatchSize == vertexData.size());
	DE_ASSERT(sizeof(vertexData[0]) == vertexStride);

	{
		const Allocation& alloc = vertexBuffer.getAllocation();

		deMemcpy(alloc.getHostPtr(), &vertexData[0], static_cast<std::size_t>(vertexDataSizeBytes));
		flushAlloc(vk, device, alloc);
		// No barrier needed, flushed memory is automatically visible
	}

	// Color attachment

	const tcu::IVec2			  renderSize				 = tcu::IVec2(256, 256);
	const VkFormat				  colorFormat				 = VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageSubresourceRange colorImageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const ImageWithMemory		  colorAttachmentImage		 (vk, device, allocator,
															 makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 1u),
															 MemoryRequirement::Any);

	// Color output buffer: image will be copied here for verification

	const VkDeviceSize		colorBufferSizeBytes = renderSize.x()*renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
	const BufferWithMemory	colorBuffer			(vk, device, allocator, makeBufferCreateInfo(colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);

	// Input buffer: tessellation levels. Data is filled in later.

	const BufferWithMemory tessLevelsBuffer(vk, device, allocator,
		makeBufferCreateInfo(sizeof(TessLevels), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Descriptors

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo tessLevelsBufferInfo = makeDescriptorBufferInfo(tessLevelsBuffer.get(), 0ull, sizeof(TessLevels));

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tessLevelsBufferInfo)
		.update(vk, device);

	// Pipeline

	const Unique<VkImageView>		colorAttachmentView	(makeImageView(vk, device, *colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorImageSubresourceRange));
	const Unique<VkRenderPass>		renderPass			(makeRenderPass(vk, device, colorFormat));
	const Unique<VkFramebuffer>		framebuffer			(makeFramebuffer(vk, device, *renderPass, *colorAttachmentView, renderSize.x(), renderSize.y()));
	const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkCommandPool>		cmdPool				(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const Unique<VkPipeline> pipeline(GraphicsPipelineBuilder()
		.setRenderSize				  (renderSize)
		.setVertexInputSingleAttribute(vertexFormat, vertexStride)
		.setPatchControlPoints		  (inPatchSize)
		.setShader					  (vk, device, VK_SHADER_STAGE_VERTEX_BIT,					context.getBinaryCollection().get("vert"), DE_NULL)
		.setShader					  (vk, device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,	context.getBinaryCollection().get("tesc"), DE_NULL)
		.setShader					  (vk, device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, context.getBinaryCollection().get("tese"), DE_NULL)
		.setShader					  (vk, device, VK_SHADER_STAGE_FRAGMENT_BIT,				context.getBinaryCollection().get("frag"), DE_NULL)
		.build						  (vk, device, *pipelineLayout, *renderPass));

	// Draw commands

	deUint32 numPassedCases = 0;

	for (deUint32 tessLevelCaseNdx = 0; tessLevelCaseNdx < tessLevelCases.size(); ++tessLevelCaseNdx)
	{
		context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Tessellation levels: " << getTessellationLevelsString(tessLevelCases[tessLevelCaseNdx], caseDef.primitiveType)
			<< tcu::TestLog::EndMessage;

		// Upload tessellation levels data to the input buffer
		{
			const Allocation& alloc				= tessLevelsBuffer.getAllocation();
			TessLevels* const bufferTessLevels	= static_cast<TessLevels*>(alloc.getHostPtr());

			*bufferTessLevels = tessLevelCases[tessLevelCaseNdx];
			flushAlloc(vk, device, alloc);
		}

		// Reset the command buffer and begin recording.
		beginCommandBuffer(vk, *cmdBuffer);

		// Change color attachment image layout
		{
			// State is slightly different on the first iteration.
			const VkImageLayout currentLayout = (tessLevelCaseNdx == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			const VkAccessFlags srcFlags	  = (tessLevelCaseNdx == 0 ? (VkAccessFlags)0 : (VkAccessFlags)VK_ACCESS_TRANSFER_READ_BIT);

			const VkImageMemoryBarrier colorAttachmentLayoutBarrier = makeImageMemoryBarrier(
				srcFlags, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				currentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				*colorAttachmentImage, colorImageSubresourceRange);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, &colorAttachmentLayoutBarrier);
		}

		// Begin render pass
		{
			const VkRect2D	renderArea	= makeRect2D(renderSize);
			const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 1.0f);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);
		}

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		{
			const VkDeviceSize vertexBufferOffset = 0ull;
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
		}

		// Process enough vertices to make a patch.
		vk.cmdDraw(*cmdBuffer, inPatchSize, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		// Copy render result to a host-visible buffer
		copyImageToBuffer(vk, *cmdBuffer, *colorAttachmentImage, *colorBuffer, renderSize);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		{
			const Allocation&	colorBufferAlloc	= colorBuffer.getAllocation();

			invalidateAlloc(vk, device, colorBufferAlloc);

			// Verify case result
			const tcu::ConstPixelBufferAccess resultImageAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAlloc.getHostPtr());

			// Load reference image
			const std::string	referenceImagePath	= caseDef.referenceImagePathPrefix + "_" + de::toString(tessLevelCaseNdx) + ".png";

			tcu::TextureLevel	referenceImage;
			tcu::ImageIO::loadPNG(referenceImage, context.getTestContext().getArchive(), referenceImagePath.c_str());

			if (tcu::fuzzyCompare(context.getTestContext().getLog(), "ImageComparison", "Image Comparison",
								  referenceImage.getAccess(), resultImageAccess, 0.002f, tcu::COMPARE_LOG_RESULT))
				++numPassedCases;
		}
	} // tessLevelCaseNdx

	return (numPassedCases == tessLevelCases.size() ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Failure"));
}

inline const char* getTessLevelsSSBODeclaration (void)
{
	return	"layout(set = 0, binding = 0, std430) readonly restrict buffer TessLevels {\n"
			"    float inner0;\n"
			"    float inner1;\n"
			"    float outer0;\n"
			"    float outer1;\n"
			"    float outer2;\n"
			"    float outer3;\n"
			"} sb_levels;\n";
}

//! Add vertex, fragment, and tessellation control shaders.
void initCommonPrograms (vk::SourceCollections& programCollection, const CaseDefinition caseDef)
{
	DE_ASSERT(!programCollection.glslSources.contains("vert"));
	DE_ASSERT(!programCollection.glslSources.contains("tesc"));
	DE_ASSERT(!programCollection.glslSources.contains("frag"));

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  highp vec2 in_v_position;\n"
			<< "layout(location = 0) out highp vec2 in_tc_position;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_tc_position = in_v_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation control shader
	{
		const int numVertices = (caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ? 3 : 4);

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(vertices = " << numVertices << ") out;\n"
			<< "\n"
			<< getTessLevelsSSBODeclaration()
			<< "\n"
			<< "layout(location = 0) in  highp vec2 in_tc_position[];\n"
			<< "layout(location = 0) out highp vec2 in_te_position[];\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_te_position[gl_InvocationID] = in_tc_position[gl_InvocationID];\n"
			<< "\n"
			<< "    gl_TessLevelInner[0] = sb_levels.inner0;\n"
			<< "    gl_TessLevelInner[1] = sb_levels.inner1;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = sb_levels.outer0;\n"
			<< "    gl_TessLevelOuter[1] = sb_levels.outer1;\n"
			<< "    gl_TessLevelOuter[2] = sb_levels.outer2;\n"
			<< "    gl_TessLevelOuter[3] = sb_levels.outer3;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  highp   vec4 in_f_color;\n"
			<< "layout(location = 0) out mediump vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = in_f_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void initProgramsFillCoverCase (vk::SourceCollections& programCollection, const CaseDefinition caseDef)
{
	DE_ASSERT(caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES || caseDef.primitiveType == TESSPRIMITIVETYPE_QUADS);

	initCommonPrograms(programCollection, caseDef);

	// Tessellation evaluation shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(" << getTessPrimitiveTypeShaderName(caseDef.primitiveType) << ", "
						 << getSpacingModeShaderName(caseDef.spacingMode) << ") in;\n"
			<< "\n"
			<< "layout(location = 0) in  highp vec2 in_te_position[];\n"
			<< "layout(location = 0) out highp vec4 in_f_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<<	(caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ?
					"    highp float d = 3.0 * min(gl_TessCoord.x, min(gl_TessCoord.y, gl_TessCoord.z));\n"
					"    highp vec2 corner0 = in_te_position[0];\n"
					"    highp vec2 corner1 = in_te_position[1];\n"
					"    highp vec2 corner2 = in_te_position[2];\n"
					"    highp vec2 pos =  corner0*gl_TessCoord.x + corner1*gl_TessCoord.y + corner2*gl_TessCoord.z;\n"
					"    highp vec2 fromCenter = pos - (corner0 + corner1 + corner2) / 3.0;\n"
					"    highp float f = (1.0 - length(fromCenter)) * (1.5 - d);\n"
					"    pos += 0.75 * f * fromCenter / (length(fromCenter) + 0.3);\n"
					"    gl_Position = vec4(pos, 0.0, 1.0);\n"
				: caseDef.primitiveType == TESSPRIMITIVETYPE_QUADS ?
					"    highp vec2 corner0 = in_te_position[0];\n"
					"    highp vec2 corner1 = in_te_position[1];\n"
					"    highp vec2 corner2 = in_te_position[2];\n"
					"    highp vec2 corner3 = in_te_position[3];\n"
					"    highp vec2 pos = (1.0-gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner0\n"
					"                   + (    gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner1\n"
					"                   + (1.0-gl_TessCoord.x)*(    gl_TessCoord.y)*corner2\n"
					"                   + (    gl_TessCoord.x)*(    gl_TessCoord.y)*corner3;\n"
					"    highp float d = 2.0 * min(abs(gl_TessCoord.x-0.5), abs(gl_TessCoord.y-0.5));\n"
					"    highp vec2 fromCenter = pos - (corner0 + corner1 + corner2 + corner3) / 4.0;\n"
					"    highp float f = (1.0 - length(fromCenter)) * sqrt(1.7 - d);\n"
					"    pos += 0.75 * f * fromCenter / (length(fromCenter) + 0.3);\n"
					"    gl_Position = vec4(pos, 0.0, 1.0);\n"
				: "")
			<< "    in_f_color = vec4(1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}
}

void initProgramsFillNonOverlapCase (vk::SourceCollections& programCollection, const CaseDefinition caseDef)
{
	DE_ASSERT(caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES || caseDef.primitiveType == TESSPRIMITIVETYPE_QUADS);

	initCommonPrograms(programCollection, caseDef);

	// Tessellation evaluation shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(" << getTessPrimitiveTypeShaderName(caseDef.primitiveType) << ", "
						 << getSpacingModeShaderName(caseDef.spacingMode) << ") in;\n"
			<< "\n"
			<< getTessLevelsSSBODeclaration()
			<< "\n"
			<< "layout(location = 0) in  highp vec2 in_te_position[];\n"
			<< "layout(location = 0) out highp vec4 in_f_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<<	(caseDef.primitiveType == TESSPRIMITIVETYPE_TRIANGLES ?
					"    highp vec2 corner0 = in_te_position[0];\n"
					"    highp vec2 corner1 = in_te_position[1];\n"
					"    highp vec2 corner2 = in_te_position[2];\n"
					"    highp vec2 pos =  corner0*gl_TessCoord.x + corner1*gl_TessCoord.y + corner2*gl_TessCoord.z;\n"
					"    gl_Position = vec4(pos, 0.0, 1.0);\n"
					"    highp int numConcentricTriangles = int(round(sb_levels.inner0)) / 2 + 1;\n"
					"    highp float d = 3.0 * min(gl_TessCoord.x, min(gl_TessCoord.y, gl_TessCoord.z));\n"
					"    highp int phase = int(d*float(numConcentricTriangles)) % 3;\n"
					"    in_f_color = phase == 0 ? vec4(1.0, 0.0, 0.0, 1.0)\n"
					"               : phase == 1 ? vec4(0.0, 1.0, 0.0, 1.0)\n"
					"               :              vec4(0.0, 0.0, 1.0, 1.0);\n"
				: caseDef.primitiveType == TESSPRIMITIVETYPE_QUADS ?
					"    highp vec2 corner0 = in_te_position[0];\n"
					"    highp vec2 corner1 = in_te_position[1];\n"
					"    highp vec2 corner2 = in_te_position[2];\n"
					"    highp vec2 corner3 = in_te_position[3];\n"
					"    highp vec2 pos = (1.0-gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner0\n"
					"                   + (    gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner1\n"
					"                   + (1.0-gl_TessCoord.x)*(    gl_TessCoord.y)*corner2\n"
					"                   + (    gl_TessCoord.x)*(    gl_TessCoord.y)*corner3;\n"
					"    gl_Position = vec4(pos, 0.0, 1.0);\n"
					"    highp int phaseX = int(round((0.5 - abs(gl_TessCoord.x-0.5)) * sb_levels.inner0));\n"
					"    highp int phaseY = int(round((0.5 - abs(gl_TessCoord.y-0.5)) * sb_levels.inner1));\n"
					"    highp int phase = min(phaseX, phaseY) % 3;\n"
					"    in_f_color = phase == 0 ? vec4(1.0, 0.0, 0.0, 1.0)\n"
					"               : phase == 1 ? vec4(0.0, 1.0, 0.0, 1.0)\n"
					"               :              vec4(0.0, 0.0, 1.0, 1.0);\n"
					: "")
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}
}

void initProgramsIsolinesCase (vk::SourceCollections& programCollection, const CaseDefinition caseDef)
{
	DE_ASSERT(caseDef.primitiveType == TESSPRIMITIVETYPE_ISOLINES);

	initCommonPrograms(programCollection, caseDef);

	// Tessellation evaluation shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(" << getTessPrimitiveTypeShaderName(caseDef.primitiveType) << ", "
						 << getSpacingModeShaderName(caseDef.spacingMode) << ") in;\n"
			<< "\n"
			<< getTessLevelsSSBODeclaration()
			<< "\n"
			<< "layout(location = 0) in  highp vec2 in_te_position[];\n"
			<< "layout(location = 0) out highp vec4 in_f_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    highp vec2 corner0 = in_te_position[0];\n"
			<< "    highp vec2 corner1 = in_te_position[1];\n"
			<< "    highp vec2 corner2 = in_te_position[2];\n"
			<< "    highp vec2 corner3 = in_te_position[3];\n"
			<< "    highp vec2 pos = (1.0-gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner0\n"
			<< "                   + (    gl_TessCoord.x)*(1.0-gl_TessCoord.y)*corner1\n"
			<< "                   + (1.0-gl_TessCoord.x)*(    gl_TessCoord.y)*corner2\n"
			<< "                   + (    gl_TessCoord.x)*(    gl_TessCoord.y)*corner3;\n"
			<< "    pos.y += 0.15*sin(gl_TessCoord.x*10.0);\n"
			<< "    gl_Position = vec4(pos, 0.0, 1.0);\n"
			<< "    highp int phaseX = int(round(gl_TessCoord.x*sb_levels.outer1));\n"
			<< "    highp int phaseY = int(round(gl_TessCoord.y*sb_levels.outer0));\n"
			<< "    highp int phase = (phaseX + phaseY) % 3;\n"
			<< "    in_f_color = phase == 0 ? vec4(1.0, 0.0, 0.0, 1.0)\n"
			<< "               : phase == 1 ? vec4(0.0, 1.0, 0.0, 1.0)\n"
			<< "               :              vec4(0.0, 0.0, 1.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}
}

inline std::string getReferenceImagePathPrefix (const std::string& caseName)
{
	return "vulkan/data/tessellation/" + caseName + "_ref";
}

struct TessStateSwitchParams
{
	const std::pair<TessPrimitiveType, TessPrimitiveType>					patchTypes;
	const std::pair<SpacingMode, SpacingMode>								spacing;
	const std::pair<VkTessellationDomainOrigin, VkTessellationDomainOrigin>	domainOrigin;
	const bool																geometryShader;

	bool nonDefaultDomainOrigin (void) const
	{
		return (domainOrigin.first != VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT ||
				domainOrigin.second != VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);
	}
};

class TessStateSwitchInstance : public vkt::TestInstance
{
public:
						TessStateSwitchInstance		(Context& context, const TessStateSwitchParams& params)
							: vkt::TestInstance	(context)
							, m_params			(params)
							{}

	virtual				~TessStateSwitchInstance	(void) {}

	tcu::TestStatus		iterate						(void);

protected:
	const TessStateSwitchParams m_params;
};

class TessStateSwitchCase : public vkt::TestCase
{
public:
					TessStateSwitchCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TessStateSwitchParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}

	virtual			~TessStateSwitchCase	(void) {}

	void			checkSupport			(Context& context) const;
	void			initPrograms			(vk::SourceCollections& programCollection) const;
	TestInstance*	createInstance			(Context& context) const { return new TessStateSwitchInstance(context, m_params); }

protected:
	const TessStateSwitchParams m_params;
};

void TessStateSwitchCase::checkSupport (Context& context) const
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (m_params.geometryShader)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (m_params.nonDefaultDomainOrigin())
		context.requireDeviceFunctionality("VK_KHR_maintenance2");
}

void TessStateSwitchCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream vert;
	vert
		<< "#version 460\n"
		<< "layout (location=0) in vec4 inPos;\n"
		<< "layout (push_constant, std430) uniform PushConstantBlock { vec2 offset; } pc;\n"
		<< "out gl_PerVertex\n"
		<< "{\n"
		<< "  vec4 gl_Position;\n"
		<< "};\n"
		<< "void main() {\n"
		<< "    gl_Position = inPos + vec4(pc.offset, 0.0, 0.0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

	if (m_params.geometryShader)
	{
		std::ostringstream geom;
		geom
			<< "#version 460\n"
			<< "layout (triangles) in;\n"
			<< "layout (triangle_strip, max_vertices=3) out;\n"
			<< "in gl_PerVertex\n"
			<< "{\n"
			<< "    vec4 gl_Position;\n"
			<< "} gl_in[3];\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "void main() {\n"
			<< "    gl_Position    = gl_in[0].gl_Position; EmitVertex();\n"
			<< "    gl_Position    = gl_in[1].gl_Position; EmitVertex();\n"
			<< "    gl_Position    = gl_in[2].gl_Position; EmitVertex();\n"
			<< "    gl_PrimitiveID = gl_PrimitiveIDIn;     EndPrimitive();\n"
			<< "}\n"
			;
		programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
	}

	const auto even			= (m_params.spacing.second == SPACINGMODE_FRACTIONAL_EVEN);
	const auto extraLevel	= (even ? "1.0" : "0.0");

	std::ostringstream tesc;
	tesc
		<< "#version 460\n"
		<< "layout (vertices=4) out;\n"
		<< "in gl_PerVertex\n"
		<< "{\n"
		<< "  vec4 gl_Position;\n"
		<< "} gl_in[gl_MaxPatchVertices];\n"
		<< "out gl_PerVertex\n"
		<< "{\n"
		<< "  vec4 gl_Position;\n"
		<< "} gl_out[];\n"
		<< "void main() {\n"
		<< "    const float extraLevel = " << extraLevel << ";\n"
		<< "    gl_TessLevelInner[0] = 10.0 + extraLevel;\n"
		<< "    gl_TessLevelInner[1] = 10.0 + extraLevel;\n"
		<< "    gl_TessLevelOuter[0] = 50.0 + extraLevel;\n"
		<< "    gl_TessLevelOuter[1] = 40.0 + extraLevel;\n"
		<< "    gl_TessLevelOuter[2] = 30.0 + extraLevel;\n"
		<< "    gl_TessLevelOuter[3] = 20.0 + extraLevel;\n"
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());

	for (uint32_t i = 0u; i < 2u; ++i)
	{
		const auto& primType	= ((i == 0u) ? m_params.patchTypes.first : m_params.patchTypes.second);
		const auto& spacing		= ((i == 0u) ? m_params.spacing.first : m_params.spacing.second);

		std::ostringstream tese;
		tese
			<< "#version 460\n"
			<< "layout (" << getTessPrimitiveTypeShaderName(primType) << ", " << getSpacingModeShaderName(spacing) << ", ccw) in;\n"
			<< "in gl_PerVertex\n"
			<< "{\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_in[gl_MaxPatchVertices];\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "  vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "// This assumes 2D, calculates barycentrics for point p inside triangle (a, b, c)\n"
			<< "vec3 calcBaryCoords(vec2 p, vec2 a, vec2 b, vec2 c)\n"
			<< "{\n"
			<< "    const vec2 v0 = b - a;\n"
			<< "    const vec2 v1 = c - a;\n"
			<< "    const vec2 v2 = p - a;\n"
			<< "\n"
			<< "    const float den = v0.x * v1.y - v1.x * v0.y;\n"
			<< "    const float v   = (v2.x * v1.y - v1.x * v2.y) / den;\n"
			<< "    const float w   = (v0.x * v2.y - v2.x * v0.y) / den;\n"
			<< "    const float u   = 1.0 - v - w;\n"
			<< "\n"
			<< "    return vec3(u, v, w);\n"
			<< "}\n"
			<< "\n"
			<< "void main() {\n"
			<< ((primType == TESSPRIMITIVETYPE_QUADS)
				// For quads.
				?	"    const float u = gl_TessCoord.x;\n"
					"    const float v = gl_TessCoord.y;\n"
					"    gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position + (1 - u) * v * gl_in[1].gl_Position + u * (1 - v) * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position;\n"
				// For triangles.
				:	"    // We have a patch with 4 corners (v0,v1,v2,v3), but triangle-based tessellation.\n"
					"    // Lets suppose the triangle covers half the patch (triangle v0,v2,v1).\n"
					"    // Expand the triangle by virtually grabbing it from the midpoint between v1 and v2 (which should fall in the middle of the patch) and stretching that point to the fourth corner (v3).\n"
					"    const vec4 origpoint = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
					"                           (gl_TessCoord.y * gl_in[2].gl_Position) +\n"
					"                           (gl_TessCoord.z * gl_in[1].gl_Position);\n"
					"    const vec4 midpoint = 0.5 * gl_in[1].gl_Position + 0.5 * gl_in[2].gl_Position;\n"
					"\n"
					"    // Find out if it falls on left or right side of the triangle.\n"
					"    vec4 halfTriangle[3];\n"
					"    vec4 stretchedHalf[3];\n"
					"\n"
					"    if (gl_TessCoord.z >= gl_TessCoord.y)\n"
					"    {\n"
					"        halfTriangle[0] = gl_in[0].gl_Position;\n"
					"        halfTriangle[1] = midpoint;\n"
					"        halfTriangle[2] = gl_in[1].gl_Position;\n"
					"\n"
					"        stretchedHalf[0] = gl_in[0].gl_Position;\n"
					"        stretchedHalf[1] = gl_in[3].gl_Position;\n"
					"        stretchedHalf[2] = gl_in[1].gl_Position;\n"
					"    }\n"
					"    else\n"
					"    {\n"
					"        halfTriangle[0] = gl_in[0].gl_Position;\n"
					"        halfTriangle[1] = gl_in[2].gl_Position;\n"
					"        halfTriangle[2] = midpoint;\n"
					"\n"
					"        stretchedHalf[0] = gl_in[0].gl_Position;\n"
					"        stretchedHalf[1] = gl_in[2].gl_Position;\n"
					"        stretchedHalf[2] = gl_in[3].gl_Position;\n"
					"    }\n"
					"\n"
					"    // Calculate the barycentric coordinates for the left or right sides.\n"
					"    vec3 sideBaryCoord = calcBaryCoords(origpoint.xy, halfTriangle[0].xy, halfTriangle[1].xy, halfTriangle[2].xy);\n"
					"\n"
					"    // Move the point by stretching the half triangle and dragging the midpoint vertex to v3.\n"
					"    gl_Position = sideBaryCoord.x * stretchedHalf[0] + sideBaryCoord.y * stretchedHalf[1] + sideBaryCoord.z * stretchedHalf[2];\n"
				)
			<< "}\n"
			;
		programCollection.glslSources.add("tese" + std::to_string(i)) << glu::TessellationEvaluationSource(tese.str());
	}

	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(0.5, 0.5, 0.5, 1.0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus TessStateSwitchInstance::iterate (void)
{
	const auto&			ctx				= m_context.getContextCommonData();
	const tcu::IVec3	fbExtent		(128, 128, 1);
	const auto			vkExtent		= makeExtent3D(fbExtent);
	const auto			colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuFormat		= mapVkFormat(colorFormat);
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			imageType		= VK_IMAGE_TYPE_2D;
	const auto			colorSRR		= makeDefaultImageSubresourceRange();
	const auto			bindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;

	ImageWithBuffer referenceBuffer	(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType, colorSRR);
	ImageWithBuffer resultBuffer	(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType, colorSRR);

	// Vertex buffer containing a single full-screen patch.
	const std::vector<tcu::Vec4> vertices
	{
		tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f),
		tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f),
		tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f),
	};
	const auto vertexCount			= de::sizeU32(vertices);
	const auto patchControlPoints	= vertexCount;

	const auto			vertexBufferSize	= static_cast<VkDeviceSize>(de::dataSize(vertices));
	const auto			vertexBufferInfo	= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	BufferWithMemory	vertexBuffer		(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
	auto&				vertexBufferAlloc	= vertexBuffer.getAllocation();
	void*				vertexBufferData	= vertexBufferAlloc.getHostPtr();
	const auto			vertexBufferOffset	= static_cast<VkDeviceSize>(0);

	deMemcpy(vertexBufferData, de::dataOrNull(vertices), de::dataSize(vertices));
	flushAlloc(ctx.vkd, ctx.device, vertexBufferAlloc);

	const auto pcSize	= static_cast<uint32_t>(sizeof(tcu::Vec2));
	const auto pcStages	= static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT);
	const auto pcRange	= makePushConstantRange(pcStages, 0u, pcSize);

	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

	const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat);

	// Framebuffers.
	const auto framebuffer0 = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, referenceBuffer.getImageView(), vkExtent.width, vkExtent.height);
	const auto framebuffer1 = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, resultBuffer.getImageView(), vkExtent.width, vkExtent.height);

	// Viewport and scissor.
	const std::vector<VkViewport>	viewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	// Shaders.
	const auto&		binaries	= m_context.getBinaryCollection();
	const auto		vertModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto		tescModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("tesc"));
	const auto		teseModule0	= createShaderModule(ctx.vkd, ctx.device, binaries.get("tese0"));
	const auto		teseModule1	= createShaderModule(ctx.vkd, ctx.device, binaries.get("tese1"));
	const auto		geomModule	= (m_params.geometryShader ? createShaderModule(ctx.vkd, ctx.device, binaries.get("geom")) : Move<VkShaderModule>());
	const auto		fragModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,								//	VkPrimitiveTopology						topology;
		VK_FALSE,														//	VkBool32								primitiveRestartEnable;
	};

	VkPipelineTessellationDomainOriginStateCreateInfo domainOriginStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,	//	VkStructureType				sType;
		nullptr,																	//	const void*					pNext;
		m_params.domainOrigin.first,													//	VkTessellationDomainOrigin	domainOrigin;
	};

	const auto tessPNext = (m_params.nonDefaultDomainOrigin() ? &domainOriginStateCreateInfo : nullptr);
	const VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		tessPNext,													//	const void*								pNext;
		0u,															//	VkPipelineTessellationStateCreateFlags	flags;
		patchControlPoints,											//	uint32_t								patchControlPoints;
	};

	const VkPipelineViewportStateCreateInfo viewportStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineViewportStateCreateFlags	flags;
		de::sizeU32(viewports),									//	uint32_t							viewportCount;
		de::dataOrNull(viewports),								//	const VkViewport*					pViewports;
		de::sizeU32(scissors),									//	uint32_t							scissorCount;
		de::dataOrNull(scissors),								//	const VkRect2D*						pScissors;
	};

	// In the rasterization parameters, use wireframe mode to see each triangle if possible.
	// This makes the test harder to pass by mistake.
	// We also cull back faces, which will help test domain origin.
	// The front face changes with the domain origin.
	const auto frontFace	= ((m_params.domainOrigin.second == VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT)
							? VK_FRONT_FACE_COUNTER_CLOCKWISE	// With the default value it's as specified in the shader.
							: VK_FRONT_FACE_CLOCKWISE);			// Otherwise the winding order changes.
	const auto polygonMode	= ((m_context.getDeviceFeatures().fillModeNonSolid)
							? VK_POLYGON_MODE_LINE
							: VK_POLYGON_MODE_FILL);
	const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,													//	VkBool32								depthClampEnable;
		VK_FALSE,													//	VkBool32								rasterizerDiscardEnable;
		polygonMode,												//	VkPolygonMode							polygonMode;
		VK_CULL_MODE_BACK_BIT,										//	VkCullModeFlags							cullMode;
		frontFace,													//	VkFrontFace								frontFace;
		VK_FALSE,													//	VkBool32								depthBiasEnable;
		0.0f,														//	float									depthBiasConstantFactor;
		0.0f,														//	float									depthBiasClamp;
		0.0f,														//	float									depthBiasSlopeFactor;
		1.0f,														//	float									lineWidth;
	};

	// Create two pipelines varying the tessellation evaluation module.
	const auto pipeline0 = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout,
		*vertModule, *tescModule, *teseModule0, *geomModule, *fragModule,
		*renderPass, 0u, nullptr, &inputAssemblyStateCreateInfo, &tessellationStateCreateInfo, &viewportStateCreateInfo,
		&rasterizationStateCreateInfo);

	domainOriginStateCreateInfo.domainOrigin = m_params.domainOrigin.second;

	const auto pipeline1 = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout,
		*vertModule, *tescModule, *teseModule1, *geomModule, *fragModule,
		*renderPass, 0u, nullptr, &inputAssemblyStateCreateInfo, &tessellationStateCreateInfo, &viewportStateCreateInfo,
		&rasterizationStateCreateInfo);

	const auto cmdPool = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBufferRef = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBufferRes = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const tcu::Vec2 noOffset		(0.0f, 0.0f);
	const tcu::Vec2 offscreenOffset	(50.0f, 50.0f);
	const tcu::Vec4 clearColor		(0.0f, 0.0f, 0.0f, 1.0f);

	// Reference image.
	beginCommandBuffer(ctx.vkd, *cmdBufferRef);
	beginRenderPass(ctx.vkd, *cmdBufferRef, *renderPass, *framebuffer0, scissors.at(0u), clearColor);
	ctx.vkd.cmdBindVertexBuffers(*cmdBufferRef, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	ctx.vkd.cmdBindPipeline(*cmdBufferRef, bindPoint, *pipeline1);
	ctx.vkd.cmdPushConstants(*cmdBufferRef, *pipelineLayout, pcStages, 0u, pcSize, &noOffset);
	ctx.vkd.cmdDraw(*cmdBufferRef, vertexCount, 1u, 0u, 0u);
	endRenderPass(ctx.vkd, *cmdBufferRef);
	copyImageToBuffer(ctx.vkd, *cmdBufferRef, referenceBuffer.getImage(), referenceBuffer.getBuffer(), fbExtent.swizzle(0, 1),
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	endCommandBuffer(ctx.vkd, *cmdBufferRef);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, *cmdBufferRef);

	// Result image.
	beginCommandBuffer(ctx.vkd, *cmdBufferRes);
	beginRenderPass(ctx.vkd, *cmdBufferRes, *renderPass, *framebuffer1, scissors.at(0u), clearColor);
	ctx.vkd.cmdBindVertexBuffers(*cmdBufferRes, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	// Draw offscreen first to force tessellation state emission.
	ctx.vkd.cmdBindPipeline(*cmdBufferRes, bindPoint, *pipeline0);
	ctx.vkd.cmdPushConstants(*cmdBufferRes, *pipelineLayout, pcStages, 0u, pcSize, &offscreenOffset);
	ctx.vkd.cmdDraw(*cmdBufferRes, vertexCount, 1u, 0u, 0u);
	// Draw on screen second changing some tessellation state.
	ctx.vkd.cmdBindPipeline(*cmdBufferRes, bindPoint, *pipeline1);
	ctx.vkd.cmdPushConstants(*cmdBufferRes, *pipelineLayout, pcStages, 0u, pcSize, &noOffset);
	ctx.vkd.cmdDraw(*cmdBufferRes, vertexCount, 1u, 0u, 0u);
	endRenderPass(ctx.vkd, *cmdBufferRes);
	copyImageToBuffer(ctx.vkd, *cmdBufferRes, resultBuffer.getImage(), resultBuffer.getBuffer(), fbExtent.swizzle(0, 1),
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	endCommandBuffer(ctx.vkd, *cmdBufferRes);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, *cmdBufferRes);

	invalidateAlloc(ctx.vkd, ctx.device, referenceBuffer.getBufferAllocation());
	invalidateAlloc(ctx.vkd, ctx.device, resultBuffer.getBufferAllocation());

	tcu::ConstPixelBufferAccess referenceAccess	(tcuFormat, fbExtent, referenceBuffer.getBufferAllocation().getHostPtr());
	tcu::ConstPixelBufferAccess resultAccess	(tcuFormat, fbExtent, resultBuffer.getBufferAllocation().getHostPtr());

	auto&			log				= m_context.getTestContext().getLog();
	const float		threshold		= 0.005f; // 1/255 < 0.005 < 2/255
	const tcu::Vec4	thresholdVec	(threshold, threshold, threshold, 0.0f);

	if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, thresholdVec, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Color result does not match reference image -- check log for details");

	// Render pass and framebuffers.const DeviceCoreFeature requiredDeviceCoreFeature
	return tcu::TestStatus::pass("Pass");
}

std::string getDomainOriginName(VkTessellationDomainOrigin value)
{
	static const size_t	prefixLen	= strlen("VK_TESSELLATION_DOMAIN_ORIGIN_");
	std::string			nameStr		= getTessellationDomainOriginName(value);

	return de::toLower(nameStr.substr(prefixLen));
}

} // anonymous

//! These tests correspond to dEQP-GLES31.functional.tessellation.misc_draw.*
tcu::TestCaseGroup* createMiscDrawTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "misc_draw", "Miscellaneous draw-result-verifying cases"));

	static const TessPrimitiveType primitivesNoIsolines[] =
	{
		TESSPRIMITIVETYPE_TRIANGLES,
		TESSPRIMITIVETYPE_QUADS,
	};

	// Triangle fill case
	for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitivesNoIsolines); ++primitiveTypeNdx)
	for (int spacingModeNdx = 0; spacingModeNdx < SPACINGMODE_LAST; ++spacingModeNdx)
	{
		const TessPrimitiveType primitiveType = primitivesNoIsolines[primitiveTypeNdx];
		const SpacingMode		spacingMode	  = static_cast<SpacingMode>(spacingModeNdx);
		const std::string		caseName	  = std::string() + "fill_cover_" + getTessPrimitiveTypeShaderName(primitiveType) + "_" + getSpacingModeShaderName(spacingMode);

		addFunctionCaseWithPrograms(group.get(), caseName, "Check that there are no obvious gaps in the triangle-filled area of a tessellated shape",
									initProgramsFillCoverCase, runTest, makeCaseDefinition(primitiveType, spacingMode, getReferenceImagePathPrefix(caseName)));
	}

	// Triangle non-overlap case
	for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitivesNoIsolines); ++primitiveTypeNdx)
	for (int spacingModeNdx = 0; spacingModeNdx < SPACINGMODE_LAST; ++spacingModeNdx)
	{
		const TessPrimitiveType primitiveType = primitivesNoIsolines[primitiveTypeNdx];
		const SpacingMode		spacingMode	  = static_cast<SpacingMode>(spacingModeNdx);
		const std::string		caseName	  = std::string() + "fill_overlap_" + getTessPrimitiveTypeShaderName(primitiveType) + "_" + getSpacingModeShaderName(spacingMode);

		addFunctionCaseWithPrograms(group.get(), caseName, "Check that there are no obvious triangle overlaps in the triangle-filled area of a tessellated shape",
									initProgramsFillNonOverlapCase, runTest, makeCaseDefinition(primitiveType, spacingMode, getReferenceImagePathPrefix(caseName)));
	}

	// Isolines
	for (int spacingModeNdx = 0; spacingModeNdx < SPACINGMODE_LAST; ++spacingModeNdx)
	{
		const SpacingMode spacingMode = static_cast<SpacingMode>(spacingModeNdx);
		const std::string caseName    = std::string() + "isolines_" + getSpacingModeShaderName(spacingMode);

		addFunctionCaseWithPrograms(group.get(), caseName, "Basic isolines render test", checkSupportCase,
									initProgramsIsolinesCase, runTest, makeCaseDefinition(TESSPRIMITIVETYPE_ISOLINES, spacingMode, getReferenceImagePathPrefix(caseName)));
	}

	// Test switching tessellation parameters on the fly.
	for (const auto& geometryShader : { false, true })
	{
		const auto nameSuffix = (geometryShader ? "_with_geom_shader" : "");

		static const VkTessellationDomainOrigin domainOrigins[] =
		{
			VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT,
			VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT,
		};

		for (const auto& firstPrimitiveType : primitivesNoIsolines)
			for (const auto& secondPrimitiveType : primitivesNoIsolines)
			{
				if (firstPrimitiveType == secondPrimitiveType)
					continue;

				const TessStateSwitchParams params
				{
					std::make_pair(firstPrimitiveType, secondPrimitiveType),
					std::make_pair(SPACINGMODE_EQUAL, SPACINGMODE_EQUAL),
					std::make_pair(VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT, VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT),
					geometryShader,
				};

				const auto testName = std::string("switch_primitive_") + getTessPrimitiveTypeShaderName(params.patchTypes.first) + "_to_" + getTessPrimitiveTypeShaderName(params.patchTypes.second) + nameSuffix;
				group->addChild(new TessStateSwitchCase(testCtx, testName, "", params));
			}

		for (const auto& firstDomainOrigin : domainOrigins)
			for (const auto& secondDomainOrigin : domainOrigins)
			{
				if (firstDomainOrigin == secondDomainOrigin)
					continue;

				const TessStateSwitchParams params
				{
					std::make_pair(TESSPRIMITIVETYPE_QUADS, TESSPRIMITIVETYPE_QUADS),
					std::make_pair(SPACINGMODE_EQUAL, SPACINGMODE_EQUAL),
					std::make_pair(firstDomainOrigin, secondDomainOrigin),
					geometryShader,
				};

				const auto testName = std::string("switch_domain_origin_") + getDomainOriginName(params.domainOrigin.first) + "_to_" + getDomainOriginName(params.domainOrigin.second) + nameSuffix;
				group->addChild(new TessStateSwitchCase(testCtx, testName, "", params));
			}

		for (int firstSpacingModeNdx = 0; firstSpacingModeNdx < SPACINGMODE_LAST; ++firstSpacingModeNdx)
			for (int secondSpacingModeNdx = 0; secondSpacingModeNdx < SPACINGMODE_LAST; ++secondSpacingModeNdx)
			{
				if (firstSpacingModeNdx == secondSpacingModeNdx)
					continue;

				const SpacingMode firstSpacingMode	= static_cast<SpacingMode>(firstSpacingModeNdx);
				const SpacingMode secondSpacingMode	= static_cast<SpacingMode>(secondSpacingModeNdx);

				const TessStateSwitchParams params
				{
					std::make_pair(TESSPRIMITIVETYPE_QUADS, TESSPRIMITIVETYPE_QUADS),
					std::make_pair(firstSpacingMode, secondSpacingMode),
					std::make_pair(VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT, VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT),
					geometryShader,
				};

				const auto testName = std::string("switch_spacing_mode_") + getSpacingModeShaderName(params.spacing.first) + "_to_" + getSpacingModeShaderName(params.spacing.second) + nameSuffix;
				group->addChild(new TessStateSwitchCase(testCtx, testName, "", params));
			}
	}

	return group.release();
}

} // tessellation
} // vkt
