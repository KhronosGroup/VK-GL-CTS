/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2020 The Khronos Group Inc.
* Copyright (c) 2020 Valve Corporation.
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
*//*
 * \file
 * \brief Extended dynamic state tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineExtendedDynamicStateTests.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuVector.hpp"
#include "tcuMaybe.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <vector>
#include <sstream>
#include <algorithm>
#include <utility>
#include <iterator>
#include <string>
#include <limits>
#include <memory>
#include <functional>

namespace vkt
{
namespace pipeline
{

namespace
{

inline vk::VkBool32 makeVkBool32(bool value)
{
	return (value ? VK_TRUE : VK_FALSE);
}

// Framebuffer size.
constexpr deUint32	kFramebufferWidth	= 64u;
constexpr deUint32	kFramebufferHeight	= 64u;

// Image formats.
constexpr	vk::VkFormat	kColorFormat	= vk::VK_FORMAT_R8G8B8A8_UNORM;
const		tcu::Vec4		kColorThreshold	(0.005f); // 1/255 < 0.005 < 2/255.

struct DepthStencilFormat
{
	vk::VkFormat	imageFormat;
	float			depthThreshold;
};

const DepthStencilFormat kDepthStencilFormats[] =
{
	{ vk::VK_FORMAT_D32_SFLOAT_S8_UINT,	0.0f		},
	{ vk::VK_FORMAT_D24_UNORM_S8_UINT,	1.0e-07f	},	// 1/(2**24-1) < 1.0e-07f < 2/(2**24-1)
};

// Vertices in buffers will have 2 components and a padding to properly test the stride.
struct GeometryVertex
{
	tcu::Vec2 coords;
	tcu::Vec2 padding;

	GeometryVertex (const tcu::Vec2& coords_)
		: coords	(coords_)
		, padding	(0.0f)
	{
	}
};

constexpr auto kVertexStride	= static_cast<vk::VkDeviceSize>(sizeof(GeometryVertex));
constexpr auto kCoordsSize		= static_cast<vk::VkDeviceSize>(sizeof(GeometryVertex::coords));

// Stencil Operation parameters, as used in vkCmdSetStencilOpEXT().
struct StencilOpParams
{
	vk::VkStencilFaceFlags  faceMask;
	vk::VkStencilOp         failOp;
	vk::VkStencilOp         passOp;
	vk::VkStencilOp         depthFailOp;
	vk::VkCompareOp         compareOp;
};

const StencilOpParams kDefaultStencilOpParams =
{
	vk::VK_STENCIL_FACE_FRONT_AND_BACK,
	vk::VK_STENCIL_OP_KEEP,
	vk::VK_STENCIL_OP_KEEP,
	vk::VK_STENCIL_OP_KEEP,
	vk::VK_COMPARE_OP_ALWAYS
};

using ViewportVec	= std::vector<vk::VkViewport>;
using ScissorVec	= std::vector<vk::VkRect2D>;
using StencilOpVec	= std::vector<StencilOpParams>;

// Generic, to be used with any state than can be set statically and, as an option, dynamically.
template<typename T>
struct StaticAndDynamicPair
{
	T				staticValue;
	tcu::Maybe<T>	dynamicValue;

	// Helper constructor to set a static value and no dynamic value.
	StaticAndDynamicPair (const T& value)
		: staticValue	(value)
		, dynamicValue	(tcu::nothing<T>())
	{
	}

	// Helper constructor to set both.
	StaticAndDynamicPair (const T& sVal, const T& dVal)
		: staticValue	(sVal)
		, dynamicValue	(tcu::just<T>(dVal))
	{
	}

	// If the dynamic value is present, swap static and dynamic values.
	void swapValues (void)
	{
		if (!dynamicValue)
			return;
		std::swap(staticValue, dynamicValue.get());
	}
};

// For anything boolean, see below.
using BooleanFlagConfig = StaticAndDynamicPair<bool>;

// Configuration for every aspect of the extended dynamic state.
using CullModeConfig				= StaticAndDynamicPair<vk::VkCullModeFlags>;
using FrontFaceConfig				= StaticAndDynamicPair<vk::VkFrontFace>;
using TopologyConfig				= StaticAndDynamicPair<vk::VkPrimitiveTopology>;
using ViewportConfig				= StaticAndDynamicPair<ViewportVec>;	// At least one element.
using ScissorConfig					= StaticAndDynamicPair<ScissorVec>;		// At least one element.
using StrideConfig					= StaticAndDynamicPair<vk::VkDeviceSize>;
using DepthTestEnableConfig			= BooleanFlagConfig;
using DepthWriteEnableConfig		= BooleanFlagConfig;
using DepthCompareOpConfig			= StaticAndDynamicPair<vk::VkCompareOp>;
using DepthBoundsTestEnableConfig	= BooleanFlagConfig;
using StencilTestEnableConfig		= BooleanFlagConfig;
using StencilOpConfig				= StaticAndDynamicPair<StencilOpVec>;	// At least one element.

const tcu::Vec4	kDefaultTriangleColor	(0.0f, 0.0f, 1.0f, 1.0f);	// Opaque blue.
const tcu::Vec4	kDefaultClearColor		(0.0f, 0.0f, 0.0f, 1.0f);	// Opaque black.

struct MeshParams
{
	tcu::Vec4	color;
	float		depth;
	bool		reversed;
	float		scaleX;
	float		scaleY;
	float		offsetX;
	float		offsetY;

	MeshParams (const tcu::Vec4&	color_		= kDefaultTriangleColor,
				float				depth_		= 0.0f,
				bool				reversed_	= false,
				float				scaleX_		= 1.0f,
				float				scaleY_		= 1.0f,
				float				offsetX_	= 0.0f,
				float				offsetY_	= 0.0f)
		: color		(color_)
		, depth		(depth_)
		, reversed	(reversed_)
		, scaleX	(scaleX_)
		, scaleY	(scaleY_)
		, offsetX	(offsetX_)
		, offsetY	(offsetY_)
	{}
};

enum class SequenceOrdering
{
	CMD_BUFFER_START	= 0,	// Set state at the start of the command buffer.
	BEFORE_DRAW			= 1,	// After binding dynamic pipeline and just before drawing.
	BETWEEN_PIPELINES	= 2,	// After a static state pipeline has been bound but before the dynamic state pipeline has been bound.
	AFTER_PIPELINES		= 3,	// After a static state pipeline and a second dynamic state pipeline have been bound.
	BEFORE_GOOD_STATIC	= 4,	// Before a static state pipeline with the correct values has been bound.
	TWO_DRAWS_DYNAMIC	= 5,	// Bind bad static pipeline and draw, followed by binding correct dynamic pipeline and drawing again.
	TWO_DRAWS_STATIC	= 6,	// Bind bad dynamic pipeline and draw, followed by binding correct static pipeline and drawing again.
};

using ReferenceColorGenerator = std::function<void(tcu::PixelBufferAccess&)>;

// Most tests expect a single output color in the whole image.
class SingleColorGenerator
{
public:
	SingleColorGenerator (const tcu::Vec4& color)
		: m_color(color)
	{}

	void operator()(tcu::PixelBufferAccess& access)
	{
		constexpr auto kWidth	= static_cast<int>(kFramebufferWidth);
		constexpr auto kHeight	= static_cast<int>(kFramebufferHeight);

		for (int y = 0; y < kHeight; ++y)
		for (int x = 0; x < kWidth; ++x)
		{
			access.setPixel(m_color, x, y);
		}
	}

private:
	const tcu::Vec4 m_color;
};

// Some tests expect the upper half and the lower half having different color values.
class HorizontalSplitGenerator
{
public:
	HorizontalSplitGenerator (const tcu::Vec4& top, const tcu::Vec4& bottom)
		: m_top(top), m_bottom(bottom)
	{}

	void operator()(tcu::PixelBufferAccess& access)
	{
		constexpr auto kWidth		= static_cast<int>(kFramebufferWidth);
		constexpr auto kHeight		= static_cast<int>(kFramebufferHeight);
		constexpr auto kHalfHeight	= kHeight / 2;

		for (int y = 0; y < kHeight; ++y)
		for (int x = 0; x < kWidth; ++x)
		{
			const auto& color = (y < kHalfHeight ? m_top : m_bottom);
			access.setPixel(color, x, y);
		}
	}

private:
	const tcu::Vec4 m_top;
	const tcu::Vec4 m_bottom;
};

struct TestConfig
{
	// Main sequence ordering.
	SequenceOrdering			sequenceOrdering;

	// Drawing parameters: tests will draw one or more flat meshes of triangles covering the whole "screen".
	std::vector<MeshParams>		meshParams;			// Mesh parameters for each full-screen layer of geometry.
	deUint32					referenceStencil;	// Reference stencil value.

	// Clearing parameters for the framebuffer.
	tcu::Vec4					clearColorValue;
	float						clearDepthValue;
	deUint32					clearStencilValue;

	// Expected output in the attachments.
	ReferenceColorGenerator		referenceColor;
	float						expectedDepth;
	deUint32					expectedStencil;

	// Depth bounds parameters for the pipeline.
	float						minDepthBounds;
	float						maxDepthBounds;

	// Force inclusion of passthrough geometry shader or not.
	bool						forceGeometryShader;

	// Offset and extra room after the vertex buffer data.
	vk::VkDeviceSize			vertexDataOffset;
	vk::VkDeviceSize			vertexDataExtraBytes;

	// Static and dynamic pipeline configuration.
	CullModeConfig				cullModeConfig;
	FrontFaceConfig				frontFaceConfig;
	TopologyConfig				topologyConfig;
	ViewportConfig				viewportConfig;
	ScissorConfig				scissorConfig;
	StrideConfig				strideConfig;
	DepthTestEnableConfig		depthTestEnableConfig;
	DepthWriteEnableConfig		depthWriteEnableConfig;
	DepthCompareOpConfig		depthCompareOpConfig;
	DepthBoundsTestEnableConfig	depthBoundsTestEnableConfig;
	StencilTestEnableConfig		stencilTestEnableConfig;
	StencilOpConfig				stencilOpConfig;

