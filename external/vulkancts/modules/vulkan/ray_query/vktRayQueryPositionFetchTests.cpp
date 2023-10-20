/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 NVIDIA Corporation.
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
 * \brief Ray Query Position Fetch Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryPositionFetchTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include "tcuVectorUtil.hpp"

#include <sstream>
#include <vector>
#include <iostream>

namespace vkt
{
namespace RayQuery
{

namespace
{

using namespace vk;

enum ShaderSourcePipeline
{
	SSP_GRAPHICS_PIPELINE,
	SSP_COMPUTE_PIPELINE,
	SSP_RAY_TRACING_PIPELINE
};

enum ShaderSourceType
{
	SST_VERTEX_SHADER,
	SST_COMPUTE_SHADER,
	SST_RAY_GENERATION_SHADER,
};

enum TestFlagBits
{
	TEST_FLAG_BIT_INSTANCE_TRANSFORM				= 1U << 0,
	TEST_FLAG_BIT_LAST								= 1U << 1,
};

std::vector<std::string> testFlagBitNames =
{
	"instance_transform",
};

struct TestParams
{
	ShaderSourceType		shaderSourceType;
	ShaderSourcePipeline	shaderSourcePipeline;
	vk::VkAccelerationStructureBuildTypeKHR	buildType;		// are we making AS on CPU or GPU
	VkFormat								vertexFormat;
	deUint32								testFlagMask;
};

static constexpr deUint32 kNumThreadsAtOnce = 1024;


class PositionFetchCase : public TestCase
{
public:
							PositionFetchCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~PositionFetchCase	(void) {}

	virtual void			checkSupport				(Context& context) const;
	virtual void			initPrograms				(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;

protected:
	TestParams				m_params;
};

class PositionFetchInstance : public TestInstance
{
public:
								PositionFetchInstance		(Context& context, const TestParams& params);
	virtual						~PositionFetchInstance	(void) {}

	virtual tcu::TestStatus		iterate							(void);

protected:
	TestParams					m_params;
};

PositionFetchCase::PositionFetchCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
{}

void PositionFetchCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_ray_query");
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_position_fetch");

