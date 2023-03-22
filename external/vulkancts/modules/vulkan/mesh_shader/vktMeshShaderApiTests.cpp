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
 * \brief Mesh Shader API Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderApiTests.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"

#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuMaybe.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <limits>

namespace vkt
{
namespace MeshShader
{

namespace
{

using namespace vk;

using GroupPtr				= de::MovePtr<tcu::TestCaseGroup>;
using ImageWithMemoryPtr	= de::MovePtr<ImageWithMemory>;
using BufferWithMemoryPtr	= de::MovePtr<BufferWithMemory>;

enum class DrawType
{
	DRAW = 0,
	DRAW_INDIRECT,
	DRAW_INDIRECT_COUNT,
};

std::ostream& operator<< (std::ostream& stream, DrawType drawType)
{
	switch (drawType)
	{
	case DrawType::DRAW:				stream << "draw";					break;
	case DrawType::DRAW_INDIRECT:		stream << "draw_indirect";			break;
	case DrawType::DRAW_INDIRECT_COUNT:	stream << "draw_indirect_count";	break;
	default: DE_ASSERT(false); break;
	}
	return stream;
}


// This helps test the maxDrawCount rule for the DRAW_INDIRECT_COUNT case.
enum class IndirectCountLimitType
{
	BUFFER_VALUE = 0,		// The actual count will be given by the count buffer.
	MAX_COUNT,				// The actual count will be given by the maxDrawCount argument passed to the draw command.
};

struct IndirectArgs
{
	uint32_t offset;
	uint32_t stride;
};

struct TestParams
{
	DrawType							drawType;
	uint32_t							seed;
	uint32_t							drawCount;				// Equivalent to taskCount or drawCount.
	uint32_t							firstTask;				// Equivalent to firstTask in every call.
	tcu::Maybe<IndirectArgs>			indirectArgs;			// Only used for DRAW_INDIRECT*.
	tcu::Maybe<IndirectCountLimitType>	indirectCountLimit;		// Only used for DRAW_INDIRECT_COUNT.
	tcu::Maybe<uint32_t>				indirectCountOffset;	// Only used for DRAW_INDIRECT_COUNT.
	bool								useTask;
};

// The framebuffer will have a number of rows and 32 columns. Each mesh shader workgroup will generate geometry to fill a single
// framebuffer row, using a triangle list with 32 triangles of different colors, each covering a framebuffer pixel.
//
// Note: the total framebuffer rows is called "full" below (e.g. 64). When using a task shader to generate work, each workgroup will
// generate a single mesh workgroup using a push constant instead of a compile-time constant.
//
// When using DRAW, the task count will tell us how many rows of pixels will be filled in the framebuffer.
//
// When using indirect draws, the full framebuffer will always be drawn into by using multiple draw command structures, except in
// the case of drawCount==0. Each draw will spawn the needed number of tasks to fill the whole framebuffer. In addition, in order to
// make all argument structures different, the number of tasks in each draw count will be slightly different and assigned
// pseudorandomly.
//
// DRAW: taskCount=0, taskCount=1, taskCount=2, taskCount=half, taskCount=full
//
// DRAW_INDIRECT: drawCount=0, drawCount=1, drawCount=2, drawCount=half, drawCount=full.
//  * With offset 0 and pseudorandom (multiples of 4).
//  * With stride adding a padding of 0 and pseudorandom (multiples of 4).
//
// DRAW_INDIRECT_COUNT: same as indirect in two variants:
//  1. Passing the count in a buffer with a large maximum.
//  2. Passing a large value in the buffer and limiting it with the maximum.

class MeshApiCase : public vkt::TestCase
{
public:
					MeshApiCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}
	virtual			~MeshApiCase	(void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	void			checkSupport	(Context& context) const override;
	TestInstance*	createInstance	(Context& context) const override;

protected:
	TestParams		m_params;
};

class MeshApiInstance : public vkt::TestInstance
{
public:
						MeshApiInstance		(Context& context, const TestParams& params)
							: vkt::TestInstance	(context)
							, m_params			(params)
							{}
	virtual				~MeshApiInstance	(void) {}

