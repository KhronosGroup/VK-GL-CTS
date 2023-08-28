/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \brief Clipping tests
 *//*--------------------------------------------------------------------*/

#include "vktClippingTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDrawUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuCommandLine.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

namespace vkt
{
namespace clipping
{
namespace
{
using namespace vk;
using de::MovePtr;
using tcu::UVec2;
using tcu::Vec4;
using tcu::IVec2;
using namespace drawutil;

enum TestConstants
{
	RENDER_SIZE								= 16,
	RENDER_SIZE_LARGE						= 128,
	NUM_RENDER_PIXELS						= RENDER_SIZE * RENDER_SIZE,
	NUM_PATCH_CONTROL_POINTS				= 3,
	MAX_CLIP_DISTANCES						= 8,
	MAX_CULL_DISTANCES						= 8,
	MAX_COMBINED_CLIP_AND_CULL_DISTANCES	= 8,
};

enum FeatureFlagBits
{
	FEATURE_TESSELLATION_SHADER							= 1u << 0,
	FEATURE_GEOMETRY_SHADER								= 1u << 1,
	FEATURE_SHADER_FLOAT_64								= 1u << 2,
	FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS			= 1u << 3,
	FEATURE_FRAGMENT_STORES_AND_ATOMICS					= 1u << 4,
	FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE	= 1u << 5,
	FEATURE_DEPTH_CLAMP									= 1u << 6,
	FEATURE_LARGE_POINTS								= 1u << 7,
	FEATURE_WIDE_LINES									= 1u << 8,
	FEATURE_SHADER_CLIP_DISTANCE						= 1u << 9,
	FEATURE_SHADER_CULL_DISTANCE						= 1u << 10,
};
typedef deUint32 FeatureFlags;

void requireFeatures (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const FeatureFlags flags)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(vki, physDevice);

	if (((flags & FEATURE_TESSELLATION_SHADER) != 0) && !features.tessellationShader)
		throw tcu::NotSupportedError("Tessellation shader not supported");

	if (((flags & FEATURE_GEOMETRY_SHADER) != 0) && !features.geometryShader)
		throw tcu::NotSupportedError("Geometry shader not supported");

	if (((flags & FEATURE_SHADER_FLOAT_64) != 0) && !features.shaderFloat64)
		throw tcu::NotSupportedError("Double-precision floats not supported");

	if (((flags & FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS) != 0) && !features.vertexPipelineStoresAndAtomics)
		throw tcu::NotSupportedError("SSBO and image writes not supported in vertex pipeline");

	if (((flags & FEATURE_FRAGMENT_STORES_AND_ATOMICS) != 0) && !features.fragmentStoresAndAtomics)
		throw tcu::NotSupportedError("SSBO and image writes not supported in fragment shader");

	if (((flags & FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE) != 0) && !features.shaderTessellationAndGeometryPointSize)
		throw tcu::NotSupportedError("Tessellation and geometry shaders don't support PointSize built-in");

	if (((flags & FEATURE_DEPTH_CLAMP) != 0) && !features.depthClamp)
		throw tcu::NotSupportedError("Depth clamp not supported");

	if (((flags & FEATURE_LARGE_POINTS) != 0) && !features.largePoints)
		throw tcu::NotSupportedError("Large points not supported");

	if (((flags & FEATURE_WIDE_LINES) != 0) && !features.wideLines)
		throw tcu::NotSupportedError("Wide lines not supported");

	if (((flags & FEATURE_SHADER_CLIP_DISTANCE) != 0) && !features.shaderClipDistance)
		throw tcu::NotSupportedError("Shader ClipDistance not supported");

	if (((flags & FEATURE_SHADER_CULL_DISTANCE) != 0) && !features.shaderCullDistance)
		throw tcu::NotSupportedError("Shader CullDistance not supported");
}

std::vector<Vec4> genVertices (const VkPrimitiveTopology topology, const Vec4& offset, const float slope)
{
	const float p  = 1.0f;
	const float hp = 0.5f;
	const float z  = 0.0f;
	const float w  = 1.0f;

	std::vector<Vec4> vertices;

	// We're setting adjacent vertices to zero where needed, as we don't use them in meaningful way.

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			vertices.push_back(offset + Vec4(0.0f, 0.0f, slope/2.0f + z, w));
			vertices.push_back(offset + Vec4( -hp,  -hp,              z, w));
			vertices.push_back(offset + Vec4(  hp,  -hp,      slope + z, w));
			vertices.push_back(offset + Vec4( -hp,   hp,              z, w));
			vertices.push_back(offset + Vec4(  hp,   hp,      slope + z, w));
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// line 0
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// line 1
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// line 2
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// line 0
			vertices.push_back(Vec4());
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// line 1
			vertices.push_back(Vec4());
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// line 2
			vertices.push_back(Vec4());
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// line 0
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// line 1
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// line 2
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// line 0
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// line 1
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// line 2
			vertices.push_back(Vec4());
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// triangle 0
			vertices.push_back(offset + Vec4(-p,  p,         z, w));
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// triangle 1
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// triangle 0
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p,  p,         z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// triangle 1
			vertices.push_back(Vec4());
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// triangle 0
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// triangle 1
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4(-p,  p,         z, w));
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));	// triangle 0
			vertices.push_back(Vec4());
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// triangle 1
			vertices.push_back(Vec4());
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			vertices.push_back(offset + Vec4( p, -p, slope + z, w));
			vertices.push_back(offset + Vec4(-p, -p,         z, w));
			vertices.push_back(offset + Vec4(-p,  p,         z, w));	// triangle 0
			vertices.push_back(offset + Vec4( p,  p, slope + z, w));	// triangle 1
			break;

		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
			DE_ASSERT(0);
			break;

		default:
			DE_ASSERT(0);
			break;
	}
	return vertices;
}

bool inline isColorInRange (const Vec4& color, const Vec4& minColor, const Vec4& maxColor)
{
	return (minColor.x() <= color.x() && color.x() <= maxColor.x())
		&& (minColor.y() <= color.y() && color.y() <= maxColor.y())
		&& (minColor.z() <= color.z() && color.z() <= maxColor.z())
		&& (minColor.w() <= color.w() && color.w() <= maxColor.w());
}

//! Count pixels that match color within threshold, in the specified region.
int countPixels (const tcu::ConstPixelBufferAccess pixels, const IVec2& regionOffset, const IVec2& regionSize, const Vec4& color, const Vec4& colorThreshold)
{
	const Vec4	minColor	= color - colorThreshold;
	const Vec4	maxColor	= color + colorThreshold;
	const int	xEnd		= regionOffset.x() + regionSize.x();
	const int	yEnd		= regionOffset.y() + regionSize.y();
	int			numPixels	= 0;

	DE_ASSERT(xEnd <= pixels.getWidth());
	DE_ASSERT(yEnd <= pixels.getHeight());

	for (int y = regionOffset.y(); y < yEnd; ++y)
	for (int x = regionOffset.x(); x < xEnd; ++x)
	{
		if (isColorInRange(pixels.getPixel(x, y), minColor, maxColor))
			++numPixels;
	}

	return numPixels;
}

int countPixels (const tcu::ConstPixelBufferAccess pixels, const Vec4& color, const Vec4& colorThreshold)
{
	return countPixels(pixels, IVec2(), IVec2(pixels.getWidth(), pixels.getHeight()), color, colorThreshold);
}

