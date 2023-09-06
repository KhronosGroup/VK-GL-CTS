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
 * \brief Tessellation Shader Input/Output Tests
 *//*--------------------------------------------------------------------*/

#include "vktTessellationShaderInputOutputTests.hpp"
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

namespace vkt
{
namespace tessellation
{

using namespace vk;

namespace
{

enum Constants
{
	RENDER_SIZE = 256,
};

//! Generic test code used by all test cases.
tcu::TestStatus runTest (Context&							context,
						 const int							numPrimitives,
						 const int							inPatchSize,
						 const int							outPatchSize,
						 const VkFormat						vertexFormat,
						 const void*						vertexData,
						 const VkDeviceSize					vertexDataSizeBytes,
						 const tcu::ConstPixelBufferAccess&	referenceImageAccess)
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_TESSELLATION_SHADER);

	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			device				= context.getDevice();
	const VkQueue			queue				= context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= context.getDefaultAllocator();

	// Vertex input: may be just some abstract numbers

	const BufferWithMemory vertexBuffer(vk, device, allocator,
		makeBufferCreateInfo(vertexDataSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible);

	{
		const Allocation& alloc = vertexBuffer.getAllocation();

		deMemcpy(alloc.getHostPtr(), vertexData, static_cast<std::size_t>(vertexDataSizeBytes));
		flushAlloc(vk, device, alloc);
		// No barrier needed, flushed memory is automatically visible
	}

	// Color attachment

	const tcu::IVec2			  renderSize				 = tcu::IVec2(RENDER_SIZE, RENDER_SIZE);
	const VkFormat				  colorFormat				 = VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageSubresourceRange colorImageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const ImageWithMemory		  colorAttachmentImage		 (vk, device, allocator,
															 makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 1u),
															 MemoryRequirement::Any);

	// Color output buffer: image will be copied here for verification

	const VkDeviceSize		colorBufferSizeBytes = renderSize.x()*renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
	const BufferWithMemory	colorBuffer			 (vk, device, allocator, makeBufferCreateInfo(colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);

	// Pipeline

	const Unique<VkImageView>		colorAttachmentView	(makeImageView			(vk, device, *colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorImageSubresourceRange));
	const Unique<VkRenderPass>		renderPass			(makeRenderPass			(vk, device, colorFormat));
	const Unique<VkFramebuffer>		framebuffer			(makeFramebuffer		(vk, device, *renderPass, *colorAttachmentView, renderSize.x(), renderSize.y()));
	const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout		(vk, device));
	const Unique<VkCommandPool>		cmdPool				(makeCommandPool		(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const Unique<VkPipeline> pipeline(GraphicsPipelineBuilder()
		.setRenderSize				  (renderSize)
		.setVertexInputSingleAttribute(vertexFormat, tcu::getPixelSize(mapVkFormat(vertexFormat)))
		.setPatchControlPoints		  (inPatchSize)
		.setShader					  (vk, device, VK_SHADER_STAGE_VERTEX_BIT,					context.getBinaryCollection().get("vert"), DE_NULL)
		.setShader					  (vk, device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,	context.getBinaryCollection().get("tesc"), DE_NULL)
		.setShader					  (vk, device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, context.getBinaryCollection().get("tese"), DE_NULL)
		.setShader					  (vk, device, VK_SHADER_STAGE_FRAGMENT_BIT,				context.getBinaryCollection().get("frag"), DE_NULL)
		.build						  (vk, device, *pipelineLayout, *renderPass));

	{
		tcu::TestLog& log = context.getTestContext().getLog();
		log << tcu::TestLog::Message
			<< "Note: input patch size is " << inPatchSize << ", output patch size is " << outPatchSize
			<< tcu::TestLog::EndMessage;
	}

	// Draw commands

	beginCommandBuffer(vk, *cmdBuffer);

	// Change color attachment image layout
	{
		const VkImageMemoryBarrier colorAttachmentLayoutBarrier = makeImageMemoryBarrier(
			(VkAccessFlags)0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			*colorAttachmentImage, colorImageSubresourceRange);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
			0u, DE_NULL, 0u, DE_NULL, 1u, &colorAttachmentLayoutBarrier);
	}

	// Begin render pass
	{
		const VkRect2D	renderArea	= makeRect2D(renderSize);
		const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 1.0f);

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);
	}

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	{
		const VkDeviceSize vertexBufferOffset = 0ull;
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	}

	// Process enough vertices to make a patch.
	vk.cmdDraw(*cmdBuffer, numPrimitives * inPatchSize, 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuffer);

