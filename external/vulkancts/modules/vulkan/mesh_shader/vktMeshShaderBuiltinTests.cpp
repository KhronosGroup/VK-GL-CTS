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
 * \brief Mesh Shader Builtin Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderBuiltinTests.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"

#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTexture.hpp"
#include "tcuTestLog.hpp"

#include <vector>
#include <algorithm>
#include <sstream>
#include <map>
#include <utility>
#include <sstream>

namespace vkt
{
namespace MeshShader
{

namespace
{

// Wraps a tcu::IVec2 with a custom operator< that uses the X and Y components in component order so it can be used as a map key.
// Can be converted to and from a tcu::IVec2 automatically.
class CoordKey
{
public:
	CoordKey (const tcu::IVec2& coords)
		: m_coords(coords)
	{}

	operator tcu::IVec2 () const
	{
		return m_coords;
	}

	bool operator< (const CoordKey& other) const
	{
		const auto& a = this->m_coords;
		const auto& b = other.m_coords;

		for (int i = 0; i < tcu::IVec2::SIZE; ++i)
		{
			if (a[i] < b[i])
				return true;
			if (a[i] > b[i])
				return false;
		}

		return false;
	}

private:
	const tcu::IVec2 m_coords;
};

using namespace vk;

using GroupPtr				= de::MovePtr<tcu::TestCaseGroup>;
using DrawCommandVec		= std::vector<VkDrawMeshTasksIndirectCommandNV>;
using ImageWithMemoryPtr	= de::MovePtr<ImageWithMemory>;
using BufferWithMemoryPtr	= de::MovePtr<BufferWithMemory>;
using ViewportVec			= std::vector<VkViewport>;
using ColorVec				= std::vector<tcu::Vec4>;
using PixelMap				= std::map<CoordKey, tcu::Vec4>; // Coordinates to color.

VkExtent2D getDefaultExtent ()
{
	return makeExtent2D(8u, 8u);
}

VkExtent2D getLinearExtent ()
{
	return makeExtent2D(8u, 1u);
}

struct JobSize
{
	uint32_t numTasks;
	uint32_t localSize;
};

JobSize getLargeJobSize ()
{
	return JobSize{8u, 8u};
}

// Single draw command with the given number of tasks, 1 by default.
DrawCommandVec getDefaultDrawCommands (uint32_t taskCount = 1u)
{
	return DrawCommandVec(1u, makeDrawMeshTasksIndirectCommandNV(taskCount, 0u));
}

// Basic fragment shader that draws fragments in blue.
std::string getBasicFragShader ()
{
	return
		"#version 460\n"
		"#extension GL_NV_mesh_shader : enable\n"
		"\n"
		"layout (location=0) out vec4 outColor;\n"
		"\n"
		"void main ()\n"
		"{\n"
		"    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
		"}\n"
		;
}

struct IterationParams
{
	VkExtent2D					colorExtent;
	uint32_t					numLayers;
	DrawCommandVec				drawArgs;
	bool						indirect;
	ViewportVec					viewports;	// If empty, a single default viewport is used.
	tcu::Maybe<FragmentSize>	fragmentSize;
};

class MeshShaderBuiltinInstance : public vkt::TestInstance
{
public:
						MeshShaderBuiltinInstance	(Context& context, const IterationParams& params)
							: vkt::TestInstance	(context)
							, m_params			(params)
							{}
	virtual				~MeshShaderBuiltinInstance	(void) {}

	tcu::TestStatus		iterate						() override;
	virtual void		verifyResults				(const tcu::ConstPixelBufferAccess& result) = 0;

protected:
	IterationParams		m_params;
};

tcu::TestStatus MeshShaderBuiltinInstance::iterate ()
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();
	const auto&		binaries	= m_context.getBinaryCollection();

	const auto		useTask		= binaries.contains("task");
	const auto		useFrag		= binaries.contains("frag");
	const auto		extent		= makeExtent3D(m_params.colorExtent.width, m_params.colorExtent.height, 1u);
	const auto		iExtent3D	= tcu::IVec3(static_cast<int>(extent.width), static_cast<int>(extent.height), static_cast<int>(m_params.numLayers));
	const auto		format		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto		tcuFormat	= mapVkFormat(format);
	const auto		colorUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto		viewType	= ((m_params.numLayers > 1u) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
	const auto		colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_params.numLayers);
	const auto		colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, m_params.numLayers);
	const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 1.0f);

	ImageWithMemoryPtr	colorBuffer;
	Move<VkImageView>	colorBufferView;
	{
		const VkImageCreateInfo colorBufferInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			format,									//	VkFormat				format;
			extent,									//	VkExtent3D				extent;
			1u,										//	uint32_t				mipLevels;
			m_params.numLayers,						//	uint32_t				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			colorUsage,								//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	uint32_t				queueFamilyIndexCount;
			nullptr,								//	const uint32_t*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};
		colorBuffer = ImageWithMemoryPtr(new ImageWithMemory(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any));
		colorBufferView = makeImageView(vkd, device, colorBuffer->get(), viewType, format, colorSRR);
	}

	// Empty descriptor set layout.
	DescriptorSetLayoutBuilder layoutBuilder;
	const auto setLayout = layoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Render pass and framebuffer.
	const auto renderPass	= makeRenderPass(vkd, device, format);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), colorBufferView.get(), extent.width, extent.height, m_params.numLayers);

	// Pipeline.
	Move<VkShaderModule> taskModule;
	Move<VkShaderModule> meshModule;
	Move<VkShaderModule> fragModule;

	if (useTask)
		taskModule = createShaderModule(vkd, device, binaries.get("task"));
	if (useFrag)
		fragModule = createShaderModule(vkd, device, binaries.get("frag"));
	meshModule = createShaderModule(vkd, device, binaries.get("mesh"));

	std::vector<VkViewport>	viewports;
	std::vector<VkRect2D>	scissors;
	if (m_params.viewports.empty())
	{
		// Default ones.
		viewports.push_back(makeViewport(extent));
		scissors.push_back(makeRect2D(extent));
	}
	else
	{
		// The desired viewports and the same number of default scissors.
		viewports.reserve(m_params.viewports.size());
		std::copy(begin(m_params.viewports), end(m_params.viewports), std::back_inserter(viewports));
		scissors.resize(viewports.size(), makeRect2D(extent));
	}

	using ShadingRateInfoPtr = de::MovePtr<VkPipelineFragmentShadingRateStateCreateInfoKHR>;
	ShadingRateInfoPtr pNext;
	if (static_cast<bool>(m_params.fragmentSize))
	{
		pNext = ShadingRateInfoPtr(new VkPipelineFragmentShadingRateStateCreateInfoKHR);
		*pNext = initVulkanStructure();

		pNext->fragmentSize		= getShadingRateSize(m_params.fragmentSize.get());
		pNext->combinerOps[0]	= VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR;
		pNext->combinerOps[1]	= VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
	}

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskModule.get(), meshModule.get(), fragModule.get(),
		renderPass.get(), viewports, scissors, 0u, nullptr, nullptr, nullptr, nullptr, nullptr, 0u, pNext.get());

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Indirect buffer if needed.
	BufferWithMemoryPtr indirectBuffer;

	DE_ASSERT(!m_params.drawArgs.empty());
	if (m_params.indirect)
	{
		// Indirect draws.
		const auto indirectBufferSize	= static_cast<VkDeviceSize>(de::dataSize(m_params.drawArgs));
		const auto indirectBufferUsage	= (VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		const auto indirectBufferInfo	= makeBufferCreateInfo(indirectBufferSize, indirectBufferUsage);
		indirectBuffer					= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, indirectBufferInfo, MemoryRequirement::HostVisible));
		auto& indirectBufferAlloc		= indirectBuffer->getAllocation();
		void* indirectBufferData		= indirectBufferAlloc.getHostPtr();

		deMemcpy(indirectBufferData, m_params.drawArgs.data(), static_cast<size_t>(indirectBufferSize));
		flushAlloc(vkd, device, indirectBufferAlloc);
	}

	// Submit commands.
	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());

	if (!m_params.indirect)
	{
		for (const auto& command : m_params.drawArgs)
			vkd.cmdDrawMeshTasksNV(cmdBuffer, command.taskCount, command.firstTask);
	}
	else
	{
		const auto numDraws	= static_cast<uint32_t>(m_params.drawArgs.size());
		const auto stride	= static_cast<uint32_t>(sizeof(decltype(m_params.drawArgs)::value_type));
		vkd.cmdDrawMeshTasksIndirectNV(cmdBuffer, indirectBuffer->get(), 0ull, numDraws, stride);
	}

	endRenderPass(vkd, cmdBuffer);

	// Output buffer to extract the color buffer contents.
	BufferWithMemoryPtr	outBuffer;
	void*				outBufferData	= nullptr;
	{
		const auto	layerSize			= static_cast<VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * extent.width * extent.height);
		const auto	outBufferSize		= layerSize * m_params.numLayers;
		const auto	outBufferUsage		= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const auto	outBufferInfo		= makeBufferCreateInfo(outBufferSize, outBufferUsage);

		outBuffer						= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, outBufferInfo, MemoryRequirement::HostVisible));
		outBufferData					= outBuffer->getAllocation().getHostPtr();
	}

	// Transition image layout.
	const auto preTransferBarrier = makeImageMemoryBarrier(
		(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT), VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorBuffer->get(), colorSRR);

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preTransferBarrier);

	// Copy image to output buffer.
	const std::vector<VkBufferImageCopy> regions (1u, makeBufferImageCopy(extent, colorSRL));
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outBuffer->get(), static_cast<uint32_t>(regions.size()), de::dataOrNull(regions));

	// Transfer to host barrier.
	const auto postTransferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &postTransferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Invalidate alloc and verify result.
	{
		auto& outBufferAlloc = outBuffer->getAllocation();
		invalidateAlloc(vkd, device, outBufferAlloc);

		tcu::ConstPixelBufferAccess	result (tcuFormat, iExtent3D, outBufferData);
		verifyResults(result);
	}

	return tcu::TestStatus::pass("Pass");
}