	// Sane defaults.
	TestConfig (SequenceOrdering ordering)
		: sequenceOrdering				(ordering)
		, meshParams					(1u, MeshParams())
		, referenceStencil				(0u)
		, clearColorValue				(kDefaultClearColor)
		, clearDepthValue				(1.0f)
		, clearStencilValue				(0u)
		, referenceColor				(SingleColorGenerator(kDefaultTriangleColor))
		, expectedDepth					(1.0f)
		, expectedStencil				(0u)
		, minDepthBounds				(0.0f)
		, maxDepthBounds				(1.0f)
		, forceGeometryShader			(false)
		, vertexDataOffset				(0ull)
		, vertexDataExtraBytes			(0ull)
		, cullModeConfig				(static_cast<vk::VkCullModeFlags>(vk::VK_CULL_MODE_NONE))
		, frontFaceConfig				(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE)
		// By default we will use a triangle fan with 6 vertices that could be wrongly interpreted as a triangle list with 2 triangles.
		, topologyConfig				(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
		, viewportConfig				(ViewportVec(1u, vk::makeViewport(kFramebufferWidth, kFramebufferHeight)))
		, scissorConfig					(ScissorVec(1u, vk::makeRect2D(kFramebufferWidth, kFramebufferHeight)))
		, strideConfig					(kVertexStride)
		, depthTestEnableConfig			(false)
		, depthWriteEnableConfig		(false)
		, depthCompareOpConfig			(vk::VK_COMPARE_OP_NEVER)
		, depthBoundsTestEnableConfig	(false)
		, stencilTestEnableConfig		(false)
		, stencilOpConfig				(StencilOpVec(1u, kDefaultStencilOpParams))
		, m_swappedValues				(false)
	{
	}

	// Get the proper viewport vector according to the test config.
	const ViewportVec& getActiveViewportVec () const
	{
		return ((viewportConfig.dynamicValue && !m_swappedValues) ? viewportConfig.dynamicValue.get() : viewportConfig.staticValue);
	}

	// Returns true if there is more than one viewport.
	bool isMultiViewport () const
	{
		return (getActiveViewportVec().size() > 1);
	}

	// Returns true if the case needs a geometry shader.
	bool needsGeometryShader () const
	{
		// Writing to gl_ViewportIndex from vertex or tesselation shaders needs the shaderOutputViewportIndex feature, which is less
		// commonly supported than geometry shaders, so we will use a geometry shader if we need to write to it.
		return (isMultiViewport() || forceGeometryShader);
	}

	// Returns true if we should use the static and dynamic values exchanged.
	// This makes the static part of the pipeline have the actual expected values.
	bool isReversed () const
	{
		return (sequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				sequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC);
	}

	// Swaps static and dynamic configuration values.
	void swapValues ()
	{
		cullModeConfig.swapValues();
		frontFaceConfig.swapValues();
		topologyConfig.swapValues();
		viewportConfig.swapValues();
		scissorConfig.swapValues();
		strideConfig.swapValues();
		depthTestEnableConfig.swapValues();
		depthWriteEnableConfig.swapValues();
		depthCompareOpConfig.swapValues();
		depthBoundsTestEnableConfig.swapValues();
		stencilTestEnableConfig.swapValues();
		stencilOpConfig.swapValues();

		m_swappedValues = !m_swappedValues;
	}

	// Returns the number of iterations when recording commands.
	deUint32 numIterations () const
	{
		deUint32 iterations = 0u;

		switch (sequenceOrdering)
		{
		case SequenceOrdering::TWO_DRAWS_DYNAMIC:
		case SequenceOrdering::TWO_DRAWS_STATIC:
			iterations = 2u;
			break;
		default:
			iterations = 1u;
			break;
		}

		return iterations;
	}

private:
	// Extended dynamic state cases as created by createExtendedDynamicStateTests() are based on the assumption that, when a state
	// has a static and a dynamic value configured at the same time, the static value is wrong and the dynamic value will give
	// expected results. That's appropriate for most test variants, but in some others we want to reverse the situation: a dynamic
	// pipeline with wrong values and a static one with good values.
	//
	// Instead of modifying how tests are created, we use isReversed() and swapValues() above, allowing us to swap static and
	// dynamic values and to know if we should do it for a given test case. However, we need to know were the good value is at any
	// given point in time in order to correctly answer some questions while running the test. m_swappedValues tracks that state.
	bool m_swappedValues;
};

struct PushConstants
{
	tcu::Vec4	triangleColor;
	float		meshDepth;
	deInt32		viewPortIndex;
	float		scaleX;
	float		scaleY;
	float		offsetX;
	float		offsetY;
};

void copy(vk::VkStencilOpState& dst, const StencilOpParams& src)
{
	dst.failOp		= src.failOp;
	dst.passOp		= src.passOp;
	dst.depthFailOp	= src.depthFailOp;
	dst.compareOp	= src.compareOp;
}

enum class TopologyClass
{
	POINT,
	LINE,
	TRIANGLE,
	PATCH,
	INVALID,
};

std::string topologyClassName (TopologyClass tclass)
{
	switch (tclass)
	{
	case TopologyClass::POINT:		return "point";
	case TopologyClass::LINE:		return "line";
	case TopologyClass::TRIANGLE:	return "triangle";
	case TopologyClass::PATCH:		return "patch";
	default:
		break;
	}

	DE_ASSERT(false);
	return "";
}

TopologyClass getTopologyClass (vk::VkPrimitiveTopology topology)
{
	switch (topology)
	{
	case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		return TopologyClass::POINT;
	case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
	case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
	case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
	case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		return TopologyClass::LINE;
	case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
	case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
	case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
	case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
	case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		return TopologyClass::TRIANGLE;
	case vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		return TopologyClass::PATCH;
	default:
		break;
	}

	DE_ASSERT(false);
	return TopologyClass::INVALID;
}

class ExtendedDynamicStateTest : public vkt::TestCase
{
public:
							ExtendedDynamicStateTest		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestConfig& testConfig);
	virtual					~ExtendedDynamicStateTest		(void) {}

	virtual void			checkSupport					(Context& context) const;
	virtual void			initPrograms					(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance					(Context& context) const;

private:
	TestConfig				m_testConfig;
};

class ExtendedDynamicStateInstance : public vkt::TestInstance
{
public:
								ExtendedDynamicStateInstance	(Context& context, const TestConfig& testConfig);
	virtual						~ExtendedDynamicStateInstance	(void) {}

	virtual tcu::TestStatus		iterate							(void);

private:
	TestConfig					m_testConfig;
};

ExtendedDynamicStateTest::ExtendedDynamicStateTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestConfig& testConfig)
	: vkt::TestCase	(testCtx, name, description)
	, m_testConfig	(testConfig)
{
	const auto staticTopologyClass = getTopologyClass(testConfig.topologyConfig.staticValue);
	DE_UNREF(staticTopologyClass); // For release builds.

	// Matching topology classes.
	DE_ASSERT(!testConfig.topologyConfig.dynamicValue ||
			  staticTopologyClass == getTopologyClass(testConfig.topologyConfig.dynamicValue.get()));

	// Supported topology classes for these tests.
	DE_ASSERT(staticTopologyClass == TopologyClass::LINE || staticTopologyClass == TopologyClass::TRIANGLE);
}

void ExtendedDynamicStateTest::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	// This is always required.
	context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

	// Check the number of viewports needed and the corresponding limits.
	const auto&	viewportConfig	= m_testConfig.viewportConfig;
	auto		numViews		= viewportConfig.staticValue.size();

	if (viewportConfig.dynamicValue)
		numViews = std::max(numViews, viewportConfig.dynamicValue.get().size());

	if (numViews > 1)
	{
		context.requireDeviceFunctionality("VK_KHR_multiview");
		const auto properties = vk::getPhysicalDeviceProperties(vki, physicalDevice);
		if (numViews > static_cast<decltype(numViews)>(properties.limits.maxViewports))
			TCU_THROW(NotSupportedError, "Number of viewports not supported (" + de::toString(numViews) + ")");
	}

	const auto&	dbTestEnable	= m_testConfig.depthBoundsTestEnableConfig;
	const bool	useDepthBounds	= (dbTestEnable.staticValue || (dbTestEnable.dynamicValue && dbTestEnable.dynamicValue.get()));
	if (useDepthBounds || m_testConfig.needsGeometryShader())
	{
		const auto features = vk::getPhysicalDeviceFeatures(vki, physicalDevice);

		// Check depth bounds test support.
		if (useDepthBounds && !features.depthBounds)
			TCU_THROW(NotSupportedError, "Depth bounds feature not supported");

		// Check geometry shader support.
		if (m_testConfig.needsGeometryShader() && !features.geometryShader)
			TCU_THROW(NotSupportedError, "Geometry shader not supported");
	}

	// Check color image format support (depth/stencil will be chosen at runtime).
	const vk::VkFormatFeatureFlags	kColorFeatures	= (vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
	const auto						colorProperties	= vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kColorFormat);

	if ((colorProperties.optimalTilingFeatures & kColorFeatures) != kColorFeatures)
		TCU_THROW(NotSupportedError, "Required color image features not supported");
}

void ExtendedDynamicStateTest::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream pushSource;
	std::ostringstream vertSource;
	std::ostringstream fragSource;
	std::ostringstream geomSource;

	pushSource
		<< "layout(push_constant, std430) uniform PushConstantsBlock {\n"
		<< "    vec4  triangleColor;\n"
		<< "    float depthValue;\n"
		<< "    int   viewPortIndex;\n"
		<< "    float scaleX;\n"
		<< "    float scaleY;\n"
		<< "    float offsetX;\n"
		<< "    float offsetY;\n"
		<< "} pushConstants;\n"
		;
	const auto pushConstants = pushSource.str();

	vertSource
		<< "#version 450\n"
		<< pushConstants
		<< "layout(location=0) in vec2 position;\n"
		<< "out gl_PerVertex\n"
		<< "{\n"
		<< "    vec4 gl_Position;\n"
		<< "};\n"
		<< "void main() {\n"
		<< "    gl_Position = vec4(position.x * pushConstants.scaleX + pushConstants.offsetX, position.y * pushConstants.scaleY + pushConstants.offsetY, pushConstants.depthValue, 1.0);\n"
		<< "}\n"
		;

	fragSource
		<< "#version 450\n"
		<< pushConstants
		<< "layout(location=0) out vec4 color;\n"
		<< "void main() {\n"
		<< "    color = pushConstants.triangleColor;\n"
		<< "}\n"
		;

