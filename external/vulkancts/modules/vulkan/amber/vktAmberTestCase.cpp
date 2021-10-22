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
#include "amber/recipe.h"

#include <iostream>

#include "deDefs.hpp"
#include "deUniquePtr.hpp"
#include "deFilePath.hpp"
#include "vktTestCaseUtil.hpp"
#include "tcuTestLog.hpp"
#include "vktAmberTestCase.hpp"
#include "vktAmberHelper.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "vkSpirVProgram.hpp"
#include "vkImageUtil.hpp"

namespace vkt
{
namespace cts_amber
{

AmberTestCase::AmberTestCase (tcu::TestContext&		testCtx,
							  const char*			name,
							  const char*			description,
							  const std::string&	readFilename)
	: TestCase(testCtx, name, description),
	  m_recipe(DE_NULL),
	  m_readFilename(readFilename)
{
}

AmberTestCase::~AmberTestCase (void)
{
	delete m_recipe;
}

TestInstance* AmberTestCase::createInstance (Context& ctx) const
{
	return new AmberTestInstance(ctx, m_recipe);
}

static amber::EngineConfig* createEngineConfig (Context& ctx)
{
	amber::EngineConfig* vkConfig = GetVulkanConfig(ctx.getInstance(),
			ctx.getPhysicalDevice(), ctx.getDevice(), &ctx.getDeviceFeatures(),
			&ctx.getDeviceFeatures2(), ctx.getInstanceExtensions(),
			ctx.getDeviceExtensions(), ctx.getUniversalQueueFamilyIndex(),
			ctx.getUniversalQueue(), ctx.getInstanceProcAddr());

	return vkConfig;
}

// Returns true if the given feature is supported by the device.
// Throws an internal error If the feature is not recognized at all.
static bool isFeatureSupported(const vkt::Context& ctx, const std::string& feature)
{
	if (feature == "Features.shaderInt16")
		return ctx.getDeviceFeatures().shaderInt16;
	if (feature == "Features.shaderInt64")
		return ctx.getDeviceFeatures().shaderInt64;
	if (feature == "Features.tessellationShader")
		return ctx.getDeviceFeatures().tessellationShader;
	if (feature == "Features.geometryShader")
		return ctx.getDeviceFeatures().geometryShader;
	if (feature == "Features.fragmentStoresAndAtomics")
		return ctx.getDeviceFeatures().fragmentStoresAndAtomics;
	if (feature == "Features.vertexPipelineStoresAndAtomics")
		return ctx.getDeviceFeatures().vertexPipelineStoresAndAtomics;
	if (feature == "Features.fillModeNonSolid")
		return ctx.getDeviceFeatures().fillModeNonSolid;
	if (feature == "Features.shaderStorageImageMultisample")
		return ctx.getDeviceFeatures().shaderStorageImageMultisample;
	if (feature == "VariablePointerFeatures.variablePointersStorageBuffer")
		return ctx.getVariablePointersFeatures().variablePointersStorageBuffer;
	if (feature == "VariablePointerFeatures.variablePointers")
		return ctx.getVariablePointersFeatures().variablePointers;
	if (feature == "SubgroupProperties.supportedStages.fragment")
		return (ctx.getSubgroupProperties().supportedStages & vk::VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
	if (feature == "SubgroupProperties.supportedOperations.vote")
		return (ctx.getSubgroupProperties().supportedOperations & vk::VK_SUBGROUP_FEATURE_VOTE_BIT) != 0;
	if (feature == "SubgroupProperties.supportedOperations.ballot")
		return (ctx.getSubgroupProperties().supportedOperations & vk::VK_SUBGROUP_FEATURE_BALLOT_BIT) != 0;

	std::string message = std::string("Unexpected feature name: ") + feature;
	TCU_THROW(InternalError, message.c_str());
}

void AmberTestCase::delayedInit(void)
{
	// Make sure the input can be parsed before we use it.
	if (!parse(m_readFilename))
	{
		std::string message = "Failed to parse Amber file: " + m_readFilename;
		TCU_THROW(InternalError, message.c_str());
	}
}

void AmberTestCase::checkSupport(Context& ctx) const
{
	// Check for instance and device extensions as declared by the test code.
	if (m_required_extensions.size())
	{
		std::set<std::string> device_extensions(ctx.getDeviceExtensions().begin(),
												ctx.getDeviceExtensions().end());
		std::set<std::string> instance_extensions(ctx.getInstanceExtensions().begin(),
												  ctx.getInstanceExtensions().end());
		std::string missing;
		for (std::set<std::string>::iterator iter = m_required_extensions.begin();
			 iter != m_required_extensions.end();
			 ++iter)
		{
			const std::string extension = *iter;
			if ((device_extensions.count(extension) == 0) &&
				(instance_extensions.count(extension) == 0))
			{
				missing += " " + extension;
			}
		}
		if (missing.size() > 0)
		{
			std::string message("Test requires unsupported extensions:");
			message += missing;
			TCU_THROW(NotSupportedError, message.c_str());
		}
	}

	// Check for required features.  Do this after extensions are checked because
	// some feature checks are only valid when corresponding extensions are enabled.
	if (m_required_features.size())
	{
		std::string missing;
		for (std::set<std::string>::iterator iter = m_required_features.begin();
			 iter != m_required_features.end();
			 ++iter)
		{
			const std::string feature = *iter;
			if (!isFeatureSupported(ctx, feature))
			{
				missing += " " + feature;
			}
		}
		if (missing.size() > 0)
		{
			std::string message("Test requires unsupported features:");
			message += missing;
			TCU_THROW(NotSupportedError, message.c_str());
		}
	}

	for (auto req : m_imageRequirements)
		checkImageSupport(ctx.getInstanceInterface(), ctx.getPhysicalDevice(), req);

	for (auto req : m_bufferRequirements)
	{
		vk::VkFormatProperties prop;
		ctx.getInstanceInterface().getPhysicalDeviceFormatProperties(ctx.getPhysicalDevice(), req.m_format, &prop);

		if ((req.m_featureFlags & prop.bufferFeatures) != req.m_featureFlags)
		{
			TCU_THROW(NotSupportedError, "Buffer format doesn't support required feature flags");
		}
	}

	// when checkSupport is called script is not yet parsed so we need to determine
	// unsupported tests by name ; in AmberTestCase we do not have access to actual
	// m_recipe implementation - we can't scan it to see if test can be executed;
	// alternatively portability extension and its features could be checked in amber.cc
	if (ctx.isDeviceFunctionalitySupported("VK_KHR_portability_subset"))
	{
		if (m_name == "triangle_fan" && !ctx.getPortabilitySubsetFeatures().triangleFans)
			TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");

		if (ctx.getPortabilitySubsetProperties().minVertexInputBindingStrideAlignment == 4)
		{
			const std::set<std::string> casesToSkip
			{
				"line-strip",
				"polygon-mode-lines",
				"r8g8-uint-highp",
				"r8g8-uint-highp-output-uint",
				"r8g8-uint-mediump",
				"r8g8-uint-mediump-output-uint",
				"inputs-outputs-mod",
			};

			if (casesToSkip.count(m_name))
				TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Stride is not multiply of minVertexInputBindingStrideAlignment");
		}
	}
}

class Delegate : public amber::Delegate
{
public:
					Delegate				(tcu::TestContext& testCtx);

	amber::Result	LoadBufferData			(const std::string			file_name,
											 amber::BufferDataFileType	file_type,
											 amber::BufferInfo*			buffer) const override;

	void			Log						(const std::string& /*message*/) override	{ DE_FATAL("amber::Delegate::Log unimplemented"); }
	bool			LogGraphicsCalls		(void) const override						{ return m_logGraphicsCalls; }
	void			SetLogGraphicsCalls		(bool log_graphics_calls)					{ m_logGraphicsCalls = log_graphics_calls; }
	bool			LogExecuteCalls			(void) const override						{ return m_logExecuteCalls; }
	void			SetLogExecuteCalls		(bool log_execute_calls)					{ m_logExecuteCalls = log_execute_calls; }
	bool			LogGraphicsCallsTime	(void) const override						{ return m_logGraphicsCallsTime; }
	void			SetLogGraphicsCallsTime	(bool log_graphics_calls_time)				{ m_logGraphicsCallsTime = log_graphics_calls_time; }
	deUint64		GetTimestampNs			(void) const override						{ DE_FATAL("amber::Delegate::GetTimestampNs unimplemented"); return 0; }
	void			SetScriptPath			(std::string path)							{ m_path = path; }

private:
	tcu::TestContext&	m_testCtx;
	std::string			m_path;
	bool				m_logGraphicsCalls;
	bool				m_logGraphicsCallsTime;
	bool				m_logExecuteCalls;
};

Delegate::Delegate (tcu::TestContext& testCtx)
	: m_testCtx					(testCtx)
	, m_path					("")
	, m_logGraphicsCalls		(false)
	, m_logGraphicsCallsTime	(false)
	, m_logExecuteCalls			(false)
{
}

amber::Result Delegate::LoadBufferData (const std::string			file_name,
										amber::BufferDataFileType	file_type,
										amber::BufferInfo*			buffer) const
{
	const tcu::Archive&				archive		= m_testCtx.getArchive();
	const de::FilePath				filePath	= de::FilePath(m_path).join(file_name);
	de::UniquePtr<tcu::Resource>	file		(archive.getResource(filePath.getPath()));
	int								numBytes	= file->getSize();
	std::vector<deUint8>			bytes		(numBytes);

	if (file_type == amber::BufferDataFileType::kPng)
		return amber::Result("Amber PNG loading unimplemented");

	file->read(bytes.data(), numBytes);

	if (bytes.empty())
		return amber::Result("Failed to load buffer data " + file_name);

	for (deUint8 byte : bytes)
	{
		amber::Value value;
		value.SetIntValue(static_cast<deUint64>(byte));
		buffer->values.push_back(value);
	}

	buffer->width	= 1;
	buffer->height	= 1;

	return {};
}

bool AmberTestCase::parse (const std::string& readFilename)
{
	std::string script = ShaderSourceProvider::getSource(m_testCtx.getArchive(), readFilename.c_str());
	if (script.empty())
		return false;

	Delegate delegate (m_testCtx);
	delegate.SetScriptPath(de::FilePath(readFilename).getDirName());

	m_recipe = new amber::Recipe();

	amber::Amber am (&delegate);
	amber::Result r = am.Parse(script, m_recipe);

	m_recipe->SetFenceTimeout(~0u); // infinity of miliseconds

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

void AmberTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::vector<amber::ShaderInfo> shaders = m_recipe->GetShaderInfo();
	for (size_t i = 0; i < shaders.size(); ++i)
	{
		const amber::ShaderInfo& shader = shaders[i];

		vk::SpirvVersion spirvVersion = vk::SPIRV_VERSION_1_0;
		DE_STATIC_ASSERT(vk::SPIRV_VERSION_LAST == vk::SPIRV_VERSION_1_5 + 1);
		if (shader.target_env == "spv1.5")
			spirvVersion = vk::SPIRV_VERSION_1_5;
		else if (shader.target_env == "spv1.4")
			spirvVersion = vk::SPIRV_VERSION_1_4;
		else if (shader.target_env == "spv1.3")
			spirvVersion = vk::SPIRV_VERSION_1_3;
		else if (shader.target_env == "spv1.2")
			spirvVersion = vk::SPIRV_VERSION_1_2;
		else if (shader.target_env == "spv1.1")
			spirvVersion = vk::SPIRV_VERSION_1_1;

		/* Hex encoded shaders do not need to be pre-compiled */
		if (shader.format == amber::kShaderFormatSpirvHex)
			continue;

		if (shader.format == amber::kShaderFormatSpirvAsm)
		{
			programCollection.spirvAsmSources.add(shader.shader_name) << shader.shader_source << m_asm_options;
		}
		else if (shader.format == amber::kShaderFormatGlsl)
		{
			bool allowSpirv14 = (spirvVersion == vk::SPIRV_VERSION_1_4);

			switch (shader.type)
			{
				case amber::kShaderTypeCompute:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::ComputeSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, 0u, allowSpirv14);
					break;
				case amber::kShaderTypeGeometry:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::GeometrySource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, 0u, allowSpirv14);
					break;
				case amber::kShaderTypeFragment:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::FragmentSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, 0u, allowSpirv14);
					break;
				case amber::kShaderTypeVertex:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::VertexSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, 0u, allowSpirv14);
					break;
				case amber::kShaderTypeTessellationControl:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::TessellationControlSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, 0u, allowSpirv14);
					break;
				case amber::kShaderTypeTessellationEvaluation:
					programCollection.glslSources.add(shader.shader_name)
						<< glu::TessellationEvaluationSource(shader.shader_source)
						<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, 0u, allowSpirv14);
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
	amber::Amber		am (DE_NULL);
	amber::Options		amber_options;
	amber::ShaderMap	shaderMap;
	amber::Result		r;

