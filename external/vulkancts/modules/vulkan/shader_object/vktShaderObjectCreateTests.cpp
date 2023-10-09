/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Shader Object Create Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectCreateTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkQueryUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkRefUtil.hpp"
#include "deRandom.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vkBuilderUtil.hpp"

namespace vkt
{
namespace ShaderObject
{

namespace
{

class ShaderObjectCreateInstance : public vkt::TestInstance
{
public:
						ShaderObjectCreateInstance	(Context& context, const bool useMeshShaders)
							: vkt::TestInstance	(context)
							, m_useMeshShaders	(useMeshShaders)
							{}
	virtual				~ShaderObjectCreateInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;

private:
	const bool m_useMeshShaders;
};

tcu::TestStatus ShaderObjectCreateInstance::iterate	(void)
{
	const vk::VkInstance		instance					= m_context.getInstance();
	const vk::InstanceDriver	instanceDriver				(m_context.getPlatformInterface(), instance);
	const vk::DeviceInterface&	vk							= m_context.getDeviceInterface();
	const vk::VkDevice			device						= m_context.getDevice();
	tcu::TestLog&				log							= m_context.getTestContext().getLog();
	const bool					tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool					geometrySupported			= m_context.getDeviceFeatures().geometryShader;

	const auto&					binaries		= m_context.getBinaryCollection();
	const auto&					vert			= binaries.get("vert");
	const auto&					tesc			= binaries.get("tesc");
	const auto&					tese			= binaries.get("tese");
	const auto&					geom			= binaries.get("geom");
	const auto&					frag			= binaries.get("frag");
	const auto&					comp			= binaries.get("comp");

	const vk::VkDescriptorSetLayoutBinding layoutBinding =
	{
		0u,														// deUint32					binding;
		vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,			// VkDescriptorType			descriptorType;
		1u,														// deUint32					arraySize;
		vk::VK_SHADER_STAGE_COMPUTE_BIT,						// VkShaderStageFlags		stageFlags;
		DE_NULL													// const VkSampler*			pImmutableSamplers;
	};

	const vk::VkDescriptorSetLayoutCreateInfo descriptorLayoutParams =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,													// const void*							pNext;
		(vk::VkDescriptorSetLayoutCreateFlags)0,					// VkDescriptorSetLayoutCreateFlags		flags;
		1u,															// deUint32								count;
		&layoutBinding												// const VkDescriptorSetLayoutBinding	pBinding;
	};

	const auto descriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &descriptorLayoutParams);

	// Todo: remove const_cast if spec is updated
	vk::VkDescriptorSetLayout* setLayout = const_cast<vk::VkDescriptorSetLayout*>(&descriptorSetLayout.get());

	std::vector<vk::VkShaderCreateInfoEXT> shaderCreateInfos =
	{
		{
			vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkShaderCreateFlagsEXT		flags;
			vk::VK_SHADER_STAGE_VERTEX_BIT,					// VkShaderStageFlagBits		stage;
			vk::getShaderObjectNextStages(vk::VK_SHADER_STAGE_VERTEX_BIT, tessellationSupported, geometrySupported),	// VkShaderStageFlags			nextStage;
			vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
			vert.getSize(),									// size_t						codeSize;
			vert.getBinary(),								// const void*					pCode;
			"main",											// const char*					pName;
			0u,												// uint32_t						setLayoutCount;
			DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
			0u,												// uint32_t						pushConstantRangeCount;
			DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
			DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
		},
		{
			vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkShaderCreateFlagsEXT		flags;
			vk::VK_SHADER_STAGE_FRAGMENT_BIT,				// VkShaderStageFlagBits		stage;
			vk::getShaderObjectNextStages(vk::VK_SHADER_STAGE_FRAGMENT_BIT, tessellationSupported, geometrySupported),	// VkShaderStageFlags			nextStage;
			vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
			frag.getSize(),									// size_t						codeSize;
			frag.getBinary(),								// const void*					pCode;
			"main",											// const char*					pName;
			0u,												// uint32_t						setLayoutCount;
			DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
			0u,												// uint32_t						pushConstantRangeCount;
			DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
			DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
		},
		{
			vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkShaderCreateFlagsEXT		flags;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,				// VkShaderStageFlagBits		stage;
			0u,												// VkShaderStageFlags			nextStage;
			vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
			comp.getSize(),									// size_t						codeSize;
			comp.getBinary(),								// const void*					pCode;
			"main",											// const char*					pName;
			1u,												// uint32_t						setLayoutCount;
			setLayout,										// VkDescriptorSetLayout*		pSetLayouts;
			0u,												// uint32_t						pushConstantRangeCount;
			DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
			DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
		},
	};

