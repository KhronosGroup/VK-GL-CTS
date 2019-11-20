#ifndef _VKTAMBERTESTCASE_HPP
#define _VKTAMBERTESTCASE_HPP
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

#include <string>
#include <set>
#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vkSpirVProgram.hpp"
#include "vktTestCase.hpp"

namespace amber { class Recipe; }

namespace vkt
{
namespace cts_amber
{

class AmberTestInstance : public TestInstance
{
public:
	AmberTestInstance	(Context&		context,
						 amber::Recipe*	recipe)
		: TestInstance(context), m_recipe(recipe)
	{
	}

	virtual tcu::TestStatus iterate (void);

private:
  amber::Recipe* m_recipe;
};

class AmberTestCase : public TestCase
{
public:
	AmberTestCase	(tcu::TestContext&	testCtx,
					 const char*		name,
					 const char*		description,
					 const std::string&	readFilename);

	virtual ~AmberTestCase (void);

	virtual TestInstance* createInstance (Context& ctx) const;

	// Check that the Vulkan implementation supports this test.
	// We have the principle that client code in dEQP should independently
	// determine if the test should be supported:
	//  - If any of the extensions registered via |addRequirement| is not
	//    supported then throw a NotSupported exception.
	//  - Otherwise, we do a secondary sanity check depending on code inside
	//    Amber itself: if the Amber test says it is not supported, then
	//    throw an internal error exception.
	virtual void checkSupport (Context& ctx) const; // override

	// If the test case uses SPIR-V Assembly, use these build options.
	// Otherwise, defaults to target Vulkan 1.0, SPIR-V 1.0.
	void setSpirVAsmBuildOptions(const vk::SpirVAsmBuildOptions& asm_options);
	virtual void delayedInit (void);
	virtual void initPrograms (vk::SourceCollections& programCollection) const;

	// Add a required instance extension, device extension, or feature bit.
	// A feature bit is represented by a string of form "<structure>.<feature>", where
	// the structure name matches the Vulkan spec, but without the leading "VkPhysicalDevice".
	// An example entry is: "VariablePointerFeatures.variablePointers".
	// An instance or device extension will not have a period in its name.
	void addRequirement(const std::string& requirement);

private:
	bool parse (const std::string& readFilename);

	amber::Recipe* m_recipe;
	vk::SpirVAsmBuildOptions m_asm_options;

	std::string m_readFilename;

	// Instance and device extensions required by the test.
	// We don't differentiate between the two:  We consider the requirement
	// satisfied if the string is registered as either an instance or device
	// extension.  Use a set for consistent ordering.
	std::set<std::string> m_required_extensions;

	// Features required by the test.
	// A feature bit is represented by a string of form "<structure>.<feature>", where
	// the structure name matches the Vulkan spec, but without the leading "VkPhysicalDevice".
	// An example entry is: "VariablePointerFeatures.variablePointers".
	// Use a set for consistent ordering.
	std::set<std::string> m_required_features;
};

AmberTestCase* createAmberTestCase (tcu::TestContext&				testCtx,
									const char*						name,
									const char*						description,
									const char*						category,
									const std::string&				filename,
									const std::vector<std::string>	requirements = std::vector<std::string>());

void createAmberTestsFromIndexFile (tcu::TestContext&	testCtx,
									tcu::TestCaseGroup*	group,
									const std::string	filename,
									const char*			category);

} // cts_amber
} // vkt

#endif // _VKTAMBERTESTCASE_HPP