	tcu::TestStatus		iterate				(void) override;

protected:
	TestParams			m_params;
};

TestInstance* MeshApiCase::createInstance (Context& context) const
{
	return new MeshApiInstance(context, m_params);
}

struct PushConstantData
{
	uint32_t width;
	uint32_t height;
	uint32_t firstTaskMesh;
	uint32_t one;
	uint32_t firstTaskTask;

	std::vector<VkPushConstantRange> getRanges (bool includeTask) const
	{
		constexpr uint32_t offsetMesh = 0u;
		constexpr uint32_t offsetTask = static_cast<uint32_t>(offsetof(PushConstantData, one));
		constexpr uint32_t sizeMesh = offsetTask;
		constexpr uint32_t sizeTask = static_cast<uint32_t>(sizeof(PushConstantData)) - offsetTask;

		const VkPushConstantRange meshRange =
		{
			VK_SHADER_STAGE_MESH_BIT_NV,	//	VkShaderStageFlags	stageFlags;
			offsetMesh,						//	uint32_t			offset;
			sizeMesh,						//	uint32_t			size;
		};
		const VkPushConstantRange taskRange =
		{
			VK_SHADER_STAGE_TASK_BIT_NV,	//	VkShaderStageFlags	stageFlags;
			offsetTask,						//	uint32_t			offset;
			sizeTask,						//	uint32_t			size;
		};

		std::vector<VkPushConstantRange> ranges (1u, meshRange);
		if (includeTask)
			ranges.push_back(taskRange);
		return ranges;
	}
};

void MeshApiCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const std::string taskDataDecl =
		"taskNV TaskData {\n"
		"    uint blockNumber;\n"
		"    uint blockRow;\n"
		"} td;\n"
		;

	// Task shader if needed.
	if (m_params.useTask)
	{
		std::ostringstream task;
		task
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1) in;\n"
			<< "\n"
			<< "layout (push_constant, std430) uniform TaskPushConstantBlock {\n"
			<< "    layout (offset=12) uint one;\n"
			<< "    layout (offset=16) uint firstTask;\n"
			<< "} pc;\n"
			<< "\n"
			<< "out " << taskDataDecl
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV  = pc.one;\n"
			<< "    td.blockNumber  = uint(gl_DrawID);\n"
			<< "    td.blockRow     = gl_WorkGroupID.x - pc.firstTask;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	// Mesh shader.
	{
		std::ostringstream mesh;
		mesh
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=32) in;\n"
			<< "layout (triangles) out;\n"
			<< "layout (max_vertices=96, max_primitives=32) out;\n"
			<< "\n"
			<< "layout (push_constant, std430) uniform MeshPushConstantBlock {\n"
			<< "    uint width;\n"
			<< "    uint height;\n"
			<< "    uint firstTask;\n"
			<< "} pc;\n"
			<< "\n"
			<< "layout (location=0) perprimitiveNV out vec4 primitiveColor[];\n"
			<< "\n"
			<< (m_params.useTask ? ("in " + taskDataDecl): "")
			<< "\n"
			<< "layout (set=0, binding=0, std430) readonly buffer BlockSizes {\n"
			<< "    uint blockSize[];\n"
			<< "} bsz;\n"
			<< "\n"
			<< "uint startOfBlock (uint blockNumber)\n"
			<< "{\n"
			<< "    uint start = 0;\n"
			<< "    for (uint i = 0; i < blockNumber; i++)\n"
			<< "        start += bsz.blockSize[i];\n"
			<< "    return start;\n"
			<< "}\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    const uint blockNumber = " << (m_params.useTask ? "td.blockNumber" : "uint(gl_DrawID)") << ";\n"
			<< "    const uint blockRow = " << (m_params.useTask ? "td.blockRow" : "(gl_WorkGroupID.x - pc.firstTask)") << ";\n"
			<< "\n"
			<< "    // Each workgroup will fill one row, and each invocation will generate a\n"
			<< "    // triangle around the pixel center in each column.\n"
			<< "    const uint row = startOfBlock(blockNumber) + blockRow;\n"
			<< "    const uint col = gl_LocalInvocationID.x;\n"
			<< "\n"
			<< "    const float fHeight = float(pc.height);\n"
			<< "    const float fWidth = float(pc.width);\n"
			<< "\n"
			<< "    // Pixel coordinates, normalized.\n"
			<< "    const float rowNorm = (float(row) + 0.5) / fHeight;\n"
			<< "    const float colNorm = (float(col) + 0.5) / fWidth;\n"
			<< "\n"
			<< "    // Framebuffer coordinates.\n"
			<< "    const float coordX = (colNorm * 2.0) - 1.0;\n"
			<< "    const float coordY = (rowNorm * 2.0) - 1.0;\n"
			<< "\n"
			<< "    const float pixelWidth = 2.0 / fWidth;\n"
			<< "    const float pixelHeight = 2.0 / fHeight;\n"
			<< "\n"
			<< "    const float offsetX = pixelWidth / 2.0;\n"
			<< "    const float offsetY = pixelHeight / 2.0;\n"
			<< "\n"
			<< "    const uint baseIndex = col*3;\n"
			<< "    const uvec3 indices = uvec3(baseIndex, baseIndex + 1, baseIndex + 2);\n"
			<< "\n"
			<< "    gl_PrimitiveCountNV = 32u;\n"
			<< "    primitiveColor[col] = vec4(rowNorm, colNorm, 0.0, 1.0);\n"
			<< "\n"
			<< "    gl_PrimitiveIndicesNV[indices.x] = indices.x;\n"
			<< "    gl_PrimitiveIndicesNV[indices.y] = indices.y;\n"
			<< "    gl_PrimitiveIndicesNV[indices.z] = indices.z;\n"
			<< "\n"
			<< "    gl_MeshVerticesNV[indices.x].gl_Position = vec4(coordX - offsetX, coordY + offsetY, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[indices.y].gl_Position = vec4(coordX + offsetX, coordY + offsetY, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[indices.z].gl_Position = vec4(coordX, coordY - offsetY, 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	// Frag shader.
	{
		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (location=0) perprimitiveNV in vec4 primitiveColor;\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    outColor = primitiveColor;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
}

void MeshApiCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportNV(context, m_params.useTask, true);

