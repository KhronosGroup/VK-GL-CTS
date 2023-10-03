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

enum class TestType
{
	DEFAULT = 0,
	CHECK_GROUP_HANDLES,
	CHECK_CAPTURE_REPLAY_HANDLES,
	CHECK_ALL_HANDLES,
};

struct TestParams
{
	LibraryConfiguration				libraryConfiguration;
	bool								multithreadedCompilation;
	bool								pipelinesCreatedUsingDHO;
	TestType							testType;
	bool								useAABBs;
	bool								useMaintenance5;
	bool								useLinkTimeOptimizations;
	bool								retainLinkTimeOptimizations;
	deUint32							width;
	deUint32							height;

	uint32_t getPixelCount (void) const
	{
		return width * height;
	}

	uint32_t getHitGroupCount (void) const
	{
		uint32_t numShadersUsed = libraryConfiguration.pipelineShaders;
		for (const auto& lib : libraryConfiguration.pipelineLibraries)
			numShadersUsed += lib.y();
		return numShadersUsed;
	}

	bool includesCaptureReplay (void) const
	{
		return (testType == TestType::CHECK_CAPTURE_REPLAY_HANDLES || testType == TestType::CHECK_ALL_HANDLES);
	}
};

// This class will help verify shader group handles in libraries by maintaining information of the library tree and being able to
// calculate the offset of the handles for each pipeline in the "flattened" array of shader group handles.
class PipelineTree
{
protected:
	// Each node represents a pipeline.
	class Node
	{
	public:
		Node (int64_t parent, uint32_t groupCount)
			: m_parent		(parent)
			, m_groupCount	(groupCount)
			, m_children	()
			, m_frozen		(false)
			, m_flatOffset	(std::numeric_limits<uint32_t>::max())
		{}

		void		appendChild				(Node* child)					{ m_children.push_back(child); }
		uint32_t	getOffset				(void) const					{ return m_flatOffset; }
		void		freeze					(void)							{ m_frozen = true; }
		uint32_t	calcOffsetRecursively	(uint32_t currentOffset)		// Returns the next offset.
		{
			DE_ASSERT(m_frozen);
			m_flatOffset = currentOffset;
			uint32_t newOffset = currentOffset + m_groupCount;
			for (auto& node : m_children)
				newOffset = node->calcOffsetRecursively(newOffset);
			return newOffset;
		}

	protected:
		const int64_t		m_parent;		// Parent pipeline (-1 for the root node).
		const uint32_t		m_groupCount;	// Shader group count in pipeline. Related to LibraryConfiguration::pipelineLibraries[1].
		std::vector<Node*>	m_children;		// How many child pipelines. Related to LibraryConfiguration::pipelineLibraries[0]
		bool				m_frozen;		// No sense to calculate offsets before the tree structure is fully constructed.
		uint32_t			m_flatOffset;	// Calculated offset in the flattened array.
	};

public:
	PipelineTree ()
		: m_nodes				()
		, m_root				(nullptr)
		, m_frozen				(false)
		, m_offsetsCalculated	(false)
	{}

	// See LibraryConfiguration::pipelineLibraries.
	void addNode (int64_t parent, uint32_t groupCount)
	{
		DE_ASSERT(m_nodes.size() < static_cast<size_t>(std::numeric_limits<uint32_t>::max()));

		if (parent < 0)
		{
			DE_ASSERT(!m_root);
			m_nodes.emplace_back(new Node(parent, groupCount));
			m_root = m_nodes.back().get();
		}
		else
		{
			DE_ASSERT(parent < static_cast<int64_t>(m_nodes.size()));
			m_nodes.emplace_back(new Node(parent, groupCount));
			m_nodes.at(static_cast<size_t>(parent))->appendChild(m_nodes.back().get());
		}
	}

	// Confirms we will not be adding more nodes to the tree.
	void freeze (void)
	{
		for (auto& node : m_nodes)
			node->freeze();
		m_frozen = true;
	}