// Abstract case that implements the generic checkSupport method.
class MeshShaderBuiltinCase : public vkt::TestCase
{
public:
					MeshShaderBuiltinCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, bool taskNeeded)
						: vkt::TestCase	(testCtx, name, description)
						, m_taskNeeded	(taskNeeded)
						{}
	virtual			~MeshShaderBuiltinCase	(void) {}

	void			checkSupport			(Context& context) const override;

protected:
	const bool		m_taskNeeded;
};

void MeshShaderBuiltinCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportNV(context, m_taskNeeded, true);
}

// Instance that verifies color layers.
class FullScreenColorInstance : public MeshShaderBuiltinInstance
{
public:
				FullScreenColorInstance		(Context& context, const IterationParams& params, const ColorVec& expectedColors)
					: MeshShaderBuiltinInstance (context, params)
					, m_expectedColors			(expectedColors)
					{}
	virtual		~FullScreenColorInstance	(void) {}

	void		verifyResults				(const tcu::ConstPixelBufferAccess& result) override;

protected:
	const ColorVec m_expectedColors;
};

void FullScreenColorInstance::verifyResults (const tcu::ConstPixelBufferAccess& result)
{
	auto&		log		= m_context.getTestContext().getLog();
	bool		fail	= false;
	const auto	width	= result.getWidth();
	const auto	height	= result.getHeight();
	const auto	depth	= result.getDepth();

	for (int z = 0; z < depth; ++z)
	{
		const auto& expected = m_expectedColors.at(z);

		for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			const auto resultColor = result.getPixel(x, y, z);
			if (resultColor != expected)
			{
				std::ostringstream msg;
				msg << "Pixel (" << x << ", " << y << ", " << z << ") failed: expected " << expected << " and found " << resultColor;
				log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
				fail = true;
			}
		}
	}

	if (fail)
	{
		log << tcu::TestLog::Image("Result", "", result);
		TCU_FAIL("Check log for details");
	}
}

// Instance that verifies single-layer framebuffers divided into 4 quadrants.
class QuadrantsInstance : public MeshShaderBuiltinInstance
{
public:
				QuadrantsInstance	(Context& context, const IterationParams& params,
									 const tcu::Vec4 topLeft,
									 const tcu::Vec4 topRight,
									 const tcu::Vec4 bottomLeft,
									 const tcu::Vec4 bottomRight)
					: MeshShaderBuiltinInstance (context, params)
					, m_topLeft					(topLeft)
					, m_topRight				(topRight)
					, m_bottomLeft				(bottomLeft)
					, m_bottomRight				(bottomRight)
					{}
	virtual		~QuadrantsInstance	(void) {}

	void		verifyResults		(const tcu::ConstPixelBufferAccess& result) override;

protected:
	const tcu::Vec4 m_topLeft;
	const tcu::Vec4 m_topRight;
	const tcu::Vec4 m_bottomLeft;
	const tcu::Vec4 m_bottomRight;
};

void QuadrantsInstance::verifyResults (const tcu::ConstPixelBufferAccess& result)
{
	const auto width	= result.getWidth();
	const auto height	= result.getHeight();
	const auto depth	= result.getDepth();

	DE_ASSERT(depth == 1);
	DE_ASSERT(width > 0 && width % 2 == 0);
	DE_ASSERT(height > 0 && height % 2 == 0);
	DE_UNREF(depth); // For release builds.

	const auto	halfWidth	= width / 2;
	const auto	halfHeight	= height / 2;
	tcu::Vec4	expected;

	for (int y = 0; y < height; ++y)
	for (int x = 0; x < width; ++x)
	{
		// Choose the right quadrant
		if (y < halfHeight)
			expected = ((x < halfWidth) ? m_topLeft : m_topRight);
		else
			expected = ((x < halfWidth) ? m_bottomLeft : m_bottomRight);

		const auto resultColor = result.getPixel(x, y);
		if (resultColor != expected)
		{
			std::ostringstream msg;
			msg << "Pixel (" << x << ", " << y  << ") failed: expected " << expected << " and found " << resultColor;
			TCU_FAIL(msg.str());
		}
	}
}

// Instance that verifies single-layer framebuffers with specific pixels set to some color.
struct PixelVerifierParams
{
	const tcu::Vec4		background;
	const PixelMap		pixelMap;
};

class PixelsInstance : public MeshShaderBuiltinInstance
{
public:
				PixelsInstance	(Context& context, const IterationParams& params, const PixelVerifierParams& pixelParams)
					: MeshShaderBuiltinInstance	(context, params)
					, m_pixelParams				(pixelParams)
					{}
	virtual		~PixelsInstance	(void) {}

	void		verifyResults	(const tcu::ConstPixelBufferAccess& result) override;

protected:
	const PixelVerifierParams m_pixelParams;
};

void PixelsInstance::verifyResults (const tcu::ConstPixelBufferAccess& result)
{
	const auto width	= result.getWidth();
	const auto height	= result.getHeight();
	const auto depth	= result.getDepth();

	DE_ASSERT(depth == 1);
	DE_UNREF(depth); // For release builds.

	for (int y = 0; y < height; ++y)
	for (int x = 0; x < width; ++x)
	{
		const tcu::IVec2	coords		(x, y);
		const auto			iter		= m_pixelParams.pixelMap.find(coords);
		const auto			expected	= ((iter == m_pixelParams.pixelMap.end()) ? m_pixelParams.background : iter->second);
		const auto			resultColor	= result.getPixel(x, y);

		if (resultColor != expected)
		{
			std::ostringstream msg;
			msg << "Pixel (" << x << ", " << y << ") failed: expected " << expected << " and found " << resultColor;
			TCU_FAIL(msg.str());
		}
	}
}

// Primitive ID cases.
class PrimitiveIdCase : public MeshShaderBuiltinCase
{
public:
					PrimitiveIdCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, bool glslFrag)
						: MeshShaderBuiltinCase (testCtx, name, description, false/*taskNeeded*/)
						, m_glslFrag			(glslFrag)
						{}
	virtual			~PrimitiveIdCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	void			checkSupport		(Context& context) const override;
	TestInstance*	createInstance		(Context& context) const override;

protected:
	// Fragment shader in GLSL means glslang will use the Geometry capability due to gl_PrimitiveID.
	const bool		m_glslFrag;
};

void PrimitiveIdCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Mesh shader.
	{
		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=3, max_primitives=1) out;\n"
			<< "\n"
			<< "perprimitiveNV out gl_MeshPerPrimitiveNV {\n"
			<< "   int gl_PrimitiveID;\n"
			<< "} gl_MeshPrimitivesNV[];\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 1u;\n"
			<< "\n"
			<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
			<< "    gl_PrimitiveIndicesNV[1] = 1;\n"
			<< "    gl_PrimitiveIndicesNV[2] = 2;\n"
			<< "\n"
			<< "    gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[1].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[2].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
			<< "\n"
			// Sets an arbitrary primitive id.
			<< "    gl_MeshPrimitivesNV[0].gl_PrimitiveID = 1629198956;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Frag shader.
	if (m_glslFrag)
	{
		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			// Checks the primitive id matches.
			<< "    outColor = ((gl_PrimitiveID == 1629198956) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0));\n"
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
	else
	{
		// This is the same shader as above, but OpCapability Geometry has been replaced by OpCapability MeshShadingNV in order to
		// access gl_PrimitiveID. This also needs the SPV_NV_mesh_shader extension.
		std::ostringstream frag;
		frag
			<< "; Version: 1.0\n"
			<< "; Generator: Khronos Glslang Reference Front End; 10\n"
			<< "; Bound: 24\n"
			<< "; Schema: 0\n"
			<< "      OpCapability Shader\n"

			// Manual change in these lines.
			//<< "      OpCapability Geometry\n"
			<< "      OpCapability MeshShadingNV\n"
			<< "      OpExtension \"SPV_NV_mesh_shader\"\n"

			<< " %1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "      OpMemoryModel Logical GLSL450\n"
			<< "      OpEntryPoint Fragment %4 \"main\" %9 %12\n"
			<< "      OpExecutionMode %4 OriginUpperLeft\n"
			<< "      OpDecorate %9 Location 0\n"
			<< "      OpDecorate %12 Flat\n"
			<< "      OpDecorate %12 BuiltIn PrimitiveId\n"
			<< " %2 = OpTypeVoid\n"
			<< " %3 = OpTypeFunction %2\n"
			<< " %6 = OpTypeFloat 32\n"
			<< " %7 = OpTypeVector %6 4\n"
			<< " %8 = OpTypePointer Output %7\n"
			<< " %9 = OpVariable %8 Output\n"
			<< "%10 = OpTypeInt 32 1\n"
			<< "%11 = OpTypePointer Input %10\n"
			<< "%12 = OpVariable %11 Input\n"
			<< "%14 = OpConstant %10 1629198956\n"
			<< "%15 = OpTypeBool\n"
			<< "%17 = OpConstant %6 0\n"
			<< "%18 = OpConstant %6 1\n"
			<< "%19 = OpConstantComposite %7 %17 %17 %18 %18\n"
			<< "%20 = OpConstantComposite %7 %17 %17 %17 %18\n"
			<< "%21 = OpTypeVector %15 4\n"
			<< " %4 = OpFunction %2 None %3\n"
			<< " %5 = OpLabel\n"
			<< "%13 = OpLoad %10 %12\n"
			<< "%16 = OpIEqual %15 %13 %14\n"
			<< "%22 = OpCompositeConstruct %21 %16 %16 %16 %16\n"
			<< "%23 = OpSelect %7 %22 %19 %20\n"
			<< "      OpStore %9 %23\n"
			<< "      OpReturn\n"
			<< "      OpFunctionEnd\n"
			;
		programCollection.spirvAsmSources.add("frag") << frag.str();
	}
}

void PrimitiveIdCase::checkSupport (Context& context) const
{
	MeshShaderBuiltinCase::checkSupport(context);

	// Fragment shader in GLSL means glslang will use the Geometry capability due to gl_PrimitiveID.
	if (m_glslFrag)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

TestInstance* PrimitiveIdCase::createInstance (Context& context) const
{
	const ColorVec			expectedColors	(1u, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
	const IterationParams	iterationParams	=
	{
		getDefaultExtent(),			//	VkExtent2D					colorExtent;
		1u,							//	uint32_t					numLayers;
		getDefaultDrawCommands(),	//	DrawCommandVec				drawArgs;
		false,						//	bool						indirect;
		{},							//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,				//	tcu::Maybe<FragmentSize>	fragmentSize;
	};
	return new FullScreenColorInstance(context, iterationParams, expectedColors);
}

// Layer builtin case.
class LayerCase : public MeshShaderBuiltinCase
{
public:
					LayerCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, bool shareVertices)
						: MeshShaderBuiltinCase	(testCtx, name, description, false/*taskNeeded*/)
						, m_shareVertices		(shareVertices)
						{}
	virtual			~LayerCase	(void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	void			checkSupport	(Context& context) const override;
	TestInstance*	createInstance	(Context& context) const override;

	static constexpr uint32_t kNumLayers = 4u;

protected:
	const bool m_shareVertices;
};

void LayerCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto localSize		= (m_shareVertices ? kNumLayers : 1u);
	const auto numPrimitives	= (m_shareVertices ? kNumLayers : 1u);
	const auto layerNumber		= (m_shareVertices ? "gl_LocalInvocationIndex" : "gl_WorkGroupID.x");

	// One layer per local invocation or work group (shared vertices or not, respectively).
	{
		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << localSize << ") in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=3, max_primitives=" << numPrimitives << ") out;\n"
			<< "\n"
			<< "perprimitiveNV out gl_MeshPerPrimitiveNV {\n"
			<< "   int gl_Layer;\n"
			<< "} gl_MeshPrimitivesNV[];\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = " << numPrimitives << ";\n"
			<< "\n"
			<< "    if (gl_LocalInvocationIndex == 0u)\n"
			<< "    {\n"
			<< "        gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesNV[1].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesNV[2].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
			<< "    }\n"
			<< "\n"
			<< "    const uint baseIndex = gl_LocalInvocationIndex * 3u;\n"
			<< "    gl_PrimitiveIndicesNV[baseIndex + 0] = 0;\n"
			<< "    gl_PrimitiveIndicesNV[baseIndex + 1] = 1;\n"
			<< "    gl_PrimitiveIndicesNV[baseIndex + 2] = 2;\n"
			<< "\n"
			<< "    gl_MeshPrimitivesNV[gl_LocalInvocationIndex].gl_Layer = int(" << layerNumber << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Fragment shader chooses one color per layer.
	{
		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "\n"
			<< "vec4 colors[" << kNumLayers << "] = vec4[](\n"
			<< "    vec4(0.0, 0.0, 1.0, 1.0),\n"
			<< "    vec4(1.0, 0.0, 1.0, 1.0),\n"
			<< "    vec4(0.0, 1.0, 1.0, 1.0),\n"
			<< "    vec4(1.0, 1.0, 0.0, 1.0)\n"
			<< ");\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    outColor = colors[gl_Layer];\n"
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
}

void LayerCase::checkSupport (Context& context) const
{
	MeshShaderBuiltinCase::checkSupport(context);

	if (!context.contextSupports(vk::ApiVersion(0u, 1u, 2u, 0u)))
		context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");
	else
	{
		const auto& features = context.getDeviceVulkan12Features();
		if (!features.shaderOutputLayer)
			TCU_THROW(NotSupportedError, "shaderOutputLayer feature not supported");
	}
}

TestInstance* LayerCase::createInstance (Context& context) const
{
	ColorVec expectedColors;

	expectedColors.reserve(kNumLayers);
	expectedColors.push_back(tcu::Vec4(0.0, 0.0, 1.0, 1.0));
	expectedColors.push_back(tcu::Vec4(1.0, 0.0, 1.0, 1.0));
	expectedColors.push_back(tcu::Vec4(0.0, 1.0, 1.0, 1.0));
	expectedColors.push_back(tcu::Vec4(1.0, 1.0, 0.0, 1.0));

	const auto numWorkGroups = (m_shareVertices ? 1u : kNumLayers);
	const IterationParams iterationParams =
	{
		getDefaultExtent(),						//	VkExtent2D					colorExtent;
		kNumLayers,								//	uint32_t					numLayers;
		getDefaultDrawCommands(numWorkGroups),	//	DrawCommandVec				drawArgs;
		false,									//	bool						indirect;
		{},										//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,							//	tcu::Maybe<FragmentSize>	fragmentSize;
	};
	return new FullScreenColorInstance(context, iterationParams, expectedColors);
}

// ViewportIndex builtin case.
class ViewportIndexCase : public MeshShaderBuiltinCase
{
public:
					ViewportIndexCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, bool shareVertices)
						: MeshShaderBuiltinCase	(testCtx, name, description, false/*taskNeeded*/)
						, m_shareVertices		(shareVertices)
						{}
	virtual			~ViewportIndexCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	void			checkSupport		(Context& context) const override;
	TestInstance*	createInstance		(Context& context) const override;

	static constexpr uint32_t kQuadrants = 4u;

protected:
	const bool m_shareVertices;
};

void ViewportIndexCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto localSize		= (m_shareVertices ? kQuadrants : 1u);
	const auto numPrimitives	= (m_shareVertices ? kQuadrants : 1u);
	const auto viewportIndex	= (m_shareVertices ? "gl_LocalInvocationIndex" : "gl_WorkGroupID.x");

	// One viewport per local invocation or work group (sharing vertices or not, respectively).
	{
		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << localSize << ") in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=3, max_primitives=" << numPrimitives << ") out;\n"
			<< "\n"
			<< "perprimitiveNV out gl_MeshPerPrimitiveNV {\n"
			<< "   int gl_ViewportIndex;\n"
			<< "} gl_MeshPrimitivesNV[];\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = " << numPrimitives << ";\n"
			<< "\n"
			<< "    if (gl_LocalInvocationIndex == 0u)\n"
			<< "    {\n"
			<< "        gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesNV[1].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesNV[2].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
			<< "    }\n"
			<< "\n"
			<< "    const uint baseIndex = gl_LocalInvocationIndex * 3u;\n"
			<< "    gl_PrimitiveIndicesNV[baseIndex + 0] = 0;\n"
			<< "    gl_PrimitiveIndicesNV[baseIndex + 1] = 1;\n"
			<< "    gl_PrimitiveIndicesNV[baseIndex + 2] = 2;\n"
			<< "\n"
			<< "    gl_MeshPrimitivesNV[gl_LocalInvocationIndex].gl_ViewportIndex = int(" << viewportIndex << ");\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Fragment shader chooses one color per viewport.
	{
		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "\n"
			<< "vec4 colors[" << kQuadrants << "] = vec4[](\n"
			<< "    vec4(0.0, 0.0, 1.0, 1.0),\n"
			<< "    vec4(1.0, 0.0, 1.0, 1.0),\n"
			<< "    vec4(0.0, 1.0, 1.0, 1.0),\n"
			<< "    vec4(1.0, 1.0, 0.0, 1.0)\n"
			<< ");\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    outColor = colors[gl_ViewportIndex];\n"
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
}

void ViewportIndexCase::checkSupport (Context& context) const
{
	MeshShaderBuiltinCase::checkSupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);

	if (!context.contextSupports(vk::ApiVersion(0u, 1u, 2u, 0u)))
		context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");
	else
	{
		const auto& features = context.getDeviceVulkan12Features();
		if (!features.shaderOutputViewportIndex)
			TCU_THROW(NotSupportedError, "shaderOutputViewportIndex feature not supported");
	}
}

TestInstance* ViewportIndexCase::createInstance (Context& context) const
{
	const auto extent = getDefaultExtent();

	DE_ASSERT(extent.width > 0u && extent.width % 2u == 0u);
	DE_ASSERT(extent.height > 0u && extent.height % 2u == 0u);

	const auto halfWidth	= static_cast<float>(extent.width) / 2.0f;
	const auto halfHeight	= static_cast<float>(extent.height) / 2.0f;

	const auto topLeft		= tcu::Vec4(0.0, 0.0, 1.0, 1.0);
	const auto topRight		= tcu::Vec4(1.0, 0.0, 1.0, 1.0);
	const auto bottomLeft	= tcu::Vec4(0.0, 1.0, 1.0, 1.0);
	const auto bottomRight	= tcu::Vec4(1.0, 1.0, 0.0, 1.0);

	ViewportVec viewports;
	viewports.reserve(kQuadrants);
	viewports.emplace_back(makeViewport(0.0f,		0.0f,		halfWidth, halfHeight, 0.0f, 1.0f));
	viewports.emplace_back(makeViewport(halfWidth,	0.0f,		halfWidth, halfHeight, 0.0f, 1.0f));
	viewports.emplace_back(makeViewport(0.0f,		halfHeight,	halfWidth, halfHeight, 0.0f, 1.0f));
	viewports.emplace_back(makeViewport(halfWidth,	halfHeight,	halfWidth, halfHeight, 0.0f, 1.0f));

	const auto numWorkGroups = (m_shareVertices ? 1u : kQuadrants);
	const IterationParams iterationParams =
	{
		getDefaultExtent(),						//	VkExtent2D					colorExtent;
		1u,										//	uint32_t					numLayers;
		getDefaultDrawCommands(numWorkGroups),	//	DrawCommandVec				drawArgs;
		false,									//	bool						indirect;
		std::move(viewports),					//	ViewportVec					viewports;
		tcu::Nothing,							//	tcu::Maybe<FragmentSize>	fragmentSize;
	};
	return new QuadrantsInstance(context, iterationParams, topLeft, topRight, bottomLeft, bottomRight);
}

// Position builtin case.
class PositionCase : public MeshShaderBuiltinCase
{
public:
					PositionCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: MeshShaderBuiltinCase (testCtx, name, description, false/*taskNeeded*/)
						{}
	virtual			~PositionCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance		(Context& context) const override;
};

void PositionCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Mesh shader: emit single triangle around the center of the top left pixel.
	{
		const auto extent	= getDefaultExtent();
		const auto fWidth	= static_cast<float>(extent.width);
		const auto fHeight	= static_cast<float>(extent.height);

		const auto pxWidth	= 2.0f / fWidth;
		const auto pxHeight = 2.0f / fHeight;

		const auto halfXPix	= pxWidth / 2.0f;
		const auto halfYPix	= pxHeight / 2.0f;

		// Center of top left pixel.
		const auto x		= -1.0f + halfXPix;
		const auto y		= -1.0f + halfYPix;

		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=3, max_primitives=1) out;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 1u;\n"
			<< "\n"
			<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
			<< "    gl_PrimitiveIndicesNV[1] = 1;\n"
			<< "    gl_PrimitiveIndicesNV[2] = 2;\n"
			<< "\n"
			<< "    gl_MeshVerticesNV[0].gl_Position = vec4(" << (x - halfXPix) << ", " << (y + halfYPix) << ", 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[1].gl_Position = vec4(" << (x + halfXPix) << ", " << (y + halfYPix) << ", 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[2].gl_Position = vec4(" << x << ", " << (y - halfYPix) << ", 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Basic fragment shader.
	{
		const auto frag = getBasicFragShader();
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
	}
}

TestInstance* PositionCase::createInstance (Context& context) const
{
	const IterationParams iterationParams =
	{
		getDefaultExtent(),			//	VkExtent2D					colorExtent;
		1u,							//	uint32_t					numLayers;
		getDefaultDrawCommands(),	//	DrawCommandVec				drawArgs;
		false,						//	bool						indirect;
		{},							//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,				//	tcu::Maybe<FragmentSize>	fragmentSize;
	};

	// Must match the shader.
	PixelMap pixelMap;
	pixelMap[tcu::IVec2(0, 0)] = tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);

	const PixelVerifierParams verifierParams =
	{
		tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),	//	const tcu::Vec4		background;
		std::move(pixelMap),				//	const PixelMap		pixelMap;
	};
	return new PixelsInstance(context, iterationParams, verifierParams);
}