	if (m_context.getDeviceFeatures().tessellationShader)
	{
		shaderCreateInfos.push_back(
			{
				vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
				DE_NULL,										// const void*					pNext;
				0u,												// VkShaderCreateFlagsEXT		flags;
				vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,	// VkShaderStageFlagBits		stage;
				vk::getShaderObjectNextStages(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, tessellationSupported, geometrySupported),	// VkShaderStageFlags			nextStage;
				vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
				tesc.getSize(),									// size_t						codeSize;
				tesc.getBinary(),								// const void*					pCode;
				"main",											// const char*					pName;
				0u,												// uint32_t						setLayoutCount;
				DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
				0u,												// uint32_t						pushConstantRangeCount;
				DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
				DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
			}
		);
		shaderCreateInfos.push_back(
			{
				vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
				DE_NULL,										// const void*					pNext;
				0u,												// VkShaderCreateFlagsEXT		flags;
				vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,// VkShaderStageFlagBits		stage;
				vk::getShaderObjectNextStages(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, tessellationSupported, geometrySupported),	// VkShaderStageFlags			nextStage;
				vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
				tese.getSize(),									// size_t						codeSize;
				tese.getBinary(),								// const void*					pCode;
				"main",											// const char*					pName;
				0u,												// uint32_t						setLayoutCount;
				DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
				0u,												// uint32_t						pushConstantRangeCount;
				DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
				DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
			}
		);
	}

	if (m_context.getDeviceFeatures().geometryShader)
	{
		shaderCreateInfos.push_back(
			{
				vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
				DE_NULL,										// const void*					pNext;
				0u,												// VkShaderCreateFlagsEXT		flags;
				vk::VK_SHADER_STAGE_GEOMETRY_BIT,				// VkShaderStageFlagBits		stage;
				vk::getShaderObjectNextStages(vk::VK_SHADER_STAGE_GEOMETRY_BIT, tessellationSupported, geometrySupported),	// VkShaderStageFlags			nextStage;
				vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
				geom.getSize(),									// size_t						codeSize;
				geom.getBinary(),								// const void*					pCode;
				"main",											// const char*					pName;
				0u,												// uint32_t						setLayoutCount;
				DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
				0u,												// uint32_t						pushConstantRangeCount;
				DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
				DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
			}
		);
	}

	if (m_useMeshShaders)
	{
		const auto& mesh = binaries.get("mesh");
		const auto& task = binaries.get("task");
		if (m_context.getMeshShaderFeaturesEXT().meshShader)
		{
			shaderCreateInfos.push_back(
				{
					vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
					DE_NULL,										// const void*					pNext;
					0u,												// VkShaderCreateFlagsEXT		flags;
					vk::VK_SHADER_STAGE_MESH_BIT_EXT,				// VkShaderStageFlagBits		stage;
					0u,												// VkShaderStageFlags			nextStage;
					vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
					mesh.getSize(),									// size_t						codeSize;
					mesh.getBinary(),								// const void*					pCode;
					"main",											// const char*					pName;
					0u,												// uint32_t						setLayoutCount;
					DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
					0u,												// uint32_t						pushConstantRangeCount;
					DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
					DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
				}
			);
		}

		if (m_context.getMeshShaderFeaturesEXT().taskShader)
		{
			shaderCreateInfos.push_back(
				{
					vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
					DE_NULL,										// const void*					pNext;
					0u,												// VkShaderCreateFlagsEXT		flags;
					vk::VK_SHADER_STAGE_TASK_BIT_EXT,				// VkShaderStageFlagBits		stage;
					0u,												// VkShaderStageFlags			nextStage;
					vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
					task.getSize(),									// size_t						codeSize;
					task.getBinary(),								// const void*					pCode;
					"main",											// const char*					pName;
					0u,												// uint32_t						setLayoutCount;
					DE_NULL,										// VkDescriptorSetLayout*		pSetLayouts;
					0u,												// uint32_t						pushConstantRangeCount;
					DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
					DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
				}
			);
		}
	}