	const VkPhysicalDeviceRayQueryFeaturesKHR& rayQueryFeaturesKHR = context.getRayQueryFeatures();
	if (rayQueryFeaturesKHR.rayQuery == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR& accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	if (m_params.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR && accelerationStructureFeaturesKHR.accelerationStructureHostCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");

	const VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR& rayTracingPositionFetchFeaturesKHR = context.getRayTracingPositionFetchFeatures();
	if (rayTracingPositionFetchFeaturesKHR.rayTracingPositionFetch == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDevicePositionFetchFeaturesKHR.rayTracingPositionFetch");

	// Check supported vertex format.
	checkAccelerationStructureVertexBufferFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.vertexFormat);

	if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
	{
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

		const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();

		if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
			TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
	}

	switch (m_params.shaderSourceType)
	{
	case SST_VERTEX_SHADER:
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
		break;
	default:
		break;
	}
}

void PositionFetchCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions buildOptions (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	deUint32 numRays = 1; // XXX

	std::ostringstream sharedHeader;
	sharedHeader
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_query : require\n"
		<< "#extension GL_EXT_ray_tracing_position_fetch : require\n"
		<< "\n"
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1, std430) buffer RayOrigins {\n"
		<< "  vec4 values[" << numRays << "];\n"
		<< "} origins;\n"
		<< "layout(set=0, binding=2, std430) buffer OutputPositions {\n"
		<< "  vec4 values[" << 3*numRays << "];\n"
		<< "} modes;\n";

	std::ostringstream mainLoop;
	mainLoop
		<< "  while (index < " << numRays << ") {\n"
		//<< "     for (int i=0; i<3; i++) {\n"
		//<< "       modes.values[3*index.x+i] = vec4(i, 0.0, 5.0, 1.0);\n"
		//<< "     }\n"
		<< "    const uint  cullMask  = 0xFF;\n"
		<< "    const vec3  origin    = origins.values[index].xyz;\n"
		<< "    const vec3  direction = vec3(0.0, 0.0, -1.0);\n"
		<< "    const float tMin      = 0.0f;\n"
		<< "    const float tMax      = 2.0f;\n"
		<< "    rayQueryEXT rq;\n"
		<< "    rayQueryInitializeEXT(rq, topLevelAS, gl_RayFlagsNoneEXT, cullMask, origin, tMin, direction, tMax);\n"
		<< "    while (rayQueryProceedEXT(rq)) {\n"
		<< "      if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {\n"
		<< "        vec3 outputVal[3];\n"
		<< "        rayQueryGetIntersectionTriangleVertexPositionsEXT(rq, false, outputVal);\n"
		<< "        for (int i=0; i<3; i++) {\n"
		<< "           modes.values[3*index.x+i] = vec4(outputVal[i], 0);\n"
//		<< "           modes.values[3*index.x+i] = vec4(1.0, 1.0, 1.0, 0);\n"
		<< "        }\n"
		<< "      }\n"
		<< "    }\n"
		<< "    index += " << kNumThreadsAtOnce << ";\n"
		<< "  }\n";

	if (m_params.shaderSourceType == SST_VERTEX_SHADER) {
		std::ostringstream vert;
		vert
			<< sharedHeader.str()
			<< "void main()\n"
			<< "{\n"
			<< "  uint index             = gl_VertexIndex.x;\n"
			<< mainLoop.str()
			<< "}\n"
			;

		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str()) << buildOptions;
	}
	else if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
	{
		std::ostringstream rgen;
		rgen
			<< sharedHeader.str()
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "void main()\n"
			<< "{\n"
			<< "  uint index             = gl_LaunchIDEXT.x;\n"
			<< mainLoop.str()
			<< "}\n"
			;

		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
	}
	else
	{
		DE_ASSERT(m_params.shaderSourceType == SST_COMPUTE_SHADER);
		std::ostringstream comp;
		comp
			<< sharedHeader.str()
			<< "layout(local_size_x=1024, local_size_y=1, local_size_z=1) in;\n"
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			<< "  uint index             = gl_LocalInvocationID.x;\n"
			<< mainLoop.str()
			<< "}\n"
			;

		programCollection.glslSources.add("comp") << glu::ComputeSource(updateRayTracingGLSL(comp.str())) << buildOptions;
	}
}

TestInstance* PositionFetchCase::createInstance (Context& context) const
{
	return new PositionFetchInstance(context, m_params);
}

PositionFetchInstance::PositionFetchInstance (Context& context, const TestParams& params)
	: TestInstance	(context)
	, m_params		(params)
{}

static Move<VkRenderPass> makeEmptyRenderPass(const DeviceInterface& vk,
	const VkDevice				device)
{
	std::vector<VkSubpassDescription>	subpassDescriptions;

	const VkSubpassDescription	description =
	{
		(VkSubpassDescriptionFlags)0,		//  VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	//  VkPipelineBindPoint				pipelineBindPoint;
		0u,									//  deUint32						inputAttachmentCount;
		DE_NULL,							//  const VkAttachmentReference*	pInputAttachments;
		0u,									//  deUint32						colorAttachmentCount;
		DE_NULL,							//  const VkAttachmentReference*	pColorAttachments;
		DE_NULL,							//  const VkAttachmentReference*	pResolveAttachments;
		DE_NULL,							//  const VkAttachmentReference*	pDepthStencilAttachment;
		0,									//  deUint32						preserveAttachmentCount;
		DE_NULL								//  const deUint32*					pPreserveAttachments;
	};
	subpassDescriptions.push_back(description);

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,							//  VkStructureType					sType;
		DE_NULL,															//  const void*						pNext;
		static_cast<VkRenderPassCreateFlags>(0u),							//  VkRenderPassCreateFlags			flags;
		0u,																	//  deUint32						attachmentCount;
		DE_NULL,															//  const VkAttachmentDescription*	pAttachments;
		static_cast<deUint32>(subpassDescriptions.size()),					//  deUint32						subpassCount;
		&subpassDescriptions[0],											//  const VkSubpassDescription*		pSubpasses;
		0u,																	//  deUint32						dependencyCount;
		DE_NULL																//  const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

static Move<VkFramebuffer> makeFramebuffer (const DeviceInterface& vk, const VkDevice device, VkRenderPass renderPass, uint32_t width, uint32_t height)
{
	const vk::VkFramebufferCreateInfo	framebufferParams =
	{
		vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,					// sType
		DE_NULL,														// pNext
		(vk::VkFramebufferCreateFlags)0,
		renderPass,														// renderPass
		0u,																// attachmentCount
		DE_NULL,														// pAttachments
		width,															// width
		height,															// height
		1u,																// layers
	};

	return createFramebuffer(vk, device, &framebufferParams);
}

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface& vk,
	const VkDevice				device,
	const VkPipelineLayout		pipelineLayout,
	const VkRenderPass			renderPass,
	const VkShaderModule		vertexModule,
	const deUint32				subpass)
{
	VkExtent2D												renderSize { 256, 256 };
	VkViewport												viewport = makeViewport(renderSize);
	VkRect2D												scissor = makeRect2D(renderSize);

	const VkPipelineViewportStateCreateInfo					viewportStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,		// VkStructureType                             sType
		DE_NULL,													// const void*                                 pNext
		(VkPipelineViewportStateCreateFlags)0,						// VkPipelineViewportStateCreateFlags          flags
		1u,															// deUint32                                    viewportCount
		&viewport,													// const VkViewport*                           pViewports
		1u,															// deUint32                                    scissorCount
		&scissor													// const VkRect2D*                             pScissors
	};

