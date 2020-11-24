/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Ray Tracing Watertightness tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingWatertightnessTests.hpp"

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "deRandom.hpp"

namespace vkt
{
	namespace RayTracing
	{
		namespace
		{
			using namespace vk;
			using namespace std;

			static const VkFlags	ALL_RAY_TRACING_STAGES = VK_SHADER_STAGE_RAYGEN_BIT_KHR
				| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
				| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
				| VK_SHADER_STAGE_MISS_BIT_KHR
				| VK_SHADER_STAGE_INTERSECTION_BIT_KHR
				| VK_SHADER_STAGE_CALLABLE_BIT_KHR;

			struct CaseDef
			{
				deUint32	width;
				deUint32	height;
				deUint32	squaresGroupCount;
				deUint32	geometriesGroupCount;
				deUint32	instancesGroupCount;
				deUint32	randomSeed;
				deUint32	depth;
				deUint32    useManyBottomASes;
			};

			enum ShaderGroups
			{
				FIRST_GROUP = 0,
				RAYGEN_GROUP = FIRST_GROUP,
				MISS_GROUP,
				HIT_GROUP,
				GROUP_COUNT
			};

			static inline tcu::Vec3 mixVec3(const tcu::Vec3& a, const tcu::Vec3& b, const float alpha)
			{
				const tcu::Vec3 result = a * alpha + b * (1.0f - alpha);

				return result;
			}

			deUint32 getShaderGroupSize(const InstanceInterface& vki,
				const VkPhysicalDevice		physicalDevice)
			{
				de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

				rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

				return rayTracingPropertiesKHR->getShaderGroupHandleSize();
			}

			deUint32 getShaderGroupBaseAlignment(const InstanceInterface& vki,
				const VkPhysicalDevice	physicalDevice)
			{
				de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

				rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

				return rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
			}

			Move<VkPipeline> makePipeline(const DeviceInterface& vkd,
				const VkDevice					device,
				vk::BinaryCollection& collection,
				de::MovePtr<RayTracingPipeline>& rayTracingPipeline,
				VkPipelineLayout					pipelineLayout,
				const deUint32					raygenGroup,
				const deUint32					missGroup,
				const deUint32					hitGroup)
			{
				Move<VkShaderModule>	raygenShader = createShaderModule(vkd, device, collection.get("rgen"), 0);
				Move<VkShaderModule>	hitShader = createShaderModule(vkd, device, collection.get("ahit"), 0);
				Move<VkShaderModule>	missShader = createShaderModule(vkd, device, collection.get("miss"), 0);

				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenShader, raygenGroup);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, hitShader, hitGroup);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missShader, missGroup);

				Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