	std::vector<vk::VkShaderEXT>	shadersSeparate		(shaderCreateInfos.size());
	std::vector<vk::VkShaderEXT>	shadersTogether		(shaderCreateInfos.size());
	for (deUint32 i = 0; i < (deUint32)shaderCreateInfos.size(); ++i)
	{
		vk.createShadersEXT(device, 1, &shaderCreateInfos[i], DE_NULL, &shadersSeparate[i]);
	}
	vk.createShadersEXT(device, (deUint32)shaderCreateInfos.size(), &shaderCreateInfos[0], DE_NULL, &shadersTogether[0]);

	bool							match				= true;
	for (deUint32 i = 0; i < (deUint32)shaderCreateInfos.size(); ++i)
	{
		size_t dataSizeSeparate = 0;
		size_t dataSizeTogether = 0;
		vk.getShaderBinaryDataEXT(device, shadersSeparate[i], &dataSizeSeparate, DE_NULL);
		vk.getShaderBinaryDataEXT(device, shadersTogether[i], &dataSizeTogether, DE_NULL);
		if (dataSizeSeparate != dataSizeTogether)
		{
			log << tcu::TestLog::Message << "Data size of shader created separately is " << dataSizeSeparate << ", but data size of shader created in the same call with others is " << dataSizeTogether << tcu::TestLog::EndMessage;
			match = false;
			break;
		}
		std::vector<deUint8> dataSeparate(dataSizeSeparate);
		std::vector<deUint8> dataTogether(dataSizeTogether);
		vk.getShaderBinaryDataEXT(device, shadersSeparate[i], &dataSizeSeparate, &dataSeparate[0]);
		vk.getShaderBinaryDataEXT(device, shadersTogether[i], &dataSizeTogether, &dataTogether[0]);
		for (deUint32 j = 0; j < dataSizeSeparate; ++j)
		{
			if (dataSeparate[j] != dataTogether[j])
			{
				log << tcu::TestLog::Message << "Data of shader created separately and data of shader created in the same call with others does not match at index " << j << tcu::TestLog::EndMessage;
				match = false;
				break;
			}
		}
		if (!match)
			break;
	}

	for (const auto& shader : shadersSeparate)
		vk.destroyShaderEXT(device, shader, DE_NULL);
	for (const auto& shader : shadersTogether)
		vk.destroyShaderEXT(device, shader, DE_NULL);

	if (!match)
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectCreateCase : public vkt::TestCase
{
public:
					ShaderObjectCreateCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const bool useMeshShaders)
						: vkt::TestCase		(testCtx, name, description)
						, m_useMeshShaders	(useMeshShaders)
						{}
	virtual			~ShaderObjectCreateCase	(void) {}

	void			checkSupport			(vkt::Context& context) const override;
	virtual void	initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override { return new ShaderObjectCreateInstance(context, m_useMeshShaders); }
private:
	bool m_useMeshShaders;
};

void ShaderObjectCreateCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
}

