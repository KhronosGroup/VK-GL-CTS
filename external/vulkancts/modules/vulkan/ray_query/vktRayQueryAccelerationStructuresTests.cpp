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
 * \brief Testing acceleration structures in ray query extension
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryAccelerationStructuresTests.hpp"

#include <array>
#include <set>
#include <limits>

#include "vkDefs.hpp"
#include "deClock.h"
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
#include "deRandom.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuFloat.hpp"

namespace vkt
{
namespace RayQuery
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

enum ShaderSourcePipeline
{
	SSP_GRAPHICS_PIPELINE,
	SSP_COMPUTE_PIPELINE,
	SSP_RAY_TRACING_PIPELINE
};

enum ShaderSourceType
{
	SST_VERTEX_SHADER,
	SST_TESSELATION_CONTROL_SHADER,
	SST_TESSELATION_EVALUATION_SHADER,
	SST_GEOMETRY_SHADER,
	SST_FRAGMENT_SHADER,
	SST_COMPUTE_SHADER,
	SST_RAY_GENERATION_SHADER,
	SST_INTERSECTION_SHADER,
	SST_ANY_HIT_SHADER,
	SST_CLOSEST_HIT_SHADER,
	SST_MISS_SHADER,
	SST_CALLABLE_SHADER,
};

enum ShaderTestType
{
	STT_GENERATE_INTERSECTION		= 0,
	STT_SKIP_INTERSECTION			= 1,
};

enum BottomTestType
{
	BTT_TRIANGLES,
	BTT_AABBS
};

enum TopTestType
{
	TTT_IDENTICAL_INSTANCES,
	TTT_DIFFERENT_INSTANCES
};

enum OperationTarget
{
	OT_NONE,
	OT_TOP_ACCELERATION,
	OT_BOTTOM_ACCELERATION
};

enum OperationType
{
	OP_NONE,
	OP_COPY,
	OP_COMPACT,
	OP_SERIALIZE
};

enum class InstanceCullFlags
{
	NONE,
	CULL_DISABLE,
	COUNTERCLOCKWISE,
	ALL,
};

enum class EmptyAccelerationStructureCase
{
	NOT_EMPTY				= 0,
	INACTIVE_TRIANGLES		= 1,
	INACTIVE_INSTANCES		= 2,
	NO_GEOMETRIES_BOTTOM	= 3,	// geometryCount zero when building.
	NO_PRIMITIVES_BOTTOM	= 4,	// primitiveCount zero when building.
	NO_PRIMITIVES_TOP		= 5,	// primitiveCount zero when building.
};

const deUint32			TEST_WIDTH			= 8;
const deUint32			TEST_HEIGHT			= 8;

struct TestParams;

class TestConfiguration
{
public:
	virtual					~TestConfiguration					();
	virtual void			initConfiguration					(Context&						context,
																 TestParams&					testParams) = 0;
	virtual void			fillCommandBuffer					(Context&						context,
																 TestParams&					testParams,
																 VkCommandBuffer				commandBuffer,
																 const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
																 const VkDescriptorImageInfo&	resultImageInfo) = 0;
	virtual bool			verifyImage							(BufferWithMemory*				resultBuffer,
																 Context&						context,
																 TestParams&					testParams) = 0;
	virtual VkFormat		getResultImageFormat				() = 0;
	virtual size_t			getResultImageFormatSize			() = 0;
	virtual VkClearValue	getClearValue						() = 0;
};

TestConfiguration::~TestConfiguration()
{
}

class SceneBuilder
{
public:
	virtual std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures	(Context&							context,
																												 TestParams&						testParams) = 0;
	virtual de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure		(Context&							context,
																												 TestParams&						testParams,
																												 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures) = 0;
};

struct TestParams
{
	ShaderSourceType						shaderSourceType;
	ShaderSourcePipeline					shaderSourcePipeline;
	vk::VkAccelerationStructureBuildTypeKHR	buildType;		// are we making AS on CPU or GPU
	VkFormat								vertexFormat;
	bool									padVertices;
	VkIndexType								indexType;
	BottomTestType							bottomTestType; // what kind of geometry is stored in bottom AS
	InstanceCullFlags						cullFlags;		// Flags for instances, if needed.
	bool									bottomUsesAOP;	// does bottom AS use arrays, or arrays of pointers
	bool									bottomGeneric;	// Bottom created as generic AS type.
	TopTestType								topTestType;	// If instances are identical then bottom geometries must have different vertices/aabbs
	bool									topUsesAOP;		// does top AS use arrays, or arrays of pointers
	bool									topGeneric;		// Top created as generic AS type.
	VkBuildAccelerationStructureFlagsKHR	buildFlags;
	OperationTarget							operationTarget;
	OperationType							operationType;
	deUint32								width;
	deUint32								height;
	deUint32								workerThreadsCount;
	EmptyAccelerationStructureCase			emptyASCase;
};

deUint32 getShaderGroupHandleSize (const InstanceInterface&	vki,
								   const VkPhysicalDevice	physicalDevice)
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

VkImageCreateInfo makeImageCreateInfo (deUint32 width, deUint32 height, deUint32 depth, VkFormat format)
{
	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,																// VkStructureType			sType;
		DE_NULL,																							// const void*				pNext;
		(VkImageCreateFlags)0u,																				// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_3D,																					// VkImageType				imageType;
		format,																								// VkFormat					format;
		makeExtent3D(width, height, depth),																	// VkExtent3D				extent;
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

Move<VkQueryPool> makeQueryPool(const DeviceInterface&		vk,
								const VkDevice				device,
								const VkQueryType			queryType,
								deUint32					queryCount)
{
	const VkQueryPoolCreateInfo				queryPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,		// sType
		DE_NULL,										// pNext
		(VkQueryPoolCreateFlags)0,						// flags
		queryType,										// queryType
		queryCount,										// queryCount
		0u,												// pipelineStatistics
	};
	return createQueryPool(vk, device, &queryPoolCreateInfo);
}


bool registerShaderModule (const DeviceInterface&								vkd,
						   const VkDevice										device,
						   Context&												context,
						   std::vector<de::SharedPtr<Move<VkShaderModule>>>&	shaderModules,
						   std::vector<VkPipelineShaderStageCreateInfo>&		shaderCreateInfos,
						   VkShaderStageFlagBits								stage,
						   const std::string&									externalNamePart,
						   const std::string&									internalNamePart)
{
	char fullShaderName[40];
	snprintf(fullShaderName, 40, externalNamePart.c_str(), internalNamePart.c_str());
	std::string fsn = fullShaderName;
	if (fsn.empty())
		return false;

	shaderModules.push_back(makeVkSharedPtr(createShaderModule(vkd, device, context.getBinaryCollection().get(fsn), 0)));

	shaderCreateInfos.push_back(
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			stage,														// stage
			shaderModules.back()->get(),								// shader
			"main",
			DE_NULL,													// pSpecializationInfo
		});

	return true;
}

bool registerShaderModule (const DeviceInterface&	vkd,
						   const VkDevice			device,
						   Context&					context,
						   RayTracingPipeline&		rayTracingPipeline,
						   VkShaderStageFlagBits	shaderStage,
						   const std::string&		externalNamePart,
						   const std::string&		internalNamePart,
						   deUint32					groupIndex)
{
	char fullShaderName[40];
	snprintf(fullShaderName, 40, externalNamePart.c_str(), internalNamePart.c_str());
	std::string fsn = fullShaderName;
	if (fsn.empty())
		return false;
	Move<VkShaderModule> shaderModule = createShaderModule(vkd, device, context.getBinaryCollection().get(fsn), 0);
	if (*shaderModule == DE_NULL)
		return false;
	rayTracingPipeline.addShader(shaderStage, shaderModule, groupIndex);
	return true;
}

VkGeometryInstanceFlagsKHR getCullFlags (InstanceCullFlags flags)
{
	VkGeometryInstanceFlagsKHR cullFlags = 0u;

	if (flags == InstanceCullFlags::CULL_DISABLE || flags == InstanceCullFlags::ALL)
		cullFlags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

	if (flags == InstanceCullFlags::COUNTERCLOCKWISE || flags == InstanceCullFlags::ALL)
		cullFlags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;

	return cullFlags;
}

class GraphicsConfiguration : public TestConfiguration
{
public:
	virtual							~GraphicsConfiguration		();
	void							initConfiguration			(Context&						context,
																 TestParams&					testParams) override;
	void							fillCommandBuffer			(Context&						context,
																 TestParams&					testParams,
																 VkCommandBuffer				commandBuffer,
																 const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
																 const VkDescriptorImageInfo&	resultImageInfo) override;
	bool							verifyImage					(BufferWithMemory*				resultBuffer,
																 Context&						context,
																 TestParams&					testParams) override;
	VkFormat						getResultImageFormat		() override;
	size_t							getResultImageFormatSize	() override;
	VkClearValue					getClearValue				() override;
protected:
	Move<VkDescriptorSetLayout>		descriptorSetLayout;
	Move<VkDescriptorPool>			descriptorPool;
	Move<VkDescriptorSet>			descriptorSet;
	Move<VkPipelineLayout>			pipelineLayout;
	Move<VkRenderPass>				renderPass;
	Move<VkFramebuffer>				framebuffer;
	std::vector<de::SharedPtr<Move<VkShaderModule> > >	shaderModules;
	Move<VkPipeline>				pipeline;
	std::vector<tcu::Vec3>			vertices;
	Move<VkBuffer>					vertexBuffer;
	de::MovePtr<Allocation>			vertexAlloc;
};

GraphicsConfiguration::~GraphicsConfiguration()
{
	shaderModules.clear();
}

void GraphicsConfiguration::initConfiguration (Context&						context,
											   TestParams&					testParams)
{
	const DeviceInterface&										vkd								= context.getDeviceInterface();
	const VkDevice												device							= context.getDevice();
	const deUint32												queueFamilyIndex				= context.getUniversalQueueFamilyIndex();
	Allocator&													allocator						= context.getDefaultAllocator();

	descriptorSetLayout																			= DescriptorSetLayoutBuilder()
																										.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL_GRAPHICS)
																										.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL_GRAPHICS)
																										.build(vkd, device);
	descriptorPool																				= DescriptorPoolBuilder()
																										.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																										.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																										.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	descriptorSet																				= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	pipelineLayout																				= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	std::vector<std::string> rayQueryTestName;
	rayQueryTestName.push_back("as_triangle");
	rayQueryTestName.push_back("as_aabb");

	const std::map<ShaderSourceType,std::vector<std::string>>	shaderNames						=
	{
										//idx:		0				1				2				3				4
										//shader:	vert,			tesc,			tese,			geom,			frag,
		{	SST_VERTEX_SHADER,					{	"vert_%s",		"",				"",				"",				"",			}	},
		{	SST_TESSELATION_CONTROL_SHADER,		{	"vert",			"tesc_%s",		"tese",			"",				"",			}	},
		{	SST_TESSELATION_EVALUATION_SHADER,	{	"vert",			"tesc",			"tese_%s",		"",				"",			}	},
		{	SST_GEOMETRY_SHADER,				{	"vert_vid",		"",				"",				"geom_%s",		"",			}	},
		{	SST_FRAGMENT_SHADER,				{	"vert",			"",				"",				"",				"frag_%s",	}	},
	};

	auto														shaderNameIt					= shaderNames.find(testParams.shaderSourceType);
	if(shaderNameIt == end(shaderNames))
		TCU_THROW(InternalError, "Wrong shader source type");

	std::vector<VkPipelineShaderStageCreateInfo>				shaderCreateInfos;
	bool tescX, teseX, fragX;
			registerShaderModule(vkd,	device,	context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_VERTEX_BIT,						shaderNameIt->second[0],	rayQueryTestName[testParams.bottomTestType]);
	tescX = registerShaderModule(vkd,	device, context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		shaderNameIt->second[1],	rayQueryTestName[testParams.bottomTestType]);
	teseX = registerShaderModule(vkd,	device,	context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	shaderNameIt->second[2],	rayQueryTestName[testParams.bottomTestType]);
			registerShaderModule(vkd,	device,	context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_GEOMETRY_BIT,					shaderNameIt->second[3],	rayQueryTestName[testParams.bottomTestType]);
	fragX = registerShaderModule(vkd,	device,	context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_FRAGMENT_BIT,					shaderNameIt->second[4],	rayQueryTestName[testParams.bottomTestType]);

	const vk::VkSubpassDescription		subpassDesc			=
	{
		(vk::VkSubpassDescriptionFlags)0,
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,							// pipelineBindPoint
		0u,																// inputCount
		DE_NULL,														// pInputAttachments
		0u,																// colorCount
		DE_NULL,														// pColorAttachments
		DE_NULL,														// pResolveAttachments
		DE_NULL,														// depthStencilAttachment
		0u,																// preserveCount
		DE_NULL,														// pPreserveAttachments
	};
	const vk::VkRenderPassCreateInfo	renderPassParams	=
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,					// sType
		DE_NULL,														// pNext
		(vk::VkRenderPassCreateFlags)0,
		0u,																// attachmentCount
		DE_NULL,														// pAttachments
		1u,																// subpassCount
		&subpassDesc,													// pSubpasses
		0u,																// dependencyCount
		DE_NULL,														// pDependencies
	};

	renderPass = createRenderPass(vkd, device, &renderPassParams);

	const vk::VkFramebufferCreateInfo	framebufferParams	=
	{
		vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,					// sType
		DE_NULL,														// pNext
		(vk::VkFramebufferCreateFlags)0,
		*renderPass,													// renderPass
		0u,																// attachmentCount
		DE_NULL,														// pAttachments
		testParams.width,												// width
		testParams.height,												// height
		1u,																// layers
	};

	framebuffer = createFramebuffer(vkd, device, &framebufferParams);

	VkPrimitiveTopology					testTopology		= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	tcu::Vec3 v0(0.0f, 0.0f, 0.0f);
	tcu::Vec3 v1(float(testParams.width) - 1.0f, 0.0f, 0.0f);
	tcu::Vec3 v2(0.0f, float(testParams.height) - 1.0f, 0.0f);
	tcu::Vec3 v3(float(testParams.width) - 1.0f, float(testParams.height) - 1.0f, 0.0f);

	switch (testParams.shaderSourceType)
	{
		case SST_TESSELATION_CONTROL_SHADER:
		case SST_TESSELATION_EVALUATION_SHADER:
			testTopology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
			vertices.push_back(v0);
			vertices.push_back(v1);
			vertices.push_back(v2);
			vertices.push_back(v1);
			vertices.push_back(v3);
			vertices.push_back(v2);
			break;
		case SST_VERTEX_SHADER:
		case SST_GEOMETRY_SHADER:
			vertices.push_back(v0);
			vertices.push_back(v1);
			vertices.push_back(v2);
			vertices.push_back(v3);
			break;
		case SST_FRAGMENT_SHADER:
			vertices.push_back( tcu::Vec3(-1.0f,  1.0f, 0.0f) );
			vertices.push_back( tcu::Vec3(-1.0f, -1.0f, 0.0f) );
			vertices.push_back( tcu::Vec3( 1.0f,  1.0f, 0.0f) );
			vertices.push_back( tcu::Vec3( 1.0f, -1.0f, 0.0f) );
			break;
		default:
			TCU_THROW(InternalError, "Wrong shader source type");
	};

	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,																// uint32_t											binding;
		sizeof(tcu::Vec3),												// uint32_t											stride;
		VK_VERTEX_INPUT_RATE_VERTEX,									// VkVertexInputRate								inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescription =
	{
		0u,																// uint32_t											location;
		0u,																// uint32_t											binding;
		VK_FORMAT_R32G32B32_SFLOAT,										// VkFormat											format;
		0u,																// uint32_t											offset;
	};

	const VkPipelineVertexInputStateCreateInfo					vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags			flags;
		1u,																// deUint32											vertexBindingDescriptionCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*			pVertexBindingDescriptions;
		1u,																// deUint32											vertexAttributeDescriptionCount;
		&vertexInputAttributeDescription								// const VkVertexInputAttributeDescription*			pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo				inputAssemblyStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags			flags;
		testTopology,													// VkPrimitiveTopology								topology;
		VK_FALSE														// VkBool32											primitiveRestartEnable;
	};

	const VkPipelineTessellationStateCreateInfo					tessellationStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,		// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		VkPipelineTessellationStateCreateFlags(0u),						// VkPipelineTessellationStateCreateFlags			flags;
		3u																// deUint32											patchControlPoints;
	};

	VkViewport													viewport						= makeViewport(testParams.width, testParams.height);
	VkRect2D													scissor							= makeRect2D(testParams.width, testParams.height);

	const VkPipelineViewportStateCreateInfo						viewportStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType									sType
		DE_NULL,														// const void*										pNext
		(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags				flags
		1u,																// deUint32											viewportCount
		&viewport,														// const VkViewport*								pViewports
		1u,																// deUint32											scissorCount
		&scissor														// const VkRect2D*									pScissors
	};

	const VkPipelineRasterizationStateCreateInfo				rasterizationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags			flags;
		VK_FALSE,														// VkBool32											depthClampEnable;
		fragX ? VK_FALSE : VK_TRUE,										// VkBool32											rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode									polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags									cullMode;
		VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace										frontFace;
		VK_FALSE,														// VkBool32											depthBiasEnable;
		0.0f,															// float											depthBiasConstantFactor;
		0.0f,															// float											depthBiasClamp;
		0.0f,															// float											depthBiasSlopeFactor;
		1.0f															// float											lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		(VkPipelineMultisampleStateCreateFlags)0,						// VkPipelineMultisampleStateCreateFlags			flags;
		VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits							rasterizationSamples;
		VK_FALSE,														// VkBool32											sampleShadingEnable;
		0.0f,															// float											minSampleShading;
		DE_NULL,														// const VkSampleMask*								pSampleMask;
		VK_FALSE,														// VkBool32											alphaToCoverageEnable;
		VK_FALSE														// VkBool32											alphaToOneEnable;
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		(VkPipelineColorBlendStateCreateFlags)0,						// VkPipelineColorBlendStateCreateFlags				flags;
		DE_FALSE,														// VkBool32											logicOpEnable;
		VK_LOGIC_OP_CLEAR,												// VkLogicOp										logicOp;
		0,																// deUint32											attachmentCount;
		DE_NULL,														// const VkPipelineColorBlendAttachmentState*		pAttachments;
		{ 1.0f, 1.0f, 1.0f, 1.0f }										// float											blendConstants[4];
	};

	const VkGraphicsPipelineCreateInfo							graphicsPipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,				// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		(VkPipelineCreateFlags)0,										// VkPipelineCreateFlags							flags;
		static_cast<deUint32>(shaderCreateInfos.size()),				// deUint32											stageCount;
		shaderCreateInfos.data(),										// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateCreateInfo,									// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyStateCreateInfo,									// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		(tescX||teseX) ? &tessellationStateCreateInfo : DE_NULL,		// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		fragX ? &viewportStateCreateInfo : DE_NULL,						// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&rasterizationStateCreateInfo,									// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		fragX ? &multisampleStateCreateInfo : DE_NULL,					// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		DE_NULL,														// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		fragX ? &colorBlendStateCreateInfo : DE_NULL,					// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,														// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout.get(),											// VkPipelineLayout									layout;
		renderPass.get(),												// VkRenderPass										renderPass;
		0u,																// deUint32											subpass;
		DE_NULL,														// VkPipeline										basePipelineHandle;
		0																// int												basePipelineIndex;
	};

	pipeline = createGraphicsPipeline(vkd, device, DE_NULL, &graphicsPipelineCreateInfo);

	const VkBufferCreateInfo									vertexBufferParams				=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,							// VkStructureType									sType;
		DE_NULL,														// const void*										pNext;
		0u,																// VkBufferCreateFlags								flags;
		VkDeviceSize(sizeof(tcu::Vec3) * vertices.size()),				// VkDeviceSize										size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,							// VkBufferUsageFlags								usage;
		VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode									sharingMode;
		1u,																// deUint32											queueFamilyIndexCount;
		&queueFamilyIndex												// const deUint32*									pQueueFamilyIndices;
	};

	vertexBuffer	= createBuffer(vkd, device, &vertexBufferParams);
	vertexAlloc		= allocator.allocate(getBufferMemoryRequirements(vkd, device, *vertexBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vkd.bindBufferMemory(device, *vertexBuffer, vertexAlloc->getMemory(), vertexAlloc->getOffset()));

	// Upload vertex data
	deMemcpy(vertexAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(tcu::Vec3));
	flushAlloc(vkd, device, *vertexAlloc);
}

