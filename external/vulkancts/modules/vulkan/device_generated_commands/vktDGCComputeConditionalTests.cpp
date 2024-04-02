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
 * \brief Device Generated Commands Conditional Rendering Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeConditionalTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vktDGCUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include <vector>
#include <memory>
#include <sstream>
#include <string>
#include <limits>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

using DGCComputePipelinePtr = std::unique_ptr<DGCComputePipeline>;
using BufferWithMemoryPtr   = std::unique_ptr<BufferWithMemory>;

enum class UseSecondaries
{
	NO = 0,
	YES_WITHOUT_INHERITANCE,
	YES_WITH_INHERITANCE,
};

struct TestParams
{
	bool           pipelineToken;        // Use a DGC indirect pipeline.
	bool           indirectCountBuffer;  // Use an indirect count buffer.
	bool           conditionValue;       // Value for the condition buffer.
	bool           inverted;             // Inverted condition?
	UseSecondaries useSecondaries;       // Use secondaries? How?
	bool           computeQueue;         // Use the compute queue instead of the universal one.
};

struct ConditionalPreprocessParams
{
	bool conditionValue;
	bool inverted;
	bool executeOnCompute;
};

inline void checkConditionalRenderingExt (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
}

void checkConditionalDGCComputeSupport (Context& context, TestParams params)
{
	checkDGCComputeSupport(context, params.pipelineToken, false);
	checkConditionalRenderingExt(context);

	if (params.useSecondaries == UseSecondaries::YES_WITH_INHERITANCE)
	{
		const auto& features = context.getConditionalRenderingFeaturesEXT();
		if (!features.inheritedConditionalRendering)
			TCU_THROW(NotSupportedError, "inheritedConditionalRendering not supported");
	}

	if (params.computeQueue)
		context.getComputeQueue(); // Will throw NotSupportedError if not available.
}

void checkConditionalPreprocessSupport (Context& context, ConditionalPreprocessParams params)
{
	checkDGCComputeSupport(context, false, false);
	checkConditionalRenderingExt(context);

	if (params.executeOnCompute)
		context.getComputeQueue(); // Will throw NotSupportedError if not available.
}

// Store the push constant value in the output buffer.
void storePushConstantProgram (SourceCollections& dst)
{
	std::ostringstream comp;
	comp
		<< "#version 460\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (set=0, binding=0, std430) buffer OutputBlock { uint value; } outputBuffer;\n"
		<< "layout (push_constant, std430) uniform PushConstantBlock { uint value; } pc;\n"
		<< "void main (void) { outputBuffer.value = pc.value; }\n"
		;
	dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

void storePushConstantProgramParams (SourceCollections& dst, TestParams)
{
	storePushConstantProgram(dst);
}

void storePushConstantProgramPreprocessParams (SourceCollections& dst, ConditionalPreprocessParams)
{
	storePushConstantProgram(dst);
}

void shaderToHostBarrier (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer)
{
	const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
}

void beginConditionalRendering (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer, VkBuffer conditionBuffer, bool inverted)
{
	uint32_t flags = 0u;
	if (inverted)
		flags |= VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT;

	const VkConditionalRenderingBeginInfoEXT beginInfo =
	{
		VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,	//	VkStructureType					sType;
		nullptr,												//	const void*						pNext;
		conditionBuffer,										//	VkBuffer						buffer;
		0ull,													//	VkDeviceSize					offset;
		flags,													//	VkConditionalRenderingFlagsEXT	flags;
	};
	vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &beginInfo);
}

// Binds the normal pipeline or updates the indirect buffer for the DGC pipeline and sets the proper barrier.
void bindOrPreparePipeline (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint, VkPipeline normalPipeline, const DGCComputePipeline* dgcPipeline)
{
	if (dgcPipeline)
	{
		vkd.cmdUpdatePipelineIndirectBufferNV(cmdBuffer, bindPoint, dgcPipeline->get());
		metadataUpdateToPreprocessBarrier(vkd, cmdBuffer);
	}
	else
	{
		DE_ASSERT(normalPipeline != VK_NULL_HANDLE);
		vkd.cmdBindPipeline(cmdBuffer, bindPoint, normalPipeline);
	}
}

