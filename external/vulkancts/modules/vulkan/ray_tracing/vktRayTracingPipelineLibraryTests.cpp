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
 * \brief Ray Tracing Pipeline Library Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingPipelineLibraryTests.hpp"

#include <list>
#include <vector>

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "tcuCommandLine.hpp"

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace vkt;

static const VkFlags	ALL_RAY_TRACING_STAGES		= VK_SHADER_STAGE_RAYGEN_BIT_KHR
													| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
													| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
													| VK_SHADER_STAGE_MISS_BIT_KHR
													| VK_SHADER_STAGE_INTERSECTION_BIT_KHR
													| VK_SHADER_STAGE_CALLABLE_BIT_KHR;

static const deUint32	RTPL_DEFAULT_SIZE			= 8u;
static const deUint32	RTPL_MAX_CHIT_SHADER_COUNT	= 16;

struct LibraryConfiguration
{
	deInt32								pipelineShaders;
	std::vector<tcu::IVec2>				pipelineLibraries; // IVec2 = ( parentID, shaderCount )
};

struct TestParams
{
	LibraryConfiguration				libraryConfiguration;
	bool								multithreadedCompilation;
	bool								pipelinesCreatedUsingDHO;
	deUint32							width;
	deUint32							height;
};

deUint32 getShaderGroupSize (const InstanceInterface&	vki,
							 const VkPhysicalDevice		physicalDevice)
{
	de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

	rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physicalDevice);
	return rayTracingPropertiesKHR->getShaderGroupHandleSize();
}

deUint32 getShaderGroupBaseAlignment (const InstanceInterface&	vki,
									  const VkPhysicalDevice	physicalDevice)
{
	de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

	rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);
	return rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
}