void GraphicsConfiguration::fillCommandBuffer (Context&						context,
											   TestParams&					testParams,
											   VkCommandBuffer				commandBuffer,
											   const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
											   const VkDescriptorImageInfo&	resultImageInfo)
{
	const DeviceInterface&				vkd									= context.getDeviceInterface();
	const VkDevice						device								= context.getDevice();

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &rayQueryAccelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	const VkRenderPassBeginInfo			renderPassBeginInfo					=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,							// VkStructureType								sType;
		DE_NULL,															// const void*									pNext;
		*renderPass,														// VkRenderPass									renderPass;
		*framebuffer,														// VkFramebuffer								framebuffer;
		makeRect2D(testParams.width, testParams.height),					// VkRect2D										renderArea;
		0u,																	// uint32_t										clearValueCount;
		DE_NULL																// const VkClearValue*							pClearValues;
	};
	VkDeviceSize						vertexBufferOffset					= 0u;

	vkd.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
	vkd.cmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
	vkd.cmdDraw(commandBuffer, deUint32(vertices.size()), 1, 0, 0);
	vkd.cmdEndRenderPass(commandBuffer);
}

bool GraphicsConfiguration::verifyImage (BufferWithMemory*					resultBuffer,
										 Context&							context,
										 TestParams&						testParams)
{
	// create result image
	const bool					allMiss							= (testParams.emptyASCase != EmptyAccelerationStructureCase::NOT_EMPTY);
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 2, resultBuffer->getAllocation().getHostPtr());

	// create reference image
	std::vector<deUint32>		reference(testParams.width * testParams.height * 2);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 2, reference.data());

	std::vector<std::vector<deUint32>> primitives				=
	{
		{0, 1, 2},
		{1, 3, 2}
	};

	tcu::UVec4					hitValue0						= tcu::UVec4(1, 0, 0, 0);
	tcu::UVec4					hitValue1						= tcu::UVec4(1, 0, 0, 0);
	tcu::UVec4					missValue						= tcu::UVec4(0, 0, 0, 0);
	tcu::UVec4					clearValue						= tcu::UVec4(0xFF, 0, 0, 0);

	switch (testParams.shaderSourceType)
	{
		case SST_VERTEX_SHADER:
			tcu::clear(referenceAccess, clearValue);
			for (deUint32 vertexNdx = 0; vertexNdx < 4; ++vertexNdx)
			{
				if (!allMiss && (vertexNdx == 1 || vertexNdx == 2))
				{
					referenceAccess.setPixel(hitValue0, vertexNdx, 0, 0);
					referenceAccess.setPixel(hitValue1, vertexNdx, 0, 1);
				}
				else
				{
					referenceAccess.setPixel(missValue, vertexNdx, 0, 0);
					referenceAccess.setPixel(missValue, vertexNdx, 0, 1);
				}
			}
			break;
		case SST_TESSELATION_EVALUATION_SHADER:
		case SST_TESSELATION_CONTROL_SHADER:
		case SST_GEOMETRY_SHADER:
			tcu::clear(referenceAccess, clearValue);
			for (deUint32 primitiveNdx = 0; primitiveNdx < primitives.size(); ++primitiveNdx)
			for (deUint32 vertexNdx = 0; vertexNdx < 3; ++vertexNdx)
			{
				deUint32 vNdx = primitives[primitiveNdx][vertexNdx];
				if (!allMiss && (vNdx==1 || vNdx==2))
				{
					referenceAccess.setPixel(hitValue0, primitiveNdx, vertexNdx, 0);
					referenceAccess.setPixel(hitValue1, primitiveNdx, vertexNdx, 1);
				}
				else
				{
					referenceAccess.setPixel(missValue, primitiveNdx, vertexNdx, 0);
					referenceAccess.setPixel(missValue, primitiveNdx, vertexNdx, 1);
				}
			}
			break;
		case SST_FRAGMENT_SHADER:
			tcu::clear(referenceAccess, missValue);
			for (deUint32 y = 0; y < testParams.height; ++y)
			for (deUint32 x = 0; x < testParams.width; ++x)
			{
				if (allMiss || ((x + y) % 2) == 0)
					continue;

				referenceAccess.setPixel(hitValue0, x, y, 0);
				referenceAccess.setPixel(hitValue1, x, y, 1);
			}
			break;
		default:
			TCU_THROW(InternalError, "Wrong shader source type");
	};

	// compare result and reference
	return tcu::intThresholdCompare(context.getTestContext().getLog(), "Result comparison", "", referenceAccess, resultAccess, tcu::UVec4(0), tcu::COMPARE_LOG_RESULT);
}

VkFormat GraphicsConfiguration::getResultImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

size_t GraphicsConfiguration::getResultImageFormatSize ()
{
	return sizeof(deUint32);
}

VkClearValue GraphicsConfiguration::getClearValue ()
{
	return makeClearValueColorU32(0xFF, 0u, 0u, 0u);
}

class ComputeConfiguration : public TestConfiguration
{
public:
	virtual							~ComputeConfiguration		();
	void							initConfiguration			(Context&						context,
																 TestParams&					testParams) override;
	void							fillCommandBuffer			(Context&						context,
																 TestParams&					testParams,
																 VkCommandBuffer				commandBuffer,
																 const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
																 const VkDescriptorImageInfo&	resultImageInfo) override;
	bool							verifyImage					(BufferWithMemory*				resultBuffer,
																 Context&						context,
																 TestParams&					testParams) override;
	VkFormat						getResultImageFormat		() override;
	size_t							getResultImageFormatSize	() override;
	VkClearValue					getClearValue				() override;
protected:
	Move<VkDescriptorSetLayout>		descriptorSetLayout;
	Move<VkDescriptorPool>			descriptorPool;
	Move<VkDescriptorSet>			descriptorSet;
	Move<VkPipelineLayout>			pipelineLayout;
	Move<VkShaderModule>			shaderModule;
	Move<VkPipeline>				pipeline;
};

ComputeConfiguration::~ComputeConfiguration()
{
}

void ComputeConfiguration::initConfiguration (Context&						context,
											  TestParams&					testParams)
{
	const DeviceInterface&				vkd									= context.getDeviceInterface();
	const VkDevice						device								= context.getDevice();

	descriptorSetLayout														= DescriptorSetLayoutBuilder()
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_COMPUTE_BIT)
																					.build(vkd, device);
	descriptorPool															= DescriptorPoolBuilder()
																					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																					.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	descriptorSet															= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	pipelineLayout															= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	std::vector<std::string> rayQueryTestName;
	rayQueryTestName.push_back("comp_as_triangle");
	rayQueryTestName.push_back("comp_as_aabb");

	shaderModule															= createShaderModule(vkd, device, context.getBinaryCollection().get(rayQueryTestName[testParams.bottomTestType]), 0u);
	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		*shaderModule,											// VkShaderModule						module;
		"main",													// const char*							pName;
		DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
		*pipelineLayout,									// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};

	pipeline																= createComputePipeline(vkd, device, DE_NULL, &pipelineCreateInfo);
}

void ComputeConfiguration::fillCommandBuffer (Context&						context,
											  TestParams&					testParams,
											  VkCommandBuffer				commandBuffer,
											  const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
											  const VkDescriptorImageInfo&	resultImageInfo)
{
	const DeviceInterface&				vkd									= context.getDeviceInterface();
	const VkDevice						device								= context.getDevice();

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &rayQueryAccelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

	vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vkd.cmdDispatch(commandBuffer, testParams.width, testParams.height, 1);
}

bool ComputeConfiguration::verifyImage (BufferWithMemory*					resultBuffer,
										Context&							context,
										TestParams&							testParams)
{
	// create result image
	const bool					allMiss							= (testParams.emptyASCase != EmptyAccelerationStructureCase::NOT_EMPTY);
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 2, resultBuffer->getAllocation().getHostPtr());

	// create reference image
	std::vector<deUint32>		reference(testParams.width * testParams.height * 2);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 2, reference.data());

	tcu::UVec4 hitValue0	= tcu::UVec4(1, 0, 0, 0);
	tcu::UVec4 hitValue1	= tcu::UVec4(1, 0, 0, 0);
	tcu::UVec4 missValue	= tcu::UVec4(0, 0, 0, 0);

	tcu::clear(referenceAccess, missValue);

	for (deUint32 y = 0; y < testParams.height; ++y)
	for (deUint32 x = 0; x < testParams.width; ++x)
	{
		if (allMiss || ((x + y) % 2) == 0)
			continue;

		referenceAccess.setPixel(hitValue0, x, y, 0);
		referenceAccess.setPixel(hitValue1, x, y, 1);
	}

	// compare result and reference
	return tcu::intThresholdCompare(context.getTestContext().getLog(), "Result comparison", "", referenceAccess, resultAccess, tcu::UVec4(0), tcu::COMPARE_LOG_RESULT);
}

VkFormat ComputeConfiguration::getResultImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

size_t ComputeConfiguration::getResultImageFormatSize ()
{
	return sizeof(deUint32);
}

VkClearValue ComputeConfiguration::getClearValue ()
{
	return makeClearValueColorU32(0xFF, 0u, 0u, 0u);
}

class RayTracingConfiguration : public TestConfiguration
{
public:
	virtual							~RayTracingConfiguration	();
	void							initConfiguration			(Context&						context,
																 TestParams&					testParams) override;
	void							fillCommandBuffer			(Context&						context,
																 TestParams&					testParams,
																 VkCommandBuffer				commandBuffer,
																 const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
																 const VkDescriptorImageInfo&	resultImageInfo) override;
	bool							verifyImage					(BufferWithMemory*				resultBuffer,
																 Context&						context,
																 TestParams&					testParams) override;
	VkFormat						getResultImageFormat		() override;
	size_t							getResultImageFormatSize	() override;
	VkClearValue					getClearValue				() override;
protected:
	Move<VkDescriptorSetLayout>		descriptorSetLayout;
	Move<VkDescriptorPool>			descriptorPool;
	Move<VkDescriptorSet>			descriptorSet;
	Move<VkPipelineLayout>			pipelineLayout;

	de::MovePtr<RayTracingPipeline>	rayTracingPipeline;
	Move<VkPipeline>				rtPipeline;

	de::MovePtr<BufferWithMemory>	raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>	hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>	missShaderBindingTable;
	de::MovePtr<BufferWithMemory>	callableShaderBindingTable;

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructure;
};

RayTracingConfiguration::~RayTracingConfiguration()
{
}

void RayTracingConfiguration::initConfiguration (Context&						context,
												 TestParams&					testParams)
{
	const InstanceInterface&			vki									= context.getInstanceInterface();
	const DeviceInterface&				vkd									= context.getDeviceInterface();
	const VkDevice						device								= context.getDevice();
	const VkPhysicalDevice				physicalDevice						= context.getPhysicalDevice();
	Allocator&							allocator							= context.getDefaultAllocator();

	descriptorSetLayout														= DescriptorSetLayoutBuilder()
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
																					.build(vkd, device);
	descriptorPool															= DescriptorPoolBuilder()
																					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																					.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	descriptorSet															= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	pipelineLayout															= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	rayTracingPipeline														= de::newMovePtr<RayTracingPipeline>();

	const std::map<ShaderSourceType,std::vector<std::string>> shaderNames =
	{
								//idx:		0				1				2				3				4				5
								//shader:	rgen,			isect,			ahit,			chit,			miss,			call
								//group:	0				1				1				1				2				3
		{	SST_RAY_GENERATION_SHADER,	{	"rgen_%s",		"",				"",				"",				"",				""			}	},
		{	SST_INTERSECTION_SHADER,	{	"rgen",			"isect_%s",		"",				"chit_isect",	"miss",			""			}	},
		{	SST_ANY_HIT_SHADER,			{	"rgen",			"isect",		"ahit_%s",		"",				"miss",			""			}	},
		{	SST_CLOSEST_HIT_SHADER,		{	"rgen",			"isect",		"",				"chit_%s",		"miss",			""			}	},
		{	SST_MISS_SHADER,			{	"rgen",			"isect",		"",				"chit",			"miss_%s",		""			}	},
		{	SST_CALLABLE_SHADER,		{	"rgen_call",	"",				"",				"chit",			"miss",			"call_%s"	}	},
	};

	std::vector<std::string> rayQueryTestName;
	rayQueryTestName.push_back("as_triangle");
	rayQueryTestName.push_back("as_aabb");

	auto shaderNameIt = shaderNames.find(testParams.shaderSourceType);
	if(shaderNameIt == end(shaderNames))
		TCU_THROW(InternalError, "Wrong shader source type");

	bool rgenX, isectX, ahitX, chitX, missX, callX;
	rgenX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_RAYGEN_BIT_KHR,			shaderNameIt->second[0],	rayQueryTestName[testParams.bottomTestType],	0);
	if (testParams.shaderSourceType == SST_INTERSECTION_SHADER)
		isectX = registerShaderModule(vkd, device, context,		*rayTracingPipeline,	VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	shaderNameIt->second[1],	rayQueryTestName[testParams.bottomTestType],	1);
	else
		isectX = false;
	ahitX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		shaderNameIt->second[2],	rayQueryTestName[testParams.bottomTestType],	1);
	chitX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	shaderNameIt->second[3],	rayQueryTestName[testParams.bottomTestType],	1);
	missX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_MISS_BIT_KHR,			shaderNameIt->second[4],	rayQueryTestName[testParams.bottomTestType],	2);
	callX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_CALLABLE_BIT_KHR,		shaderNameIt->second[5],	rayQueryTestName[testParams.bottomTestType],	3);
	bool hitX = isectX || ahitX || chitX;

	rtPipeline																= rayTracingPipeline->createPipeline(vkd, device, *pipelineLayout);

	deUint32							shaderGroupHandleSize				= getShaderGroupHandleSize(vki, physicalDevice);
	deUint32							shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);

	if (rgenX)	raygenShaderBindingTable									= rayTracingPipeline->createShaderBindingTable(vkd, device, *rtPipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
	if (hitX)	hitShaderBindingTable										= rayTracingPipeline->createShaderBindingTable(vkd, device, *rtPipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
	if (missX)	missShaderBindingTable										= rayTracingPipeline->createShaderBindingTable(vkd, device, *rtPipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
	if (callX)	callableShaderBindingTable									= rayTracingPipeline->createShaderBindingTable(vkd, device, *rtPipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, 1);
}

void RayTracingConfiguration::fillCommandBuffer (Context&						context,
												 TestParams&					testParams,
												 VkCommandBuffer				commandBuffer,
												 const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
												 const VkDescriptorImageInfo&	resultImageInfo)
{
	const InstanceInterface&			vki									= context.getInstanceInterface();
	const DeviceInterface&				vkd									= context.getDeviceInterface();
	const VkDevice						device								= context.getDevice();
	const VkPhysicalDevice				physicalDevice						= context.getPhysicalDevice();
	Allocator&							allocator							= context.getDefaultAllocator();

	{
		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
		bottomLevelAccelerationStructure->setGeometryCount(1);

		de::SharedPtr<RaytracedGeometryBase> geometry;
		if (testParams.shaderSourceType != SST_INTERSECTION_SHADER)
		{
			tcu::Vec3 v0(0.0f, float(testParams.height), 0.0f);
			tcu::Vec3 v1(0.0f, 0.0f, 0.0f);
			tcu::Vec3 v2(float(testParams.width), float(testParams.height), 0.0f);
			tcu::Vec3 v3(float(testParams.width), 0.0f, 0.0f);
			tcu::Vec3 missOffset(0.0f, 0.0f, 0.0f);
			if (testParams.shaderSourceType == SST_MISS_SHADER)
				missOffset = tcu::Vec3(1.0f + float(testParams.width), 0.0f, 0.0f);

			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
			geometry->addVertex(v0 + missOffset);
			geometry->addVertex(v1 + missOffset);
			geometry->addVertex(v2 + missOffset);
			geometry->addVertex(v2 + missOffset);
			geometry->addVertex(v1 + missOffset);
			geometry->addVertex(v3 + missOffset);
		}
		else // testParams.shaderSourceType == SST_INTERSECTION_SHADER
		{
			tcu::Vec3 v0(0.0f, 0.0f, -0.1f);
			tcu::Vec3 v1(float(testParams.width), float(testParams.height), 0.1f);

			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
			geometry->addVertex(v0);
			geometry->addVertex(v1);
		}
		bottomLevelAccelerationStructure->addGeometry(geometry);
		bottomLevelAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));

		for (auto& blas : bottomLevelAccelerationStructures)
			blas->createAndBuild(vkd, device, commandBuffer, allocator);
	}

	topLevelAccelerationStructure = makeTopLevelAccelerationStructure();
	topLevelAccelerationStructure->setInstanceCount(1);
	topLevelAccelerationStructure->addInstance(bottomLevelAccelerationStructures[0]);
	topLevelAccelerationStructure->createAndBuild(vkd, device, commandBuffer, allocator);

	const TopLevelAccelerationStructure*			topLevelAccelerationStructurePtr		= topLevelAccelerationStructure.get();
	VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		topLevelAccelerationStructurePtr->getPtr(),							//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &rayQueryAccelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

	vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *rtPipeline);

	deUint32							shaderGroupHandleSize				= getShaderGroupHandleSize(vki, physicalDevice);
	VkStridedDeviceAddressRegionKHR		raygenShaderBindingTableRegion		= raygenShaderBindingTable.get() != DE_NULL		? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize)		: makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR		hitShaderBindingTableRegion			= hitShaderBindingTable.get() != DE_NULL		? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize)			: makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR		missShaderBindingTableRegion		= missShaderBindingTable.get() != DE_NULL		? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize)		: makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR		callableShaderBindingTableRegion	= callableShaderBindingTable.get() != DE_NULL	? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize)	: makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	cmdTraceRays(vkd,
		commandBuffer,
		&raygenShaderBindingTableRegion,
		&missShaderBindingTableRegion,
		&hitShaderBindingTableRegion,
		&callableShaderBindingTableRegion,
		testParams.width, testParams.height, 1);
}

