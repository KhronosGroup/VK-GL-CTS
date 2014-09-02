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
 * \brief Simple context construction test for EGL_KHR_create_context.
 *//*--------------------------------------------------------------------*/

#include "teglCreateContextExtTests.hpp"

#include "tcuTestLog.hpp"

#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluConfigFilter.hpp"
#include "egluStrUtil.hpp"
#include "egluUtil.hpp"

#include "gluDefs.hpp"
#include "gluRenderConfig.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <EGL/eglext.h>

#include <string>
#include <vector>
#include <set>
#include <sstream>

#include <cstring>

// \note Taken from official EGL/eglext.h. EGL_EGLEXT_VERSION 20131028
#ifndef EGL_KHR_create_context
#define EGL_KHR_create_context 1
#define EGL_CONTEXT_MAJOR_VERSION_KHR     0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR     0x30FB
#define EGL_CONTEXT_FLAGS_KHR             0x30FC
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR 0x31BD
#define EGL_NO_RESET_NOTIFICATION_KHR     0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_KHR     0x31BF
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR  0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR 0x00000004
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR 0x00000002
#define EGL_OPENGL_ES3_BIT_KHR            0x00000040
#endif /* EGL_KHR_create_context */

#ifndef EGL_EXT_create_context_robustness
#define EGL_EXT_create_context_robustness 1
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT 0x30BF
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT 0x3138
#define EGL_NO_RESET_NOTIFICATION_EXT     0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_EXT     0x31BF
#endif /* EGL_EXT_create_context_robustness */

// \note Taken from official GLES2/gl2ext.h. Generated on date 20131202.
#ifndef GL_EXT_robustness
#define GL_EXT_robustness 1
#define GL_GUILTY_CONTEXT_RESET_EXT       0x8253
#define GL_INNOCENT_CONTEXT_RESET_EXT     0x8254
#define GL_UNKNOWN_CONTEXT_RESET_EXT      0x8255
#define GL_CONTEXT_ROBUST_ACCESS_EXT      0x90F3
#define GL_RESET_NOTIFICATION_STRATEGY_EXT 0x8256
#define GL_LOSE_CONTEXT_ON_RESET_EXT      0x8252
#define GL_NO_RESET_NOTIFICATION_EXT      0x8261
#endif /* GL_EXT_robustness */

#ifndef GL_ARB_robustness
/* reuse GL_NO_ERROR */
#define GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB 0x00000004
#define GL_LOSE_CONTEXT_ON_RESET_ARB      0x8252
#define GL_GUILTY_CONTEXT_RESET_ARB       0x8253
#define GL_INNOCENT_CONTEXT_RESET_ARB     0x8254
#define GL_UNKNOWN_CONTEXT_RESET_ARB      0x8255
#define GL_RESET_NOTIFICATION_STRATEGY_ARB 0x8256
#define GL_NO_RESET_NOTIFICATION_ARB      0x8261
#endif

