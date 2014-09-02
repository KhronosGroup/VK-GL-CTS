/*-------------------------------------------------------------------------
 * drawElements Quality Program Execution Server
 * ---------------------------------------------
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
 * \brief ExecServer main().
 *//*--------------------------------------------------------------------*/

#include "xsExecutionServer.hpp"
#include "deString.h"

#if (DE_OS == DE_OS_WIN32)
#	include "xsWin32TestProcess.hpp"
#else
#	include "xsPosixTestProcess.hpp"
#endif

#include <cstdlib>
#include <cstdio>

int main (int argc, const char* const* argv)
{
	xs::ExecutionServer::RunMode	runMode		= xs::ExecutionServer::RUNMODE_FOREVER;
	int								port		= 50016;

#if (DE_OS == DE_OS_WIN32)
	xs::Win32TestProcess			testProcess;
#else
	xs::PosixTestProcess			testProcess;
#endif

	DE_STATIC_ASSERT(sizeof("a") == 2);

#if (DE_OS != DE_OS_WIN32)
	// Set line buffered mode to stdout so executor gets any log messages soon enough.
	setvbuf(stdout, DE_NULL, _IOLBF, 4*1024);
#endif

	// Parse command line.
	for (int argNdx = 1; argNdx < argc; argNdx++)
	{
		const char* arg = argv[argNdx];

		if (deStringBeginsWith(arg, "--port="))
			port = atoi(arg+sizeof("--port=")-1);
		else if (deStringEqual(arg, "--single"))
			runMode = xs::ExecutionServer::RUNMODE_SINGLE_EXEC;
	}

	try
	{
		xs::ExecutionServer server(&testProcess, DE_SOCKETFAMILY_INET4, port, runMode);
		printf("Listening on port %d.\n", port);
		server.runServer();
	}
	catch (const std::exception& e)
	{
		printf("%s\n", e.what());
		return -1;
	}

	return 0;
}