	if (m_testConfig.needsGeometryShader())
	{
		const auto			topologyClass	= getTopologyClass(m_testConfig.topologyConfig.staticValue);
		const std::string	inputPrimitive	= ((topologyClass == TopologyClass::LINE) ? "lines" : "triangles");
		const deUint32		vertexCount		= ((topologyClass == TopologyClass::LINE) ? 2u : 3u);
		const std::string	outputPrimitive	= ((topologyClass == TopologyClass::LINE) ? "line_strip" : "triangle_strip");

		geomSource
			<< "#version 450\n"
			<< "layout (" << inputPrimitive << ") in;\n"
			<< "layout (" << outputPrimitive << ", max_vertices=" << vertexCount << ") out;\n"
			<< (m_testConfig.isMultiViewport() ? pushConstants : "")
			<< "in gl_PerVertex\n"
			<< "{\n"
			<< "    vec4 gl_Position;\n"
			<< "} gl_in[" << vertexCount << "];\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "void main() {\n"
			<< (m_testConfig.isMultiViewport() ? "    gl_ViewportIndex = pushConstants.viewPortIndex;\n" : "")
			;

		for (deUint32 i = 0; i < vertexCount; ++i)
		{
			geomSource
				<< "    gl_Position = gl_in[" << i << "].gl_Position;\n"
				<< "    EmitVertex();\n"
				;
		}

		geomSource
			<< "}\n"
			;
	}

	programCollection.glslSources.add("vert") << glu::VertexSource(vertSource.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragSource.str());
	if (m_testConfig.needsGeometryShader())
		programCollection.glslSources.add("geom") << glu::GeometrySource(geomSource.str());
}

TestInstance* ExtendedDynamicStateTest::createInstance (Context& context) const
{
	return new ExtendedDynamicStateInstance(context, m_testConfig);
}

ExtendedDynamicStateInstance::ExtendedDynamicStateInstance(Context& context, const TestConfig& testConfig)
	: vkt::TestInstance	(context)
	, m_testConfig		(testConfig)
{
}

void logErrors(tcu::TestLog& log, const std::string& setName, const std::string& setDesc, const tcu::ConstPixelBufferAccess& result, const tcu::ConstPixelBufferAccess& errorMask)
{
	log << tcu::TestLog::ImageSet(setName, setDesc)
		<< tcu::TestLog::Image(setName + "Result", "Result image", result)
		<< tcu::TestLog::Image(setName + "ErrorMask", "Error mask with errors marked in red", errorMask)
		<< tcu::TestLog::EndImageSet;
}

// Fill a section of the given buffer (from offset to offset+count) with repeating copies of the given data.
void fillWithPattern(vk::BufferWithMemory& buffer, size_t offset, size_t count, const void* src, size_t srcSize)
{
	auto&	alloc	= buffer.getAllocation();
	auto	ptr		= reinterpret_cast<char*>(alloc.getHostPtr());
	size_t	done	= 0u;
	size_t	pending	= count;

	while (pending > 0u)
	{
		const size_t stepSize = de::min(srcSize, pending);
		deMemcpy(ptr + offset + done, src, stepSize);
		done += stepSize;
		pending -= stepSize;
	}
}

void copyAndFlush(const vk::DeviceInterface& vkd, vk::VkDevice device, vk::BufferWithMemory& buffer, size_t offset, const void* src, size_t size)
{
	auto&	alloc	= buffer.getAllocation();
	auto	dst		= reinterpret_cast<char*>(alloc.getHostPtr());

	deMemcpy(dst + offset, src, size);
	vk::flushAlloc(vkd, device, alloc);
}

// Sets values for dynamic states if needed according to the test configuration.
void setDynamicStates(const TestConfig& testConfig, const vk::DeviceInterface& vkd, vk::VkCommandBuffer cmdBuffer)
{
	if (testConfig.cullModeConfig.dynamicValue)
		vkd.cmdSetCullModeEXT(cmdBuffer, testConfig.cullModeConfig.dynamicValue.get());

	if (testConfig.frontFaceConfig.dynamicValue)
		vkd.cmdSetFrontFaceEXT(cmdBuffer, testConfig.frontFaceConfig.dynamicValue.get());

	if (testConfig.topologyConfig.dynamicValue)
		vkd.cmdSetPrimitiveTopologyEXT(cmdBuffer, testConfig.topologyConfig.dynamicValue.get());

	if (testConfig.viewportConfig.dynamicValue)
	{
		const auto& viewports = testConfig.viewportConfig.dynamicValue.get();
		vkd.cmdSetViewportWithCountEXT(cmdBuffer, static_cast<deUint32>(viewports.size()), viewports.data());
	}

	if (testConfig.scissorConfig.dynamicValue)
	{
		const auto& scissors = testConfig.scissorConfig.dynamicValue.get();
		vkd.cmdSetScissorWithCountEXT(cmdBuffer, static_cast<deUint32>(scissors.size()), scissors.data());
	}

	if (testConfig.depthTestEnableConfig.dynamicValue)
		vkd.cmdSetDepthTestEnableEXT(cmdBuffer, makeVkBool32(testConfig.depthTestEnableConfig.dynamicValue.get()));

	if (testConfig.depthWriteEnableConfig.dynamicValue)
		vkd.cmdSetDepthWriteEnableEXT(cmdBuffer, makeVkBool32(testConfig.depthWriteEnableConfig.dynamicValue.get()));

	if (testConfig.depthCompareOpConfig.dynamicValue)
		vkd.cmdSetDepthCompareOpEXT(cmdBuffer, testConfig.depthCompareOpConfig.dynamicValue.get());

	if (testConfig.depthBoundsTestEnableConfig.dynamicValue)
		vkd.cmdSetDepthBoundsTestEnableEXT(cmdBuffer, makeVkBool32(testConfig.depthBoundsTestEnableConfig.dynamicValue.get()));

	if (testConfig.stencilTestEnableConfig.dynamicValue)
		vkd.cmdSetStencilTestEnableEXT(cmdBuffer, makeVkBool32(testConfig.stencilTestEnableConfig.dynamicValue.get()));

	if (testConfig.stencilOpConfig.dynamicValue)
	{
		for (const auto& params : testConfig.stencilOpConfig.dynamicValue.get())
			vkd.cmdSetStencilOpEXT(cmdBuffer, params.faceMask, params.failOp, params.passOp, params.depthFailOp, params.compareOp);
	}
}

// Bind the appropriate vertex buffer with a dynamic stride if the test configuration needs a dynamic stride.
// Return true if the vertex buffer was bound.
bool maybeBindVertexBufferDynStride(const TestConfig& testConfig, const vk::DeviceInterface& vkd, vk::VkCommandBuffer cmdBuffer, size_t meshIdx, vk::VkBuffer vertBuffer, vk::VkBuffer rvertBuffer, vk::VkDeviceSize vertDataSize)
{
	if (testConfig.strideConfig.dynamicValue)
	{
		const auto& viewportVec = testConfig.getActiveViewportVec();
		DE_UNREF(viewportVec); // For release builds.

		// When dynamically setting the vertex buffer stride, we cannot bind the vertex buffer in advance for some sequence
		// orderings if we have several viewports or meshes.
		DE_ASSERT((viewportVec.size() == 1u && testConfig.meshParams.size() == 1u)
					|| testConfig.sequenceOrdering == SequenceOrdering::BEFORE_DRAW
					|| testConfig.sequenceOrdering == SequenceOrdering::AFTER_PIPELINES);

		const auto strideValue = (testConfig.strideConfig.dynamicValue ? testConfig.strideConfig.dynamicValue.get() : testConfig.strideConfig.staticValue);
		vkd.cmdBindVertexBuffers2EXT(cmdBuffer, 0u, 1u, (testConfig.meshParams[meshIdx].reversed ? &rvertBuffer : &vertBuffer), &testConfig.vertexDataOffset, &vertDataSize, &strideValue);
		return true;
	}

	return false;
}

tcu::TestStatus ExtendedDynamicStateInstance::iterate (void)
{
	using ImageWithMemoryVec	= std::vector<std::unique_ptr<vk::ImageWithMemory>>;
	using ImageViewVec			= std::vector<vk::Move<vk::VkImageView>>;
	using FramebufferVec		= std::vector<vk::Move<vk::VkFramebuffer>>;

	const auto&	vki					= m_context.getInstanceInterface();
	const auto&	vkd					= m_context.getDeviceInterface();
	const auto	physicalDevice		= m_context.getPhysicalDevice();
	const auto	device				= m_context.getDevice();
	auto&		allocator			= m_context.getDefaultAllocator();
	const auto	queue				= m_context.getUniversalQueue();
	const auto	queueIndex			= m_context.getUniversalQueueFamilyIndex();
	auto&		log					= m_context.getTestContext().getLog();

	const auto	kReversed			= m_testConfig.isReversed();
	const auto	kNumIterations		= m_testConfig.numIterations();
	const auto	kSequenceOrdering	= m_testConfig.sequenceOrdering;

	const auto						kFramebufferExtent	= vk::makeExtent3D(kFramebufferWidth, kFramebufferHeight, 1u);
	const vk::VkImageUsageFlags		kColorUsage			= (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const vk::VkImageUsageFlags		kDSUsage			= (vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const vk::VkFormatFeatureFlags	kDSFeatures			= (vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);

	// Choose depth/stencil format.
	const DepthStencilFormat* dsFormatInfo = nullptr;

	for (int formatIdx = 0; formatIdx < DE_LENGTH_OF_ARRAY(kDepthStencilFormats); ++formatIdx)
	{
		const auto dsProperties = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kDepthStencilFormats[formatIdx].imageFormat);
		if ((dsProperties.optimalTilingFeatures & kDSFeatures) == kDSFeatures)
		{
			dsFormatInfo = kDepthStencilFormats + formatIdx;
			break;
		}
	}

	// Note: Not Supported insted of Fail because the transfer feature is not mandatory.
	if (!dsFormatInfo)
		TCU_THROW(NotSupportedError, "Required depth/stencil image features not supported");
	log << tcu::TestLog::Message << "Chosen depth/stencil format: " << dsFormatInfo->imageFormat << tcu::TestLog::EndMessage;

	// Swap static and dynamic values in the test configuration so the static pipeline ends up with the expected values for cases
	// where we will bind the static pipeline last before drawing.
	if (kReversed)
		m_testConfig.swapValues();

	// Create color and depth/stencil images.
	ImageWithMemoryVec colorImages;
	ImageWithMemoryVec dsImages;

	const vk::VkImageCreateInfo colorImageInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		kColorFormat,								//	VkFormat				format;
		kFramebufferExtent,							//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		kColorUsage,								//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		1u,											//	deUint32				queueFamilyIndexCount;
		&queueIndex,								//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	for (deUint32 i = 0u; i < kNumIterations; ++i)
		colorImages.emplace_back(new vk::ImageWithMemory(vkd, device, allocator, colorImageInfo, vk::MemoryRequirement::Any));

	const vk::VkImageCreateInfo dsImageInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		dsFormatInfo->imageFormat,					//	VkFormat				format;
		kFramebufferExtent,							//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		kDSUsage,									//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		1u,											//	deUint32				queueFamilyIndexCount;
		&queueIndex,								//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	for (deUint32 i = 0u; i < kNumIterations; ++i)
		dsImages.emplace_back(new vk::ImageWithMemory(vkd, device, allocator, dsImageInfo, vk::MemoryRequirement::Any));

	const auto colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto dsSubresourceRange		= vk::makeImageSubresourceRange((vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);

	ImageViewVec colorImageViews;
	ImageViewVec dsImageViews;

	for (const auto& img : colorImages)
		colorImageViews.emplace_back(vk::makeImageView(vkd, device, img->get(), vk::VK_IMAGE_VIEW_TYPE_2D, kColorFormat, colorSubresourceRange));

	for (const auto& img : dsImages)
		dsImageViews.emplace_back(vk::makeImageView(vkd, device, img->get(), vk::VK_IMAGE_VIEW_TYPE_2D, dsFormatInfo->imageFormat, dsSubresourceRange));

	// Vertex buffer.
	const auto					topologyClass = getTopologyClass(m_testConfig.topologyConfig.staticValue);
	std::vector<GeometryVertex>	vertices;

	if (topologyClass == TopologyClass::TRIANGLE)
	{
		// Full-screen triangle fan with 6 vertices.
		//
		// 4        3        2
		//  +-------+-------+
		//  |X      X      X|
		//  | X     X     X |
		//  |  X    X    X  |
		//  |   X   X   X   |
		//  |    X  X  X    |
		//  |     X X X     |
		//  |      XXX      |
		//  +-------+-------+
		// 5        0        1
		vertices.push_back(GeometryVertex(tcu::Vec2( 0.0f,  1.0f)));
		vertices.push_back(GeometryVertex(tcu::Vec2( 1.0f,  1.0f)));
		vertices.push_back(GeometryVertex(tcu::Vec2( 1.0f, -1.0f)));
		vertices.push_back(GeometryVertex(tcu::Vec2( 0.0f, -1.0f)));
		vertices.push_back(GeometryVertex(tcu::Vec2(-1.0f, -1.0f)));
		vertices.push_back(GeometryVertex(tcu::Vec2(-1.0f,  1.0f)));
	}
	else // TopologyClass::LINE
	{
		// Draw one segmented line per output row of pixels that could be wrongly interpreted as a list of lines that would not cover the whole screen.
		const float lineHeight = 2.0f / static_cast<float>(kFramebufferHeight);
		for (deUint32 rowIdx = 0; rowIdx < kFramebufferHeight; ++rowIdx)
		{
			// Offset of 0.5 pixels + one line per row from -1 to 1.
			const float yCoord = (lineHeight / 2.0f) + lineHeight * static_cast<float>(rowIdx) - 1.0f;
			vertices.push_back(GeometryVertex(tcu::Vec2(-1.0f, yCoord)));
			vertices.push_back(GeometryVertex(tcu::Vec2(-0.5f, yCoord)));
			vertices.push_back(GeometryVertex(tcu::Vec2( 0.5f, yCoord)));
			vertices.push_back(GeometryVertex(tcu::Vec2( 1.0f, yCoord)));
		}
	}

	// Reversed vertices, except for the first one (0, 5, 4, 3, 2, 1): clockwise mesh for triangles. Not to be used with lines.
	std::vector<GeometryVertex> reversedVertices(1u, vertices[0]);
	std::copy_n(vertices.rbegin(), vertices.size() - 1u, std::back_inserter(reversedVertices));

	if (topologyClass == TopologyClass::LINE)
	{
		for (const auto& mesh : m_testConfig.meshParams)
		{
			DE_UNREF(mesh); // For release builds.
			DE_ASSERT(!mesh.reversed);
		}
	}

	const auto vertDataSize				= static_cast<vk::VkDeviceSize>(vertices.size() * sizeof(decltype(vertices)::value_type));
	const auto vertBufferSize			= m_testConfig.vertexDataOffset + vertDataSize + m_testConfig.vertexDataExtraBytes;
	const auto vertBufferInfo			= vk::makeBufferCreateInfo(vertBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	vk::BufferWithMemory vertBuffer		(vkd, device, allocator, vertBufferInfo, vk::MemoryRequirement::HostVisible);
	vk::BufferWithMemory rvertBuffer	(vkd, device, allocator, vertBufferInfo, vk::MemoryRequirement::HostVisible);

	// Copy data to vertex buffers and flush allocations.
	{
		const GeometryVertex	offScreenVertex		(tcu::Vec2(0.0f, 3.0f));
		const auto				offScreenVertexSz	= sizeof(offScreenVertex);
		const auto				dataSize			= static_cast<size_t>(vertDataSize);
		const auto				offset				= static_cast<size_t>(m_testConfig.vertexDataOffset);
		const auto				extraSize			= static_cast<size_t>(m_testConfig.vertexDataExtraBytes);

		std::vector<vk::BufferWithMemory*> buffersToFill = { &vertBuffer, &rvertBuffer };
		for (auto b : buffersToFill)
		{
			// Fill bytes surrounding vertex data with the offScreenVertex.
			fillWithPattern(*b, 0u, offset, &offScreenVertex, offScreenVertexSz);
			fillWithPattern(*b, offset + dataSize, extraSize, &offScreenVertex, offScreenVertexSz);
		}

		copyAndFlush(vkd, device, vertBuffer, offset, vertices.data(), dataSize);
		copyAndFlush(vkd, device, rvertBuffer, offset, reversedVertices.data(), dataSize);
	}

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Pipeline layout.
	vk::VkShaderStageFlags pushConstantStageFlags = (vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT);
	if (m_testConfig.isMultiViewport())
		pushConstantStageFlags |= vk::VK_SHADER_STAGE_GEOMETRY_BIT;

	const vk::VkPushConstantRange pushConstantRange =
	{
		pushConstantStageFlags,							//	VkShaderStageFlags	stageFlags;
		0u,												//	deUint32			offset;
		static_cast<deUint32>(sizeof(PushConstants)),	//	deUint32			size;
	};

	const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,											//	const void*						pNext;
		0u,													//	VkPipelineLayoutCreateFlags		flags;
		1u,													//	deUint32						setLayoutCount;
		&descriptorSetLayout.get(),							//	const VkDescriptorSetLayout*	pSetLayouts;
		1u,													//	deUint32						pushConstantRangeCount;
		&pushConstantRange,									//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const auto pipelineLayout = vk::createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);

	// Render pass with single subpass.
	const vk::VkAttachmentReference colorAttachmentReference =
	{
		0u,												//	deUint32		attachment;
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout	layout;
	};

	const vk::VkAttachmentReference dsAttachmentReference =
	{
		1u,														//	deUint32		attachment;
		vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	//	VkImageLayout	layout;
	};

	const vk::VkSubpassDescription subpassDescription =
	{
		0u,										//	VkSubpassDescriptionFlags		flags;
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,	//	VkPipelineBindPoint				pipelineBindPoint;
		0u,										//	deUint32						inputAttachmentCount;
		nullptr,								//	const VkAttachmentReference*	pInputAttachments;
		1u,										//	deUint32						colorAttachmentCount;
		&colorAttachmentReference,				//	const VkAttachmentReference*	pColorAttachments;
		nullptr,								//	const VkAttachmentReference*	pResolveAttachments;
		&dsAttachmentReference,					//	const VkAttachmentReference*	pDepthStencilAttachment;
		0u,										//	deUint32						preserveAttachmentCount;
		nullptr,								//	const deUint32*					pPreserveAttachments;
	};

	std::vector<vk::VkAttachmentDescription> attachmentDescriptions;

	attachmentDescriptions.push_back(vk::VkAttachmentDescription
	{
		0u,												//	VkAttachmentDescriptionFlags	flags;
		kColorFormat,									//	VkFormat						format;
		vk::VK_SAMPLE_COUNT_1_BIT,						//	VkSampleCountFlagBits			samples;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,				//	VkAttachmentLoadOp				loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,				//	VkAttachmentStoreOp				storeOp;
		vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
		vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
	});

	attachmentDescriptions.push_back(vk::VkAttachmentDescription
	{
		0u,														//	VkAttachmentDescriptionFlags	flags;
		dsFormatInfo->imageFormat,								//	VkFormat						format;
		vk::VK_SAMPLE_COUNT_1_BIT,								//	VkSampleCountFlagBits			samples;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						//	VkAttachmentLoadOp				loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						//	VkAttachmentStoreOp				storeOp;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						//	VkAttachmentLoadOp				stencilLoadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						//	VkAttachmentStoreOp				stencilStoreOp;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,							//	VkImageLayout					initialLayout;
		vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
	});

	const vk::VkRenderPassCreateInfo renderPassCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			//	VkStructureType					sType;
		nullptr,												//	const void*						pNext;
		0u,														//	VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachmentDescriptions.size()),	//	deUint32						attachmentCount;
		attachmentDescriptions.data(),							//	const VkAttachmentDescription*	pAttachments;
		1u,														//	deUint32						subpassCount;
		&subpassDescription,									//	const VkSubpassDescription*		pSubpasses;
		0u,														//	deUint32						dependencyCount;
		nullptr,												//	const VkSubpassDependency*		pDependencies;
	};
	const auto renderPass = vk::createRenderPass(vkd, device, &renderPassCreateInfo);

	// Framebuffers.
	FramebufferVec framebuffers;

	DE_ASSERT(colorImageViews.size() == dsImageViews.size());
	for (size_t imgIdx = 0; imgIdx < colorImageViews.size(); ++imgIdx)
	{
		std::vector<vk::VkImageView> attachments;
		attachments.push_back(colorImageViews[imgIdx].get());
		attachments.push_back(dsImageViews[imgIdx].get());

		const vk::VkFramebufferCreateInfo framebufferCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	//	VkStructureType				sType;
			nullptr,										//	const void*					pNext;
			0u,												//	VkFramebufferCreateFlags	flags;
			renderPass.get(),								//	VkRenderPass				renderPass;
			static_cast<deUint32>(attachments.size()),		//	deUint32					attachmentCount;
			attachments.data(),								//	const VkImageView*			pAttachments;
			kFramebufferWidth,								//	deUint32					width;
			kFramebufferHeight,								//	deUint32					height;
			1u,												//	deUint32					layers;
		};

		framebuffers.emplace_back(vk::createFramebuffer(vkd, device, &framebufferCreateInfo));
	}

	// Shader modules.
	const auto						vertModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto						fragModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);
	vk::Move<vk::VkShaderModule>	geomModule;

	if (m_testConfig.needsGeometryShader())
		geomModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("geom"), 0u);