	// Copy render result to a host-visible buffer
	copyImageToBuffer(vk, *cmdBuffer, *colorAttachmentImage, *colorBuffer, renderSize);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	{
		const Allocation&					colorBufferAlloc	= colorBuffer.getAllocation();

		invalidateAlloc(vk, device, colorBufferAlloc);

		// Verify case result
		const tcu::ConstPixelBufferAccess	resultImageAccess	(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAlloc.getHostPtr());
		tcu::TestLog&						log					= context.getTestContext().getLog();
		const bool							ok					= tcu::fuzzyCompare(log, "ImageComparison", "Image Comparison", referenceImageAccess, resultImageAccess,
																  0.002f, tcu::COMPARE_LOG_RESULT);

		return (ok ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Failure"));
	}
}

//! Resize an image and fill with white color.
void initializeWhiteReferenceImage (tcu::TextureLevel& image, const int width, const int height)
{
	DE_ASSERT(width > 0 && height > 0);

	image.setStorage(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), width, height);
	tcu::PixelBufferAccess access = image.getAccess();

	const tcu::Vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

	for (int y = 0; y < height; ++y)
	for (int x = 0; x < width; ++x)
		access.setPixel(white, x, y);
}

namespace PatchVertexCount
{

struct CaseDefinition
{
	int				inPatchSize;
	int				outPatchSize;
	std::string		referenceImagePath;
};

void initPrograms (vk::SourceCollections& programCollection, const CaseDefinition caseDef)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  highp float in_v_attr;\n"
			<< "layout(location = 0) out highp float in_tc_attr;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_tc_attr = in_v_attr;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation control shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(vertices = " << caseDef.outPatchSize << ") out;\n"
			<< "\n"
			<< "layout(location = 0) in  highp float in_tc_attr[];\n"
			<< "layout(location = 0) out highp float in_te_attr[];\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_te_attr[gl_InvocationID] = in_tc_attr[gl_InvocationID*" << caseDef.inPatchSize << "/" << caseDef.outPatchSize << "];\n"
			<< "\n"
			<< "    gl_TessLevelInner[0] = 5.0;\n"
			<< "    gl_TessLevelInner[1] = 5.0;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 5.0;\n"
			<< "    gl_TessLevelOuter[1] = 5.0;\n"
			<< "    gl_TessLevelOuter[2] = 5.0;\n"
			<< "    gl_TessLevelOuter[3] = 5.0;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	// Tessellation evaluation shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(" << getTessPrimitiveTypeShaderName(TESSPRIMITIVETYPE_QUADS) << ") in;\n"
			<< "\n"
			<< "layout(location = 0) in  highp   float in_te_attr[];\n"
			<< "layout(location = 0) out mediump vec4  in_f_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    highp float x = gl_TessCoord.x*2.0 - 1.0;\n"
			<< "    highp float y = gl_TessCoord.y - in_te_attr[int(round(gl_TessCoord.x*float(" << caseDef.outPatchSize << "-1)))];\n"
			<< "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
			<< "    in_f_color = vec4(1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  mediump vec4 in_f_color;\n"
			<< "layout(location = 0) out mediump vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = in_f_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus test (Context& context, const CaseDefinition caseDef)
{
	// Input vertex attribute data
	std::vector<float> vertexData;
	vertexData.reserve(caseDef.inPatchSize);
	for (int i = 0; i < caseDef.inPatchSize; ++i)
	{
		const float f = static_cast<float>(i) / static_cast<float>(caseDef.inPatchSize - 1);
		vertexData.push_back(f*f);
	}
	const VkDeviceSize vertexBufferSize = sizeof(float) * vertexData.size();

	// Load reference image
	tcu::TextureLevel referenceImage;
	tcu::ImageIO::loadPNG(referenceImage, context.getTestContext().getArchive(), caseDef.referenceImagePath.c_str());

	const int numPrimitives = 1;

	return runTest(context, numPrimitives, caseDef.inPatchSize, caseDef.outPatchSize,
				   VK_FORMAT_R32_SFLOAT, &vertexData[0], vertexBufferSize, referenceImage.getAccess());
}

} // PatchVertexCountTest ns