bool RayTracingConfiguration::verifyImage (BufferWithMemory*					resultBuffer,
										   Context&								context,
										   TestParams&							testParams)
{
	// create result image
	const bool					allMiss							= (testParams.emptyASCase != EmptyAccelerationStructureCase::NOT_EMPTY);
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 2, resultBuffer->getAllocation().getHostPtr());

	// create reference image
	std::vector<deUint32>		reference(testParams.width * testParams.height * 2);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 2, reference.data());

	tcu::UVec4					missValue	(0, 0, 0, 0);
	tcu::UVec4					hitValue	(1, 0, 0, 0);

	for (deUint32 y = 0; y < testParams.height; ++y)
	for (deUint32 x = 0; x < testParams.width; ++x)
	{
		if (allMiss || ((x + y) % 2) == 0)
		{
			referenceAccess.setPixel(missValue, x, y, 0);
			referenceAccess.setPixel(missValue, x, y, 1);
		}
		else
		{
			referenceAccess.setPixel(hitValue, x, y, 0);
			referenceAccess.setPixel(hitValue, x, y, 1);
		}
	}

	// compare result and reference
	return tcu::intThresholdCompare(context.getTestContext().getLog(), "Result comparison", "", referenceAccess, resultAccess, tcu::UVec4(0), tcu::COMPARE_LOG_RESULT);
}

VkFormat RayTracingConfiguration::getResultImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

size_t RayTracingConfiguration::getResultImageFormatSize ()
{
	return sizeof(deUint32);
}

VkClearValue RayTracingConfiguration::getClearValue ()
{
	return makeClearValueColorU32(0xFF, 0u, 0u, 0u);
}

de::SharedPtr<TestConfiguration> createTestConfiguration(const ShaderSourcePipeline& shaderSourcePipeline)
{
	switch (shaderSourcePipeline)
	{
	case SSP_GRAPHICS_PIPELINE:
		return de::SharedPtr<TestConfiguration>(new GraphicsConfiguration());
	case SSP_COMPUTE_PIPELINE:
		return de::SharedPtr<TestConfiguration>(new ComputeConfiguration());
	case SSP_RAY_TRACING_PIPELINE:
		return de::SharedPtr<TestConfiguration>(new RayTracingConfiguration());
	default:
		TCU_THROW(InternalError, "Wrong shader source pipeline");
	}
	return de::SharedPtr<TestConfiguration>();
}

class CheckerboardSceneBuilder : public SceneBuilder
{
public:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures (Context&							context,
																									  TestParams&						testParams) override;
	de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure	 (Context&							context,
																									  TestParams&						testParams,
																									  std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures) override;
};