//! Check for correct cull and clip distance values. Middle bar should contain clip distance with linear values between 0 and 1. Cull distance is always 0.5 when enabled.
bool checkFragColors (const tcu::ConstPixelBufferAccess pixels, IVec2 clipRegion, int barIdx, bool hasCullDistance)
{
	for (int y = 0; y < pixels.getHeight(); ++y)
	for (int x = 0; x < pixels.getWidth(); ++x)
	{
		if (x < clipRegion.x() && y < clipRegion.y())
			continue;

		const tcu::Vec4	color = pixels.getPixel(x, y);
		const int		barWidth = pixels.getWidth() / 8;
		const bool		insideBar = x >= barWidth * barIdx && x < barWidth* (barIdx + 1);
		const float		expectedClipDistance = insideBar ? (((((float)y + 0.5f) / (float)pixels.getHeight()) - 0.5f) * 2.0f) : 0.0f;
		float			expectedCullDistance = 0.5f;
		const float		clipDistance = color.y();
		const float		cullDistance = color.z();
		const float		height = (float)pixels.getHeight();

		if (hasCullDistance)
		{
			/* Linear interpolation of the cull distance.
			 * Remember there are precision errors due to 8-bit UNORM, but they should fall inside 0.01f threshold.
			 *
			 * Notes about the results:
			 * - linear interpolation of gl_CullDistance[i] = [0.0f, 0.5f]. Correct.
			 * - Constant value:
			 *   + 0.1f: value written by vertex shader when there are other geometry-related shaders. It means the value was not overriden. Failure.
			 *   + 0.2f: value written by tessc shader when cull distance value from vertex is not 0.1f. Failure.
			 *   + 0.3f: value written by tessc shader when cull distance value from vertex is 0.1f and there is geometry shader. Failure.
			 *   + 0.4f: value written by geometry shader when cull distance is not either 0.1f (if no tess is present) or 0.3f (tess present). Failure.
			 */
			if (y >= (pixels.getHeight() / 2))
				expectedCullDistance = expectedCullDistance * (1.0f + (2.0f * (float)y) - height) / height;
			else
				expectedCullDistance = 0.0f;
		}

		if (fabs(clipDistance - expectedClipDistance) > 0.01f)
			return false;
		if (hasCullDistance && fabs(cullDistance - expectedCullDistance) > 0.01f)
			return false;
	}

	return true;
}

//! Clipping against the default clip volume.
namespace ClipVolume
{

//! Used by wide lines test.
enum LineOrientation
{
	LINE_ORIENTATION_AXIS_ALIGNED,
	LINE_ORIENTATION_DIAGONAL,
};

const VkPointClippingBehavior invalidClippingBehavior = VK_POINT_CLIPPING_BEHAVIOR_LAST;

VkPointClippingBehavior getClippingBehavior (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDevicePointClippingProperties	behaviorProperties	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES,	// VkStructureType				sType
		DE_NULL,														// void*						pNext
		invalidClippingBehavior											// VkPointClippingBehavior	pointClippingBehavior
	};
	VkPhysicalDeviceProperties2				properties2;

	DE_ASSERT(getPointClippingBehaviorName(invalidClippingBehavior) == DE_NULL);

	deMemset(&properties2, 0, sizeof(properties2));

	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &behaviorProperties;

	vk.getPhysicalDeviceProperties2(physicalDevice, &properties2);

	return behaviorProperties.pointClippingBehavior;
}

void addSimplePrograms (SourceCollections& programCollection, const float pointSize = 0.0f)
{
	// Vertex shader
	{
		const bool usePointSize = pointSize > 0.0f;

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 v_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4  gl_Position;\n"
			<< (usePointSize ? "    float gl_PointSize;\n" : "")
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = v_position;\n"
			<< (usePointSize ? "    gl_PointSize = " + de::floatToString(pointSize, 1) + ";\n" : "")
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = vec4(1.0, gl_FragCoord.z, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void initPrograms (SourceCollections& programCollection, const VkPrimitiveTopology topology)
{
	const float pointSize = (topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ? 1.0f : 0.0f);
	addSimplePrograms(programCollection, pointSize);
}

void initPrograms (SourceCollections& programCollection, const LineOrientation lineOrientation)
{
	DE_UNREF(lineOrientation);
	addSimplePrograms(programCollection);
}

void initProgramsPointSize (SourceCollections& programCollection)
{
	addSimplePrograms(programCollection, 0.75f * static_cast<float>(RENDER_SIZE));
}

//! Primitives fully inside the clip volume.
tcu::TestStatus testPrimitivesInside (Context& context, const VkPrimitiveTopology topology)
{
	int minExpectedBlackPixels = 0;

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			// We draw only 5 points.
			minExpectedBlackPixels = NUM_RENDER_PIXELS - 5;
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_GEOMETRY_SHADER);
			// Fallthrough
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			// Allow for some error.
			minExpectedBlackPixels = NUM_RENDER_PIXELS - 3 * RENDER_SIZE;
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_GEOMETRY_SHADER);
			// Fallthrough
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			// All render area should be covered.
			minExpectedBlackPixels = 0;
			break;

		default:
			DE_ASSERT(0);
			break;
	}

	std::vector<VulkanShader> shaders;
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,		context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	tcu::TestLog&	log			= context.getTestContext().getLog();
	int				numPassed	= 0;

	static const struct
	{
		const char* const	desc;
		float				zPos;
	} cases[] =
	{
		{ "Draw primitives at near clipping plane, z = 0.0",	0.0f, },
		{ "Draw primitives at z = 0.5",							0.5f, },
		{ "Draw primitives at far clipping plane, z = 1.0",		1.0f, },
	};

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		log << tcu::TestLog::Message << cases[caseNdx].desc << tcu::TestLog::EndMessage;

		const std::vector<Vec4>		vertices			= genVertices(topology, Vec4(0.0f, 0.0f, cases[caseNdx].zPos, 0.0f), 0.0f);
		FrameBufferState			framebufferState	(RENDER_SIZE, RENDER_SIZE);
		PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
		DrawCallData				drawCallData		(topology, vertices);
		VulkanProgram				vulkanProgram		(shaders);

		VulkanDrawContext			drawContext			(context, framebufferState);
		drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
		drawContext.draw();

		const int numBlackPixels = countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
		if (numBlackPixels >= minExpectedBlackPixels)
			++numPassed;
	}

	return (numPassed == DE_LENGTH_OF_ARRAY(cases) ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

//! Primitives fully outside the clip volume.
tcu::TestStatus testPrimitivesOutside (Context& context, const VkPrimitiveTopology topology)
{
	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_GEOMETRY_SHADER);
			break;
		default:
			break;
	}

	std::vector<VulkanShader> shaders;
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,		context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	tcu::TestLog&	log			= context.getTestContext().getLog();
	int				numPassed	= 0;

	static const struct
	{
		const char* const	desc;
		float				zPos;
	} cases[] =
	{
		{ "Draw primitives in front of the near clipping plane, z < 0.0",	-0.5f, },
		{ "Draw primitives behind the far clipping plane, z > 1.0",			 1.5f, },
	};

	log << tcu::TestLog::Message << "Drawing primitives outside the clip volume. Expecting an empty image." << tcu::TestLog::EndMessage;

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		log << tcu::TestLog::Message << cases[caseNdx].desc << tcu::TestLog::EndMessage;

		const std::vector<Vec4>		vertices			= genVertices(topology, Vec4(0.0f, 0.0f, cases[caseNdx].zPos, 0.0f), 0.0f);
		FrameBufferState			framebufferState	(RENDER_SIZE, RENDER_SIZE);
		PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
		DrawCallData				drawCallData		(topology, vertices);
		VulkanProgram				vulkanProgram		(shaders);

		VulkanDrawContext			drawContext			(context, framebufferState);
		drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
		drawContext.draw();

		// All pixels must be black -- nothing is drawn.
		const int numBlackPixels = countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
		if (numBlackPixels == NUM_RENDER_PIXELS)
			++numPassed;
	}

	return (numPassed == DE_LENGTH_OF_ARRAY(cases) ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

//! Primitives partially outside the clip volume, but depth clamped
tcu::TestStatus testPrimitivesDepthClamp (Context& context, const VkPrimitiveTopology topology)
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_DEPTH_CLAMP);

	std::vector<VulkanShader> shaders;
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,		context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	const int		numCases		= 4;
	const IVec2		regionSize		= IVec2(RENDER_SIZE/2, RENDER_SIZE);	//! size of the clamped region
	const int		regionPixels	= regionSize.x() * regionSize.y();
	tcu::TestLog&	log				= context.getTestContext().getLog();
	int				numPassed		= 0;

	static const struct
	{
		const char* const	desc;
		float				zPos;
		bool				depthClampEnable;
		IVec2				regionOffset;
		Vec4				color;
	} cases[numCases] =
	{
		{ "Draw primitives intersecting the near clipping plane, depth clamp disabled",	-0.5f,	false,	IVec2(0, 0),				Vec4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the near clipping plane, depth clamp enabled",	-0.5f,	true,	IVec2(0, 0),				Vec4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the far clipping plane, depth clamp disabled",	 0.5f,	false,	IVec2(RENDER_SIZE/2, 0),	Vec4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the far clipping plane, depth clamp enabled",	 0.5f,	true,	IVec2(RENDER_SIZE/2, 0),	Vec4(1.0f, 1.0f, 0.0f, 1.0f) },
	};

	// Per case minimum number of colored pixels.
	int caseMinPixels[numCases] = { 0, 0, 0, 0 };

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			caseMinPixels[0] = caseMinPixels[2] = regionPixels - 1;
			caseMinPixels[1] = caseMinPixels[3] = 2;
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_GEOMETRY_SHADER);
			// Fallthrough
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			caseMinPixels[0] = regionPixels;
			caseMinPixels[1] = RENDER_SIZE - 2;
			caseMinPixels[2] = regionPixels;
			caseMinPixels[3] = 2 * (RENDER_SIZE - 2);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_GEOMETRY_SHADER);
			// Fallthrough
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			caseMinPixels[0] = caseMinPixels[1] = caseMinPixels[2] = caseMinPixels[3] = regionPixels;
			break;

		default:
			DE_ASSERT(0);
			break;
	}

	for (int caseNdx = 0; caseNdx < numCases; ++caseNdx)
	{
		log << tcu::TestLog::Message << cases[caseNdx].desc << tcu::TestLog::EndMessage;

		const std::vector<Vec4>		vertices			= genVertices(topology, Vec4(0.0f, 0.0f, cases[caseNdx].zPos, 0.0f), 1.0f);
		FrameBufferState			framebufferState	(RENDER_SIZE, RENDER_SIZE);
		PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
		pipelineState.depthClampEnable					= cases[caseNdx].depthClampEnable;
		DrawCallData				drawCallData		(topology, vertices);
		VulkanProgram				vulkanProgram		(shaders);

		VulkanDrawContext			drawContext			(context, framebufferState);
		drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
		drawContext.draw();

		const int numPixels = countPixels(drawContext.getColorPixels(), cases[caseNdx].regionOffset, regionSize, cases[caseNdx].color, Vec4());

		if (numPixels >= caseMinPixels[caseNdx])
			++numPassed;
	}

	return (numPassed == numCases ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

//! Primitives partially outside the clip volume, but depth clipped with explicit depth clip control
tcu::TestStatus testPrimitivesDepthClip (Context& context, const VkPrimitiveTopology topology)
{
	if (!context.getDepthClipEnableFeaturesEXT().depthClipEnable)
		throw tcu::NotSupportedError("VK_EXT_depth_clip_enable not supported");

	std::vector<VulkanShader> shaders;
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,		context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	const int		numCases		= 4;
	const IVec2		regionSize		= IVec2(RENDER_SIZE/2, RENDER_SIZE);	//! size of the clamped region
	const int		regionPixels	= regionSize.x() * regionSize.y();
	tcu::TestLog&	log				= context.getTestContext().getLog();
	int				numPassed		= 0;

	static const struct
	{
		const char* const	desc;
		float				zPos;
		bool				depthClipEnable;
		IVec2				regionOffset;
		Vec4				color;
	} cases[numCases] =
	{
		{ "Draw primitives intersecting the near clipping plane, depth clip enabled",	-0.5f,	true,	IVec2(0, 0),				Vec4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the near clipping plane, depth clip disabled",	-0.5f,	false,	IVec2(0, 0),				Vec4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the far clipping plane, depth clip enabled",	 0.5f,	true,	IVec2(RENDER_SIZE/2, 0),	Vec4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ "Draw primitives intersecting the far clipping plane, depth clip disabled",	 0.5f,	false,	IVec2(RENDER_SIZE/2, 0),	Vec4(1.0f, 1.0f, 0.0f, 1.0f) },
	};

	// Per case minimum number of colored pixels.
	int caseMinPixels[numCases] = { 0, 0, 0, 0 };

	switch (topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			caseMinPixels[0] = caseMinPixels[2] = regionPixels - 1;
			caseMinPixels[1] = caseMinPixels[3] = 2;
			break;

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_GEOMETRY_SHADER);
			// Fallthrough
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			caseMinPixels[0] = regionPixels;
			caseMinPixels[1] = RENDER_SIZE - 2;
			caseMinPixels[2] = regionPixels;
			caseMinPixels[3] = 2 * (RENDER_SIZE - 2);
			break;

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_GEOMETRY_SHADER);
			// Fallthrough
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			caseMinPixels[0] = caseMinPixels[1] = caseMinPixels[2] = caseMinPixels[3] = regionPixels;
			break;

		default:
			DE_ASSERT(0);
			break;
	}

	// Test depth clip with depth clamp disabled.
	numPassed = 0;
	for (int caseNdx = 0; caseNdx < numCases; ++caseNdx)
	{
		log << tcu::TestLog::Message << cases[caseNdx].desc << tcu::TestLog::EndMessage;

		const std::vector<Vec4>		vertices			= genVertices(topology, Vec4(0.0f, 0.0f, cases[caseNdx].zPos, 0.0f), 1.0f);
		FrameBufferState			framebufferState	(RENDER_SIZE, RENDER_SIZE);
		PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
		pipelineState.depthClampEnable					= false;
		pipelineState.explicitDepthClipEnable			= true;
		pipelineState.depthClipEnable					= cases[caseNdx].depthClipEnable;
		DrawCallData				drawCallData		(topology, vertices);
		VulkanProgram				vulkanProgram		(shaders);

		VulkanDrawContext			drawContext(context, framebufferState);
		drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
		drawContext.draw();

		const int numPixels = countPixels(drawContext.getColorPixels(), cases[caseNdx].regionOffset, regionSize, cases[caseNdx].color, Vec4());

		if (numPixels >= caseMinPixels[caseNdx])
			++numPassed;
	}