void ShaderObjectCreateCase::initPrograms(vk::SourceCollections& programCollection) const
{
	std::stringstream vert;
	std::stringstream geom;
	std::stringstream tesc;
	std::stringstream tese;
	std::stringstream frag;
	std::stringstream comp;
	std::stringstream mesh;
	std::stringstream task;

	vert
		<< "#version 450\n"
		<< "layout (location=0) in vec2 inPos;\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos, 0.0f, 1.0f);\n"
		<< "}\n";

	tesc
		<< "#version 450\n"
		<< "\n"
		<< "layout(vertices = 3) out;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    gl_TessLevelInner[0] = 5.0;\n"
		<< "    gl_TessLevelInner[1] = 5.0;\n"
		<< "\n"
		<< "    gl_TessLevelOuter[0] = 5.0;\n"
		<< "    gl_TessLevelOuter[1] = 5.0;\n"
		<< "    gl_TessLevelOuter[2] = 5.0;\n"
		<< "    gl_TessLevelOuter[3] = 5.0;\n"
		<< "}\n";

	tese
		<< "#version 450\n"
		<< "\n"
		<< "layout(quads) in;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    highp float x = gl_TessCoord.x*2.0 - 1.0;\n"
		<< "    highp float y = gl_TessCoord.y*2.0 - 1.0;\n"
		<< "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
		<< "}\n";

	geom
		<< "#version 450\n"
		<< "layout(points) in;\n"
		<< "layout(points, max_vertices = 1) out;\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "    EmitVertex();\n"
		<< "    EndPrimitive();\n"
		<< "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f);\n"
		<< "}\n";

	comp
		<< "#version 450\n"
		<< "layout(local_size_x=16, local_size_x=1, local_size_x=1) in;\n"
		<< "layout(binding = 0) buffer Output {\n"
		<< "    uint values[16];\n"
		<< "} buffer_out;\n\n"
		<< "void main() {\n"
		<< "    buffer_out.values[gl_LocalInvocationID.x] = gl_LocalInvocationID.x;\n"
		<< "}\n";

	mesh
		<< "#version 460\n"
		<< "#extension GL_EXT_mesh_shader : require\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout(max_vertices = 3) out;\n"
		<< "layout(max_primitives = 1) out;\n"
		<< "layout(triangles) out;\n"
		<< "void main() {\n"
		<< "      SetMeshOutputsEXT(3,1);\n"
		<< "      gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0, 1);\n"
		<< "      gl_MeshVerticesEXT[1].gl_Position = vec4( 1.0, -1.0, 0, 1);\n"
		<< "      gl_MeshVerticesEXT[2].gl_Position = vec4( 0.0,  1.0, 0, 1);\n"
		<< "      gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0,1,2);\n"
		<< "}\n";

	task
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "struct TaskData {\n"
		<< "	int t;\n"
		<< "};\n"
		<< "taskPayloadSharedEXT TaskData td;\n"
		<< "void main ()\n"
		<< "{\n"
		<< "	td.t = 1;\n"
		<< "	EmitMeshTasksEXT(1u, 1u, 1u);\n"
		<< "}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
	programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
	programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
	if (m_useMeshShaders)
	{
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	}
}

class ShaderObjectStageInstance : public vkt::TestInstance
{
public:
						ShaderObjectStageInstance	(Context& context, const vk::VkShaderStageFlagBits stage, const bool fail, const bool useMeshShaders)
							: vkt::TestInstance	(context)
							, m_stage			(stage)
							, m_fail			(fail)
							, m_useMeshShaders	(useMeshShaders)
							{}

	virtual				~ShaderObjectStageInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;
private:
	const vk::VkShaderStageFlagBits	m_stage;
	const bool						m_fail;
	const bool						m_useMeshShaders;
};

