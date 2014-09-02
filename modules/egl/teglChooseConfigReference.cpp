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
 * \brief Choose config reference implementation.
 *//*--------------------------------------------------------------------*/

#include "teglChooseConfigReference.hpp"

#include <algorithm>
#include <vector>
#include <map>

namespace deqp
{
namespace egl
{

using eglu::ConfigInfo;

enum Criteria
{
	CRITERIA_AT_LEAST = 0,
	CRITERIA_EXACT,
	CRITERIA_MASK,
	CRITERIA_SPECIAL,

	CRITERIA_LAST
};

enum SortOrder
{
	SORTORDER_NONE	= 0,
	SORTORDER_SMALLER,
	SORTORDER_SPECIAL,

	SORTORDER_LAST
};

struct AttribRule
{
	EGLenum		name;
	EGLint		value;
	Criteria	criteria;
	SortOrder	sortOrder;

	AttribRule (void)
		: name			(EGL_NONE)
		, value			(EGL_NONE)
		, criteria		(CRITERIA_LAST)
		, sortOrder		(SORTORDER_LAST)
	{
	}

	AttribRule (EGLenum name_, EGLint value_, Criteria criteria_, SortOrder sortOrder_)
		: name			(name_)
		, value			(value_)
		, criteria		(criteria_)
		, sortOrder		(sortOrder_)
	{
	}
};

class SurfaceConfig
{
private:
	static int getCaveatRank (EGLenum caveat)
	{
		switch (caveat)
		{
			case EGL_NONE:					return 0;
			case EGL_SLOW_CONFIG:			return 1;
			case EGL_NON_CONFORMANT_CONFIG:	return 2;
			default: DE_ASSERT(DE_FALSE);	return 3;
		}
	}

	static int getColorBufferTypeRank (EGLenum type)
	{
		switch (type)
		{
			case EGL_RGB_BUFFER:			return 0;
			case EGL_LUMINANCE_BUFFER:		return 1;
			default: DE_ASSERT(DE_FALSE);	return 2;
		}
	}

	typedef bool (*CompareFunc) (const SurfaceConfig& a, const SurfaceConfig& b);

	static bool compareCaveat (const SurfaceConfig& a, const SurfaceConfig& b)
	{
		return getCaveatRank((EGLenum)a.m_info.configCaveat) < getCaveatRank((EGLenum)b.m_info.configCaveat);
	}

	static bool compareColorBufferType (const SurfaceConfig& a, const SurfaceConfig& b)
	{
		return getColorBufferTypeRank((EGLenum)a.m_info.colorBufferType) < getColorBufferTypeRank((EGLenum)b.m_info.colorBufferType);
	}

	static bool compareColorBufferBits (const SurfaceConfig& a, const SurfaceConfig& b)
	{
		DE_ASSERT(a.m_info.colorBufferType == b.m_info.colorBufferType);
		switch (a.m_info.colorBufferType)
		{
			case EGL_RGB_BUFFER:
				return (a.m_info.redSize + a.m_info.greenSize + a.m_info.blueSize + a.m_info.alphaSize)
						> (b.m_info.redSize + b.m_info.greenSize + b.m_info.blueSize + b.m_info.alphaSize);

			case EGL_LUMINANCE_BUFFER:
				return (a.m_info.luminanceSize + a.m_info.alphaSize) > (b.m_info.luminanceSize + b.m_info.alphaSize);

			default:
				DE_ASSERT(DE_FALSE);
				return true;
		}
	}

	template <EGLenum Attribute>
	static bool compareAttributeSmaller (const SurfaceConfig& a, const SurfaceConfig& b)
	{
		return a.getAttribute(Attribute) < b.getAttribute(Attribute);
	}
public:
	SurfaceConfig (EGLConfig config, ConfigInfo &info)
		: m_config(config)
		, m_info(info)
	{
	}

	EGLConfig getEglConfig (void) const
	{
		return m_config;
	}

	EGLint getAttribute (const EGLenum attribute) const
	{
		return m_info.getAttribute(attribute);
	}