#ifdef CTS_USES_VULKANSC
	if (context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
	{
		if (numPassed < numCases)
			return tcu::TestStatus::fail("Rendered image(s) are incorrect (depth clip with depth clamp disabled)");
	}

	// Test depth clip with depth clamp enabled.
	numPassed = 0;
	if (getPhysicalDeviceFeatures(context.getInstanceInterface(), context.getPhysicalDevice()).depthClamp)
	{
		for (int caseNdx = 0; caseNdx < numCases; ++caseNdx)
		{
			log << tcu::TestLog::Message << cases[caseNdx].desc << tcu::TestLog::EndMessage;

			const std::vector<Vec4>		vertices			= genVertices(topology, Vec4(0.0f, 0.0f, cases[caseNdx].zPos, 0.0f), 1.0f);
			FrameBufferState			framebufferState	(RENDER_SIZE, RENDER_SIZE);
			PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
			pipelineState.depthClampEnable					= true;
			pipelineState.explicitDepthClipEnable			= true;
			pipelineState.depthClipEnable					= cases[caseNdx].depthClipEnable;
			DrawCallData				drawCallData		(topology, vertices);
			VulkanProgram				vulkanProgram		(shaders);

			VulkanDrawContext			drawContext(context, framebufferState);
			drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
			drawContext.draw();

			const int numPixels = countPixels(drawContext.getColorPixels(), cases[caseNdx].regionOffset, regionSize, cases[caseNdx].color, Vec4());

			if (numPixels >= caseMinPixels[caseNdx])
				++numPassed;
		}

		if (numPassed < numCases)
			return tcu::TestStatus::fail("Rendered image(s) are incorrect (depth clip with depth clamp enabled)");
	}

	return tcu::TestStatus::pass("OK");
}

