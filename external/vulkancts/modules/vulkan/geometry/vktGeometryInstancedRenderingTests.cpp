/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2014 The Android Open Source Project
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
 * \brief Geometry shader instanced rendering tests
 *//*--------------------------------------------------------------------*/

#include "vktGeometryInstancedRenderingTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktGeometryTestsUtil.hpp"

#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

#include "deRandom.hpp"
#include "deMath.h"

namespace vkt
{
namespace geometry
{
namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using tcu::Vec4;
using tcu::UVec2;

struct TestParams
{
	int	numDrawInstances;
	int	numInvocations;
};

VkImageCreateInfo makeImageCreateInfo (const VkFormat format, const VkExtent3D size, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		(VkImageCreateFlags)0,							// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		format,											// VkFormat					format;
		size,											// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return imageParams;
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&	vk,
									   const VkDevice			device,
									   const VkPipelineLayout	pipelineLayout,
									   const VkRenderPass		renderPass,
									   const VkShaderModule		vertexModule,
									   const VkShaderModule		geometryModule,
									   const VkShaderModule		fragmentModule,
									   const VkExtent2D			renderSize)
{
	const std::vector<VkViewport>				viewports						(1, makeViewport(renderSize));
	const std::vector<VkRect2D>					scissors						(1, makeRect2D(renderSize));

	const VkVertexInputBindingDescription		vertexInputBindingDescription	=
	{
		0u,								// deUint32             binding;
		sizeof(Vec4),					// deUint32             stride;
		VK_VERTEX_INPUT_RATE_INSTANCE	// VkVertexInputRate    inputRate;
	};

	const VkVertexInputAttributeDescription		vertexInputAttributeDescription	=
	{
		0u,								// deUint32         location;
		0u,								// deUint32         binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat         format;
		0u								// deUint32         offset;
	};

	const VkPipelineVertexInputStateCreateInfo	vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags;
		1u,															// deUint32                                    vertexBindingDescriptionCount;
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		1u,															// deUint32                                    vertexAttributeDescriptionCount;
		&vertexInputAttributeDescription							// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	return vk::makeGraphicsPipeline(vk,									// const DeviceInterface&                        vk
									device,								// const VkDevice                                device
									pipelineLayout,						// const VkPipelineLayout                        pipelineLayout
									vertexModule,						// const VkShaderModule                          vertexShaderModule
									DE_NULL,							// const VkShaderModule                          tessellationControlModule
									DE_NULL,							// const VkShaderModule                          tessellationEvalModule
									geometryModule,						// const VkShaderModule                          geometryShaderModule
									fragmentModule,						// const VkShaderModule                          fragmentShaderModule
									renderPass,							// const VkRenderPass                            renderPass
									viewports,							// const std::vector<VkViewport>&                viewports
									scissors,							// const std::vector<VkRect2D>&                  scissors
									VK_PRIMITIVE_TOPOLOGY_POINT_LIST,	// const VkPrimitiveTopology                     topology
									0u,									// const deUint32                                subpass
									0u,									// const deUint32                                patchControlPoints
									&vertexInputStateCreateInfo);		// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
}

void draw (Context&					context,
		   const UVec2&				renderSize,
		   const VkFormat			colorFormat,
		   const Vec4&				clearColor,
		   const VkBuffer			colorBuffer,
		   const int				numDrawInstances,
		   const std::vector<Vec4>& perInstanceAttribute)
{
	const DeviceInterface&			vk						= context.getDeviceInterface();
	const VkDevice					device					= context.getDevice();
	const deUint32					queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const VkQueue					queue					= context.getUniversalQueue();
	Allocator&						allocator				= context.getDefaultAllocator();

	const VkImageSubresourceRange	colorSubresourceRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const VkExtent3D				colorImageExtent		(makeExtent3D(renderSize.x(), renderSize.y(), 1u));
	const VkExtent2D				renderExtent			(makeExtent2D(renderSize.x(), renderSize.y()));

	const Unique<VkImage>			colorImage				(makeImage		(vk, device, makeImageCreateInfo(colorFormat, colorImageExtent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		colorImageAlloc			(bindImage		(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorAttachment			(makeImageView	(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	const VkDeviceSize				vertexBufferSize		= sizeInBytes(perInstanceAttribute);
	const Unique<VkBuffer>			vertexBuffer			(makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc		(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>	vertexModule			(createShaderModule	(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	geometryModule			(createShaderModule	(vk, device, context.getBinaryCollection().get("geom"), 0u));
	const Unique<VkShaderModule>	fragmentModule			(createShaderModule	(vk, device, context.getBinaryCollection().get("frag"), 0u));

	const Unique<VkRenderPass>		renderPass				(vk::makeRenderPass		(vk, device, colorFormat));
	const Unique<VkFramebuffer>		framebuffer				(makeFramebuffer		(vk, device, *renderPass, *colorAttachment, renderSize.x(), renderSize.y()));
	const Unique<VkPipelineLayout>	pipelineLayout			(makePipelineLayout		(vk, device));
	const Unique<VkPipeline>		pipeline				(makeGraphicsPipeline	(vk, device, *pipelineLayout, *renderPass, *vertexModule, *geometryModule, *fragmentModule, renderExtent));

	const Unique<VkCommandPool>		cmdPool					(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer				(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Initialize vertex data
	{
		deMemcpy(vertexBufferAlloc->getHostPtr(), &perInstanceAttribute[0], (size_t)vertexBufferSize);
		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	beginCommandBuffer(vk, *cmdBuffer);

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(renderExtent), clearColor);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	{
		const VkDeviceSize offset = 0ull;
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &offset);
	}
	vk.cmdDraw(*cmdBuffer, 1u, static_cast<deUint32>(numDrawInstances), 0u, 0u);
	endRenderPass(vk, *cmdBuffer);

	copyImageToBuffer(vk, *cmdBuffer, *colorImage, colorBuffer, tcu::IVec2(renderSize.x(), renderSize.y()));

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
}

std::vector<Vec4> generatePerInstancePosition (const int numInstances)
{
	de::Random			rng(1234);
	std::vector<Vec4>	positions;

	for (int i = 0; i < numInstances; ++i)
	{
		const float flipX	= rng.getBool() ? 1.0f : -1.0f;
		const float flipY	= rng.getBool() ? 1.0f : -1.0f;
		const float x		= flipX * rng.getFloat(0.1f, 0.9f);	// x mustn't be 0.0, because we are using sign() in the shader
		const float y		= flipY * rng.getFloat(0.0f, 0.7f);

		positions.push_back(Vec4(x, y, 0.0f, 1.0f));
	}

	return positions;
}

//! Get a rectangle region of an image, using NDC coordinates (i.e. [-1, 1] range).
//! Result rect is cropped in either dimension to be inside the bounds of the image.
tcu::PixelBufferAccess getSubregion (tcu::PixelBufferAccess image, const float x, const float y, const float size)
{
	const float w	= static_cast<float>(image.getWidth());
	const float h	= static_cast<float>(image.getHeight());
	const float x1	= w * (x + 1.0f) * 0.5f;
	const float y1	= h * (y + 1.0f) * 0.5f;
	const float sx	= w * size * 0.5f;
	const float sy	= h * size * 0.5f;
	const float x2	= x1 + sx;
	const float y2	= y1 + sy;

	// Round and clamp only after all of the above.
	const int	ix1	= std::max(deRoundFloatToInt32(x1), 0);
	const int	ix2	= std::min(deRoundFloatToInt32(x2), image.getWidth());
	const int	iy1	= std::max(deRoundFloatToInt32(y1), 0);
	const int	iy2	= std::min(deRoundFloatToInt32(y2), image.getHeight());

	return tcu::getSubregion(image, ix1, iy1, ix2 - ix1, iy2 - iy1);
}

//! Must be in sync with the geometry shader code.
void generateReferenceImage(tcu::PixelBufferAccess image, const Vec4& clearColor, const std::vector<Vec4>& perInstancePosition, const int numInvocations)
{
	tcu::clear(image, clearColor);

	for (std::vector<Vec4>::const_iterator iterPosition = perInstancePosition.begin(); iterPosition != perInstancePosition.end(); ++iterPosition)
	for (int invocationNdx = 0; invocationNdx < numInvocations; ++invocationNdx)
	{
		const float x			= iterPosition->x();
		const float y			= iterPosition->y();
		const float	modifier	= (numInvocations > 1 ? static_cast<float>(invocationNdx) / static_cast<float>(numInvocations - 1) : 0.0f);
		const Vec4	color		(deFloatAbs(x), deFloatAbs(y), 0.2f + 0.8f * modifier, 1.0f);
		const float size		= 0.05f + 0.03f * modifier;
		const float dx			= (deFloatSign(-x) - x) / static_cast<float>(numInvocations);
		const float xOffset		= static_cast<float>(invocationNdx) * dx;
		const float yOffset		= 0.3f * deFloatSin(12.0f * modifier);

		tcu::PixelBufferAccess rect = getSubregion(image, x + xOffset - size, y + yOffset - size, size + size);
		tcu::clear(rect, color);
	}
}

void initPrograms (SourceCollections& programCollection, const TestParams params)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Geometry shader
	{
		// The shader must be in sync with reference image rendering routine.

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(points, invocations = " << params.numInvocations << ") in;\n"
			<< "layout(triangle_strip, max_vertices = 4) out;\n"
			<< "\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "in gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "} gl_in[];\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    const vec4  pos       = gl_in[0].gl_Position;\n"
			<< "    const float modifier  = " << (params.numInvocations > 1 ? "float(gl_InvocationID) / float(" + de::toString(params.numInvocations - 1) + ")" : "0.0") << ";\n"
			<< "    const vec4  color     = vec4(abs(pos.x), abs(pos.y), 0.2 + 0.8 * modifier, 1.0);\n"
			<< "    const float size      = 0.05 + 0.03 * modifier;\n"
			<< "    const float dx        = (sign(-pos.x) - pos.x) / float(" << params.numInvocations << ");\n"
			<< "    const vec4  offsetPos = pos + vec4(float(gl_InvocationID) * dx,\n"
			<< "                                       0.3 * sin(12.0 * modifier),\n"
			<< "                                       0.0,\n"
			<< "                                       0.0);\n"
			<< "\n"
			<< "    gl_Position = offsetPos + vec4(-size, -size, 0.0, 0.0);\n"
			<< "    out_color   = color;\n"
			<< "    EmitVertex();\n"
			<< "\n"
			<< "    gl_Position = offsetPos + vec4(-size,  size, 0.0, 0.0);\n"
			<< "    out_color   = color;\n"
			<< "    EmitVertex();\n"
			<< "\n"
			<< "    gl_Position = offsetPos + vec4( size, -size, 0.0, 0.0);\n"
			<< "    out_color   = color;\n"
			<< "    EmitVertex();\n"
			<< "\n"
			<< "    gl_Position = offsetPos + vec4( size,  size, 0.0, 0.0);\n"
			<< "    out_color   = color;\n"
			<< "    EmitVertex();\n"
			<<	"}\n";

		programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    o_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus test (Context& context, const TestParams params)
{
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	Allocator&						allocator			= context.getDefaultAllocator();

	const UVec2						renderSize			(128u, 128u);
	const VkFormat					colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const Vec4						clearColor			= Vec4(0.0f, 0.0f, 0.0f, 1.0f);

	const VkDeviceSize				colorBufferSize		= renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
	const Unique<VkBuffer>			colorBuffer			(makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc	(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const std::vector<Vec4>			perInstancePosition	= generatePerInstancePosition(params.numDrawInstances);

	{
		context.getTestContext().getLog()
			<< tcu::TestLog::Message << "Rendering " << params.numDrawInstances << " instance(s) of colorful quads." << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Drawing " << params.numInvocations << " quad(s), each drawn by a geometry shader invocation." << tcu::TestLog::EndMessage;
	}

	zeroBuffer(vk, device, *colorBufferAlloc, colorBufferSize);
	draw(context, renderSize, colorFormat, clearColor, *colorBuffer, params.numDrawInstances, perInstancePosition);

	// Compare result
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);
		const tcu::ConstPixelBufferAccess result(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1u, colorBufferAlloc->getHostPtr());

		tcu::TextureLevel reference(mapVkFormat(colorFormat), renderSize.x(), renderSize.y());
		generateReferenceImage(reference.getAccess(), clearColor, perInstancePosition, params.numInvocations);

		if (!tcu::fuzzyCompare(context.getTestContext().getLog(), "Image Compare", "Image Compare", reference.getAccess(), result, 0.01f, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Rendered image is incorrect");
		else
			return tcu::TestStatus::pass("OK");
	}
}

void checkSupport (Context& context, TestParams params)
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (context.getDeviceProperties().limits.maxGeometryShaderInvocations < (deUint32)params.numInvocations)
		TCU_THROW(NotSupportedError, (std::string("Unsupported limit: maxGeometryShaderInvocations < ") + de::toString(params.numInvocations)).c_str());
}

} // anonymous

//! \note CTS requires shaders to be known ahead of time (some platforms use precompiled shaders), so we can't query a limit at runtime and generate
//!       a shader based on that. This applies to number of GS invocations which can't be injected into the shader.
tcu::TestCaseGroup* createInstancedRenderingTests (tcu::TestContext& testCtx)
{
	MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "instanced", "Instanced rendering tests."));

	const int drawInstanceCases[]	=
	{
		1, 2, 4, 8,
	};
	const int invocationCases[]		=
	{
		1, 2, 8, 32,	// required by the Vulkan spec
		64, 127,		// larger than the minimum, but perhaps some implementations support it, so we'll try
	};

	for (const int* pNumDrawInstances = drawInstanceCases; pNumDrawInstances != drawInstanceCases + DE_LENGTH_OF_ARRAY(drawInstanceCases); ++pNumDrawInstances)
	for (const int* pNumInvocations   = invocationCases;   pNumInvocations   != invocationCases   + DE_LENGTH_OF_ARRAY(invocationCases);   ++pNumInvocations)
	{
		std::ostringstream caseName;
		caseName << "draw_" << *pNumDrawInstances << "_instances_" << *pNumInvocations << "_geometry_invocations";

		const TestParams params =
		{
			*pNumDrawInstances,
			*pNumInvocations,
		};

		addFunctionCaseWithPrograms(group.get(), caseName.str(), "", checkSupport, initPrograms, test, params);
	}

	return group.release();
}

} // geometry
} // vkt