// PointSize builtin case.
class PointSizeCase : public MeshShaderBuiltinCase
{
public:
					PointSizeCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: MeshShaderBuiltinCase (testCtx, name, description, false/*taskNeeded*/)
						{}
	virtual			~PointSizeCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance		(Context& context) const override;
	void			checkSupport		(Context& context) const override;

	static constexpr float kPointSize = 4.0f;
};

void PointSizeCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Mesh shader: large point covering the top left quadrant.
	{
		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "layout (points) out;\n"
			<< "layout (max_vertices=1, max_primitives=1) out;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 1u;\n"
			<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
			<< "    gl_MeshVerticesNV[0].gl_Position = vec4(-0.5, -0.5, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[0].gl_PointSize = " << kPointSize << ";\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Basic fragment shader.
	{
		const auto frag = getBasicFragShader();
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
	}
}

TestInstance* PointSizeCase::createInstance (Context& context) const
{
	const IterationParams iterationParams =
	{
		getDefaultExtent(),			//	VkExtent2D					colorExtent;
		1u,							//	uint32_t					numLayers;
		getDefaultDrawCommands(),	//	DrawCommandVec				drawArgs;
		false,						//	bool						indirect;
		{},							//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,				//	tcu::Maybe<FragmentSize>	fragmentSize;
	};

	// Must match the shader.
	const tcu::Vec4 black	(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4 blue	(0.0f, 0.0f, 1.0f, 1.0f);

	return new QuadrantsInstance(context, iterationParams, blue, black, black, black);
}

void PointSizeCase::checkSupport (Context& context) const
{
	MeshShaderBuiltinCase::checkSupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_LARGE_POINTS);

	const auto& properties = context.getDeviceProperties();
	if (kPointSize < properties.limits.pointSizeRange[0] || kPointSize > properties.limits.pointSizeRange[1])
		TCU_THROW(NotSupportedError, "Required point size outside point size range");
}

// ClipDistance builtin case.
class ClipDistanceCase : public MeshShaderBuiltinCase
{
public:
					ClipDistanceCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: MeshShaderBuiltinCase (testCtx, name, description, false/*taskNeeded*/)
						{}
	virtual			~ClipDistanceCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance		(Context& context) const override;
	void			checkSupport		(Context& context) const override;
};

void ClipDistanceCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Mesh shader: full-screen quad using different clip distances.
	{
		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=4, max_primitives=2) out;\n"
			<< "\n"
			<< "out gl_MeshPerVertexNV {\n"
			<< "    vec4  gl_Position;\n"
			<< "    float gl_ClipDistance[2];\n"
			<< "} gl_MeshVerticesNV[];\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 2u;\n"
			<< "\n"
			<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
			<< "    gl_PrimitiveIndicesNV[1] = 1;\n"
			<< "    gl_PrimitiveIndicesNV[2] = 2;\n"
			<< "    gl_PrimitiveIndicesNV[3] = 1;\n"
			<< "    gl_PrimitiveIndicesNV[4] = 3;\n"
			<< "    gl_PrimitiveIndicesNV[5] = 2;\n"
			<< "\n"
			<< "    gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[1].gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[2].gl_Position = vec4( 1.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[3].gl_Position = vec4( 1.0,  1.0, 0.0, 1.0);\n"
			<< "\n"
			// The first clip plane keeps the left half of the frame buffer.
			<< "    gl_MeshVerticesNV[0].gl_ClipDistance[0] =  1.0;\n"
			<< "    gl_MeshVerticesNV[1].gl_ClipDistance[0] =  1.0;\n"
			<< "    gl_MeshVerticesNV[2].gl_ClipDistance[0] = -1.0;\n"
			<< "    gl_MeshVerticesNV[3].gl_ClipDistance[0] = -1.0;\n"
			<< "\n"
			// The second clip plane keeps the top half of the frame buffer.
			<< "    gl_MeshVerticesNV[0].gl_ClipDistance[1] =  1.0;\n"
			<< "    gl_MeshVerticesNV[1].gl_ClipDistance[1] = -1.0;\n"
			<< "    gl_MeshVerticesNV[2].gl_ClipDistance[1] =  1.0;\n"
			<< "    gl_MeshVerticesNV[3].gl_ClipDistance[1] = -1.0;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Fragment shader chooses a constant color.
	{
		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			// White color should not actually be used, as those fragments are supposed to be discarded.
			<< "    outColor = ((gl_ClipDistance[0] >= 0.0 && gl_ClipDistance[1] >= 0.0) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(1.0, 1.0, 1.0, 1.0));\n"
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
}