	friend bool operator== (const SurfaceConfig& a, const SurfaceConfig& b)
	{
		for (std::map<EGLenum, AttribRule>::const_iterator iter = SurfaceConfig::defaultRules.begin(); iter != SurfaceConfig::defaultRules.end(); iter++)
		{
			const EGLenum attribute = iter->first;

			if (a.getAttribute(attribute) != b.getAttribute(attribute)) return false;
		}
		return true;
	}

	bool compareTo (const SurfaceConfig& b, bool skipColorBufferBits=false) const
	{
		static const SurfaceConfig::CompareFunc compareFuncs[] =
		{
			SurfaceConfig::compareCaveat,
			SurfaceConfig::compareColorBufferType,
			SurfaceConfig::compareColorBufferBits,
			SurfaceConfig::compareAttributeSmaller<EGL_BUFFER_SIZE>,
			SurfaceConfig::compareAttributeSmaller<EGL_SAMPLE_BUFFERS>,
			SurfaceConfig::compareAttributeSmaller<EGL_SAMPLES>,
			SurfaceConfig::compareAttributeSmaller<EGL_DEPTH_SIZE>,
			SurfaceConfig::compareAttributeSmaller<EGL_STENCIL_SIZE>,
			SurfaceConfig::compareAttributeSmaller<EGL_ALPHA_MASK_SIZE>,
			SurfaceConfig::compareAttributeSmaller<EGL_CONFIG_ID>
		};

		if (*this == b)
			return false; // std::sort() can compare object to itself.

		for (int ndx = 0; ndx < (int)DE_LENGTH_OF_ARRAY(compareFuncs); ndx++)
		{
			if (skipColorBufferBits && (compareFuncs[ndx] == SurfaceConfig::compareColorBufferBits))
				continue;

			if (compareFuncs[ndx](*this, b))
				return true;
			else if (compareFuncs[ndx](b, *this))
				return false;
		}

		TCU_FAIL("Unable to compare configs - duplicate ID?");
	}

	static const std::map<EGLenum, AttribRule> defaultRules;

