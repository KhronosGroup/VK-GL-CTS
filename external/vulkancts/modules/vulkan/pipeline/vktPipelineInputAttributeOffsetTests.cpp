/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Valve Corporation.
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
 * \brief Input Attribute Offset Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineInputAttributeOffsetTests.hpp"

#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuImageCompare.hpp"

#include <string>
#include <sstream>
#include <memory>
#include <vector>
#include <array>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

// StrideCase determines the way we're going to store vertex data in the vertex buffer.
//
// With packed vertices:
//
//     Vertex buffer
//    +-----+---------------------------------------------------------------------+
//    |     +---------------------------------------------------------------------+
//    |     |    +--------+--------+                                              |
//    |     |    |Attr    |Attr    |                                              |
//    |     |    |        |        | ...                                          |
//    |     |    +--------+--------+                                              |
//    |     +---------------------------------------------------------------------+
//    +-----+---------------------------------------------------------------------+
//
//    -------
//    Vertex binding offset
//
//          ------
//          Attribute offset
//
// With padded vertices:
//
//     Vertex buffer
//    +-----+---------------------------------------------------------------------+
//    |     +---------------------------------------------------------------------+
//    |     |    +--------+--------+--------+                                     |
//    |     |    |Attr    |Pad     |Attr    |                                     |
//    |     |    |        |        |        |                                     |
//    |     |    +--------+--------+--------+                                     |
//    |     +---------------------------------------------------------------------+
//    +-----+---------------------------------------------------------------------+
//
//    -------
//    Vertex binding offset
//
//          ------
//          Attribute offset
//
// With overlapping vertices, the case is similar to packed. However, the data type in the _shader_ will be a Vec4, stored in the
// buffer as Vec2's. In the shader, only the XY coordinates are properly used (ZW coordinates would belong to the next vertex).
//
enum class StrideCase { PACKED = 0, PADDED = 1, OVERLAPPING = 2, };

uint32_t getTypeSize (glu::DataType dataType)
{
	switch (dataType)
	{
	case glu::TYPE_FLOAT_VEC2:	return static_cast<uint32_t>(sizeof(tcu::Vec2));
	case glu::TYPE_FLOAT_VEC4:	return static_cast<uint32_t>(sizeof(tcu::Vec4));
	default:					break;
	}

	DE_ASSERT(false);
	return 0u;
}

struct TestParams
{
	const PipelineConstructionType	constructionType;
	const glu::DataType				dataType;			// vec2 or vec4.
	const uint32_t					bindingOffset;		// When binding vertex buffer.
	const StrideCase				strideCase;			// Pack all data or include some padding.
	const bool						useMemoryOffset;	// Apply an offset when binding memory to the buffer.
	const bool						dynamic;			// Use dynamic state or not.

	uint32_t attributeSize (void) const
	{
		return getTypeSize(dataType);
	}

	bool isOverlapping (void) const
	{
		return (strideCase == StrideCase::OVERLAPPING);
	}

	VkFormat attributeFormat (void) const
	{
		switch (dataType)
		{
		case glu::TYPE_FLOAT_VEC2:	return (isOverlapping() ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R32G32_SFLOAT);
		case glu::TYPE_FLOAT_VEC4:	return VK_FORMAT_R32G32B32A32_SFLOAT;
		default:					break;
		}

		DE_ASSERT(false);
		return VK_FORMAT_UNDEFINED;
	}

	// Given the vertex buffer binding offset, calculate the appropriate attribute offset to make them aligned.
	uint32_t attributeOffset (void) const
	{
		const auto attribSize = attributeSize();
		DE_ASSERT(bindingOffset < attribSize);
		return ((attribSize - bindingOffset) % attribSize);
	}

	// Calculates proper padding size between elements according to strideCase.
	uint32_t vertexDataPadding (void) const
	{
		if (strideCase == StrideCase::PADDED)
			return attributeSize();
		return 0u;
	}