//! Large point clipping
//! Spec: If the primitive under consideration is a point, then clipping passes it unchanged if it lies within the clip volume;
//!       otherwise, it is discarded.
tcu::TestStatus testLargePoints (Context& context)
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_LARGE_POINTS);

	bool pointClippingOutside = true;

	if (context.isDeviceFunctionalitySupported("VK_KHR_maintenance2"))
	{
		VkPointClippingBehavior clippingBehavior = getClippingBehavior(context.getInstanceInterface(), context.getPhysicalDevice());

		switch (clippingBehavior)
		{
			case VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES:		pointClippingOutside = true;				break;
			case VK_POINT_CLIPPING_BEHAVIOR_USER_CLIP_PLANES_ONLY:	pointClippingOutside = false;				break;
			case invalidClippingBehavior:							TCU_FAIL("Clipping behavior read failure");	// Does not fall through
			default:
			{
				TCU_FAIL("Unexpected clipping behavior reported");
			}
		}
	}

	std::vector<VulkanShader> shaders;
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,		context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	std::vector<Vec4> vertices;
	{
		const float delta	= 0.1f;  // much smaller than the point size
		const float p		= 1.0f + delta;

		vertices.push_back(Vec4(  -p,   -p, 0.1f, 1.0f));
		vertices.push_back(Vec4(  -p,    p, 0.2f, 1.0f));
		vertices.push_back(Vec4(   p,    p, 0.4f, 1.0f));
		vertices.push_back(Vec4(   p,   -p, 0.6f, 1.0f));
		vertices.push_back(Vec4(0.0f,   -p, 0.8f, 1.0f));
		vertices.push_back(Vec4(   p, 0.0f, 0.7f, 1.0f));
		vertices.push_back(Vec4(0.0f,    p, 0.5f, 1.0f));
		vertices.push_back(Vec4(  -p, 0.0f, 0.3f, 1.0f));
	}

	tcu::TestLog&	log	= context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Drawing several large points just outside the clip volume. Expecting an empty image or all points rendered." << tcu::TestLog::EndMessage;

	FrameBufferState			framebufferState	(RENDER_SIZE, RENDER_SIZE);
	PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
	DrawCallData				drawCallData		(VK_PRIMITIVE_TOPOLOGY_POINT_LIST, vertices);
	VulkanProgram				vulkanProgram		(shaders);

	VulkanDrawContext			drawContext(context, framebufferState);
	drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
	drawContext.draw();

	// Popful case: All pixels must be black -- nothing is drawn.
	const int	numBlackPixels	= countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
	bool		result			= false;

	// Pop-free case: All points must be rendered.
	bool allPointsRendered = true;
	for (std::vector<Vec4>::iterator i = vertices.begin(); i != vertices.end(); ++i)
	{
		if (countPixels(drawContext.getColorPixels(), Vec4(1.0f, i->z(), 0.0f, 1.0f), Vec4(0.01f)) == 0)
			allPointsRendered = false;
	}

	if (pointClippingOutside)
	{
		result = (numBlackPixels == NUM_RENDER_PIXELS || allPointsRendered);
	}
	else
	{
		// Rendering pixels without clipping: all points should be drawn.
		result = (allPointsRendered == true);
	}

	return (result ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

class WideLineVertexShader : public rr::VertexShader
{
public:
	WideLineVertexShader (void)
		: rr::VertexShader(1, 1)
	{
		m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			const tcu::Vec4 position = rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx);

			packets[packetNdx]->position = position;
			packets[packetNdx]->outputs[0] = position;
		}
	}
};

class WideLineFragmentShader : public rr::FragmentShader
{
public:
	WideLineFragmentShader (void)
		: rr::FragmentShader(1, 1)
	{
		m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments (rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
			{
				const float depth = rr::readVarying<float>(packets[packetNdx], context, 0, fragNdx).z();
				rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, tcu::Vec4(1.0f, depth, 0.0f, 1.0f));
			}
		}
	}
};
//! Wide line clipping
tcu::TestStatus testWideLines (Context& context, const LineOrientation lineOrientation)
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_WIDE_LINES);

	std::vector<VulkanShader> shaders;
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,		context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	const float delta = 0.1f;  // much smaller than the line width

	std::vector<Vec4> vertices;
	if (lineOrientation == LINE_ORIENTATION_AXIS_ALIGNED)
	{
		// Axis-aligned lines just outside the clip volume.
		const float p = 1.0f + delta;
		const float q = 0.9f;

		vertices.push_back(Vec4(-p, -q, 0.1f, 1.0f));
		vertices.push_back(Vec4(-p,  q, 0.9f, 1.0f));	// line 0
		vertices.push_back(Vec4(-q,  p, 0.1f, 1.0f));
		vertices.push_back(Vec4( q,  p, 0.9f, 1.0f));	// line 1
		vertices.push_back(Vec4( p,  q, 0.1f, 1.0f));
		vertices.push_back(Vec4( p, -q, 0.9f, 1.0f));	// line 2
		vertices.push_back(Vec4( q, -p, 0.1f, 1.0f));
		vertices.push_back(Vec4(-q, -p, 0.9f, 1.0f));	// line 3
	}
	else if (lineOrientation == LINE_ORIENTATION_DIAGONAL)
	{
		// Diagonal lines just outside the clip volume.
		const float p = 2.0f + delta;

		vertices.push_back(Vec4(  -p, 0.0f, 0.1f, 1.0f));
		vertices.push_back(Vec4(0.0f,   -p, 0.9f, 1.0f));	// line 0
		vertices.push_back(Vec4(0.0f,   -p, 0.1f, 1.0f));
		vertices.push_back(Vec4(   p, 0.0f, 0.9f, 1.0f));	// line 1
		vertices.push_back(Vec4(   p, 0.0f, 0.1f, 1.0f));
		vertices.push_back(Vec4(0.0f,    p, 0.9f, 1.0f));	// line 2
		vertices.push_back(Vec4(0.0f,    p, 0.1f, 1.0f));
		vertices.push_back(Vec4(  -p, 0.0f, 0.9f, 1.0f));	// line 3
	}
	else
		DE_ASSERT(0);

	const VkPhysicalDeviceLimits limits = getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits;

	const float		lineWidth	= std::min(static_cast<float>(RENDER_SIZE), limits.lineWidthRange[1]);
	const bool		strictLines	= limits.strictLines;
	tcu::TestLog&	log			= context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Drawing several wide lines just outside the clip volume. Expecting an empty image or all lines rendered." << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Line width is " << lineWidth << "." << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "strictLines is " << (strictLines ? "VK_TRUE." : "VK_FALSE.") << tcu::TestLog::EndMessage;

	FrameBufferState			framebufferState	(RENDER_SIZE, RENDER_SIZE);
	PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
	DrawCallData				drawCallData		(VK_PRIMITIVE_TOPOLOGY_LINE_LIST, vertices);
	VulkanProgram				vulkanProgram		(shaders);

	VulkanDrawContext			drawContext(context, framebufferState);
	drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
	drawContext.draw();

	// Popful case: All pixels must be black -- nothing is drawn.
	if (countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4()) == NUM_RENDER_PIXELS)
	{
		return tcu::TestStatus::pass("OK");
	}
	// Pop-free case: All lines must be rendered.
	else
	{
		const float					halfWidth		= lineWidth / float(RENDER_SIZE);
		std::vector<Vec4>			refVertices;

		// Create reference primitives
		for (deUint32 lineNdx = 0u; lineNdx < (deUint32)vertices.size() / 2u; lineNdx++)
		{
			const deUint32	vertexNdx0			= 2 * lineNdx;
			const deUint32	vertexNdx1			= 2 * lineNdx + 1;

			const bool		xMajorAxis			= deFloatAbs(vertices[vertexNdx1].x() - vertices[vertexNdx0].x()) >= deFloatAbs(vertices[vertexNdx1].y() - vertices[vertexNdx0].y());
			const tcu::Vec2	lineDir				= tcu::normalize(tcu::Vec2(vertices[vertexNdx1].x() - vertices[vertexNdx0].x(), vertices[vertexNdx1].y() - vertices[vertexNdx0].y()));
			const tcu::Vec4	lineNormalDir		= (strictLines)	? tcu::Vec4(lineDir.y(), -lineDir.x(), 0.0f, 0.0f)							// Line caps are perpendicular to the direction of the line segment.
												: (xMajorAxis)	? tcu::Vec4(0.0f, 1.0f, 0.0f, 0.0f) : tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f);	// Line caps are aligned to the minor axis

			const tcu::Vec4	wideLineVertices[]	=
			{
				tcu::Vec4(vertices[vertexNdx0] + lineNormalDir * halfWidth),
				tcu::Vec4(vertices[vertexNdx0] - lineNormalDir * halfWidth),
				tcu::Vec4(vertices[vertexNdx1] - lineNormalDir * halfWidth),
				tcu::Vec4(vertices[vertexNdx1] + lineNormalDir * halfWidth)
			};

			// 1st triangle
			refVertices.push_back(wideLineVertices[0]);
			refVertices.push_back(wideLineVertices[1]);
			refVertices.push_back(wideLineVertices[2]);

			// 2nd triangle
			refVertices.push_back(wideLineVertices[0]);
			refVertices.push_back(wideLineVertices[2]);
			refVertices.push_back(wideLineVertices[3]);
		}

		std::shared_ptr<rr::VertexShader>	vertexShader	= std::make_shared<WideLineVertexShader>();
		std::shared_ptr<rr::FragmentShader>	fragmentShader	= std::make_shared<WideLineFragmentShader>();

		// Draw wide line was two triangles
		DrawCallData				refCallData			(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, refVertices);

		ReferenceDrawContext		refDrawContext		(framebufferState);
		refDrawContext.registerDrawObject( pipelineState, vertexShader, fragmentShader, refCallData );
		refDrawContext.draw();

		if (tcu::intThresholdCompare(log, "Compare", "Result comparsion", refDrawContext.getColorPixels(), drawContext.getColorPixels(), tcu::UVec4(1), tcu::COMPARE_LOG_ON_ERROR))
			return tcu::TestStatus::pass("OK");
	}

	return tcu::TestStatus::fail("Rendered image(s) are incorrect");
}

} // ClipVolume ns

