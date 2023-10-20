/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2020 The Khronos Group Inc.
* Copyright (c) 2020 Valve Corporation.
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
*//*
 * \file
 * \brief Extended dynamic state tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineExtendedDynamicStateTests.hpp"
#include "vktPipelineExtendedDynamicStateMiscTests.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuVector.hpp"
#include "tcuMaybe.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuCommandLine.hpp"

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
#include <cstddef>
#include <set>
#include <array>

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

#ifndef CTS_USES_VULKANSC
vk::VkProvokingVertexModeEXT makeProvokingVertexMode (bool lastVertex)
{
	return (lastVertex ? vk::VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT : vk::VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);
}
#endif // CTS_USES_VULKANSC

// Framebuffer size.
constexpr deUint32	kFramebufferWidth	= 64u;
constexpr deUint32	kFramebufferHeight	= 64u;
const auto			kFramebufferExtent	= vk::makeExtent3D(kFramebufferWidth, kFramebufferHeight, 1u);

// Image formats.
constexpr	vk::VkFormat	kUnormColorFormat		= vk::VK_FORMAT_R8G8B8A8_UNORM;
constexpr	vk::VkFormat	kIntColorFormat			= vk::VK_FORMAT_R8G8B8A8_UINT;
constexpr	vk::VkFormat	kIntRedColorFormat		= vk::VK_FORMAT_R32_UINT;
const		tcu::Vec4		kUnormColorThreshold	(0.005f); // 1/255 < 0.005 < 2/255.

// This sample count must be supported for all formats supporting VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT.
// See 44.1.1. Supported Sample Counts.
const auto kMultiSampleCount	= vk::VK_SAMPLE_COUNT_4_BIT;
const auto kSingleSampleCount	= vk::VK_SAMPLE_COUNT_1_BIT;

// Image usage flags.
const vk::VkImageUsageFlags kColorUsage	= (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
const vk::VkImageUsageFlags kDSUsage	= (vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

// Color components.
const auto CR = vk::VK_COLOR_COMPONENT_R_BIT;
const auto CG = vk::VK_COLOR_COMPONENT_G_BIT;
const auto CB = vk::VK_COLOR_COMPONENT_B_BIT;
const auto CA = vk::VK_COLOR_COMPONENT_A_BIT;

std::string componentCodes (vk::VkColorComponentFlags components)
{
	std::string name;

	if ((components & CR) != 0u) name += "r";
	if ((components & CG) != 0u) name += "g";
	if ((components & CB) != 0u) name += "b";
	if ((components & CA) != 0u) name += "a";

	if (name.empty())
		name = "0";
	return name;
}

// Chooses clear or geometry color depending on the selected components.
tcu::Vec4 filterColor (const tcu::Vec4& clearColor, const tcu::Vec4& color, vk::VkColorComponentFlags components)
{
	const tcu::Vec4 finalColor
	(
		(((components & CR) != 0u) ? color[0] : clearColor[0]),
		(((components & CG) != 0u) ? color[1] : clearColor[1]),
		(((components & CB) != 0u) ? color[2] : clearColor[2]),
		(((components & CA) != 0u) ? color[3] : clearColor[3])
	);
	return finalColor;
}

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

using StrideVec = std::vector<vk::VkDeviceSize>;

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

// We will use several data types in vertex bindings. Each type will need to define a few things.
class VertexGenerator
{
public:
	// Some generators may need specific features.
	virtual void													checkSupport (Context&) const {}


	// For GLSL.

	// Vertex input/output attribute declarations in GLSL form. One sentence per element.
	virtual std::vector<std::string>								getAttributeDeclarations()	const = 0;

	// Get statements to calculate a vec2 called "vertexCoords" using the vertex input attributes.
	virtual std::vector<std::string>								getVertexCoordCalc()		const = 0;

	// Get vertex binding declarations as part of descriptor sets, used for mesh shading.
	virtual std::vector<std::string>								getDescriptorDeclarations()	const = 0;

	// Get statements to calculate a vec2 called "vertexCoords" using descriptor members.
	virtual std::vector<std::string>								getDescriptorCoordCalc(TopologyClass topology) const = 0;

	// Get fragment input attribute declarations in GLSL form. One sentence per element.
	virtual std::vector<std::string>								getFragInputAttributes()	const { return std::vector<std::string>(); }

	// Get fragment output post-calculations, maybe altering the "color" output variable.
	virtual std::vector<std::string>								getFragOutputCalc()			const { return std::vector<std::string>(); }

	// GLSL extensions if needed.
	virtual std::vector<std::string>								getGLSLExtensions()			const { return std::vector<std::string>(); }


	// For the pipeline.

	// Vertex attributes for VkPipelineVertexInputStateCreateInfo.
	virtual std::vector<vk::VkVertexInputAttributeDescription>		getAttributeDescriptions() const = 0;

	// Vertex attributes for VK_EXT_vertex_input_dynamic_state.
	virtual std::vector<vk::VkVertexInputAttributeDescription2EXT>	getAttributeDescriptions2()	const = 0;

	// Vertex bindings for VkPipelineVertexInputStateCreateInfo.
	virtual std::vector<vk::VkVertexInputBindingDescription>		getBindingDescriptions (const StrideVec& strides) const = 0;

	// Vertex bindings for VK_EXT_vertex_input_dynamic_state.
	virtual std::vector<vk::VkVertexInputBindingDescription2EXT>	getBindingDescriptions2 (const StrideVec& strides) const = 0;

	// Create buffer data given an array of coordinates and an initial padding.
	virtual std::vector<std::vector<deUint8>>						createVertexData (const std::vector<tcu::Vec2>& coords, vk::VkDeviceSize dataOffset, vk::VkDeviceSize trailingPadding, const void* paddingPattern, size_t patternSize) const = 0;

	// Stride of vertex data in each binding.
	virtual std::vector<vk::VkDeviceSize>							getVertexDataStrides() const = 0;
};

// Auxiliar function to create these structs more easily.
vk::VkVertexInputAttributeDescription2EXT makeVertexInputAttributeDescription2EXT (deUint32 location, deUint32 binding, vk::VkFormat format, deUint32 offset)
{
	vk::VkVertexInputAttributeDescription2EXT desc = vk::initVulkanStructure();
	desc.location = location;
	desc.binding = binding;
	desc.format = format;
	desc.offset = offset;
	return desc;
}

vk::VkVertexInputBindingDescription2EXT makeVertexInputBindingDescription2EXT (deUint32 binding, deUint32 stride, vk::VkVertexInputRate inputRate)
{
	vk::VkVertexInputBindingDescription2EXT desc = vk::initVulkanStructure();
	desc.binding = binding;
	desc.stride = stride;
	desc.inputRate = inputRate;
	desc.divisor = 1u;
	return desc;
}

// Fill a section of the given buffer (from offset to offset+count) with repeating copies of the given data.
void fillWithPattern(void* ptr_, size_t offset, size_t count, const void* src, size_t srcSize)
{
	auto	ptr		= reinterpret_cast<char*>(ptr_);
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

// Create a single binding vertex data vector given a type T for vertex data.
template<class T>
std::vector<deUint8> createSingleBindingVertexData (const std::vector<tcu::Vec2>& coords, vk::VkDeviceSize dataOffset, vk::VkDeviceSize trailingPadding, const void* paddingPattern, size_t patternSize)
{
	DE_ASSERT(!coords.empty());

	const auto dataOffsetSz			= static_cast<size_t>(dataOffset);
	const auto trailingPaddingSz	= static_cast<size_t>(trailingPadding);

	std::vector<deUint8> buffer;
	buffer.resize(dataOffsetSz + coords.size() * sizeof(T) + trailingPaddingSz);

	fillWithPattern(buffer.data(), 0u, dataOffsetSz, paddingPattern, patternSize);

	auto pos = dataOffsetSz;
	for (const auto& coord : coords)
	{
		new (&buffer[pos]) T(coord);
		pos += sizeof(T);
	}

	fillWithPattern(buffer.data(), pos, trailingPaddingSz, paddingPattern, patternSize);

	return buffer;
}

// Vertices in buffers will have 2 components and a padding to properly test the stride.
// This is the vertex type that will be used normally.
class VertexWithPadding : public VertexGenerator
{
protected:
	struct VertexData
	{
		VertexData(const tcu::Vec2& coords_)
			: coords	(coords_)
			, padding	(0.0f, 0.0f)
		{}

		tcu::Vec2 coords;
		tcu::Vec2 padding;
	};

public:
	virtual std::vector<std::string> getAttributeDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.push_back("layout(location=0) in vec2 position;");
		return declarations;
	}

	virtual std::vector<std::string> getVertexCoordCalc() const override
	{
		std::vector<std::string> statements;
		statements.push_back("vec2 vertexCoords = position;");
		return statements;
	}

	virtual std::vector<std::string> getDescriptorDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.reserve(7u);
		declarations.push_back("struct VertexData {");
		declarations.push_back("    vec2 position;");
		declarations.push_back("    vec2 padding;");
		declarations.push_back("};");
		declarations.push_back("layout(set=0, binding=0, std430) readonly buffer S0B0Block {");
		declarations.push_back("    VertexData data[];");
		declarations.push_back("} s0b0buffer;");
		return declarations;
	}

	virtual std::vector<std::string> getDescriptorCoordCalc(TopologyClass topology) const override
	{
		std::vector<std::string> statements;

		if (topology == TopologyClass::TRIANGLE)
		{
			statements.reserve(4u);
			statements.push_back("uint prim = uint(gl_WorkGroupID.x);");
			statements.push_back("uint indices[3] = uint[](prim, (prim + (1 + prim % 2)), (prim + (2 - prim % 2)));");
			statements.push_back("uint invIndex = indices[gl_LocalInvocationIndex];");
			statements.push_back("vec2 vertexCoords = s0b0buffer.data[invIndex].position;");
		}
		else if (topology == TopologyClass::LINE)
		{
			statements.reserve(9u);
			statements.push_back("const uint linesPerRow = 3u;");
			statements.push_back("const uint verticesPerRow = 4u;");
			statements.push_back("uint lineIndex = uint(gl_WorkGroupID.x);");
			statements.push_back("uint rowIndex = lineIndex / linesPerRow;");
			statements.push_back("uint lineInRow = lineIndex % linesPerRow;");
			statements.push_back("uint firstVertex = rowIndex * verticesPerRow + lineInRow;");
			statements.push_back("uint indices[2] = uint[](firstVertex, firstVertex + 1u);");
			statements.push_back("uint invIndex = indices[gl_LocalInvocationIndex];");
			statements.push_back("vec2 vertexCoords = s0b0buffer.data[invIndex].position;");
		}
		else
			DE_ASSERT(false);

		return statements;
	}

	virtual std::vector<vk::VkVertexInputAttributeDescription> getAttributeDescriptions() const override
	{
		std::vector<vk::VkVertexInputAttributeDescription> descriptions;
		descriptions.push_back(vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u));
		return descriptions;
	}

	// Vertex attributes for VK_EXT_vertex_input_dynamic_state.
	virtual std::vector<vk::VkVertexInputAttributeDescription2EXT> getAttributeDescriptions2() const override
	{
		std::vector<vk::VkVertexInputAttributeDescription2EXT> descriptions;
		descriptions.push_back(makeVertexInputAttributeDescription2EXT(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u));
		return descriptions;
	}

	// Vertex bindings for VkPipelineVertexInputStateCreateInfo.
	virtual std::vector<vk::VkVertexInputBindingDescription> getBindingDescriptions(const StrideVec& strides) const override
	{
		std::vector<vk::VkVertexInputBindingDescription> descriptions;
		descriptions.push_back(vk::makeVertexInputBindingDescription(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		return descriptions;
	}

	// Vertex bindings for VK_EXT_vertex_input_dynamic_state.
	virtual std::vector<vk::VkVertexInputBindingDescription2EXT> getBindingDescriptions2(const StrideVec& strides) const override
	{
		std::vector<vk::VkVertexInputBindingDescription2EXT> descriptions;
		descriptions.push_back(makeVertexInputBindingDescription2EXT(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		return descriptions;
	}

	virtual std::vector<std::vector<deUint8>> createVertexData (const std::vector<tcu::Vec2>& coords, vk::VkDeviceSize dataOffset, vk::VkDeviceSize trailingPadding, const void* paddingPattern, size_t patternSize) const override
	{
		return std::vector<std::vector<deUint8>>(1u, createSingleBindingVertexData<VertexData>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));
	}

	virtual std::vector<vk::VkDeviceSize> getVertexDataStrides() const override
	{
		return std::vector<vk::VkDeviceSize>(1u, static_cast<vk::VkDeviceSize>(sizeof(VertexData)));
	}
};

// Vertices in buffers will have 2 components and a padding. Same as VertexWithPadding but using 16-bit floats.
class VertexWithPadding16 : public VertexGenerator
{
protected:
	struct VertexData
	{
		VertexData(const tcu::Vec2& coords_)
			: coords	(tcu::Float16(coords_.x()), tcu::Float16(coords_.y()))
			, padding	(tcu::Float16(0.0f), tcu::Float16(0.0f))
		{}

		tcu::F16Vec2 coords;
		tcu::F16Vec2 padding;
	};

public:
	virtual void checkSupport (Context& context) const override
	{
		// We need shaderFloat16 and storageInputOutput16.
		const auto& sf16i8Features = context.getShaderFloat16Int8Features();
		if (!sf16i8Features.shaderFloat16)
			TCU_THROW(NotSupportedError, "shaderFloat16 not supported");

		const auto& storage16Features = context.get16BitStorageFeatures();
		if (!storage16Features.storageInputOutput16)
			TCU_THROW(NotSupportedError, "storageInputOutput16 not supported");
	}

	virtual std::vector<std::string> getGLSLExtensions() const override
	{
		std::vector<std::string> extensions;
		extensions.push_back("#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require");
		return extensions;
	}

	virtual std::vector<std::string> getAttributeDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.push_back("layout(location=0) in f16vec2 position;");
		return declarations;
	}

	virtual std::vector<std::string> getVertexCoordCalc() const override
	{
		std::vector<std::string> statements;
		statements.push_back("f16vec2 vertexCoords = position;");
		return statements;
	}

	virtual std::vector<std::string> getDescriptorDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.reserve(7u);
		declarations.push_back("struct VertexData {");
		declarations.push_back("    f16vec2 position;");
		declarations.push_back("    f16vec2 padding;");
		declarations.push_back("};");
		declarations.push_back("layout(set=0, binding=0, std430) readonly buffer S0B0Block {");
		declarations.push_back("    VertexData data[];");
		declarations.push_back("} s0b0buffer;");
		return declarations;
	}

	virtual std::vector<std::string> getDescriptorCoordCalc(TopologyClass topology) const override
	{
		std::vector<std::string> statements;

		if (topology == TopologyClass::TRIANGLE)
		{
			statements.reserve(4u);
			statements.push_back("uint prim = uint(gl_WorkGroupID.x);");
			statements.push_back("uint indices[3] = uint[](prim, (prim + (1 + prim % 2)), (prim + (2 - prim % 2)));");
			statements.push_back("uint invIndex = indices[gl_LocalInvocationIndex];");
			statements.push_back("f16vec2 vertexCoords = s0b0buffer.data[invIndex].position;");
		}
		else if (topology == TopologyClass::LINE)
		{
			statements.reserve(9u);
			statements.push_back("const uint linesPerRow = 3u;");
			statements.push_back("const uint verticesPerRow = 4u;");
			statements.push_back("uint lineIndex = uint(gl_WorkGroupID.x);");
			statements.push_back("uint rowIndex = lineIndex / linesPerRow;");
			statements.push_back("uint lineInRow = lineIndex % linesPerRow;");
			statements.push_back("uint firstVertex = rowIndex * verticesPerRow + lineInRow;");
			statements.push_back("uint indices[2] = uint[](firstVertex, firstVertex + 1u);");
			statements.push_back("uint invIndex = indices[gl_LocalInvocationIndex];");
			statements.push_back("f16vec2 vertexCoords = s0b0buffer.data[invIndex].position;");
		}
		else
			DE_ASSERT(false);

		return statements;
	}

	virtual std::vector<vk::VkVertexInputAttributeDescription> getAttributeDescriptions() const override
	{
		std::vector<vk::VkVertexInputAttributeDescription> descriptions;
		descriptions.push_back(vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R16G16_SFLOAT, 0u));
		return descriptions;
	}

	// Vertex attributes for VK_EXT_vertex_input_dynamic_state.
	virtual std::vector<vk::VkVertexInputAttributeDescription2EXT> getAttributeDescriptions2() const override
	{
		std::vector<vk::VkVertexInputAttributeDescription2EXT> descriptions;
		descriptions.push_back(makeVertexInputAttributeDescription2EXT(0u, 0u, vk::VK_FORMAT_R16G16_SFLOAT, 0u));
		return descriptions;
	}

	// Vertex bindings for VkPipelineVertexInputStateCreateInfo.
	virtual std::vector<vk::VkVertexInputBindingDescription> getBindingDescriptions(const StrideVec& strides) const override
	{
		std::vector<vk::VkVertexInputBindingDescription> descriptions;
		descriptions.push_back(vk::makeVertexInputBindingDescription(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		return descriptions;
	}

	// Vertex bindings for VK_EXT_vertex_input_dynamic_state.
	virtual std::vector<vk::VkVertexInputBindingDescription2EXT> getBindingDescriptions2(const StrideVec& strides) const override
	{
		std::vector<vk::VkVertexInputBindingDescription2EXT> descriptions;
		descriptions.push_back(makeVertexInputBindingDescription2EXT(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		return descriptions;
	}

	virtual std::vector<std::vector<deUint8>> createVertexData (const std::vector<tcu::Vec2>& coords, vk::VkDeviceSize dataOffset, vk::VkDeviceSize trailingPadding, const void* paddingPattern, size_t patternSize) const override
	{
		return std::vector<std::vector<deUint8>>(1u, createSingleBindingVertexData<VertexData>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));
	}

	virtual std::vector<vk::VkDeviceSize> getVertexDataStrides() const override
	{
		return std::vector<vk::VkDeviceSize>(1u, static_cast<vk::VkDeviceSize>(sizeof(VertexData)));
	}
};

// Two buffers (bindings): one with vertex data, stored contiguously without paddings, and one with instance data. Instance data
// will not be stored contiguously: the stride will be twice that of the data size, and the padding space filled with "garbage".
// Real instance data will contain a scale and an offset similar to the ones from push constants, and will be used to properly scale
// and offset meshes to make them cover the top and bottom halves of the framebuffer.
class VertexWithInstanceData : public VertexGenerator
{
protected:
	struct InstanceData
	{
		InstanceData (const tcu::Vec2& scaleAndOffsetY_)
			: scaleAndOffsetY	(scaleAndOffsetY_)
			, garbage			(0.0f /* bad scale */, 777.0f /* bad offset */)
		{}

		tcu::Vec2 scaleAndOffsetY;
		tcu::Vec2 garbage;
	};

public:
	virtual std::vector<std::string> getAttributeDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.push_back("layout(location=0) in vec2 position;");
		declarations.push_back("layout(location=1) in vec2 scaleAndOffsetY;");
		return declarations;
	}

	virtual std::vector<std::string> getVertexCoordCalc() const override
	{
		std::vector<std::string> statements;
		statements.push_back("vec2 vertexCoords = vec2(position.x, position.y * scaleAndOffsetY.x + scaleAndOffsetY.y);");
		return statements;
	}

	virtual std::vector<std::string> getDescriptorDeclarations() const override
	{
		DE_ASSERT(false); // This vertex generator should not be used with mesh shaders.
		return std::vector<std::string>();
	}

	virtual std::vector<std::string> getDescriptorCoordCalc(TopologyClass) const override
	{
		DE_ASSERT(false); // This vertex generator should not be used with mesh shaders.
		return std::vector<std::string>();
	}

	virtual std::vector<vk::VkVertexInputAttributeDescription> getAttributeDescriptions() const override
	{
		std::vector<vk::VkVertexInputAttributeDescription> descriptions;
		descriptions.push_back(vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u));
		descriptions.push_back(vk::makeVertexInputAttributeDescription(1u, 1u, vk::VK_FORMAT_R32G32_SFLOAT, 0u));
		return descriptions;
	}

	// Vertex attributes for VK_EXT_vertex_input_dynamic_state.
	virtual std::vector<vk::VkVertexInputAttributeDescription2EXT> getAttributeDescriptions2() const override
	{
		std::vector<vk::VkVertexInputAttributeDescription2EXT> descriptions;
		descriptions.push_back(makeVertexInputAttributeDescription2EXT(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u));
		descriptions.push_back(makeVertexInputAttributeDescription2EXT(1u, 1u, vk::VK_FORMAT_R32G32_SFLOAT, 0u));
		return descriptions;
	}

	// Vertex bindings for VkPipelineVertexInputStateCreateInfo.
	virtual std::vector<vk::VkVertexInputBindingDescription> getBindingDescriptions(const StrideVec& strides) const override
	{
		std::vector<vk::VkVertexInputBindingDescription> descriptions;
		descriptions.push_back(vk::makeVertexInputBindingDescription(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		descriptions.push_back(vk::makeVertexInputBindingDescription(1u, static_cast<deUint32>(strides.at(1)), vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		return descriptions;
	}

	// Vertex bindings for VK_EXT_vertex_input_dynamic_state.
	virtual std::vector<vk::VkVertexInputBindingDescription2EXT> getBindingDescriptions2(const StrideVec& strides) const override
	{
		std::vector<vk::VkVertexInputBindingDescription2EXT> descriptions;
		descriptions.push_back(makeVertexInputBindingDescription2EXT(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		descriptions.push_back(makeVertexInputBindingDescription2EXT(1u, static_cast<deUint32>(strides.at(1)), vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		return descriptions;
	}

	virtual std::vector<std::vector<deUint8>> createVertexData (const std::vector<tcu::Vec2>& coords, vk::VkDeviceSize dataOffset, vk::VkDeviceSize trailingPadding, const void* paddingPattern, size_t patternSize) const override
	{
		// Instance data for 2 instances. Scale and offset like we do with push constants.
		const std::vector<tcu::Vec2> instanceIds
		{
			tcu::Vec2(0.5f, -0.5f),
			tcu::Vec2(0.5f,  0.5f),
		};

		std::vector<std::vector<uint8_t>> buffers;
		buffers.reserve(2u);
		buffers.push_back(createSingleBindingVertexData<tcu::Vec2>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));
		buffers.push_back(createSingleBindingVertexData<InstanceData>(instanceIds, dataOffset, trailingPadding, paddingPattern, patternSize));

		return buffers;
	}

	virtual std::vector<vk::VkDeviceSize> getVertexDataStrides() const override
	{
		std::vector<vk::VkDeviceSize> strides;
		strides.reserve(2u);
		strides.push_back(static_cast<vk::VkDeviceSize>(sizeof(tcu::Vec2)));
		strides.push_back(static_cast<vk::VkDeviceSize>(sizeof(InstanceData)));
		return strides;
	}
};

// Vertex generator used when testing provoking vertices. It has an extra flat vertex output that's also a frag input. Note this
// generator only works with 3 vertices.
class ProvokingVertexWithPadding : public VertexWithPadding
{
protected:
	bool m_lastVertex;

public:
	ProvokingVertexWithPadding (bool lastVertex)
		: m_lastVertex (lastVertex)
	{}

	virtual std::vector<std::string> getAttributeDeclarations() const override
	{
		auto declarations = VertexWithPadding::getAttributeDeclarations();
		declarations.push_back("layout(location=0) flat out uint colorMultiplier;");
		return declarations;
	}

	virtual std::vector<std::string> getDescriptorDeclarations() const override
	{
		auto declarations = VertexWithPadding::getDescriptorDeclarations();
		declarations.push_back("layout(location=0) flat out uint colorMultiplier[];");
		return declarations;
	}

	virtual std::vector<std::string> getVertexCoordCalc() const override
	{
		auto statements = VertexWithPadding::getVertexCoordCalc();
		statements.push_back("const bool provokingLast = " + std::string(m_lastVertex ? "true" : "false") + ";");
		statements.push_back("colorMultiplier = (((!provokingLast && gl_VertexIndex == 0) || (provokingLast && gl_VertexIndex == 2)) ? 1 : 0);");
		return statements;
	}

	virtual std::vector<std::string> getDescriptorCoordCalc(TopologyClass topology) const override
	{
		auto statements = VertexWithPadding::getDescriptorCoordCalc(topology);
		statements.push_back("const bool provokingLast = " + std::string(m_lastVertex ? "true" : "false") + ";");
		statements.push_back("colorMultiplier[gl_LocalInvocationIndex] = (((!provokingLast && gl_LocalInvocationIndex == 0) || (provokingLast && gl_LocalInvocationIndex == gl_WorkGroupSize.x - 1u)) ? 1 : 0);");
		return statements;
	}

	virtual std::vector<std::vector<deUint8>> createVertexData (const std::vector<tcu::Vec2>& coords, vk::VkDeviceSize dataOffset, vk::VkDeviceSize trailingPadding, const void* paddingPattern, size_t patternSize) const override
	{
		static constexpr uint32_t kExpectecdCoordCount = 3u;
		DE_UNREF(kExpectecdCoordCount); // For release builds.
		DE_ASSERT(coords.size() == kExpectecdCoordCount);
		return VertexWithPadding::createVertexData(coords, dataOffset, trailingPadding, paddingPattern, patternSize);
	}

	virtual std::vector<std::string> getFragInputAttributes() const override
	{
		std::vector<std::string> declarations;
		declarations.push_back("layout(location=0) flat in uint colorMultiplier;");
		return declarations;
	}

	virtual std::vector<std::string> getFragOutputCalc() const override
	{
		std::vector<std::string> statements;
		statements.push_back("color = color * float(colorMultiplier);");
		return statements;
	}
};

// Vertices with coordinates, padding and an extra constant field.
class VertexWithExtraAttributes : public VertexGenerator
{
protected:
	struct VertexData
	{
		VertexData (const tcu::Vec2& coords_)
			: coords	(coords_)
			, ones		(1.0f, 1.0f)
		{
			deMemset(padding, 0, sizeof(padding));
		}

		tcu::Vec2 coords;
		tcu::Vec2 padding[10];
		tcu::Vec2 ones;
	};

public:
	virtual std::vector<std::string> getAttributeDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.reserve(2u);
		declarations.push_back("layout(location=0) in vec2 position;");
		declarations.push_back("layout(location=1) in vec2 ones;");
		return declarations;
	}

	virtual std::vector<std::string> getVertexCoordCalc() const override
	{
		std::vector<std::string> statements;
		statements.reserve(2u);
		statements.push_back("vec2 vertexCoords = position;");
		statements.push_back("vertexCoords = vertexCoords * ones;");
		return statements;
	}

	virtual std::vector<std::string> getDescriptorDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.reserve(8u);
		declarations.push_back("struct VertexData {");
		declarations.push_back("    vec2 coords;");
		declarations.push_back("    vec2 padding[10];");
		declarations.push_back("    vec2 ones;");
		declarations.push_back("};");
		declarations.push_back("layout(set=0, binding=0, std430) readonly buffer S0B0Block {");
		declarations.push_back("    VertexData data[];");
		declarations.push_back("} s0b0buffer;");
		return declarations;
	}

	virtual std::vector<std::string> getDescriptorCoordCalc(TopologyClass topology) const override
	{
		std::vector<std::string> statements;

		if (topology == TopologyClass::TRIANGLE)
		{
			statements.reserve(6u);
			statements.push_back("uint prim = uint(gl_WorkGroupID.x);");
			statements.push_back("uint indices[3] = uint[](prim, (prim + (1 + prim % 2)), (prim + (2 - prim % 2)));");
			statements.push_back("uint invIndex = indices[gl_LocalInvocationIndex];");
			statements.push_back("vec2 auxPos = s0b0buffer.data[invIndex].coords;");
			statements.push_back("vec2 auxOnes = s0b0buffer.data[invIndex].ones;");
			statements.push_back("vec2 vertexCoords = auxPos * auxOnes;");
		}
		else if (topology == TopologyClass::LINE)
		{
			statements.reserve(11u);
			statements.push_back("const uint linesPerRow = 3u;");
			statements.push_back("const uint verticesPerRow = 4u;");
			statements.push_back("uint lineIndex = uint(gl_WorkGroupID.x);");
			statements.push_back("uint rowIndex = lineIndex / linesPerRow;");
			statements.push_back("uint lineInRow = lineIndex % linesPerRow;");
			statements.push_back("uint firstVertex = rowIndex * verticesPerRow + lineInRow;");
			statements.push_back("uint indices[2] = uint[](firstVertex, firstVertex + 1u);");
			statements.push_back("uint invIndex = indices[gl_LocalInvocationIndex];");
			statements.push_back("vec2 auxPos = s0b0buffer.data[invIndex].coords;");
			statements.push_back("vec2 auxOnes = s0b0buffer.data[invIndex].ones;");
			statements.push_back("vec2 vertexCoords = auxPos * auxOnes;");
		}
		else
			DE_ASSERT(false);

		return statements;
	}

	virtual std::vector<vk::VkVertexInputAttributeDescription> getAttributeDescriptions() const override
	{
		std::vector<vk::VkVertexInputAttributeDescription> descriptions;
		descriptions.push_back(vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u));
		descriptions.push_back(vk::makeVertexInputAttributeDescription(1u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, static_cast<deUint32>(offsetof(VertexData, ones))));
		return descriptions;
	}

	virtual std::vector<vk::VkVertexInputAttributeDescription2EXT> getAttributeDescriptions2() const override
	{
		std::vector<vk::VkVertexInputAttributeDescription2EXT> descriptions;
		descriptions.push_back(makeVertexInputAttributeDescription2EXT(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u));
		descriptions.push_back(makeVertexInputAttributeDescription2EXT(1u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, static_cast<deUint32>(offsetof(VertexData, ones))));
		return descriptions;
	}

	virtual std::vector<vk::VkVertexInputBindingDescription> getBindingDescriptions(const StrideVec& strides) const override
	{
		std::vector<vk::VkVertexInputBindingDescription> descriptions;
		descriptions.push_back(vk::makeVertexInputBindingDescription(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		return descriptions;
	}

	virtual std::vector<vk::VkVertexInputBindingDescription2EXT> getBindingDescriptions2(const StrideVec& strides) const override
	{
		std::vector<vk::VkVertexInputBindingDescription2EXT> descriptions;
		descriptions.push_back(makeVertexInputBindingDescription2EXT(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		return descriptions;
	}

	virtual std::vector<std::vector<deUint8>> createVertexData (const std::vector<tcu::Vec2>& coords, vk::VkDeviceSize dataOffset, vk::VkDeviceSize trailingPadding, const void* paddingPattern, size_t patternSize) const override
	{
		return std::vector<std::vector<deUint8>>(1u, createSingleBindingVertexData<VertexData>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));
	}

	virtual std::vector<vk::VkDeviceSize> getVertexDataStrides() const override
	{
		return std::vector<vk::VkDeviceSize>(1u, static_cast<vk::VkDeviceSize>(sizeof(VertexData)));
	}
};

// Vertices using multiple bindings and constant fields.
// Binding 0: no data actually used.
// Binding 1: contains location 0, array of PaddingOnes.
// Binding 2: no data actually used.
// Binding 3: contains location 1, array of CoordsData.
// Binding 4: no data actually used.
// Binding 5: contains location 2, array of OneZeroPadding.
// See getAttributeDeclarations().
class MultipleBindingsVertex : public VertexGenerator
{
protected:
	struct CoordsData
	{
		tcu::Vec2 padding0;
		tcu::Vec2 coords;
		tcu::Vec2 padding1;

		CoordsData (const tcu::Vec2& coords_)
			: padding0	(0.0f, 3.0f)
			, coords	(coords_)
			, padding1	(3.0f, 0.0f)
		{}
	};

	struct PaddingOnes
	{
		tcu::Vec2 padding[4];
		tcu::Vec2 ones;

		PaddingOnes (const tcu::Vec2&)
			: ones	(1.0f, 1.0f)
		{
			deMemset(&padding, 0, sizeof(padding));
		}
	};

	struct OneZeroPadding
	{
		tcu::Vec4 oneZero;
		tcu::Vec2 padding[3];

		OneZeroPadding (const tcu::Vec2&)
			: oneZero	(1.0f, 1.0f, 0.0f, 0.0f)
		{
			deMemset(&padding, 0, sizeof(padding));
		}
	};

	struct Zeros
	{
		tcu::Vec2 zeros;

		Zeros (const tcu::Vec2&)
			: zeros	(0.0f, 0.0f)
		{}
	};

public:
	virtual std::vector<std::string> getAttributeDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.reserve(3u);

		declarations.push_back("layout(location=0) in vec2 ones;");
		declarations.push_back("layout(location=1) in vec2 position;");
		declarations.push_back("layout(location=2) in vec4 oneZero;");

		return declarations;
	}

	virtual std::vector<std::string> getVertexCoordCalc() const override
	{
		std::vector<std::string> statements;
		statements.reserve(2u);

		statements.push_back("vec2 vertexCoords = position;");
		statements.push_back("vertexCoords = ((vertexCoords * ones) + oneZero.zw) * oneZero.xy;");

		return statements;
	}

	virtual std::vector<std::string> getDescriptorDeclarations() const override
	{
		std::vector<std::string> declarations;
		declarations.reserve(23u);

		declarations.push_back("struct PaddingOnes {");
		declarations.push_back("    vec2 padding[4];");
		declarations.push_back("    vec2 ones;");
		declarations.push_back("};");
		declarations.push_back("struct CoordsData {");
		declarations.push_back("    vec2 padding0;");
		declarations.push_back("    vec2 coords;");
		declarations.push_back("    vec2 padding1;");
		declarations.push_back("};");
		declarations.push_back("struct OneZeroPadding {");
		declarations.push_back("    vec2 ones;");		// Note: we split the vec4 into two vec2s to match CPU-side alignment.
		declarations.push_back("    vec2 zeros;");
		declarations.push_back("    vec2 padding[3];");
		declarations.push_back("};");
		declarations.push_back("layout(set=0, binding=1, std430) readonly buffer S0B1Block {");
		declarations.push_back("    PaddingOnes data[];");
		declarations.push_back("} s0b1buffer;");
		declarations.push_back("layout(set=0, binding=3, std430) readonly buffer S0B3Block {");
		declarations.push_back("    CoordsData data[];");
		declarations.push_back("} s0b3buffer;");
		declarations.push_back("layout(set=0, binding=4, std430) readonly buffer S0B5Block {");
		declarations.push_back("    OneZeroPadding data[];");
		declarations.push_back("} s0b5buffer;");

		return declarations;
	}

	virtual std::vector<std::string> getDescriptorCoordCalc(TopologyClass topology) const override
	{
		std::vector<std::string> statements;

		if (topology == TopologyClass::TRIANGLE)
		{
			statements.reserve(8u);
			statements.push_back("uint prim = uint(gl_WorkGroupID.x);");
			statements.push_back("uint indices[3] = uint[](prim, (prim + (1 + prim % 2)), (prim + (2 - prim % 2)));");
			statements.push_back("uint invIndex = indices[gl_LocalInvocationIndex];");
			statements.push_back("vec2 auxOnes1 = s0b1buffer.data[invIndex].ones;");
			statements.push_back("vec2 auxCoords = s0b3buffer.data[invIndex].coords;");
			statements.push_back("vec2 auxOnes5 = s0b5buffer.data[invIndex].ones;");
			statements.push_back("vec2 auxZeros = s0b5buffer.data[invIndex].zeros;");
			statements.push_back("vec2 vertexCoords = ((auxCoords * auxOnes1) + auxZeros) * auxOnes5;");
		}
		else if (topology == TopologyClass::LINE)
		{
			statements.reserve(13u);
			statements.push_back("const uint linesPerRow = 3u;");
			statements.push_back("const uint verticesPerRow = 4u;");
			statements.push_back("uint lineIndex = uint(gl_WorkGroupID.x);");
			statements.push_back("uint rowIndex = lineIndex / linesPerRow;");
			statements.push_back("uint lineInRow = lineIndex % linesPerRow;");
			statements.push_back("uint firstVertex = rowIndex * verticesPerRow + lineInRow;");
			statements.push_back("uint indices[2] = uint[](firstVertex, firstVertex + 1u);");
			statements.push_back("uint invIndex = indices[gl_LocalInvocationIndex];");
			statements.push_back("vec2 auxOnes1 = s0b1buffer.data[invIndex].ones;");
			statements.push_back("vec2 auxCoords = s0b3buffer.data[invIndex].coords;");
			statements.push_back("vec2 auxOnes5 = s0b5buffer.data[invIndex].ones;");
			statements.push_back("vec2 auxZeros = s0b5buffer.data[invIndex].zeros;");
			statements.push_back("vec2 vertexCoords = ((auxCoords * auxOnes1) + auxZeros) * auxOnes5;");
		}
		else
			DE_ASSERT(false);

		return statements;
	}

	virtual std::vector<vk::VkVertexInputAttributeDescription> getAttributeDescriptions() const override
	{
		// We create the descriptions vector out of order to make it more interesting. See the attribute declarations.
		std::vector<vk::VkVertexInputAttributeDescription> descriptions;
		descriptions.reserve(3u);

		descriptions.push_back(vk::makeVertexInputAttributeDescription(1u, 3u, vk::VK_FORMAT_R32G32_SFLOAT, static_cast<deUint32>(offsetof(CoordsData, coords))));
		descriptions.push_back(vk::makeVertexInputAttributeDescription(2u, 5u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<deUint32>(offsetof(OneZeroPadding, oneZero))));
		descriptions.push_back(vk::makeVertexInputAttributeDescription(0u, 1u, vk::VK_FORMAT_R32G32_SFLOAT, static_cast<deUint32>(offsetof(PaddingOnes, ones))));

		return descriptions;
	}

	virtual std::vector<vk::VkVertexInputAttributeDescription2EXT> getAttributeDescriptions2() const override
	{
		// We create the descriptions vector out of order to make it more interesting. See the attribute declarations.
		std::vector<vk::VkVertexInputAttributeDescription2EXT> descriptions;
		descriptions.reserve(3u);

		descriptions.push_back(makeVertexInputAttributeDescription2EXT(2u, 5u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<deUint32>(offsetof(OneZeroPadding, oneZero))));
		descriptions.push_back(makeVertexInputAttributeDescription2EXT(1u, 3u, vk::VK_FORMAT_R32G32_SFLOAT, static_cast<deUint32>(offsetof(CoordsData, coords))));
		descriptions.push_back(makeVertexInputAttributeDescription2EXT(0u, 1u, vk::VK_FORMAT_R32G32_SFLOAT, static_cast<deUint32>(offsetof(PaddingOnes, ones))));

		return descriptions;
	}

	virtual std::vector<vk::VkVertexInputBindingDescription> getBindingDescriptions(const StrideVec& strides) const override
	{
		// Provide descriptions out of order to make it more interesting.
		std::vector<vk::VkVertexInputBindingDescription> descriptions;
		descriptions.reserve(6u);

		descriptions.push_back(vk::makeVertexInputBindingDescription(2u, static_cast<deUint32>(strides.at(2)), vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		descriptions.push_back(vk::makeVertexInputBindingDescription(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		descriptions.push_back(vk::makeVertexInputBindingDescription(1u, static_cast<deUint32>(strides.at(1)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		descriptions.push_back(vk::makeVertexInputBindingDescription(4u, static_cast<deUint32>(strides.at(4)), vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		descriptions.push_back(vk::makeVertexInputBindingDescription(3u, static_cast<deUint32>(strides.at(3)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		descriptions.push_back(vk::makeVertexInputBindingDescription(5u, static_cast<deUint32>(strides.at(5)), vk::VK_VERTEX_INPUT_RATE_VERTEX));

		return descriptions;
	}

	virtual std::vector<vk::VkVertexInputBindingDescription2EXT> getBindingDescriptions2(const StrideVec& strides) const override
	{
		// Provide descriptions out of order to make it more interesting.
		std::vector<vk::VkVertexInputBindingDescription2EXT> descriptions;
		descriptions.reserve(6u);

		descriptions.push_back(makeVertexInputBindingDescription2EXT(2u, static_cast<deUint32>(strides.at(2)), vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		descriptions.push_back(makeVertexInputBindingDescription2EXT(0u, static_cast<deUint32>(strides.at(0)), vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		descriptions.push_back(makeVertexInputBindingDescription2EXT(1u, static_cast<deUint32>(strides.at(1)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		descriptions.push_back(makeVertexInputBindingDescription2EXT(5u, static_cast<deUint32>(strides.at(5)), vk::VK_VERTEX_INPUT_RATE_VERTEX));
		descriptions.push_back(makeVertexInputBindingDescription2EXT(4u, static_cast<deUint32>(strides.at(4)), vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		descriptions.push_back(makeVertexInputBindingDescription2EXT(3u, static_cast<deUint32>(strides.at(3)), vk::VK_VERTEX_INPUT_RATE_VERTEX));

		return descriptions;
	}

	virtual std::vector<std::vector<deUint8>> createVertexData (const std::vector<tcu::Vec2>& coords, vk::VkDeviceSize dataOffset, vk::VkDeviceSize trailingPadding, const void* paddingPattern, size_t patternSize) const override
	{
		std::vector<std::vector<deUint8>> result;
		result.reserve(6u);

		result.push_back(createSingleBindingVertexData<Zeros>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));			// Not actually used.
		result.push_back(createSingleBindingVertexData<PaddingOnes>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));		// Binding 1 contains location=0 as PaddingOnes.
		result.push_back(createSingleBindingVertexData<Zeros>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));			// Not actually used.
		result.push_back(createSingleBindingVertexData<CoordsData>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));		// Binding 3 contains location=1 as CoordsData.
		result.push_back(createSingleBindingVertexData<Zeros>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));			// Not actually used.
		result.push_back(createSingleBindingVertexData<OneZeroPadding>(coords, dataOffset, trailingPadding, paddingPattern, patternSize));	// Binding 5 contains location=2 as OneZeroPadding.

		return result;
	}

	virtual std::vector<vk::VkDeviceSize> getVertexDataStrides() const override
	{
		std::vector<vk::VkDeviceSize> strides;
		strides.reserve(6u);

		strides.push_back(static_cast<vk::VkDeviceSize>(sizeof(Zeros)));
		strides.push_back(static_cast<vk::VkDeviceSize>(sizeof(PaddingOnes)));
		strides.push_back(static_cast<vk::VkDeviceSize>(sizeof(Zeros)));
		strides.push_back(static_cast<vk::VkDeviceSize>(sizeof(CoordsData)));
		strides.push_back(static_cast<vk::VkDeviceSize>(sizeof(Zeros)));
		strides.push_back(static_cast<vk::VkDeviceSize>(sizeof(OneZeroPadding)));

		return strides;
	}
};

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

struct DepthBiasParams
{
	float constantFactor;
	float clamp;
};

bool isAdvancedBlendOp (const vk::VkBlendOp blendOp)
{
	bool advanced = false;

	switch (blendOp)
	{
	case vk::VK_BLEND_OP_ZERO_EXT:
	case vk::VK_BLEND_OP_SRC_EXT:
	case vk::VK_BLEND_OP_DST_EXT:
	case vk::VK_BLEND_OP_SRC_OVER_EXT:
	case vk::VK_BLEND_OP_DST_OVER_EXT:
	case vk::VK_BLEND_OP_SRC_IN_EXT:
	case vk::VK_BLEND_OP_DST_IN_EXT:
	case vk::VK_BLEND_OP_SRC_OUT_EXT:
	case vk::VK_BLEND_OP_DST_OUT_EXT:
	case vk::VK_BLEND_OP_SRC_ATOP_EXT:
	case vk::VK_BLEND_OP_DST_ATOP_EXT:
	case vk::VK_BLEND_OP_XOR_EXT:
	case vk::VK_BLEND_OP_MULTIPLY_EXT:
	case vk::VK_BLEND_OP_SCREEN_EXT:
	case vk::VK_BLEND_OP_OVERLAY_EXT:
	case vk::VK_BLEND_OP_DARKEN_EXT:
	case vk::VK_BLEND_OP_LIGHTEN_EXT:
	case vk::VK_BLEND_OP_COLORDODGE_EXT:
	case vk::VK_BLEND_OP_COLORBURN_EXT:
	case vk::VK_BLEND_OP_HARDLIGHT_EXT:
	case vk::VK_BLEND_OP_SOFTLIGHT_EXT:
	case vk::VK_BLEND_OP_DIFFERENCE_EXT:
	case vk::VK_BLEND_OP_EXCLUSION_EXT:
	case vk::VK_BLEND_OP_INVERT_EXT:
	case vk::VK_BLEND_OP_INVERT_RGB_EXT:
	case vk::VK_BLEND_OP_LINEARDODGE_EXT:
	case vk::VK_BLEND_OP_LINEARBURN_EXT:
	case vk::VK_BLEND_OP_VIVIDLIGHT_EXT:
	case vk::VK_BLEND_OP_LINEARLIGHT_EXT:
	case vk::VK_BLEND_OP_PINLIGHT_EXT:
	case vk::VK_BLEND_OP_HARDMIX_EXT:
	case vk::VK_BLEND_OP_HSL_HUE_EXT:
	case vk::VK_BLEND_OP_HSL_SATURATION_EXT:
	case vk::VK_BLEND_OP_HSL_COLOR_EXT:
	case vk::VK_BLEND_OP_HSL_LUMINOSITY_EXT:
	case vk::VK_BLEND_OP_PLUS_EXT:
	case vk::VK_BLEND_OP_PLUS_CLAMPED_EXT:
	case vk::VK_BLEND_OP_PLUS_CLAMPED_ALPHA_EXT:
	case vk::VK_BLEND_OP_PLUS_DARKER_EXT:
	case vk::VK_BLEND_OP_MINUS_EXT:
	case vk::VK_BLEND_OP_MINUS_CLAMPED_EXT:
	case vk::VK_BLEND_OP_CONTRAST_EXT:
	case vk::VK_BLEND_OP_INVERT_OVG_EXT:
	case vk::VK_BLEND_OP_RED_EXT:
	case vk::VK_BLEND_OP_GREEN_EXT:
	case vk::VK_BLEND_OP_BLUE_EXT:
		advanced = true;
		break;
	default:
		advanced = false;
		break;
	}

	return advanced;
}

struct ColorBlendEq
{
	vk::VkBlendFactor	srcColorBlendFactor;
	vk::VkBlendFactor	dstColorBlendFactor;
	vk::VkBlendOp		colorBlendOp;
	vk::VkBlendFactor	srcAlphaBlendFactor;
	vk::VkBlendFactor	dstAlphaBlendFactor;
	vk::VkBlendOp		alphaBlendOp;

	ColorBlendEq ()
		: srcColorBlendFactor	(vk::VK_BLEND_FACTOR_ZERO)
		, dstColorBlendFactor	(vk::VK_BLEND_FACTOR_ZERO)
		, colorBlendOp			(vk::VK_BLEND_OP_ADD)
		, srcAlphaBlendFactor	(vk::VK_BLEND_FACTOR_ZERO)
		, dstAlphaBlendFactor	(vk::VK_BLEND_FACTOR_ZERO)
		, alphaBlendOp			(vk::VK_BLEND_OP_ADD)
	{}

	ColorBlendEq (vk::VkBlendFactor	srcColorBlendFactor_,
				  vk::VkBlendFactor	dstColorBlendFactor_,
				  vk::VkBlendOp		colorBlendOp_,
				  vk::VkBlendFactor	srcAlphaBlendFactor_,
				  vk::VkBlendFactor	dstAlphaBlendFactor_,
				  vk::VkBlendOp		alphaBlendOp_)
		: srcColorBlendFactor	(srcColorBlendFactor_)
		, dstColorBlendFactor	(dstColorBlendFactor_)
		, colorBlendOp			(colorBlendOp_)
		, srcAlphaBlendFactor	(srcAlphaBlendFactor_)
		, dstAlphaBlendFactor	(dstAlphaBlendFactor_)
		, alphaBlendOp			(alphaBlendOp_)
	{
		if (isAdvancedBlendOp(colorBlendOp))
			DE_ASSERT(colorBlendOp == alphaBlendOp);
	}

	bool isAdvanced () const
	{
		return isAdvancedBlendOp(colorBlendOp);
	}
};

const DepthBiasParams kNoDepthBiasParams = { 0.0f, 0.0f };

struct LineStippleParams
{
	uint32_t factor;
	uint16_t pattern;
};

enum class LineRasterizationMode
{
	NONE = 0,
	RECTANGULAR,
	BRESENHAM,
	SMOOTH,
};

using ViewportVec		= std::vector<vk::VkViewport>;
using ScissorVec		= std::vector<vk::VkRect2D>;
using StencilOpVec		= std::vector<StencilOpParams>;
using SampleMaskVec		= std::vector<vk::VkSampleMask>;
using OptRastStream		= tcu::Maybe<uint32_t>;
using OptBoolean		= tcu::Maybe<bool>;
using OptStippleParams	= tcu::Maybe<LineStippleParams>;
using OptLineRasterMode	= tcu::Maybe<LineRasterizationMode>;
using OptSampleCount	= tcu::Maybe<vk::VkSampleCountFlagBits>;
using CovModTableVec	= std::vector<float>;
using BlendConstArray	= std::array<float, 4>;
using DepthBoundsParams	= std::pair<float, float>;
#ifndef CTS_USES_VULKANSC
using ViewportSwzVec	= std::vector<vk::VkViewportSwizzleNV>;
using OptDepthBiasRepr	= tcu::Maybe<vk::VkDepthBiasRepresentationInfoEXT>;
#endif // CTS_USES_VULKANSC

// Generic, to be used with any state than can be set statically and, as an option, dynamically.
template<typename T>
struct StaticAndDynamicPair
{
	T				staticValue;
	tcu::Maybe<T>	dynamicValue;

	// Helper constructor to set a static value and no dynamic value.
	StaticAndDynamicPair (const T& value)
		: staticValue	(value)
		, dynamicValue	(tcu::Nothing)
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
using StrideConfig					= StaticAndDynamicPair<StrideVec>;		// At least one element.
using DepthTestEnableConfig			= BooleanFlagConfig;
using DepthWriteEnableConfig		= BooleanFlagConfig;
using DepthCompareOpConfig			= StaticAndDynamicPair<vk::VkCompareOp>;
using DepthBoundsTestEnableConfig	= BooleanFlagConfig;
using DepthBoundsConfig				= StaticAndDynamicPair<DepthBoundsParams>;
using StencilTestEnableConfig		= BooleanFlagConfig;
using StencilOpConfig				= StaticAndDynamicPair<StencilOpVec>;	// At least one element.
using VertexGeneratorConfig			= StaticAndDynamicPair<const VertexGenerator*>;
using DepthBiasEnableConfig			= BooleanFlagConfig;
using RastDiscardEnableConfig		= BooleanFlagConfig;
using PrimRestartEnableConfig		= BooleanFlagConfig;
using LogicOpConfig					= StaticAndDynamicPair<vk::VkLogicOp>;
using PatchControlPointsConfig		= StaticAndDynamicPair<deUint8>;
using DepthBiasConfig				= StaticAndDynamicPair<DepthBiasParams>;
using TessDomainOriginConfig		= StaticAndDynamicPair<vk::VkTessellationDomainOrigin>;
using DepthClampEnableConfig		= BooleanFlagConfig;
using PolygonModeConfig				= StaticAndDynamicPair<vk::VkPolygonMode>;
using SampleMaskConfig				= StaticAndDynamicPair<SampleMaskVec>;
using AlphaToCoverageConfig			= BooleanFlagConfig;
using AlphaToOneConfig				= BooleanFlagConfig;
using ColorWriteEnableConfig		= BooleanFlagConfig;
using ColorWriteMaskConfig			= StaticAndDynamicPair<vk::VkColorComponentFlags>;
using RasterizationStreamConfig		= StaticAndDynamicPair<OptRastStream>;
using LogicOpEnableConfig			= BooleanFlagConfig;
using ColorBlendEnableConfig		= BooleanFlagConfig;
using ColorBlendEquationConfig		= StaticAndDynamicPair<ColorBlendEq>;
using BlendConstantsConfig			= StaticAndDynamicPair<BlendConstArray>;
using ProvokingVertexConfig			= StaticAndDynamicPair<OptBoolean>;	// First vertex boolean flag.
using NegativeOneToOneConfig		= StaticAndDynamicPair<OptBoolean>;
using DepthClipEnableConfig			= StaticAndDynamicPair<OptBoolean>;
using LineStippleEnableConfig		= BooleanFlagConfig;
using LineStippleParamsConfig		= StaticAndDynamicPair<OptStippleParams>;
using SampleLocationsEnableConfig	= BooleanFlagConfig;
using ConservativeRasterModeConfig	= StaticAndDynamicPair<vk::VkConservativeRasterizationModeEXT>;
using ExtraPrimitiveOverEstConfig	= StaticAndDynamicPair<float>; // Negative numbers will mean we're not interested in setting it.
using LineRasterModeConfig			= StaticAndDynamicPair<OptLineRasterMode>;
using CoverageToColorEnableConfig	= BooleanFlagConfig;
using CoverageToColorLocationConfig	= StaticAndDynamicPair<uint32_t>;
using RasterizationSamplesConfig	= StaticAndDynamicPair<vk::VkSampleCountFlagBits>;
using LineWidthConfig				= StaticAndDynamicPair<float>;
#ifndef CTS_USES_VULKANSC
using CoverageModulationModeConfig	= StaticAndDynamicPair<vk::VkCoverageModulationModeNV>;
using CoverageModTableEnableConfig	= BooleanFlagConfig;
using CoverageModTableConfig		= StaticAndDynamicPair<CovModTableVec>;
using CoverageReductionModeConfig	= StaticAndDynamicPair<vk::VkCoverageReductionModeNV>;
using ViewportSwizzleConfig			= StaticAndDynamicPair<ViewportSwzVec>;
using ShadingRateImageEnableConfig	= BooleanFlagConfig;
using ViewportWScalingEnableConfig	= BooleanFlagConfig;
using ReprFragTestEnableConfig		= BooleanFlagConfig;
#endif // CTS_USES_VULKANSC

const tcu::Vec4		kDefaultTriangleColor	(0.0f, 0.0f, 1.0f, 1.0f);	// Opaque blue.
const tcu::Vec4		kDefaultClearColor		(0.0f, 0.0f, 0.0f, 1.0f);	// Opaque black.
const tcu::Vec4		kTransparentColor		(0.0f, 0.0f, 1.0f, 0.0f);	// Transparent version of kDefaultTriangleColor.
const tcu::Vec4		kTransparentClearColor	(0.0f, 0.0f, 0.0f, 0.0f);	// Transparent version of kDefaultClearColor.
const tcu::Vec4		kOpaqueWhite			(1.0f, 1.0f, 1.0f, 1.0f);	// Opaque white, all components active.

const tcu::UVec4	kLogicOpTriangleColor	(  0u,   0u, 255u, 255u);	// Opaque blue.
const tcu::UVec4	kGreenClearColor		(  0u, 255u,   0u, 255u);	// Opaque green, UINT.
const tcu::UVec4	kLogicOpFinalColor		(  0u, 255u, 255u, 255u);	// Opaque cyan, UINT.

// Same as kLogicOpTriangleColor. Note: tcu::Vec4 and will be cast to the appropriate type in the shader.
const tcu::Vec4 kLogicOpTriangleColorFl (static_cast<float>(kLogicOpTriangleColor.x()),
										 static_cast<float>(kLogicOpTriangleColor.y()),
										 static_cast<float>(kLogicOpTriangleColor.w()),
										 static_cast<float>(kLogicOpTriangleColor.z()));

struct MeshParams
{
	tcu::Vec4	color;
	float		depth;
	bool		reversed;
	float		scaleX;
	float		scaleY;
	float		offsetX;
	float		offsetY;
	float		stripScale;

	MeshParams (const tcu::Vec4&	color_		= kDefaultTriangleColor,
				float				depth_		= 0.0f,
				bool				reversed_	= false,
				float				scaleX_		= 1.0f,
				float				scaleY_		= 1.0f,
				float				offsetX_	= 0.0f,
				float				offsetY_	= 0.0f,
				float				stripScale_	= 0.0f)
		: color			(color_)
		, depth			(depth_)
		, reversed		(reversed_)
		, scaleX		(scaleX_)
		, scaleY		(scaleY_)
		, offsetX		(offsetX_)
		, offsetY		(offsetY_)
		, stripScale	(stripScale_)
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

// This is used when generating some test cases.
enum class ColorBlendSubCase
{
	EQ_ONLY		= 0,	// Only the equation is dynamic.
	ALL_CB		= 1,	// All color blending states are dynamic.
	ALL_BUT_LO	= 2,	// All color blending states are dynamic, except for the ones related to logic op.
};

class ReferenceColorGenerator
{
public:
	typedef std::unique_ptr<ReferenceColorGenerator> P;

	virtual void	operator()	(tcu::PixelBufferAccess&)	const = 0;
	virtual P		clone		()							const = 0;
};

using ColorVerificator = std::function<bool(const tcu::ConstPixelBufferAccess&/*result*/, const tcu::ConstPixelBufferAccess&/*reference*/, const tcu::PixelBufferAccess&/*errorMask*/)>;

// Most tests expect a single output color in the whole image.
class SingleColorGenerator : public ReferenceColorGenerator
{
public:
	SingleColorGenerator (const tcu::Vec4& color)
		: m_colorFloat	(color)
		, m_colorUint	(0u)
		, isUint		(false)
	{}

	SingleColorGenerator (const tcu::UVec4& color)
		: m_colorFloat	(0.0f)
		, m_colorUint	(color)
		, isUint		(true)
	{}

	void operator()(tcu::PixelBufferAccess& access) const override
	{
		const auto kWidth	= access.getWidth();
		const auto kHeight	= access.getHeight();

		for (int y = 0; y < kHeight; ++y)
			for (int x = 0; x < kWidth; ++x)
			{
				if (isUint)
					access.setPixel(m_colorUint, x, y);
				else
					access.setPixel(m_colorFloat, x, y);
			}
	}

	P clone() const override
	{
		return P(new SingleColorGenerator(*this));
	}

private:
	const tcu::Vec4		m_colorFloat;
	const tcu::UVec4	m_colorUint;
	const bool			isUint;
};

// Some tests expect the upper half and the lower half having different color values.
class HorizontalSplitGenerator : public ReferenceColorGenerator
{
public:
	HorizontalSplitGenerator (const tcu::Vec4& top, const tcu::Vec4& bottom)
		: m_top(top), m_bottom(bottom)
	{}

	void operator()(tcu::PixelBufferAccess& access) const override
	{
		const auto kWidth		= access.getWidth();
		const auto kHeight		= access.getHeight();
		const auto kHalfHeight	= kHeight / 2;

		for (int y = 0; y < kHeight; ++y)
			for (int x = 0; x < kWidth; ++x)
			{
				const auto& color = (y < kHalfHeight ? m_top : m_bottom);
				access.setPixel(color, x, y);
			}
	}

	P clone() const override
	{
		return P(new HorizontalSplitGenerator(*this));
	}

private:
	const tcu::Vec4 m_top;
	const tcu::Vec4 m_bottom;
};

// Primitive restart tests expect the last line to have some missing pixels.
class LastSegmentMissingGenerator : public ReferenceColorGenerator
{
public:
	LastSegmentMissingGenerator (const tcu::Vec4& geomColor, const tcu::Vec4& clearColor)
		: m_geomColor	(geomColor)
		, m_clearColor	(clearColor)
	{}

	void operator()(tcu::PixelBufferAccess& access) const override
	{
		constexpr auto kWidth				= static_cast<int>(kFramebufferWidth);
		constexpr auto kHeight				= static_cast<int>(kFramebufferHeight);
		constexpr auto kLastSegmentStart	= static_cast<int>(kWidth * 0.75f);

		for (int y = 0; y < kHeight; ++y)
		for (int x = 0; x < kWidth; ++x)
		{
			// The last segment of the last line has the background color.
			const auto& color = ((y == kHeight - 1 && x >= kLastSegmentStart) ? m_clearColor : m_geomColor);
			access.setPixel(color, x, y);
		}
	}

	P clone() const override
	{
		return P(new LastSegmentMissingGenerator(*this));
	}

private:
	const tcu::Vec4 m_geomColor;
	const tcu::Vec4 m_clearColor;
};

// Some tests (like stippled line tests) expect vertical stripes of a given width.
class VerticalStripesGenerator: public ReferenceColorGenerator
{
public:
	VerticalStripesGenerator (const tcu::Vec4& left, const tcu::Vec4& right, uint32_t width)
		: m_left(left), m_right(right), m_width(width)
	{
		DE_ASSERT(width > 0 && width <= static_cast<uint32_t>(std::numeric_limits<int>::max()));
	}

	void operator()(tcu::PixelBufferAccess& access) const override
	{
		constexpr auto kWidth		= static_cast<int>(kFramebufferWidth);
		constexpr auto kHeight		= static_cast<int>(kFramebufferHeight);

		for (int y = 0; y < kHeight; ++y)
			for (int x = 0; x < kWidth; ++x)
			{
				const int	stripeIdx	= x / static_cast<int>(m_width);
				const auto&	color		= ((stripeIdx % 2 == 0) ? m_left : m_right);
				access.setPixel(color, x, y);
			}
	}

	P clone() const override
	{
		return P(new VerticalStripesGenerator(*this));
	}

private:
	const tcu::Vec4	m_left;
	const tcu::Vec4	m_right;
	const uint32_t	m_width;
};

// Some tests may expect a center strip in the framebuffer having a different color.
class CenterStripGenerator : public ReferenceColorGenerator
{
public:
	CenterStripGenerator (const tcu::Vec4& sides, const tcu::Vec4& center)
		: m_sides	(sides)
		, m_center	(center)
		{}

	void operator()(tcu::PixelBufferAccess& access) const override
	{
		constexpr auto kWidth		= static_cast<int>(kFramebufferWidth);
		constexpr auto kHeight		= static_cast<int>(kFramebufferHeight);

		for (int y = 0; y < kHeight; ++y)
			for (int x = 0; x < kWidth; ++x)
			{
				const auto& color = ((x >= kWidth / 4 && x < (kWidth * 3) / 4) ? m_center : m_sides);
				access.setPixel(color, x, y);
			}
	}

	P clone() const override
	{
		return P(new CenterStripGenerator(*this));
	}

private:
	const tcu::Vec4 m_sides;
	const tcu::Vec4 m_center;
};

// Tests using an off-center triangle may want this generator: fill the image with a solid color but leave the top and left edges in
// a different color.
class TopLeftBorderGenerator : public ReferenceColorGenerator
{
public:
	TopLeftBorderGenerator (const tcu::Vec4& mainColor, const tcu::Vec4& borderLeft, const tcu::Vec4& corner, const tcu::Vec4& borderTop)
		: m_mainColor	(mainColor)
		, m_borderLeft	(borderLeft)
		, m_corner		(corner)
		, m_borderTop	(borderTop)
	{}

	void operator()(tcu::PixelBufferAccess& access) const override
	{
		const auto kWidth		= access.getWidth();
		const auto kHeight		= access.getHeight();

		for (int y = 0; y < kHeight; ++y)
			for (int x = 0; x < kWidth; ++x)
			{
				tcu::Vec4 color;

				if (x == 0)
				{
					if (y == 0)
						color = m_corner;
					else
						color = m_borderLeft;
				}
				else if (y == 0)
					color = m_borderTop;
				else
					color = m_mainColor;

				access.setPixel(color, x, y);
			}
	}

	P clone() const override
	{
		return P(new TopLeftBorderGenerator(*this));
	}

private:
	const tcu::Vec4 m_mainColor;
	const tcu::Vec4 m_borderLeft;
	const tcu::Vec4 m_corner;
	const tcu::Vec4 m_borderTop;
};

tcu::Vec3 removeAlpha (const tcu::Vec4& color)
{
	const tcu::Vec3 rgb (color.x(), color.y(), color.z());
	return rgb;
}

// Verifies the top left pixel matches exactly.
bool verifyTopLeftCorner (const tcu::ConstPixelBufferAccess& result, const tcu::ConstPixelBufferAccess& reference, const tcu::PixelBufferAccess& errorMask, bool partialAlpha)
{
	// Check corner.
	const auto resultColor		= result.getPixel(0, 0);
	const auto referenceColor	= reference.getPixel(0, 0);

	const auto resultColorRGB		= removeAlpha(resultColor);
	const auto referenceColorRGB	= removeAlpha(referenceColor);

	const auto red			= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	const auto green		= tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	const auto black		= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const bool alphaMatch	= (partialAlpha ? (resultColor.w() > 0.0f && resultColor.w() < 1.0f) : (resultColor.w() == referenceColor.w()));
	const bool match		= ((resultColorRGB == referenceColorRGB) && alphaMatch);

	tcu::clear(errorMask, black);
	errorMask.setPixel((match ? green : red), 0, 0);

	return match;
}

bool verifyTopLeftCornerExactly (const tcu::ConstPixelBufferAccess& result, const tcu::ConstPixelBufferAccess& reference, const tcu::PixelBufferAccess& errorMask)
{
	return verifyTopLeftCorner(result, reference, errorMask, false/*partialAlpha*/);
}

bool verifyTopLeftCornerWithPartialAlpha (const tcu::ConstPixelBufferAccess& result, const tcu::ConstPixelBufferAccess& reference, const tcu::PixelBufferAccess& errorMask)
{
	return verifyTopLeftCorner(result, reference, errorMask, true/*partialAlpha*/);
}

const VertexGenerator* getVertexWithPaddingGenerator ()
{
	static VertexWithPadding vertexWithPadding;
	return &vertexWithPadding;
}

const VertexGenerator* getVertexWithPadding16Generator ()
{
	static VertexWithPadding16 vertexWithPadding16;
	return &vertexWithPadding16;
}

const VertexGenerator* getVertexWithExtraAttributesGenerator ()
{
	static VertexWithExtraAttributes vertexWithExtraAttributes;
	return &vertexWithExtraAttributes;
}

const VertexGenerator* getVertexWithMultipleBindingsGenerator ()
{
	static MultipleBindingsVertex multipleBindingsVertex;
	return &multipleBindingsVertex;
}

const VertexGenerator* getProvokingVertexWithPaddingGenerator (bool lastVertex)
{
	if (lastVertex)
	{
		static ProvokingVertexWithPadding provokingVertexGeneratorLastVtx (true);
		return &provokingVertexGeneratorLastVtx;
	}
	static ProvokingVertexWithPadding provokingVertexGeneratorFirstVtx (false);
	return &provokingVertexGeneratorFirstVtx;
}

const VertexGenerator* getVertexWithInstanceDataGenerator ()
{
	static VertexWithInstanceData vertexWithInstanceData;
	return &vertexWithInstanceData;
}

// Create VertexGeneratorConfig varying constructor depending on having none, only the static or both.
VertexGeneratorConfig makeVertexGeneratorConfig (const VertexGenerator* staticGen, const VertexGenerator* dynamicGen)
{
	DE_ASSERT(!(dynamicGen && !staticGen));
	if (dynamicGen)
		return VertexGeneratorConfig(staticGen, dynamicGen);
	if (staticGen)
		return VertexGeneratorConfig(staticGen);
	return VertexGeneratorConfig(getVertexWithPaddingGenerator());	// Only static part with a default option.
}

// Similar to makeVertexGeneratorConfig, choosing the final value.
const VertexGenerator* chooseVertexGenerator (const VertexGenerator* staticGen, const VertexGenerator* dynamicGen)
{
	DE_ASSERT(!(dynamicGen && !staticGen));
	if (dynamicGen)
		return dynamicGen;
	if (staticGen)
		return staticGen;
	return getVertexWithPaddingGenerator();
}

#ifndef CTS_USES_VULKANSC
// Is a particular dynamic state incompatible with mesh shading pipelines?
bool isMeshShadingPipelineIncompatible (vk::VkDynamicState state)
{
	switch (state)
	{
	case vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT:
	case vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT:
	case vk::VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT:
	case vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT:
	case vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT:
		return true;
	default:
		return false;
	}

	// Unreachable.
	DE_ASSERT(false);
	return false;
}

// Is a particular dynamic state compatible with mesh shading pipelines?
bool isMeshShadingPipelineCompatible (vk::VkDynamicState state)
{
	return !isMeshShadingPipelineIncompatible(state);
}
#endif // CTS_USES_VULKANSC

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

LineRasterizationMode selectLineRasterizationMode (const vk::VkPhysicalDeviceLineRasterizationFeaturesEXT& lineRasterFeatures, bool stippleRequired, const tcu::Maybe<LineRasterizationMode>& pref)
{
	LineRasterizationMode	selectedMode	= LineRasterizationMode::NONE;
	const bool				hasPref			= static_cast<bool>(pref);

	if ((!hasPref || pref.get() == LineRasterizationMode::RECTANGULAR) && lineRasterFeatures.rectangularLines && (!stippleRequired || lineRasterFeatures.stippledRectangularLines))
		selectedMode = LineRasterizationMode::RECTANGULAR;
	else if ((!hasPref || pref.get() == LineRasterizationMode::BRESENHAM) && lineRasterFeatures.bresenhamLines && (!stippleRequired || lineRasterFeatures.stippledBresenhamLines))
		selectedMode = LineRasterizationMode::BRESENHAM;
	else if ((!hasPref || pref.get() == LineRasterizationMode::SMOOTH) && lineRasterFeatures.smoothLines && (!stippleRequired || lineRasterFeatures.stippledSmoothLines))
		selectedMode = LineRasterizationMode::SMOOTH;

	return selectedMode;
}

vk::VkLineRasterizationModeEXT makeLineRasterizationMode (LineRasterizationMode mode)
{
	vk::VkLineRasterizationModeEXT modeEXT = vk::VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT;

	switch (mode)
	{
	case LineRasterizationMode::RECTANGULAR:	modeEXT = vk::VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT;			break;
	case LineRasterizationMode::BRESENHAM:		modeEXT = vk::VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;				break;
	case LineRasterizationMode::SMOOTH:			modeEXT = vk::VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT;	break;
	default:
		DE_ASSERT(false);
		break;
	}

	return modeEXT;
}

struct TestConfig
{
	// Should we use pipeline_library to construct pipeline.
	vk::PipelineConstructionType	pipelineConstructionType;

	// Main sequence ordering.
	SequenceOrdering				sequenceOrdering;

	// Drawing parameters: tests will draw one or more flat meshes of triangles covering the whole "screen".
	std::vector<MeshParams>			meshParams;			// Mesh parameters for each full-screen layer of geometry.
	deUint32						referenceStencil;	// Reference stencil value.

	// Clearing parameters for the framebuffer.
	vk::VkClearValue				clearColorValue;
	float							clearDepthValue;
	deUint32						clearStencilValue;

	// Expected output in the attachments.
	ReferenceColorGenerator::P		referenceColor;
	float							expectedDepth;
	deUint32						expectedStencil;

	// Optional verification routine.
	tcu::Maybe<ColorVerificator>	colorVerificator;

	// Force inclusion of passthrough geometry shader or not.
	bool							forceGeometryShader;

	// Use mesh shaders instead of classic pipelines.
	bool							useMeshShaders;

	// Bind an unused mesh shading pipeline before binding the dynamic pipeline.
	// This will only be used in the CMD_BUFFER_START sequence ordering, to minimize the number of cases.
	bool							bindUnusedMeshShadingPipeline;

	// Force single vertex in the VBO.
	bool							singleVertex;
	deUint32						singleVertexDrawCount;

	// Force using an oversized triangle as the mesh.
	bool							oversizedTriangle;

	// Force using a single triangle with a small offset as the mesh.
	bool							offCenterTriangle;
	tcu::Vec2						offCenterProportion; // Relative to pixel size.

	// Force using a single oblique line: this helps test line rasterization mode.
	bool							obliqueLine;

	// Offset and extra room after the vertex buffer data.
	vk::VkDeviceSize				vertexDataOffset;
	vk::VkDeviceSize				vertexDataExtraBytes;

	// Bind and draw with a pipeline that uses dynamic patch control points but doesn't actually use a tessellation
	// shader, before using the real pipelines being tested.
	bool							useExtraDynPCPPipeline;
	// Bind and draw with a pipeline that uses same dynamic states, before using the real pipelines being tested.
	bool							useExtraDynPipeline;

	// Optional, to be used specifically for color attachments when testing coverage modulation and reduction.
	bool							coverageModulation;
	bool							coverageReduction;
	OptSampleCount					colorSampleCount;

	// Rasterization stream, if needed, used in the geometry shader.
	OptRastStream					shaderRasterizationStream;

	// Sample locations, which may be used if testing sample locations.
	tcu::Vec2						sampleLocations;

	// Optional maximum value for primitiveOverestimationSize so the test works properly.
	tcu::Maybe<float>				maxPrimitiveOverestimationSize;

	// Number of color attachments in the subpass. Note the fragment shader will only write to the last one.
	uint32_t						colorAttachmentCount;

	// Instance count.
	uint32_t						instanceCount;

	// Use viewport swizzle or not.
	bool							viewportSwizzle;

	// Use shading rate image configuration or not.
	bool							shadingRateImage;

	// Use viewport W scaling or not.
	bool							viewportWScaling;

	// Use representative fragment test or not.
	bool							representativeFragmentTest;

	// Insert extra indices for restarting lines.
	bool							extraLineRestarts;

	// Consider both the basic and advanced color blend states dynamic if any of them is dynamic.
	bool							colorBlendBoth;

	// Use color write enable state.
	bool							useColorWriteEnable;

	// Force UNORM color format.
	bool							forceUnormColorFormat;

	// Used in some tests to verify color blend pAttachments can be null if all its state is dynamic.
	bool							nullStaticColorBlendAttPtr;

	// Use dual source blending.
	bool							dualSrcBlend;

	// Use null pointers when possible for static state.
	bool							favorStaticNullPointers;

	// Force using atomic counters in the frag shader to count frag shader invocations.
	bool							forceAtomicCounters;

	// When setting the sample mask dynamically, we can use an alternative sample count specified here.
	OptSampleCount					dynamicSampleMaskCount;

#ifndef CTS_USES_VULKANSC
	// This structure is optional and can be included statically in the rasterization info or dynamically in vkCmdSetDepthBias2.
	OptDepthBiasRepr				depthBiasReprInfo;
#endif // CTS_USES_VULKANSC

	tcu::TextureChannelClass		neededDepthChannelClass;
	float							extraDepthThreshold;

	// Static values for sampleShadingEnable and minSampleShading.
	bool							sampleShadingEnable;
	float							minSampleShading;

	// Static and dynamic pipeline configuration.
	VertexGeneratorConfig			vertexGenerator;
	CullModeConfig					cullModeConfig;
	FrontFaceConfig					frontFaceConfig;
	TopologyConfig					topologyConfig;
	ViewportConfig					viewportConfig;
	ScissorConfig					scissorConfig;
	StrideConfig					strideConfig;
	DepthTestEnableConfig			depthTestEnableConfig;
	DepthWriteEnableConfig			depthWriteEnableConfig;
	DepthCompareOpConfig			depthCompareOpConfig;
	DepthBoundsTestEnableConfig		depthBoundsTestEnableConfig;
	DepthBoundsConfig				depthBoundsConfig;
	StencilTestEnableConfig			stencilTestEnableConfig;
	StencilOpConfig					stencilOpConfig;
	DepthBiasEnableConfig			depthBiasEnableConfig;
	RastDiscardEnableConfig			rastDiscardEnableConfig;
	PrimRestartEnableConfig			primRestartEnableConfig;
	LogicOpConfig					logicOpConfig;
	PatchControlPointsConfig		patchControlPointsConfig;
	DepthBiasConfig					depthBiasConfig;
	TessDomainOriginConfig			tessDomainOriginConfig;
	DepthClampEnableConfig			depthClampEnableConfig;
	PolygonModeConfig				polygonModeConfig;
	SampleMaskConfig				sampleMaskConfig;
	AlphaToCoverageConfig			alphaToCoverageConfig;
	AlphaToOneConfig				alphaToOneConfig;
	ColorWriteEnableConfig			colorWriteEnableConfig;
	ColorWriteMaskConfig			colorWriteMaskConfig;
	RasterizationStreamConfig		rasterizationStreamConfig;
	LogicOpEnableConfig				logicOpEnableConfig;
	ColorBlendEnableConfig			colorBlendEnableConfig;
	ColorBlendEquationConfig		colorBlendEquationConfig;
	BlendConstantsConfig			blendConstantsConfig;
	ProvokingVertexConfig			provokingVertexConfig;
	NegativeOneToOneConfig			negativeOneToOneConfig;
	DepthClipEnableConfig			depthClipEnableConfig;
	LineStippleEnableConfig			lineStippleEnableConfig;
	LineStippleParamsConfig			lineStippleParamsConfig;
	SampleLocationsEnableConfig		sampleLocationsEnableConfig;
	ConservativeRasterModeConfig	conservativeRasterModeConfig;
	ExtraPrimitiveOverEstConfig		extraPrimitiveOverEstConfig;
	LineRasterModeConfig			lineRasterModeConfig;
	CoverageToColorEnableConfig		coverageToColorEnableConfig;
	CoverageToColorLocationConfig	coverageToColorLocationConfig;
	RasterizationSamplesConfig		rasterizationSamplesConfig;
	LineWidthConfig					lineWidthConfig;
#ifndef CTS_USES_VULKANSC
	CoverageModulationModeConfig	coverageModulationModeConfig;
	CoverageModTableEnableConfig	coverageModTableEnableConfig;
	CoverageModTableConfig			coverageModTableConfig;
	CoverageReductionModeConfig		coverageReductionModeConfig;
	ViewportSwizzleConfig			viewportSwizzleConfig;
	ShadingRateImageEnableConfig	shadingRateImageEnableConfig;
	ViewportWScalingEnableConfig	viewportWScalingEnableConfig;
	ReprFragTestEnableConfig		reprFragTestEnableConfig;
#endif // CTS_USES_VULKANSC

	// Sane defaults.
	TestConfig (vk::PipelineConstructionType pipelineType, SequenceOrdering ordering, bool useMeshShaders_, const VertexGenerator* staticVertexGenerator = nullptr, const VertexGenerator* dynamicVertexGenerator = nullptr)
		: pipelineConstructionType		(pipelineType)
		, sequenceOrdering				(ordering)
		, meshParams					(1u, MeshParams())
		, referenceStencil				(0u)
		, clearColorValue				(vk::makeClearValueColor(kDefaultClearColor))
		, clearDepthValue				(1.0f)
		, clearStencilValue				(0u)
		, referenceColor				(new SingleColorGenerator(kDefaultTriangleColor))
		, expectedDepth					(1.0f)
		, expectedStencil				(0u)
		, colorVerificator				(tcu::Nothing)
		, forceGeometryShader			(false)
		, useMeshShaders				(useMeshShaders_)
		, bindUnusedMeshShadingPipeline	(false)
		, singleVertex					(false)
		, singleVertexDrawCount			(0)
		, oversizedTriangle				(false)
		, offCenterTriangle				(false)
		, offCenterProportion			(0.0f, 0.0f)
		, obliqueLine					(false)
		, vertexDataOffset				(0ull)
		, vertexDataExtraBytes			(0ull)
		, useExtraDynPCPPipeline		(false)
		, useExtraDynPipeline			(false)
		, coverageModulation			(false)
		, coverageReduction				(false)
		, colorSampleCount				(tcu::Nothing)
		, shaderRasterizationStream		(tcu::Nothing)
		, sampleLocations				(0.5f, 0.5f)
		, colorAttachmentCount			(1u)
		, instanceCount					(1u)
		, viewportSwizzle				(false)
		, shadingRateImage				(false)
		, viewportWScaling				(false)
		, representativeFragmentTest	(false)
		, extraLineRestarts				(false)
		, colorBlendBoth				(false)
		, useColorWriteEnable			(false)
		, forceUnormColorFormat			(false)
		, nullStaticColorBlendAttPtr	(false)
		, dualSrcBlend					(false)
		, favorStaticNullPointers		(false)
		, forceAtomicCounters			(false)
		, dynamicSampleMaskCount		(tcu::Nothing)
#ifndef CTS_USES_VULKANSC
		, depthBiasReprInfo				(tcu::Nothing)
#endif // CTS_USES_VULKANSC
		, neededDepthChannelClass		(tcu::TEXTURECHANNELCLASS_LAST)
		, extraDepthThreshold			(0.0f)
		, sampleShadingEnable			(false)
		, minSampleShading				(0.0f)
		, vertexGenerator				(makeVertexGeneratorConfig(staticVertexGenerator, dynamicVertexGenerator))
		, cullModeConfig				(static_cast<vk::VkCullModeFlags>(vk::VK_CULL_MODE_NONE))
		, frontFaceConfig				(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE)
		// By default we will use a triangle strip with 6 vertices that could be wrongly interpreted as a triangle list with 2 triangles.
		, topologyConfig				(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
		, viewportConfig				(ViewportVec(1u, vk::makeViewport(kFramebufferWidth, kFramebufferHeight)))
		, scissorConfig					(ScissorVec(1u, vk::makeRect2D(kFramebufferWidth, kFramebufferHeight)))
		// By default, the vertex stride is the size of a vertex according to the chosen vertex type.
		, strideConfig					(chooseVertexGenerator(staticVertexGenerator, dynamicVertexGenerator)->getVertexDataStrides())
		, depthTestEnableConfig			(false)
		, depthWriteEnableConfig		(false)
		, depthCompareOpConfig			(vk::VK_COMPARE_OP_NEVER)
		, depthBoundsTestEnableConfig	(false)
		, depthBoundsConfig				(std::make_pair(0.0f, 1.0f))
		, stencilTestEnableConfig		(false)
		, stencilOpConfig				(StencilOpVec(1u, kDefaultStencilOpParams))
		, depthBiasEnableConfig			(false)
		, rastDiscardEnableConfig		(false)
		, primRestartEnableConfig		(false)
		, logicOpConfig					(vk::VK_LOGIC_OP_CLEAR)
		, patchControlPointsConfig		(1u)
		, depthBiasConfig				(kNoDepthBiasParams)
		, tessDomainOriginConfig		(vk::VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT)
		, depthClampEnableConfig		(false)
		, polygonModeConfig				(vk::VK_POLYGON_MODE_FILL)
		, sampleMaskConfig				(SampleMaskVec())
		, alphaToCoverageConfig			(false)
		, alphaToOneConfig				(false)
		, colorWriteEnableConfig		(true)
		, colorWriteMaskConfig			(CR | CG | CB | CA)
		, rasterizationStreamConfig		(tcu::Nothing)
		, logicOpEnableConfig			(false)
		, colorBlendEnableConfig		(false)
		, colorBlendEquationConfig		(ColorBlendEq())
		, blendConstantsConfig			(BlendConstArray{0.0f, 0.0f, 0.0f, 0.0f})
		, provokingVertexConfig			(tcu::Nothing)
		, negativeOneToOneConfig		(tcu::Nothing)
		, depthClipEnableConfig			(tcu::Nothing)
		, lineStippleEnableConfig		(false)
		, lineStippleParamsConfig		(tcu::Nothing)
		, sampleLocationsEnableConfig	(false)
		, conservativeRasterModeConfig	(vk::VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT)
		, extraPrimitiveOverEstConfig	(-1.0f)
		, lineRasterModeConfig			(tcu::Nothing)
		, coverageToColorEnableConfig	(false)
		, coverageToColorLocationConfig	(0u)
		, rasterizationSamplesConfig	(kSingleSampleCount)
		, lineWidthConfig				(1.0f)
#ifndef CTS_USES_VULKANSC
		, coverageModulationModeConfig	(vk::VK_COVERAGE_MODULATION_MODE_NONE_NV)
		, coverageModTableEnableConfig	(false)
		, coverageModTableConfig		(CovModTableVec())
		, coverageReductionModeConfig	(vk::VK_COVERAGE_REDUCTION_MODE_MERGE_NV)
		, viewportSwizzleConfig			(ViewportSwzVec())
		, shadingRateImageEnableConfig	(false)
		, viewportWScalingEnableConfig	(false)
		, reprFragTestEnableConfig		(false)
#endif // CTS_USES_VULKANSC
		, m_swappedValues				(false)
	{
	}

	TestConfig (const TestConfig& other)
		: pipelineConstructionType		(other.pipelineConstructionType)
		, sequenceOrdering				(other.sequenceOrdering)
		, meshParams					(other.meshParams)
		, referenceStencil				(other.referenceStencil)
		, clearColorValue				(other.clearColorValue)
		, clearDepthValue				(other.clearDepthValue)
		, clearStencilValue				(other.clearStencilValue)
		, referenceColor				(other.referenceColor->clone())
		, expectedDepth					(other.expectedDepth)
		, expectedStencil				(other.expectedStencil)
		, colorVerificator				(other.colorVerificator)
		, forceGeometryShader			(other.forceGeometryShader)
		, useMeshShaders				(other.useMeshShaders)
		, bindUnusedMeshShadingPipeline	(other.bindUnusedMeshShadingPipeline)
		, singleVertex					(other.singleVertex)
		, singleVertexDrawCount			(other.singleVertexDrawCount)
		, oversizedTriangle				(other.oversizedTriangle)
		, offCenterTriangle				(other.offCenterTriangle)
		, offCenterProportion			(other.offCenterProportion)
		, obliqueLine					(other.obliqueLine)
		, vertexDataOffset				(other.vertexDataOffset)
		, vertexDataExtraBytes			(other.vertexDataExtraBytes)
		, useExtraDynPCPPipeline		(other.useExtraDynPCPPipeline)
		, useExtraDynPipeline			(other.useExtraDynPipeline)
		, coverageModulation			(other.coverageModulation)
		, coverageReduction				(other.coverageReduction)
		, colorSampleCount				(other.colorSampleCount)
		, shaderRasterizationStream		(other.shaderRasterizationStream)
		, sampleLocations				(other.sampleLocations)
		, colorAttachmentCount			(other.colorAttachmentCount)
		, instanceCount					(other.instanceCount)
		, viewportSwizzle				(other.viewportSwizzle)
		, shadingRateImage				(other.shadingRateImage)
		, viewportWScaling				(other.viewportWScaling)
		, representativeFragmentTest	(other.representativeFragmentTest)
		, extraLineRestarts				(other.extraLineRestarts)
		, colorBlendBoth				(other.colorBlendBoth)
		, useColorWriteEnable			(other.useColorWriteEnable)
		, forceUnormColorFormat			(other.forceUnormColorFormat)
		, nullStaticColorBlendAttPtr	(other.nullStaticColorBlendAttPtr)
		, dualSrcBlend					(other.dualSrcBlend)
		, favorStaticNullPointers		(other.favorStaticNullPointers)
		, forceAtomicCounters			(other.forceAtomicCounters)
		, dynamicSampleMaskCount		(other.dynamicSampleMaskCount)
#ifndef CTS_USES_VULKANSC
		, depthBiasReprInfo				(other.depthBiasReprInfo)
#endif // CTS_USES_VULKANSC
		, neededDepthChannelClass		(other.neededDepthChannelClass)
		, extraDepthThreshold			(other.extraDepthThreshold)
		, sampleShadingEnable			(other.sampleShadingEnable)
		, minSampleShading				(other.minSampleShading)
		, vertexGenerator				(other.vertexGenerator)
		, cullModeConfig				(other.cullModeConfig)
		, frontFaceConfig				(other.frontFaceConfig)
		, topologyConfig				(other.topologyConfig)
		, viewportConfig				(other.viewportConfig)
		, scissorConfig					(other.scissorConfig)
		, strideConfig					(other.strideConfig)
		, depthTestEnableConfig			(other.depthTestEnableConfig)
		, depthWriteEnableConfig		(other.depthWriteEnableConfig)
		, depthCompareOpConfig			(other.depthCompareOpConfig)
		, depthBoundsTestEnableConfig	(other.depthBoundsTestEnableConfig)
		, depthBoundsConfig				(other.depthBoundsConfig)
		, stencilTestEnableConfig		(other.stencilTestEnableConfig)
		, stencilOpConfig				(other.stencilOpConfig)
		, depthBiasEnableConfig			(other.depthBiasEnableConfig)
		, rastDiscardEnableConfig		(other.rastDiscardEnableConfig)
		, primRestartEnableConfig		(other.primRestartEnableConfig)
		, logicOpConfig					(other.logicOpConfig)
		, patchControlPointsConfig		(other.patchControlPointsConfig)
		, depthBiasConfig				(other.depthBiasConfig)
		, tessDomainOriginConfig		(other.tessDomainOriginConfig)
		, depthClampEnableConfig		(other.depthClampEnableConfig)
		, polygonModeConfig				(other.polygonModeConfig)
		, sampleMaskConfig				(other.sampleMaskConfig)
		, alphaToCoverageConfig			(other.alphaToCoverageConfig)
		, alphaToOneConfig				(other.alphaToOneConfig)
		, colorWriteEnableConfig		(other.colorWriteEnableConfig)
		, colorWriteMaskConfig			(other.colorWriteMaskConfig)
		, rasterizationStreamConfig		(other.rasterizationStreamConfig)
		, logicOpEnableConfig			(other.logicOpEnableConfig)
		, colorBlendEnableConfig		(other.colorBlendEnableConfig)
		, colorBlendEquationConfig		(other.colorBlendEquationConfig)
		, blendConstantsConfig			(other.blendConstantsConfig)
		, provokingVertexConfig			(other.provokingVertexConfig)
		, negativeOneToOneConfig		(other.negativeOneToOneConfig)
		, depthClipEnableConfig			(other.depthClipEnableConfig)
		, lineStippleEnableConfig		(other.lineStippleEnableConfig)
		, lineStippleParamsConfig		(other.lineStippleParamsConfig)
		, sampleLocationsEnableConfig	(other.sampleLocationsEnableConfig)
		, conservativeRasterModeConfig	(other.conservativeRasterModeConfig)
		, extraPrimitiveOverEstConfig	(other.extraPrimitiveOverEstConfig)
		, lineRasterModeConfig			(other.lineRasterModeConfig)
		, coverageToColorEnableConfig	(other.coverageToColorEnableConfig)
		, coverageToColorLocationConfig	(other.coverageToColorLocationConfig)
		, rasterizationSamplesConfig	(other.rasterizationSamplesConfig)
		, lineWidthConfig				(other.lineWidthConfig)
#ifndef CTS_USES_VULKANSC
		, coverageModulationModeConfig	(other.coverageModulationModeConfig)
		, coverageModTableEnableConfig	(other.coverageModTableEnableConfig)
		, coverageModTableConfig		(other.coverageModTableConfig)
		, coverageReductionModeConfig	(other.coverageReductionModeConfig)
		, viewportSwizzleConfig			(other.viewportSwizzleConfig)
		, shadingRateImageEnableConfig	(other.shadingRateImageEnableConfig)
		, viewportWScalingEnableConfig	(other.viewportWScalingEnableConfig)
		, reprFragTestEnableConfig		(other.reprFragTestEnableConfig)
#endif // CTS_USES_VULKANSC
		, m_swappedValues				(other.m_swappedValues)
	{
	}

	// Get the proper viewport vector according to the test config.
	const ViewportVec& getActiveViewportVec () const
	{
		return ((viewportConfig.dynamicValue && !m_swappedValues) ? viewportConfig.dynamicValue.get() : viewportConfig.staticValue);
	}

	// Gets the proper vertex generator according to the test config.
	const VertexGenerator* getActiveVertexGenerator () const
	{
		return ((vertexGenerator.dynamicValue && !m_swappedValues) ? vertexGenerator.dynamicValue.get() : vertexGenerator.staticValue);
	}

	// Gets the inactive vertex generator according to the test config. If there's only one, return that.
	const VertexGenerator* getInactiveVertexGenerator () const
	{
		return ((vertexGenerator.dynamicValue && m_swappedValues) ? vertexGenerator.dynamicValue.get() : vertexGenerator.staticValue);
	}

	// Get the active number of patch control points according to the test config.
	deUint32 getActivePatchControlPoints () const
	{
		return ((patchControlPointsConfig.dynamicValue && !m_swappedValues) ? patchControlPointsConfig.dynamicValue.get() : patchControlPointsConfig.staticValue);
	}

	// Get the active depth bias parameters.
	DepthBiasParams getActiveDepthBiasParams () const
	{
		return ((depthBiasConfig.dynamicValue && !m_swappedValues) ? depthBiasConfig.dynamicValue.get() : depthBiasConfig.staticValue);
	}

	vk::VkTessellationDomainOrigin getActiveTessellationDomainOrigin () const
	{
		return ((tessDomainOriginConfig.dynamicValue && !m_swappedValues) ? tessDomainOriginConfig.dynamicValue.get() : tessDomainOriginConfig.staticValue);
	}

	vk::VkPolygonMode getActivePolygonMode () const
	{
		return ((polygonModeConfig.dynamicValue && !m_swappedValues) ? polygonModeConfig.dynamicValue.get() : polygonModeConfig.staticValue);
	}

	vk::VkSampleCountFlagBits getActiveSampleCount () const
	{
		return ((rasterizationSamplesConfig.dynamicValue && !m_swappedValues) ? rasterizationSamplesConfig.dynamicValue.get() : rasterizationSamplesConfig.staticValue);
	}

	bool getActiveAlphaToOne () const
	{
		return ((alphaToOneConfig.dynamicValue && !m_swappedValues) ? alphaToOneConfig.dynamicValue.get() : alphaToOneConfig.staticValue);
	}

	bool rasterizationStreamStruct () const
	{
		return (static_cast<bool>(rasterizationStreamConfig.staticValue)
				|| (static_cast<bool>(rasterizationStreamConfig.dynamicValue) && static_cast<bool>(rasterizationStreamConfig.dynamicValue.get())));
	}

	bool provokingVertexStruct () const
	{
		return (static_cast<bool>(provokingVertexConfig.staticValue)
				|| (static_cast<bool>(provokingVertexConfig.dynamicValue) && static_cast<bool>(provokingVertexConfig.dynamicValue.get())));
	}

	bool negativeOneToOneStruct () const
	{
		return (static_cast<bool>(negativeOneToOneConfig.staticValue)
				|| (static_cast<bool>(negativeOneToOneConfig.dynamicValue) && static_cast<bool>(negativeOneToOneConfig.dynamicValue.get())));
	}

	bool depthClipEnableStruct () const
	{
		return (static_cast<bool>(depthClipEnableConfig.staticValue)
				|| (static_cast<bool>(depthClipEnableConfig.dynamicValue) && static_cast<bool>(depthClipEnableConfig.dynamicValue.get())));
	}

	bool hasStaticLineStippleParams () const
	{
		return (static_cast<bool>(lineStippleParamsConfig.staticValue));
	}

	bool hasStaticLineRasterMode () const
	{
		return (static_cast<bool>(lineRasterModeConfig.staticValue));
	}

	bool hasLineStippleParams () const
	{
		return (hasStaticLineStippleParams()
				|| (static_cast<bool>(lineStippleParamsConfig.dynamicValue) && static_cast<bool>(lineStippleParamsConfig.dynamicValue.get())));
	}

	bool hasLineRasterMode () const
	{
		return (hasStaticLineRasterMode()
				|| (static_cast<bool>(lineRasterModeConfig.dynamicValue) && static_cast<bool>(lineRasterModeConfig.dynamicValue.get())));
	}

	bool lineStippleSupportRequired () const
	{
		return (lineStippleEnableConfig.staticValue || (static_cast<bool>(lineStippleEnableConfig.dynamicValue) && lineStippleEnableConfig.dynamicValue.get()));
	}

	bool lineRasterStruct () const
	{
		return (static_cast<bool>(lineStippleEnableConfig.dynamicValue) || lineStippleEnableConfig.staticValue || hasStaticLineStippleParams() || hasStaticLineRasterMode());
	}

	bool lineRasterizationExt () const
	{
		return (lineRasterStruct() || hasLineStippleParams() || hasLineRasterMode());
	}

	bool sampleLocationsStruct () const
	{
		return (static_cast<bool>(sampleLocationsEnableConfig.dynamicValue) || sampleLocationsEnableConfig.staticValue);
	}

	bool coverageToColorStruct () const
	{
		return (static_cast<bool>(coverageToColorEnableConfig.dynamicValue) || coverageToColorEnableConfig.staticValue);
	}

	bool conservativeRasterStruct () const
	{
		return (static_cast<bool>(conservativeRasterModeConfig.dynamicValue) || conservativeRasterModeConfig.staticValue != vk::VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT
				|| static_cast<bool>(extraPrimitiveOverEstConfig.dynamicValue) || extraPrimitiveOverEstConfig.staticValue >= 0.0f);
	}

	vk::VkConservativeRasterizationModeEXT getActiveConservativeRasterMode () const
	{
		return ((static_cast<bool>(conservativeRasterModeConfig.dynamicValue) && !m_swappedValues) ? conservativeRasterModeConfig.dynamicValue.get() : conservativeRasterModeConfig.staticValue);
	}

	float getActiveExtraPrimitiveOverEstSize () const
	{
		return ((static_cast<bool>(extraPrimitiveOverEstConfig.dynamicValue) && !m_swappedValues) ? extraPrimitiveOverEstConfig.dynamicValue.get() : extraPrimitiveOverEstConfig.staticValue);
	}

	bool getActiveNegativeOneToOneValue () const
	{
		const bool				staticValue		= (static_cast<bool>(negativeOneToOneConfig.staticValue) ? negativeOneToOneConfig.staticValue.get() : false);
		const bool				hasDynamicValue	= (static_cast<bool>(negativeOneToOneConfig.dynamicValue) && static_cast<bool>(negativeOneToOneConfig.dynamicValue.get()));
		const tcu::Maybe<bool>	dynamicValue	= (hasDynamicValue ? tcu::just(negativeOneToOneConfig.dynamicValue->get()) : tcu::nothing<bool>());

		return ((hasDynamicValue && !m_swappedValues) ? dynamicValue.get() : staticValue);
	}

	bool getActiveDepthClipEnable () const
	{
		const bool				staticValue		= (static_cast<bool>(depthClipEnableConfig.staticValue) ? depthClipEnableConfig.staticValue.get() : true);
		const bool				hasDynamicValue	= (static_cast<bool>(depthClipEnableConfig.dynamicValue) && static_cast<bool>(depthClipEnableConfig.dynamicValue.get()));
		const tcu::Maybe<bool>	dynamicValue	= (hasDynamicValue ? tcu::just(depthClipEnableConfig.dynamicValue->get()) : tcu::nothing<bool>());

		return ((hasDynamicValue && !m_swappedValues) ? dynamicValue.get() : staticValue);
	}

	float getActiveLineWidth () const
	{
		return ((static_cast<bool>(lineWidthConfig.dynamicValue) && !m_swappedValues) ? lineWidthConfig.dynamicValue.get() : lineWidthConfig.staticValue);
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
		return ((isMultiViewport() && (!useMeshShaders)) || forceGeometryShader || static_cast<bool>(shaderRasterizationStream));
	}

	// Returns true if we should use the static and dynamic values exchanged.
	// This makes the static part of the pipeline have the actual expected values.
	bool isReversed () const
	{
		return (sequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				sequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC);
	}

	// Returns true if the ordering needs to bind a static pipeline first.
	bool bindStaticFirst () const
	{
		return (sequenceOrdering == SequenceOrdering::BETWEEN_PIPELINES	||
				sequenceOrdering == SequenceOrdering::AFTER_PIPELINES	||
				sequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC);
	}

	// Returns true if the test uses a static pipeline.
	bool useStaticPipeline () const
	{
		return (bindStaticFirst() || isReversed());
	}

	// Swaps static and dynamic configuration values.
	void swapValues ()
	{
		vertexGenerator.swapValues();
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
		depthBoundsConfig.swapValues();
		stencilTestEnableConfig.swapValues();
		stencilOpConfig.swapValues();
		depthBiasEnableConfig.swapValues();
		rastDiscardEnableConfig.swapValues();
		primRestartEnableConfig.swapValues();
		logicOpConfig.swapValues();
		patchControlPointsConfig.swapValues();
		depthBiasConfig.swapValues();
		tessDomainOriginConfig.swapValues();
		depthClampEnableConfig.swapValues();
		polygonModeConfig.swapValues();
		sampleMaskConfig.swapValues();
		alphaToCoverageConfig.swapValues();
		alphaToOneConfig.swapValues();
		colorWriteEnableConfig.swapValues();
		colorWriteMaskConfig.swapValues();
		rasterizationStreamConfig.swapValues();
		logicOpEnableConfig.swapValues();
		colorBlendEnableConfig.swapValues();
		colorBlendEquationConfig.swapValues();
		blendConstantsConfig.swapValues();
		provokingVertexConfig.swapValues();
		negativeOneToOneConfig.swapValues();
		depthClipEnableConfig.swapValues();
		lineStippleEnableConfig.swapValues();
		lineStippleParamsConfig.swapValues();
		sampleLocationsEnableConfig.swapValues();
		conservativeRasterModeConfig.swapValues();
		extraPrimitiveOverEstConfig.swapValues();
		lineRasterModeConfig.swapValues();
		coverageToColorEnableConfig.swapValues();
		coverageToColorLocationConfig.swapValues();
		rasterizationSamplesConfig.swapValues();
		lineWidthConfig.swapValues();
#ifndef CTS_USES_VULKANSC
		coverageModulationModeConfig.swapValues();
		coverageModTableEnableConfig.swapValues();
		coverageModTableConfig.swapValues();
		coverageReductionModeConfig.swapValues();
		viewportSwizzleConfig.swapValues();
		shadingRateImageEnableConfig.swapValues();
		viewportWScalingEnableConfig.swapValues();
		reprFragTestEnableConfig.swapValues();
#endif // CTS_USES_VULKANSC

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

	// Returns true if we're testing the logic op.
	bool testLogicOp () const
	{
		return static_cast<bool>(logicOpConfig.dynamicValue);
	}

	// Returns true if we're testing the logic op enable state.
	bool testLogicOpEnable () const
	{
		return static_cast<bool>(logicOpEnableConfig.dynamicValue);
	}

	// Returns true if we're testing the patch control points.
	bool testPatchControlPoints () const
	{
		return static_cast<bool>(patchControlPointsConfig.dynamicValue);
	}

	// Returns true if we're testing tessellation domain origin.
	bool testTessellationDomainOrigin () const
	{
		return static_cast<bool>(tessDomainOriginConfig.dynamicValue);
	}

	// Returns true if we're testing primitive restart enable.
	bool testPrimRestartEnable () const
	{
		return static_cast<bool>(primRestartEnableConfig.dynamicValue);
	}

	// Returns the topology class.
	TopologyClass topologyClass () const
	{
		return getTopologyClass(topologyConfig.staticValue);
	}

	// Returns true if the topology class is patches for tessellation.
	bool patchesTopology () const
	{
		return (topologyClass() == TopologyClass::PATCH);
	}

	// Returns true if the test needs tessellation shaders.
	bool needsTessellation () const
	{
		return (testPatchControlPoints() || patchesTopology() || testTessellationDomainOrigin());
	}

	// Returns the active line stipple enablement flag.
	bool getActiveLineStippleEnable () const
	{
		return ((static_cast<bool>(lineStippleEnableConfig.dynamicValue) && !m_swappedValues) ? lineStippleEnableConfig.dynamicValue.get() : lineStippleEnableConfig.staticValue);
	}

	// Returns the active primitive restart enablement flag.
	bool getActivePrimRestartEnable () const
	{
		return ((static_cast<bool>(primRestartEnableConfig.dynamicValue) && !m_swappedValues) ? primRestartEnableConfig.dynamicValue.get() : primRestartEnableConfig.staticValue);
	}

	// Returns the active representative fragment test enablement flag.
	bool getActiveReprFragTestEnable () const
	{
#ifndef CTS_USES_VULKANSC
		return ((static_cast<bool>(reprFragTestEnableConfig.dynamicValue) && !m_swappedValues) ? reprFragTestEnableConfig.dynamicValue.get() : reprFragTestEnableConfig.staticValue);
#else
		return false;
#endif // CTS_USES_VULKANSC
	}

	// Returns the active color blend enablement flag.
	bool getActiveColorBlendEnable () const
	{
		return ((static_cast<bool>(colorBlendEnableConfig.dynamicValue) && !m_swappedValues) ? colorBlendEnableConfig.dynamicValue.get() : colorBlendEnableConfig.staticValue);
	}

	// Returns true if the test needs an index buffer.
	bool needsIndexBuffer () const
	{
		return ((testPrimRestartEnable() || getActiveLineStippleEnable()) && !useMeshShaders);
	}

	// Returns true if the test needs the depth bias clamp feature.
	bool needsDepthBiasClampFeature () const
	{
		return (getActiveDepthBiasParams().clamp != 0.0f);
	}

	// Returns true if the configuration needs VK_EXT_extended_dynamic_state3.
	bool needsEDS3 () const
	{
		return	(	(!!tessDomainOriginConfig.dynamicValue)
				||	(!!depthClampEnableConfig.dynamicValue)
				||	(!!polygonModeConfig.dynamicValue)
				||	(!!sampleMaskConfig.dynamicValue)
				||	(!!alphaToCoverageConfig.dynamicValue)
				||	(!!alphaToOneConfig.dynamicValue)
				||	(!!colorWriteMaskConfig.dynamicValue)
				||	(!!rasterizationStreamConfig.dynamicValue)
				||	(!!logicOpEnableConfig.dynamicValue)
				||	(!!colorBlendEnableConfig.dynamicValue)
				||	(!!colorBlendEquationConfig.dynamicValue)
				||	(!!provokingVertexConfig.dynamicValue)
				||	(!!negativeOneToOneConfig.dynamicValue)
				||	(!!depthClipEnableConfig.dynamicValue)
				||	(!!lineStippleEnableConfig.dynamicValue)
				||	(!!sampleLocationsEnableConfig.dynamicValue)
				||	(!!conservativeRasterModeConfig.dynamicValue)
				||	(!!extraPrimitiveOverEstConfig.dynamicValue)
				||	(!!lineRasterModeConfig.dynamicValue)
				||	(!!coverageToColorEnableConfig.dynamicValue)
				||	(!!coverageToColorLocationConfig.dynamicValue)
				||	(!!rasterizationSamplesConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
				||	(!!coverageModulationModeConfig.dynamicValue)
				||	(!!coverageModTableEnableConfig.dynamicValue)
				||	(!!coverageModTableConfig.dynamicValue)
				||	(!!coverageReductionModeConfig.dynamicValue)
				||	(!!viewportSwizzleConfig.dynamicValue)
				||	(!!shadingRateImageEnableConfig.dynamicValue)
				||	(!!viewportWScalingEnableConfig.dynamicValue)
				||	(!!reprFragTestEnableConfig.dynamicValue)
#endif // CTS_USES_VULKANSC
				||	favorStaticNullPointers);
	}

	// Returns the appropriate color image format for the test.
	vk::VkFormat colorFormat () const
	{
		// Special case for some tests.
		if (forceUnormColorFormat)
			return kUnormColorFormat;

		// Pick int color format when testing logic op dynamic states.
		if (testLogicOp() || testLogicOpEnable())
			return kIntColorFormat;

		// Pick special color format for coverage to color.
		if (coverageToColorStruct())
			return kIntRedColorFormat;

		return kUnormColorFormat;
	}

	// Get used color sample count.
	vk::VkSampleCountFlagBits getColorSampleCount () const
	{
		const auto usedColorSampleCount	= ((coverageModulation || coverageReduction)
										? colorSampleCount.get()
										: getActiveSampleCount());
		return usedColorSampleCount;
	}

	// Returns the list of dynamic states affected by this config.
	std::vector<vk::VkDynamicState> getDynamicStates () const
	{
		std::vector<vk::VkDynamicState> dynamicStates;

		if (lineWidthConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_WIDTH);
		if (depthBiasConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BIAS);
		if (cullModeConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_CULL_MODE_EXT);
		if (frontFaceConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_FRONT_FACE_EXT);
		if (topologyConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT);
		if (viewportConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT);
		if (scissorConfig.dynamicValue)					dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT);
		if (strideConfig.dynamicValue)					dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT);
		if (depthTestEnableConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT);
		if (depthWriteEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT);
		if (depthCompareOpConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT);
		if (depthBoundsTestEnableConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT);
		if (depthBoundsConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS);
		if (stencilTestEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT);
		if (stencilOpConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_OP_EXT);
		if (vertexGenerator.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);
		if (patchControlPointsConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT);
		if (rastDiscardEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT);
		if (depthBiasEnableConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT);
		if (logicOpConfig.dynamicValue)					dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LOGIC_OP_EXT);
		if (primRestartEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT);
		if (colorWriteEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
		if (blendConstantsConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_BLEND_CONSTANTS);
		if (lineStippleParamsConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_STIPPLE_EXT);
#ifndef CTS_USES_VULKANSC
		if (tessDomainOriginConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT);
		if (depthClampEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT);
		if (polygonModeConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
		if (sampleMaskConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SAMPLE_MASK_EXT);
		if (alphaToCoverageConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
		if (alphaToOneConfig.dynamicValue)				dynamicStates.push_back(vk::VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT);
		if (colorWriteMaskConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
		if (rasterizationStreamConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT);
		if (logicOpEnableConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT);
		if (colorBlendEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
		if (colorBlendEquationConfig.dynamicValue)
		{
			if (colorBlendBoth || nullStaticColorBlendAttPtr)
			{
														dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
														dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT);
			}
			else
			{
														dynamicStates.push_back(colorBlendEquationConfig.staticValue.isAdvanced()
															? vk::VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT
															: vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
			}
		}
		if (provokingVertexConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT);
		if (negativeOneToOneConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT);
		if (depthClipEnableConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT);
		if (lineStippleEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT);
		if (sampleLocationsEnableConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT);
		if (conservativeRasterModeConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT);
		if (extraPrimitiveOverEstConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT);
		if (lineRasterModeConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT);
		if (rasterizationSamplesConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);
		if (coverageToColorEnableConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV);
		if (coverageToColorLocationConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV);
		if (coverageModulationModeConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV);
		if (coverageModTableEnableConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV);
		if (coverageModTableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV);
		if (coverageReductionModeConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV);
		if (viewportSwizzleConfig.dynamicValue)			dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV);
		if (shadingRateImageEnableConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV);
		if (viewportWScalingEnableConfig.dynamicValue)	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV);
		if (reprFragTestEnableConfig.dynamicValue)		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV);
#endif // CTS_USES_VULKANSC

		return dynamicStates;
	}

#ifndef CTS_USES_VULKANSC
	// Returns true if the test configuration uses dynamic states which are incompatible with mesh shading pipelines.
	bool badMeshShadingPipelineDynState () const
	{
		const auto states = getDynamicStates();
		return std::any_of(begin(states), end(states), isMeshShadingPipelineIncompatible);
	}
#endif // CTS_USES_VULKANSC

	bool testEDS() const
	{
		return (cullModeConfig.dynamicValue
			|| frontFaceConfig.dynamicValue
			|| topologyConfig.dynamicValue
			|| viewportConfig.dynamicValue
			|| scissorConfig.dynamicValue
			|| strideConfig.dynamicValue
			|| depthTestEnableConfig.dynamicValue
			|| depthWriteEnableConfig.dynamicValue
			|| depthCompareOpConfig.dynamicValue
			|| depthBoundsTestEnableConfig.dynamicValue
			|| stencilTestEnableConfig.dynamicValue
			|| stencilOpConfig.dynamicValue);
	}

	bool testEDS2() const
	{
		return (rastDiscardEnableConfig.dynamicValue
			|| depthBiasEnableConfig.dynamicValue
			|| primRestartEnableConfig.dynamicValue
			|| useExtraDynPCPPipeline);
	}

	bool testVertexDynamic() const
	{
		return static_cast<bool>(vertexGenerator.dynamicValue);
	}

	// Returns the list of extensions needed by this config. Note some other
	// requirements are checked with feature structs, which is particularly
	// important for extensions which have been partially promoted, like EDS
	// and EDS2. Extensions requested here have not been partially promoted.
	std::vector<std::string> getRequiredExtensions () const
	{
		std::vector<std::string> extensions;

		if (needsEDS3())
		{
			extensions.push_back("VK_EXT_extended_dynamic_state3");
		}

		if (testTessellationDomainOrigin() || getActiveTessellationDomainOrigin() != vk::VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT)
		{
			extensions.push_back("VK_KHR_maintenance2");
		}

		if (rasterizationStreamStruct())
		{
			extensions.push_back("VK_EXT_transform_feedback");
		}

		if (provokingVertexStruct())
		{
			extensions.push_back("VK_EXT_provoking_vertex");
		}

		if (negativeOneToOneStruct())
		{
			extensions.push_back("VK_EXT_depth_clip_control");
		}

		if (depthClipEnableStruct())
		{
			extensions.push_back("VK_EXT_depth_clip_enable");
		}

		if (lineRasterizationExt())
		{
			extensions.push_back("VK_EXT_line_rasterization");
		}

		if (colorBlendEquationConfig.staticValue.isAdvanced())
		{
			extensions.push_back("VK_EXT_blend_operation_advanced");
		}

		if (sampleLocationsStruct())
		{
			extensions.push_back("VK_EXT_sample_locations");
		}

		if (coverageToColorStruct())
		{
			extensions.push_back("VK_NV_fragment_coverage_to_color");
		}

		if (conservativeRasterStruct() || static_cast<bool>(maxPrimitiveOverestimationSize))
		{
			extensions.push_back("VK_EXT_conservative_rasterization");
		}

		if (coverageModulation)
		{
			extensions.push_back("VK_NV_framebuffer_mixed_samples");
		}

		if (coverageReduction)
		{
			extensions.push_back("VK_NV_coverage_reduction_mode");
		}

		if (viewportSwizzle)
		{
			extensions.push_back("VK_NV_viewport_swizzle");
		}

		if (shadingRateImage)
		{
			extensions.push_back("VK_NV_shading_rate_image");
		}

		if (viewportWScaling)
		{
			extensions.push_back("VK_NV_clip_space_w_scaling");
		}

		if (representativeFragmentTest)
		{
			extensions.push_back("VK_NV_representative_fragment_test");
		}

		if (useColorWriteEnable)
		{
			extensions.push_back("VK_EXT_color_write_enable");
		}

		return extensions;
	}

	uint32_t getFragDescriptorSetIndex () const
	{
		return (useMeshShaders ? 1u : 0u);
	}

	bool useFragShaderAtomics () const
	{
		return (representativeFragmentTest || forceAtomicCounters);
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
	float		stripScale;
};

void copy(vk::VkStencilOpState& dst, const StencilOpParams& src)
{
	dst.failOp		= src.failOp;
	dst.passOp		= src.passOp;
	dst.depthFailOp	= src.depthFailOp;
	dst.compareOp	= src.compareOp;
}

vk::VkImageCreateInfo makeImageCreateInfo (vk::VkFormat format, vk::VkExtent3D extent, vk::VkSampleCountFlagBits sampleCount, vk::VkImageUsageFlags usage, vk::VkImageCreateFlags createFlags)
{
	const vk::VkImageCreateInfo imageCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		createFlags,								//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		format,										//	VkFormat				format;
		extent,										//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		sampleCount,								//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		usage,										//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,											//	deUint32				queueFamilyIndexCount;
		nullptr,									//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
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
	DE_ASSERT(staticTopologyClass == TopologyClass::LINE || staticTopologyClass == TopologyClass::TRIANGLE
		|| staticTopologyClass == TopologyClass::PATCH);

	// Make sure these are consistent.
	DE_ASSERT(!(m_testConfig.testPatchControlPoints() && !m_testConfig.patchesTopology()));
	DE_ASSERT(!(m_testConfig.patchesTopology() && m_testConfig.getActivePatchControlPoints() <= 1u));

	// Do not use an extra dynamic patch control points pipeline if we're not testing them.
	DE_ASSERT(!m_testConfig.useExtraDynPCPPipeline || m_testConfig.testPatchControlPoints());
}

void ExtendedDynamicStateTest::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	// Check feature support.
	const auto& baseFeatures	= context.getDeviceFeatures();
	const auto& edsFeatures		= context.getExtendedDynamicStateFeaturesEXT();
	const auto& eds2Features	= context.getExtendedDynamicState2FeaturesEXT();
	const auto& viFeatures		= context.getVertexInputDynamicStateFeaturesEXT();
#ifndef CTS_USES_VULKANSC
	const auto& meshFeatures	= context.getMeshShaderFeaturesEXT();
#endif // CTS_USES_VULKANSC

	if (m_testConfig.dualSrcBlend && !baseFeatures.dualSrcBlend)
		TCU_THROW(NotSupportedError, "dualSrcBlend is not supported");

	if (m_testConfig.testEDS() && !edsFeatures.extendedDynamicState)
		TCU_THROW(NotSupportedError, "extendedDynamicState is not supported");

	if (m_testConfig.testEDS2() && !eds2Features.extendedDynamicState2)
		TCU_THROW(NotSupportedError, "extendedDynamicState2 is not supported");

	if (m_testConfig.testLogicOp() && !eds2Features.extendedDynamicState2LogicOp)
		TCU_THROW(NotSupportedError, "extendedDynamicState2LogicOp is not supported");

	if ((m_testConfig.testPatchControlPoints() || m_testConfig.useExtraDynPCPPipeline) && !eds2Features.extendedDynamicState2PatchControlPoints)
		TCU_THROW(NotSupportedError, "extendedDynamicState2PatchControlPoints is not supported");

	if (m_testConfig.testVertexDynamic() && !viFeatures.vertexInputDynamicState)
		TCU_THROW(NotSupportedError, "vertexInputDynamicState is not supported");

#ifndef CTS_USES_VULKANSC
	if ((m_testConfig.useMeshShaders || m_testConfig.bindUnusedMeshShadingPipeline) && !meshFeatures.meshShader)
		TCU_THROW(NotSupportedError, "meshShader is not supported");
#endif // CTS_USES_VULKANSC

	// Check extension support.
	const auto requiredExtensions = m_testConfig.getRequiredExtensions();
	for (const auto& extension : requiredExtensions)
		context.requireDeviceFunctionality(extension);

	// Check support needed for the vertex generators.
	m_testConfig.vertexGenerator.staticValue->checkSupport(context);
	if (m_testConfig.vertexGenerator.dynamicValue)
		m_testConfig.vertexGenerator.dynamicValue.get()->checkSupport(context);

	// Special requirement for rasterizationSamples tests.
	// The first iteration of these tests puts the pipeline in a mixed samples state,
	// where colorCount != rasterizationSamples.
	if (m_testConfig.rasterizationSamplesConfig.dynamicValue &&
		(m_testConfig.sequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC ||
		 m_testConfig.sequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC) &&
		!context.isDeviceFunctionalitySupported("VK_AMD_mixed_attachment_samples") &&
		!context.isDeviceFunctionalitySupported("VK_NV_framebuffer_mixed_samples"))

		TCU_THROW(NotSupportedError, "VK_AMD_mixed_attachment_samples or VK_NV_framebuffer_mixed_samples are not supported");

	if (m_testConfig.rasterizationSamplesConfig.dynamicValue &&
		(m_testConfig.sequenceOrdering == SequenceOrdering::BETWEEN_PIPELINES ||
		 m_testConfig.sequenceOrdering == SequenceOrdering::AFTER_PIPELINES ||
		 m_testConfig.sequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC ||
		 m_testConfig.isReversed()) &&
		(context.isDeviceFunctionalitySupported("VK_AMD_mixed_attachment_samples") ||
		context.isDeviceFunctionalitySupported("VK_NV_framebuffer_mixed_samples")))

		TCU_THROW(NotSupportedError, "Test not supported with VK_AMD_mixed_attachment_samples or VK_NV_framebuffer_mixed_samples");

	// Check the number of viewports needed and the corresponding limits.
	const auto&	viewportConfig	= m_testConfig.viewportConfig;
	auto		numViewports	= viewportConfig.staticValue.size();

	if (viewportConfig.dynamicValue)
		numViewports = std::max(numViewports, viewportConfig.dynamicValue.get().size());

	if (numViewports > 1)
	{
		const auto properties = vk::getPhysicalDeviceProperties(vki, physicalDevice);
		if (numViewports > static_cast<decltype(numViewports)>(properties.limits.maxViewports))
			TCU_THROW(NotSupportedError, "Number of viewports not supported (" + de::toString(numViewports) + ")");
	}

	const auto&	dbTestEnable	= m_testConfig.depthBoundsTestEnableConfig;
	const bool	useDepthBounds	= (dbTestEnable.staticValue || (dbTestEnable.dynamicValue && dbTestEnable.dynamicValue.get()));

	if (useDepthBounds || m_testConfig.needsGeometryShader() || m_testConfig.needsTessellation() || m_testConfig.needsDepthBiasClampFeature())
	{
		const auto features = vk::getPhysicalDeviceFeatures(vki, physicalDevice);

		// Check depth bounds test support.
		if (useDepthBounds && !features.depthBounds)
			TCU_THROW(NotSupportedError, "Depth bounds feature not supported");

		// Check geometry shader support.
		if (m_testConfig.needsGeometryShader() && !features.geometryShader)
			TCU_THROW(NotSupportedError, "Geometry shader not supported");

		// Check tessellation support
		if (m_testConfig.needsTessellation() && !features.tessellationShader)
			TCU_THROW(NotSupportedError, "Tessellation feature not supported");

		// Check depth bias clamp feature.
		if (m_testConfig.needsDepthBiasClampFeature() && !features.depthBiasClamp)
			TCU_THROW(NotSupportedError, "Depth bias clamp not supported");
	}

	// Check color image format support (depth/stencil will be chosen and checked at runtime).
	{
		const auto colorFormat		= m_testConfig.colorFormat();
		const auto colorSampleCount	= m_testConfig.getColorSampleCount();
		const auto colorImageInfo	= makeImageCreateInfo(colorFormat, kFramebufferExtent, colorSampleCount, kColorUsage, 0u);

		vk::VkImageFormatProperties formatProps;
		const auto result = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, colorImageInfo.format, colorImageInfo.imageType, colorImageInfo.tiling, colorImageInfo.usage, colorImageInfo.flags, &formatProps);

		if (result != vk::VK_SUCCESS)
			TCU_THROW(NotSupportedError, "Required color image features not supported");

		if ((formatProps.sampleCounts & colorSampleCount) != colorSampleCount)
			TCU_THROW(NotSupportedError, "Required color sample count not supported");

		// If blending is active, we need to check support explicitly.
		if (m_testConfig.getActiveColorBlendEnable())
		{
			const auto colorFormatProps = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, colorFormat);
			DE_ASSERT(colorImageInfo.tiling == vk::VK_IMAGE_TILING_OPTIMAL);
			if (!(colorFormatProps.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT))
				TCU_THROW(NotSupportedError, "Color format does not support blending");
		}
	}

	// Extended dynamic state 3 features.
	if (m_testConfig.needsEDS3())
	{
#ifndef CTS_USES_VULKANSC
		const auto& eds3Features = context.getExtendedDynamicState3FeaturesEXT();

		if (m_testConfig.testTessellationDomainOrigin() && !eds3Features.extendedDynamicState3TessellationDomainOrigin)
			TCU_THROW(NotSupportedError, "extendedDynamicState3TessellationDomainOrigin not supported");

		if (m_testConfig.depthClampEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3DepthClampEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3DepthClampEnable not supported");

		if (m_testConfig.polygonModeConfig.dynamicValue && !eds3Features.extendedDynamicState3PolygonMode)
			TCU_THROW(NotSupportedError, "extendedDynamicState3PolygonMode not supported");

		if (m_testConfig.sampleMaskConfig.dynamicValue && !eds3Features.extendedDynamicState3SampleMask)
			TCU_THROW(NotSupportedError, "extendedDynamicState3SampleMask not supported");

		if (m_testConfig.alphaToCoverageConfig.dynamicValue && !eds3Features.extendedDynamicState3AlphaToCoverageEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToCoverageEnable not supported");

		if (m_testConfig.alphaToOneConfig.dynamicValue && !eds3Features.extendedDynamicState3AlphaToOneEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToOneEnable not supported");

		if (m_testConfig.colorWriteMaskConfig.dynamicValue && !eds3Features.extendedDynamicState3ColorWriteMask)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ColorWriteMask not supported");

		if (m_testConfig.rasterizationStreamConfig.dynamicValue && !eds3Features.extendedDynamicState3RasterizationStream)
			TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationStream not supported");

		if (m_testConfig.logicOpEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3LogicOpEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3LogicOpEnable not supported");

		if (m_testConfig.colorBlendEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3ColorBlendEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ColorBlendEnable not supported");

		if (m_testConfig.colorBlendEquationConfig.dynamicValue)
		{
			const auto isAdvanced = m_testConfig.colorBlendEquationConfig.staticValue.isAdvanced();

			if (isAdvanced || m_testConfig.colorBlendBoth || m_testConfig.nullStaticColorBlendAttPtr)
			{
				if (!eds3Features.extendedDynamicState3ColorBlendAdvanced)
					TCU_THROW(NotSupportedError, "extendedDynamicState3ColorBlendAdvanced not supported");
			}

			if (!isAdvanced || m_testConfig.colorBlendBoth)
			{
				if (!eds3Features.extendedDynamicState3ColorBlendEquation)
					TCU_THROW(NotSupportedError, "extendedDynamicState3ColorBlendEquation not supported");
			}
		}

		if (m_testConfig.provokingVertexConfig.dynamicValue && !eds3Features.extendedDynamicState3ProvokingVertexMode)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ProvokingVertexMode not supported");

		if (m_testConfig.negativeOneToOneConfig.dynamicValue && !eds3Features.extendedDynamicState3DepthClipNegativeOneToOne)
			TCU_THROW(NotSupportedError, "extendedDynamicState3DepthClipNegativeOneToOne not supported");

		if (m_testConfig.depthClipEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3DepthClipEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3DepthClipEnable not supported");

		if (m_testConfig.lineStippleEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3LineStippleEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3LineStippleEnable not supported");

		if (m_testConfig.sampleLocationsEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3SampleLocationsEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3SampleLocationsEnable not supported");

		if (m_testConfig.conservativeRasterModeConfig.dynamicValue && !eds3Features.extendedDynamicState3ConservativeRasterizationMode)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ConservativeRasterizationMode not supported");

		if (m_testConfig.extraPrimitiveOverEstConfig.dynamicValue && !eds3Features.extendedDynamicState3ExtraPrimitiveOverestimationSize)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ExtraPrimitiveOverestimationSize not supported");

		if (m_testConfig.lineRasterModeConfig.dynamicValue && !eds3Features.extendedDynamicState3LineRasterizationMode)
			TCU_THROW(NotSupportedError, "extendedDynamicState3LineRasterizationMode not supported");

		if (m_testConfig.coverageToColorEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3CoverageToColorEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3CoverageToColorEnable not supported");

		if (m_testConfig.coverageToColorLocationConfig.dynamicValue && !eds3Features.extendedDynamicState3CoverageToColorLocation)
			TCU_THROW(NotSupportedError, "extendedDynamicState3CoverageToColorLocation not supported");

		if (m_testConfig.coverageModulationModeConfig.dynamicValue && !eds3Features.extendedDynamicState3CoverageModulationMode)
			TCU_THROW(NotSupportedError, "extendedDynamicState3CoverageModulationMode not supported");

		if (m_testConfig.coverageModTableEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3CoverageModulationTableEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3CoverageModulationTableEnable not supported");

		if (m_testConfig.coverageModTableConfig.dynamicValue && !eds3Features.extendedDynamicState3CoverageModulationTable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3CoverageModulationTable not supported");

		if (m_testConfig.coverageReductionModeConfig.dynamicValue)
		{
			if (!eds3Features.extendedDynamicState3CoverageReductionMode)
				TCU_THROW(NotSupportedError, "extendedDynamicState3CoverageReductionMode not supported");

			uint32_t combinationCount = 0U;
			auto result = vki.getPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(physicalDevice, &combinationCount, nullptr);
			if (result != vk::VK_SUCCESS || combinationCount == 0U)
				TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV supported no combinations");

			const vk::VkFramebufferMixedSamplesCombinationNV defaultCombination = vk::initVulkanStructure();
			std::vector<vk::VkFramebufferMixedSamplesCombinationNV> combinations(combinationCount, defaultCombination);
			result = vki.getPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(physicalDevice, &combinationCount, combinations.data());
			if (result != vk::VK_SUCCESS)
				TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV supported no combinations");

			auto findCombination = [&](vk::VkCoverageReductionModeNV const coverageReductionMode) -> bool {
				for (uint32_t i = 0U; i < combinationCount; ++i) {
					if (combinations[i].rasterizationSamples == m_testConfig.rasterizationSamplesConfig.staticValue &&
						combinations[i].colorSamples == m_testConfig.getColorSampleCount() &&
						combinations[i].coverageReductionMode == coverageReductionMode)
					{
						return true;
					}
				}
				return false;
			};
			if (!findCombination(m_testConfig.coverageReductionModeConfig.staticValue) || !findCombination(m_testConfig.coverageReductionModeConfig.dynamicValue.get()))
				TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV no matching combination found");
		}

		if (m_testConfig.viewportSwizzleConfig.dynamicValue && !eds3Features.extendedDynamicState3ViewportSwizzle)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ViewportSwizzle not supported");

		if (m_testConfig.shadingRateImageEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3ShadingRateImageEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ShadingRateImageEnable not supported");

		if (m_testConfig.viewportWScalingEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3ViewportWScalingEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ViewportWScalingEnable not supported");

		if (m_testConfig.reprFragTestEnableConfig.dynamicValue && !eds3Features.extendedDynamicState3RepresentativeFragmentTestEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3RepresentativeFragmentTestEnable not supported");

		if (m_testConfig.rasterizationSamplesConfig.dynamicValue && !eds3Features.extendedDynamicState3RasterizationSamples)
			TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");
#else
		TCU_THROW(NotSupportedError, "VulkanSC does not support extended dynamic state 3");
#endif // CTS_USES_VULKANSC
	}

	if (m_testConfig.getActivePolygonMode() != vk::VK_POLYGON_MODE_FILL)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FILL_MODE_NON_SOLID);

	if (m_testConfig.getActiveAlphaToOne())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_ALPHA_TO_ONE);

	if (m_testConfig.rasterizationStreamStruct() || static_cast<bool>(m_testConfig.shaderRasterizationStream))
	{
#ifndef CTS_USES_VULKANSC
		const auto& xfProperties = context.getTransformFeedbackPropertiesEXT();
		if (!xfProperties.transformFeedbackRasterizationStreamSelect)
			TCU_THROW(NotSupportedError, "transformFeedbackRasterizationStreamSelect not supported");

		// VUID-RuntimeSpirv-Stream-06312
		if (static_cast<bool>(m_testConfig.shaderRasterizationStream))
		{
			const auto shaderStreamId = m_testConfig.shaderRasterizationStream.get();
			if (shaderStreamId >= xfProperties.maxTransformFeedbackStreams)
				TCU_THROW(NotSupportedError, "Geometry shader rasterization stream above maxTransformFeedbackStreams limit");
		}

		// VUID-VkPipelineRasterizationStateStreamCreateInfoEXT-rasterizationStream-02325
		if (static_cast<bool>(m_testConfig.rasterizationStreamConfig.staticValue))
		{
			const auto staticStreamId = m_testConfig.rasterizationStreamConfig.staticValue.get();
			if (staticStreamId >= xfProperties.maxTransformFeedbackStreams)
				TCU_THROW(NotSupportedError, "Static stream number above maxTransformFeedbackStreams limit");
		}
		if (static_cast<bool>(m_testConfig.rasterizationStreamConfig.dynamicValue && static_cast<bool>(m_testConfig.rasterizationStreamConfig.dynamicValue.get())))
		{
			const auto dynamicStreamId = m_testConfig.rasterizationStreamConfig.dynamicValue->get();
			if (dynamicStreamId >= xfProperties.maxTransformFeedbackStreams)
				TCU_THROW(NotSupportedError, "Dynamic stream number above maxTransformFeedbackStreams limit");
		}
#else
		TCU_THROW(NotSupportedError, "VulkanSC does not support VK_EXT_transform_feedback");
#endif // CTS_USES_VULKANSC
	}

	if (m_testConfig.lineRasterizationExt())
	{
		// Check the implementation supports some type of stippled line.
		const auto&	lineRastFeatures	= context.getLineRasterizationFeaturesEXT();
		const auto	rasterMode			= selectLineRasterizationMode(lineRastFeatures, m_testConfig.lineStippleSupportRequired(), m_testConfig.lineRasterModeConfig.staticValue);

		if (rasterMode == LineRasterizationMode::NONE)
			TCU_THROW(NotSupportedError, "Wanted static line rasterization mode not supported");

		if (static_cast<bool>(m_testConfig.lineRasterModeConfig.dynamicValue) && static_cast<bool>(m_testConfig.lineRasterModeConfig.dynamicValue.get()))
		{
			const auto dynRasterMode = selectLineRasterizationMode(lineRastFeatures, m_testConfig.lineStippleSupportRequired(), m_testConfig.lineRasterModeConfig.dynamicValue.get());

			if (dynRasterMode == LineRasterizationMode::NONE)
				TCU_THROW(NotSupportedError, "Wanted dynamic line rasterization mode not supported");
		}
	}

	const auto hasMaxPrimitiveOverestimationSize = static_cast<bool>(m_testConfig.maxPrimitiveOverestimationSize);

	if (m_testConfig.conservativeRasterStruct() || hasMaxPrimitiveOverestimationSize)
	{
		const auto& conservativeRasterModeProps = context.getConservativeRasterizationPropertiesEXT();

		if (m_testConfig.getActiveConservativeRasterMode() == vk::VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT && !conservativeRasterModeProps.primitiveUnderestimation)
			TCU_THROW(NotSupportedError, "primitiveUnderestimation not supported");

		const auto	extraSize	= m_testConfig.getActiveExtraPrimitiveOverEstSize();
		const auto&	maxExtra	= conservativeRasterModeProps.maxExtraPrimitiveOverestimationSize;

		if (extraSize >= 0.0f && extraSize > maxExtra)
		{
			std::ostringstream msg;
			msg << "Extra primitive overestimation size (" << extraSize << ") above maxExtraPrimitiveOverestimationSize (" << maxExtra << ")";
			TCU_THROW(NotSupportedError, msg.str());
		}

		if (hasMaxPrimitiveOverestimationSize)
		{
			const auto maxPrimitiveOverestimationSizeVal = m_testConfig.maxPrimitiveOverestimationSize.get();
			if (conservativeRasterModeProps.primitiveOverestimationSize > maxPrimitiveOverestimationSizeVal)
			{
				std::ostringstream msg;
				msg << "primitiveOverestimationSize (" << conservativeRasterModeProps.primitiveOverestimationSize
					<< ") too big for this test (max " << maxPrimitiveOverestimationSizeVal << ")";
				TCU_THROW(NotSupportedError, msg.str());
			}
		}
	}

	if (m_testConfig.useFragShaderAtomics())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);

#ifndef CTS_USES_VULKANSC
	if (m_testConfig.depthBiasReprInfo)
	{
		const auto& reprInfo	= m_testConfig.depthBiasReprInfo.get();
		const auto& dbcFeatures	= context.getDepthBiasControlFeaturesEXT();

		if (reprInfo.depthBiasExact && !dbcFeatures.depthBiasExact)
			TCU_THROW(NotSupportedError, "depthBiasExact not supported");

		if (reprInfo.depthBiasRepresentation == vk::VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT
			&& !dbcFeatures.leastRepresentableValueForceUnormRepresentation)
		{
			TCU_THROW(NotSupportedError, "leastRepresentableValueForceUnormRepresentation not supported");
		}

		if (reprInfo.depthBiasRepresentation == vk::VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT && !dbcFeatures.floatRepresentation)
			TCU_THROW(NotSupportedError, "floatRepresentation not supported");
	}
#else
	TCU_THROW(NotSupportedError, "VulkanSC does not support VK_EXT_depth_bias_control");
#endif // CTS_USES_VULKANSC

	if (m_testConfig.getActiveLineWidth() != 1.0f)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_WIDE_LINES);

	if (m_testConfig.favorStaticNullPointers)
	{
		if (m_testConfig.primRestartEnableConfig.dynamicValue && m_testConfig.topologyConfig.dynamicValue)
		{
#ifndef CTS_USES_VULKANSC
			const auto& eds3Properties = context.getExtendedDynamicState3PropertiesEXT();
			if (!eds3Properties.dynamicPrimitiveTopologyUnrestricted)
				TCU_THROW(NotSupportedError, "dynamicPrimitiveTopologyUnrestricted not supported");
#else
			TCU_THROW(NotSupportedError, "VulkanSC does not support VK_EXT_extended_dynamic_state3");
#endif // CTS_USES_VULKANSC
		}
	}

	if (m_testConfig.sampleShadingEnable && !baseFeatures.sampleRateShading)
		TCU_THROW(NotSupportedError, "sampleRateShading not supported");

	checkPipelineConstructionRequirements(vki, physicalDevice, m_testConfig.pipelineConstructionType);
}

void ExtendedDynamicStateTest::initPrograms (vk::SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions meshBuildOptions (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::ostringstream pushSource;
	std::ostringstream fragOutputLocationStream;
	std::ostringstream vertSourceTemplateStream;
	std::ostringstream fragSourceTemplateStream;
	std::ostringstream geomSource;
	std::ostringstream tescSource;
	std::ostringstream teseSource;
	std::ostringstream meshSourceTemplateStream;

	pushSource
		<< "layout(push_constant, std430) uniform PushConstantsBlock {\n"
		<< "    vec4  triangleColor;\n"
		<< "    float depthValue;\n"
		<< "    int   viewPortIndex;\n"
		<< "    float scaleX;\n"
		<< "    float scaleY;\n"
		<< "    float offsetX;\n"
		<< "    float offsetY;\n"
		<< "    float stripScale;\n"
		<< "} pushConstants;\n"
		;
	const auto pushConstants = pushSource.str();

	const bool useAttIndex = m_testConfig.dualSrcBlend;
	for (uint32_t refIdx = 0; refIdx < m_testConfig.colorAttachmentCount; ++refIdx)
	{
		const bool			used		= (refIdx == m_testConfig.colorAttachmentCount - 1u);
		const std::string	attName		= (used ? "color" : "unused" + std::to_string(refIdx));
		const uint32_t		indexCount	= (useAttIndex ? 2u : 1u);

		for (uint32_t attIdx = 0u; attIdx < indexCount; ++attIdx)
		{
			const auto			idxStr		= std::to_string(attIdx);
			const std::string	indexDecl	= (useAttIndex ? (", index=" + idxStr) : "");
			const std::string	nameSuffix	= ((attIdx > 0u) ? idxStr : "");

			fragOutputLocationStream << "layout(location=" << refIdx << indexDecl << ") out ${OUT_COLOR_VTYPE} " << attName << nameSuffix << ";\n";
		}
	}
	const auto fragOutputLocations = fragOutputLocationStream.str();

	// The actual generator, attributes and calculations.
	const auto			topology	= m_testConfig.topologyClass();
	const auto			activeGen	= m_testConfig.getActiveVertexGenerator();
	const auto			attribDecls	= activeGen->getAttributeDeclarations();
	const auto			coordCalcs	= activeGen->getVertexCoordCalc();
	const auto			descDeclsV	= (m_testConfig.useMeshShaders ? activeGen->getDescriptorDeclarations() : std::vector<std::string>());
	const auto			descCalcsV	= (m_testConfig.useMeshShaders ? activeGen->getDescriptorCoordCalc(topology) : std::vector<std::string>());
	const auto			fragInputs	= activeGen->getFragInputAttributes();
	const auto			fragCalcs	= activeGen->getFragOutputCalc();
	const auto			glslExts	= activeGen->getGLSLExtensions();

	// The static generator, attributes and calculations, for the static pipeline, if needed.
	const auto			inactiveGen			= m_testConfig.getInactiveVertexGenerator();
	const auto			staticAttribDec		= inactiveGen->getAttributeDeclarations();
	const auto			staticCoordCalc		= inactiveGen->getVertexCoordCalc();
	const auto			staticFragInputs	= inactiveGen->getFragInputAttributes();
	const auto			staticFragCalcs		= inactiveGen->getFragOutputCalc();
	const auto			staticGlslExts		= inactiveGen->getGLSLExtensions();

	std::ostringstream	activeAttribs;
	std::ostringstream	activeCalcs;
	std::ostringstream	activeFragInputs;
	std::ostringstream	activeFragCalcs;
	std::ostringstream	activeExts;
	std::ostringstream	inactiveAttribs;
	std::ostringstream	inactiveCalcs;
	std::ostringstream	descDecls;
	std::ostringstream	descCalcs;
	std::ostringstream	inactiveFragInputs;
	std::ostringstream	inactiveFragCalcs;
	std::ostringstream	inactiveExts;

	for (const auto& decl : attribDecls)
		activeAttribs << decl << "\n";

	for (const auto& statement : coordCalcs)
		activeCalcs << "    " << statement << "\n";

	for (const auto& decl : staticAttribDec)
		inactiveAttribs << decl << "\n";

	for (const auto& statement : staticCoordCalc)
		inactiveCalcs << "    " << statement << "\n";

	for (const auto& decl : descDeclsV)
		descDecls << decl << "\n";

	for (const auto& calc : descCalcsV)
		descCalcs << "    " << calc << "\n";

	for (const auto& decl : fragInputs)
		activeFragInputs << decl << "\n";

	for (const auto& statement : fragCalcs)
		activeFragCalcs << "    " << statement << "\n";

	for (const auto& decl : staticFragInputs)
		inactiveFragInputs << decl << "\n";

	for (const auto& statement : staticFragCalcs)
		inactiveFragCalcs << "    " << statement << "\n";

	for (const auto& ext : glslExts)
		activeExts << ext << "\n";

	for (const auto& ext : staticGlslExts)
		inactiveExts << ext << "\n";

	vertSourceTemplateStream
		<< "#version 450\n"
		<< "${EXTENSIONS}"
		<< pushConstants
		<< "${ATTRIBUTES}"
		<< "out gl_PerVertex\n"
		<< "{\n"
		<< "    vec4 gl_Position;\n"
		<< "};\n"
		<< "void main() {\n"
		<< "${CALCULATIONS}"
		<< "    gl_Position = vec4(vertexCoords.x * pushConstants.scaleX + pushConstants.offsetX, vertexCoords.y * pushConstants.scaleY + pushConstants.offsetY, pushConstants.depthValue, 1.0);\n"
		<< "    vec2 stripOffset;\n"
		<< "    switch (gl_VertexIndex) {\n"
		<< "    case 0: stripOffset = vec2(0.0, 0.0); break;\n"
		<< "    case 1: stripOffset = vec2(0.0, 1.0); break;\n"
		<< "    case 2: stripOffset = vec2(1.0, 0.0); break;\n"
		<< "    case 3: stripOffset = vec2(1.0, 1.0); break;\n"
		<< "    case 4: stripOffset = vec2(2.0, 0.0); break;\n"
		<< "    case 5: stripOffset = vec2(2.0, 1.0); break;\n"
		<< "    default: stripOffset = vec2(-1000.0); break;\n"
		<< "    }\n"
		<< "    gl_Position.xy += pushConstants.stripScale * stripOffset;\n"
		<< "}\n"
		;

	tcu::StringTemplate vertSourceTemplate (vertSourceTemplateStream.str());

	const auto colorFormat	= m_testConfig.colorFormat();
	const auto vecType		= (vk::isUnormFormat(colorFormat) ? "vec4" : "uvec4");
	const auto fragSetIndex	= std::to_string(m_testConfig.getFragDescriptorSetIndex());
	const auto fragAtomics	= m_testConfig.useFragShaderAtomics();

	fragSourceTemplateStream
		<< "#version 450\n"
		<< (m_testConfig.representativeFragmentTest ? "layout(early_fragment_tests) in;\n" : "")
		<< (fragAtomics ? "layout(set=" + fragSetIndex + ", binding=0, std430) buffer AtomicBlock { uint fragCounter; } counterBuffer;\n" : "")
		<< pushConstants
		<< fragOutputLocations
		<< "${FRAG_INPUTS}"
		<< "void main() {\n"
		<< "    color = ${OUT_COLOR_VTYPE}" << (m_testConfig.dualSrcBlend ? de::toString(kOpaqueWhite) : "(pushConstants.triangleColor)") << ";\n"
		;

	if (m_testConfig.dualSrcBlend)
	{
		fragSourceTemplateStream
			<< "    color1 = ${OUT_COLOR_VTYPE}(pushConstants.triangleColor);\n"
			;
	}

	fragSourceTemplateStream
		<< "${FRAG_CALCULATIONS}"
		<< (fragAtomics ? "    atomicAdd(counterBuffer.fragCounter, 1u);\n" : "")
		<< (m_testConfig.sampleShadingEnable ? "    uint sampleId = gl_SampleID;\n" : "") // Enable sample shading for shader objects by reading gl_SampleID
		<< "}\n"
		;

	tcu::StringTemplate fragSourceTemplate (fragSourceTemplateStream.str());

	std::map<std::string, std::string> activeMap;
	std::map<std::string, std::string> inactiveMap;

	activeMap["ATTRIBUTES"]			= activeAttribs.str();
	activeMap["CALCULATIONS"]		= activeCalcs.str();
	activeMap["FRAG_INPUTS"]		= activeFragInputs.str();
	activeMap["FRAG_CALCULATIONS"]	= activeFragCalcs.str();
	activeMap["EXTENSIONS"]			= activeExts.str();
	activeMap["OUT_COLOR_VTYPE"]	= vecType;

	inactiveMap["ATTRIBUTES"]			= inactiveAttribs.str();
	inactiveMap["CALCULATIONS"]			= inactiveCalcs.str();
	inactiveMap["FRAG_INPUTS"]			= inactiveFragInputs.str();
	inactiveMap["FRAG_CALCULATIONS"]	= inactiveFragCalcs.str();
	inactiveMap["EXTENSIONS"]			= inactiveExts.str();
	inactiveMap["OUT_COLOR_VTYPE"]		= vecType;

	const auto activeVertSource		= vertSourceTemplate.specialize(activeMap);
	const auto activeFragSource		= fragSourceTemplate.specialize(activeMap);
	const auto inactiveVertSource	= vertSourceTemplate.specialize(inactiveMap);
	const auto inactiveFragSource	= fragSourceTemplate.specialize(inactiveMap);

	if (m_testConfig.needsGeometryShader())
	{
		const auto			topologyClass	= getTopologyClass(m_testConfig.topologyConfig.staticValue);
		const std::string	inputPrimitive	= ((topologyClass == TopologyClass::LINE) ? "lines" : "triangles");
		const deUint32		vertexCount		= ((topologyClass == TopologyClass::LINE) ? 2u : 3u);
		const std::string	outputPrimitive	= ((topologyClass == TopologyClass::LINE) ? "line_strip" : "triangle_strip");
		const auto			selectStream	= static_cast<bool>(m_testConfig.shaderRasterizationStream);
		const auto			streamNumber	= (selectStream ? m_testConfig.shaderRasterizationStream.get() : 0u);
		const auto			streamNumberStr	= de::toString(streamNumber);

		geomSource
			<< "#version 450\n"
			<< "layout (" << inputPrimitive << ") in;\n"
			<< "layout (" << outputPrimitive << ", max_vertices=" << vertexCount << ") out;\n"
			<< (m_testConfig.isMultiViewport() ? pushConstants : "")
			<< (selectStream ? "layout (stream=" + streamNumberStr + ") out;\n" : "")
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
				<< "    " << (selectStream ? ("EmitStreamVertex(" + streamNumberStr + ")") : "EmitVertex()") << ";\n"
				;
		}

		geomSource
			<< "}\n"
			;
	}

	if (m_testConfig.needsTessellation())
	{
		tescSource
			<< "#version 450\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "layout(vertices=3) out;\n"
			<< "in gl_PerVertex\n"
			<< "{\n"
			<< "    vec4 gl_Position;\n"
			<< "} gl_in[gl_MaxPatchVertices];\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_out[];\n"
			<< "void main() {\n"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "  gl_TessLevelOuter[0] = 3.0;\n"
			<< "  gl_TessLevelOuter[1] = 3.0;\n"
			<< "  gl_TessLevelOuter[2] = 3.0;\n"
			<< "  gl_TessLevelInner[0] = 3.0;\n"
			<< "}\n"
			;
		teseSource
			<< "#version 450\n"
			<< "#extension GL_EXT_tessellation_shader : require\n"
			<< "layout(triangles) in;\n"
			<< "in gl_PerVertex\n"
			<< "{\n"
			<< "  vec4 gl_Position;\n"
			<< "} gl_in[gl_MaxPatchVertices];\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "  vec4 gl_Position;\n"
			<< "};\n"
			<< "void main() {\n"
			<< "  gl_Position = (gl_in[0].gl_Position * gl_TessCoord.x + \n"
			<< "                 gl_in[1].gl_Position * gl_TessCoord.y + \n"
			<< "                 gl_in[2].gl_Position * gl_TessCoord.z);\n"
			<< "}\n"
			;
	}

#ifndef CTS_USES_VULKANSC
	if (m_testConfig.useMeshShaders)
	{
		DE_ASSERT(!m_testConfig.needsGeometryShader());
		DE_ASSERT(!m_testConfig.needsTessellation());
		//DE_ASSERT(!m_testConfig.needsIndexBuffer());

		// Make sure no dynamic states incompatible with mesh shading pipelines are used.
		DE_ASSERT(!m_testConfig.badMeshShadingPipelineDynState());

		// Shader below is designed to work with vertex buffers containing triangle strips as used by default.
		DE_ASSERT(m_testConfig.topologyConfig.staticValue == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
				  m_testConfig.topologyConfig.staticValue == vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
		DE_ASSERT(!m_testConfig.singleVertex);

		std::string	topologyStr;
		std::string	indicesBuiltIn;
		std::string	indicesVal;
		uint32_t	maxVertices		= 0u;

		switch (topology)
		{
		case TopologyClass::TRIANGLE:
			topologyStr		= "triangles";
			maxVertices		= 3u;
			indicesBuiltIn	= "gl_PrimitiveTriangleIndicesEXT";
			indicesVal		= "uvec3(0, 1, 2)";
			break;
		case TopologyClass::LINE:
			topologyStr		= "lines";
			maxVertices		= 2u;
			indicesBuiltIn	= "gl_PrimitiveLineIndicesEXT";
			indicesVal		= "uvec2(0, 1)";
			break;
		default:
			DE_ASSERT(false);
			break;
		}

		meshSourceTemplateStream
			<< "#version 450\n"
			<< "${EXTENSIONS}"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "layout(local_size_x=" << maxVertices << ", local_size_y=1, local_size_z=1) in;\n"
			<< "layout(" << topologyStr << ") out;\n"
			<< "layout(max_vertices=" << maxVertices << ", max_primitives=1) out;\n"
			<< pushConstants
			<< (m_testConfig.isMultiViewport()
				? "perprimitiveEXT out gl_MeshPerPrimitiveEXT { int gl_ViewportIndex; } gl_MeshPrimitivesEXT[];\n"
				: "")
			<< descDecls.str()
			<< "void main() {\n"
			<< descCalcs.str()
			<< "    SetMeshOutputsEXT(" << maxVertices << "u, 1u);\n"
			<< "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vec4(vertexCoords.x * pushConstants.scaleX + pushConstants.offsetX, vertexCoords.y * pushConstants.scaleY + pushConstants.offsetY, pushConstants.depthValue, 1.0);\n"
			<< "    if (gl_LocalInvocationIndex == 0u) {\n"
			<< "        " << indicesBuiltIn << "[0] = " << indicesVal << ";\n"
			<< (m_testConfig.isMultiViewport()
				? "        gl_MeshPrimitivesEXT[0].gl_ViewportIndex = pushConstants.viewPortIndex;\n"
				: "")
			<< "    }\n"
			<< "}\n"
			;
	}
#endif // CTS_USES_VULKANSC

	// In reversed test configurations, the pipeline with dynamic state needs to have the inactive shader.
	const auto kReversed = m_testConfig.isReversed();
	programCollection.glslSources.add("dynamicVert")	<< glu::VertexSource(kReversed ? inactiveVertSource : activeVertSource);
	programCollection.glslSources.add("staticVert")		<< glu::VertexSource(kReversed ? activeVertSource : inactiveVertSource);
	programCollection.glslSources.add("dynamicFrag")	<< glu::FragmentSource(kReversed ? inactiveFragSource : activeFragSource);
	programCollection.glslSources.add("staticFrag")		<< glu::FragmentSource(kReversed ? activeFragSource : inactiveFragSource);

	if (m_testConfig.needsGeometryShader())
		programCollection.glslSources.add("geom") << glu::GeometrySource(geomSource.str());
	if (m_testConfig.needsTessellation())
	{
		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tescSource.str());
		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(teseSource.str());
	}
	if (m_testConfig.useMeshShaders)
	{
		tcu::StringTemplate meshSourceTemplate (meshSourceTemplateStream.str());

		const auto activeMeshSource		= meshSourceTemplate.specialize(activeMap);
		const auto inactiveMeshSource	= meshSourceTemplate.specialize(inactiveMap);

		programCollection.glslSources.add("dynamicMesh")	<< glu::MeshSource(kReversed ? inactiveMeshSource : activeMeshSource) << meshBuildOptions;
		programCollection.glslSources.add("staticMesh")		<< glu::MeshSource(kReversed ? activeMeshSource : inactiveMeshSource) << meshBuildOptions;
	}

	if (m_testConfig.bindUnusedMeshShadingPipeline)
	{
		std::ostringstream meshNoOut;
		meshNoOut
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< "layout(triangles) out;\n"
			<< "layout(max_vertices=3, max_primitives=1) out;\n"
			<< "void main() {\n"
			<< "    SetMeshOutputsEXT(0u, 0u);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("meshNoOut") << glu::MeshSource(meshNoOut.str()) << meshBuildOptions;
	}

	// Extra vert and frag shaders for the extra patch control points pipeline. These draw offscreen.
	if (m_testConfig.useExtraDynPCPPipeline || m_testConfig.useExtraDynPipeline)
	{
		std::ostringstream vertDPCP;
		vertDPCP
			<< "#version 450\n"
			<< "\n"
			<< "vec2 positions[3] = vec2[](\n"
			<< "    vec2(-1.0, -1.0),\n"
			<< "    vec2( 3.0, -1.0),\n"
			<< "    vec2(-1.0,  3.0)\n"
			<< ");\n"
			<< "\n"
			<< "void main() {\n"
			<< "    gl_Position = vec4(positions[gl_VertexIndex] + 10.0 + 1.0 * float(gl_VertexIndex), 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("vertDPCP") << glu::VertexSource(vertDPCP.str());

		std::ostringstream fragDPCP;
		fragDPCP
			<< "#version 450\n"
			<< "layout(location=0) out " << vecType << " color;\n"
			<< "void main() {\n"
			<< "    color = " << vecType << "(1.0, 1.0, 1.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("fragDPCP") << glu::FragmentSource(fragDPCP.str());
	}
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

using BufferWithMemoryPtr = de::MovePtr<vk::BufferWithMemory>;

struct VertexBufferInfo
{
	VertexBufferInfo ()
		: buffer	()
		, offset	(0ull)
		, dataSize	(0ull)
	{}

	VertexBufferInfo (VertexBufferInfo&& other)
		: buffer	(other.buffer.release())
		, offset	(other.offset)
		, dataSize	(other.dataSize)
	{}

	BufferWithMemoryPtr	buffer;
	vk::VkDeviceSize	offset;
	vk::VkDeviceSize	dataSize;
};

void logErrors(tcu::TestLog& log, const std::string& setName, const std::string& setDesc, const tcu::ConstPixelBufferAccess& result, const tcu::ConstPixelBufferAccess& errorMask)
{
	log << tcu::TestLog::ImageSet(setName, setDesc)
		<< tcu::TestLog::Image(setName + "Result", "Result image", result)
		<< tcu::TestLog::Image(setName + "ErrorMask", "Error mask with errors marked in red", errorMask)
		<< tcu::TestLog::EndImageSet;
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
	if (testConfig.lineWidthConfig.dynamicValue)
		vkd.cmdSetLineWidth(cmdBuffer, testConfig.lineWidthConfig.dynamicValue.get());

	if (testConfig.depthBoundsConfig.dynamicValue)
	{
		const auto& minMaxDepth = testConfig.depthBoundsConfig.dynamicValue.get();
		vkd.cmdSetDepthBounds(cmdBuffer, minMaxDepth.first, minMaxDepth.second);
	}

	if (testConfig.cullModeConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetCullMode(cmdBuffer, testConfig.cullModeConfig.dynamicValue.get());
#else
		vkd.cmdSetCullModeEXT(cmdBuffer, testConfig.cullModeConfig.dynamicValue.get());
#endif // CTS_USES_VULKANSC

	if (testConfig.frontFaceConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetFrontFace(cmdBuffer, testConfig.frontFaceConfig.dynamicValue.get());
#else
		vkd.cmdSetFrontFaceEXT(cmdBuffer, testConfig.frontFaceConfig.dynamicValue.get());
#endif // CTS_USES_VULKANSC

	if (testConfig.topologyConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetPrimitiveTopology(cmdBuffer, testConfig.topologyConfig.dynamicValue.get());
#else
		vkd.cmdSetPrimitiveTopologyEXT(cmdBuffer, testConfig.topologyConfig.dynamicValue.get());
#endif // CTS_USES_VULKANSC

	if (testConfig.viewportConfig.dynamicValue)
	{
		const auto& viewports = testConfig.viewportConfig.dynamicValue.get();
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetViewportWithCount(cmdBuffer, static_cast<deUint32>(viewports.size()), viewports.data());
#else
		vkd.cmdSetViewportWithCountEXT(cmdBuffer, static_cast<deUint32>(viewports.size()), viewports.data());
#endif // CTS_USES_VULKANSC
	}

	if (testConfig.scissorConfig.dynamicValue)
	{
		const auto& scissors = testConfig.scissorConfig.dynamicValue.get();
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetScissorWithCount(cmdBuffer, static_cast<deUint32>(scissors.size()), scissors.data());
#else
		vkd.cmdSetScissorWithCountEXT(cmdBuffer, static_cast<deUint32>(scissors.size()), scissors.data());
#endif // CTS_USES_VULKANSC
	}

	if (testConfig.depthTestEnableConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetDepthTestEnable(cmdBuffer, makeVkBool32(testConfig.depthTestEnableConfig.dynamicValue.get()));
#else
		vkd.cmdSetDepthTestEnableEXT(cmdBuffer, makeVkBool32(testConfig.depthTestEnableConfig.dynamicValue.get()));
#endif // CTS_USES_VULKANSC

	if (testConfig.depthWriteEnableConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetDepthWriteEnable(cmdBuffer, makeVkBool32(testConfig.depthWriteEnableConfig.dynamicValue.get()));
#else
		vkd.cmdSetDepthWriteEnableEXT(cmdBuffer, makeVkBool32(testConfig.depthWriteEnableConfig.dynamicValue.get()));
#endif // CTS_USES_VULKANSC

	if (testConfig.depthCompareOpConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetDepthCompareOp(cmdBuffer, testConfig.depthCompareOpConfig.dynamicValue.get());
#else
		vkd.cmdSetDepthCompareOpEXT(cmdBuffer, testConfig.depthCompareOpConfig.dynamicValue.get());
#endif // CTS_USES_VULKANSC

	if (testConfig.depthBoundsTestEnableConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetDepthBoundsTestEnable(cmdBuffer, makeVkBool32(testConfig.depthBoundsTestEnableConfig.dynamicValue.get()));
#else
		vkd.cmdSetDepthBoundsTestEnableEXT(cmdBuffer, makeVkBool32(testConfig.depthBoundsTestEnableConfig.dynamicValue.get()));
#endif // CTS_USES_VULKANSC

	if (testConfig.stencilTestEnableConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetStencilTestEnable(cmdBuffer, makeVkBool32(testConfig.stencilTestEnableConfig.dynamicValue.get()));
#else
		vkd.cmdSetStencilTestEnableEXT(cmdBuffer, makeVkBool32(testConfig.stencilTestEnableConfig.dynamicValue.get()));
#endif // CTS_USES_VULKANSC

	if (testConfig.depthBiasEnableConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetDepthBiasEnable(cmdBuffer, makeVkBool32(testConfig.depthBiasEnableConfig.dynamicValue.get()));
#else
		vkd.cmdSetDepthBiasEnableEXT(cmdBuffer, makeVkBool32(testConfig.depthBiasEnableConfig.dynamicValue.get()));
#endif // CTS_USES_VULKANSC

	if (testConfig.depthBiasConfig.dynamicValue)
	{
		const auto& bias = testConfig.depthBiasConfig.dynamicValue.get();

#ifndef CTS_USES_VULKANSC
		if (testConfig.depthBiasReprInfo && !testConfig.isReversed())
		{
			vk::VkDepthBiasInfoEXT depthBiasInfo	= vk::initVulkanStructureConst(&testConfig.depthBiasReprInfo.get());
			depthBiasInfo.depthBiasConstantFactor	= bias.constantFactor;
			depthBiasInfo.depthBiasClamp			= bias.clamp;

			vkd.cmdSetDepthBias2EXT(cmdBuffer, &depthBiasInfo);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			vkd.cmdSetDepthBias(cmdBuffer, bias.constantFactor, bias.clamp, 0.0f);
		}
	}

	if (testConfig.rastDiscardEnableConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetRasterizerDiscardEnable(cmdBuffer, makeVkBool32(testConfig.rastDiscardEnableConfig.dynamicValue.get()));
#else
		vkd.cmdSetRasterizerDiscardEnableEXT(cmdBuffer, makeVkBool32(testConfig.rastDiscardEnableConfig.dynamicValue.get()));
#endif // CTS_USES_VULKANSC

	if (testConfig.primRestartEnableConfig.dynamicValue)
#ifndef CTS_USES_VULKANSC
		vkd.cmdSetPrimitiveRestartEnable(cmdBuffer, makeVkBool32(testConfig.primRestartEnableConfig.dynamicValue.get()));
#else
		vkd.cmdSetPrimitiveRestartEnableEXT(cmdBuffer, makeVkBool32(testConfig.primRestartEnableConfig.dynamicValue.get()));
#endif // CTS_USES_VULKANSC

	if (testConfig.logicOpConfig.dynamicValue)
		vkd.cmdSetLogicOpEXT(cmdBuffer, testConfig.logicOpConfig.dynamicValue.get());

	if (testConfig.patchControlPointsConfig.dynamicValue)
		vkd.cmdSetPatchControlPointsEXT(cmdBuffer, testConfig.patchControlPointsConfig.dynamicValue.get());

	if (testConfig.stencilOpConfig.dynamicValue)
	{
		for (const auto& params : testConfig.stencilOpConfig.dynamicValue.get())
#ifndef CTS_USES_VULKANSC
			vkd.cmdSetStencilOp(cmdBuffer, params.faceMask, params.failOp, params.passOp, params.depthFailOp, params.compareOp);
#else
			vkd.cmdSetStencilOpEXT(cmdBuffer, params.faceMask, params.failOp, params.passOp, params.depthFailOp, params.compareOp);
#endif // CTS_USES_VULKANSC
	}

	if (testConfig.vertexGenerator.dynamicValue)
	{
		const auto generator	= testConfig.vertexGenerator.dynamicValue.get();
		const auto bindings		= generator->getBindingDescriptions2(testConfig.strideConfig.staticValue);
		const auto attributes	= generator->getAttributeDescriptions2();

		vkd.cmdSetVertexInputEXT(cmdBuffer,
			static_cast<deUint32>(bindings.size()), de::dataOrNull(bindings),
			static_cast<deUint32>(attributes.size()), de::dataOrNull(attributes));
	}

	if (testConfig.colorWriteEnableConfig.dynamicValue)
	{
		const std::vector<vk::VkBool32> colorWriteEnableValues (testConfig.colorAttachmentCount, makeVkBool32(testConfig.colorWriteEnableConfig.dynamicValue.get()));
		vkd.cmdSetColorWriteEnableEXT(cmdBuffer, de::sizeU32(colorWriteEnableValues), de::dataOrNull(colorWriteEnableValues));
	}

	if (testConfig.blendConstantsConfig.dynamicValue)
		vkd.cmdSetBlendConstants(cmdBuffer, testConfig.blendConstantsConfig.dynamicValue.get().data());

	if (testConfig.lineStippleParamsConfig.dynamicValue && static_cast<bool>(testConfig.lineStippleParamsConfig.dynamicValue.get()))
	{
		const auto& stippleParams = testConfig.lineStippleParamsConfig.dynamicValue->get();
		vkd.cmdSetLineStippleEXT(cmdBuffer, stippleParams.factor, stippleParams.pattern);
	}

#ifndef CTS_USES_VULKANSC
	if (testConfig.tessDomainOriginConfig.dynamicValue)
		vkd.cmdSetTessellationDomainOriginEXT(cmdBuffer, testConfig.tessDomainOriginConfig.dynamicValue.get());

	if (testConfig.depthClampEnableConfig.dynamicValue)
		vkd.cmdSetDepthClampEnableEXT(cmdBuffer, testConfig.depthClampEnableConfig.dynamicValue.get());

	if (testConfig.polygonModeConfig.dynamicValue)
		vkd.cmdSetPolygonModeEXT(cmdBuffer, testConfig.polygonModeConfig.dynamicValue.get());

	if (testConfig.rasterizationSamplesConfig.dynamicValue)
		vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, testConfig.rasterizationSamplesConfig.dynamicValue.get());

	if (testConfig.sampleMaskConfig.dynamicValue)
	{
		const auto sampleCount	= (static_cast<bool>(testConfig.dynamicSampleMaskCount)
								? testConfig.dynamicSampleMaskCount.get()
								: testConfig.getActiveSampleCount());
		vkd.cmdSetSampleMaskEXT(cmdBuffer, sampleCount, testConfig.sampleMaskConfig.dynamicValue.get().data());
	}

	if (testConfig.alphaToCoverageConfig.dynamicValue)
		vkd.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, makeVkBool32(testConfig.alphaToCoverageConfig.dynamicValue.get()));

	if (testConfig.alphaToOneConfig.dynamicValue)
		vkd.cmdSetAlphaToOneEnableEXT(cmdBuffer, makeVkBool32(testConfig.alphaToOneConfig.dynamicValue.get()));

	if (testConfig.colorWriteMaskConfig.dynamicValue)
	{
		const std::vector<vk::VkColorComponentFlags> writeMasks (testConfig.colorAttachmentCount, testConfig.colorWriteMaskConfig.dynamicValue.get());
		vkd.cmdSetColorWriteMaskEXT(cmdBuffer, 0u, de::sizeU32(writeMasks), de::dataOrNull(writeMasks));
	}

	if (testConfig.rasterizationStreamConfig.dynamicValue && static_cast<bool>(testConfig.rasterizationStreamConfig.dynamicValue.get()))
		vkd.cmdSetRasterizationStreamEXT(cmdBuffer, testConfig.rasterizationStreamConfig.dynamicValue->get());

	if (testConfig.logicOpEnableConfig.dynamicValue)
		vkd.cmdSetLogicOpEnableEXT(cmdBuffer, makeVkBool32(testConfig.logicOpEnableConfig.dynamicValue.get()));

	if (testConfig.colorBlendEnableConfig.dynamicValue)
	{
		const auto colorBlendEnableFlag = makeVkBool32(testConfig.colorBlendEnableConfig.dynamicValue.get());
		const std::vector<vk::VkBool32> flags (testConfig.colorAttachmentCount, colorBlendEnableFlag);
		vkd.cmdSetColorBlendEnableEXT(cmdBuffer, 0u, de::sizeU32(flags), de::dataOrNull(flags));
	}

	if (testConfig.colorBlendEquationConfig.dynamicValue)
	{
		const auto&	configEq	= testConfig.colorBlendEquationConfig.dynamicValue.get();
		const auto	isAdvanced	= testConfig.colorBlendEquationConfig.staticValue.isAdvanced();

		if (isAdvanced || testConfig.colorBlendBoth || testConfig.nullStaticColorBlendAttPtr)
		{
			const vk::VkColorBlendAdvancedEXT equation =
			{
				configEq.colorBlendOp,					//	VkBlendOp			advancedBlendOp;
				VK_TRUE,								//	VkBool32			srcPremultiplied;
				VK_TRUE,								//	VkBool32			dstPremultiplied;
				vk::VK_BLEND_OVERLAP_UNCORRELATED_EXT,	//	VkBlendOverlapEXT	blendOverlap;
				VK_FALSE,								//	VkBool32			clampResults;
			};
			const std::vector<vk::VkColorBlendAdvancedEXT> equations (testConfig.colorAttachmentCount, equation);
			vkd.cmdSetColorBlendAdvancedEXT(cmdBuffer, 0u, de::sizeU32(equations), de::dataOrNull(equations));
		}

		if (!isAdvanced || testConfig.colorBlendBoth)
		{
			// VUID-VkColorBlendEquationEXT-colorBlendOp-07361 forbids colorBlendOp and alphaBlendOp to be any advanced operation.
			// When the advanced blend op will be set by vkCmdSetColorBlendAdvancedEXT, we use a legal placeholder in this call.
			vk::VkBlendOp colorBlendOp = vk::VK_BLEND_OP_ADD;
			vk::VkBlendOp alphaBlendOp = vk::VK_BLEND_OP_ADD;

			if (!isAdvanced)
			{
				colorBlendOp = configEq.colorBlendOp;
				alphaBlendOp = configEq.alphaBlendOp;
			}

			const vk::VkColorBlendEquationEXT equation =
			{
				configEq.srcColorBlendFactor,	//	VkBlendFactor	srcColorBlendFactor;
				configEq.dstColorBlendFactor,	//	VkBlendFactor	dstColorBlendFactor;
				colorBlendOp,					//	VkBlendOp		colorBlendOp;
				configEq.srcAlphaBlendFactor,	//	VkBlendFactor	srcAlphaBlendFactor;
				configEq.dstAlphaBlendFactor,	//	VkBlendFactor	dstAlphaBlendFactor;
				alphaBlendOp,					//	VkBlendOp		alphaBlendOp;
			};
			const std::vector<vk::VkColorBlendEquationEXT> equations (testConfig.colorAttachmentCount, equation);
			vkd.cmdSetColorBlendEquationEXT(cmdBuffer, 0u, de::sizeU32(equations), de::dataOrNull(equations));
		}
	}

	if (testConfig.provokingVertexConfig.dynamicValue && static_cast<bool>(testConfig.provokingVertexConfig.dynamicValue.get()))
	{
		const auto provokingVertexMode = makeProvokingVertexMode(testConfig.provokingVertexConfig.dynamicValue->get());
		vkd.cmdSetProvokingVertexModeEXT(cmdBuffer, provokingVertexMode);
	}

	if (testConfig.negativeOneToOneConfig.dynamicValue && static_cast<bool>(testConfig.negativeOneToOneConfig.dynamicValue.get()))
		vkd.cmdSetDepthClipNegativeOneToOneEXT(cmdBuffer, makeVkBool32(testConfig.negativeOneToOneConfig.dynamicValue->get()));

	if (testConfig.depthClipEnableConfig.dynamicValue && static_cast<bool>(testConfig.depthClipEnableConfig.dynamicValue.get()))
		vkd.cmdSetDepthClipEnableEXT(cmdBuffer, makeVkBool32(testConfig.depthClipEnableConfig.dynamicValue->get()));

	if (testConfig.lineStippleEnableConfig.dynamicValue)
		vkd.cmdSetLineStippleEnableEXT(cmdBuffer, makeVkBool32(testConfig.lineStippleEnableConfig.dynamicValue.get()));

	if (testConfig.sampleLocationsEnableConfig.dynamicValue)
		vkd.cmdSetSampleLocationsEnableEXT(cmdBuffer, makeVkBool32(testConfig.sampleLocationsEnableConfig.dynamicValue.get()));

	if (testConfig.conservativeRasterModeConfig.dynamicValue)
		vkd.cmdSetConservativeRasterizationModeEXT(cmdBuffer, testConfig.conservativeRasterModeConfig.dynamicValue.get());

	if (testConfig.extraPrimitiveOverEstConfig.dynamicValue)
		vkd.cmdSetExtraPrimitiveOverestimationSizeEXT(cmdBuffer, testConfig.extraPrimitiveOverEstConfig.dynamicValue.get());

	if (testConfig.lineRasterModeConfig.dynamicValue && static_cast<bool>(testConfig.lineRasterModeConfig.dynamicValue.get()))
		vkd.cmdSetLineRasterizationModeEXT(cmdBuffer, makeLineRasterizationMode(testConfig.lineRasterModeConfig.dynamicValue->get()));

	if (testConfig.coverageToColorEnableConfig.dynamicValue)
		vkd.cmdSetCoverageToColorEnableNV(cmdBuffer, makeVkBool32(testConfig.coverageToColorEnableConfig.dynamicValue.get()));

	if (testConfig.coverageToColorLocationConfig.dynamicValue)
		vkd.cmdSetCoverageToColorLocationNV(cmdBuffer, testConfig.coverageToColorLocationConfig.dynamicValue.get());

	if (testConfig.coverageModulationModeConfig.dynamicValue)
		vkd.cmdSetCoverageModulationModeNV(cmdBuffer, testConfig.coverageModulationModeConfig.dynamicValue.get());

	if (testConfig.coverageModTableEnableConfig.dynamicValue)
		vkd.cmdSetCoverageModulationTableEnableNV(cmdBuffer, makeVkBool32(testConfig.coverageModTableEnableConfig.dynamicValue.get()));

	if (testConfig.coverageModTableConfig.dynamicValue)
	{
		const auto& tableVec = testConfig.coverageModTableConfig.dynamicValue.get();
		vkd.cmdSetCoverageModulationTableNV(cmdBuffer, static_cast<uint32_t>(tableVec.size()), de::dataOrNull(tableVec));
	}

	if (testConfig.coverageReductionModeConfig.dynamicValue)
		vkd.cmdSetCoverageReductionModeNV(cmdBuffer, testConfig.coverageReductionModeConfig.dynamicValue.get());

	if (testConfig.viewportSwizzleConfig.dynamicValue)
	{
		const auto& viewportSwizzleVec = testConfig.viewportSwizzleConfig.dynamicValue.get();
		vkd.cmdSetViewportSwizzleNV(cmdBuffer, 0u, static_cast<uint32_t>(viewportSwizzleVec.size()), de::dataOrNull(viewportSwizzleVec));
	}

	if (testConfig.shadingRateImageEnableConfig.dynamicValue)
		vkd.cmdSetShadingRateImageEnableNV(cmdBuffer, makeVkBool32(testConfig.shadingRateImageEnableConfig.dynamicValue.get()));

	if (testConfig.viewportWScalingEnableConfig.dynamicValue)
		vkd.cmdSetViewportWScalingEnableNV(cmdBuffer, makeVkBool32(testConfig.viewportWScalingEnableConfig.dynamicValue.get()));

	if (testConfig.reprFragTestEnableConfig.dynamicValue)
		vkd.cmdSetRepresentativeFragmentTestEnableNV(cmdBuffer, makeVkBool32(testConfig.reprFragTestEnableConfig.dynamicValue.get()));

#endif // CTS_USES_VULKANSC
}

// Bind the appropriate vertex buffers using dynamic strides if the test configuration needs a dynamic stride.
// Return true if the vertex buffer was bound.
bool maybeBindVertexBufferDynStride(const TestConfig& testConfig, const vk::DeviceInterface& vkd, vk::VkCommandBuffer cmdBuffer, size_t meshIdx, const std::vector<VertexBufferInfo>& vertBuffers, const std::vector<VertexBufferInfo>& rvertBuffers)
{
	if (!testConfig.strideConfig.dynamicValue)
		return false;

	DE_ASSERT(!testConfig.useMeshShaders);

	const auto& viewportVec = testConfig.getActiveViewportVec();
	DE_UNREF(viewportVec); // For release builds.

	// When dynamically setting the vertex buffer stride, we cannot bind the vertex buffer in advance for some sequence
	// orderings if we have several viewports or meshes.
	DE_ASSERT((viewportVec.size() == 1u && testConfig.meshParams.size() == 1u)
				|| testConfig.sequenceOrdering == SequenceOrdering::BEFORE_DRAW
				|| testConfig.sequenceOrdering == SequenceOrdering::AFTER_PIPELINES);

	// Split buffers, offsets, sizes and strides into their own vectors for the call.
	std::vector<vk::VkBuffer>		buffers;
	std::vector<vk::VkDeviceSize>	offsets;
	std::vector<vk::VkDeviceSize>	sizes;
	const auto						strides = testConfig.strideConfig.dynamicValue.get();

	const auto& chosenBuffers = (testConfig.meshParams[meshIdx].reversed ? rvertBuffers : vertBuffers);

	buffers.reserve	(chosenBuffers.size());
	offsets.reserve	(chosenBuffers.size());
	sizes.reserve	(chosenBuffers.size());
	DE_ASSERT(chosenBuffers.size() == strides.size());

	for (const auto& vertBuffer : chosenBuffers)
	{
		buffers.push_back	(vertBuffer.buffer->get());
		offsets.push_back	(vertBuffer.offset);
		sizes.push_back		(vertBuffer.dataSize);
	}

#ifndef CTS_USES_VULKANSC
	vkd.cmdBindVertexBuffers2(cmdBuffer, 0u, static_cast<deUint32>(chosenBuffers.size()), buffers.data(), offsets.data(), sizes.data(), strides.data());
#else
	vkd.cmdBindVertexBuffers2EXT(cmdBuffer, 0u, static_cast<deUint32>(chosenBuffers.size()), buffers.data(), offsets.data(), sizes.data(), strides.data());
#endif // CTS_USES_VULKANSC

	return true;
}

// Bind the given vertex buffers with the non-dynamic call. Similar to maybeBindVertexBufferDynStride but simpler.
void bindVertexBuffers (const vk::DeviceInterface& vkd, vk::VkCommandBuffer cmdBuffer, const std::vector<VertexBufferInfo>& vertexBuffers)
{
	std::vector<vk::VkBuffer>		buffers;
	std::vector<vk::VkDeviceSize>	offsets;

	buffers.reserve	(vertexBuffers.size());
	offsets.reserve	(vertexBuffers.size());

	for (const auto& vertBuffer : vertexBuffers)
	{
		buffers.push_back	(vertBuffer.buffer->get());
		offsets.push_back	(vertBuffer.offset);
	}

	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, static_cast<deUint32>(vertexBuffers.size()), buffers.data(), offsets.data());
}

// Create a vector of VertexBufferInfo elements using the given vertex generator and set of vertices.
void prepareVertexBuffers (	std::vector<VertexBufferInfo>&	buffers,
							const vk::DeviceInterface&		vkd,
							vk::VkDevice					device,
							vk::Allocator&					allocator,
							const VertexGenerator*			generator,
							const std::vector<tcu::Vec2>&	vertices,
							deUint32						dataOffset,
							deUint32						trailingSize,
							bool							ssbos)
{
	const deUint32	paddingBytes	= 0xDEADBEEFu;
	const auto		vertexData		= generator->createVertexData(vertices, dataOffset, trailingSize, &paddingBytes, sizeof(paddingBytes));

	for (const auto& bufferBytes : vertexData)
	{
		const auto bufferSize	= static_cast<vk::VkDeviceSize>(de::dataSize(bufferBytes));
		const auto extraSize	= static_cast<vk::VkDeviceSize>(dataOffset + trailingSize);
		DE_ASSERT(bufferSize > extraSize);
		const auto dataSize		= bufferSize - extraSize;

		// Create a full-size buffer but remember the data size and offset for it.
		const auto createInfo = vk::makeBufferCreateInfo(bufferSize, (ssbos ? vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));

		VertexBufferInfo bufferInfo;
		bufferInfo.buffer	= BufferWithMemoryPtr(new vk::BufferWithMemory(vkd, device, allocator, createInfo, vk::MemoryRequirement::HostVisible));
		bufferInfo.offset	= static_cast<vk::VkDeviceSize>(dataOffset);
		bufferInfo.dataSize	= dataSize;
		buffers.emplace_back(std::move(bufferInfo));

		// Copy the whole contents to the full buffer.
		copyAndFlush(vkd, device, *buffers.back().buffer, 0, bufferBytes.data(), de::dataSize(bufferBytes));
	}
}

// Device helper: this is needed in some tests when we create custom devices.
class DeviceHelper
{
public:
	virtual ~DeviceHelper () {}
	virtual const vk::DeviceInterface&		getDeviceInterface	(void) const = 0;
	virtual vk::VkDevice					getDevice			(void) const = 0;
	virtual uint32_t						getQueueFamilyIndex	(void) const = 0;
	virtual vk::VkQueue						getQueue			(void) const = 0;
	virtual vk::Allocator&					getAllocator		(void) const = 0;
	virtual const std::vector<std::string>&	getDeviceExtensions	(void) const = 0;
};

// This one just reuses the default device from the context.
class ContextDeviceHelper : public DeviceHelper
{
public:
	ContextDeviceHelper (Context& context)
		: m_deviceInterface		(context.getDeviceInterface())
		, m_device				(context.getDevice())
		, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
		, m_queue				(context.getUniversalQueue())
		, m_allocator			(context.getDefaultAllocator())
		, m_extensions			(context.getDeviceExtensions())
		{}

	virtual ~ContextDeviceHelper () {}

	const vk::DeviceInterface&		getDeviceInterface	(void) const override	{ return m_deviceInterface;		}
	vk::VkDevice					getDevice			(void) const override	{ return m_device;				}
	uint32_t						getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	vk::VkQueue						getQueue			(void) const override	{ return m_queue;				}
	vk::Allocator&					getAllocator		(void) const override	{ return m_allocator;			}
	const std::vector<std::string>& getDeviceExtensions	(void) const override	{ return m_extensions; }

protected:
	const vk::DeviceInterface&	m_deviceInterface;
	const vk::VkDevice			m_device;
	const uint32_t				m_queueFamilyIndex;
	const vk::VkQueue			m_queue;
	vk::Allocator&				m_allocator;
	std::vector<std::string>	m_extensions;
};

// This one creates a new device with VK_NV_shading_rate_image and VK_EXT_extended_dynamic_state3.
// It also enables other extensions like VK_EXT_mesh_shader if supported, as some tests need them.
class ShadingRateImageDeviceHelper : public DeviceHelper
{
public:
	ShadingRateImageDeviceHelper (Context& context)
	{
		const auto&	vkp				= context.getPlatformInterface();
		const auto&	vki				= context.getInstanceInterface();
		const auto	instance		= context.getInstance();
		const auto	physicalDevice	= context.getPhysicalDevice();
		const auto	queuePriority	= 1.0f;

		// Queue index first.
		m_queueFamilyIndex = context.getUniversalQueueFamilyIndex();

		// Create a universal queue that supports graphics and compute.
		const vk::VkDeviceQueueCreateInfo queueParams =
		{
			vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkDeviceQueueCreateFlags		flags;
			m_queueFamilyIndex,								// deUint32						queueFamilyIndex;
			1u,												// deUint32						queueCount;
			&queuePriority									// const float*					pQueuePriorities;
		};

#ifndef CTS_USES_VULKANSC
		const auto&	contextMeshFeatures	= context.getMeshShaderFeaturesEXT();
		const auto& contextGPLFeatures	= context.getGraphicsPipelineLibraryFeaturesEXT();
		const auto& contextDBCFeatures	= context.getDepthBiasControlFeaturesEXT();
		const auto&	contextSOFeatures	= context.getShaderObjectFeaturesEXT();

		const bool	meshShaderSupport	= contextMeshFeatures.meshShader;
		const bool	gplSupport			= contextGPLFeatures.graphicsPipelineLibrary;
		const bool	dbcSupport			= contextDBCFeatures.depthBiasControl;
		const bool  shaderObjectSupport = contextSOFeatures.shaderObject;

		vk::VkPhysicalDeviceExtendedDynamicState3FeaturesEXT	eds3Features				= vk::initVulkanStructure();
		vk::VkPhysicalDeviceShadingRateImageFeaturesNV			shadingRateImageFeatures	= vk::initVulkanStructure(&eds3Features);
		vk::VkPhysicalDeviceFeatures2							features2					= vk::initVulkanStructure(&shadingRateImageFeatures);

		vk::VkPhysicalDeviceDepthBiasControlFeaturesEXT			dbcFeatures					= vk::initVulkanStructure();
		vk::VkPhysicalDeviceMeshShaderFeaturesEXT				meshFeatures				= vk::initVulkanStructure();
		vk::VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT	gplFeatures					= vk::initVulkanStructure();
		vk::VkPhysicalDeviceShaderObjectFeaturesEXT				shaderObjectFeatures		= vk::initVulkanStructure();

		const auto addFeatures = vk::makeStructChainAdder(&features2);

		if (meshShaderSupport)
			addFeatures(&meshFeatures);

		if (gplSupport)
			addFeatures(&gplFeatures);

		if (dbcSupport)
			addFeatures(&dbcFeatures);

		if (shaderObjectSupport)
			addFeatures(&shaderObjectFeatures);

		vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
#endif // CTS_USES_VULKANSC

		std::vector<const char*> extensions
		{
			"VK_EXT_extended_dynamic_state3",
			"VK_NV_shading_rate_image",
		};

#ifndef CTS_USES_VULKANSC
		if (meshShaderSupport)
			extensions.push_back("VK_EXT_mesh_shader");

		if (gplSupport)
		{
			extensions.push_back("VK_KHR_pipeline_library");
			extensions.push_back("VK_EXT_graphics_pipeline_library");
		}

		if (dbcSupport)
			extensions.push_back("VK_EXT_depth_bias_control");

		if (shaderObjectSupport)
			extensions.push_back("VK_EXT_shader_object");

		// Disable robustness.
		features2.features.robustBufferAccess = VK_FALSE;
#endif // CTS_USES_VULKANSC

		for (const auto& ext : extensions)
			m_extensions.push_back(ext);

		const vk::VkDeviceCreateInfo deviceCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,				//sType;
#ifndef CTS_USES_VULKANSC
			&features2,												//pNext;
#else
			nullptr,
#endif // CTS_USES_VULKANSC
			0u,														//flags
			1u,														//queueRecordCount;
			&queueParams,											//pRequestedQueues;
			0u,														//layerCount;
			nullptr,												//ppEnabledLayerNames;
			de::sizeU32(extensions),								// deUint32							enabledExtensionCount;
			de::dataOrNull(extensions),								// const char* const*				ppEnabledExtensionNames;
			nullptr,												//pEnabledFeatures;
		};

		m_device	= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &deviceCreateInfo);
		m_vkd		.reset(new vk::DeviceDriver(vkp, instance, m_device.get(), context.getUsedApiVersion()));
		m_queue		= getDeviceQueue(*m_vkd, *m_device, m_queueFamilyIndex, 0u);
		m_allocator	.reset(new vk::SimpleAllocator(*m_vkd, m_device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}

	virtual ~ShadingRateImageDeviceHelper () {}

	const vk::DeviceInterface&		getDeviceInterface	(void) const override	{ return *m_vkd;				}
	vk::VkDevice					getDevice			(void) const override	{ return m_device.get();		}
	uint32_t						getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	vk::VkQueue						getQueue			(void) const override	{ return m_queue;				}
	vk::Allocator&					getAllocator		(void) const override	{ return *m_allocator;			}
	const std::vector<std::string>&	getDeviceExtensions	(void) const override	{ return m_extensions;			}

protected:
	vk::Move<vk::VkDevice>					m_device;
	std::unique_ptr<vk::DeviceDriver>		m_vkd;
	deUint32								m_queueFamilyIndex;
	vk::VkQueue								m_queue;
	std::unique_ptr<vk::SimpleAllocator>	m_allocator;
	std::vector<std::string>				m_extensions;
};

std::unique_ptr<DeviceHelper> g_shadingRateDeviceHelper;
std::unique_ptr<DeviceHelper> g_contextDeviceHelper;

DeviceHelper& getDeviceHelper(Context& context, const TestConfig& testConfig)
{
	if (testConfig.shadingRateImage)
	{
		if (!g_shadingRateDeviceHelper)
			g_shadingRateDeviceHelper.reset(new ShadingRateImageDeviceHelper(context));
		return *g_shadingRateDeviceHelper;
	}

	if (!g_contextDeviceHelper)
		g_contextDeviceHelper.reset(new ContextDeviceHelper(context));
	return *g_contextDeviceHelper;
}

void cleanupDevices()
{
	g_shadingRateDeviceHelper.reset(nullptr);
	g_contextDeviceHelper.reset(nullptr);
}

tcu::TextureChannelClass getChannelClass (const tcu::TextureFormat& format)
{
	const auto generalClass = getTextureChannelClass(format.type);
	// Workaround for VK_FORMAT_X8_D24_UNORM_PACK32.
	return ((generalClass == tcu::TEXTURECHANNELCLASS_LAST) ? tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT : generalClass);
}

tcu::TestStatus ExtendedDynamicStateInstance::iterate (void)
{
	using ImageWithMemoryVec	= std::vector<std::unique_ptr<vk::ImageWithMemory>>;
	using ImageViewVec			= std::vector<vk::Move<vk::VkImageView>>;
	using RenderPassVec			= std::vector<vk::RenderPassWrapper>;

	const auto&	vki					= m_context.getInstanceInterface();
	const auto	physicalDevice		= m_context.getPhysicalDevice();
	const auto& deviceHelper		= getDeviceHelper(m_context, m_testConfig);
	const auto&	vkd					= deviceHelper.getDeviceInterface();
	const auto	device				= deviceHelper.getDevice();
	auto&		allocator			= deviceHelper.getAllocator();
	const auto	queue				= deviceHelper.getQueue();
	const auto	queueIndex			= deviceHelper.getQueueFamilyIndex();
	auto&		log					= m_context.getTestContext().getLog();

	const auto	kReversed			= m_testConfig.isReversed();
	const auto	kBindStaticFirst	= m_testConfig.bindStaticFirst();
	const auto	kUseStaticPipeline	= m_testConfig.useStaticPipeline();
	const auto	kNumIterations		= m_testConfig.numIterations();
	const auto	kColorAttCount		= m_testConfig.colorAttachmentCount;
	const auto	kSequenceOrdering	= m_testConfig.sequenceOrdering;

	const auto	kDSCreateFlags		= (m_testConfig.sampleLocationsStruct() ? static_cast<vk::VkImageCreateFlags>(vk::VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT) : 0u);
	const auto	colorFormat			= m_testConfig.colorFormat();
	const auto	colorSampleCount	= m_testConfig.getColorSampleCount();
	const auto	activeSampleCount	= m_testConfig.getActiveSampleCount();
	const bool	vertDataAsSSBO		= m_testConfig.useMeshShaders;
	const auto	pipelineBindPoint	= vk::VK_PIPELINE_BIND_POINT_GRAPHICS;
	const bool	kUseResolveAtt		= (colorSampleCount != kSingleSampleCount);
	const bool	kMultisampleDS		= (activeSampleCount != kSingleSampleCount);
	const bool	kFragAtomics		= m_testConfig.useFragShaderAtomics();

	// Choose depth/stencil format.
	const DepthStencilFormat* dsFormatInfo = nullptr;

	for (const auto& kDepthStencilFormat : kDepthStencilFormats)
	{
		// This is how we'll attempt to create images later.
		const auto dsImageInfo = makeImageCreateInfo(kDepthStencilFormat.imageFormat, kFramebufferExtent, activeSampleCount, kDSUsage, kDSCreateFlags);

		vk::VkImageFormatProperties formatProps;
		const auto result = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, dsImageInfo.format, dsImageInfo.imageType, dsImageInfo.tiling, dsImageInfo.usage, dsImageInfo.flags, &formatProps);

		// Format not supported.
		if (result != vk::VK_SUCCESS)
			continue;

		// Extent not big enough.
		const auto& maxExtent = formatProps.maxExtent;
		if (maxExtent.width < kFramebufferExtent.width || maxExtent.height < kFramebufferExtent.height || maxExtent.depth < kFramebufferExtent.depth)
			continue;

		// Sample count not supported.
		if ((formatProps.sampleCounts & activeSampleCount) != activeSampleCount)
			continue;

		if (m_testConfig.neededDepthChannelClass != tcu::TEXTURECHANNELCLASS_LAST)
		{
			const auto tcuDSFormat	= vk::getDepthCopyFormat(kDepthStencilFormat.imageFormat);
			const auto channelClass	= getChannelClass(tcuDSFormat);

			if (channelClass != m_testConfig.neededDepthChannelClass)
				continue;
		}

		dsFormatInfo = &kDepthStencilFormat;
		break;
	}

	// Note: Not Supported insted of Fail because some features are not mandatory.
	if (!dsFormatInfo)
		TCU_THROW(NotSupportedError, "Required depth/stencil image features not supported");
	log << tcu::TestLog::Message << "Chosen depth/stencil format: " << dsFormatInfo->imageFormat << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Chosen color format: " << colorFormat << tcu::TestLog::EndMessage;

	// Swap static and dynamic values in the test configuration so the static pipeline ends up with the expected values for cases
	// where we will bind the static pipeline last before drawing.
	if (kReversed)
		m_testConfig.swapValues();

	// Create color and depth/stencil images.
	ImageWithMemoryVec colorImages;
	ImageWithMemoryVec dsImages;
	ImageWithMemoryVec resolveImages;

	const auto colorImageInfo = makeImageCreateInfo(colorFormat, kFramebufferExtent, colorSampleCount, kColorUsage, 0u);
	for (deUint32 i = 0u; i < kNumIterations * kColorAttCount; ++i)
		colorImages.emplace_back(new vk::ImageWithMemory(vkd, device, allocator, colorImageInfo, vk::MemoryRequirement::Any));

	const auto dsImageInfo = makeImageCreateInfo(dsFormatInfo->imageFormat, kFramebufferExtent, activeSampleCount, kDSUsage, kDSCreateFlags);
	for (deUint32 i = 0u; i < kNumIterations; ++i)
		dsImages.emplace_back(new vk::ImageWithMemory(vkd, device, allocator, dsImageInfo, vk::MemoryRequirement::Any));

	if (kUseResolveAtt)
	{
		const auto resolveImageInfo = makeImageCreateInfo(colorFormat, kFramebufferExtent, kSingleSampleCount, kColorUsage, 0u);
		for (uint32_t i = 0u; i < kNumIterations * kColorAttCount; ++i)
			resolveImages.emplace_back(new vk::ImageWithMemory(vkd, device, allocator, resolveImageInfo, vk::MemoryRequirement::Any));
	}

	const auto colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto dsSubresourceRange		= vk::makeImageSubresourceRange((vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);

	ImageViewVec colorImageViews;
	ImageViewVec dsImageViews;
	ImageViewVec resolveImageViews;

	for (const auto& img : colorImages)
		colorImageViews.emplace_back(vk::makeImageView(vkd, device, img->get(), vk::VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	for (const auto& img : dsImages)
		dsImageViews.emplace_back(vk::makeImageView(vkd, device, img->get(), vk::VK_IMAGE_VIEW_TYPE_2D, dsFormatInfo->imageFormat, dsSubresourceRange));

	for (const auto& img : resolveImages)
		resolveImageViews.emplace_back(vk::makeImageView(vkd, device, img->get(), vk::VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	// Vertex buffer.
	const auto				topologyClass	= getTopologyClass(m_testConfig.topologyConfig.staticValue);
	std::vector<uint32_t>	indices;
	std::vector<tcu::Vec2>	vertices;

	if (m_testConfig.oversizedTriangle || m_testConfig.offCenterTriangle)
	{
		DE_ASSERT(topologyClass == TopologyClass::TRIANGLE);
		DE_ASSERT(!m_testConfig.singleVertex);
	}

	if (m_testConfig.obliqueLine)
		DE_ASSERT(topologyClass == TopologyClass::LINE);

	if (topologyClass == TopologyClass::TRIANGLE)
	{
		DE_ASSERT(!m_testConfig.needsIndexBuffer());

		if (m_testConfig.oversizedTriangle)
		{
			vertices.reserve(3u);
			vertices.push_back(tcu::Vec2(-2.0f, -2.0f));
			vertices.push_back(tcu::Vec2(-2.0f,  6.0f));
			vertices.push_back(tcu::Vec2( 6.0f, -2.0f));
		}
		else if (m_testConfig.offCenterTriangle)
		{
			// Triangle covering the whole screen, except for the first row and column, which may not be covered by all samples.
			const float horOffset	= 2.0f / static_cast<float>(kFramebufferWidth) * m_testConfig.offCenterProportion.x();
			const float vertOffset	= 2.0f / static_cast<float>(kFramebufferHeight) * m_testConfig.offCenterProportion.y();

			vertices.reserve(3u);
			vertices.push_back(tcu::Vec2(-1.0f + horOffset, -1.0f + vertOffset));
			vertices.push_back(tcu::Vec2(-1.0f + horOffset,               4.0f));
			vertices.push_back(tcu::Vec2(             4.0f, -1.0f + vertOffset));
		}
		else
		{
			// Full-screen triangle strip with 6 vertices.
			//
			// 0        2        4
			//  +-------+-------+
			//  |      XX      X|
			//  |     X X     X |
			//  |    X  X    X  |
			//  |   X   X   X   |
			//  |  X    X  X    |
			//  | X     X X     |
			//  |X      XX      |
			//  +-------+-------+
			// 1        3       5
			vertices.reserve(6u);
			vertices.push_back(tcu::Vec2(-1.0f, -1.0f));
			vertices.push_back(tcu::Vec2(-1.0f,  1.0f));
			vertices.push_back(tcu::Vec2( 0.0f, -1.0f));
			vertices.push_back(tcu::Vec2( 0.0f,  1.0f));
			vertices.push_back(tcu::Vec2( 1.0f, -1.0f));
			vertices.push_back(tcu::Vec2( 1.0f,  1.0f));
		}
	}
	else if (topologyClass == TopologyClass::PATCH)
	{
		DE_ASSERT(!m_testConfig.needsIndexBuffer());
		DE_ASSERT(m_testConfig.getActivePatchControlPoints() > 1u);

		// 2 triangles making a quad
		vertices.reserve(6u);
		vertices.push_back(tcu::Vec2(-1.0f,  1.0f));
		vertices.push_back(tcu::Vec2( 1.0f,  1.0f));
		vertices.push_back(tcu::Vec2( 1.0f, -1.0f));
		vertices.push_back(tcu::Vec2( 1.0f, -1.0f));
		vertices.push_back(tcu::Vec2(-1.0f, -1.0f));
		vertices.push_back(tcu::Vec2(-1.0f,  1.0f));
	}
	else // TopologyClass::LINE
	{
		const float pixelHeight	= 2.0f / static_cast<float>(kFramebufferHeight);
		const float pixelWidth	= 2.0f / static_cast<float>(kFramebufferWidth);

		if (m_testConfig.obliqueLine)
		{
			// The starting point of the oblique line is located in the top left pixel, in a position below and slightly to the left
			// of the pixel center. The ending point is in the middle of the right side of the framebuffer. Those coordinates make
			// sure that a bresenham-style line covers the center of the top left pixel, because the left edge of the line goes up
			// vertically from that point. However, a rectangular line misses it by a small delta because its edge goes up and to
			// the right, leaving the pixel center to its left. So the top left pixel itself may be covered or not depending on the
			// active line rasterization mode.
			//
			// Note: results may also be affected by multisample and sample locations if those are used.
			vertices.reserve(2u);
			vertices.push_back(tcu::Vec2(pixelWidth * 7.0f / 16.0f - 1.0f, pixelHeight * 12.0f / 16.0f - 1.0f));
			vertices.push_back(tcu::Vec2(1.0f, 0.0f));
		}
		else
		{
			DE_ASSERT(m_testConfig.getActivePrimRestartEnable());

			// Draw one segmented line per output row of pixels that could be wrongly interpreted as a list of lines that would not cover the whole screen.
			vertices.reserve(kFramebufferHeight * 4u);

			const auto indicesPerRow = (m_testConfig.extraLineRestarts ? 6u : 5u);
			if (m_testConfig.needsIndexBuffer())
				indices.reserve(kFramebufferHeight * indicesPerRow);

			for (deUint32 rowIdx = 0; rowIdx < kFramebufferHeight; ++rowIdx)
			{
				// Offset of 0.5 pixels + one pixel per row, from -1 to 1.
				const float yCoord = (pixelHeight / 2.0f) + pixelHeight * static_cast<float>(rowIdx) - 1.0f;
				vertices.push_back(tcu::Vec2(-1.0f, yCoord));
				vertices.push_back(tcu::Vec2(-0.5f, yCoord));
				vertices.push_back(tcu::Vec2( 0.5f, yCoord));
				vertices.push_back(tcu::Vec2( 1.0f, yCoord));

				if (m_testConfig.needsIndexBuffer())
				{
					indices.push_back(4u * rowIdx + 0u);
					indices.push_back(4u * rowIdx + 1u);

					// When using extra line restarts, insert a primitive restart index in the middle, which will result in the
					// center strip being skipped, as if the topology was a line list instead of a strip.
					if (m_testConfig.extraLineRestarts)
						indices.push_back(0xFFFFFFFFu);

					indices.push_back(4u * rowIdx + 2u);
					indices.push_back(4u * rowIdx + 3u);
					indices.push_back(0xFFFFFFFFu); // Restart line strip.
				}
			}
		}
	}

	if (m_testConfig.singleVertex)
	{
		DE_ASSERT(!m_testConfig.needsIndexBuffer());
		vertices.resize(1);
	}

	// Reversed vertices order in triangle strip (1, 0, 3, 2, 5, 4)
	std::vector<tcu::Vec2> rvertices;
	if (topologyClass == TopologyClass::TRIANGLE)
	{
		DE_ASSERT(!vertices.empty());
		if (m_testConfig.singleVertex)
			rvertices.push_back(vertices[0]);
		else if (m_testConfig.oversizedTriangle || m_testConfig.offCenterTriangle)
		{
			rvertices.reserve(3u);
			rvertices.push_back(vertices[0]);
			rvertices.push_back(vertices[2]);
			rvertices.push_back(vertices[1]);
		}
		else
		{
			rvertices.reserve(6u);
			rvertices.push_back(vertices[1]);
			rvertices.push_back(vertices[0]);
			rvertices.push_back(vertices[3]);
			rvertices.push_back(vertices[2]);
			rvertices.push_back(vertices[5]);
			rvertices.push_back(vertices[4]);
		}
	}

	if (topologyClass != TopologyClass::TRIANGLE)
	{
		for (const auto& mesh : m_testConfig.meshParams)
		{
			DE_UNREF(mesh); // For release builds.
			DE_ASSERT(!mesh.reversed);
		}
	}

	// Buffers with vertex data for the different bindings.
	std::vector<VertexBufferInfo> vertBuffers;
	std::vector<VertexBufferInfo> rvertBuffers;

	{
		const auto dataOffset	= static_cast<deUint32>(m_testConfig.vertexDataOffset);
		const auto trailingSize	= static_cast<deUint32>(m_testConfig.vertexDataExtraBytes);
		const auto generator	= m_testConfig.getActiveVertexGenerator();
		prepareVertexBuffers(vertBuffers, vkd, device, allocator, generator, vertices, dataOffset, trailingSize, vertDataAsSSBO);
		if (topologyClass == TopologyClass::TRIANGLE)
			prepareVertexBuffers(rvertBuffers, vkd, device, allocator, generator, rvertices, dataOffset, trailingSize, vertDataAsSSBO);
	}

	// Index buffer.
	BufferWithMemoryPtr indexBuffer;
	if (!indices.empty())
	{
		const auto indexDataSize	= static_cast<vk::VkDeviceSize>(de::dataSize(indices));
		const auto indexBufferInfo	= vk::makeBufferCreateInfo(indexDataSize, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

		indexBuffer = BufferWithMemoryPtr(new vk::BufferWithMemory(vkd, device, allocator, indexBufferInfo, vk::MemoryRequirement::HostVisible));
		copyAndFlush(vkd, device, *indexBuffer, 0, indices.data(), static_cast<size_t>(indexDataSize));
	}

	// Fragment counter buffer.
	BufferWithMemoryPtr	counterBuffer;
	const auto			counterBufferSize	= static_cast<vk::VkDeviceSize>(sizeof(uint32_t));

	if (kFragAtomics)
	{
		const auto		counterBufferInfo	= vk::makeBufferCreateInfo(counterBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		const uint32_t	initialValue		= 0u;

		counterBuffer = BufferWithMemoryPtr(new vk::BufferWithMemory(vkd, device, allocator, counterBufferInfo, vk::MemoryRequirement::HostVisible));
		copyAndFlush(vkd, device, *counterBuffer, 0u, &initialValue, static_cast<size_t>(counterBufferSize));
	}

	// Frag shader descriptor set layout.
	vk::Move<vk::VkDescriptorSetLayout> fragSetLayout;
	{
		vk::DescriptorSetLayoutBuilder layoutBuilder;
		if (kFragAtomics)
			layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
		fragSetLayout = layoutBuilder.build(vkd, device);
	}

	// Descriptor pool and set.
	vk::Move<vk::VkDescriptorPool>	fragDescriptorPool;
	vk::Move<vk::VkDescriptorSet>	fragDescriptorSet;

	if (kFragAtomics)
	{
		vk::DescriptorPoolBuilder poolBuilder;
		poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		fragDescriptorPool	= poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		fragDescriptorSet	= vk::makeDescriptorSet(vkd, device, fragDescriptorPool.get(), fragSetLayout.get());

		vk::DescriptorSetUpdateBuilder updateBuilder;
		const auto location = vk::DescriptorSetUpdateBuilder::Location::binding(0u);
		const auto descInfo = vk::makeDescriptorBufferInfo(counterBuffer->get(), 0ull, counterBufferSize);
		updateBuilder.writeSingle(fragDescriptorSet.get(), location, vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descInfo);
		updateBuilder.update(vkd, device);
	}

	// Push constant stages (matches SSBO stages if used).
	vk::VkShaderStageFlags pushConstantStageFlags = (
		(m_testConfig.useMeshShaders
#ifndef CTS_USES_VULKANSC
		 ? vk::VK_SHADER_STAGE_MESH_BIT_EXT
#else
		 ? 0
#endif // CTS_USES_VULKANSC
		 : vk::VK_SHADER_STAGE_VERTEX_BIT)
		| vk::VK_SHADER_STAGE_FRAGMENT_BIT);

	if (m_testConfig.needsGeometryShader())
		pushConstantStageFlags |= vk::VK_SHADER_STAGE_GEOMETRY_BIT;

	// Mesh descriptor set layout.
	vk::Move<vk::VkDescriptorSetLayout> meshSetLayout;
	if (vertDataAsSSBO)
	{
		vk::DescriptorSetLayoutBuilder layoutBuilder;
		for (size_t i = 0; i < vertBuffers.size(); ++i)
			layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, pushConstantStageFlags);
		meshSetLayout = layoutBuilder.build(vkd, device);
	}

	// Descriptor pool and set if needed.
	vk::Move<vk::VkDescriptorPool>	meshDescriptorPool;
	vk::Move<vk::VkDescriptorSet>	meshDescriptorSet;
	vk::Move<vk::VkDescriptorSet>	meshDescriptorSetRev;

	if (vertDataAsSSBO)
	{
		const auto					hasReversed		= (rvertBuffers.size() > 0u);
		const auto					descType		= vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		vk::DescriptorPoolBuilder	poolBuilder;
		poolBuilder.addType(descType, static_cast<uint32_t>(vertBuffers.size()) * 2u);

		meshDescriptorPool		= poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);
		meshDescriptorSet		= vk::makeDescriptorSet(vkd, device, meshDescriptorPool.get(), meshSetLayout.get());

		if (hasReversed)
			meshDescriptorSetRev = vk::makeDescriptorSet(vkd, device, meshDescriptorPool.get(), meshSetLayout.get());

		std::vector<vk::VkDescriptorBufferInfo> descBufferInfos;
		std::vector<vk::VkDescriptorBufferInfo> descBufferInfosRev;
		descBufferInfos.reserve(vertBuffers.size());
		if (hasReversed)
			descBufferInfosRev.reserve(rvertBuffers.size());

		vk::DescriptorSetUpdateBuilder updateBuilder;

		DE_ASSERT(vertBuffers.size() == rvertBuffers.size() || !hasReversed);
		for (size_t i = 0; i < vertBuffers.size(); ++i)
		{
			const auto binding = vk::DescriptorSetUpdateBuilder::Location::binding(static_cast<uint32_t>(i));

			descBufferInfos.push_back(vk::makeDescriptorBufferInfo(vertBuffers[i].buffer->get(), vertBuffers[i].offset, vertBuffers[i].dataSize));
			updateBuilder.writeSingle(meshDescriptorSet.get(), binding, descType, &descBufferInfos.back());

			if (hasReversed)
			{
				descBufferInfosRev.push_back(vk::makeDescriptorBufferInfo(rvertBuffers[i].buffer->get(), rvertBuffers[i].offset, rvertBuffers[i].dataSize));
				updateBuilder.writeSingle(meshDescriptorSetRev.get(), binding, descType, &descBufferInfosRev.back());
			}
		}

		updateBuilder.update(vkd, device);
	}

	// The frag shader descriptor set is the second one if both exist. See getFragDescriptorSetIndex().
	std::vector<vk::VkDescriptorSetLayout> rawSetLayouts;

	if (meshSetLayout.get() != VK_NULL_HANDLE)
		rawSetLayouts.push_back(meshSetLayout.get());

	if (fragSetLayout.get() != VK_NULL_HANDLE)
		rawSetLayouts.push_back(fragSetLayout.get());

	// Pipeline layout.
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
		de::sizeU32(rawSetLayouts),							//	deUint32						setLayoutCount;
		de::dataOrNull(rawSetLayouts),						//	const VkDescriptorSetLayout*	pSetLayouts;
		1u,													//	deUint32						pushConstantRangeCount;
		&pushConstantRange,									//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const vk::PipelineLayoutWrapper pipelineLayout (m_testConfig.pipelineConstructionType, vkd, device, &pipelineLayoutCreateInfo);

	// Render pass with single subpass. Attachment order:
	// 1) Color attachments (kColorAttCount items).
	// 2) DS attachment.
	// 3) [optional] Resolve attachments (kColorAttCount).

	DE_ASSERT(kColorAttCount > 0u);

	std::vector<vk::VkAttachmentReference> colorAttachments;
	std::vector<vk::VkAttachmentReference> resolveAttachments;

	for (uint32_t colorAttIdx = 0u; colorAttIdx < kColorAttCount; ++colorAttIdx)
	{
		colorAttachments.push_back(vk::makeAttachmentReference(colorAttIdx, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
		if (kUseResolveAtt)
			resolveAttachments.push_back(vk::makeAttachmentReference(kColorAttCount + 1u + colorAttIdx, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	}

	const vk::VkAttachmentReference dsAttachmentReference =
	{
		kColorAttCount,											//	deUint32		attachment;
		vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	//	VkImageLayout	layout;
	};

	const vk::VkSubpassDescription subpassDescription =
	{
		0u,										//	VkSubpassDescriptionFlags		flags;
		pipelineBindPoint,						//	VkPipelineBindPoint				pipelineBindPoint;
		0u,										//	deUint32						inputAttachmentCount;
		nullptr,								//	const VkAttachmentReference*	pInputAttachments;
		kColorAttCount,							//	deUint32						colorAttachmentCount;
		de::dataOrNull(colorAttachments),		//	const VkAttachmentReference*	pColorAttachments;
		de::dataOrNull(resolveAttachments),		//	const VkAttachmentReference*	pResolveAttachments;
		&dsAttachmentReference,					//	const VkAttachmentReference*	pDepthStencilAttachment;
		0u,										//	deUint32						preserveAttachmentCount;
		nullptr,								//	const deUint32*					pPreserveAttachments;
	};

	std::vector<vk::VkAttachmentDescription> attachmentDescriptions;

	// For multisample, we care about the resolve attachment, not the color one.
	const auto colorAttachmentStoreOp = (kUseResolveAtt ? vk::VK_ATTACHMENT_STORE_OP_DONT_CARE : vk::VK_ATTACHMENT_STORE_OP_STORE);

	for (uint32_t colorAttIdx = 0u; colorAttIdx < kColorAttCount; ++colorAttIdx)
	{
		attachmentDescriptions.push_back(vk::VkAttachmentDescription
		{
			0u,												//	VkAttachmentDescriptionFlags	flags;
			colorFormat,									//	VkFormat						format;
			colorSampleCount,								//	VkSampleCountFlagBits			samples;
			vk::VK_ATTACHMENT_LOAD_OP_CLEAR,				//	VkAttachmentLoadOp				loadOp;
			colorAttachmentStoreOp,							//	VkAttachmentStoreOp				storeOp;
			vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
			vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
		});
	}

	attachmentDescriptions.push_back(vk::VkAttachmentDescription
	{
		0u,														//	VkAttachmentDescriptionFlags	flags;
		dsFormatInfo->imageFormat,								//	VkFormat						format;
		activeSampleCount,										//	VkSampleCountFlagBits			samples;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						//	VkAttachmentLoadOp				loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						//	VkAttachmentStoreOp				storeOp;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						//	VkAttachmentLoadOp				stencilLoadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						//	VkAttachmentStoreOp				stencilStoreOp;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,							//	VkImageLayout					initialLayout;
		vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
	});

	if (kUseResolveAtt)
	{
		// Resolve attachments.
		for (uint32_t colorAttIdx = 0u; colorAttIdx < kColorAttCount; ++colorAttIdx)
		{
			attachmentDescriptions.push_back(vk::VkAttachmentDescription
			{
				0u,												//	VkAttachmentDescriptionFlags	flags;
				colorFormat,									//	VkFormat						format;
				kSingleSampleCount,								//	VkSampleCountFlagBits			samples;
				vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				loadOp;
				vk::VK_ATTACHMENT_STORE_OP_STORE,				//	VkAttachmentStoreOp				storeOp;
				vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
				vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
				vk::VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
				vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
			});
		}
	}

	// Render pass and framebuffers.
	RenderPassVec renderPassFramebuffers;

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

	DE_ASSERT(colorImageViews.size() == dsImageViews.size() * kColorAttCount);

	if (kUseResolveAtt)
		DE_ASSERT(colorImageViews.size() == resolveImageViews.size());

	for (size_t iterIdx = 0; iterIdx < dsImageViews.size(); ++iterIdx)
	{
		std::vector<vk::VkImage>		images;
		std::vector<vk::VkImageView>	attachments;

		for (uint32_t colorAttIdx = 0u; colorAttIdx < kColorAttCount; ++colorAttIdx)
		{
			const auto colorViewIdx = iterIdx * kColorAttCount + colorAttIdx;
			images.push_back(colorImages[colorViewIdx].get()->get());
			attachments.push_back(colorImageViews[colorViewIdx].get());
		}

		images.push_back(dsImages[iterIdx].get()->get());
		attachments.push_back(dsImageViews[iterIdx].get());

		if (kUseResolveAtt)
		{
			for (uint32_t resolveAttIdx = 0u; resolveAttIdx < kColorAttCount; ++resolveAttIdx)
			{
				const auto resolveViewIdx = iterIdx * kColorAttCount + resolveAttIdx;
				images.push_back(resolveImages[resolveViewIdx].get()->get());
				attachments.push_back(resolveImageViews[resolveViewIdx].get());
			}
		}

		renderPassFramebuffers.emplace_back(m_testConfig.pipelineConstructionType, vkd, device, &renderPassCreateInfo);

		const vk::VkFramebufferCreateInfo framebufferCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	//	VkStructureType				sType;
			nullptr,										//	const void*					pNext;
			0u,												//	VkFramebufferCreateFlags	flags;
			renderPassFramebuffers[iterIdx].get(),			//	VkRenderPass				renderPass;
			static_cast<deUint32>(attachments.size()),		//	deUint32					attachmentCount;
			attachments.data(),								//	const VkImageView*			pAttachments;
			kFramebufferWidth,								//	deUint32					width;
			kFramebufferHeight,								//	deUint32					height;
			1u,												//	deUint32					layers;
		};

		renderPassFramebuffers[iterIdx].createFramebuffer(vkd, device, &framebufferCreateInfo, images);
	}

	// Shader modules.
	const auto&	binaries			= m_context.getBinaryCollection();
	const auto	dynamicVertModule	= vk::ShaderWrapper(vkd, device, binaries.get("dynamicVert"));
	const auto	staticVertModule	= vk::ShaderWrapper(vkd, device, binaries.get("staticVert"));
	const auto	dynamicFragModule	= vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("dynamicFrag"), 0u);
	const auto	staticFragModule	= vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("staticFrag"), 0u);
	const auto	geomModule			= (m_testConfig.needsGeometryShader() ? vk::ShaderWrapper(vkd, device, binaries.get("geom")) : vk::ShaderWrapper());
	const auto	tescModule			= (m_testConfig.needsTessellation() ? vk::ShaderWrapper(vkd, device, binaries.get("tesc")) : vk::ShaderWrapper());
	const auto	teseModule			= (m_testConfig.needsTessellation() ? vk::ShaderWrapper(vkd, device, binaries.get("tese")) : vk::ShaderWrapper());
	const auto	dynamicMeshModule	= (m_testConfig.useMeshShaders ? vk::ShaderWrapper(vkd, device, binaries.get("dynamicMesh")) : vk::ShaderWrapper());
	const auto	staticMeshModule	= (m_testConfig.useMeshShaders ? vk::ShaderWrapper(vkd, device, binaries.get("staticMesh")) : vk::ShaderWrapper());
	const auto	meshNoOutModule		= (m_testConfig.bindUnusedMeshShadingPipeline ? vk::ShaderWrapper(vkd, device, binaries.get("meshNoOut")) : vk::ShaderWrapper());

	vk::ShaderWrapper	vertDPCPModule;
	vk::ShaderWrapper	fragDPCPModule;

	// Input state.
	const auto vertexBindings	= m_testConfig.vertexGenerator.staticValue->getBindingDescriptions(m_testConfig.strideConfig.staticValue);
	const auto vertexAttributes	= m_testConfig.vertexGenerator.staticValue->getAttributeDescriptions();

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		static_cast<deUint32>(vertexBindings.size()),					//	deUint32									vertexBindingDescriptionCount;
		vertexBindings.data(),											//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		static_cast<deUint32>(vertexAttributes.size()),					//	deUint32									vertexAttributeDescriptionCount;
		vertexAttributes.data(),										//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	// Input assembly.
	const vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineInputAssemblyStateCreateFlags	flags;
		m_testConfig.topologyConfig.staticValue,							//	VkPrimitiveTopology						topology;
		makeVkBool32(m_testConfig.primRestartEnableConfig.staticValue),		//	VkBool32								primitiveRestartEnable;
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

	// Rasterization state.
	void* multisamplePnext		= nullptr;
	void* rasterizationPnext	= nullptr;
	void* viewportPnext			= nullptr;

	const bool			staticStreamInfo			= static_cast<bool>(m_testConfig.rasterizationStreamConfig.staticValue);
	const bool			staticProvokingVtxInfo		= static_cast<bool>(m_testConfig.provokingVertexConfig.staticValue);
	const bool			staticDepthClipEnableInfo	= static_cast<bool>(m_testConfig.depthClipEnableConfig.staticValue);
	const bool			staticDepthClipControlInfo	= static_cast<bool>(m_testConfig.negativeOneToOneConfig.staticValue);
#ifndef CTS_USES_VULKANSC
	using RastStreamInfoPtr		= de::MovePtr<vk::VkPipelineRasterizationStateStreamCreateInfoEXT>;
	using ProvokingVtxModePtr	= de::MovePtr<vk::VkPipelineRasterizationProvokingVertexStateCreateInfoEXT>;
	using DepthClipControlPtr	= de::MovePtr<vk::VkPipelineViewportDepthClipControlCreateInfoEXT>;
	using DepthClipEnablePtr	= de::MovePtr<vk::VkPipelineRasterizationDepthClipStateCreateInfoEXT>;
	using ConservativeRastPtr	= de::MovePtr<vk::VkPipelineRasterizationConservativeStateCreateInfoEXT>;
	using DepthBiasReprInfoPtr	= de::MovePtr<vk::VkDepthBiasRepresentationInfoEXT>;

	RastStreamInfoPtr	pRasterizationStreamInfo;

	if (staticStreamInfo)
	{
		pRasterizationStreamInfo = RastStreamInfoPtr(new vk::VkPipelineRasterizationStateStreamCreateInfoEXT(vk::initVulkanStructure(rasterizationPnext)));
		pRasterizationStreamInfo->rasterizationStream = m_testConfig.rasterizationStreamConfig.staticValue.get();
		rasterizationPnext = pRasterizationStreamInfo.get();
	}

	ProvokingVtxModePtr	pProvokingVertexModeInfo;

	if (staticProvokingVtxInfo)
	{
		pProvokingVertexModeInfo = ProvokingVtxModePtr(new vk::VkPipelineRasterizationProvokingVertexStateCreateInfoEXT(vk::initVulkanStructure(rasterizationPnext)));
		pProvokingVertexModeInfo->provokingVertexMode = makeProvokingVertexMode(m_testConfig.provokingVertexConfig.staticValue.get());
		rasterizationPnext = pProvokingVertexModeInfo.get();
	}

	DepthClipEnablePtr	pDepthClipEnableInfo;

	if (staticDepthClipEnableInfo)
	{
		pDepthClipEnableInfo = DepthClipEnablePtr(new vk::VkPipelineRasterizationDepthClipStateCreateInfoEXT(vk::initVulkanStructure(rasterizationPnext)));
		pDepthClipEnableInfo->depthClipEnable = makeVkBool32(m_testConfig.depthClipEnableConfig.staticValue.get());
		rasterizationPnext = pDepthClipEnableInfo.get();
	}

	DepthClipControlPtr	pDepthClipControlInfo;

	if (staticDepthClipControlInfo)
	{
		pDepthClipControlInfo = DepthClipControlPtr(new vk::VkPipelineViewportDepthClipControlCreateInfoEXT(vk::initVulkanStructure(viewportPnext)));
		pDepthClipControlInfo->negativeOneToOne = makeVkBool32(m_testConfig.negativeOneToOneConfig.staticValue.get());
		viewportPnext = pDepthClipControlInfo.get();
	}

	ConservativeRastPtr	pConservativeRasterModeInfo;

	if (m_testConfig.conservativeRasterStruct())
	{
		pConservativeRasterModeInfo = ConservativeRastPtr(new vk::VkPipelineRasterizationConservativeStateCreateInfoEXT(vk::initVulkanStructure(rasterizationPnext)));
		rasterizationPnext = pConservativeRasterModeInfo.get();

		pConservativeRasterModeInfo->conservativeRasterizationMode		= m_testConfig.conservativeRasterModeConfig.staticValue;
		pConservativeRasterModeInfo->extraPrimitiveOverestimationSize	= m_testConfig.extraPrimitiveOverEstConfig.staticValue;
	}

	DepthBiasReprInfoPtr pDepthBiasReprInfo;

	if (m_testConfig.depthBiasReprInfo && (!m_testConfig.depthBiasConfig.dynamicValue || kReversed))
	{
		// Representation info will be passed statically.
		pDepthBiasReprInfo = DepthBiasReprInfoPtr(new vk::VkDepthBiasRepresentationInfoEXT(vk::initVulkanStructure(rasterizationPnext)));
		rasterizationPnext = pDepthBiasReprInfo.get();

		const auto& reprInfo = m_testConfig.depthBiasReprInfo.get();
		pDepthBiasReprInfo->depthBiasRepresentation	= reprInfo.depthBiasRepresentation;
		pDepthBiasReprInfo->depthBiasExact			= reprInfo.depthBiasExact;
	}
#else
	DE_ASSERT(!staticStreamInfo);
	DE_ASSERT(!staticProvokingVtxInfo);
	DE_ASSERT(!staticDepthClipEnableInfo);
	DE_ASSERT(!staticDepthClipControlInfo);
	DE_ASSERT(!m_testConfig.conservativeRasterStruct());
	DE_UNREF(staticStreamInfo);
	DE_UNREF(staticProvokingVtxInfo);
	DE_UNREF(staticDepthClipEnableInfo);
	DE_UNREF(staticDepthClipControlInfo);
#endif // CTS_USES_VULKANSC

	using LineRasterModePtr		= de::MovePtr<vk::VkPipelineRasterizationLineStateCreateInfoEXT>;
	LineRasterModePtr	pLineRasterModeInfo;

	if (m_testConfig.lineRasterStruct())
	{
		DE_ASSERT(static_cast<bool>(m_testConfig.lineStippleParamsConfig.staticValue));

		pLineRasterModeInfo = LineRasterModePtr(new vk::VkPipelineRasterizationLineStateCreateInfoEXT(vk::initVulkanStructure(rasterizationPnext)));
		rasterizationPnext = pLineRasterModeInfo.get();

		const auto&	lineRasterFeatures	= m_context.getLineRasterizationFeaturesEXT();
		const auto	lineRasterMode		= selectLineRasterizationMode(lineRasterFeatures, m_testConfig.lineStippleSupportRequired(), m_testConfig.lineRasterModeConfig.staticValue);
		const auto&	staticParams		= m_testConfig.lineStippleParamsConfig.staticValue.get();

		pLineRasterModeInfo->stippledLineEnable		= m_testConfig.lineStippleEnableConfig.staticValue;
		pLineRasterModeInfo->lineRasterizationMode	= makeLineRasterizationMode(lineRasterMode);
		pLineRasterModeInfo->lineStippleFactor		= staticParams.factor;
		pLineRasterModeInfo->lineStipplePattern		= staticParams.pattern;
	}

	const vk::VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		rasterizationPnext,												//	const void*								pNext;
		0u,																//	VkPipelineRasterizationStateCreateFlags	flags;
		makeVkBool32(m_testConfig.depthClampEnableConfig.staticValue),	//	VkBool32								depthClampEnable;
		makeVkBool32(m_testConfig.rastDiscardEnableConfig.staticValue),	//	VkBool32								rasterizerDiscardEnable;
		m_testConfig.polygonModeConfig.staticValue,						//	VkPolygonMode							polygonMode;
		m_testConfig.cullModeConfig.staticValue,						//	VkCullModeFlags							cullMode;
		m_testConfig.frontFaceConfig.staticValue,						//	VkFrontFace								frontFace;
		makeVkBool32(m_testConfig.depthBiasEnableConfig.staticValue),	//	VkBool32								depthBiasEnable;
		m_testConfig.depthBiasConfig.staticValue.constantFactor,		//	float									depthBiasConstantFactor;
		m_testConfig.depthBiasConfig.staticValue.clamp,					//	float									depthBiasClamp;
		0.0f,															//	float									depthBiasSlopeFactor;
		m_testConfig.lineWidthConfig.staticValue,						//	float									lineWidth;
	};

	using SampleLocationsPtr = de::MovePtr<vk::VkPipelineSampleLocationsStateCreateInfoEXT>;
	SampleLocationsPtr						pSampleLocations;
	std::vector<vk::VkSampleLocationEXT>	sampleLocationCoords;

#ifndef CTS_USES_VULKANSC
	using CoverageToColorPtr = de::MovePtr<vk::VkPipelineCoverageToColorStateCreateInfoNV>;
	CoverageToColorPtr						pCoverageToColor;

	using CoverageModulationPtr = de::MovePtr<vk::VkPipelineCoverageModulationStateCreateInfoNV>;
	CoverageModulationPtr					pCoverageModulation;

	using CoverageReductionPtr = de::MovePtr<vk::VkPipelineCoverageReductionStateCreateInfoNV>;
	CoverageReductionPtr					pCoverageReduction;

	using ViewportSwizzlePtr = de::MovePtr<vk::VkPipelineViewportSwizzleStateCreateInfoNV>;
	ViewportSwizzlePtr						pViewportSwizzle;

	using ShadingRateImagePtr = de::MovePtr<vk::VkPipelineViewportShadingRateImageStateCreateInfoNV>;
	ShadingRateImagePtr						pShadingRateImage;

	using ViewportWScalingPtr = de::MovePtr<vk::VkPipelineViewportWScalingStateCreateInfoNV>;
	ViewportWScalingPtr						pViewportWScaling;

	using ReprFragmentPtr = de::MovePtr<vk::VkPipelineRepresentativeFragmentTestStateCreateInfoNV>;
	ReprFragmentPtr							pReprFragment;
#endif // CTS_USES_VULKANSC

	if (m_testConfig.sampleLocationsStruct())
	{
		pSampleLocations = SampleLocationsPtr(new vk::VkPipelineSampleLocationsStateCreateInfoEXT(vk::initVulkanStructure(multisamplePnext)));
		multisamplePnext = pSampleLocations.get();

		pSampleLocations->sampleLocationsEnable							= makeVkBool32(m_testConfig.sampleLocationsEnableConfig.staticValue);
		pSampleLocations->sampleLocationsInfo							= vk::initVulkanStructure();
		pSampleLocations->sampleLocationsInfo.sampleLocationsPerPixel	= activeSampleCount;
		pSampleLocations->sampleLocationsInfo.sampleLocationGridSize	= vk::makeExtent2D(1u, 1u);
		pSampleLocations->sampleLocationsInfo.sampleLocationsCount		= static_cast<uint32_t>(activeSampleCount);

		sampleLocationCoords.reserve(pSampleLocations->sampleLocationsInfo.sampleLocationsCount);
		for (uint32_t i = 0; i < pSampleLocations->sampleLocationsInfo.sampleLocationsCount; ++i)
			sampleLocationCoords.push_back(vk::VkSampleLocationEXT{m_testConfig.sampleLocations.x(), m_testConfig.sampleLocations.y()});

		pSampleLocations->sampleLocationsInfo.pSampleLocations = sampleLocationCoords.data();
	}

#ifndef CTS_USES_VULKANSC
	if (m_testConfig.coverageToColorStruct())
	{
		pCoverageToColor = CoverageToColorPtr(new vk::VkPipelineCoverageToColorStateCreateInfoNV(vk::initVulkanStructure(multisamplePnext)));
		multisamplePnext = pCoverageToColor.get();

		pCoverageToColor->coverageToColorEnable		= makeVkBool32(m_testConfig.coverageToColorEnableConfig.staticValue);
		pCoverageToColor->coverageToColorLocation	= m_testConfig.coverageToColorLocationConfig.staticValue;
	}

	if (m_testConfig.coverageModulation)
	{
		pCoverageModulation	= CoverageModulationPtr(new vk::VkPipelineCoverageModulationStateCreateInfoNV(vk::initVulkanStructure(multisamplePnext)));
		multisamplePnext	= pCoverageModulation.get();

		pCoverageModulation->coverageModulationMode			= m_testConfig.coverageModulationModeConfig.staticValue;
		pCoverageModulation->coverageModulationTableEnable	= makeVkBool32(m_testConfig.coverageModTableEnableConfig.staticValue);
		pCoverageModulation->coverageModulationTableCount	= static_cast<uint32_t>(m_testConfig.coverageModTableConfig.staticValue.size());
		pCoverageModulation->pCoverageModulationTable		= de::dataOrNull(m_testConfig.coverageModTableConfig.staticValue);
	}

	if (m_testConfig.coverageReduction)
	{
		pCoverageReduction	= CoverageReductionPtr(new vk::VkPipelineCoverageReductionStateCreateInfoNV(vk::initVulkanStructure(multisamplePnext)));
		multisamplePnext	= pCoverageReduction.get();

		pCoverageReduction->coverageReductionMode = m_testConfig.coverageReductionModeConfig.staticValue;
	}

	if (m_testConfig.viewportSwizzle)
	{
		pViewportSwizzle	= ViewportSwizzlePtr(new vk::VkPipelineViewportSwizzleStateCreateInfoNV(vk::initVulkanStructure(viewportPnext)));
		viewportPnext		= pViewportSwizzle.get();

		const auto& swizzleVec				= m_testConfig.viewportSwizzleConfig.staticValue;
		pViewportSwizzle->viewportCount		= static_cast<uint32_t>(swizzleVec.size());
		pViewportSwizzle->pViewportSwizzles	= de::dataOrNull(swizzleVec);
	}

	const vk::VkShadingRatePaletteEntryNV	defaultShadingRatePaletteEntry	= vk::VK_SHADING_RATE_PALETTE_ENTRY_NO_INVOCATIONS_NV;
	const auto								defaultShadingRatePalette		= vk::makeShadingRatePaletteNV(1u, &defaultShadingRatePaletteEntry);
	std::vector<vk::VkShadingRatePaletteNV>	shadingRatePaletteVec;

	const auto								defaultViewportWScalingFactors	= vk::makeViewportWScalingNV(-1.0f, -1.0f);
	std::vector<vk::VkViewportWScalingNV>	viewportWScalingVec;

	if (m_testConfig.shadingRateImage)
	{
		pShadingRateImage	= ShadingRateImagePtr(new vk::VkPipelineViewportShadingRateImageStateCreateInfoNV(vk::initVulkanStructure(viewportPnext)));
		viewportPnext		= pShadingRateImage.get();

		const auto& viewportVec						= m_testConfig.getActiveViewportVec();
		pShadingRateImage->shadingRateImageEnable	= makeVkBool32(m_testConfig.shadingRateImageEnableConfig.staticValue);
		pShadingRateImage->viewportCount			= de::sizeU32(viewportVec);

		shadingRatePaletteVec.resize(viewportVec.size(), defaultShadingRatePalette);
		pShadingRateImage->pShadingRatePalettes = shadingRatePaletteVec.data();
	}

	if (m_testConfig.viewportWScaling)
	{
		pViewportWScaling	= ViewportWScalingPtr(new vk::VkPipelineViewportWScalingStateCreateInfoNV(vk::initVulkanStructure(viewportPnext)));
		viewportPnext		= pViewportWScaling.get();

		const auto& viewportVec						= m_testConfig.getActiveViewportVec();
		pViewportWScaling->viewportWScalingEnable	= makeVkBool32(m_testConfig.viewportWScalingEnableConfig.staticValue);
		pViewportWScaling->viewportCount			= de::sizeU32(viewportVec);

		viewportWScalingVec.resize(viewportVec.size(), defaultViewportWScalingFactors);
		pViewportWScaling->pViewportWScalings = viewportWScalingVec.data();
	}

	if (m_testConfig.representativeFragmentTest)
	{
		pReprFragment = ReprFragmentPtr(new vk::VkPipelineRepresentativeFragmentTestStateCreateInfoNV(vk::initVulkanStructure()));
		pReprFragment->representativeFragmentTestEnable = makeVkBool32(m_testConfig.reprFragTestEnableConfig.staticValue);
	}
#endif // CTS_USES_VULKANSC

	// Multisample state.
	const vk::VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		multisamplePnext,												//	const void*								pNext;
		0u,																//	VkPipelineMultisampleStateCreateFlags	flags;
		m_testConfig.rasterizationSamplesConfig.staticValue,			//	VkSampleCountFlagBits					rasterizationSamples;
		makeVkBool32(m_testConfig.sampleShadingEnable),					//	VkBool32								sampleShadingEnable;
		m_testConfig.minSampleShading,									//	float									minSampleShading;
		de::dataOrNull(m_testConfig.sampleMaskConfig.staticValue),		//	const VkSampleMask*						pSampleMask;
		makeVkBool32(m_testConfig.alphaToCoverageConfig.staticValue),	//	VkBool32								alphaToCoverageEnable;
		makeVkBool32(m_testConfig.alphaToOneConfig.staticValue),		//	VkBool32								alphaToOneEnable;
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
		m_testConfig.depthBoundsConfig.staticValue.first,					//	float									minDepthBounds;
		m_testConfig.depthBoundsConfig.staticValue.second,					//	float									maxDepthBounds;
	};

	// Dynamic state. Here we will set all states which have a dynamic value.
	const auto dynamicStates = m_testConfig.getDynamicStates();

	const vk::VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(dynamicStates.size()),				//	deUint32							dynamicStateCount;
		de::dataOrNull(dynamicStates),								//	const VkDynamicState*				pDynamicStates;
	};

	const vk::VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		makeVkBool32(m_testConfig.colorBlendEnableConfig.staticValue),			// VkBool32                 blendEnable
		m_testConfig.colorBlendEquationConfig.staticValue.srcColorBlendFactor,	// VkBlendFactor            srcColorBlendFactor
		m_testConfig.colorBlendEquationConfig.staticValue.dstColorBlendFactor,	// VkBlendFactor            dstColorBlendFactor
		m_testConfig.colorBlendEquationConfig.staticValue.colorBlendOp,			// VkBlendOp                colorBlendOp
		m_testConfig.colorBlendEquationConfig.staticValue.srcAlphaBlendFactor,	// VkBlendFactor            srcAlphaBlendFactor
		m_testConfig.colorBlendEquationConfig.staticValue.dstAlphaBlendFactor,	// VkBlendFactor            dstAlphaBlendFactor
		m_testConfig.colorBlendEquationConfig.staticValue.alphaBlendOp,			// VkBlendOp                alphaBlendOp
		m_testConfig.colorWriteMaskConfig.staticValue,							// VkColorComponentFlags    colorWriteMask
	};
	const std::vector<vk::VkPipelineColorBlendAttachmentState> colorBlendAttachmentStateVec (kColorAttCount, colorBlendAttachmentState);

	void* colorBlendPnext = nullptr;

	using ColorBlendAdvancedPtr = de::MovePtr<vk::VkPipelineColorBlendAdvancedStateCreateInfoEXT>;
	ColorBlendAdvancedPtr pColorBlendAdvanced;

	if (m_testConfig.colorBlendEquationConfig.staticValue.isAdvanced())
	{
		pColorBlendAdvanced						= ColorBlendAdvancedPtr(new vk::VkPipelineColorBlendAdvancedStateCreateInfoEXT(vk::initVulkanStructure(colorBlendPnext)));
		pColorBlendAdvanced->srcPremultiplied	= VK_TRUE;
		pColorBlendAdvanced->dstPremultiplied	= VK_TRUE;
		pColorBlendAdvanced->blendOverlap		= vk::VK_BLEND_OVERLAP_UNCORRELATED_EXT;
		colorBlendPnext							= pColorBlendAdvanced.get();
	}

	const std::vector<vk::VkBool32> colorWriteValues (colorBlendAttachmentStateVec.size(), m_testConfig.colorWriteEnableConfig.staticValue);

	using ColorWriteEnablePtr = de::MovePtr<vk::VkPipelineColorWriteCreateInfoEXT>;
	ColorWriteEnablePtr pColorWriteEnable;

	if (m_testConfig.useColorWriteEnable)
	{
		pColorWriteEnable						= ColorWriteEnablePtr(new vk::VkPipelineColorWriteCreateInfoEXT(vk::initVulkanStructure(colorBlendPnext)));
		pColorWriteEnable->attachmentCount		= de::sizeU32(colorWriteValues);
		pColorWriteEnable->pColorWriteEnables	= de::dataOrNull(colorWriteValues);
		colorBlendPnext							= pColorWriteEnable.get();
	}

	if (m_testConfig.nullStaticColorBlendAttPtr)
	{
		DE_ASSERT(static_cast<bool>(m_testConfig.colorBlendEnableConfig.dynamicValue));
		DE_ASSERT(static_cast<bool>(m_testConfig.colorBlendEquationConfig.dynamicValue));
		DE_ASSERT(static_cast<bool>(m_testConfig.colorWriteMaskConfig.dynamicValue));
	}

	const vk::VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                               sType
		colorBlendPnext,												// const void*                                   pNext
		0u,																// VkPipelineColorBlendStateCreateFlags          flags
		m_testConfig.logicOpEnableConfig.staticValue,					// VkBool32                                      logicOpEnable
		m_testConfig.logicOpConfig.staticValue,							// VkLogicOp                                     logicOp
		static_cast<uint32_t>(colorBlendAttachmentStateVec.size()),		// deUint32                                      attachmentCount
		(m_testConfig.nullStaticColorBlendAttPtr						// const VkPipelineColorBlendAttachmentState*    pAttachments
			? nullptr
			: de::dataOrNull(colorBlendAttachmentStateVec)),

		{																// float                                         blendConstants[4]
			m_testConfig.blendConstantsConfig.staticValue[0],
			m_testConfig.blendConstantsConfig.staticValue[1],
			m_testConfig.blendConstantsConfig.staticValue[2],
			m_testConfig.blendConstantsConfig.staticValue[3],
		},
	};

	vk::GraphicsPipelineWrapper	staticPipeline		(vki, vkd, physicalDevice, device, deviceHelper.getDeviceExtensions(), m_testConfig.pipelineConstructionType);

	// Create extra dynamic patch control points pipeline if needed.
	vk::GraphicsPipelineWrapper extraDynPCPPipeline (vki, vkd, physicalDevice, device, deviceHelper.getDeviceExtensions(), m_testConfig.pipelineConstructionType);

	if (m_testConfig.useExtraDynPCPPipeline)
	{
		vertDPCPModule = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vertDPCP"));
		fragDPCPModule = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("fragDPCP"));

		const vk::VkPipelineVertexInputStateCreateInfo	extraDPCPInputState		= vk::initVulkanStructure();
		const vk::VkDynamicState						extraDynamicState		= vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT;
		const vk::VkPipelineDynamicStateCreateInfo		extraDynamicStateInfo	=
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
			nullptr,													//	const void*							pNext;
			0u,															//	VkPipelineDynamicStateCreateFlags	flags;
			1u,															//	uint32_t							dynamicStateCount;
			&extraDynamicState,											//	const VkDynamicState*				pDynamicStates;
		};

		const vk::PipelineLayoutWrapper extraPipelineLayout (m_testConfig.pipelineConstructionType, vkd, device);

		const auto viewports	= m_testConfig.viewportConfig.staticValue;
		const auto scissors		= m_testConfig.scissorConfig.staticValue;

		extraDynPCPPipeline.setDynamicState(&extraDynamicStateInfo)
						   .setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
						   .setDefaultColorBlendState()
						   .setDefaultMultisampleState()
						   .setupVertexInputState(&extraDPCPInputState)
						   .setupPreRasterizationShaderState(
										viewports,
										scissors,
										extraPipelineLayout,
										*renderPassFramebuffers[0],
										0u,
										vertDPCPModule,
										&rasterizationStateCreateInfo)
						   .setupFragmentShaderState(extraPipelineLayout, *renderPassFramebuffers[0], 0u, fragDPCPModule, &depthStencilStateCreateInfo)
						   .setupFragmentOutputState(*renderPassFramebuffers[0], 0u)
						   .setMonolithicPipelineLayout(extraPipelineLayout)
						   .buildPipeline();
	}
	else if (m_testConfig.useExtraDynPipeline)
	{
		vertDPCPModule = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vertDPCP"));
	}

	// Create static pipeline when needed.
	if (kUseStaticPipeline)
	{
		auto viewports	= m_testConfig.viewportConfig.staticValue;
		auto scissors	= m_testConfig.scissorConfig.staticValue;

		// The viewport and scissor counts must match in the static part, which will be used by the static pipeline.
		const auto minStaticCount = static_cast<deUint32>(std::min(m_testConfig.viewportConfig.staticValue.size(), m_testConfig.scissorConfig.staticValue.size()));
		viewports.resize(minStaticCount);
		scissors.resize(minStaticCount);

		staticPipeline.setDefaultPatchControlPoints(m_testConfig.patchControlPointsConfig.staticValue)
					  .setViewportStatePnext(viewportPnext)
					  .setDefaultTessellationDomainOrigin(m_testConfig.tessDomainOriginConfig.staticValue);

		// The pAttachments pointer must never be null for the static pipeline.
		vk::VkPipelineColorBlendStateCreateInfo staticCBStateInfo = colorBlendStateCreateInfo;
		if (m_testConfig.nullStaticColorBlendAttPtr)
			staticCBStateInfo.pAttachments = de::dataOrNull(colorBlendAttachmentStateVec);

#ifndef CTS_USES_VULKANSC
		if (m_testConfig.useMeshShaders)
		{
			staticPipeline.setupPreRasterizationMeshShaderState(
												viewports,
												scissors,
												pipelineLayout,
												*renderPassFramebuffers[0],
												0u,
												vk::ShaderWrapper(),
												staticMeshModule,
												&rasterizationStateCreateInfo);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			staticPipeline.setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
						  .setupPreRasterizationShaderState(
												viewports,
												scissors,
												pipelineLayout,
												*renderPassFramebuffers[0],
												0u,
												staticVertModule,
												&rasterizationStateCreateInfo,
												tescModule,
												teseModule,
												geomModule);
		}

		staticPipeline
#ifndef CTS_USES_VULKANSC
					  .setRepresentativeFragmentTestState(pReprFragment.get())
#endif // CTS_USES_VULKANSC
					  .setupFragmentShaderState(pipelineLayout, *renderPassFramebuffers[0], 0u, staticFragModule, &depthStencilStateCreateInfo, &multisampleStateCreateInfo)
					  .setupFragmentOutputState(*renderPassFramebuffers[0], 0u, &staticCBStateInfo, &multisampleStateCreateInfo)
					  .setMonolithicPipelineLayout(pipelineLayout)
					  .buildPipeline();
	}

	// Create dynamic pipeline.
	vk::GraphicsPipelineWrapper graphicsPipeline(vki, vkd, physicalDevice, device, deviceHelper.getDeviceExtensions(), m_testConfig.pipelineConstructionType);
	vk::GraphicsPipelineWrapper extraDynPipeline(vki, vkd, physicalDevice, device, deviceHelper.getDeviceExtensions(), m_testConfig.pipelineConstructionType);
	{
		auto viewports	= m_testConfig.viewportConfig.staticValue;
		auto scissors	= m_testConfig.scissorConfig.staticValue;

		const auto finalDynamicViewportCount = (m_testConfig.viewportConfig.dynamicValue
			? m_testConfig.viewportConfig.dynamicValue.get().size()
			: m_testConfig.viewportConfig.staticValue.size());

		const auto finalDynamicScissorCount = (m_testConfig.scissorConfig.dynamicValue
			? m_testConfig.scissorConfig.dynamicValue.get().size()
			: m_testConfig.scissorConfig.staticValue.size());

		const auto minDynamicCount = static_cast<deUint32>(std::min(finalDynamicScissorCount, finalDynamicViewportCount));

		// The viewport and scissor counts must be zero when a dynamic value will be provided, as per the spec.
		if (m_testConfig.viewportConfig.dynamicValue)
		{
			graphicsPipeline.setDefaultViewportsCount();
			if (m_testConfig.useExtraDynPipeline)
				extraDynPipeline.setDefaultViewportsCount();
			viewports = std::vector<vk::VkViewport>();
		}
		else
			viewports.resize(minDynamicCount);

		if (m_testConfig.scissorConfig.dynamicValue)
		{
			graphicsPipeline.setDefaultScissorsCount();
			if (m_testConfig.useExtraDynPipeline)
				extraDynPipeline.setDefaultScissorsCount();
			scissors = std::vector<vk::VkRect2D>();
		}
		else
			scissors.resize(minDynamicCount);

		// Setting patch control points to std::numeric_limits<uint32_t>::max() will force null tessellation state pointer.
		const auto patchControlPoints	= ((m_testConfig.favorStaticNullPointers && m_testConfig.patchControlPointsConfig.dynamicValue)
										? std::numeric_limits<uint32_t>::max()
										: m_testConfig.patchControlPointsConfig.staticValue);

		const auto disableViewportState	= (m_testConfig.favorStaticNullPointers && m_testConfig.viewportConfig.dynamicValue && m_testConfig.scissorConfig.dynamicValue);

		graphicsPipeline.setDynamicState(&dynamicStateCreateInfo)
						.setDefaultPatchControlPoints(patchControlPoints)
						.setViewportStatePnext(viewportPnext)
						.setDefaultTessellationDomainOrigin(m_testConfig.tessDomainOriginConfig.staticValue)
						.disableViewportState(disableViewportState);
		if (m_testConfig.useExtraDynPipeline)
			extraDynPipeline.setDynamicState(&dynamicStateCreateInfo)
							.setDefaultPatchControlPoints(patchControlPoints)
							.setViewportStatePnext(viewportPnext)
							.setDefaultTessellationDomainOrigin(m_testConfig.tessDomainOriginConfig.staticValue)
							.disableViewportState(disableViewportState);

		const auto staticRasterizationStateCreateInfo	= ((m_testConfig.favorStaticNullPointers
															&& m_testConfig.depthClampEnableConfig.dynamicValue
															&& m_testConfig.rastDiscardEnableConfig.dynamicValue
															&& m_testConfig.polygonModeConfig.dynamicValue
															&& m_testConfig.cullModeConfig.dynamicValue
															&& m_testConfig.frontFaceConfig.dynamicValue
															&& m_testConfig.depthBiasEnableConfig.dynamicValue
															&& m_testConfig.depthBiasConfig.dynamicValue
															&& m_testConfig.lineWidthConfig.dynamicValue)
														? nullptr
														: &rasterizationStateCreateInfo);

		DE_ASSERT(!m_testConfig.useExtraDynPipeline || !m_testConfig.useMeshShaders);

		const vk::VkPipelineVertexInputStateCreateInfo emptyVertexInputStateCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
			DE_NULL,														//	const void*									pNext;
			0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
			0u,																//	deUint32									vertexBindingDescriptionCount;
			DE_NULL,														//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
			0u,																//	deUint32									vertexAttributeDescriptionCount;
			DE_NULL,														//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

#ifndef CTS_USES_VULKANSC
		if (m_testConfig.useMeshShaders)
		{
			graphicsPipeline.setupPreRasterizationMeshShaderState(
												viewports,
												scissors,
												pipelineLayout,
												*renderPassFramebuffers[0],
												0u,
												vk::ShaderWrapper(),
												dynamicMeshModule,
												staticRasterizationStateCreateInfo);
		}
		else
#endif // CTS_USES_VULKANSC
		{
			const auto staticVertexInputStateCreateInfo		= ((m_testConfig.favorStaticNullPointers && m_testConfig.testVertexDynamic())
															? nullptr
															: &vertexInputStateCreateInfo);

			const auto staticInputAssemblyStateCreateInfo	= ((m_testConfig.favorStaticNullPointers && m_testConfig.primRestartEnableConfig.dynamicValue && m_testConfig.topologyConfig.dynamicValue)
															? nullptr
															: &inputAssemblyStateCreateInfo);

			graphicsPipeline.setupVertexInputState(
												staticVertexInputStateCreateInfo,
												staticInputAssemblyStateCreateInfo,
												VK_NULL_HANDLE,
												vk::PipelineCreationFeedbackCreateInfoWrapper(),
												m_testConfig.favorStaticNullPointers)
							.setupPreRasterizationShaderState(
												viewports,
												scissors,
												pipelineLayout,
												*renderPassFramebuffers[0],
												0u,
												dynamicVertModule,
												staticRasterizationStateCreateInfo,
												tescModule,
												teseModule,
												geomModule);

			if (m_testConfig.useExtraDynPipeline)
				extraDynPipeline.setupVertexInputState(
													&emptyVertexInputStateCreateInfo,
													staticInputAssemblyStateCreateInfo,
													VK_NULL_HANDLE,
													vk::PipelineCreationFeedbackCreateInfoWrapper(),
													m_testConfig.favorStaticNullPointers)
								.setupPreRasterizationShaderState(
													viewports,
													scissors,
													pipelineLayout,
													*renderPassFramebuffers[0],
													0u,
													vertDPCPModule,
													staticRasterizationStateCreateInfo);
		}

		const auto staticMultisampleStateCreateInfo	= ((m_testConfig.favorStaticNullPointers
														&& m_testConfig.rasterizationSamplesConfig.dynamicValue
														&& m_testConfig.sampleMaskConfig.dynamicValue
														&& m_testConfig.alphaToCoverageConfig.dynamicValue
														&& m_testConfig.alphaToOneConfig.dynamicValue)
													? nullptr
													: &multisampleStateCreateInfo);

		const auto staticDepthStencilStateCreateInfo	= ((m_testConfig.favorStaticNullPointers
															&& m_testConfig.depthTestEnableConfig.dynamicValue
															&& m_testConfig.depthWriteEnableConfig.dynamicValue
															&& m_testConfig.depthCompareOpConfig.dynamicValue
															&& m_testConfig.depthBoundsTestEnableConfig.dynamicValue
															&& m_testConfig.stencilTestEnableConfig.dynamicValue
															&& m_testConfig.stencilOpConfig.dynamicValue
															&& m_testConfig.depthBoundsConfig.dynamicValue)
														? nullptr
														: &depthStencilStateCreateInfo);

		const auto staticColorBlendStateCreateInfo		= ((m_testConfig.favorStaticNullPointers
															&& m_testConfig.logicOpEnableConfig.dynamicValue
															&& m_testConfig.logicOpConfig.dynamicValue
															&& m_testConfig.colorBlendEnableConfig.dynamicValue
															&& m_testConfig.colorBlendEquationConfig.dynamicValue
															&& (m_testConfig.colorBlendBoth
																|| !m_testConfig.colorBlendEquationConfig.staticValue.isAdvanced())
															&& m_testConfig.colorWriteMaskConfig.dynamicValue
															&& m_testConfig.blendConstantsConfig.dynamicValue)
														? nullptr
														: &colorBlendStateCreateInfo);
		graphicsPipeline
#ifndef CTS_USES_VULKANSC
						.setRepresentativeFragmentTestState(pReprFragment.get())
#endif // CTS_USES_VULKANSC
						.setupFragmentShaderState(pipelineLayout, *renderPassFramebuffers[0], 0u, dynamicFragModule, staticDepthStencilStateCreateInfo, staticMultisampleStateCreateInfo)
						.setupFragmentOutputState(*renderPassFramebuffers[0], 0u, staticColorBlendStateCreateInfo, staticMultisampleStateCreateInfo)
						.setMonolithicPipelineLayout(pipelineLayout)
						.buildPipeline();
		if (m_testConfig.useExtraDynPipeline)
			extraDynPipeline
	#ifndef CTS_USES_VULKANSC
							.setRepresentativeFragmentTestState(pReprFragment.get())
	#endif // CTS_USES_VULKANSC
							.setupFragmentShaderState(pipelineLayout, *renderPassFramebuffers[0], 0u, dynamicFragModule, staticDepthStencilStateCreateInfo, staticMultisampleStateCreateInfo)
							.setupFragmentOutputState(*renderPassFramebuffers[0], 0u, staticColorBlendStateCreateInfo, staticMultisampleStateCreateInfo)
							.setMonolithicPipelineLayout(pipelineLayout)
							.buildPipeline();
	}

	vk::GraphicsPipelineWrapper meshNoOutPipeline(vki, vkd, physicalDevice, device, deviceHelper.getDeviceExtensions(), m_testConfig.pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
	if (m_testConfig.bindUnusedMeshShadingPipeline)
	{
		// Remove dynamic states which are not compatible with mesh shading pipelines.
		std::vector<vk::VkDynamicState> meshNoOutDynamicStates;
		std::copy_if(begin(dynamicStates), end(dynamicStates), std::back_inserter(meshNoOutDynamicStates), isMeshShadingPipelineCompatible);

		const vk::VkPipelineDynamicStateCreateInfo meshNoOutDynamicStateInfo =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
			nullptr,													//	const void*							pNext;
			0u,															//	VkPipelineDynamicStateCreateFlags	flags;
			de::sizeU32(meshNoOutDynamicStates),						//	uint32_t							dynamicStateCount;
			de::dataOrNull(meshNoOutDynamicStates),						//	const VkDynamicState*				pDynamicStates;
		};

		// Provide a viewport state similar to the static pipeline.
		auto viewports	= m_testConfig.viewportConfig.staticValue;
		auto scissors	= m_testConfig.scissorConfig.staticValue;

		const auto minStaticCount = static_cast<deUint32>(std::min(m_testConfig.viewportConfig.staticValue.size(), m_testConfig.scissorConfig.staticValue.size()));
		viewports.resize(minStaticCount);
		scissors.resize(minStaticCount);

		meshNoOutPipeline.setDynamicState(&meshNoOutDynamicStateInfo)
						 .setDefaultPatchControlPoints(m_testConfig.patchControlPointsConfig.staticValue)
						 .setupPreRasterizationMeshShaderState(
											viewports,
											scissors,
											pipelineLayout,
											*renderPassFramebuffers[0],
											0u,
											vk::ShaderWrapper(),
											meshNoOutModule,
											&rasterizationStateCreateInfo)
						 .setupFragmentShaderState(pipelineLayout, *renderPassFramebuffers[0], 0u, vk::ShaderWrapper(), &depthStencilStateCreateInfo, &multisampleStateCreateInfo)
						 .setupFragmentOutputState(*renderPassFramebuffers[0], 0u, &colorBlendStateCreateInfo, &multisampleStateCreateInfo)
						 .setMonolithicPipelineLayout(pipelineLayout)
						 .buildPipeline();
	}
#endif // CTS_USES_VULKANSC

	// Command buffer.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd , device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Clear values, clear to green for dynamic logicOp
	std::vector<vk::VkClearValue> clearValues (kColorAttCount, m_testConfig.clearColorValue);
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
			boundInAdvance = maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, 0u, vertBuffers, rvertBuffers);
		}

		// Begin render pass.
		renderPassFramebuffers[iteration].begin(vkd, cmdBuffer, vk::makeRect2D(kFramebufferWidth, kFramebufferHeight), static_cast<deUint32>(clearValues.size()), clearValues.data());

			// Bind a static pipeline first if needed.
			if (kBindStaticFirst && iteration == 0u)
				staticPipeline.bind(cmdBuffer);

			// Maybe set extended dynamic state here.
			if (kSequenceOrdering == SequenceOrdering::BETWEEN_PIPELINES)
			{
				setDynamicStates(m_testConfig, vkd, cmdBuffer);
				boundInAdvance = maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, 0u, vertBuffers, rvertBuffers);
			}

			// Bind dynamic pipeline.
			if ((kSequenceOrdering != SequenceOrdering::TWO_DRAWS_DYNAMIC &&
				 kSequenceOrdering != SequenceOrdering::TWO_DRAWS_STATIC) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC && iteration > 0u) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration == 0u))
			{
				if (m_testConfig.bindUnusedMeshShadingPipeline)
				{
					DE_ASSERT(kSequenceOrdering == SequenceOrdering::CMD_BUFFER_START);
					meshNoOutPipeline.bind(cmdBuffer);
				}

				if (m_testConfig.useExtraDynPCPPipeline)
				{
					extraDynPCPPipeline.bind(cmdBuffer);

					// In these two sequence orderings, the right dynamic state value will have been set before and we would be
					// setting it to a wrong value here, resulting in test failures. We keep the right value instead.
					if (kSequenceOrdering != SequenceOrdering::CMD_BUFFER_START && kSequenceOrdering != SequenceOrdering::BETWEEN_PIPELINES)
						vkd.cmdSetPatchControlPointsEXT(cmdBuffer, m_testConfig.patchControlPointsConfig.staticValue);

					vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
				}

				if (m_testConfig.useExtraDynPipeline)
				{
					extraDynPipeline.bind(cmdBuffer);

					if (kSequenceOrdering == SequenceOrdering::BEFORE_DRAW || kSequenceOrdering == SequenceOrdering::AFTER_PIPELINES || kSequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC)
						setDynamicStates(m_testConfig, vkd, cmdBuffer);

					vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
				}

				graphicsPipeline.bind(cmdBuffer);

				// When shader objects are used vkCmdSetVertexInput() will overwrite vkCmdBindBuffers2 so we have to call it again
				if (boundInAdvance && vk::isConstructionTypeShaderObject(m_testConfig.pipelineConstructionType))
					maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, 0u, vertBuffers, rvertBuffers);
			}

			if (kSequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC && iteration > 0u) ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration == 0u))
			{
				setDynamicStates(m_testConfig, vkd, cmdBuffer);
				boundInAdvance = maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, 0u, vertBuffers, rvertBuffers);
			}

			// Bind a static pipeline last if needed.
			if (kSequenceOrdering == SequenceOrdering::BEFORE_GOOD_STATIC ||
				(kSequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC && iteration > 0u))
			{
				staticPipeline.bind(cmdBuffer);
			}

			const auto& viewportVec = m_testConfig.getActiveViewportVec();
			for (size_t viewportIdx = 0u; viewportIdx < viewportVec.size(); ++viewportIdx)
			{
				for (size_t meshIdx = 0u; meshIdx < m_testConfig.meshParams.size(); ++meshIdx)
				{
					// Push constants.
					PushConstants pushConstants =
					{
						m_testConfig.meshParams[meshIdx].color,			//	tcu::Vec4	triangleColor;
						m_testConfig.meshParams[meshIdx].depth,			//	float		meshDepth;
						static_cast<deInt32>(viewportIdx),				//	deInt32		viewPortIndex;
						m_testConfig.meshParams[meshIdx].scaleX,		//	float		scaleX;
						m_testConfig.meshParams[meshIdx].scaleY,		//	float		scaleY;
						m_testConfig.meshParams[meshIdx].offsetX,		//	float		offsetX;
						m_testConfig.meshParams[meshIdx].offsetY,		//	float		offsetY;
						m_testConfig.meshParams[meshIdx].stripScale,	//	float		stripScale;
					};
					vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pushConstantStageFlags, 0u, static_cast<deUint32>(sizeof(pushConstants)), &pushConstants);

					// Track vertex bounding state for this mesh.
					bool boundBeforeDraw = false;

					// Maybe set extended dynamic state here.
					if (kSequenceOrdering == SequenceOrdering::BEFORE_DRAW || kSequenceOrdering == SequenceOrdering::AFTER_PIPELINES)
					{
						setDynamicStates(m_testConfig, vkd, cmdBuffer);
						boundBeforeDraw = maybeBindVertexBufferDynStride(m_testConfig, vkd, cmdBuffer, meshIdx, vertBuffers, rvertBuffers);
					}

					// Bind vertex buffer with static stride if needed and draw.
					if (!(boundInAdvance || boundBeforeDraw) && !m_testConfig.useMeshShaders)
					{
						bindVertexBuffers(vkd, cmdBuffer, (m_testConfig.meshParams[meshIdx].reversed ? rvertBuffers : vertBuffers));
						if (m_testConfig.needsIndexBuffer())
						{
							const auto indexType = vk::VK_INDEX_TYPE_UINT32;
							vkd.cmdBindIndexBuffer(cmdBuffer, indexBuffer->get(), 0, indexType);
						}
					}

					if (vertDataAsSSBO)
					{
						if (topologyClass == TopologyClass::LINE)
							DE_ASSERT(!m_testConfig.meshParams[meshIdx].reversed);

						const auto boundSet = (m_testConfig.meshParams[meshIdx].reversed ? meshDescriptorSetRev.get() : meshDescriptorSet.get());
						vkd.cmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, pipelineLayout.get(), 0u, 1u, &boundSet, 0u, nullptr);
					}

#ifndef CTS_USES_VULKANSC
					// Shading rate image if enabled (we'll use a null handle to simplify, which is valid).
					if (m_testConfig.shadingRateImage)
						vkd.cmdBindShadingRateImageNV(cmdBuffer, VK_NULL_HANDLE, vk::VK_IMAGE_LAYOUT_GENERAL);
#endif // CTS_USES_VULKANSC

					if (kFragAtomics)
						vkd.cmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, pipelineLayout.get(), m_testConfig.getFragDescriptorSetIndex(), 1u, &fragDescriptorSet.get(), 0u, nullptr);

					// Draw mesh.
					if (m_testConfig.needsIndexBuffer())
					{
						deUint32 numIndices = static_cast<deUint32>(indices.size());
						// For SequenceOrdering::TWO_DRAWS_DYNAMIC and TWO_DRAWS_STATIC cases, the first draw does not have primitive restart enabled
						// So, draw without using the invalid index, the second draw with primitive restart enabled will replace the results
						// using all indices.
						if (iteration == 0u && m_testConfig.testPrimRestartEnable() &&
							(m_testConfig.sequenceOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC ||
							m_testConfig.sequenceOrdering == SequenceOrdering::TWO_DRAWS_STATIC))
						{
							numIndices = 2u;
						}
						vkd.cmdDrawIndexed(cmdBuffer, numIndices, m_testConfig.instanceCount, 0u, 0u, 0u);
					}
#ifndef CTS_USES_VULKANSC
					else if (m_testConfig.useMeshShaders)
					{
						// Make sure drawing this way makes sense.
						const auto minVertCount = ((topologyClass == TopologyClass::LINE) ? 2u : 3u);
						DE_UNREF(minVertCount); // For release builds.
						DE_ASSERT(vertices.size() >= minVertCount);
						DE_ASSERT(m_testConfig.instanceCount == 1u);
						DE_ASSERT(!m_testConfig.topologyConfig.dynamicValue);

						uint32_t numPrimitives = 0u;

						if (topologyClass == TopologyClass::TRIANGLE)
						{
							DE_ASSERT(m_testConfig.topologyConfig.staticValue == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
							numPrimitives = de::sizeU32(vertices) - 2u;
						}
						else if (topologyClass == TopologyClass::LINE)
						{
							DE_ASSERT(m_testConfig.topologyConfig.staticValue == vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
							const auto vertsPerRow = 4u;
							const auto linesPerRow = 3u;
							const auto vertexCount = de::sizeU32(vertices);
							const auto rowCount = vertexCount / vertsPerRow;
							numPrimitives = rowCount * linesPerRow;

							if (m_testConfig.obliqueLine)
								numPrimitives = 1u;
							else
								DE_ASSERT(vertexCount % vertsPerRow == 0u);
						}
						else
							DE_ASSERT(false);

						vkd.cmdDrawMeshTasksEXT(cmdBuffer, numPrimitives, 1u, 1u);
					}
#endif // CTS_USES_VULKANSC
					else
					{
						uint32_t vertexCount = static_cast<deUint32>(vertices.size());
						if (m_testConfig.singleVertex)
							vertexCount = m_testConfig.singleVertexDrawCount;
						vkd.cmdDraw(cmdBuffer, vertexCount, m_testConfig.instanceCount, 0u, 0u);
					}
				}
			}

		renderPassFramebuffers[iteration].end(vkd, cmdBuffer);
	}

	if (kFragAtomics)
	{
		const auto bufferBarrier = vk::makeMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT);
		vk::cmdPipelineMemoryBarrier(vkd, cmdBuffer, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, &bufferBarrier);
	}

	vk::endCommandBuffer(vkd, cmdBuffer);

	// Submit commands.
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read result image aspects from the last used framebuffer.
	using LevelPtr = de::MovePtr<tcu::TextureLevel>;

	const tcu::UVec2	renderSize		(kFramebufferWidth, kFramebufferHeight);

	const auto			colorResultImg	= (kUseResolveAtt ? resolveImages.back()->get() : colorImages.back()->get());
	const auto			colorBuffer		= readColorAttachment(vkd, device, queue, queueIndex, allocator, colorResultImg, colorFormat, renderSize);
	const auto			colorAccess		= colorBuffer->getAccess();

	LevelPtr				depthBuffer;
	LevelPtr				stencilBuffer;
	tcu::PixelBufferAccess	depthAccess;
	tcu::PixelBufferAccess	stencilAccess;

	if (!kMultisampleDS)
	{
		depthBuffer		= readDepthAttachment(vkd, device, queue, queueIndex, allocator, dsImages.back()->get(), dsFormatInfo->imageFormat, renderSize);
		stencilBuffer	= readStencilAttachment(vkd, device, queue, queueIndex, allocator, dsImages.back()->get(), dsFormatInfo->imageFormat, renderSize, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		depthAccess		= depthBuffer->getAccess();
		stencilAccess	= stencilBuffer->getAccess();
	}

	const int kWidth	= static_cast<int>(kFramebufferWidth);
	const int kHeight	= static_cast<int>(kFramebufferHeight);

	// Generate reference color buffer.
	const auto				tcuColorFormat			= vk::mapVkFormat(colorFormat);
	tcu::TextureLevel		referenceColorLevel		(tcuColorFormat, kWidth, kHeight);
	tcu::PixelBufferAccess	referenceColorAccess	= referenceColorLevel.getAccess();
	(*m_testConfig.referenceColor)(referenceColorAccess);

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
	const bool	hasCustomVerif	= static_cast<bool>(m_testConfig.colorVerificator);
	const auto	minDepth		= m_testConfig.expectedDepth - dsFormatInfo->depthThreshold - m_testConfig.extraDepthThreshold;
	const auto	maxDepth		= m_testConfig.expectedDepth + dsFormatInfo->depthThreshold + m_testConfig.extraDepthThreshold;
	bool		colorMatch		= true;
	bool		depthMatch		= true;
	bool		stencilMatch	= true;
	bool		match;

	if (hasCustomVerif)
		colorMatch = (*m_testConfig.colorVerificator)(colorAccess, referenceColorAccess, colorErrorAccess);

	for (int y = 0; y < kHeight; ++y)
		for (int x = 0; x < kWidth; ++x)
		{
			if (!hasCustomVerif)
			{
				if (vk::isUnormFormat(colorFormat))
				{
					const auto colorPixel		= colorAccess.getPixel(x, y);
					const auto expectedPixel	= referenceColorAccess.getPixel(x, y);
					match = tcu::boolAll(tcu::lessThan(tcu::absDiff(colorPixel, expectedPixel), kUnormColorThreshold));
				}
				else
				{
					DE_ASSERT(vk::isUintFormat(colorFormat));
					const auto colorPixel		= colorAccess.getPixelUint(x, y);
					const auto expectedPixel	= referenceColorAccess.getPixelUint(x, y);
					match = (colorPixel == expectedPixel);
				}

				colorErrorAccess.setPixel((match ? kGood : kBad), x, y);
				if (!match)
					colorMatch = false;
			}

			if (!kMultisampleDS)
			{
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
		}

	if (!(colorMatch && depthMatch && stencilMatch))
	{
		if (!colorMatch)
			logErrors(log, "Color", "Result color image and error mask", colorAccess, colorErrorAccess);

		if (!depthMatch)
			logErrors(log, "Depth", "Result depth image and error mask", depthAccess, depthErrorAccess);

		if (!stencilMatch)
			logErrors(log, "Stencil", "Result stencil image and error mask", stencilAccess, stencilErrorAccess);

		if (!(colorMatch && depthMatch && stencilMatch))
			return tcu::TestStatus::fail("Incorrect value found in attachments; please check logged images");
	}

	// Check storage buffer if used.
	uint32_t fragCounter = 0u;

	if (kFragAtomics)
	{
		DE_ASSERT(m_testConfig.oversizedTriangle);
		DE_ASSERT(m_testConfig.meshParams.size() == 1u);
		DE_ASSERT(!m_testConfig.depthWriteEnableConfig.dynamicValue);	// No dynamic value for depth writes.
		DE_ASSERT(!m_testConfig.depthWriteEnableConfig.staticValue);	// No depth writes.

		auto& counterBufferAlloc	= counterBuffer->getAllocation();
		void* counterBufferData		= counterBufferAlloc.getHostPtr();
		vk::invalidateAlloc(vkd, device, counterBufferAlloc);

		deMemcpy(&fragCounter, counterBufferData, sizeof(fragCounter));
	}

	if (m_testConfig.representativeFragmentTest)
	{
		DE_ASSERT(!m_testConfig.rasterizationSamplesConfig.dynamicValue);

		// The expected number of invocations depends on how many draws are performed with the test enabled.
		// Draws with the test disabled should always result in kFramebufferHeight * kFramebufferWidth invocations.
		// Draws with the test enabled should result in at least 1 invocation, maybe more.
		uint32_t minValue = 0u;

		const uint32_t minInvocations[] =
		{
			(kFramebufferHeight * kFramebufferWidth * static_cast<uint32_t>(m_testConfig.rasterizationSamplesConfig.staticValue)),
			1u,
		};

		if (kNumIterations == 1u)
		{
			const auto testEnabled	= m_testConfig.getActiveReprFragTestEnable();
			minValue += minInvocations[testEnabled];
		}
		else if (kNumIterations == 2u)
		{
			for (uint32_t i = 0u; i < kNumIterations; ++i)
			{
				bool testEnabled = false;

#ifndef CTS_USES_VULKANSC
				// Actually varies depending on TWO_DRAWS_STATIC/_DYNAMIC, but does not affect results.
				const bool staticDraw = (i == 0u);

				if (staticDraw)
					testEnabled = m_testConfig.reprFragTestEnableConfig.staticValue;
				else
				{
					testEnabled	= (m_testConfig.reprFragTestEnableConfig.dynamicValue
								? m_testConfig.reprFragTestEnableConfig.dynamicValue.get()
								: m_testConfig.reprFragTestEnableConfig.staticValue);
				}
#endif // CTS_USES_VULKANSC

				minValue += minInvocations[testEnabled];
			}
		}
		else
		{
			DE_ASSERT(false);
		}

		log << tcu::TestLog::Message << "Fragment counter minimum value: " << minValue << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Fragment counter: " << fragCounter << tcu::TestLog::EndMessage;

		if (fragCounter < minValue)
		{
			std::ostringstream msg;
			msg << "Fragment shader invocation counter lower than expected: found " << fragCounter << " and expected at least " << minValue;
			return tcu::TestStatus::fail(msg.str());
		}
	}
	else if (kFragAtomics)
	{
		// The expected number of invocations depends on how many draws are performed and the sample count of each one.
		// Draws with the test disabled should always result in kFramebufferHeight * kFramebufferWidth invocations.
		// Draws with the test enabled should result in at least 1 invocation, maybe more.
		uint32_t sampleCount = 0u;

		if (kNumIterations == 1u)
		{
			sampleCount += static_cast<uint32_t>(m_testConfig.getActiveSampleCount());
		}
		else if (kNumIterations == 2u)
		{
			for (uint32_t i = 0u; i < kNumIterations; ++i)
			{
				// Actually varies depending on TWO_DRAWS_STATIC/_DYNAMIC, but does not affect results.
				const bool staticDraw = (i == 0u);

				if (staticDraw)
					sampleCount += static_cast<uint32_t>(m_testConfig.rasterizationSamplesConfig.staticValue);
				else
				{
					sampleCount += static_cast<uint32_t>(m_testConfig.rasterizationSamplesConfig.dynamicValue
									? m_testConfig.rasterizationSamplesConfig.dynamicValue.get()
									: m_testConfig.rasterizationSamplesConfig.staticValue);
				}
			}
		}
		else
		{
			DE_ASSERT(false);
		}

		const uint32_t expectedValue = sampleCount * kFramebufferWidth * kFramebufferHeight;

		if (fragCounter != expectedValue)
		{
			std::ostringstream msg;
			msg << "Fragment shader invocation count does not match expected value: found " << fragCounter << " and expected " << expectedValue;
			return tcu::TestStatus::fail(msg.str());
		}
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

class TestGroupWithClean : public tcu::TestCaseGroup
{
public:
	TestGroupWithClean	(tcu::TestContext& testCtx, const char* name, const char* description)
		: tcu::TestCaseGroup(testCtx, name, description)
		{}

	void deinit (void) override { cleanupDevices(); }
};

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // anonymous namespace

tcu::TestCaseGroup* createExtendedDynamicStateTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	GroupPtr extendedDynamicStateGroup(new TestGroupWithClean(testCtx, "extended_dynamic_state", "Tests for VK_EXT_extended_dynamic_state"));
	GroupPtr meshShaderGroup(new tcu::TestCaseGroup(testCtx, "mesh_shader", "Extended dynamic state with mesh shading pipelines"));

	// Auxiliar constants.
	const deUint32	kHalfWidthU	= kFramebufferWidth/2u;
	const deInt32	kHalfWidthI	= static_cast<deInt32>(kHalfWidthU);
	const float		kHalfWidthF	= static_cast<float>(kHalfWidthU);
	const float		kWidthF		= static_cast<float>(kFramebufferWidth);
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

	static const struct
	{
		bool			useMeshShaders;
		std::string		groupName;
	} kMeshShadingCases[] =
	{
		{ false,	""				},
#ifndef CTS_USES_VULKANSC
		{ true,		"mesh_shader"	},
#endif // CTS_USES_VULKANSC
	};

	static const struct
	{
		bool			bindUnusedMeshShadingPipeline;
		std::string		nameSuffix;
		std::string		descSuffix;
	} kBindUnusedCases[] =
	{
		{ false,	"",					""																},
#ifndef CTS_USES_VULKANSC
		{ true,		"_bind_unused_ms",	" and bind unused mesh shading pipeline before the dynamic one"	},
#endif // CTS_USES_VULKANSC
	};

	static const std::vector<ColorBlendSubCase> cbSubCases
	{
		ColorBlendSubCase::EQ_ONLY,
		ColorBlendSubCase::ALL_CB,
		ColorBlendSubCase::ALL_BUT_LO,
	};

	for (const auto& kMeshShadingCase : kMeshShadingCases)
	for (const auto& kOrderingCase : kOrderingCases)
	{
		if (vk::isConstructionTypeShaderObject(pipelineConstructionType) && (kOrderingCase.ordering == SequenceOrdering::BETWEEN_PIPELINES || kOrderingCase.ordering == SequenceOrdering::AFTER_PIPELINES))
			continue;

		const auto& kUseMeshShaders	= kMeshShadingCase.useMeshShaders;
		const auto& kOrdering		= kOrderingCase.ordering;

		GroupPtr orderingGroup(new tcu::TestCaseGroup(testCtx, kOrderingCase.name.c_str(), kOrderingCase.desc.c_str()));

		// Cull modes.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_FRONT_BIT;
			config.cullModeConfig.dynamicValue	= tcu::just<vk::VkCullModeFlags>(vk::VK_CULL_MODE_NONE);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "cull_none", "Dynamically set cull mode to none", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_FRONT_AND_BACK;
			config.cullModeConfig.dynamicValue	= tcu::just<vk::VkCullModeFlags>(vk::VK_CULL_MODE_BACK_BIT);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "cull_back", "Dynamically set cull mode to back", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			// Make triangles look back.
			config.meshParams[0].reversed		= true;
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.cullModeConfig.dynamicValue	= tcu::just<vk::VkCullModeFlags>(vk::VK_CULL_MODE_FRONT_BIT);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "cull_front", "Dynamically set cull mode to front", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_NONE;
			config.cullModeConfig.dynamicValue	= tcu::just<vk::VkCullModeFlags>(vk::VK_CULL_MODE_FRONT_AND_BACK);
			config.referenceColor.reset			(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "cull_front_and_back", "Dynamically set cull mode to front and back", config));
		}

		// Front face.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.frontFaceConfig.staticValue	= vk::VK_FRONT_FACE_CLOCKWISE;
			config.frontFaceConfig.dynamicValue	= tcu::just<vk::VkFrontFace>(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "front_face_cw", "Dynamically set front face to clockwise", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			// Pass triangles in clockwise order.
			config.meshParams[0].reversed		= true;
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.frontFaceConfig.staticValue	= vk::VK_FRONT_FACE_COUNTER_CLOCKWISE;
			config.frontFaceConfig.dynamicValue	= tcu::just<vk::VkFrontFace>(vk::VK_FRONT_FACE_CLOCKWISE);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "front_face_ccw", "Dynamically set front face to counter-clockwise", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.frontFaceConfig.staticValue	= vk::VK_FRONT_FACE_COUNTER_CLOCKWISE;
			config.frontFaceConfig.dynamicValue	= tcu::just<vk::VkFrontFace>(vk::VK_FRONT_FACE_CLOCKWISE);
			config.referenceColor.reset			(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "front_face_cw_reversed", "Dynamically set front face to clockwise with a counter-clockwise mesh", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			// Pass triangles in clockwise order.
			config.meshParams[0].reversed		= true;
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;
			config.frontFaceConfig.staticValue	= vk::VK_FRONT_FACE_CLOCKWISE;
			config.frontFaceConfig.dynamicValue	= tcu::just<vk::VkFrontFace>(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE);
			config.referenceColor.reset			(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "front_face_ccw_reversed", "Dynamically set front face to counter-clockwise with a clockwise mesh", config));
		}

		// Rasterizer discard
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.rastDiscardEnableConfig.staticValue	= false;
			config.rastDiscardEnableConfig.dynamicValue	= tcu::just(true);
			config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "disable_raster", "Dynamically disable rasterizer", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.rastDiscardEnableConfig.staticValue	= true;
			config.rastDiscardEnableConfig.dynamicValue	= tcu::just(false);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "enable_raster", "Dynamically enable rasterizer", config));
		}

		// Logic op
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.logicOpEnableConfig.staticValue	= true;
			config.logicOpConfig.staticValue		= vk::VK_LOGIC_OP_CLEAR;
			config.logicOpConfig.dynamicValue		= tcu::just<vk::VkLogicOp>(vk::VK_LOGIC_OP_OR);

			// Clear to green, paint in blue, expect cyan due to logic op.
			config.meshParams[0].color	= kLogicOpTriangleColorFl;
			config.clearColorValue		= vk::makeClearValueColorU32(kGreenClearColor.x(), kGreenClearColor.y(), kGreenClearColor.z(), kGreenClearColor.w());
			config.referenceColor.reset	(new SingleColorGenerator(kLogicOpFinalColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "logic_op_or", "Dynamically change logic op to VK_LOGIC_OP_OR", config));
		}

		// Logic op enable.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.logicOpEnableConfig.staticValue	= false;
			config.logicOpEnableConfig.dynamicValue	= true;
			config.logicOpConfig.staticValue		= vk::VK_LOGIC_OP_OR;

			// Clear to green, paint in blue, expect cyan due to logic op.
			config.meshParams[0].color	= kLogicOpTriangleColorFl;
			config.clearColorValue		= vk::makeClearValueColorU32(kGreenClearColor.x(), kGreenClearColor.y(), kGreenClearColor.z(), kGreenClearColor.w());
			config.referenceColor.reset (new SingleColorGenerator(kLogicOpFinalColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "logic_op_enable", "Dynamically enable logic OP", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.logicOpEnableConfig.staticValue	= true;
			config.logicOpEnableConfig.dynamicValue	= false;
			config.logicOpConfig.staticValue		= vk::VK_LOGIC_OP_OR;

			// Clear to green, paint in blue, expect cyan due to logic op.
			config.meshParams[0].color	= kLogicOpTriangleColorFl;
			config.clearColorValue		= vk::makeClearValueColorU32(kGreenClearColor.x(), kGreenClearColor.y(), kGreenClearColor.z(), kGreenClearColor.w());
			config.referenceColor.reset	(new SingleColorGenerator(kLogicOpTriangleColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "logic_op_disable", "Dynamically disable logic OP", config));
		}

		// Color blend enable.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			// The equation picks the old color instead of the new one if blending is enabled.
			config.colorBlendEquationConfig.staticValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ZERO,
																	   vk::VK_BLEND_FACTOR_ONE,
																	   vk::VK_BLEND_OP_ADD,
																	   vk::VK_BLEND_FACTOR_ZERO,
																	   vk::VK_BLEND_FACTOR_ONE,
																	   vk::VK_BLEND_OP_ADD);

			config.colorBlendEnableConfig.staticValue	= false;
			config.colorBlendEnableConfig.dynamicValue	= true;
			config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "color_blend_enable", "Dynamically enable color blending", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			// The equation picks the old color instead of the new one if blending is enabled.
			config.colorBlendEquationConfig.staticValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ZERO,
																	   vk::VK_BLEND_FACTOR_ONE,
																	   vk::VK_BLEND_OP_ADD,
																	   vk::VK_BLEND_FACTOR_ZERO,
																	   vk::VK_BLEND_FACTOR_ONE,
																	   vk::VK_BLEND_OP_ADD);

			config.colorBlendEnableConfig.staticValue	= true;
			config.colorBlendEnableConfig.dynamicValue	= false;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "color_blend_disable", "Dynamically disable color blending", config));
		}

		// Color blend equation.
		{
			for (const auto& cbSubCase : cbSubCases)
			{
				const bool onlyEq		= (cbSubCase == ColorBlendSubCase::EQ_ONLY);
				const bool allCBDynamic	= (cbSubCase == ColorBlendSubCase::ALL_CB);

				// Skip two-draws variants as this will use dynamic logic op and force UNORM color attachments, which would result in illegal operations.
				if (allCBDynamic && (kOrdering == SequenceOrdering::TWO_DRAWS_STATIC || kOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC))
					continue;

				for (int j = 0; j < 2; ++j)
				{
					const bool enableStateValue = (j > 0);

					// Do not test statically disabling color blend.
					if (onlyEq && !enableStateValue)
						continue;

					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

					// The equation picks the old color instead of the new one if blending is enabled.
					config.colorBlendEquationConfig.staticValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ZERO,
																			   vk::VK_BLEND_FACTOR_ONE,
																			   vk::VK_BLEND_OP_ADD,
																			   vk::VK_BLEND_FACTOR_ZERO,
																			   vk::VK_BLEND_FACTOR_ONE,
																			   vk::VK_BLEND_OP_ADD);

					// The dynamic value picks the new color.
					config.colorBlendEquationConfig.dynamicValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ONE,
																				vk::VK_BLEND_FACTOR_ZERO,
																				vk::VK_BLEND_OP_ADD,
																				vk::VK_BLEND_FACTOR_ONE,
																				vk::VK_BLEND_FACTOR_ZERO,
																				vk::VK_BLEND_OP_ADD);

					if (!onlyEq)
					{
						config.colorBlendEnableConfig.staticValue	= !enableStateValue;
						config.colorBlendEnableConfig.dynamicValue	= enableStateValue;
						config.colorWriteMaskConfig.staticValue		= ( 0 |  0 |  0 |  0);
						config.colorWriteMaskConfig.dynamicValue	= (CR | CG | CB | CA);
						config.blendConstantsConfig.staticValue		= BlendConstArray{1.0f, 1.0f, 1.0f, 1.0f};
						config.blendConstantsConfig.dynamicValue	= BlendConstArray{0.0f, 0.0f, 0.0f, 0.0f};
						// Note we don't set a dynamic value for alpha to coverage.

						config.useColorWriteEnable					= true;
						config.colorWriteEnableConfig.staticValue	= false;
						config.colorWriteEnableConfig.dynamicValue	= true;

						if (allCBDynamic)
						{
							config.forceUnormColorFormat				= true;
							config.logicOpEnableConfig.staticValue		= true;
							config.logicOpEnableConfig.dynamicValue		= false;
							config.logicOpConfig.staticValue			= vk::VK_LOGIC_OP_COPY;
							config.logicOpConfig.dynamicValue			= vk::VK_LOGIC_OP_CLEAR;
						}
					}
					else
					{
						config.colorBlendEnableConfig.staticValue	= enableStateValue;
					}

					const std::string stateStr		= (enableStateValue ? "enable" : "disable");
					const std::string nameSuffix	= (onlyEq ? "" : (allCBDynamic ? ("_dynamic_" + stateStr) : ("_dynamic_but_logic_op_" + stateStr)));
					const std::string descSuffix	= (onlyEq ? "" : (allCBDynamic ? " and dynamically enable color blending" : " and dynamically enable color blending except for logic op"));

					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "color_blend_equation_new_color" + nameSuffix, "Dynamically set a color equation that picks the mesh color" + descSuffix, config));

					config.colorBlendEquationConfig.swapValues();
					config.referenceColor.reset(new SingleColorGenerator(enableStateValue ? kDefaultClearColor : kDefaultTriangleColor));

					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "color_blend_equation_old_color" + nameSuffix, "Dynamically set a color equation that picks the clear color" + descSuffix, config));
				}
			}
		}

		// Color blend advanced.
		{
			for (const auto& cbSubCase : cbSubCases)
			{
				const bool onlyEq		= (cbSubCase == ColorBlendSubCase::EQ_ONLY);
				const bool allCBDynamic	= (cbSubCase == ColorBlendSubCase::ALL_CB);

				// Skip two-draws variants as this will use dynamic logic op and force UNORM color attachments, which would result in illegal operations.
				if (allCBDynamic && (kOrdering == SequenceOrdering::TWO_DRAWS_STATIC || kOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC))
					continue;

				for (int j = 0; j < 2; ++j)
				{
					const bool enableStateValue = (j > 0);

					// Do not test statically disabling color blend.
					if (onlyEq && !enableStateValue)
						continue;

					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

					// This static value picks the old color instead of the new one.
					config.colorBlendEquationConfig.staticValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ZERO,
																			   vk::VK_BLEND_FACTOR_ONE,
																			   vk::VK_BLEND_OP_DARKEN_EXT,
																			   vk::VK_BLEND_FACTOR_ZERO,
																			   vk::VK_BLEND_FACTOR_ONE,
																			   vk::VK_BLEND_OP_DARKEN_EXT);

					// The dynamic value picks the new color.
					config.colorBlendEquationConfig.dynamicValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ONE,
																				vk::VK_BLEND_FACTOR_ZERO,
																				vk::VK_BLEND_OP_LIGHTEN_EXT,
																				vk::VK_BLEND_FACTOR_ONE,
																				vk::VK_BLEND_FACTOR_ZERO,
																				vk::VK_BLEND_OP_LIGHTEN_EXT);

					if (!onlyEq)
					{
						config.colorBlendEnableConfig.staticValue	= !enableStateValue;
						config.colorBlendEnableConfig.dynamicValue	= enableStateValue;
						config.colorWriteMaskConfig.staticValue		= ( 0 |  0 |  0 |  0);
						config.colorWriteMaskConfig.dynamicValue	= (CR | CG | CB | CA);
						config.blendConstantsConfig.staticValue		= BlendConstArray{1.0f, 1.0f, 1.0f, 1.0f};
						config.blendConstantsConfig.dynamicValue	= BlendConstArray{0.0f, 0.0f, 0.0f, 0.0f};
						// Note we don't set a dynamic value for alpha to coverage.

						config.useColorWriteEnable					= true;
						config.colorWriteEnableConfig.staticValue	= false;
						config.colorWriteEnableConfig.dynamicValue	= true;

						if (allCBDynamic)
						{
							config.forceUnormColorFormat				= true;
							config.logicOpEnableConfig.staticValue		= true;
							config.logicOpEnableConfig.dynamicValue		= false;
							config.logicOpConfig.staticValue			= vk::VK_LOGIC_OP_COPY;
							config.logicOpConfig.dynamicValue			= vk::VK_LOGIC_OP_CLEAR;
						}
					}
					else
					{
						config.colorBlendEnableConfig.staticValue	= true;
					}

					const std::string stateStr		= (enableStateValue ? "enable" : "disable");
					const std::string nameSuffix	= (onlyEq ? "" : (allCBDynamic ? ("_dynamic_" + stateStr) : ("_dynamic_but_logic_op_" + stateStr)));
					const std::string descSuffix	= (onlyEq ? "" : (allCBDynamic ? " and dynamically enable color blending" : " and dynamically enable color blending except for logic op"));

					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "color_blend_equation_advanced_new_color" + nameSuffix, "Dynamically set an advanced color equation that picks the mesh color" + descSuffix, config));

					config.colorBlendEquationConfig.swapValues();
					config.referenceColor.reset(new SingleColorGenerator(enableStateValue ? kDefaultClearColor : kDefaultTriangleColor));

					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "color_blend_equation_advanced_old_color" + nameSuffix, "Dynamically set an advanced color equation that picks the clear color" + descSuffix, config));
				}
			}
		}

		// All color blend as dynamic, including both blend equations.
		{
			for (int i = 0; i < 2; ++i)
			{
				for (int j = 0; j < 2; ++j)
				{
					const bool swapEquation			= (j > 0);
					const bool picksNew				= (!swapEquation);
					const auto colorBlendResultName	= (picksNew ? "new" : "old");

					const bool colorBlendEnableDyn		= (i > 0);
					const bool colorBlendEnableStatic	= !colorBlendEnableDyn;
					const auto colorBlendStateName		= (colorBlendEnableDyn ? "enabled" : "disabled");

					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

					// We need to apply both color blending equation states instead of deciding if it's advanced or not.
					config.colorBlendBoth						= true;

					config.colorBlendEnableConfig.staticValue	= colorBlendEnableStatic;
					config.colorBlendEnableConfig.dynamicValue	= colorBlendEnableDyn;

					config.colorWriteMaskConfig.staticValue		= ( 0 |  0 |  0 |  0);
					config.colorWriteMaskConfig.dynamicValue	= (CR | CG | CB | CA);
					config.blendConstantsConfig.staticValue		= BlendConstArray{1.0f, 1.0f, 1.0f, 1.0f};
					config.blendConstantsConfig.dynamicValue	= BlendConstArray{0.0f, 0.0f, 0.0f, 0.0f};

					config.useColorWriteEnable					= true;
					config.colorWriteEnableConfig.staticValue	= false;
					config.colorWriteEnableConfig.dynamicValue	= true;

					config.forceUnormColorFormat				= true;
					config.logicOpEnableConfig.staticValue		= true;
					config.logicOpEnableConfig.dynamicValue		= false;
					config.logicOpConfig.staticValue			= vk::VK_LOGIC_OP_COPY;
					config.logicOpConfig.dynamicValue			= vk::VK_LOGIC_OP_CLEAR;

					// This static value picks the new color.
					config.colorBlendEquationConfig.staticValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ONE,
																			   vk::VK_BLEND_FACTOR_ZERO,
																			   vk::VK_BLEND_OP_LIGHTEN_EXT,
																			   vk::VK_BLEND_FACTOR_ONE,
																			   vk::VK_BLEND_FACTOR_ZERO,
																			   vk::VK_BLEND_OP_LIGHTEN_EXT);

					// The dynamic value picks the old color instead of the new one.
					config.colorBlendEquationConfig.dynamicValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ZERO,
																				vk::VK_BLEND_FACTOR_ONE,
																				vk::VK_BLEND_OP_DARKEN_EXT,
																				vk::VK_BLEND_FACTOR_ZERO,
																				vk::VK_BLEND_FACTOR_ONE,
																				vk::VK_BLEND_OP_DARKEN_EXT);

					if (swapEquation)
						config.colorBlendEquationConfig.swapValues();

					// Expected result.
					const auto expectGeomColor = (!colorBlendEnableDyn || swapEquation);
					config.referenceColor.reset(new SingleColorGenerator(expectGeomColor ? kDefaultTriangleColor : kDefaultClearColor));

					const auto testName = std::string("color_blend_all_") + colorBlendStateName + "_" + colorBlendResultName + "_color";
					const auto testDesc = std::string(std::string("Set all color blend to dynamic and dynamically set color blend to ") + colorBlendStateName + " and pick the " + colorBlendResultName + " color");
					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, testDesc, config));
				}
			}
		}

		// Dynamic color blend equation with dual blending.
		{
			// Two equations: one picks index 0 and the other one picks index 1.
			const struct
			{
				const ColorBlendEq	equation;
				const tcu::Vec4		expectedColor;
			} dualSrcCases[] =
			{
				{
					ColorBlendEq(vk::VK_BLEND_FACTOR_SRC_COLOR,
								 vk::VK_BLEND_FACTOR_ZERO,
								 vk::VK_BLEND_OP_ADD,
								 vk::VK_BLEND_FACTOR_SRC_ALPHA,
								 vk::VK_BLEND_FACTOR_ZERO,
								 vk::VK_BLEND_OP_ADD),
					// This matches our logic in the frag shader for the first color index.
					kOpaqueWhite,
				},
				{
					ColorBlendEq(vk::VK_BLEND_FACTOR_SRC1_COLOR,
								 vk::VK_BLEND_FACTOR_ZERO,
								 vk::VK_BLEND_OP_ADD,
								 vk::VK_BLEND_FACTOR_SRC1_ALPHA,
								 vk::VK_BLEND_FACTOR_ZERO,
								 vk::VK_BLEND_OP_ADD),
					// This matches our logic in the frag shader for color1.
					kDefaultTriangleColor,
				},
			};

			for (size_t dynamicPick = 0u; dynamicPick < de::arrayLength(dualSrcCases); ++dynamicPick)
			{
				DE_ASSERT(de::arrayLength(dualSrcCases) == size_t{2});

				const auto& dynamicEq	= dualSrcCases[dynamicPick].equation;
				const auto& staticEq	= dualSrcCases[size_t{1} - dynamicPick].equation;

				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.dualSrcBlend								= true;
				config.colorBlendEnableConfig.staticValue		= true;
				config.colorBlendEquationConfig.staticValue		= staticEq;
				config.colorBlendEquationConfig.dynamicValue	= dynamicEq;
				config.referenceColor.reset						(new SingleColorGenerator(dualSrcCases[dynamicPick].expectedColor));

				const auto indexStr = std::to_string(dynamicPick);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "color_blend_dual_index_" + indexStr, "Dynamically change dual source blending equation to pick color index " + indexStr, config));
			}
		}

		// Null color blend pipeline pAttachments pointer with all structure contents as dynamic states.
		{
			TestConfig config (pipelineConstructionType, kOrdering, kUseMeshShaders);

			// The equation picks the old color instead of the new one if blending is enabled.
			config.colorBlendEquationConfig.staticValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ZERO,
																		vk::VK_BLEND_FACTOR_ONE,
																		vk::VK_BLEND_OP_ADD,
																		vk::VK_BLEND_FACTOR_ZERO,
																		vk::VK_BLEND_FACTOR_ONE,
																		vk::VK_BLEND_OP_ADD);

			// The dynamic value picks the new color.
			config.colorBlendEquationConfig.dynamicValue = ColorBlendEq(vk::VK_BLEND_FACTOR_ONE,
																		vk::VK_BLEND_FACTOR_ZERO,
																		vk::VK_BLEND_OP_ADD,
																		vk::VK_BLEND_FACTOR_ONE,
																		vk::VK_BLEND_FACTOR_ZERO,
																		vk::VK_BLEND_OP_ADD);

			config.colorBlendEnableConfig.staticValue	= false;
			config.colorBlendEnableConfig.dynamicValue	= true;

			config.colorWriteMaskConfig.staticValue		= ( 0 |  0 |  0 |  0);
			config.colorWriteMaskConfig.dynamicValue	= (CR | CG | CB | CA);

			config.nullStaticColorBlendAttPtr			= true; // What this test is about.

			const char* testName = "null_color_blend_att_ptr";
			const char* testDesc = "Set all VkPipelineColorBlendAttachmentState substates as dynamic and pass a null pointer in VkPipelineColorBlendStateCreateInfo::pAttachments";
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, testDesc, config));
		}

		// Dynamically enable primitive restart
		if (!kUseMeshShaders)
		{
			for (const auto& bindUnusedCase : kBindUnusedCases)
			{
				if (bindUnusedCase.bindUnusedMeshShadingPipeline && kOrdering != SequenceOrdering::CMD_BUFFER_START)
					continue;

				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.topologyConfig.staticValue			= vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
				config.extraLineRestarts					= true;
				config.primRestartEnableConfig.staticValue	= false;
				config.primRestartEnableConfig.dynamicValue	= tcu::just(true);
				config.bindUnusedMeshShadingPipeline		= bindUnusedCase.bindUnusedMeshShadingPipeline;
				config.referenceColor.reset					(new CenterStripGenerator(kDefaultTriangleColor, kDefaultClearColor));
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, std::string("prim_restart_enable") + bindUnusedCase.nameSuffix, "Dynamically enable primitiveRestart" + bindUnusedCase.descSuffix, config));
			}
		}

		// Dynamically change the number of primitive control points
		if (!kUseMeshShaders)
		{
			for (const auto& bindUnusedCase : kBindUnusedCases)
			{
				if (bindUnusedCase.bindUnusedMeshShadingPipeline && kOrdering != SequenceOrdering::CMD_BUFFER_START)
					continue;

				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.topologyConfig.staticValue = vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
				config.patchControlPointsConfig.staticValue = 1;
				config.patchControlPointsConfig.dynamicValue = 3;
				config.bindUnusedMeshShadingPipeline = bindUnusedCase.bindUnusedMeshShadingPipeline;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "patch_control_points" + bindUnusedCase.nameSuffix, "Dynamically change patch control points" + bindUnusedCase.descSuffix, config));
			}

			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.topologyConfig.staticValue = vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
				config.patchControlPointsConfig.staticValue = 1;
				config.patchControlPointsConfig.dynamicValue = 3;
				config.useExtraDynPCPPipeline = true;

				const auto testName	= "patch_control_points_extra_pipeline";
				const auto testDesc	= "Dynamically change patch control points and draw first with a pipeline using the state and no tessellation shaders";

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, testDesc, config));
			}
		}

		// Test tessellation domain origin.
		if (!kUseMeshShaders)
		{
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.topologyConfig.staticValue = vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
				config.patchControlPointsConfig.staticValue = 3;
				config.tessDomainOriginConfig.staticValue = vk::VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;
				config.tessDomainOriginConfig.dynamicValue = vk::VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT;
				config.cullModeConfig.staticValue = vk::VK_CULL_MODE_BACK_BIT;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "tess_domain_origin_lower_left", "Dynamically set the right domain origin to lower left", config));
			}
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.topologyConfig.staticValue = vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
				config.patchControlPointsConfig.staticValue = 3;
				config.tessDomainOriginConfig.staticValue = vk::VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT;
				config.tessDomainOriginConfig.dynamicValue = vk::VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;
				config.cullModeConfig.staticValue = vk::VK_CULL_MODE_FRONT_BIT;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "tess_domain_origin_upper_left", "Dynamically set the right domain origin to upper left", config));
			}
		}

		// Dynamic topology.
		if (!kUseMeshShaders)
		{
			TestConfig baseConfig(pipelineConstructionType, kOrdering, kUseMeshShaders);

			for (int i = 0; i < 2; ++i)
			{
				const bool forceGeometryShader = (i > 0);

				static const struct
				{
					vk::VkPrimitiveTopology staticVal;
					vk::VkPrimitiveTopology dynamicVal;
				} kTopologyCases[] =
				{
					{ vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP	},
					{ vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST,		vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP		},
					{ vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,		vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST		},
				};

				for (const auto& kTopologyCase : kTopologyCases)
				{
					const auto topologyClass = getTopologyClass(kTopologyCase.staticVal);

					for (const auto& bindUnusedCase : kBindUnusedCases)
					{
						if (bindUnusedCase.bindUnusedMeshShadingPipeline && kOrdering != SequenceOrdering::CMD_BUFFER_START)
							continue;

						TestConfig config(baseConfig);
						config.forceGeometryShader					= forceGeometryShader;
						config.topologyConfig.staticValue			= kTopologyCase.staticVal;
						config.topologyConfig.dynamicValue			= tcu::just<vk::VkPrimitiveTopology>(kTopologyCase.dynamicVal);
						config.primRestartEnableConfig.staticValue	= (topologyClass == TopologyClass::LINE);
						config.patchControlPointsConfig.staticValue	= (config.needsTessellation() ? 3u : 1u);
						config.bindUnusedMeshShadingPipeline		= bindUnusedCase.bindUnusedMeshShadingPipeline;

						const std::string	className	= topologyClassName(topologyClass);
						const std::string	name		= "topology_" + className + (forceGeometryShader ? "_geom" : "") + bindUnusedCase.nameSuffix;
						const std::string	desc		= "Dynamically switch primitive topologies from the " + className + " class" + (forceGeometryShader ? " and use a geometry shader" : "") + bindUnusedCase.descSuffix;
						orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, name, desc, config));
					}
				}
			}
		}

		// Line stipple enable.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.primRestartEnableConfig.staticValue	= true;
			config.topologyConfig.staticValue			= vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			config.lineStippleEnableConfig.staticValue	= true;
			config.lineStippleEnableConfig.dynamicValue	= false;
			config.lineStippleParamsConfig.staticValue	= LineStippleParams{1u, 0x5555u};

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "line_stipple_disable", "Dynamically disable line stipple", config));

			config.lineStippleEnableConfig.swapValues();
			config.referenceColor.reset(new VerticalStripesGenerator(kDefaultTriangleColor, kDefaultClearColor, 1u));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "line_stipple_enable", "Dynamycally enable line stipple", config));
		}

		// Line stipple params.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.primRestartEnableConfig.staticValue	= true;
			config.topologyConfig.staticValue			= vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			config.lineStippleEnableConfig.staticValue	= true;
			config.lineStippleParamsConfig.staticValue	= LineStippleParams{1u, 0x5555u};
			config.lineStippleParamsConfig.dynamicValue	= LineStippleParams{2u, 0x3333u};
			config.referenceColor.reset					(new VerticalStripesGenerator(kDefaultTriangleColor, kDefaultClearColor, 4u));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "line_stipple_params", "Dynamically change the line stipple parameters", config));
		}

		// Line rasterization mode.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.topologyConfig.staticValue			= vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			config.obliqueLine							= true;
			config.colorVerificator						= verifyTopLeftCornerExactly;
			config.lineStippleEnableConfig.staticValue	= false;
			config.lineStippleParamsConfig.staticValue	= LineStippleParams{0u, 0u};
			config.lineRasterModeConfig.staticValue		= LineRasterizationMode::RECTANGULAR;
			config.lineRasterModeConfig.dynamicValue	= LineRasterizationMode::BRESENHAM;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "line_raster_mode_bresenham", "Dynamically set line rasterization mode to bresenham", config));

			config.lineRasterModeConfig.swapValues();
			config.referenceColor.reset(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "line_raster_mode_rectangular", "Dynamically set line rasterization mode to rectangular", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.topologyConfig.staticValue			= vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			config.obliqueLine							= true;
			config.colorVerificator						= verifyTopLeftCornerWithPartialAlpha;
			config.lineStippleEnableConfig.staticValue	= false;
			config.lineStippleParamsConfig.staticValue	= LineStippleParams{0u, 0u};
			config.lineRasterModeConfig.staticValue		= LineRasterizationMode::BRESENHAM;
			config.lineRasterModeConfig.dynamicValue	= LineRasterizationMode::SMOOTH;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "line_raster_mode_smooth", "Dynamically set line rasterization mode to smooth", config));
		}

		// Viewport.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
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
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			// Bad static reduced viewport.
			config.viewportConfig.staticValue	= ViewportVec(1u, vk::makeViewport(kHalfWidthU, kFramebufferHeight));
			config.viewportConfig.staticValue	= ViewportVec(1u, vk::makeViewport(kFramebufferWidth, kFramebufferHeight));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "1_full_viewport", "Dynamically set viewport to cover full framebuffer", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
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
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			// 2 scissors, reversed dynamic viewports that should result in no drawing taking place.
			config.scissorConfig.staticValue	= ScissorVec{vk::makeRect2D(0, 0, kHalfWidthU, kFramebufferHeight), vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight)};
			config.viewportConfig.staticValue	= ViewportVec{
				vk::makeViewport(0.0f, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),		// Left.
				vk::makeViewport(kHalfWidthF, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),	// Right.
			};
			config.viewportConfig.dynamicValue	= ViewportVec{config.viewportConfig.staticValue.back(), config.viewportConfig.staticValue.front()};
			config.referenceColor.reset			(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "2_viewports_switch_clean", "Dynamically switch the order with 2 viewports resulting in clean image", config));
		}

		// Scissor.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
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
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			// 1 viewport, bad static single scissor.
			config.scissorConfig.staticValue	= ScissorVec(1u, vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight));
			config.scissorConfig.dynamicValue	= ScissorVec(1u, vk::makeRect2D(kFramebufferWidth, kFramebufferHeight));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "1_full_scissor", "Dynamically set scissor to cover full framebuffer", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
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
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
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
			config.referenceColor.reset			(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "2_scissors_switch_clean", "Dynamically switch the order with 2 scissors to avoid drawing", config));
		}

		// Stride.
		if (!kUseMeshShaders)
		{
			struct
			{
				const VertexGenerator*	factory;
				const std::string		prefix;
			} strideCases[] =
			{
				{ getVertexWithPaddingGenerator(),			"stride"		},
				{ getVertexWithExtraAttributesGenerator(),	"large_stride"	},
			};

			for (const auto& strideCase : strideCases)
			{
				const auto	factory			= strideCase.factory;
				const auto&	prefix			= strideCase.prefix;
				const auto	vertexStrides	= factory->getVertexDataStrides();
				StrideVec	halfStrides;

				halfStrides.reserve(vertexStrides.size());
				for (const auto& stride : vertexStrides)
					halfStrides.push_back(stride / 2u);

				if (factory == getVertexWithExtraAttributesGenerator() && kOrdering == SequenceOrdering::TWO_DRAWS_STATIC)
				{
					// This case is invalid because it breaks VUID-vkCmdBindVertexBuffers2EXT-pStrides-03363 due to the dynamic
					// stride being less than the extent of the binding for the second attribute.
					continue;
				}

				for (const auto& bindUnusedCase : kBindUnusedCases)
				{
					if (bindUnusedCase.bindUnusedMeshShadingPipeline && kOrdering != SequenceOrdering::CMD_BUFFER_START)
						continue;

					{
						TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, factory);
						config.strideConfig.staticValue			= halfStrides;
						config.strideConfig.dynamicValue		= vertexStrides;
						config.bindUnusedMeshShadingPipeline	= bindUnusedCase.bindUnusedMeshShadingPipeline;
						orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, prefix + bindUnusedCase.nameSuffix, "Dynamically set stride" + bindUnusedCase.descSuffix, config));
					}
					{
						TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, factory);
						config.strideConfig.staticValue			= halfStrides;
						config.strideConfig.dynamicValue		= vertexStrides;
						config.vertexDataOffset					= vertexStrides[0];
						config.bindUnusedMeshShadingPipeline	= bindUnusedCase.bindUnusedMeshShadingPipeline;
						orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, prefix + "_with_offset" + bindUnusedCase.nameSuffix, "Dynamically set stride using a nonzero vertex data offset" + bindUnusedCase.descSuffix, config));
					}
					{
						TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, factory);
						config.strideConfig.staticValue			= halfStrides;
						config.strideConfig.dynamicValue		= vertexStrides;
						config.vertexDataOffset					= vertexStrides[0];
						config.vertexDataExtraBytes				= config.vertexDataOffset;
						config.bindUnusedMeshShadingPipeline	= bindUnusedCase.bindUnusedMeshShadingPipeline;

						// Make the mesh cover the top half only. If the implementation reads data outside the vertex values it may draw something to the bottom half.
						config.referenceColor.reset				(new HorizontalSplitGenerator(kDefaultTriangleColor, kDefaultClearColor));
						config.meshParams[0].scaleY				= 0.5f;
						config.meshParams[0].offsetY			= -0.5f;

						orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, prefix + "_with_offset_and_padding" + bindUnusedCase.nameSuffix, "Dynamically set stride using a nonzero vertex data offset and extra bytes" + bindUnusedCase.descSuffix, config));
					}
				}
			}

			// Dynamic stride of 0
			//
			// The "two_draws" variants are invalid because the non-zero vertex stride will cause out-of-bounds access
			// when drawing more than one vertex.
			if (kOrdering != SequenceOrdering::TWO_DRAWS_STATIC && kOrdering != SequenceOrdering::TWO_DRAWS_DYNAMIC)
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, getVertexWithExtraAttributesGenerator());
				config.strideConfig.staticValue		= config.getActiveVertexGenerator()->getVertexDataStrides();
				config.strideConfig.dynamicValue	= { 0 };
				config.vertexDataOffset				= 4;
				config.singleVertex					= true;
				config.singleVertexDrawCount		= 6;

				// Make the mesh cover the top half only. If the implementation reads data outside the vertex data it should read the
				// offscreen vertex and draw something in the bottom half.
				config.referenceColor.reset		(new HorizontalSplitGenerator(kDefaultTriangleColor, kDefaultClearColor));
				config.meshParams[0].scaleY		= 0.5f;
				config.meshParams[0].offsetY	= -0.5f;

				// Use strip scale to synthesize a strip from a vertex attribute which remains constant over the draw call.
				config.meshParams[0].stripScale = 1.0f;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "zero_stride_with_offset", "Dynamically set zero stride using a nonzero vertex data offset", config));
			}
		}

		// Depth test enable.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.depthTestEnableConfig.staticValue	= false;
			config.depthTestEnableConfig.dynamicValue	= tcu::just(true);
			// By default, the depth test never passes when enabled.
			config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_test_enable", "Dynamically enable depth test", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.depthTestEnableConfig.staticValue	= true;
			config.depthTestEnableConfig.dynamicValue	= tcu::just(false);
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_test_disable", "Dynamically disable depth test", config));
		}

		// Depth write enable.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

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
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

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

		// Depth clamp enable.
		{
			// Without clamping, the mesh depth fails the depth test after applying the viewport transform.
			// With clamping, it should pass thanks to the viewport.
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.meshParams[0].depth					= 1.5f;
			config.clearDepthValue						= 0.625f;
			config.depthTestEnableConfig.staticValue	= true;
			config.depthWriteEnableConfig.staticValue	= true;
			config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_LESS;
			config.viewportConfig.staticValue			= ViewportVec(1u, vk::makeViewport(0.0f, 0.0f, kWidthF, kHeightF, 0.0f, 0.5f));
			config.expectedDepth						= 0.5f;

			config.depthClampEnableConfig.staticValue	= false;
			config.depthClampEnableConfig.dynamicValue	= true;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_clamp_enable", "Dynamically enable depth clamp", config));
		}
		{
			// Reverse situation.
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.meshParams[0].depth					= 1.5f;
			config.clearDepthValue						= 0.625f;
			config.depthTestEnableConfig.staticValue	= true;
			config.depthWriteEnableConfig.staticValue	= true;
			config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_LESS;
			config.viewportConfig.staticValue			= ViewportVec(1u, vk::makeViewport(0.0f, 0.0f, kWidthF, kHeightF, 0.0f, 0.5f));
			config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));
			config.expectedDepth						= 0.625f;

			config.depthClampEnableConfig.staticValue	= true;
			config.depthClampEnableConfig.dynamicValue	= false;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_clamp_disable", "Dynamically disable depth clamp", config));
		}

#if 0
		// "If the depth clamping state is changed dynamically, and the pipeline was not created with
		// VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT enabled, then depth clipping is enabled when depth clamping is disabled and vice
		// versa"
		//
		// Try to verify the implementation ignores the static depth clipping state. We cannot test the following sequence orderings for this:
		// - BEFORE_GOOD_STATIC and TWO_DRAWS_STATIC because they use static-state pipelines, but for this specific case we need dynamic state as per the spec.
		// - TWO_DRAWS_DYNAMIC because the first draw may modify the framebuffer with undesired side-effects.
		if (kOrdering != SequenceOrdering::BEFORE_GOOD_STATIC && kOrdering != SequenceOrdering::TWO_DRAWS_DYNAMIC && kOrdering != SequenceOrdering::TWO_DRAWS_STATIC)
		{
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.meshParams[0].depth					= -0.5f;
				config.clearDepthValue						= 1.0f;
				config.depthTestEnableConfig.staticValue	= true;
				config.depthWriteEnableConfig.staticValue	= true;
				config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_ALWAYS;
				config.viewportConfig.staticValue			= ViewportVec(1u, vk::makeViewport(0.0f, 0.0f, kWidthF, kHeightF, 0.5f, 1.0f));
				config.expectedDepth						= 0.5f; // Geometry will be clamped to this value.

				config.depthClampEnableConfig.staticValue	= false;
				config.depthClampEnableConfig.dynamicValue	= true;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_clamp_enable_no_clip", "Dynamically enable depth clamp while making sure depth clip is disabled", config));
			}
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.meshParams[0].depth					= -0.5f;
				config.clearDepthValue						= 1.0f;
				config.depthTestEnableConfig.staticValue	= true;
				config.depthWriteEnableConfig.staticValue	= true;
				config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_ALWAYS;
				config.viewportConfig.staticValue			= ViewportVec(1u, vk::makeViewport(0.0f, 0.0f, kWidthF, kHeightF, 0.5f, 1.0f));
				config.expectedDepth						= 1.0f; // Geometry should be clipped in this case.
				config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));

				// Enable clamping dynamically, with clipping enabled statically.
				config.depthClampEnableConfig.staticValue	= false;
				config.depthClampEnableConfig.dynamicValue	= true;
				config.depthClipEnableConfig.staticValue	= OptBoolean(true);

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_clamp_enable_with_clip", "Dynamically enable depth clamp while keeping depth clip enabled statically", config));
			}
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.meshParams[0].depth					= -0.5f;
				config.clearDepthValue						= 1.0f;
				config.depthTestEnableConfig.staticValue	= true;
				config.depthWriteEnableConfig.staticValue	= true;
				config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_ALWAYS;
				config.viewportConfig.staticValue			= ViewportVec(1u, vk::makeViewport(0.0f, 0.0f, kWidthF, kHeightF, 0.5f, 1.0f));
				config.expectedDepth						= 1.0f; // Geometry should be clipped in this case.
				config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));

				config.depthClampEnableConfig.staticValue	= true;
				config.depthClampEnableConfig.dynamicValue	= false;
				if (vk::isConstructionTypeShaderObject(pipelineConstructionType))
					config.depthClipEnableConfig.staticValue = OptBoolean(true);

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_clamp_disable_with_clip", "Dynamically disable depth clamp making sure depth clipping is enabled", config));
			}
			// Note: the combination of depth clamp disabled and depth clip disabled cannot be tested because if Zf falls outside
			// [Zmin,Zmax] from the viewport, then the value of Zf is undefined during the depth test.
		}
#endif

		// Polygon mode.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.polygonModeConfig.staticValue	= vk::VK_POLYGON_MODE_FILL;
			config.polygonModeConfig.dynamicValue	= vk::VK_POLYGON_MODE_POINT;
			config.oversizedTriangle				= true;
			config.referenceColor.reset				(new SingleColorGenerator(kDefaultClearColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "polygon_mode_point", "Dynamically set polygon draw mode to points", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.polygonModeConfig.staticValue	= vk::VK_POLYGON_MODE_POINT;
			config.polygonModeConfig.dynamicValue	= vk::VK_POLYGON_MODE_FILL;
			config.oversizedTriangle				= true;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "polygon_mode_fill", "Dynamically set polygon draw mode to fill", config));
		}

		for (int i = 0; i < 2; ++i)
		{
			const bool			multisample			= (i > 0);
			const auto			activeSampleCount	= (multisample ? kMultiSampleCount : kSingleSampleCount);
			const auto			inactiveSampleCount	= (multisample ? kSingleSampleCount : kMultiSampleCount);
			const std::string	namePrefix			= (multisample ? "multi_sample_" : "single_sample_");
			const std::string	descSuffix			= (multisample ? " in multisample mode" : " in single sample mode");

			// Sample count.
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				// The static pipeline would be illegal due to VUID-VkGraphicsPipelineCreateInfo-multisampledRenderToSingleSampled-06853.
				if (!config.useStaticPipeline())
				{
					config.rasterizationSamplesConfig.staticValue	= inactiveSampleCount;
					config.rasterizationSamplesConfig.dynamicValue	= activeSampleCount;
					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, namePrefix + "rasterization_samples", "Dynamically set the rasterization sample count" + descSuffix, config));
				}
			}

			// Sample mask
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.rasterizationSamplesConfig		= activeSampleCount;
				config.sampleMaskConfig.staticValue		= SampleMaskVec(1u, 0u);
				config.sampleMaskConfig.dynamicValue	= SampleMaskVec(1u, 0xFFu);

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, namePrefix + "sample_mask_enable", "Dynamically set a sample mask that allows drawing" + descSuffix, config));
			}
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.rasterizationSamplesConfig		= activeSampleCount;
				config.sampleMaskConfig.staticValue		= SampleMaskVec(1u, 0xFFu);
				config.sampleMaskConfig.dynamicValue	= SampleMaskVec(1u, 0u);
				config.referenceColor.reset				(new SingleColorGenerator(kDefaultClearColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, namePrefix + "sample_mask_disable", "Dynamically set a sample mask that prevents drawing" + descSuffix, config));
			}

			// Alpha to coverage.
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.rasterizationSamplesConfig			= activeSampleCount;
				config.meshParams[0].color					= kTransparentColor;
				config.alphaToCoverageConfig.staticValue	= false;
				config.alphaToCoverageConfig.dynamicValue	= true;
				config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, namePrefix + "alpha_to_coverage_enable", "Dynamically enable alpha to coverage" + descSuffix, config));
			}
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.rasterizationSamplesConfig			= activeSampleCount;
				config.meshParams[0].color					= kTransparentColor;
				config.alphaToCoverageConfig.staticValue	= true;
				config.alphaToCoverageConfig.dynamicValue	= false;
				config.referenceColor.reset					(new SingleColorGenerator(kTransparentColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, namePrefix + "alpha_to_coverage_disable", "Dynamically disable alpha to coverage" + descSuffix, config));
			}

			// Alpha to one.
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.rasterizationSamplesConfig		= activeSampleCount;
				config.meshParams[0].color				= kTransparentColor;
				config.alphaToOneConfig.staticValue		= false;
				config.alphaToOneConfig.dynamicValue	= true;
				config.referenceColor.reset				(new SingleColorGenerator(kDefaultTriangleColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, namePrefix + "alpha_to_one_enable", "Dynamically enable alpha to one" + descSuffix, config));
			}
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.rasterizationSamplesConfig		= activeSampleCount;
				config.meshParams[0].color				= kTransparentColor;
				config.alphaToOneConfig.staticValue		= true;
				config.alphaToOneConfig.dynamicValue	= false;
				config.referenceColor.reset				(new SingleColorGenerator(kTransparentColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, namePrefix + "alpha_to_one_disable", "Dynamically disable alpha to one" + descSuffix, config));
			}
		}

		// Special sample mask case: make sure the dynamic sample mask count does not overwrite the actual sample mask.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			// There's guaranteed support for 1 sample and 4 samples. So the official pipeline sample count will be 1 sample, and
			// the one we'll use in the dynamic sample mask call will be 4.
			//
			// When using 4 samples, sample 3 uses a Y offset of 0.875 pixels, so we'll use an off-center triangle to try to trick
			// the implementation into having that one covered by using a Y offset of 0.75.
			config.dynamicSampleMaskCount			= tcu::just(kMultiSampleCount);
			config.sampleMaskConfig.staticValue		= SampleMaskVec(1u, 0u);
			config.sampleMaskConfig.dynamicValue	= SampleMaskVec(1u, 0xFFu);
			config.offCenterTriangle				= true;
			config.offCenterProportion				= tcu::Vec2(0.0f, 0.75f);
			config.referenceColor.reset				(new TopLeftBorderGenerator(kDefaultTriangleColor, kDefaultTriangleColor, kDefaultClearColor, kDefaultClearColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "sample_mask_count", "Dynamically set sample mask with slightly different sample count", config));
		}

		// Special rasterization samples case: make sure rasterization samples is taken from the dynamic value, but provide a larger mask.
		{
			const auto kLargeRasterizationSampleCount = vk::VK_SAMPLE_COUNT_64_BIT;
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			// We cannot create a static pipeline with the configuration below because the render pass attachments will have a
			// sample count of kMultiSampleCount and VUID-VkGraphicsPipelineCreateInfo-multisampledRenderToSingleSampled-06853
			// applies here.
			if (!config.useStaticPipeline())
			{
				config.rasterizationSamplesConfig.staticValue	= kLargeRasterizationSampleCount;
				config.rasterizationSamplesConfig.dynamicValue	= kMultiSampleCount;
				config.sampleMaskConfig.staticValue				= SampleMaskVec{ 0xFFFFFFF0u, 0xFFFFFFFFu }; // Last 4 bits off.
				config.referenceColor.reset						(new SingleColorGenerator(kDefaultClearColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "large_static_rasterization_samples_off", "Dynamically set the rasterization samples to a low value while disabling bits corresponding to the dynamic sample count", config));

				config.sampleMaskConfig.staticValue				= SampleMaskVec{ 0xFu, 0u }; // Last 4 bits on.
				config.referenceColor.reset						(new SingleColorGenerator(kDefaultTriangleColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "large_static_rasterization_samples_on", "Dynamically set the rasterization samples to a low value while enabling bits corresponding to the dynamic sample count", config));
			}
		}

		// Color write mask.
		{
			const struct
			{
				vk::VkColorComponentFlags staticVal;
				vk::VkColorComponentFlags dynamicVal;
			} colorComponentCases[] =
			{
				{	(CR | CG | CB | CA),	(CR |  0 |  0 |  0)		},
				{	(CR | CG | CB | CA),	( 0 | CG |  0 |  0)		},
				{	(CR | CG | CB | CA),	( 0 |  0 | CB |  0)		},
				{	(CR | CG | CB | CA),	( 0 |  0 |  0 | CA)		},
				{	(CR | CG | CB | CA),	( 0 |  0 |  0 |  0)		},
				{	( 0 |  0 |  0 |  0),	(CR |  0 |  0 |  0)		},
				{	( 0 |  0 |  0 |  0),	( 0 | CG |  0 |  0)		},
				{	( 0 |  0 |  0 |  0),	( 0 |  0 | CB |  0)		},
				{	( 0 |  0 |  0 |  0),	( 0 |  0 |  0 | CA)		},
				{	( 0 |  0 |  0 |  0),	(CR | CG | CB | CA)		},
			};

			for (const auto& colorCompCase : colorComponentCases)
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.clearColorValue						= vk::makeClearValueColor(kTransparentClearColor);
				config.meshParams[0].color					= kOpaqueWhite;
				config.colorWriteMaskConfig.staticValue		= colorCompCase.staticVal;
				config.colorWriteMaskConfig.dynamicValue	= colorCompCase.dynamicVal;
				config.referenceColor.reset					(new SingleColorGenerator(filterColor(kTransparentClearColor, kOpaqueWhite, colorCompCase.dynamicVal)));

				const auto staticCode	= componentCodes(colorCompCase.staticVal);
				const auto dynamicCode	= componentCodes(colorCompCase.dynamicVal);
				const auto testName		= "color_write_mask_" + staticCode + "_to_" + dynamicCode;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, "Dynamically set color write mask to " + dynamicCode, config));
			}
		}

		// Rasterization stream selection.
		if (!kUseMeshShaders)
		{
			const struct
			{
				OptRastStream	shaderStream;	// Stream in the geometry shader.
				OptRastStream	staticVal;		// Static value for the extension struct.
				OptRastStream	dynamicVal;		// Dynamic value for the setter.
				const bool		expectDraw;		// Match between actual stream and active selected value?
				const char*		name;
			} rastStreamCases[] =
			{
				{ tcu::just(1u),	tcu::Nothing,		tcu::just(1u),		true,	"none_to_one"						},
				{ tcu::just(1u),	tcu::just(0u),		tcu::just(1u),		true,	"zero_to_one"						},
				{ tcu::Nothing,		tcu::just(1u),		tcu::just(0u),		true,	"one_to_zero"						},
				{ tcu::just(0u),	tcu::just(1u),		tcu::just(0u),		true,	"one_to_zero_explicit"				},
				{ tcu::just(0u),	tcu::Nothing,		tcu::just(1u),		false,	"none_to_one_mismatch"				},
				{ tcu::just(0u),	tcu::just(0u),		tcu::just(1u),		false,	"zero_to_one_mismatch"				},
				{ tcu::Nothing,		tcu::Nothing,		tcu::just(1u),		false,	"none_to_one_mismatch_implicit"		},
				{ tcu::Nothing,		tcu::just(0u),		tcu::just(1u),		false,	"zero_to_one_mismatch_implicit"		},
			};

			for (const auto& rastStreamCase : rastStreamCases)
			{
				// In TWO_DRAWS_STATIC sequence ordering, the bad dynamic value may be tcu::Nothing, which is equivalent to not
				// calling the state-setting function, but the pipeline will be used to draw. This is illegal. The dynamic value
				// must be set if the used pipeline contains the dynamic state.
				if (kOrdering == SequenceOrdering::TWO_DRAWS_STATIC && !static_cast<bool>(rastStreamCase.staticVal))
					continue;

				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.rasterizationStreamConfig.staticValue	= rastStreamCase.staticVal;
				config.rasterizationStreamConfig.dynamicValue	= rastStreamCase.dynamicVal;
				config.shaderRasterizationStream				= rastStreamCase.shaderStream;
				config.referenceColor.reset						(new SingleColorGenerator(rastStreamCase.expectDraw ? kDefaultTriangleColor : kDefaultClearColor));

				const auto testName = std::string("rasterization_stream_") + rastStreamCase.name;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, "Dynamically switch rasterization streams", config));
			}
		}

		// Provoking vertex mode.
		{
			const struct
			{
				OptBoolean		staticVal;
				OptBoolean		dynamicVal;
				const char*		name;
				const char*		desc;
			} provokingVtxCases[] =
			{
				{ tcu::Nothing,		tcu::just(true),	"provoking_vertex_first_to_last_implicit",	"Dynamically switch provoking vertex mode from none (first) to last"	},
				{ tcu::just(false),	tcu::just(true),	"provoking_vertex_first_to_last_explicit",	"Dynamically switch provoking vertex mode from first to last"			},
				{ tcu::just(true),	tcu::just(false),	"provoking_vertex_last_to_first",			"Dynamically switch provoking vertex mode from last to first"			},
			};

			for (const auto& provokingVtxCase : provokingVtxCases)
			{
				// In TWO_DRAWS_STATIC sequence ordering, the bad dynamic value may be tcu::Nothing, which is equivalent to not
				// calling the state-setting function, but the pipeline will be used to draw. This is illegal. The dynamic value
				// must be set if the used pipeline contains the dynamic state.
				if (kOrdering == SequenceOrdering::TWO_DRAWS_STATIC && !static_cast<bool>(provokingVtxCase.staticVal))
					continue;

				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, getProvokingVertexWithPaddingGenerator(provokingVtxCase.dynamicVal.get()));
				config.provokingVertexConfig.staticValue	= provokingVtxCase.staticVal;
				config.provokingVertexConfig.dynamicValue	= provokingVtxCase.dynamicVal;
				config.oversizedTriangle					= true;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, provokingVtxCase.name, provokingVtxCase.desc, config));
			}
		}

		// Depth clip negative one to one.
		{
			const struct
			{
				OptBoolean		staticVal;
				OptBoolean		dynamicVal;
				const char*		name;
				const char*		desc;
			} negativeOneToOneCases[] =
			{
				{ tcu::Nothing,		tcu::just(true),	"negative_one_to_one_false_to_true_implicit",	"Dynamically switch negative one to one mode from none (false) to true"	},
				{ tcu::just(false),	tcu::just(true),	"negative_one_to_one_false_to_true_explicit",	"Dynamically switch negative one to one mode from false to true"		},
				{ tcu::just(true),	tcu::just(false),	"negative_one_to_one_true_to_false",			"Dynamically switch negative one to one mode from true to false"		},
			};

			for (const auto& negOneToOneCase : negativeOneToOneCases)
			{
				// In TWO_DRAWS_STATIC sequence ordering, the bad dynamic value may be tcu::Nothing, which is equivalent to not
				// calling the state-setting function, but the pipeline will be used to draw. This is illegal. The dynamic value
				// must be set if the used pipeline contains the dynamic state.
				if (kOrdering == SequenceOrdering::TWO_DRAWS_STATIC && !static_cast<bool>(negOneToOneCase.staticVal))
					continue;

				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.negativeOneToOneConfig.staticValue	= negOneToOneCase.staticVal;
				config.negativeOneToOneConfig.dynamicValue	= negOneToOneCase.dynamicVal;

				// Enable depth test and set values so it passes.
				config.depthTestEnableConfig.staticValue	= true;
				config.depthWriteEnableConfig.staticValue	= true;
				config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_LESS;
				config.meshParams[0].depth					= 0.5f;
				config.expectedDepth						= (config.getActiveNegativeOneToOneValue() ? 0.75f : 0.5f);

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, negOneToOneCase.name, negOneToOneCase.desc, config));
			}
		}

		// Depth clip enable.
		{
			const struct
			{
				OptBoolean		staticVal;
				OptBoolean		dynamicVal;
				const char*		name;
				const char*		desc;
			} depthClipEnableCases[] =
			{
				{ tcu::Nothing,		tcu::just(false),	"depth_clip_enable_true_to_false_implicit",	"Dynamically switch negative one to one mode from none (true) to false"	},
				{ tcu::just(true),	tcu::just(false),	"depth_clip_enable_true_to_false_explicit",	"Dynamically switch negative one to one mode from true to false"		},
				{ tcu::just(false),	tcu::just(true),	"depth_clip_enable_true_to_false",			"Dynamically switch negative one to one mode from false to true"		},
			};

			for (const auto& depthClipEnableCase : depthClipEnableCases)
			{
				// In TWO_DRAWS_STATIC sequence ordering, the bad dynamic value may be tcu::Nothing, which is equivalent to not
				// calling the state-setting function, but the pipeline will be used to draw. This is illegal. The dynamic value
				// must be set if the used pipeline contains the dynamic state.
				if (kOrdering == SequenceOrdering::TWO_DRAWS_STATIC && !static_cast<bool>(depthClipEnableCase.staticVal))
					continue;

				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
				config.depthClipEnableConfig.staticValue	= depthClipEnableCase.staticVal;
				config.depthClipEnableConfig.dynamicValue	= depthClipEnableCase.dynamicVal;

				const bool depthClipActive = config.getActiveDepthClipEnable();

				// Enable depth test and set values so it passes.
				config.depthTestEnableConfig.staticValue	= true;
				config.depthWriteEnableConfig.staticValue	= true;
				config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_LESS;
				config.meshParams[0].depth					= -0.5f;
				config.viewportConfig.staticValue			= ViewportVec(1u, vk::makeViewport(0.0f, 0.0f, kWidthF, kHeightF, 0.5f, 1.0f));
				config.expectedDepth						= (depthClipActive ? 1.0f : 0.25f);
				config.referenceColor.reset					(new SingleColorGenerator(depthClipActive ? kDefaultClearColor : kDefaultTriangleColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, depthClipEnableCase.name, depthClipEnableCase.desc, config));
			}
		}

		// Sample locations enablement.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.rasterizationSamplesConfig	= kMultiSampleCount;
			config.offCenterTriangle			= true;
			config.offCenterProportion			= tcu::Vec2(0.90625f, 0.90625f);

			// Push sample locations towards the bottom right corner so they're able to sample the off-center triangle.
			config.sampleLocations				= tcu::Vec2(1.0f, 1.0f);

			config.sampleLocationsEnableConfig.staticValue	= false;
			config.sampleLocationsEnableConfig.dynamicValue	= true;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "sample_locations_enable", "Dynamically enable sample locations", config));

			config.sampleLocationsEnableConfig.swapValues();
			config.referenceColor.reset(new TopLeftBorderGenerator(kDefaultTriangleColor, kDefaultClearColor, kDefaultClearColor, kDefaultClearColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "sample_locations_disable", "Dynamically disable sample locations", config));
		}

		// Coverage to color enable.
		{
			for (int i = 0; i < 2; ++i)
			{
				const bool multisample = (i > 0);

				for (int j = 0; j < 2; ++j)
				{
					const bool		covToColor		= (j > 0);
					const uint32_t	referenceRed	= ((covToColor ? (multisample ? 15u : 1u) : 48u/*matches meshParams[0].color*/));

					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

					config.oversizedTriangle						= true; // This avoids partial coverages in fragments.
					config.rasterizationSamplesConfig				= (multisample ? kMultiSampleCount : kSingleSampleCount);
					config.coverageToColorEnableConfig.staticValue	= !covToColor;
					config.coverageToColorEnableConfig.dynamicValue	= covToColor;
					config.meshParams[0].color						= tcu::Vec4(48.0f, 0.0f, 0.0f, 1.0f); // Distinct value, does not match any coverage mask.
					config.referenceColor.reset						(new SingleColorGenerator(tcu::UVec4(referenceRed, 0u, 0u, 1u)));

					const std::string	finalState	= (covToColor ? "enable" : "disable");
					const auto			testName	= "coverage_to_color_" + finalState + "_" + (multisample ? "multisample" : "single_sample");
					const auto			testDesc	= "Dynamically " + finalState + " coverage to color in " + (multisample ? "multisample" : "single sample") + " images";

					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, testDesc, config));
				}
			}
		}

		// Coverage to color location.
		{
			for (int i = 0; i < 2; ++i)
			{
				const bool multisample = (i > 0);

				for (int j = 0; j < 2; ++j)
				{
					const bool		locationLast	= (j > 0);
					const uint32_t	colorAttCount	= 4u;
					const uint32_t	covToColorLoc	= (locationLast ? colorAttCount - 1u : 0u);
					const uint32_t	referenceRed	= ((locationLast ? (multisample ? 15u : 1u) : 48u/*matches meshParams[0].color*/));

					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

					config.oversizedTriangle							= true; // This avoids partial coverages in fragments.
					config.rasterizationSamplesConfig					= (multisample ? kMultiSampleCount : kSingleSampleCount);
					config.colorAttachmentCount							= colorAttCount;
					config.coverageToColorEnableConfig.staticValue		= true;
					config.coverageToColorLocationConfig.staticValue	= (locationLast ? 0u : colorAttCount - 1u);
					config.coverageToColorLocationConfig.dynamicValue	= covToColorLoc;
					config.meshParams[0].color							= tcu::Vec4(48.0f, 0.0f, 0.0f, 1.0f); // Distinct value, does not match any coverage mask.
					config.referenceColor.reset							(new SingleColorGenerator(tcu::UVec4(referenceRed, 0u, 0u, 1u)));

					const auto	locName		= std::to_string(covToColorLoc);
					const auto	testName	= "coverage_to_color_location_" + locName + "_" + (multisample ? "multisample" : "single_sample");
					const auto	testDesc	= "Dynamically enable coverage to color in location " + locName + " using " + (multisample ? "multisample" : "single sample") + " images";

					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, testDesc, config));
				}
			}
		}

#ifndef CTS_USES_VULKANSC
		// Coverage modulation mode.
		{
			const struct
			{
				vk::VkCoverageModulationModeNV	staticVal;
				vk::VkCoverageModulationModeNV	dynamicVal;
				tcu::Vec4						partialCovFactor; // This will match the expected coverage proportion. See below.
				const char*						name;
			} modulationModeCases[] =
			{
				{ vk::VK_COVERAGE_MODULATION_MODE_NONE_NV,	vk::VK_COVERAGE_MODULATION_MODE_RGB_NV,		tcu::Vec4(0.25f, 0.25f, 0.25f, 1.0f),	"rgb"	},
				{ vk::VK_COVERAGE_MODULATION_MODE_NONE_NV,	vk::VK_COVERAGE_MODULATION_MODE_ALPHA_NV,	tcu::Vec4(1.0f, 1.0f, 1.0f, 0.25f),		"alpha"	},
				{ vk::VK_COVERAGE_MODULATION_MODE_NONE_NV,	vk::VK_COVERAGE_MODULATION_MODE_RGBA_NV,	tcu::Vec4(0.25f, 0.25f, 0.25f, 0.25f),	"rgba"	},
				{ vk::VK_COVERAGE_MODULATION_MODE_RGBA_NV,	vk::VK_COVERAGE_MODULATION_MODE_NONE_NV,	tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f),		"none"	},
			};

			for (const auto& modulationModeCase : modulationModeCases)
			{
				TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

				config.coverageModulation			= true;
				config.rasterizationSamplesConfig	= kMultiSampleCount;
				config.colorSampleCount				= kSingleSampleCount;

				// With VK_SAMPLE_COUNT_4_BIT and the standard sample locations, this pixel offset will:
				// * Leave the corner pixel uncovered.
				// * Cover the top border with sample 3 (1/4 the samples = 0.25).
				// * Cover the left border with sample 1 (1/4 the samples = 0.25).
				config.offCenterProportion	= tcu::Vec2(0.6875f, 0.6875f);
				config.offCenterTriangle	= true;

				config.coverageModulationModeConfig.staticValue		= modulationModeCase.staticVal;
				config.coverageModulationModeConfig.dynamicValue	= modulationModeCase.dynamicVal;

				const auto& partialCoverageColor = kDefaultTriangleColor * modulationModeCase.partialCovFactor;
				config.referenceColor.reset(new TopLeftBorderGenerator(kDefaultTriangleColor, partialCoverageColor, kDefaultClearColor, partialCoverageColor));

				const auto testName = std::string("coverage_modulation_mode_") + modulationModeCase.name;
				const auto testDesc = std::string("Dynamically set coverage modulation mode to ") + modulationModeCase.name;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, testName, testDesc, config));
			}
		}

		// Coverage modulation table enable.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.coverageModulation			= true;
			config.rasterizationSamplesConfig	= kMultiSampleCount;
			config.colorSampleCount				= kSingleSampleCount;

			// With VK_SAMPLE_COUNT_4_BIT and the standard sample locations, this pixel offset will:
			// * Leave the corner pixel uncovered.
			// * Cover the top border with sample 3 (1/4 the samples = 0.25).
			// * Cover the left border with sample 1 (1/4 the samples = 0.25).
			config.offCenterProportion	= tcu::Vec2(0.6875f, 0.6875f);
			config.offCenterTriangle	= true;

			const CovModTableVec table { 0.75f, 1.0f, 1.0f, 1.0f };
			config.coverageModulationModeConfig.staticValue		= vk::VK_COVERAGE_MODULATION_MODE_RGB_NV;
			config.coverageModTableConfig.staticValue			= table;

			config.coverageModTableEnableConfig.staticValue		= false;
			config.coverageModTableEnableConfig.dynamicValue	= true;

			const auto	tableCoverFactor			= tcu::Vec4(0.75f, 0.75f, 0.75f, 1.0f);
			const auto&	tablePartialCoverageColor	= kDefaultTriangleColor * tableCoverFactor;

			config.referenceColor.reset(new TopLeftBorderGenerator(kDefaultTriangleColor, tablePartialCoverageColor, kDefaultClearColor, tablePartialCoverageColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "coverage_modulation_table_enable", "Dynamically enable coverage modulation table", config));

			// Reverse situation, fall back to the default modulation factor.
			config.coverageModTableEnableConfig.swapValues();
			const auto	noTableCoverFactor			= tcu::Vec4(0.25f, 0.25f, 0.25f, 1.0f);
			const auto&	noTablePartialCoverageColor	= kDefaultTriangleColor * noTableCoverFactor;
			config.referenceColor.reset				(new TopLeftBorderGenerator(kDefaultTriangleColor, noTablePartialCoverageColor, kDefaultClearColor, noTablePartialCoverageColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "coverage_modulation_table_disable", "Dynamically disable coverage modulation table", config));
		}

		// Coverage modulation table.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.coverageModulation			= true;
			config.rasterizationSamplesConfig	= kMultiSampleCount;
			config.colorSampleCount				= kSingleSampleCount;

			// With VK_SAMPLE_COUNT_4_BIT and the standard sample locations, this pixel offset will:
			// * Cover the corner pixel with 1 sample (0.25).
			// * Cover the top border with 2 samples (0.5).
			// * Cover the left border with 2 samples (0.5).
			config.offCenterProportion	= tcu::Vec2(0.5f, 0.5f);
			config.offCenterTriangle	= true;

			config.coverageModulationModeConfig.staticValue		= vk::VK_COVERAGE_MODULATION_MODE_RGB_NV;
			config.coverageModTableEnableConfig.staticValue		= true;

			//									corner	border	unused		main
			const CovModTableVec goodTable	{	0.75f,	0.25f,	0.0f,		0.5f	};
			const CovModTableVec badTable	{	0.5f,	0.75f,	1.0f,		0.25f	};

			config.coverageModTableConfig.staticValue	= badTable;
			config.coverageModTableConfig.dynamicValue	= goodTable;

			// VK_COVERAGE_MODULATION_MODE_RGB_NV, factors for RGB according to goodTable, alpha untouched.
			const auto	cornerFactor	= tcu::Vec4(0.75f, 0.75f, 0.75f, 1.0f);
			const auto	borderFactor	= tcu::Vec4(0.25f, 0.25f, 0.25f, 1.0f);
			const auto	mainFactor		= tcu::Vec4(0.5f,  0.5f,  0.5f,  1.0f);

			const auto&	cornerColor		= kDefaultTriangleColor * cornerFactor;
			const auto&	borderColor		= kDefaultTriangleColor * borderFactor;
			const auto&	mainColor		= kDefaultTriangleColor * mainFactor;

			config.referenceColor.reset(new TopLeftBorderGenerator(mainColor, borderColor, cornerColor, borderColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "coverage_modulation_table_change", "Dynamically change coverage modulation table", config));
		}

		// Coverage reduction mode.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.coverageReduction			= true;
			config.rasterizationSamplesConfig	= kMultiSampleCount;
			config.colorSampleCount				= kSingleSampleCount;

			// With VK_SAMPLE_COUNT_4_BIT and the standard sample locations, this pixel offset will:
			// * Leave the corner pixel uncovered.
			// * Cover the top border with sample 3 (1/4 the samples = 0.25).
			// * Cover the left border with sample 1 (1/4 the samples = 0.25).
			config.offCenterProportion	= tcu::Vec2(0.6875f, 0.6875f);
			config.offCenterTriangle	= true;

			config.coverageReductionModeConfig.staticValue	= vk::VK_COVERAGE_REDUCTION_MODE_MERGE_NV;
			config.coverageReductionModeConfig.dynamicValue	= vk::VK_COVERAGE_REDUCTION_MODE_TRUNCATE_NV;

			config.referenceColor.reset(new TopLeftBorderGenerator(kDefaultTriangleColor, kDefaultClearColor, kDefaultClearColor, kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "coverage_reduction_truncate", "Dynamically set coverage reduction truncate mode", config));

			// In merge mode, the only pixel without coverage should be the corner. However, the spec is a bit ambiguous in this
			// case:
			//
			//    VK_COVERAGE_REDUCTION_MODE_MERGE_NV specifies that each color sample will be associated with an
			//    implementation-dependent subset of samples in the pixel coverage. If any of those associated samples are covered,
			//    the color sample is covered.
			//
			// We cannot be 100% sure the single color sample will be associated with the whole set of 4 rasterization samples, but
			// the test appears to pass in existing HW.
			config.coverageReductionModeConfig.swapValues();
			config.referenceColor.reset(new TopLeftBorderGenerator(kDefaultTriangleColor, kDefaultTriangleColor, kDefaultClearColor, kDefaultTriangleColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "coverage_reduction_merge", "Dynamically set coverage reduction merge mode", config));
		}

		// Viewport swizzle.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			config.viewportSwizzle				= true;
			config.oversizedTriangle			= true;
			config.cullModeConfig.staticValue	= vk::VK_CULL_MODE_BACK_BIT;

			const vk::VkViewportSwizzleNV idSwizzle
			{
				vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_X_NV,
				vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Y_NV,
				vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Z_NV,
				vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_W_NV,
			};

			const vk::VkViewportSwizzleNV yxSwizzle // Switches Y and X coordinates, makes the oversized triangle clockwise.
			{
				vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Y_NV, // <--
				vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_X_NV, // <--
				vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Z_NV,
				vk::VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_W_NV,
			};

			config.viewportSwizzleConfig.staticValue	= ViewportSwzVec(1u, idSwizzle);
			config.viewportSwizzleConfig.dynamicValue	= ViewportSwzVec(1u, yxSwizzle);
			config.frontFaceConfig.staticValue			= vk::VK_FRONT_FACE_CLOCKWISE;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "viewport_swizzle_yx", "Dynamically set a viewport swizzle with X and Y switched around", config));

			config.viewportSwizzleConfig.swapValues();
			config.frontFaceConfig.staticValue			= vk::VK_FRONT_FACE_COUNTER_CLOCKWISE;
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "viewport_swizzle_xy", "Dynamically set the viewport identity swizzle", config));
		}

		// Shading rate image enable.
		// VK_NV_shading_rate_image is disabled when using shader objects due to interaction with VK_KHR_fragment_shading_rate
		if (!vk::isConstructionTypeShaderObject(pipelineConstructionType))
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			for (int i = 0; i < 2; ++i)
			{
				const bool			sriEnable	= (i > 0);
				const std::string	enableStr	= (sriEnable ? "enable" : "disable");

				config.shadingRateImage = true;
				config.shadingRateImageEnableConfig.staticValue = !sriEnable;
				config.shadingRateImageEnableConfig.dynamicValue = sriEnable;
				config.referenceColor.reset(new SingleColorGenerator(sriEnable ? kDefaultClearColor : kDefaultTriangleColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "shading_rate_image_" + enableStr, "Dynamically " + enableStr + " a shading rate image", config));
			}
		}

		// Viewport W Scaling enable.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			for (int i = 0; i < 2; ++i)
			{
				const bool			wScalingEnable	= (i > 0);
				const std::string	enableStr		= (wScalingEnable ? "enable" : "disable");

				config.colorVerificator = verifyTopLeftCornerExactly;
				config.viewportWScaling = true;
				config.viewportWScalingEnableConfig.staticValue = !wScalingEnable;
				config.viewportWScalingEnableConfig.dynamicValue = wScalingEnable;
				config.referenceColor.reset(new SingleColorGenerator(wScalingEnable ? kDefaultClearColor : kDefaultTriangleColor));

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "viewport_w_scaling_" + enableStr, "Dynamically " + enableStr + " viewport W scaling", config));
			}
		}

		// Representative fragment test state.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			for (int i = 0; i < 2; ++i)
			{
				const bool			reprFragTestEnable	= (i > 0);
				const std::string	enableStr			= (reprFragTestEnable ? "enable" : "disable");

				config.depthTestEnableConfig.staticValue	= true;
				config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_LESS;
				config.colorWriteMaskConfig.staticValue		= 0u; // Disable color writes.
				config.oversizedTriangle					= true;
				config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));

				config.representativeFragmentTest				= true;
				config.reprFragTestEnableConfig.staticValue		= !reprFragTestEnable;
				config.reprFragTestEnableConfig.dynamicValue	= reprFragTestEnable;

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "repr_frag_test_" + enableStr, "Dynamically " + enableStr + " representative frag test", config));
			}
		}
#endif // CTS_USES_VULKANSC

		// Conservative rasterization mode.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.offCenterTriangle = true;

			// Single-sampling at the pixel center should not cover this, but overestimation should result in coverage.
			config.offCenterProportion							= tcu::Vec2(0.75f, 0.75f);
			config.extraPrimitiveOverEstConfig.staticValue		= 0.0f;
			config.conservativeRasterModeConfig.staticValue		= vk::VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
			config.conservativeRasterModeConfig.dynamicValue	= vk::VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "conservative_rasterization_mode_overestimate", "Dynamically set conservative rasterization mode to overestimation", config));

			config.conservativeRasterModeConfig.swapValues();
			config.referenceColor.reset(new TopLeftBorderGenerator(kDefaultTriangleColor, kDefaultClearColor, kDefaultClearColor, kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "conservative_rasterization_mode_disabled", "Dynamically set conservative rasterization mode to disabled", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.offCenterTriangle = true;

			// Single-sampling at the pixel center should cover this, but underestimation should result in lack of coverage.
			config.offCenterProportion							= tcu::Vec2(0.25f, 0.25f);
			config.extraPrimitiveOverEstConfig.staticValue		= 0.0f;
			config.conservativeRasterModeConfig.staticValue		= vk::VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
			config.conservativeRasterModeConfig.dynamicValue	= vk::VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT;
			config.referenceColor.reset							(new TopLeftBorderGenerator(kDefaultTriangleColor, kDefaultClearColor, kDefaultClearColor, kDefaultClearColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "conservative_rasterization_mode_underestimate", "Dynamically set conservative rasterization mode to underestimation", config));
		}

		// Extra primitive overestimation size.
		// Notes as of 2022-08-12 and gpuinfo.org:
		//    * primitiveOverestimationSize is typically 0.0, 0.001953125 or 0.00195313 (i.e. very small).
		//    * maxExtraPrimitiveOverestimationSize is typically 0.0 or 0.75 (no other values).
		//    * extraPrimitiveOverestimationSizeGranularity is typically 0.0 or 0.25 (no other values).
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.offCenterTriangle = true;

			// Move the triangle by more than one pixel, then use an extra overestimation of 0.75 to cover the border pixels too.
			config.offCenterProportion						= tcu::Vec2(1.125f, 1.125f);
			config.maxPrimitiveOverestimationSize			= 0.5f; // Otherwise the base overestimation size will be enough. This should never trigger.
			config.conservativeRasterModeConfig.staticValue	= vk::VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
			config.extraPrimitiveOverEstConfig.staticValue	= 0.0f;
			config.extraPrimitiveOverEstConfig.dynamicValue	= 0.75f; // Large enough to reach the center of the border pixel.

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "extra_overestimation_size_large", "Dynamically set the extra overestimation size to a large value", config));

			config.extraPrimitiveOverEstConfig.swapValues();
			config.referenceColor.reset(new TopLeftBorderGenerator(kDefaultTriangleColor, kDefaultClearColor, kDefaultClearColor, kDefaultClearColor));

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "extra_overestimation_size_none", "Dynamically set the extra overestimation size to zero", config));
		}

		// Depth bias enable with static or dynamic depth bias parameters.
		{
			const DepthBiasParams kAlternativeDepthBiasParams = { 2e7f, 0.25f };

			for (int dynamicBiasIter = 0; dynamicBiasIter < 2; ++dynamicBiasIter)
			{
				const bool useDynamicBias = (dynamicBiasIter > 0);

				{
					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

					// Enable depth test and write 1.0f
					config.depthTestEnableConfig.staticValue = true;
					config.depthWriteEnableConfig.staticValue = true;
					config.depthCompareOpConfig.staticValue = vk::VK_COMPARE_OP_ALWAYS;
					// Clear depth buffer to 0.25f
					config.clearDepthValue = 0.25f;
					// Write depth to 0.5f
					config.meshParams[0].depth = 0.5f;

					// Enable dynamic depth bias and expect the depth value to be clamped to 0.75f based on depthBiasConstantFactor and depthBiasClamp
					if (useDynamicBias)
					{
						config.depthBiasConfig.staticValue	= kNoDepthBiasParams;
						config.depthBiasConfig.dynamicValue	= kAlternativeDepthBiasParams;
					}
					else
					{
						config.depthBiasConfig.staticValue	= kAlternativeDepthBiasParams;
					}

					config.depthBiasEnableConfig.staticValue = false;
					config.depthBiasEnableConfig.dynamicValue = tcu::just(true);
					config.expectedDepth = 0.75f;

					std::string caseName = "depth_bias_enable";
					std::string caseDesc = "Dynamically enable the depth bias";

					if (useDynamicBias)
					{
						caseName += "_dynamic_bias_params";
						caseDesc += " and set the bias params dynamically";
					}

					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, caseName, caseDesc, config));
				}
				{
					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

					// Enable depth test and write 1.0f
					config.depthTestEnableConfig.staticValue = true;
					config.depthWriteEnableConfig.staticValue = true;
					config.depthCompareOpConfig.staticValue = vk::VK_COMPARE_OP_ALWAYS;
					// Clear depth buffer to 0.25f
					config.clearDepthValue = 0.25f;
					// Write depth to 0.5f
					config.meshParams[0].depth = 0.5f;

					// Disable dynamic depth bias and expect the depth value to remain at 0.5f based on written value
					if (useDynamicBias)
					{
						config.depthBiasConfig.staticValue	= kNoDepthBiasParams;
						config.depthBiasConfig.dynamicValue	= kAlternativeDepthBiasParams;
					}
					else
					{
						config.depthBiasConfig.staticValue	= kAlternativeDepthBiasParams;
					}

					config.depthBiasEnableConfig.staticValue = true;
					config.depthBiasEnableConfig.dynamicValue = tcu::just(false);
					config.expectedDepth = 0.5f;

					std::string caseName = "depth_bias_disable";
					std::string caseDesc = "Dynamically disable the depth bias";

					if (useDynamicBias)
					{
						caseName += "_dynamic_bias_params";
						caseDesc += " and set the bias params dynamically";
					}

					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, caseName, caseDesc, config));
				}
			}
		}

#ifndef CTS_USES_VULKANSC
		// Depth bias representation info.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			// Enable depth test and writes.
			config.depthTestEnableConfig.staticValue	= true;
			config.depthWriteEnableConfig.staticValue	= true;
			config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_ALWAYS;
			config.clearDepthValue						= 0.0f;
			config.meshParams[0].depth					= 0.125f;
			const double targetBias						= 0.5f;
			config.expectedDepth						= 0.625f; // mesh depth + target bias

			vk::VkDepthBiasRepresentationInfoEXT depthBiasReprInfo	= vk::initVulkanStructure();
			depthBiasReprInfo.depthBiasRepresentation				= vk::VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT;
			depthBiasReprInfo.depthBiasExact						= VK_TRUE;
			config.depthBiasReprInfo								= depthBiasReprInfo;
			config.neededDepthChannelClass							= tcu::TEXTURECHANNELCLASS_FLOATING_POINT;

			// We will choose a format with floating point representation, but force a UNORM exact depth bias representation.
			// With this, the value of R should be 2^(-N), with N being the number of mantissa bits plus one (2^(-24) for D32_SFLOAT).
			// To reach our target bias, the constant factor must be calculated based on it and the value of R.
			//
			// If the VkDepthBiasRepresentationInfoEXT is not taken into account, the value of R would be 2^(E-N), such that:
			// E is the maximum exponent in the range of Z values that the primitive uses (-3 for our mesh depth of 0.125).
			// N is the number of mantissa bits in the floating point format (23 in our case)
			// R would be wrongly calculated as 2^(-26) (1/4th of the intended value).
			const double minR			= 1.0 / static_cast<double>(1u << 24u);
			const double constantFactor	= targetBias / minR;

			const DepthBiasParams kPositiveBias			{ static_cast<float>(constantFactor), 0.0f };
			config.depthBiasEnableConfig.staticValue	= true;
			config.depthBiasConfig.staticValue			= kNoDepthBiasParams;
			config.depthBiasConfig.dynamicValue			= kPositiveBias;
			config.extraDepthThreshold					= static_cast<float>(minR);

			const char* caseName = "depth_bias_repr_info";
			const char* caseDesc = "Dynamically set the depth bias representation information";

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, caseName, caseDesc, config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

			// Enable depth test and writes.
			config.depthTestEnableConfig.staticValue	= true;
			config.depthWriteEnableConfig.staticValue	= true;
			config.depthCompareOpConfig.staticValue		= vk::VK_COMPARE_OP_ALWAYS;
			config.clearDepthValue						= 0.25f;	// Clear depth buffer to 0.25.
			config.meshParams[0].depth					= 0.5f;		// Set mesh depth to 0.5 as a base.

			// Enable dynamic depth bias to add a 0.25 bias to the mesh depth (using float representation), expecting the final
			// depth to be 0.75.
			vk::VkDepthBiasRepresentationInfoEXT depthBiasReprInfo	= vk::initVulkanStructure();
			depthBiasReprInfo.depthBiasRepresentation				= vk::VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT;
			depthBiasReprInfo.depthBiasExact						= VK_FALSE;
			config.depthBiasReprInfo								= depthBiasReprInfo;

			const DepthBiasParams kPositiveBias			{ 0.25f, 0.0f };
			config.depthBiasEnableConfig.staticValue	= true;
			config.depthBiasConfig.staticValue			= kNoDepthBiasParams;
			config.depthBiasConfig.dynamicValue			= kPositiveBias;
			config.expectedDepth						= 0.75f;

			const char* caseName = "depth_bias_repr_info_float";
			const char* caseDesc = "Dynamically set the depth bias representation information to float representation";

			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, caseName, caseDesc, config));
		}
#endif // CTS_USES_VULKANSC

		// Depth compare op.
		{
			TestConfig baseConfig(pipelineConstructionType, kOrdering, kUseMeshShaders);
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
				config.referenceColor.reset					(new SingleColorGenerator(kDefaultClearColor));
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
				config.referenceColor.reset					(new SingleColorGenerator(kAlternativeColor));
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
				config.referenceColor.reset					(new SingleColorGenerator(kAlternativeColor));
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
				config.referenceColor.reset					(new SingleColorGenerator(kAlternativeColor));
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

				config.referenceColor.reset					(new SingleColorGenerator(kAlternativeColor));
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
			TestConfig baseConfig(pipelineConstructionType, kOrdering, kUseMeshShaders);
			baseConfig.depthBoundsConfig.staticValue			= std::make_pair(0.25f, 0.75f);
			baseConfig.meshParams[0].depth						= 0.0f;

			{
				TestConfig config = baseConfig;
				config.depthBoundsTestEnableConfig.staticValue	= false;
				config.depthBoundsTestEnableConfig.dynamicValue	= tcu::just(true);
				config.referenceColor.reset						(new SingleColorGenerator(kDefaultClearColor));
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_bounds_test_enable", "Dynamically enable the depth bounds test", config));
			}
			{
				TestConfig config = baseConfig;
				config.depthBoundsTestEnableConfig.staticValue	= true;
				config.depthBoundsTestEnableConfig.dynamicValue	= tcu::just(false);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "depth_bounds_test_disable", "Dynamically disable the depth bounds test", config));
			}
		}

		// Stencil test enable.
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.stencilTestEnableConfig.staticValue				= false;
			config.stencilTestEnableConfig.dynamicValue				= tcu::just(true);
			config.stencilOpConfig.staticValue.front().compareOp	= vk::VK_COMPARE_OP_NEVER;
			config.referenceColor.reset								(new SingleColorGenerator(kDefaultClearColor));
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "stencil_test_enable", "Dynamically enable the stencil test", config));
		}
		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.stencilTestEnableConfig.staticValue				= true;
			config.stencilTestEnableConfig.dynamicValue				= tcu::just(false);
			config.stencilOpConfig.staticValue.front().compareOp	= vk::VK_COMPARE_OP_NEVER;
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
				{ vk::VK_STENCIL_FACE_FRONT_AND_BACK,		"face_both_single"	},
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

			for (const auto& face : kFaces)
			for (const auto& compare : kCompare)
			for (const auto& op : kStencilOps)
			{
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
							for (int extraPipelineIter = 0; extraPipelineIter < 2; ++extraPipelineIter)
							{
								const bool useExtraPipeline = (extraPipelineIter > 0);		// Bind and draw with another pipeline using the same dynamic states.

								if (useExtraPipeline && (kOrdering == SequenceOrdering::TWO_DRAWS_DYNAMIC || kOrdering == SequenceOrdering::TWO_DRAWS_STATIC))
									continue;

								if (useExtraPipeline && kUseMeshShaders)
									continue;

								const bool depthFail		= (subCaseIdx > 0);				// depthFail would be the second variant.
								const bool globalPass		= (wouldPass && !depthFail);	// Global result of the stencil+depth test.

								// Start tuning test parameters.
								TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);

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
								config.referenceColor.reset	(new SingleColorGenerator(globalPass ? kDefaultTriangleColor : kDefaultClearColor));
								config.expectedDepth		= config.clearDepthValue; // No depth writing by default.
								config.expectedStencil		= stencilResult(op.stencilOp, clearVal, refValU8, kMinVal, kMaxVal);

								config.useExtraDynPipeline	= useExtraPipeline;

								const std::string testName = std::string("stencil_state")
									+ ((useExtraPipeline) ? "_extra_pipeline" : "")
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
		}

		// Vertex input.
		if (!kUseMeshShaders)
		{
			for (const auto& bindUnusedCase : kBindUnusedCases)
			{
				if (bindUnusedCase.bindUnusedMeshShadingPipeline && kOrdering != SequenceOrdering::CMD_BUFFER_START)
					continue;

				// TWO_DRAWS_STATIC would be invalid because it violates VUID-vkCmdBindVertexBuffers2EXT-pStrides-03363 due to the
				// dynamic stride being less than the extent of the binding for the second attribute.
				if (kOrdering != SequenceOrdering::TWO_DRAWS_STATIC)
				{
					{
						const auto	staticGen	= getVertexWithPaddingGenerator();
						const auto	dynamicGen	= getVertexWithExtraAttributesGenerator();
						const auto	goodStrides	= dynamicGen->getVertexDataStrides();
						StrideVec	badStrides;

						badStrides.reserve(goodStrides.size());
						for (const auto& stride : goodStrides)
							badStrides.push_back(stride / 2u);

						TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, staticGen, dynamicGen);
						config.strideConfig.staticValue			= badStrides;
						config.strideConfig.dynamicValue		= goodStrides;
						config.bindUnusedMeshShadingPipeline	= bindUnusedCase.bindUnusedMeshShadingPipeline;
						orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "vertex_input" + bindUnusedCase.nameSuffix, "Dynamically set vertex input" + bindUnusedCase.descSuffix, config));
					}
					{
						const auto	staticGen	= getVertexWithInstanceDataGenerator();
						const auto	goodStrides	= staticGen->getVertexDataStrides();
						StrideVec	badStrides;

						DE_ASSERT(goodStrides.size() == 2u);
						badStrides.reserve(2u);
						badStrides.push_back(goodStrides.at(0u));
						badStrides.push_back(goodStrides.at(1u) / 2u); // Halve instance rate stride.

						TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, staticGen);
						config.strideConfig.staticValue			= badStrides;
						config.strideConfig.dynamicValue		= goodStrides;
						config.bindUnusedMeshShadingPipeline	= bindUnusedCase.bindUnusedMeshShadingPipeline;
						config.instanceCount					= 2u;
						orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "instance_rate_stride" + bindUnusedCase.nameSuffix, "Dynamically set instance rate stride" + bindUnusedCase.descSuffix, config));
					}
				}

				{
					// Variant without mixing in the stride config.
					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, getVertexWithPaddingGenerator(), getVertexWithExtraAttributesGenerator());
					config.bindUnusedMeshShadingPipeline = bindUnusedCase.bindUnusedMeshShadingPipeline;
					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "vertex_input_no_dyn_stride" + bindUnusedCase.nameSuffix, "Dynamically set vertex input without using dynamic strides" + bindUnusedCase.descSuffix, config));
				}

				{
					// Variant using multiple bindings.
					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, getVertexWithExtraAttributesGenerator(), getVertexWithMultipleBindingsGenerator());
					config.bindUnusedMeshShadingPipeline = bindUnusedCase.bindUnusedMeshShadingPipeline;
					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "vertex_input_multiple_bindings" + bindUnusedCase.nameSuffix, "Dynamically set vertex input with multiple bindings" + bindUnusedCase.descSuffix, config));
				}

				{
					// Variant checking dynamic vertex inputs with 16-bit floats.
					TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders, getVertexWithPaddingGenerator(), getVertexWithPadding16Generator());
					config.bindUnusedMeshShadingPipeline = bindUnusedCase.bindUnusedMeshShadingPipeline;
					orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "vertex_input_float16" + bindUnusedCase.nameSuffix, "Dynamically set vertex input with float16 inputs" + bindUnusedCase.descSuffix, config));
				}
			}
		}

		// Null state pointers.
		{
			TestConfig baseConfig(pipelineConstructionType, kOrdering, kUseMeshShaders);
			baseConfig.favorStaticNullPointers = true;

			if (!kUseMeshShaders)
			{
				TestConfig config(pipelineConstructionType, kOrdering, false, getVertexWithPaddingGenerator(), getVertexWithExtraAttributesGenerator());
				config.favorStaticNullPointers = true;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "null_vertex_input_state", "Use null pVertexInputState", config));
			}

			if (!kUseMeshShaders)
			{
				TestConfig config(baseConfig);
				config.topologyConfig.staticValue			= vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				config.topologyConfig.dynamicValue			= vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
				config.extraLineRestarts					= true;
				config.primRestartEnableConfig.staticValue	= false;
				config.primRestartEnableConfig.dynamicValue	= tcu::just(true);
				config.referenceColor.reset					(new CenterStripGenerator(kDefaultTriangleColor, kDefaultClearColor));
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "null_input_assembly_state", "Use null pVertexInputState", config));
			}

			if (!kUseMeshShaders)
			{
				TestConfig config(baseConfig);
				config.topologyConfig.staticValue = vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
				config.patchControlPointsConfig.staticValue = 1;
				config.patchControlPointsConfig.dynamicValue = 3;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "null_tessellation_state", "Use null pTessellationState", config));
			}

			{
				TestConfig config(baseConfig);

				config.viewportConfig.staticValue	= ViewportVec{
					vk::makeViewport(kHalfWidthF, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),	// Right.
					vk::makeViewport(0.0f, 0.0f, kHalfWidthF, kHeightF, 0.0f, 1.0f),		// Left.
				};

				config.scissorConfig.staticValue	= ScissorVec{
					vk::makeRect2D(kHalfWidthI, 0, kHalfWidthU, kFramebufferHeight),
					vk::makeRect2D(kHalfWidthU, kFramebufferHeight),
				};

				config.scissorConfig.dynamicValue	= ScissorVec{config.scissorConfig.staticValue.back(), config.scissorConfig.staticValue.front()};
				config.viewportConfig.dynamicValue	= ViewportVec{config.viewportConfig.staticValue.back(), config.viewportConfig.staticValue.front()};

				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "null_viewport_state", "Use null pViewportState", config));
			}

			{
				TestConfig config(baseConfig);
				config.depthClampEnableConfig.staticValue		= true;
				config.depthClampEnableConfig.dynamicValue		= false;
				config.rastDiscardEnableConfig.staticValue		= true;
				config.rastDiscardEnableConfig.dynamicValue		= false;
				config.polygonModeConfig.staticValue			= vk::VK_POLYGON_MODE_POINT;
				config.polygonModeConfig.dynamicValue			= vk::VK_POLYGON_MODE_FILL;
				config.cullModeConfig.staticValue				= vk::VK_CULL_MODE_FRONT_AND_BACK;
				config.cullModeConfig.dynamicValue				= vk::VK_CULL_MODE_NONE;
				config.frontFaceConfig.staticValue				= vk::VK_FRONT_FACE_CLOCKWISE;
				config.frontFaceConfig.dynamicValue				= vk::VK_FRONT_FACE_COUNTER_CLOCKWISE;
				config.depthBiasEnableConfig.staticValue		= true;
				config.depthBiasEnableConfig.dynamicValue		= false;
				config.depthBiasConfig.staticValue				= DepthBiasParams{1.0f, 1.0f};
				config.depthBiasConfig.dynamicValue				= kNoDepthBiasParams;
				config.lineWidthConfig.staticValue				= 0.0f;
				config.lineWidthConfig.dynamicValue				= 1.0f;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "null_rasterization_state", "Use null pRasterizationState", config));
			}

			{
				TestConfig config(baseConfig);
				config.rasterizationSamplesConfig.staticValue	= kMultiSampleCount;
				config.rasterizationSamplesConfig.dynamicValue	= kSingleSampleCount;
				config.sampleMaskConfig.staticValue				= SampleMaskVec(1u, 0u);
				config.sampleMaskConfig.dynamicValue			= SampleMaskVec(1u, 0xFFu);
				config.alphaToCoverageConfig.staticValue		= true;
				config.alphaToCoverageConfig.dynamicValue		= false;
				config.alphaToOneConfig.staticValue				= true;
				config.alphaToOneConfig.dynamicValue			= false;
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "null_multisample_state", "Use null pMultisampleState", config));
			}

			{
				TestConfig config(baseConfig);
				config.depthTestEnableConfig.staticValue		= true;
				config.depthTestEnableConfig.dynamicValue		= false;
				config.depthWriteEnableConfig.staticValue		= true;
				config.depthWriteEnableConfig.dynamicValue		= false;
				config.depthCompareOpConfig.staticValue			= vk::VK_COMPARE_OP_NEVER;
				config.depthCompareOpConfig.dynamicValue		= vk::VK_COMPARE_OP_ALWAYS;
				config.depthBoundsTestEnableConfig.staticValue	= true;
				config.depthBoundsTestEnableConfig.dynamicValue	= false;
				config.stencilTestEnableConfig.staticValue		= true;
				config.stencilTestEnableConfig.dynamicValue		= false;
				config.stencilOpConfig.staticValue				= StencilOpVec(1u, StencilOpParams{vk::VK_STENCIL_FACE_FRONT_AND_BACK, vk::VK_STENCIL_OP_INVERT, vk::VK_STENCIL_OP_INVERT, vk::VK_STENCIL_OP_INVERT, vk::VK_COMPARE_OP_NEVER});
				config.stencilOpConfig.dynamicValue				= StencilOpVec(1u, StencilOpParams{vk::VK_STENCIL_FACE_FRONT_AND_BACK, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_ALWAYS});
				config.depthBoundsConfig.staticValue			= std::make_pair(1.0f, 1.0f);
				config.depthBoundsConfig.dynamicValue			= std::make_pair(0.0f, 0.0f);
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "null_depth_stencil_state", "Use null pDepthStencilState", config));
			}

			{
				TestConfig config(baseConfig);
				config.logicOpEnableConfig.staticValue			= true;
				config.logicOpEnableConfig.dynamicValue			= false;
				config.logicOpConfig.staticValue				= vk::VK_LOGIC_OP_CLEAR;
				config.logicOpConfig.dynamicValue				= vk::VK_LOGIC_OP_COPY;
				config.colorBlendEnableConfig.staticValue		= true;
				config.colorBlendEnableConfig.dynamicValue		= false;
				config.colorBlendEquationConfig.staticValue		= ColorBlendEq();
				config.colorBlendEquationConfig.dynamicValue	= ColorBlendEq(vk::VK_BLEND_FACTOR_ONE, vk::VK_BLEND_FACTOR_ONE, vk::VK_BLEND_OP_ADD, vk::VK_BLEND_FACTOR_ONE, vk::VK_BLEND_FACTOR_ONE, vk::VK_BLEND_OP_ADD);
				config.colorWriteMaskConfig.staticValue			= 0u;
				config.colorWriteMaskConfig.dynamicValue		= (CR | CG | CB | CA);
				config.blendConstantsConfig.staticValue			= BlendConstArray{1.0f, 1.0f, 1.0f, 1.0f};
				config.blendConstantsConfig.dynamicValue		= BlendConstArray{0.0f, 0.0f, 0.0f, 0.0f};
				orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "null_color_blend_state", "Use null pColorBlendState", config));
			}
		}

		{
			TestConfig config(pipelineConstructionType, kOrdering, kUseMeshShaders);
			config.sampleShadingEnable						= true;
			config.minSampleShading							= 1.0f;
			config.forceAtomicCounters						= true;
			config.oversizedTriangle						= true;
			config.rasterizationSamplesConfig.staticValue	= kSingleSampleCount;
			config.rasterizationSamplesConfig.dynamicValue	= kMultiSampleCount;
			orderingGroup->addChild(new ExtendedDynamicStateTest(testCtx, "sample_shading_sample_count", "Test number of frag shader invocations with sample shading enabled and dynamic sample counts", config));
		}

		tcu::TestCaseGroup* group = (kUseMeshShaders ? meshShaderGroup.get() : extendedDynamicStateGroup.get());
		group->addChild(orderingGroup.release());
	}

	extendedDynamicStateGroup->addChild(meshShaderGroup.release());
	extendedDynamicStateGroup->addChild(createExtendedDynamicStateMiscTests(testCtx, pipelineConstructionType));
	return extendedDynamicStateGroup.release();
}

} // pipeline
} // vkt