VkImageCreateInfo makeImageCreateInfo (deUint32 width, deUint32 height, VkFormat format)
{
	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,																// VkStructureType			sType;
		DE_NULL,																							// const void*				pNext;
		(VkImageCreateFlags)0u,																				// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,																					// VkImageType				imageType;
		format,																								// VkFormat					format;
		makeExtent3D(width, height, 1),																		// VkExtent3D				extent;
		1u,																									// deUint32					mipLevels;
		1u,																									// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,																				// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,																			// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,																			// VkSharingMode			sharingMode;
		0u,																									// deUint32					queueFamilyIndexCount;
		DE_NULL,																							// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED																			// VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

class RayTracingPipelineLibraryTestCase : public TestCase
{
	public:
							RayTracingPipelineLibraryTestCase	(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
							~RayTracingPipelineLibraryTestCase	(void);

	virtual void			checkSupport								(Context& context) const;
	virtual	void			initPrograms								(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance								(Context& context) const;
private:
	TestParams				m_data;
};

struct DeviceTestFeatures
{
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR		rayTracingPipelineFeatures;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR	accelerationStructureFeatures;
	VkPhysicalDeviceBufferDeviceAddressFeaturesKHR		deviceAddressFeatures;
	VkPhysicalDeviceFeatures2							deviceFeatures;

	void linkStructures ()
	{
		rayTracingPipelineFeatures.pNext	= nullptr;
		accelerationStructureFeatures.pNext	= &rayTracingPipelineFeatures;
		deviceAddressFeatures.pNext			= &accelerationStructureFeatures;
		deviceFeatures.pNext				= &deviceAddressFeatures;
	}

	DeviceTestFeatures (const InstanceInterface& vki, VkPhysicalDevice physicalDevice)
	{
		rayTracingPipelineFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		accelerationStructureFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		deviceAddressFeatures.sType			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
		deviceFeatures.sType				= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

		linkStructures();
		vki.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);
	}
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
		const auto&	vkp				= context.getPlatformInterface();
		const auto&	vki				= context.getInstanceInterface();
		const auto	instance		= context.getInstance();
		const auto	physicalDevice	= context.getPhysicalDevice();
		const auto	queuePriority	= 1.0f;

		// Queue index first.
		queueFamilyIndex = context.getUniversalQueueFamilyIndex();

		// Get device features (these have already been checked in the test case).
		DeviceTestFeatures features(vki, physicalDevice);
		features.linkStructures();

		// Make sure robust buffer access is disabled as in the default device.
		features.deviceFeatures.features.robustBufferAccess = VK_FALSE;

		const VkDeviceQueueCreateInfo queueInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	//	VkStructureType				sType;
			nullptr,									//	const void*					pNext;
			0u,											//	VkDeviceQueueCreateFlags	flags;
			queueFamilyIndex,							//	deUint32					queueFamilyIndex;
			1u,											//	deUint32					queueCount;
			&queuePriority,								//	const float*				pQueuePriorities;
		};

		// Required extensions.
		std::vector<const char*> requiredExtensions;
		requiredExtensions.push_back("VK_KHR_ray_tracing_pipeline");
		requiredExtensions.push_back("VK_KHR_pipeline_library");
		requiredExtensions.push_back("VK_KHR_acceleration_structure");
		requiredExtensions.push_back("VK_KHR_deferred_host_operations");
		requiredExtensions.push_back("VK_KHR_buffer_device_address");
		requiredExtensions.push_back("VK_EXT_descriptor_indexing");
		requiredExtensions.push_back("VK_KHR_spirv_1_4");
		requiredExtensions.push_back("VK_KHR_shader_float_controls");

		const VkDeviceCreateInfo createInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,				//	VkStructureType					sType;
			features.deviceFeatures.pNext,						//	const void*						pNext;
			0u,													//	VkDeviceCreateFlags				flags;
			1u,													//	deUint32						queueCreateInfoCount;
			&queueInfo,											//	const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
			0u,													//	deUint32						enabledLayerCount;
			nullptr,											//	const char* const*				ppEnabledLayerNames;
			static_cast<deUint32>(requiredExtensions.size()),	//	deUint32						enabledExtensionCount;
			requiredExtensions.data(),							//	const char* const*				ppEnabledExtensionNames;
			&features.deviceFeatures.features,					//	const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		// Create custom device and related objects.
		device		= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &createInfo);
		vkd			= de::MovePtr<DeviceDriver>(new DeviceDriver(vkp, instance, device.get()));
		queue		= getDeviceQueue(*vkd, *device, queueFamilyIndex, 0u);
		allocator	= de::MovePtr<SimpleAllocator>(new SimpleAllocator(*vkd, device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}
};

class RayTracingPipelineLibraryTestInstance : public TestInstance
{
public:
																	RayTracingPipelineLibraryTestInstance	(Context& context, const TestParams& data);
																	~RayTracingPipelineLibraryTestInstance	(void);
	tcu::TestStatus													iterate									(void);

protected:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures		(DeviceHelper& deviceHelper, VkCommandBuffer cmdBuffer);
	de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure			(DeviceHelper& deviceHelper, VkCommandBuffer cmdBuffer,
																											 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures);
	de::MovePtr<BufferWithMemory>									runTest									(DeviceHelper& deviceHelper);
private:
	TestParams														m_data;
};


RayTracingPipelineLibraryTestCase::RayTracingPipelineLibraryTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayTracingPipelineLibraryTestCase::~RayTracingPipelineLibraryTestCase	(void)
{
}

void RayTracingPipelineLibraryTestCase::checkSupport(Context& context) const
{
	const auto&	vki					= context.getInstanceInterface();
	const auto	physicalDevice		= context.getPhysicalDevice();
	const auto	supportedExtensions	= enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);

	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_ray_tracing_pipeline")))
		TCU_THROW(NotSupportedError, "VK_KHR_ray_tracing_pipeline not supported");

	// VK_KHR_pipeline_library must be supported if the ray tracing pipeline extension is supported, which it should be at this point.
	// If it's not supported, this is considered a failure.
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_pipeline_library")))
		TCU_FAIL("VK_KHR_pipeline_library not supported but VK_KHR_ray_tracing_pipeline supported");

	// VK_KHR_acceleration_structure is required by VK_KHR_ray_tracing_pipeline.
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_acceleration_structure")))
		TCU_FAIL("VK_KHR_acceleration_structure not supported but VK_KHR_ray_tracing_pipeline supported");

	// VK_KHR_deferred_host_operations is required by VK_KHR_acceleration_structure.
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_deferred_host_operations")))
		TCU_FAIL("VK_KHR_deferred_host_operations not supported but VK_KHR_acceleration_structure supported");

	// The same for VK_KHR_buffer_device_address.
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_buffer_device_address")))
		TCU_FAIL("VK_KHR_buffer_device_address not supported but VK_KHR_acceleration_structure supported");

	// Get and check needed features.
	DeviceTestFeatures testFeatures (vki, physicalDevice);

	if (!testFeatures.rayTracingPipelineFeatures.rayTracingPipeline)
		TCU_THROW(NotSupportedError, "Ray tracing pipelines not supported");

	if (!testFeatures.accelerationStructureFeatures.accelerationStructure)
		TCU_THROW(NotSupportedError, "Acceleration structures not supported");

	if (!testFeatures.deviceAddressFeatures.bufferDeviceAddress)
		TCU_FAIL("Acceleration structures supported but bufferDeviceAddress not supported");
}

void RayTracingPipelineLibraryTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT uvec4 hitValue;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, float(gl_LaunchIDEXT.z + 0.5f));\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  hitValue       = uvec4(" << RTPL_MAX_CHIT_SHADER_COUNT+1 << ",0,0,0);\n"
			"  traceRayEXT(topLevelAS, 0, 0xFF, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), hitValue);\n"
			"}\n";
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4("<< RTPL_MAX_CHIT_SHADER_COUNT <<",0,0,1);\n"
			"}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	for(deUint32 i=0; i<RTPL_MAX_CHIT_SHADER_COUNT; ++i)
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4(" << i << ",0,0,1);\n"
			"}\n";
		std::stringstream csname;
		csname << "chit" << i;
		programCollection.glslSources.add(csname.str()) << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

TestInstance* RayTracingPipelineLibraryTestCase::createInstance (Context& context) const
{
	return new RayTracingPipelineLibraryTestInstance(context, m_data);
}

RayTracingPipelineLibraryTestInstance::RayTracingPipelineLibraryTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

RayTracingPipelineLibraryTestInstance::~RayTracingPipelineLibraryTestInstance (void)
{
}

std::vector<de::SharedPtr<BottomLevelAccelerationStructure> > RayTracingPipelineLibraryTestInstance::initBottomAccelerationStructures (DeviceHelper& deviceHelper, VkCommandBuffer cmdBuffer)
{
	const auto&														vkd			= *deviceHelper.vkd;
	const auto														device		= deviceHelper.device.get();
	auto&															allocator	= *deviceHelper.allocator;
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	tcu::Vec3 v0(0.0, 1.0, 0.0);
	tcu::Vec3 v1(0.0, 0.0, 0.0);
	tcu::Vec3 v2(1.0, 1.0, 0.0);
	tcu::Vec3 v3(1.0, 0.0, 0.0);

	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		// let's build a 3D chessboard of geometries
		if (((x + y) % 2) == 0)
			continue;
		tcu::Vec3 xyz((float)x, (float)y, 0.0f);
		std::vector<tcu::Vec3>	geometryData;

		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
		bottomLevelAccelerationStructure->setGeometryCount(1u);

		geometryData.push_back(xyz + v0);
		geometryData.push_back(xyz + v1);
		geometryData.push_back(xyz + v2);
		geometryData.push_back(xyz + v2);
		geometryData.push_back(xyz + v1);
		geometryData.push_back(xyz + v3);

		bottomLevelAccelerationStructure->addGeometry(geometryData, true);
		bottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> RayTracingPipelineLibraryTestInstance::initTopAccelerationStructure (DeviceHelper& deviceHelper, VkCommandBuffer cmdBuffer,
																												std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
{
	const auto&									vkd			= *deviceHelper.vkd;
	const auto									device		= deviceHelper.device.get();
	auto&										allocator	= *deviceHelper.allocator;

	deUint32 instanceCount = m_data.width * m_data.height / 2;

	de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();
	result->setInstanceCount(instanceCount);

	deUint32 currentInstanceIndex	= 0;
	deUint32 numShadersUsed			= m_data.libraryConfiguration.pipelineShaders;
	for (auto it = begin(m_data.libraryConfiguration.pipelineLibraries), eit = end(m_data.libraryConfiguration.pipelineLibraries); it != eit; ++it)
		numShadersUsed += it->y();

	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		if (((x + y) % 2) == 0)
			continue;
		const VkTransformMatrixKHR			identityMatrix =
		{
			{								//  float	matrix[3][4];
				{ 1.0f, 0.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 1.0f, 0.0f },
			}
		};

		result->addInstance(bottomLevelAccelerationStructures[currentInstanceIndex], identityMatrix, 0, 0xFF, currentInstanceIndex % numShadersUsed, 0U);
		currentInstanceIndex++;
	}
	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

void compileShaders (DeviceHelper& deviceHelper, Context& context, de::SharedPtr<de::MovePtr<RayTracingPipeline>>& pipeline, const std::vector<std::tuple<std::string, VkShaderStageFlagBits>>& shaderData)
{
	const auto&	vkd		= *deviceHelper.vkd;
	const auto	device	= deviceHelper.device.get();

	for (deUint32 i=0; i< shaderData.size(); ++i)
	{
		std::string				shaderName;
		VkShaderStageFlagBits	shaderStage;
		std::tie(shaderName, shaderStage) = shaderData[i];
		pipeline->get()->addShader(shaderStage, createShaderModule(vkd, device, context.getBinaryCollection().get(shaderName), 0), i);
	}
}

struct CompileShadersMultithreadData
{
	DeviceHelper&														deviceHelper;
	Context&															context;
	de::SharedPtr<de::MovePtr<RayTracingPipeline>>&						pipeline;
	const std::vector<std::tuple<std::string, VkShaderStageFlagBits>>&	shaderData;
};

void compileShadersThread (void* param)
{
	CompileShadersMultithreadData* csmd = (CompileShadersMultithreadData*)param;
	compileShaders(csmd->deviceHelper, csmd->context, csmd->pipeline, csmd->shaderData);
}

de::MovePtr<BufferWithMemory> RayTracingPipelineLibraryTestInstance::runTest (DeviceHelper& deviceHelper)
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const auto&							vkd									= *deviceHelper.vkd;
	const auto							device								= deviceHelper.device.get();
	const auto							queueFamilyIndex					= deviceHelper.queueFamilyIndex;
	const auto							queue								= deviceHelper.queue;
	auto&								allocator							= *deviceHelper.allocator;
	const deUint32						pixelCount							= m_data.height * m_data.width;
	const deUint32						shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);
	const deUint32						shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);

	const Move<VkDescriptorSetLayout>	descriptorSetLayout					= DescriptorSetLayoutBuilder()
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
																					.build(vkd, device);
	const Move<VkDescriptorPool>		descriptorPool						= DescriptorPoolBuilder()
																					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																					.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const Move<VkDescriptorSet>			descriptorSet						= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	const Move<VkPipelineLayout>		pipelineLayout						= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	// sort pipeline library configurations ( including main pipeline )
	std::vector<std::tuple<int, deUint32, deUint32>> libraryList;
	{
		// push main pipeline on the list
		deUint32 shaderOffset	= 0U;
		libraryList.push_back(std::make_tuple(-1, shaderOffset, m_data.libraryConfiguration.pipelineShaders));
		shaderOffset			+= m_data.libraryConfiguration.pipelineShaders;

		for (size_t i = 0; i < m_data.libraryConfiguration.pipelineLibraries.size(); ++i)
		{
			int parentIndex			= m_data.libraryConfiguration.pipelineLibraries[i].x();
			deUint32 shaderCount	= deUint32(m_data.libraryConfiguration.pipelineLibraries[i].y());
			if (parentIndex < 0 || parentIndex >= int(libraryList.size()) )
				TCU_THROW(InternalError, "Wrong library tree definition");
			libraryList.push_back(std::make_tuple(parentIndex, shaderOffset, shaderCount));
			shaderOffset			+= shaderCount;
		}
	}

	// create pipeline libraries
	std::vector<de::SharedPtr<de::MovePtr<RayTracingPipeline>>>					pipelineLibraries(libraryList.size());
	std::vector<std::vector<std::tuple<std::string, VkShaderStageFlagBits>>>	pipelineShaders(libraryList.size());
	for (size_t idx=0; idx < libraryList.size(); ++idx)
	{
		int			parentIndex;
		deUint32	shaderCount, shaderOffset;
		std::tie(parentIndex, shaderOffset, shaderCount) = libraryList[idx];

		// create pipeline objects
		de::SharedPtr<de::MovePtr<RayTracingPipeline>> pipeline = makeVkSharedPtr(de::MovePtr<RayTracingPipeline>(new RayTracingPipeline));

		(*pipeline)->setDeferredOperation(m_data.pipelinesCreatedUsingDHO);

		// all pipelines are pipeline libraries, except for the main pipeline
		if(idx>0)
			pipeline->get()->setCreateFlags(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);
		pipeline->get()->setMaxPayloadSize(16U); // because rayPayloadInEXT is uvec4 ( = 16 bytes ) for all chit shaders
		pipelineLibraries[idx] = pipeline;

		// prepare all shader names for all pipelines
		if (idx == 0)
		{
			pipelineShaders[0].push_back(std::make_tuple( "rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR ));
			pipelineShaders[0].push_back(std::make_tuple( "miss", VK_SHADER_STAGE_MISS_BIT_KHR ));
		}
		for ( deUint32 i=0; i < shaderCount; ++i)
		{
			std::stringstream csname;
			csname << "chit" << shaderOffset + i;
			pipelineShaders[idx].push_back(std::make_tuple( csname.str(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR ));
		}
	}
	// singlethreaded / multithreaded compilation of all shaders
	if (m_data.multithreadedCompilation)
	{
		std::vector<CompileShadersMultithreadData> csmds;
		for (deUint32 i = 0; i < pipelineLibraries.size(); ++i)
			csmds.push_back(CompileShadersMultithreadData{ deviceHelper, m_context, pipelineLibraries[i], pipelineShaders[i] });

		std::vector<deThread>	threads;
		for (deUint32 i = 0; i < csmds.size(); ++i)
			threads.push_back(deThread_create(compileShadersThread, (void*)&csmds[i], DE_NULL));

		for (deUint32 i = 0; i < threads.size(); ++i)
		{
			deThread_join(threads[i]);
			deThread_destroy(threads[i]);
		}
	}
	else // m_data.multithreadedCompilation == false
	{
		for (deUint32 i = 0; i < pipelineLibraries.size(); ++i)
			compileShaders(deviceHelper, m_context, pipelineLibraries[i], pipelineShaders[i]);
	}

	// connect libraries into a tree structure
	for (size_t idx = 0; idx < libraryList.size(); ++idx)
	{
		int			parentIndex;
		deUint32 shaderCount, shaderOffset;
		std::tie(parentIndex, shaderCount, shaderOffset) = libraryList[idx];
		if (parentIndex != -1)
			pipelineLibraries[parentIndex]->get()->addLibrary(pipelineLibraries[idx]);
	}

	// build main pipeline and all pipeline libraries that it depends on
	std::vector<de::SharedPtr<Move<VkPipeline>>>	pipelines				= pipelineLibraries[0]->get()->createPipelineWithLibraries(vkd, device, *pipelineLayout);
	DE_ASSERT(pipelines.size() > 0);
	VkPipeline pipeline = pipelines[0]->get();

	deUint32							numShadersUsed						= m_data.libraryConfiguration.pipelineShaders;
	for (auto it = begin(m_data.libraryConfiguration.pipelineLibraries), eit = end(m_data.libraryConfiguration.pipelineLibraries); it != eit; ++it)
		numShadersUsed += it->y();

	// build shader binding tables
	const de::MovePtr<BufferWithMemory>	raygenShaderBindingTable			= pipelineLibraries[0]->get()->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
	const de::MovePtr<BufferWithMemory>	missShaderBindingTable				= pipelineLibraries[0]->get()->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1 );
	const de::MovePtr<BufferWithMemory>	hitShaderBindingTable				= pipelineLibraries[0]->get()->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, numShadersUsed);
	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, numShadersUsed * shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	const VkFormat						imageFormat							= VK_FORMAT_R32_UINT;
	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, imageFormat);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, imageFormat, imageSubresourceRange);

	const VkBufferCreateInfo			resultBufferCreateInfo				= makeBufferCreateInfo(pixelCount*sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				resultBufferImageRegion				= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 1), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		resultBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructure;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		const VkImageMemoryBarrier			preImageBarrier						= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																					**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);

		const VkClearValue					clearValue							= makeClearValueColorU32(0xFF, 0u, 0u, 0u);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);

		const VkImageMemoryBarrier			postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
																					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																					**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		bottomLevelAccelerationStructures	= initBottomAccelerationStructures(deviceHelper, *cmdBuffer);
		topLevelAccelerationStructure		= initTopAccelerationStructure(deviceHelper, *cmdBuffer, bottomLevelAccelerationStructures);

		const TopLevelAccelerationStructure*			topLevelAccelerationStructurePtr		= topLevelAccelerationStructure.get();
		VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			1u,																	//  deUint32							accelerationStructureCount;
			topLevelAccelerationStructurePtr->getPtr(),							//  const VkAccelerationStructureKHR*	pAccelerationStructures;
		};

		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
			.update(vkd, device);

		vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

		vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);

		cmdTraceRays(vkd,
			*cmdBuffer,
			&raygenShaderBindingTableRegion,
			&missShaderBindingTableRegion,
			&hitShaderBindingTableRegion,
			&callableShaderBindingTableRegion,
			m_data.width, m_data.height, 1);

		const VkMemoryBarrier							postTraceMemoryBarrier					= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		const VkMemoryBarrier							postCopyMemoryBarrier					= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	return resultBuffer;
}

