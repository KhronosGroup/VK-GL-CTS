/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief EGL Config selection helper.
 *//*--------------------------------------------------------------------*/

#include "egluConfigFilter.hpp"
#include "egluUtil.hpp"
#include "egluConfigInfo.hpp"

#include <algorithm>

using std::vector;

namespace eglu
{

bool ConfigFilter::match (EGLDisplay display, EGLConfig config) const
{
	EGLint	cmpValue	= getConfigAttribInt(display, config, m_attribute);
	bool	isMatch		= false;

	switch (m_rule)
	{
		case FILTER_EQUAL:				isMatch = (cmpValue == m_value);			break;
		case FILTER_GREATER_OR_EQUAL:	isMatch = (cmpValue >= m_value);			break;
		case FILTER_AND:				isMatch = (cmpValue & m_value) == m_value;	break;
		case FILTER_NOT_SET:			isMatch = (cmpValue & m_value) == 0;		break;
		default:						DE_ASSERT(false);							break;
	}

	return isMatch;
}

bool ConfigFilter::match (const ConfigInfo& configInfo) const
{
	EGLint	cmpValue	= configInfo.getAttribute(m_attribute);
	bool	isMatch		= false;

	switch (m_rule)
	{
		case FILTER_EQUAL:				isMatch = (cmpValue == m_value);			break;
		case FILTER_GREATER_OR_EQUAL:	isMatch = (cmpValue >= m_value);			break;
		case FILTER_AND:				isMatch = (cmpValue & m_value) == m_value;	break;
		case FILTER_NOT_SET:			isMatch = (cmpValue & m_value) == 0;		break;
		default:						DE_ASSERT(false);							break;
	}

	return isMatch;
}

FilterList ConfigColorBits::operator== (tcu::RGBA bits) const
{
	FilterList list;
	list << (ConfigRedSize()	== bits.getRed())
		 << (ConfigGreenSize()	== bits.getGreen())
		 << (ConfigBlueSize()	== bits.getBlue())
		 << (ConfigAlphaSize()	== bits.getAlpha());
	return list;
}

FilterList ConfigColorBits::operator>= (tcu::RGBA bits) const
{
	FilterList list;
	list << (ConfigRedSize()	>= bits.getRed())
		 << (ConfigGreenSize()	>= bits.getGreen())
		 << (ConfigBlueSize()	>= bits.getBlue())
		 << (ConfigAlphaSize()	>= bits.getAlpha());
	return list;
}

FilterList& FilterList::operator<< (const ConfigFilter& rule)
{
	m_rules.push_back(rule);
	return *this;
}

FilterList& FilterList::operator<< (const FilterList& other)
{
	size_t oldEnd = m_rules.size();
	m_rules.resize(m_rules.size()+other.m_rules.size());
	std::copy(other.m_rules.begin(), other.m_rules.end(), m_rules.begin()+oldEnd);
	return *this;
}

bool FilterList::match (const EGLDisplay display, EGLConfig config) const
{
	for (vector<ConfigFilter>::const_iterator ruleIter = m_rules.begin(); ruleIter != m_rules.end(); ruleIter++)
	{
		if (!ruleIter->match(display, config))
			return false;
	}
	return true;
}

bool FilterList::match (const ConfigInfo& configInfo) const
{
	for (vector<ConfigFilter>::const_iterator ruleIter = m_rules.begin(); ruleIter != m_rules.end(); ruleIter++)
	{
		if (!ruleIter->match(configInfo))
			return false;
	}
	return true;
}

} // eglu