	amber_options.engine			= amber::kEngineTypeVulkan;
	amber_options.config			= createEngineConfig(m_context);
	amber_options.execution_type	= amber::ExecutionType::kExecute;

	// Check for extensions as declared by the Amber script itself.  Throw an internal
	// error if that's more demanding.
	r = am.AreAllRequirementsSupported(m_recipe, &amber_options);
	if (!r.IsSuccess())
	{
		// dEQP does not to rely on external code to determine whether
		// a test is supported.  So throw an internal error here instead
		// of a NotSupportedError.  If an Amber test is not supported, then
		// you must override this method and throw a NotSupported exception
		// before reach here.
		TCU_THROW(InternalError, r.Error().c_str());
	}

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

	r = am.ExecuteWithShaderData(m_recipe, &amber_options, shaderMap);
	if (!r.IsSuccess()) {
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< r.Error()
			<< "\n"
			<< tcu::TestLog::EndMessage;
	}

	delete amber_options.config;

	return r.IsSuccess() ? tcu::TestStatus::pass("Pass") :tcu::TestStatus::fail("Fail");
}

void AmberTestCase::setSpirVAsmBuildOptions (const vk::SpirVAsmBuildOptions& asm_options)
{
	m_asm_options = asm_options;
}

void AmberTestCase::addRequirement (const std::string& requirement)
{
	if (requirement.find(".") != std::string::npos)
		m_required_features.insert(requirement);
	else
		m_required_extensions.insert(requirement);
}

void AmberTestCase::addImageRequirement (vk::VkImageCreateInfo info)
{
	m_imageRequirements.push_back(info);
}

void AmberTestCase::addBufferRequirement (BufferRequirement req)
{
	m_bufferRequirements.push_back(req);
}

} // cts_amber
} // vkt