TestInstance* ClipDistanceCase::createInstance (Context& context) const
{
	const IterationParams iterationParams =
	{
		getDefaultExtent(),			//	VkExtent2D					colorExtent;
		1u,							//	uint32_t					numLayers;
		getDefaultDrawCommands(),	//	DrawCommandVec				drawArgs;
		false,						//	bool						indirect;
		{},							//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,				//	tcu::Maybe<FragmentSize>	fragmentSize;
	};

	// Must match the shader.
	const tcu::Vec4 black	(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4 blue	(0.0f, 0.0f, 1.0f, 1.0f);

	return new QuadrantsInstance(context, iterationParams, blue, black, black, black);
}

void ClipDistanceCase::checkSupport (Context& context) const
{
	MeshShaderBuiltinCase::checkSupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_CLIP_DISTANCE);
}

// CullDistance builtin case.
class CullDistanceCase : public MeshShaderBuiltinCase
{
public:
					CullDistanceCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: MeshShaderBuiltinCase (testCtx, name, description, false/*taskNeeded*/)
						{}
	virtual			~CullDistanceCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance		(Context& context) const override;
	void			checkSupport		(Context& context) const override;
};

void CullDistanceCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Mesh shader: two quads covering the whole screen, one on top of the other.
	// Use cull distances to discard the bottom quad.
	// Use cull distances to paint the top one in two colors: blue on the left, white on the right.
	{
		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=6, max_primitives=4) out;\n"
			<< "\n"
			<< "out gl_MeshPerVertexNV {\n"
			<< "    vec4  gl_Position;\n"
			<< "    float gl_CullDistance[2];\n"
			<< "} gl_MeshVerticesNV[];\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 4u;\n"
			<< "\n"
			<< "    gl_PrimitiveIndicesNV[0]  = 0;\n"
			<< "    gl_PrimitiveIndicesNV[1]  = 1;\n"
			<< "    gl_PrimitiveIndicesNV[2]  = 3;\n"
			<< "    gl_PrimitiveIndicesNV[3]  = 1;\n"
			<< "    gl_PrimitiveIndicesNV[4]  = 4;\n"
			<< "    gl_PrimitiveIndicesNV[5]  = 3;\n"
			<< "    gl_PrimitiveIndicesNV[6]  = 1;\n"
			<< "    gl_PrimitiveIndicesNV[7]  = 2;\n"
			<< "    gl_PrimitiveIndicesNV[8]  = 4;\n"
			<< "    gl_PrimitiveIndicesNV[9]  = 2;\n"
			<< "    gl_PrimitiveIndicesNV[10] = 5;\n"
			<< "    gl_PrimitiveIndicesNV[11] = 4;\n"
			<< "\n"
			<< "    gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[1].gl_Position = vec4(-1.0,  0.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[2].gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[3].gl_Position = vec4( 1.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[4].gl_Position = vec4( 1.0,  0.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[5].gl_Position = vec4( 1.0,  1.0, 0.0, 1.0);\n"
			<< "\n"
			// The first cull plane discards the bottom quad
			<< "    gl_MeshVerticesNV[0].gl_CullDistance[0] =  1.0;\n"
			<< "    gl_MeshVerticesNV[1].gl_CullDistance[0] = -1.0;\n"
			<< "    gl_MeshVerticesNV[2].gl_CullDistance[0] = -2.0;\n"
			<< "    gl_MeshVerticesNV[3].gl_CullDistance[0] =  1.0;\n"
			<< "    gl_MeshVerticesNV[4].gl_CullDistance[0] = -1.0;\n"
			<< "    gl_MeshVerticesNV[5].gl_CullDistance[0] = -2.0;\n"
			<< "\n"
			// The second cull plane helps paint left and right different.
			<< "    gl_MeshVerticesNV[0].gl_CullDistance[1] =  1.0;\n"
			<< "    gl_MeshVerticesNV[1].gl_CullDistance[1] =  1.0;\n"
			<< "    gl_MeshVerticesNV[2].gl_CullDistance[1] =  1.0;\n"
			<< "    gl_MeshVerticesNV[3].gl_CullDistance[1] = -1.0;\n"
			<< "    gl_MeshVerticesNV[4].gl_CullDistance[1] = -1.0;\n"
			<< "    gl_MeshVerticesNV[5].gl_CullDistance[1] = -1.0;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Fragment shader chooses color based on the second cull distance.
	{
		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    outColor = ((gl_CullDistance[1] >= 0.0) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(1.0, 1.0, 1.0, 1.0));\n"
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
}

TestInstance* CullDistanceCase::createInstance (Context& context) const
{
	const IterationParams iterationParams =
	{
		getDefaultExtent(),			//	VkExtent2D					colorExtent;
		1u,							//	uint32_t					numLayers;
		getDefaultDrawCommands(),	//	DrawCommandVec				drawArgs;
		false,						//	bool						indirect;
		{},							//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,				//	tcu::Maybe<FragmentSize>	fragmentSize;
	};

	// Must match the shader.
	const tcu::Vec4 black	(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4 blue	(0.0f, 0.0f, 1.0f, 1.0f);
	const tcu::Vec4 white	(1.0f, 1.0f, 1.0f, 1.0f);

	return new QuadrantsInstance(context, iterationParams, blue, white, black, black);
}

void CullDistanceCase::checkSupport (Context& context) const
{
	MeshShaderBuiltinCase::checkSupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_CULL_DISTANCE);
}

// Generates statements to draw a triangle around the given pixel number, knowing the framebuffer width (len).
// Supposes the height of the framebuffer is 1.
std::string triangleForPixel(const std::string& pixel, const std::string& len, const std::string& baseIndex)
{
	std::ostringstream statements;
	statements
		<< "    const float imgWidth = float(" << len << ");\n"
		<< "    const float pixWidth = (2.0 / imgWidth);\n"
		<< "    const float halfPix  = (pixWidth / 2.0);\n"
		<< "    const float xCenter  = (((float(" << pixel << ") + 0.5) / imgWidth) * 2.0 - 1.0);\n"
		<< "    const float xLeft    = (xCenter - halfPix);\n"
		<< "    const float xRight   = (xCenter + halfPix);\n"
		<< "    const uvec3 indices  = uvec3(" << baseIndex << " + 0, " << baseIndex << " + 1, " << baseIndex << " + 2);\n"
		<< "\n"
		<< "    gl_PrimitiveIndicesNV[indices.x] = indices.x;\n"
		<< "    gl_PrimitiveIndicesNV[indices.y] = indices.y;\n"
		<< "    gl_PrimitiveIndicesNV[indices.z] = indices.z;\n"
		<< "\n"
		<< "    gl_MeshVerticesNV[indices.x].gl_Position = vec4(xLeft,    0.5, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesNV[indices.y].gl_Position = vec4(xRight,   0.5, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesNV[indices.z].gl_Position = vec4(xCenter, -0.5, 0.0, 1.0);\n"
		;
	return statements.str();
}

// WorkGroupID builtin case.
class WorkGroupIdCase : public MeshShaderBuiltinCase
{
public:
					WorkGroupIdCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, bool taskNeeded)
						: MeshShaderBuiltinCase (testCtx, name, description, taskNeeded)
						, m_extent				(getLinearExtent())
						{}
	virtual			~WorkGroupIdCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance		(Context& context) const override;

protected:
	const VkExtent2D m_extent;
};

void WorkGroupIdCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const std::string taskDataDecl =
		"taskNV TaskData {\n"
		"    uint id;\n"
		"    uint size;\n"
		"} td;\n"
		;

	// Mesh shader: each work group fills one pixel.
	{
		const std::string pixel = (m_taskNeeded ? "td.id"   : "gl_WorkGroupID.x"   );
		const std::string len   = (m_taskNeeded ? "td.size" : de::toString(m_extent.width) );

		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=3, max_primitives=1) out;\n"
			<< "\n"
			<< (m_taskNeeded ? ("in " + taskDataDecl) : "")
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 1u;\n"
			<< "\n"
			<< triangleForPixel(pixel, len, "0")
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	if (m_taskNeeded)
	{
		std::ostringstream task;
		task
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "\n"
			<< "out " << taskDataDecl
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = 1u;\n"
			<< "    td.id          = gl_WorkGroupID.x;\n"
			<< "    td.size        = " << m_extent.width << ";\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	// Basic fragment shader.
	{
		const auto frag = getBasicFragShader();
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
	}
}

