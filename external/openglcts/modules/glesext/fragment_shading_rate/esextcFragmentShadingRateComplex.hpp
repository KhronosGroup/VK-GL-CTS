#ifndef _ESEXTCFRAGMENTSHADINGRATECOMPLEX_HPP
#define _ESEXTCFRAGMENTSHADINGRATECOMPLEX_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2022-2022 The Khronos Group Inc.
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
 */

/*!
 * \file  esextcFragmentShadingRateComplex.hpp
 * \brief Base test group for fragment shading rate complex tests
 */ /*-------------------------------------------------------------------*/

#include "../esextcTestCaseBase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"

namespace glcts
{
class FragmentShadingRateComplex : public TestCaseGroupBase
{
public:
	FragmentShadingRateComplex(glcts::Context& context, const ExtParameters& extParams);

	~FragmentShadingRateComplex(void) override
	{
	}

	void init(void) override;

private:
	FragmentShadingRateComplex(const FragmentShadingRateComplex& other);
	FragmentShadingRateComplex& operator=(const FragmentShadingRateComplex& other);
};

} // namespace glcts

#endif // _ESEXTCFRAGMENTSHADINGRATECOMPLEX_HPP
