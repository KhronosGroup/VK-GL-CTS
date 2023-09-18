/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Vulkan Transform Feedback Simple Tests
 *//*--------------------------------------------------------------------*/

#include "vktTransformFeedbackSimpleTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include <iostream>
#include <functional>
#include <set>
#include <algorithm>
#include <limits>
#include <memory>
#include <map>

namespace vkt
{
namespace TransformFeedback
{
namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using de::SharedPtr;

#define VALIDATE_MINIMUM(A,B) if ((A) < (B)) TCU_FAIL(#A "==" + de::toString(A) + " which is less than required by specification (" + de::toString(B) + ")")
#define VALIDATE_BOOL(A) if (! ( (A) == VK_TRUE || (A) == VK_FALSE) ) TCU_FAIL(#A " expected to be VK_TRUE or VK_FALSE. Received " + de::toString((deUint64)(A)))

const deUint32				INVOCATION_COUNT	= 8u;
const std::vector<deUint32>	LINES_LIST			{ 2, 6, 3 };
const std::vector<deUint32>	TRIANGLES_LIST		{ 3, 8, 6, 5, 4 };

enum TestType
{
	TEST_TYPE_BASIC,
	TEST_TYPE_RESUME,
	TEST_TYPE_STREAMS,
	TEST_TYPE_XFB_POINTSIZE,
	TEST_TYPE_XFB_CLIPDISTANCE,
	TEST_TYPE_XFB_CULLDISTANCE,
	TEST_TYPE_XFB_CLIP_AND_CULL,
	TEST_TYPE_WINDING,
	TEST_TYPE_STREAMS_POINTSIZE,
	TEST_TYPE_STREAMS_CLIPDISTANCE,
	TEST_TYPE_STREAMS_CULLDISTANCE,
	TEST_TYPE_MULTISTREAMS,
	TEST_TYPE_MULTISTREAMS_SAME_LOCATION,
	TEST_TYPE_DRAW_INDIRECT,
	TEST_TYPE_DRAW_INDIRECT_MULTIVIEW,
	TEST_TYPE_BACKWARD_DEPENDENCY,
	TEST_TYPE_BACKWARD_DEPENDENCY_INDIRECT,
	TEST_TYPE_QUERY_GET,
	TEST_TYPE_QUERY_COPY,
	TEST_TYPE_QUERY_COPY_STRIDE_ZERO,
	TEST_TYPE_QUERY_RESET,
	TEST_TYPE_MULTIQUERY,
	TEST_TYPE_DEPTH_CLIP_CONTROL_VERTEX,
	TEST_TYPE_DEPTH_CLIP_CONTROL_GEOMETRY,
	TEST_TYPE_DEPTH_CLIP_CONTROL_TESE,
	TEST_TYPE_LINES_TRIANGLES,
	TEST_TYPE_DRAW_OUTSIDE,
	TEST_TYPE_HOLES_VERTEX,
	TEST_TYPE_HOLES_GEOMETRY,
	TEST_TYPE_LAST
};

enum StreamId0Mode
{
	STREAM_ID_0_NORMAL					= 0,
	STREAM_ID_0_BEGIN_QUERY_INDEXED		= 1,
	STREAM_ID_0_END_QUERY_INDEXED		= 2,
};

struct TestParameters
{
	const PipelineConstructionType pipelineConstructionType;

	TestType			testType;
	deUint32			bufferSize;
	deUint32			partCount;
	deUint32			streamId;
	deUint32			pointSize;
	deUint32			vertexStride;
	StreamId0Mode		streamId0Mode;
	bool				query64bits;
	bool				noOffsetArray;
	bool				requireRastStreamSelect;
	bool				omitShaderWrite;
	bool				useMaintenance5;
	VkPrimitiveTopology	primTopology;
	bool				queryResultWithAvailability;

	bool isPoints (void) const
	{
		return (primTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	}

	bool usingTess (void) const
	{
		return (primTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
	}

	bool requiresFullPipeline (void) const
	{
		return (testType == TEST_TYPE_STREAMS
				|| testType == TEST_TYPE_STREAMS_POINTSIZE
				|| testType == TEST_TYPE_STREAMS_CULLDISTANCE
				|| testType == TEST_TYPE_STREAMS_CLIPDISTANCE
				|| (testType == TEST_TYPE_WINDING && primTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST));
	}

	bool usingGeom (void) const
	{
		static const std::set<TestType> nonFullPipelineTestTypesWithGeomShaders
		{
			TEST_TYPE_DEPTH_CLIP_CONTROL_GEOMETRY,
			TEST_TYPE_MULTISTREAMS,
			TEST_TYPE_MULTISTREAMS_SAME_LOCATION,
			TEST_TYPE_QUERY_GET,
			TEST_TYPE_QUERY_COPY,
			TEST_TYPE_QUERY_COPY_STRIDE_ZERO,
			TEST_TYPE_QUERY_RESET,
			TEST_TYPE_MULTIQUERY,
			TEST_TYPE_LINES_TRIANGLES,
		};

		const auto itr = nonFullPipelineTestTypesWithGeomShaders.find(testType);
		return (itr != nonFullPipelineTestTypesWithGeomShaders.end() || requiresFullPipeline());
	}

	bool usingTessGeom (void) const
	{
		return (usingTess() || usingGeom());
	}

	// Returns true if we want to set PointSize in some shaders. Note some test types always need/want PointSize, independently of
	// this value, as it's in the nature of the test.
	bool pointSizeWanted (void) const
	{
		return (pointSize > 0u);
	}
};

// Device helper: this is needed in some tests when we create custom devices.
class DeviceHelper
{
public:
	virtual ~DeviceHelper () {}
	virtual const DeviceInterface&	getDeviceInterface	(void) const = 0;
	virtual VkDevice				getDevice			(void) const = 0;
	virtual uint32_t				getQueueFamilyIndex	(void) const = 0;
	virtual VkQueue					getQueue			(void) const = 0;
	virtual Allocator&				getAllocator		(void) const = 0;
};

// This one just reuses the default device from the context.
class ContextDeviceHelper : public DeviceHelper
{
public:
	ContextDeviceHelper (Context& context)
		: m_deviceInterface		(context.getDeviceInterface())
		, m_device				(context.getDevice())
		, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
		, m_queue				(context.getUniversalQueue())
		, m_allocator			(context.getDefaultAllocator())
		{}

	virtual ~ContextDeviceHelper () {}

	const DeviceInterface&	getDeviceInterface	(void) const override	{ return m_deviceInterface;		}
	VkDevice				getDevice			(void) const override	{ return m_device;				}
	uint32_t				getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	VkQueue					getQueue			(void) const override	{ return m_queue;				}
	Allocator&				getAllocator		(void) const override	{ return m_allocator;			}

protected:
	const DeviceInterface&	m_deviceInterface;
	const VkDevice			m_device;
	const uint32_t			m_queueFamilyIndex;
	const VkQueue			m_queue;
	Allocator&				m_allocator;
};

class NoShaderTessellationAndGeometryPointSizeDeviceHelper : public DeviceHelper
{
public:
	// Forbid copy and assignment.
	NoShaderTessellationAndGeometryPointSizeDeviceHelper (const DeviceHelper&) = delete;
	NoShaderTessellationAndGeometryPointSizeDeviceHelper& operator= (const DeviceHelper& other) = delete;

	NoShaderTessellationAndGeometryPointSizeDeviceHelper (Context& context)
	{
		const auto&	vkp					= context.getPlatformInterface();
		const auto&	vki					= context.getInstanceInterface();
		const auto	instance			= context.getInstance();
		const auto	physicalDevice		= context.getPhysicalDevice();

		m_queueFamilyIndex = context.getUniversalQueueFamilyIndex();

		// Get device features (these have to be checked in checkSupport).
		VkPhysicalDeviceFeatures2								features2			= initVulkanStructure();
		VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT		gplFeatures			= initVulkanStructure();
		VkPhysicalDeviceTransformFeedbackFeaturesEXT			xfbFeatures			= initVulkanStructure();
		VkPhysicalDeviceMultiviewFeatures						multiviewFeatures	= initVulkanStructure();
		VkPhysicalDeviceHostQueryResetFeatures					hostQueryResetFeat	= initVulkanStructure();

		const auto addFeatures = makeStructChainAdder(&features2);
		addFeatures(&xfbFeatures);
		if (context.isDeviceFunctionalitySupported("VK_EXT_graphics_pipeline_library"))
			addFeatures(&gplFeatures);
		if (context.isDeviceFunctionalitySupported("VK_KHR_multiview"))
			addFeatures(&multiviewFeatures);
		if (context.isDeviceFunctionalitySupported("VK_EXT_host_query_reset"))
			addFeatures(&hostQueryResetFeat);

		vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

		features2.features.robustBufferAccess						= VK_FALSE;	// Disable robustness.
		features2.features.shaderTessellationAndGeometryPointSize	= VK_FALSE;	// Disable shaderTessellationAndGeometryPointSize.

		const auto queuePriority = 1.0f;
		const VkDeviceQueueCreateInfo queueInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,			//	VkStructureType					sType;
			nullptr,											//	const void*						pNext;
			0u,													//	VkDeviceQueueCreateFlags		flags;
			m_queueFamilyIndex,									//	deUint32						queueFamilyIndex;
			1u,													//	deUint32						queueCount;
			&queuePriority,										//	const float*					pQueuePriorities;
		};

		const auto creationExtensions = context.getDeviceCreationExtensions();

		const VkDeviceCreateInfo createInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	//	VkStructureType					sType;
			&features2,								//	const void*						pNext;
			0u,										//	VkDeviceCreateFlags				flags;
			1u,										//	deUint32						queueCreateInfoCount;
			&queueInfo,								//	const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
			0u,										//	deUint32						enabledLayerCount;
			nullptr,								//	const char* const*				ppEnabledLayerNames;
			de::sizeU32(creationExtensions),		//	deUint32						enabledExtensionCount;
			de::dataOrNull(creationExtensions),		//	const char* const*				ppEnabledExtensionNames;
			nullptr,								//	const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		// Create custom device and related objects
		const auto enableValidation = context.getTestContext().getCommandLine().isValidationEnabled();

		m_device	= createCustomDevice(enableValidation, vkp, instance, vki, physicalDevice, &createInfo);
		m_vkd		.reset(new DeviceDriver(vkp, instance, *m_device, context.getUsedApiVersion()));
		m_queue		= getDeviceQueue(*m_vkd, *m_device, m_queueFamilyIndex, 0u);
		m_allocator	.reset(new SimpleAllocator(*m_vkd, *m_device, getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}

	virtual ~NoShaderTessellationAndGeometryPointSizeDeviceHelper () {}

	const vk::DeviceInterface&	getDeviceInterface	(void) const override	{ return *m_vkd;				}
	vk::VkDevice				getDevice			(void) const override	{ return m_device.get();		}
	uint32_t					getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	vk::VkQueue					getQueue			(void) const override	{ return m_queue;				}
	vk::Allocator&				getAllocator		(void) const override	{ return *m_allocator;			}

protected:
	vk::Move<vk::VkDevice>					m_device;
	std::unique_ptr<vk::DeviceDriver>		m_vkd;
	deUint32								m_queueFamilyIndex;
	vk::VkQueue								m_queue;
	std::unique_ptr<vk::SimpleAllocator>	m_allocator;
};

std::unique_ptr<DeviceHelper> g_noShaderTessellationAndGeometryPointSizeHelper;
std::unique_ptr<DeviceHelper> g_contextDeviceHelper;

DeviceHelper& getDeviceHelper (Context& context, const TestParameters& parameters)
{
	const bool isPoints			= parameters.isPoints();
	const bool pointSizeWanted	= parameters.pointSizeWanted();
	const bool usingTessGeom	= parameters.usingTessGeom();
	const bool featureAvailable	= context.getDeviceFeatures().shaderTessellationAndGeometryPointSize;

	if (isPoints && !pointSizeWanted && usingTessGeom && featureAvailable)
	{
		// We can run these tests, but we must use a custom device with no shaderTessellationAndGeometryPointSize.
		if (!g_noShaderTessellationAndGeometryPointSizeHelper)
			g_noShaderTessellationAndGeometryPointSizeHelper.reset(new NoShaderTessellationAndGeometryPointSizeDeviceHelper(context));
		return *g_noShaderTessellationAndGeometryPointSizeHelper;
	}

	// The default device works otherwise.
	if (!g_contextDeviceHelper)
		g_contextDeviceHelper.reset(new ContextDeviceHelper(context));
	return *g_contextDeviceHelper;
}

void cleanupDevices()
{
	g_noShaderTessellationAndGeometryPointSizeHelper.reset(nullptr);
	g_contextDeviceHelper.reset(nullptr);
}

struct TopologyInfo
{
	deUint32							primSize;			// The size of the on primitive.
	std::string							topologyName;		// The suffix for the name of test.
	std::function<deUint64(deUint64)>	getNumPrimitives;	// The number of primitives generated.
	std::function<deUint64(deUint64)>	getNumVertices;		// The number of vertices generated.
};

const std::map<VkPrimitiveTopology, TopologyInfo> topologyData =
{
	{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST						, { 1, ""								,[](deUint64 vertexCount)	{	return vertexCount;				}	,[](deUint64 primCount)	{	return primCount;			}, } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST						, { 2, "line_list_"						,[](deUint64 vertexCount)	{	return vertexCount / 2u;		}	,[](deUint64 primCount) {	return primCount * 2u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						, { 2, "line_strip_"					,[](deUint64 vertexCount)	{	return vertexCount - 1u;		}	,[](deUint64 primCount) {	return primCount + 1u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					, { 3, "triangle_list_"					,[](deUint64 vertexCount)	{	return vertexCount / 3u;		}	,[](deUint64 primCount) {	return primCount * 3u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					, { 3, "triangle_strip_"				,[](deUint64 vertexCount)	{	return vertexCount - 2u;		}	,[](deUint64 primCount) {	return primCount + 2u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN					, { 3, "triangle_fan_"					,[](deUint64 vertexCount)	{	return vertexCount - 2u;		}	,[](deUint64 primCount) {	return primCount + 2u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY		, { 2, "line_list_with_adjacency_"		,[](deUint64 vertexCount)	{	return vertexCount / 4u;		}	,[](deUint64 primCount) {	return primCount * 4u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		, { 2, "line_strip_with_adjacency_"		,[](deUint64 vertexCount)	{	return vertexCount - 3u;		}	,[](deUint64 primCount) {	return primCount + 3u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY	, { 3, "triangle_list_with_adjacency_"	,[](deUint64 vertexCount)	{	return vertexCount / 6u;		}	,[](deUint64 primCount) {	return primCount * 6u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	, { 3, "triangle_strip_with_adjacency_"	,[](deUint64 vertexCount)	{	return (vertexCount - 4u) / 2u;	}	,[](deUint64 primCount) {	return primCount * 2u + 4u;	}, } },
	{ VK_PRIMITIVE_TOPOLOGY_PATCH_LIST						, { 3, "patch_list_"					,[](deUint64 vertexCount)	{	return vertexCount / 3u;		}	,[](deUint64 primCount) {	return primCount * 3u;		}, } },
};

struct TransformFeedbackQuery
{
	deUint32	written;
	deUint32	attempts;
};

const deUint32	MINIMUM_TF_BUFFER_SIZE	= (1<<27);
const deUint32	IMAGE_SIZE				= 64u;

template<typename T>
inline SharedPtr<Unique<T> > makeSharedPtr(Move<T> move)
{
	return SharedPtr<Unique<T> >(new Unique<T>(move));
}

template<typename T>
const T* getInvalidatedHostPtr (const DeviceInterface& vk, const VkDevice device, Allocation& bufAlloc)
{
	invalidateAlloc(vk, device, bufAlloc);

	return static_cast<T*>(bufAlloc.getHostPtr());
}

using PipelineLayoutWrapperPtr = std::unique_ptr<PipelineLayoutWrapper>;

PipelineLayoutWrapperPtr makePipelineLayout (PipelineConstructionType	pipelineConstructionType,
											 const DeviceInterface&		vk,
											 const VkDevice				device,
											 const uint32_t				pcSize = sizeof(uint32_t))
{
	const VkPushConstantRange			pushConstantRanges			=
	{
		VK_SHADER_STAGE_VERTEX_BIT,						//  VkShaderStageFlags				stageFlags;
		0u,												//  deUint32						offset;
		pcSize,											//  deUint32						size;
	};

	const VkPipelineLayoutCreateInfo	pipelineLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//  VkStructureType					sType;
		DE_NULL,										//  const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,					//  VkPipelineLayoutCreateFlags		flags;
		0u,												//  deUint32						setLayoutCount;
		DE_NULL,										//  const VkDescriptorSetLayout*	pSetLayouts;
		1u,												//  deUint32						pushConstantRangeCount;
		&pushConstantRanges,							//  const VkPushConstantRange*		pPushConstantRanges;
	};

	PipelineLayoutWrapperPtr pipelineLayoutWrapper(new PipelineLayoutWrapper(pipelineConstructionType, vk, device, &pipelineLayoutCreateInfo));
	return pipelineLayoutWrapper;
}

using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;

GraphicsPipelineWrapperPtr makeGraphicsPipeline (const PipelineConstructionType	pipelineConstructionType,
												 const InstanceInterface&		vki,
												 const DeviceInterface&			vk,
												 const VkPhysicalDevice			physicalDevice,
												 const VkDevice					device,
												 const std::vector<std::string>&deviceExtensions,
												 const PipelineLayoutWrapper&	pipelineLayout,
												 const VkRenderPass				renderPass,
												 const ShaderWrapper&			vertexModule,
												 const ShaderWrapper&			tessellationControlModule,
												 const ShaderWrapper&			tessellationEvalModule,
												 const ShaderWrapper&			geometryModule,
												 const ShaderWrapper&			fragmentModule,
												 const VkExtent2D				renderSize,
												 const deUint32					subpass,
												 const deUint32*				rasterizationStreamPtr	= DE_NULL,
												 const VkPrimitiveTopology		topology				= VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
												 const bool						inputVertices			= false,
												 const bool						depthClipControl		= false,
												 const uint32_t					attachmentCount			= 0u)
{
	const std::vector<VkViewport>	viewports	(1u, makeViewport(renderSize));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(renderSize));

	const VkPipelineViewportDepthClipControlCreateInfoEXT	depthClipControlCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT,	// VkStructureType	sType;
		DE_NULL,																// const void*		pNext;
		VK_TRUE,																// VkBool32		negativeOneToOne;
	};

	const void* pipelineViewportStatePNext = (depthClipControl ? &depthClipControlCreateInfo : nullptr);

	const VkPipelineVertexInputStateCreateInfo				vertexInputStateCreateInfo			= initVulkanStructure();
	const VkPipelineVertexInputStateCreateInfo*				vertexInputStateCreateInfoPtr		= (inputVertices ? nullptr : &vertexInputStateCreateInfo);
	const VkBool32											disableRasterization				= (fragmentModule.getModule() == VK_NULL_HANDLE);
	const deUint32											rasterizationStream					= ((!rasterizationStreamPtr) ? 0u : *rasterizationStreamPtr);

	const VkPipelineRasterizationStateStreamCreateInfoEXT	rasterizationStateStreamCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,						//  VkStructureType										sType;
		DE_NULL,																					//  const void*											pNext;
		0,																							//  VkPipelineRasterizationStateStreamCreateFlagsEXT	flags;
		rasterizationStream																			//  deUint32											rasterizationStream;
	};

	const VkPipelineRasterizationStateCreateInfo			rasterizationStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//  VkStructureType							sType
		&rasterizationStateStreamCreateInfo,						//  const void*								pNext
		0u,															//  VkPipelineRasterizationStateCreateFlags	flags
		VK_FALSE,													//  VkBool32								depthClampEnable
		disableRasterization,										//  VkBool32								rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										//  VkPolygonMode							polygonMode
		VK_CULL_MODE_NONE,											//  VkCullModeFlags							cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							//  VkFrontFace								frontFace
		VK_FALSE,													//  VkBool32								depthBiasEnable
		0.0f,														//  float									depthBiasConstantFactor
		0.0f,														//  float									depthBiasClamp
		0.0f,														//  float									depthBiasSlopeFactor
		1.0f														//  float									lineWidth
	};

	const VkPipelineRasterizationStateCreateInfo*			rasterizationStateCreateInfoPtr		= ((!rasterizationStreamPtr) ? nullptr : &rasterizationStateCreateInfo);
	const VkPipelineColorBlendAttachmentState				defaultAttachmentState				=
	{
		VK_FALSE,						//	VkBool32				blendEnable;
		VK_BLEND_FACTOR_ZERO,			//	VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,			//	VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,				//	VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ZERO,			//	VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,			//	VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,				//	VkBlendOp				alphaBlendOp;
		(VK_COLOR_COMPONENT_R_BIT		//	VkColorComponentFlags	colorWriteMask;
		|VK_COLOR_COMPONENT_G_BIT
		|VK_COLOR_COMPONENT_B_BIT
		|VK_COLOR_COMPONENT_A_BIT),
	};
	const std::vector<VkPipelineColorBlendAttachmentState>	attachmentStates					(attachmentCount, defaultAttachmentState);
	const VkPipelineColorBlendStateCreateInfo				colorBlendStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,													//	const void*									pNext;
		0u,															//	VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,													//	VkBool32									logicOpEnable;
		VK_LOGIC_OP_CLEAR,											//	VkLogicOp									logicOp;
		de::sizeU32(attachmentStates),								//	uint32_t									attachmentCount;
		de::dataOrNull(attachmentStates),							//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									//	float										blendConstants[4];
	};

	GraphicsPipelineWrapperPtr	pipelineWrapperPtr	(new GraphicsPipelineWrapper(vki, vk, physicalDevice, device, deviceExtensions, pipelineConstructionType));
	auto&						pipelineWrapper		= *pipelineWrapperPtr;

	pipelineWrapper
		.setMonolithicPipelineLayout(pipelineLayout)
		.setDefaultDepthStencilState()
		.setDefaultMultisampleState()
		.setDefaultPatchControlPoints(3u)
		.setDefaultTopology(topology)
		.setDefaultRasterizationState()
		.setDefaultRasterizerDiscardEnable(disableRasterization)
		.setViewportStatePnext(pipelineViewportStatePNext)
		.setupVertexInputState(vertexInputStateCreateInfoPtr)
		.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass, subpass, vertexModule, rasterizationStateCreateInfoPtr, tessellationControlModule, tessellationEvalModule, geometryModule)
		.setupFragmentShaderState(pipelineLayout, renderPass, subpass, fragmentModule)
		.setupFragmentOutputState(renderPass, subpass, &colorBlendStateCreateInfo)
		.buildPipeline();

	return pipelineWrapperPtr;
}

VkImageCreateInfo makeImageCreateInfo (const VkImageCreateFlags flags, const VkImageType type, const VkFormat format, const VkExtent2D size, const deUint32 numLayers, const VkImageUsageFlags usage)
{
	const VkExtent3D		extent		= { size.width, size.height, 1u };
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		flags,											// VkImageCreateFlags		flags;
		type,											// VkImageType				imageType;
		format,											// VkFormat					format;
		extent,											// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		numLayers,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return imageParams;
}

Move<VkRenderPass> makeCustomRenderPass (const DeviceInterface&		vk,
										 const VkDevice				device,
										 const VkFormat				format = VK_FORMAT_UNDEFINED)
{
	std::vector<VkSubpassDescription>	subpassDescriptions;
	std::vector<VkSubpassDependency>	subpassDependencies;
	const bool							hasColorAtt				= (format != VK_FORMAT_UNDEFINED);

	std::vector<VkAttachmentDescription>	attachmentDescs;
	std::vector<VkAttachmentReference>		attachmentRefs;

	if (hasColorAtt)
	{
		attachmentDescs.push_back(makeAttachmentDescription(
			0u,
			format,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
		attachmentRefs.push_back(makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	}

	const VkSubpassDescription	description	=
	{
		(VkSubpassDescriptionFlags)0,		//  VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	//  VkPipelineBindPoint				pipelineBindPoint;
		0u,									//  deUint32						inputAttachmentCount;
		nullptr,							//  const VkAttachmentReference*	pInputAttachments;
		de::sizeU32(attachmentRefs),		//  deUint32						colorAttachmentCount;
		de::dataOrNull(attachmentRefs),		//  const VkAttachmentReference*	pColorAttachments;
		nullptr,							//  const VkAttachmentReference*	pResolveAttachments;
		nullptr,							//  const VkAttachmentReference*	pDepthStencilAttachment;
		0u,									//  deUint32						preserveAttachmentCount;
		nullptr,							//  const deUint32*					pPreserveAttachments;
	};
	subpassDescriptions.push_back(description);

	const VkSubpassDependency	dependency	=
	{
		0u,													//  deUint32				srcSubpass;
		0u,													//  deUint32				dstSubpass;
		VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,		//  VkPipelineStageFlags	srcStageMask;
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,				//  VkPipelineStageFlags	dstStageMask;
		VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,	//  VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,	//  VkAccessFlags			dstAccessMask;
		0u													//  VkDependencyFlags		dependencyFlags;
	};
	subpassDependencies.push_back(dependency);

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			//  VkStructureType					sType;
		nullptr,											//  const void*						pNext;
		static_cast<VkRenderPassCreateFlags>(0u),			//  VkRenderPassCreateFlags			flags;
		de::sizeU32(attachmentDescs),						//  deUint32						attachmentCount;
		de::dataOrNull(attachmentDescs),					//  const VkAttachmentDescription*	pAttachments;
		de::sizeU32(subpassDescriptions),					//  deUint32						subpassCount;
		de::dataOrNull(subpassDescriptions),				//  const VkSubpassDescription*		pSubpasses;
		de::sizeU32(subpassDependencies),					//  deUint32						dependencyCount;
		de::dataOrNull(subpassDependencies),				//  const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

VkImageMemoryBarrier makeImageMemoryBarrier	(const VkAccessFlags			srcAccessMask,
											 const VkAccessFlags			dstAccessMask,
											 const VkImageLayout			oldLayout,
											 const VkImageLayout			newLayout,
											 const VkImage					image,
											 const VkImageSubresourceRange	subresourceRange)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		srcAccessMask,									// VkAccessFlags			outputMask;
		dstAccessMask,									// VkAccessFlags			inputMask;
		oldLayout,										// VkImageLayout			oldLayout;
		newLayout,										// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return barrier;
}

VkBufferMemoryBarrier makeBufferMemoryBarrier (const VkAccessFlags	srcAccessMask,
											   const VkAccessFlags	dstAccessMask,
											   const VkBuffer		buffer,
											   const VkDeviceSize	offset,
											   const VkDeviceSize	bufferSizeBytes)
{
	const VkBufferMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
		DE_NULL,									//  const void*		pNext;
		srcAccessMask,								//  VkAccessFlags	srcAccessMask;
		dstAccessMask,								//  VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
		buffer,										//  VkBuffer		buffer;
		offset,										//  VkDeviceSize	offset;
		bufferSizeBytes,							//  VkDeviceSize	size;
	};
	return barrier;
}

VkMemoryBarrier makeMemoryBarrier (const VkAccessFlags	srcAccessMask,
								   const VkAccessFlags	dstAccessMask)
{
	const VkMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		srcAccessMask,						// VkAccessFlags			outputMask;
		dstAccessMask,						// VkAccessFlags			inputMask;
	};
	return barrier;
}

VkQueryPoolCreateInfo makeQueryPoolCreateInfo (const deUint32 queryCountersNumber)
{
	const VkQueryPoolCreateInfo			queryPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,		//  VkStructureType					sType;
		DE_NULL,										//  const void*						pNext;
		(VkQueryPoolCreateFlags)0,						//  VkQueryPoolCreateFlags			flags;
		VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,	//  VkQueryType						queryType;
		queryCountersNumber,							//  deUint32						queryCount;
		0u,												//  VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	return queryPoolCreateInfo;
}

void fillBuffer (const DeviceInterface& vk, const VkDevice device, Allocation& bufferAlloc, VkDeviceSize bufferSize, const void* data, const VkDeviceSize dataSize)
{
	const VkMappedMemoryRange	memRange		=
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//  VkStructureType	sType;
		DE_NULL,								//  const void*		pNext;
		bufferAlloc.getMemory(),				//  VkDeviceMemory	memory;
		bufferAlloc.getOffset(),				//  VkDeviceSize	offset;
		VK_WHOLE_SIZE							//  VkDeviceSize	size;
	};
	std::vector<deUint8>		dataVec			(static_cast<deUint32>(bufferSize), 0u);

	DE_ASSERT(bufferSize >= dataSize);

	deMemcpy(&dataVec[0], data, static_cast<deUint32>(dataSize));

	deMemcpy(bufferAlloc.getHostPtr(), &dataVec[0], dataVec.size());
	VK_CHECK(vk.flushMappedMemoryRanges(device, 1u, &memRange));
}

deUint32 destripedLineCount (const std::vector<deUint32>& lineStripeSizesList)
{
	deUint32 result = 0;

	DE_ASSERT(!lineStripeSizesList.empty());

	for (auto x : lineStripeSizesList)
		result += x > 1 ? x - 1 : 0;

	return result;
}

deUint32 destripedTriangleCount (const std::vector<deUint32>& triangleStripeSizesList)
{
	deUint32 result = 0;

	DE_ASSERT(!triangleStripeSizesList.empty());

	for (auto x : triangleStripeSizesList)
		result += x > 2 ? x - 2 : 0;

	return result;
}

class TransformFeedbackTestInstance : public TestInstance
{
public:
													TransformFeedbackTestInstance	(Context& context, const TestParameters& parameters);
protected:
	void											validateLimits					();
	std::vector<VkDeviceSize>						generateSizesList				(const size_t bufBytes, const size_t chunkCount);
	std::vector<VkDeviceSize>						generateOffsetsList				(const std::vector<VkDeviceSize>& sizesList);
	void											verifyTransformFeedbackBuffer	(const DeviceHelper& deviceHelper,
																					 const MovePtr<Allocation>& bufAlloc,
																					 const deUint32 bufBytes);

	const VkExtent2D								m_imageExtent2D;
	const TestParameters							m_parameters;
	VkPhysicalDeviceTransformFeedbackPropertiesEXT	m_transformFeedbackProperties;
	de::Random										m_rnd;
};

TransformFeedbackTestInstance::TransformFeedbackTestInstance (Context& context, const TestParameters& parameters)
	: TestInstance		(context)
	, m_imageExtent2D	(makeExtent2D(IMAGE_SIZE, IMAGE_SIZE))
	, m_parameters		(parameters)
	, m_rnd				(context.getTestContext().getCommandLine().getBaseSeed())
{
	VkPhysicalDeviceProperties2 deviceProperties2;

	deMemset(&deviceProperties2, 0, sizeof(deviceProperties2));
	deMemset(&m_transformFeedbackProperties, 0, sizeof(m_transformFeedbackProperties));

	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &m_transformFeedbackProperties;

	m_transformFeedbackProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
	m_transformFeedbackProperties.pNext = DE_NULL;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &deviceProperties2);

	validateLimits();
}

void TransformFeedbackTestInstance::validateLimits ()
{
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackBuffers, 1);
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackBufferSize, MINIMUM_TF_BUFFER_SIZE);
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackStreamDataSize, 512);
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize, 512);
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride, 512);

	VALIDATE_BOOL(m_transformFeedbackProperties.transformFeedbackQueries);
	VALIDATE_BOOL(m_transformFeedbackProperties.transformFeedbackStreamsLinesTriangles);
	VALIDATE_BOOL(m_transformFeedbackProperties.transformFeedbackRasterizationStreamSelect);
	VALIDATE_BOOL(m_transformFeedbackProperties.transformFeedbackDraw);
}

std::vector<VkDeviceSize> TransformFeedbackTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const int					minChunkSlot	= static_cast<int>(1);
	const int					maxChunkSlot	= static_cast<int>(bufBytes / sizeof(deUint32));
	int							prevOffsetSlot	= 0;
	std::map<int, bool>			offsetsSet;
	std::vector<VkDeviceSize>	result;

	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(minChunkSlot <= maxChunkSlot);
	DE_ASSERT(chunkCount > 0);
	// To be effective this algorithm requires that chunkCount is much less than amount of chunks possible
	DE_ASSERT(8 * chunkCount <= static_cast<size_t>(maxChunkSlot));

	offsetsSet[0] = true;

	// Create a list of unique offsets first
	for (size_t chunkNdx = 1; chunkNdx < chunkCount; ++chunkNdx)
	{
		int chunkSlot;

		do
		{
			chunkSlot = m_rnd.getInt(minChunkSlot, maxChunkSlot - 1);
		} while (offsetsSet.find(chunkSlot) != offsetsSet.end());

		offsetsSet[chunkSlot] = true;
	}
	offsetsSet[maxChunkSlot] = true;

	// Calculate sizes of offsets list
	result.reserve(chunkCount);
	for (std::map<int, bool>::iterator mapIt = offsetsSet.begin(); mapIt != offsetsSet.end(); ++mapIt)
	{
		const int offsetSlot = mapIt->first;

		if (offsetSlot == 0)
			continue;

		DE_ASSERT(prevOffsetSlot < offsetSlot && offsetSlot > 0);

		result.push_back(static_cast<VkDeviceSize>(static_cast<size_t>(offsetSlot - prevOffsetSlot) * sizeof(deUint32)));

		prevOffsetSlot = offsetSlot;
	}

	DE_ASSERT(result.size() == chunkCount);

	return result;
}

std::vector<VkDeviceSize> TransformFeedbackTestInstance::generateOffsetsList (const std::vector<VkDeviceSize>& sizesList)
{
	VkDeviceSize				offset	= 0ull;
	std::vector<VkDeviceSize>	result;

	result.reserve(sizesList.size());

	for (size_t chunkNdx = 0; chunkNdx < sizesList.size(); ++chunkNdx)
	{
		result.push_back(offset);

		offset += sizesList[chunkNdx];
	}

	DE_ASSERT(sizesList.size() == result.size());

	return result;
}

void TransformFeedbackTestInstance::verifyTransformFeedbackBuffer (const DeviceHelper&			deviceHelper,
																   const MovePtr<Allocation>&	bufAlloc,
																   const deUint32				bufBytes)
{
	const DeviceInterface&	vk			= deviceHelper.getDeviceInterface();
	const VkDevice			device		= deviceHelper.getDevice();
	const deUint32			numPoints	= static_cast<deUint32>(bufBytes / sizeof(deUint32));
	const deUint32*			tfData		= getInvalidatedHostPtr<deUint32>(vk, device, *bufAlloc);

	for (deUint32 i = 0; i < numPoints; ++i)
		if (tfData[i] != i)
			TCU_FAIL(std::string("Failed at item ") + de::toString(i) + " received:" + de::toString(tfData[i]) + " expected:" + de::toString(i));
}

class TransformFeedbackBasicTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackBasicTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate								(void);
};

TransformFeedbackBasicTestInstance::TransformFeedbackBasicTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
}

tcu::TestStatus TransformFeedbackBasicTestInstance::iterate (void)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&							vki						= m_context.getInstanceInterface();
	const auto							physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue					= deviceHelper.getQueue();
	Allocator&							allocator				= deviceHelper.getAllocator();

	const ShaderWrapper					vertexModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					nullModule;
	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout			(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule, nullModule, nullModule, nullModule, nullModule, m_imageExtent2D, 0u, &m_parameters.streamId));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			for (deUint32 drawNdx = 0; drawNdx < m_parameters.partCount; ++drawNdx)
			{
				const deUint32	startValue	= static_cast<deUint32>(tfBufBindingOffsets[drawNdx] / sizeof(deUint32));
				const deUint32	numPoints	= static_cast<deUint32>(tfBufBindingSizes[drawNdx] / sizeof(deUint32));

				vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffsets[drawNdx], &tfBufBindingSizes[drawNdx]);

				vk.cmdPushConstants(*cmdBuffer, pipelineLayout->get(), VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				{
					vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			}
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackResumeTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackResumeTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate								(void);
};

TransformFeedbackResumeTestInstance::TransformFeedbackResumeTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
}

tcu::TestStatus TransformFeedbackResumeTestInstance::iterate (void)
{
	const auto&								deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&								vki						= m_context.getInstanceInterface();
	const auto								physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&					vk						= deviceHelper.getDeviceInterface();
	const VkDevice							device					= deviceHelper.getDevice();
	const deUint32							queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue							queue					= deviceHelper.getQueue();
	Allocator&								allocator				= deviceHelper.getAllocator();

	const ShaderWrapper						vertexModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper						kNullModule;
	const Unique<VkRenderPass>				renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>				framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto								pipelineLayout			(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto								pipeline				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule, kNullModule, kNullModule, kNullModule, kNullModule, m_imageExtent2D, 0u, &m_parameters.streamId));