TestInstance* WorkGroupIdCase::createInstance (Context& context) const
{
	// Must match the shader.
	const ColorVec			expectedColors	(1u, tcu::Vec4(0.0, 0.0, 1.0, 1.0));
	const IterationParams	iterationParams	=
	{
		m_extent,								//	VkExtent2D					colorExtent;
		1u,										//	uint32_t					numLayers;
		getDefaultDrawCommands(m_extent.width),	//	DrawCommandVec				drawArgs;
		false,									//	bool						indirect;
		{},										//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,							//	tcu::Maybe<FragmentSize>	fragmentSize;
	};
	return new FullScreenColorInstance(context, iterationParams, expectedColors);
}

// Variable to use.
enum class LocalInvocation { ID=0, INDEX };

// LocalInvocationId and LocalInvocationIndex builtin cases. These are also used to test WorkGroupSize.
class LocalInvocationCase : public MeshShaderBuiltinCase
{
public:
					LocalInvocationCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, bool taskNeeded, LocalInvocation variable)
						: MeshShaderBuiltinCase (testCtx, name, description, taskNeeded)
						, m_extent				(getLinearExtent())
						, m_variable			(variable)
						{}
	virtual			~LocalInvocationCase	(void) {}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

protected:
	const VkExtent2D      m_extent;
	const LocalInvocation m_variable;
};

void LocalInvocationCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Invocation index to use.
	const std::string localIndex = ((m_variable == LocalInvocation::ID) ? "gl_LocalInvocationID.x" : "gl_LocalInvocationIndex");

	// Task data.
	std::ostringstream taskDataDecl;
	taskDataDecl
		<< "taskNV TaskData {\n"
		// indexNumber[x] == x
		<< "    uint indexNumber[" << m_extent.width << "];\n"
		<< "    uint size;\n"
		<< "} td;\n"
		;
	const auto taskDataDeclStr = taskDataDecl.str();

	// Mesh shader: each work group fills one pixel.
	{
		const std::string pixel     = (m_taskNeeded ? "td.indexNumber[gl_WorkGroupID.x]" : localIndex);
		const std::string len       = (m_taskNeeded ? "td.size" : "gl_WorkGroupSize.x");
		const auto        localSize = (m_taskNeeded ? 1u : m_extent.width);
		const auto        maxVert   = localSize * 3u;
		const std::string baseIndex = (m_taskNeeded ? "0" : "(" + localIndex + " * 3u)");

		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << localSize << ") in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=" << maxVert << ", max_primitives=" << localSize << ") out;\n"
			<< "\n"
			<< (m_taskNeeded ? ("in " + taskDataDeclStr) : "")
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = " << localSize << ";\n"
			<< "\n"
			<< triangleForPixel(pixel, len, baseIndex)
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	if (m_taskNeeded)
	{
		std::ostringstream task;
		task
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << m_extent.width << ") in;\n"
			<< "\n"
			<< "out " << taskDataDeclStr
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = " << m_extent.width << ";\n"
			<< "    td.indexNumber[" << localIndex << "] = " << localIndex << ";\n"
			<< "    td.size = gl_WorkGroupSize.x;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	// Basic fragment shader.
	{
		const auto frag = getBasicFragShader();
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
	}
}

TestInstance* LocalInvocationCase::createInstance (Context& context) const
{
	// Must match the shader.
	const ColorVec			expectedColors	(1u, tcu::Vec4(0.0, 0.0, 1.0, 1.0));
	const IterationParams	iterationParams	=
	{
		m_extent,					//	VkExtent2D					colorExtent;
		1u,							//	uint32_t					numLayers;
		getDefaultDrawCommands(),	//	DrawCommandVec				drawArgs;
		false,						//	bool						indirect;
		{},							//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,				//	tcu::Maybe<FragmentSize>	fragmentSize;
	};
	return new FullScreenColorInstance(context, iterationParams, expectedColors);
}

// GlobalInvocationId builtin case.
class GlobalInvocationIdCase : public MeshShaderBuiltinCase
{
public:
					GlobalInvocationIdCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, bool taskNeeded)
						: MeshShaderBuiltinCase (testCtx, name, description, taskNeeded)
						, m_jobSize				(getLargeJobSize())
						, m_extent				{m_jobSize.numTasks * m_jobSize.localSize, 1u}
						{}
	virtual			~GlobalInvocationIdCase		(void) {}

	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override;

protected:
	const JobSize    m_jobSize;
	const VkExtent2D m_extent;
};

void GlobalInvocationIdCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto& localSize = m_jobSize.localSize;

	// Task data.
	std::ostringstream taskDataDecl;
	taskDataDecl
		<< "taskNV TaskData {\n"
		<< "    uint pixelId[" << localSize << "];\n"
		<< "    uint size;\n"
		<< "} td;\n"
		;
	const auto taskDataDeclStr = taskDataDecl.str();

	// Mesh shader: each work group fills one pixel.
	{
		const std::string pixel     = (m_taskNeeded ? "td.pixelId[gl_LocalInvocationIndex]" : "gl_GlobalInvocationID.x");
		const std::string len       = (m_taskNeeded ? "td.size" : de::toString(m_extent.width));
		const std::string baseIndex = "(gl_LocalInvocationIndex * 3u)";
		const auto        maxVert   = localSize * 3u;

		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << localSize << ") in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=" << maxVert << ", max_primitives=" << localSize << ") out;\n"
			<< "\n"
			<< (m_taskNeeded ? ("in " + taskDataDeclStr) : "")
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = " << localSize << ";\n"
			<< "\n"
			<< triangleForPixel(pixel, len, baseIndex)
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	if (m_taskNeeded)
	{
		std::ostringstream task;
		task
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << localSize << ") in;\n"
			<< "\n"
			<< "out " << taskDataDeclStr
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = 1;\n"
			<< "    td.pixelId[gl_LocalInvocationIndex] = gl_GlobalInvocationID.x;\n"
			<< "    td.size = " << m_extent.width << ";\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	// Basic fragment shader.
	{
		const auto frag = getBasicFragShader();
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
	}
}

TestInstance* GlobalInvocationIdCase::createInstance (Context& context) const
{
	// Must match the shader.
	const ColorVec			expectedColors	(1u, tcu::Vec4(0.0, 0.0, 1.0, 1.0));
	const IterationParams	iterationParams	=
	{
		m_extent,									//	VkExtent2D					colorExtent;
		1u,											//	uint32_t					numLayers;
		getDefaultDrawCommands(m_jobSize.numTasks),	//	DrawCommandVec				drawArgs;
		false,										//	bool						indirect;
		{},											//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,								//	tcu::Maybe<FragmentSize>	fragmentSize;
	};
	return new FullScreenColorInstance(context, iterationParams, expectedColors);
}

// DrawIndex builtin case.
class DrawIndexCase : public MeshShaderBuiltinCase
{
public:
					DrawIndexCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, bool taskNeeded)
						: MeshShaderBuiltinCase (testCtx, name, description, taskNeeded)
						, m_extent				(getLinearExtent())
						{}
	virtual			~DrawIndexCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance		(Context& context) const override;

protected:
	const VkExtent2D m_extent;
};

void DrawIndexCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const std::string taskDataDecl =
		"taskNV TaskData {\n"
		"    uint id;\n"
		"    uint size;\n"
		"} td;\n"
		;

	const auto drawIndex = "uint(gl_DrawID)";

	// Mesh shader: each work group fills one pixel.
	{
		const std::string pixel = (m_taskNeeded ? "td.id"   : drawIndex);
		const std::string len   = (m_taskNeeded ? "td.size" : de::toString(m_extent.width));

		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=3, max_primitives=1) out;\n"
			<< "\n"
			<< (m_taskNeeded ? ("in " + taskDataDecl) : "")
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 1u;\n"
			<< "\n"
			<< triangleForPixel(pixel, len, "0")
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	if (m_taskNeeded)
	{
		std::ostringstream task;
		task
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "\n"
			<< "out " << taskDataDecl
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = 1u;\n"
			<< "    td.id          = " << drawIndex << ";\n"
			<< "    td.size        = " << m_extent.width << ";\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	// Basic fragment shader.
	{
		const auto frag = getBasicFragShader();
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
	}
}

TestInstance* DrawIndexCase::createInstance (Context& context) const
{
	// Must match the shader.
	const ColorVec			expectedColors	(1u, tcu::Vec4(0.0, 0.0, 1.0, 1.0));
	const DrawCommandVec	commands		(m_extent.width, makeDrawMeshTasksIndirectCommandNV(1u, 0u));
	const IterationParams	iterationParams	=
	{
		m_extent,		//	VkExtent2D					colorExtent;
		1u,				//	uint32_t					numLayers;
		commands,		//	DrawCommandVec				drawArgs;
		true,			//	bool						indirect;
		{},				//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::Nothing,	//	tcu::Maybe<FragmentSize>	fragmentSize;
	};
	return new FullScreenColorInstance(context, iterationParams, expectedColors);
}