	// Calculates proper binding stride according to strideCase.
	uint32_t bindingStride (void) const
	{
		return attributeSize() + vertexDataPadding();
	}
};

using VertexVec	= std::vector<tcu::Vec2>;
using BytesVec	= std::vector<uint8_t>;

BytesVec buildVertexBufferData (const VertexVec& origVertices, const TestParams& params)
{
	DE_ASSERT(!origVertices.empty());

	VertexVec vertices (origVertices);

	if (params.isOverlapping())
	{
		// Each vertex will be read as a vec4, so we need one extra element at the end to make the last vec4 read valid and avoid going beyond the end of the buffer.
		DE_ASSERT(params.dataType == glu::TYPE_FLOAT_VEC2);
		vertices.push_back(tcu::Vec2(0.0f, 0.0f));
	}

	const auto			vertexCount		= de::sizeU32(vertices);
	const auto			dataSize		= params.bindingOffset + params.attributeOffset() + vertexCount * params.bindingStride();
	const tcu::Vec2		zw				(0.0f, 1.0f);
	const auto			zwSize			= static_cast<uint32_t>(sizeof(zw));
	const auto			srcVertexSize	= static_cast<uint32_t>(sizeof(VertexVec::value_type));
	const bool			needsZW			(params.attributeSize() > srcVertexSize); // vec4 needs each vec2 with zw appended.
	const auto			paddingSize		= params.vertexDataPadding();
	BytesVec			data			(dataSize, uint8_t{0});

	uint8_t* nextVertexPtr = data.data() + params.bindingOffset + params.attributeOffset();

	for (uint32_t vertexIdx = 0u; vertexIdx < vertexCount; ++vertexIdx)
	{
		// Copy vertex.
		deMemcpy(nextVertexPtr, &vertices.at(vertexIdx), srcVertexSize);
		nextVertexPtr += srcVertexSize;

		// Copy extra ZW values if needed.
		if (needsZW)
		{
			deMemcpy(nextVertexPtr, &zw, zwSize);
			nextVertexPtr += zwSize;
		}

		// Skip the padding bytes.
		nextVertexPtr += paddingSize;
	}

	return data;
}

tcu::Vec4 getDefaultColor (void)
{
	return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
}

tcu::Vec4 getClearColor (void)
{
	return tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

tcu::IVec3 getDefaultExtent (void)
{
	return tcu::IVec3(4, 4, 1); // Multiple pixels and vertices, not too big.
}

// Generate one triangle per pixel.
VertexVec generateVertices (uint32_t width, uint32_t height)
{
	VertexVec vertices;
	vertices.reserve(width * height * 3u); // 3 points (1 triangle) per pixel.

	// Normalized pixel width and height.
	const auto pixelWidth	= 2.0f / static_cast<float>(width);
	const auto pixelHeight	= 2.0f / static_cast<float>(height);
	const auto widthMargin	= pixelWidth / 4.0f;
	const auto heightMargin	= pixelHeight / 4.0f;

	for (uint32_t y = 0; y < height; ++y)
		for (uint32_t x = 0; x < width; ++x)
		{
			// Normalized pixel center.
			const auto pixelCenterX = ((static_cast<float>(x) + 0.5f) / static_cast<float>(width)) * 2.0f - 1.0f;
			const auto pixelCenterY = ((static_cast<float>(y) + 0.5f) / static_cast<float>(height)) * 2.0f - 1.0f;

			vertices.push_back(tcu::Vec2(pixelCenterX, pixelCenterY - heightMargin));				// Top
			vertices.push_back(tcu::Vec2(pixelCenterX - widthMargin, pixelCenterY + heightMargin));	// Bottom left.
			vertices.push_back(tcu::Vec2(pixelCenterX + widthMargin, pixelCenterY + heightMargin));	// Bottom right.
		}

	return vertices;
}

class InputAttributeOffsetCase : public vkt::TestCase
{
public:
					InputAttributeOffsetCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}
	virtual			~InputAttributeOffsetCase	(void) {}
	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override;
	void			checkSupport				(Context& context) const override;

protected:
	const TestParams m_params;
};