tcu::TestStatus ShaderObjectStageInstance::iterate (void)
{
	const vk::VkInstance					instance				= m_context.getInstance();
	const vk::InstanceDriver				instanceDriver			(m_context.getPlatformInterface(), instance);
	const vk::DeviceInterface&				vk						= m_context.getDeviceInterface();
	const vk::VkDevice						device					= m_context.getDevice();
	tcu::TestLog&							log						= m_context.getTestContext().getLog();
	const bool								tessellationSupported	= m_context.getDeviceFeatures().tessellationShader;
	const bool								geometrySupported		= m_context.getDeviceFeatures().geometryShader;

	const auto&								binaries		= m_context.getBinaryCollection();

	de::Random								random			(102030);
	std::vector<vk::VkShaderStageFlagBits>	stages			=
	{
		vk::VK_SHADER_STAGE_VERTEX_BIT,
		vk::VK_SHADER_STAGE_FRAGMENT_BIT,
		vk::VK_SHADER_STAGE_COMPUTE_BIT,
	};
	if (m_context.getDeviceFeatures().tessellationShader)
	{
		stages.push_back(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		stages.push_back(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	}
	if (m_context.getDeviceFeatures().geometryShader)
	{
		stages.push_back(vk::VK_SHADER_STAGE_GEOMETRY_BIT);
	}
	if (m_useMeshShaders)
	{
		if (m_context.getMeshShaderFeaturesEXT().meshShader)
			stages.push_back(vk::VK_SHADER_STAGE_MESH_BIT_EXT);
		if (m_context.getMeshShaderFeaturesEXT().taskShader)
			stages.push_back(vk::VK_SHADER_STAGE_TASK_BIT_EXT);
	}

	const deUint32								count	= m_stage == vk::VK_SHADER_STAGE_ALL ? 50 : 10;

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));


	std::vector<vk::VkShaderCreateInfoEXT>		shaderCreateInfos;

	for (deUint32 i = 0; i < count; ++i)
	{
		vk::VkShaderStageFlagBits stage;
		if (m_stage == vk::VK_SHADER_STAGE_ALL)
			stage = stages[random.getUint32() % stages.size()];
		else
			stage = m_stage;

		bool useLayout = stage == vk::VK_SHADER_STAGE_COMPUTE_BIT;

		const auto&				src			= binaries.get(getShaderName(stage) + std::to_string(i % 10));
		shaderCreateInfos.push_back(
			{
				vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
				DE_NULL,										// const void*					pNext;
				0u,												// VkShaderCreateFlagsEXT		flags;
				stage,											// VkShaderStageFlagBits		stage;
				vk::getShaderObjectNextStages(stage, tessellationSupported, geometrySupported),			// VkShaderStageFlags			nextStage;
				vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
				src.getSize(),									// size_t						codeSize;
				src.getBinary(),								// const void*					pCode;
				"main",											// const char*					pName;
				useLayout ? 1u : 0u,							// uint32_t						setLayoutCount;
				useLayout ? &*descriptorSetLayout : DE_NULL,	// VkDescriptorSetLayout*		pSetLayouts;
				0u,												// uint32_t						pushConstantRangeCount;
				DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
				DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
			});
	}

	std::vector<vk::VkShaderEXT>		shaders			(count, VK_NULL_HANDLE);
	vk::VkResult						result;
	result = vk.createShadersEXT(device, count, &shaderCreateInfos[0], DE_NULL, &shaders[0]);
	if (result != vk::VK_SUCCESS)
	{
		log << tcu::TestLog::Message << "vkCreateShadersEXT returned " << result << tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Fail");
	}

	std::vector<size_t>					binarySizes		(count);
	std::vector<std::vector<deUint8>>	binaryData		(count);
	for (size_t i = 0; i < count; ++i)
	{
		vk.getShaderBinaryDataEXT(device, shaders[i], &binarySizes[i], DE_NULL);
		binaryData[i].resize(binarySizes[i]);
		vk.getShaderBinaryDataEXT(device, shaders[i], &binarySizes[i], (void*)&binaryData[i][0]);
	}

	for (const auto& shader : shaders)
		vk.destroyShaderEXT(device, shader, DE_NULL);

	const deUint32	failIndex = random.getUint32() % count;

	for (deUint32 i = 0; i < count; ++i)
	{
		shaderCreateInfos[i].codeType = vk::VK_SHADER_CODE_TYPE_BINARY_EXT;
		if (m_fail && failIndex == i)
			shaderCreateInfos[i].codeSize = 1;
		else
			shaderCreateInfos[i].codeSize = binarySizes[i];
		shaderCreateInfos[i].pCode = &binaryData[i][0];
	}

	deUint32 garbage = 1234u;
	std::vector<vk::VkShaderEXT>		binaryShaders	(count, garbage); // Fill with garbage
	result = vk.createShadersEXT(device, count, &shaderCreateInfos[0], DE_NULL, &binaryShaders[0]);

	if (m_fail)
	{
		if (result != vk::VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT)
		{
			log << tcu::TestLog::Message << "Shader at index " << failIndex << "was created with size 0, but vkCreateShadersEXT returned " << result << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}

		for (deUint32 i = 0; i < failIndex; ++i)
		{
			if (binaryShaders[i] == garbage)
			{
				log << tcu::TestLog::Message << "vkCreateShadersEXT returned VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT and failed at index " << failIndex << ", but shader at index " << i << "was not created" << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
			vk.destroyShaderEXT(device, binaryShaders[i], DE_NULL);
		}
		if (binaryShaders[failIndex] != VK_NULL_HANDLE)
		{
			log << tcu::TestLog::Message << "vkCreateShadersEXT returned VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT, creating shader at index " << failIndex << " failed, but the shader is not VK_NULL_HANDLE" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}
	else
	{
		if (result != vk::VK_SUCCESS)
		{
			log << tcu::TestLog::Message << "vkCreateShadersEXT returned " << result << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}

		for (const auto& shader : binaryShaders)
			vk.destroyShaderEXT(device, shader, DE_NULL);
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectStageCase : public vkt::TestCase
{
public:
					ShaderObjectStageCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const vk::VkShaderStageFlagBits stage, const bool fail, const bool useMeshShaders)
						: vkt::TestCase		(testCtx, name, description)
						, m_stage			(stage)
						, m_fail			(fail)
						, m_useMeshShaders	(useMeshShaders)
						{}
	virtual			~ShaderObjectStageCase	(void) {}

	void			checkSupport			(vkt::Context& context) const override;
	virtual void	initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override { return new ShaderObjectStageInstance(context, m_stage, m_fail, m_useMeshShaders); }
private:
	const vk::VkShaderStageFlagBits			m_stage;
	const bool								m_fail;
	const bool								m_useMeshShaders;
};

void ShaderObjectStageCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
	if (m_useMeshShaders)
		context.requireDeviceFunctionality("VK_EXT_mesh_shader");

	if (m_stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || m_stage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
	if (m_stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	if (m_stage == vk::VK_SHADER_STAGE_TASK_BIT_EXT && context.getMeshShaderFeaturesEXT().taskShader == VK_FALSE)
		TCU_THROW(NotSupportedError, "Task shaders not supported");
	if (m_stage == vk::VK_SHADER_STAGE_MESH_BIT_EXT && context.getMeshShaderFeaturesEXT().meshShader == VK_FALSE)
		TCU_THROW(NotSupportedError, "Mesh shaders not supported");
}

void ShaderObjectStageCase::initPrograms (vk::SourceCollections& programCollection) const
{
	for (deUint32 i = 0; i < 10; ++i)
	{
		std::stringstream vert;
		std::stringstream geom;
		std::stringstream tesc;
		std::stringstream tese;
		std::stringstream frag;
		std::stringstream comp;
		std::stringstream mesh;
		std::stringstream task;

		vert
			<< "#version 450\n"
			<< "layout (location=0) in vec2 inPos;\n"
			<< "void main() {\n"
			<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
			<< "    gl_Position = vec4(pos * float(" << i << "), 0.0f, 1.0f);\n"
			<< "}\n";

		tesc
			<< "#version 450\n"
			<< "\n"
			<< "layout(vertices = 3) out;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_TessLevelInner[0] = 5.0 + float(" << i << ");\n"
			<< "    gl_TessLevelInner[1] = 5.0 + float(" << i << ");\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 5.0;\n"
			<< "    gl_TessLevelOuter[1] = 5.0;\n"
			<< "    gl_TessLevelOuter[2] = 5.0;\n"
			<< "    gl_TessLevelOuter[3] = 5.0;\n"
			<< "}\n";

		tese
			<< "#version 450\n"
			<< "\n"
			<< "layout(quads) in;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    highp float x = gl_TessCoord.x * float(" << i << ") - 1.0;\n"
			<< "    highp float y = gl_TessCoord.y * float(" << i << ") - 1.0;\n"
			<< "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
			<< "}\n";

		geom
			<< "#version 450\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = gl_in[0].gl_Position;\n"
			<< "    gl_Position.xy += vec2(float(" << i << "));\n"
			<< "    EmitVertex();\n"
			<< "    EndPrimitive();\n"
			<< "}\n";

		frag
			<< "#version 450\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "void main() {\n"
			<< "    outColor = vec4(1.0f / (1.0f + float(" << i << ")));\n"
			<< "}\n";

		comp
			<< "#version 450\n"
			<< "layout(local_size_x=16, local_size_x=1, local_size_x=1) in;\n"
			<< "layout(binding = 0) buffer Output {\n"
			<< "    uint values[16];\n"
			<< "} buffer_out;\n\n"
			<< "void main() {\n"
			<< "    buffer_out.values[gl_LocalInvocationID.x] = gl_LocalInvocationID.x + " << i << ";\n"
			<< "}\n";

		mesh
			<< "#version 460\n"
			<< "#extension GL_EXT_mesh_shader : require\n"
			<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			<< "layout(max_vertices = 3) out;\n"
			<< "layout(max_primitives = 1) out;\n"
			<< "layout(triangles) out;\n"
			<< "void main() {\n"
			<< "      SetMeshOutputsEXT(3,1);\n"
			<< "      gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0, 1);\n"
			<< "      gl_MeshVerticesEXT[1].gl_Position = vec4( 1.0, -1.0, 0, 1);\n"
			<< "      gl_MeshVerticesEXT[2].gl_Position = vec4( 0.0,  1.0, 0, 1);\n"
			<< "      gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0,1,2);\n"
			<< "}\n";

		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< "struct TaskData {\n"
			<< "	int t;\n"
			<< "};\n"
			<< "taskPayloadSharedEXT TaskData td;\n"
			<< "void main ()\n"
			<< "{\n"
			<< "	td.t = 1;\n"
			<< "	EmitMeshTasksEXT(1u, 1u, 1u);\n"
			<< "}\n";

		programCollection.glslSources.add("vert" + std::to_string(i)) << glu::VertexSource(vert.str());
		programCollection.glslSources.add("tesc" + std::to_string(i)) << glu::TessellationControlSource(tesc.str());
		programCollection.glslSources.add("tese" + std::to_string(i)) << glu::TessellationEvaluationSource(tese.str());
		programCollection.glslSources.add("geom" + std::to_string(i)) << glu::GeometrySource(geom.str());
		programCollection.glslSources.add("frag" + std::to_string(i)) << glu::FragmentSource(frag.str());
		programCollection.glslSources.add("comp" + std::to_string(i)) << glu::ComputeSource(comp.str());
		if (m_useMeshShaders)
		{
			programCollection.glslSources.add("mesh" + std::to_string(i)) << glu::MeshSource(mesh.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
			programCollection.glslSources.add("task" + std::to_string(i)) << glu::TaskSource(task.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
		}
	}
}

}

tcu::TestCaseGroup* createShaderObjectCreateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> createGroup(new tcu::TestCaseGroup(testCtx, "create", ""));

	de::MovePtr<tcu::TestCaseGroup> multipleGroup(new tcu::TestCaseGroup(testCtx, "multiple", ""));

	multipleGroup->addChild(new ShaderObjectCreateCase(testCtx, "all", "", false));
	multipleGroup->addChild(new ShaderObjectCreateCase(testCtx, "all_with_mesh", "", true));

	createGroup->addChild(multipleGroup.release());

	const struct
	{
		vk::VkShaderStageFlagBits	stage;
		const bool					useMeshShaders;
		const char*					name;
	} stageTests[] =
	{
		{ vk::VK_SHADER_STAGE_VERTEX_BIT,					false,		"vert"				},
		{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		false,		"tesc"				},
		{ vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	false,		"tese"				},
		{ vk::VK_SHADER_STAGE_GEOMETRY_BIT,					false,		"geom"				},
		{ vk::VK_SHADER_STAGE_FRAGMENT_BIT,					false,		"frag"				},
		{ vk::VK_SHADER_STAGE_COMPUTE_BIT,					false,		"comp"				},
		{ vk::VK_SHADER_STAGE_MESH_BIT_EXT,					true,		"mesh"				},
		{ vk::VK_SHADER_STAGE_TASK_BIT_EXT,					true,		"task"				},
		{ vk::VK_SHADER_STAGE_ALL,							false,		"all"				},
		{ vk::VK_SHADER_STAGE_ALL,							true,		"all_with_mesh"		},
	};

	const struct
	{
		bool		fail;
		const char* name;
	} failTests[] =
	{
		{ false,	"succeed"		},
		{ true,		"fail"			},
	};

	for (const auto& stage : stageTests)
	{
		de::MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stage.name, ""));
		for (const auto& failTest : failTests)
		{
			stageGroup->addChild(new ShaderObjectStageCase(testCtx, failTest.name, "", stage.stage, failTest.fail, stage.useMeshShaders));
		}
		createGroup->addChild(stageGroup.release());
	}

	return createGroup.release();
}

} // ShaderObject
} // vkt