std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> CheckerboardSceneBuilder::initBottomAccelerationStructures (Context&			context,
																														  TestParams&		testParams)
{
	DE_UNREF(context);

	// Cull flags can only be used with triangles.
	DE_ASSERT(testParams.cullFlags == InstanceCullFlags::NONE || testParams.bottomTestType == BTT_TRIANGLES);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	const auto instanceFlags = getCullFlags(testParams.cullFlags);

	tcu::Vec3 v0(0.0, 1.0, 0.0);
	tcu::Vec3 v1(0.0, 0.0, 0.0);
	tcu::Vec3 v2(1.0, 1.0, 0.0);
	tcu::Vec3 v3(1.0, 0.0, 0.0);

	if (testParams.emptyASCase == EmptyAccelerationStructureCase::INACTIVE_TRIANGLES)
	{
		const auto nanValue = tcu::Float32::nan().asFloat();
		v0.x() = nanValue;
		v1.x() = nanValue;
		v2.x() = nanValue;
		v3.x() = nanValue;
	}

	if (testParams.topTestType == TTT_DIFFERENT_INSTANCES)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
		bottomLevelAccelerationStructure->setGeometryCount(1u);
		de::SharedPtr<RaytracedGeometryBase> geometry;
		if (testParams.bottomTestType == BTT_TRIANGLES)
		{
			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, testParams.vertexFormat, testParams.indexType, testParams.padVertices);
			if (testParams.indexType == VK_INDEX_TYPE_NONE_KHR)
			{
				if (instanceFlags == 0u)
				{
					geometry->addVertex(v0);
					geometry->addVertex(v1);
					geometry->addVertex(v2);
					geometry->addVertex(v2);
					geometry->addVertex(v1);
					geometry->addVertex(v3);
				}
				else // Counterclockwise so the flags will be needed for the geometry to be visible.
				{
					geometry->addVertex(v2);
					geometry->addVertex(v1);
					geometry->addVertex(v0);
					geometry->addVertex(v3);
					geometry->addVertex(v1);
					geometry->addVertex(v2);
				}
			}
			else // m_data.indexType != VK_INDEX_TYPE_NONE_KHR
			{
				geometry->addVertex(v0);
				geometry->addVertex(v1);
				geometry->addVertex(v2);
				geometry->addVertex(v3);

				if (instanceFlags == 0u)
				{
					geometry->addIndex(0);
					geometry->addIndex(1);
					geometry->addIndex(2);
					geometry->addIndex(2);
					geometry->addIndex(1);
					geometry->addIndex(3);
				}
				else // Counterclockwise so the flags will be needed for the geometry to be visible.
				{
					geometry->addIndex(2);
					geometry->addIndex(1);
					geometry->addIndex(0);
					geometry->addIndex(3);
					geometry->addIndex(1);
					geometry->addIndex(2);
				}

			}
		}
		else // m_data.bottomTestType == BTT_AABBS
		{
			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, testParams.vertexFormat, testParams.indexType, testParams.padVertices);

			if (!testParams.padVertices)
			{
				// Single AABB.
				geometry->addVertex(tcu::Vec3(0.0f, 0.0f, -0.1f));
				geometry->addVertex(tcu::Vec3(1.0f, 1.0f, 0.1f));
			}
			else
			{
				// Multiple AABBs covering the same space.
				geometry->addVertex(tcu::Vec3(0.0f, 0.0f, -0.1f));
				geometry->addVertex(tcu::Vec3(0.5f, 0.5f,  0.1f));

				geometry->addVertex(tcu::Vec3(0.5f, 0.5f, -0.1f));
				geometry->addVertex(tcu::Vec3(1.0f, 1.0f,  0.1f));

				geometry->addVertex(tcu::Vec3(0.0f, 0.5f, -0.1f));
				geometry->addVertex(tcu::Vec3(0.5f, 1.0f,  0.1f));

				geometry->addVertex(tcu::Vec3(0.5f, 0.0f, -0.1f));
				geometry->addVertex(tcu::Vec3(1.0f, 0.5f,  0.1f));
			}
		}

		bottomLevelAccelerationStructure->addGeometry(geometry);
		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}
	else // m_data.topTestType == TTT_IDENTICAL_INSTANCES
	{
		tcu::TextureFormat	texFormat	= mapVkFormat(testParams.vertexFormat);
		tcu::Vec3			scale		( 1.0f, 1.0f, 1.0f );
		if (tcu::getTextureChannelClass(texFormat.type) == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT)
			scale = tcu::Vec3(1.0f / float(testParams.width), 1.0f / float(testParams.height), 1.0f);

		// triangle and aabb tests use geometries/aabbs with different vertex positions and the same identity matrix in each instance data
		for (deUint32 y = 0; y < testParams.height; ++y)
		for (deUint32 x = 0; x < testParams.width; ++x)
		{
			// let's build a chessboard of geometries
			if (((x + y) % 2) == 0)
				continue;
			tcu::Vec3 xyz((float)x, (float)y, 0.0f);

			de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
			bottomLevelAccelerationStructure->setGeometryCount(1u);

			de::SharedPtr<RaytracedGeometryBase> geometry;
			if (testParams.bottomTestType == BTT_TRIANGLES)
			{
				geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, testParams.vertexFormat, testParams.indexType, testParams.padVertices);
				if (testParams.indexType == VK_INDEX_TYPE_NONE_KHR)
				{
					if (instanceFlags == 0u)
					{
						geometry->addVertex(scale * (xyz + v0));
						geometry->addVertex(scale * (xyz + v1));
						geometry->addVertex(scale * (xyz + v2));
						geometry->addVertex(scale * (xyz + v2));
						geometry->addVertex(scale * (xyz + v1));
						geometry->addVertex(scale * (xyz + v3));
					}
					else // Counterclockwise so the flags will be needed for the geometry to be visible.
					{
						geometry->addVertex(scale * (xyz + v2));
						geometry->addVertex(scale * (xyz + v1));
						geometry->addVertex(scale * (xyz + v0));
						geometry->addVertex(scale * (xyz + v3));
						geometry->addVertex(scale * (xyz + v1));
						geometry->addVertex(scale * (xyz + v2));
					}
				}
				else
				{
					geometry->addVertex(scale * (xyz + v0));
					geometry->addVertex(scale * (xyz + v1));
					geometry->addVertex(scale * (xyz + v2));
					geometry->addVertex(scale * (xyz + v3));

					if (instanceFlags == 0u)
					{
						geometry->addIndex(0);
						geometry->addIndex(1);
						geometry->addIndex(2);
						geometry->addIndex(2);
						geometry->addIndex(1);
						geometry->addIndex(3);
					}
					else // Counterclockwise so the flags will be needed for the geometry to be visible.
					{
						geometry->addIndex(2);
						geometry->addIndex(1);
						geometry->addIndex(0);
						geometry->addIndex(3);
						geometry->addIndex(1);
						geometry->addIndex(2);
					}
				}
			}
			else // testParams.bottomTestType == BTT_AABBS
			{
				geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, testParams.vertexFormat, testParams.indexType, testParams.padVertices);

				if (!testParams.padVertices)
				{
					// Single AABB.
					geometry->addVertex(scale * (xyz + tcu::Vec3(0.0f, 0.0f, -0.1f)));
					geometry->addVertex(scale * (xyz + tcu::Vec3(1.0f, 1.0f, 0.1f)));
				}
				else
				{
					// Multiple AABBs covering the same space.
					geometry->addVertex(scale * (xyz + tcu::Vec3(0.0f, 0.0f, -0.1f)));
					geometry->addVertex(scale * (xyz + tcu::Vec3(0.5f, 0.5f,  0.1f)));

					geometry->addVertex(scale * (xyz + tcu::Vec3(0.5f, 0.5f, -0.1f)));
					geometry->addVertex(scale * (xyz + tcu::Vec3(1.0f, 1.0f,  0.1f)));

					geometry->addVertex(scale * (xyz + tcu::Vec3(0.0f, 0.5f, -0.1f)));
					geometry->addVertex(scale * (xyz + tcu::Vec3(0.5f, 1.0f,  0.1f)));

					geometry->addVertex(scale * (xyz + tcu::Vec3(0.5f, 0.0f, -0.1f)));
					geometry->addVertex(scale * (xyz + tcu::Vec3(1.0f, 0.5f,  0.1f)));
				}
			}

			bottomLevelAccelerationStructure->addGeometry(geometry);
			result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
		}
	}

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> CheckerboardSceneBuilder::initTopAccelerationStructure (Context&			context,
																								   TestParams&		testParams,
																								   std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
{
	DE_UNREF(context);

	const auto instanceCount = testParams.width * testParams.height / 2u;
	const auto instanceFlags = getCullFlags(testParams.cullFlags);

	de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();
	result->setInstanceCount(instanceCount);

	if (testParams.topTestType == TTT_DIFFERENT_INSTANCES)
	{

		for (deUint32 y = 0; y < testParams.height; ++y)
		for (deUint32 x = 0; x < testParams.width; ++x)
		{
			if (((x + y) % 2) == 0)
				continue;
			const VkTransformMatrixKHR			transformMatrixKHR =
			{
				{								//  float	matrix[3][4];
					{ 1.0f, 0.0f, 0.0f, (float)x },
					{ 0.0f, 1.0f, 0.0f, (float)y },
					{ 0.0f, 0.0f, 1.0f, 0.0f },
				}
			};
			result->addInstance(bottomLevelAccelerationStructures[0], transformMatrixKHR, 0u, 0xFFu, 0u, instanceFlags);
		}
	}
	else // testParams.topTestType == TTT_IDENTICAL_INSTANCES
	{
		tcu::TextureFormat	texFormat	= mapVkFormat(testParams.vertexFormat);
		tcu::Vec3			scale		( 1.0f, 1.0f, 1.0f );
		if (tcu::getTextureChannelClass(texFormat.type) == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT)
			scale = tcu::Vec3(float(testParams.width), float(testParams.height), 1.0f);

		const VkTransformMatrixKHR			transformMatrixKHR =
		{
			{								//  float	matrix[3][4];
				{ scale.x(), 0.0f, 0.0f, 0.0f },
				{ 0.0f, scale.y(), 0.0f, 0.0f },
				{ 0.0f, 0.0f, scale.z(), 0.0f },
			}
		};

		deUint32 currentInstanceIndex = 0;

		for (deUint32 y = 0; y < testParams.height; ++y)
		for (deUint32 x = 0; x < testParams.width; ++x)
		{
			if (((x + y) % 2) == 0)
				continue;
			result->addInstance(bottomLevelAccelerationStructures[currentInstanceIndex++], transformMatrixKHR, 0u, 0xFFu, 0u, instanceFlags);
		}
	}

	return result;
}

void commonASTestsCheckSupport(Context& context)
{
	context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_query");

	const VkPhysicalDeviceRayQueryFeaturesKHR&	rayQueryFeaturesKHR = context.getRayQueryFeatures();
	if (rayQueryFeaturesKHR.rayQuery == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
}

class RayQueryASBasicTestCase : public TestCase
{
public:
							RayQueryASBasicTestCase		(tcu::TestContext& context, const char* name, const char* desc, const TestParams& data);
							~RayQueryASBasicTestCase	(void);

	virtual void			checkSupport				(Context& context) const;
	virtual	void			initPrograms				(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;
protected:
	TestParams				m_data;
};

class RayQueryASFuncArgTestCase : public RayQueryASBasicTestCase
{
public:
							RayQueryASFuncArgTestCase		(tcu::TestContext& context, const char* name, const char* desc, const TestParams& data);
							~RayQueryASFuncArgTestCase		(void) {}

	virtual	void			initPrograms					(SourceCollections& programCollection) const;
};

class RayQueryASBasicTestInstance : public TestInstance
{
public:
									RayQueryASBasicTestInstance		(Context& context,
																	 const TestParams& data);
									~RayQueryASBasicTestInstance	(void);
	tcu::TestStatus					iterate							(void);
protected:
	bool							iterateNoWorkers				(void);
	bool							iterateWithWorkers				(void);
	de::MovePtr<BufferWithMemory>	runTest							(TestConfiguration* testConfiguration,
																	 SceneBuilder* sceneBuilder,
																	 const deUint32 workerThreadsCount);


private:
	TestParams														m_data;
};

RayQueryASBasicTestCase::RayQueryASBasicTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams& data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayQueryASBasicTestCase::~RayQueryASBasicTestCase (void)
{
}

void RayQueryASBasicTestCase::checkSupport (Context& context) const
{
	commonASTestsCheckSupport(context);

	const VkPhysicalDeviceFeatures2& features2 = context.getDeviceFeatures2();

	if ((m_data.shaderSourceType == SST_TESSELATION_CONTROL_SHADER ||
		 m_data.shaderSourceType == SST_TESSELATION_EVALUATION_SHADER) &&
		features2.features.tessellationShader == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFeatures2.tessellationShader");

	if (m_data.shaderSourceType == SST_GEOMETRY_SHADER &&
		features2.features.geometryShader == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFeatures2.geometryShader");

	if (m_data.shaderSourceType == SST_RAY_GENERATION_SHADER ||
		m_data.shaderSourceType == SST_INTERSECTION_SHADER ||
		m_data.shaderSourceType == SST_ANY_HIT_SHADER ||
		m_data.shaderSourceType == SST_CLOSEST_HIT_SHADER ||
		m_data.shaderSourceType == SST_MISS_SHADER ||
		m_data.shaderSourceType == SST_CALLABLE_SHADER)
	{
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

		const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();

		if(rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE )
			TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
	}

	switch (m_data.shaderSourceType)
	{
	case SST_VERTEX_SHADER:
	case SST_TESSELATION_CONTROL_SHADER:
	case SST_TESSELATION_EVALUATION_SHADER:
	case SST_GEOMETRY_SHADER:
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
		break;
	default:
		break;
	}

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR && accelerationStructureFeaturesKHR.accelerationStructureHostCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");

	// Check supported vertex format.
	checkAccelerationStructureVertexBufferFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_data.vertexFormat);
}

void RayQueryASBasicTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	// create parts of programs responsible for test execution
	std::vector<std::string> rayQueryTest;
	std::vector<std::string> rayQueryTestName;
	rayQueryTestName.push_back("as_triangle");
	rayQueryTestName.push_back("as_aabb");

	{
		std::stringstream css;
		css <<
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  rayQueryEXT rq;\n"
			"  rayQueryInitializeEXT(rq, rqTopLevelAS, " << ((m_data.cullFlags == InstanceCullFlags::NONE) ? "0" : "gl_RayFlagsCullBackFacingTrianglesEXT") << ", 0xFF, origin, tmin, direct, tmax);\n"
			"  if(rayQueryProceedEXT(rq))\n"
			"  {\n"
			"    if (rayQueryGetIntersectionTypeEXT(rq, false)==gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      hitValue.y = 1;\n"
			"      hitValue.x = 1;\n"
			"    }\n"
			"  }\n";
		rayQueryTest.push_back(css.str());
	}
	{
		std::stringstream css;
		css <<
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  rayQueryEXT rq;\n"
			"  rayQueryInitializeEXT(rq, rqTopLevelAS, 0, 0xFF, origin, tmin, direct, tmax);\n"
			"  if(rayQueryProceedEXT(rq))\n"
			"  {\n"
			"    if (rayQueryGetIntersectionTypeEXT(rq, false)==gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      hitValue.y = 1;\n"
			"      hitValue.x = 1;\n"
			"    }\n"
			"  }\n";
		rayQueryTest.push_back(css.str());
	}

	// create all programs
	if (m_data.shaderSourcePipeline == SSP_GRAPHICS_PIPELINE)
	{
		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"layout (location = 0) in vec3 position;\n"
				"out gl_PerVertex\n"
				"{\n"
				"  vec4 gl_Position;\n"
				"};\n"
				"void main()\n"
				"{\n"
				"  gl_Position = vec4(position, 1.0);\n"
				"}\n";
			programCollection.glslSources.add("vert") << glu::VertexSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"layout (location = 0) in vec3 position;\n"
				"out gl_PerVertex\n"
				"{\n"
				"  vec4 gl_Position;\n"
				"};\n"
				"layout(location = 0) out int vertexIndex;\n"
				"void main()\n"
				"{\n"
				"  gl_Position = vec4(position, 1.0);\n"
				"  vertexIndex = gl_VertexIndex;\n"
				"}\n";
			programCollection.glslSources.add("vert_vid") << glu::VertexSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout (location = 0) in vec3 position;\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3  origin   = vec3(float(position.x) + 0.5, float(position.y) + 0.5, 0.5);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"  imageStore(result, ivec3(gl_VertexIndex, 0, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_VertexIndex, 0, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"  gl_Position = vec4(position,1);\n"
				"}\n";
			std::stringstream cssName;
			cssName << "vert_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::VertexSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"in gl_PerVertex {\n"
				"  vec4  gl_Position;\n"
				"} gl_in[];\n"
				"layout(vertices = 3) out;\n"
				"void main (void)\n"
				"{\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"  gl_TessLevelInner[0] = 1;\n"
				"  gl_TessLevelOuter[0] = 1;\n"
				"  gl_TessLevelOuter[1] = 1;\n"
				"  gl_TessLevelOuter[2] = 1;\n"
				"}\n";
			programCollection.glslSources.add("tesc") << glu::TessellationControlSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"in gl_PerVertex {\n"
				"  vec4  gl_Position;\n"
				"} gl_in[];\n"
				"layout(vertices = 3) out;\n"
				"void main (void)\n"
				"{\n"
				"  vec3  origin   = vec3(gl_in[gl_InvocationID].gl_Position.x + 0.5, gl_in[gl_InvocationID].gl_Position.y + 0.5, 0.5);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"  imageStore(result, ivec3(gl_PrimitiveID, gl_InvocationID, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_PrimitiveID, gl_InvocationID, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"  gl_TessLevelInner[0] = 1;\n"
				"  gl_TessLevelOuter[0] = 1;\n"
				"  gl_TessLevelOuter[1] = 1;\n"
				"  gl_TessLevelOuter[2] = 1;\n"
				"}\n";
			std::stringstream cssName;
			cssName << "tesc_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::TessellationControlSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(triangles, equal_spacing, ccw) in;\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main (void)\n"
				"{\n"
				"  for (int i = 0; i < 3; ++i)\n"
				"  {\n"
				"    vec3  origin   = vec3(gl_in[i].gl_Position.x + 0.5, gl_in[i].gl_Position.y + 0.5, 0.5);\n"
				"    uvec4 hitValue = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"    imageStore(result, ivec3(gl_PrimitiveID, i, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"    imageStore(result, ivec3(gl_PrimitiveID, i, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"  }\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"}\n";
			std::stringstream cssName;
			cssName << "tese_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::TessellationEvaluationSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"layout(triangles, equal_spacing, ccw) in;\n"
				"void main (void)\n"
				"{\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"}\n";

			programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(triangles) in;\n"
				"layout (triangle_strip, max_vertices = 4) out;\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"\n"
				"in gl_PerVertex {\n"
				"  vec4  gl_Position;\n"
				"} gl_in[];\n"
				"layout(location = 0) in int vertexIndex[];\n"
				"out gl_PerVertex {\n"
				"  vec4 gl_Position;\n"
				"};\n"
				"void main (void)\n"
				"{\n"
				"  // geometry shader may reorder the vertices, keeping only the winding of the triangles.\n"
				"  // To iterate from the 'first vertex' of the triangle we need to find it first by looking for\n"
				"  // smallest vertex index value.\n"
				"  int minVertexIndex = 10000;"
				"  int firstVertex;"
				"  for (int i = 0; i < gl_in.length(); ++i)\n"
				"  {\n"
				"    if (minVertexIndex > vertexIndex[i])\n"
				"    {\n"
				"      minVertexIndex = vertexIndex[i];\n"
				"      firstVertex    = i;\n"
				"    }\n"
				"  }\n"
				"  for (int j = 0; j < gl_in.length(); ++j)\n"
				"  {\n"
				"    // iterate starting at firstVertex, possibly wrapping around, so the triangle is\n"
				"    // always iterated starting from the smallest vertex index, as found above.\n"
				"    int i = (firstVertex + j) % gl_in.length();\n"
				"    vec3  origin   = vec3(gl_in[i].gl_Position.x + 0.5, gl_in[i].gl_Position.y + 0.5, 0.5);\n"
				"    uvec4 hitValue = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"    imageStore(result, ivec3(gl_PrimitiveIDIn, j, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"    imageStore(result, ivec3(gl_PrimitiveIDIn, j, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"    gl_Position      = gl_in[i].gl_Position;\n"
				"    EmitVertex();\n"
				"  }\n"
				"  EndPrimitive();\n"
				"}\n";
			std::stringstream cssName;
			cssName << "geom_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::GeometrySource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3  origin   = vec3(gl_FragCoord.x, gl_FragCoord.y, 0.5);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"  imageStore(result, ivec3(gl_FragCoord.xy-vec2(0.5,0.5), 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_FragCoord.xy-vec2(0.5,0.5), 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"}\n";
			std::stringstream cssName;
			cssName << "frag_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::FragmentSource(css.str()) << buildOptions;
		}
	}
	else if (m_data.shaderSourcePipeline == SSP_COMPUTE_PIPELINE)
	{
		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3  origin   = vec3(float(gl_GlobalInvocationID.x) + 0.5, float(gl_GlobalInvocationID.y) + 0.5, 0.5);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"  imageStore(result, ivec3(gl_GlobalInvocationID.xy, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_GlobalInvocationID.xy, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"}\n";
			std::stringstream cssName;
			cssName << "comp_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::ComputeSource(css.str()) << buildOptions;
		}
	}
	else if (m_data.shaderSourcePipeline == SSP_RAY_TRACING_PIPELINE)
	{
		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) rayPayloadEXT uvec4 hitValue;\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
				"void main()\n"
				"{\n"
				"  float tmin     = 0.0;\n"
				"  float tmax     = 1.0;\n"
				"  vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5, float(gl_LaunchIDEXT.y) + 0.5, 0.5);\n"
				"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
				"  hitValue       = uvec4(0,0,0,0);\n"
				"  traceRayEXT(topLevelAS, 0, 0xFF, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"}\n";
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
				"layout(set = 0, binding = 2) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3  origin    = vec3(float(gl_LaunchIDEXT.x) + 0.5, float(gl_LaunchIDEXT.y) + 0.5, 0.5);\n"
				"  uvec4  hitValue = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"}\n";
			std::stringstream cssName;
			cssName << "rgen_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"struct CallValue\n{\n"
				"  vec3  origin;\n"
				"  uvec4 hitValue;\n"
				"};\n"
				"layout(location = 0) callableDataEXT CallValue param;\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
				"void main()\n"
				"{\n"
				"  param.origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5, float(gl_LaunchIDEXT.y) + 0.5, 0.5);\n"
				"  param.hitValue = uvec4(0, 0, 0, 0);\n"
				"  executeCallableEXT(0, 0);\n"
				"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 0), uvec4(param.hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 1), uvec4(param.hitValue.y, 0, 0, 0));\n"
				"}\n";
			programCollection.glslSources.add("rgen_call") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"hitAttributeEXT uvec4 hitValue;\n"
				"void main()\n"
				"{\n"
				"  reportIntersectionEXT(0.5f, 0);\n"
				"}\n";

			programCollection.glslSources.add("isect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"hitAttributeEXT uvec4 hitValue;\n"
				"layout(set = 0, binding = 2) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3 origin = gl_WorldRayOriginEXT;\n"
				"  hitValue    = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"  reportIntersectionEXT(0.5f, 0);\n"
				"}\n";
			std::stringstream cssName;
			cssName << "isect_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
				"layout(set = 0, binding = 2) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3 origin = gl_WorldRayOriginEXT;\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"}\n";
			std::stringstream cssName;
			cssName << "ahit_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
				"void main()\n"
				"{\n"
				"  hitValue.y = 3;\n"
				"}\n";

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
				"layout(set = 0, binding = 2) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3 origin = gl_WorldRayOriginEXT;\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"}\n";
			std::stringstream cssName;
			cssName << "chit_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
				"hitAttributeEXT uvec4 hitAttrib;\n"
				"void main()\n"
				"{\n"
				"  hitValue = hitAttrib;\n"
				"}\n";

			programCollection.glslSources.add("chit_isect") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
				"void main()\n"
				"{\n"
				"  hitValue.x = 4;\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
				"layout(set = 0, binding = 2) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3 origin = gl_WorldRayOriginEXT;\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"}\n";
			std::stringstream cssName;
			cssName << "miss_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"struct CallValue\n{\n"
				"  vec3  origin;\n"
				"  uvec4 hitValue;\n"
				"};\n"
				"layout(location = 0) callableDataInEXT CallValue result;\n"
				"layout(set = 0, binding = 2) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3 origin    = result.origin;\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n" <<
				rayQueryTest[m_data.bottomTestType] <<
				"  result.hitValue = hitValue;\n"
				"}\n";
			std::stringstream cssName;
			cssName << "call_" << rayQueryTestName[m_data.bottomTestType];

			programCollection.glslSources.add(cssName.str()) << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}
	}
}

TestInstance* RayQueryASBasicTestCase::createInstance (Context& context) const
{
	return new RayQueryASBasicTestInstance(context, m_data);
}

RayQueryASFuncArgTestCase::RayQueryASFuncArgTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams& data)
	: RayQueryASBasicTestCase (context, name, desc, data)
{
}

void RayQueryASFuncArgTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::SpirVAsmBuildOptions	spvBuildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);

	DE_ASSERT(m_data.shaderSourcePipeline == SSP_COMPUTE_PIPELINE);
	DE_ASSERT(m_data.bottomTestType == BTT_TRIANGLES);

	// The SPIR-V assembly shader below is based on the following GLSL code.
	// In it, rayQueryInitializeBottomWrapper has been modified to take a
	// bare AS as the second argument, instead of a pointer.
	//
	//	#version 460 core
	//	#extension GL_EXT_ray_query : require
	//	layout(r32ui, set = 0, binding = 0) uniform uimage3D result;
	//	layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;
	//
	//	void rayQueryInitializeBottomWrapper(rayQueryEXT rayQuery,
	//	       accelerationStructureEXT topLevel,
	//	       uint rayFlags, uint cullMask, vec3 origin,
	//	       float tMin, vec3 direction, float tMax)
	//	{
	//	  rayQueryInitializeEXT(rayQuery, topLevel, rayFlags, cullMask, origin, tMin, direction, tMax);
	//	}
	//
	//	void rayQueryInitializeTopWrapper(rayQueryEXT rayQuery,
	//	       accelerationStructureEXT topLevel,
	//	       uint rayFlags, uint cullMask, vec3 origin,
	//	       float tMin, vec3 direction, float tMax)
	//	{
	//	  rayQueryInitializeBottomWrapper(rayQuery, topLevel, rayFlags, cullMask, origin, tMin, direction, tMax);
	//	}
	//
	//	void main()
	//	{
	//	  vec3  origin   = vec3(float(gl_GlobalInvocationID.x) + 0.5, float(gl_GlobalInvocationID.y) + 0.5, 0.5);
	//	  uvec4 hitValue = uvec4(0,0,0,0);
	//	  float tmin     = 0.0;
	//	  float tmax     = 1.0;
	//	  vec3  direct   = vec3(0.0, 0.0, -1.0);
	//	  rayQueryEXT rq;
	//	  rayQueryInitializeTopWrapper(rq, rqTopLevelAS, 0, 0xFF, origin, tmin, direct, tmax);
	//	  if(rayQueryProceedEXT(rq))
	//	  {
	//	    if (rayQueryGetIntersectionTypeEXT(rq, false)==gl_RayQueryCandidateIntersectionTriangleEXT)
	//	    {
	//	      hitValue.y = 1;
	//	      hitValue.x = 1;
	//	    }
	//	  }
	//	  imageStore(result, ivec3(gl_GlobalInvocationID.xy, 0), uvec4(hitValue.x, 0, 0, 0));
	//	  imageStore(result, ivec3(gl_GlobalInvocationID.xy, 1), uvec4(hitValue.y, 0, 0, 0));
	//	}

	std::stringstream css;
	css
		<< "; SPIR-V\n"
		<< "; Version: 1.4\n"
		<< "; Generator: Khronos Glslang Reference Front End; 10\n"
		<< "; Bound: 139\n"
		<< "; Schema: 0\n"
		<< "OpCapability Shader\n"
		<< "OpCapability RayQueryKHR\n"
		<< "OpExtension \"SPV_KHR_ray_query\"\n"
		<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
		<< "OpMemoryModel Logical GLSL450\n"
		<< "OpEntryPoint GLCompute %4 \"main\" %60 %86 %114\n"
		<< "OpExecutionMode %4 LocalSize 1 1 1\n"
		<< "OpDecorate %60 BuiltIn GlobalInvocationId\n"
		<< "OpDecorate %86 DescriptorSet 0\n"
		<< "OpDecorate %86 Binding 1\n"
		<< "OpDecorate %114 DescriptorSet 0\n"
		<< "OpDecorate %114 Binding 0\n"
		<< "%2 = OpTypeVoid\n"
		<< "%3 = OpTypeFunction %2\n"

		// Bare query type
		<< "%6 = OpTypeRayQueryKHR\n"

		// Pointer to query.
		<< "%7 = OpTypePointer Function %6\n"

		// Bare AS type.
		<< "%8 = OpTypeAccelerationStructureKHR\n"

		// Pointer to AS.
		<< "%9 = OpTypePointer UniformConstant %8\n"

		<< "%10 = OpTypeInt 32 0\n"
		<< "%11 = OpTypePointer Function %10\n"
		<< "%12 = OpTypeFloat 32\n"
		<< "%13 = OpTypeVector %12 3\n"
		<< "%14 = OpTypePointer Function %13\n"
		<< "%15 = OpTypePointer Function %12\n"

		// This is the function type for rayQueryInitializeTopWrapper and the old rayQueryInitializeBottomWrapper.
		<< "%16 = OpTypeFunction %2 %7 %9 %11 %11 %14 %15 %14 %15\n"

		// This is the new function type for the modified rayQueryInitializeBottomWrapper that uses a bare AS.
		//<< "%16b = OpTypeFunction %2 %6 %8 %11 %11 %14 %15 %14 %15\n"
		<< "%16b = OpTypeFunction %2 %7 %8 %11 %11 %14 %15 %14 %15\n"

		<< "%58 = OpTypeVector %10 3\n"
		<< "%59 = OpTypePointer Input %58\n"
		<< "%60 = OpVariable %59 Input\n"
		<< "%61 = OpConstant %10 0\n"
		<< "%62 = OpTypePointer Input %10\n"
		<< "%66 = OpConstant %12 0.5\n"
		<< "%68 = OpConstant %10 1\n"
		<< "%74 = OpTypeVector %10 4\n"
		<< "%75 = OpTypePointer Function %74\n"
		<< "%77 = OpConstantComposite %74 %61 %61 %61 %61\n"
		<< "%79 = OpConstant %12 0\n"
		<< "%81 = OpConstant %12 1\n"
		<< "%83 = OpConstant %12 -1\n"
		<< "%84 = OpConstantComposite %13 %79 %79 %83\n"
		<< "%86 = OpVariable %9 UniformConstant\n"
		<< "%87 = OpConstant %10 255\n"
		<< "%99 = OpTypeBool\n"
		<< "%103 = OpConstantFalse %99\n"
		<< "%104 = OpTypeInt 32 1\n"
		<< "%105 = OpConstant %104 0\n"
		<< "%112 = OpTypeImage %10 3D 0 0 0 2 R32ui\n"
		<< "%113 = OpTypePointer UniformConstant %112\n"
		<< "%114 = OpVariable %113 UniformConstant\n"
		<< "%116 = OpTypeVector %10 2\n"
		<< "%119 = OpTypeVector %104 2\n"
		<< "%121 = OpTypeVector %104 3\n"
		<< "%132 = OpConstant %104 1\n"

		// This is main().
		<< "%4 = OpFunction %2 None %3\n"
		<< "%5 = OpLabel\n"
		<< "%57 = OpVariable %14 Function\n"
		<< "%76 = OpVariable %75 Function\n"
		<< "%78 = OpVariable %15 Function\n"
		<< "%80 = OpVariable %15 Function\n"
		<< "%82 = OpVariable %14 Function\n"
		<< "%85 = OpVariable %7 Function\n"
		<< "%88 = OpVariable %11 Function\n"
		<< "%89 = OpVariable %11 Function\n"
		<< "%90 = OpVariable %14 Function\n"
		<< "%92 = OpVariable %15 Function\n"
		<< "%94 = OpVariable %14 Function\n"
		<< "%96 = OpVariable %15 Function\n"
		<< "%63 = OpAccessChain %62 %60 %61\n"
		<< "%64 = OpLoad %10 %63\n"
		<< "%65 = OpConvertUToF %12 %64\n"
		<< "%67 = OpFAdd %12 %65 %66\n"
		<< "%69 = OpAccessChain %62 %60 %68\n"
		<< "%70 = OpLoad %10 %69\n"
		<< "%71 = OpConvertUToF %12 %70\n"
		<< "%72 = OpFAdd %12 %71 %66\n"
		<< "%73 = OpCompositeConstruct %13 %67 %72 %66\n"
		<< "OpStore %57 %73\n"
		<< "OpStore %76 %77\n"
		<< "OpStore %78 %79\n"
		<< "OpStore %80 %81\n"
		<< "OpStore %82 %84\n"
		<< "OpStore %88 %61\n"
		<< "OpStore %89 %87\n"
		<< "%91 = OpLoad %13 %57\n"
		<< "OpStore %90 %91\n"
		<< "%93 = OpLoad %12 %78\n"
		<< "OpStore %92 %93\n"
		<< "%95 = OpLoad %13 %82\n"
		<< "OpStore %94 %95\n"
		<< "%97 = OpLoad %12 %80\n"
		<< "OpStore %96 %97\n"
		<< "%98 = OpFunctionCall %2 %35 %85 %86 %88 %89 %90 %92 %94 %96\n"
		<< "%100 = OpRayQueryProceedKHR %99 %85\n"
		<< "OpSelectionMerge %102 None\n"
		<< "OpBranchConditional %100 %101 %102\n"
		<< "%101 = OpLabel\n"
		<< "%106 = OpRayQueryGetIntersectionTypeKHR %10 %85 %105\n"
		<< "%107 = OpIEqual %99 %106 %61\n"
		<< "OpSelectionMerge %109 None\n"
		<< "OpBranchConditional %107 %108 %109\n"
		<< "%108 = OpLabel\n"
		<< "%110 = OpAccessChain %11 %76 %68\n"
		<< "OpStore %110 %68\n"
		<< "%111 = OpAccessChain %11 %76 %61\n"
		<< "OpStore %111 %68\n"
		<< "OpBranch %109\n"
		<< "%109 = OpLabel\n"
		<< "OpBranch %102\n"
		<< "%102 = OpLabel\n"
		<< "%115 = OpLoad %112 %114\n"
		<< "%117 = OpLoad %58 %60\n"
		<< "%118 = OpVectorShuffle %116 %117 %117 0 1\n"
		<< "%120 = OpBitcast %119 %118\n"
		<< "%122 = OpCompositeExtract %104 %120 0\n"
		<< "%123 = OpCompositeExtract %104 %120 1\n"
		<< "%124 = OpCompositeConstruct %121 %122 %123 %105\n"
		<< "%125 = OpAccessChain %11 %76 %61\n"
		<< "%126 = OpLoad %10 %125\n"
		<< "%127 = OpCompositeConstruct %74 %126 %61 %61 %61\n"
		<< "OpImageWrite %115 %124 %127 ZeroExtend\n"
		<< "%128 = OpLoad %112 %114\n"
		<< "%129 = OpLoad %58 %60\n"
		<< "%130 = OpVectorShuffle %116 %129 %129 0 1\n"
		<< "%131 = OpBitcast %119 %130\n"
		<< "%133 = OpCompositeExtract %104 %131 0\n"
		<< "%134 = OpCompositeExtract %104 %131 1\n"
		<< "%135 = OpCompositeConstruct %121 %133 %134 %132\n"
		<< "%136 = OpAccessChain %11 %76 %68\n"
		<< "%137 = OpLoad %10 %136\n"
		<< "%138 = OpCompositeConstruct %74 %137 %61 %61 %61\n"
		<< "OpImageWrite %128 %135 %138 ZeroExtend\n"
		<< "OpReturn\n"
		<< "OpFunctionEnd\n"

		// This is rayQueryInitializeBottomWrapper, calling OpRayQueryInitializeKHR.
		// We have modified the function type so it takes bare arguments.
		//%25 = OpFunction %2 None %16
		<< "%25 = OpFunction %2 None %16b\n"

		// These is the modified parameter.
		<< "%17 = OpFunctionParameter %7\n"
		//<< "%17 = OpFunctionParameter %6\n"
		//%18 = OpFunctionParameter %9
		<< "%18 = OpFunctionParameter %8\n"

		<< "%19 = OpFunctionParameter %11\n"
		<< "%20 = OpFunctionParameter %11\n"
		<< "%21 = OpFunctionParameter %14\n"
		<< "%22 = OpFunctionParameter %15\n"
		<< "%23 = OpFunctionParameter %14\n"
		<< "%24 = OpFunctionParameter %15\n"
		<< "%26 = OpLabel\n"

		// We no longer need to load this parameter.
		//%37 = OpLoad %8 %18

		<< "%38 = OpLoad %10 %19\n"
		<< "%39 = OpLoad %10 %20\n"
		<< "%40 = OpLoad %13 %21\n"
		<< "%41 = OpLoad %12 %22\n"
		<< "%42 = OpLoad %13 %23\n"
		<< "%43 = OpLoad %12 %24\n"

		// We call OpRayQueryInitializeKHR with bare arguments.
		// Note: some experimental lines to pass a bare rayQuery as the first argument have been commented out.
		//OpRayQueryInitializeKHR %17 %37 %38 %39 %40 %41 %42 %43
		<< "OpRayQueryInitializeKHR %17 %18 %38 %39 %40 %41 %42 %43\n"

		<< "OpReturn\n"
		<< "OpFunctionEnd\n"

		// This is rayQueryInitializeTopWrapper, calling rayQueryInitializeBottomWrapper.
		<< "%35 = OpFunction %2 None %16\n"
		<< "%27 = OpFunctionParameter %7\n"
		<< "%28 = OpFunctionParameter %9\n"
		<< "%29 = OpFunctionParameter %11\n"
		<< "%30 = OpFunctionParameter %11\n"
		<< "%31 = OpFunctionParameter %14\n"
		<< "%32 = OpFunctionParameter %15\n"
		<< "%33 = OpFunctionParameter %14\n"
		<< "%34 = OpFunctionParameter %15\n"
		<< "%36 = OpLabel\n"
		<< "%44 = OpVariable %11 Function\n"
		<< "%46 = OpVariable %11 Function\n"
		<< "%48 = OpVariable %14 Function\n"
		<< "%50 = OpVariable %15 Function\n"
		<< "%52 = OpVariable %14 Function\n"
		<< "%54 = OpVariable %15 Function\n"

		// We need to load the second argument.
		//<< "%27b = OpLoad %6 %27\n"
		<< "%28b = OpLoad %8 %28\n"

		<< "%45 = OpLoad %10 %29\n"
		<< "OpStore %44 %45\n"
		<< "%47 = OpLoad %10 %30\n"
		<< "OpStore %46 %47\n"
		<< "%49 = OpLoad %13 %31\n"
		<< "OpStore %48 %49\n"
		<< "%51 = OpLoad %12 %32\n"
		<< "OpStore %50 %51\n"
		<< "%53 = OpLoad %13 %33\n"
		<< "OpStore %52 %53\n"
		<< "%55 = OpLoad %12 %34\n"
		<< "OpStore %54 %55\n"

		// We call rayQueryInitializeBottomWrapper with the loaded argument.
		//%56 = OpFunctionCall %2 %25 %27 %28 %44 %46 %48 %50 %52 %54
		//<< "%56 = OpFunctionCall %2 %25 %27b %28b %44 %46 %48 %50 %52 %54\n"
		<< "%56 = OpFunctionCall %2 %25 %27 %28b %44 %46 %48 %50 %52 %54\n"

		<< "OpReturn\n"
		<< "OpFunctionEnd\n"
		;

	programCollection.spirvAsmSources.add("comp_as_triangle") << spvBuildOptions << css.str();
}

