#ifndef _ESEXTCDISJOINTTIMERQUERYHANDLEREUSE_HPP
#define _ESEXTCDISJOINTTIMERQUERYHANDLEREUSE_HPP
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
 * \file  esextcDisjointTimerQueryHandleReuse.hpp
 * \brief Timer query handle reuse tests
 */ /*-------------------------------------------------------------------*/

#include "../esextcTestCaseBase.hpp"
#include "esextcDisjointTimerQueryBase.hpp"

namespace glcts
{

class DisjointTimerQueryHandleReuse : public DisjointTimerQueryBase
{
public:
	DisjointTimerQueryHandleReuse			(Context& context, const ExtParameters& extParams, const char* name,
											 const char* description);

	virtual ~DisjointTimerQueryHandleReuse	()
	{
	}

	virtual IterateResult iterate(void);

private:
	void		initTest(void);
};

} // namespace glcts

#endif // _ESEXTCDISJOINTTIMERQUERYHANDLEREUSE_HPP