	const VkPipelineInputAssemblyStateCreateInfo			inputAssemblyStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType
		DE_NULL,														// const void*                                pNext
		0u,																// VkPipelineInputAssemblyStateCreateFlags    flags
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,								// VkPrimitiveTopology                        topology
		VK_FALSE														// VkBool32                                   primitiveRestartEnable
	};

	const VkPipelineVertexInputStateCreateInfo				vertexInputStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,									//  VkStructureType									sType
		DE_NULL,																					//  const void*										pNext
		(VkPipelineVertexInputStateCreateFlags)0,													//  VkPipelineVertexInputStateCreateFlags			flags
		0u,																							//  deUint32										vertexBindingDescriptionCount
		DE_NULL,																					//  const VkVertexInputBindingDescription*			pVertexBindingDescriptions
		0u,																							//  deUint32										vertexAttributeDescriptionCount
		DE_NULL,																					//  const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
	};

	const VkPipelineRasterizationStateCreateInfo			rasterizationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//  VkStructureType							sType
		DE_NULL,													//  const void*								pNext
		0u,															//  VkPipelineRasterizationStateCreateFlags	flags
		VK_FALSE,													//  VkBool32								depthClampEnable
		VK_TRUE,													//  VkBool32								rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										//  VkPolygonMode							polygonMode
		VK_CULL_MODE_NONE,											//  VkCullModeFlags							cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							//  VkFrontFace								frontFace
		VK_FALSE,													//  VkBool32								depthBiasEnable
		0.0f,														//  float									depthBiasConstantFactor
		0.0f,														//  float									depthBiasClamp
		0.0f,														//  float									depthBiasSlopeFactor
		1.0f														//  float									lineWidth
	};

	return makeGraphicsPipeline(vk,									// const DeviceInterface&							vk
		device,								// const VkDevice									device
		pipelineLayout,						// const VkPipelineLayout							pipelineLayout
		vertexModule,						// const VkShaderModule								vertexShaderModule
		DE_NULL,							// const VkShaderModule								tessellationControlModule
		DE_NULL,							// const VkShaderModule								tessellationEvalModule
		DE_NULL,							// const VkShaderModule								geometryShaderModule
		DE_NULL,							// const VkShaderModule								fragmentShaderModule
		renderPass,							// const VkRenderPass								renderPass
		subpass,							// const deUint32									subpass
		&vertexInputStateCreateInfo,		// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
		&inputAssemblyStateCreateInfo,		// const VkPipelineInputAssemblyStateCreateInfo*	inputAssemblyStateCreateInfo
		DE_NULL,							// const VkPipelineTessellationStateCreateInfo*		tessStateCreateInfo
		&viewportStateCreateInfo,			// const VkPipelineViewportStateCreateInfo*			viewportStateCreateInfo
		&rasterizationStateCreateInfo);	// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
}

