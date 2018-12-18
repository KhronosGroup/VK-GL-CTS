#ifndef _VKTVKRUNNERTESTCASE_HPP
#define _VKTVKRUNNERTESTCASE_HPP
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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"

struct vr_source;
struct vr_script;
struct vr_script_shader_code;

namespace vkt
{
namespace vkrunner
{

struct TestCaseData
{
	std::string						categoryname;
	std::string						filename;
	int								num_shaders;
	struct vr_source				*source;
	struct vr_script				*script;
	struct vr_script_shader_code	*shaders;
};

class VkRunnerTestInstance : public TestInstance
{
public:
	VkRunnerTestInstance (Context& context, const TestCaseData& testCaseData)
		: TestInstance(context),
		  m_testCaseData(testCaseData)
	{
	}

	virtual tcu::TestStatus iterate (void);

private:
	TestCaseData m_testCaseData;
};

class VkRunnerTestCase : public TestCase
{
public:
	VkRunnerTestCase (tcu::TestContext&	testCtx,
					  const char*		categoryname,
					  const char*		filename,
					  const char*		name,
					  const char*		description);

	~VkRunnerTestCase (void);

	void addTokenReplacement(const char *token,
							 const char *replacement);

	bool getShaders();

	virtual TestInstance* createInstance (Context& ctx) const;

	void initPrograms(vk::SourceCollections& programCollection) const;

private:
	TestCaseData m_testCaseData;
};

} // vkrunner
} // vkt

#endif // _VKTVKRUNNERTESTCASE_HPP
