/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Test procedural geometry with complex bouding box sets
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingProceduralGeometryTests.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuFloat.hpp"

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace vkt;

static const VkFlags	ALL_RAY_TRACING_STAGES	= VK_SHADER_STAGE_RAYGEN_BIT_KHR
												| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
												| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
												| VK_SHADER_STAGE_MISS_BIT_KHR
												| VK_SHADER_STAGE_INTERSECTION_BIT_KHR
												| VK_SHADER_STAGE_CALLABLE_BIT_KHR;

enum class TestType
{
	OBJECT_BEHIND_BOUNDING_BOX = 0,
	TRIANGLE_IN_BETWEEN
};

struct DeviceHelper
{
	Move<VkDevice>					device;
	de::MovePtr<DeviceDriver>		vkd;
	deUint32						queueFamilyIndex;
	VkQueue							queue;
	de::MovePtr<SimpleAllocator>	allocator;

	DeviceHelper (Context& context)
	{
		const auto&	vkp					= context.getPlatformInterface();
		const auto&	vki					= context.getInstanceInterface();
		const auto	instance			= context.getInstance();
		const auto	physicalDevice		= context.getPhysicalDevice();

		queueFamilyIndex = context.getUniversalQueueFamilyIndex();

		// Get device features (these have to be checked in the test case)
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR		rayTracingPipelineFeatures		= initVulkanStructure();
		VkPhysicalDeviceAccelerationStructureFeaturesKHR	accelerationStructureFeatures	= initVulkanStructure(&rayTracingPipelineFeatures);
		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR		deviceAddressFeatures			= initVulkanStructure(&accelerationStructureFeatures);
		VkPhysicalDeviceFeatures2							deviceFeatures					= initVulkanStructure(&deviceAddressFeatures);

		vki.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

		// Make sure robust buffer access is disabled as in the default device
		deviceFeatures.features.robustBufferAccess = VK_FALSE;

		const auto queuePriority = 1.0f;
		const VkDeviceQueueCreateInfo queueInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,			//	VkStructureType					sType;
			nullptr,											//	const void*						pNext;
			0u,													//	VkDeviceQueueCreateFlags		flags;
			queueFamilyIndex,									//	deUint32						queueFamilyIndex;
			1u,													//	deUint32						queueCount;
			&queuePriority,										//	const float*					pQueuePriorities;
		};

		// Required extensions - create device with VK_KHR_ray_tracing_pipeline but
		// without VK_KHR_pipeline_library to also test that that combination works
		std::vector<const char*> requiredExtensions
		{
			"VK_KHR_ray_tracing_pipeline",
			"VK_KHR_acceleration_structure",
			"VK_KHR_deferred_host_operations",
			"VK_KHR_buffer_device_address",
			"VK_EXT_descriptor_indexing",
			"VK_KHR_spirv_1_4",
			"VK_KHR_shader_float_controls"
		};

		const VkDeviceCreateInfo createInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,				//	VkStructureType					sType;
			deviceFeatures.pNext,								//	const void*						pNext;
			0u,													//	VkDeviceCreateFlags				flags;
			1u,													//	deUint32						queueCreateInfoCount;
			&queueInfo,											//	const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
			0u,													//	deUint32						enabledLayerCount;
			nullptr,											//	const char* const*				ppEnabledLayerNames;
			static_cast<deUint32>(requiredExtensions.size()),	//	deUint32						enabledExtensionCount;
			requiredExtensions.data(),							//	const char* const*				ppEnabledExtensionNames;
			&deviceFeatures.features,							//	const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		// Create custom device and related objects
		device		= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &createInfo);
		vkd			= de::MovePtr<DeviceDriver>(new DeviceDriver(vkp, instance, device.get(), context.getUsedApiVersion()));
		queue		= getDeviceQueue(*vkd, *device, queueFamilyIndex, 0u);
		allocator	= de::MovePtr<SimpleAllocator>(new SimpleAllocator(*vkd, device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}
};

class RayTracingProceduralGeometryTestBase : public TestInstance
{
public:

	RayTracingProceduralGeometryTestBase	(Context& context);
	~RayTracingProceduralGeometryTestBase	(void) = default;

	tcu::TestStatus iterate					(void) override;

protected:

	virtual void setupRayTracingPipeline() = 0;
	virtual void setupAccelerationStructures() = 0;

private:

