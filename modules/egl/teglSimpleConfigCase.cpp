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
 * \brief Simple Context construction test.
 *//*--------------------------------------------------------------------*/

#include "teglSimpleConfigCase.hpp"
#include "deStringUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include <map>
#include <set>
#include <algorithm>

namespace deqp
{
namespace egl
{

using std::map;
using std::set;
using std::vector;
using std::string;
using tcu::TestLog;
using eglu::ConfigInfo;

SimpleConfigCase::SimpleConfigCase (EglTestContext& eglTestCtx, const char* name, const char* description, const vector<EGLint>& configIds)
	: TestCase		(eglTestCtx, name, description)
	, m_configIds	(configIds)
{
}

SimpleConfigCase::~SimpleConfigCase (void)
{
}

void SimpleConfigCase::init (void)
{
	const tcu::egl::Display& display = m_eglTestCtx.getDisplay();

	// Log matching configs.
	m_testCtx.getLog() << TestLog::Message << "Matching configs: " << tcu::formatArray(m_configIds.begin(), m_configIds.end()) << TestLog::EndMessage;

	// Config id set.
	set<EGLint> idSet(m_configIds.begin(), m_configIds.end());

	if (idSet.size() != m_configIds.size())
	{
		DE_ASSERT(idSet.size() < m_configIds.size());
		m_testCtx.getLog() << tcu::TestLog::Message << "Warning: Duplicate config IDs in list" << TestLog::EndMessage;
	}

	// Get all configs
	vector<EGLConfig> allConfigs;
	display.getConfigs(allConfigs);

	// Collect list of configs with matching IDs
	m_configs.clear();
	for (vector<EGLConfig>::const_iterator cfgIter = allConfigs.begin(); cfgIter != allConfigs.end(); ++cfgIter)
	{
		const EGLint	configId	= display.getConfigAttrib(*cfgIter, EGL_CONFIG_ID);
		const bool		isInSet		= idSet.find(configId) != idSet.end();

		if (isInSet)
			m_configs.push_back(*cfgIter);
	}

	if (m_configs.empty())
	{
		// If no compatible configs are found, it is reported as NotSupported
		throw tcu::NotSupportedError("No compatible configs found");
	}

	// Init config iter
	m_configIter = m_configs.begin();

	// Init test case result to Pass
	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

SimpleConfigCase::IterateResult SimpleConfigCase::iterate (void)
{
	DE_ASSERT(m_configIter != m_configs.end());

	tcu::egl::Display&	display	= m_eglTestCtx.getDisplay();
	EGLConfig			config	= *m_configIter++;

	try
	{
		executeForConfig(display, config);
	}
	catch (const tcu::TestError& e)
	{
		m_testCtx.getLog() << e;
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}
	// \note Other errors are handled by framework (resource / internal errors).

	return (m_configIter != m_configs.end()) ? CONTINUE : STOP;
}

namespace
{

void addConfigId (map<string, NamedConfigIdSet*>& setMap, const char* name, const ConfigInfo& info)
{
	DE_ASSERT(setMap.find(name) != setMap.end());
	setMap[name]->getConfigIds().push_back(info.configId);
}

bool filterConfigStencil (map<string, NamedConfigIdSet*>& setMap, const char* namePrefix, const ConfigInfo& info)
{
	if (info.stencilSize > 0)
		addConfigId(setMap, (string(namePrefix) + "stencil").c_str(), info);
	else
		addConfigId(setMap, (string(namePrefix) + "no_stencil").c_str(), info);
	return true;
}

bool filterConfigDepth (map<string, NamedConfigIdSet*>& setMap, const char* namePrefix, const ConfigInfo& info)
{
	if (info.depthSize > 0)
		return filterConfigStencil(setMap, (string(namePrefix) + "depth_").c_str(), info);
	else
		return filterConfigStencil(setMap, (string(namePrefix) + "no_depth_").c_str(), info);
}

bool filterConfigColor (map<string, NamedConfigIdSet*>& setMap, const char* namePrefix, const ConfigInfo& info)
{
	static const struct
	{
		const char*	name;
		int			red, green, blue, alpha;
	}
	colorRules[] =
	{
		{ "rgb565",		5, 6, 5, 0 },
		{ "rgb888",		8, 8, 8, 0 },
		{ "rgba4444",	4, 4, 4, 4 },
		{ "rgba5551",	5, 5, 5, 1 },
		{ "rgba8888",	8, 8, 8, 8 }
	};
	for (int ndx = 0; ndx < (int)DE_LENGTH_OF_ARRAY(colorRules); ndx++)
	{
		if (info.redSize	== colorRules[ndx].red		&&
			info.greenSize	== colorRules[ndx].green	&&
			info.blueSize	== colorRules[ndx].blue		&&
			info.alphaSize	== colorRules[ndx].alpha)
			return filterConfigDepth(setMap, (string(namePrefix) + colorRules[ndx].name + "_").c_str(), info);
	}

	return false; // Didn't match any
}

} // anonymous

void NamedConfigIdSet::getDefaultSets (vector<NamedConfigIdSet>& configSets, const vector<ConfigInfo>& configInfos, const eglu::FilterList& baseFilters)
{
	// Set list
	configSets.push_back(NamedConfigIdSet("rgb565_no_depth_no_stencil",		"RGB565 configs without depth or stencil"));
	configSets.push_back(NamedConfigIdSet("rgb565_no_depth_stencil",		"RGB565 configs with stencil and no depth"));
	configSets.push_back(NamedConfigIdSet("rgb565_depth_no_stencil",		"RGB565 configs with depth and no stencil"));
	configSets.push_back(NamedConfigIdSet("rgb565_depth_stencil",			"RGB565 configs with depth and stencil"));
	configSets.push_back(NamedConfigIdSet("rgb888_no_depth_no_stencil",		"RGB888 configs without depth or stencil"));
	configSets.push_back(NamedConfigIdSet("rgb888_no_depth_stencil",		"RGB888 configs with stencil and no depth"));
	configSets.push_back(NamedConfigIdSet("rgb888_depth_no_stencil",		"RGB888 configs with depth and no stencil"));
	configSets.push_back(NamedConfigIdSet("rgb888_depth_stencil",			"RGB888 configs with depth and stencil"));
	configSets.push_back(NamedConfigIdSet("rgba4444_no_depth_no_stencil",	"RGBA4444 configs without depth or stencil"));
	configSets.push_back(NamedConfigIdSet("rgba4444_no_depth_stencil",		"RGBA4444 configs with stencil and no depth"));
	configSets.push_back(NamedConfigIdSet("rgba4444_depth_no_stencil",		"RGBA4444 configs with depth and no stencil"));
	configSets.push_back(NamedConfigIdSet("rgba4444_depth_stencil",			"RGBA4444 configs with depth and stencil"));
	configSets.push_back(NamedConfigIdSet("rgba5551_no_depth_no_stencil",	"RGBA5551 configs without depth or stencil"));
	configSets.push_back(NamedConfigIdSet("rgba5551_no_depth_stencil",		"RGBA5551 configs with stencil and no depth"));
	configSets.push_back(NamedConfigIdSet("rgba5551_depth_no_stencil",		"RGBA5551 configs with depth and no stencil"));
	configSets.push_back(NamedConfigIdSet("rgba5551_depth_stencil",			"RGBA5551 configs with depth and stencil"));
	configSets.push_back(NamedConfigIdSet("rgba8888_no_depth_no_stencil",	"RGBA8888 configs without depth or stencil"));
	configSets.push_back(NamedConfigIdSet("rgba8888_no_depth_stencil",		"RGBA8888 configs with stencil and no depth"));
	configSets.push_back(NamedConfigIdSet("rgba8888_depth_no_stencil",		"RGBA8888 configs with depth and no stencil"));
	configSets.push_back(NamedConfigIdSet("rgba8888_depth_stencil",			"RGBA8888 configs with depth and stencil"));
	configSets.push_back(NamedConfigIdSet("other",							"All other configs"));

	// Build set map
	map<string, NamedConfigIdSet*> setMap;
	for (int ndx = 0; ndx < (int)configSets.size(); ndx++)
		setMap[configSets[ndx].getName()] = &configSets[ndx];

	// Filter configs
	for (vector<ConfigInfo>::const_iterator cfgIter = configInfos.begin(); cfgIter != configInfos.end(); cfgIter++)
	{
		const ConfigInfo& info = *cfgIter;

		if (!baseFilters.match(info))
			continue;

		if (!filterConfigColor(setMap, "", info))
		{
			// Add to "other" set
			addConfigId(setMap, "other", info);
		}
	}

	// Sort config ids
	for (vector<NamedConfigIdSet>::iterator i = configSets.begin(); i != configSets.end(); i++)
	{
		vector<EGLint>& ids = i->getConfigIds();
		std::sort(ids.begin(), ids.end());
	}
}

} // egl
} // deqp