	// VUID-vkCmdDrawMeshTasksIndirectNV-drawCount-02718
	if (m_params.drawType == DrawType::DRAW_INDIRECT && m_params.drawCount > 1u)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_DRAW_INDIRECT);
	}

	// VUID-vkCmdDrawMeshTasksIndirectCountNV-None-04445
	if (m_params.drawType == DrawType::DRAW_INDIRECT_COUNT)
		context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
}

template <typename T>
BufferWithMemoryPtr makeStridedBuffer(const DeviceInterface& vkd, VkDevice device, Allocator& alloc, const std::vector<T>& elements, uint32_t offset, uint32_t stride, VkBufferUsageFlags usage, uint32_t endPadding)
{
	const auto elementSize	= static_cast<uint32_t>(sizeof(T));
	const auto actualStride	= std::max(elementSize, stride);
	const auto bufferSize	= static_cast<size_t>(offset) + static_cast<size_t>(actualStride) * elements.size() + static_cast<size_t>(endPadding);
	const auto bufferInfo	= makeBufferCreateInfo(static_cast<VkDeviceSize>(bufferSize), usage);

	BufferWithMemoryPtr buffer(new BufferWithMemory(vkd, device, alloc, bufferInfo, MemoryRequirement::HostVisible));
	auto& bufferAlloc	= buffer->getAllocation();
	char* bufferDataPtr	= reinterpret_cast<char*>(bufferAlloc.getHostPtr());

	char* itr = bufferDataPtr + offset;
	for (const auto& elem : elements)
	{
		deMemcpy(itr, &elem, sizeof(elem));
		itr += actualStride;
	}
	if (endPadding > 0u)
		deMemset(itr, 0xFF, endPadding);

	flushAlloc(vkd, device, bufferAlloc);

	return buffer;
}

VkExtent3D getExtent ()
{
	return makeExtent3D(32u, 64u, 1u);
}

