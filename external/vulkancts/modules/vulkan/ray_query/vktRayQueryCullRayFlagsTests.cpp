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
 * \brief Testing cull ray flags in ray query extension
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryCullRayFlagsTests.hpp"

#include <array>

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
#include "deRandom.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

#include "vkRayTracingUtil.hpp"

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
	STT_OPACITY						= 0,
	STT_TERMINATE_ON_FIRST_HIT		= 1,
	STT_FACE_CULLING				= 2,
	STT_SKIP_GEOMETRY				= 3,
};

enum RayFlags
{
	RF_None							= 0U,
	RF_Opaque						= 1U,
	RF_NoOpaque						= 2U,
	RF_TerminateOnFirstHit			= 4U,
	RF_SkipClosestHitShader			= 8U,
	RF_CullBackFacingTriangles		= 16U,
	RF_CullFrontFacingTriangles		= 32U,
	RF_CullOpaque					= 64U,
	RF_CullNoOpaque					= 128U,
	RF_SkipTriangles				= 256U,
	RF_SkipAABB						= 512U,
};

enum BottomTestType
{
	BTT_TRIANGLES	= 0,
	BTT_AABBS		= 1,
};

const deUint32			TEST_WIDTH			= 8;
const deUint32			TEST_HEIGHT			= 8;

struct TestParams;

std::string getRayFlagTestName(const RayFlags& flag)
{
	switch (flag)
	{
		case RF_None:						return std::string("none");
		case RF_Opaque:						return std::string("opaque");
		case RF_NoOpaque:					return std::string("noopaque");
		case RF_TerminateOnFirstHit:		return std::string("terminateonfirsthit");
		case RF_SkipClosestHitShader:		return std::string("skipclosesthitshader");
		case RF_CullBackFacingTriangles:	return std::string("cullbackfacingtriangles");
		case RF_CullFrontFacingTriangles:	return std::string("cullfrontfacingtriangles");
		case RF_CullOpaque:					return std::string("cullopaque");
		case RF_CullNoOpaque:				return std::string("cullnoopaque");
		case RF_SkipTriangles:				return std::string("skiptriangles");
		case RF_SkipAABB:					return std::string("skipaabb");
		default:							TCU_THROW(InternalError, "Wrong flag");
	}
	return std::string();
}

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
																 const VkDescriptorBufferInfo&	paramBufferDescriptorInfo,
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