namespace ClipDistance
{

struct CaseDefinition
{
	const VkPrimitiveTopology	topology;
	const bool					dynamicIndexing;
	const bool					enableTessellation;
	const bool					enableGeometry;
	const int					numClipDistances;
	const int					numCullDistances;
	const bool					readInFragmentShader;

	CaseDefinition (const VkPrimitiveTopology	topology_,
					const int					numClipDistances_,
					const int					numCullDistances_,
					const bool					enableTessellation_,
					const bool					enableGeometry_,
					const bool					dynamicIndexing_,
					const bool					readInFragmentShader_)
		: topology				(topology_)
		, dynamicIndexing		(dynamicIndexing_)
		, enableTessellation	(enableTessellation_)
		, enableGeometry		(enableGeometry_)
		, numClipDistances		(numClipDistances_)
		, numCullDistances		(numCullDistances_)
		, readInFragmentShader	(readInFragmentShader_)
	{
	}
};

void initPrograms (SourceCollections& programCollection, const CaseDefinition caseDef)
{
	DE_ASSERT(caseDef.numClipDistances + caseDef.numCullDistances <= MAX_COMBINED_CLIP_AND_CULL_DISTANCES);

	std::string perVertexBlock;
	{
		std::ostringstream str;
		str << "gl_PerVertex {\n"
			<< "    vec4  gl_Position;\n";
		if (caseDef.numClipDistances > 0)
			str << "    float gl_ClipDistance[" << caseDef.numClipDistances << "];\n";
		if (caseDef.numCullDistances > 0)
			str << "    float gl_CullDistance[" << caseDef.numCullDistances << "];\n";
		str << "}";
		perVertexBlock = str.str();
	}

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 v_position;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "out " << perVertexBlock << ";\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = v_position;\n"
			<< "    out_color   = vec4(1.0, 0.5 * (v_position.x + 1.0), 0.0, 1.0);\n"
			<< "\n"
			<< "    const int barNdx = gl_VertexIndex / 6;\n";
		if (caseDef.dynamicIndexing)
		{
			if (caseDef.numClipDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numClipDistances << "; ++i)\n"
					<< "        gl_ClipDistance[i] = (barNdx == i ? v_position.y : 0.0);\n";
			if (caseDef.numCullDistances > 0)
			{
				src << "    for (int i = 0; i < " << caseDef.numCullDistances << "; ++i)\n";
				if (!caseDef.readInFragmentShader)
				{
					src << "		gl_CullDistance[i] = (gl_Position.x >= 0.75f) ? -0.5f : 0.5f;\n";
				}
				else
				{
					if (caseDef.enableTessellation || caseDef.enableGeometry)
						src << "        gl_CullDistance[i] = 0.1f;\n";
					else
						src << "        gl_CullDistance[i] = (gl_Position.y < 0) ? -0.5f : 0.5f;\n";
				}
			}
		}
		else
		{
			for (int i = 0; i < caseDef.numClipDistances; ++i)
				src << "    gl_ClipDistance[" << i << "] = (barNdx == " << i << " ? v_position.y : 0.0);\n";

			for (int i = 0; i < caseDef.numCullDistances; ++i)
			{
				if (!caseDef.readInFragmentShader)
				{
					src << "    gl_CullDistance[" << i << "] = (gl_Position.x >= 0.75f) ? -0.5f : 0.5f;\n";
				}
				else
				{
					if (caseDef.enableTessellation || caseDef.enableGeometry)
						src << "    gl_CullDistance[" << i << "] = 0.1f;\n";
					else
						src << "    gl_CullDistance[" << i << "] = (gl_Position.y < 0) ? -0.5f : 0.5f;\n";
				}
			}
		}
		src	<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	if (caseDef.enableTessellation)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(vertices = " << NUM_PATCH_CONTROL_POINTS << ") out;\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color[];\n"
			<< "\n"
			<< "in " << perVertexBlock << " gl_in[gl_MaxPatchVertices];\n"
			<< "\n"
			<< "out " << perVertexBlock << " gl_out[];\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_TessLevelInner[0] = 1.0;\n"
			<< "    gl_TessLevelInner[1] = 1.0;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 1.0;\n"
			<< "    gl_TessLevelOuter[1] = 1.0;\n"
			<< "    gl_TessLevelOuter[2] = 1.0;\n"
			<< "    gl_TessLevelOuter[3] = 1.0;\n"
			<< "\n"
			<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "    out_color[gl_InvocationID]          = in_color[gl_InvocationID];\n"
			<< "\n";
		if (caseDef.dynamicIndexing)
		{
			if (caseDef.numClipDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numClipDistances << "; ++i)\n"
					<< "        gl_out[gl_InvocationID].gl_ClipDistance[i] = gl_in[gl_InvocationID].gl_ClipDistance[i];\n";
			if (caseDef.numCullDistances > 0)
			{
				src << "    for (int i = 0; i < " << caseDef.numCullDistances << "; ++i)\n";
				src << "    {\n";
				if (!caseDef.readInFragmentShader)
				{
					src << "    gl_out[gl_InvocationID].gl_CullDistance[i] = (gl_in[gl_InvocationID].gl_Position.x >= 0.75f) ? -0.5f : 0.5f;\n";
				}
				else
				{
					src << "        gl_out[gl_InvocationID].gl_CullDistance[i] = (gl_in[gl_InvocationID].gl_CullDistance[i] == 0.1f) ? ";
					if (caseDef.enableGeometry)
						src << "0.3f";
					else
						src << "((gl_in[gl_InvocationID].gl_Position.y < 0) ? -0.5f : 0.5f)";
					src << " : 0.2f;\n";
				}
				src << "    }\n";
			}
		}
		else
		{
			for (int i = 0; i < caseDef.numClipDistances; ++i)
				src << "    gl_out[gl_InvocationID].gl_ClipDistance[" << i << "] = gl_in[gl_InvocationID].gl_ClipDistance[" << i << "];\n";
			for (int i = 0; i < caseDef.numCullDistances; ++i)
			{
				if (!caseDef.readInFragmentShader)
				{
					src << "    gl_out[gl_InvocationID].gl_CullDistance[" << i << "] = (gl_in[gl_InvocationID].gl_Position.x >= 0.75f) ? -0.5f : 0.5f;\n";
				}
				else
				{
					src << "    gl_out[gl_InvocationID].gl_CullDistance[" << i << "] = (gl_in[gl_InvocationID].gl_CullDistance[" << i << "] == 0.1f) ? ";
					if (caseDef.enableGeometry)
						src << "0.3f";
					else
						src << "((gl_in[gl_InvocationID].gl_Position.y < 0) ? -0.5f : 0.5f)";
					src << " : 0.2f;\n";
				}
			}
		}
		src << "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	if (caseDef.enableTessellation)
	{
		DE_ASSERT(NUM_PATCH_CONTROL_POINTS == 3);  // assumed in shader code

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(triangles, equal_spacing, ccw) in;\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "in " << perVertexBlock << " gl_in[gl_MaxPatchVertices];\n"
			<< "\n"
			<< "out " << perVertexBlock << ";\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    vec3 px     = gl_TessCoord.x * gl_in[0].gl_Position.xyz;\n"
			<< "    vec3 py     = gl_TessCoord.y * gl_in[1].gl_Position.xyz;\n"
			<< "    vec3 pz     = gl_TessCoord.z * gl_in[2].gl_Position.xyz;\n"
			<< "    gl_Position = vec4(px + py + pz, 1.0);\n"
			<< "    out_color   = (in_color[0] + in_color[1] + in_color[2]) / 3.0;\n"
			<< "\n";
		if (caseDef.dynamicIndexing)
		{
			if (caseDef.numClipDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numClipDistances << "; ++i)\n"
					<< "        gl_ClipDistance[i] = gl_TessCoord.x * gl_in[0].gl_ClipDistance[i]\n"
					<< "                           + gl_TessCoord.y * gl_in[1].gl_ClipDistance[i]\n"
					<< "                           + gl_TessCoord.z * gl_in[2].gl_ClipDistance[i];\n";
			if (caseDef.numCullDistances > 0)
				src << "    for (int i = 0; i < " << caseDef.numCullDistances << "; ++i)\n"
					<< "        gl_CullDistance[i] = gl_TessCoord.x * gl_in[0].gl_CullDistance[i]\n"
					<< "                           + gl_TessCoord.y * gl_in[1].gl_CullDistance[i]\n"
					<< "                           + gl_TessCoord.z * gl_in[2].gl_CullDistance[i];\n";
		}
		else
		{
			for (int i = 0; i < caseDef.numClipDistances; ++i)
				src << "    gl_ClipDistance[" << i << "] = gl_TessCoord.x * gl_in[0].gl_ClipDistance[" << i << "]\n"
					<< "                       + gl_TessCoord.y * gl_in[1].gl_ClipDistance[" << i << "]\n"
					<< "                       + gl_TessCoord.z * gl_in[2].gl_ClipDistance[" << i << "];\n";
			for (int i = 0; i < caseDef.numCullDistances; ++i)
				src << "    gl_CullDistance[" << i << "] = gl_TessCoord.x * gl_in[0].gl_CullDistance[" << i << "]\n"
					<< "                       + gl_TessCoord.y * gl_in[1].gl_CullDistance[" << i << "]\n"
					<< "                       + gl_TessCoord.z * gl_in[2].gl_CullDistance[" << i << "];\n";
		}
		src << "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	if (caseDef.enableGeometry)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(triangles) in;\n"
			<< "layout(triangle_strip, max_vertices = 3) out;\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color[];\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "in " << perVertexBlock << " gl_in[];\n"
			<< "\n"
			<< "out " << perVertexBlock << ";\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n";
		for (int vertNdx = 0; vertNdx < 3; ++vertNdx)
		{
			if (vertNdx > 0)
				src << "\n";
			src << "    gl_Position = gl_in[" << vertNdx << "].gl_Position;\n"
				<< "    out_color   = in_color[" << vertNdx << "];\n";
			if (caseDef.dynamicIndexing)
			{
				if (caseDef.numClipDistances > 0)
					src << "    for (int i = 0; i < " << caseDef.numClipDistances << "; ++i)\n"
						<< "        gl_ClipDistance[i] = gl_in[" << vertNdx << "].gl_ClipDistance[i];\n";
				if (caseDef.numCullDistances > 0)
				{
					src << "    for (int i = 0; i < " << caseDef.numCullDistances << "; ++i)\n";
					src << "    {\n";
					if (!caseDef.readInFragmentShader)
					{
						src << "    gl_CullDistance[i] = (gl_in[" << vertNdx << "].gl_Position.x >= 0.75f) ? -0.5f : 0.5f;\n";
					}
					else
					{
						src << "        gl_CullDistance[i] = (gl_in[" << vertNdx << "].gl_CullDistance[i] == ";
						if (caseDef.enableTessellation)
							src << "0.3f";
						else
							src << "0.1f";
						src << ") ? ((gl_in[" << vertNdx << "].gl_Position.y < 0) ? -0.5f : 0.5f) : 0.4f;\n";
					}
					src << "    }\n";
				}
			}
			else
			{
				for (int i = 0; i < caseDef.numClipDistances; ++i)
					src << "    gl_ClipDistance[" << i << "] = gl_in[" << vertNdx << "].gl_ClipDistance[" << i << "];\n";

				for (int i = 0; i < caseDef.numCullDistances; ++i)
				{
					if (!caseDef.readInFragmentShader)
					{
						src << "    gl_CullDistance[" << i << "] = (gl_in[" << vertNdx << "].gl_Position.x >= 0.75f) ? -0.5f : 0.5f;\n";
					}
					else
					{
						src << "        gl_CullDistance[" << i << "] = (gl_in[" << vertNdx << "].gl_CullDistance[" << i << "] == ";
						if (caseDef.enableTessellation)
							src << "0.3f";
						else
							src << "0.1f";
						src << ") ? ((gl_in[" << vertNdx << "].gl_Position.y < 0) ? -0.5f : 0.5f) : 0.4f;\n";
					}
				}
			}
			src << "    EmitVertex();\n";
		}
		src	<< "}\n";

		programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in flat vec4 in_color;\n"
			<< "layout(location = 0) out vec4 o_color;\n";
		if (caseDef.readInFragmentShader)
		{
			if (caseDef.numClipDistances > 0)
				src << "in float gl_ClipDistance[" << caseDef.numClipDistances << "];\n";
			if (caseDef.numCullDistances > 0)
				src << "in float gl_CullDistance[" << caseDef.numCullDistances << "];\n";
		}
		src << "\n"
			<< "void main (void)\n"
			<< "{\n";

		if (caseDef.readInFragmentShader)
		{
			src << "    o_color = vec4(in_color.r, "
				<< (caseDef.numClipDistances > 0 ? std::string("gl_ClipDistance[") + de::toString(caseDef.numClipDistances / 2) + "], " : "0.0, ")
				<< (caseDef.numCullDistances > 0 ? std::string("gl_CullDistance[") + de::toString(caseDef.numCullDistances / 2) + "], " : "0.0, ")
				<< " 1.0);\n";
		}
		else
		{
			src << "    o_color = vec4(in_color.rgb + vec3(0.0, 0.0, 0.5), 1.0);\n";  // mix with a constant color in case variable wasn't passed correctly through stages
		}

		src << "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus testClipDistance (Context& context, const CaseDefinition caseDef)
{
	// Check test requirements
	{
		const InstanceInterface&		vki			= context.getInstanceInterface();
		const VkPhysicalDevice			physDevice	= context.getPhysicalDevice();
		const VkPhysicalDeviceLimits	limits		= getPhysicalDeviceProperties(vki, physDevice).limits;

		FeatureFlags requirements = (FeatureFlags)0;

		if (caseDef.numClipDistances > 0)
			requirements |= FEATURE_SHADER_CLIP_DISTANCE;
		if (caseDef.numCullDistances > 0)
			requirements |= FEATURE_SHADER_CULL_DISTANCE;
		if (caseDef.enableTessellation)
			requirements |= FEATURE_TESSELLATION_SHADER;
		if (caseDef.enableGeometry)
			requirements |= FEATURE_GEOMETRY_SHADER;

		requireFeatures(vki, physDevice, requirements);

		// Check limits for supported features

		if (caseDef.numClipDistances > 0 && limits.maxClipDistances < MAX_CLIP_DISTANCES)
			return tcu::TestStatus::fail("maxClipDistances smaller than the minimum required by the spec");
		if (caseDef.numCullDistances > 0 && limits.maxCullDistances < MAX_CULL_DISTANCES)
			return tcu::TestStatus::fail("maxCullDistances smaller than the minimum required by the spec");
		if (caseDef.numCullDistances > 0 && limits.maxCombinedClipAndCullDistances < MAX_COMBINED_CLIP_AND_CULL_DISTANCES)
			return tcu::TestStatus::fail("maxCombinedClipAndCullDistances smaller than the minimum required by the spec");
	}

	std::vector<VulkanShader> shaders;
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,		context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));
	if (caseDef.enableTessellation)
	{
		shaders.push_back(VulkanShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,	context.getBinaryCollection().get("tesc")));
		shaders.push_back(VulkanShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	context.getBinaryCollection().get("tese")));
	}
	if (caseDef.enableGeometry)
		shaders.push_back(VulkanShader(VK_SHADER_STAGE_GEOMETRY_BIT,	context.getBinaryCollection().get("geom")));

	const int numBars = MAX_COMBINED_CLIP_AND_CULL_DISTANCES;

	std::vector<Vec4> vertices;
	{
		const float	dx = 2.0f / numBars;
		for (int i = 0; i < numBars; ++i)
		{
			const float x = -1.0f + dx * static_cast<float>(i);

			vertices.push_back(Vec4(x,      -1.0f, 0.0f, 1.0f));
			vertices.push_back(Vec4(x,       1.0f, 0.0f, 1.0f));
			vertices.push_back(Vec4(x + dx, -1.0f, 0.0f, 1.0f));

			vertices.push_back(Vec4(x,       1.0f, 0.0f, 1.0f));
			vertices.push_back(Vec4(x + dx,  1.0f, 0.0f, 1.0f));
			vertices.push_back(Vec4(x + dx, -1.0f, 0.0f, 1.0f));
		}
	}

	tcu::TestLog& log = context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Drawing " << numBars << " colored bars, clipping the first " << caseDef.numClipDistances << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Using " << caseDef.numClipDistances << " ClipDistance(s) and " << caseDef.numCullDistances << " CullDistance(s)" << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Expecting upper half of the clipped bars to be black." << tcu::TestLog::EndMessage;

	FrameBufferState			framebufferState	(RENDER_SIZE, RENDER_SIZE);
	PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
	if (caseDef.enableTessellation)
		pipelineState.numPatchControlPoints = NUM_PATCH_CONTROL_POINTS;
	DrawCallData				drawCallData		(caseDef.topology, vertices);
	VulkanProgram				vulkanProgram		(shaders);

	VulkanDrawContext			drawContext			(context, framebufferState);
	drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
	drawContext.draw();

	// Count black pixels in the whole image.
	const int	numBlackPixels			= countPixels(drawContext.getColorPixels(), Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
	const IVec2	clipRegion				= IVec2(caseDef.numClipDistances * RENDER_SIZE / numBars, RENDER_SIZE / 2);
	// Cull is set to > 0.75 in the shader if caseDef.readInFragmentShader is false
	const int	barsCulled				= (int)deFloor((0.25f) / (1.0f / numBars));
	const IVec2	cullRegion				= (caseDef.readInFragmentShader || caseDef.numCullDistances == 0) ? IVec2(0.0f, 0.0f) : IVec2(barsCulled, RENDER_SIZE);
	const int	expectedClippedPixels	= clipRegion.x() * clipRegion.y() + cullRegion.x() * cullRegion.y();
	// Make sure the bottom half has no black pixels (possible if image became corrupted).
	const int	guardPixels				= countPixels(drawContext.getColorPixels(), IVec2(0, RENDER_SIZE/2), clipRegion, Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4());
	const bool	fragColorsOk			= caseDef.readInFragmentShader ? checkFragColors(drawContext.getColorPixels(), clipRegion, caseDef.numClipDistances / 2, caseDef.numCullDistances > 0) : true;

	return (numBlackPixels == expectedClippedPixels && guardPixels == 0 && fragColorsOk ? tcu::TestStatus::pass("OK")
																						: tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

} // ClipDistance ns

namespace ClipDistanceComplementarity
{

void initPrograms (SourceCollections& programCollection, const int numClipDistances)
{
	// Vertex shader
	{
		DE_ASSERT(numClipDistances > 0);
		const int clipDistanceLastNdx = numClipDistances - 1;

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 v_position;    // we are passing ClipDistance in w component\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4  gl_Position;\n"
			<< "    float gl_ClipDistance[" << numClipDistances << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position        = vec4(v_position.xyz, 1.0);\n";
		for (int i = 0; i < clipDistanceLastNdx; ++i)
			src << "    gl_ClipDistance[" << i << "] = 0.0;\n";
		src << "    gl_ClipDistance[" << clipDistanceLastNdx << "] = v_position.w;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = vec4(1.0, 1.0, 1.0, 0.5);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus testComplementarity (Context& context, const int numClipDistances)
{
	// Check test requirements
	{
		const InstanceInterface&		vki			= context.getInstanceInterface();
		const VkPhysicalDevice			physDevice	= context.getPhysicalDevice();

		requireFeatures(vki, physDevice, FEATURE_SHADER_CLIP_DISTANCE);
	}

	std::vector<VulkanShader> shaders;
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,		context.getBinaryCollection().get("vert")));
	shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag")));

	std::vector<Vec4> vertices;
	{
		de::Random	rnd						(1234);
		const int	numSections				= 16;
		const int	numVerticesPerSection	= 4;	// logical verticies, due to triangle list topology we actually use 6 per section

		DE_ASSERT(RENDER_SIZE_LARGE % numSections == 0);

		std::vector<float> clipDistances(numVerticesPerSection * numSections);
		for (int i = 0; i < static_cast<int>(clipDistances.size()); ++i)
			clipDistances[i] = rnd.getFloat(-1.0f, 1.0f);

		// Two sets of identical primitives, but with a different ClipDistance sign.
		for (int setNdx = 0; setNdx < 2; ++setNdx)
		{
			const float sign = (setNdx == 0 ? 1.0f : -1.0f);
			const float	dx	 = 2.0f / static_cast<float>(numSections);

			for (int i = 0; i < numSections; ++i)
			{
				const int	ndxBase	= numVerticesPerSection * i;
				const float x		= -1.0f + dx * static_cast<float>(i);
				const Vec4	p0		= Vec4(x,      -1.0f, 0.0f, sign * clipDistances[ndxBase + 0]);
				const Vec4	p1		= Vec4(x,       1.0f, 0.0f, sign * clipDistances[ndxBase + 1]);
				const Vec4	p2		= Vec4(x + dx,  1.0f, 0.0f, sign * clipDistances[ndxBase + 2]);
				const Vec4	p3		= Vec4(x + dx, -1.0f, 0.0f, sign * clipDistances[ndxBase + 3]);

				vertices.push_back(p0);
				vertices.push_back(p1);
				vertices.push_back(p2);

				vertices.push_back(p2);
				vertices.push_back(p3);
				vertices.push_back(p0);
			}
		}
	}

	tcu::TestLog& log = context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Draw two sets of primitives with blending, differing only with ClipDistance sign." << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Using " << numClipDistances << " clipping plane(s), one of them possibly having negative values." << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Expecting a uniform gray area, no missing (black) nor overlapped (white) pixels." << tcu::TestLog::EndMessage;

	FrameBufferState			framebufferState	(RENDER_SIZE_LARGE, RENDER_SIZE_LARGE);
	PipelineState				pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
	pipelineState.blendEnable	= true;
	DrawCallData				drawCallData		(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vertices);
	VulkanProgram				vulkanProgram		(shaders);

	VulkanDrawContext			drawContext			(context, framebufferState);
	drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
	drawContext.draw();

	const int numGrayPixels		= countPixels(drawContext.getColorPixels(), Vec4(0.5f, 0.5f, 0.5f, 1.0f), Vec4(0.02f, 0.02f, 0.02f, 0.0f));
	const int numExpectedPixels	= RENDER_SIZE_LARGE * RENDER_SIZE_LARGE;

	return (numGrayPixels == numExpectedPixels ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

} // ClipDistanceComplementarity ns

namespace CullDistance
{
	void checkSupport(Context& context)
	{
		const InstanceInterface&	vki			= context.getInstanceInterface();
		const VkPhysicalDevice		physDevice	= context.getPhysicalDevice();

		requireFeatures(vki, physDevice, FEATURE_SHADER_CULL_DISTANCE);
	}

	void initPrograms(SourceCollections& programCollection)
	{
		// setup triangle with three per-vertex cull distance values:
		// v0: gl_CullDistance = {  0.0,  0.0, -1.0 };
		// v1: gl_CullDistance = {  0.0, -1.0,  0.0 };
		// v2: gl_CullDistance = { -1.0,  0.0,  0.0 };
		// each vertex has a negative cull distance value but the triangle must not
		// be culled because none of the three half-spaces is negative for all vertices

		programCollection.glslSources.add("vert") << glu::VertexSource(
			"#version 450\n"
			"layout(location = 0) in vec4 v_position;\n"
			"out gl_PerVertex {\n"
			"  vec4  gl_Position;\n"
			"  float gl_CullDistance[3];\n"
			"};\n"
			"void main (void)\n"
			"{\n"
			"  gl_Position = v_position;\n"
			"  gl_CullDistance[0] = 0.0 - float(gl_VertexIndex == 2);\n"
			"  gl_CullDistance[1] = 0.0 - float(gl_VertexIndex == 1);\n"
			"  gl_CullDistance[2] = 0.0 - float(gl_VertexIndex == 0);\n"
			"}\n");

		programCollection.glslSources.add("frag") << glu::FragmentSource(
			"#version 450\n"
			"layout(location = 0) out vec4 o_color;\n"
			"void main (void)\n"
			"{\n"
			"  o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
			"}\n");
	}

	tcu::TestStatus testCullDistance(Context& context)
	{
		std::vector<VulkanShader> shaders
		{
			VulkanShader(VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("vert")),
			VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("frag"))
		};

		std::vector<Vec4> vertices
		{
			{ -3.0f,  0.0f, 0.0f, 1.0f },
			{  0.0f,  3.0f, 0.0f, 1.0f },
			{  0.0f, -3.0f, 0.0f, 1.0f },
		};

		VulkanProgram		vulkanProgram		(shaders);
		FrameBufferState	framebufferState	(RENDER_SIZE, RENDER_SIZE);
		PipelineState		pipelineState		(context.getDeviceProperties().limits.subPixelPrecisionBits);
		DrawCallData		drawCallData		(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices);
		VulkanDrawContext	drawContext			(context, framebufferState);

		drawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
		drawContext.draw();

		const int numDrawnPixels = countPixels(drawContext.getColorPixels(), Vec4(1.0f, 0.0f, 0.0f, 1.0f), Vec4(0.02f, 0.02f, 0.02f, 0.0f));
		const int numExpectedPixels = RENDER_SIZE * RENDER_SIZE / 2;

		// triangle should be drawn and half of framebuffer should be filled with red color
		if (numDrawnPixels == numExpectedPixels)
			return tcu::TestStatus::pass("OK");

		return tcu::TestStatus::fail("Triangle was not drawn");
	}

} // CullDistance

void checkTopologySupport(Context& context, const VkPrimitiveTopology topology)
{
#ifndef CTS_USES_VULKANSC
	if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
		context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().triangleFans)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
	}
#else
	DE_UNREF(context);
	DE_UNREF(topology);
#endif // CTS_USES_VULKANSC
}

void addClippingTests (tcu::TestCaseGroup* clippingTestsGroup)
{
	tcu::TestContext& testCtx = clippingTestsGroup->getTestContext();

	// Clipping against the clip volume
	{
		using namespace ClipVolume;

		static const VkPrimitiveTopology cases[] =
		{
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
			VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
		};

		MovePtr<tcu::TestCaseGroup> clipVolumeGroup(new tcu::TestCaseGroup(testCtx, "clip_volume", "clipping with the clip volume"));

		// Fully inside the clip volume
		{
			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "inside", ""));

			for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
				addFunctionCaseWithPrograms<VkPrimitiveTopology>(
					group.get(), getPrimitiveTopologyShortName(cases[caseNdx]), "", checkTopologySupport, initPrograms, testPrimitivesInside, cases[caseNdx]);

			clipVolumeGroup->addChild(group.release());
		}

		// Fully outside the clip volume
		{
			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "outside", ""));

			for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
				addFunctionCaseWithPrograms<VkPrimitiveTopology>(
					group.get(), getPrimitiveTopologyShortName(cases[caseNdx]), "", checkTopologySupport, initPrograms, testPrimitivesOutside, cases[caseNdx]);

			clipVolumeGroup->addChild(group.release());
		}