using std::set;
using std::string;
using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace egl
{

namespace
{

size_t getAttribListLength (const EGLint* attribList)
{
	size_t size = 0;

	while (attribList[size] != EGL_NONE)
		size++;

	return size + 1;
}

string eglContextFlagsToString (EGLint flags)
{
	std::ostringstream	stream;

	if (flags == 0)
		stream << "<None>";
	else
	{
		bool first = true;

		if ((flags & EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR) != 0)
		{
			if (!first)
				stream << "|";

			first = false;

			stream << "EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR";
		}

		if ((flags & EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR) != 0)
		{
			if (!first)
				stream << "|";

			first = false;

			stream << "EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR";
		}

		if ((flags & EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR) != 0)
		{
			if (!first)
				stream << "|";

			stream << "EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR";
		}
	}

	return stream.str();
}

string eglProfileMaskToString (EGLint mask)
{
	std::ostringstream	stream;

	if (mask == 0)
		stream << "<None>";
	else
	{
		bool first = true;

		if ((mask & EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR) != 0)
		{
			if (!first)
				stream << "|";

			first = false;

			stream << "EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR";
		}

		if ((mask & EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR) != 0)
		{
			if (!first)
				stream << "|";

			stream << "EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR";
		}
	}

	return stream.str();
}

const char* eglResetNotificationStrategyToString (EGLint strategy)
{
	switch (strategy)
	{
		case EGL_NO_RESET_NOTIFICATION_KHR:		return "EGL_NO_RESET_NOTIFICATION_KHR";
		case EGL_LOSE_CONTEXT_ON_RESET_KHR:		return "EGL_LOSE_CONTEXT_ON_RESET_KHR";
		default:
			return "<Unknown>";
	}
}

class CreateContextExtCase : public TestCase
{
public:
								CreateContextExtCase	(EglTestContext& eglTestCtx, EGLenum api, const EGLint* attribList, const eglu::FilterList& filter, const char* name, const char* description);
								~CreateContextExtCase	(void);

	void						executeForConfig		(tcu::egl::Display& display, EGLConfig config, tcu::egl::Surface& surface);

	void						init					(void);
	void						deinit					(void);

	IterateResult				iterate					(void);
	void						checkRequiredExtensions	(void);
	void						logAttribList			(void);
	bool						validateCurrentContext	(const glw::Functions& gl);

private:
	bool						m_isOk;
	int							m_iteration;

	const eglu::FilterList		m_filter;
	vector<EGLint>				m_attribList;
	const EGLenum				m_api;

	vector<EGLConfig>			m_configs;
	glu::ContextType			m_glContextType;
};

glu::ContextType attribListToContextType (EGLenum api, const EGLint* attribList)
{
	EGLint				majorVersion	= 1;
	EGLint				minorVersion	= 0;
	glu::ContextFlags	flags			= glu::ContextFlags(0);
	glu::Profile		profile			= api == EGL_OPENGL_ES_API ? glu::PROFILE_ES : glu::PROFILE_CORE;
	const EGLint*		iter			= attribList;

	while((*iter) != EGL_NONE)
	{
		switch (*iter)
		{
			case EGL_CONTEXT_MAJOR_VERSION_KHR:
				iter++;
				majorVersion = (*iter);
				iter++;
				break;

			case EGL_CONTEXT_MINOR_VERSION_KHR:
				iter++;
				minorVersion = (*iter);
				iter++;
				break;

			case EGL_CONTEXT_FLAGS_KHR:
				iter++;

				if ((*iter & EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR) != 0)
					flags = flags | glu::CONTEXT_ROBUST;

				if ((*iter & EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR) != 0)
					flags = flags | glu::CONTEXT_DEBUG;

				if ((*iter & EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR) != 0)
					flags = flags | glu::CONTEXT_FORWARD_COMPATIBLE;

				iter++;
				break;

			case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR:
				iter++;

				if (*iter == EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR)
					profile = glu::PROFILE_COMPATIBILITY;
				else if (*iter != EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR)
					throw tcu::InternalError("Indeterminate OpenGL profile");

				iter++;
				break;

			case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR:
				iter += 2;
				break;

			case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
				iter += 2;
				break;

			case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
				iter += 2;
				break;

			default:
				DE_ASSERT(DE_FALSE);
		}
	}

	return glu::ContextType(majorVersion, minorVersion, profile, flags);
}

CreateContextExtCase::CreateContextExtCase (EglTestContext& eglTestCtx, EGLenum api, const EGLint* attribList, const eglu::FilterList& filter, const char* name, const char* description)
	: TestCase			(eglTestCtx, name, description)
	, m_isOk			(true)
	, m_iteration		(0)
	, m_filter			(filter)
	, m_attribList		(attribList, attribList + getAttribListLength(attribList))
	, m_api				(api)
	, m_glContextType	(attribListToContextType(api, attribList))
{
}

CreateContextExtCase::~CreateContextExtCase (void)
{
	deinit();
}

void CreateContextExtCase::init (void)
{
	vector<EGLConfig> configs;
	m_eglTestCtx.getDisplay().getConfigs(configs);

	for (int configNdx = 0; configNdx < (int)configs.size(); configNdx++)
	{
		if (m_filter.match(m_eglTestCtx.getDisplay().getEGLDisplay(), configs[configNdx]))
			m_configs.push_back(configs[configNdx]);
	}
}

void CreateContextExtCase::deinit (void)
{
	m_attribList	= vector<EGLint>();
	m_configs		= vector<EGLConfig>();
}

void CreateContextExtCase::logAttribList (void)
{
	const EGLint*		iter = &(m_attribList[0]);
	std::ostringstream	attribListString;

	while ((*iter) != EGL_NONE)
	{
		switch (*iter)
		{
			case EGL_CONTEXT_MAJOR_VERSION_KHR:
				iter++;
				attribListString << "EGL_CONTEXT_MAJOR_VERSION_KHR(EGL_CONTEXT_CLIENT_VERSION), " << (*iter) << ", ";
				iter++;
				break;

			case EGL_CONTEXT_MINOR_VERSION_KHR:
				iter++;
				attribListString << "EGL_CONTEXT_MINOR_VERSION_KHR, " << (*iter) << ", ";
				iter++;
				break;

			case EGL_CONTEXT_FLAGS_KHR:
				iter++;
				attribListString << "EGL_CONTEXT_FLAGS_KHR, " << eglContextFlagsToString(*iter) << ", ";
				iter++;
				break;

			case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR:
				iter++;
				attribListString << "EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, " << eglProfileMaskToString(*iter) << ", ";
				iter++;
				break;

			case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR:
				iter++;
				attribListString << "EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, " << eglResetNotificationStrategyToString(*iter) << ", ";
				iter++;
				break;

			case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
				iter++;
				attribListString << "EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, ";

				if (*iter == EGL_FALSE && *iter == EGL_TRUE)
					attribListString << (*iter ? "EGL_TRUE" : "EGL_FALSE");
				else
					attribListString << (*iter);
				iter++;
				break;

			default:
				DE_ASSERT(DE_FALSE);
		}
	}

	attribListString << "EGL_NONE";
	m_testCtx.getLog() << TestLog::Message << "EGL attrib list: { " << attribListString.str() << " }" << TestLog::EndMessage;
}

void CreateContextExtCase::checkRequiredExtensions (void)
{
	bool			isOk = true;
	set<string>		requiredExtensions;
	vector<string>	extensions;

	m_eglTestCtx.getDisplay().getExtensions(extensions);

	{
		const EGLint* iter = &(m_attribList[0]);

		while ((*iter) != EGL_NONE)
		{
			switch (*iter)
			{
				case EGL_CONTEXT_MAJOR_VERSION_KHR:
					iter++;
					iter++;
					break;

				case EGL_CONTEXT_MINOR_VERSION_KHR:
					iter++;
					requiredExtensions.insert("EGL_KHR_create_context");
					iter++;
					break;

				case EGL_CONTEXT_FLAGS_KHR:
					iter++;
					requiredExtensions.insert("EGL_KHR_create_context");
					iter++;
					break;

				case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR:
					iter++;
					requiredExtensions.insert("EGL_KHR_create_context");
					iter++;
					break;

				case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR:
					iter++;
					requiredExtensions.insert("EGL_KHR_create_context");
					iter++;
					break;

				case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
					iter++;
					requiredExtensions.insert("EGL_EXT_create_context_robustness");
					iter++;
					break;

				case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
					iter++;
					requiredExtensions.insert("EGL_EXT_create_context_robustness");
					iter++;
					break;

				default:
					DE_ASSERT(DE_FALSE);
			}
		}
	}

	for (std::set<string>::const_iterator reqExt = requiredExtensions.begin(); reqExt != requiredExtensions.end(); ++reqExt)
	{
		bool found = false;

		for (int extNdx = 0; extNdx < (int)extensions.size(); extNdx++)
		{
			if (*reqExt == extensions[extNdx])
				found = true;
		}

		if (!found)
		{
			m_testCtx.getLog() << TestLog::Message << "Required extension '" << (*reqExt) << "' not supported" << TestLog::EndMessage;
			isOk = false;
		}
	}

	if (!isOk)
		throw tcu::NotSupportedError("Required extensions not supported", "", __FILE__, __LINE__);
}

bool hasExtension (const glw::Functions& gl, const char* extension)
{
	std::istringstream	stream((const char*)gl.getString(GL_EXTENSIONS));
	string				ext;

	while (std::getline(stream, ext, ' '))
	{
		if (ext == extension)
			return true;
	}

	return false;
}

bool checkVersionString (TestLog& log, const glw::Functions& gl, bool desktop, int major, int minor)
{
	const char* const	versionStr	= (const char*)gl.getString(GL_VERSION);
	const char*			iter		= versionStr;

	int majorVersion = 0;
	int minorVersion = 0;

	// Check embedded version prefixes
	if (!desktop)
	{
		const char* prefix		= NULL;
		const char* prefixIter	= NULL;

		if (major == 1)
			prefix = "OpenGL ES-CM ";
		else
			prefix = "OpenGL ES ";

		prefixIter = prefix;

		while (*prefixIter)
		{
			if ((*prefixIter) != (*iter))
			{
				log << TestLog::Message << "Invalid version string prefix. Expected '" << prefix << "'." << TestLog::EndMessage;
				return false;
			}

			prefixIter++;
			iter++;
		}
	}

	while ((*iter) && (*iter) != '.')
	{
		const int val = (*iter) - '0';

		// Not a number
		if (val < 0 || val > 9)
		{
			log << TestLog::Message << "Failed to parse major version number. Not a number." << TestLog::EndMessage;
			return false;
		}

		// Leading zero
		if (majorVersion == 0 && val == 0)
		{
			log << TestLog::Message << "Failed to parse major version number. Begins with zero." << TestLog::EndMessage;
			return false;
		}

		majorVersion = majorVersion * 10 + val;

		iter++;
	}

	// Invalid format
	if ((*iter) != '.')
	{
		log << TestLog::Message << "Failed to parse version. Expected '.' after major version number." << TestLog::EndMessage;
		return false;
	}

	iter++;

	while ((*iter) && (*iter) != ' ' && (*iter) != '.')
	{
		const int val = (*iter) - '0';

		// Not a number
		if (val < 0 || val > 9)
		{
			log << TestLog::Message << "Failed to parse minor version number. Not a number." << TestLog::EndMessage;
			return false;
		}

		// Leading zero
		if (minorVersion == 0 && val == 0)
		{
			// Leading zeros in minor version
			if ((*(iter + 1)) != ' ' && (*(iter + 1)) != '.' && (*(iter + 1)) != '\0')
			{
				log << TestLog::Message << "Failed to parse minor version number. Leading zeros." << TestLog::EndMessage;
				return false;
			}
		}

		minorVersion = minorVersion * 10 + val;

		iter++;
	}

	// Invalid format
	if ((*iter) != ' ' && (*iter) != '.' && (*iter) != '\0')
		return false;

	if (desktop)
	{
		if (majorVersion < major)
		{
			log << TestLog::Message << "Major version is less than required." << TestLog::EndMessage;
			return false;
		}
		else if (majorVersion == major && minorVersion < minor)
		{
			log << TestLog::Message << "Minor version is less than required." << TestLog::EndMessage;
			return false;
		}
		else if (majorVersion == major && minorVersion == minor)
			return true;

		if (major < 3 || (major == 3 && minor == 0))
		{
			if (majorVersion == 3 && minorVersion == 1)
			{
				if (hasExtension(gl, "GL_ARB_compatibility"))
					return true;
				else
				{
					log << TestLog::Message << "Required OpenGL 3.0 or earlier. Got OpenGL 3.1 without GL_ARB_compatibility." << TestLog::EndMessage;
					return false;
				}
			}
			else if (majorVersion > 3 || (majorVersion == 3 && minorVersion >= minor))
			{
				deInt32 profile = 0;

				gl.getIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
				GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

				if (profile == GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
					return true;
				else
				{
					log << TestLog::Message << "Required OpenGL 3.0 or earlier. Got later version without compatibility profile." << TestLog::EndMessage;
					return false;
				}
			}
			else
				DE_ASSERT(false);

			return false;
		}
		else if (major == 3 && minor == 1)
		{
			if (majorVersion > 3 || (majorVersion == 3 && minorVersion >= minor))
			{
				deInt32 profile = 0;

				gl.getIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
				GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

				if (profile == GL_CONTEXT_CORE_PROFILE_BIT)
					return true;
				else
				{
					log << TestLog::Message << "Required OpenGL 3.1. Got later version without core profile." << TestLog::EndMessage;
					return false;
				}
			}
			else
				DE_ASSERT(false);

			return false;
		}
		else
		{
			log << TestLog::Message << "Couldn't do any further compatibilyt checks." << TestLog::EndMessage;
			return true;
		}
	}
	else
	{
		if (majorVersion < major)
		{
			log << TestLog::Message << "Major version is less than required." << TestLog::EndMessage;
			return false;
		}
		else if (majorVersion == major && minorVersion < minor)
		{
			log << TestLog::Message << "Minor version is less than required." << TestLog::EndMessage;
			return false;
		}
		else
			return true;
	}
}

bool checkVersionQueries (TestLog& log, const glw::Functions& gl, int major, int minor)
{
	deInt32 majorVersion = 0;
	deInt32	minorVersion = 0;

	gl.getIntegerv(GL_MAJOR_VERSION, &majorVersion);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

	gl.getIntegerv(GL_MINOR_VERSION, &minorVersion);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

	if (majorVersion < major || (majorVersion == major && minorVersion < minor))
	{
		if (majorVersion < major)
			log << TestLog::Message << "glGetIntegerv(GL_MAJOR_VERSION) returned '" << majorVersion << "' expected at least '" << major << "'" << TestLog::EndMessage;
		else if (majorVersion == major && minorVersion < minor)
			log << TestLog::Message << "glGetIntegerv(GL_MINOR_VERSION) returned '" << minorVersion << "' expected '" << minor << "'" << TestLog::EndMessage;
		else
			DE_ASSERT(false);

		return false;
	}
	else
		return true;
}

bool CreateContextExtCase::validateCurrentContext (const glw::Functions& gl)
{
	bool				isOk					= true;
	TestLog&			log						= m_testCtx.getLog();
	const EGLint*		iter					= &(m_attribList[0]);

	EGLint				majorVersion			= -1;
	EGLint				minorVersion			= -1;
	EGLint				contextFlags			= -1;
	EGLint				profileMask				= -1;
	EGLint				notificationStrategy	= -1;
	EGLint				robustAccessExt			= -1;
	EGLint				notificationStrategyExt	= -1;

	while ((*iter) != EGL_NONE)
	{
		switch (*iter)
		{
			case EGL_CONTEXT_MAJOR_VERSION_KHR:
				iter++;
				majorVersion = (*iter);
				iter++;
				break;

			case EGL_CONTEXT_MINOR_VERSION_KHR:
				iter++;
				minorVersion = (*iter);
				iter++;
				break;

			case EGL_CONTEXT_FLAGS_KHR:
				iter++;
				contextFlags = (*iter);
				iter++;
				break;

			case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR:
				iter++;
				profileMask = (*iter);
				iter++;
				break;

			case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR:
				iter++;
				notificationStrategy = (*iter);
				iter++;
				break;

			case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
				iter++;
				robustAccessExt = *iter;
				iter++;
				break;

			case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
				iter++;
				notificationStrategyExt = *iter;
				iter++;
				break;

			default:
				DE_ASSERT(DE_FALSE);
		}
	}

	const string version = (const char*)gl.getString(GL_VERSION);

	log << TestLog::Message << "GL_VERSION: '" << version << "'" << TestLog::EndMessage;

	if (majorVersion == -1)
		majorVersion = 1;

	if (minorVersion == -1)
		minorVersion = 0;

	if (m_api == EGL_OPENGL_ES_API)
	{
		if (!checkVersionString(log, gl, false, majorVersion, minorVersion))
			isOk = false;

		if (majorVersion == 3)
		{
			if (!checkVersionQueries(log, gl, majorVersion, minorVersion))
				isOk = false;
		}
	}
	else if (m_api == EGL_OPENGL_API)
	{
		if (!checkVersionString(log, gl, true, majorVersion, minorVersion))
			isOk = false;

		if (majorVersion >= 3)
		{
			if (!checkVersionQueries(log, gl, majorVersion, minorVersion))
				isOk = false;
		}
	}
	else
		DE_ASSERT(false);


	if (contextFlags != -1)
	{
		if (m_api == EGL_OPENGL_API && (majorVersion > 3 || (majorVersion == 3 && minorVersion >= 1)))
		{
			deInt32 contextFlagsGL;

			DE_ASSERT(m_api == EGL_OPENGL_API);

			if (contextFlags == -1)
				contextFlags = 0;

			gl.getIntegerv(GL_CONTEXT_FLAGS, &contextFlagsGL);

			if (contextFlags != contextFlagsGL)
			{
				log << TestLog::Message << "Invalid GL_CONTEXT_FLAGS. Expected '" << eglContextFlagsToString(contextFlags) << "' got '" << eglContextFlagsToString(contextFlagsGL) << "'" << TestLog::EndMessage;
				isOk = false;
			}
		}
	}

	if (profileMask != -1 || (m_api == EGL_OPENGL_API && (majorVersion >= 3)))
	{
		if (profileMask == -1)
			profileMask = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;

		DE_ASSERT(m_api == EGL_OPENGL_API);

		if (majorVersion < 3 || (majorVersion == 3 && minorVersion < 2))
		{
			// \note Ignore profile masks. This is not an error
		}
		else
		{
			deInt32 profileMaskGL = 0;

			gl.getIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

			if (profileMask != profileMaskGL)
			{
				log << TestLog::Message << "Invalid GL_CONTEXT_PROFILE_MASK. Expected '" << eglProfileMaskToString(profileMask) << "' got '" << eglProfileMaskToString(profileMaskGL) << "'" << TestLog::EndMessage;
				isOk = false;
			}
		}
	}

	if (notificationStrategy != -1)
	{
		if (m_api == EGL_OPENGL_API)
		{
			deInt32 strategy;

			gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB, &strategy);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

			if (notificationStrategy == EGL_NO_RESET_NOTIFICATION_KHR && strategy != GL_NO_RESET_NOTIFICATION_ARB)
			{
				log << TestLog::Message << "glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB) returned '" << strategy << "', expected 'GL_NO_RESET_NOTIFICATION_ARB'" << TestLog::EndMessage;
				isOk = false;
			}
			else if (notificationStrategy == EGL_LOSE_CONTEXT_ON_RESET_KHR && strategy != GL_LOSE_CONTEXT_ON_RESET_ARB)
			{
				log << TestLog::Message << "glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB) returned '" << strategy << "', expected 'GL_LOSE_CONTEXT_ON_RESET_ARB'" << TestLog::EndMessage;
				isOk = false;
			}
		}
		else if (m_api == EGL_OPENGL_ES_API)
		{
			deInt32 strategy;

			gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY_EXT, &strategy);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

			if (notificationStrategy == EGL_NO_RESET_NOTIFICATION_KHR && strategy != GL_NO_RESET_NOTIFICATION_EXT)
			{
				log << TestLog::Message << "glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_EXT) returned '" << strategy << "', expected 'GL_NO_RESET_NOTIFICATION_EXT'" << TestLog::EndMessage;
				isOk = false;
			}
			else if (notificationStrategy == EGL_LOSE_CONTEXT_ON_RESET_KHR && strategy != GL_LOSE_CONTEXT_ON_RESET_EXT)
			{
				log << TestLog::Message << "glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_EXT) returned '" << strategy << "', expected 'GL_LOSE_CONTEXT_ON_RESET_EXT'" << TestLog::EndMessage;
				isOk = false;
			}
		}
	}

	if (notificationStrategyExt != -1)
	{
		if (m_api == EGL_OPENGL_API)
		{
			deInt32 strategy;

			gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB, &strategy);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

			if (notificationStrategyExt == EGL_NO_RESET_NOTIFICATION_KHR && strategy != GL_NO_RESET_NOTIFICATION_ARB)
			{
				log << TestLog::Message << "glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB) returned '" << strategy << "', expected 'GL_NO_RESET_NOTIFICATION_ARB'" << TestLog::EndMessage;
				isOk = false;
			}
			else if (notificationStrategyExt == EGL_LOSE_CONTEXT_ON_RESET_KHR && strategy != GL_LOSE_CONTEXT_ON_RESET_ARB)
			{
				log << TestLog::Message << "glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB) returned '" << strategy << "', expected 'GL_LOSE_CONTEXT_ON_RESET_ARB'" << TestLog::EndMessage;
				isOk = false;
			}
		}
		else if (m_api == EGL_OPENGL_ES_API)
		{
			deInt32 strategy;

			gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY_EXT, &strategy);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

			if (notificationStrategyExt == EGL_NO_RESET_NOTIFICATION_KHR && strategy != GL_NO_RESET_NOTIFICATION_EXT)
			{
				log << TestLog::Message << "glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_EXT) returned '" << strategy << "', expected 'GL_NO_RESET_NOTIFICATION_EXT'" << TestLog::EndMessage;
				isOk = false;
			}
			else if (notificationStrategyExt == EGL_LOSE_CONTEXT_ON_RESET_KHR && strategy != GL_LOSE_CONTEXT_ON_RESET_EXT)
			{
				log << TestLog::Message << "glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_EXT) returned '" << strategy << "', expected 'GL_LOSE_CONTEXT_ON_RESET_EXT'" << TestLog::EndMessage;
				isOk = false;
			}
		}
	}
	

	if (robustAccessExt == EGL_TRUE)
	{
		if (m_api == EGL_OPENGL_API)
		{
			if (!hasExtension(gl, "GL_ARB_robustness"))
			{
				log << TestLog::Message << "Created robustness context but it doesn't support GL_ARB_robustness." << TestLog::EndMessage;
				isOk = false;
			}
		}
		else if (m_api == EGL_OPENGL_ES_API)
		{
			if (!hasExtension(gl, "GL_EXT_robustness"))
			{
				log << TestLog::Message << "Created robustness context but it doesn't support GL_EXT_robustness." << TestLog::EndMessage;
				isOk = false;
			}
		}

		if (m_api == EGL_OPENGL_API && (majorVersion > 3 || (majorVersion == 3 && minorVersion >= 1)))
		{
			deInt32 contextFlagsGL;

			DE_ASSERT(m_api == EGL_OPENGL_API);

			gl.getIntegerv(GL_CONTEXT_FLAGS, &contextFlagsGL);

			if ((contextFlagsGL & GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB) != 0)
			{
				log << TestLog::Message << "Invalid GL_CONTEXT_FLAGS. GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB to be set, got '" << eglContextFlagsToString(contextFlagsGL) << "'" << TestLog::EndMessage;
				isOk = false;
			}
		}
		else if (m_api == EGL_OPENGL_ES_API)
		{
			deUint8 robustAccessGL;

			gl.getBooleanv(GL_CONTEXT_ROBUST_ACCESS_EXT, &robustAccessGL);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glGetBooleanv()");

			if (robustAccessGL != GL_TRUE)
			{
				log << TestLog::Message << "Invalid GL_CONTEXT_ROBUST_ACCESS_EXT returned by glGetBooleanv(). Got '" << robustAccessGL << "' expected GL_TRUE." << TestLog::EndMessage;
				isOk = false;
			}
		}
		
	}

	return isOk;
}

