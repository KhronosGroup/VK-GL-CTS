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
 * \brief Ray Query Builtin tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryBuiltinTests.hpp"

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

enum TestType
{
	TEST_TYPE_FLOW							= 0,
	TEST_TYPE_PRIMITIVE_ID,
	TEST_TYPE_INSTANCE_ID,
	TEST_TYPE_INSTANCE_CUSTOM_INDEX,
	TEST_TYPE_INTERSECTION_T_KHR,
	TEST_TYPE_OBJECT_RAY_ORIGIN_KHR,
	TEST_TYPE_OBJECT_RAY_DIRECTION_KHR,
	TEST_TYPE_OBJECT_TO_WORLD_KHR,
	TEST_TYPE_WORLD_TO_OBJECT_KHR,
	TEST_TYPE_NULL_ACCELERATION_STRUCTURE,
	TEST_TYPE_LAST
};

enum GeomType
{
	GEOM_TYPE_TRIANGLES,
	GEOM_TYPE_AABBS,
	GEOM_TYPE_LAST,
};

const deUint32	TEST_WIDTH					= 8;
const deUint32	TEST_HEIGHT					= 8;
const deUint32	FIXED_POINT_DIVISOR			= 1024 * 1024;
const deUint32	FIXED_POINT_ALLOWED_ERROR	= 4;

struct TestParams;

typedef void (*CheckSupportFunc)(Context& context, const TestParams& testParams);
typedef void (*InitProgramsFunc)(SourceCollections& programCollection, const TestParams& testParams);
typedef const std::string (*ShaderBodyTextFunc)(const TestParams& testParams);

class PipelineConfiguration
{
public:
					PipelineConfiguration	()	{};
	virtual			~PipelineConfiguration	()	{};

	virtual void	initConfiguration	(Context&							context,
										 TestParams&						testParams) = 0;
	virtual void	fillCommandBuffer	(Context&							context,
										 TestParams&						testParams,
										 VkCommandBuffer					commandBuffer,
										 const VkAccelerationStructureKHR*	rayQueryTopAccelerationStructurePtr,
										 const VkDescriptorImageInfo&		resultImageInfo) = 0;
};

class TestConfiguration
{
public:
																	TestConfiguration				()
																		: m_bottomAccelerationStructures	()
																		, m_topAccelerationStructure		()
																		, m_expected						()
																	{
																	}
	virtual															~TestConfiguration				()
																	{
																	}

	virtual const VkAccelerationStructureKHR*						initAccelerationStructures		(Context&							context,
																									 TestParams&						testParams,
																									 VkCommandBuffer					cmdBuffer) = 0;
	virtual bool													verify							(BufferWithMemory*					resultBuffer,
																									 Context&							context,
																									 TestParams&						testParams);

protected:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	m_bottomAccelerationStructures;
	de::SharedPtr<TopLevelAccelerationStructure>					m_topAccelerationStructure;
	std::vector<deInt32>											m_expected;
};

class TestConfigurationFloat : public TestConfiguration
{
public:
																	TestConfigurationFloat			()
																		: TestConfiguration	()
																	{
																	}
	virtual															~TestConfigurationFloat			()
																	{
																	}
	virtual bool													verify							(BufferWithMemory*					resultBuffer,
																									 Context&							context,
																									 TestParams&						testParams) override;
};

class TestConfigurationVector : public TestConfiguration
{
public:
																	TestConfigurationVector			()
																		: TestConfiguration	()
																	{
																	}
	virtual															~TestConfigurationVector		()
																	{
																	}
	virtual bool													verify							(BufferWithMemory*					resultBuffer,
																									 Context&							context,
																									 TestParams&						testParams) override;
};

class TestConfigurationMatrix : public TestConfiguration
{
public:
																	TestConfigurationMatrix			()
																		: TestConfiguration	()
																	{
																	}
	virtual															~TestConfigurationMatrix		()
																	{
																	}
	virtual bool													verify							(BufferWithMemory*					resultBuffer,
																									 Context&							context,
																									 TestParams&						testParams) override;
};

struct TestParams
{
	deUint32				width;
	deUint32				height;
	deUint32				depth;
	TestType				testType;
	VkShaderStageFlagBits	stage;
	GeomType				geomType;
	deUint32				squaresGroupCount;
	deUint32				geometriesGroupCount;
	deUint32				instancesGroupCount;
	VkFormat				format;
	CheckSupportFunc		pipelineCheckSupport;
	InitProgramsFunc		pipelineInitPrograms;
	ShaderBodyTextFunc		testConfigShaderBodyText;
	CheckSupportFunc		testConfigCheckSupport;
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

VkImageCreateInfo makeImageCreateInfo (VkFormat				format,
									   deUint32				width,
									   deUint32				height,
									   deUint32				depth,
									   VkImageType			imageType	= VK_IMAGE_TYPE_3D,
									   VkImageUsageFlags	usageFlags	= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
{
	const VkImageCreateInfo	imageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		imageType,								// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(width, height, depth),		// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usageFlags,								// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

Move<VkPipeline> makeComputePipeline (const DeviceInterface&		vk,
									  const VkDevice				device,
									  const VkPipelineLayout		pipelineLayout,
									  const VkShaderModule			shaderModule)
{
	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		shaderModule,											// VkShaderModule						module;
		"main",													// const char*							pName;
		DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};

	return createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo);
}

static const std::string getMissPassthrough (void)
{
	const std::string missPassthrough =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";

	return missPassthrough;
}

static const std::string getHitPassthrough (void)
{
	const std::string hitPassthrough =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"hitAttributeEXT vec3 attribs;\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";

	return hitPassthrough;
}

static const std::string getGraphicsPassthrough (void)
{
	std::ostringstream src;

	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "}\n";

	return src.str();
}

static const std::string getVertexPassthrough (void)
{
	std::ostringstream src;

	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
		<< "\n"
		<< "layout(location = 0) in vec4 in_position;\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "  gl_Position = in_position;\n"
		<< "}\n";

	return src.str();
}

class GraphicsConfiguration : public PipelineConfiguration
{
public:
	static void						checkSupport			(Context&							context,
															 const TestParams&					testParams);
	static void						initPrograms			(SourceCollections&					programCollection,
															 const TestParams&					testParams);

									GraphicsConfiguration	();
	virtual							~GraphicsConfiguration	() {};

	void							initVertexBuffer		(Context&							context,
															 TestParams&						testParams);
	Move<VkPipeline>				makeGraphicsPipeline	(Context&							context,
															 TestParams&						testParams);
	virtual void					initConfiguration		(Context&							context,
															 TestParams&						testParams) override;
	virtual void					fillCommandBuffer		(Context&							context,
															 TestParams&						testParams,
															 VkCommandBuffer					commandBuffer,
															 const VkAccelerationStructureKHR*	rayQueryTopAccelerationStructurePtr,
															 const VkDescriptorImageInfo&		resultImageInfo) override;

private:
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSet>			m_descriptorSet;

	VkFormat						m_framebufferFormat;
	Move<VkImage>					m_framebufferImage;
	de::MovePtr<Allocation>			m_framebufferImageAlloc;
	Move<VkImageView>				m_framebufferAttachment;

	Move<VkShaderModule>			m_vertShaderModule;
	Move<VkShaderModule>			m_geomShaderModule;
	Move<VkShaderModule>			m_tescShaderModule;
	Move<VkShaderModule>			m_teseShaderModule;
	Move<VkShaderModule>			m_fragShaderModule;

	Move<VkRenderPass>				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkPipeline>				m_pipeline;

	deUint32						m_vertexCount;
	Move<VkBuffer>					m_vertexBuffer;
	de::MovePtr<Allocation>			m_vertexBufferAlloc;
};

GraphicsConfiguration::GraphicsConfiguration()
	: PipelineConfiguration		()
	, m_descriptorSetLayout		()
	, m_descriptorPool			()
	, m_descriptorSet			()
	, m_framebufferFormat		(VK_FORMAT_R8G8B8A8_UNORM)
	, m_framebufferImage		()
	, m_framebufferImageAlloc	()
	, m_framebufferAttachment	()
	, m_vertShaderModule		()
	, m_geomShaderModule		()
	, m_tescShaderModule		()
	, m_teseShaderModule		()
	, m_fragShaderModule		()
	, m_renderPass				()
	, m_framebuffer				()
	, m_pipelineLayout			()
	, m_pipeline				()
	, m_vertexCount				(0)
	, m_vertexBuffer			()
	, m_vertexBufferAlloc		()
{
}

void GraphicsConfiguration::checkSupport (Context&			context,
										  const TestParams&	testParams)
{
	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
			break;

		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		{
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

			break;
		}

		case VK_SHADER_STAGE_GEOMETRY_BIT:
		{
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

			break;
		}

		case VK_SHADER_STAGE_FRAGMENT_BIT:
			break;

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

void GraphicsConfiguration::initPrograms (SourceCollections&	programCollection,
										  const TestParams&		testParams)
{
	const vk::ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	const std::string				testShaderBody		= testParams.testConfigShaderBodyText(testParams);

	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		{
			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  const int   posId    = int(gl_VertexIndex / 3);\n"
					<< "  const int   vertId   = int(gl_VertexIndex % 3);\n"
					<< "  const ivec3 size     = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "  const ivec3 pos      = ivec3(posId % size.x, posId / size.x, 0);\n"
					<< "\n"
					<< "  if (vertId == 0)\n"
					<< "  {\n"
					<< "    testFunc(pos, size);\n"
					<< "  }\n"
					<< "}\n";

				programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << buildOptions;
			}

			programCollection.glslSources.add("frag") << glu::FragmentSource(getGraphicsPassthrough()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		{
			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "\n"
					<< "layout(location = 0) in vec4 in_position;\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "};\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  gl_Position = in_position;\n"
					<< "}\n";

				programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << buildOptions;
			}

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_tessellation_shader : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_in[];\n"
					<< "layout(vertices = 4) out;\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_out[];\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "\n"
					<< "  if (gl_InvocationID == 0)\n"
					<< "  {\n"
					<< "    const ivec3 size = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "    for (int y = 0; y < size.y; y++)\n"
					<< "    for (int x = 0; x < size.x; x++)\n"
					<< "    {\n"
					<< "      const ivec3 pos = ivec3(x, y, 0);\n"
					<< "      testFunc(pos, size);\n"
					<< "    }\n"
					<< "  }\n"
					<< "\n"
					<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
					<< "  gl_TessLevelInner[0] = 1;\n"
					<< "  gl_TessLevelInner[1] = 1;\n"
					<< "  gl_TessLevelOuter[gl_InvocationID] = 1;\n"
					<< "}\n";

				programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str()) << buildOptions;
			}

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_tessellation_shader : require\n"
					<< "layout(quads, equal_spacing, ccw) in;\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_in[];\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  gl_Position = gl_in[0].gl_Position;\n"
					<< "}\n";

				programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str()) << buildOptions;
			}

			break;
		}

		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		{
			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "\n"
					<< "layout(location = 0) in vec4 in_position;\n"
					<< "out gl_PerVertex"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "};\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  gl_Position = in_position;\n"
					<< "}\n";

				programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << buildOptions;
			}

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_tessellation_shader : require\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_in[];\n"
					<< "layout(vertices = 4) out;\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_out[];\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
					<< "  gl_TessLevelInner[0] = 1;\n"
					<< "  gl_TessLevelInner[1] = 1;\n"
					<< "  gl_TessLevelOuter[gl_InvocationID] = 1;\n"
					<< "}\n";

				programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str()) << buildOptions;
			}

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_tessellation_shader : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					<< "layout(quads, equal_spacing, ccw) in;\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_in[];\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  const ivec3 size = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "\n"
					<< "  if (gl_PrimitiveID == 0)\n"
					<< "  {\n"
					<< "    const ivec3 size = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "    for (int y = 0; y < size.y; y++)\n"
					<< "    for (int x = 0; x < size.x; x++)\n"
					<< "    {\n"
					<< "      const ivec3 pos = ivec3(x, y, 0);\n"
					<< "      testFunc(pos, size);\n"
					<< "    }\n"
					<< "  }\n"
					<< "\n"
					<< "  gl_Position = gl_in[0].gl_Position;\n"
					<< "}\n";

				programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str()) << buildOptions;
			}

			break;
		}

		case VK_SHADER_STAGE_GEOMETRY_BIT:
		{
			programCollection.glslSources.add("vert") << glu::VertexSource(getVertexPassthrough()) << buildOptions;

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "layout(triangles) in;\n"
					<< "layout(points, max_vertices = 1) out;\n"
					<< "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  const int   posId    = int(gl_PrimitiveIDIn);\n"
					<< "  const ivec3 size     = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "  const ivec3 pos      = ivec3(posId % size.x, posId / size.x, 0);\n"
					<< "\n"
					<< "  testFunc(pos, size);\n"
					<< "}\n";

				programCollection.glslSources.add("geom") << glu::GeometrySource(src.str()) << buildOptions;
			}

			break;
		}

		case VK_SHADER_STAGE_FRAGMENT_BIT:
		{
			programCollection.glslSources.add("vert") << glu::VertexSource(getVertexPassthrough()) << buildOptions;

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  const ivec3 size     = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "  const ivec3 pos      = ivec3(int(gl_FragCoord.x - 0.5f), int(gl_FragCoord.y - 0.5f), 0);\n"
					<< "\n"
					<< "  testFunc(pos, size);\n"
					<< "}\n";

				programCollection.glslSources.add("frag") << glu::FragmentSource(src.str()) << buildOptions;
			}

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

void GraphicsConfiguration::initVertexBuffer (Context&		context,
											  TestParams&	testParams)
{
	const DeviceInterface&	vkd			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	const deUint32			width		= testParams.width;
	const deUint32			height		= testParams.height;
	Allocator&				allocator	= context.getDefaultAllocator();
	std::vector<tcu::Vec4>	vertices;

	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		{
			const float z = 0.0f;
			const float w = 1.0f;

			vertices.reserve(3 * height * width);

			for (deUint32 y = 0; y < height; ++y)
			for (deUint32 x = 0; x < width; ++x)
			{
				const float	x0	= float(x + 0) / float(width);
				const float	y0	= float(y + 0) / float(height);
				const float	x1	= float(x + 1) / float(width);
				const float	y1	= float(y + 1) / float(height);
				const float	xm	= (x0 + x1) / 2.0f;
				const float	ym	= (y0 + y1) / 2.0f;

				vertices.push_back(tcu::Vec4(x0, y0, z, w));
				vertices.push_back(tcu::Vec4(xm, y1, z, w));
				vertices.push_back(tcu::Vec4(x1, ym, z, w));
			}

			break;
		}

		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		{
			const float		z = 0.0f;
			const float		w = 1.0f;
			const tcu::Vec4	a = tcu::Vec4(-1.0f, -1.0f, z, w);
			const tcu::Vec4	b = tcu::Vec4(+1.0f, -1.0f, z, w);
			const tcu::Vec4	c = tcu::Vec4(+1.0f, +1.0f, z, w);
			const tcu::Vec4	d = tcu::Vec4(-1.0f, +1.0f, z, w);

			vertices.push_back(a);
			vertices.push_back(b);
			vertices.push_back(c);
			vertices.push_back(d);

			break;
		}

		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		{
			const float		z = 0.0f;
			const float		w = 1.0f;
			const tcu::Vec4	a = tcu::Vec4(-1.0f, -1.0f, z, w);
			const tcu::Vec4	b = tcu::Vec4(+1.0f, -1.0f, z, w);
			const tcu::Vec4	c = tcu::Vec4(+1.0f, +1.0f, z, w);
			const tcu::Vec4	d = tcu::Vec4(-1.0f, +1.0f, z, w);

			vertices.push_back(a);
			vertices.push_back(b);
			vertices.push_back(c);
			vertices.push_back(d);

			break;
		}

		case VK_SHADER_STAGE_GEOMETRY_BIT:
		{
			const float z = 0.0f;
			const float w = 1.0f;

			vertices.reserve(3 * height * width);

			for (deUint32 y = 0; y < height; ++y)
			for (deUint32 x = 0; x < width; ++x)
			{
				const float	x0	= float(x + 0) / float(width);
				const float	y0	= float(y + 0) / float(height);
				const float	x1	= float(x + 1) / float(width);
				const float	y1	= float(y + 1) / float(height);
				const float	xm	= (x0 + x1) / 2.0f;
				const float	ym	= (y0 + y1) / 2.0f;

				vertices.push_back(tcu::Vec4(x0, y0, z, w));
				vertices.push_back(tcu::Vec4(xm, y1, z, w));
				vertices.push_back(tcu::Vec4(x1, ym, z, w));
			}

			break;
		}

		case VK_SHADER_STAGE_FRAGMENT_BIT:
		{
			const float		z = 1.0f;
			const float		w = 1.0f;
			const tcu::Vec4	a = tcu::Vec4(-1.0f, -1.0f, z, w);
			const tcu::Vec4	b = tcu::Vec4(+1.0f, -1.0f, z, w);
			const tcu::Vec4	c = tcu::Vec4(-1.0f, +1.0f, z, w);
			const tcu::Vec4	d = tcu::Vec4(+1.0f, +1.0f, z, w);

			vertices.push_back(a);
			vertices.push_back(b);
			vertices.push_back(c);

			vertices.push_back(b);
			vertices.push_back(c);
			vertices.push_back(d);

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");

	}

	// Initialize vertex buffer
	{
		const VkDeviceSize			vertexBufferSize		= sizeof(vertices[0][0]) * vertices[0].SIZE * vertices.size();
		const VkBufferCreateInfo	vertexBufferCreateInfo	= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		m_vertexCount		= static_cast<deUint32>(vertices.size());
		m_vertexBuffer		= createBuffer(vkd, device, &vertexBufferCreateInfo);
		m_vertexBufferAlloc	= bindBuffer(vkd, device, allocator, *m_vertexBuffer, vk::MemoryRequirement::HostVisible);

		deMemcpy(m_vertexBufferAlloc->getHostPtr(), vertices.data(), (size_t)vertexBufferSize);
		flushAlloc(vkd, device, *m_vertexBufferAlloc);
	}
}