tcu::TestStatus PositionFetchInstance::iterate (void)
{
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	auto&		alloc	= m_context.getDefaultAllocator();
	const auto	qIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	queue	= m_context.getUniversalQueue();

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Build acceleration structures.
	auto topLevelAS		= makeTopLevelAccelerationStructure();
	auto bottomLevelAS	= makeBottomLevelAccelerationStructure();

	const std::vector<tcu::Vec3> triangle =
	{
		tcu::Vec3(0.0f, 0.0f, 0.0f),
		tcu::Vec3(1.0f, 0.0f, 0.0f),
		tcu::Vec3(0.0f, 1.0f, 0.0f),
	};

	const VkTransformMatrixKHR notQuiteIdentityMatrix3x4 = { { { 0.98f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.97f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.99f, 0.0f } } };

	de::SharedPtr<RaytracedGeometryBase> geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, m_params.vertexFormat, VK_INDEX_TYPE_NONE_KHR);

	for (auto & v : triangle) {
		geometry->addVertex(v);
	}

	bottomLevelAS->addGeometry(geometry);
	bottomLevelAS->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR);
	bottomLevelAS->setBuildType(m_params.buildType);
	bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);
	de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr (bottomLevelAS.release());

	topLevelAS->setInstanceCount(1);
	topLevelAS->setBuildType(m_params.buildType);
	topLevelAS->addInstance(blasSharedPtr, (m_params.testFlagMask & TEST_FLAG_BIT_INSTANCE_TRANSFORM) ? notQuiteIdentityMatrix3x4 : identityMatrix3x4);
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	// One ray for this test
	// XXX Should it be multiple triangles and one ray per triangle for more coverage?
	// XXX If it's really one ray, the origin buffer is complete overkill
	deUint32 numRays = 1; // XXX

	// SSBO buffer for origins.
	const auto originsBufferSize		= static_cast<VkDeviceSize>(sizeof(tcu::Vec4) * numRays);
	const auto originsBufferInfo		= makeBufferCreateInfo(originsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory originsBuffer	(vkd, device, alloc, originsBufferInfo, MemoryRequirement::HostVisible);
	auto& originsBufferAlloc			= originsBuffer.getAllocation();
	void* originsBufferData				= originsBufferAlloc.getHostPtr();

	std::vector<tcu::Vec4> origins;
	std::vector<tcu::Vec3> expectedOutputPositions;
	origins.reserve(numRays);
	expectedOutputPositions.reserve(3*numRays);

	// Fill in vector of expected outputs
	for (deUint32 index = 0; index < numRays; index++) {
		for (deUint32 vert = 0; vert < 3; vert++) {
			tcu::Vec3 pos = triangle[vert];

			expectedOutputPositions.push_back(pos);
		}
	}

	// XXX Arbitrary location and see above
	for (deUint32 index = 0; index < numRays; index++) {
		origins.push_back(tcu::Vec4(0.25, 0.25, 1.0, 0.0));
	}

	const auto				originsBufferSizeSz = static_cast<size_t>(originsBufferSize);
	deMemcpy(originsBufferData, origins.data(), originsBufferSizeSz);
	flushAlloc(vkd, device, originsBufferAlloc);

	// Storage buffer for output modes
	const auto outputPositionsBufferSize = static_cast<VkDeviceSize>(3 * 4 * sizeof(float) * numRays);
	const auto outputPositionsBufferInfo = makeBufferCreateInfo(outputPositionsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory outputPositionsBuffer(vkd, device, alloc, outputPositionsBufferInfo, MemoryRequirement::HostVisible);
	auto& outputPositionsBufferAlloc = outputPositionsBuffer.getAllocation();
	void* outputPositionsBufferData = outputPositionsBufferAlloc.getHostPtr();
	deMemset(outputPositionsBufferData, 0xFF, static_cast<size_t>(outputPositionsBufferSize));
	flushAlloc(vkd, device, outputPositionsBufferAlloc);

	// Descriptor set layout.
	DescriptorSetLayoutBuilder dsLayoutBuilder;
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL);
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
	const auto setLayout = dsLayoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	// Update descriptor set.
	{
		const VkWriteDescriptorSetAccelerationStructureKHR accelDescInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			nullptr,
			1u,
			topLevelAS.get()->getPtr(),
		};
		const auto inStorageBufferInfo = makeDescriptorBufferInfo(originsBuffer.get(), 0ull, VK_WHOLE_SIZE);
		const auto storageBufferInfo = makeDescriptorBufferInfo(outputPositionsBuffer.get(), 0ull, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inStorageBufferInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo);
		updateBuilder.update(vkd, device);
	}

	Move<VkPipeline>				pipeline;
	de::MovePtr<BufferWithMemory>	raygenSBT;
	Move<VkRenderPass>				renderPass;
	Move<VkFramebuffer>				framebuffer;

	if (m_params.shaderSourceType == SST_VERTEX_SHADER)
	{
		auto vertexModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0);

		const uint32_t width = 32u;
		const uint32_t height = 32u;
		renderPass = makeEmptyRenderPass(vkd, device);
		framebuffer = makeFramebuffer(vkd, device, *renderPass, width, height);
		pipeline = makeGraphicsPipeline(vkd, device, *pipelineLayout, *renderPass, *vertexModule, 0);

		const VkRenderPassBeginInfo			renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,							// VkStructureType								sType;
			DE_NULL,															// const void*									pNext;
			*renderPass,														// VkRenderPass									renderPass;
			*framebuffer,														// VkFramebuffer								framebuffer;
			makeRect2D(width, height),											// VkRect2D										renderArea;
			0u,																	// uint32_t										clearValueCount;
			DE_NULL																// const VkClearValue*							pClearValues;
		};

		vkd.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
		vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		vkd.cmdDraw(cmdBuffer, kNumThreadsAtOnce, 1, 0, 0);
		vkd.cmdEndRenderPass(cmdBuffer);
	}
	else if (m_params.shaderSourceType == SST_RAY_GENERATION_SHADER)
	{
		const auto& vki = m_context.getInstanceInterface();
		const auto	physDev = m_context.getPhysicalDevice();

		// Shader module.
		auto rgenModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);

		// Get some ray tracing properties.
		deUint32 shaderGroupHandleSize = 0u;
		deUint32 shaderGroupBaseAlignment = 1u;
		{
			const auto rayTracingPropertiesKHR = makeRayTracingProperties(vki, physDev);
			shaderGroupHandleSize = rayTracingPropertiesKHR->getShaderGroupHandleSize();
			shaderGroupBaseAlignment = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
		}

		auto raygenSBTRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
		auto unusedSBTRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

		{
			const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
			rayTracingPipeline->setCreateFlags(VK_PIPELINE_CREATE_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0);

			pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

			raygenSBT = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
			raygenSBTRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		}

		// Trace rays.
		vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
		vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &unusedSBTRegion, &unusedSBTRegion, &unusedSBTRegion, kNumThreadsAtOnce, 1u, 1u);
	}
	else
	{
		DE_ASSERT(m_params.shaderSourceType == SST_COMPUTE_SHADER);
		// Shader module.
		const auto compModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0);

		// Pipeline.
		const VkPipelineShaderStageCreateInfo shaderInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
			nullptr,												//	const void*							pNext;
			0u,														//	VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							//	VkShaderStageFlagBits				stage;
			compModule.get(),										//	VkShaderModule						module;
			"main",													//	const char*							pName;
			nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
		};
		const VkComputePipelineCreateInfo pipelineInfo =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	//	VkStructureType					sType;
			nullptr,										//	const void*						pNext;
			0u,												//	VkPipelineCreateFlags			flags;
			shaderInfo,										//	VkPipelineShaderStageCreateInfo	stage;
			pipelineLayout.get(),							//	VkPipelineLayout				layout;
			DE_NULL,										//	VkPipeline						basePipelineHandle;
			0,												//	deInt32							basePipelineIndex;
		};
		pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineInfo);

		// Dispatch work with ray queries.
		vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
		vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
		vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
	}

	// Barrier for the output buffer.
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify results.
	std::vector<tcu::Vec4>	outputData(expectedOutputPositions.size());
	const auto				outputPositionsBufferSizeSz = static_cast<size_t>(outputPositionsBufferSize);

	invalidateAlloc(vkd, device, outputPositionsBufferAlloc);
	DE_ASSERT(de::dataSize(outputData) == outputPositionsBufferSizeSz);
	deMemcpy(outputData.data(), outputPositionsBufferData, outputPositionsBufferSizeSz);

	for (size_t i = 0; i < outputData.size(); ++i)
	{
		/*const */ auto& outVal = outputData[i]; // Should be const but .xyz() isn't
		tcu::Vec3 outVec3 = outVal.xyz();
		const auto& expectedVal = expectedOutputPositions[i];
		const auto& diff = expectedOutputPositions[i] - outVec3;
		float len = dot(diff, diff);

		// XXX Find a better epsilon
		if (!(len < 1e-5))
		{
			std::ostringstream msg;
			msg << "Unexpected value found for element " << i << ": expected " << expectedVal << " and found " << outVal << ";";
			TCU_FAIL(msg.str());
		}
#if 0
		else
		{
			std::ostringstream msg;
			msg << "Expected value found for element " << i << ": expected " << expectedVal << " and found " << outVal << ";\n";
			std::cout << msg.str();
		}
#endif
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup*	createPositionFetchTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "position_fetch", "Test ray pipeline shaders using position fetch"));

	struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		const char* name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};


	const struct
	{
		ShaderSourceType						shaderSourceType;
		ShaderSourcePipeline					shaderSourcePipeline;
		std::string								name;
	} shaderSourceTypes[] =
	{
		{ SST_VERTEX_SHADER,					SSP_GRAPHICS_PIPELINE,		"vertex_shader"				},
		{ SST_COMPUTE_SHADER,					SSP_COMPUTE_PIPELINE,		"compute_shader",			},
		{ SST_RAY_GENERATION_SHADER,			SSP_RAY_TRACING_PIPELINE,	"rgen_shader",				},
	};

	const VkFormat vertexFormats[] =
	{
		// Mandatory formats.
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16B16A16_SNORM,

		// Additional formats.
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_SFLOAT,
	};

	for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name.c_str(), ""));

		for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name, ""));

			for (size_t vertexFormatNdx = 0; vertexFormatNdx < DE_LENGTH_OF_ARRAY(vertexFormats); ++vertexFormatNdx)
			{
				const auto format = vertexFormats[vertexFormatNdx];
				const auto formatName = getFormatSimpleName(format);

				de::MovePtr<tcu::TestCaseGroup> vertexFormatGroup(new tcu::TestCaseGroup(group->getTestContext(), formatName.c_str(), ""));

				for (deUint32 testFlagMask = 0; testFlagMask < TEST_FLAG_BIT_LAST; testFlagMask++)
				{
					std::string maskName = "";

					for (deUint32 bit = 0; bit < testFlagBitNames.size(); bit++)
					{
						if (testFlagMask & (1 << bit))
						{
							if (maskName != "")
								maskName += "_";
							maskName += testFlagBitNames[bit];
						}
					}
					if (maskName == "")
						maskName = "NoFlags";

					de::MovePtr<tcu::TestCaseGroup> testFlagGroup(new tcu::TestCaseGroup(group->getTestContext(), maskName.c_str(), ""));

					TestParams testParams
					{
						shaderSourceTypes[shaderSourceNdx].shaderSourceType,
						shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
						buildTypes[buildTypeNdx].buildType,
						format,
						testFlagMask,
					};

					vertexFormatGroup->addChild(new PositionFetchCase(testCtx, maskName, "", testParams));
				}
				buildGroup->addChild(vertexFormatGroup.release());
			}
			sourceTypeGroup->addChild(buildGroup.release());
		}
		group->addChild(sourceTypeGroup.release());
	}

	return group.release();
}
} // RayQuery
} // vkt