RayQueryASBasicTestInstance::RayQueryASBasicTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

RayQueryASBasicTestInstance::~RayQueryASBasicTestInstance (void)
{
}

de::MovePtr<BufferWithMemory> RayQueryASBasicTestInstance::runTest (TestConfiguration* testConfiguration,
																	SceneBuilder* sceneBuilder,
																	const deUint32 workerThreadsCount)
{
	testConfiguration->initConfiguration(m_context, m_data);

	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();

	const bool							htCopy								= (workerThreadsCount != 0) && (m_data.operationType == OP_COPY);
	const bool							htSerialize							= (workerThreadsCount != 0) && (m_data.operationType == OP_SERIALIZE);


	const VkFormat						imageFormat							= testConfiguration->getResultImageFormat();
	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, 2, imageFormat);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, imageFormat, imageSubresourceRange);

	const VkBufferCreateInfo			resultBufferCreateInfo				= makeBufferCreateInfo(m_data.width * m_data.height * 2 * testConfiguration->getResultImageFormatSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				resultBufferImageRegion				= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 2), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		resultBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			resultImageInfo						= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructure;
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	bottomLevelAccelerationStructureCopies;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructureCopy;
	std::vector<de::SharedPtr<SerialStorage>>						bottomSerialized;
	std::vector<de::SharedPtr<SerialStorage>>						topSerialized;
	std::vector<VkDeviceSize>			accelerationCompactedSizes;
	std::vector<VkDeviceSize>			accelerationSerialSizes;
	Move<VkQueryPool>					m_queryPoolCompact;
	Move<VkQueryPool>					m_queryPoolSerial;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		const VkImageMemoryBarrier			preImageBarrier					= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																					**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);

		const VkClearValue					clearValue						= testConfiguration->getClearValue();
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);

		const VkImageMemoryBarrier			postImageBarrier				= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
																				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																				**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		// build bottom level acceleration structures and their copies ( only when we are testing copying bottom level acceleration structures )
		bool									bottomCompact		= m_data.operationType == OP_COMPACT && m_data.operationTarget == OT_BOTTOM_ACCELERATION;
		bool									bottomSerial		= m_data.operationType == OP_SERIALIZE && m_data.operationTarget == OT_BOTTOM_ACCELERATION;
		const bool								buildWithoutGeom	= (m_data.emptyASCase == EmptyAccelerationStructureCase::NO_GEOMETRIES_BOTTOM);
		const bool								bottomNoPrimitives	= (m_data.emptyASCase == EmptyAccelerationStructureCase::NO_PRIMITIVES_BOTTOM);
		const bool								topNoPrimitives		= (m_data.emptyASCase == EmptyAccelerationStructureCase::NO_PRIMITIVES_TOP);
		const bool								inactiveInstances	= (m_data.emptyASCase == EmptyAccelerationStructureCase::INACTIVE_INSTANCES);
		bottomLevelAccelerationStructures							= sceneBuilder->initBottomAccelerationStructures(m_context, m_data);
		VkBuildAccelerationStructureFlagsKHR	allowCompactionFlag	= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
		VkBuildAccelerationStructureFlagsKHR	emptyCompactionFlag	= VkBuildAccelerationStructureFlagsKHR(0);
		VkBuildAccelerationStructureFlagsKHR	bottomCompactFlags	= (bottomCompact ? allowCompactionFlag : emptyCompactionFlag);
		VkBuildAccelerationStructureFlagsKHR	bottomBuildFlags	= m_data.buildFlags | bottomCompactFlags;
		std::vector<VkAccelerationStructureKHR>	accelerationStructureHandles;
		std::vector<VkDeviceSize>				bottomBlasCompactSize;
		std::vector<VkDeviceSize>				bottomBlasSerialSize;

		for (auto& blas : bottomLevelAccelerationStructures)
		{
			blas->setBuildType						(m_data.buildType);
			blas->setBuildFlags						(bottomBuildFlags);
			blas->setUseArrayOfPointers				(m_data.bottomUsesAOP);
			blas->setCreateGeneric					(m_data.bottomGeneric);
			blas->setBuildWithoutGeometries			(buildWithoutGeom);
			blas->setBuildWithoutPrimitives			(bottomNoPrimitives);
			blas->createAndBuild					(vkd, device, *cmdBuffer, allocator);
			accelerationStructureHandles.push_back	(*(blas->getPtr()));
		}

		if (m_data.operationType == OP_COMPACT)
		{
			deUint32 queryCount	= (m_data.operationTarget == OT_BOTTOM_ACCELERATION) ? deUint32(bottomLevelAccelerationStructures.size()) : 1u;
			if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
				m_queryPoolCompact = makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryCount);
			if (m_data.operationTarget == OT_BOTTOM_ACCELERATION)
				queryAccelerationStructureSize(vkd, device, *cmdBuffer, accelerationStructureHandles, m_data.buildType, m_queryPoolCompact.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 0u, bottomBlasCompactSize);
		}
		if (m_data.operationType == OP_SERIALIZE)
		{
			deUint32 queryCount	= (m_data.operationTarget == OT_BOTTOM_ACCELERATION) ? deUint32(bottomLevelAccelerationStructures.size()) : 1u;
			if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
				m_queryPoolSerial = makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, queryCount);
			if (m_data.operationTarget == OT_BOTTOM_ACCELERATION)
				queryAccelerationStructureSize(vkd, device, *cmdBuffer, accelerationStructureHandles, m_data.buildType, m_queryPoolSerial.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u, bottomBlasSerialSize);
		}

		// if AS is built on GPU and we are planning to make a compact copy of it or serialize / deserialize it - we have to have download query results to CPU
		if ((m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) && (bottomCompact || bottomSerial))
		{
			endCommandBuffer(vkd, *cmdBuffer);

			submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

			if (bottomCompact)
				VK_CHECK(vkd.getQueryPoolResults(device, *m_queryPoolCompact, 0u, deUint32(bottomBlasCompactSize.size()), sizeof(VkDeviceSize) * bottomBlasCompactSize.size(), bottomBlasCompactSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
			if (bottomSerial)
				VK_CHECK(vkd.getQueryPoolResults(device, *m_queryPoolSerial, 0u, deUint32(bottomBlasSerialSize.size()), sizeof(VkDeviceSize) * bottomBlasSerialSize.size(), bottomBlasSerialSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

			vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
			beginCommandBuffer(vkd, *cmdBuffer, 0u);
		}

		auto bottomLevelAccelerationStructuresPtr								= &bottomLevelAccelerationStructures;
		if (m_data.operationType != OP_NONE && m_data.operationTarget == OT_BOTTOM_ACCELERATION)
		{
			switch (m_data.operationType)
			{
			case OP_COPY:
			{
				for (size_t i = 0; i < bottomLevelAccelerationStructures.size(); ++i)
				{
					de::MovePtr<BottomLevelAccelerationStructure> asCopy = makeBottomLevelAccelerationStructure();
					asCopy->setDeferredOperation(htCopy, workerThreadsCount);
					asCopy->setBuildType(m_data.buildType);
					asCopy->setBuildFlags(m_data.buildFlags);
					asCopy->setUseArrayOfPointers(m_data.bottomUsesAOP);
					asCopy->setCreateGeneric(m_data.bottomGeneric);
					asCopy->setBuildWithoutGeometries(buildWithoutGeom);
					asCopy->setBuildWithoutPrimitives(bottomNoPrimitives);
					asCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, bottomLevelAccelerationStructures[i].get(), 0u, 0u);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
				}
				break;
			}
			case OP_COMPACT:
			{
				for (size_t i = 0; i < bottomLevelAccelerationStructures.size(); ++i)
				{
					de::MovePtr<BottomLevelAccelerationStructure> asCopy = makeBottomLevelAccelerationStructure();
					asCopy->setBuildType(m_data.buildType);
					asCopy->setBuildFlags(m_data.buildFlags);
					asCopy->setUseArrayOfPointers(m_data.bottomUsesAOP);
					asCopy->setCreateGeneric(m_data.bottomGeneric);
					asCopy->setBuildWithoutGeometries(buildWithoutGeom);
					asCopy->setBuildWithoutPrimitives(bottomNoPrimitives);
					asCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, bottomLevelAccelerationStructures[i].get(), bottomBlasCompactSize[i], 0u);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
				}
				break;
			}
			case OP_SERIALIZE:
			{
				for (size_t i = 0; i < bottomLevelAccelerationStructures.size(); ++i)
				{
					de::SharedPtr<SerialStorage> storage(new SerialStorage(vkd, device, allocator, m_data.buildType, bottomBlasSerialSize[i]));

					bottomLevelAccelerationStructures[i]->setDeferredOperation(htSerialize, workerThreadsCount);
					bottomLevelAccelerationStructures[i]->serialize(vkd, device, *cmdBuffer, storage.get());
					bottomSerialized.push_back(storage);

					if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
					{
						endCommandBuffer(vkd, *cmdBuffer);

						submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

						vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
						beginCommandBuffer(vkd, *cmdBuffer, 0u);
					}

					de::MovePtr<BottomLevelAccelerationStructure> asCopy = makeBottomLevelAccelerationStructure();
					asCopy->setBuildType(m_data.buildType);
					asCopy->setBuildFlags(m_data.buildFlags);
					asCopy->setUseArrayOfPointers(m_data.bottomUsesAOP);
					asCopy->setCreateGeneric(m_data.bottomGeneric);
					asCopy->setBuildWithoutGeometries(buildWithoutGeom);
					asCopy->setBuildWithoutPrimitives(bottomNoPrimitives);
					asCopy->setDeferredOperation(htSerialize, workerThreadsCount);
					asCopy->createAndDeserializeFrom(vkd, device, *cmdBuffer, allocator, storage.get(), 0u);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
				}
				break;
			}
			default:
				DE_ASSERT(DE_FALSE);
			}
			bottomLevelAccelerationStructuresPtr = &bottomLevelAccelerationStructureCopies;
		}

		// build top level acceleration structures and their copies ( only when we are testing copying top level acceleration structures )
		bool									topCompact			= m_data.operationType == OP_COMPACT && m_data.operationTarget == OT_TOP_ACCELERATION;
		bool									topSerial			= m_data.operationType == OP_SERIALIZE && m_data.operationTarget == OT_TOP_ACCELERATION;
		VkBuildAccelerationStructureFlagsKHR	topCompactFlags		= (topCompact ? allowCompactionFlag : emptyCompactionFlag);
		VkBuildAccelerationStructureFlagsKHR	topBuildFlags		= m_data.buildFlags | topCompactFlags;
		std::vector<VkAccelerationStructureKHR> topLevelStructureHandles;
		std::vector<VkDeviceSize>				topBlasCompactSize;
		std::vector<VkDeviceSize>				topBlasSerialSize;

		topLevelAccelerationStructure								= sceneBuilder->initTopAccelerationStructure(m_context, m_data, *bottomLevelAccelerationStructuresPtr);
		topLevelAccelerationStructure->setBuildType					(m_data.buildType);
		topLevelAccelerationStructure->setBuildFlags				(topBuildFlags);
		topLevelAccelerationStructure->setBuildWithoutPrimitives	(topNoPrimitives);
		topLevelAccelerationStructure->setUseArrayOfPointers		(m_data.topUsesAOP);
		topLevelAccelerationStructure->setCreateGeneric				(m_data.topGeneric);
		topLevelAccelerationStructure->setInactiveInstances			(inactiveInstances);
		topLevelAccelerationStructure->createAndBuild				(vkd, device, *cmdBuffer, allocator);
		topLevelStructureHandles.push_back							(*(topLevelAccelerationStructure->getPtr()));

		if (topCompact)
			queryAccelerationStructureSize(vkd, device, *cmdBuffer, topLevelStructureHandles, m_data.buildType, m_queryPoolCompact.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 0u, topBlasCompactSize);
		if (topSerial)
			queryAccelerationStructureSize(vkd, device, *cmdBuffer, topLevelStructureHandles, m_data.buildType, m_queryPoolSerial.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u, topBlasSerialSize);

		// if AS is built on GPU and we are planning to make a compact copy of it or serialize / deserialize it - we have to have download query results to CPU
		if ((m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) && (topCompact || topSerial))
		{
			endCommandBuffer(vkd, *cmdBuffer);

			submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

			if (topCompact)
				VK_CHECK(vkd.getQueryPoolResults(device, *m_queryPoolCompact, 0u, deUint32(topBlasCompactSize.size()), sizeof(VkDeviceSize) * topBlasCompactSize.size(), topBlasCompactSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
			if (topSerial)
				VK_CHECK(vkd.getQueryPoolResults(device, *m_queryPoolSerial, 0u, deUint32(topBlasSerialSize.size()), sizeof(VkDeviceSize) * topBlasSerialSize.size(), topBlasSerialSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

			vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
			beginCommandBuffer(vkd, *cmdBuffer, 0u);
		}

		const TopLevelAccelerationStructure*			topLevelRayTracedPtr	= topLevelAccelerationStructure.get();
		if (m_data.operationType != OP_NONE && m_data.operationTarget == OT_TOP_ACCELERATION)
		{
			switch (m_data.operationType)
			{
				case OP_COPY:
				{
					topLevelAccelerationStructureCopy = makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setDeferredOperation(htCopy, workerThreadsCount);
					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setBuildWithoutPrimitives(topNoPrimitives);
					topLevelAccelerationStructureCopy->setInactiveInstances(inactiveInstances);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->setCreateGeneric(m_data.topGeneric);
					topLevelAccelerationStructureCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, topLevelAccelerationStructure.get(), 0u, 0u);
					break;
				}
				case OP_COMPACT:
				{
					topLevelAccelerationStructureCopy = makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setBuildWithoutPrimitives(topNoPrimitives);
					topLevelAccelerationStructureCopy->setInactiveInstances(inactiveInstances);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->setCreateGeneric(m_data.topGeneric);
					topLevelAccelerationStructureCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, topLevelAccelerationStructure.get(), topBlasCompactSize[0], 0u);
					break;
				}
				case OP_SERIALIZE:
				{
					de::SharedPtr<SerialStorage> storage(new SerialStorage(vkd, device, allocator, m_data.buildType, topBlasSerialSize[0]));

					topLevelAccelerationStructure->setDeferredOperation(htSerialize, workerThreadsCount);
					topLevelAccelerationStructure->serialize(vkd, device, *cmdBuffer, storage.get());
					topSerialized.push_back(storage);

					if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
					{
						endCommandBuffer(vkd, *cmdBuffer);

						submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

						vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
						beginCommandBuffer(vkd, *cmdBuffer, 0u);
					}

					topLevelAccelerationStructureCopy = makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setBuildWithoutPrimitives(topNoPrimitives);
					topLevelAccelerationStructureCopy->setInactiveInstances(inactiveInstances);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->setCreateGeneric(m_data.topGeneric);
					topLevelAccelerationStructureCopy->setDeferredOperation(htSerialize, workerThreadsCount);
					topLevelAccelerationStructureCopy->createAndDeserializeFrom(vkd, device, *cmdBuffer, allocator, storage.get(), 0u);
					break;
				}
				default:
					DE_ASSERT(DE_FALSE);
			}
			topLevelRayTracedPtr = topLevelAccelerationStructureCopy.get();
		}

		const VkMemoryBarrier				preTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &preTraceMemoryBarrier);

		VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			1u,																	//  deUint32							accelerationStructureCount;
			topLevelRayTracedPtr->getPtr(),										//  const VkAccelerationStructureKHR*	pAccelerationStructures;
		};

		testConfiguration->fillCommandBuffer(m_context, m_data, *cmdBuffer, accelerationStructureWriteDescriptorSet, resultImageInfo);

		const VkMemoryBarrier							postTestMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		const VkMemoryBarrier							postCopyMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTestMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	return resultBuffer;
}

bool RayQueryASBasicTestInstance::iterateNoWorkers (void)
{
	de::SharedPtr<TestConfiguration> testConfiguration	= createTestConfiguration(m_data.shaderSourcePipeline);
	de::SharedPtr<SceneBuilder> sceneBuilder			= de::SharedPtr<SceneBuilder>(new CheckerboardSceneBuilder());

	const de::MovePtr<BufferWithMemory>	buffer		= runTest(testConfiguration.get(), sceneBuilder.get(), 0);

	return testConfiguration->verifyImage(buffer.get(), m_context, m_data);
}

bool RayQueryASBasicTestInstance::iterateWithWorkers (void)
{
	de::SharedPtr<SceneBuilder> sceneBuilder = de::SharedPtr<SceneBuilder>(new CheckerboardSceneBuilder());

	de::SharedPtr<TestConfiguration> testConfigurationS		= createTestConfiguration(m_data.shaderSourcePipeline);
	de::MovePtr<BufferWithMemory>	singleThreadBufferCPU	= runTest(testConfigurationS.get(), sceneBuilder.get(), 0);
	const bool						singleThreadValidation	= testConfigurationS->verifyImage(singleThreadBufferCPU.get(), m_context, m_data);
	testConfigurationS.clear();

	de::SharedPtr<TestConfiguration> testConfigurationM		= createTestConfiguration(m_data.shaderSourcePipeline);
	de::MovePtr<BufferWithMemory>	multiThreadBufferCPU	= runTest(testConfigurationM.get(), sceneBuilder.get(), m_data.workerThreadsCount);
	const bool						multiThreadValidation	= testConfigurationM->verifyImage(multiThreadBufferCPU.get(), m_context, m_data);
	testConfigurationM.clear();

	const deUint32					result					= singleThreadValidation && multiThreadValidation;

	return result;
}

tcu::TestStatus RayQueryASBasicTestInstance::iterate(void)
{
	bool result;
	if (m_data.workerThreadsCount != 0)
		result = iterateWithWorkers();
	else
		result = iterateNoWorkers();

	if (result)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

// Tests dynamic indexing of acceleration structures
class RayQueryASDynamicIndexingTestCase : public TestCase
{
public:
						RayQueryASDynamicIndexingTestCase		(tcu::TestContext& context, const char* name);
						~RayQueryASDynamicIndexingTestCase		(void) = default;

	void				checkSupport							(Context& context) const override;
	void				initPrograms							(SourceCollections& programCollection) const override;
	TestInstance*		createInstance							(Context& context) const override;
};

class RayQueryASDynamicIndexingTestInstance : public TestInstance
{
public:
						RayQueryASDynamicIndexingTestInstance	(Context& context);
						~RayQueryASDynamicIndexingTestInstance	(void) = default;
	tcu::TestStatus		iterate									(void) override;
};

RayQueryASDynamicIndexingTestCase::RayQueryASDynamicIndexingTestCase(tcu::TestContext& context, const char* name)
	: TestCase(context, name, "")
{
}

void RayQueryASDynamicIndexingTestCase::checkSupport(Context& context) const
{
	commonASTestsCheckSupport(context);
	context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");
}

void RayQueryASDynamicIndexingTestCase::initPrograms(SourceCollections& programCollection) const
{
	const vk::SpirVAsmBuildOptions spvBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);

	// compute shader is defined in spir-v as it requires possing pointer to TLAS that was read from ssbo;
	// original spir-v code was generated using following glsl code but resulting spir-v code was modiifed

	// #version 460 core
	// #extension GL_EXT_ray_query : require
	// #extension GL_EXT_nonuniform_qualifier : enable

	// #define ARRAY_SIZE 500
	// layout(set = 0, binding = 0) uniform accelerationStructureEXT tlasArray[ARRAY_SIZE];
	// layout(set = 0, binding = 1) readonly buffer topLevelASPointers {
	//     uvec2 ptr[];
	// } tlasPointers;
	// layout(set = 0, binding = 2) readonly buffer topLevelASIndices {
	//     uint idx[];
	// } tlasIndices;
	// layout(set = 0, binding = 3, std430) writeonly buffer Result {
	//     uint value[];
	// } result;

	// void main()
	// {
	//   float tmin      = 0.0;
	//   float tmax      = 2.0;
	//   vec3  origin    = vec3(0.25f, 0.5f, 1.0);
	//   vec3  direction = vec3(0.0,0.0,-1.0);
	//   uint  tlasIndex = tlasIndices.idx[nonuniformEXT(gl_GlobalInvocationID.x)];

	//   rayQueryEXT rq;
	//   rayQueryInitializeEXT(rq, tlasArray[nonuniformEXT(tlasIndex)], gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, origin, tmin, direction, tmax);
	//   atomicAdd(result.value[nonuniformEXT(gl_GlobalInvocationID.x)], 2);

	//   if (rayQueryProceedEXT(rq))
	//   {
	//     if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
	//       atomicAdd(result.value[nonuniformEXT(gl_GlobalInvocationID.x + gl_NumWorkGroups.x)], 3);
	//   }

	//   //rayQueryInitializeEXT(rq, tlasArray[nonuniformEXT(tlasIndex)], gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, origin, tmin, direction, tmax);
	//   rayQueryInitializeEXT(rq, *tlasPointers.ptr[nonuniformEXT(tlasIndex)], gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, origin, tmin, direction, tmax);
	//   atomicAdd(result.value[nonuniformEXT(gl_GlobalInvocationID.x + gl_NumWorkGroups.x * 2)], 5);

	//   if (rayQueryProceedEXT(rq))
	//   {
	//     if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
	//       atomicAdd(result.value[nonuniformEXT(gl_GlobalInvocationID.x + gl_NumWorkGroups.x * 3)], 7);
	//   }
	// }

	const std::string compSource =
		"OpCapability Shader\n"
		"OpCapability RayQueryKHR\n"
		"OpCapability ShaderNonUniform\n"
		"OpExtension \"SPV_EXT_descriptor_indexing\"\n"
		"OpExtension \"SPV_KHR_ray_query\"\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %4 \"main\" %var_index_ssbo %33 %var_as_arr_uni_ptr %64 %83 %var_as_pointers_ssbo\n"
		"OpExecutionMode %4 LocalSize 1 1 1\n"
		"OpDecorate %25 ArrayStride 4\n"
		"OpMemberDecorate %26 0 NonWritable\n"
		"OpMemberDecorate %26 0 Offset 0\n"
		"OpDecorate %26 Block\n"
		"OpDecorate %var_index_ssbo DescriptorSet 0\n"
		"OpDecorate %var_index_ssbo Binding 2\n"
		"OpDecorate %33 BuiltIn GlobalInvocationId\n"
		"OpDecorate %38 NonUniform\n"
		"OpDecorate %40 NonUniform\n"
		"OpDecorate %41 NonUniform\n"
		"OpDecorate %var_as_arr_uni_ptr DescriptorSet 0\n"
		"OpDecorate %var_as_arr_uni_ptr Binding 0\n"
		"OpDecorate %51 NonUniform\n"
		"OpDecorate %53 NonUniform\n"
		"OpDecorate %54 NonUniform\n"
		"OpDecorate %61 ArrayStride 4\n"
		"OpMemberDecorate %62 0 NonReadable\n"
		"OpMemberDecorate %62 0 Offset 0\n"
		"OpDecorate %62 Block\n"
		"OpDecorate %64 DescriptorSet 0\n"
		"OpDecorate %64 Binding 3\n"
		"OpDecorate %67 NonUniform\n"
		"OpDecorate %83 BuiltIn NumWorkgroups\n"
		"OpDecorate %87 NonUniform\n"
		"OpDecorate %as_index NonUniform\n"
		"OpDecorate %as_device_addres NonUniform\n"
		"OpDecorate %105 NonUniform\n"
		"OpDecorate %122 NonUniform\n"
		"OpDecorate %127 ArrayStride 8\n"
		"OpMemberDecorate %128 0 NonWritable\n"
		"OpMemberDecorate %128 0 Offset 0\n"
		"OpDecorate %128 Block\n"
		"OpDecorate %var_as_pointers_ssbo DescriptorSet 0\n"
		"OpDecorate %var_as_pointers_ssbo Binding 1\n"
		"%2							= OpTypeVoid\n"
		"%3							= OpTypeFunction %2\n"
		"%6							= OpTypeFloat 32\n"
		"%7							= OpTypePointer Function %6\n"
		"%9							= OpConstant %6 0\n"
		"%11						= OpConstant %6 2\n"
		"%12						= OpTypeVector %6 3\n"
		"%13						= OpTypePointer Function %12\n"
		"%15						= OpConstant %6 0.25\n"
		"%16						= OpConstant %6 0.5\n"
		"%17						= OpConstant %6 1\n"
		"%18						= OpConstantComposite %12 %15 %16 %17\n"
		"%20						= OpConstant %6 -1\n"
		"%21						= OpConstantComposite %12 %9 %9 %20\n"
		"%type_uint32				= OpTypeInt 32 0\n"
		"%23						= OpTypePointer Function %type_uint32\n"
		"%25						= OpTypeRuntimeArray %type_uint32\n"
		"%26						= OpTypeStruct %25\n"
		"%27						= OpTypePointer StorageBuffer %26\n"
		"%var_index_ssbo			= OpVariable %27 StorageBuffer\n"
		"%29						= OpTypeInt 32 1\n"
		"%c_int32_0					= OpConstant %29 0\n"
		"%31						= OpTypeVector %type_uint32 3\n"
		"%32						= OpTypePointer Input %31\n"
		"%33						= OpVariable %32 Input\n"
		"%34						= OpConstant %type_uint32 0\n"
		"%35						= OpTypePointer Input %type_uint32\n"
		"%type_uint32_ssbo_ptr		= OpTypePointer StorageBuffer %type_uint32\n"
		"%42						= OpTypeRayQueryKHR\n"
		"%43						= OpTypePointer Function %42\n"
		"%type_as					= OpTypeAccelerationStructureKHR\n"
		"%46						= OpConstant %type_uint32 500\n"
		"%type_as_arr				= OpTypeArray %type_as %46\n"
		"%type_as_arr_uni_ptr		= OpTypePointer UniformConstant %type_as_arr\n"
		"%var_as_arr_uni_ptr		= OpVariable %type_as_arr_uni_ptr UniformConstant\n"
		"%type_as_uni_ptr			= OpTypePointer UniformConstant %type_as\n"
		"%55						= OpConstant %type_uint32 16\n"
		"%56						= OpConstant %type_uint32 255\n"
		"%61						= OpTypeRuntimeArray %type_uint32\n"
		"%62						= OpTypeStruct %61\n"
		"%63						= OpTypePointer StorageBuffer %62\n"
		"%64						= OpVariable %63 StorageBuffer\n"
		"%69						= OpConstant %type_uint32 2\n"
		"%70						= OpConstant %type_uint32 1\n"
		"%72						= OpTypeBool\n"
		"%76						= OpConstantFalse %72\n"
		"%83						= OpVariable %32 Input\n"
		"%89						= OpConstant %type_uint32 3\n"
		"%107						= OpConstant %type_uint32 5\n"
		"%124						= OpConstant %type_uint32 7\n"

		// <changed_section>
		"%v2uint					= OpTypeVector %type_uint32 2\n"
		"%127						= OpTypeRuntimeArray %v2uint\n"
		"%128						= OpTypeStruct %127\n"
		"%129						= OpTypePointer StorageBuffer %128\n"
		"%var_as_pointers_ssbo		= OpVariable %129 StorageBuffer\n"
		"%type_uint64_ssbo_ptr		= OpTypePointer StorageBuffer %v2uint\n"
		// </changed_section>

		// void main()
		"%4							= OpFunction %2 None %3\n"
		"%5							= OpLabel\n"
		"%8							= OpVariable %7 Function\n"
		"%10						= OpVariable %7 Function\n"
		"%14						= OpVariable %13 Function\n"
		"%19						= OpVariable %13 Function\n"
		"%24						= OpVariable %23 Function\n"
		"%var_ray_query				= OpVariable %43 Function\n"
		"OpStore %8 %9\n"
		"OpStore %10 %11\n"
		"OpStore %14 %18\n"
		"OpStore %19 %21\n"
		"%36						= OpAccessChain %35 %33 %34\n"
		"%37						= OpLoad %type_uint32 %36\n"
		"%38						= OpCopyObject %type_uint32 %37\n"
		"%40						= OpAccessChain %type_uint32_ssbo_ptr %var_index_ssbo %c_int32_0 %38\n"
		"%41						= OpLoad %type_uint32 %40\n"
		"OpStore %24 %41\n"

		// rayQueryInitializeEXT using AS that was read from array
		"%50						= OpLoad %type_uint32 %24\n"
		"%51						= OpCopyObject %type_uint32 %50\n"
		"%53						= OpAccessChain %type_as_uni_ptr %var_as_arr_uni_ptr %51\n"
		"%54						= OpLoad %type_as %53\n"
		"%57						= OpLoad %12 %14\n"
		"%58						= OpLoad %6 %8\n"
		"%59						= OpLoad %12 %19\n"
		"%60						= OpLoad %6 %10\n"
		"OpRayQueryInitializeKHR %var_ray_query %54 %55 %56 %57 %58 %59 %60\n"

		"%65						= OpAccessChain %35 %33 %34\n"
		"%66						= OpLoad %type_uint32 %65\n"
		"%67						= OpCopyObject %type_uint32 %66\n"
		"%68						= OpAccessChain %type_uint32_ssbo_ptr %64 %c_int32_0 %67\n"
		"%71						= OpAtomicIAdd %type_uint32 %68 %70 %34 %69\n"

		"%73						= OpRayQueryProceedKHR %72 %var_ray_query\n"
		"OpSelectionMerge %75 None\n"
		"OpBranchConditional %73 %74 %75\n"
		"%74						= OpLabel\n"

		"%77						= OpRayQueryGetIntersectionTypeKHR %type_uint32 %var_ray_query %c_int32_0\n"
		"%78						= OpIEqual %72 %77 %34\n"
		"OpSelectionMerge %80 None\n"
		"OpBranchConditional %78 %79 %80\n"
		"%79						= OpLabel\n"
		"%81						= OpAccessChain %35 %33 %34\n"
		"%82						= OpLoad %type_uint32 %81\n"
		"%84						= OpAccessChain %35 %83 %34\n"
		"%85						= OpLoad %type_uint32 %84\n"
		"%86						= OpIAdd %type_uint32 %82 %85\n"
		"%87						= OpCopyObject %type_uint32 %86\n"
		"%88						= OpAccessChain %type_uint32_ssbo_ptr %64 %c_int32_0 %87\n"
		"%90						= OpAtomicIAdd %type_uint32 %88 %70 %34 %89\n"
		"OpBranch %80\n"
		"%80						= OpLabel\n"
		"OpBranch %75\n"
		"%75						= OpLabel\n"

		// rayQueryInitializeEXT using pointer to AS
		"%91						= OpLoad %type_uint32 %24\n"
		"%as_index					= OpCopyObject %type_uint32 %91\n"

		// <changed_section>
		"%as_device_addres_ptr		= OpAccessChain %type_uint64_ssbo_ptr %var_as_pointers_ssbo %c_int32_0 %as_index\n"
		"%as_device_addres			= OpLoad %v2uint %as_device_addres_ptr\n"
		"%as_to_use					= OpConvertUToAccelerationStructureKHR %type_as %as_device_addres\n"
		// </changed_section>

		"%95						= OpLoad %12 %14\n"
		"%96						= OpLoad %6 %8\n"
		"%97						= OpLoad %12 %19\n"
		"%98						= OpLoad %6 %10\n"
		"OpRayQueryInitializeKHR %var_ray_query %as_to_use %55 %56 %95 %96 %97 %98\n"

		"%99						= OpAccessChain %35 %33 %34\n"
		"%100						= OpLoad %type_uint32 %99\n"
		"%101						= OpAccessChain %35 %83 %34\n"
		"%102						= OpLoad %type_uint32 %101\n"
		"%103						= OpIMul %type_uint32 %102 %69\n"
		"%104						= OpIAdd %type_uint32 %100 %103\n"
		"%105						= OpCopyObject %type_uint32 %104\n"
		"%106						= OpAccessChain %type_uint32_ssbo_ptr %64 %c_int32_0 %105\n"
		"%108						= OpAtomicIAdd %type_uint32 %106 %70 %34 %107\n"

		"%109						= OpRayQueryProceedKHR %72 %var_ray_query\n"
		"OpSelectionMerge %111 None\n"
		"OpBranchConditional %109 %110 %111\n"
		"%110						= OpLabel\n"

		"%112						= OpRayQueryGetIntersectionTypeKHR %type_uint32 %var_ray_query %c_int32_0\n"
		"%113						= OpIEqual %72 %112 %34\n"
		"OpSelectionMerge %115 None\n"
		"OpBranchConditional %113 %114 %115\n"
		"%114						= OpLabel\n"
		"%116						= OpAccessChain %35 %33 %34\n"
		"%117						= OpLoad %type_uint32 %116\n"
		"%118						= OpAccessChain %35 %83 %34\n"
		"%119						= OpLoad %type_uint32 %118\n"
		"%120						= OpIMul %type_uint32 %119 %89\n"
		"%121						= OpIAdd %type_uint32 %117 %120\n"
		"%122						= OpCopyObject %type_uint32 %121\n"
		"%123						= OpAccessChain %type_uint32_ssbo_ptr %64 %c_int32_0 %122\n"
		"%125						= OpAtomicIAdd %type_uint32 %123 %70 %34 %124\n"
		"OpBranch %115\n"
		"%115						= OpLabel\n"
		"OpBranch %111\n"
		"%111						= OpLabel\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

	programCollection.spirvAsmSources.add("comp") << compSource << spvBuildOptions;
}

TestInstance* RayQueryASDynamicIndexingTestCase::createInstance(Context& context) const
{
	return new RayQueryASDynamicIndexingTestInstance(context);
}


RayQueryASDynamicIndexingTestInstance::RayQueryASDynamicIndexingTestInstance(Context& context)
	: vkt::TestInstance(context)
{
}

tcu::TestStatus RayQueryASDynamicIndexingTestInstance::iterate(void)
{
	const DeviceInterface&		vkd							= m_context.getDeviceInterface();
	const VkDevice				device						= m_context.getDevice();
	const deUint32				queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkQueue				queue						= m_context.getUniversalQueue();
	Allocator&					allocator					= m_context.getDefaultAllocator();
	const deUint32				tlasCount					= 500;	// changing this will require also changing shaders
	const deUint32				activeTlasCount				= 32;	// number of tlas out of <tlasCount> that will be active

	const Move<VkDescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addArrayBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, tlasCount, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)			// pointers to all acceleration structures
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)			// ssbo with indices of all acceleration structures
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)			// ssbo with result values
		.build(vkd, device);

	const Move<VkDescriptorPool> descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, tlasCount)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);

	const Move<VkPipelineLayout>		pipelineLayout		= makePipelineLayout(vkd, device, descriptorSetLayout.get());
	Move<VkShaderModule>				shaderModule		= createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);
	const VkComputePipelineCreateInfo	pipelineCreateInfo
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType						sType
		DE_NULL,												// const void*							pNext
		0u,														// VkPipelineCreateFlags				flags
		{														// VkPipelineShaderStageCreateInfo		stage
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			*shaderModule,
			"main",
			DE_NULL
		},
		*pipelineLayout,										// VkPipelineLayout						layout
		DE_NULL,												// VkPipeline							basePipelineHandle
		0,														// deInt32								basePipelineIndex
	};

	Move<VkPipeline>				pipeline				= createComputePipeline(vkd, device, DE_NULL, &pipelineCreateInfo);

	const VkDeviceSize				pointerBufferSize		= tlasCount * sizeof(VkDeviceAddress);
	const VkBufferCreateInfo		pointerBufferCreateInfo = makeBufferCreateInfo(pointerBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<BufferWithMemory>	pointerBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, pointerBufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));

	const VkDeviceSize				indicesBufferSize		= activeTlasCount * sizeof(deUint32);
	const VkBufferCreateInfo		indicesBufferCreateInfo = makeBufferCreateInfo(indicesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<BufferWithMemory>	indicesBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, indicesBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDeviceSize				resultBufferSize		= activeTlasCount * sizeof(deUint32) * 4;
	const VkBufferCreateInfo		resultBufferCreateInfo	= makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	de::MovePtr<BufferWithMemory>	resultBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const Move<VkCommandPool>		cmdPool					= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>		cmdBuffer				= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	de::SharedPtr<BottomLevelAccelerationStructure>				blas = de::SharedPtr<BottomLevelAccelerationStructure>(makeBottomLevelAccelerationStructure().release());
	std::vector<de::MovePtr<TopLevelAccelerationStructure>>		tlasVect(tlasCount);
	std::vector<VkDeviceAddress>								tlasPtrVect(tlasCount);
	std::vector<VkAccelerationStructureKHR>						tlasVkVect;

	// randomly scatter AS indices across the range (number of them should be equal to the max subgroup size)
	deRandom rnd;
	deRandom_init(&rnd, 123);
	std::set<deUint32> asIndicesSet;
	while (asIndicesSet.size() < activeTlasCount)
		asIndicesSet.insert(deRandom_getUint32(&rnd) % tlasCount);

	// fill indices buffer
	deUint32 helperIndex = 0;
	auto& indicesBufferAlloc = indicesBuffer->getAllocation();
	deUint32* indicesBufferPtr = reinterpret_cast<deUint32*>(indicesBufferAlloc.getHostPtr());
	std::for_each(asIndicesSet.begin(), asIndicesSet.end(),
		[&helperIndex, indicesBufferPtr](const deUint32& index)
		{
			indicesBufferPtr[helperIndex++] = index;
		});
	vk::flushAlloc(vkd, device, indicesBufferAlloc);

	// clear result buffer
	auto& resultBufferAlloc = resultBuffer->getAllocation();
	void* resultBufferPtr = resultBufferAlloc.getHostPtr();
	deMemset(resultBufferPtr, 0, static_cast<size_t>(resultBufferSize));
	vk::flushAlloc(vkd, device, resultBufferAlloc);

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		// build bottom level acceleration structure
		blas->setGeometryData(
			{
				{ 0.0, 0.0, 0.0 },
				{ 1.0, 0.0, 0.0 },
				{ 0.0, 1.0, 0.0 },
			},
			true,
			0u
			);

		blas->createAndBuild(vkd, device, *cmdBuffer, allocator);

		// build top level acceleration structures
		for (deUint32 tlasIndex = 0; tlasIndex < tlasCount; ++tlasIndex)
		{
			auto& tlas = tlasVect[tlasIndex];
			tlas = makeTopLevelAccelerationStructure();
			tlas->setInstanceCount(1);
			tlas->addInstance(blas);
			if (!asIndicesSet.count(tlasIndex))
			{
				// tlas that are not in asIndicesSet should be empty but it is hard to do
				// that with current cts utils so we are marking them as inactive instead
				tlas->setInactiveInstances(true);
			}
			tlas->createAndBuild(vkd, device, *cmdBuffer, allocator);

			// get acceleration structure device address
			const VkAccelerationStructureDeviceAddressInfoKHR addressInfo =
			{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,	// VkStructureType				sType
				DE_NULL,															// const void*					pNext
				*tlas->getPtr()														// VkAccelerationStructureKHR	accelerationStructure
			};
			VkDeviceAddress vkda = vkd.getAccelerationStructureDeviceAddressKHR(device, &addressInfo);
			tlasPtrVect[tlasIndex] = vkda;
		}

		// fill pointer buffer
		vkd.cmdUpdateBuffer(*cmdBuffer, **pointerBuffer, 0, pointerBufferSize, tlasPtrVect.data());

		// wait for data transfers
		const VkMemoryBarrier uploadBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &uploadBarrier, 1u);

		// wait for as build
		const VkMemoryBarrier asBuildBarrier = makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &asBuildBarrier, 1u);

		tlasVkVect.reserve(tlasCount);
		for (auto& tlas : tlasVect)
			tlasVkVect.push_back(*tlas->getPtr());

		VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	// VkStructureType						sType;
			DE_NULL,															// const void*							pNext;
			tlasCount,															// deUint32								accelerationStructureCount;
			tlasVkVect.data(),													// const VkAccelerationStructureKHR*	pAccelerationStructures;
		};

		const vk::VkDescriptorBufferInfo pointerBufferInfo	= makeDescriptorBufferInfo(**pointerBuffer, 0u, VK_WHOLE_SIZE);
		const vk::VkDescriptorBufferInfo indicesBufferInfo	= makeDescriptorBufferInfo(**indicesBuffer, 0u, VK_WHOLE_SIZE);
		const vk::VkDescriptorBufferInfo resultInfo			= makeDescriptorBufferInfo(**resultBuffer,  0u, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder()
			.writeArray (*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, tlasCount, &accelerationStructureWriteDescriptorSet)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pointerBufferInfo)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indicesBufferInfo)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(3u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo)
			.update(vkd, device);

		vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

		vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

		vkd.cmdDispatch(*cmdBuffer, activeTlasCount, 1, 1);

		const VkMemoryBarrier postTraceMemoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), resultBufferSize);

	// verify result buffer
	deUint32		failures = 0;
	const deUint32*	resultPtr = reinterpret_cast<deUint32*>(resultBuffer->getAllocation().getHostPtr());
	for (deUint32 index = 0; index < activeTlasCount; ++index)
	{
		failures += (resultPtr[0 * activeTlasCount + index] != 2) +
					(resultPtr[1 * activeTlasCount + index] != 3) +
					(resultPtr[2 * activeTlasCount + index] != 5) +
					(resultPtr[3 * activeTlasCount + index] != 7);
	}

	if (failures)
		return tcu::TestStatus::fail(de::toString(failures) + " failures, " + de::toString(4 * activeTlasCount - failures) + " are ok");
	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