class InputAttributeOffsetInstance : public vkt::TestInstance
{
public:
						InputAttributeOffsetInstance	(Context& context, const TestParams& params)
							: vkt::TestInstance (context)
							, m_params(params)
							{}
	virtual				~InputAttributeOffsetInstance	(void) {}
	tcu::TestStatus		iterate							(void) override;

protected:
	const TestParams m_params;
};

TestInstance* InputAttributeOffsetCase::createInstance (Context& context) const
{
	return new InputAttributeOffsetInstance(context, m_params);
}

void InputAttributeOffsetCase::checkSupport (Context &context) const
{
	const auto&			vki				= context.getInstanceInterface();
	const auto			physicalDevice	= context.getPhysicalDevice();

	checkPipelineConstructionRequirements(vki, physicalDevice, m_params.constructionType);

#ifndef CTS_USES_VULKANSC
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset"))
	{
		const auto&	properties		= context.getPortabilitySubsetProperties();
		const auto&	minStrideAlign	= properties.minVertexInputBindingStrideAlignment;
		const auto	bindingStride	= m_params.bindingStride();

		if (bindingStride < minStrideAlign || bindingStride % minStrideAlign != 0u)
			TCU_THROW(NotSupportedError, "Binding stride " + std::to_string(bindingStride) + " not a multiple of " + std::to_string(minStrideAlign));
	}
#endif // CTS_USES_VULKANSC

	if (m_params.dynamic)
		context.requireDeviceFunctionality("VK_EXT_vertex_input_dynamic_state");
}