	static std::map<EGLenum, AttribRule> initAttribRules (void)
	{
		// \todo [2011-03-24 pyry] From EGL 1.4 spec - check that this is valid for other versions as well
		std::map<EGLenum, AttribRule> rules;

		//									Attribute									Default				Selection Criteria	Sort Order			Sort Priority
		rules[EGL_BUFFER_SIZE]				= AttribRule(EGL_BUFFER_SIZE,				0,					CRITERIA_AT_LEAST,	SORTORDER_SMALLER);	//	4
		rules[EGL_RED_SIZE]					= AttribRule(EGL_RED_SIZE,					0,					CRITERIA_AT_LEAST,	SORTORDER_SPECIAL);	//	3
		rules[EGL_GREEN_SIZE]				= AttribRule(EGL_GREEN_SIZE,				0,					CRITERIA_AT_LEAST,	SORTORDER_SPECIAL);	//	3
		rules[EGL_BLUE_SIZE]				= AttribRule(EGL_BLUE_SIZE,					0,					CRITERIA_AT_LEAST,	SORTORDER_SPECIAL);	//	3
		rules[EGL_LUMINANCE_SIZE]			= AttribRule(EGL_LUMINANCE_SIZE,			0,					CRITERIA_AT_LEAST,	SORTORDER_SPECIAL);	//	3
		rules[EGL_ALPHA_SIZE]				= AttribRule(EGL_ALPHA_SIZE,				0,					CRITERIA_AT_LEAST,	SORTORDER_SPECIAL);	//	3
		rules[EGL_ALPHA_MASK_SIZE]			= AttribRule(EGL_ALPHA_MASK_SIZE,			0,					CRITERIA_AT_LEAST,	SORTORDER_SMALLER);	//	9
		rules[EGL_BIND_TO_TEXTURE_RGB]		= AttribRule(EGL_BIND_TO_TEXTURE_RGB,		EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_BIND_TO_TEXTURE_RGBA]		= AttribRule(EGL_BIND_TO_TEXTURE_RGBA,		EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_COLOR_BUFFER_TYPE]		= AttribRule(EGL_COLOR_BUFFER_TYPE,			EGL_RGB_BUFFER,		CRITERIA_EXACT,		SORTORDER_NONE);	//	2
		rules[EGL_CONFIG_CAVEAT]			= AttribRule(EGL_CONFIG_CAVEAT,				EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_SPECIAL);	//	1
		rules[EGL_CONFIG_ID]				= AttribRule(EGL_CONFIG_ID,					EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_SMALLER);	//	11
		rules[EGL_CONFORMANT]				= AttribRule(EGL_CONFORMANT,				0,					CRITERIA_MASK,		SORTORDER_NONE);
		rules[EGL_DEPTH_SIZE]				= AttribRule(EGL_DEPTH_SIZE,				0,					CRITERIA_AT_LEAST,	SORTORDER_SMALLER);	//	7
		rules[EGL_LEVEL]					= AttribRule(EGL_LEVEL,						0,					CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_MATCH_NATIVE_PIXMAP]		= AttribRule(EGL_MATCH_NATIVE_PIXMAP,		EGL_NONE,			CRITERIA_SPECIAL,	SORTORDER_NONE);
		rules[EGL_MAX_SWAP_INTERVAL]		= AttribRule(EGL_MAX_SWAP_INTERVAL,			EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_MIN_SWAP_INTERVAL]		= AttribRule(EGL_MIN_SWAP_INTERVAL,			EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_NATIVE_RENDERABLE]		= AttribRule(EGL_NATIVE_RENDERABLE,			EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_NATIVE_VISUAL_TYPE]		= AttribRule(EGL_NATIVE_VISUAL_TYPE,		EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_SPECIAL);	//	10
		rules[EGL_RENDERABLE_TYPE]			= AttribRule(EGL_RENDERABLE_TYPE,			EGL_OPENGL_ES_BIT,	CRITERIA_MASK,		SORTORDER_NONE);
		rules[EGL_SAMPLE_BUFFERS]			= AttribRule(EGL_SAMPLE_BUFFERS,			0,					CRITERIA_AT_LEAST,	SORTORDER_SMALLER);	//	5
		rules[EGL_SAMPLES]					= AttribRule(EGL_SAMPLES,					0,					CRITERIA_AT_LEAST,	SORTORDER_SMALLER);	//	6
		rules[EGL_STENCIL_SIZE]				= AttribRule(EGL_STENCIL_SIZE,				0,					CRITERIA_AT_LEAST,	SORTORDER_SMALLER);	//	8
		rules[EGL_SURFACE_TYPE]				= AttribRule(EGL_SURFACE_TYPE,				EGL_WINDOW_BIT,		CRITERIA_MASK,		SORTORDER_NONE);
		rules[EGL_TRANSPARENT_TYPE]			= AttribRule(EGL_TRANSPARENT_TYPE,			EGL_NONE,			CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_TRANSPARENT_RED_VALUE]	= AttribRule(EGL_TRANSPARENT_RED_VALUE,		EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_TRANSPARENT_GREEN_VALUE]	= AttribRule(EGL_TRANSPARENT_GREEN_VALUE,	EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_NONE);
		rules[EGL_TRANSPARENT_BLUE_VALUE]	= AttribRule(EGL_TRANSPARENT_BLUE_VALUE,	EGL_DONT_CARE,		CRITERIA_EXACT,		SORTORDER_NONE);

		return rules;
	}
private:
	EGLConfig m_config;
	ConfigInfo m_info;
};

const std::map<EGLenum, AttribRule> SurfaceConfig::defaultRules = SurfaceConfig::initAttribRules();

class CompareConfigs
{
public:
	CompareConfigs (bool skipColorBufferBits)
		: m_skipColorBufferBits(skipColorBufferBits)
	{
	}

	bool operator() (const SurfaceConfig& a, const SurfaceConfig& b)
	{
		return a.compareTo(b, m_skipColorBufferBits);
	}

private:
	bool m_skipColorBufferBits;
};

class ConfigFilter
{
private:
	std::map<EGLenum, AttribRule> m_rules;
public:
	ConfigFilter ()
		: m_rules(SurfaceConfig::defaultRules)
	{
	}

