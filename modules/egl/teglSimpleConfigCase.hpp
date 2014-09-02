#ifndef _TEGLSIMPLECONFIGCASE_HPP
#define _TEGLSIMPLECONFIGCASE_HPP
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

#include "tcuDefs.hpp"
#include "teglTestCase.hpp"
#include "tcuEgl.hpp"
#include "egluConfigFilter.hpp"

#include <vector>
#include <string>

namespace deqp
{
namespace egl
{

class SimpleConfigCase : public TestCase
{
public:
							SimpleConfigCase			(EglTestContext& eglTestCtx, const char* name, const char* description, const std::vector<EGLint>& configIds);
	virtual					~SimpleConfigCase			(void);

	void					init						(void);
	IterateResult			iterate						(void);

private:
	virtual void			executeForConfig			(tcu::egl::Display& display, EGLConfig config)	= DE_NULL;

							SimpleConfigCase			(const SimpleConfigCase& other);
	SimpleConfigCase&		operator=					(const SimpleConfigCase& other);

	std::vector<EGLint>					m_configIds;
	std::vector<EGLConfig>				m_configs;
	std::vector<EGLConfig>::iterator	m_configIter;
};

class NamedConfigIdSet
{
public:
								NamedConfigIdSet		(void) {}
								NamedConfigIdSet		(const char* name, const char* description) : m_name(name), m_description(description) {}
								NamedConfigIdSet		(const char* name, const char* description, const std::vector<EGLint>& configIds) : m_name(name), m_description(description), m_configIds(configIds) {}

	const char*					getName					(void) const	{ return m_name.c_str();		}
	const char*					getDescription			(void) const	{ return m_description.c_str();	}
	const std::vector<EGLint>	getConfigIds			(void) const	{ return m_configIds;			}

	std::vector<EGLint>&		getConfigIds			(void)			{ return m_configIds;			}

	static void					getDefaultSets			(std::vector<NamedConfigIdSet>& configSets, const std::vector<eglu::ConfigInfo>& configInfos, const eglu::FilterList& baseFilters);

private:
	std::string					m_name;
	std::string					m_description;
	std::vector<EGLint>			m_configIds;
};

} // egl
} // deqp

#endif // _TEGLSIMPLECONFIGCASE_HPP