	// When obtaining shader group handles from the root pipeline, we get a vector of handles in which some of those handles come from pipeline libraries.
	// This method returns, for each pipeline, the offset of its shader group handles in that vector as the number of shader groups (not bytes).
	std::vector<uint32_t> getGroupOffsets (void)
	{
		DE_ASSERT(m_frozen);

		if (!m_offsetsCalculated)
		{
			calcOffsets();
			m_offsetsCalculated = true;
		}

		std::vector<uint32_t> offsets;
		offsets.reserve(m_nodes.size());

		for (const auto& node : m_nodes)
			offsets.push_back(node->getOffset());

		return offsets;
	}

protected:
	void calcOffsets (void)
	{
		DE_ASSERT(m_frozen);
		if (m_root)
		{
			m_root->calcOffsetRecursively(0);
		}
	}

	std::vector<std::unique_ptr<Node>>	m_nodes;
	Node*								m_root;
	bool								m_frozen;
	bool								m_offsetsCalculated;
};

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

class RayTracingPipelineLibraryTestInstance : public TestInstance
{
public:
																	RayTracingPipelineLibraryTestInstance	(Context& context, const TestParams& data);
																	~RayTracingPipelineLibraryTestInstance	(void);
	tcu::TestStatus													iterate									(void);

protected:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures		(VkCommandBuffer cmdBuffer);
	de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure			(VkCommandBuffer cmdBuffer,
																											 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures);
	std::vector<uint32_t>											runTest									(bool replay = false);
private:
	TestParams														m_data;
	PipelineTree													m_pipelineTree;
	std::vector<uint8_t>											m_captureReplayHandles;
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
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	context.requireDeviceFunctionality("VK_KHR_pipeline_library");

	if (m_data.testType != TestType::DEFAULT)
		context.requireDeviceFunctionality("VK_EXT_pipeline_library_group_handles");

	if (m_data.useLinkTimeOptimizations)
		context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

