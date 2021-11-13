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
 * \brief Pipeline Bind Point Tests
 *//*--------------------------------------------------------------------*/
#include "vktPipelineBindPointTests.hpp"
#include "vktPipelineImageUtil.hpp"

#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkRayTracingUtil.hpp"

#include "tcuVector.hpp"

#include <algorithm>
#include <string>
#include <sstream>
#include <type_traits>
#include <utility>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

// These operations will be tried in different orders.
// To avoid combinatory explosions, we'll only use two pipeline types per test, which means 2 pipeline bind operations and 2 related set bind operations.
// The following types will be mixed: (graphics, compute), (graphics, ray tracing) and (compute, ray tracing).
enum class SetupOp
{
	BIND_GRAPHICS_PIPELINE		= 0,
	BIND_COMPUTE_PIPELINE		= 1,
	BIND_RAYTRACING_PIPELINE	= 2,
	BIND_GRAPHICS_SET			= 3,
	BIND_COMPUTE_SET			= 4,
	BIND_RAYTRACING_SET			= 5,
	OP_COUNT					= 6,
};

// How to bind each set.
enum class SetUpdateType
{
	WRITE				= 0,
	PUSH				= 1,
	PUSH_WITH_TEMPLATE	= 2,
	TYPE_COUNT			= 3,
};

// Types of operations to dispatch. They will be tried in different orders and are related to the setup sequence.
enum class DispatchOp
{
	DRAW		= 0,
	COMPUTE		= 1,
	TRACE_RAYS	= 2,
	OP_COUNT	= 3,
};

constexpr auto kTestBindPoints			= 2;					// Two bind points per test.
constexpr auto kSetupSequenceSize		= kTestBindPoints * 2;	// For each bind point: bind pipeline and bind set.
constexpr auto kDispatchSequenceSize	= kTestBindPoints;		// Dispatch two types of work, matching the bind points being used.

using SetupSequence		= tcu::Vector<SetupOp, kSetupSequenceSize>;
using DispatchSequence	= tcu::Vector<DispatchOp, kDispatchSequenceSize>;

// Test parameters.
struct TestParams
{
	SetUpdateType		graphicsSetUpdateType;
	SetUpdateType		computeSetUpdateType;
	SetUpdateType		rayTracingSetUpdateType;
	SetupSequence		setupSequence;
	DispatchSequence	dispatchSequence;

protected:
	bool hasSetupOp (SetupOp op) const
	{
		for (int i = 0; i < decltype(setupSequence)::SIZE; ++i)
		{
			if (setupSequence[i] == op)
				return true;
		}
		return false;
	}

	bool hasAnyOf (const std::vector<SetupOp>& opVec) const
	{
		for (const auto& op : opVec)
		{
			if (hasSetupOp(op))
				return true;
		}
		return false;
	}

public:
	bool hasGraphics (void) const
	{
		const std::vector<SetupOp> setupOps {SetupOp::BIND_GRAPHICS_PIPELINE, SetupOp::BIND_GRAPHICS_SET};
		return hasAnyOf(setupOps);
	}

	bool hasCompute (void) const
	{
		const std::vector<SetupOp> setupOps {SetupOp::BIND_COMPUTE_PIPELINE, SetupOp::BIND_COMPUTE_SET};
		return hasAnyOf(setupOps);
	}

	bool hasRayTracing (void) const
	{
		const std::vector<SetupOp> setupOps {SetupOp::BIND_RAYTRACING_PIPELINE, SetupOp::BIND_RAYTRACING_SET};
		return hasAnyOf(setupOps);
	}

};

// Expected output values in each buffer.
constexpr deUint32 kExpectedBufferValueGraphics		= 1u;
constexpr deUint32 kExpectedBufferValueCompute		= 2u;
constexpr deUint32 kExpectedBufferValueRayTracing	= 3u;

class BindPointTest : public vkt::TestCase
{
public:
							BindPointTest		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~BindPointTest		(void) {}

	virtual void			checkSupport		(Context& context) const;
	virtual void			initPrograms		(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance		(Context& context) const;

protected:
	TestParams				m_params;
};

class BindPointInstance : public vkt::TestInstance
{
public:
								BindPointInstance	(Context& context, const TestParams& params);
	virtual						~BindPointInstance	(void) {}

