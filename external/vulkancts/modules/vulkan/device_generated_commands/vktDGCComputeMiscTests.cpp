/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Device Generated Commands Compute Misc Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeMiscTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include <numeric>
#include <sstream>
#include <vector>
#include <limits>
#include <string>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

constexpr uint32_t kManyexecutesLocalInvocations = 64u;

struct ManyExecutesParams
{
	uint32_t executeCount;   // Amount of executes to run.
	bool     secondaries;    // Use secondary command buffer.
	bool     computeQueue;   // Use the compute queue.
};

void manyExecutesCheckSupport (Context& context, ManyExecutesParams)
{
	checkDGCComputeSupport(context, false, false);
}

void fullReplayCheckSupport (Context& context)
{
	checkDGCComputeSupport(context, true, true);
}

// The idea here is that each command sequence will set the push constant to select an index and launch a single workgroup, which
// will increase the buffer value by 1 in each invocation, so every output buffer value ends up being kLocalInvocations.
void manyExecutesInitPrograms (SourceCollections& dst, ManyExecutesParams)
{
	std::ostringstream comp;
	comp
		<< "#version 460\n"
		<< "layout (local_size_x=" << kManyexecutesLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
		<< "layout (set=0, binding=0, std430) buffer OutputBlock { uint values[]; } outputBuffer;\n"
		<< "layout (push_constant, std430) uniform PushConstantBlock { uint valueIndex; } pc;\n"
		<< "void main (void) { atomicAdd(outputBuffer.values[pc.valueIndex], 1u); }\n"
		;
	dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

// Idea: perform two runs: one with the normal pipeline and the second one with a replayed address. Verify both runs succeed.
// Each run will write to a different value index.
void fullReplayInitPrograms (SourceCollections& dst)
{
	std::ostringstream comp;
	comp
		<< "#version 460\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (set=0, binding=0, std430) buffer OutputBlock { uint values[]; } outputBuffer;\n"
		<< "layout (push_constant, std430) uniform PushConstantBlock { uint valueIndex; } pc;\n"
		<< "void main (void) { atomicAdd(outputBuffer.values[pc.valueIndex], 1u); }\n"
		;
	dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus manyExecutesRun (Context& context, ManyExecutesParams params)
{
	const auto& ctx = context.getContextCommonData();
	const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	const auto qfIndex = (params.computeQueue ? context.getComputeQueueFamilyIndex() : ctx.qfIndex);
	const auto queue = (params.computeQueue ? context.getComputeQueue() : ctx.queue);

	// Output buffer.
	const auto valueSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
	const auto outputBufferSize = params.executeCount * valueSize;
	const auto outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory outputBuffer (ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& outputBufferAlloc = outputBuffer.getAllocation();
	void* outputBufferData = outputBufferAlloc.getHostPtr();

	deMemset(outputBufferData, 0, static_cast<size_t>(outputBufferSize));
	flushAlloc(ctx.vkd, ctx.device, outputBufferAlloc);

	// Descriptor set layout, pool and set preparation.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(descType, stageFlags);
	const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(descType);
	const auto descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

	DescriptorSetUpdateBuilder setUpdateBuilder;
	const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
	setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &outputBufferDescInfo);
	setUpdateBuilder.update(ctx.vkd, ctx.device);

	// Push constants
	const auto pcSize = static_cast<uint32_t>(sizeof(uint32_t));
	const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

	// Shader.
	const auto& binaries = context.getBinaryCollection();
	const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Pipeline.
	const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

	// Generated commands layout: push constant and dispatch.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	cmdsLayoutBuilder.addPushConstantToken(0u, 0u, *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, pcSize);
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	// Increase the value index (indicated by the push constant) in each sequence, then dispatch one workgroup.
	const auto genCmdsItemCount = (4u /*push constant + dispatch arguments*/ * params.executeCount);
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(genCmdsItemCount);
	for (uint32_t i = 0u; i < params.executeCount; ++i)
	{
		genCmdsData.push_back(i);  // Push constant.
		genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
		genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
		genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z
	}

	// Generated indirect commands buffer.
	const auto genCmdsBufferSize = de::dataSize(genCmdsData);
	const auto genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	BufferWithMemory genCmdsBuffer (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
	void* genCmdsBufferData = genCmdsBufferAlloc.getHostPtr();

	deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
	flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

	// Critical for the test: instead of running all these sequences in parallel, we execute one sequence at a time.
	// For the preprocess buffer, we'll use a region of the same large buffer in each execution.

	// Preprocess buffer.
	const auto& dgcProperties       = context.getDeviceGeneratedCommandsProperties();
	const auto  preprocessAlignment = static_cast<VkDeviceSize>(dgcProperties.minIndirectCommandsBufferOffsetAlignment);

	VkMemoryRequirements    preprocessBufferReqs;
	VkDeviceSize            preprocessBufferStride;
	Move<VkBuffer>          preprocessBuffer;
	de::MovePtr<Allocation> preprocessBufferAlloc;

	{
		const auto genCmdMemReqsInfo = makeGeneratedCommandsMemoryRequirementsInfoNV(bindPoint, *pipeline, *cmdsLayout, 1u);
		preprocessBufferReqs = getGeneratedCommandsMemoryRequirementsNV(ctx.vkd, ctx.device, &genCmdMemReqsInfo);

		// Round up to the proper alignment, and multiply by the number of executions.
		preprocessBufferStride = de::roundUp(preprocessBufferReqs.size, preprocessAlignment);
		preprocessBufferReqs.size = preprocessBufferStride * params.executeCount;

		const auto preprocessBufferCreateInfo = makeBufferCreateInfo(preprocessBufferReqs.size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		preprocessBuffer = createBuffer(ctx.vkd, ctx.device, &preprocessBufferCreateInfo);
		preprocessBufferAlloc = ctx.allocator.allocate(preprocessBufferReqs, MemoryRequirement::Any);
		VK_CHECK(ctx.vkd.bindBufferMemory(ctx.device, *preprocessBuffer, preprocessBufferAlloc->getMemory(), preprocessBufferAlloc->getOffset()));
	}

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, qfIndex);
	const auto priCmdBuffer = *cmd.cmdBuffer;

	Move<VkCommandBuffer> secCmdBuffer;
	if (params.secondaries)
		secCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	// Main command buffer contents.
	const auto mainCmdBuffer = (params.secondaries ? *secCmdBuffer : priCmdBuffer);
	beginCommandBuffer(ctx.vkd, mainCmdBuffer);

	ctx.vkd.cmdBindDescriptorSets(mainCmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	ctx.vkd.cmdBindPipeline(mainCmdBuffer, bindPoint, *pipeline);

	// Again, key for the test: run multiple executions instead of a single one.
	const auto genCmdsStride = static_cast<VkDeviceSize>(cmdsLayoutBuilder.getStreamRange(0u));
	for (uint32_t i = 0u; i < params.executeCount; ++i)
	{
		// Specify a per-execution offset in the commands stream.
		const auto genCmdsBufferOffset = genCmdsStride * i;
		const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, genCmdsBufferOffset);
		const auto preprocessOffset = (preprocessBufferStride * i);
		const VkGeneratedCommandsInfoNV cmdsInfo =
		{
			VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,	//	VkStructureType						sType;
			nullptr,										//	const void*							pNext;
			bindPoint,										//	VkPipelineBindPoint					pipelineBindPoint;
			*pipeline,										//	VkPipeline							pipeline;
			*cmdsLayout,									//	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
			1u,												//	uint32_t							streamCount;
			&streamInfo,									//	const VkIndirectCommandsStreamNV*	pStreams;
			1u,												//	uint32_t							sequencesCount;
			*preprocessBuffer,								//	VkBuffer							preprocessBuffer;
			preprocessOffset,								//	VkDeviceSize						preprocessOffset;
			preprocessBufferStride,							//	VkDeviceSize						preprocessSize;
			VK_NULL_HANDLE,									//	VkBuffer							sequencesCountBuffer;
			0ull,											//	VkDeviceSize						sequencesCountOffset;
			VK_NULL_HANDLE,									//	VkBuffer							sequencesIndexBuffer;
			0ull,											//	VkDeviceSize						sequencesIndexOffset;
		};
		ctx.vkd.cmdExecuteGeneratedCommandsNV(mainCmdBuffer, VK_FALSE, &cmdsInfo);
	}
	{
		const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(ctx.vkd, mainCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
	}
	endCommandBuffer(ctx.vkd, mainCmdBuffer);

	if (params.secondaries)
	{
		beginCommandBuffer(ctx.vkd, priCmdBuffer);
		ctx.vkd.cmdExecuteCommands(priCmdBuffer, 1u, &secCmdBuffer.get());
		endCommandBuffer(ctx.vkd, priCmdBuffer);
	}

	submitCommandsAndWait(ctx.vkd, ctx.device, queue, priCmdBuffer);

	// Verify results.
	std::vector<uint32_t> outputValues (params.executeCount, std::numeric_limits<uint32_t>::max());
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(de::dataOrNull(outputValues), outputBufferData, de::dataSize(outputValues));

	bool fail = false;
	auto& log = context.getTestContext().getLog();

	for (uint32_t i = 0; i < params.executeCount; ++i)
	{
		const auto& result = outputValues.at(i);
		if (result != kManyexecutesLocalInvocations)
		{
			log << tcu::TestLog::Message
				<< "Error at execution " << i << ": expected " << kManyexecutesLocalInvocations << " but found " << result
				<< tcu::TestLog::EndMessage;
			fail = true;
		}
	}

	if (fail)
		return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus fullReplayRun (Context& context)
{
	const auto& ctx        = context.getContextCommonData();
	const auto  descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto  stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto  bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
	auto&       log        = context.getTestContext().getLog();
	const auto  iterCount  = 2u; // First one capturing the address and the second one replaying it.

	// Output buffer.
	std::vector<uint32_t> outputValues           (iterCount, 0u);
	const auto            outputBufferSize       = static_cast<VkDeviceSize>(de::dataSize(outputValues));
	const auto            outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory      outputBuffer           (ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&                 outputBufferAlloc      = outputBuffer.getAllocation();
	void*                 outputBufferData       = outputBufferAlloc.getHostPtr();

	deMemcpy(outputBufferData, de::dataOrNull(outputValues), de::dataSize(outputValues));
	flushAlloc(ctx.vkd, ctx.device, outputBufferAlloc);

	// Descriptor set layout, pool and set preparation.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(descType, stageFlags);
	const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(descType);
	const auto descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

	DescriptorSetUpdateBuilder setUpdateBuilder;
	const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
	setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &outputBufferDescInfo);
	setUpdateBuilder.update(ctx.vkd, ctx.device);

	// Push constants
	const auto u32Size = static_cast<uint32_t>(sizeof(uint32_t));
	const auto pcSize  = u32Size;
	const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

	// Shader.
	const auto& binaries   = context.getBinaryCollection();
	const auto  compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Indirect commands layout.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	cmdsLayoutBuilder.addPipelineToken(0u, 0u);
	cmdsLayoutBuilder.addPushConstantToken(0u, cmdsLayoutBuilder.getStreamRange(0u), *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, cmdsLayoutBuilder.getStreamRange(0u));
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// This one will store the captured address for the second iteration.
	VkDeviceAddress capturedAddress = 0ull;

	for (uint32_t iterIdx = 0u; iterIdx < iterCount; ++iterIdx)
	{
		// Prepare the pipeline.
		DGCComputePipelineMetaDataPool metadataPool (DGCComputePipelineMetaDataPool::kDefaultMultiplier, true); // Enable capture/replay.
		DGCComputePipeline pipeline (metadataPool, ctx.vkd, ctx.device, ctx.allocator, 0u, *pipelineLayout, 0u, *compModule, nullptr, capturedAddress);
		const auto pipelineAddress = pipeline.getIndirectDeviceAddress();

		if (capturedAddress != 0ull)
		{
			if (capturedAddress != pipelineAddress)
				TCU_FAIL("Captured address and replayed address do not match");
		}
		capturedAddress = pipelineAddress;

		// Generated indirect commands buffer contents.
		std::vector<uint32_t> genCmdsData;
		genCmdsData.reserve(6u /*pipeline address + push constant + dispatch command*/);
		pushBackDeviceAddress(genCmdsData, pipelineAddress);  // Pipeline address (2 uints).
		genCmdsData.push_back(iterIdx);                       // Push constant: the index to the output buffer.
		genCmdsData.push_back(1u);                            // VkDispatchIndirectCommand::x
		genCmdsData.push_back(1u);                            // VkDispatchIndirectCommand::y
		genCmdsData.push_back(1u);                            // VkDispatchIndirectCommand::z

		// Generated indirect commands buffer.
		const auto       genCmdsBufferSize       = de::dataSize(genCmdsData);
		const auto       genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		BufferWithMemory genCmdsBuffer           (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
		auto&            genCmdsBufferAlloc      = genCmdsBuffer.getAllocation();
		void*            genCmdsBufferData       = genCmdsBufferAlloc.getHostPtr();

		deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
		flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

		// Preprocess buffer for 1 sequence.
		PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, VK_NULL_HANDLE, *cmdsLayout, 1u);

		// Command pool and buffer.
		CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
		const auto cmdBuffer = *cmd.cmdBuffer;

		beginCommandBuffer(ctx.vkd, cmdBuffer);

		ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		ctx.vkd.cmdUpdatePipelineIndirectBufferNV(cmdBuffer, bindPoint, *pipeline);
		metadataUpdateToPreprocessBarrier(ctx.vkd, cmdBuffer);
		{
			const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, 0ull);
			const VkGeneratedCommandsInfoNV cmdsInfo =
			{
				VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,	//	VkStructureType						sType;
				nullptr,										//	const void*							pNext;
				bindPoint,										//	VkPipelineBindPoint					pipelineBindPoint;
				VK_NULL_HANDLE,									//	VkPipeline							pipeline;
				*cmdsLayout,									//	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
				1u,												//	uint32_t							streamCount;
				&streamInfo,									//	const VkIndirectCommandsStreamNV*	pStreams;
				1u,												//	uint32_t							sequencesCount;
				*preprocessBuffer,								//	VkBuffer							preprocessBuffer;
				0ull,											//	VkDeviceSize						preprocessOffset;
				preprocessBuffer.getSize(),						//	VkDeviceSize						preprocessSize;
				VK_NULL_HANDLE,									//	VkBuffer							sequencesCountBuffer;
				0ull,											//	VkDeviceSize						sequencesCountOffset;
				VK_NULL_HANDLE,									//	VkBuffer							sequencesIndexBuffer;
				0ull,											//	VkDeviceSize						sequencesIndexOffset;
			};
			ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, VK_FALSE, &cmdsInfo);
		}
		{
			const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
			cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
		}
		endCommandBuffer(ctx.vkd, cmdBuffer);
		submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
	}

	// Verify results.
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(de::dataOrNull(outputValues), outputBufferData, de::dataSize(outputValues));

	const auto reference = 1u;
	bool       fail      = false;

	for (size_t i = 0u; i < outputValues.size(); ++i)
	{
		const auto result = outputValues.at(i);
		if (reference != result)
		{
			std::ostringstream msg;
			msg << "Unexpected value found in output buffer position " << i << ": expected " << reference << " but found " << result;
			log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
			fail = true;
		}
	}

	if (fail)
		return tcu::TestStatus::fail("Unexpected data found in output buffer; check log for details");
	return tcu::TestStatus::pass("Pass");
}

class ScratchSpaceInstance : public vkt::TestInstance
{
public:
						ScratchSpaceInstance	(Context& context) : vkt::TestInstance(context) {}
	virtual				~ScratchSpaceInstance	(void) {}

	tcu::TestStatus		iterate					(void) override;
};

class ScratchSpaceCase : public vkt::TestCase
{
public:
					ScratchSpaceCase	(tcu::TestContext& testCtx, const std::string& name) : vkt::TestCase(testCtx, name) {}
	virtual			~ScratchSpaceCase	(void) {}

	void			initPrograms		(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance		(Context& context) const override;
	void			checkSupport		(Context& context) const override;

};

void ScratchSpaceCase::checkSupport (Context& context) const
{
	checkDGCComputeSupport(context, true, false);
	context.getComputeQueue(); // Throws NotSupportedError if not available.
}

// The goal of this large shader is to make sure some scratch space is needed due to register spilling, and that this scratch space
// is allocated correctly. Register spilling is attempted to be guaranteed due to the amount of combinations from input vars and the
// non-uniform control flow from the shader used below.
void ScratchSpaceCase::initPrograms (SourceCollections& dst) const
{
	const std::string code = ShaderSourceProvider::getSource(m_testCtx.getArchive(), "vulkan/device_generated_commands/ScratchSpace.comp.spvasm");
	dst.spirvAsmSources.add("comp") << code;
}

TestInstance* ScratchSpaceCase::createInstance (Context& context) const
{
	return new ScratchSpaceInstance(context);
}

constexpr int32_t kScratchSpaceLocalInvocations = 4; // Must match ScratchSpace.comp

tcu::TestStatus ScratchSpaceInstance::iterate ()
{
	// Must match ScratchSpace.comp: these were obtained in practice by converting the shader to C.
	static const std::vector<int32_t> expectedOutputs
	{
		-256,
		-46,
		-327,
		-722,
	};

	const auto& ctx         = m_context.getContextCommonData();
	const auto  bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const auto  descType    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto  stageFlags  = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto  bindPoint   = VK_PIPELINE_BIND_POINT_COMPUTE;

	// Output buffer.
	std::vector<int32_t> outputValues (kScratchSpaceLocalInvocations, 0);
	const auto       outputBufferSize       = static_cast<VkDeviceSize>(de::dataSize(outputValues));
	const auto       outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, bufferUsage);
	BufferWithMemory outputBuffer           (ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&            outputBufferAlloc      = outputBuffer.getAllocation();
	void*            outputBufferData       = outputBufferAlloc.getHostPtr();

	deMemcpy(outputBufferData, de::dataOrNull(outputValues), de::dataSize(outputValues));
	flushAlloc(ctx.vkd, ctx.device, outputBufferAlloc);

	// Input buffer.
	std::vector<int32_t> inputValues;
	inputValues.resize(kScratchSpaceLocalInvocations);
	std::iota(begin(inputValues), end(inputValues), 0);

	const auto       inputBufferSize       = static_cast<VkDeviceSize>(de::dataSize(inputValues));
	const auto       inputBufferCreateInfo = makeBufferCreateInfo(inputBufferSize, bufferUsage);
	BufferWithMemory inputBuffer           (ctx.vkd, ctx.device, ctx.allocator, inputBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&            inputBufferAlloc      = inputBuffer.getAllocation();
	void*            inputBufferData       = inputBufferAlloc.getHostPtr();

	deMemcpy(inputBufferData, de::dataOrNull(inputValues), de::dataSize(inputValues));
	flushAlloc(ctx.vkd, ctx.device, inputBufferAlloc);

	// Descriptor set layout, pool and set preparation.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(descType, stageFlags);
	setLayoutBuilder.addSingleBinding(descType, stageFlags);
	const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(descType, 2u/*input and output buffers*/);
	const auto descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

	DescriptorSetUpdateBuilder setUpdateBuilder;
	const auto inputBufferDescInfo  = makeDescriptorBufferInfo(*inputBuffer, 0ull, inputBufferSize);
	const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
	setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &inputBufferDescInfo);
	setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), descType, &outputBufferDescInfo);
	setUpdateBuilder.update(ctx.vkd, ctx.device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

	// Shader.
	const auto& binaries   = m_context.getBinaryCollection();
	const auto  compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// DGC Pipeline.
	DGCComputePipelineMetaDataPool metadataPool;
	const DGCComputePipeline dgcPipeline (metadataPool, ctx.vkd, ctx.device, ctx.allocator, 0u, *pipelineLayout, 0u, *compModule);

	// Uncomment this to verify the shader properties if needed.
#if 0
	{
		auto& log = m_context.getTestContext().getLog();
		VkPipelineInfoKHR pipelineInfo = initVulkanStructure();
		pipelineInfo.pipeline = *dgcPipeline;
		uint32_t executableCount = 0u;
		VK_CHECK(ctx.vkd.getPipelineExecutablePropertiesKHR(ctx.device, &pipelineInfo, &executableCount, nullptr));

		for (uint32_t i = 0; i < executableCount; ++i)
		{
			VkPipelineExecutableInfoKHR executableInfo = initVulkanStructure();
			executableInfo.pipeline = *dgcPipeline;
			executableInfo.executableIndex = i;
			uint32_t statsCount = 0u;
			VK_CHECK(ctx.vkd.getPipelineExecutableStatisticsKHR(ctx.device, &executableInfo, &statsCount, nullptr));
			if (statsCount == 0u)
				continue;
			std::vector<VkPipelineExecutableStatisticKHR> stats (statsCount);
			VK_CHECK(ctx.vkd.getPipelineExecutableStatisticsKHR(ctx.device, &executableInfo, &statsCount, de::dataOrNull(stats)));
			std::string valueStr;
			for (uint32_t j = 0u; j < statsCount; ++j)
			{
				const auto& stat = stats.at(j);
				if (stat.format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR)
					valueStr = std::to_string(stat.value.b32);
				else if (stat.format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR)
					valueStr = std::to_string(stat.value.i64);
				else if (stat.format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR)
					valueStr = std::to_string(stat.value.u64);
				else if (stat.format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR)
					valueStr = std::to_string(stat.value.f64);
				else
					DE_ASSERT(false);
				log << tcu::TestLog::Message << stat.name << " (" << stat.description << "): " << valueStr << tcu::TestLog::EndMessage;
			}
		}
	}
#endif

	// Indirect commands layout: pipeline token followed by dispatch.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	cmdsLayoutBuilder.addPipelineToken(0u, 0u);
	cmdsLayoutBuilder.addDispatchToken(0u, cmdsLayoutBuilder.getStreamRange(0u));
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(5u /*pipeline bind + dispatch*/);
	pushBackDeviceAddress(genCmdsData, dgcPipeline.getIndirectDeviceAddress());
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z

	// Generated indirect commands buffer.
	const auto       genCmdsBufferSize       = de::dataSize(genCmdsData);
	const auto       genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	BufferWithMemory genCmdsBuffer           (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&            genCmdsBufferAlloc      = genCmdsBuffer.getAllocation();
	void*            genCmdsBufferData       = genCmdsBufferAlloc.getHostPtr();

	deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
	flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

	// Preprocess buffer for 1 sequence.
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, VK_NULL_HANDLE, *cmdsLayout, 1u);

	// We will update the pipeline metadata buffer on the universal queue, and submit the dispatch to the compute queue.
	{
		CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
		const auto cmdBuffer = *cmd.cmdBuffer;

		beginCommandBuffer(ctx.vkd, cmdBuffer);
		ctx.vkd.cmdUpdatePipelineIndirectBufferNV(cmdBuffer, bindPoint, *dgcPipeline);
		metadataUpdateToPreprocessBarrier(ctx.vkd, cmdBuffer);
		endCommandBuffer(ctx.vkd, cmdBuffer);
		submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
		ctx.vkd.deviceWaitIdle(ctx.device); // Why not?
	}
	{
		CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, m_context.getComputeQueueFamilyIndex());
		const auto cmdBuffer = *cmd.cmdBuffer;

		beginCommandBuffer(ctx.vkd, cmdBuffer);
		ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		{
			const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, 0ull);
			const VkGeneratedCommandsInfoNV cmdsInfo =
			{
				VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,	//	VkStructureType						sType;
				nullptr,										//	const void*							pNext;
				bindPoint,										//	VkPipelineBindPoint					pipelineBindPoint;
				VK_NULL_HANDLE,									//	VkPipeline							pipeline;
				*cmdsLayout,									//	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
				1u,												//	uint32_t							streamCount;
				&streamInfo,									//	const VkIndirectCommandsStreamNV*	pStreams;
				1u,												//	uint32_t							sequencesCount;
				*preprocessBuffer,								//	VkBuffer							preprocessBuffer;
				0ull,											//	VkDeviceSize						preprocessOffset;
				preprocessBuffer.getSize(),						//	VkDeviceSize						preprocessSize;
				VK_NULL_HANDLE,									//	VkBuffer							sequencesCountBuffer;
				0ull,											//	VkDeviceSize						sequencesCountOffset;
				VK_NULL_HANDLE,									//	VkBuffer							sequencesIndexBuffer;
				0ull,											//	VkDeviceSize						sequencesIndexOffset;
			};
			ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, VK_FALSE, &cmdsInfo);
		}
		{
			const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
			cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
		}
		endCommandBuffer(ctx.vkd, cmdBuffer);
		submitCommandsAndWait(ctx.vkd, ctx.device, m_context.getComputeQueue(), cmdBuffer);
	}

	// Verify results.
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(de::dataOrNull(outputValues), outputBufferData, de::dataSize(outputValues));

	auto& log = m_context.getTestContext().getLog();
	bool fail = false;

	DE_ASSERT(expectedOutputs.size() == outputValues.size());
	for (size_t i = 0u; i < outputValues.size(); ++i)
	{
		const auto reference = expectedOutputs.at(i);
		const auto result    = outputValues.at(i);

		if (result != reference)
		{
			std::ostringstream msg;
			msg << "Unexpected value found in output buffer at position " << i << ": expected " << reference << " but found " << result;
			log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
			fail = true;
		}
	}

	if (fail)
		return tcu::TestStatus::fail("Unexpected values in output buffer; check log for details");
	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createDGCComputeMiscTests (tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "misc"));

	for (const auto executeCount : { 64u, 1024u, 8192u })
		for (const auto useSecondaries : { false, true })
			for (const auto useComputeQueue: { false, true })
			{
				const ManyExecutesParams params {executeCount, useSecondaries, useComputeQueue};
				const auto typeVariant = (useSecondaries ? "_secondary_cmd" : "_primary_cmd");
				const auto queueVariant = (useComputeQueue ? "_compute_queue" : "_universal_queue");
				const auto testName = std::string("execute_many_") + std::to_string(executeCount) + typeVariant + queueVariant;
				addFunctionCaseWithPrograms(mainGroup.get(), testName, manyExecutesCheckSupport, manyExecutesInitPrograms, manyExecutesRun, params);
			}

	addFunctionCaseWithPrograms(mainGroup.get(), "full_replay", fullReplayCheckSupport, fullReplayInitPrograms, fullReplayRun);

	mainGroup->addChild(new ScratchSpaceCase(testCtx, "scratch_space"));

	return mainGroup.release();
}

} // DGC
} // vkt