namespace PerPatchData
{

enum CaseType
{
	CASETYPE_PRIMITIVE_ID_TCS,
	CASETYPE_PRIMITIVE_ID_TES,
	CASETYPE_PATCH_VERTICES_IN_TCS,
	CASETYPE_PATCH_VERTICES_IN_TES,
	CASETYPE_TESS_LEVEL_INNER0_TES,
	CASETYPE_TESS_LEVEL_INNER1_TES,
	CASETYPE_TESS_LEVEL_OUTER0_TES,
	CASETYPE_TESS_LEVEL_OUTER1_TES,
	CASETYPE_TESS_LEVEL_OUTER2_TES,
	CASETYPE_TESS_LEVEL_OUTER3_TES,
};

enum Constants
{
	OUTPUT_PATCH_SIZE	= 5,
	INPUT_PATCH_SIZE	= 10,
};

struct CaseDefinition
{
	CaseType		caseType;
	std::string		caseName;
	bool			usesReferenceImageFromFile;
	std::string		referenceImagePath;
	std::string		caseDescription;
};

int getNumPrimitives (const CaseType type)
{
	return (type == CASETYPE_PRIMITIVE_ID_TCS || type == CASETYPE_PRIMITIVE_ID_TES ? 8 : 1);
}

void initPrograms (vk::SourceCollections& programCollection, const CaseDefinition caseDef)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  highp float in_v_attr;\n"
			<< "layout(location = 0) out highp float in_tc_attr;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_tc_attr = in_v_attr;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation control shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(vertices = " << OUTPUT_PATCH_SIZE << ") out;\n"
			<< "\n"
			<< "layout(location = 0) in  highp float in_tc_attr[];\n"
			<< "layout(location = 0) out highp float in_te_attr[];\n"
			<< "\n"
			<< (caseDef.caseType == CASETYPE_PRIMITIVE_ID_TCS	   ? "layout(location = 1) patch out mediump int in_te_primitiveIDFromTCS;\n" :
				caseDef.caseType == CASETYPE_PATCH_VERTICES_IN_TCS ? "layout(location = 1) patch out mediump int in_te_patchVerticesInFromTCS;\n" : "")
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_te_attr[gl_InvocationID] = in_tc_attr[gl_InvocationID];\n"
			<< (caseDef.caseType == CASETYPE_PRIMITIVE_ID_TCS	   ? "    in_te_primitiveIDFromTCS = gl_PrimitiveID;\n" :
				caseDef.caseType == CASETYPE_PATCH_VERTICES_IN_TCS ? "    in_te_patchVerticesInFromTCS = gl_PatchVerticesIn;\n" : "")
			<< "\n"
			<< "    gl_TessLevelInner[0] = 9.0;\n"
			<< "    gl_TessLevelInner[1] = 8.0;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 7.0;\n"
			<< "    gl_TessLevelOuter[1] = 6.0;\n"
			<< "    gl_TessLevelOuter[2] = 5.0;\n"
			<< "    gl_TessLevelOuter[3] = 4.0;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	// Tessellation evaluation shader
	{
		const float xScale = 1.0f / static_cast<float>(getNumPrimitives(caseDef.caseType));

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(" << getTessPrimitiveTypeShaderName(TESSPRIMITIVETYPE_QUADS) << ") in;\n"
			<< "\n"
			<< "layout(location = 0) in  highp   float in_te_attr[];\n"
			<< "layout(location = 0) out mediump vec4  in_f_color;\n"
			<< "\n"
			<< (caseDef.caseType == CASETYPE_PRIMITIVE_ID_TCS	   ? "layout(location = 1) patch in mediump int in_te_primitiveIDFromTCS;\n" :
				caseDef.caseType == CASETYPE_PATCH_VERTICES_IN_TCS ? "layout(location = 1) patch in mediump int in_te_patchVerticesInFromTCS;\n" : "")
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    highp float x = (gl_TessCoord.x*float(" << xScale << ") + in_te_attr[0]) * 2.0 - 1.0;\n"
			<< "    highp float y = gl_TessCoord.y*2.0 - 1.0;\n"
			<< "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
			<< (caseDef.caseType == CASETYPE_PRIMITIVE_ID_TCS		? "    bool ok = in_te_primitiveIDFromTCS == 3;\n" :
				caseDef.caseType == CASETYPE_PRIMITIVE_ID_TES		? "    bool ok = gl_PrimitiveID == 3;\n" :
				caseDef.caseType == CASETYPE_PATCH_VERTICES_IN_TCS	? "    bool ok = in_te_patchVerticesInFromTCS == " + de::toString(INPUT_PATCH_SIZE) + ";\n" :
				caseDef.caseType == CASETYPE_PATCH_VERTICES_IN_TES	? "    bool ok = gl_PatchVerticesIn == " + de::toString(OUTPUT_PATCH_SIZE) + ";\n" :
				caseDef.caseType == CASETYPE_TESS_LEVEL_INNER0_TES	? "    bool ok = abs(gl_TessLevelInner[0] - 9.0) < 0.1f;\n" :
				caseDef.caseType == CASETYPE_TESS_LEVEL_INNER1_TES	? "    bool ok = abs(gl_TessLevelInner[1] - 8.0) < 0.1f;\n" :
				caseDef.caseType == CASETYPE_TESS_LEVEL_OUTER0_TES	? "    bool ok = abs(gl_TessLevelOuter[0] - 7.0) < 0.1f;\n" :
				caseDef.caseType == CASETYPE_TESS_LEVEL_OUTER1_TES	? "    bool ok = abs(gl_TessLevelOuter[1] - 6.0) < 0.1f;\n" :
				caseDef.caseType == CASETYPE_TESS_LEVEL_OUTER2_TES	? "    bool ok = abs(gl_TessLevelOuter[2] - 5.0) < 0.1f;\n" :
				caseDef.caseType == CASETYPE_TESS_LEVEL_OUTER3_TES	? "    bool ok = abs(gl_TessLevelOuter[3] - 4.0) < 0.1f;\n" : "")
			<< "    in_f_color = ok ? vec4(1.0) : vec4(vec3(0.0), 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  mediump vec4 in_f_color;\n"
			<< "layout(location = 0) out mediump vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = in_f_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus test (Context& context, const CaseDefinition caseDef)
{
	DE_ASSERT(!caseDef.usesReferenceImageFromFile || !caseDef.referenceImagePath.empty());

	// Input vertex attribute data
	const int		   numPrimitives	= getNumPrimitives(caseDef.caseType);
	std::vector<float> vertexData		(INPUT_PATCH_SIZE * numPrimitives, 0.0f);
	const VkDeviceSize vertexBufferSize = sizeof(float) * vertexData.size();

	for (int i = 0; i < numPrimitives; ++i)
		vertexData[INPUT_PATCH_SIZE * i] = static_cast<float>(i) / static_cast<float>(numPrimitives);

	tcu::TextureLevel referenceImage;
	if (caseDef.usesReferenceImageFromFile)
		tcu::ImageIO::loadPNG(referenceImage, context.getTestContext().getArchive(), caseDef.referenceImagePath.c_str());
	else
		initializeWhiteReferenceImage(referenceImage, RENDER_SIZE, RENDER_SIZE);

	return runTest(context, numPrimitives, INPUT_PATCH_SIZE, OUTPUT_PATCH_SIZE,
				   VK_FORMAT_R32_SFLOAT, &vertexData[0], vertexBufferSize, referenceImage.getAccess());
}

} // PerPatchData ns

namespace GLPosition
{

enum CaseType
{
	CASETYPE_VS_TO_TCS = 0,
	CASETYPE_TCS_TO_TES,
	CASETYPE_VS_TO_TCS_TO_TES,
};

void initPrograms (vk::SourceCollections& programCollection, const CaseType caseType)
{
	const bool vsToTCS  = caseType == CASETYPE_VS_TO_TCS  || caseType == CASETYPE_VS_TO_TCS_TO_TES;
	const bool tcsToTES = caseType == CASETYPE_TCS_TO_TES || caseType == CASETYPE_VS_TO_TCS_TO_TES;

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  highp vec4 in_v_attr;\n"
			<< (!vsToTCS ? "layout(location = 0) out highp vec4 in_tc_attr;\n" : "")
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    " << (vsToTCS ? "gl_Position" : "in_tc_attr") << " = in_v_attr;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation control shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(vertices = 3) out;\n"
			<< "\n"
			<< (!vsToTCS  ? "layout(location = 0) in  highp vec4 in_tc_attr[];\n" : "")
			<< (!tcsToTES ? "layout(location = 0) out highp vec4 in_te_attr[];\n" : "")
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    " << (tcsToTES ? "gl_out[gl_InvocationID].gl_Position" : "in_te_attr[gl_InvocationID]") << " = "
					  << (vsToTCS  ? "gl_in[gl_InvocationID].gl_Position" : "in_tc_attr[gl_InvocationID]") << ";\n"
			<< "\n"
			<< "    gl_TessLevelInner[0] = 2.0;\n"
			<< "    gl_TessLevelInner[1] = 3.0;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 4.0;\n"
			<< "    gl_TessLevelOuter[1] = 5.0;\n"
			<< "    gl_TessLevelOuter[2] = 6.0;\n"
			<< "    gl_TessLevelOuter[3] = 7.0;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	// Tessellation evaluation shader
	{
		const std::string tesIn0 = tcsToTES ? "gl_in[0].gl_Position" : "in_te_attr[0]";
		const std::string tesIn1 = tcsToTES ? "gl_in[1].gl_Position" : "in_te_attr[1]";
		const std::string tesIn2 = tcsToTES ? "gl_in[2].gl_Position" : "in_te_attr[2]";

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(" << getTessPrimitiveTypeShaderName(TESSPRIMITIVETYPE_TRIANGLES) << ") in;\n"
			<< "\n"
			<< (!tcsToTES ? "layout(location = 0) in  highp vec4 in_te_attr[];\n" : "")
			<< "layout(location = 0) out highp vec4 in_f_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    highp vec2 xy = gl_TessCoord.x * " << tesIn0 << ".xy\n"
			<< "                  + gl_TessCoord.y * " << tesIn1 << ".xy\n"
			<< "                  + gl_TessCoord.z * " << tesIn2 << ".xy;\n"
			<< "    gl_Position = vec4(xy, 0.0, 1.0);\n"
			<< "    in_f_color = vec4(" << tesIn0 << ".z + " << tesIn1 << ".w,\n"
			<< "                      " << tesIn2 << ".z + " << tesIn0 << ".w,\n"
			<< "                      " << tesIn1 << ".z + " << tesIn2 << ".w,\n"
			<< "                      1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
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

tcu::TestStatus test (Context& context, const CaseType caseType)
{
	DE_UNREF(caseType);

	// Input vertex attribute data
	static const float vertexData[3*4] =
	{
		-0.8f, -0.7f, 0.1f, 0.7f,
		-0.5f,  0.4f, 0.2f, 0.5f,
		 0.3f,  0.2f, 0.3f, 0.45f
	};

	tcu::TextureLevel referenceImage;
	tcu::ImageIO::loadPNG(referenceImage, context.getTestContext().getArchive(), "vulkan/data/tessellation/gl_position_ref.png");

	const int numPrimitives = 1;
	const int inPatchSize   = 3;
	const int outPatchSize  = 3;

	return runTest(context, numPrimitives, inPatchSize, outPatchSize,
				   VK_FORMAT_R32G32B32A32_SFLOAT, vertexData, sizeof(vertexData), referenceImage.getAccess());
}

} // GLPosition ns

namespace Barrier
{

enum Constants
{
	NUM_VERTICES = 32,
};

void initPrograms (vk::SourceCollections& programCollection)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  highp float in_v_attr;\n"
			<< "layout(location = 0) out highp float in_tc_attr;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_tc_attr = in_v_attr;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation control shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(vertices = " << NUM_VERTICES << ") out;\n"
			<< "\n"
			<< "layout(location = 0) in  highp float in_tc_attr[];\n"
			<< "layout(location = 0) out highp float in_te_attr[];\n"
			<< "\n"
			<< "layout(location = 1) patch out highp float in_te_patchAttr;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_te_attr[gl_InvocationID] = in_tc_attr[gl_InvocationID];\n"
			<< "    in_te_patchAttr = 0.0f;\n"
			<< "\n"
			<< "    barrier();\n"
			<< "\n"
			<< "    if (gl_InvocationID == 5)\n"
			<< "		in_te_patchAttr = float(gl_InvocationID)*0.1;\n"
			<< "\n"
			<< "    barrier();\n"
			<< "\n"
			<< "    highp float temp = in_te_patchAttr + in_te_attr[gl_InvocationID];\n"
			<< "\n"
			<< "    barrier();\n"
			<< "\n"
			<< "    if (gl_InvocationID == " << NUM_VERTICES << "-1)\n"
			<< "		in_te_patchAttr = float(gl_InvocationID);\n"
			<< "\n"
			<< "    barrier();\n"
			<< "\n"
			<< "    in_te_attr[gl_InvocationID] = temp;\n"
			<< "\n"
			<< "    barrier();\n"
			<< "\n"
			<< "    temp = temp + in_te_attr[(gl_InvocationID+1) % " << NUM_VERTICES << "];\n"
			<< "\n"
			<< "    barrier();\n"
			<< "\n"
			<< "    in_te_attr[gl_InvocationID] = 0.25*temp;\n"
			<< "\n"
			<< "    gl_TessLevelInner[0] = 32.0;\n"
			<< "    gl_TessLevelInner[1] = 32.0;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 32.0;\n"
			<< "    gl_TessLevelOuter[1] = 32.0;\n"
			<< "    gl_TessLevelOuter[2] = 32.0;\n"
			<< "    gl_TessLevelOuter[3] = 32.0;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	// Tessellation evaluation shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(" << getTessPrimitiveTypeShaderName(TESSPRIMITIVETYPE_QUADS) << ") in;\n"
			<< "\n"
			<< "layout(location = 0) in       highp float in_te_attr[];\n"
			<< "layout(location = 1) patch in highp float in_te_patchAttr;\n"
			<< "\n"
			<< "layout(location = 0) out highp float in_f_blue;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    highp float x = gl_TessCoord.x*2.0 - 1.0;\n"
			<< "    highp float y = gl_TessCoord.y - in_te_attr[int(round(gl_TessCoord.x*float(" << NUM_VERTICES << "-1)))];\n"
			<< "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
			<< "    in_f_blue = abs(in_te_patchAttr - float(" << NUM_VERTICES << "-1));\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  highp   float in_f_blue;\n"
			<< "layout(location = 0) out mediump vec4  o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = vec4(1.0, 0.0, in_f_blue, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus test (Context& context)
{
	// Input vertex attribute data
	std::vector<float> vertexData		(NUM_VERTICES);
	const VkDeviceSize vertexBufferSize = sizeof(float) * vertexData.size();

	for (int i = 0; i < NUM_VERTICES; ++i)
		vertexData[i] = static_cast<float>(i) / (NUM_VERTICES - 1);

	tcu::TextureLevel referenceImage;
	tcu::ImageIO::loadPNG(referenceImage, context.getTestContext().getArchive(), "vulkan/data/tessellation/barrier_ref.png");

	const int numPrimitives = 1;
	const int inPatchSize   = NUM_VERTICES;
	const int outPatchSize  = NUM_VERTICES;

	return runTest(context, numPrimitives, inPatchSize, outPatchSize,
				   VK_FORMAT_R32_SFLOAT, &vertexData[0], vertexBufferSize, referenceImage.getAccess());
}

} // Barrier ns

namespace CrossInvocation
{

enum Constants
{
	OUTPUT_PATCH_SIZE	= 3,
	INPUT_PATCH_SIZE	= 10
};

enum CaseType
{
	CASETYPE_PER_VERTEX,
	CASETYPE_PER_PATCH
};

enum DataType
{
	DATATYPE_INT,
	DATATYPE_UINT,
	DATATYPE_FLOAT,
	DATATYPE_VEC3,
	DATATYPE_VEC4,
	DATATYPE_MAT4X3
};

struct CaseDefinition
{
	CaseType	caseType;
	DataType	dataType;
};

void initPrograms (vk::SourceCollections& programCollection, const CaseDefinition caseDef)
{
	static const std::string	typeStr[]		=
	{
		"int",
		"uint",
		"float",
		"vec3",
		"vec4",
		"mat4x3"
	};
	const std::string			dataType		= typeStr[caseDef.dataType];
	const int					varyingSize[]	= { 1, 1, 1, 1, 1, 4 };

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  highp float in_v_attr;\n"
			<< "layout(location = 0) out highp float in_tc_attr;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    in_tc_attr = in_v_attr;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation control shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(vertices = " << OUTPUT_PATCH_SIZE << ") out;\n"
			<< "\n"
			<< "layout(location = 0) in  highp float in_tc_attr[];\n"
			<< "layout(location = 0) out highp float in_te_attr[];\n"
			<< "\n";

		if (caseDef.caseType == CASETYPE_PER_VERTEX)
			src << "layout(location = 1) out mediump " << dataType << " in_te_data0[];\n"
				<< "layout(location = " << varyingSize[caseDef.dataType] + 1 << ") out mediump " << dataType << " in_te_data1[];\n";
		else
			src << "layout(location = 1) patch out mediump " << dataType << " in_te_data0[" << OUTPUT_PATCH_SIZE << "];\n"
				<< "layout(location = " << OUTPUT_PATCH_SIZE * varyingSize[caseDef.dataType] + 1 << ") patch out mediump " << dataType << " in_te_data1[" << OUTPUT_PATCH_SIZE << "];\n";

		src	<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    " << dataType << " d = " << dataType << "(gl_InvocationID);\n"
			<< "    in_te_data0[gl_InvocationID] = d;\n"
			<< "    barrier();\n"
			<< "    in_te_data1[gl_InvocationID] = d + in_te_data0[(gl_InvocationID + 1) % " << OUTPUT_PATCH_SIZE << "];\n"
			<< "\n"
			<< "    in_te_attr[gl_InvocationID] = in_tc_attr[gl_InvocationID];\n"
			<< "\n"
			<< "    gl_TessLevelInner[0] = 1.0;\n"
			<< "    gl_TessLevelInner[1] = 1.0;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 1.0;\n"
			<< "    gl_TessLevelOuter[1] = 1.0;\n"
			<< "    gl_TessLevelOuter[2] = 1.0;\n"
			<< "    gl_TessLevelOuter[3] = 1.0;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	// Tessellation evaluation shader
	{
		const float xScale = 1.0f / 8.0f;

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(" << getTessPrimitiveTypeShaderName(TESSPRIMITIVETYPE_QUADS) << ") in;\n"
			<< "\n"
			<< "layout(location = 0) in  highp   float in_te_attr[];\n"
			<< "layout(location = 0) out mediump vec4  in_f_color;\n"
			<< "\n";

		if (caseDef.caseType == CASETYPE_PER_VERTEX)
			src << "layout(location = 1) in mediump " << dataType << " in_te_data0[];\n"
				<< "layout(location = " << varyingSize[caseDef.dataType] + 1 << ") in mediump " << dataType << " in_te_data1[];\n";
		else
			src << "layout(location = 1) patch in mediump " << dataType << " in_te_data0[" << OUTPUT_PATCH_SIZE << "];\n"
				<< "layout(location = " << OUTPUT_PATCH_SIZE * varyingSize[caseDef.dataType] + 1 << ") patch in mediump " << dataType << " in_te_data1[" << OUTPUT_PATCH_SIZE << "];\n";

		src << "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    highp float x = (gl_TessCoord.x*float(" << xScale << ") + in_te_attr[0]) * 2.0 - 1.0;\n"
			<< "    highp float y = gl_TessCoord.y*2.0 - 1.0;\n"
			<< "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
			<< "    bool ok = true;\n"
			<< "    for (int i = 0; i < " << OUTPUT_PATCH_SIZE << "; i++)\n"
			<< "    {\n"
			<< "         int ref = i + (i + 1) % " << OUTPUT_PATCH_SIZE << ";\n"
			<< "         if (in_te_data1[i] != " << dataType << "(ref)) ok = false;\n"
			<< "    }\n"
			<< "    in_f_color = ok ? vec4(1.0) : vec4(vec3(0.0), 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) in  mediump vec4 in_f_color;\n"
			<< "layout(location = 0) out mediump vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = in_f_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus test (Context& context, const CaseDefinition caseDef)
{
	DE_UNREF(caseDef);
	// Input vertex attribute data
	const int		   numPrimitives	= 8;
	std::vector<float> vertexData		(INPUT_PATCH_SIZE * numPrimitives, 0.0f);
	const VkDeviceSize vertexBufferSize = sizeof(float) * vertexData.size();

	for (int i = 0; i < numPrimitives; ++i)
		vertexData[INPUT_PATCH_SIZE * i] = static_cast<float>(i) / static_cast<float>(numPrimitives);

	tcu::TextureLevel referenceImage;
	initializeWhiteReferenceImage(referenceImage, RENDER_SIZE, RENDER_SIZE);

	return runTest(context, numPrimitives, INPUT_PATCH_SIZE, OUTPUT_PATCH_SIZE,
				   VK_FORMAT_R32_SFLOAT, &vertexData[0], vertexBufferSize, referenceImage.getAccess());
}

} // CrossInvocation ns

} // anonymous

//! These tests correspond to dEQP-GLES31.functional.tessellation.shader_input_output.*
tcu::TestCaseGroup* createShaderInputOutputTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "shader_input_output", "Test tessellation control and evaluation shader inputs and outputs"));