	virtual tcu::TestStatus		iterate				(void);

protected:
	TestParams					m_params;
};

BindPointTest::BindPointTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

void BindPointTest::checkSupport (Context& context) const
{
	if (m_params.graphicsSetUpdateType != SetUpdateType::WRITE || m_params.computeSetUpdateType != SetUpdateType::WRITE)
	{
		context.requireDeviceFunctionality("VK_KHR_push_descriptor");

		if (m_params.graphicsSetUpdateType == SetUpdateType::PUSH_WITH_TEMPLATE || m_params.computeSetUpdateType == SetUpdateType::PUSH_WITH_TEMPLATE)
			context.requireDeviceFunctionality("VK_KHR_descriptor_update_template");
	}

	if (m_params.hasRayTracing())
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
}

void BindPointTest::initPrograms (vk::SourceCollections& programCollection) const
{
	// The flags array will only have 1 element.
	const std::string descriptorDecl = "layout(set=0, binding=0, std430) buffer BufferBlock { uint flag[]; } outBuffer;\n";

	if (m_params.hasGraphics())
	{
		std::ostringstream vert;
		vert
			<< "#version 450\n"
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			// Full-screen clockwise triangle strip with 4 vertices.
			<< "	const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
			<< "	const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
			<< "	gl_Position = vec4(x, y, 0.0, 1.0);\n"
			<< "}\n"
			;

		// Note: the color attachment will be a 1x1 image, so gl_FragCoord.xy is (0.5, 0.5).
		std::ostringstream frag;
		frag
			<< "#version 450\n"
			<< descriptorDecl
			<< "layout(location=0) out vec4 outColor;\n"
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			<< "  const uint xCoord = uint(trunc(gl_FragCoord.x));\n"
			<< "  const uint yCoord = uint(trunc(gl_FragCoord.y));\n"
			<< "  outBuffer.flag[xCoord + yCoord] = " << kExpectedBufferValueGraphics << "u;\n"
			<< "  outColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
			<< "}\n"
			;

		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}

	if (m_params.hasCompute())
	{
		// Note: we will only dispatch 1 group.
		std::ostringstream comp;
		comp
			<< "#version 450\n"
			<< descriptorDecl
			<< "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			<< "  const uint index = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y + gl_GlobalInvocationID.z;\n"
			<< "  outBuffer.flag[index] = " << kExpectedBufferValueCompute << "u;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
	}

	if (m_params.hasRayTracing())
	{
		// We will only call the ray gen shader once.
		std::ostringstream rgen;
		rgen
			<< "#version 460\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< descriptorDecl
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			<< "  const uint index = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y + gl_LaunchIDEXT.z;\n"
			<< "  outBuffer.flag[index] = " << kExpectedBufferValueRayTracing << "u;\n"
			<< "}\n"
			;

		const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
	}
}

vkt::TestInstance* BindPointTest::createInstance (Context& context) const
{
	return new BindPointInstance(context, m_params);
}

BindPointInstance::BindPointInstance (Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

Move<VkDescriptorSetLayout> makeSetLayout(const DeviceInterface& vkd, VkDevice device, VkShaderStageFlags stages, bool push)
{
	VkDescriptorSetLayoutCreateFlags createFlags = 0u;
	if (push)
		createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

	DescriptorSetLayoutBuilder builder;
	builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
	return builder.build(vkd, device, createFlags);
}

void zeroOutAndFlush (const DeviceInterface& vkd, VkDevice device, BufferWithMemory& buffer, VkDeviceSize bufferSize)
{
	auto& alloc		= buffer.getAllocation();
	void* hostPtr	= alloc.getHostPtr();

	deMemset(hostPtr, 0, static_cast<size_t>(bufferSize));
	flushAlloc(vkd, device, alloc);
}

void makePoolAndSet (const DeviceInterface& vkd, VkDevice device, VkDescriptorSetLayout layout, Move<VkDescriptorPool>& pool, Move<VkDescriptorSet>& set)
{
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	pool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	set		= makeDescriptorSet(vkd, device, pool.get(), layout);
}

void writeSetUpdate (const DeviceInterface& vkd, VkDevice device, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkDescriptorSet set)
{
	DescriptorSetUpdateBuilder updateBuilder;
	const auto bufferInfo = makeDescriptorBufferInfo(buffer, offset, size);
	updateBuilder.writeSingle(set, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
	updateBuilder.update(vkd, device);
}

Move<VkDescriptorUpdateTemplate> makeUpdateTemplate (const DeviceInterface& vkd, VkDevice device, VkDescriptorSetLayout setLayout, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout)
{
	const auto									templateEntry		= makeDescriptorUpdateTemplateEntry(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<deUintptr>(0), static_cast<deUintptr>(sizeof(VkDescriptorBufferInfo)));
	const VkDescriptorUpdateTemplateCreateInfo	templateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkDescriptorUpdateTemplateCreateFlags	flags;
		1u,															//	deUint32								descriptorUpdateEntryCount;
		&templateEntry,												//	const VkDescriptorUpdateTemplateEntry*	pDescriptorUpdateEntries;
		VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR,	//	VkDescriptorUpdateTemplateType			templateType;
		setLayout,													//	VkDescriptorSetLayout					descriptorSetLayout;
		bindPoint,													//	VkPipelineBindPoint						pipelineBindPoint;
		pipelineLayout,												//	VkPipelineLayout						pipelineLayout;
		0u,															//	deUint32								set;
	};
	return createDescriptorUpdateTemplate(vkd, device, &templateCreateInfo);
}

void pushBufferDescriptor(const DeviceInterface& vkd, VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size)
{
	const auto					bufferInfo	= makeDescriptorBufferInfo(buffer, offset, size);
	const VkWriteDescriptorSet	write		=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	//	VkStructureType					sType;
		nullptr,								//	const void*						pNext;
		DE_NULL,								//	VkDescriptorSet					dstSet;
		0u,										//	deUint32						dstBinding;
		0u,										//	deUint32						dstArrayElement;
		1u,										//	deUint32						descriptorCount;
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		//	VkDescriptorType				descriptorType;
		nullptr,								//	const VkDescriptorImageInfo*	pImageInfo;
		&bufferInfo,							//	const VkDescriptorBufferInfo*	pBufferInfo;
		nullptr,								//	const VkBufferView*				pTexelBufferView;
	};
	vkd.cmdPushDescriptorSetKHR(cmdBuffer, bindPoint, layout, 0u, 1u, &write);
}

void verifyBufferContents (const DeviceInterface& vkd, VkDevice device, const BufferWithMemory& buffer, const std::string& bufferName, deUint32 expected)
{
	auto&				bufferAlloc	= buffer.getAllocation();
	const auto			dataPtr		= reinterpret_cast<deUint32*>(bufferAlloc.getHostPtr());
	deUint32			data;

	invalidateAlloc(vkd, device, bufferAlloc);
	deMemcpy(&data, dataPtr, sizeof(data));

	if (data != expected)
	{
		std::ostringstream msg;
		msg << "Invalid value found in " << bufferName << " buffer: expected " << expected << " and found " << data;
		TCU_FAIL(msg.str());
	}
}

VkBufferMemoryBarrier makeBufferBarrier (VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size)
{
	return makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, buffer, offset, size);
}

