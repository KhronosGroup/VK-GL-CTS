#ifndef _TEGLRENDERCASE_HPP
#define _TEGLRENDERCASE_HPP
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
 * \brief Base class for rendering tests.
 * \todo [2011-07-18 pyry] Uses currently gles2-specific RenderContext.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "teglTestCase.hpp"
#include "teglSimpleConfigCase.hpp"

#include <vector>

namespace deqp
{
namespace egl
{

class RenderCase : public SimpleConfigCase
{
public:
					RenderCase				(EglTestContext& eglTestCtx, const char* name, const char* description, EGLint apiMask, EGLint surfaceTypeMask, const std::vector<EGLint>& configIds);
	virtual			~RenderCase				(void);

	static EGLint	getSupportedApis		(void);

protected:
	virtual void	executeForConfig		(tcu::egl::Display& display, EGLConfig config);

	virtual void	executeForSurface		(tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config) = DE_NULL;

	EGLint			m_apiMask;
	EGLint			m_surfaceTypeMask;
};

class SingleContextRenderCase : public RenderCase
{
public:
					SingleContextRenderCase		(EglTestContext& eglTestCtx, const char* name, const char* description, EGLint apiMask, EGLint surfaceTypeMask, const std::vector<EGLint>& configIds);
	virtual			~SingleContextRenderCase	(void);

protected:
	virtual void	executeForSurface			(tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config);

	virtual void	executeForContext			(const tcu::egl::Display& display, tcu::egl::Context& context, tcu::egl::Surface& surface, EGLint api) = DE_NULL;
};

class MultiContextRenderCase : public RenderCase
{
public:
						MultiContextRenderCase		(EglTestContext& eglTestCtx, const char* name, const char* description, EGLint api, EGLint surfaceType, const std::vector<EGLint>& configIds, int numContextsPerApi);
	virtual				~MultiContextRenderCase		(void);

protected:
	void				executeForSurface			(tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config);

	virtual void		executeForContexts			(tcu::egl::Display& display, tcu::egl::Surface& surface, EGLConfig config, const std::vector<std::pair<EGLint, tcu::egl::Context*> >& contexts) = DE_NULL;

	int					m_numContextsPerApi;
};

class RenderConfigIdSet : public NamedConfigIdSet
{
public:
	RenderConfigIdSet (const char* name, const char* description, std::vector<EGLint> configIds, EGLint surfaceTypeMask)
		: NamedConfigIdSet	(name, description, configIds)
		, m_surfaceTypeMask	(surfaceTypeMask)
	{
	}

	EGLint getSurfaceTypeMask (void) const
	{
		return m_surfaceTypeMask;
	}

private:
	EGLint m_surfaceTypeMask;
};

void getDefaultRenderConfigIdSets (std::vector<RenderConfigIdSet>& configSets, const std::vector<eglu::ConfigInfo>& configInfos, const eglu::FilterList& baseFilters);

} // egl
} // deqp

#endif // _TEGLRENDERCASE_HPP
