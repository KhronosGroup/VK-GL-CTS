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

#include "amber/recipe.h"
#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"

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
					 const char*		description);

	virtual ~AmberTestCase (void);

	virtual TestInstance* createInstance (Context& ctx) const;

	bool parse(const char* category, const std::string& filename);
	void initPrograms(vk::SourceCollections& programCollection) const;

private:
	amber::Recipe* m_recipe;
};

} // cts_amber
} // vkt

#endif // _VKTAMBERTESTCASE_HPP
