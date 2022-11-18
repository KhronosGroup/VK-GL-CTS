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
 * \brief Ray Tracing Opacity Micromap Tests
 *//*--------------------------------------------------------------------*/


#include "vktRayTracingOpacityMicromapTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <vector>
#include <iostream>

namespace vkt
{
namespace RayTracing
{

namespace
{

using namespace vk;

enum TestFlagBits
{
	TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE				= 1U << 0,
	TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG				= 1U << 1,
	TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE	= 1U << 2,
	TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE			= 1U << 3,
	TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG			= 1U << 4,
	TEST_FLAG_BIT_LAST								= 1U << 5,
};

std::vector<std::string> testFlagBitNames =
{
	"force_opaque_instance",
	"force_opaque_ray_flag",
	"disable_opacity_micromap_instance",
	"force_2_state_instance",
	"force_2_state_ray_flag",
};

struct TestParams
{
	bool					useSpecialIndex;
	deUint32				testFlagMask;
	deUint32				subdivisionLevel; // Must be 0 for useSpecialIndex
	deUint32				mode; // Special index value if useSpecialIndex, 2 or 4 for number of states otherwise
	deUint32				seed;
};

class OpacityMicromapCase : public TestCase
{
public:
	OpacityMicromapCase(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~OpacityMicromapCase(void) {}

	virtual void			checkSupport(Context& context) const;
	virtual void			initPrograms(vk::SourceCollections& programCollection) const;
	virtual TestInstance* createInstance(Context& context) const;

protected:
	TestParams				m_params;
};

class OpacityMicromapInstance : public TestInstance
{
public:
	OpacityMicromapInstance(Context& context, const TestParams& params);
	virtual						~OpacityMicromapInstance(void) {}

	virtual tcu::TestStatus		iterate(void);

protected:
	TestParams					m_params;
};

OpacityMicromapCase::OpacityMicromapCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
{}

void OpacityMicromapCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	context.requireDeviceFunctionality("VK_EXT_opacity_micromap");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR& accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	const VkPhysicalDeviceOpacityMicromapFeaturesEXT& opacityMicromapFeaturesEXT = context.getOpacityMicromapFeaturesEXT();
	if (opacityMicromapFeaturesEXT.micromap == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceOpacityMicromapFeaturesEXT.micromap");

	const VkPhysicalDeviceOpacityMicromapPropertiesEXT& opacityMicromapPropertiesEXT = context.getOpacityMicromapPropertiesEXT();

	if (!m_params.useSpecialIndex)
	{
		switch (m_params.mode)
		{
		case 2:
			if (m_params.subdivisionLevel > opacityMicromapPropertiesEXT.maxOpacity2StateSubdivisionLevel)
				TCU_THROW(NotSupportedError, "Requires a higher supported 2 state subdivision level");
			break;
		case 4:
			if (m_params.subdivisionLevel > opacityMicromapPropertiesEXT.maxOpacity4StateSubdivisionLevel)
				TCU_THROW(NotSupportedError, "Requires a higher supported 4 state subdivision level");
			break;
		default:
			DE_ASSERT(false);
			break;
		}
	}
}

static deUint32 levelToSubtriangles(deUint32 level)
{
	return 1 << (2 * level);
}


void OpacityMicromapCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions buildOptions (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	deUint32 numRays = levelToSubtriangles(m_params.subdivisionLevel);

	std::ostringstream layoutDecls;
	layoutDecls
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1, std430) buffer RayOrigins {\n"
		<< "  vec4 values[" << numRays << "];\n"
		<< "} origins;\n"
		<< "layout(set=0, binding=2, std430) buffer OutputModes {\n"
		<< "  uint values[" << numRays << "];\n"
		<< "} modes;\n"
		;
	const auto layoutDeclsStr = layoutDecls.str();

	std::string flagsString = (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG) ? "gl_RayFlagsOpaqueEXT" : "gl_RayFlagsNoneEXT";

	if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG)
		flagsString += " | gl_RayFlagsForceOpacityMicromap2StateEXT";