	const Unique<VkCommandPool>				cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	VkBufferCreateInfo						tfBufCreateInfo			= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);

#ifndef CTS_USES_VULKANSC
	vk::VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();
	if (m_parameters.useMaintenance5)
	{
		bufferUsageFlags2.usage = (VkBufferUsageFlagBits2KHR)tfBufCreateInfo.usage;
		tfBufCreateInfo.pNext = &bufferUsageFlags2;
		tfBufCreateInfo.usage = 0;
	}
#endif // CTS_USES_VULKANSC

	const Move<VkBuffer>					tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>				tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier					tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>			tfBufBindingSizes		= std::vector<VkDeviceSize>(1, m_parameters.bufferSize);
	const std::vector<VkDeviceSize>			tfBufBindingOffsets		= std::vector<VkDeviceSize>(1, 0ull);

	const size_t							tfcBufSize				= 16 * sizeof(deUint32) * m_parameters.partCount;
	VkBufferCreateInfo						tfcBufCreateInfo		= makeBufferCreateInfo(tfcBufSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT);

#ifndef CTS_USES_VULKANSC
	if (m_parameters.useMaintenance5)
	{
		bufferUsageFlags2.usage = (VkBufferUsageFlagBits2KHR)tfcBufCreateInfo.usage;
		tfcBufCreateInfo.pNext = &bufferUsageFlags2;
		tfcBufCreateInfo.usage = 0;
	}
#endif // CTS_USES_VULKANSC