	if (m_data.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	if (m_data.includesCaptureReplay())
	{
		const auto& rtFeatures = context.getRayTracingPipelineFeatures();
		if (!rtFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay)
			TCU_THROW(NotSupportedError, "rayTracingPipelineShaderGroupHandleCaptureReplay not supported");
	}

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

	if (m_data.useAABBs)
	{
		std::ostringstream isec;
		isec
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "void main()\n"
			<< "{\n"
			<< "  reportIntersectionEXT(gl_RayTminEXT, 0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("isec") << glu::IntersectionSource(updateRayTracingGLSL(isec.str())) << buildOptions;
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
	, m_pipelineTree		()
{
	// Build the helper pipeline tree, which helps for some tests.
	m_pipelineTree.addNode(-1, static_cast<uint32_t>(m_data.libraryConfiguration.pipelineShaders + 2/*rgen and miss for the root pipeline*/));

	for (const auto& lib : m_data.libraryConfiguration.pipelineLibraries)
		m_pipelineTree.addNode(lib.x(), static_cast<uint32_t>(lib.y()));

	m_pipelineTree.freeze();
}

RayTracingPipelineLibraryTestInstance::~RayTracingPipelineLibraryTestInstance (void)
{
}

std::vector<de::SharedPtr<BottomLevelAccelerationStructure> > RayTracingPipelineLibraryTestInstance::initBottomAccelerationStructures (VkCommandBuffer cmdBuffer)
{
	const auto&														vkd			= m_context.getDeviceInterface();
	const auto														device		= m_context.getDevice();
	auto&															allocator	= m_context.getDefaultAllocator();
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

			if (m_data.useAABBs)
			{
				geometryData.push_back(xyz + v1);
				geometryData.push_back(xyz + v2);
			}
			else
			{
				geometryData.push_back(xyz + v0);
				geometryData.push_back(xyz + v1);
				geometryData.push_back(xyz + v2);
				geometryData.push_back(xyz + v2);
				geometryData.push_back(xyz + v1);
				geometryData.push_back(xyz + v3);
			}

			bottomLevelAccelerationStructure->addGeometry(geometryData, !m_data.useAABBs/*triangles*/);
			bottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
			result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
		}

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> RayTracingPipelineLibraryTestInstance::initTopAccelerationStructure (VkCommandBuffer cmdBuffer,
																												std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
{
	const auto&									vkd			= m_context.getDeviceInterface();
	const auto									device		= m_context.getDevice();
	auto&										allocator	= m_context.getDefaultAllocator();

	deUint32 instanceCount = m_data.width * m_data.height / 2;

	de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();
	result->setInstanceCount(instanceCount);

	deUint32 currentInstanceIndex	= 0;
	deUint32 numShadersUsed			= m_data.getHitGroupCount();

	for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			if (((x + y) % 2) == 0)
				continue;

			result->addInstance(bottomLevelAccelerationStructures[currentInstanceIndex], identityMatrix3x4, 0, 0xFF, currentInstanceIndex % numShadersUsed, 0U);
			currentInstanceIndex++;
		}
	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

void compileShaders (Context& context,
					 de::SharedPtr<de::MovePtr<RayTracingPipeline>>& pipeline,
					 const std::vector<std::tuple<std::string, VkShaderStageFlagBits>>& shaderData,
					 const Move<VkShaderModule>& isecMod)
{
	const auto&	vkd			= context.getDeviceInterface();
	const auto	device		= context.getDevice();
	const auto&	binaries	= context.getBinaryCollection();
	const bool	hasISec		= static_cast<bool>(isecMod);

	for (deUint32 i=0; i< shaderData.size(); ++i)
	{
		std::string				shaderName;
		VkShaderStageFlagBits	shaderStage;
		std::tie(shaderName, shaderStage) = shaderData[i];

		auto pipelinePtr = pipeline->get();
		pipelinePtr->addShader(shaderStage, createShaderModule(vkd, device, binaries.get(shaderName)), i);
		if (hasISec && shaderStage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
			pipelinePtr->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, isecMod.get(), i);
	}
}

struct CompileShadersMultithreadData
{
	Context&															context;
	de::SharedPtr<de::MovePtr<RayTracingPipeline>>&						pipeline;
	const std::vector<std::tuple<std::string, VkShaderStageFlagBits>>&	shaderData;
	const Move<VkShaderModule>&											isecMod;
};

void compileShadersThread (void* param)
{
	CompileShadersMultithreadData* csmd = (CompileShadersMultithreadData*)param;
	compileShaders(csmd->context, csmd->pipeline, csmd->shaderData, csmd->isecMod);
}

std::vector<uint32_t> getAllGroupCounts (const std::vector<de::SharedPtr<de::MovePtr<RayTracingPipeline>>>& rayTracingPipelines)
{
	std::vector<uint32_t> allGroupCounts;
	allGroupCounts.reserve(rayTracingPipelines.size());
	std::transform(begin(rayTracingPipelines), end(rayTracingPipelines), std::back_inserter(allGroupCounts),
		[](const de::SharedPtr<de::MovePtr<RayTracingPipeline>>& rtPipeline) { return rtPipeline->get()->getFullShaderGroupCount(); });

	return allGroupCounts;
}

// Sometimes we want to obtain shader group handles and do checks on them, and the processing we do is the same for normal handles
// and for capture/replay handles. Yet their sizes can be different, and the function to get them also changes. The type below
// provides a small abstraction so we only have to choose the right class to instantiate, and the rest of the code is the same.
class HandleGetter
{
public:
	HandleGetter			(const uint32_t handleSize) : m_handleSize(handleSize)	{}
	virtual ~HandleGetter	()														{}

	virtual std::vector<uint8_t> getShaderGroupHandlesVector (const RayTracingPipeline*	rtPipeline,
															  const DeviceInterface&	vkd,
															  const VkDevice			device,
															  const VkPipeline			pipeline,
															  const uint32_t			firstGroup,
															  const uint32_t			groupCount) const = 0;

protected:
	const uint32_t m_handleSize;
};

class NormalHandleGetter : public HandleGetter
{
public:
	NormalHandleGetter			(const uint32_t shaderGroupHandleSize) : HandleGetter(shaderGroupHandleSize)	{}
	virtual ~NormalHandleGetter	()																				{}

	std::vector<uint8_t> getShaderGroupHandlesVector (const RayTracingPipeline*	rtPipeline,
													  const DeviceInterface&	vkd,
													  const VkDevice			device,
													  const VkPipeline			pipeline,
													  const uint32_t			firstGroup,
													  const uint32_t			groupCount) const override
	{
		return rtPipeline->getShaderGroupHandles(vkd, device, pipeline, m_handleSize, firstGroup, groupCount);
	}
};

class CaptureReplayHandleGetter : public HandleGetter
{
public:
	CaptureReplayHandleGetter			(const uint32_t shaderGroupHandleCaptureReplaySize) : HandleGetter(shaderGroupHandleCaptureReplaySize)	{}
	virtual ~CaptureReplayHandleGetter	()																				{}

	std::vector<uint8_t> getShaderGroupHandlesVector (const RayTracingPipeline*	rtPipeline,
													  const DeviceInterface&	vkd,
													  const VkDevice			device,
													  const VkPipeline			pipeline,
													  const uint32_t			firstGroup,
													  const uint32_t			groupCount) const override
	{
		return rtPipeline->getShaderGroupReplayHandles(vkd, device, pipeline, m_handleSize, firstGroup, groupCount);
	}
};

std::vector<uint32_t> RayTracingPipelineLibraryTestInstance::runTest (bool replay)
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const auto&							vkd									= m_context.getDeviceInterface();
	const auto							device								= m_context.getDevice();
	const auto							queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const auto							queue								= m_context.getUniversalQueue();
	auto&								allocator							= m_context.getDefaultAllocator();
	const auto							pixelCount							= m_data.getPixelCount();
	const auto							hitGroupCount						= m_data.getHitGroupCount();
	const auto							rayTracingProperties				= makeRayTracingProperties(vki, physicalDevice);
	const uint32_t						shaderGroupHandleSize				= rayTracingProperties->getShaderGroupHandleSize();
	const uint32_t						shaderGroupBaseAlignment			= rayTracingProperties->getShaderGroupBaseAlignment();
	const uint32_t						shaderGroupHandleReplaySize			= rayTracingProperties->getShaderGroupHandleCaptureReplaySize();
	const auto							allGroupOffsets						= m_pipelineTree.getGroupOffsets();

	// Make sure we only replay in CAPTURE_REPLAY handles mode.
	// When checking capture/replay handles, the first iteration will save the handles to m_captureReplayHandles.
	// In the second iteration, the replay argument will be true and we'll use the saved m_captureReplayHandles when creating pipelines.
	if (replay)
		DE_ASSERT(m_data.includesCaptureReplay());

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
	std::vector<std::tuple<int, deUint32, deUint32>> pipelineInfoList;
	{
		// push main pipeline on the list
		deUint32 shaderOffset	= 0U;
		pipelineInfoList.push_back(std::make_tuple(-1, shaderOffset, m_data.libraryConfiguration.pipelineShaders));
		shaderOffset			+= m_data.libraryConfiguration.pipelineShaders;

		for (size_t i = 0; i < m_data.libraryConfiguration.pipelineLibraries.size(); ++i)
		{
			int parentIndex			= m_data.libraryConfiguration.pipelineLibraries[i].x();
			deUint32 shaderCount	= deUint32(m_data.libraryConfiguration.pipelineLibraries[i].y());
			if (parentIndex < 0 || parentIndex >= int(pipelineInfoList.size()) )
				TCU_THROW(InternalError, "Wrong library tree definition");
			pipelineInfoList.push_back(std::make_tuple(parentIndex, shaderOffset, shaderCount));
			shaderOffset			+= shaderCount;
		}
	}

	// create pipeline libraries and build a pipeline tree.
	std::vector<de::SharedPtr<de::MovePtr<RayTracingPipeline>>>					rtPipelines(pipelineInfoList.size());
	std::vector<std::vector<std::tuple<std::string, VkShaderStageFlagBits>>>	pipelineShaders(pipelineInfoList.size());
	for (size_t idx=0; idx < pipelineInfoList.size(); ++idx)
	{
		int			parentIndex;
		deUint32	shaderCount, shaderOffset;
		std::tie(parentIndex, shaderOffset, shaderCount) = pipelineInfoList[idx];

		// create pipeline objects
		de::SharedPtr<de::MovePtr<RayTracingPipeline>> rtPipeline = makeVkSharedPtr(de::MovePtr<RayTracingPipeline>(new RayTracingPipeline));

		(*rtPipeline)->setDeferredOperation(m_data.pipelinesCreatedUsingDHO);

		VkPipelineCreateFlags creationFlags = 0u;

		// all pipelines are pipeline libraries, except for the main pipeline
		if (idx > 0)
			creationFlags |= VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;

		// Sometimes we need capture/replay handles.
		if (m_data.includesCaptureReplay())
			creationFlags |= VK_PIPELINE_CREATE_RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_BIT_KHR;

		if (m_data.useLinkTimeOptimizations)
		{
			if (m_data.retainLinkTimeOptimizations)
				creationFlags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
			else
				creationFlags |= VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
		}

		rtPipeline->get()->setCreateFlags(creationFlags);
		if (m_data.useMaintenance5)
			rtPipeline->get()->setCreateFlags2(translateCreateFlag(creationFlags));

		rtPipeline->get()->setMaxPayloadSize(16U); // because rayPayloadInEXT is uvec4 ( = 16 bytes ) for all chit shaders
		rtPipelines[idx] = rtPipeline;

		// prepare all shader names for all pipelines
		if (idx == 0)
		{
			pipelineShaders[0].push_back(std::make_tuple( "rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR ));
			pipelineShaders[0].push_back(std::make_tuple( "miss", VK_SHADER_STAGE_MISS_BIT_KHR ));
		}
		for (uint32_t i = 0; i < shaderCount; ++i)
		{
			std::stringstream csname;
			csname << "chit" << shaderOffset + i;
			pipelineShaders[idx].push_back(std::make_tuple( csname.str(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR ));
		}
	}

	const auto isecMod	= (m_data.useAABBs
						? createShaderModule(vkd, device, m_context.getBinaryCollection().get("isec"))
						: Move<VkShaderModule>());

	// singlethreaded / multithreaded compilation of all shaders
	if (m_data.multithreadedCompilation)
	{
		std::vector<CompileShadersMultithreadData> csmds;
		for (deUint32 i = 0; i < rtPipelines.size(); ++i)
			csmds.push_back(CompileShadersMultithreadData{ m_context, rtPipelines[i], pipelineShaders[i], isecMod });

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
		for (deUint32 i = 0; i < rtPipelines.size(); ++i)
			compileShaders(m_context, rtPipelines[i], pipelineShaders[i], isecMod);
	}

	// connect libraries into a tree structure
	for (size_t idx = 0; idx < pipelineInfoList.size(); ++idx)
	{
		int			parentIndex;
		deUint32 shaderCount, shaderOffset;
		std::tie(parentIndex, shaderOffset, shaderCount) = pipelineInfoList[idx];
		if (parentIndex != -1)
			rtPipelines[parentIndex]->get()->addLibrary(rtPipelines[idx]);
	}

	// Add the saved capture/replay handles when in replay mode.
	if (replay)
	{
		for (size_t pipelineIdx = 0; pipelineIdx < rtPipelines.size(); ++pipelineIdx)
		{
			const auto pipelineOffsetBytes = allGroupOffsets.at(pipelineIdx) * shaderGroupHandleReplaySize;
			for (size_t groupIdx = 0; groupIdx < pipelineShaders.at(pipelineIdx).size(); ++groupIdx)
			{
				const auto groupOffsetBytes = pipelineOffsetBytes + groupIdx * shaderGroupHandleReplaySize;
				rtPipelines[pipelineIdx]->get()->setGroupCaptureReplayHandle(static_cast<uint32_t>(groupIdx), &m_captureReplayHandles.at(groupOffsetBytes));
			}
		}
	}

	// build main pipeline and all pipeline libraries that it depends on
	const auto										firstRTPipeline	= rtPipelines.at(0)->get();
	std::vector<de::SharedPtr<Move<VkPipeline>>>	pipelines		= firstRTPipeline->createPipelineWithLibraries(vkd, device, *pipelineLayout);
	const VkPipeline								pipeline		= pipelines.at(0)->get();

	// Obtain and verify shader group handles.
	if (m_data.testType != TestType::DEFAULT)
	{
		// When checking all handles, we'll do two iterations, checking the normal handles first and the capture/replay handles later.
		const bool					checkAllHandles	= (m_data.testType == TestType::CHECK_ALL_HANDLES);
		const uint32_t				iterations		= (checkAllHandles ? 2u : 1u);

		for (uint32_t iter = 0u; iter < iterations; ++iter)
		{
			const bool					normalHandles	= (iter == 0u && m_data.testType != TestType::CHECK_CAPTURE_REPLAY_HANDLES);
			const auto					handleSize		= (normalHandles ? shaderGroupHandleSize : shaderGroupHandleReplaySize);
			de::MovePtr<HandleGetter>	handleGetter	(normalHandles
														? static_cast<HandleGetter*>(new NormalHandleGetter(handleSize))
														: static_cast<HandleGetter*>(new CaptureReplayHandleGetter(handleSize)));

			const auto allHandles		= handleGetter->getShaderGroupHandlesVector(firstRTPipeline, vkd, device, pipeline, 0u, firstRTPipeline->getFullShaderGroupCount());
			const auto allGroupCounts	= getAllGroupCounts(rtPipelines);

			DE_ASSERT(allGroupOffsets.size() == rtPipelines.size());
			DE_ASSERT(allGroupCounts.size() == rtPipelines.size());
			DE_ASSERT(rtPipelines.size() == pipelines.size());

			for (size_t idx = 0; idx < rtPipelines.size(); ++idx)
			{
				const auto	curRTPipeline	= rtPipelines[idx]->get();
				const auto&	curPipeline		= pipelines[idx]->get();
				const auto&	curGroupOffset	= allGroupOffsets[idx];
				const auto& curGroupCount	= allGroupCounts[idx];
				const auto	curHandles		= handleGetter->getShaderGroupHandlesVector(curRTPipeline, vkd, device, curPipeline, 0u, curGroupCount);

				const auto	rangeStart		= curGroupOffset * shaderGroupHandleSize;
				const auto	rangeEnd		= (curGroupOffset + curGroupCount) * shaderGroupHandleSize;

				const std::vector<uint8_t> handleRange (allHandles.begin() + rangeStart, allHandles.begin() + rangeEnd);
				if (handleRange != curHandles)
				{
					std::ostringstream msg;
					msg << (normalHandles ? "" : "Capture Replay ") << "Shader Group Handle verification failed for pipeline " << idx;
					TCU_FAIL(msg.str());
				}
			}

			// Save or check capture/replay handles.
			if (!normalHandles)
			{
				if (replay)
				{
					// Check saved handles.
					if (allHandles != m_captureReplayHandles)
						TCU_FAIL("Capture Replay Shader Group Handles do not match creation handles for top-level pipeline");
				}
				else
				{
					// Save handles for the replay phase.
					m_captureReplayHandles = allHandles;
				}
			}
		}
	}

	// build shader binding tables
	const de::MovePtr<BufferWithMemory>		raygenShaderBindingTable			= firstRTPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
	const de::MovePtr<BufferWithMemory>		missShaderBindingTable				= firstRTPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1 );
	const de::MovePtr<BufferWithMemory>		hitShaderBindingTable				= firstRTPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, hitGroupCount);
	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, hitGroupCount * shaderGroupHandleSize);
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
	auto&								resultBufferAlloc					= resultBuffer->getAllocation();

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

		bottomLevelAccelerationStructures	= initBottomAccelerationStructures(*cmdBuffer);
		topLevelAccelerationStructure		= initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructures);

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

	invalidateAlloc(vkd, device, resultBufferAlloc);

	std::vector<uint32_t> resultVector (pixelCount);
	deMemcpy(resultVector.data(), resultBufferAlloc.getHostPtr(), de::dataSize(resultVector));

	return resultVector;
}

tcu::TestStatus RayTracingPipelineLibraryTestInstance::iterate (void)
{
	// run test using arrays of pointers
	const auto	numShadersUsed	= m_data.getHitGroupCount();
	const auto	bufferVec		= runTest();

	if (m_data.includesCaptureReplay())
	{
		const auto replayResults = runTest(true/*replay*/);
		if (bufferVec != replayResults)
			return tcu::TestStatus::fail("Replay results differ from original results");
	}

	deUint32	failures		= 0;
	deUint32	pos				= 0;
	deUint32	shaderIdx		= 0;

	// Verify results.
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

			if (bufferVec.at(pos) != expectedResult)
				failures++;

			++pos;
		}

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("failures=" + de::toString(failures));
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

	struct
	{
		const TestType	testType;
		const char*		suffix;
	} testTypeCases[] =
	{
		{ TestType::DEFAULT,						""									},
		{ TestType::CHECK_GROUP_HANDLES,			"_check_group_handles"				},
		{ TestType::CHECK_CAPTURE_REPLAY_HANDLES,	"_check_capture_replay_handles"		},
		{ TestType::CHECK_ALL_HANDLES,				"_check_all_handles"				},
	};

	struct
	{
		const bool		useAABBs;
		const char*		suffix;
	} geometryTypeCases[] =
	{
		{ false,	""			},
		{ true,		"_aabbs"	},
	};

	for (size_t threadNdx = 0; threadNdx < DE_LENGTH_OF_ARRAY(threadData); ++threadNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> threadGroup(new tcu::TestCaseGroup(group->getTestContext(), threadData[threadNdx].name, ""));

		for (size_t libConfigNdx = 0; libConfigNdx < DE_LENGTH_OF_ARRAY(libraryConfigurationData); ++libConfigNdx)
		{
			for (const auto& testTypeCase : testTypeCases)
			{
				for (const auto& geometryCase : geometryTypeCases)
				{
					TestParams testParams
					{
						libraryConfigurationData[libConfigNdx].libraryConfiguration,
						threadData[threadNdx].multithreaded,
						threadData[threadNdx].pipelinesCreatedUsingDHO,
						testTypeCase.testType,
						geometryCase.useAABBs,
						false,
						false,
						false,
						RTPL_DEFAULT_SIZE,
						RTPL_DEFAULT_SIZE
					};

					const std::string testName = std::string(libraryConfigurationData[libConfigNdx].name) + geometryCase.suffix + testTypeCase.suffix;
					threadGroup->addChild(new RayTracingPipelineLibraryTestCase(group->getTestContext(), testName.c_str(), "", testParams));
				}
			}
		}
		group->addChild(threadGroup.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(group->getTestContext(), "misc", ""));

		TestParams testParamsMaintenance5
		{
			libraryConfigurationData[1].libraryConfiguration,
			false,
			false,
			TestType::CHECK_CAPTURE_REPLAY_HANDLES,
			false,
			true,
			false,
			true,
			RTPL_DEFAULT_SIZE,
			RTPL_DEFAULT_SIZE
		};
		miscGroup->addChild(new RayTracingPipelineLibraryTestCase(group->getTestContext(), "maintenance5", "", testParamsMaintenance5));

		TestParams testParamsUseLinkTimeOpt
		{
			libraryConfigurationData[5].libraryConfiguration,
			false,
			false,
			TestType::DEFAULT,
			true,
			true,
			false,
			false,
			RTPL_DEFAULT_SIZE,
			RTPL_DEFAULT_SIZE
		};
		miscGroup->addChild(new RayTracingPipelineLibraryTestCase(group->getTestContext(), "use_link_time_optimizations", "", testParamsUseLinkTimeOpt));

		TestParams testParamsRetainLinkTimeOpt
		{
			libraryConfigurationData[5].libraryConfiguration,
			false,
			false,
			TestType::DEFAULT,
			true,
			true,
			true,
			false,
			RTPL_DEFAULT_SIZE,
			RTPL_DEFAULT_SIZE
		};
		miscGroup->addChild(new RayTracingPipelineLibraryTestCase(group->getTestContext(), "retain_link_time_optimizations", "", testParamsRetainLinkTimeOpt));

		group->addChild(miscGroup.release());
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