TestCase::IterateResult CreateContextExtCase::iterate (void)
{
	if (m_iteration == 0)
	{
		logAttribList();
		checkRequiredExtensions();
	}

	if (m_iteration < (int)m_configs.size())
	{
		const EGLConfig		config			= m_configs[m_iteration];
		tcu::egl::Display&	display			= m_eglTestCtx.getDisplay();
		const EGLint		surfaceTypes	= display.getConfigAttrib(config, EGL_SURFACE_TYPE);
		const EGLint		configId		= display.getConfigAttrib(config, EGL_CONFIG_ID);

		if ((surfaceTypes & EGL_PBUFFER_BIT) != 0)
		{
			tcu::ScopedLogSection section(m_testCtx.getLog(), ("EGLConfig ID: " + de::toString(configId) + " with PBuffer").c_str(), ("EGLConfig ID: " + de::toString(configId)).c_str());
			const EGLint attribList[] =
			{
				EGL_WIDTH,	64,
				EGL_HEIGHT,	64,
				EGL_NONE
			};

			tcu::egl::PbufferSurface pbuffer(display, config, attribList);
			executeForConfig(display, config, pbuffer);
		}
		else if ((surfaceTypes & EGL_WINDOW_BIT) != 0)
		{
			de::UniquePtr<eglu::NativeWindow>	window	(m_eglTestCtx.createNativeWindow(display.getEGLDisplay(), config, DE_NULL, 256, 256, eglu::parseWindowVisibility(m_testCtx.getCommandLine())));
			tcu::egl::WindowSurface				surface	(display, eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *window, display.getEGLDisplay(), config, DE_NULL));

			executeForConfig(display, config, surface);
		}
		else if ((surfaceTypes & EGL_PIXMAP_BIT) != 0)
		{
			de::UniquePtr<eglu::NativePixmap>	pixmap	(m_eglTestCtx.createNativePixmap(display.getEGLDisplay(), config, DE_NULL, 256, 256));
			tcu::egl::PixmapSurface				surface	(display, eglu::createPixmapSurface(m_eglTestCtx.getNativeDisplay(), *pixmap, display.getEGLDisplay(), config, DE_NULL));

			executeForConfig(display, config, surface);
		}
		else // No supported surface type
			TCU_CHECK(false);

		m_iteration++;
		return CONTINUE;
	}
	else
	{
		if (m_configs.size() == 0)
		{
			m_testCtx.getLog() << TestLog::Message << "No supported configs found" << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No supported configs found");
		}
		else if (m_isOk)
			m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		else
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

		return STOP;
	}
}

