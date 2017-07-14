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
 * \brief Tessellation Winding Tests
 *//*--------------------------------------------------------------------*/

#include "vktTessellationWindingTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTessellationUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkStrUtil.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace tessellation
{

using namespace vk;

namespace
{

std::string getCaseName (const TessPrimitiveType primitiveType, const Winding winding)
{
	std::ostringstream str;
	str << getTessPrimitiveTypeShaderName(primitiveType) << "_" << getWindingShaderName(winding);
	return str.str();
}

inline VkFrontFace mapFrontFace (const Winding winding)
{
	switch (winding)
	{
		case WINDING_CCW:	return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		case WINDING_CW:	return VK_FRONT_FACE_CLOCKWISE;
		default:
			DE_ASSERT(false);
			return VK_FRONT_FACE_LAST;
	}
}

//! Returns true when the image passes the verification.
bool verifyResultImage (tcu::TestLog&						log,
						const tcu::ConstPixelBufferAccess	image,
						const TessPrimitiveType				primitiveType,
						const Winding						winding,
						const Winding						frontFaceWinding)
{
	const int totalNumPixels	= image.getWidth()*image.getHeight();
	const int badPixelTolerance = (primitiveType == TESSPRIMITIVETYPE_TRIANGLES ? 5*de::max(image.getWidth(), image.getHeight()) : 0);

	const tcu::Vec4 white = tcu::RGBA::white().toVec();
	const tcu::Vec4 red   = tcu::RGBA::red().toVec();

	int numWhitePixels = 0;
	int numRedPixels   = 0;
	for (int y = 0; y < image.getHeight();	y++)
	for (int x = 0; x < image.getWidth();	x++)
	{
		numWhitePixels += image.getPixel(x, y) == white ? 1 : 0;
		numRedPixels   += image.getPixel(x, y) == red   ? 1 : 0;
	}

	DE_ASSERT(numWhitePixels + numRedPixels <= totalNumPixels);

	log << tcu::TestLog::Message << "Note: got " << numWhitePixels << " white and " << numRedPixels << " red pixels" << tcu::TestLog::EndMessage;

	const int otherPixels = totalNumPixels - numWhitePixels - numRedPixels;
	if (otherPixels > badPixelTolerance)
	{
		log << tcu::TestLog::Message
			<< "Failure: Got " << otherPixels << " other than white or red pixels (maximum tolerance " << badPixelTolerance << ")"
			<< tcu::TestLog::EndMessage;
		return false;
	}

	if (frontFaceWinding == winding)
	{
		if (primitiveType == TESSPRIMITIVETYPE_TRIANGLES)
		{
			if (de::abs(numWhitePixels - totalNumPixels/2) > badPixelTolerance)
			{
				log << tcu::TestLog::Message << "Failure: wrong number of white pixels; expected approximately " << totalNumPixels/2 << tcu::TestLog::EndMessage;
				return false;
			}
		}
		else if (primitiveType == TESSPRIMITIVETYPE_QUADS)
		{
			if (numWhitePixels != totalNumPixels)
			{
				log << tcu::TestLog::Message << "Failure: expected only white pixels (full-viewport quad)" << tcu::TestLog::EndMessage;
				return false;
			}
		}
		else
			DE_ASSERT(false);
	}
	else
	{
		if (numWhitePixels != 0)
		{
			log << tcu::TestLog::Message << "Failure: expected only red pixels (everything culled)" << tcu::TestLog::EndMessage;
			return false;
		}
	}

	return true;
}

class WindingTest : public TestCase
{
public:
								WindingTest		(tcu::TestContext&			testCtx,
												 const TessPrimitiveType	primitiveType,
												 const Winding				winding);

	void						initPrograms	(SourceCollections&			programCollection) const;
	TestInstance*				createInstance	(Context&					context) const;

private:
	const TessPrimitiveType		m_primitiveType;
	const Winding				m_winding;
};

WindingTest::WindingTest (tcu::TestContext&			testCtx,
						  const TessPrimitiveType	primitiveType,
						  const Winding				winding)
	: TestCase			(testCtx, getCaseName(primitiveType, winding), "")
	, m_primitiveType	(primitiveType)
	, m_winding			(winding)
{
}

void WindingTest::initPrograms (SourceCollections& programCollection) const
{
	// Vertex shader - no inputs
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation control shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "\n"
			<< "layout(vertices = 1) out;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
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
			<< "layout(" << getTessPrimitiveTypeShaderName(m_primitiveType) << ", "
						 << getWindingShaderName(m_winding) << ") in;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = vec4(gl_TessCoord.xy*2.0 - 1.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
			<< "\n"
			<< "layout(location = 0) out mediump vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = vec4(1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

class WindingTestInstance : public TestInstance
{
public:
								WindingTestInstance (Context&					context,
													 const TessPrimitiveType	primitiveType,
													 const Winding				winding);

	tcu::TestStatus				iterate				(void);

private:
	const TessPrimitiveType		m_primitiveType;
	const Winding				m_winding;
};

WindingTestInstance::WindingTestInstance (Context&					context,
										  const TessPrimitiveType	primitiveType,
										  const Winding				winding)
	: TestInstance		(context)
	, m_primitiveType	(primitiveType)
	, m_winding			(winding)
{
}

tcu::TestStatus WindingTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Color attachment

	const tcu::IVec2			  renderSize				 = tcu::IVec2(64, 64);
	const VkFormat				  colorFormat				 = VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageSubresourceRange colorImageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Image					  colorAttachmentImage		 (vk, device, allocator,
															 makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 1u),
															 MemoryRequirement::Any);

	// Color output buffer: image will be copied here for verification

	const VkDeviceSize	colorBufferSizeBytes = renderSize.x()*renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
	const Buffer		colorBuffer			 (vk, device, allocator, makeBufferCreateInfo(colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);

	// Pipeline

	const Unique<VkImageView>		colorAttachmentView(makeImageView                       (vk, device, *colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorImageSubresourceRange));
	const Unique<VkRenderPass>		renderPass         (makeRenderPass                      (vk, device, colorFormat));
	const Unique<VkFramebuffer>		framebuffer        (makeFramebuffer                     (vk, device, *renderPass, *colorAttachmentView, renderSize.x(), renderSize.y(), 1u));
	const Unique<VkPipelineLayout>	pipelineLayout     (makePipelineLayoutWithoutDescriptors(vk, device));

	const VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;

	// Front face is static state, so we have to create two pipelines.

	const Unique<VkPipeline> pipelineCounterClockwise(GraphicsPipelineBuilder()
		.setRenderSize	 (renderSize)
		.setCullModeFlags(cullMode)
		.setFrontFace	 (VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.setShader		 (vk, device, VK_SHADER_STAGE_VERTEX_BIT,				   m_context.getBinaryCollection().get("vert"), DE_NULL)
		.setShader		 (vk, device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,    m_context.getBinaryCollection().get("tesc"), DE_NULL)
		.setShader		 (vk, device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, m_context.getBinaryCollection().get("tese"), DE_NULL)
		.setShader		 (vk, device, VK_SHADER_STAGE_FRAGMENT_BIT,				   m_context.getBinaryCollection().get("frag"), DE_NULL)
		.build			 (vk, device, *pipelineLayout, *renderPass));

	const Unique<VkPipeline> pipelineClockwise(GraphicsPipelineBuilder()
		.setRenderSize   (renderSize)
		.setCullModeFlags(cullMode)
		.setFrontFace    (VK_FRONT_FACE_CLOCKWISE)
		.setShader		 (vk, device, VK_SHADER_STAGE_VERTEX_BIT,				   m_context.getBinaryCollection().get("vert"), DE_NULL)
		.setShader		 (vk, device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,	   m_context.getBinaryCollection().get("tesc"), DE_NULL)
		.setShader		 (vk, device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, m_context.getBinaryCollection().get("tese"), DE_NULL)
		.setShader		 (vk, device, VK_SHADER_STAGE_FRAGMENT_BIT,				   m_context.getBinaryCollection().get("frag"), DE_NULL)
		.build			 (vk, device, *pipelineLayout, *renderPass));

	const struct // not static
	{
		Winding		frontFaceWinding;
		VkPipeline	pipeline;
	} testCases[] =
	{
		{ WINDING_CCW,	*pipelineCounterClockwise },
		{ WINDING_CW,	*pipelineClockwise		  },
	};

	tcu::TestLog& log = m_context.getTestContext().getLog();
	log << tcu::TestLog::Message << "Pipeline uses " << getCullModeFlagsStr(cullMode) << tcu::TestLog::EndMessage;

	bool success = true;

	// Draw commands

	const Unique<VkCommandPool>   cmdPool  (makeCommandPool  (vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(testCases); ++caseNdx)
	{
		const Winding frontFaceWinding = testCases[caseNdx].frontFaceWinding;

		log << tcu::TestLog::Message << "Setting " << getFrontFaceName(mapFrontFace(frontFaceWinding)) << tcu::TestLog::EndMessage;

		// Reset the command buffer and begin.
		beginCommandBuffer(vk, *cmdBuffer);

		// Change color attachment image layout
		{
			// State is slightly different on the first iteration.
			const VkImageLayout currentLayout = (caseNdx == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			const VkAccessFlags srcFlags	  = (caseNdx == 0 ? (VkAccessFlags)0          : (VkAccessFlags)VK_ACCESS_TRANSFER_READ_BIT);

			const VkImageMemoryBarrier colorAttachmentLayoutBarrier = makeImageMemoryBarrier(
				srcFlags, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				currentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				*colorAttachmentImage, colorImageSubresourceRange);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, &colorAttachmentLayoutBarrier);
		}

		// Begin render pass
		{
			const VkRect2D renderArea = {
				makeOffset2D(0, 0),
				makeExtent2D(renderSize.x(), renderSize.y()),
			};
			const tcu::Vec4 clearColor = tcu::RGBA::red().toVec();

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);
		}

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, testCases[caseNdx].pipeline);

		// Process a single abstract vertex.
		vk.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		// Copy render result to a host-visible buffer
		{
			const VkImageMemoryBarrier colorAttachmentPreCopyBarrier = makeImageMemoryBarrier(
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				*colorAttachmentImage, colorImageSubresourceRange);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, &colorAttachmentPreCopyBarrier);
		}
		{
			const VkBufferImageCopy copyRegion = makeBufferImageCopy(makeExtent3D(renderSize.x(), renderSize.y(), 1), makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
			vk.cmdCopyImageToBuffer(*cmdBuffer, *colorAttachmentImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &copyRegion);
		}
		{
			const VkBufferMemoryBarrier postCopyBarrier = makeBufferMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, colorBufferSizeBytes);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
				0u, DE_NULL, 1u, &postCopyBarrier, 0u, DE_NULL);
		}

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		{
			// Log rendered image
			const Allocation& colorBufferAlloc = colorBuffer.getAllocation();
			invalidateMappedMemoryRange(vk, device, colorBufferAlloc.getMemory(), colorBufferAlloc.getOffset(), colorBufferSizeBytes);

			const tcu::ConstPixelBufferAccess imagePixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAlloc.getHostPtr());
			log << tcu::TestLog::Image("color0", "Rendered image", imagePixelAccess);

			// Verify case result
			success = success && verifyResultImage(log, imagePixelAccess, m_primitiveType, m_winding, frontFaceWinding);
		}
	}  // for windingNdx

	return (success ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Failure"));
}

TestInstance* WindingTest::createInstance (Context& context) const
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_TESSELLATION_SHADER);

	return new WindingTestInstance(context, m_primitiveType, m_winding);
}

} // anonymous

//! These tests correspond to dEQP-GLES31.functional.tessellation.winding.*
tcu::TestCaseGroup* createWindingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "winding", "Test the cw and ccw input layout qualifiers"));

	static const TessPrimitiveType primitivesNoIsolines[] =
	{
		TESSPRIMITIVETYPE_TRIANGLES,
		TESSPRIMITIVETYPE_QUADS,
	};

	for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitivesNoIsolines); ++primitiveTypeNdx)
	for (int windingNdx = 0; windingNdx < WINDING_LAST; ++windingNdx)
		group->addChild(new WindingTest(testCtx, primitivesNoIsolines[primitiveTypeNdx], (Winding)windingNdx));

	return group.release();
}

} // tessellation
} // vkt
