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
 * \brief OpenGL ES rendering context.
 *//*--------------------------------------------------------------------*/

#include "gluRenderContext.hpp"
#include "gluDefs.hpp"
#include "gluRenderConfig.hpp"
#include "gluES3PlusWrapperContext.hpp"
#include "gluFboRenderContext.hpp"
#include "gluPlatform.hpp"
#include "gluStrUtil.hpp"
#include "glwInitFunctions.hpp"
#include "glwEnums.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"
#include "deStringUtil.hpp"

namespace glu
{

inline bool versionGreaterOrEqual (ApiType a, ApiType b)
{
	return a.getMajorVersion() > b.getMajorVersion() ||
		   (a.getMajorVersion() == b.getMajorVersion() && a.getMinorVersion() >= b.getMinorVersion());
}

bool contextSupports (ContextType ctxType, ApiType requiredApiType)
{
	// \todo [2014-10-06 pyry] Check exact forward-compatible restrictions.
	const bool forwardCompatible = (ctxType.getFlags() & CONTEXT_FORWARD_COMPATIBLE) != 0;

	if (isContextTypeES(ctxType))
	{
		DE_ASSERT(!forwardCompatible);
		return requiredApiType.getProfile() == PROFILE_ES &&
			   versionGreaterOrEqual(ctxType.getAPI(), requiredApiType);
	}
	else if (isContextTypeGLCore(ctxType))
	{
		if (forwardCompatible)
			return ctxType.getAPI() == requiredApiType;
		else
			return requiredApiType.getProfile() == PROFILE_CORE &&
				   versionGreaterOrEqual(ctxType.getAPI(), requiredApiType);
	}
	else if (isContextTypeGLCompatibility(ctxType))
	{
		DE_ASSERT(!forwardCompatible);
		return (requiredApiType.getProfile() == PROFILE_CORE || requiredApiType.getProfile() == PROFILE_COMPATIBILITY) &&
			   versionGreaterOrEqual(ctxType.getAPI(), requiredApiType);
	}
	else
	{
		DE_ASSERT(false);
		return false;
	}
}

static ContextFlags parseContextFlags (const std::string& flagsStr)
{
	const std::vector<std::string>	flagNames	= de::splitString(flagsStr, ',');
	ContextFlags					flags		= ContextFlags(0);
	static const struct
	{
		const char*		name;
		ContextFlags	flag;
	} s_flagMap[] =
	{
		{ "debug",		CONTEXT_DEBUG	},
		{ "robust",		CONTEXT_ROBUST	}
	};

	for (std::vector<std::string>::const_iterator flagIter = flagNames.begin(); flagIter != flagNames.end(); ++flagIter)
	{
		int ndx;
		for (ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_flagMap); ndx++)
		{
			if (*flagIter == s_flagMap[ndx].name)
			{
				flags = flags | s_flagMap[ndx].flag;
				break;
			}
		}

		if (ndx == DE_LENGTH_OF_ARRAY(s_flagMap))
		{
			tcu::print("ERROR: Unrecognized GL context flag '%s'\n", flagIter->c_str());
			tcu::print("Supported GL context flags:\n");

			for (ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_flagMap); ndx++)
				tcu::print("  %s\n", s_flagMap[ndx].name);

			throw tcu::NotSupportedError((std::string("Unknown GL context flag '") + *flagIter + "'").c_str(), DE_NULL, __FILE__, __LINE__);
		}
	}

	return flags;
}

RenderContext* createDefaultRenderContext (tcu::Platform& platform, const tcu::CommandLine& cmdLine, ApiType apiType)
{
	const ContextFactoryRegistry&	registry		= platform.getGLPlatform().getContextFactoryRegistry();
	RenderConfig					config;
	const char*						factoryName		= cmdLine.getGLContextType();
	const ContextFactory*			factory			= DE_NULL;
	ContextFlags					ctxFlags		= ContextFlags(0);

	if (registry.empty())
		throw tcu::NotSupportedError("OpenGL is not supported", DE_NULL, __FILE__, __LINE__);

	if (cmdLine.getGLContextFlags())
		ctxFlags = parseContextFlags(cmdLine.getGLContextFlags());

	config.type = glu::ContextType(apiType, ctxFlags);
	parseRenderConfig(&config, cmdLine);

	if (factoryName)
	{
		factory = registry.getFactoryByName(factoryName);

		if (!factory)
		{
			tcu::print("ERROR: Unknown or unsupported GL context type '%s'\n", factoryName);
			tcu::print("Supported GL context types:\n");

			for (int factoryNdx = 0; factoryNdx < (int)registry.getFactoryCount(); factoryNdx++)
			{
				const ContextFactory* curFactory = registry.getFactoryByIndex(factoryNdx);
				tcu::print("  %s: %s\n", curFactory->getName(), curFactory->getDescription());
			}

			throw tcu::NotSupportedError((std::string("Unknown GL context type '") + factoryName + "'").c_str(), DE_NULL, __FILE__, __LINE__);
		}
	}
	else
		factory = registry.getDefaultFactory();

	try
	{
		if (cmdLine.getSurfaceType() == tcu::SURFACETYPE_FBO)
			return new FboRenderContext(*factory, config, cmdLine);
		else
			return factory->createContext(config, cmdLine);

	}
	catch (const std::exception&)
	{
		// If ES31 context is not available, try using wrapper.
		if (config.type.getAPI() == ApiType::es(3,1))
		{
			tcu::print("Warning: Unable to create native OpenGL ES 3.1 context, will use wrapper context.\n");
			return new ES3PlusWrapperContext(*factory, config, cmdLine);
		}
		else
			throw;
	}
}