tcu::TestStatus MeshApiInstance::iterate (void)
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();

	const auto		extent		= getExtent();
	const auto		iExtent3D	= tcu::IVec3(static_cast<int>(extent.width), static_cast<int>(extent.height), static_cast<int>(extent.depth));
	const auto		iExtent2D	= tcu::IVec2(iExtent3D.x(), iExtent3D.y());
	const auto		format		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto		tcuFormat	= mapVkFormat(format);
	const auto		colorUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto		colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const tcu::Vec4	clearColor	(0.0f, 0.0f, 0.0f, 1.0f);
	const float		colorThres	= 0.005f; // 1/255 < 0.005 < 2/255
	const tcu::Vec4	threshold	(colorThres, colorThres, 0.0f, 0.0f);

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
			1u,										//	uint32_t				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			colorUsage,								//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	uint32_t				queueFamilyIndexCount;
			nullptr,								//	const uint32_t*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};
		colorBuffer = ImageWithMemoryPtr(new ImageWithMemory(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any));
		colorBufferView = makeImageView(vkd, device, colorBuffer->get(), VK_IMAGE_VIEW_TYPE_2D, format, colorSRR);
	}

	// Prepare buffer containing the array of block sizes.
	de::Random				rnd				(m_params.seed);
	std::vector<uint32_t>	blockSizes;

	const uint32_t			vectorSize		= std::max(1u, m_params.drawCount);
	const uint32_t			largeDrawCount	= vectorSize + 1u; // The indirect buffer needs to have some padding at the end. See below.
	const uint32_t			evenBlockSize	= extent.height / vectorSize;
	uint32_t				remainingRows	= extent.height;

	blockSizes.reserve(vectorSize);
	for (uint32_t i = 0; i < vectorSize - 1u; ++i)
	{
		const auto blockSize = static_cast<uint32_t>(rnd.getInt(1, evenBlockSize));
		remainingRows -= blockSize;
		blockSizes.push_back(blockSize);
	}
	blockSizes.push_back(remainingRows);

	const auto			blockSizesBufferSize	= static_cast<VkDeviceSize>(de::dataSize(blockSizes));
	BufferWithMemoryPtr	blockSizesBuffer		= makeStridedBuffer(vkd, device, alloc, blockSizes, 0u, 0u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0u);

	// Descriptor set layout, pool and set.
	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV);
	const auto setLayout = layoutBuilder.build(vkd, device);

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	// Update descriptor set.
	{
		DescriptorSetUpdateBuilder updateBuilder;

		const auto location				= DescriptorSetUpdateBuilder::Location::binding(0u);
		const auto descriptorBufferInfo	= makeDescriptorBufferInfo(blockSizesBuffer->get(), 0ull, blockSizesBufferSize);

		updateBuilder.writeSingle(descriptorSet.get(), location, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo);
		updateBuilder.update(vkd, device);
	}

	// Pipeline layout.
	PushConstantData	pcData;
	const auto			pcRanges		= pcData.getRanges(m_params.useTask);
	const auto			pipelineLayout	= makePipelineLayout(vkd, device, 1u, &setLayout.get(), static_cast<uint32_t>(pcRanges.size()), de::dataOrNull(pcRanges));

	// Push constants.
	pcData.width			= extent.width;
	pcData.height			= extent.height;
	pcData.firstTaskMesh	= m_params.firstTask;
	pcData.one				= 1u;
	pcData.firstTaskTask	= m_params.firstTask;

	// Render pass and framebuffer.
	const auto renderPass	= makeRenderPass(vkd, device, format);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), colorBufferView.get(), extent.width, extent.height);

	// Pipeline.
	Move<VkShaderModule> taskModule;
	Move<VkShaderModule> meshModule;
	Move<VkShaderModule> fragModule;

	const auto& binaries = m_context.getBinaryCollection();
	if (m_params.useTask)
		taskModule = createShaderModule(vkd, device, binaries.get("task"));
	meshModule = createShaderModule(vkd, device, binaries.get("mesh"));
	fragModule = createShaderModule(vkd, device, binaries.get("frag"));

	const std::vector<VkViewport>	viewports	(1u, makeViewport(extent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(extent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskModule.get(), meshModule.get(), fragModule.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Indirect and count buffers if needed.
	BufferWithMemoryPtr indirectBuffer;
	BufferWithMemoryPtr countBuffer;

	if (m_params.drawType != DrawType::DRAW)
	{
		// Indirect draws.
		DE_ASSERT(static_cast<bool>(m_params.indirectArgs));
		const auto& indirectArgs = m_params.indirectArgs.get();

		// Check stride and offset validity.
		DE_ASSERT(indirectArgs.offset % 4u == 0u);
		DE_ASSERT(indirectArgs.stride % 4u == 0u && (indirectArgs.stride == 0u || indirectArgs.stride >= static_cast<uint32_t>(sizeof(VkDrawMeshTasksIndirectCommandNV))));

		// Prepare struct vector, which will be converted to a buffer with the proper stride and offset later.
		std::vector<VkDrawMeshTasksIndirectCommandNV> commands;
		commands.reserve(blockSizes.size());

		std::transform(begin(blockSizes), end(blockSizes), std::back_inserter(commands),
			[this](uint32_t blockSize) { return VkDrawMeshTasksIndirectCommandNV{blockSize, this->m_params.firstTask}; });

		const auto padding	= static_cast<uint32_t>(sizeof(VkDrawMeshTasksIndirectCommandNV));
		indirectBuffer		= makeStridedBuffer(vkd, device, alloc, commands, indirectArgs.offset, indirectArgs.stride, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, padding);

		// Prepare count buffer if needed.
		if (m_params.drawType == DrawType::DRAW_INDIRECT_COUNT)
		{
			DE_ASSERT(static_cast<bool>(m_params.indirectCountLimit));
			DE_ASSERT(static_cast<bool>(m_params.indirectCountOffset));

			const auto countBufferValue	= ((m_params.indirectCountLimit.get() == IndirectCountLimitType::BUFFER_VALUE)
										? m_params.drawCount
										: largeDrawCount);

			const std::vector<uint32_t> singleCount (1u, countBufferValue);
			countBuffer = makeStridedBuffer(vkd, device, alloc, singleCount, m_params.indirectCountOffset.get(), static_cast<uint32_t>(sizeof(uint32_t)), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, 0u);
		}
	}

	// Submit commands.
	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor);

	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	{
		const char* pcDataPtr = reinterpret_cast<const char*>(&pcData);
		for (const auto& range : pcRanges)
			vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), range.stageFlags, range.offset, range.size, pcDataPtr + range.offset);
	}
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());

	if (m_params.drawType == DrawType::DRAW)
	{
		vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params.drawCount, m_params.firstTask);
	}
	else if (m_params.drawType == DrawType::DRAW_INDIRECT)
	{
		const auto& indirectArgs = m_params.indirectArgs.get();
		vkd.cmdDrawMeshTasksIndirectNV(cmdBuffer, indirectBuffer->get(), indirectArgs.offset, m_params.drawCount, indirectArgs.stride);
	}
	else if (m_params.drawType == DrawType::DRAW_INDIRECT_COUNT)
	{
		const auto& indirectArgs		= m_params.indirectArgs.get();
		const auto& indirectCountOffset	= m_params.indirectCountOffset.get();
		const auto& indirectCountLimit	= m_params.indirectCountLimit.get();

		const auto maxCount	= ((indirectCountLimit == IndirectCountLimitType::MAX_COUNT)
							? m_params.drawCount
							: largeDrawCount);
		vkd.cmdDrawMeshTasksIndirectCountNV(cmdBuffer, indirectBuffer->get(), indirectArgs.offset, countBuffer->get(), indirectCountOffset, maxCount, indirectArgs.stride);
	}
	else
		DE_ASSERT(false);

	endRenderPass(vkd, cmdBuffer);

	// Output buffer to extract the color buffer.
	BufferWithMemoryPtr	outBuffer;
	void*				outBufferData = nullptr;
	{
		const auto	outBufferSize	= static_cast<VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * extent.width * extent.height);
		const auto	outBufferUsage	= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const auto	outBufferInfo	= makeBufferCreateInfo(outBufferSize, outBufferUsage);

		outBuffer					= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, outBufferInfo, MemoryRequirement::HostVisible));
		outBufferData				= outBuffer->getAllocation().getHostPtr();
	}

	copyImageToBuffer(vkd, cmdBuffer, colorBuffer->get(), outBuffer->get(), iExtent2D);
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Generate reference image and compare.
	{
		auto&						log				= m_context.getTestContext().getLog();
		auto&						outBufferAlloc	= outBuffer->getAllocation();
		tcu::ConstPixelBufferAccess	result			(tcuFormat, iExtent3D, outBufferData);
		tcu::TextureLevel			referenceLevel	(tcuFormat, iExtent3D.x(), iExtent3D.y());
		const auto					reference		= referenceLevel.getAccess();
		const auto					setName			= de::toString(m_params.drawType) + "_draw_count_" + de::toString(m_params.drawCount) + (m_params.useTask ? "_with_task" : "_no_task");
		const auto					fHeight			= static_cast<float>(extent.height);
		const auto					fWidth			= static_cast<float>(extent.width);

		invalidateAlloc(vkd, device, outBufferAlloc);

		for (int y = 0; y < iExtent3D.y(); ++y)
		for (int x = 0; x < iExtent3D.x(); ++x)
		{
			const tcu::Vec4 refColor	= ((m_params.drawCount == 0u || (m_params.drawType == DrawType::DRAW && y >= static_cast<int>(m_params.drawCount)))
										? clearColor
										: tcu::Vec4(
											// These match the per-primitive color set by the mesh shader.
											(static_cast<float>(y) + 0.5f) / fHeight,
											(static_cast<float>(x) + 0.5f) / fWidth,
											0.0f,
											1.0f));
			reference.setPixel(refColor, x, y);
		}

		if (!tcu::floatThresholdCompare(log, setName.c_str(), "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
			return tcu::TestStatus::fail("Image comparison failed; check log for details");
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createMeshShaderApiTests (tcu::TestContext& testCtx)
{
	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "api", "Mesh Shader API tests"));

	const DrawType drawCases[] =
	{
		DrawType::DRAW,
		DrawType::DRAW_INDIRECT,
		DrawType::DRAW_INDIRECT_COUNT,
	};

	const auto		extent				= getExtent();
	const uint32_t	drawCountCases[]	= { 0u, 1u, 2u, extent.height / 2u, extent.height };

	const uint32_t normalStride	= static_cast<uint32_t>(sizeof(VkDrawMeshTasksIndirectCommandNV));
	const uint32_t largeStride	= 2u * normalStride + 4u;
	const uint32_t altOffset	= 20u;

	const struct
	{
		tcu::Maybe<IndirectArgs>	indirectArgs;
		const char*					name;
	} indirectArgsCases[] =
	{
		{ tcu::nothing<IndirectArgs>(),							"no_indirect_args"			},

		// Offset 0, varying strides.
		{ tcu::just(IndirectArgs{ 0u, 0u }),					"offset_0_stride_0"			},
		{ tcu::just(IndirectArgs{ 0u, normalStride }),			"offset_0_stride_normal"	},
		{ tcu::just(IndirectArgs{ 0u, largeStride }),			"offset_0_stride_large"		},

		// Nonzero offset, varying strides.
		{ tcu::just(IndirectArgs{ altOffset, 0u }),				"offset_alt_stride_0"		},
		{ tcu::just(IndirectArgs{ altOffset, normalStride }),	"offset_alt_stride_normal"	},
		{ tcu::just(IndirectArgs{ altOffset, largeStride }),	"offset_alt_stride_large"	},
	};

	const struct
	{
		tcu::Maybe<IndirectCountLimitType>	limitType;
		const char*							name;
	} countLimitCases[] =
	{
		{ tcu::nothing<IndirectCountLimitType>(),			"no_count_limit"		},
		{ tcu::just(IndirectCountLimitType::BUFFER_VALUE),	"count_limit_buffer"	},
		{ tcu::just(IndirectCountLimitType::MAX_COUNT),		"count_limit_max_count"	},
	};

	const struct
	{
		tcu::Maybe<uint32_t>	countOffset;
		const char*				name;
	} countOffsetCases[] =
	{
		{ tcu::nothing<uint32_t>(),	"no_count_offset"	},
		{ tcu::just(uint32_t{0u}),	"count_offset_0"	},
		{ tcu::just(altOffset),		"count_offset_alt"	},
	};

	const struct
	{
		bool		useTask;
		const char*	name;
	} taskCases[] =
	{
		{ false,	"no_task_shader"	},
		{ true,		"with_task_shader"	},
	};

	const struct
	{
		uint32_t	firstTask;
		const char*	name;
	} firstTaskCases[] =
	{
		{ 0u,		"first_task_zero"		},
		{ 1001u,	"first_task_nonzero"	},
	};

	uint32_t seed = 1628678795u;

	for (const auto& drawCase : drawCases)
	{
		const auto drawCaseName			= de::toString(drawCase);
		const bool isIndirect			= (drawCase != DrawType::DRAW);
		const bool isIndirectNoCount	= (drawCase == DrawType::DRAW_INDIRECT);
		const bool isIndirectCount		= (drawCase == DrawType::DRAW_INDIRECT_COUNT);

		GroupPtr drawGroup(new tcu::TestCaseGroup(testCtx, drawCaseName.c_str(), ""));

		for (const auto& drawCountCase : drawCountCases)
		{
			const auto drawCountName = "draw_count_" + de::toString(drawCountCase);
			GroupPtr drawCountGroup(new tcu::TestCaseGroup(testCtx, drawCountName.c_str(), ""));

			for (const auto& indirectArgsCase : indirectArgsCases)
			{
				const bool hasIndirectArgs	= static_cast<bool>(indirectArgsCase.indirectArgs);
				const bool strideZero		= (hasIndirectArgs && indirectArgsCase.indirectArgs.get().stride == 0u);

				if (isIndirect != hasIndirectArgs)
					continue;

				// VUID-vkCmdDrawMeshTasksIndirectNV-drawCount-02146 and VUID-vkCmdDrawMeshTasksIndirectCountNV-stride-02182.
				if (((isIndirectNoCount && drawCountCase > 1u) || isIndirectCount) && strideZero)
					continue;

				GroupPtr indirectArgsGroup(new tcu::TestCaseGroup(testCtx, indirectArgsCase.name, ""));

				for (const auto& countLimitCase : countLimitCases)
				{
					const bool hasCountLimit = static_cast<bool>(countLimitCase.limitType);

					if (isIndirectCount != hasCountLimit)
						continue;

					GroupPtr countLimitGroup(new tcu::TestCaseGroup(testCtx, countLimitCase.name, ""));

					for (const auto& countOffsetCase : countOffsetCases)
					{
						const bool hasCountOffsetType = static_cast<bool>(countOffsetCase.countOffset);

						if (isIndirectCount != hasCountOffsetType)
							continue;

						GroupPtr countOffsetGroup(new tcu::TestCaseGroup(testCtx, countOffsetCase.name, ""));

						for (const auto& taskCase : taskCases)
						{
							GroupPtr taskCaseGrp(new tcu::TestCaseGroup(testCtx, taskCase.name, ""));

							for (const auto& firstTaskCase : firstTaskCases)
							{
								const TestParams params =
								{
									drawCase,						//	DrawType							drawType;
									seed++,							//	uint32_t							seed;
									drawCountCase,					//	uint32_t							drawCount;
									firstTaskCase.firstTask,		//	uint32_t							firstTask;
									indirectArgsCase.indirectArgs,	//	tcu::Maybe<IndirectArgs>			indirectArgs;
									countLimitCase.limitType,		//	tcu::Maybe<IndirectCountLimitType>	indirectCountLimit;
									countOffsetCase.countOffset,	//	tcu::Maybe<uint32_t>				indirectCountOffset;
									taskCase.useTask,				//	bool								useTask;
								};

								taskCaseGrp->addChild(new MeshApiCase(testCtx, firstTaskCase.name, "", params));
							}

							countOffsetGroup->addChild(taskCaseGrp.release());
						}

						countLimitGroup->addChild(countOffsetGroup.release());
					}

					indirectArgsGroup->addChild(countLimitGroup.release());
				}

				drawCountGroup->addChild(indirectArgsGroup.release());
			}

			drawGroup->addChild(drawCountGroup.release());
		}

		mainGroup->addChild(drawGroup.release());
	}

	return mainGroup.release();
}

} // MeshShader
} // vkt