struct TestParams
{
	deUint32							width;
	deUint32							height;
	ShaderSourceType					shaderSourceType;
	ShaderSourcePipeline				shaderSourcePipeline;
	ShaderTestType						shaderTestType;
	RayFlags							flag0;
	RayFlags							flag1;
	BottomTestType						bottomType;
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

std::vector<deUint32> getHitResult (const TestParams& testParams)
{
	deUint32 rayFlags				= testParams.flag0 | testParams.flag1;
	std::vector<deUint32> hitResult	= { 2,1,2,1 };
	switch (testParams.shaderTestType)
	{
		case STT_OPACITY:
		{
			if (rayFlags & RF_Opaque)		hitResult = { 2,2,2,2 };
			if (rayFlags & RF_NoOpaque)		hitResult = { 1,1,1,1 };
			if (rayFlags & RF_CullOpaque)	std::replace(std::begin(hitResult), std::end(hitResult), 2, 0);
			if (rayFlags & RF_CullNoOpaque)	std::replace(std::begin(hitResult), std::end(hitResult), 1, 0);
			break;
		}
		case STT_TERMINATE_ON_FIRST_HIT:
		{
			// all triangles should be hit
			break;
		}
		case STT_FACE_CULLING:
		{
			if (testParams.bottomType == BTT_AABBS)
				break;
			if (rayFlags & RF_CullBackFacingTriangles)	hitResult = { 2,1,0,0 };
			if (rayFlags & RF_CullFrontFacingTriangles)	hitResult = { 0,0,2,1 };
			break;
		}
		case STT_SKIP_GEOMETRY:
		{
			if (testParams.bottomType == BTT_TRIANGLES && rayFlags & RF_SkipTriangles)	hitResult = { 0,0,0,0 };
			if (testParams.bottomType == BTT_AABBS && rayFlags & RF_SkipAABB)			hitResult = { 0,0,0,0 };
			break;
		}
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
	}
	return hitResult;
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
																 const VkDescriptorBufferInfo&	paramBufferDescriptorInfo,
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
																										.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
																										.build(vkd, device);
	descriptorPool																				= DescriptorPoolBuilder()
																										.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																										.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																										.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
																										.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	descriptorSet																				= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	pipelineLayout																				= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	std::vector<std::string> rayQueryTestName;
	rayQueryTestName.push_back("rayflags_triangle");
	rayQueryTestName.push_back("rayflags_aabb");

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
			registerShaderModule(vkd,	device,	context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_VERTEX_BIT,						shaderNameIt->second[0],	rayQueryTestName[testParams.bottomType]);
	tescX = registerShaderModule(vkd,	device, context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		shaderNameIt->second[1],	rayQueryTestName[testParams.bottomType]);
	teseX = registerShaderModule(vkd,	device,	context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	shaderNameIt->second[2],	rayQueryTestName[testParams.bottomType]);
			registerShaderModule(vkd,	device,	context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_GEOMETRY_BIT,					shaderNameIt->second[3],	rayQueryTestName[testParams.bottomType]);
	fragX = registerShaderModule(vkd,	device,	context,	shaderModules,	shaderCreateInfos,	VK_SHADER_STAGE_FRAGMENT_BIT,					shaderNameIt->second[4],	rayQueryTestName[testParams.bottomType]);

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
	tcu::Vec3							v0					(2.0f, 2.0f, 0.0f);
	tcu::Vec3							v1					(float(testParams.width) - 2.0f, 2.0f, 0.0f);
	tcu::Vec3							v2					(2.0f, float(testParams.height) - 2.0f, 0.0f);
	tcu::Vec3							v3					(float(testParams.width) - 2.0f, float(testParams.height) - 2.0f, 0.0f);

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

void GraphicsConfiguration::fillCommandBuffer (Context&							context,
											   TestParams&						testParams,
											   VkCommandBuffer					commandBuffer,
											   const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
											   const VkDescriptorBufferInfo&	paramBufferDescriptorInfo,
											   const VkDescriptorImageInfo&		resultImageInfo)
{
	const DeviceInterface&				vkd									= context.getDeviceInterface();
	const VkDevice						device								= context.getDevice();

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &rayQueryAccelerationStructureWriteDescriptorSet)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &paramBufferDescriptorInfo)
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
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 2, resultBuffer->getAllocation().getHostPtr());

	// create reference image
	std::vector<deUint32>		reference(testParams.width * testParams.height * 2);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 2, reference.data());

	// 4 squares have characteristics: (front, opaque), (front, no_opaque), (back, opaque), (back, no_opaque)
	// First we calculate test results for each square
	std::vector<deUint32> hitResult = getHitResult(testParams);

	std::vector<std::vector<tcu::UVec2>> squares =
	{
		{ {1,1}, {4,4} },
		{ {4,1}, {7,4} },
		{ {1,4}, {4,7} },
		{ {4,4}, {7,7} },
	};
	std::vector<std::vector<deUint32>> primitives =
	{
		{0, 1, 2},
		{1, 3, 2}
	};

	tcu::UVec4 missValue(0, 0, 0, 0);
	tcu::UVec4 clearValue(0xFF, 0, 0, 0);

	switch (testParams.shaderSourceType)
	{
		case SST_VERTEX_SHADER:
			tcu::clear(referenceAccess, clearValue);
			for (deUint32 vNdx = 0; vNdx < 4; ++vNdx)
			{
				tcu::UVec4 hitValue(hitResult[vNdx], 0, 0, 0);
				referenceAccess.setPixel(hitValue, vNdx, 0, 0);
				referenceAccess.setPixel(hitValue, vNdx, 0, 1);
			}
			break;
		case SST_TESSELATION_CONTROL_SHADER:
		case SST_TESSELATION_EVALUATION_SHADER:
		case SST_GEOMETRY_SHADER:
			tcu::clear(referenceAccess, clearValue);
			for (deUint32 primitiveNdx = 0; primitiveNdx < primitives.size(); ++primitiveNdx)
			for (deUint32 vertexNdx = 0; vertexNdx < 3; ++vertexNdx)
			{
				deUint32 vNdx = primitives[primitiveNdx][vertexNdx];
				tcu::UVec4 hitValue(hitResult[vNdx], 0, 0, 0);
				referenceAccess.setPixel(hitValue, primitiveNdx, vertexNdx, 0);
				referenceAccess.setPixel(hitValue, primitiveNdx, vertexNdx, 1);
			}
			break;
		case SST_FRAGMENT_SHADER:
			tcu::clear(referenceAccess, missValue);
			for (deUint32 squareNdx = 0; squareNdx < squares.size(); ++squareNdx)
			{
				tcu::UVec4 hitValue(hitResult[squareNdx], 0, 0, 0);
				for (deUint32 y = squares[squareNdx][0].y(); y < squares[squareNdx][1].y(); ++y)
				for (deUint32 x = squares[squareNdx][0].x(); x < squares[squareNdx][1].x(); ++x)
				{
					referenceAccess.setPixel(hitValue, x, y, 0);
					referenceAccess.setPixel(hitValue, x, y, 1);
				}
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
																 const VkDescriptorBufferInfo&	paramBufferDescriptorInfo,
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
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
																					.build(vkd, device);
	descriptorPool															= DescriptorPoolBuilder()
																					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																					.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
																					.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	descriptorSet															= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	pipelineLayout															= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	std::vector<std::string> rayQueryTestName;
	rayQueryTestName.push_back("comp_rayflags_triangle");
	rayQueryTestName.push_back("comp_rayflags_aabb");

	shaderModule															= createShaderModule(vkd, device, context.getBinaryCollection().get(rayQueryTestName[testParams.bottomType]), 0u);
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

void ComputeConfiguration::fillCommandBuffer (Context&							context,
											  TestParams&						testParams,
											  VkCommandBuffer					commandBuffer,
											  const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
											  const VkDescriptorBufferInfo&		paramBufferDescriptorInfo,
											  const VkDescriptorImageInfo&		resultImageInfo)
{
	const DeviceInterface&				vkd									= context.getDeviceInterface();
	const VkDevice						device								= context.getDevice();

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &rayQueryAccelerationStructureWriteDescriptorSet)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &paramBufferDescriptorInfo)
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
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 2, resultBuffer->getAllocation().getHostPtr());

	// create reference image
	std::vector<deUint32>		reference(testParams.width * testParams.height * 2);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 2, reference.data());

	// 4 squares have characteristics: (front, opaque), (front, no_opaque), (back, opaque), (back, no_opaque)
	// First we calculate test results for each square
	std::vector<deUint32> hitResult = getHitResult(testParams);

	std::vector<std::vector<tcu::UVec2>> squares =
	{
		{ {1,1}, {4,4} },
		{ {4,1}, {7,4} },
		{ {1,4}, {4,7} },
		{ {4,4}, {7,7} },
	};

	tcu::UVec4 missValue(0, 0, 0, 0);
	tcu::clear(referenceAccess, missValue);

	for (deUint32 squareNdx = 0; squareNdx < squares.size(); ++squareNdx)
	{
		tcu::UVec4 hitValue(hitResult[squareNdx], 0, 0, 0);
		for (deUint32 y = squares[squareNdx][0].y(); y < squares[squareNdx][1].y(); ++y)
		for (deUint32 x = squares[squareNdx][0].x(); x < squares[squareNdx][1].x(); ++x)
		{
			referenceAccess.setPixel(hitValue, x, y, 0);
			referenceAccess.setPixel(hitValue, x, y, 1);
		}
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
																 const VkDescriptorBufferInfo&	paramBufferDescriptorInfo,
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
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ALL_RAY_TRACING_STAGES)
																					.build(vkd, device);
	descriptorPool															= DescriptorPoolBuilder()
																					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																					.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
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
	rayQueryTestName.push_back("rayflags_triangle");
	rayQueryTestName.push_back("rayflags_aabb");

	auto shaderNameIt = shaderNames.find(testParams.shaderSourceType);
	if(shaderNameIt == end(shaderNames))
		TCU_THROW(InternalError, "Wrong shader source type");

	bool rgenX, isectX, ahitX, chitX, missX, callX;
	rgenX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_RAYGEN_BIT_KHR,			shaderNameIt->second[0],	rayQueryTestName[testParams.bottomType],	0);
	if (testParams.shaderSourceType == SST_INTERSECTION_SHADER)
		isectX = registerShaderModule(vkd, device, context,		*rayTracingPipeline,	VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	shaderNameIt->second[1],	rayQueryTestName[testParams.bottomType],	1);
	else
		isectX = false;
	ahitX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		shaderNameIt->second[2],	rayQueryTestName[testParams.bottomType],	1);
	chitX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	shaderNameIt->second[3],	rayQueryTestName[testParams.bottomType],	1);
	missX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_MISS_BIT_KHR,			shaderNameIt->second[4],	rayQueryTestName[testParams.bottomType],	2);
	callX = registerShaderModule(vkd,	device,	context,		*rayTracingPipeline,	VK_SHADER_STAGE_CALLABLE_BIT_KHR,		shaderNameIt->second[5],	rayQueryTestName[testParams.bottomType],	3);
	bool hitX = isectX || ahitX || chitX;

	rtPipeline																= rayTracingPipeline->createPipeline(vkd, device, *pipelineLayout);

	deUint32							shaderGroupHandleSize				= getShaderGroupHandleSize(vki, physicalDevice);
	deUint32							shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);

	if (rgenX)	raygenShaderBindingTable									= rayTracingPipeline->createShaderBindingTable(vkd, device, *rtPipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
	if (hitX)	hitShaderBindingTable										= rayTracingPipeline->createShaderBindingTable(vkd, device, *rtPipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
	if (missX)	missShaderBindingTable										= rayTracingPipeline->createShaderBindingTable(vkd, device, *rtPipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
	if (callX)	callableShaderBindingTable									= rayTracingPipeline->createShaderBindingTable(vkd, device, *rtPipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, 1);
}

void RayTracingConfiguration::fillCommandBuffer (Context&							context,
												 TestParams&						testParams,
												 VkCommandBuffer					commandBuffer,
												 const VkWriteDescriptorSetAccelerationStructureKHR&	rayQueryAccelerationStructureWriteDescriptorSet,
												 const VkDescriptorBufferInfo&		paramBufferDescriptorInfo,
												 const VkDescriptorImageInfo&		resultImageInfo)
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
			tcu::Vec3	v0			(0.0f, 0.0f, 0.0f);
			tcu::Vec3	v1			(float(testParams.width), 0.0f, 0.0f);
			tcu::Vec3	v2			(0.0f, float(testParams.height), 0.0f);
			tcu::Vec3	v3			(float(testParams.width), float(testParams.height), 0.0f);
			tcu::Vec3	missOffset	(0.0f, 0.0f, 0.0f);
			if(testParams.shaderSourceType == SST_MISS_SHADER)
				missOffset	= tcu::Vec3(1.0f+ float(testParams.width), 0.0f, 0.0f);

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
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(3u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &paramBufferDescriptorInfo)
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
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 2, resultBuffer->getAllocation().getHostPtr());

	// create reference image
	std::vector<deUint32>		reference(testParams.width * testParams.height * 2);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 2, reference.data());

	// 4 squares have characteristics: (front, opaque), (front, no_opaque), (back, opaque), (back, no_opaque)
	// First we calculate test results for each square
	std::vector<deUint32> hitResult = getHitResult(testParams);

	std::vector<std::vector<tcu::UVec2>> squares =
	{
		{ {1,1}, {4,4} },
		{ {4,1}, {7,4} },
		{ {1,4}, {4,7} },
		{ {4,4}, {7,7} },
	};

	tcu::UVec4 missValue(0, 0, 0, 0);
	tcu::clear(referenceAccess, missValue);

	for (deUint32 squareNdx = 0; squareNdx < squares.size(); ++squareNdx)
	{
		tcu::UVec4 hitValue(hitResult[squareNdx], 0, 0, 0);
		for (deUint32 y = squares[squareNdx][0].y(); y < squares[squareNdx][1].y(); ++y)
		for (deUint32 x = squares[squareNdx][0].x(); x < squares[squareNdx][1].x(); ++x)
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

class RayQueryCullRayFlagsTestCase : public TestCase
{
	public:
							RayQueryCullRayFlagsTestCase			(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
							~RayQueryCullRayFlagsTestCase			(void);

	virtual void			checkSupport								(Context& context) const;
	virtual	void			initPrograms								(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance								(Context& context) const;
private:
	TestParams				m_data;
};

class TraversalControlTestInstance : public TestInstance
{
public:
																	TraversalControlTestInstance	(Context& context, const TestParams& data);
																	~TraversalControlTestInstance	(void);
	tcu::TestStatus													iterate									(void);

private:
	TestParams														m_data;
};

RayQueryCullRayFlagsTestCase::RayQueryCullRayFlagsTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayQueryCullRayFlagsTestCase::~RayQueryCullRayFlagsTestCase (void)
{
}

void RayQueryCullRayFlagsTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_query");

	const VkPhysicalDeviceRayQueryFeaturesKHR&	rayQueryFeaturesKHR								= context.getRayQueryFeatures();
	if (rayQueryFeaturesKHR.rayQuery == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	const VkPhysicalDeviceFeatures2&			features2										= context.getDeviceFeatures2();

	if ((m_data.shaderSourceType == SST_TESSELATION_CONTROL_SHADER ||
		 m_data.shaderSourceType == SST_TESSELATION_EVALUATION_SHADER) &&
		features2.features.tessellationShader == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFeatures2.tessellationShader");

	if (m_data.shaderSourceType == SST_GEOMETRY_SHADER &&
		features2.features.geometryShader == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFeatures2.geometryShader");

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

	if ( m_data.shaderSourceType == SST_RAY_GENERATION_SHADER ||
		 m_data.shaderSourceType == SST_INTERSECTION_SHADER ||
		 m_data.shaderSourceType == SST_ANY_HIT_SHADER ||
		 m_data.shaderSourceType == SST_CLOSEST_HIT_SHADER ||
		 m_data.shaderSourceType == SST_MISS_SHADER ||
		 m_data.shaderSourceType == SST_CALLABLE_SHADER)
	{
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

		const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();

		if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
			TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
	}
}

void RayQueryCullRayFlagsTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	// create parts of programs responsible for test execution
	std::vector<std::string> rayQueryTest;
	std::vector<std::string> rayQueryTestName;
	rayQueryTestName.push_back("rayflags_triangle");
	rayQueryTestName.push_back("rayflags_aabb");

	{
		// All of the tests use the same shader for triangles
		std::stringstream css;
		css <<
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  rayQueryEXT rq;\n"
			"  rayQueryInitializeEXT(rq, rqTopLevelAS, rqFlags, 0xFF, origin, tmin, direct, tmax);\n"
			"  if(rayQueryProceedEXT(rq))\n"
			"  {\n"
			"    if (rayQueryGetRayFlagsEXT(rq) == rqFlags)\n"
			"    {\n"
			"      if (rayQueryGetIntersectionTypeEXT(rq, false)==gl_RayQueryCandidateIntersectionTriangleEXT)"
			"      {\n"
			"        hitValue.x = 1;\n"
			"        hitValue.y = 1;\n"
			"      }\n"
			"    }\n"
			"  }\n"
			"  else\n"
			"  {\n"
			"    if (rayQueryGetRayFlagsEXT(rq) == rqFlags)\n"
			"    {\n"
			"      if (rayQueryGetIntersectionTypeEXT(rq, true)==gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"      {\n"
			"        hitValue.x = 2;\n"
			"        hitValue.y = 2;\n"
			"      }\n"
			"    }\n"
			"  }\n";
		rayQueryTest.push_back(css.str());
	}

	{
		// All of the tests use the same shader for aabbss
		std::stringstream css;
		css <<
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  rayQueryEXT rq;\n"
			"  rayQueryInitializeEXT(rq, rqTopLevelAS, rqFlags, 0xFF, origin, tmin, direct, tmax);\n"
			"  if(rayQueryProceedEXT(rq))\n"
			"  {\n"
			"    if (rayQueryGetRayFlagsEXT(rq) == rqFlags)\n"
			"    {\n"
			"      if (rayQueryGetIntersectionTypeEXT(rq, false)==gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"      {\n"
			"        if(rayQueryGetIntersectionCandidateAABBOpaqueEXT(rq))\n"
			"        {\n"
			"          hitValue.x = 2;\n"
			"          hitValue.y = 2;\n"
			"        }\n"
			"        else\n"
			"        {\n"
			"          hitValue.x = 1;\n"
			"          hitValue.y = 1;\n"
			"        }\n"
			"      }\n"
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
			programCollection.glslSources.add("vert") << glu::VertexSource(css.str());
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
			programCollection.glslSources.add("vert_vid") << glu::VertexSource(css.str());
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout (location = 0) in vec3 position;\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"layout(set = 0, binding = 2) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  vec3  origin   = vec3(float(position.x), float(position.y), 0.5f);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n"
				"  uint  rqFlags  = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"  imageStore(result, ivec3(gl_VertexIndex, 0, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_VertexIndex, 0, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"  gl_Position = vec4(position,1);\n"
				"}\n";
			std::stringstream cssName;
			cssName << "vert_" << rayQueryTestName[m_data.bottomType];

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
			programCollection.glslSources.add("tesc") << glu::TessellationControlSource(css.str());
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"layout(set = 0, binding = 2) uniform params { uvec4 rayFlags; };\n"
				"in gl_PerVertex {\n"
				"  vec4  gl_Position;\n"
				"} gl_in[];\n"
				"layout(vertices = 3) out;\n"
				"void main (void)\n"
				"{\n"
				"  vec3  origin   = vec3(gl_in[gl_InvocationID].gl_Position.x, gl_in[gl_InvocationID].gl_Position.y, 0.5f);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n"
				"  uint  rqFlags  = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"  imageStore(result, ivec3(gl_PrimitiveID, gl_InvocationID, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_PrimitiveID, gl_InvocationID, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"  gl_TessLevelInner[0] = 1;\n"
				"  gl_TessLevelOuter[0] = 1;\n"
				"  gl_TessLevelOuter[1] = 1;\n"
				"  gl_TessLevelOuter[2] = 1;\n"
				"}\n";
			std::stringstream cssName;
			cssName << "tesc_" << rayQueryTestName[m_data.bottomType];

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
				"layout(set = 0, binding = 2) uniform params { uvec4 rayFlags; };\n"
				"void main (void)\n"
				"{\n"
				"  for (int i = 0; i < 3; ++i)\n"
				"  {\n"
				"    vec3  origin   = vec3(gl_in[i].gl_Position.x, gl_in[i].gl_Position.y, 0.5f);\n"
				"    uvec4 hitValue = uvec4(0,0,0,0);\n"
				"    uint  rqFlags  = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"    imageStore(result, ivec3(gl_PrimitiveID, i, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"    imageStore(result, ivec3(gl_PrimitiveID, i, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"  }\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"}\n";
			std::stringstream cssName;
			cssName << "tese_" << rayQueryTestName[m_data.bottomType];

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

			programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(css.str());
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
				"layout(set = 0, binding = 2) uniform params { uvec4 rayFlags; };\n"
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
				"    vec3  origin   = vec3(gl_in[i].gl_Position.x, gl_in[i].gl_Position.y, 0.5f);\n"
				"    uvec4 hitValue = uvec4(0,0,0,0);\n"
				"    uint  rqFlags  = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"    imageStore(result, ivec3(gl_PrimitiveIDIn, j, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"    imageStore(result, ivec3(gl_PrimitiveIDIn, j, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"    gl_Position      = gl_in[i].gl_Position;\n"
				"    EmitVertex();\n"
				"  }\n"
				"  EndPrimitive();\n"
				"}\n";
			std::stringstream cssName;
			cssName << "geom_" << rayQueryTestName[m_data.bottomType];

			programCollection.glslSources.add(cssName.str()) << glu::GeometrySource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"layout(set = 0, binding = 2) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  vec3  origin   = vec3(gl_FragCoord.x, gl_FragCoord.y, 0.5f);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n"
				"  uint  rqFlags  = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"  imageStore(result, ivec3(gl_FragCoord.xy-vec2(0.5,0.5), 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_FragCoord.xy-vec2(0.5,0.5), 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"}\n";
			std::stringstream cssName;
			cssName << "frag_" << rayQueryTestName[m_data.bottomType];

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
				"layout(set = 0, binding = 2) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  vec3  origin   = vec3(float(gl_GlobalInvocationID.x) + 0.5f, float(gl_GlobalInvocationID.y) + 0.5f, 0.5f);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n"
				"  uint  rqFlags  = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"  imageStore(result, ivec3(gl_GlobalInvocationID.xy, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_GlobalInvocationID.xy, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"}\n";
			std::stringstream cssName;
			cssName << "comp_" << rayQueryTestName[m_data.bottomType];

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
				"  vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 0.5f);\n"
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
				"layout(set = 0, binding = 3) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 0.5f);\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n"
				"  uint  rqFlags  = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 0), uvec4(hitValue.x, 0, 0, 0));\n"
				"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 1), uvec4(hitValue.y, 0, 0, 0));\n"
				"}\n";
			std::stringstream cssName;
			cssName << "rgen_" << rayQueryTestName[m_data.bottomType];

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
				"  uint  rqFlags;\n"
				"};\n"
				"layout(location = 0) callableDataEXT CallValue param;\n"
				"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
				"layout(set = 0, binding = 3) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  param.origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 0.5f);\n"
				"  param.hitValue = uvec4(0, 0, 0, 0);\n"
				"  param.rqFlags  = rayFlags.x;\n"
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
				"layout(set = 0, binding = 3) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  vec3 origin   = gl_WorldRayOriginEXT;\n"
				"  hitValue      = uvec4(0,0,0,0);\n"
				"  uint rqFlags  = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"  reportIntersectionEXT(0.5f, 0);\n"
				"}\n";
			std::stringstream cssName;
			cssName << "isect_" << rayQueryTestName[m_data.bottomType];

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
				"layout(set = 0, binding = 3) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  vec3 origin  = gl_WorldRayOriginEXT;\n"
				"  uint rqFlags = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"}\n";
			std::stringstream cssName;
			cssName << "ahit_" << rayQueryTestName[m_data.bottomType];

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
				"layout(set = 0, binding = 3) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  vec3 origin  = gl_WorldRayOriginEXT;\n"
				"  uint rqFlags = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"}\n";
			std::stringstream cssName;
			cssName << "chit_" << rayQueryTestName[m_data.bottomType];

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
				"layout(set = 0, binding = 3) uniform params { uvec4 rayFlags; };\n"
				"void main()\n"
				"{\n"
				"  vec3 origin  = gl_WorldRayOriginEXT;\n"
				"  uint rqFlags = rayFlags.x;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"}\n";
			std::stringstream cssName;
			cssName << "miss_" << rayQueryTestName[m_data.bottomType];

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
				"  uint  rqFlags;\n"
				"};\n"
				"layout(location = 0) callableDataInEXT CallValue result;\n"
				"layout(set = 0, binding = 2) uniform accelerationStructureEXT rqTopLevelAS;\n"
				"void main()\n"
				"{\n"
				"  vec3  origin   = result.origin;\n"
				"  uvec4 hitValue = uvec4(0,0,0,0);\n"
				"  uint  rqFlags  = result.rqFlags;\n" <<
				rayQueryTest[m_data.bottomType] <<
				"  result.hitValue = hitValue;\n"
				"}\n";
			std::stringstream cssName;
			cssName << "call_" << rayQueryTestName[m_data.bottomType];

			programCollection.glslSources.add(cssName.str()) << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
		}
	}
}

TestInstance* RayQueryCullRayFlagsTestCase::createInstance (Context& context) const
{
	return new TraversalControlTestInstance(context, m_data);
}

TraversalControlTestInstance::TraversalControlTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

TraversalControlTestInstance::~TraversalControlTestInstance (void)
{
}

tcu::TestStatus TraversalControlTestInstance::iterate (void)
{
	de::SharedPtr<TestConfiguration> testConfiguration;

	switch (m_data.shaderSourcePipeline)
	{
	case SSP_GRAPHICS_PIPELINE:
		testConfiguration = de::SharedPtr<TestConfiguration>(new GraphicsConfiguration());
		break;
	case SSP_COMPUTE_PIPELINE:
		testConfiguration = de::SharedPtr<TestConfiguration>(new ComputeConfiguration());
		break;
	case SSP_RAY_TRACING_PIPELINE:
		testConfiguration = de::SharedPtr<TestConfiguration>(new RayTracingConfiguration());
		break;
	default:
		TCU_THROW(InternalError, "Wrong shader source pipeline");
	}

	testConfiguration->initConfiguration(m_context, m_data);

	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();

	const VkFormat						imageFormat							= testConfiguration->getResultImageFormat();
	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, 2, imageFormat);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, imageFormat, imageSubresourceRange);

	const VkBufferCreateInfo			paramBufferCreateInfo				= makeBufferCreateInfo(sizeof(tcu::UVec4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>		paramBuffer							= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, paramBufferCreateInfo, MemoryRequirement::HostVisible));
	tcu::UVec4							paramData							(m_data.flag0 | m_data.flag1, 0, 0, 0);
	deMemcpy(paramBuffer->getAllocation().getHostPtr(), &paramData, sizeof(tcu::UVec4));
	flushAlloc(vkd, device, paramBuffer->getAllocation());

	const VkDescriptorBufferInfo		paramBufferDescriptorInfo			= makeDescriptorBufferInfo(paramBuffer->get(), paramBuffer->getAllocation().getOffset(), sizeof(tcu::UVec4));

	const VkBufferCreateInfo			resultBufferCreateInfo				= makeBufferCreateInfo(m_data.width * m_data.height * 2 * testConfiguration->getResultImageFormatSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				resultBufferImageRegion				= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 2), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		resultBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			resultImageInfo						= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	rayQueryBottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>						rayQueryTopLevelAccelerationStructure;

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

		rayQueryTopLevelAccelerationStructure = makeTopLevelAccelerationStructure();
		// In case of triangle AS consists of 4 squares:
		// - left squares are marked as opaque, right squares - as nonopaque
		// - higher squares are front facing, lower - back facing
		// In case of AABBs - it's just 2 rectangles ( no face culling idea in AABBs )
		// - left rectangle is marked as opaque, right rectangle - as nonopaque
		{
			tcu::Vec3						v[3][3];
			for (deUint32 y = 0; y < 3; ++y)
			for (deUint32 x = 0; x < 3; ++x)
				v[x][y] = tcu::Vec3(1.0f + 0.5f * ( float(m_data.width) - 2.0f ) * float(x), 1.0f + 0.5f * ( float(m_data.height) - 2.0f ) * float(y), 0.0f );
			VkTransformMatrixKHR			identityMatrix = { { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } } };


			if (m_data.bottomType == BTT_TRIANGLES)
			{
				// offsets taking front facing into account
				std::vector<std::vector<tcu::UVec2>> faceCullingOffsets = {
					{ {0,0}, {1,0}, {0,1}, {0,1}, {1,0}, {1,1} },
					{ {0,0}, {0,1}, {1,0}, {1,0}, {0,1}, {1,1} },
				};

				rayQueryTopLevelAccelerationStructure->setInstanceCount(4);

				for (deUint32 y = 0; y < 2; ++y)
				for (deUint32 x = 0; x < 2; ++x)
				{
					de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
					bottomLevelAccelerationStructure->setGeometryCount(1);
					de::SharedPtr<RaytracedGeometryBase>			geometry							= makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);

					deUint32										faceCullingNdx						= y % 2;
					VkGeometryInstanceFlagsKHR						instanceFlags						= (x % 2) ? VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR : VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;

					for (size_t vNdx = 0; vNdx < faceCullingOffsets[faceCullingNdx].size(); vNdx++)
					{
						tcu::UVec2& off = faceCullingOffsets[faceCullingNdx][vNdx];
						geometry->addVertex( v[x+off.x()][y+off.y()] );
					}
					bottomLevelAccelerationStructure->addGeometry(geometry);

					rayQueryBottomLevelAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
					rayQueryBottomLevelAccelerationStructures.back()->createAndBuild(vkd, device, *cmdBuffer, allocator);

					rayQueryTopLevelAccelerationStructure->addInstance(rayQueryBottomLevelAccelerationStructures.back(), identityMatrix, 0u, 0xFF, 0u, instanceFlags);
				}

			}
			else // testParams.bottomType != BTT_TRIANGLES
			{
				std::vector<std::vector<tcu::Vec3>> aabbCoords = {
					{ v[0][0] + tcu::Vec3(0.0f, 0.0f, -0.1f), v[1][2] + tcu::Vec3(0.0f, 0.0f, 0.1f) },
					{ v[1][0] + tcu::Vec3(0.0f, 0.0f, -0.1f), v[2][2] + tcu::Vec3(0.0f, 0.0f, 0.1f) },
				};

				rayQueryTopLevelAccelerationStructure->setInstanceCount(aabbCoords.size());

				for (size_t aNdx = 0; aNdx < aabbCoords.size(); aNdx++)
				{
					de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
					bottomLevelAccelerationStructure->setGeometryCount(1);
					de::SharedPtr<RaytracedGeometryBase>			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);

					VkGeometryInstanceFlagsKHR						instanceFlags = (aNdx % 2) ? VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR : VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;

					geometry->addVertex(aabbCoords[aNdx][0]);
					geometry->addVertex(aabbCoords[aNdx][1]);

					bottomLevelAccelerationStructure->addGeometry(geometry);

					rayQueryBottomLevelAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
					rayQueryBottomLevelAccelerationStructures.back()->createAndBuild(vkd, device, *cmdBuffer, allocator);

					rayQueryTopLevelAccelerationStructure->addInstance(rayQueryBottomLevelAccelerationStructures.back(), identityMatrix, 0u, 0xFF, 0u, instanceFlags);
				}
			}
		}

		rayQueryTopLevelAccelerationStructure->createAndBuild(vkd, device, *cmdBuffer, allocator);

		const TopLevelAccelerationStructure*			rayQueryTopLevelAccelerationStructurePtr	= rayQueryTopLevelAccelerationStructure.get();
		VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet		=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			1u,																	//  deUint32							accelerationStructureCount;
			rayQueryTopLevelAccelerationStructurePtr->getPtr(),					//  const VkAccelerationStructureKHR*	pAccelerationStructures;
		};

		testConfiguration->fillCommandBuffer(m_context, m_data, *cmdBuffer, accelerationStructureWriteDescriptorSet, paramBufferDescriptorInfo, resultImageInfo);

		const VkMemoryBarrier							postTestMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		const VkMemoryBarrier							postCopyMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTestMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	bool result = testConfiguration->verifyImage(resultBuffer.get(), m_context, m_data);

	if (!result)
		return tcu::TestStatus::fail("Fail");
	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

tcu::TestCaseGroup*	createCullRayFlagsTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "ray_flags", "Tests verifying ray flags in ray query extension"));

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

	struct ShaderTestTypeData
	{
		ShaderTestType									shaderTestType;
		const char*										name;
		std::vector<std::vector<RayFlags>>				flag; // bottom test type, flag0, flag1
	} shaderTestTypes[] =
	{
		{ STT_OPACITY,					"opacity", {
			{ RF_None, RF_Opaque, RF_NoOpaque, RF_CullOpaque, RF_CullNoOpaque },
			{ RF_None, RF_Opaque, RF_NoOpaque, RF_CullOpaque, RF_CullNoOpaque },
		} },
		{ STT_TERMINATE_ON_FIRST_HIT,	"terminate_on_first_hit", {
			{ RF_TerminateOnFirstHit },
			{ RF_TerminateOnFirstHit },
		} },
		{ STT_FACE_CULLING,			"face_culling", {
			{ RF_CullBackFacingTriangles, RF_CullFrontFacingTriangles },
			{  },
		} },
		{ STT_SKIP_GEOMETRY,			"skip_geometry", {
			{ RF_SkipTriangles, RF_SkipAABB },
			{ RF_SkipTriangles, RF_SkipAABB },
		} },
	};

	struct
	{
		BottomTestType							testType;
		const char*								name;
	} bottomTestTypes[] =
	{
		{ BTT_TRIANGLES,						"triangles" },
		{ BTT_AABBS,							"aabbs" },
	};

	for (size_t shaderSourceNdx = 0; shaderSourceNdx < DE_LENGTH_OF_ARRAY(shaderSourceTypes); ++shaderSourceNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> sourceTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderSourceTypes[shaderSourceNdx].name, ""));

		for (size_t shaderTestNdx = 0; shaderTestNdx < DE_LENGTH_OF_ARRAY(shaderTestTypes); ++shaderTestNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> testTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderTestTypes[shaderTestNdx].name, ""));

			for (size_t testTypeNdx = 0; testTypeNdx < shaderTestTypes[shaderTestNdx].flag.size(); ++testTypeNdx)
			{
				de::MovePtr<tcu::TestCaseGroup> bottomTestTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), bottomTestTypes[testTypeNdx].name, ""));

				const auto& flags = shaderTestTypes[shaderTestNdx].flag[testTypeNdx];

				for (size_t flagNdx = 0; flagNdx < flags.size(); ++flagNdx)
				{
					std::stringstream testName;
					testName << getRayFlagTestName(flags[flagNdx]);

					TestParams testParams
					{
						TEST_WIDTH,
						TEST_HEIGHT,
						shaderSourceTypes[shaderSourceNdx].shaderSourceType,
						shaderSourceTypes[shaderSourceNdx].shaderSourcePipeline,
						shaderTestTypes[shaderTestNdx].shaderTestType,
						flags[flagNdx],
						RF_None,
						bottomTestTypes[testTypeNdx].testType
					};
					bottomTestTypeGroup->addChild(new RayQueryCullRayFlagsTestCase(group->getTestContext(), testName.str().c_str(), "", testParams));
				}

				std::vector<tcu::TestNode*> tests;
				bottomTestTypeGroup->getChildren(tests);
				if(!tests.empty())
					testTypeGroup->addChild(bottomTestTypeGroup.release());
			}
			sourceTypeGroup->addChild(testTypeGroup.release());
		}
		group->addChild(sourceTypeGroup.release());
	}

	return group.release();
}

}	// RayQuery

}	// vkt