	VkWriteDescriptorSetAccelerationStructureKHR	makeASWriteDescriptorSet	(const VkAccelerationStructureKHR* pAccelerationStructure);
	void											clearBuffer					(de::SharedPtr<BufferWithMemory> buffer, VkDeviceSize bufferSize);

protected:

	DeviceHelper													m_customDevice;
	de::MovePtr<RayTracingPipeline>									m_rayTracingPipeline;
	Move<VkPipelineLayout>											m_pipelineLayout;
	Move<VkPipeline>												m_pipeline;
	de::MovePtr<BufferWithMemory>									m_rgenShaderBT;
	de::MovePtr<BufferWithMemory>									m_chitShaderBT;
	de::MovePtr<BufferWithMemory>									m_missShaderBT;

	Move<VkDescriptorSetLayout>										m_descriptorSetLayout;
	Move<VkCommandPool>												m_cmdPool;
	Move<VkCommandBuffer>											m_cmdBuffer;

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	m_blasVect;
	de::SharedPtr<TopLevelAccelerationStructure>					m_referenceTLAS;
	de::SharedPtr<TopLevelAccelerationStructure>					m_resultTLAS;
};

RayTracingProceduralGeometryTestBase::RayTracingProceduralGeometryTestBase(Context& context)
	: vkt::TestInstance	(context)
	, m_customDevice	(context)
	, m_referenceTLAS	(makeTopLevelAccelerationStructure().release())
	, m_resultTLAS		(makeTopLevelAccelerationStructure().release())
{
}