void CreateContextExtCase::executeForConfig (tcu::egl::Display& display, EGLConfig config, tcu::egl::Surface& surface)
{
	tcu::egl::Context*		context		= DE_NULL;

	TCU_CHECK_EGL_CALL(eglBindAPI(m_api));

	try
	{
		glw::Functions	gl;

		context = new tcu::egl::Context(display, config, &(m_attribList[0]), m_api);
		context->makeCurrent(surface, surface);

		m_eglTestCtx.getGLFunctions(gl, m_glContextType.getAPI());

		if (!validateCurrentContext(gl))
			m_isOk = false;

		delete context;
	}
	catch (const eglu::Error& error)
	{
		delete context;

		if (error.getError() == EGL_BAD_MATCH)
			m_testCtx.getLog() << TestLog::Message << "Context creation failed with error EGL_BAD_CONTEXT. Config doesn't support api version." << TestLog::EndMessage;
		else if (error.getError() == EGL_BAD_CONFIG)
			m_testCtx.getLog() << TestLog::Message << "Context creation failed with error EGL_BAD_MATCH. Context attribute compination not supported." << TestLog::EndMessage;
		else
		{
			m_testCtx.getLog() << TestLog::Message << "Context creation failed with error " << eglu::getErrorStr(error.getError()) << ". Error is not result of unsupported api etc." << TestLog::EndMessage;
			m_isOk = false;
		}
	}
	catch (...)
	{
		delete context;
		throw;
	}
}

