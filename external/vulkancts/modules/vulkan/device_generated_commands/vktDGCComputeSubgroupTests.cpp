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
 * \brief Device Generated Commands Compute Subgroup Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeSubgroupTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktDGCUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include <sstream>
#include <vector>
#include <memory>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

struct BuiltinParams
{
	uint32_t totalInvocations;
	uint32_t subgroupSize;
	bool     pipelineToken;
	bool     computeQueue;

	uint32_t getNumSubgroups (void) const
	{
		DE_ASSERT(totalInvocations % subgroupSize == 0u);
		return (totalInvocations / subgroupSize);
	}
};

void checkSubgroupSupport (Context& context, BuiltinParams params)
{
	checkDGCComputeSupport(context, params.pipelineToken, false);

	if (context.getUsedApiVersion() < VK_API_VERSION_1_3)
		TCU_THROW(NotSupportedError, "Vulkan 1.3 not supported");

	const auto& vk13Properties = context.getDeviceVulkan13Properties();

	if (params.subgroupSize < vk13Properties.minSubgroupSize || params.subgroupSize > vk13Properties.maxSubgroupSize)
		TCU_THROW(NotSupportedError, "Unsupported subgroup size");

	if ((vk13Properties.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT) == 0u)
		TCU_THROW(NotSupportedError, "Compute stage does not support a required subgroup size");

	if (params.computeQueue)
		context.getComputeQueue(); // Throws NotSupportedError if not available.
}

void builtinVerificationProgram (SourceCollections& dst, BuiltinParams params)
{
	ShaderBuildOptions buildOptions (dst.usedVulkanVersion, SPIRV_VERSION_1_6, 0u);

	std::ostringstream comp;
	comp
		<< "#version 460\n"
		<< "#extension GL_KHR_shader_subgroup_basic  : require\n"
		<< "#extension GL_KHR_shader_subgroup_ballot : require\n"
		<< "\n"
		<< "layout (local_size_x=" << params.totalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
		<< "\n"
		<< "layout (set=0, binding=0) buffer NumSubgroupsBlock { uint verification[]; } numSubgroupsBuffer;\n"
		<< "layout (set=0, binding=1) buffer SubgroupIdBlock   { uint verification[]; } subgroupIdBuffer;\n"
		<< "layout (set=0, binding=2) buffer SubgroupSizeBlock { uint verification[]; } subgroupSizeBuffer;\n"
		<< "layout (set=0, binding=3) buffer invocationIdBlock { uint verification[]; } invocationIdBuffer;\n"
		<< "layout (set=0, binding=4) buffer eqMaskBlock       { uint verification[]; } eqMaskBuffer;\n"
		<< "layout (set=0, binding=5) buffer geMaskBlock       { uint verification[]; } geMaskBuffer;\n"
		<< "layout (set=0, binding=6) buffer gtMaskBlock       { uint verification[]; } gtMaskBuffer;\n"
		<< "layout (set=0, binding=7) buffer leMaskBlock       { uint verification[]; } leMaskBuffer;\n"
		<< "layout (set=0, binding=8) buffer ltMaskBlock       { uint verification[]; } ltMaskBuffer;\n"
		<< "\n"
		<< "uint boolToUint (bool value)\n"
		<< "{\n"
		<< "   return (value ? 1 : 0);\n"
		<< "}\n"
		<< "\n"
		<< "bool checkMaskComponent (uint mask, uint offset, uint validBits, uint bitIndex, uint expectedLess, uint expectedEqual, uint expectedGreater)\n"
		<< "{\n"
		<< "    bool ok = true;\n"
		<< "    for (uint i = 0; i < 32; ++i)\n"
		<< "    {\n"
		<< "        const uint bit = ((mask >> i) & 1);\n"
		<< "        const uint idx = offset + i;\n"
		<< "\n"
		<< "        if (idx < validBits) {\n"
		<< "            if (idx < bitIndex && bit != expectedLess)\n"
		<< "                ok = false;\n"
		<< "            else if (idx == bitIndex && bit != expectedEqual)\n"
		<< "                ok = false;\n"
		<< "            else if (idx > bitIndex && bit != expectedGreater)\n"
		<< "                ok = false;\n"
		<< "        }\n"
		<< "        else if (bit != 0)\n"
		<< "            ok = false;\n"
		<< "    }\n"
		<< "    return ok;\n"
		<< "}\n"
		<< "\n"
		<< "bool checkMask (uvec4 mask, uint validBits, uint bitIndex, uint expectedLess, uint expectedEqual, uint expectedGreater)\n"
		<< "{\n"
		<< "   return (checkMaskComponent(mask.x,  0, validBits, bitIndex, expectedLess, expectedEqual, expectedGreater) &&\n"
		<< "           checkMaskComponent(mask.y, 32, validBits, bitIndex, expectedLess, expectedEqual, expectedGreater) &&\n"
		<< "           checkMaskComponent(mask.z, 64, validBits, bitIndex, expectedLess, expectedEqual, expectedGreater) &&\n"
		<< "           checkMaskComponent(mask.w, 96, validBits, bitIndex, expectedLess, expectedEqual, expectedGreater));\n"
		<< "}\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    const uint index = gl_SubgroupInvocationID + gl_SubgroupID * gl_SubgroupSize;\n"
		<< "\n"
		<< "    numSubgroupsBuffer.verification[index] = boolToUint(gl_NumSubgroups == " << params.getNumSubgroups() << ");\n"
		<< "    subgroupIdBuffer.verification  [index] = boolToUint(gl_SubgroupID >= 0 && gl_SubgroupID < gl_NumSubgroups);\n"
		<< "    subgroupSizeBuffer.verification[index] = boolToUint(gl_SubgroupSize == " << params.subgroupSize << ");\n"
		<< "    invocationIdBuffer.verification[index] = boolToUint(gl_SubgroupInvocationID >= 0 && gl_SubgroupInvocationID < gl_SubgroupSize);\n"
		<< "\n"
		<< "    eqMaskBuffer.verification[index] = boolToUint(checkMask(gl_SubgroupEqMask, gl_SubgroupSize, gl_SubgroupInvocationID, 0, 1, 0));\n"
		<< "    geMaskBuffer.verification[index] = boolToUint(checkMask(gl_SubgroupGeMask, gl_SubgroupSize, gl_SubgroupInvocationID, 0, 1, 1));\n"
		<< "    gtMaskBuffer.verification[index] = boolToUint(checkMask(gl_SubgroupGtMask, gl_SubgroupSize, gl_SubgroupInvocationID, 0, 0, 1));\n"
		<< "    leMaskBuffer.verification[index] = boolToUint(checkMask(gl_SubgroupLeMask, gl_SubgroupSize, gl_SubgroupInvocationID, 1, 1, 0));\n"
		<< "    ltMaskBuffer.verification[index] = boolToUint(checkMask(gl_SubgroupLtMask, gl_SubgroupSize, gl_SubgroupInvocationID, 1, 0, 0));\n"
		<< "}\n"
		;

	dst.glslSources.add("comp") << glu::ComputeSource(comp.str()) << buildOptions;
}

