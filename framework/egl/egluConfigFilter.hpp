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
#include "tcuRGBA.hpp"

#include "eglwDefs.hpp"
#include "eglwEnums.hpp"

#include <vector>

namespace eglw
{
class Library;
}

namespace eglu
{

class ConfigInfo;

class CandidateConfig
{
public:
					CandidateConfig		(const eglw::Library& egl, eglw::EGLDisplay display, eglw::EGLConfig config);
					CandidateConfig		(const ConfigInfo& configInfo);

	int				get					(deUint32 attrib) const;

	int				id					(void) const { return get(EGL_CONFIG_ID);		}
	int				redSize				(void) const { return get(EGL_RED_SIZE);		}
	int				greenSize			(void) const { return get(EGL_GREEN_SIZE);		}
	int				blueSize			(void) const { return get(EGL_BLUE_SIZE);		}
	int				alphaSize			(void) const { return get(EGL_ALPHA_SIZE);		}
	int				depthSize			(void) const { return get(EGL_DEPTH_SIZE);		}
	int				stencilSize			(void) const { return get(EGL_STENCIL_SIZE);	}
	int				samples				(void) const { return get(EGL_SAMPLES);			}

	deUint32		renderableType		(void) const { return (deUint32)get(EGL_RENDERABLE_TYPE);	}
	deUint32		surfaceType			(void) const { return (deUint32)get(EGL_SURFACE_TYPE);		}

	tcu::RGBA		colorBits			(void) const { return tcu::RGBA(redSize(), greenSize(), blueSize(), alphaSize());	}

private:
	enum Type
	{
		TYPE_EGL_OBJECT = 0,
		TYPE_CONFIG_INFO,

		TYPE_LAST
	};

	const Type		m_type;
	union
	{
		struct
		{
			const eglw::Library*	egl;
			eglw::EGLDisplay		display;
			eglw::EGLConfig			config;
		} object;
		const ConfigInfo*			configInfo;
	} m_cfg;
};

typedef bool (*ConfigFilter) (const CandidateConfig& candidate);

class FilterList
{
public:
								FilterList		(void) {}
								~FilterList		(void) {}

	FilterList&					operator<<		(ConfigFilter filter);
	FilterList&					operator<<		(const FilterList& other);

	bool						match			(const eglw::Library& egl, eglw::EGLDisplay display, eglw::EGLConfig config) const;
	bool						match			(const ConfigInfo& configInfo) const;
	bool						match			(const CandidateConfig& candidate) const;

private:
	std::vector<ConfigFilter>	m_rules;
};

} // eglu

#endif // _EGLUCONFIGFILTER_HPP