Move<VkPipeline> GraphicsConfiguration::makeGraphicsPipeline (Context&		context,
															  TestParams&	testParams)
{
	const DeviceInterface&			vkd					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	const bool						tessStageTest		= (testParams.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || testParams.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	const VkPrimitiveTopology		topology			= tessStageTest ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	const deUint32					patchControlPoints	= tessStageTest ? 4 : 0;
	const std::vector<VkViewport>	viewports			(1, makeViewport(testParams.width, testParams.height));
	const std::vector<VkRect2D>		scissors			(1, makeRect2D(testParams.width, testParams.height));

	return vk::makeGraphicsPipeline	(vkd,
									 device,
									 *m_pipelineLayout,
									 *m_vertShaderModule,
									 *m_tescShaderModule,
									 *m_teseShaderModule,
									 *m_geomShaderModule,
									 *m_fragShaderModule,
									 *m_renderPass,
									 viewports,
									 scissors,
									 topology,
									 0,
									 patchControlPoints);
}

void GraphicsConfiguration::initConfiguration (Context&		context,
											   TestParams&	testParams)
{
	const DeviceInterface&	vkd			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	Allocator&				allocator	= context.getDefaultAllocator();
	vk::BinaryCollection&	collection	= context.getBinaryCollection();
	VkShaderStageFlags		shaders		= static_cast<VkShaderStageFlags>(0);
	deUint32				shaderCount	= 0;

	if (collection.contains("vert")) shaders |= VK_SHADER_STAGE_VERTEX_BIT;
	if (collection.contains("geom")) shaders |= VK_SHADER_STAGE_GEOMETRY_BIT;
	if (collection.contains("tesc")) shaders |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	if (collection.contains("tese")) shaders |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	if (collection.contains("frag")) shaders |= VK_SHADER_STAGE_FRAGMENT_BIT;

	for (BinaryCollection::Iterator it = collection.begin(); it != collection.end(); ++it)
		shaderCount++;

	if (shaderCount != (deUint32)dePop32(shaders))
		TCU_THROW(InternalError, "Unused shaders detected in the collection");

	if (0 != (shaders & VK_SHADER_STAGE_VERTEX_BIT))					m_vertShaderModule = createShaderModule(vkd, device, collection.get("vert"), 0);
	if (0 != (shaders & VK_SHADER_STAGE_GEOMETRY_BIT))					m_geomShaderModule = createShaderModule(vkd, device, collection.get("geom"), 0);
	if (0 != (shaders & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT))		m_tescShaderModule = createShaderModule(vkd, device, collection.get("tesc"), 0);
	if (0 != (shaders & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))	m_teseShaderModule = createShaderModule(vkd, device, collection.get("tese"), 0);
	if (0 != (shaders & VK_SHADER_STAGE_FRAGMENT_BIT))					m_fragShaderModule = createShaderModule(vkd, device, collection.get("frag"), 0);

	m_descriptorSetLayout	= DescriptorSetLayoutBuilder()
								.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL_GRAPHICS)
								.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL_GRAPHICS)
								.build(vkd, device);
	m_descriptorPool		= DescriptorPoolBuilder()
								.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
								.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
								.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	m_descriptorSet			= makeDescriptorSet		(vkd, device, *m_descriptorPool, *m_descriptorSetLayout);
	m_framebufferImage		= makeImage				(vkd, device, makeImageCreateInfo(m_framebufferFormat, testParams.width, testParams.height, 1u, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
	m_framebufferImageAlloc	= bindImage				(vkd, device, allocator, *m_framebufferImage, MemoryRequirement::Any);
	m_framebufferAttachment	= makeImageView			(vkd, device, *m_framebufferImage, VK_IMAGE_VIEW_TYPE_2D, m_framebufferFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	m_renderPass			= makeRenderPass		(vkd, device, m_framebufferFormat);
	m_framebuffer			= makeFramebuffer		(vkd, device, *m_renderPass, *m_framebufferAttachment, testParams.width, testParams.height);
	m_pipelineLayout		= makePipelineLayout	(vkd, device, m_descriptorSetLayout.get());
	m_pipeline				= makeGraphicsPipeline	(context, testParams);

	initVertexBuffer(context, testParams);
}

void GraphicsConfiguration::fillCommandBuffer (Context&								context,
											   TestParams&							testParams,
											   VkCommandBuffer						cmdBuffer,
											   const VkAccelerationStructureKHR*	rayQueryTopAccelerationStructurePtr,
											   const VkDescriptorImageInfo&			resultImageInfo)
{
	const DeviceInterface&								vkd												= context.getDeviceInterface();
	const VkDevice										device											= context.getDevice();
	const VkDeviceSize									vertexBufferOffset								= 0;
	const VkWriteDescriptorSetAccelerationStructureKHR	rayQueryAccelerationStructureWriteDescriptorSet	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		rayQueryTopAccelerationStructurePtr,								//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &rayQueryAccelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &m_descriptorSet.get(), 0, DE_NULL);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);

	beginRenderPass(vkd, cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, testParams.width, testParams.height), tcu::UVec4());

	vkd.cmdDraw(cmdBuffer, m_vertexCount, 1u, 0u, 0u);

	endRenderPass(vkd, cmdBuffer);
}

class ComputeConfiguration : public PipelineConfiguration
{
public:
								ComputeConfiguration	();
	virtual						~ComputeConfiguration	() {};

	static void					checkSupport			(Context&							context,
														 const TestParams&					testParams);
	static void					initPrograms			(SourceCollections&					programCollection,
														 const TestParams&					testParams);

	virtual void				initConfiguration		(Context&							context,
														 TestParams&						testParams) override;
	virtual void				fillCommandBuffer		(Context&							context,
														 TestParams&						testParams,
														 VkCommandBuffer					commandBuffer,
														 const VkAccelerationStructureKHR*	rayQueryTopAccelerationStructurePtr,
														 const VkDescriptorImageInfo&		resultImageInfo) override;

protected:
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSet>		m_descriptorSet;
	Move<VkPipelineLayout>		m_pipelineLayout;

	Move<VkShaderModule>		m_shaderModule;

	Move<VkPipeline>			m_pipeline;
};

ComputeConfiguration::ComputeConfiguration ()
	: PipelineConfiguration	()
	, m_descriptorSetLayout	()
	, m_descriptorPool		()
	, m_descriptorSet		()
	, m_pipelineLayout		()

	, m_shaderModule		()

	, m_pipeline			()
{
}

void ComputeConfiguration::checkSupport (Context&			context,
										 const TestParams&	testParams)
{
	DE_UNREF(context);
	DE_UNREF(testParams);
}

void ComputeConfiguration::initPrograms (SourceCollections&	programCollection,
										 const TestParams&	testParams)
{
	const vk::ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	const std::string				testShaderBody		= testParams.testConfigShaderBodyText(testParams);
	const std::string				testBody			=
		"  ivec3       pos      = ivec3(gl_WorkGroupID);\n"
		"  ivec3       size     = ivec3(gl_NumWorkGroups);\n"
		+ testShaderBody;

	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_COMPUTE_BIT:
		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
				"\n"
				"void main()\n"
				"{\n"
				<< testBody <<
				"}\n";

			programCollection.glslSources.add("comp") << glu::ComputeSource(updateRayTracingGLSL(css.str())) << buildOptions;

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

void ComputeConfiguration::initConfiguration (Context&		context,
											  TestParams&	testParams)
{
	DE_UNREF(testParams);

	const DeviceInterface&	vkd			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	vk::BinaryCollection&	collection	= context.getBinaryCollection();

	m_descriptorSetLayout	= DescriptorSetLayoutBuilder()
								.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
								.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_COMPUTE_BIT)
								.build(vkd, device);
	m_descriptorPool		= DescriptorPoolBuilder()
								.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
								.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
								.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	m_descriptorSet			= makeDescriptorSet(vkd, device, *m_descriptorPool, *m_descriptorSetLayout);
	m_pipelineLayout		= makePipelineLayout(vkd, device, m_descriptorSetLayout.get());
	m_shaderModule			= createShaderModule(vkd, device, collection.get("comp"), 0);
	m_pipeline				= makeComputePipeline(vkd, device, *m_pipelineLayout, *m_shaderModule);
}

void ComputeConfiguration::fillCommandBuffer (Context&							context,
											  TestParams&						testParams,
											  VkCommandBuffer					cmdBuffer,
											  const VkAccelerationStructureKHR*	rayQueryTopAccelerationStructurePtr,
											  const VkDescriptorImageInfo&		resultImageInfo)
{
	const DeviceInterface&								vkd												= context.getDeviceInterface();
	const VkDevice										device											= context.getDevice();
	const VkWriteDescriptorSetAccelerationStructureKHR	rayQueryAccelerationStructureWriteDescriptorSet	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		rayQueryTopAccelerationStructurePtr,								//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &rayQueryAccelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, 1, &m_descriptorSet.get(), 0, DE_NULL);

	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline.get());

	vkd.cmdDispatch(cmdBuffer, testParams.width, testParams.height, 1);
}

class RayTracingConfiguration : public PipelineConfiguration
{
public:
													RayTracingConfiguration				();
	virtual											~RayTracingConfiguration			() {};

	static void										checkSupport						(Context&							context,
																						 const TestParams&					testParams);
	static void										initPrograms						(SourceCollections&					programCollection,
																						 const TestParams&					testParams);

	virtual void									initConfiguration					(Context&							context,
																						 TestParams&						testParams) override;
	virtual void									fillCommandBuffer					(Context&							context,
																						 TestParams&						testParams,
																						 VkCommandBuffer					commandBuffer,
																						 const VkAccelerationStructureKHR*	rayQueryTopAccelerationStructurePtr,
																						 const VkDescriptorImageInfo&		resultImageInfo) override;

protected:
	de::MovePtr<BufferWithMemory>					createShaderBindingTable			(const InstanceInterface&			vki,
																						 const DeviceInterface&				vkd,
																						 const VkDevice						device,
																						 const VkPhysicalDevice				physicalDevice,
																						 const VkPipeline					pipeline,
																						 Allocator&							allocator,
																						 de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																						 const deUint32						group);

protected:
	deUint32										m_shaders;
	deUint32										m_raygenShaderGroup;
	deUint32										m_missShaderGroup;
	deUint32										m_hitShaderGroup;
	deUint32										m_callableShaderGroup;
	deUint32										m_shaderGroupCount;

	Move<VkDescriptorSetLayout>						m_descriptorSetLayout;
	Move<VkDescriptorPool>							m_descriptorPool;
	Move<VkDescriptorSet>							m_descriptorSet;
	Move<VkPipelineLayout>							m_pipelineLayout;

	de::MovePtr<RayTracingPipeline>					m_rayTracingPipeline;
	Move<VkPipeline>								m_pipeline;

	de::MovePtr<BufferWithMemory>					m_raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_missShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_callableShaderBindingTable;

	VkStridedDeviceAddressRegionKHR					m_raygenShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_missShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_hitShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_callableShaderBindingTableRegion;

	de::SharedPtr<BottomLevelAccelerationStructure>	m_bottomLevelAccelerationStructure;
	de::SharedPtr<TopLevelAccelerationStructure>	m_topLevelAccelerationStructure;
};

RayTracingConfiguration::RayTracingConfiguration()
	: m_shaders								(0)
	, m_raygenShaderGroup					(~0u)
	, m_missShaderGroup						(~0u)
	, m_hitShaderGroup						(~0u)
	, m_callableShaderGroup					(~0u)
	, m_shaderGroupCount					(0)

	, m_descriptorSetLayout					()
	, m_descriptorPool						()
	, m_descriptorSet						()
	, m_pipelineLayout						()

	, m_rayTracingPipeline					()
	, m_pipeline							()

	, m_raygenShaderBindingTable			()
	, m_hitShaderBindingTable				()
	, m_missShaderBindingTable				()
	, m_callableShaderBindingTable			()

	, m_raygenShaderBindingTableRegion		()
	, m_missShaderBindingTableRegion		()
	, m_hitShaderBindingTableRegion			()
	, m_callableShaderBindingTableRegion	()

	, m_bottomLevelAccelerationStructure	()
	, m_topLevelAccelerationStructure		()
{
}

void RayTracingConfiguration::checkSupport (Context&			context,
											const TestParams&	testParams)
{
	DE_UNREF(testParams);

	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
}