/********************/

void addBasicBuildingTests(tcu::TestCaseGroup* group)
{
	struct ShaderSourceTypeData
	{
		ShaderSourceType						shaderSourceType;
		ShaderSourcePipeline					shaderSourcePipeline;
		const char*								name;
	} shaderSourceTypes[] =
	{
		{ SST_FRAGMENT_SHADER,					SSP_GRAPHICS_PIPELINE,		"fragment_shader",			},
		{ SST_COMPUTE_SHADER,					SSP_COMPUTE_PIPELINE,		"compute_shader",			},
		{ SST_CLOSEST_HIT_SHADER,				SSP_RAY_TRACING_PIPELINE,	"chit_shader",				},
	};

	struct
	{
		vk::VkAccelerationStructureBuildTypeKHR	buildType;
		const char*								name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,				"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,				"gpu_built"	},
	};

	struct
	{
		BottomTestType							testType;
		bool									usesAOP;
		const char*								name;
	} bottomTestTypes[] =
	{
		{ BTT_TRIANGLES,	false,										"triangles" },
		{ BTT_TRIANGLES,	true,										"triangles_aop" },
		{ BTT_AABBS,		false,										"aabbs" },
		{ BTT_AABBS,		true,										"aabbs_aop" },
	};

	struct
	{
		TopTestType								testType;
		bool									usesAOP;
		const char*								name;
	} topTestTypes[] =
	{
		{ TTT_IDENTICAL_INSTANCES,	false,								"identical_instances" },
		{ TTT_IDENTICAL_INSTANCES,	true,								"identical_instances_aop" },
		{ TTT_DIFFERENT_INSTANCES,	false,								"different_instances" },
		{ TTT_DIFFERENT_INSTANCES,	true,								"different_instances_aop" },
	};

	struct BuildFlagsData
	{
		VkBuildAccelerationStructureFlagsKHR	flags;
		const char*								name;
	};

	BuildFlagsData optimizationTypes[] =
	{
		{ VkBuildAccelerationStructureFlagsKHR(0u),						"0" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,	"fasttrace" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,	"fastbuild" },
	};

	BuildFlagsData updateTypes[] =
	{
		{ VkBuildAccelerationStructureFlagsKHR(0u),						"0" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,			"update" },
	};

	BuildFlagsData compactionTypes[] =
	{
		{ VkBuildAccelerationStructureFlagsKHR(0u),						"0" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,		"compaction" },
	};

	BuildFlagsData lowMemoryTypes[] =
	{
		{ VkBuildAccelerationStructureFlagsKHR(0u),						"0" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR,			"lowmemory" },
	};

	struct
	{
		bool		padVertices;
		const char*	name;
	} paddingType[] =
	{
		{ false,	"nopadding"	},
		{ true,		"padded"	},
	};

	struct
	{
		bool		topGeneric;
		bool		bottomGeneric;
		const char*	suffix;
	} createGenericParams[] =
	{
		{	false,	false,	""					},
		{	false,	true,	"_bottomgeneric"	},
		{	true,	false,	"_topgeneric"		},
		{	true,	true,	"_bothgeneric"		},
	};

	for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name, ""));

		for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name, ""));

			for (size_t bottomNdx = 0; bottomNdx < DE_LENGTH_OF_ARRAY(bottomTestTypes); ++bottomNdx)
			{
				de::MovePtr<tcu::TestCaseGroup> bottomGroup(new tcu::TestCaseGroup(group->getTestContext(), bottomTestTypes[bottomNdx].name, ""));

				for (size_t topNdx = 0; topNdx < DE_LENGTH_OF_ARRAY(topTestTypes); ++topNdx)
				{
					de::MovePtr<tcu::TestCaseGroup> topGroup(new tcu::TestCaseGroup(group->getTestContext(), topTestTypes[topNdx].name, ""));

					for (int paddingTypeIdx = 0; paddingTypeIdx < DE_LENGTH_OF_ARRAY(paddingType); ++paddingTypeIdx)
					{
						de::MovePtr<tcu::TestCaseGroup> paddingTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), paddingType[paddingTypeIdx].name, ""));

						for (size_t optimizationNdx = 0; optimizationNdx < DE_LENGTH_OF_ARRAY(optimizationTypes); ++optimizationNdx)
						{
							for (size_t updateNdx = 0; updateNdx < DE_LENGTH_OF_ARRAY(updateTypes); ++updateNdx)
							{
								for (size_t compactionNdx = 0; compactionNdx < DE_LENGTH_OF_ARRAY(compactionTypes); ++compactionNdx)
								{
									for (size_t lowMemoryNdx = 0; lowMemoryNdx < DE_LENGTH_OF_ARRAY(lowMemoryTypes); ++lowMemoryNdx)
									{
										for (int createGenericIdx = 0; createGenericIdx < DE_LENGTH_OF_ARRAY(createGenericParams); ++createGenericIdx)
										{
											std::string testName =
												std::string(optimizationTypes[optimizationNdx].name) + "_" +
												std::string(updateTypes[updateNdx].name) + "_" +
												std::string(compactionTypes[compactionNdx].name) + "_" +
												std::string(lowMemoryTypes[lowMemoryNdx].name) +
												std::string(createGenericParams[createGenericIdx].suffix);

											TestParams testParams
											{
												shaderSourceTypes[shaderSourceNdx].shaderSourceType,
												shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
												buildTypes[buildTypeNdx].buildType,
												VK_FORMAT_R32G32B32_SFLOAT,
												paddingType[paddingTypeIdx].padVertices,
												VK_INDEX_TYPE_NONE_KHR,
												bottomTestTypes[bottomNdx].testType,
												InstanceCullFlags::NONE,
												bottomTestTypes[bottomNdx].usesAOP,
												createGenericParams[createGenericIdx].bottomGeneric,
												topTestTypes[topNdx].testType,
												topTestTypes[topNdx].usesAOP,
												createGenericParams[createGenericIdx].topGeneric,
												optimizationTypes[optimizationNdx].flags | updateTypes[updateNdx].flags | compactionTypes[compactionNdx].flags | lowMemoryTypes[lowMemoryNdx].flags,
												OT_NONE,
												OP_NONE,
												TEST_WIDTH,
												TEST_HEIGHT,
												0u,
												EmptyAccelerationStructureCase::NOT_EMPTY,
											};
											paddingTypeGroup->addChild(new RayQueryASBasicTestCase(group->getTestContext(), testName.c_str(), "", testParams));
										}
									}
								}
							}
						}
						topGroup->addChild(paddingTypeGroup.release());
					}
					bottomGroup->addChild(topGroup.release());
				}
				buildGroup->addChild(bottomGroup.release());
			}
			sourceTypeGroup->addChild(buildGroup.release());
		}
		group->addChild(sourceTypeGroup.release());
	}
}