tcu::TestStatus RayTracingPipelineLibraryTestInstance::iterate (void)
{
	// run test using arrays of pointers
	DeviceHelper						deviceHelper(m_context);
	const de::MovePtr<BufferWithMemory>	buffer		= runTest(deviceHelper);
	const deUint32*						bufferPtr	= (deUint32*)buffer->getAllocation().getHostPtr();

	deUint32							failures		= 0;
	deUint32							pos				= 0;
	deUint32							shaderIdx		= 0;
	deUint32							numShadersUsed	= m_data.libraryConfiguration.pipelineShaders;
	for (auto it = begin(m_data.libraryConfiguration.pipelineLibraries), eit = end(m_data.libraryConfiguration.pipelineLibraries); it != eit; ++it)
		numShadersUsed += it->y();

	// verify results
	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		deUint32 expectedResult;
		if ((x + y) % 2)
		{
			expectedResult = shaderIdx % numShadersUsed;
			++shaderIdx;
		}
		else
			expectedResult = RTPL_MAX_CHIT_SHADER_COUNT;

		if (bufferPtr[pos] != expectedResult)
			failures++;
		++pos;
	}

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail (failures=" + de::toString(failures) + ")");
}

}	// anonymous

void addPipelineLibraryConfigurationsTests (tcu::TestCaseGroup* group)
{
	struct ThreadData
	{
		bool									multithreaded;
		bool									pipelinesCreatedUsingDHO;
		const char*								name;
	} threadData[] =
	{
		{ false,	false,	"singlethreaded_compilation"	},
		{ true,		false,	"multithreaded_compilation"		},
		{ true,		true,	"multithreaded_compilation_dho"	},
	};

	struct LibraryConfigurationData
	{
		LibraryConfiguration		libraryConfiguration;
		const char*					name;
	} libraryConfigurationData[] =
	{
		{ {0, { { 0, 1 } } },								"s0_l1"			},	// 0 shaders in a main pipeline. 1 pipeline library with 1 shader
		{ {1, { { 0, 1 } } },								"s1_l1"			},	// 1 shader  in a main pipeline. 1 pipeline library with 1 shader
		{ {0, { { 0, 1 }, { 0, 1 } } },						"s0_l11"		},	// 0 shaders in a main pipeline. 2 pipeline libraries with 1 shader each
		{ {3, { { 0, 1 }, { 0, 1 } } },						"s3_l11"		},	// 3 shaders in a main pipeline. 2 pipeline libraries with 1 shader each
		{ {0, { { 0, 2 }, { 0, 3 } } },						"s0_l23"		},	// 0 shaders in a main pipeline. 2 pipeline libraries with 2 and 3 shaders respectively
		{ {2, { { 0, 2 }, { 0, 3 } } },						"s2_l23"		},	// 2 shaders in a main pipeline. 2 pipeline libraries with 2 and 3 shaders respectively
		{ {0, { { 0, 1 }, { 1, 1 } } },						"s0_l1_l1"		},	// 0 shaders in a main pipeline. 2 pipeline libraries with 1 shader each. Second library is a child of a first library
		{ {1, { { 0, 1 }, { 1, 1 } } },						"s1_l1_l1"		},	// 1 shader  in a main pipeline. 2 pipeline libraries with 1 shader each. Second library is a child of a first library
		{ {0, { { 0, 2 }, { 1, 3 } } },						"s0_l2_l3"		},	// 0 shaders in a main pipeline. 2 pipeline libraries with 2 and 3 shaders respectively. Second library is a child of a first library
		{ {3, { { 0, 2 }, { 1, 3 } } },						"s3_l2_l3"		},	// 3 shaders in a main pipeline. 2 pipeline libraries with 2 and 3 shaders respectively. Second library is a child of a first library
		{ {3, { { 0, 2 }, { 0, 3 }, { 0, 2 } } },			"s3_l232"		},	// 3 shaders in a main pipeline. 3 pipeline libraries with 2, 3 and 2 shaders respectively.
		{ {3, { { 0, 2 }, { 1, 2 }, { 1, 2 }, { 0, 2 } } },	"s3_l22_l22"	},	// 3 shaders in a main pipeline. 4 pipeline libraries with 2 shaders each. Second and third library is a child of a first library
	};

	for (size_t threadNdx = 0; threadNdx < DE_LENGTH_OF_ARRAY(threadData); ++threadNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> threadGroup(new tcu::TestCaseGroup(group->getTestContext(), threadData[threadNdx].name, ""));

		for (size_t libConfigNdx = 0; libConfigNdx < DE_LENGTH_OF_ARRAY(libraryConfigurationData); ++libConfigNdx)
		{
			TestParams testParams
			{
				libraryConfigurationData[libConfigNdx].libraryConfiguration,
				threadData[threadNdx].multithreaded,
				threadData[threadNdx].pipelinesCreatedUsingDHO,
				RTPL_DEFAULT_SIZE,
				RTPL_DEFAULT_SIZE
			};
			threadGroup->addChild(new RayTracingPipelineLibraryTestCase(group->getTestContext(), libraryConfigurationData[libConfigNdx].name, "", testParams));
		}
		group->addChild(threadGroup.release());
	}
}

tcu::TestCaseGroup*	createPipelineLibraryTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "pipeline_library", "Tests verifying pipeline libraries"));

	addTestGroup(group.get(), "configurations", "Test different configurations of pipeline libraries", addPipelineLibraryConfigurationsTests);

	return group.release();
}

}	// RayTracing

}	// vkt