// Primitive Shading Rate case.
class PrimitiveShadingRateCase : public MeshShaderBuiltinCase
{
public:
					PrimitiveShadingRateCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, FragmentSize topSize, FragmentSize bottomSize)
						: MeshShaderBuiltinCase	(testCtx, name, description, false/*taskNeeded*/)
						, m_topSize				(topSize)
						, m_bottomSize			(bottomSize)
						{}
	virtual			~PrimitiveShadingRateCase	(void) {}

	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	void			checkSupport				(Context& context) const override;
	TestInstance*	createInstance				(Context& context) const override;

protected:
	const FragmentSize m_topSize;
	const FragmentSize m_bottomSize;
};

void PrimitiveShadingRateCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Shading rate masks to use.
	const auto topMask			= getGLSLShadingRateMask(m_topSize);
	const auto bottomMask		= getGLSLShadingRateMask(m_bottomSize);
	const auto topMaskVal		= getSPVShadingRateValue(m_topSize);
	const auto bottomMaskVal	= getSPVShadingRateValue(m_bottomSize);

	// Mesh shader.
	{
		// Similar to the GLSL code below if glslang accepted it.
		// Top quad with two triangles and bottom quad with two triangles.
		// One shading rate mask each.
#if 0
		#version 460
		#extension GL_NV_mesh_shader : enable
		#extension GL_EXT_fragment_shading_rate : enable

		layout (local_size_x=1) in;
		layout (triangles) out;
		layout (max_vertices=6, max_primitives=4) out;

		perprimitiveNV out gl_MeshPerPrimitiveNV {
			int gl_PrimitiveShadingRateEXT;
		} gl_MeshPrimitivesNV[];

		void main ()
		{
			gl_PrimitiveCountNV = 4u;

			const vec4 topLeft  = vec4(-1.0, -1.0, 0.0, 1.0);
			const vec4 midLeft  = vec4(-1.0,  0.0, 0.0, 1.0);
			const vec4 botLeft  = vec4(-1.0,  1.0, 0.0, 1.0);

			const vec4 topRight = vec4( 1.0, -1.0, 0.0, 1.0);
			const vec4 midRight = vec4( 1.0,  0.0, 0.0, 1.0);
			const vec4 botRight = vec4( 1.0,  1.0, 0.0, 1.0);

			gl_MeshVerticesNV[0].gl_Position = topLeft;
			gl_MeshVerticesNV[1].gl_Position = midLeft;
			gl_MeshVerticesNV[2].gl_Position = botLeft;

			gl_MeshVerticesNV[3].gl_Position = topRight;
			gl_MeshVerticesNV[4].gl_Position = midRight;
			gl_MeshVerticesNV[5].gl_Position = botRight;

			gl_PrimitiveIndicesNV[0]  = 0u;
			gl_PrimitiveIndicesNV[1]  = 1u;
			gl_PrimitiveIndicesNV[2]  = 3u;
			gl_PrimitiveIndicesNV[3]  = 1u;
			gl_PrimitiveIndicesNV[4]  = 4u;
			gl_PrimitiveIndicesNV[5]  = 3u;
			gl_PrimitiveIndicesNV[6]  = 1u;
			gl_PrimitiveIndicesNV[7]  = 2u;
			gl_PrimitiveIndicesNV[8]  = 4u;
			gl_PrimitiveIndicesNV[9]  = 2u;
			gl_PrimitiveIndicesNV[10] = 5u;
			gl_PrimitiveIndicesNV[11] = 4u;

			gl_MeshPrimitivesNV[0].gl_PrimitiveShadingRateEXT = TOP_MASK;
			gl_MeshPrimitivesNV[1].gl_PrimitiveShadingRateEXT = TOP_MASK;
			gl_MeshPrimitivesNV[2].gl_PrimitiveShadingRateEXT = BOTTOM_MASK;
			gl_MeshPrimitivesNV[3].gl_PrimitiveShadingRateEXT = BOTTOM_MASK;
		}
#endif
		std::ostringstream meshSPV;
		meshSPV
			<< "; SPIR-V\n"
			<< "; Version: 1.0\n"
			<< "; Generator: Khronos Glslang Reference Front End; 10\n"
			<< "; Bound: 81\n"
			<< "; Schema: 0\n"
			<< "      OpCapability MeshShadingNV\n"
			<< "      OpCapability FragmentShadingRateKHR\n"					// Added manually.
			<< "      OpExtension \"SPV_NV_mesh_shader\"\n"
			<< "      OpExtension \"SPV_KHR_fragment_shading_rate\"\n"			// Added manually.
			<< " %1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "      OpMemoryModel Logical GLSL450\n"
			<< "      OpEntryPoint MeshNV %4 \"main\" %8 %20 %47 %73\n"
			<< "      OpExecutionMode %4 LocalSize 1 1 1\n"
			<< "      OpExecutionMode %4 OutputVertices 6\n"
			<< "      OpExecutionMode %4 OutputPrimitivesNV 4\n"
			<< "      OpExecutionMode %4 OutputTrianglesNV\n"
			<< "      OpDecorate %8 BuiltIn PrimitiveCountNV\n"
			<< "      OpMemberDecorate %16 0 BuiltIn Position\n"
			<< "      OpMemberDecorate %16 1 BuiltIn PointSize\n"
			<< "      OpMemberDecorate %16 2 BuiltIn ClipDistance\n"
			<< "      OpMemberDecorate %16 3 BuiltIn CullDistance\n"
			<< "      OpMemberDecorate %16 4 PerViewNV\n"
			<< "      OpMemberDecorate %16 4 BuiltIn PositionPerViewNV\n"
			<< "      OpMemberDecorate %16 5 PerViewNV\n"
			<< "      OpMemberDecorate %16 5 BuiltIn ClipDistancePerViewNV\n"
			<< "      OpMemberDecorate %16 6 PerViewNV\n"
			<< "      OpMemberDecorate %16 6 BuiltIn CullDistancePerViewNV\n"
			<< "      OpDecorate %16 Block\n"
			<< "      OpDecorate %47 BuiltIn PrimitiveIndicesNV\n"
			<< "      OpMemberDecorate %70 0 PerPrimitiveNV\n"
			<< "      OpMemberDecorate %70 0 BuiltIn PrimitiveShadingRateKHR\n"	// Replaced PrimitiveID with this.
			<< "      OpDecorate %70 Block\n"
			<< "      OpDecorate %80 BuiltIn WorkgroupSize\n"
			<< " %2 = OpTypeVoid\n"
			<< " %3 = OpTypeFunction %2\n"
			<< " %6 = OpTypeInt 32 0\n"
			<< " %7 = OpTypePointer Output %6\n"
			<< " %8 = OpVariable %7 Output\n"
			<< " %9 = OpConstant %6 4\n"
			<< "%10 = OpTypeFloat 32\n"
			<< "%11 = OpTypeVector %10 4\n"
			<< "%12 = OpConstant %6 1\n"
			<< "%13 = OpTypeArray %10 %12\n"
			<< "%14 = OpTypeArray %11 %9\n"
			<< "%15 = OpTypeArray %13 %9\n"
			<< "%16 = OpTypeStruct %11 %10 %13 %13 %14 %15 %15\n"
			<< "%17 = OpConstant %6 6\n"
			<< "%18 = OpTypeArray %16 %17\n"
			<< "%19 = OpTypePointer Output %18\n"
			<< "%20 = OpVariable %19 Output\n"
			<< "%21 = OpTypeInt 32 1\n"
			<< "%tm = OpConstant %21 " << topMaskVal << "\n"					// Added mask value line.
			<< "%bm = OpConstant %21 " << bottomMaskVal << "\n"					// Ditto.
			<< "%22 = OpConstant %21 0\n"
			<< "%23 = OpConstant %10 -1\n"
			<< "%24 = OpConstant %10 0\n"
			<< "%25 = OpConstant %10 1\n"
			<< "%26 = OpConstantComposite %11 %23 %23 %24 %25\n"
			<< "%27 = OpTypePointer Output %11\n"
			<< "%29 = OpConstant %21 1\n"
			<< "%30 = OpConstantComposite %11 %23 %24 %24 %25\n"
			<< "%32 = OpConstant %21 2\n"
			<< "%33 = OpConstantComposite %11 %23 %25 %24 %25\n"
			<< "%35 = OpConstant %21 3\n"
			<< "%36 = OpConstantComposite %11 %25 %23 %24 %25\n"
			<< "%38 = OpConstant %21 4\n"
			<< "%39 = OpConstantComposite %11 %25 %24 %24 %25\n"
			<< "%41 = OpConstant %21 5\n"
			<< "%42 = OpConstantComposite %11 %25 %25 %24 %25\n"
			<< "%44 = OpConstant %6 12\n"
			<< "%45 = OpTypeArray %6 %44\n"
			<< "%46 = OpTypePointer Output %45\n"
			<< "%47 = OpVariable %46 Output\n"
			<< "%48 = OpConstant %6 0\n"
			<< "%51 = OpConstant %6 3\n"
			<< "%56 = OpConstant %21 6\n"
			<< "%58 = OpConstant %21 7\n"
			<< "%59 = OpConstant %6 2\n"
			<< "%61 = OpConstant %21 8\n"
			<< "%63 = OpConstant %21 9\n"
			<< "%65 = OpConstant %21 10\n"
			<< "%66 = OpConstant %6 5\n"
			<< "%68 = OpConstant %21 11\n"
			<< "%70 = OpTypeStruct %21\n"
			<< "%71 = OpTypeArray %70 %9\n"
			<< "%72 = OpTypePointer Output %71\n"
			<< "%73 = OpVariable %72 Output\n"
			<< "%74 = OpTypePointer Output %21\n"
			<< "%79 = OpTypeVector %6 3\n"
			<< "%80 = OpConstantComposite %79 %12 %12 %12\n"
			<< " %4 = OpFunction %2 None %3\n"
			<< " %5 = OpLabel\n"
			<< "      OpStore %8 %9\n"
			<< "%28 = OpAccessChain %27 %20 %22 %22\n"
			<< "      OpStore %28 %26\n"
			<< "%31 = OpAccessChain %27 %20 %29 %22\n"
			<< "      OpStore %31 %30\n"
			<< "%34 = OpAccessChain %27 %20 %32 %22\n"
			<< "      OpStore %34 %33\n"
			<< "%37 = OpAccessChain %27 %20 %35 %22\n"
			<< "      OpStore %37 %36\n"
			<< "%40 = OpAccessChain %27 %20 %38 %22\n"
			<< "      OpStore %40 %39\n"
			<< "%43 = OpAccessChain %27 %20 %41 %22\n"
			<< "      OpStore %43 %42\n"
			<< "%49 = OpAccessChain %7 %47 %22\n"
			<< "      OpStore %49 %48\n"
			<< "%50 = OpAccessChain %7 %47 %29\n"
			<< "      OpStore %50 %12\n"
			<< "%52 = OpAccessChain %7 %47 %32\n"
			<< "      OpStore %52 %51\n"
			<< "%53 = OpAccessChain %7 %47 %35\n"
			<< "      OpStore %53 %12\n"
			<< "%54 = OpAccessChain %7 %47 %38\n"
			<< "      OpStore %54 %9\n"
			<< "%55 = OpAccessChain %7 %47 %41\n"
			<< "      OpStore %55 %51\n"
			<< "%57 = OpAccessChain %7 %47 %56\n"
			<< "      OpStore %57 %12\n"
			<< "%60 = OpAccessChain %7 %47 %58\n"
			<< "      OpStore %60 %59\n"
			<< "%62 = OpAccessChain %7 %47 %61\n"
			<< "      OpStore %62 %9\n"
			<< "%64 = OpAccessChain %7 %47 %63\n"
			<< "      OpStore %64 %59\n"
			<< "%67 = OpAccessChain %7 %47 %65\n"
			<< "      OpStore %67 %66\n"
			<< "%69 = OpAccessChain %7 %47 %68\n"
			<< "      OpStore %69 %9\n"
			<< "%75 = OpAccessChain %74 %73 %22 %22\n"
			<< "      OpStore %75 %tm\n"										// Store the proper mask here. First triangle, top primitive.
			<< "%76 = OpAccessChain %74 %73 %29 %22\n"
			<< "      OpStore %76 %tm\n"										// Second triangle, top primitive.
			<< "%77 = OpAccessChain %74 %73 %32 %22\n"
			<< "      OpStore %77 %bm\n"										// Third triangle, bottom primitive.
			<< "%78 = OpAccessChain %74 %73 %35 %22\n"
			<< "      OpStore %78 %bm\n"										// Fourth triangle, bottom primitive.
			<< "      OpReturn\n"
			<< "      OpFunctionEnd\n"
			;
		programCollection.spirvAsmSources.add("mesh") << meshSPV.str();
	}

	// Frag shader.
	{
		const auto extent		= getDefaultExtent();
		const auto halfHeight	= static_cast<float>(extent.height) / 2.0f;

		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "#extension GL_EXT_fragment_shading_rate : enable\n"
			<< "\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			// Checks the shading rate matches.
			<< "    const int expectedRate = ((gl_FragCoord.y < " << halfHeight << ")? " << topMask << " : " << bottomMask << ");\n"
			<< "    outColor = ((gl_ShadingRateEXT == expectedRate) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0));\n"
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
}