tcu::TestStatus RayTracingProceduralGeometryTestBase::iterate(void)
{
	const DeviceInterface&	vkd					= *m_customDevice.vkd;
	const VkDevice			device				= *m_customDevice.device;
	const deUint32			queueFamilyIndex	= m_customDevice.queueFamilyIndex;
	const VkQueue			queue				= m_customDevice.queue;
	Allocator&				allocator			= *m_customDevice.allocator;
	const deUint32			sgHandleSize		= m_context.getRayTracingPipelineProperties().shaderGroupHandleSize;
	const deUint32			imageSize			= 64u;

	const Move<VkDescriptorPool> descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 2u)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
		.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

	m_descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)	// as with single/four aabb's
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES)				// ssbo with result/reference values
		.build(vkd, device);

	const Move<VkDescriptorSet>			referenceDescriptorSet	= makeDescriptorSet(vkd, device, *descriptorPool, *m_descriptorSetLayout);
	const Move<VkDescriptorSet>			resultDescriptorSet		= makeDescriptorSet(vkd, device, *descriptorPool, *m_descriptorSetLayout);

	const VkDeviceSize					resultBufferSize		= imageSize * imageSize * sizeof(int);
	const VkBufferCreateInfo			resultBufferCreateInfo	= makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	de::SharedPtr<BufferWithMemory>		referenceBuffer			= de::SharedPtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));
	de::SharedPtr<BufferWithMemory>		resultBuffer			= de::SharedPtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	m_rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

	setupRayTracingPipeline();

	const VkStridedDeviceAddressRegionKHR	rgenSBTR		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, m_rgenShaderBT->get(), 0), sgHandleSize, sgHandleSize);
	const VkStridedDeviceAddressRegionKHR	chitSBTR		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, m_chitShaderBT->get(), 0), sgHandleSize, sgHandleSize);
	const VkStridedDeviceAddressRegionKHR	missSBTR		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, m_missShaderBT->get(), 0), sgHandleSize, sgHandleSize);
	const VkStridedDeviceAddressRegionKHR	callableSBTR	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	m_cmdPool	= createCommandPool(vkd, device, 0, queueFamilyIndex);
	m_cmdBuffer	= allocateCommandBuffer(vkd, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// clear result and reference buffers
	clearBuffer(resultBuffer, resultBufferSize);
	clearBuffer(referenceBuffer, resultBufferSize);

	beginCommandBuffer(vkd, *m_cmdBuffer, 0u);
	{
		setupAccelerationStructures();

		// update descriptor sets
		{
			typedef DescriptorSetUpdateBuilder::Location DSL;

			const VkWriteDescriptorSetAccelerationStructureKHR	referenceAS		= makeASWriteDescriptorSet(m_referenceTLAS->getPtr());
			const VkDescriptorBufferInfo						referenceSSBO	= makeDescriptorBufferInfo(**referenceBuffer, 0u, VK_WHOLE_SIZE);
			DescriptorSetUpdateBuilder()
				.writeSingle(*referenceDescriptorSet, DSL::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &referenceAS)
				.writeSingle(*referenceDescriptorSet, DSL::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &referenceSSBO)
				.update(vkd, device);

			const VkWriteDescriptorSetAccelerationStructureKHR	resultAS	= makeASWriteDescriptorSet(m_resultTLAS->getPtr());
			const VkDescriptorBufferInfo						resultSSBO	= makeDescriptorBufferInfo(**resultBuffer, 0u, VK_WHOLE_SIZE);
			DescriptorSetUpdateBuilder()
				.writeSingle(*resultDescriptorSet, DSL::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &resultAS)
				.writeSingle(*resultDescriptorSet, DSL::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultSSBO)
				.update(vkd, device);
		}

		// wait for data transfers
		const VkMemoryBarrier bufferUploadBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &bufferUploadBarrier, 1u);

		// wait for as build
		const VkMemoryBarrier asBuildBarrier = makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
		cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &asBuildBarrier, 1u);

		vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipeline);

		// generate reference
		vkd.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipelineLayout, 0, 1, &referenceDescriptorSet.get(), 0, DE_NULL);
		cmdTraceRays(vkd, *m_cmdBuffer, &rgenSBTR, &missSBTR, &chitSBTR, &callableSBTR, imageSize, imageSize, 1);

		// generate result
		vkd.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipelineLayout, 0, 1, &resultDescriptorSet.get(), 0, DE_NULL);
		cmdTraceRays(vkd, *m_cmdBuffer, &rgenSBTR, &missSBTR, &chitSBTR, &callableSBTR, imageSize, imageSize, 1);

		const VkMemoryBarrier postTraceMemoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);
	}
	endCommandBuffer(vkd, *m_cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, m_cmdBuffer.get());

	// verify result buffer
	auto referenceAllocation = referenceBuffer->getAllocation();
	invalidateMappedMemoryRange(vkd, device, referenceAllocation.getMemory(), referenceAllocation.getOffset(), resultBufferSize);

	auto resultAllocation = resultBuffer->getAllocation();
	invalidateMappedMemoryRange(vkd, device, resultAllocation.getMemory(), resultAllocation.getOffset(), resultBufferSize);

	tcu::TextureFormat		imageFormat		(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
	tcu::PixelBufferAccess	referenceAccess	(imageFormat, imageSize, imageSize, 1, referenceAllocation.getHostPtr());
	tcu::PixelBufferAccess	resultAccess	(imageFormat, imageSize, imageSize, 1, resultAllocation.getHostPtr());

	if (tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Result comparison", "", referenceAccess, resultAccess, tcu::UVec4(0), tcu::COMPARE_LOG_EVERYTHING))
		return tcu::TestStatus::pass("Pass");
	return tcu::TestStatus::fail("Fail");
}

VkWriteDescriptorSetAccelerationStructureKHR RayTracingProceduralGeometryTestBase::makeASWriteDescriptorSet(const VkAccelerationStructureKHR* pAccelerationStructure)
{
	return
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	// VkStructureType						sType
		DE_NULL,															// const void*							pNext
		1u,																	// deUint32								accelerationStructureCount
		pAccelerationStructure												// const VkAccelerationStructureKHR*	pAccelerationStructures
	};
}

void RayTracingProceduralGeometryTestBase::clearBuffer(de::SharedPtr<BufferWithMemory> buffer, VkDeviceSize bufferSize)
{
	const DeviceInterface&	vkd				= *m_customDevice.vkd;
	const VkDevice			device			= *m_customDevice.device;
	auto&					bufferAlloc		= buffer->getAllocation();
	void*					bufferPtr		= bufferAlloc.getHostPtr();

	deMemset(bufferPtr, 1, static_cast<size_t>(bufferSize));
	vk::flushAlloc(vkd, device, bufferAlloc);
}

class ObjectBehindBoundingBoxInstance : public RayTracingProceduralGeometryTestBase
{
public:

	ObjectBehindBoundingBoxInstance(Context& context);

	void setupRayTracingPipeline() override;
	void setupAccelerationStructures() override;
};