tcu::TestStatus conditionalDispatchRun (Context& context, TestParams params)
{
	const auto& ctx        = context.getContextCommonData();
	const auto  descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto  stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto  bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
	const auto  qfIndex    = (params.computeQueue ? context.getComputeQueueFamilyIndex() : ctx.qfIndex);
	const auto  queue      = (params.computeQueue ? context.getComputeQueue()            : ctx.queue);

	// Output buffer.
	const auto       outputBufferSize       = static_cast<VkDeviceSize>(sizeof(uint32_t));
	const auto       outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory outputBuffer           (ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&            outputBufferAlloc      = outputBuffer.getAllocation();
	void*            outputBufferData       = outputBufferAlloc.getHostPtr();

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
	const auto u32Size = static_cast<uint32_t>(sizeof(uint32_t));
	const auto pcValue = 777u; // Arbitrary.
	const auto pcSize  = u32Size;
	const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

	// Shader.
	const auto& binaries   = context.getBinaryCollection();
	const auto  compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Pipeline, multiple options.
	DGCComputePipelineMetaDataPool metadataPool;
	DGCComputePipelinePtr          dgcPipeline;
	Move<VkPipeline>               normalPipeline;

	if (params.pipelineToken)
		dgcPipeline.reset(new DGCComputePipeline(metadataPool, ctx.vkd, ctx.device, ctx.allocator, 0u, *pipelineLayout, stageFlags, *compModule));
	else
		normalPipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

	// Indirect commands layout. Push constant followed by dispatch, optionally preceded by a pipeline bind.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	if (params.pipelineToken)
		cmdsLayoutBuilder.addPipelineToken(0u, 0u);
	cmdsLayoutBuilder.addPushConstantToken(0u, cmdsLayoutBuilder.getStreamRange(0u), *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, cmdsLayoutBuilder.getStreamRange(0u));
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(6u /*pipeline address, push constant and dispatch*/);
	if (params.pipelineToken)
		pushBackDeviceAddress(genCmdsData, dgcPipeline->getIndirectDeviceAddress());
	genCmdsData.push_back(pcValue);
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

	// Conditional rendering buffer.
	const auto       conditionBufferSize  = u32Size;
	const auto       conditionBufferInfo  = makeBufferCreateInfo(conditionBufferSize, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
	BufferWithMemory conditionBuffer      (ctx.vkd, ctx.device, ctx.allocator, conditionBufferInfo, MemoryRequirement::HostVisible);
	const uint32_t   conditionBufferValue = (params.conditionValue ? 2u : 0u); // Avoid using value 1, just to make it interesting.
	auto&            conditionBufferAlloc = conditionBuffer.getAllocation();
	void*            conditionBufferData  = conditionBufferAlloc.getHostPtr();

	deMemcpy(conditionBufferData, &conditionBufferValue, sizeof(conditionBufferValue));
	flushAlloc(ctx.vkd, ctx.device, conditionBufferAlloc);

	// Preprocess buffer for 256 sequences (actually only using one, but we'll pretend we may use more).
	// Note the minimum property requirements are large enough so that 256 sequences should fit.
	// Also note normalPipeline will be VK_NULL_HANDLE for the pipeline token case, which is exactly what we want.
	const auto potentialSequenceCount = 256u;
	const auto actualSequenceCount    = 1u;
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *normalPipeline, *cmdsLayout, potentialSequenceCount);

	// (Optional) Sequence count buffer.
	BufferWithMemoryPtr sequenceCountBuffer;
	if (params.indirectCountBuffer)
	{
		const auto bufferSize = u32Size;
		const auto createInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		sequenceCountBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::HostVisible));

		auto& allocation = sequenceCountBuffer->getAllocation();
		void* dataptr    = allocation.getHostPtr();

		deMemcpy(dataptr, &actualSequenceCount, sizeof(actualSequenceCount));
		flushAlloc(ctx.vkd, ctx.device, allocation);
	}

	// Generated commands info.
	const auto infoSequencesCount = (params.indirectCountBuffer ? potentialSequenceCount : actualSequenceCount);
	const auto infoCountBuffer    = (params.indirectCountBuffer ? sequenceCountBuffer->get() : VK_NULL_HANDLE);
	const auto streamInfo         = makeIndirectCommandsStreamNV(*genCmdsBuffer, 0ull);

	const VkGeneratedCommandsInfoNV cmdsInfo =
	{
		VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,	//	VkStructureType						sType;
		nullptr,										//	const void*							pNext;
		bindPoint,										//	VkPipelineBindPoint					pipelineBindPoint;
		*normalPipeline,								//	VkPipeline							pipeline;
		*cmdsLayout,									//	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
		1u,												//	uint32_t							streamCount;
		&streamInfo,									//	const VkIndirectCommandsStreamNV*	pStreams;
		infoSequencesCount,								//	uint32_t							sequencesCount;
		*preprocessBuffer,								//	VkBuffer							preprocessBuffer;
		0ull,											//	VkDeviceSize						preprocessOffset;
		preprocessBuffer.getSize(),						//	VkDeviceSize						preprocessSize;
		infoCountBuffer,								//	VkBuffer							sequencesCountBuffer;
		0ull,											//	VkDeviceSize						sequencesCountOffset;
		VK_NULL_HANDLE,									//	VkBuffer							sequencesIndexBuffer;
		0ull,											//	VkDeviceSize						sequencesIndexOffset;
	};

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;
	const auto secondary = ((params.useSecondaries != UseSecondaries::NO)
	                     ? allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)
	                     : Move<VkCommandBuffer>());

	if (params.useSecondaries == UseSecondaries::YES_WITH_INHERITANCE)
	{
		// Prepare secondary command buffer contents.
		const VkCommandBufferInheritanceConditionalRenderingInfoEXT icrInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT,	//	VkStructureType	sType;
			nullptr,																		//	const void*		pNext;
			VK_TRUE,																		//	VkBool32		conditionalRenderingEnable;
		};
		const auto sec = *secondary;
		beginSecondaryCommandBuffer(ctx.vkd, sec, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, &icrInfo);
		ctx.vkd.cmdBindDescriptorSets(sec, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		bindOrPreparePipeline(ctx.vkd, sec, bindPoint, *normalPipeline, dgcPipeline.get());
		ctx.vkd.cmdExecuteGeneratedCommandsNV(sec, VK_FALSE, &cmdsInfo);
		endCommandBuffer(ctx.vkd, sec);

		// In the primary, set up the conditional rendering part and execute the secondary command buffer.
		beginCommandBuffer(ctx.vkd, cmdBuffer);
		beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
		ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &sec);
		ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
		shaderToHostBarrier(ctx.vkd, cmdBuffer);
		endCommandBuffer(ctx.vkd, cmdBuffer);
	}
	else if (params.useSecondaries == UseSecondaries::YES_WITHOUT_INHERITANCE)
	{
		// Prepare secondary command buffer contents.
		const auto sec = *secondary;
		beginSecondaryCommandBuffer(ctx.vkd, sec);
		ctx.vkd.cmdBindDescriptorSets(sec, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		bindOrPreparePipeline(ctx.vkd, sec, bindPoint, *normalPipeline, dgcPipeline.get());
		beginConditionalRendering(ctx.vkd, sec, *conditionBuffer, params.inverted);
		ctx.vkd.cmdExecuteGeneratedCommandsNV(sec, VK_FALSE, &cmdsInfo);
		ctx.vkd.cmdEndConditionalRenderingEXT(sec);
		endCommandBuffer(ctx.vkd, sec);

		// In the primary, just execute the secondary command buffer.
		beginCommandBuffer(ctx.vkd, cmdBuffer);
		ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &sec);
		shaderToHostBarrier(ctx.vkd, cmdBuffer);
		endCommandBuffer(ctx.vkd, cmdBuffer);
	}
	else if (params.useSecondaries == UseSecondaries::NO)
	{
		// Everything is recorded on the primary command buffer.
		beginCommandBuffer(ctx.vkd, cmdBuffer);
		beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
		ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		bindOrPreparePipeline(ctx.vkd, cmdBuffer, bindPoint, *normalPipeline, dgcPipeline.get());
		ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, VK_FALSE, &cmdsInfo);
		ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
		shaderToHostBarrier(ctx.vkd, cmdBuffer);
		endCommandBuffer(ctx.vkd, cmdBuffer);
	}
	else
		DE_ASSERT(false);

	// Verify results.
	uint32_t outputValue = 0u;
	submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

	// Note the expected value is a logical xor of the condition value and the inverted flag.
	const auto expectedValue = ((params.conditionValue != params.inverted) ? pcValue : 0u);

	if (outputValue != expectedValue)
	{
		std::ostringstream msg;
		msg << "Unexpected value found in output buffer; expected " << expectedValue << " but found " << outputValue;
		return tcu::TestStatus::fail(msg.str());
	}
	return tcu::TestStatus::pass("Pass");
}

