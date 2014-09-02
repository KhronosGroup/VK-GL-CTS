#ifndef _GLUES3PLUSWRAPPERCONTEXT_HPP
#define _GLUES3PLUSWRAPPERCONTEXT_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief OpenGL ES 3plus wrapper context.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluRenderContext.hpp"
#include "glwFunctions.hpp"

namespace tcu
{
class CommandLine;
}

namespace glu
{

class ContextFactory;
struct RenderConfig;

namespace es3plus
{

class Context;

} // es3plus

/*--------------------------------------------------------------------*//*!
 * \brief OpenGL ES 3plus wrapper.
 *//*--------------------------------------------------------------------*/
class ES3PlusWrapperContext : public RenderContext
{
public:
										ES3PlusWrapperContext	(const ContextFactory& factory, const RenderConfig& config, const tcu::CommandLine& cmdLine);
	virtual								~ES3PlusWrapperContext	(void);

	virtual ContextType					getType					(void) const;
	virtual const glw::Functions&		getFunctions			(void) const	{ return m_functions;							}

	virtual const tcu::RenderTarget&	getRenderTarget			(void) const	{ return m_context->getRenderTarget();			}
	virtual deUint32					getDefaultFramebuffer	(void) const	{ return m_context->getDefaultFramebuffer();	}
	virtual void						postIterate				(void)			{ m_context->postIterate();						}

private:
	RenderContext*						m_context;				//!< Actual GL 4.3 core context.
	glw::Functions						m_functions;

	es3plus::Context*					m_wrapperCtx;			//!< Wrapper context
};

} // glu

#endif // _GLUES3PLUSWRAPPERCONTEXT_HPP