void RayTracingConfiguration::initPrograms (SourceCollections&	programCollection,
											const TestParams&	testParams)
{
	const vk::ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	const std::string				testShaderBody		= testParams.testConfigShaderBodyText(testParams);
	const std::string				testBody			=
		"  ivec3       pos      = ivec3(gl_LaunchIDEXT);\n"
		"  ivec3       size     = ivec3(gl_LaunchSizeEXT);\n"
		+ testShaderBody;

	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"#extension GL_EXT_ray_query : require\n"
				"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
				"layout(set = 0, binding = 2) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
				"\n"
				"void main()\n"
				"{\n"
				<< testBody <<
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"#extension GL_EXT_ray_query : require\n"
					"hitAttributeEXT vec3 attribs;\n"
					"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					"layout(set = 0, binding = 2) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					"\n"
					"void main()\n"
					"{\n"
					<< testBody <<
					"}\n";

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
			}

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"#extension GL_EXT_ray_query : require\n"
					"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					"hitAttributeEXT vec3 attribs;\n"
					"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					"layout(set = 0, binding = 2) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					"\n"
					"void main()\n"
					"{\n"
					<< testBody <<
					"}\n";

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"#extension GL_EXT_ray_query : require\n"
					"hitAttributeEXT vec3 hitAttribute;\n"
					"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					"layout(set = 0, binding = 2) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					"\n"
					"void main()\n"
					"{\n"
					<< testBody <<
					"  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
					"  reportIntersectionEXT(1.0f, 0);\n"
					"}\n";

				programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_MISS_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"#extension GL_EXT_ray_query : require\n"
					"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					"layout(set = 0, binding = 2) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					"\n"
					"void main()\n"
					"{\n"
					<< testBody <<
					"}\n";

				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
		{
			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"#extension GL_EXT_ray_query : require\n"
					"layout(location = 0) callableDataEXT float dummy;"
					"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
					"\n"
					"void main()\n"
					"{\n"
					"  executeCallableEXT(0, 0);\n"
					"}\n";

				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
			}

			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"#extension GL_EXT_ray_query : require\n"
					"layout(location = 0) callableDataInEXT float dummy;"
					"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					"layout(set = 0, binding = 2) uniform accelerationStructureEXT rayQueryTopLevelAccelerationStructure;\n"
					"\n"
					"void main()\n"
					"{\n"
					<< testBody <<
					"}\n";

				programCollection.glslSources.add("call") << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

de::MovePtr<BufferWithMemory> RayTracingConfiguration::createShaderBindingTable (const InstanceInterface&			vki,
																				 const DeviceInterface&				vkd,
																				 const VkDevice						device,
																				 const VkPhysicalDevice				physicalDevice,
																				 const VkPipeline					pipeline,
																				 Allocator&							allocator,
																				 de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																				 const deUint32						group)
{
	de::MovePtr<BufferWithMemory>	shaderBindingTable;

	if (group < m_shaderGroupCount)
	{
		const deUint32	shaderGroupHandleSize		= getShaderGroupHandleSize(vki, physicalDevice);
		const deUint32	shaderGroupBaseAlignment	= getShaderGroupBaseAlignment(vki, physicalDevice);

		shaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, group, 1u);
	}

	return shaderBindingTable;
}

void RayTracingConfiguration::initConfiguration (Context&		context,
												 TestParams&	testParams)
{
	DE_UNREF(testParams);

	const InstanceInterface&	vki						= context.getInstanceInterface();
	const DeviceInterface&		vkd						= context.getDeviceInterface();
	const VkDevice				device					= context.getDevice();
	const VkPhysicalDevice		physicalDevice			= context.getPhysicalDevice();
	vk::BinaryCollection&		collection				= context.getBinaryCollection();
	Allocator&					allocator				= context.getDefaultAllocator();
	const deUint32				shaderGroupHandleSize	= getShaderGroupHandleSize(vki, physicalDevice);
	const VkShaderStageFlags	hitStages				= VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	deUint32					shaderCount				= 0;

	m_shaderGroupCount = 0;

	if (collection.contains("rgen")) m_shaders |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	if (collection.contains("ahit")) m_shaders |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	if (collection.contains("chit")) m_shaders |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	if (collection.contains("miss")) m_shaders |= VK_SHADER_STAGE_MISS_BIT_KHR;
	if (collection.contains("sect")) m_shaders |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	if (collection.contains("call")) m_shaders |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	for (BinaryCollection::Iterator it = collection.begin(); it != collection.end(); ++it)
		shaderCount++;

	if (shaderCount != (deUint32)dePop32(m_shaders))
		TCU_THROW(InternalError, "Unused shaders detected in the collection");

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
		m_raygenShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
		m_missShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & hitStages))
		m_hitShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))
		m_callableShaderGroup	= m_shaderGroupCount++;

	m_rayTracingPipeline				= de::newMovePtr<RayTracingPipeline>();

	m_descriptorSetLayout				= DescriptorSetLayoutBuilder()
											.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
											.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
											.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
											.build(vkd, device);
	m_descriptorPool					= DescriptorPoolBuilder()
											.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
											.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
											.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
											.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	m_descriptorSet						= makeDescriptorSet(vkd, device, *m_descriptorPool, *m_descriptorSetLayout);

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR			, createShaderModule(vkd, device, collection.get("rgen"), 0), m_raygenShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR			, createShaderModule(vkd, device, collection.get("ahit"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))		m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR		, createShaderModule(vkd, device, collection.get("chit"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR			, createShaderModule(vkd, device, collection.get("miss"), 0), m_missShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR	, createShaderModule(vkd, device, collection.get("sect"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))		m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR		, createShaderModule(vkd, device, collection.get("call"), 0), m_callableShaderGroup);

	m_pipelineLayout					= makePipelineLayout(vkd, device, m_descriptorSetLayout.get());
	m_pipeline							= m_rayTracingPipeline->createPipeline(vkd, device, *m_pipelineLayout);

	m_raygenShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_raygenShaderGroup);
	m_missShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_missShaderGroup);
	m_hitShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_hitShaderGroup);
	m_callableShaderBindingTable		= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_callableShaderGroup);

	m_raygenShaderBindingTableRegion	= m_raygenShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, m_raygenShaderBindingTable->get(), 0), 0, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	m_missShaderBindingTableRegion		= m_missShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, m_missShaderBindingTable->get(), 0), 0, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	m_hitShaderBindingTableRegion		= m_hitShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, m_hitShaderBindingTable->get(), 0), 0, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	m_callableShaderBindingTableRegion	= m_callableShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, m_callableShaderBindingTable->get(), 0), 0, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
}

void RayTracingConfiguration::fillCommandBuffer (Context&							context,
												 TestParams&						testParams,
												 VkCommandBuffer					commandBuffer,
												 const VkAccelerationStructureKHR*	rayQueryTopAccelerationStructurePtr,
												 const VkDescriptorImageInfo&		resultImageInfo)
{
	const DeviceInterface&							vkd									= context.getDeviceInterface();
	const VkDevice									device								= context.getDevice();
	Allocator&										allocator							= context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
	de::MovePtr<TopLevelAccelerationStructure>		topLevelAccelerationStructure		= makeTopLevelAccelerationStructure();

	m_bottomLevelAccelerationStructure = de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release());
	m_bottomLevelAccelerationStructure->setDefaultGeometryData(testParams.stage);
	m_bottomLevelAccelerationStructure->createAndBuild(vkd, device, commandBuffer, allocator);

	m_topLevelAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(topLevelAccelerationStructure.release());
	m_topLevelAccelerationStructure->setInstanceCount(1);
	m_topLevelAccelerationStructure->addInstance(m_bottomLevelAccelerationStructure);
	m_topLevelAccelerationStructure->createAndBuild(vkd, device, commandBuffer, allocator);

	const TopLevelAccelerationStructure*				topLevelAccelerationStructurePtr				= m_topLevelAccelerationStructure.get();
	const VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet			=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		topLevelAccelerationStructurePtr->getPtr(),							//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};
	const VkWriteDescriptorSetAccelerationStructureKHR	rayQueryAccelerationStructureWriteDescriptorSet	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		rayQueryTopAccelerationStructurePtr,								//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &rayQueryAccelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipelineLayout, 0, 1, &m_descriptorSet.get(), 0, DE_NULL);

	vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline.get());

	cmdTraceRays(vkd,
		commandBuffer,
		&m_raygenShaderBindingTableRegion,
		&m_missShaderBindingTableRegion,
		&m_hitShaderBindingTableRegion,
		&m_callableShaderBindingTableRegion,
		testParams.width, testParams.height, 1);
}

bool TestConfiguration::verify (BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	tcu::TestLog&	log			= context.getTestContext().getLog();
	const deUint32	width		= testParams.width;
	const deUint32	height		= testParams.height;
	const deInt32*	resultPtr	= (deInt32*)resultBuffer->getAllocation().getHostPtr();
	const deInt32*	expectedPtr	= m_expected.data();
	deUint32		failures	= 0;
	deUint32		pos			= 0;

	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
	{
		if (resultPtr[pos] != expectedPtr[pos])
			failures++;

		pos++;
	}

	if (failures != 0)
	{
		const char * names[] = { "Retrieved:", "Expected:" };

		for (deUint32 n = 0; n < 2; ++n)
		{
			std::stringstream	css;

			pos = 0;

			for (deUint32 y = 0; y < height; ++y)
			{
				for (deUint32 x = 0; x < width; ++x)
				{
					if (resultPtr[pos] != expectedPtr[pos])
						css << std::setw(12) << (n == 0 ? resultPtr[pos] : expectedPtr[pos]) << ",";
					else
						css << "____________,";

					pos++;
				}

				css << std::endl;
			}

			log << tcu::TestLog::Message << names[n] << tcu::TestLog::EndMessage;
			log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
		}
	}

	return (failures == 0);
}

bool TestConfigurationFloat::verify (BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	tcu::TestLog&	log			= context.getTestContext().getLog();
	const float		eps			= float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
	const deUint32	width		= testParams.width;
	const deUint32	height		= testParams.height;
	const deInt32*	resultPtr	= (deInt32*)resultBuffer->getAllocation().getHostPtr();
	const deInt32*	expectedPtr	= m_expected.data();
	deUint32		failures	= 0;
	deUint32		pos			= 0;

	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
	{
		const float		retrievedValue		= float(resultPtr[pos]) / float(FIXED_POINT_DIVISOR);
		const float		expectedValue	= float(expectedPtr[pos]) / float(FIXED_POINT_DIVISOR);

		if (deFloatAbs(retrievedValue - expectedValue) > eps)
			failures++;

		pos++;
	}

	if (failures != 0)
	{
		const char * names[] = { "Retrieved:", "Expected:" };

		for (deUint32 n = 0; n < 2; ++n)
		{
			std::stringstream	css;

			pos = 0;

			for (deUint32 y = 0; y < height; ++y)
			{
				for (deUint32 x = 0; x < width; ++x)
				{
					const float	retrievedValue	= float(resultPtr[pos]) / float(FIXED_POINT_DIVISOR);
					const float	expectedValue	= float(expectedPtr[pos]) / float(FIXED_POINT_DIVISOR);

					if (deFloatAbs(retrievedValue - expectedValue) > eps)
						css << std::setprecision(8) << std::setw(12) << (n == 0 ? retrievedValue : expectedValue) << ",";
					else
						css << "____________,";

					pos++;
				}

				css << std::endl;
			}

			log << tcu::TestLog::Message << names[n] << tcu::TestLog::EndMessage;
			log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
		}
	}

	return (failures == 0);
}