tcu::TestStatus verifyBuiltins (Context& context, BuiltinParams params)
{
	const auto&  ctx               = context.getContextCommonData();
	const auto   descType          = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto   stageFlags        = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
	const auto   bindPoint         = VK_PIPELINE_BIND_POINT_COMPUTE;
	auto&        log               = context.getTestContext().getLog();
	const auto   queue             = (params.computeQueue ? context.getComputeQueue() : ctx.queue);
	const auto   qfIndex           = (params.computeQueue ? context.getComputeQueueFamilyIndex() : ctx.qfIndex);
	const size_t outputBufferCount = 9u; // This must match the shader.

	// Output buffers.
	std::vector<uint32_t> outputValues           (params.totalInvocations, 0u);
	const auto            outputBufferSize       = static_cast<VkDeviceSize>(de::dataSize(outputValues));
	const auto            outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
	std::vector<BufferWithMemoryPtr> outputBuffers;
	outputBuffers.reserve(outputBufferCount);

	for (size_t i = 0; i < outputBufferCount; ++i)
	{
		outputBuffers.emplace_back(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible));
		auto& bufferAlloc = outputBuffers.back()->getAllocation();
		void* bufferData  = bufferAlloc.getHostPtr();

		deMemcpy(bufferData, de::dataOrNull(outputValues), de::dataSize(outputValues));
		flushAlloc(ctx.vkd, ctx.device, bufferAlloc);
	}

	// Descriptor set layout, pool and set preparation.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	for (size_t i = 0; i < outputBuffers.size(); ++i)
		setLayoutBuilder.addSingleBinding(descType, stageFlags);
	const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(descType, de::sizeU32(outputBuffers));
	const auto descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

	DescriptorSetUpdateBuilder setUpdateBuilder;
	for (size_t i = 0; i < outputBuffers.size(); ++i)
	{
		const auto descInfo = makeDescriptorBufferInfo(outputBuffers.at(i)->get(), 0ull, outputBufferSize);
		setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(static_cast<uint32_t>(i)), descType, &descInfo);
	}
	setUpdateBuilder.update(ctx.vkd, ctx.device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

	// Shader.
	const auto& binaries   = context.getBinaryCollection();
	const auto  compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

	// Pipeline: either a normal compute pipeline or a DGC compute pipeline.
	using DGCComputePipelinePtr = std::unique_ptr<DGCComputePipeline>;
	DGCComputePipelineMetaDataPool metadataPool;
	DGCComputePipelinePtr          dgcPipeline;
	VkDeviceAddress                dgcPipelineAddress = 0ull;
	Move<VkPipeline>               normalPipeline;

	if (params.pipelineToken)
	{
		dgcPipeline.reset(new DGCComputePipeline(
			metadataPool,
			ctx.vkd,
			ctx.device,
			ctx.allocator,
			0u,
			*pipelineLayout,
			0u,
			*compModule,
			nullptr,
			0ull,
			VK_NULL_HANDLE,
			-1,
			params.subgroupSize));

		dgcPipelineAddress = dgcPipeline->getIndirectDeviceAddress();
	}
	else
	{
		normalPipeline = makeComputePipeline(
			ctx.vkd,
			ctx.device,
			*pipelineLayout,
			0u,
			nullptr,
			*compModule,
			0u,
			nullptr,
			VK_NULL_HANDLE,
			params.subgroupSize);
	}

	// Indirect commands layout. Note the dispatch token is last, but its offset in the sequence is 0.
	IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, bindPoint);
	if (params.pipelineToken)
		cmdsLayoutBuilder.addPipelineToken(0u, 0u);
	cmdsLayoutBuilder.addDispatchToken(0u, cmdsLayoutBuilder.getStreamRange(0u));
	const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

	// Generated indirect commands buffer contents.
	std::vector<uint32_t> genCmdsData;
	genCmdsData.reserve(5u /*2 for the pipeline device address and 3 for the indirect dispatch command*/);
	if (params.pipelineToken)
		pushBackDeviceAddress(genCmdsData, dgcPipelineAddress);
	genCmdsData.push_back(1u); // Dispatch token data.
	genCmdsData.push_back(1u);
	genCmdsData.push_back(1u);

	// Generated indirect commands buffer.
	const auto       genCmdsBufferSize       = de::dataSize(genCmdsData);
	const auto       genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	BufferWithMemory genCmdsBuffer           (ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&            genCmdsBufferAlloc      = genCmdsBuffer.getAllocation();
	void*            genCmdsBufferData       = genCmdsBufferAlloc.getHostPtr();

	deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
	flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

	// Preprocess buffer for 1 sequence. Note normalPipeline will be VK_NULL_HANDLE when using a DGC pipeline, which is what we want.
	PreprocessBuffer preprocessBuffer (ctx.vkd, ctx.device, ctx.allocator, bindPoint, *normalPipeline, *cmdsLayout, 1u);

	// Command pool and buffer.
	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	if (!params.pipelineToken)
		ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
	else
	{
		ctx.vkd.cmdUpdatePipelineIndirectBufferNV(cmdBuffer, bindPoint, dgcPipeline->get());
		metadataUpdateToPreprocessBarrier(ctx.vkd, cmdBuffer);
	}

	{
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
	submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

	// Verify results.
	bool testFail = false;
	for (size_t i = 0; i < outputBuffers.size(); ++i)
	{
		auto& outputBuffer = *outputBuffers.at(i);
		auto& bufferAlloc  = outputBuffer.getAllocation();
		void* bufferData   = bufferAlloc.getHostPtr();

		deMemcpy(de::dataOrNull(outputValues), bufferData, de::dataSize(outputValues));

		for (size_t j = 0; j < outputValues.size(); ++j)
		{
			const auto reference = 1u;
			const auto result    = outputValues.at(j);

			if (result != reference)
			{
				testFail = true;
				log << tcu::TestLog::Message
					<< "Unexpected value at binding " << i << " position " << j
					<< ": expected " << reference
					<< " but found " << result
					<< tcu::TestLog::EndMessage;
			}
		}
	}

	if (testFail)
		return tcu::TestStatus::fail("Unexpected value found in output buffers; check log for details");
	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createDGCComputeSubgroupTests (tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr mainGroup     (new tcu::TestCaseGroup(testCtx, "subgroups"));
	GroupPtr builtinsGroup (new tcu::TestCaseGroup(testCtx, "builtins"));

	const std::vector<uint32_t> invocationCounts { 16u, 32u, 64u, 128u };

	for (const auto computeQueue : { false, true })
		for (const auto dgcPipeline : { false, true })
			for (const auto workgroupSize : invocationCounts)
				for (const auto subgroupSize : invocationCounts)
				{
					if (subgroupSize > workgroupSize)
						break;

					const auto testName
						= "workgroup_size_" + std::to_string(workgroupSize)
						+ "_subgroup_size_" + std::to_string(subgroupSize)
						+ (dgcPipeline ? "_dgc_pipeline" : "_normal_pipeline")
						+ (computeQueue ? "_cq" : "");

					const BuiltinParams params { workgroupSize, subgroupSize, dgcPipeline, computeQueue };
					addFunctionCaseWithPrograms(builtinsGroup.get(), testName, checkSubgroupSupport, builtinVerificationProgram, verifyBuiltins, params);
				}

	mainGroup->addChild(builtinsGroup.release());
	return mainGroup.release();
}

} // DGC
} // vkt
