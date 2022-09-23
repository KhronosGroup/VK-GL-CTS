#ifndef _ESEXTCFRAGMENTSHADINGRATEBASIC_HPP
#define _ESEXTCFRAGMENTSHADINGRATEBASIC_HPP
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
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/*!
 * \file  esextcesextcFragmentShadingRateBasic.hpp
 * \brief FragmentShadingRateEXT basic render test
 */ /*-------------------------------------------------------------------*/

#include "../esextcTestCaseBase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"

#include <vector>

namespace glu
{
class ShaderProgram;
}

namespace glcts
{
class FragmentShadingRateBasic : public TestCaseBase
{
	struct TestcaseParam
	{
		deUint32 seed	= 0;
		deUint32 width	= 0;
		deUint32 height = 0;
	};

public:
	FragmentShadingRateBasic(Context& context, const ExtParameters& extParams, const char* name,
							 const char* description);

	~FragmentShadingRateBasic() override
	{
	}

	void		  init(void) override;
	void		  deinit(void) override;
	IterateResult iterate(void) override;

private:
	std::string genVS() const;
	std::string genFS() const;
	glw::GLenum translateDrawIDToShadingRate(deUint32 drawID) const;

	void		   setupTest(void);
	glw::GLboolean verifyError(const glw::GLenum expected_error, const char* description) const;

private:
	TestcaseParam			 m_tcParam;
	glu::ShaderProgram*		 m_program;
	glw::GLuint				 m_to_id;
	glw::GLuint				 m_fbo_id;
	glw::GLuint				 m_vbo_id;
	std::vector<glw::GLenum> m_availableShadingRates;
};

} // namespace glcts

#endif // _ESEXTCFRAGMENTSHADINGRATEBASIC_HPP