bool TestConfigurationVector::verify (BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	tcu::TestLog&	log			= context.getTestContext().getLog();
	const float		eps			= float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
	const deUint32	width		= testParams.width;
	const deUint32	height		= testParams.height;
	const deUint32	depth		= 3u; // vec3
	const deInt32*	resultPtr	= (deInt32*)resultBuffer->getAllocation().getHostPtr();
	const deInt32*	expectedPtr	= m_expected.data();
	deUint32		failures	= 0;
	deUint32		pos			= 0;

	for (deUint32 z = 0; z < depth; ++z)
	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
	{
		const float	retrievedValue	= float(resultPtr[pos]) / float(FIXED_POINT_DIVISOR);
		const float	expectedValue	= float(expectedPtr[pos]) / float(FIXED_POINT_DIVISOR);

		if (deFloatAbs(retrievedValue - expectedValue) > eps)
			failures++;

		++pos;
	}

	if (failures != 0)
	{
		const char*			names[] = { "Retrieved", "Expected " };
		std::stringstream	css;

		for (deUint32 y = 0; y < height; ++y)
		{
			for (deUint32 x = 0; x < width; ++x)
			{
				for (deUint32 n = 0; n < 2; ++n)
				{
					css << names[n] << " at (" << x << "," << y << ") {";

					for (deUint32 z = 0; z < depth; ++z)
					{
						pos = x + width * (y + height * z);

						const float	retrievedValue	= float(resultPtr[pos]) / float(FIXED_POINT_DIVISOR);
						const float	expectedValue	= float(expectedPtr[pos]) / float(FIXED_POINT_DIVISOR);

						if (deFloatAbs(retrievedValue - expectedValue) > eps)
							css << std::setprecision(8) << std::setw(12) << (n == 0 ? retrievedValue : expectedValue) << ",";
						else
							css << "____________,";
					}

					css << "}" << std::endl;
				}
			}
		}

		log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
	}

	return failures == 0;
}

bool TestConfigurationMatrix::verify (BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	tcu::TestLog&	log			= context.getTestContext().getLog();
	const float		eps			= float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
	const deUint32	width		= testParams.width;
	const deUint32	height		= testParams.height;
	const deUint32	depth		= 12u; // mat3x4 or mat4x3
	const deInt32*	resultPtr	= (deInt32*)resultBuffer->getAllocation().getHostPtr();
	const deInt32*	expectedPtr	= m_expected.data();
	deUint32		failures	= 0;
	deUint32		pos			= 0;

	for (deUint32 z = 0; z < depth; ++z)
	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
	{
		const float	retrievedValue	= float(resultPtr[pos]) / float(FIXED_POINT_DIVISOR);
		const float	expectedValue	= float(expectedPtr[pos]) / float(FIXED_POINT_DIVISOR);

		if (deFloatAbs(retrievedValue - expectedValue) > eps)
			failures++;

		++pos;
	}

	if (failures != 0)
	{
		const char*			names[] = { "Retrieved", "Expected" };
		std::stringstream	css;

		for (deUint32 y = 0; y < height; ++y)
		{
			for (deUint32 x = 0; x < width; ++x)
			{
				css << "At (" << x << "," << y << ")" << std::endl;
				for (deUint32 n = 0; n < 2; ++n)
				{
					css << names[n] << std::endl << "{" << std::endl;

					for (deUint32 z = 0; z < depth; ++z)
					{
						pos = x + width * (y + height * z);

						const float	retrievedValue	= float(resultPtr[pos]) / float(FIXED_POINT_DIVISOR);
						const float	expectedValue	= float(expectedPtr[pos]) / float(FIXED_POINT_DIVISOR);

						if (z % 4 == 0)
							css << "    {";

						if (deFloatAbs(retrievedValue - expectedValue) > eps)
							css << std::setprecision(5) << std::setw(9) << (n == 0 ? retrievedValue : expectedValue) << ",";
						else
							css << "_________,";

						if (z % 4 == 3)
							css << "}" << std::endl;
					}

					css << "}" << std::endl;
				}
			}
		}

		log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
	}

	return failures == 0;
}

