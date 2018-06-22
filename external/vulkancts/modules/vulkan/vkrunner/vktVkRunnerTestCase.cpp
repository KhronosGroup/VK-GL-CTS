/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Intel Corporation
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
 * \brief Functional tests using vkrunner
 *//*--------------------------------------------------------------------*/

#include <assert.h>
#include <vkrunner/vkrunner.h>

#include "vktVkRunnerTestCase.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace vkrunner
{

static const char *
vr_stage_name[VR_SHADER_STAGE_N_STAGES] = {
	"vertex",
	"tess_ctrl",
	"tess_eval",
	"geometry",
	"fragment",
	"compute",
};

static void errorCb(const char* message,
					void* user_data)
{
	VkRunnerTestCase* test = (VkRunnerTestCase*) user_data;

	test->getTestContext().getLog()
		<< tcu::TestLog::Message
		<< message
		<< "\n"
		<< tcu::TestLog::EndMessage;
}

VkRunnerTestCase::VkRunnerTestCase (tcu::TestContext&	testCtx,
									const char*		categoryname,
									const char*		filename,
									const char*		name,
									const char*		description)
	: TestCase(testCtx, name, description)
{
	m_testCaseData.categoryname = categoryname;
	m_testCaseData.filename = filename;
	m_testCaseData.num_shaders = 0;
	m_testCaseData.script = DE_NULL;
	m_testCaseData.shaders = DE_NULL;

	std::string readFilename("vulkan/vkrunner/");
	readFilename.append(m_testCaseData.categoryname);
	readFilename.append("/");
	readFilename.append(m_testCaseData.filename);
	m_testCaseData.source = vr_source_from_file(readFilename.c_str());
}

VkRunnerTestCase::~VkRunnerTestCase (void)
{
	if (m_testCaseData.num_shaders)
	{
		for (int i = 0; i < m_testCaseData.num_shaders; i++)
		{
			/* shaders[i]->source were allocated by VkRunner. We don't need them anymore. */
			deFree(m_testCaseData.shaders[i].source);
		}
		deFree(m_testCaseData.shaders);
	}
	if (m_testCaseData.script)
		vr_script_free(m_testCaseData.script);
	if (m_testCaseData.source)
		vr_source_free(m_testCaseData.source);
}

void VkRunnerTestCase::addTokenReplacement(const char *token,
									  const char *replacement)
{
	vr_source_add_token_replacement(m_testCaseData.source,
									token,
									replacement);
}

bool VkRunnerTestCase::getShaders()
{
	/* Create a temporary vr_config to log shader_test parsing errors to test's log file */
	struct vr_config *config = vr_config_new();
	vr_config_set_user_data(config, this);
	vr_config_set_error_cb(config, errorCb);
	m_testCaseData.script = vr_script_load(config, m_testCaseData.source);

	if (m_testCaseData.script == DE_NULL)
	{
		/* Parser returned an error or shader_test file doesn't exist */
		vr_config_free(config);
		return false;
	}

	m_testCaseData.num_shaders = vr_script_get_num_shaders(m_testCaseData.script);
	m_testCaseData.shaders = (struct vr_script_shader_code *)
		malloc(sizeof(struct vr_script_shader_code)*m_testCaseData.num_shaders);
	vr_script_get_shaders(m_testCaseData.script,
							m_testCaseData.source,
							m_testCaseData.shaders);
	vr_config_free(config);
	return true;
}

TestInstance* VkRunnerTestCase::createInstance(Context& ctx) const
{
	return new VkRunnerTestInstance(ctx, m_testCaseData);
}

void VkRunnerTestCase::initPrograms(vk::SourceCollections& programCollection) const
{
	int num_shader[VR_SHADER_STAGE_N_STAGES] = {0};

	for (int i = 0; i < m_testCaseData.num_shaders; i++)
	{
		num_shader[m_testCaseData.shaders[i].stage]++;
		if (num_shader[m_testCaseData.shaders[i].stage] > 1)
			TCU_THROW(InternalError, "Multiple shaders per stage are not currently supported");

		/* We ignore the SPIR-V shaders in binary form */
		if (m_testCaseData.shaders[i].source_type == VR_SCRIPT_SOURCE_TYPE_GLSL)
		{
			switch (m_testCaseData.shaders[i].stage)
			{
			case VR_SHADER_STAGE_VERTEX:
				programCollection.glslSources.add(vr_stage_name[m_testCaseData.shaders[i].stage]) << glu::VertexSource(m_testCaseData.shaders[i].source);
				break;
			case VR_SHADER_STAGE_TESS_CTRL:
				programCollection.glslSources.add(vr_stage_name[m_testCaseData.shaders[i].stage]) << glu::TessellationControlSource(m_testCaseData.shaders[i].source);
				break;
			case VR_SHADER_STAGE_TESS_EVAL:
				programCollection.glslSources.add(vr_stage_name[m_testCaseData.shaders[i].stage]) << glu::TessellationEvaluationSource(m_testCaseData.shaders[i].source);
				break;
			case VR_SHADER_STAGE_GEOMETRY:
				programCollection.glslSources.add(vr_stage_name[m_testCaseData.shaders[i].stage]) << glu::GeometrySource(m_testCaseData.shaders[i].source);
				break;
			case VR_SHADER_STAGE_FRAGMENT:
				programCollection.glslSources.add(vr_stage_name[m_testCaseData.shaders[i].stage]) << glu::FragmentSource(m_testCaseData.shaders[i].source);
				break;
			case VR_SHADER_STAGE_COMPUTE:
				programCollection.glslSources.add(vr_stage_name[m_testCaseData.shaders[i].stage]) << glu::ComputeSource(m_testCaseData.shaders[i].source);
				break;
			default:
				assert(0 && "Shader type is not supported");
			}
		} else if (m_testCaseData.shaders[i].source_type == VR_SCRIPT_SOURCE_TYPE_SPIRV)
		{
			programCollection.spirvAsmSources.add(vr_stage_name[m_testCaseData.shaders[i].stage]) << m_testCaseData.shaders[i].source;
		}
	}
}

tcu::TestStatus VkRunnerTestInstance::iterate (void)
{
	/* Get the compiled version of the text-based shaders and replace them */
	for (int stage = 0; stage < VR_SHADER_STAGE_N_STAGES; stage++)
	{
		std::string name(vr_stage_name[stage]);
		if (!m_context.getBinaryCollection().contains(name))
			continue;
		size_t source_length = m_context.getBinaryCollection().get(name).getSize();
		unsigned *source = (unsigned *) deMalloc(source_length);
		deMemcpy(source,
			   m_context.getBinaryCollection().get(name).getBinary(),
			   source_length);
		vr_script_replace_shaders_stage_binary(m_testCaseData.script,
											   (enum vr_shader_stage)stage,
											   source_length,
											   source);
		deFree(source);
	}

	/* Replace text-based shaders by their binary equivalent and execute the test */
	vr_result res = vr_executor_execute_script(m_context.getExecutor(), m_testCaseData.script);

	switch (res)
	{
	case VR_RESULT_FAIL:
		return tcu::TestStatus::fail("Fail");
	case VR_RESULT_PASS:
		return tcu::TestStatus::pass("Pass");
	case VR_RESULT_SKIP:
		return tcu::TestStatus::incomplete();
	}

	return tcu::TestStatus::fail("Fail");
}

} // vkrunner
} // vkt
