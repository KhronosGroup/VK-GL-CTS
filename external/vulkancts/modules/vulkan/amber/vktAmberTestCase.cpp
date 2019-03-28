/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Functional tests using amber
 *//*--------------------------------------------------------------------*/

#include <amber/amber.h>

#include <iostream>

#include "deUniquePtr.hpp"
#include "deFilePath.hpp"
#include "vktTestCaseUtil.hpp"
#include "tcuTestLog.hpp"
#include "vktAmberTestCase.hpp"
#include "vktAmberHelper.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace cts_amber
{

AmberTestCase::AmberTestCase (tcu::TestContext& testCtx,
							  const char*		name,
							  const char*		description)
	: TestCase(testCtx, name, description)
{
}

AmberTestCase::~AmberTestCase (void)
{
	delete m_recipe;
}

TestInstance* AmberTestCase::createInstance(Context& ctx) const
{
	return new AmberTestInstance(ctx, m_recipe);
}

bool AmberTestCase::parse(const char* category, const std::string& filename)
{
	std::string readFilename("vulkan/amber/");
	readFilename.append(category);
	readFilename.append("/");
	readFilename.append(filename);

	std::string script = ShaderSourceProvider::getSource(m_testCtx.getArchive(), readFilename.c_str());
	if (script.empty())
		return false;

	m_recipe = new amber::Recipe();

	amber::Amber am;
	amber::Result r = am.Parse(script, m_recipe);
	if (!r.IsSuccess())
	{
		getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Failed to parse Amber test "
			<< readFilename
			<< ": "
			<< r.Error()
			<< "\n"
			<< tcu::TestLog::EndMessage;
		// TODO(dneto): Enhance Amber to not require this.
		m_recipe->SetImpl(DE_NULL);
		return false;
	}
	return true;
}

void AmberTestCase::initPrograms(vk::SourceCollections& programCollection) const
{
	std::vector<amber::ShaderInfo> shaders = m_recipe->GetShaderInfo();
	for (size_t i = 0; i < shaders.size(); ++i)
	{
		const amber::ShaderInfo& shader = shaders[i];

		/* Hex encoded shaders do not need to be pre-compiled */
		if (shader.format == amber::kShaderFormatSpirvHex)
			continue;

		if (shader.format == amber::kShaderFormatSpirvAsm)
		{
			programCollection.spirvAsmSources.add(shader.shader_name) << shader.shader_source;
		}
		else if (shader.format == amber::kShaderFormatGlsl)
		{
			switch (shader.type)
			{
				case amber::kShaderTypeCompute:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::ComputeSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, 0u);
					break;
				case amber::kShaderTypeGeometry:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::GeometrySource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, 0u);
					break;
				case amber::kShaderTypeFragment:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::FragmentSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, 0u);
					break;
				case amber::kShaderTypeVertex:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::VertexSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, 0u);
					break;
				case amber::kShaderTypeTessellationControl:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::TessellationControlSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, 0u);
					break;
				case amber::kShaderTypeTessellationEvaluation:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::TessellationEvaluationSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, 0u);
					break;
				case amber::kShaderTypeMulti:
					DE_ASSERT(false && "Multi shaders not supported");
					break;
			}
		}
		else
		{
			DE_ASSERT(false && "Shader format not supported");
		}
	}
}

tcu::TestStatus AmberTestInstance::iterate (void)
{
	amber::ShaderMap shaderMap;

	std::vector<amber::ShaderInfo> shaders = m_recipe->GetShaderInfo();
	for (size_t i = 0; i < shaders.size(); ++i)
	{
		const amber::ShaderInfo& shader = shaders[i];

		if (!m_context.getBinaryCollection().contains(shader.shader_name))
			continue;

		size_t len = m_context.getBinaryCollection().get(shader.shader_name).getSize();
		/* This is a compiled spir-v binary which must be made of 4-byte words. We
		 * are moving into a word sized vector so divide by 4
		 */
		std::vector<deUint32> data;
		data.resize(len >> 2);
		deMemcpy(data.data(), m_context.getBinaryCollection().get(shader.shader_name).getBinary(), len);

		shaderMap[shader.shader_name] = data;
	}

	amber::EngineConfig*	vkConfig = GetVulkanConfig(m_context.getInstance(),
			m_context.getPhysicalDevice(), m_context.getDevice(), &m_context.getDeviceFeatures(),
			&m_context.getDeviceFeatures2(), m_context.getInstanceExtensions(),
			m_context.getDeviceExtensions(), m_context.getUniversalQueueFamilyIndex(),
			m_context.getUniversalQueue(), m_context.getInstanceProcAddr());

	amber::Amber						am;
	amber::Options						amber_options;
	amber_options.engine				= amber::kEngineTypeVulkan;
	amber_options.config				= vkConfig;
	amber_options.delegate				= DE_NULL;
	amber_options.pipeline_create_only	= false;

	amber::Result r = am.ExecuteWithShaderData(m_recipe, &amber_options, shaderMap);
	if (!r.IsSuccess()) {
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< r.Error()
			<< "\n"
			<< tcu::TestLog::EndMessage;
	}

	delete vkConfig;

	return r.IsSuccess() ? tcu::TestStatus::pass("Pass") :tcu::TestStatus::fail("Fail");
}

} // cts_amber
} // vkt