class TestConfigurationFlow : public TestConfiguration
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationFlow::initAccelerationStructures (Context&			context,
																					 TestParams&		testParams,
																					 VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const float									z										= -1.0f;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_expected = std::vector<deInt32>(width * height, 1);

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	for (size_t instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();

		for (size_t geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (size_t squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= n + 1;
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back());
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationFlow::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  uint        value    = 4;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value--;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value--;\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, 0.5f);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value--;\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  uint        value    = 4;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value--;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value--;\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value--;\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationPrimitiveId : public TestConfiguration
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationPrimitiveId::initAccelerationStructures (Context&		context,
																							TestParams&		testParams,
																							VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const float									z										= -1.0f;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == width * height);

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	m_expected.resize(width * height);

	for (deUint32 instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();

		for (deUint32 geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (deUint32 squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= (n + 11) % (width * height);
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				m_expected[n] = squareNdx;

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back(), identityMatrix3x4, instanceNdx + 1);
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationPrimitiveId::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  uint        value    = -1;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value--;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value--;\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, 0.5f);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  uint        value    = -1;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value--;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value--;\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationInstanceId : public TestConfiguration
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationInstanceId::initAccelerationStructures (Context&			context,
																						   TestParams&		testParams,
																						   VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const float									z										= -1.0f;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == width * height);

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	m_expected.resize(width * height);

	for (deUint32 instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();

		for (deUint32 geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (deUint32 squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= (n + 11) % (width * height);
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);

				m_expected[n] = instanceNdx;

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back(), identityMatrix3x4, instanceNdx + 1);
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationInstanceId::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  uint        value    = -1;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value--;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value--;\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, 0.5f);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  uint        value    = -1;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value--;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value--;\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationInstanceCustomIndex : public TestConfiguration
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationInstanceCustomIndex::initAccelerationStructures (Context&		context,
																									TestParams&		testParams,
																									VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const float									z										= -1.0f;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == width * height);

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	m_expected.resize(width * height);

	for (deUint32 instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();

		for (deUint32 geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (deUint32 squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= (n + 11) % (width * height);
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);

				m_expected[n] = instanceNdx + 1;

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back(), identityMatrix3x4, instanceNdx + 1);
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationInstanceCustomIndex::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  uint        value    = -1;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value--;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value--;\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, 0.5f);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  uint        value    = -1;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value--;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value--;\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationIntersectionT : public TestConfigurationFloat
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationIntersectionT::initAccelerationStructures (Context&			context,
																							  TestParams&		testParams,
																							  VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == width * height);

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	m_expected.resize(width * height);

	for (deUint32 instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();

		for (deUint32 geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (deUint32 squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= (n + 11) % (width * height);
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);
				const float		eps	= 1.0f / float(FIXED_POINT_DIVISOR);
				const float		z	= -deFloatAbs(eps + float(startPos.x()) * float(startPos.y()) / float (width * height));

				m_expected[n] = -int(z * FIXED_POINT_DIVISOR);

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));

				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back(), identityMatrix3x4);
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationIntersectionT::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  int         value    = -k;\n"
			"  const float t        = abs(float(pos.x * pos.y) / float (size.x * size.y));\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= k;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value -= k;\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, t);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value = int(k * rayQueryGetIntersectionTEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  int         value    = -k;\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= k;\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value -= k;\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value = int(k * rayQueryGetIntersectionTEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationObjectRayOrigin : public TestConfigurationVector
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationObjectRayOrigin::initAccelerationStructures (Context&			context,
																								TestParams&		testParams,
																								VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const deUint32								depth									= testParams.depth;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	const float									z										= -1.0f;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	deUint32									pos										= 0;
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == width * height);

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	for (deUint32 instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();

		for (deUint32 geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (deUint32 squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= (n + 11) % (width * height);
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));

				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back(), identityMatrix3x4);
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	m_expected.resize(width * height * depth);
	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
		m_expected[pos++] = int(float(FIXED_POINT_DIVISOR) * (0.5f + float(x)) / float(width));

	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
		m_expected[pos++] = int(float(FIXED_POINT_DIVISOR) * (0.5f + float(y)) / float(height));

	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
		m_expected[pos++] = 0;

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationObjectRayOrigin::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  ivec3       value    = ivec3(-k);\n"
			"  const float t        = abs(float(pos.x * pos.y) / float (size.x * size.y));\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= ivec3(k);\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value -= ivec3(k);\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, t);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value = ivec3(k * rayQueryGetIntersectionObjectRayOriginEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 0), ivec4(value.x, 0, 0, 0));\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 1), ivec4(value.y, 0, 0, 0));\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 2), ivec4(value.z, 0, 0, 0));\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  ivec3       value    = ivec3(-k);\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= ivec3(k);\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value -= ivec3(k);\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value = ivec3(k * rayQueryGetIntersectionObjectRayOriginEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 0), ivec4(value.x, 0, 0, 0));\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 1), ivec4(value.y, 0, 0, 0));\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 2), ivec4(value.z, 0, 0, 0));\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationObjectRayDirection : public TestConfigurationVector
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationObjectRayDirection::initAccelerationStructures (Context&			context,
																								   TestParams&		testParams,
																								   VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const deUint32								depth									= testParams.depth;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	const float									z										= -1.0f;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	deUint32									pos										= 0;
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == width * height);

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	for (deUint32 instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();

		for (deUint32 geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (deUint32 squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= (n + 11) % (width * height);
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));

				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back(), identityMatrix3x4);
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	m_expected.resize(width * height * depth);
	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
		m_expected[pos++] = 0;

	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
		m_expected[pos++] = 0;

	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
		m_expected[pos++] = -static_cast<deInt32>(FIXED_POINT_DIVISOR);

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationObjectRayDirection::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  ivec3       value    = ivec3(-k);\n"
			"  const float t        = abs(float(pos.x * pos.y) / float (size.x * size.y));\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= ivec3(k);\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value -= ivec3(k);\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, t);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value = ivec3(k * rayQueryGetIntersectionObjectRayDirectionEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 0), ivec4(value.x, 0, 0, 0));\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 1), ivec4(value.y, 0, 0, 0));\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 2), ivec4(value.z, 0, 0, 0));\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  ivec3       value    = ivec3(-k);\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= ivec3(k);\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value -= ivec3(k);\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value = ivec3(k * rayQueryGetIntersectionObjectRayDirectionEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 0), ivec4(value.x, 0, 0, 0));\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 1), ivec4(value.y, 0, 0, 0));\n"
			"  imageStore(result, ivec3(pos.x, pos.y, 2), ivec4(value.z, 0, 0, 0));\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationObjectToWorld : public TestConfigurationMatrix
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationObjectToWorld::initAccelerationStructures (Context&			context,
																							  TestParams&		testParams,
																							  VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	const float									z										= -1.0f;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == width * height);

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	for (deUint32 instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
		VkTransformMatrixKHR							transform									= identityMatrix3x4;

		transform.matrix[0][3] = (1.0f / 8.0f) / float(width);
		transform.matrix[1][3] = (1.0f / 16.0f) / float(height);

		for (deUint32 geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (deUint32 squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= (n + 11) % (width * height);
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back(), transform);
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	{
		const deUint32							imageDepth				= 4 * 4;
		const int								translateColumnNumber	= 3;
		const deUint32							colCount				= 4;
		const deUint32							rowCount				= 3;
		const deUint32							zStride					= height * width;
		const deUint32							expectedFloats			= imageDepth * zStride;
		const float								translateX				= (+1.0f / 8.0f) / float(width);
		const float								translateY				= (+1.0f / 16.0f) / float(height);
		tcu::Matrix<float, rowCount, colCount>	m;

		m[translateColumnNumber][0] = translateX;
		m[translateColumnNumber][1] = translateY;

		m_expected.resize(expectedFloats);

		for (deUint32 y = 0; y < height; ++y)
		{
			for (deUint32 x = 0; x < width; ++x)
			{
				const deUint32	elem0Pos	= x + width * y;

				for (deUint32 rowNdx = 0; rowNdx < rowCount; ++rowNdx)
				for (deUint32 colNdx = 0; colNdx < colCount; ++colNdx)
				{
					const deUint32	zNdx	= rowNdx * colCount + colNdx;
					const deUint32	posNdx	= elem0Pos + zStride * zNdx;

					m_expected[posNdx] = static_cast<deInt32>(FIXED_POINT_DIVISOR * m[colNdx][rowNdx]);
				}
			}
		}
	}

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationObjectToWorld::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  mat4x3      value    = mat4x3(-k);\n"
			"  const float t        = abs(float(pos.x * pos.y) / float (size.x * size.y));\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= mat4x3(k);\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value -= mat4x3(k);\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, t);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value = mat4x3(k * rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  int ndx = -1;\n"
			"  for (int row = 0; row < 3; row++)\n"
			"  for (int col = 0; col < 4; col++)\n"
			"  {\n"
			"    ndx++;\n"
			"    ivec3 p = ivec3(pos.xy, ndx);\n"
			"    float r = value[col][row];\n"
			"    ivec4 c = ivec4(int(r),0,0,1);\n"
			"    imageStore(result, p, c);\n"
			"  }\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  mat4x3      value    = mat4x3(-k);\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= mat4x3(k);\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value -= mat4x3(k);\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value = mat4x3(k * rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  int ndx = -1;\n"
			"  for (int row = 0; row < 3; row++)\n"
			"  for (int col = 0; col < 4; col++)\n"
			"  {\n"
			"    ndx++;\n"
			"    ivec3 p = ivec3(pos.xy, ndx);\n"
			"    float r = value[col][row];\n"
			"    ivec4 c = ivec4(int(r),0,0,1);\n"
			"    imageStore(result, p, c);\n"
			"  }\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationWorldToObject : public TestConfigurationMatrix
{
public:
	static const std::string					getShaderBodyText			(const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures	(Context&							context,
																			 TestParams&						testParams,
																			 VkCommandBuffer					cmdBuffer) override;
};

const VkAccelerationStructureKHR* TestConfigurationWorldToObject::initAccelerationStructures (Context&			context,
																							  TestParams&		testParams,
																							  VkCommandBuffer	cmdBuffer)
{
	const DeviceInterface&						vkd										= context.getDeviceInterface();
	const VkDevice								device									= context.getDevice();
	Allocator&									allocator								= context.getDefaultAllocator();
	const deUint32								width									= testParams.width;
	const deUint32								height									= testParams.height;
	const bool									triangles								= (testParams.geomType == GEOM_TYPE_TRIANGLES);
	const deUint32								instancesGroupCount						= testParams.instancesGroupCount;
	const deUint32								geometriesGroupCount					= testParams.geometriesGroupCount;
	const deUint32								squaresGroupCount						= testParams.squaresGroupCount;
	const float									z										= -1.0f;
	tcu::UVec2									startPos								= tcu::UVec2(0, 0);
	de::MovePtr<TopLevelAccelerationStructure>	rayQueryTopLevelAccelerationStructure	= makeTopLevelAccelerationStructure();

	DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == width * height);

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());

	m_topAccelerationStructure->setInstanceCount(instancesGroupCount);

	for (deUint32 instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
		VkTransformMatrixKHR							transform									= identityMatrix3x4;

		transform.matrix[0][3] = (1.0f / 8.0f) / float(width);
		transform.matrix[1][3] = (1.0f / 16.0f) / float(height);

		for (deUint32 geometryNdx = 0; geometryNdx < geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(squaresGroupCount * (triangles ? 3u : 2u));

			for (deUint32 squareNdx = 0; squareNdx < squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= width * startPos.y() + startPos.x();
				const deUint32	m	= (n + 11) % (width * height);
				const float		x0	= float(startPos.x() + 0) / float(width);
				const float		y0	= float(startPos.y() + 0) / float(height);
				const float		x1	= float(startPos.x() + 1) / float(width);
				const float		y1	= float(startPos.y() + 1) / float(height);

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / width;
				startPos.x() = m % width;
			}

			rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
		}

		rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));
		m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back(), transform);
	}

	m_topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

	{
		const deUint32							imageDepth				= 4 * 4;
		const int								translateColumnNumber	= 3;
		const deUint32							colCount				= 4;
		const deUint32							rowCount				= 3;
		const deUint32							zStride					= height * width;
		const deUint32							expectedFloats			= imageDepth * zStride;
		const float								translateX				= (-1.0f / 8.0f) / float(width);
		const float								translateY				= (-1.0f / 16.0f) / float(height);
		tcu::Matrix<float, rowCount, colCount>	m;

		m[translateColumnNumber][0] = translateX;
		m[translateColumnNumber][1] = translateY;

		m_expected.resize(expectedFloats);

		for (deUint32 y = 0; y < height; ++y)
		{
			for (deUint32 x = 0; x < width; ++x)
			{
				const deUint32	elem0Pos	= x + width * y;

				for (deUint32 rowNdx = 0; rowNdx < rowCount; ++rowNdx)
				for (deUint32 colNdx = 0; colNdx < colCount; ++colNdx)
				{
					const deUint32	zNdx	= rowNdx * colCount + colNdx;
					const deUint32	posNdx	= elem0Pos + zStride * zNdx;

					m_expected[posNdx] = static_cast<deInt32>(FIXED_POINT_DIVISOR * m[colNdx][rowNdx]);
				}
			}
		}
	}

	return m_topAccelerationStructure.get()->getPtr();
}

const std::string TestConfigurationWorldToObject::getShaderBodyText (const TestParams& testParams)
{
	if (testParams.geomType == GEOM_TYPE_AABBS)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = 0;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  mat4x3      value    = mat4x3(-k);\n"
			"  const float t        = abs(float(pos.x * pos.y) / float (size.x * size.y));\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= mat4x3(k);\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)\n"
			"    {\n"
			"      value -= mat4x3(k);\n"
			"      rayQueryGenerateIntersectionEXT(rayQuery, t);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionGeneratedEXT)\n"
			"        value = mat4x3(k * rayQueryGetIntersectionWorldToObjectEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  int ndx = -1;\n"
			"  for (int row = 0; row < 3; row++)\n"
			"  for (int col = 0; col < 4; col++)\n"
			"  {\n"
			"    ndx++;\n"
			"    ivec3 p = ivec3(pos.xy, ndx);\n"
			"    float r = value[col][row];\n"
			"    ivec4 c = ivec4(int(r),0,0,1);\n"
			"    imageStore(result, p, c);\n"
			"  }\n";

		return result;
	}
	else if (testParams.geomType == GEOM_TYPE_TRIANGLES)
	{
		const std::string result =
			"  const int   k        = " +de::toString(FIXED_POINT_DIVISOR) + ";\n"
			"  uint        rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
			"  uint        cullMask = 0xFF;\n"
			"  float       tmin     = 0.0;\n"
			"  float       tmax     = 9.0;\n"
			"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
			"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
			"  mat4x3      value    = mat4x3(-k);\n"
			"  rayQueryEXT rayQuery;\n"
			"\n"
			"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
			"\n"
			"  if (rayQueryProceedEXT(rayQuery))\n"
			"  {\n"
			"    value -= mat4x3(k);\n"
			"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
			"    {\n"
			"      value -= mat4x3(k);\n"
			"      rayQueryConfirmIntersectionEXT(rayQuery);\n"
			"\n"
			"      rayQueryProceedEXT(rayQuery);\n"
			"\n"
			"      if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
			"        value = mat4x3(k * rayQueryGetIntersectionWorldToObjectEXT(rayQuery, true));\n"
			"    }\n"
			"  }\n"
			"\n"
			"  int ndx = -1;\n"
			"  for (int row = 0; row < 3; row++)\n"
			"  for (int col = 0; col < 4; col++)\n"
			"  {\n"
			"    ndx++;\n"
			"    ivec3 p = ivec3(pos.xy, ndx);\n"
			"    float r = value[col][row];\n"
			"    ivec4 c = ivec4(int(r),0,0,1);\n"
			"    imageStore(result, p, c);\n"
			"  }\n";

		return result;
	}
	else
	{
		TCU_THROW(InternalError, "Unknown geometry type");
	}
}