	const Move<VkBuffer>					tfcBuf					= createBuffer(vk, device, &tfcBufCreateInfo);
	const MovePtr<Allocation>				tfcBufAllocation		= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfcBuf), MemoryRequirement::Any);
	const std::vector<VkDeviceSize>			tfcBufBindingOffsets	= generateOffsetsList(generateSizesList(tfcBufSize, m_parameters.partCount));
	const VkBufferMemoryBarrier				tfcBufBarrier			= makeBufferMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT, VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT, *tfcBuf, 0ull, VK_WHOLE_SIZE);

	const std::vector<VkDeviceSize>			chunkSizesList			= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>			chunkOffsetsList		= generateOffsetsList(chunkSizesList);

	DE_ASSERT(tfBufBindingSizes.size() == 1);
	DE_ASSERT(tfBufBindingOffsets.size() == 1);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));
	VK_CHECK(vk.bindBufferMemory(device, *tfcBuf, tfcBufAllocation->getMemory(), tfcBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		for (size_t drawNdx = 0; drawNdx < m_parameters.partCount; ++drawNdx)
		{
			const deUint32	startValue = static_cast<deUint32>(chunkOffsetsList[drawNdx] / sizeof(deUint32));
			const deUint32	numPoints = static_cast<deUint32>(chunkSizesList[drawNdx] / sizeof(deUint32));
			const deUint32	countBuffersCount = (drawNdx == 0) ? 0 : 1;

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
			{

				vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

				vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

				vk.cmdPushConstants(*cmdBuffer, pipelineLayout->get(), VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, countBuffersCount, (drawNdx == 0) ? DE_NULL : &*tfcBuf, (drawNdx == 0) ? DE_NULL : &tfcBufBindingOffsets[drawNdx - 1]);
				{
					vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 1, &*tfcBuf, &tfcBufBindingOffsets[drawNdx]);
			}
			endRenderPass(vk, *cmdBuffer);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 0u, DE_NULL, 1u, &tfcBufBarrier, 0u, DE_NULL);
		}

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackWindingOrderTestInstance : public TransformFeedbackTestInstance
{
public:
	TransformFeedbackWindingOrderTestInstance(Context& context, const TestParameters& parameters);

protected:
	struct TopologyParameters
	{
		// number of vertex in primitive; 2 for line, 3 for triangle
		deUint32 vertexPerPrimitive;

		// pointer to function calculating number of points that
		// will be generated for given part count
		std::function<deUint32(deUint32)> getNumGeneratedPoints;

		// pointer to function generating expected values; parameter is
		// primitive index, result array with expected data for primitive vertex
		std::function<std::vector<deUint32>(deUint32)> getExpectedValuesForPrimitive;
	};
	typedef const std::map<VkPrimitiveTopology, TopologyParameters> TopologyParametersMap;

protected:
	const TopologyParametersMap&	getTopologyParametersMap					(void);
	tcu::TestStatus					iterate										(void);
	void							verifyTransformFeedbackBuffer				(const DeviceHelper& deviceHelper,
																				 const MovePtr<Allocation>& bufAlloc,
																				 const deUint32 bufBytes);

private:
	TopologyParameters				m_tParameters;
	const bool						m_requiresTesselationStage;
};

TransformFeedbackWindingOrderTestInstance::TransformFeedbackWindingOrderTestInstance(Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
	, m_requiresTesselationStage(parameters.primTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
{
	if (m_requiresTesselationStage && !context.getDeviceFeatures().tessellationShader)
		throw tcu::NotSupportedError("Tessellation shader not supported");

	TopologyParametersMap topologyParametersMap = getTopologyParametersMap();
	DE_ASSERT(topologyParametersMap.find(parameters.primTopology) != topologyParametersMap.end());
	m_tParameters = topologyParametersMap.at(parameters.primTopology);
}

const TransformFeedbackWindingOrderTestInstance::TopologyParametersMap& TransformFeedbackWindingOrderTestInstance::getTopologyParametersMap(void)
{
	static const TopologyParametersMap topologyParametersMap =
	{
		{
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			{
				1u,
				[](deUint32 partCount)	{	return partCount;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ i, i + 1u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
			{
				2u,
				[](deUint32 partCount)	{	return partCount;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ 2 * i, 2 * i + 1u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
			{
				2u,
				[](deUint32 partCount)	{	return 2u * (partCount - 1);	},
				[](deUint32 i)			{	return std::vector<deUint32>{ i, i + 1u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			{
				3u,
				[](deUint32 partCount)	{	return partCount;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ 3 * i, 3 * i + 1u, 3 * i + 2u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			{
				3u,
				[](deUint32 partCount)	{	return 3u * (partCount - 2);	},
				[](deUint32 i)
				{
					const deUint32	iMod2 = i % 2;
					return std::vector<deUint32>{ i, i + 1 + iMod2, i + 2 - iMod2 };
				}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
			{
				3u,
				[](deUint32 partCount)	{	return partCount;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ i + 1, i + 2, 0 };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
			{
				2u,
				[](deUint32 partCount)	{	return partCount / 4u;	},		// note: this cant be replaced with partCount / 2 as for partCount=6 we will get 3 instead of 2
				[](deUint32 i)			{	return std::vector<deUint32>{ i + 1u, i + 2u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
			{
				2u,
				[](deUint32 partCount)	{	return 2u * (partCount - 3u);	},
				[](deUint32 i)			{	return std::vector<deUint32>{ i + 1u, i + 2u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
			{
				3u,
				[](deUint32 partCount)	{	return partCount / 2u;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ 6 * i, 6 * i + 2u, 6 * i + 4u	};	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
			{
				3u,
				[](deUint32 partCount)	{	return 3u * (partCount / 2u - 2u);	},
				[](deUint32 i)
				{
					const bool even = (0 == i % 2);
					if (even)
						return std::vector<deUint32>{ 2 * i + 0, 2 * i + 2, 2 * i + 4 };
					return std::vector<deUint32>{ 2 * i + 0, 2 * i + 4, 2 * i + 2 };
				}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
			{
				9u,
				[](deUint32 partCount)	{	return partCount * 3u;	},
				[](deUint32 i)
				{
					// we cant generate vertex numbers in tesselation evaluation shader;
					// check if patch index is correct for every 9 generated vertex
					return std::vector<deUint32>(9, i);
				}
			}
		}
	};

	return topologyParametersMap;
}

tcu::TestStatus TransformFeedbackWindingOrderTestInstance::iterate (void)
{
	DE_ASSERT(m_parameters.partCount >= 6);

	const auto&						deviceHelper		= getDeviceHelper(m_context, m_parameters);
	const auto&						vki					= m_context.getInstanceInterface();
	const auto						physicalDevice		= m_context.getPhysicalDevice();
	const DeviceInterface&			vk					= deviceHelper.getDeviceInterface();
	const VkDevice					device				= deviceHelper.getDevice();
	const deUint32					queueFamilyIndex	= deviceHelper.getQueueFamilyIndex();
	const VkQueue					queue				= deviceHelper.getQueue();
	Allocator&						allocator			= deviceHelper.getAllocator();

	const ShaderWrapper				vertexModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	ShaderWrapper					tescModule;
	ShaderWrapper					teseModule;
	const ShaderWrapper				kNullModule;

	if (m_requiresTesselationStage)
	{
		tescModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("tesc"), 0u);
		teseModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("tese"), 0u);
	}

	const Unique<VkRenderPass>		renderPass			(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>		framebuffer			(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto						pipelineLayout		(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto						pipeline			(makeGraphicsPipeline					(m_parameters.pipelineConstructionType,
																								 vki, vk, physicalDevice, device, m_context.getDeviceExtensions(),
																								 *pipelineLayout, *renderPass,
																								 vertexModule,
																								 tescModule,
																								 teseModule,
																								 kNullModule,
																								 kNullModule,
																								 m_imageExtent2D, 0u, DE_NULL, m_parameters.primTopology));
	const Unique<VkCommandPool>		cmdPool				(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const VkDeviceSize				bufferSize			= m_tParameters.getNumGeneratedPoints	(m_parameters.partCount) * sizeof(deUint32);
	const VkBufferCreateInfo		tfBufCreateInfo		= makeBufferCreateInfo					(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>			tfBuf				= createBuffer							(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>		tfBufAllocation		= allocator.allocate					(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier			tfMemoryBarrier		= makeMemoryBarrier						(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const VkDeviceSize				tfBufBindingSize	= bufferSize;
	const VkDeviceSize				tfBufBindingOffset	= 0u;
	const deUint32					startValue			= 0u;

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffset, &tfBufBindingSize);

			vk.cmdPushConstants(*cmdBuffer, pipelineLayout->get(), VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, m_parameters.partCount, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, static_cast<deUint32>(bufferSize));

	return tcu::TestStatus::pass("Pass");
}

template <typename T, int Size>
bool operator>(const tcu::Vector<T, Size>& a, const tcu::Vector<T, Size>& b)
{
	return tcu::boolAny(tcu::greaterThan(a, b));
}

template <typename T, int Size>
tcu::Vector<T, Size> elemAbsDiff (const tcu::Vector<T, Size>& a, const tcu::Vector<T, Size>& b)
{
	return tcu::absDiff(a, b);
}

uint32_t elemAbsDiff (uint32_t a, uint32_t b)
{
	if (a > b)
		return a - b;
	return b - a;
}

template <typename T>
std::vector<std::string> verifyVertexDataWithWinding (const std::vector<T>& reference, const T* result, const size_t vertexCount, const size_t verticesPerPrimitive, const T& threshold)
{
	//DE_ASSERT(vertexCount % verticesPerPrimitive == 0);
	//DE_ASSERT(reference.size() == vertexCount);
	const size_t primitiveCount = vertexCount / verticesPerPrimitive;

	std::vector<std::string> errors;

	for (size_t primIdx = 0; primIdx < primitiveCount; ++primIdx)
	{
		const auto	pastVertexCount	= verticesPerPrimitive * primIdx;
		const T*	resultPrim		= result + pastVertexCount;
		const T*	referencePrim	= &reference.at(pastVertexCount);
		bool		primitiveOK		= false;

		// Vertices must be in the same winding order, but the first vertex may vary. We test every rotation below.
		// E.g. vertices 0 1 2 could be stored as 0 1 2, 2 0 1 or 1 2 0.
		for (size_t firstVertex = 0; firstVertex < verticesPerPrimitive; ++firstVertex)
		{
			bool match = true;
			for (size_t vertIdx = 0; vertIdx < verticesPerPrimitive; ++vertIdx)
			{
				const auto& refVertex = referencePrim[(firstVertex + vertIdx) % verticesPerPrimitive]; // Rotation.
				const auto& resVertex = resultPrim[vertIdx];

				if (elemAbsDiff(refVertex, resVertex) > threshold)
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				primitiveOK = true;
				break;
			}
		}

		if (!primitiveOK)
		{
			// Log error.
			std::ostringstream err;
			err << "Primitive " << primIdx << " failed: expected rotation of [";
			for (size_t i = 0; i < verticesPerPrimitive; ++i)
				err << ((i > 0) ? ", " : "") << referencePrim[i];
			err << "] but found [";
			for (size_t i = 0; i < verticesPerPrimitive; ++i)
				err << ((i > 0) ? ", " : "") << resultPrim[i];
			err << "]; threshold: " << threshold;
			errors.push_back(err.str());
		}
	}

	return errors;
}

void checkErrorVec (tcu::TestLog& log, const std::vector<std::string>& errors)
{
	if (!errors.empty())
	{
		for (const auto& err : errors)
			log << tcu::TestLog::Message << err << tcu::TestLog::EndMessage;
		TCU_FAIL("Vertex data verification failed; check log for details");
	}
}

void TransformFeedbackWindingOrderTestInstance::verifyTransformFeedbackBuffer (const DeviceHelper&			deviceHelper,
																			   const MovePtr<Allocation>&	bufAlloc,
																			   const deUint32				bufBytes)
{
	const DeviceInterface&	vk					= deviceHelper.getDeviceInterface();
	const VkDevice			device				= deviceHelper.getDevice();
	const deUint32			numPoints			= static_cast<deUint32>(bufBytes / sizeof(deUint32));
	const deUint32			vertexPerPrimitive	= m_tParameters.vertexPerPrimitive;
	const deUint32			numPrimitives		= numPoints / vertexPerPrimitive;
	const deUint32*			tfData				= getInvalidatedHostPtr<deUint32>(vk, device, *bufAlloc);

	std::vector<uint32_t> referenceValues;
	referenceValues.reserve(numPrimitives * vertexPerPrimitive);

	for (uint32_t primIdx = 0; primIdx < numPrimitives; ++primIdx)
	{
		const auto expectedValues = m_tParameters.getExpectedValuesForPrimitive(primIdx);
		for (const auto& value : expectedValues)
			referenceValues.push_back(value);
	}

	const auto errors = verifyVertexDataWithWinding(referenceValues, tfData, numPoints, vertexPerPrimitive, 0u/*threshold*/);
	checkErrorVec(m_context.getTestContext().getLog(), errors);
}

class TransformFeedbackBuiltinTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackBuiltinTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate									(void);
	void				verifyTransformFeedbackBuffer			(const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const VkDeviceSize offset, const deUint32 bufBytes, const uint32_t onePeriodicity);
};

TransformFeedbackBuiltinTestInstance::TransformFeedbackBuiltinTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&		vki			= m_context.getInstanceInterface();
	const VkPhysicalDevice			physDevice	= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures	features	= getPhysicalDeviceFeatures(vki, physDevice);

	const deUint32 tfBuffersSupported	= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32 tfBuffersRequired	= m_parameters.partCount;

	if ((m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE || m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) && !features.shaderClipDistance)
		TCU_THROW(NotSupportedError, std::string("shaderClipDistance feature is not supported"));
	if ((m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE || m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) && !features.shaderCullDistance)
		TCU_THROW(NotSupportedError, std::string("shaderCullDistance feature is not supported"));
	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());
}

void TransformFeedbackBuiltinTestInstance::verifyTransformFeedbackBuffer (const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const VkDeviceSize offset, const deUint32 bufBytes, const uint32_t onePeriodicity)
{
	const DeviceInterface&	vk			= deviceHelper.getDeviceInterface();
	const VkDevice			device		= deviceHelper.getDevice();
	const deUint32			numPoints	= bufBytes / static_cast<deUint32>(sizeof(float));
	const deUint8*			tfDataBytes	= getInvalidatedHostPtr<deUint8>(vk, device, *bufAlloc);
	const float*			tfData		= (float*)&tfDataBytes[offset];

	for (deUint32 i = 0; i < numPoints; ++i)
	{
		// onePeriodicity, when different from zero, indicates the periodic position of a 1.0 value in the results buffer. This is
		// typically used when we need to emit a PointSize value together with other interesting data to the XFB buffer.
		const bool		isOne		= (onePeriodicity > 0u && (i % onePeriodicity == onePeriodicity - 1u));
		const deUint32	divisor		= 32768u;
		const float		epsilon		= (isOne ? 0.0f : 1.0f / float(divisor));
		const float		expected	= (isOne ? 1.0f : float(i) / float(divisor));

		if (deAbs(tfData[i] - expected) > epsilon)
			TCU_FAIL(std::string("Failed at item ") + de::toString(i) + " received:" + de::toString(tfData[i]) + " expected:" + de::toString(expected));
	}
}

tcu::TestStatus TransformFeedbackBuiltinTestInstance::iterate (void)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&							vki						= m_context.getInstanceInterface();
	const auto							physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue					= deviceHelper.getQueue();
	Allocator&							allocator				= deviceHelper.getAllocator();

	const ShaderWrapper					vertexModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					kNullModule;
	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto&							pipelineLayout			(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule, kNullModule, kNullModule, kNullModule, kNullModule, m_imageExtent2D, 0u, &m_parameters.streamId));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkDeviceSize					tfBufSize				= m_parameters.bufferSize * m_parameters.partCount;
	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(tfBufSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>			tfBufArray				= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= std::vector<VkDeviceSize>(m_parameters.partCount, m_parameters.bufferSize);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);
	const deUint32						perVertexDataSize		= (m_parameters.testType == TEST_TYPE_XFB_POINTSIZE)     ? static_cast<deUint32>(1u * sizeof(float))
																: (m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE)  ? static_cast<deUint32>(8u * sizeof(float))
																: (m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE)  ? static_cast<deUint32>(8u * sizeof(float))
																: (m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) ? static_cast<deUint32>(6u * sizeof(float))
																: 0u;
	const bool							pointSizeWanted			= m_parameters.pointSizeWanted();
	const uint32_t						onePeriodicity			= (pointSizeWanted && m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE)  ? 8u
																: (pointSizeWanted && m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE)  ? 8u
																: (pointSizeWanted && m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) ? 6u
																: 0u;
	const deUint32						numPoints				= m_parameters.bufferSize / perVertexDataSize;

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, tfBufBindingOffsets[m_parameters.partCount - 1], numPoints * perVertexDataSize, onePeriodicity);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackDepthClipControlTestInstance : public TransformFeedbackTestInstance
{
public:
	TransformFeedbackDepthClipControlTestInstance		(Context& context, const TestParameters& parameters);

protected:
	uint32_t			getFloatsPerVertex				(void) const;
	uint32_t			getActualBufferSize				(void) const;
	tcu::TestStatus		iterate							(void);
	void				verifyTransformFeedbackBuffer	(const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const VkDeviceSize offset, const deUint32 bufBytes);
};

TransformFeedbackDepthClipControlTestInstance::TransformFeedbackDepthClipControlTestInstance (Context& context, const TestParameters& parameters)
		: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&		vki			= m_context.getInstanceInterface();
	const VkPhysicalDevice			physDevice	= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures	features	= getPhysicalDeviceFeatures(vki, physDevice);

	const deUint32 tfBuffersSupported	= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32 tfBuffersRequired	= m_parameters.partCount;

	if (!context.isDeviceFunctionalitySupported("VK_EXT_depth_clip_control"))
		TCU_THROW(NotSupportedError, "VK_EXT_depth_clip_control is not supported");

	if (parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_GEOMETRY && !features.geometryShader)
		TCU_THROW(NotSupportedError, "Geometry shader not supported");

	if (parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_TESE && !features.tessellationShader)
		TCU_THROW(NotSupportedError, "Tessellation shader not supported");

	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());
}

uint32_t TransformFeedbackDepthClipControlTestInstance::getFloatsPerVertex (void) const
{
	return (m_parameters.pointSizeWanted() ? 5u : 4u); // 4 for position, 1 for pointsize in some cases. Needs to match shaders.
}

uint32_t TransformFeedbackDepthClipControlTestInstance::getActualBufferSize (void) const
{
	if (m_parameters.testType != TEST_TYPE_DEPTH_CLIP_CONTROL_TESE || !m_parameters.pointSizeWanted())
		return m_parameters.bufferSize;

	// For cases using tesellation and point size, we want the same number of points in the PointSize and the non-PointSize case,
	// which means the buffer size has to change a bit, and we'll consider the buffer size indicated in the test parameters as a
	// reference to calculate the number of points in the non-PointSize case. For PointSize cases we'll calculate the actual buffer
	// size based on the target number of points and the amount of data used by each one, reversing the usual test logic.

	// These have to match shader code.
	const auto floatsPerVertexNoPointSize	= 4u;
	const auto floatsPerVertexPointSize		= 5u;
	const auto vertexSizeNoPointSize		= static_cast<uint32_t>(sizeof(float)) * floatsPerVertexNoPointSize;
	const auto vertexSizePointSize			= static_cast<uint32_t>(sizeof(float)) * floatsPerVertexPointSize;

	const auto numVertices					= m_parameters.bufferSize / vertexSizeNoPointSize;
	const auto actualBufferSize				= numVertices * vertexSizePointSize;

	return actualBufferSize;
}

void TransformFeedbackDepthClipControlTestInstance::verifyTransformFeedbackBuffer (const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const VkDeviceSize offset, const deUint32 bufBytes)
{
	const DeviceInterface&	vk			= deviceHelper.getDeviceInterface();
	const VkDevice			device		= deviceHelper.getDevice();
	const uint32_t			flPerVertex	= getFloatsPerVertex();
	const deUint32			numVertices	= bufBytes / static_cast<deUint32>(sizeof(float) * flPerVertex);
	const deUint8*			tfDataBytes	= getInvalidatedHostPtr<deUint8>(vk, device, *bufAlloc);
	const float*			tfData		= (float*)&tfDataBytes[offset];
	std::vector<float>		result;

	// We only care about the depth (z) value.
	for (deUint32 i = 0; i < numVertices; i++)
		result.push_back(tfData[i * flPerVertex + 2]);

	// Tessellation generates triangles whose vertex data might be written into
	// transform feedback buffer in a different order than generated by the vertex
	// shader. Sort the values here to allow comparison.
	if (m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_TESE)
	{
		std::sort(result.begin(), result.end());
	}

	// Verify the vertex depth values match with the ones written by the shader.
	for (deUint32 i = 0; i < numVertices; i++)
	{
		const float	expected	= (float)i / 3.0f - 1.0f;
		const float	epsilon		= 0.0001f;

		if (deAbs(result[i] - expected) > epsilon)
			TCU_FAIL(std::string("Failed at vertex ") + de::toString(i) + " depth. Received:" + de::toString(result[i]) + " expected:" + de::toString(expected));
	}
}

tcu::TestStatus TransformFeedbackDepthClipControlTestInstance::iterate (void)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&							vki						= m_context.getInstanceInterface();
	const auto							physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue					= deviceHelper.getQueue();
	Allocator&							allocator				= deviceHelper.getAllocator();

	const ShaderWrapper					kNullModule;
	const ShaderWrapper					vertexModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	ShaderWrapper						geomModule;
	ShaderWrapper						tescModule;
	ShaderWrapper						teseModule;
	const bool							hasGeomShader			= m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_GEOMETRY;
	const bool							hasTessellation			= m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_TESE;

	if (hasGeomShader)
		geomModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("geom"), 0u);

	if (hasTessellation)
	{
		tescModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("tesc"), 0u);
		teseModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("tese"), 0u);
	}

	const Unique<VkRenderPass>			renderPass				(makeRenderPass(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout			(TransformFeedback::makePipelineLayout (m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline				(makeGraphicsPipeline(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule, tescModule, teseModule, geomModule, kNullModule, m_imageExtent2D, 0u, &m_parameters.streamId, m_parameters.primTopology, false, true));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const auto							bufferSizeParam			= getActualBufferSize();
	const VkDeviceSize					tfBufSize				= bufferSizeParam * m_parameters.partCount;
	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(tfBufSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>			tfBufArray				= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= std::vector<VkDeviceSize>(m_parameters.partCount, bufferSizeParam);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);
	const uint32_t						floatsPerVertex			= getFloatsPerVertex();
	const deUint32						perVertexDataSize		= static_cast<deUint32>(floatsPerVertex * sizeof(float));
	const deUint32						numVertices				= bufferSizeParam / perVertexDataSize;

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, tfBufBindingOffsets[m_parameters.partCount - 1], bufferSizeParam);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackMultistreamTestInstance : public TransformFeedbackTestInstance
{
public:
								TransformFeedbackMultistreamTestInstance	(Context& context, const TestParameters& parameters);

protected:
	std::vector<VkDeviceSize>	generateSizesList							(const size_t bufBytes, const size_t chunkCount);
	void						verifyTransformFeedbackBuffer				(const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes);
	tcu::TestStatus				iterate										(void);
};

TransformFeedbackMultistreamTestInstance::TransformFeedbackMultistreamTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;
	const deUint32											tfBuffersSupported			= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32											tfBuffersRequired			= m_parameters.partCount;
	const deUint32											bytesPerVertex				= m_parameters.bufferSize / m_parameters.partCount;
	const deUint32											tfStreamDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackStreamDataSize;
	const deUint32											tfBufferDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize;
	const deUint32											tfBufferDataStrideSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride;

	DE_ASSERT(m_parameters.partCount == 2u);

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());

	if (tfStreamDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreamDataSize=" + de::toString(tfStreamDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataSize=" + de::toString(tfBufferDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataStrideSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataStride=" + de::toString(tfBufferDataStrideSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());
}

std::vector<VkDeviceSize> TransformFeedbackMultistreamTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const VkDeviceSize			chunkSize	= bufBytes / chunkCount;
	std::vector<VkDeviceSize>	result		(chunkCount, chunkSize);

	DE_ASSERT(chunkSize * chunkCount == bufBytes);
	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(chunkCount > 0);
	DE_ASSERT(result.size() == chunkCount);

	return result;
}

void TransformFeedbackMultistreamTestInstance::verifyTransformFeedbackBuffer (const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes)
{
	const DeviceInterface&	vk			= deviceHelper.getDeviceInterface();
	const VkDevice			device		= deviceHelper.getDevice();
	const deUint32			numPoints	= static_cast<deUint32>(bufBytes / sizeof(deUint32));
	const float*			tfData		= getInvalidatedHostPtr<float>(vk, device, *bufAlloc);

	for (deUint32 i = 0; i < numPoints; ++i)
		if (tfData[i] != float(i))
			TCU_FAIL(std::string("Failed at item ") + de::toString(float(i)) + " received:" + de::toString(tfData[i]) + " expected:" + de::toString(i));
}

tcu::TestStatus TransformFeedbackMultistreamTestInstance::iterate (void)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&							vki						= m_context.getInstanceInterface();
	const auto							physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue					= deviceHelper.getQueue();
	Allocator&							allocator				= deviceHelper.getAllocator();

	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));

	const ShaderWrapper					vertexModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					geomModule				(vk, device, m_context.getBinaryCollection().get("geom"), 0u);
	const ShaderWrapper					kNullModule;

	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout			(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule, kNullModule, kNullModule, geomModule, kNullModule, m_imageExtent2D, 0u));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>			tfBufArray				= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0u, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackMultistreamSameLocationTestInstance final : public TransformFeedbackTestInstance
{
public:
	TransformFeedbackMultistreamSameLocationTestInstance(Context& context, const TestParameters& parameters);
protected:
	tcu::TestStatus		iterate							(void) override;
	void				verifyTransformFeedbackBuffer	(const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, deUint32 bufBytes);
};

void TransformFeedbackMultistreamSameLocationTestInstance::verifyTransformFeedbackBuffer (const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes)
{
	const DeviceInterface&	vk			= deviceHelper.getDeviceInterface();
	const VkDevice			device		= deviceHelper.getDevice();
	const auto				numPoints	= static_cast<deUint32>(bufBytes / sizeof(deUint32));
	const auto*				tuData		= getInvalidatedHostPtr<deUint32>(vk, device, *bufAlloc);

	for (deUint32 i = 0; i < numPoints; ++i)
		if (tuData[i] != i*2 - ((i / 16) == 0 ? 0 : 31))
			TCU_FAIL(std::string("Failed at item ") + de::toString(i) + " received:" + de::toString(tuData[i]) + " expected:" + de::toString(i));

	return;
}

TransformFeedbackMultistreamSameLocationTestInstance::TransformFeedbackMultistreamSameLocationTestInstance(Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;
	const deUint32											tfBuffersSupported			= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32											tfBuffersRequired			= 1;
	const deUint32											bytesPerVertex				= 4;
	const deUint32											tfStreamDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackStreamDataSize;
	const deUint32											tfBufferDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize;
	const deUint32											tfBufferDataStrideSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride;

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());

	if (tfStreamDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreamDataSize=" + de::toString(tfStreamDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataSize=" + de::toString(tfBufferDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataStrideSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataStride=" + de::toString(tfBufferDataStrideSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());
}

tcu::TestStatus TransformFeedbackMultistreamSameLocationTestInstance::iterate (void)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&							vki						= m_context.getInstanceInterface();
	const auto							physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue					= deviceHelper.getQueue();
	Allocator&							allocator				= deviceHelper.getAllocator();

	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));

	const ShaderWrapper					vertexModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					geomModule				(vk, device, m_context.getBinaryCollection().get("geom"), 0u);
	const ShaderWrapper					kNullModule;

	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout			(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule, kNullModule, kNullModule, geomModule, kNullModule, m_imageExtent2D, 0u));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>			tfBufArray				= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= { m_parameters.bufferSize / 2, m_parameters.bufferSize / 2 };
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= { 0, m_parameters.bufferSize / 2 };

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0u, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, 16u, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackStreamsTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackStreamsTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate									(void);
	bool				verifyImage								(const VkFormat imageFormat, const VkExtent2D& size, const void* resultData);
};

TransformFeedbackStreamsTestInstance::TransformFeedbackStreamsTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;
	const bool												geomPointSizeRequired		= m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE;

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (geomPointSizeRequired && !features.shaderTessellationAndGeometryPointSize)
		TCU_THROW(NotSupportedError, "shaderTessellationAndGeometryPointSize feature is not supported");
}

bool TransformFeedbackStreamsTestInstance::verifyImage (const VkFormat imageFormat, const VkExtent2D& size, const void* resultData)
{
	const tcu::RGBA				magentaRGBA		(tcu::RGBA(0xFF, 0x00, 0xFF, 0xFF));
	const tcu::Vec4				magenta			(magentaRGBA.toVec());
	const tcu::Vec4				black			(tcu::RGBA::black().toVec());
	const tcu::TextureFormat	textureFormat	(mapVkFormat(imageFormat));
	const int					dataSize		(size.width * size.height * textureFormat.getPixelSize());
	tcu::TextureLevel			referenceImage	(textureFormat, size.width, size.height);
	tcu::PixelBufferAccess		referenceAccess	(referenceImage.getAccess());

	// Generate reference image
	if (m_parameters.testType == TEST_TYPE_STREAMS)
	{
		for (int y = 0; y < referenceImage.getHeight(); ++y)
		{
			const tcu::Vec4&	validColor = y < referenceImage.getHeight() / 2 ? black : magenta;

			for (int x = 0; x < referenceImage.getWidth(); ++x)
				referenceAccess.setPixel(validColor, x, y);
		}
	}

	if (m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE || m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE)
	{
		for (int y = 0; y < referenceImage.getHeight(); ++y)
			for (int x = 0; x < referenceImage.getWidth(); ++x)
			{
				const tcu::Vec4&	validColor	= (y >= referenceImage.getHeight() / 2) && (x >= referenceImage.getWidth() / 2) ? magenta : black;

				referenceAccess.setPixel(validColor, x, y);
			}
	}

	if (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE)
	{
		const int			pointSize	= static_cast<int>(m_parameters.pointSize);
		const tcu::Vec4&	validColor	= black;

		for (int y = 0; y < referenceImage.getHeight(); ++y)
			for (int x = 0; x < referenceImage.getWidth(); ++x)
				referenceAccess.setPixel(validColor, x, y);

		referenceAccess.setPixel(magenta, (1 + referenceImage.getWidth()) / 4 - 1, (referenceImage.getHeight() * 3) / 4 - 1);

		for (int y = 0; y < pointSize; ++y)
			for (int x = 0; x < pointSize; ++x)
				referenceAccess.setPixel(magenta, x + (referenceImage.getWidth() * 3) / 4 - 1, y + (referenceImage.getHeight() * 3) / 4 - 1);
	}

	if (deMemCmp(resultData, referenceAccess.getDataPtr(), dataSize) != 0)
	{
		const tcu::ConstPixelBufferAccess	resultImage	(textureFormat, size.width, size.height, 1, resultData);
		bool								ok;

		ok = tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Image comparison", "", referenceAccess, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT);

		return ok;
	}

	return true;
}

tcu::TestStatus TransformFeedbackStreamsTestInstance::iterate (void)
{
	const auto&							deviceHelper		= getDeviceHelper(m_context, m_parameters);
	const auto&							vki					= m_context.getInstanceInterface();
	const auto							physicalDevice		= m_context.getPhysicalDevice();
	const DeviceInterface&				vk					= deviceHelper.getDeviceInterface();
	const VkDevice						device				= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex	= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue				= deviceHelper.getQueue();
	Allocator&							allocator			= deviceHelper.getAllocator();

	const Unique<VkRenderPass>			renderPass			(makeRenderPass			(vk, device, VK_FORMAT_R8G8B8A8_UNORM));

	const ShaderWrapper					vertModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					geomModule			(vk, device, m_context.getBinaryCollection().get("geom"), 0u);
	const ShaderWrapper					fragModule			(vk, device, m_context.getBinaryCollection().get("frag"), 0u);
	const ShaderWrapper					kNullModule;

	const VkFormat						colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageUsageFlags				imageUsageFlags		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const tcu::RGBA						clearColor			(tcu::RGBA::black());
	const VkImageSubresourceRange		colorSubresRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const VkDeviceSize					colorBufferSize		(m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat)));
	const Unique<VkImage>				colorImage			(makeImage								(vk, device, makeImageCreateInfo(0u, VK_IMAGE_TYPE_2D, colorFormat, m_imageExtent2D, 1u, imageUsageFlags)));
	const UniquePtr<Allocation>			colorImageAlloc		(bindImage								(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>			colorAttachment		(makeImageView							(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>				colorBuffer			(makeBuffer								(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			colorBufferAlloc	(bindBuffer								(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer						(vk, device, *renderPass, *colorAttachment, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout		(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline			(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertModule, kNullModule, kNullModule, geomModule, fragModule, m_imageExtent2D, 0u, &m_parameters.streamId, m_parameters.primTopology, false, false, 1u));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkImageMemoryBarrier			preCopyBarrier		= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *colorImage, colorSubresRange);
	const VkBufferImageCopy				region				= makeBufferImageCopy(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																				  makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	const VkBufferMemoryBarrier			postCopyBarrier		= makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, VK_WHOLE_SIZE);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor.toVec());
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdDraw(*cmdBuffer, 2u, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
		vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);

		invalidateAlloc(vk, device, *colorBufferAlloc);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	if (!verifyImage(colorFormat, m_imageExtent2D, colorBufferAlloc->getHostPtr()))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackIndirectDrawTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackIndirectDrawTestInstance	(Context& context, const TestParameters& parameters, bool multiview);

protected:
	tcu::TestStatus		iterate										(void);
	bool				verifyImage									(const VkFormat imageFormat, const VkExtent2D& size, const void* resultData, uint32_t layerIdx = std::numeric_limits<uint32_t>::max());

	const bool			m_multiview;
};

TransformFeedbackIndirectDrawTestInstance::TransformFeedbackIndirectDrawTestInstance (Context& context, const TestParameters& parameters, bool multiview)
	: TransformFeedbackTestInstance	(context, parameters)
	, m_multiview					(multiview)
{
	const InstanceInterface&		vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice			physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceLimits	limits						= getPhysicalDeviceProperties(vki, physDevice).limits;
	const deUint32					tfBufferDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize;
	const deUint32					tfBufferDataStrideSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride;

	if (m_transformFeedbackProperties.transformFeedbackDraw == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackDraw feature is not supported");

	if (limits.maxVertexInputBindingStride < m_parameters.vertexStride)
		TCU_THROW(NotSupportedError, std::string("maxVertexInputBindingStride=" + de::toString(limits.maxVertexInputBindingStride) + ", while test requires " + de::toString(m_parameters.vertexStride)).c_str());

	if (tfBufferDataSizeSupported < m_parameters.vertexStride)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataSize=" + de::toString(tfBufferDataSizeSupported) + ", while test requires " + de::toString(m_parameters.vertexStride)).c_str());

	if (tfBufferDataStrideSupported < m_parameters.vertexStride)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataStride=" + de::toString(tfBufferDataStrideSupported) + ", while test requires " + de::toString(m_parameters.vertexStride)).c_str());
}

bool TransformFeedbackIndirectDrawTestInstance::verifyImage (const VkFormat imageFormat, const VkExtent2D& size, const void* resultData, uint32_t layerIdx)
{
	const tcu::Vec4				white			(tcu::RGBA::white().toVec());
	const tcu::TextureFormat	textureFormat	(mapVkFormat(imageFormat));
	const int					dataSize		(size.width * size.height * textureFormat.getPixelSize());
	tcu::TextureLevel			referenceImage	(textureFormat, size.width, size.height);
	tcu::PixelBufferAccess		referenceAccess	(referenceImage.getAccess());
	const bool					isMultilayer	= (layerIdx != std::numeric_limits<uint32_t>::max());
	const std::string			setName			= "Image comparison" + (isMultilayer ? " (layer " + std::to_string(layerIdx) + ")" : std::string());

	// Generate reference image
	for (int y = 0; y < referenceImage.getHeight(); ++y)
		for (int x = 0; x < referenceImage.getWidth(); ++x)
			referenceAccess.setPixel(white, x, y);

	if (deMemCmp(resultData, referenceAccess.getDataPtr(), dataSize) != 0)
	{
		const tcu::ConstPixelBufferAccess	resultImage	(textureFormat, size.width, size.height, 1, resultData);
		bool								ok;

		ok = tcu::intThresholdCompare(m_context.getTestContext().getLog(), setName.c_str(), "", referenceAccess, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT);

		return ok;
	}

	return true;
}

tcu::TestStatus TransformFeedbackIndirectDrawTestInstance::iterate (void)
{
	const auto&							deviceHelper		= getDeviceHelper(m_context, m_parameters);
	const auto&							vki					= m_context.getInstanceInterface();
	const auto							physicalDevice		= m_context.getPhysicalDevice();
	const DeviceInterface&				vk					= deviceHelper.getDeviceInterface();
	const VkDevice						device				= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex	= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue				= deviceHelper.getQueue();
	Allocator&							allocator			= deviceHelper.getAllocator();
	const uint32_t						layerCount			= (m_multiview ? 2u : 1u);
	const auto							colorViewType		= (layerCount > 1u ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);

	// Only used for multiview.
	const std::vector<uint32_t>			subpassViewMasks	{ ((1u << layerCount) - 1u) };

	const VkRenderPassMultiviewCreateInfo multiviewCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,	//	VkStructureType	sType;
		nullptr,												//	const void*		pNext;
		de::sizeU32(subpassViewMasks),							//	uint32_t		subpassCount;
		de::dataOrNull(subpassViewMasks),						//	const uint32_t*	pViewMasks;
		0u,														//	uint32_t		dependencyCount;
		nullptr,												//	const int32_t*	pViewOffsets;
		de::sizeU32(subpassViewMasks),							//	uint32_t		correlationMaskCount;
		de::dataOrNull(subpassViewMasks),						//	const uint32_t*	pCorrelationMasks;
	};

	const Unique<VkRenderPass>			renderPass			(makeRenderPass			(vk,
																					 device,
																					 VK_FORMAT_R8G8B8A8_UNORM,
																					 VK_FORMAT_UNDEFINED,
																					 VK_ATTACHMENT_LOAD_OP_CLEAR,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																					 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																					 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
																					 nullptr,
																					 (m_multiview ? &multiviewCreateInfo : nullptr)));

	const ShaderWrapper					vertModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					fragModule			(vk, device, m_context.getBinaryCollection().get("frag"), 0u);
	const ShaderWrapper					kNullModule;

	const VkFormat						colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageUsageFlags				imageUsageFlags		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const tcu::RGBA						clearColor			(tcu::RGBA::black());
	const VkImageSubresourceRange		colorSubresRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount));
	const VkDeviceSize					layerSize			(m_imageExtent2D.width * m_imageExtent2D.height * static_cast<uint32_t>(tcu::getPixelSize(mapVkFormat(colorFormat))));
	const VkDeviceSize					colorBufferSize		(layerSize * layerCount);
	const Unique<VkImage>				colorImage			(makeImage				(vk, device, makeImageCreateInfo(0u, VK_IMAGE_TYPE_2D, colorFormat, m_imageExtent2D, layerCount, imageUsageFlags)));
	const UniquePtr<Allocation>			colorImageAlloc		(bindImage				(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>			colorAttachment		(makeImageView			(vk, device, *colorImage, colorViewType, colorFormat, colorSubresRange));
	const Unique<VkBuffer>				colorBuffer			(makeBuffer				(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			colorBufferAlloc	(bindBuffer				(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const deUint32						vertexCount			= 6u;
	const VkDeviceSize					vertexBufferSize	= vertexCount * m_parameters.vertexStride;
	const VkBufferUsageFlags			vertexBufferUsage	= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const Unique<VkBuffer>				vertexBuffer		(makeBuffer				(vk, device, vertexBufferSize, vertexBufferUsage));
	const UniquePtr<Allocation>			vertexBufferAlloc	(bindBuffer				(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));
	const VkDeviceSize					vertexBufferOffset	(0u);
	const float							vertexBufferVals[]	=
																{
																	-1.0f, -1.0f, 0.0f, 1.0f,
																	-1.0f, +1.0f, 0.0f, 1.0f,
																	+1.0f, -1.0f, 0.0f, 1.0f,
																	-1.0f, +1.0f, 0.0f, 1.0f,
																	+1.0f, -1.0f, 0.0f, 1.0f,
																	+1.0f, +1.0f, 0.0f, 1.0f,
																};

	const deUint32						counterBufferValue	= m_parameters.vertexStride * vertexCount;
	const VkDeviceSize					counterBufferSize	= sizeof(counterBufferValue);
	const VkBufferUsageFlags			counterBufferUsage	= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const Unique<VkBuffer>				counterBuffer		(makeBuffer								(vk, device, counterBufferSize, counterBufferUsage));
	const UniquePtr<Allocation>			counterBufferAlloc	(bindBuffer								(vk, device, allocator, *counterBuffer, MemoryRequirement::HostVisible));

	// Note: for multiview the framebuffer layer count is also 1.
	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer						(vk, device, *renderPass, *colorAttachment, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout		(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline			(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertModule, kNullModule, kNullModule, kNullModule, fragModule, m_imageExtent2D, 0u, DE_NULL, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true, false, 1u));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkImageMemoryBarrier			preCopyBarrier		= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *colorImage, colorSubresRange);
	const VkBufferImageCopy				region				= makeBufferImageCopy(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																				  makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, layerCount));
	const VkBufferMemoryBarrier			postCopyBarrier		= makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, VK_WHOLE_SIZE);

	fillBuffer(vk, device, *counterBufferAlloc, counterBufferSize, &counterBufferValue, counterBufferSize);
	fillBuffer(vk, device, *vertexBufferAlloc, vertexBufferSize, vertexBufferVals, sizeof(vertexBufferVals));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor.toVec());
		{
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdDrawIndirectByteCountEXT(*cmdBuffer, 1u, 0u, *counterBuffer, 0u, 0u, m_parameters.vertexStride);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
		vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);

		invalidateAlloc(vk, device, *colorBufferAlloc);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	bool fail = false;
	for (uint32_t layerIdx = 0u; layerIdx < layerCount; ++layerIdx)
	{
		const auto dataPtr = reinterpret_cast<uint8_t*>(colorBufferAlloc->getHostPtr()) + layerIdx * layerSize;
		if (!verifyImage(colorFormat, m_imageExtent2D, dataPtr))
			fail = true;
	}

	if (fail)
		return tcu::TestStatus::fail("Fail; check log for details");

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackBackwardDependencyTestInstance : public TransformFeedbackTestInstance
{
public:
								TransformFeedbackBackwardDependencyTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus				iterate											(void);
	std::vector<VkDeviceSize>	generateSizesList								(const size_t bufBytes, const size_t chunkCount);
};

TransformFeedbackBackwardDependencyTestInstance::TransformFeedbackBackwardDependencyTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	if (m_transformFeedbackProperties.transformFeedbackDraw == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackDraw feature is not supported");
}

std::vector<VkDeviceSize> TransformFeedbackBackwardDependencyTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const VkDeviceSize			chunkSize	= bufBytes / chunkCount;
	std::vector<VkDeviceSize>	result		(chunkCount, chunkSize);

	DE_ASSERT(chunkSize * chunkCount == bufBytes);
	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(chunkCount > 0);
	DE_ASSERT(result.size() == chunkCount);

	return result;
}

tcu::TestStatus TransformFeedbackBackwardDependencyTestInstance::iterate (void)
{
	const auto&							deviceHelper		= getDeviceHelper(m_context, m_parameters);
	const auto&							vki					= m_context.getInstanceInterface();
	const auto							physicalDevice		= m_context.getPhysicalDevice();
	const DeviceInterface&				vk					= deviceHelper.getDeviceInterface();
	const VkDevice						device				= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex	= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue				= deviceHelper.getQueue();
	Allocator&							allocator			= deviceHelper.getAllocator();

	const std::vector<VkDeviceSize>		chunkSizesList		= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>		chunkOffsetsList	= generateOffsetsList(chunkSizesList);

	const uint32_t						numPoints			= static_cast<uint32_t>(chunkSizesList[0] / sizeof(uint32_t));
	const bool							indirectDraw		= (m_parameters.testType == TEST_TYPE_BACKWARD_DEPENDENCY_INDIRECT);

	// Color buffer.
	const tcu::IVec3					fbExtent			(static_cast<int>(numPoints), 1, 1);
	const auto							vkExtent			= makeExtent3D(fbExtent);
	const std::vector<VkViewport>		viewports			(1u, makeViewport(vkExtent));
	const std::vector<VkRect2D>			scissors			(1u, makeRect2D(vkExtent));

	const auto							colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const auto							colorUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const tcu::Vec4						clearColor			(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4						geomColor			(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader.
	ImageWithBuffer						colorBuffer			(vk, device, allocator, vkExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D);

	// Must match vertex shader.
	struct PushConstants
	{
		uint32_t	startValue;
		float		width;
		float		posY;
	};

	const auto							pcSize				= static_cast<uint32_t>(sizeof(PushConstants));
	const ShaderWrapper					vertexModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					fragModule			(vk, device, m_context.getBinaryCollection().get("frag"), 0u);
	const ShaderWrapper					kNullModule;
	const Unique<VkRenderPass>			renderPass			(TransformFeedback::makeCustomRenderPass(vk, device, colorFormat));
	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer						(vk, device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height));
	const auto							pipelineLayout		(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device, pcSize));
	const auto							pipeline			(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule, kNullModule, kNullModule, kNullModule, fragModule, makeExtent2D(vkExtent.width, vkExtent.height), 0u, nullptr, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false, false, 1u));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo			tfBufCreateInfo		= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf				= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>			tfBufAllocation		= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier		= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const VkDeviceSize					tfBufBindingSize	= m_parameters.bufferSize;
	const VkDeviceSize					tfBufBindingOffset	= 0ull;

	const size_t						tfcBufSize			= sizeof(deUint32);
	const VkBufferCreateInfo			tfcBufCreateInfo	= makeBufferCreateInfo(tfcBufSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	const Move<VkBuffer>				tfcBuf				= createBuffer(vk, device, &tfcBufCreateInfo);
	const MovePtr<Allocation>			tfcBufAllocation	= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfcBuf), MemoryRequirement::Any);
	const VkDeviceSize					tfcBufBindingOffset	= 0ull;
	const VkMemoryBarrier				tfcMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT, VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT);

	using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
	BufferWithMemoryPtr					indirectBuffer;
	VkDeviceSize						indirectBufferSize;
	VkBufferCreateInfo					indirectBufferInfo;
	std::vector<VkDrawIndirectCommand>	indirectCommands;
	const auto							indirectStructSize	= static_cast<uint32_t>(sizeof(decltype(indirectCommands)::value_type));
	const auto							indirectStride		= indirectStructSize * 2u; // See below.

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));
	VK_CHECK(vk.bindBufferMemory(device, *tfcBuf, tfcBufAllocation->getMemory(), tfcBufAllocation->getOffset()));

	DE_ASSERT(m_parameters.partCount == 2u);

	if (indirectDraw)
	{
		// Prepare indirect commands. The first entry will be used as the count.
		// Each subsequent indirect command will be padded with an unused structure.
		indirectCommands.reserve(numPoints + 1u);
		indirectCommands.push_back(VkDrawIndirectCommand{numPoints, 0u, 0u, 0u});

		for (uint32_t drawIdx = 0u; drawIdx < numPoints; ++drawIdx)
		{
			indirectCommands.push_back(VkDrawIndirectCommand{1u, 1u, drawIdx, 0u});
			indirectCommands.push_back(VkDrawIndirectCommand{0u, 0u, 0u, 0u});
		}

		indirectBufferSize = static_cast<VkDeviceSize>(de::dataSize(indirectCommands));
		indirectBufferInfo = makeBufferCreateInfo(indirectBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

		indirectBuffer.reset(new BufferWithMemory(vk, device, allocator, indirectBufferInfo, MemoryRequirement::HostVisible));
		auto& indirectBufferAlloc	= indirectBuffer->getAllocation();
		void* indirectBufferData	= indirectBufferAlloc.getHostPtr();

		deMemcpy(indirectBufferData, de::dataOrNull(indirectCommands), de::dataSize(indirectCommands));
		flushAlloc(vk, device, indirectBufferAlloc);
	}

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffset, &tfBufBindingSize);

			{
				const uint32_t		startValue = static_cast<uint32_t>(chunkOffsetsList[0] / sizeof(uint32_t));
				const PushConstants	pcData
				{
					startValue,
					static_cast<float>(vkExtent.width),
					static_cast<float>(10.0f), // Push the points offscreen.
				};

				vk.cmdPushConstants(*cmdBuffer, pipelineLayout->get(), VK_SHADER_STAGE_VERTEX_BIT, 0u, pcSize, &pcData);

				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				{
					if (indirectDraw)
						vk.cmdDrawIndirectCount(*cmdBuffer, indirectBuffer->get(), indirectStructSize, indirectBuffer->get(), 0u, numPoints, indirectStride);
					else
						vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 1, &*tfcBuf, m_parameters.noOffsetArray ? DE_NULL : &tfcBufBindingOffset);
			}

			if (indirectDraw)
			{
				// This should be a no-op but allows us to reset the indirect draw counter in case it could influence the follow-up indirect draw.
				vk.cmdDrawIndirectCount(*cmdBuffer, indirectBuffer->get(), indirectStructSize, indirectBuffer->get(), 0u, 0u/*no draws*/, indirectStride);
			}

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 1u, &tfcMemoryBarrier, 0u, DE_NULL, DE_NULL, 0u);

			{
				const uint32_t		startValue = static_cast<deUint32>(chunkOffsetsList[1] / sizeof(deUint32));
				const PushConstants	pcData
				{
					startValue,
					static_cast<float>(vkExtent.width),
					static_cast<float>(0.0f), // Points onscreen in this second draw.
				};

				vk.cmdPushConstants(*cmdBuffer, pipelineLayout->get(), VK_SHADER_STAGE_VERTEX_BIT, 0u, pcSize, &pcData);

				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 1, &*tfcBuf, m_parameters.noOffsetArray ? DE_NULL : &tfcBufBindingOffset);
				{
					vk.cmdDrawIndirectByteCountEXT(*cmdBuffer, 1u, 0u, *tfcBuf, 0u, 0u, 4u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			}

		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	copyImageToBuffer(vk, *cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, m_parameters.bufferSize);

	// Verify color buffer, to check vkCmdDrawIndirectByteCountEXT worked.
	const auto					tcuFormat	= mapVkFormat(colorFormat);
	tcu::TextureLevel			refLevel	(tcuFormat, fbExtent.x(), fbExtent.y());
	const auto					refAccess	= refLevel.getAccess();
	const auto					resAlloc	= colorBuffer.getBufferAllocation();
	tcu::ConstPixelBufferAccess	resAccess	(tcuFormat, fbExtent, resAlloc.getHostPtr());
	auto&						log			= m_context.getTestContext().getLog();
	const tcu::Vec4				threshold	(0.0f, 0.0f, 0.0f, 0.0f);

	tcu::clear(refAccess, geomColor);
	invalidateAlloc(vk, device, resAlloc);

	if (!tcu::floatThresholdCompare(log, "Result", "", refAccess, resAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Color buffer contains unexpected results; check log for details");

	return tcu::TestStatus::pass("Pass");
}


class TransformFeedbackQueryTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackQueryTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate								(void);
};

TransformFeedbackQueryTestInstance::TransformFeedbackQueryTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (streamsRequired > 1 && transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (m_transformFeedbackProperties.transformFeedbackQueries == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackQueries feature is not supported");
}

tcu::TestStatus TransformFeedbackQueryTestInstance::iterate (void)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&							vki						= m_context.getInstanceInterface();
	const auto							physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue					= deviceHelper.getQueue();
	Allocator&							allocator				= deviceHelper.getAllocator();

	const deUint64						overflowVertices		= 3u;
	const deUint32						bytesPerVertex			= static_cast<deUint32>(4 * sizeof(float));
	const deUint64						numVerticesInBuffer		= m_parameters.bufferSize / bytesPerVertex;
	const deUint64						numVerticesToWrite		= numVerticesInBuffer + overflowVertices;
	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));

	const ShaderWrapper					vertModule				(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					geomModule				(vk, device, m_context.getBinaryCollection().get("geom"), 0u);
	const ShaderWrapper					kNullModule;

	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout			(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertModule, kNullModule, kNullModule, geomModule, kNullModule, m_imageExtent2D, 0u, &m_parameters.streamId, m_parameters.primTopology));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const deUint32						tfBufferSize			= (deUint32)topologyData.at(m_parameters.primTopology).getNumPrimitives(numVerticesInBuffer) * (deUint32)topologyData.at(m_parameters.primTopology).primSize * bytesPerVertex;
	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(tfBufferSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>			tfBufAllocation			= bindBuffer(vk, device, allocator, *tfBuf, MemoryRequirement::HostVisible);
	const VkDeviceSize					tfBufBindingSize		= tfBufferSize;
	const VkDeviceSize					tfBufBindingOffset		= 0ull;

	const size_t						queryResultWidth		= (m_parameters.query64bits ? sizeof(deUint64) : sizeof(deUint32));
	const vk::VkQueryControlFlags		queryExtraFlags			= (m_parameters.query64bits ? vk::VK_QUERY_RESULT_64_BIT : 0);
	const deUint32						queryCountersNumber		= 1u;
	const deUint32						queryIndex				= 0u;
	constexpr deUint32					queryResultElements		= 2u;
	const deUint32						queryDataSize			= static_cast<deUint32>(queryResultElements * queryResultWidth) + (m_parameters.queryResultWithAvailability ? (deUint32)queryResultWidth : 0u);
	const VkQueryPoolCreateInfo			queryPoolCreateInfo		= makeQueryPoolCreateInfo(queryCountersNumber);
	const Unique<VkQueryPool>			queryPool				(createQueryPool(vk, device, &queryPoolCreateInfo));

	const VkQueryResultFlagBits			queryWait				= m_parameters.queryResultWithAvailability ? VK_QUERY_RESULT_WITH_AVAILABILITY_BIT : VK_QUERY_RESULT_WAIT_BIT;

	Move<VkBuffer>						queryPoolResultsBuffer;
	de::MovePtr<Allocation>				queryPoolResultsBufferAlloc;

	tcu::TestLog&						log						= m_context.getTestContext().getLog();

	DE_ASSERT(numVerticesInBuffer * bytesPerVertex == m_parameters.bufferSize);

	if (m_parameters.testType == TEST_TYPE_QUERY_COPY || m_parameters.testType == TEST_TYPE_QUERY_COPY_STRIDE_ZERO)
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,       // VkStructureType      sType;
			DE_NULL,                                    // const void*          pNext;
			0u,                                         // VkBufferCreateFlags  flags;
			queryDataSize,                              // VkDeviceSize         size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,           // VkBufferUsageFlags   usage;
			VK_SHARING_MODE_EXCLUSIVE,                  // VkSharingMode        sharingMode;
			1u,                                         // deUint32             queueFamilyCount;
			&queueFamilyIndex                           // const deUint32*      pQueueFamilyIndices;
		};

		queryPoolResultsBuffer = createBuffer(vk, device, &bufferParams);
		queryPoolResultsBufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *queryPoolResultsBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(device, *queryPoolResultsBuffer, queryPoolResultsBufferAlloc->getMemory(), queryPoolResultsBufferAlloc->getOffset()));
	}

	beginCommandBuffer(vk, *cmdBuffer);
	{
		if (m_parameters.testType != TEST_TYPE_QUERY_RESET)
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, queryIndex, queryCountersNumber);

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0u, 1u, &*tfBuf, &tfBufBindingOffset, &tfBufBindingSize);

			if (m_parameters.streamId == 0 && m_parameters.streamId0Mode != STREAM_ID_0_BEGIN_QUERY_INDEXED)
				vk.cmdBeginQuery(*cmdBuffer, *queryPool, queryIndex, 0u);
			else
				vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex, 0u, m_parameters.streamId);
			{
				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				{
					vk.cmdDraw(*cmdBuffer, static_cast<deUint32>(numVerticesToWrite), 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			}
			if (m_parameters.streamId == 0 && m_parameters.streamId0Mode != STREAM_ID_0_END_QUERY_INDEXED)
				vk.cmdEndQuery(*cmdBuffer, *queryPool, queryIndex);
			else
				vk.cmdEndQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex, m_parameters.streamId);
		}
		endRenderPass(vk, *cmdBuffer);

		if (m_parameters.testType == TEST_TYPE_QUERY_COPY || m_parameters.testType == TEST_TYPE_QUERY_COPY_STRIDE_ZERO)
		{
			VkDeviceSize copyStride = queryDataSize;
			if (queryCountersNumber == 1u && m_parameters.testType == TEST_TYPE_QUERY_COPY_STRIDE_ZERO)
				copyStride = 0;

			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, queryIndex, queryCountersNumber, *queryPoolResultsBuffer, 0u, copyStride, (queryWait | queryExtraFlags));

			const VkBufferMemoryBarrier bufferBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
				DE_NULL,									// const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
				*queryPoolResultsBuffer,					// VkBuffer			buffer;
				0ull,										// VkDeviceSize		offset;
				VK_WHOLE_SIZE								// VkDeviceSize		size;
			};
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
		}

	}
	endCommandBuffer(vk, *cmdBuffer);

	if (m_parameters.testType == TEST_TYPE_QUERY_RESET)
		vk.resetQueryPool(device, *queryPool, queryIndex, queryCountersNumber);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	{
		union Results
		{
			deUint32	elements32[queryResultElements];
			deUint64	elements64[queryResultElements];
		};

		std::vector<deUint8>	queryData		(queryDataSize, 0u);
		const Results*			queryResults	= reinterpret_cast<Results*>(queryData.data());

		if (m_parameters.testType != TEST_TYPE_QUERY_COPY && m_parameters.testType != TEST_TYPE_QUERY_COPY_STRIDE_ZERO)
		{
			vk.getQueryPoolResults(device, *queryPool, queryIndex, queryCountersNumber, queryDataSize, queryData.data(), queryDataSize, (queryWait | queryExtraFlags));
		}
		else
		{
			invalidateAlloc(vk, device, *queryPoolResultsBufferAlloc);
			deMemcpy(queryData.data(), queryPoolResultsBufferAlloc->getHostPtr(), queryData.size());
		}

		// Query results not available
		if (*reinterpret_cast<deUint32*>(&queryData[queryDataSize - queryResultWidth]) == 0)
			return tcu::TestStatus::pass("Pass");

		// The number of primitives successfully written to the corresponding transform feedback buffer.
		const deUint64	numPrimitivesWritten	= (m_parameters.query64bits ? queryResults->elements64[0] : queryResults->elements32[0]);

		// The number of primitives output to the vertex stream.
		const deUint64	numPrimitivesNeeded		= (m_parameters.query64bits ? queryResults->elements64[1] : queryResults->elements32[1]);

		// Count how many primitives we should get by using selected topology.
		const auto		primitivesInBuffer		= topologyData.at(m_parameters.primTopology).getNumPrimitives(numVerticesInBuffer);
		const auto		primitivesToWrite		= topologyData.at(m_parameters.primTopology).getNumPrimitives(numVerticesToWrite);

		log << tcu::TestLog::Message << "Primitives Written / Expected :  " << de::toString(numPrimitivesWritten) << " / " << de::toString(primitivesInBuffer) << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Primitives  Needed / Expected :  " << de::toString(numPrimitivesNeeded) << " / " << de::toString(primitivesToWrite) << tcu::TestLog::EndMessage;

		if (numPrimitivesWritten != primitivesInBuffer)
			return tcu::TestStatus::fail("numPrimitivesWritten=" + de::toString(numPrimitivesWritten) + " while expected " + de::toString(primitivesInBuffer));

		if (numPrimitivesNeeded != primitivesToWrite)
			return tcu::TestStatus::fail("numPrimitivesNeeded=" + de::toString(numPrimitivesNeeded) + " while expected " + de::toString(primitivesToWrite));
	}

	if (m_parameters.testType == TEST_TYPE_QUERY_RESET)
	{
		constexpr deUint32		queryResetElements		= queryResultElements + 1; // For the availability bit.

		union Results
		{
			deUint32	elements32[queryResetElements];
			deUint64	elements64[queryResetElements];
		};

		const deUint32			queryDataAvailSize		(static_cast<deUint32>(queryResetElements * queryResultWidth));
		std::vector<deUint8>	queryData				(queryDataAvailSize, 0u);
		Results*				queryResults			= reinterpret_cast<Results*>(queryData.data());

		// Initialize values
		if (m_parameters.query64bits)
		{
			queryResults->elements64[0] = 1u;	// numPrimitivesWritten
			queryResults->elements64[1] = 1u;	// numPrimitivesNeeded
			queryResults->elements64[2] = 1u;	// Availability bit
		}
		else
		{
			queryResults->elements32[0] = 1u;	// numPrimitivesWritten
			queryResults->elements32[1] = 1u;	// numPrimitivesNeeded
			queryResults->elements32[2] = 1u;	// Availability bit
		}

		vk.resetQueryPool(device, *queryPool, queryIndex, queryCountersNumber);

		vk::VkResult	res						= vk.getQueryPoolResults(device, *queryPool, queryIndex, queryCountersNumber, queryDataAvailSize, queryData.data(), queryDataAvailSize, (vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | queryExtraFlags));
		const deUint64	numPrimitivesWritten	= (m_parameters.query64bits ? queryResults->elements64[0] : queryResults->elements32[0]);
		const deUint64	numPrimitivesNeeded		= (m_parameters.query64bits ? queryResults->elements64[1] : queryResults->elements32[1]);
		const deUint64	availabilityState		= (m_parameters.query64bits ? queryResults->elements64[2] : queryResults->elements32[2]);

		/* From the Vulkan spec:
			*
			* If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
			* for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
			* However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
			*/
		if (res != vk::VK_NOT_READY || availabilityState != 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
	    if (numPrimitivesWritten != 1u || numPrimitivesNeeded != 1u)
			return tcu::TestStatus::fail("QueryPoolResults data was modified");

	}

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackMultiQueryTestInstance : public TransformFeedbackTestInstance
{
public:
								TransformFeedbackMultiQueryTestInstance	(Context& context, const TestParameters& parameters);

protected:
	std::vector<VkDeviceSize>	generateSizesList							(const size_t bufBytes, const size_t chunkCount);
	void						verifyTransformFeedbackBuffer				(const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes, const deUint32 bufOffset, const float expected);
	tcu::TestStatus				iterate										(void);
};

TransformFeedbackMultiQueryTestInstance::TransformFeedbackMultiQueryTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;
	const deUint32											tfBuffersSupported			= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32											tfBuffersRequired			= m_parameters.partCount;
	const deUint32											bytesPerVertex				= m_parameters.bufferSize / m_parameters.partCount;
	const deUint32											tfStreamDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackStreamDataSize;
	const deUint32											tfBufferDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize;
	const deUint32											tfBufferDataStrideSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride;

	DE_ASSERT(m_parameters.partCount == 2u);

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());

	if (tfStreamDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreamDataSize=" + de::toString(tfStreamDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataSize=" + de::toString(tfBufferDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataStrideSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataStride=" + de::toString(tfBufferDataStrideSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (m_transformFeedbackProperties.transformFeedbackQueries == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackQueries feature is not supported");
}

std::vector<VkDeviceSize> TransformFeedbackMultiQueryTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const VkDeviceSize			chunkSize	= bufBytes / chunkCount;
	std::vector<VkDeviceSize>	result		(chunkCount, chunkSize);

	DE_ASSERT(chunkSize * chunkCount == bufBytes);
	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(chunkCount > 0);
	DE_ASSERT(result.size() == chunkCount);

	return result;
}

void TransformFeedbackMultiQueryTestInstance::verifyTransformFeedbackBuffer (const DeviceHelper& deviceHelper, const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes, const deUint32 bufOffset, const float expected)
{
	const DeviceInterface&	vk			= deviceHelper.getDeviceInterface();
	const VkDevice			device		= deviceHelper.getDevice();
	const deUint32			numPoints	= bufBytes / static_cast<deUint32>(sizeof(float));
	const deUint8*			tfDataRaw	= getInvalidatedHostPtr<deUint8>(vk, device, *bufAlloc);
	const float*			tfData		= reinterpret_cast<const float*>(&tfDataRaw[bufOffset]);

	for (deUint32 i = 0; i < numPoints; ++i)
		if (tfData[i] != expected)
			TCU_FAIL(std::string("Failed at item ") + de::toString(i) + " received:" + de::toString(tfData[i]) + " expected:" + de::toString(expected));
}

tcu::TestStatus TransformFeedbackMultiQueryTestInstance::iterate (void)
{
	const auto&									deviceHelper				= getDeviceHelper(m_context, m_parameters);
	const auto&									vki							= m_context.getInstanceInterface();
	const auto									physicalDevice				= m_context.getPhysicalDevice();
	const DeviceInterface&						vk							= deviceHelper.getDeviceInterface();
	const VkDevice								device						= deviceHelper.getDevice();
	const deUint32								queueFamilyIndex			= deviceHelper.getQueueFamilyIndex();
	const std::vector<deUint32>					queueFamilyIndices			= { queueFamilyIndex };
	const VkQueue								queue						= deviceHelper.getQueue();
	Allocator&									allocator					= deviceHelper.getAllocator();

	const Unique<VkRenderPass>					renderPass					(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));

	const ShaderWrapper							vertModule					(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper							geomModule					(vk, device, m_context.getBinaryCollection().get("geom"), 0u);
	const ShaderWrapper							kNullModule;

	const Unique<VkFramebuffer>					framebuffer					(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto									pipelineLayout				(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto									pipeline					(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertModule, kNullModule, kNullModule, geomModule, kNullModule, m_imageExtent2D, 0u));
	const Unique<VkCommandPool>					cmdPool						(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>				cmdBuffer					(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo					tfBufCreateInfo				= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>						tfBuf						= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>					tfBufArray					= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>					tfBufAllocation				= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier						tfMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>				tfBufBindingSizes			= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>				tfBufBindingOffsets			= generateOffsetsList(tfBufBindingSizes);
	const std::vector<float>					tfBufExpectedValues			= { 0.5f, 0.5f + float(m_parameters.streamId) };
	const deUint32								maxBufferSizeBytes			= static_cast<deUint32>(*std::max_element(tfBufBindingSizes.begin(), tfBufBindingSizes.end()));
	const deUint32								bytesPerVertex				= static_cast<deUint32>(4 * sizeof(float));
	const deUint32								numVerticesInBuffer			= maxBufferSizeBytes / bytesPerVertex;
	const deUint32								numDrawVertices				= numVerticesInBuffer / 2;

	const deUint32								queryIndex					= 0u;
	const deUint32								queryCountersNumber			= 2u;
	const deUint32								queryStride					= sizeof(TransformFeedbackQuery);
	const deUint32								queryDataSize				= queryCountersNumber * queryStride;
	const VkQueryPoolCreateInfo					queryPoolCreateInfo			= makeQueryPoolCreateInfo(queryCountersNumber);
	const Unique<VkQueryPool>					queryPool					(createQueryPool(vk, device, &queryPoolCreateInfo));
	const deUint32								queryInvalidCounterValue	= 999999u;
	std::vector<TransformFeedbackQuery>			queryResultData				(queryCountersNumber, TransformFeedbackQuery{ queryInvalidCounterValue, queryInvalidCounterValue });
	const std::vector<TransformFeedbackQuery>	queryExpectedData			({ TransformFeedbackQuery{ numVerticesInBuffer, 3 * numDrawVertices }, TransformFeedbackQuery{ numDrawVertices, numDrawVertices } });

	const VkBufferCreateInfo					queryBufferCreateInfo		= makeBufferCreateInfo(queryDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, queueFamilyIndices);
	const Move<VkBuffer>						queryPoolResultsBuffer		= createBuffer(vk, device, &queryBufferCreateInfo);
	const MovePtr<Allocation>					queryPoolResultsBufferAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, device, *queryPoolResultsBuffer), MemoryRequirement::HostVisible);

	DE_ASSERT(queryCountersNumber == queryExpectedData.size());

	VK_CHECK(vk.bindBufferMemory(device, *queryPoolResultsBuffer, queryPoolResultsBufferAlloc->getMemory(), queryPoolResultsBufferAlloc->getOffset()));

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		vk.cmdResetQueryPool(*cmdBuffer, *queryPool, queryIndex, queryCountersNumber);

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0u, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex + 0, 0u, 0u);
			vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex + 1, 0u, m_parameters.streamId);
			{
				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				{
					vk.cmdDraw(*cmdBuffer, numDrawVertices, 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			}
			vk.cmdEndQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex + 1, m_parameters.streamId);
			vk.cmdEndQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex + 0, 0);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	vk.getQueryPoolResults(device, *queryPool, queryIndex, queryCountersNumber, queryDataSize, queryResultData.data(), queryStride, (vk::VK_QUERY_RESULT_WAIT_BIT));

	DE_ASSERT(queryResultData.size() == queryCountersNumber && queryExpectedData.size() == queryCountersNumber);
	DE_ASSERT(queryCountersNumber > 0);

	for (size_t counterNdx = 0; counterNdx < queryCountersNumber; ++counterNdx)
	{
		const TransformFeedbackQuery&	result		= queryResultData[counterNdx];
		const TransformFeedbackQuery&	expected	= queryExpectedData[counterNdx];

		DE_ASSERT(expected.written != queryInvalidCounterValue);
		DE_ASSERT(expected.attempts != queryInvalidCounterValue);

		if (result.written == queryInvalidCounterValue || result.attempts == queryInvalidCounterValue)
			return tcu::TestStatus::fail("Query counters read failed");

		if (result.written != expected.written)
		{
			const std::string	comment	= "At counter " + de::toString(counterNdx) + " vertices written " + de::toString(result.written) + ", while expected " + de::toString(expected.written);

			return tcu::TestStatus::fail(comment.c_str());
		}


		if (result.attempts != expected.attempts)
		{
			const std::string	comment = "At counter " + de::toString(counterNdx) + " attempts committed " + de::toString(result.attempts) + ", while expected " + de::toString(expected.attempts);

			return tcu::TestStatus::fail(comment.c_str());
		}

		if (counterNdx == 0 && !m_parameters.omitShaderWrite)
			verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, bytesPerVertex * expected.written, static_cast<deUint32>(tfBufBindingOffsets[counterNdx]), tfBufExpectedValues[counterNdx]);
	}

	return tcu::TestStatus::pass("Pass");
}


class TransformFeedbackLinesOrTrianglesTestInstance : public TransformFeedbackTestInstance
{
public:
								TransformFeedbackLinesOrTrianglesTestInstance	(Context& context, const TestParameters& parameters);

protected:
	std::vector<VkDeviceSize>	generateSizesList								(const size_t bufBytes, const size_t chunkCount);
	void						verifyTransformFeedbackBufferLines				(const DeviceHelper&			deviceHelper,
																				 const MovePtr<Allocation>&		bufAlloc,
																				 const deUint32					bufBytes,
																				 const std::vector<deUint32>&	primitives,
																				 const deUint32					invocationCount,
																				 const deUint32					partCount);
	void						verifyTransformFeedbackBufferTriangles			(const DeviceHelper&			deviceHelper,
																				 const MovePtr<Allocation>&		bufAlloc,
																				 const deUint32					bufBytes,
																				 const std::vector<deUint32>&	primitives,
																				 const deUint32					invocationCount,
																				 const deUint32					partCount);
	tcu::TestStatus				iterate											(void);
};

TransformFeedbackLinesOrTrianglesTestInstance::TransformFeedbackLinesOrTrianglesTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&							vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice								physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures						features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&	transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32										streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32										streamsRequired				= m_parameters.streamId + 1;
	const deUint32										tfBuffersSupported			= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32										tfBuffersRequired			= m_parameters.partCount;

	DE_ASSERT(m_parameters.partCount == 2u);

	if (m_transformFeedbackProperties.transformFeedbackStreamsLinesTriangles == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackStreamsLinesTriangles required");

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());
}

std::vector<VkDeviceSize> TransformFeedbackLinesOrTrianglesTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const VkDeviceSize			chunkSize	= bufBytes / chunkCount;
	std::vector<VkDeviceSize>	result		(chunkCount, chunkSize);

	DE_ASSERT(chunkSize * chunkCount == bufBytes);
	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(chunkCount > 0);
	DE_ASSERT(result.size() == chunkCount);

	return result;
}

void TransformFeedbackLinesOrTrianglesTestInstance::verifyTransformFeedbackBufferLines (const DeviceHelper&				deviceHelper,
																						const MovePtr<Allocation>&		bufAlloc,
																						const deUint32					bufBytes,
																						const std::vector<deUint32>&	primitives,
																						const deUint32					invocationCount,
																						const deUint32					partCount)
{
	const DeviceInterface&	vk			= deviceHelper.getDeviceInterface();
	const VkDevice			device		= deviceHelper.getDevice();
	const tcu::Vec4*		tfData		= getInvalidatedHostPtr<tcu::Vec4>(vk, device, *bufAlloc);
	const deUint32			stripeCount	= static_cast<deUint32>(primitives.size());
	const deUint32			vertexCount	= 2 * destripedLineCount(primitives) * invocationCount * partCount;
	const deUint32			numPoints	= static_cast<deUint32>(bufBytes / sizeof(tcu::Vec4));
	deUint32				n			= 0;
	std::vector<tcu::Vec4>	reference;

	reference.reserve(vertexCount);

	for (deUint32 partNdx = 0; partNdx < partCount; ++partNdx)
	{
		for (deUint32 invocationNdx = 0; invocationNdx < invocationCount; ++invocationNdx)
		{
			for (deUint32 stripeNdx = 0; stripeNdx < stripeCount; ++stripeNdx)
			{
				const deUint32 stripeVertexCount = primitives[stripeNdx];

				for (deUint32 vertexNdx = 0; vertexNdx < stripeVertexCount; ++vertexNdx)
				{
					const bool		firstOrLast	= vertexNdx == 0 || vertexNdx == stripeVertexCount - 1;
					const tcu::Vec4 v			(float(n++), float(invocationNdx), float(stripeNdx), float(vertexNdx));

					reference.push_back(v);

					if (!firstOrLast)
						reference.push_back(v);
				}
			}
		}
	}

	DE_ASSERT(reference.size() == numPoints);

	const tcu::Vec4 threshold (0.0001f, 0.0001f, 0.0001f, 0.0001f);
	const auto errors = verifyVertexDataWithWinding(reference, tfData, numPoints, 2u, threshold);
	checkErrorVec(m_context.getTestContext().getLog(), errors);
}

void TransformFeedbackLinesOrTrianglesTestInstance::verifyTransformFeedbackBufferTriangles (const DeviceHelper&				deviceHelper,
																							const MovePtr<Allocation>&		bufAlloc,
																							const deUint32					bufBytes,
																							const std::vector<deUint32>&	primitives,
																							const deUint32					invocationCount,
																							const deUint32					partCount)
{
	const DeviceInterface&	vk			= deviceHelper.getDeviceInterface();
	const VkDevice			device		= deviceHelper.getDevice();
	const tcu::Vec4*		tfData		= getInvalidatedHostPtr<tcu::Vec4>(vk, device, *bufAlloc);
	const deUint32			stripeCount	= static_cast<deUint32>(primitives.size());
	const deUint32			vertexCount	= 3 * destripedLineCount(primitives) * invocationCount * partCount;
	const deUint32			numPoints	= static_cast<deUint32>(bufBytes / sizeof(tcu::Vec4));
	deUint32				n			= 0;
	std::vector<tcu::Vec4>	reference;

	reference.reserve(vertexCount);

	for (deUint32 partNdx = 0; partNdx < partCount; ++partNdx)
	{
		for (deUint32 invocationNdx = 0; invocationNdx < invocationCount; ++invocationNdx)
		{
			for (deUint32 stripeNdx = 0; stripeNdx < stripeCount; ++stripeNdx)
			{
				const deUint32			stripeVertexCount	= primitives[stripeNdx];
				const deUint32			trianglesCount		= stripeVertexCount - 2;
				std::vector<tcu::Vec4>	stripe				(stripeVertexCount);

				for (deUint32 vertexNdx = 0; vertexNdx < stripeVertexCount; ++vertexNdx)
					stripe[vertexNdx] = tcu::Vec4(float(n++), float(invocationNdx), float(stripeNdx), float(vertexNdx));

				for (deUint32 triangleNdx = 0; triangleNdx < trianglesCount; ++triangleNdx)
				{
					if (triangleNdx % 2 == 0)
					{
						reference.push_back(stripe[triangleNdx + 0]);
						reference.push_back(stripe[triangleNdx + 1]);
						reference.push_back(stripe[triangleNdx + 2]);
					}
					else
					{
						reference.push_back(stripe[triangleNdx + 0]);
						reference.push_back(stripe[triangleNdx + 2]);
						reference.push_back(stripe[triangleNdx + 1]);
					}
				}
			}
		}
	}

	DE_ASSERT(reference.size() == numPoints);

	const tcu::Vec4 threshold (0.0001f, 0.0001f, 0.0001f, 0.0001f);
	const auto errors = verifyVertexDataWithWinding(reference, tfData, numPoints, 3u, threshold);
	checkErrorVec(m_context.getTestContext().getLog(), errors);
}

tcu::TestStatus TransformFeedbackLinesOrTrianglesTestInstance::iterate (void)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&							vki						= m_context.getInstanceInterface();
	const auto							physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue					= deviceHelper.getQueue();
	Allocator&							allocator				= deviceHelper.getAllocator();

	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));

	const ShaderWrapper					vertexModule			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					geomModule				(vk, device, m_context.getBinaryCollection().get("geom"), 0u);
	const ShaderWrapper					kNullModule;

	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout			(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule, kNullModule, kNullModule, geomModule, kNullModule, m_imageExtent2D, 0u, &m_parameters.streamId));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const deUint32						tfBufferSize			= m_parameters.bufferSize;
	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(tfBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>			tfBufArray				= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= generateSizesList(tfBufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0u, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, INVOCATION_COUNT, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	switch (m_parameters.primTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:		verifyTransformFeedbackBufferLines(deviceHelper, tfBufAllocation, tfBufferSize, LINES_LIST, INVOCATION_COUNT, m_parameters.partCount);			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:	verifyTransformFeedbackBufferTriangles(deviceHelper, tfBufAllocation, tfBufferSize, TRIANGLES_LIST, INVOCATION_COUNT, m_parameters.partCount);	break;
		default:									TCU_THROW(InternalError, "Unknown topology");
	}

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackDrawOutsideTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackDrawOutsideTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate										(void);
};

TransformFeedbackDrawOutsideTestInstance::TransformFeedbackDrawOutsideTestInstance(Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
}

tcu::TestStatus TransformFeedbackDrawOutsideTestInstance::iterate (void)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_parameters);
	const auto&							vki						= m_context.getInstanceInterface();
	const auto							physicalDevice			= m_context.getPhysicalDevice();
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const deUint32						queueFamilyIndex		= deviceHelper.getQueueFamilyIndex();
	const VkQueue						queue					= deviceHelper.getQueue();
	Allocator&							allocator				= deviceHelper.getAllocator();

	const ShaderWrapper					vertexModule1			(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const ShaderWrapper					vertexModule2			(vk, device, m_context.getBinaryCollection().get("vert2"), 0u);
	const ShaderWrapper					kNullModule;
	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const auto							pipelineLayout			(TransformFeedback::makePipelineLayout	(m_parameters.pipelineConstructionType, vk, device));
	const auto							pipeline1				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule1, kNullModule, kNullModule, kNullModule, kNullModule, m_imageExtent2D, 0u, &m_parameters.streamId));
	const auto							pipeline2				(makeGraphicsPipeline					(m_parameters.pipelineConstructionType, vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), *pipelineLayout, *renderPass, vertexModule2, kNullModule, kNullModule, kNullModule, kNullModule, m_imageExtent2D, 0u, &m_parameters.streamId));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				if (i == 0)
					vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline1->getPipeline());
				else
					vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline2->getPipeline());

				for (deUint32 drawNdx = 0; drawNdx < m_parameters.partCount; ++drawNdx)
				{
					const deUint32	startValue = static_cast<deUint32>(tfBufBindingOffsets[drawNdx] / sizeof(deUint32));
					const deUint32	numPoints = static_cast<deUint32>(tfBufBindingSizes[drawNdx] / sizeof(deUint32));

					vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffsets[drawNdx], &tfBufBindingSizes[drawNdx]);

					vk.cmdPushConstants(*cmdBuffer, pipelineLayout->get(), VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

					if (i == 0)
						vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
					{
						vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
					}
					if (i == 0)
						vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				}
			}
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(deviceHelper, tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackHolesInstance : public vkt::TestInstance
{
public:
						TransformFeedbackHolesInstance	(Context& context, const bool extraDraw)
							: vkt::TestInstance	(context)
							, m_extraDraw		(extraDraw)
							{}
						~TransformFeedbackHolesInstance	(void) {}

	tcu::TestStatus		iterate							(void) override;

protected:
	const bool m_extraDraw;
};

tcu::TestStatus TransformFeedbackHolesInstance::iterate (void)
{
	const auto&			ctx				= m_context.getContextCommonData();
	const tcu::IVec3	fbExtent		(1, 1, 1);
	const auto			vkExtent		= makeExtent3D(fbExtent);
	const auto			fbFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuFormat		= mapVkFormat(fbFormat);
	const auto			fbUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const tcu::Vec4		clearColor		(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4		geomColor		(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader values.
	const tcu::Vec4		threshold		(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
	const auto			bindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	const auto&			binaries		= m_context.getBinaryCollection();
	const bool			hasGeom			= binaries.contains("geom");
	const auto			dataStages		= (hasGeom ? VK_SHADER_STAGE_GEOMETRY_BIT : VK_SHADER_STAGE_VERTEX_BIT);
	const auto			xfbCompCount	= 3u; // Per vertex.
	const auto			xfbChunkSize	= xfbCompCount * sizeof(float); // Per vertex, in bytes.
	const auto			totalDraws		= (m_extraDraw ? 2u : 1u);

	// Color buffer with verification buffer.
	ImageWithBuffer colorBuffer (
		ctx.vkd,
		ctx.device,
		ctx.allocator,
		vkExtent,
		fbFormat,
		fbUsage,
		VK_IMAGE_TYPE_2D);

	// Vertices.
	const std::vector<tcu::Vec4> vertices { tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f) };

	// Vertex buffer.
	const auto			vbSize			= static_cast<VkDeviceSize>(de::dataSize(vertices));
	const auto			vbInfo			= makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	BufferWithMemory	vertexBuffer	(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
	const auto			vbAlloc			= vertexBuffer.getAllocation();
	void*				vbData			= vbAlloc.getHostPtr();
	const auto			vbOffset		= static_cast<VkDeviceSize>(0);

	deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
	flushAlloc(ctx.vkd, ctx.device, vbAlloc);

	// XFB buffer. When using an extra draw, leave space for a possible second draw (NB: but it should not be recorded, see below).
	const auto			xfbSizeFactor	= static_cast<VkDeviceSize>(totalDraws);
	const auto			xfbBufferSize	= static_cast<VkDeviceSize>(xfbChunkSize * vertices.size()) * xfbSizeFactor;
	const auto			xfbBufferInfo	= makeBufferCreateInfo(xfbBufferSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	BufferWithMemory	xfbBuffer		(ctx.vkd, ctx.device, ctx.allocator, xfbBufferInfo, MemoryRequirement::HostVisible);
	const auto			xfbBufferAlloc	= xfbBuffer.getAllocation();
	void*				xfbBufferData	= xfbBufferAlloc.getHostPtr();
	const auto			xfbBufferOffset	= static_cast<VkDeviceSize>(0);

	deMemset(xfbBufferData, 0, static_cast<size_t>(xfbBufferSize));
	flushAlloc(ctx.vkd, ctx.device, xfbBufferAlloc);

	// Push constants.
	const tcu::Vec3				pcData	{ 10.0, 20.0, 30.0 }; // Must match the expected values in the frag shader.
	const auto					pcSize	= static_cast<uint32_t>(sizeof(pcData));
	const auto					pcRange	= makePushConstantRange(dataStages, 0u, pcSize);

	const auto pipelineLayout	= makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);
	const auto renderPass		= makeRenderPass(ctx.vkd, ctx.device, fbFormat);
	const auto framebuffer		= makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

	// Modules.
	const auto	vertModule		= createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto	geomModule		= (hasGeom ? createShaderModule(ctx.vkd, ctx.device, binaries.get("geom")) : Move<VkShaderModule>());
	const auto	fragModule		= createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

	const std::vector<VkViewport>	viewports	(1u, makeViewport(vkExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(vkExtent));

	const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout,
		*vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, *geomModule, *fragModule,
		*renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = *cmd.cmdBuffer;

	beginCommandBuffer(ctx.vkd, cmdBuffer);
	beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
	ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
	ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
	ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, dataStages, 0u, pcSize, &pcData);
	ctx.vkd.cmdBindTransformFeedbackBuffersEXT(cmdBuffer, 0u, 1u, &xfbBuffer.get(), &xfbBufferOffset, &xfbBufferSize);
	ctx.vkd.cmdBeginTransformFeedbackEXT(cmdBuffer, 0u, 0u, nullptr, nullptr);
	ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
	ctx.vkd.cmdEndTransformFeedbackEXT(cmdBuffer, 0u, 0u, nullptr, nullptr);
	if (m_extraDraw)
	{
		// When m_extraDraw is true, record a new draw outside the transform feedback section. The XFB buffer will have enough space
		// to record this draw, but it should not be recorded, obviously, so the values in the buffer should stay zero. We are also
		// avoiding any state changes between both draws.
		ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
	}
	endRenderPass(ctx.vkd, cmdBuffer);
	const auto xfbBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	ctx.vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &xfbBarrier, 0u, nullptr, 0u, nullptr);
	copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(),
		fbExtent.swizzle(0, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
		VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	endCommandBuffer(ctx.vkd, cmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

	// Verify color output.
	invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
	tcu::PixelBufferAccess resultAccess (tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

	tcu::TextureLevel	referenceLevel	(tcuFormat, fbExtent.x(), fbExtent.y());
	auto				referenceAccess	= referenceLevel.getAccess();
	tcu::clear(referenceAccess, geomColor);

	auto& log = m_context.getTestContext().getLog();
	if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

	// Verify XFB buffer.
	const tcu::Vec3	refRecordedValues	{ pcData.x(), 0.0f, pcData.z() };	// Per-vertex, must match vert/geom shader, note Y is not saved.
	const tcu::Vec3	refEmptyValues		{ 0.0f, 0.0f, 0.0f };				// For empty areas of the XFB buffer.
	const auto		dataPtr				= reinterpret_cast<const char*>(xfbBufferData);

	for (uint32_t drawIdx = 0u; drawIdx < totalDraws; ++drawIdx)
	{
		const auto& refValues = ((drawIdx > 0u) ? refEmptyValues : refRecordedValues);
		for (size_t vertIdx = 0u; vertIdx < vertices.size(); ++vertIdx)
		{
			const auto	vertexDataPtr	= dataPtr + (vertIdx * xfbChunkSize) + (drawIdx * vertices.size() * xfbChunkSize);
			tcu::Vec3	vertValues		(0.0f, 0.0f, 0.0f);
			deMemcpy(&vertValues, vertexDataPtr, sizeof(vertValues));

			if (vertValues != refValues)
			{
				std::ostringstream msg;
				msg << "Invalid data found for vertex " << vertIdx << ": expected " << refRecordedValues << " and found " << vertValues;
				TCU_FAIL(msg.str());
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackTestCase : public vkt::TestCase
{
public:
						TransformFeedbackTestCase	(tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters);

protected:
	vkt::TestInstance*	createInstance				(vkt::Context& context) const;
	void				initPrograms				(SourceCollections& programCollection) const;
	virtual void		checkSupport				(Context& context) const;

	TestParameters		m_parameters;
};

TransformFeedbackTestCase::TransformFeedbackTestCase (tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters)
	: TestCase		(context, name, description)
	, m_parameters	(parameters)
{
}

std::string vectorToString (const std::vector<deUint32>& v)
{
	std::ostringstream css;

	DE_ASSERT(!v.empty());

	for (auto x: v)
		css << x << ",";

	return css.str().substr(0, css.str().size() - 1);
}

vkt::TestInstance*	TransformFeedbackTestCase::createInstance (vkt::Context& context) const
{
	if (m_parameters.testType == TEST_TYPE_BASIC)
		return new TransformFeedbackBasicTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_RESUME)
		return new TransformFeedbackResumeTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_XFB_POINTSIZE)
		return new TransformFeedbackBuiltinTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE)
		return new TransformFeedbackBuiltinTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE)
		return new TransformFeedbackBuiltinTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL)
		return new TransformFeedbackBuiltinTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_WINDING)
		return new TransformFeedbackWindingOrderTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_STREAMS)
		return new TransformFeedbackStreamsTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE)
		return new TransformFeedbackStreamsTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE)
		return new TransformFeedbackStreamsTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE)
		return new TransformFeedbackStreamsTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_MULTISTREAMS)
		return new TransformFeedbackMultistreamTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_MULTISTREAMS_SAME_LOCATION)
		return new TransformFeedbackMultistreamSameLocationTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_DRAW_INDIRECT)
		return new TransformFeedbackIndirectDrawTestInstance(context, m_parameters, false);

	if (m_parameters.testType == TEST_TYPE_DRAW_INDIRECT_MULTIVIEW)
		return new TransformFeedbackIndirectDrawTestInstance(context, m_parameters, true);

	if (m_parameters.testType == TEST_TYPE_BACKWARD_DEPENDENCY || m_parameters.testType == TEST_TYPE_BACKWARD_DEPENDENCY_INDIRECT)
		return new TransformFeedbackBackwardDependencyTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_QUERY_GET				||
		m_parameters.testType == TEST_TYPE_QUERY_COPY				||
		m_parameters.testType == TEST_TYPE_QUERY_COPY_STRIDE_ZERO	||
		m_parameters.testType == TEST_TYPE_QUERY_RESET)
		return new TransformFeedbackQueryTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_MULTIQUERY)
		return new TransformFeedbackMultiQueryTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_VERTEX	||
		m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_GEOMETRY	||
		m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_TESE)
		return new TransformFeedbackDepthClipControlTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_LINES_TRIANGLES)
		return new TransformFeedbackLinesOrTrianglesTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_DRAW_OUTSIDE)
		return new TransformFeedbackDrawOutsideTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_HOLES_VERTEX || m_parameters.testType == TEST_TYPE_HOLES_GEOMETRY)
	{
		// We repurpose partCount to indicate somehow the number of draws.
		const bool extraDraw = (m_parameters.partCount > 1u);
		return new TransformFeedbackHolesInstance (context, extraDraw);
	}

	TCU_THROW(InternalError, "Specified test type not found");
}

