/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Color clear tests.
 *//*--------------------------------------------------------------------*/

#include "teglColorClearTests.hpp"
#include "teglColorClearCase.hpp"

#include <EGL/eglext.h>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#	define EGL_OPENGL_ES3_BIT_KHR	0x0040
#endif
#if !defined(EGL_CONTEXT_MAJOR_VERSION_KHR)
#	define EGL_CONTEXT_MAJOR_VERSION_KHR EGL_CONTEXT_CLIENT_VERSION
#endif

using std::string;
using std::vector;

namespace deqp
{
namespace egl
{

ColorClearTests::ColorClearTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "color_clears", "Color clears with different client APIs")
{
}

ColorClearTests::~ColorClearTests (void)
{
}

struct ColorClearGroupSpec
{
	const char*		name;
	const char*		desc;
	EGLint			apiBits;
	int				numContextsPerApi;
};

template <class ClearClass>
static void createColorClearGroups (EglTestContext& eglTestCtx, tcu::TestCaseGroup* group, const ColorClearGroupSpec* first, const ColorClearGroupSpec* last)
{
	for (const ColorClearGroupSpec* groupIter = first; groupIter != last; groupIter++)
	{
		tcu::TestCaseGroup* configGroup = new tcu::TestCaseGroup(eglTestCtx.getTestContext(), groupIter->name, groupIter->desc);
		group->addChild(configGroup);

		vector<RenderConfigIdSet>	configSets;
		eglu::FilterList			filters;
		filters << (eglu::ConfigRenderableType() & groupIter->apiBits);
		getDefaultRenderConfigIdSets(configSets, eglTestCtx.getConfigs(), filters);

		for (vector<RenderConfigIdSet>::const_iterator setIter = configSets.begin(); setIter != configSets.end(); setIter++)
			configGroup->addChild(new ClearClass(eglTestCtx, setIter->getName(), "", groupIter->apiBits, setIter->getSurfaceTypeMask(), setIter->getConfigIds(), groupIter->numContextsPerApi));
	}
}

void ColorClearTests::init (void)
{
	static const ColorClearGroupSpec singleContextCases[] =
	{
		{ "gles1",			"Color clears using GLES1",											EGL_OPENGL_ES_BIT,										1 },
		{ "gles2",			"Color clears using GLES2",											EGL_OPENGL_ES2_BIT,										1 },
		{ "gles3",			"Color clears using GLES3",											EGL_OPENGL_ES3_BIT_KHR,									1 },
		{ "vg",				"Color clears using OpenVG",										EGL_OPENVG_BIT,											1 }
	};

	static const ColorClearGroupSpec multiContextCases[] =
	{
		{ "gles1",				"Color clears using multiple GLES1 contexts to shared surface",		EGL_OPENGL_ES_BIT,												3 },
		{ "gles2",				"Color clears using multiple GLES2 contexts to shared surface",		EGL_OPENGL_ES2_BIT,												3 },
		{ "gles3",				"Color clears using multiple GLES3 contexts to shared surface",		EGL_OPENGL_ES3_BIT_KHR,											3 },
		{ "vg",					"Color clears using multiple OpenVG contexts to shared surface",	EGL_OPENVG_BIT,													3 },
		{ "gles1_gles2",		"Color clears using multiple APIs to shared surface",				EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT,							1 },
		{ "gles1_gles2_gles3",	"Color clears using multiple APIs to shared surface",				EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT|EGL_OPENGL_ES3_BIT_KHR,	1 },
		{ "gles1_vg",			"Color clears using multiple APIs to shared surface",				EGL_OPENGL_ES_BIT|EGL_OPENVG_BIT,								1 },
		{ "gles2_vg",			"Color clears using multiple APIs to shared surface",				EGL_OPENGL_ES2_BIT|EGL_OPENVG_BIT,								1 },
		{ "gles3_vg",			"Color clears using multiple APIs to shared surface",				EGL_OPENGL_ES3_BIT_KHR|EGL_OPENVG_BIT,							1 },
		{ "gles1_gles2_vg",		"Color clears using multiple APIs to shared surface",				EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT|EGL_OPENVG_BIT,			1 }
	};

	tcu::TestCaseGroup* singleContextGroup = new tcu::TestCaseGroup(m_testCtx, "single_context", "Single-context color clears");
	addChild(singleContextGroup);
	createColorClearGroups<SingleThreadColorClearCase>(m_eglTestCtx, singleContextGroup, &singleContextCases[0], &singleContextCases[DE_LENGTH_OF_ARRAY(singleContextCases)]);

	tcu::TestCaseGroup* multiContextGroup = new tcu::TestCaseGroup(m_testCtx, "multi_context", "Multi-context color clears with shared surface");
	addChild(multiContextGroup);
	createColorClearGroups<SingleThreadColorClearCase>(m_eglTestCtx, multiContextGroup, &multiContextCases[0], &multiContextCases[DE_LENGTH_OF_ARRAY(multiContextCases)]);

	tcu::TestCaseGroup* multiThreadGroup = new tcu::TestCaseGroup(m_testCtx, "multi_thread", "Multi-thread color clears with shared surface");
	addChild(multiThreadGroup);
	createColorClearGroups<MultiThreadColorClearCase>(m_eglTestCtx, multiThreadGroup, &multiContextCases[0], &multiContextCases[DE_LENGTH_OF_ARRAY(multiContextCases)]);
}

} // egl
} // deqp