	std::ostringstream rgen;
	rgen
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "#extension GL_EXT_opacity_micromap : require\n"
		<< "\n"
		<< "layout(location=0) rayPayloadEXT uint value;\n"
		<< "\n"
		<< layoutDeclsStr
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  const uint  cullMask  = 0xFF;\n"
		<< "  const vec3  origin    = origins.values[gl_LaunchIDEXT.x].xyz;\n"
		<< "  const vec3  direction = vec3(0.0, 0.0, -1.0);\n"
		<< "  const float tMin      = 0.0;\n"
		<< "  const float tMax      = 2.0;\n"
		<< "  value                 = 0xFFFFFFFF;\n"
		<< "  traceRayEXT(topLevelAS, " << flagsString << ", cullMask, 0, 0, 0, origin, tMin, direction, tMax, 0);\n"
		<< "  modes.values[gl_LaunchIDEXT.x] = value;\n"
		<< "}\n"
		;

	std::ostringstream ah;
	ah
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "\n"
		<< layoutDeclsStr
		<< "\n"
		<< "layout(location=0) rayPayloadInEXT uint value;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  value = 1;\n"
		<< "  terminateRayEXT;\n"
		<< "}\n"
		;

	std::ostringstream ch;
	ch
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "\n"
		<< layoutDeclsStr
		<< "\n"
		<< "layout(location=0) rayPayloadInEXT uint value;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  if (value != 1) {\n" // If we didn't already run AH mark as CH
		<< "    value = 2;\n"
		<< "  }\n"
		<< "}\n"
		;

	std::ostringstream miss;
	miss
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< layoutDeclsStr
		<< "\n"
		<< "layout(location=0) rayPayloadInEXT uint value;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  value = 0;\n"
		<< "}\n";

	programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
	programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(miss.str())) << buildOptions;
	programCollection.glslSources.add("ah") << glu::AnyHitSource(updateRayTracingGLSL(ah.str())) << buildOptions;
	programCollection.glslSources.add("ch") << glu::ClosestHitSource(updateRayTracingGLSL(ch.str())) << buildOptions;
}

TestInstance* OpacityMicromapCase::createInstance (Context& context) const
{
	return new OpacityMicromapInstance(context, m_params);
}

OpacityMicromapInstance::OpacityMicromapInstance (Context& context, const TestParams& params)
	: TestInstance	(context)
	, m_params		(params)
{}

tcu::Vec2 calcSubtriangleCentroid(const deUint32 index, const deUint32 subdivisionLevel)
{
	if (subdivisionLevel == 0) {
		return tcu::Vec2(1.0f / 3.0f, 1.0f / 3.0f);
	}

	deUint32 d = index;

	d = ((d >> 1) & 0x22222222u) | ((d << 1) & 0x44444444u) | (d & 0x99999999u);
	d = ((d >> 2) & 0x0c0c0c0cu) | ((d << 2) & 0x30303030u) | (d & 0xc3c3c3c3u);
	d = ((d >> 4) & 0x00f000f0u) | ((d << 4) & 0x0f000f00u) | (d & 0xf00ff00fu);
	d = ((d >> 8) & 0x0000ff00u) | ((d << 8) & 0x00ff0000u) | (d & 0xff0000ffu);

	deUint32 f = (d & 0xffffu) | ((d << 16) & ~d);

	f ^= (f >> 1) & 0x7fff7fffu;
	f ^= (f >> 2) & 0x3fff3fffu;
	f ^= (f >> 4) & 0x0fff0fffu;
	f ^= (f >> 8) & 0x00ff00ffu;

	deUint32 t = (f ^ d) >> 16;

	deUint32 iu = ((f & ~t) | (d & ~t) | (~d & ~f & t)) & 0xffffu;
	deUint32 iv = ((f >> 16) ^ d) & 0xffffu;
	deUint32 iw = ((~f & ~t) | (d & ~t) | (~d & f & t)) & ((1 << subdivisionLevel) - 1);

	const float scale = 1.0f / float(1 << subdivisionLevel);

	float u = (1.0f / 3.0f) * scale;
	float v = (1.0f / 3.0f) * scale;

	// we need to only look at "subdivisionLevel" bits
	iu = iu & ((1 << subdivisionLevel) - 1);
	iv = iv & ((1 << subdivisionLevel) - 1);
	iw = iw & ((1 << subdivisionLevel) - 1);

	bool upright = (iu & 1) ^ (iv & 1) ^ (iw & 1);
	if (!upright)
	{
		iu = iu + 1;
		iv = iv + 1;
	}

	if (upright)
	{
		return tcu::Vec2(
			u + (float)iu * scale,
			v + (float)iv * scale
		);
	} else
	{
		return tcu::Vec2(
			(float)iu * scale - u,
			(float)iv * scale - v
		);
	}
}

tcu::TestStatus OpacityMicromapInstance::iterate (void)
{
	const auto&	vki		= m_context.getInstanceInterface();
	const auto	physDev	= m_context.getPhysicalDevice();
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	auto&		alloc	= m_context.getDefaultAllocator();
	const auto	qIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	queue	= m_context.getUniversalQueue();
	const auto	stages	= VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Build acceleration structures.
	auto topLevelAS		= makeTopLevelAccelerationStructure();
	auto bottomLevelAS	= makeBottomLevelAccelerationStructure();

	deUint32 numSubtriangles = levelToSubtriangles(m_params.subdivisionLevel);
	deUint32 opacityMicromapBytes = (m_params.mode == 2) ? (numSubtriangles + 3) / 4 : (numSubtriangles + 1) / 2;

	// Generate random micromap data
	std::vector<deUint8> opacityMicromapData;

	de::Random rnd(m_params.seed);

	while (opacityMicromapData.size() < opacityMicromapBytes) {
		opacityMicromapData.push_back(rnd.getUint8());
	}

	// Build a micromap (ignore infrastructure for now)
	// Create the buffer with the mask and index data
	// Allocate a fairly conservative bound for now
	const auto micromapDataBufferSize = static_cast<VkDeviceSize>(1024 + opacityMicromapBytes);
	const auto micromapDataBufferCreateInfo = makeBufferCreateInfo(micromapDataBufferSize,
		VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	BufferWithMemory micromapDataBuffer(vkd, device, alloc, micromapDataBufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
	auto& micromapDataBufferAlloc = micromapDataBuffer.getAllocation();
	void* micromapDataBufferData = micromapDataBufferAlloc.getHostPtr();

	const int TriangleOffset = 0;
	const int IndexOffset = 256;
	const int DataOffset = 512;

	// Fill out VkMicromapUsageEXT with size information
	VkMicromapUsageEXT mmUsage = { };
	mmUsage.count = 1;
	mmUsage.subdivisionLevel = m_params.subdivisionLevel;
	mmUsage.format = m_params.mode == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT : VK_OPACITY_MICROMAP_FORMAT_4_STATE_EXT;

	{
		deUint8* data = static_cast<deUint8*>(micromapDataBufferData);

		deMemset(data, 0, size_t(micromapDataBufferCreateInfo.size));

		DE_STATIC_ASSERT(sizeof(VkMicromapTriangleEXT) == 8);

		// Triangle information
		VkMicromapTriangleEXT* tri = (VkMicromapTriangleEXT*)(&data[TriangleOffset]);
		tri->dataOffset = 0;
		tri->subdivisionLevel = uint16_t(mmUsage.subdivisionLevel);
		tri->format = uint16_t(mmUsage.format);

		// Micromap data
		{
			for (size_t i = 0; i < opacityMicromapData.size(); i++) {
				data[DataOffset + i] = opacityMicromapData[i];
			}
		}

		// Index information
		*((deUint32*)&data[IndexOffset]) = m_params.useSpecialIndex ? m_params.mode : 0;
	}

	// Query the size from the build info
	VkMicromapBuildInfoEXT mmBuildInfo = {
		VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT,	// VkStructureType						sType;
		DE_NULL,									// const void*							pNext;
		VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT,		// VkMicromapTypeEXT					type;
		0,											// VkBuildMicromapFlagsEXT				flags;
		VK_BUILD_MICROMAP_MODE_BUILD_EXT,			// VkBuildMicromapModeEXT				mode;
		DE_NULL,									// VkMicromapEXT						dstMicromap;
		1,											// uint32_t							usageCountsCount;
		&mmUsage,									// const VkMicromapUsageEXT*			pUsageCounts;
		DE_NULL,									// const VkMicromapUsageEXT* const*	ppUsageCounts;
		makeDeviceOrHostAddressConstKHR(DE_NULL),	// VkDeviceOrHostAddressConstKHR		data;
		makeDeviceOrHostAddressKHR(DE_NULL),		// VkDeviceOrHostAddressKHR			scratchData;
		makeDeviceOrHostAddressConstKHR(DE_NULL),	// VkDeviceOrHostAddressConstKHR		triangleArray;
		0,											// VkDeviceSize						triangleArrayStride;
	};

	VkMicromapBuildSizesInfoEXT sizeInfo = {
		VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT,	// VkStructureType	sType;
		DE_NULL,											// const void* pNext;
		0,													// VkDeviceSize	micromapSize;
		0,													// VkDeviceSize	buildScratchSize;
		DE_FALSE,											// VkBool32		discardable;
	};

	vkd.getMicromapBuildSizesEXT(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &mmBuildInfo, &sizeInfo);

	// Create the backing and scratch storage
	const auto micromapBackingBufferCreateInfo = makeBufferCreateInfo(sizeInfo.micromapSize,
		VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	BufferWithMemory micromapBackingBuffer(vkd, device, alloc, micromapBackingBufferCreateInfo, MemoryRequirement::Local | MemoryRequirement::DeviceAddress);

	const auto micromapScratchBufferCreateInfo = makeBufferCreateInfo(sizeInfo.buildScratchSize,
		VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	BufferWithMemory micromapScratchBuffer(vkd, device, alloc, micromapScratchBufferCreateInfo, MemoryRequirement::Local | MemoryRequirement::DeviceAddress);

	// Create the micromap itself
	VkMicromapCreateInfoEXT maCreateInfo = {
		VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT,	  // VkStructureType				sType;
		DE_NULL,									  // const void* pNext;
		0,											  // VkMicromapCreateFlagsEXT	createFlags;
		micromapBackingBuffer.get(),				  // VkBuffer					buffer;
		0,											  // VkDeviceSize				offset;
		sizeInfo.micromapSize,						  // VkDeviceSize				size;
		VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT,		  // VkMicromapTypeEXT			type;
		0ull										  // VkDeviceAddress				deviceAddress;
	};

	VkMicromapEXT micromap;

	VK_CHECK(vkd.createMicromapEXT(device, &maCreateInfo, nullptr, &micromap));

	// Do the build
	mmBuildInfo.dstMicromap = micromap;
	mmBuildInfo.data = makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(), DataOffset);
	mmBuildInfo.triangleArray = makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(), TriangleOffset);
	mmBuildInfo.scratchData = makeDeviceOrHostAddressKHR(vkd, device, micromapScratchBuffer.get(), 0);

	vkd.cmdBuildMicromapsEXT(cmdBuffer, 1, &mmBuildInfo);

	{
		VkMemoryBarrier2 memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, NULL,
			VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT, VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_MICROMAP_READ_BIT_EXT };
		VkDependencyInfoKHR dependencyInfo = {
			VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,		// VkStructureType						sType;
			DE_NULL,									// const void*							pNext;
			0u,											// VkDependencyFlags					dependencyFlags;
			1u,											// uint32_t							memoryBarrierCount;
			&memoryBarrier,								// const VkMemoryBarrier2KHR*			pMemoryBarriers;
			0u,											// uint32_t							bufferMemoryBarrierCount;
			DE_NULL,									// const VkBufferMemoryBarrier2KHR*	pBufferMemoryBarriers;
			0u,											// uint32_t							imageMemoryBarrierCount;
			DE_NULL,									// const VkImageMemoryBarrier2KHR*		pImageMemoryBarriers;
		};

		vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
	}

	// Attach the micromap to the geometry
	VkAccelerationStructureTrianglesOpacityMicromapEXT opacityGeometryMicromap = {
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT,				//VkStructureType						sType;
		DE_NULL,																				//void*								pNext;
		VK_INDEX_TYPE_UINT32,																	//VkIndexType							indexType;
		makeDeviceOrHostAddressConstKHR(vkd, device, micromapDataBuffer.get(), IndexOffset),	//VkDeviceOrHostAddressConstKHR		indexBuffer;
		0u,																						//VkDeviceSize						indexStride;
		0u,																						//uint32_t							baseTriangle;
		1u,																						//uint32_t							usageCountsCount;
		& mmUsage,																				//const VkMicromapUsageEXT*			pUsageCounts;
		DE_NULL,																				//const VkMicromapUsageEXT* const*	ppUsageCounts;
		micromap																				//VkMicromapEXT						micromap;
	};

	const std::vector<tcu::Vec3> triangle =
	{
		tcu::Vec3(0.0f, 0.0f, 0.0f),
		tcu::Vec3(1.0f, 0.0f, 0.0f),
		tcu::Vec3(0.0f, 1.0f, 0.0f),
	};

	bottomLevelAS->addGeometry(triangle, true/*is triangles*/, 0, &opacityGeometryMicromap);
	if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
		bottomLevelAS->setBuildFlags(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISABLE_OPACITY_MICROMAPS_EXT);
	bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);
	de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr (bottomLevelAS.release());

	VkGeometryInstanceFlagsKHR instanceFlags = 0;

	if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE)
		instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPACITY_MICROMAP_2_STATE_EXT;
	if (m_params.testFlagMask & TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE)
		instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
	if (m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE)
		instanceFlags |= VK_GEOMETRY_INSTANCE_DISABLE_OPACITY_MICROMAPS_EXT;