	// Patch vertex counts
	{
		static const struct
		{
			int inPatchSize;
			int outPatchSize;
		} patchVertexCountCases[] =
		{
			{  5, 10 },
			{ 10,  5 }
		};

		for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(patchVertexCountCases); caseNdx++)
		{
			const int inSize	= patchVertexCountCases[caseNdx].inPatchSize;
			const int outSize	= patchVertexCountCases[caseNdx].outPatchSize;

			const std::string caseName = "patch_vertices_" + de::toString(inSize) + "_in_" + de::toString(outSize) + "_out";
			const PatchVertexCount::CaseDefinition caseDef =
			{
				inSize, outSize, "vulkan/data/tessellation/" + caseName + "_ref.png"
			};

			addFunctionCaseWithPrograms(group.get(), caseName, "Test input and output patch vertex counts",
										PatchVertexCount::initPrograms, PatchVertexCount::test, caseDef);
		}
	}

	// Per patch data
	{
		static const PerPatchData::CaseDefinition cases[] =
		{
			{ PerPatchData::CASETYPE_PRIMITIVE_ID_TCS,		"primitive_id_tcs",		  true, "vulkan/data/tessellation/primitive_id_tcs_ref.png", "Read gl_PrimitiveID in TCS and pass it as patch output to TES" },
			{ PerPatchData::CASETYPE_PRIMITIVE_ID_TES,		"primitive_id_tes",		  true, "vulkan/data/tessellation/primitive_id_tes_ref.png", "Read gl_PrimitiveID in TES" },
			{ PerPatchData::CASETYPE_PATCH_VERTICES_IN_TCS,	"patch_vertices_in_tcs",  false, "", "Read gl_PatchVerticesIn in TCS and pass it as patch output to TES" },
			{ PerPatchData::CASETYPE_PATCH_VERTICES_IN_TES,	"patch_vertices_in_tes",  false, "", "Read gl_PatchVerticesIn in TES" },
			{ PerPatchData::CASETYPE_TESS_LEVEL_INNER0_TES,	"tess_level_inner_0_tes", false, "", "Read gl_TessLevelInner[0] in TES" },
			{ PerPatchData::CASETYPE_TESS_LEVEL_INNER1_TES,	"tess_level_inner_1_tes", false, "", "Read gl_TessLevelInner[1] in TES" },
			{ PerPatchData::CASETYPE_TESS_LEVEL_OUTER0_TES,	"tess_level_outer_0_tes", false, "", "Read gl_TessLevelOuter[0] in TES" },
			{ PerPatchData::CASETYPE_TESS_LEVEL_OUTER1_TES,	"tess_level_outer_1_tes", false, "", "Read gl_TessLevelOuter[1] in TES" },
			{ PerPatchData::CASETYPE_TESS_LEVEL_OUTER2_TES,	"tess_level_outer_2_tes", false, "", "Read gl_TessLevelOuter[2] in TES" },
			{ PerPatchData::CASETYPE_TESS_LEVEL_OUTER3_TES,	"tess_level_outer_3_tes", false, "", "Read gl_TessLevelOuter[3] in TES" },
		};

		for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
			addFunctionCaseWithPrograms(group.get(), cases[caseNdx].caseName, cases[caseNdx].caseDescription,
										PerPatchData::initPrograms, PerPatchData::test, cases[caseNdx]);
	}