class CreateContextExtGroup : public TestCaseGroup
{
public:
						CreateContextExtGroup	(EglTestContext& eglTestCtx, EGLenum api, EGLint apiBit, const EGLint* attribList, const char* name, const char* description);
	virtual				~CreateContextExtGroup	(void);

	void				init					(void);

private:
	const EGLenum		m_api;
	const EGLint		m_apiBit;
	vector<EGLint>		m_attribList;
};

CreateContextExtGroup::CreateContextExtGroup (EglTestContext& eglTestCtx, EGLenum api, EGLint apiBit, const EGLint* attribList, const char* name, const char* description)
	: TestCaseGroup (eglTestCtx, name, description)
	, m_api			(api)
	, m_apiBit		(apiBit)
	, m_attribList	(attribList, attribList + getAttribListLength(attribList))
{
}

CreateContextExtGroup::~CreateContextExtGroup (void)
{
}

void CreateContextExtGroup::init (void)
{
	const struct
	{
		const char*				name;
		const char*				description;

		EGLint					redSize;
		EGLint					greenSize;
		EGLint					blueSize;
		EGLint					alphaSize;

		bool					hasDepth;
		bool					hasStencil;
	} groups[] =
	{
		{ "rgb565_no_depth_no_stencil",		"RGB565 configs without depth or stencil",		5, 6, 5, 0, false,	false	},
		{ "rgb565_no_depth_stencil",		"RGB565 configs with stencil and no depth",		5, 6, 5, 0, false,	true	},
		{ "rgb565_depth_no_stencil",		"RGB565 configs with depth and no stencil",		5, 6, 5, 0, true,	false	},
		{ "rgb565_depth_stencil",			"RGB565 configs with depth and stencil",		5, 6, 5, 0, true,	true	},

		{ "rgb888_no_depth_no_stencil",		"RGB888 configs without depth or stencil",		8, 8, 8, 0, false,	false	},
		{ "rgb888_no_depth_stencil",		"RGB888 configs with stencil and no depth",		8, 8, 8, 0, false,	true	},
		{ "rgb888_depth_no_stencil",		"RGB888 configs with depth and no stencil",		8, 8, 8, 0, true,	false	},
		{ "rgb888_depth_stencil",			"RGB888 configs with depth and stencil",		8, 8, 8, 0, true,	true	},

		{ "rgba4444_no_depth_no_stencil",	"RGBA4444 configs without depth or stencil",	4, 4, 4, 4, false,	false	},
		{ "rgba4444_no_depth_stencil",		"RGBA4444 configs with stencil and no depth",	4, 4, 4, 4, false,	true	},
		{ "rgba4444_depth_no_stencil",		"RGBA4444 configs with depth and no stencil",	4, 4, 4, 4, false,	false	},
		{ "rgba4444_depth_stencil",			"RGBA4444 configs with depth and stencil",		4, 4, 4, 4, true,	true	},

		{ "rgba5551_no_depth_no_stencil",	"RGBA5551 configs without depth or stencil",	5, 5, 5, 1, false,	false	},
		{ "rgba5551_no_depth_stencil",		"RGBA5551 configs with stencil and no depth",	5, 5, 5, 1, false,	true	},
		{ "rgba5551_depth_no_stencil",		"RGBA5551 configs with depth and no stencil",	5, 5, 5, 1, true,	false	},
		{ "rgba5551_depth_stencil",			"RGBA5551 configs with depth and stencil",		5, 5, 5, 1, true,	true	},

		{ "rgba8888_no_depth_no_stencil",	"RGBA8888 configs without depth or stencil",	8, 8, 8, 8, false,	false	},
		{ "rgba8888_no_depth_stencil",		"RGBA8888 configs with stencil and no depth",	8, 8, 8, 8, false,	true	},
		{ "rgba8888_depth_no_stencil",		"RGBA8888 configs with depth and no stencil",	8, 8, 8, 8, true,	false	},
		{ "rgba8888_depth_stencil",			"RGBA8888 configs with depth and stencil",		8, 8, 8, 8, true,	true	}
	};

	for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(groups); groupNdx++)
	{
		eglu::FilterList filter;

		filter
		<< (eglu::ConfigRedSize()	== groups[groupNdx].redSize)
		<< (eglu::ConfigGreenSize()	== groups[groupNdx].greenSize)
		<< (eglu::ConfigBlueSize()	== groups[groupNdx].blueSize)
		<< (eglu::ConfigAlphaSize()	== groups[groupNdx].alphaSize);

		if (groups[groupNdx].hasDepth)
			filter << (eglu::ConfigDepthSize() >= 1);

		if (groups[groupNdx].hasStencil)
			filter << (eglu::ConfigStencilSize() >= 1);

		filter << (eglu::ConfigRenderableType() & m_apiBit);

		addChild(new CreateContextExtCase(m_eglTestCtx, m_api, &(m_attribList[0]), filter, groups[groupNdx].name, groups[groupNdx].description));
	}
	// \todo [mika] Add other group
}

} // anonymous