	topLevelAS->setInstanceCount(1);
	topLevelAS->addInstance(blasSharedPtr, identityMatrix3x4, 0, 0xFFu, 0u, instanceFlags);
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	// One ray per subtriangle for this test
	deUint32 numRays = numSubtriangles;

	// SSBO buffer for origins.
	const auto originsBufferSize		= static_cast<VkDeviceSize>(sizeof(tcu::Vec4) * numRays);
	const auto originsBufferInfo		= makeBufferCreateInfo(originsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory originsBuffer	(vkd, device, alloc, originsBufferInfo, MemoryRequirement::HostVisible);
	auto& originsBufferAlloc			= originsBuffer.getAllocation();
	void* originsBufferData				= originsBufferAlloc.getHostPtr();

	std::vector<tcu::Vec4> origins;
	std::vector<deUint32> expectedOutputModes;
	origins.reserve(numRays);
	expectedOutputModes.reserve(numRays);

	// Fill in vector of expected outputs
	for (deUint32 index = 0; index < numRays; index++) {
		deUint32 state = m_params.testFlagMask & (TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE | TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG) ?
			VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT : VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT;

		if (!(m_params.testFlagMask & TEST_FLAG_BIT_DISABLE_OPACITY_MICROMAP_INSTANCE))
		{
			if (m_params.useSpecialIndex)
			{
				state = m_params.mode;
			}
			else
			{
				if (m_params.mode == 2) {
					deUint8 byte = opacityMicromapData[index / 8];
					state = (byte >> (index % 8)) & 0x1;
				} else {
					DE_ASSERT(m_params.mode == 4);
					deUint8 byte = opacityMicromapData[index / 4];
					state = (byte >> 2*(index % 4)) & 0x3;
				}
				// Process in SPECIAL_INDEX number space
				state = ~state;
			}

			if (m_params.testFlagMask & (TEST_FLAG_BIT_FORCE_2_STATE_INSTANCE | TEST_FLAG_BIT_FORCE_2_STATE_RAY_FLAG))
			{
				if (state == deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_TRANSPARENT_EXT))
					state =  deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT);
				if (state == deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT))
					state =  deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT);
			}
		}

		if (state != deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT))
		{
			if (m_params.testFlagMask & (TEST_FLAG_BIT_FORCE_OPAQUE_INSTANCE | TEST_FLAG_BIT_FORCE_OPAQUE_RAY_FLAG))
			{
				state = deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT);
			} else if (state != deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT)) {
				state = deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT);
			}
		}

		if (state == deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT))
		{
			expectedOutputModes.push_back(0);
		}
		else if (state == deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT))
		{
			expectedOutputModes.push_back(1);
		}
		else if (state == deUint32(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT))
		{
			expectedOutputModes.push_back(2);
		}
		else
		{
			DE_ASSERT(false);
		}
	}

	for (deUint32 index = 0; index < numRays; index++) {
		tcu::Vec2 centroid = calcSubtriangleCentroid(index, m_params.subdivisionLevel);
		origins.push_back(tcu::Vec4(centroid.x(), centroid.y(), 1.0, 0.0));
	}

	const auto				originsBufferSizeSz = static_cast<size_t>(originsBufferSize);
	deMemcpy(originsBufferData, origins.data(), originsBufferSizeSz);
	flushAlloc(vkd, device, originsBufferAlloc);

	// Storage buffer for output modes
	const auto outputModesBufferSize = static_cast<VkDeviceSize>(sizeof(deUint32) * numRays);
	const auto outputModesBufferInfo = makeBufferCreateInfo(outputModesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory outputModesBuffer(vkd, device, alloc, outputModesBufferInfo, MemoryRequirement::HostVisible);
	auto& outputModesBufferAlloc = outputModesBuffer.getAllocation();
	void* outputModesBufferData = outputModesBufferAlloc.getHostPtr();
	deMemset(outputModesBufferData, 0xFF, static_cast<size_t>(outputModesBufferSize));
	flushAlloc(vkd, device, outputModesBufferAlloc);

	// Descriptor set layout.
	DescriptorSetLayoutBuilder dsLayoutBuilder;
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
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
		const auto storageBufferInfo = makeDescriptorBufferInfo(outputModesBuffer.get(), 0ull, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inStorageBufferInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo);
		updateBuilder.update(vkd, device);
	}

	// Shader modules.
	auto rgenModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);
	auto missModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0);
	auto ahModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ah"), 0);
	auto chModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("ch"), 0);

	// Get some ray tracing properties.
	deUint32 shaderGroupHandleSize		= 0u;
	deUint32 shaderGroupBaseAlignment	= 1u;
	{
		const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physDev);
		shaderGroupHandleSize				= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		shaderGroupBaseAlignment			= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
	}

	// Create raytracing pipeline and shader binding tables.
	Move<VkPipeline>				pipeline;
	de::MovePtr<BufferWithMemory>	raygenSBT;
	de::MovePtr<BufferWithMemory>	missSBT;
	de::MovePtr<BufferWithMemory>	hitSBT;
	de::MovePtr<BufferWithMemory>	callableSBT;

	auto raygenSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	auto missSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	auto hitSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	auto callableSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
		rayTracingPipeline->setCreateFlags(VK_PIPELINE_CREATE_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,      rgenModule, 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,        missModule, 1);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,     ahModule, 2);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chModule, 2);

		pipeline		= rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

		raygenSBT		= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		missSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		missSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		hitSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
		hitSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	}

	// Trace rays.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, numRays, 1u, 1u);

	// Barrier for the output buffer.
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify results.
	std::vector<deUint32>	outputData(expectedOutputModes.size());
	const auto				outputModesBufferSizeSz = static_cast<size_t>(outputModesBufferSize);

	invalidateAlloc(vkd, device, outputModesBufferAlloc);
	DE_ASSERT(de::dataSize(outputData) == outputModesBufferSizeSz);
	deMemcpy(outputData.data(), outputModesBufferData, outputModesBufferSizeSz);

	for (size_t i = 0; i < outputData.size(); ++i)
	{
		const auto& outVal = outputData[i];
		const auto& expectedVal = expectedOutputModes[i];

		if (outVal != expectedVal)
		{
			std::ostringstream msg;
			msg << "Unexpected value found for ray " << i << ": expected " << expectedVal << " and found " << outVal << ";";
			TCU_FAIL(msg.str());
		}
#if 0
		else
		{
			std::ostringstream msg;
			msg << "Expected value found for ray " << i << ": expected " << expectedVal << " and found " << outVal << ";\n"; // XXX Debug remove
			std::cout << msg.str();
		}
#endif
	}


	return tcu::TestStatus::pass("Pass");
}

} // anonymous