class TestConfigurationNullASStruct : public TestConfiguration
{
public:
												TestConfigurationNullASStruct		();
												~TestConfigurationNullASStruct		();

	static const std::string					getShaderBodyText					(const TestParams&					testParams);
	static void									checkSupport						(Context&							context,
																					 const TestParams&					testParams);

	virtual const VkAccelerationStructureKHR*	initAccelerationStructures			(Context&							context,
																					 TestParams&						testParams,
																					 VkCommandBuffer					cmdBuffer) override;
protected:
	Move<VkAccelerationStructureKHR>			m_emptyAccelerationStructure;
};

TestConfigurationNullASStruct::TestConfigurationNullASStruct ()
	: TestConfiguration				()
	, m_emptyAccelerationStructure	()
{
}

TestConfigurationNullASStruct::~TestConfigurationNullASStruct ()
{
}

const VkAccelerationStructureKHR* TestConfigurationNullASStruct::initAccelerationStructures (Context&			context,
																							 TestParams&		testParams,
																							 VkCommandBuffer	cmdBuffer)
{
	DE_UNREF(context);
	DE_UNREF(cmdBuffer);

	m_expected = std::vector<deInt32>(testParams.width * testParams.height, 1);

	return &m_emptyAccelerationStructure.get();
}

void TestConfigurationNullASStruct::checkSupport (Context&			context,
												 const TestParams&	testParams)
{
	DE_UNREF(testParams);

	context.requireDeviceFunctionality("VK_EXT_robustness2");

	const vk::VkPhysicalDeviceRobustness2FeaturesEXT&	robustness2FeaturesEXT	= context.getRobustness2FeaturesEXT();

	if (!robustness2FeaturesEXT.nullDescriptor)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRobustness2FeaturesEXT::nullDescriptor");
}

const std::string TestConfigurationNullASStruct::getShaderBodyText (const TestParams& testParams)
{
	DE_UNREF(testParams);

	const std::string result =
		"  uint        rayFlags = 0;\n"
		"  uint        cullMask = 0xFF;\n"
		"  float       tmin     = 0.0;\n"
		"  float       tmax     = 9.0;\n"
		"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
		"  vec3        direct   = vec3(0.0, 0.0, -1.0);\n"
		"  uint        value    = 1;\n"
		"  rayQueryEXT rayQuery;\n"
		"\n"
		"  rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
		"\n"
		"  if (rayQueryProceedEXT(rayQuery))\n"
		"  {\n"
		"    value++;\n"
		"\n"
		"    rayQueryTerminateEXT(rayQuery);\n"
		"  }\n"
		"\n"
		"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

	return result;
}

class RayQueryBuiltinTestInstance : public TestInstance
{
public:
										RayQueryBuiltinTestInstance		(Context& context, const TestParams& data);
	virtual								~RayQueryBuiltinTestInstance	(void);
	tcu::TestStatus						iterate							(void);

private:
	TestParams							m_data;
	de::MovePtr<TestConfiguration>		m_testConfig;
	de::MovePtr<PipelineConfiguration>	m_pipelineConfig;
};

RayQueryBuiltinTestInstance::RayQueryBuiltinTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
	switch (m_data.testType)
	{
		case TEST_TYPE_FLOW:						m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationFlow());					break;
		case TEST_TYPE_PRIMITIVE_ID:				m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationPrimitiveId());			break;
		case TEST_TYPE_INSTANCE_ID:					m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationInstanceId());			break;
		case TEST_TYPE_INSTANCE_CUSTOM_INDEX:		m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationInstanceCustomIndex());	break;
		case TEST_TYPE_INTERSECTION_T_KHR:			m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationIntersectionT());		break;
		case TEST_TYPE_OBJECT_RAY_ORIGIN_KHR:		m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationObjectRayOrigin());		break;
		case TEST_TYPE_OBJECT_RAY_DIRECTION_KHR:	m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationObjectRayDirection());	break;
		case TEST_TYPE_OBJECT_TO_WORLD_KHR:			m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationObjectToWorld());		break;
		case TEST_TYPE_WORLD_TO_OBJECT_KHR:			m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationWorldToObject());		break;
		case TEST_TYPE_NULL_ACCELERATION_STRUCTURE:	m_testConfig = de::MovePtr<TestConfiguration>(new TestConfigurationNullASStruct());			break;
		default: TCU_THROW(InternalError, "Unknown test type");
	}

	switch (m_data.stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		case VK_SHADER_STAGE_FRAGMENT_BIT:
		{
			m_pipelineConfig = de::MovePtr<PipelineConfiguration>(new GraphicsConfiguration());
			break;
		}

		case VK_SHADER_STAGE_COMPUTE_BIT:
		{
			m_pipelineConfig = de::MovePtr<PipelineConfiguration>(new ComputeConfiguration());
			break;
		}

		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		case VK_SHADER_STAGE_MISS_BIT_KHR:
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
		{
			m_pipelineConfig = de::MovePtr<PipelineConfiguration>(new RayTracingConfiguration());
			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown shader stage");
	}
}

RayQueryBuiltinTestInstance::~RayQueryBuiltinTestInstance (void)
{
}

tcu::TestStatus RayQueryBuiltinTestInstance::iterate (void)
{
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();

	const deUint32						width								= m_data.width;
	const deUint32						height								= m_data.height;
	const deUint32						depth								= m_data.depth;
	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.format, width, height, depth);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, m_data.format, imageSubresourceRange);

	const deUint32						pixelSize							= mapVkFormat(m_data.format).getPixelSize();
	const VkBufferCreateInfo			resultBufferCreateInfo				= makeBufferCreateInfo(width * height * depth * pixelSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				resultBufferImageRegion				= makeBufferImageCopy(makeExtent3D(width, height, depth), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		resultBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			resultImageInfo						= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const VkAccelerationStructureKHR*	topAccelerationStructurePtr			= DE_NULL;

	m_pipelineConfig->initConfiguration(m_context, m_data);

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		const VkImageMemoryBarrier	preImageBarrier			= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **image, imageSubresourceRange);
		const VkClearValue			clearValue				= makeClearValueColorU32(0u, 0u, 0u, 0u);
		const VkImageMemoryBarrier	postImageBarrier		= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, **image, imageSubresourceRange);
		const VkMemoryBarrier		postTestMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		topAccelerationStructurePtr = m_testConfig->initAccelerationStructures(m_context, m_data, *cmdBuffer);

		m_pipelineConfig->fillCommandBuffer(m_context, m_data, *cmdBuffer, topAccelerationStructurePtr, resultImageInfo);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTestMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	if (m_testConfig->verify(resultBuffer.get(), m_context, m_data))
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

class RayQueryBuiltinTestCase : public TestCase
{
	public:
							RayQueryBuiltinTestCase		(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
							~RayQueryBuiltinTestCase	(void);

	virtual void			checkSupport				(Context& context) const;
	virtual	void			initPrograms				(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;

private:
	TestParams				m_data;
};

RayQueryBuiltinTestCase::RayQueryBuiltinTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayQueryBuiltinTestCase::~RayQueryBuiltinTestCase (void)
{
}

void RayQueryBuiltinTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_query");

	const VkPhysicalDeviceRayQueryFeaturesKHR&				rayQueryFeaturesKHR					= context.getRayQueryFeatures();
	if (rayQueryFeaturesKHR.rayQuery == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_query requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	m_data.pipelineCheckSupport(context, m_data);

	if (m_data.testConfigCheckSupport != DE_NULL)
		m_data.testConfigCheckSupport(context, m_data);
}

TestInstance* RayQueryBuiltinTestCase::createInstance (Context& context) const
{
	return new RayQueryBuiltinTestInstance(context, m_data);
}

void RayQueryBuiltinTestCase::initPrograms (SourceCollections& programCollection) const
{
	m_data.pipelineInitPrograms(programCollection, m_data);
}

static inline CheckSupportFunc getPipelineCheckSupport (const VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return GraphicsConfiguration::checkSupport;

		case VK_SHADER_STAGE_COMPUTE_BIT:
			return ComputeConfiguration::checkSupport;

		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		case VK_SHADER_STAGE_MISS_BIT_KHR:
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			return RayTracingConfiguration::checkSupport;

		default:
			TCU_THROW(InternalError, "Unknown shader stage");
	}
}

static inline InitProgramsFunc getPipelineInitPrograms (const VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return GraphicsConfiguration::initPrograms;

		case VK_SHADER_STAGE_COMPUTE_BIT:
			return ComputeConfiguration::initPrograms;

		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		case VK_SHADER_STAGE_MISS_BIT_KHR:
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			return RayTracingConfiguration::initPrograms;

		default:
			TCU_THROW(InternalError, "Unknown shader stage");
	}
}

static inline ShaderBodyTextFunc getShaderBodyTextFunc (const TestType testType)
{
	switch (testType)
	{
		case TEST_TYPE_FLOW:						return TestConfigurationFlow::getShaderBodyText;				break;
		case TEST_TYPE_PRIMITIVE_ID:				return TestConfigurationPrimitiveId::getShaderBodyText;			break;
		case TEST_TYPE_INSTANCE_ID:					return TestConfigurationInstanceId::getShaderBodyText;			break;
		case TEST_TYPE_INSTANCE_CUSTOM_INDEX:		return TestConfigurationInstanceCustomIndex::getShaderBodyText;	break;
		case TEST_TYPE_INTERSECTION_T_KHR:			return TestConfigurationIntersectionT::getShaderBodyText;		break;
		case TEST_TYPE_OBJECT_RAY_ORIGIN_KHR:		return TestConfigurationObjectRayOrigin::getShaderBodyText;		break;
		case TEST_TYPE_OBJECT_RAY_DIRECTION_KHR:	return TestConfigurationObjectRayDirection::getShaderBodyText;	break;
		case TEST_TYPE_OBJECT_TO_WORLD_KHR:			return TestConfigurationObjectToWorld::getShaderBodyText;		break;
		case TEST_TYPE_WORLD_TO_OBJECT_KHR:			return TestConfigurationWorldToObject::getShaderBodyText;		break;
		case TEST_TYPE_NULL_ACCELERATION_STRUCTURE:	return TestConfigurationNullASStruct::getShaderBodyText;		break;
		default:									TCU_THROW(InternalError, "Unknown test type");
	}
}