// Creates a buffer memory barrier structure to sync access from preprocessing to execution.
VkBufferMemoryBarrier makePreprocessToExecuteBarrier (VkBuffer buffer, VkDeviceSize size, uint32_t srcQueueIndex, uint32_t dstQueueIndex)
{
	return makeBufferMemoryBarrier(
		VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
		VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
		buffer, 0ull, size,
		srcQueueIndex, dstQueueIndex);
}

// These tests try to check conditional rendering does not affect preprocessing.
tcu::TestStatus conditionalPreprocessRun (Context& context, ConditionalPreprocessParams params)
{
	const auto& ctx        = context.getContextCommonData();
	const auto  descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto  stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto  bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
	const auto  seqCount   = 1u;

	// Output buffer.
	const auto       outputBufferSize       = static_cast<VkDeviceSize>(sizeof(uint32_t));
	const auto       outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory outputBuffer           (ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&            outputBufferAlloc      = outputBuffer.getAllocation();
	void*            outputBufferData       = outputBufferAlloc.getHostPtr();

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
	const auto u32Size = static_cast<uint32_t>(sizeof(uint32_t));
	const auto pcValue = 777u; // Arbitrary.
	const auto pcSize  = u32Size;
	const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

	// Shader.
	const auto& binaries   = context.getBinaryCollection();
	const auto  compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Pipeline, multiple options.
	const auto normalPipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

	// Indirect commands layout. Push constant followed by dispatch, optionally preceded by a pipeline bind.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_NV, bindPoint);
	cmdsLayoutBuilder.addPushConstantToken(0u, cmdsLayoutBuilder.getStreamRange(0u), *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, cmdsLayoutBuilder.getStreamRange(0u));
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(4u /*pipeline address, push constant and dispatch*/);
	genCmdsData.push_back(pcValue);
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

	// Conditional rendering buffer.
	const auto       conditionBufferSize  = u32Size;
	const auto       conditionBufferInfo  = makeBufferCreateInfo(conditionBufferSize, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
	BufferWithMemory conditionBuffer      (ctx.vkd, ctx.device, ctx.allocator, conditionBufferInfo, MemoryRequirement::HostVisible);
	const uint32_t   conditionBufferValue = (params.conditionValue ? 2u : 0u); // Avoid using value 1, just to make it interesting.
	auto&            conditionBufferAlloc = conditionBuffer.getAllocation();
	void*            conditionBufferData  = conditionBufferAlloc.getHostPtr();

	deMemcpy(conditionBufferData, &conditionBufferValue, sizeof(conditionBufferValue));
	flushAlloc(ctx.vkd, ctx.device, conditionBufferAlloc);

	// Preprocess buffer.
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *normalPipeline, *cmdsLayout, seqCount);

	// Generated commands info.
	const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, 0ull);

	const VkGeneratedCommandsInfoNV cmdsInfo =
	{
		VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,	//	VkStructureType						sType;
		nullptr,										//	const void*							pNext;
		bindPoint,										//	VkPipelineBindPoint					pipelineBindPoint;
		*normalPipeline,								//	VkPipeline							pipeline;
		*cmdsLayout,									//	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
		1u,												//	uint32_t							streamCount;
		&streamInfo,									//	const VkIndirectCommandsStreamNV*	pStreams;
		seqCount,										//	uint32_t							sequencesCount;
		*preprocessBuffer,								//	VkBuffer							preprocessBuffer;
		0ull,											//	VkDeviceSize						preprocessOffset;
		preprocessBuffer.getSize(),						//	VkDeviceSize						preprocessSize;
		VK_NULL_HANDLE,									//	VkBuffer							sequencesCountBuffer;
		0ull,											//	VkDeviceSize						sequencesCountOffset;
		VK_NULL_HANDLE,									//	VkBuffer							sequencesIndexBuffer;
		0ull,											//	VkDeviceSize						sequencesIndexOffset;
	};

	// Command pool and buffer.
	using CommandPoolWithBufferPtr = std::unique_ptr<CommandPoolWithBuffer>;
	CommandPoolWithBufferPtr computeCmd;
	uint32_t                 compQueueIndex = std::numeric_limits<uint32_t>::max();
	CommandPoolWithBuffer    cmd (ctx.vkd, ctx.device, ctx.qfIndex);

	auto queue     = ctx.queue;
	auto cmdBuffer = *cmd.cmdBuffer;
	beginCommandBuffer(ctx.vkd, cmdBuffer);

	// Record the preprocessing step with conditional rendering enabled.
	beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
	ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
	ctx.vkd.cmdPreprocessGeneratedCommandsNV(cmdBuffer, &cmdsInfo);
	ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
	preprocessToExecuteBarrier(ctx.vkd, cmdBuffer);

	if (params.executeOnCompute)
	{
		compQueueIndex = static_cast<uint32_t>(context.getComputeQueueFamilyIndex());

		// These will be used to transfer buffers from the preprocess queue to the execution queue.
		std::vector<VkBufferMemoryBarrier> ownershipBarriers;

		ownershipBarriers.push_back(makePreprocessToExecuteBarrier(outputBuffer.get(), outputBufferSize, ctx.qfIndex, compQueueIndex));
		ownershipBarriers.push_back(makePreprocessToExecuteBarrier(genCmdsBuffer.get(), genCmdsBufferSize, ctx.qfIndex, compQueueIndex));
		ownershipBarriers.push_back(makePreprocessToExecuteBarrier(preprocessBuffer.get(), preprocessBuffer.getSize(), ctx.qfIndex, compQueueIndex));

		ctx.vkd.cmdPipelineBarrier(cmdBuffer,
			VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			0u,
			0u, nullptr,
			de::sizeU32(ownershipBarriers), de::dataOrNull(ownershipBarriers),
			0u, nullptr);

		endCommandBuffer(ctx.vkd, cmdBuffer);
		submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

		// Switch to the compute command buffer and queue.
		computeCmd.reset(new CommandPoolWithBuffer(ctx.vkd, ctx.device, compQueueIndex));
		cmdBuffer = *computeCmd->cmdBuffer;
		queue     = context.getComputeQueue();

		beginCommandBuffer(ctx.vkd, cmdBuffer);

		// This is the "acquire" barrier to transfer buffer ownership for execution. See above.
		ctx.vkd.cmdPipelineBarrier(cmdBuffer,
			VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			0u,
			0u, nullptr,
			de::sizeU32(ownershipBarriers), de::dataOrNull(ownershipBarriers),
			0u, nullptr);

		ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
	}

	ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, VK_TRUE, &cmdsInfo);
	endCommandBuffer(ctx.vkd, cmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

	// Verify results.
	uint32_t outputValue = 0u;
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

	// In these cases, we expect conditional rendering to not affect preprocessing.
	const auto expectedValue = pcValue;
	if (outputValue != expectedValue)
	{
		std::ostringstream msg;
		msg << "Unexpected value found in output buffer; expected " << expectedValue << " but found " << outputValue;
		return tcu::TestStatus::fail(msg.str());
	}
	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createDGCComputeConditionalTests (tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "conditional_rendering"));
	GroupPtr generalGroup (new tcu::TestCaseGroup(testCtx, "general"));
	GroupPtr preprocessGroup (new tcu::TestCaseGroup(testCtx, "preprocess"));

	// General tests.
	const struct
	{
		UseSecondaries useSecondaries;
		const char*    suffix;
	} secondaryUsageCases[] =
	{
		{ UseSecondaries::NO,                      "_primary"                    },
		{ UseSecondaries::YES_WITHOUT_INHERITANCE, "_secondary"                  },
		{ UseSecondaries::YES_WITH_INHERITANCE,    "_secondary_with_inheritance" },
	};

	for (const auto pipelineToken : { false, true })
		for (const auto indirectCountBuffer : { false, true })
			for (const auto conditionValue : { false, true })
				for (const auto inverted : { false, true })
					for (const auto secondaryUsage : secondaryUsageCases)
						for (const auto computeQueue : { false, true })
						{
							const TestParams params
							{
								pipelineToken,
								indirectCountBuffer,
								conditionValue,
								inverted,
								secondaryUsage.useSecondaries,
								computeQueue,
							};

							const std::string testName = std::string()
								+ (pipelineToken ? "pipeline_token" : "classic_bind")
								+ (indirectCountBuffer ? "_with_count_buffer" : "_without_count_buffer")
								+ (conditionValue ? "_condition_true" : "_condition_false")
								+ secondaryUsage.suffix
								+ (inverted ? "_inverted_flag" : "")
								+ (computeQueue ? "_cq" : "_uq");

							addFunctionCaseWithPrograms(generalGroup.get(), testName,
								checkConditionalDGCComputeSupport,
								storePushConstantProgramParams,
								conditionalDispatchRun,
								params);
						}

	// Preprocessing tests.
	for (const auto conditionValue : { false, true })
		for (const auto inverted : { false, true })
			for (const auto execOnCompute : { false, true })
			{
				const ConditionalPreprocessParams params
				{
					conditionValue,
					inverted,
					execOnCompute,
				};

				const std::string testName = std::string()
					+ (conditionValue ? "condition_true" : "condition_false")
					+ (inverted ? "_inverted_flag" : "")
					+ (execOnCompute ? "_exec_on_compute" : "");

				addFunctionCaseWithPrograms(preprocessGroup.get(), testName,
					checkConditionalPreprocessSupport,
					storePushConstantProgramPreprocessParams,
					conditionalPreprocessRun,
					params);
			}

	mainGroup->addChild(generalGroup.release());
	mainGroup->addChild(preprocessGroup.release());
	return mainGroup.release();
}

} // DGC
} // vkt