CreateContextExtTests::CreateContextExtTests (EglTestContext& eglTestCtx)
	: TestCaseGroup(eglTestCtx, "create_context_ext", "EGL_KHR_create_context tests.")
{
}

CreateContextExtTests::~CreateContextExtTests (void)
{
}

void CreateContextExtTests::init (void)
{
	const size_t	maxAttributeCount = 10;
	const struct
	{
		const char*	name;
		const char*	description;
		EGLenum		api;
		EGLint		apiBit;
		EGLint		attribList[maxAttributeCount];
	} groupList[] =
	{
#if 0
		// \todo [mika] Not supported by glw
		// OpenGL ES 1.x
		{ "gles_10", "Create OpenGL ES 1.0 context", EGL_OPENGL_ES_API, EGL_OPENGL_ES_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 1, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_NONE} },
		{ "gles_11", "Create OpenGL ES 1.1 context", EGL_OPENGL_ES_API, EGL_OPENGL_ES_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 1, EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_NONE } },
#endif
		// OpenGL ES 2.x
		{ "gles_20", "Create OpenGL ES 2.0 context", EGL_OPENGL_ES_API, EGL_OPENGL_ES2_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 2, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_NONE } },
		{ "robust_gles_20", "Create robust OpenGL ES 2.0 context", EGL_OPENGL_ES_API, EGL_OPENGL_ES2_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 2, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },
		// OpenGL ES 3.x
		{ "gles_30", "Create OpenGL ES 3.0 context", EGL_OPENGL_ES_API, EGL_OPENGL_ES3_BIT_KHR,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_NONE} },
		{ "robust_gles_30", "Create OpenGL ES 3.0 context", EGL_OPENGL_ES_API, EGL_OPENGL_ES3_BIT_KHR,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },
#if 0
		// \todo [mika] Not supported by glw
		// \note [mika] Should we really test 1.x?
		{ "gl_10", "Create OpenGL 1.0 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 1, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_NONE} },
		{ "gl_11", "Create OpenGL 1.1 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 1, EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_NONE } },

		// OpenGL 2.x
		{ "gl_20", "Create OpenGL 2.0 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 2, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_NONE } },
		{ "gl_21", "Create OpenGL 2.1 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 2, EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_NONE } },
#endif
		// OpenGL 3.x
		{ "gl_30", "Create OpenGL 3.0 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_NONE } },
		{ "robust_gl_30", "Create robust OpenGL 3.0 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },
		{ "gl_31", "Create OpenGL 3.1 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_NONE } },
		{ "robust_gl_31", "Create robust OpenGL 3.1 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },
		{ "gl_32", "Create OpenGL 3.2 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 2, EGL_NONE } },
		{ "robust_gl_32", "Create robust OpenGL 3.2 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 2, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },
		{ "gl_33", "Create OpenGL 3.3 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 3, EGL_NONE } },
		{ "robust_gl_33", "Create robust OpenGL 3.3 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 3, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },

		// OpenGL 4.x
		{ "gl_40", "Create OpenGL 4.0 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_NONE } },
		{ "robust_gl_40", "Create robust OpenGL 4.0 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },
		{ "gl_41", "Create OpenGL 4.1 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_NONE } },
		{ "robust_gl_41", "Create robust OpenGL 4.1 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },
		{ "gl_42", "Create OpenGL 4.2 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 2, EGL_NONE } },
		{ "robust_gl_42", "Create robust OpenGL 4.2 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 2, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },
		{ "gl_43", "Create OpenGL 4.3 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 3, EGL_NONE } },
		{ "robust_gl_43", "Create robust OpenGL 4.3 context", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 3, EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE } },

		// Robust contexts with EGL_EXT_create_context_robustness
		{ "robust_gles_2_ext", "Create robust OpenGL ES 2.0 context with EGL_EXT_create_context_robustness.", EGL_OPENGL_ES_API, EGL_OPENGL_ES2_BIT,
			{ EGL_CONTEXT_CLIENT_VERSION, 2, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_TRUE, EGL_NONE } },
		{ "robust_gles_3_ext", "Create robust OpenGL ES 3.0 context with EGL_EXT_create_context_robustness.", EGL_OPENGL_ES_API, EGL_OPENGL_ES3_BIT_KHR,
			{ EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_TRUE, EGL_NONE } },
#if 0
	// glu/glw doesn't support any version of OpenGL and EGL doesn't allow use of EGL_CONTEXT_CLIENT_VERSION with OpenGL and doesn't define which OpenGL version should be returned.
		{ "robust_gl_ext", "Create robust OpenGL context with EGL_EXT_create_context_robustness.", EGL_OPENGL_API, EGL_OPENGL_BIT,
			{ EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_TRUE, EGL_NONE } }
#endif
	};

	for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(groupList); groupNdx++)
		addChild(new CreateContextExtGroup(m_eglTestCtx, groupList[groupNdx].api, groupList[groupNdx].apiBit, groupList[groupNdx].attribList, groupList[groupNdx].name, groupList[groupNdx].description));
}

} // egl
} // deqp