		// Depth clamping
		{
			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "depth_clamp", ""));

			for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
				addFunctionCaseWithPrograms<VkPrimitiveTopology>(
					group.get(), getPrimitiveTopologyShortName(cases[caseNdx]), "", checkTopologySupport, initPrograms, testPrimitivesDepthClamp, cases[caseNdx]);

			clipVolumeGroup->addChild(group.release());
		}

		// Depth clipping
		{
			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "depth_clip", ""));

			for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
				addFunctionCaseWithPrograms<VkPrimitiveTopology>(
					group.get(), getPrimitiveTopologyShortName(cases[caseNdx]), "", checkTopologySupport, initPrograms, testPrimitivesDepthClip, cases[caseNdx]);

			clipVolumeGroup->addChild(group.release());
		}

		// Large points and wide lines
		{
			// \note For both points and lines, if an unsupported size/width is selected, the nearest supported size will be chosen.
			//       We do have to check for feature support though.

			MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "clipped", ""));

			addFunctionCaseWithPrograms(group.get(), "large_points", "", initProgramsPointSize, testLargePoints);

			addFunctionCaseWithPrograms<LineOrientation>(group.get(), "wide_lines_axis_aligned", "", initPrograms, testWideLines, LINE_ORIENTATION_AXIS_ALIGNED);
			addFunctionCaseWithPrograms<LineOrientation>(group.get(), "wide_lines_diagonal",	 "", initPrograms, testWideLines, LINE_ORIENTATION_DIAGONAL);

			clipVolumeGroup->addChild(group.release());
		}

		clippingTestsGroup->addChild(clipVolumeGroup.release());
	}

	// User-defined clip planes
	{
		MovePtr<tcu::TestCaseGroup> clipDistanceGroup(new tcu::TestCaseGroup(testCtx, "user_defined", "user-defined clip planes"));

		// ClipDistance, CullDistance and maxCombinedClipAndCullDistances usage
		{
			using namespace ClipDistance;

			static const struct
			{
				const char* const	groupName;
				const char* const	description;
				bool				useCullDistance;
			} caseGroups[] =
			{
				{ "clip_distance",		"use ClipDistance",										false },
				{ "clip_cull_distance",	"use ClipDistance and CullDistance at the same time",	true  },
			};

			static const struct
			{
				const char* const	name;
				bool				readInFragmentShader;
			} fragmentShaderReads[] =
			{

				{ "",						false	},
				{ "_fragmentshader_read",	true	}
			};

			const deUint32 flagTessellation = 1u << 0;
			const deUint32 flagGeometry		= 1u << 1;

			for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(caseGroups); ++groupNdx)
			for (int indexingMode = 0; indexingMode < 2; ++indexingMode)
			{
				const bool			dynamicIndexing	= (indexingMode == 1);
				const std::string	mainGroupName	= de::toString(caseGroups[groupNdx].groupName) + (dynamicIndexing ? "_dynamic_index" : "");

				MovePtr<tcu::TestCaseGroup>	mainGroup(new tcu::TestCaseGroup(testCtx, mainGroupName.c_str(), ""));

				for (deUint32 shaderMask = 0u; shaderMask <= (flagTessellation | flagGeometry); ++shaderMask)
				{
					const bool			useTessellation	= (shaderMask & flagTessellation) != 0;
					const bool			useGeometry		= (shaderMask & flagGeometry) != 0;
					const std::string	shaderGroupName	= std::string("vert") + (useTessellation ? "_tess" : "") + (useGeometry ? "_geom" : "");

					MovePtr<tcu::TestCaseGroup>	shaderGroup(new tcu::TestCaseGroup(testCtx, shaderGroupName.c_str(), ""));

					for (int numClipPlanes = 1; numClipPlanes <= MAX_CLIP_DISTANCES; ++numClipPlanes)
					for (int fragmentShaderReadNdx = 0; fragmentShaderReadNdx < DE_LENGTH_OF_ARRAY(fragmentShaderReads); ++fragmentShaderReadNdx)
					{
						const int					numCullPlanes	= (caseGroups[groupNdx].useCullDistance
																		? std::min(static_cast<int>(MAX_CULL_DISTANCES), MAX_COMBINED_CLIP_AND_CULL_DISTANCES - numClipPlanes)
																		: 0);
						const std::string			caseName		= de::toString(numClipPlanes) + (numCullPlanes > 0 ? "_" + de::toString(numCullPlanes) : "") + de::toString(fragmentShaderReads[fragmentShaderReadNdx].name);
						const VkPrimitiveTopology	topology		= (useTessellation ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

						addFunctionCaseWithPrograms<CaseDefinition>(
							shaderGroup.get(), caseName, caseGroups[groupNdx].description, initPrograms, testClipDistance,
							CaseDefinition(topology, numClipPlanes, numCullPlanes, useTessellation, useGeometry, dynamicIndexing, fragmentShaderReads[fragmentShaderReadNdx].readInFragmentShader));
					}
					mainGroup->addChild(shaderGroup.release());
				}
				clipDistanceGroup->addChild(mainGroup.release());
			}
		}
		clippingTestsGroup->addChild(clipDistanceGroup.release());

		// Complementarity criterion (i.e. clipped and not clipped areas must add up to a complete primitive with no holes nor overlap)
		{
			using namespace ClipDistanceComplementarity;

			MovePtr<tcu::TestCaseGroup>	group(new tcu::TestCaseGroup(testCtx, "complementarity", ""));

			for (int numClipDistances = 1; numClipDistances <= MAX_CLIP_DISTANCES; ++numClipDistances)
				addFunctionCaseWithPrograms<int>(group.get(), de::toString(numClipDistances).c_str(), "", initPrograms, testComplementarity, numClipDistances);

			clippingTestsGroup->addChild(group.release());
		}

		{
			using namespace CullDistance;

			MovePtr<tcu::TestCaseGroup>	group(new tcu::TestCaseGroup(testCtx, "misc", ""));

			addFunctionCaseWithPrograms(group.get(), "negative_and_non_negative_cull_distance", "", checkSupport, initPrograms, testCullDistance);

			clippingTestsGroup->addChild(group.release());
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name.c_str(), "Clipping tests", addClippingTests);
}

} // clipping
} // vkt
