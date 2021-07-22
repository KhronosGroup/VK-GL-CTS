#ifndef _ESEXTCDISJOINTTIMERQUERYTESTS_HPP
#define _ESEXTCDISJOINTTIMERQUERYTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google LLC
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/*!
 * \file  esextcDisjointTimerQueryTests.hpp
 * \brief Base test group for disjoint timer query tests
 */ /*-------------------------------------------------------------------*/

#include "../esextcTestCaseBase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"

namespace glcts
{

/* Base test group for texture buffer tests */
class DisjointTimerQueryTests : public TestCaseGroupBase
{
public:
	DisjointTimerQueryTests				(glcts::Context& context, const ExtParameters& extParams);

	virtual ~DisjointTimerQueryTests	(void)
	{
	}

	void init (void);

private:
	DisjointTimerQueryTests				(const DisjointTimerQueryTests& other);
	DisjointTimerQueryTests& operator=	(const DisjointTimerQueryTests& other);
};

} // namespace glcts

#endif // _ESEXTCDISJOINTTIMERQUERYTESTS_HPP