void PrimitiveShadingRateCase::checkSupport (Context& context) const
{
	MeshShaderBuiltinCase::checkSupport(context);

	context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");
}

TestInstance* PrimitiveShadingRateCase::createInstance (Context& context) const
{
	const ColorVec			expectedColors	(1u, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
	FragmentSizeVector		fsInUse			{m_topSize, m_bottomSize};
	const IterationParams	iterationParams	=
	{
		getDefaultExtent(),												//	VkExtent2D					colorExtent;
		1u,																//	uint32_t					numLayers;
		getDefaultDrawCommands(),										//	DrawCommandVec				drawArgs;
		false,															//	bool						indirect;
		{},																//	ViewportVec					viewports;	// If empty, a single default viewport is used.
		tcu::just(getBadShadingRateSize(begin(fsInUse), end(fsInUse))),	//	tcu::Maybe<FragmentSize>	fragmentSize;
	};
	return new FullScreenColorInstance(context, iterationParams, expectedColors);
}

} // anonymous

tcu::TestCaseGroup* createMeshShaderBuiltinTests (tcu::TestContext& testCtx)
{
	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "builtin", "Mesh Shader Builtin Tests"));

	mainGroup->addChild(new PositionCase				(testCtx, "position", ""));
	mainGroup->addChild(new PointSizeCase				(testCtx, "point_size", ""));
	mainGroup->addChild(new ClipDistanceCase			(testCtx, "clip_distance", ""));
	mainGroup->addChild(new CullDistanceCase			(testCtx, "cull_distance", ""));
	mainGroup->addChild(new PrimitiveIdCase				(testCtx, "primitive_id_glsl", "", true/*glslFrag*/));
	mainGroup->addChild(new PrimitiveIdCase				(testCtx, "primitive_id_spirv", "", false/*glslFrag*/));
	mainGroup->addChild(new LayerCase					(testCtx, "layer", "", false/*shareVertices*/));
	mainGroup->addChild(new LayerCase					(testCtx, "layer_shared", "", true/*shareVertices*/));
	mainGroup->addChild(new ViewportIndexCase			(testCtx, "viewport_index", "", false/*shareVertices*/));
	mainGroup->addChild(new ViewportIndexCase			(testCtx, "viewport_index_shared", "", true/*shareVertices*/));
	mainGroup->addChild(new WorkGroupIdCase				(testCtx, "work_group_id_in_mesh", "", false/*taskNeeded*/));
	mainGroup->addChild(new WorkGroupIdCase				(testCtx, "work_group_id_in_task", "", true/*taskNeeded*/));
	mainGroup->addChild(new LocalInvocationCase			(testCtx, "local_invocation_id_in_mesh", "", false/*taskNeeded*/, LocalInvocation::ID));
	mainGroup->addChild(new LocalInvocationCase			(testCtx, "local_invocation_id_in_task", "", true/*taskNeeded*/, LocalInvocation::ID));
	mainGroup->addChild(new LocalInvocationCase			(testCtx, "local_invocation_index_in_task", "", true/*taskNeeded*/, LocalInvocation::INDEX));
	mainGroup->addChild(new LocalInvocationCase			(testCtx, "local_invocation_index_in_mesh", "", false/*taskNeeded*/, LocalInvocation::INDEX));
	mainGroup->addChild(new GlobalInvocationIdCase		(testCtx, "global_invocation_id_in_mesh", "", false/*taskNeeded*/));
	mainGroup->addChild(new GlobalInvocationIdCase		(testCtx, "global_invocation_id_in_task", "", true/*taskNeeded*/));
	mainGroup->addChild(new DrawIndexCase				(testCtx, "draw_index_in_mesh", "", false/*taskNeeded*/));
	mainGroup->addChild(new DrawIndexCase				(testCtx, "draw_index_in_task", "", true/*taskNeeded*/));

	// Primitive shading rate tests.
	{
		const auto sizeCount = static_cast<int>(FragmentSize::SIZE_COUNT);

		for (int i = 0; i < sizeCount; ++i)
		for (int j = 0; j < sizeCount; ++j)
		{
			const auto topSize		= static_cast<FragmentSize>(i);
			const auto bottomSize	= static_cast<FragmentSize>(j);

			const auto topExtent	= getShadingRateSize(topSize);
			const auto bottomExtent	= getShadingRateSize(bottomSize);

			const auto testName		= "primitive_shading_rate_"
										+ std::to_string(topExtent.width) + "x" + std::to_string(topExtent.height)
										+ "_"
										+ std::to_string(bottomExtent.width) + "x" + std::to_string(bottomExtent.height)
									;

			mainGroup->addChild(new PrimitiveShadingRateCase(testCtx, testName, "", topSize, bottomSize));
		}
	}

	return mainGroup.release();
}

} // MeshShader
} // vkt