constexpr deUint32 kMaxSubdivisionLevel = 15;

tcu::TestCaseGroup*	createOpacityMicromapTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "opacity_micromap", "Test acceleration structures using opacity micromap with ray pipelines"));

	deUint32 seed = 1614343620u;

	const struct
	{
		bool									useSpecialIndex;
		std::string								name;
	} specialIndexUse[] =
	{
		{ false,								"map_value"},
		{ true,									"special_index"},
	};

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

		for (size_t specialIndexNdx = 0; specialIndexNdx < DE_LENGTH_OF_ARRAY(specialIndexUse); ++specialIndexNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> specialGroup(new tcu::TestCaseGroup(testFlagGroup->getTestContext(), specialIndexUse[specialIndexNdx].name.c_str(), ""));

			if (specialIndexUse[specialIndexNdx].useSpecialIndex)
			{
				for (deUint32 specialIndex = 0; specialIndex < 4; specialIndex++) {
					TestParams testParams
					{
						specialIndexUse[specialIndexNdx].useSpecialIndex,
						testFlagMask,
						0,
						~specialIndex,
						seed++,
					};

					std::stringstream css;
					css << specialIndex;

					specialGroup->addChild(new OpacityMicromapCase(testCtx, css.str().c_str(), "", testParams));
				}
				testFlagGroup->addChild(specialGroup.release());
			}
			else
			{
				struct {
					deUint32 mode;
					std::string name;
				} modes[] =
				{
					{ 2, "2"},
					{ 4, "4" }
				};
				for (deUint32 modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(modes); ++modeNdx)
				{
					de::MovePtr<tcu::TestCaseGroup> modeGroup(new tcu::TestCaseGroup(testFlagGroup->getTestContext(), modes[modeNdx].name.c_str(), ""));

					for (deUint32 level = 0; level <= kMaxSubdivisionLevel; level++)
					{
						TestParams testParams
						{
							specialIndexUse[specialIndexNdx].useSpecialIndex,
							testFlagMask,
							level,
							modes[modeNdx].mode,
							seed++,
						};

						std::stringstream css;
						css << "level_" << level;

						modeGroup->addChild(new OpacityMicromapCase(testCtx, css.str().c_str(), "", testParams));
					}
					specialGroup->addChild(modeGroup.release());
				}
				testFlagGroup->addChild(specialGroup.release());
			}
		}

		group->addChild(testFlagGroup.release());
	}


	return group.release();
}

} // RayTracing
} // vkt