void TransformFeedbackTestCase::checkSupport (Context& context) const
{
	context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_parameters.pipelineConstructionType);

	context.requireDeviceFunctionality("VK_EXT_transform_feedback");

	if (context.getTransformFeedbackFeaturesEXT().transformFeedback == VK_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedback feature is not supported");

	if (m_parameters.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	// transformFeedbackRasterizationStreamSelect is required when vertex streams other than zero are rasterized
	if (m_parameters.requireRastStreamSelect && (context.getTransformFeedbackPropertiesEXT().transformFeedbackRasterizationStreamSelect == VK_FALSE) && (m_parameters.streamId > 0))
		TCU_THROW(NotSupportedError, "transformFeedbackRasterizationStreamSelect property is not supported");

	if (m_parameters.testType == TEST_TYPE_DRAW_INDIRECT_MULTIVIEW)
	{
		const auto& features = context.getMultiviewFeatures();
		if (!features.multiview)
			TCU_THROW(NotSupportedError, "multiview not supported");
	}

	if (m_parameters.testType == TEST_TYPE_BACKWARD_DEPENDENCY_INDIRECT)
		context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");

	if (m_parameters.testType == TEST_TYPE_HOLES_GEOMETRY)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (m_parameters.pointSize > 1u)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_LARGE_POINTS);

	if (m_parameters.usingGeom())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (m_parameters.usingTess())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	const auto& coreFeatures = context.getDeviceFeatures();

	if (m_parameters.pointSizeWanted() && m_parameters.usingTessGeom() && !coreFeatures.shaderTessellationAndGeometryPointSize)
		TCU_THROW(NotSupportedError, "shaderTessellationAndGeometryPointSize not supported");

	if (m_parameters.testType == TEST_TYPE_QUERY_RESET)
		context.requireDeviceFunctionality("VK_EXT_host_query_reset");
}