ObjectBehindBoundingBoxInstance::ObjectBehindBoundingBoxInstance(Context& context)
	: RayTracingProceduralGeometryTestBase(context)
{
}

void ObjectBehindBoundingBoxInstance::setupRayTracingPipeline()
{
	const DeviceInterface&	vkd				= *m_customDevice.vkd;
	const VkDevice			device			= *m_customDevice.device;
	Allocator&				allocator		= *m_customDevice.allocator;
	vk::BinaryCollection&	bc				= m_context.getBinaryCollection();
	const deUint32			sgHandleSize	= m_context.getRayTracingPipelineProperties().shaderGroupHandleSize;
	const deUint32			sgBaseAlignment	= m_context.getRayTracingPipelineProperties().shaderGroupBaseAlignment;

	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,			createShaderModule(vkd, device, bc.get("rgen"), 0), 0);
	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	createShaderModule(vkd, device, bc.get("isec"), 0), 1);
	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, bc.get("chit"), 0), 1);
	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, bc.get("miss"), 0), 2);

	m_pipelineLayout	= makePipelineLayout(vkd, device, m_descriptorSetLayout.get());
	m_pipeline			= m_rayTracingPipeline->createPipeline(vkd, device, *m_pipelineLayout);
	m_rgenShaderBT		= m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize, sgBaseAlignment, 0, 1);
	m_chitShaderBT		= m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize, sgBaseAlignment, 1, 1);
	m_missShaderBT		= m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize, sgBaseAlignment, 2, 1);
}

void ObjectBehindBoundingBoxInstance::setupAccelerationStructures()
{
	const DeviceInterface&	vkd			= *m_customDevice.vkd;
	const VkDevice			device		= *m_customDevice.device;
	Allocator&				allocator	= *m_customDevice.allocator;

	// build reference acceleration structure - single aabb big enough to fit whole procedural geometry
	de::SharedPtr<BottomLevelAccelerationStructure> referenceBLAS(makeBottomLevelAccelerationStructure().release());
	referenceBLAS->setGeometryData(
		{
			{  0.0,  0.0, -64.0 },
			{ 64.0, 64.0, -16.0 },
		},
		false,
		0
		);
	referenceBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(referenceBLAS);

	m_referenceTLAS->setInstanceCount(1);
	m_referenceTLAS->addInstance(m_blasVect.back());
	m_referenceTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);

	// build result acceleration structure - wall of 4 aabb's and generated object is actualy behind it (as it is just 1.0 unit thick)
	de::SharedPtr<BottomLevelAccelerationStructure> resultBLAS(makeBottomLevelAccelerationStructure().release());
	resultBLAS->setGeometryData(
		{
			{  0.0,  0.0, 0.0 },	// |  |
			{ 32.0, 32.0, 1.0 },	// |* |
			{ 32.0,  0.0, 0.0 },	//    |  |
			{ 64.0, 32.0, 1.0 },	//    | *|
			{  0.0, 32.0, 0.0 },	// |* |
			{ 32.0, 64.0, 1.0 },	// |  |
			{ 32.0, 32.0, 0.0 },	//    | *|
			{ 64.0, 64.0, 1.0 },	//    |  |
		},
		false,
		0
		);
	resultBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(resultBLAS);

	m_resultTLAS->setInstanceCount(1);
	m_resultTLAS->addInstance(m_blasVect.back());
	m_resultTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
}

class TriangleInBeteenInstance : public RayTracingProceduralGeometryTestBase
{
public:

	TriangleInBeteenInstance(Context& context);

	void setupRayTracingPipeline() override;
	void setupAccelerationStructures() override;
};

TriangleInBeteenInstance::TriangleInBeteenInstance(Context& context)
	: RayTracingProceduralGeometryTestBase(context)
{
}

