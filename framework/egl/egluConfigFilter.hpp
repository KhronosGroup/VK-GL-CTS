#ifndef _EGLUCONFIGFILTER_HPP
#define _EGLUCONFIGFILTER_HPP
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

#include "tcuDefs.hpp"
#include "egluHeaderWrapper.hpp"
#include "tcuRGBA.hpp"

#include <vector>

namespace eglu
{

class ConfigInfo;

class ConfigFilter
{
public:
	enum Filter
	{
		FILTER_EQUAL = 0,
		FILTER_GREATER_OR_EQUAL,
		FILTER_AND,
		FILTER_NOT_SET,

		FILTER_LAST
	};

	ConfigFilter (EGLint attribute, EGLint value, Filter rule)
		: m_attribute	(attribute)
		, m_value		(value)
		, m_rule		(rule)
	{
	}

	ConfigFilter (void)
		: m_attribute	(0)
		, m_value		(0)
		, m_rule		(FILTER_LAST)
	{
	}

	bool match (EGLDisplay display, EGLConfig config) const;

	bool match (const ConfigInfo& configInfo) const;

private:
	EGLint		m_attribute;
	EGLint		m_value;
	Filter		m_rule;
};

template <EGLint Attribute>
class FilterTemplate
{
public:
					FilterTemplate			(void) {}
					~FilterTemplate			(void) {}

	ConfigFilter	operator==				(EGLint value) const	{ return ConfigFilter(Attribute, value, ConfigFilter::FILTER_EQUAL);			}
	ConfigFilter	operator>=				(EGLint value) const	{ return ConfigFilter(Attribute, value, ConfigFilter::FILTER_GREATER_OR_EQUAL);	}
	ConfigFilter	operator&				(EGLint value) const	{ return ConfigFilter(Attribute, value, ConfigFilter::FILTER_AND);				}
	ConfigFilter	operator^				(EGLint value) const	{ return ConfigFilter(Attribute, value, ConfigFilter::FILTER_NOT_SET);			}
};

// Helpers for filters
typedef FilterTemplate<EGL_CONFIG_ID>		ConfigId;
typedef FilterTemplate<EGL_RED_SIZE>		ConfigRedSize;
typedef FilterTemplate<EGL_GREEN_SIZE>		ConfigGreenSize;
typedef FilterTemplate<EGL_BLUE_SIZE>		ConfigBlueSize;
typedef FilterTemplate<EGL_ALPHA_SIZE>		ConfigAlphaSize;
typedef FilterTemplate<EGL_DEPTH_SIZE>		ConfigDepthSize;
typedef FilterTemplate<EGL_STENCIL_SIZE>	ConfigStencilSize;
typedef FilterTemplate<EGL_RENDERABLE_TYPE>	ConfigRenderableType;
typedef FilterTemplate<EGL_SURFACE_TYPE>	ConfigSurfaceType;
typedef FilterTemplate<EGL_SAMPLES>			ConfigSamples;

class FilterList
{
public:
								FilterList		(void) {}
								~FilterList		(void) {}

	FilterList&					operator<<		(const ConfigFilter& rule);
	FilterList&					operator<<		(const FilterList& other);

	bool						match			(const EGLDisplay display, EGLConfig config) const;
	bool						match			(const ConfigInfo& configInfo) const;

private:
	std::vector<ConfigFilter>	m_rules;
};

class ConfigColorBits
{
public:
					ConfigColorBits			(void) {};

	FilterList		operator==				(tcu::RGBA bits) const;
	FilterList		operator>=				(tcu::RGBA bits) const;
};

} // eglu

#endif // _EGLUCONFIGFILTER_HPP