void addVertexIndexFormatsTests(tcu::TestCaseGroup* group)
{
	struct ShaderSourceTypeData
	{
		ShaderSourceType						shaderSourceType;
		ShaderSourcePipeline					shaderSourcePipeline;
		const char*								name;
	} shaderSourceTypes[] =
	{
		{ SST_VERTEX_SHADER,					SSP_GRAPHICS_PIPELINE,		"vertex_shader"				},
		{ SST_TESSELATION_CONTROL_SHADER,		SSP_GRAPHICS_PIPELINE,		"tess_control_shader"		},
		{ SST_TESSELATION_EVALUATION_SHADER,	SSP_GRAPHICS_PIPELINE,		"tess_evaluation_shader"	},
		{ SST_GEOMETRY_SHADER,					SSP_GRAPHICS_PIPELINE,		"geometry_shader",			},
		{ SST_FRAGMENT_SHADER,					SSP_GRAPHICS_PIPELINE,		"fragment_shader",			},
		{ SST_COMPUTE_SHADER,					SSP_COMPUTE_PIPELINE,		"compute_shader",			},
		{ SST_RAY_GENERATION_SHADER,			SSP_RAY_TRACING_PIPELINE,	"rgen_shader",				},
		{ SST_INTERSECTION_SHADER,				SSP_RAY_TRACING_PIPELINE,	"isect_shader",				},
		{ SST_ANY_HIT_SHADER,					SSP_RAY_TRACING_PIPELINE,	"ahit_shader",				},
		{ SST_CLOSEST_HIT_SHADER,				SSP_RAY_TRACING_PIPELINE,	"chit_shader",				},
		{ SST_MISS_SHADER,						SSP_RAY_TRACING_PIPELINE,	"miss_shader",				},
		{ SST_CALLABLE_SHADER,					SSP_RAY_TRACING_PIPELINE,	"call_shader",				},
	};

	struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		const char*											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
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

	struct
	{
		VkIndexType								indexType;
		const char*								name;
	} indexFormats[] =
	{
		{ VK_INDEX_TYPE_NONE_KHR ,				"index_none"	},
		{ VK_INDEX_TYPE_UINT16 ,				"index_uint16"	},
		{ VK_INDEX_TYPE_UINT32 ,				"index_uint32"	},
	};

	struct
	{
		bool		padVertices;
		const char*	name;
	} paddingType[] =
	{
		{ false,	"nopadding"	},
		{ true,		"padded"	},
	};

	for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name, ""));

		for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name, ""));

			for (size_t vertexFormatNdx = 0; vertexFormatNdx < DE_LENGTH_OF_ARRAY(vertexFormats); ++vertexFormatNdx)
			{
				const auto format		= vertexFormats[vertexFormatNdx];
				const auto formatName	= getFormatSimpleName(format);

				de::MovePtr<tcu::TestCaseGroup> vertexFormatGroup(new tcu::TestCaseGroup(group->getTestContext(), formatName.c_str(), ""));

				for (int paddingIdx = 0; paddingIdx < DE_LENGTH_OF_ARRAY(paddingType); ++paddingIdx)
				{
					de::MovePtr<tcu::TestCaseGroup> paddingGroup(new tcu::TestCaseGroup(group->getTestContext(), paddingType[paddingIdx].name, ""));

					for (size_t indexFormatNdx = 0; indexFormatNdx < DE_LENGTH_OF_ARRAY(indexFormats); ++indexFormatNdx)
					{
						TestParams testParams
						{
							shaderSourceTypes[shaderSourceNdx].shaderSourceType,
							shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
							buildTypes[buildTypeNdx].buildType,
							format,
							paddingType[paddingIdx].padVertices,
							indexFormats[indexFormatNdx].indexType,
							BTT_TRIANGLES,
							InstanceCullFlags::NONE,
							false,
							false,
							TTT_IDENTICAL_INSTANCES,
							false,
							false,
							VkBuildAccelerationStructureFlagsKHR(0u),
							OT_NONE,
							OP_NONE,
							TEST_WIDTH,
							TEST_HEIGHT,
							0u,
							EmptyAccelerationStructureCase::NOT_EMPTY,
						};
						paddingGroup->addChild(new RayQueryASBasicTestCase(group->getTestContext(), indexFormats[indexFormatNdx].name, "", testParams));
					}
					vertexFormatGroup->addChild(paddingGroup.release());
				}
				buildGroup->addChild(vertexFormatGroup.release());
			}
			sourceTypeGroup->addChild(buildGroup.release());
		}
		group->addChild(sourceTypeGroup.release());
	}
}