				return pipeline;
			}

			VkImageCreateInfo makeImageCreateInfo(deUint32 width, deUint32 height, deUint32 depth, VkFormat format)
			{
				const VkImageUsageFlags	usage		= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				const auto				imageType	= (depth == 1)	? VK_IMAGE_TYPE_2D
																	: VK_IMAGE_TYPE_3D;

				const VkImageCreateInfo	imageCreateInfo =
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
					DE_NULL,								// const void*				pNext;
					(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
					imageType,
					format,									// VkFormat					format;
					makeExtent3D(width, height, depth),		// VkExtent3D				extent;
					1u,										// deUint32					mipLevels;
					1u,										// deUint32					arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
					VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
					usage,									// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
					0u,										// deUint32					queueFamilyIndexCount;
					DE_NULL,								// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
				};

				return imageCreateInfo;
			}

			class RayTracingWatertightnessTestInstance : public TestInstance
			{
			public:
				RayTracingWatertightnessTestInstance(Context& context, const CaseDef& data, const bool& useClosedFan);
				~RayTracingWatertightnessTestInstance(void);
				tcu::TestStatus												iterate(void);

			protected:
				void														checkSupportInInstance(void) const;
				de::MovePtr<BufferWithMemory>								runTest(void);
				de::MovePtr<TopLevelAccelerationStructure>					initTopAccelerationStructure(VkCommandBuffer											cmdBuffer,
					vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures);
				vector<de::SharedPtr<BottomLevelAccelerationStructure>	>	initBottomAccelerationStructures(VkCommandBuffer	cmdBuffer);
				de::MovePtr<BottomLevelAccelerationStructure>				initBottomAccelerationStructure(VkCommandBuffer	cmdBuffer,
					bool				triangles);

			private:
				CaseDef														m_data;
				const bool													m_useClosedFan;
			};

			RayTracingWatertightnessTestInstance::RayTracingWatertightnessTestInstance(Context& context, const CaseDef& data, const bool& useClosedFan)
				: vkt::TestInstance(context)
				, m_data(data)
				, m_useClosedFan(useClosedFan)
			{
			}

			RayTracingWatertightnessTestInstance::~RayTracingWatertightnessTestInstance(void)
			{
			}

			class RayTracingTestCase : public TestCase
			{
			public:
				RayTracingTestCase(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data, const bool& useClosedFan);
				~RayTracingTestCase(void);

				virtual	void			initPrograms(SourceCollections& programCollection) const;
				virtual TestInstance* createInstance(Context& context) const;
				virtual void			checkSupport(Context& context) const;

			private:
				CaseDef					m_data;
				const bool				m_useClosedFan;
			};

			RayTracingTestCase::RayTracingTestCase(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data, const bool& useClosedFan)
				: vkt::TestCase(context, name, desc)
				, m_data(data)
				, m_useClosedFan(useClosedFan)
			{
			}

			RayTracingTestCase::~RayTracingTestCase(void)
			{
			}

			void RayTracingTestCase::checkSupport(Context& context) const
			{
				context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
				context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

				const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();
				if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
					TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

				const VkPhysicalDeviceAccelerationStructureFeaturesKHR& accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
				if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
					TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
			}

			void RayTracingTestCase::initPrograms(SourceCollections& programCollection) const
			{
				const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
				{
					std::stringstream css;

					if (!m_useClosedFan)
					{
						css <<
							"#version 460 core\n"
							"#extension GL_EXT_ray_tracing : require\n"
							"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
							"hitAttributeEXT vec3 attribs;\n"
							"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
							"void main()\n"
							"{\n"
							"  uvec4 color = uvec4(1,0,0,1);\n"
							"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
							"}\n";
					}
					else
					{
						css << "#version 460 core\n"
							"\n"
							"#extension GL_EXT_ray_tracing : require\n"
							"\n"
							"layout(location = 0)                        rayPayloadInEXT vec3     hitValue;\n"
							"layout(r32ui, set = 0, binding = 0) uniform                 uimage3D result;\n"
							"\n"
							"hitAttributeEXT vec3 attribs;\n"
							"\n"
							"void main()\n"
							"{\n"
							"    imageAtomicAdd(result, ivec3(gl_LaunchIDEXT.xy, gl_PrimitiveID), 1);\n"
							"}\n";
					}

					programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				{
					std::stringstream css;

					if (!m_useClosedFan)
					{
						css <<
							"#version 460 core\n"
							"#extension GL_EXT_ray_tracing : require\n"
							"layout(location = 0) rayPayloadInEXT dummyPayload { vec4 dummy; };\n"
							"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
							"void main()\n"
							"{\n"
							"  uvec4 color = uvec4(2,0,0,1);\n"
							"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
							"}\n";
					}
					else
					{
						css << "#version 460 core\n"
							"\n"
							"#extension GL_EXT_ray_tracing : require\n"
							"\n"
							"layout(location = 0)                        rayPayloadInEXT dummyPayload { vec4 dummy; };\n"
							"layout(r32ui, set = 0, binding = 0) uniform uimage3D        result;\n"
							"\n"
							"void main()\n"
							"{\n"
							"    imageAtomicAdd(result, ivec3(gl_LaunchIDEXT.xy, 0), 10000);\n"
							"}\n";
					}

					programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				if (!m_useClosedFan)
				{
					programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
				}
				else
				{
					std::stringstream	css;
					const auto& nSharedEdges = m_data.squaresGroupCount;

					// NOTE: Zeroth invocation fires at the center of the closed fan. Subsequent invocations trace rays against center of shared edges.
					css << "#version 460 core\n"
						"\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"\n"
						"layout(location = 0)         rayPayloadEXT vec3                     hitValue;\n"
						"layout(set = 0, binding = 1) uniform       accelerationStructureEXT topLevelAS;\n"
						"\n"
						"void main()\n"
						"{\n"
						"    uint  rayFlags = 0;\n"
						"    uint  cullMask = 0xFF;\n"
						"    float tmin     = 0.01;\n"
						"    float tmax     = 9.0;\n"
						"    uint  nRay     = gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
						"    vec3  origin   = vec3(0.0, 0.0, -1.0);\n"
						"\n"
						"    if (nRay > "<< de::toString(nSharedEdges + 1) << ")\n"
						"    {\n"
						"        return;\n"
						"    }\n"
						"\n"
						"    float angleDiff    = 2.0 * 3.14159265 / " << de::toString(nSharedEdges) << ";\n"
						"    vec2  sharedEdgeP1 = vec2(0, 0);\n"
						"    vec2  sharedEdgeP2 = (nRay == 0) ? vec2     (0, 0)\n"
						"                                     : vec2     (sin(angleDiff * (nRay - 1) ), cos(angleDiff * (nRay - 1) ));\n"
						"    vec3  target       = vec3     (mix(sharedEdgeP1, sharedEdgeP2, vec2(0.5) ), 0.0);\n"
						"    vec3  direct       = normalize(target - origin);\n"
						"\n"
						"    traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
						"}\n";

					programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}
			}

			TestInstance* RayTracingTestCase::createInstance(Context& context) const
			{
				return new RayTracingWatertightnessTestInstance(context, m_data, m_useClosedFan);
			}

			de::MovePtr<TopLevelAccelerationStructure> RayTracingWatertightnessTestInstance::initTopAccelerationStructure(VkCommandBuffer												cmdBuffer,
				vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
			{
				const DeviceInterface& vkd = m_context.getDeviceInterface();
				const VkDevice								device = m_context.getDevice();
				Allocator& allocator = m_context.getDefaultAllocator();
				de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();

				result->setInstanceCount(bottomLevelAccelerationStructures.size());

				for (size_t structNdx = 0; structNdx < bottomLevelAccelerationStructures.size(); ++structNdx)
					result->addInstance(bottomLevelAccelerationStructures[structNdx]);

				result->createAndBuild(vkd, device, cmdBuffer, allocator);

				return result;
			}

			de::MovePtr<BottomLevelAccelerationStructure> RayTracingWatertightnessTestInstance::initBottomAccelerationStructure(VkCommandBuffer	cmdBuffer,
				bool				triangle)
			{
				const DeviceInterface& vkd = m_context.getDeviceInterface();
				const VkDevice									device = m_context.getDevice();
				Allocator& allocator = m_context.getDefaultAllocator();
				de::MovePtr<BottomLevelAccelerationStructure>	result = makeBottomLevelAccelerationStructure();
				de::Random										rng(m_data.randomSeed);
				std::vector<tcu::Vec3>							vertices;
				std::vector<tcu::UVec3>							triangles;
				std::vector<tcu::Vec3>							geometryData;

				result->setGeometryCount(1u);

				if (!m_useClosedFan)
				{
					vertices.reserve(3u * m_data.squaresGroupCount);

					vertices.push_back(tcu::Vec3(0.0f, 0.0f, -1.0f));
					vertices.push_back(tcu::Vec3(0.0f, 1.0f, -1.0f));
					vertices.push_back(tcu::Vec3(1.0f, 0.0f, -1.0f));
					vertices.push_back(tcu::Vec3(1.0f, 1.0f, -1.0f));

					triangles.reserve(m_data.squaresGroupCount);

					triangles.push_back(tcu::UVec3(0, 1, 2));
					triangles.push_back(tcu::UVec3(3, 2, 1));

					while (triangles.size() < m_data.squaresGroupCount)
					{
						const deUint32		n = (deUint32)rng.getInt(0, (deUint32)triangles.size() - 1);
						tcu::UVec3& t = triangles[n];
						const tcu::Vec3& a = vertices[t.x()];
						const tcu::Vec3& b = vertices[t.y()];
						const tcu::Vec3& c = vertices[t.z()];
						const float			alfa = rng.getFloat(0.01f, 0.99f);
						const float			beta = rng.getFloat(0.01f, 0.99f);
						const tcu::Vec3		mixed = mixVec3(mixVec3(a, b, alfa), c, beta);
						const float			z = -rng.getFloat(0.01f, 0.99f);
						const tcu::Vec3		d = tcu::Vec3(mixed.x(), mixed.y(), z);
						const deUint32& p = t.x();
						const deUint32& q = t.y();
						deUint32& r = t.z();
						const deUint32		R = (deUint32)vertices.size();

						vertices.push_back(d);

						triangles.push_back(tcu::UVec3(q, r, R));
						triangles.push_back(tcu::UVec3(p, r, R));
						r = R;
					}

					geometryData.reserve(3u * triangles.size());

					for (size_t i = 0; i < triangles.size(); ++i)
					{
						geometryData.push_back(vertices[triangles[i].x()]);
						geometryData.push_back(vertices[triangles[i].y()]);
						geometryData.push_back(vertices[triangles[i].z()]);
					}

					result->addGeometry(geometryData, triangle);
				}
				else
				{
					// Build a closed fan.
					vertices.push_back(tcu::Vec3(0.0f, 0.0f, 0.0f));

					for (deUint32 nSharedEdge = 0; nSharedEdge < m_data.squaresGroupCount; ++nSharedEdge)
					{
						const auto newVertex = tcu::Vec3(
							deFloatSin(2.0f * float(nSharedEdge) * 3.14159265f / float(m_data.squaresGroupCount)),
							deFloatCos(2.0f * float(nSharedEdge) * 3.14159265f / float(m_data.squaresGroupCount)),
							0.0f);

						vertices.push_back(newVertex);
					}

					for (deUint32 nSharedEdge = 0; nSharedEdge < m_data.squaresGroupCount; ++nSharedEdge)
					{
						const auto newTri = tcu::UVec3(
							0,
							1 + nSharedEdge,
							(nSharedEdge != m_data.squaresGroupCount - 1)	? (2 + nSharedEdge)
																			: 1
						);

						triangles.push_back(newTri);
					}

					geometryData.reserve(3u * triangles.size());

					for (size_t i = 0; i < triangles.size(); ++i)
					{
						geometryData.push_back(vertices[triangles[i].x()]);
						geometryData.push_back(vertices[triangles[i].y()]);
						geometryData.push_back(vertices[triangles[i].z()]);
					}

					result->addGeometry(geometryData, triangle, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
				}

				result->createAndBuild(vkd, device, cmdBuffer, allocator);

				return result;
			}

			vector<de::SharedPtr<BottomLevelAccelerationStructure> > RayTracingWatertightnessTestInstance::initBottomAccelerationStructures(VkCommandBuffer	cmdBuffer)
			{
				vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

				if (!m_useClosedFan)
				{
					for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)
					{
						de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = initBottomAccelerationStructure(cmdBuffer, true);

						result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
					}
				}
				else
				{
					// Build a closed fan.
					std::vector<tcu::Vec3>	vertices;
					std::vector<tcu::UVec3>	triangles;

					vertices.push_back(tcu::Vec3(0.0f, 0.0f, 0.0f));

					for (deUint32 nSharedEdge = 0; nSharedEdge < m_data.squaresGroupCount; ++nSharedEdge)
					{
						const auto newVertex = tcu::Vec3(
							deFloatSin(2.0f * float(nSharedEdge) * 3.14159265f / float(m_data.squaresGroupCount)),
							deFloatCos(2.0f * float(nSharedEdge) * 3.14159265f / float(m_data.squaresGroupCount)),
							0.0f);

						vertices.push_back(newVertex);
					}

					for (deUint32 nSharedEdge = 0; nSharedEdge < m_data.squaresGroupCount; ++nSharedEdge)
					{
						const auto newTri = tcu::UVec3(
							0,
							1 + nSharedEdge,
							(nSharedEdge != m_data.squaresGroupCount - 1) ? (2 + nSharedEdge)
							: 1
						);

						triangles.push_back(newTri);
					}

					{
						Allocator&				allocator	= m_context.getDefaultAllocator	();
						const VkDevice			device		= m_context.getDevice			();
						const DeviceInterface&	vkd			= m_context.getDeviceInterface	();

						if (!m_data.useManyBottomASes)
						{
							std::vector<tcu::Vec3>							geometryData;
							de::MovePtr<BottomLevelAccelerationStructure>	resultBLAS	= makeBottomLevelAccelerationStructure();

							geometryData.reserve(3u * triangles.size());

							for (size_t i = 0; i < triangles.size(); ++i)
							{
								geometryData.push_back(vertices[triangles[i].x()]);
								geometryData.push_back(vertices[triangles[i].y()]);
								geometryData.push_back(vertices[triangles[i].z()]);
							}

							resultBLAS->addGeometry		(geometryData, true /* triangles */, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
							resultBLAS->createAndBuild	(vkd, device, cmdBuffer, allocator);

							result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(resultBLAS.release()));
						}
						else
						{
							for (size_t i = 0; i < triangles.size(); ++i)
							{
								std::vector<tcu::Vec3>							geometryData;
								de::MovePtr<BottomLevelAccelerationStructure>	resultBLAS = makeBottomLevelAccelerationStructure();

								geometryData.push_back(vertices[triangles[i].x()]);
								geometryData.push_back(vertices[triangles[i].y()]);
								geometryData.push_back(vertices[triangles[i].z()]);

								resultBLAS->addGeometry		(geometryData, true /* triangles */, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
								resultBLAS->createAndBuild	(vkd, device, cmdBuffer, allocator);

								result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(resultBLAS.release()));
							}
						}
					}
				}

				return result;
			}

			de::MovePtr<BufferWithMemory> RayTracingWatertightnessTestInstance::runTest(void)
			{
				const InstanceInterface& vki = m_context.getInstanceInterface();
				const DeviceInterface& vkd = m_context.getDeviceInterface();
				const VkDevice						device = m_context.getDevice();
				const VkPhysicalDevice				physicalDevice = m_context.getPhysicalDevice();
				const deUint32						queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
				const VkQueue						queue = m_context.getUniversalQueue();
				Allocator& allocator = m_context.getDefaultAllocator();
				const VkFormat						format = VK_FORMAT_R32_UINT;
				const deUint32						pixelCount = m_data.width * m_data.height * m_data.depth;
				const deUint32						shaderGroupHandleSize = getShaderGroupSize(vki, physicalDevice);
				const deUint32						shaderGroupBaseAlignment = getShaderGroupBaseAlignment(vki, physicalDevice);

				const Move<VkDescriptorSetLayout>	descriptorSetLayout = DescriptorSetLayoutBuilder()
					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
					.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
					.build(vkd, device);
				const Move<VkDescriptorPool>		descriptorPool = DescriptorPoolBuilder()
					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
					.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
				const Move<VkDescriptorSet>			descriptorSet = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
				const Move<VkPipelineLayout>		pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());
				const Move<VkCommandPool>			cmdPool = createCommandPool(vkd, device, 0, queueFamilyIndex);
				const Move<VkCommandBuffer>			cmdBuffer = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

				de::MovePtr<RayTracingPipeline>		rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
				const Move<VkPipeline>				pipeline = makePipeline(vkd, device, m_context.getBinaryCollection(), rayTracingPipeline, *pipelineLayout, RAYGEN_GROUP, MISS_GROUP, HIT_GROUP);
				const de::MovePtr<BufferWithMemory>	raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, RAYGEN_GROUP, 1u);
				const de::MovePtr<BufferWithMemory>	missShaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, MISS_GROUP, 1u);
				const de::MovePtr<BufferWithMemory>	hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, HIT_GROUP, 1u);
				const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
				const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
				const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
				const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

				const VkImageCreateInfo				imageCreateInfo = makeImageCreateInfo(m_data.width, m_data.height, m_data.depth, format);
				const VkImageSubresourceRange		imageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
				const de::MovePtr<ImageWithMemory>	image = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
				const Move<VkImageView>				imageView = makeImageView(vkd, device, **image, (m_data.depth != 1) ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D, format, imageSubresourceRange);

				const VkBufferCreateInfo			bufferCreateInfo = makeBufferCreateInfo(pixelCount * sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
				const VkImageSubresourceLayers		bufferImageSubresourceLayers = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1);
				const VkBufferImageCopy				bufferImageRegion = makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, m_data.depth), bufferImageSubresourceLayers);
				de::MovePtr<BufferWithMemory>		buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

				const VkDescriptorImageInfo			descriptorImageInfo = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

				const VkImageMemoryBarrier			preImageBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					**image, imageSubresourceRange);
				const VkImageMemoryBarrier			postImageBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
					**image, imageSubresourceRange);
				const VkMemoryBarrier				postTraceMemoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
				const VkMemoryBarrier				postCopyMemoryBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, 0);
				const VkClearValue					clearValue = (!m_useClosedFan)	? makeClearValueColorU32(5u, 5u, 5u, 255u)
																					: makeClearValueColorU32(0u, 0u, 0u, 0u);

				vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
				de::MovePtr<TopLevelAccelerationStructure>					topLevelAccelerationStructure;

				beginCommandBuffer(vkd, *cmdBuffer, 0u);
				{
					cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
					vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
					cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

					bottomLevelAccelerationStructures = initBottomAccelerationStructures(*cmdBuffer);
					topLevelAccelerationStructure = initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructures);

					const TopLevelAccelerationStructure* topLevelAccelerationStructurePtr = topLevelAccelerationStructure.get();
					VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet =
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

					vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

					if (!m_useClosedFan)
					{
						cmdTraceRays(vkd,
							*cmdBuffer,
							&raygenShaderBindingTableRegion,
							&missShaderBindingTableRegion,
							&hitShaderBindingTableRegion,
							&callableShaderBindingTableRegion,
							m_data.width, m_data.height, 1);
					}
					else
					{
						cmdTraceRays(vkd,
							*cmdBuffer,
							&raygenShaderBindingTableRegion,
							&missShaderBindingTableRegion,
							&hitShaderBindingTableRegion,
							&callableShaderBindingTableRegion,
							1 + m_data.width,
							m_data.height,
							1);
					}

					cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

					vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &bufferImageRegion);

					cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
				}
				endCommandBuffer(vkd, *cmdBuffer);

				submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

				invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(), pixelCount * sizeof(deUint32));

				return buffer;
			}

			void RayTracingWatertightnessTestInstance::checkSupportInInstance(void) const
			{
				const InstanceInterface& vki = m_context.getInstanceInterface();
				const VkPhysicalDevice					physicalDevice = m_context.getPhysicalDevice();
				const vk::VkPhysicalDeviceProperties& properties = m_context.getDeviceProperties();
				const deUint32							requiredAllocations = 8u
					+ TopLevelAccelerationStructure::getRequiredAllocationCount()
					+ m_data.instancesGroupCount * BottomLevelAccelerationStructure::getRequiredAllocationCount();
				de::MovePtr<RayTracingProperties>		rayTracingProperties = makeRayTracingProperties(vki, physicalDevice);

				if (rayTracingProperties->getMaxPrimitiveCount() < m_data.squaresGroupCount)
					TCU_THROW(NotSupportedError, "Triangles required more than supported");

				if (rayTracingProperties->getMaxGeometryCount() < m_data.geometriesGroupCount)
					TCU_THROW(NotSupportedError, "Geometries required more than supported");

				if (rayTracingProperties->getMaxInstanceCount() < m_data.instancesGroupCount)
					TCU_THROW(NotSupportedError, "Instances required more than supported");

				if (properties.limits.maxMemoryAllocationCount < requiredAllocations)
					TCU_THROW(NotSupportedError, "Test requires more allocations allowed");
			}

			tcu::TestStatus RayTracingWatertightnessTestInstance::iterate(void)
			{
				checkSupportInInstance();

				const de::MovePtr<BufferWithMemory>	bufferGPU				= runTest();
				const deUint32*						bufferPtrGPU			= (deUint32*)bufferGPU->getAllocation().getHostPtr();
				deUint32							failures				= 0;
				deUint32							qualityWarningIssued	= 0;
				if (!m_useClosedFan)
				{
					deUint32	pos = 0;

					for (deUint32 nIntersection = 0; nIntersection < m_data.squaresGroupCount; ++nIntersection)
					{
						if (bufferPtrGPU[pos] != 1)
							failures++;

						++pos;
					}
				}
				else
				{
					// Values larger than 1, excl. 10000 raise a failure since they indicate the impl ignored the VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR flag.
					// A value of 10000 triggers a quality warning, as this indicates a miss which, per spec language, is discouraged but not forbidden.
					//
					// See the miss shader for explanation of the magic number.
					for (deUint32 pos = 0; pos < m_data.width * m_data.height * m_data.depth; ++pos)
					{
						if (bufferPtrGPU[pos] == 10000)
						{
							failures++;
						}
						else
						if (bufferPtrGPU[pos] > 1)
						{
							failures				++;
							qualityWarningIssued	= 1;
						}
					}
				}

				if (failures == 0)
				{
					if (qualityWarningIssued)
						return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Miss shader invoked for a shared edge/vertex.");
					else
						return tcu::TestStatus::pass("Pass");
				}
				else
					return tcu::TestStatus::fail("failures=" + de::toString(failures));
			}

		}	// anonymous

		tcu::TestCaseGroup* createWatertightnessTests(tcu::TestContext& testCtx)
		{
			de::MovePtr<tcu::TestCaseGroup> watertightnessGroup(new tcu::TestCaseGroup(testCtx, "watertightness", "Ray watertightness tests"));

			const size_t	numTests = 10;

			for (size_t testNdx = 0; testNdx < numTests; ++testNdx)
			{
				de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, de::toString(testNdx).c_str(), ""));
				const deUint32					sizes[] = { 4, 16, 64, 256, 1024, 4096, 16384, 65536 };

				// Legacy tests
				for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
				{
					const deUint32	squaresGroupCount = sizes[sizesNdx];
					const deUint32	geometriesGroupCount = 1;
					const deUint32	instancesGroupCount = 1;
					const deUint32	randomSeed = (deUint32)(5 * testNdx + 11 * sizes[sizesNdx]);
					const CaseDef	caseDef =
					{
						256u,
						256u,
						squaresGroupCount,
						geometriesGroupCount,
						instancesGroupCount,
						randomSeed,
						1, /* depth - irrelevant */
						0  /* useManyBottomASes - irrelevant */
					};
					const std::string	testName = de::toString(caseDef.squaresGroupCount);

					group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef, false /* useClosedFan */));
				}

				watertightnessGroup->addChild(group.release());
			}

			// Closed fan tests
			{
				const deUint32	sizes[] = { 4, 16, 64, 256, 1024 };

				for (deUint32 nBottomASConfig = 0; nBottomASConfig < 2; ++nBottomASConfig)
				{
					const auto groupName = (nBottomASConfig == 0)	? "closedFan"
																	: "closedFan2";

					de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, groupName, ""));

					for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
					{
						const deUint32	sharedEdgeCount = sizes[sizesNdx];
						const CaseDef	caseDef =
						{
							// The extra item in <width> is required to accomodate the extra center vertex, against which the test also shoots rays.
							1 + static_cast<deUint32>(deSqrt(sharedEdgeCount)),	/* width  */
							static_cast<deUint32>(deSqrt(sharedEdgeCount)),		/* height */
							sharedEdgeCount,
							1,														/* geometriesGroupCount - irrelevant */
							1,														/* instancesGroupCount  - irrelevant */
							1,														/* randomSeed           - irrelevant */
							sharedEdgeCount,										/* depth							 */
							nBottomASConfig
						};
						const std::string	testName = de::toString(sharedEdgeCount);

						group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef, true /* useClosedFan */));
					}

					watertightnessGroup->addChild(group.release());
				}
			}

			return watertightnessGroup.release();
		}

	}	// RayTracing
}	// vkt