static std::vector<std::string> getExtensions (const glw::Functions& gl, ApiType apiType)
{
	using std::vector;
	using std::string;

	if (apiType.getProfile() == PROFILE_ES && apiType.getMajorVersion() == 2)
	{
		TCU_CHECK(gl.getString);

		const char*	extStr	= (const char*)gl.getString(GL_EXTENSIONS);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetString(GL_EXTENSIONS)");

		if (extStr)
			return de::splitString(extStr);
		else
			throw tcu::TestError("glGetString(GL_EXTENSIONS) returned null pointer", DE_NULL, __FILE__, __LINE__);
	}
	else
	{
		int				numExtensions	= 0;
		vector<string>	extensions;

		TCU_CHECK(gl.getIntegerv && gl.getStringi);

		gl.getIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv(GL_NUM_EXTENSIONS)");

		if (numExtensions > 0)
		{
			extensions.resize(numExtensions);

			for (int ndx = 0; ndx < numExtensions; ndx++)
			{
				const char* const ext = (const char*)gl.getStringi(GL_EXTENSIONS, ndx);
				GLU_EXPECT_NO_ERROR(gl.getError(), "glGetStringi(GL_EXTENSIONS)");

				if (ext)
					extensions[ndx] = ext;
				else
					throw tcu::TestError("glGetStringi(GL_EXTENSIONS) returned null pointer", DE_NULL, __FILE__, __LINE__);
			}

		}

		return extensions;
	}
}

void initCoreFunctions (glw::Functions* dst, const glw::FunctionLoader* loader, ApiType apiType)
{
	static const struct
	{
		ApiType		apiType;
		void		(*initFunc)		(glw::Functions* gl, const glw::FunctionLoader* loader);
	} s_initFuncs[] =
	{
		{ ApiType::es(2,0),		glw::initES20		},
		{ ApiType::es(3,0),		glw::initES30		},
		{ ApiType::es(3,1),		glw::initES31		},
		{ ApiType::core(3,0),	glw::initGL30Core	},
		{ ApiType::core(3,1),	glw::initGL31Core	},
		{ ApiType::core(3,2),	glw::initGL32Core	},
		{ ApiType::core(3,3),	glw::initGL33Core	},
		{ ApiType::core(4,0),	glw::initGL40Core	},
		{ ApiType::core(4,1),	glw::initGL41Core	},
		{ ApiType::core(4,2),	glw::initGL42Core	},
		{ ApiType::core(4,3),	glw::initGL43Core	},
		{ ApiType::core(4,4),	glw::initGL44Core	},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_initFuncs); ndx++)
	{
		if (s_initFuncs[ndx].apiType == apiType)
		{
			s_initFuncs[ndx].initFunc(dst, loader);
			return;
		}
	}

	throw tcu::InternalError(std::string("Don't know how to load functions for ") + de::toString(apiType));
}

void initExtensionFunctions (glw::Functions* dst, const glw::FunctionLoader* loader, ApiType apiType)
{
	std::vector<std::string> extensions = getExtensions(*dst, apiType);

	if (!extensions.empty())
	{
		std::vector<const char*> extStr(extensions.size());

		for (size_t ndx = 0; ndx < extensions.size(); ndx++)
			extStr[ndx] = extensions[ndx].c_str();

		initExtensionFunctions(dst, loader, apiType, (int)extStr.size(), &extStr[0]);
	}
}

void initExtensionFunctions (glw::Functions* dst, const glw::FunctionLoader* loader, ApiType apiType, int numExtensions, const char* const* extensions)
{
	if (apiType.getProfile() == PROFILE_ES)
		glw::initExtensionsES(dst, loader, numExtensions, extensions);
	else
		glw::initExtensionsGL(dst, loader, numExtensions, extensions);
}

void initFunctions (glw::Functions* dst, const glw::FunctionLoader* loader, ApiType apiType)
{
	initCoreFunctions(dst, loader, apiType);
	initExtensionFunctions(dst, loader, apiType);
}

} // glu