	void setValue (EGLenum name, EGLint value)
	{
		DE_ASSERT(SurfaceConfig::defaultRules.find(name) != SurfaceConfig::defaultRules.end());
		m_rules[name].value = value;
	}

	void setValues (std::vector<std::pair<EGLenum, EGLint> > values)
	{
		for (size_t ndx = 0; ndx < values.size(); ndx++)
		{
			const EGLenum	name	= values[ndx].first;
			const EGLint	value	= values[ndx].second;

			setValue(name, value);
		}
	}

	AttribRule getAttribute (EGLenum name)
	{
		DE_ASSERT(SurfaceConfig::defaultRules.find(name) != SurfaceConfig::defaultRules.end());
		return m_rules[name];
	}

	bool isMatch (const SurfaceConfig& config)
	{
		bool result = true;
		for (std::map<EGLenum, AttribRule>::const_iterator iter = m_rules.begin(); iter != m_rules.end(); iter++)
		{
			const AttribRule rule = iter->second;

			if (rule.value == EGL_DONT_CARE)
				continue;
			else if (rule.name == EGL_MATCH_NATIVE_PIXMAP)
				TCU_CHECK(rule.value == EGL_NONE); // Not supported
			else if (rule.name == EGL_TRANSPARENT_RED_VALUE || rule.name == EGL_TRANSPARENT_GREEN_VALUE || rule.name == EGL_TRANSPARENT_BLUE_VALUE)
				continue;
			else
			{
				const EGLint cfgValue = config.getAttribute(rule.name);

				switch (rule.criteria)
				{
					case CRITERIA_EXACT:	result = rule.value == cfgValue;				break;
					case CRITERIA_AT_LEAST:	result = rule.value <= cfgValue;				break;
					case CRITERIA_MASK:		result = (rule.value & cfgValue) == rule.value;	break;
					default:				TCU_FAIL("Unknown criteria");
				}
			}

			if (result == false) return false;
		}

		return true;
	}

	bool isColorBitsUnspecified (void)
	{
		const EGLenum	bitAttribs[]	= { EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_LUMINANCE_SIZE };

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(bitAttribs); ndx++)
		{
			const EGLenum	attrib	= bitAttribs[ndx];
			const EGLint	value	= getAttribute(attrib).value;

			if (value != 0 && value != EGL_DONT_CARE) return false;
		}

		return true;
	}

	std::vector<SurfaceConfig> filter (const std::vector<SurfaceConfig>& configs)
	{
		std::vector<SurfaceConfig> out;

		for (std::vector<SurfaceConfig>::const_iterator iter = configs.begin(); iter != configs.end(); iter++)
		{
			if (isMatch(*iter)) out.push_back(*iter);
		}

		return out;
	}
};

void chooseConfigReference (const tcu::egl::Display& display, std::vector<EGLConfig>& dst, const std::vector<std::pair<EGLenum, EGLint> >& attributes)
{
	// Get all configs
	std::vector<EGLConfig> eglConfigs;
	display.getConfigs(eglConfigs);

	// Config infos
	std::vector<ConfigInfo> configInfos;
	configInfos.resize(eglConfigs.size());
	for (size_t ndx = 0; ndx < eglConfigs.size(); ndx++)
		display.describeConfig(eglConfigs[ndx], configInfos[ndx]);

	TCU_CHECK_EGL_MSG("Config query failed");

	// Pair configs with info
	std::vector<SurfaceConfig> configs;
	for (size_t ndx = 0; ndx < eglConfigs.size(); ndx++)
		configs.push_back(SurfaceConfig(eglConfigs[ndx], configInfos[ndx]));

	// Filter configs
	ConfigFilter configFilter;
	configFilter.setValues(attributes);

	std::vector<SurfaceConfig> filteredConfigs = configFilter.filter(configs);

	// Sort configs
	std::sort(filteredConfigs.begin(), filteredConfigs.end(), CompareConfigs(configFilter.isColorBitsUnspecified()));

	// Write to dst list
	dst.resize(filteredConfigs.size());
	for (size_t ndx = 0; ndx < filteredConfigs.size(); ndx++)
		dst[ndx] = filteredConfigs[ndx].getEglConfig();
}

} // egl
} // deqp
