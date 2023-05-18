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
 * \file  esextcFragmentShadingRateComplex.cpp
 * \brief Base test group for fragment shading rate combination tests
 */ /*-------------------------------------------------------------------*/

#include "esextcFragmentShadingRateComplex.hpp"
#include "esextcFragmentShadingRateCombinedTests.hpp"
#include "glw.h"

namespace glcts
{

/// Constructor
///
/// @param context       Test context
/// @param extParams   extra parameters
FragmentShadingRateComplex::FragmentShadingRateComplex(glcts::Context& context, const ExtParameters& extParams)
	: TestCaseGroupBase(context, extParams, "complex", "Fragment Shading Rate Complex Tests")
{
}

/// Initializes test cases for fragment shading rate tests
void FragmentShadingRateComplex::init(void)
{
	TestNode::init();

	// on/off combination list
	// 1. ShadingRate API
	// 2. Primitive Shading Rate
	// 3. Attachment Shading Rate
	// 4. op1 Enums, Keep, Replace, Min, Max, Mul
	// 5. op2 Enums, Keep, Replace, Min, Max, Mul
	// 6. MultiSample Enable
	// 7. Framebuffer sizes

	struct BooleanTestParam
	{
		bool		state;
		std::string name;
	};

	struct EnumTestParam
	{
		glw::GLenum state;
		std::string name;
	};

	struct IntTestParam
	{
		int			state;
		std::string name;
	};

	const std::vector<BooleanTestParam> shadingRateAPIs{ { false, "" }, { true, "api_" } };

	const std::vector<BooleanTestParam> shadingRatePrimitives{ { false, "" }, { true, "primitive_" } };

	const std::vector<BooleanTestParam> shadingRateAttachments{ { false, "" }, { true, "attachment_" } };

	const std::vector<EnumTestParam> combineAttachments{ { GL_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_EXT, "keep_" },
														 { GL_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_EXT,
														   "replace_" },
														 { GL_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_EXT, "min_" },
														 { GL_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_EXT, "max_" },
														 { GL_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_EXT, "mul_" } };

	const std::vector<BooleanTestParam> msaas{ { false, "" }, { true, "msaa_" } };

	const std::vector<IntTestParam> sizes{
		{ 6, "6x6" },
		{ 37, "37x37" },
		{ 256, "256x256" },
	};

	for (const BooleanTestParam& shadingRateAPI : shadingRateAPIs)
	{
		for (const BooleanTestParam& shadingRatePrimitive : shadingRatePrimitives)
		{
			for (const BooleanTestParam& shadingRateAttachment : shadingRateAttachments)
			{
				if (!shadingRateAPI.state && !shadingRatePrimitive.state && !shadingRateAttachment.state)
				{
					// fragment shading rate does not use
					continue;
				}

				for (const EnumTestParam& op0 : combineAttachments)
				{
					for (const EnumTestParam& op1 : combineAttachments)
					{
						for (const BooleanTestParam& msaa : msaas)
						{
							for (const IntTestParam& sz : sizes)
							{
								std::string name = shadingRateAPI.name + shadingRatePrimitive.name +
												   shadingRateAttachment.name + op0.name + op1.name + msaa.name +
												   sz.name;
								FragmentShadingRateCombined::TestcaseParam testcaseParam = {
									shadingRateAPI.state,
									shadingRatePrimitive.state,
									shadingRateAttachment.state,
									op0.state,
									op1.state,
									msaa.state,
									sz.state,
								};

								addChild(new FragmentShadingRateCombined(m_context, m_extParams, testcaseParam,
																		 name.c_str(), ""));
							}
						}
					}
				}
			}
		}
	}
}
} // namespace glcts