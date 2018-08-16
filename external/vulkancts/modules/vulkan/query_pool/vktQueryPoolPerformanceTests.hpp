#ifndef _VKTQUERYPOOLPERFORMANCETESTS_HPP
#define _VKTQUERYPOOLPERFORMANCETESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Vulkan Performance Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

namespace vkt
{
namespace QueryPool
{

class QueryPoolPerformanceTests : public tcu::TestCaseGroup
{
public:
	QueryPoolPerformanceTests	(tcu::TestContext &testCtx);
	~QueryPoolPerformanceTests	(void)	{}
	void init					(void);

private:
	QueryPoolPerformanceTests				(const QueryPoolPerformanceTests &other);
	QueryPoolPerformanceTests&	operator=	(const QueryPoolPerformanceTests &other);
};

} // QueryPool
} // vkt

#endif // _VKTQUERYPOOLPERFORMANCETESTS_HPP
