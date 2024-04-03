#ifndef _VKTQUERYPOOLSTATISTICSTESTS_HPP
#define _VKTQUERYPOOLSTATISTICSTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Vulkan Statistics Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

namespace vkt
{
namespace QueryPool
{

class QueryPoolStatisticsTests : public tcu::TestCaseGroup
{
public:
	QueryPoolStatisticsTests	(tcu::TestContext &testCtx);
	void init					(void) override;
	void deinit					(void) override;

private:
	QueryPoolStatisticsTests				(const QueryPoolStatisticsTests &other);
	QueryPoolStatisticsTests&	operator=	(const QueryPoolStatisticsTests &other);
};

} // QueryPool
} // vkt

#endif // _VKTQUERYPOOLSTATISTICSTESTS_HPP
