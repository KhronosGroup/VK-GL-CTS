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
 * \brief Basic definitions.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "deFilePath.hpp"
#include "qpDebugOut.h"

#include <sstream>
#include <stdarg.h>

namespace tcu
{

void die (const char* format, ...)
{
	va_list args;
	va_start(args, format);
	qpDiev(format, args);
	va_end(args);
}

void print (const char* format, ...)
{
	va_list args;
	va_start(args, format);
	qpPrintv(format, args);
	va_end(args);
}

static std::string formatError (const char* message, const char* expr, const char* file, int line)
{
	std::ostringstream msg;
	msg << (message ? message : "Runtime check failed");

	if (expr)
		msg << ": '" << expr << '\'';

	if (file)
		msg << " at " << de::FilePath(file).getBaseName() << ":" << line;

	return msg.str();
}

Exception::Exception (const char* message, const char* expr, const char* file, int line)
	: std::runtime_error(formatError(message, expr, file, line))
	, m_message			(message ? message : "Runtime check failed")
{
}

Exception::Exception (const std::string& message)
	: std::runtime_error(message)
	, m_message			(message)
{
}

TestError::TestError (const char* message, const char* expr, const char* file, int line)
	: Exception(message, expr, file, line)
{
}

TestError::TestError (const std::string& message)
	: Exception(message)
{
}

InternalError::InternalError (const char* message, const char* expr, const char* file, int line)
	: Exception(message, expr, file, line)
{
}

InternalError::InternalError (const std::string& message)
	: Exception(message)
{
}

ResourceError::ResourceError (const char* message, const char* expr, const char* file, int line)
	: Exception(message, expr, file, line)
{
}

ResourceError::ResourceError (const std::string& message)
	: Exception(message)
{
}

NotSupportedError::NotSupportedError (const char* message, const char* expr, const char* file, int line)
	: Exception(message, expr, file, line)
{
}

NotSupportedError::NotSupportedError (const std::string& message)
	: Exception(message)
{
}

} // namespace tcu