void TransformFeedbackTestCase::initPrograms (SourceCollections& programCollection) const
{
	const bool backwardDependency	= (m_parameters.testType == TEST_TYPE_BACKWARD_DEPENDENCY
									|| m_parameters.testType == TEST_TYPE_BACKWARD_DEPENDENCY_INDIRECT);
	const bool vertexShaderOnly		=  m_parameters.testType == TEST_TYPE_BASIC
									|| m_parameters.testType == TEST_TYPE_RESUME
									|| (m_parameters.testType == TEST_TYPE_WINDING && m_parameters.primTopology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
	const bool requiresFullPipeline	= m_parameters.requiresFullPipeline();
	const bool xfbBuiltinPipeline	=  m_parameters.testType == TEST_TYPE_XFB_POINTSIZE
									|| m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE
									|| m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE
									|| m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL;
	const bool pointSizeWanted		= m_parameters.pointSizeWanted();
	const auto pointSizeStr			= std::to_string(m_parameters.pointSize);

	if (m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_VERTEX)
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0) out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4 gl_Position;\n"
				<< (pointSizeWanted ? "    float gl_PointSize;\n" : "")
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position = vec4(1.0, 1.0, float(gl_VertexIndex) / 3.0 - 1.0, 1.0);\n"
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_GEOMETRY)
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4  gl_Position;\n"
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position = vec4(1.0, 1.0, float(gl_VertexIndex) / 3.0 - 1.0, 1.0);\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Geometry shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "layout(points, max_vertices = 1) out;\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0) out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4 gl_Position;\n"
				<< (pointSizeWanted ? "    float gl_PointSize;\n" : "")
				<< "};\n"
				<< "\n"
				<< "in gl_PerVertex\n"
				<< "{\n"
				<< "    vec4  gl_Position;\n"
				<< "} gl_in[];\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position = gl_in[0].gl_Position;\n"
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "    EmitVertex();\n"
				<< "    EndPrimitive();\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_DEPTH_CLIP_CONTROL_TESE)
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4  gl_Position;\n"
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position = vec4(1.0, 1.0, float(gl_VertexIndex) / 3.0 - 1.0, 1.0);\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Tesselation control shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(vertices = 3) out;\n"
				<< "in gl_PerVertex\n"
				<< "{\n"
				<< "    vec4 gl_Position;\n"
				<< "} gl_in[gl_MaxPatchVertices];\n"
				<< "out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4 gl_Position;\n"
				<< "} gl_out[];\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_TessLevelInner[0] = 0.0;\n"
				<< "    gl_TessLevelOuter[0] = 1.0;\n"
				<< "    gl_TessLevelOuter[1] = 1.0;\n"
				<< "    gl_TessLevelOuter[2] = 1.0;\n"
				<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				<< "}\n";
			programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
		}

		// Tessellation evaluation shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(triangles, ccw) in;\n"
				<< "in gl_PerVertex\n"
				<< "{\n"
				<< "    vec4 gl_Position;\n"
				<< "} gl_in[gl_MaxPatchVertices];\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0) out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4 gl_Position;\n"
				<< (pointSizeWanted ? "    float gl_PointSize;\n" : "")
				<< "};\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    vec4 p0 = gl_TessCoord.x * gl_in[0].gl_Position;\n"
				<< "    vec4 p1 = gl_TessCoord.y * gl_in[1].gl_Position;\n"
				<< "    vec4 p2 = gl_TessCoord.z * gl_in[2].gl_Position;\n"
				<< "    gl_Position = p0 + p1 + p2;\n"
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "}\n";
			programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
		}

		return;
	}

	if (vertexShaderOnly)
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(push_constant) uniform pushConstants\n"
				<< "{\n"
				<< "    uint start;\n"
				<< "} uInput;\n"
				<< "\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0) out uint idx_out;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    idx_out = uInput.start + gl_VertexIndex;\n"
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		return;
	}

	if (backwardDependency)
	{
		// Vertex shader.
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(push_constant, std430) uniform PushConstantBlock\n"
				<< "{\n"
				<< "    uint  start;\n"
				<< "    float width;\n"
				<< "    float posY;\n"
				<< "} pc;\n"
				<< "\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0) out uint idx_out;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    idx_out           = pc.start + gl_VertexIndex;\n"
				<< "    const float posX  = ((float(gl_VertexIndex) + 0.5) / pc.width) * 2.0 - 1.0;\n"
				<< "    gl_Position       = vec4(posX, pc.posY, 0.0, 1.0);\n"
				<< "    gl_PointSize      = 1.0;\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Fragment shader.
		{
			std::ostringstream frag;
			frag
				<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout (location=0) out vec4 outColor;\n"
				<< "void main (void) { outColor = vec4(0.0, 0.0, 1.0, 1.0); }\n"
				;
			programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
		}

		return;
	}

	if (m_parameters.primTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(push_constant) uniform pushConstants\n"
				<< "{\n"
				<< "    uint start;\n"
				<< "} uInput;\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";
			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Tesselation control shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(vertices = 3) out;\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_TessLevelInner[0] = 2.0;\n" // generate three triangles out of each patch
				<< "    gl_TessLevelOuter[0] = 1.0;\n"
				<< "    gl_TessLevelOuter[1] = 1.0;\n"
				<< "    gl_TessLevelOuter[2] = 1.0;\n"
				<< "}\n";
			programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
		}

		// Tessellation evaluation shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(triangles, ccw) in;\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0) out uint idx_out;\n"
				<< (pointSizeWanted ? "out gl_PerVertex { float gl_PointSize; };\n" : "")
				<< "\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    idx_out = gl_PrimitiveID;\n" // all vertex generated from patch will have its id
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "}\n";
			programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
		}

		return;
	}

	if (xfbBuiltinPipeline)
	{
		const std::string	outputBuiltIn		= (m_parameters.testType == TEST_TYPE_XFB_POINTSIZE)     ? "float gl_PointSize;\n"
												: (m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE)  ? (std::string("float gl_ClipDistance[") + (pointSizeWanted ? "7" : "8") + "];\n" + (pointSizeWanted ? "float gl_PointSize;\n" : ""))
												: (m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE)  ? (std::string("float gl_CullDistance[") + (pointSizeWanted ? "7" : "8") + "];\n" + (pointSizeWanted ? "float gl_PointSize;\n" : ""))
												: (m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) ? (std::string("float gl_CullDistance[") + (pointSizeWanted ? "4" : "5") + "];\nfloat gl_ClipDistance[1];\n" + (pointSizeWanted ? "float gl_PointSize;\n" : ""))
												: "";
		const std::string	operationBuiltIn	= (m_parameters.testType == TEST_TYPE_XFB_POINTSIZE)     ? "gl_PointSize = float(gl_VertexIndex) / 32768.0f;\n"
												: (m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE)  ? (pointSizeWanted ? "gl_PointSize = " + pointSizeStr + ".0;\n" : "") + std::string("for (int i=0; i<") + (pointSizeWanted ? "7" : "8") + "; i++) gl_ClipDistance[i] = float(8 * gl_VertexIndex + i) / 32768.0f;\n"
												: (m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE)  ? (pointSizeWanted ? "gl_PointSize = " + pointSizeStr + ".0;\n" : "") + std::string("for (int i=0; i<") + (pointSizeWanted ? "7" : "8") + "; i++) gl_CullDistance[i] = float(8 * gl_VertexIndex + i) / 32768.0f;\n"
												: (m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) ? (pointSizeWanted ? "gl_PointSize = " + pointSizeStr + ".0;\n" : "") + std::string("for (int i=0; i<") + (pointSizeWanted ? "4" : "5") + "; i++) gl_CullDistance[i] = float(6 * gl_VertexIndex + i) / 32768.0f;\n"
																										   "gl_ClipDistance[0] = float(6 * gl_VertexIndex + " + (pointSizeWanted ? "4" : "5") + ") / 32768.0f;\n"
												: "";

		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(xfb_buffer = " << m_parameters.partCount - 1 << ", xfb_offset = 0) out gl_PerVertex\n"
				<< "{\n"
				<< outputBuiltIn
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< operationBuiltIn
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_MULTISTREAMS)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		{
			const deUint32		s	= m_parameters.streamId;
			std::ostringstream	src;

			DE_ASSERT(s != 0);

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "\n"
				<< "layout(points, max_vertices = 32) out;\n"
				<< "layout(stream = " << 0 << ", xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
				<< "layout(stream = " << s << ", xfb_buffer = 1, xfb_offset = 0, xfb_stride = 16, location = 1) out vec4 out1;\n"
				<< "\n"
				<< "const int counts[] = int[](1, 1, 2, 4, 8);\n"
				<< "\n"
				<< (pointSizeWanted ? "out gl_PerVertex { float gl_PointSize; };\n\n" : "")
				<< "void main(void)\n"
				<< "{\n"
				<< "    int c0 = 0;\n"
				<< "    int c1 = 0;\n"
				<< "\n"
				<< "    // Start 1st buffer from point where 0th buffer ended\n"
				<< "    for (int i = 0; i < counts.length(); i++)\n"
				<< "        c1 = c1 + 4 * counts[i];\n"
				<< "\n"
				<< "    for (int i = 0; i < counts.length(); i++)\n"
				<< "    {\n"
				<< "        const int n0 = counts[i];\n"
				<< "        const int n1 = counts[counts.length() - 1 - i];\n"
				<< "\n"
				<< "        for (int j = 0; j < n0; j++)\n"
				<< "        {\n"
				<< "            out0 = vec4(ivec4(c0, c0 + 1, c0 + 2, c0 + 3));\n"
				<< "            c0 = c0 + 4;\n"
				<< (pointSizeWanted ? "            gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "            EmitStreamVertex(0);\n"
				<< "            EndStreamPrimitive(0);\n"
				<< "        }\n"
				<< "\n"
				<< "        for (int j = 0; j < n1; j++)\n"
				<< "        {\n"
				<< "            out1 = vec4(ivec4(c1, c1 + 1, c1 + 2, c1 + 3));\n"
				<< "            c1 = c1 + 4;\n"
				<< (pointSizeWanted ? "            gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "            EmitStreamVertex(" << s << ");\n"
				<< "            EndStreamPrimitive(" << s << ");\n"
				<< "        }\n"
				<< "    }\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_MULTISTREAMS_SAME_LOCATION)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location=0) out uint id;"
				<< "void main(void)\n"
				<< "{\n"
				<< "  id = gl_VertexIndex;"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		{
			const deUint32		s	= m_parameters.streamId;
			std::ostringstream	src;

			DE_ASSERT(s != 0);

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "\n"
				<< "layout(points, max_vertices = 2) out;\n"
				<< "\n"
				<< "layout(location=0) in uint id[1];"
				<< "layout(stream = " << 0 << ", xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0, component = 0) out uint out0;\n"
				<< "layout(stream = " << s << ", xfb_buffer = 1, xfb_offset = 0, xfb_stride = 4, location = 0, component = 1) out uint out1;\n"
				<< "\n"
				<< (pointSizeWanted ? "out gl_PerVertex { float gl_PointSize; };\n\n" : "")
				<< "void main(void)\n"
				<< "{\n"
				<< "	out0 = id[0] * 2 + 0;\n"
				<< (pointSizeWanted ? "	gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "	EmitStreamVertex(0);\n"
				<< "	EndStreamPrimitive(0);\n"
				<< "\n"
				<< "	out1 = id[0] * 2 + 1;\n"
				<< (pointSizeWanted ? "	gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "	EmitStreamVertex(" << s << ");\n"
				<< "	EndStreamPrimitive(" << s << ");\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		return;
	}

	if (requiresFullPipeline)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		{
			const deUint32		s					= m_parameters.streamId;
			const bool			requirePoints		= m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE;
			const std::string	outputPrimitiveType	= requirePoints ? "points" : "triangle_strip";
			const std::string	pointSizeDecl		= "    float gl_PointSize;\n";
			const std::string	extraDecl			= (pointSizeWanted ? pointSizeDecl : "");
			const std::string	extraStmt			= (pointSizeWanted ? "gl_PointSize = " + pointSizeStr + ".0; " : "");
			const std::string	outputBuiltIn		= (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE)    ? pointSizeDecl
													: (m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE) ? "    float gl_ClipDistance[];\n" + extraDecl
													: (m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE) ? "    float gl_CullDistance[];\n" + extraDecl
													: extraDecl;
			std::ostringstream	src;

			DE_ASSERT(s != 0);

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "layout(" << outputPrimitiveType << ", max_vertices = 16) out;\n"
				<< "layout(stream = " << s << ") out;\n"
				<< "layout(location = 0) out vec4 color;\n"
				<< "\n"
				<< "layout(stream = " << s << ") out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4 gl_Position;\n"
				<< outputBuiltIn
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    // Color constants\n"
				<< "    vec4 g = vec4(0.0, 1.0, 0.0, 1.0);\n"
				<< "    vec4 m = vec4(1.0, 0.0, 1.0, 1.0);\n"
				<< "    // Coordinate constants: leftmost column\n"
				<< "    vec4 a = vec4(-1.0,-1.0, 0.0, 1.0);\n"
				<< "    vec4 b = vec4(-1.0, 0.0, 0.0, 1.0);\n"
				<< "    vec4 c = vec4(-1.0, 1.0, 0.0, 1.0);\n"
				<< "    // Coordinate constants: middle column\n"
				<< "    vec4 i = vec4( 0.0,-1.0, 0.0, 1.0);\n"
				<< "    vec4 j = vec4( 0.0, 0.0, 0.0, 1.0);\n"
				<< "    vec4 k = vec4( 0.0, 1.0, 0.0, 1.0);\n"
				<< "    // Coordinate constants: rightmost column\n"
				<< "    vec4 x = vec4( 1.0,-1.0, 0.0, 1.0);\n"
				<< "    vec4 y = vec4( 1.0, 0.0, 0.0, 1.0);\n"
				<< "    vec4 z = vec4( 1.0, 1.0, 0.0, 1.0);\n"
				<< "\n";

			if (m_parameters.testType == TEST_TYPE_STREAMS)
			{
				src << "    if (gl_PrimitiveIDIn == 0)\n"
					<< "    {\n"
					<< "        color = m; gl_Position = b; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = y; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n"
					<< "    else\n"
					<< "    {\n"
					<< "        color = m; gl_Position = y; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = z; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n";
			}

			if (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE)
			{
				const std::string	pointSize	= "gl_PointSize = " + de::toString(m_parameters.pointSize) + ".0f";

				src << "    if (gl_PrimitiveIDIn == 0)\n"
					<< "    {\n"
					<< "        color = g; gl_Position = (a + j) / 2.0f; gl_PointSize = 1.0f; EmitStreamVertex(0);\n"
					<< "        EndStreamPrimitive(0);\n"
					<< "        color = m; gl_Position = (b + k) / 2.0f; gl_PointSize = 1.0f; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n"
					<< "    else\n"
					<< "    {\n"
					<< "        color = g; gl_Position = (j + x) / 2.0f; " << pointSize << "; EmitStreamVertex(0);\n"
					<< "        EndStreamPrimitive(0);\n"
					<< "        color = m; gl_Position = (k + y) / 2.0f; " << pointSize << "; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n";
			}

			if (m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE)
			{
				src << "    if (gl_PrimitiveIDIn == 0)\n"
					<< "    {\n"
					<< "        color = m; gl_Position = b; gl_ClipDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; gl_ClipDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = y; gl_ClipDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n"
					<< "    else\n"
					<< "    {\n"
					<< "        color = m; gl_Position = y; gl_ClipDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; gl_ClipDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = z; gl_ClipDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n";
			}

			if (m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE)
			{
				src << "    if (gl_PrimitiveIDIn == 0)\n"
					<< "    {\n"
					<< "        color = m; gl_Position = b; gl_CullDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; gl_CullDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = j; gl_CullDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "        color = m; gl_Position = j; gl_CullDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; gl_CullDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = k; gl_CullDistance[0] = -1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n"
					<< "    else\n"
					<< "    {\n"
					<< "        color = m; gl_Position = j; gl_CullDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = k; gl_CullDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = y; gl_CullDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "        color = m; gl_Position = y; gl_CullDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = k; gl_CullDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = z; gl_CullDistance[0] =  1.0; " + extraStmt + "EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n";
			}

			src << "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		// Fragment shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in  vec4 i_color;\n"
				<< "layout(location = 0) out vec4 o_color;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    o_color = i_color;\n"
				<< "}\n";

			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_DRAW_INDIRECT || m_parameters.testType == TEST_TYPE_DRAW_INDIRECT_MULTIVIEW)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in vec4 in_position;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position = in_position;\n"
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Fragment shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) out vec4 o_color;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    o_color = vec4(1.0, 1.0, 1.0, 1.0);\n"
				<< "}\n";

			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_QUERY_GET				||
		m_parameters.testType == TEST_TYPE_QUERY_COPY				||
		m_parameters.testType == TEST_TYPE_QUERY_COPY_STRIDE_ZERO	||
		m_parameters.testType == TEST_TYPE_QUERY_RESET)
	{
		struct TopologyShaderInfo
		{
			std::string glslIn;
			std::string glslOut;
			std::string spirvIn;
			std::string spirvOut;
		};

		const std::map<VkPrimitiveTopology, TopologyShaderInfo>	primitiveNames	=
		{
			{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST						, { "points"				, "points"			, "InputPoints"				, "OutputPoints"		} },
			{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST						, { "lines"					, "line_strip"		, "InputLines"				, "OutputLineStrip"		} },
			{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						, { "lines"					, "line_strip"		, "InputLines"				, "OutputLineStrip"		} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					, { "triangles"				, "triangle_strip"	, "Triangles"				, "OutputTriangleStrip"	} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					, { "triangles"				, "triangle_strip"	, "Triangles"				, "OutputTriangleStrip"	} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN					, { "triangles"				, "triangle_strip"	, "Triangles"				, "OutputTriangleStrip"	} },
			{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY		, { "lines_adjacency"		, "line_strip"		, "InputLinesAdjacency"		, "OutputLineStrip"		} },
			{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		, { "lines_adjacency"		, "line_strip"		, "InputLinesAdjacency"		, "OutputLineStrip"		} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY	, { "triangles_adjacency"	, "triangle_strip"	, "InputTrianglesAdjacency"	, "OutputTriangleStrip"	} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	, { "triangles_adjacency"	, "triangle_strip"	, "InputTrianglesAdjacency"	, "OutputTriangleStrip"	} }
		};

		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) out vec4 out0;\n"
				<< "\n"
				<< "out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4  gl_Position;\n"
				<< (pointSizeWanted ? "    float gl_PointSize;\n" : "")
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position  = vec4(0.0, 0.0, 0.0, 1.0);\n"
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "    float n = 4.0 * float(gl_VertexIndex);\n"
				<< "    out0 = vec4(n + 0.0, n + 1.0, n + 2.0, n + 3.0);\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		if (m_parameters.streamId == 0)
		{
			std::ostringstream	src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(" << primitiveNames.at(m_parameters.primTopology).glslIn << ") in;\n"
				<< "layout(location = 0) in vec4 in0[];\n"
				<< "\n"
				<< "layout(" << primitiveNames.at(m_parameters.primTopology).glslOut << ", max_vertices = " << topologyData.at(m_parameters.primTopology).primSize<< ") out;\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
				<< "\n"
				<< "in gl_PerVertex\n"
				<< "{\n"
				<< "    vec4  gl_Position;\n"
				<< (pointSizeWanted ? "    float gl_PointSize;\n" : "")
				<< "} gl_in[];\n"
				<< "out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4  gl_Position;\n"
				<< (pointSizeWanted ? "    float gl_PointSize;\n" : "")
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position  = gl_in[0].gl_Position;\n"
				<< (pointSizeWanted ? "    gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
				;

			for (deUint32 i = 0; i < topologyData.at(m_parameters.primTopology).primSize; i++)
			{
				if (!m_parameters.omitShaderWrite)
					src << "    out0 = in0[" << i << "];\n";
				src << "    EmitVertex();\n";
			}

			src << "    EndPrimitive();\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}
		else
		{
			const deUint32		s	= m_parameters.streamId;
			std::ostringstream	src;

			if (m_parameters.testType == TEST_TYPE_QUERY_GET)
			{
				// The SPIR-V program below is roughly equivalent to the following GLSL code:
				//
				// #version 450
				// #extension GL_ARB_enhanced_layouts : require
				//
				// layout(points) in;
				// layout(location = 0) in vec4 in0[];
				//
				// layout(points, max_vertices = 1) out;
				// layout(location=0, stream=1, xfb_buffer=0, xfb_stride=16) out OutBlock {
				//     layout(xfb_offset=0, location=0) vec4 out0;
				// } outBlock;
				//
				// in gl_PerVertex
				// {
				//     vec4  gl_Position;
				//     float gl_PointSize;
				// } gl_in[];
				// out gl_PerVertex
				// {
				//     vec4  gl_Position;
				//     float gl_PointSize;
				// };
				//
				// void main(void)
				// {
				//     gl_Position  = gl_in[0].gl_Position;
				//     gl_PointSize = gl_in[0].gl_PointSize;
				//     outBlock.out0 = in0[0];
				//     EmitStreamVertex(1);
				//     EndStreamPrimitive(1);
				// }
				//
				// However, the stream number has been parametrized and the code generated by glslang has been tuned to move the
				// Stream, XfbBuffer and XfbStride decorations to the structure member instead of the block. This allows us to test
				// transform feedback decorations on structure members as part of these basic tests.
				src	<< "; SPIR-V\n"
					<< "; Version: 1.0\n"
					<< "; Generator: Khronos Glslang Reference Front End; 10\n"
					<< "; Bound: 64\n"
					<< "; Schema: 0\n"
					<< "               OpCapability Geometry\n"
					<< "               OpCapability TransformFeedback\n"
					<< "               OpCapability GeometryStreams\n"
					<< "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
					<< "               OpMemoryModel Logical GLSL450\n"
					<< "               OpEntryPoint Geometry %main \"main\" %outBlock %in0 %InputBuiltInArrayVar %OutputBuiltInsVar\n"
					<< "               OpExecutionMode %main Xfb\n"
					<< "               OpExecutionMode %main " << primitiveNames.at(m_parameters.primTopology).spirvIn << "\n"
					<< "               OpExecutionMode %main Invocations 1\n"
					<< "               OpExecutionMode %main " << primitiveNames.at(m_parameters.primTopology).spirvOut << "\n"
					<< "               OpExecutionMode %main OutputVertices " << topologyData.at(m_parameters.primTopology).primSize << "\n"
					<< "               OpSource GLSL 450\n"
					<< "               OpSourceExtension \"GL_ARB_enhanced_layouts\"\n"
					<< "               OpName %main \"main\"\n"
					<< "               OpName %OutBlock \"OutBlock\"\n"
					<< "               OpMemberName %OutBlock 0 \"out0\"\n"
					<< "               OpName %outBlock \"outBlock\"\n"
					<< "               OpName %in0 \"in0\"\n"
					<< "               OpMemberDecorate %OutBlock 0 Location 0\n"
					<< "               OpMemberDecorate %OutBlock 0 Offset 0\n"
					// These Stream, XfbBuffer and XfbStride decorations have been moved to the struct member.
					<< "               OpMemberDecorate %OutBlock 0 Stream " << s << "\n"
					<< "               OpMemberDecorate %OutBlock 0 XfbBuffer 0\n"
					<< "               OpMemberDecorate %OutBlock 0 XfbStride 16\n"
					<< "               OpDecorate %OutBlock Block\n"
					<< "               OpMemberDecorate %BuiltIns 0 BuiltIn Position\n"
					<< (pointSizeWanted ? "               OpMemberDecorate %BuiltIns 1 BuiltIn PointSize\n" : "")
					<< "               OpDecorate %BuiltIns Block\n"
					// The decorations mentioned above were using OpDecorate and assigned to %outBlock itself here.
					<< "               OpDecorate %in0 Location 0\n"
					<< "       %void = OpTypeVoid\n"
					<< "          %3 = OpTypeFunction %void\n"
					<< "      %float = OpTypeFloat 32\n"
					<< "    %v4float = OpTypeVector %float 4\n"
					<< "   %OutBlock = OpTypeStruct %v4float\n"
					<< "%_ptr_Output_OutBlock = OpTypePointer Output %OutBlock\n"
					<< "   %outBlock = OpVariable %_ptr_Output_OutBlock Output\n"
					<< "        %int = OpTypeInt 32 1\n"
					<< "      %int_0 = OpConstant %int 0\n"
					;

				for (deUint32 i = 1; i < topologyData.at(m_parameters.primTopology).primSize + 1; i++)
				{
					src << "%int_" << i << " = OpConstant %int " << i << "\n";
				}

				src << "       %uint = OpTypeInt 32 0\n"
					<< "     %uint_0 = OpConstant %uint " << topologyData.at(m_parameters.primTopology).primSize << "\n"
					<< "%_arr_v4float_uint_0 = OpTypeArray %v4float %uint_0\n"
					<< "%_ptr_Input__arr_v4float_uint_0 = OpTypePointer Input %_arr_v4float_uint_0\n"
					<< "        %in0 = OpVariable %_ptr_Input__arr_v4float_uint_0 Input\n"
					<< "%_ptr_Input_v4float = OpTypePointer Input %v4float\n"
					<< "%_ptr_Input_float = OpTypePointer Input %float\n"
					<< "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
					<< "%_ptr_Output_float = OpTypePointer Output %float\n"
					<< "  %streamNum = OpConstant %int " << s << "\n"
					<< "%BuiltIns = OpTypeStruct %v4float" << (pointSizeWanted ? " %float" : "") << "\n"
					<< "%InputBuiltInArray = OpTypeArray %BuiltIns %int_1\n"
					<< "%InputBuiltInArrayPtr = OpTypePointer Input %InputBuiltInArray\n"
					<< "%InputBuiltInArrayVar = OpVariable %InputBuiltInArrayPtr Input\n"
					<< "%OutputBuiltInsPtr = OpTypePointer Output %BuiltIns\n"
					<< "%OutputBuiltInsVar = OpVariable %OutputBuiltInsPtr Output\n"
					<< "       %main = OpFunction %void None %3\n"
					<< "          %5 = OpLabel\n"
					<< "%in_gl_Position_Ptr = OpAccessChain %_ptr_Input_v4float %InputBuiltInArrayVar %int_0 %int_0\n"
					<< "%in_gl_Position = OpLoad %v4float %in_gl_Position_Ptr\n"
					<< "%out_gl_Position_Ptr = OpAccessChain %_ptr_Output_v4float %OutputBuiltInsVar %int_0\n"
					<< (pointSizeWanted ? "%in_gl_PointSize_Ptr = OpAccessChain %_ptr_Input_float %InputBuiltInArrayVar %int_0 %int_1\n" : "")
					<< (pointSizeWanted ? "%in_gl_PointSize = OpLoad %float %in_gl_PointSize_Ptr\n" : "")
					<< (pointSizeWanted ? "%out_gl_PointSize_Ptr = OpAccessChain %_ptr_Output_float %OutputBuiltInsVar %int_1\n" : "")
					;

				for (deUint32 i = 1; i < topologyData.at(m_parameters.primTopology).primSize + 1; i++)
				{
					src << "%" << i << "1 = OpAccessChain %_ptr_Input_v4float %in0 %int_" << i << "\n"
						<< "          %" << i << "2 = OpLoad %v4float %" << i << "1\n"
						<< "          %" << i << "3 = OpAccessChain %_ptr_Output_v4float %outBlock %int_0\n"
						<< "               OpStore %" << i << "3 %" << i << "2\n"
						<< "               OpStore %out_gl_Position_Ptr %in_gl_Position\n"
						<< (pointSizeWanted ? "               OpStore %out_gl_PointSize_Ptr %in_gl_PointSize\n" : "")
						<< "               OpEmitStreamVertex %streamNum\n"
						;
				}

				src << "               OpEndStreamPrimitive %streamNum\n"
					<< "               OpReturn\n"
					<< "               OpFunctionEnd\n"
					;

				programCollection.spirvAsmSources.add("geom") << src.str();
			}
			else
			{
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
					<< "\n"
					<< "layout(" << primitiveNames.at(m_parameters.primTopology).glslIn << ") in;\n"
					<< "layout(location = 0) in vec4 in0[];\n"
					<< "\n"
					<< "layout(" << primitiveNames.at(m_parameters.primTopology).glslOut << ", max_vertices = " << topologyData.at(m_parameters.primTopology).primSize << ") out;\n"
					<< "layout(stream = " << s << ", xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
					<< "\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "    vec4  gl_Position;\n"
					<< (pointSizeWanted ? "    float gl_PointSize;\n" : "")
					<< "} gl_in[];\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "    vec4  gl_Position;\n"
					<< (pointSizeWanted ? "    float gl_PointSize;\n" : "")
					<< "};\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "    gl_Position  = gl_in[0].gl_Position;\n"
					<< (pointSizeWanted ? "    gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
					;

				for (deUint32 i = 0; i < topologyData.at(m_parameters.primTopology).primSize; i++)
				{
					src << "    out0 = in0[" << i << "];\n"
						<< "    EmitStreamVertex(" << s << ");\n";
				}

				src << "    EndStreamPrimitive(" << s << ");\n"
					<< "}\n";

				programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
			}
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_MULTIQUERY)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) out ivec4 out0;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    out0 = ivec4(gl_VertexIndex);\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		{
			const deUint32		s	= m_parameters.streamId;
			std::ostringstream	src;

			DE_ASSERT(s != 0);

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "\n"
				<< "layout(points, max_vertices = 4) out;\n"
				<< "\n"
				<< "layout(stream = " << 0 << ", xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
				<< "layout(stream = " << s << ", xfb_buffer = 1, xfb_offset = 0, xfb_stride = 16, location = 1) out vec4 out1;\n"
				<< "\n"
				<< (pointSizeWanted ? "out gl_PerVertex { float gl_PointSize; };\n\n" : "")
				<< "void main(void)\n"
				<< "{\n"
				<< "    const int   n0 = 3;\n"
				<< "    const int   n1 = 1;\n"
				<< "    const float c0 = 0.5f;\n"
				<< "    const float c1 = 0.5f + float(" << s << ");\n"
				<< "\n"
				<< "    for (int j = 0; j < n0; j++)\n"
				<< "    {\n";

			if (!m_parameters.omitShaderWrite)
				src << "        out0 = vec4(c0);\n";

			src	<< (pointSizeWanted ? "        gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "        EmitStreamVertex(0);\n"
				<< "        EndStreamPrimitive(0);\n"
				<< "    }\n"
				<< "\n"
				<< "    for (int j = 0; j < n1; j++)\n"
				<< "    {\n"
				<< "        out1 = vec4(c1);\n"
				<< (pointSizeWanted ? "        gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "        EmitStreamVertex(" << s << ");\n"
				<< "        EndStreamPrimitive(" << s << ");\n"
				<< "    }\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_LINES_TRIANGLES)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		{
			const deUint32		s			= m_parameters.streamId;
			const bool			line		= isPrimitiveTopologyLine(m_parameters.primTopology);
			const bool			tri			= isPrimitiveTopologyTriangle(m_parameters.primTopology);
			const std::string	p			= line ? std::string("line_strip")
											: tri ? std::string("triangle_strip")
											: std::string("");
			const std::string	vertexCount	= line ? vectorToString(LINES_LIST)
											: tri ? vectorToString(TRIANGLES_LIST)
											: std::string("");
			std::ostringstream	src;

			DE_ASSERT(s != 0);

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "\n"
				<< "layout(" << p << ", max_vertices = 256) out;\n"
				<< "layout(stream = " << 0 << ", xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
				<< "layout(stream = " << s << ", xfb_buffer = 1, xfb_offset = 0, xfb_stride = 16, location = 1) out vec4 out1;\n"
				<< "\n"
				<< "const int vertices_in_primitive[] = int[](" << vertexCount  << ");\n"
				<< "\n"
				<< "int num_vertices_in_primitives()\n"
				<< "{\n"
				<< "    int c = 0;\n"
				<< "\n"
				<< "    for (int i = 0; i < vertices_in_primitive.length(); i++)\n"
				<< "        c = c + vertices_in_primitive[i];\n"
				<< "\n"
				<< "    return c;\n"
				<< "}\n"
				<< "\n"
				<< (pointSizeWanted ? "out gl_PerVertex { float gl_PointSize; };\n\n" : "")
				<< "void main(void)\n"
				<< "{\n"
				<< "    int vc = num_vertices_in_primitives();\n"
				<< "    int c0 = vc * gl_PrimitiveIDIn;\n"
				<< "    int c1 = vc * (" << INVOCATION_COUNT << " + gl_PrimitiveIDIn);\n"
				<< "\n"
				<< "    for (int i = 0; i < vertices_in_primitive.length(); i++)\n"
				<< "    {\n"
				<< "        const int n = vertices_in_primitive[i];\n"
				<< "\n"
				<< "        for (int j = 0; j < n; j++)\n"
				<< "        {\n"
				<< "            out0 = vec4(ivec4(c0, gl_PrimitiveIDIn, i, j));\n"
				<< "            c0 = c0 + 1;\n"
				<< (pointSizeWanted ? "            gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "            EmitStreamVertex(0);\n"
				<< "\n"
				<< "            out1 = vec4(ivec4(c1, gl_PrimitiveIDIn, i, j));\n"
				<< "            c1 = c1 + 1;\n"
				<< (pointSizeWanted ? "            gl_PointSize = " + pointSizeStr + ".0;\n" : "")
				<< "            EmitStreamVertex(" << s << ");\n"
				<< "        }\n"
				<< "\n"
				<< "        EndStreamPrimitive(0);\n"
				<< "        EndStreamPrimitive(" << s << ");\n"
				<< "    }\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_DRAW_OUTSIDE)
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(push_constant) uniform pushConstants\n"
				<< "{\n"
				<< "    uint start;\n"
				<< "} uInput;\n"
				<< "\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0) out uint idx_out;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    idx_out = uInput.start + gl_VertexIndex;\n"
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0f;\n" : "")
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(push_constant) uniform pushConstants\n"
				<< "{\n"
				<< "    uint start;\n"
				<< "} uInput;\n"
				<< "\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0) out uint idx_out;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    idx_out = uInput.start + gl_VertexIndex * 2u;\n"
				<< (pointSizeWanted ? "    gl_PointSize = " + pointSizeStr + ".0f;\n" : "")
				<< "}\n";

			programCollection.glslSources.add("vert2") << glu::VertexSource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_HOLES_VERTEX || m_parameters.testType == TEST_TYPE_HOLES_GEOMETRY)
	{
		// The fragment shader is the same in both variants.
		{
			std::ostringstream frag;
			frag
				<< "#version 460\n"
				<< "layout (location=0) out vec4 outColor;\n"
				<< "\n"
				<< "layout (location = 0) in float goku;\n"
				<< "layout (location = 0, component = 1) in float trunks;\n"
				<< "layout (location = 0, component = 2) in float vegeta;\n"
				<< "\n"
				<< "void main ()\n"
				<< "{\n"
				<< "    outColor = ((goku == 10.0 && trunks == 20.0 && vegeta == 30.0)\n"
				<< "             ? vec4(0.0, 0.0, 1.0, 1.0)\n"
				<< "             : vec4(0.0, 0.0, 0.0, 1.0));\n"
				<< "}\n"
				;
			programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
		}

		const std::string pcDecl =
			"layout (push_constant, std430) uniform PushConstantBlock {\n"
			"    vec3 values;\n"
			"} pc;\n"
			;

		const std::string dbChars =
			"layout (location = 0, xfb_buffer = 0, xfb_stride = 12, xfb_offset = 0) flat out float goku;\n"
			"layout (location = 0, component = 1) flat out float trunks;\n"
			"layout (location = 0, xfb_buffer = 0, xfb_stride = 12, xfb_offset = 8, component = 2) flat out float vegeta;\n"
			;

		const std::string assignments =
			"    goku   = pc.values.x;\n"
			"    trunks = pc.values.y;\n"
			"    vegeta = pc.values.z;\n"
			;

		if (m_parameters.testType == TEST_TYPE_HOLES_GEOMETRY)
		{
			std::ostringstream geom;
			geom
				<< "#version 460\n"
				<< "layout (points) in;\n"
				<< "layout (max_vertices=1, points) out;\n"
				<< "\n"
				<< dbChars
				<< "\n"
				<< pcDecl
				<< "\n"
				<< "void main ()\n"
				<< "{\n"
				<< "    gl_Position  = gl_in[0].gl_Position;\n"
				<< "    gl_PointSize = gl_in[0].gl_PointSize;\n"
				<< "\n"
				<< assignments
				<< "\n"
				<< "    EmitVertex();\n"
				<< "}\n"
				;
			programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
		}

		const bool vertOnly = (m_parameters.testType == TEST_TYPE_HOLES_VERTEX);
		std::ostringstream vert;
		vert
			<< "#version 460\n"
			<< "layout (location = 0) in vec4 inPos;\n"
			<< "\n"
			<< (vertOnly ? dbChars : "")
			<< "\n"
			<< (vertOnly ? pcDecl : "")
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_Position  = inPos;\n"
			<< "    gl_PointSize = 1.0;\n"
			<< "\n"
			<< (vertOnly ? assignments : "")
			<< "}\n"
			;
		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

		return;
	}

	DE_ASSERT(0 && "Unknown test");
}

// Some tests use point lists, others do not. Sometimes we want to test
// using the point size either because we know it caused issues in some
// implementations or because the point size will be stored in the transform
// feedback buffer. Other times it's mandatory to write to the point size.
//
// * TestParameters::primTopology controls the topology type.
// * TestParameters::pointSize controls if we want to write to PointSize or not.
// * TestParameters::usingTessGeom() can be used to check if we use Geometry or
//   Tessellation shaders, and it must match what initPrograms() does.
// * "Feature", in the table below, represents
//   shaderTessellationAndGeometryPointSize.
// * Most variants are OK, but some variants cannot be run.
//   * In some cases, we detect those at checkSupport() time and avoid running
//     them.
//   * In some cases, the variants are simply illegal in theory, and we must
//     avoid generating them.
//   * In some cases, we must switch to using a custom device when running the
//     test.
//
//  Point List		PointSize Wanted	Using Tess/Geom		Feature Available	Outcome
//  -------------------------------------------------------------------------------------------
//  0				0					0					0					OK
//  0				0					0					1					OK
//  0				0					1					0					OK
//  0				0					1					1					OK
//  0				1					0					0					OK, In Vertex Shader
//  0				1					0					1					OK, In Vertex Shader
//  0				1					1					0					Nope, cannot use PointSize (checkSupport)
//  0				1					1					1					OK
//  1				0					0					0					Nope, must write to it In Vertex Shader (avoid generating these variants)
//  1				0					0					1					Nope, must write to it In Vertex Shader (avoid generating these variants)
//  1				0					1					0					OK, implicit 1.0 in Tess/Geom
//  1				0					1					1					OK, but we must disable the feature with a Custom Device (test runtime)
//  1				1					0					0					OK
//  1				1					0					1					OK
//  1				1					1					0					Nope, cannot use PointSize (checkSupport)
//  1				1					1					1					OK
//
void addTransformFeedbackTestCaseVariants (tcu::TestCaseGroup* group, const std::string& name, const std::string& desc, const TestParameters& parameters)
{
	std::vector<uint32_t> pointSizes (1u, parameters.pointSize);

	if (parameters.pointSize == 0u)
		pointSizes.push_back(1u);

	int caseCount = 0;
	for (const auto& pointSize : pointSizes)
	{
		const auto		testName	= name + ((caseCount > 0) ? "_ptsz" : ""); // Only add suffix if we're adding more than one case.
		TestParameters	params		(parameters);
		params.pointSize			= pointSize;

		// There are some test variants which are illegal.
		if (params.isPoints() && !params.pointSizeWanted() && !params.usingTessGeom())
			continue; // We need to emit the point size in the vertex shader.

		group->addChild(new TransformFeedbackTestCase(group->getTestContext(), testName.c_str(), desc.c_str(), params));
		++caseCount;
	}
}

void createTransformFeedbackSimpleTests(tcu::TestCaseGroup* group, vk::PipelineConstructionType constructionType)
{
	{
		const deUint32		bufferCounts[]	= { 1u, 2u, 4u, 8u };
		const deUint32		bufferSizes[]	= { 256u, 512u, 128u * 1024u };
		const TestType		testTypes[]		= { TEST_TYPE_BASIC, TEST_TYPE_RESUME, TEST_TYPE_XFB_POINTSIZE, TEST_TYPE_XFB_CLIPDISTANCE, TEST_TYPE_XFB_CULLDISTANCE, TEST_TYPE_XFB_CLIP_AND_CULL, TEST_TYPE_DRAW_OUTSIDE };
		const std::string	testTypeNames[]	= { "basic", "resume", "xfb_pointsize", "xfb_clipdistance", "xfb_culldistance", "xfb_clip_and_cull", "draw_outside" };

		for (deUint32 testTypesNdx = 0; testTypesNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypesNdx)
		{
			const TestType		testType	= testTypes[testTypesNdx];
			const std::string	testName	= testTypeNames[testTypesNdx];

			for (deUint32 bufferCountsNdx = 0; bufferCountsNdx < DE_LENGTH_OF_ARRAY(bufferCounts); ++bufferCountsNdx)
			{
				const deUint32	partCount	= bufferCounts[bufferCountsNdx];

				for (deUint32 bufferSizesNdx = 0; bufferSizesNdx < DE_LENGTH_OF_ARRAY(bufferSizes); ++bufferSizesNdx)
				{
					const deUint32	bufferSize	= bufferSizes[bufferSizesNdx];
					TestParameters	parameters	= { constructionType, testType, bufferSize, partCount, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, true, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

					addTransformFeedbackTestCaseVariants(group, (testName + "_" + de::toString(partCount) + "_" + de::toString(bufferSize)), "Simple Transform Feedback test", parameters);

					parameters.streamId0Mode = STREAM_ID_0_BEGIN_QUERY_INDEXED;
					addTransformFeedbackTestCaseVariants(group, (testName + "_beginqueryindexed_streamid_0_" + de::toString(partCount) + "_" + de::toString(bufferSize)), "Simple Transform Feedback test", parameters);

					parameters.streamId0Mode = STREAM_ID_0_END_QUERY_INDEXED;
					addTransformFeedbackTestCaseVariants(group, (testName + "_endqueryindexed_streamid_0_" + de::toString(partCount) + "_" + de::toString(bufferSize)), "Simple Transform Feedback test", parameters);
				}
			}
		}
	}

	{
		const deUint32		bufferCounts[]	= { 6u, 8u, 10u, 12u };
		const TestType		testType		= TEST_TYPE_WINDING;
		const std::string	testName		= "winding";

		for (const auto& topology : topologyData)
		{
			// Note: no need to test POINT_LIST as is tested in many tests.
			if (topology.first == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
				continue;

			for (deUint32 bufferCountsNdx = 0; bufferCountsNdx < DE_LENGTH_OF_ARRAY(bufferCounts); ++bufferCountsNdx)
			{
				const deUint32	vertexCount	= bufferCounts[bufferCountsNdx];

				TestParameters	parameters	= { constructionType, testType, 0u, vertexCount, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, false, false, false, topology.first, false };

				addTransformFeedbackTestCaseVariants(group, (testName + "_" + topology.second.topologyName + de::toString(vertexCount)), "Topology winding test", parameters);
			}
		}
	}

	{
		for (int i = 0; i < 2; ++i)
		{
			const bool			multiview		= (i > 0);
			const deUint32		vertexStrides[]	= { 4u, 61u, 127u, 251u, 509u };
			const TestType		testType		= (multiview ? TEST_TYPE_DRAW_INDIRECT_MULTIVIEW : TEST_TYPE_DRAW_INDIRECT);
			const std::string	testName		= std::string("draw_indirect") + (multiview ? "_multiview" : "");

			for (deUint32 vertexStridesNdx = 0; vertexStridesNdx < DE_LENGTH_OF_ARRAY(vertexStrides); ++vertexStridesNdx)
			{
				const deUint32	vertexStrideBytes	= static_cast<deUint32>(sizeof(deUint32) * vertexStrides[vertexStridesNdx]);
				TestParameters	parameters			= { constructionType, testType, 0u, 0u, 0u, 0u, vertexStrideBytes, STREAM_ID_0_NORMAL, false, false, false, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

				addTransformFeedbackTestCaseVariants(group, (testName + "_" + de::toString(vertexStrideBytes)), "Rendering tests with various strides", parameters);

				parameters.streamId0Mode = STREAM_ID_0_BEGIN_QUERY_INDEXED;
				addTransformFeedbackTestCaseVariants(group, (testName + "_beginqueryindexed_streamid_0_" + de::toString(vertexStrideBytes)), "Rendering tests with various strides", parameters);

				parameters.streamId0Mode = STREAM_ID_0_END_QUERY_INDEXED;
				addTransformFeedbackTestCaseVariants(group, (testName + "_endqueryindexed_streamid_0_" + de::toString(vertexStrideBytes)), "Rendering tests with various strides", parameters);
			}
		}
	}

	{
		const struct
		{
			TestType		testType;
			const char*		testName;
		} testCases[] =
		{
			{ TEST_TYPE_BACKWARD_DEPENDENCY,			"backward_dependency"			},
			{ TEST_TYPE_BACKWARD_DEPENDENCY_INDIRECT,	"backward_dependency_indirect"	},
		};

		for (const auto& testCase : testCases)
		{
			const auto&			testType	= testCase.testType;
			const std::string	testName	= testCase.testName;
			TestParameters		parameters	= { constructionType, testType, 512u, 2u, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, false, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

			addTransformFeedbackTestCaseVariants(group, testName, "Rendering test checks backward pipeline dependency", parameters);

			parameters.streamId0Mode = STREAM_ID_0_BEGIN_QUERY_INDEXED;
			addTransformFeedbackTestCaseVariants(group, (testName + "_beginqueryindexed_streamid_0"), "Rendering test checks backward pipeline dependency", parameters);

			parameters.streamId0Mode = STREAM_ID_0_END_QUERY_INDEXED;
			addTransformFeedbackTestCaseVariants(group, (testName + "_endqueryindexed_streamid_0"), "Rendering test checks backward pipeline dependency", parameters);

			parameters.noOffsetArray = true;
			addTransformFeedbackTestCaseVariants(group, (testName + "_no_offset_array"), "Rendering test checks backward pipeline dependency (using NULL for offset array)", parameters);
		}
	}

	{
		const deUint32		usedStreamId[]			= { 0u, 1u, 3u, 6u, 14u };
		const deUint32		vertexCounts[]			= { 6u, 61u, 127u, 251u, 509u }; // Lowest value has to be at least 6. Otherwise the triangles with adjacency can't be generated.
		const TestType		testType				= TEST_TYPE_QUERY_GET;
		const std::string	testName				= "query";
		const TestType		testTypeCopy[]			= { TEST_TYPE_QUERY_COPY, TEST_TYPE_QUERY_COPY_STRIDE_ZERO };
		const std::string	testNameCopy[]			= { "query_copy", "query_copy_stride_zero" };
		const TestType		testTypeHostQueryReset	= TEST_TYPE_QUERY_RESET;
		const std::string	testNameHostQueryReset	= "host_query_reset";

		for (const auto& topology : topologyData)
		{
			// Currently, we don't test tessellation here.
			if (topology.first == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
				continue;

			for (const auto& streamCounts : usedStreamId)
			{
				const deUint32	streamId	= streamCounts;

				for (const auto& numVertices : vertexCounts)
				{
					for (deUint32 i = 0; i < 2; ++i)
					{
						const bool				query64Bits		= (i == 1);
						const std::string		widthStr		= (query64Bits ? "_64bits" : "_32bits");

						deUint32				vertCount		= numVertices;

						// The number of vertices in original test was 4.
						if (topology.first == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && vertCount == 6) vertCount -= 2;

						// Round the number of vertices to match the used primitive topology - if necessary.
						const deUint32			primitiveCount	= (deUint32)topology.second.getNumPrimitives(vertCount);
						const deUint32			vertexCount		= (deUint32)topology.second.getNumVertices(primitiveCount);

						DE_ASSERT(vertexCount > 0);

						const deUint32			bytesPerVertex	= static_cast<deUint32>(4 * sizeof(float));
						const deUint32			bufferSize		= bytesPerVertex * vertexCount;
						TestParameters			parameters		= { constructionType, testType, bufferSize, 0u, streamId, 0u, 0u, STREAM_ID_0_NORMAL, query64Bits, false, true, false, false, topology.first, false };
						const std::string		fullTestName	= testName + "_" + topology.second.topologyName + de::toString(streamId) + "_" + de::toString(vertexCount) + widthStr;
						addTransformFeedbackTestCaseVariants(group, fullTestName, "Written primitives query test", parameters);

						TestParameters			omitParameters	= { constructionType, testType, bufferSize, 0u, streamId, 0u, 0u, STREAM_ID_0_NORMAL, query64Bits, false, true, true, false, topology.first, false };
						const std::string		omitTestName	= testName + "_omit_write_" + topology.second.topologyName + de::toString(streamId) + "_" + de::toString(vertexCount) + widthStr;
						addTransformFeedbackTestCaseVariants(group, omitTestName, "Written primitives query test", omitParameters);

						for (deUint32 testTypeCopyNdx = 0; testTypeCopyNdx < DE_LENGTH_OF_ARRAY(testTypeCopy); testTypeCopyNdx++)
						{
							TestParameters			parametersCopy		= { constructionType, testTypeCopy[testTypeCopyNdx], bufferSize, 0u, streamId, 0u, 0u, STREAM_ID_0_NORMAL, query64Bits, false, true, false, false, topology.first, false };
							const std::string		fullTestNameCopy	= testNameCopy[testTypeCopyNdx] + "_" + topology.second.topologyName + de::toString(streamId) + "_" + de::toString(vertexCount) + widthStr;
							addTransformFeedbackTestCaseVariants(group, fullTestNameCopy, "Written primitives query test", parametersCopy);

							parametersCopy.queryResultWithAvailability = true;
							const std::string		fullTestNameQueryWithAvailability = testNameCopy[testTypeCopyNdx] + "_" + topology.second.topologyName + de::toString(streamId) + "_" + de::toString(vertexCount) + widthStr + "_query_with_availability";
							addTransformFeedbackTestCaseVariants(group, fullTestNameQueryWithAvailability, "Written primitives query test", parametersCopy);
						}

						const TestParameters	parametersHostQueryReset	= { constructionType, testTypeHostQueryReset, bufferSize, 0u, streamId, 0u, 0u, STREAM_ID_0_NORMAL, query64Bits, false, true, false, false, topology.first, false };
						const std::string		fullTestNameHostQueryReset	= testNameHostQueryReset + "_" + topology.second.topologyName + de::toString(streamId) + "_" + de::toString(vertexCount) + widthStr;
						addTransformFeedbackTestCaseVariants(group, fullTestNameHostQueryReset, "Written primitives query test", parametersHostQueryReset);

						if (streamId == 0)
						{
							std::string	testNameStream0 = fullTestName;
							testNameStream0 += "_beginqueryindexed_streamid_0";
							parameters.streamId0Mode = STREAM_ID_0_BEGIN_QUERY_INDEXED;
							addTransformFeedbackTestCaseVariants(group, testNameStream0, "Written primitives query test", parameters);

							testNameStream0 = fullTestName;
							testNameStream0 += "_endqueryindexed_streamid_0";
							parameters.streamId0Mode = STREAM_ID_0_END_QUERY_INDEXED;
							addTransformFeedbackTestCaseVariants(group, testNameStream0, "Written primitives query test", parameters);
						}
					}
				}
			}
		}
	}

	// Depth clip control tests.
	{
		TestParameters	parameters	= { constructionType, TEST_TYPE_DEPTH_CLIP_CONTROL_VERTEX, 96, 1u, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, true, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

		addTransformFeedbackTestCaseVariants(group, "depth_clip_control_vertex", "", parameters);
	}
	{
		TestParameters	parameters	= { constructionType, TEST_TYPE_DEPTH_CLIP_CONTROL_GEOMETRY, 96, 1u, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, true, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

		addTransformFeedbackTestCaseVariants(group, "depth_clip_control_geometry", "", parameters);
	}
	{
		TestParameters	parameters	= { constructionType, TEST_TYPE_DEPTH_CLIP_CONTROL_TESE, 96, 1u, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, true, false, false, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, false };

		addTransformFeedbackTestCaseVariants(group, "depth_clip_control_tese", "", parameters);
	}

	{
		const deUint32		usedStreamId[]	= { 1u, 3u, 6u, 14u };
		const TestType		testType		= TEST_TYPE_LINES_TRIANGLES;
		const std::string	testName		= "lines_or_triangles";

		for (const auto& topology : topologyData)
		{
			const deUint32	outputVertexCount	= (topology.first == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)     ? 2 * destripedLineCount(LINES_LIST)
												: (topology.first == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) ? 3 * destripedTriangleCount(TRIANGLES_LIST)
												: 0;

			if (outputVertexCount == 0)
				continue;

			for (const auto& streamId : usedStreamId)
			{
				const deUint32			partCount		= 2u;
				const deUint32			bytesPerVertex	= static_cast<deUint32>(sizeof(tcu::Vec4));
				const deUint32			bufferSize		= partCount * INVOCATION_COUNT * outputVertexCount * bytesPerVertex;
				const std::string		fullTestName	= testName + "_" + topology.second.topologyName + de::toString(streamId);
				const TestParameters	parameters		=
				{
					constructionType,
					testType,			//  TestType			testType;
					bufferSize,			//  deUint32			bufferSize;
					partCount,			//  deUint32			partCount;
					streamId,			//  deUint32			streamId;
					0u,					//  deUint32			pointSize;
					0u,					//  deUint32			vertexStride;
					STREAM_ID_0_NORMAL,	//  StreamId0Mode		streamId0Mode;
					false,				//  bool				query64bits;
					false,				//  bool				noOffsetArray;
					true,				//  bool				requireRastStreamSelect;
					false,				//  bool				omitShaderWrite;
					false,				//  bool				useMaintenance5;
					topology.first,		//  VkPrimitiveTopology	primTopology;
					false				//  bool				queryResultWithAvailability;
				};

				addTransformFeedbackTestCaseVariants(group, fullTestName, "", parameters);
			}
		}
	}

#ifndef CTS_USES_VULKANSC
	{
		const TestParameters parameters
		{
			constructionType,
			TEST_TYPE_RESUME,						//  TestType			testType;
			96u,									//  deUint32			bufferSize;
			2u,										//  deUint32			partCount;
			1u,										//  deUint32			streamId;
			0u,										//  deUint32			pointSize;
			0u,										//  deUint32			vertexStride;
			STREAM_ID_0_NORMAL,						//  StreamId0Mode		streamId0Mode;
			false,									//  bool				query64bits;
			false,									//  bool				noOffsetArray;
			true,									//  bool				requireRastStreamSelect;
			false,									//  bool				omitShaderWrite;
			true,									//  bool				useMaintenance5;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	//  VkPrimitiveTopology	primTopology;
			false									//  bool				queryResultWithAvailability
		};
		group->addChild(new TransformFeedbackTestCase(group->getTestContext(), "maintenance5", "", parameters));
	}
#endif // CTS_USES_VULKANSC
}

void createTransformFeedbackStreamsSimpleTests (tcu::TestCaseGroup* group, vk::PipelineConstructionType constructionType)
{
	const deUint32		usedStreamId[]		= { 1u, 3u, 6u, 14u };
	const TestType		testTypes[]			= { TEST_TYPE_STREAMS, TEST_TYPE_STREAMS_POINTSIZE, TEST_TYPE_STREAMS_CLIPDISTANCE, TEST_TYPE_STREAMS_CULLDISTANCE };
	const std::string	testTypeNames[]		= { "streams", "streams_pointsize", "streams_clipdistance", "streams_culldistance" };

	for (deUint32 testTypesNdx = 0; testTypesNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypesNdx)
	{
		const TestType		testType	= testTypes[testTypesNdx];
		const std::string	testName	= testTypeNames[testTypesNdx];
		const deUint32		pointSize	= (testType == TEST_TYPE_STREAMS_POINTSIZE) ? 2u : 0u;

		for (deUint32 streamCountsNdx = 0; streamCountsNdx < DE_LENGTH_OF_ARRAY(usedStreamId); ++streamCountsNdx)
		{
			const deUint32	streamId	= usedStreamId[streamCountsNdx];
			TestParameters	parameters	= { constructionType, testType, 0u, 0u, streamId, pointSize, 0u, STREAM_ID_0_NORMAL, false, false, true, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

			addTransformFeedbackTestCaseVariants(group, (testName + "_" + de::toString(streamId)), "Streams usage test", parameters);
		}
	}

	{
		const TestType		testType	= TEST_TYPE_MULTISTREAMS;
		const std::string	testName	= "multistreams";

		for (deUint32 bufferCountsNdx = 0; bufferCountsNdx < DE_LENGTH_OF_ARRAY(usedStreamId); ++bufferCountsNdx)
		{
			const deUint32			streamId			= usedStreamId[bufferCountsNdx];
			const deUint32			streamsUsed			= 2u;
			const deUint32			maxBytesPerVertex	= 256u;
			const TestParameters	parameters			= { constructionType, testType, maxBytesPerVertex * streamsUsed, streamsUsed, streamId, 0u, 0u, STREAM_ID_0_NORMAL, false, false, false, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

			addTransformFeedbackTestCaseVariants(group, (testName + "_" + de::toString(streamId)), "Simultaneous multiple streams usage test", parameters);
		}
	}

	{
		const TestType		testType	= TEST_TYPE_MULTISTREAMS_SAME_LOCATION;
		const std::string	testName	= "multistreams_same_location";
		for (const auto streamId : usedStreamId)
		{
			const deUint32			streamsUsed			= 2u;
			const TestParameters	parameters			= { constructionType, testType, 32 * 4, streamsUsed, streamId, 0u, 0u, STREAM_ID_0_NORMAL, false, false, false, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

			addTransformFeedbackTestCaseVariants(group, (testName + "_" + de::toString(streamId)), "Simultaneous multiple streams to the same location usage test", parameters);
		}
	}

	{
		const TestType		testType	= TEST_TYPE_MULTIQUERY;
		const std::string	testName	= "multiquery";

		for (deUint32 bufferCountsNdx = 0; bufferCountsNdx < DE_LENGTH_OF_ARRAY(usedStreamId); ++bufferCountsNdx)
		{
			const deUint32			streamId			= usedStreamId[bufferCountsNdx];
			const deUint32			streamsUsed			= 2u;
			const deUint32			maxBytesPerVertex	= 256u;
			const TestParameters	parameters			= { constructionType, testType, maxBytesPerVertex * streamsUsed, streamsUsed, streamId, 0u, 0u, STREAM_ID_0_NORMAL, false, false, false, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };
			const TestParameters	writeOmitParameters	= { constructionType, testType, maxBytesPerVertex * streamsUsed, streamsUsed, streamId, 0u, 0u, STREAM_ID_0_NORMAL, false, false, false, true, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };

			addTransformFeedbackTestCaseVariants(group, (testName + "_" + de::toString(streamId)), "Simultaneous multiple queries usage test", parameters);
			addTransformFeedbackTestCaseVariants(group, (testName + "_omit_write_" + de::toString(streamId)), "Simultaneous multiple queries usage test", writeOmitParameters);
		}
	}

	{
		struct
		{
			TestType		testType;
			const char*		suffix;
		} holeCases[] =
		{
			{ TEST_TYPE_HOLES_VERTEX,	"_vert" },
			{ TEST_TYPE_HOLES_GEOMETRY,	"_geom" },
		};
		const std::string testNameBase = "holes";

		for (const auto& holeCase : holeCases)
			for (const auto& extraDraw : { false, true})
			{
				const auto				partCount	= (extraDraw ? 2u : 1u);
				const auto				testName	= testNameBase + (extraDraw ? "_extra_draw" : "");
				const TestParameters	parameters	{ constructionType, holeCase.testType, 0u, partCount, 0u, 1u, 0u, STREAM_ID_0_NORMAL, false, false, false, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, false };;

				group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + holeCase.suffix).c_str(), "Test skipping components in the XFB buffer", parameters));
			}
	}
}

void createTransformFeedbackAndStreamsSimpleTests (tcu::TestCaseGroup* group, vk::PipelineConstructionType constructionType)
{
	createTransformFeedbackSimpleTests(group, constructionType);
	createTransformFeedbackStreamsSimpleTests(group, constructionType);
}

class TestGroupWithClean : public tcu::TestCaseGroup
{
public:
			TestGroupWithClean	(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
				: tcu::TestCaseGroup(testCtx, name.c_str(), description.c_str())
				{}

	virtual	~TestGroupWithClean	(void) { cleanupDevices(); }
};

} // anonymous

tcu::TestCaseGroup* createTransformFeedbackSimpleTests (tcu::TestContext& testCtx, vk::PipelineConstructionType constructionType)
{
	static const std::map<vk::PipelineConstructionType, std::string> groupNameSuffix
	{
		std::make_pair(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC,					""					),
		std::make_pair(PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY,			"_fast_gpl"			),
		std::make_pair(PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY,	"_optimized_gpl"	),
	};

	de::MovePtr<tcu::TestCaseGroup> mainGroup(new TestGroupWithClean(testCtx, "simple" + groupNameSuffix.at(constructionType), "Transform Feedback Simple tests"));
	createTransformFeedbackAndStreamsSimpleTests(mainGroup.get(), constructionType);
	return mainGroup.release();
}

} // TransformFeedback
} // vkt