void TriangleInBeteenInstance::setupRayTracingPipeline()
{
	const DeviceInterface&	vkd				= *m_customDevice.vkd;
	const VkDevice			device			= *m_customDevice.device;
	Allocator&				allocator		= *m_customDevice.allocator;
	vk::BinaryCollection&	bc				= m_context.getBinaryCollection();
	const deUint32			sgHandleSize	= m_context.getRayTracingPipelineProperties().shaderGroupHandleSize;
	const deUint32			sgBaseAlignment = m_context.getRayTracingPipelineProperties().shaderGroupBaseAlignment;

	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,			createShaderModule(vkd, device, bc.get("rgen"), 0), 0);
	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	createShaderModule(vkd, device, bc.get("isec"), 0), 1);
	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, bc.get("chit"), 0), 1);
	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, bc.get("chit_triangle"), 0), 2);
	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, bc.get("miss"), 0), 3);

	m_pipelineLayout	= makePipelineLayout(vkd, device, m_descriptorSetLayout.get());
	m_pipeline			= m_rayTracingPipeline->createPipeline(vkd, device, *m_pipelineLayout);
	m_rgenShaderBT		= m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize, sgBaseAlignment, 0, 1);
	m_chitShaderBT		= m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize, sgBaseAlignment, 1, 2);
	m_missShaderBT		= m_rayTracingPipeline->createShaderBindingTable(vkd, device, *m_pipeline, allocator, sgHandleSize, sgBaseAlignment, 3, 1);
}

void TriangleInBeteenInstance::setupAccelerationStructures()
{
	const DeviceInterface&	vkd			= *m_customDevice.vkd;
	const VkDevice			device		= *m_customDevice.device;
	Allocator&				allocator	= *m_customDevice.allocator;

	de::SharedPtr<BottomLevelAccelerationStructure> triangleBLAS(makeBottomLevelAccelerationStructure().release());
	triangleBLAS->setGeometryData(
		{
			{ 16.0, 16.0, -8.0 },
			{ 56.0, 32.0, -8.0 },
			{ 32.0, 48.0, -8.0 },
		},
		true,
		VK_GEOMETRY_OPAQUE_BIT_KHR
		);
	triangleBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(triangleBLAS);

	de::SharedPtr<BottomLevelAccelerationStructure> fullElipsoidBLAS(makeBottomLevelAccelerationStructure().release());
	fullElipsoidBLAS->setGeometryData(
		{
			{  0.0,  0.0, -64.0 },
			{ 64.0, 64.0, -16.0 },
		},
		false,
		0
		);
	fullElipsoidBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(fullElipsoidBLAS);

	// build reference acceleration structure - triangle and a single aabb big enough to fit whole procedural geometry
	m_referenceTLAS->setInstanceCount(2);
	m_referenceTLAS->addInstance(fullElipsoidBLAS);
	m_referenceTLAS->addInstance(triangleBLAS);
	m_referenceTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);

	de::SharedPtr<BottomLevelAccelerationStructure> elipsoidWallBLAS(makeBottomLevelAccelerationStructure().release());
	elipsoidWallBLAS->setGeometryData(
		{
			{  0.0,  0.0, 0.0 },	// |*  |
			{ 20.0, 64.0, 1.0 },
			{ 20.0,  0.0, 0.0 },	// | * |
			{ 44.0, 64.0, 1.0 },
			{ 44.0,  0.0, 0.0 },	// |  *|
			{ 64.0, 64.0, 1.0 },
		},
		false,
		0
		);
	elipsoidWallBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(elipsoidWallBLAS);

	// build result acceleration structure - triangle and a three aabb's (they are in front of triangle but generate intersections behind it)
	m_resultTLAS->setInstanceCount(2);
	m_resultTLAS->addInstance(elipsoidWallBLAS);
	m_resultTLAS->addInstance(triangleBLAS);
	m_resultTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
}

class RayTracingProceduralGeometryTestCase : public TestCase
{
public:
	RayTracingProceduralGeometryTestCase		(tcu::TestContext& context, const char* name, TestType testType);
	~RayTracingProceduralGeometryTestCase		(void) = default;

	void				checkSupport			(Context& context) const override;
	void				initPrograms			(SourceCollections& programCollection) const override;
	TestInstance*		createInstance			(Context& context) const override;

protected:
	TestType m_testType;
};

RayTracingProceduralGeometryTestCase::RayTracingProceduralGeometryTestCase(tcu::TestContext& context, const char* name, TestType testType)
	: TestCase		(context, name, "")
	, m_testType	(testType)
{
}

void RayTracingProceduralGeometryTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");

	if (!context.getRayTracingPipelineFeatures().rayTracingPipeline)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	if (!context.getAccelerationStructureFeatures().accelerationStructure)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
}