void recordBufferBarrier (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer, VkPipelineStageFlagBits stage, const VkBufferMemoryBarrier& barrier)
{
	vkd.cmdPipelineBarrier(cmdBuffer, stage, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &barrier, 0u, nullptr);
}

tcu::TestStatus BindPointInstance::iterate (void)
{
	const auto&	vki			= m_context.getInstanceInterface();
	const auto	physDev		= m_context.getPhysicalDevice();
	const auto&	vkd			= m_context.getDeviceInterface();
	const auto	device		= m_context.getDevice();
	const auto	qIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto	queue		= m_context.getUniversalQueue();
	auto&		alloc		= m_context.getDefaultAllocator();

	const auto	imageFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto	imageExtent		= makeExtent3D(1u, 1u, 1u);
	const auto	imageType		= VK_IMAGE_TYPE_2D;
	const auto	imageViewType	= VK_IMAGE_VIEW_TYPE_2D;
	const auto	imageUsage		= static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	const auto	viewport		= makeViewport(imageExtent);
	const auto	scissor			= makeRect2D(imageExtent);

	const auto	hasGraphics		= m_params.hasGraphics();
	const auto	hasCompute		= m_params.hasCompute();
	const auto	hasRayTracing	= m_params.hasRayTracing();

	// Storage buffers.
	const auto bufferSize		= static_cast<VkDeviceSize>(sizeof(deUint32));
	const auto bufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	using BufferWithMemoryPtr = de::MovePtr<BufferWithMemory>;
	using ImageWithMemoryPtr = de::MovePtr<ImageWithMemory>;

	BufferWithMemoryPtr graphicsBuffer;
	BufferWithMemoryPtr computeBuffer;
	BufferWithMemoryPtr rayTracingBuffer;

	if (hasGraphics)	graphicsBuffer		= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible));
	if (hasCompute)		computeBuffer		= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible));
	if (hasRayTracing)	rayTracingBuffer	= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible));

	if (hasGraphics)	zeroOutAndFlush(vkd, device, *graphicsBuffer, bufferSize);
	if (hasCompute)		zeroOutAndFlush(vkd, device, *computeBuffer, bufferSize);
	if (hasRayTracing)	zeroOutAndFlush(vkd, device, *rayTracingBuffer, bufferSize);

	ImageWithMemoryPtr	colorAttachment;
	Move<VkImageView>	colorAttachmentView;

	if (hasGraphics)
	{
		// Color attachment.
		const VkImageCreateInfo imageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			imageType,								//	VkImageType				imageType;
			imageFormat,							//	VkFormat				format;
			imageExtent,							//	VkExtent3D				extent;
			1u,										//	deUint32				mipLevels;
			1u,										//	deUint32				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			imageUsage,								//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			1u,										//	deUint32				queueFamilyIndexCount;
			&qIndex,								//	const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};

		const auto subresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		colorAttachment					= ImageWithMemoryPtr(new ImageWithMemory(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any));
		colorAttachmentView				= makeImageView(vkd, device, colorAttachment->get(), imageViewType, imageFormat, subresourceRange);
	}

	// Command buffer and pool.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Set and pipeline layouts.
	Move<VkDescriptorSetLayout> graphicsSetLayout;
	Move<VkDescriptorSetLayout> computeSetLayout;
	Move<VkDescriptorSetLayout> rayTracingSetLayout;

	if (hasGraphics)	graphicsSetLayout	= makeSetLayout(vkd, device, VK_SHADER_STAGE_FRAGMENT_BIT, (m_params.graphicsSetUpdateType != SetUpdateType::WRITE));
	if (hasCompute)		computeSetLayout	= makeSetLayout(vkd, device, VK_SHADER_STAGE_COMPUTE_BIT, (m_params.computeSetUpdateType != SetUpdateType::WRITE));
	if (hasRayTracing)	rayTracingSetLayout	= makeSetLayout(vkd, device, VK_SHADER_STAGE_RAYGEN_BIT_KHR, (m_params.rayTracingSetUpdateType != SetUpdateType::WRITE));

	Move<VkPipelineLayout> graphicsPipelineLayout;
	Move<VkPipelineLayout> computePipelineLayout;
	Move<VkPipelineLayout> rayTracingPipelineLayout;

	if (hasGraphics)	graphicsPipelineLayout		= makePipelineLayout(vkd, device, graphicsSetLayout.get());
	if (hasCompute)		computePipelineLayout		= makePipelineLayout(vkd, device, computeSetLayout.get());
	if (hasRayTracing)	rayTracingPipelineLayout	= makePipelineLayout(vkd, device, rayTracingSetLayout.get());

	// Shader modules.
	Move<VkShaderModule> vertShader;
	Move<VkShaderModule> fragShader;
	Move<VkShaderModule> compShader;
	Move<VkShaderModule> rgenShader;

	if (hasGraphics)	vertShader = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	if (hasGraphics)	fragShader = createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);
	if (hasCompute)		compShader = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);
	if (hasRayTracing)	rgenShader = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0u);

	Move<VkRenderPass>	renderPass;
	Move<VkFramebuffer>	framebuffer;
	Move<VkPipeline>	graphicsPipeline;

	if (hasGraphics)
	{
		// Render pass and framebuffer.
		renderPass	= makeRenderPass(vkd, device, imageFormat);
		framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), colorAttachmentView.get(), imageExtent.width, imageExtent.height);

		// Graphics pipeline.
		std::vector<VkViewport>	viewports(1u, viewport);
		std::vector<VkRect2D>	scissors(1u, scissor);

		const VkPipelineVertexInputStateCreateInfo vertexInputState =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType
			nullptr,													// const void*                                 pNext
			0u,															// VkPipelineVertexInputStateCreateFlags       flags
			0u,															// deUint32                                    vertexBindingDescriptionCount
			nullptr,													// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
			0u,															// deUint32                                    vertexAttributeDescriptionCount
			nullptr,													// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
		};

		graphicsPipeline = makeGraphicsPipeline(vkd, device, graphicsPipelineLayout.get(),
			vertShader.get(), DE_NULL, DE_NULL, DE_NULL, fragShader.get(),	// Shaders.
			renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInputState);
	}

	// Compute pipeline.
	Move<VkPipeline> computePipeline;

	if (hasCompute)
	{
		const VkPipelineShaderStageCreateInfo computeShaderStageInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
			nullptr,												//	const void*							pNext;
			0u,														//	VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							//	VkShaderStageFlagBits				stage;
			compShader.get(),										//	VkShaderModule						module;
			"main",													//	const char*							pName;
			nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo computePipelineCreateInfo =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	//	VkStructureType					sType;
			nullptr,										//	const void*						pNext;
			0u,												//	VkPipelineCreateFlags			flags;
			computeShaderStageInfo,							//	VkPipelineShaderStageCreateInfo	stage;
			computePipelineLayout.get(),					//	VkPipelineLayout				layout;
			DE_NULL,										//	VkPipeline						basePipelineHandle;
			0u,												//	deInt32							basePipelineIndex;
		};

		computePipeline = createComputePipeline(vkd, device, DE_NULL, &computePipelineCreateInfo);
	}

	// Ray tracing pipeline and shader binding tables.
	using RayTracingPipelineHelperPtr = de::MovePtr<RayTracingPipeline>;

	RayTracingPipelineHelperPtr		rayTracingPipelineHelper;
	Move<VkPipeline>				rayTracingPipeline;
	BufferWithMemoryPtr				raygenSBT;

	VkStridedDeviceAddressRegionKHR	raygenSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	missSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	hitSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	callableSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	if (hasRayTracing)
	{
		const auto rtProperties				= makeRayTracingProperties(vki, physDev);
		const auto shaderGroupHandleSize	= rtProperties->getShaderGroupHandleSize();
		const auto shaderGroupBaseAlignment	= rtProperties->getShaderGroupBaseAlignment();
		rayTracingPipelineHelper			= RayTracingPipelineHelperPtr(new RayTracingPipeline());

		rayTracingPipelineHelper->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenShader, 0);
		rayTracingPipeline = rayTracingPipelineHelper->createPipeline(vkd, device, rayTracingPipelineLayout.get());

		raygenSBT		= rayTracingPipelineHelper->createShaderBindingTable(vkd, device, rayTracingPipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}

	// Descriptor pools and sets if needed.
	Move<VkDescriptorPool>	graphicsDescriptorPool;
	Move<VkDescriptorPool>	computeDescriptorPool;
	Move<VkDescriptorPool>	rayTracingDescriptorPool;
	Move<VkDescriptorSet>	graphicsDescritorSet;
	Move<VkDescriptorSet>	computeDescriptorSet;
	Move<VkDescriptorSet>	rayTracingDescriptorSet;

	if (m_params.graphicsSetUpdateType == SetUpdateType::WRITE)
	{
		makePoolAndSet(vkd, device, graphicsSetLayout.get(), graphicsDescriptorPool, graphicsDescritorSet);
		writeSetUpdate(vkd, device, graphicsBuffer->get(), 0ull, bufferSize, graphicsDescritorSet.get());
	}

	if (m_params.computeSetUpdateType == SetUpdateType::WRITE)
	{
		makePoolAndSet(vkd, device, computeSetLayout.get(), computeDescriptorPool, computeDescriptorSet);
		writeSetUpdate(vkd, device, computeBuffer->get(), 0ull, bufferSize, computeDescriptorSet.get());
	}

	if (m_params.rayTracingSetUpdateType == SetUpdateType::WRITE)
	{
		makePoolAndSet(vkd, device, rayTracingSetLayout.get(), rayTracingDescriptorPool, rayTracingDescriptorSet);
		writeSetUpdate(vkd, device, rayTracingBuffer->get(), 0ull, bufferSize, rayTracingDescriptorSet.get());
	}

	// Templates if needed.
	Move<VkDescriptorUpdateTemplate> graphicsUpdateTemplate;
	Move<VkDescriptorUpdateTemplate> computeUpdateTemplate;
	Move<VkDescriptorUpdateTemplate> rayTracingUpdateTemplate;

	if (m_params.graphicsSetUpdateType == SetUpdateType::PUSH_WITH_TEMPLATE)
		graphicsUpdateTemplate = makeUpdateTemplate(vkd, device, graphicsSetLayout.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get());

	if (m_params.computeSetUpdateType == SetUpdateType::PUSH_WITH_TEMPLATE)
		computeUpdateTemplate = makeUpdateTemplate(vkd, device, computeSetLayout.get(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout.get());

	if (m_params.rayTracingSetUpdateType == SetUpdateType::PUSH_WITH_TEMPLATE)
		rayTracingUpdateTemplate = makeUpdateTemplate(vkd, device, rayTracingSetLayout.get(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipelineLayout.get());

	beginCommandBuffer(vkd, cmdBuffer);

	// Helper flags to check the test has been specified properly.
	bool boundGraphicsPipeline		= false;
	bool boundGraphicsSet			= false;
	bool boundComputePipeline		= false;
	bool boundComputeSet			= false;
	bool boundRayTracingPipeline	= false;
	bool boundRayTracingSet			= false;

	// Setup operations in desired order.
	for (int i = 0; i < decltype(m_params.setupSequence)::SIZE; ++i)
	{
		const auto& setupOp = m_params.setupSequence[i];
		switch (setupOp)
		{
		case SetupOp::BIND_GRAPHICS_PIPELINE:
			vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
			boundGraphicsPipeline = true;
			break;

		case SetupOp::BIND_COMPUTE_PIPELINE:
			vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.get());
			boundComputePipeline = true;
			break;

		case SetupOp::BIND_RAYTRACING_PIPELINE:
			vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline.get());
			boundRayTracingPipeline = true;
			break;

		case SetupOp::BIND_GRAPHICS_SET:
			if (m_params.graphicsSetUpdateType == SetUpdateType::WRITE)
				vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0u, 1u, &graphicsDescritorSet.get(), 0u, nullptr);
			else if (m_params.graphicsSetUpdateType == SetUpdateType::PUSH)
				pushBufferDescriptor(vkd, cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), graphicsBuffer->get(), 0ull, bufferSize);
			else if (m_params.graphicsSetUpdateType == SetUpdateType::PUSH_WITH_TEMPLATE)
			{
				const auto bufferInfo = makeDescriptorBufferInfo(graphicsBuffer->get(), 0ull, bufferSize);
				vkd.cmdPushDescriptorSetWithTemplateKHR(cmdBuffer, graphicsUpdateTemplate.get(), graphicsPipelineLayout.get(), 0u, &bufferInfo);
			}
			else
				DE_ASSERT(false);
			boundGraphicsSet = true;
			break;

		case SetupOp::BIND_COMPUTE_SET:
			if (m_params.computeSetUpdateType == SetUpdateType::WRITE)
				vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout.get(), 0u, 1u, &computeDescriptorSet.get(), 0u, nullptr);
			else if (m_params.computeSetUpdateType == SetUpdateType::PUSH)
				pushBufferDescriptor(vkd, cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout.get(), computeBuffer->get(), 0ull, bufferSize);
			else if (m_params.computeSetUpdateType == SetUpdateType::PUSH_WITH_TEMPLATE)
			{
				const auto bufferInfo = makeDescriptorBufferInfo(computeBuffer->get(), 0ull, bufferSize);
				vkd.cmdPushDescriptorSetWithTemplateKHR(cmdBuffer, computeUpdateTemplate.get(), computePipelineLayout.get(), 0u, &bufferInfo);
			}
			else
				DE_ASSERT(false);
			boundComputeSet = true;
			break;

		case SetupOp::BIND_RAYTRACING_SET:
			if (m_params.rayTracingSetUpdateType == SetUpdateType::WRITE)
				vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipelineLayout.get(), 0u, 1u, &rayTracingDescriptorSet.get(), 0u, nullptr);
			else if (m_params.rayTracingSetUpdateType == SetUpdateType::PUSH)
				pushBufferDescriptor(vkd, cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipelineLayout.get(), rayTracingBuffer->get(), 0ull, bufferSize);
			else if (m_params.rayTracingSetUpdateType == SetUpdateType::PUSH_WITH_TEMPLATE)
			{
				const auto bufferInfo = makeDescriptorBufferInfo(rayTracingBuffer->get(), 0ull, bufferSize);
				vkd.cmdPushDescriptorSetWithTemplateKHR(cmdBuffer, rayTracingUpdateTemplate.get(), rayTracingPipelineLayout.get(), 0u, &bufferInfo);
			}
			else
				DE_ASSERT(false);
			boundRayTracingSet = true;
			break;

		default:
			DE_ASSERT(false);
			break;
		}
	}

	// Avoid warning in release builds.
	DE_UNREF(boundGraphicsPipeline);
	DE_UNREF(boundGraphicsSet);
	DE_UNREF(boundComputePipeline);
	DE_UNREF(boundComputeSet);
	DE_UNREF(boundRayTracingPipeline);
	DE_UNREF(boundRayTracingSet);

	// Dispatch operations in desired order.
	for (int i = 0; i < decltype(m_params.dispatchSequence)::SIZE; ++i)
	{
		const auto& dispatchOp = m_params.dispatchSequence[i];
		switch (dispatchOp)
		{
		case DispatchOp::DRAW:
			DE_ASSERT(boundGraphicsPipeline && boundGraphicsSet);
			beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissor, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
			vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
			endRenderPass(vkd, cmdBuffer);
			break;

		case DispatchOp::COMPUTE:
			DE_ASSERT(boundComputePipeline && boundComputeSet);
			vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
			break;

		case DispatchOp::TRACE_RAYS:
			DE_ASSERT(boundRayTracingPipeline && boundRayTracingSet);
			cmdTraceRays(vkd, cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, 1u, 1u, 1u);
			break;

		default:
			DE_ASSERT(false);
			break;
		}
	}

	if (hasGraphics)
	{
		const auto graphicsBufferBarrier = makeBufferBarrier(graphicsBuffer->get(), 0ull, bufferSize);
		recordBufferBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, graphicsBufferBarrier);
	}
	if (hasCompute)
	{
		const auto computeBufferBarrier = makeBufferBarrier(computeBuffer->get(), 0ull, bufferSize);
		recordBufferBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, computeBufferBarrier);
	}
	if (hasRayTracing)
	{
		const auto rayTracingBufferBarrier = makeBufferBarrier(rayTracingBuffer->get(), 0ull, bufferSize);
		recordBufferBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, rayTracingBufferBarrier);
	}

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify storage buffers.
	if (hasGraphics)	verifyBufferContents(vkd, device, *graphicsBuffer, "graphics", kExpectedBufferValueGraphics);
	if (hasCompute)		verifyBufferContents(vkd, device, *computeBuffer, "compute", kExpectedBufferValueCompute);
	if (hasRayTracing)	verifyBufferContents(vkd, device, *rayTracingBuffer, "raytracing", kExpectedBufferValueRayTracing);

	// Verify color attachment.
	if (hasGraphics)
	{
		const auto		textureLevel	= readColorAttachment(vkd, device, queue, qIndex, alloc, colorAttachment->get(), imageFormat, tcu::UVec2(imageExtent.width, imageExtent.height));
		const auto		pixelBuffer		= textureLevel->getAccess();
		const auto		iWidth			= static_cast<int>(imageExtent.width);
		const auto		iHeight			= static_cast<int>(imageExtent.height);
		const tcu::Vec4	expectedColor	(0.0f, 1.0f, 0.0f, 1.0f);

		for (int y = 0; y < iHeight; ++y)
		for (int x = 0; x < iWidth; ++x)
		{
			const auto value = pixelBuffer.getPixel(x, y);
			if (value != expectedColor)
			{
				std::ostringstream msg;
				msg << "Unexpected color found in attachment: expected " << expectedColor << " but found " << value;
				TCU_FAIL(msg.str());
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

// Auxiliar string conversion functions.

std::string toString(SetUpdateType updateType)
{
	switch (updateType)
	{
	case SetUpdateType::WRITE:				return "write";
	case SetUpdateType::PUSH:				return "push";
	case SetUpdateType::PUSH_WITH_TEMPLATE:	return "template_push";
	default:								DE_ASSERT(false); break;
	}

	return "";
}

std::string toString(const SetupSequence& setupSequence)
{
	std::ostringstream out;

	out << "setup";
	for (int i = 0; i < std::remove_reference<decltype(setupSequence)>::type::SIZE; ++i)
	{
		out << "_";
		switch (setupSequence[i])
		{
		case SetupOp::BIND_GRAPHICS_PIPELINE:	out << "gp";		break;
		case SetupOp::BIND_COMPUTE_PIPELINE:	out << "cp";		break;
		case SetupOp::BIND_RAYTRACING_PIPELINE:	out << "rp";		break;
		case SetupOp::BIND_GRAPHICS_SET:		out << "gs";		break;
		case SetupOp::BIND_COMPUTE_SET:			out << "cs";		break;
		case SetupOp::BIND_RAYTRACING_SET:		out << "rs";		break;
		default:								DE_ASSERT(false);	break;
		}
	}

	return out.str();
}

std::string toString(const DispatchSequence& dispatchSequence)
{
	std::ostringstream out;

	out << "cmd";
	for (int i = 0; i < std::remove_reference<decltype(dispatchSequence)>::type::SIZE; ++i)
	{
		out << "_";
		switch (dispatchSequence[i])
		{
		case DispatchOp::COMPUTE:		out << "dispatch";	break;
		case DispatchOp::DRAW:			out << "draw";		break;
		case DispatchOp::TRACE_RAYS:	out << "tracerays";	break;
		default:						DE_ASSERT(false);	break;
		}
	}

	return out.str();
}

std::string toString(VkPipelineBindPoint point)
{
	if (point == VK_PIPELINE_BIND_POINT_GRAPHICS)			return "graphics";
	if (point == VK_PIPELINE_BIND_POINT_COMPUTE)			return "compute";
	if (point == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)	return "raytracing";

	DE_ASSERT(false);
	return "";
}

} // anonymous

tcu::TestCaseGroup* createBindPointTests (tcu::TestContext& testCtx)
{
	using GroupPtr		= de::MovePtr<tcu::TestCaseGroup>;
	using BindPointPair	= tcu::Vector<VkPipelineBindPoint, kTestBindPoints>;

	GroupPtr bindPointGroup(new tcu::TestCaseGroup(testCtx, "bind_point", "Tests checking bind points are independent and used properly"));

	// Bind point combinations to test.
	const BindPointPair testPairs[] =
	{
		BindPointPair(VK_PIPELINE_BIND_POINT_GRAPHICS,	VK_PIPELINE_BIND_POINT_COMPUTE),
		BindPointPair(VK_PIPELINE_BIND_POINT_GRAPHICS,	VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR),
		BindPointPair(VK_PIPELINE_BIND_POINT_COMPUTE,	VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR),
	};

	for (int testPairIdx = 0; testPairIdx < DE_LENGTH_OF_ARRAY(testPairs); ++testPairIdx)
	{
		const auto& testPair = testPairs[testPairIdx];

		// Default values. Two of them will be overwritten later.
		TestParams params;
		params.graphicsSetUpdateType	= SetUpdateType::TYPE_COUNT;
		params.computeSetUpdateType		= SetUpdateType::TYPE_COUNT;
		params.rayTracingSetUpdateType	= SetUpdateType::TYPE_COUNT;

		// What to test based on the test pair.
		// Note: updateTypePtrs will tell us which of the set update type members above we need to vary (graphics, compute, ray tracing).
		SetUpdateType*	updateTypePtrs	[kTestBindPoints] = {	nullptr,				nullptr					};
		SetupOp			pipelineBinds	[kTestBindPoints] = {	SetupOp::OP_COUNT,		SetupOp::OP_COUNT		};
		SetupOp			setBinds		[kTestBindPoints] = {	SetupOp::OP_COUNT,		SetupOp::OP_COUNT		};
		DispatchOp		dispatches		[kTestBindPoints] = {	DispatchOp::OP_COUNT,	DispatchOp::OP_COUNT	};

		for (int elemIdx = 0; elemIdx < std::remove_reference<decltype(testPair)>::type::SIZE; ++elemIdx)
		{
			if (testPair[elemIdx] == VK_PIPELINE_BIND_POINT_GRAPHICS)
			{
				updateTypePtrs[elemIdx]	= &params.graphicsSetUpdateType;	// Test different graphics set update types.
				pipelineBinds[elemIdx]	= SetupOp::BIND_GRAPHICS_PIPELINE;
				setBinds[elemIdx]		= SetupOp::BIND_GRAPHICS_SET;
				dispatches[elemIdx]		= DispatchOp::DRAW;
			}
			else if (testPair[elemIdx] == VK_PIPELINE_BIND_POINT_COMPUTE)
			{
				updateTypePtrs[elemIdx]	= &params.computeSetUpdateType;		// Test different compute set update types.
				pipelineBinds[elemIdx]	= SetupOp::BIND_COMPUTE_PIPELINE;
				setBinds[elemIdx]		= SetupOp::BIND_COMPUTE_SET;
				dispatches[elemIdx]		= DispatchOp::COMPUTE;
			}
			else if (testPair[elemIdx] == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
			{
				updateTypePtrs[elemIdx]	= &params.rayTracingSetUpdateType;	// Test different ray tracing set update types.
				pipelineBinds[elemIdx]	= SetupOp::BIND_RAYTRACING_PIPELINE;
				setBinds[elemIdx]		= SetupOp::BIND_RAYTRACING_SET;
				dispatches[elemIdx]		= DispatchOp::TRACE_RAYS;
			}
		}

		const std::string	pairName	= toString(testPair[0]) + "_" + toString(testPair[1]);
		GroupPtr			pairGroup	(new tcu::TestCaseGroup(testCtx, pairName.c_str(), ""));

		// Combine two update types.
		for (int firstUpdateTypeIdx = 0; firstUpdateTypeIdx < static_cast<int>(SetUpdateType::TYPE_COUNT); ++firstUpdateTypeIdx)
		for (int secondUpdateTypeIdx = 0; secondUpdateTypeIdx < static_cast<int>(SetUpdateType::TYPE_COUNT); ++ secondUpdateTypeIdx)
		{
			const auto			firstUpdateType		= static_cast<SetUpdateType>(firstUpdateTypeIdx);
			const auto			secondUpdateType	= static_cast<SetUpdateType>(secondUpdateTypeIdx);
			const std::string	updateGroupName		= toString(firstUpdateType) + "_" + toString(secondUpdateType);
			GroupPtr			updateGroup			(new tcu::TestCaseGroup(testCtx, updateGroupName.c_str(), ""));

			// Change update types of the relevant sets.
			*updateTypePtrs[0] = firstUpdateType;
			*updateTypePtrs[1] = secondUpdateType;

			// Prepare initial permutation of test parameters.
			params.setupSequence[0] = pipelineBinds[0];
			params.setupSequence[1] = pipelineBinds[1];
			params.setupSequence[2] = setBinds[0];
			params.setupSequence[3] = setBinds[1];

			// Permutate setup sequence and dispatch sequence.
			const auto ssBegin	= params.setupSequence.m_data;
			const auto ssEnd	= ssBegin + decltype(params.setupSequence)::SIZE;
			do
			{
				const auto	setupGroupName	= toString(params.setupSequence);
				GroupPtr	setupGroup		(new tcu::TestCaseGroup(testCtx, setupGroupName.c_str(), ""));

				// Reset dispatch sequence permutation.
				params.dispatchSequence = dispatches;

				const auto dsBegin	= params.dispatchSequence.m_data;
				const auto dsEnd	= dsBegin + decltype(params.dispatchSequence)::SIZE;
				do
				{
					const auto testName = toString(params.dispatchSequence);
					setupGroup->addChild(new BindPointTest(testCtx, testName, "", params));
				} while (std::next_permutation(dsBegin, dsEnd));

				updateGroup->addChild(setupGroup.release());

			} while (std::next_permutation(ssBegin, ssEnd));

			pairGroup->addChild(updateGroup.release());
		}

		bindPointGroup->addChild(pairGroup.release());
	}


	return bindPointGroup.release();
}

} // pipeline
} // vkt