void InputAttributeOffsetCase::initPrograms (vk::SourceCollections& programCollection) const
{
	{
		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "void main (void) { outColor = vec4" << getDefaultColor() << "; }\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}

	{
		const auto extraComponents	= ((m_params.dataType == glu::TYPE_FLOAT_VEC4)
									? ""
									: ((m_params.isOverlapping())
									// Simulate that we use the .zw components in order to force the implementation to read them.
									? ", floor(abs(inPos.z) / 1000.0), (floor(abs(inPos.w) / 2500.0) + 1.0)" // Should result in 0.0, 1.0.
									:", 0.0, 1.0"));
		const auto componentSelect	= (m_params.isOverlapping() ? ".xy" : "");

		std::ostringstream vert;
		vert
			<< "#version 460\n"
			<< "layout (location=0) in " << glu::getDataTypeName(m_params.isOverlapping() ? glu::TYPE_FLOAT_VEC4 : m_params.dataType) << " inPos;\n"
			<< "void main (void) { gl_Position = vec4(inPos" << componentSelect << extraComponents << "); }\n"
			;
		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	}
}

tcu::TestStatus InputAttributeOffsetInstance::iterate (void)
{
	const auto				ctx					= m_context.getContextCommonData();
	const auto				fbExtent			= getDefaultExtent();
	const auto				vkExtent			= makeExtent3D(fbExtent);
	const auto				vertices			= generateVertices(vkExtent.width, vkExtent.height);
	const auto				vertexBufferData	= buildVertexBufferData(vertices, m_params);
	const auto				colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const auto				colorUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	// Vertex buffer.
	const auto	vertexBufferSize	= static_cast<VkDeviceSize>(de::dataSize(vertexBufferData));
	const auto	vertexBufferInfo	= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const auto	vertexBuffer		= makeBuffer(ctx.vkd, ctx.device, vertexBufferInfo);
	const auto	vertexBufferOffset	= static_cast<VkDeviceSize>(m_params.bindingOffset);

	// Allocate and bind buffer memory.
	// If useMemoryOffset is true, we'll allocate extra memory that satisfies alignment requirements for the buffer and the attributes.
	auto		vertexBufferReqs	= getBufferMemoryRequirements(ctx.vkd, ctx.device, *vertexBuffer);
	const auto	memoryOffset		= (m_params.useMemoryOffset ? (de::lcm(vertexBufferReqs.alignment, static_cast<VkDeviceSize>(m_params.attributeSize()))) : 0ull);
	vertexBufferReqs.size			+= memoryOffset;
	auto		vertexBufferAlloc	= ctx.allocator.allocate(vertexBufferReqs, MemoryRequirement::HostVisible);
	VK_CHECK(ctx.vkd.bindBufferMemory(ctx.device, *vertexBuffer, vertexBufferAlloc->getMemory(), memoryOffset));

	// Copy vertices to vertex buffer.
	const auto dstPtr = reinterpret_cast<char*>(vertexBufferAlloc->getHostPtr()) + memoryOffset; // Need to add offset manually here.
	deMemcpy(dstPtr, de::dataOrNull(vertexBufferData), de::dataSize(vertexBufferData));
	flushAlloc(ctx.vkd, ctx.device, *vertexBufferAlloc);

	// Color buffer.
	ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D);

	// Render pass and framebuffer.
	auto renderPass	= RenderPassWrapper(m_params.constructionType, ctx.vkd, ctx.device, colorFormat);
	renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

	// Shaders.
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	vertModule	= ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto	fragModule	= ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag"));

	std::vector<VkDynamicState> dynamicStates;
	if (m_params.dynamic)
		dynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

	const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineDynamicStateCreateFlags	flags;
		de::sizeU32(dynamicStates),								//	uint32_t							dynamicStateCount;
		de::dataOrNull(dynamicStates),							//	const VkDynamicState*				pDynamicStates;
	};

	// Vertex input values according to test parameters.
	const auto vertexInputBinding	= makeVertexInputBindingDescription(0u, m_params.bindingStride(), VK_VERTEX_INPUT_RATE_VERTEX);
	const auto vertexInputAttribute	= makeVertexInputAttributeDescription(0u, 0u, m_params.attributeFormat(), m_params.attributeOffset());

	using VertexInputStatePtr = std::unique_ptr<VkPipelineVertexInputStateCreateInfo>;
	VertexInputStatePtr pipelineVertexInputState;
	if (!m_params.dynamic)
	{
		pipelineVertexInputState.reset(new VkPipelineVertexInputStateCreateInfo);
		*pipelineVertexInputState									= initVulkanStructure();
		pipelineVertexInputState->vertexBindingDescriptionCount		= 1u;
		pipelineVertexInputState->pVertexBindingDescriptions		= &vertexInputBinding;
		pipelineVertexInputState->vertexAttributeDescriptionCount	= 1u;
		pipelineVertexInputState->pVertexAttributeDescriptions		= &vertexInputAttribute;
	}

	const std::vector<VkViewport>	viewports	(1u, makeViewport(vkExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(vkExtent));

	// Pipeline.
	const PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device);
	GraphicsPipelineWrapper pipelineWrapper (ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(), m_params.constructionType);
	pipelineWrapper
		.setMonolithicPipelineLayout(pipelineLayout)
		.setDefaultDepthStencilState()
		.setDefaultColorBlendState()
		.setDefaultRasterizationState()
		.setDefaultMultisampleState()
		.setDefaultVertexInputState(false)
		.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.setDynamicState(&dynamicStateCreateInfo)
		.setupVertexInputState(pipelineVertexInputState.get())
		.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule)
		.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
		.setupFragmentOutputState(*renderPass, 0u)
		.buildPipeline();

	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	// Draw and copy image to verification buffer.
	beginCommandBuffer(ctx.vkd, cmdBuffer);
	{
		renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), getClearColor());
		pipelineWrapper.bind(cmdBuffer);
		ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
		if (m_params.dynamic)
		{
			VkVertexInputBindingDescription2EXT dynamicBinding = initVulkanStructure();
			dynamicBinding.binding		= vertexInputBinding.binding;
			dynamicBinding.inputRate	= vertexInputBinding.inputRate;
			dynamicBinding.stride		= vertexInputBinding.stride;
			dynamicBinding.divisor		= 1u;

			VkVertexInputAttributeDescription2EXT dynamicAttribute = initVulkanStructure();
			dynamicAttribute.location	= vertexInputAttribute.location;
			dynamicAttribute.binding	= vertexInputAttribute.binding;
			dynamicAttribute.format		= vertexInputAttribute.format;
			dynamicAttribute.offset		= vertexInputAttribute.offset;

			ctx.vkd.cmdSetVertexInputEXT(cmdBuffer, 1u, &dynamicBinding, 1u, &dynamicAttribute);
		}
		ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
		renderPass.end(ctx.vkd, cmdBuffer);
	}
	{
		copyImageToBuffer(
			ctx.vkd,
			cmdBuffer,
			colorBuffer.getImage(),
			colorBuffer.getBuffer(),
			tcu::IVec2(fbExtent.x(), fbExtent.y()),
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			1u,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	endCommandBuffer(ctx.vkd, cmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
	invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());

	// Check color buffer.
	auto&								log				= m_context.getTestContext().getLog();
	const auto							tcuFormat		= mapVkFormat(colorFormat);
	const tcu::ConstPixelBufferAccess	resultAccess	(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());
	const tcu::Vec4						threshold		(0.0f, 0.0f, 0.0f, 0.0f);

	if (!tcu::floatThresholdCompare(log, "Result", "", getDefaultColor(), resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected color buffer contents -- check log for details");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup* createInputAttributeOffsetTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "input_attribute_offset", "Test input attribute offsets"));

	for (const auto dataType : { glu::TYPE_FLOAT_VEC2, glu::TYPE_FLOAT_VEC4 })
	{
		const auto typeSize = getTypeSize(dataType);
		GroupPtr dataTypeGrp (new tcu::TestCaseGroup(testCtx, glu::getDataTypeName(dataType), ""));

		for (uint32_t offset = 0u; offset < typeSize; ++offset)
		{
			const auto offsetGrpName = "offset_" + std::to_string(offset);
			GroupPtr offsetGrp (new tcu::TestCaseGroup(testCtx, offsetGrpName.c_str(), ""));

			for (const auto strideCase : { StrideCase::PACKED, StrideCase::PADDED, StrideCase::OVERLAPPING })
			{
				if (strideCase == StrideCase::OVERLAPPING && dataType != glu::TYPE_FLOAT_VEC2)
					continue;

				const std::array<const char*, 3> strideNames { "packed", "padded", "overlapping" };
				GroupPtr strideGrp (new tcu::TestCaseGroup(testCtx, strideNames.at(static_cast<int>(strideCase)), ""));

				for (const auto useMemoryOffset : { false, true })
				{
					const std::array<const char*, 2> memoryOffsetGrpNames { "no_memory_offset", "with_memory_offset" };
					GroupPtr memoryOffsetGrp (new tcu::TestCaseGroup(testCtx, memoryOffsetGrpNames.at(static_cast<int>(useMemoryOffset)), ""));

					for (const auto& dynamic : { false, true })
					{
						const TestParams params
						{
							pipelineConstructionType,
							dataType,
							offset,
							strideCase,
							useMemoryOffset,
							dynamic,
						};
						const auto testName = (dynamic ? "dynamic" : "static");
						memoryOffsetGrp->addChild(new InputAttributeOffsetCase(testCtx, testName, "", params));
					}

					strideGrp->addChild(memoryOffsetGrp.release());
				}

				offsetGrp->addChild(strideGrp.release());
			}

			dataTypeGrp->addChild(offsetGrp.release());
		}

		mainGroup->addChild(dataTypeGrp.release());
	}

	return mainGroup.release();
}

} // pipeline
} // vkt