	// gl_Position
	{
		static const struct
		{
			GLPosition::CaseType	type;
			std::string				caseName;
		} cases[] =
		{
			{ GLPosition::CASETYPE_VS_TO_TCS,		 "gl_position_vs_to_tcs"		},
			{ GLPosition::CASETYPE_TCS_TO_TES,		 "gl_position_tcs_to_tes"		},
			{ GLPosition::CASETYPE_VS_TO_TCS_TO_TES, "gl_position_vs_to_tcs_to_tes" },
		};

		for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
			addFunctionCaseWithPrograms(group.get(), cases[caseNdx].caseName, "Pass gl_Position between VS and TCS, or between TCS and TES",
										GLPosition::initPrograms, GLPosition::test, cases[caseNdx].type);
	}

	// Barrier
	addFunctionCaseWithPrograms(group.get(), "barrier", "Basic barrier usage", Barrier::initPrograms, Barrier::test);

	// Cross invocation communication
	{
		static const struct
		{
			CrossInvocation::CaseType	caseType;
			std::string					name;
		} caseTypes[] =
		{
			{ CrossInvocation::CASETYPE_PER_VERTEX,	 "cross_invocation_per_vertex"	},
			{ CrossInvocation::CASETYPE_PER_PATCH,	 "cross_invocation_per_patch"	}
		};

		static const struct
		{
			CrossInvocation::DataType	dataType;
			std::string					name;
		} dataTypes[] =
		{
			{ CrossInvocation::DATATYPE_INT,	"int"		},
			{ CrossInvocation::DATATYPE_UINT,	"uint"		},
			{ CrossInvocation::DATATYPE_FLOAT,	"float"		},
			{ CrossInvocation::DATATYPE_VEC3,	"vec3"		},
			{ CrossInvocation::DATATYPE_VEC4,	"vec4"		},
			{ CrossInvocation::DATATYPE_MAT4X3,	"mat4x3"	}
		};

		for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(caseTypes); ++caseNdx)
			for (int dataTypeNdx = 0; dataTypeNdx < DE_LENGTH_OF_ARRAY(dataTypes); ++dataTypeNdx)
			{
				std::string						testName	= caseTypes[caseNdx].name + "_" + dataTypes[dataTypeNdx].name;
				CrossInvocation::CaseDefinition	caseDef		= { caseTypes[caseNdx].caseType, dataTypes[dataTypeNdx].dataType };

				addFunctionCaseWithPrograms(group.get(), testName, "Write output varyings from multiple invocations.",
						CrossInvocation::initPrograms, CrossInvocation::test, caseDef);
			}
	}

	return group.release();
}

} // tessellation
} // vkt
