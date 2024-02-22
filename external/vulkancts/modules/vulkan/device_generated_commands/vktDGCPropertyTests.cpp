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
 * \brief Device Generated Commands Property Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCPropertyTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "deRandom.hpp"

#include <numeric>
#include <vector>
#include <sstream>
#include <limits>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

void checkBasicDGCComputeSupport (Context& context)
{
	checkDGCComputeSupport(context, false, false);
}

void checkBufferOffsetAlignmentSupport (Context& context, uint32_t offset)
{
	checkBasicDGCComputeSupport(context);

	const auto& dgcProperties = context.getDeviceGeneratedCommandsProperties();
	if (offset % dgcProperties.minIndirectCommandsBufferOffsetAlignment != 0u)
		TCU_THROW(NotSupportedError, "Requested offset not a multiple of minIndirectCommandsBufferOffsetAlignment");
}

enum class AlignmentType
{
	COUNT_BUFFER = 0, // Check minSequencesCountBufferOffsetAlignment
	INDEX_BUFFER,     // Check minSequencesIndexBufferOffsetAlignment
};

void checkSequencesOffsetAlignmentSupport (Context& context, AlignmentType)
{
	checkBasicDGCComputeSupport(context);
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

void storePushConstantProgramWithOffset (SourceCollections& dst, uint32_t)
{
	storePushConstantProgram(dst);
}

// Store the push constant value in the output buffer position indicated by another push constant.
void storePushConstantWithIndexProgram (SourceCollections& dst)
{
	std::ostringstream comp;
	comp
		<< "#version 460\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (set=0, binding=0, std430) buffer OutputBlock { uint values[]; } outputBuffer;\n"
		<< "layout (push_constant, std430) uniform PushConstantBlock { uint index; uint value; } pc;\n"
		<< "void main (void) { outputBuffer.values[pc.index] = pc.value; }\n"
		;
	dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

void storePushConstantWithIndexAlignmentProgram (SourceCollections& dst, AlignmentType)
{
	storePushConstantWithIndexProgram(dst);
}

tcu::TestStatus maxIndirectCommandsTokenCountRun (Context& context)
{
	const auto& ctx = context.getContextCommonData();
	const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	auto& log = context.getTestContext().getLog();

	// Output buffer.
	const auto outputBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
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

	// Generated commands layout: test the token count limit.
	const auto& propertyLimit = context.getDeviceGeneratedCommandsProperties().maxIndirectCommandsTokenCount;
	log << tcu::TestLog::Message << "maxIndirectCommandsTokenCount: " << propertyLimit << tcu::TestLog::EndMessage;
	if (propertyLimit < 1u)
		TCU_FAIL("maxIndirectCommandsTokenCount too low");

	constexpr uint32_t kMaxTokens = 1024u; // Hard reasonable limit: this is much higher than typical limits.
	auto chosenLimit = propertyLimit;
	if (propertyLimit > kMaxTokens)
	{
		log << tcu::TestLog::Message << "Limiting token count to " << kMaxTokens << tcu::TestLog::EndMessage;
		chosenLimit = kMaxTokens;
	}
	const auto pcCmdsCount = chosenLimit - 1u; // The last one will be the dispatch token.

	// Push constants first, overwriting the value, then a dispatch.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	for (uint32_t i = 0u; i < pcCmdsCount; ++i)
		cmdsLayoutBuilder.addPushConstantToken(0u, i * pcSize, *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, pcCmdsCount * pcSize);
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	// Add a lot of push constant values, where only the last one is correct, followed by the dispatch command.
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(3u/*dispatch*/ + pcCmdsCount/*push constants*/);
	for (uint32_t i = 0u; i < pcCmdsCount; ++i)
		genCmdsData.push_back(i + 1u);  // Push constant.
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z

	// Generated indirect commands buffer.
	const auto genCmdsBufferSize = de::dataSize(genCmdsData);
	const auto genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	BufferWithMemory genCmdsBuffer (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
	void* genCmdsBufferData = genCmdsBufferAlloc.getHostPtr();

	deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
	flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

	// Preprocess buffer for 1 sequence.
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *pipeline, *cmdsLayout, 1u);

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
	{
		const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, 0ull);
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

	// Verify results.
	uint32_t outputValue = 0u;
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

	if (outputValue != pcCmdsCount)
	{
		std::ostringstream msg;
		msg << "Unexpected value found in output buffer; expected " << pcCmdsCount << " but found " << outputValue;
		return tcu::TestStatus::fail(msg.str());
	}
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus maxIndirectCommandsStreamCountRun (Context& context)
{
	const auto& ctx = context.getContextCommonData();
	const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	auto& log = context.getTestContext().getLog();

	// Output buffer.
	const auto outputBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
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

	// Generated commands layout: test the stream count limit. We'll use one token per stream, so the token limit also has to be
	// taken into account.
	const auto& dgcProperties = context.getDeviceGeneratedCommandsProperties();
	const auto& maxStreamCount = dgcProperties.maxIndirectCommandsStreamCount;
	const auto& maxTokenCount = dgcProperties.maxIndirectCommandsTokenCount;
	constexpr uint32_t kMaxValue = 1024u; // Hard reasonable limit: this is much higher than typical limits.

	if (maxStreamCount < 1u)
		TCU_FAIL("maxIndirectCommandsStreamCount too low");

	if (maxTokenCount < 1u)
		TCU_FAIL("maxIndirectCommandsTokenCount too low");

	const auto chosenLimit = std::min(kMaxValue, std::min(maxTokenCount, maxStreamCount));
	const auto pcCmdsCount = chosenLimit - 1u; // The last one will be the dispatch token.

	log << tcu::TestLog::Message << "maxIndirectCommandsStreamCount: " << maxStreamCount << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "maxIndirectCommandsTokenCount:  " << maxTokenCount << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Chosen limit:                   " << chosenLimit << tcu::TestLog::EndMessage;

	// Push constants first, overwriting the value, then a dispatch. Each token in its own stream.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	for (uint32_t i = 0u; i < pcCmdsCount; ++i)
		cmdsLayoutBuilder.addPushConstantToken(i, 0u, *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(pcCmdsCount, 0u);
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	// Add a lot of push constant values, where only the last one is correct, followed by the dispatch command.
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(3u/*dispatch*/ + pcCmdsCount/*push constants*/);
	for (uint32_t i = 0u; i < pcCmdsCount; ++i)
		genCmdsData.push_back(i + 1u);  // Push constant.
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z

	// Generated indirect commands buffers. One per token, but we'll use a single allocation for all the push constant buffers.
	std::vector<Move<VkBuffer>> genCmdsBuffers;
	de::MovePtr<Allocation> pcStreamBuffersAlloc;
	de::MovePtr<Allocation> dispatchBufferAlloc;

	genCmdsBuffers.reserve(chosenLimit);

	// The push constant buffers are all identical.
	if (pcCmdsCount > 0u)
	{
		const auto pcStreamBufferSize = static_cast<VkDeviceSize>(pcSize);
		const auto pcStreamBufferCreateInfo = makeBufferCreateInfo(pcStreamBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		VkMemoryRequirements pcStreamBufferMemReqs;

		for (uint32_t i = 0u; i < pcCmdsCount; ++i)
		{
			genCmdsBuffers.push_back(createBuffer(ctx.vkd, ctx.device, &pcStreamBufferCreateInfo));
			if (i == 0u)
				pcStreamBufferMemReqs = getBufferMemoryRequirements(ctx.vkd, ctx.device, genCmdsBuffers.back().get());
		}

		// Each buffer will use a memory chunk.
		const auto chunkSize = de::roundUp(pcStreamBufferMemReqs.size, pcStreamBufferMemReqs.alignment);
		pcStreamBufferMemReqs.size = chunkSize * pcCmdsCount;
		pcStreamBuffersAlloc = ctx.allocator.allocate(pcStreamBufferMemReqs, MemoryRequirement::HostVisible);
		char* pcData = reinterpret_cast<char*>(pcStreamBuffersAlloc->getHostPtr());

		for (uint32_t i = 0u; i < pcCmdsCount; ++i)
		{
			const auto allocOffset = i*chunkSize;
			VK_CHECK(ctx.vkd.bindBufferMemory(ctx.device, genCmdsBuffers.at(i).get(),
											  pcStreamBuffersAlloc->getMemory(),
											  pcStreamBuffersAlloc->getOffset() + allocOffset));

			deMemcpy(pcData + allocOffset, &genCmdsData.at(i), pcSize);
		}

		flushAlloc(ctx.vkd, ctx.device, *pcStreamBuffersAlloc);
	}

	// Indirect dispatch command buffer.
	{
		const auto dispatchBufferSize = static_cast<VkDeviceSize>(sizeof(VkDispatchIndirectCommand));
		const auto dispatchBufferCreateInfo = makeBufferCreateInfo(dispatchBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

		genCmdsBuffers.push_back(createBuffer(ctx.vkd, ctx.device, &dispatchBufferCreateInfo));
		const auto dispatchBufferMemReqs = getBufferMemoryRequirements(ctx.vkd, ctx.device, genCmdsBuffers.back().get());
		dispatchBufferAlloc = ctx.allocator.allocate(dispatchBufferMemReqs, MemoryRequirement::HostVisible);
		VK_CHECK(ctx.vkd.bindBufferMemory(ctx.device, genCmdsBuffers.back().get(), dispatchBufferAlloc->getMemory(), dispatchBufferAlloc->getOffset()));

		// The last 3 uints would be the indirect dispatch arguments.
		void* dispatchBufferData = dispatchBufferAlloc->getHostPtr();
		deMemcpy(dispatchBufferData, &genCmdsData.at(pcCmdsCount), sizeof(VkDispatchIndirectCommand));
		flushAlloc(ctx.vkd, ctx.device, *dispatchBufferAlloc);
	}

	// Preprocess buffer for 1 sequence.
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *pipeline, *cmdsLayout, 1u);

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
	{
		std::vector<VkIndirectCommandsStreamNV> streamInfos;
		streamInfos.reserve(genCmdsBuffers.size());
		for (const auto& buffer : genCmdsBuffers)
			streamInfos.push_back(makeIndirectCommandsStreamNV(*buffer, 0ull));

		const VkGeneratedCommandsInfoNV cmdsInfo =
		{
			VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,	//	VkStructureType						sType;
			nullptr,										//	const void*							pNext;
			bindPoint,										//	VkPipelineBindPoint					pipelineBindPoint;
			*pipeline,										//	VkPipeline							pipeline;
			*cmdsLayout,									//	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
			de::sizeU32(streamInfos),						//	uint32_t							streamCount;
			de::dataOrNull(streamInfos),					//	const VkIndirectCommandsStreamNV*	pStreams;
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

	// Verify results.
	uint32_t outputValue = 0u;
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

	if (outputValue != pcCmdsCount)
	{
		std::ostringstream msg;
		msg << "Unexpected value found in output buffer; expected " << pcCmdsCount << " but found " << outputValue;
		return tcu::TestStatus::fail(msg.str());
	}
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus maxIndirectCommandsTokenOffsetRun (Context& context)
{
	const auto& ctx = context.getContextCommonData();
	const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	auto& log = context.getTestContext().getLog();

	// Output buffer.
	const auto outputBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
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
	const auto u32Size = static_cast<uint32_t>(sizeof(uint32_t));
	const auto pcValue = 777u; // Arbitrary.
	const auto pcSize = u32Size;
	const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

	// Shader.
	const auto& binaries = context.getBinaryCollection();
	const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Pipeline.
	const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

	// Generated commands layout: test the token offset limit. We'll use two tokens: the push constant one and the dispatch. Since
	// the idea is to test the token offset limit, but we're also affected by the stream stride limit, we'll put the dispatch token
	// first in the buffer (note: NOT in the layout) and we'll put the single push constant token last, so the stride is as small as
	// possible.
	//
	// Lets start with the max stride first and then reduce that for the offset if needed. Also, set a maximum reasonable limit so
	// we don't allocate a huge amount of memory.
	const auto& dgcProperties = context.getDeviceGeneratedCommandsProperties();
	const auto& maxStride = dgcProperties.maxIndirectCommandsStreamStride;
	const auto& maxTokenOffset = dgcProperties.maxIndirectCommandsTokenOffset;
	constexpr uint32_t kHardMax = 1024u * 1024u; // 1MB is a lot for a single sequence. Note we'll likely use this limit.

	constexpr uint32_t minRequiredOffset = u32Size * 3u; // 3 uints for the indirect dispatch args.
	constexpr uint32_t minRequiredStride = minRequiredOffset + pcSize;

	if (maxStride < minRequiredStride)
		TCU_FAIL("maxIndirectCommandsStreamStride too low");

	if (maxTokenOffset < minRequiredOffset)
		TCU_FAIL("maxIndirectCommandsTokenOffset too low");

	// The offset of the push constant is the lowest of the max stride - sizeof(uint32_t) and the max token offset property.
	// Note we round the max token offset down to make sure the push constant is aligned.
	const auto pcTokenOffset = std::min(kHardMax, de::roundDown(std::min(maxStride - pcSize, maxTokenOffset), u32Size));
	const auto streamStride = pcTokenOffset + pcSize;

	log << tcu::TestLog::Message << "maxIndirectCommandsTokenOffset:  " << maxTokenOffset << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "maxIndirectCommandsStreamStride: " << maxStride << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Hard maximum for the test:       " << kHardMax << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Chosen token offset:             " << pcTokenOffset << tcu::TestLog::EndMessage;

	// Indirect commands layout. Note the dispatch token is last, but its offset in the sequence is 0.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	cmdsLayoutBuilder.addPushConstantToken(0u, pcTokenOffset, *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, 0u);
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	std::vector<uint8_t> genCmdsData (streamStride, 0);
	const VkDispatchIndirectCommand dispatchCmd { 1u, 1u, 1u };
	deMemcpy(de::dataOrNull(genCmdsData), &dispatchCmd, sizeof(dispatchCmd));
	deMemcpy(de::dataOrNull(genCmdsData) + pcTokenOffset, &pcValue, sizeof(pcValue));

	// Generated indirect commands buffer.
	const auto genCmdsBufferSize = de::dataSize(genCmdsData);
	const auto genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	BufferWithMemory genCmdsBuffer (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
	void* genCmdsBufferData = genCmdsBufferAlloc.getHostPtr();

	deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
	flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

	// Preprocess buffer for 1 sequence.
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *pipeline, *cmdsLayout, 1u);

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
	{
		const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, 0ull);
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

	// Verify results.
	uint32_t outputValue = 0u;
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

	if (outputValue != pcValue)
	{
		std::ostringstream msg;
		msg << "Unexpected value found in output buffer; expected " << pcValue << " but found " << outputValue;
		return tcu::TestStatus::fail(msg.str());
	}
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus maxIndirectCommandsStreamStrideRun (Context& context)
{
	const auto& ctx = context.getContextCommonData();
	const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	auto& log = context.getTestContext().getLog();
	const auto u32Size = static_cast<uint32_t>(sizeof(uint32_t));

	// Push constants. This must match the shader.
	struct PushConstants
	{
		uint32_t index;
		uint32_t value;
	};

	const auto pcSize = static_cast<uint32_t>(sizeof(PushConstants));
	const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

	const std::vector<PushConstants> pcValues
	{
		{ 0u, 555u },
		{ 1u, 777u },
	};

	std::vector<uint32_t> outputBufferValues { 0u, 0u };
	DE_ASSERT(outputBufferValues.size() == pcValues.size());

	// Output buffer.
	const auto outputBufferSize = static_cast<VkDeviceSize>(de::dataSize(outputBufferValues));
	const auto outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory outputBuffer (ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& outputBufferAlloc = outputBuffer.getAllocation();
	void* outputBufferData = outputBufferAlloc.getHostPtr();

	deMemcpy(outputBufferData, de::dataOrNull(outputBufferValues), de::dataSize(outputBufferValues));
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

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

	// Shader.
	const auto& binaries = context.getBinaryCollection();
	const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Pipeline.
	const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

	// To test the maximum stride, we'll generate a couple of dispatches and set them apart by the chosen stream stride.
	// Both dispatches will have to be emitted and will use the push constant values to update the buffer.
	const auto& dgcProperties = context.getDeviceGeneratedCommandsProperties();
	const auto& maxStride = dgcProperties.maxIndirectCommandsStreamStride;
	constexpr uint32_t kHardMax = 1024u * 1024u; // 1MB is a lot for a single sequence. Note we'll likely use this limit.
	constexpr uint32_t minRequiredStride = pcSize + static_cast<uint32_t>(sizeof(VkDispatchIndirectCommand));

	if (maxStride < minRequiredStride)
		TCU_FAIL("maxIndirectCommandsStreamStride too low");

	// We need to round down the chosen stride to make sure push constants and dispatch commands are aligned.
	const auto chosenStride = de::roundDown(std::min(kHardMax, maxStride), u32Size);
	const auto dataSize = chosenStride * de::sizeU32(pcValues);

	log << tcu::TestLog::Message << "maxIndirectCommandsStreamStride: " << maxStride << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Hard maximum for the test:       " << kHardMax << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "Chosen stride:                   " << chosenStride << tcu::TestLog::EndMessage;

	// Indirect commands layout.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	cmdsLayoutBuilder.addPushConstantToken(0u, 0u, *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, pcSize);
	cmdsLayoutBuilder.setStreamStride(0u, chosenStride);
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	std::vector<uint8_t> genCmdsData (dataSize, 0);
	const VkDispatchIndirectCommand dispatchCmd { 1u, 1u, 1u };

	for (size_t i = 0u; i < pcValues.size(); ++i)
	{
		const auto offset = i * chosenStride;
		deMemcpy(de::dataOrNull(genCmdsData) + offset, &pcValues.at(i), pcSize);
		deMemcpy(de::dataOrNull(genCmdsData) + offset + pcSize, &dispatchCmd, sizeof(dispatchCmd));
	}

	// Generated indirect commands buffer.
	const auto genCmdsBufferSize = de::dataSize(genCmdsData);
	const auto genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	BufferWithMemory genCmdsBuffer (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
	void* genCmdsBufferData = genCmdsBufferAlloc.getHostPtr();

	deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
	flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

	// Preprocess buffer.
	const auto maxSequences = de::sizeU32(pcValues);
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *pipeline, *cmdsLayout, maxSequences);

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
	{
		const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, 0ull);
		const VkGeneratedCommandsInfoNV cmdsInfo =
		{
			VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,	//	VkStructureType						sType;
			nullptr,										//	const void*							pNext;
			bindPoint,										//	VkPipelineBindPoint					pipelineBindPoint;
			*pipeline,										//	VkPipeline							pipeline;
			*cmdsLayout,									//	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
			1u,												//	uint32_t							streamCount;
			&streamInfo,									//	const VkIndirectCommandsStreamNV*	pStreams;
			maxSequences,									//	uint32_t							sequencesCount;
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

	// Verify results.
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(de::dataOrNull(outputBufferValues), outputBufferData, de::dataSize(outputBufferValues));

	bool fail = false;
	for (size_t i = 0u; i < outputBufferValues.size(); ++i)
	{
		const auto& result = outputBufferValues.at(i);

		// Find the reference value in the push constants.
		bool hasRef = false;
		uint32_t reference = std::numeric_limits<uint32_t>::max();

		for (size_t j = 0u; j < pcValues.size(); ++j)
		{
			if (pcValues.at(j).index == i)
			{
				hasRef = true;
				reference = pcValues.at(j).value;
				break;
			}
		}

		if (!hasRef)
			DE_ASSERT(false);

		if (reference != result)
		{
			fail = true;
			log << tcu::TestLog::Message
				<< "Unexpected value found at index " << i << ": expected " << reference << " but found " << result
				<< tcu::TestLog::EndMessage;
		}
	}

	if (fail)
		return tcu::TestStatus::fail("Unexpected value found in output buffer; check log for details");
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus minIndirectCommandsBufferOffsetAlignmentRun (Context& context, uint32_t offset)
{
	const auto& ctx = context.getContextCommonData();
	const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

	// Make sure the requested offset meets the alignment requirements.
	const auto u32Size = static_cast<uint32_t>(sizeof(uint32_t));
	DE_ASSERT(offset % u32Size == 0u);
	const auto offsetItems = offset / u32Size;

	// Output buffer.
	const auto outputBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
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
	const auto pcValue = 777u; // Arbitrary.
	const auto pcSize = u32Size;
	const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

	// Shader.
	const auto& binaries = context.getBinaryCollection();
	const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Pipeline.
	const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

	// Indirect commands layout: push constant and dispatch command.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	cmdsLayoutBuilder.addPushConstantToken(0u, 0u, *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, pcSize);
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(offsetItems + 4u /*push constant and indirect dispatch command*/);

	for (uint32_t i = 0u; i < offsetItems; ++i)
		genCmdsData.push_back(0x1AB2C3D4u);
	genCmdsData.push_back(pcValue);
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
	genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z

	// Generated indirect commands buffer.
	const auto genCmdsBufferSize = de::dataSize(genCmdsData);
	const auto genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	BufferWithMemory genCmdsBuffer (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
	void* genCmdsBufferData = genCmdsBufferAlloc.getHostPtr();

	deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
	flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

	// Preprocess buffer for 1 sequence.
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *pipeline, *cmdsLayout, 1u);

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
	{
		const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, static_cast<VkDeviceSize>(offset));
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

	// Verify results.
	uint32_t outputValue = 0u;
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

	if (outputValue != pcValue)
	{
		std::ostringstream msg;
		msg << "Unexpected value found in output buffer; expected " << pcValue << " but found " << outputValue;
		return tcu::TestStatus::fail(msg.str());
	}
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus minSequencesOffsetAlignmentsRun (Context& context, AlignmentType alignmentType)
{
	const auto& ctx                 = context.getContextCommonData();
	const auto  descType            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto  stageFlags          = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto  bindPoint           = VK_PIPELINE_BIND_POINT_COMPUTE;
	const auto  u32Size             = static_cast<uint32_t>(sizeof(uint32_t));
	const auto& dgcProperties       = context.getDeviceGeneratedCommandsProperties();
	const auto  memProperties       = getPhysicalDeviceMemoryProperties(ctx.vki, ctx.physicalDevice);
	const auto  nonCoherentAtomSize = context.getDeviceProperties().limits.nonCoherentAtomSize;
	auto&       log                 = context.getTestContext().getLog();

	// These tests will use a relatively large number of sequences, but some variants will use a count buffer to cut the amount of
	// sequences in half. In those cases, the output buffer (and the indirect cmds buffer) will still have space for the whole set,
	// and we'll verify the second half is zero-ed out in the output buffer.
	const auto totalValueCount = 512u;
	const auto countInBuffer   = totalValueCount / ((alignmentType == AlignmentType::COUNT_BUFFER) ? 2u : 1u);

	// Push constants. This must match the shader.
	struct PushConstants
	{
		uint32_t index;
		uint32_t value;
	};

	const auto pcSize = static_cast<uint32_t>(sizeof(PushConstants));
	const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

	// Each position "i" will hold value (i + 1)*1000. We'll use this function in the verification later.
	const auto getStoredValue = [](const uint32_t i) { return (i + 1u)*1000u; };

	// Create the array of push constants.
	std::vector<PushConstants> pcValues;
	pcValues.reserve(totalValueCount);
	for (uint32_t i = 0u; i < totalValueCount; ++i)
		pcValues.push_back(PushConstants{ i, getStoredValue(i) });

	std::vector<uint32_t> outputBufferValues (totalValueCount, 0u);
	DE_ASSERT(outputBufferValues.size() == pcValues.size());

	// Output buffer.
	const auto outputBufferSize = static_cast<VkDeviceSize>(de::dataSize(outputBufferValues));
	const auto outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory outputBuffer (ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& outputBufferAlloc = outputBuffer.getAllocation();
	void* outputBufferData = outputBufferAlloc.getHostPtr();

	deMemcpy(outputBufferData, de::dataOrNull(outputBufferValues), de::dataSize(outputBufferValues));
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

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

	// Shader.
	const auto& binaries = context.getBinaryCollection();
	const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Pipeline.
	const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

	// Indirect commands layout.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	cmdsLayoutBuilder.addPushConstantToken(0u, 0u, *pipelineLayout, stageFlags, 0u, pcSize);
	cmdsLayoutBuilder.addDispatchToken(0u, pcSize);
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	const VkDispatchIndirectCommand dispatchCmd { 1u, 1u, 1u };
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(pcValues.size() * 5u /*2 push constant values, 3 values for the dispatch above*/);
	for (const auto& pcValue : pcValues)
	{
		genCmdsData.push_back(pcValue.index);
		genCmdsData.push_back(pcValue.value);
		genCmdsData.push_back(dispatchCmd.x);
		genCmdsData.push_back(dispatchCmd.y);
		genCmdsData.push_back(dispatchCmd.z);
	}

	// Generated indirect commands buffer.
	const auto genCmdsBufferSize = de::dataSize(genCmdsData);
	const auto genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	BufferWithMemory genCmdsBuffer (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto& genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
	void* genCmdsBufferData = genCmdsBufferAlloc.getHostPtr();

	deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
	flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

	// Preprocess buffer.
	const auto maxSequences = totalValueCount;
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *pipeline, *cmdsLayout, maxSequences);

	// Count buffer if testing it.
	using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
	BufferWithMemoryPtr countBuffer;
	VkDeviceSize countBufferOffset = 0ull;

	if (alignmentType == AlignmentType::COUNT_BUFFER)
	{
		const auto  offset        = dgcProperties.minSequencesCountBufferOffsetAlignment;
		const auto  size          = u32Size;
		const auto  bufferSize    = static_cast<VkDeviceSize>(offset + size);

		log << tcu::TestLog::Message << "minSequencesCountBufferOffsetAlignment: " << offset << tcu::TestLog::EndMessage;
		countBufferOffset = static_cast<VkDeviceSize>(offset);

		// We will also throw in an offset in the memory allocation to make things more interesting.
		SimpleAllocator allocatorWithOffset (
			ctx.vkd,
			ctx.device,
			memProperties,
			SimpleAllocator::OffsetParams{nonCoherentAtomSize, countBufferOffset});

		const auto createInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		countBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, allocatorWithOffset, createInfo, MemoryRequirement::HostVisible));
		auto& countBufferAlloc = countBuffer->getAllocation();
		char* countBufferDataPtr = reinterpret_cast<char*>(countBufferAlloc.getHostPtr());

		deMemcpy(countBufferDataPtr + offset, &countInBuffer, sizeof(countInBuffer));
		flushAlloc(ctx.vkd, ctx.device, countBufferAlloc);
	}

	// Index buffer if testing it.
	std::vector<uint32_t> indices;
	BufferWithMemoryPtr indicesBuffer;
	VkDeviceSize indicesBufferOffset = 0ull;

	if (alignmentType == AlignmentType::INDEX_BUFFER)
	{
		// Generate shuffled indices.
		indices.resize(totalValueCount, 0u);
		std::iota(begin(indices), end(indices), 0u);

		de::Random rnd(1707306954u);
		rnd.shuffle(begin(indices), end(indices));

		const auto offset     = dgcProperties.minSequencesIndexBufferOffsetAlignment;
		const auto bufferSize = static_cast<VkDeviceSize>(offset + de::dataSize(indices));

		log << tcu::TestLog::Message << "minSequencesIndexBufferOffsetAlignment: " << offset << tcu::TestLog::EndMessage;
		indicesBufferOffset = static_cast<VkDeviceSize>(offset);

		// We will also throw in an offset in the memory allocation to make things more interesting.
		SimpleAllocator allocatorWithOffset (
			ctx.vkd,
			ctx.device,
			memProperties,
			SimpleAllocator::OffsetParams{nonCoherentAtomSize, indicesBufferOffset});

		const auto createInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		indicesBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, allocatorWithOffset, createInfo, MemoryRequirement::HostVisible));
		auto& indicesBufferAlloc = indicesBuffer->getAllocation();
		char* indicesBufferDataPtr = reinterpret_cast<char*>(indicesBufferAlloc.getHostPtr());

		deMemcpy(indicesBufferDataPtr + offset, de::dataOrNull(indices), de::dataSize(indices));
		flushAlloc(ctx.vkd, ctx.device, indicesBufferAlloc);
	}

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
	{
		const auto streamInfo = makeIndirectCommandsStreamNV(*genCmdsBuffer, 0ull);
		const VkGeneratedCommandsInfoNV cmdsInfo =
		{
			VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,				//	VkStructureType						sType;
			nullptr,													//	const void*							pNext;
			bindPoint,													//	VkPipelineBindPoint					pipelineBindPoint;
			*pipeline,													//	VkPipeline							pipeline;
			*cmdsLayout,												//	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
			1u,															//	uint32_t							streamCount;
			&streamInfo,												//	const VkIndirectCommandsStreamNV*	pStreams;
			maxSequences,												//	uint32_t							sequencesCount;
			*preprocessBuffer,											//	VkBuffer							preprocessBuffer;
			0ull,														//	VkDeviceSize						preprocessOffset;
			preprocessBuffer.getSize(),									//	VkDeviceSize						preprocessSize;
			(countBuffer ? countBuffer->get() : VK_NULL_HANDLE),		//	VkBuffer							sequencesCountBuffer;
			countBufferOffset,											//	VkDeviceSize						sequencesCountOffset;
			(indicesBuffer ? indicesBuffer->get() : VK_NULL_HANDLE),	//	VkBuffer							sequencesIndexBuffer;
			indicesBufferOffset,										//	VkDeviceSize						sequencesIndexOffset;
		};
		ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, VK_FALSE, &cmdsInfo);
	}
	{
		const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
	}
	endCommandBuffer(ctx.vkd, cmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

	// Verify results.
	invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
	deMemcpy(de::dataOrNull(outputBufferValues), outputBufferData, de::dataSize(outputBufferValues));

	bool fail = false;
	for (size_t i = 0u; i < outputBufferValues.size(); ++i)
	{
		const auto& result = outputBufferValues.at(i);
		const auto& reference = ((i < countInBuffer) ? getStoredValue(static_cast<uint32_t>(i)) : 0u);

		// Find the reference value in the push constants.
		if (reference != result)
		{
			fail = true;
			log << tcu::TestLog::Message
				<< "Unexpected value found at index " << i << ": expected " << reference << " but found " << result
				<< tcu::TestLog::EndMessage;
		}
	}

	if (fail)
		return tcu::TestStatus::fail("Unexpected value found in output buffer; check log for details");
	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus validLimits (Context& context)
{
	const auto& properties = context.getDeviceGeneratedCommandsProperties();

	// Note: we are assuming a value of 0 in maxGraphicsShaderGroupCount is a way to signal the implementation only supports
	// DGC-compute but not graphics. This is not per-spec with the current spec, but it's a compromise. In hindsight, there should
	// have been a separate feature bit for this.
	if (properties.maxGraphicsShaderGroupCount < (1u << 12) && properties.maxGraphicsShaderGroupCount != 0u)
		TCU_FAIL("maxGraphicsShaderGroupCount not in required range");

	if (properties.maxIndirectSequenceCount < (1u << 20))
		TCU_FAIL("maxIndirectSequenceCount not in required range");

	if (properties.maxIndirectCommandsTokenCount < (16u))
		TCU_FAIL("maxIndirectCommandsTokenCount not in required range");

	if (properties.maxIndirectCommandsStreamCount < (16u))
		TCU_FAIL("maxIndirectCommandsStreamCount not in required range");

	if (properties.maxIndirectCommandsTokenOffset < (2047u))
		TCU_FAIL("maxIndirectCommandsTokenOffset not in required range");

	if (properties.maxIndirectCommandsStreamStride < (2048u))
		TCU_FAIL("maxIndirectCommandsStreamStride not in required range");

	if (properties.minSequencesCountBufferOffsetAlignment > (256u))
		TCU_FAIL("minSequencesCountBufferOffsetAlignment not in required range");

	if (properties.minSequencesIndexBufferOffsetAlignment > (256u))
		TCU_FAIL("minSequencesIndexBufferOffsetAlignment not in required range");

	if (properties.minIndirectCommandsBufferOffsetAlignment > (256u))
		TCU_FAIL("minIndirectCommandsBufferOffsetAlignment not in required range");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createDGCPropertyTests (tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "properties"));
	addFunctionCase(mainGroup.get(), "valid_limits", checkDGCSupport, validLimits);
	addFunctionCaseWithPrograms(mainGroup.get(), "maxIndirectCommandsTokenCount", checkBasicDGCComputeSupport, storePushConstantProgram, maxIndirectCommandsTokenCountRun);
	addFunctionCaseWithPrograms(mainGroup.get(), "maxIndirectCommandsStreamCount", checkBasicDGCComputeSupport, storePushConstantProgram, maxIndirectCommandsStreamCountRun);
	addFunctionCaseWithPrograms(mainGroup.get(), "maxIndirectCommandsTokenOffset", checkBasicDGCComputeSupport, storePushConstantProgram, maxIndirectCommandsTokenOffsetRun);
	addFunctionCaseWithPrograms(mainGroup.get(), "maxIndirectCommandsStreamStrideRun", checkBasicDGCComputeSupport, storePushConstantWithIndexProgram, maxIndirectCommandsStreamStrideRun);

	const std::vector<uint32_t> offsets { 4u, 8u, 256u };
	for (const auto& offset : offsets)
	{
		const auto testName = std::string("minIndirectCommandsBufferOffsetAlignment_offset_") + std::to_string(offset);
		addFunctionCaseWithPrograms(mainGroup.get(), testName, checkBufferOffsetAlignmentSupport, storePushConstantProgramWithOffset, minIndirectCommandsBufferOffsetAlignmentRun, offset);
	}

	const struct
	{
		AlignmentType alignmentType;
		const char*   name;
	} alignmentTests[] =
	{
		{ AlignmentType::COUNT_BUFFER, "minSequencesCountBufferOffsetAlignment" },
		{ AlignmentType::INDEX_BUFFER, "minSequencesIndexBufferOffsetAlignment" },
	};

	for (const auto& alignmentCase : alignmentTests)
		addFunctionCaseWithPrograms(mainGroup.get(), alignmentCase.name, checkSequencesOffsetAlignmentSupport, storePushConstantWithIndexAlignmentProgram, minSequencesOffsetAlignmentsRun, alignmentCase.alignmentType);

	return mainGroup.release();
}

} // DGC
} // vkt