static inline CheckSupportFunc getTestConfigCheckSupport (const TestType testType)
{
	if (testType >= TEST_TYPE_LAST)
		TCU_THROW(InternalError, "Unknown test type");

	switch (testType)
	{
		case TEST_TYPE_NULL_ACCELERATION_STRUCTURE:	return TestConfigurationNullASStruct::checkSupport;	break;
		default:									return DE_NULL;
	}
}

}	// anonymous

const struct PipelineStages
{
	VkShaderStageFlagBits	stage;
	const char*				name;
}
pipelineStages[] =
{
	{ VK_SHADER_STAGE_VERTEX_BIT,					"vert"	},
	{ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		"tesc"	},
	{ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	"tese"	},
	{ VK_SHADER_STAGE_GEOMETRY_BIT,					"geom"	},
	{ VK_SHADER_STAGE_FRAGMENT_BIT,					"frag"	},
	{ VK_SHADER_STAGE_COMPUTE_BIT,					"comp"	},
	{ VK_SHADER_STAGE_RAYGEN_BIT_KHR,				"rgen"	},
	{ VK_SHADER_STAGE_ANY_HIT_BIT_KHR,				"ahit"	},
	{ VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,			"chit"	},
	{ VK_SHADER_STAGE_MISS_BIT_KHR,					"miss"	},
	{ VK_SHADER_STAGE_INTERSECTION_BIT_KHR,			"sect"	},
	{ VK_SHADER_STAGE_CALLABLE_BIT_KHR,				"call"	},
};

const struct GeomTypes
{
	GeomType	geomType;
	const char*	name;
}
geomTypes[] =
{
	{ GEOM_TYPE_TRIANGLES,							"triangles" },
	{ GEOM_TYPE_AABBS,								"aabbs" },
};

tcu::TestCaseGroup*	createBuiltinTests	(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		group	(new tcu::TestCaseGroup(testCtx, "builtin", "Tests verifying builtins provided by ray query"));

	const struct TestTypes
	{
		TestType	testType;
		const char*	name;
	}
	testTypes[] =
	{
		{ TEST_TYPE_FLOW,								"flow"					},
		{ TEST_TYPE_PRIMITIVE_ID,						"primitiveid"			},
		{ TEST_TYPE_INSTANCE_ID,						"instanceid"			},
		{ TEST_TYPE_INSTANCE_CUSTOM_INDEX,				"instancecustomindex"	},
		{ TEST_TYPE_INTERSECTION_T_KHR,					"intersectiont"			},
		{ TEST_TYPE_OBJECT_RAY_ORIGIN_KHR,				"objectrayorigin"		},
		{ TEST_TYPE_OBJECT_RAY_DIRECTION_KHR,			"objectraydirection"	},
		{ TEST_TYPE_OBJECT_TO_WORLD_KHR,				"objecttoworld"			},
		{ TEST_TYPE_WORLD_TO_OBJECT_KHR,				"worldtoobject"			},
	};

	for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup>	testTypeGroup					(new tcu::TestCaseGroup(group->getTestContext(), testTypes[testTypeNdx].name, ""));
		const TestType					testType						= testTypes[testTypeNdx].testType;
		const ShaderBodyTextFunc		testConfigShaderBodyTextFunc	= getShaderBodyTextFunc(testType);
		const bool						fixedPointVectorOutput			=  testType == TEST_TYPE_OBJECT_RAY_ORIGIN_KHR
																		|| testType == TEST_TYPE_OBJECT_RAY_DIRECTION_KHR;
		const bool						fixedPointMatrixOutput			=  testType == TEST_TYPE_OBJECT_TO_WORLD_KHR
																		|| testType == TEST_TYPE_WORLD_TO_OBJECT_KHR;
		const bool						single							=  testTypeNdx == TEST_TYPE_FLOW
																		|| testType == TEST_TYPE_OBJECT_RAY_ORIGIN_KHR
																		|| testType == TEST_TYPE_OBJECT_RAY_DIRECTION_KHR
																		|| testType == TEST_TYPE_OBJECT_TO_WORLD_KHR
																		|| testType == TEST_TYPE_WORLD_TO_OBJECT_KHR;
		const deUint32					imageDepth						= fixedPointMatrixOutput ? 4 * 4
																		: fixedPointVectorOutput ? 4
																		: 1;

		for (size_t pipelineStageNdx = 0; pipelineStageNdx < DE_LENGTH_OF_ARRAY(pipelineStages); ++pipelineStageNdx)
		{
			de::MovePtr<tcu::TestCaseGroup>	sourceTypeGroup			(new tcu::TestCaseGroup(group->getTestContext(), pipelineStages[pipelineStageNdx].name, ""));
			const VkShaderStageFlagBits		stage					= pipelineStages[pipelineStageNdx].stage;
			const CheckSupportFunc			pipelineCheckSupport	= getPipelineCheckSupport(stage);
			const InitProgramsFunc			pipelineInitPrograms	= getPipelineInitPrograms(stage);
			const deUint32					instancesGroupCount		= single ? 1 : 2;
			const deUint32					geometriesGroupCount	= single ? 1 : 8;
			const deUint32					squaresGroupCount		= (TEST_WIDTH * TEST_HEIGHT) / geometriesGroupCount / instancesGroupCount;

			DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == TEST_WIDTH * TEST_HEIGHT);

			for (size_t geomTypeNdx = 0; geomTypeNdx < DE_LENGTH_OF_ARRAY(geomTypes); ++geomTypeNdx)
			{
				const GeomType		geomType	= geomTypes[geomTypeNdx].geomType;
				const TestParams	testParams	=
				{
					TEST_WIDTH,						//  deUint32				width;
					TEST_HEIGHT,					//  deUint32				height;
					imageDepth,						//  deUint32				depth;
					testType,						//  TestType				testType;
					stage,							//  VkShaderStageFlagBits	stage;
					geomType,						//  GeomType				geomType;
					squaresGroupCount,				//  deUint32				squaresGroupCount;
					geometriesGroupCount,			//  deUint32				geometriesGroupCount;
					instancesGroupCount,			//  deUint32				instancesGroupCount;
					VK_FORMAT_R32_SINT,				//  VkFormat				format;
					pipelineCheckSupport,			//  CheckSupportFunc		pipelineCheckSupport;
					pipelineInitPrograms,			//  InitProgramsFunc		pipelineInitPrograms;
					testConfigShaderBodyTextFunc,	//  ShaderTestTextFunc		testConfigShaderBodyText;
					DE_NULL,						//  CheckSupportFunc		testConfigCheckSupport;
				};

				sourceTypeGroup->addChild(new RayQueryBuiltinTestCase(group->getTestContext(), geomTypes[geomTypeNdx].name, "", testParams));
			}

			testTypeGroup->addChild(sourceTypeGroup.release());
		}

		group->addChild(testTypeGroup.release());
	}

	return group.release();
}

tcu::TestCaseGroup*	createAdvancedTests	(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		group	(new tcu::TestCaseGroup(testCtx, "advanced", "Advanced ray query tests"));

	const struct TestTypes
	{
		TestType	testType;
		const char*	name;
	}
	testTypes[] =
	{
		{ TEST_TYPE_NULL_ACCELERATION_STRUCTURE,	"null_as"	},
	};

	for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup>	testTypeGroup					(new tcu::TestCaseGroup(group->getTestContext(), testTypes[testTypeNdx].name, ""));
		const TestType					testType						= testTypes[testTypeNdx].testType;
		const ShaderBodyTextFunc		testConfigShaderBodyTextFunc	= getShaderBodyTextFunc(testType);
		const CheckSupportFunc			testConfigCheckSupport			= getTestConfigCheckSupport(testType);
		const deUint32					imageDepth						= 1;

		for (size_t pipelineStageNdx = 0; pipelineStageNdx < DE_LENGTH_OF_ARRAY(pipelineStages); ++pipelineStageNdx)
		{
			de::MovePtr<tcu::TestCaseGroup>	sourceTypeGroup			(new tcu::TestCaseGroup(group->getTestContext(), pipelineStages[pipelineStageNdx].name, ""));
			const VkShaderStageFlagBits		stage					= pipelineStages[pipelineStageNdx].stage;
			const CheckSupportFunc			pipelineCheckSupport	= getPipelineCheckSupport(stage);
			const InitProgramsFunc			pipelineInitPrograms	= getPipelineInitPrograms(stage);
			const deUint32					instancesGroupCount		= 1;
			const deUint32					geometriesGroupCount	= 1;
			const deUint32					squaresGroupCount		= (TEST_WIDTH * TEST_HEIGHT) / geometriesGroupCount / instancesGroupCount;

			DE_ASSERT(instancesGroupCount * geometriesGroupCount * squaresGroupCount == TEST_WIDTH * TEST_HEIGHT);

			for (size_t geomTypeNdx = 0; geomTypeNdx < DE_LENGTH_OF_ARRAY(geomTypes); ++geomTypeNdx)
			{
				const GeomType		geomType	= geomTypes[geomTypeNdx].geomType;
				const TestParams	testParams	=
				{
					TEST_WIDTH,						//  deUint32				width;
					TEST_HEIGHT,					//  deUint32				height;
					imageDepth,						//  deUint32				depth;
					testType,						//  TestType				testType;
					stage,							//  VkShaderStageFlagBits	stage;
					geomType,						//  GeomType				geomType;
					squaresGroupCount,				//  deUint32				squaresGroupCount;
					geometriesGroupCount,			//  deUint32				geometriesGroupCount;
					instancesGroupCount,			//  deUint32				instancesGroupCount;
					VK_FORMAT_R32_SINT,				//  VkFormat				format;
					pipelineCheckSupport,			//  CheckSupportFunc		pipelineCheckSupport;
					pipelineInitPrograms,			//  InitProgramsFunc		pipelineInitPrograms;
					testConfigShaderBodyTextFunc,	//  ShaderTestTextFunc		testConfigShaderBodyText;
					testConfigCheckSupport,			//  CheckSupportFunc		testConfigCheckSupport;
				};

				sourceTypeGroup->addChild(new RayQueryBuiltinTestCase(group->getTestContext(), geomTypes[geomTypeNdx].name, "", testParams));
			}

			testTypeGroup->addChild(sourceTypeGroup.release());
		}

		group->addChild(testTypeGroup.release());
	}

	return group.release();
}

}	// RayQuery
}	// vkt