	// Shader stages.
	std::vector<vk::VkPipelineShaderStageCreateInfo> shaderStages;

	vk::VkPipelineShaderStageCreateInfo shaderStageCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineShaderStageCreateFlags	flags;
		vk::VK_SHADER_STAGE_VERTEX_BIT,								//	VkShaderStageFlagBits				stage;
		vertModule.get(),											//	VkShaderModule						module;
		"main",														//	const char*							pName;
		nullptr,													//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	shaderStages.push_back(shaderStageCreateInfo);
	shaderStageCreateInfo.stage = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageCreateInfo.module = fragModule.get();
	shaderStages.push_back(shaderStageCreateInfo);

	if (m_testConfig.needsGeometryShader())
	{
		shaderStageCreateInfo.stage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		shaderStageCreateInfo.module = geomModule.get();
		shaderStages.push_back(shaderStageCreateInfo);
	}

	// Input state.
	const auto vertexBinding	= vk::makeVertexInputBindingDescription(0u, static_cast<deUint32>(m_testConfig.strideConfig.staticValue), vk::VK_VERTEX_INPUT_RATE_VERTEX);
	const auto vertexAttribute	= vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u);

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		1u,																//	deUint32									vertexBindingDescriptionCount;
		&vertexBinding,													//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		1u,																//	deUint32									vertexAttributeDescriptionCount;
		&vertexAttribute,												//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	// Input assembly.
	const vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineInputAssemblyStateCreateFlags	flags;
		m_testConfig.topologyConfig.staticValue,							//	VkPrimitiveTopology						topology;
		VK_FALSE,															//	VkBool32								primitiveRestartEnable;
	};

	// Viewport state.
	if (m_testConfig.viewportConfig.dynamicValue)
		DE_ASSERT(m_testConfig.viewportConfig.dynamicValue.get().size() > 0u);
	else
		DE_ASSERT(m_testConfig.viewportConfig.staticValue.size() > 0u);

	if (m_testConfig.scissorConfig.dynamicValue)
		DE_ASSERT(m_testConfig.scissorConfig.dynamicValue.get().size() > 0u);
	else
		DE_ASSERT(m_testConfig.scissorConfig.staticValue.size() > 0u);

	// The viewport and scissor counts must match in the static part, which will be used by the static pipeline.
	const auto minStaticCount = static_cast<deUint32>(std::min(m_testConfig.viewportConfig.staticValue.size(), m_testConfig.scissorConfig.staticValue.size()));

	// For the static pipeline.
	const vk::VkPipelineViewportStateCreateInfo staticViewportStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,					//	VkStructureType						sType;
		nullptr,																	//	const void*							pNext;
		0u,																			//	VkPipelineViewportStateCreateFlags	flags;
		minStaticCount,																//	deUint32							viewportCount;
		m_testConfig.viewportConfig.staticValue.data(),								//	const VkViewport*					pViewports;
		minStaticCount,																//	deUint32							scissorCount;
		m_testConfig.scissorConfig.staticValue.data(),								//	const VkRect2D*						pScissors;
	};

	// For the dynamic pipeline.
	const auto finalDynamicViewportCount = (m_testConfig.viewportConfig.dynamicValue
		? m_testConfig.viewportConfig.dynamicValue.get().size()
		: m_testConfig.viewportConfig.staticValue.size());

	const auto finalDynamicScissorCount = (m_testConfig.scissorConfig.dynamicValue
		? m_testConfig.scissorConfig.dynamicValue.get().size()
		: m_testConfig.scissorConfig.staticValue.size());

	const auto minDynamicCount = static_cast<deUint32>(std::min(finalDynamicScissorCount, finalDynamicViewportCount));

	// The viewport and scissor counts must be zero when a dynamic value will be provided, as per the spec.
	const vk::VkPipelineViewportStateCreateInfo dynamicViewportStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,					//	VkStructureType						sType;
		nullptr,																	//	const void*							pNext;
		0u,																			//	VkPipelineViewportStateCreateFlags	flags;
		(m_testConfig.viewportConfig.dynamicValue ? 0u : minDynamicCount),			//	deUint32							viewportCount;
		m_testConfig.viewportConfig.staticValue.data(),								//	const VkViewport*					pViewports;
		(m_testConfig.scissorConfig.dynamicValue ? 0u : minDynamicCount),			//	deUint32							scissorCount;
		m_testConfig.scissorConfig.staticValue.data(),								//	const VkRect2D*						pScissors;
	};

	// Rasterization state.
	const vk::VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														//	VkBool32								depthClampEnable;
		VK_FALSE,														//	VkBool32								rasterizerDiscardEnable;
		vk::VK_POLYGON_MODE_FILL,										//	VkPolygonMode							polygonMode;
		m_testConfig.cullModeConfig.staticValue,						//	VkCullModeFlags							cullMode;
		m_testConfig.frontFaceConfig.staticValue,						//	VkFrontFace								frontFace;
		VK_FALSE,														//	VkBool32								depthBiasEnable;
		0.0f,															//	float									depthBiasConstantFactor;
		0.0f,															//	float									depthBiasClamp;
		0.0f,															//	float									depthBiasSlopeFactor;
		1.0f,															//	float									lineWidth;
	};

	// Multisample state.
	const vk::VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineMultisampleStateCreateFlags	flags;
		vk::VK_SAMPLE_COUNT_1_BIT,										//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,														//	VkBool32								sampleShadingEnable;
		0.0f,															//	float									minSampleShading;
		nullptr,														//	const VkSampleMask*						pSampleMask;
		VK_FALSE,														//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,														//	VkBool32								alphaToOneEnable;
	};

	// Depth/stencil state.
	vk::VkStencilOpState	staticFrontStencil;
	vk::VkStencilOpState	staticBackStencil;
	bool					staticFrontStencilSet	= false;
	bool					staticBackStencilSet	= false;

	// Common setup for the front and back operations.
	staticFrontStencil.compareMask	= 0xFFu;
	staticFrontStencil.writeMask	= 0xFFu;
	staticFrontStencil.reference	= m_testConfig.referenceStencil;
	staticBackStencil				= staticFrontStencil;

	for (const auto& op : m_testConfig.stencilOpConfig.staticValue)
	{
		if ((op.faceMask & vk::VK_STENCIL_FACE_FRONT_BIT) != 0u)
		{
			copy(staticFrontStencil, op);
			staticFrontStencilSet = true;
		}
		if ((op.faceMask & vk::VK_STENCIL_FACE_BACK_BIT) != 0u)
		{
			copy(staticBackStencil, op);
			staticBackStencilSet = true;
		}
	}

	// Default values for the static part.
	if (!staticFrontStencilSet)
		copy(staticFrontStencil, kDefaultStencilOpParams);
	if (!staticBackStencilSet)
		copy(staticBackStencil, kDefaultStencilOpParams);

	const vk::VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineDepthStencilStateCreateFlags	flags;
		makeVkBool32(m_testConfig.depthTestEnableConfig.staticValue),		//	VkBool32								depthTestEnable;
		makeVkBool32(m_testConfig.depthWriteEnableConfig.staticValue),		//	VkBool32								depthWriteEnable;
		m_testConfig.depthCompareOpConfig.staticValue,						//	VkCompareOp								depthCompareOp;
		makeVkBool32(m_testConfig.depthBoundsTestEnableConfig.staticValue),	//	VkBool32								depthBoundsTestEnable;
		makeVkBool32(m_testConfig.stencilTestEnableConfig.staticValue),		//	VkBool32								stencilTestEnable;
		staticFrontStencil,													//	VkStencilOpState						front;
		staticBackStencil,													//	VkStencilOpState						back;
		m_testConfig.minDepthBounds,										//	float									minDepthBounds;
		m_testConfig.maxDepthBounds,										//	float									maxDepthBounds;
	};

	// Dynamic state. Here we will set all states which have a dynamic value.
	std::vector<vk::VkDynamicState> dynamicStates;

	if (m_testConfig.cullModeConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_CULL_MODE_EXT);
	if (m_testConfig.frontFaceConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_FRONT_FACE_EXT);
	if (m_testConfig.topologyConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT);
	if (m_testConfig.viewportConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT);
	if (m_testConfig.scissorConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT);
	if (m_testConfig.strideConfig.dynamicValue)					dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT);
	if (m_testConfig.depthTestEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT);
	if (m_testConfig.depthWriteEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT);
	if (m_testConfig.depthCompareOpConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT);
	if (m_testConfig.depthBoundsTestEnableConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT);
	if (m_testConfig.stencilTestEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT);
	if (m_testConfig.stencilOpConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_OP_EXT);

	const vk::VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(dynamicStates.size()),				//	deUint32							dynamicStateCount;
		dynamicStates.data(),										//	const VkDynamicState*				pDynamicStates;
	};

	const vk::VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE,						// VkBool32                 blendEnable
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcColorBlendFactor
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstColorBlendFactor
		vk::VK_BLEND_OP_ADD,			// VkBlendOp                colorBlendOp
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcAlphaBlendFactor
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstAlphaBlendFactor
		vk::VK_BLEND_OP_ADD,			// VkBlendOp                alphaBlendOp
		vk::VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags    colorWriteMask
		| vk::VK_COLOR_COMPONENT_G_BIT
		| vk::VK_COLOR_COMPONENT_B_BIT
		| vk::VK_COLOR_COMPONENT_A_BIT
	};

	const vk::VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                               sType
		nullptr,														// const void*                                   pNext
		0u,																// VkPipelineColorBlendStateCreateFlags          flags
		VK_FALSE,														// VkBool32                                      logicOpEnable
		vk::VK_LOGIC_OP_CLEAR,											// VkLogicOp                                     logicOp
		1u,																// deUint32                                      attachmentCount
		&colorBlendAttachmentState,										// const VkPipelineColorBlendAttachmentState*    pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }										// float                                         blendConstants[4]
	};

	const vk::VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfoTemplate =
	{
		vk::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	//	VkStructureType									sType;
		nullptr,												//	const void*										pNext;
		0u,														//	VkPipelineCreateFlags							flags;
		static_cast<deUint32>(shaderStages.size()),				//	deUint32										stageCount;
		shaderStages.data(),									//	const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateCreateInfo,							//	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyStateCreateInfo,							//	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		nullptr,												//	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
		nullptr,												//	const VkPipelineViewportStateCreateInfo*		pViewportState;
		&rasterizationStateCreateInfo,							//	const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&multisampleStateCreateInfo,							//	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&depthStencilStateCreateInfo,							//	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
		&colorBlendStateCreateInfo,								//	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		nullptr,												//	const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout.get(),									//	VkPipelineLayout								layout;
		renderPass.get(),										//	VkRenderPass									renderPass;
		0u,														//	deUint32										subpass;
		DE_NULL,												//	VkPipeline										basePipelineHandle;
		0,														//	deInt32											basePipelineIndex;
	};

	vk::Move<vk::VkPipeline>	staticPipeline;
	const bool					bindStaticFirst		= (kSequenceOrdering == SequenceOrdering::BETWEEN_PIPELINES	||
													   kSequenceOrdering == SequenceOrdering::AFTER_PIPELINES	||
													   kSequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC);
	const bool					useStaticPipeline	= (bindStaticFirst || kReversed);

	// Create static pipeline when needed.
	if (useStaticPipeline)
	{
		auto staticPipelineCreateInfo			= graphicsPipelineCreateInfoTemplate;
		staticPipelineCreateInfo.pViewportState	= &staticViewportStateCreateInfo;
		staticPipeline							= vk::createGraphicsPipeline(vkd, device, DE_NULL, &staticPipelineCreateInfo);
	}

	// Create dynamic pipeline.
	vk::Move<vk::VkPipeline> graphicsPipeline;
	{
		auto dynamicPipelineCreateInfo				= graphicsPipelineCreateInfoTemplate;
		dynamicPipelineCreateInfo.pDynamicState		= &dynamicStateCreateInfo;
		dynamicPipelineCreateInfo.pViewportState	= &dynamicViewportStateCreateInfo;
		graphicsPipeline							= vk::createGraphicsPipeline(vkd, device, DE_NULL, &dynamicPipelineCreateInfo);
	}

	// Command buffer.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd , device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Clear values.
	std::vector<vk::VkClearValue> clearValues;
	clearValues.push_back(vk::makeClearValueColor(m_testConfig.clearColorValue));
	clearValues.push_back(vk::makeClearValueDepthStencil(m_testConfig.clearDepthValue, m_testConfig.clearStencilValue));

	// Record command buffer.
	vk::beginCommandBuffer(vkd, cmdBuffer);

	for (deUint32 iteration = 0u; iteration < kNumIterations; ++iteration)
	{
		// Track in-advance vertex buffer binding.
		bool boundInAdvance = false;

		// Maybe set extended dynamic state here.
		if (kSequenceOrdering == SequenceOrdering::CMD_BUFFER_START)
		{
			setDynamicStates(m_testConfig, vkd, cmdBuffer);
			boundInAdvance = maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, 0u, vertBuffer.get(), rvertBuffer.get(), vertDataSize);
		}

		// Begin render pass.
		vk::beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffers[iteration].get(), vk::makeRect2D(kFramebufferWidth, kFramebufferHeight), static_cast<deUint32>(clearValues.size()), clearValues.data());

			// Bind a static pipeline first if needed.
			if (bindStaticFirst && iteration == 0u)
				vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipeline.get());

			// Maybe set extended dynamic state here.
			if (kSequenceOrdering == SequenceOrdering::BETWEEN_PIPELINES)
			{
				setDynamicStates(m_testConfig, vkd, cmdBuffer);
				boundInAdvance = maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, 0u, vertBuffer.get(), rvertBuffer.get(), vertDataSize);
			}

			// Bind dynamic pipeline.
			if ((kSequenceOrdering != SequenceOrdering::TWO_DRAWS_DYNAMIC &&
				 kSequenceOrdering != SequenceOrdering::TWO_DRAWS_STATIC) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC && iteration > 0u) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration == 0u))
			{
				vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
			}

			if (kSequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC && iteration > 0u) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration == 0u))
			{
				setDynamicStates(m_testConfig, vkd, cmdBuffer);
				boundInAdvance = maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, 0u, vertBuffer.get(), rvertBuffer.get(), vertDataSize);
			}

			// Bind a static pipeline last if needed.
			if (kSequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration > 0u))
			{
				vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipeline.get());
			}

			const auto& viewportVec = m_testConfig.getActiveViewportVec();
			for (size_t viewportIdx = 0u; viewportIdx < viewportVec.size(); ++viewportIdx)
			{
				for (size_t meshIdx = 0u; meshIdx < m_testConfig.meshParams.size(); ++meshIdx)
				{
					// Push constants.
					PushConstants pushConstants =
					{
						m_testConfig.meshParams[meshIdx].color,		//	tcu::Vec4	triangleColor;
						m_testConfig.meshParams[meshIdx].depth,		//	float		meshDepth;
						static_cast<deInt32>(viewportIdx),			//	deInt32		viewPortIndex;
						m_testConfig.meshParams[meshIdx].scaleX,	//	float		scaleX;
						m_testConfig.meshParams[meshIdx].scaleY,	//	float		scaleY;
						m_testConfig.meshParams[meshIdx].offsetX,	//	float		offsetX;
						m_testConfig.meshParams[meshIdx].offsetY,	//	float		offsetY;
					};
					vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pushConstantStageFlags, 0u, static_cast<deUint32>(sizeof(pushConstants)), &pushConstants);

					// Track vertex bounding state for this mesh.
					bool boundBeforeDraw = false;

					// Maybe set extended dynamic state here.
					if (kSequenceOrdering == SequenceOrdering::BEFORE_DRAW || kSequenceOrdering == SequenceOrdering::AFTER_PIPELINES)
					{
						setDynamicStates(m_testConfig, vkd, cmdBuffer);
						boundBeforeDraw = maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, meshIdx, vertBuffer.get(), rvertBuffer.get(), vertDataSize);
					}

					// Bind vertex buffer with static stride if needed and draw.
					if (!(boundInAdvance || boundBeforeDraw))
						vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, (m_testConfig.meshParams[meshIdx].reversed ? &rvertBuffer.get() : &vertBuffer.get()), &m_testConfig.vertexDataOffset);

					// Draw mesh.
					vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(vertices.size()), 1u, 0u, 0u);
				}
			}

		vk::endRenderPass(vkd, cmdBuffer);
	}

	vk::endCommandBuffer(vkd, cmdBuffer);

	// Submit commands.
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read result image aspects from the last used framebuffer.
	const tcu::UVec2	renderSize		(kFramebufferWidth, kFramebufferHeight);
	const auto			colorBuffer		= readColorAttachment(vkd, device, queue, queueIndex, allocator, colorImages.back()->get(), kColorFormat, renderSize);
	const auto			depthBuffer		= readDepthAttachment(vkd, device, queue, queueIndex, allocator, dsImages.back()->get(), dsFormatInfo->imageFormat, renderSize);
	const auto			stencilBuffer	= readStencilAttachment(vkd, device, queue, queueIndex, allocator, dsImages.back()->get(), dsFormatInfo->imageFormat, renderSize, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	const auto			colorAccess		= colorBuffer->getAccess();
	const auto			depthAccess		= depthBuffer->getAccess();
	const auto			stencilAccess	= stencilBuffer->getAccess();

	const int kWidth	= static_cast<int>(kFramebufferWidth);
	const int kHeight	= static_cast<int>(kFramebufferHeight);

	// Generate reference color buffer.
	const auto				tcuColorFormat			= vk::mapVkFormat(kColorFormat);
	tcu::TextureLevel		referenceColorLevel		(tcuColorFormat, kWidth, kHeight);
	tcu::PixelBufferAccess	referenceColorAccess	= referenceColorLevel.getAccess();
	m_testConfig.referenceColor(referenceColorAccess);

	const tcu::TextureFormat	errorFormat			(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
	tcu::TextureLevel			colorError			(errorFormat, kWidth, kHeight);
	tcu::TextureLevel			depthError			(errorFormat, kWidth, kHeight);
	tcu::TextureLevel			stencilError		(errorFormat, kWidth, kHeight);
	const auto					colorErrorAccess	= colorError.getAccess();
	const auto					depthErrorAccess	= depthError.getAccess();
	const auto					stencilErrorAccess	= stencilError.getAccess();
	const tcu::Vec4				kGood				(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4				kBad				(1.0f, 0.0f, 0.0f, 1.0f);

	// Check expected values.
	const auto	minDepth		= m_testConfig.expectedDepth - dsFormatInfo->depthThreshold;
	const auto	maxDepth		= m_testConfig.expectedDepth + dsFormatInfo->depthThreshold;
	bool		colorMatch		= true;
	bool		depthMatch		= true;
	bool		stencilMatch	= true;
	bool		match;

	for (int y = 0; y < kHeight; ++y)
	for (int x = 0; x < kWidth; ++x)
	{
		const auto colorPixel		= colorAccess.getPixel(x, y);
		const auto expectedPixel	= referenceColorAccess.getPixel(x, y);

		match = tcu::boolAll(tcu::lessThan(tcu::absDiff(colorPixel, expectedPixel), kColorThreshold));
		colorErrorAccess.setPixel((match ? kGood : kBad), x, y);
		if (!match)
			colorMatch = false;

		const auto depthPixel = depthAccess.getPixDepth(x, y);
		match = de::inRange(depthPixel, minDepth, maxDepth);
		depthErrorAccess.setPixel((match ? kGood : kBad), x, y);
		if (!match)
			depthMatch = false;

		const auto stencilPixel = static_cast<deUint32>(stencilAccess.getPixStencil(x, y));
		match = (stencilPixel == m_testConfig.expectedStencil);
		stencilErrorAccess.setPixel((match ? kGood : kBad), x, y);
		if (!match)
			stencilMatch = false;
	}

	if (!(colorMatch && depthMatch && stencilMatch))
	{
		if (!colorMatch)
			logErrors(log, "Color", "Result color image and error mask", colorAccess, colorErrorAccess);

		if (!depthMatch)
			logErrors(log, "Depth", "Result depth image and error mask", depthAccess, depthErrorAccess);

		if (!stencilMatch)
			logErrors(log, "Stencil", "Result stencil image and error mask", stencilAccess, stencilErrorAccess);

		return tcu::TestStatus::fail("Incorrect value found in attachments; please check logged images");
	}

	return tcu::TestStatus::pass("Pass");
}

bool stencilPasses(vk::VkCompareOp op, deUint8 storedValue, deUint8 referenceValue)
{
	switch (op)
	{
	case vk::VK_COMPARE_OP_NEVER:				return false;
	case vk::VK_COMPARE_OP_LESS:				return (referenceValue <	storedValue);
	case vk::VK_COMPARE_OP_EQUAL:				return (referenceValue ==	storedValue);
	case vk::VK_COMPARE_OP_LESS_OR_EQUAL:		return (referenceValue <=	storedValue);
	case vk::VK_COMPARE_OP_GREATER:				return (referenceValue >	storedValue);
	case vk::VK_COMPARE_OP_GREATER_OR_EQUAL:	return (referenceValue >=	storedValue);
	case vk::VK_COMPARE_OP_ALWAYS:				return true;
	default: DE_ASSERT(false); return false;
	}

	return false;	// Unreachable.
}

deUint8 stencilResult(vk::VkStencilOp op, deUint8 storedValue, deUint8 referenceValue, deUint8 min, deUint8 max)
{
	deUint8 result = storedValue;

	switch (op)
	{
	case vk::VK_STENCIL_OP_KEEP:					break;
	case vk::VK_STENCIL_OP_ZERO:					result = 0; break;
	case vk::VK_STENCIL_OP_REPLACE:					result = referenceValue; break;
	case vk::VK_STENCIL_OP_INCREMENT_AND_CLAMP:		result = ((result == max) ? result : static_cast<deUint8>(result + 1)); break;
	case vk::VK_STENCIL_OP_DECREMENT_AND_CLAMP:		result = ((result == min) ? result : static_cast<deUint8>(result - 1)); break;
	case vk::VK_STENCIL_OP_INVERT:					result = static_cast<deUint8>(~result); break;
	case vk::VK_STENCIL_OP_INCREMENT_AND_WRAP:		result = ((result == max) ? min : static_cast<deUint8>(result + 1)); break;
	case vk::VK_STENCIL_OP_DECREMENT_AND_WRAP:		result = ((result == min) ? max : static_cast<deUint8>(result - 1)); break;
	default: DE_ASSERT(false); break;
	}

	return result;
}

} // anonymous namespace

tcu::TestCaseGroup* createExtendedDynamicStateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> extendedDynamicStateGroup(new tcu::TestCaseGroup(testCtx, "extended_dynamic_state", "Tests for VK_EXT_extended_dynamic_state"));

	// Auxiliar constants.
	const deUint32	kHalfWidthU	= kFramebufferWidth/2u;
	const deInt32	kHalfWidthI	= static_cast<deInt32>(kHalfWidthU);
	const float		kHalfWidthF	= static_cast<float>(kHalfWidthU);
	const float		kHeightF	= static_cast<float>(kFramebufferHeight);

	static const struct
	{
		SequenceOrdering	ordering;
		std::string			name;
		std::string			desc;
	} kOrderingCases[] =
	{
		{ SequenceOrdering::CMD_BUFFER_START,	"cmd_buffer_start",		"Dynamic state set after command buffer start"																								},
		{ SequenceOrdering::BEFORE_DRAW,		"before_draw",			"Dynamic state set just before drawing"																										},
		{ SequenceOrdering::BETWEEN_PIPELINES,	"between_pipelines",	"Dynamic after a pipeline with static states has been bound and before a pipeline with dynamic states has been bound"						},
		{ SequenceOrdering::AFTER_PIPELINES,	"after_pipelines",		"Dynamic state set after both a static-state pipeline and a second dynamic-state pipeline have been bound"									},
		{ SequenceOrdering::BEFORE_GOOD_STATIC,	"before_good_static",	"Dynamic state set after a dynamic pipeline has been bound and before a second static-state pipeline with the right values has been bound"	},
		{ SequenceOrdering::TWO_DRAWS_DYNAMIC,	"two_draws_dynamic",	"Bind bad static pipeline and draw, followed by binding correct dynamic pipeline and drawing again"											},
		{ SequenceOrdering::TWO_DRAWS_STATIC,	"two_draws_static",		"Bind bad dynamic pipeline and draw, followed by binding correct static pipeline and drawing again"											},
	};

	for (int orderingIdx = 0; orderingIdx < DE_LENGTH_OF_ARRAY(kOrderingCases); ++orderingIdx)
	{
		const auto& kOrderingCase	= kOrderingCases[orderingIdx];
		const auto& kOrdering		= kOrderingCase.ordering;

		de::MovePtr<tcu::TestCaseGroup> orderingGroup(new tcu::TestCaseGroup(testCtx, kOrderingCase.name.c_str(), kOrderingCase.desc.c_str()));

		// Cull modes.
		{
			TestConfig config(kOrdering);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_FRONT_BIT;
			config.cullModeConfig.dynamicValue	= tcu::just<vk::VkCullModeFlags>(vk::VK_CULL_MODE_NONE);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "cull_none", "Dynamically set cull mode to none", config));
		}
		{
			TestConfig config(kOrdering);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_FRONT_AND_BACK;
			config.cullModeConfig.dynamicValue	= tcu::just<vk::VkCullModeFlags>(vk::VK_CULL_MODE_BACK_BIT);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "cull_back", "Dynamically set cull mode to back", config));
		}
		{
			TestConfig config(kOrdering);
			// Make triangles look back.
			config.meshParams[0].reversed		= true;
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.cullModeConfig.dynamicValue	= tcu::just<vk::VkCullModeFlags>(vk::VK_CULL_MODE_FRONT_BIT);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "cull_front", "Dynamically set cull mode to front", config));
		}
		{
			TestConfig config(kOrdering);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_NONE;
			config.cullModeConfig.dynamicValue	= tcu::just<vk::VkCullModeFlags>(vk::VK_CULL_MODE_FRONT_AND_BACK);
			config.referenceColor				= SingleColorGenerator(kDefaultClearColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "cull_front_and_back", "Dynamically set cull mode to front and back", config));
		}

		// Front face.
		{
			TestConfig config(kOrdering);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.frontFaceConfig.staticValue	= vk::VK_FRONT_FACE_CLOCKWISE;
			config.frontFaceConfig.dynamicValue	= tcu::just<vk::VkFrontFace>(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "front_face_cw", "Dynamically set front face to clockwise", config));
		}
		{
			TestConfig config(kOrdering);
			// Pass triangles in clockwise order.
			config.meshParams[0].reversed		= true;
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.frontFaceConfig.staticValue	= vk::VK_FRONT_FACE_COUNTER_CLOCKWISE;
			config.frontFaceConfig.dynamicValue	= tcu::just<vk::VkFrontFace>(vk::VK_FRONT_FACE_CLOCKWISE);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "front_face_ccw", "Dynamically set front face to counter-clockwise", config));
		}
		{
			TestConfig config(kOrdering);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.frontFaceConfig.staticValue	= vk::VK_FRONT_FACE_COUNTER_CLOCKWISE;
			config.frontFaceConfig.dynamicValue	= tcu::just<vk::VkFrontFace>(vk::VK_FRONT_FACE_CLOCKWISE);
			config.referenceColor				= SingleColorGenerator(kDefaultClearColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "front_face_cw_reversed", "Dynamically set front face to clockwise with a counter-clockwise mesh", config));
		}
		{
			TestConfig config(kOrdering);
			// Pass triangles in clockwise order.
			config.meshParams[0].reversed		= true;
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.frontFaceConfig.staticValue	= vk::VK_FRONT_FACE_CLOCKWISE;
			config.frontFaceConfig.dynamicValue	= tcu::just<vk::VkFrontFace>(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE);
			config.referenceColor				= SingleColorGenerator(kDefaultClearColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "front_face_ccw_reversed", "Dynamically set front face to counter-clockwise with a clockwise mesh", config));
		}

		// Dynamic topology.
		{
			TestConfig baseConfig(kOrdering);

			for (int i = 0; i < 2; ++i)
			{
				const bool forceGeometryShader = (i > 0);

				static const struct
				{
					vk::VkPrimitiveTopology staticVal;
					vk::VkPrimitiveTopology dynamicVal;
				} kTopologyCases[] =
				{
					{ vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN	},
					{ vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP	},
				};

				for (int topoCaseIdx = 0; topoCaseIdx < DE_LENGTH_OF_ARRAY(kTopologyCases); ++topoCaseIdx)
				{
					TestConfig config(baseConfig);
					config.forceGeometryShader			= forceGeometryShader;
					config.topologyConfig.staticValue	= kTopologyCases[topoCaseIdx].staticVal;
					config.topologyConfig.dynamicValue	= tcu::just<vk::VkPrimitiveTopology>(kTopologyCases[topoCaseIdx].dynamicVal);

					const std::string	className	= topologyClassName(getTopologyClass(config.topologyConfig.staticValue));
					const std::string	name		= "topology_" + className + (forceGeometryShader ? "_geom" : "");
					const std::string	desc		= "Dynamically switch primitive topologies from the " + className + " class" + (forceGeometryShader ? " and use a geometry shader" : "");
					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, name, desc, config));
				}
			}
		}

		// Viewport.
		{
			TestConfig config(kOrdering);
			// 2 scissors, bad static single viewport.
			config.scissorConfig.staticValue	= ScissorVec{vk::makeRect2D(0, 0, kHalfWidthU, kFramebufferHeight), vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight)};
			config.viewportConfig.staticValue	= ViewportVec(1u, vk::makeViewport(kHalfWidthU, kFramebufferHeight));
			config.viewportConfig.dynamicValue	= ViewportVec{
				vk::makeViewport(0.0f, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),
				vk::makeViewport(kHalfWidthF, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),
			};
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "2_viewports", "Dynamically set 2 viewports", config));
		}
		{
			TestConfig config(kOrdering);
			// Bad static reduced viewport.
			config.viewportConfig.staticValue	= ViewportVec(1u, vk::makeViewport(kHalfWidthU, kFramebufferHeight));
			config.viewportConfig.staticValue	= ViewportVec(1u, vk::makeViewport(kFramebufferWidth, kFramebufferHeight));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "1_full_viewport", "Dynamically set viewport to cover full framebuffer", config));
		}
		{
			TestConfig config(kOrdering);
			// 2 scissors (left half, right half), 2 reversed static viewports that need fixing (right, left).
			config.scissorConfig.staticValue	= ScissorVec{vk::makeRect2D(0, 0, kHalfWidthU, kFramebufferHeight), vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight)};
			config.viewportConfig.staticValue	= ViewportVec{
				vk::makeViewport(kHalfWidthF, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),	// Right.
				vk::makeViewport(0.0f, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),		// Left.
			};
			config.viewportConfig.dynamicValue	= ViewportVec{config.viewportConfig.staticValue.back(), config.viewportConfig.staticValue.front()};
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "2_viewports_switch", "Dynamically switch the order with 2 viewports", config));
		}
		{
			TestConfig config(kOrdering);
			// 2 scissors, reversed dynamic viewports that should result in no drawing taking place.
			config.scissorConfig.staticValue	= ScissorVec{vk::makeRect2D(0, 0, kHalfWidthU, kFramebufferHeight), vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight)};
			config.viewportConfig.staticValue	= ViewportVec{
				vk::makeViewport(0.0f, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),		// Left.
				vk::makeViewport(kHalfWidthF, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),	// Right.
			};
			config.viewportConfig.dynamicValue	= ViewportVec{config.viewportConfig.staticValue.back(), config.viewportConfig.staticValue.front()};
			config.referenceColor				= SingleColorGenerator(kDefaultClearColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "2_viewports_switch_clean", "Dynamically switch the order with 2 viewports resulting in clean image", config));
		}

		// Scissor.
		{
			TestConfig config(kOrdering);
			// 2 viewports, bad static single scissor.
			config.viewportConfig.staticValue	= ViewportVec{
				vk::makeViewport(0.0f, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),
				vk::makeViewport(kHalfWidthF, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),
			};
			config.scissorConfig.staticValue	= ScissorVec(1u, vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight));
			config.scissorConfig.dynamicValue	= ScissorVec{
				vk::makeRect2D(kHalfWidthU, kFramebufferHeight),
				vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight),
			};
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "2_scissors", "Dynamically set 2 scissors", config));
		}
		{
			TestConfig config(kOrdering);
			// 1 viewport, bad static single scissor.
			config.scissorConfig.staticValue	= ScissorVec(1u, vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight));
			config.scissorConfig.dynamicValue	= ScissorVec(1u, vk::makeRect2D(kFramebufferWidth, kFramebufferHeight));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "1_full_scissor", "Dynamically set scissor to cover full framebuffer", config));
		}
		{
			TestConfig config(kOrdering);
			// 2 viewports, 2 reversed scissors that need fixing.
			config.viewportConfig.staticValue	= ViewportVec{
				vk::makeViewport(0.0f, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),
				vk::makeViewport(kHalfWidthF, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),
			};
			config.scissorConfig.staticValue	= ScissorVec{
				vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight),
				vk::makeRect2D(kHalfWidthU, kFramebufferHeight),
			};
			config.scissorConfig.dynamicValue	= ScissorVec{config.scissorConfig.staticValue.back(), config.scissorConfig.staticValue.front()};
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "2_scissors_switch", "Dynamically switch the order with 2 scissors", config));
		}
		{
			TestConfig config(kOrdering);
			// 2 viewports, 2 scissors switched to prevent drawing.
			config.viewportConfig.staticValue	= ViewportVec{
				vk::makeViewport(0.0f, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),
				vk::makeViewport(kHalfWidthF, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),
			};
			config.scissorConfig.staticValue	= ScissorVec{
				vk::makeRect2D(kHalfWidthU, kFramebufferHeight),
				vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight),
			};
			config.scissorConfig.dynamicValue	= ScissorVec{config.scissorConfig.staticValue.back(), config.scissorConfig.staticValue.front()};
			config.referenceColor				= SingleColorGenerator(kDefaultClearColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "2_scissors_switch_clean", "Dynamically switch the order with 2 scissors to avoid drawing", config));
		}

		// Stride.
		{
			TestConfig config(kOrdering);
			config.strideConfig.staticValue		= kCoordsSize;
			config.strideConfig.dynamicValue	= kVertexStride;
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "stride", "Dynamically set stride", config));
		}
		{
			TestConfig config(kOrdering);
			config.strideConfig.staticValue		= kCoordsSize;
			config.strideConfig.dynamicValue	= kVertexStride;
			config.vertexDataOffset				= static_cast<vk::VkDeviceSize>(sizeof(GeometryVertex));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "stride_with_offset", "Dynamically set stride using a nonzero vertex data offset", config));
		}
		{
			TestConfig config(kOrdering);
			config.strideConfig.staticValue		= kCoordsSize;
			config.strideConfig.dynamicValue	= kVertexStride;
			config.vertexDataOffset				= static_cast<vk::VkDeviceSize>(sizeof(GeometryVertex));
			config.vertexDataExtraBytes			= config.vertexDataOffset;

			// Make the mesh cover the top half only. If the implementation reads data outside the vertex data it should read the
			// offscreen vertex and draw something in the bottom half.
			config.referenceColor				= HorizontalSplitGenerator(kDefaultTriangleColor, kDefaultClearColor);
			config.meshParams[0].scaleY			= 0.5f;
			config.meshParams[0].offsetY		= -0.5f;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "stride_with_offset_and_padding", "Dynamically set stride using a nonzero vertex data offset and extra bytes", config));
		}

		// Depth test enable.
		{
			TestConfig config(kOrdering);
			config.depthTestEnableConfig.staticValue	= false;
			config.depthTestEnableConfig.dynamicValue	= tcu::just(true);
			// By default, the depth test never passes when enabled.
			config.referenceColor						= SingleColorGenerator(kDefaultClearColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_test_enable", "Dynamically enable depth test", config));
		}
		{
			TestConfig config(kOrdering);
			config.depthTestEnableConfig.staticValue	= true;
			config.depthTestEnableConfig.dynamicValue	= tcu::just(false);
			config.referenceColor						= SingleColorGenerator(kDefaultTriangleColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_test_disable", "Dynamically disable depth test", config));
		}

		// Depth write enable.
		{
			TestConfig config(kOrdering);

			// Enable depth test and set values so it passes.
			config.depthTestEnableConfig.staticValue	= true;
			config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_LESS;
			config.clearDepthValue						= 0.5f;
			config.meshParams[0].depth					= 0.25f;

			// Enable writes and expect the mesh value.
			config.depthWriteEnableConfig.staticValue	= false;
			config.depthWriteEnableConfig.dynamicValue	= tcu::just(true);
			config.expectedDepth						= 0.25f;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_write_enable", "Dynamically enable writes to the depth buffer", config));
		}
		{
			TestConfig config(kOrdering);

			// Enable depth test and set values so it passes.
			config.depthTestEnableConfig.staticValue	= true;
			config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_LESS;
			config.clearDepthValue						= 0.5f;
			config.meshParams[0].depth					= 0.25f;

			// But disable writing dynamically and expect the clear value.
			config.depthWriteEnableConfig.staticValue	= true;
			config.depthWriteEnableConfig.dynamicValue	= tcu::just(false);
			config.expectedDepth						= 0.5f;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_write_disable", "Dynamically disable writes to the depth buffer", config));
		}

		// Depth compare op.
		{
			TestConfig baseConfig(kOrdering);
			const tcu::Vec4 kAlternativeColor				(0.0f, 0.0f, 0.5f, 1.0f);
			baseConfig.depthTestEnableConfig.staticValue	= true;
			baseConfig.depthWriteEnableConfig.staticValue	= true;
			baseConfig.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_NEVER;
			baseConfig.clearDepthValue						= 0.5f;

			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_ALWAYS;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_NEVER;
				config.meshParams[0].depth					= 0.25f;
				config.expectedDepth						= 0.5f;
				config.referenceColor						= SingleColorGenerator(kDefaultClearColor);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_never", "Dynamically set the depth compare operator to NEVER", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_LESS;
				config.meshParams[0].depth					= 0.25f;
				config.expectedDepth						= 0.25f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_less", "Dynamically set the depth compare operator to LESS", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_GREATER;
				config.meshParams[0].depth					= 0.75f;
				config.expectedDepth						= 0.75f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_greater", "Dynamically set the depth compare operator to GREATER", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_EQUAL;
				config.meshParams[0].depth					= 0.5f;
				config.meshParams[0].color					= kAlternativeColor;
				// Draw another mesh in front to verify it does not pass the equality test.
				config.meshParams.push_back(MeshParams(kDefaultTriangleColor, 0.25f));
				config.expectedDepth						= 0.5f;
				config.referenceColor						= SingleColorGenerator(kAlternativeColor);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_equal", "Dynamically set the depth compare operator to EQUAL", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_LESS_OR_EQUAL;
				config.meshParams[0].depth					= 0.25f;
				config.expectedDepth						= 0.25f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_less_equal_less", "Dynamically set the depth compare operator to LESS_OR_EQUAL and draw with smaller depth", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_LESS_OR_EQUAL;
				config.meshParams[0].depth					= 0.5f;
				config.expectedDepth						= 0.5f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_less_equal_equal", "Dynamically set the depth compare operator to LESS_OR_EQUAL and draw with equal depth", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_LESS_OR_EQUAL;
				config.meshParams[0].depth					= 0.25f;
				// Draw another mesh with the same depth in front of it.
				config.meshParams.push_back(MeshParams(kAlternativeColor, 0.25f));
				config.expectedDepth						= 0.25f;
				config.referenceColor						= SingleColorGenerator(kAlternativeColor);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_less_equal_less_then_equal", "Dynamically set the depth compare operator to LESS_OR_EQUAL and draw two meshes with less and equal depth", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_GREATER_OR_EQUAL;
				config.meshParams[0].depth					= 0.75f;
				config.expectedDepth						= 0.75f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_greater_equal_greater", "Dynamically set the depth compare operator to GREATER_OR_EQUAL and draw with greater depth", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_GREATER_OR_EQUAL;
				config.meshParams[0].depth					= 0.5f;
				config.expectedDepth						= 0.5f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_greater_equal_equal", "Dynamically set the depth compare operator to GREATER_OR_EQUAL and draw with equal depth", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_GREATER_OR_EQUAL;
				config.meshParams[0].depth					= 0.75f;
				// Draw another mesh with the same depth in front of it.
				config.meshParams.push_back(MeshParams(kAlternativeColor, 0.75f));
				config.expectedDepth						= 0.75f;
				config.referenceColor						= SingleColorGenerator(kAlternativeColor);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_greater_equal_greater_then_equal", "Dynamically set the depth compare operator to GREATER_OR_EQUAL and draw two meshes with greater and equal depth", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_NOT_EQUAL;

				// Draw first mesh in front.
				config.meshParams[0].depth					= 0.25f;
				// Draw another mesh in the back, this should pass too.
				config.meshParams.push_back(MeshParams(kAlternativeColor, 0.5f));
				// Finally a new mesh with the same depth. This should not pass.
				config.meshParams.push_back(MeshParams(kDefaultTriangleColor, 0.5f));

				config.referenceColor						= SingleColorGenerator(kAlternativeColor);
				config.expectedDepth						= 0.5f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_not_equal", "Dynamically set the depth compare operator to NOT_EQUAL", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthCompareOpConfig.dynamicValue	= vk::VK_COMPARE_OP_ALWAYS;

				config.meshParams[0].depth					= 0.5f;
				config.expectedDepth						= 0.5f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_always_equal", "Dynamically set the depth compare operator to ALWAYS and draw with equal depth", config));

				config.meshParams[0].depth					= 0.25f;
				config.expectedDepth						= 0.25f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_always_less", "Dynamically set the depth compare operator to ALWAYS and draw with less depth", config));

				config.meshParams[0].depth					= 0.75f;
				config.expectedDepth						= 0.75f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_compare_always_greater", "Dynamically set the depth compare operator to ALWAYS and draw with greater depth", config));
			}
		}

		// Depth bounds test.
		{
			TestConfig baseConfig(kOrdering);
			baseConfig.minDepthBounds							= 0.25f;
			baseConfig.maxDepthBounds							= 0.75f;
			baseConfig.meshParams[0].depth						= 0.0f;

			{
				TestConfig config = baseConfig;
				config.depthBoundsTestEnableConfig.staticValue	= false;
				config.depthBoundsTestEnableConfig.dynamicValue	= tcu::just(true);
				config.referenceColor							= SingleColorGenerator(kDefaultClearColor);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_bounds_test_enable", "Dynamically enable the depth bounds test", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthBoundsTestEnableConfig.staticValue	= true;
				config.depthBoundsTestEnableConfig.dynamicValue	= tcu::just(false);
				config.referenceColor							= SingleColorGenerator(kDefaultTriangleColor);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_bounds_test_disable", "Dynamically disable the depth bounds test", config));
			}
		}

		// Stencil test enable.
		{
			TestConfig config(kOrdering);
			config.stencilTestEnableConfig.staticValue				= false;
			config.stencilTestEnableConfig.dynamicValue				= tcu::just(true);
			config.stencilOpConfig.staticValue.front().compareOp	= vk::VK_COMPARE_OP_NEVER;
			config.referenceColor									= SingleColorGenerator(kDefaultClearColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "stencil_test_enable", "Dynamically enable the stencil test", config));
		}
		{
			TestConfig config(kOrdering);
			config.stencilTestEnableConfig.staticValue				= true;
			config.stencilTestEnableConfig.dynamicValue				= tcu::just(false);
			config.stencilOpConfig.staticValue.front().compareOp	= vk::VK_COMPARE_OP_NEVER;
			config.referenceColor									= SingleColorGenerator(kDefaultTriangleColor);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "stencil_test_disable", "Dynamically disable the stencil test", config));
		}

		// Stencil operation. Many combinations are possible.
		{
			static const struct
			{
				vk::VkStencilFaceFlags	face;
				std::string				name;
			} kFaces[] =
			{
				{ vk::VK_STENCIL_FACE_FRONT_BIT,			"face_front"		},
				{ vk::VK_STENCIL_FACE_BACK_BIT,				"face_back"			},
				{ vk::VK_STENCIL_FRONT_AND_BACK,			"face_both_single"	},
				{ vk::VK_STENCIL_FACE_FLAG_BITS_MAX_ENUM,	"face_both_dual"	},	// MAX_ENUM is a placeholder.
			};

			static const struct
			{
				vk::VkCompareOp		compareOp;
				std::string			name;
			} kCompare[] =
			{
				{ vk::VK_COMPARE_OP_NEVER,				"xf"		},
				{ vk::VK_COMPARE_OP_LESS,				"lt"		},
				{ vk::VK_COMPARE_OP_EQUAL,				"eq"		},
				{ vk::VK_COMPARE_OP_LESS_OR_EQUAL,		"le"		},
				{ vk::VK_COMPARE_OP_GREATER,			"gt"		},
				{ vk::VK_COMPARE_OP_GREATER_OR_EQUAL,	"ge"		},
				{ vk::VK_COMPARE_OP_ALWAYS,				"xt"		},
			};

			using u8vec = std::vector<deUint8>;

			static const auto kMinVal	= std::numeric_limits<deUint8>::min();
			static const auto kMaxVal	= std::numeric_limits<deUint8>::max();
			static const auto kMidVal	= static_cast<deUint8>(kMaxVal * 2u / 5u);
			static const auto kMinValI	= static_cast<int>(kMinVal);
			static const auto kMaxValI	= static_cast<int>(kMaxVal);

			static const struct
			{
				vk::VkStencilOp		stencilOp;
				std::string			name;
				u8vec				clearValues;	// One test per clear value interesting for this operation.
				vk::VkStencilOp		incompatibleOp;	// Alternative operation giving incompatible results for the given values.
			} kStencilOps[] =
			{
				{ vk::VK_STENCIL_OP_KEEP,					"keep",			u8vec{kMidVal},					vk::VK_STENCIL_OP_ZERO					},
				{ vk::VK_STENCIL_OP_ZERO,					"zero",			u8vec{kMidVal},					vk::VK_STENCIL_OP_KEEP					},
				{ vk::VK_STENCIL_OP_REPLACE,				"replace",		u8vec{kMidVal},					vk::VK_STENCIL_OP_ZERO					},
				{ vk::VK_STENCIL_OP_INCREMENT_AND_CLAMP,	"inc_clamp",	u8vec{kMaxVal - 1, kMaxVal},	vk::VK_STENCIL_OP_ZERO					},
				{ vk::VK_STENCIL_OP_DECREMENT_AND_CLAMP,	"dec_clamp",	u8vec{kMinVal + 1, kMinVal},	vk::VK_STENCIL_OP_INCREMENT_AND_CLAMP	},
				{ vk::VK_STENCIL_OP_INVERT,					"invert",		u8vec{kMidVal},					vk::VK_STENCIL_OP_ZERO					},
				{ vk::VK_STENCIL_OP_INCREMENT_AND_WRAP,		"inc_wrap",		u8vec{kMaxVal - 1, kMaxVal},	vk::VK_STENCIL_OP_KEEP					},
				{ vk::VK_STENCIL_OP_DECREMENT_AND_WRAP,		"dec_wrap",		u8vec{kMinVal + 1, kMinVal},	vk::VK_STENCIL_OP_KEEP					},
			};

			for (int facesIdx	= 0; facesIdx	< DE_LENGTH_OF_ARRAY(kFaces);		++facesIdx)
			for (int compareIdx	= 0; compareIdx	< DE_LENGTH_OF_ARRAY(kCompare);		++compareIdx)
			for (int opIdx		= 0; opIdx		< DE_LENGTH_OF_ARRAY(kStencilOps);	++opIdx)
			{
				const auto& face	= kFaces[facesIdx];
				const auto& compare	= kCompare[compareIdx];
				const auto& op		= kStencilOps[opIdx];

				// Try clearing the stencil value with different values.
				for (const auto clearVal : op.clearValues)
				{
					// Use interesting values as the reference stencil value.
					for (int delta = -1; delta <= 1; ++delta)
					{
						const int refVal = clearVal + delta;
						if (refVal < kMinValI || refVal > kMaxValI)
							continue;

						const auto refValU8		= static_cast<deUint8>(refVal);
						const auto refValU32	= static_cast<deUint32>(refVal);

						// Calculate outcome of the stencil test itself.
						const bool wouldPass = stencilPasses(compare.compareOp, clearVal, refValU8);

						// If the test passes, use an additional variant for the depthFail operation.
						const int subCases = (wouldPass ? 2 : 1);

						for (int subCaseIdx = 0; subCaseIdx < subCases; ++subCaseIdx)
						{
							const bool depthFail	= (subCaseIdx > 0);				// depthFail would be the second variant.
							const bool globalPass	= (wouldPass && !depthFail);	// Global result of the stencil+depth test.

							// Start tuning test parameters.
							TestConfig config(kOrdering);

							// No face culling is applied by default, so both the front and back operations could apply depending on the mesh.
							if (face.face == vk::VK_STENCIL_FACE_FRONT_BIT)
							{
								// Default parameters are OK.
							}
							else if (face.face == vk::VK_STENCIL_FACE_BACK_BIT)
							{
								// Reverse the mesh so it applies the back operation.
								config.meshParams[0].reversed = true;
							}
							else	// Front and back.
							{
								// Draw both a front and a back-facing mesh so both are applied.
								// The first mesh will be drawn in the top half and the second mesh in the bottom half.

								// Make the second mesh a reversed copy of the first mesh.
								config.meshParams.push_back(config.meshParams.front());
								config.meshParams.back().reversed = true;

								// Apply scale and offset to the top mesh.
								config.meshParams.front().scaleY = 0.5f;
								config.meshParams.front().offsetY = -0.5f;

								// Apply scale and offset to the bottom mesh.
								config.meshParams.back().scaleY = 0.5f;
								config.meshParams.back().offsetY = 0.5f;
							}

							// Enable the stencil test.
							config.stencilTestEnableConfig.staticValue = true;

							// Set dynamic configuration.
							StencilOpParams dynamicStencilConfig;
							dynamicStencilConfig.faceMask		= face.face;
							dynamicStencilConfig.compareOp		= compare.compareOp;
							dynamicStencilConfig.failOp			= vk::VK_STENCIL_OP_MAX_ENUM;
							dynamicStencilConfig.passOp			= vk::VK_STENCIL_OP_MAX_ENUM;
							dynamicStencilConfig.depthFailOp	= vk::VK_STENCIL_OP_MAX_ENUM;

							// Set operations so only the appropriate operation for this case gives the right result.
							vk::VkStencilOp* activeOp		= nullptr;
							vk::VkStencilOp* inactiveOps[2]	= { nullptr, nullptr };
							if (wouldPass)
							{
								if (depthFail)
								{
									activeOp		= &dynamicStencilConfig.depthFailOp;
									inactiveOps[0]	= &dynamicStencilConfig.passOp;
									inactiveOps[1]	= &dynamicStencilConfig.failOp;
								}
								else
								{
									activeOp		= &dynamicStencilConfig.passOp;
									inactiveOps[0]	= &dynamicStencilConfig.depthFailOp;
									inactiveOps[1]	= &dynamicStencilConfig.failOp;
								}
							}
							else
							{
								activeOp		= &dynamicStencilConfig.failOp;
								inactiveOps[0]	= &dynamicStencilConfig.passOp;
								inactiveOps[1]	= &dynamicStencilConfig.depthFailOp;
							}

							*activeOp = op.stencilOp;
							*inactiveOps[0] = op.incompatibleOp;
							*inactiveOps[1] = op.incompatibleOp;

							// Make sure all ops have been configured properly.
							DE_ASSERT(dynamicStencilConfig.failOp != vk::VK_STENCIL_OP_MAX_ENUM);
							DE_ASSERT(dynamicStencilConfig.passOp != vk::VK_STENCIL_OP_MAX_ENUM);
							DE_ASSERT(dynamicStencilConfig.depthFailOp != vk::VK_STENCIL_OP_MAX_ENUM);

							// Set an incompatible static operation too.
							auto& staticStencilConfig		= config.stencilOpConfig.staticValue.front();
							staticStencilConfig.faceMask	= face.face;
							staticStencilConfig.compareOp	= (globalPass ? vk::VK_COMPARE_OP_NEVER : vk::VK_COMPARE_OP_ALWAYS);
							staticStencilConfig.passOp		= op.incompatibleOp;
							staticStencilConfig.failOp		= op.incompatibleOp;
							staticStencilConfig.depthFailOp	= op.incompatibleOp;

							// Set dynamic configuration.
							StencilOpVec stencilOps;
							stencilOps.push_back(dynamicStencilConfig);

							if (stencilOps.front().faceMask == vk::VK_STENCIL_FACE_FLAG_BITS_MAX_ENUM)
							{
								// This is the dual case. We will set the front and back face values with two separate calls.
								stencilOps.push_back(stencilOps.front());
								stencilOps.front().faceMask		= vk::VK_STENCIL_FACE_FRONT_BIT;
								stencilOps.back().faceMask		= vk::VK_STENCIL_FACE_BACK_BIT;
								staticStencilConfig.faceMask	= vk::VK_STENCIL_FACE_FRONT_AND_BACK;
							}

							config.stencilOpConfig.dynamicValue	= tcu::just(stencilOps);
							config.clearStencilValue			= clearVal;
							config.referenceStencil				= refValU32;

							if (depthFail)
							{
								// Enable depth test and make it fail.
								config.depthTestEnableConfig.staticValue	= true;
								config.clearDepthValue						= 0.5f;
								config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_LESS;

								for (auto& meshPar : config.meshParams)
									meshPar.depth = 0.75f;
							}

							// Set expected outcome.
							config.referenceColor	= SingleColorGenerator(globalPass ? kDefaultTriangleColor : kDefaultClearColor);
							config.expectedDepth	= config.clearDepthValue; // No depth writing by default.
							config.expectedStencil	= stencilResult(op.stencilOp, clearVal, refValU8, kMinVal, kMaxVal);

							const std::string testName = std::string("stencil_state")
								+ "_" + face.name
								+ "_" + compare.name
								+ "_" + op.name
								+ "_clear_" + de::toString(static_cast<int>(clearVal))
								+ "_ref_" + de::toString(refVal)
								+ "_" + (wouldPass ? (depthFail ? "depthfail" : "pass") : "fail");

							orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, "Dynamically configure stencil test, variant " + testName, config));
						}
					}
				}
			}
		}

		extendedDynamicStateGroup->addChild(orderingGroup.release());
	}

	return extendedDynamicStateGroup.release();
}

} // pipeline
} // vkt
