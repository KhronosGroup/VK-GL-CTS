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
 * \file  esextcFragmentShadingRateRenderTarget.cpp
 * \brief Base test group for fragment shading rate render target tests
 */ /*-------------------------------------------------------------------*/

#include "esextcFragmentShadingRateRenderTarget.hpp"
#include "esextcFragmentShadingRateAttachmentTests.hpp"
#include "glw.h"

namespace glcts
{

/// Constructor
///
/// @param context       Test context
/// @param extParams   extra parameters
FragmentShadingRateRenderTarget::FragmentShadingRateRenderTarget(glcts::Context&	  context,
																 const ExtParameters& extParams)
	: TestCaseGroupBase(context, extParams, "render_target", "Fragment Shading Rate Attachment Tests")
{
}

/// Initializes test cases for fragment shading rate tests
void FragmentShadingRateRenderTarget::init(void)
{
	TestNode::init();

	// Combination or selection list
	// scissor
	// multiLayer
	// multiView
	// attachmentShadingRate
	// multiShadingRate
	// framebufferSize

	// Only one of following option can be enabled. scissor multiLayer, multiView.
	// multiShadingRate can be enabled either cases multiLayer or multiView.
	// scissor test enable only for the single layer case.
	// layerCount is 2 for multLayer or multiView is enabled.

	struct TestKindParam
	{
		FragmentShadingRateAttachment::TestKind state;
		std::string								name;
	};

	struct BooleanTestParam
	{
		bool		state;
		std::string name;
	};

	struct UintTestParam
	{
		deUint32	state;
		std::string name;
	};

	const std::vector<TestKindParam> testKindParams{
		{ FragmentShadingRateAttachment::TestKind::Scissor, "scissor_" },
		{ FragmentShadingRateAttachment::TestKind::MultiView, "multiview_" },
	};

	const std::vector<BooleanTestParam> attachmentShadingRateParams{ { false, "api_" }, { true, "attachment_" } };

	const std::vector<BooleanTestParam> multiShadingRateParams{ { false, "" }, { true, "multishadingratelayer_" } };

	const std::vector<UintTestParam> sizes{
		{ 6, "6x6" },
		{ 37, "37x37" },
		{ 256, "256x256" },
	};

	for (const TestKindParam& testKind : testKindParams)
	{
		for (const BooleanTestParam& attachmentShadingRate : attachmentShadingRateParams)
		{
			for (const BooleanTestParam& multiShadingRate : multiShadingRateParams)
			{
				for (const UintTestParam& sz : sizes)
				{
					const deUint32 layerCount =
						(testKind.state == FragmentShadingRateAttachment::TestKind::MultiView) ? 2 : 1;

					if (multiShadingRate.state && ((layerCount <= 1) || !attachmentShadingRate.state))
					{
						continue;
					}

					std::string name = testKind.name + attachmentShadingRate.name + multiShadingRate.name + sz.name;

					FragmentShadingRateAttachment::TestcaseParam testcaseParam = {
						testKind.state, attachmentShadingRate.state, multiShadingRate.state, sz.state, layerCount
					};

					addChild(
						new FragmentShadingRateAttachment(m_context, m_extParams, testcaseParam, name.c_str(), ""));
				}
			}
		}
	}
}
} // namespace glcts