void addOperationTestsImpl (tcu::TestCaseGroup* group, const deUint32 workerThreads)
{
	struct ShaderSourceTypeData
	{
		ShaderSourceType						shaderSourceType;
		ShaderSourcePipeline					shaderSourcePipeline;
		const char*								name;
	} shaderSourceTypes[] =
	{
		{ SST_VERTEX_SHADER,					SSP_GRAPHICS_PIPELINE,		"vertex_shader"				},
		{ SST_TESSELATION_CONTROL_SHADER,		SSP_GRAPHICS_PIPELINE,		"tess_control_shader"		},
		{ SST_TESSELATION_EVALUATION_SHADER,	SSP_GRAPHICS_PIPELINE,		"tess_evaluation_shader"	},
		{ SST_GEOMETRY_SHADER,					SSP_GRAPHICS_PIPELINE,		"geometry_shader",			},
		{ SST_FRAGMENT_SHADER,					SSP_GRAPHICS_PIPELINE,		"fragment_shader",			},
		{ SST_COMPUTE_SHADER,					SSP_COMPUTE_PIPELINE,		"compute_shader",			},
		{ SST_RAY_GENERATION_SHADER,			SSP_RAY_TRACING_PIPELINE,	"rgen_shader",				},
		{ SST_INTERSECTION_SHADER,				SSP_RAY_TRACING_PIPELINE,	"isect_shader",				},
		{ SST_ANY_HIT_SHADER,					SSP_RAY_TRACING_PIPELINE,	"ahit_shader",				},
		{ SST_CLOSEST_HIT_SHADER,				SSP_RAY_TRACING_PIPELINE,	"chit_shader",				},
		{ SST_MISS_SHADER,						SSP_RAY_TRACING_PIPELINE,	"miss_shader",				},
		{ SST_CALLABLE_SHADER,					SSP_RAY_TRACING_PIPELINE,	"call_shader",				},
	};

	struct
	{
		OperationType										operationType;
		const char*											name;
	} operationTypes[] =
	{
		{ OP_COPY,											"copy"			},
		{ OP_COMPACT,										"compaction"	},
		{ OP_SERIALIZE,										"serialization"	},
	};

	struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		const char*											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	struct
	{
		OperationTarget										operationTarget;
		const char*											name;
	} operationTargets[] =
	{
		{ OT_TOP_ACCELERATION,								"top_acceleration_structure"		},
		{ OT_BOTTOM_ACCELERATION,							"bottom_acceleration_structure"	},
	};

	struct
	{
		BottomTestType										testType;
		const char*											name;
	} bottomTestTypes[] =
	{
		{ BTT_TRIANGLES,									"triangles" },
		{ BTT_AABBS,										"aabbs" },
	};

	for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name, ""));

		for (size_t operationTypeNdx = 0; operationTypeNdx < DE_LENGTH_OF_ARRAY(operationTypes); ++operationTypeNdx)
		{
			if (workerThreads > 0)
				if (operationTypes[operationTypeNdx].operationType != OP_COPY && operationTypes[operationTypeNdx].operationType != OP_SERIALIZE)
					continue;

			de::MovePtr<tcu::TestCaseGroup> operationTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), operationTypes[operationTypeNdx].name, ""));

			for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
			{
				if (workerThreads > 0 && buildTypes[buildTypeNdx].buildType != VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR)
					continue;

				de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name, ""));

				for (size_t operationTargetNdx = 0; operationTargetNdx < DE_LENGTH_OF_ARRAY(operationTargets); ++operationTargetNdx)
				{
					de::MovePtr<tcu::TestCaseGroup> operationTargetGroup(new tcu::TestCaseGroup(group->getTestContext(), operationTargets[operationTargetNdx].name, ""));

					for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(bottomTestTypes); ++testTypeNdx)
					{
						TopTestType topTest = (operationTargets[operationTargetNdx].operationTarget == OT_TOP_ACCELERATION) ? TTT_DIFFERENT_INSTANCES : TTT_IDENTICAL_INSTANCES;

						TestParams testParams
						{
							shaderSourceTypes[shaderSourceNdx].shaderSourceType,
							shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
							buildTypes[buildTypeNdx].buildType,
							VK_FORMAT_R32G32B32_SFLOAT,
							false,
							VK_INDEX_TYPE_NONE_KHR,
							bottomTestTypes[testTypeNdx].testType,
							InstanceCullFlags::NONE,
							false,
							false,
							topTest,
							false,
							false,
							VkBuildAccelerationStructureFlagsKHR(0u),
							operationTargets[operationTargetNdx].operationTarget,
							operationTypes[operationTypeNdx].operationType,
							TEST_WIDTH,
							TEST_HEIGHT,
							workerThreads,
							EmptyAccelerationStructureCase::NOT_EMPTY,
						};
						operationTargetGroup->addChild(new RayQueryASBasicTestCase(group->getTestContext(), bottomTestTypes[testTypeNdx].name, "", testParams));
					}
					buildGroup->addChild(operationTargetGroup.release());
				}
				operationTypeGroup->addChild(buildGroup.release());
			}
			sourceTypeGroup->addChild(operationTypeGroup.release());
		}
		group->addChild(sourceTypeGroup.release());
	}
}

void addOperationTests (tcu::TestCaseGroup* group)
{
	addOperationTestsImpl(group, 0);
}

void addHostThreadingOperationTests (tcu::TestCaseGroup* group)
{
	const deUint32	threads[]	= { 1, 2, 3, 4, 8, std::numeric_limits<deUint32>::max() };

	for (size_t threadsNdx = 0; threadsNdx < DE_LENGTH_OF_ARRAY(threads); ++threadsNdx)
	{
		const std::string groupName = threads[threadsNdx] != std::numeric_limits<deUint32>::max()
									? de::toString(threads[threadsNdx])
									: "max";

		de::MovePtr<tcu::TestCaseGroup> threadGroup(new tcu::TestCaseGroup(group->getTestContext(), groupName.c_str(), ""));

		addOperationTestsImpl(threadGroup.get(), threads[threadsNdx]);

		group->addChild(threadGroup.release());
	}
}

void addFuncArgTests (tcu::TestCaseGroup* group)
{
	const struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		const char*											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	auto& ctx = group->getTestContext();

	for (int buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
	{
		TestParams testParams
		{
			SST_COMPUTE_SHADER,
			SSP_COMPUTE_PIPELINE,
			buildTypes[buildTypeNdx].buildType,
			VK_FORMAT_R32G32B32_SFLOAT,
			false,
			VK_INDEX_TYPE_NONE_KHR,
			BTT_TRIANGLES,
			InstanceCullFlags::NONE,
			false,
			false,
			TTT_IDENTICAL_INSTANCES,
			false,
			false,
			VkBuildAccelerationStructureFlagsKHR(0u),
			OT_NONE,
			OP_NONE,
			TEST_WIDTH,
			TEST_HEIGHT,
			0u,
			EmptyAccelerationStructureCase::NOT_EMPTY,
		};

		group->addChild(new RayQueryASFuncArgTestCase(ctx, buildTypes[buildTypeNdx].name, "", testParams));
	}
}

void addInstanceTriangleCullingTests (tcu::TestCaseGroup* group)
{
	const struct
	{
		ShaderSourceType						shaderSourceType;
		ShaderSourcePipeline					shaderSourcePipeline;
		std::string								name;
	} shaderSourceTypes[] =
	{
		{ SST_VERTEX_SHADER,					SSP_GRAPHICS_PIPELINE,		"vertex_shader"				},
		{ SST_TESSELATION_CONTROL_SHADER,		SSP_GRAPHICS_PIPELINE,		"tess_control_shader"		},
		{ SST_TESSELATION_EVALUATION_SHADER,	SSP_GRAPHICS_PIPELINE,		"tess_evaluation_shader"	},
		{ SST_GEOMETRY_SHADER,					SSP_GRAPHICS_PIPELINE,		"geometry_shader",			},
		{ SST_FRAGMENT_SHADER,					SSP_GRAPHICS_PIPELINE,		"fragment_shader",			},
		{ SST_COMPUTE_SHADER,					SSP_COMPUTE_PIPELINE,		"compute_shader",			},
		{ SST_RAY_GENERATION_SHADER,			SSP_RAY_TRACING_PIPELINE,	"rgen_shader",				},
		{ SST_INTERSECTION_SHADER,				SSP_RAY_TRACING_PIPELINE,	"isect_shader",				},
		{ SST_ANY_HIT_SHADER,					SSP_RAY_TRACING_PIPELINE,	"ahit_shader",				},
		{ SST_CLOSEST_HIT_SHADER,				SSP_RAY_TRACING_PIPELINE,	"chit_shader",				},
		{ SST_MISS_SHADER,						SSP_RAY_TRACING_PIPELINE,	"miss_shader",				},
		{ SST_CALLABLE_SHADER,					SSP_RAY_TRACING_PIPELINE,	"call_shader",				},
	};

	const struct
	{
		InstanceCullFlags	cullFlags;
		std::string			name;
	} cullFlags[] =
	{
		{ InstanceCullFlags::NONE,				"noflags"		},
		{ InstanceCullFlags::COUNTERCLOCKWISE,	"ccw"			},
		{ InstanceCullFlags::CULL_DISABLE,		"nocull"		},
		{ InstanceCullFlags::ALL,				"ccw_nocull"	},
	};

	const struct
	{
		TopTestType	topType;
		std::string	name;
	} topType[] =
	{
		{ TTT_DIFFERENT_INSTANCES, "transformed"	},	// Each instance has its own transformation matrix.
		{ TTT_IDENTICAL_INSTANCES, "notransform"	},	// "Identical" instances, different geometries.
	};

	const struct
	{
		vk::VkAccelerationStructureBuildTypeKHR	buildType;
		std::string								name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	const struct
	{
		VkIndexType	indexType;
		std::string	name;
	} indexFormats[] =
	{
		{ VK_INDEX_TYPE_NONE_KHR ,	"index_none"	},
		{ VK_INDEX_TYPE_UINT16 ,	"index_uint16"	},
		{ VK_INDEX_TYPE_UINT32 ,	"index_uint32"	},
	};

	auto& ctx = group->getTestContext();

	for (int shaderSourceIdx = 0; shaderSourceIdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceIdx)
	{
		de::MovePtr<tcu::TestCaseGroup> shaderSourceGroup(new tcu::TestCaseGroup(ctx, shaderSourceTypes[shaderSourceIdx].name.c_str(), ""));

		for (int buildTypeIdx = 0; buildTypeIdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> buildTypeGroup(new tcu::TestCaseGroup(ctx, buildTypes[buildTypeIdx].name.c_str(), ""));

			for (int indexFormatIdx = 0; indexFormatIdx < DE_LENGTH_OF_ARRAY(indexFormats); ++indexFormatIdx)
			{
				de::MovePtr<tcu::TestCaseGroup> indexTypeGroup(new tcu::TestCaseGroup(ctx, indexFormats[indexFormatIdx].name.c_str(), ""));

				for (int topTypeIdx = 0; topTypeIdx < DE_LENGTH_OF_ARRAY(topType); ++topTypeIdx)
				{
					for (int cullFlagsIdx = 0; cullFlagsIdx < DE_LENGTH_OF_ARRAY(cullFlags); ++cullFlagsIdx)
					{
						const std::string testName = topType[topTypeIdx].name + "_" + cullFlags[cullFlagsIdx].name;

						TestParams testParams
						{
							shaderSourceTypes[shaderSourceIdx].shaderSourceType,
							shaderSourceTypes[shaderSourceIdx].shaderSourcePipeline,
							buildTypes[buildTypeIdx].buildType,
							VK_FORMAT_R32G32B32_SFLOAT,
							false,
							indexFormats[indexFormatIdx].indexType,
							BTT_TRIANGLES,
							cullFlags[cullFlagsIdx].cullFlags,
							false,
							false,
							topType[topTypeIdx].topType,
							false,
							false,
							VkBuildAccelerationStructureFlagsKHR(0u),
							OT_NONE,
							OP_NONE,
							TEST_WIDTH,
							TEST_HEIGHT,
							0u,
							EmptyAccelerationStructureCase::NOT_EMPTY,
						};
						indexTypeGroup->addChild(new RayQueryASBasicTestCase(ctx, testName.c_str(), "", testParams));
					}
				}
				buildTypeGroup->addChild(indexTypeGroup.release());
			}
			shaderSourceGroup->addChild(buildTypeGroup.release());
		}
		group->addChild(shaderSourceGroup.release());
	}
}

void addDynamicIndexingTests(tcu::TestCaseGroup* group)
{
	auto& ctx = group->getTestContext();
	group->addChild(new RayQueryASDynamicIndexingTestCase(ctx, "dynamic_indexing"));
}

void addEmptyAccelerationStructureTests (tcu::TestCaseGroup* group)
{
	const struct
	{
		ShaderSourceType						shaderSourceType;
		ShaderSourcePipeline					shaderSourcePipeline;
		std::string								name;
	} shaderSourceTypes[] =
	{
		{ SST_VERTEX_SHADER,					SSP_GRAPHICS_PIPELINE,		"vertex_shader"				},
		{ SST_TESSELATION_CONTROL_SHADER,		SSP_GRAPHICS_PIPELINE,		"tess_control_shader"		},
		{ SST_TESSELATION_EVALUATION_SHADER,	SSP_GRAPHICS_PIPELINE,		"tess_evaluation_shader"	},
		{ SST_GEOMETRY_SHADER,					SSP_GRAPHICS_PIPELINE,		"geometry_shader",			},
		{ SST_FRAGMENT_SHADER,					SSP_GRAPHICS_PIPELINE,		"fragment_shader",			},
		{ SST_COMPUTE_SHADER,					SSP_COMPUTE_PIPELINE,		"compute_shader",			},
		{ SST_RAY_GENERATION_SHADER,			SSP_RAY_TRACING_PIPELINE,	"rgen_shader",				},
		{ SST_INTERSECTION_SHADER,				SSP_RAY_TRACING_PIPELINE,	"isect_shader",				},
		{ SST_ANY_HIT_SHADER,					SSP_RAY_TRACING_PIPELINE,	"ahit_shader",				},
		{ SST_CLOSEST_HIT_SHADER,				SSP_RAY_TRACING_PIPELINE,	"chit_shader",				},
		{ SST_MISS_SHADER,						SSP_RAY_TRACING_PIPELINE,	"miss_shader",				},
		{ SST_CALLABLE_SHADER,					SSP_RAY_TRACING_PIPELINE,	"call_shader",				},
	};

	const struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		std::string											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	const struct
	{
		VkIndexType								indexType;
		std::string								name;
	} indexFormats[] =
	{
		{ VK_INDEX_TYPE_NONE_KHR ,				"index_none"		},
		{ VK_INDEX_TYPE_UINT16 ,				"index_uint16"	},
		{ VK_INDEX_TYPE_UINT32 ,				"index_uint32"	},
	};

	const struct
	{
		EmptyAccelerationStructureCase	emptyASCase;
		std::string						name;
	} emptyCases[] =
	{
		{ EmptyAccelerationStructureCase::INACTIVE_TRIANGLES,	"inactive_triangles"	},
		{ EmptyAccelerationStructureCase::INACTIVE_INSTANCES,	"inactive_instances"	},
		{ EmptyAccelerationStructureCase::NO_GEOMETRIES_BOTTOM,	"no_geometries_bottom"	},
		{ EmptyAccelerationStructureCase::NO_PRIMITIVES_TOP,	"no_primitives_top"		},
		{ EmptyAccelerationStructureCase::NO_PRIMITIVES_BOTTOM,	"no_primitives_bottom"	},
	};

	auto& ctx = group->getTestContext();

	for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(new tcu::TestCaseGroup(ctx, shaderSourceTypes[shaderSourceNdx].name.c_str(), ""));

		for (int buildTypeIdx = 0; buildTypeIdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> buildTypeGroup(new tcu::TestCaseGroup(ctx, buildTypes[buildTypeIdx].name.c_str(), ""));

			for (int indexFormatIdx = 0; indexFormatIdx < DE_LENGTH_OF_ARRAY(indexFormats); ++indexFormatIdx)
			{
				de::MovePtr<tcu::TestCaseGroup> indexTypeGroup(new tcu::TestCaseGroup(ctx, indexFormats[indexFormatIdx].name.c_str(), ""));

				for (int emptyCaseIdx = 0; emptyCaseIdx < DE_LENGTH_OF_ARRAY(emptyCases); ++emptyCaseIdx)
				{
							TestParams testParams
							{
								shaderSourceTypes[shaderSourceNdx].shaderSourceType,
								shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
								buildTypes[buildTypeIdx].buildType,
								VK_FORMAT_R32G32B32_SFLOAT,
								false,
								indexFormats[indexFormatIdx].indexType,
								BTT_TRIANGLES,
								InstanceCullFlags::NONE,
								false,
								false,
								TTT_IDENTICAL_INSTANCES,
								false,
								false,
								VkBuildAccelerationStructureFlagsKHR(0u),
								OT_NONE,
								OP_NONE,
								TEST_WIDTH,
								TEST_HEIGHT,
								0u,
								emptyCases[emptyCaseIdx].emptyASCase,
							};
					indexTypeGroup->addChild(new RayQueryASBasicTestCase(ctx, emptyCases[emptyCaseIdx].name.c_str(), "", testParams));
				}
				buildTypeGroup->addChild(indexTypeGroup.release());
			}
			sourceTypeGroup->addChild(buildTypeGroup.release());
		}
		group->addChild(sourceTypeGroup.release());
	}
}

tcu::TestCaseGroup*	createAccelerationStructuresTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "acceleration_structures", "Acceleration structure tests using rayQuery feature"));

	addTestGroup(group.get(), "flags", "Test building AS with different build types, build flags and geometries/instances using arrays or arrays of pointers", addBasicBuildingTests);
	addTestGroup(group.get(), "format", "Test building AS with different vertex and index formats", addVertexIndexFormatsTests);
	addTestGroup(group.get(), "operations", "Test copying, compaction and serialization of AS", addOperationTests);
	addTestGroup(group.get(), "host_threading", "Test host threading operations", addHostThreadingOperationTests);
	addTestGroup(group.get(), "function_argument", "Test using AS as function argument using both pointers and bare values", addFuncArgTests);
	addTestGroup(group.get(), "instance_triangle_culling", "Test building AS with counterclockwise triangles and/or disabling face culling", addInstanceTriangleCullingTests);
	addTestGroup(group.get(), "dynamic_indexing", "Exercise dynamic indexing of acceleration structures", addDynamicIndexingTests);
	addTestGroup(group.get(), "empty", "Test building empty acceleration structures using different methods", addEmptyAccelerationStructureTests);

	return group.release();
}

}	// RayQuery

}	// vkt