void RayTracingProceduralGeometryTestCase::initPrograms(SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions glslBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::string rgenSource =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"layout(location = 0) rayPayloadEXT int payload;\n"

		"layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
		"layout(set = 0, binding = 1, std430) writeonly buffer Result {\n"
		"    int value[];\n"
		"} result;\n"

		"void main()\n"
		"{\n"
		"  float tmin        = 0.0;\n"
		"  float tmax        = 50.0;\n"
		"  vec3  origin      = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 2.0);\n"
		"  vec3  direction   = vec3(0.0,0.0,-1.0);\n"
		"  uint  resultIndex = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x;\n"

		"  traceRayEXT(tlas, gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);\n"
		// to be able to display result in cherry this is interpreated as r8g8b8a8 during verification
		// we are using only red but we need to add alpha (note: r and a may be swapped depending on endianness)
		"  result.value[resultIndex] = payload + 0xFF000000;\n"
		"};\n";
	programCollection.glslSources.add("rgen") << glu::RaygenSource(rgenSource) << glslBuildOptions;

	std::string isecSource =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"

		"void main()\n"
		"{\n"
		// note: same elipsoid center and radii are also defined in chit shader
		"  vec3 center = vec3(32.0, 32.0, -30.0);\n"
		"  vec3 radii  = vec3(30.0, 15.0, 5.0);\n"

		// simplify to ray sphere intersection
		"  vec3  eliDir = gl_WorldRayOriginEXT - center;\n"
		"  vec3  eliS   = eliDir / radii;\n"
		"  vec3  rayS   = gl_WorldRayDirectionEXT / radii;\n"

		"  float a = dot(rayS, rayS);\n"
		"  float b = dot(eliS, rayS);\n"
		"  float c = dot(eliS, eliS);\n"
		"  float h = b * b - a * (c - 1.0);\n"
		"  if (h < 0.0)\n"
		"    return;\n"
		"  reportIntersectionEXT((-b - sqrt(h)) / a, 0);\n"
		"}\n";
	programCollection.glslSources.add("isec") << glu::IntersectionSource(isecSource) << glslBuildOptions;

	std::string chitSource =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"layout(location = 0) rayPayloadInEXT int payload;\n"
		"\n"
		"void main()\n"
		"{\n"
		// note: same elipsoid center and radii are also defined in chit shader
		"  vec3 center    = vec3(32.0, 32.0, -30.0);\n"
		"  vec3 radii     = vec3(30.0, 15.0, 5.0);\n"
		"  vec3 lightDir  = normalize(vec3(0.0, 0.0, 1.0));\n"
		"  vec3 hitPos    = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;\n"
		"  vec3 hitNormal = normalize((hitPos - center) / radii);\n"

		"  payload = 50 + int(200.0 * clamp(dot(hitNormal, lightDir), 0.0, 1.0));\n"
		"}\n";
	programCollection.glslSources.add("chit") << glu::ClosestHitSource(chitSource) << glslBuildOptions;

	if (m_testType == TestType::TRIANGLE_IN_BETWEEN)
	{
		std::string chitTriangleSource =
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT int payload;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  payload = 250;\n"
			"}\n";
		programCollection.glslSources.add("chit_triangle") << glu::ClosestHitSource(chitTriangleSource) << glslBuildOptions;
	}

	std::string missSource =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"layout(location = 0) rayPayloadInEXT int payload;\n"
		"void main()\n"
		"{\n"
		"  payload = 30;\n"
		"}\n";
	programCollection.glslSources.add("miss") << glu::MissSource(missSource) << glslBuildOptions;
}

TestInstance* RayTracingProceduralGeometryTestCase::createInstance(Context& context) const
{
	if (m_testType == TestType::TRIANGLE_IN_BETWEEN)
		return new TriangleInBeteenInstance(context);

	// TestType::OBJECT_BEHIND_BOUNDING_BOX
	return new ObjectBehindBoundingBoxInstance(context);
}

}	// anonymous

tcu::TestCaseGroup*	createProceduralGeometryTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "procedural_geometry", "Test procedural geometry with complex bouding box sets"));

	group->addChild(new RayTracingProceduralGeometryTestCase(testCtx, "object_behind_bounding_boxes",	TestType::OBJECT_BEHIND_BOUNDING_BOX));
	group->addChild(new RayTracingProceduralGeometryTestCase(testCtx, "triangle_in_between",			TestType::TRIANGLE_IN_BETWEEN));

	return group.release();
}

}	// RayTracing

}	// vkt
