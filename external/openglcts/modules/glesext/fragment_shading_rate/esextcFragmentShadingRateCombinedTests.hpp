#ifndef _ESEXTCFRAGMENTSHADINGRATECOMBINEDTESTS_HPP
#define _ESEXTCFRAGMENTSHADINGRATECOMBINEDTESTS_HPP
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
 */

/*!
 * \file  esextcesextcFragmentShadingRateCombinedTests.hpp
 * \brief FragmentShadingRateEXT tests for combination of conditions
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
class FragmentShadingRateCombined : public TestCaseBase
{
public:
	struct TestcaseParam
	{
		bool		useShadingRateAPI;
		bool		useShadingRatePrimitive;
		bool		useShadingRateAttachment;
		glw::GLenum combinerOp0;
		glw::GLenum combinerOp1;
		bool		msaa;
		deInt32		framebufferSize;
	};

	struct Extent2D
	{
		deUint32 width;
		deUint32 height;
	};

public:
	FragmentShadingRateCombined(Context& context, const ExtParameters& extParams,
								const FragmentShadingRateCombined::TestcaseParam& testcaseParam, const char* name,
								const char* description);

	virtual ~FragmentShadingRateCombined()
	{
	}

	void		  init(void) override;
	void		  deinit(void) override;
	IterateResult iterate(void) override;

private:
	std::string genVS();
	std::string genFS();
	std::string genCS();
	glw::GLenum translateDrawIDToShadingRate(deUint32 drawID) const;
	glw::GLenum translatePrimIDToShadingRate(deUint32 primID) const;
	glw::GLenum translateCoordsToShadingRate(deUint32 srx, deUint32 sry) const;
	deUint32	getPrimitiveID(deUint32 drawID) const;
	deUint32	simulate(deUint32 drawID, deUint32 primID, deUint32 x, deUint32 y);
	Extent2D	packedShadingRateToExtent(deUint32 packedRate) const;
	deUint32	shadingRateExtentToClampedMask(Extent2D ext, bool allowSwap) const;

	void	 setupTest(void);
	Extent2D combine(Extent2D extent0, Extent2D extent1, glw::GLenum combineOp) const;

private:
	TestcaseParam			 m_tcParam;
	glu::ShaderProgram*		 m_renderProgram;
	glu::ShaderProgram*		 m_computeProgram;
	glw::GLuint				 m_to_id;
	glw::GLuint				 m_sr_to_id;
	glw::GLuint				 m_fbo_id;
	glw::GLuint				 m_vbo_id;
	std::vector<glw::GLenum> m_availableShadingRates;
	glw::GLint				 m_srTexelWidth;
	glw::GLint				 m_srTexelHeight;
	std::vector<deUint32>	 m_simulationCache;
};

} // namespace glcts

#endif // _ESEXTCFRAGMENTSHADINGRATECOMBINEDTESTS_HPP
